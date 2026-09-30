// vr_menuui.cpp -- the VR menu style (vr_menu_vr_style): Ironwail's menus, and the port's own VR
// pages, as they are, made for a headset by what draws them rather than by menus of their own.
//
// - The menu canvas (CANVAS_MENU) fills the panel's height, whatever scr_menuscale says, and
//   scales y by vr_menu_spacing more than x (VR_MenuCanvas): every row the menus lay out 8 pixels
//   apart comes out further apart, and hit tests, which go through the same transform (the menus'
//   mouse code), follow. The 2D layer draws characters and pictures at their own size there,
//   centred on where they were (gl_draw.c, Draw_KeepMenuGlyphSize), so the text is not stretched.
//   The panel is vr_menu_height times Quake's 200 rows high (more rows at once, with room to spare
//   in a headset): Quake's 320 x 200 stays in its middle, and the menus that lay out from the
//   canvas's bounds (Ironwail's lists: options, key bindings, maps, mods) and the VR pages use the
//   rows above and below. Menus drawn from pictures with a cursor stepping over them (the main,
//   single player and multiplayer menus...) keep Quake's spacing: their rows are already 20
//   pixels apart.
// - The widgets are drawn anew (the menus call Quake's M_Draw* functions, which hand over): the
//   slider is a track filled up to a round thumb, the checkbox a switch, the text box a panel with
//   a thin border (and a list's scrollbar thumb a pill), and the selected row gets a highlight bar.
// - A laser from the pointing hand (the main hand, or the one whose trigger was pressed last)
//   meets the panel: that spot is the menu's mouse (M_Mousemove), so rows under it are selected as
//   with a mouse, and the trigger is the left mouse button there: it picks an item, sets a slider
//   where it points and drags it, drags a list's scrollbar. The sticks, A and B work as
//   before.
// - "Back to game": a button at the panel's top left (or the menu button, vr_input.cpp)
//   closes the menu from whatever page it is on; opening it again (the menu button, Escape,
//   togglemenu) returns to that page, its selection and scroll as they were (vr_menu_remember).
//   Only that way of closing it is remembered: Escape from the main menu, or a menu closing
//   because a game started or loaded, opens the main menu next time, as Quake does.
// - Under it, "Advanced VR" and "Levels" jump to the Advanced VR Options and to the level list from
//   any menu. The laser clicks the three; the sticks reach them too: a stick's click on any menu, or
//   up from a VR page's first setting (down from its last), then up and down, A to press, B back.
// - The main hand's stick scrolls a page with a scrollbar (the VR pages, and Ironwail's lists: the
//   options, maps, mods, key bindings), a row at a time at a rate growing with the push, the
//   selection kept where it is while it stays in view. It never changes a setting: its left and
//   right do nothing in menus, only the off hand's stick (and the laser) change values
//   (vr_input.cpp).

#include "vr_checklist.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_gfx.hpp"
#include "vr_hue.hpp"
#include "vr_main.hpp"
#include "vr_mem.hpp"
#include "vr_menu.hpp"
#include "vr_menuui.hpp"
#include "vr_panel.hpp"
#include "vr_units.hpp"

#include <cmath>
#include <cstring>
#include <vector>

extern "C" {
extern float m_mousex, m_mousey; // menu.c: the menus' mouse, in menu coordinates
extern m_state_e m_skill_prevmenu, m_quit_prevstate; // menu.c: where the skill and quit menus came from

// menu.c: its menus' openers, and its lists for the VR controllers (M_ScrollList: // QVR).
void M_Menu_SinglePlayer_f(void);
void M_Menu_Load_f(void);
void M_Menu_Save_f(void);
void M_Menu_Maps_f(void);
void M_Menu_MultiPlayer_f(void);
void M_Menu_Setup_f(void);
void M_Menu_Net_f(void);
void M_Options_Init(m_state_e state);
void M_Menu_Keys_f(void);
void M_Menu_Mods_f(void);
void M_Menu_Help_f(void);
qboolean M_ScrollList(int rows);
qboolean M_ListPosition(int* cursor, int* scroll, qboolean set);
}

using namespace qvr;

namespace
{

// ----------------------------------------------------------------------------
// Layout
// ----------------------------------------------------------------------------

[[nodiscard]] float spacingSetting()
{
    return CLAMP(1.f, vr_menu_spacing.value, 2.f);
}

// The menu's height in menu pixels (Quake's 200, times vr_menu_height), whole rows.
[[nodiscard]] int heightSetting()
{
    return static_cast<int>(std::lround(200.f * CLAMP(1.f, vr_menu_height.value, 2.f) / 8.f)) * 8;
}

// The current menu's row spacing: 1 for the menus drawn from pictures, whose cursor steps over a
// picture's rows (spacing would move the cursor off them).
[[nodiscard]] float rowSpacing()
{
    switch(m_state)
    {
        case m_main:
        case m_singleplayer:
        case m_multiplayer:
        case m_net:
        case m_skill:
        case m_help:
        case m_quit: // drawn over the menu it came from
            return 1.f;
        default: return spacingSetting();
    }
}

// Canvas (2D) units per menu pixel across: the menu, 320 wide and heightSetting() high, its rows
// spaced out as much as they can be, fits the canvas; the same for every menu, so that the text is
// the same size in all. Quake's 320 x 200 stays in the middle: the menus that lay out from the
// canvas's bounds (Ironwail's lists, the VR pages) get the rows above and below it.
[[nodiscard]] float canvasScale()
{
    return std::fmin(vid.guiwidth / 320.f, vid.guiheight / (heightSetting() * spacingSetting()));
}

// The styled widgets are drawn only into the menu canvas while it is the VR one.
[[nodiscard]] bool styled()
{
    return menuui::active() && glcanvas.type == CANVAS_MENU;
}

// ----------------------------------------------------------------------------
// Drawing, in menu coordinates
// ----------------------------------------------------------------------------

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
constexpr glm::vec4 boxBorder{0.60f, 0.40f, 0.18f, 1.f};
constexpr glm::vec4 boxFill{0.07f, 0.055f, 0.04f, 0.92f};
constexpr glm::vec4 scrollThumb{0.86f, 0.55f, 0.18f, 0.9f};
constexpr glm::vec4 buttonHover{0.45f, 0.26f, 0.08f, 0.95f};
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
        k = std::fmax(1.f, -t.scale[1] * vid.guiheight / (t.scale[0] * vid.guiwidth));
        step = std::fmax(2.f / (t.scale[0] * vid.guiwidth), 0.25f);
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
        r = std::fmin(r, std::fmin(half, (x1 - x0) * 0.5f));
        const float straight = half - r;
        rect(x0, x1, yc, straight, color);
        for(float t = straight; t < half; t += step)
        {
            const float t1 = std::fmin(t + step, half);
            const float e = (t + t1) * 0.5f - straight;
            const float inset = r - std::sqrt(std::fmax(0.f, r * r - e * e));
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
            const float t1 = std::fmin(t + step, half);
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
            const float t1 = std::fmin(t + step, half);
            const float inset = (t + t1) * 0.5f * w / half;
            band(x - w, x - inset, yc, -t1, -t, color);
            band(x - w, x - inset, yc, t, t1, color);
        }
    }
};

