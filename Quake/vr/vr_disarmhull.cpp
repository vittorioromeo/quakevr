// vr_disarmhull.cpp -- an armed humanoid's disarm volumes as hulls of what is drawn: see vr_disarmhull.hpp.

#include "vr_disarmhull.hpp"

#include "vr_agarms.hpp"
#include "vr_anchor.hpp"
#include "vr_drawnpose.hpp"
#include "vr_geom.hpp"
#include "vr_held.hpp"
#include "vr_hitmodel.hpp"
#include "vr_hitreact.hpp"
#include "vr_lines.hpp"
#include "vr_mem.hpp"
#include "vr_progs.hpp"
#include "vr_ragdoll.hpp"
#include "vr_render.hpp"
#include "vr_rig2.hpp"

#include "Zancle/Base/Limits.hpp"
#include "Zancle/Container/Array.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/MinMax.hpp"

#include <box3d/collision.h>
#include <glm/geometric.hpp>
#include <glm/matrix.hpp>

namespace qvr::disarmhull
{
namespace
{

// Box3D's quickhull keeps at most this many corners (its 128 edges: a hull of more may fail); the points it leaves out
// push the faces out instead (Hull::slack).
constexpr za::Array<int, 4> budgets{40, 32, 24, 16};

[[nodiscard]] b3Vec3 b3v(const glm::vec3& v)
{
    return b3Vec3{v.x, v.y, v.z};
}

[[nodiscard]] glm::vec3 gv(const b3Vec3& v)
{
    return glm::vec3{v.x, v.y, v.z};
}

// The points' box as the hull (too few points, or all in a plane).
void boxHull(Hull& out)
{
    const glm::vec3 lo = out.lo, hi = out.hi;
    out.corners.clear();
    for(int i = 0; i < 8; i++)
    {
        out.corners.pushBack(b3v({(i & 1) ? hi.x : lo.x, (i & 2) ? hi.y : lo.y, (i & 4) ? hi.z : lo.z}));
    }
    out.planes.clear();
    for(int axis = 0; axis < 3; axis++)
    {
        glm::vec3 n{0.f};
        n[axis] = 1.f;
        out.planes.pushBack(glm::vec4{n, hi[axis]});
        out.planes.pushBack(glm::vec4{-n, -lo[axis]});
    }
    out.edges.clear();
    for(int i = 0; i < 8; i++)
    {
        for(int bit = 1; bit < 8; bit <<= 1)
        {
            if(!(i & bit))
            {
                out.edges.pushBack(glm::ivec2{i, i | bit});
            }
        }
    }
    out.slack = 0.f;
    out.ok = true;
}

// Whether the path a..b (the hull's space) meets the hull grown by `grow` (each face moved out by it), and where it enters
// (`enter`: its share).
[[nodiscard]] bool clip(const Hull& h, const glm::vec3& a, const glm::vec3& b, float grow, float& enter)
{
    float t0 = 0.f, t1 = 1.f;
    const glm::vec3 d = b - a;
    for(const glm::vec4& p : h.planes)
    {
        const glm::vec3 n{p};
        const float room = p.w + grow - glm::dot(n, a);
        const float den = glm::dot(n, d);
        if(za::fabs(den) < 1e-8f)
        {
            if(room < 0.f)
            {
                return false;
            }
            continue;
        }
        const float t = room / den;
        if(den < 0.f)
        {
            t0 = za::max(t0, t);
        }
        else
        {
            t1 = za::min(t1, t);
        }
        if(t0 > t1)
        {
            return false;
        }
    }
    enter = t0;
    return true;
}

// The distance between the path a..b (the hull's space) and the hull (its corners rounded by its slack: holding every
// point it was made of), and the path's share at its nearest point.
[[nodiscard]] float distance(const Hull& h, const glm::vec3& a, const glm::vec3& b, float& share)
{
    b3DistanceInput in{};
    in.proxyA.points = h.corners.data();
    in.proxyA.count = static_cast<int>(h.corners.size());
    in.proxyA.radius = h.slack;
    const b3Vec3 path[2] = {b3v(a), b3v(b)};
    const float len2 = glm::dot(b - a, b - a);
    in.proxyB.points = path;
    in.proxyB.count = len2 > 1e-8f ? 2 : 1;
    in.proxyB.radius = 0.f;
    in.transform = b3Transform_identity;
    in.useRadii = true;
    b3SimplexCache cache{};
    const b3DistanceOutput o = b3ShapeDistance(&in, &cache, nullptr, 0);
    share = len2 > 1e-8f ? za::clamp(glm::dot(gv(o.pointB) - a, b - a) / len2, 0.f, 1.f) : 0.f;
    return o.distance;
}

// Each corner's way out as its faces move out by 1 (the mitred offset: n.m = 1 for its faces), for drawing it grown.
[[nodiscard]] glm::vec3 cornerOut(const Hull& h, const glm::vec3& c)
{
    glm::mat3 m{0.f};
    glm::vec3 s{0.f};
    for(const glm::vec4& p : h.planes)
    {
        const glm::vec3 n{p};
        if(za::fabs(glm::dot(n, c) - p.w) <= h.slack + 0.05f)
        {
            m += glm::outerProduct(n, n);
            s += n;
        }
    }
    if(za::fabs(glm::determinant(m)) > 1e-4f)
    {
        const glm::vec3 out = glm::inverse(m) * s;
        return glm::length(out) > 3.f ? glm::normalize(out) * 3.f : out;
    }
    return glm::length(s) > 1e-6f ? glm::normalize(s) : glm::vec3{0.f};
}

// The hands' hulls this frame (hand): remade once a frame for each one asked for (their points, the drawn arm's in the
// world, are never the same two frames running while it lives: a cache of the last frame's hit 0 of 1200 in
// armed_mix32, 2026-11-07; make's quickhull works in a scratch arena instead).
struct HandEntry
{
    int num{0}, side{0}, frame{-1};
    bool ok{false};
    bool inDraw{false}; // made while a frame draws it (its model the authored rig's skinned copy: rig2::sourceModel)
    HandSource source{HandSource::None};
    Hull hull[handParts];
};
constexpr int maxHands = 16;
// The hands' hulls Show Disarm Volumes drew last (handShown; the tests' check of what he sees: disarmhull 23 / 24).
struct ShownHand
{
    int num{0}, side{0}, frame{-1};
    bool inDraw{false};
    HandSource source{HandSource::None};
    Hull hull[handParts];
};
constexpr int maxShown = 8;
struct HandHulls
{
    za::Array<HandEntry, maxHands> entries{};
    za::Array<ShownHand, maxShown> shown{};
    int next{0};                             // the one remade next when none is free
    int nextShown{0};                        // ... shown
    za::Vector<glm::vec3> drawn;             // the body's vertices as drawn (scratch)
    za::Vector<glm::vec3> region[handParts]; // ... those on the hand, on the forearm
};
HandHulls hands;
// make's points for Box3D and its quickhull's memory (the main thread's): b3CreateHullWith's two blocks (its working
// memory, then the hull), each a slot grown to the most asked, given out afresh for each call (nothing freed).
constexpr int arenaSlots = 2;
struct MakeScratch
{
    za::Vector<b3Vec3> points;
    za::Vector<char> slot[arenaSlots];
    auto members() { return mem::list(points, slot[0], slot[1]); }
};
mem::Scratch<MakeScratch> makeScratch{"disarm hulls"};
int arenaUsed = 0; // slots given out in this b3CreateHullWith call
// Blocks asked past the slots (b3CreateHullImpl asks for two today; a Box3D update might ask more): each its own,
// dropped at the next call (the hull is valid until then).
za::Vector<za::Vector<char>> arenaOverflow;

void* arenaAlloc(size_t size, int32_t alignment)
{
    const za::SizeT need = static_cast<za::SizeT>(size) + static_cast<za::SizeT>(alignment);
    if(arenaUsed >= arenaSlots)
    {
        arenaOverflow.pushBack(za::Vector<char>{});
    }
    za::Vector<char>& s = arenaUsed < arenaSlots ? makeScratch.slot[arenaUsed++] : arenaOverflow.back();
    if(s.size() < need)
    {
        s.resize(need);
    }
    const uintptr_t at = reinterpret_cast<uintptr_t>(s.data());
    const uintptr_t mask = static_cast<uintptr_t>(alignment) - 1;
    return reinterpret_cast<void*>((at + mask) & ~mask);
}

void arenaFree(void*, size_t) {} // (the slots are given out afresh at the next call)

// b3CreateHull's hull in the arena (valid until the next call; never b3DestroyHull).
[[nodiscard]] b3HullData* arenaHull(const b3Vec3* points, int count, int budget)
{
    arenaUsed = 0;
    arenaOverflow.clear();
    return b3CreateHullWith(points, count, budget, arenaAlloc, arenaFree);
}

// `h` as a Hull{} is, its vectors emptied but keeping their room.
void reset(Hull& h)
{
    h.corners.clear();
    h.planes.clear();
    h.edges.clear();
    h.slack = 0.f;
    h.lo = glm::vec3{0.f};
    h.hi = glm::vec3{0.f};
    h.points = 0;
    h.ok = false;
}

[[nodiscard]] bool local()
{
    return sv.active && cls.state == ca_connected;
}

// The body of armed humanoid `g` as drawn now (world), into hands.drawn; `source` where it came from; its .mdl. Only its
// drawn body: on the authored rig alive (drawnpose::armedMesh), its ragdoll's; neither (not drawn lately, no local
// view): none, no hand volume (its weapon's alone counts). Never its animation frame's (hitmodel::vertexPoint): the
// drawn arms are posed on the weapon, the frame's are not (his note 2026-11-01 02:20: a cyan hull "completely off",
// moving with the animation: ROUND23.md, "The cyan hand hull off").
[[nodiscard]] qmodel_t* drawnBody(edict_t* g, HandSource& source)
{
    source = HandSource::None;
    const int mi = static_cast<int>(g->v.modelindex);
    qmodel_t* model = mi > 0 && mi < MAX_MODELS ? sv.models[mi] : nullptr;
    if(!model || !local())
    {
        return nullptr;
    }
    if(drawnpose::armedMesh(g, hands.drawn, nullptr))
    {
        source = HandSource::Live;
    }
    else if(ragdoll::skinnedVertices(NUM_FOR_EDICT(g), hands.drawn, nullptr, true))
    {
        source = HandSource::Ragdoll;
    }
    return source == HandSource::None ? nullptr : model;
}

// Arm `side`'s part `part` of `model` (agarms::armPart) as hands.drawn has it, into `out`.
void partPoints(const qmodel_t* model, int side, int part, za::Vector<glm::vec3>& out)
{
    out.clear();
    const za::Vector<int>* verts = nullptr;
    if(!agarms::armPart(model, side, part, verts))
    {
        return;
    }
    for(const int v : *verts)
    {
        if(v >= 0 && static_cast<za::SizeT>(v) < hands.drawn.size())
        {
            out.pushBack(hands.drawn[static_cast<za::SizeT>(v)]);
        }
    }
}

// The hand on `side` and its forearm's vertices of armed humanoid `g` as drawn now (world), into hands.region (each
// part's); `source` where they came from. False: neither has any.
[[nodiscard]] bool handPoints(edict_t* g, int side, HandSource& source)
{
    qmodel_t* model = drawnBody(g, source);
    bool any = false;
    for(int k = 0; k < handParts; k++)
    {
        hands.region[k].clear();
        if(model)
        {
            partPoints(model, side, k, hands.region[k]);
            any = any || !hands.region[k].empty();
        }
    }
    if(!any)
    {
        source = HandSource::None;
    }
    return any;
}

[[nodiscard]] glm::vec3 toLocal(const Placed& p, const glm::vec3& w)
{
    return glm::transpose(p.axes) * (w - p.origin);
}

// The most any of `points` (world) lies outside the placed hull (its planes), and the ratio of the hull's corners' box's
// diagonal (world) to the points' box's.
void measure(const Placed& p, const za::Vector<glm::vec3>& points, float& outside, float& ratio)
{
    outside = -ZA_FLOAT_MAX;
    glm::vec3 lo{ZA_FLOAT_MAX}, hi{-ZA_FLOAT_MAX}, clo{ZA_FLOAT_MAX}, chi{-ZA_FLOAT_MAX};
    for(const glm::vec3& w : points)
    {
        const glm::vec3 l = toLocal(p, w);
        for(const glm::vec4& pl : p.hull->planes)
        {
            outside = za::max(outside, glm::dot(glm::vec3{pl}, l) - pl.w);
        }
        lo = glm::min(lo, w);
        hi = glm::max(hi, w);
    }
    for(const b3Vec3& c : p.hull->corners)
    {
        const glm::vec3 w = p.origin + p.axes * gv(c);
        clo = glm::min(clo, w);
        chi = glm::max(chi, w);
    }
    const float d = glm::length(hi - lo);
    ratio = d > 1e-4f ? glm::length(chi - clo) / d : 0.f;
}

// The weapon `p`'s vertices as the renderer draws its client entity (its matrix, its posed vertices), world.
[[nodiscard]] bool renderedVertices(edict_t* p, za::Vector<glm::vec3>& out)
{
    out.clear();
    const int num = NUM_FOR_EDICT(p);
    if(!local() || num <= 0 || num >= cl.num_entities || !cl_entities[num].model ||
        cl_entities[num].model->type != mod_alias)
    {
        return false;
    }
    const entity_t& e = cl_entities[num];
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(e.model));
    float m[16];
    render::entityMatrix(e, false, ENTSCALE_DEFAULT, glm::vec3{0.f}, m);
    for(int v = 0; v < hdr->numverts; v++)
    {
        const glm::vec3 b = anchor::posedVertex(e, v, 0.f);
        out.pushBack({m[0] * b.x + m[4] * b.y + m[8] * b.z + m[12], m[1] * b.x + m[5] * b.y + m[9] * b.z + m[13],
            m[2] * b.x + m[6] * b.y + m[10] * b.z + m[14]});
    }
    return !out.empty();
}

// The placed hull's long axis (world): its two corners farthest apart, the second the one further along its model's
// forward (a weapon's: its tip), and its length.
[[nodiscard]] glm::vec3 longAxis(const Placed& p, float& length)
{
    const Hull& h = *p.hull;
    int ia = 0, ib = 0;
    float best = -1.f;
    for(za::SizeT i = 0; i < h.corners.size(); i++)
    {
        for(za::SizeT j = i + 1; j < h.corners.size(); j++)
        {
            const glm::vec3 d = gv(h.corners[i]) - gv(h.corners[j]);
            if(glm::dot(d, d) > best)
            {
                best = glm::dot(d, d);
                ia = static_cast<int>(i);
                ib = static_cast<int>(j);
            }
        }
    }
    glm::vec3 d = gv(h.corners[static_cast<za::SizeT>(ib)]) - gv(h.corners[static_cast<za::SizeT>(ia)]);
    length = glm::length(d);
    d = length > 1e-4f ? d / length : glm::vec3{1.f, 0.f, 0.f};
    if(d.x < 0.f)
    {
        d = -d;
    }
    return p.axes * d;
}

} // namespace

