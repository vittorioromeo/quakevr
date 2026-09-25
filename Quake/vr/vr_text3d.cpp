// vr_text3d.cpp -- see vr_text3d.hpp. Layout from the old engine's R_DrawWorldText.

#include "vr_text3d.hpp"
#include "vr_color.hpp"
#include "vr_gfx.hpp"
#include "vr_engine.hpp"
#include "vr_shadows.hpp"
#include "vr_decals.hpp"
#include "vr_cvars.hpp"
#include "vr_worldtext.hpp"
#include "vr_gadget.hpp"
#include "vr_profile.hpp"

#include <algorithm>
#include <array>
#include <cmath>
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
std::vector<gfx::Vertex> backings; // the wrist log's backing (blended; drawOverlay, over the eye's image)
std::vector<gfx::Vertex> logText;  // and its text
std::vector<gfx::Vertex> glows;    // the screens' soft glows (added)
// The ammo screens as small CRTs (vr_weapon_screen_crt): each screen text's image (its face and
// text, in the order they were queued), drawn at the end of the 2D pass (renderScreens) and shown
// in the eyes the next frame through Shade::Screen, like the wrist gadget's.
constexpr int screenScale = 8;   // texels a pixel of the virtual screen (a font pixel)
constexpr int maxScreenImages = 4;
struct ScreenImage
{
    gfx::Target target;
    int width{0}, height{0}; // its virtual screen (font pixels), 0 before it is drawn
    int frame{-1};           // the host frame it was drawn in
};
std::array<ScreenImage, maxScreenImages> screenImages;

// An ammo screen's quad this frame, with its own glitch.
struct ScreenQuad
{
    std::array<gfx::Vertex, 6> vertices;
    glm::vec4 params;
    glm::vec3 size;
    gfx::Texture texture;
};
std::vector<ScreenQuad> screenQuads;
int screenCount = 0; // screen texts laid out this frame

// The ammo screens' CRT strength (vr_weapon_screen_crt).
[[nodiscard]] float screenCrt()
{
    return CLAMP(0.f, vr_weapon_screen_crt.value, 2.f);
}

// A screen text's shape: its lines' longest, and how many there are; the room round them, in font
// pixels (whole ones, so that its image's pixels fall on the font's).
struct ScreenShape
{
    int columns{0}, rows{0}, pad{0};
    [[nodiscard]] int width() const { return columns * 8 + pad * 2; }
    [[nodiscard]] int height() const { return rows * 8 + pad * 2; }
};

[[nodiscard]] int screenPad()
{
    return static_cast<int>(std::lround(8.f * 0.35f * std::max(0.f, vr_weapon_screen_padding.value)));
}

// The screens' palette (the wrist gadget's): its text, and its face behind it.
[[nodiscard]] glm::vec3 screenText()
{
    const float bright = CLAMP(0.f, vr_gadget_screen_brightness.value, 2.f);
    return glm::min(hsv(vr_gadget_screen_hue.value, 0.55f, bright), glm::vec3{1.f});
}

[[nodiscard]] glm::vec3 screenFace()
{
    const float back = CLAMP(0.f, vr_gadget_screen_background.value, 4.f);
    return hsv(vr_gadget_screen_hue.value, 0.57f, 0.12f * std::max(back, 0.2f));
}

gadget::Log wristLog;
gadget::Glow gadgetGlow;
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

void quad(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d, const glm::vec4& color,
    std::vector<gfx::Vertex>& out = panels)
{
    for(const glm::vec3* p : {&a, &b, &c, &a, &c, &d})
    {
        out.push_back({*p, {0.f, 0.f}, color});
    }
}

