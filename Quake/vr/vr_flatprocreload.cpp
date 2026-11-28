// vr_flatprocreload.cpp -- see vr_flatprocreload.hpp. Flatscreen play's procedural reload as drawn (flatprocreload; his
// words 2026-11-28: "When pressing the reload button, the off-hand will go back and forth from the pouch to the shotgun's
// shell entry at the bottom, until the shotgun is full or the player cancels the reloading by firing or performing a
// quick melee. The hand holding the shotgun should slightly tilt it ... We should be able to rely on the same code paths as
// VR"; docs/vr-port/FLATPLAY.md, "Reloading"). Since reloadsys (2026-11-28): played from the gun's keyframes
// (vr_reloadsys.hpp, quakevr/data/reloads.txt, the Reload Studio), the shotgun's and the super shotgun's.
//
// The game decides (QC vr_flatreload.qc, "The procedural reload"): R or the empty clip on a gun with a reload here, held in
// one hand, the other empty, with vr_flat_proc_reload on, raises the gun hand's bit in STAT_QVR_FLATRELOAD (16 the off hand's
// gun, 32 the main's) instead of the timed trip to the hip, and drops it when the gun is full, the reserve out, or the reload
// is cancelled (a shot, a quick melee, the weapon changed, its own failsafe). This file plays the gun's keys while it is up:
// each key moves the free hand (to its pose, the ammo pouch, the item at its destination's mouth or in it, or held) and the
// gun hand's pose (the tilt that presents the destination) from where each was drawn last, over its times and ease; its
// action at its start (Take: impulse 99 once the hand is at the pouch's hotspot, the key waiting for the item in the hand;
// Insert: the key waiting at its end for the item to go in, VR's own port test on the server; Open / Close: impulses 107 /
// 108, the super shotgun broken open / snapped shut by the server); keys whose condition fails (the gun closed, loaded)
// skipped. The loop's keys repeat while the server's reload runs; when it is over (or cancelled) the `finish` keys play.
// The item going in (gone from the hand, the clip up) goes on after the insert's key at once.
//
// The free hand's place at the destination is solved backwards each frame (spotsOf): the item's pose there (the port
// measured on the gun's game pose; the item's turn the reload's, on the gun's) -> the item's place in the hand
// (held::placeInHand, the server's own) -> the hand; the key's offset and turn on top in the gun's frame.
//
// Cancels: a shot of the gun's hand (the server cancels it too: its fire button), a punch or shove (impulse 106; the shove's
// and bash's impulses cancel it on the server), the failsafe: an item at its destination not seated within
// vr_flat_prl_failsafe seconds (a wall, the hand blocked), or nothing taken at the pouch within it (impulse 106, the reason
// printed and kept for the report). The server puts a round still in the hand back in the reserve.
//
// On the move (prlmove): the hand aims the round at the port in the game's frame (the port measured in the game's gun hand,
// the round's place in the hand the server's), the move sends that very port (portNow), and the server's carry keeps the
// round in the hand through the body and the gun (QC VR_Carry_Follow). The drawn free hand rides the drawn gun while it
// takes the item to its destination (attachDrawn), and its fingers take a headset's grip on the item (gripShare).
//
// The key-pose lock (vr_flat_prl_lock N: the gun's key N's end pose; -1: vr_flat_prl_scrub's share of one pass through its
// keys, the loop's once): the poses made each frame from the data, so an edit (the studio's) shows at once; the server is
// told nothing. vr_flat_proc_reload_debug 1 prints each step, 2 the gun as drawn each frame (fprl_trace:), 3 the gaps on the
// way to the port (fprl_gap:); vr_flatprocreload_report the state and counts; vr_flat_prl_test_miss N stops the item N
// units short of its destination (the tests' blocked port).

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
#include "vr_reloadsys.hpp"
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

using reloadsys::Action;
using reloadsys::Cond;
using reloadsys::Key;
using reloadsys::Reload;
using reloadsys::Target;

// A pose of the hands off their own: the free hand off its pose (the body's frame: forward, right, up) and its turn (its yaw
// off the body's); the gun hand's turn and move off its own (as applied: mirrored already).
struct Offsets
{
    glm::vec3 handOfs{0.f};
    glm::vec3 handRot{0.f};
    bool hand{false}; // handOfs / handRot hold a pose (else: the hand at its own)
    glm::vec3 gunRot{0.f};
    glm::vec3 gunOfs{0.f};
};

// The trip (no function-local statics: a named file-scope struct, CODE_STYLE.md).
struct Trip
{
    bool on{false};
    int weapon{0};               // the gun's weapon ID (its reload: reloadsys::forWeapon)
    int key{-1};                 // the key playing
    int gun{HAND_MAIN};          // the gun's hand
    int free{HAND_OFF};          // the hand that reloads it
    double keyAt{-1e9};          // the key's start (vr_gametime)
    float t{0.f};                // the free hand's share of the key, before its ease
    float gt{0.f};               // the gun's
    bool snap{true};             // take the key's start from the hands as applied last (apply)
    Offsets from;                // the key's start
    Offsets last;                // the hands as applied last
    bool takeSent{false};        // impulse 99 went for this key
    double takeAt{-1e9};
    bool actionSent{false};      // the key's open / close impulse went
    bool hadItem{false};         // an item was in the hand since the take
    bool atPort{false};          // the insert's push done: waiting for the item to go in
    double atPortAt{-1e9};
    int clipAtCarry{0};          // the gun's clip as the item was taken (a round seated: it rose)
    bool ending{false};          // playing the finish keys
};