// ----------------------------------------------------------------------------
// The pointer
// ----------------------------------------------------------------------------

struct Hit
{
    bool valid{false};
    glm::vec3 point{0.f};
    glm::vec2 uv{0.f};           // on the canvas, v up
    bool onRuntimePanel{false}; // on the runtime's own panel (the menus before a map): no world to draw the laser in
};

int pointingHand = HAND_MAIN;
Hit hits[HAND_COUNT];
bool mouseHeld[HAND_COUNT]{};
glm::ivec2 lastSent{-100000};
glm::vec2 lastSentMenu{0.f}; // the menus' mouse it made

// The selected row's highlight as last drawn (the menu, its y), and as the pointer last saw it: a
// tick in the pointing hand when the pointer moves the selection.
struct Highlight
{
    int menu{m_none};
    int y{0};
    bool operator==(const Highlight&) const = default;
};
Highlight drawnHighlight, seenHighlight;

// A hand's pointing ray in the world: the controller as tracked, aimed as a gun is (the hand calibration's pitch and
// yaw: vr_gunangle, vr_offhandpitch), placed from the head. Not the hands' state's pose, which follows a held
// weapon's weight in game time (frozen while a menu pauses the game).
[[nodiscard]] bool pointerRay(const hands::State& s, int hand, glm::vec3& origin, glm::vec3& dir)
{
    const TrackingState& t = tracking();
    if(!s.valid || !t.head.valid || !t.hands[hand].valid)
    {
        return false;
    }

    const auto quakeFromTracking = [](const glm::vec3& v) { return glm::vec3{-v.z, -v.x, v.y}; };
    const float turn = hands::playSpaceYaw();
    const glm::quat aim = hands::aimedController(t.hands[hand].orientation, hand); // the calibration's pitch and yaw

    origin = s.head + hands::rotateYaw(quakeFromTracking(t.hands[hand].position - t.head.position) * units::metresToUnits(), turn);
    dir = glm::normalize(hands::rotateYaw(quakeFromTracking(aim * glm::vec3{0.f, 0.f, -1.f}), turn));
    return true;
}

// vr_mock_laser (tests): the main hand's laser on a spot of the menu (menu coordinates), whatever
// the hand's pose.
struct MockLaser
{
    bool on{false};
    glm::vec2 spot{0.f};
};
MockLaser mockLaser;

// The mock laser's spot on the canvas (v up).
[[nodiscard]] glm::vec2 mockLaserUv()
{
    drawtransform_t t;
    Draw_GetCanvasTransform(CANVAS_MENU, &t);
    return {(mockLaser.spot.x * t.scale[0] + t.offset[0] + 1.f) * 0.5f, (mockLaser.spot.y * t.scale[1] + t.offset[1] + 1.f) * 0.5f};
}

// Where a hand points on the runtime's own panel, which shows the menus while the world is not drawn (before a map, over
// a demo: Backend::runtimePanel), in tracking space: the controller as tracked, aimed as a gun is.
// Where each hand's ray last met the runtime panel's plane (uv, even off it; menu_vr pos).
glm::vec2 runtimePanelUv[HAND_COUNT]{};

[[nodiscard]] Hit intersectRuntimePanel(int hand)
{
    Hit hit;
    Backend* be = backend();
    Pose panel;
    glm::vec2 size{0.f};
    if(!be || !be->runtimePanel(panel, size) || size.x <= 0.f || size.y <= 0.f)
    {
        return hit;
    }
    if(mockLaser.on && hand == HAND_MAIN)
    {
        return {true, glm::vec3{0.f}, mockLaserUv(), true};
    }
    const Pose& controller = tracking().hands[hand];
    if(!controller.valid)
    {
        return hit;
    }

    const glm::vec3 dir = hands::aimedController(controller.orientation, hand) * glm::vec3{0.f, 0.f, -1.f};
    const glm::vec3 normal = panel.orientation * glm::vec3{0.f, 0.f, 1.f};
    const float along = glm::dot(dir, normal);
    if(std::fabs(along) < 1e-6f)
    {
        return hit;
    }
    const float t = glm::dot(panel.position - controller.position, normal) / along;
    if(t <= 0.f)
    {
        return hit;
    }
    const glm::vec3 local = glm::inverse(panel.orientation) * (controller.position + dir * t - panel.position);
    const glm::vec2 uv{local.x / size.x + 0.5f, local.y / size.y + 0.5f};
    runtimePanelUv[hand] = uv;
    if(uv.x < 0.f || uv.x > 1.f || uv.y < 0.f || uv.y > 1.f)
    {
        return hit;
    }
    return {true, glm::vec3{0.f}, uv, true};
}

