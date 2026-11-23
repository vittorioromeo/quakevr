// The dynamic lights in the lightmap (vr_gl_world_dlmap; C5b, QUEST_PLAN.md 6zm): Quake's own way, for the ES
// profile (the Quest). Once a frame the texels of the world's surfaces (and the unturned brush entities') a dynamic
// light reaches get the light summed on the CPU, as GLQuake's R_AddDynamicLights did, and those blocks are uploaded
// into a texture laid out as the lightmap (C5g: its own, DlightTex; not the lightmap, whose light the shader curves:
// vr_light_contrast, the fill light, vr_light_cap); the next frame makes dark again where no light is any more. The
// world is then drawn with its form without the lights' loop (glprogs.world_unlit, VR_WorldUnlit), which adds that
// texture's light where the loop adds its own (dynamic_light: as bright, the same cap): the per-pixel loop that cost
// the Quest's fights most of their world pass (6z, 6zl) is gone, at the lightmap's 16-unit grid (the old look).
// The models, particles and everything else still take the lights per pixel (they read the lights' buffer).
// The falloffs are the shaders' (vr_dlight_falloff: DarkPlaces' or Quake's, vr_dlight_angle, a spot's cone), computed
// at each texel's place on the surface.

#include "vr_dlightmap.hpp"

#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_mem.hpp"
#include "vr_profile.hpp"
#include "vr_zancle.hpp"
#include "vr_api.h"
#include "vr_api_render.h"

#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Base/Memcpy.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sqrt.hpp"

#include <glm/glm.hpp>


extern "C" {
extern unsigned* lightmap_data; // r_brush.c: the map's lightmap texels (what lightmap_texture was made from)
extern int lightmap_width;      // r_brush.c
extern int lightmap_height;     // r_brush.c
}

namespace qvr::dlightmap
{
namespace
{

// One light as the texels take it (in the model's space: the world's, or a brush entity's).
struct Light
{
    glm::vec3 pos{0.f};
    float radius = 0.f;
    glm::vec3 color{0.f};
    float minlight = 0.f; // Quake's minlight; DarkPlaces': the share the faces turned away still get
    glm::vec4 spot{0.f};  // as gpulight_t's (zero: a point light)
};

// A block of texels written this frame or before (its place in the lightmap), to put back when no light is there.
struct Block
{
    int surf = 0; // in cl.worldmodel->surfaces
    int x = 0, y = 0, w = 0, h = 0;
};

// What the lightmap holds now beyond the map's own texels (the main thread; forgotten at a map change: its blocks are
// the old map's, and the new lightmap is made clean).
struct Written
{
    za::Vector<Block> blocks;      // the blocks with light added, as uploaded
    za::Vector<int> surfStamp;     // per surface: the frame it was last lit (index: the surface's)
    // C5c (vr_gl_world_dlmap_batch): the texture's texels as the frame leaves them (the map's own and the light added),
    // the frame's writes gathered here and sent in one upload per band of bandRows rows, not one per surface: a tiler's
    // driver copies or waits for a texture a frame in flight still reads at every upload.
    za::Vector<unsigned> atlas;
    auto members() { return mem::list(blocks, surfStamp, atlas); }
};
mem::Cache<Written> written{"dlightmap blocks", mem::MapChange};

// A frame's work (contents meaningless from one frame to the next).
struct FrameScratch
{
    za::Vector<Light> lights;        // the frame's lights, in the world's space
    za::Vector<Light> local;         // ... in a brush entity's
    za::Vector<glm::ivec2> pairs;    // (surface, light) to light
    za::Vector<Block> keep;          // the blocks still lit
    za::Vector<glm::vec3> sums;      // a surface's added light, texel by texel
    za::Vector<unsigned> texels;     // a block to upload
    auto members() { return mem::list(lights, local, pairs, keep, sums, texels); }
};
mem::Scratch<FrameScratch> scratch{"dlightmap"};

int builtFrame = -1;      // host_framecount of the last build
bool frameActive = false; // this frame's lights are in the lightmap (the views draw the world without the loop)
int frameStamp = 0;       // counts the builds (surfStamp's values)
const gltexture_t* madeFor = nullptr; // the lightmap texture the blocks were written into
double lastMs = 0.0;      // the last build's CPU time (vr_gl_world_dlmap_info)

[[nodiscard]] bool wanted()
{
    return worldWanted() && glprogs.world_unlit[0][0] && cl.worldmodel && cl.worldmodel->nodes &&
           lightmap_texture && lightmap_data && !r_fullbright_cheatsafe;
}

// The batched uploads' bands (vr_gl_world_dlmap_batch): each band's dirty rectangle this frame (x1 <= x0: clean).
constexpr int bandRows = 16;
constexpr int maxBands = 256; // 4096 rows
struct Band
{
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
};
Band bands[maxBands];
bool batching = false;  // this frame's uploads gathered into Written::atlas (set by beginUploads)
bool atlasValid = false; // Written::atlas mirrors the texture (false after an unbatched frame or a new map)
int uploadsLast = 0;    // the last frame's glTexSubImage2D calls (vr_gl_world_dlmap_info)
int uploadedTexels = 0; // ... and their texels

// C5g: the light texture (dlTexture, as big as the lightmap, laid out as it): RGB10_A2, the light over lightScale in
// each 10-bit channel (a step of 1/256 of Quake's full light: finer than the lightmap's own; up to twice its brightest).
constexpr float lightScale = 4.f;
GLuint dlTexture = 0;
int dlWidth = 0, dlHeight = 0;

[[nodiscard]] unsigned packLight(const glm::vec3& v)
{
    const auto ch = [](float c) {
        return static_cast<unsigned>(za::clamp(c * (1023.f / lightScale) + 0.5f, 0.f, 1023.f));
    };
    return ch(v.r) | (ch(v.g) << 10) | (ch(v.b) << 20) | (3u << 30);
}

void bindLightmap()
{
    // (GL_BindNative skips a texture already bound there, unit and all: the null texture first makes it select unit 0)
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, 0);
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, dlTexture);
}

