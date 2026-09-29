// vr_rope.cpp -- see vr_rope.hpp.

#include "vr_rope.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_gfx.hpp"
#include "vr_lines.hpp"
#include "vr_mem.hpp"
#include "vr_profile.hpp"
#include "vr_trace.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>

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
std::unordered_map<int, RopeSlack> slacks;

// A rope's corners as last sent (beam key -> the corners the server's rope wraps round, vr_ropesim.cpp, and when): the
// drawn chain is pinned at them. Not sent for a while: none (the server sends them when they change, and every quarter
// of a second while there are any).
struct RopeCorners
{
    std::vector<glm::vec3> corners;
    double time{0.0}; // cl.time
};
std::unordered_map<int, RopeCorners> cornerSets;
constexpr double cornersFresh = 1.0; // seconds

// A rope's slack as drawn (vr_grapple_rope_sim): a chain of points, Rope Point Spacing apart, pinned at the gun, the
// corners and the hook, hanging between them (gravity, a little drag) and lying on the world: each point is traced
// from where it was to where it goes (stopped on what it meets, sliding along it), and each piece is traced too (one
// that goes through the world is pushed out of it). Beam key -> chain.
struct Chain
{
    std::vector<glm::vec3> p;    // the points (the pins among them)
    std::vector<glm::vec3> prev; // last frame's (Verlet)
    std::vector<int> pins;       // each path point's (the gun, the corners, the hook) index in p
    std::vector<glm::vec3> drawn; // the points drawn: p, a piece still through the world taken round what it meets
    float lastDt{0.f};
    double time{-1.0};           // realtime of its last step
};
std::unordered_map<int, Chain> chains;
constexpr int chainMaxPoints = 128;
constexpr float chainDrag = 1.5f;      // of a point's speed lost to the air, a second
constexpr float chainSlide = 0.6f;     // of its speed along a surface a point keeps where it touches it
constexpr float chainGravity = 800.f;  // units/s/s

// vr_debug_rope: this frame's drawn ropes (their points, pins and path), drawn in the world by debugDraw.
struct DebugRope
{
    std::vector<glm::vec3> points;
    std::vector<glm::vec3> path;
};
std::vector<DebugRope> debugRopes;
int debugFrame = -1;

// A model's mesh as the rope draws it (its first pose's triangles, BentVertex each), and its skin.
struct Mesh
{
    const aliashdr_t* hdr{nullptr};
    std::vector<gfx::BentVertex> vertices;
    gfx::Texture skin{0};
    gfx::Texture fullbright{0};
};

[[nodiscard]] std::size_t heldBytes(const Mesh& m) // (vr_mem.hpp)
{
    return mem::heldBytes(m.vertices);
}

// The ropes' models' meshes, by model (checked against its data on use; the models' slots are other models' after a
// game change, their data made again by a model reload).
struct RopeMeshes
{
    std::unordered_map<const qmodel_t*, Mesh> meshes;
    auto members() { return std::tie(meshes); }
};
mem::Cache<RopeMeshes> cache{"rope meshes", mem::GameDirChange | mem::ModelReload};

// The rope's curve worked out, and the frame's upload (the main thread).
struct RopeScratch
{
    std::vector<glm::vec3> onLine;         // the points on the line (ropePoints)
    std::vector<char> lies;                // whether each lies on the floor (traced points)
    std::vector<glm::vec3> samples;        // the points kept as the curve's samples (addSamples)
    std::vector<glm::vec3> points;         // a rope's points (VR_DrawRope)
    std::vector<glm::vec4> data;           // the frame's upload: each mesh once, then the curves (drawOpaque)
    std::vector<const Mesh*> packed;       // each mesh once
    std::vector<int> packedFirst;          // and its first vec4
    std::vector<glm::vec3> path;           // a rope's path: the gun, the corners, the hook (VR_DrawRope)
    std::vector<glm::vec3> from;           // the chain's points before this step (stepChain)
    std::vector<glm::vec3> resampled;      // (resample)
    std::vector<float> lengths;            // each piece between two pins: its length and its points
    std::vector<int> counts;
    std::vector<char> pinned;              // each chain point: a pin
    auto members() { return std::tie(onLine, lies, samples, points, data, packed, packedFirst, path, from, resampled, lengths, counts, pinned); }
};
mem::Scratch<RopeScratch> scratch{"rope"};

