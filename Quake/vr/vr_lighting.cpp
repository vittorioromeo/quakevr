// vr_lighting.cpp -- see vr_lighting.hpp.

#include "vr_lighting.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_gfx.hpp"
#include "vr_modellight.hpp"
#include "vr_profile.hpp"
#include "vr_stereo.hpp"
#include "vr_water.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <vector>

using namespace qvr;

namespace
{

// Must match SHADOW_FUNCTIONS in gl_shaders.h.
constexpr float shadowNear = 1.f;
constexpr float shadowBorder = 4.f;

struct Face
{
    glm::vec3 fwd, right, up;
};
constexpr Face faces[6] = {
    {{1.f, 0.f, 0.f}, {0.f, -1.f, 0.f}, {0.f, 0.f, 1.f}},
    {{-1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 0.f, 1.f}},
    {{0.f, 1.f, 0.f}, {1.f, 0.f, 0.f}, {0.f, 0.f, 1.f}},
    {{0.f, -1.f, 0.f}, {-1.f, 0.f, 0.f}, {0.f, 0.f, 1.f}},
    {{0.f, 0.f, 1.f}, {0.f, 1.f, 0.f}, {1.f, 0.f, 0.f}},
    {{0.f, 0.f, -1.f}, {0.f, -1.f, 0.f}, {1.f, 0.f, 0.f}},
};

// ----------------------------------------------------------------------------
// GL resources

GLuint depthProgram = 0;

struct DepthTarget
{
    GLuint tex = 0;
    GLuint fbo = 0;
    int width = 0;
    int height = 0;
};

DepthTarget atlas;        // rendered every frame: dynamic lights, and map lights' moving casters
DepthTarget staticAtlas;  // map lights' world depth, cached
// GPU time of the shadow pass: begin and end timestamps, a few frames in flight.
constexpr int timerFrames = 4;
GLuint timers[timerFrames][2]{};
bool timerPending[timerFrames]{};
int timerIndex = 0;
float lastMs = 0.f;

bool ensureProgram()
{
    if(depthProgram)
    {
        return true;
    }
    static const char* vs = R"(#version 430
layout(location = 0) uniform mat4 MVP;
layout(location = 0) in vec3 Pos;
void main()
{
    gl_Position = MVP * vec4(Pos, 1.0);
}
)";
    depthProgram = gfx::glProgram(vs, nullptr, "vr shadow depth");
    return depthProgram != 0;
}

void destroy(DepthTarget& t)
{
    if(t.fbo)
    {
        GL_DeleteFramebuffersFunc(1, &t.fbo);
    }
    if(t.tex)
    {
        glDeleteTextures(1, &t.tex);
    }
    t = DepthTarget{};
}

// A reversed-Z D32F depth texture that samples as a shadow map (hardware compare, bilinear).
bool ensure(DepthTarget& t, int width, int height, const char* name)
{
    if(t.tex && t.width == width && t.height == height)
    {
        return true;
    }
    destroy(t);
    glGenTextures(1, &t.tex);
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, t.tex);
    GL_TexStorage2DFunc(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT32F, width, height);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_GEQUAL); // lit where the receiver is as near or nearer
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, 0);
    GL_ObjectLabelFunc(GL_TEXTURE, t.tex, -1, name);

    GL_GenFramebuffersFunc(1, &t.fbo);
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, t.fbo);
    GL_FramebufferTexture2DFunc(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, t.tex, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    const bool complete = GL_CheckFramebufferStatusFunc(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, 0);
    if(!complete)
    {
        Con_Warning("VR lighting: %s framebuffer incomplete\n", name);
        destroy(t);
        return false;
    }
    t.width = width;
    t.height = height;
    return true;
}

// ----------------------------------------------------------------------------
// Matrices

// A shadow view: a square tile looking along fwd (right and up across it), spread the tangent of
// its half angle (inside the border). A point light has six (its cube faces, spread 1); a spot
// light one, round its cone.
struct ShadowView
{
    glm::vec3 fwd, right, up;
    float spread;
    glm::vec2 at; // the tile's corner in the atlas (texels)
};

// A point light's six faces, 3 x 2 from `origin`.
int cubeViews(glm::vec2 origin, float size, ShadowView out[6])
{
    for(int face = 0; face < 6; face++)
    {
        const Face& f = faces[face];
        out[face] = {f.fwd, f.right, f.up, 1.f,
            origin + glm::vec2{static_cast<float>(face % 3), static_cast<float>(face / 3)} * size};
    }
    return 6;
}

// A spot light's frame: must match SpotShadow in gl_shaders.h.
void spotFrame(const glm::vec3& dir, glm::vec3& right, glm::vec3& up)
{
    right = glm::normalize(glm::cross(dir, std::abs(dir.z) < 0.9f ? glm::vec3{0.f, 0.f, 1.f} : glm::vec3{1.f, 0.f, 0.f}));
    up = glm::cross(right, dir);
}

// A view's view-projection: reversed depth SHADOW_NEAR / distance along its axis, no far plane,
// widened by the border so that filtering near a tile's edge reads its own texels.
glm::mat4 viewProj(const glm::vec3& light, const ShadowView& v, float size)
{
    const float k = 1.f - 2.f * shadowBorder / size;
    glm::mat4 view{1.f};
    for(int c = 0; c < 3; c++)
    {
        view[c][0] = v.right[c];
        view[c][1] = v.up[c];
        view[c][2] = v.fwd[c];
    }
    view[3][0] = -glm::dot(v.right, light);
    view[3][1] = -glm::dot(v.up, light);
    view[3][2] = -glm::dot(v.fwd, light);

    glm::mat4 proj{0.f};
    proj[0][0] = k / v.spread;
    proj[1][1] = k / v.spread;
    proj[3][2] = shadowNear; // z' = near
    proj[2][3] = 1.f;        // w' = distance along the axis
    return proj * view;
}

// The view's frustum as Quake planes (inward), for R_CullBox on the alias models.
void viewFrustum(const glm::vec3& light, const ShadowView& v, float size, mplane_t out[4])
{
    const float k = 1.f - 2.f * shadowBorder / size;
    const glm::vec3 axis = v.fwd * (v.spread / k);
    const glm::vec3 normals[4] = {axis - v.right, axis + v.right, axis - v.up, axis + v.up};
    for(int i = 0; i < 4; i++)
    {
        const glm::vec3 n = glm::normalize(normals[i]);
        out[i].normal[0] = n.x;
        out[i].normal[1] = n.y;
        out[i].normal[2] = n.z;
        out[i].dist = glm::dot(n, light);
        out[i].type = PLANE_ANYZ;
        out[i].signbits = static_cast<byte>((n.x < 0.f ? 1 : 0) | (n.y < 0.f ? 2 : 0) | (n.z < 0.f ? 4 : 0));
    }
}

