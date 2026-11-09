// vr_flatleanpose.cpp -- vr_flatleanpose_test [main:off ...] (vr_flatleanpose.hpp): the guns' poses leaning, flat-screen.

#include "vr_flatleanpose.hpp"

#include "vr_api.h"
#include "vr_backend.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_flatmove.hpp"
#include "vr_flatplay.hpp"
#include "vr_flatsight.hpp"
#include "vr_hands.hpp"
#include "vr_protocol.hpp"
#include "vr_testkit.hpp"
#include "vr_trace.hpp"

#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Asin.hpp"
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
    for(int i = 1; i < Cmd_Argc() && t.pairCount < pairsMost; i++)
    {
        const char* a = Cmd_Argv(i);
        const char* colon = strchr(a, ':');
        if(!colon)
        {
            Con_Printf("vr_flatleanpose_test: pairs are main:off weapon ids (\"%s\")\n", a);
            test = Test{};
            return;
        }
        t.pairs[t.pairCount][0] = atoi(a);
        t.pairs[t.pairCount][1] = atoi(colon + 1);
        t.pairCount++;
    }
    if(t.pairCount == 0)
    {
        for(const auto& c : common)
        {
            t.pairs[t.pairCount][0] = c[0];
            t.pairs[t.pairCount][1] = c[1];
            t.pairCount++;
        }
    }
    t.phase = 1;
    t.phaseAt = vr_gametime;
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_flatleanpose_test", test_f);
}

void frame()
{
    frameTest();
}

} // namespace qvr::flatleanpose
