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

} // namespace qvr::gfx