// Whether a view's pyramid (out to `radius`) can reach what the eyes see: its apex and far corners
// all outside one view frustum plane (with a margin for the other eye) means it cannot.
bool viewVisible(const glm::vec3& light, const ShadowView& v, float radius)
{
    glm::vec3 pts[5] = {light};
    int n = 1;
    for(float sx : {-1.f, 1.f})
    {
        for(float sy : {-1.f, 1.f})
        {
            pts[n++] = light + (v.fwd + (v.right * sx + v.up * sy) * v.spread) * radius;
        }
    }
    for(int p = 0; p < 4; p++)
    {
        const glm::vec3 normal{frustum[p].normal[0], frustum[p].normal[1], frustum[p].normal[2]};
        bool allOut = true;
        for(const glm::vec3& pt : pts)
        {
            if(glm::dot(normal, pt) - frustum[p].dist > -16.f)
            {
                allOut = false;
                break;
            }
        }
        if(allOut)
        {
            return false;
        }
    }
    return true;
}

// The smallest sphere round a spot light's cone out to `radius` (outer cone's cosine `c`): must
// match the light clustering in gl_shaders.h.
void spotBounds(const glm::vec3& light, const glm::vec3& dir, float radius, float c, glm::vec3& center, float& r)
{
    c = std::clamp(c, 0.f, 1.f);
    const float along = c >= 0.7071f ? radius / (2.f * c) : radius * c;
    r = std::min((c >= 0.7071f ? along : radius * std::sqrt(1.f - c * c)) * 1.01f + 1.f, radius);
    center = light + dir * along;
}

// ----------------------------------------------------------------------------
// Casters

std::vector<uint32_t> indices;

void addSurface(const msurface_t* s)
{
    if(s->flags & (SURF_DRAWSKY | SURF_DRAWTURB))
    {
        return;
    }
    for(int k = 2; k < s->numedges; k++)
    {
        indices.push_back(static_cast<uint32_t>(s->vbo_firstvert));
        indices.push_back(static_cast<uint32_t>(s->vbo_firstvert + k - 1));
        indices.push_back(static_cast<uint32_t>(s->vbo_firstvert + k));
    }
}

// The world's surfaces within `radius` of `p` (walking the BSP like R_MarkLights).
void collectWorld(const mnode_t* node, const glm::vec3& p, float radius)
{
    while(node->contents >= 0)
    {
        const mplane_t* plane = node->plane;
        const float d = p.x * plane->normal[0] + p.y * plane->normal[1] + p.z * plane->normal[2] - plane->dist;
        if(d > radius)
        {
            node = node->children[0];
            continue;
        }
        if(d < -radius)
        {
            node = node->children[1];
            continue;
        }
        const msurface_t* s = cl.worldmodel->surfaces + node->firstsurface;
        for(unsigned i = 0; i < node->numsurfaces; i++, s++)
        {
            if(p.x + radius < s->mins[0] || p.x - radius > s->maxs[0] || p.y + radius < s->mins[1] ||
                p.y - radius > s->maxs[1] || p.z + radius < s->mins[2] || p.z - radius > s->maxs[2])
            {
                continue;
            }
            addSurface(s);
        }
        collectWorld(node->children[0], p, radius);
        node = node->children[1];
    }
}

struct BrushCaster
{
    entity_t* e;
    size_t first, count; // in `indices`
};
std::vector<BrushCaster> brushCasters;
std::vector<entity_t*> aliasCasters;

bool touches(const entity_t* e, const glm::vec3& light, float radius)
{
    const glm::vec3 lo{e->model->mins[0], e->model->mins[1], e->model->mins[2]};
    const glm::vec3 hi{e->model->maxs[0], e->model->maxs[1], e->model->maxs[2]};
    const float r = std::max(glm::length(lo), glm::length(hi)) * ENTSCALE_DECODE(e->scale);
    return glm::distance(glm::vec3{e->origin[0], e->origin[1], e->origin[2]}, light) < radius + r;
}

// Doors, lifts and platforms near the light.
// With `itemsOnly`, only the brush models of items (ammo and health boxes, maps/b_*.bsp): they
// move; the map's own doors and lifts are in its baked light already.
void collectBrushes(const glm::vec3& light, float radius, bool itemsOnly)
{
    brushCasters.clear();
    for(int i = 1; i < cl.num_entities; i++)
    {
        entity_t& e = cl_entities[i];
        if(!e.model || e.model->type != mod_brush || e.model == cl.worldmodel || e.msgtime != cl.mtime[0] ||
            (itemsOnly && e.model->name[0] == '*') || !touches(&e, light, radius))
        {
            continue;
        }
        const size_t first = indices.size();
        const msurface_t* s = e.model->surfaces + e.model->firstmodelsurface;
        for(int k = 0; k < e.model->nummodelsurfaces; k++, s++)
        {
            addSurface(s);
        }
        if(indices.size() > first)
        {
            brushCasters.push_back({&e, first, indices.size() - first});
        }
    }
}

// Models near the light: monsters, items, and (for map lights, vr_shadow_self) the player's own
// body and hands. Not the light's own entity (a rocket), not see-through or shadowless ones.
void collectAliases(const glm::vec3& light, float radius, int ownEntity, bool mapLight)
{
    aliasCasters.clear();
    const int self = static_cast<int>(vr_shadow_self.value);
    for(int i = 0; i < cl_numvisedicts; i++)
    {
        entity_t* e = cl_visedicts[i];
        if(!e->model || e->model->type != mod_alias || (e->model->flags & MOD_NOSHADOW) || e->alpha != ENTALPHA_DEFAULT ||
            e == &cl_entities[ownEntity] || !touches(e, light, radius))
        {
            continue;
        }
        if(VR_IsViewEntity(e))
        {
            const bool body = strstr(e->model->name, "vrbody") != nullptr;
            if(!mapLight || self <= 0 || (self == 1 && !body))
            {
                continue;
            }
        }
        aliasCasters.push_back(e);
    }
}

// ----------------------------------------------------------------------------
// Drawing

int facesDrawn = 0;
int modelsDrawn = 0; // model draws, over all faces
float cpuMs = 0.f;

void drawIndices(const glm::mat4& mvp, size_t first, size_t count)
{
    if(!count)
    {
        return;
    }
    GL_UniformMatrix4fvFunc(0, 1, GL_FALSE, &mvp[0][0]);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(count), GL_UNSIGNED_INT,
        reinterpret_cast<const void*>(first * sizeof(uint32_t)));
}

