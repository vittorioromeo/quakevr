// vr_xr_linuxgl.hpp -- Linux: the game's GL context as the OpenXR backend binds it to the session: GLX
// (XrGraphicsBindingOpenGLXlibKHR, XR_KHR_opengl_enable) or EGL (XrGraphicsBindingEGLMNDX, XR_MNDX_egl_enable: SDL's
// Wayland driver, or its X11 driver on EGL). Its own translation unit: the X11 and GLX headers' macros (None, Bool,
// Status, True...) stay out of the backend's (vr_backend_openxr.cpp). EGL is looked up at run time in the libEGL SDL
// loaded (dlopen RTLD_NOLOAD): no EGL headers or link dependency.

#pragma once

#include <stdint.h>

namespace qvr::xrgl
{

enum class Api : uint8_t
{
    None,
    Glx,
    Egl,
};

// The current context and what the binding needs of it (the handles as the GLX and EGL types: Display*, GLXContext,
// GLXFBConfig, EGLDisplay, EGLConfig, EGLContext, EGLSurface; drawable a GLXDrawable, an XID).
struct Binding
{
    Api api{Api::None};
    void* context{nullptr}; // GLXContext or EGLContext
    // GLX
    void* display{nullptr};
    unsigned long drawable{0};
    void* fbConfig{nullptr};
    uint32_t visualId{0};
    // EGL
    void* eglDisplay{nullptr};
    void* eglConfig{nullptr};
    void* eglDraw{nullptr};
    void* eglRead{nullptr};
};

[[nodiscard]] const char* apiName(Api api);

// The calling thread's current context: GLX's if one is, else EGL's (Api::None: neither); `why` says what is missing
// (an empty string: nothing), for the log.
[[nodiscard]] Binding current(const char** why);

// The same context and surfaces (drawable) as `b`.
[[nodiscard]] bool same(const Binding& a, const Binding& b);

// `b`'s context current on the calling thread again (false: glXMakeCurrent or eglMakeCurrent failed).
[[nodiscard]] bool makeCurrent(const Binding& b);

// No context current on the calling thread (vr_xr_test_drop_context).
void release();

// A GL function by name, through `api`'s loader (glXGetProcAddressARB, eglGetProcAddress; null: none).
[[nodiscard]] void* proc(Api api, const char* name);

// eglGetProcAddress itself, for XrGraphicsBindingEGLMNDX.getProcAddress (null: libEGL not loaded).
[[nodiscard]] void* eglGetProcAddressFn();

// The EGL display's vendor and version ("EGL <version>, <vendor>"; "" when `b` is not EGL), for the log.
[[nodiscard]] const char* eglDescription(const Binding& b);

} // namespace qvr::xrgl
