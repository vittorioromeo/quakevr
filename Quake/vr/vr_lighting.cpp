// vr_lighting.cpp -- see vr_lighting.hpp.

#include "vr_modelmetadata.hpp"
#include "vr_lighting.hpp"
#include "vr_portals.hpp"
#include "vr_ao.hpp"
#include "vr_avatar.hpp"
#include "vr_main.hpp"
#include "vr_collectfx.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_gfx.hpp"
#include "vr_mem.hpp"
#include "vr_modellight.hpp"
#include "vr_profile.hpp"
#include "vr_stereo.hpp"
#include "vr_water.hpp"

#include "Zancle/Algorithm/AnyOf.hpp"
#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Base/PtrDiffT.hpp"
#include "Zancle/Container/Array.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Cbrt.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Exp2.hpp"
#include "Zancle/Math/Floor.hpp"
#include "Zancle/Math/Log2.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Pow.hpp"
#include "Zancle/Math/Round.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "vr_zancle.hpp"

#include <string.h>

using namespace qvr;

namespace
{

// Must match SHADOW_FUNCTIONS in vr_glsl.h.
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
GLuint depthLayeredProgram = 0; // vr_shadow_layered's (gl_viewport_layer_able)

struct DepthTarget
{
    GLuint tex = 0;
    GLuint fbo = 0;
    int width = 0;
    int height = 0;
};

DepthTarget atlas;        // rendered every frame: dynamic lights, and map lights' moving casters
DepthTarget staticAtlas;  // map lights' world depth, cached
DepthTarget noAtlas;      // 1 x 1, bound for an atlas not made (shadows off, no map lights): a shadow sampler on a unit
                          // without a depth texture is undefined behaviour (a GL debug warning every draw)
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
    static constexpr const char* vs = R"(#version 430
layout(location = 0) uniform mat4 MVP;
layout(location = 4) uniform vec4 ClipPlane;
layout(location = 0) in vec3 Pos;
void main()
{
    gl_Position = MVP * vec4(Pos, 1.0);
    gl_ClipDistance[0] = dot(ClipPlane, vec4(Pos, 1.0));
}
)";
    depthProgram = gfx::glProgram(vs, nullptr, "vr shadow depth");
    if(depthProgram && gl_viewport_layer_able && !depthLayeredProgram)
    {
        // The world's and the brush casters' for vr_shadow_layered: a draw's (Entry >> 3) matrices into the face (Entry & 7)
        // and its viewport, each draw's instances its faces; the same arithmetic as the one above.
        static constexpr const char* extensions[4] = {"", "GL_ARB_shader_viewport_layer_array",
            "GL_AMD_vertex_shader_viewport_index", "GL_NV_viewport_array2"};
        static constexpr const char* layeredBody = R"(
struct Draw
{
    mat4 MVP[6];
    vec4 ClipPlane;
};
layout(std430, binding = 7) restrict readonly buffer DrawBuffer
{
    Draw draws[];
};
layout(location = 0) in uint Entry;
layout(location = 1) in vec3 Pos;
void main()
{
    uint face = Entry & 7u;
    uint draw = Entry >> 3;
    gl_Position = draws[draw].MVP[face] * vec4(Pos, 1.0);
    gl_ClipDistance[0] = dot(draws[draw].ClipPlane, vec4(Pos, 1.0));
    gl_ViewportIndex = int(face);
}
)";
        char source[1024];
        q_snprintf(source, sizeof(source), "#version 430\n#extension %s : require\n%s",
            extensions[za::clamp(gl_viewport_layer_able, 1, 3)], layeredBody);
        depthLayeredProgram = gfx::glProgram(source, nullptr, "vr shadow depth layered");
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
        GL_DeleteNativeTexture(t.tex); // (and out of the engine's bound-texture cache: its name comes back at once)
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
    right = glm::normalize(glm::cross(dir, za::abs(dir.z) < 0.9f ? glm::vec3{0.f, 0.f, 1.f} : glm::vec3{1.f, 0.f, 0.f}));
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
    c = za::clamp(c, 0.f, 1.f);
    const float along = c >= 0.7071f ? radius / (2.f * c) : radius * c;
    r = za::min((c >= 0.7071f ? along : radius * za::sqrt(1.f - c * c)) * 1.01f + 1.f, radius);
    center = light + dir * along;
}

// ----------------------------------------------------------------------------
// Casters

za::Vector<uint32_t> indices;

void addSurface(const msurface_t* s)
{
    if(s->flags & (SURF_DRAWSKY | SURF_DRAWTURB))
    {
        return;
    }
    for(int k = 2; k < s->numedges; k++)
    {
        indices.pushBack(static_cast<uint32_t>(s->vbo_firstvert));
        indices.pushBack(static_cast<uint32_t>(s->vbo_firstvert + k - 1));
        indices.pushBack(static_cast<uint32_t>(s->vbo_firstvert + k));
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
za::Vector<BrushCaster> brushCasters;
za::Vector<entity_t*> aliasCasters;

// A map light's moving casters, collected once a frame (whether it has any), drawn later.
struct SlotCasters
{
    za::Vector<entity_t*> aliases;
    za::Vector<BrushCaster> brushes;
    za::Vector<uint32_t> indices;
};
za::Vector<SlotCasters> slotCasters; // by map slot

// vr_shadow_layered's world and brush casters: a draw's matrices into each face (the layered depth shader's Draw,
// std430), and its indirect command.
struct LayeredDraw
{
    glm::mat4 mvp[6];
    glm::vec4 clip;
};
static_assert(sizeof(LayeredDraw) == 6 * 64 + 16);
struct LayeredCaster
{
    entity_t* e;
    unsigned char bits; // its faces
};
struct IndirectCommand
{
    GLuint count, instanceCount, firstIndex;
    GLint baseVertex;
    GLuint baseInstance;
};

// A shadow map's draw's buffers (the main thread).
struct CasterScratch
{
    za::Vector<glm::mat4> brushModels;    // the brush casters' model matrices
    za::Vector<entity_t*> faceCasters;    // the alias casters drawn in this face (layered: in their faces)
    za::Vector<unsigned char> posed;      // whether each alias caster is posed by bones
    za::Vector<unsigned char> faceBits;   // layered: each of faceCasters' faces (bit f: face f)
    za::Vector<LayeredCaster> layered;    // layered: the casters to sort by model (the alias renderer's batches)
    za::Vector<entity_t*> holey;          // layered: the holey-skinned casters, drawn a face at a time
    za::Vector<unsigned char> holeyBits;  // their faces
    za::Vector<LayeredDraw> draws;        // layered: the world's and the brush casters' draws
    za::Vector<uint32_t> entries;         // their instances: draw << 3 | face
    za::Vector<IndirectCommand> commands; // and commands
    auto members() { return qvr::mem::list(brushModels, faceCasters, posed, faceBits, layered, holey, holeyBits, draws, entries, commands); }
};
mem::Scratch<CasterScratch> casterScratch{"shadow casters"};

bool touches(const entity_t* e, const glm::vec3& light, float radius)
{
    const glm::vec3 lo{e->model->mins[0], e->model->mins[1], e->model->mins[2]};
    const glm::vec3 hi{e->model->maxs[0], e->model->maxs[1], e->model->maxs[2]};
    const float r = za::max(glm::length(lo), glm::length(hi)) * VR_EntityScale(e);
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
            (itemsOnly && qvr::modelmeta::has(e.model, qvr::modelmeta::Trait::Submodel)) || !touches(&e, light, radius))
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
            brushCasters.pushBack({&e, first, indices.size() - first});
        }
    }
    // A box put away, shrinking into its holster (vr_collectfx.cpp): it cast its shadow in the hand a moment ago.
    for(int c = 0; c < collectfx::maxCopies; c++)
    {
        entity_t* e = collectfx::liveCopy(c);
        if(!e || e->model->type != mod_brush || qvr::modelmeta::has(e->model, qvr::modelmeta::Trait::Submodel) ||
            !touches(e, light, radius))
        {
            continue;
        }
        const size_t first = indices.size();
        const msurface_t* s = e->model->surfaces + e->model->firstmodelsurface;
        for(int k = 0; k < e->model->nummodelsurfaces; k++, s++)
        {
            addSurface(s);
        }
        if(indices.size() > first)
        {
            brushCasters.pushBack({e, first, indices.size() - first});
        }
    }
}

// Models near the light: monsters, items, and (vr_shadow_self) the player's own body and hands: for map lights, and
// for dynamic lights that are not the player's own (`self`: the flashlight, rockets, explosions; not your muzzle
// flashes or powerup glows, which are inside or at the body). Not the light's own entity (a rocket), not see-through or
// shadowless ones. The view entities are placed by the VR transforms (their offsets, the posed arms), which their
// bounds don't include: they are tested with a wider sphere here and in each face (renderLight).
float viewEntityReach(const entity_t* e)
{
    const glm::vec3 lo{e->model->mins[0], e->model->mins[1], e->model->mins[2]};
    const glm::vec3 hi{e->model->maxs[0], e->model->maxs[1], e->model->maxs[2]};
    return za::max(glm::length(lo), glm::length(hi)) * VR_EntityScale(e) * 2.f + 16.f;
}

void collectAliases(const glm::vec3& light, float radius, int ownEntity, bool self, bool bothRooms = false)
{
    aliasCasters.clear();
    const int selfCasters = self ? static_cast<int>(vr_shadow_self.value) : 0;
    for(int i = 0; i < cl_numvisedicts; i++)
    {
        entity_t* e = cl_visedicts[i];
        if(!e->model || e->model->type != mod_alias || (e->model->flags & MOD_NOSHADOW) || e->alpha != ENTALPHA_DEFAULT ||
            e == &cl_entities[ownEntity])
        {
            continue;
        }
        if(VR_IsViewEntity(e))
        {
            const bool body = modelmeta::has(e->model, modelmeta::Trait::ContainsBody);
            if(selfCasters <= 0 || (selfCasters == 1 && !body) ||
                glm::distance(glm::vec3{e->origin[0], e->origin[1], e->origin[2]}, light) > radius + viewEntityReach(e))
            {
                continue;
            }
        }
        else if(!touches(e, light, radius))
        {
            continue;
        }
        aliasCasters.pushBack(e);
    }
    // The camera's list has been culled to one room. Virtual-light shadows need blockers on both path segments.
    if(bothRooms)
    {
        for(int i = 1; i < cl.num_entities; i++)
        {
            entity_t* e = &cl_entities[i];
            if(!e->model || e->model->type != mod_alias || e->msgtime != cl.mtime[0] ||
               (e->model->flags & MOD_NOSHADOW) || e->alpha != ENTALPHA_DEFAULT || i == ownEntity ||
               !touches(e, light, radius) || za::anyOf(aliasCasters.begin(), aliasCasters.end(),
                   [e](const auto* p) { return p == e; })) { continue; }
            aliasCasters.pushBack(e);
        }
    }
}

