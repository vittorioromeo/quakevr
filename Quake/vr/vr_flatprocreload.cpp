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
// Cancels: a shot of the gun's hand (the server cancels it too: its fire button), a punch or shove (impulse 106; the shove's
// and bash's impulses cancel it on the server), the failsafe: a round at the port not seated within vr_flat_prl_failsafe
// seconds (a wall, the hand blocked), or nothing taken at the pouch within it (impulse 106, the reason printed and kept for
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
#include "vr_weapons.hpp"

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

// The drawn free hand carried on the drawn gun (attachDrawn): the gun hand's game pose and its drawn pose as last noted
// (noteDrawn), and how much the free hand goes with it (0 at the pouch, 1 bringing the round to the port and in).
struct DrawnGun
{
    glm::vec3 gamePos{0.f};
    glm::vec3 gameRot{0.f};
    glm::vec3 drawnPos{0.f};
    glm::vec3 drawnRot{0.f};
    int hand{-1};
    int frame{-1000};
    float share{0.f};
};

DrawnGun dg;

// The key-pose lock (vr_flat_prl_lock; his words 2026-11-28: "please add a way to lock the animation for debug purposes in
// those keyframes so that I can tweak the poses precisely"): the gun and the free hand held at one key pose, made from the
// tunables each frame as a trip's are, so a change shows at once. The server knows nothing of it (no take, nothing loaded).
enum Key : int
{
    KeyOff,
    KeyRest,
    KeyTilted,
    KeyPouch,
    KeyGrip,
    KeyMouth,
    KeyInserted,
    KeyReturn,
    KeyScrub,
    KeyCount
};
constexpr const char* keyNames[KeyCount]{
    "off", "rest", "gun tilted", "hand at pouch", "gripping the pair", "at the port's mouth", "inserted", "returning", "scrub"};

// The free hand's places on a trip: its pose, at the pouch, the round at the port's mouth, the round in the port.
enum Spot : int
{
    SpotRest,
    SpotPouch,
    SpotMouth,
    SpotPort,
    SpotCount
};

struct Lock
{
    bool on{false};
    int key{KeyOff};
    int shown{KeyOff};    // the key last announced
    int gun{HAND_MAIN};
    int free{HAND_OFF};
    Spot from{SpotRest};  // the hand between two places, `t` of the way (before its ease)
    Spot to{SpotRest};
    float t{1.f};
    float tilt{0.f};      // the gun's tilt share (before its ease)
    bool arc{false};
    bool pair{false};     // a taped pair drawn in the free hand (vr_view.cpp prlPair)
    glm::vec3 lastOfs{0.f}; // the free hand as the lock placed it last (as Trip's: off its pose in the body's frame, its turn)
    glm::vec3 lastRot{0.f};
    // The pair as it sat in the free hand at the last real take (measureRound); before one, as measured on the shotgun
    // (prlfix, 2026-11-28: forward, left, up in the hand's frame; its model's axes in the hand's).
    glm::vec3 roundIn{-0.30f, -0.18f, -1.86f};
    glm::mat3 roundAxes{1.f}; // (measured 1, 0, -0.03 / 0, 1, 0 / 0.03, 0, 1: as the hand)
    float roundScale{1.f};
};

Lock lk;

constexpr int procBitBase = 16; // STAT_QVR_FLATRELOAD: 16 the off hand's gun, 32 the main's (QC VR_FlatReload_ProcBit)
constexpr int procOnBit = 64;   // the procedural reload on: the pouch shown (QC VR_FLATRL_PROC_ON)
constexpr float pi = 3.14159265f;
constexpr float attachTime = 0.12f; // (s: the drawn free hand onto the drawn gun and off it, attachDrawn)

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

[[nodiscard]] const char* spotName(Spot p)
{
    switch(p)
    {
        case SpotPouch: return "pouch";
        case SpotMouth: return "mouth";
        case SpotPort: return "port";
        default: return "rest";
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
        Cbuf_InsertText("impulse 106\n");
    }
    if(debugOn())
    {
        Con_Printf("flatprocreload: cancelled (%s)%s\n", why, tellServer ? ": impulse 106" : "");
    }
    if(t.phase != Phase::None && t.phase != Phase::Return)
    {
        setPhase(Phase::Return, why);
    }
}

