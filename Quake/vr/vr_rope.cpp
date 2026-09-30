// vr_rope.cpp -- see vr_rope.hpp.

#include "vr_rope.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_gfx.hpp"
#include "vr_lines.hpp"
#include "vr_mem.hpp"
#include "vr_profile.hpp"
#include "vr_protocol.hpp"
#include "vr_trace.hpp"

#include "Zancle/Algorithm/Find.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Acos.hpp"
#include "Zancle/Math/Ceil.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/Vocabulary/UniquePtr.hpp"
#include "vr_zancle.hpp"


extern "C" float VR_BeamScale(qmodel_t* model); // vr_client.cpp
extern "C" void VR_AliasLightCurve(float lightcolor[3]);
extern "C" float VR_ViewModelMinLight(void);

using namespace qvr;

namespace
{

// How finely a hanging rope's curve is sampled: the chord between two samples no farther than sagittaTolerance from
// the curve, and the rope turning by maxBend at most from one to the next (a short rope hanging deep bends sharply at
// the bottom: many samples there), no closer than minStep (the links' own rings are about a unit apart), and from
// minSamples to maxSamples pieces in all.
constexpr float sagittaTolerance = 0.05f; // units
constexpr float maxBend = 0.052f;         // radians (3 degrees)
constexpr float minStep = 0.5f;           // units
constexpr int minPieces = 8;
constexpr int maxPieces = 160;
constexpr int maxTraces = 40; // floor traces a rope at most, but where it leaves the floor (ropePoints)

// Rogue's links: one every 30 model units (times the beam's scale), as the engine lays a beam's.
constexpr float linkPeriod = 30.f;

// A rope's slack (beam key -> the share of the rope's length that hangs), as sent and as drawn: eased towards what the
// server sends (it comes in 125 steps) so that a sagging rope never jumps.
struct RopeSlack
{
    float target{0.f};
    float shown{0.f};
    double time{0.0};
    double logAt{0.0}; // vr_grapple_debug 2: the next line about its curve
};
ankerl::unordered_dense::map<int, RopeSlack> slacks;

// A rope's corners as last sent (beam key -> the corners the server's rope wraps round, vr_ropesim.cpp, and when): the
// drawn chain is pinned at them. Not sent for a while: none (the server sends them when they change, and every quarter
// of a second while there are any).
struct RopeCorners
{
    za::Vector<glm::vec3> corners;
    double time{0.0}; // cl.time
};
ankerl::unordered_dense::map<int, RopeCorners> cornerSets;
constexpr double cornersFresh = 1.0; // seconds

// A rope's slack as drawn (vr_grapple_rope_sim): a chain of points, Rope Point Spacing apart, pinned at the gun, the
// corners and the hook, hanging between them (gravity, a little drag) and lying on the world: each point is traced
// from where it was to where it goes (stopped on what it meets, sliding along it), and each piece is traced too (one
// that goes through the world is pushed out of it). Beam key -> chain.
struct Chain
{
    za::Vector<glm::vec3> p;    // the points (the pins among them)
    za::Vector<glm::vec3> prev; // last frame's (Verlet)
    za::Vector<int> pins;       // each path point's (the gun, the corners, the hook) index in p
    za::Vector<glm::vec3> drawn; // the points drawn: p, a piece still through the world taken round what it meets
    float lastDt{0.f};
    float fastest{0.f};          // the farthest a free point moved this step (units: vr_grapple_rope_draw_dump), which
    int fastestAt{-1};
    bool tail{false};            // its last pin is the hook's tail (hookTail), not a corner
    float endAngle{-1.f};        // degrees between the drawn rope's last few units and the hook's length (-1: no hook)
    double time{-1.0};           // realtime of its last step
};
ankerl::unordered_dense::map<int, Chain> chains;
constexpr int chainMaxPoints = 128;
constexpr float chainDrag = 1.5f;      // of a point's speed lost to the air, a second
constexpr float chainSlide = 0.6f;     // of its speed along a surface a point keeps where it touches it
constexpr float chainGravity = 800.f;  // units/s/s

// vr_debug_rope: this frame's drawn ropes (their points, pins and path), drawn in the world by debugDraw.
struct DebugRope
{
    za::Vector<glm::vec3> points;
    za::Vector<glm::vec3> path;
};
za::Vector<DebugRope> debugRopes;
int debugFrame = -1;

// A model's mesh as the rope draws it (its first pose's triangles, BentVertex each), and its skin.
struct Mesh
{
    const aliashdr_t* hdr{nullptr};
    za::Vector<gfx::BentVertex> vertices;
    float xMax{0.f}; // a link's far end (model units along the rope)
    gfx::Texture skin{0};
    gfx::Texture fullbright{0};
};

[[nodiscard]] za::SizeT heldBytes(const Mesh& m) // (vr_mem.hpp)
{
    return mem::heldBytes(m.vertices);
}

// The ropes' models' meshes, by model (checked against its data on use; the models' slots are other models' after a
// game change, their data made again by a model reload).
struct RopeMeshes
{
    ankerl::unordered_dense::map<const qmodel_t*, za::UniquePtr<Mesh>> meshes; // (pointers into it are kept)
    auto members() { return qvr::mem::list(meshes); }
};
mem::Cache<RopeMeshes> cache{"rope meshes", mem::GameDirChange | mem::ModelReload};

// The rope's curve worked out, and the frame's upload (the main thread).
struct RopeScratch
{
    za::Vector<glm::vec3> onLine;         // the points on the line (ropePoints)
    za::Vector<char> lies;                // whether each lies on the floor (traced points)
    za::Vector<glm::vec3> samples;        // the points kept as the curve's samples (addSamples)
    za::Vector<glm::vec3> points;         // a rope's points (VR_DrawRope)
    za::Vector<glm::vec4> data;           // the frame's upload: each mesh once, then the curves (drawOpaque)
    za::Vector<const Mesh*> packed;       // each mesh once
    za::Vector<int> packedFirst;          // and its first vec4
    za::Vector<glm::vec3> path;           // a rope's path: the gun, the corners, the hook (VR_DrawRope)
    za::Vector<glm::vec3> from;           // the chain's points before this step (stepChain)
    za::Vector<glm::vec3> resampled;      // (resample)
    za::Vector<float> lengths;            // each piece between two pins: its length and its points
    za::Vector<int> counts;
    za::Vector<char> pinned;              // each chain point: a pin
    auto members() { return qvr::mem::list(onLine, lies, samples, points, data, packed, packedFirst, path, from, resampled, lengths, counts, pinned); }
};
mem::Scratch<RopeScratch> scratch{"rope"};

// The frame's ropes as uploaded (drawOpaque's first view), drawn from it in every view.
struct RopeDraw
{
    gfx::BentBatch batch;
    za::Vector<int> meshFirst; // queued[i]'s mesh's first vec4
    int curveBase{0};           // the curves' first vec4
    int builtFrame{-1};
};
RopeDraw ropeDraw;

// This frame's ropes (CL_UpdateTEnts puts them here; every view draws them).
struct Queued
{
    const Mesh* mesh;
    int firstSample;
    int samples;
    int copies;
    float period; // model units from one link to the next (linkPeriod, less so that the last ends at the hook)
    float scale;
    glm::vec3 light;
};
za::Vector<Queued> queued;
za::Vector<gfx::CurveSample> curve;
int queuedFrame = -1;

// The mesh of `model` (a Quake MDL: an MD3 or IQM replacement is not bent; the beam is drawn as before), made again
// when the model is loaded again.
const Mesh* meshOf(qmodel_t* model)
{
    if(!model || model->type != mod_alias)
    {
        return nullptr;
    }
    const aliashdr_t* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(model));
    if(!hdr || hdr->poseverttype != aliashdr_t::PV_QUAKE1 || hdr->numposes < 1 || hdr->numindexes < 3 ||
        !hdr->gltextures[0][0])
    {
        return nullptr;
    }
    Mesh& m = qza::stableAt<Mesh>(cache.meshes, model);
    if(m.hdr == hdr)
    {
        return &m;
    }
    m.hdr = hdr;
    m.skin = hdr->gltextures[0][0]->texnum;
    m.fullbright = hdr->fbtextures[0][0] ? hdr->fbtextures[0][0]->texnum : 0;
    const auto* base = reinterpret_cast<const byte*>(hdr);
    const auto* desc = reinterpret_cast<const aliasmesh_t*>(base + hdr->meshdesc);
    const auto* indexes = reinterpret_cast<const unsigned short*>(base + hdr->indexes);
    const auto* pose = reinterpret_cast<const trivertx_t*>(base + hdr->vertexes) + hdr->numverts * hdr->frames[0].firstpose;
    const float hs = 1.f / static_cast<float>(TexMgr_PadConditional(hdr->skinwidth));
    const float vs = 1.f / static_cast<float>(TexMgr_PadConditional(hdr->skinheight));
    m.vertices.clear();
    m.vertices.reserve(hdr->numindexes);
    m.xMax = -1e9f;
    for(int i = 0; i < hdr->numindexes; i++)
    {
        const aliasmesh_t& d = desc[indexes[i]];
        const trivertx_t& t = pose[d.vertindex];
        gfx::BentVertex v;
        v.pos = {hdr->scale[0] * t.v[0] + hdr->scale_origin[0], hdr->scale[1] * t.v[1] + hdr->scale_origin[1],
            hdr->scale[2] * t.v[2] + hdr->scale_origin[2], 0.f};
        v.uv = {hs * (static_cast<float>(d.st[0]) + 0.5f), vs * (static_cast<float>(d.st[1]) + 0.5f), 0.f, 0.f};
        m.vertices.pushBack(v);
        m.xMax = za::max(m.xMax, v.pos.x);
    }
    return &m;
}

