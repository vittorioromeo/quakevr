#include "gl_util.hpp"
#include "glquake.hpp"

#include <GL/glew.h>

namespace quake::vr::gl_util
{

[[nodiscard]] gl_beginend_guard::gl_beginend_guard(const GLenum mode) noexcept
{
    glBegin(mode);
}

gl_beginend_guard::~gl_beginend_guard() noexcept
{
    glEnd();
}

void gl_showfn_guard::setup_gl() noexcept
{
    glDisable(GL_DEPTH_TEST);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    GL_PolygonOffset(OFFSET_SHOWTRIS);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);
}

void gl_showfn_guard::cleanup_gl() noexcept
{
    glColor3f(1, 1, 1);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    GL_PolygonOffset(OFFSET_NONE);
    glEnable(GL_DEPTH_TEST);
}

[[nodiscard]] gl_showfn_guard::gl_showfn_guard() noexcept
{
    setup_gl();
}

gl_showfn_guard::~gl_showfn_guard() noexcept
{
    cleanup_gl();
}

void gl_vertex(const qvec2& v) noexcept
{
    glVertex2f(v[0], v[1]);
}

void gl_vertex(const qvec3& v) noexcept
{
    glVertex3f(v[0], v[1], v[2]);
}

} // namespace quake::vr::gl_util
