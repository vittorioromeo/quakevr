// vr_envmap.cpp -- see vr_envmap.hpp.

#include "vr_modelmetadata.hpp"
#include "vr_envmap.hpp"
#include "vr_main.hpp"
#include "vr_cvars.hpp"
#include "vr_gfx.hpp"
#include "vr_hands.hpp"
#include "vr_lighting.hpp"
#include "vr_profile.hpp"
#include "vr_view.hpp"
#include "vr_weapons.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/ToString.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <string.h>

extern "C" float skyflatcolor[3]; // gl_sky.c: the sky's average colour

namespace qvr::envmap
{
namespace
{

constexpr int size = 64;       // each face's texels
constexpr int levels = 7;      // 64 down to 1
constexpr float nearZ = 2.f;   // units
constexpr int stillEvery = 8;  // frames between faces while the cube's place hasn't moved
constexpr float moveUnits = 4.f; // how far the hands move before the cube follows them
constexpr int wantedFrames = 30; // frames the cube is kept up to date after a model last used it
constexpr int numStyles = static_cast<int>(sizeof(r_lightbuffer.lightstyles) / sizeof(r_lightbuffer.lightstyles[0]));
static_assert(2 + numStyles <= 96, "the shaders' Styles[] would run into their Textured and Centre (locations 96, 97)");

GLuint cube = 0;
GLuint faceViews[6] = {}; // each face of the cube as a 2D texture with its levels (glTextureView): its own mipmaps
GLuint depth = 0;
GLuint fbo = 0;
GLuint program = 0;
GLuint colorBuffer = 0; // per vertex of the brush models' vertex buffer: albedo, emissive (RGBA8 each)
GLuint indexBuffer = 0; // the world's faces but the sky, as triangles, a texture's together
GLsizei numIndices = 0;
bool failed = false;

// A texture's triangles in the index buffer (the water cube draws them with it bound).
struct TexRange
{
    texture_t* texture;
    GLsizei first, count;
};
za::Vector<TexRange> texRanges;

// The world's level water and slime faces seen from above (not teleports' or lava's), for the water cube's place:
// the face's height, its box across, the leaf over it (its PVS bit).
struct LevelLiquid
{
    float z;
    glm::vec2 mins, maxs;
    glm::vec2 mid; // its vertices' centroid
    int leaf;      // the one over it, in the PVS's bits (the leaf's number less 1)
};
za::Vector<LevelLiquid> levelLiquids;

// What the buffers were made for.
const qmodel_t* builtWorld = nullptr;
int builtGeneration = -1;
GLuint builtVbo = 0;
size_t builtVboSize = 0;

// The cube: its faces done since it was made (6: usable), the next face, where the current round of faces is seen
// from, frames since the last face.
int facesDone = 0;
int nextFace = 0;
glm::vec3 centre{0.f};
bool centreValid = false;
int sinceFace = 0;
int lastWanted = -1000; // host_framecount a model last asked for reflections

// The cube map's faces (GL's order, +X -X +Y -Y +Z -Z): where each looks, and its up (as GL samples cube maps).
const glm::vec3 faceForward[6] = {{1.f, 0.f, 0.f}, {-1.f, 0.f, 0.f}, {0.f, 1.f, 0.f},
                                  {0.f, -1.f, 0.f}, {0.f, 0.f, 1.f}, {0.f, 0.f, -1.f}};
const glm::vec3 faceUp[6] = {{0.f, -1.f, 0.f}, {0.f, -1.f, 0.f}, {0.f, 0.f, 1.f},
                             {0.f, 0.f, -1.f}, {0.f, -1.f, 0.f}, {0.f, -1.f, 0.f}};

// The world's own attributes (glvert_t: position, texture and lightmap coordinates, the lightmap's offset to its
// other styles, the styles), and each face's colours from the colour buffer.
// (Made once, where the program is: returned by value, nothing kept.)
[[nodiscard]] za::String vertexShader()
{
    return "#version 430\n"
           "#define NSTYLES " + za::toString(numStyles) + "\n" + R"(
layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec4 in_uv;
layout(location = 2) in float in_lmofs;
layout(location = 3) in ivec4 in_styles;
layout(location = 4) in vec4 in_albedo;
layout(location = 5) in vec4 in_emissive;
layout(location = 0) uniform mat4 ViewProj;
layout(location = 2) uniform float Styles[NSTYLES];
layout(location = 0) out vec2 out_lmuv;
layout(location = 1) flat out vec4 out_albedo;
layout(location = 2) flat out vec3 out_emissive;
layout(location = 3) flat out vec4 out_styles;
layout(location = 4) flat out float out_lmofs;
layout(location = 5) out vec3 out_pos;
layout(location = 6) out vec2 out_uv;
float Style(int i)
{
    return i < NSTYLES ? Styles[i] : 1.0;
}
void main()
{
    gl_Position = ViewProj * vec4(in_pos, 1.0);
    out_lmuv = in_uv.zw;
    out_pos = in_pos;
    out_uv = in_uv.xy;
    out_albedo = in_albedo;
    out_emissive = in_emissive.rgb;
    out_lmofs = in_lmofs;
    out_styles.x = Style(in_styles.x);
    if(in_styles.y == 255)
        out_styles.yzw = vec3(-1.0);
    else if(in_styles.z == 255)
        out_styles.yzw = vec3(Style(in_styles.y), -1.0, -1.0);
    else
        out_styles.yzw = vec3(Style(in_styles.y), Style(in_styles.z), Style(in_styles.w));
}
)";
}

// The lightmap as the world shader reads it (its styles, the contrast), times the face's average colour, doubled
// (Quake's overbright), plus its glowing texels. The water's cube (Textured 1): the faces' own textures where they
// have them (the albedo's alpha 1, a cutout's 0.5; the world shader's split of the fullbright texels, alpha 0, and
// its fullbright texture on unit 2), and in alpha how far the texel is from the cube's centre, in units.
constexpr const char* fragmentShader = R"(#version 430
layout(binding = 0) uniform sampler2D LMTex;
layout(binding = 1) uniform sampler2D Tex;
layout(binding = 2) uniform sampler2D FullbrightTex;
layout(location = 1) uniform float Contrast;
layout(location = 98) uniform float Ambient; // QVR: the room's own fill light (vr_ambient_light), as in the world shader
layout(location = 96) uniform int Textured; // 0: the colours (the weapons' cube); 1: the textures, 2: and a fullbright one
layout(location = 97) uniform vec3 Centre;
layout(location = 0) in vec2 in_lmuv;
layout(location = 1) flat in vec4 in_albedo;
layout(location = 2) flat in vec3 in_emissive;
layout(location = 3) flat in vec4 in_styles;
layout(location = 4) flat in float in_lmofs;
layout(location = 5) in vec3 in_pos;
layout(location = 6) in vec2 in_uv;
layout(location = 0) out vec4 Out;
void main()
{
    vec4 tex = vec4(1.0);
    bool textured = Textured > 0 && in_albedo.a > 0.25;
    if(textured)
    {
        tex = texture(Tex, in_uv, 1.0); // a little blurrier than the eyes' (the cube's texels are coarse)
        if(in_albedo.a < 0.75 && tex.a < 0.666) // a cutout's holes
            discard;
    }
    vec4 lm0 = textureLod(LMTex, in_lmuv, 0.0);
    vec3 light;
    if(in_styles.y < 0.0)
        light = in_styles.x * lm0.rgb;
    else
    {
        vec4 lm1 = textureLod(LMTex, vec2(in_lmuv.x + in_lmofs, in_lmuv.y), 0.0);
        if(in_styles.z < 0.0)
            light = in_styles.x * lm0.rgb + in_styles.y * lm1.rgb;
        else
        {
            vec4 lm2 = textureLod(LMTex, vec2(in_lmuv.x + in_lmofs * 2.0, in_lmuv.y), 0.0);
            light = vec3(dot(in_styles, lm0), dot(in_styles, lm1), dot(in_styles, lm2));
        }
    }
    if(Contrast != 1.0 || Ambient > 0.0) // the same curve as the world's (qvr::lighting::lightCurve): the fill light, then the contrast
        light = 0.5 * pow(max(light, vec3(0.0)) * 2.0 + Ambient, vec3(Contrast));
    float dist = distance(in_pos, Centre);
    if(!textured)
    {
        Out = vec4(in_albedo.rgb * light * 2.0 + in_emissive, dist);
        return;
    }
    vec3 c = in_albedo.a < 0.75 ? tex.rgb * light * 2.0 : mix(tex.rgb, tex.rgb * light * 2.0, tex.a);
    if(Textured > 1)
        c += texture(FullbrightTex, in_uv, 1.0).rgb;
    Out = vec4(c, dist);
}
)";

