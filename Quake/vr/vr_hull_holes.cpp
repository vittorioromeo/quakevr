// vr_hull_holes.cpp -- hole blockers (vr_hull_holes; HULLS.md, "Holes"). The compiled hulls narrower than Quake's 32 box
// (yours at 16, narrow monsters') pass through gaps Quake's box never fit: between a grate's bars, a gate's bars, a row of
// pillars, a lift and its shaft's wall. At map load (build, on the pool) this finds them and adds invisible blocker
// brushes to the trees' brushes only (never hull 0: shots, sight and drawing don't see them), so a narrow box is stopped
// where Quake's was, and gets as close as its width lets it everywhere else.
//
// A gap: two solids facing each other along x or y with less than 32 units between them, on the same line. No 32-wide box
// can hold a point of that line between them (its 32 units along the line reach one or the other), whatever its height,
// so Quake's box never was there and a blocker there changes nothing for it. (A model's own gaps of exactly 32 count too
// since backlog1122: Quake's box only bridges such a hole, as hull 1's check confirms.) The blocker is the space between the two
// faces, found exactly for any convex brushes: over the area where they face each other, the gap along the line is a
// convex function of the line's place, so a cell whose four corner lines all have a gap under 32 has it all over, and the
// box between the corners' faces holds only gap points and points inside the two brushes. Slanted faces are cut into
// smaller cells. Pairs are looked at within each model (the world, a gate's bars, a func_wall grate: blockers that move
// with it) and between the world and the brush models that stay or only travel a line (a func_wall, a lift's whole
// travel as a box: blockers in the world's tree; a door shut: blockers in its own tree, moving with it), and between two
// of those (and the corners between those blockers: crossCorners). Since holes2 (2026-11-22) also along
// the diagonals (slanted bars), seen from above (bars at two heights) and in the wide band (thin gaps up to
// vr_hull_holes_wide), each kept only where hull 1 shows Quake's box can't reach into it (the last two: standing, over a
// fall). The limits are settings (vr_hull_holes_*: Settings); vr_hull_holes_mark reports a miss.

#include "vr_hull_internal.hpp"
#include "vr_cvars.hpp"
#include "vr_strings.hpp"
#include "vr_testkit.hpp"
#include "vr_lines.hpp"
#include "vr_text3d.hpp"
#include "vr_units.hpp"