// The light texture for this lightmap: made dark (a new map's, or a new size).
void makeTexture()
{
    if(dlTexture)
    {
        GL_DeleteNativeTexture(dlTexture);
        dlTexture = 0;
    }
    dlWidth = lightmap_width;
    dlHeight = lightmap_height;
    za::Vector<unsigned>& dark = scratch.texels;
    dark.assign(static_cast<size_t>(dlWidth) * static_cast<size_t>(dlHeight), 0u);
    glGenTextures(1, &dlTexture);
    bindLightmap();
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB10_A2, dlWidth, dlHeight, 0, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, dark.data());
    GL_ObjectLabelFunc(GL_TEXTURE, dlTexture, -1, "dynamic lightmap"); // (vr_vram_report)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void upload(int x, int y, int w, int h, const unsigned* texels)
{
    if(batching)
    {
        unsigned* atlas = written.atlas.data();
        for(int r = 0; r < h; r++)
        {
            ZA_MEMCPY(atlas + static_cast<size_t>(y + r) * lightmap_width + x, texels + static_cast<size_t>(r) * w,
                static_cast<size_t>(w) * sizeof(unsigned));
        }
        for(int b = y / bandRows; b <= (y + h - 1) / bandRows && b < maxBands; b++)
        {
            Band& d = bands[b];
            const int top = za::max(y, b * bandRows), bottom = za::min(y + h, (b + 1) * bandRows);
            if(d.x1 <= d.x0)
            {
                d = {x, top, x + w, bottom};
            }
            else
            {
                d = {za::min(d.x0, x), za::min(d.y0, top), za::max(d.x1, x + w), za::max(d.y1, bottom)};
            }
        }
        return;
    }
    bindLightmap();
    glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, texels);
    uploadsLast++;
    uploadedTexels += w * h;
}