// The light at a point as the alias models get it (vr_coil.cpp's; 1 is Quake's full light): for a skin's lit texels
// (Rogue's chain is all fullbright).
glm::vec3 worldLight(const glm::vec3& p)
{
    if(!cl.worldmodel)
    {
        return glm::vec3{0.5f};
    }
    vec3_t v{p.x, p.y, p.z};
    lightcache_t cache{};
    R_LightPoint(v, 0.f, &cache);
    float c[3] = {lightcolor[0], lightcolor[1], lightcolor[2]};
    VR_AliasLightCurve(c);
    const float add = 3.f * VR_ViewModelMinLight() - (c[0] + c[1] + c[2]);
    glm::vec3 out{c[0], c[1], c[2]};
    if(add > 0.f)
    {
        out += glm::vec3{add / 3.f};
    }
    return out / 128.f;
}

// The slack shown eased towards the one sent (it comes in 125 steps): a sagging rope never jumps.
void easeSlack(RopeSlack& r)
{
    const float dt = static_cast<float>(za::clamp(realtime - r.time, 0.0, 0.1));
    r.time = realtime;
    r.shown += (r.target - r.shown) * za::min(1.f, 10.f * dt);
}

// The rope from `a` to `b` as points into `out` (at least 2): straight (2) when taut, hanging when slack: the parabola
// of its length over the line between its ends, sagging down across the line (a rope hanging straight down stays
// straight), lying on the floor where it would go through it. `h`: how deep it hangs (0 straight).
void ropePoints(const RopeSlack& r, const glm::vec3& a, const glm::vec3& b, za::Vector<glm::vec3>& out, float& length, float& h)
{
    out.clear();
    out.pushBack(a);
    const float chord = glm::distance(a, b);
    length = chord;
    h = 0.f;
    if(!vr_grapple_sag.value || r.shown < 0.002f || chord < 1.f)
    {
        out.pushBack(b);
        return;
    }

    // A parabola of height h over a chord D is about D + 8 h^2 / (3 D) long; no deeper than half the rope (a V).
    length = chord / za::max(0.02f, 1.f - r.shown);
    h = za::min(0.5f * length, za::sqrt(3.f * chord * (length - chord) / 8.f));
    const glm::vec3 along = (b - a) / chord;
    const glm::vec3 down = glm::vec3{0.f, 0.f, -1.f} - along * -along.z; // gravity across the line
    const float depth = h * glm::length(down);
    if(depth < 0.5f)
    {
        out.pushBack(b);
        h = 0.f;
        return;
    }

    // As many pieces as its bend needs: its curvature is greatest at the bottom, 8 h / D^2 (the sag across the line),
    // over the curve's length (summed over 16 chords: the length formula above is only close for a shallow sag).
    const float kappa = 8.f * depth / (chord * chord);
    const float step = za::max(minStep, za::min(za::sqrt(8.f * sagittaTolerance / kappa), maxBend / kappa));
    float arc = 0.f;
    for(int i = 0; i < 16; i++)
    {
        const float t0 = static_cast<float>(i) / 16.f, t1 = static_cast<float>(i + 1) / 16.f;
        arc += glm::length((b - a) * (t1 - t0) + down * (4.f * h * (t1 * (1.f - t1) - t0 * (1.f - t0))));
    }
    const int n = za::clamp(static_cast<int>(za::ceil(arc / step)), minPieces, maxPieces);
    // The floor: each point traced from the line down to it (the old 17 points' way), lying on what it meets. A long
    // rope's points are traced maxTraces at most, evenly (every k-th); between two traced ones that both lie on the
    // floor, the points lie on the line between them (a flat floor: on it); where one lies and the other hangs (where the
    // rope leaves the floor), every point between is traced.
    za::Vector<glm::vec3>& onLine = scratch.onLine;
    za::Vector<char>& lies = scratch.lies; // on the floor (traced points)
    onLine.clear();
    onLine.resize(n + 1, a);
    lies.clear();
    lies.resize(n + 1, 0);
    for(int i = 1; i < n; i++)
    {
        const float t = static_cast<float>(i) / static_cast<float>(n);
        onLine[i] = a + (b - a) * t;
        out.pushBack(onLine[i] + down * (4.f * h * t * (1.f - t)));
    }
    out.pushBack(b);
    const auto trace = [&](int i) {
        if(glm::distance(onLine[i], out[i]) > 0.25f)
        {
            const trace_t tr = worldtrace::world(onLine[i], out[i], false);
            if(tr.fraction < 1.f && !tr.startsolid)
            {
                out[i] = worldtrace::endPos(tr) + glm::vec3{0.f, 0.f, 1.f};
                lies[i] = 1;
            }
        }
    };
    const int k = (n - 1 + maxTraces - 1) / maxTraces; // (1 up to maxTraces interior points)
    int last = 0; // the last traced point (0: the rope's start, not on the floor)
    for(int i = 1; i <= n; i++)
    {
        if(i < n && i % k != 0 && i != n - 1)
        {
            continue;
        }
        if(i < n)
        {
            trace(i);
        }
        if(i - last > 1)
        {
            if(lies[last] && lies[i])
            {
                for(int j = last + 1; j < i; j++)
                {
                    out[j] = glm::mix(out[last], out[i], static_cast<float>(j - last) / static_cast<float>(i - last));
                }
            }
            else if(lies[last] || lies[i])
            {
                for(int j = last + 1; j < i; j++)
                {
                    trace(j);
                }
            }
        }
        last = i;
    }
}

