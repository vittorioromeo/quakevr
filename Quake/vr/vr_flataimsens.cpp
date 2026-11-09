// vr_flataimsens.cpp -- the look's speed in the flatscreen ironsight (vr_flataimsens.hpp).
//
// The match. A point m half screen heights from the centre stands atan(m * tan(fovY / 2)) off the view's line, fovY the
// vertical field of view; the mouse turns that angle at hipfire to put it on the crosshair, and sighted the same counts
// turn the sighted view's angle to its own such point: scale = atan(m * T(sighted)) / atan(m * T(hipfire)), T(fov) the
// tangent of the vertical half-angle. As m goes to 0 that is the tangents' ratio IN_FovScale has always used, so 0 is it
// exactly (no atan taken); 1.33 is the 4:3 screen's edge (Battlefield's uniform soldier aiming at its 133%), 1.78 the
// 16:9 screen's. The vertical tangent is Quake's fov's (a 4:3 horizontal one, scr_fov) times the view's own ratio
// (r_refdef: fov_y against basefov), so a wider screen does not move the point.
//
// The multiplier. vr_flat_aim_sens over the ironsight's travel (flatsight::adsShare, the same the field of view narrows
// by): 1 + (sens - 1) * travel.
//
// Where. in_sdl.c: IN_FovScale (the mouse and the gyro) multiplies its tangent ratio by lookFactor (VR_FlatAimSens), and
// the stick's look, which no fov ever slowed, by stickScale (VR_FlatAimStickScale). Neither applies outside flatscreen
// play: a headset's look is its head and hands. The speed FOV's widening stays out (VR_FlatLookFov, as before).
//
// vr_flat_aim_sens_test (-flatscreenmode, a weapon with sights held, standing still): at fov 95 and 75, hipfire and
// sighted, with the match and the multiplier set and not, sights in, feeds the mouse 200 counts (IN_MockMouseMotion) and
// checks the degrees turned against hipfire's: match 0 is the old tangent ratio, the others their formulas, unsighted
// unchanged; and the stick's scale and the menu's readout agree. "flataimsens: N passed, M failed".

#include "vr_flataimsens.hpp"

#include "vr_api.h"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_flatplay.hpp"
#include "vr_flatsight.hpp"
#include "vr_testkit.hpp"
#include "vr_zancle.hpp"

#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"

#include <glm/trigonometric.hpp>

#include <cmath>

extern "C" cvar_t scr_fov; // gl_screen.c

