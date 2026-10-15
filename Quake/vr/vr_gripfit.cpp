// vr_gripfit.cpp -- the grip fit: the prop moved out of the hand once per grab (vr_gripfit.hpp).

#include "vr_gripfit.hpp"
#include "vr_box3d.hpp"
#include "vr_cvars.hpp"
#include "vr_held.hpp"
#include "vr_units.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Chrono/Clock.hpp"
#include "Zancle/Container/Array.hpp"
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
constexpr int mostSteps = 64;

// What a fit works on (filled at each grab; kept for vr_grip_fit_last's bench): the hand's spheres in the prop's frame,
// open and in the fist (the same spheres: `closed` false if the fist isn't known), and the shape's faces.
struct Inputs
{
    za::Array<glm::vec4, maxSpheres> open{};
    za::Array<glm::vec4, maxSpheres> fist{};
    int spheres{0};
    bool closed{false};
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
// nearest face in the hull it is deepest in.
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

// The hand moved by `d` (the prop's frame): its deepest sphere's depth, and the way out (the sunk spheres' normals,
// weighted by depth; the deepest's alone if they cancel).
float measure(const Inputs& in, const glm::vec3& d, glm::vec3& escape)
{
    float deepest = -1e30f;
    glm::vec3 sum{0.f}, deepestNormal{0.f, 0.f, 1.f};
    for(int i = 0; i < in.spheres; i++)
    {
        glm::vec3 n{0.f, 0.f, 1.f};
        float depth = sphereDepth(in, in.open[i], d, n);
        if(in.closed)
        {
            glm::vec3 nc{0.f, 0.f, 1.f};
            const float dc = sphereDepth(in, in.fist[i], d, nc);
            if(dc < depth)
            {
                depth = dc;
                n = nc;
            }
        }
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

// The fit itself on `in`: the hand's move (the prop's frame), its depth before and after, the steps taken.
glm::vec3 run(const Inputs& in, float& before, float& after, int& steps)
{
    glm::vec3 escape{0.f};
    before = measure(in, glm::vec3{0.f}, escape);
    after = before;
    steps = 0;
    glm::vec3 d{0.f}, best{0.f};
    float depth = before;
    while(depth > in.maxDepth && steps < in.steps)
    {
        // At most a step, no more than the deepest sphere needs (a tenth of a step at least).
        const float amount = za::fmin(in.step, za::fmax(depth - in.maxDepth, 0.1f * in.step));
        glm::vec3 next = d + escape * amount;
        if(in.cap > 0.f)
        {
            const float length = glm::length(next);
            if(length > in.cap)
            {
                next *= in.cap / length;
            }
            if(glm::length(next - d) < 1e-3f * in.step)
            {
                break; // at the cap
            }
        }
        d = next;
        steps++;
        depth = measure(in, d, escape);
        if(depth < after)
        {
            after = depth;
            best = d;
        }
    }
    return best;
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
    in.spheres = za::min(static_cast<int>(open.size()), maxSpheres);
    in.closed = fist.size() == open.size();
    for(int i = 0; i < in.spheres; i++)
    {
        in.open[i] = glm::vec4{handToProp * glm::vec3{open[i]} + handInProp, open[i].w};
        if(in.closed)
        {
            in.fist[i] = glm::vec4{handToProp * glm::vec3{fist[i]} + handInProp, fist[i].w};
        }
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
    r.spheres = in.spheres;
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
    za::I64 most = 0, total = 0;
    for(int i = 0; i < runs; i++)
    {
        const za::I64 t0 = za::Clock::nowNanoseconds();
        (void)run(worst, before, after, steps);
        const za::I64 t = za::Clock::nowNanoseconds() - t0;
        total += t;
        most = za::max(most, t);
    }
    Con_Printf("vr_grip_fit_last: worst case (%d steps) %.2f us mean, %.2f us most over %d runs\n", steps,
        static_cast<double>(total) / runs / 1000.0, static_cast<double>(most) / 1000.0, runs);
}

} // namespace qvr::gripfit
