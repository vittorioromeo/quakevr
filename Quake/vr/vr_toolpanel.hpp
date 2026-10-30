#pragma once

// vr_toolpanel.hpp -- what the in-world tool panels share (docs/vr-port/CODE_STYLE.md, "Canonical helpers"):
//
//   pushed buttons   a panel of buttons in the world (Holster Setup, Edit Blade Shape, Pose Studio): a button's middle
//                    on its grid (buttonCentre) and whether a hand point presses it (pressedBy: 1 cm of slack round it,
//                    `depth` in front and behind);
//   undo             a bounded undo stack: the oldest snapshot dropped when it is full (pushUndo);
//   say              a tool's feedback line, kept on its panel a while and echoed to the console;
//   a panel image    drawn in Quake's 2D pass in its own virtual pixels (the toolgun's panel, the arena's perk cards):
//                    Rect, fill, text (Quake's font, gold or white), textFit (centred, shrunk to a width).
//
// Each tool keeps its own constants (button sizes, how long a line shows, its console prefix) and passes them in.

#include "vr_units.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/String/String.hpp"

#include <glm/glm.hpp>

namespace qvr::toolpanel
{

// A panel's place in the world: its middle and unit right, up and normal (Quake units).
struct Frame
{
    const glm::vec3& at;
    const glm::vec3& right;
    const glm::vec3& up;
    const glm::vec3& normal;
};

// A grid of same-sized buttons (m): their size, the gap between them, how far in front or behind a press reaches.
struct Grid
{
    float width;
    float height;
    float gap;
    float depth;
};

// The middle of the button `col` columns right and `row` rows up of the panel's middle (rows and columns may be halves).
[[nodiscard]] inline glm::vec3 buttonCentre(const Frame& f, const Grid& g, float col, float row)
{
    const float m2u = units::metresToUnits();
    return f.at + f.right * (col * (g.width + g.gap) * m2u) + f.up * (row * (g.height + g.gap) * m2u);
}

// Whether the point `p` (a hand) presses the button whose middle is `centre`.
[[nodiscard]] inline bool pressedBy(const Frame& f, const Grid& g, const glm::vec3& centre, const glm::vec3& p)
{
    const float m2u = units::metresToUnits();
    const glm::vec3 d = p - centre;
    return za::fabs(glm::dot(d, f.right)) <= (g.width * 0.5f + 0.01f) * m2u &&
           za::fabs(glm::dot(d, f.up)) <= (g.height * 0.5f + 0.01f) * m2u &&
           za::fabs(glm::dot(d, f.normal)) <= g.depth * m2u;
}

// The first of `count` buttons that `p` presses (-1: none); `centre(b)` its middle, `shown(b)` whether it is there.
template <typename CentreFn, typename ShownFn>
[[nodiscard]] int buttonUnder(const Frame& f, const Grid& g, int count, const glm::vec3& p, CentreFn&& centre, ShownFn&& shown)
{
    for(int b = 0; b < count; b++)
    {
        if(shown(b) && pressedBy(f, g, centre(b), p))
        {
            return b;
        }
    }
    return -1;
}

// `item` pushed on a bounded undo stack: when it holds `max` already, its oldest is dropped first.
template <typename T>
void pushUndo(za::Vector<T>& undo, int max, T&& item)
{
    if(static_cast<int>(undo.size()) >= max)
    {
        for(int i = 0; i + 1 < static_cast<int>(undo.size()); i++)
        {
            undo[static_cast<za::SizeT>(i)] = static_cast<T&&>(undo[static_cast<za::SizeT>(i + 1)]);
        }
        undo.popBack();
    }
    undo.pushBack(static_cast<T&&>(item));
}

// A tool's feedback line: shown on its panel for `seconds` (real time) and printed as "<tool>: <text>".
void say(za::String& line, double& until, const char* tool, const za::String& text, double seconds);

// ---------------------------------------------------------------------------------------------------------------------
// A panel image's 2D drawing (Quake's 2D pass, the panel's virtual pixels).

struct Rect
{
    float x0{0.f}, y0{0.f}, x1{0.f}, y1{0.f};
    [[nodiscard]] bool contains(const glm::vec2& p) const { return p.x >= x0 && p.x < x1 && p.y >= y0 && p.y < y1; }
    [[nodiscard]] float cx() const { return (x0 + x1) * 0.5f; }
    [[nodiscard]] float cy() const { return (y0 + y1) * 0.5f; }
    [[nodiscard]] float w() const { return x1 - x0; }
    [[nodiscard]] float h() const { return y1 - y0; }
};

void fill(const Rect& r, const glm::vec4& c);

// Quake's font, `dim` pixels a character, gold (the menus' labels) or white (at most 95 characters).
void text(float x, float y, float dim, const char* s, bool gold);

// Centred on cx, as large as `dim` or smaller to fit `width`.
void textFit(float cx, float y, float width, float dim, const char* s, bool gold);

} // namespace qvr::toolpanel
