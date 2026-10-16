// vr_gripfit.cpp -- the grip fit: the prop moved out of the drawn hand once per grab (vr_gripfit.hpp).

#include "vr_gripfit.hpp"
#include "vr_box3d.hpp"
#include "vr_cvars.hpp"
#include "vr_handrig.hpp"
#include "vr_held.hpp"
#include "vr_units.hpp"
#include "vr_view.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Chrono/Clock.hpp"
#include "Zancle/Container/Array.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Fmax.hpp"
#include "Zancle/Math/Fmin.hpp"
#include "Zancle/Math/MinMax.hpp"


namespace qvr::gripfit
{

namespace
{

constexpr int fingers = handrig::FingerCount;
constexpr int samples = 3;                                // each finger's curls: open, half, the tightest fist
constexpr float sampleCurl[samples] = {0.f, 2.f, 4.f};
constexpr int bodyMost = 40;                              // the palm, the back of the hand, the wrist
constexpr int fingerMost = 4;                             // a finger's (the thumb's with its ball)
constexpr int maxPoints = bodyMost + fingers * samples * fingerMost;
constexpr int maxPlanes = 256;
constexpr int maxHulls = 16;
constexpr int maxDirections = 5;
constexpr int mostMeasured = 2;                           // the ways out measured where they lead
constexpr int drawnMost = 128;                            // the drawn vertices the faces are pushed out to, at most
constexpr float drawnReachCm = 6.f;                       // a drawn vertex further out of a hull is another part's
constexpr float none = 1e30f;

// The rig's points (its vertices' indices), chosen once per rig: the palm joint's, and each finger's.
struct RigPoints
{
    bool made{false};
    unsigned generation{0};
    za::Vector<int> body;
    za::Vector<int> finger[fingers];
    float length{0.f}; // the hand's length (rig units, along +x)
};
RigPoints rigPoints;

// The hand's rig frame, kept every frame (setHandRig).
struct HandRig
{
    bool valid{false};
    glm::mat4 rigToHand{1.f};
    handrig::Pose pose;
};
HandRig handRigs[2];

// What a fit works on (filled at each grab; kept for vr_grip_fit_last's bench): the hand's points in the prop's frame
// (the palm's, then each finger's at each curl: finger f's curl s from fingerStart[f] + s * perFinger[f]) and the
// shape's faces.
struct Inputs
{
    za::Array<glm::vec3, maxPoints> points{};
    int body{0};
    int perFinger[fingers]{};
    int fingerStart[fingers]{};
    za::Array<glm::vec4, maxPlanes> planes{};
    za::Array<int, maxHulls> hullEnds{};
    int hulls{0};
    float bodyAllowed{0.f}, fingerAllowed{0.f}, cap{0.f};
};

Inputs inputs;
Result lastResult;

// vr_grip_fit_drawn's: the drawn hand's points and parts, the prop's drawn vertices, its shape (no allocation after the
// first); and the fit's drawn vertices when it reads them itself.
struct Scratch
{
    za::Vector<glm::vec4> hand;
    za::Vector<view::HandPart> parts;
    za::Vector<glm::vec3> prop;
    za::Vector<glm::vec3> fitProp;
    Inputs shape;
};
Scratch scratch;

// The `most` of `from` (vertex indices) spread over them: each the furthest from those chosen before, from the one
// furthest from their middle (once per rig).
void farthest(const za::Vector<int>& from, int most, za::Vector<int>& out)
{
    out.clear();
    const za::Vector<handrig::Vertex>& vs = handrig::rig().vertices;
    if(from.empty())
    {
        return;
    }
    glm::vec3 mid{0.f};
    for(const int i : from)
    {
        mid += vs[static_cast<size_t>(i)].pos;
    }
    mid /= static_cast<float>(from.size());
    za::Vector<float> dist; // (squared: from the middle, then from the nearest chosen)
    for(const int i : from)
    {
        const glm::vec3 d = vs[static_cast<size_t>(i)].pos - mid;
        dist.pushBack(glm::dot(d, d));
    }
    for(int n = 0; n < most && n < static_cast<int>(from.size()); n++)
    {
        size_t pick = 0;
        for(size_t k = 1; k < dist.size(); k++)
        {
            pick = dist[k] > dist[pick] ? k : pick;
        }
        if(n > 0 && dist[pick] <= 0.f)
        {
            break; // (every vertex already chosen)
        }
        const glm::vec3 c = vs[static_cast<size_t>(from[pick])].pos;
        out.pushBack(from[pick]);
        for(size_t k = 0; k < dist.size(); k++)
        {
            const glm::vec3 d = vs[static_cast<size_t>(from[k])].pos - c;
            dist[k] = n == 0 ? glm::dot(d, d) : za::fmin(dist[k], glm::dot(d, d));
        }
    }
}

const RigPoints& rigPointsNow()
{
    RigPoints& r = rigPoints;
    if(r.made && r.generation == handrig::generation())
    {
        return r;
    }
    r.made = true;
    r.generation = handrig::generation();
    const handrig::Rig& rig = handrig::rig();
    za::Vector<int> groups[1 + fingers];
    float lo = none, hi = -none;
    for(size_t i = 0; i < rig.vertices.size(); i++)
    {
        const handrig::Vertex& v = rig.vertices[i];
        int best = 0;
        for(int k = 1; k < v.count && k < 4; k++)
        {
            best = v.weight[k] > v.weight[best] ? k : best;
        }
        const int joint = v.joint[best];
        const bool palm = joint < 0 || joint >= handrig::data::numJoints || handrig::data::joints[joint].kind == handrig::data::PalmJoint;
        const int finger = palm ? -1 : za::clamp(static_cast<int>(handrig::data::joints[joint].finger), 0, fingers - 1);
        groups[palm ? 0 : 1 + finger].pushBack(static_cast<int>(i));
        lo = za::fmin(lo, v.pos.x);
        hi = za::fmax(hi, v.pos.x);
    }
    r.length = hi > lo ? hi - lo : 0.f;
    farthest(groups[0], bodyMost, r.body);
    for(int f = 0; f < fingers; f++)
    {
        farthest(groups[1 + f], fingerMost, r.finger[f]);
    }
    return r;
}

[[nodiscard]] glm::vec3 blend(const handrig::Posed& posed, const handrig::Vertex& v)
{
    glm::vec3 out{0.f};
    for(int i = 0; i < v.count && i < 4; i++)
    {
        out += v.weight[i] * posed.joint[v.joint[i]](v.pos);
    }
    return v.count == 1 ? posed.joint[v.joint[0]](v.pos) : out;
}

// How far `p` is in the shape past `allowed` (0: no further): in a convex hull, the least of its face distances inward
// (left at the first face it is within `allowed` of: not in that hull past it); the most of the hulls'. `face`, if given:
// the nearest face of the hull it is deepest in.
float excess(const Inputs& in, const glm::vec3& p, float allowed, int* face = nullptr)
{
    float most = 0.f;
    int begin = 0;
    for(int k = 0; k < in.hulls; k++)
    {
        const int end = in.hullEnds[k];
        float inner = none;
        int nearest = begin;
        int j = begin;
        for(; j < end; j++)
        {
            const glm::vec4& pl = in.planes[j];
            const float s = pl.w - (pl.x * p.x + pl.y * p.y + pl.z * p.z);
            if(s <= allowed)
            {
                break;
            }
            if(s < inner)
            {
                inner = s;
                nearest = j;
            }
        }
        if(j == end && end > begin && inner - allowed > most)
        {
            most = inner - allowed;
            if(face)
            {
                *face = nearest;
            }
        }
        begin = end;
    }
    return most;
}

// How far `p` must go along `e` (unit) to be in the shape no further than `allowed`: in each hull it is in past it, from
// where the first of its faces (each one's distance falling by its normal's share of the move) comes within it; the most
// of the hulls'. 0 if it already is; `none` if it can't along `e`.
float reach(const Inputs& in, const glm::vec3& p, const glm::vec3& e, float allowed)
{
    float most = 0.f;
    int begin = 0;
    for(int k = 0; k < in.hulls; k++)
    {
        const int end = in.hullEnds[k];
        float least = none;
        for(int j = begin; j < end; j++)
        {
            const glm::vec4& pl = in.planes[j];
            const float s = pl.w - (pl.x * p.x + pl.y * p.y + pl.z * p.z);
            if(s <= allowed)
            {
                least = 0.f;
                break;
            }
            const float along = pl.x * e.x + pl.y * e.y + pl.z * e.z;
            if(along > 1e-6f)
            {
                least = za::fmin(least, (s - allowed) / along);
            }
        }
        most = za::fmax(most, least);
        begin = end;
    }
    return most;
}

struct Overlap
{
    float body{0.f}, fingers{0.f}, deepest{0.f}; // units

