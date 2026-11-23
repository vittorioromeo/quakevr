// vr_tonemap.cpp -- the eyes' tone curve, colour grade and last dither (see vr_tonemap.h), and vr_eyeshot.

#include "vr_tonemap.hpp"
#include "vr_bullettime.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_flatlook.hpp"
#include "vr_flatplay.hpp"
#include "vr_files.hpp"
#include "vr_gfx.hpp"
#include "vr_glutil.hpp"
#include "vr_input.hpp"
#include "vr_stereo.hpp"
#include "vr_visor.hpp"

#include "Zancle/Base/Memcpy.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "vr_api_render.h"

#include <stdio.h>

#if defined(SDL_FRAMEWORK) || defined(NO_SDL_CONFIG)
#include <SDL2/SDL.h>
#else
#include "SDL.h"
#endif

namespace qvr::tonemap
{
namespace
{

// The curve's knee, white point and the float scene's top: vr_tonemap.hpp (flat-screen tone mapping shares them).

// The grades: quakevr/gfx/vr/grade_<name>.png (Misc/quakevr/make_grades.py), loaded when first used.
struct Grade
{
    const char* name;
    GLuint tex;
    bool failed;
};
Grade grades[] = {{"film", 0, false}, {"cold", 0, false}, {"warm", 0, false}, {"moss", 0, false}, {"violet", 0, false}};
enum GradeIndex
{
    Film,
    Cold,
    Warm,
    Moss,
    Violet
};

// vr_grade 4: by episode (the map's name: e1m* ... e4m*), others (start, end, mission packs, mods) Quake VR's.
[[nodiscard]] int episodeGrade()
{
    const char* m = cl.mapname;
    if(m[0] == 'e' && m[1] >= '1' && m[1] <= '4' && m[2] == 'm')
    {
        constexpr int byEpisode[] = {Cold, Moss, Warm, Violet};
        return byEpisode[m[1] - '1'];
    }
    return Film;
}

[[nodiscard]] GLuint gradeTexture(Grade& g)
{
    if(g.tex || g.failed)
    {
        return g.tex;
    }
    const int mark = Hunk_LowMark();
    int w = 0, h = 0;
    enum srcformat fmt;
    const byte* data = Image_LoadImage(va("gfx/vr/grade_%s", g.name), &w, &h, &fmt);
    if(!data || h < 2 || w != h * h)
    {
        Hunk_FreeToLowMark(mark);
        Con_Warning("VR: colour grade gfx/vr/grade_%s.png missing or not an N*N x N strip\n", g.name);
        g.failed = true;
        return 0;
    }
    // The strip's texel (x = b * n + r, y = g) to the table's (r, g, b).
    const int n = h;
    za::Vector<byte> texels(static_cast<za::SizeT>(n) * n * n * 4);
    for(int b = 0; b < n; b++)
    {
        for(int gr = 0; gr < n; gr++)
        {
            for(int r = 0; r < n; r++)
            {
                const byte* src = data + (static_cast<za::SizeT>(gr) * w + b * n + r) * 4;
                byte* dst = texels.data() + ((static_cast<za::SizeT>(b) * n + gr) * n + r) * 4;
                ZA_MEMCPY(dst, src, 4);
            }
        }
    }
    Hunk_FreeToLowMark(mark);

    glGenTextures(1, &g.tex);
    GL_BindNative(GL_TEXTURE3, GL_TEXTURE_3D, g.tex);
    GL_ObjectLabelFunc(GL_TEXTURE, g.tex, -1, va("vr grade %s", g.name));
    GL_TexImage3DFunc(GL_TEXTURE_3D, 0, GL_RGBA8, n, n, n, 0, GL_RGBA, GL_UNSIGNED_BYTE, texels.data());
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    return g.tex;
}

// vr_eyeshot: a number for this session's shots.
int eyeshotCount = 0;

void writePfm(const za::String& path, const za::Vector<float>& rgb, int width, int height)
{
    FILE* f = fopen(path.cStr(), "wb");
    if(!f)
    {
        return;
    }
    fprintf(f, "PF\n%d %d\n-1.0\n", width, height); // bottom row first, as GL reads
    fwrite(rgb.data(), sizeof(float), rgb.size(), f);
    fclose(f);
}

} // namespace

bool active()
{
    return vr_tonemap.value > 0.f;
}

unsigned sceneFormat()
{
    if(!active())
    {
        return GL_RGB10_A2;
    }
    return vr_tonemap.value >= 2.f ? GL_R11F_G11F_B10F : GL_RGBA16F; // 2: the cheaper float format (coarser)
}

glm::vec4 bind(unsigned unit)
{
    glm::vec4 p{0.f, knee, white, 0.f};
    if(active())
    {
        p.x = za::clamp(vr_exposure.value, 0.1f, 4.f);
    }
    const int choice = static_cast<int>(vr_grade.value);
    const float strength = za::clamp(vr_grade_strength.value, 0.f, 1.f);
    if(choice >= 1 && choice <= 4 && strength > 0.f)
    {
        constexpr int byChoice[] = {Film, Cold, Warm};
        const GLuint tex = gradeTexture(grades[choice == 4 ? episodeGrade() : byChoice[choice - 1]]);
        if(tex)
        {
            GL_BindNative(GL_TEXTURE0 + unit, GL_TEXTURE_3D, tex);
            p.w = strength;
        }
    }
    return p;
}

namespace
{
// The in-tile pass (drawInTile): the post-process's last steps over an eye drawn straight into its image.
GLuint inTileProgram = 0;
bool inTileFailed = false;
int inTileAble = -1; // GL_EXT_shader_framebuffer_fetch (-1: not looked up yet)

// The post-process's steps after the tone curve that the eye needs now: bullet time's look, the comfort vignette, the
// grade, the dither (QVR_POSTPROCESS_GRADE_DITHER).
[[nodiscard]] bool inTileWanted()
{
    const int choice = static_cast<int>(vr_grade.value);
    const bool grade = choice >= 1 && choice <= 4 && vr_grade_strength.value > 0.f;
    return grade || vr_dither.value > 0.f || bullettime::look().strength > 0.f || input::comfortVignette() > 0.f;
}

[[nodiscard]] bool inTileSupported()
{
    if(inTileAble < 0)
    {
        inTileAble = SDL_GL_ExtensionSupported("GL_EXT_shader_framebuffer_fetch") ? 1 : 0;
    }
    return inTileAble > 0 && !inTileFailed;
}
} // namespace

bool postInScene()
{
    // The scene's shaders roll the world's, the liquids' and the models' colours off at exposure 1 (the particles and
    // sprites aren't scaled); the headset's gamma and contrast are the post-process's alone; the rest the in-tile pass's.
    return vr_exposure.value == 1.f && vr_gamma.value == 1.f && vr_contrast.value == 1.f &&
           (!inTileWanted() || inTileSupported());
}

void drawInTile(unsigned fbo, int width, int height)
{
    if(!inTileWanted() || !inTileSupported())
    {
        return;
    }
    if(!inTileProgram)
    {
        inTileProgram = gfx::glProgram(gfx::fullscreenTriangleVs, VR_InTileFragmentGLSL(), "vr in-tile grade");
        inTileFailed = !inTileProgram;
        gfx::ownDeferred(inTileProgram, &inTileFailed);
        if(inTileFailed)
        {
            Con_Warning("VR: the in-tile grade's program failed: the eyes go through the post-process\n");
            return;
        }
    }
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width, height);
    GL_SetState(GLS_BLEND_OPAQUE | GLS_NO_ZTEST | GLS_NO_ZWRITE | GLS_CULL_NONE | GLS_ATTRIBS(0));
    GL_UseProgram(inTileProgram);
    VR_PostProcessView(); // (PostView: the drawn size, for the vignettes)
    VR_PostProcessTone(); // (Tone's grade strength and its table on unit 3, Dither, SlowLook)
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void eyeshot(int eye, unsigned imageFbo, unsigned sceneFbo, int width, int height)
{
    if(vr_eyeshot.value <= 0.f)
    {
        return;
    }
    const za::String dir = za::String{com_gamedir} + "/eyeshots";
    files::createDirectories(dir.cStr());
    const char* side = eye == 0 ? "L" : "R";
    const za::String base = va("eyeshots/%s_%03d_%s", cl.mapname[0] ? cl.mapname : "none", eyeshotCount, side);

    // (read as RGBA, the alpha dropped: OpenGL ES reads no GL_RGB, which left the Quest's eye images black)
    const za::SizeT texels = static_cast<za::SizeT>(width) * height;
    za::Vector<byte> rgba(texels * 4);
    za::Vector<byte> pixels(texels * 3);
    GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, imageFbo);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    while(glGetError() != GL_NO_ERROR)
    {
    }
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    if(const GLenum err = glGetError(); err != GL_NO_ERROR) // (the image unread: say why, not a black file alone)
    {
        Con_Printf("eyeshot: the image's read failed (OpenGL error 0x%X, framebuffer %u status 0x%X)\n", err, imageFbo,
            GL_CheckFramebufferStatusFunc(GL_READ_FRAMEBUFFER));
    }
    for(za::SizeT i = 0; i < texels * 3; i++)
    {
        pixels[i] = rgba[i / 3 * 4 + i % 3];
    }
    Image_WritePNG((base + ".png").cStr(), pixels.data(), width, height, 24, false);

