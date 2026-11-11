// vr_shells.cpp -- see vr_shells.hpp.
//
// Each casing is a point with a velocity, an orientation and an angular velocity. In the air it
// falls at 9.81 m/s^2 (in Quake units at vr_world_scale, times sv_gravity / 800: low-gravity
// maps are floaty), with a little air drag (much more in a liquid, where it sinks slowly);
// a line trace through the world and the brush entities (worldtrace::world, the client's own
// data) finds what it hits: it bounces off with some restitution and friction, tumbling anew, and
// tinks. On a floor too slow to bounce it lies down on its side, rolls (across its axis) or slides
// (along it) to a stop and rests; a resting casing only checks now and then that its floor is still
// there (a lift gone down). Going into water, slime or lava it makes a tiny splash, a ripple and a quiet plip
// (enterLiquid), and the liquid takes most of its speed: it then sinks slowly.

#include "vr_random.hpp"
#include "vr_modelmetadata.hpp"
#include "vr_shells.hpp"
#include "vr_autopump.hpp"
#include "vr_engine.hpp"
#include "vr_anchor.hpp"
#include "vr_backend.hpp"
#include "vr_cvars.hpp"
#include "vr_tunables.hpp"
#include "vr_flatplay.hpp"
#include "vr_flatreload.hpp"
#include "vr_flick.hpp"
#include "vr_gunsfx.hpp"
#include "vr_hands.hpp"
#include "vr_gunfx.hpp"
#include "vr_particles.hpp"
#include "vr_profile.hpp"
#include "vr_trace.hpp"
#include "vr_units.hpp"
#include "vr_water.hpp"

#include "Zancle/Base/GetArraySize.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/PtrDiffT.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Chrono/Clock.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/Random/FastNonCryptoRng.hpp"
#include "vr_zancle.hpp"
#include "vr_audio.hpp"
#include "vr_geom.hpp"

#include <string.h>