// A frame's uploads begin: batched (vr_gl_world_dlmap_batch, the atlas fits the bands) into the atlas, its copy of the
// texture's texels made when it is not the texture's (a new map, or after unbatched frames).
void beginUploads()
{
    uploadsLast = uploadedTexels = 0;
    const size_t size = static_cast<size_t>(lightmap_width) * static_cast<size_t>(lightmap_height);
    batching = vr_gl_world_dlmap_batch.value != 0.f && lightmap_height <= bandRows * maxBands;
    if(!batching)
    {
        atlasValid = false;
        return;
    }
    if(!atlasValid || written.atlas.size() != size)
    {
        // (the texture is dark but for last frame's blocks; those are made dark or written again this frame)
        written.atlas.assign(size, 0u);
        atlasValid = true;
    }
    for(Band& b : bands)
    {
        b = Band{};
    }
}

// The frame's bands uploaded, each its dirty rectangle in one call.
void endUploads()
{
    if(!batching)
    {
        return;
    }
    batching = false;
    bool bound = false;
    for(const Band& b : bands)
    {
        if(b.x1 <= b.x0)
        {
            continue;
        }
        if(!bound)
        {
            bindLightmap();
            glPixelStorei(GL_UNPACK_ROW_LENGTH, lightmap_width);
            bound = true;
        }
        glTexSubImage2D(GL_TEXTURE_2D, 0, b.x0, b.y0, b.x1 - b.x0, b.y1 - b.y0, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV,
            written.atlas.data() + static_cast<size_t>(b.y0) * lightmap_width + b.x0);
        uploadsLast++;
        uploadedTexels += (b.x1 - b.x0) * (b.y1 - b.y0);
    }
    if(bound)
    {
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    }
}

// The block dark again in the light texture.
void restore(const Block& b)
{
    za::Vector<unsigned>& t = scratch.texels;
    t.assign(static_cast<size_t>(b.w) * static_cast<size_t>(b.h), 0u);
    upload(b.x, b.y, b.w, b.h, t.data());
}

void restoreAll()
{
    if(madeFor == lightmap_texture && lightmap_data)
    {
        beginUploads();
        for(const Block& b : written.blocks)
        {
            restore(b);
        }
        endUploads();
    }
    written.blocks.clear();
}

// The surfaces of the world's BSP the sphere's box touches (walked as Quake's R_MarkLights).
void collectWorld(const mnode_t* node, const glm::vec3& c, float r, int light, za::Vector<glm::ivec2>& pairs)
{
    while(node && node->contents >= 0)
    {
        const mplane_t* p = node->plane;
        const float d = (p->type < 3 ? c[p->type] : c.x * p->normal[0] + c.y * p->normal[1] + c.z * p->normal[2]) - p->dist;
        if(d > r)
        {
            node = node->children[0];
            continue;
        }
        if(d < -r)
        {
            node = node->children[1];
            continue;
        }
        const msurface_t* s = cl.worldmodel->surfaces + node->firstsurface;
        for(unsigned i = 0; i < node->numsurfaces; i++, s++)
        {
            if(c.x + r < s->mins[0] || c.x - r > s->maxs[0] || c.y + r < s->mins[1] || c.y - r > s->maxs[1] ||
                c.z + r < s->mins[2] || c.z - r > s->maxs[2])
            {
                continue;
            }
            pairs.pushBack({static_cast<int>(s - cl.worldmodel->surfaces), light});
        }
        collectWorld(node->children[0], c, r, light, pairs);
        node = node->children[1];
    }
}

[[nodiscard]] float darkplacesAtten(float d, float radius)
{
    const float k = d / za::max(radius, 1.f);
    return za::clamp((1.f - k) * 2.f / (1.f + k * k), 0.f, 1.f);
}