struct ProcState
{
    Trip trip;
    bool bitUp[2]{false, false}; // the server's procedural bit for each hand's gun, last frame
    int gunHand{HAND_MAIN};      // the hand whose gun is posed (the trip's, the lock's)
    Offsets gunNow;              // the gun as posed this frame (apply; the report)
    double lastTime{-1e9};
    // The port as last measured (the gun hand's frame: forward, left, up), and its opening's way.
    glm::vec3 portIn{0.f};
    glm::vec3 faceIn{0.f, 0.f, -1.f};
    bool portKnown{false};
    float portGap{-1.f};         // the carried item's distance from its destination as placed (the report; -1 none)
    float insertGap{-1.f};       // and as the last insert's key reached its end (the studio's success readout)
    float faceToFree{0.f};       // how much the port's opening faces the free hand (cosine; the report)
    int trips{0};                // procedural reloads started (the bit seen coming up)
    int takes{0};                // impulse 99 sent
    int seated{0};               // items seen going in (gone from the hand, the clip up)
    int dropped{0};              // carried items gone without the clip rising
    int cancels{0};
    int failsafes{0};
    const char* lastCancel{"-"};
    float grip{0.f};             // the free hand's fingers on the item (gripShare), before its ease
};

ProcState ps;

// The item as it sits in the free hand (forward, left, up in the hand's frame; its model's axes in the hand's): measured at
// each real take (measureRound, held::placeInHand: the server's own place for it); before one, as measured on the shotgun
// (prlfix, 2026-11-28). The trip's backwards solve and the lock's drawn pair use it.
struct ItemInHand
{
    glm::vec3 at{-0.30f, -0.18f, -1.86f};
    glm::mat3 axes{1.f}; // (measured 1, 0, -0.03 / 0, 1, 0 / 0.03, 0, 1: as the hand)
    float scale{1.f};
};
ItemInHand item;

// The drawn free hand carried on the drawn gun (attachDrawn): the gun hand's game pose and its drawn pose as last noted
// (noteDrawn), and how much the free hand goes with it (0 at the pouch, 1 bringing the item to its destination and in).
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
// those keyframes so that I can tweak the poses precisely"), generalised to any key of the gun's reload (reloadsys).
struct Lock
{
    bool on{false};
    int key{0};        // the key held (from 0), its end pose; or the key the scrub is in
    float t{1.f};      // the scrub's share of that key (1: its end)
    int shown{-100};   // the lock value last announced
    int gun{HAND_MAIN};
    int free{HAND_OFF};
    int weapon{0};
    bool pair{false};  // the item drawn in the free hand (vr_view.cpp prlPair)
    Offsets last;      // the hands as the lock placed them last
};

Lock lk;

constexpr int procBitBase = 16; // STAT_QVR_FLATRELOAD: 16 the off hand's gun, 32 the main's (QC VR_FlatReload_ProcBit)
constexpr int procOnBit = 64;   // the procedural reload on: the pouch shown (QC VR_FLATRL_PROC_ON)
constexpr int ssgOpenFlag = 32; // STAT_QVR_WEAPONFLAGS: QC QVR_WPNFLAG_SSG_OPEN
constexpr float pi = 3.14159265f;
constexpr float attachTime = 0.12f; // (s: the drawn free hand onto the drawn gun and off it, attachDrawn)
constexpr float gripTime = 0.08f;   // (s: the free hand's fingers onto the item and off it, gripShare)

[[nodiscard]] float timeOf(const Tunable& c, float least)
{
    return za::max(least, c.value);
}

[[nodiscard]] const char* handName(int h)
{
    return h == HAND_MAIN ? "main" : "off";
}

[[nodiscard]] bool debugOn()
{
    return vr_flat_proc_reload_debug.value != 0.f;
}

[[nodiscard]] int carriedEnt(int h)
{
    return cl.stats[h == HAND_MAIN ? protocol::STAT_QVR_CARRYMAIN : protocol::STAT_QVR_CARRYOFF];
}

[[nodiscard]] int clipOf(int h)
{
    return cl.stats[h == HAND_MAIN ? protocol::STAT_QVR_WEAPONCLIP : protocol::STAT_QVR_WEAPONCLIP2];
}

[[nodiscard]] int weaponOf(int h)
{
    return cl.stats[h == HAND_MAIN ? protocol::STAT_QVR_WEAPON : protocol::STAT_QVR_WEAPON2];
}

[[nodiscard]] int flagsOf(int h)
{
    return cl.stats[h == HAND_MAIN ? protocol::STAT_QVR_WEAPONFLAGS : protocol::STAT_QVR_WEAPONFLAGS2];
}

[[nodiscard]] float sideOf(int h)
{
    return h == HAND_MAIN ? 1.f : -1.f; // (mirrored for the left hand)
}

