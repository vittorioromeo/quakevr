// vr_ropesim.cpp -- see vr_ropesim.hpp.
//
// The chain: points `rest` apart (vr_grapple_rope_spacing, the rope's length over a whole number of pieces, at most
// maxPieces), its ends held at the gun and the hook. Each server frame: Verlet (gravity, a little drag), then
// vr_grapple_rope_iterations passes of the pieces' lengths (each piece pulled back to `rest`, the ends fixed), then each
// point swept from where it was to where it went (a sphere of vr_grapple_rope_radius, Box3D's shapes: the world's mesh,
// doors and lifts, props; not the rope's two ends' entities): stopped against what it meets, sliding (its speed into
// the surface gone, half its speed along it); a point something moved into (a prop dropped on it) is put back on top.
// The pieces are not swept (only their points): a sharp edge can cut a piece's corner by a little (finer spacing, less).
//
// The taut path: from the game's end along the chain to the hook, keeping to the points a straight line can't skip (the
// line of sight from the last corner kept, to the chain's points in turn): the rope pulled tight along where it lies.
// Nothing in the way: one line of sight, straight from end to end (what the QC did before). The QC's rope measures and
// pulls along it (vr_grapple.qc VR_Grapple_RopeShape).
//
// The chain is as long as the path from the gun (the muzzle, not the body the game holds by) plus the slack the game's
// rope has: taut in the game, taut drawn.

#include "vr_ropesim.hpp"
#include "vr_box3d.hpp"
#include "vr_cvars.hpp"
#include "vr_profile.hpp"
#include "vr_protocol.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>

using namespace qvr;

