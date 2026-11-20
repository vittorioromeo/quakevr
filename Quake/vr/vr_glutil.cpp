// vr_glutil.cpp -- the GL object helpers every effect's shaders and targets share (vr_glutil.hpp).

#include "vr_glutil.hpp"
#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/String/ToString.hpp"
#include "vr_api.h"

#if defined(SDL_FRAMEWORK) || defined(NO_SDL_CONFIG)
#include <SDL2/SDL.h>
#else
#include "SDL.h"
#endif

#include <cstring>

namespace qvr::gfx
{

namespace
{

// A program linked while the checks are deferred (beginDeferredChecks): its statuses read at endDeferredChecks.
struct DeferredProgram
{
    GLuint program{0};
    GLuint vs{0}; // (deleted at the link: kept by the program while attached, its status still there to read)
    GLuint fs{0};
    char name[64]{};
};

// An owner's program variable and failure flag (ownDeferred): set at endDeferredChecks if its program failed.
struct DeferredOwner
{
    GLuint* program{nullptr};
    bool* failed{nullptr};
};

bool deferring = false;
za::Vector<DeferredProgram> deferredPrograms;
za::Vector<DeferredOwner> deferredOwners;

// Shader `shader`'s compile status, warning with its info log on a failure.
[[nodiscard]] bool compileStatus(const GLuint shader, const char* name)
{
    GLint ok = 0;
    GL_GetShaderivFunc(shader, GL_COMPILE_STATUS, &ok);
    if(!ok)
    {
        char log[2048];
        log[0] = 0;
        GL_GetShaderInfoLogFunc(shader, sizeof(log), nullptr, log);
        Con_Warning("VR: %s shader failed to compile:\n%s\n", name, log);
        (void)VR_GLESShaderError(nullptr); // (-glescompat: counted)
    }
    return ok != 0;
}

// Program `p`'s link status, warning with its info log on a failure.
[[nodiscard]] bool linkStatus(const GLuint p, const char* name)
{
    GLint ok = 0;
    GL_GetProgramivFunc(p, GL_LINK_STATUS, &ok);
    if(!ok)
    {
        char log[2048];
        log[0] = 0;
        GL_GetProgramInfoLogFunc(p, sizeof(log), nullptr, log);
        Con_Warning("VR: %s shader failed to link:\n%s\n", name, log);
        (void)VR_GLESShaderError(nullptr); // (-glescompat: counted)
    }
    return ok != 0;
}

constexpr const char* nothingVs = "#version 430\nvoid main() { gl_Position = vec4(0.0, 0.0, 2.0, 1.0); }\n"; // clipped
constexpr const char* nothingFs = "#version 430\nvoid main() { discard; }\n";
constexpr const char* nothingCs = "#version 430\nlayout(local_size_x = 1) in;\nvoid main() {}\n";

// A deferred program that failed and that no owner said it holds (ownDeferred): relinked from shaders that draw (or
// dispatch) nothing, the stages it had, so its holder never uses a deleted name (a safety net: every program
// gfx::precompilePrograms makes has its owner).
void relinkToNothing(const DeferredProgram& d)
{
    GLint type = 0;
    if(d.fs)
    {
        GL_GetShaderivFunc(d.fs, GL_SHADER_TYPE, &type);
    }
    const bool compute = type == GL_COMPUTE_SHADER;
    for(const GLuint s : {d.vs, d.fs})
    {
        if(s)
        {
            GL_DetachShaderFunc(d.program, s); // (flagged deleted at the link: gone now)
        }
    }
    const auto attach = [&](GLenum type, const char* source) {
        const GLuint s = GL_CreateShaderFunc(type);
        VR_GLShaderSource(s, 1, &source);
        GL_CompileShaderFunc(s);
        GL_AttachShaderFunc(d.program, s);
        GL_DeleteShaderFunc(s);
    };
    if(compute)
    {
        attach(GL_COMPUTE_SHADER, nothingCs);
    }
    else
    {
        attach(GL_VERTEX_SHADER, nothingVs);
        if(d.fs)
        {
            attach(GL_FRAGMENT_SHADER, nothingFs);
        }
    }
    GL_LinkProgramFunc(d.program);
    Con_Warning("VR: %s draws nothing\n", d.name);
}

} // namespace

namespace
{
using GetBufferSubDataFn = void(APIENTRY*)(GLenum, GLintptr, GLsizeiptr, void*);
using ClearTexImageFn = void(APIENTRY*)(GLuint, GLint, GLenum, GLenum, const void*);
struct DesktopOnly // (looked up on first use; desktop GL's, not in ES)
{
    bool looked{false};
    GetBufferSubDataFn getBufferSubData{nullptr};
    ClearTexImageFn clearTexImage{nullptr};
};
DesktopOnly desktopOnly;

void lookUpDesktopOnly()
{
    if(!desktopOnly.looked)
    {
        desktopOnly.looked = true;
        desktopOnly.getBufferSubData = reinterpret_cast<GetBufferSubDataFn>(SDL_GL_GetProcAddress("glGetBufferSubData"));
        desktopOnly.clearTexImage = reinterpret_cast<ClearTexImageFn>(SDL_GL_GetProcAddress("glClearTexImage"));
    }
}

// readBuffer's ES form for a buffer mapped already (the persistent, coherent upload rings: GL_Upload's, kept under ES):
// mapped again it can't be (GL_INVALID_OPERATION), so its range is copied into a buffer of its own and that one read.
using GetBufferParameterivFn = void(APIENTRY*)(GLenum, GLenum, GLint*);
using CopyBufferSubDataFn = void(APIENTRY*)(GLenum, GLenum, GLintptr, GLintptr, GLsizeiptr);
struct EsBufferCalls // (ES 3.0's core, looked up on first use: not in the engine's table)
{
    bool looked{false};
    GetBufferParameterivFn getBufferParameteriv{nullptr};
    CopyBufferSubDataFn copyBufferSubData{nullptr};
};
EsBufferCalls esBuffer;

bool readMappedCopy(GLenum target, GLintptr offset, GLsizeiptr size, void* out)
{
    GLint previous = 0;
    glGetIntegerv(GL_COPY_WRITE_BUFFER_BINDING, &previous);
    GLuint copy = 0;
    GL_GenBuffersFunc(1, &copy);
    GL_BindBufferFunc(GL_COPY_WRITE_BUFFER, copy);
    GL_BufferDataFunc(GL_COPY_WRITE_BUFFER, size, nullptr, GL_STREAM_READ);
    esBuffer.copyBufferSubData(target, GL_COPY_WRITE_BUFFER, offset, 0, size);
    const void* p = GL_MapBufferRangeFunc(GL_COPY_WRITE_BUFFER, 0, size, GL_MAP_READ_BIT);
    bool read = false;
    if(p)
    {
        memcpy(out, p, static_cast<size_t>(size));
        read = GL_UnmapBufferFunc(GL_COPY_WRITE_BUFFER) == GL_TRUE;
    }
    GL_BindBufferFunc(GL_COPY_WRITE_BUFFER, static_cast<GLuint>(previous));
    GL_DeleteBuffersFunc(1, &copy);
    return read;
}
} // namespace

void drawNoColour()
{
    const GLenum none = GL_NONE;
    GL_DrawBuffersFunc(1, &none);
}

bool readBuffer(GLenum target, GLintptr offset, GLsizeiptr size, void* out)
{
    if(VR_GLESCompat())
    {
        if(!esBuffer.looked)
        {
            esBuffer.looked = true;
            esBuffer.getBufferParameteriv =
                reinterpret_cast<GetBufferParameterivFn>(SDL_GL_GetProcAddress("glGetBufferParameteriv"));
            esBuffer.copyBufferSubData = reinterpret_cast<CopyBufferSubDataFn>(SDL_GL_GetProcAddress("glCopyBufferSubData"));
        }
        GLint mapped = GL_FALSE;
        if(esBuffer.getBufferParameteriv)
        {
            esBuffer.getBufferParameteriv(target, GL_BUFFER_MAPPED, &mapped);
        }
        if(mapped && esBuffer.copyBufferSubData)
        {
            return readMappedCopy(target, offset, size, out);
        }
        const void* p = GL_MapBufferRangeFunc(target, offset, size, GL_MAP_READ_BIT);
        if(!p)
        {
            return false;
        }
        memcpy(out, p, static_cast<size_t>(size));
        return GL_UnmapBufferFunc(target) == GL_TRUE;
    }
    lookUpDesktopOnly();
    if(!desktopOnly.getBufferSubData)
    {
        return false;
    }
    desktopOnly.getBufferSubData(target, offset, size, out);
    return true;
}

void clearTexture(GLuint tex, const float rgba[4])
{
    if(VR_GLESCompat())
    {
        GLint previous = 0;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previous);
        GLuint fbo = 0;
        GL_GenFramebuffersFunc(1, &fbo);
        GL_BindFramebufferFunc(GL_DRAW_FRAMEBUFFER, fbo);
        GL_FramebufferTexture2DFunc(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
        const GLenum colour = GL_COLOR_ATTACHMENT0;
        GL_DrawBuffersFunc(1, &colour);
        // (glClearTexImage, the call this stands for, ignores the scissor and the colour mask; a framebuffer clear
        // follows both: the whole texture, every channel, then they are put back, as readDepthBits does)
        GLboolean colourMask[4] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};
        glGetBooleanv(GL_COLOR_WRITEMASK, colourMask);
        const GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDisable(GL_SCISSOR_TEST);
        GL_ClearBufferfvFunc(GL_COLOR, 0, rgba);
        glColorMask(colourMask[0], colourMask[1], colourMask[2], colourMask[3]);
        if(scissor)
        {
            glEnable(GL_SCISSOR_TEST);
        }
        GL_BindFramebufferFunc(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(previous));
        GL_DeleteFramebuffersFunc(1, &fbo);
        return;
    }
    lookUpDesktopOnly();
    if(desktopOnly.clearTexImage)
    {
        desktopOnly.clearTexImage(tex, 0, GL_RGBA, GL_FLOAT, rgba);
    }
}

