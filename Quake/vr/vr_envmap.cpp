// vr_envmap.cpp -- see vr_envmap.hpp.

#include "vr_envmap.hpp"
#include "vr_cvars.hpp"
#include "vr_gfx.hpp"
#include "vr_hands.hpp"
#include "vr_profile.hpp"
#include "vr_view.hpp"
#include "vr_weapons.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

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

GLuint cube = 0;
GLuint depth = 0;
GLuint fbo = 0;
GLuint program = 0;
GLuint colorBuffer = 0; // per vertex of the brush models' vertex buffer: albedo, emissive (RGBA8 each)
GLuint indexBuffer = 0; // the world's faces but the sky, as triangles
GLsizei numIndices = 0;
bool failed = false;

// What the buffers were made for.
const qmodel_t* builtWorld = nullptr;
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
const char* vertexShader()
{
    static const std::string src = "#version 430\n"
                                   "#define NSTYLES " + std::to_string(numStyles) + "\n" + R"(
layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec4 in_uv;
layout(location = 2) in float in_lmofs;
layout(location = 3) in ivec4 in_styles;
layout(location = 4) in vec4 in_albedo;
layout(location = 5) in vec4 in_emissive;
layout(location = 0) uniform mat4 ViewProj;
layout(location = 2) uniform float Styles[NSTYLES];
layout(location = 0) out vec2 out_lmuv;
layout(location = 1) flat out vec3 out_albedo;
layout(location = 2) flat out vec3 out_emissive;
layout(location = 3) flat out vec4 out_styles;
layout(location = 4) flat out float out_lmofs;
float Style(int i)
{
    return i < NSTYLES ? Styles[i] : 1.0;
}
void main()
{
    gl_Position = ViewProj * vec4(in_pos, 1.0);
    out_lmuv = in_uv.zw;
    out_albedo = in_albedo.rgb;
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
    return src.c_str();
}

// The lightmap as the world shader reads it (its styles, the contrast), times the face's average colour, doubled
// (Quake's overbright), plus its glowing texels.
constexpr const char* fragmentShader = R"(#version 430
layout(binding = 0) uniform sampler2D LMTex;
layout(location = 1) uniform float Contrast;
layout(location = 0) in vec2 in_lmuv;
layout(location = 1) flat in vec3 in_albedo;
layout(location = 2) flat in vec3 in_emissive;
layout(location = 3) flat in vec4 in_styles;
layout(location = 4) flat in float in_lmofs;
layout(location = 0) out vec4 Out;
void main()
{
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
    if(Contrast != 1.0)
        light = 0.5 * pow(max(light, vec3(0.0)) * 2.0, vec3(Contrast));
    Out = vec4(in_albedo * light * 2.0 + in_emissive, 1.0);
}
)";

[[nodiscard]] std::uint32_t packColor(const glm::vec3& c)
{
    const glm::uvec3 u = glm::uvec3(glm::clamp(c, 0.f, 1.f) * 255.f + 0.5f);
    return u.r | (u.g << 8) | (u.b << 16) | (255u << 24);
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
    int w = std::max<int>(1, t->width), h = std::max<int>(1, t->height);
    while(w > 1 || h > 1)
    {
        GLint next = 0;
        glGetTexLevelParameteriv(GL_TEXTURE_2D, level + 1, GL_TEXTURE_WIDTH, &next);
        if(next <= 0)
        {
            break;
        }
        level++;
        w = std::max(1, w >> 1);
        h = std::max(1, h >> 1);
    }
    glGetTexLevelParameteriv(GL_TEXTURE_2D, level, GL_TEXTURE_WIDTH, &w);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, level, GL_TEXTURE_HEIGHT, &h);
    if(w <= 0 || h <= 0 || w * h > 256 * 256)
    {
        return glm::vec4{0.5f, 0.5f, 0.5f, 1.f};
    }
    std::vector<float> px(static_cast<size_t>(w) * h * 4);
    glGetTexImage(GL_TEXTURE_2D, level, GL_RGBA, GL_FLOAT, px.data());
    glm::vec4 sum{0.f};
    for(size_t i = 0; i < px.size(); i += 4)
    {
        sum += glm::vec4{px[i], px[i + 1], px[i + 2], px[i + 3]};
    }
    return sum / static_cast<float>(w * h);
}