namespace qvr::flataimsens
{
namespace
{

constexpr float mostMatch = 4.f;
constexpr float leastSens = 0.05f;
constexpr float mostSens = 5.f;
constexpr int testCounts = 200;           // the mouse's counts fed each step
constexpr double testStepSeconds = 5.0;   // a step longer than this has failed (a safety net, never its end)
constexpr int settledFrames = 3;          // frames the view stands at the step's fov before the mouse moves

[[nodiscard]] float halfTan(float fov)
{
    return std::tan(glm::radians(za::clamp(fov, 1.f, 179.f) * 0.5f));
}

// tan(fovY / 2) per tan(basefov / 2): the view's own (r_refdef, as SCR_CalcRefdef made it) in flatscreen play; 0.75
// (Quake's 4:3, scr_fov_adapt) for a menu read in a headset.
[[nodiscard]] float verticalPerFov()
{
    const float b = r_refdef.basefov;
    const float y = r_refdef.fov_y;
    if(!flatplay::active() || b < 1.f || b > 179.f || y < 1.f || y > 179.f)
    {
        return 0.75f;
    }
    return halfTan(y) / halfTan(b);
}

[[nodiscard]] float match()
{
    return za::clamp(vr_flat_aim_sens_match.value, 0.f, mostMatch);
}

[[nodiscard]] float sensAt(float travel)
{
    return 1.f + (za::clamp(vr_flat_aim_sens.value, leastSens, mostSens) - 1.f) * za::clamp(travel, 0.f, 1.f);
}

// The match's ratio of angles against the tangents' (1 at match 0).
[[nodiscard]] float matchOverTangents(float lookFov, float hipFov)
{
    const float m = match();
    if(m <= 0.f)
    {
        return 1.f;
    }
    const float tl = halfTan(lookFov);
    const float th = halfTan(hipFov);
    const float k = m * verticalPerFov();
    return (std::atan(k * tl) / std::atan(k * th)) / (tl / th);
}

// ---- vr_flat_aim_sens_test ----

struct Step
{
    float fov;
    bool sighted;
    float sens;
    float match;
};

constexpr Step steps[] = {
    {95.f, false, 1.f, 0.f},     // hipfire: IN_FovScale's 1
    {95.f, true, 1.f, 0.f},      // the old tangent ratio, exactly
    {95.f, true, 1.f, 1.78f},    // the 16:9 edge
    {95.f, true, 0.5f, 0.f},     // the multiplier
    {95.f, false, 0.5f, 1.78f},  // unsighted: unchanged by either
    {75.f, true, 0.5f, 1.78f},   // another fov: both
    {75.f, true, 1.f, 0.f},      // and the tangents there
};
constexpr int stepCount = static_cast<int>(sizeof(steps) / sizeof(steps[0]));

struct Test
{
    int phase{0}; // 0 off, 1 set the step, 2 settle, 3 the turn taken, 99 the verdict
    int step{0};
    double phaseAt{0.0};
    int settled{0};
    float yaw0{0.f};
    float lookFov{0.f};
    float factor{1.f};
    testkit::Tally tally;
    float fovWas{90.f};
    float sensWas{1.f};
    float matchWas{0.f};
};

Test test;

void next(int phase)
{
    test.phase = phase;
    test.phaseAt = vr_gametime;
    test.settled = 0;
}

void finish()
{
    Cvar_SetValueQuick(&scr_fov, test.fovWas);
    Cvar_SetValueQuick(&vr_flat_aim_sens, test.sensWas);
    Cvar_SetValueQuick(&vr_flat_aim_sens_match, test.matchWas);
    vid.recalc_refdef = 1;
    Cmd_ExecuteString("vr_flat_aim 0", src_command);
    test.tally.verdict("flataimsens");
    test = Test{};
}

// The scale the step should turn at, from its own numbers: the tangents' ratio, or the match's angles, times the
// multiplier, all at the sighted fov the travel ends on.
[[nodiscard]] float wanted(const Step& s)
{
    if(!s.sighted)
    {
        return 1.f;
    }
    const float hip = s.fov;
    const float look = flatsight::adsFovAt(hip);
    const float tl = halfTan(look);
    const float th = halfTan(hip);
    const float k = s.match * verticalPerFov();
    const float ratio = s.match > 0.f ? std::atan(k * tl) / std::atan(k * th) : tl / th;
    return ratio * s.sens;
}

void testFrame()
{
    Test& t = test;
    if(t.phase == 0)
    {
        return;
    }
    char line[320];
    if(!flatplay::active())
    {
        t.tally.check(false, "flataimsens", "flatscreen play went off");
        finish();
        return;
    }
    if(vr_gametime - t.phaseAt > testStepSeconds)
    {
        q_snprintf(line, sizeof(line), "timed out (step %d, phase %d, travel %.2f, look fov %.2f)", t.step, t.phase,
            flatsight::adsShare(), VR_FlatLookFov(r_refdef.basefov));
        t.tally.check(false, "flataimsens", line);
        finish();
        return;
    }
    const Step& s = steps[t.step];
    switch(t.phase)
    {
        case 1:
            Cvar_SetValueQuick(&scr_fov, s.fov);
            Cvar_SetValueQuick(&vr_flat_aim_sens, s.sens);
            Cvar_SetValueQuick(&vr_flat_aim_sens_match, s.match);
            vid.recalc_refdef = 1;
            // (At once: the script that started the test waits out its buffer.)
            Cmd_ExecuteString(s.sighted ? "vr_flat_aim 1" : "vr_flat_aim 0", src_command);
            cl.viewangles[PITCH] = 0.f;
            next(2);
            break;
        case 2:
        {
            // Settled: the travel all done (or none), the view drawn at the fov it ends on.
            const float travel = flatsight::adsShare();
            const float want = s.sighted ? flatsight::adsFovAt(s.fov) : s.fov;
            const float look = VR_FlatLookFov(r_refdef.basefov);
            const bool there = travel == (s.sighted ? 1.f : 0.f) && za::abs(look - want) < 1e-3f;
            t.settled = there ? t.settled + 1 : 0;
            if(t.settled >= settledFrames)
            {
                t.yaw0 = cl.viewangles[YAW];
                t.lookFov = look;
                t.factor = VR_FlatAimSens(look, scr_fov.value);
                IN_MockMouseMotion(testCounts, 0);
                next(3);
            }
            break;
        }
        case 3:
            if(cl.viewangles[YAW] != t.yaw0)
            {
                const float turned = -std::remainder(cl.viewangles[YAW] - t.yaw0, 360.f);
                const float hipDegrees = m_yaw.value * sensitivity.value * static_cast<float>(testCounts);
                const float got = turned / hipDegrees;
                const float want = wanted(s);
                const float tangents = halfTan(t.lookFov) / halfTan(s.fov);
                const float stick = VR_FlatAimStickScale(t.lookFov);
                const float menu = s.sighted ? sightedScale(s.fov) : 1.f;
                q_snprintf(line, sizeof(line),
                    "step %d: fov %.0f %s (look fov %.2f), sens %.2f, match %.2f: turned %.4f of hipfire, want %.4f "
                    "(tangents %.4f, factor %.6f, stick %.4f, menu %.4f)",
                    t.step, s.fov, s.sighted ? "sighted" : "hipfire", t.lookFov, s.sens, s.match, got, want, tangents,
                    t.factor, stick, menu);
                Con_Printf("flataimsens: %s\n", line);
                bool ok = za::abs(got - want) <= 1e-3f * za::max(want, 0.1f);
                // Match 0, multiplier 1: the old scale to the bit (the factor exactly 1); unsighted: no factor at all.
                if((s.match == 0.f && s.sens == 1.f) || !s.sighted)
                {
                    ok = ok && t.factor == 1.f;
                }
                ok = ok && za::abs(stick - want) <= 1e-4f && za::abs(menu - want) <= 1e-4f;
                t.tally.check(ok, "flataimsens", line);
                if(++t.step >= stepCount)
                {
                    finish();
                    return;
                }
                next(1);
            }
            break;
        default: break;
    }
}

void test_f()
{
    if(!flatplay::active())
    {
        Con_Printf("vr_flat_aim_sens_test: flat-screen play (-flatscreenmode), a weapon with sights held, standing still\n");
        return;
    }
    test = Test{};
    test.fovWas = scr_fov.value;
    test.sensWas = vr_flat_aim_sens.value;
    test.matchWas = vr_flat_aim_sens_match.value;
    next(1);
}

void status_f()
{
    const float look = VR_FlatLookFov(r_refdef.basefov);
    Con_Printf("aim sens: travel %.2f, look fov %.2f of %.2f, IN_FovScale's tangents %.4f times %.4f; sighted at full "
               "travel %.4f of hipfire (vr_flat_aim_sens %g, vr_flat_aim_sens_match %g); stick %.4f\n",
        flatsight::adsShare(), look, scr_fov.value, halfTan(look) / halfTan(scr_fov.value), lookFactor(look, scr_fov.value),
        sightedScale(scr_fov.value), vr_flat_aim_sens.value, vr_flat_aim_sens_match.value, stickScale(look));
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_flat_aim_sens_test", test_f);
    Cmd_AddCommand("vr_flat_aim_sens_status", status_f);
}

void frame()
{
    testFrame();
}

float scaleFor(float lookFov, float hipFov, float travel)
{
    return halfTan(lookFov) / halfTan(hipFov) * matchOverTangents(lookFov, hipFov) * sensAt(travel);
}

float lookFactor(float lookFov, float hipFov)
{
    const float travel = flatsight::adsShare();
    if(!flatplay::active() || travel <= 0.f)
    {
        return 1.f;
    }
    // (At match 0 and multiplier 1 this is 1 * 1 exactly: IN_FovScale's own ratio to the bit.)
    return matchOverTangents(lookFov, hipFov) * sensAt(travel);
}

float stickScale(float lookFov)
{
    const float travel = flatsight::adsShare();
    if(!flatplay::active() || travel <= 0.f)
    {
        return 1.f;
    }
    return scaleFor(lookFov, flatsight::hipFov(), travel);
}

float sightedScale(float hipFov)
{
    return scaleFor(flatsight::adsFovAt(hipFov), hipFov, 1.f);
}

} // namespace qvr::flataimsens

// in_sdl.c IN_FovScale: what its tangent ratio is multiplied by (lookFactor).
extern "C" float VR_FlatAimSens(float lookfov, float hipfov)
{
    return qvr::flataimsens::lookFactor(lookfov, hipfov);
}

// in_sdl.c IN_JoyMove's stick look: the ironsight's slowing of it (stickScale).
extern "C" float VR_FlatAimStickScale(float lookfov)
{
    return qvr::flataimsens::stickScale(lookfov);
}
