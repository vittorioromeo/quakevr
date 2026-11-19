// vr_slide.cpp -- the slide, flatscreen and headset play's one (vr_slide.hpp; docs/vr-port/FLATPLAY.md, "Sliding";
// ROUND25.md, "Sliding in VR").

#include "vr_slide.hpp"

#include "vr_api.h"
#include "vr_avatar.hpp"
#include "vr_cvars.hpp"
#include "vr_hands.hpp"
#include "vr_engine.hpp"
#include "vr_geom.hpp"
#include "vr_hull.hpp"
#include "vr_mem.hpp"
#include "vr_progs.hpp"
#include "vr_slidesound.hpp"
#include "vr_testkit.hpp"
#include "vr_tunables.hpp"
#include "vr_units.hpp"

#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/trigonometric.hpp>

extern "C" qboolean keydown[MAX_KEYS]; // keys.c

namespace qvr::slide
{

namespace
{

constexpr float steerBack = 0.35f; // radians (20 degrees) round straight back where the move keys don't steer a slide

// ------------------------------------------------------------------------------------------------------------- server

struct ServerState
{
    Slide slide[slots];
    bool headsetHeld[slots]{}; // a headset client's slide input, its last move (setHeadsetInput)
};

ServerState server;

// The turning stick's gesture (stickGesture): armed back in the middle, held while pulled down.
struct Gesture
{
    bool armed{true};
    bool held{false};
};

Gesture gesture;

// The headset's view drop in a slide (vr_slide_camera; viewDrop): this machine's player, eased.
struct ViewDrop
{
    float drop{0.f};       // units the view is let down now
    float rate{0.f};       // its speed (units/s), the spring's
    float want{0.f};       // where it is going
    float full{0.f};       // the whole way down of the last slide (viewDropShare)
    float eye{-1.f};       // the real eyes over the box's floor as this slide started (-1: not sliding)
    double lastTime{-1.0}; // vr_gametime at the last step
};

ViewDrop view;

// Seconds after a headset player's slide ends that his eyes, let down by the view drop and coming back up, do not lay him
// down (vr_hull.cpp): the slide's eyes are vr_prone_eye's own height at the defaults (22).
constexpr double riseGrace = 0.5;

// The input `ent` slides by: a flatscreen player's crouch key, a headset player's stick gesture.
[[nodiscard]] bool inputHeld(const edict_t* ent, int num)
{
    return hull::isFlat(ent) ? hull::flatCrouchHeld(ent) : server.headsetHeld[num];
}

// Whether `ent`'s way of playing slides at all: flatscreen, vr_flat_slide; a headset, its move's bit (sent only with
// vr_slide on: the server never reads the client's own setting).
[[nodiscard]] bool enabledFor(const edict_t* ent)
{
    return hull::isFlat(ent) ? slideOn() : true;
}

int slideMove(edict_t* ent, Slide& s, double now, int onground, const float* wishdir, float wishspeed, float* velocity)
{
    const int num = clientSlot(ent);
    const bool held = inputHeld(ent, num);
    const bool pressed = held && !s.held;
    // On the ground: Quake's flag, or the floor right under him and not going up (moving fast, the flag is off every
    // other move: the walk's clip down to the floor sets it only on the moves that meet it).
    // A jump: going up with the jump key held since it (QC PlayerJump clears FL_JUMPRELEASED until the key is let go).
    // Going up alone is no jump: up a ramp or a step at a slide's speed the floor's clip gives him more than jumpedUp.
    const int flags = static_cast<int>(ent->v.flags);
    const bool jumped = velocity[2] > jumpedUp && (ent->v.button2 != 0.f || !(flags & FL_JUMPRELEASED));
    bool floorFound = false;
    const glm::vec3 normal = floorNormal(ent, floorFound);
    const bool ground = onground || (floorFound && !jumped);
    const bool landed = ground && !s.grounded;
    s.held = held;
    s.grounded = ground;
    const float zWas = s.lastZ;
    s.lastZ = ent->v.origin[2];
    const glm::vec2 vel{velocity[0], velocity[1]};
    const float speed = glm::length(vel);

    if(!enabledFor(ent) || !held || hull::isProne(ent) || ent->v.waterlevel >= 2 ||
       static_cast<int>(ent->v.movetype) != MOVETYPE_WALK)
    {
        s.endWhy = s.on ? (!held ? "the key let go" : hull::isProne(ent) ? "prone" : ent->v.waterlevel >= 2 ? "water" : "not a walk") : s.endWhy;
        endSlide(s, now);
        return 0;
    }
    if(s.on && !ground)
    {
        if(jumped && !s.airborne)
        {
            s.jumpEnds++;
            s.endWhy = "a jump";
            endSlide(s, now); // a jump: Quake's air move keeps the speed
        }
        else
        {
            s.airborne = true; // off a ledge: on again when he lands
            s.airSteps++;
        }
        return 0;
    }
    if(s.on && s.airborne && ground)
    {
        s.airborne = false;
    }
    if(!s.on)
    {
        const float least = za::clamp(vr_flat_slide_min.value, 0.f, 400.f);
        if(!ground || (!pressed && !landed) || speed < za::max(least, 1.f) || now < s.cooldownUntil) // (>= 1: the boost divides by it)
        {
            return 0;
        }
        // Too tired for its first part: no slide, a plain crouch (the decision of 2026-11-10: not a weaker slide; the
        // tired run is slower already, vr_stamina_speed).
        const float part = slideStaminaPart();
        if(part > 0.f && progs::staminaLeft(ent) + 1e-3f < part)
        {
            s.tiredRefusals++;
            if(vr_flatplay_debug.value)
            {
                Con_Printf("slide: none: %.1f stamina left, its first part is %.1f\n", progs::staminaLeft(ent), part);
            }
            return 0;
        }
        s.on = true;
        s.starts++;
        s.groundSteps = s.airSteps = 0;
        s.startSpeed = speed;
        s.startedAt = now;
        s.partsPaid = 0;
        s.staminaPaid = 0.f;
        paySlideStamina(ent, s, now);
        const float top = za::clamp(vr_flat_slide_max.value, 200.f, 1000.f);
        const float boosted = za::max(speed, za::min(speed + za::clamp(vr_flat_slide_boost.value, 0.f, 200.f), top));
        velocity[0] *= boosted / speed;
        velocity[1] *= boosted / speed;
        return 1;
    }
    const glm::vec2 wish = wishspeed > 1.f ? glm::vec2{wishdir[0], wishdir[1]} : glm::vec2{0.f};
    s.groundSteps++;
    // Up a step (the last move rose on a level floor: a ramp's rise is its slope's): the speed a ramp as steep would
    // have taken over the step's length, vr_flat_slide_slope * rise / speed. Stairs slow it as a ramp does.
    glm::vec2 stepped = vel;
    const float rise = ent->v.origin[2] - zWas;
    if(rise > 1.f && rise < 24.f && glm::length(glm::vec2{normal.x, normal.y}) <= 0.02f && speed > 1.f)
    {
        const float loss = za::clamp(vr_flat_slide_slope.value, 0.f, 2000.f) * rise / za::max(speed, 100.f);
        stepped *= za::max(0.f, speed - loss) / speed;
    }
    const SlideStep st = slideStep(stepped, normal, wish, static_cast<float>(host_frametime));
    velocity[0] = st.vel.x;
    velocity[1] = st.vel.y;
    if(st.ended)
    {
        s.endWhy = "slowed";
        endSlide(s, now); // into the crouch (flatscreen, the key held) or his own height (a headset): the walk's friction from the next move
    }
    return 1;
}

// ---------------------------------------------------------------------------------------------------------------- tests

// vr_slide_test (a headset, the mock's sticks; vr_slide is set by the test and put back): on vrtesthall's long stretch
// (setpos at the start of every case), he runs (the moving stick forward) to a run's speed, then the turning stick
// does the case's thing for a while: vr_slide 0 pulled straight down (no slide, +movedown as before); vr_slide 1 pulled
// 30 and 45 degrees off down, straight down short of the threshold, snap and smooth turns swept round to straight down
// and turning a little downwards (no slide); straight down from the middle (a slide: its boost, length and end as the
// flat slide's, a crouched box, no +movedown; up again after it); and let go mid-slide (it ends).
struct Case
{
    const char* name;
    int slide;      // vr_slide
    int snap;       // vr_snap_turn: -1 as it is
    bool slides;    // a slide expected
    bool sweep;     // the stick swept round the rim from straight right to `stick`
    float x, y;     // the turning stick held
    bool letGo;     // let go 0.15 s into the slide
    int camera;     // vr_slide_camera (the view's drop: checked in the full slides)
};

constexpr Case cases[] = {
    {"vr_slide 0: straight down", 0, -1, false, false, 0.f, -1.f, false, 1},
    {"30 degrees off down", 1, -1, false, false, 0.5f, -0.866f, false, 1},
    {"45 degrees off down", 1, -1, false, false, -0.707f, -0.707f, false, 1},
    {"straight down at 0.7 (under the threshold)", 1, -1, false, false, 0.f, -0.7f, false, 1},
    {"snap turn swept round to down", 1, 45, false, true, 0.f, -1.f, false, 1},
    {"smooth turn swept round to down", 1, 0, false, true, 0.f, -1.f, false, 1},
    {"snap turn a little downwards", 1, 45, false, false, 0.9f, -0.43f, false, 1},
    {"smooth turn a little downwards", 1, 0, false, false, -0.9f, -0.43f, false, 1},
    {"straight down", 1, -1, true, false, 0.f, -1.f, false, 1},
    {"straight down, let go mid-slide", 1, -1, true, false, 0.f, -1.f, true, 1},
    {"straight down, vr_slide_camera 0", 1, -1, true, false, 0.f, -1.f, false, 0},
};
constexpr int caseCount = static_cast<int>(sizeof(cases) / sizeof(cases[0]));
constexpr int sweepFrames = 12;  // the rim sweep's frames, right to down
constexpr double watchFor = 0.6; // seconds a no-slide case is watched

struct VrTest
{
    int phase{0};          // 0 off; 1 placed, running up; 2 the stick's case; 3 the slide's end, standing up; 4 next
    int index{0};
    double phaseAt{0.0};
    int frame{0};
    float slideWas{0.f};   // vr_slide, vr_snap_turn, vr_slide_camera before
    float snapWas{0.f};
    float cameraWas{1.f};
    // The view's height (the eyes over the box's floor): before the slide, its lowest in it, when it first came within 5%
    // of the drop's way down (the flat slide's eyes) and back up after it; the pose printed mid-slide.
    float eye0{0.f};
    float eyeMin{0.f};
    float eyeMax{0.f};
    double downAt{-1.0};
    double upAt{-1.0};
    double slideEndAt{-1.0};
    bool posed{false};
    int startsWas{0};
    int soundsWas{0};      // the slide sound's starts before
    float startSpeed{0.f};
    float peak{0.f};
    double slideAt{-1.0};
    double endAt{-1.0};
    glm::vec3 from{0.f};
    bool crouchedAll{true}; // his box crouched on every frame of the slide on the ground
    bool downKey{false};    // +movedown's key (the turning stick's down, DPAD_DOWN) seen held
    testkit::Tally tally;
};

VrTest vrTest;

void check(bool ok, const char* what)
{
    if(ok)
    {
        Con_Printf("vrslide: PASS %s\n", what);
    }
    vrTest.tally.check(ok, "vrslide", what);
}

void stick(const char* hand, float x, float y)
{
    char cmd[96];
    q_snprintf(cmd, sizeof(cmd), "vr_mock_stick %s %.3f %.3f\n", hand, x, y);
    Cbuf_InsertText(cmd);
}

void place()
{
    Cbuf_InsertText("vr_mock_stick main 0 0\nvr_mock_stick off 0 0\nsetpos 0 -380 25 0 90 0\nnoclip 0\nvr_mock_stick off 0 1\n");
}

// The drawn view's eyes over `pl`'s box's floor.
[[nodiscard]] float viewEye(const edict_t* pl)
{
    const hands::State& s = hands::current();
    return 0.5f * (s.eyeOrigin[0].z + s.eyeOrigin[1].z) - (pl->v.origin[2] + pl->v.mins[2]);
}

// One line of the body as last posed (the tests: the flat slide's and the headset's compared, vrslide_test.sh): the head's
// height over the box's floor, then each hip, knee and ankle (the body's left, then its right) forward of the head along
// the view's yaw, to its left, and over the floor.
void printPose(const char* tag, const edict_t* pl)
{
    avatar::Skeleton k;
    if(!avatar::skeleton(k) || !k.legs)
    {
        Con_Printf("%s: no body posed\n", tag);
        return;
    }
    const float floorZ = pl->v.origin[2] + pl->v.mins[2];
    const float yaw = glm::radians(hands::current().headAngles.y);
    const glm::vec2 fwd{za::cos(yaw), za::sin(yaw)};
    const glm::vec2 left{-fwd.y, fwd.x};
    char line[512];
    const hands::State& s = hands::current();
    int n = q_snprintf(line, sizeof(line), "%s: pose floor %.1f head %.1f", tag,
        s.head.z - s.headHeight * units::metresToUnits() - floorZ, k.head.z - floorZ);
    // (Two lines: a console line wraps past its width.)
    const glm::vec3* joints[3] = {k.hip, k.knee, k.ankle};
    for(int j = 0; j < 3; j++)
    {
        if(j == 1)
        {
            Con_Printf("%s\n", line);
            n = q_snprintf(line, sizeof(line), "%s: pose legs", tag);
        }
        for(int side = 0; side < 2; side++)
        {
            const glm::vec3 d = joints[j][side] - k.head;
            n += q_snprintf(line + n, sizeof(line) - static_cast<size_t>(n), " %.1f %.1f %.1f", glm::dot(glm::vec2{d}, fwd),
                glm::dot(glm::vec2{d}, left), joints[j][side].z - floorZ);
        }
    }
    Con_Printf("%s\n", line);
}

void testFrame()
{
    VrTest& t = vrTest;
    if(t.phase == 0 || !sv.active)
    {
        return;
    }
    progs::ServerVm vm;
    edict_t* pl = progs::localPlayer();
    if(!pl)
    {
        return;
    }
    const double now = qcvm->time;
    const double inPhase = now - t.phaseAt;
    const Slide& s = state(1);
    const float speed = za::sqrt(pl->v.velocity[0] * pl->v.velocity[0] + pl->v.velocity[1] * pl->v.velocity[1]);
    const bool ground = (static_cast<int>(pl->v.flags) & FL_ONGROUND) != 0;
    const auto next = [&](int phase) {
        t.phase = phase;
        t.phaseAt = now;
        t.frame = 0;
    };
    const Case& c = cases[za::min(t.index, caseCount - 1)];
    char line[256];
    if(inPhase > 6.0 && t.phase != 5)
    {
        q_snprintf(line, sizeof(line), "%s: timed out (step %d: speed %.0f, ground %d, sliding %d)", c.name, t.phase, speed,
            ground ? 1 : 0, s.on ? 1 : 0);
        check(false, line);
        next(5);
    }
    switch(t.phase)
    {
        case 1: // running up to speed
            if(inPhase > 0.1 && ground && speed > 300.f && now > s.cooldownUntil && !s.on)
            {
                t.startsWas = s.starts;
                t.soundsWas = slidesound::starts();
                t.startSpeed = speed;
                t.peak = 0.f;
                t.slideAt = t.endAt = -1.0;
                t.from = glm::vec3{pl->v.origin[0], pl->v.origin[1], pl->v.origin[2]};
                t.crouchedAll = true;
                t.downKey = false;
                t.eye0 = t.eyeMin = t.eyeMax = viewEye(pl);
                t.downAt = t.upAt = t.slideEndAt = -1.0;
                t.posed = false;
                next(2);
                if(!c.sweep)
                {
                    stick("main", 0.f, 0.f); // (from the middle)
                }
            }
            break;
        case 2: // the case's stick
        {
            if(c.sweep)
            {
                // Round the rim from straight right (0 degrees) to straight down, a frame a step, then held there.
                const float a = -glm::half_pi<float>() * static_cast<float>(za::min(t.frame, sweepFrames)) / sweepFrames;
                stick("main", za::cos(a), za::sin(a));
            }
            else if(t.frame == 1)
            {
                stick("main", c.x, c.y);
            }
            t.frame++;
            t.downKey = t.downKey || keydown[K_DPAD_DOWN];
            t.peak = za::max(t.peak, speed);
            if(s.on && t.slideAt < 0.0)
            {
                t.slideAt = now;
            }
            if(s.on && !s.airborne && t.slideAt >= 0.0 && now - t.slideAt > 0.05)
            {
                t.crouchedAll = t.crouchedAll && hull::isCrouched(pl);
            }
            const float way = t.eye0 - slideEye(); // the view drop's whole way down
            if(t.slideAt >= 0.0)
            {
                const float e = viewEye(pl);
                t.eyeMin = za::min(t.eyeMin, e);
                t.eyeMax = za::max(t.eyeMax, e);
                if(t.downAt < 0.0 && za::abs(e - slideEye()) < 0.05f * way)
                {
                    t.downAt = now;
                }
                // The body mid-slide, the pose settled (compared with the flat slide's: vrslide_test.sh).
                if(!t.posed && c.camera != 0 && !c.letGo && now - t.slideAt >= 0.5)
                {
                    printPose("vrslide_pose", pl);
                    t.posed = true;
                }
            }
            if(!c.slides)
            {
                if(inPhase > watchFor)
                {
                    q_snprintf(line, sizeof(line), "%s: no slide (slides %d -> %d)%s", c.name, t.startsWas, s.starts,
                        c.slide == 0 ? (t.downKey ? ", +movedown's key pressed as before" : ", +movedown's key NOT pressed") : "");
                    check(s.starts == t.startsWas && (c.slide != 0 || t.downKey), line);
                    next(4);
                }
                break;
            }
            if(c.letGo && t.slideAt >= 0.0 && now - t.slideAt > 0.15 && t.endAt < 0.0)
            {
                stick("main", 0.f, 0.f);
                t.endAt = now;
            }
            if(t.slideAt >= 0.0 && !s.on)
            {
                const float secs = static_cast<float>(now - t.slideAt);
                if(c.letGo)
                {
                    q_snprintf(line, sizeof(line), "%s: it ends (%s) %.2f s after letting go", c.name, s.endWhy, now - t.endAt);
                    check(!strcmp(s.endWhy, "the key let go") && now - t.endAt < 0.1, line);
                    t.slideEndAt = now;
                    next(3);
                    break;
                }
                const float ease = za::clamp(vr_slide_camera_time.value, 0.05f, 0.5f);
                if(c.camera != 0)
                {
                    q_snprintf(line, sizeof(line),
                        "%s: the view down from %.1f to the flat slide's eyes %.1f (lowest %.1f), 95 percent of the way in %.3f s (ease %.2f s)",
                        c.name, t.eye0, slideEye(), t.eyeMin, t.downAt - t.slideAt, ease);
                    check(t.downAt >= 0.0 && t.downAt - t.slideAt <= ease + 0.05 && t.eyeMin > slideEye() - 0.2f &&
                        t.eyeMin < slideEye() + 0.2f, line);
                }
                else
                {
                    q_snprintf(line, sizeof(line), "%s: the view stays at the real head (%.1f, %.1f to %.1f)", c.name, t.eye0, t.eyeMin,
                        t.eyeMax);
                    check(t.eyeMax - t.eyeMin < 0.5f && za::abs(t.eyeMin - t.eye0) < 0.5f, line);
                }
                const float boost = za::clamp(vr_flat_slide_boost.value, 0.f, 200.f);
                q_snprintf(line, sizeof(line), "%s: slid from %.0f u/s, peak %.0f (boost %.0f), its key not +movedown's (%d)", c.name,
                    t.startSpeed, t.peak, boost, t.downKey ? 1 : 0);
                check(t.peak > t.startSpeed + boost * 0.7f && !t.downKey, line);
                // The flat slide's own checks (vr_flatslide_test): on level ground it loses vr_flat_slide_friction a second.
                const float friction = za::max(1.f, vr_flat_slide_friction.value);
                const float endSpeed = za::clamp(vr_flat_slide_end.value, 0.f, 300.f);
                const float want = (t.peak - endSpeed) / friction;
                const glm::vec3 at{pl->v.origin[0], pl->v.origin[1], pl->v.origin[2]};
                const float dist = glm::length(glm::vec2{at - t.from});
                q_snprintf(line, sizeof(line),
                    "%s: slide %.2f s (friction's %.2f; %d moves on the ground, %d off), %.0f units (%.0f from the start), ends (%s) at %.0f u/s",
                    c.name, secs, want, s.groundSteps, s.airSteps, dist, 0.5f * (t.peak + endSpeed) * want, s.endWhy, speed);
                check(za::abs(secs - want) < 0.1f + 0.1f * want && dist > 0.4f * (t.peak + endSpeed) * want &&
                    !strcmp(s.endWhy, "slowed"), line);
                q_snprintf(line, sizeof(line), "%s: his box crouched all through it (%d; the flat slide's: eyes %.0f over the feet)",
                    c.name, t.crouchedAll ? 1 : 0, slideEye());
                check(t.crouchedAll, line);
                q_snprintf(line, sizeof(line), "%s: its scrape started once (%d)", c.name, slidesound::starts() - t.soundsWas);
                check(slidesound::starts() == t.soundsWas + 1, line);
                stick("main", 0.f, 0.f);
                t.slideEndAt = now;
                next(3);
            }
            break;
        }
        case 3: // standing up again after it (his own head's height), the view back up at it
        {
            const float way = t.eye0 - slideEye();
            if(t.upAt < 0.0 && za::abs(viewEye(pl) - t.eye0) < 0.05f * way)
            {
                t.upAt = now;
            }
            if(!hull::isCrouched(pl) && (c.camera == 0 || viewDropNow() == 0.f))
            {
                q_snprintf(line, sizeof(line), "%s: up again after it (%.2f s)", c.name, inPhase);
                check(true, line);
                if(c.camera != 0)
                {
                    const float ease = za::clamp(vr_slide_camera_time.value, 0.05f, 0.5f);
                    q_snprintf(line, sizeof(line), "%s: the view back up to the real head %.1f (now %.1f), 95 percent in %.3f s (ease %.2f s)",
                        c.name, t.eye0, viewEye(pl), t.upAt - t.slideEndAt, ease);
                    check(t.upAt >= 0.0 && t.upAt - t.slideEndAt <= ease + 0.05 && za::abs(viewEye(pl) - t.eye0) < 0.1f, line);
                }
                next(4);
            }
            else if(inPhase > 0.8)
            {
                q_snprintf(line, sizeof(line), "%s: still crouched or the view down %.2f s after it (drop %.1f)", c.name, inPhase,
                    viewDropNow());
                check(false, line);
                next(4);
            }
            break;
        }
        case 4: // the next case
            if(++t.index >= caseCount)
            {
                next(5);
                break;
            }
            {
                char cmd[96];
                const Case& n = cases[t.index];
                q_snprintf(cmd, sizeof(cmd), "vr_slide %d\nvr_slide_camera %d\n", n.slide, n.camera);
                Cbuf_InsertText(cmd);
                if(n.snap >= 0)
                {
                    q_snprintf(cmd, sizeof(cmd), "vr_snap_turn %d\n", n.snap);
                    Cbuf_InsertText(cmd);
                }
            }
            place();
            next(1);
            break;
        default:
        {
            char cmd[96];
            q_snprintf(cmd, sizeof(cmd), "vr_mock_stick main 0 0\nvr_mock_stick off 0 0\nvr_slide %g\nvr_snap_turn %g\nvr_slide_camera %g\n",
                t.slideWas, t.snapWas, t.cameraWas);
            Cbuf_InsertText(cmd);
            t.tally.verdict("vrslide");
            t = VrTest{};
            break;
        }
    }
}

// vr_slide_pose_test (flatscreen play, vrslide_test.sh; +forward held by the script): from vrtesthall's long stretch, run
// to a run's speed, the crouch key slides, and 0.5 s into the slide the body as posed is printed (printPose) to compare
// with the headset's at the same point of its slide (vr_slide_test's "straight down").
struct PoseTest
{
    int phase{0}; // 0 off; 1 running up; 2 sliding
    double phaseAt{0.0};
    double slideAt{-1.0};
};

PoseTest poseTest;

void poseTest_f()
{
    if(!sv.active)
    {
        Con_Printf("flatslide_pose: a game you host, flatscreen play\n");
        return;
    }
    poseTest = PoseTest{};
    poseTest.phase = 1;
    progs::ServerVm vm;
    poseTest.phaseAt = qcvm->time;
    Cbuf_InsertText("setpos 0 -380 25 0 90 0\nnoclip 0\n"); // (setpos leaves him noclipping)
}

void vrTest_f()
{
    if(!sv.active)
    {
        Con_Printf("vr_slide_test: a headset (the mock's) in a game you host\n");
        return;
    }
    vrTest = VrTest{};
    vrTest.slideWas = vr_slide.value;
    vrTest.snapWas = vr_snap_turn.value;
    vrTest.cameraWas = vr_slide_camera.value;
    vrTest.phase = 1;
    progs::ServerVm vm;
    vrTest.phaseAt = qcvm->time;
    Cbuf_InsertText("vr_slide 0\n");
    place();
}

} // namespace

Slide& state(int num)
{
    return server.slide[num >= 0 && num < slots ? num : 0];
}

void resetServer()
{
    server = ServerState{};
}

namespace
{
// Forgotten with the server's world (vr_mem.hpp's resets): by client number, a slide held through a load or a restart
// doesn't carry on.
mem::Reset serverWorldReset{"slide", mem::ResetEvent::ServerWorld, mem::ResetPhase::State, &resetServer};
} // namespace

int clientSlot(const edict_t* ent)
{
    if(!sv.active || !ent || ent->free)
    {
        return -1;
    }
    const int num = NUM_FOR_EDICT(const_cast<edict_t*>(ent));
    return num >= 1 && num <= svs.maxclients && num < slots ? num : -1;
}

bool slideOn()
{
    return vr_flat_slide.value != 0.f;
}

glm::vec3 floorNormal(edict_t* ent, bool& found)
{
    found = false;
    vec3_t start, end;
    VectorCopy(ent->v.origin, start);
    VectorCopy(ent->v.origin, end);
    end[2] -= 4.f;
    const trace_t tr = SV_Move(start, ent->v.mins, ent->v.maxs, end, MOVE_NOMONSTERS, ent);
    if(tr.fraction < 1.f && !tr.startsolid && tr.plane.normal[2] > 0.5f)
    {
        found = true;
        return glm::vec3{tr.plane.normal[0], tr.plane.normal[1], tr.plane.normal[2]};
    }
    return glm::vec3{0.f, 0.f, 1.f};
}

// The slide's stamina (his note 2026-11-10: "1/3 as soon as the slide starts, 1/3 0.2s later, 1/3 0.4s later. This is
// to prevent slides that get accidentally interrupted by small geometry from costing too much"): the same pool as
// parries, shoves and the shoulder charge (the visor's STAM: QC VR_Stamina_Effort), only while that pool is used
// (VR_Stamina_On). A part's cost, 0 when the slide is free.
int slideStaminaParts()
{
    return static_cast<int>(za::clamp(vr_flat_slide_stamina_parts.value, 1.f, 6.f));
}

float slideStaminaPart()
{
    const float cost = za::clamp(vr_flat_slide_stamina.value, 0.f, 100.f);
    return cost > 0.f && progs::staminaOn() ? cost / static_cast<float>(slideStaminaParts()) : 0.f;
}

void paySlideStamina(edict_t* ent, Slide& s, double now)
{
    const float part = slideStaminaPart();
    const double step = static_cast<double>(za::clamp(vr_flat_slide_stamina_step.value, 0.f, 1.f));
    while(s.on && part > 0.f && s.partsPaid < slideStaminaParts() &&
          now - s.startedAt + 1e-6 >= step * static_cast<double>(s.partsPaid))
    {
        const float paid = part * progs::staminaEffort(ent, part, "a slide");
        s.staminaPaid += paid;
        s.partsPaid++;
        if(vr_flatplay_debug.value)
        {
            Con_Printf("flatmove: slide stamina part %d of %d: %.1f (%.1f in all, %.1f left)\n", s.partsPaid,
                slideStaminaParts(), paid, s.staminaPaid, progs::staminaLeft(ent));
        }
    }
}

void endSlide(Slide& s, double now)
{
    if(s.on)
    {
        s.cooldownUntil = now + za::clamp(vr_flat_slide_cooldown.value, 0.f, 3.f);
        s.endedAt = now;
    }
    s.on = false;
    s.airborne = false;
}

void setHeadsetInput(const edict_t* ent, bool held)
{
    const int num = clientSlot(ent);
    if(num >= 0)
    {
        server.headsetHeld[num] = held;
    }
}

bool onServer(const edict_t* ent)
{
    const int num = clientSlot(ent);
    return num >= 0 && enabledFor(ent) && server.slide[num].on;
}

bool headsetOnServer(const edict_t* ent)
{
    return !hull::isFlat(ent) && onServer(ent);
}

float slideEye()
{
    // (Quake's standing eyes: its view height 22 over the box's middle, 24 over its feet.)
    return 46.f - za::clamp(vr_flat_crouch_drop.value, 0.f, 40.f) - za::clamp(vr_flat_slide_drop.value, 0.f, 14.f);
}

bool headsetSliding()
{
    return vr_slide.value != 0.f && sv.active && svs.maxclients >= 1 && server.slide[1].on && !server.slide[1].airborne;
}

bool headsetPose()
{
    return vr_slide.value != 0.f && vr_slide_body.value != 0.f && sv.active && svs.maxclients >= 1 && server.slide[1].on;
}

bool headsetRising(const edict_t* ent)
{
    const int num = clientSlot(ent);
    if(num < 0 || hull::isFlat(ent))
    {
        return false;
    }
    const Slide& s = server.slide[num];
    if(s.on || s.endedAt < 0.0)
    {
        return false;
    }
    progs::ServerVm vm;
    const double now = qcvm->time;
    return now >= s.endedAt && now - s.endedAt < riseGrace;
}

float viewDrop(float eyeOverOrigin)
{
    ViewDrop& v = view;
    const edict_t* pl = sv.active ? progs::localPlayer() : nullptr;
    const bool on = vr_slide.value != 0.f && vr_slide_camera.value != 0.f && pl != nullptr && svs.maxclients >= 1 &&
                    server.slide[1].on && pl->v.health > 0.f;
    // The real eyes over the box's floor as the slide starts: the drop takes them to slideEye from there, so the head's
    // own moves since stay one to one (an eye already under it: no drop). (Over the client's own origin, the one the view
    // stands on: the server's moves a unit up into the crouched box a frame before the client's does.)
    if(on && v.eye < 0.f)
    {
        v.eye = eyeOverOrigin - pl->v.mins[2];
    }
    else if(!on)
    {
        v.eye = -1.f;
    }
    v.want = on ? za::max(0.f, v.eye - slideEye()) : 0.f;
    if(on)
    {
        v.full = v.want;
    }
    // Stepped once a frame (the hands are placed more than once a frame: dt 0 then).
    const double now = vr_gametime;
    double dt = v.lastTime >= 0.0 ? now - v.lastTime : 0.0;
    if(dt < 0.0 || !sv.active)
    {
        v.drop = v.rate = 0.f; // (a new map, a new game: the clock went back)
        dt = 0.0;
    }
    v.lastTime = now;
    geom::easeCritical(v.drop, v.rate, v.want, za::clamp(vr_slide_camera_time.value, 0.05f, 0.5f),
        static_cast<float>(za::min(dt, 0.1)));
    if(v.want == 0.f && v.drop < 0.01f && za::abs(v.rate) < 0.1f)
    {
        v.drop = v.rate = 0.f; // settled: exactly the tracked head again
    }
    return v.drop;
}

bool viewDropMoving()
{
    return za::abs(view.drop - view.want) > 0.05f;
}

float viewDropNow()
{
    return view.drop;
}

float viewDropShare()
{
    return view.full > 0.01f ? za::clamp(view.drop / view.full, 0.f, 1.f) : 0.f;
}

SlideStep slideStep(const glm::vec2& vel, const glm::vec3& normal, const glm::vec2& wish, float dt)
{
    SlideStep out{vel, false};
    // Down the slope: the floor's normal leans toward the way down; its level part is the slope's sine.
    const glm::vec2 down{normal.x, normal.y};
    const float sine = glm::length(down);
    if(sine > 0.02f)
    {
        out.vel += down * (za::clamp(vr_flat_slide_slope.value, 0.f, 2000.f) * dt);
    }
    float speed = glm::length(out.vel);
    // The move keys turn it a little.
    const float steer = glm::radians(za::clamp(vr_flat_slide_steer.value, 0.f, 180.f)) * dt;
    if(steer > 0.f && speed > 1.f && glm::length(wish) > 0.1f)
    {
        const float have = za::atan2(out.vel.y, out.vel.x);
        float want = za::atan2(wish.y, wish.x) - have;
        while(want > glm::pi<float>())
        {
            want -= 2.f * glm::pi<float>();
        }
        while(want < -glm::pi<float>())
        {
            want += 2.f * glm::pi<float>();
        }
        // Toward any way but straight back: A or D alone ask for 90 degrees off the view, and the slide's way is seldom
        // the view's (he turned the mouse, or it already curved), so a side key often asked a little behind it and
        // the old "only ahead of it" turned nothing. A key held straight back (within steerBack of it) is no brake
        // and no turn round.
        if(za::abs(want) < glm::pi<float>() - steerBack)
        {
            const float turn = have + za::clamp(want, -steer, steer);
            out.vel = glm::vec2{za::cos(turn), za::sin(turn)} * speed;
        }
    }
    const float friction = za::clamp(vr_flat_slide_friction.value, 0.f, 1000.f);
    const float top = za::clamp(vr_flat_slide_max.value, 200.f, 1000.f);
    const float newSpeed = za::min(za::max(0.f, speed - friction * dt), top);
    out.vel = speed > 1e-3f ? out.vel * (newSpeed / speed) : glm::vec2{0.f};
    speed = newSpeed;
    out.ended = speed < za::clamp(vr_flat_slide_end.value, 0.f, 300.f);
    return out;
}

bool stickGesture(const glm::vec2& st, bool allowed)
{
    Gesture& g = gesture;
    const float len = glm::length(st);
    if(len < 0.3f)
    {
        g.armed = true; // back in the middle
    }
    // Degrees off straight down.
    const float off = glm::degrees(za::atan2(za::abs(st.x), -st.y));
    const float cone = za::clamp(vr_slide_stick_angle.value, 5.f, 45.f);
    const bool inCone = st.y < 0.f && off <= cone;
    if(!allowed)
    {
        g.held = false;
        g.armed = g.armed && (len < 0.5f || inCone);
        return false;
    }
    if(g.held)
    {
        // Held while still pulled down (a little looser than its start: a thumb's wobble keeps it).
        g.held = st.y < -0.5f && off <= cone + 15.f;
        return g.held;
    }
    if(len > 0.5f && !inCone)
    {
        g.armed = false; // a turn (or a pull off down): nothing until back in the middle, so a sweep round the rim never slides
    }
    if(g.armed && inCone && -st.y >= za::clamp(vr_slide_stick_threshold.value, 0.5f, 0.99f))
    {
        g.held = true;
        g.armed = false;
    }
    return g.held;
}

bool gestureHeld()
{
    return gesture.held;
}

void init()
{
    Cmd_AddCommand("vr_slide_test", vrTest_f);
    Cmd_AddCommand("vr_slide_pose_test", poseTest_f);
}

void poseTestFrame()
{
    PoseTest& t = poseTest;
    if(t.phase == 0 || !sv.active)
    {
        return;
    }
    progs::ServerVm vm;
    edict_t* pl = progs::localPlayer();
    if(!pl)
    {
        return;
    }
    const double now = qcvm->time;
    const Slide& s = state(1);
    const float speed = za::sqrt(pl->v.velocity[0] * pl->v.velocity[0] + pl->v.velocity[1] * pl->v.velocity[1]);
    const bool ground = (static_cast<int>(pl->v.flags) & FL_ONGROUND) != 0;
    if(now - t.phaseAt > 6.0)
    {
        Con_Printf("flatslide_pose: timed out (step %d: speed %.0f, sliding %d, starts %d, refused %d, ended: %s)\n", t.phase, speed,
            s.on ? 1 : 0, s.starts, s.tiredRefusals, s.endWhy);
        Cbuf_InsertText("-flatcrouch\n");
        t = PoseTest{};
        return;
    }
    if(t.phase == 1 && ground && speed > 300.f && !s.on)
    {
        Cbuf_InsertText("+flatcrouch\n");
        t.phase = 2;
        t.phaseAt = now;
    }
    else if(t.phase == 2)
    {
        if(s.on && t.slideAt < 0.0)
        {
            t.slideAt = now;
        }
        if(t.slideAt >= 0.0 && now - t.slideAt >= 0.5)
        {
            printPose("flatslide_pose", pl);
            Con_Printf("flatslide_pose: done\n");
            Cbuf_InsertText("-flatcrouch\n");
            t = PoseTest{};
        }
    }
}

void frame()
{
    testFrame();
}

} // namespace qvr::slide

using namespace qvr;

extern "C" int VR_SlideMove(edict_t* ent, int onground, const float* wishdir, float wishspeed, float* velocity)
{
    const int num = slide::clientSlot(ent);
    if(num < 0)
    {
        return 0;
    }
    slide::Slide& s = slide::state(num);
    const double now = qcvm->time;
    if(now < s.lastTime)
    {
        s = slide::Slide{}; // a new map (the clock starts again)
    }
    s.lastTime = now;
    const int out = slide::slideMove(ent, s, now, onground, wishdir, wishspeed, velocity);
    // The stamina's later parts as they fall due (in the air too: the same slide), and QC's word of the slide on the
    // ground (.vr_sliding: its charge into a monster, vr_kdtrig.qc VR_KdTrig_SlideCharge).
    slide::paySlideStamina(ent, s, now);
    progs::setFieldFloat(ent, progs::fields().vr_sliding, s.on && !s.airborne ? 1.f : 0.f);
    return out;
}

extern "C" int VR_SlideStepUp(edict_t* ent)
{
    const int num = slide::clientSlot(ent);
    if(num < 0)
    {
        return 0;
    }
    const slide::Slide& s = slide::state(num);
    return s.on && !s.airborne && s.grounded ? 1 : 0;
}