// The frame's ropes as uploaded (drawOpaque's first view), drawn from it in every view.
struct RopeDraw
{
    gfx::BentBatch batch;
    std::vector<int> meshFirst; // queued[i]'s mesh's first vec4
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
    float scale;
    glm::vec3 light;
};
std::vector<Queued> queued;
std::vector<gfx::CurveSample> curve;
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
    Mesh& m = cache.meshes[model];
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
    for(int i = 0; i < hdr->numindexes; i++)
    {
        const aliasmesh_t& d = desc[indexes[i]];
        const trivertx_t& t = pose[d.vertindex];
        gfx::BentVertex v;
        v.pos = {hdr->scale[0] * t.v[0] + hdr->scale_origin[0], hdr->scale[1] * t.v[1] + hdr->scale_origin[1],
            hdr->scale[2] * t.v[2] + hdr->scale_origin[2], 0.f};
        v.uv = {hs * (static_cast<float>(d.st[0]) + 0.5f), vs * (static_cast<float>(d.st[1]) + 0.5f), 0.f, 0.f};
        m.vertices.push_back(v);
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
    const float dt = static_cast<float>(std::clamp(realtime - r.time, 0.0, 0.1));
    r.time = realtime;
    r.shown += (r.target - r.shown) * std::min(1.f, 10.f * dt);
}

