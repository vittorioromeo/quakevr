// vr_flatgrenade.cpp -- flatscreen play's quick grenade as drawn (his words 2026-11-10, QC vr_flatgrenade.qc's header;
// docs/vr-port/FLATPLAY.md, "The quick grenade").
//
// The game decides (QC vr_flatgrenade.qc): G (vr_flat_key_grenade, sent here as impulse 57) picks the hand and runs the
// phases, which come back in STAT_QVR_FLATGREN (the phase + 8 for the main hand). This file draws them, each a share of
// the hand's way to a place off its shoulder, eased (smooth01) and laid over the pose in this order:
//   - hold: the grenade held out in view (vr_flat_grenade_hold_*, along the look), from the trip to the pouch until the
//     throw's follow-through is over; it eases back over vr_flat_grenade_back_time;
//   - wind: the overhead wind-up (vr_flat_grenade_wind_*, level with the body), while the fire button winds up the throw,
//     in vr_flat_grenade_wind_time; gone as the throw's follow-through comes;
//   - follow: the throw's follow-through (vr_flat_grenade_follow_*, along the look), in vr_flat_grenade_follow_time, then
//     back with the hold;
//   - away: the trip to the pouch (vr_flat_grenade_fetch_*, at his side and a little behind, level with the body, out of
//     view), over vr_flat_grenade_fetch_time; back in view with the grenade over vr_flat_grenade_up_time.
// The pose made is the game's too (the server sees the hand where it is drawn, and the grenade follows it). While a hand
// is busy with it, it doesn't punch, and the off hand lets go of a weapon's grip (vr_flatsight.cpp, vr_twohand.cpp),
// taking it again as it eases back. vr_flat_grenade_debug 1 prints the steps (flatgren: ...); vr_flatgrenade_report the state.

#include "vr_flatgrenade.hpp"

#include "vr_api.h"
#include "vr_backend.hpp" // HAND_OFF / HAND_MAIN
#include "vr_cvars.hpp"
#include "vr_tunables.hpp"
#include "vr_engine.hpp"
#include "vr_flatplay.hpp"
#include "vr_geom.hpp"
#include "vr_protocol.hpp"

#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "vr_zancle.hpp"

extern "C" qboolean keydown[MAX_KEYS]; // keys.c
extern "C" int Key_StringToKeynum(const char* str); // keys.c

