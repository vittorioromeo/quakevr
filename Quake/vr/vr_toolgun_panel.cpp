// vr_toolgun_panel.cpp -- the toolgun's panel (vr_toolgun.hpp, toolgun::panel; docs/vr-port/TOOLGUN.md): a small panel
// of its own over the gun (vr_toolgun_panel_size wide, placed as vr_toolgun_menu_up, _forward and _tilt say), opened and
// closed by the gun's B/Y, pointed at with the other hand's thin laser (vr_toolgun_laser_size) and clicked with its
// trigger. The game goes on meanwhile (you can walk; the gun's trigger still uses the tool).
//
//   the tools     a row of tiles, each with its icon (a tap switches; the gun's stick press does too), and More;
//   the tool's    only its own choices: Spawn the spawn list as a grid of thumbnails (vr_toolgun_icons.cpp) in tabs
//                 (Monsters, Props, Weapons, Items), a page at a time, and Pick, Reset, Clear (Monsters' Armed Humanoid
//                 tile: its page of dropdowns, the kind and each hand's weapon: armedPage); Joint its joints as tiles;
//                 Physgun and Scale their switches; Remove a word on how;
//   More          the cheats (Debug > Cheats and Recording's rows: vr_menu_toolgun.inc) and the panel's size.
//
// Laid out each frame by one function (layout) twice, in an immediate mode: in the host frame (Mode::Input: where the
// laser meets it lights a widget, a press there clicks it, a slider follows the held trigger) and in the 2D pass's end
// (Mode::Draw: its image, drawn with the VR menus' widgets and colours: vr_menuui.cpp, vr_menupaint.hpp, Quake's font).
// Its image is a target of its own (256 x 192 virtual pixels, three texels each, mipmapped), shown in each eye on a quad
// over the gun (not depth tested, as the menus). Tests: vr_toolgun_mock_laser (the laser on a spot or on a widget by its
// name), vr_toolgun_panel_dump (the widgets, where the laser is).

#include "vr_toolgun.hpp"

#include "vr_backend.hpp"
#include "vr_box3d.hpp"
#include "vr_cvars.hpp"
#include "vr_main.hpp"
#include "vr_menu.hpp"
#include "vr_menupaint.hpp"
#include "vr_menuui.hpp"
#include "vr_renderdoc.hpp"
#include "vr_toolgun_icons.hpp"
#include "vr_toolgun_brush.hpp"
#include "vr_brushes.hpp"

#include "Zancle/Container/InPlaceVector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Round.hpp"
#include "Zancle/Math/Sin.hpp"

#include <stdio.h>
#include <string.h>

using namespace qvr;
using qvr::toolgun::Tool;

namespace
{

namespace colors = qvr::menupaint::colors;
using qvr::menupaint::Painter;

constexpr float panelWidth = 256.f; // its virtual pixels
constexpr float panelHeight = 192.f;
constexpr int texelsPerPixel = 3;
constexpr float pad = 4.f;

constexpr const char* sections[] = {"Monsters", "Props", "Weapons", "Items", "Fire"}; // (Fire: torches and flames)
constexpr int sectionCount = 5;
constexpr int gridColumns = 4;
constexpr int gridRows = 2;
constexpr int maxEntries = 256; // a tab's entries and its groups' empty cells (AD support: every installed spawn entry)
constexpr int maxPacks = 16;    // the groups a tab shows (toolgun::spawnablePack)
// // a section's (the spawn list has fewer)

constexpr const char* jointHints[] = {"Shoot a prop, then another: they stay stuck as they are.",
    "Shoot a prop, then another: the first turns about the second point.",
    "Shoot a prop, then another: it turns about the second face's normal.",
    "Shoot a prop, then another: it slides along the second face's normal.",
    "Shoot a prop, then another: never further apart than now (brown).",
    "Shoot a prop, then another: pulled back to now's distance (steel).", "Shoot a prop: it loses all its joints."};
constexpr int jointHintCount = static_cast<int>(sizeof(jointHints) / sizeof(jointHints[0]));
static_assert(jointHintCount == static_cast<int>(qvr::box3d::ToolJoint::Count) + 1, "a hint for each joint kind and Unjoin");

struct Rect
{
    float x0{0.f}, y0{0.f}, x1{0.f}, y1{0.f};
    [[nodiscard]] bool contains(const glm::vec2& p) const { return p.x >= x0 && p.x < x1 && p.y >= y0 && p.y < y1; }
    [[nodiscard]] float cx() const { return (x0 + x1) * 0.5f; }
    [[nodiscard]] float cy() const { return (y0 + y1) * 0.5f; }
    [[nodiscard]] float w() const { return x1 - x0; }
    [[nodiscard]] float h() const { return y1 - y0; }
};

// A widget laid out by the last input pass: its name and place (vr_toolgun_mock_laser <name>, vr_toolgun_panel_dump).
struct Spot
{
    char name[48]{}; // (the longest label, "Clear Props Made Since the Map Loaded", is 37)
    Rect r;
};

// A dropdown's list (dropdown, listOverlay): the one open, by its label ("": none), its page; from the pass that laid the
// dropdown out (valid until that pass's end: the list is laid out last), its rows' names and the one chosen; a row
// clicked, for the dropdown labelled `pickedFor` to return on its next pass.
struct DropList
{
    char name[48]{};
    int page{0};
    const char* const* labels{nullptr};
    int count{0};
    int chosen{-1};
    bool seen{false}; // its dropdown laid out this pass (one gone, its page left: the list closes)
    char pickedFor[48]{};
    int picked{-1};
};

// The panel's state (the main thread: the input, the host frame, the 2D pass).
struct PanelState
{
    bool open{false};
    bool more{false};            // More's page instead of the tool's
    bool armed{false};           // Spawn > Monsters' Armed Humanoid page instead of its grid (armedPage)
    DropList drop;               // a dropdown's list open over the panel
    bool overlayNow{false};      // the list's widgets being laid out (the others sleep while it is open)
    int shownTool{-1};           // the tool the last frame saw (another, from the stick press: More's page closes)
    int section{0};             // the spawn grid's tab
    int gridPage[sectionCount]{}; // and each tab's page
    int texturePage{0};          // the Brush tool's textures' page

    // The laser (the other hand's), from the host frame.
    bool hit{false};
    glm::vec2 cursor{0.f}; // where it meets the panel (its virtual pixels)
    bool held{false};      // a press on the panel held (its trigger)
    bool pressed{false};   // pressed since the last input pass
    int dragging{-1};      // the slider that press took (its widget number)
    int hovered{-1};       // the widget under the laser (its number), the last input pass
    bool mock{false};      // vr_toolgun_mock_laser: the laser on this spot whatever the hand's pose
    glm::vec2 mockSpot{0.f};

