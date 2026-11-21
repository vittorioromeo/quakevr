// vr_hull_holes.cpp -- hole blockers (vr_hull_holes; HULLS.md, "Holes"). The compiled hulls narrower than Quake's 32 box
// (yours at 16, narrow monsters') pass through gaps Quake's box never fit: between a grate's bars, a gate's bars, a row of
// pillars, a lift and its shaft's wall. At map load (build, on the pool) this finds them and adds invisible blocker
// brushes to the trees' brushes only (never hull 0: shots, sight and drawing don't see them), so a narrow box is stopped
// where Quake's was, and gets as close as its width lets it everywhere else.
//
// A gap: two solids facing each other along x or y with less than 32 units between them, on the same line. No 32-wide box
// can hold a point of that line between them (its 32 units along the line reach one or the other), whatever its height,
// so Quake's box never was there and a blocker there changes nothing for it. The blocker is the space between the two
// faces, found exactly for any convex brushes: over the area where they face each other, the gap along the line is a
// convex function of the line's place, so a cell whose four corner lines all have a gap under 32 has it all over, and the
// box between the corners' faces holds only gap points and points inside the two brushes. Slanted faces are cut into
// smaller cells. Pairs are looked at within each model (the world, a gate's bars, a func_wall grate: blockers that move
// with it) and between the world and the brush models that stay or only travel a line (a func_wall, a door's or a lift's
// whole travel as a box: blockers in the world's tree).

#include "vr_hull_internal.hpp"
#include "vr_cvars.hpp"
#include "vr_strings.hpp"
#include "vr_testkit.hpp"

#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Chrono/Clock.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/MinMax.hpp"

#include <math.h>
#include <stdlib.h>
#include <string.h>

