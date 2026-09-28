// vr_ledges.cpp -- the ledge map (vr_ledges.hpp): every ledge of each brush model, found once from the BSP, for
// climbing (vr_climb.cpp) to look holds up in instead of searching the space round the hand with traces.
//
// What a ledge is. The lip of a walkable top (a drawn face whose normal's z is at least minTopNormal; not sky, not a
// liquid's surface): an edge of the face with open space just beyond it, under the top's level (the top ends there:
// not a floor going on in the next face, not a wall rising from it), room for a hand over it (handRoom, at the hold),
// and a drop beyond it: going out square to the lip at a hand's height over the top, within edgeReach, a column from 2
// units over the top to minDrop under it with nothing in it. Steps down on the way out are passed over (a trim under
// the lip, the next rung down); a wall on the way ends it (no drop). So stairs aren't ledges (their treads come every
// 8 to 16 units down), nor are floors or walls; a rung, a beam or a thin wall's top is a ledge on each side that has a
// drop (a rung against a wall: its front and its ends).
//
// How it is found. For each brush model (the world, and each of its inline models "*N", in their own space), once:
// the edges of its walkable faces that pass the tests at their middle are joined when they lie on one line (the BSP
// splits faces, and one lip is often several faces' edges, split in different places: T-junctions don't matter here,
// the pieces are joined along the line); then along each line, every sampleStep units, the tests again, and runs of
// passing samples are the ledges (where one starts or ends between two samples: found to an eighth of a unit by
// halving). For each sample, also: how far out the drop starts (dropOut: a hand over the top takes a lip only this much
// less than edgeReach in), and how deep the top is from the lip in (up to maxDepth: the mantle's footing). The tests
// use the model's own hull 0 (what the hands' probes, Quake's point traces, collide with; clip brushes aren't in it),
// so another model's solid (a door over a world ledge, the world under a plat's lip) is checked when a hold is taken
// (vr_climb.cpp: the room over the hold, the drop, the line of sight).
//
// A grid of 64-unit cells lists the ledges in each cell (a sorted vector of (cell, ledge)), for the ledges near a hand.
//
// Kept until the map changes (a different world model); a saved game of the same map keeps them. Not affected by
// vr_world_scale: a ledge is geometry, in map units.

#include "vr_ledges.hpp"
#include "vr_cvars.hpp"
#include "vr_lines.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <map>
#include <memory>
#include <string>

namespace qvr::ledges
{

namespace
{

[[nodiscard]] glm::vec3 vec(const float* v)
{
    return {v[0], v[1], v[2]};
}

// A model's hull 0, in its own space.
struct Hull
{
    hull_t* hull;

    [[nodiscard]] bool solid(const glm::vec3& p) const
    {
        vec3_t v{p.x, p.y, p.z};
        return SV_HullPointContents(hull, hull->firstclipnode, v) == CONTENTS_SOLID;
    }

    [[nodiscard]] trace_t trace(const glm::vec3& a, const glm::vec3& b) const
    {
        trace_t tr;
        memset(&tr, 0, sizeof(tr));
        tr.fraction = 1.f;
        tr.allsolid = true;
        vec3_t s{a.x, a.y, a.z}, e{b.x, b.y, b.z};
        VectorCopy(e, tr.endpos);
        SV_RecursiveHullCheck(hull, hull->firstclipnode, 0.f, 1.f, s, e, &tr);
        return tr;
    }

    // The parts of the line from `a` to `b` in solid, as fractions of it (in order).
    void spans(const glm::vec3& a, const glm::vec3& b, std::vector<glm::vec2>& out) const
    {
        out.clear();
        spansOf(hull->firstclipnode, 0.f, 1.f, a, b, out);
    }
    void spansOf(int num, float f0, float f1, const glm::vec3& a, const glm::vec3& b, std::vector<glm::vec2>& out) const
    {
        while(num >= 0)
        {
            const mclipnode_t& node = hull->clipnodes[num];
            const mplane_t& plane = hull->planes[node.planenum];
            const glm::vec3 p0 = a + (b - a) * f0, p1 = a + (b - a) * f1;
            const glm::vec3 n{plane.normal[0], plane.normal[1], plane.normal[2]};
            const float d0 = glm::dot(n, p0) - plane.dist, d1 = glm::dot(n, p1) - plane.dist;
            if(d0 >= 0.f && d1 >= 0.f)
            {
                num = node.children[0];
                continue;
            }
            if(d0 < 0.f && d1 < 0.f)
            {
                num = node.children[1];
                continue;
            }
            const float fm = f0 + (f1 - f0) * (d0 / (d0 - d1));
            const int first = d0 < 0.f ? 1 : 0;
            spansOf(node.children[first], f0, fm, a, b, out);
            num = node.children[first ^ 1];
            f0 = fm;
        }
        if(num == CONTENTS_SOLID && f1 > f0)
        {
            if(!out.empty() && out.back().y >= f0 - 1e-6f)
            {
                out.back().y = f1;
            }
            else
            {
                out.emplace_back(f0, f1);
            }
        }
    }