[[nodiscard]] Hit intersect(const hands::State& s, int hand)
{
    Hit hit;
    glm::vec3 corner, xAxis, yAxis, origin, dir;
    if(!s.valid || !panel::menuQuad(s, corner, xAxis, yAxis))
    {
        return intersectRuntimePanel(hand); // (the menu is not in the eyes)
    }
    if(mockLaser.on && hand == HAND_MAIN)
    {
        const glm::vec2 uv = mockLaserUv();
        return {true, corner + xAxis * uv.x + yAxis * uv.y, uv};
    }
    if(!pointerRay(s, hand, origin, dir))
    {
        return hit;
    }

    const glm::vec3 normal = glm::cross(xAxis, yAxis);
    const float along = glm::dot(dir, normal);
    if(std::fabs(along) < 1e-6f)
    {
        return hit;
    }
    const float t = glm::dot(corner - origin, normal) / along;
    if(t <= 0.f)
    {
        return hit;
    }

    const glm::vec3 p = origin + dir * t;
    const glm::vec2 uv{glm::dot(p - corner, xAxis) / glm::dot(xAxis, xAxis), glm::dot(p - corner, yAxis) / glm::dot(yAxis, yAxis)};
    if(uv.x < 0.f || uv.x > 1.f || uv.y < 0.f || uv.y > 1.f)
    {
        return hit;
    }
    return {true, p, uv};
}

// The spot as a window position (the canvas covers the 2D layer's viewport) to M_Mousemove: when
// it moved past the hand's tremor (so that it does not undo the sticks' moves), at all while the
// button is held (dragging), or when the desktop's mouse moved the menus' since.
void moveMouse(const Hit& hit, bool force, bool held)
{
    const glm::ivec2 p{glx + static_cast<int>(hit.uv.x * glwidth), gly + static_cast<int>((1.f - hit.uv.y) * glheight)};
    const glm::ivec2 d = glm::abs(p - lastSent);
    const int threshold = held ? 0 : 3;
    if(force || d.x > threshold || d.y > threshold || glm::vec2{m_mousex, m_mousey} != lastSentMenu)
    {
        lastSent = p;
        M_Mousemove(p.x, p.y);
        lastSentMenu = {m_mousex, m_mousey};
    }
}

// A camera-facing strip from `a` to `b` (soft edges), or a disc at `a` (b == a).
void appendStrip(std::vector<gfx::Vertex>& v, const glm::vec3& a, const glm::vec3& b, float width,
    const glm::vec4& colorA, const glm::vec4& colorB)
{
    glm::vec3 eye, right, up;
    gfx::sceneCamera(eye, right, up);
    const float h = width * 0.5f;

    if(a == b)
    {
        const glm::vec3 r = right * h;
        const glm::vec3 u = up * h;
        const gfx::Vertex q[4]{{a - r - u, {-1.f, -1.f}, colorA}, {a + r - u, {1.f, -1.f}, colorA},
            {a + r + u, {1.f, 1.f}, colorA}, {a - r + u, {-1.f, 1.f}, colorA}};
        for(const int i : {0, 1, 2, 0, 2, 3})
        {
            v.push_back(q[i]);
        }
        return;
    }

    const glm::vec3 side = glm::normalize(glm::cross(b - a, eye - a)) * h;
    const gfx::Vertex q[4]{{a - side, {-1.f, 0.f}, colorA}, {a + side, {1.f, 0.f}, colorA},
        {b + side, {1.f, 0.f}, colorB}, {b - side, {-1.f, 0.f}, colorB}};
    for(const int i : {0, 1, 2, 0, 2, 3})
    {
        v.push_back(q[i]);
    }
}

// ----------------------------------------------------------------------------
// The corner's buttons: Back to game, Advanced VR, Levels, Checklist
// ----------------------------------------------------------------------------

// A column at the panel's top left, over every menu: "Back to game" (closes the menu, which reopens
// where it was), "Advanced VR" (the Advanced VR Options page), "Levels" (Ironwail's level list) and
// "Checklist" (the playtest checklist, its open items counted on it; vr_checklist.hpp), from any page. The laser clicks them; the sticks reach them too (focus): a click of either stick
// on any menu, or on a VR page up from its first setting (down from its last).
enum Tool
{
    ToolBack,
    ToolAdvanced,
    ToolLevels,
    ToolChecklist,
    ToolCount
};

// (The checklist's count after its label: "Checklist 99" at most, as wide as "Back to game".)
constexpr const char* toolLabels[ToolCount]{"Back to game", "Advanced VR", "Levels", "Checklist 99"};

// Where the column goes: across, menu pixels; up and down, from the canvas's top in true pixels
// (menu pixels as wide as they are across: `k` menu rows' pixels each, the canvas's row spacing).
struct ToolbarLayout
{
    static constexpr float corner = 4.f; // true pixels from the panel's edges
    static constexpr float half = 7.f;   // half a button's height
    static constexpr float gap = 2.f;    // between two buttons
    static constexpr float icon = 9.f;   // an icon's width

    float left{0.f}, top{0.f}; // the canvas's corner
    float k{1.f};
    float x0{0.f}, x1{0.f};
    bool labels{false};

    // A button's middle (menu y).
    [[nodiscard]] float yc(int tool) const { return top + (corner + half + tool * (2.f * half + gap)) / k; }