// The simulated rope's points (`sim`) as a smooth curve from `a` to `b` (the beam's ends: the gun as drawn, the hook) into
// `out`: its first and last points moved there (the next ones eased along), a Catmull-Rom curve through them, each piece
// cut in pieces of about 2 units (6 at most).
void simPoints(const za::Vector<glm::vec3>& sim, const glm::vec3& a, const glm::vec3& b, za::Vector<glm::vec3>& out,
    float& length)
{
    za::Vector<glm::vec3>& p = scratch.onLine;
    p = sim;
    const int n = static_cast<int>(p.size());
    // The ends as drawn: the gun where the hand is now, not where the server last had it; its difference eased out over
    // the first pieces (and the hook's over the last ones).
    const glm::vec3 da = a - p.front();
    const glm::vec3 db = b - p.back();
    const int ease = za::min(4, n / 2);
    for(int i = 0; i < ease; i++)
    {
        const float w = 1.f - static_cast<float>(i) / static_cast<float>(ease);
        p[static_cast<za::SizeT>(i)] += da * w;
        p[static_cast<za::SizeT>(n - 1 - i)] += db * w;
    }
    out.clear();
    length = 0.f;
    for(int i = 0; i + 1 < n; i++)
    {
        const glm::vec3& p0 = p[static_cast<za::SizeT>(za::max(0, i - 1))];
        const glm::vec3& p1 = p[static_cast<za::SizeT>(i)];
        const glm::vec3& p2 = p[static_cast<za::SizeT>(i + 1)];
        const glm::vec3& p3 = p[static_cast<za::SizeT>(za::min(n - 1, i + 2))];
        const float len = glm::distance(p1, p2);
        length += len;
        const int k = za::clamp(static_cast<int>(za::ceil(len / 2.f)), 1, 6);
        for(int j = 0; j < k; j++)
        {
            const float t = static_cast<float>(j) / static_cast<float>(k);
            const float t2 = t * t, t3 = t2 * t;
            out.pushBack(0.5f * (2.f * p1 + (p2 - p0) * t + (2.f * p0 - 5.f * p1 + 4.f * p2 - p3) * t2 +
                                     (3.f * p1 - p0 - 3.f * p2 + p3) * t3));
        }
    }
    out.pushBack(p.back());
}

// The polyline `pts[0 .. count - 1]` as `n` + 1 points evenly along it (its ends kept), appended to `out`.
void resample(const glm::vec3* pts, int count, int n, za::Vector<glm::vec3>& out)
{
    float total = 0.f;
    for(int i = 1; i < count; i++)
    {
        total += glm::distance(pts[i - 1], pts[i]);
    }
    out.pushBack(pts[0]);
    int seg = 1;
    float segStart = 0.f;
    for(int k = 1; k < n; k++)
    {
        const float want = total * static_cast<float>(k) / static_cast<float>(n);
        while(seg + 1 < count && segStart + glm::distance(pts[seg - 1], pts[seg]) < want)
        {
            segStart += glm::distance(pts[seg - 1], pts[seg]);
            seg++;
        }
        const float len = count > 1 ? glm::distance(pts[seg - 1], pts[seg]) : 0.f;
        const float t = len > 1e-4f ? za::clamp((want - segStart) / len, 0.f, 1.f) : 0.f;
        out.pushBack(count > 1 ? glm::mix(pts[seg - 1], pts[seg], t) : pts[0]);
    }
    out.pushBack(pts[count - 1]);
}

