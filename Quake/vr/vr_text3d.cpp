// vr_text3d.cpp -- see vr_text3d.hpp. Layout from the old engine's R_DrawWorldText.

#include "vr_text3d.hpp"
#include "vr_gfx.hpp"
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

std::vector<Queued> queued;
std::vector<gfx::Vertex> vertices;

void glyph(const glm::vec3& topLeft, const glm::vec3& right, const glm::vec3& down, unsigned char c)
{
    const glm::vec4 uv = gfx::fontGlyph(c);
    const gfx::Vertex tl{topLeft, {uv.x, uv.y}};
    const gfx::Vertex tr{topLeft + right, {uv.z, uv.y}};
    const gfx::Vertex br{topLeft + right + down, {uv.z, uv.w}};
    const gfx::Vertex bl{topLeft + down, {uv.x, uv.w}};
    for(const gfx::Vertex& v : {tl, tr, br, tl, br, bl})
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

    if(vertices.empty())
    {
        return;
    }

    gfx::draw(vertices, gfx::sceneViewProjection(),
        {.shade = gfx::Shade::TextureCutout, .blend = gfx::Blend::Opaque, .depthTest = true, .depthWrite = true},
        gfx::fontTexture());
}