    if(vr_eyeshot.value >= 2.f && vr_eyeshot.value < 3.f)
    {
        za::Vector<float> scene4(texels * 4);
        za::Vector<float> scene(texels * 3);
        GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, sceneFbo);
        glReadPixels(0, 0, width, height, GL_RGBA, GL_FLOAT, scene4.data());
        for(za::SizeT i = 0; i < texels * 3; i++)
        {
            scene[i] = scene4[i / 3 * 4 + i % 3];
        }
        writePfm(za::String{com_gamedir} + "/" + base + ".pfm", scene, width, height);
    }
    GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, 0);
    Con_Printf("Wrote %s.png (the eye at %.2f %.2f %.2f, angles %.2f %.2f %.2f)\n", base.cStr(), r_refdef.vieworg[0],
        r_refdef.vieworg[1], r_refdef.vieworg[2], r_refdef.viewangles[0], r_refdef.viewangles[1], r_refdef.viewangles[2]);
    if(eye == 1)
    {
        ++eyeshotCount;
        Cvar_SetQuick(&vr_eyeshot, "0");
    }
}

namespace
{
// glProbe: one framebuffer's state and how much of it (w x h from the origin) is not black.
void probeFbo(const char* what, unsigned fbo, int w, int h)
{
    if(!fbo)
    {
        Con_Printf("  %s: none\n", what);
        return;
    }
    while(glGetError() != GL_NO_ERROR)
    {
    }
    GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, fbo);
    const GLenum status = GL_CheckFramebufferStatusFunc(GL_READ_FRAMEBUFFER);
    GLint samples = 0, format = 0;
    glGetIntegerv(GL_SAMPLES, &samples);
    glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT, &format);
    const za::SizeT texels = static_cast<za::SizeT>(za::max(1, w)) * za::max(1, h);
    za::Vector<float> px(texels * 4);
    za::SizeT lit = 0;
    double sum = 0.0, rgb[3] = {};
    GLenum err = GL_NO_ERROR;
    if(status == GL_FRAMEBUFFER_COMPLETE && samples <= 1)
    {
        glReadPixels(0, 0, w, h, GL_RGBA, GL_FLOAT, px.data());
        err = glGetError();
        if(err != GL_NO_ERROR) // (a fixed-point buffer: bytes)
        {
            za::Vector<byte> b(texels * 4);
            glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, b.data());
            err = glGetError();
            for(za::SizeT i = 0; i < texels * 4; i++)
            {
                px[i] = static_cast<float>(b[i]) / 255.f;
            }
        }
        GLint type = 0;
        glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_TYPE, &type);
        if(err != GL_NO_ERROR && type == GL_UNSIGNED_INT_2_10_10_10_REV) // (ES: an RGB10_A2 buffer reads only so)
        {
            za::Vector<GLuint> p(texels);
            glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, p.data());
            err = glGetError();
            for(za::SizeT i = 0; i < texels; i++)
            {
                for(int c = 0; c < 3; c++)
                {
                    px[4 * i + c] = static_cast<float>((p[i] >> (10 * c)) & 1023u) / 1023.f;
                }
            }
        }
        for(za::SizeT i = 0; i < texels; i++)
        {
            const float s = px[4 * i] + px[4 * i + 1] + px[4 * i + 2];
            lit += s > 1.f / 255.f ? 1 : 0;
            sum += s / 3.f;
            for(int c = 0; c < 3; c++)
            {
                rgb[c] += px[4 * i + c];
            }
        }
    }
    const double n = static_cast<double>(texels);
    Con_Printf("  %s: fbo %u status 0x%x samples %d (read format 0x%x) %dx%d: %.2f%% not black, mean %.4f (rgb %.3f %.3f %.3f)%s\n",
        what, fbo, status, samples, format, w, h, 100.0 * static_cast<double>(lit) / n, sum / n, rgb[0] / n, rgb[1] / n,
        rgb[2] / n, err != GL_NO_ERROR ? va(" (read error 0x%x)", err) : "");
}
} // namespace

