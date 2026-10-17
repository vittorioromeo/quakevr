// vr_ambient.cpp -- see vr_ambient.hpp.

#include "vr_ambient.hpp"
#include "vr_main.hpp"
#include "vr_cvars.hpp"
#include "vr_evict.hpp"
#include "vr_jobs.hpp"
#include "vr_lighting.hpp"
#include "vr_profile.hpp"

#include "Zancle/Algorithm/Copy.hpp"
#include "Zancle/Algorithm/Find.hpp"
#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Base/UIntPtrT.hpp"
#include "Zancle/Chrono/Clock.hpp"
#include "Zancle/Chrono/Time.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/MinMax.hpp"


using namespace qvr;

namespace
{

// Rays from the model: the 6 axes, the 12 edges' and the 8 corners' directions of a cube.
constexpr int NUM_RAYS = 26;
constexpr float RAY_LENGTH = 1024.f;

// Retraced when it moved this far (held weapons and hands: less, they move with the hand and are
// seen closest) or this long passed (doors open, lights switch).
constexpr float MOVE_UNITS = 16.f;
constexpr float MOVE_UNITS_VIEW = 6.f;
constexpr double REFRESH_SECONDS = 1.0;

// Traces a frame (entities drawn for the first time go first, then the held ones, then the stale):
// the cost stays small with many monsters in view.
constexpr int MAX_TRACED_PER_FRAME = 8;
constexpr int MAX_FRESH_PER_FRAME = 24;

// How fast the cube fades from the last trace to a new one (per second).
constexpr float EASE_RATE = 5.f;

// How much of the model's directional shading (from vr_modellight's lights) the cube replaces:
// half, so a lamp in sight still lights its side, and the surroundings give the rest.
constexpr float DIR_REPLACED = 0.5f;

constexpr glm::vec3 AXES[6] = {{1.f, 0.f, 0.f}, {-1.f, 0.f, 0.f}, {0.f, 1.f, 0.f},
                               {0.f, -1.f, 0.f}, {0.f, 0.f, 1.f}, {0.f, 0.f, -1.f}};

struct Rays
{
    glm::vec3 dir[NUM_RAYS];
    float weight[NUM_RAYS][6]; // each face's cosine lobe over the rays, summing to 1 per face
};

// Built before main (no first-call guard): read by any thread.
const Rays rayTable = [] {
    Rays r{};
    int n = 0;
    for(int x = -1; x <= 1; x++)
    {
        for(int y = -1; y <= 1; y++)
        {
            for(int z = -1; z <= 1; z++)
            {
                if(x || y || z)
                {
                    r.dir[n++] = glm::normalize(glm::vec3{x, y, z});
                }
            }
        }
    }
    for(int f = 0; f < 6; f++)
    {
        float sum = 0.f;
        for(int i = 0; i < NUM_RAYS; i++)
        {
            r.weight[i][f] = za::max(0.f, glm::dot(r.dir[i], AXES[f]));
            sum += r.weight[i][f];
        }
        for(int i = 0; i < NUM_RAYS; i++)
        {
            r.weight[i][f] /= sum;
        }
    }
    return r;
}();

const Rays& rays()
{
    return rayTable;
}

enum class HitKind
{
    None,
    Surface,
    Sky,
    Lava
};

struct Hit
{
    HitKind kind{HitKind::None};
    const msurface_t* surf{nullptr};
    int ds{0};
    int dt{0};
};

// The baked light at a point of a surface (InterpolateLightmap, gl_rlight.c): every light style at
// its current value, coloured, bilinear between the luxels. 0..255 (128: Quake's full light).
glm::vec3 lightmapAt(const msurface_t* surf, int ds, int dt)
{
    const int smax = (surf->extents[0] >> 4) + 1;
    const int tmax = (surf->extents[1] >> 4) + 1;
    const int s0 = za::min(ds >> 4, smax - 1), t0 = za::min(dt >> 4, tmax - 1);
    const int s1 = za::min(s0 + 1, smax - 1), t1 = za::min(t0 + 1, tmax - 1);
    const float fs = static_cast<float>(ds & 15) / 16.f, ft = static_cast<float>(dt & 15) / 16.f;
    const byte* lightmap = surf->samples;
    glm::vec3 c00{0.f}, c01{0.f}, c10{0.f}, c11{0.f};
    for(int maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++)
    {
        const float scale = static_cast<float>(d_lightstylevalue[surf->styles[maps]]) / 256.f;
        const auto at = [&](int s, int t) {
            const byte* p = lightmap + (t * smax + s) * 3;
            return glm::vec3{p[0], p[1], p[2]} * scale;
        };
        c00 += at(s0, t0);
        c01 += at(s1, t0);
        c10 += at(s0, t1);
        c11 += at(s1, t1);
        lightmap += smax * tmax * 3;
    }
    return glm::mix(glm::mix(c00, c01, fs), glm::mix(c10, c11, fs), ft);
}

// RecursiveLightPoint (gl_rlight.c) along any ray: the first world surface the ray crosses that
// faces it, where its texture's extents hold the crossing. Liquids other than lava are seen
// through (the floor under them is lit); surfaces without a lightmap are passed.
bool trace(const mnode_t* node, const glm::vec3& dir, glm::vec3 start, const glm::vec3& end, Hit& hit)
{
    while(node->contents >= 0)
    {
        const mplane_t* plane = node->plane;
        float front, back;
        if(plane->type < 3)
        {
            front = start[plane->type] - plane->dist;
            back = end[plane->type] - plane->dist;
        }
        else
        {
            const glm::vec3 n{plane->normal[0], plane->normal[1], plane->normal[2]};
            front = glm::dot(start, n) - plane->dist;
            back = glm::dot(end, n) - plane->dist;
        }
        if((back < 0.f) == (front < 0.f))
        {
            node = node->children[front < 0.f];
            continue;
        }

        const glm::vec3 mid = start + (end - start) * (front / (front - back));
        if(trace(node->children[front < 0.f], dir, start, mid, hit))
        {
            return true;
        }

        const msurface_t* surf = cl.worldmodel->surfaces + node->firstsurface;
        for(unsigned i = 0; i < node->numsurfaces; i++, surf++)
        {
            const float* pn = surf->plane->normal;
            float facing = pn[0] * dir.x + pn[1] * dir.y + pn[2] * dir.z;
            if(surf->flags & SURF_PLANEBACK)
            {
                facing = -facing;
            }
            if(facing >= 0.f)
            {
                continue;
            }
            const float* v0 = surf->texinfo->vecs[0];
            const float* v1 = surf->texinfo->vecs[1];
            int ds = static_cast<int>(static_cast<double>(mid.x) * v0[0] + static_cast<double>(mid.y) * v0[1] +
                                      static_cast<double>(mid.z) * v0[2] + v0[3]);
            int dt = static_cast<int>(static_cast<double>(mid.x) * v1[0] + static_cast<double>(mid.y) * v1[1] +
                                      static_cast<double>(mid.z) * v1[2] + v1[3]);
            if(ds < surf->texturemins[0] || dt < surf->texturemins[1])
            {
                continue;
            }
            ds -= surf->texturemins[0];
            dt -= surf->texturemins[1];
            if(ds > surf->extents[0] || dt > surf->extents[1])
            {
                continue;
            }
            if(surf->flags & SURF_DRAWSKY)
            {
                hit.kind = HitKind::Sky;
                return true;
            }
            if(surf->flags & SURF_DRAWTURB)
            {
                if(surf->flags & SURF_DRAWLAVA)
                {
                    hit.kind = HitKind::Lava;
                    return true;
                }
                continue;
            }
            if((surf->flags & SURF_DRAWTILED) || !surf->samples)
            {
                continue;
            }
            hit = Hit{HitKind::Surface, surf, ds, dt};
            return true;
        }

        node = node->children[front >= 0.f];
        start = mid;
    }
    return false;
}

float luma(const glm::vec3& c)
{
    return glm::dot(c, glm::vec3{0.299f, 0.587f, 0.114f});
}

using Cube = glm::vec3[6];

constexpr int MAX_PROBE_STYLES = 8;

// What the rays from a point hit: kept, so that the light is read again at the current light
// styles (flickering and switched lights) without tracing again.
struct Probe
{
    Hit hits[NUM_RAYS];
    bool animated{false}; // a surface hit has a light style other than the normal one
    // The light styles of the surfaces hit (-1: more than MAX_PROBE_STYLES), whose values its shading reads.
    int numStyles{0};
    unsigned char styles[MAX_PROBE_STYLES]{};
};

void traceProbe(const glm::vec3& p, Probe& out)
{
    const Rays& r = rays();
    out.animated = false;
    out.numStyles = 0;
    for(int i = 0; i < NUM_RAYS; i++)
    {
        out.hits[i] = Hit{};
        trace(cl.worldmodel->nodes, r.dir[i], p, p + r.dir[i] * RAY_LENGTH, out.hits[i]);
        if(out.hits[i].kind == HitKind::Surface)
        {
            for(int m = 0; m < MAXLIGHTMAPS && out.hits[i].surf->styles[m] != 255; m++)
            {
                const unsigned char style = out.hits[i].surf->styles[m];
                out.animated |= style != 0;
                if(out.numStyles >= 0 && za::find(out.styles, out.styles + out.numStyles, style) == out.styles + out.numStyles)
                {
                    out.numStyles = out.numStyles < MAX_PROBE_STYLES ? out.numStyles + 1 : -1;
                    if(out.numStyles > 0)
                    {
                        out.styles[out.numStyles - 1] = style;
                    }
                }
            }
        }
    }
}

// The six faces' light from what the rays hit, over their average (1 on average), sharpened by the
// contrast.
void shade(const Probe& probe, Cube& out)
{
    const Rays& r = rays();
    glm::vec3 light[NUM_RAYS];
    glm::vec3 sum{0.f};
    int surfaces = 0;
    for(int i = 0; i < NUM_RAYS; i++)
    {
        const Hit& hit = probe.hits[i];
        light[i] = glm::vec3{0.f};
        if(hit.kind == HitKind::Surface)
        {
            light[i] = lightmapAt(hit.surf, hit.ds, hit.dt);
            sum += light[i];
            surfaces++;
        }
        else if(hit.kind == HitKind::Lava)
        {
            light[i] = glm::vec3{220.f, 100.f, 35.f};
        }
    }

    // The sky is brighter than the walls round it (a neutral half again their average); nothing
    // hit (open space beyond the rays' reach) is as the rest.
    const glm::vec3 mean = surfaces ? sum / static_cast<float>(surfaces) : glm::vec3{96.f};
    for(int i = 0; i < NUM_RAYS; i++)
    {
        if(probe.hits[i].kind == HitKind::Sky)
        {
            light[i] = glm::vec3{za::max(luma(mean), 48.f) * 1.5f};
        }
        else if(probe.hits[i].kind == HitKind::None)
        {
            light[i] = mean;
        }
    }

    // The world's own fill light and lightmap contrast (vr_ambient_light, vr_light_contrast), so that
    // the sides match how bright the walls they face look (qvr::lighting::lightCurve, the same curve).
    const float curve = lighting::lightContrast();
    const float lift = lighting::ambientFloor();
    Cube faces{};
    for(int i = 0; i < NUM_RAYS; i++)
    {
        const glm::vec3 c = 128.f * glm::pow(glm::max(light[i], glm::vec3{0.f}) / 128.f + lift, glm::vec3{curve});
        for(int f = 0; f < 6; f++)
        {
            faces[f] += c * r.weight[i][f];
        }
    }

    // Over the average, by channel (the floor's colour stays the model's; the sides keep their
    // tints), a dark channel not divided by next to nothing; then sharpened (or softened) by
    // vr_model_ambient_contrast, kept within 0.2 .. 2.5 (no side black) and brought back to 1 on
    // average.
    glm::vec3 avg{0.f};
    for(const glm::vec3& f : faces)
    {
        avg += f / 6.f;
    }
    const float top = za::max(avg.r, za::max(avg.g, avg.b));
    const glm::vec3 denom = glm::max(avg, glm::vec3{za::max(top * 0.3f, 1.f)});
    const float contrast = za::clamp(vr_model_ambient_contrast.value, 0.f, 2.f);
    float total = 0.f;
    for(int f = 0; f < 6; f++)
    {
        const glm::vec3 ratio = glm::clamp(faces[f] / denom, glm::vec3{0.05f}, glm::vec3{8.f});
        out[f] = glm::clamp(glm::pow(ratio, glm::vec3{contrast}), glm::vec3{0.2f}, glm::vec3{2.5f});
        total += luma(out[f]);
    }
    const float norm = total > 1e-3f ? 6.f / total : 1.f;
    for(glm::vec3& f : out)
    {
        f *= norm;
    }
}

// A probe's shade() (a pure function of the probe, its light styles' values and the settings it reads), and
// what it was worked out at.
struct ShadeMemo
{
    bool valid{false};
    float lightContrast{0.f}, ambientContrast{0.f}, ambient{0.f}; // qvr::lighting::lightContrast, vr_model_ambient_contrast, light::ambientFloor
    int styles[MAX_PROBE_STYLES]{};
    Cube cube{};
};

struct Cached
{
    glm::vec3 samplePos{0.f};
    Probe previous;    // the last trace but one, faded out over EASE_RATE
    Probe current;
    float blend{1.f};  // from previous (0) to current (1)
    Cube cube{};
    float settings{-1.f}; // the contrast settings the cube was shaded with
    double tracedAt{-1.0};
    int frame{-1};
    ShadeMemo currentShaded, previousShaded; // shade() of current and previous
    // The model's middle the sample point was last found from, and the anchor: the same again find
    // the same point (the world does not move).
    glm::vec3 centerAsked{0.f}, anchorAsked{0.f}, openAt{0.f};
    bool openValid{false};
    int aheadFrame{-1}; // host_framecount it was last worked out ahead (prefetchAdd)
    glm::vec3 originAsked{0.f}; // the entity's origin when centerAsked was (wanted's guess at the point now)
};

// Whether `m` holds shade(probe) as it would be now: the same settings, its light styles at the same values.
[[nodiscard]] bool shadeFresh(const ShadeMemo& m, const Probe& probe)
{
    if(!m.valid || probe.numStyles < 0 || m.lightContrast != lighting::lightContrast() ||
       m.ambientContrast != vr_model_ambient_contrast.value || m.ambient != lighting::ambientFloor())
    {
        return false;
    }
    for(int k = 0; k < probe.numStyles; k++)
    {
        if(d_lightstylevalue[probe.styles[k]] != m.styles[k])
        {
            return false;
        }
    }
    return true;
}

// shade(probe), from the memo when it is fresh.
const Cube& shadeMemo(ShadeMemo& m, const Probe& probe)
{
    if(!shadeFresh(m, probe))
    {
        shade(probe, m.cube);
        m.valid = probe.numStyles >= 0;
        m.lightContrast = lighting::lightContrast();
        m.ambientContrast = vr_model_ambient_contrast.value;
        m.ambient = lighting::ambientFloor();
        for(int k = 0; k < probe.numStyles; k++)
        {
            m.styles[k] = d_lightstylevalue[probe.styles[k]];
        }
    }
    return m.cube;
}

// Whether two probes hit the same: their shade() is the same at the same light styles and settings.
[[nodiscard]] bool sameProbe(const Probe& a, const Probe& b)
{
    if(a.animated != b.animated || a.numStyles != b.numStyles)
    {
        return false;
    }
    for(int i = 0; i < NUM_RAYS; i++)
    {
        const Hit& x = a.hits[i];
        const Hit& y = b.hits[i];
        if(x.kind != y.kind || x.surf != y.surf || x.ds != y.ds || x.dt != y.dt)
        {
            return false;
        }
    }
    for(int k = 0; k < a.numStyles; k++)
    {
        if(a.styles[k] != b.styles[k])
        {
            return false;
        }
    }
    return true;
}

// An entity's probe work for this frame, done ahead on the pool before a batch of models is drawn (ambient::prefetch,
// vr_model_ambient_prefetch): openPoint, traceProbe and shade() are pure functions of their inputs (the world, which
// does not move, the light styles and the settings), so entityCube takes a result from here only when its inputs come
// out the same as the ones it was worked out from, and works it out itself otherwise: the same results either way.
// Its decisions (which entity is due, the budgets, the easing) stay in entityCube, in the draw's order.
constexpr int MAX_AHEAD_SHADES = 3; // the new probe's, the current one's, the previous one's
// A batch's likely traces (wanted's guess) from which it is worked out ahead: a trace is about 10 us, the rest a
// fraction of one; the pool's helpers cost a few us to post and wake 20-40 us later (vr_jobs.hpp), so fewer are
// quicker in the draw (measured: 4 a batch broke even; ROUND22.md "Model light probes ahead"). Most frames trace one or
// two; a map's start, a teleport or a crowd on the move trace a dozen or more.
constexpr int MIN_TRACES_AHEAD = 8;
struct Ahead
{
    const entity_t* e{nullptr};
    bool view{false};
    glm::vec3 center{0.f}, anchor{0.f}; // the inputs: modelCenter, the anchor
    glm::vec3 open{0.f};                // openPoint(center, anchor)
    bool traced{false};                 // whether it was due a trace from `open`
    Probe probe;                        // traceProbe(open)
    double traceSeconds{0.0};
    int numShaded{0};
    Probe shadedProbe[MAX_AHEAD_SHADES]; // shade() of the probes whose memos were stale
    ShadeMemo shaded[MAX_AHEAD_SHADES];
};

const qmodel_t* loadedWorld = nullptr;
int loadedGeneration = -1;
ankerl::unordered_dense::map<const entity_t*, Cached> cache;
za::Vector<Ahead> ahead; // this batch's: the first aheadCount (prefetchAdd .. prefetchEnd; kept, not made again)
za::SizeT aheadCount = 0;
ankerl::unordered_dense::map<const entity_t*, za::SizeT> aheadIndex;
// The entities without a cache entry worked out ahead this frame (those with one: Cached::aheadFrame).
ankerl::unordered_dense::map<const entity_t*, int> aheadSeen;
int aheadSeenFrame = -1;
jobs::Site aheadSite{"model ambient prefetch"};
// vr_model_ambient_show's counts since the last: results taken from ahead, and (vr_model_ambient_prefetch 2) the ones
// that differed from the ones worked out again (none expected).
long long aheadUsed = 0;
long long aheadChecked = 0;
long long aheadDiffered = 0;
long long tracedInDraw = 0; // the traces not taken from ahead
Eviction eviction;
int budgetFrame = -1;
int tracedThisFrame = 0;
int freshThisFrame = 0;
long long samplesTaken = 0; // for vr_model_ambient_show
double sampleSeconds = 0.0;

// The model's middle in the world: its frame's box through the drawn matrix (Quake's models: the
// held weapons' and hands' transforms included), or its origin and middle height.
glm::vec3 modelCenter(const entity_t* e, const float m[16], const aliashdr_t* hdr)
{
    glm::vec3 local{0.f};
    if(hdr && hdr->poseverttype == aliashdr_t::PV_QUAKE1 && hdr->numframes > 0)
    {
        const int frame = e->frame >= 0 && e->frame < hdr->numframes ? e->frame : 0;
        const maliasframedesc_t& f = hdr->frames[frame];
        for(int i = 0; i < 3; i++)
        {
            local[i] = (static_cast<float>(f.bboxmin.v[i]) + static_cast<float>(f.bboxmax.v[i])) * 0.5f;
        }
        return glm::vec3{m[0] * local.x + m[4] * local.y + m[8] * local.z + m[12],
                         m[1] * local.x + m[5] * local.y + m[9] * local.z + m[13],
                         m[2] * local.x + m[6] * local.y + m[10] * local.z + m[14]};
    }
    return glm::vec3{m[12], m[13], m[14] + (e->model->mins[2] + e->model->maxs[2]) * 0.5f};
}

bool solid(const glm::vec3& p)
{
    vec3_t v{p.x, p.y, p.z};
    return Mod_PointInLeaf(v, cl.worldmodel)->contents == CONTENTS_SOLID;
}

// A sample point in the open: rays from inside a wall (a hand pushed into it, a holster at the
// body's edge) would read the rooms behind it. Moved towards `anchor` (the eye for what you hold
// and wear, the origin for the rest) until it is out.
glm::vec3 openPoint(const glm::vec3& p, const glm::vec3& anchor)
{
    if(!solid(p))
    {
        return p;
    }
    for(const float t : {0.25f, 0.5f, 0.75f, 1.f})
    {
        const glm::vec3 q = glm::mix(p, anchor, t);
        if(!solid(q))
        {
            return q;
        }
    }
    return anchor;
}

void setOff(float out[6][4])
{
    for(int f = 0; f < 6; f++)
    {
        out[f][0] = out[f][1] = out[f][2] = 1.f;
        out[f][3] = 0.f;
    }
    out[1][3] = 1.f; // the directional shading kept whole
}

// Staggered between entities, so that they don't all come due in the same frame.
double staggerOf(const entity_t* e)
{
    return static_cast<double>(reinterpret_cast<za::UIntPtrT>(e) % 97) * 0.005;
}

// On the pool (prefetch): what entityCube will likely work out for a.e this frame, from its cached state as it is now
// (only read: the main thread waits for the call).
void workAhead(Ahead& a)
{
    const auto it = cache.find(a.e);
    const Cached* c = it != cache.end() ? &it->second : nullptr;
    a.open = c && c->openValid && c->centerAsked == a.center && c->anchorAsked == a.anchor ? c->openAt
                                                                                           : openPoint(a.center, a.anchor);
    const glm::vec3& p = a.open;
    const bool fresh = !c || c->tracedAt < 0.0;
    const float moved = fresh ? 0.f : glm::distance(p, c->samplePos);
    const bool due = fresh || moved > (a.view ? MOVE_UNITS_VIEW : MOVE_UNITS) ||
                     realtime - c->tracedAt > REFRESH_SECONDS + staggerOf(a.e);
    a.traced = due && (fresh || p != c->samplePos); // (the budget aside: entityCube's)
    a.numShaded = 0;
    const auto shadeAhead = [&a](const Probe& probe) {
        a.shadedProbe[a.numShaded] = probe;
        ShadeMemo& m = a.shaded[a.numShaded++];
        m.valid = false;
        shadeMemo(m, probe);
    };
    if(a.traced)
    {
        const za::Clock clock;
        traceProbe(p, a.probe);
        a.traceSeconds = static_cast<double>(clock.getElapsedTime().asMicroseconds()) / 1e6;
        shadeAhead(a.probe);
    }
    if(c && c->tracedAt >= 0.0)
    {
        const bool teleport = fresh || moved > 128.f;
        // The current probe: still current, or the previous one from this trace on.
        if((!a.traced || (!teleport && c->blend >= 0.5f)) && !shadeFresh(c->currentShaded, c->current))
        {
            shadeAhead(c->current);
        }
        if((a.traced ? !teleport && c->blend < 0.5f : c->blend < 1.f) && !shadeFresh(c->previousShaded, c->previous))
        {
            shadeAhead(c->previous);
        }
    }
}

// This frame's work ahead for e (null: none).
const Ahead* aheadFor(const entity_t* e)
{
    if(aheadCount == 0)
    {
        return nullptr;
    }
    const auto it = aheadIndex.find(e);
    return it != aheadIndex.end() && it->second < aheadCount ? &ahead[it->second] : nullptr;
}

[[nodiscard]] bool checkAhead() { return vr_model_ambient_prefetch.value >= 2.f; }

void noteAhead(bool same)
{
    aheadUsed++;
    if(checkAhead())
    {
        aheadChecked++;
        aheadDiffered += same ? 0 : 1;
    }
}

// openPoint(center, anchor), from the work ahead when it was worked out from the same.
glm::vec3 openPointAhead(const glm::vec3& center, const glm::vec3& anchor, const Ahead* a)
{
    if(a && a->center == center && a->anchor == anchor)
    {
        noteAhead(!checkAhead() || openPoint(center, anchor) == a->open);
        return a->open;
    }
    return openPoint(center, anchor);
}

// traceProbe(p, out), from the work ahead when it traced from the same point; the seconds it took.
double traceProbeAhead(const glm::vec3& p, Probe& out, const Ahead* a)
{
    if(a && a->traced && a->open == p)
    {
        out = a->probe;
        if(checkAhead())
        {
            Probe again;
            traceProbe(p, again);
            noteAhead(sameProbe(again, out));
        }
        else
        {
            noteAhead(true);
        }
        return a->traceSeconds;
    }
    tracedInDraw++;
    const za::Clock clock;
    traceProbe(p, out);
    return static_cast<double>(clock.getElapsedTime().asMicroseconds()) / 1e6;
}

// shadeMemo(m, probe), its shading taken from the work ahead when that was of the same probe and is fresh.
const Cube& shadeMemoAhead(ShadeMemo& m, const Probe& probe, const Ahead* a)
{
    if(a && !shadeFresh(m, probe))
    {
        for(int k = 0; k < a->numShaded; k++)
        {
            if(shadeFresh(a->shaded[k], probe) && sameProbe(a->shadedProbe[k], probe))
            {
                m = a->shaded[k];
                bool same = true;
                if(checkAhead())
                {
                    Cube again{};
                    shade(probe, again);
                    for(int f = 0; f < 6; f++)
                    {
                        same = same && again[f] == m.cube[f];
                    }
                }
                noteAhead(same);
                return m.cube;
            }
        }
    }
    return shadeMemo(m, probe);
}

} // namespace

