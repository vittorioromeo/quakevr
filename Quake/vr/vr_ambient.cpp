// vr_ambient.cpp -- see vr_ambient.hpp.

#include "vr_ambient.hpp"
#include "vr_cvars.hpp"
#include "vr_profile.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

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

const Rays& rays()
{
    static const Rays table = [] {
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
                r.weight[i][f] = std::max(0.f, glm::dot(r.dir[i], AXES[f]));
                sum += r.weight[i][f];
            }
            for(int i = 0; i < NUM_RAYS; i++)
            {
                r.weight[i][f] /= sum;
            }
        }
        return r;
    }();
    return table;
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
    const int s0 = std::min(ds >> 4, smax - 1), t0 = std::min(dt >> 4, tmax - 1);
    const int s1 = std::min(s0 + 1, smax - 1), t1 = std::min(t0 + 1, tmax - 1);
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

// What the rays from a point hit: kept, so that the light is read again at the current light
// styles (flickering and switched lights) without tracing again.
struct Probe
{
    Hit hits[NUM_RAYS];
    bool animated{false}; // a surface hit has a light style other than the normal one
};

void traceProbe(const glm::vec3& p, Probe& out)
{
    const Rays& r = rays();
    out.animated = false;
    for(int i = 0; i < NUM_RAYS; i++)
    {
        out.hits[i] = Hit{};
        trace(cl.worldmodel->nodes, r.dir[i], p, p + r.dir[i] * RAY_LENGTH, out.hits[i]);
        if(out.hits[i].kind == HitKind::Surface)
        {
            for(int m = 0; m < MAXLIGHTMAPS && out.hits[i].surf->styles[m] != 255; m++)
            {
                out.animated |= out.hits[i].surf->styles[m] != 0;
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
            light[i] = glm::vec3{std::max(luma(mean), 48.f) * 1.5f};
        }
        else if(probe.hits[i].kind == HitKind::None)
        {
            light[i] = mean;
        }
    }

    // The world's lightmap contrast (vr_light_contrast), so that the sides match how bright the
    // walls they face look.
    const float curve = std::clamp(vr_light_contrast.value, 0.5f, 3.f);
    Cube faces{};
    for(int i = 0; i < NUM_RAYS; i++)
    {
        const glm::vec3 c = 128.f * glm::pow(glm::max(light[i], glm::vec3{0.f}) / 128.f, glm::vec3{curve});
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
    const float top = std::max(avg.r, std::max(avg.g, avg.b));
    const glm::vec3 denom = glm::max(avg, glm::vec3{std::max(top * 0.3f, 1.f)});
    const float contrast = std::clamp(vr_model_ambient_contrast.value, 0.f, 2.f);
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
};

const qmodel_t* loadedWorld = nullptr;
std::unordered_map<const entity_t*, Cached> cache;
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
    std::vector<Near> nearest;
    const glm::vec3 eye{r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]};
    for(const auto& [e, c] : cache)
    {
        if(c.tracedAt >= 0.0 && c.frame >= host_framecount - 2 && (Cmd_Argc() < 2 || !VR_IsViewEntity(e)))
        {
            nearest.push_back({glm::distance(eye, c.samplePos), e, &c});
        }
    }
    std::sort(nearest.begin(), nearest.end(), [](const Near& a, const Near& b) { return a.dist < b.dist; });
    Con_Printf("AMB %lld samples, %.1f us each (%d rays), %d entities cached\n", samplesTaken,
               samplesTaken ? sampleSeconds * 1e6 / static_cast<double>(samplesTaken) : 0.0, NUM_RAYS,
               static_cast<int>(cache.size()));
    static const char* names[6] = {"+X", "-X", "+Y", "-Y", "+Z", "-Z"};
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

void ambient::entityCube(const entity_t* e, const float modelMatrix[16], const void* aliashdr, bool enabled,
                         float out[6][4])
{
    setOff(out);
    if(static bool registered = false; !registered) // a tuning command (no init hook of its own)
    {
        registered = true;
        Cmd_AddCommand("vr_model_ambient_show", show_f);
    }
    if(!enabled || vr_model_ambient_dir.value == 0.f || !cl.worldmodel || !cl.worldmodel->lightdata || !e ||
       !e->model || (e->model->flags & MOD_FBRIGHTHACK))
    {
        return;
    }
    if(cl.worldmodel != loadedWorld)
    {
        cache.clear();
        loadedWorld = cl.worldmodel;
    }
    if(budgetFrame != host_framecount)
    {
        budgetFrame = host_framecount;
        tracedThisFrame = 0;
        freshThisFrame = 0;
    }

    Cached& c = cache[e];
    if(c.frame != host_framecount)
    {
        QVR_PROFILE("model ambient");
        const bool view = e == &cl.viewent || VR_IsViewEntity(e);
        const glm::vec3 anchor = view ? glm::vec3{r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]}
                                      : glm::vec3{e->origin[0], e->origin[1], e->origin[2] + 8.f};
        const glm::vec3 p = openPoint(modelCenter(e, modelMatrix, static_cast<const aliashdr_t*>(aliashdr)), anchor);
        const bool fresh = c.tracedAt < 0.0;
        const float moved = fresh ? 0.f : glm::distance(p, c.samplePos);
        // Staggered between entities, so that they don't all come due in the same frame.
        const double stagger = static_cast<double>(reinterpret_cast<std::uintptr_t>(e) % 97) * 0.005;
        const bool due = fresh || moved > (view ? MOVE_UNITS_VIEW : MOVE_UNITS) ||
                         realtime - c.tracedAt > REFRESH_SECONDS + stagger;
        const bool allowed = fresh ? freshThisFrame < MAX_FRESH_PER_FRAME
                                   : view || tracedThisFrame < MAX_TRACED_PER_FRAME;
        bool reshade = false;
        if(due && allowed)
        {
            const auto t0 = std::chrono::steady_clock::now();
            if(fresh || moved > 128.f) // new, or teleported: no easing from where it was
            {
                traceProbe(p, c.current);
                c.blend = 1.f;
            }
            else
            {
                if(c.blend >= 0.5f) // mid-way the older of the two goes
                {
                    c.previous = c.current;
                }
                traceProbe(p, c.current);
                c.blend = 0.f;
            }
            sampleSeconds += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
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
                c.blend = 1.f - (1.f - c.blend) * std::exp(-static_cast<float>(host_frametime) * EASE_RATE);
                c.blend = c.blend > 0.995f ? 1.f : c.blend;
                reshade = true;
            }
            // Flickering or switched lights: read every frame, in step with the world's.
            const float settings = vr_model_ambient_contrast.value * 16.f + vr_light_contrast.value;
            if(reshade || c.current.animated || (c.blend < 1.f && c.previous.animated) || settings != c.settings)
            {
                c.settings = settings;
                shade(c.current, c.cube);
                if(c.blend < 1.f)
                {
                    Cube older;
                    shade(c.previous, older);
                    for(int f = 0; f < 6; f++)
                    {
                        c.cube[f] = glm::mix(older[f], c.cube[f], c.blend);
                    }
                }
            }
        }
        c.frame = host_framecount;

        // Evict entities not drawn for a while (temporary entities come and go).
        if(cache.size() > 2048)
        {
            std::erase_if(cache, [](const auto& kv) { return kv.second.frame < host_framecount - 100; });
        }
    }

    const auto it = cache.find(e);
    if(it == cache.end() || it->second.tracedAt < 0.0)
    {
        return; // not traced yet (over this frame's budget): shaded as before
    }
    for(int f = 0; f < 6; f++)
    {
        out[f][0] = it->second.cube[f].r;
        out[f][1] = it->second.cube[f].g;
        out[f][2] = it->second.cube[f].b;
    }
    out[0][3] = 1.f;
    out[1][3] = 1.f - DIR_REPLACED;
}


extern "C" void VR_AliasAmbient(const entity_t* e, const float matrix[16], const void* aliashdr, int enabled,
                                float out[24])
{
    ambient::entityCube(e, matrix, aliashdr, enabled != 0, reinterpret_cast<float(*)[4]>(out));
}