void destroyWorld()
{
    for(GLuint* b : {&colorBuffer, &indexBuffer})
    {
        if(*b)
        {
            GL_DeleteBuffer(*b);
            *b = 0;
        }
    }
    numIndices = 0;
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

    std::unordered_map<const texture_t*, std::pair<std::uint32_t, std::uint32_t>> colors;
    std::vector<std::uint32_t> perVertex(numVerts * 2, 0u);
    std::vector<std::uint32_t> indices;
    for(int i = 0; i < m->nummodelsurfaces; i++)
    {
        const msurface_t* s = &m->surfaces[m->firstmodelsurface + i];
        if((s->flags & SURF_DRAWSKY) || s->numedges < 3 || s->vbo_firstvert < 0 ||
            static_cast<size_t>(s->vbo_firstvert + s->numedges) > numVerts)
        {
            continue;
        }
        const texture_t* t = m->textures[s->texinfo->texnum];
        if(!t || !t->gltexture)
        {
            continue;
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
            it = colors.emplace(t, std::make_pair(packColor(albedo), packColor(emissive))).first;
        }
        for(int k = 0; k < s->numedges; k++)
        {
            perVertex[(s->vbo_firstvert + k) * 2] = it->second.first;
            perVertex[(s->vbo_firstvert + k) * 2 + 1] = it->second.second;
        }
        for(int k = 2; k < s->numedges; k++)
        {
            indices.push_back(s->vbo_firstvert);
            indices.push_back(s->vbo_firstvert + k - 1);
            indices.push_back(s->vbo_firstvert + k);
        }
    }
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, 0);

    builtWorld = m;
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
    program = gfx::glProgram(vertexShader(), fragmentShader, "vr envmap");
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

void renderFace(int face)
{
    // Reversed depth to [0, 1] (the engine's glClipControl), infinitely far: depth = near / distance.
    glm::mat4 proj{0.f};
    proj[0][0] = 1.f; // 90 degrees
    proj[1][1] = 1.f;
    proj[2][3] = -1.f;
    proj[3][2] = nearZ;
    const glm::mat4 viewProj = proj * glm::lookAt(centre, centre + faceForward[face], faceUp[face]);

    GL_FramebufferTexture2DFunc(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, cube, 0);
    const GLfloat sky[4] = {0.16f, 0.17f, 0.2f, 1.f};
    const GLfloat far0 = 0.f;
    GL_ClearBufferfvFunc(GL_COLOR, 0, sky);
    GL_ClearBufferfvFunc(GL_DEPTH, 0, &far0);
    GL_UniformMatrix4fvFunc(0, 1, GL_FALSE, glm::value_ptr(viewProj));
    glDrawElements(GL_TRIANGLES, numIndices, GL_UNSIGNED_INT, nullptr);
}

} // namespace