// ----------------------------------------------------------------------------
// Drawing

int facesDrawn = 0;
int modelsDrawn = 0; // model draws, over all faces
float cpuMs = 0.f;

glm::vec4 aliasShadowClip{0.f, 0.f, 0.f, 1.f};

void drawIndices(const glm::mat4& mvp, size_t first, size_t count, const glm::vec4& clip)
{
    if(!count)
    {
        return;
    }
    GL_UniformMatrix4fvFunc(0, 1, GL_FALSE, &mvp[0][0]);
    GL_Uniform4fvFunc(4, 1, &clip[0]);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(count), GL_UNSIGNED_INT,
        reinterpret_cast<const void*>(first * sizeof(uint32_t)));
}

bool sphereInView(const mplane_t planes[4], const float c[3], float r)
{
    for(int p = 0; p < 4; p++)
    {
        if(DotProduct(planes[p].normal, c) - planes[p].dist < -r)
        {
            return false;
        }
    }
    return true;
}

// Whether a view (a light's face) can hold any of the moving casters: the brush casters (not culled
// per face), or an alias caster whose bounding sphere reaches into the face's (widened) pyramid. The
// player's own models and skeletal (posed) ones get at least 96 units and twice their bounds: posed
// limbs, hands and held weapons reach past a model's bounds.
bool viewHasCasters(const glm::vec3& light, const ShadowView& view, float size, bool brushes, bool aliases,
    const za::Vector<unsigned char>& posed)
{
    if(brushes && !brushCasters.empty())
    {
        return true;
    }
    if(!aliases)
    {
        return false;
    }
    mplane_t planes[4];
    viewFrustum(light, view, size, planes);
    for(size_t i = 0; i < aliasCasters.size(); i++)
    {
        const entity_t* e = aliasCasters[i];
        const glm::vec3 lo{e->model->mins[0], e->model->mins[1], e->model->mins[2]};
        const glm::vec3 hi{e->model->maxs[0], e->model->maxs[1], e->model->maxs[2]};
        float r = za::max(glm::length(lo), glm::length(hi)) * VR_EntityScale(e);
        if(posed.size() > i && posed[i])
        {
            r = za::max(r * 2.f, 96.f);
        }
        else if(VR_IsViewEntity(e))
        {
            r = viewEntityReach(e);
        }
        const glm::vec3 c{e->origin[0], e->origin[1], e->origin[2]};
        bool inside = true;
        for(const mplane_t& p : planes)
        {
            if(p.normal[0] * c.x + p.normal[1] * c.y + p.normal[2] * c.z - p.dist < -r)
            {
                inside = false;
                break;
            }
        }
        if(inside)
        {
            return true;
        }
    }
    return false;
}

// vr_shadow_layered: a light's casters drawn once into all its faces (each face a viewport, chosen in the vertex shader:
// gl_viewport_layer_able), instead of once per face. layeredOverride -1 as the cvar says, 0 or 1 forced
// (vr_shadow_layered_check).
int layeredOverride = -1;

bool layeredShadows()
{
    const bool on = layeredOverride >= 0 ? layeredOverride != 0 : vr_shadow_layered.value != 0.f;
    return on && gl_viewport_layer_able && depthLayeredProgram && glprogs.alias_depth_layered[0];
}

// A light's views drawn at once (renderLight, vr_shadow_layered): the same depth as a face at a time. The world and the
// brush casters in one indirect draw (a draw's instances: its faces); each alias caster once, with the faces it
// reaches by the same tests a face at a time makes (R_CullModelForEntity on the face's frustum, the view entities'
// wider sphere, the posed ones in every face), set up once (lerp, matrices, bones); the holey-skinned ones (their
// fragment shader's alpha test) a face at a time after.
void drawLayered(const glm::vec3& light, float radius, const ShadowView* views, int numViews, float size, size_t worldCount,
    bool brushes, bool aliases, unsigned* faceMask, const glm::vec4& plane, GLuint ibuf, GLbyte* iofs)
{
    CasterScratch& cs = casterScratch;
    glm::mat4 vps[6]{};
    mplane_t planes[6][4]{};
    unsigned visible = 0u;
    GLuint numVisible = 0;
    for(int face = 0; face < numViews; face++)
    {
        const ShadowView& view = views[face];
        if(!viewVisible(light, view, radius))
        {
            continue;
        }
        visible |= 1u << face;
        numVisible++;
        vps[face] = viewProj(light, view, size);
        viewFrustum(light, view, size, planes[face]);
        // As glViewport's integers a face at a time.
        GL_ViewportIndexedfFunc(static_cast<GLuint>(face), static_cast<float>(static_cast<int>(view.at.x)),
            static_cast<float>(static_cast<int>(view.at.y)), static_cast<float>(static_cast<int>(size)),
            static_cast<float>(static_cast<int>(size)));
        facesDrawn++;
        if(faceMask && viewHasCasters(light, view, size, brushes, aliases, cs.posed))
        {
            *faceMask |= 1u << face;
        }
    }
    if(!visible)
    {
        return;
    }

    if(!indices.empty() && (worldCount || (brushes && !brushCasters.empty())))
    {
        cs.draws.clear();
        cs.entries.clear();
        cs.commands.clear();
        const size_t base = reinterpret_cast<uintptr_t>(iofs) / sizeof(uint32_t);
        const auto add = [&](size_t first, size_t count, const glm::mat4* model, const glm::vec4& clip)
        {
            if(!count)
            {
                return;
            }
            const uint32_t draw = static_cast<uint32_t>(cs.draws.size());
            LayeredDraw& d = cs.draws.emplaceBack();
            const GLuint baseInstance = static_cast<GLuint>(cs.entries.size());
            for(int face = 0; face < 6; face++)
            {
                d.mvp[face] = glm::mat4{0.f};
                if(visible & (1u << face))
                {
                    d.mvp[face] = model ? vps[face] * *model : vps[face]; // as drawIndices' a face at a time
                    cs.entries.pushBack(draw << 3 | static_cast<uint32_t>(face));
                }
            }
            d.clip = clip;
            cs.commands.pushBack({static_cast<GLuint>(count), numVisible, static_cast<GLuint>(base + first), 0, baseInstance});
        };
        add(0, worldCount, nullptr, plane);
        for(size_t i = 0; brushes && i < cs.brushModels.size(); i++)
        {
            add(brushCasters[i].first, brushCasters[i].count, &cs.brushModels[i], glm::transpose(cs.brushModels[i]) * plane);
        }
        if(!cs.commands.empty())
        {
            GLuint drawBuf = 0, entryBuf = 0, commandBuf = 0;
            GLbyte *drawOfs = nullptr, *entryOfs = nullptr, *commandOfs = nullptr;
            const size_t drawBytes = cs.draws.size() * sizeof(LayeredDraw);
            GL_Upload(GL_SHADER_STORAGE_BUFFER, cs.draws.data(), drawBytes, &drawBuf, &drawOfs);
            GL_Upload(GL_ARRAY_BUFFER, cs.entries.data(), cs.entries.size() * sizeof(uint32_t), &entryBuf, &entryOfs);
            GL_Upload(GL_DRAW_INDIRECT_BUFFER, cs.commands.data(), cs.commands.size() * sizeof(IndirectCommand), &commandBuf,
                &commandOfs);
            GL_UseProgram(depthLayeredProgram);
            GL_SetState(GLS_BLEND_OPAQUE | GLS_CULL_NONE | GLS_ATTRIBS(2) | GLS_INSTANCED_ATTRIBS(1));
            GL_BindBufferRange(GL_SHADER_STORAGE_BUFFER, 7, drawBuf, reinterpret_cast<GLintptr>(drawOfs),
                static_cast<GLsizeiptr>(drawBytes));
            GL_BindBuffer(GL_ARRAY_BUFFER, entryBuf);
            GL_VertexAttribIPointerFunc(0, 1, GL_UNSIGNED_INT, 0, entryOfs);
            GL_BindBuffer(GL_ARRAY_BUFFER, gl_bmodel_vbo);
            GL_VertexAttribPointerFunc(1, 3, GL_FLOAT, GL_FALSE, sizeof(glvert_t), nullptr);
            GL_BindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibuf);
            GL_BindBuffer(GL_DRAW_INDIRECT_BUFFER, commandBuf);
            GL_MultiDrawElementsIndirectFunc(GL_TRIANGLES, GL_UNSIGNED_INT, commandOfs, static_cast<GLsizei>(cs.commands.size()),
                sizeof(IndirectCommand));
        }
    }

    if(aliases && !aliasCasters.empty())
    {
        QVR_PROFILE("shadow alias draw");
        mplane_t savedFrustum[4];
        memcpy(savedFrustum, frustum, sizeof(savedFrustum));
        profile::begin("shadow caster faces", false); // which faces each caster reaches
        cs.layered.clear();
        cs.holey.clear();
        cs.holeyBits.clear();
        for(size_t i = 0; i < aliasCasters.size(); i++)
        {
            entity_t* e = aliasCasters[i];
            const bool view = VR_IsViewEntity(e);
            const float reach = view ? viewEntityReach(e) : 0.f;
            unsigned bits = cs.posed[i] ? visible : 0u;
            // A face at a time: in the face's casters by the sphere (the view entities) or R_CullModelForEntity, then
            // culled by the alias renderer's own R_CullModelForEntity (its bounds once here, R_CullBox each face).
            vec3_t mins, maxs;
            if(!cs.posed[i])
            {
                R_GetEntityBounds(e, mins, maxs);
            }
            for(int face = 0; face < numViews && !cs.posed[i]; face++)
            {
                if(!(visible & (1u << face)))
                {
                    continue;
                }
                memcpy(frustum, planes[face], sizeof(planes[face]));
                if((!view || sphereInView(frustum, e->origin, reach)) && !R_CullBox(mins, maxs))
                {
                    bits |= 1u << face;
                }
            }
            if(!bits)
            {
                continue;
            }
            for(unsigned b = bits; b; b &= b - 1u)
            {
                modelsDrawn++; // a draw per face, as a face at a time
            }
            if(e->model->flags & MF_HOLEY)
            {
                cs.holey.pushBack(e);
                cs.holeyBits.pushBack(static_cast<unsigned char>(bits));
            }
            else
            {
                cs.layered.pushBack({e, static_cast<unsigned char>(bits)});
            }
        }
        memcpy(frustum, savedFrustum, sizeof(savedFrustum));
        // By model and skin: the alias renderer draws a run of the same model and skin as one batch (the depth is the
        // nearest whatever the order).
        za::quickSort(cs.layered.begin(), cs.layered.end(), [](const LayeredCaster& a, const LayeredCaster& b)
        {
            const uintptr_t ma = reinterpret_cast<uintptr_t>(a.e->model), mb = reinterpret_cast<uintptr_t>(b.e->model);
            return ma != mb ? ma < mb : a.e->skinnum < b.e->skinnum;
        });
        cs.faceCasters.clear();
        cs.faceBits.clear();
        for(const LayeredCaster& c : cs.layered)
        {
            cs.faceCasters.pushBack(c.e);
            cs.faceBits.pushBack(c.bits);
        }
        profile::end();
        if(!cs.faceCasters.empty())
        {
            R_DrawAliasModelsDepthLayered(cs.faceCasters.data(), cs.faceBits.data(), static_cast<int>(cs.faceCasters.size()),
                &vps[0][0][0]);
        }
        // The holey ones a face at a time (glViewport sets every viewport: after the layered draws).
        for(int face = 0; face < numViews && !cs.holey.empty(); face++)
        {
            cs.faceCasters.clear();
            for(size_t i = 0; i < cs.holey.size(); i++)
            {
                if(cs.holeyBits[i] & (1u << face))
                {
                    cs.faceCasters.pushBack(cs.holey[i]);
                }
            }
            if(cs.faceCasters.empty())
            {
                continue;
            }
            const ShadowView& view = views[face];
            glViewport(static_cast<int>(view.at.x), static_cast<int>(view.at.y), static_cast<int>(size), static_cast<int>(size));
            memcpy(frustum, planes[face], sizeof(planes[face]));
            float savedViewProj[16];
            memcpy(savedViewProj, r_matviewproj, sizeof(savedViewProj));
            memcpy(r_matviewproj, &vps[face][0][0], sizeof(r_matviewproj));
            R_DrawAliasModelsDepth(cs.faceCasters.data(), static_cast<int>(cs.faceCasters.size()));
            memcpy(r_matviewproj, savedViewProj, sizeof(savedViewProj));
            memcpy(frustum, savedFrustum, sizeof(savedFrustum));
        }
    }
}