// Renders a light's views (tiles of `size` texels) in `target`: `worldCount` indices of the world
// (from 0), the brush casters, and the alias casters.
void renderLight(DepthTarget& target, const glm::vec3& light, float radius, const ShadowView* views, int numViews, float size,
    size_t worldCount, bool brushes, bool aliases)
{
    const bool anyGeometry = !indices.empty();
    GLuint ibuf = 0;
    GLbyte* iofs = nullptr;
    if(anyGeometry)
    {
        GL_Upload(GL_ELEMENT_ARRAY_BUFFER, indices.data(), indices.size() * sizeof(uint32_t), &ibuf, &iofs);
    }

    GL_BindFramebufferFunc(GL_FRAMEBUFFER, target.fbo);
    for(int face = 0; face < numViews; face++)
    {
        const ShadowView& view = views[face];
        if(!viewVisible(light, view, radius))
        {
            continue;
        }
        glViewport(static_cast<int>(view.at.x), static_cast<int>(view.at.y), static_cast<int>(size), static_cast<int>(size));
        const glm::mat4 vp = viewProj(light, view, size);
        facesDrawn++;

        if(anyGeometry && (worldCount || (brushes && !brushCasters.empty())))
        {
            GL_UseProgram(depthProgram);
            GL_SetState(GLS_BLEND_OPAQUE | GLS_CULL_NONE | GLS_ATTRIBS(1));
            GL_BindBuffer(GL_ARRAY_BUFFER, gl_bmodel_vbo);
            GL_VertexAttribPointerFunc(0, 3, GL_FLOAT, GL_FALSE, sizeof(glvert_t), nullptr);
            GL_BindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibuf);
            const size_t base = reinterpret_cast<uintptr_t>(iofs) / sizeof(uint32_t);
            drawIndices(vp, base, worldCount);
            if(brushes)
            {
                for(const BrushCaster& b : brushCasters)
                {
                    float m[16];
                    // As R_DrawBrushModels draws it: pitch inverted, then the networked scale and
                    // offset (ammo and health boxes are drawn at a quarter of their size).
                    vec3_t angles{-b.e->angles[0], b.e->angles[1], b.e->angles[2]};
                    R_EntityMatrix(m, b.e->origin, angles, b.e->scale);
                    VR_BrushTransform(b.e, m);
                    glm::mat4 model;
                    memcpy(&model[0][0], m, sizeof(m));
                    drawIndices(vp * model, base + b.first, b.count);
                }
            }
        }

        if(aliases && !aliasCasters.empty())
        {
            // The alias renderer reads the camera's view-projection and frustum: this face's.
            float savedViewProj[16];
            mplane_t savedFrustum[4];
            memcpy(savedViewProj, r_matviewproj, sizeof(savedViewProj));
            memcpy(savedFrustum, frustum, sizeof(savedFrustum));
            memcpy(r_matviewproj, &vp[0][0], sizeof(r_matviewproj));
            viewFrustum(light, view, size, frustum);
            R_DrawAliasModels(aliasCasters.data(), static_cast<int>(aliasCasters.size()));
            modelsDrawn += static_cast<int>(aliasCasters.size());
            memcpy(r_matviewproj, savedViewProj, sizeof(savedViewProj));
            memcpy(frustum, savedFrustum, sizeof(savedFrustum));
        }
    }
}

// ----------------------------------------------------------------------------
// Light selection

struct DlightSlot
{
    bool selected = false;
    bool spot = false; // one tile round its cone instead of six faces
    float size = 0.f;
    glm::vec2 origin{0.f};
};
std::array<DlightSlot, MAX_DLIGHTS> dlightSlots;

// Spot lights (lighting::dlightSpot): valid while the slot holds the same light (its key and
// death time).
struct Spot
{
    int key = 0;
    float die = -1.f;
    glm::vec3 dir{1.f, 0.f, 0.f};
    float cosInner = 1.f, cosOuter = 1.f;
};
std::array<Spot, MAX_DLIGHTS> spots;

[[nodiscard]] const Spot* spotOf(int index)
{
    if(index < 0 || index >= MAX_DLIGHTS)
    {
        return nullptr;
    }
    const Spot& s = spots[index];
    return s.key == cl_dlights[index].key && s.die == cl_dlights[index].die ? &s : nullptr;
}

// The tangent of a spot's shadow tile's half angle: its outer cone, and a little more (the
// normal offset and the filter reach past the cone's edge).
[[nodiscard]] float spotSpread(const Spot& s)
{
    const float c = std::clamp(s.cosOuter, 0.1f, 1.f);
    return std::sqrt(1.f - c * c) / c * 1.06f + 0.01f;
}

// Lights marked to cast no shadow (lighting::dlightNoShadow): valid while the slot holds the same
// light (its key and death time).
struct NoShadow
{
    int key = 0;
    float die = -1.f;
};
std::array<NoShadow, MAX_DLIGHTS> noShadows;

struct MapSlot
{
    int light = -1;       // index in modellight::mapLights()
    float fade = 0.f;     // 0..1
    bool wanted = false;
    bool cached = false;  // world depth rendered in staticAtlas
    bool hasCasters = false;
    glm::vec2 staticOrigin{0.f};
    glm::vec2 origin{0.f}; // this frame's moving casters, in the atlas
};
std::vector<MapSlot> mapSlots;
float mapSlotSize = 0.f;
const qmodel_t* slotsWorld = nullptr;

// A light to shadow, and how much it matters (the greater the more).
struct Candidate
{
    int index;
    float score;
};

[[nodiscard]] float pow2Floor(float v)
{
    return std::exp2(std::floor(std::log2(std::max(v, 1.f))));
}

// The viewer's PVS, or null for everything visible (Mod_LeafPVS decompresses it on every call).
[[nodiscard]] const byte* viewPVS()
{
    if(!r_viewleaf || r_viewleaf->contents == CONTENTS_SOLID)
    {
        return nullptr;
    }
    return Mod_LeafPVS(r_viewleaf, cl.worldmodel);
}

bool lightVisible(const glm::vec3& p, const byte* vis)
{
    if(!vis)
    {
        return true;
    }
    vec3_t v{p.x, p.y, p.z};
    const mleaf_t* leaf = Mod_PointInLeaf(v, cl.worldmodel);
    const int index = static_cast<int>(leaf - cl.worldmodel->leafs) - 1;
    if(index < 0)
    {
        return true;
    }
    return (vis[index >> 3] & (1 << (index & 7))) != 0;
}

