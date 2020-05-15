#pragma once

#include <GL/glew.h>

#include <string_view>
#include <filesystem>

namespace quake
{

[[nodiscard]] GLuint make_gl_program(const std::string_view src_vertex,
    const std::string_view src_geometry,
    const std::string_view src_frag) noexcept;

[[nodiscard]] GLuint make_gl_program(const std::string_view src_vertex,
    const std::string_view src_frag) noexcept;

} // namespace quake