namespace
{

// vr_model_ambient_show [1]: the cubes of the models nearest the view (1: not the held ones or the body; tuning).
void show_f()
{
    struct Near
    {
        float dist;
        const entity_t* e;
        const Cached* c;
    };
    za::Vector<Near> nearest;
    const glm::vec3 eye{r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]};
    for(const auto& [e, c] : cache)
    {
        if(c.tracedAt >= 0.0 && c.frame >= host_framecount - 2 && (Cmd_Argc() < 2 || !VR_IsViewEntity(e)))
        {
            nearest.pushBack({glm::distance(eye, c.samplePos), e, &c});
        }
    }
    za::quickSort(nearest.begin(), nearest.end(), [](const Near& a, const Near& b) { return a.dist < b.dist; });
    Con_Printf("AMB %lld samples, %.1f us each (%d rays), %d entities cached\n", samplesTaken,
               samplesTaken ? sampleSeconds * 1e6 / static_cast<double>(samplesTaken) : 0.0, NUM_RAYS,
               static_cast<int>(cache.size()));
    Con_Printf("AMB prefetch (vr_model_ambient_prefetch %g): %lld results taken from it, %lld checked, %lld differed; "
               "%lld traced in the draw\n",
        static_cast<double>(vr_model_ambient_prefetch.value), aheadUsed, aheadChecked, aheadDiffered, tracedInDraw);
    aheadUsed = aheadChecked = aheadDiffered = tracedInDraw = 0;
    static constexpr const char* names[6] = {"+X", "-X", "+Y", "-Y", "+Z", "-Z"};
    for(size_t i = 0; i < nearest.size() && i < 6; i++)
    {
        const Cached& c = *nearest[i].c;
        Con_Printf("AMB %s at %.0f %.0f %.0f (%.0f away):", nearest[i].e->model ? nearest[i].e->model->name : "?", c.samplePos.x,
                   c.samplePos.y, c.samplePos.z, nearest[i].dist);
        for(int f = 0; f < 6; f++)
        {
            Con_Printf(" %s %.2f %.2f %.2f", names[f], c.cube[f].r, c.cube[f].g, c.cube[f].b);
        }
        Con_Printf("\n");
    }
}

} // namespace

