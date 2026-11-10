// vr_flatswap.cpp -- flatscreen play's hand swap as drawn (his words 2026-11-10: "Give the hand swap button ('3') a short
// animation. When dual wielding or holding a weapon with one hand only, both hands move close to each other, perform the
// swap, then go back to their positions. When holding a single weapon with two hands, the off-hand detaches, moves closer
// to the main hand, the swap is performed, and the hands then go to their rightful positions."; FLATPLAY.md, "The hand swap").
//
// The game swaps (QC vr_wpnholster.qc VR_FlatSwapHands, impulse 48), as before; this file only times it and draws the hands.
// The key's press (vr_flat_key_swap) starts the trip: over vr_flat_swap_in seconds the hands go to their meeting point --
// each half of vr_flat_swap_gap off the middle of the two, vr_flat_swap_down below it; a weapon held in both hands: the
// free hand off its grip to vr_flat_swap_gap off the hand holding the weapon, which stays -- and the swap is sent as they
// meet. Once the hands' weapons have changed (the server's answer), each hand turned the way the other was (the weapon
// keeps its turn as it changes hands), they ease back over vr_flat_swap_out to their own poses: the free hand onto the
// weapon's grip again (vr_flatsight.cpp, vr_twohand.cpp), a weapon in the hand that took it.
//
// The places are kept in the body's level frame off the head (taken on the first pose after the press), so a turn of the
// view carries the hands with it. The fire buttons do nothing while it runs (flatplay::routeFire). vr_flat_swap_anim 0:
// the swap at once, as before. vr_flat_swap_debug 1 prints the steps (flatswap: ...); vr_flatswap_report the state.

#include "vr_flatswap.hpp"

#include "vr_api.h"
#include "vr_backend.hpp" // HAND_OFF / HAND_MAIN
#include "vr_cvars.hpp"
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
    double startAt{-1e9};
    double sentAt{-1e9};
    double peakAt{-1e9};     // when the swap was seen: the way back starts
    bool captured{false};    // the hands' places taken (the first pose after the press)
    glm::vec3 local[2]{glm::vec3{0.f}, glm::vec3{0.f}}; // each hand then, off the head in the level frame (fwd, right, up)
    glm::vec3 rot[2]{glm::vec3{0.f}, glm::vec3{0.f}};   // and its angles, the yaw against the view's
    float share{0.f};        // the last pose's share of the way to the meeting point (the report)
    bool meetPrinted{false}; // (vr_flat_swap_debug) the meeting point's line printed
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

[[nodiscard]] const char* handName(int h)
{
    return h == HAND_MAIN ? "main" : "off";
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
        return geom::smooth01(static_cast<float>(vr_gametime - sw.startAt) / inTime());
    }
    return 1.f - geom::smooth01(static_cast<float>(vr_gametime - sw.peakAt) / outTime());
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

