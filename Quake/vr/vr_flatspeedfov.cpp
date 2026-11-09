// vr_flatspeedfov.cpp -- flatscreen play's speed FOV (vr_flatspeedfov.hpp; docs/vr-port/FLATPLAY.md, "The speed FOV").
//
// The speed is the client's velocity along the floor (cl.velocity: a slide's, a bunny hop's in the air), so it is the
// same hosting a game or joined to one. The multiplier is applied in flatsight::fov after the ironsight's narrowing, to
// r_refdef.basefov (gl_screen.c SCR_CalcRefdef through VR_FlatAdsFov): the projection, the visor crosshair (which projects
// with the frame's own view-projection) and vr_flat_model_fov (its scale is taken against basefov, so the player's own
// models keep their size) all follow it. The look's speed does not: in_sdl.c IN_FovScale reads VR_FlatLookFov, the field
// of view without the widening, so a degree of mouse is a degree of turn at any speed.
//
// vr_flat_speed_fov_test (maps/vrtesthall.bsp, open level ground ahead): walk, run (the view at its own field of view),
// slide (it widens), the ease back to it standing, then a bunny hop chain (strafe-jumping, the view turned with the speed:
// it widens); the speeds and fields of view measured, no frame's step a pop. `vr_flat_speed_fov_test shots` takes a
// screenshot running, early in the slide, at its widest and eased back.

#include "vr_flatspeedfov.hpp"

#include "vr_api.h"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_flatmove.hpp"
#include "vr_flatplay.hpp"
#include "vr_geom.hpp"
#include "vr_testkit.hpp"
#include "vr_zancle.hpp"

#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sqrt.hpp"

#include <glm/trigonometric.hpp>

extern "C" cvar_t scr_fov; // gl_screen.c