namespace
{
GLuint probeQueries[2] = {}; // glProbeScene: the eye scene's primitives generated, any samples passed
GLuint probeDrawProgram = 0; // glProbeDraw: one colour at one depth
int probeStates = 0;         // probeState: the draws dumped this probe frame (reset by glProbeScene)
using ValidateProgramFn = void(APIENTRY*)(GLuint);
using GetIntegeriFn = void(APIENTRY*)(GLenum, GLuint, GLint*);
using GetInteger64iFn = void(APIENTRY*)(GLenum, GLuint, GLint64*);
using GetProgramInterfaceFn = void(APIENTRY*)(GLuint, GLenum, GLenum, GLint*);
using GetProgramResourceFn = void(APIENTRY*)(GLuint, GLenum, GLuint, GLsizei, const GLenum*, GLsizei, GLsizei*, GLint*);
using GetProgramResourceNameFn = void(APIENTRY*)(GLuint, GLenum, GLuint, GLsizei, GLsizei*, GLchar*);
ValidateProgramFn probeValidate = nullptr; // (not in the engine's function list)
GetIntegeriFn probeGetIndexed = nullptr;
GetInteger64iFn probeGetIndexed64 = nullptr;
GetProgramInterfaceFn probeInterface = nullptr;
GetProgramResourceFn probeResource = nullptr;
GetProgramResourceNameFn probeResourceName = nullptr;

constexpr const char* probeDrawVs = R"(#version 430
void main()
{
    ivec2 v = ivec2(gl_VertexID & 1, gl_VertexID >> 1);
    gl_Position = vec4(vec2(v) * 4.0 - 1.0, 0.0, 1.0);
}
)";
constexpr const char* probeDrawFs = R"(#version 430
layout(location = 0) uniform vec4 Colour;
layout(location = 1) uniform float Depth;
layout(location = 0) out vec4 Out;
void main()
{
    Out = Colour;
    gl_FragDepth = Depth;
}
)";
// vr_gl_probe 4 and 5: the world's vertices through the frame's ViewProj (4), or the first world instance's matrix
// too (5), in one colour: the world's vertex path cut down to its data.
constexpr const char* probeWorldVs = R"(#version 430
layout(std140, binding = 0) uniform FrameDataUBO { mat4 ViewProj; };
struct Instance { vec4 mat[3]; };
layout(std430, binding = 2) restrict readonly buffer InstanceBuffer { vec4 instance_words[]; };
layout(location = 0) uniform int Mode;
layout(location = 0) in vec3 in_pos;
void main()
{
    vec3 p = in_pos;
    if(Mode == 5)
    {
        mat4x3 world = transpose(mat3x4(instance_words[0], instance_words[1], instance_words[2]));
        p = mat3(world[0], world[1], world[2]) * p + world[3];
    }
    gl_Position = ViewProj * vec4(p, 1.0);
    if(Mode >= 6) // the world's clip distances: 1 and 2 written (0 too in mode 7), plane 1 on (off in mode 8)
    {
        gl_ClipDistance[1] = 0.0;
        gl_ClipDistance[2] = 1.0;
        if(Mode == 7)
        {
            gl_ClipDistance[0] = 1.0;
        }
    }
}
)";
constexpr const char* probeWorldFs = R"(#version 430
layout(location = 0) out vec4 Out;
void main()
{
    Out = vec4(0.25, 1.0, 0.25, 1.0);
}
)";
GLuint probeWorldProgram = 0;
} // namespace

