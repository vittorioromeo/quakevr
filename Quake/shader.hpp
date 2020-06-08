#pragma once

#include <GL/glew.h>

#include <string_view>
#include <filesystem>

// TODO VR: (P2) pimpl
#include <GL/glew.h>

#include <string_view>
#include <filesystem>
#include <cassert>
#include <vector>
#include <cstdio>

namespace quake
{

[[nodiscard]] GLuint make_gl_program(const std::string_view src_vertex,
    const std::string_view src_geometry,
    const std::string_view src_frag) noexcept;

[[nodiscard]] GLuint make_gl_program(const std::string_view src_vertex,
    const std::string_view src_frag) noexcept;

struct gl_shader_src
{
    GLenum _shader_type;
    std::string_view _src;
};

struct gl_attr_binding
{
    std::string_view _name;
    GLuint _attr_index;
};

class gl_program_builder
{
private:
    std::vector<gl_shader_src> _shader_srcs;
    std::vector<gl_attr_binding> _attr_bindings;

public:
    [[nodiscard]] gl_program_builder&& add_shader(
        const gl_shader_src& shader_src) &&;

    [[nodiscard]] gl_program_builder&& add_attr_binding(
        const gl_attr_binding& attr_binding) &&;

    [[nodiscard]] GLuint compile_and_link() &&;
};

} // namespace quake
