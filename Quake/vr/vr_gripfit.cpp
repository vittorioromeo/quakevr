// vr_gripfit.cpp -- the grip fit: the prop moved out of the hand once per grab (vr_gripfit.hpp).

#include "vr_gripfit.hpp"
#include "vr_box3d.hpp"
#include "vr_cvars.hpp"
#include "vr_held.hpp"
#include "vr_units.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Chrono/Clock.hpp"
#include "Zancle/Container/Array.hpp"
#include "Zancle/Math/Ceil.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Fmax.hpp"
#include "Zancle/Math/Fmin.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sqrt.hpp"


namespace qvr::gripfit
{

namespace
{

constexpr int maxSpheres = 160;
constexpr int maxPlanes = 256;
constexpr int maxHulls = 16;
constexpr int mostSteps = 32;

// A sphere of the hand in the prop's frame: open (`open`) and in the fist (`fist`, when `two`: a finger's; a palm's is the
// same in both, tested once).
struct Probe
{
    glm::vec4 open{0.f};
    glm::vec4 fist{0.f};
    bool two{false};
};

// What a fit works on (filled at each grab; kept for vr_grip_fit_last's bench): the hand's spheres and the shape's faces.
struct Inputs
{
    za::Array<Probe, maxSpheres> probes{};
    int count{0};
    za::Array<glm::vec4, maxPlanes> planes{};
    za::Array<int, maxHulls> hullEnds{};
    int hulls{0};
    float maxDepth{0.f}, step{0.f}, cap{0.f};
    int steps{0};
};

Inputs inputs;
Result lastResult;
za::Vector<glm::vec4> openSpheres[2];

// How deep the sphere `s` moved by `d` is in the shape (units; negative: clear of it), and the outward normal of its
// nearest face in the hull it is deepest in. A hull it is clear of is left at the first face it is clear of (its depth
// then only known to be 0 or less: all the fit needs).
float sphereDepth(const Inputs& in, const glm::vec4& s, const glm::vec3& d, glm::vec3& normal)
{
    const glm::vec3 c = glm::vec3{s} + d;
    float deepest = -1e30f;
    int begin = 0;
    for(int k = 0; k < in.hulls; k++)
    {
        float out = -1e30f;
        int face = begin;
        for(int j = begin; j < in.hullEnds[k]; j++)
        {
            const glm::vec4& p = in.planes[j];
            const float sd = p.x * c.x + p.y * c.y + p.z * c.z - p.w;
            if(sd > out)
            {
                out = sd;
                face = j;
                if(sd >= s.w)
                {
                    break;
                }
            }
        }
        const float depth = s.w - out;
        if(depth > deepest)
        {
            deepest = depth;
            normal = glm::vec3{in.planes[face]};
        }
        begin = in.hullEnds[k];
    }
    return deepest;
}

// How far the sphere `s` must go along `e` (unit) to sink no deeper than `allowed` into the shape: in each hull, its
// depth is its radius less the most of its face distances, each of which grows along `e` by its normal's share of it,
// so it is in no deeper from the least distance on that one face of them reaches the radius less `allowed`, and stays
// so on. 0 if it already is.
float sphereReach(const Inputs& in, const glm::vec4& s, const glm::vec3& e, float allowed)
{
    const glm::vec3 c{s};
    const float target = s.w - allowed;
    float most = 0.f;
    int begin = 0;
    for(int k = 0; k < in.hulls; k++)
    {
        float least = 1e30f;
        for(int j = begin; j < in.hullEnds[k]; j++)
        {
            const glm::vec4& p = in.planes[j];
            const float sd = p.x * c.x + p.y * c.y + p.z * c.z - p.w;
            if(sd >= target)
            {
                least = 0.f;
                break;
            }
            const float along = p.x * e.x + p.y * e.y + p.z * e.z;
            if(along > 1e-6f)
            {
                least = za::fmin(least, (target - sd) / along);
            }
        }
        most = za::fmax(most, least);
        begin = in.hullEnds[k];
    }
    return most;
}

// The probe's depth: the less of its two poses' (a finger clear of it either way is one the grasp keeps out of it).
float probeDepth(const Inputs& in, const Probe& p, const glm::vec3& d, glm::vec3& normal)
{
    float depth = sphereDepth(in, p.open, d, normal);
    if(p.two && depth > 0.f)
    {
        glm::vec3 n{0.f, 0.f, 1.f};
        const float other = sphereDepth(in, p.fist, d, n);
        if(other < depth)
        {
            depth = other;
            normal = n;
        }
    }
    return depth;
}

// The hand moved by `d`: its deepest probe's depth, and the way out (the sunk ones' nearest faces' normals, weighted by
// depth; the deepest's alone if they cancel).
float measure(const Inputs& in, const glm::vec3& d, glm::vec3& escape)
{
    float deepest = -1e30f;
    glm::vec3 sum{0.f}, deepestNormal{0.f, 0.f, 1.f};
    for(int i = 0; i < in.count; i++)
    {
        glm::vec3 n{0.f, 0.f, 1.f};
        const float depth = probeDepth(in, in.probes[i], glm::vec3{d}, n);
        if(depth > 0.f)
        {
            sum += n * depth;
        }
        if(depth > deepest)
        {
            deepest = depth;
            deepestNormal = n;
        }
    }
    const float length = glm::length(sum);
    escape = length > 1e-4f ? sum / length : deepestNormal;
    return deepest;
}

// The fit itself on `in`: the hand's move (the prop's frame), its depth before and after, the steps it took. Three
// passes over the hand's spheres and the shape's faces, however many steps: the depth before and the way out; how far
// along it each sphere must go (sphereReach: the steps needed, known at once, not stepped and measured again); the
// depth there. Whole steps (the last may go a little past the allowed depth), no more than vr_grip_fit_steps of them
// nor further than vr_grip_fit_max; if that leaves it deeper than before (it never should), it is not moved.
glm::vec3 run(const Inputs& in, float& before, float& after, int& steps)
{
    glm::vec3 escape{0.f};
    before = measure(in, glm::vec3{0.f}, escape);
    after = before;
    steps = 0;
    if(before <= in.maxDepth || in.steps <= 0)
    {
        return glm::vec3{0.f};
    }
    float need = 0.f;
    for(int i = 0; i < in.count; i++)
    {
        const Probe& p = in.probes[i];
        float t = sphereReach(in, p.open, escape, in.maxDepth);
        if(p.two && t > 0.f)
        {
            t = za::fmin(t, sphereReach(in, p.fist, escape, in.maxDepth));
        }
        need = za::fmax(need, t);
    }
    steps = za::max(1, static_cast<int>(za::fmin(za::ceil(need / in.step - 1e-4f), static_cast<float>(in.steps))));
    float t = static_cast<float>(steps) * in.step;
    if(in.cap > 0.f && t > in.cap)
    {
        t = in.cap;
    }
    const glm::vec3 d = escape * t;
    glm::vec3 unused{0.f};
    const float depth = measure(in, d, unused);
    if(depth >= before)
    {
        return glm::vec3{0.f};
    }
    after = depth;
    return d;
}

} // namespace

za::Vector<glm::vec4>& openHand(int hand)
{
    return openSpheres[hand == 0 ? 0 : 1];
}

const Result& last()
{
    return lastResult;
}

bool fit(int num, int hand, bool local, bool twoHand, const glm::mat3& handToProp, const glm::vec3& handInProp, const glm::vec3& lo,
    const glm::vec3& hi, glm::vec3& move)
{
    move = glm::vec3{0.f};
    if(!vr_grip_fit.value || !local || (hand != 0 && hand != 1))
    {
        return false;
    }
    const za::Vector<glm::vec4>& open = openSpheres[hand];
    const za::Vector<glm::vec4>& fist = held::fist(hand);
    if(open.empty())
    {
        return false;
    }
    const za::I64 t0 = za::Clock::nowNanoseconds();
    Inputs& in = inputs;
    in.count = za::min(static_cast<int>(open.size()), maxSpheres);
    const bool closed = fist.size() == open.size();
    for(int i = 0; i < in.count; i++)
    {
        Probe& p = in.probes[i];
        p.open = glm::vec4{handToProp * glm::vec3{open[i]} + handInProp, open[i].w};
        p.two = closed && glm::distance(glm::vec3{open[i]}, glm::vec3{fist[i]}) > 1e-3f;
        p.fist = p.two ? glm::vec4{handToProp * glm::vec3{fist[i]} + handInProp, fist[i].w} : p.open;
    }
    int planes = box3d::hullPlanes(num, in.planes.data(), maxPlanes, in.hullEnds.data(), maxHulls, in.hulls);
    const bool hull = planes > 0;
    if(!hull)
    {
        // Its drawn box: six faces.
        if(glm::any(glm::greaterThanEqual(lo, hi)))
        {
            return false;
        }
        for(int a = 0; a < 3; a++)
        {
            glm::vec3 n{0.f};
            n[a] = 1.f;
            in.planes[2 * a] = glm::vec4{n, hi[a]};
            in.planes[2 * a + 1] = glm::vec4{-n, -lo[a]};
        }
        planes = 6;
        in.hulls = 1;
        in.hullEnds[0] = 6;
    }
    const float cm = 0.01f * units::metresToUnits();
    in.maxDepth = vr_grip_fit_depth.value * cm;
    in.step = za::fmax(vr_grip_fit_step.value, 0.05f) * cm;
    in.cap = za::fmax(vr_grip_fit_max.value, 0.f) * cm;
    in.steps = za::clamp(static_cast<int>(vr_grip_fit_steps.value), 0, mostSteps);

    Result& r = lastResult;
    r = Result{};
    move = run(in, r.before, r.after, r.steps);
    r.ran = true;
    r.num = num;
    r.hand = hand;
    r.twoHand = twoHand;
    r.hull = hull;
    r.move = move;
    r.spheres = in.count;
    r.planes = planes;
    r.micros = static_cast<double>(za::Clock::nowNanoseconds() - t0) / 1000.0;
    if(developer.value || vr_debug_carry.value)
    {
        Con_Printf("gripfit: %d %s hand%s: depth %.2f -> %.2f units (allowed %.2f), %d steps, moved %.2f units, %d spheres, %d %s "
                   "faces, %.1f us\n",
            num, hand ? "main" : "off", twoHand ? " (second of two)" : "", r.before, r.after, in.maxDepth, r.steps,
            glm::length(move), r.spheres, planes, hull ? "hull" : "box", r.micros);
    }
    return glm::length(move) > 0.f;
}

void last_f()
{
    const Result& r = lastResult;
    if(!r.ran)
    {
        Con_Printf("vr_grip_fit_last: no grip fitted yet (vr_grip_fit %s)\n", vr_grip_fit.string);
        return;
    }
    Con_Printf("vr_grip_fit_last: entity %d, %s hand%s: depth %.2f -> %.2f units, %d steps, moved %.2f %.2f %.2f (prop frame), "
               "%d spheres, %d %s faces, %.1f us\n",
        r.num, r.hand ? "main" : "off", r.twoHand ? " (second of two)" : "", r.before, r.after, r.steps, r.move.x, r.move.y,
        r.move.z, r.spheres, r.planes, r.hull ? "hull" : "box", r.micros);

    // The worst case: the same inputs, every step taken (nothing is ever deep enough), 1000 times.
    Inputs worst = inputs;
    worst.maxDepth = -1e30f;
    worst.cap = 0.f;
    worst.steps = mostSteps;
    constexpr int runs = 1000;
    float before = 0.f, after = 0.f;
    int steps = 0;
    za::I64 most = 0, least = 0, total = 0;
    int slow = 0; // (over 50 us: the thread put off by the system, most likely)
    for(int i = 0; i < runs; i++)
    {
        const za::I64 t0 = za::Clock::nowNanoseconds();
        (void)run(worst, before, after, steps);
        const za::I64 t = za::Clock::nowNanoseconds() - t0;
        total += t;
        most = za::max(most, t);
        least = i == 0 ? t : za::min(least, t);
        slow += t > 50000 ? 1 : 0;
    }
    Con_Printf("vr_grip_fit_last: worst case (%d steps) %.2f us mean, %.2f least, %.2f most, %d of %d over 50 us\n", steps,
        static_cast<double>(total) / runs / 1000.0, static_cast<double>(least) / 1000.0, static_cast<double>(most) / 1000.0,
        slow, runs);
}

} // namespace qvr::gripfit