void make(const glm::vec3* points, int count, Hull& out)
{
    reset(out); // (its vectors keep their room: a hand's hulls are remade every frame)
    out.points = count;
    out.lo = glm::vec3{ZA_FLOAT_MAX};
    out.hi = glm::vec3{-ZA_FLOAT_MAX};
    if(count <= 0)
    {
        return;
    }
    za::Vector<b3Vec3>& pts = makeScratch.points;
    pts.clear();
    pts.reserve(static_cast<za::SizeT>(count));
    for(int i = 0; i < count; i++)
    {
        out.lo = glm::min(out.lo, points[i]);
        out.hi = glm::max(out.hi, points[i]);
        pts.pushBack(b3v(points[i]));
    }
    b3HullData* h = nullptr;
    for(int budget : budgets)
    {
        if(count >= 4 && (h = arenaHull(pts.data(), count, budget)) != nullptr)
        {
            break;
        }
    }
    if(!h || h->faceCount < 4)
    {
        boxHull(out);
        return;
    }
    const b3Vec3* corners = b3GetHullPoints(h);
    for(int i = 0; i < h->vertexCount; i++)
    {
        out.corners.pushBack(corners[i]);
    }
    const b3Plane* planes = b3GetHullPlanes(h);
    for(int f = 0; f < h->faceCount; f++)
    {
        const glm::vec3 n = gv(planes[f].normal);
        float w = planes[f].offset;
        for(int i = 0; i < count; i++)
        {
            w = za::max(w, glm::dot(n, points[i]));
        }
        out.slack = za::max(out.slack, w - planes[f].offset);
        out.planes.pushBack(glm::vec4{n, w});
    }
    const b3HullHalfEdge* edges = b3GetHullEdges(h);
    for(int e = 0; e < h->edgeCount; e++)
    {
        if(e < edges[e].twin)
        {
            out.edges.pushBack(glm::ivec2{edges[e].origin, edges[edges[e].twin].origin});
        }
    }
    out.ok = true; // (h the arena's: nothing to free)
}

