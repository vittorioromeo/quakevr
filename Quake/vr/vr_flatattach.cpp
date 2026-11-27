// vr_flatattach.cpp -- flatscreen play's Attach key as drawn (flatattachanim; his words 2026-11-27: "there needs to be an
// animated transition for the hand attaching/detaching the dagger (and bayonet too), at the moment it happens abruptly";
// docs/vr-port/FLATPLAY.md, "Attachments").
//
// The game decides (QC vr_attach.qc VR_Attach_Key, impulse 52): the bayonet or dagger one hand holds goes on the gun in the
// other, else a gun's attachment comes off into the empty other hand. J (vr_flat_key_attach) no longer sends the impulse at
// once: this file foresees what the key will do (the same order as VR_Attach_Key: the main hand's gun first), and the free
// hand goes to the gun (vr_flat_attach_reach_time), turned the gun's way, the way the flat reload's trip takes the gun's
// hand to the hip (vr_flatreload.cpp). Putting one on, the hand stops with what it holds a blade's slide ahead of the mount
// (vr_flat_attach_slide units along the blade), the impulse goes, and the attachment, on the gun now, is drawn sliding back
// along its blade into the mount over vr_flat_attach_slide_time, the empty hand pushing it; taking one off, the hand goes to
// the mount, the impulse goes, and the hand slides it off along the blade. Then the hand comes back
// (vr_flat_attach_return_time). The hand's place is the game's too (the server sees it at the gun), and the impulse is the
// only thing sent: nothing new on the wire.
//
// Cancelled (the hand straight back, no impulse if it hasn't gone) by a shot from either hand, the gun changing in its
// hand (switched, dropped, thrown, swapped), or what the key would do changing on the way (the attachment let go). Nothing
// foreseen (or the gun not drawn): the impulse at once, as before. The headset's attach is the hands' own (unchanged).
//
// vr_flat_attach_debug 1 prints the trip's steps (flatattach: ...); vr_flatattach_report its state and counts.

#include "vr_flatattach.hpp"

#include "vr_attach.hpp"
#include "vr_backend.hpp" // HAND_OFF / HAND_MAIN
#include "vr_client.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_flatplay.hpp"
#include "vr_geom.hpp"
#include "vr_held.hpp"
#include "vr_protocol.hpp"
#include "vr_tunables.hpp"
#include "vr_view.hpp"

#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "vr_zancle.hpp"

