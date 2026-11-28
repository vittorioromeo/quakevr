// vr_flatprocreload.cpp -- see vr_flatprocreload.hpp. Flatscreen play's procedural reload as drawn (flatprocreload; his
// words 2026-11-28: "When pressing the reload button, the off-hand will go back and forth from the pouch to the shotgun's
// shell entry at the bottom, until the shotgun is full or the player cancels the reloading by firing or performing a
// quick melee. The hand holding the shotgun should slightly tilt it ... so that the shell entry at the bottom is easier to
// reach. We should be able to rely on the same code paths as VR"; docs/vr-port/FLATPLAY.md, "Reloading").
//
// The game decides (QC vr_flatreload.qc, "The procedural reload"): R or the empty clip on the shotgun held in one hand, the
// other empty, with vr_flat_proc_reload on, raises the gun hand's bit in STAT_QVR_FLATRELOAD (16 the off hand's gun, 32 the
// main's) instead of the timed trip to the hip, and drops it when the gun is full, the reserve out, or the reload is
// cancelled (a shot, a quick melee, the weapon changed, its own failsafe). This file drives the free hand while it is up:
//   Reach  to the ammo pouch (shown in flat-screen play while bit 64 is up: vr_body.cpp ammoPouchEnabled), from its pose or
//          from the port (vr_flat_prl_reach_time);
//   Grab   in the pouch: impulse 99 as its hotspot is the pouch's, the server's take (VR_Reload_Take, a headset's grip
//          there: a taped pair, a single shell for the last place) puts the round in the hand (vr_flat_prl_grab_time);
//   Carry  the round to the mouth of the gun's loading port, vr_flat_prl_approach units out of its opening, the way bowed
//          by vr_flat_prl_arc (vr_flat_prl_carry_time);
//   Insert pushed into the port (vr_flat_prl_insert_time): the server's carry loads it as it comes within the port's reach
//          (VR_Carry_HandFrame's VR_Reload_HeldFrame, a headset's own test), and the round leaves the hand: Reach again,
//          or back if the server's bit is down (full);
//   Return to its pose (vr_flat_prl_return_time), when the bit drops, a shot, a punch, or the failsafe.
// Every move starts from where the hand was drawn last (its place off its pose, in the body's frame), so a change of plan
// mid-way (a cancel, the next round) is smooth. The hand's place is the game's too: the server sees it at the pouch and at
// the port (the hotspot, the carried round's place), which is what takes and loads the rounds. The gun's hand tilts the gun
// (vr_flat_prl_tilt_*, mirrored for the left hand) and moves it in (vr_flat_prl_gun_*) to present its port, eased in and
// out (vr_flat_prl_tilt_in_time / _out_time). The arm's IK turns the wrist and forearm with the hand.
//
// Cancels: a shot of the gun's hand (the server cancels it too: its fire button), a punch or shove (impulse 100; the shove's
// and bash's impulses cancel it on the server), the failsafe: a round at the port not seated within vr_flat_prl_failsafe
// seconds (a wall, the hand blocked), or nothing taken at the pouch within it (impulse 100, the reason printed and kept for
// the report). The server puts a round still in the hand back in the reserve.
//
// vr_flat_proc_reload_debug 1 prints each step (flatprocreload: ...); vr_flatprocreload_report the state and counts;
// vr_flat_prl_test_miss N stops the hand N units short of the port (the tests' blocked port).

#include "vr_flatprocreload.hpp"

#include "vr_api.h"
#include "vr_backend.hpp" // HAND_OFF / HAND_MAIN
#include "vr_body.hpp"
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
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "vr_zancle.hpp"