float gap(const Placed& p, const glm::vec3& a, const glm::vec3& b, float grow, float* along)
{
    if(!p.hull || !p.hull->ok)
    {
        return ZA_FLOAT_MAX;
    }
    const Hull& h = *p.hull;
    const glm::vec3 la = toLocal(p, a), lb = toLocal(p, b);
    float share = 0.f, enter = 0.f;
    const float d = distance(h, la, lb, share);
    float g;
    if(grow >= 0.f)
    {
        // (grown: the hull rounded by `grow`, exactly; where it enters, its faces moved out by it)
        g = d - grow;
        if(g <= 0.f && clip(h, la, lb, grow, enter))
        {
            share = enter;
        }
    }
    else if(clip(h, la, lb, grow, enter))
    {
        g = 0.f; // (shrunk: its faces moved in; met)
        share = enter;
    }
    else
    {
        g = d - grow;
    }
    if(along)
    {
        *along = share;
    }
    return g;
}

float spheresGap(const Placed& p, const za::Vector<glm::vec4>& spheres, float grow, float stopAt)
{
    float best = ZA_FLOAT_MAX;
    for(const glm::vec4& s : spheres)
    {
        const glm::vec3 c{s};
        best = za::min(best, gap(p, c, c, grow) - s.w);
        if(best <= stopAt)
        {
            break;
        }
    }
    return best;
}