// Renders a light's views (tiles of `size` texels) in `target`: `worldCount` indices of the world
// (from 0), the brush casters, and the alias casters. faceMask: the views drawn with moving casters
// in them (viewHasCasters), for the world shader to skip the others' lookups (MapLightShadow).
void renderLight(DepthTarget& target, const glm::vec3& light, float radius, const ShadowView* views, int numViews, float size,
    size_t worldCount, bool brushes, bool aliases, unsigned* faceMask = nullptr, const glm::vec4* clip = nullptr)
{
    const glm::vec4 plane = clip ? *clip : glm::vec4{0.f, 0.f, 0.f, 1.f};
    aliasShadowClip = plane;
    if(clip) { glEnable(GL_CLIP_DISTANCE0); }
    const bool anyGeometry = !indices.empty();
    GLuint ibuf = 0;
    GLbyte* iofs = nullptr;
    if(anyGeometry)
    {
        GL_Upload(GL_ELEMENT_ARRAY_BUFFER, indices.data(), indices.size() * sizeof(uint32_t), &ibuf, &iofs);
    }

    // The brush casters' model matrices, the same for every face.
    za::Vector<glm::mat4>& brushModels = casterScratch.brushModels;
    brushModels.clear();
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
            glm::mat4& model = brushModels.emplaceBack();
            memcpy(&model[0][0], m, sizeof(m));
        }
    }
    // Skeletal (IK-posed) casters: the alias renderer draws them unculled (their limbs reach past the
    // model's bounds); each face gets them all.
    za::Vector<entity_t*>& faceCasters = casterScratch.faceCasters;
    za::Vector<unsigned char>& posed = casterScratch.posed;
    posed.resize(aliasCasters.size());
    for(size_t i = 0; aliases && i < aliasCasters.size(); i++)
    {
        posed[i] = VR_AliasBonePoses(aliasCasters[i], nullptr) != 0;
    }
    avatar::shadowLight(light); // your body's head in this light's shadow (vr_shadow_head)

    GL_BindFramebufferFunc(GL_FRAMEBUFFER, target.fbo);
    const bool layered = layeredShadows();
    if(layered)
    {
        drawLayered(light, radius, views, numViews, size, worldCount, brushes, aliases, faceMask, plane, ibuf, iofs);
    }
    for(int face = 0; !layered && face < numViews; face++) // a face at a time (vr_shadow_layered 0, or no extension)
    {
        const ShadowView& view = views[face];
        if(!viewVisible(light, view, radius))
        {
            continue;
        }
        glViewport(static_cast<int>(view.at.x), static_cast<int>(view.at.y), static_cast<int>(size), static_cast<int>(size));
        const glm::mat4 vp = viewProj(light, view, size);
        facesDrawn++;
        if(faceMask && viewHasCasters(light, view, size, brushes, aliases, posed))
        {
            *faceMask |= 1u << face;
        }

        if(anyGeometry && (worldCount || (brushes && !brushCasters.empty())))
        {
            GL_UseProgram(depthProgram);
            GL_SetState(GLS_BLEND_OPAQUE | GLS_CULL_NONE | GLS_ATTRIBS(1));
            GL_BindBuffer(GL_ARRAY_BUFFER, gl_bmodel_vbo);
            GL_VertexAttribPointerFunc(0, 3, GL_FLOAT, GL_FALSE, sizeof(glvert_t), nullptr);
            GL_BindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibuf);
            const size_t base = reinterpret_cast<uintptr_t>(iofs) / sizeof(uint32_t);
            drawIndices(vp, base, worldCount, plane);
            for(size_t i = 0; i < brushModels.size(); i++)
            {
                drawIndices(vp * brushModels[i], base + brushCasters[i].first, brushCasters[i].count,
                    glm::transpose(brushModels[i]) * plane);
            }
        }

        if(aliases && !aliasCasters.empty())
        {
            QVR_PROFILE("shadow alias draw");
            // The alias renderer reads the camera's view-projection and frustum: this face's.
            mplane_t savedFrustum[4];
            memcpy(savedFrustum, frustum, sizeof(savedFrustum));
            viewFrustum(light, view, size, frustum);
            // Only the casters in this face: the alias renderer's own cull (R_CullModelForEntity, after
            // it has set the model up) would leave out the rest.
            // The view entities (the hands, the guns) by a wider sphere (viewEntityReach).
            faceCasters.clear();
            for(size_t i = 0; i < aliasCasters.size(); i++)
            {
                entity_t* e = aliasCasters[i];
                if(posed[i] || (VR_IsViewEntity(e) ? sphereInView(frustum, e->origin, viewEntityReach(e)) : !R_CullModelForEntity(e)))
                {
                    faceCasters.pushBack(e);
                }
            }
            if(!faceCasters.empty())
            {
                float savedViewProj[16];
                memcpy(savedViewProj, r_matviewproj, sizeof(savedViewProj));
                memcpy(r_matviewproj, &vp[0][0], sizeof(r_matviewproj));
                R_DrawAliasModelsDepth(faceCasters.data(), static_cast<int>(faceCasters.size())); // no lighting set up
                modelsDrawn += static_cast<int>(faceCasters.size());
                memcpy(r_matviewproj, savedViewProj, sizeof(savedViewProj));
            }
            memcpy(frustum, savedFrustum, sizeof(savedFrustum));
        }
    }
    if(clip) { glDisable(GL_CLIP_DISTANCE0); }
    aliasShadowClip = glm::vec4{0.f, 0.f, 0.f, 1.f};
}

// ----------------------------------------------------------------------------
// Light selection

struct DlightSlot
{
    bool selected = false; // has a tile in the atlas this frame (wanted, or fading out)
    bool wanted = false;   // among the vr_shadow_dlights most important this frame
    bool spot = false;     // one tile round its cone instead of six faces
    bool seen = false;     // the same light (key) was alive at the last selection
    bool fresh = false;    // ... and was not: it has just appeared
    int key = 0;
    float held = 0.f;      // seconds it has been wanted (shadowHoldTime)
    float strength = 0.f;  // its shadow's fade, 0..1 (shadowFadeTime)
    float distFade = 1.f;  // less over the last shadowDistanceFade of vr_shadow_distance
    float size = 0.f;
    glm::vec2 origin{0.f};
};

// Shadows never switch on or off at once (a light chosen or dropped, a step closer or back): they
// fade over shadowFadeTime; the lights chosen keep their shadows shadowHoldTime (unless far outdone:
// shadowHoldBonus), and lose them only to lights clearly more important (shadowHysteresis); and
// they fade out over the last
// shadowDistanceFade of vr_shadow_distance. A light fading out keeps its tile until faded, past
// the budget by at most shadowFadeExtra lights.
constexpr float shadowFadeTime = 0.4f;
constexpr float shadowHoldTime = 1.f;
constexpr float shadowHysteresis = 1.3f;
constexpr float shadowHoldBonus = 2.f; // (on top of the hysteresis, within shadowHoldTime: a far brighter light still wins)
constexpr float shadowDistanceFade = 0.15f;
constexpr int shadowFadeExtra = 2;

[[nodiscard]] float distanceFade(float beyond)
{
    // beyond: how far the light's reach is from the viewer (its distance less its radius).
    const float d = za::max(vr_shadow_distance.value, 1.f);
    return za::clamp((d - beyond) / (shadowDistanceFade * d), 0.f, 1.f);
}
za::Array<DlightSlot, MAX_DLIGHTS> dlightSlots;

// Spot lights (lighting::dlightSpot): valid while the slot holds the same light (its key and
// death time).
struct Spot
{
    int key = 0;
    float die = -1.f;
    glm::vec3 dir{1.f, 0.f, 0.f};
    float cosInner = 1.f, cosOuter = 1.f;
};
za::Array<Spot, MAX_DLIGHTS> spots;

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
    const float c = za::clamp(s.cosOuter, 0.1f, 1.f);
    return za::sqrt(1.f - c * c) / c * 1.06f + 0.01f;
}

// Lights marked to cast no shadow (lighting::dlightNoShadow): valid while the slot holds the same
// light (its key and death time).
struct NoShadow
{
    int key = 0;
    float die = -1.f;
};
za::Array<NoShadow, MAX_DLIGHTS> noShadows;