namespace
{
bool showCommandRegistered = false; // (vr_model_ambient_show: registered on the first call)
} // namespace

void ambient::entityCube(const entity_t* e, const float modelMatrix[16], const void* aliashdr, bool enabled,
                         float out[6][4])
{
    setOff(out);
    if(!showCommandRegistered) // a tuning command (no init hook of its own)
    {
        showCommandRegistered = true;
        Cmd_AddCommand("vr_model_ambient_show", show_f);
    }
    if(!enabled || vr_model_ambient_dir.value == 0.f || !cl.worldmodel || !cl.worldmodel->lightdata || !e ||
       !e->model || (e->model->flags & MOD_FBRIGHTHACK))
    {
        return;
    }
    if(cl.worldmodel != loadedWorld || worldGeneration() != loadedGeneration) // the same model can hold another map: a map name loaded again (vr_relit_maps switched)
    {
        cache.clear();
        eviction = {};
        loadedWorld = cl.worldmodel;
        loadedGeneration = worldGeneration();
    }
    if(budgetFrame != host_framecount)
    {
        budgetFrame = host_framecount;
        tracedThisFrame = 0;
        freshThisFrame = 0;
    }

    Cached* entry = &cache[e];
    if(entry->frame != host_framecount)
    {
        Cached& c = *entry;
        QVR_PROFILE("model ambient");
        const Ahead* a = aheadFor(e);
        const bool view = e == &cl.viewent || VR_IsViewEntity(e);
        const glm::vec3 anchor = view ? glm::vec3{r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]}
                                      : glm::vec3{e->origin[0], e->origin[1], e->origin[2] + 8.f};
        const glm::vec3 center = modelCenter(e, modelMatrix, static_cast<const aliashdr_t*>(aliashdr));
        c.originAsked = glm::vec3{e->origin[0], e->origin[1], e->origin[2]};
        if(!c.openValid || center != c.centerAsked || anchor != c.anchorAsked)
        {
            c.centerAsked = center;
            c.anchorAsked = anchor;
            c.openAt = openPointAhead(center, anchor, a);
            c.openValid = true;
        }
        const glm::vec3 p = c.openAt;
        const bool fresh = c.tracedAt < 0.0;
        const float moved = fresh ? 0.f : glm::distance(p, c.samplePos);
        const double stagger = staggerOf(e);
        const bool due = fresh || moved > (view ? MOVE_UNITS_VIEW : MOVE_UNITS) ||
                         realtime - c.tracedAt > REFRESH_SECONDS + stagger;
        const bool allowed = fresh ? freshThisFrame < MAX_FRESH_PER_FRAME
                                   : view || tracedThisFrame < MAX_TRACED_PER_FRAME;
        bool reshade = false;
        if(due && !fresh && p == c.samplePos)
        {
            // Due by time alone, from the same point: the rays (the world's, which does not move) would hit
            // the same surfaces again, and their light is read as it is now anyway.
            c.tracedAt = realtime;
        }
        else if(due && allowed)
        {
            double seconds = 0.0;
            if(fresh || moved > 128.f) // new, or teleported: no easing from where it was
            {
                seconds = traceProbeAhead(p, c.current, a);
                c.currentShaded.valid = false;
                c.blend = 1.f;
            }
            else
            {
                if(c.blend >= 0.5f) // mid-way the older of the two goes
                {
                    c.previous = c.current;
                    c.previousShaded = c.currentShaded;
                }
                seconds = traceProbeAhead(p, c.current, a);
                c.currentShaded.valid = false;
                c.blend = 0.f;
            }
            sampleSeconds += seconds;
            samplesTaken++;
            c.samplePos = p;
            c.tracedAt = realtime;
            (fresh ? freshThisFrame : tracedThisFrame)++;
            reshade = true;
        }
        if(c.tracedAt >= 0.0)
        {
            if(c.blend < 1.f)
            {
                c.blend = 1.f - (1.f - c.blend) * za::exp(-static_cast<float>(host_frametime) * EASE_RATE);
                c.blend = c.blend > 0.995f ? 1.f : c.blend;
                reshade = true;
            }
            // Flickering or switched lights: read every frame, in step with the world's.
            const float settings = lighting::ambientFloor() * 4096.f + vr_model_ambient_contrast.value * 16.f +
                                   lighting::lightContrast(); // the settings shade() reads, as one number
            // (Each probe's shading is kept while its light styles hold still: easing only mixes them.)
            if(reshade || (c.current.animated && !shadeFresh(c.currentShaded, c.current)) ||
               (c.blend < 1.f && c.previous.animated) || settings != c.settings)
            {
                c.settings = settings;
                const Cube& now = shadeMemoAhead(c.currentShaded, c.current, a);
                za::copy(now, now + 6, c.cube); // (a Cube's six faces)
                if(c.blend < 1.f)
                {
                    const Cube& older = shadeMemoAhead(c.previousShaded, c.previous, a);
                    for(int f = 0; f < 6; f++)
                    {
                        c.cube[f] = glm::mix(older[f], c.cube[f], c.blend);
                    }
                }
            }
        }
        c.frame = host_framecount;

        // Evict entities not drawn for a while (temporary entities come and go): at most once a frame (vr_evict.hpp).
        // `e` is still there (the eviction leaves the entities drawn this frame), but maybe not where it was: a dense
        // map's erase moves its last value into the erased slot.
        const za::SizeT before = cache.size();
        evictStale(cache, 2048, 100, host_framecount, eviction);
        if(cache.size() != before)
        {
            entry = &cache.find(e)->second;
        }
    }

    const Cached& c = *entry;
    if(c.tracedAt < 0.0)
    {
        return; // not traced yet (over this frame's budget): shaded as before
    }
    for(int f = 0; f < 6; f++)
    {
        out[f][0] = c.cube[f].r;
        out[f][1] = c.cube[f].g;
        out[f][2] = c.cube[f].b;
    }
    out[0][3] = 1.f;
    out[1][3] = 1.f - DIR_REPLACED;
}


