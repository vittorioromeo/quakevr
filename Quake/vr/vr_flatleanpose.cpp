// vr_flatleanpose.cpp -- vr_flatleanpose_test [main:off ...] (vr_flatleanpose.hpp): the guns' poses leaning, flat-screen.

#include "vr_flatleanpose.hpp"

#include "vr_api.h"
#include "vr_backend.hpp"
#include "vr_tunables.hpp"
#include "vr_engine.hpp"
#include "vr_flatmove.hpp"
#include "vr_flatplay.hpp"
#include "vr_flatsight.hpp"
#include "vr_hands.hpp"
#include "vr_progs.hpp"
#include "vr_protocol.hpp"
#include "vr_testkit.hpp"
#include "vr_trace.hpp"

#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Asin.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"

#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

#include <cstdlib>
#include <cstring>

namespace qvr::flatleanpose
{
namespace
{

constexpr int pairsMost = 8;
constexpr int poseCount = 5;      // upright, full left, full right, leant into the wall at his side, leant away from it
constexpr double drawTime = 0.5;  // seconds after the pair is in his hands (the draw, the hands' ease)
constexpr double settleLeast = 0.3; // seconds a pose is held at least (the lean's own time is 0.18 s)
constexpr int stillFrames = 10;   // ... and then until both hands stayed within stillMove for this many frames
constexpr float stillMove = 0.01f;
constexpr float rigidMost = 1.f;  // units a leant gun may stand off the upright one turned rigidly round the pivot
constexpr float shotDegMost = 0.05f; // degrees a shot may be off the crosshair's point
constexpr float wallGap = 17.f;   // units off the nearer wall at his side (his box's half and 1), as vr_flatlean_test
constexpr float boxBottom = 24.f; // his standing box's floor under his origin

const char* const poseName[poseCount] = {"upright", "left", "right", "wall into", "wall away"};

struct Sample
{
    bool held{false};
    glm::vec3 pose{0.f};  // the pose's hand, world
    glm::vec3 drawn{0.f}; // the drawn hand, world
    bool fires{false};
    bool aims{false};
    float shotDeg{0.f};
    float shotApart{0.f};
    float depth{0.f};
};

struct Test
{
    int phase{0};
    double phaseAt{0.0};
    testkit::Tally tally;
    int pairs[pairsMost][2]{};
    int pairCount{0};
    int pair{0};
    int pose{0};
    int still{0};
    glm::vec3 last[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    glm::vec3 eye[poseCount]{};
    float roll[poseCount]{};  // the view's lean roll (flatmove::leanRoll)
    float floorZ{0.f};
    float wallSide{1.f};
    glm::vec3 start{0.f};
    Sample s[poseCount][2]{};
    int blockedWall{0};   // every pair's guns stopped leant at the wall that fired upright
};

Test test;

void check(bool ok, const char* what)
{
    if(ok)
    {
        Con_Printf("flatleanpose: PASS %s\n", what);
    }
    test.tally.check(ok, "flatleanpose", what);
}

[[nodiscard]] glm::vec3 levelRight(float yaw)
{
    const float y = glm::radians(yaw);
    return glm::vec3{za::sin(y), -za::cos(y), 0.f};
}

[[nodiscard]] glm::vec3 levelFwd(float yaw)
{
    const float y = glm::radians(yaw);
    return glm::vec3{za::cos(y), za::sin(y), 0.f};
}

// The pair (main:off weapon ids) into his hands, the off hand's first (as vr_flatpronedual_test); off 0: the main alone.
void givePair(const int (&p)[2])
{
    char give[160];
    const int off = p[1], main = p[0];
    const int n = off < 20 ? q_snprintf(give, sizeof(give), "impulse %d; wait; wait; ", 170 + off)
                           : q_snprintf(give, sizeof(give), "vr_test_give_wid %d; impulse 149; wait; wait; impulse 48; wait; wait; ", off);
    if(main < 20)
    {
        q_snprintf(give + n, sizeof(give) - n, "impulse %d\n", 150 + main);
    }
    else
    {
        q_snprintf(give + n, sizeof(give) - n, "vr_test_give_wid %d; impulse 149\n", main);
    }
    Cbuf_InsertText(give);
}

// Pairs from the command's arguments from `first` on (main:off), else `common`. False on a malformed one.
template <int N>
[[nodiscard]] bool readPairs(int first, const int (&common)[N][2], int (&pairs)[pairsMost][2], int& count, const char* cmd)
{
    count = 0;
    for(int i = first; i < Cmd_Argc() && count < pairsMost; i++)
    {
        const char* a = Cmd_Argv(i);
        const char* colon = strchr(a, ':');
        if(!colon)
        {
            Con_Printf("%s: pairs are main:off weapon ids (\"%s\")\n", cmd, a);
            return false;
        }
        pairs[count][0] = atoi(a);
        pairs[count][1] = atoi(colon + 1);
        count++;
    }
    if(count == 0)
    {
        for(; count < N && count < pairsMost; count++)
        {
            pairs[count][0] = common[count][0];
            pairs[count][1] = common[count][1];
        }
    }
    return true;
}

void lean(int side)
{
    Cbuf_InsertText(side < 0 ? "-flatleanright; +flatleanleft\n" : side > 0 ? "-flatleanleft; +flatleanright\n" : "-flatleanleft; -flatleanright\n");
}

void sample(int pose)
{
    Test& t = test;
    const hands::State& st = hands::current();
    const flatplay::FireRoute r = flatplay::routeFire(true, true);
    t.eye[pose] = st.head;
    t.roll[pose] = flatmove::leanRoll();
    for(int h = 0; h < HAND_COUNT; h++)
    {
        Sample& m = t.s[pose][h];
        m = Sample{};
        m.held = flatplay::heldIn(h) >= 0 && !flatplay::meleeIn(h);
        if(!m.held)
        {
            continue;
        }
        m.pose = st.pos[h];
        m.drawn = flatplay::drawnHand(h);
        m.fires = r.fire[h];
        m.aims = flatsight::shotOff(st, h, m.shotDeg, m.shotApart);
        m.depth = flatsight::wallDepth(h);
    }
}

// One gun's line: its pose and its drawn hand off the eye (right, forward, up in the level frame), the pose's height off the floor.
int describe(char* line, int size, int pose, int h)
{
    const Test& t = test;
    const Sample& m = t.s[pose][h];
    const float yaw = cl.viewangles[YAW];
    const glm::vec3 r = levelRight(yaw), f = levelFwd(yaw), up{0.f, 0.f, 1.f};
    const glm::vec3 p = m.pose - t.eye[pose], d = m.drawn - t.eye[pose];
    // (Short: the console log wraps a line at about 120 characters.)
    return q_snprintf(line, size, "|%s %.1f %.1f %.1f fl %.1f dr %.1f %.1f %s %.3f d %.1f", h == HAND_MAIN ? "main" : "off", glm::dot(p, r),
        glm::dot(p, f), glm::dot(p, up), m.pose.z - t.floorZ, glm::dot(d, r), glm::dot(d, up), m.fires ? "fires" : "STOP", m.shotDeg, m.depth);
}

// The leant poses against the upright one turned rigidly with the upper body round the lean's pivot (vr_flat_lean_pivot
// below the upright eye, vr_flat_lean_arms of the eye's own turn there).
void judge(const int (&p)[2])
{
    Test& t = test;
    char line[600];
    const float yaw = cl.viewangles[YAW];
    const glm::vec3 r = levelRight(yaw), f = levelFwd(yaw);
    const float pivot = za::clamp(vr_flat_lean_arms_pivot.value, 0.f, 120.f);
    for(int pose = 1; pose <= 2; pose++)
    {
        const glm::vec3 shift = t.eye[pose] - t.eye[0];
        const float side = glm::dot(shift, r);
        const float tilt = pivot >= 1.f ? za::asin(za::clamp(side / pivot, -1.f, 1.f)) : glm::radians(t.roll[pose]);
        const float angle = tilt * za::clamp(vr_flat_lean_arms.value, 0.f, 1.f);
        // (Round the level forward: +angle tilts the top to the right.)
        const glm::vec3 up{0.f, 0.f, 1.f};
        const float c = za::cos(angle), sn = za::sin(angle);
        for(int h = HAND_MAIN; h >= HAND_OFF; h--)
        {
            const Sample& now = t.s[pose][h];
            const Sample& was = t.s[0][h];
            if(!now.held || !was.held)
            {
                continue;
            }
            const auto turned = [&](const glm::vec3& at) {
                const glm::vec3 v = at - t.eye[0];
                const float vr = glm::dot(v, r), vu = glm::dot(v, up), vf = glm::dot(v, f);
                return t.eye[pose] + f * vf + r * (vr * c + vu * sn) + up * (vu * c - vr * sn);
            };
            // (The pose's hand, the shots' and the walls'; and the drawn one, what he sees.)
            const float off = glm::distance(now.pose, turned(was.pose));
            const float drawnOff = glm::distance(now.drawn, turned(was.drawn));
            q_snprintf(line, sizeof(line), "%d:%d %s %s: %.2f off rigid %.1f deg, drawn %.2f (floor %.1f, upright %.1f)",
                p[0], p[1], poseName[pose], h == HAND_MAIN ? "main" : "off", off, glm::degrees(angle), drawnOff, now.drawn.z - t.floorZ,
                was.drawn.z - t.floorZ);
            check(off < rigidMost && drawnOff < rigidMost, line);
            q_snprintf(line, sizeof(line), "%d:%d %s %s fires on the crosshair (%s, %.3f deg off, stops %.1f off)",
                p[0], p[1], poseName[pose], h == HAND_MAIN ? "main" : "off", now.fires ? "fires" : "STOPPED", now.shotDeg, now.shotApart);
            check(now.fires && (!now.aims || now.shotDeg < shotDegMost), line);
        }
    }
}

void judgeWall(const int (&p)[2])
{
    Test& t = test;
    int stopped = 0;
    for(int pose = 3; pose < poseCount; pose++)
    {
        for(int h = 0; h < HAND_COUNT; h++)
        {
            stopped += t.s[pose][h].held && t.s[0][h].fires && !t.s[pose][h].fires ? 1 : 0;
        }
    }
    t.blockedWall += stopped;
    char line[200];
    q_snprintf(line, sizeof(line), "%d:%d leant at a wall at his %s, %.0f off: guns stopped that fire upright: %d", p[0],
        p[1], t.wallSide < 0.f ? "left" : "right", wallGap, stopped);
    check(stopped == 0, line);
}

void frameTest()
{
    Test& t = test;
    if(t.phase == 0)
    {
        return;
    }
    const double now = vr_gametime;
    const double inPhase = now - t.phaseAt;
    const auto next = [&](int phase) {
        t.phase = phase;
        t.phaseAt = now;
    };
    char line[600];
    if(inPhase > 8.0 && t.phase != 99)
    {
        q_snprintf(line, sizeof(line), "timed out (step %d, pair %d, pose %d)", t.phase, t.pair, t.pose);
        check(false, line);
        lean(0);
        next(99);
    }
    const int (&p)[2] = t.pairs[za::min(t.pair, pairsMost - 1)];
    const entity_t& pl = cl_entities[cl.viewentity];
    const glm::vec3 origin{pl.origin[0], pl.origin[1], pl.origin[2]};
    switch(t.phase)
    {
        case 1: // the pair into his hands: the off hand's first (as vr_flatpronedual_test)
        {
            givePair(p);
            lean(0);
            cl.viewangles[PITCH] = 0.f;
            next(2);
            break;
        }
        case 2:
            if(cl.stats[protocol::STAT_QVR_WEAPON] == p[0] && cl.stats[protocol::STAT_QVR_WEAPON2] == p[1])
            {
                next(3);
            }
            break;
        case 3: // drawn
            if(inPhase > drawTime)
            {
                t.pose = 0;
                t.still = 0;
                t.start = origin;
                t.floorZ = origin.z - boxBottom;
                next(4);
            }
            break;
        case 4: // a pose held until the hands are still
        {
            cl.viewangles[PITCH] = 0.f;
            const hands::State& s = hands::current();
            const float moved = za::max(glm::distance(s.pos[0], t.last[0]), glm::distance(s.pos[1], t.last[1]));
            t.last[0] = s.pos[0];
            t.last[1] = s.pos[1];
            t.still = moved < stillMove ? t.still + 1 : 0;
            if(inPhase < settleLeast || t.still < stillFrames)
            {
                break;
            }
            sample(t.pose);
            // (A line a gun: the console wraps a longer one.)
            for(int h = HAND_MAIN; h >= HAND_OFF; h--)
            {
                if(t.s[t.pose][h].held)
                {
                    const int n = q_snprintf(line, sizeof(line), "%d:%d %s eye %.1f %.1f ", p[0], p[1], poseName[t.pose],
                        glm::dot(t.eye[t.pose] - t.eye[0], levelRight(cl.viewangles[YAW])), t.eye[t.pose].z - t.eye[0].z);
                    describe(line + n, sizeof(line) - n, t.pose, h);
                    Con_Printf("flatleanpose: %s\n", line);
                }
            }
            t.pose++;
            t.still = 0;
            if(t.pose == 1 || t.pose == 2)
            {
                lean(t.pose == 1 ? -1 : 1);
                next(4);
            }
            else if(t.pose == 3)
            {
                judge(p);
                // At the nearer wall at his side, wallGap off it (as vr_flatlean_test), upright first.
                const glm::vec3 r = levelRight(cl.viewangles[YAW]);
                const trace_t left = worldtrace::world(origin, origin - r * 2048.f);
                const trace_t right = worldtrace::world(origin, origin + r * 2048.f);
                const bool toLeft = left.fraction <= right.fraction;
                const float frac = toLeft ? left.fraction : right.fraction;
                t.wallSide = toLeft ? -1.f : 1.f;
                const glm::vec3 to = t.start + r * (t.wallSide * (frac * 2048.f - wallGap));
                q_snprintf(line, sizeof(line), "setpos %.1f %.1f %.1f\n", to.x, to.y, to.z);
                Cbuf_InsertText(line);
                lean(0);
                next(5);
            }
            else if(t.pose == poseCount)
            {
                judgeWall(p);
                lean(0);
                q_snprintf(line, sizeof(line), "setpos %.1f %.1f %.1f\n", t.start.x, t.start.y, t.start.z);
                Cbuf_InsertText(line);
                next(++t.pair < t.pairCount ? 6 : 7);
            }
            else
            {
                lean(t.pose == 3 ? static_cast<int>(t.wallSide) : -static_cast<int>(t.wallSide));
                next(4);
            }
            break;
        }
        case 5: // at the wall: the upright pose there, for the wall's own reference (pose 0 is re-taken)
        {
            const hands::State& s = hands::current();
            const float moved = za::max(glm::distance(s.pos[0], t.last[0]), glm::distance(s.pos[1], t.last[1]));
            t.last[0] = s.pos[0];
            t.last[1] = s.pos[1];
            t.still = moved < stillMove ? t.still + 1 : 0;
            if(inPhase < settleLeast || t.still < stillFrames)
            {
                break;
            }
            sample(0);
            t.still = 0;
            lean(static_cast<int>(t.wallSide));
            next(4);
            break;
        }
        case 6: // the next pair, back where he started
            if(inPhase > 0.1)
            {
                next(1);
            }
            break;
        case 7:
            q_snprintf(line, sizeof(line), "every pair at the wall: guns stopped that fire upright there: %d", t.blockedWall);
            Con_Printf("flatleanpose: %s\n", line);
            next(99);
            break;
        default:
            lean(0);
            t.tally.verdict("flatleanpose");
            test = Test{};
            break;
    }
}

void test_f()
{
    if(!flatplay::active() || !sv.active)
    {
        Con_Printf("vr_flatleanpose_test: flat-screen play (-flatscreenmode) in a game you host, standing in the open\n");
        return;
    }
    test = Test{};
    Test& t = test;
    // (The pairs asked for, main:off weapon ids; none: a shotgun alone, two shotguns, two nail pistols, two grunt's guns.)
    constexpr int common[][2] = {{4, 0}, {4, 4}, {15, 15}, {29, 29}};
    if(!readPairs(1, common, t.pairs, t.pairCount, "vr_flatleanpose_test"))
    {
        test = Test{};
        return;
    }
    t.phase = 1;
    t.phaseAt = vr_gametime;
}

// ------------------------------------------------------------------------------------------------ vr_flatslidepose_test
// The guns in a slide against the same guns crouched still (the slide is a crouch at speed): per pair, at a level look,
// 30 degrees down and 30 up, crouched at the start (the reference), then run and crouched at speed into a slide, sampled
// at its start, middle and end. Per gun, in the screen's frame (right, up, forward of the view's angles): the pose's hand
// off the pose's eye, the drawn hand off the drawn view, the gun's pitch, yaw and roll off the view's, whether it fires
// and how far its shot is off the crosshair's point. The slide's own offsets (Gun Forward / Down / Out / Roll Slide) are
// taken off for the checks, so the rigid rule is what is checked: the guns ride the eye as they are crouched.

constexpr int slidePitchCount = 3;
// (Quake's pitch: + looks down. Not 30 down: from the slide's low eye the floor's point is nearer than the guns' muzzles,
// which then aim as at a wall.)
constexpr float slidePitches[slidePitchCount] = {0.f, 20.f, -20.f};
constexpr int slideMoments = 4;
const char* const slideMomentName[slideMoments] = {"crouched", "start", "mid", "end"};
constexpr double slideStartAt = 0.1; // seconds into the slide
constexpr double slideMidAt = 0.35;  // ... its pose eased in (vr_flat_slide_body_time 0.2)
constexpr int slideRing = 8;
constexpr int slideEndBack = 3;           // the end: this many frames before the slide ended
constexpr float slideRigidMost = 1.f;     // units a slid gun may stand off the crouched one, on the screen
constexpr float slideTurnMost = 1.5f;     // degrees a slid gun may turn off the crouched one against the view
constexpr float slideDrawnMost = 1.f;     // units the drawn hand may stand off the pose's, on the screen
constexpr float slideShotDegMost = 0.25f; // degrees a shot may be off the crosshair's point (crouched still, looking down 30: up to 0.08)

struct SlideSample
{
    bool held{false};
    glm::vec3 pose{0.f};  // right, up, forward off the pose's eye
    glm::vec3 drawn{0.f}; // ... the drawn hand off the drawn view
    glm::vec3 gun{0.f};   // pitch (+ up), yaw (+ right), roll (+ the right side up) off the view, degrees
    bool fires{false};
    bool aims{false};
    float shotDeg{0.f};
    float depth{0.f}; // into a wall ahead (flatsight::wallDepth)
};

struct SlideTest
{
    int phase{0};
    double phaseAt{0.0};
    testkit::Tally tally;
    int pairs[pairsMost][2]{};
    int pairCount{0};
    int pair{0};
    int pitch{0};
    int still{0};
    glm::vec3 last[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    glm::vec3 start{0.f};
    float yaw{0.f};
    bool placed{false}; // the start given on the command line (else where he stands)
    SlideSample s[slideMoments][2]{};
    SlideSample ring[slideRing][2]{};
    int ringN{0};
    double slideAt{-1.0};
    int holds0{0};
    int holds{0};
    float worstPose{0.f}, worstDrawn{0.f}, worstTurn{0.f}, worstApart{0.f};
    float saved[4]{};
    bool shots{false};
    int pitches{slidePitchCount};
};

SlideTest slideTest;

// The slide's own gun offsets: taken off for the run, given back after.
Tunable* const slideOffsets[] = {&vr_flat_gun_ahead_slide, &vr_flat_gun_down_slide, &vr_flat_gun_out_slide, &vr_flat_slide_gun_roll};

void setOffset(int i, float value)
{
    char cmd[96];
    q_snprintf(cmd, sizeof(cmd), "%s %g\n", slideOffsets[i]->name, value);
    Cbuf_InsertText(cmd);
}

void slideCheck(bool ok, const char* what)
{
    if(ok)
    {
        Con_Printf("flatslidepose: PASS %s\n", what);
    }
    slideTest.tally.check(ok, "flatslidepose", what);
}

void slideSample(SlideSample (&out)[2])
{
    const hands::State& st = hands::current();
    const flatplay::FireRoute r = flatplay::routeFire(true, true);
    const glm::vec3 aim{cl.viewangles[0], cl.viewangles[1], cl.viewangles[2]};
    glm::vec3 f, rt, u;
    hands::angleVectors(aim, f, rt, u);
    const glm::vec3 view = flatplay::lastViewOrigin();
    const auto screen = [&](const glm::vec3& v) { return glm::vec3{glm::dot(v, rt), glm::dot(v, u), glm::dot(v, f)}; };
    for(int h = 0; h < HAND_COUNT; h++)
    {
        SlideSample& m = out[h];
        m = SlideSample{};
        m.held = flatplay::heldIn(h) >= 0 && !flatplay::meleeIn(h);
        if(!m.held)
        {
            continue;
        }
        m.pose = screen(st.pos[h] - st.head);
        m.drawn = screen(flatplay::drawnHand(h) - view);
        glm::vec3 gf, gr, gu;
        hands::angleVectors(st.rot[h], gf, gr, gu);
        m.gun = {glm::degrees(za::asin(za::clamp(glm::dot(gf, u), -1.f, 1.f))), glm::degrees(za::atan2(glm::dot(gf, rt), glm::dot(gf, f))),
            glm::degrees(za::atan2(glm::dot(gr, u), glm::dot(gu, u)))};
        m.fires = r.fire[h];
        float apart = 0.f;
        m.aims = flatsight::shotOff(st, h, m.shotDeg, apart);
        m.depth = flatsight::wallDepth(h);
    }
}

void slideKeys(const char* keys)
{
    Cbuf_InsertText(keys);
}

void slideShot()
{
    if(slideTest.shots)
    {
        Cbuf_InsertText("screenshot\n");
    }
}

void slideJudge(const int (&p)[2])
{
    SlideTest& t = slideTest;
    char line[300];
    const float pitch = slidePitches[t.pitch];
    for(int moment = 0; moment < slideMoments; moment++)
    {
        for(int h = HAND_MAIN; h >= HAND_OFF; h--)
        {
            const SlideSample& m = t.s[moment][h];
            if(!m.held)
            {
                continue;
            }
            q_snprintf(line, sizeof(line), "%d:%d p%.0f %s %s pose %.1f %.1f %.1f dr %.1f %.1f %.1f gun %.1f %.1f %.1f %s %.3f", p[0], p[1], pitch,
                slideMomentName[moment], h == HAND_MAIN ? "main" : "off", m.pose.x, m.pose.y, m.pose.z, m.drawn.x, m.drawn.y, m.drawn.z, m.gun.x,
                m.gun.y, m.gun.z, m.fires ? "fires" : "STOP", m.shotDeg);
            Con_Printf("flatslidepose: %s\n", line);
        }
    }
    for(int moment = 1; moment < slideMoments; moment++)
    {
        for(int h = HAND_MAIN; h >= HAND_OFF; h--)
        {
            const SlideSample& now = t.s[moment][h];
            const SlideSample& was = t.s[0][h];
            if(!now.held || !was.held)
            {
                continue;
            }
            const float off = glm::distance(now.pose, was.pose);
            const float drawnOff = glm::distance(now.drawn, was.drawn);
            // (The drawn hand off the pose's, against the same crouched: the arm's hold pulls a crouched gun in too, looking
            // up or down.)
            const float apart = glm::distance(now.drawn - now.pose, was.drawn - was.pose);
            const glm::vec3 turn = now.gun - was.gun;
            // (The roll: the pitch and yaw go with the crosshair's point, nearer from the slide's lower eye looking down; the
            // shot through it is checked below, so a gun pitched off by the body would fail there.)
            const float turnMost = za::abs(turn.z);
            t.worstPose = za::max(t.worstPose, off);
            t.worstDrawn = za::max(t.worstDrawn, drawnOff);
            t.worstTurn = za::max(t.worstTurn, turnMost);
            t.worstApart = za::max(t.worstApart, apart);
            // (The start: the slide's pose still easing in, the view's drop with it; said, the middle and the end checked.)
            if(moment == 1)
            {
                continue;
            }
            q_snprintf(line, sizeof(line), "%d:%d p%.0f %s %s: off crouched %.2f, drawn %.2f, drawn off pose %.2f more, turned %.1f %.1f %.1f deg", p[0],
                p[1], pitch, slideMomentName[moment], h == HAND_MAIN ? "main" : "off", off, drawnOff, apart, turn.x, turn.y, turn.z);
            slideCheck(off < slideRigidMost && drawnOff < slideRigidMost && apart < slideDrawnMost && turnMost < slideTurnMost, line);
            q_snprintf(line, sizeof(line), "%d:%d p%.0f %s %s fires on the crosshair (%s, %.3f deg off, wall %.1f)", p[0], p[1], pitch,
                slideMomentName[moment], h == HAND_MAIN ? "main" : "off", now.fires ? "fires" : "STOPPED", now.shotDeg, now.depth);
            // (A gun in a wall ahead -- the ramp's slide ends at the spiral room's wall -- keeps the wall's own rule:
            // vr_flat_wall_block_fire.)
            slideCheck((now.fires || now.depth > 0.f) && (!now.aims || now.shotDeg < slideShotDegMost), line);
        }
    }
}

void slidePlace()
{
    SlideTest& t = slideTest;
    progs::ServerVm vm;
    edict_t* pl = progs::localPlayer();
    if(!pl)
    {
        return;
    }
    pl->v.origin[0] = t.start.x;
    pl->v.origin[1] = t.start.y;
    pl->v.origin[2] = t.start.z;
    pl->v.velocity[0] = pl->v.velocity[1] = pl->v.velocity[2] = 0.f;
    SV_LinkEdict(pl, false);
}

void slideFrameTest()
{
    SlideTest& t = slideTest;
    if(t.phase == 0)
    {
        return;
    }
    const double now = vr_gametime;
    const double inPhase = now - t.phaseAt;
    const auto next = [&](int phase) {
        t.phase = phase;
        t.phaseAt = now;
        t.still = 0;
    };
    char line[300];
    if(inPhase > 8.0 && t.phase != 99)
    {
        q_snprintf(line, sizeof(line), "timed out (step %d, pair %d, pitch %d)", t.phase, t.pair, t.pitch);
        slideCheck(false, line);
        slideKeys("-forward\n-flatcrouch\n");
        next(99);
    }
    const int (&p)[2] = t.pairs[za::min(t.pair, pairsMost - 1)];
    if(t.phase >= 4 && t.phase < 99)
    {
        cl.viewangles[YAW] = t.yaw;
        cl.viewangles[PITCH] = slidePitches[t.pitch];
    }
    const float speed = za::sqrt(cl.velocity[0] * cl.velocity[0] + cl.velocity[1] * cl.velocity[1]);
    const auto handsStill = [&]() {
        const hands::State& s = hands::current();
        const float moved = za::max(glm::distance(s.pos[0], t.last[0]), glm::distance(s.pos[1], t.last[1]));
        t.last[0] = s.pos[0];
        t.last[1] = s.pos[1];
        t.still = moved < stillMove ? t.still + 1 : 0;
        return t.still >= stillFrames;
    };
    switch(t.phase)
    {
        case 1:
            slideKeys("-forward\n-flatcrouch\n");
            givePair(p);
            next(2);
            break;
        case 2:
            if(cl.stats[protocol::STAT_QVR_WEAPON] == p[0] && cl.stats[protocol::STAT_QVR_WEAPON2] == p[1])
            {
                next(3);
            }
            break;
        case 3:
            if(inPhase > drawTime)
            {
                t.pitch = 0;
                next(4);
            }
            break;
        case 4: // back at the start, keys up
            slideKeys("-forward\n-flatcrouch\n");
            slidePlace();
            next(5);
            break;
        case 5: // crouched still: the reference
            if(inPhase > 0.1 && !flatmove::slidePose() && speed < 1.f && t.still == 0 && flatplay::crouchRatio() < 0.01f)
            {
                slideKeys("+flatcrouch\n");
            }
            if(flatplay::crouchRatio() > 0.99f && inPhase > 0.5 && handsStill())
            {
                slideSample(t.s[0]);
                slideShot();
                slideKeys("-flatcrouch\n");
                next(6);
            }
            break;
        case 6: // up again, then run
            if(flatplay::crouchRatio() < 0.01f && inPhase > 0.3)
            {
                slideKeys("+forward\n");
                next(7);
            }
            break;
        case 7: // at speed: crouch
            if(speed > 300.f)
            {
                slideKeys("+flatcrouch\n");
                t.slideAt = -1.0;
                t.ringN = 0;
                t.holds0 = flatplay::armHolds();
                next(8);
            }
            break;
        case 8: // sliding
        {
            const bool on = flatmove::slidePose();
            if(on && t.slideAt < 0.0)
            {
                t.slideAt = now;
                t.s[1][0].held = t.s[1][1].held = t.s[2][0].held = t.s[2][1].held = false;
            }
            if(t.slideAt < 0.0)
            {
                break;
            }
            if(on)
            {
                slideSample(t.ring[t.ringN % slideRing]);
                t.ringN++;
                const double in = now - t.slideAt;
                if(in >= slideStartAt && !t.s[1][HAND_MAIN].held)
                {
                    slideSample(t.s[1]);
                    slideShot();
                }
                if(in >= slideMidAt && !t.s[2][HAND_MAIN].held)
                {
                    slideSample(t.s[2]);
                    slideShot();
                }
                break;
            }
            // Ended: the end a few frames before.
            const int back = za::min(slideEndBack, t.ringN - 1);
            for(int h = 0; h < HAND_COUNT; h++)
            {
                t.s[3][h] = t.ring[(t.ringN - 1 - back) % slideRing][h];
            }
            t.holds += flatplay::armHolds() - t.holds0;
            slideKeys("-forward\n-flatcrouch\n");
            slideJudge(p);
            if(++t.pitch < t.pitches)
            {
                next(4);
            }
            else
            {
                next(++t.pair < t.pairCount ? 1 : 9);
            }
            break;
        }
        case 9:
            q_snprintf(line, sizeof(line), "worst off crouched %.2f (drawn %.2f), turned %.1f deg, drawn off pose %.2f; the arm's hold pulled a drawn gun in %d times",
                t.worstPose, t.worstDrawn, t.worstTurn, t.worstApart, t.holds);
            Con_Printf("flatslidepose: %s\n", line);
            next(99);
            break;
        default:
            slideKeys("-forward\n-flatcrouch\n");
            for(int i = 0; i < 4; i++)
            {
                setOffset(i, t.saved[i]);
            }
            t.tally.verdict("flatslidepose");
            slideTest = SlideTest{};
            break;
    }
}

void slideTest_f()
{
    if(!flatplay::active() || !sv.active)
    {
        Con_Printf("vr_flatslidepose_test [x y z yaw] [main:off ...]: flat-screen play (-flatscreenmode) in a game you host, room to slide ahead\n");
        return;
    }
    slideTest = SlideTest{};
    SlideTest& t = slideTest;
    int first = 1;
    const entity_t& pl = cl_entities[cl.viewentity];
    t.start = {pl.origin[0], pl.origin[1], pl.origin[2]};
    t.yaw = cl.viewangles[YAW];
    if(Cmd_Argc() >= 5 && !strchr(Cmd_Argv(1), ':'))
    {
        t.start = {static_cast<float>(atof(Cmd_Argv(1))), static_cast<float>(atof(Cmd_Argv(2))), static_cast<float>(atof(Cmd_Argv(3)))};
        t.yaw = static_cast<float>(atof(Cmd_Argv(4)));
        first = 5;
    }
    // ("keep": the slide's own offsets left as they are, for a look at what he sees; the rigid checks then fail by them.
    // "shots": a screenshot at each moment sampled. "level": the level look only.)
    bool keep = false;
    for(; first < Cmd_Argc(); first++)
    {
        const char* a = Cmd_Argv(first);
        if(!strcmp(a, "keep"))
        {
            keep = true;
        }
        else if(!strcmp(a, "shots"))
        {
            t.shots = true;
        }
        else if(!strcmp(a, "level"))
        {
            t.pitches = 1;
        }
        else
        {
            break;
        }
    }
    // (A shotgun alone, two shotguns, two nail pistols.)
    constexpr int common[][2] = {{4, 0}, {4, 4}, {15, 15}};
    if(!readPairs(first, common, t.pairs, t.pairCount, "vr_flatslidepose_test"))
    {
        slideTest = SlideTest{};
        return;
    }
    for(int i = 0; i < 4; i++)
    {
        t.saved[i] = slideOffsets[i]->value;
        setOffset(i, keep ? t.saved[i] : 0.f);
    }
    t.phase = 1;
    t.phaseAt = vr_gametime;
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_flatleanpose_test", test_f);
    Cmd_AddCommand("vr_flatslidepose_test", slideTest_f);
}

void frame()
{
    frameTest();
    slideFrameTest();
}

} // namespace qvr::flatleanpose