// alpha: the water cube's flag (the fragment shader's in_albedo.a): 255 its texture, 128 a cutout's, 0 the colour
[[nodiscard]] za::U32 packColor(const glm::vec3& c, za::U32 alpha = 255u)
{
    const glm::uvec3 u = glm::uvec3(glm::clamp(c, 0.f, 1.f) * 255.f + 0.5f);
    return u.r | (u.g << 8) | (u.b << 16) | (alpha << 24);
}

// A texture's average colour and alpha: its smallest mip level, read back (once per texture as a map loads; the
// replacement textures' own colours).
[[nodiscard]] glm::vec4 averageColor(const gltexture_t* t)
{
    if(!t || !t->texnum || t->target != GL_TEXTURE_2D)
    {
        return glm::vec4{0.5f, 0.5f, 0.5f, 1.f};
    }
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, t->texnum);
    int level = 0;
    int w = za::max<int>(1, t->width), h = za::max<int>(1, t->height);
    while(w > 1 || h > 1)
    {
        GLint next = 0;
        glGetTexLevelParameteriv(GL_TEXTURE_2D, level + 1, GL_TEXTURE_WIDTH, &next);
        if(next <= 0)
        {
            break;
        }
        level++;
        w = za::max(1, w >> 1);
        h = za::max(1, h >> 1);
    }
    glGetTexLevelParameteriv(GL_TEXTURE_2D, level, GL_TEXTURE_WIDTH, &w);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, level, GL_TEXTURE_HEIGHT, &h);
    if(w <= 0 || h <= 0 || w * h > 256 * 256)
    {
        return glm::vec4{0.5f, 0.5f, 0.5f, 1.f};
    }
    za::Vector<float> px(static_cast<size_t>(w) * h * 4);
    glGetTexImage(GL_TEXTURE_2D, level, GL_RGBA, GL_FLOAT, px.data());
    glm::vec4 sum{0.f};
    for(size_t i = 0; i < px.size(); i += 4)
    {
        sum += glm::vec4{px[i], px[i + 1], px[i + 2], px[i + 3]};
    }
    return sum / static_cast<float>(w * h);
}