extern "C" void VR_AliasAmbient(const entity_t* e, const float matrix[16], const void* aliashdr, int enabled,
                                float out[24])
{
    ambient::entityCube(e, matrix, aliashdr, enabled != 0, reinterpret_cast<float(*)[4]>(out));
}

bool ambient::prefetchOn()
{
    return vr_model_ambient_prefetch.value > 0.f && vr_model_ambient_dir.value != 0.f && cl.worldmodel &&
           cl.worldmodel->lightdata && cl.worldmodel == loadedWorld && worldGeneration() == loadedGeneration;
}

int ambient::wanted(const entity_t* e)
{
    if(!e || !e->model || (e->model->flags & MOD_FBRIGHTHACK))
    {
        return 0;
    }
    // Not again for a later batch this frame (worked out ahead for an earlier one, its draw culled it or let it be).
    const auto it = cache.find(e);
    if(it == cache.end())
    {
        if(aheadSeenFrame != host_framecount)
        {
            aheadSeen.clear();
            aheadSeenFrame = host_framecount;
        }
        return aheadSeen.find(e) == aheadSeen.end() ? 2 : 0; // (new: traced)
    }
    const Cached& c = it->second;
    if(c.frame == host_framecount || c.aheadFrame == host_framecount)
    {
        return 0;
    }
    if(c.tracedAt < 0.0 || !c.openValid)
    {
        return 2;
    }
    // Likely due a trace (a guess, without its matrix: the point last found moved as the entity's origin did since).
    const bool view = e == &cl.viewent || VR_IsViewEntity(e);
    const glm::vec3 p = c.openAt + glm::vec3{e->origin[0], e->origin[1], e->origin[2]} - c.originAsked;
    return glm::distance(p, c.samplePos) > (view ? MOVE_UNITS_VIEW : MOVE_UNITS) ||
                   realtime - c.tracedAt > REFRESH_SECONDS + staggerOf(e)
               ? 2
               : 1;
}

