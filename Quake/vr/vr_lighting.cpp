// vr_lighting.cpp -- see vr_lighting.hpp.

#include "vr_lighting.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_modellight.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <vector>

using namespace qvr;

extern "C" int VR_IsViewEntity(const entity_t* e);

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

GLuint compile(GLenum type, const char* source)
{
    const GLuint shader = GL_CreateShaderFunc(type);
    GL_ShaderSourceFunc(shader, 1, &source, nullptr);
    GL_CompileShaderFunc(shader);
    GLint ok = 0;
    GL_GetShaderivFunc(shader, GL_COMPILE_STATUS, &ok);
    if(!ok)
    {
        char log[1024];
        GL_GetShaderInfoLogFunc(shader, sizeof(log), nullptr, log);
        Con_Warning("VR lighting: shader: %s\n", log);
    }
    return shader;
}

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
    const GLuint shader = compile(GL_VERTEX_SHADER, vs);
    depthProgram = GL_CreateProgramFunc();
    GL_AttachShaderFunc(depthProgram, shader);
    GL_LinkProgramFunc(depthProgram);
    GL_DeleteShaderFunc(shader);
    GLint ok = 0;
    GL_GetProgramivFunc(depthProgram, GL_LINK_STATUS, &ok);
    if(!ok)
    {
        Con_Warning("VR lighting: depth shader failed to link\n");
        GL_DeleteProgramFunc(depthProgram);
        depthProgram = 0;
    }
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

// A face's view-projection: reversed depth SHADOW_NEAR / distance along its axis, no far plane,
// widened by the border so that filtering near a face's edge reads its own texels.
glm::mat4 faceViewProj(const glm::vec3& light, int face, float size)
{
    const Face& f = faces[face];
    const float k = 1.f - 2.f * shadowBorder / size;
    glm::mat4 view{1.f};
    for(int c = 0; c < 3; c++)
    {
        view[c][0] = f.right[c];
        view[c][1] = f.up[c];
        view[c][2] = f.fwd[c];
    }
    view[3][0] = -glm::dot(f.right, light);
    view[3][1] = -glm::dot(f.up, light);
    view[3][2] = -glm::dot(f.fwd, light);

    glm::mat4 proj{0.f};
    proj[0][0] = k;
    proj[1][1] = k;
    proj[3][2] = shadowNear; // z' = near
    proj[2][3] = 1.f;        // w' = distance along the axis
    return proj * view;
}