    // The last button's bottom edge (menu y).
    [[nodiscard]] float bottom() const { return yc(ToolCount - 1) + half / k; }

    // What a click on `tool` takes: as far as the panel's edges, split halfway between buttons.
    [[nodiscard]] bool hit(int tool, float x, float y) const
    {
        const float y0 = tool == 0 ? top : yc(tool) - (half + gap * 0.5f) / k;
        const float y1 = yc(tool) + (half + (tool == ToolCount - 1 ? 2.f : gap * 0.5f)) / k;
        return x >= left && x <= x1 + 2.f && y >= y0 && y <= y1;
    }
};

// From the menu canvas's transform (as the menus lay out, and whether or not it is the one set).
[[nodiscard]] ToolbarLayout toolbarLayout()
{
    drawtransform_t t;
    Draw_GetCanvasTransform(CANVAS_MENU, &t);
    float right, bottom;
    ToolbarLayout l;
    Draw_GetTransformBounds(&t, &l.left, &l.top, &right, &bottom);
    l.k = std::fmax(1.f, -t.scale[1] * vid.guiheight / (t.scale[0] * vid.guiwidth)); // as Painter's

    // All as wide as the widest label, their right edges a character left of Quake's plaque (x 16): on
    // a wide panel near the menu rather than out at its corner. Where the labels do not fit, only
    // the icons, in the corner.
    float widest = 0.f;
    for(const char* label : toolLabels)
    {
        widest = std::fmax(widest, 8.f * static_cast<float>(strlen(label)));
    }
    const float width = 4.f + ToolbarLayout::icon + 4.f + widest + 5.f;
    l.x1 = 16.f - 8.f;
    l.x0 = l.x1 - width;
    l.labels = l.x0 >= l.left + ToolbarLayout::corner;
    if(!l.labels)
    {
        l.x0 = l.left + ToolbarLayout::corner;
        l.x1 = l.x0 + 4.f + ToolbarLayout::icon + 4.f;
    }
    return l;
}

struct Toolbar
{
    int menu{m_none};  // the menu it was last drawn over (m_none: not drawn)
    int hovered{-1};   // the button under the laser
    int focused{-1};   // the button the sticks selected (-1: the menu has the selection)
    int focusMenu{m_none};
    glm::vec2 focusMouse{0.f}; // the laser's spot when they did: moving it on gives the selection back
};
Toolbar toolbar;

// The button at a spot of the menu (-1: none).
[[nodiscard]] int toolAt(float x, float y)
{
    if(toolbar.menu != m_state)
    {
        return -1;
    }
    const ToolbarLayout l = toolbarLayout();
    for(int t = 0; t < ToolCount; t++)
    {
        if(l.hit(t, x, y))
        {
            return t;
        }
    }
    return -1;
}

void focusTool(int tool)
{
    toolbar.focused = tool;
    toolbar.focusMenu = m_state;
    toolbar.focusMouse = {m_mousex, m_mousey};
}

// The page "Back to game" left (m_none: none), to open again.
struct Remembered
{
    int state{m_none};
    int vrPage{0};
    bool list{false}; // a list's selection and scroll
    int cursor{0};
    int scroll{0};
};
Remembered remembered;

// The menu to reopen for `state`: the one under a dialog (the skill and quit menus: the menu they
// came from; a mod's details: the mods), the first page of a flow that cannot be resumed on its own
// (the multiplayer game setup, searches and server lists: the multiplayer menu); m_none for the
// main menu (nothing to remember) or none.
[[nodiscard]] int reopenable(int state)
{
    if(state == m_skill)
    {
        state = m_skill_prevmenu;
    }
    else if(state == m_quit)
    {
        state = m_quit_prevstate;
    }

    switch(state)
    {
        case m_modinfo: return m_mods;
        case m_calibration: return m_gamepad;
        case m_lanconfig:
        case m_gameoptions:
        case m_search:
        case m_slist: return m_multiplayer;
        case m_main:
        case m_skill:
        case m_quit: return m_none;
        default: return state;
    }
}

void haptic(int hand, float seconds, float amplitude)
{
    if(Backend* be = backend(); be && !vr_disablehaptics.value)
    {
        be->haptic(hand, seconds, 200.f, amplitude);
    }
}

// ----------------------------------------------------------------------------
// Scrolling with the stick
// ----------------------------------------------------------------------------

// Scrolls the current menu's list by `rows` (0: only whether it can).
[[nodiscard]] bool scrollMenu(int rows)
{
    return m_state == m_vr ? menu::scroll(rows) : M_ScrollList(rows) != 0;
}

// The laser's strips, each eye (the main thread).
struct MenuUiScratch
{
    std::vector<gfx::Vertex> laser;
    auto members() { return std::tie(laser); }
};
mem::Scratch<MenuUiScratch> scratch{"menu laser"};

} // namespace