namespace qvr::hull::detail
{
namespace
{

constexpr double quakeBox = 32.0;  // hull 1's width: a gap narrower than this along a line never held Quake's box
constexpr double gapMargin = 0.1;  // ... by this much at least (32 exactly is Quake's box fitting)
constexpr double lineTol = 0.01;   // a line this close to a brush's face meets it
constexpr double minOverlap = 1.0; // two brushes facing each other over less than this: nothing worth closing
constexpr double minCell = 2.0;    // a slanted face's area cut into cells no smaller than this
constexpr int maxCells = 64;       // ... and no more tests than this a pair
constexpr double reachInset = 2.0; // Quake's box reaching into a blocker by this much (hull 1 is rounded by a unit or so)
constexpr double minThick = 0.25;  // a blocker thinner than this along its line is left out (faces touching)
// (and a gap narrower than the narrowest compiled box, minWidth (8), is left open: no box gets into it anyway)

// One side of a gap: a brush (its planes in Brushes) or a box (a door's or a lift's travel).
struct Solid
{
    za::U32 first = 0, count = 0; // its planes (count 0: the box lo..hi itself)
    glm::dvec3 lo{0.0}, hi{0.0};
    int group = 0; // 0: the model's own brushes; 1: a still brush model's (func_wall); 2: a mover's travel
};

struct HoleBox
{
    glm::dvec3 lo, hi;
};

// Where a solid is along axis a on the line through (pu, pv) on the other two (u, v): false if the line misses it.
bool extentOnLine(const Brushes& b, const Solid& s, int a, int u, int v, double pu, double pv, double& lo, double& hi)
{
    if(pu < s.lo[u] - lineTol || pu > s.hi[u] + lineTol || pv < s.lo[v] - lineTol || pv > s.hi[v] + lineTol)
    {
        return false;
    }
    lo = s.lo[a];
    hi = s.hi[a];
    for(za::U32 i = 0; i < s.count; ++i)
    {
        const Plane& p = b.planes[s.first + i];
        const double na = p.normal[a];
        const double r = static_cast<double>(p.dist) - static_cast<double>(p.normal[u]) * pu -
                         static_cast<double>(p.normal[v]) * pv;
        if(za::abs(na) < 1e-9)
        {
            if(r < -lineTol)
            {
                return false;
            }
            continue;
        }
        if(na > 0.0)
        {
            hi = za::min(hi, r / na);
        }
        else
        {
            lo = za::max(lo, r / na);
        }
    }
    return hi >= lo - lineTol;
}

struct PairWork
{
    const Brushes& b;
    const Solid& A; // the near side along a
    const Solid& B; // the far side
    int a, u, v;
    int tests;
    za::Vector<HoleBox>& out;
};

// The facing area's cell u0..u1 x v0..v1: a blocker if its four corner lines have a gap under Quake's box, else cut in
// two (a slanted face's, or the corners off one of them) while the pair has tests left.
void cells(PairWork& w, double u0, double u1, double v0, double v1)
{
    if(++w.tests > maxCells)
    {
        return;
    }
    const double us[2] = {u0, u1};
    const double vs[2] = {v0, v1};
    double nearEnd = -1e300, farStart = 1e300;
    bool ok = true, touching = true;
    for(int k = 0; k < 4; ++k)
    {
        double la = 0.0, ha = 0.0, lb = 0.0, hb = 0.0;
        if(!extentOnLine(w.b, w.A, w.a, w.u, w.v, us[k & 1], vs[k >> 1], la, ha) ||
            !extentOnLine(w.b, w.B, w.a, w.u, w.v, us[k & 1], vs[k >> 1], lb, hb))
        {
            ok = touching = false;
            continue;
        }
        const double gap = lb - ha;
        if(gap >= quakeBox - gapMargin || gap < minWidth)
        {
            ok = false;
        }
        if(gap > minThick)
        {
            touching = false;
        }
        nearEnd = za::max(nearEnd, ha);
        farStart = za::min(farStart, lb);
    }
    if(ok)
    {
        if(farStart - nearEnd > minThick)
        {
            HoleBox box;
            box.lo[w.a] = nearEnd;
            box.hi[w.a] = farStart;
            box.lo[w.u] = u0;
            box.hi[w.u] = u1;
            box.lo[w.v] = v0;
            box.hi[w.v] = v1;
            w.out.pushBack(box);
        }
        return;
    }
    if(touching)
    {
        return; // touching (or overlapping) at every corner: the gap is convex, so nowhere between either
    }
    const double du = u1 - u0, dv = v1 - v0;
    if(du >= dv && du >= minCell * 2.0)
    {
        const double m = (u0 + u1) * 0.5;
        cells(w, u0, m, v0, v1);
        cells(w, m, u1, v0, v1);
    }
    else if(dv >= minCell * 2.0)
    {
        const double m = (v0 + v1) * 0.5;
        cells(w, u0, u1, v0, m);
        cells(w, u0, u1, m, v1);
    }
}

// (The search shared out on the pool: chunks of the near sides, each with its own output, merged in the chunks' order:
// the same blockers in the same order as on one thread.)
jobs::Site holesSite{"hull holes"}; // (its parallelFor: vr_jobs_sites)
constexpr za::SizeT holeChunk = 256;

struct ChunkOut
{
    za::Vector<HoleBox> own, cross;
    int pairs = 0;
};

// The pairs whose near side is order[from..to) along axis a.
void searchFrom(const Brushes& b, const za::Vector<Solid>& s, const za::Vector<za::U32>& order, int a, za::SizeT from,
    za::SizeT to, ChunkOut& out)
{
    const int u = (a + 1) % 3, v = (a + 2) % 3;
    for(za::SizeT ia = from; ia < to; ++ia)
    {
        const Solid& A = s[order[ia]];
        const double lower = za::max(A.lo[a], A.hi[a] - quakeBox);
        const double upper = A.hi[a] + quakeBox - gapMargin;
        za::SizeT lo = 0, hi = order.size();
        while(lo < hi)
        {
            const za::SizeT mid = (lo + hi) / 2;
            if(s[order[mid]].lo[a] < lower)
            {
                lo = mid + 1;
            }
            else
            {
                hi = mid;
            }
        }
        for(za::SizeT j = lo; j < order.size() && s[order[j]].lo[a] < upper; ++j)
        {
            if(j == ia)
            {
                continue;
            }
            const Solid& B = s[order[j]];
            if((A.group != 0 && B.group != 0) || B.hi[a] <= A.hi[a] + minThick)
            {
                continue;
            }
            const double u0 = za::max(A.lo[u], B.lo[u]), u1 = za::min(A.hi[u], B.hi[u]);
            const double v0 = za::max(A.lo[v], B.lo[v]), v1 = za::min(A.hi[v], B.hi[v]);
            if(u1 - u0 < minOverlap || v1 - v0 < minOverlap)
            {
                continue;
            }
            ++out.pairs;
            PairWork w{b, A, B, a, u, v, 0, (A.group != 0 || B.group != 0) ? out.cross : out.own};
            cells(w, u0, u1, v0, v1);
        }
    }
}

// Every pair of solids facing each other along x or y within Quake's box (at least one of them group 0): its blockers
// into own (both group 0) or cross. Pairs looked at: returned.
int searchPairs(const Brushes& b, const za::Vector<Solid>& s, za::Vector<HoleBox>& own, za::Vector<HoleBox>& cross)
{
    int pairs = 0;
    for(int a = 0; a < 2; ++a)
    {
        za::Vector<za::U32> order(s.size());
        for(za::SizeT i = 0; i < s.size(); ++i)
        {
            order[i] = static_cast<za::U32>(i);
        }
        za::quickSort(order.begin(), order.end(),
            [&s, a](za::U32 x, za::U32 y) { return s[x].lo[a] != s[y].lo[a] ? s[x].lo[a] < s[y].lo[a] : x < y; });
        za::Vector<ChunkOut> outs((order.size() + holeChunk - 1) / holeChunk);
        jobs::parallelFor(holesSite, outs.size(), 1,
            [&](za::SizeT begin, za::SizeT end)
            {
                for(za::SizeT c = begin; c < end; ++c)
                {
                    searchFrom(b, s, order, a, c * holeChunk, za::min(order.size(), (c + 1) * holeChunk), outs[c]);
                }
            });
        for(ChunkOut& o : outs)
        {
            own.emplaceBackRange(o.own.data(), o.own.size());
            cross.emplaceBackRange(o.cross.data(), o.cross.size());
            pairs += o.pairs;
        }
    }
    return pairs;
}

int pointContents(const hull_t& h, int num, const glm::dvec3& p)
{
    while(num >= 0)
    {
        const mclipnode_t& node = h.clipnodes[num];
        const mplane_t& plane = h.planes[node.planenum];
        const double d = (plane.type < 3 ? p[plane.type] : glm::dot(glm::dvec3{plane.normal[0], plane.normal[1],
                                                                       plane.normal[2]}, p)) - plane.dist;
        num = node.children[d < 0.0 ? 1 : 0];
    }
    return num;
}

// A blocker wholly inside the model's solid (two pieces of one wall with more wall between them): not needed.
bool insideSolid(const hull_t& h0, int head, const HoleBox& box)
{
    if(head < 0)
    {
        return false;
    }
    const glm::dvec3 c = (box.lo + box.hi) * 0.5, half = (box.hi - box.lo) * 0.45;
    if(pointContents(h0, head, c) != CONTENTS_SOLID)
    {
        return false;
    }
    for(int k = 0; k < 8; ++k)
    {
        const glm::dvec3 p{c.x + ((k & 1) ? half.x : -half.x), c.y + ((k & 2) ? half.y : -half.y),
            c.z + ((k & 4) ? half.z : -half.z)};
        const int contents = pointContents(h0, head, p);
        if(contents != CONTENTS_SOLID && contents != CONTENTS_SKY)
        {
            return false;
        }
    }
    return true;
}

// Sorted, the same boxes once.
void dedupe(za::Vector<HoleBox>& boxes)
{
    auto less = [](const HoleBox& x, const HoleBox& y)
    {
        for(int i = 0; i < 3; ++i)
        {
            if(x.lo[i] != y.lo[i])
            {
                return x.lo[i] < y.lo[i];
            }
            if(x.hi[i] != y.hi[i])
            {
                return x.hi[i] < y.hi[i];
            }
        }
        return false;
    };
    za::quickSort(boxes.begin(), boxes.end(), less);
    za::SizeT n = 0;
    for(za::SizeT i = 0; i < boxes.size(); ++i)
    {
        if(n == 0 || less(boxes[n - 1], boxes[i]))
        {
            boxes[n++] = boxes[i];
        }
    }
    boxes.resize(n);
    // ... and none inside another (a grate's hole is found by both its pairs of bars, a slanted face's cells overlap).
    auto inside = [](const HoleBox& x, const HoleBox& y)
    {
        return x.lo.x >= y.lo.x && x.lo.y >= y.lo.y && x.lo.z >= y.lo.z && x.hi.x <= y.hi.x && x.hi.y <= y.hi.y &&
               x.hi.z <= y.hi.z;
    };
    za::Vector<char> dropped(n, 0);
    for(za::SizeT i = 0; i < n; ++i)
    {
        for(za::SizeT j = i + 1; j < n && boxes[j].lo.x <= boxes[i].hi.x && !dropped[i]; ++j)
        {
            if(dropped[j])
            {
                continue;
            }
            if(inside(boxes[j], boxes[i]))
            {
                dropped[j] = 1;
            }
            else if(inside(boxes[i], boxes[j]))
            {
                dropped[i] = 1;
            }
        }
    }
    za::SizeT m = 0;
    for(za::SizeT i = 0; i < n; ++i)
    {
        if(!dropped[i])
        {
            boxes[m++] = boxes[i];
        }
    }
    boxes.resize(m);
}

// Boxes side by side with the same cross-section made one (a wall cut into pieces by the BSP gives a corridor's blocker
// in pieces): each is gap points and solid only, so their union is too.
void mergeBoxes(za::Vector<HoleBox>& boxes)
{
    for(int k = 0; k < 3; ++k)
    {
        const int p = (k + 1) % 3, q = (k + 2) % 3;
        za::quickSort(boxes.begin(), boxes.end(),
            [p, q, k](const HoleBox& x, const HoleBox& y)
            {
                const double xs[5] = {x.lo[p], x.hi[p], x.lo[q], x.hi[q], x.lo[k]};
                const double ys[5] = {y.lo[p], y.hi[p], y.lo[q], y.hi[q], y.lo[k]};
                for(int i = 0; i < 5; ++i)
                {
                    if(xs[i] != ys[i])
                    {
                        return xs[i] < ys[i];
                    }
                }
                return false;
            });
        za::SizeT n = 0;
        for(za::SizeT i = 0; i < boxes.size(); ++i)
        {
            HoleBox& last = boxes[n == 0 ? 0 : n - 1];
            const HoleBox& x = boxes[i];
            if(n && last.lo[p] == x.lo[p] && last.hi[p] == x.hi[p] && last.lo[q] == x.lo[q] && last.hi[q] == x.hi[q] &&
                x.lo[k] <= last.hi[k] + 1e-6)
            {
                last.hi[k] = za::max(last.hi[k], x.hi[k]);
            }
            else
            {
                boxes[n++] = x;
            }
        }
        boxes.resize(n);
    }
}

// Quake's box (hull 1: what Quake compiled, clip brushes and all) placed somewhere it fits and reaching into the
// blocker by more than `inset` on every side (a coarse lattice of its centres): a blocker it would feel. (Hull 0 has
// solids hull 1 lacks: detail brushes compiled without collision, as Arcane Dimensions' maps have.)
bool quakeBoxReaches(const hull_t& h1, const HoleBox& box, double inset, int maxSteps, glm::dvec3* where = nullptr)
{
    glm::dvec3 from, to;
    int steps[3];
    for(int k = 0; k < 3; ++k)
    {
        from[k] = box.lo[k] + inset - h1.clip_maxs[k];
        to[k] = box.hi[k] - inset - h1.clip_mins[k];
        steps[k] = to[k] > from[k] ? za::min(maxSteps, static_cast<int>((to[k] - from[k]) / 2.0) + 1) : 0;
    }
    for(int ix = 0; ix <= steps[0]; ++ix)
    {
        for(int iy = 0; iy <= steps[1]; ++iy)
        {
            for(int iz = 0; iz <= steps[2]; ++iz)
            {
                const int idx[3] = {ix, iy, iz};
                glm::dvec3 c;
                for(int k = 0; k < 3; ++k)
                {
                    c[k] = steps[k] ? from[k] + (to[k] - from[k]) * idx[k] / steps[k] : (from[k] + to[k]) * 0.5;
                }
                const int contents = pointContents(h1, h1.firstclipnode, c);
                if(contents != CONTENTS_SOLID && contents != CONTENTS_SKY)
                {
                    if(where)
                    {
                        *where = c;
                    }
                    return true;
                }
            }
        }
    }
    return false;
}

void emitBox(Brushes& b, const HoleBox& box)
{
    const auto first = static_cast<za::U32>(b.planes.size());
    for(int axis = 0; axis < 3; ++axis)
    {
        for(int s = 0; s < 2; ++s)
        {
            glm::vec3 n{0.f};
            n[axis] = s ? 1.f : -1.f;
            b.planes.pushBack(Plane{n, static_cast<float>(s ? box.hi[axis] : -box.lo[axis]), 1.f});
        }
    }
    b.brushes.pushBack(Brush{first, 6u, glm::vec3{box.lo}, glm::vec3{box.hi}, false});
}

// ---------------------------------------------------------------------------------------------------------------
// The map's brush entities, read from the world's entity string (its own reader: this runs on the pool while the
// server spawns the map with COM_Parse).

struct BrushEnt
{
    char classname[64] = {};
    int sub = -1; // "*n"
    glm::dvec3 origin{0.0};
    double angle = 0.0;
    double lip = 8.0, height = 0.0;
    bool hasLip = false, targeted = false;
};

const char* nextToken(const char* p, char* out, za::SizeT cap)
{
    for(;;)
    {
        while(*p && static_cast<unsigned char>(*p) <= ' ')
        {
            ++p;
        }
        if(p[0] == '/' && p[1] == '/')
        {
            while(*p && *p != '\n')
            {
                ++p;
            }
            continue;
        }
        break;
    }
    if(!*p)
    {
        return nullptr;
    }
    za::SizeT n = 0;
    if(*p == '{' || *p == '}')
    {
        out[n++] = *p++;
    }
    else if(*p == '"')
    {
        ++p;
        while(*p && *p != '"')
        {
            if(n + 1 < cap)
            {
                out[n++] = *p;
            }
            ++p;
        }
        if(*p == '"')
        {
            ++p;
        }
    }
    else
    {
        while(*p && static_cast<unsigned char>(*p) > ' ')
        {
            if(n + 1 < cap)
            {
                out[n++] = *p;
            }
            ++p;
        }
    }
    out[n] = 0;
    return p;
}

void readBrushEnts(const char* text, za::Vector<BrushEnt>& ents)
{
    if(!text)
    {
        return;
    }
    char key[64], value[256];
    const char* p = text;
    while((p = nextToken(p, key, sizeof(key))) != nullptr)
    {
        if(key[0] != '{')
        {
            continue;
        }
        BrushEnt e;
        while((p = nextToken(p, key, sizeof(key))) != nullptr && key[0] != '}')
        {
            if(!(p = nextToken(p, value, sizeof(value))))
            {
                break;
            }
            if(!strcmp(key, "classname"))
            {
                strncpy(e.classname, value, sizeof(e.classname) - 1);
            }
            else if(!strcmp(key, "model") && value[0] == '*')
            {
                e.sub = atoi(value + 1);
            }
            else if(!strcmp(key, "origin"))
            {
                double x = 0.0, y = 0.0, z = 0.0;
                if(sscanf(value, "%lf %lf %lf", &x, &y, &z) == 3)
                {
                    e.origin = glm::dvec3{x, y, z};
                }
            }
            else if(!strcmp(key, "angle"))
            {
                e.angle = atof(value);
            }
            else if(!strcmp(key, "lip"))
            {
                e.lip = atof(value);
                e.hasLip = true;
            }
            else if(!strcmp(key, "height"))
            {
                e.height = atof(value);
            }
            else if(!strcmp(key, "targetname") && value[0])
            {
                e.targeted = true;
            }
        }
        if(e.sub > 0)
        {
            ents.pushBack(e);
        }
        if(!p)
        {
            break;
        }
    }
}


// A model's brushes as solids of a group, and their bounds.
void addSolids(const Brushes& b, za::SizeT sub, int group, za::Vector<Solid>& out, glm::dvec3& lo, glm::dvec3& hi)
{
    const SubModel& sm = b.subs[sub];
    lo = glm::dvec3{1e300};
    hi = glm::dvec3{-1e300};
    for(za::U32 i = 0; i < sm.numBrushes; ++i)
    {
        const Brush& br = b.brushes[sm.firstBrush + i];
        Solid s;
        s.first = br.first;
        s.count = br.count;
        s.lo = glm::dvec3{br.mins};
        s.hi = glm::dvec3{br.maxs};
        s.group = group;
        lo = glm::min(lo, s.lo);
        hi = glm::max(hi, s.hi);
        if(group >= 0)
        {
            out.pushBack(s);
        }
    }
}

} // namespace

za::U64 holesKey(const qmodel_t* world)
{
    if(vr_hull_holes.value == 0.f)
    {
        return 0x5a17ull;
    }
    za::U64 h = 0xcbf29ce484222325ull; // (FNV-1a over the entity string: a lift's height, which models are doors)
    for(const char* p = world ? world->entities : nullptr; p && *p; ++p)
    {
        h = (h ^ static_cast<unsigned char>(*p)) * 0x100000001b3ull;
    }
    return h ^ 0x401eull;
}

void addHoles(Brushes& b, const qmodel_t* world)
{
    b.holePairs = 0;
    b.holeMs = 0.0;
    if(vr_hull_holes.value == 0.f || b.subs.empty())
    {
        return;
    }
    const auto t0 = za::Clock::nowNanoseconds();
    const hull_t& h0 = world->hulls[0];
    za::Vector<BrushEnt> ents;
    readBrushEnts(world->entities, ents);
    const auto subCount = static_cast<int>(b.subs.size());

    // The world: its own pairs, then the pairs with the brush models that stay (a func_wall no one removes) or travel a
    // line (a door's or a lift's travel as one box).
    za::Vector<Solid> solids;
    glm::dvec3 lo, hi;
    addSolids(b, 0, 0, solids, lo, hi);
    for(const BrushEnt& e : ents)
    {
        if(e.sub >= subCount || e.origin != glm::dvec3{0.0})
        {
            continue; // (a model moved by its origin: its brushes aren't where they are drawn here)
        }
        const auto sub = static_cast<za::SizeT>(e.sub);
        if(!strcmp(e.classname, "func_wall") && !e.targeted)
        {
            addSolids(b, sub, 1, solids, lo, hi);
        }
        else if(!strcmp(e.classname, "func_plat") || !strcmp(e.classname, "func_door"))
        {
            addSolids(b, sub, -1, solids, lo, hi); // (its bounds alone)
            if(lo.x > hi.x)
            {
                continue;
            }
            glm::dvec3 move{0.0};
            if(e.classname[5] == 'p')
            {
                move.z = -(e.height > 0.0 ? e.height : (hi.z - lo.z) - 8.0); // QC plat: pos2 under pos1
            }
            else
            {
                glm::dvec3 dir{0.0};
                if(e.angle == -1.0)
                {
                    dir.z = 1.0;
                }
                else if(e.angle == -2.0)
                {
                    dir.z = -1.0;
                }
                else
                {
                    const double r = e.angle * (3.14159265358979323846 / 180.0);
                    dir.x = za::abs(cos(r)) < 1e-6 ? 0.0 : cos(r);
                    dir.y = za::abs(sin(r)) < 1e-6 ? 0.0 : sin(r);
                }
                const glm::dvec3 size = hi - lo;
                const double travel =
                    za::abs(dir.x) * size.x + za::abs(dir.y) * size.y + za::abs(dir.z) * size.z - e.lip; // QC door
                move = dir * za::max(travel, 0.0);
            }
            Solid s;
            s.lo = glm::min(lo, lo + move);
            s.hi = glm::max(hi, hi + move);
            s.group = 2;
            solids.pushBack(s);
        }
    }
    za::Vector<HoleBox> own, cross;
    b.holePairs += searchPairs(b, solids, own, cross);
    dedupe(own);
    mergeBoxes(own);
    dedupe(cross);
    mergeBoxes(cross);
    const hull_t& h1 = world->hulls[1];
    const bool haveHull1 = h1.clipnodes && h1.firstclipnode <= h1.lastclipnode;
    {
        const int head = b.subs[0].head;
        SubModel& w = b.subs[0];
        w.firstHole = static_cast<za::U32>(b.brushes.size());
        for(const HoleBox& x : own)
        {
            if(!insideSolid(h0, head, x) && !(haveHull1 && quakeBoxReaches(h1, x, reachInset, 2)))
            {
                emitBox(b, x);
            }
        }
        const auto ownCount = static_cast<za::U32>(b.brushes.size()) - w.firstHole;
        for(const HoleBox& x : cross)
        {
            if(!insideSolid(h0, head, x))
            {
                emitBox(b, x);
            }
        }
        w.numHoles = static_cast<za::U32>(b.brushes.size()) - w.firstHole;
        w.crossHoles = w.numHoles - ownCount;
    }

    // Each solid brush model's own gaps (a gate's bars, a func_wall grate): blockers that move with it.
    for(const BrushEnt& e : ents)
    {
        if(e.sub >= subCount || !strings::startsWith(e.classname, "func_") || !strcmp(e.classname, "func_illusionary") ||
            b.subs[static_cast<za::SizeT>(e.sub)].numHoles != 0)
        {
            continue;
        }
        const auto sub = static_cast<za::SizeT>(e.sub);
        solids.clear();
        addSolids(b, sub, 0, solids, lo, hi);
        if(solids.size() < 2)
        {
            continue;
        }
        za::Vector<HoleBox> found, none;
        b.holePairs += searchPairs(b, solids, found, none);
        dedupe(found);
        mergeBoxes(found);
        SubModel& m = b.subs[sub];
        m.firstHole = static_cast<za::U32>(b.brushes.size());
        for(const HoleBox& x : found)
        {
            if(!insideSolid(h0, m.head, x))
            {
                emitBox(b, x);
            }
        }
        m.numHoles = static_cast<za::U32>(b.brushes.size()) - m.firstHole;
    }
    b.holeMs = za::nanosecondsToMilliseconds(za::Clock::nowNanoseconds() - t0);
}

namespace
{

// ---------------------------------------------------------------------------------------------------------------
// The test aids (Debug > Tests > Player Hitbox; Movement > Player Hitbox).


// vr_hull_holes_census: the map's blockers, and whether Quake's box (hull 1, the clip brushes in it) fits anywhere it
// would reach into one of the world's own by more than a unit: it never should (a blocker only where Quake's box never was).
void census_f()
{
    qmodel_t* world = sv.worldmodel;
    const Brushes* bp = sv.active ? worldBrushes(world) : nullptr;
    if(!bp)
    {
        Con_Printf("vr_hull_holes_census: needs a map\n");
        return;
    }
    const Brushes& b = *bp;
    int models = 0, modelHoles = 0;
    for(za::SizeT i = 1; i < b.subs.size(); ++i)
    {
        if(b.subs[i].numHoles)
        {
            ++models;
            modelHoles += static_cast<int>(b.subs[i].numHoles);
        }
    }
    const SubModel& w = b.subs[0];
    const za::U32 own = w.numHoles - w.crossHoles;
    const hull_t& h1 = world->hulls[1];
    constexpr double inset = reachInset;
    int reach = 0;
    double area = 0.0;
    glm::dvec3 firstReach{0.0};
    for(za::U32 i = 0; i < own; ++i)
    {
        const Brush& br = b.brushes[w.firstHole + i];
        const glm::dvec3 lo{br.mins}, hi{br.maxs};
        area += (hi.x - lo.x) * (hi.y - lo.y);
        glm::dvec3 c{0.0};
        const bool hit = quakeBoxReaches(h1, HoleBox{lo, hi}, inset, 16, &c);
        if(hit && !reach)
        {
            firstReach = c;
        }
        reach += hit ? 1 : 0;
    }
    Con_Printf("holes: %s: world %u blockers (%u its own, %u with brush models), %d in %d brush models; %d pairs, %.1f ms; "
               "Quake's box fits into %d of its own%s\n",
        world->name, w.numHoles, own, w.crossHoles, modelHoles, models, b.holePairs, b.holeMs, reach,
        reach ? va(" (first: centre %.0f %.0f %.0f)", firstReach.x, firstReach.y, firstReach.z) : "");
    const Tree& pt = tree; // (the player's: its compile, for the blockers' share of it)
    Con_Printf("HOLECENSUS|%s|%u|%u|%d|%d|%d|%.1f|%d|%.0f|%.1f|%zu|%d\n", world->name, own, w.crossHoles, modelHoles,
        models, b.holePairs, b.holeMs, reach, area, pt.ms, static_cast<size_t>(pt.nodes.size()), pt.fromDisk ? 1 : 0);
}

// vr_hull_holes_near [radius]: the blockers near you (a brush model's in its own space).
void near_f()
{
    const Brushes* bp = sv.active ? worldBrushes(sv.worldmodel) : nullptr;
    const edict_t* player = progs::localPlayer();
    if(!bp || !player)
    {
        Con_Printf("vr_hull_holes_near: needs a map\n");
        return;
    }
    const double radius = Cmd_Argc() > 1 ? Q_atof(Cmd_Argv(1)) : 256.0;
    const glm::dvec3 at{player->v.origin[0], player->v.origin[1], player->v.origin[2]};
    int shown = 0, total = 0;
    for(za::SizeT s = 0; s < bp->subs.size(); ++s)
    {
        const SubModel& sm = bp->subs[s];
        for(za::U32 i = 0; i < sm.numHoles; ++i)
        {
            const Brush& br = bp->brushes[sm.firstHole + i];
            const glm::dvec3 lo{br.mins}, hi{br.maxs};
            if(glm::length(glm::clamp(at, lo, hi) - at) > radius)
            {
                continue;
            }
            ++total;
            if(shown++ < 40)
            {
                Con_Printf("hole (model %d%s): %.1f %.1f %.1f .. %.1f %.1f %.1f\n", static_cast<int>(s),
                    s == 0 && i >= sm.numHoles - sm.crossHoles ? ", with a brush model" : "", lo.x, lo.y, lo.z, hi.x, hi.y,
                    hi.z);
            }
        }
    }
    Con_Printf("vr_hull_holes_near: %d blockers within %.0f units\n", total, radius);
}

// vr_hull_holes_test: on maps/vrbrushholes (Misc/quakevr/make_vrbrushholes_map.py: the places below), the player's box
// and a narrow monster's (a grunt at vr_mhull_army 16) at a grate, a bar gate, a lift's edge and a pillar row; with
// vr_hull_holes on they are stopped (and still get 8 units from a face), off they pass (the scene is a real hole).
// Verdict: "brushholes: N passed, M failed" (off: "brushholes_off: ...").
void holeTest_f()
{
    const bool on = vr_hull_holes.value != 0.f;
    const char* name = on ? "brushholes" : "brushholes_off";
    testkit::Tally t;
    edict_t* player = progs::localPlayer();
    if(!player || !sv.worldmodel || strcmp(sv.worldmodel->name, "maps/vrbrushholes.bsp"))
    {
        t.check(false, name, "needs maps/vrbrushholes.bsp");
        t.verdict(name);
        return;
    }
    const float async = vr_hull_async.value, army = vr_mhull_army.value;
    Cvar_SetValueQuick(&vr_hull_async, 0.f); // (the monster's tree compiled at its first move, not on the pool)
    Cvar_SetValueQuick(&vr_mhull_army, 16.f);
    vec3_t was;
    VectorCopy(player->v.origin, was);
    const progs::ServerVm vm; // (ED_Alloc, ED_Free: the server's QC)
    edict_t* monster = ED_Alloc();
    monster->v.classname = PR_SetEngineString("monster_army");
    monster->v.solid = SOLID_SLIDEBOX;
    monster->v.movetype = MOVETYPE_STEP;
    monster->v.flags = FL_MONSTER;
    const vec3_t mmins = {-16.f, -16.f, -24.f}, mmaxs = {16.f, 16.f, 40.f};
    VectorCopy(mmins, monster->v.mins);
    VectorCopy(mmaxs, monster->v.maxs);
    VectorSubtract(mmaxs, mmins, monster->v.size);
    const auto move = [&](edict_t* who, float x0, float y0, float z0, float x1, float y1, float z1)
    {
        vec3_t a = {x0, y0, z0}, e = {x1, y1, z1};
        VectorCopy(a, who->v.origin);
        return SV_Move(a, who->v.mins, who->v.maxs, e, MOVE_NOMONSTERS, who);
    };
    char what[256];
    const auto report = [&](bool ok, const char* who, const char* scene, const trace_t& tr)
    {
        snprintf(what, sizeof(what), "%s, %s: ended at %.2f %.2f %.2f (fraction %.3f)", who, scene, tr.endpos[0],
            tr.endpos[1], tr.endpos[2], tr.fraction);
        if(t.check(ok, name, what))
        {
            Con_Printf("%s: PASS %s\n", name, what);
        }
    };
    for(int k = 0; k < 2; ++k)
    {
        edict_t* who = k ? monster : player;
        const char* label = k ? "monster" : "player";
        // The grate (pit x -768..-568, y -64..136; bars 8 every 32, z -8..0): down through a 24-unit hole.
        trace_t tr = move(who, -748.f, -44.f, 25.f, -748.f, -44.f, -100.f);
        report(on ? tr.endpos[2] > 23.9f : tr.endpos[2] < 0.f, label, on ? "grate drop stopped on it" : "grate drop fell", tr);
        tr = move(who, -748.f, -44.f, 25.f, -600.f, -44.f, 25.f);
        report(tr.fraction == 1.f, label, "walk across the grate", tr);
        // The gate (bars y 260..268): through the gap between the wall and the first bar (the world and the door) and
        // between two bars (the door's own).
        for(const float x : {-340.f, -308.f})
        {
            tr = move(who, x, 200.f, 24.f, x, 330.f, 24.f);
            const bool touch = za::abs(260.f - 8.f - tr.endpos[1]) < 0.2f;
            report(on ? touch : tr.fraction == 1.f, label,
                on ? (x < -320.f ? "gate gap by the wall stopped 8 from it" : "gate gap between bars stopped 8 from them")
                   : (x < -320.f ? "gate gap by the wall passed" : "gate gap between bars passed"),
                tr);
        }
        // The lift (plat x 448..568, y -96..24, up at z 128; the shaft's walls x 588, y 44): down the 20-unit slot east.
        tr = move(who, 578.f, -30.f, 153.f, 578.f, -30.f, 30.f);
        report(on ? tr.endpos[2] > 151.9f : tr.endpos[2] < 100.f, label, on ? "lift slot drop stopped" : "lift slot drop fell", tr);
        // The pillars (16 wide, 24 apart, y 448..464): through a gap.
        tr = move(who, 188.f, 380.f, 24.f, 188.f, 530.f, 24.f);
        report(on ? za::abs(448.f - 8.f - tr.endpos[1]) < 0.2f : tr.fraction == 1.f, label,
            on ? "pillar gap stopped 8 from them" : "pillar gap passed", tr);
    }
    // Close up as before (the player): a pillar's side, the hall's north wall, an open walk.
    trace_t tr = move(player, 100.f, 456.f, 24.f, 300.f, 456.f, 24.f);
    report(za::abs(160.f - 8.f - tr.endpos[0]) < 0.2f, "player", "a pillar's side: 8 from it", tr);
    tr = move(player, 0.f, 600.f, 24.f, 0.f, 800.f, 24.f);
    report(za::abs(768.f - 8.f - tr.endpos[1]) < 0.2f, "player", "the north wall: 8 from it", tr);
    tr = move(player, -200.f, -600.f, 24.f, 200.f, -600.f, 24.f);
    report(tr.fraction == 1.f, "player", "an open walk", tr);
    VectorCopy(was, player->v.origin);
    SV_LinkEdict(player, false);
    ED_Free(monster);
    Cvar_SetValueQuick(&vr_hull_async, async);
    Cvar_SetValueQuick(&vr_mhull_army, army);
    t.verdict(name);
}

} // namespace

void registerHoleCommands()
{
    Cmd_AddCommand("vr_hull_holes_census", census_f);
    Cmd_AddCommand("vr_hull_holes_near", near_f);
    Cmd_AddCommand("vr_hull_holes_test", holeTest_f);
}

} // namespace qvr::hull::detail
