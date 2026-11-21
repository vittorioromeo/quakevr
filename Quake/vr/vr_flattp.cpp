// vr_flattp.cpp -- flatscreen play's over-the-shoulder third-person camera (vr_flattp.hpp; docs/vr-port/FLATPLAY.md, "The
// third-person camera").
//
// The state is one camera's: how far out along its arm it stands (`share`, 0..1 of the arm the settings ask for), which
// shoulder it is on (`side`, -1 left .. 1 right, eased through a swap), and what the last drawn frame had (its pivot, its
// place). It is stepped once a drawn frame, in V_CalcRefdef (view); everything that asks for the camera between (the
// crosshair's point, the pose's question whether the view is his own eyes) reads it without stepping it, with the sweep
// redone for its own eye and aim: the camera it is told is never further out than that sweep allows either.
//
// vr_flat_tp_test (e1m1's start, a gun in hand): the option off leaves the view exactly as it was; on, in the open the
// camera stands at the offsets; backed into a wall it is pulled in; a walk round the room turning and strafing along its
// walls and corners never has the camera in a wall or a wall between it and the eye; the gun's shot (the visor crosshair's
// trace of it) lands on the camera's crosshair point; aiming down the sights goes into his eyes (vr_flat_tp_aim 0) and
// back out after, or stays out drawn in (vr_flat_tp_aim 1) with the gun still on the point. `vr_flat_tp_test shots`
// takes a screenshot in the open, against the wall and in each aim.

#include "vr_flattp.hpp"

#include "vr_api.h"
#include "vr_backend.hpp" // HAND_MAIN
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_seat.hpp"
#include "vr_flatplay.hpp"
#include "vr_flatsight.hpp"
#include "vr_flatxhair.hpp"
#include "vr_flycam.hpp"
#include "vr_geom.hpp"
#include "vr_hands.hpp"
#include "vr_testkit.hpp"
#include "vr_trace.hpp"
#include "vr_zancle.hpp"

#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"

#include <glm/geometric.hpp>