#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/Macros.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Chrono/Clock.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/MinMax.hpp"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace qvr::hull::detail
{
namespace
{

constexpr double quakeBox = 32.0;    // hull 1's width: a gap narrower than this along a line never held Quake's box
constexpr double quakeHeight = 56.0; // ... and its height (the gaps seen from above: two solids closer than this in z)
constexpr double gapMargin = 0.1;    // ... by this much at least (32 exactly is Quake's box fitting)
constexpr double lineTol = 0.01;     // a line this close to a brush's face meets it
constexpr double minOverlap = 1.0;   // two brushes facing each other over less than this: nothing worth closing
constexpr double minThick = 0.25;    // a blocker thinner than this along its line is left out (faces touching)
constexpr double thinSolid = 16.0;   // the gaps seen from above and the wide ones: between solids no thicker than this in z (bars)
constexpr double fallDepth = 104.0;  // "over a fall": nothing under a blocker for this far (Quake's box, 56, and its jump)
constexpr double rootHalf = 0.70710678118654752440;

// The search's settings (vr_hull_holes_*: HULLS.md, "Holes"), read once a build (addHoles) for it and its pool jobs.
struct Settings
{
    double maxGap = quakeBox;   // the line test's limit (vr_hull_holes_max)
    double crossGap = quakeBox; // ... between the world and a brush model or two brush models: never past Quake's box
    double minGap = 8.0;        // narrower gaps left open (vr_hull_holes_min)
    double wide = 48.0;         // the world's thin gaps up to this over a fall (vr_hull_holes_wide; 0 off)
    double minCell = 2.0;       // a slanted face's area cut into cells no smaller than this (vr_hull_holes_cell)
    double reach = 2.0;         // Quake's box reaching into a blocker by this much drops it (vr_hull_holes_reach)
    int maxCells = 64;          // ... and no more tests than this a pair (vr_hull_holes_cells)
    bool diag = true, models = true, vert = true; // vr_hull_holes_diag, _models, _vert
};

Settings readSettings()
{
    Settings s;
    s.maxGap = za::clamp(static_cast<double>(vr_hull_holes_max.value), 8.0, 64.0);
    s.crossGap = za::min(s.maxGap, quakeBox);
    s.minGap = za::clamp(static_cast<double>(vr_hull_holes_min.value), 0.0, s.maxGap);
    s.wide = vr_hull_holes_wide.value > s.maxGap ? za::min(static_cast<double>(vr_hull_holes_wide.value), 96.0) : 0.0;
    s.minCell = za::clamp(static_cast<double>(vr_hull_holes_cell.value), 0.5, 32.0);
    s.reach = za::clamp(static_cast<double>(vr_hull_holes_reach.value), 0.0, 16.0);
    s.maxCells = za::clamp(static_cast<int>(vr_hull_holes_cells.value), 1, 4096);
    s.diag = vr_hull_holes_diag.value != 0.f;
    s.models = vr_hull_holes_models.value != 0.f;
    s.vert = vr_hull_holes_vert.value != 0.f;
    return s;
}

// seat-shared: the world's hole blockers, built with the map
Settings cfg; // the last build's (addHoles; the census and the mark read it too)

// One side of a gap: a brush (its planes in Brushes) or a box (a door's or a lift's travel).
struct Solid
{
    za::U32 first = 0, count = 0; // its planes (count 0: the box lo..hi itself)
    glm::dvec3 lo{0.0}, hi{0.0};
    int group = 0;        // 0: the model's own brushes; 1: a still brush model's (func_wall); 2: a mover's travel
    int model = 0;        // its submodel (two of one brush model aren't a pair in the world's search)
    bool attach = false;  // a door's closed box (group 2): its blockers go in its own tree and move with it
    bool slanted = false; // an upright prism with a face turned in x and y (a slanted bar, a round hole's wedge, a
                          // turned wall; not a rock's or an arch's piece): the diagonal search, between two of them
};

struct HoleBox
{
    glm::dvec3 lo, hi;
};

// The diagonal searches' lines: along tiltA[d], the other horizontal tiltU[d]; their boxes are in (a, u, z).
const glm::dvec3 tiltA[2] = {{rootHalf, rootHalf, 0.0}, {rootHalf, -rootHalf, 0.0}};
const glm::dvec3 tiltU[2] = {{-rootHalf, rootHalf, 0.0}, {rootHalf, rootHalf, 0.0}};

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

// A solid's bounds' reach along the horizontal direction d: lo..hi.
void spanAlong(const Solid& s, const glm::dvec3& d, double& lo, double& hi)
{
    const glm::dvec3 c = (s.lo + s.hi) * 0.5, h = (s.hi - s.lo) * 0.5;
    const double m = glm::dot(c, d), r = za::abs(d.x) * h.x + za::abs(d.y) * h.y;
    lo = m - r;
    hi = m + r;
}

// The same along the diagonal line p = pu * tiltU[d] + z pv + t * tiltA[d] (a brush: its planes bound it).
bool extentTilted(const Brushes& b, const Solid& s, int d, double pu, double pv, double& lo, double& hi)
{
    double ulo = 0.0, uhi = 0.0;
    spanAlong(s, tiltU[d], ulo, uhi);
    if(pu < ulo - lineTol || pu > uhi + lineTol || pv < s.lo.z - lineTol || pv > s.hi.z + lineTol)
    {
        return false;
    }
    spanAlong(s, tiltA[d], lo, hi);
    for(za::U32 i = 0; i < s.count; ++i)
    {
        const Plane& p = b.planes[s.first + i];
        const glm::dvec3 n{p.normal};
        const double na = glm::dot(n, tiltA[d]);
        const double r = static_cast<double>(p.dist) - glm::dot(n, tiltU[d]) * pu - n.z * pv;
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

// Seen from above: where a solid is along axis a (x or y) on the vertical plane through ph on the other one, over its
// whole height (the union of its lines at five heights: a thin solid's).
bool extentFromAbove(const Brushes& b, const Solid& s, int a, double ph, double& lo, double& hi)
{
    const int h = 1 - a;
    bool any = false;
    for(int k = 0; k < 5; ++k)
    {
        const double z = s.lo.z + 0.05 + (s.hi.z - s.lo.z - 0.1) * k / 4.0;
        double l = 0.0, e = 0.0;
        if(extentOnLine(b, s, a, h, 2, ph, z, l, e))
        {
            lo = any ? za::min(lo, l) : l;
            hi = any ? za::max(hi, e) : e;
            any = true;
        }
    }
    return any;
}

enum class Mode
{
    Axis,  // along x or y (a, u, v: the axes)
    Tilt,  // along a diagonal (dir), its box in (a, u, z)
    Above, // along x or y seen from above (a, u = the other one; v: the lower solid's height, not cut)
};

struct PairWork
{
    const Brushes& b;
    const Solid& A; // the near side along a
    const Solid& B; // the far side
    Mode mode;
    int a, u, v, dir;
    double limit, low; // a blocker where every corner's gap is under limit (and one at least low: the wide band)
    int tests;
    za::Vector<HoleBox>& out;
};

bool lineGap(const PairWork& w, double pu, double pv, double& ha, double& lb)
{
    double la = 0.0, hb = 0.0;
    switch(w.mode)
    {
        case Mode::Axis:
            return extentOnLine(w.b, w.A, w.a, w.u, w.v, pu, pv, la, ha) &&
                   extentOnLine(w.b, w.B, w.a, w.u, w.v, pu, pv, lb, hb);
        case Mode::Tilt:
            return extentTilted(w.b, w.A, w.dir, pu, pv, la, ha) && extentTilted(w.b, w.B, w.dir, pu, pv, lb, hb);
        case Mode::Above:
            return extentFromAbove(w.b, w.A, w.a, pu, la, ha) && extentFromAbove(w.b, w.B, w.a, pu, lb, hb);
    }
    return false;
}

// The facing area's cell u0..u1 x v0..v1: a blocker if its four corner lines have a gap under the limit, else cut in
// two (a slanted face's, or the corners off one of them) while the pair has tests left.
void cells(PairWork& w, double u0, double u1, double v0, double v1)
{
    if(++w.tests > cfg.maxCells)
    {
        return;
    }
    const double us[2] = {u0, u1};
    const double vs[2] = {v0, v1};
    double nearEnd = -1e300, farStart = 1e300, widest = 0.0;
    bool ok = true, touching = true;
    for(int k = 0; k < 4; ++k)
    {
        double ha = 0.0, lb = 0.0;
        if(!lineGap(w, us[k & 1], vs[k >> 1], ha, lb))
        {
            ok = touching = false;
            continue;
        }
        const double gap = lb - ha;
        if(gap >= w.limit - gapMargin || gap < cfg.minGap)
        {
            ok = false;
        }
        if(gap > minThick)
        {
            touching = false;
        }
        widest = za::max(widest, gap);
        nearEnd = za::max(nearEnd, ha);
        farStart = za::min(farStart, lb);
    }
    if(ok)
    {
        if(farStart - nearEnd > minThick && widest >= w.low)
        {
            HoleBox box;
            const int a = w.mode == Mode::Tilt ? 0 : w.a, u = w.mode == Mode::Tilt ? 1 : w.u;
            const int v = w.mode == Mode::Tilt ? 2 : w.v;
            box.lo[a] = nearEnd;
            box.hi[a] = farStart;
            box.lo[u] = u0;
            box.hi[u] = u1;
            box.lo[v] = v0;
            box.hi[v] = v1;
            w.out.pushBack(box);
        }
        return;
    }
    if(touching)
    {
        return; // touching (or overlapping) at every corner: the gap is convex, so nowhere between either
    }
    const double du = u1 - u0, dv = w.mode == Mode::Above ? 0.0 : v1 - v0;
    if(du >= dv && du >= cfg.minCell * 2.0)
    {
        const double m = (u0 + u1) * 0.5;
        cells(w, u0, m, v0, v1);
        cells(w, m, u1, v0, v1);
    }
    else if(dv >= cfg.minCell * 2.0)
    {
        const double m = (v0 + v1) * 0.5;
        cells(w, u0, u1, v0, m);
        cells(w, u0, u1, m, v1);
    }
}

// (The search shared out on the pool: chunks of the near sides, each with its own output, merged in the chunks' order:
// the same blockers in the same order as on one thread.)
// seat-shared: the worker pool's site
jobs::Site holesSite{"hull holes"}; // (its parallelFor: vr_jobs_sites)
constexpr za::SizeT holeChunk = 256;
constexpr double gridCell = 64.0;      // the solids' grid over x and y: a near side looks only in the cells beside it
constexpr za::SizeT bigCells = 4096;   // a solid over more cells than this is looked at by every near side instead

// What a search finds: own (both sides the model's own), cross (one a brush model), tilt (own, along each diagonal:
// in (a, u, z)) and stand (the world's own seen from above or in the wide band: kept only over a fall, where Quake's
// box can't stand reaching into them).
struct Found
{
    za::Vector<HoleBox> own, cross, tilt[2], stand;
    za::Vector<HoleBox> ownPieces, tiltPieces[2], standPieces; // (tidy: before merging, for splitDropped)
    za::Vector<HoleBox> attached; // with a door's closed box: in the door's own tree (attachedModel[i]: its submodel)
    za::Vector<int> attachedModel;
    int pairs = 0;

    void add(const Found& o)
    {
        own.emplaceBackRange(o.own.data(), o.own.size());
        cross.emplaceBackRange(o.cross.data(), o.cross.size());
        attached.emplaceBackRange(o.attached.data(), o.attached.size());
        attachedModel.emplaceBackRange(o.attachedModel.data(), o.attachedModel.size());
        for(int d = 0; d < 2; ++d)
        {
            tilt[d].emplaceBackRange(o.tilt[d].data(), o.tilt[d].size());
        }
        stand.emplaceBackRange(o.stand.data(), o.stand.size());
        pairs += o.pairs;
    }
};

// The solids by grid cell over x and y (each in every cell its bounds reach), the huge ones apart.
struct SolidGrid
{
    double x0 = 0.0, y0 = 0.0;
    int nx = 0, ny = 0;
    za::Vector<za::U32> start; // [cell]: its solids in list from start[cell] to start[cell + 1]
    za::Vector<za::U32> list;
    za::Vector<za::U32> big;

    int cellX(double x) const { return za::clamp(static_cast<int>(floor((x - x0) / gridCell)), 0, nx - 1); }
    int cellY(double y) const { return za::clamp(static_cast<int>(floor((y - y0) / gridCell)), 0, ny - 1); }
};

void makeGrid(const za::Vector<Solid>& s, SolidGrid& g)
{
    glm::dvec3 lo{1e300}, hi{-1e300};
    for(const Solid& x : s)
    {
        lo = glm::min(lo, x.lo);
        hi = glm::max(hi, x.hi);
    }
    g.x0 = lo.x;
    g.y0 = lo.y;
    g.nx = za::clamp(static_cast<int>((hi.x - lo.x) / gridCell) + 1, 1, 1024);
    g.ny = za::clamp(static_cast<int>((hi.y - lo.y) / gridCell) + 1, 1, 1024);
    const auto cells = static_cast<za::SizeT>(g.nx) * static_cast<za::SizeT>(g.ny);
    g.start.clear();
    g.start.resize(cells + 1, 0);
    g.list.clear();
    g.big.clear();
    for(int pass = 0; pass < 2; ++pass)
    {
        za::Vector<za::U32> fill;
        if(pass == 1)
        {
            for(za::SizeT c = 0; c < cells; ++c)
            {
                g.start[c + 1] += g.start[c];
            }
            g.list.resize(g.start[cells]);
            fill.emplaceBackRange(g.start.data(), cells);
        }
        for(za::SizeT i = 0; i < s.size(); ++i)
        {
            const int cx0 = g.cellX(s[i].lo.x), cx1 = g.cellX(s[i].hi.x);
            const int cy0 = g.cellY(s[i].lo.y), cy1 = g.cellY(s[i].hi.y);
            if(static_cast<za::SizeT>(cx1 - cx0 + 1) * static_cast<za::SizeT>(cy1 - cy0 + 1) > bigCells)
            {
                if(pass == 0)
                {
                    g.big.pushBack(static_cast<za::U32>(i));
                }
                continue;
            }
            for(int cy = cy0; cy <= cy1; ++cy)
            {
                for(int cx = cx0; cx <= cx1; ++cx)
                {
                    const auto c = static_cast<za::SizeT>(cy) * static_cast<za::SizeT>(g.nx) + static_cast<za::SizeT>(cx);
                    if(pass == 0)
                    {
                        ++g.start[c + 1];
                    }
                    else
                    {
                        g.list[fill[c]++] = static_cast<za::U32>(i);
                    }
                }
            }
        }
    }
}

// The pair (A near, B far) along a: its blockers, if they face each other within the limits. world: the world's own
// search (the wide band and the gaps seen from above).
void tryPair(const Brushes& b, const Solid& A, const Solid& B, int a, bool world, Found& out)
{
    const bool own = A.group == 0 && B.group == 0;
    if(&A == &B || (!own && A.group != 0 && B.group != 0 && (!cfg.models || A.model == B.model)))
    {
        return;
    }
    // The model's own gaps up to the limit itself (Quake's box exactly as wide as a hole only bridges it, standing on its
    // edges: Quoth start's 32 x 32 drain), as hull 1's check keeps them honest; with a brush model (the line test
    // alone) under it. (In cells, a gap of limit - gapMargin or more is out: here limit + gapMargin.)
    const double limit = own ? cfg.maxGap + 2.0 * gapMargin : cfg.crossGap;
    const bool thin = world && own && A.hi.z - A.lo.z <= thinSolid && B.hi.z - B.lo.z <= thinSolid;
    const double reach = thin ? za::max(limit, cfg.wide) : limit;
    const double lower = za::max(A.lo[a], A.hi[a] - reach);
    const double upper = A.hi[a] + reach - gapMargin;
    if(B.lo[a] < lower || B.lo[a] >= upper || B.hi[a] <= A.hi[a] + minThick)
    {
        return;
    }
    const int u = (a + 1) % 3, v = (a + 2) % 3, h = 1 - a;
    const double u0 = za::max(A.lo[u], B.lo[u]), u1 = za::min(A.hi[u], B.hi[u]);
    const double v0 = za::max(A.lo[v], B.lo[v]), v1 = za::min(A.hi[v], B.hi[v]);
    const double h0 = za::max(A.lo[h], B.lo[h]), h1 = za::min(A.hi[h], B.hi[h]);
    const double zOverlap = za::min(A.hi.z, B.hi.z) - za::max(A.lo.z, B.lo.z);
    if(h1 - h0 < minOverlap)
    {
        return;
    }
    if(zOverlap < minOverlap)
    {
        // Seen from above: two thin solids at different heights (bars at two levels), closer in z than Quake's box is
        // tall; the blocker at the lower one's height.
        if(thin && cfg.vert && -zOverlap < quakeHeight - gapMargin)
        {
            const Solid& low = A.hi.z <= B.hi.z ? A : B;
            ++out.pairs;
            PairWork w{b, A, B, Mode::Above, a, h, 2, 0, cfg.maxGap, 0.0, 0, out.stand};
            cells(w, h0, h1, low.lo.z, low.hi.z);
        }
        return;
    }
    if(u1 - u0 < minOverlap || v1 - v0 < minOverlap)
    {
        return;
    }
    ++out.pairs;
    if(B.lo[a] < A.hi[a] + limit - gapMargin)
    {
        if(A.attach || B.attach)
        {
            // With a door's closed box: in the door's tree (the near one's, of two doors), so that they go where it
            // goes (DM1's sinking bars: blockers left between them in the world kept the opened way shut).
            PairWork w{b, A, B, Mode::Axis, a, u, v, 0, limit, 0.0, 0, out.attached};
            cells(w, u0, u1, v0, v1);
            out.attachedModel.resize(out.attached.size(), A.attach ? A.model : B.model);
        }
        else
        {
            PairWork w{b, A, B, Mode::Axis, a, u, v, 0, limit, 0.0, 0, own ? out.own : out.cross};
            cells(w, u0, u1, v0, v1);
        }
    }
    if(thin && cfg.wide > limit)
    {
        PairWork w{b, A, B, Mode::Axis, a, u, v, 0, cfg.wide, limit - gapMargin, 0, out.stand};
        cells(w, u0, u1, v0, v1);
    }
}

// The pair along diagonal d (the model's own brushes, one slanted at least): its blockers, near and far found on the
// line through the middle of where they face each other.
void tryTilt(const Brushes& b, const Solid& S, const Solid& T, int d, Found& out)
{
    const double limit = cfg.maxGap / rootHalf; // (32 x 1.414: Quake's box across a diagonal)
    double sa0, sa1, ta0, ta1, su0, su1, tu0, tu1;
    spanAlong(S, tiltA[d], sa0, sa1);
    spanAlong(T, tiltA[d], ta0, ta1);
    if(za::max(ta0 - sa1, sa0 - ta1) >= limit - gapMargin)
    {
        return;
    }
    spanAlong(S, tiltU[d], su0, su1);
    spanAlong(T, tiltU[d], tu0, tu1);
    const double u0 = za::max(su0, tu0), u1 = za::min(su1, tu1);
    const double v0 = za::max(S.lo.z, T.lo.z), v1 = za::min(S.hi.z, T.hi.z);
    if(u1 - u0 < minOverlap || v1 - v0 < minOverlap)
    {
        return;
    }
    const double uc = (u0 + u1) * 0.5, vc = (v0 + v1) * 0.5;
    double sl, sh, tl, th;
    const bool sMeets = extentTilted(b, S, d, uc, vc, sl, sh), tMeets = extentTilted(b, T, d, uc, vc, tl, th);
    bool sNear = (sa0 + sa1) < (ta0 + ta1);
    if(sMeets && tMeets)
    {
        if(tl >= sh - lineTol)
        {
            sNear = true;
        }
        else if(sl >= th - lineTol)
        {
            sNear = false;
        }
        else
        {
            return; // (overlapping there)
        }
    }
    ++out.pairs;
    PairWork w{b, sNear ? S : T, sNear ? T : S, Mode::Tilt, 0, 1, 2, d, limit, 0.0, 0, out.tilt[d]};
    cells(w, u0, u1, v0, v1);
}

// The pairs whose near side is s[from..to), along x and y, from the grid's cells beside each: a solid met in several
// of them is taken in the first only (the lowest cell of both the asked area and its own). Then the diagonals: each
// pair of the model's own once (the later one met from the earlier), one slanted at least.
void searchFrom(const Brushes& b, const za::Vector<Solid>& s, const SolidGrid& g, za::SizeT from, za::SizeT to,
    bool world, Found& out)
{
    const double wideReach = za::max(cfg.maxGap, world ? cfg.wide : 0.0); // (from a thin solid: the wide band)
    const double tiltReach = cfg.maxGap / rootHalf;
    for(za::SizeT ia = from; ia < to; ++ia)
    {
        const Solid& A = s[ia];
        for(int a = 0; a < 2; ++a)
        {
            const int h = 1 - a; // the other one of x and y
            double qlo[2], qhi[2];
            const double reach = A.hi.z - A.lo.z <= thinSolid ? wideReach : cfg.maxGap;
            qlo[a] = A.hi[a] - reach;
            qhi[a] = A.hi[a] + reach;
            qlo[h] = A.lo[h];
            qhi[h] = A.hi[h];
            const int cx0 = g.cellX(qlo[0]), cx1 = g.cellX(qhi[0]), cy0 = g.cellY(qlo[1]), cy1 = g.cellY(qhi[1]);
            for(int cy = cy0; cy <= cy1; ++cy)
            {
                for(int cx = cx0; cx <= cx1; ++cx)
                {
                    const auto c = static_cast<za::SizeT>(cy) * static_cast<za::SizeT>(g.nx) + static_cast<za::SizeT>(cx);
                    for(za::U32 k = g.start[c]; k < g.start[c + 1]; ++k)
                    {
                        const Solid& B = s[g.list[k]];
                        if(za::max(cx0, g.cellX(B.lo.x)) != cx || za::max(cy0, g.cellY(B.lo.y)) != cy)
                        {
                            continue; // (taken in an earlier cell)
                        }
                        tryPair(b, A, B, a, world, out);
                    }
                }
            }
            for(const za::U32 k : g.big)
            {
                tryPair(b, A, s[k], a, world, out);
            }
        }
        if(!cfg.diag || A.group != 0 || !A.slanted)
        {
            continue;
        }
        const int cx0 = g.cellX(A.lo.x - tiltReach), cx1 = g.cellX(A.hi.x + tiltReach);
        const int cy0 = g.cellY(A.lo.y - tiltReach), cy1 = g.cellY(A.hi.y + tiltReach);
        const auto tilted = [&](za::U32 ib)
        {
            const Solid& B = s[ib];
            if(ib <= ia || B.group != 0 || (!A.slanted || !B.slanted))
            {
                return;
            }
            tryTilt(b, A, B, 0, out);
            tryTilt(b, A, B, 1, out);
        };
        for(int cy = cy0; cy <= cy1; ++cy)
        {
            for(int cx = cx0; cx <= cx1; ++cx)
            {
                const auto c = static_cast<za::SizeT>(cy) * static_cast<za::SizeT>(g.nx) + static_cast<za::SizeT>(cx);
                for(za::U32 k = g.start[c]; k < g.start[c + 1]; ++k)
                {
                    const Solid& B = s[g.list[k]];
                    if(za::max(cx0, g.cellX(B.lo.x)) == cx && za::max(cy0, g.cellY(B.lo.y)) == cy)
                    {
                        tilted(g.list[k]);
                    }
                }
            }
        }
        for(const za::U32 k : g.big)
        {
            tilted(k);
        }
    }
}

// Every pair of solids facing each other within the limits (at least one of them group 0, or two brush models'): its
// blockers into out.
void searchPairs(const Brushes& b, const za::Vector<Solid>& s, bool world, Found& out)
{
    if(s.empty())
    {
        return;
    }
    SolidGrid g;
    makeGrid(s, g);
    za::Vector<Found> outs((s.size() + holeChunk - 1) / holeChunk);
    jobs::parallelFor(holesSite, outs.size(), 1,
        [&](za::SizeT begin, za::SizeT end)
        {
            for(za::SizeT c = begin; c < end; ++c)
            {
                searchFrom(b, s, g, c * holeChunk, za::min(s.size(), (c + 1) * holeChunk), world, outs[c]);
            }
        });
    for(const Found& o : outs)
    {
        out.add(o);
    }
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

// A blocker as a box turned about z: centre c, half axes e (an axis box's along x, y and z; e[2] always vertical).
struct OBox
{
    glm::dvec3 c;
    glm::dvec3 e[3];
};

OBox axisBox(const HoleBox& x)
{
    const glm::dvec3 h = (x.hi - x.lo) * 0.5;
    return OBox{(x.lo + x.hi) * 0.5, {glm::dvec3{h.x, 0.0, 0.0}, glm::dvec3{0.0, h.y, 0.0}, glm::dvec3{0.0, 0.0, h.z}}};
}

// A diagonal search's box (in (a, u, z) along tiltA[d]) in the world.
OBox tiltBox(const HoleBox& x, int d)
{
    const glm::dvec3 m = (x.lo + x.hi) * 0.5, h = (x.hi - x.lo) * 0.5;
    return OBox{tiltA[d] * m.x + tiltU[d] * m.y + glm::dvec3{0.0, 0.0, m.z},
        {tiltA[d] * h.x, tiltU[d] * h.y, glm::dvec3{0.0, 0.0, h.z}}};
}

// A blocker wholly inside the model's solid (two pieces of one wall with more wall between them): not needed.
bool insideSolid(const hull_t& h0, int head, const OBox& box)
{
    if(head < 0)
    {
        return false;
    }
    if(pointContents(h0, head, box.c) != CONTENTS_SOLID)
    {
        return false;
    }
    for(int k = 0; k < 8; ++k)
    {
        const glm::dvec3 p = box.c + box.e[0] * ((k & 1) ? 0.9 : -0.9) + box.e[1] * ((k & 2) ? 0.9 : -0.9) +
                             box.e[2] * ((k & 4) ? 0.9 : -0.9);
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
// brush (a blocker) by more than `inset` on every side (a coarse lattice of its centres, at or over minCz): a blocker
// it would feel. (Hull 0 has solids hull 1 lacks: detail brushes compiled without collision, as Arcane Dimensions'
// maps have.) The brush's planes (its bevels with them) tell whether the box reaches into it.
bool quakeBoxReaches(const hull_t& h1, int head, const Brushes& b, const Brush& br, double inset, double minCz,
    int maxSteps, glm::dvec3* where = nullptr)
{
    glm::dvec3 from, to, off, half;
    int steps[3];
    for(int k = 0; k < 3; ++k)
    {
        from[k] = br.mins[k] + inset - h1.clip_maxs[k];
        to[k] = br.maxs[k] - inset - h1.clip_mins[k];
        off[k] = (h1.clip_mins[k] + h1.clip_maxs[k]) * 0.5;
        half[k] = (h1.clip_maxs[k] - h1.clip_mins[k]) * 0.5 - inset;
    }
    from.z = za::max(from.z, minCz);
    for(int k = 0; k < 3; ++k)
    {
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
                bool into = true;
                for(za::U32 i = 0; i < br.count && into; ++i)
                {
                    const Plane& p = b.planes[br.first + i];
                    const glm::dvec3 n{p.normal};
                    into = glm::dot(n, c + off) - (za::abs(n.x) * half.x + za::abs(n.y) * half.y + za::abs(n.z) * half.z) <
                           static_cast<double>(p.dist);
                }
                const int contents = into ? pointContents(h1, head, c) : CONTENTS_SOLID;
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

// Exact (or erring toward yes): whether the shape of centres (c, half axes e, then grown by the box half) meets any of
// hull 1's empty leaves under node num (a leaf met only past its corners counts too: a blocker dropped that might have
// been kept).
bool shapeMeetsEmpty(const hull_t& h, int num, const glm::dvec3& c, const glm::dvec3* e, const glm::dvec3& half)
{
    while(num >= 0)
    {
        const mclipnode_t& node = h.clipnodes[num];
        const mplane_t& plane = h.planes[node.planenum];
        const glm::dvec3 n{plane.normal[0], plane.normal[1], plane.normal[2]};
        const double d = glm::dot(n, c) - plane.dist;
        const double r = za::abs(glm::dot(n, e[0])) + za::abs(glm::dot(n, e[1])) + za::abs(glm::dot(n, e[2])) +
                         za::abs(n.x) * half.x + za::abs(n.y) * half.y + za::abs(n.z) * half.z;
        if(d - r >= 0.0)
        {
            num = node.children[0];
        }
        else if(d + r < 0.0)
        {
            num = node.children[1];
        }
        else
        {
            if(shapeMeetsEmpty(h, node.children[0], c, e, half))
            {
                return true;
            }
            num = node.children[1];
        }
    }
    return num != CONTENTS_SOLID && num != CONTENTS_SKY;
}

// Quake's box (hull 1 from node head) placed anywhere it fits (its centre at or over minCz) reaching into the blocker
// by more than inset (its box less inset on every side meeting the blocker, as the census tests it): every centre whose
// box does so tested at once.
bool quakeBoxFits(const hull_t& h1, int head, const OBox& box, double inset, double minCz = -1e300)
{
    glm::dvec3 e[3], half, c = box.c;
    for(int k = 0; k < 3; ++k)
    {
        e[k] = box.e[k];
        c[k] -= (h1.clip_mins[k] + h1.clip_maxs[k]) * 0.5;
        half[k] = za::max(0.0, (h1.clip_maxs[k] - h1.clip_mins[k]) * 0.5 - inset);
    }
    // (e[2] is vertical, the others level: the centres' heights an interval, cut at minCz)
    const double zlo = za::max(c.z - e[2].z - half.z, minCz), zhi = c.z + e[2].z + half.z;
    if(zhi <= zlo)
    {
        return false;
    }
    c.z = (zlo + zhi) * 0.5;
    half.z = (zhi - zlo) * 0.5;
    e[2] = glm::dvec3{0.0};
    return shapeMeetsEmpty(h1, head, c, e, half);
}

// Over a fall: nothing under the blocker (its middle and its corners) for fallDepth, so Quake's box hanging under it
// could not be standing or jumping there. (Its corners a unit in: a grate's blocker runs to the pit's walls.)
bool overFall(const hull_t& h0, int head, const HoleBox& box)
{
    const double x0 = box.lo.x + 1.0, x1 = box.hi.x - 1.0, y0 = box.lo.y + 1.0, y1 = box.hi.y - 1.0;
    const double xs[5] = {(box.lo.x + box.hi.x) * 0.5, x0, x1, x0, x1};
    const double ys[5] = {(box.lo.y + box.hi.y) * 0.5, y0, y0, y1, y1};
    for(int k = 0; k < 5; ++k)
    {
        for(double d = 4.0; d <= fallDepth; d += 20.0)
        {
            if(pointContents(h0, head, glm::dvec3{xs[k], ys[k], box.lo.z - d}) != CONTENTS_EMPTY)
            {
                return false;
            }
        }
    }
    return true;
}

// Open above: nothing solid over the blocker's middle for a box's height and more (the way down a gap you'd fall
// through: not a rafter under a ceiling, where no one walks).
bool openAbove(const hull_t& h0, int head, const HoleBox& box)
{
    const glm::dvec3 c{(box.lo.x + box.hi.x) * 0.5, (box.lo.y + box.hi.y) * 0.5, box.hi.z};
    for(double d = 2.0; d <= quakeHeight + 10.0; d += 8.0)
    {
        const int contents = pointContents(h0, head, c + glm::dvec3{0.0, 0.0, d});
        if(contents == CONTENTS_SOLID || contents == CONTENTS_SKY)
        {
            return false;
        }
    }
    return true;
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

// A diagonal search's box (in (a, u, z) along tiltA[d]) as a brush: its six faces (+a, -a, +u, -u, +z, -z: tiltCorners
// reads them back), then its bounds as the axial bevels in x and y.
void emitTilt(Brushes& b, const HoleBox& box, int d)
{
    const auto first = static_cast<za::U32>(b.planes.size());
    const glm::dvec3 dirs[3] = {tiltA[d], tiltU[d], glm::dvec3{0.0, 0.0, 1.0}};
    for(int k = 0; k < 3; ++k)
    {
        b.planes.pushBack(Plane{glm::vec3{dirs[k]}, static_cast<float>(box.hi[k]), 1.f});
        b.planes.pushBack(Plane{glm::vec3{-dirs[k]}, static_cast<float>(-box.lo[k]), 1.f});
    }
    const OBox o = tiltBox(box, d);
    const glm::dvec3 r = glm::abs(o.e[0]) + glm::abs(o.e[1]) + glm::abs(o.e[2]);
    const glm::dvec3 lo = o.c - r, hi = o.c + r;
    for(int axis = 0; axis < 2; ++axis)
    {
        glm::vec3 n{0.f};
        n[axis] = 1.f;
        b.planes.pushBack(Plane{n, static_cast<float>(hi[axis]), 1.f});
        b.planes.pushBack(Plane{-n, static_cast<float>(-lo[axis]), 1.f});
    }
    b.brushes.pushBack(Brush{first, 10u, glm::vec3{lo}, glm::vec3{hi}, false});
}

// A blocker brush's corners (k: bit 0 its first axis's high side, bit 1 the second's, bit 2 z's): a diagonal one's
// from its faces (emitTilt), an axis box's from its bounds.
void blockerCorners(const Brushes& b, const Brush& br, glm::vec3* c)
{
    const glm::vec3 n0 = b.planes[br.first].normal;
    if(br.count == 10u && za::abs(n0.x) > 0.01f && za::abs(n0.y) > 0.01f)
    {
        const Plane* p = &b.planes[br.first];
        for(int k = 0; k < 8; ++k)
        {
            const float a = (k & 1) ? p[0].dist : -p[1].dist, u = (k & 2) ? p[2].dist : -p[3].dist;
            c[k] = p[0].normal * a + p[2].normal * u + glm::vec3{0.f, 0.f, (k & 4) ? p[4].dist : -p[5].dist};
        }
        return;
    }
    for(int k = 0; k < 8; ++k)
    {
        c[k] = glm::vec3{(k & 1) ? br.maxs.x : br.mins.x, (k & 2) ? br.maxs.y : br.mins.y, (k & 4) ? br.maxs.z : br.mins.z};
    }
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
        s.model = static_cast<int>(sub);
        bool upright = true, turned = false; // (an upright prism: every face level or upright; one turned in x and y)
        for(za::U32 k = 0; k < br.count && upright; ++k)
        {
            const glm::vec3& n = b.planes[br.first + k].normal;
            upright = za::abs(n.z) < 0.001f || za::abs(n.z) > 0.999f;
            turned = turned || (za::abs(n.x) > 0.01f && za::abs(n.y) > 0.01f);
        }
        s.slanted = upright && turned;
        lo = glm::min(lo, s.lo);
        hi = glm::max(hi, s.hi);
        if(group >= 0)
        {
            out.pushBack(s);
        }
    }
}

// The found boxes' checks on the pool. Each kept one goes into keep[i] (0 keep; 1 inside solid; 2 Quake's box reaches
// it; 3 not over a fall): own (the line test, then Quake's box), tilt (along diagonal d), stand (over a fall).
enum class Kind
{
    Own,
    Tilt0,
    Tilt1,
    Stand,
};

char checkOne(const HoleBox& x, Kind kind, const hull_t& h0, int head, const hull_t* h1, int head1)
{
    const OBox o = kind == Kind::Tilt0 ? tiltBox(x, 0) : kind == Kind::Tilt1 ? tiltBox(x, 1) : axisBox(x);
    if(insideSolid(h0, head, o))
    {
        return 1;
    }
    if(kind == Kind::Stand && (!overFall(h0, head, x) || !openAbove(h0, head, x)))
    {
        return 3;
    }
    if(!h1 || head1 < 0)
    {
        return kind == Kind::Own ? 0 : 2; // (no hull 1 to tell: only the line test's kept)
    }
    const bool reaches = kind == Kind::Stand
                             ? quakeBoxFits(*h1, head1, o, cfg.reach, x.lo.z - cfg.reach - h1->clip_mins[2])
                             : quakeBoxFits(*h1, head1, o, cfg.reach);
    return reaches ? 2 : 0;
}

void checkBoxes(const za::Vector<HoleBox>& boxes, Kind kind, const hull_t& h0, int head, const hull_t* h1, int head1,
    za::Vector<char>& verdict, bool pool = true)
{
    verdict.clear();
    verdict.resize(boxes.size(), 0);
    if(!pool) // (a brush model's, already on the pool)
    {
        for(za::SizeT i = 0; i < boxes.size(); ++i)
        {
            verdict[i] = checkOne(boxes[i], kind, h0, head, h1, head1);
        }
        return;
    }
    jobs::parallelFor(holesSite, boxes.size(), 256,
        [&](za::SizeT begin, za::SizeT end)
        {
            for(za::SizeT i = begin; i < end; ++i)
            {
                verdict[i] = checkOne(boxes[i], kind, h0, head, h1, head1);
            }
        });
}

// A merged box Quake's box reaches can hold pieces it doesn't (Quoth start's drain: the hole's blocker merged with a
// neighbour's it reaches, and the hole stayed half open): such a box is put back as its pieces (dedupe's list, sorted
// by lo.x), each checked on its own, the kept ones merged again. (Merging kept pieces keeps them kept: Quake's box
// less the reach meets a union only where it meets a piece.)
void splitDropped(za::Vector<HoleBox>& boxes, za::Vector<char>& verdict, const za::Vector<HoleBox>& pieces, Kind kind,
    const hull_t& h0, int head, const hull_t* h1, int head1, bool pool = true)
{
    constexpr double eps = 1e-6;
    za::Vector<HoleBox> sub;
    za::Vector<za::SizeT> owner;
    for(za::SizeT i = 0; i < boxes.size(); ++i)
    {
        if(verdict[i] != 2)
        {
            continue;
        }
        const HoleBox& m = boxes[i];
        za::SizeT lo = 0, hi = pieces.size(); // (the first piece with lo.x >= m.lo.x - eps)
        while(lo < hi)
        {
            const za::SizeT mid = (lo + hi) / 2;
            if(pieces[mid].lo.x < m.lo.x - eps)
            {
                lo = mid + 1;
            }
            else
            {
                hi = mid;
            }
        }
        const za::SizeT first = sub.size();
        for(za::SizeT j = lo; j < pieces.size() && pieces[j].lo.x <= m.hi.x + eps; ++j)
        {
            const HoleBox& p = pieces[j];
            bool inside = true;
            for(int k = 0; k < 3 && inside; ++k)
            {
                inside = p.lo[k] >= m.lo[k] - eps && p.hi[k] <= m.hi[k] + eps;
            }
            if(inside)
            {
                sub.pushBack(p);
                owner.pushBack(i);
            }
        }
        if(sub.size() - first < 2)
        {
            sub.resize(first); // (one piece: the box itself)
            owner.resize(first);
        }
    }
    if(sub.empty())
    {
        return;
    }
    za::Vector<char> sv;
    checkBoxes(sub, kind, h0, head, h1, head1, sv, pool);
    za::Vector<HoleBox> nb, kept;
    za::Vector<char> nv;
    za::SizeT k = 0;
    for(za::SizeT i = 0; i < boxes.size(); ++i)
    {
        if(k >= sub.size() || owner[k] != i)
        {
            nb.pushBack(boxes[i]);
            nv.pushBack(verdict[i]);
            continue;
        }
        kept.clear();
        for(; k < sub.size() && owner[k] == i; ++k)
        {
            if(sv[k] == 0)
            {
                kept.pushBack(sub[k]);
            }
            else
            {
                nb.pushBack(sub[k]);
                nv.pushBack(sv[k]);
            }
        }
        mergeBoxes(kept);
        for(const HoleBox& x : kept)
        {
            nb.pushBack(x);
            nv.pushBack(0);
        }
    }
    boxes = ZA_MOVE(nb);
    verdict = ZA_MOVE(nv);
}

// The boxes kept (verdict 0) emitted, the ones Quake's box reaches (2) into dropped (lo, hi pairs); their count.
za::U32 emitKept(Brushes& b, const za::Vector<HoleBox>& boxes, const za::Vector<char>& verdict, int d,
    za::Vector<glm::vec3>* dropped)
{
    za::U32 n = 0;
    for(za::SizeT i = 0; i < boxes.size(); ++i)
    {
        if(verdict[i] == 0)
        {
            if(d < 0)
            {
                emitBox(b, boxes[i]);
            }
            else
            {
                emitTilt(b, boxes[i], d);
            }
            ++n;
        }
        else if(verdict[i] == 2 && dropped)
        {
            const OBox o = d < 0 ? axisBox(boxes[i]) : tiltBox(boxes[i], d);
            const glm::dvec3 r = glm::abs(o.e[0]) + glm::abs(o.e[1]) + glm::abs(o.e[2]);
            dropped->pushBack(glm::vec3{o.c - r});
            dropped->pushBack(glm::vec3{o.c + r});
        }
    }
    return n;
}

void tidy(Found& f)
{
    dedupe(f.own);
    f.ownPieces = f.own;
    mergeBoxes(f.own);
    dedupe(f.cross);
    mergeBoxes(f.cross);
    for(int d = 0; d < 2; ++d)
    {
        dedupe(f.tilt[d]);
        f.tiltPieces[d] = f.tilt[d];
        mergeBoxes(f.tilt[d]);
    }
    dedupe(f.stand);
    f.standPieces = f.stand;
    mergeBoxes(f.stand);
}

// The corners between the cross blockers (a brush model's with the world): vrbrushholes' lift (his mark, backlog1122d)
// has 20-unit slots east and north of the plat, each blocked, and the 20 x 20 square between them stayed open (no pair
// faces across it: the plat's corner is diagonal to the shaft's). A blocker is space no point of Quake's box was ever
// in, so it counts as a solid for a second look: a gap under the limit between it and a solid along a line is one
// Quake's box was never in either (its 32 along the line reach the blocker or the solid). One pass: each cross blocker
// paired with the solids near it, both ways along x and y; the new boxes join the cross ones.
void crossCorners(const Brushes& b, const za::Vector<Solid>& solids, Found& found)
{
    if(found.cross.empty() || solids.empty())
    {
        return;
    }
    SolidGrid g;
    makeGrid(solids, g);
    Found more;
    for(const HoleBox& x : found.cross)
    {
        Solid blocker;
        blocker.lo = x.lo;
        blocker.hi = x.hi;
        blocker.group = 3;  // (not the model's own: the line test alone, as the cross pairs)
        blocker.model = -2; // (two blockers aren't a pair)
        const auto pair = [&](const Solid& s)
        {
            for(int a = 0; a < 2; ++a)
            {
                tryPair(b, blocker, s, a, false, more);
                tryPair(b, s, blocker, a, false, more);
            }
        };
        const double r = cfg.maxGap;
        const int cx0 = g.cellX(x.lo.x - r), cx1 = g.cellX(x.hi.x + r);
        const int cy0 = g.cellY(x.lo.y - r), cy1 = g.cellY(x.hi.y + r);
        for(int cy = cy0; cy <= cy1; ++cy)
        {
            for(int cx = cx0; cx <= cx1; ++cx)
            {
                const auto c = static_cast<za::SizeT>(cy) * static_cast<za::SizeT>(g.nx) + static_cast<za::SizeT>(cx);
                for(za::U32 k = g.start[c]; k < g.start[c + 1]; ++k)
                {
                    const Solid& s = solids[g.list[k]];
                    if(za::max(cx0, g.cellX(s.lo.x)) == cx && za::max(cy0, g.cellY(s.lo.y)) == cy)
                    {
                        pair(s); // (each solid once: in the first cell of both)
                    }
                }
            }
        }
        for(const za::U32 k : g.big)
        {
            pair(solids[k]);
        }
    }
    if(more.cross.empty())
    {
        return;
    }
    found.cross.emplaceBackRange(more.cross.data(), more.cross.size());
    dedupe(found.cross);
    mergeBoxes(found.cross);
}

} // namespace

// (the last build's phases, ms: set up, search, dedupe and merge, checks and emit: vr_hull_holes_census)
// seat-shared: the server's map build's timings
double holePhases[4] = {};

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
    const Settings s = readSettings(); // (and the settings)
    const double values[] = {s.maxGap, s.minGap, s.wide, s.minCell, s.reach, static_cast<double>(s.maxCells),
        s.diag ? 1.0 : 0.0, s.models ? 1.0 : 0.0, s.vert ? 1.0 : 0.0};
    for(const double v : values)
    {
        h = (h ^ static_cast<za::U64>(v * 1024.0)) * 0x100000001b3ull;
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
    cfg = readSettings();
    const auto t0 = za::Clock::nowNanoseconds();
    const hull_t& h0 = world->hulls[0];
    za::Vector<BrushEnt> ents;
    readBrushEnts(world->entities, ents);
    const auto subCount = static_cast<int>(b.subs.size());

    // The world: its own pairs, then the pairs with the brush models that stay (a func_wall no one removes) or travel a
    // line (a door's or a lift's travel as one box), and theirs with each other.
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
            // A lift: its whole travel as one box (the shaft's slots beside it stay shut wherever it is). A door: its
            // closed box, the blockers beside it in its own tree, moving with it (an opened door takes them along:
            // DM1's sinking grate bars, backlog1122d; its travel's box left them standing in the opened way).
            const bool plat = e.classname[5] == 'p';
            const glm::dvec3 move{0.0, 0.0, plat ? -(e.height > 0.0 ? e.height : (hi.z - lo.z) - 8.0) : 0.0};
            Solid s;
            s.lo = glm::min(lo, lo + move);
            s.hi = glm::max(hi, hi + move);
            s.group = 2;
            s.model = e.sub;
            s.attach = !plat;
            solids.pushBack(s);
        }
    }
    Found found;
    const auto tA = za::Clock::nowNanoseconds();
    searchPairs(b, solids, true, found);
    b.holePairs += found.pairs;
    const auto tB = za::Clock::nowNanoseconds();
    tidy(found);
    crossCorners(b, solids, found);
    const auto tC = za::Clock::nowNanoseconds();
    const hull_t& h1 = world->hulls[1];
    const bool haveHull1 = h1.clipnodes && h1.firstclipnode <= h1.lastclipnode;
    {
        const int head = b.subs[0].head;
        SubModel& w = b.subs[0];
        w.firstHole = static_cast<za::U32>(b.brushes.size());
        b.holeDropped.clear();
        // [the line test's][the diagonals'][over a fall][with brush models: the line test's alone]
        za::Vector<char> verdict;
        const hull_t* hull1 = haveHull1 ? &h1 : nullptr;
        checkBoxes(found.own, Kind::Own, h0, head, hull1, h1.firstclipnode, verdict);
        splitDropped(found.own, verdict, found.ownPieces, Kind::Own, h0, head, hull1, h1.firstclipnode);
        za::U32 own = emitKept(b, found.own, verdict, -1, &b.holeDropped);
        for(int d = 0; d < 2; ++d)
        {
            const Kind kind = d ? Kind::Tilt1 : Kind::Tilt0;
            checkBoxes(found.tilt[d], kind, h0, head, hull1, h1.firstclipnode, verdict);
            splitDropped(found.tilt[d], verdict, found.tiltPieces[d], kind, h0, head, hull1, h1.firstclipnode);
            own += emitKept(b, found.tilt[d], verdict, d, &b.holeDropped);
        }
        checkBoxes(found.stand, Kind::Stand, h0, head, hull1, h1.firstclipnode, verdict);
        splitDropped(found.stand, verdict, found.standPieces, Kind::Stand, h0, head, hull1, h1.firstclipnode);
        w.standHoles = emitKept(b, found.stand, verdict, -1, &b.holeDropped);
        own += w.standHoles;
        for(const HoleBox& x : found.cross)
        {
            if(!insideSolid(h0, head, axisBox(x)))
            {
                emitBox(b, x);
            }
        }
        w.numHoles = static_cast<za::U32>(b.brushes.size()) - w.firstHole;
        w.crossHoles = w.numHoles - own;
    }
    const auto tD = za::Clock::nowNanoseconds();
    holePhases[0] = za::nanosecondsToMilliseconds(tA - t0);
    holePhases[1] = za::nanosecondsToMilliseconds(tB - tA);
    holePhases[2] = za::nanosecondsToMilliseconds(tC - tB);
    holePhases[3] = za::nanosecondsToMilliseconds(tD - tC);

    // Each solid brush model's own gaps (a gate's bars, a func_wall grate): blockers that move with it. The models found
    // on the pool together (each kept blocker in its own list), then added in the entities' order.
    struct ModelWork
    {
        za::SizeT sub = 0;
        za::Vector<HoleBox> kept, keptTilt[2];
        int pairs = 0;
    };
    za::Vector<ModelWork> models;
    const auto attachedTo = [&](int sub)
    {
        for(const int m : found.attachedModel)
        {
            if(m == sub)
            {
                return true;
            }
        }
        return false;
    };
    for(const BrushEnt& e : ents)
    {
        if(e.sub >= subCount || !strings::startsWith(e.classname, "func_") || !strcmp(e.classname, "func_illusionary") ||
            (b.subs[static_cast<za::SizeT>(e.sub)].numBrushes < 2 && !attachedTo(e.sub)))
        {
            continue;
        }
        bool seen = false;
        for(const ModelWork& m : models)
        {
            seen = seen || m.sub == static_cast<za::SizeT>(e.sub);
        }
        if(!seen)
        {
            models.emplaceBack().sub = static_cast<za::SizeT>(e.sub);
        }
    }
    jobs::parallelFor(holesSite, models.size(), 1,
        [&](za::SizeT begin, za::SizeT end)
        {
            for(za::SizeT k = begin; k < end; ++k)
            {
                ModelWork& mw = models[k];
                za::Vector<Solid> own;
                glm::dvec3 mlo, mhi;
                addSolids(b, mw.sub, 0, own, mlo, mhi);
                Found f;
                searchPairs(b, own, false, f);
                mw.pairs = f.pairs;
                tidy(f);
                const int head = b.subs[mw.sub].head;
                const int head1 = world->submodels[mw.sub].headnode[1];
                const hull_t* mh1 = haveHull1 && head1 >= 0 ? &h1 : nullptr;
                za::Vector<char> v;
                const auto keep = [&](za::Vector<HoleBox>& boxes, const za::Vector<HoleBox>& pieces, Kind kind,
                                      za::Vector<HoleBox>& kept)
                {
                    checkBoxes(boxes, kind, h0, head, mh1, head1, v, false);
                    splitDropped(boxes, v, pieces, kind, h0, head, mh1, head1, false);
                    for(za::SizeT i = 0; i < boxes.size(); ++i)
                    {
                        if(v[i] == 0)
                        {
                            kept.pushBack(boxes[i]);
                        }
                    }
                };
                keep(f.own, f.ownPieces, Kind::Own, mw.kept);
                keep(f.tilt[0], f.tiltPieces[0], Kind::Tilt0, mw.keptTilt[0]);
                keep(f.tilt[1], f.tiltPieces[1], Kind::Tilt1, mw.keptTilt[1]);
            }
        });
    for(const ModelWork& mw : models)
    {
        b.holePairs += mw.pairs;
        SubModel& m = b.subs[mw.sub];
        m.firstHole = static_cast<za::U32>(b.brushes.size());
        for(const HoleBox& x : mw.kept)
        {
            emitBox(b, x);
        }
        for(int d = 0; d < 2; ++d)
        {
            for(const HoleBox& x : mw.keptTilt[d])
            {
                emitTilt(b, x, d);
            }
        }
        // The blockers between its closed box and the world (a door's: in the world's coordinates as it stands shut,
        // which are its own: its origin is 0).
        za::Vector<HoleBox> attached;
        for(za::SizeT i = 0; i < found.attached.size(); ++i)
        {
            if(found.attachedModel[i] == static_cast<int>(mw.sub))
            {
                attached.pushBack(found.attached[i]);
            }
        }
        dedupe(attached);
        mergeBoxes(attached);
        const auto ownEnd = static_cast<za::U32>(b.brushes.size());
        for(const HoleBox& x : attached)
        {
            if(!insideSolid(h0, b.subs[0].head, axisBox(x)))
            {
                emitBox(b, x);
            }
        }
        m.numHoles = static_cast<za::U32>(b.brushes.size()) - m.firstHole;
        m.crossHoles = static_cast<za::U32>(b.brushes.size()) - ownEnd;
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
    const double inset = cfg.reach;
    int reach = 0, tilted = 0, reachTilt = 0, reachStand = 0;
    double area = 0.0;
    glm::dvec3 firstReach{0.0};
    for(za::U32 i = 0; i < own; ++i)
    {
        const Brush& br = b.brushes[w.firstHole + i];
        const glm::dvec3 lo{br.mins}, hi{br.maxs};
        area += (hi.x - lo.x) * (hi.y - lo.y);
        tilted += br.count == 10u ? 1 : 0;
        // (the ones over a fall: Quake's box standing, its bottom no lower than theirs)
        const double minCz = i >= own - w.standHoles ? lo.z - inset - h1.clip_mins[2] : -1e300;
        glm::dvec3 c{0.0};
        const bool hit = quakeBoxReaches(h1, h1.firstclipnode, b, br, inset, minCz, 16, &c);
        if(hit && !reach)
        {
            firstReach = c;
        }
        reach += hit ? 1 : 0;
        reachTilt += hit && br.count == 10u ? 1 : 0;
        reachStand += hit && i >= own - w.standHoles ? 1 : 0;
    }
    // ... and the brush models' own, in each model's hull 1 (counted in reach too).
    for(za::SizeT s = 1; s < b.subs.size() && static_cast<int>(s) < world->numsubmodels; ++s)
    {
        const SubModel& sm = b.subs[s];
        const int head1 = world->submodels[s].headnode[1];
        for(za::U32 i = 0; i < sm.numHoles - sm.crossHoles && head1 >= 0; ++i) // (a door's with the world: not its own)
        {
            const Brush& br = b.brushes[sm.firstHole + i];
            tilted += br.count == 10u ? 1 : 0;
            glm::dvec3 c{0.0};
            if(quakeBoxReaches(h1, head1, b, br, inset, -1e300, 16, &c))
            {
                if(!reach)
                {
                    firstReach = c;
                }
                ++reach;
            }
        }
    }
    Con_Printf("holes: %s: world %u blockers (%u its own, %u of them over a fall, %u with brush models), %d in %d brush "
               "models, %d diagonal; %d pairs, %.1f ms; Quake's box fits into %d of its own%s\n",
        world->name, w.numHoles, own, w.standHoles, w.crossHoles, modelHoles, models, tilted, b.holePairs, b.holeMs, reach,
        reach ? va(" (first: centre %.0f %.0f %.0f)", firstReach.x, firstReach.y, firstReach.z) : "");
    const Tree& pt = tree; // (the player's: its compile, for the blockers' share of it)
    Con_Printf("holes: phases %.1f %.1f %.1f %.1f ms\n", holePhases[0], holePhases[1], holePhases[2], holePhases[3]);
    Con_Printf("HOLECENSUS|%s|%u|%u|%d|%d|%d|%.1f|%d|%.0f|%.1f|%zu|%d|%d|%u|%d|%d\n", world->name, own, w.crossHoles, modelHoles,
        models, b.holePairs, b.holeMs, reach, area, pt.ms, static_cast<size_t>(pt.nodes.size()), pt.fromDisk ? 1 : 0,
        tilted, w.standHoles, reachTilt, reachStand);
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
                    i >= sm.numHoles - sm.crossHoles ? ", with a brush model" : "", lo.x, lo.y, lo.z, hi.x, hi.y,
                    hi.z);
            }
        }
    }
    Con_Printf("vr_hull_holes_near: %d blockers within %.0f units\n", total, radius);
}

// vr_hull_holes_test: on maps/vrbrushholes (Misc/quakevr/make_vrbrushholes_map.py: the places below), the player's box
// and a narrow monster's (a grunt at vr_mhull_army 16) at a grate, a bar gate, a lift's edge, a pillar row, a diagonal
// grate, a louvred grate, two func_walls, a drain (a 32 x 32 hole: Quoth start's) and the lift's corner; with
// vr_hull_holes on they are stopped (and still get 8 units from a face), off they pass (the scene is a real hole).
// Verdict: "brushholes: N passed, M failed" (off: "brushholes_off: ...").
// On maps/dm1.bsp (his mark, backlog1122d): the grate at x 352..512, y 1352..1368: world bars at x 376..392 and
// 480..496, and between them two sinking bars (func_doors "t2", angle -2, x 412..428 and 448..464). Shut, a narrow
// box stops 8 from them (holes on; off it slips between the two); opened (moved down as QC opens them: their height
// less the lip of 8), the player walks through between them, holes on or off. Verdict "brushholes_dm1: ..."
// ("brushholes_dm1_off: ...").
void holeTestDm1(edict_t* player, bool on)
{
    const char* name = on ? "brushholes_dm1" : "brushholes_dm1_off";
    testkit::Tally t;
    edict_t* bars[2] = {};
    int found = 0;
    const progs::ServerVm vm; // (the server's edicts and strings)
    for(int i = 1; i < sv.qcvm.num_edicts && found < 2; ++i)
    {
        edict_t* e = EDICT_NUM(i);
        if(!e->free && !strcmp(PR_GetString(e->v.classname), "door") && !strcmp(PR_GetString(e->v.targetname), "t2"))
        {
            bars[found++] = e;
        }
    }
    vec3_t was;
    VectorCopy(player->v.origin, was);
    char what[256];
    const auto walk = [&](float z, bool pass, const char* scene)
    {
        vec3_t a = {438.f, 1300.f, z}, e = {438.f, 1420.f, z};
        VectorCopy(a, player->v.origin);
        const trace_t tr = SV_Move(a, player->v.mins, player->v.maxs, e, MOVE_NOMONSTERS, player);
        const bool ok = pass ? tr.fraction == 1.f : za::abs(1352.f - 8.f - tr.endpos[1]) < 0.2f;
        snprintf(what, sizeof(what), "%s: ended at %.2f %.2f %.2f (fraction %.3f)", scene, tr.endpos[0], tr.endpos[1],
            tr.endpos[2], tr.fraction);
        if(t.check(ok, name, what))
        {
            Con_Printf("%s: PASS %s\n", name, what);
        }
    };
    if(t.check(found == 2, name, "the two sinking bars (func_door t2)"))
    {
        walk(-120.f, !on, on ? "shut: stopped 8 from the bars" : "shut: slipped between the bars");
        for(edict_t* e : bars)
        {
            e->v.origin[2] = -(e->v.size[2] - 8.f); // (QC door: pos2 = pos1 + movedir * (size - lip))
            SV_LinkEdict(e, false);
        }
        walk(-110.f, true, "opened: walked through"); // (feet -134: over the opened bars' 8-high lips)
        for(edict_t* e : bars)
        {
            e->v.origin[2] = 0.f;
            SV_LinkEdict(e, false);
        }
    }
    VectorCopy(was, player->v.origin);
    SV_LinkEdict(player, false);
    t.verdict(name);
}

void holeTest_f()
{
    const bool on = vr_hull_holes.value != 0.f;
    const char* name = on ? "brushholes" : "brushholes_off";
    testkit::Tally t;
    edict_t* player = progs::localPlayer();
    if(player && sv.worldmodel && !strcmp(sv.worldmodel->name, "maps/dm1.bsp"))
    {
        holeTestDm1(player, on);
        return;
    }
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
        // The square between the two slots (x 568..588, y 24..44: the plat's corner diagonal to the shaft's; his mark).
        tr = move(who, 578.f, 34.f, 153.f, 578.f, 34.f, 30.f);
        report(on ? tr.endpos[2] > 151.9f : tr.endpos[2] < 100.f, label,
            on ? "lift corner drop stopped" : "lift corner drop fell", tr);
        // The pillars (16 wide, 24 apart, y 448..464): through a gap.
        tr = move(who, 188.f, 380.f, 24.f, 188.f, 530.f, 24.f);
        report(on ? za::abs(448.f - 8.f - tr.endpos[1]) < 0.2f : tr.fraction == 1.f, label,
            on ? "pillar gap stopped 8 from them" : "pillar gap passed", tr);
        // The diagonal grate (pit x -768..-568, y -560..-360; bars 8 thick turned 45 degrees, 30 apart across them, 42
        // along x and y: the line test misses them; Quake's box needs 45.25 across): down through the middle.
        tr = move(who, -668.f, -460.f, 25.f, -668.f, -460.f, -100.f);
        report(on ? tr.endpos[2] > 23.9f : tr.endpos[2] < 0.f, label,
            on ? "diagonal grate drop stopped on it" : "diagonal grate drop fell", tr);
        tr = move(who, -668.f, -545.f, 25.f, -668.f, -375.f, 25.f);
        report(tr.fraction == 1.f, label, "walk across the diagonal grate", tr);
        // The louvred grate (pit x -768..-568, y -300..-100): bars at z -8..0 with 48-unit gaps, under each gap a bar at
        // z -16..-8: 20-unit gaps seen from above, none along a line. The second gap (y -236..-216: the first one's lower
        // bar is 28 from the pit's wall, a gap along a line).
        tr = move(who, -668.f, -226.f, 25.f, -668.f, -226.f, -100.f);
        report(on ? tr.endpos[2] > 15.9f : tr.endpos[2] < -16.f, label,
            on ? "louvred grate drop stopped at the lower bars" : "louvred grate drop fell", tr);
        // Two func_walls (x 700..764 and 788..852, y -500..-436, 128 high): through the 24-unit gap between them.
        tr = move(who, 776.f, -560.f, 24.f, 776.f, -380.f, 24.f);
        report(on ? za::abs(-500.f - 8.f - tr.endpos[1]) < 0.2f : tr.fraction == 1.f, label,
            on ? "gap between two func_walls stopped 8 from them" : "gap between two func_walls passed", tr);
        // The drain (Quoth start's shape: a plinth, top z 32, its frame z 16..32 round a 32 x 32 hole at x -316..-284,
        // y -316..-284 over a pit to z 0): down through the hole exactly as wide as Quake's box, then across it.
        tr = move(who, -300.f, -300.f, 57.f, -300.f, -300.f, -60.f);
        report(on ? tr.endpos[2] > 55.9f : tr.endpos[2] < 30.f, label, on ? "drain drop stopped on it" : "drain drop fell",
            tr);
        tr = move(who, -300.f, -370.f, 57.f, -300.f, -230.f, 57.f);
        report(tr.fraction == 1.f, label, "walk across the drain", tr);
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

// A line of vr_hull_holes_mark's report: to the console and the marks file (when it opened).
void markLine(FILE* f, const char* fmt, ...)
{
    char text[640];
    va_list args;
    va_start(args, fmt);
    vsnprintf(text, sizeof(text), fmt, args);
    va_end(args);
    Con_Printf("%s\n", text);
    if(f)
    {
        fprintf(f, "%s\n", text);
    }
}

// vr_hull_holes_mark [radius] [note...]: a gap you got (or fell) through that you shouldn't have, reported for a worker
// to see why it stayed open: the map, where you are, the settings (now and as built), where Quake's box fits round you,
// the world's solids near you (and up to 256 above: the grate you fell through), the gaps on the lines through you
// along x, y and the diagonals, and the blockers and dropped boxes near. Printed, and added to
// quakevr/hull_holes_marks.txt (HULLS.md, "Holes").
void mark_f()
{
    qmodel_t* world = sv.worldmodel;
    const Brushes* bp = sv.active ? worldBrushes(world) : nullptr;
    const edict_t* player = progs::localPlayer();
    if(!bp || !player || bp->subs.empty())
    {
        Con_Printf("vr_hull_holes_mark: needs a map\n");
        return;
    }
    const Brushes& b = *bp;
    const progs::ServerVm vm; // (the server's time)
    const double radius = Cmd_Argc() > 1 ? za::clamp(static_cast<double>(Q_atof(Cmd_Argv(1))), 16.0, 1024.0) : 96.0;
    char note[256] = "";
    for(int i = 2; i < Cmd_Argc(); ++i)
    {
        q_strlcat(note, Cmd_Argv(i), sizeof(note));
        q_strlcat(note, " ", sizeof(note));
    }
    const glm::dvec3 at{player->v.origin[0], player->v.origin[1], player->v.origin[2]};
    const char* path = va("%s/hull_holes_marks.txt", com_gamedir);
    char file[MAX_OSPATH];
    q_strlcpy(file, path, sizeof(file));
    FILE* f = fopen(file, "a");
    markLine(f, "mark: %s at %.1f %.1f %.1f (feet %.1f), angles %.0f %.0f, time %.1f; your width %.0f%s%s", world->name,
        at.x, at.y, at.z, at.z + player->v.mins[2], player->v.angles[0], player->v.angles[1], qcvm->time, vr_hull_width.value,
        note[0] ? "; note: " : "", note);
    const Settings now = readSettings();
    markLine(f, "mark: settings now: on %d max %g min %g wide %g diag %d models %d vert %d cells %d cell %g reach %g; "
                "as built: max %g min %g wide %g diag %d models %d vert %d cells %d cell %g reach %g",
        vr_hull_holes.value != 0.f ? 1 : 0, now.maxGap, now.minGap, now.wide, now.diag, now.models, now.vert, now.maxCells,
        now.minCell, now.reach, cfg.maxGap, cfg.minGap, cfg.wide, cfg.diag, cfg.models, cfg.vert, cfg.maxCells, cfg.minCell,
        cfg.reach);
    const hull_t& h1 = world->hulls[1];
    char fits[160] = "";
    for(int k = 0; k < 6; ++k)
    {
        const double dz = -32.0 + 32.0 * k;
        const int c = h1.clipnodes ? pointContents(h1, h1.firstclipnode, at + glm::dvec3{0.0, 0.0, dz}) : CONTENTS_SOLID;
        q_strlcat(fits, va(" %+.0f:%s", dz, c == CONTENTS_SOLID || c == CONTENTS_SKY ? "no" : "yes"), sizeof(fits));
    }
    markLine(f, "mark: Quake's box fits with its centre here (by height off yours):%s", fits);

    // The world's solids near (their planes when slanted), and the heights to look along.
    za::Vector<Solid> all, nearby;
    glm::dvec3 lo, hi;
    addSolids(b, 0, 0, all, lo, hi);
    const glm::dvec3 qlo{at.x - radius, at.y - radius, at.z - radius}, qhi{at.x + radius, at.y + radius, at.z + 256.0};
    for(const Solid& s : all)
    {
        if(s.hi.x >= qlo.x && s.lo.x <= qhi.x && s.hi.y >= qlo.y && s.lo.y <= qhi.y && s.hi.z >= qlo.z && s.lo.z <= qhi.z)
        {
            nearby.pushBack(s);
        }
    }
    za::Vector<double> heights;
    heights.pushBack(at.z + player->v.mins[2] + 1.0);
    heights.pushBack(at.z);
    for(za::SizeT i = 0; i < nearby.size(); ++i)
    {
        const Solid& s = nearby[i];
        if(i < 48)
        {
            markLine(f, "mark: solid %d: %.1f %.1f %.1f .. %.1f %.1f %.1f, %u planes%s", static_cast<int>(i), s.lo.x, s.lo.y,
                s.lo.z, s.hi.x, s.hi.y, s.hi.z, s.count, s.slanted ? ", slanted:" : "");
            for(za::U32 k = 0; k < s.count && s.slanted; ++k)
            {
                const Plane& p = b.planes[s.first + k];
                if(za::abs(p.normal.x) > 0.001f && za::abs(p.normal.x) < 0.999f)
                {
                    markLine(f, "mark:   plane %.4f %.4f %.4f dist %.2f", p.normal.x, p.normal.y, p.normal.z, p.dist);
                }
            }
        }
        const double mid = (s.lo.z + s.hi.z) * 0.5;
        bool have = false;
        for(const double z : heights)
        {
            have = have || za::abs(z - mid) < 4.0;
        }
        if(!have && heights.size() < 10 && s.hi.z - s.lo.z <= 64.0)
        {
            heights.pushBack(mid);
        }
    }
    markLine(f, "mark: %d solids within %.0f (and 256 above)%s", static_cast<int>(nearby.size()), radius,
        nearby.size() > 48 ? " (the first 48 listed)" : "");

    // The gaps on the lines through you at those heights: the nearest solid behind and ahead along x, y, the diagonals.
    static constexpr const char* lineNames[4] = {"x", "y", "diagonal x+y", "diagonal x-y"};
    for(const double z : heights)
    {
        const glm::dvec3 p{at.x, at.y, z};
        for(int l = 0; l < 4; ++l)
        {
            const double t = l < 2 ? p[l] : glm::dot(p, tiltA[l - 2]);
            double behind = -1e300, ahead = 1e300;
            bool inside = false;
            for(const Solid& s : nearby)
            {
                double sl = 0.0, sh = 0.0;
                const bool meets = l == 0   ? extentOnLine(b, s, 0, 1, 2, p.y, p.z, sl, sh)
                                   : l == 1 ? extentOnLine(b, s, 1, 2, 0, p.z, p.x, sl, sh)
                                            : extentTilted(b, s, l - 2, glm::dot(p, tiltU[l - 2]), p.z, sl, sh);
                if(!meets)
                {
                    continue;
                }
                inside = inside || (sl < t && sh > t);
                if(sh <= t)
                {
                    behind = za::max(behind, sh);
                }
                if(sl >= t)
                {
                    ahead = za::min(ahead, sl);
                }
            }
            if(!inside && behind > -1e299 && ahead < 1e299 && ahead - behind < 128.0)
            {
                markLine(f, "mark: line at z %.1f along %s: gap %.2f (%.2f .. %.2f; limit %.2f)", z, lineNames[l],
                    ahead - behind, behind, ahead, l < 2 ? cfg.maxGap : cfg.maxGap / rootHalf);
            }
        }
    }

    // The blockers near (the world's by kind, the brush models' in their own space), and the ones left out.
    const SubModel& w = b.subs[0];
    const za::U32 crossFrom = w.numHoles - w.crossHoles, standFrom = crossFrom - w.standHoles;
    int shown = 0;
    for(za::SizeT s = 0; s < b.subs.size(); ++s)
    {
        const SubModel& sm = b.subs[s];
        for(za::U32 i = 0; i < sm.numHoles; ++i)
        {
            const Brush& br = b.brushes[sm.firstHole + i];
            const glm::dvec3 blo{br.mins}, bhi{br.maxs};
            if(glm::length(glm::clamp(at, blo, bhi) - at) > radius)
            {
                continue; // (a brush model's: in its own space, where it is in the map when it hasn't moved)
            }
            const char* kind = s != 0 ? "a brush model's own" : i >= crossFrom ? "with a brush model" :
                               i >= standFrom ? "over a fall" : br.count == 10u ? "diagonal" : "the world's own";
            if(shown++ < 24)
            {
                markLine(f, "mark: blocker (model %d, %s): %.1f %.1f %.1f .. %.1f %.1f %.1f", static_cast<int>(s), kind,
                    blo.x, blo.y, blo.z, bhi.x, bhi.y, bhi.z);
            }
        }
    }
    int dropped = 0;
    for(za::SizeT i = 0; i + 1 < b.holeDropped.size(); i += 2)
    {
        const glm::dvec3 dlo{b.holeDropped[i]}, dhi{b.holeDropped[i + 1]};
        if(glm::length(glm::clamp(at, dlo, dhi) - at) <= radius && dropped++ < 16)
        {
            markLine(f, "mark: left out (Quake's box reaches it): %.1f %.1f %.1f .. %.1f %.1f %.1f", dlo.x, dlo.y, dlo.z,
                dhi.x, dhi.y, dhi.z);
        }
    }
    markLine(f, "mark: %d blockers and %d left out within %.0f; %s %s", shown, dropped, radius,
        f ? "added to" : "could not open", file);
    if(f)
    {
        fprintf(f, "\n");
        fclose(f);
    }
}

// vr_hull_holes_rebuild: the gap blockers found again with the settings as they are now, without leaving the map: the
// game saved and loaded (holes_rebuild.sav; the map's brushes and trees are built again as their key has the
// settings). One player only; with others, the next map load.
void rebuild_f()
{
    if(!sv.active)
    {
        Con_Printf("vr_hull_holes_rebuild: needs a map\n");
        return;
    }
    if(svs.maxclients > 1)
    {
        Con_Printf("vr_hull_holes_rebuild: one player only (with others they are found again at the next map load)\n");
        return;
    }
    Cbuf_AddText("save holes_rebuild\nload holes_rebuild\n");
}

} // namespace

namespace
{

// A box by its corners (blockerCorners' order), moved by at.
void drawCorners(const glm::vec3* corners, const glm::vec3& at, const glm::vec4& edge, const glm::vec4& face)
{
    glm::vec3 c[8];
    for(int k = 0; k < 8; ++k)
    {
        c[k] = corners[k] + at;
    }
    static constexpr int edges[12][2] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3}, {4, 6}, {5, 7}, {0, 4}, {1, 5},
        {2, 6}, {3, 7}};
    for(const auto& e : edges)
    {
        lines::sceneLine(c[e[0]], c[e[1]], 0.6f, edge, edge);
    }
    static constexpr int quads[6][4] = {{0, 1, 3, 2}, {4, 6, 7, 5}, {0, 4, 5, 1}, {2, 3, 7, 6}, {0, 2, 6, 4}, {1, 5, 7, 3}};
    for(const auto& q : quads)
    {
        lines::triangle(c[q[0]], c[q[1]], c[q[2]], face);
        lines::triangle(c[q[0]], c[q[2]], c[q[3]], face);
    }
}

bool nearView(const glm::vec3& lo, const glm::vec3& hi, const glm::vec3& view, float range)
{
    return glm::length(glm::clamp(view, lo, hi) - view) <= range;
}

void drawBox(const glm::vec3& lo, const glm::vec3& hi, const glm::vec4& edge, const glm::vec4& face)
{
    glm::vec3 c[8];
    for(int k = 0; k < 8; ++k)
    {
        c[k] = glm::vec3{(k & 1) ? hi.x : lo.x, (k & 2) ? hi.y : lo.y, (k & 4) ? hi.z : lo.z};
    }
    drawCorners(c, glm::vec3{0.f}, edge, face);
}

void drawBlocker(const Brushes& b, const Brush& br, const glm::vec3& at, const glm::vec4& edge, const glm::vec4& face)
{
    glm::vec3 c[8];
    blockerCorners(b, br, c);
    drawCorners(c, at, edge, face);
}

} // namespace

} // namespace qvr::hull::detail