struct MapSlot
{
    int light = -1;       // index in modellight::mapLights()
    float fade = 0.f;     // 0..1
    float distFade = 1.f; // less over the last shadowDistanceFade of vr_shadow_distance
    float held = 0.f;     // seconds since it was chosen (shadowHoldTime)
    bool wanted = false;
    bool cached = false;  // world depth rendered in staticAtlas
    bool hasCasters = false;
    unsigned faceMask = 0x3fu; // the faces its moving casters were drawn in this frame (bit 0 +x .. bit 5 -z)
    glm::vec2 staticOrigin{0.f};
    glm::vec2 origin{0.f}; // this frame's moving casters, in the atlas
};
za::Vector<MapSlot> mapSlots;
float mapSlotSize = 0.f;
const qmodel_t* slotsWorld = nullptr;
int slotsGeneration = -1;

// A light to shadow, and how much it matters (the greater the more).
struct Candidate
{
    int index;
    float score;
};

// A block to pack into the shadow atlas (see pack, "Atlas packing").
struct Request
{
    float size;
    glm::vec2* origin;
    float* packedSize; // the face size it was packed at, if wanted
    float columns = 3.f, rows = 2.f; // in faces: a point light's 3 x 2, a spot light's one tile
};

// The shadowed lights' choice and packing, each frame (the main thread).
struct ShadowScratch
{
    za::Vector<Candidate> dlightCandidates; // (selectDlights)
    za::Vector<Candidate> dlightFading;     // (selectDlights: their strength as the score)
    za::Vector<Candidate> mapCandidates;    // the map lights near the viewer
    za::Vector<Request> requests;           // the faces packed into the atlas
    auto members() { return qvr::mem::list(dlightCandidates, dlightFading, mapCandidates, requests); }
};
mem::Scratch<ShadowScratch> shadowScratch{"shadow lights"};

[[nodiscard]] float pow2Floor(float v)
{
    return za::exp2(za::floor(za::log2(za::max(v, 1.f))));
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

void selectDlights(const glm::vec3& eye, float dt)
{
    const int maxShadowed = static_cast<int>(vr_shadow_dlights.value);
    const float maxSize = pow2Floor(za::clamp(vr_shadow_dlight_size.value, 64.f, 2048.f));
    za::Vector<Candidate>& candidates = shadowScratch.dlightCandidates;
    za::Vector<Candidate>& fading = shadowScratch.dlightFading;
    candidates.clear();
    fading.clear();
    for(int i = 0; i < MAX_DLIGHTS; i++)
    {
        const dlight_t& l = cl_dlights[i];
        DlightSlot& slot = dlightSlots[i];
        const bool alive = l.die >= cl.time && l.radius > 0.f && l.spawn <= cl.time;
        if(!alive || maxShadowed <= 0 || (slot.seen && slot.key != l.key))
        {
            // Gone (or another light in its place): its shadow goes with it.
            slot = DlightSlot{};
            if(!alive || maxShadowed <= 0)
            {
                continue;
            }
        }
        slot.fresh = !slot.seen;
        slot.seen = true;
        slot.key = l.key;
        const glm::vec3 p{l.origin[0], l.origin[1], l.origin[2]};
        const float dist = glm::distance(p, eye);
        slot.distFade = distanceFade(dist - l.radius);
        // Muzzle flashes are brief and right by the viewer's gun: last, if at all.
        const bool muzzle = l.key == cl.viewentity && l.die - cl.time <= 0.11;
        const bool eligible = !(noShadows[i].key == l.key && noShadows[i].die == l.die) && slot.distFade > 0.f &&
                              !(muzzle && !vr_shadow_muzzleflash.value);
        if(!eligible)
        {
            slot.wanted = false;
            slot.held = 0.f;
            if(slot.selected && slot.strength > 0.f)
            {
                fading.pushBack({i, slot.strength});
            }
            else
            {
                slot.selected = false;
                slot.strength = 0.f;
            }
            continue;
        }
        // Importance: the angular size of its light's reach (what it lights, as the viewer sees
        // it), not the distance alone.
        float score = l.radius / za::max(dist, l.radius * 0.25f);
        if(muzzle)
        {
            score *= 0.25f;
        }
        if(slot.wanted)
        {
            score *= shadowHysteresis;
            if(slot.held < shadowHoldTime)
            {
                score *= shadowHoldBonus; // kept a while once chosen (unless far outdone)
            }
        }
        candidates.pushBack({i, score});
    }
    za::quickSort(candidates.begin(), candidates.end(), [](auto& a, auto& b) { return a.score > b.score; });

    // The rest fade out, the brightest few keeping their tiles until faded.
    for(size_t c = static_cast<size_t>(za::max(maxShadowed, 0)); c < candidates.size(); c++)
    {
        DlightSlot& slot = dlightSlots[candidates[c].index];
        slot.wanted = false;
        slot.held = 0.f;
        if(slot.selected && slot.strength > 0.f)
        {
            fading.pushBack({candidates[c].index, slot.strength});
        }
        else
        {
            slot.selected = false;
            slot.strength = 0.f;
        }
    }
    if(candidates.size() > static_cast<size_t>(za::max(maxShadowed, 0)))
    {
        candidates.resize(static_cast<size_t>(za::max(maxShadowed, 0)));
    }
    za::quickSort(fading.begin(), fading.end(), [](auto& a, auto& b) { return a.score > b.score; });
    const float step = dt / shadowFadeTime;
    for(size_t f = 0; f < fading.size(); f++)
    {
        DlightSlot& slot = dlightSlots[fading[f].index];
        slot.strength = f < static_cast<size_t>(shadowFadeExtra) ? za::max(0.f, slot.strength - step) : 0.f;
        slot.selected = slot.strength > 0.f;
    }

    for(const Candidate& c : candidates)
    {
        const int i = c.index;
        DlightSlot& slot = dlightSlots[i];
        // A light that has just appeared (an explosion, a muzzle flash) has its shadow at once; one
        // that was there unshadowed fades its in.
        slot.strength = slot.fresh ? 1.f : za::min(1.f, slot.strength + step);
        slot.held = slot.wanted ? slot.held + dt : 0.f;
        slot.wanted = true;
        // DarkPlaces' level of detail: about a texel per unit of radius close by, less farther. A
        // spot light's one tile gets twice a face's size (still a third of a cube's texels, over
        // its narrow cone).
        const dlight_t& l = cl_dlights[i];
        const bool spot = spotOf(i) != nullptr && spotSpread(*spotOf(i)) < 2.f;
        const float tileMax = spot ? za::min(2.f * maxSize, 2048.f) : maxSize;
        const float dist = glm::distance(glm::vec3{l.origin[0], l.origin[1], l.origin[2]}, eye);
        const float want = l.radius * vr_shadow_precision.value / za::sqrt(za::max(1.f, dist / l.radius)) * (spot ? 2.f : 1.f);
        float size = za::clamp(pow2Floor(want), 64.f, tileMax);
        if(slot.selected && slot.spot == spot && slot.size > 0.f && want > slot.size * 0.7f && want < slot.size * 2.8f)
        {
            size = za::min(slot.size, tileMax); // hysteresis: keep the size unless it is well off
        }
        slot.selected = true;
        slot.spot = spot;
        slot.size = size;
    }
}

void selectMapLights(const glm::vec3& eye, float dt)
{
    int wanted = static_cast<int>(vr_shadow_maplights.value);
    const float size = pow2Floor(za::clamp(vr_shadow_maplight_size.value, 64.f, 2048.f));
    // The world depth cache: slots of 3 x 2 faces, in at most 8192 x 8192.
    const int columns = za::max(1, static_cast<int>(8192.f / (3.f * size)));
    const int rows = za::max(1, static_cast<int>(8192.f / (2.f * size)));
    wanted = za::clamp(wanted, 0, columns * rows);

    if(cl.worldmodel != slotsWorld || worldGeneration() != slotsGeneration || size != mapSlotSize || static_cast<int>(mapSlots.size()) != wanted)
    {
        mapSlots.clear();
        mapSlots.resize(static_cast<size_t>(wanted), MapSlot{});
        mapSlotSize = size;
        slotsWorld = cl.worldmodel;
        slotsGeneration = worldGeneration();
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
    za::Vector<Candidate>& candidates = shadowScratch.mapCandidates;
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
        // Its light at the viewer (and a little round): positive, so that the hysteresis's factor
        // favours the ones shown.
        float score = za::max(1.f, l.value + 128.f - dist * l.scale);
        for(const MapSlot& s : mapSlots)
        {
            if(s.light == i && s.wanted)
            {
                score *= shadowHysteresis;
                if(s.held < shadowHoldTime)
                {
                    score *= shadowHoldBonus; // kept a while once chosen (unless far outdone)
                }
            }
        }
        candidates.pushBack({i, score});
    }
    za::quickSort(candidates.begin(), candidates.end(), [](auto& a, auto& b) { return a.score > b.score; });
    if(static_cast<int>(candidates.size()) > wanted)
    {
        candidates.resize(static_cast<size_t>(wanted));
    }

    for(MapSlot& s : mapSlots)
    {
        s.wanted = za::anyOf(candidates.begin(), candidates.end(), [&](auto& c) { return c.index == s.light; });
    }
    for(const Candidate& c : candidates)
    {
        if(za::anyOf(mapSlots.begin(), mapSlots.end(), [&](auto& s) { return s.light == c.index; }))
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
                s.held = 0.f;
                break;
            }
        }
    }
    // Fade in and out (shadowFadeTime); a slot is freed once faded out (a light chosen meanwhile
    // waits for it).
    for(MapSlot& s : mapSlots)
    {
        if(s.light < 0)
        {
            continue;
        }
        if(s.light >= static_cast<int>(lights.size()))
        {
            s.light = -1;
            continue;
        }
        const auto& l = lights[s.light];
        s.distFade = distanceFade(glm::distance(l.pos, eye) - l.value / l.scale);
        s.held = s.wanted ? s.held + dt : 0.f;
        s.fade = za::clamp(s.fade + (s.wanted ? dt : -dt) / shadowFadeTime, 0.f, 1.f);
        if(!s.wanted && s.fade <= 0.f)
        {
            s.light = -1;
            s.cached = false;
        }
    }
}

// ----------------------------------------------------------------------------
// Atlas packing (the blocks: Request)

// Rows of blocks (a point light's 3 x 2 faces, a spot light's one tile), tallest first; when they
// do not fit, everything is halved and packed again (as DarkPlaces does), down to 32-texel faces.
// Returns the scale they were all packed at, 0 if they did not fit.
float pack(za::Vector<Request>& requests, int atlasSize)
{
    za::quickSort(requests.begin(), requests.end(), [](auto& a, auto& b) { return a.size * a.rows > b.size * b.rows; });
    for(float scale = 1.f; scale >= 1.f / 16.f; scale *= 0.5f)
    {
        float x = 0.f, y = 0.f, rowHeight = 0.f;
        bool fits = true;
        for(Request& r : requests)
        {
            const float s = za::max(32.f, r.size * scale);
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
            rowHeight = za::max(rowHeight, r.rows * s);
        }
        if(fits)
        {
            for(const Request& r : requests)
            {
                if(r.packedSize)
                {
                    *r.packedSize = za::max(32.f, r.size * scale);
                }
            }
            return scale;
        }
    }
    return 0.f;
}

