// vr_lines.cpp -- see vr_lines.hpp.

#include "vr_lines.hpp"

#include <vector>

namespace qvr::lines
{
namespace
{

struct Line
{
    glm::vec3 a, b;
    float width;
    glm::vec4 colorA, colorB;
    bool point;
};

struct Vertex
{
    glm::vec3 pos;
    glm::vec4 color;
    glm::vec2 uv; // -1..1 across the width (and along a point), for the soft edge
};

std::vector<Line> queue;
std::vector<Vertex> vertices;

GLuint program = 0;
GLuint vbo = 0;

constexpr const char* vertexShader = R"(#version 430
layout(location = 0) uniform mat4 MVP;
layout(location = 0) in vec3 Pos;
layout(location = 1) in vec4 Color;
layout(location = 2) in vec2 UV;
out vec4 color;
out vec2 uv;
void main()
{
    color = Color;
    uv = UV;
    gl_Position = MVP * vec4(Pos, 1.0);
}
)";

constexpr const char* fragmentShader = R"(#version 430
in vec4 color;
in vec2 uv;
out vec4 result;
void main()
{
    float falloff = clamp(1.0 - dot(uv, uv), 0.0, 1.0);
    result = vec4(color.rgb, color.a * falloff);
}
)";

[[nodiscard]] GLuint compile(GLenum type, const char* source)
{
    const GLuint shader = GL_CreateShaderFunc(type);
    GL_ShaderSourceFunc(shader, 1, &source, nullptr);
    GL_CompileShaderFunc(shader);
    return shader;
}

bool ensureProgram()
{
    if(program)
    {
        return true;
    }

    const GLuint vs = compile(GL_VERTEX_SHADER, vertexShader);
    const GLuint fs = compile(GL_FRAGMENT_SHADER, fragmentShader);
    program = GL_CreateProgramFunc();
    GL_AttachShaderFunc(program, vs);
    GL_AttachShaderFunc(program, fs);
    GL_LinkProgramFunc(program);
    GL_DeleteShaderFunc(vs);
    GL_DeleteShaderFunc(fs);

    GLint ok = 0;
    GL_GetProgramivFunc(program, GL_LINK_STATUS, &ok);
    if(!ok)
    {
        Con_Warning("VR: line shader failed to link\n");
        GL_DeleteProgramFunc(program);
        program = 0;
        return false;
    }

    GL_GenBuffersFunc(1, &vbo);
    return true;
}

void quad(const glm::vec3 (&p)[4], const glm::vec4 (&c)[4], const glm::vec2 (&uv)[4])
{
    for(int i : {0, 1, 2, 0, 2, 3})
    {
        vertices.push_back({p[i], c[i], uv[i]});
    }
}

} // namespace

void line(const glm::vec3& a, const glm::vec3& b, float width, const glm::vec4& colorA, const glm::vec4& colorB)
{
    queue.push_back({a, b, width, colorA, colorB, false});
}

void point(const glm::vec3& p, float size, const glm::vec4& color)
{
    queue.push_back({p, p, size, color, color, true});
}

void drawInEye(const glm::vec3& eye)
{
    if(queue.empty() || !ensureProgram())
    {
        return;
    }

    vertices.clear();
    for(const Line& l : queue)
    {
        if(l.point)
        {
            // A disc facing the eye.
            const glm::vec3 toEye = glm::normalize(eye - l.a);
            const glm::vec3 side = glm::normalize(glm::cross(toEye, std::fabs(toEye.z) < 0.99f
                                                                        ? glm::vec3{0.f, 0.f, 1.f}
                                                                        : glm::vec3{1.f, 0.f, 0.f}));
            const glm::vec3 up = glm::cross(side, toEye);
            const float r = l.width * 0.5f;
            quad({l.a - side * r - up * r, l.a + side * r - up * r, l.a + side * r + up * r, l.a - side * r + up * r},
                {l.colorA, l.colorA, l.colorA, l.colorA}, {{-1.f, -1.f}, {1.f, -1.f}, {1.f, 1.f}, {-1.f, 1.f}});
            continue;
        }

        // A ribbon along the line, turned to face the eye.
        const glm::vec3 dir = l.b - l.a;
        glm::vec3 side = glm::cross(dir, eye - (l.a + l.b) * 0.5f);
        if(glm::length(side) < 1e-4f)
        {
            continue;
        }
        side = glm::normalize(side) * (l.width * 0.5f);
        quad({l.a - side, l.b - side, l.b + side, l.a + side}, {l.colorA, l.colorB, l.colorB, l.colorA},
            {{0.f, -1.f}, {0.f, -1.f}, {0.f, 1.f}, {0.f, 1.f}});
    }

    glm::mat4 viewProj;
    memcpy(&viewProj[0][0], r_matviewproj, sizeof(r_matviewproj));

    GL_UseProgram(program);
    GL_SetState(GLS_BLEND_ALPHA | GLS_NO_ZTEST | GLS_NO_ZWRITE | GLS_CULL_NONE | GLS_ATTRIBS(3));
    GL_UniformMatrix4fvFunc(0, 1, GL_FALSE, &viewProj[0][0]);

    GL_BindBuffer(GL_ARRAY_BUFFER, vbo);
    GL_BufferDataFunc(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)), vertices.data(),
        GL_STREAM_DRAW);
    GL_VertexAttribPointerFunc(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, pos)));
    GL_VertexAttribPointerFunc(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, color)));
    GL_VertexAttribPointerFunc(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, uv)));
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
    GL_BindBuffer(GL_ARRAY_BUFFER, 0);
}

void clear()
{
    queue.clear();
}

} // namespace qvr::lines