// The wrist gadget's log (gadget::log), facing the camera (`eye`, `right`, `up`): left-aligned
// lines on a dark, translucent backing, its bottom edge's centre just over the gadget's screen. It
// fades out as the screen turns away from the viewer (the arm lowered, or aiming).
void layoutLog(const gadget::Log& log, const glm::vec3& eye, const glm::vec3& right, const glm::vec3& up)
{
    const glm::vec3 toEye = eye - log.base;
    const float facing = glm::dot(log.normal, toEye) / std::max(glm::length(toEye), 0.01f);
    const float shown = CLAMP(0.f, (facing - 0.2f) / 0.3f, 1.f);
    if(shown <= 0.f)
    {
        return;
    }

    size_t longest = 0;
    float alpha = 0.f;
    for(size_t i = 0; i < log.lines.size(); i++)
    {
        longest = std::max(longest, log.lines[i].size());
        alpha = std::max(alpha, log.alpha[i] * shown);
    }
    if(longest == 0)
    {
        return;
    }

    const float charSize = log.charSize;
    const float lineStep = charSize * 1.25f;
    const float width = charSize * static_cast<float>(longest);
    const float height = lineStep * static_cast<float>(log.lines.size());
    const glm::vec3 bottom = log.base + up * log.lift;
    const glm::vec3 topLeft = bottom - right * (width * 0.5f) + up * height;

    const float pad = charSize * 0.5f;
    const glm::vec3 l = -right * (width * 0.5f + pad), r = right * (width * 0.5f + pad);
    const glm::vec3 b = bottom - up * pad, t = bottom + up * (height + pad);
    quad(b + l, b + r, t + r, t + l, glm::vec4{log.backColor, 0.6f * alpha}, backings);

    const glm::vec3 hInc = right * charSize;
    const glm::vec3 vInc = -up * charSize;
    for(size_t i = 0; i < log.lines.size(); i++)
    {
        const glm::vec4 color{log.color, log.alpha[i] * shown};
        glm::vec3 p = topLeft - up * (lineStep * static_cast<float>(i) + (lineStep - charSize) * 0.5f);
        for(const char c : log.lines[i])
        {
            if(c != ' ')
            {
                glyph(p, hInc, vInc, static_cast<unsigned char>(c), color, logText);
            }
            p += hInc;
        }
    }
}

// A soft glow round a screen's edge (gadget::Glow), as the bloom would give it: two rings, a
// narrow bright one and a wide faint one, each fading out from the edge (the vertex colour's alpha
// times 1 - distance^2: Shade::SoftEdge), their corners quarter discs.
void glowRing(const gadget::Glow& g, float spread, float alpha)
{
    const glm::vec3 r = g.right, u = g.up;
    const float w = g.halfSize.x, h = g.halfSize.y;
    const glm::vec4 color{glm::vec3{g.color}, alpha};
    // A quad from (x0, y0) to (x1, y1) in the screen's plane, with its corners' fading coordinates.
    const auto piece = [&](float x0, float y0, float x1, float y1, glm::vec2 f00, glm::vec2 f10, glm::vec2 f11,
                           glm::vec2 f01) {
        const gfx::Vertex a{g.centre + r * x0 + u * y0, f00, color};
        const gfx::Vertex b{g.centre + r * x1 + u * y0, f10, color};
        const gfx::Vertex c{g.centre + r * x1 + u * y1, f11, color};
        const gfx::Vertex d{g.centre + r * x0 + u * y1, f01, color};
        for(const gfx::Vertex* v : {&a, &b, &c, &a, &c, &d})
        {
            glows.push_back(*v);
        }
    };
    const float s = spread;
    const glm::vec2 o{0.f}, x{1.f, 0.f}, y{0.f, 1.f}, xy{1.f, 1.f};
    piece(w, -h, w + s, h, o, x, x, o);     // right
    piece(-w - s, -h, -w, h, x, o, o, x);   // left
    piece(-w, h, w, h + s, o, o, y, y);     // top
    piece(-w, -h - s, w, -h, y, y, o, o);   // bottom
    piece(w, h, w + s, h + s, o, x, xy, y); // corners
    piece(-w - s, h, -w, h + s, x, o, y, xy);
    piece(w, -h - s, w + s, -h, y, xy, x, o);
    piece(-w - s, -h - s, -w, -h, xy, y, o, x);
}

void glow(const gadget::Glow& g)
{
    glowRing(g, g.spread * 0.35f, g.color.a);
    glowRing(g, g.spread, g.color.a * 0.45f);
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

// Characters a line of `length` is moved in by, of the longest `longest`.
[[nodiscard]] float indent(Align align, size_t longest, size_t length)
{
    const float slack = static_cast<float>(longest - length);
    return align == Align::Left ? 0.f : align == Align::Centre ? slack * 0.5f : slack;
}

// `text`'s lines into textLines; the longest's length.
size_t splitLines(std::string_view text)
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
    return longest;
}