namespace qvr::flattp
{
namespace
{

constexpr float backMost = 200.f;  // vr_flat_tp_back's range
constexpr float sideMost = 64.f;   // vr_flat_tp_right's
constexpr float upMost = 64.f;     // vr_flat_tp_up's (and as far down)
constexpr float probeMost = 16.f;  // vr_flat_tp_probe's: the eye stands 16 from a wall at the nearest (his box's half width)
constexpr float awayMin = 0.5f;    // the camera this far off the eye counts as away from it
constexpr float headNear = 8.f;    // the body's fade reaches vr_flat_tp_fade_min this near the eye
constexpr float swapBelow = 0.5f;  // the near shoulder's arm clear for less than this share: swap, if the other is clearer
constexpr float swapBetter = 0.25f; // by this much
constexpr float swapBack = 0.9f;   // and back once the near shoulder's arm is clear this far again

struct State
{
    float share{1.f};          // how far out along the arm (0..1)
    float side{1.f};           // -1 the left shoulder .. 1 the right, eased through a swap
    bool swapped{false};       // on the other shoulder for now (vr_flat_tp_auto_swap)
    double lastTime{-1e9};
    bool drawn{false};         // the last frame drew from the camera
    glm::vec3 pivot{0.f};      // the last drawn frame's eye, its walk bob left out
    glm::vec3 cam{0.f};        // and its camera
    glm::vec3 inOrg{0.f};      // the view as it came in and went out (the test's "off leaves it as it was")
    glm::vec3 outOrg{0.f};
    float offLen{0.f};         // the camera's distance off the eye, last drawn
    float allowed{1.f};        // the share of the arm its sweep allowed, last drawn
    float lastFovScale{1.f};   // what the field of view was last recalculated for
};

// One camera answered for an eye and an aim this frame (the crosshair asks several times a pose).
struct Memo
{
    glm::vec3 eye{0.f}, aim{0.f}, cam{0.f};
    float share{-1.f}, side{0.f};
    int frame{-1};
};
// A seat's third-person camera (SPLITSCREEN.md, phase 4a): the seat owns it (seat::Part::FlatTp).
struct SeatTp
{
    State st;
    Memo memo;
};

// splitscreen (SPLITSCREEN.md, phase 4a): the current seat's (seat::Part::FlatTp)
[[nodiscard]] SeatTp& seatTp()
{
    return seat::part<SeatTp>(seat::current(), seat::Part::FlatTp);
}
[[nodiscard]] State& st()
{
    return seatTp().st;
}
[[nodiscard]] Memo& memo()
{
    return seatTp().memo;
}

[[nodiscard]] float preferredSide()
{
    return vr_flat_tp_side.value != 0.f ? -1.f : 1.f; // 0 the right shoulder, 1 the left
}

// How far the camera is drawn in while aiming down the sights: 1 out at the offsets, 0 in his eyes (vr_flat_tp_aim 0),
// vr_flat_tp_aim_scale (vr_flat_tp_aim 1).
[[nodiscard]] float aimShare()
{
    const float ads = geom::smooth01(flatsight::adsShare());
    if(vr_flat_tp_aim.value == 0.f)
    {
        return 1.f - ads;
    }
    return 1.f - ads * (1.f - za::clamp(vr_flat_tp_aim_scale.value, 0.1f, 1.f));
}

// The camera's arm off the eye, in the look's frame (yaw and pitch, no roll), in two legs as the third-person games sweep
// it: out to the shoulder `side` and up first, then back. A wall behind stops it over the shoulder, not in his head; a
// wall at the shoulder's side stops it short of the shoulder. Its share: how far along the two, by length.
struct Arm
{
    glm::vec3 side{0.f}; // eye -> over the shoulder
    glm::vec3 back{0.f}; // -> the camera
    [[nodiscard]] glm::vec3 whole() const { return side + back; }
    [[nodiscard]] float length() const { return glm::length(side) + glm::length(back); }
    [[nodiscard]] glm::vec3 at(float share) const
    {
        const float a = glm::length(side), b = glm::length(back);
        const float d = za::clamp(share, 0.f, 1.f) * (a + b);
        if(d <= a)
        {
            return a > 0.f ? side * (d / a) : glm::vec3{0.f};
        }
        return side + (b > 0.f ? back * ((d - a) / b) : glm::vec3{0.f});
    }
};

[[nodiscard]] Arm armFor(const glm::vec3& aim, float side)
{
    glm::vec3 fwd{}, right{}, up{};
    hands::angleVectors({aim.x, aim.y, 0.f}, fwd, right, up);
    const float back = za::clamp(vr_flat_tp_back.value, 0.f, backMost);
    const float out = za::clamp(vr_flat_tp_right.value, 0.f, sideMost);
    const float lift = za::clamp(vr_flat_tp_up.value, -upMost, upMost);
    // (Sighted with the camera kept out, vr_flat_tp_aim 1, only the back's leg draws in: the shoulder's stays, so his head
    // stays off the crosshair.)
    const float k = aimShare();
    const float kSide = vr_flat_tp_aim.value != 0.f ? 1.f : k;
    return {(right * (out * side) + up * lift) * kSide, -fwd * (back * k)};
}

[[nodiscard]] float probe()
{
    return za::clamp(vr_flat_tp_probe.value, 0.f, probeMost);
}

[[nodiscard]] bool solidAt(const glm::vec3& p)
{
    const int c = worldtrace::clientContents(p);
    return c == CONTENTS_SOLID || c == CONTENTS_SKY;
}

// How far along from `from` to `to` a ball of radius `r` gets (five lines: the middle one and four r off it), less r: the
// camera's room. A side line starting in a wall is left out (the eye is that near it; the middle line decides).
[[nodiscard]] float clearAlong(const glm::vec3& from, const glm::vec3& to, float r)
{
    const glm::vec3 d = to - from;
    const float len = glm::length(d);
    if(len < 0.01f)
    {
        return len;
    }
    const glm::vec3 dir = d / len;
    const glm::vec3 across = glm::cross(dir, glm::vec3{0.f, 0.f, 1.f});
    const glm::vec3 a = glm::length(across) > 1e-3f ? glm::normalize(across) : glm::vec3{1.f, 0.f, 0.f};
    const glm::vec3 b = glm::cross(a, dir);
    const glm::vec3 offsets[5] = {glm::vec3{0.f}, a * r, -a * r, b * r, -b * r};
    float best = len;
    const float reach = len + r; // (the ball's front past the camera)
    const glm::vec3 end = from + dir * reach;
    for(int i = 0; i < (r > 0.f ? 5 : 1); i++)
    {
        const trace_t tr = worldtrace::world(from + offsets[i], end + offsets[i]);
        if(tr.startsolid || tr.allsolid)
        {
            if(i == 0)
            {
                return 0.f;
            }
            continue;
        }
        if(tr.fraction < 1.f)
        {
            best = za::min(best, tr.fraction * reach - r); // (stopped r short of what it met)
        }
    }
    return za::max(0.f, best);
}

// The camera's share of `arm` its sweep allows from `eye`: the shoulder's leg, then (all of that clear) the back's.
[[nodiscard]] float allowedShare(const glm::vec3& eye, const Arm& arm)
{
    const float total = arm.length();
    if(total < 0.01f)
    {
        return 1.f;
    }
    const float a = glm::length(arm.side);
    const float first = a < 0.01f ? a : clearAlong(eye, eye + arm.side, probe());
    if(first < a - 0.01f)
    {
        return first / total;
    }
    const float b = glm::length(arm.back);
    return (a + (b < 0.01f ? b : clearAlong(eye + arm.side, eye + arm.whole(), probe()))) / total;
}

// A camera in the sky's volume or a wall the lines missed (a sliver between them): drawn in towards the eye till it isn't.
[[nodiscard]] glm::vec3 keepOut(const glm::vec3& eye, glm::vec3 cam)
{
    for(int i = 0; i < 6 && solidAt(cam); i++)
    {
        cam = eye + (cam - eye) * 0.5f;
    }
    return solidAt(cam) ? eye : cam;
}

[[nodiscard]] glm::vec3 place(const glm::vec3& eye, const glm::vec3& aim, float share, float side)
{
    const Arm arm = armFor(aim, side);
    return keepOut(eye, eye + arm.at(za::min(share, allowedShare(eye, arm))));
}

// The shoulder: the preferred one, or the other while the near side is blocked and the other is clearer.
// Returns the preferred side's sweep when it made one (allowedShare of armFor(aim, preferredSide())), -1 otherwise: the
// view's own sweep when the camera is on that side (nearly always) takes it, not up to ten traces again.
float stepSide(const glm::vec3& eye, const glm::vec3& aim, float dt)
{
    const float pref = preferredSide();
    float mine = -1.f;
    if(vr_flat_tp_auto_swap.value == 0.f || vr_flat_tp_right.value <= 0.f)
    {
        st().swapped = false;
    }
    else
    {
        mine = allowedShare(eye, armFor(aim, pref));
        if(!st().swapped)
        {
            st().swapped = mine < swapBelow && allowedShare(eye, armFor(aim, -pref)) > mine + swapBetter;
        }
        else if(mine >= swapBack)
        {
            st().swapped = false;
        }
    }
    const float target = st().swapped ? -pref : pref;
    const float tau = za::clamp(vr_flat_tp_ease_side.value, 0.f, 2.f);
    st().side = tau <= 0.f ? target : geom::approachExp(st().side, target, dt, tau);
    if(za::abs(st().side - target) < 1e-3f)
    {
        st().side = target;
    }
    return mine;
}

void reset()
{
    st().share = 1.f;
    st().side = preferredSide();
    st().swapped = false;
    st().drawn = false;
    st().offLen = 0.f;
    st().lastTime = -1e9;
    memo() = Memo{};
}

// ---------------------------------------------------------------------------------------------------------------------------
// vr_flat_tp_test

constexpr double phaseSeconds = 8.0; // a step's give-up (a safety net: the steps wait on what they test)
constexpr int settleFrames = 10;
constexpr int walkFrames = 432;      // 6 s at 72 frames a second
constexpr int walkStrafeEvery = 72;
constexpr float walkTurn = 0.75f;    // degrees a frame
constexpr int dirCount = 16;

struct Test
{
    int phase{0};
    double phaseAt{0.0};
    int frames{0};
    int steady{0};
    bool shots{false};
    testkit::Tally tally;
    int offFrames{0}, offMoved{0};
    int checked{0}, bad{0};      // frames with the camera checked, and in a wall / a wall between it and the eye
    char firstBad[160]{};
    glm::vec3 lastOrigin{0.f};
    glm::vec3 lastHit{0.f};
    float yaw{0.f};
    bool strafeRight{false};
    char startPos[96]{};         // "setpos x y z" of where it started (the aim's spot)
    glm::vec3 startOrigin{0.f};
};
Test test; // seat-shared: a test's own (one at a time)

void check(bool ok, const char* what)
{
    if(ok)
    {
        Con_Printf("flattp: PASS %s\n", what);
    }
    test.tally.check(ok, "flattp", what);
}

void nextPhase(int phase)
{
    const float* o = cl_entities[cl.viewentity].origin;
    Con_Printf("flattp: step %d from %.0f %.0f %.0f, yaw %.0f, movetype %d\n", phase, o[0], o[1], o[2], cl.viewangles[YAW],
        sv.active && sv_player ? static_cast<int>(sv_player->v.movetype) : -1);
    test.phase = phase;
    test.phaseAt = vr_gametime;
    test.frames = 0;
    test.steady = 0;
}

void shot()
{
    if(test.shots)
    {
        Cbuf_InsertText("screenshot\n");
    }
}

[[nodiscard]] float clearance(const glm::vec3& eye, float yaw)
{
    const glm::vec3 dir = hands::forward({0.f, yaw, 0.f});
    return worldtrace::world(eye, eye + dir * 2048.f).fraction * 2048.f;
}

// The yaw whose camera arm (the preferred shoulder's) is clearest, and then whose back is.
[[nodiscard]] float openYaw(const glm::vec3& eye)
{
    float best = 0.f, bestRoom = -1.f;
    for(int i = 0; i < dirCount * 2; i++)
    {
        const float yaw = 180.f * static_cast<float>(i) / dirCount;
        const float room = allowedShare(eye, armFor({0.f, yaw, 0.f}, preferredSide())) * 4096.f + clearance(eye, yaw + 180.f);
        if(room > bestRoom)
        {
            best = yaw;
            bestRoom = room;
        }
    }
    return best;
}

// The yaw the nearest wall is at.
[[nodiscard]] float wallYaw(const glm::vec3& eye)
{
    float best = 0.f, bestRoom = 1e9f;
    for(int i = 0; i < dirCount; i++)
    {
        const float yaw = 360.f * static_cast<float>(i) / dirCount;
        const float room = clearance(eye, yaw);
        if(room < bestRoom)
        {
            best = yaw;
            bestRoom = room;
        }
    }
    return best;
}

// A yaw with a wall ahead to shoot at, some way off (the furthest such under 1200), the camera's arm clear behind and the
// gun's side of the view clear ahead as well as the middle.
[[nodiscard]] float targetYaw(const glm::vec3& eye)
{
    float best = 0.f, bestRoom = -1.f;
    for(int i = 0; i < dirCount * 2; i++)
    {
        const float yaw = 180.f * static_cast<float>(i) / dirCount;
        const glm::vec3 right = hands::forward({0.f, yaw - 90.f, 0.f});
        const float room = za::min(clearance(eye, yaw), clearance(eye + right * 12.f, yaw));
        const bool armClear = allowedShare(eye, armFor({0.f, yaw, 0.f}, preferredSide())) >= 0.999f;
        if(armClear && room > 128.f && room < 1200.f && room > bestRoom)
        {
            best = yaw;
            bestRoom = room;
        }
    }
    return best;
}

// setpos leaves him in noclip: the test walks into the walls for real.
void walkMode()
{
    if(sv.active && sv_player && static_cast<int>(sv_player->v.movetype) == static_cast<int>(MOVETYPE_NOCLIP))
    {
        sv_player->v.movetype = static_cast<float>(MOVETYPE_WALK);
        noclip_anglehack = false;
    }
}

void face(float pitch, float yaw)
{
    cl.viewangles[PITCH] = pitch;
    cl.viewangles[YAW] = yaw;
    cl.viewangles[ROLL] = 0.f;
}

// The camera this frame: in no wall, nothing between it and the eye.
void checkClear()
{
    if(!st().drawn)
    {
        return;
    }
    test.checked++;
    const trace_t tr = worldtrace::world(st().cam, st().pivot);
    const bool inWall = solidAt(st().cam);
    const bool between = tr.startsolid || tr.allsolid || tr.fraction < 0.999f;
    if(inWall || between)
    {
        if(test.bad == 0)
        {
            q_snprintf(test.firstBad, sizeof(test.firstBad), "step %d frame %d: camera %.1f %.1f %.1f %s", test.phase,
                test.frames, st().cam.x, st().cam.y, st().cam.z, inWall ? "in a wall" : "behind a wall");
        }
        test.bad++;
    }
}

// The gun's shot (the visor crosshair's trace of the move's line) against the camera's crosshair point.
void checkAim(const char* when)
{
    glm::vec3 hit{0.f};
    float offPx = 0.f;
    const hands::State& s = hands::current();
    const glm::vec3 aim{cl.viewangles[0], cl.viewangles[1], cl.viewangles[2]};
    const glm::vec3 point = flatsight::crosshairHit(s, aim);
    const bool known = flatxhair::lastHit(HAND_MAIN, hit, offPx);
    const float range = glm::distance(point, st().cam);
    const float miss = glm::distance(hit, point);
    char line[400];
    q_snprintf(line, sizeof(line), "%s: the shot lands %.2f units off the crosshair's point %.0f units away (%.1f px off the centre; "
        "point %.0f %.0f %.0f, hit %.0f %.0f %.0f, camera %.0f %.0f %.0f, muzzle %.0f %.0f %.0f)",
        when, miss, range, offPx, point.x, point.y, point.z, hit.x, hit.y, hit.z, st().cam.x, st().cam.y, st().cam.z,
        s.muzzle[HAND_MAIN].x, s.muzzle[HAND_MAIN].y, s.muzzle[HAND_MAIN].z);
    check(known && miss < 2.f + range * 0.01f && offPx < 6.f, line);
}

void testFrame()
{
    Test& t = test;
    if(t.phase == 0)
    {
        return;
    }
    t.frames++;
    char line[256];
    if(vr_gametime - t.phaseAt > phaseSeconds && t.phase != 99)
    {
        const float* o = cl_entities[cl.viewentity].origin;
        q_snprintf(line, sizeof(line), "timed out (step %d, frame %d: share %.3f of %.3f, ads %.2f, away %d, steady %d, origin %.0f %.0f %.0f, "
            "yaw %.0f)", t.phase, t.frames, st().share, st().allowed, flatsight::adsShare(), away() ? 1 : 0, t.steady, o[0], o[1], o[2],
            cl.viewangles[YAW]);
        check(false, line);
        Cbuf_InsertText("-forward\n-back\n-moveright\n-moveleft\nvr_flat_aim 0\n");
        nextPhase(99);
    }
    const glm::vec3 origin = geom::vec(cl_entities[cl.viewentity].origin);
    const glm::vec3 eye = st().pivot;
    if(t.phase >= 2 && t.phase < 99)
    {
        checkClear();
    }
    switch(t.phase)
    {
        case 1: // the option off: the view leaves the hook exactly as it came
            t.offFrames++;
            t.offMoved += st().outOrg != st().inOrg || st().drawn ? 1 : 0;
            if(t.offFrames >= settleFrames)
            {
                q_snprintf(line, sizeof(line), "off: the view as it was on %d of %d frames, field of view x%.3f",
                    t.offFrames - t.offMoved, t.offFrames, fovScale());
                check(t.offMoved == 0 && fovScale() == 1.f && !away(), line);
                face(0.f, openYaw(st().inOrg));
                Cbuf_InsertText("vr_flat_tp 1\n");
                nextPhase(2);
            }
            break;
        case 2: // on, in the open: at the offsets
            t.steady = st().drawn && st().share >= 0.999f && st().side == preferredSide() ? t.steady + 1 : 0;
            if(t.steady >= settleFrames)
            {
                const glm::vec3 want = armFor({cl.viewangles[0], cl.viewangles[1], 0.f}, st().side).whole();
                const float off = glm::distance(st().cam - eye, want);
                q_snprintf(line, sizeof(line), "open: the camera %.2f units off where the offsets put it (%.1f off the eye, %.1f asked)",
                    off, st().offLen, glm::length(want));
                check(off < 0.5f && away(), line);
                shot();
                face(0.f, wallYaw(eye) + 180.f);
                Cbuf_InsertText("+back\n");
                t.lastOrigin = origin;
                nextPhase(3);
            }
            break;
        case 3: // backed into the nearest wall: pulled in
            t.steady = glm::distance(origin, t.lastOrigin) < 0.05f && t.frames > 4 ? t.steady + 1 : 0;
            t.lastOrigin = origin;
            if(t.steady >= settleFrames)
            {
                const float asked = glm::length(armFor({cl.viewangles[0], cl.viewangles[1], 0.f}, st().side).whole());
                q_snprintf(line, sizeof(line), "wall: backed into it, the camera pulled in to %.1f of %.1f off the eye (share %.2f of "
                    "the arm, shoulder %.2f)", st().offLen, asked, st().share, st().side);
                check(st().offLen < asked - 1.f && st().drawn, line);
                shot();
                Cbuf_InsertText("-back\n+forward\n+moveright\n");
                t.yaw = cl.viewangles[YAW];
                t.strafeRight = true;
                nextPhase(4);
            }
            break;
        case 4: // the walk: turning, strafing into the walls and corners
            t.yaw += walkTurn;
            face(0.f, t.yaw);
            t.phaseAt = vr_gametime; // (its length is counted in frames)
            if(t.frames % walkStrafeEvery == 0 && t.frames < walkFrames)
            {
                t.strafeRight = !t.strafeRight;
                Cbuf_InsertText(t.strafeRight ? "-moveleft\n+moveright\n" : "-moveright\n+moveleft\n");
            }
            if(t.frames >= walkFrames)
            {
                Cbuf_InsertText("-forward\n-moveright\n-moveleft\n");
                q_snprintf(line, sizeof(line), "walk: %d frames checked, %d with the camera in a wall or behind one%s%s", t.checked,
                    t.bad, t.bad ? ": first " : "", t.firstBad);
                check(t.bad == 0 && t.checked >= walkFrames / 2, line);
                Cbuf_InsertText(t.startPos);
                nextPhase(5);
            }
            break;
        case 5: // the aim: back where it started, facing a wall some way off with room behind
            if(t.yaw < 1e8f) // (not yet facing: back at the start for a few frames first)
            {
                t.steady = glm::distance(origin, t.startOrigin) < 1.f ? t.steady + 1 : 0;
                if(t.steady >= 3)
                {
                    walkMode();
                    face(0.f, targetYaw(eye));
                    t.yaw = 1e9f;
                    t.steady = 0;
                }
                break;
            }
            {
            glm::vec3 hit{0.f};
            float offPx = 0.f;
            const bool known = flatxhair::lastHit(HAND_MAIN, hit, offPx);
            t.steady = known && glm::distance(hit, t.lastHit) < 0.2f && cl.velocity[0] * cl.velocity[0] + cl.velocity[1] * cl.velocity[1] < 1.f
                           ? t.steady + 1
                           : 0;
            t.lastHit = hit;
            if(t.steady >= settleFrames * 2 && st().share >= st().allowed - 1e-3f)
            {
                checkAim("aim, third person");
                Cbuf_InsertText("vr_flat_tp_aim 0\nvr_flat_aim 1\n");
                nextPhase(6);
            }
            break;
            }
        case 6: // aiming down the sights, vr_flat_tp_aim 0: into his eyes
            t.steady = flatsight::adsShare() >= 1.f ? t.steady + 1 : 0;
            if(t.steady >= settleFrames)
            {
                q_snprintf(line, sizeof(line), "sights, first person: the camera %.2f off the eye, away %d, field of view x%.3f",
                    st().offLen, away() ? 1 : 0, fovScale());
                check(st().offLen < awayMin && !away() && fovScale() == 1.f, line);
                shot();
                Cbuf_InsertText("vr_flat_aim 0\n");
                nextPhase(7);
            }
            break;
        case 7: // and back out
            t.steady = flatsight::adsShare() <= 0.f && st().share >= st().allowed - 1e-3f ? t.steady + 1 : 0;
            if(t.steady >= settleFrames)
            {
                q_snprintf(line, sizeof(line), "sights off: back out, %.1f off the eye", st().offLen);
                check(away() && st().offLen > 10.f, line);
                Cbuf_InsertText("vr_flat_tp_aim 1\nvr_flat_aim 1\n");
                nextPhase(8);
            }
            break;
        case 8: // aiming down the sights, vr_flat_tp_aim 1: kept out, drawn in, the field of view narrowed
            t.steady = flatsight::adsShare() >= 1.f ? t.steady + 1 : 0;
            if(t.steady >= settleFrames * 2)
            {
                const float asked = glm::length(armFor({cl.viewangles[0], cl.viewangles[1], 0.f}, st().side).whole());
                const float hip = flatsight::hipFov() * fovScale();
                q_snprintf(line, sizeof(line), "sights, third person: the camera %.1f off the eye (%.1f asked), field of view %.1f of %.1f",
                    st().offLen, asked, r_refdef.basefov, hip);
                check(away() && st().offLen <= asked + 0.5f && st().offLen > awayMin && r_refdef.basefov < hip - 1.f, line);
                checkAim("aim, third person sighted");
                shot();
                Cbuf_InsertText("vr_flat_aim 0\nvr_flat_tp_aim 0\n");
                nextPhase(99);
            }
            break;
        case 99:
            if(flatsight::adsShare() <= 0.f || t.frames > 200)
            {
                t.tally.verdict("flattp");
                Cbuf_InsertText("vr_flat_tp 0\n");
                t.phase = 0;
            }
            break;
        default: break;
    }
}

void test_f()
{
    if(!flatplay::active())
    {
        Con_Printf("flattp: flatscreen play only (start the game with -flatscreenmode)\n");
        return;
    }
    test = Test{};
    test.shots = Cmd_Argc() > 1 && q_strcasecmp(Cmd_Argv(1), "shots") == 0;
    const float* o = cl_entities[cl.viewentity].origin;
    q_snprintf(test.startPos, sizeof(test.startPos), "setpos %.1f %.1f %.1f\n", o[0], o[1], o[2]);
    test.startOrigin = geom::vec(o);
    walkMode();
    Cbuf_InsertText("vr_flat_tp 0\nvr_flat_tp_aim 0\n");
    nextPhase(1);
}

void toggle_f()
{
    if(!flatplay::active())
    {
        Con_Printf("Third-person camera: flatscreen play only (start the game with -flatscreenmode).\n");
        return;
    }
    Cvar_SetValueQuick(&vr_flat_tp, vr_flat_tp.value != 0.f ? 0.f : 1.f);
    Con_Printf("Third-person camera %s.\n", vr_flat_tp.value != 0.f ? "on" : "off");
}

void status_f()
{
    Con_Printf("third-person camera: %s (vr_flat_tp %g; up: %d, away from the eyes: %d)\n", vr_flat_tp.value != 0.f ? "on" : "off",
        vr_flat_tp.value, on() ? 1 : 0, away() ? 1 : 0);
    Con_Printf("  offsets back %g right %g up %g, field of view x%g, probe %g; shoulder %s (now %.2f%s), auto swap %g\n",
        vr_flat_tp_back.value, vr_flat_tp_right.value, vr_flat_tp_up.value, vr_flat_tp_fov.value, vr_flat_tp_probe.value,
        preferredSide() > 0.f ? "right" : "left", st().side, st().swapped ? ", swapped" : "", vr_flat_tp_auto_swap.value);
    Con_Printf("  out %.3f of the arm, %.1f off the eye; ease out %g s, shoulder %g s; sights: %s (scale %g); body %.2f opaque\n",
        st().share, st().offLen, vr_flat_tp_ease_out.value, vr_flat_tp_ease_side.value,
        vr_flat_tp_aim.value != 0.f ? "stay in third person" : "first person", vr_flat_tp_aim_scale.value, bodyAlpha());
    // The sweep as it stands: how much of each leg is clear (the shoulder's, then the back's from the shoulder).
    if(st().drawn)
    {
        const Arm arm = armFor({cl.viewangles[0], cl.viewangles[1], 0.f}, st().side);
        Con_Printf("  eye %.1f %.1f %.1f, camera %.1f %.1f %.1f; clear: the shoulder's leg %.1f of %.1f, the back's %.1f of %.1f; "
                   "allowed %.3f of the arm\n",
            st().pivot.x, st().pivot.y, st().pivot.z, st().cam.x, st().cam.y, st().cam.z, clearAlong(st().pivot, st().pivot + arm.side, probe()),
            glm::length(arm.side), clearAlong(st().pivot + arm.side, st().pivot + arm.whole(), probe()), glm::length(arm.back),
            allowedShare(st().pivot, arm));
    }
}

} // namespace

bool on()
{
    return flatplay::active() && vr_flat_tp.value != 0.f && cl.stats[STAT_HEALTH] > 0 && !cl.intermission && !flycam::on() &&
           flatplay::orbit.dist <= 0.f;
}

bool away()
{
    return on() && st().drawn && st().offLen > awayMin;
}

glm::vec3 camera(const glm::vec3& eye, const glm::vec3& aim)
{
    if(!on())
    {
        return eye;
    }
    Memo& m = memo();
    if(m.frame == host_framecount && m.share == st().share && m.side == st().side && m.eye == eye && m.aim == aim)
    {
        return m.cam;
    }
    m.frame = host_framecount;
    m.share = st().share;
    m.side = st().side;
    m.eye = eye;
    m.aim = aim;
    m.cam = place(eye, aim, st().share, st().side);
    return m.cam;
}

glm::vec3 rayStart(const glm::vec3& eye, const glm::vec3& aim, const glm::vec3& dir)
{
    if(!on())
    {
        return eye;
    }
    const glm::vec3 cam = camera(eye, aim);
    const glm::vec3 level = cam + dir * za::max(0.f, glm::dot(eye - cam, dir));
    // (Beside his head along the ray: in a wall there, from as far as the camera's ray gets.)
    const trace_t tr = worldtrace::world(cam, level);
    return tr.fraction < 1.f ? cam + (level - cam) * za::max(0.f, tr.fraction - 0.01f) : level;
}

float fovScale()
{
    if(!on())
    {
        return 1.f;
    }
    const float k = vr_flat_tp_aim.value == 0.f ? 1.f - geom::smooth01(flatsight::adsShare()) : 1.f;
    return 1.f + (za::clamp(vr_flat_tp_fov.value, 0.5f, 1.5f) - 1.f) * k;
}

float bodyAlpha()
{
    if(!away())
    {
        return 1.f;
    }
    const float far = za::clamp(vr_flat_tp_fade.value, 0.f, 64.f);
    if(far <= headNear)
    {
        return 1.f;
    }
    const float lowest = za::clamp(vr_flat_tp_fade_min.value, 0.f, 1.f);
    return lowest + (1.f - lowest) * geom::smooth01((st().offLen - headNear) / (far - headNear));
}

void view(float* vieworg, const float* viewangles, float bob)
{
    (void)viewangles;
    st().inOrg = st().outOrg = geom::vec(vieworg);
    if(!on())
    {
        if(st().drawn)
        {
            reset();
        }
        return;
    }
    const double now = vr_gametime;
    const float dt = st().lastTime > -1e8 ? za::clamp(static_cast<float>(now - st().lastTime), 0.f, 0.1f) : 0.f;
    st().lastTime = now;
    // The eye, its walk bob left out (the camera rides the body, not the bob); the arm in the look's frame (the pose's aim:
    // the angles the crosshair's point is measured with, not the drawn view's kick and roll).
    const glm::vec3 eye = st().inOrg - glm::vec3{0.f, 0.f, bob};
    const glm::vec3 aim{cl.viewangles[PITCH], cl.viewangles[YAW], 0.f};
    if(!st().drawn)
    {
        st().side = preferredSide();
        st().share = 0.f; // (out from his eyes, eased: no pop as it comes on)
    }
    const float preferredShare = stepSide(eye, aim, dt);
    const Arm arm = armFor(aim, st().side);
    // (on the preferred side, its sweep already made: the same arm; mid-swap or swapped, its own)
    const float allowed = preferredShare >= 0.f && st().side == preferredSide() ? preferredShare : allowedShare(eye, arm);
    st().allowed = allowed;
    const float tau = za::clamp(vr_flat_tp_ease_out.value, 0.f, 2.f);
    // In at once (never further out than the sweep allows: no frame shows the inside of a wall), out eased.
    st().share = allowed <= st().share || tau <= 0.f ? allowed : geom::approachExp(st().share, allowed, dt, tau);
    if(allowed - st().share < 1e-3f)
    {
        st().share = allowed;
    }
    const glm::vec3 cam = keepOut(eye, eye + arm.at(st().share));
    st().pivot = eye;
    st().cam = cam;
    st().offLen = glm::distance(cam, eye);
    st().drawn = true;
    st().outOrg = cam;
    vieworg[0] = cam.x;
    vieworg[1] = cam.y;
    vieworg[2] = cam.z;
}

void frame(bool modeOn)
{
    const float scale = modeOn ? fovScale() : 1.f;
    if(scale != st().lastFovScale)
    {
        st().lastFovScale = scale;
        vid.recalc_refdef = 1; // the field of view is chosen when the engine recalculates its view (gl_screen.c)
    }
    if(!modeOn && st().drawn)
    {
        reset();
    }
    testFrame();
}

void init()
{
    Cmd_AddCommand("vr_flat_tp_toggle", toggle_f);
    Cmd_AddCommand("vr_flat_tp_status", status_f);
    Cmd_AddCommand("vr_flat_tp_test", test_f);
}

} // namespace qvr::flattp

// view.c's V_CalcRefdef, after the lean (VR_FlatViewLean) and before the death view: the third-person camera
// (vr_flat_tp; vr_flattp.cpp). Outside flatscreen play, or with it off, the view is left exactly as it was.
extern "C" void VR_FlatThirdPersonView(float* vieworg, float* viewangles, float bob)
{
    qvr::flattp::view(vieworg, viewangles, bob);
}