namespace qvr::shells
{
namespace
{

enum Flags : int
{
    FlagFlick = 1, // a flick reload: flung by the spinning weapon
    FlagPushed = 2, // thrown out by the hand's push (the heavy revolver's: QC VR_Revolver_Push): on with the port's velocity
    FlagAfterOpen = 4, // the super shotgun broken open (flat-screen play, QC QVR_EJECT_AFTER_OPEN): the QC's delay counts from
                       // when it is drawn fully open, its barrels down and its open pose eased in (his words 2026-11-10)
};

// The tunable ejections (his words 2026-11-10: "make the shells ejected from the SSG and revolver cylinder fly off away
// from the ejection sites"): a weapon's speed, way out (in the gun's frame: back, up and outward, away from the body's
// middle, mirrored with the model in the off hand), random spread (a share of the speed) and tumble.
enum class Tune : int
{
    Table, // the table's own
    Ssg,   // vr_shells_ssg_*
    Rv,    // vr_shells_rv_*
};

// A weapon's ejection ports, in its model's space (as its frames' vertices: +x forward, +y left,
// +z up; mirrored with the model in the off hand), picked on the models (v_shot: just out of the
// ejection port on the receiver's right side, improve_weapons3.py; v_shot2: the two chambers at the
// breech). Casings go out along `dir` at `speed` metres per second, tumbling about `spinAxis`
// (model space) at `spin` radians per second. `anchor` is a point near the ports (an anchor, see
// vr_anchor.hpp) whose move from frame 0 moves them with the firing animation (the recoil).
struct Weapon
{
    modelmeta::Id model;
    int ports;
    glm::vec3 port[2];
    glm::vec3 dir[2];
    float speed;
    glm::vec3 spinAxis;
    float spin;
    float smoke;  // puff at the port,
    int sparks;   // and sparks
    const char* anchor;
    Tune tune{Tune::Table}; // speed, way out, spread and spin from the tunables (`dir`'s sideways split per port kept)
};

constexpr Weapon weaponTable[] = {
    {modelmeta::Id::VShot, 1, {{14.5f, -2.75f, 4.65f}}, {{-0.3f, -1.f, 0.65f}}, 1.6f, {0.f, 0.f, 1.f}, -16.f, 1.f, 3,
        "14.372 -1.7654 6.1451"},
    {modelmeta::Id::VShot2, 2, {{11.6f, 1.2f, 6.9f}, {11.6f, -1.2f, 6.9f}}, {{-1.f, 0.15f, 0.5f}, {-1.f, -0.15f, 0.5f}},
        1.6f, {0.f, 1.f, 0.f}, 9.f, 1.5f, 1, "11.6867 1.5246 7.4443", Tune::Ssg},
    // AD support: the Widowmaker's ejection slot (vr_adweapons.cpp widowmakerParts), on the receiver's outer side (-y),
    // opposite its shell port: out sideways and up, onto the ground (as the shotgun's).
    {modelmeta::Id::AdVrWidowmaker, 1, {{7.25f, -3.9f, 7.7f}}, {{-0.25f, -1.f, 0.6f}}, 1.6f, {0.f, 0.f, 1.f}, -16.f, 1.f, 3,
        "12.5 -3.6 9.2"},
    // The heavy revolver (QC vr_revolver.qc): its spent shells drop out of the back of its swung-out cylinder (eject:
    // out of its chambers as drawn), slowly, back along the bore.
    {modelmeta::Id::VRevolver, 1, {{3.4f, 0.f, 5.608f}}, {{-1.f, 0.f, 0.f}}, 0.5f, {0.f, 1.f, 0.f}, 4.f, 0.6f, 0,
        "3.4 0 5.608", Tune::Rv},
};

// A weapon's ejection as tuned now: its way out (model space, not normalized: `split` the port's own sideways share),
// speed (m/s), spread (a share of it) and spin (rad/s).
struct Ejection
{
    glm::vec3 dir;
    float speed;
    float spread;
    float spin;
};

[[nodiscard]] Ejection ejectionFor(const Weapon& w, int port)
{
    const float split = w.dir[port].y; // (the super shotgun's chambers: each a little to its own side)
    switch(w.tune)
    {
        case Tune::Ssg:
            return {glm::vec3{-vr_shells_ssg_back.value, split - vr_shells_ssg_out.value, vr_shells_ssg_up.value},
                za::max(0.f, vr_shells_ssg_speed.value), za::clamp(vr_shells_ssg_spread.value, 0.f, 1.f), vr_shells_ssg_spin.value};
        case Tune::Rv:
            return {glm::vec3{-vr_shells_rv_back.value, split - vr_shells_rv_out.value, vr_shells_rv_up.value},
                za::max(0.f, vr_shells_rv_speed.value), za::clamp(vr_shells_rv_spread.value, 0.f, 1.f), vr_shells_rv_spin.value};
        case Tune::Table: break;
    }
    return {w.dir[port], w.speed, 0.15f, w.spin};
}

// The casings' models and skins (the QC's `kind`: QVR_CASING_SHOTGUNSHELL 0 the red shotgun shell, QVR_CASING_SLUG 1
// the heavy revolver's blue slug, make_shell.py's skin 1).
constexpr const char* kindModels[] = {"progs/vr_shell.mdl", "progs/vr_shell.mdl"};
constexpr int kindSkins[] = {0, 1};
static_assert(za::getArraySize(kindSkins) == za::getArraySize(kindModels));

// Their sounds: on a hard surface, into a liquid.
constexpr const char* tinkSounds[] = {"vr/shell_tink1.wav", "vr/shell_tink2.wav", "vr/shell_tink3.wav"};
constexpr const char* plipSounds[] = {"vr/shell_plip1.wav", "vr/shell_plip2.wav", "vr/shell_plip3.wav"};

// Quake's weapons are drawn about a quarter bigger than the real ones (the shotgun is 1.25 m
// long): the shells are too, to match them.
constexpr float modelScale = 1.25f;
// Half the shell's width (the rim's, make_shell.py) in metres, as drawn: how high a lying one's
// middle is.
constexpr float shellRadius = 0.0112f * modelScale;
constexpr int maxShells = 64;
constexpr float fadeTime = 1.5f; // seconds it fades out over, at the end of vr_shells_life
constexpr float trailTime = 1.2f; // seconds it trails smoke for

// A flick reload's casings leave when the spin has turned the barrels down (the breech open
// upwards, in front of the hand): thrown forward and a little up, away from the head, they fall
// in front of the player. They wait for it at most this long (the spin takes a third of a second).
constexpr float flickEjectAngle = 255.f;
constexpr double flickEjectWait = 0.6;
// A broken-open super shotgun's casings wait for it to be drawn fully open at most this long (FlagAfterOpen).
constexpr double afterOpenWait = 2.5;

struct Shell
{
    bool active{false};
    int kind{0};
    glm::vec3 pos{0.f};
    glm::vec3 vel{0.f};
    glm::quat rot{1.f, 0.f, 0.f, 0.f};
    glm::vec3 angVel{0.f}; // radians per second, world axes
    glm::vec3 ground{0.f, 0.f, 1.f}; // the floor's normal, while on it
    double born{0.0};
    double nextCheck{0.0}; // resting: when to look for the floor again
    double nextTink{0.0};
    float smoke{1.f};
    int tinks{0};
    bool onGround{false};
    bool resting{false};
    bool wet{false}; // in a liquid (as of its last move)
    // The heavy revolver's: units it goes straight out of its chamber (the ejector's stroke, from `strokeFrom`) before
    // the face's rule turns it (awayFromFace, to `strokeOut`'s side: away from the frame); 0: done.
    float stroke{0.f};
    int hand{0};
    glm::vec3 strokeFrom{0.f};
    glm::vec3 strokeOut{0.f};
    entity_t ent{};
};

struct Pending
{
    int hand;
    int kind;
    int count;
    int flags;
    double time;    // when it goes (the QC's delay),
    double created; // when the QC said so (the shot: the shotgun's auto pump stroke it waits for)
    double opened{-1.0}; // FlagAfterOpen: when the gun was drawn fully open (the QC's delay counts from it)
};

// Each hand's ejection port as last seen, for its speed.
struct Track
{
    const qmodel_t* model{nullptr};
    glm::vec3 pos{0.f};
    glm::vec3 vel{0.f};
    double time{-1.0};
};

Shell shells[maxShells];
za::Vector<Pending> pending;
Track tracks[2];
double lastRun = -1.0;
int lastFrame = -1;

za::FastNonCryptoRng rng{static_cast<za::U64>(za::Clock::nowNanoseconds())}; // a new sequence every run

[[nodiscard]] glm::vec3 onSphere()
{
    const float z = vrand::range(rng, -1.f, 1.f);
    const float a = vrand::range(rng, 0.f, 6.2831853f);
    const float r = za::sqrt(za::max(0.f, 1.f - z * z));
    return {r * za::cos(a), r * za::sin(a), z};
}

[[nodiscard]] float unitsPerMetre()
{
    return units::metresToUnits();
}

[[nodiscard]] float gravity()
{
    return 9.81f * unitsPerMetre() * za::clamp(sv_gravity.value / 800.f, 0.f, 4.f);
}

[[nodiscard]] float restHeight()
{
    return shellRadius * unitsPerMetre();
}

[[nodiscard]] bool inLiquid(const glm::vec3& p)
{
    return worldtrace::isLiquid(worldtrace::liquidContents(p, true));
}

[[nodiscard]] const Weapon* weaponFor(const qmodel_t* model)
{
    if(!model)
    {
        return nullptr;
    }
    const auto& info = modelmeta::get(model);
    for(const Weapon& w : weaponTable)
    {
        if(info.is(w.model))
        {
            return &w;
        }
    }
    return nullptr;
}

// Where the firing animation has moved the ports from frame 0 (model units).
[[nodiscard]] glm::vec3 animationShift(const view::ViewEntity& ve, const Weapon& w)
{
    if(!w.anchor || !ve.ent.model || ve.ent.model->type != mod_alias)
    {
        return glm::vec3{0.f};
    }
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(ve.ent.model));
    const glm::vec3 now = anchor::posedAnchor(ve.ent, w.anchor, ve.zeroBlend);
    const glm::vec3 rest = anchor::posedAnchor(ve.ent, w.anchor, 1.f);
    return (now - rest) * geom::vec(hdr->scale);
}

// A direction in the model's space, in the world.
[[nodiscard]] glm::vec3 worldDir(const view::ViewEntity& ve, const glm::vec3& at, const glm::vec3& dir)
{
    const glm::vec3 d = view::modelPoint(ve, at + dir) - view::modelPoint(ve, at);
    const float l = glm::length(d);
    return l > 1e-6f ? d / l : glm::vec3{0.f, 0.f, 1.f};
}

// Follows each hand's first port, for the speed a casing leaves with (the hand's swing, a flick's
// spin, the player's own movement).
void trackPorts(const view::ViewEntity (&weapons)[2])
{
    for(int hand = 0; hand < 2; hand++)
    {
        const view::ViewEntity& ve = weapons[hand];
        Track& t = tracks[hand];
        const Weapon* w = ve.visible ? weaponFor(ve.ent.model) : nullptr;
        if(!w)
        {
            t = Track{};
            continue;
        }

        const glm::vec3 pos = view::modelPoint(ve, w->port[0] + animationShift(ve, *w));
        const float dt = static_cast<float>(cl.time - t.time);
        if(t.model == ve.ent.model && t.time >= 0.0 && dt > 0.f && dt < 0.2f)
        {
            glm::vec3 v = (pos - t.pos) / dt;
            const float speed = glm::length(v);
            const float cap = 12.f * unitsPerMetre(); // a teleport or a respawn is no swing
            v = speed > cap * 3.f ? glm::vec3{0.f} : speed > cap ? v * (cap / speed) : v;
            t.vel = glm::mix(t.vel, v, 0.6f);
        }
        else if(t.model != ve.ent.model)
        {
            t.vel = glm::vec3{0.f};
        }
        if(dt > 0.f || t.time < 0.0)
        {
            t.model = ve.ent.model;
            t.pos = pos;
            t.time = cl.time;
        }
    }
}

[[nodiscard]] Shell& newShell()
{
    Shell* oldest = &shells[0];
    for(Shell& s : shells)
    {
        if(!s.active)
        {
            return s;
        }
        if(s.born < oldest->born)
        {
            oldest = &s;
        }
    }
    return *oldest;
}

// A flick's casing, thrown out of the spinning gun: forward (the way the head looks, turned a
// little away from it towards where the gun is) and a little up, scattered, with a quarter of the
// hand's own swing (not the spin's speed, which flung them into the face) and the player's movement.
[[nodiscard]] glm::vec3 flickVelocity(int hand, const glm::vec3& pos)
{
    const hands::State& hs = hands::current();
    const float upm = unitsPerMetre();
    const glm::vec3 look = hands::forward({0.f, hs.headAngles.y, 0.f});
    glm::vec3 away{pos.x - hs.head.x, pos.y - hs.head.y, 0.f};
    away = glm::length(away) > 1e-3f ? glm::normalize(away) : look;
    glm::vec3 out = look * 0.7f + away * 0.3f;
    out = glm::length(out) > 1e-3f ? glm::normalize(out) : look;
    const glm::vec3 side = glm::normalize(glm::cross(out, glm::vec3{0.f, 0.f, 1.f}));

    glm::vec3 handVel = hs.vel[hand] * upm * 0.25f;
    const float cap = 1.5f * upm;
    if(glm::length(handVel) > cap)
    {
        handVel *= cap / glm::length(handVel);
    }
    const float speed = vrand::range(rng, 1.1f, 1.7f) * upm;
    return out * speed + side * (vrand::range(rng, -0.35f, 0.35f) * upm) + glm::vec3{0.f, 0.f, vrand::range(rng, 0.3f, 0.7f) * upm} + handVel +
           geom::vec(cl.velocity);
}

// Never at the face: a casing starting within an arm's length of the head does not fly towards it
// (it goes away from it at least a little, along the ground), and does not rise to the eyes. The speed it had towards
// the face is turned sideways past it (vr_shells_face_deflect of it: a gun's breech facing the eyes, the super shotgun
// broken open in front of them or the revolver's cylinder, throws its shells off to the side instead of dropping them
// straight down; his words 2026-11-10): to the side it already goes to, else the side of the look the gun is on.
// `prefer` (a world way, or none): the side to turn it to when that way runs across the line to the head (the revolver's
// shells: out, away from its frame, the side its cylinder hangs out on; not across the gun).
[[nodiscard]] glm::vec3 awayFromFace(const glm::vec3& pos, glm::vec3 vel, int hand, const glm::vec3* prefer = nullptr)
{
    const hands::State& hs = hands::current();
    if(!hs.valid)
    {
        return vel;
    }
    const float upm = unitsPerMetre();
    const glm::vec3 player{cl.velocity[0], cl.velocity[1], 0.f};
    glm::vec3 toHead{hs.head.x - pos.x, hs.head.y - pos.y, 0.f};
    const float dist = glm::length(toHead);
    if(dist >= 1.f * upm || dist < 1e-3f)
    {
        return vel;
    }
    toHead /= dist;
    const float towards = glm::dot(vel - player, toHead); // relative to the player, who moves along
    const float wanted = -0.3f * upm;
    if(towards > wanted)
    {
        vel -= toHead * (towards - wanted);
        if(const float deflect = za::clamp(vr_shells_face_deflect.value, 0.f, 1.f); deflect > 0.f)
        {
            // The side: a strong sideways throw's own (half the speed towards the face or more: a side port); else the
            // side of the look the gun is on (not the scatter's: a shell from the right hand's gun never crosses the
            // view); centred (in the sights), the throw's own side, else the hand's.
            glm::vec3 side{-toHead.y, toHead.x, 0.f}; // (horizontal, across the line to the head)
            const float across = glm::dot(vel - player, side);
            const glm::vec3 look = hands::forward({0.f, hs.headAngles.y, 0.f});
            const glm::vec3 right{look.y, -look.x, 0.f};
            const float off = glm::dot(pos - hs.head, right);
            const float toRight = glm::dot(side, right) >= 0.f ? 1.f : -1.f; // (`side` to the look's right: +1)
            float sign = 0.f;
            if(const float pref = prefer ? glm::dot(*prefer, side) : 0.f; za::fabs(pref) > 0.2f)
            {
                sign = pref > 0.f ? 1.f : -1.f;
            }
            else if(za::fabs(across) >= 0.5f * (towards - wanted))
            {
                sign = across > 0.f ? 1.f : -1.f;
            }
            else if(za::fabs(off) > 1.f)
            {
                sign = (off > 0.f ? 1.f : -1.f) * toRight;
            }
            else if(za::fabs(across) > 0.05f * upm)
            {
                sign = across > 0.f ? 1.f : -1.f;
            }
            else
            {
                sign = (hand == HAND_MAIN ? 1.f : -1.f) * toRight;
            }
            vel += side * (sign * deflect * (towards - wanted));
        }
    }
    // Its highest point at least 15 cm under the eyes, unless it is clear of the face by then (vr_shells_face_clear metres
    // to the side or ahead of the head, level: thrown off past it, it may rise; never back towards the head, so it only
    // gets further from it on the way).
    const float g = gravity();
    if(vel.z > 0.f && g > 0.f)
    {
        const float peak = vel.z / g;
        const glm::vec3 at = pos + (vel - player) * peak;
        const float clear = za::max(0.f, vr_shells_face_clear.value) * upm;
        if(glm::length(glm::vec3{at.x - hs.head.x, at.y - hs.head.y, 0.f}) >= clear)
        {
            return vel;
        }
    }
    const float room = hs.head.z - 0.15f * upm - pos.z;
    const float maxUp = room > 0.f ? za::sqrt(2.f * g * room) : 0.f;
    vel.z = za::min(vel.z, maxUp);
    return vel;
}

// The shotgun's spent shell waits for its auto pump's stroke to reach the back (vr_autopump.cpp), instead of the QC's
// delay: true with that time in `rear` and the stroke's average speed back (model units per second) in `backSpeed`.
[[nodiscard]] bool pumpedEject(const view::ViewEntity (&weapons)[2], const Pending& p, double& rear, float& backSpeed)
{
    const view::ViewEntity& ve = weapons[p.hand];
    const Weapon* w = ve.visible && !(p.flags & FlagFlick) ? weaponFor(ve.ent.model) : nullptr;
    return w && w->model == modelmeta::Id::VShot && autopump::rearTime(p.hand, p.created, rear, backSpeed);
}

void eject(const view::ViewEntity (&weapons)[2], const Pending& p)
{
    const view::ViewEntity& ve = weapons[p.hand];
    const Weapon* w = ve.visible ? weaponFor(ve.ent.model) : nullptr;
    if(!w || p.kind < 0 || p.kind >= static_cast<int>(za::getArraySize(kindModels)))
    {
        return; // the weapon is gone from the hand (thrown, holstered) or has no port
    }

    const bool flick = (p.flags & FlagFlick) != 0;
    const glm::vec3 shift = animationShift(ve, *w);
    const float upm = unitsPerMetre();
    double rear = 0.0;
    float backSpeed = 0.f;
    const bool pumped = pumpedEject(weapons, p, rear, backSpeed);
    if(vr_debug_weaponfx.value)
    {
        Con_Printf("shells eject hand %d kind %d skin %d x%d t %.3f%s\n", p.hand, p.kind, kindSkins[p.kind], p.count, cl.time,
            pumped                       ? va(" (auto pump back at %.3f)", rear)
            : (p.flags & FlagAfterOpen) ? va(" (%.3f s after the break, %.3f s after it was drawn open)", cl.time - p.created,
                                               cl.time - p.opened)
                                         : va(" (due %.3f)", p.time));
    }

    for(int i = 0; i < p.count; i++)
    {
        const int port = i % w->ports;
        glm::vec3 at = w->port[port] + shift;
        const Ejection ej = ejectionFor(*w, port);
        glm::vec3 portDir = ej.dir;
        if(const float open = w->model == modelmeta::Id::VShot2 ? view::ssgOpenAngle(p.hand) : 0.f; open > 0.f)
        {
            // The super shotgun broken open (immersive reloading): out of its chambers as they are drawn, turned down
            // with the barrels (vr_view.cpp; make_ssg_open.py's breech face at x 12.7, z 7), backwards and up.
            at = view::ssgTurned(glm::vec3{12.7f, w->port[port].y, 7.f}, open) + shift;
            portDir = view::ssgTurned(portDir, open, false);
        }
        const bool revolver = w->model == modelmeta::Id::VRevolver;
        glm::vec3 rvOut{0.f, 1.f, 0.f};
        if(revolver)
        {
            // The heavy revolver swung out: each out of a chamber of its cylinder as drawn (vr_view.cpp
            // setupRevolverParts: the crane's swing, the cylinder's own turn; its middle 0.364 behind the back face),
            // back along the bore and out, away from the frame (the side the cylinder hangs out on: not across the gun).
            rvOut = view::revolverOutward(view::revolverSwing(p.hand));
            at = view::revolverChamber(p.hand, i % 6, -0.364f) + shift;
            portDir = glm::vec3{-vr_shells_rv_back.value, 0.f, vr_shells_rv_up.value} + rvOut * vr_shells_rv_out.value;
        }
        const glm::vec3 pos = view::modelPoint(ve, at);

        // The weapon's axes at the port (right-handed again when the model is mirrored).
        const glm::vec3 fwd = worldDir(ve, at, {1.f, 0.f, 0.f});
        glm::vec3 up = worldDir(ve, at, {0.f, 0.f, 1.f});
        const glm::vec3 left = glm::normalize(glm::cross(up, fwd));
        up = glm::cross(fwd, left);
        const glm::vec3 dir = worldDir(ve, at, portDir);
        if(vr_debug_weaponfx.value)
        {
            // (Its way out along the model's +y, as drawn: mirrored in the off hand. A side port's load point faces +y.)
            Con_Printf("shells out hand %d: along the model's +y %.2f, up %.2f\n", p.hand,
                glm::dot(dir, worldDir(ve, at, {0.f, 1.f, 0.f})), glm::dot(dir, up));
        }

        Shell& s = newShell();
        s = Shell{};
        s.active = true;
        s.kind = p.kind;
        s.born = cl.time;
        s.pos = pos;
        s.wet = inLiquid(pos);
        s.smoke = flick ? 1.5f : 1.f;

        if(flick)
        {
            s.vel = flickVelocity(p.hand, pos);
        }
        else
        {
            const float speed = ej.speed * upm * vrand::range(rng, 0.8f, 1.2f);
            const glm::vec3 scatter = onSphere() * (speed * ej.spread);
            const float carried = (p.flags & FlagPushed) ? vr_revolver_eject_impulse.value : 0.9f;
            s.vel = dir * speed + scatter + tracks[p.hand].vel * carried;
            if(vr_debug_weaponfx.value && (p.flags & FlagPushed))
            {
                Con_Printf("shells pushed hand %d: port %.2f m/s, out at %.2f m/s, %.2f along the port's velocity\n", p.hand,
                    glm::length(tracks[p.hand].vel) / upm, glm::length(s.vel) / upm,
                    glm::dot(s.vel, glm::normalize(tracks[p.hand].vel + glm::vec3{1e-4f})) / upm);
            }
            if(pumped)
            {
                // Thrown back a little with the action, as fast as the stroke went back on average (a share of it).
                const glm::vec3 back = view::modelPoint(ve, at - glm::vec3{backSpeed, 0.f, 0.f}) - view::modelPoint(ve, at);
                s.vel += back * 0.4f;
            }
        }
        const glm::vec3 before = s.vel;
        s.hand = p.hand;
        s.strokeOut = revolver ? worldDir(ve, at, rvOut) : glm::vec3{0.f};
        if(revolver && !flick && vr_shells_rv_stroke.value > 0.f)
        {
            // Straight out of the back of its chamber first (the ejector's push), then turned past the face (fly).
            s.stroke = vr_shells_rv_stroke.value * upm;
            s.strokeFrom = pos;
        }
        else
        {
            s.vel = awayFromFace(pos, s.vel, p.hand, revolver ? &s.strokeOut : nullptr);
        }
        if(vr_debug_weaponfx.value)
        {
            // (In the look's frame, relative to the player's own movement: forward, right, up, m/s; and the gun's.)
            const hands::State& hs = hands::current();
            const glm::vec3 look = hands::forward({0.f, hs.headAngles.y, 0.f});
            const glm::vec3 right{look.y, -look.x, 0.f};
            const glm::vec3 rel = s.vel - geom::vec(cl.velocity);
            const glm::vec3 was = before - geom::vec(cl.velocity);
            Con_Printf("shells velocity hand %d: forward %.2f right %.2f up %.2f m/s (before the face's rule %.2f %.2f %.2f)\n",
                p.hand, glm::dot(rel, look) / upm, glm::dot(rel, right) / upm, rel.z / upm, glm::dot(was, look) / upm,
                glm::dot(was, right) / upm, was.z / upm);
            Con_Printf("shells from the eyes hand %d: %.2f m forward %.2f right %.2f down, the gun at %.2f m/s\n", p.hand,
                glm::dot(pos - hs.head, look) / upm, glm::dot(pos - hs.head, right) / upm, (hs.head.z - pos.z) / upm,
                glm::length(tracks[p.hand].vel) / upm);
            if(w->model == modelmeta::Id::VRevolver)
            {
                // (Against its cylinder as drawn: back along its axis, out (away from the frame, the side the cylinder
                // hangs out on), the gun's up; how far off straight back it goes; how far it starts off its chamber.)
                const float swing = view::revolverSwing(p.hand);
                const glm::vec3 out = worldDir(ve, at, view::revolverOutward(swing));
                const glm::vec3 chamber = view::modelPoint(ve, view::revolverChamber(p.hand, i % 6, 0.f));
                const glm::vec3 rel = s.vel - geom::vec(cl.velocity) - tracks[p.hand].vel * 0.9f;
                const float len = glm::length(rel);
                const float off = len > 1e-4f ? glm::degrees(std::acos(za::clamp(glm::dot(rel / len, -fwd), -1.f, 1.f))) : 0.f;
                const glm::vec3 d = pos - chamber;
                Con_Printf("shells rv hand %d chamber %d: back %.2f out %.2f up %.2f m/s, %.0f deg off the axis; starts "
                           "%.2f across its chamber's axis, %.2f along\n",
                    p.hand, i % 6, glm::dot(rel, -fwd) / upm, glm::dot(rel, out) / upm, glm::dot(rel, up) / upm, off,
                    glm::length(d - fwd * glm::dot(d, fwd)), glm::dot(d, fwd));
            }
        }

        // Lying in the chamber: its open end forward, the weapon's up its up.
        s.rot = glm::quat_cast(glm::mat3{fwd, left, up});
        const glm::vec3 spinAxis = worldDir(ve, at, w->spinAxis);
        s.angVel = spinAxis * (ej.spin * vrand::range(rng, 0.75f, 1.25f)) + onSphere() * vrand::range(rng, 1.f, 4.f);
        if(flick)
        {
            s.angVel += onSphere() * vrand::range(rng, 12.f, 22.f);
        }

        particles::shellEject(pos, dir, w->smoke * (flick ? 1.5f : 1.f), w->sparks);
        if(w->model != modelmeta::Id::VRevolver && i < w->ports)
        {
            gunfx::portSmoke(pos, dir, flick); // the port's quick puff (the revolver's cylinder: none)
        }
    }
}

void tink(Shell& s, float impact, const glm::vec3& normal)
{
    const float threshold = 0.7f * unitsPerMetre(); // metres per second into the surface
    if(vr_shells_sound.value <= 0.f || impact < threshold || s.tinks >= 4 || cl.time < s.nextTink || inLiquid(s.pos))
    {
        return;
    }
    s.tinks++;
    s.nextTink = cl.time + 0.06;

    // Of what it landed on: metal's ring, wood's knock; stone (and the rest) the tinks.
    const int k = rng.getI(0, 2);
    const char* surface = gunsfx::casingSound(s.pos, normal, k);
    sfx_t* sfx = S_PrecacheSound(surface ? surface : tinkSounds[k]);
    if(!sfx)
    {
        return;
    }
    const float loud = za::clamp(impact / (4.f * unitsPerMetre()), 0.15f, 1.f);
    vec3_t org{s.pos.x, s.pos.y, s.pos.z};
    const float pct = za::clamp(vr_snd_pitch_jitter.value, 0.f, 25.f) * 0.01f; // (a little higher or lower each time)
    S_StartSoundPitch(0, 0, sfx, org, za::min(vr_shells_sound.value, 1.f) * 0.45f * loud, 2.f, 1.f + rng.getF(-pct, pct));
}

// Going into a liquid between `dry` and `wet` (a move's ends): where it crosses the surface, a tiny splash (the splash
// preset's parts at a casing's size, particles::shellSplash), the smallest ripple on the liquid (water::addRipple) and
// a quiet plip (the recorded plips pitched up, make_sounds.py), louder the faster it goes in; the liquid takes most of
// its speed and spin. Many can land at once (a flick reload's pair, a burst of shots): at most a few splashes and two
// sounds each tenth of a second.
// enterLiquid's limits: at most a few splashes and two sounds each tenth of a second (cl.time), never the same plip twice.
struct Landings
{
    double window = -1.0;
    int splashes = 0, sounds = 0;
    int lastPlip = -1;
};
Landings landings;

void enterLiquid(Shell& s, glm::vec3 dry, glm::vec3 wet)
{
    for(int i = 0; i < 8; i++)
    {
        const glm::vec3 mid = (dry + wet) * 0.5f;
        (inLiquid(mid) ? wet : dry) = mid;
    }
    const glm::vec3 surface = (dry + wet) * 0.5f;
    const float upm = unitsPerMetre();
    const float speed = glm::length(s.vel);
    const float strength = za::clamp((speed / upm - 0.5f) / 3.5f, 0.f, 1.f); // 0.5 .. 4 m/s
    const glm::vec3 dir = speed > 1e-3f ? s.vel / speed : glm::vec3{0.f, 0.f, -1.f};

    double& window = landings.window;
    int& splashes = landings.splashes;
    int& sounds = landings.sounds;
    if(cl.time - window > 0.1 || cl.time < window)
    {
        window = cl.time;
        splashes = sounds = 0;
    }
    const bool splashed = splashes < 4;
    if(splashed)
    {
        splashes++;
        particles::shellSplash(surface, dir, strength);
        water::addRipple(surface, 0.4f + 0.5f * strength); // 1.1 to 2.3 units at the shipped amplitude
    }
    const float volume = za::min(vr_shells_sound.value, 1.f) * za::clamp(vr_water_sounds.value, 0.f, 1.f);
    if(developer.value >= 2)
    {
        Con_Printf("VR shell into a liquid: %.1f %.1f %.1f, %.2f m/s%s%s\n", surface.x, surface.y, surface.z, speed / upm,
            splashed ? ", splash" : "", sounds < 2 && volume > 0.f ? ", plip" : "");
    }
    if(sounds < 2 && volume > 0.f)
    {
        int& last = landings.lastPlip;
        int k = rng.getI(0, 1);
        k += last >= 0 && k >= last ? 1 : 0; // never the one played last
        last = k;
        if(sfx_t* sfx = S_PrecacheSound(plipSounds[k]))
        {
            sounds++;
            audio::playAt(sfx, surface, volume * (0.25f + 0.3f * strength), 2.f);
        }
    }

    s.vel *= 0.3f;
    s.angVel *= 0.5f;
    s.smoke = 0.f;
}

// The rotation turning unit vector `a` onto `b` (not opposite ones: a lying shell is never
// upside down along the floor's plane).
[[nodiscard]] glm::quat shortestArc(const glm::vec3& a, const glm::vec3& b)
{
    const glm::vec3 c = glm::cross(a, b);
    const glm::quat q{1.f + glm::dot(a, b), c.x, c.y, c.z};
    const float l = glm::length(q);
    return l > 1e-4f ? q / l : glm::quat{1.f, 0.f, 0.f, 0.f};
}

void spinBy(Shell& s, const glm::vec3& angVel, float dt)
{
    const float a = glm::length(angVel) * dt;
    if(a > 1e-6f)
    {
        s.rot = glm::normalize(glm::angleAxis(a, glm::normalize(angVel)) * s.rot);
    }
}

void fly(Shell& s, float dt)
{
    if(s.stroke > 0.f && glm::length(s.pos - s.strokeFrom) >= s.stroke)
    {
        // The revolver's shell out of its chamber: now the face's rule (past it, out, away from the frame).
        s.stroke = 0.f;
        s.vel = awayFromFace(s.pos, s.vel, s.hand, &s.strokeOut);
        if(vr_debug_weaponfx.value)
        {
            const glm::vec3 rel = s.vel - geom::vec(cl.velocity);
            const float upm = unitsPerMetre();
            Con_Printf("shells rv stroke done hand %d after %.3f s: out %.2f (away from the frame), up %.2f m/s\n", s.hand,
                cl.time - s.born, glm::dot(rel, s.strokeOut) / upm, rel.z / upm);
        }
    }
    const bool liquid = s.wet;
    s.vel.z -= gravity() * dt * (liquid ? 0.2f : 1.f);
    s.vel *= za::exp(-(liquid ? 4.f : 0.15f) * dt);
    s.angVel *= za::exp(-(liquid ? 2.f : 0.1f) * dt);

    const glm::vec3 from = s.pos;
    const glm::vec3 to = s.pos + s.vel * dt;
    const trace_t tr = worldtrace::world(from, to);
    if(tr.startsolid || tr.allsolid)
    {
        s.resting = true; // stuck in something: leave it be
        s.vel = glm::vec3{0.f};
        return;
    }

    if(tr.fraction < 1.f)
    {
        const glm::vec3 n = worldtrace::normal(tr);
        const float into = -glm::dot(s.vel, n);
        glm::vec3 tangent = s.vel + n * into;
        tangent *= 0.65f; // friction of the hit
        const float bounce = into * vrand::range(rng, 0.3f, 0.45f);
        s.vel = tangent + n * bounce;
        s.angVel = s.angVel * 0.5f + onSphere() * za::min(into / unitsPerMetre() * 6.f, 25.f);
        s.pos = worldtrace::endPos(tr) + n * restHeight();
        tink(s, into, n);

        if(n.z > 0.7f && bounce < 0.6f * unitsPerMetre())
        {
            s.onGround = true;
            s.ground = n;
            s.vel -= n * glm::dot(s.vel, n);
        }
    }
    else
    {
        s.pos = to;
    }
    if(const bool wet = inLiquid(s.pos); wet != s.wet)
    {
        s.wet = wet;
        if(wet)
        {
            enterLiquid(s, from, s.pos);
        }
    }
    spinBy(s, s.angVel, dt);

    const float age = static_cast<float>(cl.time - s.born);
    if(age < trailTime && !liquid)
    {
        particles::shellTrail(from, s.pos, s.smoke * gunfx::shellTrail() * (1.f - age / trailTime)); // (vr_gunfx_shell_trail)
    }
}

void roll(Shell& s, float dt)
{
    const glm::vec3 n = s.ground;
    const float r = restHeight();

    // Lie down on the side: the axis turns into the floor's plane.
    const glm::vec3 axis = s.rot * glm::vec3{1.f, 0.f, 0.f};
    glm::vec3 flat = axis - n * glm::dot(axis, n);
    flat = glm::length(flat) > 1e-3f ? glm::normalize(flat) : glm::normalize(glm::cross(n, glm::vec3{0.3f, 1.f, 0.f}));
    const glm::quat lieDown = shortestArc(axis, flat);
    s.rot = glm::normalize(glm::slerp(glm::quat{1.f, 0.f, 0.f, 0.f}, lieDown, za::min(1.f, dt * 14.f)) * s.rot);

    // Rolls across its axis, slides along it; both slow down.
    const glm::vec3 along = flat * glm::dot(s.vel, flat);
    const glm::vec3 across = s.vel - along;
    s.vel = along * za::exp(-7.f * dt) + across * za::exp(-2.2f * dt);
    spinBy(s, glm::cross(n, across) / r, dt);

    const glm::vec3 from = s.pos;
    const glm::vec3 to = s.pos + s.vel * dt;
    const trace_t tr = worldtrace::world(s.pos, to);
    if(tr.fraction < 1.f && !tr.startsolid)
    {
        const glm::vec3 wn = worldtrace::normal(tr);
        s.vel -= wn * (1.3f * glm::dot(s.vel, wn)); // off a wall
        s.pos = worldtrace::endPos(tr) + wn * 0.05f;
    }
    else if(!tr.startsolid)
    {
        s.pos = to;
    }
    if(const bool wet = inLiquid(s.pos); wet != s.wet) // rolled into a pool on a sloping floor
    {
        s.wet = wet;
        if(wet)
        {
            enterLiquid(s, from, s.pos);
        }
    }

    // Still on a floor? Off a ledge it falls again.
    const trace_t down = worldtrace::world(s.pos + n * r, s.pos - n * (r + 1.5f));
    if(down.fraction >= 1.f || down.startsolid || worldtrace::normal(down).z <= 0.7f)
    {
        s.onGround = false;
        return;
    }
    s.ground = worldtrace::normal(down);
    s.pos = worldtrace::endPos(down) + s.ground * r;

    if(glm::length(s.vel) < 0.02f * unitsPerMetre() && glm::dot(axis, flat) > 0.999f)
    {
        s.resting = true;
        s.vel = glm::vec3{0.f};
        s.nextCheck = cl.time + vrand::range(rng, 0.3f, 0.6f);
    }
}

// A resting casing looks now and then whether its floor is still under it.
void rest(Shell& s)
{
    if(cl.time < s.nextCheck)
    {
        return;
    }
    s.nextCheck = cl.time + vrand::range(rng, 0.3f, 0.6f);
    const float r = restHeight();
    const trace_t down = worldtrace::world(s.pos, s.pos - s.ground * (r + 1.f));
    if(down.fraction >= 1.f && !down.startsolid)
    {
        s.resting = false;
        s.onGround = false;
        s.tinks = 2; // a quieter landing
    }
}

void addToScene(Shell& s, qmodel_t* model, float life)
{
    const float age = static_cast<float>(cl.time - s.born);
    const float alpha = za::clamp((life - age) / fadeTime, 0.f, 1.f);
    if(!model || cl_numvisedicts >= MAX_VISEDICTS || alpha <= 0.f)
    {
        return;
    }

    entity_t& e = s.ent;
    if(e.model != model)
    {
        e = entity_t{};
        e.lerpflags |= LERP_RESETANIM;
    }
    e.model = model;
    e.frame = 0;
    e.skinnum = kindSkins[s.kind];
    e.colormap = vid.colormap;
    e.scale = static_cast<unsigned char>(CLAMP(1.f, units::worldScale() * ENTSCALE_DEFAULT * modelScale + 0.5f, 255.f));
    e.alpha = alpha >= 1.f ? ENTALPHA_DEFAULT : static_cast<unsigned char>(ENTALPHA_ENCODE(alpha));

    const glm::mat3 m = glm::mat3_cast(s.rot);
    const glm::vec3 a = hands::anglesFromVectors(m[0], m[2]);
    const glm::vec3 angles{-a.x, a.y, a.z}; // alias models' pitch is the other way
    for(int i = 0; i < 3; i++)
    {
        e.origin[i] = s.pos[i];
        e.angles[i] = angles[i];
    }
    cl_visedicts[cl_numvisedicts++] = &e;
}

void ejectTest_f()
{
    if(cls.state != ca_connected)
    {
        return;
    }
    const int hand = Cmd_Argc() > 1 ? CLAMP(0, Q_atoi(Cmd_Argv(1)), 1) : HAND_MAIN;
    const int count = Cmd_Argc() > 2 ? CLAMP(1, Q_atoi(Cmd_Argv(2)), 8) : 1;
    const int flags = Cmd_Argc() > 3 && Q_atoi(Cmd_Argv(3)) ? FlagFlick : 0;
    if(flags & FlagFlick)
    {
        flick::spin(hand); // as a flick: the casings leave with the spin
    }
    pending.pushBack({hand, 0, count, flags, cl.time, cl.time});
}

} // namespace