void glProbeWorld(unsigned vbo, unsigned ibo, int stride, int indices)
{
    const int mode = static_cast<int>(vr_gl_probe.value);
    if(mode < 4 || mode > 8 || !vbo || !ibo || indices <= 0)
    {
        return;
    }
    if(!probeWorldProgram)
    {
        probeWorldProgram = gfx::glProgram(probeWorldVs, probeWorldFs, "vr gl probe world");
        if(!probeWorldProgram)
        {
            return;
        }
    }
    GL_UseProgram(probeWorldProgram);
    GL_SetState(GLS_BLEND_OPAQUE | GLS_NO_ZTEST | GLS_NO_ZWRITE | GLS_CULL_NONE | GLS_ATTRIBS(1));
    GL_Uniform1iFunc(0, mode);
    GL_BindBuffer(GL_ARRAY_BUFFER, vbo);
    GL_BindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    GL_VertexAttribPointerFunc(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    const bool plane = mode == 6 || mode == 7;
    if(plane)
    {
        glEnable(GL_CLIP_DISTANCE1);
    }
    glDrawElements(GL_TRIANGLES, indices, GL_UNSIGNED_INT, nullptr);
    if(plane)
    {
        glDisable(GL_CLIP_DISTANCE1);
    }
    static constexpr const char* what[] = {"ViewProj alone", "the first instance's matrix too",
        "clip distances 1 and 2 written, plane 1 on", "clip distances 0 to 2 written, plane 1 on",
        "clip distances 1 and 2 written, no plane on"};
    Con_Printf("  probe draw %d (the world's first %d indices in green: %s) drawn\n", mode, indices, what[mode - 4]);
}

void glProbeDraw(bool after)
{
    const int mode = static_cast<int>(vr_gl_probe.value);
    if(mode != (after ? 3 : 2))
    {
        return;
    }
    if(!probeDrawProgram)
    {
        probeDrawProgram = gfx::glProgram(probeDrawVs, probeDrawFs, "vr gl probe draw");
        if(!probeDrawProgram)
        {
            return;
        }
    }
    GLfloat clearDepth = 0.f;
    glGetFloatv(GL_DEPTH_CLEAR_VALUE, &clearDepth);
    GL_UseProgram(probeDrawProgram);
    GL_SetState(GLS_BLEND_OPAQUE | (after ? 0 : GLS_NO_ZTEST) | GLS_NO_ZWRITE | GLS_CULL_NONE | GLS_ATTRIBS(0));
    if(after)
    {
        GL_Uniform4fFunc(0, 1.f, 0.25f, 0.25f, 1.f);
    }
    else
    {
        GL_Uniform4fFunc(0, 0.25f, 0.5f, 1.f, 1.f);
    }
    GL_Uniform1fFunc(1, clearDepth);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    GLint fbo = 0, depthFunc = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &fbo);
    glGetIntegerv(GL_DEPTH_FUNC, &depthFunc);
    Con_Printf("  probe draw %d (%s, depth %g, func 0x%x) into fbo %d\n", mode,
        after ? "red where depth is the clear's" : "blue under the scene", clearDepth, depthFunc, fbo);
}