// The port of the gun in `g` measured in its hand's frame (hands::State::loadPort, placed by the view with the muzzle), and
// its opening's way; kept from the last good frame. In the game's hand (the pose the view placed it from:
// State::placedFrom, its turn), not the drawn one: the server tests the round against the port in the game's frame (both
// sent with the move), and running the drawn gun hand sways 1-1.5 units and 2-3 degrees off the game's (prlmove,
// 2026-11-28: measured from the drawn hand, the free hand aimed the pair as far beside the port the server tests).
void measurePort(int g)
{
    const hands::State& cur = hands::current();
    if(!cur.loadPortValid[g])
    {
        return;
    }
    const glm::vec3 pos = cur.placedFrom[g];
    const glm::vec3 rot = cur.rot[g];
    // vr_flat_proc_reload_debug 3, carrying a round to the port: the drawn gun hand off the game's, and the round's gap from
    // the port as drawn (the free hand drawn, the port drawn) and in the game (what the server tests).
    const Trip& t = ps.trip;
    glm::vec3 dp, dr, fp, fr;
    if(vr_flat_proc_reload_debug.value >= 3.f && (t.phase == Phase::Carry || t.phase == Phase::Insert) && t.roundKnown &&
        view::drawnHandPose(g, dp, dr) && view::drawnHandPose(1 - g, fp, fr))
    {
        const glm::vec3 round = fp + held::axesFromAngles(&fr[0], true) * t.roundIn;
        Con_Printf("fprl_gap: %s %.2f gun drawn off the game's %.2f; round from the port drawn %.2f, game %.2f\n", phaseName(t.phase),
            t.t, glm::distance(dp, pos), glm::distance(round, cur.loadPort[g]), ps.portGap);
    }
    const glm::mat3 toHand = glm::transpose(held::axesFromAngles(&rot[0], true));
    ps.portIn = toHand * (cur.loadPort[g] - pos);
    if(glm::length(cur.loadPortFace[g]) > 1e-3f)
    {
        ps.faceIn = glm::normalize(toHand * cur.loadPortFace[g]);
    }
    ps.portKnown = glm::length(ps.portIn) < 48.f;
}

// The carried round's place in the free hand's frame: where it sits in the hand as drawn (vr_held.cpp placeInHand: the
// server's own place for it in the hand, its .carry_offset), not where the entity is drawn: that one is interpolated
// between the server's frames, a frame's walk behind the hand on the move (running, 3.3 units off: the hand then put the
// pair beside the port, and the failsafe cancelled every trip; prlmove, 2026-11-28).
void measureRound(Trip& t)
{
    const int ent = carriedEnt(t.free);
    glm::vec3 in, other;
    glm::mat3 axes;
    bool both = false;
    if(ent <= 0 || ent >= cl.num_entities || held::placeInHand(t.free, in, axes, both, other) != ent)
    {
        return;
    }
    const entity_t& e = cl_entities[ent];
    if(glm::length(in) < 16.f)
    {
        t.roundIn = in;
        t.roundKnown = true;
        // (Kept for the lock's pair: vr_flat_prl_lock.)
        lk.roundIn = in;
        lk.roundAxes = axes;
        lk.roundScale = ENTSCALE_DECODE(e.scale);
        if(debugOn())
        {
            const glm::mat3& a = lk.roundAxes;
            Con_Printf("flatprocreload: the pair in the hand at %.2f %.2f %.2f, axes %.2f %.2f %.2f / %.2f %.2f %.2f / %.2f %.2f %.2f, "
                       "size %.2f\n",
                in.x, in.y, in.z, a[0].x, a[0].y, a[0].z, a[1].x, a[1].y, a[1].z, a[2].x, a[2].y, a[2].z, lk.roundScale);
        }
    }
}

