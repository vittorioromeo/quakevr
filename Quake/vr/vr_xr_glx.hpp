// vr_xr_glx.hpp -- Linux: the game's GLX context as the OpenXR backend binds it to the session
// (XrGraphicsBindingOpenGLXlibKHR, XR_KHR_opengl_enable). Its own translation unit: the X11 and GLX headers' macros
// (None, Bool, Status, True...) stay out of the backend's (vr_backend_openxr.cpp).

#pragma once

#include <stdint.h>

namespace qvr::xrglx
{

// The current GLX context and what the binding needs of it (the handles as the GLX types: Display*, GLXContext,
// GLXFBConfig; drawable a GLXDrawable, an XID).
struct Binding
{
    void* display{nullptr};
    unsigned long drawable{0};
    void* context{nullptr};
    void* fbConfig{nullptr};
    uint32_t visualId{0};
};

// The calling thread's current GLX context (false: none: SDL made an EGL context, or nothing is current); `why` says
// which, for the log.
[[nodiscard]] bool current(Binding& out, const char** why);

// `b`'s context current on the calling thread again (false: glXMakeCurrent failed).
[[nodiscard]] bool makeCurrent(const Binding& b);

// No context current on the calling thread (vr_xr_test_drop_context).
void release();

// A GL or GLX function by name (glXGetProcAddressARB; null: none).
[[nodiscard]] void* proc(const char* name);

} // namespace qvr::xrglx
