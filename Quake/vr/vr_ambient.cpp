// vr_ambient.cpp -- see vr_ambient.hpp.

#include "vr_ambient.hpp"
#include "vr_geom.hpp"
#include "vr_main.hpp"
#include "vr_cvars.hpp"
#include "vr_diskcache.hpp"
#include "vr_evict.hpp"
#include <string.h>
#include "vr_jobs.hpp"
#include "vr_lighting.hpp"
#include "vr_lines.hpp"
#include "vr_profile.hpp"
#include "vr_testkit.hpp"
#include "vr_trace.hpp"

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
    Lava,
    Liquid // (a ray told to stop at slime or a teleporter: trace's stopAt)
};

struct Hit
{
    HitKind kind{HitKind::None};
    const msurface_t* surf{nullptr};
    int ds{0};
    int dt{0};
};

// One light style's layer of a surface's lightmap at a point: coloured, bilinear between the luxels, 0..255, before the
// style's value.
glm::vec3 lightmapLayer(const msurface_t* surf, int ds, int dt, int layer)
{
    const int smax = (surf->extents[0] >> 4) + 1;
    const int tmax = (surf->extents[1] >> 4) + 1;
    const int s0 = za::min(ds >> 4, smax - 1), t0 = za::min(dt >> 4, tmax - 1);
    const int s1 = za::min(s0 + 1, smax - 1), t1 = za::min(t0 + 1, tmax - 1);
    const float fs = static_cast<float>(ds & 15) / 16.f, ft = static_cast<float>(dt & 15) / 16.f;
    const byte* lightmap = surf->samples + static_cast<za::SizeT>(layer) * smax * tmax * 3;
    const auto at = [&](int s, int t) {
        const byte* p = lightmap + (t * smax + s) * 3;
        return glm::vec3{p[0], p[1], p[2]};
    };
    return glm::mix(glm::mix(at(s0, t0), at(s1, t0), fs), glm::mix(at(s0, t1), at(s1, t1), fs), ft);
}

// The baked light at a point of a surface (InterpolateLightmap, gl_rlight.c): every light style at
// its current value. 0..255 (128: Quake's full light).
glm::vec3 lightmapAt(const msurface_t* surf, int ds, int dt)
{
    glm::vec3 c{0.f};
    for(int maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++)
    {
        c += lightmapLayer(surf, ds, dt, maps) * (static_cast<float>(d_lightstylevalue[surf->styles[maps]]) / 256.f);
    }
    return c;
}