namespace
{
#ifndef GL_TEXTURE_RED_TYPE
#define GL_TEXTURE_RED_TYPE 0x8C10
#endif
#ifndef GL_TEXTURE_DEPTH_TYPE
#define GL_TEXTURE_DEPTH_TYPE 0x8C16
#endif

// readTexImage's ES form's depth copy: a triangle over the target, each texel's depth bits as an unsigned integer.
constexpr const char* depthBitsVs = "#version 430\n"
                                    "void main()\n"
                                    "{\n"
                                    "    gl_Position = vec4(float((gl_VertexID & 1) * 4 - 1), float((gl_VertexID & 2) * 2 - 1), 0.0, 1.0);\n"
                                    "}\n";
constexpr const char* depthBitsFs = "#version 430\n"
                                    "layout(location = 0) uniform int Level;\n"
                                    "layout(binding = 0) uniform sampler2D Depth;\n"
                                    "layout(location = 0) out uvec4 Bits;\n"
                                    "void main()\n"
                                    "{\n"
                                    "    Bits = uvec4(floatBitsToUint(texelFetch(Depth, ivec2(gl_FragCoord.xy), Level).r), 0u, 0u, 0u);\n"
                                    "}\n";
constexpr int depthStripRows = 256; // rows read back at a time (RGBA_INTEGER: 16 bytes a texel)

struct EsReadback // (made on first use; the context's for the session)
{
    GLuint depthProgram{0};
    GLuint sampler{0}; // nearest, no comparison: the depth texture's own compare mode set aside
    bool failed{false};
};
EsReadback esReadback;

[[nodiscard]] GLenum bindingOf(GLenum target)
{
    switch(target)
    {
        case GL_TEXTURE_2D: return GL_TEXTURE_BINDING_2D;
        case GL_TEXTURE_3D: return GL_TEXTURE_BINDING_3D;
        case GL_TEXTURE_2D_ARRAY: return GL_TEXTURE_BINDING_2D_ARRAY;
        default: return GL_TEXTURE_BINDING_CUBE_MAP; // (a face)
    }
}

[[nodiscard]] bool makeDepthBitsProgram()
{
    if(esReadback.depthProgram || esReadback.failed)
    {
        return esReadback.depthProgram != 0;
    }
    const auto stage = [](GLenum type, const char* source, const char* name) -> GLuint {
        const GLuint s = GL_CreateShaderFunc(type);
        VR_GLShaderSource(s, 1, &source);
        GL_CompileShaderFunc(s);
        if(!compileStatus(s, name))
        {
            GL_DeleteShaderFunc(s);
            return 0;
        }
        return s;
    };
    const GLuint vs = stage(GL_VERTEX_SHADER, depthBitsVs, "readback depth bits vs");
    const GLuint fs = stage(GL_FRAGMENT_SHADER, depthBitsFs, "readback depth bits fs");
    esReadback.depthProgram = vs && fs ? linkProgram(vs, fs, "readback depth bits") : 0;
    if(!esReadback.depthProgram)
    {
        GL_DeleteShaderFunc(vs);
        GL_DeleteShaderFunc(fs);
        esReadback.failed = true;
        return false;
    }
    GL_GenSamplersFunc(1, &esReadback.sampler);
    GL_SamplerParameteriFunc(esReadback.sampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    GL_SamplerParameteriFunc(esReadback.sampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    GL_SamplerParameteriFunc(esReadback.sampler, GL_TEXTURE_COMPARE_MODE, GL_NONE);
    return true;
}

// The depth bits of level `level` of 2D texture `tex` (w by h), drawn into an R32UI texture, read row strips at a time
// (as RGBA_INTEGER, the form ES guarantees) into `out` (w * h floats, rows `stride` bytes apart).
[[nodiscard]] bool readDepthBits(GLuint tex, GLint level, int w, int h, unsigned char* out, size_t stride)
{
    if(!makeDepthBitsProgram())
    {
        return false;
    }
    GLint viewport[4] = {};
    glGetIntegerv(GL_VIEWPORT, viewport);
    GLboolean colourMask[4] = {};
    glGetBooleanv(GL_COLOR_WRITEMASK, colourMask);
    const GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST);
    GLuint bits = 0;
    glGenTextures(1, &bits);
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, bits);
    GL_TexStorage2DFunc(GL_TEXTURE_2D, 1, GL_R32UI, w, h);
    GLuint fbo = 0;
    GL_GenFramebuffersFunc(1, &fbo);
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, fbo);
    GL_FramebufferTexture2DFunc(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bits, 0);
    const GLenum colour = GL_COLOR_ATTACHMENT0;
    GL_DrawBuffersFunc(1, &colour);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    const bool complete = boundFramebufferComplete();
    if(complete)
    {
        glViewport(0, 0, w, h);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDisable(GL_SCISSOR_TEST);
        GL_SetState(GLS_BLEND_OPAQUE | GLS_NO_ZTEST | GLS_NO_ZWRITE | GLS_CULL_NONE | GLS_ATTRIBS(0));
        GL_UseProgram(esReadback.depthProgram);
        GL_Uniform1iFunc(0, level);
        GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, tex);
        GLint sampler = 0;
        glGetIntegerv(GL_SAMPLER_BINDING, &sampler);
        GL_BindSamplerFunc(0, esReadback.sampler);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        GL_BindSamplerFunc(0, static_cast<GLuint>(sampler));
        za::Vector<GLuint> strip;
        strip.resize(static_cast<size_t>(w) * depthStripRows * 4);
        GLint packAlignment = 4;
        glGetIntegerv(GL_PACK_ALIGNMENT, &packAlignment);
        glPixelStorei(GL_PACK_ALIGNMENT, 4);
        for(int y = 0; y < h; y += depthStripRows)
        {
            const int rows = za::min(depthStripRows, h - y);
            glReadPixels(0, y, w, rows, GL_RGBA_INTEGER, GL_UNSIGNED_INT, strip.data());
            for(int r = 0; r < rows; r++)
            {
                unsigned char* row = out + static_cast<size_t>(y + r) * stride;
                for(int x = 0; x < w; x++)
                {
                    memcpy(row + static_cast<size_t>(x) * 4, &strip[(static_cast<size_t>(r) * w + x) * 4], 4);
                }
            }
        }
        glPixelStorei(GL_PACK_ALIGNMENT, packAlignment);
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        glColorMask(colourMask[0], colourMask[1], colourMask[2], colourMask[3]);
        if(scissor)
        {
            glEnable(GL_SCISSOR_TEST);
        }
    }
    GL_DeleteFramebuffersFunc(1, &fbo);
    GL_DeleteNativeTexture(bits);
    return complete;
}