// Two lights, one traversal each: paired source/exit cube maps, 256-unit tiles at most.
// Never overwrite a native dlight slot or let a virtual light recurse through another portal.
struct PortalLight
{
    int index = -1;
    portals::LightGate gate;
    glm::vec3 pos;
    glm::vec2 sourceTile{0.f}, exitTile{0.f};
    float size = 256.f, score = 0.f;
};
za::Array<PortalLight, 2> portalLights;
int portalLightCount = 0;

void selectPortalLights(const glm::vec3& eye)
{
    portalLightCount = 0;
    if(!r_dynamic.value) { return; }
    for(int i = 0; i < MAX_DLIGHTS; i++)
    {
        const dlight_t& l = cl_dlights[i];
        if(l.radius <= 0.f || l.die < cl.time || l.spawn > cl.time) { continue; }
        const glm::vec3 pos{l.origin[0], l.origin[1], l.origin[2]};
        portals::LightGate gates[16];
        const int count = portals::lightGates(pos, l.radius, gates, 16);
        for(int j = 0; j < count; j++)
        {
            const auto& gate = gates[j];
            const glm::vec3 virtualPos = gate.turn * (pos - gate.from) + gate.to;
            bool visible = true;
            for(const mplane_t& p : frustum)
            {
                if(glm::dot(glm::vec3{p.normal[0],p.normal[1],p.normal[2]}, virtualPos) - p.dist + l.radius < 0.f)
                    { visible = false; break; }
            }
            if(!visible) { continue; }
            // A spotlight must reach some of the aperture; shader clipping handles its exact cone.
            if(const Spot* spot = spotOf(i))
            {
                const glm::vec3 centre = (gate.mins + gate.maxs) * 0.5f;
                const float reach = glm::length(gate.maxs - gate.mins) * 0.5f;
                const glm::vec3 d = centre - pos;
                if(glm::dot(spot->dir, d) + reach <= 0.f) { continue; }
                const float along = glm::dot(spot->dir, d);
                if(glm::length(d - spot->dir * along) > reach + za::max(along,0.f) * spotSpread(*spot)) { continue; }
            }
            const float score = l.radius / za::max(glm::distance(virtualPos, eye), l.radius * 0.25f) *
                (spotOf(i) ? 2.f : 1.f);
            int at = portalLightCount;
            if(at == 2)
            {
                at = portalLights[0].score < portalLights[1].score ? 0 : 1;
                if(score <= portalLights[at].score) { continue; }
            }
            else { portalLightCount++; }
            portalLights[at] = {i, gate, virtualPos, {}, {}, 256.f, score};
        }
    }
}

void renderPortalLight(PortalLight& l)
{
    const dlight_t& source = cl_dlights[l.index];
    const glm::vec3 origin{source.origin[0], source.origin[1], source.origin[2]};
    const int own = source.key > 0 && source.key < cl.num_entities ? source.key : 0;
    mplane_t saved[4];
    memcpy(saved, frustum, sizeof(saved));
    // Source occlusion is evaluated at the entry point, before the source's backing wall.
    // These tiles cannot be culled by the destination camera.
    for(mplane_t& p : frustum) { p.dist = -1e9f; }
    ShadowView views[6];
    indices.clear();
    collectWorld(cl.worldmodel->nodes, origin, source.radius);
    size_t worldCount = indices.size();
    collectBrushes(origin, source.radius, false);
    collectAliases(origin, source.radius, own, source.key != cl.viewentity, true);
    renderLight(atlas, origin, source.radius, views, cubeViews(l.sourceTile, l.size, views), l.size,
        worldCount, true, true);
    memcpy(frustum, saved, sizeof(saved));
    const glm::vec3 normal = l.gate.turn * -l.gate.normal;
    const glm::vec4 clip{normal, -glm::dot(normal, l.gate.to)};
    indices.clear();
    collectWorld(cl.worldmodel->nodes, l.pos, source.radius);
    worldCount = indices.size();
    collectBrushes(l.pos, source.radius, false);
    collectAliases(l.pos, source.radius, own, source.key != cl.viewentity, true);
    renderLight(atlas, l.pos, source.radius, views, cubeViews(l.exitTile, l.size, views), l.size,
        worldCount, true, true, nullptr, &clip);
}

bool frameEnabled = false;
int renderedFrame = -1;
bool renderedPortal = false; // shadow selection currently belongs to a portal or the ordinary camera
double lastTime = 0.0;
double lastPrint = 0.0;        // vr_shadow_stats's last print (VR_RenderShadowMaps)
bool clipControlWarned = false; // (shadowsSupported: once)

bool shadowsSupported()
{
    bool& warned = clipControlWarned;
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

// vr_shadow_layered_check [repeats] [cache]: this frame's shadow maps drawn a face at a time and layered (vr_shadow_layered),
// `repeats` times each, alternating, timed (CPU: the submission; GPU: timestamps round it, waited for), then once more
// each with the map lights' cached world drawn again too, read back and compared texel by texel (both atlases).
// "cache": as vr_shadow_layered says both times, the casters set up for each light, then once a pass
// (R_AliasDepthCacheBegin).
int layeredCheckRepeats = 0;
bool layeredCheckCache = false; // the comparison is the casters' set-up cache off and on
bool setupCacheOn = true;

void layeredCheck_f()
{
    layeredCheckRepeats = Cmd_Argc() > 1 ? za::clamp(Q_atoi(Cmd_Argv(1)), 1, 500) : 1;
    layeredCheckCache = Cmd_Argc() > 2 && !q_strcasecmp(Cmd_Argv(2), "cache");
    if(!gl_viewport_layer_able || !depthLayeredProgram)
    {
        Con_Printf("vr_shadow_layered_check: no layered shadows here (%s), compared with themselves\n",
            gl_viewport_layer_able ? "no program yet" : "no GL_ARB_shader_viewport_layer_array or AMD/NV one");
    }
}

// An atlas' depth, read back.
void readDepth(const DepthTarget& t, za::Vector<float>& out)
{
    out.resize(static_cast<size_t>(t.width) * static_cast<size_t>(t.height));
    if(out.empty())
    {
        return;
    }
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, t.tex);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, GL_FLOAT, out.data());
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, 0);
}

// Two read-backs compared: texels that differ, the most they differ (and in units in the last place: depths are
// positive floats, so their bits order as they do), and each one's FNV-1a hash.
struct DepthDiff
{
    size_t differ = 0;
    float most = 0.f;
    uint32_t mostUlp = 0u;
    uint32_t hash[2] = {2166136261u, 2166136261u};
};

DepthDiff compareDepth(const za::Vector<float>& a, const za::Vector<float>& b)
{
    DepthDiff r;
    size_t& differ = r.differ;
    float& most = r.most;
    uint32_t& mostUlp = r.mostUlp;
    uint32_t* hash = r.hash;
    for(size_t i = 0; i < a.size() && i < b.size(); i++)
    {
        uint32_t ua = 0u, ub = 0u;
        memcpy(&ua, &a[i], sizeof(ua));
        memcpy(&ub, &b[i], sizeof(ub));
        hash[0] = (hash[0] ^ ua) * 16777619u;
        hash[1] = (hash[1] ^ ub) * 16777619u;
        if(ua != ub)
        {
            differ++;
            most = za::max(most, za::abs(a[i] - b[i]));
            mostUlp = za::max(mostUlp, ua > ub ? ua - ub : ub - ua);
        }
    }
    return r;
}

void printDiff(const char* name, const DepthTarget& t, const DepthDiff& d)
{
    Con_Printf("  %s %dx%d: %zu texels differ (most %g, %u ulp); hashes %08x %08x\n", name, t.width, t.height, d.differ,
        d.most, d.mostUlp, d.hash[0], d.hash[1]);
}

template <class Render>
void layeredCheck(const Render& renderAll)
{
    const int repeats = layeredCheckRepeats;
    layeredCheckRepeats = 0;
    GLuint queries[2] = {};
    GL_GenQueriesFunc(2, queries);
    double cpu[2] = {}, gpu[2] = {};
    int calls[2] = {}, faces[2] = {}, models[2] = {}, hits[2] = {}, misses[2] = {};
    R_AliasDepthCacheStats(&hits[0], &misses[0]); // (cleared)
    hits[0] = misses[0] = 0;
    za::Vector<float> depth[2];     // a face at a time: the atlas, the static atlas
    za::Vector<float> layeredDepth; // layered: one, then the other (three read-backs at most)
    DepthDiff diffs[2];
    for(int r = 0; r <= repeats; r++)
    {
        for(int way = 0; way < 2; way++)
        {
            layeredOverride = layeredCheckCache ? -1 : way;
            setupCacheOn = !layeredCheckCache || way == 1;
            const bool last = r == repeats; // the read-back: every atlas drawn again
            if(last)
            {
                for(MapSlot& s : mapSlots)
                {
                    s.cached = false;
                }
            }
            glFinish();
            const int calls0 = vr_profcounts.drawcalls;
            GL_QueryCounterFunc(queries[0], GL_TIMESTAMP);
            const double t0 = Sys_DoubleTime();
            renderAll();
            const double t1 = Sys_DoubleTime();
            int h = 0, m = 0;
            R_AliasDepthCacheStats(&h, &m);
            hits[way] += h;
            misses[way] += m;
            GL_QueryCounterFunc(queries[1], GL_TIMESTAMP);
            GLuint64 begin = 0, end = 0;
            GL_GetQueryObjectui64vFunc(queries[0], GL_QUERY_RESULT, &begin);
            GL_GetQueryObjectui64vFunc(queries[1], GL_QUERY_RESULT, &end);
            if(!last)
            {
                cpu[way] += (t1 - t0) * 1000.0;
                gpu[way] += static_cast<double>(end - begin) / 1e6;
                calls[way] = vr_profcounts.drawcalls - calls0;
                faces[way] = facesDrawn;
                models[way] = modelsDrawn;
                continue;
            }
            if(way == 0)
            {
                readDepth(atlas, depth[0]);
                readDepth(staticAtlas, depth[1]);
                continue;
            }
            readDepth(atlas, layeredDepth);
            diffs[0] = compareDepth(depth[0], layeredDepth);
            readDepth(staticAtlas, layeredDepth);
            diffs[1] = compareDepth(depth[1], layeredDepth);
        }
    }
    layeredOverride = -1;
    setupCacheOn = true;
    GL_DeleteQueriesFunc(2, queries);
    int dl = 0, spots = 0, ml = 0;
    for(const DlightSlot& s : dlightSlots)
    {
        dl += s.selected ? 1 : 0;
        spots += s.selected && s.spot ? 1 : 0;
    }
    for(const MapSlot& s : mapSlots)
    {
        ml += (s.light >= 0 && s.hasCasters) ? 1 : 0;
    }
    Con_Printf("vr_shadow_layered_check (%d repeats, means; %d dynamic (%d spot), %d portal, %d map lights):\n", repeats, dl,
        spots, portalLightCount, ml);
    for(int way = 0; way < 2; way++)
    {
        Con_Printf("  %s: %d draw calls, %d faces, %d model draws (%d set up, %d reused); CPU %.3f ms, GPU %.3f ms\n",
            layeredCheckCache ? (way ? "set up once " : "each light  ") : way ? "layered     " : "face at a time", calls[way], faces[way], models[way],
            misses[way] / (repeats + 1), hits[way] / (repeats + 1), cpu[way] / repeats, gpu[way] / repeats);
    }
    printDiff("atlas", atlas, diffs[0]);
    printDiff("static atlas", staticAtlas, diffs[1]);
}

} // namespace

