#pragma once

// vr_glutil.hpp -- the GL object helpers every effect's shaders and targets share. See vr_glutil.cpp.

#include "vr_engine.hpp"

namespace qvr::gfx
{

// Links a program of vertex shader `vs` and fragment (or compute) shader `fs` (either may be 0: not attached), then
// deletes the shaders. A failed link warns with the info log, deletes the program and gives 0; else the program,
// labelled `name`.
[[nodiscard]] GLuint linkProgram(GLuint vs, GLuint fs, const char* name);

// Whether the framebuffer bound to GL_FRAMEBUFFER is complete.
[[nodiscard]] bool boundFramebufferComplete();

// How a RenderTarget's texture is made (one level, bilinear).
struct RenderTargetFormat
{
    GLenum internalFormat{GL_RGBA8};
    bool depth{false};            // a depth texture: the depth attachment, no draw or read buffer
    bool shadowCompare{false};    // sampled as a shadow map (compared to the reference: lit where GEQUAL)
    bool clampToEdge{true};       // else the default wrap (repeat)
    bool maxLevel0{false};        // GL_TEXTURE_MAX_LEVEL 0 set
    const char* fboName{nullptr}; // the framebuffer's label (none: unlabelled)
};

// A texture with a framebuffer drawing into it, made for a size (the effects' offscreen targets).
struct RenderTarget
{
    GLuint tex{0};
    GLuint fbo{0};
    int width{0};
    int height{0};

    // Made for width x height (kept when it already is; texture unit 0 left unbound, its framebuffer left bound),
    // `name` the texture's label. False, and empty, when the framebuffer is incomplete (the caller warns).
    [[nodiscard]] bool ensure(int w, int h, const RenderTargetFormat& format, const char* name);
    void destroy();
};

} // namespace qvr::gfx