void resetWater();

void destroyWorld()
{
    resetWater();
    for(GLuint* b : {&colorBuffer, &indexBuffer})
    {
        if(*b)
        {
            GL_DeleteBuffer(*b);
            *b = 0;
        }
    }
    numIndices = 0;
    texRanges.clear();
    levelLiquids.clear();
    builtWorld = nullptr;
    builtVbo = 0;
    builtVboSize = 0;
    facesDone = 0;
    centreValid = false;
}

// The world's faces (not its brush entities, not the sky: the cube is cleared to the sky's colour), each in its
// texture's average colour: lit by the lightmap, its fullbright texels glowing; liquids unlit.
void buildWorld()
{
    destroyWorld();
    const double start = Sys_DoubleTime();
    qmodel_t* m = cl.worldmodel;
    const size_t numVerts = gl_bmodel_vbo_size / sizeof(glvert_t);
    if(!m || !gl_bmodel_vbo || !numVerts)
    {
        return;
    }

    struct Colors
    {
        za::U32 albedo, emissive; // packed
    };
    ankerl::unordered_dense::map<const texture_t*, Colors> colors;
    za::Vector<za::U32> perVertex(numVerts * 2, 0u);
    ankerl::unordered_dense::map<texture_t*, za::Vector<za::U32>> byTexture;
    za::Vector<texture_t*> textureOrder; // as first met (the same each build)
    for(int i = 0; i < m->nummodelsurfaces; i++)
    {
        const msurface_t* s = &m->surfaces[m->firstmodelsurface + i];
        if((s->flags & SURF_DRAWSKY) || s->numedges < 3 || s->vbo_firstvert < 0 ||
            static_cast<size_t>(s->vbo_firstvert + s->numedges) > numVerts)
        {
            continue;
        }
        texture_t* t = m->textures[s->texinfo->texnum];
        if(!t || !t->gltexture)
        {
            continue;
        }
        if((t->type == TEXTYPE_WATER || t->type == TEXTYPE_SLIME) && za::fabs(s->plane->normal[2]) > 0.99f)
        {
            // (both sides of a level face share its plane; seen from above where the air is over it)
            LevelLiquid l;
            l.z = s->plane->dist * s->plane->normal[2];
            l.mins = {s->mins[0], s->mins[1]};
            l.maxs = {s->maxs[0], s->maxs[1]};
            glm::vec2 mid{0.f}; // its vertices' centroid (in the face: it is convex)
            for(int k = 0; k < s->numedges; k++)
            {
                const int e = m->surfedges[s->firstedge + k];
                const float* v = m->vertexes[e >= 0 ? m->edges[e].v[0] : m->edges[-e].v[1]].position;
                mid += glm::vec2{v[0], v[1]};
            }
            mid /= static_cast<float>(s->numedges);
            l.mid = mid;
            vec3_t above = {mid.x, mid.y, l.z + 2.f};
            const mleaf_t* leaf = Mod_PointInLeaf(above, m);
            l.leaf = leaf && leaf->contents != CONTENTS_SOLID ? static_cast<int>(leaf - m->leafs) - 1 : -1;
            if(l.leaf >= 0 && leaf->contents == CONTENTS_EMPTY)
            {
                levelLiquids.pushBack(l);
            }
        }
        auto it = colors.find(t);
        if(it == colors.end())
        {
            const glm::vec4 base = averageColor(t->gltexture);
            glm::vec3 albedo{base};
            glm::vec3 emissive{0.f};
            if(t->type != TEXTYPE_CUTOUT && !TEXTYPE_ISLIQUID(t->type))
            {
                // Ironwail's fullbright texels in the texture's alpha (0: unlit), or a texture of their own.
                albedo = glm::vec3{base} * base.a;
                emissive = glm::vec3{base} * (1.f - base.a);
            }
            if(t->fullbright)
            {
                emissive += glm::vec3{averageColor(t->fullbright)};
            }
            const za::U32 flag = TEXTYPE_ISLIQUID(t->type) ? 0u : t->type == TEXTYPE_CUTOUT ? 128u : 255u;
            it = colors.emplace(t, Colors{packColor(albedo, flag), packColor(emissive)}).first;
        }
        for(int k = 0; k < s->numedges; k++)
        {
            perVertex[(s->vbo_firstvert + k) * 2] = it->second.albedo;
            perVertex[(s->vbo_firstvert + k) * 2 + 1] = it->second.emissive;
        }
        auto bt = byTexture.find(t);
        if(bt == byTexture.end())
        {
            bt = byTexture.emplace(t, za::Vector<za::U32>{}).first;
            textureOrder.pushBack(t);
        }
        for(int k = 2; k < s->numedges; k++)
        {
            bt->second.pushBack(s->vbo_firstvert);
            bt->second.pushBack(s->vbo_firstvert + k - 1);
            bt->second.pushBack(s->vbo_firstvert + k);
        }
    }
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, 0);

    za::Vector<za::U32> indices;
    for(texture_t* t : textureOrder)
    {
        const za::Vector<za::U32>& list = byTexture.find(t)->second;
        texRanges.pushBack(TexRange{t, static_cast<GLsizei>(indices.size()), static_cast<GLsizei>(list.size())});
        for(const za::U32 index : list)
        {
            indices.pushBack(index);
        }
    }

    builtWorld = m;
    builtGeneration = worldGeneration();
    builtVbo = gl_bmodel_vbo;
    builtVboSize = gl_bmodel_vbo_size;
    if(indices.empty())
    {
        return;
    }
    colorBuffer = GL_CreateBuffer(GL_ARRAY_BUFFER, GL_STATIC_DRAW, "vr envmap colours",
        perVertex.size() * sizeof(perVertex[0]), perVertex.data());
    indexBuffer = GL_CreateBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_STATIC_DRAW, "vr envmap indices",
        indices.size() * sizeof(indices[0]), indices.data());
    numIndices = static_cast<GLsizei>(indices.size());
    Con_DPrintf("VR envmap: %d triangles, %d textures, built in %.1f ms\n", numIndices / 3,
        static_cast<int>(colors.size()), (Sys_DoubleTime() - start) * 1000.0);
}

