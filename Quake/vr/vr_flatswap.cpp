// vr_flatswap.cpp -- flatscreen play's hand swap as drawn (his words 2026-11-10: "Give the hand swap button ('3') a short
// animation. When dual wielding or holding a weapon with one hand only, both hands move close to each other, perform the
// swap, then go back to their positions. When holding a single weapon with two hands, the off-hand detaches, moves closer
// to the main hand, the swap is performed, and the hands then go to their rightful positions."; FLATPLAY.md, "The hand swap").
//
// The game swaps (QC vr_wpnholster.qc VR_FlatSwapHands, impulse 48), as before; this file only times it and draws the hands.
// The key's press (vr_flat_key_swap) starts the trip: over vr_flat_swap_in seconds the hands go to their meeting point --
// each half of vr_flat_swap_gap off the middle of the two, vr_flat_swap_down below it -- and the swap is sent as they
// meet. Once the hands' weapons have changed (the server's answer), each hand turned the way the other was (the weapon
// keeps its turn as it changes hands), they ease back over vr_flat_swap_out to their own poses.
//
// A weapon held in both hands goes in three steps (his words 2026-11-10, later: "The off-hand should be released first
// and go to the regular 'empty hand in front' pose, then the weapon should be passed to that empty hand, and then the new
// off-hand should be attached"; apply2h): the free hand off the grip to the empty hand's guard
// (vr_flat_swap_2h_release_time), the weapon brought across to it and the swap sent there (_pass_time), then the hand that
// took it to its hold and the hand that let go onto the gun's two-hand grip (_attach_time; vr_flatsight.cpp,
// vr_twohand.cpp: the grip is blocked until the swap has happened, gripBlocked).
//
// Every weapon held alone with the other hand empty counts as held in both (2026-11-11, vr_flat_swap_2h_all): on its
// grip (the rocket launcher) or in the support pose beside it (the shotguns, the nailguns, melee weapons). What a hand
// carries (a prop, a gib, a torch, a ragdoll's limb) changes hands as a weapon does (vr_flat_swap_things; QC
// VR_FlatSwapHands moves it to the other hand's grip): a prop and a gun, two props, a prop and an empty hand meet and swap.
//
// The places are kept in the view's frame off the head (taken on the first pose after the press; Frame), so a turn of the
// view, up and down as well as around, carries the hands and the weapon with it, and the swap plays the same against
// the view whether he looks up, level or down (his words 2026-11-11). The empty hand's guard the free hand goes to is
// placed as at a level view and carried into the view's frame the same way; the tunables are offsets in that frame. The fire buttons do nothing while it runs (flatplay::routeFire). vr_flat_swap_anim 0:
// the swap at once, as before. vr_flat_swap_debug 1 prints the steps (flatswap: ...); vr_flatswap_report the state.

#include "vr_flatswap.hpp"

#include "vr_api.h"
#include "vr_backend.hpp" // HAND_OFF / HAND_MAIN
#include "vr_cvars.hpp"
#include "vr_tunables.hpp"
#include "vr_engine.hpp"
#include "vr_flatgrenade.hpp"
#include "vr_flatplay.hpp"
#include "vr_flatreload.hpp"
#include "vr_geom.hpp"
#include "vr_held.hpp"
#include "vr_protocol.hpp"
#include "vr_twohand.hpp"
#include "vr_weapon_ids.h"

#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "vr_zancle.hpp"

