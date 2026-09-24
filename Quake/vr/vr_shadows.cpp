// vr_shadows.cpp -- soft blob shadows under the player and the hands (vr_player_shadows: 0 off,
// 1 hands, 2 body, 3 both). The old engine projected the models' shadows (r_shadows), which
// Ironwail does not have; a dark disc on the floor below serves the same purpose in VR:
// judging heights when jumping or reaching down. Drawn in the scene pass, depth-tested.
// Needs the local server's world for the traces (see vr_trace).

#include "vr_backend.hpp"
#include "vr_cvars.hpp"
#include "vr_hands.hpp"
#include "vr_main.hpp"
#include "vr_trace.hpp"

#include <vector>

using namespace qvr;

namespace
{

struct Vertex
{
    glm::vec3 pos;
    glm::vec2 uv; // -1..1 across the disc
    float alpha;
};

std::vector<Vertex> vertices;
GLuint program = 0;
GLuint vbo = 0;

constexpr const char* vertexShader = R"(#version 430
layout(location = 0) uniform mat4 MVP;
layout(location = 0) in vec3 Pos;
layout(location = 1) in vec2 UV;
layout(location = 2) in float Alpha;
out vec2 uv;
out float alpha;
void main()
{
    uv = UV;
    alpha = Alpha;
    gl_Position = MVP * vec4(Pos, 1.0);
}
)";

constexpr const char* fragmentShader = R"(#version 430
in vec2 uv;
in float alpha;
out vec4 color;
void main()
{
    float falloff = clamp(1.0 - dot(uv, uv), 0.0, 1.0);
    color = vec4(0.0, 0.0, 0.0, alpha * falloff);
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
        GL_DeleteProgramFunc(program);
        program = 0;
        return false;
    }

    GL_GenBuffersFunc(1, &vbo);
    return true;
}

// A disc on the floor below `from`, fading with height (up to `range` units).
void blob(const glm::vec3& from, float radius, float range, float strength)
{
    const auto tr = worldtrace::move(from, glm::vec3{0.f}, glm::vec3{0.f}, from - glm::vec3{0.f, 0.f, range}, MOVE_NOMONSTERS);
    if(!tr || tr->fraction >= 1.f || tr->startsolid)
    {
        return;
    }

    const glm::vec3 n = worldtrace::normal(*tr);
    if(n.z < 0.7f)
    {
        return; // not a floor
    }

    const float alpha = strength * (1.f - tr->fraction);
    const glm::vec3 centre = worldtrace::endPos(*tr) + n * 0.25f;
    const glm::vec3 u = glm::normalize(glm::cross(n, glm::vec3{0.f, 1.f, 0.f} + n * 0.001f)) * radius;
    const glm::vec3 v = glm::cross(n, u);

    const Vertex c[4] = {{centre - u - v, {-1.f, -1.f}, alpha}, {centre + u - v, {1.f, -1.f}, alpha},
        {centre + u + v, {1.f, 1.f}, alpha}, {centre - u + v, {-1.f, 1.f}, alpha}};
    for(int i : {0, 1, 2, 0, 2, 3})
    {
        vertices.push_back(c[i]);
    }
}

} // namespace

// From VR_DrawSceneOpaque (vr_text3d.cpp).
void VR_DrawShadows()
{
    const int mode = static_cast<int>(vr_player_shadows.value);
    const hands::State& s = hands::current();
    if(mode <= 0 || !vrActive() || !s.valid)
    {
        return;
    }

    vertices.clear();
    if(mode == 2 || mode == 3)
    {
        blob(s.playerOrigin, 14.f, 256.f, 0.65f);
    }
    if(mode == 1 || mode == 3)
    {
        for(const glm::vec3& hand : s.pos)
        {
            blob(hand, 3.5f, 96.f, 0.7f);
        }
    }

    if(vertices.empty() || !ensureProgram())
    {
        return;
    }

    GL_UseProgram(program);
    GL_SetState(GLS_BLEND_ALPHA | GLS_NO_ZWRITE | GLS_CULL_NONE | GLS_ATTRIBS(3));
    GL_UniformMatrix4fvFunc(0, 1, GL_FALSE, r_matviewproj);

    GL_BindBuffer(GL_ARRAY_BUFFER, vbo);
    GL_BufferDataFunc(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)), vertices.data(),
        GL_STREAM_DRAW);
    GL_VertexAttribPointerFunc(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, pos)));
    GL_VertexAttribPointerFunc(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, uv)));
    GL_VertexAttribPointerFunc(2, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, alpha)));
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
    GL_BindBuffer(GL_ARRAY_BUFFER, 0);
}