// RecursiveLightPoint (gl_rlight.c) along any ray: the first world surface the ray crosses that
// faces it, where the crossing is on the surface (its texture's extents are only the box round it: a thin diagonal band's
// box is half a room, of luxels outside it that are dark). Liquids other than `stopAt`'s are seen through (the floor under
// them is lit); surfaces without a lightmap are passed.
bool trace(const mnode_t* node, const glm::vec3& dir, glm::vec3 start, const glm::vec3& end, Hit& hit, int stopAt = SURF_DRAWLAVA)
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
        if(trace(node->children[front < 0.f], dir, start, mid, hit, stopAt))
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
            if(ds > surf->extents[0] || dt > surf->extents[1] || !R_SurfaceHoldsPoint(surf, &mid.x))
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
                if(surf->flags & stopAt)
                {
                    hit.kind = (surf->flags & SURF_DRAWLAVA) ? HitKind::Lava : HitKind::Liquid;
                    hit.surf = surf;
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

// The six faces' drawn light over their average, by channel (the sides keep their tints), a dark channel not divided by
// next to nothing; then sharpened (or softened) by vr_model_ambient_contrast, kept within 0.2 .. 2.5 (no side black) and
// brought back to 1 on average.
void ratiosFromFaces(const Cube& faces, Cube& out)
{
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
    for(int f = 0; f < 6; f++)
    {
        out[f] *= norm;
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

    ratiosFromFaces(faces, out);
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

const qmodel_t* loadedWorld = nullptr;
int loadedGeneration = -1;
ankerl::unordered_dense::map<const entity_t*, Cached> cache;
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

// ------------------------------------------------------------------------------------------------
// The light grid (vr_model_light_grid; the models' brightness and the cube): probes in the open, every
// vr_model_light_grid_cell units
// over the map, each gathering the same rays as above at the map's load (the light of every surface round it, whatever
// its side) and keeping one set of values per light style, so that flickering and switched lights still move on models.
// A model reads the eight probes round it, each frame, with no rays: blended by distance, so nothing pops; a probe in a
// wall, or one the model can't see (the next room's, through a thin wall), left out and the rest weighted up. What's drawn
// beside a model reads the same probes (ambient::lightPoint; the particles, gridLightOf). Lit liquids: below, "GLOW".
// The knobs, tuning constants (tune; Debug > Tunables > Graphics > Models and Effects > Light Grid): vr_model_light_grid_intensity, _mix, the liquids' strengths and
// ranges are read at each read; vr_model_light_grid_near is in the probes' values and vr_model_light_grid_cell is their
// spacing: a change of either builds the grid again. Show Light Probes (vr_model_light_grid_show): gridShow, below.

constexpr za::SizeT GRID_MAX_CELLS = 32u * 1024u * 1024u; // a larger map's cells are made twice the size until it fits

// The rays are gathered as a power mean with the default vr_light_contrast's power, not plain averages: the world is
// drawn through that curve, and one lit wall in view of a dark room lights a model as it looks, not as its luxels add
// up. Uniform surroundings come out exactly as their light.
constexpr float GRID_POWER = 2.5f;

constexpr unsigned GRID_CACHE_VERSION = 3u;         // a build's: bumped when gridProbe's values change
constexpr double GRID_CACHE_SECONDS = 0.1;          // a grid that took this long is kept on disk (the smaller read no faster)
constexpr float GRID_AIR = 256.f;                  // a probe has a surface within this straight below it
constexpr float GRID_FIXED = 128.f;                 // a GridEntry's values over Quake's lightmap units
constexpr float GRID_MASK_MOVE = 4.f;               // the probes in sight looked at again when it moved this far
constexpr float GRID_TELEPORT = 128.f;              // moved this far in a frame: no easing from where it was

// One light style's light at a probe, at the style's value 256.
struct GridEntry
{
    unsigned short style;       // or GRID_GLOW: a lit liquid's light, after the styles' (gridGlow)
    unsigned short level[3];    // what lights the model: the surfaces' light (the sky and lava as the rest; see gridProbe)
    unsigned short faces[6][3]; // the cube's six cosine lobes (as shade()'s)
};

// A cell (its probe, at its low corner): GRID_CLEAR, the entry count less one, the first entry.
constexpr unsigned GRID_CLEAR = 1u << 31; // its eight corners are probes that all see each other: no lines to look
constexpr unsigned GRID_COUNT_SHIFT = 27;
constexpr unsigned GRID_COUNT_MASK = 15u; // (MAX_PROBE_STYLES styles and a liquid of each kind)
constexpr unsigned GRID_FIRST = (1u << GRID_COUNT_SHIFT) - 1u;

// Lit liquids: what lights a model besides the baked light. Lava, slime and teleporters glow and no lightmap says so
// (water doesn't; a sky-lit room's surfaces already have the sky's light, and the sky is everywhere outdoors: neither
// counts). Each kind's entries are worked out with no strength and read with vr_model_light_<kind> and its _range, so
// both are live: `level` and `faces` are the liquid's colour by the square root of how much of the probe's view it fills
// (all of it: the colour; a bit of a pool past a shore's edge still lights a model, and over the pool it doesn't burn),
// and the style field holds the mean distance to it.
constexpr unsigned short GRID_GLOW = 0x8000u;
constexpr int GRID_GLOW_KIND_SHIFT = 12;
constexpr unsigned short GRID_GLOW_DISTANCE = 0x0FFFu;
constexpr int GLOW_KINDS = 3;
constexpr int GLOW_STOP = SURF_DRAWLAVA | SURF_DRAWSLIME | SURF_DRAWTELE;
constexpr float GLOW_BRIGHTEST = 220.f;              // a liquid's colour: its texture's mean, its top channel this bright
constexpr float GLOW_GAIN = 1.5f;                    // strength 1: a model on a pool's shore (a twentieth of its view) gets some 50, over the pool some 190
constexpr glm::vec3 GLOW_COLOURS[GLOW_KINDS] = {{220.f, 100.f, 35.f}, {90.f, 220.f, 60.f}, {140.f, 90.f, 220.f}}; // (no texture to read)
constexpr float GLOW_REACH = 768.f;                  // the farthest a liquid counts (the range sliders' top)
constexpr int GLOW_RAYS = 128;                       // a pool beside a model takes more rays to find than the 26
constexpr int GLOW_BLOCK = 4;                        // cells: a part of the grid with no liquid in reach traces none of them

struct GlowKnob
{
    const Tunable* strength;
    const Tunable* range;
};
const GlowKnob GLOW_KNOBS[GLOW_KINDS] = {{&vr_model_light_lava, &vr_model_light_lava_range},
                                         {&vr_model_light_slime, &vr_model_light_slime_range},
                                         {&vr_model_light_teleport, &vr_model_light_teleport_range}};

struct GlowRayTable
{
    glm::vec3 dir[GLOW_RAYS];
    float weight[GLOW_RAYS][6]; // as Rays'
};

// A spiral over the sphere. Built before main (no first-call guard): read by any thread.
const GlowRayTable glowRays = [] {
    GlowRayTable r{};
    constexpr float GOLDEN_ANGLE = 2.39996323f;
    for(int i = 0; i < GLOW_RAYS; i++)
    {
        const float z = 1.f - (2.f * static_cast<float>(i) + 1.f) / static_cast<float>(GLOW_RAYS);
        const float across = glm::sqrt(za::max(0.f, 1.f - z * z));
        const float turn = GOLDEN_ANGLE * static_cast<float>(i);
        r.dir[i] = glm::vec3{glm::cos(turn) * across, glm::sin(turn) * across, z};
    }
    for(int f = 0; f < 6; f++)
    {
        float sum = 0.f;
        for(int i = 0; i < GLOW_RAYS; i++)
        {
            r.weight[i][f] = za::max(0.f, glm::dot(r.dir[i], AXES[f]));
            sum += r.weight[i][f];
        }
        for(int i = 0; i < GLOW_RAYS; i++)
        {
            r.weight[i][f] /= sum;
        }
    }
    return r;
}();

struct Grid
{
    const qmodel_t* world{nullptr};
    int generation{-1};
    glm::vec3 origin{0.f};
    float cell{48.f};                                                    // what it was built with (doubled for a huge map)
    float cellSetting{48.f};                                             // vr_model_light_grid_cell when it was built
    int size[3]{0, 0, 0};
    za::Vector<unsigned> cells;
    za::Vector<GridEntry> entries;
    za::SizeT probes{0}, clearCells{0}, styledProbes{0};
    double buildMs{0.0};
    float nearWeight{64.f};                                              // vr_model_light_grid_near when it was built
    ankerl::unordered_dense::map<const texture_t*, glm::vec3> glowColours; // the map's lit liquids' textures
    za::Vector<unsigned char> glowBlocks;                            // 1: a liquid within reach of the block (GLOW_BLOCK)
    int blocks[3]{0, 0, 0};
    unsigned long long glowKey{0};                                   // the colours, for the disk cache's key
    unsigned long long builtKey{0};                                  // the disk cache's key of what cells/entries hold (0: unknown)
    za::SizeT glowProbes[GLOW_KINDS]{};

    [[nodiscard]] za::SizeT index(int x, int y, int z) const
    {
        return (static_cast<za::SizeT>(z) * size[1] + y) * size[0] + x;
    }
    [[nodiscard]] glm::vec3 point(int x, int y, int z) const
    {
        return origin + glm::vec3{x, y, z} * cell;
    }
};

Grid grid;
jobs::Site gridProbesSite{"model light grid probes"}; // (its parallelFor: vr_jobs_sites)
jobs::Site gridClearSite{"model light grid cells"};

[[nodiscard]] unsigned short gridFixed(float v)
{
    return static_cast<unsigned short>(za::clamp(v * GRID_FIXED + 0.5f, 0.f, 65535.f));
}

// How far along a ray from `p` the surface it hit is.
[[nodiscard]] float distanceTo(const msurface_t* surf, const glm::vec3& p, const glm::vec3& dir, float reach)
{
    const mplane_t* plane = surf->plane;
    const glm::vec3 n{plane->normal[0], plane->normal[1], plane->normal[2]};
    const float across = glm::dot(n, dir);
    return glm::abs(across) > 1e-4f ? za::clamp((plane->dist - glm::dot(n, p)) / across, 0.f, reach) : reach;
}

[[nodiscard]] int glowKindOf(int surfFlags)
{
    return (surfFlags & SURF_DRAWLAVA) ? 0 : (surfFlags & SURF_DRAWSLIME) ? 1 : 2;
}

[[nodiscard]] const texture_t* surfaceTexture(const msurface_t* surf)
{
    return cl.worldmodel->textures[surf->texinfo->texnum];
}

// A lit liquid's colour: its texture's mean, its brighter texels counting more (what glows; the dark cracks don't), as
// bright as GLOW_BRIGHTEST (a map's green lava lights green).
[[nodiscard]] glm::vec3 glowColourOf(const texture_t* tx, int kind)
{
    const size_t pixels = tx ? static_cast<size_t>(tx->width) * static_cast<size_t>(tx->height) : 0u;
    if(pixels == 0u || cl.worldmodel->bspversion == BSPVERSION_QUAKE64)
    {
        return GLOW_COLOURS[kind];
    }
    const byte* in = reinterpret_cast<const byte*>(tx + 1);
    glm::vec3 sum{0.f};
    for(size_t i = 0; i < pixels; i++)
    {
        const unsigned rgba = d_8to24table[in[i]];
        const glm::vec3 texel{static_cast<float>(rgba & 255u), static_cast<float>((rgba >> 8) & 255u), static_cast<float>((rgba >> 16) & 255u)};
        const float top = za::max(texel.r, za::max(texel.g, texel.b));
        sum += texel * top * top;
    }
    const float top = za::max(sum.r, za::max(sum.g, sum.b));
    return top > 0.f ? sum * (GLOW_BRIGHTEST / top) : GLOW_COLOURS[kind];
}

// Before the probes: the lit liquids' colours, and which blocks of the grid one can reach.
void glowPrepare()
{
    const qmodel_t* world = cl.worldmodel;
    grid.glowColours.clear();
    grid.glowKey = diskcache::fnvStart;
    za::SizeT total = 1;
    for(int i = 0; i < 3; i++)
    {
        grid.blocks[i] = grid.size[i] / GLOW_BLOCK + 1;
        total *= static_cast<za::SizeT>(grid.blocks[i]);
    }
    grid.glowBlocks.clear();
    grid.glowBlocks.resize(total, static_cast<unsigned char>(0));
    const float block = grid.cell * static_cast<float>(GLOW_BLOCK);
    for(int s = 0; s < world->numsurfaces; s++)
    {
        const msurface_t& surf = world->surfaces[s];
        if(!(surf.flags & SURF_DRAWTURB) || !(surf.flags & GLOW_STOP))
        {
            continue;
        }
        const texture_t* tx = surfaceTexture(&surf);
        if(!grid.glowColours.contains(tx))
        {
            const glm::vec3 colour = glowColourOf(tx, glowKindOf(surf.flags));
            grid.glowColours[tx] = colour;
            grid.glowKey = diskcache::fnv(grid.glowKey, &colour, sizeof(colour));
        }
        int lo[3], hi[3];
        for(int i = 0; i < 3; i++)
        {
            lo[i] = za::clamp(static_cast<int>((surf.mins[i] - GLOW_REACH - grid.origin[i]) / block), 0, grid.blocks[i] - 1);
            hi[i] = za::clamp(static_cast<int>((surf.maxs[i] + GLOW_REACH - grid.origin[i]) / block), 0, grid.blocks[i] - 1);
        }
        for(int z = lo[2]; z <= hi[2]; z++)
        {
            for(int y = lo[1]; y <= hi[1]; y++)
            {
                for(int x = lo[0]; x <= hi[0]; x++)
                {
                    grid.glowBlocks[(static_cast<za::SizeT>(z) * grid.blocks[1] + y) * grid.blocks[0] + x] = 1;
                }
            }
        }
    }
}

// A probe's lit liquids: an entry for each kind its rays reach (walls stop them, as any ray). Water is seen through.
void gridGlow(const glm::vec3& p, int x, int y, int z, za::Vector<GridEntry>& out, int& entries)
{
    if(grid.glowColours.empty() ||
       !grid.glowBlocks[(static_cast<za::SizeT>(z / GLOW_BLOCK) * grid.blocks[1] + y / GLOW_BLOCK) * grid.blocks[0] + x / GLOW_BLOCK])
    {
        return;
    }
    struct Sum
    {
        glm::vec3 level{0.f};
        glm::vec3 faces[6]{};
        float distance{0.f};
        int rays{0};
    };
    Sum sums[GLOW_KINDS];
    for(int i = 0; i < GLOW_RAYS; i++)
    {
        const glm::vec3& dir = glowRays.dir[i];
        Hit hit;
        trace(cl.worldmodel->nodes, dir, p, p + dir * GLOW_REACH, hit, GLOW_STOP);
        if((hit.kind != HitKind::Lava && hit.kind != HitKind::Liquid) || !hit.surf)
        {
            continue;
        }
        Sum& sum = sums[glowKindOf(hit.surf->flags)];
        const auto colour = grid.glowColours.find(surfaceTexture(hit.surf));
        const glm::vec3 c = colour != grid.glowColours.end() ? colour->second : GLOW_COLOURS[glowKindOf(hit.surf->flags)];
        sum.level += c;
        for(int f = 0; f < 6; f++)
        {
            sum.faces[f] += c * glowRays.weight[i][f];
        }
        sum.distance += distanceTo(hit.surf, p, dir, GLOW_REACH);
        sum.rays++;
    }
    for(int k = 0; k < GLOW_KINDS; k++)
    {
        const Sum& sum = sums[k];
        if(sum.rays == 0)
        {
            continue;
        }
        GridEntry entry{};
        const int distance = za::min(static_cast<int>(sum.distance / static_cast<float>(sum.rays) + 0.5f), static_cast<int>(GRID_GLOW_DISTANCE));
        entry.style = static_cast<unsigned short>(GRID_GLOW | (k << GRID_GLOW_KIND_SHIFT) | distance);
        const float eased = 1.f / glm::sqrt(static_cast<float>(sum.rays) / static_cast<float>(GLOW_RAYS)); // (the square root's, on the sum)
        for(int c = 0; c < 3; c++)
        {
            entry.level[c] = gridFixed(sum.level[c] * eased / static_cast<float>(GLOW_RAYS));
            for(int f = 0; f < 6; f++)
            {
                entry.faces[f][c] = gridFixed(sum.faces[f][c] * eased);
            }
        }
        out.pushBack(entry);
        entries++;
    }
}

// One probe: the entries of the styles of the surfaces its rays hit (the first MAX_PROBE_STYLES, later ones as the
// first), then its lit liquids' (gridGlow). The sky is brighter than the walls round it, on the cube as in shade(), but
// it isn't baked light: what lights the model (`level`) takes it as the rest of the surfaces, as it does the rays that
// hit nothing or lava (whose light is its own entry).
void gridProbe(const glm::vec3& p, int x, int y, int z, za::Vector<GridEntry>& out, unsigned& cellValue)
{
    const Rays& r = rays();
    Hit hits[NUM_RAYS];
    unsigned char styles[MAX_PROBE_STYLES];
    glm::vec3 sum[MAX_PROBE_STYLES];
    int numStyles = 0;
    int surfaces = 0;
    const auto styleIndex = [&](unsigned char style) {
        const unsigned char* const found = za::find(styles, styles + numStyles, style);
        if(found != styles + numStyles)
        {
            return static_cast<int>(found - styles);
        }
        if(numStyles == MAX_PROBE_STYLES)
        {
            return 0;
        }
        styles[numStyles] = style;
        sum[numStyles] = glm::vec3{0.f};
        return numStyles++;
    };

    glm::vec3 layers[NUM_RAYS][MAXLIGHTMAPS];
    int layerStyle[NUM_RAYS][MAXLIGHTMAPS];
    for(int i = 0; i < NUM_RAYS; i++)
    {
        trace(cl.worldmodel->nodes, r.dir[i], p, p + r.dir[i] * RAY_LENGTH, hits[i]);
        if(hits[i].kind != HitKind::Surface)
        {
            continue;
        }
        surfaces++;
        const msurface_t* surf = hits[i].surf;
        for(int m = 0; m < MAXLIGHTMAPS; m++)
        {
            layerStyle[i][m] = -1;
            if(surf->styles[m] != 255)
            {
                layerStyle[i][m] = styleIndex(surf->styles[m]);
                layers[i][m] = lightmapLayer(surf, hits[i].ds, hits[i].dt, m);
                sum[layerStyle[i][m]] += layers[i][m];
            }
        }
    }
    if(numStyles == 0)
    {
        styles[0] = 0;
        sum[0] = glm::vec3{0.f};
        numStyles = 1;
    }

    const auto gathered = [](const glm::vec3& c) { return glm::pow(glm::max(c, glm::vec3{0.f}) / 128.f, glm::vec3{GRID_POWER}); };
    const auto spread = [](const glm::vec3& c) { return glm::pow(c, glm::vec3{1.f / GRID_POWER}) * 128.f; };

    const unsigned first = static_cast<unsigned>(out.size());
    for(int s = 0; s < numStyles; s++)
    {
        const glm::vec3 mean = surfaces ? sum[s] / static_cast<float>(surfaces) : glm::vec3{s == 0 ? 96.f : 0.f};
        const float meanLuma = luma(mean);
        const glm::vec3 sky{(s == 0 ? za::max(meanLuma, 48.f) : meanLuma) * 1.5f};
        glm::vec3 faces[6]{};
        glm::vec3 level{0.f};
        float levelWeight = 0.f;
        for(int i = 0; i < NUM_RAYS; i++)
        {
            glm::vec3 surface{0.f};
            glm::vec3 lobes = mean;
            if(hits[i].kind == HitKind::Surface)
            {
                for(int m = 0; m < MAXLIGHTMAPS; m++)
                {
                    if(layerStyle[i][m] == s)
                    {
                        surface += layers[i][m];
                    }
                }
                lobes = surface;
            }
            else
            {
                surface = mean;
                lobes = hits[i].kind == HitKind::Sky ? sky : mean;
            }
            const glm::vec3 lobesGathered = gathered(lobes);
            const glm::vec3 surfaceGathered = gathered(surface);
            float rayWeight = 0.f;
            for(int f = 0; f < 6; f++)
            {
                faces[f] += lobesGathered * r.weight[i][f];
                rayWeight += r.weight[i][f];
            }
            // A surface's lightmap says how much light fell where it is, so the nearer it is the more it says about
            // here (a lamp's pool beside the model: the lamp lights the model too; far walls, little).
            float along = RAY_LENGTH;
            if(hits[i].kind == HitKind::Surface)
            {
                along = distanceTo(hits[i].surf, p, r.dir[i], RAY_LENGTH);
            }
            rayWeight /= along + grid.nearWeight;
            level += surfaceGathered * rayWeight;
            levelWeight += rayWeight;
        }
        level = spread(level / levelWeight);
        GridEntry entry{};
        entry.style = styles[s];
        for(int c = 0; c < 3; c++)
        {
            entry.level[c] = gridFixed(level[c]);
        }
        for(int f = 0; f < 6; f++)
        {
            const glm::vec3 face = spread(faces[f]);
            for(int c = 0; c < 3; c++)
            {
                entry.faces[f][c] = gridFixed(face[c]);
            }
        }
        out.pushBack(entry);
    }
    int entries = numStyles;
    gridGlow(p, x, y, z, out, entries);
    cellValue = first | (static_cast<unsigned>(entries - 1) << GRID_COUNT_SHIFT);
}

// The file's key: all that gridProbe reads that can change without the planes changing (the lightmaps, their styles),
// and the planes for the rest. (The lightmaps' length is the furthest a surface's samples go.)
[[nodiscard]] unsigned long long gridCacheKey()
{
    const qmodel_t* world = cl.worldmodel;
    size_t lightBytes = 0;
    unsigned long long h = diskcache::fnvStart;
    for(int i = 0; i < world->numsurfaces; i++)
    {
        const msurface_t& surf = world->surfaces[i];
        h = diskcache::fnv(h, surf.styles, sizeof(surf.styles));
        if(surf.samples)
        {
            int layers = 0;
            while(layers < MAXLIGHTMAPS && surf.styles[layers] != 255)
            {
                layers++;
            }
            const size_t size = static_cast<size_t>((surf.extents[0] >> 4) + 1) * static_cast<size_t>((surf.extents[1] >> 4) + 1) * 3u * layers;
            lightBytes = za::max(lightBytes, static_cast<size_t>(surf.samples - world->lightdata) + size);
        }
    }
    h = diskcache::wideHash(h, world->lightdata, lightBytes);
    h = diskcache::wideHash(h, world->planes, static_cast<size_t>(world->numplanes) * sizeof(mplane_t));
    h = diskcache::fnv(h, &grid.nearWeight, sizeof(grid.nearWeight));
    h = diskcache::fnv(h, &grid.cell, sizeof(grid.cell));
    h = diskcache::fnv(h, &grid.glowKey, sizeof(grid.glowKey));
    return diskcache::fnv(h, grid.size, sizeof(grid.size));
}

// The counts, and the line of a load's log.
void gridFinish(double start)
{
    grid.probes = grid.clearCells = grid.styledProbes = 0;
    for(za::SizeT& n : grid.glowProbes)
    {
        n = 0;
    }
    for(const unsigned cell : grid.cells)
    {
        if((cell & GRID_FIRST) != GRID_FIRST)
        {
            grid.probes++;
            const GridEntry* entry = &grid.entries[cell & GRID_FIRST];
            const unsigned count = ((cell >> GRID_COUNT_SHIFT) & GRID_COUNT_MASK) + 1u;
            grid.styledProbes += count > 1u && !(entry[1].style & GRID_GLOW) ? 1u : 0u;
            for(unsigned k = 0; k < count; k++)
            {
                if(entry[k].style & GRID_GLOW)
                {
                    grid.glowProbes[(entry[k].style >> GRID_GLOW_KIND_SHIFT) & 7]++;
                }
            }
            grid.clearCells += (cell & GRID_CLEAR) != 0u ? 1u : 0u;
        }
    }
    grid.buildMs = (Sys_DoubleTime() - start) * 1000.0;
    Con_DPrintf("model light grid: %d x %d x %d of %.0f, %zu probes, %zu entries, %.1f MB, %.0f ms\n", grid.size[0], grid.size[1],
                grid.size[2], grid.cell, static_cast<size_t>(grid.probes), static_cast<size_t>(grid.entries.size()),
                static_cast<double>(grid.cells.size() * sizeof(unsigned) + grid.entries.size() * sizeof(GridEntry)) / 1048576.0,
                grid.buildMs);
}

[[nodiscard]] float gridNearSetting()
{
    return za::clamp(vr_model_light_grid_near.value, 1.f, 1024.f);
}

[[nodiscard]] float gridCellSetting()
{
    return za::clamp(vr_model_light_grid_cell.value, 16.f, 256.f);
}

// The grid of the current map: built when it loads, on the job threads (there are no rays left to trace after this).
void gridBuild()
{
    const double start = Sys_DoubleTime();
    // The last build's, kept aside: the same map loaded again (a restart, a level change back) reuses them below when its
    // key (the disk cache's: the lightmaps, the planes, the settings) is the same (ROUND24.md, "Tracy session 11-07").
    za::Vector<unsigned> lastCells;
    za::Vector<GridEntry> lastEntries;
    lastCells.swap(grid.cells);
    lastEntries.swap(grid.entries);
    const unsigned long long lastKey = grid.builtKey;
    grid.builtKey = 0ull;
    grid.cells.clear();
    grid.entries.clear();
    grid.world = cl.worldmodel;
    grid.generation = worldGeneration();
    grid.probes = grid.clearCells = grid.styledProbes = 0;
    grid.size[0] = grid.size[1] = grid.size[2] = 0;
    grid.nearWeight = gridNearSetting();
    grid.cellSetting = gridCellSetting();
    if(!cl.worldmodel || !cl.worldmodel->lightdata)
    {
        return;
    }

    // On a lattice over the whole world, a little off the multiples of 16 where the map's planes tend to lie.
    constexpr float GRID_OFF = 2.5f;
    grid.cell = grid.cellSetting;
    for(;;)
    {
        za::SizeT total = 1;
        for(int i = 0; i < 3; i++)
        {
            grid.origin[i] = glm::floor(cl.worldmodel->mins[i] / grid.cell) * grid.cell - GRID_OFF;
            grid.size[i] = static_cast<int>((cl.worldmodel->maxs[i] - grid.origin[i]) / grid.cell) + 2;
            total *= static_cast<za::SizeT>(grid.size[i]);
        }
        if(total <= GRID_MAX_CELLS)
        {
            grid.cells.resize(total, GRID_FIRST);
            break;
        }
        grid.cell *= 2.f;
    }

    glowPrepare();

    // A big map's grid, kept from an earlier load (vr_model_light_grid_cache).
    const int cacheMode = diskcache::modeOf(diskcache::Product::LightGrid);
    const unsigned long long cacheKey = cacheMode ? gridCacheKey() : 0ull;
    if(cacheMode == 1 && cacheKey != 0ull && cacheKey == lastKey && lastCells.size() == grid.cells.size())
    {
        grid.cells.swap(lastCells);
        grid.entries.swap(lastEntries);
        grid.builtKey = cacheKey;
        gridFinish(start);
        return;
    }
    const za::String cacheBuild = diskcache::buildOf(&GRID_CACHE_VERSION, 1);
    za::Vector<char> cached;
    unsigned cachedCells = 0u, cachedEntries = 0u;
    const bool hit = cacheMode && diskcache::read(diskcache::Product::LightGrid, cacheBuild.cStr(), cacheKey, "mlg", "QMLG", cachedCells,
                                                  cachedEntries, cached, "(the model light grid)") &&
                     cachedCells == grid.cells.size() &&
                     cached.size() == cachedCells * sizeof(unsigned) + cachedEntries * sizeof(GridEntry);
    if(hit && cacheMode == 1)
    {
        memcpy(grid.cells.data(), cached.data(), cachedCells * sizeof(unsigned));
        grid.entries.resize(cachedEntries, GridEntry{});
        memcpy(grid.entries.data(), cached.data() + cachedCells * sizeof(unsigned), cachedEntries * sizeof(GridEntry));
        grid.builtKey = cacheKey;
        gridFinish(start);
        return;
    }

    // The probes, a layer at a time; each layer's entries are its own until they are all in one list.
    const int layers = grid.size[2];
    za::Vector<za::Vector<GridEntry>> layerEntries(static_cast<za::SizeT>(layers));
    jobs::parallelFor(gridProbesSite, static_cast<za::SizeT>(layers), 1, [&layerEntries](za::SizeT z0, za::SizeT z1) {
        for(int z = static_cast<int>(z0); z < static_cast<int>(z1); z++)
        {
            za::Vector<GridEntry>& out = layerEntries[z];
            for(int y = 0; y < grid.size[1]; y++)
            {
                for(int x = 0; x < grid.size[0]; x++)
                {
                    const glm::vec3 p = grid.point(x, y, z);
                    vec3_t v{p.x, p.y, p.z};
                    const int contents = Mod_PointInLeaf(v, cl.worldmodel)->contents;
                    if(contents == CONTENTS_SOLID || contents == CONTENTS_SKY)
                    {
                        continue;
                    }
                    // Not in the open air, where nothing stands (an outdoor map's sky is nearly all of its cells).
                    Hit below;
                    constexpr glm::vec3 DOWN{0.f, 0.f, -1.f};
                    trace(cl.worldmodel->nodes, DOWN, p, p + DOWN * GRID_AIR, below);
                    if(below.kind == HitKind::Surface || below.kind == HitKind::Lava)
                    {
                        gridProbe(p, x, y, z, out, grid.cells[grid.index(x, y, z)]);
                    }
                }
            }
        }
    });
    za::SizeT totalEntries = 0;
    for(const za::Vector<GridEntry>& layer : layerEntries)
    {
        totalEntries += layer.size();
    }
    grid.entries.reserve(totalEntries);
    for(int z = 0; z < layers; z++)
    {
        const unsigned base = static_cast<unsigned>(grid.entries.size());
        for(const GridEntry& entry : layerEntries[z])
        {
            grid.entries.pushBack(entry);
        }
        for(int y = 0; y < grid.size[1]; y++)
        {
            for(int x = 0; x < grid.size[0]; x++)
            {
                unsigned& cell = grid.cells[grid.index(x, y, z)];
                if((cell & GRID_FIRST) != GRID_FIRST)
                {
                    cell += base;
                }
            }
        }
    }

    // The cells with nothing in them: all eight corners probes, and the long diagonals clear.
    za::Vector<unsigned char> clear(grid.cells.size(), static_cast<unsigned char>(0));
    jobs::parallelFor(gridClearSite, static_cast<za::SizeT>(za::max(layers - 1, 0)), 1, [&clear](za::SizeT z0, za::SizeT z1) {
        for(int z = static_cast<int>(z0); z < static_cast<int>(z1); z++)
        {
            for(int y = 0; y + 1 < grid.size[1]; y++)
            {
                for(int x = 0; x + 1 < grid.size[0]; x++)
                {
                    bool open = true;
                    for(int c = 0; c < 8 && open; c++)
                    {
                        open = (grid.cells[grid.index(x + (c & 1), y + ((c >> 1) & 1), z + (c >> 2))] & GRID_FIRST) != GRID_FIRST;
                    }
                    for(int c = 0; c < 4 && open; c++)
                    {
                        const glm::vec3 a = grid.point(x + (c & 1), y + (c >> 1), z);
                        const glm::vec3 b = grid.point(x + 1 - (c & 1), y + 1 - (c >> 1), z + 1);
                        open = worldtrace::line(a, b) >= 1.f;
                    }
                    clear[grid.index(x, y, z)] = open ? 1 : 0;
                }
            }
        }
    });
    for(za::SizeT i = 0; i < clear.size(); i++)
    {
        if(clear[i])
        {
            grid.cells[i] |= GRID_CLEAR;
        }
    }

    const size_t cellBytes = grid.cells.size() * sizeof(unsigned);
    const size_t entryBytes = grid.entries.size() * sizeof(GridEntry);
    if(hit)
    {
        diskcache::noteChecked(diskcache::Product::LightGrid, cachedCells == grid.cells.size() && cached.size() == cellBytes + entryBytes &&
            memcmp(cached.data(), grid.cells.data(), cellBytes) == 0 && memcmp(cached.data() + cellBytes, grid.entries.data(), entryBytes) == 0,
            "(the model light grid)");
    }
    else if(cacheMode && Sys_DoubleTime() - start >= GRID_CACHE_SECONDS)
    {
        za::Vector<char> bytes;
        bytes.resize(cellBytes + entryBytes);
        memcpy(bytes.data(), grid.cells.data(), cellBytes);
        memcpy(bytes.data() + cellBytes, grid.entries.data(), entryBytes);
        diskcache::write(diskcache::Product::LightGrid, cacheBuild.cStr(), cacheKey, "mlg", "QMLG", static_cast<unsigned>(grid.cells.size()),
                         static_cast<unsigned>(grid.entries.size()), bytes.data(), bytes.size());
    }
    grid.builtKey = cacheKey;
    gridFinish(start);
}

[[nodiscard]] bool gridReady()
{
    if(grid.world != cl.worldmodel || grid.generation != worldGeneration() || grid.nearWeight != gridNearSetting() ||
       grid.cellSetting != gridCellSetting())
    {
        gridBuild(); // (on switching the setting on or moving vr_model_light_grid_near or _cell: a map's load has built it otherwise)
    }
    return grid.probes != 0;
}

// The light of a grid's probes at their styles' current values.
struct GridRead
{
    glm::vec3 level{0.f};
    Cube faces{};
};

// Where a point is in the grid: its cell and how far across.
struct GridSpot
{
    int x, y, z;
    glm::vec3 across;
    [[nodiscard]] float weight(int c) const
    {
        return ((c & 1) ? across.x : 1.f - across.x) * (((c >> 1) & 1) ? across.y : 1.f - across.y) *
               ((c >> 2) ? across.z : 1.f - across.z);
    }
    [[nodiscard]] za::SizeT corner(int c) const
    {
        return grid.index(x + (c & 1), y + ((c >> 1) & 1), z + (c >> 2));
    }
};

[[nodiscard]] GridSpot gridSpot(const glm::vec3& p)
{
    const glm::vec3 f = (p - grid.origin) / grid.cell;
    GridSpot s{};
    s.x = za::clamp(static_cast<int>(glm::floor(f.x)), 0, grid.size[0] - 2);
    s.y = za::clamp(static_cast<int>(glm::floor(f.y)), 0, grid.size[1] - 2);
    s.z = za::clamp(static_cast<int>(glm::floor(f.z)), 0, grid.size[2] - 2);
    s.across = glm::clamp(f - glm::vec3{s.x, s.y, s.z}, glm::vec3{0.f}, glm::vec3{1.f});
    return s;
}

// Which of the cell's corners `p` sees: all in a clear cell; else a line to each (its light through the wall left out).
[[nodiscard]] unsigned gridSeen(const GridSpot& s, const glm::vec3& p)
{
    if(grid.cells[s.corner(0)] & GRID_CLEAR)
    {
        return 255u;
    }
    unsigned seen = 0u;
    for(int c = 0; c < 8; c++)
    {
        if(s.weight(c) > 0.f && (grid.cells[s.corner(c)] & GRID_FIRST) != GRID_FIRST &&
           worldtrace::line(p, grid.point(s.x + (c & 1), s.y + ((c >> 1) & 1), s.z + (c >> 2))) >= 1.f)
        {
            seen |= 1u << c;
        }
    }
    return seen;
}

// What a lit liquid's entry is read at: the kind's strength, fading out by its range from the probe's distance to it.
[[nodiscard]] float glowScale(unsigned short style)
{
    const GlowKnob& knob = GLOW_KNOBS[(style >> GRID_GLOW_KIND_SHIFT) & 7];
    const float range = knob.range->value;
    const float left = range > 0.f ? 1.f - static_cast<float>(style & GRID_GLOW_DISTANCE) / range : 0.f;
    return left > 0.f ? GLOW_GAIN * za::max(knob.strength->value, 0.f) * left * left : 0.f;
}

// The light at a spot from the corners seen, weighted up to make up for the rest; false when there are none. (`withFaces`
// false: its level only, what's drawn beside the models needs no more.)
[[nodiscard]] bool gridRead(const GridSpot& s, unsigned seen, GridRead& out, bool withFaces = true)
{
    out = GridRead{};
    float total = 0.f;
    for(int c = 0; c < 8; c++)
    {
        const float w = s.weight(c);
        const unsigned cell = grid.cells[s.corner(c)];
        if(w <= 0.f || !(seen & (1u << c)) || (cell & GRID_FIRST) == GRID_FIRST)
        {
            continue;
        }
        total += w;
        const GridEntry* entry = &grid.entries[cell & GRID_FIRST];
        const int count = static_cast<int>((cell >> GRID_COUNT_SHIFT) & GRID_COUNT_MASK) + 1;
        for(int k = 0; k < count; k++, entry++)
        {
            const float scale = w * ((entry->style & GRID_GLOW) ? glowScale(entry->style) : static_cast<float>(d_lightstylevalue[entry->style]) / 256.f) / GRID_FIXED;
            out.level += glm::vec3{entry->level[0], entry->level[1], entry->level[2]} * scale;
            for(int f = 0; withFaces && f < 6; f++)
            {
                out.faces[f] += glm::vec3{entry->faces[f][0], entry->faces[f][1], entry->faces[f][2]} * scale;
            }
        }
    }
    if(total <= 1e-4f)
    {
        return false;
    }
    out.level *= za::max(vr_model_light_grid_intensity.value, 0.f) / total;
    for(glm::vec3& f : out.faces)
    {
        f /= total;
    }
    return true;
}

// Whether the light of the corners seen moves with a light style.
[[nodiscard]] bool gridStyled(const GridSpot& s, unsigned seen)
{
    for(int c = 0; c < 8; c++)
    {
        const unsigned cell = grid.cells[s.corner(c)];
        if(!(seen & (1u << c)) || (cell & GRID_FIRST) == GRID_FIRST)
        {
            continue;
        }
        const GridEntry* entry = &grid.entries[cell & GRID_FIRST];
        const int count = static_cast<int>((cell >> GRID_COUNT_SHIFT) & GRID_COUNT_MASK) + 1;
        for(int k = 0; k < count; k++, entry++)
        {
            if(!(entry->style & GRID_GLOW) && entry->style != 0)
            {
                return true;
            }
        }
    }
    return false;
}

// What's drawn beside the models reads the same probes (ambient::lightPoint), a read for each 16 units a frame at most:
// a burst of blood asks a few hundred times, all in one place.
struct PointRead
{
    int frame{-1};
    int key[3]{};
    bool found{false};
    glm::vec3 level{0.f};
};
constexpr za::SizeT POINT_READS = 256; // (a power of two)
constexpr float POINT_READ_SIZE = 16.f;
PointRead pointReads[POINT_READS];

// One entity's reading, once a frame. `drawnLevel`, `drawnCube`: what it is lit by; off them by the part of a step (a
// probe coming into sight or going out of it) still fading, so that those don't pop either.
struct GridCached
{
    int frame{-1};
    bool valid{false};
    glm::vec3 point{0.f}, seenAt{0.f};
    za::SizeT cell{0};
    unsigned seen{0u};
    GridRead read;
    float settings{-1.f};
    Cube cube{};
    glm::vec3 fadeLevel{0.f};
    Cube fadeCube{};
    glm::vec3 drawnLevel{0.f};
    Cube drawnCube{};
};

ankerl::unordered_dense::map<const entity_t*, GridCached> gridCache;
Eviction gridEviction;
const qmodel_t* gridCacheWorld = nullptr;
int gridCacheGeneration = -1;
float gridCacheCell = 0.f; // (another spacing: the entries' cells are other cells)

// Where a model reads the light: its origin (VR_AliasLightOrigin: a climbing hand's as if not moved for looks) at the
// middle of its height; held weapons and hands at their origin, what's round them being a hand's width away.
[[nodiscard]] glm::vec3 gridPoint(const entity_t* e, bool view)
{
    float origin[3];
    VR_AliasLightOrigin(e, origin);
    glm::vec3 at{origin[0], origin[1], origin[2]};
    glm::vec3 anchor = geom::vec(r_refdef.vieworg);
    if(!view)
    {
        anchor = at + glm::vec3{0.f, 0.f, 8.f};
        at.z += (e->model->mins[2] + e->model->maxs[2]) * 0.5f;
    }
    return openPoint(at, anchor);
}

// The entity's light from the grid this frame; null when the grid is off or has nothing for it (the old way, then).
[[nodiscard]] const GridCached* gridLight(const entity_t* e)
{
    if(vr_model_light_grid.value == 0.f || !cl.worldmodel || !cl.worldmodel->lightdata || !e || !e->model ||
       (e->model->flags & MOD_FBRIGHTHACK) || !gridReady())
    {
        return nullptr;
    }
    if(cl.worldmodel != gridCacheWorld || worldGeneration() != gridCacheGeneration || grid.cell != gridCacheCell)
    {
        gridCacheCell = grid.cell;
        gridCache.clear();
        gridEviction = {};
        gridCacheWorld = cl.worldmodel;
        gridCacheGeneration = worldGeneration();
    }

    GridCached* entry = &gridCache[e];
    if(entry->frame == host_framecount)
    {
        return entry->valid ? entry : nullptr;
    }
    {
        GridCached& c = *entry;
        c.frame = host_framecount;
        const bool view = e == &cl.viewent || VR_IsViewEntity(e);
        const glm::vec3 p = gridPoint(e, view);
        const GridSpot spot = gridSpot(p);
        const za::SizeT cell = spot.corner(0);
        const bool clear = (grid.cells[cell] & GRID_CLEAR) != 0u;
        const bool sameCell = c.valid && cell == c.cell;
        unsigned seen = c.seen;
        if(!sameCell || (!clear && glm::distance(p, c.seenAt) > GRID_MASK_MOVE))
        {
            seen = gridSeen(spot, p);
            c.seenAt = p;
        }

        GridRead read;
        const bool teleported = c.valid && glm::distance(p, c.point) > GRID_TELEPORT;
        const bool readable = gridRead(spot, seen, read);
        if(readable)
        {
            // A step in what is read: from one set of corners to another. A step into another clear cell is none: the
            // two cells share the corners that count there.
            const bool stepped = c.valid && !teleported && (sameCell ? seen != c.seen : !(clear && (grid.cells[c.cell] & GRID_CLEAR)));
            const float settings = lighting::ambientFloor() * 4096.f + vr_model_ambient_contrast.value * 16.f + lighting::lightContrast() +
                                   lighting::capKey() * 65536.f;
            if(!c.valid || settings != c.settings || memcmp(&read, &c.read, sizeof(GridRead)) != 0)
            {
                Cube faces{};
                for(int f = 0; f < 6; f++)
                {
                    faces[f] = read.faces[f];
                    lighting::lightCurve(&faces[f].x);
                }
                ratiosFromFaces(faces, c.cube);
                c.settings = settings;
            }
            if(stepped)
            {
                c.fadeLevel = c.drawnLevel - read.level;
                for(int f = 0; f < 6; f++)
                {
                    c.fadeCube[f] = c.drawnCube[f] - c.cube[f];
                }
            }
            else if(!c.valid || teleported)
            {
                c.fadeLevel = glm::vec3{0.f};
                for(glm::vec3& f : c.fadeCube)
                {
                    f = glm::vec3{0.f};
                }
            }
            c.read = read;
            c.seen = seen;
            c.cell = cell;
            c.point = p;
            c.valid = true;
        }
        // (None in sight, the point in a wall's corner: the light it last had, or the old way if it has none.)
        if(c.valid)
        {
            const float fade = za::exp(-static_cast<float>(host_frametime) * EASE_RATE);
            c.fadeLevel *= fade;
            c.drawnLevel = c.read.level + c.fadeLevel;
            for(int f = 0; f < 6; f++)
            {
                c.fadeCube[f] *= fade;
                c.drawnCube[f] = c.cube[f] + c.fadeCube[f];
            }
        }
    }

    const za::SizeT before = gridCache.size();
    evictStale(gridCache, 2048, 100, host_framecount, gridEviction);
    if(gridCache.size() != before)
    {
        entry = &gridCache.find(e)->second;
    }
    return entry->valid ? entry : nullptr;
}

// Show Light Probes (vr_model_light_grid_show): each probe within GRID_SHOW_REACH of `at` as a cross, the colour white
// paint has under what a model there would be lit by (its styles as they are now, the intensity and the world's curve:
// full light is white), hidden behind what is in front of it. Then the model looked at, or else the nearest: a line from
// where it reads to each of the eight corners it reads, as thick as that corner's say and the corner's colour (one it
// can't see: thin and red), and a dot there in the colour it reads. Compare a cross with the floor beside it. The count
// of crosses drawn (the test's).
constexpr float GRID_SHOW_REACH = 256.f;
constexpr float GRID_SHOW_CROSS = 2.5f;  // half a cross's arm
constexpr float GRID_SHOW_LOOK = 48.f;   // a model this near the line of sight is the one looked at
constexpr float GRID_SHOW_MODEL = 768.f; // or the nearest within this

[[nodiscard]] glm::vec4 gridShowColour(glm::vec3 light)
{
    lighting::lightCurve(&light.x);
    return glm::vec4{glm::min(light / 128.f, glm::vec3{1.f}), 1.f};
}

// A probe's own light, what a model right on it would read.
[[nodiscard]] bool gridProbeColour(int x, int y, int z, glm::vec4& out)
{
    GridRead read;
    if((grid.cells[grid.index(x, y, z)] & GRID_FIRST) == GRID_FIRST || !gridRead(GridSpot{x, y, z, glm::vec3{0.f}}, 1u, read, false))
    {
        return false;
    }
    out = gridShowColour(read.level);
    return true;
}

za::SizeT gridShowProbes(const glm::vec3& at)
{
    za::SizeT drawn = 0;
    const GridSpot centre = gridSpot(at);
    const int reach = static_cast<int>(glm::ceil(GRID_SHOW_REACH / grid.cell));
    for(int z = za::max(centre.z - reach, 0); z <= za::min(centre.z + reach + 1, grid.size[2] - 2); z++)
    {
        for(int y = za::max(centre.y - reach, 0); y <= za::min(centre.y + reach + 1, grid.size[1] - 2); y++)
        {
            for(int x = za::max(centre.x - reach, 0); x <= za::min(centre.x + reach + 1, grid.size[0] - 2); x++)
            {
                const glm::vec3 p = grid.point(x, y, z);
                glm::vec4 colour;
                if(glm::distance(p, at) > GRID_SHOW_REACH || !gridProbeColour(x, y, z, colour))
                {
                    continue;
                }
                for(int axis = 0; axis < 3; axis++)
                {
                    glm::vec3 arm{0.f};
                    arm[axis] = GRID_SHOW_CROSS;
                    lines::sceneLine(p - arm, p + arm, 1.f, colour, colour);
                }
                drawn++;
            }
        }
    }
    return drawn;
}

// The model looked at from `at` along `forward`, or else the nearest; null when there's none.
[[nodiscard]] const entity_t* gridShowModel(const glm::vec3& at, const glm::vec3& forward)
{
    const entity_t* looked = nullptr;
    const entity_t* nearest = nullptr;
    float lookedBy = GRID_SHOW_LOOK, nearestBy = GRID_SHOW_MODEL;
    for(int i = 0; i < cl_numvisedicts; i++)
    {
        const entity_t* e = cl_visedicts[i];
        if(!e || !e->model || e->model->type != mod_alias || (e->model->flags & MOD_FBRIGHTHACK) || e == &cl.viewent || VR_IsViewEntity(e))
        {
            continue;
        }
        const glm::vec3 to = gridPoint(e, false) - at;
        const float along = glm::dot(to, forward);
        const float off = glm::length(to - forward * along);
        if(along > 0.f && off < lookedBy)
        {
            lookedBy = off;
            looked = e;
        }
        if(glm::length(to) < nearestBy)
        {
            nearestBy = glm::length(to);
            nearest = e;
        }
    }
    return looked ? looked : nearest;
}

void gridShowModelProbes(const entity_t* e)
{
    const glm::vec3 p = gridPoint(e, false);
    const GridSpot spot = gridSpot(p);
    const unsigned seen = gridSeen(spot, p);
    const glm::vec4 unseen{1.f, 0.1f, 0.1f, 0.8f};
    for(int c = 0; c < 8; c++)
    {
        const int x = spot.x + (c & 1), y = spot.y + ((c >> 1) & 1), z = spot.z + (c >> 2);
        glm::vec4 colour;
        if((grid.cells[grid.index(x, y, z)] & GRID_FIRST) == GRID_FIRST)
        {
            continue;
        }
        if((seen & (1u << c)) && x + 1 < grid.size[0] && y + 1 < grid.size[1] && z + 1 < grid.size[2] && gridProbeColour(x, y, z, colour))
        {
            lines::line(p, grid.point(x, y, z), 0.2f + 1.3f * spot.weight(c), colour, colour);
        }
        else
        {
            lines::line(p, grid.point(x, y, z), 0.2f, unseen, unseen);
        }
    }
    GridRead read;
    if(gridRead(spot, seen, read, false))
    {
        lines::point(p, 5.f, gridShowColour(read.level));
    }
}

// vr_model_light_grid_info: what the map's grid holds; the light read at the view.
void gridInfo_f()
{
    if(!cl.worldmodel || !gridReady())
    {
        Con_Printf("model light grid: none (no lit world)\n");
        return;
    }
    Con_Printf("model light grid: %d x %d x %d cells of %.0f, %zu probes (%zu with more than one light style), %zu clear cells, %.1f MB, built in %.0f ms\n",
               grid.size[0], grid.size[1], grid.size[2], grid.cell, static_cast<size_t>(grid.probes), static_cast<size_t>(grid.styledProbes),
               static_cast<size_t>(grid.clearCells),
               static_cast<double>(grid.cells.size() * sizeof(unsigned) + grid.entries.size() * sizeof(GridEntry)) / 1048576.0, grid.buildMs);
    Con_Printf("model light grid: lit liquids in reach of %zu probes (lava), %zu (slime), %zu (teleporter); %zu entries\n",
               static_cast<size_t>(grid.glowProbes[0]), static_cast<size_t>(grid.glowProbes[1]), static_cast<size_t>(grid.glowProbes[2]),
               static_cast<size_t>(grid.entries.size()));
    {
        unsigned long long h = diskcache::wideHash(diskcache::fnvStart, grid.cells.data(), grid.cells.size() * sizeof(unsigned));
        h = diskcache::wideHash(h, grid.entries.data(), grid.entries.size() * sizeof(GridEntry));
        Con_Printf("model light grid: contents hash %016llx (key %016llx)\n", h, grid.builtKey);
    }
    for(const auto& [tx, colour] : grid.glowColours)
    {
        Con_Printf("model light grid: liquid %s lights %.0f %.0f %.0f\n", tx ? tx->name : "?", colour.r, colour.g, colour.b);
    }

    // At the view (the eye: a model's middle, about): the grid's level with and without the liquids, the floor's.
    {
        const glm::vec3 eye = geom::vec(r_refdef.vieworg);
        const GridSpot spot = gridSpot(eye);
        const unsigned seen = gridSeen(spot, eye);
        GridRead read;
        if(gridRead(spot, seen, read, false))
        {
            glm::vec3 baked{0.f};
            float total = 0.f;
            for(int c = 0; c < 8; c++)
            {
                const unsigned cell = grid.cells[spot.corner(c)];
                if(!(seen & (1u << c)) || (cell & GRID_FIRST) == GRID_FIRST || spot.weight(c) <= 0.f)
                {
                    continue;
                }
                total += spot.weight(c);
                const GridEntry* entry = &grid.entries[cell & GRID_FIRST];
                const int count = static_cast<int>((cell >> GRID_COUNT_SHIFT) & GRID_COUNT_MASK) + 1;
                for(int k = 0; k < count; k++)
                {
                    if(!(entry[k].style & GRID_GLOW))
                    {
                        baked += glm::vec3{entry[k].level[0], entry[k].level[1], entry[k].level[2]} * spot.weight(c) *
                                 (static_cast<float>(d_lightstylevalue[entry[k].style]) / (256.f * GRID_FIXED));
                    }
                }
            }
            baked *= za::max(vr_model_light_grid_intensity.value, 0.f) / za::max(total, 1e-4f);
            vec3_t v{eye.x, eye.y, eye.z};
            lightcache_t floorCache{};
            (void)R_LightPoint(v, 0.f, &floorCache);
            const glm::vec3 floorLight{lightcolor[0], lightcolor[1], lightcolor[2]};
            glm::vec3 drawn = read.level;
            lighting::lightCurve(&drawn.x);
            Con_Printf("model light grid: at the view: baked %.0f %.0f %.0f, with liquids %.0f %.0f %.0f, drawn %.0f %.0f %.0f; the floor's %.0f %.0f %.0f\n",
                       baked.r, baked.g, baked.b, read.level.r, read.level.g, read.level.b, drawn.r, drawn.g, drawn.b, floorLight.r, floorLight.g, floorLight.b);
        }
    }

    // The model looked at (or the nearest), and every probe just over a floor: what a model reads, drawn, against the
    // floor under it, drawn: is the probes' light on a par with the world's?
    {
        const auto drawnFloor = [](const glm::vec3& at) {
            vec3_t v{at.x, at.y, at.z};
            lightcache_t cache{};
            (void)R_LightPoint(v, 0.f, &cache);
            glm::vec3 light{lightcolor[0], lightcolor[1], lightcolor[2]};
            lighting::lightCurve(&light.x);
            return light;
        };
        const glm::vec3 eye = geom::vec(r_refdef.vieworg);
        if(const entity_t* e = gridShowModel(eye, geom::vec(vpn)))
        {
            const glm::vec3 p = gridPoint(e, false);
            const GridSpot spot = gridSpot(p);
            GridRead read;
            if(gridRead(spot, gridSeen(spot, p), read, false))
            {
                lighting::lightCurve(&read.level.x);
                const glm::vec3 under = drawnFloor(geom::vec(e->origin));
                Con_Printf("model light grid: %s reads %.0f %.0f %.0f drawn; the floor under it %.0f %.0f %.0f drawn\n", e->model->name,
                           read.level.r, read.level.g, read.level.b, under.r, under.g, under.b);
            }
        }
        constexpr float OVER_FLOOR = 64.f;
        float gridSum = 0.f, floorSum = 0.f;
        int spots = 0, dimmer = 0, brighter = 0;
        for(int z = 0; z + 1 < grid.size[2]; z++)
        {
            for(int y = 0; y + 1 < grid.size[1]; y++)
            {
                for(int x = 0; x + 1 < grid.size[0]; x++)
                {
                    const glm::vec3 p = grid.point(x, y, z);
                    GridRead read;
                    if((grid.cells[grid.index(x, y, z)] & GRID_FIRST) == GRID_FIRST || worldtrace::line(p, p - glm::vec3{0.f, 0.f, OVER_FLOOR}) >= 1.f ||
                       !gridRead(GridSpot{x, y, z, glm::vec3{0.f}}, 1u, read, false))
                    {
                        continue;
                    }
                    lighting::lightCurve(&read.level.x);
                    const float at = (read.level.r + read.level.g + read.level.b) / 3.f;
                    const glm::vec3 under = drawnFloor(p);
                    const float below = (under.r + under.g + under.b) / 3.f;
                    gridSum += at;
                    floorSum += below;
                    spots++;
                    dimmer += at < below * 0.7f ? 1 : 0;
                    brighter += at > below * 1.3f ? 1 : 0;
                }
            }
        }
        Con_Printf("model light grid: at %d probes over a floor, drawn: the probes' mean %.1f, the floors' %.1f; %d probes under 0.7 of their floor, %d over 1.3\n",
                   spots, gridSum / static_cast<float>(za::max(spots, 1)), floorSum / static_cast<float>(za::max(spots, 1)), dimmer, brighter);
    }

    // Thin walls: at the middle of each cell with something in it, how much light the probes out of sight would add.
    int spots = 0, kept = 0;
    float worst = 0.f;
    glm::vec3 worstAt{0.f};
    for(int z = 0; z + 1 < grid.size[2]; z++)
    {
        for(int y = 0; y + 1 < grid.size[1]; y++)
        {
            for(int x = 0; x + 1 < grid.size[0]; x++)
            {
                if(grid.cells[grid.index(x, y, z)] & GRID_CLEAR)
                {
                    continue;
                }
                const glm::vec3 p = grid.point(x, y, z) + glm::vec3{grid.cell * 0.5f};
                if(solid(p))
                {
                    continue;
                }
                const GridSpot spot = gridSpot(p);
                GridRead inSight, all;
                if(!gridRead(spot, gridSeen(spot, p), inSight) || !gridRead(spot, 255u, all))
                {
                    continue;
                }
                spots++;
                lighting::lightCurve(&inSight.level.x);
                lighting::lightCurve(&all.level.x);
                const float more = luma(all.level) - luma(inSight.level);
                kept += more > 16.f ? 1 : 0;
                if(more > worst)
                {
                    worst = more;
                    worstAt = p;
                }
            }
        }
    }
    Con_Printf("model light grid: %d cells with walls in them; the probes out of sight would add over 16 drawn light at %d, %.0f at worst, at %.0f %.0f %.0f\n",
               spots, kept, worst, worstAt.x, worstAt.y, worstAt.z);

    // What a model's reading costs: every cell's middle, with its lines where it has walls.
    const za::Clock clock;
    int reads = 0;
    float readSum = 0.f;
    for(int z = 0; z + 1 < grid.size[2]; z += 3)
    {
        for(int y = 0; y + 1 < grid.size[1]; y += 3)
        {
            for(int x = 0; x + 1 < grid.size[0]; x += 3)
            {
                const glm::vec3 p = grid.point(x, y, z) + glm::vec3{grid.cell * 0.5f};
                const GridSpot spot = gridSpot(p);
                GridRead read;
                if(gridRead(spot, gridSeen(spot, p), read))
                {
                    Cube faces{};
                    for(int f = 0; f < 6; f++)
                    {
                        faces[f] = read.faces[f];
                        lighting::lightCurve(&faces[f].x);
                    }
                    Cube cube{};
                    ratiosFromFaces(faces, cube);
                    readSum += cube[0].r;
                    reads++;
                }
            }
        }
    }
    Con_Printf("model light grid: a model's reading %.2f us (of %d; checksum %.0f), its own rays %.1f us (of %lld)\n",
               static_cast<double>(clock.getElapsedTime().asMicroseconds()) / za::max(reads, 1), reads, static_cast<double>(readSum),
               samplesTaken ? sampleSeconds * 1e6 / static_cast<double>(samplesTaken) : 0.0, samplesTaken);

    // Light styles: the first probe with an animated style that gives it some light, as it is read now (run it a few
    // frames apart to see it move).
    for(za::SizeT i = 0; i < grid.cells.size(); i++)
    {
        const unsigned cell = grid.cells[i];
        const GridEntry* entry = (cell & GRID_FIRST) != GRID_FIRST ? &grid.entries[cell & GRID_FIRST] : nullptr;
        if(entry && ((cell >> GRID_COUNT_SHIFT) & GRID_COUNT_MASK) != 0u && !(entry[1].style & GRID_GLOW) && entry[1].level[0] >= 16.f * GRID_FIXED && d_lightstylevalue[entry[1].style] != 0)
        {
            const float now = (static_cast<float>(entry[0].level[0]) * static_cast<float>(d_lightstylevalue[entry[0].style]) +
                               static_cast<float>(entry[1].level[0]) * static_cast<float>(d_lightstylevalue[entry[1].style])) / (256.f * GRID_FIXED);
            Con_Printf("model light grid: a probe's styles %d %d, their values now %d %d, its light now %.1f\n", entry[0].style, entry[1].style,
                       d_lightstylevalue[entry[0].style], d_lightstylevalue[entry[1].style], now);
            break;
        }
    }
}

// vr_model_light_walk <x0 y0 z0> <x1 y1 z1>: the light a model would have walking that line, the floor's under it (the
// old way) and the grid's, both drawn (lighting::lightCurve): the largest change in a 4-unit step, the verdict line
// last (a seam shows as a step in the floor's).
void gridWalk_f()
{
    if(Cmd_Argc() < 7 || !cl.worldmodel || !gridReady())
    {
        Con_Printf("vr_model_light_walk <x0 y0 z0> <x1 y1 z1>\n");
        return;
    }
    const glm::vec3 a{Q_atof(Cmd_Argv(1)), Q_atof(Cmd_Argv(2)), Q_atof(Cmd_Argv(3))};
    const glm::vec3 b{Q_atof(Cmd_Argv(4)), Q_atof(Cmd_Argv(5)), Q_atof(Cmd_Argv(6))};
    constexpr float WALK_STEP = 4.f;
    const int steps = za::max(1, static_cast<int>(glm::distance(a, b) / WALK_STEP));
    float lastFloor = -1.f, lastGrid = -1.f, jumpFloor = 0.f, jumpGrid = 0.f, loFloor = 1e9f, hiFloor = 0.f, loGrid = 1e9f, hiGrid = 0.f;
    for(int i = 0; i <= steps; i++)
    {
        const glm::vec3 p = glm::mix(a, b, static_cast<float>(i) / static_cast<float>(steps));
        lightcache_t cache = {};
        vec3_t v{p.x, p.y, p.z};
        R_LightPoint(v, 0.f, &cache);
        float floorLight[3] = {lightcolor[0], lightcolor[1], lightcolor[2]};
        lighting::lightCurve(floorLight);
        const GridSpot spot = gridSpot(p);
        GridRead read;
        if(!gridRead(spot, gridSeen(spot, p), read))
        {
            continue;
        }
        lighting::lightCurve(&read.level.x);
        const float f = luma(glm::vec3{floorLight[0], floorLight[1], floorLight[2]});
        const float g = luma(read.level);
        jumpFloor = lastFloor >= 0.f ? za::max(jumpFloor, glm::abs(f - lastFloor)) : 0.f;
        jumpGrid = lastGrid >= 0.f ? za::max(jumpGrid, glm::abs(g - lastGrid)) : 0.f;
        lastFloor = f;
        lastGrid = g;
        loFloor = za::min(loFloor, f);
        hiFloor = za::max(hiFloor, f);
        loGrid = za::min(loGrid, g);
        hiGrid = za::max(hiGrid, g);
    }
    Con_Printf("model light walk: floor %.0f..%.0f step %.1f, grid %.0f..%.0f step %.1f (drawn light, 128 full)\n", loFloor, hiFloor, jumpFloor, loGrid, hiGrid, jumpGrid);
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
    if(const GridCached* lit = gridLight(e))
    {
        for(int f = 0; f < 6; f++)
        {
            out[f][0] = lit->drawnCube[f].r;
            out[f][1] = lit->drawnCube[f].g;
            out[f][2] = lit->drawnCube[f].b;
        }
        // vr_model_light_grid_mix: 0 the map's lights' directional shading alone, 0.5 half of it under the whole cube (as
        // the rays below), 1 the cube alone.
        const float mix = za::clamp(vr_model_light_grid_mix.value, 0.f, 1.f);
        out[0][3] = za::min(mix * 2.f, 1.f);
        out[1][3] = 1.f - mix;
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
        const bool view = e == &cl.viewent || VR_IsViewEntity(e);
        const glm::vec3 anchor = view ? geom::vec(r_refdef.vieworg)
                                      : glm::vec3{e->origin[0], e->origin[1], e->origin[2] + 8.f};
        const glm::vec3 center = modelCenter(e, modelMatrix, static_cast<const aliashdr_t*>(aliashdr));
        if(!c.openValid || center != c.centerAsked || anchor != c.anchorAsked)
        {
            c.centerAsked = center;
            c.anchorAsked = anchor;
            c.openAt = openPoint(center, anchor);
            c.openValid = true;
        }
        const glm::vec3 p = c.openAt;
        const bool fresh = c.tracedAt < 0.0;
        const float moved = fresh ? 0.f : glm::distance(p, c.samplePos);
        // Staggered between entities, so that they don't all come due in the same frame.
        const double stagger = static_cast<double>(reinterpret_cast<za::UIntPtrT>(e) % 97) * 0.005;
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
            const za::Clock clock;
            if(fresh || moved > 128.f) // new, or teleported: no easing from where it was
            {
                traceProbe(p, c.current);
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
                traceProbe(p, c.current);
                c.currentShaded.valid = false;
                c.blend = 0.f;
            }
            sampleSeconds += static_cast<double>(clock.getElapsedTime().asMicroseconds()) / 1e6;
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
                                   lighting::lightContrast() + lighting::capKey() * 65536.f; // the settings shade() reads, as one number
            // (Each probe's shading is kept while its light styles hold still: easing only mixes them.)
            if(reshade || (c.current.animated && !shadeFresh(c.currentShaded, c.current)) ||
               (c.blend < 1.f && c.previous.animated) || settings != c.settings)
            {
                c.settings = settings;
                const Cube& now = shadeMemo(c.currentShaded, c.current);
                za::copy(now, now + 6, c.cube); // (a Cube's six faces)
                if(c.blend < 1.f)
                {
                    const Cube& older = shadeMemo(c.previousShaded, c.previous);
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

// R_AliasLightPoint: the model's light from the grid into `lightcolor` (R_LightPoint's units), nonzero; 0 for the old way.
extern "C" int VR_ModelLightGrid(const entity_t* e, float out[3])
{
    const GridCached* lit = gridLight(e);
    if(!lit)
    {
        return 0;
    }
    out[0] = lit->drawnLevel.r;
    out[1] = lit->drawnLevel.g;
    out[2] = lit->drawnLevel.b;
    return 1;
}

void ambient::gridDebugDraw(const glm::vec3& head)
{
    if(vr_model_light_grid_show.value == 0.f || !ambient::gridOn())
    {
        return;
    }
    (void)gridShowProbes(head);
    if(const entity_t* e = gridShowModel(head, geom::vec(vpn)))
    {
        gridShowModelProbes(e);
    }
}

bool ambient::gridOn()
{
    return vr_model_light_grid.value != 0.f && cl.worldmodel && cl.worldmodel->lightdata && gridReady();
}

bool ambient::gridLightAt(const glm::vec3& p, glm::vec3& out, bool* styled)
{
    const GridSpot spot = gridSpot(p);
    const unsigned seen = gridSeen(spot, p);
    GridRead read;
    if(!gridRead(spot, seen, read, false))
    {
        return false;
    }
    out = read.level;
    if(styled)
    {
        *styled = gridStyled(spot, seen);
    }
    return true;
}

int ambient::lightPoint(const glm::vec3& p, lightcache_t& cache)
{
    if(gridOn())
    {
        const int key[3] = {static_cast<int>(glm::floor(p.x / POINT_READ_SIZE)), static_cast<int>(glm::floor(p.y / POINT_READ_SIZE)),
                            static_cast<int>(glm::floor(p.z / POINT_READ_SIZE))};
        const unsigned hash = (static_cast<unsigned>(key[0]) * 73856093u) ^ (static_cast<unsigned>(key[1]) * 19349663u) ^
                              (static_cast<unsigned>(key[2]) * 83492791u);
        PointRead& read = pointReads[hash & (POINT_READS - 1)];
        if(read.frame != host_framecount || read.key[0] != key[0] || read.key[1] != key[1] || read.key[2] != key[2])
        {
            read.frame = host_framecount;
            za::copy(key, key + 3, read.key);
            read.found = gridLightAt(p, read.level);
        }
        if(read.found)
        {
            lightcolor[0] = read.level.r;
            lightcolor[1] = read.level.g;
            lightcolor[2] = read.level.b;
            return static_cast<int>((lightcolor[0] + lightcolor[1] + lightcolor[2]) * (1.f / 3.f));
        }
    }
    vec3_t v{p.x, p.y, p.z};
    return R_LightPoint(v, 0.f, &cache);
}

namespace
{

// vr_model_light_grid_test: on this map, the reads' sums and every knob; ends with its verdict line. (A development
// build's: it sets tuning constants, by the setter `tune` uses, and puts them back.)
#ifdef QVR_RELEASE
void gridTest_f()
{
    Con_Printf("vr_model_light_grid_test: a development build's\n");
}
#else
void setKnob(const Tunable& knob, float value)
{
    tunableSet(tunableIndex(knob.name), va("%g", static_cast<double>(value)));
}

void gridTest_f()
{
    static constexpr const char* GLOW_NAMES[GLOW_KINDS] = {"lava", "slime", "teleporter"};
    testkit::Tally tally;
    const auto check = [&](bool ok, const char* what, double a, double b) {
        tally.check(ok);
        Con_Printf("light grid test: %s: %s (%.3f, %.3f)\n", ok ? "ok" : "FAIL", what, a, b);
    };
    if(!ambient::gridOn() || grid.probes == 0)
    {
        check(false, "the map has a grid", 0.0, 0.0);
        tally.verdict("light grid test");
        return;
    }
    const auto gridHash = [] {
        unsigned long long h = diskcache::fnv(diskcache::fnvStart, grid.cells.data(), grid.cells.size() * sizeof(unsigned));
        return diskcache::fnv(h, grid.entries.data(), grid.entries.size() * sizeof(GridEntry));
    };
    Con_Printf("light grid test: %s, %zu probes, %zu entries, %zu lava, %zu slime, %zu teleporter\n", cl.worldmodel->name, static_cast<size_t>(grid.probes),
               static_cast<size_t>(grid.entries.size()), static_cast<size_t>(grid.glowProbes[0]), static_cast<size_t>(grid.glowProbes[1]), static_cast<size_t>(grid.glowProbes[2]));

    // Spots over the map: the middles of cells with a reading.
    za::Vector<glm::vec3> spots;
    for(int z = 0; z + 1 < grid.size[2]; z += 2)
    {
        for(int y = 0; y + 1 < grid.size[1]; y += 3)
        {
            for(int x = 0; x + 1 < grid.size[0] && spots.size() < 2000; x += 3)
            {
                const glm::vec3 p = grid.point(x, y, z) + glm::vec3{grid.cell * 0.5f};
                const GridSpot spot = gridSpot(p);
                GridRead read;
                if(!solid(p) && gridRead(spot, gridSeen(spot, p), read, false))
                {
                    spots.pushBack(p);
                }
            }
        }
    }
    check(!spots.empty(), "spots with a reading", static_cast<double>(spots.size()), 0.0);

    // What's drawn beside a model reads what the model there reads; cached in its cell within a frame; costs little.
    const za::Clock readClock;
    float worst = 0.f;
    for(const glm::vec3& p : spots)
    {
        const GridSpot spot = gridSpot(p);
        GridRead read;
        (void)gridRead(spot, gridSeen(spot, p), read);
        for(PointRead& r : pointReads)
        {
            r.frame = -1;
        }
        lightcache_t cache{};
        (void)ambient::lightPoint(p, cache);
        worst = za::max(worst, glm::distance(read.level, glm::vec3{lightcolor[0], lightcolor[1], lightcolor[2]}));
    }
    check(worst < 0.01f, "an effect's light = a model's there, largest difference", static_cast<double>(worst), 0.0);
    const double readUs = static_cast<double>(readClock.getElapsedTime().asMicroseconds()) / za::max(static_cast<int>(spots.size()), 1);
    {
        for(PointRead& r : pointReads)
        {
            r.frame = -1;
        }
        lightcache_t cache{};
        const glm::vec3 p = spots[0];
        const za::Clock clock;
        int sum = 0;
        for(int i = 0; i < 10000; i++)
        {
            sum += ambient::lightPoint(p, cache);
        }
        Con_Printf("light grid test: both reads of a spot %.2f us; a cached read %.4f us (checksum %d)\n", readUs,
                   static_cast<double>(clock.getElapsedTime().asMicroseconds()) / 10000.0, sum);
    }

    // Overall intensity: the level, live; not the probes.
    {
        const float was = vr_model_light_grid_intensity.value;
        const glm::vec3 p = spots[spots.size() / 2];
        glm::vec3 one{0.f}, two{0.f};
        (void)ambient::gridLightAt(p, one);
        setKnob(vr_model_light_grid_intensity, was * 2.f);
        (void)ambient::gridLightAt(p, two);
        setKnob(vr_model_light_grid_intensity, was);
        check(glm::distance(one * 2.f, two) < 0.05f, "intensity x2 doubles the level", static_cast<double>(luma(one)), static_cast<double>(luma(two)));
    }

    // The mix: how the cube and the map's lights' directional shading share the model.
    const entity_t* entity = cl.viewent.model ? &cl.viewent : nullptr;
    for(int i = 1; !entity && i < cl.num_entities; i++)
    {
        const entity_t* e = &cl_entities[i];
        entity = e->model && e->model->type == mod_alias && !(e->model->flags & MOD_FBRIGHTHACK) ? e : nullptr;
    }
    if(entity && gridLight(entity))
    {
        const float was = vr_model_light_grid_mix.value;
        float cube[6][4];
        const float mixes[3] = {0.f, 0.5f, 1.f};
        const float wantCube[3] = {0.f, 1.f, 1.f};
        const float wantDir[3] = {1.f, 0.5f, 0.f};
        for(int m = 0; m < 3; m++)
        {
            setKnob(vr_model_light_grid_mix, mixes[m]);
            ambient::entityCube(entity, nullptr, nullptr, true, cube);
            check(cube[0][3] == wantCube[m] && cube[1][3] == wantDir[m], "the mix's cube and directional shares", static_cast<double>(cube[0][3]), static_cast<double>(cube[1][3]));
        }
        setKnob(vr_model_light_grid_mix, was);
    }
    else
    {
        Con_Printf("light grid test: no model to read the mix from (not counted)\n");
    }

    // The near weight: another grid at once, and the same one back. Nothing else changes.
    {
        const float was = vr_model_light_grid_near.value;
        const unsigned long long before = gridHash();
        const za::SizeT entries = grid.entries.size();
        setKnob(vr_model_light_grid_near, was * 0.25f);
        (void)ambient::gridOn();
        const unsigned long long nearer = gridHash();
        const za::SizeT entriesNearer = grid.entries.size();
        setKnob(vr_model_light_grid_near, was);
        (void)ambient::gridOn();
        check(nearer != before && entriesNearer == entries, "another near weight: new values, same entries", static_cast<double>(entriesNearer), static_cast<double>(entries));
        check(gridHash() == before, "the near weight put back: the same grid", 0.0, 0.0);
    }

    // The spacing: built again at once, without a reload, every probe still read; and the same grid back. Show Light
    // Probes draws what's there.
    {
        const float was = vr_model_light_grid_cell.value;
        const unsigned long long before = gridHash();
        const za::SizeT probes = grid.probes;
        const float cell = grid.cell;
        const za::SizeT shown = gridShowProbes(spots[0]);
        setKnob(vr_model_light_grid_cell, cell * 2.f);
        (void)ambient::gridOn();
        const za::SizeT coarser = grid.probes;
        const za::SizeT shownCoarser = gridShowProbes(spots[0]);
        check(grid.cell == cell * 2.f && coarser > 0u && coarser < probes, "twice the spacing: built again, fewer probes", static_cast<double>(coarser), static_cast<double>(probes));
        check(shown > 0u && shownCoarser > 0u && shownCoarser < shown, "Show Light Probes: fewer crosses at twice the spacing", static_cast<double>(shownCoarser), static_cast<double>(shown));
        GridRead coarseRead;
        const GridSpot spot = gridSpot(spots[0]);
        const bool coarseReads = gridRead(spot, gridSeen(spot, spots[0]), coarseRead);
        check(coarseReads, "twice the spacing: a spot still reads", static_cast<double>(coarseRead.level.g), 0.0);
        setKnob(vr_model_light_grid_cell, was);
        (void)ambient::gridOn();
        check(grid.cell == cell && gridHash() == before, "the spacing put back: the same grid", static_cast<double>(grid.cell), static_cast<double>(cell));
    }

    // Off: the floor's, as R_LightPoint.
    {
        const float was = vr_model_light_grid.value;
        Cvar_SetValueQuick(&vr_model_light_grid, 0.f);
        vec3_t v{spots[0].x, spots[0].y, spots[0].z};
        lightcache_t a{}, b{};
        const int floorLight = R_LightPoint(v, 0.f, &a);
        check(ambient::lightPoint(spots[0], b) == floorLight, "vr_model_light_grid 0: effects read the floor", static_cast<double>(floorLight), 0.0);
        Cvar_SetValueQuick(&vr_model_light_grid, was);
    }

    // Each kind of lit liquid, probe by probe (a probe's own place: only it is read).
    for(int kind = 0; kind < GLOW_KINDS; kind++)
    {
        if(grid.glowProbes[kind] == 0)
        {
            Con_Printf("light grid test: no %s here\n", GLOW_NAMES[kind]);
            continue;
        }
        const Tunable& strength = *GLOW_KNOBS[kind].strength;
        const Tunable& range = *GLOW_KNOBS[kind].range;
        const float wasStrength = strength.value, wasRange = range.value;
        const float testStrength = za::max(wasStrength, 0.5f);
        int counted = 0, warmer = 0, linear = 0, rangeZero = 0, sideLit = 0;
        float nearGlow = 0.f, farGlow = 0.f;
        int nearCount = 0, farCount = 0;
        int rays = 0, raysClear = 0;
        const auto readAt = [](const GridSpot& spot, float s, const Tunable& knob) {
            setKnob(knob, s);
            GridRead r;
            (void)gridRead(spot, 1u, r);
            return r;
        };
        for(int z = 0; z + 1 < grid.size[2]; z++)
        {
            for(int y = 0; y + 1 < grid.size[1]; y++)
            {
                for(int x = 0; x + 1 < grid.size[0] && counted < 4000; x++)
                {
                    const unsigned cell = grid.cells[grid.index(x, y, z)];
                    if((cell & GRID_FIRST) == GRID_FIRST)
                    {
                        continue;
                    }
                    const GridEntry* entry = &grid.entries[cell & GRID_FIRST];
                    const int count = static_cast<int>((cell >> GRID_COUNT_SHIFT) & GRID_COUNT_MASK) + 1;
                    const GridEntry* mine = nullptr;
                    for(int k = 0; k < count; k++)
                    {
                        if((entry[k].style & GRID_GLOW) && ((entry[k].style >> GRID_GLOW_KIND_SHIFT) & 7) == kind)
                        {
                            mine = &entry[k];
                        }
                    }
                    if(!mine)
                    {
                        continue;
                    }
                    counted++;
                    const glm::vec3 p = grid.point(x, y, z);
                    GridSpot spot{};
                    spot.x = x;
                    spot.y = y;
                    spot.z = z;
                    spot.across = glm::vec3{0.f};
                    setKnob(range, wasRange);
                    const GridRead none = readAt(spot, 0.f, strength);
                    const GridRead some = readAt(spot, testStrength, strength);
                    const GridRead twice = readAt(spot, testStrength * 2.f, strength);
                    const glm::vec3 glow = some.level - none.level;
                    setKnob(strength, testStrength);
                    const GridRead rangeOff = readAt(spot, 0.f, range);
                    setKnob(range, wasRange);
                    const float distance = static_cast<float>(mine->style & GRID_GLOW_DISTANCE);
                    if(glow.r + glow.g + glow.b > 0.f || distance >= wasRange || mine->level[0] + mine->level[1] + mine->level[2] < 8)
                    {
                        warmer++;
                    }
                    linear += glm::distance(twice.level - none.level, glow * 2.f) < 0.1f ? 1 : 0;
                    rangeZero += glm::distance(rangeOff.level, none.level) < 0.01f ? 1 : 0;
                    float sideDiff = 0.f;
                    for(int f = 0; f < 6; f++)
                    {
                        sideDiff += glm::distance(some.faces[f], none.faces[f]);
                    }
                    sideLit += sideDiff > 0.f || distance >= wasRange || mine->level[0] + mine->level[1] + mine->level[2] < 8 ? 1 : 0;
                    if(distance < 96.f)
                    {
                        nearGlow += luma(glow);
                        nearCount++;
                    }
                    else if(distance >= 256.f)
                    {
                        farGlow += luma(glow);
                        farCount++;
                    }
                    // Walls: each ray that counted a liquid, by the world's own line (another tracer than the rays').
                    if((counted & 15) == 0)
                    {
                        for(int i = 0; i < GLOW_RAYS; i++)
                        {
                            const glm::vec3& dir = glowRays.dir[i];
                            Hit hit;
                            trace(cl.worldmodel->nodes, dir, p, p + dir * GLOW_REACH, hit, GLOW_STOP);
                            if((hit.kind == HitKind::Lava || hit.kind == HitKind::Liquid) && hit.surf && glowKindOf(hit.surf->flags) == kind)
                            {
                                const float along = distanceTo(hit.surf, p, dir, GLOW_REACH);
                                rays++;
                                raysClear += worldtrace::line(p, p + dir * za::max(along - 2.f, 0.f)) >= 1.f ? 1 : 0;
                            }
                        }
                    }
                }
            }
        }
        setKnob(strength, wasStrength);
        setKnob(range, wasRange);
        Con_Printf("light grid test: %s: %d probes read; near (under 96) mean %.1f of %d, far (256 up) mean %.1f of %d\n", GLOW_NAMES[kind], counted,
                   nearCount ? static_cast<double>(nearGlow) / nearCount : 0.0, nearCount, farCount ? static_cast<double>(farGlow) / farCount : 0.0, farCount);
        check(warmer == counted, "its light adds to the probes in range", static_cast<double>(warmer), static_cast<double>(counted));
        check(sideLit == counted, "and to the cube's sides", static_cast<double>(sideLit), static_cast<double>(counted));
        check(linear == counted, "strength x2 doubles it", static_cast<double>(linear), static_cast<double>(counted));
        check(rangeZero == counted, "range 0: as strength 0", static_cast<double>(rangeZero), static_cast<double>(counted));
        if(nearCount && farCount)
        {
            check(nearGlow / static_cast<float>(nearCount) > 3.f * farGlow / static_cast<float>(farCount), "falls off with distance", static_cast<double>(nearGlow) / nearCount, static_cast<double>(farGlow) / farCount);
        }
        check(rays > 0 && static_cast<float>(raysClear) >= 0.97f * static_cast<float>(rays), "walls block it: the rays that count it are clear by the world's line", static_cast<double>(raysClear), static_cast<double>(rays));
    }
    tally.verdict("light grid test");
}
#endif

} // namespace

// VR_NewMap (prewarm): the grid is built with the map's load, not at its first frame.
void ambient::prepareGrid()
{
    if(vr_model_light_grid.value != 0.f)
    {
        gridBuild();
    }
    if(!Cmd_Exists("vr_model_light_grid_info"))
    {
        Cmd_AddCommand("vr_model_light_grid_info", gridInfo_f);
        Cmd_AddCommand("vr_model_light_walk", gridWalk_f);
        Cmd_AddCommand("vr_model_light_grid_test", gridTest_f);
    }
}

void ambient::onGameDirChanged()
{
    gridCache.clear();
    gridEviction = {};
    gridCacheWorld = nullptr;
    gridCacheGeneration = -1;
    grid.world = nullptr;
    grid.generation = -1;
    grid.probes = 0;
    cache.clear();
    eviction = {};
    loadedWorld = nullptr;
    loadedGeneration = -1;
}