void selectDlights(const glm::vec3& eye)
{
    const int maxShadowed = static_cast<int>(vr_shadow_dlights.value);
    const float maxSize = pow2Floor(std::clamp(vr_shadow_dlight_size.value, 64.f, 2048.f));
    static std::vector<Candidate> candidates;
    candidates.clear();
    for(int i = 0; i < MAX_DLIGHTS; i++)
    {
        const dlight_t& l = cl_dlights[i];
        DlightSlot& slot = dlightSlots[i];
        const bool alive = l.die >= cl.time && l.radius > 0.f && l.spawn <= cl.time;
        if(!alive || maxShadowed <= 0 || (noShadows[i].key == l.key && noShadows[i].die == l.die))
        {
            slot.selected = false;
            continue;
        }
        const glm::vec3 p{l.origin[0], l.origin[1], l.origin[2]};
        const float dist = glm::distance(p, eye);
        if(dist - l.radius > vr_shadow_distance.value)
        {
            slot.selected = false;
            continue;
        }
        // Muzzle flashes are brief and right by the viewer's gun: last, if at all.
        const bool muzzle = l.key == cl.viewentity && l.die - cl.time <= 0.11;
        if(muzzle && !vr_shadow_muzzleflash.value)
        {
            slot.selected = false;
            continue;
        }
        float score = l.radius / std::max(dist, l.radius * 0.25f);
        if(muzzle)
        {
            score *= 0.25f;
        }
        if(slot.selected)
        {
            score *= 1.25f; // hysteresis
        }
        candidates.push_back({i, score});
    }
    std::sort(candidates.begin(), candidates.end(), [](auto& a, auto& b) { return a.score > b.score; });

    std::array<bool, MAX_DLIGHTS> chosen{};
    for(size_t c = 0; c < candidates.size() && static_cast<int>(c) < maxShadowed; c++)
    {
        chosen[candidates[c].index] = true;
    }
    for(int i = 0; i < MAX_DLIGHTS; i++)
    {
        DlightSlot& slot = dlightSlots[i];
        if(!chosen[i])
        {
            slot.selected = false;
            continue;
        }
        // DarkPlaces' level of detail: about a texel per unit of radius close by, less farther. A
        // spot light's one tile gets twice a face's size (still a third of a cube's texels, over
        // its narrow cone).
        const dlight_t& l = cl_dlights[i];
        const bool spot = spotOf(i) != nullptr && spotSpread(*spotOf(i)) < 2.f;
        const float tileMax = spot ? std::min(2.f * maxSize, 2048.f) : maxSize;
        const float dist = glm::distance(glm::vec3{l.origin[0], l.origin[1], l.origin[2]}, eye);
        const float want = l.radius * vr_shadow_precision.value / std::sqrt(std::max(1.f, dist / l.radius)) * (spot ? 2.f : 1.f);
        float size = std::clamp(pow2Floor(want), 64.f, tileMax);
        if(slot.selected && slot.spot == spot && slot.size > 0.f && want > slot.size * 0.7f && want < slot.size * 2.8f)
        {
            size = std::min(slot.size, tileMax); // hysteresis: keep the size unless it is well off
        }
        slot.selected = true;
        slot.spot = spot;
        slot.size = size;
    }
}

void selectMapLights(const glm::vec3& eye, float dt)
{
    int wanted = static_cast<int>(vr_shadow_maplights.value);
    const float size = pow2Floor(std::clamp(vr_shadow_maplight_size.value, 64.f, 2048.f));
    // The world depth cache: slots of 3 x 2 faces, in at most 8192 x 8192.
    const int columns = std::max(1, static_cast<int>(8192.f / (3.f * size)));
    const int rows = std::max(1, static_cast<int>(8192.f / (2.f * size)));
    wanted = std::clamp(wanted, 0, columns * rows);

    if(cl.worldmodel != slotsWorld || size != mapSlotSize || static_cast<int>(mapSlots.size()) != wanted)
    {
        mapSlots.assign(static_cast<size_t>(wanted), MapSlot{});
        mapSlotSize = size;
        slotsWorld = cl.worldmodel;
        for(int s = 0; s < wanted; s++)
        {
            mapSlots[s].staticOrigin = {static_cast<float>(s % columns) * 3.f * size, static_cast<float>(s / columns) * 2.f * size};
        }
    }
    if(mapSlots.empty())
    {
        destroy(staticAtlas);
        return;
    }

    // The lights whose light reaches near the viewer (and that the viewer's leaf can see), the
    // brightest there first; the ones already shown keep their place unless clearly beaten.
    const auto& lights = modellight::mapLights();
    const byte* vis = viewPVS();
    static std::vector<Candidate> candidates;
    candidates.clear();
    for(int i = 0; i < static_cast<int>(lights.size()); i++)
    {
        const auto& l = lights[i];
        const float dist = glm::distance(l.pos, eye);
        const float reach = l.value / l.scale;
        if(dist > reach + 128.f || dist - reach > vr_shadow_distance.value || !lightVisible(l.pos, vis))
        {
            continue;
        }
        float score = l.value + 128.f - dist * l.scale;
        for(const MapSlot& s : mapSlots)
        {
            if(s.light == i && s.wanted)
            {
                score *= 1.3f;
            }
        }
        candidates.push_back({i, score});
    }
    std::sort(candidates.begin(), candidates.end(), [](auto& a, auto& b) { return a.score > b.score; });
    if(static_cast<int>(candidates.size()) > wanted)
    {
        candidates.resize(static_cast<size_t>(wanted));
    }

    for(MapSlot& s : mapSlots)
    {
        s.wanted = std::any_of(candidates.begin(), candidates.end(), [&](auto& c) { return c.index == s.light; });
    }
    for(const Candidate& c : candidates)
    {
        if(std::any_of(mapSlots.begin(), mapSlots.end(), [&](auto& s) { return s.light == c.index; }))
        {
            continue;
        }
        for(MapSlot& s : mapSlots)
        {
            if(s.light < 0)
            {
                s.light = c.index;
                s.wanted = true;
                s.cached = false;
                s.fade = 0.f;
                break;
            }
        }
    }
    // Fade in and out over a quarter of a second; a slot is freed once faded out.
    for(MapSlot& s : mapSlots)
    {
        if(s.light < 0)
        {
            continue;
        }
        s.fade = std::clamp(s.fade + (s.wanted ? dt : -dt) * 4.f, 0.f, 1.f);
        if(!s.wanted && s.fade <= 0.f)
        {
            s.light = -1;
            s.cached = false;
        }
    }
}

// ----------------------------------------------------------------------------
// Atlas packing

struct Request
{
    float size;
    glm::vec2* origin;
    float* packedSize; // the face size it was packed at, if wanted
    float columns = 3.f, rows = 2.f; // in faces: a point light's 3 x 2, a spot light's one tile
};

// Rows of blocks (a point light's 3 x 2 faces, a spot light's one tile), tallest first; when they
// do not fit, everything is halved and packed again (as DarkPlaces does), down to 32-texel faces.
// Returns the scale they were all packed at, 0 if they did not fit.
float pack(std::vector<Request>& requests, int atlasSize)
{
    std::sort(requests.begin(), requests.end(), [](auto& a, auto& b) { return a.size * a.rows > b.size * b.rows; });
    for(float scale = 1.f; scale >= 1.f / 16.f; scale *= 0.5f)
    {
        float x = 0.f, y = 0.f, rowHeight = 0.f;
        bool fits = true;
        for(Request& r : requests)
        {
            const float s = std::max(32.f, r.size * scale);
            if(x + r.columns * s > static_cast<float>(atlasSize))
            {
                x = 0.f;
                y += rowHeight;
                rowHeight = 0.f;
            }
            if(y + r.rows * s > static_cast<float>(atlasSize))
            {
                fits = false;
                break;
            }
            *r.origin = {x, y};
            x += r.columns * s;
            rowHeight = std::max(rowHeight, r.rows * s);
        }
        if(fits)
        {
            for(const Request& r : requests)
            {
                if(r.packedSize)
                {
                    *r.packedSize = std::max(32.f, r.size * scale);
                }
            }
            return scale;
        }
    }
    return 0.f;
}

bool frameEnabled = false;
int renderedFrame = -1;
double lastTime = 0.0;

bool shadowsSupported()
{
    static bool warned = false;
    if(!gl_clipcontrol_able)
    {
        if(!warned)
        {
            Con_Warning("VR lighting: shadows need GL_ARB_clip_control\n");
            warned = true;
        }
        return false;
    }
    return true;
}

} // namespace