namespace qvr::menuui
{

bool active()
{
    return vr_menu_vr_style.value && vrActive() && key_dest == key_menu && m_state != m_none;
}

float panelHeight()
{
    return active() ? vid.guiheight / canvasScale() * vr_menu_scale.value : 0.f;
}

int menuHeight()
{
    return active() ? heightSetting() : 200;
}

void update(const hands::State& s)
{
    const bool on = active(); // (the laser meets the panel in the eyes, or the runtime's: intersect)
    for(int h = 0; h < HAND_COUNT; h++)
    {
        hits[h] = on ? intersect(s, h) : Hit{};
    }

    if(on && hits[pointingHand].valid)
    {
        moveMouse(hits[pointingHand], false, mouseHeld[pointingHand]);

        Backend* be = backend();
        if(drawnHighlight != seenHighlight && drawnHighlight.menu == seenHighlight.menu && be && !vr_disablehaptics.value)
        {
            be->haptic(pointingHand, 0.01f, 200.f, 0.12f);
        }
    }
    seenHighlight = drawnHighlight;

    // The corner's buttons light up under the laser, with a tick.
    const int hovered = on && hits[pointingHand].valid ? toolAt(m_mousex, m_mousey) : -1;
    if(hovered >= 0 && hovered != toolbar.hovered)
    {
        haptic(pointingHand, 0.01f, 0.15f);
    }
    toolbar.hovered = hovered;

    // The sticks' selection on them lasts while the menu stays and the laser is not moved on (a
    // hand's tremor aside).
    if(toolbar.focused >= 0 &&
        (!active() || m_state != toolbar.focusMenu || M_WaitingForKeyBinding() ||
            (on && hits[pointingHand].valid && glm::distance(glm::vec2{m_mousex, m_mousey}, toolbar.focusMouse) > 12.f)))
    {
        toolbar.focused = -1;
    }
}

void mockLaser_f()
{
    if(Cmd_Argc() == 2 && !q_strcasecmp(Cmd_Argv(1), "off"))
    {
        mockLaser.on = false;
        return;
    }
    if(Cmd_Argc() == 2)
    {
        for(int t = 0; t < ToolCount; t++)
        {
            static constexpr const char* names[ToolCount]{"back", "advanced", "levels", "checklist"};
            if(!q_strcasecmp(Cmd_Argv(1), names[t]))
            {
                const ToolbarLayout l = toolbarLayout();
                mockLaser = {true, {(l.x0 + l.x1) * 0.5f, l.yc(t)}};
                pointingHand = HAND_MAIN;
                return;
            }
        }
    }
    if(Cmd_Argc() == 3)
    {
        mockLaser = {true, {Q_atof(Cmd_Argv(1)), Q_atof(Cmd_Argv(2))}};
        pointingHand = HAND_MAIN;
        return;
    }
    Con_Printf("vr_mock_laser <x> <y> | back | advanced | levels | checklist | off: the main hand's laser on that spot of the menu\n");
}

void printLaser()
{
    const Hit& h = hits[pointingHand];
    Pose panel;
    glm::vec2 size{0.f};
    Backend* be = backend();
    const bool runtime = be && be->runtimePanel(panel, size);
    const glm::vec2 uv = h.valid ? h.uv : runtimePanelUv[pointingHand];
    Con_Printf("menu_vr pos: laser (%s hand) %s, at %.3f %.3f of the panel%s\n", pointingHand == HAND_MAIN ? "main" : "off",
        !h.valid ? "off the menu" : h.onRuntimePanel ? "on the runtime's panel" : "on the panel in the eyes", uv.x, uv.y,
        runtime ? " (the runtime's panel shown)" : "");
}

float toolbarBottom()
{
    return active() ? toolbarLayout().bottom() : -1e9f;
}

bool toolbarFocused()
{
    return active() && toolbar.focused >= 0 && toolbar.focusMenu == m_state;
}

void focusToolbar(int dir)
{
    focusTool(dir > 0 ? ToolBack : ToolCount - 1);
    S_LocalSound("misc/menu1.wav");
}

void backToGame(int hand)
{
    if(key_dest != key_menu || m_state == m_none)
    {
        return;
    }

    remembered = {};
    remembered.state = reopenable(m_state);
    if(remembered.state == m_vr)
    {
        remembered.vrPage = menu::currentPage();
    }
    else if(remembered.state == m_state)
    {
        remembered.list = M_ListPosition(&remembered.cursor, &remembered.scroll, false);
    }

    // As Quake's main menu leaves for the game.
    IN_Activate();
    key_dest = key_game;
    m_state = m_none;
    haptic(hand, 0.04f, 0.4f);
}

// A corner button pressed (`hand` its pulse). The jumps go where the menus' own links go, so Back
// from there walks up the menus as always (Advanced VR Options: to the VR Settings; the levels: to
// Single Player), never round in a loop; the page left keeps its selection and scroll for when it is
// shown again.
void useTool(int tool, int hand)
{
    toolbar.focused = -1;
    switch(tool)
    {
        case ToolBack: backToGame(hand); break;
        case ToolAdvanced: menu::jumpToAdvanced(); break;
        case ToolLevels:
            if(m_state == m_maps)
            {
                S_LocalSound("misc/menu1.wav");
                break;
            }
            Cmd_TokenizeString("menu_maps"); // (it looks at its command's arguments)
            M_Menu_Maps_f();
            break;
        case ToolChecklist: menu::jumpToChecklist(); break;
        default: break;
    }
}

bool scrollStick(float y)
{
    static bool pushed = false;
    static double carry = 0.0;
    static double last = 0.0;
    const double dt = realtime - last;
    last = realtime;

    if(key_dest != key_menu || !scrollMenu(0))
    {
        pushed = false;
        return false;
    }

    // Past a dead zone, 3 rows a second up to 25 at full push; the first row at once.
    constexpr float deadzone = 0.2f;
    const float t = (std::fabs(y) - deadzone) / (1.f - deadzone);
    if(t <= 0.f)
    {
        pushed = false;
        return true;
    }
    if(!pushed || dt > 0.25)
    {
        pushed = true;
        carry = 1.0;
    }
    else
    {
        carry += (3.0 + 22.0 * t * t) * dt;
    }

    const int rows = static_cast<int>(carry);
    carry -= rows;
    if(rows > 0 && scrollMenu(y > 0.f ? -rows : rows))
    {
        haptic(HAND_MAIN, 0.005f, 0.06f);
    }
    return true;
}

int triggerKey(int hand, bool down, int key)
{
    if(!down)
    {
        if(mouseHeld[hand])
        {
            mouseHeld[hand] = false;
            return K_MOUSE1;
        }
        return key;
    }

    // Binding a key takes the trigger as itself.
    if(!active() || M_WaitingForKeyBinding())
    {
        return key;
    }

    pointingHand = hand;
    if(!hits[hand].valid)
    {
        return key;
    }
    moveMouse(hits[hand], true, true); // the click lands where it points now
    mouseHeld[hand] = true;
    return K_MOUSE1;
}

void drawInEye(const hands::State& s)
{
    if(!active() || !s.valid)
    {
        return;
    }

    // At the pose the eyes are drawn with.
    const int h = pointingHand;
    glm::vec3 start, dir;
    if(!pointerRay(s, h, start, dir))
    {
        return;
    }
    const Hit hit = intersect(s, h);
    const glm::vec3 end = hit.valid ? hit.point : start + dir * 30.f;

    std::vector<gfx::Vertex>& vertices = scratch.laser;
    vertices.clear();
    const float bright = mouseHeld[h] ? 1.f : 0.8f;
    // In the player's hue (vr_menu_laser_hue; 35 its old amber, the menus' own).
    appendStrip(vertices, start, end, 0.3f, hue::color(vr_menu_laser_hue, 0.6f, 1.f, 0.9f * bright),
        hue::color(vr_menu_laser_hue, 0.5f, 1.f, hit.valid ? 0.5f * bright : 0.f));
    if(hit.valid)
    {
        appendStrip(vertices, hit.point, hit.point, 1.6f, hue::color(vr_menu_laser_hue, 0.3f, 1.f, 1.f), {});
        appendStrip(vertices, hit.point, hit.point, 0.7f, {1.f, 1.f, 1.f, 1.f}, {});
    }

    gfx::draw(vertices, gfx::sceneViewProjection(),
        {.shade = gfx::Shade::SoftEdge, .blend = gfx::Blend::Alpha, .depthTest = false, .depthWrite = false});
}

} // namespace qvr::menuui