namespace qvr::flatgrenade
{

namespace
{

// The game's phases (QC VR_FLATGREN_*).
constexpr int phaseFetch = 1;
constexpr int phaseHeld = 2; // (3: armed, held the same way)
constexpr int phaseWind = 4;
constexpr int phaseThrown = 5;

// Each hand's shares (0..1, before their ease).
struct Shares
{
    float away{0.f};
    float hold{0.f};
    float wind{0.f};
    float follow{0.f};
};

// The grenade as drawn (no function-local statics: a named file-scope struct, CODE_STYLE.md).
struct GrenadeState
{
    Shares hand[2];
    int phase{0};        // the server's, this frame
    int hand0{-1};       // its hand (kept after the phase ends, while the hand eases back)
    bool keyDown{false}; // G, last frame
    int printed{0};      // (vr_flat_grenade_debug) the places whose arrival was printed this grenade: bit per place
    int presses{0};
    double lastTime{-1e9};
};

GrenadeState gs;

[[nodiscard]] float secs(const Tunable& t)
{
    return za::clamp(t.value, 0.02f, 2.f);
}

[[nodiscard]] const char* handName(int h)
{
    return h == HAND_MAIN ? "main" : "off";
}

[[nodiscard]] bool anyShare(const Shares& a)
{
    return a.away > 0.f || a.hold > 0.f || a.wind > 0.f || a.follow > 0.f;
}

// `v` towards `want` (0 or 1) at 1 / `up` a second rising and 1 / `down` falling.
void step(float& v, bool want, float up, float down, float dt)
{
    v = want ? za::min(1.f, v + dt / up) : za::max(0.f, v - dt / down);
}

void report_f()
{
    Con_Printf("flatgren: phase %d, hand %s; off: away %.2f hold %.2f wind %.2f follow %.2f; main: away %.2f hold %.2f wind %.2f "
               "follow %.2f; G presses %d\n",
        gs.phase, gs.hand0 < 0 ? "none" : handName(gs.hand0), gs.hand[HAND_OFF].away, gs.hand[HAND_OFF].hold,
        gs.hand[HAND_OFF].wind, gs.hand[HAND_OFF].follow, gs.hand[HAND_MAIN].away, gs.hand[HAND_MAIN].hold,
        gs.hand[HAND_MAIN].wind, gs.hand[HAND_MAIN].follow, gs.presses);
}

// A place off the shoulder `sh`: `fwd` along `f`, `out` along the hand's own side of `r`, `up` along `u`, in shares of the
// arm's reach `span`.
[[nodiscard]] glm::vec3 place(const glm::vec3& sh, const glm::vec3& f, const glm::vec3& r, const glm::vec3& u, float side,
    float span, float fwd, float out, float up)
{
    return sh + (f * fwd + r * (side * out) + u * up) * span;
}

void blend(hands::State& s, int h, const glm::vec3& pos, const glm::vec3& rot, float a)
{
    if(a <= 0.f)
    {
        return;
    }
    s.pos[h] += (pos - s.pos[h]) * a;
    s.rot[h].x += geom::yawDelta(rot.x, s.rot[h].x) * a;
    s.rot[h].y += geom::yawDelta(rot.y, s.rot[h].y) * a;
    s.rot[h].z += geom::yawDelta(rot.z, s.rot[h].z) * a;
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_flatgrenade_report", report_f);
}

void frame(bool on)
{
    double dt = gs.lastTime > -1e8 ? vr_gametime - gs.lastTime : 0.0;
    gs.lastTime = vr_gametime;
    dt = za::clamp(dt, 0.0, 0.1);
    if(!on)
    {
        const int presses = gs.presses;
        gs = GrenadeState{};
        gs.presses = presses;
        return;
    }

    // G: the game picks the hand and runs it (QC VR_FlatGrenade_Key).
    const int k = vr_flat_key_grenade.string[0] ? Key_StringToKeynum(vr_flat_key_grenade.string) : -1;
    const bool down = key_dest == key_game && k > 0 && k < MAX_KEYS && keydown[k] != 0;
    if(down && !gs.keyDown)
    {
        gs.presses++;
        Cbuf_InsertText("impulse 57\n"); // (InsertText: now, not after what is queued)
        if(vr_flat_grenade_debug.value)
        {
            Con_Printf("flatgren: %s: quick grenade\n", vr_flat_key_grenade.string);
        }
    }
    gs.keyDown = down;

    const int stat = cl.stats[protocol::STAT_QVR_FLATGREN];
    const int phase = stat & 7;
    if(phase != gs.phase && vr_flat_grenade_debug.value)
    {
        Con_Printf("flatgren: phase %d -> %d (%s hand)\n", gs.phase, phase, (stat & 8) ? "main" : "off");
    }
    gs.phase = phase;
    if(phase != 0)
    {
        gs.hand0 = (stat & 8) ? HAND_MAIN : HAND_OFF;
    }
    if(phase == phaseFetch)
    {
        gs.printed = 0;
    }
    const float t = static_cast<float>(dt);
    if(t <= 0.f)
    {
        return;
    }
    for(int h : {HAND_OFF, HAND_MAIN})
    {
        Shares& a = gs.hand[h];
        const bool mine = phase != 0 && h == gs.hand0;
        const float back = secs(vr_flat_grenade_back_time);
        const float follow = secs(vr_flat_grenade_follow_time);
        step(a.away, mine && phase == phaseFetch, secs(vr_flat_grenade_fetch_time), secs(vr_flat_grenade_up_time), t);
        step(a.hold, mine, secs(vr_flat_grenade_fetch_time), back, t);
        step(a.wind, mine && phase == phaseWind, secs(vr_flat_grenade_wind_time), follow, t);
        step(a.follow, mine && phase == phaseThrown, follow, back, t);
    }
}

void apply(hands::State& s)
{
    for(int h : {HAND_OFF, HAND_MAIN})
    {
        const Shares& a = gs.hand[h];
        if(!anyShare(a))
        {
            continue;
        }
        const glm::vec3 sh = flatplay::shoulderOf(s, h);
        const float span = flatplay::armReach();
        // The look, pitched no further than the hands follow it (vr_flat_pose_pitch), and the body's level frame.
        const float most = za::clamp(vr_flat_pose_pitch.value, 0.f, 90.f);
        const glm::vec3 look{za::clamp(s.headAngles.x, -most, most), s.headAngles.y, 0.f};
        glm::vec3 vf, vr, vu, lf, lr, lu;
        hands::angleVectors(look, vf, vr, vu);
        hands::angleVectors({0.f, look.y, 0.f}, lf, lr, lu);
        const float side = glm::dot(sh - s.head, lr) >= 0.f ? 1.f : -1.f; // (the hand's own side, whichever is main)

        const glm::vec3 holdAt = place(sh, vf, vr, vu, side, span, vr_flat_grenade_hold_fwd.value, vr_flat_grenade_hold_out.value,
            -vr_flat_grenade_hold_down.value);
        const glm::vec3 windAt = place(sh, lf, lr, lu, side, span, vr_flat_grenade_wind_fwd.value, vr_flat_grenade_wind_out.value,
            vr_flat_grenade_wind_up.value);
        const glm::vec3 followAt = place(sh, vf, vr, vu, side, span, vr_flat_grenade_follow_fwd.value,
            vr_flat_grenade_follow_out.value, -vr_flat_grenade_follow_down.value);
        const glm::vec3 fetchAt = place(sh, lf, lr, lu, side, span, vr_flat_grenade_fetch_fwd.value, vr_flat_grenade_fetch_out.value,
            -vr_flat_grenade_fetch_down.value);
        const auto pitch = [](const Tunable& t) { return za::clamp(t.value, -90.f, 90.f); };

        blend(s, h, holdAt, {look.x + pitch(vr_flat_grenade_hold_pitch), look.y, 0.f}, geom::smooth01(a.hold));
        blend(s, h, windAt, {pitch(vr_flat_grenade_wind_pitch), look.y, 0.f}, geom::smooth01(a.wind));
        blend(s, h, followAt, {look.x + pitch(vr_flat_grenade_follow_pitch), look.y, 0.f}, geom::smooth01(a.follow));
        blend(s, h, fetchAt, {pitch(vr_flat_grenade_fetch_pitch), look.y, 0.f}, geom::smooth01(a.away));

        // (vr_flat_grenade_debug: where the hand is as each place is reached, off the head along the look: the tests.)
        if(vr_flat_grenade_debug.value)
        {
            const float full[4] = {a.away, a.hold * (1.f - a.away) * (1.f - a.wind) * (1.f - a.follow), a.wind, a.follow};
            const char* names[4] = {"the pouch", "held", "overhead", "the follow-through"};
            for(int i = 0; i < 4; i++)
            {
                const int bit = 1 << i;
                if(full[i] >= 1.f && !(gs.printed & bit))
                {
                    gs.printed |= bit;
                    const glm::vec3 d = s.pos[h] - s.head;
                    Con_Printf("flatgren: %s hand at %s: %.1f forward, %.1f right, %.1f up of the eye, pitch %.0f\n", handName(h),
                        names[i], glm::dot(d, vf), glm::dot(d, vr), glm::dot(d, vu), s.rot[h].x);
                }
            }
        }
    }
}

int heldHand()
{
    return gs.phase >= phaseHeld && gs.phase <= phaseWind ? gs.hand0 : -1;
}

bool busy(int h)
{
    // (The phases only: on the way back after the throw the hand may take the grip again, so it eases from the
    // follow-through straight onto it.)
    return h >= 0 && h < 2 && gs.phase != 0 && gs.hand0 == h;
}

} // namespace qvr::flatgrenade
