// vr_menupaint.hpp -- flat shapes for the VR menus (the VR menu style's widgets, the corner's buttons, the search page):
// Draw_FillEx in menu coordinates, rows as the menu spaces them, and the style's colours (vr_menuui.cpp, vr_menu.cpp).

#pragma once

#include "vr_engine.hpp"

#include "Zancle/Math/Fmax.hpp"
#include "Zancle/Math/Fmin.hpp"
#include "Zancle/Math/Sqrt.hpp"

#include <glm/glm.hpp>

namespace qvr::menupaint
{

namespace colors
{
constexpr glm::vec4 track{0.16f, 0.13f, 0.10f, 0.95f};
constexpr glm::vec4 fill{0.86f, 0.55f, 0.18f, 1.f};
constexpr glm::vec4 thumbRing{0.55f, 0.32f, 0.10f, 1.f};
constexpr glm::vec4 thumb{1.f, 0.90f, 0.70f, 1.f};
constexpr glm::vec4 marker{0.75f, 0.90f, 1.f, 0.9f};
constexpr glm::vec4 thumbRingPast{0.20f, 0.45f, 0.72f, 1.f}; // a value past the bar's end
constexpr glm::vec4 thumbPast{0.78f, 0.92f, 1.f, 1.f};
constexpr glm::vec4 switchOff{0.30f, 0.27f, 0.24f, 1.f};
constexpr glm::vec4 knobOff{0.62f, 0.58f, 0.52f, 1.f};
constexpr glm::vec4 highlight{1.f, 0.72f, 0.35f, 0.14f};
constexpr glm::vec4 highlightEdge{1.f, 0.70f, 0.30f, 0.9f};
constexpr glm::vec4 listHover{1.f, 0.72f, 0.35f, 0.28f}; // a drop-down list's highlighted choice
constexpr glm::vec4 boxBorder{0.60f, 0.40f, 0.18f, 1.f};
constexpr glm::vec4 boxFill{0.07f, 0.055f, 0.04f, 0.92f};
constexpr glm::vec4 scrollThumb{0.86f, 0.55f, 0.18f, 0.9f};
constexpr glm::vec4 buttonHover{0.45f, 0.26f, 0.08f, 0.95f};
constexpr glm::vec4 recording{0.90f, 0.15f, 0.10f, 1.f}; // the spectator camera's reminder
} // namespace colors

// Draws flat shapes with Draw_FillEx. Across, menu pixels; up and down, "true" menu pixels (as
// wide as they are across) from a line of the menu's own (spaced) coordinates: a row's middle is
// its y + 4. Colours are tinted by the canvas colour the menu set (fades, dimmed items).
struct Painter
{
    float k{1.f};    // the canvas's row spacing
    float step{1.f}; // a canvas unit in menu pixels (a quarter of one at least): curves' steps
    glm::vec4 tint{1.f};

    Painter()
    {
        const drawtransform_t& t = glcanvas.transform;
        k = za::fmax(1.f, -t.scale[1] * vid.guiheight / (t.scale[0] * vid.guiwidth));
        step = za::fmax(2.f / (t.scale[0] * vid.guiwidth), 0.25f);
        const uint32_t c = glcanvas.colorstack[glcanvas.colorstacktop];
        tint = glm::vec4{c & 0xff, (c >> 8) & 0xff, (c >> 16) & 0xff, (c >> 24) & 0xff} / 255.f;
    }

    // x0..x1 across, from `top` to `bottom` true pixels below the line `y`.
    void band(float x0, float x1, float y, float top, float bottom, const glm::vec4& color) const
    {
        if(x1 <= x0 || bottom <= top)
        {
            return;
        }
        const glm::vec4 c = color * tint;
        const float rgb[3]{c.r, c.g, c.b};
        Draw_FillEx(x0, y + top / k, x1 - x0, (bottom - top) / k, rgb, c.a);
    }

    void rect(float x0, float x1, float yc, float half, const glm::vec4& color) const
    {
        band(x0, x1, yc, -half, half, color);
    }

    // Rounded corners of radius `r`, drawn a canvas unit high at a time.
    void rounded(float x0, float x1, float yc, float half, float r, const glm::vec4& color) const
    {
        r = za::fmin(r, za::fmin(half, (x1 - x0) * 0.5f));
        const float straight = half - r;
        rect(x0, x1, yc, straight, color);
        for(float t = straight; t < half; t += step)
        {
            const float t1 = za::fmin(t + step, half);
            const float e = (t + t1) * 0.5f - straight;
            const float inset = r - za::sqrt(za::fmax(0.f, r * r - e * e));
            band(x0 + inset, x1 - inset, yc, -t1, -t, color);
            band(x0 + inset, x1 - inset, yc, t, t1, color);
        }
    }

    void disc(float xc, float yc, float r, const glm::vec4& color) const
    {
        rounded(xc - r, xc + r, yc, r, r, color);
    }

    // A triangle pointing left, its tip at x, `w` wide and `half` high each way.
    void arrowHead(float x, float w, float yc, float half, const glm::vec4& color) const
    {
        for(float t = 0.f; t < half; t += step)
        {
            const float t1 = za::fmin(t + step, half);
            const float inset = (t + t1) * 0.5f * w / half;
            band(x + inset, x + w, yc, -t1, -t, color);
            band(x + inset, x + w, yc, t, t1, color);
        }
    }

    // The same pointing right, its tip at x.
    void arrowHeadRight(float x, float w, float yc, float half, const glm::vec4& color) const
    {
        for(float t = 0.f; t < half; t += step)
        {
            const float t1 = za::fmin(t + step, half);
            const float inset = (t + t1) * 0.5f * w / half;
            band(x - w, x - inset, yc, -t1, -t, color);
            band(x - w, x - inset, yc, t, t1, color);
        }
    }
};

} // namespace qvr::menupaint