// ----------------------------------------------------------------------------
// Engine hooks
// ----------------------------------------------------------------------------

extern "C" int VR_MenuCanvas(float* scalex, float* scaley)
{
    if(!menuui::active())
    {
        return 0;
    }
    const float s = canvasScale();
    *scalex = s;
    *scaley = s * rowSpacing();
    return 1;
}

namespace
{

// Quake's slider: 10 cells from x, the thumb's middle at x + 4 .. x + 76 (as the menus' mouse
// code maps it), the value's text at x + 96. A value past an end (`past` -1 left, 1 right): the
// thumb at that end in another colour, and an arrow just outside it (between the bar and the text,
// or between the row's cursor and the bar).
void drawStyledSlider(int x, int y, float range, float marker, int past, const char* desc)
{
    const Painter p;
    const float yc = y + 4.f;
    const float thumb = x + 4.f + 72.f * (past ? (past > 0 ? 1.f : 0.f) : CLAMP(0.f, range, 1.f));

    p.rounded(x - 1.f, x + 81.f, yc, 1.75f, 1.75f, colors::track);
    p.rounded(x - 1.f, thumb, yc, 1.75f, 1.75f, colors::fill);
    if(marker >= 0.f)
    {
        const float m = x + 4.f + 72.f * CLAMP(0.f, marker, 1.f);
        p.rect(m - 0.6f, m + 0.6f, yc, 4.f, colors::marker);
    }
    p.disc(thumb, yc, 4.5f, past ? colors::thumbRingPast : colors::thumbRing);
    p.disc(thumb, yc, 3.25f, past ? colors::thumbPast : colors::thumb);
    if(past > 0)
    {
        p.arrowHeadRight(x + 89.f, 5.f, yc, 3.f, colors::thumbPast);
    }
    else if(past < 0)
    {
        p.arrowHead(x - 9.f, 5.f, yc, 3.f, colors::thumbPast);
    }

    if(x + 96 + 5 * 8 < glcanvas.right)
    {
        M_Print(x + 96, y, desc);
    }
}

} // namespace

bool qvr::menuui::drawSlider(int x, int y, float range, int past, const char* desc)
{
    if(!styled())
    {
        return false;
    }
    drawStyledSlider(x, y, range, -1.f, past, desc);
    return true;
}

extern "C" int VR_MenuDrawSlider(int x, int y, float range, float marker, const char* desc)
{
    if(!styled())
    {
        return 0;
    }
    drawStyledSlider(x, y, range, marker, 0, desc);
    return 1;
}

extern "C" int VR_MenuDrawCheckbox(int x, int y, int on)
{
    if(!styled())
    {
        return 0;
    }

    const Painter p;
    const float yc = y + 4.f;
    p.rounded(x, x + 19.f, yc, 4.25f, 4.25f, on ? colors::fill : colors::switchOff);
    p.disc(on ? x + 14.75f : x + 4.25f, yc, 3.f, on ? colors::thumb : colors::knobOff);
    return 1;
}

// Quake's box: a border 8 pixels wide round `width` (rounded up to even) characters across and
// `lines` rows, from x, y.
extern "C" int VR_MenuDrawTextBox(int x, int y, int width, int lines)
{
    if(!styled())
    {
        return 0;
    }

    const Painter p;
    const float inner = ((width + 1) / 2) * 16.f;

    if(width <= 0)
    {
        // A list's scrollbar thumb.
        const float yc = y + 4.f * (lines + 2);
        const float half = 4.f * (lines + 2) * p.k - 1.5f;
        p.rounded(x + 6.f, x + 10.f, yc, half, 2.f, colors::scrollThumb);
        return 1;
    }

    // Round its rows' characters (their middles 8 apart in the spaced coordinates).
    const float yc = y + 8.f + 4.f * lines;
    const float half = 4.f * (lines - 1) * p.k + 4.f + 3.5f;
    const float x0 = x + 8.f - 5.f;
    const float x1 = x + 8.f + inner + 5.f;
    p.rounded(x0, x1, yc, half, 3.f, colors::boxBorder);
    p.rounded(x0 + 1.f, x1 - 1.f, yc, half - 1.f, 2.f, colors::boxFill);
    return 1;
}