// A key's condition on the gun in hand `g` now.
[[nodiscard]] bool condMet(Cond c, int g)
{
    const bool open = (flagsOf(g) & ssgOpenFlag) != 0;
    switch(c)
    {
        case Cond::Closed: return !open;
        case Cond::Opened: return open;
        case Cond::Loaded: return clipOf(g) > 0;
        default: return true;
    }
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

// A turn's (pitch, yaw, roll) mirrored by `side` (yaw and roll).
[[nodiscard]] glm::vec3 mirrorRot(const glm::vec3& r, float side)
{
    return {r.x, side * r.y, side * r.z};
}

// A key's gun pose as applied for the gun in hand `g`: its turn mirrored, its move in the body's frame (forward, right, up:
// "in" towards the body's middle).
void keyGun(const Key& k, int g, glm::vec3& rot, glm::vec3& ofs)
{
    const float side = sideOf(g);
    rot = mirrorRot(k.gunRot, side);
    ofs = glm::vec3{k.gunOfs.x, -side * k.gunOfs.y, k.gunOfs.z};
}

[[nodiscard]] const Reload* reloadOf(int weapon)
{
    return reloadsys::forWeapon(weapon);
}

[[nodiscard]] const char* keyName(const Reload* r, int key)
{
    return r && key >= 0 && key < r->keyCount ? r->keys[key].name : "none";
}

void startKey(int key, const char* why)
{
    Trip& t = ps.trip;
    const Reload* r = reloadOf(t.weapon);
    if(debugOn() && key != t.key)
    {
        Con_Printf("flatprocreload: %s -> %s (%s; the %s hand, the gun in the %s)\n", keyName(r, t.key), keyName(r, key), why,
            handName(t.free), handName(t.gun));
    }
    t.key = key;
    t.keyAt = vr_gametime;
    t.t = 0.f;
    t.gt = 0.f;
    t.snap = true;
    t.takeSent = false;
    t.actionSent = false;
    t.atPort = false;
}

// The trip over: the hands at their own (the gun's tail eased out by the last key).
void endTrip()
{
    Trip& t = ps.trip;
    if(debugOn())
    {
        Con_Printf("flatprocreload: the %s hand back in its pose\n", handName(t.free));
    }
    t.on = false;
    t.key = -1;
    t.last = Offsets{};
}

// The reload's end: its finish keys from where the hands are (none: the trip over at once).
void toEnding(const char* why)
{
    Trip& t = ps.trip;
    const Reload* r = reloadOf(t.weapon);
    t.ending = true;
    const int k = r ? reloadsys::firstKey(*r, true, [&](Cond c) { return condMet(c, t.gun); }) : -1;
    if(k < 0)
    {
        endTrip();
        return;
    }
    startKey(k, why);
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
    if(t.on && !t.ending)
    {
        toEnding(why);
    }
}

// The port of the gun in `g` measured in its hand's frame (hands::State::loadPort, placed by the view with the muzzle), and
// its opening's way; kept from the last good frame. In the game's hand (State::placedFrom, its turn), not the drawn one: the
// server tests the round against the port in the game's frame (prlmove, 2026-11-28).
void measurePort(int g)
{
    const hands::State& cur = hands::current();
    if(!cur.loadPortValid[g])
    {
        return;
    }
    const glm::vec3 pos = cur.placedFrom[g];
    const glm::vec3 rot = cur.rot[g];
    const Trip& t = ps.trip;
    glm::vec3 dp, dr, fp, fr;
    if(vr_flat_proc_reload_debug.value >= 3.f && t.on && t.hadItem && view::drawnHandPose(g, dp, dr) && view::drawnHandPose(1 - g, fp, fr))
    {
        const glm::vec3 round = fp + held::axesFromAngles(&fr[0], true) * item.at;
        Con_Printf("fprl_gap: %d %.2f gun drawn off the game's %.2f; round from the port drawn %.2f, game %.2f\n", t.key, t.t,
            glm::distance(dp, pos), glm::distance(round, cur.loadPort[g]), ps.portGap);
    }
    const glm::mat3 toHand = glm::transpose(held::axesFromAngles(&rot[0], true));
    ps.portIn = toHand * (cur.loadPort[g] - pos);
    if(glm::length(cur.loadPortFace[g]) > 1e-3f)
    {
        ps.faceIn = glm::normalize(toHand * cur.loadPortFace[g]);
    }
    ps.portKnown = glm::length(ps.portIn) < 48.f;
}

// The carried item's place in the free hand's frame: where it sits in the hand (vr_held.cpp placeInHand: the server's own
// place for it in the hand, its .carry_offset), not where the entity is drawn (interpolated, a frame's walk behind on the
// move; prlmove).
void measureRound(int free)
{
    const int ent = carriedEnt(free);
    glm::vec3 in, other;
    glm::mat3 axes;
    bool both = false;
    if(ent <= 0 || ent >= cl.num_entities || held::placeInHand(free, in, axes, both, other) != ent || glm::length(in) >= 16.f)
    {
        return;
    }
    item.at = in;
    item.axes = axes;
    item.scale = ENTSCALE_DECODE(cl_entities[ent].scale);
    if(debugOn())
    {
        const glm::mat3& a = item.axes;
        Con_Printf("flatprocreload: the pair in the hand at %.2f %.2f %.2f, axes %.2f %.2f %.2f / %.2f %.2f %.2f / %.2f %.2f %.2f, "
                   "size %.2f\n",
            in.x, in.y, in.z, a[0].x, a[0].y, a[0].z, a[1].x, a[1].y, a[1].z, a[2].x, a[2].y, a[2].z, item.scale);
    }
}

// vr_flat_proc_reload_debug 2: the gun as drawn last frame, each frame of the reload (its hand and its model's origin from
// the camera, in the camera's frame; its hand's turn): read by flatprocreload_check.py's continuity check. Fields: time, the
// key (its number from 1, or lock), the free hand's share, the gun's share (1: settled at the key's pose).
void traceGun()
{
    const int g = ps.gunHand;
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
    char what[16];
    q_snprintf(what, sizeof(what), lk.on ? "lock" : "k%d", ps.trip.key + 1);
    Con_Printf("fprl_trace: %.4f %s %.3f %.3f h %.2f %.2f %.2f r %.1f %.1f %.1f g %.2f %.2f %.2f\n", vr_gametime, what,
        lk.on ? lk.t : ps.trip.t, lk.on ? 1.f : ps.trip.gt, glm::dot(a, f), glm::dot(a, r), glm::dot(a, u), hr.x, hr.y - cur.headAngles.y, hr.z,
        glm::dot(b, f), glm::dot(b, r), glm::dot(b, u));
}

void report_f()
{
    const Trip& t = ps.trip;
    const Reload* r = reloadOf(t.on ? t.weapon : lk.weapon);
    const int bits = cl.stats[protocol::STAT_QVR_FLATRELOAD];
    Con_Printf("flatprocreload: key %d %s gun %s free %s share %.2f gun %.2f; server bits %d (on %d, off gun %d, main gun %d); carried %d; "
               "port %s %.1f %.1f %.1f, round %.2f from it, its opening to the free hand %.2f; hotspot %d\n",
        t.key + 1, keyName(r, t.key), handName(t.gun), handName(t.free), t.t, t.gt, bits, (bits & procOnBit) ? 1 : 0,
        (bits & procBitBase) ? 1 : 0, (bits & (procBitBase * 2)) ? 1 : 0, carriedEnt(t.free), ps.portKnown ? "known" : "unknown", ps.portIn.x,
        ps.portIn.y, ps.portIn.z, ps.portGap, ps.faceToFree, hands::current().hotspot[t.free]);
    Con_Printf("flatprocreload: trips %d takes %d seated %d dropped %d cancels %d failsafes %d last cancel %s; insert gap %.2f\n", ps.trips,
        ps.takes, ps.seated, ps.dropped, ps.cancels, ps.failsafes, ps.lastCancel, ps.insertGap);
    // The lock: its key, and the free hand as drawn last from the gun's hand (in the gun hand's frame) and its turn.
    glm::vec3 fp{0.f}, fr{0.f}, gp{0.f}, gr{0.f};
    const bool drawn = lk.on && view::drawnHandPose(lk.free, fp, fr) && view::drawnHandPose(lk.gun, gp, gr);
    const glm::vec3 rel = drawn ? glm::transpose(held::axesFromAngles(&gr[0], true)) * (fp - gp) : glm::vec3{0.f};
    const glm::vec3& gun = ps.gunNow.gunRot;
    Con_Printf("flatprocreload: lock %s key %d (%s) %.2f; free hand %s from the gun hand %.2f %.2f %.2f turned %.1f %.1f %.1f; "
               "gun %.1f %.1f %.1f pair %d busy %d round gap %.2f grip %.2f fingers %.2f\n",
        lk.on ? "on" : "off", lk.on ? lk.key + 1 : 0, lk.on ? keyName(r, lk.key) : "-", lk.t, handName(lk.free), rel.x, rel.y, rel.z, fr.x,
        fr.y - gr.y, fr.z, gun.x, gun.y, gun.z, lk.pair ? 1 : 0, busy(lk.free) ? 1 : 0, ps.portGap, gripShare(lk.free),
        view::drawnGripCurl(lk.free));
}

[[nodiscard]] int lockValue()
{
    return static_cast<int>(vr_flat_prl_lock.value + (vr_flat_prl_lock.value >= 0.f ? 0.5f : -0.5f));
}

[[nodiscard]] bool gunIn(int h)
{
    return weapons::heldModel(h) != nullptr && weapons::heldSlot(h) != weapons::fistSlot();
}

// The hand of the gun the lock holds: a gun with a reload in one hand, the other empty (the main hand's first); -1 none.
[[nodiscard]] int lockGun()
{
    for(int g : {HAND_MAIN, HAND_OFF})
    {
        if(gunIn(g) && !gunIn(1 - g) && !flatplay::holdsThing(1 - g) && reloadOf(weaponOf(g)))
        {
            return g;
        }
    }
    return -1;
}

// vr_flat_prl_lock_next / _prev: the next or the previous key (from off or the scrub, the first or the last).
void lockStep(int by)
{
    const int g = lockGun();
    const Reload* r = g >= 0 ? reloadOf(weaponOf(g)) : nullptr;
    const int n = r ? r->keyCount : 1;
    int k = lockValue();
    if(k <= 0)
    {
        k = by > 0 ? 0 : n + 1;
    }
    k += by;
    k = k > n ? 1 : (k < 1 ? n : k);
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

// The free hand's target this frame for key `k` (its pose as `s` has it now, before the trip moves it; the gun in `g` posed
// already): place and turn. Hold: `held` (where it was). The port and its opening's way into `port`, `face`.
void target(const hands::State& s, int f, int g, const Reload& r, const Key& k, const glm::vec3& heldPos, const glm::vec3& heldRot,
    glm::vec3& pos, glm::vec3& rot, glm::vec3* portOut = nullptr)
{
    const float side = sideOf(f);
    const glm::mat3 gunAxes = held::axesFromAngles(&s.rot[g][0], true);
    const glm::vec3 port = s.pos[g] + gunAxes * ps.portIn;
    const glm::vec3 face = glm::normalize(gunAxes * ps.faceIn);
    if(portOut)
    {
        *portOut = port;
    }
    switch(k.target)
    {
        case Target::Rest:
            pos = s.pos[f] + fromBody({k.handOfs.x, side * k.handOfs.y, k.handOfs.z}, s.bodyYaw);
            rot = s.rot[f] + mirrorRot(k.handRot, side);
            return;
        case Target::Pouch:
            pos = body::ammoPouchPosition(s) + fromBody({k.handOfs.x, side * k.handOfs.y, k.handOfs.z}, s.bodyYaw);
            rot = glm::vec3{0.f, s.bodyYaw, 0.f} + mirrorRot(k.handRot, side);
            return;
        case Target::Mouth:
        case Target::Dest:
        {
            // Backwards: the item's turn at its destination (the reload's, the key's on top, in the gun's frame), its place
            // (the port, out of its opening by the approach; the key's offset in the gun's frame: forward, left, up, the right
            // mirrored), then the hand that holds it there as it sits in the hand.
            const glm::mat3 itemDest = gunAxes * held::axesFromAngles(&mirrorRot(k.handRot, side)[0], true) *
                                       held::axesFromAngles(&mirrorRot(r.itemRot, side)[0], true);
            const float out = za::max(k.target == Target::Mouth ? k.approach : 0.f, vr_flat_prl_test_miss.value);
            const glm::vec3 itemAt = port + face * out + gunAxes * glm::vec3{k.handOfs.x, side * k.handOfs.y, k.handOfs.z};
            const glm::mat3 hand = itemDest * glm::transpose(item.axes);
            pos = itemAt - hand * item.at;
            held::anglesFromAxes(hand, &rot[0], true);
            return;
        }
        default:
            pos = heldPos;
            rot = heldRot;
            return;
    }
}

// The hands' place for `o` (offsets from their own poses in `s`).
void placeOf(const hands::State& s, int f, const Offsets& o, glm::vec3& pos, glm::vec3& rot)
{
    pos = o.hand ? s.pos[f] + fromBody(o.handOfs, s.bodyYaw) : s.pos[f];
    rot = o.hand ? o.handRot + glm::vec3{0.f, s.bodyYaw, 0.f} : s.rot[f];
}

void offsetsOf(const hands::State& s, int f, const glm::vec3& pos, const glm::vec3& rot, Offsets& o)
{
    o.handOfs = toBody(pos - s.pos[f], s.bodyYaw);
    o.handRot = rot - glm::vec3{0.f, s.bodyYaw, 0.f};
    o.hand = true;
}

// Key `k` played from `from` at shares `t` (the hand) and `gt` (the gun), before their ease: the gun posed in `s` (first: the
// destination moves with it), then the free hand. Its result in `out` (the next key's start).
void playKey(hands::State& s, int f, int g, const Reload& r, int k, const Offsets& from, float t, float gt, Offsets& out)
{
    const Key& key = r.keys[k];
    glm::vec3 gunRot, gunOfs;
    keyGun(key, g, gunRot, gunOfs);
    const float ge = reloadsys::ease(key.ease, gt);
    out.gunRot = from.gunRot + (gunRot - from.gunRot) * ge;
    out.gunOfs = from.gunOfs + (gunOfs - from.gunOfs) * ge;
    glm::vec3 fwd, right;
    bodyAxes(s.bodyYaw, fwd, right);
    s.rot[g] += out.gunRot;
    s.pos[g] += fromBody(out.gunOfs, s.bodyYaw);
    ps.gunHand = g;
    ps.gunNow = out;
    glm::vec3 fromPos, fromRot, toPos, toRot, port;
    placeOf(s, f, from, fromPos, fromRot);
    target(s, f, g, r, key, fromPos, fromRot, toPos, toRot, &port);
    const float e = reloadsys::ease(key.ease, t);
    glm::vec3 pos = fromPos + (toPos - fromPos) * e;
    if(key.arc > 0.f)
    {
        // Bowed up and out (away from the body's middle) on the way, most at its middle.
        pos += (glm::vec3{0.f, 0.f, 1.f} + right * (sideOf(f) * 0.5f)) * (key.arc * za::sin(pi * za::clamp(t, 0.f, 1.f)));
    }
    const glm::vec3 rot = flatplay::blendHandAngles(fromRot, toRot, e);
    offsetsOf(s, f, pos, rot, out);
    s.pos[f] = pos;
    s.rot[f] = rot;
    const glm::vec3 toFree = pos - port;
    const glm::vec3 face = glm::normalize(held::axesFromAngles(&s.rot[g][0], true) * ps.faceIn);
    ps.faceToFree = glm::length(toFree) > 1e-3f ? glm::dot(face, glm::normalize(toFree)) : 0.f;
    ps.portGap = glm::distance(pos + held::axesFromAngles(&rot[0], true) * item.at, port);
}

// The end pose of key `k` (its targets reached), from the end of the keys before it (the lock: each key's pose made from
// the data each frame). The hands of `s` are left as they were.
void keyEnd(const hands::State& s, int f, int g, const Reload& r, int k, Offsets& out)
{
    Offsets o; // (the hands at their own before the first key)
    for(int i = 0; i <= k && i < r.keyCount; i++)
    {
        hands::State tmp = s;
        Offsets next;
        playKey(tmp, f, g, r, i, o, 1.f, 1.f, next);
        o = next;
    }
    out = o;
}

// This frame's lock (`value`: a key from 1, or -1 the scrub): the gun and the place. False: no gun with a reload held
// one-handed to lock.
bool lockFrame(int value)
{
    const int g = lockGun();
    if(g < 0)
    {
        if(lk.on || lk.shown != -100)
        {
            Con_Printf("flatprocreload: the lock needs a gun with a procedural reload in one hand and the other empty\n");
        }
        lk.on = false;
        lk.shown = -100;
        return false;
    }
    if(ps.trip.on)
    {
        ps.trip = Trip{}; // (a trip under way dropped: the server's give-up ends its side, the round refunded)
    }
    const Reload& r = *reloadOf(weaponOf(g));
    lk.on = true;
    lk.gun = g;
    lk.free = 1 - g;
    lk.weapon = r.weapon;
    if(value > 0)
    {
        lk.key = za::min(value, r.keyCount) - 1;
        lk.t = 1.f;
    }
    else
    {
        // The scrub: its share of one pass through the keys (each its hand's time).
        float total = 0.f;
        for(int i = 0; i < r.keyCount; i++)
        {
            total += reloadsys::handTime(r.keys[i]);
        }
        float at = za::clamp(vr_flat_prl_scrub.value, 0.f, 1.f) * total;
        lk.key = r.keyCount - 1;
        lk.t = 1.f;
        for(int i = 0; i < r.keyCount; i++)
        {
            const float d = reloadsys::handTime(r.keys[i]);
            if(at <= d || i == r.keyCount - 1)
            {
                lk.key = i;
                lk.t = za::clamp(at / d, 0.f, 1.f);
                break;
            }
            at -= d;
        }
    }
    const int take = reloadsys::takeKey(r), insert = reloadsys::insertKey(r);
    lk.pair = take >= 0 && insert >= take && lk.key >= take && lk.key <= insert;
    if(lk.shown != value)
    {
        Con_Printf("flatprocreload: locked at %s %d (%s; the %s hand, the gun in the %s)\n", value > 0 ? "key" : "the scrub in key",
            lk.key + 1, r.keys[lk.key].name, handName(lk.free), handName(g));
        lk.shown = value;
    }
    measurePort(g);
    return true;
}

void applyLock(hands::State& s)
{
    const Reload* r = reloadOf(lk.weapon);
    if(!r)
    {
        return;
    }
    Offsets from;
    if(lk.key > 0)
    {
        keyEnd(s, lk.free, lk.gun, *r, lk.key - 1, from);
    }
    const float gt = za::clamp(lk.t * reloadsys::handTime(r->keys[lk.key]) / reloadsys::gunTime(r->keys[lk.key]), 0.f, 1.f);
    playKey(s, lk.free, lk.gun, *r, lk.key, from, lk.t, gt, lk.last);
    if(!lk.pair)
    {
        ps.portGap = -1.f;
    }
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_flatprocreload_report", report_f);
    Cmd_AddCommand("vr_flat_prl_lock_next", lockNext_f);
    Cmd_AddCommand("vr_flat_prl_lock_prev", lockPrev_f);
    reloadsys::init();
    (void)reloadsys::load();
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
        lk.shown = -100;
        dg.share = 0.f;
        return;
    }
    const float step = static_cast<float>(dt);
    const Reload* r = t.on ? reloadOf(t.weapon) : nullptr;
    const Key* key = r && t.key >= 0 ? &r->keys[t.key] : nullptr;
    // The drawn free hand on the drawn gun: eased in as the item sets off for its destination, out as the hand leaves it.
    {
        const Reload* lr = lk.on ? reloadOf(lk.weapon) : nullptr;
        const Key* lkey = lr ? &lr->keys[lk.key] : nullptr;
        const bool toPort = lk.on ? (lk.pair && lkey && (lkey->target == Target::Mouth || lkey->target == Target::Dest))
                                  : (key && (key->target == Target::Mouth || key->target == Target::Dest));
        const float rate = step / attachTime;
        dg.share = toPort ? za::min(1.f, dg.share + rate) : za::max(0.f, dg.share - rate);
        // The free hand's fingers: a headset's grip on the item while it holds one (gripShare), eased in and out.
        const bool holds = lk.on ? lk.pair : (t.on && carriedEnt(t.free) > 0);
        const float grip = step / gripTime;
        ps.grip = holds ? za::min(1.f, ps.grip + grip) : za::max(0.f, ps.grip - grip);
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
    // The key-pose lock: held there, the trip's steps not run (a reload started under it isn't driven).
    if(const int value = lockValue(); value != 0)
    {
        if(vr_flat_proc_reload_debug.value >= 2.f && lk.on && dt > 0.0)
        {
            traceGun();
        }
        if(lockFrame(value))
        {
            return;
        }
    }
    else if(lk.on)
    {
        // Let go: the reload's end played from where the lock drew the hands (as a cancel's).
        lk.on = false;
        lk.shown = -100;
        t = Trip{};
        t.on = true;
        t.weapon = lk.weapon;
        t.gun = lk.gun;
        t.free = lk.free;
        t.last = lk.last;
        if(debugOn())
        {
            Con_Printf("flatprocreload: the lock let go\n");
        }
        toEnding("the lock let go");
        upHand = -1;
        r = reloadOf(t.weapon);
    }
    if(upHand >= 0 && (!t.on || t.ending))
    {
        const Reload* nr = reloadOf(weaponOf(upHand));
        if(nr)
        {
            if(!t.on)
            {
                t.last = Offsets{};
            }
            t.on = true;
            t.ending = false;
            t.weapon = nr->weapon;
            t.gun = upHand;
            t.free = 1 - upHand;
            t.hadItem = false;
            t.key = -1;
            const int first = reloadsys::firstKey(*nr, false, [&](Cond c) { return condMet(c, t.gun); });
            if(first >= 0)
            {
                startKey(first, "the server's reload starts");
            }
            else
            {
                endTrip();
            }
            r = nr;
        }
    }
    if(vr_flat_proc_reload_debug.value >= 2.f && t.on && dt > 0.0)
    {
        traceGun(); // (as drawn last frame: before this frame's step)
    }
    if(!t.on || !r || t.key < 0)
    {
        t.on = t.on && r && t.key >= 0;
        return;
    }
    const bool up = ps.bitUp[t.gun];
    if(!t.ending)
    {
        if(!up)
        {
            toEnding("the server's reload is over");
        }
        else if(client::attacking(t.gun))
        {
            cancel("fired", false); // (the server cancels it by the fire button)
        }
        else if(flatplay::punching(t.free) || flatplay::punching(t.gun) || flatplay::shoving(t.free) || flatplay::shoving(t.gun))
        {
            cancel("a quick melee", true);
        }
        if(!t.on)
        {
            return;
        }
    }
    measurePort(t.gun);
    key = &r->keys[t.key];
    t.t = za::min(1.f, t.t + step / reloadsys::handTime(*key));
    t.gt = za::min(1.f, t.gt + step / reloadsys::gunTime(*key));
    const double since = vr_gametime - t.keyAt;
    const int carried = carriedEnt(t.free);
    const float failsafe = timeOf(vr_flat_prl_failsafe, 0.05f);
    const auto cond = [&](Cond c) { return condMet(c, t.gun); };
    // The key's open / close at its start.
    if(!t.actionSent && (key->action == Action::Open || key->action == Action::Close))
    {
        t.actionSent = true;
        Cbuf_InsertText(key->action == Action::Open ? "impulse 107\n" : "impulse 108\n");
        if(debugOn())
        {
            Con_Printf("flatprocreload: impulse %d: the gun %s\n", key->action == Action::Open ? 107 : 108,
                key->action == Action::Open ? "opened" : "closed");
        }
    }
    // The item gone from the hand (seated, or lost) on its way: on after the insert's key at once.
    const int insert = reloadsys::insertKey(*r);
    if(carried > 0 && !t.hadItem && !t.ending)
    {
        t.hadItem = true;
        measureRound(t.free);
        t.clipAtCarry = clipOf(t.gun);
    }
    if(t.hadItem && carried == 0 && !t.ending)
    {
        t.hadItem = false;
        const bool in = clipOf(t.gun) > t.clipAtCarry;
        (in ? ps.seated : ps.dropped)++;
        if(debugOn())
        {
            Con_Printf("flatprocreload: the round %s (key %d %s %.2f s in; clip %d; fingers %.2f, grip %.2f)\n", in ? "went in" : "left the hand",
                t.key + 1, key->name, since, clipOf(t.gun), view::drawnGripCurl(t.free), gripShare(t.free));
        }
        const int n = insert >= 0 ? reloadsys::nextKey(*r, insert, up, cond) : -1;
        if(!up)
        {
            toEnding(in ? "seated: the gun is full" : "lost on the way: the reload is over");
        }
        else if(n >= 0)
        {
            startKey(n, in ? "seated: the next round" : "lost on the way: another");
        }
        else
        {
            toEnding(in ? "seated: the last key" : "lost on the way");
        }
        return;
    }
    bool done = t.t >= 1.f;
    switch(key->action)
    {
        case Action::Take:
            // (Sent again every 0.1 s while nothing comes: the server may see the hand at another hotspot for a frame, its
            // place sent with the move; seen once sliding at 60 fps, reloadsys's matrix.)
            if(t.takeSent && carried == 0 && vr_gametime - t.takeAt >= 0.1)
            {
                t.takeSent = false;
            }
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
            done = carried > 0 && since >= static_cast<double>(reloadsys::handTime(*key)) - 1e-4;
            if(carried == 0 && since >= static_cast<double>(reloadsys::handTime(*key) + failsafe))
            {
                ps.failsafes++;
                cancel(t.takeSent ? "failsafe: the pouch gave nothing" : "failsafe: the hand never reached the pouch's hotspot", true);
                return;
            }
            break;
        case Action::Insert:
            done = false;
            if(t.t >= 1.f)
            {
                if(!t.atPort)
                {
                    t.atPort = true;
                    t.atPortAt = vr_gametime;
                    ps.insertGap = ps.portGap;
                }
                else if(vr_gametime - t.atPortAt >= static_cast<double>(failsafe))
                {
                    ps.failsafes++;
                    cancel("failsafe: the round wasn't seated at the port", true);
                    return;
                }
            }
            break;
        default: break;
    }
    if(!done)
    {
        return;
    }
    // A key between the take and the insert waits for nothing more; the last key also for the gun's pose.
    const int n = reloadsys::nextKey(*r, t.key, up && !t.ending, cond);
    if(n < 0)
    {
        if(t.gt >= 1.f)
        {
            endTrip();
        }
        return;
    }
    if(!t.ending && !up && !r->keys[n].finish)
    {
        toEnding("the server's reload is over");
        return;
    }
    startKey(n, "the key's end");
}

void apply(hands::State& s)
{
    if(lk.on)
    {
        applyLock(s);
        return;
    }
    Trip& t = ps.trip;
    const Reload* r = t.on ? reloadOf(t.weapon) : nullptr;
    if(!r || t.key < 0 || t.key >= r->keyCount)
    {
        ps.gunNow = Offsets{};
        return;
    }
    if(t.snap)
    {
        t.snap = false;
        t.from = t.last;
    }
    playKey(s, t.free, t.gun, *r, t.key, t.from, t.t, t.gt, t.last);
    if(carriedEnt(t.free) <= 0)
    {
        ps.portGap = -1.f;
    }
}

float gripShare(int h)
{
    const int f = lk.on ? lk.free : ps.trip.free;
    return busy(h) && h == f ? geom::smooth01(ps.grip) : 0.f;
}

void noteDrawn(const hands::State& s, int hand, const glm::vec3& pos, const glm::vec3& rot)
{
    const int g = lk.on ? lk.gun : ps.trip.gun;
    if(hand != g || (!lk.on && !ps.trip.on))
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
    // The game's free hand from the game's gun hand, put on the gun hand as drawn (its sway, its own drawn turn).
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
    const bool trip = ps.trip.on && !ps.trip.ending && g == ps.trip.gun;
    if(!trip || !ps.portKnown || !s.loadPortValid[g])
    {
        return false;
    }
    port = s.pos[g] + held::axesFromAngles(&s.rot[g][0], true) * ps.portIn;
    return true;
}

bool busy(int h)
{
    return (ps.trip.on && h == ps.trip.free) || (lk.on && h == lk.free);
}

bool lockPair(int& hand, float& scale)
{
    hand = lk.free;
    scale = item.scale;
    return lk.on && lk.pair;
}

glm::vec3 lockPairPlace(const glm::vec3& handPos, const glm::vec3& handRot, glm::mat3& axes)
{
    const glm::mat3 hand = held::axesFromAngles(&handRot[0], true);
    axes = hand * item.axes;
    return handPos + hand * item.at;
}

Status status()
{
    Status st;
    st.locked = lk.on;
    st.lockKey = lk.on ? lk.key : -1;
    st.lockShare = lk.t;
    st.lockGun = lk.on ? lk.gun : lockGun();
    st.lockWeapon = st.lockGun >= 0 ? weaponOf(st.lockGun) : 0;
    st.tripOn = ps.trip.on;
    st.tripWeapon = ps.trip.weapon;
    st.tripKey = ps.trip.key;
    st.tripShare = ps.trip.t;
    st.portGap = ps.portGap;
    st.insertGap = ps.insertGap;
    st.trips = ps.trips;
    st.seated = ps.seated;
    st.dropped = ps.dropped;
    st.cancels = ps.cancels;
    st.failsafes = ps.failsafes;
    st.lastCancel = ps.lastCancel;
    return st;
}

bool lockFrames(glm::vec3& framePos, glm::mat3& frameAxes, glm::vec3& handPos, glm::vec3& handRot, int& kind)
{
    const Reload* r = lk.on ? reloadOf(lk.weapon) : nullptr;
    if(!r || !view::drawnHandPose(lk.free, handPos, handRot))
    {
        return false;
    }
    const hands::State& cur = hands::current();
    const Key& k = r->keys[lk.key];
    kind = static_cast<int>(k.target);
    glm::vec3 fwd, right;
    bodyAxes(cur.bodyYaw, fwd, right);
    framePos = handPos;
    if(k.target == Target::Mouth || k.target == Target::Dest)
    {
        glm::vec3 gp, gr;
        if(!view::drawnHandPose(lk.gun, gp, gr))
        {
            return false;
        }
        frameAxes = held::axesFromAngles(&gr[0], true); // (forward, left, up)
    }
    else
    {
        frameAxes = glm::mat3{fwd, -right, glm::vec3{0.f, 0.f, 1.f}};
    }
    return true;
}

} // namespace qvr::flatprocreload