[[nodiscard]] bool ensureTargets()
{
    if(cube || failed)
    {
        return !failed;
    }
    program = gfx::glProgram(vertexShader().cStr(), fragmentShader, "vr envmap");
    if(!program)
    {
        failed = true;
        return false;
    }

    glGenTextures(1, &cube);
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_CUBE_MAP, cube);
    GL_TexStorage2DFunc(GL_TEXTURE_CUBE_MAP, levels, GL_R11F_G11F_B10F, size, size);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    GL_ObjectLabelFunc(GL_TEXTURE, cube, -1, "vr envmap");

    // One view per face, so that only the face just drawn gets its mipmaps made again (not all six).
    using TextureViewFn = void(APIENTRY*)(GLuint, GLenum, GLuint, GLenum, GLuint, GLuint, GLuint, GLuint);
    if(const auto textureView = reinterpret_cast<TextureViewFn>(SDL_GL_GetProcAddress("glTextureView")))
    {
        glGenTextures(6, faceViews);
        for(int f = 0; f < 6; f++)
        {
            textureView(faceViews[f], GL_TEXTURE_2D, cube, GL_R11F_G11F_B10F, 0, levels, static_cast<GLuint>(f), 1);
        }
    }

    glGenTextures(1, &depth);
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, depth);
    GL_TexStorage2DFunc(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT32F, size, size);
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, 0);

    GL_GenFramebuffersFunc(1, &fbo);
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, fbo);
    GL_FramebufferTexture2DFunc(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X, cube, 0);
    GL_FramebufferTexture2DFunc(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth, 0);
    const bool ok = GL_CheckFramebufferStatusFunc(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, 0);
    if(!ok)
    {
        Con_Warning("VR envmap: framebuffer incomplete\n");
        shutdown();
        failed = true;
        return false;
    }
    facesDone = 0;
    return true;
}

// Where the cube is seen from: between the hands (the weapons), or the head if that is in a wall.
[[nodiscard]] glm::vec3 viewPoint()
{
    const hands::State& s = hands::current();
    glm::vec3 p = (s.pos[0] + s.pos[1]) * 0.5f;
    vec3_t q{p.x, p.y, p.z};
    if(Mod_PointInLeaf(q, cl.worldmodel)->contents == CONTENTS_SOLID)
    {
        p = s.head;
    }
    return p;
}

// Draws a face of a cube seen from `at`: its texture's level 0 attached to the framebuffer bound, cleared to `clear`
// (alpha: the cube's distance there), its program in use and the world's buffers bound (bindWorld).
void drawFace(GLuint tex, int face, const glm::vec3& at, const GLfloat clear[4], bool textured)
{
    // Reversed depth to [0, 1] (the engine's glClipControl), infinitely far: depth = near / distance.
    glm::mat4 proj{0.f};
    proj[0][0] = 1.f; // 90 degrees
    proj[1][1] = 1.f;
    proj[2][3] = -1.f;
    proj[3][2] = nearZ;
    const glm::mat4 viewProj = proj * glm::lookAt(at, at + faceForward[face], faceUp[face]);

    GL_FramebufferTexture2DFunc(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, tex, 0);
    const GLfloat far0 = 0.f;
    GL_ClearBufferfvFunc(GL_COLOR, 0, clear);
    GL_ClearBufferfvFunc(GL_DEPTH, 0, &far0);
    GL_UniformMatrix4fvFunc(0, 1, GL_FALSE, glm::value_ptr(viewProj));
    GL_Uniform3fFunc(97, at.x, at.y, at.z);
    if(!textured)
    {
        GL_Uniform1iFunc(96, 0);
        glDrawElements(GL_TRIANGLES, numIndices, GL_UNSIGNED_INT, nullptr);
        return;
    }
    for(const TexRange& r : texRanges) // a texture at a time, as it is now (animated ones)
    {
        const texture_t* t = R_TextureAnimation(r.texture, 0);
        if(!t || !t->gltexture)
        {
            t = r.texture;
        }
        GL_Bind(GL_TEXTURE1, t->gltexture);
        if(t->fullbright)
        {
            GL_Bind(GL_TEXTURE2, t->fullbright);
        }
        GL_Uniform1iFunc(96, t->fullbright ? 2 : 1);
        glDrawElements(GL_TRIANGLES, r.count, GL_UNSIGNED_INT,
            reinterpret_cast<void*>(static_cast<uintptr_t>(r.first) * sizeof(za::U32)));
    }
}

