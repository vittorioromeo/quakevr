// vr_gfx_gl.cpp -- vr_gfx.hpp on Ironwail's OpenGL renderer: one shader for all the module's
// triangles, framebuffer-backed targets, and Ironwail's 2D functions (Draw_*, glcanvas) pointed
// at a target. The only GL in the module besides the stereo view setup (vr_stereo.cpp), the
// OpenXR graphics binding and the engine's own entity hooks.

#include "vr_gfx.hpp"
#include "vr_engine.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace qvr::gfx
{
namespace
{

constexpr const char* vertexShader = R"(#version 430
layout(location = 0) uniform mat4 MVP;
layout(location = 0) in vec3 Pos;
layout(location = 1) in vec2 UV;
layout(location = 2) in vec4 Color;
out vec2 uv;
out vec4 color;
void main()
{
    uv = UV;
    color = Color;
    gl_Position = MVP * vec4(Pos, 1.0);
}
)";

// Mode: the Shade.
constexpr const char* fragmentShader = R"(#version 430
layout(location = 1) uniform int Mode;
layout(binding = 0) uniform sampler2D Tex;
in vec2 uv;
in vec4 color;
out vec4 result;
void main()
{
    if(Mode == 1)
    {
        float falloff = clamp(1.0 - dot(uv, uv), 0.0, 1.0);
        result = vec4(color.rgb, color.a * falloff);
    }
    else if(Mode == 2)
    {
        result = texture(Tex, uv) * color;
    }
    else if(Mode == 3)
    {
        vec4 c = texture(Tex, uv);
        if(c.a < 0.666)
            discard;
        result = vec4(c.rgb * color.rgb, 1.0);
    }
    else
    {
        result = color;
    }
}
)";

GLuint program = 0;
GLuint vbo = 0;
bool programFailed = false;

[[nodiscard]] GLuint compile(GLenum type, const char* source)
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
        Con_Warning("VR: shader: %s\n", log);
    }
    return shader;
}

bool ensureProgram()
{
    if(program || programFailed)
    {
        return program != 0;
    }

    const GLuint vs = compile(GL_VERTEX_SHADER, vertexShader);
    const GLuint fs = compile(GL_FRAGMENT_SHADER, fragmentShader);
    program = GL_CreateProgramFunc();
    GL_AttachShaderFunc(program, vs);
    GL_AttachShaderFunc(program, fs);
    GL_LinkProgramFunc(program);
    GL_DeleteShaderFunc(vs);
    GL_DeleteShaderFunc(fs);

    GLint ok = 0;
    GL_GetProgramivFunc(program, GL_LINK_STATUS, &ok);
    if(!ok)
    {
        Con_Warning("VR: shader failed to link\n");
        GL_DeleteProgramFunc(program);
        program = 0;
        programFailed = true;
        return false;
    }

    GL_GenBuffersFunc(1, &vbo);
    return true;
}

// begin2D() / end2D().
struct Saved2D
{
    bool active{false};
    GLint framebuffer{0};
    GLint viewport[4]{};
    glcanvas_t canvas{};
};
Saved2D saved2D;

std::unordered_map<std::string, qpic_t*> pics;

[[nodiscard]] qpic_t* picNamed(const char* name)
{
    const auto it = pics.find(name);
    if(it != pics.end())
    {
        return it->second;
    }
    qpic_t* p = Draw_PicFromWad(name);
    pics.emplace(name, p);
    return p;
}

GLuint copyFbo = 0;

// glBlendFuncSeparate (GL 1.4), which Ironwail does not load.
using BlendFuncSeparateFn = void(APIENTRY*)(GLenum, GLenum, GLenum, GLenum);
BlendFuncSeparateFn blendFuncSeparate = nullptr;

} // namespace

void draw(std::span<const Vertex> triangles, const glm::mat4& mvp, const State& state, Texture texture)
{
    if(triangles.empty() || !ensureProgram())
    {
        return;
    }

    unsigned flags = GLS_CULL_NONE | GLS_ATTRIBS(3);
    flags |= state.blend == Blend::Opaque ? GLS_BLEND_OPAQUE : GLS_BLEND_ALPHA;
    if(!state.depthTest)
    {
        flags |= GLS_NO_ZTEST;
    }
    if(!state.depthWrite)
    {
        flags |= GLS_NO_ZWRITE;
    }

    GL_UseProgram(program);
    GL_SetState(flags);
    if(state.blend == Blend::Premultiplied)
    {
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    }
    else if(state.blend == Blend::Modulate)
    {
        glBlendFunc(GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA);
    }
    GL_UniformMatrix4fvFunc(0, 1, GL_FALSE, &mvp[0][0]);
    GL_Uniform1iFunc(1, static_cast<GLint>(state.shade));
    if(texture)
    {
        GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, texture);
    }

    GL_BindBuffer(GL_ARRAY_BUFFER, vbo);
    GL_BufferDataFunc(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(triangles.size_bytes()), triangles.data(), GL_STREAM_DRAW);
    GL_VertexAttribPointerFunc(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, pos)));
    GL_VertexAttribPointerFunc(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, uv)));
    GL_VertexAttribPointerFunc(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, color)));
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(triangles.size()));
    GL_BindBuffer(GL_ARRAY_BUFFER, 0);

    if(state.blend == Blend::Premultiplied || state.blend == Blend::Modulate)
    {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // what GLS_BLEND_ALPHA expects
    }
}

glm::mat4 sceneViewProjection()
{
    glm::mat4 viewProj;
    memcpy(&viewProj[0][0], r_matviewproj, sizeof(r_matviewproj));
    return viewProj;
}

void sceneCamera(glm::vec3& origin, glm::vec3& right, glm::vec3& up)
{
    origin = {r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]};
    right = {vright[0], vright[1], vright[2]};
    up = {vup[0], vup[1], vup[2]};
}

