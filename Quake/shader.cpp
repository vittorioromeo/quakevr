#include "shader.hpp"

#include <GL/glew.h>

#include <string_view>
#include <filesystem>
#include <cassert>
#include <vector>
#include <cstdio>

namespace quake
{

namespace
{

[[nodiscard, maybe_unused]] constexpr bool is_valid_shader_type(
    const GLenum shader_type) noexcept
{
    return shader_type == GL_COMPUTE_SHADER ||
           shader_type == GL_VERTEX_SHADER ||
           shader_type == GL_TESS_CONTROL_SHADER ||
           shader_type == GL_TESS_EVALUATION_SHADER ||
           shader_type == GL_GEOMETRY_SHADER ||
           shader_type == GL_FRAGMENT_SHADER;
}

[[nodiscard]] constexpr const char* shader_type_to_str(
    const GLenum shader_type) noexcept
{
    if(shader_type == GL_COMPUTE_SHADER)
    {
        return "GL_COMPUTE_SHADER";
    }

    if(shader_type == GL_VERTEX_SHADER)
    {
        return "GL_VERTEX_SHADER";
    }

    if(shader_type == GL_TESS_CONTROL_SHADER)
    {
        return "GL_TESS_CONTROL_SHADER";
    }

    if(shader_type == GL_TESS_EVALUATION_SHADER)
    {
        return "GL_TESS_EVALUATION_SHADER";
    }

    if(shader_type == GL_GEOMETRY_SHADER)
    {
        return "GL_GEOMETRY_SHADER";
    }

    assert(shader_type == GL_FRAGMENT_SHADER);
    return "GL_FRAGMENT_SHADER";
}

void impl_gl_shader_source(
    const GLuint shader_id, const std::string_view src) noexcept
{
    const char* data{src.data()};
    const GLint length{static_cast<GLint>(src.size())};

    glShaderSource(shader_id, 1, &data, &length);
}

[[nodiscard]] GLuint impl_make_gl_shader(
    const GLenum shader_type, const std::string_view src) noexcept
{
    assert(is_valid_shader_type(shader_type));

    const GLuint shader_id{glCreateShader(shader_type)};
    assert(shader_id != 0);

    impl_gl_shader_source(shader_id, src);
    glCompileShader(shader_id);

    GLint result_code{GL_FALSE};
    glGetShaderiv(shader_id, GL_COMPILE_STATUS, &result_code);

    GLint info_log_length;
    glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &info_log_length);

    if(info_log_length > 0)
    {
        std::vector<GLchar> buffer(info_log_length + 1);

        glGetShaderInfoLog(shader_id, info_log_length, nullptr, buffer.data());

        std::printf("Failure compiling '%s' with id '%d':\n%s\n",
            shader_type_to_str(shader_type), shader_id, buffer.data());
    }

    assert(result_code == GL_TRUE);
    return shader_id;
}

} // namespace

[[nodiscard]] GLuint make_gl_program(const std::string_view src_vertex,
    const std::string_view src_geometry,
    const std::string_view src_fragment) noexcept
{
    return gl_program_builder{}
        .add_shader(gl_shader_src{GL_VERTEX_SHADER, src_vertex})
        .add_shader(gl_shader_src{GL_GEOMETRY_SHADER, src_geometry})
        .add_shader(gl_shader_src{GL_FRAGMENT_SHADER, src_fragment})
        .compile_and_link();
}

[[nodiscard]] GLuint make_gl_program(const std::string_view src_vertex,
    const std::string_view src_fragment) noexcept
{
    return gl_program_builder{}
        .add_shader(gl_shader_src{GL_VERTEX_SHADER, src_vertex})
        .add_shader(gl_shader_src{GL_FRAGMENT_SHADER, src_fragment})
        .compile_and_link();
}

[[nodiscard]] gl_program_builder&& gl_program_builder::add_shader(
    const gl_shader_src& shader_src) &&
{
    _shader_srcs.emplace_back(shader_src);
    return std::move(*this);
}

[[nodiscard]] gl_program_builder&& gl_program_builder::add_attr_binding(
    const gl_attr_binding& attr_binding) &&
{
    _attr_bindings.emplace_back(attr_binding);
    return std::move(*this);
}

[[nodiscard]] GLuint gl_program_builder::compile_and_link() &&
{
    // Compile shaders
    std::vector<GLuint> shader_ids;
    for(const auto& [shader_type, shader_src] : _shader_srcs)
    {
        shader_ids.emplace_back(impl_make_gl_shader(shader_type, shader_src));
    }

    // Create program
    const GLuint program_id{glCreateProgram()};
    assert(program_id != 0);

    // Attach shaders
    for(const GLuint sid : shader_ids)
    {
        glAttachShader(program_id, sid);
    }

    // Bind attribute locations
    for(const auto& [name, attr_index] : _attr_bindings)
    {
        glBindAttribLocation(program_id, attr_index, name.data());
    }

    // Link program
    glLinkProgram(program_id);

    // Check program
    GLint result_code{GL_FALSE};
    glGetProgramiv(program_id, GL_LINK_STATUS, &result_code);

    GLint info_log_length;
    glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &info_log_length);

    if(info_log_length > 0)
    {
        std::vector<GLchar> buffer(info_log_length + 1);

        glGetProgramInfoLog(
            program_id, info_log_length, nullptr, buffer.data());

        std::printf("Failure compiling program with id '%d':\n%s\n", program_id,
            buffer.data());
    }

    assert(result_code == GL_TRUE);

    for(const GLuint sid : shader_ids)
    {
        glDetachShader(program_id, sid);
    }

    for(const GLuint sid : shader_ids)
    {
        glDeleteShader(sid);
    }

    return program_id;
}

} // namespace quake