void glProbeScene(bool begin)
{
    if(vr_gl_probe.value <= 0.f)
    {
        return;
    }
    if(begin)
    {
        probeStates = 0;
        if(!probeQueries[0])
        {
            GL_GenQueriesFunc(2, probeQueries);
        }
        GL_BeginQueryFunc(GL_PRIMITIVES_GENERATED, probeQueries[0]);
        GL_BeginQueryFunc(GL_ANY_SAMPLES_PASSED, probeQueries[1]);
        return;
    }
    GL_EndQueryFunc(GL_PRIMITIVES_GENERATED);
    GL_EndQueryFunc(GL_ANY_SAMPLES_PASSED);
    GLuint prims = 0, samples = 0;
    GL_GetQueryObjectuivFunc(probeQueries[0], GL_QUERY_RESULT, &prims);
    GL_GetQueryObjectuivFunc(probeQueries[1], GL_QUERY_RESULT, &samples);
    Con_Printf("  the scene's draws: %u primitives generated, samples passed: %s\n", prims, samples ? "some" : "none");
}

void glProbe(int eye, int stage, unsigned sceneFbo, unsigned compositeFbo, unsigned imageFbo, int width, int height,
    int imageWidth, int imageHeight)
{
    if(vr_gl_probe.value <= 0.f)
    {
        return;
    }
    GLint previous = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previous);
    static constexpr const char* stages[] = {"scene drawn", "post-processed", "with the UI"};
    Con_Printf("vr_gl_probe: eye %c, %s\n", eye == 0 ? 'L' : 'R', stages[za::clamp(stage, 0, 2)]);
    if(stage == 0)
    {
        probeFbo("scene", sceneFbo, width, height);
    }
    probeFbo("composite", compositeFbo, width, height);
    probeFbo("image", imageFbo, imageWidth, imageHeight);
    if(stage == 0) // the state the scene was drawn with (per framebuffer: its draw buffer)
    {
        GLint drawFbo = 0, drawBuffer = 0, viewport[4] = {}, mask[4] = {};
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFbo);
        glGetIntegerv(GL_DRAW_BUFFER0, &drawBuffer);
        glGetIntegerv(GL_VIEWPORT, viewport);
        glGetIntegerv(GL_COLOR_WRITEMASK, mask);
        Con_Printf("  state: draw fbo %d draw buffer 0x%x viewport %d %d %d %d colour mask %d%d%d%d scissor %d discard %d "
                   "blend %d depth test %d cull %d\n",
            drawFbo, drawBuffer, viewport[0], viewport[1], viewport[2], viewport[3], mask[0], mask[1], mask[2], mask[3],
            glIsEnabled(GL_SCISSOR_TEST) ? 1 : 0, glIsEnabled(GL_RASTERIZER_DISCARD) ? 1 : 0, glIsEnabled(GL_BLEND) ? 1 : 0,
            glIsEnabled(GL_DEPTH_TEST) ? 1 : 0, glIsEnabled(GL_CULL_FACE) ? 1 : 0);
        GLfloat clearDepth = -1.f, clearColour[4] = {};
        GLint depthFunc = 0, depthMask = 0, stencilFunc = 0;
        glGetFloatv(GL_DEPTH_CLEAR_VALUE, &clearDepth);
        glGetFloatv(GL_COLOR_CLEAR_VALUE, clearColour);
        glGetIntegerv(GL_DEPTH_FUNC, &depthFunc);
        glGetIntegerv(GL_DEPTH_WRITEMASK, &depthMask);
        glGetIntegerv(GL_STENCIL_FUNC, &stencilFunc);
        Con_Printf("  depth: clear %g func 0x%x write %d; stencil test %d func 0x%x; clear colour %g %g %g %g\n", clearDepth,
            depthFunc, depthMask, glIsEnabled(GL_STENCIL_TEST) ? 1 : 0, stencilFunc, clearColour[0], clearColour[1],
            clearColour[2], clearColour[3]);
        for(const unsigned f : {sceneFbo, compositeFbo})
        {
            GL_BindFramebufferFunc(GL_DRAW_FRAMEBUFFER, f);
            glGetIntegerv(GL_DRAW_BUFFER0, &drawBuffer);
            Con_Printf("  fbo %u: draw buffer 0x%x\n", f, drawBuffer);
        }
        GL_BindFramebufferFunc(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(drawFbo));
    }
    if(stage == 0 && eye == 0) // the self-test: a cleared RGBA8 framebuffer read back
    {
        GLuint tex = 0, fbo = 0;
        GLint drawBefore = 0;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawBefore);
        glGenTextures(1, &tex);
        GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, tex);
        GL_TexStorage2DFunc(GL_TEXTURE_2D, 1, GL_RGBA8, 4, 4);
        GL_GenFramebuffersFunc(1, &fbo);
        GL_BindFramebufferFunc(GL_DRAW_FRAMEBUFFER, fbo);
        GL_FramebufferTexture2DFunc(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
        const GLfloat colour[4] = {1.f, 0.5f, 0.25f, 1.f};
        GL_ClearBufferfvFunc(GL_COLOR, 0, colour);
        probeFbo("self-test (cleared to 1 0.5 0.25: mean 0.58)", fbo, 4, 4);
        GL_BindFramebufferFunc(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(drawBefore));
        GL_DeleteFramebuffersFunc(1, &fbo);
        GL_DeleteNativeTexture(tex);
    }
    GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, static_cast<GLuint>(previous));
    if(eye == 1 && stage == 2)
    {
        Cvar_SetQuick(&vr_gl_probe, "0");
    }
}