// A surface's light from `lights` (the pairs' lights, [first, last) in `pairs`), added to its texels and uploaded.
void lightSurface(const msurface_t* s, const Light* lights, const glm::ivec2* first, const glm::ivec2* last, bool darkplaces,
    float angle)
{
    if(!s->samples || s->styles[0] == 255 || (s->flags & (SURF_DRAWSKY | SURF_DRAWTURB)) || s->lightmaptexturenum < 0)
    {
        return;
    }
    const int smax = (s->extents[0] >> 4) + 1;
    const int tmax = (s->extents[1] >> 4) + 1;
    // The texel's place on the plane: the texture's axes and the plane solved for the point (the lightmap's (s, t) are
    // its texture coordinates at 16 units a texel, from texturemins).
    const mtexinfo_t* tex = s->texinfo;
    const glm::vec3 a{tex->vecs[0][0], tex->vecs[0][1], tex->vecs[0][2]};
    const glm::vec3 b{tex->vecs[1][0], tex->vecs[1][1], tex->vecs[1][2]};
    glm::vec3 n{s->plane->normal[0], s->plane->normal[1], s->plane->normal[2]};
    const float dist = s->plane->dist;
    const glm::mat3 rows = glm::transpose(glm::mat3(a, b, n)); // rows a, b, n
    if(za::fabs(glm::determinant(rows)) < 1e-6f)
    {
        return;
    }
    const glm::mat3 inv = glm::inverse(rows);
    const glm::vec3 p0 = inv * glm::vec3(static_cast<float>(s->texturemins[0]) - tex->vecs[0][3],
                                   static_cast<float>(s->texturemins[1]) - tex->vecs[1][3], dist);
    const glm::vec3 ds = inv * glm::vec3(16.f, 0.f, 0.f);
    const glm::vec3 dt = inv * glm::vec3(0.f, 16.f, 0.f);
    if(s->flags & SURF_PLANEBACK)
    {
        n = -n;
    }

    za::Vector<glm::vec3>& sums = scratch.sums;
    sums.assign(static_cast<size_t>(smax) * static_cast<size_t>(tmax), glm::vec3(0.f));
    bool any = false;
    for(const glm::ivec2* pr = first; pr != last; pr++)
    {
        const Light& l = lights[pr->y];
        const float r2 = l.radius * l.radius;
        for(int t = 0; t < tmax; t++)
        {
            glm::vec3 p = p0 + dt * static_cast<float>(t);
            glm::vec3* out = sums.data() + static_cast<size_t>(t) * smax;
            for(int si = 0; si < smax; si++, p += ds)
            {
                const glm::vec3 v = l.pos - p;
                const float d2 = glm::dot(v, v);
                if(d2 >= r2)
                {
                    continue;
                }
                const float d = za::sqrt(za::max(d2, 1e-12f));
                const float ndl = glm::dot(n, v) / d;
                float add;
                if(darkplaces)
                {
                    add = darkplacesAtten(d, l.radius) * 0.5f *
                          (1.f + angle * (l.minlight + (1.f - l.minlight) * za::max(ndl, 0.f) - 1.f));
                }
                else
                {
                    const float sd = glm::dot(v, n);
                    const float rad = l.radius - za::fabs(sd);
                    if(rad < l.minlight)
                    {
                        continue;
                    }
                    const float planar = za::sqrt(za::max(d2 - sd * sd, 0.f));
                    add = za::clamp((rad - l.minlight - planar) / 16.f, 0.f, 1.f) * za::max(0.f, rad - planar) / 256.f *
                          (1.f + angle * (2.f * za::max(ndl, 0.f) - 1.f));
                }
                if(l.spot.x != 0.f || l.spot.y != 0.f || l.spot.z != 0.f)
                {
                    const float k = za::clamp(l.spot.w + glm::dot(glm::vec3(l.spot), v / d), 0.f, 1.f);
                    add *= 1.f - k * k * (3.f - 2.f * k);
                }
                if(add > 0.f)
                {
                    out[si] += add * l.color;
                    any = true;
                }
            }
        }
    }
    if(!any)
    {
        return;
    }

    // C5g: the block in the light texture, at the surface's first style's place in the lightmap (the shader reads it
    // where it reads that style), the light as the per-pixel loop's dynamic_light, packed (packLight).
    const int w = smax;
    const lightmap_t& lm = lightmaps[s->lightmaptexturenum];
    const int x0 = lm.xofs + s->light_s, y0 = lm.yofs + s->light_t;
    if(x0 < 0 || y0 < 0 || x0 + w > lightmap_width || y0 + tmax > lightmap_height)
    {
        return;
    }
    za::Vector<unsigned>& texels = scratch.texels;
    texels.resize(static_cast<size_t>(w) * static_cast<size_t>(tmax));
    for(size_t i = 0; i < texels.size(); i++)
    {
        texels[i] = packLight(sums[i]);
    }
    upload(x0, y0, w, tmax, texels.data());
    const int index = static_cast<int>(s - cl.worldmodel->surfaces);
    scratch.keep.pushBack({index, x0, y0, w, tmax});
    written.surfStamp[static_cast<size_t>(index)] = frameStamp;
}