// vr_flat_proc_reload_debug 2: the gun as drawn last frame, each frame of the reload (its hand and its model's origin from
// the camera, in the camera's frame: forward, right, up; its hand's turn): the prlfix jolt's measurement, read by
// flatprocreload_check.py's continuity check.
void traceGun()
{
    const int g = ps.tiltHand;
    glm::vec3 hp, hr, wo;
    if(!view::drawnHandPose(g, hp, hr) || !view::drawnWeaponOrigin(g, wo))
    {
        return;
    }
    const hands::State& cur = hands::current();
    glm::vec3 f, r, u;
    hands::angleVectors(cur.headAngles, f, r, u);
    const glm::vec3 a = hp - cur.head;
    const glm::vec3 b = wo - cur.head;
    // (Short: a console line wraps past ~120 characters. Fields: time, phase, its share, tilt; the hand; its turn; the gun.)
    Con_Printf("fprl_trace: %.4f %s %.3f %.3f h %.2f %.2f %.2f r %.1f %.1f %.1f g %.2f %.2f %.2f\n", vr_gametime,
        lk.on ? "lock" : phaseName(ps.trip.phase), lk.on ? lk.t : ps.trip.t, ps.tilt, glm::dot(a, f), glm::dot(a, r), glm::dot(a, u), hr.x,
        hr.y - cur.headAngles.y, hr.z, glm::dot(b, f), glm::dot(b, r), glm::dot(b, u));
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
    // The lock (prlfix): its key, and the free hand as drawn last from the gun's hand (in the gun hand's frame) and its turn.
    glm::vec3 fp{0.f}, fr{0.f}, gp{0.f}, gr{0.f};
    const bool drawn = lk.on && view::drawnHandPose(lk.free, fp, fr) && view::drawnHandPose(lk.gun, gp, gr);
    const glm::vec3 rel = drawn ? glm::transpose(held::axesFromAngles(&gr[0], true)) * (fp - gp) : glm::vec3{0.f};
    Con_Printf("flatprocreload: lock %s key %d (%s) %s %.2f %s; free hand %s from the gun hand %.2f %.2f %.2f turned %.1f %.1f %.1f; "
               "tilt %.2f pair %d busy %d round gap %.2f\n",
        lk.on ? "on" : "off", lk.key, keyNames[lk.key], spotName(lk.from), lk.t, spotName(lk.to), handName(lk.free), rel.x, rel.y, rel.z,
        fr.x, fr.y - gr.y, fr.z, ps.tilt, lk.pair ? 1 : 0, busy(lk.free) ? 1 : 0, ps.portGap);
}

[[nodiscard]] int lockKey()
{
    return za::clamp(static_cast<int>(vr_flat_prl_lock.value + 0.5f), static_cast<int>(KeyOff), static_cast<int>(KeyScrub));
}

// vr_flat_prl_lock_next / _prev: the next or the previous key pose (rest .. returning, round; from off or scrub, the first or
// the last).
void lockStep(int by)
{
    int k = lockKey();
    if(k == KeyOff || k == KeyScrub)
    {
        k = by > 0 ? KeyOff : KeyScrub;
    }
    k += by;
    k = k > KeyReturn ? KeyRest : (k < KeyRest ? KeyReturn : k);
    Cvar_SetValueQuick(&vr_flat_prl_lock, static_cast<float>(k));
}

void lockNext_f()
{
    lockStep(1);
}

void lockPrev_f()
{
    lockStep(-1);
}

[[nodiscard]] bool gunIn(int h)
{
    return weapons::heldModel(h) != nullptr && weapons::heldSlot(h) != weapons::fistSlot();
}

// The hand of the gun the lock holds: a gun in one hand, the other empty (the main hand's first); -1 none.
[[nodiscard]] int lockGun()
{
    for(int g : {HAND_MAIN, HAND_OFF})
    {
        if(gunIn(g) && !gunIn(1 - g) && !flatplay::holdsThing(1 - g))
        {
            return g;
        }
    }
    return -1;
}