void probeUpload(const char* what, unsigned buffer, size_t offset, const void* data, size_t size)
{
    if(vr_gl_probe.value <= 0.f || !buffer)
    {
        return;
    }
    za::Vector<byte> back(static_cast<za::SizeT>(size));
    GL_BindBufferFunc(GL_COPY_READ_BUFFER, buffer);
    const bool read = gfx::readBuffer(GL_COPY_READ_BUFFER, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size), back.data());
    GL_BindBufferFunc(GL_COPY_READ_BUFFER, 0);
    size_t differ = 0, zero = 0;
    for(size_t i = 0; i < size; i++)
    {
        differ += back[i] != static_cast<const byte*>(data)[i] ? 1 : 0;
        zero += back[i] == 0 ? 1 : 0;
    }
    float first[4] = {};
    ZA_MEMCPY(first, back.data(), za::min(sizeof first, size));
    Con_Printf("  %s upload: buffer %u at %d, %d bytes: %s; %d bytes differ, %d zero; first %g %g %g %g\n", what, buffer,
        static_cast<int>(offset), static_cast<int>(size), read ? "read back" : "NOT read", static_cast<int>(differ),
        static_cast<int>(zero), first[0], first[1], first[2], first[3]);
}

void probeState(const char* where)
{
    const int fix = static_cast<int>(vr_gl_probe_fix.value);
    if(fix & 1)
    {
        for(GLenum c = GL_CLIP_DISTANCE0; c <= GL_CLIP_DISTANCE3; c++)
        {
            glDisable(c);
        }
    }
    if(fix & 2)
    {
        glDisable(GL_CULL_FACE);
    }
    if(vr_gl_probe.value <= 0.f || probeStates >= 3) // (the first few draws of the probe's frame)
    {
        return;
    }
    ++probeStates;
    GLint program = 0, vao = 0, fbo = 0, mask = 0, depthFunc = 0, depthMask = 0, frontFace = 0, viewport[4] = {};
    GLfloat range[2] = {};
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &fbo);
    if(!probeValidate)
    {
        probeValidate = reinterpret_cast<ValidateProgramFn>(SDL_GL_GetProcAddress("glValidateProgram"));
        probeGetIndexed = reinterpret_cast<GetIntegeriFn>(SDL_GL_GetProcAddress("glGetIntegeri_v"));
        probeGetIndexed64 = reinterpret_cast<GetInteger64iFn>(SDL_GL_GetProcAddress("glGetInteger64i_v"));
        probeInterface = reinterpret_cast<GetProgramInterfaceFn>(SDL_GL_GetProcAddress("glGetProgramInterfaceiv"));
        probeResource = reinterpret_cast<GetProgramResourceFn>(SDL_GL_GetProcAddress("glGetProgramResourceiv"));
        probeResourceName = reinterpret_cast<GetProgramResourceNameFn>(SDL_GL_GetProcAddress("glGetProgramResourceName"));
    }
    if(probeGetIndexed)
    {
        probeGetIndexed(GL_SAMPLE_MASK_VALUE, 0, &mask);
    }
    glGetIntegerv(GL_DEPTH_FUNC, &depthFunc);
    glGetIntegerv(GL_DEPTH_WRITEMASK, &depthMask);
    glGetIntegerv(GL_FRONT_FACE, &frontFace);
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetFloatv(GL_DEPTH_RANGE, range);
    const auto on = [](GLenum cap) { return glIsEnabled(cap) ? 1 : 0; };
    Con_Printf("  state at %s: program %d vao %d fbo %d viewport %d %d %d %d depth range %g %g func 0x%x write %d front 0x%x; "
               "clip distances %d%d%d%d depth clamp %d alpha to coverage %d coverage %d sample mask %d (0x%x) cull %d "
               "offset %d stencil %d scissor %d discard %d\n",
        where, program, vao, fbo, viewport[0], viewport[1], viewport[2], viewport[3], range[0], range[1], depthFunc, depthMask,
        frontFace, on(GL_CLIP_DISTANCE0), on(GL_CLIP_DISTANCE1), on(GL_CLIP_DISTANCE2), on(GL_CLIP_DISTANCE3),
        on(GL_DEPTH_CLAMP), on(GL_SAMPLE_ALPHA_TO_COVERAGE), on(GL_SAMPLE_COVERAGE), on(GL_SAMPLE_MASK), mask,
        on(GL_CULL_FACE), on(GL_POLYGON_OFFSET_FILL), on(GL_STENCIL_TEST), on(GL_SCISSOR_TEST), on(GL_RASTERIZER_DISCARD));
    if(program && probeValidate)
    {
        probeValidate(static_cast<GLuint>(program));
        GLint valid = 0;
        GL_GetProgramivFunc(static_cast<GLuint>(program), GL_VALIDATE_STATUS, &valid);
        char log[512] = {};
        GL_GetProgramInfoLogFunc(static_cast<GLuint>(program), sizeof log, nullptr, log);
        Con_Printf("  program %d validates: %s %s\n", program, valid ? "yes" : "NO", log);
    }
    if(program && probeInterface && probeResource && probeResourceName && probeGetIndexed64)
    {
        // Each buffer block the program reads: its size as the driver lays it out, against the range bound to it.
        for(const GLenum iface : {GLenum{GL_UNIFORM_BLOCK}, GLenum{GL_SHADER_STORAGE_BLOCK}})
        {
            GLint count = 0;
            probeInterface(static_cast<GLuint>(program), iface, GL_ACTIVE_RESOURCES, &count);
            for(GLint i = 0; i < count; i++)
            {
                const GLenum props[2] = {GL_BUFFER_BINDING, GL_BUFFER_DATA_SIZE};
                GLint values[2] = {};
                probeResource(static_cast<GLuint>(program), iface, static_cast<GLuint>(i), 2, props, 2, nullptr, values);
                char name[64] = {};
                probeResourceName(static_cast<GLuint>(program), iface, static_cast<GLuint>(i), sizeof name, nullptr, name);
                const bool ubo = iface == GL_UNIFORM_BLOCK;
                GLint64 bound = 0, start = 0;
                GLint buffer = 0;
                probeGetIndexed64(ubo ? GL_UNIFORM_BUFFER_SIZE : GL_SHADER_STORAGE_BUFFER_SIZE, static_cast<GLuint>(values[0]), &bound);
                probeGetIndexed64(ubo ? GL_UNIFORM_BUFFER_START : GL_SHADER_STORAGE_BUFFER_START, static_cast<GLuint>(values[0]), &start);
                if(probeGetIndexed)
                {
                    probeGetIndexed(ubo ? GL_UNIFORM_BUFFER_BINDING : GL_SHADER_STORAGE_BUFFER_BINDING, static_cast<GLuint>(values[0]), &buffer);
                }
                Con_Printf("    %s %s: binding %d, %d bytes laid out; bound buffer %d at %d, %d bytes%s\n", ubo ? "uniform block" : "storage block",
                    name, values[0], values[1], buffer, static_cast<int>(start), static_cast<int>(bound),
                    bound > 0 && bound < values[1] ? " (SHORT)" : "");
            }
        }
    }
    while(glGetError() != GL_NO_ERROR)
    {
    }
}

