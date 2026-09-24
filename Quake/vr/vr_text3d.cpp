// vr_text3d.cpp -- see vr_text3d.hpp. Layout from the old engine's R_DrawWorldText.

#include "vr_text3d.hpp"
#include "vr_engine.hpp"
#include "vr_shadows.hpp"
#include "vr_cvars.hpp"
#include "vr_worldtext.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace qvr::text3d
{
namespace
{

struct Queued
{
    std::string text;
    glm::vec3 pos;
    glm::vec3 angles;
    Align align;
    float scale;
};

struct Vertex
{
    glm::vec3 pos;
    glm::vec2 uv;
};

std::vector<Queued> queued;
std::vector<Vertex> vertices;

GLuint program = 0;
GLuint vbo = 0;

constexpr const char* vertexShader = R"(#version 430
layout(location = 0) uniform mat4 MVP;
layout(location = 0) in vec3 Pos;
layout(location = 1) in vec2 UV;
out vec2 uv;
void main()
{
    uv = UV;
    gl_Position = MVP * vec4(Pos, 1.0);
}
)";

constexpr const char* fragmentShader = R"(#version 430
layout(binding = 0) uniform sampler2D Font;
in vec2 uv;
out vec4 color;
void main()
{
    vec4 c = texture(Font, uv);
    if(c.a < 0.666)
        discard;
    color = vec4(c.rgb, 1.0);
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
        Con_Warning("VR: text shader failed to link\n");
        GL_DeleteProgramFunc(program);
        program = 0;
        return false;
    }

    GL_GenBuffersFunc(1, &vbo);
    return true;
}

// Ironwail's console font atlas: 16x16 cells of 10x10 texels, each an 8x8 glyph with a
// one-texel border.
void glyph(const glm::vec3& topLeft, const glm::vec3& right, const glm::vec3& down, unsigned char c)
{
    constexpr float atlas = 160.f;
    const float u0 = ((c & 15) * 10 + 1) / atlas;
    const float v0 = ((c >> 4) * 10 + 1) / atlas;
    const float u1 = u0 + 8.f / atlas;
    const float v1 = v0 + 8.f / atlas;

    const Vertex tl{topLeft, {u0, v0}};
    const Vertex tr{topLeft + right, {u1, v0}};
    const Vertex br{topLeft + right + down, {u1, v1}};
    const Vertex bl{topLeft + down, {u0, v1}};
    for(const Vertex& v : {tl, tr, br, tl, br, bl})
    {
        vertices.push_back(v);
    }
}

void layout(std::string_view text, const glm::vec3& pos, const glm::vec3& angles, Align align, float scale)
{
    std::vector<std::string_view> lines;
    for(size_t start = 0; start <= text.size();)
    {
        const size_t end = std::min(text.find('\n', start), text.size());
        lines.push_back(text.substr(start, end - start));
        start = end + 1;
    }

    size_t longest = 0;
    for(std::string_view l : lines)
    {
        longest = std::max(longest, l.size());
    }
    if(longest == 0)
    {
        return;
    }

    glm::vec3 right, up;
    vec3_t a{angles.x, angles.y, angles.z}, f, r, u;
    AngleVectors(a, f, r, u);
    right = {r[0], r[1], r[2]};
    up = {u[0], u[1], u[2]};

    const float charSize = 8.f * scale;
    const glm::vec3 hInc = right * charSize;
    const glm::vec3 vInc = -up * charSize;

    const glm::vec3 topLeft = pos - (hInc * static_cast<float>(longest) + vInc * static_cast<float>(lines.size())) * 0.5f;

    for(size_t i = 0; i < lines.size(); i++)
    {
        const float slack = static_cast<float>(longest - lines[i].size());
        const float indent = align == Align::Left ? 0.f : align == Align::Centre ? slack * 0.5f : slack;

        glm::vec3 p = topLeft + vInc * static_cast<float>(i) + hInc * indent;
        for(char c : lines[i])
        {
            if(c != ' ')
            {
                glyph(p, hInc, vInc, static_cast<unsigned char>(c));
            }
            p += hInc;
        }
    }
}

} // namespace

void queue(std::string_view text, const glm::vec3& pos, const glm::vec3& angles, Align align, float scale)
{
    queued.push_back({std::string{text}, pos, angles, align, scale});
}

void clear()
{
    queued.clear();
}

} // namespace qvr::text3d


extern "C" void VR_DrawSceneOpaque()
{
    using namespace qvr;
    using namespace qvr::text3d;

    if(!(cl.protocolflags & PRFL_QUAKEVR))
    {
        return;
    }

    shadows::draw();

    vertices.clear();
    for(const worldtext::WorldText& wt : worldtext::clientTexts())
    {
        layout(wt.text, wt.pos, wt.angles, static_cast<Align>(wt.hAlign), wt.scale);
    }
    for(const Queued& q : queued)
    {
        layout(q.text, q.pos, q.angles, q.align, q.scale);
    }

    if(vertices.empty() || !ensureProgram())
    {
        return;
    }

    GL_UseProgram(program);
    GL_SetState(GLS_BLEND_OPAQUE | GLS_CULL_NONE | GLS_ATTRIBS(2));
    GL_UniformMatrix4fvFunc(0, 1, GL_FALSE, r_matviewproj);
    GL_Bind(GL_TEXTURE0, char_texture);

    GL_BindBuffer(GL_ARRAY_BUFFER, vbo);
    GL_BufferDataFunc(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)), vertices.data(),
        GL_STREAM_DRAW);
    GL_VertexAttribPointerFunc(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, pos)));
    GL_VertexAttribPointerFunc(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, uv)));
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
    GL_BindBuffer(GL_ARRAY_BUFFER, 0);
}