[[nodiscard]] bool pointInWorld(const glm::vec3& q)
{
    if(!cl.worldmodel)
    {
        return false;
    }
    vec3_t v{q.x, q.y, q.z};
    return Mod_PointInLeaf(v, cl.worldmodel)->contents == CONTENTS_SOLID;
}

// The chain `ch` stepped for this frame along `path` (the gun, the corners, the hook), `slack` of its length hanging (0 ..
// 1): the pieces between two pins share the slack by their lengths. Its last piece straight when `ch.tail` (out of the
// hook's back: hookTail).
void stepChain(Chain& ch, const za::Vector<glm::vec3>& path, float slack)
{
    const int pieces = static_cast<int>(path.size()) - 1;
    const int straightPiece = ch.tail ? pieces - 1 : -1;
    za::Vector<float>& lengths = scratch.lengths;
    za::Vector<int>& counts = scratch.counts;
    lengths.clear();
    lengths.resize(static_cast<za::SizeT>(pieces), 0.f);
    counts.clear();
    counts.resize(static_cast<za::SizeT>(pieces), 1);
    float straight = 0.f;
    for(int i = 0; i < pieces; i++)
    {
        straight += i == straightPiece ? 0.f : glm::distance(path[static_cast<za::SizeT>(i)], path[static_cast<za::SizeT>(i) + 1]);
    }
    const float total = straight / za::max(0.02f, 1.f - za::clamp(slack, 0.f, 1.f));
    const float spacing =
        qza::maxOf(2.f, vr_grapple_rope_spacing.value, total / static_cast<float>(chainMaxPoints - pieces - 1));
    for(int i = 0; i < pieces; i++)
    {
        const float d = glm::distance(path[static_cast<za::SizeT>(i)], path[static_cast<za::SizeT>(i) + 1]);
        if(i == straightPiece)
        {
            lengths[static_cast<za::SizeT>(i)] = d;
            counts[static_cast<za::SizeT>(i)] = 1;
            continue;
        }
        const float share = straight > 1e-3f ? d / straight : 1.f / static_cast<float>(pieces - (straightPiece >= 0 ? 1 : 0));
        lengths[static_cast<za::SizeT>(i)] = d + (total - straight) * share;
        counts[static_cast<za::SizeT>(i)] =
            za::max(1, static_cast<int>(za::ceil(lengths[static_cast<za::SizeT>(i)] / spacing)));
    }

    const double now = realtime;
    bool rebuild = ch.p.empty() || ch.pins.size() != path.size() || ch.time < 0.0 || now - ch.time > 0.5 || now < ch.time;
    for(za::SizeT k = 0; k < ch.pins.size() && !rebuild; k++)
    {
        rebuild = glm::distance(ch.p[static_cast<za::SizeT>(ch.pins[k])], path[k]) > 128.f; // (jumped: a new place)
    }
    const float dt = rebuild ? 1.f / 90.f : static_cast<float>(za::clamp(now - ch.time, 1.0 / 500.0, 1.0 / 20.0));
    if(rebuild)
    {
        ch.p.clear();
        ch.pins.clear();
        for(int i = 0; i < pieces; i++)
        {
            ch.pins.pushBack(static_cast<int>(ch.p.size()));
            for(int j = 0; j < counts[static_cast<za::SizeT>(i)]; j++)
            {
                ch.p.pushBack(glm::mix(path[static_cast<za::SizeT>(i)], path[static_cast<za::SizeT>(i) + 1],
                    static_cast<float>(j) / static_cast<float>(counts[static_cast<za::SizeT>(i)])));
            }
        }
        ch.pins.pushBack(static_cast<int>(ch.p.size()));
        ch.p.pushBack(path.back());
        ch.prev = ch.p;
        ch.lastDt = dt;
    }
    else
    {
        // A piece's points as many as its length needs now (reeled in, paid out): the chain there resampled, its shape
        // kept.
        bool same = true;
        for(int i = 0; i < pieces && same; i++)
        {
            same = ch.pins[static_cast<za::SizeT>(i) + 1] - ch.pins[static_cast<za::SizeT>(i)] ==
                   counts[static_cast<za::SizeT>(i)];
        }
        if(!same)
        {
            for(za::Vector<glm::vec3>* v : {&ch.p, &ch.prev})
            {
                za::Vector<glm::vec3>& out = scratch.resampled;
                out.clear();
                for(int i = 0; i < pieces; i++)
                {
                    const int a = ch.pins[static_cast<za::SizeT>(i)], b = ch.pins[static_cast<za::SizeT>(i) + 1];
                    resample(v->data() + a, b - a + 1, counts[static_cast<za::SizeT>(i)], out);
                    out.popBack(); // (the next piece's first)
                }
                out.pushBack(v->back());
                *v = out;
            }
            int at = 0;
            for(int i = 0; i < pieces; i++)
            {
                ch.pins[static_cast<za::SizeT>(i)] = at;
                at += counts[static_cast<za::SizeT>(i)];
            }
            ch.pins.back() = at;
        }
    }
    za::Vector<char>& pinned = scratch.pinned;
    pinned.clear();
    pinned.resize(ch.p.size(), 0);
    for(za::SizeT k = 0; k < ch.pins.size(); k++)
    {
        const za::SizeT i = static_cast<za::SizeT>(ch.pins[k]);
        pinned[i] = 1;
        ch.p[i] = ch.prev[i] = path[k];
    }

    // Verlet: the free points fall, a little slowed by the air.
    za::Vector<glm::vec3>& from = scratch.from;
    from = ch.p;
    const float keep = za::exp(-chainDrag * dt) * (ch.lastDt > 0.f ? dt / ch.lastDt : 1.f);
    const glm::vec3 fall{0.f, 0.f, -chainGravity * dt * dt};
    for(za::SizeT i = 0; i < ch.p.size(); i++)
    {
        if(!pinned[i])
        {
            const glm::vec3 v = (ch.p[i] - ch.prev[i]) * keep;
            ch.prev[i] = ch.p[i];
            ch.p[i] += v + fall;
        }
    }

    // The pieces' lengths (the pins fixed).
    const int iterations = za::clamp(static_cast<int>(vr_grapple_rope_iterations.value), 1, 64);
    for(int it = 0; it < iterations; it++)
    {
        for(int i = 0; i < pieces; i++)
        {
            const float rest = lengths[static_cast<za::SizeT>(i)] / static_cast<float>(counts[static_cast<za::SizeT>(i)]);
            for(int j = ch.pins[static_cast<za::SizeT>(i)]; j < ch.pins[static_cast<za::SizeT>(i) + 1]; j++)
            {
                glm::vec3& a = ch.p[static_cast<za::SizeT>(j)];
                glm::vec3& b = ch.p[static_cast<za::SizeT>(j) + 1];
                const float wa = pinned[static_cast<za::SizeT>(j)] ? 0.f : 1.f;
                const float wb = pinned[static_cast<za::SizeT>(j) + 1] ? 0.f : 1.f;
                const glm::vec3 d = b - a;
                const float len = glm::length(d);
                if(len <= rest || wa + wb <= 0.f)
                {
                    continue; // (a rope pulls, it doesn't push: bunched up, its slack lies still rather than shoving)
                }
                const glm::vec3 corr = d * ((len - rest) / (len * (wa + wb)));
                a += corr * wa;
                b -= corr * wb;
            }
        }
    }

    // The world: each free point traced from where it was (stopped on what it meets, the rest of its move slid along it;
    // its speed into it gone, some of its speed along it kept), then each piece (one through the world: its free ends
    // pushed out past where it met it).
    const float rad = za::clamp(vr_grapple_rope_radius.value, 0.1f, 8.f);
    for(za::SizeT i = 0; i < ch.p.size(); i++)
    {
        if(pinned[i])
        {
            continue;
        }
        // Lying on the floor: held its thickness over it, its speed into it gone and some of its speed along it lost.
        // (A point moved only by the trace below sank through the thickness each frame and was put back on top: the
        // slack on the floor bounced by the thickness, many times a second.)
        const auto settle = [&] {
            const trace_t down = worldtrace::world(ch.p[i], ch.p[i] - glm::vec3{0.f, 0.f, rad});
            if(down.startsolid || down.allsolid || down.fraction >= 1.f)
            {
                return;
            }
            const glm::vec3 n = worldtrace::normal(down);
            if(n.z < 0.3f)
            {
                return;
            }
            const glm::vec3 on = worldtrace::endPos(down) + n * rad;
            const float up = glm::dot(on - ch.p[i], n);
            if(up <= 0.f || worldtrace::world(ch.p[i], on).fraction < 1.f)
            {
                return;
            }
            glm::vec3 v = ch.p[i] - ch.prev[i];
            v -= n * za::min(0.f, glm::dot(v, n));
            v -= (v - n * glm::dot(v, n)) * (1.f - chainSlide);
            ch.p[i] += n * up;
            ch.prev[i] = ch.p[i] - v;
        };
        if(glm::distance(from[i], ch.p[i]) < 1e-4f)
        {
            settle();
            continue;
        }
        const glm::vec3 was = from[i], to = ch.p[i];
        const trace_t tr = worldtrace::world(was, to);
        if(tr.startsolid || tr.allsolid || tr.fraction >= 1.f)
        {
            if(!tr.startsolid && !tr.allsolid)
            {
                settle();
            }
            continue;
        }
        const glm::vec3 n = worldtrace::normal(tr);
        const glm::vec3 at = worldtrace::endPos(tr) + n * rad;
        glm::vec3 rest = to - at;
        rest -= n * glm::dot(rest, n);
        const trace_t tr2 = worldtrace::world(at, at + rest);
        if(tr2.startsolid)
        {
            ch.p[i] = at;
        }
        else if(tr2.fraction < 1.f)
        {
            ch.p[i] = worldtrace::endPos(tr2) + worldtrace::normal(tr2) * rad;
        }
        else
        {
            ch.p[i] = at + rest;
        }
        glm::vec3 along = to - was;
        along -= n * glm::dot(along, n);
        ch.prev[i] = ch.p[i] - along * chainSlide;
        settle();
    }
    for(za::SizeT i = 0; i + 1 < ch.p.size(); i++)
    {
        if(pinned[i] && pinned[i + 1])
        {
            continue;
        }
        const trace_t tr = worldtrace::world(ch.p[i], ch.p[i + 1]);
        if(tr.startsolid || tr.allsolid || tr.fraction >= 1.f)
        {
            continue;
        }
        const glm::vec3 n = worldtrace::normal(tr);
        const glm::vec3 hit = worldtrace::endPos(tr);
        for(const za::SizeT j : {i, i + 1})
        {
            if(pinned[j])
            {
                continue;
            }
            const float under = glm::dot(hit - ch.p[j], n) + rad;
            if(under > 0.f)
            {
                const glm::vec3 to = ch.p[j] + n * under;
                if(worldtrace::world(ch.p[j], to).fraction >= 1.f)
                {
                    ch.p[j] = to;
                    ch.prev[j] += n * under; // (no speed from it)
                }
            }
        }
    }

    // As drawn: a piece still through the world goes round what it meets (a point off each side's first hit), so the rope
    // is never drawn through it; the chain itself gets out over the next frames.
    ch.drawn.clear();
    ch.drawn.pushBack(ch.p[0]);
    for(za::SizeT i = 0; i + 1 < ch.p.size(); i++)
    {
        if(!pinned[i] || !pinned[i + 1])
        {
            const trace_t there = worldtrace::world(ch.p[i], ch.p[i + 1]);
            if(!there.startsolid && !there.allsolid && there.fraction < 1.f)
            {
                const trace_t back = worldtrace::world(ch.p[i + 1], ch.p[i]);
                ch.drawn.pushBack(worldtrace::endPos(there) + worldtrace::normal(there) * rad);
                if(!back.startsolid && !back.allsolid && back.fraction < 1.f)
                {
                    ch.drawn.pushBack(worldtrace::endPos(back) + worldtrace::normal(back) * rad);
                }
            }
        }
        ch.drawn.pushBack(ch.p[i + 1]);
    }
    ch.fastest = 0.f;
    ch.fastestAt = -1;
    for(za::SizeT i = 0; i < ch.p.size() && !rebuild; i++)
    {
        if(!pinned[i] && glm::distance(ch.p[i], from[i]) > ch.fastest)
        {
            ch.fastest = glm::distance(ch.p[i], from[i]);
            ch.fastestAt = static_cast<int>(i);
        }
    }
    ch.lastDt = dt;
    ch.time = now;
}