void probeBuffer(const char* what, unsigned buffer, size_t offset, size_t size)
{
    if(vr_gl_probe.value <= 0.f || !buffer || !size)
    {
        return;
    }
    za::Vector<GLuint> back(static_cast<za::SizeT>((size + 3) / 4));
    GL_MemoryBarrierFunc(GL_BUFFER_UPDATE_BARRIER_BIT); // (a compute shader's writes, made visible to the read)
    GL_BindBufferFunc(GL_COPY_READ_BUFFER, buffer);
    const bool read = gfx::readBuffer(GL_COPY_READ_BUFFER, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size), back.data());
    GL_BindBufferFunc(GL_COPY_READ_BUFFER, 0);
    size_t nonzero = 0;
    GLuint largest = 0;
    for(const GLuint u : back)
    {
        nonzero += u != 0 ? 1 : 0;
        largest = za::max(largest, u);
    }
    GLuint first[8] = {};
    ZA_MEMCPY(first, back.data(), za::min(sizeof first, back.size() * sizeof(GLuint)));
    Con_Printf("  %s: buffer %u at %d, %d words: %s; %d not zero, largest %u; first %u %u %u %u %u %u %u %u\n", what, buffer,
        static_cast<int>(offset), static_cast<int>(back.size()), read ? "read back" : "NOT read", static_cast<int>(nonzero),
        largest, first[0], first[1], first[2], first[3], first[4], first[5], first[6], first[7]);
}