// The face's frustum as Quake planes (inward), for R_CullBox on the alias models.
void faceFrustum(const glm::vec3& light, int face, float size, mplane_t out[4])
{
    const Face& f = faces[face];
    const float k = 1.f - 2.f * shadowBorder / size;
    const glm::vec3 normals[4] = {f.fwd / k - f.right, f.fwd / k + f.right, f.fwd / k - f.up, f.fwd / k + f.up};
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

// Whether a face's cone (out to `radius`) can reach what the eyes see: its apex and far corners
// all outside one view frustum plane (with a margin for the other eye) means it cannot.
bool faceVisible(const glm::vec3& light, int face, float radius)
{
    const Face& f = faces[face];
    glm::vec3 pts[5] = {light};
    int n = 1;
    for(float sx : {-1.f, 1.f})
    {
        for(float sy : {-1.f, 1.f})
        {
            pts[n++] = light + (f.fwd + f.right * sx + f.up * sy) * radius;
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

// Renders the six faces of a light at `origin` (texels) in `target`: `worldCount` indices of the
// world (from 0), the brush casters, and the alias casters.
void renderLight(DepthTarget& target, const glm::vec3& light, float radius, glm::vec2 origin, float size, size_t worldCount,
    bool brushes, bool aliases)
{
    const bool anyGeometry = !indices.empty();
    GLuint ibuf = 0;
    GLbyte* iofs = nullptr;
    if(anyGeometry)
    {
        GL_Upload(GL_ELEMENT_ARRAY_BUFFER, indices.data(), indices.size() * sizeof(uint32_t), &ibuf, &iofs);
    }

    GL_BindFramebufferFunc(GL_FRAMEBUFFER, target.fbo);
    for(int face = 0; face < 6; face++)
    {
        if(!faceVisible(light, face, radius))
        {
            continue;
        }
        const int x = static_cast<int>(origin.x) + (face % 3) * static_cast<int>(size);
        const int y = static_cast<int>(origin.y) + (face / 3) * static_cast<int>(size);
        glViewport(x, y, static_cast<int>(size), static_cast<int>(size));
        const glm::mat4 vp = faceViewProj(light, face, size);
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
            faceFrustum(light, face, size, frustum);
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
    float size = 0.f;
    glm::vec2 origin{0.f};
};
std::array<DlightSlot, MAX_DLIGHTS> dlightSlots;

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

[[nodiscard]] float pow2Floor(float v)
{
    return std::exp2(std::floor(std::log2(std::max(v, 1.f))));
}

bool lightVisible(const glm::vec3& p)
{
    if(!r_viewleaf || r_viewleaf->contents == CONTENTS_SOLID)
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
    const byte* vis = Mod_LeafPVS(r_viewleaf, cl.worldmodel);
    return (vis[index >> 3] & (1 << (index & 7))) != 0;
}

void selectDlights(const glm::vec3& eye)
{
    const int maxShadowed = static_cast<int>(vr_shadow_dlights.value);
    const float maxSize = pow2Floor(std::clamp(vr_shadow_dlight_size.value, 64.f, 2048.f));
    struct Candidate
    {
        int index;
        float score;
    };
    std::vector<Candidate> candidates;
    for(int i = 0; i < MAX_DLIGHTS; i++)
    {
        const dlight_t& l = cl_dlights[i];
        DlightSlot& slot = dlightSlots[i];
        const bool alive = l.die >= cl.time && l.radius > 0.f && l.spawn <= cl.time;
        if(!alive || maxShadowed <= 0)
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
        // DarkPlaces' level of detail: about a texel per unit of radius close by, less farther.
        const dlight_t& l = cl_dlights[i];
        const float dist = glm::distance(glm::vec3{l.origin[0], l.origin[1], l.origin[2]}, eye);
        const float want = l.radius * vr_shadow_precision.value / std::sqrt(std::max(1.f, dist / l.radius));
        float size = std::clamp(pow2Floor(want), 64.f, maxSize);
        if(slot.selected && slot.size > 0.f && want > slot.size * 0.7f && want < slot.size * 2.8f)
        {
            size = std::min(slot.size, maxSize); // hysteresis: keep the size unless it is well off
        }
        slot.selected = true;
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
    struct Candidate
    {
        int index;
        float score;
    };
    std::vector<Candidate> candidates;
    for(int i = 0; i < static_cast<int>(lights.size()); i++)
    {
        const auto& l = lights[i];
        const float dist = glm::distance(l.pos, eye);
        const float reach = l.value / l.scale;
        if(dist > reach + 128.f || dist - reach > vr_shadow_distance.value || !lightVisible(l.pos))
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
    bool* placed;
};

// Rows of 3 x 2 face blocks, largest first; when they do not fit, everything is halved and
// packed again (as DarkPlaces does), down to 32-texel faces.
void pack(std::vector<Request>& requests, int atlasSize)
{
    std::sort(requests.begin(), requests.end(), [](auto& a, auto& b) { return a.size > b.size; });
    for(float scale = 1.f; scale >= 1.f / 16.f; scale *= 0.5f)
    {
        float x = 0.f, y = 0.f, rowHeight = 0.f;
        bool fits = true;
        for(Request& r : requests)
        {
            const float s = std::max(32.f, r.size * scale);
            if(x + 3.f * s > static_cast<float>(atlasSize))
            {
                x = 0.f;
                y += rowHeight;
                rowHeight = 0.f;
            }
            if(y + 2.f * s > static_cast<float>(atlasSize))
            {
                fits = false;
                break;
            }
            *r.origin = {x, y};
            *r.placed = true;
            x += 3.f * s;
            rowHeight = std::max(rowHeight, 2.f * s);
        }
        if(fits)
        {
            for(Request& r : requests)
            {
                r.size = std::max(32.f, r.size * scale);
            }
            return;
        }
    }
    for(Request& r : requests)
    {
        *r.placed = false;
    }
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
    std::vector<Request> requests;
    std::array<bool, MAX_DLIGHTS> dlightPlaced{};
    static std::array<bool, 256> mapPlacedFlags;
    mapPlacedFlags.fill(false);
    for(int i = 0; i < MAX_DLIGHTS; i++)
    {
        if(dlightSlots[i].selected)
        {
            requests.push_back({dlightSlots[i].size, &dlightSlots[i].origin, &dlightPlaced[i]});
        }
    }
    for(size_t s = 0; s < mapSlots.size() && s < mapPlacedFlags.size(); s++)
    {
        if(mapSlots[s].light >= 0 && mapSlots[s].hasCasters)
        {
            requests.push_back({mapSlotSize, &mapSlots[s].origin, &mapPlacedFlags[s]});
        }
    }
    pack(requests, atlasSize);
    // The map lights' moving casters must match their cached world faces' size.
    for(const Request& r : requests)
    {
        for(size_t s = 0; s < mapSlots.size() && s < 256; s++)
        {
            if(r.placed == &mapPlacedFlags[s] && r.size != mapSlotSize)
            {
                mapPlacedFlags[s] = false;
            }
        }
        for(int i = 0; i < MAX_DLIGHTS; i++)
        {
            if(r.placed == &dlightPlaced[i])
            {
                dlightSlots[i].size = r.size;
            }
        }
    }
    for(int i = 0; i < MAX_DLIGHTS; i++)
    {
        dlightSlots[i].selected = dlightSlots[i].selected && dlightPlaced[i];
    }
    for(size_t s = 0; s < mapSlots.size() && s < 256; s++)
    {
        mapSlots[s].hasCasters = mapSlots[s].hasCasters && mapPlacedFlags[s];
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
        brushCasters.clear();
        aliasCasters.clear();
        // All six faces: the cache must not depend on where the viewer looks.
        mplane_t saved[4];
        memcpy(saved, frustum, sizeof(saved));
        for(mplane_t& p : frustum)
        {
            p.dist = -1e9f;
        }
        renderLight(staticAtlas, l.pos, l.value / l.scale, s.staticOrigin, mapSlotSize, indices.size(), false, false);
        memcpy(frustum, saved, sizeof(saved));
        s.cached = true;
    }

    GL_BindFramebufferFunc(GL_FRAMEBUFFER, atlas.fbo);
    glScissor(0, 0, atlas.width, atlas.height);
    GL_SetState(glstate & ~GLS_NO_ZWRITE);
    glClearDepth(0.f);
    glClear(GL_DEPTH_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);

    // Dynamic lights: the world, doors and lifts, and models.
    for(int i = 0; i < MAX_DLIGHTS; i++)
    {
        if(!dlightSlots[i].selected)
        {
            continue;
        }
        const dlight_t& l = cl_dlights[i];
        const glm::vec3 p{l.origin[0], l.origin[1], l.origin[2]};
        indices.clear();
        collectWorld(cl.worldmodel->nodes, p, l.radius);
        const size_t worldCount = indices.size();
        collectBrushes(p, l.radius, false);
        collectAliases(p, l.radius, l.key > 0 && l.key < cl.num_entities ? l.key : 0, false);
        renderLight(atlas, p, l.radius, dlightSlots[i].origin, dlightSlots[i].size, worldCount, true, true);
    }

    // Map lights: the moving things only.
    for(MapSlot& s : mapSlots)
    {
        if(s.light < 0 || !s.hasCasters || s.light >= static_cast<int>(lights.size()))
        {
            continue;
        }
        const auto& l = lights[s.light];
        indices.clear();
        brushCasters.clear();
        collectAliases(l.pos, l.value / l.scale, 0, true);
        collectBrushes(l.pos, l.value / l.scale, true);
        renderLight(atlas, l.pos, l.value / l.scale, s.origin, mapSlotSize, 0, true, true);
    }

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

// R_PushDlights, for each dynamic light it sends: its shadow's faces, if it has one.
extern "C" void VR_DlightShadow(int index, gpulight_t* out)
{
    memset(out->shadow, 0, sizeof(out->shadow));
    memset(out->shadow2, 0, sizeof(out->shadow2));
    if(frameEnabled && index >= 0 && index < MAX_DLIGHTS && dlightSlots[index].selected)
    {
        out->shadow[0] = dlightSlots[index].origin.x;
        out->shadow[1] = dlightSlots[index].origin.y;
        out->shadow[2] = dlightSlots[index].size;
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
    r_framedata.shadowflags = static_cast<int>(flags);
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
    }
}

// R_SetupAliasLighting: nonzero when the shader lights models per pixel (skip the flat add).
extern "C" int VR_ModelDlightsPerPixel(void)
{
    return vr_dlight_models.value != 0.f;
}

// ----------------------------------------------------------------------------
// Presets

void lighting::applyPreset(int preset)
{
    struct Preset
    {
        float dlights, dlightSize, maplights, maplightSize, filter, atlasSize, distance, models, angle, entityShadows,
            modelLighting;
    };
    static constexpr Preset presets[] = {
        // off: Quake's own look (flat dynamic lights, no shadows)
        {0, 256, 0, 512, 1, 4096, 1024, 0, 0, 0, 0},
        // low
        {2, 256, 0, 512, 1, 4096, 1024, 1, 1, 1, 1},
        // medium
        {4, 512, 2, 512, 1, 4096, 1536, 1, 1, 1, 1},
        // high
        {6, 512, 4, 512, 2, 4096, 2048, 1, 1, 1, 1},
        // ultra
        {8, 1024, 4, 1024, 3, 8192, 3072, 1, 1, 1, 1},
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