namespace qvr::hull
{

void holesDebugDraw(const glm::vec3& view, float yaw)
{
    using namespace detail;
    if(vr_hull_holes_show.value == 0.f || !sv.active || !sv.worldmodel)
    {
        return;
    }
    const Brushes* bp = worldBrushes(sv.worldmodel);
    if(!bp || bp->subs.empty())
    {
        return;
    }
    const Brushes& b = *bp;
    const float range = za::max(64.f, vr_hull_holes_show_range.value);
    constexpr za::SizeT maxDrawn = 4000;
    const glm::vec4 cyan{0.2f, 0.9f, 1.f, 0.9f}, yellow{1.f, 0.85f, 0.2f, 0.9f}, magenta{1.f, 0.3f, 0.9f, 0.9f},
        red{1.f, 0.2f, 0.15f, 0.9f}, green{0.35f, 1.f, 0.35f, 0.9f};
    const auto faceOf = [](const glm::vec4& c) { return glm::vec4{c.r, c.g, c.b, 0.16f}; };
    za::SizeT drawn = 0;
    const SubModel& w = b.subs[0];
    const za::U32 crossFrom = w.numHoles - w.crossHoles, standFrom = crossFrom - w.standHoles;
    for(za::U32 i = 0; i < w.numHoles && drawn < maxDrawn; ++i)
    {
        const Brush& br = b.brushes[w.firstHole + i];
        if(nearView(br.mins, br.maxs, view, range))
        {
            const glm::vec4& col = i >= crossFrom ? yellow : i >= standFrom ? green : cyan;
            drawBlocker(b, br, glm::vec3{0.f}, col, faceOf(col));
            ++drawn;
        }
    }
    for(za::SizeT i = 0; i + 1 < b.holeDropped.size() && drawn < maxDrawn; i += 2)
    {
        if(nearView(b.holeDropped[i], b.holeDropped[i + 1], view, range))
        {
            drawBox(b.holeDropped[i], b.holeDropped[i + 1], red, faceOf(red));
            ++drawn;
        }
    }
    // The brush models' own, where their entities are now.
    int modelHoles = 0;
    {
        const progs::ServerVm vm; // (the view's draws run with the client's VM current)
        for(int n = 1; n < qcvm->num_edicts && drawn < maxDrawn; ++n)
        {
            const edict_t* e = EDICT_NUM(n);
            if(e->free || !e->v.model)
            {
                continue;
            }
            const char* model = PR_GetString(e->v.model);
            if(model[0] != '*')
            {
                continue;
            }
            const int sub = atoi(model + 1);
            if(sub <= 0 || sub >= static_cast<int>(b.subs.size()))
            {
                continue;
            }
            const SubModel& sm = b.subs[static_cast<za::SizeT>(sub)];
            const glm::vec3 at{e->v.origin[0], e->v.origin[1], e->v.origin[2]};
            modelHoles += static_cast<int>(sm.numHoles);
            for(za::U32 i = 0; i < sm.numHoles && drawn < maxDrawn; ++i)
            {
                const Brush& br = b.brushes[sm.firstHole + i];
                if(nearView(br.mins + at, br.maxs + at, view, range))
                {
                    drawBlocker(b, br, at, magenta, faceOf(magenta));
                    ++drawn;
                }
            }
        }
    }
    if(key_dest != key_game)
    {
        return;
    }
    char text[256];
    snprintf(text, sizeof(text),
        "GAP BLOCKERS%s\nworld %u (cyan), over a fall %u (green), with brush models %u (yellow)\nin brush models %d "
        "(magenta), left out %d (red)\n%d drawn within %.0f units",
        vr_hull_holes.value != 0.f ? "" : " (off: vr_hull_holes 0)", standFrom, w.standHoles, w.crossHoles, modelHoles,
        static_cast<int>(b.holeDropped.size() / 2), static_cast<int>(drawn), range);
    const glm::vec3 fwd{cosf(yaw * (3.14159265f / 180.f)), sinf(yaw * (3.14159265f / 180.f)), 0.f};
    const float m2u = units::metresToUnits();
    const glm::vec3 at = view + fwd * (0.8f * m2u) - glm::vec3{0.f, 0.f, 0.3f * m2u};
    text3d::queueOverlay(text, at, glm::vec3{0.f, yaw, 0.f}, 0.05f);
}

} // namespace qvr::hull

namespace qvr::hull::detail
{

void registerHoleCommands()
{
    Cmd_AddCommand("vr_hull_holes_census", census_f);
    Cmd_AddCommand("vr_hull_holes_near", near_f);
    Cmd_AddCommand("vr_hull_holes_test", holeTest_f);
    Cmd_AddCommand("vr_hull_holes_mark", mark_f);
    Cmd_AddCommand("vr_hull_holes_rebuild", rebuild_f);
}

} // namespace qvr::hull::detail