bool weapon(edict_t* p, Placed& out)
{
    out = Placed{};
    if(!p || p->free)
    {
        return false;
    }
    const Hull* h = held::drawnHull(p);
    if(!h || !h->ok)
    {
        return false;
    }
    out.hull = h;
    glm::vec3 at, fw, up;
    if(local() && hitreact::drawnWeapon(NUM_FOR_EDICT(p), at, fw, up))
    {
        // (as drawn: x forward, y left, z up)
        out.axes = glm::mat3{fw, glm::cross(up, fw), up};
        out.origin = at;
    }
    else
    {
        out.axes = held::axesFromAngles(p->v.angles, false);
        out.origin = geom::vec(p->v.origin);
    }
    return true;
}

bool hand(edict_t* g, int side, HandPlaced& out, HandSource* source, bool* inDraw)
{
    out = HandPlaced{};
    if(!g || g->free)
    {
        return false;
    }
    const int num = NUM_FOR_EDICT(g);
    HandEntry* e = nullptr;
    for(HandEntry& h : hands.entries)
    {
        if(h.num == num && h.side == side && h.frame == host_framecount)
        {
            e = &h;
            break;
        }
    }
    if(!e)
    {
        for(HandEntry& h : hands.entries)
        {
            if(h.frame != host_framecount)
            {
                e = &h;
                break;
            }
        }
        if(!e)
        {
            e = &hands.entries[static_cast<za::SizeT>(hands.next)];
            hands.next = (hands.next + 1) % maxHands;
        }
        e->num = num;
        e->side = side;
        e->frame = host_framecount;
        e->inDraw = local() && num < cl.num_entities && rig2::sourceModel(&cl_entities[num]) != nullptr;
        e->ok = handPoints(g, side, e->source);
        bool any = false;
        for(int k = 0; k < handParts; k++)
        {
            reset(e->hull[k]);
            if(e->ok && !hands.region[k].empty())
            {
                make(hands.region[k].data(), static_cast<int>(hands.region[k].size()), e->hull[k]);
                any = any || e->hull[k].ok;
            }
        }
        e->ok = any;
    }
    if(!e->ok)
    {
        return false;
    }
    for(int k = 0; k < handParts; k++)
    {
        out.part[k].hull = e->hull[k].ok ? &e->hull[k] : nullptr;
    }
    if(source)
    {
        *source = e->source;
    }
    if(inDraw)
    {
        *inDraw = e->inDraw;
    }
    return true;
}