// The model's +y and +z at the rope's start, as the engine turns a beam's link along `forward` (pitch and yaw, no
// roll): +y its left, +z its up.
void startFrame(const glm::vec3& forward, glm::vec3& side, glm::vec3& up)
{
    const glm::vec3 worldUp{0.f, 0.f, 1.f};
    glm::vec3 left = glm::cross(worldUp, forward);
    if(glm::dot(left, left) < 1e-8f)
    {
        left = {0.f, 1.f, 0.f}; // straight up or down: yaw 0
    }
    side = glm::normalize(left);
    up = glm::normalize(glm::cross(forward, side));
}

// The points as the curve's samples (their arc lengths, and the model's axes carried along with the least twist from
// the start's), appended to `curve`. The number of samples (points closer than a hundredth of a unit merged).
int addSamples(const za::Vector<glm::vec3>& pts, float& total)
{
    za::Vector<glm::vec3>& p = scratch.samples;
    p.clear();
    for(const glm::vec3& q : pts)
    {
        if(p.empty() || glm::distance(p.back(), q) > 0.01f)
        {
            p.pushBack(q);
        }
    }
    if(p.size() < 2)
    {
        total = 0.f;
        return 0;
    }
    const int n = static_cast<int>(p.size());
    const auto tangent = [&](int i) {
        const glm::vec3 d = i == 0 ? p[1] - p[0] : i == n - 1 ? p[n - 1] - p[n - 2] : p[i + 1] - p[i - 1];
        return glm::normalize(d);
    };
    glm::vec3 side, up;
    startFrame(glm::normalize(p[1] - p[0]), side, up);
    float s = 0.f;
    for(int i = 0; i < n; i++)
    {
        if(i > 0)
        {
            s += glm::distance(p[i - 1], p[i]);
            // Carried on: the last side made square to this tangent (a rotation-minimising frame, to a close
            // approximation at these small steps); a turn so sharp that it lines up with the tangent starts afresh.
            const glm::vec3 t = tangent(i);
            glm::vec3 sd = side - t * glm::dot(side, t);
            if(glm::dot(sd, sd) < 1e-6f)
            {
                startFrame(t, side, up);
            }
            else
            {
                side = glm::normalize(sd);
                up = glm::normalize(glm::cross(t, side));
            }
        }
        gfx::CurveSample c;
        c.pos = {p[i], s};
        c.side = {side, 0.f};
        c.up = {up, 0.f};
        curve.pushBack(c);
    }
    total = s;
    return n;
}

} // namespace