// The lock's scrub: the share `share` of one trip from rest (the reach with the tilt in, the grab, the carry, the insert,
// the way back with the tilt out) as the trip's own times have it.
void lockScrub(float share)
{
    const float reach = phaseTime(Phase::Reach), grab = phaseTime(Phase::Grab), carry = phaseTime(Phase::Carry);
    const float insert = phaseTime(Phase::Insert), back = phaseTime(Phase::Return);
    const float tiltIn = timeOf(vr_flat_prl_tilt_in_time, 0.02f), tiltOut = timeOf(vr_flat_prl_tilt_out_time, 0.02f);
    const float out = reach + grab + carry + insert;
    const float time = share * (out + za::max(back, tiltOut));
    const auto move = [](Spot from, Spot to, float t, bool arc, bool pair) {
        lk.from = from;
        lk.to = to;
        lk.t = za::clamp(t, 0.f, 1.f);
        lk.arc = arc;
        lk.pair = pair;
    };
    lk.tilt = za::min(1.f, time / tiltIn);
    if(time < reach)
    {
        move(SpotRest, SpotPouch, time / reach, false, false);
    }
    else if(time < reach + grab)
    {
        move(SpotPouch, SpotPouch, 1.f, false, true); // (the take as the hand gets there)
    }
    else if(time < reach + grab + carry)
    {
        move(SpotPouch, SpotMouth, (time - reach - grab) / carry, true, true);
    }
    else if(time < out)
    {
        move(SpotMouth, SpotPort, (time - reach - grab - carry) / insert, false, true);
    }
    else
    {
        move(SpotPort, SpotRest, (time - out) / back, false, false);
        lk.tilt = za::max(0.f, za::min(1.f, out / tiltIn) - (time - out) / tiltOut);
    }
}

// The free hand `f`'s places this frame (its pose as `s` has it now, before the trip moves it; the gun in `g` tilted), and
// the port and its opening's way. `roundIn`: the carried round's place in the hand (nullptr unknown: the hand itself
// goes to the port).
struct Spots
{
    glm::vec3 pos[SpotCount];
    glm::vec3 rot[SpotCount];
    glm::vec3 port;
    glm::vec3 face;
};

[[nodiscard]] Spots spotsOf(const hands::State& s, int f, int g, const glm::vec3* roundIn)
{
    Spots sp;
    const float side = sideOf(f);
    const glm::mat3 gunAxes = held::axesFromAngles(&s.rot[g][0], true);
    sp.port = s.pos[g] + gunAxes * ps.portIn;
    sp.face = glm::normalize(gunAxes * ps.faceIn);
    // Where the hand goes for the round to be at `point`, turned `rot`.
    const auto handFor = [&](const glm::vec3& point, const glm::vec3& rot) {
        return roundIn ? point - held::axesFromAngles(&rot[0], true) * *roundIn : point;
    };
    sp.pos[SpotRest] = s.pos[f];
    sp.rot[SpotRest] = s.rot[f];
    sp.pos[SpotPouch] = body::ammoPouchPosition(s) +
                        fromBody({vr_flat_prl_pouch_fwd.value, side * vr_flat_prl_pouch_right.value, vr_flat_prl_pouch_up.value}, s.bodyYaw);
    sp.rot[SpotPouch] =
        glm::vec3{vr_flat_prl_pouch_pitch.value, s.bodyYaw + side * vr_flat_prl_pouch_yaw.value, side * vr_flat_prl_pouch_roll.value};
    // The hand's turn at the port: the gun's, turned by vr_flat_prl_port_* (mirrored).
    const glm::vec3 portRot =
        s.rot[g] + glm::vec3{vr_flat_prl_port_pitch.value, side * vr_flat_prl_port_yaw.value, side * vr_flat_prl_port_roll.value};
    sp.rot[SpotMouth] = sp.rot[SpotPort] = portRot;
    sp.pos[SpotMouth] = handFor(sp.port + sp.face * za::max(vr_flat_prl_approach.value, vr_flat_prl_test_miss.value), portRot);
    sp.pos[SpotPort] = handFor(sp.port + sp.face * za::max(0.f, vr_flat_prl_test_miss.value), portRot);
    return sp;
}

// The lock's pose (lockFrame set its places, share and tilt; the tilt is applied already): made from this frame's places,
// so a tunable's change shows at once.
void applyLock(hands::State& s)
{
    const int f = lk.free;
    const Spots sp = spotsOf(s, f, lk.gun, &lk.roundIn);
    const float side = sideOf(f);
    glm::vec3 fwd, right;
    bodyAxes(s.bodyYaw, fwd, right);
    const float e = ease(lk.t);
    const glm::vec3 restPos = s.pos[f];
    glm::vec3 pos = sp.pos[lk.from] + (sp.pos[lk.to] - sp.pos[lk.from]) * e;
    if(lk.arc)
    {
        pos += (glm::vec3{0.f, 0.f, 1.f} + right * (side * 0.5f)) * (vr_flat_prl_arc.value * za::sin(pi * lk.t));
    }
    s.pos[f] = pos;
    s.rot[f] = flatplay::blendHandAngles(sp.rot[lk.from], sp.rot[lk.to], e);
    lk.lastOfs = toBody(s.pos[f] - restPos, s.bodyYaw);
    lk.lastRot = s.rot[f] - glm::vec3{0.f, s.bodyYaw, 0.f};
    ps.portGap = lk.pair ? glm::distance(s.pos[f] + held::axesFromAngles(&s.rot[f][0], true) * lk.roundIn, sp.port) : -1.f;
}