bool handShown(edict_t* g, int side, HandPlaced& out)
{
    HandSource source = HandSource::None;
    bool inDraw = false;
    if(!hand(g, side, out, &source, &inDraw))
    {
        return false;
    }
    const int num = NUM_FOR_EDICT(g);
    ShownHand* s = nullptr;
    for(ShownHand& h : hands.shown)
    {
        if(h.num == num && h.side == side)
        {
            s = &h;
            break;
        }
    }
    if(!s)
    {
        s = &hands.shown[static_cast<za::SizeT>(hands.nextShown)];
        hands.nextShown = (hands.nextShown + 1) % maxShown;
    }
    if(s->num == num && s->side == side && s->frame == host_framecount)
    {
        return true;
    }
    s->num = num;
    s->side = side;
    s->frame = host_framecount;
    s->source = source;
    s->inDraw = inDraw;
    for(int k = 0; k < handParts; k++)
    {
        s->hull[k] = out.part[k].hull ? *out.part[k].hull : Hull{};
    }
    return true;
}

float gap(const HandPlaced& h, const glm::vec3& a, const glm::vec3& b, float grow, float* along)
{
    // (both met: the one the path enters first)
    float best = 1e6f, bestAt = 0.f;
    for(int k = 0; k < handParts; k++)
    {
        if(!h.part[k].hull)
        {
            continue;
        }
        float at = 0.f;
        const float g = gap(h.part[k], a, b, grow, &at);
        if((g <= 0.f && best <= 0.f) ? at < bestAt : g < best)
        {
            best = g;
            bestAt = at;
        }
    }
    if(along)
    {
        *along = bestAt;
    }
    return best;
}