// ----------------------------------------------------------------------------
// Engine hooks

// R_SetupView, before R_PushDlights: once per frame (both eyes share it).
extern "C" void VR_RenderShadowMaps(void)
{
    if(renderedFrame == host_framecount)
    {
        return;
    }
    renderedFrame = host_framecount;
    QVR_GPU_PROFILE("shadow maps");
    const double cpuStart = Sys_DoubleTime();
    const float dt = static_cast<float>(std::clamp(realtime - lastTime, 0.0, 0.1));
    lastTime = realtime;

    frameEnabled = (vr_shadow_dlights.value > 0.f || vr_shadow_maplights.value > 0.f) && cl.worldmodel &&
                   r_drawworld_cheatsafe && shadowsSupported() && ensureProgram();
    if(!frameEnabled)
    {
        for(DlightSlot& s : dlightSlots)
        {
            s.selected = false;
        }
        mapSlots.clear();
        slotsWorld = nullptr;
        destroy(atlas);
        destroy(staticAtlas);
        return;
    }

    const glm::vec3 eye{r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]};
    selectDlights(eye);
    selectMapLights(eye, dt);

    // Moving casters of the map lights: none near means nothing to draw (and no light entry).
    const auto& lights = modellight::mapLights();
    for(MapSlot& s : mapSlots)
    {
        s.hasCasters = false;
        if(s.light >= 0 && s.light < static_cast<int>(lights.size()))
        {
            const auto& l = lights[s.light];
            indices.clear();
            collectAliases(l.pos, l.value / l.scale, 0, true);
            collectBrushes(l.pos, l.value / l.scale, true);
            s.hasCasters = !aliasCasters.empty() || !brushCasters.empty();
        }
    }

    // Pack this frame's faces.
    const int atlasSize = static_cast<int>(pow2Floor(std::clamp(vr_shadow_atlas.value, 1024.f, 8192.f)));
    static std::vector<Request> requests;
    requests.clear();
    for(DlightSlot& slot : dlightSlots)
    {
        if(slot.selected)
        {
            requests.push_back({slot.size, &slot.origin, &slot.size, slot.spot ? 1.f : 3.f, slot.spot ? 1.f : 2.f});
        }
    }
    for(MapSlot& s : mapSlots)
    {
        if(s.light >= 0 && s.hasCasters)
        {
            requests.push_back({mapSlotSize, &s.origin, nullptr});
        }
    }
    // What does not fit casts no shadow this frame; the map lights' moving casters must also
    // match their cached world faces' size (not scaled down).
    const float packScale = pack(requests, atlasSize);
    for(DlightSlot& slot : dlightSlots)
    {
        slot.selected = slot.selected && packScale > 0.f;
    }
    for(MapSlot& s : mapSlots)
    {
        s.hasCasters = s.hasCasters && packScale == 1.f;
    }

    if(!ensure(atlas, atlasSize, atlasSize, "shadow atlas"))
    {
        frameEnabled = false;
        return;
    }
    if(!mapSlots.empty())
    {
        const int columns = std::max(1, static_cast<int>(8192.f / (3.f * mapSlotSize)));
        const int used = static_cast<int>(mapSlots.size());
        const int w = static_cast<int>(3.f * mapSlotSize) * std::min(columns, used);
        const int h = static_cast<int>(2.f * mapSlotSize) * ((used + columns - 1) / columns);
        if(staticAtlas.width != w || staticAtlas.height != h)
        {
            for(MapSlot& s : mapSlots)
            {
                s.cached = false;
            }
        }
        if(!ensure(staticAtlas, w, h, "shadow static atlas"))
        {
            mapSlots.clear();
        }
    }

    // Render.
    const bool timing = vr_shadow_stats.value != 0.f;
    if(timing)
    {
        if(!timers[0][0])
        {
            GL_GenQueriesFunc(timerFrames * 2, &timers[0][0]);
        }
        for(int f = 0; f < timerFrames; f++)
        {
            GLint available = 0;
            if(timerPending[f] && (GL_GetQueryObjectivFunc(timers[f][1], GL_QUERY_RESULT_AVAILABLE, &available), available))
            {
                GLuint64 begin = 0, end = 0;
                GL_GetQueryObjectui64vFunc(timers[f][0], GL_QUERY_RESULT, &begin);
                GL_GetQueryObjectui64vFunc(timers[f][1], GL_QUERY_RESULT, &end);
                lastMs = static_cast<float>(end - begin) / 1e6f;
                timerPending[f] = false;
            }
        }
        if(!timerPending[timerIndex])
        {
            GL_QueryCounterFunc(timers[timerIndex][0], GL_TIMESTAMP);
        }
    }

    GL_BeginGroup("Shadow maps");
    facesDrawn = 0;
    modelsDrawn = 0;
    glEnable(GL_SCISSOR_TEST);

    profile::begin("map light world", true);
    // Map lights' world depth, once per light (the world does not move).
    for(MapSlot& s : mapSlots)
    {
        if(s.light < 0 || s.cached || s.light >= static_cast<int>(lights.size()))
        {
            continue;
        }
        const auto& l = lights[s.light];
        GL_BindFramebufferFunc(GL_FRAMEBUFFER, staticAtlas.fbo);
        glScissor(static_cast<int>(s.staticOrigin.x), static_cast<int>(s.staticOrigin.y), static_cast<int>(3.f * mapSlotSize),
            static_cast<int>(2.f * mapSlotSize));
        GL_SetState(glstate & ~GLS_NO_ZWRITE);
        glClearDepth(0.f);
        glClear(GL_DEPTH_BUFFER_BIT);
        indices.clear();
        collectWorld(cl.worldmodel->nodes, l.pos, l.value / l.scale);
        // All six faces: the cache must not depend on where the viewer looks.
        mplane_t saved[4];
        memcpy(saved, frustum, sizeof(saved));
        for(mplane_t& p : frustum)
        {
            p.dist = -1e9f;
        }
        ShadowView views[6];
        renderLight(staticAtlas, l.pos, l.value / l.scale, views, cubeViews(s.staticOrigin, mapSlotSize, views), mapSlotSize,
            indices.size(), false, false);
        memcpy(frustum, saved, sizeof(saved));
        s.cached = true;
    }
    profile::end();

    profile::begin("atlas clear", true);
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, atlas.fbo);
    glScissor(0, 0, atlas.width, atlas.height);
    GL_SetState(glstate & ~GLS_NO_ZWRITE);
    glClearDepth(0.f);
    glClear(GL_DEPTH_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
    profile::end();

    profile::begin("dlight shadows", true);
    // Dynamic lights: the world, doors and lifts, and models.
    for(int i = 0; i < MAX_DLIGHTS; i++)
    {
        if(!dlightSlots[i].selected)
        {
            continue;
        }
        const dlight_t& l = cl_dlights[i];
        const DlightSlot& slot = dlightSlots[i];
        const glm::vec3 p{l.origin[0], l.origin[1], l.origin[2]};
        // A spot light's casters: those round its cone.
        glm::vec3 center = p;
        float reach = l.radius;
        ShadowView views[6];
        int numViews = 0;
        if(const Spot* spot = spotOf(i); slot.spot && spot)
        {
            spotBounds(p, spot->dir, l.radius, spot->cosOuter, center, reach);
            ShadowView& v = views[numViews++];
            v.fwd = spot->dir;
            spotFrame(spot->dir, v.right, v.up);
            v.spread = spotSpread(*spot);
            v.at = slot.origin;
        }
        else
        {
            numViews = cubeViews(slot.origin, slot.size, views);
        }
        indices.clear();
        collectWorld(cl.worldmodel->nodes, center, reach);
        const size_t worldCount = indices.size();
        collectBrushes(center, reach, false);
        collectAliases(center, reach, l.key > 0 && l.key < cl.num_entities ? l.key : 0, false);
        renderLight(atlas, p, l.radius, views, numViews, slot.size, worldCount, true, true);
    }

    profile::end();

    profile::begin("map light shadows", true);
    // Map lights: the moving things only.
    for(MapSlot& s : mapSlots)
    {
        if(s.light < 0 || !s.hasCasters || s.light >= static_cast<int>(lights.size()))
        {
            continue;
        }
        const auto& l = lights[s.light];
        indices.clear();
        collectAliases(l.pos, l.value / l.scale, 0, true);
        collectBrushes(l.pos, l.value / l.scale, true);
        ShadowView views[6];
        renderLight(atlas, l.pos, l.value / l.scale, views, cubeViews(s.origin, mapSlotSize, views), mapSlotSize, 0, true, true);
    }

    profile::end();

    GL_BindFramebufferFunc(GL_FRAMEBUFFER, 0);
    GL_BindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    GL_EndGroup();

    cpuMs = cpuMs * 0.9f + static_cast<float>((Sys_DoubleTime() - cpuStart) * 1000.0) * 0.1f;
    if(timing)
    {
        if(!timerPending[timerIndex])
        {
            GL_QueryCounterFunc(timers[timerIndex][1], GL_TIMESTAMP);
            timerPending[timerIndex] = true;
        }
        timerIndex = (timerIndex + 1) % timerFrames;

        static double lastPrint = 0.0;
        if(realtime - lastPrint > 1.0)
        {
            lastPrint = realtime;
            int dl = 0, ml = 0;
            for(const DlightSlot& s : dlightSlots)
            {
                dl += s.selected ? 1 : 0;
            }
            for(const MapSlot& s : mapSlots)
            {
                ml += (s.light >= 0 && s.hasCasters) ? 1 : 0;
            }
            Con_Printf("shadows: %d dynamic, %d map lights, %d faces, %d models; GPU %.2f ms, CPU %.2f ms\n", dl, ml, facesDrawn,
                modelsDrawn, lastMs, cpuMs);
        }
    }
}

