// vr_moveguard.cpp -- see vr_moveguard.hpp.

#include "vr_moveguard.hpp"

#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_mpavatar.hpp"
#include "vr_testkit.hpp"
#include "vr_units.hpp"

#include "Zancle/Base/IsFinite.hpp"
#include "Zancle/Base/Limits.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sqrt.hpp"

namespace qvr::moveguard
{
namespace
{

// The server's per client counts and room-scale budgets (the main thread's: the server's frame).
struct Slot
{
    Counts counts;
    float walkLeft{-1.f}; // metres of room-scale walk saved up (-1: full, a new player)
};
Slot slots[MAX_SCOREBOARD];

[[nodiscard]] bool finite(const glm::vec3& v)
{
    return ZA_ISFINITE(v.x) && ZA_ISFINITE(v.y) && ZA_ISFINITE(v.z);
}

// Place `p` (world) within `most` of `base`; a place at the world's origin is none (a hand the client had no pose for,
// a lamp unlit, a magazine absent: kept as it is).
void place(glm::vec3& p, const glm::vec3& base, float most, int& held)
{
    if(p != glm::vec3{0.f})
    {
        p = base + within(p - base, most, held);
    }
}

[[nodiscard]] float sizeOf(const glm::vec3& v)
{
    return glm::length(v);
}

[[nodiscard]] float sizeOf(float v)
{
    return za::abs(v);
}

} // namespace

float unitsPerMetre(float worldScale)
{
    return units::perMetre * za::max(ZA_ISFINITE(worldScale) ? worldScale : 0.f, scaleFloor);
}

glm::vec3 within(const glm::vec3& v, float most, int& held)
{
    if(!finite(v))
    {
        held++;
        return glm::vec3{0.f};
    }
    const float len = glm::length(v);
    if(len <= most)
    {
        return v;
    }
    held++;
    return v * (most / len);
}

float within(float v, float most, int& held)
{
    if(!ZA_ISFINITE(v))
    {
        held++;
        return 0.f;
    }
    if(za::abs(v) <= most)
    {
        return v;
    }
    held++;
    return v < 0.f ? -most : most;
}

glm::vec3 relative(const glm::vec3& p, const glm::vec3& origin, float upm)
{
    int held = 0;
    return p == glm::vec3{0.f} ? p - origin : within(p - origin, reachMetres * upm, held);
}

glm::vec3 speed(const glm::vec3& v)
{
    int held = 0;
    return within(v, speedMost, held);
}

Held guard(VrMove& m, const glm::vec3& base, float upm, int slot)
{
    Held h;
    const float reach = reachMetres * upm;
    // (vr_mp_guard_debug: each held value named, its size and its bound; under 1, also each over that share of it)
    const float share = vr_mp_guard_debug.value;
    const auto told = [&](const char* what, int i, int before, int now, float size, float most) {
        if(share > 0.f && (now != before || size > most * share))
        {
            Con_Printf("mp_guard_%s: slot=%d %s[%d] %.3f of %.3f\n", now != before ? "held" : "near", slot, what, i,
                size, most);
        }
    };
    const auto spot = [&](glm::vec3& p, const char* what, int i) {
        const int before = h.places;
        const float size = p == glm::vec3{0.f} ? 0.f : glm::length(p - base);
        place(p, base, reach, h.places);
        told(what, i, before, h.places, size, reach);
    };
    const auto fast = [&](auto& v, float most, const char* what, int i) {
        const int before = h.speeds;
        const float size = sizeOf(v);
        v = within(v, most, h.speeds);
        told(what, i, before, h.speeds, size, most);
    };
    for(int i = 0; i < 2; i++)
    {
        VrHandMove& hand = m.hands[i];
        spot(hand.pos, "pos", i);
        spot(hand.tracked, "tracked", i);
        fast(hand.vel, speedMost, "vel", i);
        fast(hand.ownVel, speedMost, "ownVel", i);
        fast(hand.throwVel, speedMost, "throwVel", i);
        fast(hand.throwFlick, speedMost, "throwFlick", i);
        fast(hand.velMag, speedMost, "velMag", i);
        fast(hand.angVel, spinMost, "angVel", i);
        // Where a throw left the hand: QC's back-projection starts it there (at most 0.3 s ago: vr_carry.qc
        // VR_Throw_BackProject), so within reach and as far as he can have moved since. Held, not counted: it comes from
        // the hand's history, which an honest player's spawn or teleport leaves far behind him for a moment.
        const float age = ZA_ISFINITE(hand.throwAge) ? za::clamp(hand.throwAge, 0.f, 0.3f) : 0.3f;
        int uncounted = 0;
        place(hand.throwPos, base, reach + age * za::max(sv_maxvelocity.value, 0.f), uncounted);
    }
    fast(m.headVel, speedMost, "headVel", 0);
    spot(m.headPos, "headPos", 0);
    spot(m.lampLens, "lampLens", 0);
    for(int i = 0; i < 2; i++)
    {
        spot(m.muzzlePos[i], "muzzlePos", i);
        spot(m.loadPort[i], "loadPort", i);
        m.launchOfs[i] = within(m.launchOfs[i], launchMetres * upm, h.places);
        if(m.magBox[i][1] != glm::vec3{0.f}) // (none: all zero)
        {
            place(m.magBox[i][0], base, reach, h.places);
            for(int k = 1; k < 4; k++)
            {
                m.magBox[i][k] = within(m.magBox[i][k], boxMetres * upm, h.places);
            }
        }
    }
    // (Not the hands and holsters as drawn, nor the ammo pouch: the other players' drawing of him, and places no hand
    // reaches past its own bound -- the last frame's view, a frame behind his origin at a spawn or a teleport.)
    return h;
}

float slotScale(int slot)
{
    float scale = 0.f;
    return mpavatar::serverWorldScale(slot, scale) ? scale : scaleFloor;
}

void roomscale(int slot, float v[3], double dt, float upm)
{
    if(slot < 0 || slot >= MAX_SCOREBOARD || !(dt > 0.0))
    {
        return;
    }
    Slot& s = slots[slot];
    if(budget(s.walkLeft, v, dt, upm))
    {
        s.counts.walks++;
    }
}

bool budget(float& walkLeft, float v[3], double dt, float upm)
{
    if(!(dt > 0.0))
    {
        return false;
    }
    const float burst = walkBurst * upm;
    walkLeft = walkLeft < 0.f ? burst : za::min(burst, walkLeft + walkMost * upm * static_cast<float>(dt));
    if(!ZA_ISFINITE(v[0]) || !ZA_ISFINITE(v[1]))
    {
        v[0] = v[1] = 0.f;
        return true;
    }
    const float wanted = za::sqrt(v[0] * v[0] + v[1] * v[1]) * static_cast<float>(dt); // (units this move)
    if(wanted <= walkLeft)
    {
        walkLeft -= wanted;
        return false;
    }
    const float k = walkLeft / wanted;
    v[0] *= k;
    v[1] *= k;
    walkLeft = 0.f;
    return true;
}

float walkLeft(int slot)
{
    return slot >= 0 && slot < MAX_SCOREBOARD ? slots[slot].walkLeft : -1.f;
}

void count(int slot, const Held& h)
{
    if(slot >= 0 && slot < MAX_SCOREBOARD)
    {
        Counts& c = slots[slot].counts;
        c.moves++;
        c.places += h.places;
        c.speeds += h.speeds;
    }
}

void countFist(int slot, int n)
{
    if(slot >= 0 && slot < MAX_SCOREBOARD)
    {
        slots[slot].counts.fists += n;
    }
}

void countSpot(int slot)
{
    if(slot >= 0 && slot < MAX_SCOREBOARD)
    {
        slots[slot].counts.spots++;
    }
}

void countRewind(int slot)
{
    if(slot >= 0 && slot < MAX_SCOREBOARD)
    {
        slots[slot].counts.rewinds++;
    }
}

Counts counts(int slot)
{
    return slot >= 0 && slot < MAX_SCOREBOARD ? slots[slot].counts : Counts{};
}

void serverReset(int slot)
{
    if(slot >= 0 && slot < MAX_SCOREBOARD)
    {
        slots[slot] = Slot{};
    }
}

void cheatForTest(int bits, VrMove& m, float& time, float& viewBack)
{
    const glm::vec3 ahead{2000.f, 0.f, 0.f};
    if(bits & 1)
    {
        m.hands[1].pos = m.origin + ahead;
        m.hands[1].tracked = m.hands[1].pos;
        m.muzzlePos[1] = m.origin + ahead;
        m.loadPort[1] = m.origin + ahead;
    }
    if(bits & 2)
    {
        for(VrHandMove& h : m.hands)
        {
            h.vel = h.throwVel = glm::vec3{500.f, 0.f, 0.f};
            h.velMag = 500.f;
        }
    }
    if(bits & 4)
    {
        m.roomscaleMove = glm::vec3{20000.f, 0.f, 0.f};
    }
    if(bits & 8)
    {
        viewBack = 1.f;
        time -= 2.f;
    }
}

namespace
{

// vr_moveguard_selftest: made-up moves through the bounds -- an honest one (a tall player's hands up, a long two-handed
// weapon's muzzle at arm's length, a fast real throw) untouched; a cheat's (a hand, a muzzle across the map, a
// supersonic throw, NaNs) held at the bounds, counted; the room-scale budget: a walk at a walking head's pace never cut,
// a speed hack's cut to it.
void selftest_f()
{
    testkit::Tally t;
    const auto check = [&t](bool ok, const char* what) { t.check(ok, "moveguard", what); };
    const float upm = unitsPerMetre(1.2f);
    check(za::abs(upm - units::perMetre * scaleFloor) < 1e-3f, "a world scale under the floor: the floor's units");
    check(za::abs(unitsPerMetre(3.f) - units::perMetre * 3.f) < 1e-3f, "a giant's (world scale 3): his own");
    const glm::vec3 base{100.f, 200.f, 24.f};
    const float m = units::perMetre * 1.2f; // (his places: at the default world scale)

    VrMove honest;
    honest.origin = base;
    honest.headPos = base + glm::vec3{8.f, 0.f, 1.2f * m};                      // a 2.1 m player's eyes, leaning
    honest.hands[1].pos = honest.headPos + glm::vec3{0.f, 0.f, 0.8f * m};       // his arm straight up
    honest.hands[1].tracked = honest.hands[1].pos;
    honest.muzzlePos[1] = honest.hands[1].pos + glm::vec3{0.f, 0.f, 1.2f * m}; // a rifle pointed up from it
    honest.loadPort[1] = honest.hands[1].pos + glm::vec3{0.f, 0.f, 0.3f * m};
    honest.hands[0].pos = base + glm::vec3{0.9f * m, 0.f, 0.f};
    honest.hands[1].vel = glm::vec3{25.f, 0.f, 0.f};                              // a hard real throw
    honest.hands[1].throwVel = glm::vec3{0.f, 28.f, 3.f};
    honest.hands[1].velMag = 25.f;
    honest.hands[1].angVel = glm::vec3{0.f, 0.f, 60.f};
    honest.hands[1].throwPos = honest.hands[1].pos;
    honest.hands[1].throwAge = 0.05f;
    honest.magBox[1][0] = honest.hands[1].pos;
    honest.magBox[1][1] = glm::vec3{0.1f * m, 0.f, 0.f};
    VrMove h = honest;
    Held held = guard(h, base, upm);
    check(held.places == 0 && held.speeds == 0, "an honest move: nothing held");
    check(h.muzzlePos[1] == honest.muzzlePos[1] && h.hands[1].pos == honest.hands[1].pos &&
              h.hands[1].throwVel == honest.hands[1].throwVel,
        "an honest move: its places and speeds as sent");

    VrMove cheat = honest;
    cheat.hands[1].pos = base + glm::vec3{5000.f, 0.f, 0.f};
    cheat.muzzlePos[1] = base + glm::vec3{0.f, -3000.f, 0.f};
    cheat.loadPort[0] = base + glm::vec3{0.f, 0.f, 900.f};
    cheat.hands[1].throwVel = glm::vec3{1000.f, 0.f, 0.f};
    cheat.hands[1].velMag = 1e6f;
    cheat.hands[0].vel = glm::vec3{ZA_FLOAT_NAN, 0.f, 0.f};
    cheat.launchOfs[1] = glm::vec3{4000.f, 0.f, 0.f};
    cheat.magBox[1][1] = glm::vec3{500.f, 0.f, 0.f};
    held = guard(cheat, base, upm);
    const float reach = reachMetres * upm;
    check(held.places == 5 && held.speeds == 3, va("a cheat's move: 5 places and 3 speeds held (%d, %d)", held.places,
                                                    held.speeds));
    check(za::abs(glm::length(cheat.hands[1].pos - base) - reach) < 1e-2f, "a hand across the map: at the reach");
    check(za::abs(glm::length(cheat.muzzlePos[1] - base) - reach) < 1e-2f && cheat.muzzlePos[1].y < base.y,
        "a muzzle across the map: at the reach, the way it was sent");
    check(za::abs(glm::length(cheat.hands[1].throwVel) - speedMost) < 1e-3f, "a supersonic throw: a real arm's");
    check(cheat.hands[1].velMag == speedMost && cheat.hands[0].vel == glm::vec3{0.f}, "a speed's size held, a NaN zero");
    check(za::abs(glm::length(cheat.launchOfs[1]) - launchMetres * upm) < 1e-2f, "a launch offset held");
    check(za::abs(glm::length(cheat.magBox[1][1]) - boxMetres * upm) < 1e-2f, "a magazine's half-axis held");

    VrMove none; // (no pose yet: all zero)
    held = guard(none, base, upm);
    check(held.places == 0 && held.speeds == 0 && none.hands[0].pos == glm::vec3{0.f}, "a move with no pose: untouched");

    VrMove late = honest; // (a throw's place far off: held to his reach and 0.3 s of his fastest, not counted)
    late.hands[1].throwPos = base + glm::vec3{5000.f, 0.f, 0.f};
    late.hands[1].throwAge = 6.f;
    held = guard(late, base, upm);
    check(held.places == 0 && za::abs(glm::length(late.hands[1].throwPos - base) - (reach + 0.3f * sv_maxvelocity.value)) < 0.1f,
        "a throw's place far off: held, not counted");

    {
        VrMove far = honest;
        far.hands[1].pos = base + glm::vec3{0.f, 0.f, 10000.f};
        const glm::vec3 replayed = relative(far.hands[1].pos, base, upm);
        (void)guard(far, base, upm);
        // (the server's field from its base: the same place to a float's rounding; the walk's input is inputOf's on both)
        check(glm::length(replayed - (far.hands[1].pos - base)) < 1e-3f &&
                  relative(honest.hands[0].pos, base, upm) == honest.hands[0].pos - base,
            "the prediction's place: the server's, held or not");
    }
    check(speed(glm::vec3{0.f, 100.f, 0.f}) == glm::vec3{0.f, speedMost, 0.f}, "the prediction's speed: as the server's");

    // The room-scale budget (a slot of its own, put back after).
    const int slot = MAX_SCOREBOARD - 1;
    const Slot kept = slots[slot];
    slots[slot] = Slot{};
    const double dt = 1.0 / 72.0;
    bool cut = false;
    for(int i = 0; i < 72 * 10; i++) // 10 s at 3 m/s, a recentring's 14 units every second
    {
        float v[3]{3.f * upm, 0.f, 0.f};
        if(i % 72 == 0)
        {
            v[0] += 14.f / static_cast<float>(dt);
        }
        const float was = v[0];
        roomscale(slot, v, dt, upm);
        cut = cut || v[0] != was;
    }
    check(!cut && slots[slot].counts.walks == 0, "a walk at a walking head's pace, recentred: never cut");
    float gone = 0.f;
    for(int i = 0; i < 72; i++) // 1 s of a 100 m/s hack
    {
        float v[3]{100.f * upm, 0.f, 0.f};
        roomscale(slot, v, dt, upm);
        gone += v[0] * static_cast<float>(dt);
    }
    check(gone <= (walkMost + walkBurst) * upm + 1.f && slots[slot].counts.walks > 0,
        va("a speed hack: cut to the budget (%.0f units in 1 s)", gone));
    slots[slot] = kept;

    t.verdict("moveguard");
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_moveguard_selftest", selftest_f);
}

} // namespace qvr::moveguard