[[nodiscard]] int channelsOf(GLenum format)
{
    switch(format)
    {
        case GL_RGBA: return 4;
        case GL_RGB: return 3;
        case GL_RED:
        case GL_DEPTH_COMPONENT: return 1;
        default: return 0;
    }
}

// The ES form of readTexImage (the framebuffer bindings kept; `out` already zeroed).
[[nodiscard]] bool readTexImageEs(GLenum target, GLint level, GLenum format, GLenum type, unsigned char* out, int w,
    int h, int layers, size_t stride)
{
    GLint name = 0;
    glGetIntegerv(bindingOf(target), &name);
    GLint depthType = GL_NONE, redType = GL_NONE, compressed = 0;
    glGetTexLevelParameteriv(target, level, GL_TEXTURE_DEPTH_TYPE, &depthType);
    glGetTexLevelParameteriv(target, level, GL_TEXTURE_RED_TYPE, &redType);
    glGetTexLevelParameteriv(target, level, GL_TEXTURE_COMPRESSED, &compressed);
    if(depthType != GL_NONE)
    {
        return format == GL_DEPTH_COMPONENT && type == GL_FLOAT && target == GL_TEXTURE_2D &&
               readDepthBits(static_cast<GLuint>(name), level, w, h, out, stride);
    }
    const bool floats = redType == GL_FLOAT;
    if(compressed || format == GL_DEPTH_COMPONENT || (!floats && redType != GL_UNSIGNED_NORMALIZED))
    {
        return false; // (integer and compressed textures: no ES read)
    }
    const int channels = channelsOf(format);
    const size_t texelBytes = floats ? 16 : 4; // what glReadPixels gives: RGBA floats or bytes
    za::Vector<unsigned char> rgba;
    rgba.resize(static_cast<size_t>(w) * static_cast<size_t>(h) * texelBytes);
    GLint packAlignment = 4;
    glGetIntegerv(GL_PACK_ALIGNMENT, &packAlignment);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    GLuint fbo = 0;
    GL_GenFramebuffersFunc(1, &fbo);
    GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, fbo);
    bool ok = true;
    for(int z = 0; z < layers && ok; z++)
    {
        if(target == GL_TEXTURE_3D || target == GL_TEXTURE_2D_ARRAY)
        {
            GL_FramebufferTextureLayerFunc(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, static_cast<GLuint>(name), level, z);
        }
        else
        {
            GL_FramebufferTexture2DFunc(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, target, static_cast<GLuint>(name), level);
        }
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        ok = GL_CheckFramebufferStatusFunc(GL_READ_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
        if(!ok)
        {
            break;
        }
        glReadPixels(0, 0, w, h, GL_RGBA, floats ? GL_FLOAT : GL_UNSIGNED_BYTE, rgba.data());
        for(int y = 0; y < h; y++)
        {
            unsigned char* row = out + (static_cast<size_t>(z) * h + y) * stride;
            for(int x = 0; x < w; x++)
            {
                const unsigned char* src = &rgba[(static_cast<size_t>(y) * w + x) * texelBytes];
                for(int c = 0; c < channels; c++)
                {
                    const size_t at = static_cast<size_t>(x) * channels + c;
                    if(type == GL_FLOAT)
                    {
                        float v = 0.f;
                        if(floats)
                        {
                            memcpy(&v, src + c * 4, 4);
                        }
                        else
                        {
                            v = static_cast<float>(src[c]) / 255.f;
                        }
                        memcpy(row + at * 4, &v, 4);
                    }
                    else if(floats)
                    {
                        float v = 0.f;
                        memcpy(&v, src + c * 4, 4);
                        row[at] = static_cast<unsigned char>(za::clamp(v, 0.f, 1.f) * 255.f + 0.5f);
                    }
                    else
                    {
                        row[at] = src[c];
                    }
                }
            }
        }
    }
    GL_DeleteFramebuffersFunc(1, &fbo);
    glPixelStorei(GL_PACK_ALIGNMENT, packAlignment);
    return ok;
}
} // namespace