namespace
{

// What VR_TuneDlight gave a dynamic light beyond Quake's fields, for DarkPlaces' lights: the ambient
// share (what faces away still gets) and the time its colour fades out over. Valid while the slot
// holds the same light (its key and death time).
struct DlightLook
{
    int key = 0;
    float die = -1.f;
    float ambient = 0.f;
    float fade = 0.f;
};
DlightLook dlightLooks[MAX_DLIGHTS];

// With DarkPlaces' falloff, a light's minlight is its ambient share (the shaders read it so), and a
// flash's colour fades out. Lights Quake VR does not tune (EF_DIMLIGHT, EF_BRIGHTLIGHT, the powerups'
// glows) get DarkPlaces' brightness for them: 1.5, 3 for the big ones.
void darkplacesLight(int index, gpulight_t* out)
{
    const dlight_t& dl = cl_dlights[index];
    const DlightLook& look = dlightLooks[index];
    if(look.key == dl.key && look.die == dl.die)
    {
        out->minlight = look.ambient;
        if(look.fade > 0.f)
        {
            const float k = std::clamp(static_cast<float>((dl.die - cl.time) / look.fade), 0.f, 1.f);
            for(float& c : out->color)
            {
                c *= k;
            }
        }
        return;
    }
    out->minlight = 0.f;
    const float k = dl.radius >= 400.f ? 3.f : 1.5f;
    for(float& c : out->color)
    {
        c *= k;
    }
}

} // namespace

// R_PushDlights, for each dynamic light it sends: its shadow's faces, if it has one.
extern "C" void VR_DlightShadow(int index, gpulight_t* out)
{
    memset(out->shadow, 0, sizeof(out->shadow));
    memset(out->shadow2, 0, sizeof(out->shadow2));
    memset(out->spot, 0, sizeof(out->spot));
    const Spot* spot = spotOf(index);
    if(spot)
    {
        // The cone as the shaders read it: 1 - smoothstep(0, 1, w - dot(xyz, the direction to the point)).
        const float s = 1.f / std::max(spot->cosInner - spot->cosOuter, 1e-3f);
        out->spot[0] = spot->dir.x * s;
        out->spot[1] = spot->dir.y * s;
        out->spot[2] = spot->dir.z * s;
        out->spot[3] = spot->cosInner * s;
    }
    if(frameEnabled && index >= 0 && index < MAX_DLIGHTS && dlightSlots[index].selected)
    {
        out->shadow[0] = dlightSlots[index].origin.x;
        out->shadow[1] = dlightSlots[index].origin.y;
        out->shadow[2] = dlightSlots[index].size;
        if(dlightSlots[index].spot && spot)
        {
            out->shadow2[0] = spotSpread(*spot); // one tile round its cone
        }
    }
    if(vr_dlight_falloff.value != 0.f && index >= 0 && index < MAX_DLIGHTS)
    {
        darkplacesLight(index, out);
    }
}

void lighting::dlightLook(const dlight_t* dl, float ambient, float fade)
{
    const std::ptrdiff_t index = dl - cl_dlights;
    if(index < 0 || index >= MAX_DLIGHTS)
    {
        return;
    }
    dlightLooks[index] = DlightLook{dl->key, dl->die, ambient, fade};
}

void lighting::dlightSpot(const dlight_t* dl, const glm::vec3& dir, float innerDegrees, float outerDegrees)
{
    const std::ptrdiff_t index = dl - cl_dlights;
    const float len = glm::length(dir);
    if(index < 0 || index >= MAX_DLIGHTS || len < 1e-6f)
    {
        return;
    }
    const float outer = std::clamp(outerDegrees, 1.f, 179.f);
    const float inner = std::clamp(innerDegrees, 0.f, outer - 0.5f);
    spots[index] = Spot{dl->key, dl->die, dir / len, std::cos(glm::radians(inner)), std::cos(glm::radians(outer))};
}

extern "C" float VR_SpotCone(const gpulight_t* l, const float point[3])
{
    const glm::vec3 axis{l->spot[0], l->spot[1], l->spot[2]};
    const glm::vec3 d{point[0] - l->pos[0], point[1] - l->pos[1], point[2] - l->pos[2]};
    const float len = glm::length(d);
    if(glm::dot(axis, axis) <= 0.f || len < 1e-3f)
    {
        return 1.f;
    }
    const float t = std::clamp(l->spot[3] - glm::dot(axis, d) / len, 0.f, 1.f);
    return 1.f - t * t * (3.f - 2.f * t);
}

