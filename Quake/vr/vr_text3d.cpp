// vr_text3d.cpp -- see vr_text3d.hpp. Layout from the old engine's R_DrawWorldText.

#include "vr_text3d.hpp"
#include "vr_color.hpp"
#include "vr_gfx.hpp"
#include "vr_engine.hpp"
#include "vr_shadows.hpp"
#include "vr_decals.hpp"
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
    bool screen;
};

std::vector<Queued> queued;
std::vector<gfx::Vertex> vertices; // glyphs
std::vector<gfx::Vertex> panels;   // screens behind them
std::vector<gfx::Vertex> floating; // floating texts (blended: they fade)
int builtFrame = -1; // the host frame they were laid out in; -1 when texts were queued since
std::vector<std::string_view> textLines; // layout()'s, kept between calls

void glyph(const glm::vec3& topLeft, const glm::vec3& right, const glm::vec3& down, unsigned char c, const glm::vec4& color,
    std::vector<gfx::Vertex>& out = vertices)
{
    const glm::vec4 uv = gfx::fontGlyph(c);
    const gfx::Vertex tl{topLeft, {uv.x, uv.y}, color};
    const gfx::Vertex tr{topLeft + right, {uv.z, uv.y}, color};
    const gfx::Vertex br{topLeft + right + down, {uv.z, uv.w}, color};
    const gfx::Vertex bl{topLeft + down, {uv.x, uv.w}, color};
    for(const gfx::Vertex& v : {tl, tr, br, tl, br, bl})
    {
        out.push_back(v);
    }
}

// A floating text (worldtext::FloatText) at client time `now`, facing the camera (`eye`, `right`,
// `up`): it pops in a little large, rises 20 units, easing out, and fades over its last 40%. A dark
// copy just behind it keeps it readable against anything. Pulled towards the viewer a little, so
// that it is not hidden inside what was hit.
void layoutFloating(const worldtext::FloatText& ft, double now, const glm::vec3& eye, const glm::vec3& right,
    const glm::vec3& up)
{
    if(ft.text.empty())
    {
        return;
    }

    const float t = CLAMP(0.f, static_cast<float>((now - ft.start) / worldtext::floatTextLife), 1.f);
    const float rise = 1.f - (1.f - t) * (1.f - t);
    const float alpha = t < 0.6f ? 1.f : 1.f - (t - 0.6f) / 0.4f;
    const float pop = 1.f + 0.35f * std::max(0.f, 1.f - t / 0.12f);

    glm::vec3 pos = ft.pos + glm::vec3{0.f, 0.f, 20.f * rise};
    glm::vec3 toEye = eye - pos;
    const float dist = glm::length(toEye);
    toEye = dist > 0.01f ? toEye / dist : glm::vec3{0.f};
    pos += toEye * std::min(10.f, dist * 0.5f);

    const float charSize = 8.f * ft.scale * pop;
    const glm::vec3 hInc = right * charSize;
    const glm::vec3 vInc = -up * charSize;
    const glm::vec3 topLeft = pos - (hInc * static_cast<float>(ft.text.size()) + vInc) * 0.5f;
    const glm::vec3 shadow = (right - up) * (charSize * 0.1f) - toEye * 0.3f;

    for(const bool dark : {true, false})
    {
        const glm::vec4 color = dark ? glm::vec4{0.f, 0.f, 0.f, alpha * 0.8f} : glm::vec4{ft.color, alpha};
        glm::vec3 p = topLeft + (dark ? shadow : glm::vec3{0.f});
        for(const char c : ft.text)
        {
            if(c != ' ')
            {
                glyph(p, hInc, vInc, static_cast<unsigned char>(c), color, floating);
            }
            p += hInc;
        }
    }
}

void quad(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d, const glm::vec4& color)
{
    for(const glm::vec3* p : {&a, &b, &c, &a, &c, &d})
    {
        panels.push_back({*p, {0.f, 0.f}, color});
    }
}

// A box centred on `c`, half extents `hr` along `right`, `hu` along `up`, `hn` along `n`; its
// faces shaded a little by which way they face, so that it reads as a solid.
void box(const glm::vec3& c, const glm::vec3& right, const glm::vec3& up, const glm::vec3& n, float hr, float hu, float hn,
    const glm::vec3& color)
{
    const glm::vec3 r = right * hr, u = up * hu, f = n * hn;
    const auto shade = [&](float k) { return glm::vec4{color * k, 1.f}; };
    quad(c + f - r - u, c + f + r - u, c + f + r + u, c + f - r + u, shade(1.f));   // front
    quad(c - f - r - u, c - f - r + u, c - f + r + u, c - f + r - u, shade(0.6f));  // back
    quad(c - r - u - f, c - r - u + f, c - r + u + f, c - r + u - f, shade(0.75f)); // left
    quad(c + r - u - f, c + r + u - f, c + r + u + f, c + r - u + f, shade(0.75f)); // right
    quad(c - u - r - f, c - u + r - f, c - u + r + f, c - u - r + f, shade(0.65f)); // bottom
    quad(c + u - r - f, c + u - r + f, c + u + r + f, c + u + r - f, shade(0.9f));  // top
}