    gfx::Target target;
    za::InPlaceVector<Spot, 96> spots;
};
PanelState ps;

enum class Mode
{
    Input,
    Draw
};

struct Ui
{
    Mode mode;
    int counter{0};
    int hovered{-1};
    bool modal{false}; // a dropdown's list is open: only its own widgets wake (listOverlay)
};

[[nodiscard]] Tool currentTool()
{
    return static_cast<Tool>(za::clamp(static_cast<int>(vr_toolgun_tool.value), 0, static_cast<int>(Tool::Count) - 1));
}

[[nodiscard]] int laserHand()
{
    const int gun = toolgun::heldHand();
    return gun < 0 ? -1 : 1 - gun;
}


// The panel's quad over the gun: its corner (0, 0) (bottom left) and the axes spanning it (unit right and up times its
// size), from the tracked controller holding the gun.
[[nodiscard]] bool quadOf(const hands::State& s, glm::vec3& corner, glm::vec3& xAxis, glm::vec3& yAxis)
{
    const int gun = toolgun::heldHand();
    glm::vec3 origin, fwd, r, u;
    if(gun < 0 || !toolgun::controllerFrame(s, gun, origin, fwd, r, u))
    {
        return false;
    }
    // Over the gun, leaning back from upright by the tilt, facing you (its normal: right x up, back along the gun).
    const float tilt = glm::radians(vr_toolgun_menu_tilt.value);
    const glm::vec3 up = glm::normalize(u * za::cos(tilt) + fwd * za::sin(tilt));
    const float width = za::max(vr_toolgun_panel_size.value, 2.f);
    const float height = width * panelHeight / panelWidth;
    const glm::vec3 centre = origin + u * vr_toolgun_menu_up.value + up * (height * 0.5f) + fwd * vr_toolgun_menu_forward.value;
    corner = centre - r * (width * 0.5f) - up * (height * 0.5f);
    xAxis = r * width;
    yAxis = up * height;
    return true;
}

// Where the laser hand's ray (or the mock laser) meets the quad: the point, and the panel's pixel there.
[[nodiscard]] bool laserHit(const hands::State& s, glm::vec3& point, glm::vec2& cursor)
{
    glm::vec3 corner, xAxis, yAxis, origin, dir, r, u;
    const int hand = laserHand();
    if(hand < 0 || !quadOf(s, corner, xAxis, yAxis))
    {
        return false;
    }
    if(ps.mock)
    {
        cursor = ps.mockSpot;
        point = corner + xAxis * (cursor.x / panelWidth) + yAxis * (1.f - cursor.y / panelHeight);
        return true;
    }
    if(!toolgun::controllerFrame(s, hand, origin, dir, r, u))
    {
        return false;
    }
    const glm::vec3 normal = glm::cross(xAxis, yAxis);
    const float along = glm::dot(dir, normal);
    if(za::fabs(along) < 1e-6f)
    {
        return false;
    }
    const float t = glm::dot(corner - origin, normal) / along;
    if(t <= 0.f)
    {
        return false;
    }
    point = origin + dir * t;
    const float uu = glm::dot(point - corner, xAxis) / glm::dot(xAxis, xAxis);
    const float vv = glm::dot(point - corner, yAxis) / glm::dot(yAxis, yAxis);
    if(uu < 0.f || uu > 1.f || vv < 0.f || vv > 1.f)
    {
        return false;
    }
    cursor = glm::vec2{uu * panelWidth, (1.f - vv) * panelHeight};
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// Drawing (Mode::Draw: into the panel's image, its virtual pixels).

void fill(const Rect& r, const glm::vec4& c)
{
    const float rgb[3]{c.r, c.g, c.b};
    Draw_FillEx(r.x0, r.y0, r.w(), r.h(), rgb, c.a);
}

// Quake's font, `dim` pixels a character, gold (the menus' labels) or white.
void text(float x, float y, float dim, const char* s, bool gold)
{
    char buf[96];
    int n = 0;
    for(; s[n] && n < static_cast<int>(sizeof(buf)) - 1; n++)
    {
        buf[n] = gold ? static_cast<char>(static_cast<unsigned char>(s[n]) | 0x80) : s[n];
    }
    buf[n] = '\0';
    Draw_StringEx(x, y, dim, buf);
}

// Centred on x, as large as `dim` or smaller to fit `width`.
void textFit(float cx, float y, float width, float dim, const char* s, bool gold)
{
    const float len = static_cast<float>(strlen(s));
    const float d = len > 0.f ? za::min(dim, width / len) : dim;
    text(cx - len * d * 0.5f, y + (dim - d) * 0.5f, d, s, gold);
}

// Words wrapped to `width` from (x, y), `dim` a character, lines `dim` + 1 apart; the lines drawn.
int paragraph(float x, float y, float width, float dim, const char* s, bool gold)
{
    const int perLine = za::max(1, static_cast<int>(width / dim));
    int lines = 0;
    while(*s)
    {
        int n = static_cast<int>(strlen(s));
        if(n > perLine)
        {
            n = perLine;
            while(n > 0 && s[n] != ' ')
            {
                n--;
            }
            if(n == 0)
            {
                n = perLine;
            }
        }
        char line[96];
        q_strlcpy(line, s, za::min(static_cast<int>(sizeof(line)), n + 1));
        text(x, y + lines * (dim + 1.f), dim, line, gold);
        lines++;
        s += n;
        while(*s == ' ')
        {
            s++;
        }
    }
    return lines;
}

// A short line from (x0, y0) to (x1, y1), `w` thick (squares along it).
void line(const Painter& p, float x0, float y0, float x1, float y1, float w, const glm::vec4& c)
{
    const float len = glm::length(glm::vec2{x1 - x0, y1 - y0});
    const int steps = za::max(1, static_cast<int>(len / 0.5f));
    for(int i = 0; i <= steps; i++)
    {
        const float t = static_cast<float>(i) / steps;
        const float x = x0 + (x1 - x0) * t, y = y0 + (y1 - y0) * t;
        p.rect(x - w * 0.5f, x + w * 0.5f, y, w * 0.5f, c);
    }
}

void square(const Painter& p, float x0, float y0, float x1, float y1, float w, const glm::vec4& c)
{
    p.rect(x0, x1, y0 + w * 0.5f, w * 0.5f, c);
    p.rect(x0, x1, y1 - w * 0.5f, w * 0.5f, c);
    p.rect(x0, x0 + w, (y0 + y1) * 0.5f, (y1 - y0) * 0.5f, c);
    p.rect(x1 - w, x1, (y0 + y1) * 0.5f, (y1 - y0) * 0.5f, c);
}

[[nodiscard]] glm::vec4 toolColor(int tool)
{
    switch(static_cast<Tool>(tool))
    {
    case Tool::Spawn: return {0.3f, 0.9f, 1.f, 1.f};
    case Tool::Remove: return {1.f, 0.3f, 0.2f, 1.f};
    case Tool::Physgun: return {0.45f, 0.6f, 1.f, 1.f};
    case Tool::Scale: return {1.f, 0.85f, 0.2f, 1.f};
    case Tool::Joint: return {0.4f, 1.f, 0.4f, 1.f};
    case Tool::Brush: return {1.f, 0.6f, 0.2f, 1.f};
    default: return {0.9f, 0.85f, 0.75f, 1.f};
    }
}

// A tool's icon (the tools' count: More), about 14 pixels across round (cx, cy).
void toolIcon(int tool, float cx, float cy)
{
    const Painter p;
    const glm::vec4 c = toolColor(tool);
    const glm::vec4 back = colors::track;
    switch(static_cast<Tool>(tool))
    {
    case Tool::Spawn: // a plus
        p.rect(cx - 6.f, cx + 6.f, cy, 1.5f, c);
        p.rect(cx - 1.5f, cx + 1.5f, cy, 6.f, c);
        break;
    case Tool::Remove: // a cross
        line(p, cx - 5.f, cy - 5.f, cx + 5.f, cy + 5.f, 2.5f, c);
        line(p, cx - 5.f, cy + 5.f, cx + 5.f, cy - 5.f, 2.5f, c);
        break;
    case Tool::Physgun: // a ring round a dot, a beam into it
        p.disc(cx + 1.f, cy - 1.f, 5.5f, c);
        p.disc(cx + 1.f, cy - 1.f, 3.8f, back);
        p.disc(cx + 1.f, cy - 1.f, 1.8f, c);
        line(p, cx - 7.f, cy + 6.f, cx - 3.f, cy + 2.f, 1.5f, c);
        break;
    case Tool::Scale: // a box and a larger one, an arrow out
        square(p, cx - 6.f, cy - 6.f, cx + 6.f, cy + 6.f, 1.2f, c);
        p.rect(cx - 6.f, cx - 1.f, cy + 3.5f, 2.5f, c);
        line(p, cx - 1.f, cy + 1.f, cx + 4.f, cy - 4.f, 1.5f, c);
        p.rect(cx + 1.f, cx + 5.f, cy - 4.5f, 0.75f, c);
        p.rect(cx + 4.25f, cx + 5.f, cy - 2.5f, 2.f, c);
        break;
    case Tool::Joint: // two balls on a rod
        line(p, cx - 5.f, cy + 4.f, cx + 5.f, cy - 4.f, 1.5f, c);
        p.disc(cx - 5.f, cy + 4.f, 2.8f, c);
        p.disc(cx + 5.f, cy - 4.f, 2.8f, c);
        break;
    case Tool::Brush: // a box seen from above a corner: its front square, its top and side behind
        square(p, cx - 6.f, cy - 2.f, cx + 2.f, cy + 6.f, 1.2f, c);
        line(p, cx - 6.f, cy - 2.f, cx - 2.f, cy - 6.f, 1.2f, c);
        line(p, cx + 2.f, cy - 2.f, cx + 6.f, cy - 6.f, 1.2f, c);
        line(p, cx + 2.f, cy + 6.f, cx + 6.f, cy + 2.f, 1.2f, c);
        line(p, cx - 2.f, cy - 6.f, cx + 6.f, cy - 6.f, 1.2f, c);
        line(p, cx + 6.f, cy - 6.f, cx + 6.f, cy + 2.f, 1.2f, c);
        break;
    default: // More: three dots
        for(const float dx : {-5.f, 0.f, 5.f})
        {
            p.disc(cx + dx, cy, 1.8f, c);
        }
        break;
    }
}

// A joint's icon round (cx, cy).
void jointIcon(int joint, float cx, float cy, const glm::vec4& c)
{
    const Painter p;
    const glm::vec4 dim{c.r * 0.6f, c.g * 0.6f, c.b * 0.6f, 1.f};
    switch(joint)
    {
    case 0: // weld: two blocks stuck together, a bead between
        p.rect(cx - 8.f, cx - 1.f, cy, 4.f, dim);
        p.rect(cx + 1.f, cx + 8.f, cy, 4.f, dim);
        p.rect(cx - 1.5f, cx + 1.5f, cy, 5.f, c);
        break;
    case 1: // ball: a rod on a ball
        line(p, cx - 7.f, cy - 5.f, cx, cy + 1.f, 1.5f, dim);
        p.disc(cx, cy + 1.f, 3.5f, c);
        p.rect(cx - 6.f, cx + 6.f, cy + 6.f, 1.f, dim);
        break;
    case 2: // hinge: two leaves on a pin
        line(p, cx, cy, cx - 8.f, cy - 3.f, 2.f, dim);
        line(p, cx, cy, cx + 8.f, cy - 3.f, 2.f, dim);
        p.disc(cx, cy, 2.5f, c);
        break;
    case 3: // slider: a block on a rail
        p.rect(cx - 9.f, cx + 9.f, cy + 2.f, 0.8f, dim);
        p.rect(cx - 3.f, cx + 3.f, cy - 1.f, 3.f, c);
        p.rect(cx + 5.f, cx + 7.f, cy - 1.f, 1.f, dim);
        p.rect(cx - 7.f, cx - 5.f, cy - 1.f, 1.f, dim);
        break;
    case 4: // rope: two dots and a sagging line
        line(p, cx - 7.f, cy - 2.f, cx, cy + 3.f, 1.2f, glm::vec4{0.75f, 0.55f, 0.3f, 1.f});
        line(p, cx, cy + 3.f, cx + 7.f, cy - 2.f, 1.2f, glm::vec4{0.75f, 0.55f, 0.3f, 1.f});
        p.disc(cx - 7.f, cy - 2.f, 2.f, c);
        p.disc(cx + 7.f, cy - 2.f, 2.f, c);
        break;
    case 5: // spring: a zigzag between two dots
        for(int i = 0; i < 6; i++)
        {
            const float x0 = cx - 6.f + i * 2.f;
            line(p, x0, cy + (i % 2 ? 3.f : -3.f), x0 + 2.f, cy + (i % 2 ? -3.f : 3.f), 1.f, glm::vec4{0.75f, 0.78f, 0.82f, 1.f});
        }
        p.disc(cx - 8.f, cy, 2.f, c);
        p.disc(cx + 8.f, cy, 2.f, c);
        break;
    default: // unjoin: two dots, the line between them broken
        p.rect(cx - 6.f, cx - 2.f, cy, 0.8f, dim);
        p.rect(cx + 2.f, cx + 6.f, cy, 0.8f, dim);
        line(p, cx - 1.5f, cy - 3.f, cx + 1.5f, cy + 3.f, 1.f, glm::vec4{1.f, 0.35f, 0.25f, 1.f});
        p.disc(cx - 8.f, cy, 2.f, c);
        p.disc(cx + 8.f, cy, 2.f, c);
        break;
    }
}

// A tile or a button: the menus' button (lit: chosen), a glow under the laser.
void tile(const Rect& r, bool chosen, bool hover)
{
    menuui::paintButton(r.x0, r.x1, r.cy(), r.h() * 0.5f, chosen);
    if(hover)
    {
        const Painter p;
        p.rounded(r.x0 + 1.f, r.x1 - 1.f, r.cy(), r.h() * 0.5f - 1.f, 2.f, colors::highlight);
        p.rounded(r.x0 + 1.f, r.x1 - 1.f, r.y1 - 1.5f, 0.6f, 0.6f, colors::highlightEdge);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Widgets (both passes): each a number in the layout's order and a name.

[[nodiscard]] bool hovering(const Rect& r)
{
    return ps.hit && r.contains(ps.cursor) && (!ps.drop.name[0] || ps.overlayNow); // (under an open list: nothing)
}

// The widget's interaction: lit under the laser; in the input pass, true when a press landed on it (taken).
bool interact(Ui& ui, const Rect& r, const char* name, int& id, bool enabled = true) // AD support
{
    id = ui.counter++;
    enabled = enabled && (!ui.modal || ps.overlayNow); // (a dropdown's list open: only its own)
    const bool over = enabled && hovering(r); // AD support
    if(ui.mode != Mode::Input)
    {
        return false;
    }
    if(over)
    {
        ui.hovered = id;
    }
    if(ps.spots.size() < ps.spots.capacity())
    {
        Spot& spot = ps.spots.emplaceBack();
        q_strlcpy(spot.name, name, sizeof(spot.name));
        spot.r = r;
    }
    if(over && ps.pressed)
    {
        ps.pressed = false;
        ps.dragging = id;
        S_LocalSound("misc/menu3.wav");
        toolgun::haptic(laserHand(), 0.015f, 0.3f);
        return true;
    }
    return false;
}

// A button: its label centred (gold; white while chosen or lit).
bool button(Ui& ui, const Rect& r, const char* label, bool chosen = false, float dim = 7.f)
{
    int id = 0;
    const bool clicked = interact(ui, r, label, id);
    if(ui.mode == Mode::Draw)
    {
        const bool hover = hovering(r);
        tile(r, chosen, hover);
        textFit(r.cx(), r.cy() - dim * 0.5f, r.w() - 4.f, dim, label, !chosen && !hover);
    }
    return clicked;
}

// A switch row: its label on the left, the menus' switch on the right; a click flips the cvar.
void toggleRow(Ui& ui, const Rect& r, const char* label, cvar_t& cvar)
{
    int id = 0;
    if(interact(ui, r, label, id))
    {
        Cvar_SetValueQuick(&cvar, cvar.value != 0.f ? 0.f : 1.f);
    }
    if(ui.mode == Mode::Draw)
    {
        tile(r, false, hovering(r));
        text(r.x0 + 5.f, r.cy() - 3.5f, 7.f, label, !hovering(r));
        menuui::paintSwitch(r.x1 - 25.f, r.cy() - 4.f, cvar.value != 0.f);
    }
}

// A row of choices for `cvar` (a click sets that one's value), its label left of them.
void choiceRow(Ui& ui, const Rect& r, const char* label, cvar_t& cvar, const char* const* names, const float* values, int n)
{
    if(ui.mode == Mode::Draw)
    {
        text(r.x0 + 2.f, r.cy() - 3.f, 6.f, label, true);
    }
    const float x0 = r.x0 + 92.f;
    const float w = (r.x1 - x0 - (n - 1) * 2.f) / n;
    for(int i = 0; i < n; i++)
    {
        const Rect b{x0 + i * (w + 2.f), r.y0, x0 + i * (w + 2.f) + w, r.y1};
        char name[48];
        q_snprintf(name, sizeof(name), "%s %s", label, names[i]);
        int id = 0;
        if(interact(ui, b, name, id))
        {
            Cvar_SetValueQuick(&cvar, values[i]);
        }
        if(ui.mode == Mode::Draw)
        {
            const bool chosen = za::fabs(cvar.value - values[i]) < 1e-3f;
            tile(b, chosen, hovering(b));
            textFit(b.cx(), b.cy() - 3.f, b.w() - 3.f, 6.f, names[i], !chosen && !hovering(b));
        }
    }
}

// A slider row: its label, Quake's slider (the VR style's) from min to max in steps; held, the laser drags it.
void sliderRow(Ui& ui, const Rect& r, const char* label, cvar_t& cvar, float min, float max, float step, const char* format)
{
    const float x = r.x0 + 92.f; // the slider's 10 cells from here (its thumb x + 4 .. x + 76)
    int id = 0;
    (void)interact(ui, Rect{x - 4.f, r.y0, x + 84.f, r.y1}, label, id);
    if(ui.mode == Mode::Input && ps.held && ps.dragging == id && ps.hit)
    {
        const float t = za::clamp((ps.cursor.x - (x + 4.f)) / 72.f, 0.f, 1.f);
        const float v = za::clamp(min + za::round((max - min) * t / step) * step, min, max);
        if(v != cvar.value)
        {
            Cvar_SetValueQuick(&cvar, v);
        }
    }
    if(ui.mode == Mode::Draw)
    {
        text(r.x0 + 2.f, r.cy() - 3.f, 6.f, label, true);
        char value[16];
        q_snprintf(value, sizeof(value), format, cvar.value);
        menuui::paintSlider(static_cast<int>(x), static_cast<int>(r.cy() - 4.f), (cvar.value - min) / (max - min), value);
    }
}

// A dropdown's list: rows a column (two for a long list), a page at a time.
constexpr int listRows = 8;
constexpr float listPitch = 13.f; // a row's height and the gap under it

[[nodiscard]] int listPerPage(int n)
{
    return n > listRows ? listRows * 2 : listRows;
}

void closeList()
{
    ps.drop.name[0] = '\0';
    ps.drop.seen = false;
}

// A dropdown (a list picker; any page may use one): its label on the left, the choice's name in a box with a chevron;
// clicked, its list opens over the panel (listOverlay: laid out last, a page of rows, < > for more), a row clicked
// chooses it and closes the list, a click off the list closes it. While a list is open the panel's other widgets
// sleep. Returns the row chosen (once, the pass after its click), -1 none. `labels` must live to the layout's end;
// `enabled` false: shown dimmed, not opened.
int dropdown(Ui& ui, const Rect& r, const char* label, const char* const* labels, int n, int chosen, bool enabled = true)
{
    DropList& d = ps.drop;
    int picked = -1;
    if(d.picked >= 0 && !strcmp(d.pickedFor, label))
    {
        picked = d.picked < n ? d.picked : -1;
        d.picked = -1;
        d.pickedFor[0] = '\0';
    }
    const Rect box{r.x0 + za::min(64.f, r.w() * 0.35f), r.y0, r.x1, r.y1};
    int id = 0;
    if(interact(ui, box, label, id, enabled && n > 0))
    {
        q_strlcpy(d.name, label, sizeof(d.name));
        d.page = chosen >= 0 ? chosen / listPerPage(n) : 0;
    }
    const bool open = d.name[0] && !strcmp(d.name, label);
    if(open)
    {
        d.labels = labels;
        d.count = n;
        d.chosen = chosen;
        d.seen = enabled;
    }
    if(ui.mode == Mode::Draw)
    {
        if(!enabled)
        {
            GL_PushCanvasColor(1.f, 1.f, 1.f, 0.4f);
        }
        text(r.x0 + 2.f, r.cy() - 3.f, 6.f, label, true);
        const bool hover = enabled && hovering(box);
        tile(box, open, hover);
        const char* value = chosen >= 0 && chosen < n ? labels[chosen] : "";
        textFit((box.x0 + box.x1 - 10.f) * 0.5f, box.cy() - 3.f, box.w() - 16.f, 6.f, value, !hover && !open);
        // Its chevron (down: the list opens below).
        const Painter p;
        const float cx = box.x1 - 7.f, cy = box.cy();
        const glm::vec4 c = hover || open ? glm::vec4{1.f} : colors::boxBorder;
        line(p, cx - 3.f, cy - 1.5f, cx, cy + 1.5f, 1.f, c);
        line(p, cx, cy + 1.5f, cx + 3.f, cy - 1.5f, 1.f, c);
        if(!enabled)
        {
            GL_PopCanvasColor();
        }
    }
    return picked;
}

// The open dropdown's list over the panel (the layout's last: drawn over the rest, its rows the only widgets awake): its
// label and Close, its rows (the chosen one lit), its pages. A click off it closes it.
void listOverlay(Ui& ui)
{
    DropList& d = ps.drop;
    if(!d.name[0])
    {
        return;
    }
    if(!d.seen || !d.labels || d.count <= 0)
    {
        if(ui.mode == Mode::Input)
        {
            closeList(); // (its dropdown is gone: another page, another tool)
        }
        return;
    }
    ps.overlayNow = true;
    const Rect area{pad, 44.f, panelWidth - pad, panelHeight - pad};
    if(ui.mode == Mode::Draw)
    {
        fill(area, colors::boxBorder);
        fill(Rect{area.x0 + 1.f, area.y0 + 1.f, area.x1 - 1.f, area.y1 - 1.f}, glm::vec4{0.05f, 0.04f, 0.03f, 1.f});
        text(area.x0 + 5.f, 48.f, 7.f, d.name, true);
    }
    const char* const* labels = d.labels; // (a row's click closes the list: its rows are still drawn this pass)
    const int count = d.count;
    const int chosen = d.chosen;
    char name[48];
    q_strlcpy(name, d.name, sizeof(name));
    bool done = button(ui, Rect{area.x1 - 44.f, 46.f, area.x1 - 3.f, 57.f}, "Close", false, 6.f);
    const int per = listPerPage(count);
    const int cols = per / listRows;
    const int pages = (count + per - 1) / per;
    d.page = za::clamp(d.page, 0, pages - 1);
    const int page = d.page;
    const float cw = (area.w() - 8.f - (cols - 1) * 4.f) / cols;
    for(int k = 0; k < per; k++)
    {
        const int i = page * per + k;
        if(i >= count)
        {
            break;
        }
        const int col = k / listRows, row = k % listRows;
        const float y = 60.f + row * listPitch;
        const Rect rr{area.x0 + 4.f + col * (cw + 4.f), y, area.x0 + 4.f + col * (cw + 4.f) + cw, y + listPitch - 1.f};
        if(button(ui, rr, labels[i], i == chosen, 6.f))
        {
            ps.drop.picked = i;
            q_strlcpy(ps.drop.pickedFor, name, sizeof(ps.drop.pickedFor));
            done = true;
        }
    }
    if(pages > 1)
    {
        const float y0 = 172.f, y1 = 185.f;
        if(button(ui, Rect{area.x0 + 4.f, y0, area.x0 + 22.f, y1}, "<"))
        {
            d.page = (page + pages - 1) % pages;
        }
        if(button(ui, Rect{area.x0 + 54.f, y0, area.x0 + 72.f, y1}, ">"))
        {
            d.page = (page + 1) % pages;
        }
        if(ui.mode == Mode::Draw)
        {
            char t[16];
            q_snprintf(t, sizeof(t), "%d/%d", page + 1, pages);
            textFit(area.x0 + 38.f, (y0 + y1) * 0.5f - 3.f, 28.f, 6.f, t, false);
        }
    }
    if(ui.mode == Mode::Input && ps.pressed)
    {
        ps.pressed = false; // (a click off its rows: it closes, nothing under it clicked)
        done = true;
    }
    if(done && ui.mode == Mode::Input)
    {
        closeList();
    }
    ps.overlayNow = false;
}

// ---------------------------------------------------------------------------------------------------------------------
// The pages.

void toolRow(Ui& ui)
{
    constexpr int tiles = static_cast<int>(Tool::Count) + 1; // (and More)
    const float gap = 3.f;
    const float w = (panelWidth - pad * 2.f - gap * (tiles - 1)) / tiles;
    for(int t = 0; t < tiles; t++)
    {
        const Rect r{pad + t * (w + gap), pad, pad + t * (w + gap) + w, pad + 30.f};
        const bool more = t == static_cast<int>(Tool::Count);
        const char* name = more ? "More" : toolgun::toolName(t);
        const bool chosen = more ? ps.more : !ps.more && static_cast<int>(currentTool()) == t;
        int id = 0;
        if(interact(ui, r, name, id))
        {
            if(more)
            {
                ps.more = !ps.more;
            }
            else
            {
                ps.more = false;
                toolgun::setTool(t);
            }
        }
        if(ui.mode == Mode::Draw)
        {
            const bool hover = hovering(r);
            tile(r, chosen, hover);
            toolIcon(t, r.cx(), r.y0 + 11.f);
            textFit(r.cx(), r.y0 + 21.f, r.w() - 2.f, 6.f, name, !chosen && !hover);
        }
    }
}

// The info line under the tools: the state line's first line (the tool, its choice).
void infoLine(Ui& ui)
{
    if(ui.mode != Mode::Draw)
    {
        return;
    }
    char line[96];
    q_strlcpy(line, ps.more ? "Cheats and the panel" : toolgun::stateLine(), sizeof(line));
    if(char* nl = strchr(line, '\n'))
    {
        *nl = '\0';
    }
    textFit(panelWidth * 0.5f, 38.f, panelWidth - pad * 2.f, 6.f, line, false);
}

// A thumbnail tile: the entry's picture, its name under it (two lines if it is long).
bool thumbTile(Ui& ui, const Rect& r, int entry)
{
    char label[80]; // AD support
    q_snprintf(label, sizeof(label), "%s%s", toolgun::spawnableName(entry), toolgun::spawnableAvailable(entry) ? "" : " (unavailable)"); // AD support
    const char* name = label; // AD support
    int id = 0;
    const bool available = toolgun::spawnableAvailable(entry); // AD support
    const bool clicked = interact(ui, r, name, id, available); // AD support
    if(!available && ui.mode == Mode::Input && ps.pressed && (!ui.modal || ps.overlayNow) && hovering(r))
    {
        ps.pressed = false; // (a greyed tile pressed: why, in the console and its log)
        Con_Printf("toolgun: %s unavailable: %s\n", toolgun::spawnableName(entry), toolgun::spawnableUnavailableWhy(entry));
    }
    if(ui.mode == Mode::Draw)
    {
        if(!available) { GL_PushCanvasColor(1.f, 1.f, 1.f, 0.4f); } // AD support
        const bool chosen = available && toolgun::selectedSpawnable() == entry; // AD support
        const bool hover = available && hovering(r); // AD support
        tile(r, chosen, hover);
        const bool side = !strcmp(toolgun::spawnableSection(entry), "Weapons");
        const gfx::Picture* pic = toolgun::icons::get(toolgun::spawnableModel(entry), toolgun::spawnableSkin(entry), side);
        const float size = 36.f;
        if(pic)
        {
            gfx::draw2D::picture(*pic, r.cx() - size * 0.5f, r.y0 + 2.f, size, size);
        }
        else
        {
            const Painter p;
            p.disc(r.cx(), r.y0 + 2.f + size * 0.5f, 3.f, colors::boxBorder); // (still being made, or none)
        }
        // Its name: one line, or split at the space nearest its middle.
        const bool gold = !available || (!chosen && !hover); // AD support: unavailable entries stay visible
        const int len = static_cast<int>(strlen(name));
        const float dim = 6.f;
        if(len * dim <= r.w() - 3.f || !strchr(name, ' '))
        {
            textFit(r.cx(), r.y1 - 10.f, r.w() - 3.f, dim, name, gold);
        }
        else
        {
            int split = -1;
            for(int i = 0; i < len; i++)
            {
                if(name[i] == ' ' && (split < 0 || za::abs(i - len / 2) < za::abs(split - len / 2)))
                {
                    split = i;
                }
            }
            char first[48];
            q_strlcpy(first, name, za::min(static_cast<int>(sizeof(first)), split + 1));
            textFit(r.cx(), r.y1 - 14.f, r.w() - 3.f, dim, first, gold);
            textFit(r.cx(), r.y1 - 7.5f, r.w() - 3.f, dim, name + split + 1, gold);
        }
    }
    if(ui.mode == Mode::Draw && !available) { GL_PopCanvasColor(); } // AD support
    return clicked;
}

// The Spawn tab's grid for its section (spawnPage): made again only when the section (or the list's size) changes, not
// each frame. The list is fixed (vr_toolgun.cpp spawnables); whether an entry is available is asked as it is drawn.
constexpr int armedEntry = -2; // the grid's Armed Humanoid tile (Monsters, after id's and Quake VR's own: armedPage)

// The armed monsters' per-weapon entries (vr_toolgun_packs.inc's "Armed Grunts" and "Armed Enforcers"): left off the
// grid, the Armed Humanoid page makes them all (vr_toolgun_armed_tiles 1: listed too). Their numbers stay
// (vr_toolgun_select, the Debug menus).
[[nodiscard]] bool armedPackHidden(int i)
{
    return vr_toolgun_armed_tiles.value == 0.f && !strncmp(toolgun::spawnablePack(i), "Armed ", 6);
}

struct SpawnGrid
{
    int section{-1};
    int count{-1}; // toolgun::spawnableCount() it was made for
    float tiles{-1.f}; // and vr_toolgun_armed_tiles
    int perPage{0};
    za::InPlaceVector<int, maxEntries> entries; // spawnable index, -1: an empty cell
    za::InPlaceVector<int, maxPacks> groupPages; // each group's first page (">>": the next group's)
};
SpawnGrid spawnGridMade;

const SpawnGrid& spawnGrid(int perPage)
{
    SpawnGrid& g = spawnGridMade;
    if(g.section == ps.section && g.count == toolgun::spawnableCount() && g.perPage == perPage &&
        g.tiles == vr_toolgun_armed_tiles.value)
    {
        return g;
    }
    g.section = ps.section;
    g.count = toolgun::spawnableCount();
    g.tiles = vr_toolgun_armed_tiles.value;
    g.perPage = perPage;
    g.entries.clear();
    g.groupPages.clear();
    const auto inSection = [](int i) { return !strcmp(toolgun::spawnableSection(i), sections[ps.section]) && !armedPackHidden(i); };
    za::InPlaceVector<const char*, maxPacks> packs;
    packs.pushBack("");
    for(int i = 0; i < toolgun::spawnableCount(); i++)
    {
        const char* pack = toolgun::spawnablePack(i);
        bool known = !strcmp(pack, "AD");
        for(int k = 0; k < static_cast<int>(packs.size()) && !known; k++)
        {
            known = !strcmp(packs[k], pack);
        }
        if(inSection(i) && !known && packs.size() < packs.capacity() - 1)
        {
            packs.pushBack(pack);
        }
    }
    packs.pushBack("AD");
    za::InPlaceVector<int, maxEntries>& entries = g.entries;
    for(int k = 0; k < static_cast<int>(packs.size()); k++)
    {
        bool first = true;
        for(int i = 0; i < toolgun::spawnableCount() && entries.size() < entries.capacity(); i++)
        {
            if(!inSection(i) || strcmp(toolgun::spawnablePack(i), packs[k]))
            {
                continue;
            }
            while(first && k > 0 && entries.size() % perPage != 0 && entries.size() < entries.capacity())
            {
                entries.pushBack(-1);
            }
            if(first)
            {
                g.groupPages.pushBack(static_cast<int>(entries.size()) / perPage);
            }
            first = false;
            if(entries.size() < entries.capacity())
            {
                entries.pushBack(i);
            }
        }
        if(k == 0 && ps.section == 0 && entries.size() < entries.capacity())
        {
            entries.pushBack(armedEntry); // (Monsters: after id's and Quake VR's own, the Armed Humanoid page's tile)
        }
    }
    return g;
}

// The spawn tool's commands at the bar's right: Pick, Reset, Clear.
void spawnCommands(Ui& ui, const Rect& bar)
{
    const float bw = 52.f;
    const char* const labels[] = {"Pick", "Reset", "Clear"};
    const char* const commands[] = {"vr_toolgun_pick\n", "vr_toolgun_reset\n", "vr_toolgun_select clear\n"};
    for(int i = 0; i < 3; i++)
    {
        const float x1 = bar.x1 - (2 - i) * (bw + 4.f);
        if(button(ui, Rect{x1 - bw, bar.y0, x1, bar.y1}, labels[i], false, 6.f))
        {
            Cbuf_AddText(commands[i]);
        }
    }
}

// A model's thumbnail (vr_toolgun_icons.cpp) in a box, its caption under it; a word instead when there is no model.
void pictureBox(const Rect& r, const char* model, bool side, const char* caption, const char* word)
{
    tile(r, false, false);
    const gfx::Picture* pic = model && model[0] ? toolgun::icons::get(model, 0, side) : nullptr;
    const float size = za::min(r.w() - 4.f, r.h() - 12.f);
    if(pic)
    {
        gfx::draw2D::picture(*pic, r.cx() - size * 0.5f, r.y0 + 2.f, size, size);
    }
    else if(word)
    {
        textFit(r.cx(), r.y0 + 2.f + size * 0.5f - 4.f, r.w() - 4.f, 8.f, word, true);
    }
    textFit(r.cx(), r.y1 - 9.f, r.w() - 3.f, 5.f, caption, true);
}

// The Monsters tab's Armed Humanoid tile: opens its page (armedPage); lit while its choice is the spawn tool's.
bool armedTile(Ui& ui, const Rect& r)
{
    int id = 0;
    const bool clicked = interact(ui, r, "Armed Humanoid", id);
    if(ui.mode == Mode::Draw)
    {
        const bool chosen = toolgun::armed::chosen();
        const bool hover = hovering(r);
        tile(r, chosen, hover);
        const char* model = toolgun::armed::kindModel(za::max(0, static_cast<int>(vr_toolgun_armed_kind.value)));
        const gfx::Picture* pic = toolgun::icons::get(model[0] ? model : "progs/vrgrunt.mdl", 0, false);
        if(pic)
        {
            gfx::draw2D::picture(*pic, r.cx() - 18.f, r.y0 + 2.f, 36.f, 36.f);
        }
        textFit(r.cx(), r.y1 - 14.f, r.w() - 3.f, 6.f, "Armed", !chosen && !hover);
        textFit(r.cx(), r.y1 - 7.5f, r.w() - 3.f, 6.f, "Humanoid", !chosen && !hover);
    }
    return clicked;
}

// The Armed Humanoid page's lists for a kind (dropdown's rows: the names live in the toolgun's catalogue), made again
// when the kind or the catalogue changes.
constexpr int armedListMax = 100;
struct ArmedLists
{
    int generation{-1};
    int kind{-1};
    za::InPlaceVector<const char*, 8> kinds;
    za::InPlaceVector<const char*, armedListMax> right; // Random from Pool, Unarmed, its weapons
    za::InPlaceVector<int, armedListMax> rightCode;
    za::InPlaceVector<const char*, armedListMax> left; // None, its one-handed weapons
    za::InPlaceVector<int, armedListMax> leftCode;
};
ArmedLists armedLists;

const ArmedLists& armedListsFor(int kind)
{
    ArmedLists& l = armedLists;
    if(l.generation == toolgun::armed::generation() && l.kind == kind)
    {
        return l;
    }
    l = {};
    l.generation = toolgun::armed::generation();
    l.kind = kind;
    for(int k = 0; k < toolgun::armed::kindCount() && l.kinds.size() < l.kinds.capacity(); k++)
    {
        l.kinds.pushBack(toolgun::armed::kindName(k));
    }
    for(const int code : {0, -1})
    {
        l.right.pushBack(toolgun::armed::codeName(code));
        l.rightCode.pushBack(code);
    }
    l.left.pushBack("None");
    l.leftCode.pushBack(0);
    for(int i = 0; i < toolgun::armed::weaponCount(); i++)
    {
        const int code = toolgun::armed::weaponCode(i);
        if(toolgun::armed::fits(kind, code) && l.right.size() < l.right.capacity())
        {
            l.right.pushBack(toolgun::armed::codeName(code));
            l.rightCode.pushBack(code);
        }
        if(toolgun::armed::oneHanded(kind, code) && l.left.size() < l.left.capacity())
        {
            l.left.pushBack(toolgun::armed::codeName(code));
            l.leftCode.pushBack(code);
        }
    }
    return l;
}

template <typename Codes>
[[nodiscard]] int indexOfCode(const Codes& codes, int code)
{
    for(int i = 0; i < static_cast<int>(codes.size()); i++)
    {
        if(codes[i] == code)
        {
            return i;
        }
    }
    return -1;
}

// Spawn > Monsters > Armed Humanoid (ROUND22.md, "Armed monsters: the toolgun's spawn page"): an armed monster made to
// order: its kind (every row of the armed monsters' table: QC vr_armedmon.qc), its right hand's weapon (any its row
// holds, one from the pool at random, or none), its left hand's (none, or a one-handed weapon with a one-handed one in
// the right: dual wielding). Each choice is the spawn tool's at once (its ghost holds them); the pictures under them,
// a word on what it will do; Back to the grid.
void armedPage(Ui& ui)
{
    const int kind = za::clamp(static_cast<int>(vr_toolgun_armed_kind.value), 0, za::max(0, toolgun::armed::kindCount() - 1));
    const ArmedLists& l = armedListsFor(kind);
    const int code = static_cast<int>(vr_toolgun_armed_weapon.value);
    const int code2 = static_cast<int>(vr_toolgun_armed_weapon2.value);
    const float x0 = pad, x1 = panelWidth - pad;
    if(l.kinds.empty())
    {
        if(ui.mode == Mode::Draw)
        {
            paragraph(x0 + 4.f, 70.f, x1 - x0 - 8.f, 7.f, "No armed monsters here: start a game.", true);
        }
    }
    else
    {
        const int k = dropdown(ui, Rect{x0, 64.f, x1, 77.f}, "Kind", l.kinds.data(), static_cast<int>(l.kinds.size()), kind);
        if(k >= 0)
        {
            Cvar_SetValueQuick(&vr_toolgun_armed_kind, static_cast<float>(k));
            toolgun::armed::choose();
        }
        const int r = dropdown(ui, Rect{x0, 81.f, x1, 94.f}, "Right Hand", l.right.data(), static_cast<int>(l.right.size()),
            indexOfCode(l.rightCode, code));
        if(r >= 0)
        {
            Cvar_SetValueQuick(&vr_toolgun_armed_weapon, static_cast<float>(l.rightCode[r]));
            toolgun::armed::choose();
        }
        const bool pairs = code > 0 && toolgun::armed::oneHanded(kind, code);
        const int lc = dropdown(ui, Rect{x0, 98.f, x1, 111.f}, "Left Hand", l.left.data(), static_cast<int>(l.left.size()),
            pairs ? za::max(0, indexOfCode(l.leftCode, code2)) : 0, pairs);
        if(lc >= 0)
        {
            Cvar_SetValueQuick(&vr_toolgun_armed_weapon2, static_cast<float>(l.leftCode[lc]));
            toolgun::armed::choose();
        }
        if(ui.mode == Mode::Draw && !ps.drop.name[0]) // (none under an open list: the pictures are drawn after it)
        {
            // The pictures: the kind, its right hand's, its left hand's; a word on what it will do.
            const float y0 = 116.f, y1 = 170.f, w = 50.f;
            pictureBox(Rect{x0, y0, x0 + w, y1}, toolgun::armed::kindModel(kind), false, toolgun::armed::kindName(kind), nullptr);
            pictureBox(Rect{x0 + w + 3.f, y0, x0 + 2.f * w + 3.f, y1}, toolgun::armed::ghostModel(0), true,
                code == 0 ? "Random" : code < 0 ? "Fists" : toolgun::armed::codeName(code), code == 0 ? "?" : "-");
            pictureBox(Rect{x0 + 2.f * w + 6.f, y0, x0 + 3.f * w + 6.f, y1}, toolgun::armed::ghostModel(1), true,
                pairs && code2 > 0 ? toolgun::armed::codeName(code2) : "Left: none", "-");
            char note[160];
            if(code == 0)
            {
                q_snprintf(note, sizeof(note), "Random: a weapon from the pool (Combat > Armed Grunts), a second one-handed one %.0f%% of the time.",
                    Cvar_VariableValue("vr_agrunt_dual_chance") * 100.f);
            }
            else if(code < 0)
            {
                q_strlcpy(note, "Unarmed: it fights with its fists.", sizeof(note));
            }
            else if(pairs && code2 > 0)
            {
                q_strlcpy(note, "Dual wielding: it swings them in turn, now and then both at once.", sizeof(note));
            }
            else if(pairs)
            {
                q_strlcpy(note, "One-handed: a second one-handed weapon can go in its left hand.", sizeof(note));
            }
            else
            {
                q_strlcpy(note, "Two-handed or a gun: its left hand stays on it.", sizeof(note));
            }
            paragraph(x0 + 3.f * w + 12.f, y0 + 2.f, x1 - (x0 + 3.f * w + 12.f), 5.f, note, false);
        }
    }
    const Rect bar{pad, 175.f, panelWidth - pad, 189.f};
    if(button(ui, Rect{bar.x0, bar.y0, bar.x0 + 36.f, bar.y1}, "Back", false, 6.f))
    {
        ps.armed = false;
    }
    if(button(ui, Rect{bar.x0 + 40.f, bar.y0, bar.x0 + 80.f, bar.y1}, "Choose", toolgun::armed::chosen(), 6.f))
    {
        toolgun::armed::choose();
    }
    spawnCommands(ui, bar);
}

void spawnPage(Ui& ui)
{
    const float gap = 4.f;
    // The tabs.
    const float tw = (panelWidth - pad * 2.f - gap * (sectionCount - 1)) / sectionCount;
    for(int i = 0; i < sectionCount; i++)
    {
        const Rect r{pad + i * (tw + gap), 47.f, pad + i * (tw + gap) + tw, 60.f};
        if(button(ui, r, sections[i], ps.section == i, 6.f))
        {
            ps.section = i;
            ps.armed = false;
        }
    }
    if(ps.armed && ps.section == 0)
    {
        armedPage(ui);
        return;
    }
    // The section's entries, a page of them: id's and Quake VR's own first, then each pack's (the Scourge of Armagon's,
    // the Dissolution of Eternity's, the Machine packs', ...: toolgun::spawnablePack) as a group of its own from a fresh
    // page, in the list's order, Arcane Dimensions' last (-1: an empty cell).
    constexpr int perPage = gridColumns * gridRows;
    const SpawnGrid& grid = spawnGrid(perPage);
    const za::InPlaceVector<int, maxEntries>& entries = grid.entries;
    const za::InPlaceVector<int, maxPacks>& groupPages = grid.groupPages;
    const int pages = za::max(1, (static_cast<int>(entries.size()) + perPage - 1) / perPage);
    int& page = ps.gridPage[ps.section];
    page = za::clamp(page, 0, pages - 1);
    const float cw = (panelWidth - pad * 2.f - gap * (gridColumns - 1)) / gridColumns;
    const float ch = 52.f;
    for(int k = 0; k < perPage; k++)
    {
        const int n = page * perPage + k;
        if(n >= static_cast<int>(entries.size()))
        {
            break;
        }
        if(entries[n] < 0 && entries[n] != armedEntry)
        {
            continue; // AD support: the cell before Arcane Dimensions' group
        }
        const int col = k % gridColumns, row = k / gridColumns;
        const Rect r{pad + col * (cw + gap), 63.f + row * (ch + gap), pad + col * (cw + gap) + cw, 63.f + row * (ch + gap) + ch};
        if(entries[n] == armedEntry)
        {
            if(armedTile(ui, r))
            {
                ps.armed = true;
                toolgun::armed::choose(); // (its choice the spawn tool's at once: the ghost shows it)
            }
            continue;
        }
        if(thumbTile(ui, r, entries[n]))
        {
            toolgun::selectSpawnable(entries[n]);
        }
    }
    // The pages, and the spawn tool's commands.
    const Rect bar{pad, 175.f, panelWidth - pad, 189.f};
    if(pages > 1)
    {
        if(button(ui, Rect{bar.x0, bar.y0, bar.x0 + 16.f, bar.y1}, "<"))
        {
            page = (page + pages - 1) % pages;
        }
        if(button(ui, Rect{bar.x0 + 44.f, bar.y0, bar.x0 + 60.f, bar.y1}, ">"))
        {
            page = (page + 1) % pages;
        }
        // The next pack's group (the first again after the last).
        if(groupPages.size() > 1 && button(ui, Rect{bar.x0 + 63.f, bar.y0, bar.x0 + 81.f, bar.y1}, ">>"))
        {
            int next = groupPages[0];
            for(int k = static_cast<int>(groupPages.size()) - 1; k >= 0; k--)
            {
                next = groupPages[k] > page ? groupPages[k] : next;
            }
            page = next;
        }
        if(ui.mode == Mode::Draw)
        {
            char t[16];
            q_snprintf(t, sizeof(t), "%d/%d", page + 1, pages);
            textFit(bar.x0 + 30.f, bar.cy() - 3.f, 24.f, 6.f, t, false);
        }
    }
    spawnCommands(ui, bar);
}

void removePage(Ui& ui)
{
    if(ui.mode == Mode::Draw)
    {
        paragraph(pad + 4.f, 54.f, panelWidth - pad * 2.f - 8.f, 7.f,
            "Point at a thing: it glows red. The trigger removes it (a fire put out, a head's flies quiet). Never you.", true);
    }
    if(button(ui, Rect{pad + 30.f, 150.f, panelWidth - pad - 30.f, 166.f}, "Clear Props Made Since the Map Loaded", false, 6.f))
    {
        Cbuf_AddText("vr_scene_clean props\n");
    }
}

void physgunPage(Ui& ui)
{
    if(ui.mode == Mode::Draw)
    {
        paragraph(pad + 2.f, 48.f, panelWidth - pad * 2.f - 4.f, 6.f,
            "Hold the trigger on a thing to drag it; your wrist turns it. The other trigger freezes it. While held: gun "
            "stick nearer/farther and turn, other stick up/down/aside. X/A held: snap.",
            true);
    }
    toggleRow(ui, Rect{pad, 90.f, panelWidth - pad, 106.f}, "Turn With the Wrist", vr_toolgun_physgun_wrist);
    constexpr const char* snapNames[] = {"Off", "15", "45", "90"};
    constexpr float snapValues[] = {0.f, 15.f, 45.f, 90.f};
    choiceRow(ui, Rect{pad, 110.f, panelWidth - pad, 124.f}, "Snap (X/A)", vr_toolgun_snap, snapNames, snapValues, 4);
    constexpr const char* axisNames[] = {"Pitch", "Yaw", "Roll"};
    constexpr float axisValues[] = {0.f, 1.f, 2.f};
    choiceRow(ui, Rect{pad, 128.f, panelWidth - pad, 142.f}, "Sticks Turn", vr_toolgun_turn_axis, axisNames, axisValues, 3);
    if(button(ui, Rect{pad + 40.f, 150.f, panelWidth - pad - 40.f, 166.f}, "Unfreeze Everything", false, 7.f))
    {
        Cbuf_AddText("vr_toolgun_unfreeze_all\n");
    }
}

void scalePage(Ui& ui)
{
    if(ui.mode == Mode::Draw)
    {
        paragraph(pad + 2.f, 48.f, panelWidth - pad * 2.f - 4.f, 6.f,
            "Aim at a handle of a prop's box (it turns orange), hold the trigger and pull or push. X/A held: the gun's "
            "stick grows and shrinks it.",
            true);
    }
    toggleRow(ui, Rect{pad, 96.f, panelWidth - pad, 112.f}, "Proportional", vr_toolgun_scale_uniform);
    if(button(ui, Rect{pad + 40.f, 124.f, panelWidth - pad - 40.f, 140.f}, "Reset Its Size", false, 7.f))
    {
        Cbuf_AddText("vr_toolgun_scale_reset\n");
    }
}

void jointPage(Ui& ui)
{
    const int count = toolgun::jointCount();
    const float gap = 4.f;
    const float w = (panelWidth - pad * 2.f - gap * 3.f) / 4.f;
    const float h = 38.f;
    const int current = za::clamp(static_cast<int>(vr_toolgun_joint.value), 0, count - 1);
    for(int j = 0; j < count; j++)
    {
        const int col = j % 4, row = j / 4;
        const float x0 = pad + col * (w + gap) + (row == 1 ? (w + gap) * 0.5f : 0.f); // (the second row's 3 centred)
        const Rect r{x0, 47.f + row * (h + gap), x0 + w, 47.f + row * (h + gap) + h};
        const char* name = toolgun::jointName(j);
        int id = 0;
        if(interact(ui, r, name, id))
        {
            Cvar_SetValueQuick(&vr_toolgun_joint, static_cast<float>(j));
            Con_Printf("toolgun: joint %s\n", name);
        }
        if(ui.mode == Mode::Draw)
        {
            const bool chosen = current == j, hover = hovering(r);
            tile(r, chosen, hover);
            jointIcon(j, r.cx(), r.y0 + 14.f, toolColor(static_cast<int>(Tool::Joint)));
            textFit(r.cx(), r.y1 - 10.f, r.w() - 3.f, 7.f, name, !chosen && !hover);
        }
    }
    if(ui.mode == Mode::Draw)
    {
        paragraph(pad + 2.f, 134.f, panelWidth - pad * 2.f - 4.f, 6.f, jointHints[za::clamp(current, 0, jointHintCount - 1)], false);
    }
    if(button(ui, Rect{pad + 40.f, 156.f, panelWidth - pad - 40.f, 172.f}, "Remove Every Joint", false, 7.f))
    {
        Cbuf_AddText("vr_toolgun_unjoin_all\n");
    }
}

// The Brush tool: its modes, the grid, the map's textures (a page of thumbnails: the one chosen lit), Pick and Whole
// Brush (painting all of a box or the face shot).
void brushPage(Ui& ui)
{
    const int modes = toolgun::brushtool::modeCount();
    const int current = za::clamp(static_cast<int>(vr_toolgun_brush_mode.value), 0, modes - 1);
    const float gap = 3.f;
    const float mw = (panelWidth - pad * 2.f - gap * (modes - 1)) / modes;
    for(int slot = 0; slot < modes; slot++)
    {
        const int i = toolgun::brushtool::modeAt(slot);
        const Rect r{pad + slot * (mw + gap), 47.f, pad + slot * (mw + gap) + mw, 60.f};
        if(button(ui, r, toolgun::brushtool::modeName(i), current == i, 6.f))
        {
            Cvar_SetValueQuick(&vr_toolgun_brush_mode, static_cast<float>(i));
            toolgun::brushtool::reset();
            Con_Printf("toolgun: brush mode %s\n", toolgun::brushtool::modeName(i));
        }
    }
    constexpr const char* gridNames[] = {"1", "4", "8", "16", "32", "64"};
    constexpr float gridValues[] = {1.f, 4.f, 8.f, 16.f, 32.f, 64.f};
    choiceRow(ui, Rect{pad, 63.f, panelWidth - pad - 42.f, 75.f}, "Grid", vr_toolgun_brush_grid, gridNames, gridValues, 6);
    // The full bake: the map relit by light.exe with the brushes in it (vr_brush_bake_map), then reloaded where you are.
    if(button(ui, Rect{panelWidth - pad - 40.f, 63.f, panelWidth - pad, 75.f}, "Bake Copy", false, 6.f))
    {
        Cbuf_AddText("vr_brush_bake_map\n");
    }

    // The shape Create makes, and a cylinder's or pyramid's sides.
    {
        constexpr const char* shapeLabels[] = {"Box", "Wedge", "Cylinder", "Pyramid"};
        constexpr const char* sideNames[] = {"3", "4", "6", "8", "12"};
        constexpr float sideValues[] = {3.f, 4.f, 6.f, 8.f, 12.f};
        const Rect row{pad, 78.f, panelWidth - pad, 89.f};
        if(ui.mode == Mode::Draw)
        {
            text(row.x0 + 2.f, row.cy() - 3.f, 6.f, "Shape", true);
        }
        const float sx = row.x0 + 30.f, sw = 31.f;
        for(int k = 0; k < 4; k++)
        {
            const Rect b{sx + k * (sw + 2.f), row.y0, sx + k * (sw + 2.f) + sw, row.y1};
            if(button(ui, b, shapeLabels[k], static_cast<int>(vr_toolgun_brush_shape.value) == k, 6.f))
            {
                Cvar_SetValueQuick(&vr_toolgun_brush_shape, static_cast<float>(k));
                Con_Printf("toolgun: brush shape %s\n", brushes::shapeName(static_cast<brushes::Shape>(k)));
            }
        }
        const float dx = sx + 4.f * (sw + 2.f) + 6.f, dw = (row.x1 - dx - 4.f * 2.f) / 5.f;
        for(int k = 0; k < 5; k++)
        {
            const Rect b{dx + k * (dw + 2.f), row.y0, dx + k * (dw + 2.f) + dw, row.y1};
            if(button(ui, b, sideNames[k], za::fabs(vr_toolgun_brush_sides.value - sideValues[k]) < 0.5f, 6.f))
            {
                Cvar_SetValueQuick(&vr_toolgun_brush_sides, sideValues[k]);
            }
        }
    }

    // The textures: six a row, two rows a page.
    constexpr int columns = 6, rows = 2, perPage = columns * rows;
    const int count = brushes::textureCount();
    const int pages = za::max(1, (count + perPage - 1) / perPage);
    int& page = ps.texturePage;
    page = za::clamp(page, 0, pages - 1);
    const float tw = (panelWidth - pad * 2.f - gap * (columns - 1)) / columns, th = 39.f;
    const char* chosen = brushes::currentTexture();
    for(int k = 0; k < perPage; k++)
    {
        const int index = page * perPage + k;
        texture_t* t = index < count ? brushes::textureAt(index) : nullptr;
        if(!t)
        {
            break;
        }
        const int col = k % columns, row = k / columns;
        const Rect r{pad + col * (tw + gap), 92.f + row * (th + 3.f), pad + col * (tw + gap) + tw, 92.f + row * (th + 3.f) + th};
        int id = 0;
        if(interact(ui, r, t->name, id))
        {
            brushes::setCurrentTexture(t->name);
            Con_Printf("toolgun: texture %s\n", t->name);
        }
        if(ui.mode == Mode::Draw)
        {
            const bool on = !q_strcasecmp(chosen, t->name), hover = hovering(r);
            tile(r, on, hover);
            const float side = 26.f;
            Draw_TextureRect(r.cx() - side * 0.5f, r.y0 + 2.f, side, side, t->gltexture, 1.f);
            textFit(r.cx(), r.y1 - 9.f, r.w() - 2.f, 5.f, t->name, !on && !hover);
        }
    }
    const Rect bar{pad, 175.f, panelWidth - pad, 189.f};
    if(pages > 1)
    {
        if(button(ui, Rect{bar.x0, bar.y0, bar.x0 + 16.f, bar.y1}, "<"))
        {
            page = (page + pages - 1) % pages;
        }
        if(button(ui, Rect{bar.x0 + 44.f, bar.y0, bar.x0 + 60.f, bar.y1}, ">"))
        {
            page = (page + 1) % pages;
        }
        if(ui.mode == Mode::Draw)
        {
            char text[16];
            q_snprintf(text, sizeof(text), "%d/%d", page + 1, pages);
            textFit(bar.x0 + 30.f, bar.cy() - 3.f, 24.f, 6.f, text, false);
        }
    }
    if(button(ui, Rect{bar.x0 + 64.f, bar.y0, bar.x0 + 90.f, bar.y1}, "Undo", false, 6.f))
    {
        Cbuf_AddText("vr_brush_undo\n");
    }
    if(button(ui, Rect{bar.x0 + 93.f, bar.y0, bar.x0 + 133.f, bar.y1}, vr_toolgun_brush_texlock.value != 0.f ? "Tex Lock" : "Tex Free",
           vr_toolgun_brush_texlock.value != 0.f, 6.f))
    {
        Cvar_SetValueQuick(&vr_toolgun_brush_texlock, vr_toolgun_brush_texlock.value != 0.f ? 0.f : 1.f);
    }
    if(button(ui, Rect{bar.x1 - 112.f, bar.y0, bar.x1 - 88.f, bar.y1}, "Pick", toolgun::brushtool::picking(), 6.f))
    {
        toolgun::brushtool::pickTexture();
    }
    if(button(ui, Rect{bar.x1 - 85.f, bar.y0, bar.x1, bar.y1}, vr_toolgun_brush_paint_all.value != 0.f ? "Paint: Whole" : "Paint: One Face",
           false, 6.f))
    {
        Cvar_SetValueQuick(&vr_toolgun_brush_paint_all, vr_toolgun_brush_paint_all.value != 0.f ? 0.f : 1.f);
    }
}

void morePage(Ui& ui)
{
    const int n = menu::toolgunCheatCount();
    const float gap = 3.f;
    const float w = (panelWidth - pad * 2.f - gap) / 2.f;
    const float h = 11.f;
    for(int i = 0; i < n && i < 20; i++)
    {
        const int col = i % 2, row = i / 2;
        const Rect r{pad + col * (w + gap), 47.f + row * (h + 1.f), pad + col * (w + gap) + w, 47.f + row * (h + 1.f) + h};
        if(button(ui, r, menu::toolgunCheatName(i), menu::toolgunCheatOn(i), 6.f))
        {
            menu::toolgunCheatUse(i);
        }
    }
    if(qvr::renderdoc::attached())
    {
        // RenderDoc's Capture Frame (vr_renderdoc.cpp): the panel is hidden for the captured frame; the delay 0 / 1 / 3 s.
        const float y = 158.f;
        if(button(ui, Rect{pad, y, pad + 120.f, y + h}, "Capture Frame (RenderDoc)", false, 6.f))
        {
            qvr::renderdoc::captureFromCvars();
        }
        char label[24];
        q_snprintf(label, sizeof(label), "Delay: %g s", vr_renderdoc_delay.value);
        if(button(ui, Rect{pad + 124.f, y, pad + 180.f, y + h}, label, false, 6.f))
        {
            const float d = vr_renderdoc_delay.value;
            Cvar_SetValueQuick(&vr_renderdoc_delay, d < 0.5f ? 1.f : d < 2.f ? 3.f : 0.f);
        }
    }
    sliderRow(ui, Rect{pad, 172.f, panelWidth - pad, 188.f}, "Panel Size", vr_toolgun_panel_size, 3.f, 12.f, 0.5f, "%.1f");
}

void layout(Ui& ui)
{
    if(ui.mode == Mode::Draw)
    {
        // The panel: the menus' box, edged.
        fill(Rect{0.f, 0.f, panelWidth, panelHeight}, colors::boxBorder);
        fill(Rect{1.f, 1.f, panelWidth - 1.f, panelHeight - 1.f}, glm::vec4{0.07f, 0.055f, 0.04f, 1.f});
    }
    ui.modal = ps.drop.name[0] != '\0';
    ps.drop.seen = false;
    toolRow(ui);
    infoLine(ui);
    if(ps.more)
    {
        morePage(ui);
    }
    else
    {
        switch(currentTool())
        {
        case Tool::Spawn: spawnPage(ui); break;
        case Tool::Remove: removePage(ui); break;
        case Tool::Physgun: physgunPage(ui); break;
        case Tool::Scale: scalePage(ui); break;
        case Tool::Joint: jointPage(ui); break;
        case Tool::Brush: brushPage(ui); break;
        default: break;
        }
    }
    listOverlay(ui); // (last: over the rest)
}

// ---------------------------------------------------------------------------------------------------------------------
// Commands.

// vr_toolgun_mock_laser <x> <y> | <widget's name> | off (tests): the other hand's laser on that spot of the panel (its
// virtual pixels, 256 x 192 from its top left) or on the middle of the widget so named (as vr_toolgun_panel_dump lists
// them; the panel open), whatever the hand's pose.
void mockLaser_f()
{
    if(Cmd_Argc() == 2 && !q_strcasecmp(Cmd_Argv(1), "off"))
    {
        ps.mock = false;
        return;
    }
    if(Cmd_Argc() == 3 && (Cmd_Argv(1)[0] == '-' || (Cmd_Argv(1)[0] >= '0' && Cmd_Argv(1)[0] <= '9')))
    {
        ps.mock = true;
        ps.mockSpot = glm::vec2{Q_atof(Cmd_Argv(1)), Q_atof(Cmd_Argv(2))};
        return;
    }
    if(Cmd_Argc() >= 2)
    {
        const char* name = Cmd_Args();
        for(const Spot& spot : ps.spots)
        {
            if(!q_strcasecmp(spot.name, name))
            {
                ps.mock = true;
                ps.mockSpot = glm::vec2{spot.r.cx(), spot.r.cy()};
                return;
            }
        }
        Con_Printf("vr_toolgun_mock_laser: no \"%s\" on the panel (vr_toolgun_panel_dump lists them)\n", name);
        return;
    }
    Con_Printf("vr_toolgun_mock_laser <x> <y> | <widget's name> | off\n");
}

// vr_toolgun_panel_dump: the panel's state and its widgets as the last input pass laid them out.
void dump_f()
{
    Con_Printf("toolgun panel: %s, %s, laser %s at %.0f %.0f (widget %d), held %d\n", ps.open ? "open" : "closed",
        toolgun::panel::shown() ? "shown" : "hidden", ps.hit ? "on it" : "off it", ps.cursor.x, ps.cursor.y, ps.hovered,
        ps.held ? 1 : 0);
    for(int i = 0; i < static_cast<int>(ps.spots.size()); i++)
    {
        const Spot& s = ps.spots[i];
        Con_Printf("  %d \"%s\" %.0f %.0f %.0f %.0f\n", i, s.name, s.r.x0, s.r.y0, s.r.x1, s.r.y1);
    }
}

void icons_f()
{
    toolgun::icons::print();
}

} // namespace

namespace qvr::toolgun::panel
{

bool shown()
{
    return ps.open && heldHand() >= 0 && key_dest == key_game && !cl.intermission;
}

void toggle()
{
    if(ps.open)
    {
        close();
        return;
    }
    if(heldHand() < 0)
    {
        return;
    }
    ps.open = true;
    Con_Printf("toolgun: panel open\n");
}

void close()
{
    if(ps.open)
    {
        Con_Printf("toolgun: panel closed\n");
    }
    ps.open = false;
    closeList();
    ps.held = ps.pressed = false;
    ps.dragging = -1;
    ps.hit = false;
}

bool trigger(int hand, bool down)
{
    if(!down)
    {
        const bool was = ps.held;
        ps.held = false;
        ps.dragging = -1;
        return was;
    }
    if(!shown() || hand != laserHand() || !ps.hit)
    {
        return false;
    }
    ps.held = true;
    ps.pressed = true;
    return true;
}

void frame(const hands::State& s)
{
    if(heldHand() < 0)
    {
        close();
        return;
    }
    const int tool = static_cast<int>(currentTool());
    if(tool != ps.shownTool)
    {
        if(ps.shownTool >= 0)
        {
            ps.more = false;
        }
        ps.shownTool = tool;
    }
    if(!shown())
    {
        ps.hit = false;
        return;
    }
    toolgun::armed::refresh(); // (the Armed Humanoid page's lists: asked of QC once a map)
    glm::vec3 point;
    ps.hit = laserHit(s, point, ps.cursor);
    Ui ui{Mode::Input};
    ps.spots.clear();
    layout(ui);
    ps.pressed = false; // (a press off every widget: nothing)
    if(ui.hovered != ps.hovered && ui.hovered >= 0)
    {
        toolgun::haptic(laserHand(), 0.01f, 0.12f); // a tick on each widget the laser comes onto
    }
    ps.hovered = ui.hovered;
}

void render()
{
    if(!shown())
    {
        return;
    }
    toolgun::icons::makePending(2);
    gfx::ensureTarget(ps.target, static_cast<int>(panelWidth) * texelsPerPixel, static_cast<int>(panelHeight) * texelsPerPixel,
        true, "toolgun panel");
    gfx::begin2D(ps.target, static_cast<int>(panelWidth), static_cast<int>(panelHeight));
    Ui ui{Mode::Draw};
    layout(ui);
    gfx::end2D();
}

void drawInEye(const hands::State& s)
{
    glm::vec3 corner, xAxis, yAxis;
    if(!shown() || qvr::renderdoc::hidingPanel() || !ps.target.texture || !s.valid || !quadOf(s, corner, xAxis, yAxis))
    {
        return;
    }
    const glm::vec4 white{1.f};
    const gfx::Vertex c[4] = {{corner, {0.f, 0.f}, white}, {corner + xAxis, {1.f, 0.f}, white},
        {corner + xAxis + yAxis, {1.f, 1.f}, white}, {corner + yAxis, {0.f, 1.f}, white}};
    const gfx::Vertex quad[6] = {c[0], c[1], c[2], c[0], c[2], c[3]};
    gfx::draw(quad, gfx::sceneViewProjection(),
        {.shade = gfx::Shade::Texture, .blend = gfx::Blend::Opaque, .depthTest = false, .depthWrite = false}, ps.target.texture);

    // The laser, thin: from the other hand to where it meets the panel (a short stub when it doesn't).
    glm::vec3 origin, fwd, r, u, point;
    glm::vec2 cursor;
    const int hand = laserHand();
    if(hand < 0 || !toolgun::controllerFrame(s, hand, origin, fwd, r, u))
    {
        return;
    }
    const bool on = laserHit(s, point, cursor);
    menuui::drawLaser(origin, on ? point : origin + fwd * 8.f, on, ps.held, za::clamp(vr_toolgun_laser_size.value, 0.05f, 2.f));
}

void registerCommands()
{
    Cmd_AddCommand("vr_toolgun_mock_laser", mockLaser_f);
    Cmd_AddCommand("vr_toolgun_panel_dump", dump_f);
    Cmd_AddCommand("vr_toolgun_icons", icons_f);
}

} // namespace qvr::toolgun::panel