void lighting::dlightNoShadow(const dlight_t* dl)
{
    const std::ptrdiff_t index = dl - cl_dlights;
    if(index >= 0 && index < MAX_DLIGHTS)
    {
        noShadows[index] = NoShadow{dl->key, dl->die};
    }
}

// Models are lit from the lightmap at their feet (R_LightPoint: 128 is Quake's full light): the
// same contrast as the world's.
extern "C" void VR_AliasLightCurve(float lightcolor[3])
{
    const float c = std::clamp(vr_light_contrast.value, 0.5f, 3.f);
    if(c == 1.f)
    {
        return;
    }
    for(int i = 0; i < 3; i++)
    {
        lightcolor[i] = 128.f * std::pow(std::max(0.f, lightcolor[i]) / 128.f, c);
    }
}

// R_PushDlights, after the dynamic lights: the map lights with moving casters, the frame's
// lighting settings, and the atlases on texture units 4 and 5.
extern "C" void VR_PushMapLights(void)
{
    unsigned flags = static_cast<unsigned>(std::clamp(static_cast<int>(vr_shadow_filter.value), 0, 3));
    if(vr_dlight_uncapped.value)
    {
        flags |= 4u;
    }
    if(vr_dlight_models.value)
    {
        flags |= 8u;
    }
    if(vr_dlight_falloff.value)
    {
        flags |= 16u;
    }
    if(vr_model_light_parity.value)
    {
        flags |= 32u;
    }
    r_framedata.shadowflags = static_cast<int>(flags);
    // Lightmap contrast about Quake's full light (a lightmap value of a half, before the doubling):
    // shade darker, well lit walls as they were, the brightest a little brighter.
    r_framedata.lighttweak[0] = std::clamp(vr_light_contrast.value, 0.5f, 3.f);
    // How much the normal maps shade the baked light (from a direction the shader guesses from the lightmap).
    r_framedata.lighttweak[1] = std::clamp(vr_normalmap_baked.value, 0.f, 2.f);
    // Dynamic lights' sheen, and how deep the normal maps' bumps are (0: flat).
    r_framedata.lighttweak[2] = std::clamp(vr_specular.value, 0.f, 4.f);
    r_framedata.lighttweak[3] = vr_normalmaps.value != 0.f ? std::clamp(vr_normalmap_strength.value, 0.f, 8.f) : 0.f;
    // Parallax occlusion mapping on the world (the heights are in the normal maps' alpha): how deep, how far it
    // reaches, and the most steps along a ray.
    const bool parallax = vr_parallax.value != 0.f && vr_normalmaps.value != 0.f;
    r_framedata.parallax[0] = parallax ? std::clamp(vr_parallax_depth.value, 0.f, 16.f) : 0.f;
    r_framedata.parallax[1] = std::clamp(vr_parallax_distance.value, 64.f, 4096.f);
    r_framedata.parallax[2] = std::clamp(std::round(vr_parallax_steps.value), 4.f, 64.f);
    r_framedata.parallax[3] = 0.f;
    r_framedata.shadowbias = std::max(0.f, vr_shadow_bias.value);
    r_framedata.dlightangle = std::clamp(vr_dlight_angle.value, 0.f, 1.f);

    GL_BindNative(GL_TEXTURE4, GL_TEXTURE_2D, atlas.tex);
    GL_BindNative(GL_TEXTURE5, GL_TEXTURE_2D, staticAtlas.tex);

    if(!frameEnabled)
    {
        return;
    }
    const auto& lights = modellight::mapLights();
    const float strength = std::clamp(vr_shadow_maplight_strength.value, 0.f, 1.f);
    for(const MapSlot& s : mapSlots)
    {
        if(s.light < 0 || !s.hasCasters || s.fade <= 0.f || s.light >= static_cast<int>(lights.size()) ||
            r_framedata.numlights >= MAX_DLIGHTS)
        {
            continue;
        }
        const auto& l = lights[s.light];
        const float reach = l.value / l.scale;
        bool culled = false;
        for(int j = 0; j < 4; j++)
        {
            const mplane_t* p = &frustum[j];
            if(p->normal[0] * l.pos.x + p->normal[1] * l.pos.y + p->normal[2] * l.pos.z - p->dist + reach < 0.f)
            {
                culled = true;
                break;
            }
        }
        if(culled)
        {
            continue;
        }
        gpulight_t* out = &r_lightbuffer.lights[r_framedata.numlights++];
        out->pos[0] = l.pos.x;
        out->pos[1] = l.pos.y;
        out->pos[2] = l.pos.z;
        out->radius = reach;
        out->color[0] = strength * s.fade;
        out->color[1] = 0.f;
        out->color[2] = 0.f;
        out->minlight = 0.f;
        out->shadow[0] = s.origin.x;
        out->shadow[1] = s.origin.y;
        out->shadow[2] = mapSlotSize;
        out->shadow[3] = 1.f;
        out->shadow2[0] = s.staticOrigin.x;
        out->shadow2[1] = s.staticOrigin.y;
        out->shadow2[2] = l.value;
        out->shadow2[3] = l.scale;
        memset(out->spot, 0, sizeof(out->spot));
    }
}

// R_SetupAliasLighting: nonzero when the shader lights models per pixel (skip the flat add).
extern "C" int VR_ModelDlightsPerPixel(void)
{
    return vr_dlight_models.value != 0.f;
}

// ----------------------------------------------------------------------------
// The DarkPlaces look (round 10)

// GL_PostProcess: the eyes are neutral unless the player sets the headset's own gamma and contrast;
// the desktop's (often raised for a monitor) would brighten the headset.
extern "C" void VR_PostProcessGamma(float* gamma, float* contrast)
{
    if(!stereo::isRenderingEye())
    {
        return;
    }
    *gamma = std::clamp(vr_gamma.value, 0.25f, 4.f);
    *contrast = std::clamp(vr_contrast.value, 0.5f, 2.f);
}

extern "C" int VR_TextureSmoothing(void)
{
    return std::clamp(static_cast<int>(vr_texture_smooth.value), 0, 2);
}

extern "C" int VR_NormalMaps(void)
{
    return vr_normalmaps.value != 0.f;
}