bool readTexImage(GLenum target, GLint level, GLenum format, GLenum type, void* out, bool esForm)
{
    GLint w = 0, h = 0, d = 1;
    glGetTexLevelParameteriv(target, level, GL_TEXTURE_WIDTH, &w);
    glGetTexLevelParameteriv(target, level, GL_TEXTURE_HEIGHT, &h);
    if(target == GL_TEXTURE_3D || target == GL_TEXTURE_2D_ARRAY)
    {
        glGetTexLevelParameteriv(target, level, GL_TEXTURE_DEPTH, &d);
    }
    const int channels = channelsOf(format);
    const size_t typeBytes = type == GL_FLOAT ? 4 : type == GL_UNSIGNED_BYTE ? 1 : 0;
    GLint packAlignment = 4;
    glGetIntegerv(GL_PACK_ALIGNMENT, &packAlignment);
    const size_t align = static_cast<size_t>(za::max(packAlignment, 1));
    const size_t stride = (static_cast<size_t>(w) * channels * typeBytes + align - 1) / align * align;
    const size_t bytes = stride * static_cast<size_t>(h) * static_cast<size_t>(d);
#ifdef __ANDROID__
    esForm = true; // (OpenGL ES has no glGetTexImage: the ES form, whatever was asked)
#endif
    if(!esForm)
    {
#ifndef __ANDROID__
        glGetTexImage(target, level, format, type, out);
#endif
        return true;
    }
    memset(out, 0, bytes);
    if(!channels || !typeBytes || w <= 0 || h <= 0)
    {
        return false;
    }
    GLint drawFb = 0, readFb = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFb);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFb);
    GLint readBuffer = GL_BACK;
    glGetIntegerv(GL_READ_BUFFER, &readBuffer);
    const bool ok = readTexImageEs(target, level, format, type, static_cast<unsigned char*>(out), w, h, d, stride);
    GL_BindFramebufferFunc(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(drawFb));
    GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, static_cast<GLuint>(readFb));
    glReadBuffer(static_cast<GLenum>(readBuffer));
    return ok;
}