bool ambient::prefetchWorth(int likelyTraces)
{
    return likelyTraces >= MIN_TRACES_AHEAD;
}

void ambient::prefetchAdd(const entity_t* e, const float modelMatrix[16], const void* aliashdr)
{
    if(aheadCount == ahead.size())
    {
        ahead.emplaceBack();
    }
    const za::SizeT index = aheadCount++;
    Ahead& a = ahead[index]; // (its probes written only where used)
    a.e = e;
    a.view = e == &cl.viewent || VR_IsViewEntity(e);
    a.anchor = a.view ? glm::vec3{r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]}
                      : glm::vec3{e->origin[0], e->origin[1], e->origin[2] + 8.f};
    a.center = modelCenter(e, modelMatrix, static_cast<const aliashdr_t*>(aliashdr));
    a.traced = false;
    a.numShaded = 0;
    aheadIndex[e] = index;
    const auto it = cache.find(e);
    if(it == cache.end())
    {
        aheadSeen[e] = 1;
    }
    else
    {
        it->second.aheadFrame = host_framecount;
    }
}

void ambient::prefetchRun()
{
    if(aheadCount == 0)
    {
        return;
    }
    QVR_PROFILE("model ambient prefetch");
    jobs::parallelFor(aheadSite, aheadCount, 1, [](za::SizeT begin, za::SizeT end) {
        for(za::SizeT i = begin; i < end; i++)
        {
            workAhead(ahead[i]);
        }
    });
}

void ambient::prefetchEnd()
{
    aheadCount = 0;
    aheadIndex.clear();
}

extern "C" int VR_AliasAmbientPrefetchOn(void)
{
    return ambient::prefetchOn() ? 1 : 0;
}

extern "C" int VR_AliasAmbientWanted(const entity_t* e)
{
    return ambient::wanted(e);
}

extern "C" int VR_AliasAmbientPrefetchWorth(int likelyTraces)
{
    return ambient::prefetchWorth(likelyTraces) ? 1 : 0;
}

extern "C" void VR_AliasAmbientPrefetchAdd(const entity_t* e, const float matrix[16], const void* aliashdr)
{
    ambient::prefetchAdd(e, matrix, aliashdr);
}

extern "C" void VR_AliasAmbientPrefetchRun(void)
{
    ambient::prefetchRun();
}

extern "C" void VR_AliasAmbientPrefetchEnd(void)
{
    ambient::prefetchEnd();
}

void ambient::onGameDirChanged()
{
    prefetchEnd();
    cache.clear();
    eviction = {};
    loadedWorld = nullptr;
    loadedGeneration = -1;
}