void draw(const HandPlaced& h, float grow, const glm::vec4& colour, float width)
{
    for(const Placed& p : h.part)
    {
        draw(p, grow, colour, width);
    }
}

void draw(const Placed& p, float grow, const glm::vec4& colour, float width)
{
    if(!p.hull || !p.hull->ok)
    {
        return;
    }
    const Hull& h = *p.hull;
    za::Array<glm::vec3, 160> at;
    const za::SizeT n = za::min<za::SizeT>(h.corners.size(), at.size());
    for(za::SizeT i = 0; i < n; i++)
    {
        const glm::vec3 c = gv(h.corners[i]);
        at[i] = p.origin + p.axes * (grow != 0.f ? c + cornerOut(h, c) * grow : c);
    }
    for(const glm::ivec2& e : h.edges)
    {
        if(static_cast<za::SizeT>(e.x) < n && static_cast<za::SizeT>(e.y) < n)
        {
            lines::line(at[static_cast<za::SizeT>(e.x)], at[static_cast<za::SizeT>(e.y)], width, colour, colour);
        }
    }
}

namespace
{

// The hand's queries of disarmhull (what 1 / 2, 11 / 12, 15 / 16, 17 / 18, 21 / 22, 23 / 24); false: not one of them.
[[nodiscard]] bool handQuery(edict_t* e, int what, const glm::vec3& a, const glm::vec3& b, float grow)
{
    const int side = (what == 1 || what == 11 || what == 15 || what == 17 || what == 21 || what == 23) ? 0
                     : (what == 2 || what == 12 || what == 16 || what == 18 || what == 22 || what == 24) ? 1
                                                                                                          : -1;
    if(side < 0)
    {
        return false;
    }
    if(what == 23 || what == 24)
    {
        const int num = e && !e->free ? NUM_FOR_EDICT(e) : -1;
        for(const ShownHand& s : hands.shown)
        {
            if(s.num == num && s.side == side && s.frame >= 0)
            {
                HandPlaced sp;
                for(int k = 0; k < handParts; k++)
                {
                    sp.part[k].hull = s.hull[k].ok ? &s.hull[k] : nullptr;
                }
                if(!sp.part[0].hull && !sp.part[1].hull)
                {
                    break;
                }
                progs::returnVec({gap(sp, a, b, grow, nullptr),
                    static_cast<float>(static_cast<int>(s.source) + (s.inDraw ? 10 : 0)),
                    static_cast<float>(host_framecount - s.frame)});
                return true;
            }
        }
        progs::returnVec({1e6f, 0.f, -1.f});
        return true;
    }
    HandPlaced hp;
    HandSource source = HandSource::None;
    if(!hand(e, side, hp, &source, nullptr))
    {
        progs::returnVec(what < 10 ? glm::vec3{1e6f, 0.f, 0.f} : glm::vec3{0.f, 0.f, -1.f});
        return true;
    }
    if(what <= 2)
    {
        float along = 0.f;
        const float g = gap(hp, a, b, grow, &along);
        progs::returnVec({g, along, 1.f});
        return true;
    }
    if(what == 15 || what == 16)
    {
        int points = 0;
        for(const Placed& pp : hp.part)
        {
            points += pp.hull ? pp.hull->points : 0;
        }
        progs::returnVec({static_cast<float>(source), static_cast<float>(points), 0.f});
        return true;
    }
    if(what == 21 || what == 22)
    {
        // (the hand's middle: its first hull's corners' mean, world; the forearm's when it has no hand)
        const Hull& h = hp.part[0].hull ? *hp.part[0].hull : *hp.part[1].hull;
        glm::vec3 mid{0.f};
        for(const b3Vec3& c : h.corners)
        {
            mid += gv(c);
        }
        progs::returnVec(mid / static_cast<float>(za::max<za::SizeT>(h.corners.size(), 1)));
        return true;
    }
    // (a test's query: made each call)
    qmodel_t* model = drawnBody(e, source);
    if(!model)
    {
        progs::returnVec({0.f, 0.f, -1.f});
        return true;
    }
    if(what == 11 || what == 12)
    {
        // '<the most any part's vertex lies outside its own hull> <the hand's vertices> <the forearm's>'
        float most = -ZA_FLOAT_MAX;
        float count[handParts]{0.f, 0.f};
        za::Vector<glm::vec3> points;
        for(int k = 0; k < handParts; k++)
        {
            partPoints(model, side, k, points);
            count[k] = static_cast<float>(points.size());
            if(hp.part[k].hull && !points.empty())
            {
                float outside = 0.f, ratio = 0.f;
                measure(hp.part[k], points, outside, ratio);
                most = za::max(most, outside);
            }
        }
        progs::returnVec({most, count[0], count[1]});
        return true;
    }
    // 17 / 18: '<its upper arm's top vertices (agarms::armPart 2) in either hull> <of the ragdoll rig's hand bone and its
    // parent's vertices (the old hull's region), those on neither the hand nor the forearm> <the old region's count>'
    za::Vector<glm::vec3> upper;
    partPoints(model, side, 2, upper);
    int inside = 0;
    for(const glm::vec3& u : upper)
    {
        inside += gap(hp, u, u, 0.f, nullptr) <= 0.f ? 1 : 0;
    }
    const ragdoll::Rig* rig = ragdoll::rigFor(model);
    rigpose::Rigid grip;
    int handBone = -1;
    int old = 0, oldOff = 0;
    if(rig && agarms::ragdollGrip(model, side, grip, handBone) && handBone >= 0 && handBone < rig->numBones)
    {
        const int arm = rig->bones[handBone].parent;
        za::Vector<char> on(rig->vertBone.size(), 0);
        for(int k = 0; k < handParts; k++)
        {
            const za::Vector<int>* verts = nullptr;
            if(agarms::armPart(model, side, k, verts))
            {
                for(const int v : *verts)
                {
                    if(v >= 0 && static_cast<za::SizeT>(v) < on.size())
                    {
                        on[static_cast<za::SizeT>(v)] = 1;
                    }
                }
            }
        }
        for(za::SizeT v = 0; v < rig->vertBone.size(); v++)
        {
            if(rig->vertBone[v] == handBone || rig->vertBone[v] == arm)
            {
                old++;
                oldOff += on[v] ? 0 : 1;
            }
        }
    }
    progs::returnVec({static_cast<float>(inside), static_cast<float>(oldOff), static_cast<float>(old)});
    return true;
}

} // namespace

