#pragma once

#include "quakeglm_qvec2.hpp"
#include "quakeglm_qvec3.hpp"

#include <GL/glew.h>

namespace quake::vr::gl_util
{

struct gl_beginend_guard
{
    [[nodiscard]] gl_beginend_guard(const GLenum mode) noexcept;
    ~gl_beginend_guard() noexcept;
};

class gl_showfn_guard
{
private:
    static void setup_gl() noexcept;
    static void cleanup_gl() noexcept;

public:
    [[nodiscard]] gl_showfn_guard() noexcept;
    ~gl_showfn_guard() noexcept;

    gl_showfn_guard(const gl_showfn_guard&) = delete;
    gl_showfn_guard(gl_showfn_guard&&) = delete;

    gl_showfn_guard& operator=(const gl_showfn_guard&) = delete;
    gl_showfn_guard& operator=(gl_showfn_guard&&) = delete;

    template <typename F>
    void draw_points_with_size(float size, F&& f_draw) noexcept
    {
        glEnable(GL_POINT_SMOOTH);
        glPointSize(size);

        {
            const gl_beginend_guard guard{GL_POINTS};
            f_draw();
        }

        glDisable(GL_POINT_SMOOTH);
    }

    template <typename F>
    void draw_points(F&& f_draw) noexcept
    {
        draw_points_with_size(12.f, f_draw);
    }

    template <typename F>
    void draw_points_and_lines(F&& f_draw) noexcept
    {
        glLineWidth(2.f * glwidth / vid.width);

        glEnable(GL_LINE_SMOOTH);
        glShadeModel(GL_SMOOTH);

        {
            const gl_beginend_guard guard{GL_LINES};
            f_draw();
        }

        glShadeModel(GL_FLAT);
        glDisable(GL_LINE_SMOOTH);

        draw_points(f_draw);
    }

    template <typename F>
    void draw_line_strip(const float size, F&& f_draw)
    {
        glLineWidth(size * glwidth / vid.width);

        glEnable(GL_LINE_SMOOTH);
        glShadeModel(GL_SMOOTH);

        {
            const gl_beginend_guard guard{GL_LINE_STRIP};
            f_draw();
        }

        glShadeModel(GL_FLAT);
        glDisable(GL_LINE_SMOOTH);
    }
};

void gl_vertex(const qvec2& v) noexcept;
void gl_vertex(const qvec3& v) noexcept;

} // namespace quake::vr::gl_util