namespace qvr::flatattach
{

namespace
{

enum class Phase : int
{
    None,
    Reach,  // the free hand to the gun
    Slide,  // the impulse sent: the blade slid along the barrel (on: into the mount; off: out of it in the hand)
    Return, // the hand back to its pose
};

enum class Action : int
{
    Attach,
    Detach,
};

// The trip (no function-local statics: a named file-scope struct, CODE_STYLE.md).
struct Trip
{
    Phase phase{Phase::None};
    Action action{Action::Attach};
    int gun{HAND_MAIN};       // the gun's hand
    int free{HAND_OFF};       // the hand that goes to it
    int kind{attach::None};   // the attachment
    int uid{0};               // the gun's id as the trip starts (another: cancelled)
    double startAt{-1e9};     // J's press (vr_gametime)
    float amount{0.f};        // the hand's share of the way to the gun, before its ease
    float slide{0.f};         // the slide's share (0 .. 1)
    glm::vec3 mount{0.f};     // the mount in the gun hand's frame (forward, left, up), from the drawn gun
    glm::vec3 way{1.f, 0.f, 0.f}; // the blade's way there (mount to tip)
    glm::vec3 held{0.f};      // the held attachment's mount in the free hand's frame (putting one on; taking off: none)
    glm::vec3 at{0.f};        // where the free hand goes now, in the gun hand's frame
    bool sent{false};         // the impulse went (or the hand's own snap put it on on the way)
};

struct AttachState
{
    Trip trip;
    double lastTime{-1e9};
    int presses{0};    // J's presses
    int trips{0};      // trips started
    int instant{0};    // impulses sent at once (nothing foreseen)
    int sent{0};       // impulses sent on arrival
    int cancels{0};
    float sendDelay{-1.f}; // the last trip's press to its impulse (s)
    const char* lastCancel{"-"};
};

AttachState as;

constexpr int foregripCarried = 2; // QC QVR_WPNFLAG_FOREGRIP_CARRIED

[[nodiscard]] float reachTime()
{
    return vr_flat_attach_reach_time.value;
}

[[nodiscard]] float slideTime()
{
    return za::max(0.02f, vr_flat_attach_slide_time.value);
}

[[nodiscard]] float returnTime()
{
    return za::max(0.02f, vr_flat_attach_return_time.value);
}

[[nodiscard]] float slideUnits()
{
    return za::clamp(vr_flat_attach_slide.value, 0.f, 32.f);
}

[[nodiscard]] const char* handName(int h)
{
    return h == HAND_MAIN ? "main" : "off";
}

[[nodiscard]] const char* kindName(int kind)
{
    const attach::KindInfo* const k = attach::info(kind);
    return k ? k->name : "none";
}

[[nodiscard]] const char* phaseName(Phase p)
{
    switch(p)
    {
        case Phase::Reach: return "reach";
        case Phase::Slide: return "slide";
        case Phase::Return: return "return";
        default: return "none";
    }
}

[[nodiscard]] int gunUid(int h)
{
    return cl.stats[h == HAND_MAIN ? protocol::STAT_QVR_WEAPONUID : protocol::STAT_QVR_WEAPONUID2];
}

[[nodiscard]] int weaponFlags(int h)
{
    return cl.stats[h == HAND_MAIN ? protocol::STAT_QVR_WEAPONFLAGS : protocol::STAT_QVR_WEAPONFLAGS2];
}

[[nodiscard]] int carriedEnt(int h)
{
    return cl.stats[h == HAND_MAIN ? protocol::STAT_QVR_CARRYMAIN : protocol::STAT_QVR_CARRYOFF];
}

// The attachment hand `h` holds that may go on a gun (QC VR_Attach_PropKind on its carried prop, VR_Attach_HeldDagger).
[[nodiscard]] int heldKind(int h)
{
    const int ent = carriedEnt(h);
    if(ent > 0 && ent < cl.num_entities && cl_entities[ent].model)
    {
        const attach::KindInfo* const b = attach::info(attach::Bayonet);
        if(b && q_strcasecmp(cl_entities[ent].model->name, b->model) == 0)
        {
            return attach::Bayonet;
        }
    }
    if(vr_attach_daggers.value != 0.f && flatplay::daggerIn(h) && !(weaponFlags(h) & foregripCarried))
    {
        return attach::Dagger;
    }
    return attach::None;
}

// QC VR_Attach_CanTake, as the client sees it: a gun with a record and none on it, not hung by its foregrip, drawn, that
// takes `kind`.
[[nodiscard]] bool canTake(int g, int kind)
{
    const int uid = gunUid(g);
    glm::vec3 m, t;
    return uid != 0 && attach::kindOfUid(uid) == attach::None && !(weaponFlags(g) & foregripCarried) &&
           view::attachSpot(g, kind, m, t);
}

// What J will do (QC VR_Attach_Key's order): false if nothing.
[[nodiscard]] bool foresee(Action& action, int& gun, int& free, int& kind)
{
    for(int g : {HAND_MAIN, HAND_OFF})
    {
        const int o = 1 - g;
        const int k = heldKind(o);
        if(k != attach::None && canTake(g, k))
        {
            action = Action::Attach;
            gun = g;
            free = o;
            kind = k;
            return true;
        }
    }
    for(int g : {HAND_MAIN, HAND_OFF})
    {
        const int o = 1 - g;
        const int k = gunUid(g) != 0 ? attach::kindOfUid(gunUid(g)) : attach::None;
        if(k != attach::None && held::handEmpty(o))
        {
            action = Action::Detach;
            gun = g;
            free = o;
            kind = k;
            return true;
        }
    }
    return false;
}

// Whether J would still do what the trip went for.
[[nodiscard]] bool stillDoes(const Trip& t)
{
    Action a;
    int g, f, k;
    return foresee(a, g, f, k) && a == t.action && g == t.gun && f == t.free && k == t.kind;
}

// A world point and way in hand `h`'s drawn frame (forward, left, up); false if it wasn't drawn.
[[nodiscard]] bool drawnFrame(int h, glm::vec3& pos, glm::mat3& axes)
{
    glm::vec3 rot;
    if(!view::drawnHandPose(h, pos, rot))
    {
        return false;
    }
    axes = held::axesFromAngles(&rot[0], true);
    return true;
}

// The trip's places measured from the drawn hands and gun as J is pressed: the mount and its blade's way in the gun hand's
// frame, and what the free hand holds in its own.
[[nodiscard]] bool measure(Trip& t)
{
    glm::vec3 gp, mW, tW;
    glm::mat3 ga;
    if(!drawnFrame(t.gun, gp, ga) || !view::attachSpot(t.gun, t.kind, mW, tW) || glm::length(tW - mW) < 1e-3f)
    {
        return false;
    }
    const glm::mat3 toGun = glm::transpose(ga);
    t.mount = toGun * (mW - gp);
    t.way = glm::normalize(toGun * (tW - mW));
    t.held = glm::vec3{0.f};
    if(t.action == Action::Attach)
    {
        glm::vec3 fp, hW{0.f};
        glm::mat3 fa;
        const int ent = carriedEnt(t.free);
        bool known = false;
        if(t.kind == attach::Bayonet)
        {
            known = ent > 0 && ent < cl.num_entities;
            if(known)
            {
                hW = geom::vec(cl_entities[ent].origin); // (a carried prop drawn in the hand: vr_held.cpp)
            }
        }
        else
        {
            known = view::drawnWeaponOrigin(t.free, hW); // (a dagger: the weapon drawn in the hand)
        }
        if(known && drawnFrame(t.free, fp, fa))
        {
            t.held = glm::transpose(fa) * (hW - fp);
            if(glm::length(t.held) > 24.f)
            {
                t.held = glm::vec3{0.f}; // (not drawn in the hand yet: the hand itself to the place)
            }
        }
    }
    return true;
}

// The free hand's place in the gun hand's frame: on, the held mount a slide ahead of the gun's, then pushed in; off, at
// the mount, then slid out along the blade.
[[nodiscard]] glm::vec3 arrival(const Trip& t)
{
    return t.action == Action::Attach ? t.mount + t.way * slideUnits() - t.held : t.mount - t.held;
}

[[nodiscard]] glm::vec3 slideEnd(const Trip& t)
{
    return t.action == Action::Attach ? t.mount - t.held : t.mount + t.way * slideUnits() - t.held;
}

void sendImpulse(const char* why)
{
    Cbuf_InsertText("impulse 52\n");
    if(vr_flat_attach_debug.value)
    {
        Con_Printf("flatattach: impulse 52 (%s)\n", why);
    }
}

void cancel(const char* why)
{
    Trip& t = as.trip;
    as.cancels++;
    as.lastCancel = why;
    t.phase = Phase::Return;
    if(vr_flat_attach_debug.value)
    {
        Con_Printf("flatattach: cancelled (%s) %.2f s after J%s\n", why, vr_gametime - t.startAt, t.sent ? "" : ": no impulse");
    }
}

void report_f()
{
    const Trip& t = as.trip;
    Con_Printf("flatattach: phase %s action %s gun %s free %s kind %s share %.2f slide %.2f sent %d; presses %d trips %d "
               "instant %d sent %d cancels %d last delay %.3f last cancel %s\n",
        phaseName(t.phase), t.action == Action::Attach ? "attach" : "detach", handName(t.gun), handName(t.free), kindName(t.kind),
        geom::smooth01(t.amount), t.slide, t.sent ? 1 : 0, as.presses, as.trips, as.instant, as.sent, as.cancels, as.sendDelay,
        as.lastCancel);
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_flatattach_report", report_f);
}

void press(const char* what)
{
    as.presses++;
    Trip& t = as.trip;
    if(t.phase != Phase::None)
    {
        if(vr_flat_attach_debug.value)
        {
            Con_Printf("flatattach: %s: a trip is on (%s)\n", what, phaseName(t.phase));
        }
        return;
    }
    Trip n;
    const bool known = reachTime() > 0.f && foresee(n.action, n.gun, n.free, n.kind) && measure(n);
    if(!known)
    {
        as.instant++;
        sendImpulse("at once: nothing foreseen");
        return;
    }
    n.phase = Phase::Reach;
    n.uid = gunUid(n.gun);
    n.startAt = vr_gametime;
    n.at = arrival(n);
    t = n;
    as.trips++;
    if(vr_flat_attach_debug.value)
    {
        Con_Printf("flatattach: %s: %s the %s, the %s hand to the %s hand's gun (mount %.1f %.1f %.1f, held %.1f %.1f %.1f)\n", what,
            t.action == Action::Attach ? "putting on" : "taking off", kindName(t.kind), handName(t.free), handName(t.gun), t.mount.x,
            t.mount.y, t.mount.z, t.held.x, t.held.y, t.held.z);
    }
}

void frame(bool on)
{
    double dt = as.lastTime > -1e8 ? vr_gametime - as.lastTime : 0.0;
    as.lastTime = vr_gametime;
    dt = za::clamp(dt, 0.0, 0.1);
    Trip& t = as.trip;
    if(!on)
    {
        t = Trip{};
        return;
    }
    if(t.phase == Phase::None)
    {
        return;
    }
    const float step = static_cast<float>(dt);
    // A shot, the gun changed in its hand, or (on the way) what J would do changed: straight back.
    if(t.phase == Phase::Reach || t.phase == Phase::Slide)
    {
        if(gunUid(t.gun) != t.uid)
        {
            cancel("the gun left the hand");
        }
        else if(client::attacking(t.gun) || client::attacking(t.free))
        {
            cancel("fired");
        }
        else if(t.phase == Phase::Reach && !stillDoes(t))
        {
            cancel(t.action == Action::Attach ? "the attachment let go" : "nothing to take off");
        }
    }
    if(t.phase == Phase::Reach)
    {
        t.amount = za::min(1.f, t.amount + step / za::max(0.02f, reachTime()));
        if(t.amount >= 1.f)
        {
            // (On: the hand's own snap may have put it on on its way, QC VR_Attach_Frame: no impulse then, which would take
            // it off again.)
            const bool snapped = t.action == Action::Attach && attach::kindOfUid(gunUid(t.gun)) == t.kind;
            if(!snapped)
            {
                sendImpulse("the hand at the gun");
                as.sent++;
            }
            else if(vr_flat_attach_debug.value)
            {
                Con_Printf("flatattach: already on (snapped on the way): no impulse\n");
            }
            as.sendDelay = static_cast<float>(vr_gametime - t.startAt);
            t.sent = true;
            t.phase = Phase::Slide;
            t.slide = 0.f;
        }
    }
    else if(t.phase == Phase::Slide)
    {
        t.slide = za::min(1.f, t.slide + step / slideTime());
        t.at = glm::mix(arrival(t), slideEnd(t), geom::smooth01(t.slide));
        if(t.slide >= 1.f)
        {
            t.phase = Phase::Return;
            if(vr_flat_attach_debug.value)
            {
                Con_Printf("flatattach: slid %s (%.2f s after J)\n", t.action == Action::Attach ? "on" : "off", vr_gametime - t.startAt);
            }
        }
    }
    else if(t.phase == Phase::Return)
    {
        t.amount = za::max(0.f, t.amount - step / returnTime());
        if(t.amount <= 0.f)
        {
            t.phase = Phase::None;
            if(vr_flat_attach_debug.value)
            {
                Con_Printf("flatattach: the %s hand back (%.2f s after J)\n", handName(t.free), vr_gametime - t.startAt);
            }
        }
    }
}

void apply(hands::State& s)
{
    const Trip& t = as.trip;
    if(t.phase == Phase::None || t.amount <= 0.f)
    {
        return;
    }
    const float a = geom::smooth01(t.amount);
    const glm::mat3 gunAxes = held::axesFromAngles(&s.rot[t.gun][0], true);
    const glm::vec3 to = s.pos[t.gun] + gunAxes * t.at;
    s.pos[t.free] += (to - s.pos[t.free]) * a;
    s.rot[t.free] = flatplay::blendHandAngles(s.rot[t.free], s.rot[t.gun], a);
}

bool busy(int h)
{
    return as.trip.phase != Phase::None && h == as.trip.free;
}

float slideAlong(int hand)
{
    const Trip& t = as.trip;
    if(t.phase != Phase::Slide || t.action != Action::Attach || hand != t.gun)
    {
        return 0.f;
    }
    return slideUnits() * (1.f - geom::smooth01(t.slide));
}

} // namespace qvr::flatattach