namespace qvr::flatprocreload
{

namespace
{

enum class Phase : int
{
    None,
    Reach,
    Grab,
    Carry,
    Insert,
    Return,
};

// The trip (no function-local statics: a named file-scope struct, CODE_STYLE.md).
struct Trip
{
    Phase phase{Phase::None};
    int gun{HAND_MAIN};          // the gun's hand
    int free{HAND_OFF};          // the hand that reloads it
    double phaseAt{-1e9};        // the phase's start (vr_gametime)
    float t{0.f};                // the phase's share, before its ease
    bool snap{true};             // take the move's start from the hand as drawn last (apply)
    glm::vec3 fromOfs{0.f};      // the move's start: the hand's place off its pose, in the body's frame (fwd, right, up)
    glm::vec3 fromRot{0.f};      // and its turn (its yaw off the body's)
    glm::vec3 lastOfs{0.f};      // the hand as applied last, the same way
    glm::vec3 lastRot{0.f};
    bool applied{false};         // lastOfs / lastRot hold a pose of this trip
    bool takeSent{false};        // impulse 99 went for this grab
    double takeAt{-1e9};
    glm::vec3 roundIn{0.f};      // the carried round's place in the free hand's frame (fwd, left, up), measured at the pouch
    bool roundKnown{false};
    bool atPort{false};          // the insert's push done: waiting for the round to go in
    double atPortAt{-1e9};
    int clipAtCarry{0};          // the gun's clip as the carry started (a round seated: it rose)
};

struct ProcState
{
    Trip trip;
    bool bitUp[2]{false, false}; // the server's procedural bit for each hand's gun, last frame
    float tilt{0.f};             // the gun's tilt share, before its ease
    int tiltHand{HAND_MAIN};
    double lastTime{-1e9};
    // The port as last measured (the gun hand's frame: forward, left, up), and its opening's way.
    glm::vec3 portIn{0.f};
    glm::vec3 faceIn{0.f, 0.f, -1.f};
    bool portKnown{false};
    float portGap{-1.f};         // the carried round's distance from the port as drawn (the report; -1 none)
    float faceToFree{0.f};       // how much the port's opening faces the free hand (cosine; the report)
    int trips{0};                // procedural reloads started (the bit seen coming up)
    int takes{0};                // impulse 99 sent
    int seated{0};               // rounds seen going in (the carried round gone, the clip up)
    int dropped{0};              // carried rounds gone without the clip rising
    int cancels{0};
    const char* lastCancel{"-"};
};

ProcState ps;

constexpr int procBitBase = 16; // STAT_QVR_FLATRELOAD: 16 the off hand's gun, 32 the main's (QC VR_FlatReload_ProcBit)
constexpr int procOnBit = 64;   // the procedural reload on: the pouch shown (QC VR_FLATRL_PROC_ON)
constexpr float pi = 3.14159265f;

[[nodiscard]] float timeOf(const Tunable& c, float least)
{
    return za::max(least, c.value);
}

[[nodiscard]] const char* handName(int h)
{
    return h == HAND_MAIN ? "main" : "off";
}

[[nodiscard]] const char* phaseName(Phase p)
{
    switch(p)
    {
        case Phase::Reach: return "reach";
        case Phase::Grab: return "grab";
        case Phase::Carry: return "carry";
        case Phase::Insert: return "insert";
        case Phase::Return: return "return";
        default: return "none";
    }
}

[[nodiscard]] bool debugOn()
{
    return vr_flat_proc_reload_debug.value != 0.f;
}

[[nodiscard]] float phaseTime(Phase p)
{
    switch(p)
    {
        case Phase::Reach: return timeOf(vr_flat_prl_reach_time, 0.02f);
        case Phase::Grab: return timeOf(vr_flat_prl_grab_time, 0.f);
        case Phase::Carry: return timeOf(vr_flat_prl_carry_time, 0.02f);
        case Phase::Insert: return timeOf(vr_flat_prl_insert_time, 0.02f);
        case Phase::Return: return timeOf(vr_flat_prl_return_time, 0.02f);
        default: return 1.f;
    }
}

[[nodiscard]] float ease(float t)
{
    const float k = za::clamp(vr_flat_prl_ease.value, 0.f, 1.f);
    const float c = za::clamp(t, 0.f, 1.f);
    return c + (geom::smooth01(c) - c) * k;
}

[[nodiscard]] int carriedEnt(int h)
{
    return cl.stats[h == HAND_MAIN ? protocol::STAT_QVR_CARRYMAIN : protocol::STAT_QVR_CARRYOFF];
}

[[nodiscard]] int clipOf(int h)
{
    return cl.stats[h == HAND_MAIN ? protocol::STAT_QVR_WEAPONCLIP : protocol::STAT_QVR_WEAPONCLIP2];
}

[[nodiscard]] float sideOf(int h)
{
    return h == HAND_MAIN ? 1.f : -1.f; // (mirrored for the left hand)
}

// The body's frame at yaw `bodyYaw` (forward, right; up is +z).
void bodyAxes(float bodyYaw, glm::vec3& fwd, glm::vec3& right)
{
    const float yaw = glm::radians(bodyYaw);
    fwd = glm::vec3{za::cos(yaw), za::sin(yaw), 0.f};
    right = glm::vec3{za::sin(yaw), -za::cos(yaw), 0.f};
}

[[nodiscard]] glm::vec3 toBody(const glm::vec3& v, float bodyYaw)
{
    glm::vec3 fwd, right;
    bodyAxes(bodyYaw, fwd, right);
    return {glm::dot(v, fwd), glm::dot(v, right), v.z};
}

[[nodiscard]] glm::vec3 fromBody(const glm::vec3& v, float bodyYaw)
{
    glm::vec3 fwd, right;
    bodyAxes(bodyYaw, fwd, right);
    return fwd * v.x + right * v.y + glm::vec3{0.f, 0.f, v.z};
}

void setPhase(Phase p, const char* why)
{
    Trip& t = ps.trip;
    if(debugOn() && p != t.phase)
    {
        Con_Printf("flatprocreload: %s -> %s (%s; the %s hand, the gun in the %s)\n", phaseName(t.phase), phaseName(p), why,
            handName(t.free), handName(t.gun));
    }
    t.phase = p;
    t.phaseAt = vr_gametime;
    t.t = 0.f;
    t.snap = true;
    if(p == Phase::Grab)
    {
        t.takeSent = false;
    }
    if(p == Phase::Insert)
    {
        t.atPort = false;
    }
}

void cancel(const char* why, bool tellServer)
{
    Trip& t = ps.trip;
    ps.cancels++;
    ps.lastCancel = why;
    if(tellServer)
    {
        Cbuf_InsertText("impulse 100\n");
    }
    if(debugOn())
    {
        Con_Printf("flatprocreload: cancelled (%s)%s\n", why, tellServer ? ": impulse 100" : "");
    }
    if(t.phase != Phase::None && t.phase != Phase::Return)
    {
        setPhase(Phase::Return, why);
    }
}

// The port of the gun in `g` measured in its hand's frame as drawn last (hands::State::loadPort, placed by the view with
// the muzzle), and its opening's way; kept from the last good frame.
void measurePort(int g)
{
    const hands::State& cur = hands::current();
    glm::vec3 pos, rot;
    if(!cur.loadPortValid[g] || !view::drawnHandPose(g, pos, rot))
    {
        return;
    }
    const glm::mat3 toHand = glm::transpose(held::axesFromAngles(&rot[0], true));
    ps.portIn = toHand * (cur.loadPort[g] - pos);
    if(glm::length(cur.loadPortFace[g]) > 1e-3f)
    {
        ps.faceIn = glm::normalize(toHand * cur.loadPortFace[g]);
    }
    ps.portKnown = glm::length(ps.portIn) < 48.f;
}

// The carried round's place in the free hand's frame, from the drawn hand and the round (a carried prop drawn in the hand:
// vr_held.cpp), measured while the hand is still at the pouch.
void measureRound(Trip& t)
{
    const int ent = carriedEnt(t.free);
    glm::vec3 pos, rot;
    if(ent <= 0 || ent >= cl.num_entities || !view::drawnHandPose(t.free, pos, rot))
    {
        return;
    }
    const glm::vec3 r = geom::vec(cl_entities[ent].origin);
    const glm::vec3 in = glm::transpose(held::axesFromAngles(&rot[0], true)) * (r - pos);
    if(glm::length(in) < 16.f)
    {
        t.roundIn = in;
        t.roundKnown = true;
    }
}

void report_f()
{
    const Trip& t = ps.trip;
    const int bits = cl.stats[protocol::STAT_QVR_FLATRELOAD];
    Con_Printf("flatprocreload: phase %s gun %s free %s share %.2f; server bits %d (on %d, off gun %d, main gun %d); carried %d; "
               "tilt %.2f; port %s %.1f %.1f %.1f, round %.1f from it, its opening to the free hand %.2f; hotspot %d\n",
        phaseName(t.phase), handName(t.gun), handName(t.free), t.t, bits, (bits & procOnBit) ? 1 : 0, (bits & procBitBase) ? 1 : 0,
        (bits & (procBitBase * 2)) ? 1 : 0, carriedEnt(t.free), geom::smooth01(ps.tilt), ps.portKnown ? "known" : "unknown",
        ps.portIn.x, ps.portIn.y, ps.portIn.z, ps.portGap, ps.faceToFree, hands::current().hotspot[t.free]);
    Con_Printf("flatprocreload: trips %d takes %d seated %d dropped %d cancels %d last cancel %s\n", ps.trips, ps.takes, ps.seated,
        ps.dropped, ps.cancels, ps.lastCancel);
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_flatprocreload_report", report_f);
}

bool pouchShown()
{
    return flatplay::active() && (cl.stats[protocol::STAT_QVR_FLATRELOAD] & procOnBit) != 0;
}

glm::vec3 pouchOffset(float bodyYaw)
{
    if(!pouchShown())
    {
        return glm::vec3{0.f};
    }
    return fromBody({vr_flat_ammo_pouch_x.value, vr_flat_ammo_pouch_y.value, vr_flat_ammo_pouch_z.value}, bodyYaw);
}

glm::vec3 pouchTurn()
{
    return pouchShown() ? glm::vec3{vr_flat_ammo_pouch_pitch.value, vr_flat_ammo_pouch_yaw.value, vr_flat_ammo_pouch_roll.value}
                        : glm::vec3{0.f};
}

float pouchScale(float headsetScale)
{
    return pouchShown() ? vr_flat_ammo_pouch_scale.value : headsetScale;
}

void frame(bool on)
{
    double dt = ps.lastTime > -1e8 ? vr_gametime - ps.lastTime : 0.0;
    ps.lastTime = vr_gametime;
    dt = za::clamp(dt, 0.0, 0.1);
    Trip& t = ps.trip;
    if(!on)
    {
        ps = ProcState{};
        return;
    }
    const float step = static_cast<float>(dt);
    const int bits = cl.stats[protocol::STAT_QVR_FLATRELOAD];
    int upHand = -1;
    for(int h : {HAND_OFF, HAND_MAIN})
    {
        const bool up = (bits & (procBitBase << h)) != 0;
        if(up && !ps.bitUp[h])
        {
            ps.trips++;
            upHand = h;
        }
        ps.bitUp[h] = up;
    }
    if(upHand >= 0 && (t.phase == Phase::None || t.phase == Phase::Return))
    {
        const bool fresh = t.phase == Phase::None;
        t.gun = upHand;
        t.free = 1 - upHand;
        if(fresh)
        {
            t.applied = false;
        }
        setPhase(Phase::Reach, "the server's reload starts");
    }
    const bool up = t.phase != Phase::None && ps.bitUp[t.gun];
    // The gun's tilt: in while the reload runs, out after.
    if(up)
    {
        ps.tiltHand = t.gun;
    }
    ps.tilt = up ? za::min(1.f, ps.tilt + step / timeOf(vr_flat_prl_tilt_in_time, 0.02f))
                 : za::max(0.f, ps.tilt - step / timeOf(vr_flat_prl_tilt_out_time, 0.02f));
    if(t.phase == Phase::None)
    {
        return;
    }
    if(t.phase != Phase::Return)
    {
        if(!up)
        {
            setPhase(Phase::Return, "the server's reload is over");
        }
        else if(client::attacking(t.gun))
        {
            cancel("fired", false); // (the server cancels it by the fire button)
        }
        else if(flatplay::punching(t.free) || flatplay::punching(t.gun) || flatplay::shoving(t.free) || flatplay::shoving(t.gun))
        {
            cancel("a quick melee", true);
        }
    }
    measurePort(t.gun);
    t.t = za::min(1.f, t.t + step / phaseTime(t.phase));
    const double since = vr_gametime - t.phaseAt;
    const int carried = carriedEnt(t.free);
    const float failsafe = timeOf(vr_flat_prl_failsafe, 0.05f);
    switch(t.phase)
    {
        case Phase::Reach:
            if(t.t >= 1.f)
            {
                setPhase(Phase::Grab, "at the pouch");
            }
            break;
        case Phase::Grab:
            if(!t.takeSent && hands::current().hotspot[t.free] == body::HS_AMMO_POUCH && carried == 0)
            {
                Cbuf_InsertText("impulse 99\n");
                t.takeSent = true;
                t.takeAt = vr_gametime;
                ps.takes++;
                if(debugOn())
                {
                    Con_Printf("flatprocreload: impulse 99: the %s hand takes from the pouch (%.2f s in)\n", handName(t.free), since);
                }
            }
            if(carried > 0 && since >= static_cast<double>(phaseTime(Phase::Grab)) - 1e-4)
            {
                measureRound(t);
                t.clipAtCarry = clipOf(t.gun);
                setPhase(Phase::Carry, "a round in the hand");
            }
            else if(carried == 0 && since >= static_cast<double>(phaseTime(Phase::Grab) + failsafe))
            {
                cancel(t.takeSent ? "failsafe: the pouch gave nothing" : "failsafe: the hand never reached the pouch's hotspot", true);
            }
            break;
        case Phase::Carry:
        case Phase::Insert:
            if(carried == 0)
            {
                const bool in = clipOf(t.gun) > t.clipAtCarry;
                (in ? ps.seated : ps.dropped)++;
                if(debugOn())
                {
                    Con_Printf("flatprocreload: the round %s (%s %.2f s in; clip %d)\n", in ? "went in" : "left the hand",
                        phaseName(t.phase), since, clipOf(t.gun));
                }
                setPhase(Phase::Reach, in ? "seated: the next round" : "lost on the way: another");
            }
            else if(t.phase == Phase::Carry && t.t >= 1.f)
            {
                setPhase(Phase::Insert, "at the port's mouth");
            }
            else if(t.phase == Phase::Insert && t.t >= 1.f)
            {
                if(!t.atPort)
                {
                    t.atPort = true;
                    t.atPortAt = vr_gametime;
                }
                else if(vr_gametime - t.atPortAt >= static_cast<double>(failsafe))
                {
                    cancel("failsafe: the round wasn't seated at the port", true);
                }
            }
            break;
        case Phase::Return:
            if(t.t >= 1.f)
            {
                if(debugOn())
                {
                    Con_Printf("flatprocreload: the %s hand back in its pose\n", handName(t.free));
                }
                t.phase = Phase::None;
                t.applied = false;
            }
            break;
        default: break;
    }
}

void apply(hands::State& s)
{
    // The gun's tilt first (its port, measured in its hand's frame, goes with it).
    const float tilt = geom::smooth01(ps.tilt);
    if(tilt > 0.f)
    {
        const int g = ps.tiltHand;
        const float side = sideOf(g);
        glm::vec3 fwd, right;
        bodyAxes(s.bodyYaw, fwd, right);
        s.rot[g] += glm::vec3{vr_flat_prl_tilt_pitch.value, side * vr_flat_prl_tilt_yaw.value, side * vr_flat_prl_tilt_roll.value} * tilt;
        s.pos[g] += (fwd * vr_flat_prl_gun_fwd.value + glm::vec3{0.f, 0.f, vr_flat_prl_gun_up.value} -
                        right * (side * vr_flat_prl_gun_in.value)) * tilt;
    }
    Trip& t = ps.trip;
    if(t.phase == Phase::None)
    {
        return;
    }
    const int f = t.free;
    const int g = t.gun;
    const glm::vec3 restPos = s.pos[f];
    const glm::vec3 restRot = s.rot[f];
    if(t.snap)
    {
        t.snap = false;
        t.fromOfs = t.applied ? t.lastOfs : glm::vec3{0.f};
        t.fromRot = t.applied ? t.lastRot : restRot - glm::vec3{0.f, s.bodyYaw, 0.f};
    }
    const float side = sideOf(f);
    glm::vec3 fwd, right;
    bodyAxes(s.bodyYaw, fwd, right);
    const glm::mat3 gunAxes = held::axesFromAngles(&s.rot[g][0], true);
    const glm::vec3 port = s.pos[g] + gunAxes * ps.portIn;
    const glm::vec3 face = glm::normalize(gunAxes * ps.faceIn);
    // The hand's turn at the port: the gun's, turned by vr_flat_prl_port_* (mirrored).
    const glm::vec3 portRot =
        s.rot[g] + glm::vec3{vr_flat_prl_port_pitch.value, side * vr_flat_prl_port_yaw.value, side * vr_flat_prl_port_roll.value};
    // Where the hand goes for the round to be at `point`, turned `rot`.
    const auto handFor = [&](const glm::vec3& point, const glm::vec3& rot)
    {
        return t.roundKnown ? point - held::axesFromAngles(&rot[0], true) * t.roundIn : point;
    };
    glm::vec3 toPos = restPos;
    glm::vec3 toRot = restRot;
    float arc = 0.f;
    switch(t.phase)
    {
        case Phase::Reach:
        case Phase::Grab:
            toPos = body::ammoPouchPosition(s) +
                    fromBody({vr_flat_prl_pouch_fwd.value, side * vr_flat_prl_pouch_right.value, vr_flat_prl_pouch_up.value}, s.bodyYaw);
            toRot = glm::vec3{vr_flat_prl_pouch_pitch.value, s.bodyYaw + side * vr_flat_prl_pouch_yaw.value, side * vr_flat_prl_pouch_roll.value};
            arc = t.phase == Phase::Reach && t.applied ? vr_flat_prl_arc.value : 0.f;
            break;
        case Phase::Carry:
            toRot = portRot;
            toPos = handFor(port + face * za::max(vr_flat_prl_approach.value, vr_flat_prl_test_miss.value), toRot);
            arc = vr_flat_prl_arc.value;
            break;
        case Phase::Insert:
            toRot = portRot;
            toPos = handFor(port + face * za::max(0.f, vr_flat_prl_test_miss.value), toRot);
            break;
        default: break; // (Return: its pose)
    }
    const float e = ease(t.t);
    const glm::vec3 from = restPos + fromBody(t.fromOfs, s.bodyYaw);
    const glm::vec3 fromRot = t.fromRot + glm::vec3{0.f, s.bodyYaw, 0.f};
    glm::vec3 pos = from + (toPos - from) * e;
    if(arc > 0.f)
    {
        // Bowed up and out (away from the body's middle) on the way, most at its middle.
        pos += (glm::vec3{0.f, 0.f, 1.f} + right * (side * 0.5f)) * (arc * za::sin(pi * za::clamp(t.t, 0.f, 1.f)));
    }
    s.pos[f] = pos;
    s.rot[f] = flatplay::blendHandAngles(fromRot, toRot, e);
    t.lastOfs = toBody(s.pos[f] - restPos, s.bodyYaw);
    t.lastRot = s.rot[f] - glm::vec3{0.f, s.bodyYaw, 0.f};
    t.applied = true;
    // The report: the carried round's distance from the port as placed now, and how much the opening faces the free hand.
    const glm::vec3 toFree = s.pos[f] - port;
    ps.faceToFree = glm::length(toFree) > 1e-3f ? glm::dot(face, glm::normalize(toFree)) : 0.f;
    ps.portGap = t.roundKnown && carriedEnt(f) > 0 ? glm::distance(s.pos[f] + held::axesFromAngles(&s.rot[f][0], true) * t.roundIn, port)
                                                   : -1.f;
}

bool busy(int h)
{
    return ps.trip.phase != Phase::None && h == ps.trip.free;
}

} // namespace qvr::flatprocreload