// The arrow cursor's row, across the menu: from the arrow (or the menu's left, where the row's
// label is), as far to the other side of the menu's middle. Nonzero: the menu's cursor is not drawn
// at all (the sticks' selection is on the corner's buttons).
extern "C" int VR_MenuDrawHighlight(int cx, int cy)
{
    if(!styled())
    {
        return 0;
    }
    if(menuui::toolbarFocused())
    {
        return 1;
    }

    drawnHighlight = {m_state, cy};

    const Painter p;
    const float left = std::fmin(cx - 4.f, 8.f);
    const float right = 320.f - left;
    const float yc = cy + 4.f;
    p.rounded(left, right, yc, 5.5f, 2.f, colors::highlight);
    p.rect(left, left + 1.5f, yc, 4.5f, colors::highlightEdge);
    return 0;
}

// The vertical Quake plaque on the options pages: with the VR style's taller panel the rows reach
// down past it, so it is left out (the corner's buttons stand by the title instead).
extern "C" int VR_MenuHidesPlaque()
{
    return qvr::menuui::active();
}

// The menus that lay out from the canvas's bounds (Ironwail's lists: levels, mods, options, key
// bindings) start below the corner's buttons.
extern "C" void VR_MenuBounds(int* top, int* height)
{
    const float bottom = menuui::toolbarBottom();
    const int below = static_cast<int>(std::ceil(bottom)) + 4;
    if(below <= *top)
    {
        return;
    }
    const int end = *top + *height;
    *top = below;
    *height = q_max(end - below, 0) & ~7;
}

namespace
{

// A button's icon, `x` its left, `yc` its middle: Back to game's arrow, Advanced VR's sliders, the
// levels' flag, the checklist's lines.
void drawToolIcon(const Painter& p, int tool, float x, float yc, const glm::vec4& ink)
{
    const float w = ToolbarLayout::icon;
    switch(tool)
    {
        case ToolBack:
            p.arrowHead(x, 5.f, yc, 4.5f, ink);
            p.rect(x + 4.f, x + w, yc, 1.25f, ink);
            break;
        case ToolAdvanced:
            // Three sliders, their knobs set apart.
            for(int i = 0; i < 3; i++)
            {
                const float y = yc + (i - 1) * 3.5f / p.k;
                const float knob = x + (i == 0 ? 2.5f : i == 1 ? 6.5f : 4.f);
                p.rect(x, x + w, y, 0.6f, ink);
                p.rect(knob - 1.f, knob + 1.f, y, 1.5f, ink);
            }
            break;
        case ToolLevels:
            // A flag on its pole.
            p.band(x + 1.f, x + 2.4f, yc, -4.5f, 4.5f, ink);
            p.arrowHeadRight(x + w, w - 2.4f, yc - 2.f / p.k, 2.6f, ink);
            break;
        case ToolChecklist:
            // Three lines, each after a box.
            for(int i = 0; i < 3; i++)
            {
                const float y = yc + (i - 1) * 3.5f / p.k;
                p.rect(x, x + 2.f, y, 1.f, ink);
                p.rect(x + 3.5f, x + w, y, 0.6f, ink);
            }
            break;
        default: break;
    }
}

} // namespace

// The corner's buttons (over every menu, not while a key is being bound): their labels where they fit
// left of Quake's plaque (x 16), else only their icons.
extern "C" void VR_MenuDrawOverlay()
{
    toolbar.menu = m_none;
    if(!styled() || M_WaitingForKeyBinding())
    {
        return;
    }

    const Painter p;
    const ToolbarLayout l = toolbarLayout();
    checklist::refresh(); // (the file looked at once a second)
    char checklistLabel[16];
    const int open = checklist::openCount();
    if(open > 0)
    {
        q_snprintf(checklistLabel, sizeof(checklistLabel), "Checklist %d", q_min(open, 99));
    }
    else
    {
        q_strlcpy(checklistLabel, "Checklist", sizeof(checklistLabel));
    }
    for(int t = 0; t < ToolCount; t++)
    {
        const bool hot = toolbar.hovered == t || (menuui::toolbarFocused() && toolbar.focused == t);
        const float yc = l.yc(t);
        p.rounded(l.x0, l.x1, yc, ToolbarLayout::half, 3.f, hot ? colors::highlightEdge : colors::boxBorder);
        p.rounded(l.x0 + 1.f, l.x1 - 1.f, yc, ToolbarLayout::half - 1.f, 2.f, hot ? colors::buttonHover : colors::boxFill);

        const glm::vec4& ink = hot ? colors::thumb : colors::fill;
        const float ix = l.x0 + 4.f;
        drawToolIcon(p, t, ix, yc, ink);

        if(l.labels)
        {
            float x = ix + ToolbarLayout::icon + 4.f;
            for(const char* c = t == ToolChecklist ? checklistLabel : toolLabels[t]; *c; c++, x += 8.f)
            {
                Draw_CharacterEx(x, yc - 4.f, 8.f, 8.f, hot ? *c : (*c | 128));
            }
        }
    }
    toolbar.menu = m_state;

    // On the runtime's panel (no world to draw the laser in), where the laser points: a dot in its hue.
    if(const Hit& hit = hits[pointingHand]; hit.valid && hit.onRuntimePanel)
    {
        p.disc(m_mousex, m_mousey, 3.f, hue::color(vr_menu_laser_hue, 0.3f, 1.f, 1.f));
        p.disc(m_mousex, m_mousey, 1.4f, {1.f, 1.f, 1.f, 1.f});
    }
}