// The program, its uniforms and the world's vertex, colour and index buffers, for drawFace.
void bindWorld()
{
    GL_SetState(GLS_BLEND_OPAQUE | GLS_CULL_NONE | GLS_ATTRIBS(6));
    GL_UseProgram(program);
    GL_Uniform1fFunc(1, lighting::lightContrast());
    GL_Uniform1fFunc(98, lighting::ambientFloor()); // the reflections are lit as the room is
    for(int i = 0; i < numStyles; i++)
    {
        GL_Uniform1fFunc(2 + i, r_lightbuffer.lightstyles[i]);
    }
    GL_Bind(GL_TEXTURE0, lightmap_texture);

    GL_BindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
    GL_BindBuffer(GL_ARRAY_BUFFER, gl_bmodel_vbo);
    GL_VertexAttribPointerFunc(0, 3, GL_FLOAT, GL_FALSE, sizeof(glvert_t), (void*)offsetof(glvert_t, pos));
    GL_VertexAttribPointerFunc(1, 4, GL_FLOAT, GL_FALSE, sizeof(glvert_t), (void*)offsetof(glvert_t, st));
    GL_VertexAttribPointerFunc(2, 1, GL_FLOAT, GL_FALSE, sizeof(glvert_t), (void*)offsetof(glvert_t, lmofs));
    GL_VertexAttribIPointerFunc(3, 4, GL_UNSIGNED_BYTE, sizeof(glvert_t), (void*)offsetof(glvert_t, styles));
    GL_BindBuffer(GL_ARRAY_BUFFER, colorBuffer);
    GL_VertexAttribPointerFunc(4, 4, GL_UNSIGNED_BYTE, GL_TRUE, 8, (void*)0);
    GL_VertexAttribPointerFunc(5, 4, GL_UNSIGNED_BYTE, GL_TRUE, 8, (void*)4);
}

// The world's buffers for this map (made again for a new one, or the brush models' vertices made again).
[[nodiscard]] bool ensureWorld()
{
    if(builtWorld != cl.worldmodel || builtGeneration != worldGeneration() || builtVbo != gl_bmodel_vbo || builtVboSize != gl_bmodel_vbo_size)
    {
        QVR_PROFILE("env cube build");
        buildWorld();
    }
    return numIndices && ensureTargets();
}

// ----------------------------------------------------------------------------
// The water's cube (vr_water_reflections): the room round the level water or slime nearest the head that it sees
// from above, as seen from just over that surface (`above` units), textured (the weapons' cube has each texture's
// average colour: fine on a gun's small curved metal, blotchy in a calm pool), its texels' distances from its centre
// in alpha. The liquid shaders (vr_glsl.h, LiquidReflection) follow the reflected ray from each point of the surface
// out to the distances it gives, for a reflection where things are (not at infinity, as a plain cube map's). Two
// cubes in turn: a round of six faces (two a frame) is drawn into the back one from one place while the front one
// is read, then they swap, so the faces read are all seen from where the shaders think. Once a frame, for both eyes.
namespace wet
{

constexpr int size = 128;           // each face's texels
constexpr int levels = 8;           // 128 down to 1
constexpr float above = 8.f;        // units over the surface its centre is
constexpr float range = 1536.f;     // how far from the head a surface may be to be chosen
constexpr float switchMargin = 64.f; // how much nearer another height's surface must be to take the cube from this one's
constexpr float moveUnits = 32.f;   // how far the place wanted moves before a cube is made there
constexpr int facesPerFrame = 2;
constexpr int refreshEvery = 12;    // frames between rounds while the place stays (flickering lights, doors)
constexpr float skyDistance = 8192.f;
constexpr float fadeDistance = 1280.f; // how far across from its centre the reflection fades out (from 0.6 of it)

struct Cube
{
    GLuint tex = 0;
    glm::vec3 centre{0.f};
    float planeZ = 0.f;
    bool valid = false;
};
Cube cubes[2];
int front = 0;
GLuint depth = 0;
GLuint fbo = 0;
bool failed = false;
bool active = false;  // a surface was chosen this frame (the shaders read the front cube)
bool inRound = false; // a round of faces under way into the back cube
int nextFace = 0;
int sinceRound = 0;

[[nodiscard]] bool ensureTargets()
{
    if(fbo || failed)
    {
        return !failed;
    }
    for(Cube& c : cubes)
    {
        glGenTextures(1, &c.tex);
        GL_BindNative(GL_TEXTURE0, GL_TEXTURE_CUBE_MAP, c.tex);
        GL_TexStorage2DFunc(GL_TEXTURE_CUBE_MAP, levels, GL_RGBA16F, size, size);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        GL_ObjectLabelFunc(GL_TEXTURE, c.tex, -1, "vr water cube");
        c.valid = false;
    }
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_CUBE_MAP, 0);