int readTexImageSame(GLenum target, GLint level, GLenum format, GLenum type, const char* what)
{
    GLint redType = GL_NONE, depthType = GL_NONE, compressed = 0;
    glGetTexLevelParameteriv(target, level, GL_TEXTURE_RED_TYPE, &redType);
    glGetTexLevelParameteriv(target, level, GL_TEXTURE_DEPTH_TYPE, &depthType);
    glGetTexLevelParameteriv(target, level, GL_TEXTURE_COMPRESSED, &compressed);
    if(compressed || (depthType == GL_NONE && redType != GL_FLOAT && redType != GL_UNSIGNED_NORMALIZED))
    {
        return -1;
    }
    GLint w = 0, h = 0, d = 1;
    glGetTexLevelParameteriv(target, level, GL_TEXTURE_WIDTH, &w);
    glGetTexLevelParameteriv(target, level, GL_TEXTURE_HEIGHT, &h);
    if(target == GL_TEXTURE_3D || target == GL_TEXTURE_2D_ARRAY)
    {
        glGetTexLevelParameteriv(target, level, GL_TEXTURE_DEPTH, &d);
    }
    const size_t bytes = static_cast<size_t>(w) * h * d * 4 * (type == GL_FLOAT ? 4 : 1) + 16; // (the most it needs)
    za::Vector<unsigned char> desktop, es;
    desktop.resize(bytes);
    es.resize(bytes);
    memset(desktop.data(), 0, bytes);
    memset(es.data(), 0, bytes);
    (void)readTexImage(target, level, format, type, desktop.data(), false);
    const bool read = readTexImage(target, level, format, type, es.data(), true);
    const bool same = read && memcmp(desktop.data(), es.data(), bytes) == 0;
    if(!same)
    {
        Con_Warning("readback %s (%dx%dx%d level %d): %s\n", what, w, h, d, level, read ? "differs" : "not read");
    }
    return same ? 1 : 0;
}