    // Nothing solid from `a` to `b`.
    [[nodiscard]] bool clear(const glm::vec3& a, const glm::vec3& b) const
    {
        const trace_t tr = trace(a, b);
        return !tr.allsolid && !tr.startsolid && tr.fraction >= 1.f;
    }
};

// A lip piece: an edge of a walkable face, the face's side of it in, `out` beyond it.
struct Piece
{
    glm::vec3 v0, v1;
    glm::vec3 out, normal;
};

// A line of lip: pieces joined. Points on it: `base` + `along` * s, the height rising `slope` a unit along.
struct Line
{
    glm::vec3 out, normal;
    glm::vec3 along;  // horizontal, square to `out` (out turned a quarter anticlockwise)
    float offset;     // dot(xy, out) on the line
    float z0, slope;  // the height at s = 0, and its rise per unit of s
    float s0, s1;     // the lip's extent

    [[nodiscard]] glm::vec3 at(float s) const
    {
        const glm::vec3 xy = out * offset + along * s;
        return {xy.x, xy.y, z0 + slope * s};
    }
};

// The height at `p` of the plane through `lip` with the normal `n` (a top).
[[nodiscard]] float topHeight(const glm::vec3& lip, const glm::vec3& n, const glm::vec3& p)
{
    return lip.z - (n.x * (p.x - lip.x) + n.y * (p.y - lip.y)) / n.z;
}

// The tests at a point of lip (see the top of the file).
struct Test
{
    bool ok{false};
    Sample sample;
};

[[nodiscard]] Test test(const Hull& h, const Line& line, float s)
{
    Test r;
    const glm::vec3 lip = line.at(s);
    const glm::vec3& o = line.out;
    const glm::vec3 up{0.f, 0.f, 1.f};
    // The top ends here: open beyond it under its level, open over it just in.
    if(h.solid(lip + o * 0.5f - up) || h.solid(lip - o * 0.5f + up))
    {
        return r;
    }
    // The drop: going out (half a unit, then every unit to edgeReach), the first column with nothing from 2 over the
    // top to minDrop under it; a wall on the way there, at the hand's height over the top, and there is none. A column
    // is traced only where three lines out from the lip (near the column's foot, its middle, just under the top) are
    // in the open (spans): a floor or a step below, the usual miss, needs no trace.
    const float top = lip.z;
    float dropOut = -1.f;
    thread_local std::vector<glm::vec2> rows[3];
    const float rowZ[3] = {top - minDrop + 0.5f, top - 0.5f * minDrop, top - 1.f};
    const glm::vec3 near0 = lip + o * 0.5f, far0 = lip + o * edgeReach;
    for(int k = 0; k < 3; k++)
    {
        h.spans(glm::vec3{near0.x, near0.y, rowZ[k]}, glm::vec3{far0.x, far0.y, rowZ[k]}, rows[k]);
    }
    for(float u = 0.5f; u <= edgeReach + 1e-3f; u = u < 1.f ? 1.f : u + 1.f)
    {
        const float f = (u - 0.5f) / (edgeReach - 0.5f);
        const auto solidAt = [&](const std::vector<glm::vec2>& sp) {
            return std::any_of(sp.begin(), sp.end(), [&](const glm::vec2& x) { return f >= x.x - 1e-4f && f <= x.y + 1e-4f; });
        };
        if(solidAt(rows[0]) || solidAt(rows[1]) || solidAt(rows[2]))
        {
            continue;
        }
        const glm::vec3 p = lip + o * u;
        if(h.clear(p + up * 2.f, glm::vec3{p.x, p.y, top - minDrop}))
        {
            dropOut = u;
            break;
        }
    }
    if(dropOut < 0.f || (dropOut > 0.5f && !h.clear(lip + o * 0.5f + up * 2.f, lip + o * dropOut + up * 2.f)))
    {
        return r;
    }
    // Room for the hand over the hold.
    glm::vec3 hold = lip - o * holdInset;
    hold.z = topHeight(lip, line.normal, hold);
    if(!h.clear(hold + up * 2.f, hold + up * (2.f + handRoom)))
    {
        return r;
    }
    // The top's depth: going in from the lip (every unit, from half a unit in), as far as there's solid within 3 under
    // the top (its plane), and none 1 over it (a wall rising behind the top ends it: a rung against a wall is 8 deep, not
    // the wall's depth more). The solid along a line half a unit under the top, and along one 1 over it, found once
    // (spans): a point under the top in solid, or over it in the open, needs nothing more.
    const glm::vec3 inward = -o;
    const glm::vec3 from = lip + inward * 0.5f, to = lip + inward * (maxDepth + 0.5f);
    const auto onPlane = [&](const glm::vec3& p, float dz) { return glm::vec3{p.x, p.y, topHeight(lip, line.normal, p) + dz}; };
    thread_local std::vector<glm::vec2> under, over;
    h.spans(onPlane(from, -0.5f), onPlane(to, -0.5f), under);
    h.spans(onPlane(from, 1.f), onPlane(to, 1.f), over);
    const auto inSpans = [](const std::vector<glm::vec2>& spans, float f) {
        return std::any_of(spans.begin(), spans.end(), [&](const glm::vec2& sp) { return f > sp.x && f < sp.y; });
    };
    float depth = 0.f;
    for(float d = 0.5f; d <= maxDepth; d += 1.f)
    {
        const float f = (d - 0.5f) / maxDepth; // along from..to
        if(inSpans(over, f))
        {
            break; // a wall
        }
        bool onTop = inSpans(under, f);
        if(!onTop)
        {
            const glm::vec3 p = lip + inward * d;
            const float z = topHeight(lip, line.normal, p);
            const trace_t t = h.trace(glm::vec3{p.x, p.y, z + 1.f}, glm::vec3{p.x, p.y, z - 4.f});
            onTop = t.fraction < 1.f && t.endpos[2] >= z - 3.f;
        }
        if(!onTop)
        {
            break;
        }
        depth = d + 0.5f;
    }
    r.ok = true;
    r.sample = Sample{dropOut, depth};
    return r;
}

// The lip pieces of the model's walkable faces.
void findPieces(const qmodel_t* model, const Hull& h, std::vector<Piece>& pieces, int& faces)
{
    std::vector<glm::vec3> poly;
    for(int i = 0; i < model->nummodelsurfaces; i++)
    {
        const msurface_t& surf = model->surfaces[model->firstmodelsurface + i];
        if(surf.flags & (SURF_DRAWSKY | SURF_DRAWTURB) || surf.numedges < 3)
        {
            continue;
        }
        glm::vec3 normal = vec(surf.plane->normal);
        if(surf.flags & SURF_PLANEBACK)
        {
            normal = -normal;
        }
        if(normal.z < minTopNormal)
        {
            continue;
        }
        faces++;
        poly.clear();
        glm::vec3 centre{0.f};
        for(int k = 0; k < surf.numedges; k++)
        {
            const int e = model->surfedges[surf.firstedge + k];
            const int v = static_cast<int>(e >= 0 ? model->edges[e].v[0] : model->edges[-e].v[1]);
            poly.push_back(vec(model->vertexes[v].position));
            centre += poly.back();
        }
        centre /= static_cast<float>(poly.size());
        for(size_t k = 0; k < poly.size(); k++)
        {
            const glm::vec3 v0 = poly[k], v1 = poly[(k + 1) % poly.size()];
            const glm::vec2 e{v1.x - v0.x, v1.y - v0.y};
            if(glm::length(e) < 0.25f)
            {
                continue;
            }
            glm::vec3 out = glm::normalize(glm::vec3{e.y, -e.x, 0.f});
            const glm::vec3 mid = 0.5f * (v0 + v1);
            if(glm::dot(glm::vec2{out}, glm::vec2{mid - centre}) < 0.f)
            {
                out = -out;
            }
            // A quick look (the middle and near the ends): most edges are between faces of one floor, or at a wall's
            // foot.
            const glm::vec3 down{0.f, 0.f, 1.f};
            const glm::vec3 along = (v1 - v0) / glm::distance(v0, v1) * 0.125f;
            if(h.solid(mid + out * 0.5f - down) && h.solid(v0 + along + out * 0.5f - down) && h.solid(v1 - along + out * 0.5f - down))
            {
                continue;
            }
            pieces.push_back(Piece{v0, v1, out, normal});
        }
    }
}

// The pieces joined into lines (those on one line, the same way out, touching or overlapping).
void joinPieces(const std::vector<Piece>& pieces, std::vector<Line>& lines)
{
    struct Keyed
    {
        std::array<int64_t, 4> key;
        Line line;
    };
    std::vector<Keyed> keyed;
    keyed.reserve(pieces.size());
    for(const Piece& p : pieces)
    {
        Line l;
        l.out = p.out;
        l.normal = p.normal;
        l.along = glm::vec3{-p.out.y, p.out.x, 0.f};
        l.offset = glm::dot(glm::vec2{p.v0}, glm::vec2{p.out});
        float a = glm::dot(glm::vec2{p.v0}, glm::vec2{l.along}), b = glm::dot(glm::vec2{p.v1}, glm::vec2{l.along});
        float za = p.v0.z, zb = p.v1.z;
        if(a > b)
        {
            std::swap(a, b);
            std::swap(za, zb);
        }
        l.slope = b - a > 1e-4f ? (zb - za) / (b - a) : 0.f;
        l.z0 = za - l.slope * a;
        l.s0 = a;
        l.s1 = b;
        const auto q = [](float v, float scale) { return static_cast<int64_t>(std::llround(v * scale)); };
        keyed.push_back(Keyed{{q(std::atan2(p.out.y, p.out.x), 2000.f), q(l.offset, 8.f), q(l.z0, 8.f), q(l.slope, 1000.f)}, l});
    }
    std::sort(keyed.begin(), keyed.end(), [](const Keyed& x, const Keyed& y) {
        return x.key != y.key ? x.key < y.key : x.line.s0 < y.line.s0;
    });
    for(size_t i = 0; i < keyed.size();)
    {
        Line line = keyed[i].line;
        size_t j = i + 1;
        for(; j < keyed.size() && keyed[j].key == keyed[i].key; j++)
        {
            const Line& next = keyed[j].line;
            if(next.s0 <= line.s1 + 0.05f)
            {
                line.s1 = std::max(line.s1, next.s1);
                continue;
            }
            lines.push_back(line);
            line = next;
        }
        lines.push_back(line);
        i = j;
    }
}

// The ledges along a line: runs of samples that pass the tests.
void lineLedges(const Hull& h, const Line& line, Map& map)
{
    const float length = line.s1 - line.s0;
    if(length < 0.25f)
    {
        return;
    }
    constexpr float endInset = 0.125f; // the tests just inside the lip's ends (not on a corner's planes)
    const auto sAt = [&](float t) { return line.s0 + std::clamp(t, endInset, length - endInset); };
    const int count = static_cast<int>(std::ceil(length / sampleStep - 1e-4f)) + 1;
    std::vector<Test> tests(static_cast<size_t>(count));
    std::vector<float> ts(static_cast<size_t>(count));
    // Every fourth sample first, then those between: one between two that agree (the ledge or not, how far out the drop
    // starts, how deep the top is) is the same, else it is tested too. (A feature narrower than a few units along a lip,
    // between two samples that agree, is left out: a gap in a railing too narrow for a hand; the checks when a hold is
    // taken see what is there.)
    const auto same = [](const Test& x, const Test& y) {
        return x.ok == y.ok && (!x.ok || (x.sample.dropOut == y.sample.dropOut && x.sample.depth == y.sample.depth));
    };
    for(int i = 0; i < count; i++)
    {
        ts[i] = std::min(static_cast<float>(i) * sampleStep, length);
        if(i % 4 == 0 || i == count - 1)
        {
            tests[i] = test(h, line, sAt(ts[i]));
        }
    }
    for(const int stride : {2, 1})
    {
        for(int i = stride; i < count - 1; i += 2 * stride)
        {
            const int lo = i - stride, hi = std::min(i + stride, count - 1);
            tests[i] = same(tests[lo], tests[hi]) ? tests[lo] : test(h, line, sAt(ts[i]));
        }
    }
    // Where the tests change between two samples: halved three times (to an eighth of a unit).
    const auto boundary = [&](float okT, float badT) {
        for(int k = 0; k < 3; k++)
        {
            const float mid = 0.5f * (okT + badT);
            (test(h, line, sAt(mid)).ok ? okT : badT) = mid;
        }
        return okT;
    };
    const float stretch = std::sqrt(1.f + line.slope * line.slope); // a unit along s is this long along the lip
    for(int i = 0; i < count;)
    {
        if(!tests[i].ok)
        {
            i++;
            continue;
        }
        int j = i;
        while(j + 1 < count && tests[j + 1].ok)
        {
            j++;
        }
        const float start = i > 0 ? boundary(ts[i], ts[i - 1]) : 0.f;
        const float end = j + 1 < count ? boundary(ts[j], ts[j + 1]) : length;
        if(end - start >= 0.25f)
        {
            Edge e;
            e.a = line.at(line.s0 + start);
            const glm::vec3 b = line.at(line.s0 + end);
            e.len = glm::distance(e.a, b);
            e.dir = e.len > 1e-4f ? (b - e.a) / e.len : line.along;
            e.out = line.out;
            e.normal = line.normal;
            e.firstSample = static_cast<int>(map.samples.size());
            e.samples = j - i + 1;
            e.sampleStart = (ts[i] - start) * stretch;
            e.spacing = sampleStep * stretch;
            for(int k = i; k <= j; k++)
            {
                map.samples.push_back(tests[k].sample);
            }
            e.mins = glm::min(e.a, b);
            e.maxs = glm::max(e.a, b);
            map.edges.push_back(e);
        }
        i = j + 1;
    }
}

[[nodiscard]] std::unique_ptr<Map> build(const qmodel_t* model)
{
    auto map = std::make_unique<Map>();
    const double t0 = Sys_DoubleTime();
    hull_t* hull = const_cast<hull_t*>(&model->hulls[0]);
    if(model->type == mod_brush && hull->clipnodes && hull->planes && model->surfaces)
    {
        const Hull h{hull};
        std::vector<Piece> pieces;
        findPieces(model, h, pieces, map->faces);
        map->pieces = static_cast<int>(pieces.size());
        std::vector<Line> lines;
        joinPieces(pieces, lines);
        map->lines = static_cast<int>(lines.size());
        for(const Line& line : lines)
        {
            lineLedges(h, line, *map);
        }
    }
    map->finish();
    map->ms = (Sys_DoubleTime() - t0) * 1000.0;
    return map;
}

// The maps made, by model name ("*N" for the world's inline models), for the world model they were made with.
struct Cache
{
    char world[MAX_QPATH]{};
    int vertexes{0}, surfaces{0};
    std::map<std::string, std::unique_ptr<Map>> maps;
} cache;

[[nodiscard]] bool cacheCurrent()
{
    const qmodel_t* w = sv.worldmodel;
    return w && !strcmp(cache.world, w->name) && cache.vertexes == w->numvertexes && cache.surfaces == w->numsurfaces;
}

int dropped = 0; // generation()

void resetCache()
{
    dropped++;
    cache.maps.clear();
    cache.world[0] = 0;
    if(const qmodel_t* w = sv.worldmodel)
    {
        q_strlcpy(cache.world, w->name, sizeof(cache.world));
        cache.vertexes = w->numvertexes;
        cache.surfaces = w->numsurfaces;
    }
}

// Whether `model` is the world or one of its inline models (the ones built from the map's BSP).
[[nodiscard]] bool fromThisMap(const qmodel_t* model)
{
    return model && model->type == mod_brush && sv.worldmodel &&
           (model == sv.worldmodel || (model->name[0] == '*' && model->surfaces == sv.worldmodel->surfaces));
}

// Totals over the maps made (vr_ledges, and the load's line with developer or vr_climb_debug).
void printStats(const char* when)
{
    int edges = 0, samples = 0;
    size_t bytes = 0;
    double ms = 0.0;
    float length = 0.f;
    const Map* world = nullptr;
    for(const auto& [name, m] : cache.maps)
    {
        edges += static_cast<int>(m->edges.size());
        samples += static_cast<int>(m->samples.size());
        bytes += m->bytes();
        ms += m->ms;
        for(const Edge& e : m->edges)
        {
            length += e.len;
        }
        if(sv.worldmodel && name == sv.worldmodel->name)
        {
            world = m.get();
        }
    }
    Con_Printf("ledges%s: %s: %d models, %d ledges (%.0f units of lip), %d samples, %.1f KB, built in %.1f ms", when,
        cache.world, static_cast<int>(cache.maps.size()), edges, length, samples, static_cast<double>(bytes) / 1024.0, ms);
    if(world)
    {
        Con_Printf("; the world: %d walkable faces, %d lip pieces in %d lines, %d ledges, %.1f ms", world->faces,
            world->pieces, world->lines, static_cast<int>(world->edges.size()), world->ms);
    }
    Con_Printf("\n");
}

void ledges_f()
{
    if(!sv.active || !sv.worldmodel)
    {
        Con_Printf("vr_ledges: no map\n");
        return;
    }
    if(Cmd_Argc() > 1 && !strcmp(Cmd_Argv(1), "rebuild"))
    {
        resetCache();
    }
    (void)of(sv.worldmodel);
    for(int i = 1; i < sv.worldmodel->numsubmodels; i++)
    {
        (void)of(Mod_ForName(va("*%d", i), false));
    }
    printStats("");
}

} // namespace

const Sample& Map::sampleAt(const Edge& e, float t) const
{
    const int i = std::clamp(static_cast<int>(std::lround((t - e.sampleStart) / e.spacing)), 0, e.samples - 1);
    return samples[static_cast<size_t>(e.firstSample + i)];
}

namespace
{

[[nodiscard]] glm::ivec3 cellOf(const glm::vec3& p, float size)
{
    return glm::ivec3{glm::floor(p / size)};
}

[[nodiscard]] uint64_t keyOf(const glm::ivec3& c)
{
    const auto part = [](int v) { return static_cast<uint64_t>(static_cast<uint32_t>(v) & 0x1fffffu); };
    return (part(c.x) << 42) | (part(c.y) << 21) | part(c.z);
}

} // namespace

void Map::finish()
{
    cells.clear();
    for(size_t i = 0; i < edges.size(); i++)
    {
        const glm::ivec3 lo = cellOf(edges[i].mins, cellSize), hi = cellOf(edges[i].maxs, cellSize);
        for(int x = lo.x; x <= hi.x; x++)
        {
            for(int y = lo.y; y <= hi.y; y++)
            {
                for(int z = lo.z; z <= hi.z; z++)
                {
                    cells.emplace_back(keyOf(glm::ivec3{x, y, z}), static_cast<int>(i));
                }
            }
        }
    }
    std::sort(cells.begin(), cells.end());
    seen.assign(edges.size(), 0u);
    stamp = 0;
}

void Map::nearby(const glm::vec3& mins, const glm::vec3& maxs, std::vector<int>& out) const
{
    if(edges.empty())
    {
        return;
    }
    if(++stamp == 0)
    {
        std::fill(seen.begin(), seen.end(), 0u);
        stamp = 1;
    }
    const glm::ivec3 lo = cellOf(mins, cellSize), hi = cellOf(maxs, cellSize);
    for(int x = lo.x; x <= hi.x; x++)
    {
        for(int y = lo.y; y <= hi.y; y++)
        {
            for(int z = lo.z; z <= hi.z; z++)
            {
                const uint64_t key = keyOf(glm::ivec3{x, y, z});
                auto it = std::lower_bound(cells.begin(), cells.end(), std::make_pair(key, 0));
                for(; it != cells.end() && it->first == key; ++it)
                {
                    const Edge& e = edges[static_cast<size_t>(it->second)];
                    if(seen[static_cast<size_t>(it->second)] == stamp || e.mins.x > maxs.x || e.mins.y > maxs.y ||
                        e.mins.z > maxs.z || e.maxs.x < mins.x || e.maxs.y < mins.y || e.maxs.z < mins.z)
                    {
                        continue;
                    }
                    seen[static_cast<size_t>(it->second)] = stamp;
                    out.push_back(it->second);
                }
            }
        }
    }
}

size_t Map::bytes() const
{
    return sizeof(Map) + edges.capacity() * sizeof(Edge) + samples.capacity() * sizeof(Sample) +
           cells.capacity() * sizeof(cells[0]) + seen.capacity() * sizeof(uint32_t);
}

int generation()
{
    return dropped;
}

const Map* of(const qmodel_t* model)
{
    if(!fromThisMap(model))
    {
        return nullptr;
    }
    if(!cacheCurrent())
    {
        resetCache();
    }
    auto it = cache.maps.find(model->name);
    if(it == cache.maps.end())
    {
        it = cache.maps.emplace(model->name, build(model)).first;
    }
    return it->second.get();
}

void afterLoad()
{
    if(!sv.worldmodel)
    {
        return;
    }
    const bool fresh = !cacheCurrent();
    if(fresh)
    {
        resetCache();
    }
    if(vr_climb.value == 0.f && !vr_debug_ledges.value)
    {
        return; // made when first asked for
    }
    (void)of(sv.worldmodel);
    for(int i = 1; i < sv.worldmodel->numsubmodels; i++)
    {
        (void)of(Mod_ForName(va("*%d", i), false));
    }
    if(fresh && (developer.value || vr_climb_debug.value))
    {
        printStats(" (map load)");
    }
}

void init()
{
    Cmd_AddCommand("vr_ledges", ledges_f);
}

void debugDraw()
{
    if(!vr_debug_ledges.value || !sv.active || !sv.worldmodel || cls.state != ca_connected)
    {
        return;
    }
    qcvm_t* oldvm = nullptr;
    PR_PushQCVM(&sv.qcvm, &oldvm);
    const glm::vec3 eye = vec(r_refdef.vieworg);
    const float range = std::max(64.f, vr_debug_ledges.value > 1.f ? vr_debug_ledges.value : 512.f);
    const glm::vec3 lipColour{0.2f, 1.f, 0.3f}, trimColour{1.f, 0.9f, 0.1f}, moverColour{1.f, 0.45f, 0.9f};
    std::vector<int> nearby;
    const auto drawMap = [&](const Map& m, const glm::vec3& offset, bool mover) {
        nearby.clear();
        m.nearby(eye - offset - glm::vec3{range}, eye - offset + glm::vec3{range}, nearby);
        for(const int i : nearby)
        {
            const Edge& e = m.edges[static_cast<size_t>(i)];
            const glm::vec3 a = e.a + offset, b = e.point(e.len) + offset;
            const glm::vec3 base = mover ? moverColour : lipColour;
            lines::line(a, b, 0.35f, glm::vec4{base, 1.f}, glm::vec4{base, 1.f});
            // Every 8 units: the way out (a tick, yellow where the drop starts further out than 2 units: a trim, a rung
            // below), and the top's depth in (grey, up to maxDepth).
            for(float t = 0.f; t <= e.len + 1e-3f; t += 8.f)
            {
                const float at = std::min(t, e.len);
                const Sample& s = m.sampleAt(e, at);
                const glm::vec3 p = e.point(at) + offset;
                const glm::vec3 tick = s.dropOut > 2.f ? trimColour : base;
                lines::line(p, p + e.out * 3.f, 0.25f, glm::vec4{tick, 1.f}, glm::vec4{tick, 0.6f});
                lines::line(p, p - e.out * s.depth, 0.12f, glm::vec4{0.8f, 0.8f, 0.8f, 0.7f}, glm::vec4{0.8f, 0.8f, 0.8f, 0.1f});
            }
        }
    };
    if(const Map* world = of(sv.worldmodel))
    {
        drawMap(*world, glm::vec3{0.f}, false);
    }
    for(int i = 1; i < qcvm->num_edicts; i++)
    {
        edict_t* ent = EDICT_NUM(i);
        const int index = static_cast<int>(ent->v.modelindex);
        if(ent->free || static_cast<int>(ent->v.solid) != SOLID_BSP || index <= 0 || index >= MAX_MODELS)
        {
            continue;
        }
        if(const Map* m = of(sv.models[index]))
        {
            drawMap(*m, vec(ent->v.origin), true);
        }
    }
    PR_PopQCVM(oldvm);
}

} // namespace qvr::ledges
