#pragma once

// vr_glutil.hpp -- the GL object helpers every effect's shaders and targets share. See vr_glutil.cpp.

#include "vr_engine.hpp"

namespace qvr::gfx
{

// Links a program of vertex shader `vs` and fragment (or compute) shader `fs` (either may be 0: not attached), then
// deletes the shaders. A failed link warns with the info log, deletes the program and gives 0; else the program,
// labelled `name`.
[[nodiscard]] GLuint linkProgram(GLuint vs, GLuint fs, const char* name);

// Whether `shader` (just compiled, labelled `name`) compiled: a failure warns with the info log. True without reading
// its status while the checks are deferred (beginDeferredChecks): read then at endDeferredChecks.
[[nodiscard]] bool compiled(GLuint shader, const char* name);

// gfx::precompilePrograms with the driver's parallel compile (gl_parallel_compile_able): between these, every
// compile and link is queued without reading its status (linkProgram gives the program), so the driver's threads take
// them all; endDeferredChecks then reads each one's statuses in order, warns as compiled/linkProgram would and labels
// the programs (the driver's glObjectLabel waits for the link: 480 of a cold 555 ms). A program that failed is
// handled as a one-at-a-time make would have: its owner (ownDeferred) gets 0 and its failure flag set, so it takes its
// fallback or skips its pass, and the program is deleted. (One with no owner said: relinked to a program that draws
// nothing, a safety net.) Without the extension they do nothing.
void beginDeferredChecks();
void endDeferredChecks();
// Whether the driver has finished every deferred program (GL_COMPLETION_STATUS_KHR: never waits); true when none.
[[nodiscard]] bool deferredComplete();

// Called by a program's owner right after making it (`program` and `failed`, null if it has no flag, are its own
// variables, alive until endDeferredChecks): while the checks are deferred, if the program turns out to have failed,
// endDeferredChecks sets `program` to 0 and `*failed` to true. Otherwise nothing.
void ownDeferred(GLuint& program, bool* failed);

// Whether the framebuffer bound to GL_FRAMEBUFFER is complete.
[[nodiscard]] bool boundFramebufferComplete();

// The bound framebuffer draws no colour (glDrawBuffers with GL_NONE: the form ES has).
void drawNoColour();

// `size` bytes at `offset` of the buffer bound to `target` read into `out`, waiting for the GPU: glGetBufferSubData,
// or under -glescompat a read mapping (ES has no glGetBufferSubData). False when neither can.
[[nodiscard]] bool readBuffer(GLenum target, GLintptr offset, GLsizeiptr size, void* out);

// Level 0 of the 2D texture `tex` set to `rgba` everywhere: glClearTexImage, or under -glescompat a clear through a
// framebuffer (ES has no glClearTexImage; `tex` must be colour-renderable). The framebuffer binding is kept.
void clearTexture(GLuint tex, const float rgba[4]);

// Level `level` of the texture bound to `target` (GL_TEXTURE_2D, 3D, 2D_ARRAY or a cube face) read into `out` as
// glGetTexImage would (`format` GL_RGBA, GL_RGB, GL_RED or GL_DEPTH_COMPONENT; `type` GL_UNSIGNED_BYTE or GL_FLOAT;
// the pack alignment kept), waiting for the GPU. With `esForm` (VR_GLGetTexImage under -glescompat) the OpenGL ES
// form, since ES has no glGetTexImage: each layer attached to a framebuffer and read with glReadPixels in the
// format ES guarantees (RGBA bytes from normalized textures, RGBA floats from float ones), then converted; a depth
// texture (2D) copied first, bit for bit, into an R32UI one (ES reads no depth). False, `out` zeroed, when it cannot:
// an integer, compressed or not colour-renderable texture under the ES form, or a format or type not listed.
[[nodiscard]] bool readTexImage(GLenum target, GLint level, GLenum format, GLenum type, void* out, bool esForm);

// vr_gles_readback_test's part: level `level` of the texture bound to `target` read both ways (readTexImage with and
// without esForm) as `format`/`type`: 1 the same bytes, 0 not (a warning names `what`), -1 the ES form cannot read it
// (an integer or compressed texture: skipped).
[[nodiscard]] int readTexImageSame(GLenum target, GLint level, GLenum format, GLenum type, const char* what);

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