    glGenTextures(1, &depth);
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, depth);
    GL_TexStorage2DFunc(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT32F, size, size);
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, 0);

    GL_GenFramebuffersFunc(1, &fbo);
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, fbo);
    GL_FramebufferTexture2DFunc(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X, cubes[0].tex, 0);
    GL_FramebufferTexture2DFunc(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth, 0);
    const bool ok = GL_CheckFramebufferStatusFunc(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, 0);
    if(!ok)
    {
        Con_Warning("VR water cube: framebuffer incomplete\n");
        failed = true;
        return false;
    }
    return true;
}

void destroy()
{
    if(fbo)
    {
        GL_DeleteFramebuffersFunc(1, &fbo);
        fbo = 0;
    }
    for(Cube& c : cubes)
    {
        if(c.tex)
        {
            GL_DeleteNativeTexture(c.tex);
            c.tex = 0;
        }
        c.valid = false;
    }
    if(depth)
    {
        GL_DeleteNativeTexture(depth);
        depth = 0;
    }
    failed = false;
    active = false;
    inRound = false;
}

// Where the cube is wanted: over the level water or slime nearest the head (within `range`) that is below it and in
// its PVS, at the point of that face nearest the head (its centroid if that is in a wall); keeping to the height of
// the cube read unless another's is clearly nearer. False: none (or the head in a wall or a liquid).
[[nodiscard]] bool wantedPlace(glm::vec3& at, float& planeZ)
{
    const glm::vec3 head = hands::current().head;
    vec3_t h{head.x, head.y, head.z};
    mleaf_t* leaf = Mod_PointInLeaf(h, cl.worldmodel);
    if(!leaf || leaf->contents != CONTENTS_EMPTY)
    {
        return false;
    }
    const byte* vis = Mod_LeafPVS(leaf, cl.worldmodel);
    const Cube& f = cubes[front];
    const LevelLiquid* best = nullptr;
    const LevelLiquid* bestHere = nullptr; // at the front cube's height
    float bestDist = range, bestHereDist = range;
    for(const LevelLiquid& l : levelLiquids)
    {
        if(l.z > head.z - 1.f || (vis && !(vis[l.leaf >> 3] & (1 << (l.leaf & 7)))))
        {
            continue;
        }
        const glm::vec2 nearest = glm::clamp(glm::vec2{head}, l.mins, l.maxs);
        const float d = glm::length(glm::vec3{nearest - glm::vec2{head}, l.z - head.z});
        if(d < bestDist)
        {
            bestDist = d;
            best = &l;
        }
        if(f.valid && za::fabs(l.z - f.planeZ) < 0.5f && d < bestHereDist)
        {
            bestHereDist = d;
            bestHere = &l;
        }
    }
    if(bestHere && bestHereDist <= bestDist + switchMargin)
    {
        best = bestHere;
    }
    if(!best)
    {
        return false;
    }
    planeZ = best->z;
    at = glm::vec3{glm::clamp(glm::vec2{head}, best->mins, best->maxs), best->z + above};
    vec3_t p{at.x, at.y, at.z};
    if(Mod_PointInLeaf(p, cl.worldmodel)->contents == CONTENTS_SOLID)
    {
        at = glm::vec3{best->mid, best->z + above};
    }
    return true;
}

void update()
{
    active = false;
    if(vr_water_reflections.value <= 0.f || vr_water_fresnel.value <= 0.f || !cl.worldmodel || !gl_clipcontrol_able ||
        !lightmap_texture)
    {
        return;
    }
    if(!ensureWorld() || levelLiquids.empty() || !ensureTargets())
    {
        return;
    }
    glm::vec3 at;
    float planeZ = 0.f;
    if(!wantedPlace(at, planeZ))
    {
        return;
    }
    active = true;

    Cube& f = cubes[front];
    Cube& b = cubes[1 - front];
    if(!inRound)
    {
        const bool moved = !f.valid || za::fabs(planeZ - f.planeZ) > 0.5f || glm::distance(at, f.centre) > moveUnits;
        if(!moved && ++sinceRound < refreshEvery)
        {
            return;
        }
        inRound = true;
        nextFace = 0;
        sinceRound = 0;
        b.valid = false;
        b.centre = moved ? at : f.centre;
        b.planeZ = moved ? planeZ : f.planeZ;
    }

    QVR_GPU_PROFILE("water cube");
    GL_BeginGroup("VR water cube");
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, size, size);
    bindWorld();
    const GLfloat sky[4] = {skyflatcolor[0], skyflatcolor[1], skyflatcolor[2], skyDistance};
    for(int i = 0; i < facesPerFrame && nextFace < 6; i++)
    {
        drawFace(b.tex, nextFace++, b.centre, sky, true);
    }
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, 0);
    if(nextFace >= 6)
    {
        GL_BindNative(GL_TEXTURE0, GL_TEXTURE_CUBE_MAP, b.tex);
        GL_GenerateMipmapFunc(GL_TEXTURE_CUBE_MAP);
        GL_BindNative(GL_TEXTURE0, GL_TEXTURE_CUBE_MAP, 0);
        b.valid = true;
        front = 1 - front;
        inRound = false;
    }
    GL_EndGroup();
}

} // namespace wet

void resetWater()
{
    for(wet::Cube& c : wet::cubes)
    {
        c.valid = false;
    }
    wet::inRound = false;
    wet::active = false;
}

} // namespace