void level(const hands::State& s, glm::vec3& fwd, glm::vec3& right, glm::vec3& up)
{
    hands::angleVectors({0.f, s.headAngles.y, 0.f}, fwd, right, up);
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
    const bool nothing = wo == WID_FIST && wm == WID_FIST;
    const bool carrying = held::heldEntity(HAND_OFF) != 0 || held::heldEntity(HAND_MAIN) != 0;
    const bool grenade = flatgrenade::busy(HAND_OFF) || flatgrenade::busy(HAND_MAIN);
    if(nothing || carrying || grenade || flatreload::busy(HAND_OFF) || flatreload::busy(HAND_MAIN))
    {
        if(vr_flat_swap_debug.value)
        {
            Con_Printf("flatswap: not drawn (%s): the swap at once\n",
                nothing ? "both hands empty" : carrying ? "a hand carries a thing" : grenade ? "the quick grenade" : "a reload under way");
        }
        return false;
    }
    reset();
    sw.running = true;
    sw.startAt = vr_gametime;
    sw.widWas[HAND_OFF] = wo;
    sw.widWas[HAND_MAIN] = wm;
    const int lone = flatplay::loneWeaponHand();
    sw.twoHand = lone >= 0 && twohand::helping(1 - lone);
    sw.holder = lone >= 0 ? lone : HAND_MAIN;
    if(vr_flat_swap_debug.value)
    {
        Con_Printf("flatswap: start (%s; off %d, main %d)\n",
            sw.twoHand ? "a weapon in both hands: the free hand to it" : "the hands to each other", wo, wm);
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
    if(!sw.sent && now - sw.startAt >= static_cast<double>(inTime()) - 1e-4)
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
        const bool changed = widIn(HAND_OFF) != sw.widWas[HAND_OFF] || widIn(HAND_MAIN) != sw.widWas[HAND_MAIN];
        // (Two of the same weapon swap with no change to see: the wait's end is the answer then.)
        const bool same = sw.widWas[HAND_OFF] == sw.widWas[HAND_MAIN];
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
    if(sw.seen && now - sw.peakAt >= static_cast<double>(outTime()))
    {
        sw.running = false;
        sw.share = 0.f;
        if(vr_flat_swap_debug.value)
        {
            Con_Printf("flatswap: done (%.2f s)\n", now - sw.startAt);
        }
    }
}

void apply(hands::State& s)
{
    if(!sw.running)
    {
        return;
    }
    glm::vec3 fwd, right, up;
    level(s, fwd, right, up);
    const float yaw = s.headAngles.y;
    if(!sw.captured)
    {
        for(int h : {HAND_OFF, HAND_MAIN})
        {
            const glm::vec3 d = s.pos[h] - s.head;
            sw.local[h] = {glm::dot(d, fwd), glm::dot(d, right), glm::dot(d, up)};
            sw.rot[h] = {s.rot[h].x, geom::yawDelta(s.rot[h].y, yaw), s.rot[h].z};
        }
        sw.captured = true;
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
        at[h] = s.head + fwd * sw.local[h].x + right * sw.local[h].y + up * sw.local[h].z;
    }
    const float gap = za::clamp(vr_flat_swap_gap.value, 0.f, 16.f);
    const glm::vec3 down = up * -za::clamp(vr_flat_swap_down.value, -16.f, 16.f);
    glm::vec3 meet[2];
    if(sw.twoHand)
    {
        // The free hand off its grip to the hand holding the weapon, which stays where it is.
        const int H = sw.holder, F = 1 - H;
        const glm::vec3 d = at[F] - at[H];
        const glm::vec3 way = glm::length(d) > 0.01f ? glm::normalize(d) : (F == HAND_OFF ? -right : right);
        meet[H] = at[H];
        meet[F] = at[H] + way * gap + down;
    }
    else
    {
        // Between them across the body, as far forward and as high as the higher hand (the one with a weapon: an empty hand
        // hangs below the view, and halfway down to it they met out of sight).
        const glm::vec3& lo = sw.local[HAND_OFF];
        const glm::vec3& hi = sw.local[HAND_MAIN];
        const glm::vec3& top = lo.z > hi.z ? lo : hi;
        const glm::vec3 mid = s.head + fwd * top.x + right * ((lo.y + hi.y) * 0.5f) + up * top.z + down;
        const glm::vec3 d = at[HAND_MAIN] - at[HAND_OFF];
        const glm::vec3 way = glm::length(d) > 0.01f ? glm::normalize(d) : right;
        meet[HAND_MAIN] = mid + way * (gap * 0.5f);
        meet[HAND_OFF] = mid - way * (gap * 0.5f);
    }
    for(int h : {HAND_OFF, HAND_MAIN})
    {
        s.pos[h] += (meet[h] - s.pos[h]) * a;
        // On the way in each hand keeps its own turn; once swapped each is turned as the other was, so the weapon keeps
        // its turn as it changes hands, and eases into its new hand's pose on the way back.
        const glm::vec3& r = sw.seen ? sw.rot[1 - h] : sw.rot[h];
        const glm::vec3 want{r.x, r.y + yaw, r.z};
        s.rot[h].x += geom::yawDelta(want.x, s.rot[h].x) * a;
        s.rot[h].y += geom::yawDelta(want.y, s.rot[h].y) * a;
        s.rot[h].z += geom::yawDelta(want.z, s.rot[h].z) * a;
    }
    if(vr_flat_swap_debug.value && a >= 0.999f && !sw.meetPrinted)
    {
        sw.meetPrinted = true;
        Con_Printf("flatswap: met: the hands %.1f units apart (were %.1f), off moved %.1f, main moved %.1f\n",
            glm::distance(s.pos[HAND_OFF], s.pos[HAND_MAIN]), glm::distance(at[HAND_OFF], at[HAND_MAIN]),
            glm::distance(s.pos[HAND_OFF], at[HAND_OFF]), glm::distance(s.pos[HAND_MAIN], at[HAND_MAIN]));
    }
}

bool busy()
{
    return sw.running;
}

bool gripBlocked()
{
    return sw.running && !sw.seen;
}

} // namespace qvr::flatswap