namespace qvr::flatswap
{

namespace
{

// A swap whose answer never comes (the game refused it: a hand took a thing meanwhile) is given up on after this long at
// the meeting point, and the hands go back.
constexpr double answerWait = 0.3;

// The swap as drawn (no function-local statics: a named file-scope struct, CODE_STYLE.md).
struct Swap
{
    bool running{false};
    bool sent{false};        // impulse 48 sent
    bool seen{false};        // the hands' weapons changed (or the wait for it given up)
    bool twoHand{false};     // a weapon held in both hands at the press: the free hand goes to the holding one
    int holder{HAND_MAIN};   // (twoHand) the hand holding the weapon
    int widWas[2]{WID_FIST, WID_FIST}; // what each hand held at the press (STAT_QVR_WEAPON2, STAT_QVR_WEAPON)
    int carryWas[2]{0, 0};   // and what each carried (STAT_QVR_CARRYOFF, STAT_QVR_CARRYMAIN: a prop, a gib, a torch, a limb)
    double startAt{-1e9};
    double sentAt{-1e9};
    double peakAt{-1e9};     // when the swap was seen: the way back starts
    bool captured{false};    // the hands' places taken (the first pose after the press)
    glm::vec3 local[2]{glm::vec3{0.f}, glm::vec3{0.f}}; // each hand then, off the head in the view's frame (fwd, right, up)
    glm::vec3 rot[2]{glm::vec3{0.f}, glm::vec3{0.f}};   // and its angles against the view's (Frame::rotToLocal)
    float share{0.f};        // the last pose's share of the way to the meeting point (the report)
    bool meetPrinted{false}; // (vr_flat_swap_debug) the meeting point's line printed
    // A weapon held in both hands (his words 2026-11-10: "The off-hand should be released first and go to the regular
    // 'empty hand in front' pose, then the weapon should be passed to that empty hand, and then the new off-hand should
    // be attached"): each hand where the pass left it (in the view's frame off the head, turned against the view), and
    // whether the release's end was printed (vr_flat_swap_debug).
    glm::vec3 passLocal[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    glm::vec3 passRot[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    bool releasePrinted{false};
    bool guardTaken{false};
    glm::vec3 guardLocal{0.f}; // the free hand's guard, in the view's frame (taken at the press)
    int presses{0};
    int swaps{0};
};

Swap sw;

[[nodiscard]] float inTime()
{
    return za::clamp(vr_flat_swap_in.value, 0.02f, 2.f);
}

[[nodiscard]] float outTime()
{
    return za::clamp(vr_flat_swap_out.value, 0.02f, 2.f);
}

[[nodiscard]] int widIn(int h)
{
    return cl.stats[h == HAND_MAIN ? protocol::STAT_QVR_WEAPON : protocol::STAT_QVR_WEAPON2];
}

// The entity `h` carries (the server's word), 0 none.
[[nodiscard]] int carryIn(int h)
{
    return cl.stats[h == HAND_MAIN ? protocol::STAT_QVR_CARRYMAIN : protocol::STAT_QVR_CARRYOFF];
}

[[nodiscard]] const char* handName(int h)
{
    return h == HAND_MAIN ? "main" : "off";
}

[[nodiscard]] float releaseTime()
{
    return za::clamp(vr_flat_swap_2h_release_time.value, 0.02f, 2.f);
}

[[nodiscard]] float passTime()
{
    return za::clamp(vr_flat_swap_2h_pass_time.value, 0.02f, 2.f);
}

[[nodiscard]] float attachTime()
{
    return za::clamp(vr_flat_swap_2h_attach_time.value, 0.02f, 2.f);
}

// When the swap is sent after the press (the hands meet; held in both hands: the release, then the pass), and how long
// the way back takes once it has happened (held in both hands: the new free hand's way onto the grip).
[[nodiscard]] float sendTime()
{
    return sw.twoHand ? releaseTime() + passTime() : inTime();
}

[[nodiscard]] float backTime()
{
    return sw.twoHand ? attachTime() : outTime();
}

// The trip's share now: up to the meeting point, held there until the swap arrives, back.
[[nodiscard]] float shareNow()
{
    if(!sw.running)
    {
        return 0.f;
    }
    if(!sw.seen)
    {
        return geom::smooth01(static_cast<float>(vr_gametime - sw.startAt) / sendTime());
    }
    return 1.f - geom::smooth01(static_cast<float>(vr_gametime - sw.peakAt) / backTime());
}

// A new swap's state, the counts kept.
void reset()
{
    const int presses = sw.presses;
    const int swaps = sw.swaps;
    sw = Swap{};
    sw.presses = presses;
    sw.swaps = swaps;
}

// The frame the swap is posed in: the view's (pitch and yaw, the pitch held to vr_flat_pose_pitch as the hands' own
// poses hold it; his words 2026-11-11: "the current camera angle should be taken into account"), off the head. Places in it are (fwd, right, up); turns are Quake angles against its axes
// (x forward, y left, z up), so at a level view a turn's pitch and roll are the world's and its yaw the yaw off the view's.
struct Frame
{
    float pitch{0.f}; // (the look's, held to vr_flat_pose_pitch)
    glm::vec3 head{0.f};
    glm::vec3 fwd{1.f, 0.f, 0.f};
    glm::vec3 right{0.f, -1.f, 0.f};
    glm::vec3 up{0.f, 0.f, 1.f};

    [[nodiscard]] glm::vec3 toLocal(const glm::vec3& p) const
    {
        const glm::vec3 d = p - head;
        return {glm::dot(d, fwd), glm::dot(d, right), glm::dot(d, up)};
    }

    [[nodiscard]] glm::vec3 toWorld(const glm::vec3& l) const
    {
        return head + fwd * l.x + right * l.y + up * l.z;
    }

    [[nodiscard]] glm::vec3 dirIn(const glm::vec3& d) const
    {
        return {glm::dot(d, fwd), -glm::dot(d, right), glm::dot(d, up)};
    }

    [[nodiscard]] glm::vec3 dirOut(const glm::vec3& l) const
    {
        return fwd * l.x - right * l.y + up * l.z;
    }

    [[nodiscard]] glm::vec3 rotToLocal(const glm::vec3& angles) const
    {
        glm::vec3 f, r, u;
        hands::angleVectors(angles, f, r, u);
        return hands::anglesFromVectors(dirIn(f), dirIn(u));
    }

    [[nodiscard]] glm::vec3 rotToWorld(const glm::vec3& local) const
    {
        glm::vec3 f, r, u;
        hands::angleVectors(local, f, r, u);
        return hands::anglesFromVectors(dirOut(f), dirOut(u));
    }
};

[[nodiscard]] Frame viewFrame(const hands::State& s)
{
    Frame v;
    // (Off the eye the view is drawn from: the neck pivot carries it with the look, vr_flat_eye_pivot, as the poses' hands.)
    v.head = s.head + flatplay::eyeNudge(s.headAngles);
    // (Pitched no further than the hands' own poses follow the look, vr_flat_pose_pitch: flatplay's poseHands, flatfgrab's
    // placeFrame. Past it the guns stay put, and the swap with them.)
    const float most = za::clamp(vr_flat_pose_pitch.value, 0.f, 90.f);
    v.pitch = za::clamp(s.headAngles.x, -most, most);
    hands::angleVectors({v.pitch, s.headAngles.y, 0.f}, v.fwd, v.right, v.up);
    return v;
}

[[nodiscard]] Frame levelFrame(const hands::State& s)
{
    Frame v;
    v.head = s.head + flatplay::eyeNudge({0.f, s.headAngles.y, 0.f}); // (a level look's eye)
    hands::angleVectors({0.f, s.headAngles.y, 0.f}, v.fwd, v.right, v.up);
    return v;
}

void report_f()
{
    Con_Printf("flatswap: running %d, %s, holder %s, sent %d, seen %d, share %.2f; presses %d, swaps %d (anim %g)\n",
        sw.running ? 1 : 0, sw.twoHand ? "a weapon in both hands" : "each hand its own", handName(sw.holder), sw.sent ? 1 : 0,
        sw.seen ? 1 : 0, sw.share, sw.presses, sw.swaps, vr_flat_swap_anim.value);
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_flatswap_report", report_f);
}

bool press()
{
    sw.presses++;
    if(vr_flat_swap_anim.value == 0.f)
    {
        return false;
    }
    if(sw.running)
    {
        return true; // (a press while one runs: no second swap, which would swap them back halfway)
    }
    const int wo = widIn(HAND_OFF);
    const int wm = widIn(HAND_MAIN);
    const int co = carryIn(HAND_OFF);
    const int cm = carryIn(HAND_MAIN);
    // What a hand carries changes hands too (his words 2026-11-11: "The swap hands key should also seamlessly work when a
    // prop is being held, or any combination of props/weapons"; QC VR_FlatSwapHands, vr_flat_swap_things), drawn the way
    // two weapons are: the hands meet, swap and go back. One thing held in both hands (a big prop, two limbs of one body)
    // has nothing to change: the game swaps nothing, and nothing is drawn.
    const bool things = vr_flat_swap_things.value != 0.f;
    const bool nothing = wo == WID_FIST && wm == WID_FIST && (!things || (co == 0 && cm == 0));
    const bool carrying = things ? co != 0 && co == cm
                                 : co != 0 || cm != 0 || held::heldEntity(HAND_OFF) != 0 || held::heldEntity(HAND_MAIN) != 0;
    const bool grenade = flatgrenade::busy(HAND_OFF) || flatgrenade::busy(HAND_MAIN);
    if(nothing || carrying || grenade || flatreload::busy(HAND_OFF) || flatreload::busy(HAND_MAIN))
    {
        Cvar_SetValueQuick(&vr_flat_swap_seen, 0.f);
        if(vr_flat_swap_debug.value)
        {
            Con_Printf("flatswap: not drawn (%s): the swap at once\n",
                nothing                ? "both hands empty"
                : carrying && things   ? "one thing in both hands"
                : carrying             ? "a hand carries a thing"
                : grenade              ? "the quick grenade"
                                       : "a reload under way");
        }
        return false;
    }
    reset();
    sw.running = true;
    sw.startAt = vr_gametime;
    sw.widWas[HAND_OFF] = wo;
    sw.widWas[HAND_MAIN] = wm;
    sw.carryWas[HAND_OFF] = co;
    sw.carryWas[HAND_MAIN] = cm;
    // A weapon in both hands (his words 2026-11-11: "All cases where a single weapon is held with two hands should follow
    // those animation steps"): one weapon, the other hand empty. That hand is on the weapon's two-hand grip (the rocket
    // launcher's, the grenade launcher's: a grip ahead of the trigger hand, vr_twohand's hold) or, where the grip stands on
    // or behind the trigger hand (the shotguns', the nailguns'), on the melee weapons, in the support pose beside it
    // (flatplay's targetOffset, vr_flat_support_*): held in both either way. Before, only vr_twohand's hold counted
    // (twohand::helping), and every gun of the second kind went the hands' meeting way: vr_flat_swap_2h_all 0.
    const int lone = flatplay::loneWeaponHand();
    const bool freeEmpty = lone >= 0 && !flatplay::holdsThing(1 - lone);
    sw.twoHand = lone >= 0 && (vr_flat_swap_2h_all.value != 0.f ? freeEmpty : twohand::helping(1 - lone));
    sw.holder = lone >= 0 ? lone : HAND_MAIN;
    Cvar_SetValueQuick(&vr_flat_swap_seen, sw.twoHand ? 2.f : 1.f);
    if(vr_flat_swap_debug.value)
    {
        Con_Printf("flatswap: start (%s; off %d, main %d; carried off %d, main %d)\n",
            sw.twoHand ? "a weapon in both hands: release, pass, attach" : "the hands to each other", wo, wm, co, cm);
    }
    return true;
}

void frame(bool on)
{
    if(!on)
    {
        if(sw.running && !sw.sent)
        {
            Cbuf_InsertText("impulse 48\n"); // (the mode ended on the way: the press still swaps)
        }
        reset();
        return;
    }
    if(!sw.running)
    {
        return;
    }
    const double now = vr_gametime;
    if(!sw.sent && now - sw.startAt >= static_cast<double>(sendTime()) - 1e-4)
    {
        sw.sent = true;
        sw.sentAt = now;
        sw.swaps++;
        Cbuf_InsertText("impulse 48\n"); // (InsertText: now, not after what is queued; QC VR_FlatSwapHands)
        if(vr_flat_swap_debug.value)
        {
            Con_Printf("flatswap: the hands meet (%.2f s): swap sent\n", now - sw.startAt);
        }
    }
    if(sw.sent && !sw.seen)
    {
        const bool changed = widIn(HAND_OFF) != sw.widWas[HAND_OFF] || widIn(HAND_MAIN) != sw.widWas[HAND_MAIN] ||
                             carryIn(HAND_OFF) != sw.carryWas[HAND_OFF] || carryIn(HAND_MAIN) != sw.carryWas[HAND_MAIN];
        // (Two of the same weapon swap with no change to see: the wait's end is the answer then.)
        const bool same = sw.widWas[HAND_OFF] == sw.widWas[HAND_MAIN] && sw.carryWas[HAND_OFF] == sw.carryWas[HAND_MAIN];
        if(changed || now - sw.sentAt >= (same ? 0.05 : answerWait))
        {
            sw.seen = true;
            sw.peakAt = now;
            if(vr_flat_swap_debug.value)
            {
                Con_Printf("flatswap: %s (%.2f s): back\n", changed ? "swapped" : same ? "the same weapon in each" : "no swap came",
                    now - sw.startAt);
            }
        }
    }
    if(sw.seen && now - sw.peakAt >= static_cast<double>(backTime()))
    {
        sw.running = false;
        sw.share = 0.f;
        if(vr_flat_swap_debug.value)
        {
            Con_Printf("flatswap: done (%.2f s)\n", now - sw.startAt);
        }
    }
}

namespace
{

// `h`'s turn towards `want` (Quake angles) by `a`.
void turnTo(hands::State& s, int h, const glm::vec3& want, float a)
{
    s.rot[h].x += geom::yawDelta(want.x, s.rot[h].x) * a;
    s.rot[h].y += geom::yawDelta(want.y, s.rot[h].y) * a;
    s.rot[h].z += geom::yawDelta(want.z, s.rot[h].z) * a;
}

// (vr_flat_swap_debug) Each hand's place and turn in the view's frame as they meet: the same whatever the view's pitch
// (flatswappitch_test.sh compares them). One line a hand (the console's lines are short).
void printInView(const hands::State& s, const Frame& v)
{
    for(int h : {HAND_OFF, HAND_MAIN})
    {
        const glm::vec3 p = v.toLocal(s.pos[h]), r = v.rotToLocal(s.rot[h]);
        Con_Printf("flatswap: in view %.1f %.1f: %s %.2f %.2f %.2f turn %.2f %.2f %.2f\n", s.headAngles.x, v.pitch, handName(h), p.x,
            p.y, p.z, r.x, r.y, r.z);
    }
}

// A weapon held in both hands, in three steps. The release (vr_flat_swap_2h_release_time): the free hand off the grip to
// the empty hand's guard in front of him (vr_flat_guard_*: the pose an empty hand stands in, off its shoulder in shares of
// the arm's reach; placed as at a level view, then carried into the view's frame with the rest). The pass
// (vr_flat_swap_2h_pass_time): the hand holding the weapon brings it across to that hand, vr_flat_swap_gap off it, and
// the swap is sent as it gets there. The attach (vr_flat_swap_2h_attach_time): once swapped, the hand that took it eases
// to its own hold, the gun turned as it was passed, and the hand that let go of it onto the gun's two-hand grip
// (flatsight's pin: the grip is free again from the swap on, gripBlocked).
void apply2h(hands::State& s, const Frame& v)
{
    const int H = sw.holder, F = 1 - H;
    const double t = vr_gametime - sw.startAt;
    if(sw.seen)
    {
        // The attach: from where the pass left them, each turned as the other was then, to its own pose.
        const float a = 1.f - geom::smooth01(static_cast<float>(vr_gametime - sw.peakAt) / attachTime());
        sw.share = a;
        for(int h : {HAND_OFF, HAND_MAIN})
        {
            s.pos[h] += (v.toWorld(sw.passLocal[h]) - s.pos[h]) * a;
            turnTo(s, h, v.rotToWorld(sw.passRot[1 - h]), a);
        }
        return;
    }

    // The empty hand's guard for the free hand, off its shoulder (as flatplay's rest pose places it at a level view),
    // turned up or down with the view about the head. Taken at the press, with the hands' places: the body's own moves
    // as the grip lets go (the sight's stance easing out) don't move it on the way.
    const Frame lv = levelFrame(s);
    const float side = F == HAND_OFF ? -1.f : 1.f;
    if(!sw.guardTaken)
    {
        const glm::vec3 guardLevel = flatplay::levelShoulderOf(s, F) + (lv.fwd * vr_flat_guard_fwd.value +
                                                                          lv.right * (side * vr_flat_guard_out.value) -
                                                                          lv.up * vr_flat_guard_down.value) *
                                                                             flatplay::armReach();
        sw.guardLocal = lv.toLocal(guardLevel);
        sw.guardTaken = true;
    }
    const glm::vec3 guard = v.toWorld(sw.guardLocal);
    const glm::vec3 guardRot =
        v.rotToWorld({za::clamp(vr_flat_guard_pitch.value, -90.f, 90.f), 0.f, side * vr_flat_guard_roll.value});
    const float release = geom::smooth01(static_cast<float>(t) / releaseTime());
    const float pass = sw.sent ? 1.f : geom::smooth01(static_cast<float>(t - releaseTime()) / passTime());
    sw.share = 0.5f * (release + pass);

    // The weapon's hand stays as it held the gun at the press while the other lets go (its own pose turns the gun the
    // moment the grip is gone: a one-handed hold), then brings it across.
    s.pos[H] = v.toWorld(sw.local[H]);
    s.rot[H] = v.rotToWorld(sw.rot[H]);
    s.pos[F] += (guard - s.pos[F]) * release;
    turnTo(s, F, guardRot, release);
    if(pass > 0.f)
    {
        const float gap = za::clamp(vr_flat_swap_gap.value, 0.f, 16.f);
        const glm::vec3 d = s.pos[H] - guard;
        const glm::vec3 way = glm::length(d) > 0.01f ? glm::normalize(d) : (H == HAND_MAIN ? v.right : -v.right);
        s.pos[H] += (guard + way * gap - s.pos[H]) * pass;
    }
    if(vr_flat_swap_debug.value && release >= 1.f && !sw.releasePrinted)
    {
        sw.releasePrinted = true;
        const glm::vec3 heldAt = v.toWorld(sw.local[H]);
        Con_Printf("flatswap: 2h: released (%.2f s): the %s hand %.1f units from the empty hand's guard, %.1f from the %s hand "
                   "(moved %.1f since the press)\n", t, handName(F), glm::distance(s.pos[F], guard), glm::distance(s.pos[F], s.pos[H]),
            handName(H), glm::distance(s.pos[H], heldAt));
        const glm::vec3 sh = lv.toLocal(flatplay::levelShoulderOf(s, F)), now = lv.toLocal(flatplay::shoulderOf(s, F));
        Con_Printf("flatswap: 2h: the guard off the %s shoulder at %.2f %.2f %.2f (a level look's; now %.2f %.2f %.2f), yaw %.1f, body %.1f\n",
            handName(F), sh.x, sh.y, sh.z, now.x, now.y, now.z, s.headAngles.y, s.bodyYaw);
    }
    // (Every frame until the swap arrives: where the attach starts from.)
    for(int h : {HAND_OFF, HAND_MAIN})
    {
        sw.passLocal[h] = v.toLocal(s.pos[h]);
        sw.passRot[h] = v.rotToLocal(s.rot[h]);
    }
    if(vr_flat_swap_debug.value && sw.sent && !sw.meetPrinted)
    {
        sw.meetPrinted = true;
        Con_Printf("flatswap: met: the hands %.1f units apart (the pass), the %s hand %.1f from the guard\n",
            glm::distance(s.pos[HAND_OFF], s.pos[HAND_MAIN]), handName(F), glm::distance(s.pos[F], guard));
        printInView(s, v);
    }
}

} // namespace

void apply(hands::State& s)
{
    if(!sw.running)
    {
        return;
    }
    const Frame v = viewFrame(s);
    if(!sw.captured)
    {
        for(int h : {HAND_OFF, HAND_MAIN})
        {
            sw.local[h] = v.toLocal(s.pos[h]);
            sw.rot[h] = v.rotToLocal(s.rot[h]);
        }
        sw.captured = true;
    }
    if(sw.twoHand)
    {
        apply2h(s, v);
        return;
    }
    const float a = shareNow();
    sw.share = a;
    if(a <= 0.f)
    {
        return;
    }
    glm::vec3 at[2];
    for(int h : {HAND_OFF, HAND_MAIN})
    {
        at[h] = v.toWorld(sw.local[h]);
    }
    const float gap = za::clamp(vr_flat_swap_gap.value, 0.f, 16.f);
    const glm::vec3 down = v.up * -za::clamp(vr_flat_swap_down.value, -16.f, 16.f);
    glm::vec3 meet[2];
    {
        // Between them across the body, as far forward and as high as the higher hand (the one with a weapon: an empty hand
        // hangs below the view, and halfway down to it they met out of sight).
        const glm::vec3& lo = sw.local[HAND_OFF];
        const glm::vec3& hi = sw.local[HAND_MAIN];
        const glm::vec3& top = lo.z > hi.z ? lo : hi;
        const glm::vec3 mid = v.toWorld({top.x, (lo.y + hi.y) * 0.5f, top.z}) + down;
        const glm::vec3 d = at[HAND_MAIN] - at[HAND_OFF];
        const glm::vec3 way = glm::length(d) > 0.01f ? glm::normalize(d) : v.right;
        meet[HAND_MAIN] = mid + way * (gap * 0.5f);
        meet[HAND_OFF] = mid - way * (gap * 0.5f);
    }
    if(vr_flat_swap_debug.value >= 2.f)
    {
        // (2: every frame, each hand's own pose and where the swap puts it, in the view's frame.)
        for(int h : {HAND_OFF, HAND_MAIN})
        {
            const glm::vec3 own = v.toLocal(s.pos[h]), put = v.toLocal(s.pos[h] + (meet[h] - s.pos[h]) * a);
            Con_Printf("flatswap: frame %.2f %s: own %.1f %.1f %.1f, drawn %.1f %.1f %.1f\n", a, handName(h), own.x, own.y, own.z,
                put.x, put.y, put.z);
        }
    }
    for(int h : {HAND_OFF, HAND_MAIN})
    {
        s.pos[h] += (meet[h] - s.pos[h]) * a;
        // On the way in each hand keeps its own turn; once swapped a hand that took a weapon is turned as the other was,
        // so the weapon keeps its turn as it changes hands, and eases into its new hand's pose on the way back. A hand
        // that took a carried thing (or nothing) keeps its own: the game fits the thing to that hand's own grip
        // (VR_Carry_FitToHand, mirrored for the left), and the other hand's turn put a brick out of sight under the view.
        const bool crossTurn = sw.seen && widIn(h) != WID_FIST;
        turnTo(s, h, v.rotToWorld(crossTurn ? sw.rot[1 - h] : sw.rot[h]), a);
    }
    if(vr_flat_swap_debug.value && a >= 0.999f && !sw.meetPrinted)
    {
        sw.meetPrinted = true;
        Con_Printf("flatswap: met: the hands %.1f units apart (were %.1f), off moved %.1f, main moved %.1f\n",
            glm::distance(s.pos[HAND_OFF], s.pos[HAND_MAIN]), glm::distance(at[HAND_OFF], at[HAND_MAIN]),
            glm::distance(s.pos[HAND_OFF], at[HAND_OFF]), glm::distance(s.pos[HAND_MAIN], at[HAND_MAIN]));
        printInView(s, v);
    }
}

bool busy()
{
    return sw.running;
}

bool meeting()
{
    return sw.running && !sw.twoHand;
}

bool gripBlocked()
{
    return sw.running && !sw.seen;
}

} // namespace qvr::flatswap