bool compiled(const GLuint shader, const char* name)
{
    return deferring || compileStatus(shader, name);
}

void beginDeferredChecks()
{
    deferring = gl_parallel_compile_able != 0;
    deferredPrograms.clear();
    deferredOwners.clear();
}

void ownDeferred(GLuint& program, bool* failed)
{
    if(deferring && program)
    {
        deferredOwners.pushBack(DeferredOwner{&program, failed});
    }
}

bool deferredComplete()
{
    constexpr GLenum completionStatus = 0x91B1; // GL_COMPLETION_STATUS_KHR (ARB's the same)
    for(const DeferredProgram& d : deferredPrograms)
    {
        GLint done = GL_FALSE;
        GL_GetProgramivFunc(d.program, completionStatus, &done);
        if(!done)
        {
            return false;
        }
    }
    return true;
}

void endDeferredChecks()
{
    deferring = false;
    for(const DeferredProgram& d : deferredPrograms)
    {
        // (each status read waits for that one only: the rest went on compiling meanwhile)
        bool ok = !d.vs || compileStatus(d.vs, d.name);
        ok = (!d.fs || compileStatus(d.fs, d.name)) && ok;
        ok = ok && linkStatus(d.program, d.name);
        if(!ok)
        {
            // As a one-at-a-time make would have: the owner's program 0 and its failure flag set (its fallback
            // path or a skipped pass), the program deleted.
            bool owned = false;
            for(const DeferredOwner& o : deferredOwners)
            {
                if(*o.program == d.program)
                {
                    *o.program = 0;
                    if(o.failed)
                    {
                        *o.failed = true;
                    }
                    owned = true;
                }
            }
            if(owned)
            {
                GL_DeleteProgramFunc(d.program);
                continue;
            }
            relinkToNothing(d);
        }
        GL_ObjectLabelFunc(GL_PROGRAM, d.program, -1, d.name);
    }
    deferredPrograms.clear();
    deferredPrograms.shrinkToFit();
    deferredOwners.clear();
    deferredOwners.shrinkToFit();
}