// The corner's buttons take the laser's clicks on them, and the sticks' keys while they have the
// selection: up and down move it (off the column's ends back to the menu: on a VR page, round to its
// other end), A or Enter presses, B gives it back; a click of either stick takes it (or gives it back).
extern "C" int VR_MenuKey(int key, int repeat)
{
    if(!menuui::active() || M_WaitingForKeyBinding())
    {
        toolbar.focused = -1;
        return 0;
    }

    if(key == K_MOUSE1)
    {
        const int t = toolAt(m_mousex, m_mousey);
        if(t < 0)
        {
            toolbar.focused = -1;
            return 0;
        }
        menuui::useTool(t, pointingHand);
        return 1;
    }

    if(key == K_LTHUMB || key == K_RTHUMB)
    {
        if(repeat)
        {
            return 1;
        }
        if(menuui::toolbarFocused())
        {
            toolbar.focused = -1;
        }
        else
        {
            focusTool(ToolBack);
        }
        S_LocalSound("misc/menu1.wav");
        return 1;
    }

    if(!menuui::toolbarFocused())
    {
        toolbar.focused = -1;
        return 0;
    }

    switch(key)
    {
        case K_UPARROW:
        case K_DOWNARROW:
        {
            const int dir = key == K_DOWNARROW ? 1 : -1;
            const int next = toolbar.focused + dir;
            if(next >= 0 && next < ToolCount)
            {
                toolbar.focused = next;
            }
            else if(repeat)
            {
                return 1; // a held stick stops at the column's end: a new push leaves it
            }
            else
            {
                toolbar.focused = -1;
                if(m_state == m_vr)
                {
                    menu::selectEnd(dir); // on round the page: its first setting below, its last above
                }
            }
            S_LocalSound("misc/menu1.wav");
            return 1;
        }
        case K_ENTER:
        case K_KP_ENTER:
        case K_ABUTTON: menuui::useTool(toolbar.focused, HAND_MAIN); return 1;
        case K_ESCAPE:
        case K_BBUTTON:
        case K_MOUSE2:
        case K_MOUSE4:
            toolbar.focused = -1;
            S_LocalSound("misc/menu1.wav");
            return 1;
        case K_LEFTARROW:
        case K_RIGHTARROW: return 1; // nothing to change here (and nothing under it changed)
        default:
            toolbar.focused = -1; // another key: the menu's again
            return 0;
    }
}

// Opening the menu: the page "Back to game" left, over the main menu (where pages that go back
// where they came from lead, and what is left when a page cannot open now: saving outside a single
// player game). Once: the next time, the main menu again unless it closed that way again.
extern "C" int VR_MenuReopen()
{
    const Remembered r = remembered;
    remembered = {};
    if(!vr_menu_remember.value || !vrActive() || r.state == m_none)
    {
        return 0;
    }

    M_Menu_Main_f();
    switch(r.state)
    {
        case m_singleplayer: M_Menu_SinglePlayer_f(); break;
        case m_load: M_Menu_Load_f(); break;
        case m_save: M_Menu_Save_f(); break;
        case m_maps:
            Cmd_TokenizeString("menu_maps"); // it looks at its command's arguments
            M_Menu_Maps_f();
            break;
        case m_multiplayer: M_Menu_MultiPlayer_f(); break;
        case m_setup: M_Menu_Setup_f(); break;
        case m_net: M_Menu_Net_f(); break;
        case m_options:
        case m_video:
        case m_graphics:
        case m_interface:
        case m_game:
        case m_gamepad: M_Options_Init(static_cast<m_state_e>(r.state)); break;
        case m_keys: M_Menu_Keys_f(); break;
        case m_mods: M_Menu_Mods_f(); break;
        case m_help: M_Menu_Help_f(); break;
        case m_vr: menu::reopen(r.vrPage); break;
        default: break;
    }

    if(r.list && m_state == r.state)
    {
        int cursor = r.cursor;
        int scroll = r.scroll;
        M_ListPosition(&cursor, &scroll, true);
    }
    return 1;
}

// Live preview (Ironwail's ui_live_preview, on by default): in VR, a single player game keeps
// running under the settings pages (the options and their pages, the VR Settings), so that what
// they change can be seen in motion (water, lights, effects). The main menu and the others still
// pause it, and so do the settings pages while a monster is after the player: the menu is still a
// safe pause in a fight. (Lava, slime and drowning go on hurting under it.)
extern "C" cvar_t ui_live_preview; // menu.c

namespace
{

[[nodiscard]] bool monsterHunting()
{
    const edict_t* player = svs.clients[0].edict;
    if(!player)
    {
        return false;
    }
    const int target = EDICT_TO_PROG(player);
    for(int i = svs.maxclients + 1; i < qcvm->num_edicts; i++)
    {
        const edict_t* e = EDICT_NUM(i);
        if(!e->free && (static_cast<int>(e->v.flags) & FL_MONSTER) && e->v.health > 0.f && e->v.enemy == target)
        {
            return true;
        }
    }
    return false;
}

} // namespace

extern "C" int VR_MenuRunsGame()
{
    if(!ui_live_preview.value || !vrActive() || key_dest != key_menu || !sv.active || svs.maxclients != 1 ||
        cl.intermission)
    {
        return 0;
    }
    switch(m_state)
    {
        case m_options:
        case m_video:
        case m_graphics:
        case m_interface:
        case m_game:
        case m_gamepad:
        case m_vr: return !monsterHunting();
        default: return 0;
    }
}