// The rope from `a` to `b` as points into `out` (at least 2): straight (2) when taut, hanging when slack: the parabola
// of its length over the line between its ends, sagging down across the line (a rope hanging straight down stays
// straight), lying on the floor where it would go through it. `h`: how deep it hangs (0 straight).
void ropePoints(const RopeSlack& r, const glm::vec3& a, const glm::vec3& b, std::vector<glm::vec3>& out, float& length, float& h)
{
    out.clear();
    out.push_back(a);
    const float chord = glm::distance(a, b);
    length = chord;
    h = 0.f;
    if(!vr_grapple_sag.value || r.shown < 0.002f || chord < 1.f)
    {
        out.push_back(b);
        return;
    }

    // A parabola of height h over a chord D is about D + 8 h^2 / (3 D) long; no deeper than half the rope (a V).
    length = chord / std::max(0.02f, 1.f - r.shown);
    h = std::min(0.5f * length, std::sqrt(3.f * chord * (length - chord) / 8.f));
    const glm::vec3 along = (b - a) / chord;
    const glm::vec3 down = glm::vec3{0.f, 0.f, -1.f} - along * -along.z; // gravity across the line
    const float depth = h * glm::length(down);
    if(depth < 0.5f)
    {
        out.push_back(b);
        h = 0.f;
        return;
    }

    // As many pieces as its bend needs: its curvature is greatest at the bottom, 8 h / D^2 (the sag across the line),
    // over the curve's length (summed over 16 chords: the length formula above is only close for a shallow sag).
    const float kappa = 8.f * depth / (chord * chord);
    const float step = std::max(minStep, std::min(std::sqrt(8.f * sagittaTolerance / kappa), maxBend / kappa));
    float arc = 0.f;
    for(int i = 0; i < 16; i++)
    {
        const float t0 = static_cast<float>(i) / 16.f, t1 = static_cast<float>(i + 1) / 16.f;
        arc += glm::length((b - a) * (t1 - t0) + down * (4.f * h * (t1 * (1.f - t1) - t0 * (1.f - t0))));
    }
    const int n = std::clamp(static_cast<int>(std::ceil(arc / step)), minPieces, maxPieces);
    // The floor: each point traced from the line down to it (the old 17 points' way), lying on what it meets. A long
    // rope's points are traced maxTraces at most, evenly (every k-th); between two traced ones that both lie on the
    // floor, the points lie on the line between them (a flat floor: on it); where one lies and the other hangs (where the
    // rope leaves the floor), every point between is traced.
    std::vector<glm::vec3>& onLine = scratch.onLine;
    std::vector<char>& lies = scratch.lies; // on the floor (traced points)
    onLine.assign(n + 1, a);
    lies.assign(n + 1, 0);
    for(int i = 1; i < n; i++)
    {
        const float t = static_cast<float>(i) / static_cast<float>(n);
        onLine[i] = a + (b - a) * t;
        out.push_back(onLine[i] + down * (4.f * h * t * (1.f - t)));
    }
    out.push_back(b);
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
void simPoints(const std::vector<glm::vec3>& sim, const glm::vec3& a, const glm::vec3& b, std::vector<glm::vec3>& out,
    float& length)
{
    std::vector<glm::vec3>& p = scratch.onLine;
    p = sim;
    const int n = static_cast<int>(p.size());
    // The ends as drawn: the gun where the hand is now, not where the server last had it; its difference eased out over
    // the first pieces (and the hook's over the last ones).
    const glm::vec3 da = a - p.front();
    const glm::vec3 db = b - p.back();
    const int ease = std::min(4, n / 2);
    for(int i = 0; i < ease; i++)
    {
        const float w = 1.f - static_cast<float>(i) / static_cast<float>(ease);
        p[static_cast<std::size_t>(i)] += da * w;
        p[static_cast<std::size_t>(n - 1 - i)] += db * w;
    }
    out.clear();
    length = 0.f;
    for(int i = 0; i + 1 < n; i++)
    {
        const glm::vec3& p0 = p[static_cast<std::size_t>(std::max(0, i - 1))];
        const glm::vec3& p1 = p[static_cast<std::size_t>(i)];
        const glm::vec3& p2 = p[static_cast<std::size_t>(i + 1)];
        const glm::vec3& p3 = p[static_cast<std::size_t>(std::min(n - 1, i + 2))];
        const float len = glm::distance(p1, p2);
        length += len;
        const int k = std::clamp(static_cast<int>(std::ceil(len / 2.f)), 1, 6);
        for(int j = 0; j < k; j++)
        {
            const float t = static_cast<float>(j) / static_cast<float>(k);
            const float t2 = t * t, t3 = t2 * t;
            out.push_back(0.5f * (2.f * p1 + (p2 - p0) * t + (2.f * p0 - 5.f * p1 + 4.f * p2 - p3) * t2 +
                                     (3.f * p1 - p0 - 3.f * p2 + p3) * t3));
        }
    }
    out.push_back(p.back());
}

// The polyline `pts[0 .. count - 1]` as `n` + 1 points evenly along it (its ends kept), appended to `out`.
void resample(const glm::vec3* pts, int count, int n, std::vector<glm::vec3>& out)
{
    float total = 0.f;
    for(int i = 1; i < count; i++)
    {
        total += glm::distance(pts[i - 1], pts[i]);
    }
    out.push_back(pts[0]);
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
        const float t = len > 1e-4f ? std::clamp((want - segStart) / len, 0.f, 1.f) : 0.f;
        out.push_back(count > 1 ? glm::mix(pts[seg - 1], pts[seg], t) : pts[0]);
    }
    out.push_back(pts[count - 1]);
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
// 1): the pieces between two pins share the slack by their lengths.
void stepChain(Chain& ch, const std::vector<glm::vec3>& path, float slack)
{
    const int pieces = static_cast<int>(path.size()) - 1;
    std::vector<float>& lengths = scratch.lengths;
    std::vector<int>& counts = scratch.counts;
    lengths.assign(static_cast<std::size_t>(pieces), 0.f);
    counts.assign(static_cast<std::size_t>(pieces), 1);
    float straight = 0.f;
    for(int i = 0; i < pieces; i++)
    {
        straight += glm::distance(path[static_cast<std::size_t>(i)], path[static_cast<std::size_t>(i) + 1]);
    }
    const float total = straight / std::max(0.02f, 1.f - std::clamp(slack, 0.f, 1.f));
    const float spacing =
        std::max({2.f, vr_grapple_rope_spacing.value, total / static_cast<float>(chainMaxPoints - pieces - 1)});
    for(int i = 0; i < pieces; i++)
    {
        const float d = glm::distance(path[static_cast<std::size_t>(i)], path[static_cast<std::size_t>(i) + 1]);
        const float share = straight > 1e-3f ? d / straight : 1.f / static_cast<float>(pieces);
        lengths[static_cast<std::size_t>(i)] = d + (total - straight) * share;
        counts[static_cast<std::size_t>(i)] =
            std::max(1, static_cast<int>(std::ceil(lengths[static_cast<std::size_t>(i)] / spacing)));
    }

    const double now = realtime;
    bool rebuild = ch.p.empty() || ch.pins.size() != path.size() || ch.time < 0.0 || now - ch.time > 0.5 || now < ch.time;
    for(std::size_t k = 0; k < ch.pins.size() && !rebuild; k++)
    {
        rebuild = glm::distance(ch.p[static_cast<std::size_t>(ch.pins[k])], path[k]) > 128.f; // (jumped: a new place)
    }
    const float dt = rebuild ? 1.f / 90.f : static_cast<float>(std::clamp(now - ch.time, 1.0 / 500.0, 1.0 / 20.0));
    if(rebuild)
    {
        ch.p.clear();
        ch.pins.clear();
        for(int i = 0; i < pieces; i++)
        {
            ch.pins.push_back(static_cast<int>(ch.p.size()));
            for(int j = 0; j < counts[static_cast<std::size_t>(i)]; j++)
            {
                ch.p.push_back(glm::mix(path[static_cast<std::size_t>(i)], path[static_cast<std::size_t>(i) + 1],
                    static_cast<float>(j) / static_cast<float>(counts[static_cast<std::size_t>(i)])));
            }
        }
        ch.pins.push_back(static_cast<int>(ch.p.size()));
        ch.p.push_back(path.back());
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
            same = ch.pins[static_cast<std::size_t>(i) + 1] - ch.pins[static_cast<std::size_t>(i)] ==
                   counts[static_cast<std::size_t>(i)];
        }
        if(!same)
        {
            for(std::vector<glm::vec3>* v : {&ch.p, &ch.prev})
            {
                std::vector<glm::vec3>& out = scratch.resampled;
                out.clear();
                for(int i = 0; i < pieces; i++)
                {
                    const int a = ch.pins[static_cast<std::size_t>(i)], b = ch.pins[static_cast<std::size_t>(i) + 1];
                    resample(v->data() + a, b - a + 1, counts[static_cast<std::size_t>(i)], out);
                    out.pop_back(); // (the next piece's first)
                }
                out.push_back(v->back());
                *v = out;
            }
            int at = 0;
            for(int i = 0; i < pieces; i++)
            {
                ch.pins[static_cast<std::size_t>(i)] = at;
                at += counts[static_cast<std::size_t>(i)];
            }
            ch.pins.back() = at;
        }
    }
    std::vector<char>& pinned = scratch.pinned;
    pinned.assign(ch.p.size(), 0);
    for(std::size_t k = 0; k < ch.pins.size(); k++)
    {
        const std::size_t i = static_cast<std::size_t>(ch.pins[k]);
        pinned[i] = 1;
        ch.p[i] = ch.prev[i] = path[k];
    }

    // Verlet: the free points fall, a little slowed by the air.
    std::vector<glm::vec3>& from = scratch.from;
    from = ch.p;
    const float keep = std::exp(-chainDrag * dt) * (ch.lastDt > 0.f ? dt / ch.lastDt : 1.f);
    const glm::vec3 fall{0.f, 0.f, -chainGravity * dt * dt};
    for(std::size_t i = 0; i < ch.p.size(); i++)
    {
        if(!pinned[i])
        {
            const glm::vec3 v = (ch.p[i] - ch.prev[i]) * keep;
            ch.prev[i] = ch.p[i];
            ch.p[i] += v + fall;
        }
    }

    // The pieces' lengths (the pins fixed).
    const int iterations = std::clamp(static_cast<int>(vr_grapple_rope_iterations.value), 1, 64);
    for(int it = 0; it < iterations; it++)
    {
        for(int i = 0; i < pieces; i++)
        {
            const float rest = lengths[static_cast<std::size_t>(i)] / static_cast<float>(counts[static_cast<std::size_t>(i)]);
            for(int j = ch.pins[static_cast<std::size_t>(i)]; j < ch.pins[static_cast<std::size_t>(i) + 1]; j++)
            {
                glm::vec3& a = ch.p[static_cast<std::size_t>(j)];
                glm::vec3& b = ch.p[static_cast<std::size_t>(j) + 1];
                const float wa = pinned[static_cast<std::size_t>(j)] ? 0.f : 1.f;
                const float wb = pinned[static_cast<std::size_t>(j) + 1] ? 0.f : 1.f;
                const glm::vec3 d = b - a;
                const float len = glm::length(d);
                if(len < 1e-5f || wa + wb <= 0.f)
                {
                    continue;
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
    const float rad = std::clamp(vr_grapple_rope_radius.value, 0.1f, 8.f);
    for(std::size_t i = 0; i < ch.p.size(); i++)
    {
        if(pinned[i] || glm::distance(from[i], ch.p[i]) < 1e-4f)
        {
            continue;
        }
        const glm::vec3 was = from[i], to = ch.p[i];
        const trace_t tr = worldtrace::world(was, to);
        if(tr.startsolid || tr.allsolid || tr.fraction >= 1.f)
        {
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
    }
    for(std::size_t i = 0; i + 1 < ch.p.size(); i++)
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
        for(const std::size_t j : {i, i + 1})
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
    ch.drawn.push_back(ch.p[0]);
    for(std::size_t i = 0; i + 1 < ch.p.size(); i++)
    {
        if(!pinned[i] || !pinned[i + 1])
        {
            const trace_t there = worldtrace::world(ch.p[i], ch.p[i + 1]);
            if(!there.startsolid && !there.allsolid && there.fraction < 1.f)
            {
                const trace_t back = worldtrace::world(ch.p[i + 1], ch.p[i]);
                ch.drawn.push_back(worldtrace::endPos(there) + worldtrace::normal(there) * rad);
                if(!back.startsolid && !back.allsolid && back.fraction < 1.f)
                {
                    ch.drawn.push_back(worldtrace::endPos(back) + worldtrace::normal(back) * rad);
                }
            }
        }
        ch.drawn.push_back(ch.p[i + 1]);
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
int addSamples(const std::vector<glm::vec3>& pts, float& total)
{
    std::vector<glm::vec3>& p = scratch.samples;
    p.clear();
    for(const glm::vec3& q : pts)
    {
        if(p.empty() || glm::distance(p.back(), q) > 0.01f)
        {
            p.push_back(q);
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
        curve.push_back(c);
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
    RopeCorners& r = cornerSets[ent | ((beamId + 1) << 16)]; // (VR_ParseBeamEntity's key)
    r.corners.resize(static_cast<std::size_t>(count));
    glm::vec3 at{0.f};
    for(int i = 0; i < count; i++)
    {
        for(int k = 0; k < 3; k++)
        {
            at[k] = i == 0 ? MSG_ReadCoord(cl.protocolflags) : at[k] + static_cast<float>(MSG_ReadShort()) / 8.f;
        }
        r.corners[static_cast<std::size_t>(i)] = at;
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
        for(std::size_t i = 0; i < ch.drawn.size(); i++)
        {
            inside += pointInWorld(ch.drawn[i]) ? 1 : 0;
            lowest = std::min(lowest, ch.drawn[i].z);
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
        for(std::size_t i = 0; i + 1 < ch.drawn.size(); i++)
        {
            length += glm::distance(ch.drawn[i], ch.drawn[i + 1]);
        }
        Con_Printf("rope drawn %d: %d points, %d corners, length %.1f, lowest %.1f; %d points inside the world, %d pieces "
                   "through it\n",
            key & 0xFFFF, static_cast<int>(ch.drawn.size()), static_cast<int>(ch.pins.size()) - 2, static_cast<double>(length),
            static_cast<double>(lowest), inside, crossing);
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
        for(std::size_t i = 0; i + 1 < d.points.size(); i++)
        {
            lines::line(d.points[i], d.points[i + 1], 0.12f, piece, piece);
        }
        for(const glm::vec3& q : d.points)
        {
            lines::point(q, 0.5f, point);
        }
        for(std::size_t i = 0; i < d.path.size(); i++)
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
        std::vector<glm::vec4>& data = scratch.data;
        std::vector<const Mesh*>& packed = scratch.packed;
        std::vector<int>& packedFirst = scratch.packedFirst;
        data.clear();
        packed.clear();
        packedFirst.clear();
        meshFirst.clear();
        for(const Queued& q : queued)
        {
            const auto at = std::find(packed.begin(), packed.end(), q.mesh);
            if(at != packed.end())
            {
                meshFirst.push_back(packedFirst[at - packed.begin()]);
                continue;
            }
            meshFirst.push_back(static_cast<int>(data.size()));
            packed.push_back(q.mesh);
            packedFirst.push_back(meshFirst.back());
            for(const gfx::BentVertex& v : q.mesh->vertices)
            {
                data.push_back(v.pos);
                data.push_back(v.uv);
            }
        }
        curveBase = static_cast<int>(data.size());
        for(const gfx::CurveSample& c : curve)
        {
            data.push_back(c.pos);
            data.push_back(c.side);
            data.push_back(c.up);
        }
        batch = gfx::uploadBent(data);
    }
    QVR_GPU_PROFILE("grapple rope draw");
    for(std::size_t i = 0; i < queued.size(); i++)
    {
        const Queued& q = queued[i];
        gfx::BentDraw d;
        d.meshFirst = meshFirst[i];
        d.meshVertices = static_cast<int>(q.mesh->vertices.size());
        d.curveFirst = curveBase + 3 * q.firstSample;
        d.samples = q.samples;
        d.copies = q.copies;
        d.period = linkPeriod;
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
        it = live ? std::next(it) : slacks.erase(it);
    }
    for(auto it = cornerSets.begin(); it != cornerSets.end();)
    {
        it = cl.time - it->second.time > 2.0 || it->second.time > cl.time ? cornerSets.erase(it) : std::next(it);
    }
    for(auto it = chains.begin(); it != chains.end();)
    {
        it = slacks.count(it->first) ? std::next(it) : chains.erase(it); // (its beam ended)
    }
    if(debugFrame != host_framecount)
    {
        debugFrame = host_framecount;
        debugRopes.clear();
    }
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
    std::vector<glm::vec3>& pts = scratch.points;
    float length = 0.f, h = 0.f;
    easeSlack(r);
    if(vr_grapple_rope_sim.value)
    {
        // The server's rope (vr_ropesim.cpp): straight between the corners it wraps round; its slack hanging between
        // them (the chain), or straight (vr_grapple_sag 0).
        std::vector<glm::vec3>& path = scratch.path;
        path.clear();
        path.push_back(a);
        const auto c = cornerSets.find(ent);
        if(c != cornerSets.end() && cl.time - c->second.time <= cornersFresh && c->second.time <= cl.time)
        {
            path.insert(path.end(), c->second.corners.begin(), c->second.corners.end());
        }
        path.push_back(b);
        if(vr_grapple_sag.value)
        {
            Chain& ch = chains[ent];
            stepChain(ch, path, r.shown);
            simPoints(ch.drawn, a, b, pts, length);
            if(vr_debug_rope.value)
            {
                debugRopes.push_back({ch.drawn, path});
            }
        }
        else
        {
            pts = path;
            if(vr_debug_rope.value)
            {
                debugRopes.push_back({path, path});
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
    q.copies = std::max(1, static_cast<int>(std::ceil(total / (linkPeriod * q.scale))));
    q.light = worldLight(pts[pts.size() / 2]);
    queued.push_back(q);

    if(vr_grapple_debug.value >= 2 && realtime >= r.logAt)
    {
        r.logAt = realtime + 0.5;
        Con_Printf("grapple: rope %d drawn: chord %.1f, length %.1f, sag %.1f, %d samples, %d links, built in %.1f us\n",
            ent, static_cast<double>(glm::distance(a, b)), static_cast<double>(total), static_cast<double>(h), q.samples,
            q.copies, (Sys_DoubleTime() - t0) * 1e6);
    }
    return 1;
}
