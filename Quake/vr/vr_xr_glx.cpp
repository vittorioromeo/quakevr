// vr_xr_glx.cpp -- Linux: the game's GLX context for the OpenXR session's binding (vr_xr_glx.hpp).

#if defined(QVR_HAVE_OPENXR) && !defined(_WIN32)

#include "vr_xr_glx.hpp"

#include <GL/glx.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

namespace qvr::xrglx
{

bool current(Binding& out, const char** why)
{
    out = Binding{};
    Display* const display = glXGetCurrentDisplay();
    const GLXContext context = glXGetCurrentContext();
    if(!display || !context)
    {
        *why = "no GLX context current (SDL's may be EGL: SDL_VIDEO_X11_FORCE_EGL=0, SDL_VIDEODRIVER=x11)";
        return false;
    }
    out.display = display;
    out.context = context;
    out.drawable = glXGetCurrentDrawable();
    // The context's framebuffer configuration and its visual (GLX 1.3: the context's FBConfig id, looked up).
    int id = 0;
    if(glXQueryContext(display, context, GLX_FBCONFIG_ID, &id) == Success && id)
    {
        int screen = DefaultScreen(display);
        glXQueryContext(display, context, GLX_SCREEN, &screen);
        const int attributes[] = {GLX_FBCONFIG_ID, id, None};
        int count = 0;
        GLXFBConfig* configs = glXChooseFBConfig(display, screen, attributes, &count);
        if(configs && count > 0)
        {
            out.fbConfig = configs[0];
            if(XVisualInfo* visual = glXGetVisualFromFBConfig(display, configs[0]))
            {
                out.visualId = static_cast<uint32_t>(visual->visualid);
                XFree(visual);
            }
        }
        if(configs)
        {
            XFree(configs);
        }
    }
    *why = out.fbConfig ? "" : "the context's FBConfig not found (bound without it)";
    return true;
}

bool makeCurrent(const Binding& b)
{
    return b.display && glXMakeCurrent(static_cast<Display*>(b.display), b.drawable, static_cast<GLXContext>(b.context));
}

void release()
{
    if(Display* const display = glXGetCurrentDisplay())
    {
        glXMakeCurrent(display, None, nullptr);
    }
}

void* proc(const char* name)
{
    return reinterpret_cast<void*>(glXGetProcAddressARB(reinterpret_cast<const GLubyte*>(name)));
}

} // namespace qvr::xrglx

#endif