GLuint linkProgram(const GLuint vs, const GLuint fs, const char* name)
{
    const GLuint p = GL_CreateProgramFunc();
    if(vs)
    {
        GL_AttachShaderFunc(p, vs);
    }
    if(fs)
    {
        GL_AttachShaderFunc(p, fs);
    }
    VR_GLStep("link %s", name);
    GL_LinkProgramFunc(p);
    if(vs)
    {
        GL_DeleteShaderFunc(vs);
    }
    if(fs)
    {
        GL_DeleteShaderFunc(fs);
    }
    if(deferring)
    {
        DeferredProgram d;
        d.program = p;
        d.vs = vs;
        d.fs = fs;
        q_strlcpy(d.name, name, sizeof(d.name));
        deferredPrograms.pushBack(d);
        return p; // (labelled at endDeferredChecks: the driver's label waits for the link)
    }
    GLint ok = 0;
    GL_GetProgramivFunc(p, GL_LINK_STATUS, &ok);
    if(!ok)
    {
        char log[2048];
        log[0] = 0;
        GL_GetProgramInfoLogFunc(p, sizeof(log), nullptr, log);
        Con_Warning("VR: %s shader failed to link:\n%s\n", name, log);
        (void)VR_GLESShaderError(nullptr); // (-glescompat: counted)
        GL_DeleteProgramFunc(p);
        return 0;
    }
    GL_ObjectLabelFunc(GL_PROGRAM, p, -1, name);
    return p;
}

