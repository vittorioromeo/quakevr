#pragma once

// vr_glutil.hpp -- the GL object helpers every effect's shaders and targets share. See vr_glutil.cpp.

#include "vr_engine.hpp"

#include "Zancle/String/String.hpp"

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

// Called by a program's owner right after making it (`program` and `failed`, null if it has no flag, are its own
// variables, alive until endDeferredChecks): while the checks are deferred, if the program turns out to have failed,
// endDeferredChecks sets `program` to 0 and `*failed` to true. Otherwise nothing.
void ownDeferred(GLuint& program, bool* failed);

// Whether the framebuffer bound to GL_FRAMEBUFFER is complete.
[[nodiscard]] bool boundFramebufferComplete();

// How a RenderTarget's texture is made (one level, bilinear; or with mipmaps, trilinear).
struct RenderTargetFormat
{
    GLenum internalFormat{GL_RGBA8};
    bool mipmaps{false};          // a whole mipmap chain (filtered trilinearly; rebuilt by its user: end2D, Shade::Screen's glow)
    bool depth{false};            // a depth texture: the depth attachment, no draw or read buffer
    bool shadowCompare{false};    // sampled as a shadow map (compared to the reference: lit where GEQUAL)
    bool clampToEdge{true};       // else the default wrap (repeat)
    bool maxLevel0{false};        // GL_TEXTURE_MAX_LEVEL 0 set
    const char* fboName{nullptr}; // the framebuffer's label (none: unlabelled)
};

// A texture with a framebuffer drawing into it, made for a size (the effects' offscreen targets, the panels' canvases,
// the in-world screens and boards).
struct RenderTarget
{
    GLuint tex{0};
    GLuint fbo{0};
    int width{0};
    int height{0};
    int levels{1}; // its mipmap levels (RenderTargetFormat::mipmaps: more than 1)

    // Made for width x height (kept when it already is, with as many levels; texture unit 0 left unbound, its
    // framebuffer left bound), `name` (a literal) the texture's label and what it counts as (targetsMadeByName). False,
    // and empty, when the framebuffer is incomplete (the caller warns).
    [[nodiscard]] bool ensure(int w, int h, const RenderTargetFormat& format, const char* name);
    void destroy();
};

// How many times RenderTargets were (re)made so far (vr_memstats: steady in play, not a frame), and by name, as
// "name:count" words.
extern int targetsMade;
[[nodiscard]] za::String targetsMadeByName();

} // namespace qvr::gfx