namespace qvr::rope
{

void setSlack(int key, float slack)
{
    slacks[key].target = slack;
}

void parseCorners()
{
    const int ent = MSG_ReadShort();
    const int beamId = MSG_ReadByte();
    const int count = MSG_ReadByte();
    const int key = ent | ((beamId + 1) << 16); // (VR_ParseBeamEntity's key)
    if(count == protocol::ropeEnded)
    {
        // Its hook's rope is another beam's now: this one goes at once (it lasted 0.2 s, drawn from wherever its start
        // went: a phantom rope from another gun, or from the hand that let it go).
        for(int i = 0; i < MAX_BEAMS; i++)
        {
            if(cl_beams[i].entity == key)
            {
                cl_beams[i].model = nullptr;
            }
        }
        if(vr_grapple_debug.value >= 2 && slacks.count(key))
        {
            Con_Printf("grapple: rope beam %d/%d ended (its hook's rope is another beam's now)\n", ent, beamId);
        }
        slacks.erase(key);
        cornerSets.erase(key);
        chains.erase(key);
        return;
    }
    RopeCorners& r = cornerSets[key];
    r.corners.resize(static_cast<za::SizeT>(count));
    glm::vec3 at{0.f};
    for(int i = 0; i < count; i++)
    {
        for(int k = 0; k < 3; k++)
        {
            at[k] = i == 0 ? MSG_ReadCoord(cl.protocolflags) : at[k] + static_cast<float>(MSG_ReadShort()) / 8.f;
        }
        r.corners[static_cast<za::SizeT>(i)] = at;
    }
    r.time = cl.time;
}

// vr_grapple_rope_draw_dump: each drawn rope's chain (tests): its points and corners, how many points are inside the
// world and how many pieces go through it (none: it lies on it, round it).
void drawDump_f()
{
    for(const auto& [key, ch] : chains)
    {
        int inside = 0, crossing = 0;
        float lowest = 1e9f;
        for(za::SizeT i = 0; i < ch.drawn.size(); i++)
        {
            inside += pointInWorld(ch.drawn[i]) ? 1 : 0;
            lowest = za::min(lowest, ch.drawn[i].z);
            if(i + 1 < ch.drawn.size())
            {
                const trace_t tr = worldtrace::world(ch.drawn[i], ch.drawn[i + 1]);
                if(tr.fraction < 0.999f || tr.startsolid)
                {
                    crossing++;
                    Con_Printf("rope drawn %d: piece %d of %d through the world: %.1f %.1f %.1f .. %.1f %.1f %.1f (at %.2f%s)\n",
                        key & 0xFFFF, static_cast<int>(i), static_cast<int>(ch.drawn.size()) - 1, static_cast<double>(ch.drawn[i].x),
                        static_cast<double>(ch.drawn[i].y), static_cast<double>(ch.drawn[i].z), static_cast<double>(ch.drawn[i + 1].x),
                        static_cast<double>(ch.drawn[i + 1].y), static_cast<double>(ch.drawn[i + 1].z), static_cast<double>(tr.fraction),
                        tr.startsolid ? ", starting inside" : "");
                }
            }
        }
        float length = 0.f;
        for(za::SizeT i = 0; i + 1 < ch.drawn.size(); i++)
        {
            length += glm::distance(ch.drawn[i], ch.drawn[i + 1]);
        }
        Con_Printf("rope drawn %d: %d points, %d corners, length %.1f, lowest %.1f; %d points inside the world, %d pieces "
                   "through it; most a point moved %.2f units in %.1f ms (%d of %zu, at z %.1f); into the hook %.0f degrees off\n",
            key & 0xFFFF, static_cast<int>(ch.drawn.size()), static_cast<int>(ch.pins.size()) - 2 - (ch.tail ? 1 : 0),
            static_cast<double>(length), static_cast<double>(lowest), inside, crossing, static_cast<double>(ch.fastest),
            1000.0 * static_cast<double>(ch.lastDt), ch.fastestAt, ch.p.size(), ch.fastestAt >= 0 ? static_cast<double>(ch.p[static_cast<za::SizeT>(ch.fastestAt)].z) : 0.0,
            static_cast<double>(ch.endAngle));
    }
}

void debugDraw()
{
    if(!vr_debug_rope.value || debugFrame < host_framecount - 1)
    {
        return;
    }
    const glm::vec4 piece{0.2f, 1.f, 0.3f, 0.9f}, point{1.f, 1.f, 1.f, 1.f}, corner{1.f, 0.2f, 0.2f, 1.f},
        end{1.f, 0.9f, 0.1f, 1.f}, taut{1.f, 0.9f, 0.1f, 0.5f};
    for(const DebugRope& d : debugRopes)
    {
        for(za::SizeT i = 0; i + 1 < d.points.size(); i++)
        {
            lines::line(d.points[i], d.points[i + 1], 0.12f, piece, piece);
        }
        for(const glm::vec3& q : d.points)
        {
            lines::point(q, 0.5f, point);
        }
        for(za::SizeT i = 0; i < d.path.size(); i++)
        {
            if(i + 1 < d.path.size())
            {
                lines::line(d.path[i], d.path[i + 1], 0.06f, taut, taut);
            }
            lines::point(d.path[i], i == 0 || i + 1 == d.path.size() ? 1.2f : 1.6f,
                i == 0 || i + 1 == d.path.size() ? end : corner);
        }
    }
}

void forget()
{
    slacks.clear();
    cornerSets.clear();
    chains.clear();
    debugRopes.clear();
    queued.clear();
    curve.clear();
}

void drawOpaque()
{
    if(queued.empty())
    {
        return;
    }
    // Made and uploaded once a frame (each mesh once, then the curves), drawn from it in every view.
    auto& [batch, meshFirst, curveBase, builtFrame] = ropeDraw;
    if(builtFrame != host_framecount)
    {
        builtFrame = host_framecount;
        QVR_PROFILE("grapple rope upload");
        za::Vector<glm::vec4>& data = scratch.data;
        za::Vector<const Mesh*>& packed = scratch.packed;
        za::Vector<int>& packedFirst = scratch.packedFirst;
        data.clear();
        packed.clear();
        packedFirst.clear();
        meshFirst.clear();
        for(const Queued& q : queued)
        {
            const auto at = za::find(packed.begin(), packed.end(), q.mesh);
            if(at != packed.end())
            {
                meshFirst.pushBack(packedFirst[at - packed.begin()]);
                continue;
            }
            meshFirst.pushBack(static_cast<int>(data.size()));
            packed.pushBack(q.mesh);
            packedFirst.pushBack(meshFirst.back());
            for(const gfx::BentVertex& v : q.mesh->vertices)
            {
                data.pushBack(v.pos);
                data.pushBack(v.uv);
            }
        }
        curveBase = static_cast<int>(data.size());
        for(const gfx::CurveSample& c : curve)
        {
            data.pushBack(c.pos);
            data.pushBack(c.side);
            data.pushBack(c.up);
        }
        batch = gfx::uploadBent(data);
    }
    QVR_GPU_PROFILE("grapple rope draw");
    for(za::SizeT i = 0; i < queued.size(); i++)
    {
        const Queued& q = queued[i];
        gfx::BentDraw d;
        d.meshFirst = meshFirst[i];
        d.meshVertices = static_cast<int>(q.mesh->vertices.size());
        d.curveFirst = curveBase + 3 * q.firstSample;
        d.samples = q.samples;
        d.copies = q.copies;
        d.period = q.period;
        d.scale = q.scale;
        d.skin = q.mesh->skin;
        d.fullbright = q.mesh->fullbright;
        d.light = q.light;
        gfx::drawBent(batch, d);
    }
}

} // namespace qvr::rope