void update()
{
    if(vr_weapon_reflections.value <= 0.f || !cl.worldmodel || !gl_clipcontrol_able || !lightmap_texture ||
        host_framecount - lastWanted > wantedFrames)
    {
        return;
    }
    if(builtWorld != cl.worldmodel || builtVbo != gl_bmodel_vbo || builtVboSize != gl_bmodel_vbo_size)
    {
        QVR_PROFILE("env cube build");
        buildWorld();
    }
    if(!numIndices || !ensureTargets())
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
    GL_SetState(GLS_BLEND_OPAQUE | GLS_CULL_NONE | GLS_ATTRIBS(6));
    GL_UseProgram(program);
    GL_Uniform1fFunc(1, std::clamp(vr_light_contrast.value, 0.5f, 3.f));
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

    renderFace(nextFace);
    nextFace = (nextFace + 1) % 6;
    facesDone = std::min(facesDone + 1, 1 << 20);

    GL_BindFramebufferFunc(GL_FRAMEBUFFER, 0);
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_CUBE_MAP, cube);
    GL_GenerateMipmapFunc(GL_TEXTURE_CUBE_MAP);
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_CUBE_MAP, 0);
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
    std::vector<float> face(size * size * 3);
    std::vector<std::uint8_t> img(18 + size * 6 * size * 3, 0);
    img[2] = 2; // uncompressed true colour
    img[12] = static_cast<std::uint8_t>((size * 6) & 255);
    img[13] = static_cast<std::uint8_t>((size * 6) >> 8);
    img[14] = static_cast<std::uint8_t>(size & 255);
    img[15] = static_cast<std::uint8_t>(size >> 8);
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
                std::uint8_t* d = &img[18 + (y * size * 6 + f * size + x) * 3];
                d[0] = static_cast<std::uint8_t>(std::clamp(c[2], 0.f, 1.f) * 255.f);
                d[1] = static_cast<std::uint8_t>(std::clamp(c[1], 0.f, 1.f) * 255.f);
                d[2] = static_cast<std::uint8_t>(std::clamp(c[0], 0.f, 1.f) * 255.f);
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
    if(fbo)
    {
        GL_DeleteFramebuffersFunc(1, &fbo);
        fbo = 0;
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

} // namespace qvr::envmap

namespace
{

using namespace qvr;

[[nodiscard]] bool startsWith(const char* s, const char* prefix)
{
    return !strncmp(s, prefix, strlen(prefix));
}

// How metal a model is (0 none .. 1): the share of its grey and blue-grey texels (the alias shader's mask) that
// reflect. Held weapons by their model; the rest none.
[[nodiscard]] float weaponMetal(const char* name, float& lod)
{
    struct Entry
    {
        const char* name;
        float metal;
        float lod; // the cube's mip level read: blurrier (rougher) the higher
    };
    static constexpr Entry table[] = {
        {"progs/v_shot.mdl", 1.f, 2.5f},    {"progs/v_shot2.mdl", 1.f, 2.5f}, {"progs/v_nail.mdl", 1.f, 2.5f},
        {"progs/v_nail2.mdl", 1.f, 2.5f},   {"progs/v_rock.mdl", 0.8f, 3.f},  {"progs/v_rock2.mdl", 0.8f, 3.f},
        {"progs/v_light.mdl", 0.9f, 2.5f},  {"progs/v_axe.mdl", 0.9f, 2.f},   {"progs/v_ksword.mdl", 1.f, 1.5f},
        {"progs/v_hksword.mdl", 1.f, 1.5f}, {"progs/v_hammer.mdl", 0.8f, 2.5f},
    };
    for(const Entry& e : table)
    {
        if(!strcmp(name, e.name))
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
    const char* name = e->model->name;
    const bool view = e == &cl.viewent || VR_IsViewEntity(e);
    const bool self = e == &cl_entities[cl.viewentity];
    const bool weapon = view && weapons::slotForModel(e->model) >= 0 && !startsWith(name, "progs/hand") &&
                        !startsWith(name, "progs/finger_");

    // Rim light: monsters and things full, held weapons less, your hands a little, your body not (seen from
    // inside, its edges are everywhere).
    const float rim = std::clamp(vr_rim_light.value, 0.f, 1.f);
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
        const bool hand = startsWith(name, "progs/hand") || startsWith(name, "progs/finger_") ||
                          startsWith(name, "progs/openhand");
        out[0] = hand ? rim * 0.35f : 0.f;
    }
    else
    {
        out[0] = rim;
    }

    // Reflections: held weapons, and weapons lying close to the cube's place (its view of the room is roughly
    // theirs within a few metres).
    const float strength = std::clamp(vr_weapon_reflections_strength.value, 0.f, 2.f);
    if(vr_weapon_reflections.value <= 0.f || strength <= 0.f)
    {
        return;
    }
    float lod = 3.f;
    float metal = 0.f;
    if(weapon)
    {
        metal = weaponMetal(name, lod);
    }
    else if(!view && (startsWith(name, "progs/g_") || startsWith(name, "progs/v_")))
    {
        metal = weaponMetal(name, lod) * 0.8f;
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