namespace qvr::flatspeedfov
{
namespace
{

constexpr float maxMultiplier = 1.5f;
constexpr float mostFov = 170.f;
constexpr float testPhaseSeconds = 12.f; // a test step longer than this has failed (a safety net, never its end)
constexpr int steadyFrames = 36;         // half a second of fixed frames at one speed: walking or running steadily
constexpr int bhopHops = 8;              // the bunny hop chain's jumps

struct State
{
    float mult{1.f};        // eased
    float target{1.f};      // the speed's own
    float speed{0.f};       // along the floor, this frame
    double lastTime{-1e9};
    float widenedIn{-1.f};  // widen's last call: the field of view given and returned (VR_FlatLookFov)
    float widenedOut{-1.f};
};

State st;

// vr_flat_speed_fov_test's steps (0: not running).
struct Test
{
    int phase{0};
    double phaseAt{0.0};
    testkit::Tally tally;
    bool shots{false};
    int steady{0};          // frames at a steady speed
    float lastSpeed{0.f};
    float base{90.f};       // the view's own field of view (scr_fov, as gl_screen.c's basefov is it with nothing on)
    float walkSpeed{0.f}, walkFov{0.f};
    float runSpeed{0.f}, runFov{0.f};
    float slideStart{0.f}, slidePeak{0.f}, slideFovPeak{0.f};
    bool seenSlide{false};
    bool shotEarly{false};
    double slideAt{0.0};
    float lastFov{0.f};
    float biggestStep{0.f}; // the largest frame's change of the field of view (a pop: a jump of it)
    float biggestRatio{0.f}; // and of it against what the ease allows that frame (over 1: a pop)
    float easeSecs{0.f};
    int hops{0};
    bool airborne{false};
    bool jumpHeld{false};
    float hopSpeeds[bhopHops]{};
    float bhopPeak{0.f}, bhopFovPeak{0.f};
};

Test test;

[[nodiscard]] float targetFor(float speed)
{
    const float most = za::clamp(vr_flat_speed_fov_max.value, 1.f, maxMultiplier);
    const float from = za::max(0.f, vr_flat_speed_fov_from.value);
    const float top = za::max(from + 1.f, vr_flat_speed_fov_top.value);
    return 1.f + (most - 1.f) * geom::smoothstep(from, top, speed);
}

void check(bool ok, const char* what)
{
    if(ok)
    {
        Con_Printf("flatspeedfov: PASS %s\n", what);
    }
    test.tally.check(ok, "flatspeedfov", what);
}

void nextPhase(int phase)
{
    test.phase = phase;
    test.phaseAt = vr_gametime;
    test.steady = 0;
}

void shot()
{
    if(test.shots)
    {
        Cbuf_InsertText("screenshot\n");
    }
}

void testFrame(float dt)
{
    Test& t = test;
    if(t.phase == 0)
    {
        return;
    }
    const float speed = st.speed;
    const float fov = r_refdef.basefov;
    const bool ground = cl.onground;
    if(t.lastFov > 0.f && dt > 0.f)
    {
        // A pop is a step larger than the ease allows: its first frame after the whole widening is asked for at once
        // (the slide's boost) is the largest a frame may take.
        const float span = za::clamp(vr_flat_speed_fov_max.value, 1.f, maxMultiplier) - 1.f +
                           za::clamp(vr_flat_slide_fov_bonus.value, 0.f, maxMultiplier - 1.f);
        const float tauIn = za::max(0.01f, vr_flat_speed_fov_ease.value);
        const float allowed = t.base * span * (1.f - za::exp(-dt / tauIn)) + 0.01f;
        t.biggestStep = za::max(t.biggestStep, za::abs(fov - t.lastFov));
        t.biggestRatio = za::max(t.biggestRatio, za::abs(fov - t.lastFov) / allowed);
    }
    t.lastFov = fov;
    t.steady = za::abs(speed - t.lastSpeed) < 0.5f && speed > 100.f && ground ? t.steady + 1 : 0;
    t.lastSpeed = speed;
    char line[256];
    if(vr_gametime - t.phaseAt > testPhaseSeconds && t.phase != 99)
    {
        q_snprintf(line, sizeof(line), "timed out (step %d: speed %.0f, ground %d, sliding %d, fov %.2f)", t.phase, speed,
            ground ? 1 : 0, flatmove::sliding() ? 1 : 0, fov);
        check(false, line);
        Cbuf_InsertText("-forward\n-moveright\n-jump\n-flatcrouch\n");
        nextPhase(99);
    }
    switch(t.phase)
    {
        case 1: // walking, steadily: the view's own field of view
            if(t.steady >= steadyFrames)
            {
                t.walkSpeed = speed;
                t.walkFov = fov;
                q_snprintf(line, sizeof(line), "walking at %.0f u/s the fov is the view's own: %.3f of %.3f", speed, fov, t.base);
                check(za::abs(fov - t.base) < 0.01f && st.mult == 1.f, line);
                Cbuf_InsertText("cl_alwaysrun 1\n");
                nextPhase(2);
            }
            break;
        case 2: // running, steadily: still its own
            if(t.steady >= steadyFrames && speed > t.walkSpeed + 50.f)
            {
                t.runSpeed = speed;
                t.runFov = fov;
                q_snprintf(line, sizeof(line), "running at %.0f u/s the fov is the view's own: %.3f of %.3f", speed, fov, t.base);
                check(za::abs(fov - t.base) < 0.01f && st.mult == 1.f, line);
                shot();
                t.biggestStep = t.biggestRatio = 0.f;
                t.seenSlide = t.shotEarly = false;
                Cbuf_InsertText("+flatcrouch\n");
                nextPhase(3);
            }
            break;
        case 3: // the slide: it widens with the slide's speed, and narrows back as it slows
            if(flatmove::sliding() && !t.seenSlide)
            {
                t.seenSlide = true;
                t.slideAt = vr_gametime;
                t.slideStart = speed;
            }
            if(t.seenSlide)
            {
                t.slidePeak = za::max(t.slidePeak, speed);
                if(fov > t.slideFovPeak)
                {
                    t.slideFovPeak = fov;
                }
                if(!t.shotEarly && vr_gametime - t.slideAt > 0.1)
                {
                    t.shotEarly = true;
                    shot();
                }
            }
            if(t.seenSlide && !flatmove::sliding())
            {
                q_snprintf(line, sizeof(line), "a slide from %.0f u/s (peak %.0f) widens the fov: at most %.2f of %.2f (x%.3f; the curve's x%.3f at the peak)",
                    t.slideStart, t.slidePeak, t.slideFovPeak, t.base, t.slideFovPeak / t.base, targetFor(t.slidePeak));
                check(t.slideFovPeak > t.base * 1.02f, line);
                Cbuf_InsertText("-flatcrouch\n-forward\n");
                nextPhase(4);
            }
            break;
        case 4: // stopped: it eases back to the view's own, no frame a pop
            if(za::abs(fov - t.base) < 0.01f)
            {
                t.easeSecs = static_cast<float>(vr_gametime - t.phaseAt);
                const float out = za::max(0.05f, vr_flat_speed_fov_ease_out.value);
                q_snprintf(line, sizeof(line), "eases back to the view's own fov in %.2f s after the slide (ease out %.2f s)", t.easeSecs, out);
                check(t.easeSecs < 8.f * out + 1.5f, line);
                q_snprintf(line, sizeof(line), "no pop: the largest frame's change of the fov %.3f degrees, %.2f of what the ease allows", t.biggestStep, t.biggestRatio);
                check(t.biggestRatio <= 1.05f, line);
                shot();
                cl.viewangles[YAW] += 180.f; // back the way he came: the slide ended near the hall's far wall
                Cbuf_InsertText("+forward\n");
                nextPhase(5);
            }
            break;
        case 5: // running again, then the chain: jump, and in the air strafe (the side key alone, the view turned with the
                // speed: Quake's air move adds speed at a right angle to it), jump again as he lands
            if(t.steady >= steadyFrames / 2)
            {
                t.hops = 0;
                t.airborne = false;
                t.biggestStep = t.biggestRatio = 0.f;
                Cbuf_InsertText("+jump\n");
                t.jumpHeld = true;
                nextPhase(6);
            }
            break;
        case 6:
        {
            if(speed > 50.f)
            {
                cl.viewangles[YAW] = glm::degrees(za::atan2(cl.velocity[1], cl.velocity[0]));
            }
            if(!ground && !t.airborne)
            {
                t.airborne = true;
                if(t.hops == 0)
                {
                    Cbuf_InsertText("-forward\n+moveright\n");
                }
                Cbuf_InsertText("-jump\n");
                t.jumpHeld = false;
            }
            else if(ground && t.airborne)
            {
                t.airborne = false;
                t.hopSpeeds[t.hops] = speed;
                t.hops++;
                if(t.hops < bhopHops)
                {
                    Cbuf_InsertText("+jump\n");
                    t.jumpHeld = true;
                }
            }
            t.bhopPeak = za::max(t.bhopPeak, speed);
            t.bhopFovPeak = za::max(t.bhopFovPeak, fov);
            if(t.hops >= bhopHops)
            {
                char hops[128] = "";
                for(int i = 0; i < bhopHops; i++)
                {
                    char one[16];
                    q_snprintf(one, sizeof(one), "%s%.0f", i ? " " : "", t.hopSpeeds[i]);
                    q_strlcat(hops, one, sizeof(hops));
                }
                q_snprintf(line, sizeof(line), "a bunny hop chain (landings at %s u/s, peak %.0f) widens the fov: at most %.2f (x%.3f)", hops,
                    t.bhopPeak, t.bhopFovPeak, t.bhopFovPeak / t.base);
                check(t.bhopFovPeak > t.base * 1.02f, line);
                Cbuf_InsertText("-moveright\n-jump\n");
                nextPhase(7);
            }
            break;
        }
        case 7: // and back
            if(za::abs(fov - t.base) < 0.01f)
            {
                q_snprintf(line, sizeof(line), "eases back after the chain in %.2f s; the largest frame's change %.3f degrees, %.2f of what the ease allows",
                    vr_gametime - t.phaseAt, t.biggestStep, t.biggestRatio);
                check(t.biggestRatio <= 1.05f, line);
                Con_Printf("flatspeedfov: measured walk %.0f u/s fov %.2f, run %.0f fov %.2f, slide %.0f..%.0f fov to %.2f, bhop peak %.0f fov to %.2f (base %.2f)\n",
                    t.walkSpeed, t.walkFov, t.runSpeed, t.runFov, t.slideStart, t.slidePeak, t.slideFovPeak, t.bhopPeak, t.bhopFovPeak, t.base);
                nextPhase(99);
            }
            break;
        default:
            t.tally.verdict("flatspeedfov");
            t = Test{};
            break;
    }
}

void test_f()
{
    if(!flatplay::active())
    {
        Con_Printf("vr_flat_speed_fov_test: flat-screen play (-flatscreenmode) only, open level ground ahead\n");
        return;
    }
    test = Test{};
    test.phase = 1;
    test.phaseAt = vr_gametime;
    test.shots = Cmd_Argc() > 1 && !q_strcasecmp(Cmd_Argv(1), "shots");
    test.base = scr_fov.value;
    Cbuf_InsertText("cl_alwaysrun 0\n+forward\n");
}

void status_f()
{
    Con_Printf("speed fov: %s, speed %.0f u/s, x%.3f (the speed's x%.3f), fov %.2f of %.2f (vr_flat_speed_fov_max %g from %g to %g u/s, ease %g s in, %g s out)\n",
        vr_flat_speed_fov.value != 0.f ? "on" : "off", st.speed, st.mult, st.target, r_refdef.basefov, scr_fov.value,
        vr_flat_speed_fov_max.value, vr_flat_speed_fov_from.value, vr_flat_speed_fov_top.value, vr_flat_speed_fov_ease.value,
        vr_flat_speed_fov_ease_out.value);
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_flat_speed_fov_test", test_f);
    Cmd_AddCommand("vr_flat_speed_fov_status", status_f);
}

void frame(bool on)
{
    if(!on)
    {
        if(st.mult != 1.f)
        {
            vid.recalc_refdef = 1;
        }
        st = State{};
        test = Test{};
        return;
    }
    const double now = vr_gametime;
    const float dt = st.lastTime > -1e8 ? za::clamp(static_cast<float>(now - st.lastTime), 0.f, 0.1f) : 0.f;
    st.lastTime = now;
    st.speed = za::sqrt(cl.velocity[0] * cl.velocity[0] + cl.velocity[1] * cl.velocity[1]);
    // A slide's own extra on top of its speed's (vr_flat_slide_fov_bonus, 0 by default): the ease takes it in and out.
    const float bonus = flatmove::sliding() ? za::clamp(vr_flat_slide_fov_bonus.value, 0.f, maxMultiplier - 1.f) : 0.f;
    st.target = vr_flat_speed_fov.value != 0.f && !cl.intermission ? za::min(maxMultiplier, targetFor(st.speed) + bonus) : 1.f;
    const float was = st.mult;
    const float tau = st.target > st.mult ? vr_flat_speed_fov_ease.value : vr_flat_speed_fov_ease_out.value;
    st.mult = tau <= 0.f ? st.target : geom::approachExp(st.mult, st.target, dt, tau);
    if(za::abs(st.mult - st.target) < 1e-4f)
    {
        st.mult = st.target; // settled: exactly 1 again, so the view is exactly its own
    }
    if(st.mult != was)
    {
        vid.recalc_refdef = 1; // the field of view is chosen when the engine recalculates its view (gl_screen.c)
    }
    testFrame(dt);
}

float widen(float fov, float adsShare)
{
    const float m = 1.f + (st.mult - 1.f) * (1.f - za::clamp(adsShare, 0.f, 1.f));
    const float out = m == 1.f ? fov : za::min(mostFov, fov * m);
    st.widenedIn = fov;
    st.widenedOut = out;
    return out;
}

float multiplier()
{
    return st.mult;
}

float lookFov(float basefov)
{
    return st.widenedOut == basefov && st.widenedIn > 0.f ? st.widenedIn : basefov;
}

} // namespace qvr::flatspeedfov

// in_sdl.c IN_FovScale: the field of view the look's speed is measured against -- `basefov` without the speed FOV's
// widening, so the mouse turns the same degrees at any speed (the ironsight's narrowing still slows it, as Quake's zoom).
extern "C" float VR_FlatLookFov(float basefov)
{
    return qvr::flatspeedfov::lookFov(basefov);
}