extern "C" void VR_ForgetEndedRopes()
{
    // This frame's ropes are put anew (VR_DrawRope, for each beam).
    queued.clear();
    curve.clear();
    queuedFrame = host_framecount;

    // A rope whose beam ended (the hook let go, or its hand's other rope): its next one starts from its own slack.
    for(auto it = slacks.begin(); it != slacks.end();)
    {
        bool live = false;
        for(int i = 0; i < MAX_BEAMS && !live; i++)
        {
            const beam_t& b = cl_beams[i];
            live = b.entity == it->first && b.model && b.starttime <= cl.time && b.endtime >= cl.time;
        }
        if(!live && vr_grapple_debug.value >= 2)
        {
            Con_Printf("grapple: rope %d's beam ended (its slack %.3f forgotten)\n", it->first, static_cast<double>(it->second.shown));
        }
        it = live ? (it + 1) : slacks.erase(it);
    }
    for(auto it = cornerSets.begin(); it != cornerSets.end();)
    {
        it = cl.time - it->second.time > 2.0 || it->second.time > cl.time ? cornerSets.erase(it) : (it + 1);
    }
    for(auto it = chains.begin(); it != chains.end();)
    {
        it = slacks.count(it->first) ? (it + 1) : chains.erase(it); // (its beam ended)
    }
    if(debugFrame != host_framecount)
    {
        debugFrame = host_framecount;
        debugRopes.clear();
    }
}

