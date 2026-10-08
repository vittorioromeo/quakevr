// vr_convex.cpp -- a drawn model's solid in convex pieces; see vr_convex.hpp.

#include "vr_convex.hpp"

#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Container/Array.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "vr_zancle.hpp"

#include <box3d/collision.h>
#include <math.h>

namespace qvr::convex
{
namespace
{

constexpr za::U8 inside = 0;  // a cell of the solid's inside (not reached from the grid's border)
constexpr za::U8 surface = 1; // a cell a triangle passes through
constexpr za::U8 outside = 2; // reached from the border
constexpr int pad = 2;        // empty cells round the model
constexpr int maxCells = 200; // along the longest axis

struct Grid
{
    glm::vec3 origin{0.f};
    float cell{1.f};
    int n[3]{0, 0, 0};
    za::Vector<za::U8> state;
    za::Vector<float> dist; // an outside cell's distance from the solid (cells); the solid's 0

    [[nodiscard]] int index(int x, int y, int z) const { return x + n[0] * (y + n[1] * z); }
    [[nodiscard]] int coord(float v, int axis) const
    {
        return CLAMP(0, static_cast<int>(floorf((v - origin[axis]) / cell)), n[axis] - 1);
    }
    [[nodiscard]] int cellOf(const glm::vec3& p) const { return index(coord(p.x, 0), coord(p.y, 1), coord(p.z, 2)); }
};

void voxelise(const za::Vector<glm::vec3>& corners, Grid& g)
{
    const size_t count = g.state.size();
    for(size_t t = 0; t + 2 < corners.size(); t += 3)
    {
        const glm::vec3 a = corners[t], b = corners[t + 1], c = corners[t + 2];
        const float longest = za::max(glm::distance(a, b), za::max(glm::distance(a, c), glm::distance(b, c)));
        // Half a cell apart: the cells marked are 26-connected, which no 6-connected flood passes.
        const int steps = CLAMP(1, static_cast<int>(ceilf(longest / (g.cell * 0.5f))), 1024);
        for(int i = 0; i <= steps; i++)
        {
            for(int j = 0; j <= steps - i; j++)
            {
                const glm::vec3 p = a + (b - a) * (static_cast<float>(i) / steps) + (c - a) * (static_cast<float>(j) / steps);
                const int k = g.cellOf(p);
                if(static_cast<size_t>(k) < count)
                {
                    g.state[static_cast<size_t>(k)] = surface;
                }
            }
        }
    }
    // The outside: flooded (6-connected) from the border, which the pad leaves empty.
    za::Vector<int> stack;
    const auto reach = [&](int x, int y, int z) {
        const int k = g.index(x, y, z);
        if(g.state[static_cast<size_t>(k)] == inside)
        {
            g.state[static_cast<size_t>(k)] = outside;
            stack.pushBack(k);
        }
    };
    for(int z = 0; z < g.n[2]; z++)
    {
        for(int y = 0; y < g.n[1]; y++)
        {
            for(int x = 0; x < g.n[0]; x++)
            {
                if(x == 0 || y == 0 || z == 0 || x == g.n[0] - 1 || y == g.n[1] - 1 || z == g.n[2] - 1)
                {
                    reach(x, y, z);
                }
            }
        }
    }
    const int layer = g.n[0] * g.n[1];
    while(!stack.empty())
    {
        const int k = stack.back();
        stack.popBack();
        const int x = k % g.n[0], y = (k / g.n[0]) % g.n[1], z = k / layer;
        if(x > 0) reach(x - 1, y, z);
        if(x < g.n[0] - 1) reach(x + 1, y, z);
        if(y > 0) reach(x, y - 1, z);
        if(y < g.n[1] - 1) reach(x, y + 1, z);
        if(z > 0) reach(x, y, z - 1);
        if(z < g.n[2] - 1) reach(x, y, z + 1);
    }
    // Each outside cell's distance from the solid: a two-pass chamfer over the 26 neighbours (1, sqrt 2, sqrt 3).
    constexpr float unreached = 1e9f;
    g.dist.resize(count);
    for(size_t k = 0; k < count; k++)
    {
        g.dist[k] = g.state[k] == outside ? unreached : 0.f;
    }
    const auto pass = [&](int dir) {
        const int x0 = dir > 0 ? 0 : g.n[0] - 1, y0 = dir > 0 ? 0 : g.n[1] - 1, z0 = dir > 0 ? 0 : g.n[2] - 1;
        for(int z = z0; z >= 0 && z < g.n[2]; z += dir)
        {
            for(int y = y0; y >= 0 && y < g.n[1]; y += dir)
            {
                for(int x = x0; x >= 0 && x < g.n[0]; x += dir)
                {
                    const int k = g.index(x, y, z);
                    float& d = g.dist[static_cast<size_t>(k)];
                    if(d == 0.f)
                    {
                        continue;
                    }
                    // The 13 neighbours before it in this pass's order.
                    for(int dz = -1; dz <= 0; dz++)
                    {
                        for(int dy = -1; dy <= 1; dy++)
                        {
                            for(int dx = -1; dx <= 1; dx++)
                            {
                                if(dz == 0 && (dy > 0 || (dy == 0 && dx >= 0)))
                                {
                                    continue;
                                }
                                const int nx = x + dx * dir, ny = y + dy * dir, nz = z + dz * dir;
                                if(nx < 0 || ny < 0 || nz < 0 || nx >= g.n[0] || ny >= g.n[1] || nz >= g.n[2])
                                {
                                    continue;
                                }
                                const int m = (dx != 0) + (dy != 0) + (dz != 0);
                                const float w = m == 1 ? 1.f : m == 2 ? 1.41421f : 1.73205f;
                                d = za::min(d, g.dist[static_cast<size_t>(g.index(nx, ny, nz))] + w);
                            }
                        }
                    }
                }
            }
        }
    };
    pass(1);
    pass(-1);
}

// A box of model space: a piece's. cut[axis][side]: that face is a cut (the solid goes on past it), not the model's
// bounds.
struct Region
{
    glm::vec3 lo{0.f}, hi{0.f};
    bool cut[3][2]{{false, false}, {false, false}, {false, false}};
};

// The triangle a, b, c clipped to the region's box: its corners added to `out`.
void clipTriangle(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const Region& r, za::Vector<b3Vec3>& out)
{
    za::Array<glm::vec3, 16> poly, next;
    int count = 3;
    poly[0] = a;
    poly[1] = b;
    poly[2] = c;
    for(int axis = 0; axis < 3 && count > 0; axis++)
    {
        for(int side = 0; side < 2 && count > 0; side++)
        {
            const float bound = side ? r.hi[axis] : r.lo[axis];
            const auto in = [&](const glm::vec3& p) { return side ? p[axis] <= bound : p[axis] >= bound; };
            int n = 0;
            for(int i = 0; i < count && n < 15; i++)
            {
                const glm::vec3& p = poly[static_cast<size_t>(i)];
                const glm::vec3& q = poly[static_cast<size_t>((i + 1) % count)];
                const bool pin = in(p), qin = in(q);
                if(pin)
                {
                    next[static_cast<size_t>(n++)] = p;
                }
                if(pin != qin && n < 15)
                {
                    const float t = (bound - p[axis]) / (q[axis] - p[axis]);
                    glm::vec3 s = p + (q - p) * t;
                    s[axis] = bound;
                    next[static_cast<size_t>(n++)] = s;
                }
            }
            poly = next;
            count = n;
        }
    }
    for(int i = 0; i < count; i++)
    {
        const glm::vec3& p = poly[static_cast<size_t>(i)];
        out.pushBack(b3Vec3{p.x, p.y, p.z});
    }
}

// The points whose hull is the region's piece of the solid: the triangles clipped to it, and on each cut face the
// inside cells there (their middles brought onto it), so that the pieces meet across the cut.
void regionPoints(const za::Vector<glm::vec3>& corners, const Grid& g, const Region& r, za::Vector<b3Vec3>& out)
{
    out.clear();
    for(size_t t = 0; t + 2 < corners.size(); t += 3)
    {
        const glm::vec3 a = corners[t], b = corners[t + 1], c = corners[t + 2];
        const glm::vec3 lo = glm::min(a, glm::min(b, c)), hi = glm::max(a, glm::max(b, c));
        if(lo.x > r.hi.x || lo.y > r.hi.y || lo.z > r.hi.z || hi.x < r.lo.x || hi.y < r.lo.y || hi.z < r.lo.z)
        {
            continue;
        }
        clipTriangle(a, b, c, r, out);
    }
    for(int axis = 0; axis < 3; axis++)
    {
        const int u = (axis + 1) % 3, v = (axis + 2) % 3;
        const int u0 = static_cast<int>(ceilf((r.lo[u] - g.origin[u]) / g.cell - 0.5f));
        const int u1 = static_cast<int>(floorf((r.hi[u] - g.origin[u]) / g.cell - 0.5f));
        const int v0 = static_cast<int>(ceilf((r.lo[v] - g.origin[v]) / g.cell - 0.5f));
        const int v1 = static_cast<int>(floorf((r.hi[v] - g.origin[v]) / g.cell - 0.5f));
        for(int side = 0; side < 2; side++)
        {
            if(!r.cut[axis][side])
            {
                continue;
            }
            const float plane = side ? r.hi[axis] : r.lo[axis];
            // (The cuts are on the cells' borders: the cell just inside the region.)
            const int ca = static_cast<int>(floorf((plane - g.origin[axis]) / g.cell + (side ? -0.5f : 0.5f)));
            if(ca < 0 || ca >= g.n[axis])
            {
                continue;
            }
            // (Only the outermost of each row and column: those inside them add nothing to the hull.)
            const int cu0 = za::max(u0, 0), cu1 = za::min(u1, g.n[u] - 1);
            const int cv0 = za::max(v0, 0), cv1 = za::min(v1, g.n[v] - 1);
            const auto solid = [&](int cu, int cv) {
                int c[3];
                c[axis] = ca;
                c[u] = cu;
                c[v] = cv;
                return g.state[static_cast<size_t>(g.index(c[0], c[1], c[2]))] == inside;
            };
            const auto add = [&](int cu, int cv) {
                glm::vec3 p;
                p[axis] = plane;
                p[u] = g.origin[u] + (static_cast<float>(cu) + 0.5f) * g.cell;
                p[v] = g.origin[v] + (static_cast<float>(cv) + 0.5f) * g.cell;
                out.pushBack(b3Vec3{p.x, p.y, p.z});
            };
            for(int pass = 0; pass < 2; pass++)
            {
                const int a0 = pass ? cv0 : cu0, a1 = pass ? cv1 : cu1, b0 = pass ? cu0 : cv0, b1 = pass ? cu1 : cv1;
                for(int i = a0; i <= a1; i++)
                {
                    int first = -1, last = -1;
                    for(int j = b0; j <= b1; j++)
                    {
                        if(pass ? solid(j, i) : solid(i, j))
                        {
                            first = first < 0 ? j : first;
                            last = j;
                        }
                    }
                    if(first >= 0)
                    {
                        pass ? add(first, i) : add(i, first);
                        if(last != first)
                        {
                            pass ? add(last, i) : add(i, last);
                        }
                    }
                }
            }
        }
    }
    // (Duplicates out: the triangles share their corners.)
    za::quickSort(out.begin(), out.end(), [](const b3Vec3& p, const b3Vec3& q) {
        return p.x != q.x ? p.x < q.x : p.y != q.y ? p.y < q.y : p.z < q.z;
    });
    size_t kept = 0;
    for(size_t i = 0; i < out.size(); i++)
    {
        if(kept == 0 || out[i].x != out[kept - 1].x || out[i].y != out[kept - 1].y || out[i].z != out[kept - 1].z)
        {
            out[kept++] = out[i];
        }
    }
    out.resize(kept);
}

// How far the points reach out of the hull (the most any is beyond one of its faces).
[[nodiscard]] float shortfall(const b3HullData* hull, const za::Vector<b3Vec3>& points)
{
    const b3Plane* planes = b3GetHullPlanes(hull);
    float most = 0.f;
    for(const b3Vec3& p : points)
    {
        float out = -1e9f;
        for(int i = 0; i < hull->faceCount; i++)
        {
            out = za::max(out, b3Dot(planes[i].normal, p) - planes[i].offset);
        }
        most = za::max(most, out);
    }
    return most;
}

constexpr int maxBudget = 44; // (a hull of triangles of more has more than Box3D's 128 edges)

[[nodiscard]] b3HullData* hullOf(const za::Vector<b3Vec3>& points, float cell);

// The hull of the points: vertices enough to leave none of them more than a third of a cell out (as Box3D allows).
[[nodiscard]] b3HullData* hullOf(const za::Vector<b3Vec3>& points, float cell)
{
    const int count = static_cast<int>(points.size());
    int budget = 24;
    b3HullData* h = hull(points.data(), count, budget);
    while(h && budget < maxBudget && h->vertexCount >= budget && shortfall(h, points) > cell / 3.f)
    {
        budget = za::min(budget * 3 / 2, maxBudget);
        b3HullData* more = b3CreateHull(points.data(), count, budget);
        if(!more)
        {
            break;
        }
        b3DestroyHull(h);
        h = more;
    }
    return h;
}

// The most the hull's surface lies off the solid (units): its faces sampled about a cell apart, each sample's cell's
// distance from the solid (less half a cell: a face on the surface may lie in the cell beside the one it is drawn in).
[[nodiscard]] float gapOf(const b3HullData* hull, const Grid& g)
{
    const b3Vec3* points = b3GetHullPoints(hull);
    const b3HullHalfEdge* edges = b3GetHullEdges(hull);
    const b3HullFace* faces = b3GetHullFaces(hull);
    float most = 0.f;
    za::Array<glm::vec3, 64> poly;
    for(int f = 0; f < hull->faceCount; f++)
    {
        int count = 0;
        const int start = faces[f].edge;
        int e = start;
        do
        {
            const b3Vec3 p = points[edges[e].origin];
            poly[static_cast<size_t>(count++)] = glm::vec3{p.x, p.y, p.z};
            e = edges[e].next;
        } while(e != start && count < 64);
        for(int t = 1; t + 1 < count; t++)
        {
            const glm::vec3 a = poly[0], b = poly[static_cast<size_t>(t)], c = poly[static_cast<size_t>(t + 1)];
            const float longest = za::max(glm::distance(a, b), za::max(glm::distance(a, c), glm::distance(b, c)));
            const int steps = CLAMP(1, static_cast<int>(ceilf(longest / g.cell)), 256);
            for(int i = 0; i <= steps; i++)
            {
                for(int j = 0; j <= steps - i; j++)
                {
                    const glm::vec3 p =
                        a + (b - a) * (static_cast<float>(i) / steps) + (c - a) * (static_cast<float>(j) / steps);
                    most = za::max(most, g.dist[static_cast<size_t>(g.cellOf(p))]);
                }
            }
        }
    }
    return za::max(0.f, most - 0.5f) * g.cell;
}

constexpr int shortlisted = 3; // cuts each way whose halves' hulls are made

struct Candidate
{
    int axis{0};
    float plane{0.f};
    float score{0.f};
};

// The region's cells (as indices, end exclusive: the cuts are on the cells' borders).
void cellRange(const Grid& g, const Region& r, int lo[3], int hi[3])
{
    for(int i = 0; i < 3; i++)
    {
        lo[i] = za::max(0, static_cast<int>(floorf((r.lo[i] - g.origin[i]) / g.cell + 0.01f)));
        hi[i] = za::min(g.n[i], static_cast<int>(ceilf((r.hi[i] - g.origin[i]) / g.cell - 0.01f)));
    }
}

// The cuts worth trying: along each axis, the `shortlisted` cuts whose halves' solid cells' boxes hold the least
// (each slice's box of solid across the axis, put together from either end).
void shortlist(const Grid& g, const Region& r, za::Array<Candidate, 3 * shortlisted>& out, int& count)
{
    count = 0;
    int lo[3], hi[3];
    cellRange(g, r, lo, hi);
    struct Box
    {
        int lo[3]{1 << 30, 1 << 30, 1 << 30};
        int hi[3]{-1, -1, -1};
        void add(const int c[3])
        {
            for(int i = 0; i < 3; i++)
            {
                lo[i] = za::min(lo[i], c[i]);
                hi[i] = za::max(hi[i], c[i]);
            }
        }
        void add(const Box& o)
        {
            for(int i = 0; i < 3; i++)
            {
                lo[i] = za::min(lo[i], o.lo[i]);
                hi[i] = za::max(hi[i], o.hi[i]);
            }
        }
        [[nodiscard]] float volume() const
        {
            return hi[0] < lo[0] ? 0.f
                                 : static_cast<float>(hi[0] - lo[0] + 1) * static_cast<float>(hi[1] - lo[1] + 1) *
                                       static_cast<float>(hi[2] - lo[2] + 1);
        }
    };
    za::Vector<Box> slices[3];
    for(int axis = 0; axis < 3; axis++)
    {
        slices[axis].resize(static_cast<size_t>(za::max(hi[axis] - lo[axis], 0)));
    }
    for(int z = lo[2]; z < hi[2]; z++)
    {
        for(int y = lo[1]; y < hi[1]; y++)
        {
            for(int x = lo[0]; x < hi[0]; x++)
            {
                if(g.state[static_cast<size_t>(g.index(x, y, z))] == outside)
                {
                    continue;
                }
                const int c[3]{x, y, z};
                for(int axis = 0; axis < 3; axis++)
                {
                    slices[axis][static_cast<size_t>(c[axis] - lo[axis])].add(c);
                }
            }
        }
    }
    za::Vector<Box> below;
    for(int axis = 0; axis < 3; axis++)
    {
        const int n = hi[axis] - lo[axis];
        below.resize(static_cast<size_t>(n));
        Box sum;
        for(int k = 0; k < n; k++)
        {
            sum.add(slices[axis][static_cast<size_t>(k)]);
            below[static_cast<size_t>(k)] = sum; // slices 0..k
        }
        Candidate best[shortlisted];
        int kept = 0;
        Box above;
        for(int k = n - 1; k >= 3; k--) // the cut between slices k - 1 and k: at least two cells each side
        {
            above.add(slices[axis][static_cast<size_t>(k)]);
            if(n - k < 2)
            {
                continue;
            }
            const Box& rest = below[static_cast<size_t>(k - 1)];
            if(rest.hi[0] < rest.lo[0] || above.hi[0] < above.lo[0])
            {
                continue; // (no solid on one side)
            }
            const float score = rest.volume() + above.volume();
            int at = kept;
            while(at > 0 && best[at - 1].score > score)
            {
                at--;
            }
            if(at >= shortlisted)
            {
                continue;
            }
            for(int i = za::min(kept, shortlisted - 1); i > at; i--)
            {
                best[i] = best[i - 1];
            }
            best[at] = Candidate{axis, g.origin[axis] + static_cast<float>(lo[axis] + k) * g.cell, score};
            kept = za::min(kept + 1, shortlisted);
        }
        for(int i = 0; i < kept; i++)
        {
            out[static_cast<size_t>(count++)] = best[i];
        }
    }
}

} // namespace

b3HullData* hull(const b3Vec3* points, int count, int budget)
{
    if(count < 4)
    {
        return nullptr;
    }
    for(budget = za::min(budget, B3_MAX_HULL_VERTICES); budget >= 4; budget = budget * 2 / 3)
    {
        if(b3HullData* h = b3CreateHull(points, count, budget))
        {
            return h;
        }
    }
    return nullptr;
}

namespace
{

struct Work
{
    Region region;
    b3HullData* hull{nullptr};
    float gap{0.f};
    bool done{false};
};

} // namespace

bool decompose(const za::Vector<glm::vec3>& corners, const Settings& settings, za::Vector<Piece>& pieces, Report* report)
{
    const double start = Sys_DoubleTime();
    pieces.clear();
    Report local;
    Report& rep = report ? *report : local;
    rep = Report{};
    if(corners.size() < 12)
    {
        return false;
    }
    glm::vec3 lo{1e9f}, hi{-1e9f};
    for(const glm::vec3& p : corners)
    {
        lo = glm::min(lo, p);
        hi = glm::max(hi, p);
    }
    const glm::vec3 size = hi - lo;
    const float longest = za::max(size.x, za::max(size.y, size.z));
    Grid g;
    g.cell = za::max(settings.cell, longest / static_cast<float>(maxCells));
    for(int i = 0; i < 3; i++)
    {
        g.n[i] = static_cast<int>(ceilf(size[i] / g.cell)) + 2 * pad;
    }
    g.origin = lo - glm::vec3{static_cast<float>(pad) * g.cell};
    const size_t count = static_cast<size_t>(g.n[0]) * static_cast<size_t>(g.n[1]) * static_cast<size_t>(g.n[2]);
    rep.cell = g.cell;
    rep.cells = static_cast<int>(count);
    if(longest <= 0.f || count > 4'000'000)
    {
        return false;
    }
    g.state.resize(count);
    for(size_t k = 0; k < count; k++)
    {
        g.state[k] = inside;
    }
    voxelise(corners, g);
    const double voxelised = Sys_DoubleTime();
    size_t solidCells = 0;
    for(size_t k = 0; k < count; k++)
    {
        solidCells += g.state[k] != outside;
    }
    rep.solid = static_cast<float>(solidCells) * g.cell * g.cell * g.cell;

    za::Vector<Work> work;
    za::Vector<b3Vec3> points, pointsA, pointsB;
    Work whole;
    whole.region.lo = lo;
    whole.region.hi = hi;
    regionPoints(corners, g, whole.region, points);
    whole.hull = hullOf(points, g.cell);
    if(!whole.hull)
    {
        rep.ms = (Sys_DoubleTime() - start) * 1000.0;
        return false;
    }
    whole.gap = gapOf(whole.hull, g);
    rep.wholeGap = whole.gap;
    rep.wholeVolume = whole.hull->volume;
    work.pushBack(whole);

    while(static_cast<int>(work.size()) < settings.maxPieces)
    {
        int worst = -1;
        for(size_t i = 0; i < work.size(); i++)
        {
            if(!work[i].done && work[i].gap > settings.tolerance && (worst < 0 || work[i].gap > work[static_cast<size_t>(worst)].gap))
            {
                worst = static_cast<int>(i);
            }
        }
        if(worst < 0)
        {
            break;
        }
        Work& w = work[static_cast<size_t>(worst)];
        // The cut along the model's axes, on a cell's border, at least two cells in from the piece's ends, where the
        // two halves' hulls hold the least: the few cuts each way whose halves' solid's boxes hold the least
        // (shortlist), then those halves' hulls.
        za::Array<Candidate, 3 * shortlisted> found;
        int foundCount = 0;
        shortlist(g, w.region, found, foundCount);
        float bestScore = w.hull->volume * 0.995f;
        int bestAxis = -1;
        float bestPlane = 0.f;
        for(int c = 0; c < foundCount; c++)
        {
            const int axis = found[static_cast<size_t>(c)].axis;
            const float plane = found[static_cast<size_t>(c)].plane;
            Region ra = w.region, rb = w.region;
            ra.hi[axis] = plane;
            ra.cut[axis][1] = true;
            rb.lo[axis] = plane;
            rb.cut[axis][0] = true;
            regionPoints(corners, g, ra, pointsA);
            regionPoints(corners, g, rb, pointsB);
            // (Their volumes only: hulls of a fixed budget, near enough to compare.)
            b3HullData* ha = hull(pointsA.data(), static_cast<int>(pointsA.size()), 24);
            b3HullData* hb = hull(pointsB.data(), static_cast<int>(pointsB.size()), 24);
            // (A half of no hull: nothing there, or too flat to be one; with points, not a cut to make.)
            const bool ok = (ha || pointsA.size() < 4) && (hb || pointsB.size() < 4) && (ha || hb);
            const float score = (ha ? ha->volume : 0.f) + (hb ? hb->volume : 0.f);
            if(ha)
            {
                b3DestroyHull(ha);
            }
            if(hb)
            {
                b3DestroyHull(hb);
            }
            if(ok && score < bestScore)
            {
                bestScore = score;
                bestAxis = axis;
                bestPlane = plane;
            }
        }
        if(bestAxis < 0)
        {
            w.done = true; // (no cut holds less)
            continue;
        }
        Work a, b;
        a.region = b.region = w.region;
        a.region.hi[bestAxis] = bestPlane;
        a.region.cut[bestAxis][1] = true;
        b.region.lo[bestAxis] = bestPlane;
        b.region.cut[bestAxis][0] = true;
        regionPoints(corners, g, a.region, pointsA);
        regionPoints(corners, g, b.region, pointsB);
        a.hull = hullOf(pointsA, g.cell);
        b.hull = hullOf(pointsB, g.cell);
        if((!a.hull && pointsA.size() >= 4) || (!b.hull && pointsB.size() >= 4))
        {
            // (A half with no hull would leave a hole: the piece stays whole.)
            for(b3HullData* half : {a.hull, b.hull})
            {
                if(half)
                {
                    b3DestroyHull(half);
                }
            }
            w.done = true;
            continue;
        }
        b3DestroyHull(w.hull);
        work.erase(work.begin() + worst);
        for(Work* half : {&a, &b})
        {
            if(half->hull)
            {
                half->gap = gapOf(half->hull, g);
                work.pushBack(*half);
            }
        }
    }

    rep.gap = 0.f;
    rep.volume = 0.f;
    for(Work& w : work)
    {
        w.gap = w.done ? gapOf(w.hull, g) : w.gap;
        rep.gap = za::max(rep.gap, w.gap);
        if(settings.verbose)
        {
            Con_Printf("  piece %.1f %.1f %.1f .. %.1f %.1f %.1f: %d vertices, %.1f cubic units, %.2f off\n", w.region.lo.x,
                w.region.lo.y, w.region.lo.z, w.region.hi.x, w.region.hi.y, w.region.hi.z, w.hull->vertexCount,
                w.hull->volume, w.gap);
        }
        rep.volume += w.hull->volume;
        Piece& piece = pieces.emplaceBack();
        const b3Vec3* hp = b3GetHullPoints(w.hull);
        for(int i = 0; i < w.hull->vertexCount; i++)
        {
            piece.points.pushBack(glm::vec3{hp[i].x, hp[i].y, hp[i].z});
        }
        b3DestroyHull(w.hull);
    }
    rep.ms = (Sys_DoubleTime() - start) * 1000.0;
    if(settings.verbose)
    {
        Con_Printf("  voxelised in %.1f ms, %d triangles\n", (voxelised - start) * 1000.0, static_cast<int>(corners.size() / 3));
    }
    if(pieces.size() < 2)
    {
        pieces.clear();
        return false;
    }
    return true;
}

} // namespace qvr::convex