void update()
{
    wet::update(); // the water's (vr_water_reflections)

    if(vr_weapon_reflections.value <= 0.f || !cl.worldmodel || !gl_clipcontrol_able || !lightmap_texture ||
        host_framecount - lastWanted > wantedFrames)
    {
        return;
    }
    if(!ensureWorld())
    {
        return;
    }

    // One face a frame while the cube's round is under way or the hands have moved, else one every few frames
    // (flickering lights, doors); each round of six faces is seen from one place.
    const glm::vec3 p = viewPoint();
    const bool moved = !centreValid || glm::distance(p, centre) > moveUnits;
    ++sinceFace;
    if(facesDone >= 6 && !moved && sinceFace < stillEvery)
    {
        return;
    }
    if(nextFace == 0)
    {
        centre = p;
        centreValid = true;
    }
    sinceFace = 0;

    QVR_GPU_PROFILE("env cube");
    GL_BeginGroup("VR envmap");
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, size, size);
    bindWorld();

    const int face = nextFace;
    const GLfloat sky[4] = {0.16f, 0.17f, 0.2f, 1.f};
    drawFace(cube, face, centre, sky, false);
    nextFace = (nextFace + 1) % 6;
    facesDone = za::min(facesDone + 1, 1 << 20);

    GL_BindFramebufferFunc(GL_FRAMEBUFFER, 0);
    if(faceViews[face])
    {
        // That face's levels only: the others' are as they were made.
        GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, faceViews[face]);
        GL_GenerateMipmapFunc(GL_TEXTURE_2D);
        GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, 0);
    }
    else
    {
        GL_BindNative(GL_TEXTURE0, GL_TEXTURE_CUBE_MAP, cube);
        GL_GenerateMipmapFunc(GL_TEXTURE_CUBE_MAP);
        GL_BindNative(GL_TEXTURE0, GL_TEXTURE_CUBE_MAP, 0);
    }
    GL_EndGroup();
}

// vr_envmap_dump: the cube's faces (+X -X +Y -Y +Z -Z, as GL samples them) side by side into <gamedir>/envmap.tga.
void dump_f()
{
    if(!cube || facesDone < 6)
    {
        Con_Printf("vr_envmap_dump: no cube yet (hold a weapon, vr_weapon_reflections 1)\n");
        return;
    }
    za::Vector<float> face(size * size * 3);
    za::Vector<za::U8> img(18 + size * 6 * size * 3, 0);
    img[2] = 2; // uncompressed true colour
    img[12] = static_cast<za::U8>((size * 6) & 255);
    img[13] = static_cast<za::U8>((size * 6) >> 8);
    img[14] = static_cast<za::U8>(size & 255);
    img[15] = static_cast<za::U8>(size >> 8);
    img[16] = 24;
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_CUBE_MAP, cube);
    for(int f = 0; f < 6; f++)
    {
        glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, 0, GL_RGB, GL_FLOAT, face.data());
        for(int y = 0; y < size; y++)
        {
            for(int x = 0; x < size; x++)
            {
                const float* c = &face[(y * size + x) * 3];
                za::U8* d = &img[18 + (y * size * 6 + f * size + x) * 3];
                d[0] = static_cast<za::U8>(za::clamp(c[2], 0.f, 1.f) * 255.f);
                d[1] = static_cast<za::U8>(za::clamp(c[1], 0.f, 1.f) * 255.f);
                d[2] = static_cast<za::U8>(za::clamp(c[0], 0.f, 1.f) * 255.f);
            }
        }
    }
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_CUBE_MAP, 0);
    const char* path = va("%s/envmap.tga", com_gamedir);
    if(FILE* file = fopen(path, "wb"))
    {
        fwrite(img.data(), 1, img.size(), file);
        fclose(file);
        Con_Printf("Wrote %s (centre %.0f %.0f %.0f)\n", path, centre.x, centre.y, centre.z);
    }
}

void init()
{
    Cmd_AddCommand("vr_envmap_dump", dump_f);
}

void shutdown()
{
    destroyWorld();
    wet::destroy();
    if(fbo)
    {
        GL_DeleteFramebuffersFunc(1, &fbo);
        fbo = 0;
    }
    for(GLuint& v : faceViews)
    {
        if(v)
        {
            GL_DeleteNativeTexture(v);
            v = 0;
        }
    }
    for(GLuint* t : {&cube, &depth})
    {
        if(*t)
        {
            GL_DeleteNativeTexture(*t);
            *t = 0;
        }
    }
    if(program)
    {
        GL_DeleteProgramFunc(program);
        program = 0;
    }
    facesDone = 0;
    nextFace = 0;
    failed = false;
}

[[nodiscard]] bool ready()
{
    return cube && facesDone >= 6;
}

[[nodiscard]] unsigned texture()
{
    return cube;
}

[[nodiscard]] const glm::vec3& place()
{
    return centre;
}

void wanted()
{
    lastWanted = host_framecount;
}

void waterFrameData(float out[4], float out2[4], bool eyeInLiquid)
{
    const wet::Cube& f = wet::cubes[wet::front];
    const bool on = wet::active && f.valid && !eyeInLiquid;
    out[0] = f.centre.x;
    out[1] = f.centre.y;
    out[2] = f.centre.z;
    out[3] = on ? za::clamp(vr_water_reflections.value, 0.f, 1.f) : 0.f;
    out2[0] = f.planeZ;
    out2[1] = wet::fadeDistance;
    out2[2] = 0.5f; // a little softer than its texels: they are coarse
    out2[3] = static_cast<float>(wet::levels - 1);
}