void layout(std::string_view text, const glm::vec3& pos, const glm::vec3& angles, Align align, float scale,
    bool screen = false)
{
    const size_t longest = splitLines(text);
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
    // side facing the viewer (right x up points at them); the gadget's palette. As a CRT
    // (vr_weapon_screen_crt), the face and the text are its image (renderScreens), drawn over the
    // face through the CRT shader, with a glitch of its own: the screens glitch at other moments.
    glm::vec4 textColor{1.f};
    bool imaged = false;
    if(screen)
    {
        const int index = screenCount++;
        const glm::vec3 n = glm::normalize(glm::cross(right, up));
        const float bright = CLAMP(0.f, vr_gadget_screen_brightness.value, 2.f);
        textColor = glm::vec4{screenText(), 1.f};

        const float halfW = charSize * static_cast<float>(longest) * 0.5f;
        const float halfH = charSize * static_cast<float>(textLines.size()) * 0.5f;
        const float pad = charSize / 8.f * static_cast<float>(screenPad());
        const float bezel = charSize * 0.3f;
        const float depth = charSize * 0.5f;
        const float gap = charSize * 0.04f;

        box(pos - n * (gap * 2.f + depth * 0.5f), right, up, n, halfW + pad + bezel, halfH + pad + bezel, depth * 0.5f,
            glm::vec3{0.13f, 0.13f, 0.14f});
        const glm::vec3 c = pos - n * gap;
        const glm::vec3 bl = c - right * (halfW + pad) - up * (halfH + pad);
        const glm::vec3 br = c + right * (halfW + pad) - up * (halfH + pad);
        const glm::vec3 tr = c + right * (halfW + pad) + up * (halfH + pad);
        const glm::vec3 tl = c - right * (halfW + pad) + up * (halfH + pad);

        const float crt = screenCrt();
        const ScreenImage* image = index < maxScreenImages ? &screenImages[static_cast<size_t>(index)] : nullptr;
        imaged = crt > 0.f && image && image->width > 0 && image->target.texture &&
                 image->frame >= host_framecount - 2; // not left over from before a pause of the 2D pass
        if(imaged)
        {
            // The image is last frame's (the 2D pass comes after the eyes), stretched over this one's
            // face should its text have changed shape.
            const glm::vec4 phosphor = textColor;
            const gfx::Vertex v[4] = {
                {bl, {0.f, 0.f}, phosphor}, {br, {1.f, 0.f}, phosphor}, {tr, {1.f, 1.f}, phosphor}, {tl, {0.f, 1.f}, phosphor}};
            const double offset = 2.9 + 3.7 * index;
            const float time = static_cast<float>(std::fmod(realtime, 1000.0));
            screenQuads.push_back({.vertices = {v[0], v[1], v[2], v[0], v[2], v[3]},
                .params = {time, crt, gadget::glitch(realtime + offset) * std::min(crt, 1.f), gadget::textGlow()},
                .size = {static_cast<float>(image->width), static_cast<float>(image->height), 1.f},
                .texture = image->target.texture});
        }
        else
        {
            quad(bl, br, tr, tl, glm::vec4{screenFace(), 1.f});
        }

        // Its glow (vr_screen_glow), over the bezel and a little beyond, just in front of the face.
        const float k = CLAMP(0.f, vr_screen_glow.value, 3.f);
        if(k > 0.f && bright > 0.f)
        {
            glow({.centre = pos - n * (gap * 0.5f), .right = right, .up = up, .halfSize = {halfW + pad, halfH + pad},
                .spread = bezel + charSize * 0.6f, .color = glm::vec4{glm::vec3{textColor}, 0.22f * k}});
        }
    }

    for(size_t i = 0; i < textLines.size() && !imaged; i++)
    {
        glm::vec3 p = topLeft + vInc * static_cast<float>(i) + hInc * indent(align, longest, textLines[i].size());
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

void drawTranslucent()
{
    if(!(cl.protocolflags & PRFL_QUAKEVR) || builtFrame != host_framecount ||
        (floating.empty() && glows.empty()))
    {
        return;
    }
    const glm::mat4 viewProjection = gfx::sceneViewProjection();
    if(!glows.empty())
    {
        gfx::draw(glows, viewProjection,
            {.shade = gfx::Shade::SoftEdge, .blend = gfx::Blend::Additive, .depthTest = true, .depthWrite = false});
    }
    if(!floating.empty())
    {
        gfx::draw(floating, viewProjection,
            {.shade = gfx::Shade::Texture, .blend = gfx::Blend::Alpha, .depthTest = true, .depthWrite = false},
            gfx::fontTexture());
    }
}

void drawOverlay()
{
    if(!(cl.protocolflags & PRFL_QUAKEVR) || builtFrame != host_framecount || logText.empty())
    {
        return;
    }
    const glm::mat4 viewProjection = gfx::sceneViewProjection();
    gfx::draw(backings, viewProjection,
        {.shade = gfx::Shade::Color, .blend = gfx::Blend::Alpha, .depthTest = false, .depthWrite = false});
    gfx::draw(logText, viewProjection,
        {.shade = gfx::Shade::Texture, .blend = gfx::Blend::Alpha, .depthTest = false, .depthWrite = false},
        gfx::fontTexture());
}

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

void renderScreens()
{
    if(screenCrt() <= 0.f)
    {
        return;
    }

    int index = 0;
    for(const Queued& q : queued)
    {
        if(!q.screen)
        {
            continue;
        }
        if(index >= maxScreenImages)
        {
            break;
        }
        ScreenImage& image = screenImages[static_cast<size_t>(index++)];
        const size_t longest = splitLines(q.text);
        if(longest == 0)
        {
            image.width = 0;
            continue;
        }

        // Its face and its text, in the screen's colours, on a virtual screen of font pixels (the
        // CRT shader makes it one phosphor colour again).
        const int pad = screenPad();
        image.width = static_cast<int>(longest) * 8 + pad * 2;
        image.height = static_cast<int>(textLines.size()) * 8 + pad * 2;
        gfx::ensureTarget(image.target, image.width * screenScale, image.height * screenScale, true); // mipmaps: the glow
        gfx::begin2D(image.target, image.width, image.height);
        gfx::draw2D::fill(0.f, 0.f, static_cast<float>(image.width), static_cast<float>(image.height), screenFace());
        gfx::draw2D::color(glm::vec4{screenText(), 1.f});
        for(size_t i = 0; i < textLines.size(); i++)
        {
            const std::string line{textLines[i]};
            const float x = static_cast<float>(pad) + 8.f * indent(q.align, longest, line.size());
            gfx::draw2D::text(x, static_cast<float>(pad + 8 * static_cast<int>(i)), 8.f, line.c_str());
        }
        gfx::draw2D::color(glm::vec4{1.f});
        gfx::end2D();
        image.frame = host_framecount;
    }
}

} // namespace qvr::text3d

extern "C" void VR_DrawSceneOpaque()
{
    QVR_GPU_PROFILE("vr opaque (text3d)");
    using namespace qvr;
    using namespace qvr::text3d;

    gadget::drawScreen(); // the wrist gadget's screen (vr_gadget.cpp)

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
        backings.clear();
        logText.clear();
        glows.clear();
        screenQuads.clear();
        screenCount = 0;
        if(gadget::screenGlow(gadgetGlow))
        {
            glow(gadgetGlow);
        }
        for(const worldtext::WorldText& wt : worldtext::clientTexts())
        {
            layout(wt.text, wt.pos, wt.angles, static_cast<Align>(wt.hAlign), wt.scale);
        }
        for(const Queued& q : queued)
        {
            layout(q.text, q.pos, q.angles, q.align, q.scale, q.screen);
        }

        // Facing the first view drawn this frame (the eyes are a few centimetres apart).
        glm::vec3 eye, right, up;
        gfx::sceneCamera(eye, right, up);
        for(const worldtext::FloatText& ft : worldtext::clientFloatTexts(cl.time))
        {
            layoutFloating(ft, cl.time, eye, right, up);
        }
        if(gadget::log(wristLog))
        {
            layoutLog(wristLog, eye, right, up);
        }
    }

    const glm::mat4 viewProjection = gfx::sceneViewProjection();
    gfx::draw(panels, viewProjection,
        {.shade = gfx::Shade::Color, .blend = gfx::Blend::Opaque, .depthTest = true, .depthWrite = true});
    gfx::draw(vertices, viewProjection,
        {.shade = gfx::Shade::TextureCutout, .blend = gfx::Blend::Opaque, .depthTest = true, .depthWrite = true},
        gfx::fontTexture());
    for(const ScreenQuad& q : screenQuads)
    {
        gfx::draw(q.vertices, viewProjection,
            {.shade = gfx::Shade::Screen, .blend = gfx::Blend::Opaque, .depthTest = true, .depthWrite = true,
                .params = q.params, .screen = q.size},
            q.texture);
    }
    // The blended ones wait for the translucent pass (drawTranslucent): drawn here, writing no
    // depth, the sky drawn next would paint over them wherever it is behind.
}