// vector disarmhull(entity e, float what, vector a, vector b, float grow):
//   what 0: held weapon `e`'s volume (its drawn model's hull, where drawn); 1 / 2: armed humanoid `e`'s hand and forearm
//     (two hulls: the nearer counts) on side 0 (its main weapon's) / 1 (its left one's): '<gap> <share> <1>' (gap: the
//     path a..b to it grown by `grow`, 0 or less met; share: along the path where it is first met, or its nearest
//     point); '<huge> 0 0' none.
//   what 10: weapon `e`'s hull against its vertices as the renderer draws its client entity: '<the most one lies outside
//     it (units)> <the hull's box's diagonal over theirs> <its slack>'; '0 0 -1' not drawn. 11 / 12: the hand on side 0 /
//     1 against the vertices they were made of: '<most outside> <the hand's vertices> <the forearm's>'.
//   what 13: weapon `e`'s volume's long axis (world, unit; its tip's way); 14: '<its length> <its corners' most distance
//     off the line through its middle along that axis> 0'. The tests (vr_disarmhull_test.qc).
//   what 15 / 16: the hand on side 0 / 1 now: '<its source: 1 the authored rig as drawn, 2 its ragdoll> <its points> 0';
//     '0 0 -1' none (not drawn lately: no hand volume).
//   what 17 / 18: the hand on side 0 / 1: '<its upper arm's top vertices inside its hulls> <of the old single hull's
//     region (the ragdoll rig's hand bone and its parent), the vertices on neither the hand nor the forearm> <that
//     region's count>' (his note 2026-11-01 18:02: the cyan hull took the biceps).
//   what 20 / 21 / 22: volume 0 / 1 / 2's middle (its corners' mean, world; the tests aim at it; a hand's: its hand's).
//   what 23 / 24: the hand hulls on side 0 / 1 Show Disarm Volumes drew last (handShown: what he sees, made while the
//     frame drew it): '<the gap of a..b to them> <its source, plus 10 if made while its frame drew it> <frames since>';
//     '<huge> 0 -1' none drawn.
void PF_disarmhull()
{
    edict_t* e = G_EDICT(OFS_PARM0);
    const int what = static_cast<int>(G_FLOAT(OFS_PARM1));
    const glm::vec3 a = geom::vec(G_VECTOR(OFS_PARM2));
    const glm::vec3 b = geom::vec(G_VECTOR(OFS_PARM3));
    const float grow = G_FLOAT(OFS_PARM4);
    if(handQuery(e, what, a, b, grow))
    {
        return;
    }
    Placed p;
    const bool found = (what == 0 || what == 10 || what == 13 || what == 14 || what == 20) && weapon(e, p);
    if(!found)
    {
        progs::returnVec(what < 10 ? glm::vec3{1e6f, 0.f, 0.f} : glm::vec3{0.f, 0.f, -1.f});
        return;
    }
    if(what < 10)
    {
        float along = 0.f;
        const float g = gap(p, a, b, grow, &along);
        progs::returnVec({g, along, 1.f});
        return;
    }
    if(what >= 20)
    {
        // (the volume's middle: its corners' mean, world)
        glm::vec3 mid{0.f};
        for(const b3Vec3& c : p.hull->corners)
        {
            mid += gv(c);
        }
        mid /= static_cast<float>(za::max<za::SizeT>(p.hull->corners.size(), 1));
        progs::returnVec(p.origin + p.axes * mid);
        return;
    }
    if(what == 13 || what == 14)
    {
        float length = 0.f;
        const glm::vec3 ax = longAxis(p, length);
        if(what == 13)
        {
            progs::returnVec(ax);
            return;
        }
        const glm::vec3 la = glm::transpose(p.axes) * ax;
        glm::vec3 mid{0.f};
        for(const b3Vec3& c : p.hull->corners)
        {
            mid += gv(c);
        }
        mid /= static_cast<float>(za::max<za::SizeT>(p.hull->corners.size(), 1));
        float off = 0.f;
        for(const b3Vec3& c : p.hull->corners)
        {
            const glm::vec3 r = gv(c) - mid;
            off = za::max(off, glm::length(r - la * glm::dot(r, la)));
        }
        progs::returnVec({length, off, 0.f});
        return;
    }
    za::Vector<glm::vec3> points; // (a test's query: made each call)
    if(!renderedVertices(e, points))
    {
        progs::returnVec({0.f, 0.f, -1.f});
        return;
    }
    float outside = 0.f, ratio = 0.f;
    measure(p, points, outside, ratio);
    progs::returnVec({outside, ratio, p.hull->slack});
}

} // namespace qvr::disarmhull