// The pairs sorted by surface, each surface lit once with its lights.
void lightPairs(const Light* lights, bool darkplaces, float angle)
{
    za::Vector<glm::ivec2>& pairs = scratch.pairs;
    za::Vector<int>& stamps = written.surfStamp;
    za::quickSort(pairs.begin(), pairs.end(), [](const glm::ivec2& p, const glm::ivec2& q) { return p.x != q.x ? p.x < q.x : p.y < q.y; });
    for(size_t i = 0; i < pairs.size();)
    {
        size_t j = i + 1;
        while(j < pairs.size() && pairs[j].x == pairs[i].x)
        {
            j++;
        }
        const int surf = pairs[i].x;
        if(surf >= 0 && static_cast<size_t>(surf) < stamps.size() && stamps[static_cast<size_t>(surf)] != frameStamp)
        {
            lightSurface(cl.worldmodel->surfaces + surf, lights, pairs.data() + i, pairs.data() + j, darkplaces, angle);
        }
        i = j;
    }
}

void build()
{
    frameStamp++;
    if(madeFor != lightmap_texture || dlWidth != lightmap_width || dlHeight != lightmap_height || !dlTexture)
    {
        written.blocks.clear(); // a new map's lightmap: a new light texture, dark
        atlasValid = false;
        madeFor = lightmap_texture;
        makeTexture();
    }
    beginUploads();
    za::Vector<int>& stamps = written.surfStamp;
    if(stamps.size() != static_cast<size_t>(cl.worldmodel->numsurfaces))
    {
        stamps.assign(static_cast<size_t>(cl.worldmodel->numsurfaces), 0);
    }
    const bool darkplaces = vr_dlight_falloff.value != 0.f;
    const float angle = za::clamp(vr_dlight_angle.value, 0.f, 1.f);

    // The frame's lights (not one view's: both eyes read the same lightmap), as R_PushDlights sends them
    za::Vector<Light>& lights = scratch.lights;
    lights.clear();
    if(r_dynamic.value)
    {
        const dlight_t* dl = cl_dlights;
        for(int i = 0; i < MAX_DLIGHTS; i++, dl++)
        {
            if(dl->spawn > cl.time || dl->die < cl.time || !dl->radius)
            {
                continue;
            }
            gpulight_t g{};
            g.pos[0] = dl->origin[0];
            g.pos[1] = dl->origin[1];
            g.pos[2] = dl->origin[2];
            g.radius = dl->radius;
            g.color[0] = dl->color[0];
            g.color[1] = dl->color[1];
            g.color[2] = dl->color[2];
            g.minlight = dl->minlight;
            VR_DlightShadow(i, &g);
            if(g.shadow2[3] < 0.f) // a rig light the walls don't take (vr_rig_lights_world 0)
            {
                continue;
            }
            Light l;
            l.pos = {g.pos[0], g.pos[1], g.pos[2]};
            l.radius = g.radius;
            l.color = {g.color[0], g.color[1], g.color[2]};
            l.minlight = g.minlight;
            l.spot = {g.spot[0], g.spot[1], g.spot[2], g.spot[3]};
            lights.pushBack(l);
        }
    }

    scratch.keep.clear();
    za::Vector<glm::ivec2>& pairs = scratch.pairs;
    pairs.clear();
    for(size_t i = 0; i < lights.size(); i++)
    {
        collectWorld(cl.worldmodel->nodes, lights[i].pos, lights[i].radius, static_cast<int>(i), pairs);
    }
    lightPairs(lights.data(), darkplaces, angle);

    // The brush entities (doors, lifts) unturned: the lights moved into their model's space
    for(int e = 0; e < cl_numvisedicts && !lights.empty(); e++)
    {
        const entity_t* ent = cl_visedicts[e];
        if(!ent || !ent->model || ent->model->type != mod_brush || ent->model == cl.worldmodel || ent->model->name[0] != '*' ||
            ent->angles[0] != 0.f || ent->angles[1] != 0.f || ent->angles[2] != 0.f)
        {
            continue;
        }
        const qmodel_t* m = ent->model;
        za::Vector<Light>& local = scratch.local;
        local.clear();
        pairs.clear();
        const glm::vec3 o{ent->origin[0], ent->origin[1], ent->origin[2]};
        for(const Light& l : lights)
        {
            Light k = l;
            k.pos -= o;
            const float r = k.radius;
            if(k.pos.x + r < m->mins[0] || k.pos.x - r > m->maxs[0] || k.pos.y + r < m->mins[1] || k.pos.y - r > m->maxs[1] ||
                k.pos.z + r < m->mins[2] || k.pos.z - r > m->maxs[2])
            {
                continue;
            }
            const int li = static_cast<int>(local.size());
            local.pushBack(k);
            const msurface_t* s = m->surfaces + m->firstmodelsurface;
            for(int i = 0; i < m->nummodelsurfaces; i++, s++)
            {
                if(k.pos.x + r < s->mins[0] || k.pos.x - r > s->maxs[0] || k.pos.y + r < s->mins[1] ||
                    k.pos.y - r > s->maxs[1] || k.pos.z + r < s->mins[2] || k.pos.z - r > s->maxs[2])
                {
                    continue;
                }
                pairs.pushBack({static_cast<int>(s - cl.worldmodel->surfaces), li});
            }
        }
        if(!local.empty())
        {
            lightPairs(local.data(), darkplaces, angle);
        }
    }

    // Last frame's blocks no light holds now: the map's texels back
    for(const Block& b : written.blocks)
    {
        if(b.surf >= 0 && static_cast<size_t>(b.surf) < stamps.size() && stamps[static_cast<size_t>(b.surf)] != frameStamp)
        {
            restore(b);
        }
    }
    written.blocks.clear();
    for(const Block& b : scratch.keep)
    {
        written.blocks.pushBack(b);
    }
    endUploads();
}

} // namespace