int nextEyeshot()
{
    return eyeshotCount;
}

} // namespace qvr::tonemap

using namespace qvr;

extern "C" void VR_GLProbeUpload(const char* what, unsigned buffer, size_t offset, const void* data, size_t size)
{
    tonemap::probeUpload(what, buffer, offset, data, size);
}

extern "C" void VR_GLProbeBuffer(const char* what, unsigned buffer, size_t offset, size_t size)
{
    tonemap::probeBuffer(what, buffer, offset, size);
}

extern "C" void VR_GLProbeState(const char* where)
{
    tonemap::probeState(where);
}

extern "C" void VR_GLProbeSceneEnd(unsigned vbo, unsigned ibo, int stride, int indices)
{
    if(stereo::isRenderingEye())
    {
        tonemap::glProbeDraw(true);
        tonemap::glProbeWorld(vbo, ibo, stride, indices);
    }
}

extern "C" float VR_SceneTone(void)
{
    if(stereo::isRenderingEye())
    {
        // Drawn straight into the eye's 8-bit image (stereo::directEye): no post-process, so the world, the liquids and
        // the models roll their colours off with the curve themselves (negative: the exposure; QvrSceneTone, vr_glsl.h).
        if(stereo::directEye() && tonemap::active())
        {
            return -za::clamp(vr_exposure.value, 0.1f, 4.f);
        }
        return tonemap::active() ? tonemap::sceneMax : 1.f;
    }
    return flatlook::floatScene() ? tonemap::sceneMax : 1.f; // flat-screen tone mapping (vr_flat_tonemap)
}

extern "C" float VR_SceneDither(float dither)
{
    // (the eyes' post-process dithers into the 8-bit image; drawn straight into it, the in-tile pass: tonemap::drawInTile)
    return stereo::isRenderingEye() && vr_dither.value > 0.f ? 0.f : dither;
}

// Flatscreen play's window needs the post-process pass while bullet time's look is on (GL_NeedsPostprocess).
extern "C" int VR_FlatBulletTimeLook(void)
{
    return flatplay::active() && bullettime::look().strength > 0.f ? 1 : 0;
}

// Uniforms 7 (Tone) and 8 (Dither) of the non-palettized post-process; zero for the window.
extern "C" void VR_PostProcessTone(void)
{
    if(!stereo::isRenderingEye())
    {
        GL_Uniform4fFunc(7, 0.f, 0.f, 0.f, 0.f);
        // Flatscreen play's graded view (vr_flatlook.cpp) dithered into the 8-bit window as the eyes are (vr_dither; fixed)
        GL_Uniform4fFunc(8, flatlook::active() && vr_dither.value > 0.f ? 1.f / 255.f : 0.f, 0.f, 0.f, 0.f);
        if(flatplay::active()) // flatscreen play: the window gets bullet time's look, the same tunables and easing as the eyes
        {
            const bullettime::Look slow = bullettime::look();
            GL_Uniform4fFunc(9, slow.strength, slow.desaturate, slow.vignette, 0.f);
            GL_Uniform3fFunc(10, slow.tint.x, slow.tint.y, slow.tint.z);
            // The visor's panels keep their colours (vr_flat_visor_no_bt_tint): its layer's alpha leaves them alone.
            glm::vec4 rect{0.f};
            const unsigned mask = slow.strength > 0.f ? visor::tintMask(rect) : 0u;
            if(mask)
            {
                GL_BindNative(GL_TEXTURE7, GL_TEXTURE_2D, mask);
            }
            GL_Uniform4fFunc(11, rect.x, rect.y, mask ? rect.z : 0.f, rect.w);
        }
        else
        {
            GL_Uniform4fFunc(9, 0.f, 0.f, 0.f, 0.f);
            GL_Uniform4fFunc(11, 0.f, 0.f, 0.f, 0.f);
        }
        return;
    }
    const glm::vec4 tone = tonemap::bind(3);
    GL_Uniform4fFunc(7, tone.x, tone.y, tone.z, tone.w);
    const int mode = static_cast<int>(vr_dither.value);
    const float frame = mode == 2 ? static_cast<float>(host_framecount % 64) : 0.f;
    const float otherEye = mode != 3 && stereo::eye() == 1 ? 1.f : 0.f;
    GL_Uniform4fFunc(8, mode > 0 ? 1.f / 255.f : 0.f, otherEye, frame, 0.f);
    const bullettime::Look slow = bullettime::look(); // bullet time's look (the eyes only: not the spectator camera)
    GL_Uniform4fFunc(9, slow.strength, slow.desaturate, slow.vignette, input::comfortVignette()); // (and the comfort vignette)
    GL_Uniform3fFunc(10, slow.tint.x, slow.tint.y, slow.tint.z);
    GL_Uniform4fFunc(11, 0.f, 0.f, 0.f, 0.f); // (the flat-screen visor's mask: never an eye's)
}