namespace
{

constexpr int maxPieces = 96;     // points in a chain at most (a long rope's pieces are longer)
constexpr int maxCorners = 16;    // corners of a taut path at most
constexpr float drag = 0.985f;    // of a point's speed kept each frame (the air)
constexpr float slide = 0.5f;     // of its speed along a surface a point keeps, touching it
constexpr float sightSlack = 1.f; // units short of a point a line of sight may stop (the point is on the surface)

struct Rope
{
    std::vector<glm::vec3> p;    // the chain's points (p[0] the gun, the last the hook)
    std::vector<glm::vec3> prev; // last frame's (Verlet)
    std::vector<glm::vec3> from; // this frame's start (the sweep)
    float rest{0.f};             // a piece's length
    int frame{-1};               // the server frame stepped last (sv.qcvm.time's)
    double time{0.0};
    ropesim::Shape shape;
};

std::unordered_map<int, Rope> ropes;
std::vector<glm::vec3> resampled; // (resample)
std::vector<glm::vec3> taut;      // (tautPath: the points, the game's end first)
int countedFrame = -1;
int countRopes = 0;
int countPoints = 0;

[[nodiscard]] float spacing()
{
    return std::clamp(vr_grapple_rope_spacing.value, 2.f, 256.f);
}

[[nodiscard]] int iterations()
{
    return std::clamp(static_cast<int>(vr_grapple_rope_iterations.value), 1, 64);
}

[[nodiscard]] float radius()
{
    return std::clamp(vr_grapple_rope_radius.value, 0.1f, 8.f);
}

// The polyline `pts` as `n` + 1 points evenly along it (its ends kept).
void resample(std::vector<glm::vec3>& pts, int n)
{
    float total = 0.f;
    for(std::size_t i = 1; i < pts.size(); i++)
    {
        total += glm::distance(pts[i - 1], pts[i]);
    }
    resampled.clear();
    resampled.push_back(pts.front());
    std::size_t seg = 1;
    float segStart = 0.f;
    for(int k = 1; k < n; k++)
    {
        const float want = total * static_cast<float>(k) / static_cast<float>(n);
        while(seg + 1 < pts.size() && segStart + glm::distance(pts[seg - 1], pts[seg]) < want)
        {
            segStart += glm::distance(pts[seg - 1], pts[seg]);
            seg++;
        }
        const float len = glm::distance(pts[seg - 1], pts[seg]);
        const float t = len > 1e-4f ? std::clamp((want - segStart) / len, 0.f, 1.f) : 0.f;
        resampled.push_back(glm::mix(pts[seg - 1], pts[seg], t));
    }
    resampled.push_back(pts.back());
    pts = resampled;
}

// Whether `b` is seen from `a` (a thin line of sight; stopping just short of `b` counts: it lies on a surface).
[[nodiscard]] bool sees(const glm::vec3& a, const glm::vec3& b, int skipA, int skipB)
{
    box3d::RopeHit hit;
    if(!box3d::ropeCast(a, b, 0.25f * radius(), skipA, skipB, hit))
    {
        return true;
    }
    const float d = glm::distance(a, b);
    return hit.fraction * d >= d - sightSlack;
}

// The taut path from `game` along the chain (its inner points) to `end`: its shape.
ropesim::Shape tautPath(const Rope& r, const glm::vec3& game, const glm::vec3& end, int skipA, int skipB)
{
    taut.clear();
    taut.push_back(game);
    for(std::size_t i = 1; i + 1 < r.p.size(); i++)
    {
        taut.push_back(r.p[i]);
    }
    taut.push_back(end);
    const std::size_t last = taut.size() - 1;

    glm::vec3 corners[maxCorners];
    int n = 0;
    std::size_t c = 0;
    while(n < maxCorners && !sees(taut[c], taut[last], skipA, skipB))
    {
        std::size_t j = c + 1;
        while(j + 1 < last && sees(taut[c], taut[j + 1], skipA, skipB))
        {
            j++;
        }
        if(j >= last)
        {
            break;
        }
        corners[n++] = taut[j];
        c = j;
    }

    // Pulled tight: each corner slides towards the straight line between its neighbours as far as both lines stay clear,
    // coming to rest against what the rope goes round (a sagging chain's corner is low, a lagging one off the edge); one
    // the straight line doesn't need is gone (the rope came off it).
    for(int pass = 0; pass < 2 && n > 0; pass++)
    {
        for(int k = 0; k < n; k++)
        {
            const glm::vec3 prev = k == 0 ? game : corners[k - 1];
            const glm::vec3 next = k == n - 1 ? end : corners[k + 1];
            const glm::vec3 pn = next - prev;
            const float pn2 = glm::dot(pn, pn);
            const float t = pn2 > 1e-4f ? std::clamp(glm::dot(corners[k] - prev, pn) / pn2, 0.f, 1.f) : 0.f;
            const glm::vec3 target = prev + pn * t;
            if(sees(prev, next, skipA, skipB))
            {
                for(int m = k; m + 1 < n; m++)
                {
                    corners[m] = corners[m + 1];
                }
                n--;
                k--;
                continue;
            }
            float lo = 0.f, hi = 1.f;
            for(int step = 0; step < 6; step++)
            {
                const float mid = 0.5f * (lo + hi);
                const glm::vec3 x = glm::mix(corners[k], target, mid);
                if(sees(prev, x, skipA, skipB) && sees(x, next, skipA, skipB))
                {
                    lo = mid;
                }
                else
                {
                    hi = mid;
                }
            }
            corners[k] = glm::mix(corners[k], target, lo);
        }
    }

    ropesim::Shape s;
    glm::vec3 at = game;
    for(int k = 0; k < n; k++)
    {
        s.path += glm::distance(at, corners[k]);
        at = corners[k];
    }
    s.path += glm::distance(at, end);
    s.pivotA = n > 0 ? corners[0] : end;
    s.beyondA = s.path - glm::distance(game, s.pivotA);
    s.pivotB = n > 0 ? corners[n - 1] : game;
    s.beyondB = s.path - glm::distance(end, s.pivotB);
    return s;
}

// A straight chain of `n` pieces from `a` to `b`, at rest.
void straight(Rope& r, const glm::vec3& a, const glm::vec3& b, int n)
{
    r.p.resize(static_cast<std::size_t>(n) + 1);
    for(int i = 0; i <= n; i++)
    {
        r.p[static_cast<std::size_t>(i)] = glm::mix(a, b, static_cast<float>(i) / static_cast<float>(n));
    }
    r.prev = r.p;
}

void count(int points)
{
    if(countedFrame != host_framecount)
    {
        countedFrame = host_framecount;
        countRopes = 0;
        countPoints = 0;
    }
    countRopes++;
    countPoints += points;
}

// vr_grapple_rope_dump: each rope's points and taut path (tests).
void dump_f()
{
    for(const auto& [num, r] : ropes)
    {
        Con_Printf("rope %d: %d points %.1f apart; taut path %.1f, first corners %.1f %.1f %.1f (%.1f beyond) and %.1f %.1f "
                   "%.1f (%.1f beyond)\n",
            num, static_cast<int>(r.p.size()), static_cast<double>(r.rest), static_cast<double>(r.shape.path),
            static_cast<double>(r.shape.pivotA.x), static_cast<double>(r.shape.pivotA.y), static_cast<double>(r.shape.pivotA.z),
            static_cast<double>(r.shape.beyondA), static_cast<double>(r.shape.pivotB.x), static_cast<double>(r.shape.pivotB.y),
            static_cast<double>(r.shape.pivotB.z), static_cast<double>(r.shape.beyondB));
        int inside = 0; // (inner points in the world or a prop: none, it can't pass through)
        for(std::size_t i = 1; i + 1 < r.p.size(); i++)
        {
            inside += box3d::ropeOverlaps(r.p[i], 0.5f * radius(), 0, 0) ? 1 : 0; // (half: one lying is as close as a cast leaves it)
        }
        Con_Printf("rope %d: %d inner points inside the world or a prop\n", num, inside);
        for(std::size_t i = 0; i < r.p.size(); i++)
        {
            Con_Printf("rope %d point %d: %.1f %.1f %.1f\n", num, static_cast<int>(i), static_cast<double>(r.p[i].x),
                static_cast<double>(r.p[i].y), static_cast<double>(r.p[i].z));
        }
    }
}

// vr_grapple_rope_cast x y z x y z [radius]: the rope's sweep between two points (tests).
void cast_f()
{
    if(Cmd_Argc() < 7)
    {
        Con_Printf("vr_grapple_rope_cast x y z x y z [radius]\n");
        return;
    }
    const glm::vec3 a{Q_atof(Cmd_Argv(1)), Q_atof(Cmd_Argv(2)), Q_atof(Cmd_Argv(3))};
    const glm::vec3 b{Q_atof(Cmd_Argv(4)), Q_atof(Cmd_Argv(5)), Q_atof(Cmd_Argv(6))};
    const float rad = Cmd_Argc() > 7 ? Q_atof(Cmd_Argv(7)) : radius();
    box3d::RopeHit hit;
    const bool any = box3d::ropeCast(a, b, rad, 0, 0, hit);
    Con_Printf("rope cast: %s, fraction %.3f, centre %.1f %.1f %.1f, normal %.2f %.2f %.2f; overlaps at the end: %d\n",
        any ? "hit" : "clear", static_cast<double>(hit.fraction), static_cast<double>(hit.centre.x),
        static_cast<double>(hit.centre.y), static_cast<double>(hit.centre.z), static_cast<double>(hit.normal.x),
        static_cast<double>(hit.normal.y), static_cast<double>(hit.normal.z), box3d::ropeOverlaps(b, rad, 0, 0) ? 1 : 0);
}

} // namespace