bool frame()
{
    if(builtFrame == host_framecount)
    {
        return frameActive;
    }
    builtFrame = host_framecount;
    if(!wanted())
    {
        if(!written.blocks.empty())
        {
            restoreAll();
        }
        frameActive = false;
        return false;
    }
    const double t0 = Sys_DoubleTime();
    build();
    frameActive = true;
    lastMs = (Sys_DoubleTime() - t0) * 1000.0;
    if(vr_perf_recording)
    {
        VR_PerfEvent(VR_PERF_LIGHTMAP, lastMs, "dynamic lights");
    }
    return true;
}

namespace
{
// vr_gl_world_dlmap_info (Debug > Profiling and Memory): what the last build did.
void info_f()
{
    Con_Printf("vr_gl_world_dlmap %g: %s; %d lights, %d texel blocks with light added, last build %.3f ms (CPU)%s\n",
        vr_gl_world_dlmap.value, frameActive ? "active" : "off",
        static_cast<int>(scratch.lights.size()), static_cast<int>(written.blocks.size()), lastMs,
        glprogs.world_unlit[0][0] ? "" : " (no unlit world form: the ES profile only)");
    Con_Printf("vr_gl_world_dlmap: %d uploads, %d texels (vr_gl_world_dlmap_batch %g)\n", uploadsLast, uploadedTexels,
        vr_gl_world_dlmap_batch.value);
}
} // namespace

void registerCommands()
{
    Cmd_AddCommand("vr_gl_world_dlmap_info", info_f);
}

} // namespace qvr::dlightmap

extern "C" unsigned VR_DlightmapTexture(void)
{
    return qvr::dlightmap::frameActive ? qvr::dlightmap::dlTexture : 0u;
}