unsigned waterCubeTexture()
{
    const wet::Cube& f = wet::cubes[wet::front];
    return wet::active && f.valid ? f.tex : 0u;
}

} // namespace qvr::envmap

namespace
{

using namespace qvr;

// How metal a model is (0 none .. 1): the share of its grey and blue-grey texels (the alias shader's mask) that
// reflect. Held weapons by their model; the rest none.
[[nodiscard]] float weaponMetal(modelmeta::Id id, float& lod)
{
    struct Entry
    {
        modelmeta::Id id;
        float metal;
        float lod; // the cube's mip level read: blurrier (rougher) the higher
    };
    static constexpr Entry table[] = {
        {modelmeta::Id::VShot, 1.f, 2.5f},    {modelmeta::Id::VShot2, 1.f, 2.5f}, {modelmeta::Id::VNail, 1.f, 2.5f},
        {modelmeta::Id::VNail2, 1.f, 2.5f},   {modelmeta::Id::VRock, 0.8f, 3.f},  {modelmeta::Id::VRock2, 0.8f, 3.f},
        {modelmeta::Id::VLight, 0.9f, 2.5f},  {modelmeta::Id::VAxe, 0.9f, 2.f},   {modelmeta::Id::VKsword, 1.f, 1.5f},
        {modelmeta::Id::VHksword, 1.f, 1.5f}, {modelmeta::Id::VHammer, 0.8f, 2.5f},
        {modelmeta::Id::VCrowbar, 0.5f, 3.f}, // (painted: its worn steel shows at the edges and the ends)
        {modelmeta::Id::Mg3SuperAxe, 0.8f, 2.5f}, {modelmeta::Id::Mg3SuperAxeGlow, 0.8f, 2.5f}, // (MG3's, as Mjolnir)
    };
    for(const Entry& e : table)
    {
        if(id == e.id)
        {
            lod = e.lod;
            return e.metal;
        }
    }
    lod = 3.f;
    return 0.7f; // other weapons (the expansions')
}

} // namespace

// The alias instance's rim light and reflections (r_alias.c): x the rim light's strength, y the reflections'
// (0 none), z the cube's mip level they read, w unused.
extern "C" void VR_AliasSurface(const entity_t* e, float out[4])
{
    out[0] = out[1] = out[2] = out[3] = 0.f;
    if(!e || !e->model || e->model->type != mod_alias)
    {
        return;
    }
    const auto& info = modelmeta::get(e->model);
    const bool view = e == &cl.viewent || VR_IsViewEntity(e);
    const bool self = e == &cl_entities[cl.viewentity];
    const bool weapon = view && weapons::slotForModel(e->model) >= 0 && !info.has(modelmeta::Trait::Hand) &&
                        !info.has(modelmeta::Trait::Finger);

    // Rim light: monsters and things full, held weapons less, your hands a little, your body not (seen from
    // inside, its edges are everywhere).
    const float rim = za::clamp(vr_rim_light.value, 0.f, 1.f);
    if(self)
    {
        out[0] = 0.f;
    }
    else if(weapon)
    {
        out[0] = rim * 0.6f;
    }
    else if(view)
    {
        const bool hand = info.has(modelmeta::Trait::Hand) || info.has(modelmeta::Trait::Finger);
        out[0] = hand ? rim * 0.35f : 0.f;
    }
    else
    {
        out[0] = rim;
    }

    // Reflections: held weapons, and weapons lying close to the cube's place (its view of the room is roughly
    // theirs within a few metres).
    const float strength = za::clamp(vr_weapon_reflections_strength.value, 0.f, 2.f);
    if(vr_weapon_reflections.value <= 0.f || strength <= 0.f)
    {
        return;
    }
    float lod = 3.f;
    float metal = 0.f;
    if(weapon)
    {
        metal = weaponMetal(modelmeta::get(e->model).id, lod);
    }
    else if(!view && (info.has(modelmeta::Trait::WorldWeapon) || info.has(modelmeta::Trait::ViewWeapon)))
    {
        metal = weaponMetal(modelmeta::get(e->model).id, lod) * 0.8f;
        const glm::vec3 o{e->origin[0], e->origin[1], e->origin[2]};
        metal *= 1.f - glm::smoothstep(96.f, 256.f, glm::distance(o, envmap::place()));
        lod += 0.5f;
    }
    if(metal <= 0.f)
    {
        return;
    }
    envmap::wanted();
    if(!envmap::ready())
    {
        return;
    }
    out[1] = metal * strength;
    out[2] = lod;
    out[3] = vr_weapon_reflections.value >= 2.f ? 1.f : 0.f; // debug: the cube as a mirror, all over the model
}

// The reflections' cube map for the alias shader (0: none yet).
extern "C" unsigned VR_EnvCubeTexture(void)
{
    return qvr::envmap::ready() ? qvr::envmap::texture() : 0u;
}

// The water's cube map for the liquid shaders (unit 16, LiquidCube; 0: none this frame).
extern "C" unsigned VR_WaterCubeTexture(void)
{
    return qvr::envmap::waterCubeTexture();
}
