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

} // namespace qvr::gfx