Texture fontTexture()
{
    return char_texture ? char_texture->texnum : 0;
}

// Ironwail's console font atlas: 16 x 16 cells of 10 x 10 texels, each an 8 x 8 glyph with a
// one-texel border.
glm::vec4 fontGlyph(unsigned char c)
{
    constexpr float atlas = 160.f;
    const float u0 = ((c & 15) * 10 + 1) / atlas;
    const float v0 = ((c >> 4) * 10 + 1) / atlas;
    return {u0, v0, u0 + 8.f / atlas, v0 + 8.f / atlas};
}

void ensureTarget(Target& target, int width, int height)
{
    if(target.texture && target.width == width && target.height == height)
    {
        return;
    }

    GLuint texture = target.texture;
    if(texture)
    {
        glDeleteTextures(1, &texture);
    }
    GLuint fbo = target.framebuffer;
    if(!fbo)
    {
        GL_GenFramebuffersFunc(1, &fbo);
    }

    glGenTextures(1, &texture);
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, texture);
    GL_TexStorage2DFunc(GL_TEXTURE_2D, 1, GL_RGBA8, width, height);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    GLint previous = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previous);
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, fbo);
    GL_FramebufferTexture2DFunc(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, static_cast<GLuint>(previous));

    target = {texture, fbo, width, height};
}

void begin2D(const Target& target, int virtualWidth, int virtualHeight)
{
    Draw_Flush();

    saved2D.active = true;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &saved2D.framebuffer);
    glGetIntegerv(GL_VIEWPORT, saved2D.viewport);
    saved2D.canvas = glcanvas;

    GL_BindFramebufferFunc(GL_FRAMEBUFFER, target.framebuffer);
    glViewport(0, 0, target.width, target.height);

    // A canvas of its own: the virtual screen over the whole target, y down.
    glcanvas.type = CANVAS_INVALID;
    glcanvas.transform.scale[0] = 2.f / virtualWidth;
    glcanvas.transform.scale[1] = -2.f / virtualHeight;
    glcanvas.transform.offset[0] = -1.f;
    glcanvas.transform.offset[1] = 1.f;
    Draw_GetTransformBounds(&glcanvas.transform, &glcanvas.left, &glcanvas.top, &glcanvas.right, &glcanvas.bottom);
    glcanvas.blendmode = GLS_BLEND_ALPHA;
    GL_SetCanvasColor(1.f, 1.f, 1.f, 1.f);
}

void end2D()
{
    if(!saved2D.active)
    {
        return;
    }
    Draw_Flush();
    saved2D.active = false;

    glcanvas = saved2D.canvas;
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, static_cast<GLuint>(saved2D.framebuffer));
    glViewport(saved2D.viewport[0], saved2D.viewport[1], saved2D.viewport[2], saved2D.viewport[3]);
}

namespace draw2D
{

void color(const glm::vec4& rgba)
{
    GL_SetCanvasColor(rgba.r, rgba.g, rgba.b, rgba.a);
}

void fill(float x, float y, float w, float h, const glm::vec3& rgb)
{
    const float c[3] = {rgb.r, rgb.g, rgb.b};
    Draw_FillEx(x, y, w, h, c, 1.f);
}

void pic(float x, float y, const char* wadName, float scale)
{
    if(qpic_t* p = picNamed(wadName))
    {
        Draw_SubPic(x, y, p->width * scale, p->height * scale, p, 0.f, 0.f, 1.f, 1.f, rgb_white, 1.f);
    }
}

void text(float x, float y, float size, const char* str)
{
    Draw_StringEx(x, y, size, str);
}

} // namespace draw2D

void beginCanvas(Target& canvas, int width, int height)
{
    ensureTarget(canvas, width, height);
    GL_ResetState(); // re-applies the blend, now with applyCanvasBlend() (VR_CanvasBlend)
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, canvas.framebuffer);
    glViewport(0, 0, width, height);
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void endCanvas()
{
    GL_ResetState(); // back to Ironwail's usual blend
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, GL_NeedsPostprocess() ? framebufs.composite.fbo : 0);
    glViewport(glx, gly, glwidth, glheight);
}

bool applyCanvasBlend()
{
    if(!blendFuncSeparate)
    {
        blendFuncSeparate = reinterpret_cast<BlendFuncSeparateFn>(SDL_GL_GetProcAddress("glBlendFuncSeparate"));
        if(!blendFuncSeparate)
        {
            return false;
        }
    }
    blendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    return true;
}

void copy(const Target& target, Texture image)
{
    if(!copyFbo)
    {
        GL_GenFramebuffersFunc(1, &copyFbo);
    }

    GLint drawFbo = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFbo);

    GL_BindFramebufferFunc(GL_DRAW_FRAMEBUFFER, copyFbo);
    GL_FramebufferTexture2DFunc(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, image, 0);
    GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, target.framebuffer);
    GL_BlitFramebufferFunc(0, 0, target.width, target.height, 0, 0, target.width, target.height, GL_COLOR_BUFFER_BIT,
        GL_NEAREST);
    GL_FramebufferTexture2DFunc(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);

    GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, static_cast<GLuint>(drawFbo));
    GL_BindFramebufferFunc(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(drawFbo));
}

Texture createTexture(int width, int height, const void* rgba)
{
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    GL_TexStorage2DFunc(GL_TEXTURE_2D, 1, GL_RGBA8, width, height);
    if(rgba)
    {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    GL_ClearBindings();
    return tex;
}

void destroyTexture(Texture texture)
{
    GLuint tex = texture;
    glDeleteTextures(1, &tex);
    GL_ClearBindings();
}

} // namespace qvr::gfx