// The rope's end at the hook (`b`: its tail, where the server has the rope start on it) goes into the hook along its
// length: the point `tailLength` (times its size) out of its back, clear of the world, into `out` (false: no hook there,
// a Rogue grapple's rope). The hook is the nearest progs/hook.mdl within a few units.
bool hookTail(const glm::vec3& b, glm::vec3& out, glm::vec3& dir)
{
    const float tailLength = za::clamp(vr_grapple_rope_tail.value, 0.f, 16.f);
    const entity_t* best = nullptr;
    float bestD = 12.f;
    for(int i = 1; i < cl.num_entities; i++)
    {
        const entity_t& e = cl_entities[i];
        if(!e.model || e.msgtime != cl.mtime[0] || strcmp(e.model->name, "progs/hook.mdl"))
        {
            continue;
        }
        const float d = glm::distance(glm::vec3{e.origin[0], e.origin[1], e.origin[2]}, b);
        if(d < bestD)
        {
            bestD = d;
            best = &e;
        }
    }
    if(!best)
    {
        return false;
    }
    vec3_t angles{-best->angles[0], best->angles[1], best->angles[2]}, f, r, u; // (alias models' pitch is the other way)
    AngleVectors(angles, f, r, u);
    const float scale = best->scale ? static_cast<float>(best->scale) / static_cast<float>(ENTSCALE_DEFAULT) : 1.f;
    dir = glm::vec3{f[0], f[1], f[2]};
    const glm::vec3 back = b - dir * (tailLength * scale);
    const trace_t tr = worldtrace::world(b, back);
    if(tr.startsolid || tr.allsolid)
    {
        return false;
    }
    out = tr.fraction < 1.f ? glm::mix(b, back, za::max(0.f, tr.fraction - 0.1f)) : back;
    return glm::distance(out, b) > 0.5f;
}

extern "C" int VR_DrawRope(int ent, qmodel_t* model, const float* start, const float* end)
{
    const auto it = slacks.find(ent);
    if(it == slacks.end())
    {
        return 0; // not a grappling hook's rope (or not the Quake VR progs')
    }
    const Mesh* mesh = meshOf(model);
    if(!mesh)
    {
        return 0;
    }
    QVR_PROFILE("grapple rope");
    static bool registered = false;
    if(!registered)
    {
        registered = true;
        Cmd_AddCommand("vr_grapple_rope_draw_dump", rope::drawDump_f);
    }
    const double t0 = Sys_DoubleTime();
    RopeSlack& r = it->second;
    const glm::vec3 a{start[0], start[1], start[2]};
    const glm::vec3 b{end[0], end[1], end[2]};
    za::Vector<glm::vec3>& pts = scratch.points;
    float length = 0.f, h = 0.f;
    easeSlack(r);
    if(vr_grapple_rope_sim.value)
    {
        // The server's rope (vr_ropesim.cpp): straight between the corners it wraps round; its slack hanging between
        // them (the chain), or straight (vr_grapple_sag 0).
        za::Vector<glm::vec3>& path = scratch.path;
        path.clear();
        path.pushBack(a);
        const auto c = cornerSets.find(ent);
        if(c != cornerSets.end() && cl.time - c->second.time <= cornersFresh && c->second.time <= cl.time)
        {
            path.emplaceBackRange(c->second.corners.data(), c->second.corners.size());
        }
        // Into the hook along it, not across it (the last pieces hung from its tail any way, and the last link stuck out
        // through its side).
        glm::vec3 tail, dir{0.f};
        const bool hasTail = hookTail(b, tail, dir) && glm::distance(path.back(), tail) > 1.f;
        if(hasTail)
        {
            path.pushBack(tail);
        }
        path.pushBack(b);
        if(vr_grapple_debug.value >= 4 && developer.value)
        {
            // (Every rope drawn, each frame: which beam, from where to where, round which corners.)
            const glm::vec3 c0 = path.size() > 2 ? path[1] : b;
            Con_Printf("grapple: rope beam %d/%d drawn: %.0f %.0f %.0f -> %.0f %.0f %.0f, %d corners (first %.0f %.0f %.0f)\n",
                ent & 0xFFFF, (ent >> 16) - 1, static_cast<double>(a.x), static_cast<double>(a.y), static_cast<double>(a.z),
                static_cast<double>(b.x), static_cast<double>(b.y), static_cast<double>(b.z),
                static_cast<int>(path.size()) - 2 - (hasTail ? 1 : 0), static_cast<double>(c0.x), static_cast<double>(c0.y),
                static_cast<double>(c0.z));
        }
        if(vr_grapple_sag.value)
        {
            Chain& ch = chains[ent];
            ch.tail = hasTail;
            stepChain(ch, path, r.shown);
            simPoints(ch.drawn, a, b, pts, length);
            // (vr_grapple_rope_draw_dump: how far the rope's last 3 units turn from the hook's length.)
            ch.endAngle = -1.f;
            if(dir != glm::vec3{0.f} && pts.size() >= 2)
            {
                za::SizeT k = pts.size() - 2;
                while(k > 0 && glm::distance(pts[k], pts.back()) < 3.f)
                {
                    k--;
                }
                const glm::vec3 d = pts.back() - pts[k];
                if(glm::length(d) > 0.1f)
                {
                    ch.endAngle = glm::degrees(za::acos(za::clamp(glm::dot(glm::normalize(d), dir), -1.f, 1.f)));
                }
            }
            if(vr_debug_rope.value)
            {
                debugRopes.pushBack({ch.drawn, path});
            }
        }
        else
        {
            pts = path;
            if(vr_debug_rope.value)
            {
                debugRopes.pushBack({path, path});
            }
        }
    }
    else
    {
        ropePoints(r, a, b, pts, length, h);
    }
    Queued q;
    q.mesh = mesh;
    q.firstSample = static_cast<int>(curve.size());
    float total = 0.f;
    q.samples = addSamples(pts, total);
    if(q.samples < 2)
    {
        return 1; // (its ends together: nothing to draw)
    }
    q.scale = VR_BeamScale(model);
    // The links from the gun, the last one ending at the hook (not past it, along the rope's end: it stuck out of the
    // hook): a little closer together to fit.
    const float along = total / q.scale; // model units
    q.period = linkPeriod;
    q.copies = 1;
    if(along > mesh->xMax + 0.01f)
    {
        q.copies = 1 + static_cast<int>(za::ceil((along - mesh->xMax) / linkPeriod));
        q.period = (along - mesh->xMax) / static_cast<float>(q.copies - 1);
    }
    q.light = worldLight(pts[pts.size() / 2]);
    queued.pushBack(q);

    if(vr_grapple_debug.value >= 2 && realtime >= r.logAt)
    {
        r.logAt = realtime + 0.5;
        Con_Printf("grapple: rope %d drawn: chord %.1f, length %.1f, sag %.1f, %d samples, %d links, built in %.1f us\n",
            ent, static_cast<double>(glm::distance(a, b)), static_cast<double>(total), static_cast<double>(h), q.samples,
            q.copies, (Sys_DoubleTime() - t0) * 1e6);
    }
    return 1;
}
