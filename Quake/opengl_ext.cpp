#include "opengl_ext.hpp"

/*
namespace
{
    struct
    {
        void* func;
        const char* name;
    } quake_gl_exts[] = {
        {&glBindFramebufferEXT, "glBindFramebufferEXT"},
        {&glBlitFramebufferEXT, "glBlitFramebufferEXT"},
        {&glDeleteFramebuffersEXT, "glDeleteFramebuffersEXT"},
        {&glGenFramebuffersEXT, "glGenFramebuffersEXT"},
        {&glTexImage2DMultisampleEXT, "glTexImage2DMultisample"},
        {&glFramebufferTexture2DEXT, "glFramebufferTexture2DEXT"},
        {&glFramebufferRenderbufferEXT, "glFramebufferRenderbufferEXT"},
        {&glCheckFramebufferStatusEXT, "glCheckFramebufferStatusEXT"},
        {&wglSwapIntervalEXT, "wglSwapIntervalEXT"},
        {nullptr, nullptr},
    };
} // namespace

PFNGLBINDFRAMEBUFFEREXTPROC glBindFramebufferEXT;
PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC glCheckFramebufferStatusEXT;
PFNGLBLITFRAMEBUFFEREXTPROC glBlitFramebufferEXT;
PFNGLDELETEFRAMEBUFFERSEXTPROC glDeleteFramebuffersEXT;
PFNGLGENFRAMEBUFFERSEXTPROC glGenFramebuffersEXT;
PFNGLFRAMEBUFFERTEXTURE2DEXTPROC glFramebufferTexture2DEXT;
PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC glFramebufferRenderbufferEXT;
PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT;
PFNGLTEXIMAGE2DMULTISAMPLEPROC glTexImage2DMultisampleEXT;

namespace quake::gl
{
    [[nodiscard]] bool InitOpenGLExtensions() noexcept
    {
        static bool extensions_initialized{false};

        if(extensions_initialized)
        {
            return true;
        }

        for(int i = 0; quake_gl_exts[i].func; ++i)
        {
            void* const func = SDL_GL_GetProcAddress(quake_gl_exts[i].name);
            if(!func)
            {
                return false;
            }

            *((void**)quake_gl_exts[i].func) = func;
        }

        extensions_initialized = true;
        return extensions_initialized;
    }
} // namespace quake::gl
*/