// ----------------------------------------------------------------------------
// Engine hooks

// R_SetupView, before R_PushDlights: once per frame (both eyes share it).
extern "C" void VR_RenderShadowMaps(void)
{
    // Portal and ordinary cameras choose different lights. Rebuild when switching, preserving the existing budgets.
    // The second eye without a portal still shares the first eye's data.
    const bool portal=portals::viewing();
    if(renderedFrame == host_framecount && !portal && !renderedPortal) { return; }
    renderedPortal=portal;
    renderedFrame = host_framecount;
    QVR_GPU_PROFILE("shadow maps");
    const double cpuStart = Sys_DoubleTime();
    const float dt = static_cast<float>(za::clamp(vr_gametime - lastTime, 0.0, 0.1));
    lastTime = vr_gametime;

    const glm::vec3 eye{r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]};
    selectPortalLights(eye);
    frameEnabled = (vr_shadow_dlights.value > 0.f || vr_shadow_maplights.value > 0.f || portalLightCount > 0) && cl.worldmodel &&
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

    profile::begin("shadow select", false);
    selectDlights(eye, dt);
    selectMapLights(eye, dt);

    // Moving casters of the map lights: none near means nothing to draw (and no light entry). Kept
    // for their drawing below.
    const auto& lights = modellight::mapLights();
    slotCasters.resize(mapSlots.size());
    for(size_t i = 0; i < mapSlots.size(); i++)
    {
        MapSlot& s = mapSlots[i];
        s.hasCasters = false;
        if(s.light >= 0 && s.light < static_cast<int>(lights.size()))
        {
            const auto& l = lights[s.light];
            indices.clear();
            collectAliases(l.pos, l.value / l.scale, 0, true);
            collectBrushes(l.pos, l.value / l.scale, true);
            s.hasCasters = !aliasCasters.empty() || !brushCasters.empty();
            SlotCasters& c = slotCasters[i];
            c.aliases.swap(aliasCasters);
            c.brushes.swap(brushCasters);
            c.indices.swap(indices);
        }
    }

    // Pack this frame's faces.
    const int atlasSize = static_cast<int>(pow2Floor(za::clamp(vr_shadow_atlas.value, 1024.f, 8192.f)));
    za::Vector<Request>& requests = shadowScratch.requests;
    requests.clear();
    for(DlightSlot& slot : dlightSlots)
    {
        if(slot.selected)
        {
            requests.pushBack({slot.size, &slot.origin, &slot.size, slot.spot ? 1.f : 3.f, slot.spot ? 1.f : 2.f});
        }
    }
    for(MapSlot& s : mapSlots)
    {
        if(s.light >= 0 && s.hasCasters)
        {
            requests.pushBack({mapSlotSize, &s.origin, nullptr});
        }
    }
    for(int i = 0; i < portalLightCount; i++)
    {
        PortalLight& l = portalLights[i];
        requests.pushBack({l.size, &l.sourceTile, &l.size});
        requests.pushBack({l.size, &l.exitTile, nullptr});
    }
    // What does not fit casts no shadow this frame; the map lights' moving casters must also
    // match their cached world faces' size (not scaled down).
    const float packScale = pack(requests, atlasSize);
    if(packScale <= 0.f) { portalLightCount = 0; }
    for(DlightSlot& slot : dlightSlots)
    {
        slot.selected = slot.selected && packScale > 0.f;
    }
    for(MapSlot& s : mapSlots)
    {
        s.hasCasters = s.hasCasters && packScale == 1.f;
    }
    profile::end();
    if(vr_shadow_stats.value >= 2.f)
    {
        // vr_shadow_stats 2: each frame, the shadowed lights and their shadows' strength (+ chosen, - fading out).
        Con_Printf("shadowsel %.3f at %.0f %.0f %.0f: dlights", vr_gametime, eye.x, eye.y, eye.z);
        for(int i = 0; i < MAX_DLIGHTS; i++)
        {
            const DlightSlot& s = dlightSlots[i];
            if(s.selected)
            {
                Con_Printf(" %d%c%.2f", s.key, s.wanted ? '+' : '-', s.strength * s.distFade);
            }
        }
        Con_Printf("; map lights");
        for(const MapSlot& s : mapSlots)
        {
            if(s.light >= 0)
            {
                Con_Printf(" %d%c%.2f%s", s.light, s.wanted ? '+' : '-', s.fade * s.distFade, s.hasCasters ? "" : "(no casters)");
            }
        }
        Con_Printf("\n");
    }

    if(!ensure(atlas, atlasSize, atlasSize, "shadow atlas"))
    {
        frameEnabled = false;
        return;
    }
    if(!mapSlots.empty())
    {
        const int columns = za::max(1, static_cast<int>(8192.f / (3.f * mapSlotSize)));
        const int used = static_cast<int>(mapSlots.size());
        const int w = static_cast<int>(3.f * mapSlotSize) * za::min(columns, used);
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
    // Everything drawn into the atlases (twice by vr_shadow_layered_check).
    const auto renderAll = [&]()
    {
        R_AliasDepthCacheBegin(setupCacheOn); // each caster set up once in this pass (r_alias.c)
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
            profile::begin("dlight casters", false);
            indices.clear();
            collectWorld(cl.worldmodel->nodes, center, reach);
            const size_t worldCount = indices.size();
            collectBrushes(center, reach, false);
            collectAliases(center, reach, l.key > 0 && l.key < cl.num_entities ? l.key : 0, l.key != cl.viewentity);
            profile::end();
            renderLight(atlas, p, l.radius, views, numViews, slot.size, worldCount, true, true);
        }

        profile::end();

        profile::begin("portal light shadows", true);
        for(int i = 0; i < portalLightCount; i++) { renderPortalLight(portalLights[i]); }
        profile::end();

        profile::begin("map light shadows", true);
        // Map lights: the moving things only.
        for(size_t i = 0; i < mapSlots.size(); i++)
        {
            MapSlot& s = mapSlots[i];
            if(s.light < 0 || !s.hasCasters || s.light >= static_cast<int>(lights.size()))
            {
                continue;
            }
            const auto& l = lights[s.light];
            SlotCasters& c = slotCasters[i]; // collected above
            aliasCasters.swap(c.aliases);
            brushCasters.swap(c.brushes);
            indices.swap(c.indices);
            ShadowView views[6];
            s.faceMask = 0u;
            renderLight(atlas, l.pos, l.value / l.scale, views, cubeViews(s.origin, mapSlotSize, views), mapSlotSize, 0, true, true,
                &s.faceMask);
            aliasCasters.swap(c.aliases); // back: drawn again by vr_shadow_layered_check
            brushCasters.swap(c.brushes);
            indices.swap(c.indices);
        }

        profile::end();
        R_AliasDepthCacheEnd();
    };
    if(layeredCheckRepeats > 0)
    {
        layeredCheck(renderAll);
    }
    else
    {
        renderAll();
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

        if(vr_gametime - lastPrint > 1.0)
        {
            lastPrint = vr_gametime;
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
            const float k = za::clamp(static_cast<float>((dl.die - cl.time) / look.fade), 0.f, 1.f);
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
    memset(out->gateplane, 0, sizeof(*out) - offsetof(gpulight_t, gateplane));
    const Spot* spot = spotOf(index);
    if(spot)
    {
        // The cone as the shaders read it: 1 - smoothstep(0, 1, w - dot(xyz, the direction to the point)).
        const float s = 1.f / za::max(spot->cosInner - spot->cosOuter, 1e-3f);
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
        // The share its shadow lets through (fading in or out; 0 full shadow).
        out->shadow2[1] = 1.f - za::clamp(dlightSlots[index].strength * dlightSlots[index].distFade, 0.f, 1.f);
    }
    if(vr_dlight_falloff.value != 0.f && index >= 0 && index < MAX_DLIGHTS)
    {
        darkplacesLight(index, out);
    }
}

extern "C" void VR_AliasShadowClip(void)
{
    GL_Uniform4fvFunc(86, 1, &aliasShadowClip[0]);
}

extern "C" void VR_PushPortalLights(void)
{
    if(!frameEnabled || !r_dynamic.value) { return; }
    for(int i = 0; i < portalLightCount && r_framedata.numlights < MAX_DLIGHTS; i++)
    {
        const PortalLight& l = portalLights[i];
        const dlight_t& source = cl_dlights[l.index];
        gpulight_t* out = &r_lightbuffer.lights[r_framedata.numlights++];
        memset(out, 0, sizeof(*out));
        VR_DlightShadow(l.index, out);
        out->radius = source.radius;
        out->minlight = source.minlight;
        for(int c = 0; c < 3; c++) { out->pos[c] = l.pos[c]; out->color[c] = source.color[c]; }
        if(vr_dlight_falloff.value != 0.f) { darkplacesLight(l.index, out); }
        const glm::vec3 spot = l.gate.turn * glm::vec3{out->spot[0],out->spot[1],out->spot[2]};
        for(int c = 0; c < 3; c++) { out->spot[c] = spot[c]; }
        out->shadow[0] = l.exitTile.x; out->shadow[1] = l.exitTile.y; out->shadow[2] = l.size;
        memset(out->shadow2, 0, sizeof(out->shadow2)); // cube maps even for a spotlight
        const glm::vec3 normal = l.gate.turn * -l.gate.normal;
        for(int c = 0; c < 3; c++)
        {
            out->gateplane[c] = normal[c]; out->gatelo[c] = l.gate.mins[c]; out->gatehi[c] = l.gate.maxs[c];
            for(int row = 0; row < 3; row++) { out->gateinverse[row][c] = l.gate.turn[row][c]; }
        }
        out->gateplane[3] = glm::dot(normal, l.gate.to);
        out->gatelo[3] = 1.f;
        const glm::vec3 shift = l.gate.from - glm::transpose(l.gate.turn) * l.gate.to;
        for(int row = 0; row < 3; row++) { out->gateinverse[row][3] = shift[row]; }
        out->gateshadow[0] = l.sourceTile.x; out->gateshadow[1] = l.sourceTile.y; out->gateshadow[2] = l.size;
    }
}

void lighting::dlightLook(const dlight_t* dl, float ambient, float fade)
{
    const za::PtrDiffT index = dl - cl_dlights;
    if(index < 0 || index >= MAX_DLIGHTS)
    {
        return;
    }
    dlightLooks[index] = DlightLook{dl->key, dl->die, ambient, fade};
}

void lighting::dlightSpot(const dlight_t* dl, const glm::vec3& dir, float innerDegrees, float outerDegrees)
{
    const za::PtrDiffT index = dl - cl_dlights;
    const float len = glm::length(dir);
    if(index < 0 || index >= MAX_DLIGHTS || len < 1e-6f)
    {
        return;
    }
    const float outer = za::clamp(outerDegrees, 1.f, 179.f);
    const float inner = za::clamp(innerDegrees, 0.f, outer - 0.5f);
    spots[index] = Spot{dl->key, dl->die, dir / len, za::cos(glm::radians(inner)), za::cos(glm::radians(outer))};
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
    const float t = za::clamp(l->spot[3] - glm::dot(axis, d) / len, 0.f, 1.f);
    return 1.f - t * t * (3.f - 2.f * t);
}

void lighting::dlightNoShadow(const dlight_t* dl)
{
    const za::PtrDiffT index = dl - cl_dlights;
    if(index >= 0 && index < MAX_DLIGHTS)
    {
        noShadows[index] = NoShadow{dl->key, dl->die};
    }
}

// The baked light's own settings, read the same way by the world shader (r_framedata.lighttweak and
// .ambient, QVR_WORLD_FS_LIGHT), by the models' light (VR_AliasLightCurve) and by the light around
// them (vr_ambient.cpp): a monster is lit by the room its model stands in.
float lighting::lightContrast()
{
    return za::clamp(vr_light_contrast.value, 0.5f, 3.f);
}

// The room's own fill light (vr_ambient_light): a share of Quake's full light added to the baked
// light before the contrast, so a map reads clearly lit without the flashlight while its lamps stay
// brighter. 0 keeps Quake's; the Debug A/B (vr_ambient_light_ab) takes it off at once, to compare.
float lighting::ambientFloor()
{
    return vr_ambient_light_ab.value != 0.f ? 0.f : za::clamp(vr_ambient_light.value, 0.f, 1.f);
}

// The curve onto a lightmap value (R_LightPoint: 128 is Quake's full light): the fill light first,
// then the contrast about that same full light. Models are lit from the lightmap at their feet with
// this; the world shader does the same on total_light, where Quake's full light is 0.5.
void lighting::lightCurve(float* lightcolor)
{
    const float c = lightContrast();
    const float a = ambientFloor();
    if(c == 1.f && a == 0.f)
    {
        return;
    }
    for(int i = 0; i < 3; i++)
    {
        lightcolor[i] = 128.f * za::pow(za::max(0.f, lightcolor[i]) / 128.f + a, c);
    }
}

// Models are lit from the lightmap at their feet (R_LightPoint: 128 is Quake's full light): the
// same fill light and contrast as the world's.
extern "C" void VR_AliasLightCurve(float lightcolor[3])
{
    lighting::lightCurve(lightcolor);
}

// R_PushDlights, after the dynamic lights: the map lights with moving casters, the frame's
// lighting settings, and the atlases on texture units 4 and 5.
extern "C" void VR_PushMapLights(void)
{
    unsigned flags = static_cast<unsigned>(za::clamp(static_cast<int>(vr_shadow_filter.value), 0, 3));
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
    if(VR_AlphaToCoverage())
    {
        flags |= 64u; // alpha-tested surfaces give their coverage as alpha (r_world.c, r_alias.c enable it)
    }
    if(vr_deluxemap.value != 0.f && lux_texture != nullptr)
    {
        flags |= 128u; // the baked light's real directions (the map's .lux, r_brush.c: LuxTex, unit 9) for its bumps
    }
    r_framedata.shadowflags = static_cast<int>(flags);
    // Lightmap contrast about Quake's full light (a lightmap value of a half, before the doubling):
    // shade darker, well lit walls as they were, the brightest a little brighter.
    r_framedata.lighttweak[0] = lighting::lightContrast();
    // How much the normal maps shade the baked light (from a direction the shader guesses from the lightmap).
    r_framedata.lighttweak[1] = za::clamp(vr_normalmap_baked.value, 0.f, 2.f);
    // Dynamic lights' sheen, and how deep the normal maps' bumps are (0: flat).
    r_framedata.lighttweak[2] = za::clamp(vr_specular.value, 0.f, 4.f);
    r_framedata.lighttweak[3] = vr_normalmaps.value != 0.f ? za::clamp(vr_normalmap_strength.value, 0.f, 8.f) : 0.f;
    // The room's own fill light (vr_ambient_light): the world shader adds it to the lightmap before the
    // contrast above, so the shade reads lit and the lamps stay as bright (qvr::lighting::ambientFloor).
    r_framedata.ambient[0] = lighting::ambientFloor();
    r_framedata.ambient[1] = r_framedata.ambient[2] = r_framedata.ambient[3] = 0.f;
    // Parallax occlusion mapping on the world (the heights are in the normal maps' alpha): how deep, how far it
    // reaches, and the most steps along a ray.
    const bool parallax = vr_parallax.value != 0.f && vr_normalmaps.value != 0.f;
    r_framedata.parallax[0] = parallax ? za::clamp(vr_parallax_depth.value, 0.f, 16.f) : 0.f;
    r_framedata.parallax[1] = za::clamp(vr_parallax_distance.value, 64.f, 4096.f);
    r_framedata.parallax[2] = za::clamp(za::round(vr_parallax_steps.value), 4.f, 64.f);
    // The steps refining the hit after the walk, and the grazing fade: gone at vr_parallax_grazing degrees off the
    // normal, whole 12 degrees before (cosines; 0 0 at 90 or more: no fade).
    r_framedata.parallax2[0] = za::clamp(za::round(vr_parallax_refine.value), 0.f, 8.f);
    const float grazing = za::clamp(vr_parallax_grazing.value, 30.f, 90.f);
    r_framedata.parallax2[1] = grazing < 90.f ? za::cos(glm::radians(grazing - 12.f)) : 0.f;
    r_framedata.parallax2[2] = grazing < 90.f ? za::cos(glm::radians(grazing)) : 0.f;
    // Pixel depth offset: the hits' depth written, so what meets the relief meets it where it is (r_world.c then
    // bounds it in the world's depth pre-pass: glprogs.world_depth_pdo).
    // 2: and drawn as that depth (vr_parallax_debug).
    r_framedata.parallax2[3] = parallax && vr_parallax_depth_write.value != 0.f ? (vr_parallax_debug.value != 0.f ? 2.f : 1.f) : 0.f;
    // Specular anti-aliasing: how much the sheen's lobe widens by the bumps under a pixel (0 off).
    r_framedata.parallax[3] = za::clamp(vr_specular_aa.value, 0.f, 4.f);
    r_framedata.shadowbias = za::max(0.f, vr_shadow_bias.value);
    r_framedata.dlightangle = za::clamp(vr_dlight_angle.value, 0.f, 1.f);

    if((!atlas.tex || !staticAtlas.tex) && !noAtlas.tex)
    {
        ensure(noAtlas, 1, 1, "shadow atlas placeholder");
    }
    GL_BindNative(GL_TEXTURE4, GL_TEXTURE_2D, atlas.tex ? atlas.tex : noAtlas.tex);
    GL_BindNative(GL_TEXTURE5, GL_TEXTURE_2D, staticAtlas.tex ? staticAtlas.tex : noAtlas.tex);
    ao::upload(); // dynamic ambient occlusion's occluders for this eye (vr_ao.cpp; uniform block 2)

    if(!frameEnabled)
    {
        return;
    }
    const auto& lights = modellight::mapLights();
    const float strength = za::clamp(vr_shadow_maplight_strength.value, 0.f, 1.f);
    for(const MapSlot& s : mapSlots)
    {
        if(s.light < 0 || !s.hasCasters || !s.faceMask || s.fade * s.distFade <= 0.f || s.light >= static_cast<int>(lights.size()) ||
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
        out->color[0] = strength * s.fade * s.distFade;
        out->color[1] = 0.f;
        out->color[2] = 0.f;
        out->minlight = 0.f;
        out->shadow[0] = s.origin.x;
        out->shadow[1] = s.origin.y;
        out->shadow[2] = mapSlotSize;
        out->shadow[3] = 1.f + static_cast<float>(s.faceMask); // nonzero: a map light; 1 + the faces with moving casters
        out->shadow2[0] = s.staticOrigin.x;
        out->shadow2[1] = s.staticOrigin.y;
        out->shadow2[2] = l.value;
        out->shadow2[3] = l.scale;
        memset(out->spot, 0, sizeof(out->spot));
        memset(out->gateplane, 0, sizeof(*out) - offsetof(gpulight_t, gateplane));
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
    *gamma = za::clamp(vr_gamma.value, 0.25f, 4.f);
    *contrast = za::clamp(vr_contrast.value, 0.5f, 2.f);
}

extern "C" int VR_TextureSmoothing(void)
{
    return za::clamp(static_cast<int>(vr_texture_smooth.value), 0, 2);
}

// Fences and grates (alpha-tested textures): their mips keep the top level's coverage (gl_texmgr.c), and with
// MSAA they are drawn with alpha to coverage.
extern "C" int VR_AlphaMipCoverage(void)
{
    return vr_alpha_coverage.value != 0.f;
}

extern "C" int VR_AlphaToCoverage(void)
{
    return vr_alpha_coverage.value != 0.f && framebufs.scene.samples > 1;
}

extern "C" int VR_NormalMaps(void)
{
    return vr_normalmaps.value != 0.f;
}

// Each drawn instance's parallax depth in units (0: none): the world's vr_parallax_depth; the ammo and health
// boxes (maps/b_*.bsp) vr_parallax_items and models vr_parallax_models, both in their own units, so scaled with
// the size they are drawn at (the boxes are drawn a quarter size: the world's depth would be most of the box);
// other brush entities (doors, lifts) the world's, scaled likewise. The scale is the drawn matrix's (the entity's,
// the networked one and the held weapons' own), without an alias model's vertex scale (modelscale). A model whose
// skin has an authored normal map with heights (heights 2: a baked map's alpha, bake_normals.py) is carved as deep as
// it was baked (AUTHORED_HEIGHT_DEPTH units at alpha 0) times vr_parallax_authored; one without heights (0: a map
// without alpha) isn't carved at all.
extern "C" float VR_ParallaxDepth(const entity_t* e, const float matrix[16], const float modelscale[3], int heights)
{
    // bake_normals.py's AUTHORED_DEPTH: model units under the surface an authored map's alpha 0 lies
    constexpr float AUTHORED_HEIGHT_DEPTH = 0.4f;
    if(vr_parallax.value == 0.f || vr_normalmaps.value == 0.f || heights == 0)
    {
        return 0.f;
    }
    const float world = za::clamp(vr_parallax_depth.value, 0.f, 16.f);
    if(e == &cl_entities[0])
    {
        return world;
    }
    const glm::mat3 m{glm::vec3{matrix[0], matrix[1], matrix[2]}, glm::vec3{matrix[4], matrix[5], matrix[6]},
                      glm::vec3{matrix[8], matrix[9], matrix[10]}};
    float det = za::abs(glm::determinant(m));
    if(modelscale)
    {
        det /= za::max(za::abs(modelscale[0] * modelscale[1] * modelscale[2]), 1e-12f);
    }
    const float scale = za::cbrt(det);
    float depth = world;
    if(modelscale)
    {
        depth = heights == 2 ? AUTHORED_HEIGHT_DEPTH * za::clamp(vr_parallax_authored.value, 0.f, 4.f)
                             : za::clamp(vr_parallax_models.value, 0.f, 4.f);
    }
    else if(e->model && qvr::modelmeta::has(e->model, qvr::modelmeta::Trait::AmmoBoxInsensitive))
    {
        depth = za::clamp(vr_parallax_items.value, 0.f, 8.f);
    }
    return depth * za::clamp(scale, 0.f, 4.f);
}

extern "C" int VR_ModelLightParity(void)
{
    return vr_model_light_parity.value != 0.f;
}

// How much a model's skin bumps shade its own light (the alias shader's ModelBumpShade), from the light's direction
// (vr_modellight): vr_normalmap_models times the world's vr_normalmap_baked; the held weapons and hands, a hand's
// width from the eyes, VIEWMODEL_BUMPS of it (their 8-bit skins' made bumps turn to noise that close), unless their
// normal map is authored (`authored`: a real shape, as strong close up as far away).
extern "C" float VR_ModelBumps(const entity_t* e, int authored)
{
    constexpr float VIEWMODEL_BUMPS = 0.5f;
    if(vr_normalmaps.value == 0.f)
    {
        return 0.f;
    }
    const float k = za::clamp(vr_normalmap_models.value, 0.f, 2.f) * za::clamp(vr_normalmap_baked.value, 0.f, 2.f);
    return !authored && (e == &cl.viewent || VR_IsViewEntity(e)) ? k * VIEWMODEL_BUMPS : k;
}

// How much a model's normal map bends its normal (the alias shader's BumpedNormalK): a made one's vr_normalmap_strength
// (the world's LightTweak.w), an authored one's vr_normalmap_authored (its own shape, 1 as drawn); 0 without normal
// maps.
extern "C" float VR_ModelNormalMapScale(int authored)
{
    if(vr_normalmaps.value == 0.f)
    {
        return 0.f;
    }
    return authored ? za::clamp(vr_normalmap_authored.value, 0.f, 2.f) : za::clamp(vr_normalmap_strength.value, 0.f, 8.f);
}

extern "C" float VR_ViewModelMinLight(void)
{
    return za::clamp(vr_viewmodel_minlight.value, 0.f, 128.f);
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
    preset = za::clamp(preset, 0, 4);
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
    look(vr_ambient_light, 0.f); // Quake's maps have no fill light of their own
    look(vr_bloom, 0.f);
    look(vr_tonemap, 0.f); // the eyes' float scene and tone curve, the grade (vr_tonemap.cpp)
    look(vr_grade, 0.f);
    look(vr_flash_scale, 1.f);
    look(vr_explosion_light_scale, 1.f);
    look(vr_colored_lights, 0.f);
    look(vr_projectile_lights, 0.f);
    // The lava nails' lights (a few on Low) and the lightning's stream of lights (Medium and up).
    Cvar_SetQuick(&vr_lavanail_lights, preset >= 2 ? vr_lavanail_lights.default_string : preset == 1 ? "4" : "0");
    Cvar_SetQuick(&vr_lavagun_light, preset >= 1 ? vr_lavagun_light.default_string : "0"); // the lava guns' glow (vr_emissive.cpp): Low and up
    Cvar_SetQuick(&vr_beam_lights, preset >= 2 ? vr_beam_lights.default_string : "0");
    Cvar_SetQuick(&vr_torch_lights, preset >= 2 ? vr_torch_lights.default_string : preset == 1 ? "4" : "0"); // torches' flicker
    look(vr_weapon_screen_light, 0.f);
    look(vr_gadget_light, 0.f);
    look(vr_screen_glow, 0.f);
    look(vr_screen_text_glow, 0.f);
    look(vr_weapon_glow, 0.f);
    look(vr_dlight_uncapped, 0.f);
    look(vr_dlight_falloff, 0.f);
    look(vr_specular, 0.f);
    look(vr_model_light_parity, 0.f);
    look(vr_model_ambient_dir, 0.f); // directional ambient on models (vr_ambient.cpp)
    look(vr_rim_light, 0.f); // rim light on models (vr_envmap.cpp)
    Cvar_SetQuick(&vr_weapon_reflections, preset >= 2 ? "1" : "0"); // weapons' reflections (vr_envmap.cpp): Medium and up
    look(vr_viewmodel_minlight, 24.f);
    look(vr_texture_smooth, 0.f);
    look(vr_alpha_coverage, 0.f); // fences' mips as Quake's (thinning out with distance)
    look(vr_water_splash, 0.f); // liquid splashes (vr_particles.cpp)
    look(vr_particle_light, 0.f); // lit particles (vr_particles.cpp): Quake's were all as bright as their colours
    Cvar_SetQuick(&vr_soft_particles, preset >= 2 ? "1" : "0"); // soft particles and sprites (vr_particles.cpp): Medium and up
    Cvar_SetValueQuick(&vr_normalmaps, p.normalmaps); // made as the next map loads
    Cvar_SetValueQuick(&vr_parallax, p.parallax);
    Cvar_SetQuick(&vr_detail, preset >= 2 ? vr_detail.default_string : "0"); // detail textures (vr_detail.cpp): Medium and up, at the default (off)
    // Dynamic ambient occlusion (vr_ao.cpp): Medium and up.
    Cvar_SetQuick(&vr_ao_dynamic, preset >= 2 ? vr_ao_dynamic.default_string : "0");
    Cvar_SetQuick(&vr_ao_brush, preset >= 2 ? vr_ao_brush.default_string : "0");
    Cvar_SetQuick(&vr_ao_models, preset >= 2 ? vr_ao_models.default_string : "0");
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

// vr_alpha_coverage: the alpha-tested textures' mips made again (with or without their coverage kept).
void onAlphaCoverage(cvar_t*)
{
    TexMgr_ReloadAlphaTested();
}

// vr_light_test [radius] [seconds] [distance] [cone]: a dynamic light in front of the view, to see (and
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
    if(Cmd_Argc() > 4)
    {
        const float cone = za::clamp(static_cast<float>(Q_atof(Cmd_Argv(4))), 1.f, 85.f);
        lighting::dlightSpot(dl, glm::vec3{fwd[0], fwd[1], fwd[2]}, cone * 0.8f, cone);
    }
}

// vr_light_probe: the baked light at a few points round you, as the map has it and as it is drawn
// with the current Graphics > Lights (vr_ambient_light, vr_light_contrast). The same curve lights
// models (VR_AliasLightCurve), so the second column is what a model standing there would be lit by.
void lightProbe_f()
{
    if(cl.worldmodel == nullptr || cl.worldmodel->lightdata == nullptr)
    {
        Con_Printf("light probe: no lit world model to read\n");
        return;
    }

    const float a = lighting::ambientFloor();
    const float c = lighting::lightContrast();
    Con_Printf("light probe: ambient %.2f, contrast %.2f (128 is Quake's full light)\n", a, c);

    vec3_t fwd, right, up;
    AngleVectors(r_refdef.viewangles, fwd, right, up);
    const char* names[6] = {"at you", "64 ahead", "128 ahead", "48 up", "48 down", "64 right"};
    const float along[6] = {0.f, 64.f, 128.f, 0.f, 0.f, 64.f};
    const float side[6] = {0.f, 0.f, 0.f, 0.f, 0.f, 64.f};
    const float rise[6] = {0.f, 0.f, 0.f, 48.f, -48.f, 0.f};
    for(int i = 0; i < 6; i++)
    {
        vec3_t p;
        for(int k = 0; k < 3; k++)
        {
            p[k] = r_refdef.vieworg[k] + fwd[k] * along[i] + right[k] * side[i] + up[k] * rise[i];
        }
        lightcache_t cache = {};
        R_LightPoint(p, 0.f, &cache);
        float drawn[3] = {lightcolor[0], lightcolor[1], lightcolor[2]};
        lighting::lightCurve(drawn);
        Con_Printf("  %-9s baked %3d %3d %3d -> drawn %5.0f %5.0f %5.0f\n", names[i],
                   static_cast<int>(lightcolor[0]), static_cast<int>(lightcolor[1]), static_cast<int>(lightcolor[2]),
                   drawn[0], drawn[1], drawn[2]);
    }
}

} // namespace

void lighting::init()
{
    Cvar_SetCallback(&vr_graphics_preset, onPreset);
    Cmd_AddCommand("vr_light_test", lightTest_f);
    Cmd_AddCommand("vr_light_probe", lightProbe_f);
    Cmd_AddCommand("vr_shadow_layered_check", layeredCheck_f);
    Cvar_SetCallback(&vr_alpha_coverage, onAlphaCoverage);
    ao::init();
}

// Lights given shadows this frame: dynamic ones, and map lights (vr_memstats).
void lighting::shadowCounts(int& dlights, int& mapLights)
{
    dlights = frameEnabled ? portalLightCount : 0;
    for(const DlightSlot& slot : dlightSlots)
    {
        dlights += slot.selected ? 1 : 0;
    }
    mapLights = 0;
    for(const MapSlot& slot : mapSlots)
    {
        mapLights += slot.light >= 0 && slot.fade > 0.f ? 1 : 0;
    }
}