void layout(std::string_view text, const glm::vec3& pos, const glm::vec3& angles, Align align, float scale,
    bool screen = false)
{
    textLines.clear();
    for(size_t start = 0; start <= text.size();)
    {
        const size_t end = std::min(text.find('\n', start), text.size());
        textLines.push_back(text.substr(start, end - start));
        start = end + 1;
    }

    size_t longest = 0;
    for(std::string_view l : textLines)
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

    // A sign read from behind is turned round rather than mirrored (screens have a back).
    if(!screen)
    {
        glm::vec3 eye, camRight, camUp;
        gfx::sceneCamera(eye, camRight, camUp);
        if(glm::dot(eye - pos, glm::cross(right, up)) < 0.f)
        {
            right = -right;
        }
    }

    const float charSize = 8.f * scale;
    const glm::vec3 hInc = right * charSize;
    const glm::vec3 vInc = -up * charSize;

    const glm::vec3 topLeft =
        pos - (hInc * static_cast<float>(longest) + vInc * static_cast<float>(textLines.size())) * 0.5f;

    // The screen: a bezel box behind the text and its lit face just behind the text, the readable
    // side facing the viewer (right x up points at them); the gadget's palette.
    glm::vec4 textColor{1.f};
    if(screen)
    {
        const glm::vec3 n = glm::normalize(glm::cross(right, up));
        const float hue = vr_gadget_screen_hue.value;
        const float bright = CLAMP(0.f, vr_gadget_screen_brightness.value, 2.f);
        const float back = CLAMP(0.f, vr_gadget_screen_background.value, 4.f);
        textColor = glm::vec4{glm::min(hsv(hue, 0.55f, bright), glm::vec3{1.f}), 1.f};

        const float halfW = charSize * static_cast<float>(longest) * 0.5f;
        const float halfH = charSize * static_cast<float>(textLines.size()) * 0.5f;
        const float pad = charSize * 0.35f * std::max(0.f, vr_weapon_screen_padding.value);
        const float bezel = charSize * 0.3f;
        const float depth = charSize * 0.5f;
        const float gap = charSize * 0.04f;

        box(pos - n * (gap * 2.f + depth * 0.5f), right, up, n, halfW + pad + bezel, halfH + pad + bezel, depth * 0.5f,
            glm::vec3{0.13f, 0.13f, 0.14f});
        const glm::vec4 face{hsv(hue, 0.57f, 0.12f * std::max(back, 0.2f)), 1.f};
        const glm::vec3 c = pos - n * gap;
        quad(c - right * (halfW + pad) - up * (halfH + pad), c + right * (halfW + pad) - up * (halfH + pad),
            c + right * (halfW + pad) + up * (halfH + pad), c - right * (halfW + pad) + up * (halfH + pad), face);
    }

    for(size_t i = 0; i < textLines.size(); i++)
    {
        const float slack = static_cast<float>(longest - textLines[i].size());
        const float indent = align == Align::Left ? 0.f : align == Align::Centre ? slack * 0.5f : slack;

        glm::vec3 p = topLeft + vInc * static_cast<float>(i) + hInc * indent;
        for(char c : textLines[i])
        {
            if(c != ' ')
            {
                glyph(p, hInc, vInc, static_cast<unsigned char>(c), textColor);
            }
            p += hInc;
        }
    }
}

} // namespace

void queue(std::string_view text, const glm::vec3& pos, const glm::vec3& angles, Align align, float scale, bool screen)
{
    queued.push_back({std::string{text}, pos, angles, align, scale, screen});
    builtFrame = -1;
}

void clear()
{
    queued.clear();
    builtFrame = -1;
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

    decals::draw();
    shadows::draw();

    // Laid out once a frame, for both eyes.
    if(builtFrame != host_framecount)
    {
        builtFrame = host_framecount;
        vertices.clear();
        panels.clear();
        floating.clear();
        for(const worldtext::WorldText& wt : worldtext::clientTexts())
        {
            layout(wt.text, wt.pos, wt.angles, static_cast<Align>(wt.hAlign), wt.scale);
        }
        for(const Queued& q : queued)
        {
            layout(q.text, q.pos, q.angles, q.align, q.scale, q.screen);
        }

        // Facing the first view drawn this frame (the eyes are a few centimetres apart).
        const std::vector<worldtext::FloatText>& floatTexts = worldtext::clientFloatTexts(cl.time);
        if(!floatTexts.empty())
        {
            glm::vec3 eye, right, up;
            gfx::sceneCamera(eye, right, up);
            for(const worldtext::FloatText& ft : floatTexts)
            {
                layoutFloating(ft, cl.time, eye, right, up);
            }
        }
    }

    const glm::mat4 viewProjection = gfx::sceneViewProjection();
    gfx::draw(panels, viewProjection,
        {.shade = gfx::Shade::Color, .blend = gfx::Blend::Opaque, .depthTest = true, .depthWrite = true});
    gfx::draw(vertices, viewProjection,
        {.shade = gfx::Shade::TextureCutout, .blend = gfx::Blend::Opaque, .depthTest = true, .depthWrite = true},
        gfx::fontTexture());
    if(!floating.empty())
    {
        gfx::draw(floating, viewProjection,
            {.shade = gfx::Shade::Texture, .blend = gfx::Blend::Alpha, .depthTest = true, .depthWrite = false},
            gfx::fontTexture());
    }
}
