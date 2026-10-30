// vr_glutil.cpp -- the GL object helpers every effect's shaders and targets share (vr_glutil.hpp).

#include "vr_glutil.hpp"

namespace qvr::gfx
{

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