namespace qvr::ropesim
{

Shape step(edict_t* hook, const glm::vec3& gun, const glm::vec3& game, const glm::vec3& end, float length, int skipA,
    int skipB)
{
    Rope& r = ropes[NUM_FOR_EDICT(hook)];
    const int frame = static_cast<int>(std::lround(sv.qcvm.time * 1000.0));
    if(r.frame == frame)
    {
        return r.shape;
    }
    QVR_PROFILE("grapple rope sim");
    static bool registered = false;
    if(!registered)
    {
        registered = true;
        Cmd_AddCommand("vr_grapple_rope_dump", dump_f);
        Cmd_AddCommand("vr_grapple_rope_cast", cast_f);
    }
    const float dt = static_cast<float>(std::clamp(sv.qcvm.time - r.time, 0.0, 0.1));
    const bool fresh = r.p.size() < 2 || sv.qcvm.time - r.time > 0.5 || glm::distance(r.p.front(), gun) > 256.f ||
                       glm::distance(r.p.back(), end) > 256.f;
    r.frame = frame;
    r.time = sv.qcvm.time;

    if(fresh)
    {
        r.shape = Shape{};
        r.shape.path = glm::distance(game, end);
        r.shape.pivotA = end;
        r.shape.pivotB = game;
    }
    // The chain's length: the path from the gun (the game's, less its first stretch from the body, plus the gun's), and the
    // game's slack.
    const float slack = std::max(0.f, length - r.shape.path);
    const glm::vec3 firstCorner = fresh ? end : r.shape.pivotA;
    const float fromGun = fresh ? glm::distance(gun, end)
                                : std::max(0.f, r.shape.path - glm::distance(game, firstCorner)) + glm::distance(gun, firstCorner);
    const float chain = std::max(1.f, fromGun + slack);
    const int n = std::clamp(static_cast<int>(std::ceil(chain / spacing())), 1, maxPieces);
    if(fresh)
    {
        straight(r, gun, end, n);
    }
    else if(static_cast<int>(r.p.size()) != n + 1)
    {
        r.p.front() = gun;
        r.p.back() = end;
        resample(r.p, n);
        r.prev.front() = gun;
        r.prev.back() = end;
        resample(r.prev, n);
    }
    r.rest = chain / static_cast<float>(n);

    // Verlet: the inner points fall, a little slowed by the air.
    const glm::vec3 fall{0.f, 0.f, -sv_gravity.value * dt * dt};
    r.from = r.p;
    for(int i = 1; i < n; i++)
    {
        glm::vec3& q = r.p[static_cast<std::size_t>(i)];
        const glm::vec3 v = (q - r.prev[static_cast<std::size_t>(i)]) * drag;
        r.prev[static_cast<std::size_t>(i)] = q;
        q += v + fall;
    }
    r.p.front() = gun;
    r.p.back() = end;

    // The pieces' lengths.
    for(int it = 0; it < iterations(); it++)
    {
        for(int i = 0; i < n; i++)
        {
            glm::vec3& a = r.p[static_cast<std::size_t>(i)];
            glm::vec3& b = r.p[static_cast<std::size_t>(i) + 1];
            const glm::vec3 d = b - a;
            const float len = glm::length(d);
            if(len < 1e-5f)
            {
                continue;
            }
            const float wa = i == 0 ? 0.f : 1.f;
            const float wb = i + 1 == n ? 0.f : 1.f;
            if(wa + wb <= 0.f)
            {
                continue;
            }
            const glm::vec3 corr = d * ((len - r.rest) / (len * (wa + wb)));
            a += corr * wa;
            b -= corr * wb;
        }
    }

    // What it can't pass through: each inner point swept from where it was; one something moved into, put back on top.
    const float rad = radius();
    for(int i = 1; i < n; i++)
    {
        glm::vec3& q = r.p[static_cast<std::size_t>(i)];
        const glm::vec3 was = r.from[static_cast<std::size_t>(i)];
        box3d::RopeHit hit;
        if(box3d::ropeCast(was, q, rad, skipA, skipB, hit))
        {
            const glm::vec3 moved = q - was;
            q = hit.centre + hit.normal * 0.05f;
            // Its speed into the surface gone, some of its speed along it kept (next frame's Verlet).
            const glm::vec3 along = moved - hit.normal * glm::dot(moved, hit.normal);
            r.prev[static_cast<std::size_t>(i)] = q - along * slide;
        }
        else if(box3d::ropeOverlaps(q, rad, skipA, skipB))
        {
            const glm::vec3 above = q + glm::vec3{0.f, 0.f, 32.f};
            if(box3d::ropeCast(above, q, rad, skipA, skipB, hit) && hit.fraction > 0.f)
            {
                q = hit.centre + hit.normal * 0.05f;
                r.prev[static_cast<std::size_t>(i)] = q;
            }
        }
    }

    r.shape = tautPath(r, game, end, skipA, skipB);
    count(n + 1);
    if(vr_grapple_debug.value >= 2 && static_cast<int>(sv.qcvm.time * 2.0) != static_cast<int>((sv.qcvm.time - dt) * 2.0))
    {
        Con_Printf("grapple: rope %d simulated: %d points %.1f apart, chain %.1f (slack %.1f), taut path %.1f, first corner %.1f "
                   "%.1f %.1f (%.1f beyond)\n",
            NUM_FOR_EDICT(hook), n + 1, static_cast<double>(r.rest), static_cast<double>(chain), static_cast<double>(slack),
            static_cast<double>(r.shape.path), static_cast<double>(r.shape.pivotA.x), static_cast<double>(r.shape.pivotA.y),
            static_cast<double>(r.shape.pivotA.z), static_cast<double>(r.shape.beyondA));
    }
    return r.shape;
}

const Shape& shape(edict_t* hook)
{
    static const Shape none;
    const auto it = ropes.find(NUM_FOR_EDICT(hook));
    return it == ropes.end() ? none : it->second.shape;
}

void send(edict_t* hook, edict_t* owner, int beamId)
{
    const auto it = ropes.find(NUM_FOR_EDICT(hook));
    if(it == ropes.end() || sv.state != ss_active)
    {
        return;
    }
    const std::vector<glm::vec3>& p = it->second.p;
    const int count = static_cast<int>(std::min<std::size_t>(p.size(), 255));
    if(count < 2 || sv.datagram.cursize + 8 + count * 12 > MAX_DATAGRAM - 64)
    {
        return;
    }
    // TODO QVR: bandwidth. Every point is sent to every client each frame (up to ~1.2 KB per rope). Send only the ends
    // and the wrap corners (the client simulates the slack), quantize the coordinates, and send only on change or to nearby clients.
    MSG_WriteByte(&sv.datagram, protocol::svc_quakevr);
    MSG_WriteByte(&sv.datagram, protocol::QVR_SVC_ROPE);
    MSG_WriteShort(&sv.datagram, NUM_FOR_EDICT(owner));
    MSG_WriteByte(&sv.datagram, beamId);
    MSG_WriteByte(&sv.datagram, count);
    for(int i = 0; i < count; i++)
    {
        for(int k = 0; k < 3; k++)
        {
            MSG_WriteCoord(&sv.datagram, p[static_cast<std::size_t>(i)][k], sv.protocolflags);
        }
    }
}

void forget(int num)
{
    ropes.erase(num);
}

void reset()
{
    ropes.clear();
}

void profileCounts(int& nRopes, int& nPoints)
{
    const bool now = countedFrame == host_framecount || countedFrame == host_framecount - 1;
    nRopes = now ? countRopes : 0;
    nPoints = now ? countPoints : 0;
}

} // namespace qvr::ropesim