void parseEject()
{
    const int hand = MSG_ReadByte();
    const int kind = MSG_ReadByte();
    const int count = MSG_ReadByte();
    const int flags = MSG_ReadByte();
    const float delay = MSG_ReadByte() / 100.f;

    if(!vr_shells.value || (hand != HAND_OFF && hand != HAND_MAIN) || pending.size() >= 16)
    {
        return;
    }
    pending.pushBack({hand, kind, za::min(count, 8), flags, cl.time + delay, cl.time});
}

void frame(const view::ViewEntity (&weapons)[2])
{
    if(lastFrame == host_framecount)
    {
        return;
    }
    lastFrame = host_framecount;
    QVR_PROFILE("shells");

    trackPorts(weapons);

    for(za::SizeT i = 0; i < pending.size();)
    {
        // A flick's casings wait for the spin to turn the barrels down.
        Pending& pe = pending[i];
        const float spun = (pe.flags & FlagFlick) ? flick::spinAngle(pe.hand) : -1.f;
        const bool waitSpin = spun >= 0.f && spun < flickEjectAngle && cl.time < pe.time + flickEjectWait;
        // The broken-open super shotgun's wait for it to be drawn fully open: its barrels down (vr_reload_ssg_open_angle)
        // and, in flat-screen play, its open pose eased in; then the QC's delay. (Not the super shotgun in the hand any
        // more, or stuck: at once, at most afterOpenWait after the break.)
        if((pe.flags & FlagAfterOpen) && pe.opened < 0.0)
        {
            const view::ViewEntity& ve = weapons[pe.hand];
            const Weapon* w = ve.visible ? weaponFor(ve.ent.model) : nullptr;
            const float target = za::clamp(vr_reload_ssg_open_angle.value, 0.f, 80.f);
            const bool barrels = view::ssgOpenAngle(pe.hand) >= target - 0.01f;
            const bool posed = !flatplay::active() || flatreload::openShare(pe.hand) >= 0.999f;
            if(!w || w->model != modelmeta::Id::VShot2 || (barrels && posed) || cl.time >= pe.created + afterOpenWait)
            {
                pe.opened = cl.time;
                if(vr_debug_weaponfx.value)
                {
                    Con_Printf("shells: hand %d drawn open %.3f s after the break (barrels %.1f of %.1f deg, pose %.2f)\n", pe.hand,
                        cl.time - pe.created, view::ssgOpenAngle(pe.hand), target, flatreload::openShare(pe.hand));
                }
            }
        }
        // The shotgun's waits for its auto pump's stroke to reach the back instead of the QC's delay.
        double rear = 0.0;
        float backSpeed = 0.f;
        const double due = (pe.flags & FlagAfterOpen) ? (pe.opened < 0.0 ? cl.time + 1.0 : pe.opened + (pe.time - pe.created))
                           : pumpedEject(weapons, pe, rear, backSpeed) ? rear
                                                                       : pe.time;
        if(cl.time >= due && !waitSpin)
        {
            if(vr_shells.value)
            {
                eject(weapons, pending[i]);
            }
            pending.erase(pending.begin() + static_cast<za::PtrDiffT>(i));
        }
        else
        {
            i++;
        }
    }

    const float dt = lastRun >= 0.0 ? static_cast<float>(CLAMP(0.0, cl.time - lastRun, 0.05)) : 0.f;
    lastRun = cl.time;
    const float life = za::max(vr_shells_life.value, 1.f);

    qmodel_t* models[za::getArraySize(kindModels)];
    for(za::SizeT k = 0; k < za::getArraySize(kindModels); k++)
    {
        models[k] = view::viewModel(kindModels[k]); // (Mod_ForName's, kept: not looked up by name every frame)
    }

    for(Shell& s : shells)
    {
        if(!s.active)
        {
            continue;
        }
        if(!vr_shells.value || cl.time - s.born > life || cl.time < s.born)
        {
            s.active = false;
            continue;
        }
        if(dt > 0.f)
        {
            if(s.resting)
            {
                rest(s);
            }
            else if(s.onGround)
            {
                roll(s, dt);
            }
            else
            {
                fly(s, dt);
            }
        }
        addToScene(s, models[s.kind], life);
    }
}

void prepare()
{
    for(const char* name : kindModels)
    {
        (void)Mod_ForName(name, false);
    }
    for(const char* name : tinkSounds)
    {
        (void)S_PrecacheSound(name);
    }
    for(const char* name : plipSounds)
    {
        (void)S_PrecacheSound(name);
    }
}

bool isCasing(const entity_t* e)
{
    for(const Shell& s : shells)
    {
        if(&s.ent == e)
        {
            return true;
        }
    }
    return false;
}

void clear()
{
    for(Shell& s : shells)
    {
        s.active = false;
    }
    pending.clear();
    for(Track& t : tracks)
    {
        t = Track{};
    }
    lastRun = -1.0;
}

void registerCommands()
{
    Cmd_AddCommand("vr_shells_eject", ejectTest_f);
}

// Casings in the world (vr_memstats).
int liveCount()
{
    int n = 0;
    for(const Shell& s : shells)
    {
        n += s.active ? 1 : 0;
    }
    return n;
}

} // namespace qvr::shells