// Each drawn instance's parallax depth in units (0: none): the world's vr_parallax_depth; the ammo and health
// boxes (maps/b_*.bsp) vr_parallax_items and models vr_parallax_models, both in their own units, so scaled with
// the size they are drawn at (the boxes are drawn a quarter size: the world's depth would be most of the box);
// other brush entities (doors, lifts) the world's, scaled likewise. The scale is the drawn matrix's (the entity's,
// the networked one and the held weapons' own), without an alias model's vertex scale (modelscale).
extern "C" float VR_ParallaxDepth(const entity_t* e, const float matrix[16], const float modelscale[3])
{
    if(vr_parallax.value == 0.f || vr_normalmaps.value == 0.f)
    {
        return 0.f;
    }
    const float world = std::clamp(vr_parallax_depth.value, 0.f, 16.f);
    if(e == &cl_entities[0])
    {
        return world;
    }
    const glm::mat3 m{glm::vec3{matrix[0], matrix[1], matrix[2]}, glm::vec3{matrix[4], matrix[5], matrix[6]},
                      glm::vec3{matrix[8], matrix[9], matrix[10]}};
    float det = std::abs(glm::determinant(m));
    if(modelscale)
    {
        det /= std::max(std::abs(modelscale[0] * modelscale[1] * modelscale[2]), 1e-12f);
    }
    const float scale = std::cbrt(det);
    float depth = world;
    if(modelscale)
    {
        depth = std::clamp(vr_parallax_models.value, 0.f, 4.f);
    }
    else if(e->model && !q_strncasecmp(e->model->name, "maps/b_", 7))
    {
        depth = std::clamp(vr_parallax_items.value, 0.f, 8.f);
    }
    return depth * std::clamp(scale, 0.f, 4.f);
}

extern "C" int VR_ModelLightParity(void)
{
    return vr_model_light_parity.value != 0.f;
}

// How much a model's skin bumps shade its own light (the alias shader's ModelBumpShade), from the light's direction
// (vr_modellight): vr_normalmap_models times the world's vr_normalmap_baked; the held weapons and hands, a hand's
// width from the eyes, VIEWMODEL_BUMPS of it (their 8-bit skins' bumps turn to noise that close).
extern "C" float VR_ModelBumps(const entity_t* e)
{
    constexpr float VIEWMODEL_BUMPS = 0.5f;
    if(vr_normalmaps.value == 0.f)
    {
        return 0.f;
    }
    const float k = std::clamp(vr_normalmap_models.value, 0.f, 2.f) * std::clamp(vr_normalmap_baked.value, 0.f, 2.f);
    return e == &cl.viewent || VR_IsViewEntity(e) ? k * VIEWMODEL_BUMPS : k;
}

extern "C" float VR_ViewModelMinLight(void)
{
    return std::clamp(vr_viewmodel_minlight.value, 0.f, 128.f);
}

// ----------------------------------------------------------------------------
// Presets

void lighting::applyPreset(int preset)
{
    struct Preset
    {
        float dlights, dlightSize, maplights, maplightSize, filter, atlasSize, distance, models, angle, entityShadows,
            modelLighting, look, // look: contrast, bloom, coloured uncapped DarkPlaces lights, sheen, models on a
                                 // par with the world, smooth replacement textures (0: Quake's)
            normalmaps, parallax;
    };
    static constexpr Preset presets[] = {
        // off: Quake's own look (flat dynamic lights, no shadows)
        {0, 256, 0, 512, 1, 4096, 1024, 0, 0, 0, 0, 0, 0, 0},
        // low
        {2, 256, 0, 512, 1, 4096, 1024, 1, 1, 1, 1, 1, 0, 0},
        // medium
        {4, 512, 2, 512, 1, 4096, 1536, 1, 1, 1, 1, 1, 1, 1},
        // high
        {6, 512, 4, 512, 2, 4096, 2048, 1, 1, 1, 1, 1, 1, 1},
        // ultra
        {8, 1024, 4, 1024, 3, 8192, 3072, 1, 1, 1, 1, 1, 1, 1},
    };
    preset = std::clamp(preset, 0, 4);
    const Preset& p = presets[preset];
    Cvar_SetValueQuick(&vr_shadow_dlights, p.dlights);
    Cvar_SetValueQuick(&vr_shadow_dlight_size, p.dlightSize);
    Cvar_SetValueQuick(&vr_shadow_maplights, p.maplights);
    Cvar_SetValueQuick(&vr_shadow_maplight_size, p.maplightSize);
    Cvar_SetValueQuick(&vr_shadow_filter, p.filter);
    Cvar_SetValueQuick(&vr_shadow_atlas, p.atlasSize);
    Cvar_SetValueQuick(&vr_shadow_distance, p.distance);
    Cvar_SetValueQuick(&vr_dlight_models, p.models);
    Cvar_SetValueQuick(&vr_dlight_angle, p.angle);
    Cvar_SetValueQuick(&vr_entity_shadows, p.entityShadows);
    Cvar_SetValueQuick(&vr_model_lighting, p.modelLighting);
    const auto look = [&](cvar_t& var, float quake) {
        Cvar_SetQuick(&var, p.look != 0.f ? var.default_string : va("%g", quake));
    };
    look(vr_light_contrast, 1.f);
    look(vr_bloom, 0.f);
    look(vr_flash_scale, 1.f);
    look(vr_explosion_light_scale, 1.f);
    look(vr_colored_lights, 0.f);
    look(vr_projectile_lights, 0.f);
    look(vr_weapon_screen_light, 0.f);
    look(vr_gadget_light, 0.f);
    look(vr_screen_glow, 0.f);
    look(vr_screen_text_glow, 0.f);
    look(vr_weapon_glow, 0.f);
    look(vr_dlight_uncapped, 0.f);
    look(vr_dlight_falloff, 0.f);
    look(vr_specular, 0.f);
    look(vr_model_light_parity, 0.f);
    look(vr_viewmodel_minlight, 24.f);
    look(vr_texture_smooth, 0.f);
    look(vr_water_splash, 0.f); // liquid splashes (vr_particles.cpp)
    Cvar_SetValueQuick(&vr_normalmaps, p.normalmaps); // made as the next map loads
    Cvar_SetValueQuick(&vr_parallax, p.parallax);
    water::applyPreset(preset); // liquids (vr_water.cpp)
}

namespace
{

// vr_graphics_preset (not saved: the settings it sets are): applies the preset chosen.
void onPreset(cvar_t* var)
{
    if(var->value >= 0.f)
    {
        lighting::applyPreset(static_cast<int>(var->value));
    }
}

// vr_light_test [radius] [seconds] [distance]: a dynamic light in front of the view, to see (and
// tune) dynamic lights and their shadows.
void lightTest_f()
{
    const float radius = Cmd_Argc() > 1 ? static_cast<float>(Q_atof(Cmd_Argv(1))) : 300.f;
    const float seconds = Cmd_Argc() > 2 ? static_cast<float>(Q_atof(Cmd_Argv(2))) : 5.f;
    const float distance = Cmd_Argc() > 3 ? static_cast<float>(Q_atof(Cmd_Argv(3))) : 48.f;
    vec3_t fwd, right, up;
    AngleVectors(r_refdef.viewangles, fwd, right, up);
    dlight_t* dl = CL_AllocDlight(0);
    for(int i = 0; i < 3; i++)
    {
        dl->origin[i] = r_refdef.vieworg[i] + fwd[i] * distance;
    }
    dl->radius = radius;
    dl->minlight = 32;
    dl->die = static_cast<float>(cl.time + seconds);
    dl->color[0] = dl->color[1] = dl->color[2] = 1.f;
}

} // namespace

void lighting::init()
{
    Cvar_SetCallback(&vr_graphics_preset, onPreset);
    Cmd_AddCommand("vr_light_test", lightTest_f);
}