bool boundFramebufferComplete()
{
    return GL_CheckFramebufferStatusFunc(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}

void RenderTarget::destroy()
{
    if(fbo)
    {
        GL_DeleteFramebuffersFunc(1, &fbo);
    }
    if(tex)
    {
        GL_DeleteNativeTexture(tex); // (and out of the engine's bound-texture cache: its name comes back at once)
    }
    *this = RenderTarget{};
}

int targetsMade = 0;

namespace
{

struct TargetMade
{
    const char* name;
    int count;
};
za::Vector<TargetMade> targetsMadeNamed; // by name, in the order first made (the main thread's)

void noteMade(const char* name, int w, int h, int wasW, int wasH)
{
    targetsMade++;
    TargetMade* named = nullptr;
    for(TargetMade& n : targetsMadeNamed)
    {
        if(ZA_STRCMP(n.name, name) == 0)
        {
            named = &n;
        }
    }
    if(!named)
    {
        targetsMadeNamed.pushBack({name, 0});
        named = &targetsMadeNamed.back();
    }
    named->count++;
    Con_DPrintf("VR: render target \"%s\" made at %dx%d (was %dx%d), %d times so far\n", name, w, h, wasW, wasH,
        named->count);
}

} // namespace

za::String targetsMadeByName()
{
    za::String out;
    for(const TargetMade& n : targetsMadeNamed)
    {
        out += (out.empty() ? "" : " ") + za::String{n.name} + ":" + za::toString(n.count);
    }
    return out;
}

bool RenderTarget::ensure(const int w, const int h, const RenderTargetFormat& format, const char* name)
{
    int wanted = 1;
    while(format.mipmaps && za::max(w, h) >> wanted)
    {
        wanted++;
    }
    if(tex && width == w && height == h && levels == wanted)
    {
        return true;
    }
    noteMade(name, w, h, width, height);
    destroy();
    glGenTextures(1, &tex);
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, tex);
    GL_TexStorage2DFunc(GL_TEXTURE_2D, wanted, format.internalFormat, w, h);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, wanted > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    if(format.clampToEdge)
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    if(format.maxLevel0)
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    }
    if(format.shadowCompare)
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_GEQUAL);
    }
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, 0);
    GL_ObjectLabelFunc(GL_TEXTURE, tex, -1, name); // (vr_vram_report)

    GL_GenFramebuffersFunc(1, &fbo);
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, fbo);
    if(format.fboName)
    {
        GL_ObjectLabelFunc(GL_FRAMEBUFFER, fbo, -1, format.fboName);
    }
    GL_FramebufferTexture2DFunc(GL_FRAMEBUFFER, format.depth ? GL_DEPTH_ATTACHMENT : GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    if(format.depth)
    {
        gfx::drawNoColour();
        glReadBuffer(GL_NONE);
    }
    if(!boundFramebufferComplete())
    {
        destroy();
        return false;
    }
    width = w;
    height = h;
    levels = wanted;
    return true;
}

} // namespace qvr::gfx

extern "C" void VR_GLGetTexImage(unsigned target, int level, unsigned format, unsigned type, void* pixels)
{
    (void)qvr::gfx::readTexImage(target, level, format, type, pixels, VR_GLESCompat() != 0);
}

extern "C" int VR_GLReadTexImageSame(unsigned target, int level, unsigned format, unsigned type, const char* what)
{
    return qvr::gfx::readTexImageSame(target, level, format, type, what);
}