// This frame's lock at `key`: the hands, the place and the tilt. False: no gun held one-handed to lock.
bool lockFrame(int key)
{
    const int g = lockGun();
    if(g < 0)
    {
        if(lk.on || lk.shown != KeyOff)
        {
            Con_Printf("flatprocreload: the lock needs a gun in one hand and the other empty\n");
        }
        lk.on = false;
        lk.shown = KeyOff;
        return false;
    }
    if(ps.trip.phase != Phase::None)
    {
        ps.trip = Trip{}; // (a trip under way dropped: the server's give-up ends its side, the round refunded)
    }
    lk.on = true;
    lk.key = key;
    lk.gun = g;
    lk.free = 1 - g;
    lk.from = lk.to = SpotRest;
    lk.t = 1.f;
    lk.tilt = 1.f;
    lk.arc = lk.pair = false;
    switch(key)
    {
        case KeyRest: lk.tilt = 0.f; break;
        case KeyPouch: lk.from = lk.to = SpotPouch; break;
        case KeyGrip:
            lk.from = lk.to = SpotPouch;
            lk.pair = true;
            break;
        case KeyMouth:
            lk.from = lk.to = SpotMouth;
            lk.pair = true;
            break;
        case KeyInserted:
            lk.from = lk.to = SpotPort;
            lk.pair = true;
            break;
        case KeyReturn:
            // (Half way back, empty: the tilt as far out as it is then, as the way back and the tilt out start together.)
            lk.from = SpotPort;
            lk.t = 0.5f;
            lk.tilt = za::max(0.f, 1.f - 0.5f * phaseTime(Phase::Return) / timeOf(vr_flat_prl_tilt_out_time, 0.02f));
            break;
        case KeyScrub: lockScrub(za::clamp(vr_flat_prl_scrub.value, 0.f, 1.f)); break;
        default: break; // (KeyTilted: the hand at rest, the gun tilted)
    }
    ps.tilt = lk.tilt;
    ps.tiltHand = g;
    if(lk.shown != key)
    {
        Con_Printf("flatprocreload: locked at key %d (%s; the %s hand, the gun in the %s)\n", key, keyNames[key], handName(lk.free),
            handName(g));
        lk.shown = key;
    }
    measurePort(g);
    return true;
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_flatprocreload_report", report_f);
    Cmd_AddCommand("vr_flat_prl_lock_next", lockNext_f);
    Cmd_AddCommand("vr_flat_prl_lock_prev", lockPrev_f);
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
        lk.on = false;
        lk.shown = KeyOff;
        dg.share = 0.f;
        return;
    }
    const float step = static_cast<float>(dt);
    // The drawn free hand on the drawn gun: eased in as the round sets off for the port, out as the hand leaves it.
    {
        const bool toPort = lk.on ? (lk.pair && (lk.to == SpotMouth || lk.to == SpotPort))
                                  : (t.phase == Phase::Carry || t.phase == Phase::Insert);
        const float rate = step / attachTime;
        dg.share = toPort ? za::min(1.f, dg.share + rate) : za::max(0.f, dg.share - rate);
    }
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
    // The key-pose lock (prlfix): held there, the trip's steps not run (a reload started under it isn't driven).
    if(const int key = lockKey(); key != KeyOff)
    {
        if(vr_flat_proc_reload_debug.value >= 2.f && lk.on && dt > 0.0)
        {
            traceGun();
        }
        if(lockFrame(key))
        {
            return;
        }
    }
    else if(lk.on)
    {
        // Let go: the hand back to its pose from where the lock drew it, the gun's tilt out (a Return, as a cancel's).
        lk.on = false;
        lk.shown = KeyOff;
        t = Trip{};
        t.gun = lk.gun;
        t.free = lk.free;
        t.applied = true;
        t.lastOfs = lk.lastOfs;
        t.lastRot = lk.lastRot;
        setPhase(Phase::Return, "the lock let go");
        upHand = -1;
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
    if(vr_flat_proc_reload_debug.value >= 2.f && (t.phase != Phase::None || ps.tilt > 0.f) && dt > 0.0)
    {
        traceGun(); // (as drawn last frame: before this frame's step)
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
            if(!t.roundKnown && carried > 0)
            {
                measureRound(t); // (not placed in the hand yet at the grab's end: the hand itself aims at the port till then)
            }
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
    if(lk.on)
    {
        applyLock(s);
        return;
    }
    Trip& t = ps.trip;
    if(t.phase == Phase::None)
    {
        return;
    }
    const int f = t.free;
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
    const Spots sp = spotsOf(s, f, t.gun, t.roundKnown ? &t.roundIn : nullptr);
    const glm::vec3& port = sp.port;
    const glm::vec3& face = sp.face;
    Spot to = SpotRest; // (Return: its pose)
    float arc = 0.f;
    switch(t.phase)
    {
        case Phase::Reach:
        case Phase::Grab:
            to = SpotPouch;
            arc = t.phase == Phase::Reach && t.applied ? vr_flat_prl_arc.value : 0.f;
            break;
        case Phase::Carry:
            to = SpotMouth;
            arc = vr_flat_prl_arc.value;
            break;
        case Phase::Insert: to = SpotPort; break;
        default: break;
    }
    const glm::vec3 toPos = sp.pos[to];
    const glm::vec3 toRot = sp.rot[to];
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

void noteDrawn(const hands::State& s, int hand, const glm::vec3& pos, const glm::vec3& rot)
{
    const int g = lk.on ? lk.gun : ps.trip.gun;
    if(hand != g || (!lk.on && ps.trip.phase == Phase::None))
    {
        return;
    }
    dg = DrawnGun{s.pos[hand], s.rot[hand], pos, rot, hand, host_framecount, dg.share};
}

void attachDrawn(const hands::State& s, int hand, glm::vec3& pos, glm::vec3& rot)
{
    const int f = lk.on ? lk.free : ps.trip.free;
    if(!busy(hand) || hand != f || dg.share <= 0.f || dg.hand != 1 - hand || host_framecount - dg.frame > 1)
    {
        return;
    }
    // The game's free hand from the game's gun hand, put on the gun hand as drawn (its sway, its own drawn turn): the round
    // drawn where the server has it from the port it tests, into the port as drawn. (A gun in the off hand is drawn after
    // the free main hand: its drawn offset is last frame's, put on its game hand of this frame.)
    const int g = 1 - hand;
    const glm::mat3 turn = held::axesFromAngles(&dg.drawnRot[0], true) * glm::transpose(held::axesFromAngles(&dg.gameRot[0], true));
    const glm::vec3 onGun = s.pos[g] + (dg.drawnPos - dg.gamePos) + turn * (s.pos[hand] - s.pos[g]);
    glm::vec3 onRot;
    held::anglesFromAxes(turn * held::axesFromAngles(&s.rot[hand][0], true), &onRot[0], true);
    const float k = geom::smooth01(dg.share);
    pos += (onGun - pos) * k;
    rot = flatplay::blendHandAngles(rot, onRot, k);
}

bool portNow(const hands::State& s, int g, glm::vec3& port)
{
    const bool trip = ps.trip.phase != Phase::None && ps.trip.phase != Phase::Return && g == ps.trip.gun;
    if(!trip || !ps.portKnown || !s.loadPortValid[g])
    {
        return false;
    }
    port = s.pos[g] + held::axesFromAngles(&s.rot[g][0], true) * ps.portIn;
    return true;
}

bool busy(int h)
{
    return (ps.trip.phase != Phase::None && h == ps.trip.free) || (lk.on && h == lk.free);
}

bool lockPair(int& hand, float& scale)
{
    hand = lk.free;
    scale = lk.roundScale;
    return lk.on && lk.pair;
}

glm::vec3 lockPairPlace(const glm::vec3& handPos, const glm::vec3& handRot, glm::mat3& axes)
{
    const glm::mat3 hand = held::axesFromAngles(&handRot[0], true);
    axes = hand * lk.roundAxes;
    return handPos + hand * lk.roundIn;
}

} // namespace qvr::flatprocreload