    [[nodiscard]] float total() const { return body + fingers; }
};

// The hand moved by `d`: the overlap (each finger at its least sunk curl). `weights`, if given: each face's share of the
// way out (the sunk points' nearest faces, by how deep).
Overlap overlap(const Inputs& in, const glm::vec3& d, float* weights)
{
    Overlap o;
    for(int i = 0; i < in.body; i++)
    {
        int face = -1;
        const float x = excess(in, in.points[i] + d, in.bodyAllowed, weights ? &face : nullptr);
        o.body += x;
        o.deepest = za::fmax(o.deepest, x);
        if(weights && face >= 0 && x > 0.f)
        {
            weights[face] += x;
        }
    }
    for(int f = 0; f < fingers; f++)
    {
        float best = none, bestDeepest = 0.f;
        int bestSample = 0;
        for(int s = 0; s < samples && best > 0.f; s++)
        {
            const int first = in.fingerStart[f] + s * in.perFinger[f];
            float sum = 0.f, deepest = 0.f;
            for(int k = 0; k < in.perFinger[f] && sum < best; k++)
            {
                const float x = excess(in, in.points[first + k] + d, in.fingerAllowed);
                sum += x;
                deepest = za::fmax(deepest, x);
            }
            if(sum < best)
            {
                best = sum;
                bestDeepest = deepest;
                bestSample = s;
            }
        }
        if(best >= none || in.perFinger[f] == 0)
        {
            continue;
        }
        o.fingers += best;
        o.deepest = za::fmax(o.deepest, bestDeepest);
        if(weights && best > 0.f)
        {
            const int first = in.fingerStart[f] + bestSample * in.perFinger[f];
            for(int k = 0; k < in.perFinger[f]; k++)
            {
                int face = -1;
                const float x = excess(in, in.points[first + k] + d, in.fingerAllowed, &face);
                if(face >= 0 && x > 0.f)
                {
                    weights[face] += x;
                }
            }
        }
    }
    return o;
}

// How far the hand must go along `e` for nothing to be in past its allowance (each finger at the curl that clears
// soonest); `none` if it can't.
float need(const Inputs& in, const glm::vec3& e)
{
    float t = 0.f;
    for(int i = 0; i < in.body && t < none; i++)
    {
        t = za::fmax(t, reach(in, in.points[i], e, in.bodyAllowed));
    }
    for(int f = 0; f < fingers && t < none; f++)
    {
        float best = none;
        for(int s = 0; s < samples && best > 0.f; s++)
        {
            const int first = in.fingerStart[f] + s * in.perFinger[f];
            float most = 0.f;
            for(int k = 0; k < in.perFinger[f] && most < best; k++)
            {
                most = za::fmax(most, reach(in, in.points[first + k], e, in.fingerAllowed));
            }
            best = za::fmin(best, most);
        }
        t = za::fmax(t, in.perFinger[f] > 0 ? best : 0.f);
    }
    return t;
}

// The fit itself on `in`: the hand's move (the prop's frame), the overlap before and after, the ways tried and measured.
glm::vec3 run(const Inputs& in, Overlap& before, Overlap& after, int& directions, int& measured)
{
    za::Array<float, maxPlanes> weights{};
    const int planes = in.hulls > 0 ? in.hullEnds[in.hulls - 1] : 0;
    for(int j = 0; j < planes; j++)
    {
        weights[j] = 0.f;
    }
    before = overlap(in, glm::vec3{0.f}, weights.data());
    after = before;
    directions = measured = 0;
    if(before.total() <= 0.f || in.cap <= 0.f)
    {
        return glm::vec3{0.f};
    }

    // The ways out: the depth-weighted sum of the sunk points' nearest faces, the most weighted of them, their first two's
    // middle.
    za::Array<glm::vec3, maxDirections> dirs{};
    int n = 0;
    const auto add = [&](const glm::vec3& v) {
        const float l = glm::length(v);
        if(l < 1e-4f || n >= maxDirections)
        {
            return;
        }
        const glm::vec3 e = v / l;
        for(int i = 0; i < n; i++)
        {
            if(glm::dot(dirs[i], e) > 0.995f)
            {
                return;
            }
        }
        dirs[n++] = e;
    };
    glm::vec3 sum{0.f};
    for(int j = 0; j < planes; j++)
    {
        sum += glm::vec3{in.planes[j]} * weights[j];
    }
    add(sum);
    int top[maxDirections - 2]{};
    int tops = 0;
    for(; tops < maxDirections - 2; tops++)
    {
        int best = -1;
        for(int j = 0; j < planes; j++)
        {
            bool taken = false;
            for(int i = 0; i < tops; i++)
            {
                taken = taken || top[i] == j;
            }
            best = !taken && weights[j] > 0.f && (best < 0 || weights[j] > weights[best]) ? j : best;
        }
        if(best < 0)
        {
            break;
        }
        top[tops] = best;
        add(glm::vec3{in.planes[best]});
    }
    if(tops >= 2)
    {
        add(glm::vec3{in.planes[top[0]]} + glm::vec3{in.planes[top[1]]});
    }
    directions = n;

    // How far each must go; measured in that order (the least move first), at most vr_grip_fit_reach: the first that
    // clears is kept, else the one in least.
    za::Array<float, maxDirections> t{};
    za::Array<int, maxDirections> order{};
    for(int i = 0; i < n; i++)
    {
        t[i] = need(in, dirs[i]);
        int k = i;
        for(; k > 0 && t[order[k - 1]] > t[i]; k--)
        {
            order[k] = order[k - 1];
        }
        order[k] = i;
    }
    glm::vec3 move{0.f};
    for(int m = 0; m < n && m < mostMeasured; m++)
    {
        const int i = order[m];
        const glm::vec3 d = dirs[i] * za::fmin(t[i] + 0.01f, in.cap);
        const Overlap o = overlap(in, d, nullptr);
        measured++;
        if(o.total() < after.total() - 1e-4f)
        {
            after = o;
            move = d;
        }
        if(o.total() <= 0.f)
        {
            break;
        }
    }
    return move;
}

// The prop's shape into `in`: its hulls' faces (or its drawn box), each pushed out to the drawn vertices `drawn` (its
// frame) where they stand out of it (a vertex within drawnReachCm of the hull); the most a face moved (units). False: none.
bool makeShape(int num, const glm::vec3& lo, const glm::vec3& hi, const za::Vector<glm::vec3>* drawn, Inputs& in, bool& hull,
    float& grown)
{
    grown = 0.f;
    int planes = box3d::hullPlanes(num, in.planes.data(), maxPlanes, in.hullEnds.data(), maxHulls, in.hulls);
    hull = planes > 0;
    if(!hull)
    {
        if(glm::any(glm::greaterThanEqual(lo, hi)))
        {
            return false;
        }
        for(int a = 0; a < 3; a++)
        {
            glm::vec3 nrm{0.f};
            nrm[a] = 1.f;
            in.planes[2 * a] = glm::vec4{nrm, hi[a]};
            in.planes[2 * a + 1] = glm::vec4{-nrm, -lo[a]};
        }
        planes = 6;
        in.hulls = 1;
        in.hullEnds[0] = 6;
    }
    if(!drawn || drawn->empty())
    {
        return true;
    }
    const float margin = drawnReachCm * 0.01f * units::metresToUnits();
    const size_t count = drawn->size();
    const size_t stride = za::max(static_cast<size_t>(1), (count + drawnMost - 1) / drawnMost);
    if(in.hulls == 1)
    {
        // One hull (most props): each face out to the drawn model's furthest point along it (no further than the margin).
        for(int j = 0; j < planes; j++)
        {
            glm::vec4& pl = in.planes[j];
            float most = pl.w;
            for(size_t i = 0; i < count; i += stride)
            {
                const glm::vec3& v = (*drawn)[i];
                most = za::fmax(most, pl.x * v.x + pl.y * v.y + pl.z * v.z);
            }
            most = za::fmin(most, pl.w + margin);
            grown = za::fmax(grown, most - pl.w);
            pl.w = most;
        }
        return true;
    }
    for(size_t i = 0; i < count; i += stride)
    {
        const glm::vec3& v = (*drawn)[i];
        int begin = 0;
        for(int k = 0; k < in.hulls; k++)
        {
            const int end = in.hullEnds[k];
            float out = -none;
            for(int j = begin; j < end; j++)
            {
                const glm::vec4& pl = in.planes[j];
                out = za::fmax(out, pl.x * v.x + pl.y * v.y + pl.z * v.z - pl.w);
            }
            if(out > 0.f && out <= margin)
            {
                for(int j = begin; j < end; j++)
                {
                    glm::vec4& pl = in.planes[j];
                    const float s = pl.x * v.x + pl.y * v.y + pl.z * v.z;
                    if(s > pl.w)
                    {
                        grown = za::fmax(grown, s - pl.w);
                        pl.w = s;
                    }
                }
            }
            begin = end;
        }
    }
    return true;
}

} // namespace

void setHandRig(int hand, const glm::mat4* rigToHand, const handrig::Pose& pose)
{
    if(hand != 0 && hand != 1)
    {
        return;
    }
    HandRig& h = handRigs[hand];
    h.valid = rigToHand != nullptr;
    if(h.valid)
    {
        h.rigToHand = *rigToHand;
        (void)rigPointsNow(); // (made with the rig, not at a grab)
        h.pose = pose;
    }
}

const Result& last()
{
    return lastResult;
}

bool fit(int num, int hand, bool local, bool twoHand, const glm::mat3& handToProp, const glm::vec3& handInProp, const glm::vec3& lo,
    const glm::vec3& hi, const za::Vector<glm::vec3>* drawn, glm::vec3& move)
{
    move = glm::vec3{0.f};
    if(!vr_grip_fit.value || !local || (hand != 0 && hand != 1) || !handRigs[hand].valid)
    {
        return false;
    }
    const za::I64 t0 = za::Clock::nowNanoseconds();
    const RigPoints& r = rigPointsNow();
    const HandRig& h = handRigs[hand];
    if(r.body.empty() || r.length <= 0.f)
    {
        return false;
    }
    Inputs& in = inputs;
    if(!drawn && num > 0 && sv.active && num < sv.qcvm.num_edicts)
    {
        edict_t* const e = reinterpret_cast<edict_t*>(reinterpret_cast<byte*>(sv.qcvm.edicts) + num * sv.qcvm.edict_size);
        drawn = held::drawnVertices(e, scratch.fitProp) ? &scratch.fitProp : nullptr;
    }
    bool hull = false;
    float grown = 0.f;
    const bool shaped = makeShape(num, lo, hi, drawn, in, hull, grown);
    const za::I64 tShape = za::Clock::nowNanoseconds();
    if(!shaped)
    {
        return false;
    }

    // The drawn hand's points, posed: the palm's (rigid with it) and each finger's at each curl.
    const za::Vector<handrig::Vertex>& vs = handrig::rig().vertices;
    const glm::mat4& toHand = h.rigToHand;
    const auto place = [&](const glm::vec3& p) { return handToProp * glm::vec3{toHand * glm::vec4{p, 1.f}} + handInProp; };
    in.body = static_cast<int>(r.body.size());
    int next = in.body;
    for(int f = 0; f < fingers; f++)
    {
        in.perFinger[f] = static_cast<int>(r.finger[f].size());
        in.fingerStart[f] = next;
        next += samples * in.perFinger[f];
    }
    for(int s = 0; s < samples; s++)
    {
        handrig::Pose p = h.pose;
        for(int f = 0; f < fingers; f++)
        {
            for(float& c : p.curl[f])
            {
                c = sampleCurl[s];
            }
        }
        p.metacarpal = glm::quat{1.f, 0.f, 0.f, 0.f};
        handrig::Posed posed;
        handrig::pose(p, posed);
        if(s == 0)
        {
            for(int i = 0; i < in.body; i++)
            {
                in.points[i] = place(blend(posed, vs[static_cast<size_t>(r.body[static_cast<size_t>(i)])]));
            }
        }
        for(int f = 0; f < fingers; f++)
        {
            for(int k = 0; k < in.perFinger[f]; k++)
            {
                in.points[in.fingerStart[f] + s * in.perFinger[f] + k] =
                    place(blend(posed, vs[static_cast<size_t>(r.finger[f][static_cast<size_t>(k)])]));
            }
        }
    }
    const za::I64 tPose = za::Clock::nowNanoseconds();
    const float cm = 0.01f * units::metresToUnits();
    in.bodyAllowed = za::fmax(vr_grip_fit_palm_depth.value, 0.f) * cm;
    in.fingerAllowed = za::fmax(vr_grip_fit_finger_depth.value, 0.f) * cm;
    in.cap = za::fmax(vr_grip_fit_reach.value, 0.f) * r.length * glm::length(glm::vec3{toHand[0]});

    Result& res = lastResult;
    res = Result{};
    Overlap before, after;
    move = run(in, before, after, res.directions, res.measured);
    res.ran = true;
    res.num = num;
    res.hand = hand;
    res.twoHand = twoHand;
    res.hull = hull;
    res.move = move;
    res.before = before.total() / cm;
    res.beforeFingers = before.fingers / cm;
    res.deepestBefore = before.deepest / cm;
    res.after = after.total() / cm;
    res.afterFingers = after.fingers / cm;
    res.deepestAfter = after.deepest / cm;
    res.cap = in.cap;
    res.points = next;
    res.planes = in.hulls > 0 ? in.hullEnds[in.hulls - 1] : 0;
    res.grown = grown / cm;
    res.micros = static_cast<double>(za::Clock::nowNanoseconds() - t0) / 1000.0;
    res.shapeMicros = static_cast<double>(tShape - t0) / 1000.0;
    res.poseMicros = static_cast<double>(tPose - tShape) / 1000.0;
    if(developer.value || vr_debug_carry.value)
    {
        Con_Printf("gripfit: %d %s hand%s: overlap %.1f cm (fingers %.1f), deepest %.2f -> %.1f (%.1f), %.2f cm; moved %.2f cm "
                   "(most %.1f), %d ways, %d measured\n",
            num, hand ? "main" : "off", twoHand ? " (second of two)" : "", res.before, res.beforeFingers, res.deepestBefore,
            res.after, res.afterFingers, res.deepestAfter, glm::length(move) / cm, in.cap / cm, res.directions, res.measured);
        Con_Printf("gripfit: %d points, %d %s faces (pushed out to the model up to %.1f cm), %.1f us (shape %.1f, pose %.1f)\n",
            res.points, res.planes, hull ? "hull" : "box", res.grown, res.micros, res.shapeMicros, res.poseMicros);
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
    const float cm = 0.01f * units::metresToUnits();
    Con_Printf("vr_grip_fit_last: entity %d, %s hand%s: overlap %.1f cm (fingers %.1f) -> %.1f (%.1f), deepest %.2f -> %.2f cm\n",
        r.num, r.hand ? "main" : "off", r.twoHand ? " (second of two)" : "", r.before, r.beforeFingers, r.after, r.afterFingers,
        r.deepestBefore, r.deepestAfter);
    Con_Printf("vr_grip_fit_last: moved %.2f %.2f %.2f cm (prop frame; most %.1f), %d ways, %d measured, %d points, %d %s faces, "
               "%.1f us\n",
        r.move.x / cm, r.move.y / cm, r.move.z / cm, r.cap / cm, r.directions, r.measured, r.points, r.planes,
        r.hull ? "hull" : "box", r.micros);

    // The worst case: the same inputs with every point sunk in every hull (nothing ever clears), 1000 times.
    Inputs& worst = scratch.shape;
    worst = inputs;
    worst.bodyAllowed = worst.fingerAllowed = -none * 1e-10f;
    constexpr int runs = 1000;
    Overlap before, after;
    int directions = 0, measured = 0;
    za::I64 most = 0, least = 0, total = 0;
    int slow = 0; // (over 100 us: the thread put off by the system, most likely)
    for(int i = 0; i < runs; i++)
    {
        const za::I64 t0 = za::Clock::nowNanoseconds();
        (void)run(worst, before, after, directions, measured);
        const za::I64 t = za::Clock::nowNanoseconds() - t0;
        total += t;
        most = za::max(most, t);
        least = i == 0 ? t : za::min(least, t);
        slow += t > 100000 ? 1 : 0;
    }
    Con_Printf("vr_grip_fit_last: worst case (%d ways, %d measured) %.2f us mean, %.2f least, %.2f most, %d of %d over 100 us\n",
        directions, measured, static_cast<double>(total) / runs / 1000.0, static_cast<double>(least) / 1000.0,
        static_cast<double>(most) / 1000.0, slow, runs);
}

bool drawnOverlap(int hand, DrawnOverlap& out)
{
    out = DrawnOverlap{};
    const int num = held::heldEntity(hand);
    if(!sv.active || num <= 0 || num >= sv.qcvm.num_edicts)
    {
        return false;
    }
    Scratch& d = scratch;
    edict_t* const e = reinterpret_cast<edict_t*>(reinterpret_cast<byte*>(sv.qcvm.edicts) + num * sv.qcvm.edict_size);
    const glm::vec3 lo{e->v.mins[0], e->v.mins[1], e->v.mins[2]}, hi{e->v.maxs[0], e->v.maxs[1], e->v.maxs[2]};
    bool hull = false;
    float grown = 0.f;
    const bool drawn = held::drawnVertices(e, d.prop);
    if(!makeShape(num, lo, hi, drawn ? &d.prop : nullptr, d.shape, hull, grown) || !hull ||
        !view::drawnHandSurface(hand, d.hand, &d.parts))
    {
        return false;
    }
    const int index = static_cast<int>(e->v.modelindex);
    const qmodel_t* const model = index > 0 && index < MAX_MODELS ? sv.models[index] : nullptr;
    const glm::mat3 toProp = glm::transpose(held::axesFromAngles(e->v.angles, model && model->type == mod_brush));
    const glm::vec3 origin{e->v.origin[0], e->v.origin[1], e->v.origin[2]};
    const float cm = 0.01f * units::metresToUnits();
    out.num = num;
    for(size_t i = 0; i < d.hand.size(); i++)
    {
        const float depth = excess(d.shape, toProp * (glm::vec3{d.hand[i]} - origin), 0.f) / cm;
        const view::HandPart part = i < d.parts.size() ? d.parts[i] : view::HandPart::Hand;
        const bool finger = part == view::HandPart::Fingers || part == view::HandPart::Fingertips || part == view::HandPart::Thumb;
        DrawnOverlap::Part& p = finger ? out.fingers : out.body;
        p.points++;
        if(depth > 0.f)
        {
            p.inside++;
            p.sum += depth;
            p.deepest = za::fmax(p.deepest, depth);
        }
    }
    if(developer.value >= 2.f && lastResult.ran && lastResult.num == num) // (a frame check: the drawn hand's middle against the fit's)
    {
        glm::vec3 drawnMid{0.f}, fitMid{0.f};
        for(const glm::vec4& w : d.hand)
        {
            drawnMid += toProp * (glm::vec3{w} - origin);
        }
        drawnMid /= static_cast<float>(za::max(static_cast<int>(d.hand.size()), 1));
        for(int i = 0; i < inputs.body; i++)
        {
            fitMid += inputs.points[i] + lastResult.move;
        }
        fitMid /= static_cast<float>(za::max(inputs.body, 1));
        Con_Printf("  middles (prop frame): drawn %.1f %.1f %.1f, the fit's palm %.1f %.1f %.1f\n", drawnMid.x, drawnMid.y,
            drawnMid.z, fitMid.x, fitMid.y, fitMid.z);
    }
    return true;
}

void drawn_f()
{
    for(int hand = 1; hand >= 0; hand--)
    {
        DrawnOverlap o;
        if(!drawnOverlap(hand, o))
        {
            continue;
        }
        const auto line = [](const char* what, const DrawnOverlap::Part& p) {
            Con_Printf("  %s: %d of %d points in, deepest %.2f cm, total %.1f cm\n", what, p.inside, p.points, p.deepest, p.sum);
        };
        Con_Printf("vr_grip_fit_drawn: %s hand, entity %d, the drawn hand in its shape (hulls out to the model):\n",
            hand ? "main" : "off", o.num);
        line("hand (palm, back, knuckles)", o.body);
        line("fingers and thumb", o.fingers);
    }
}

} // namespace qvr::gripfit
