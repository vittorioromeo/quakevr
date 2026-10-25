// vr_xr_linuxgl.cpp -- Linux: the game's GLX or EGL context for the OpenXR session's binding (vr_xr_linuxgl.hpp).

#if defined(QVR_HAVE_OPENXR) && !defined(_WIN32)

#include "vr_xr_linuxgl.hpp"

#include <GL/glx.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <dlfcn.h>
#include <stdio.h>

namespace qvr::xrgl
{

namespace
{

// EGL, as libEGL has it (the types and values of EGL/egl.h, which isn't included: no header or link dependency).
using EGLint = int32_t;
using EGLBoolean = unsigned int;
constexpr EGLint eglConfigId = 0x3028;   // EGL_CONFIG_ID
constexpr EGLint eglNone = 0x3038;       // EGL_NONE
constexpr EGLint eglVendor = 0x3053;     // EGL_VENDOR
constexpr EGLint eglVersion = 0x3054;    // EGL_VERSION
constexpr EGLint eglDrawSurface = 0x3059; // EGL_DRAW
constexpr EGLint eglReadSurface = 0x305A; // EGL_READ

struct Egl
{
    bool looked{false};
    void* library{nullptr};
    void* (*getCurrentContext)(){nullptr};
    void* (*getCurrentDisplay)(){nullptr};
    void* (*getCurrentSurface)(EGLint){nullptr};
    EGLBoolean (*queryContext)(void*, void*, EGLint, EGLint*){nullptr};
    EGLBoolean (*chooseConfig)(void*, const EGLint*, void**, EGLint, EGLint*){nullptr};
    EGLBoolean (*makeCurrent)(void*, void*, void*, void*){nullptr};
    const char* (*queryString)(void*, EGLint){nullptr};
    void* getProcAddress{nullptr}; // void (*(*)(const char*))()
};

Egl egl;
char eglText[256];

template <typename F>
void bind(F& f, const char* name)
{
    f = reinterpret_cast<F>(dlsym(egl.library, name));
}

// libEGL's functions when the process has it loaded (SDL loads it for an EGL context; it is not loaded here).
const Egl& eglFunctions()
{
    if(egl.looked)
    {
        return egl;
    }
    egl.looked = true;
    constexpr const char* names[] = {"libEGL.so.1", "libEGL.so"};
    for(const char* name : names)
    {
        egl.library = egl.library ? egl.library : dlopen(name, RTLD_LAZY | RTLD_NOLOAD);
    }
    if(!egl.library)
    {
        return egl;
    }
    bind(egl.getCurrentContext, "eglGetCurrentContext");
    bind(egl.getCurrentDisplay, "eglGetCurrentDisplay");
    bind(egl.getCurrentSurface, "eglGetCurrentSurface");
    bind(egl.queryContext, "eglQueryContext");
    bind(egl.chooseConfig, "eglChooseConfig");
    bind(egl.makeCurrent, "eglMakeCurrent");
    bind(egl.queryString, "eglQueryString");
    egl.getProcAddress = dlsym(egl.library, "eglGetProcAddress");
    return egl;
}

bool glxCurrent(Binding& out, const char** why)
{
    Display* const display = glXGetCurrentDisplay();
    const GLXContext context = glXGetCurrentContext();
    if(!display || !context)
    {
        return false;
    }
    out.api = Api::Glx;
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
    *why = out.fbConfig ? "" : "the GLX context's FBConfig not found (bound without it)";
    return true;
}

bool eglCurrent(Binding& out, const char** why)
{
    const Egl& e = eglFunctions();
    if(!e.getCurrentContext || !e.getCurrentDisplay || !e.getCurrentSurface)
    {
        return false;
    }
    void* const context = e.getCurrentContext();
    void* const display = e.getCurrentDisplay();
    if(!context || !display)
    {
        return false;
    }
    out.api = Api::Egl;
    out.context = context;
    out.eglDisplay = display;
    out.eglDraw = e.getCurrentSurface(eglDrawSurface);
    out.eglRead = e.getCurrentSurface(eglReadSurface);
    // The context's config (EGL_CONFIG_ID, looked up; none for a context made without one, EGL_KHR_no_config_context).
    EGLint id = 0;
    if(e.queryContext && e.chooseConfig && e.queryContext(display, context, eglConfigId, &id) && id)
    {
        const EGLint attributes[] = {eglConfigId, id, eglNone};
        void* config = nullptr;
        EGLint count = 0;
        if(e.chooseConfig(display, attributes, &config, 1, &count) && count > 0)
        {
            out.eglConfig = config;
        }
    }
    *why = out.eglConfig ? "" : "the EGL context's config not found (bound without it)";
    return true;
}

} // namespace

const char* apiName(Api api)
{
    return api == Api::Glx ? "GLX" : api == Api::Egl ? "EGL" : "none";
}

Binding current(const char** why)
{
    Binding out;
    *why = "";
    if(glxCurrent(out, why))
    {
        return out;
    }
    out = Binding{};
    if(eglCurrent(out, why))
    {
        return out;
    }
    *why = eglFunctions().library ? "no GL context current (neither GLX nor EGL)"
                                  : "no GL context current (no GLX context, and libEGL is not loaded)";
    return Binding{};
}

bool same(const Binding& a, const Binding& b)
{
    return a.api == b.api && a.context == b.context && a.drawable == b.drawable && a.eglDraw == b.eglDraw &&
           a.eglRead == b.eglRead;
}

bool makeCurrent(const Binding& b)
{
    if(b.api == Api::Glx)
    {
        return b.display && glXMakeCurrent(static_cast<Display*>(b.display), b.drawable, static_cast<GLXContext>(b.context));
    }
    const Egl& e = eglFunctions();
    return b.api == Api::Egl && e.makeCurrent && e.makeCurrent(b.eglDisplay, b.eglDraw, b.eglRead, b.context);
}

void release()
{
    if(Display* const display = glXGetCurrentDisplay(); display && glXGetCurrentContext())
    {
        glXMakeCurrent(display, None, nullptr);
        return;
    }
    const Egl& e = eglFunctions();
    if(e.getCurrentDisplay && e.makeCurrent)
    {
        if(void* const display = e.getCurrentDisplay())
        {
            e.makeCurrent(display, nullptr, nullptr, nullptr);
        }
    }
}

void* proc(Api api, const char* name)
{
    if(api == Api::Egl)
    {
        void* const fn = eglGetProcAddressFn();
        return fn ? reinterpret_cast<void* (*)(const char*)>(fn)(name) : nullptr;
    }
    return reinterpret_cast<void*>(glXGetProcAddressARB(reinterpret_cast<const GLubyte*>(name)));
}

void* eglGetProcAddressFn()
{
    return eglFunctions().getProcAddress;
}

const char* eglDescription(const Binding& b)
{
    const Egl& e = eglFunctions();
    eglText[0] = 0;
    if(b.api == Api::Egl && e.queryString)
    {
        const char* version = e.queryString(b.eglDisplay, eglVersion);
        const char* vendor = e.queryString(b.eglDisplay, eglVendor);
        snprintf(eglText, sizeof(eglText), "EGL %s, %s", version ? version : "?", vendor ? vendor : "?");
    }
    return eglText;
}

} // namespace qvr::xrgl

#endif
