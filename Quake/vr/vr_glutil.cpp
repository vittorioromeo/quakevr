// vr_glutil.cpp -- the GL object helpers every effect's shaders and targets share (vr_glutil.hpp).

#include "vr_glutil.hpp"

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

bool deferring = false;
za::Vector<DeferredProgram> deferredPrograms;

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
    }
    return ok != 0;
}

constexpr const char* nothingVs = "#version 430\nvoid main() { gl_Position = vec4(0.0, 0.0, 2.0, 1.0); }\n"; // clipped
constexpr const char* nothingFs = "#version 430\nvoid main() { discard; }\n";
constexpr const char* nothingCs = "#version 430\nlayout(local_size_x = 1) in;\nvoid main() {}\n";

// A deferred program that failed: relinked from shaders that draw (or dispatch) nothing, the stages it had.
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
        GL_ShaderSourceFunc(s, 1, &source, nullptr);
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

bool compiled(const GLuint shader, const char* name)
{
    return deferring || compileStatus(shader, name);
}

void beginDeferredChecks()
{
    deferring = gl_parallel_compile_able != 0;
    deferredPrograms.clear();
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
            relinkToNothing(d);
        }
        GL_ObjectLabelFunc(GL_PROGRAM, d.program, -1, d.name);
    }
    deferredPrograms.clear();
    deferredPrograms.shrinkToFit();
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

bool RenderTarget::ensure(const int w, const int h, const RenderTargetFormat& format, const char* name)
{
    if(tex && width == w && height == h)
    {
        return true;
    }
    destroy();
    glGenTextures(1, &tex);
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, tex);
    GL_TexStorage2DFunc(GL_TEXTURE_2D, 1, format.internalFormat, w, h);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
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
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
    }
    if(!boundFramebufferComplete())
    {
        destroy();
        return false;
    }
    width = w;
    height = h;
    return true;
}

} // namespace qvr::gfx
