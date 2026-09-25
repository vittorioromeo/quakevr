// vr_menuui.cpp -- the VR menu style (vr_menu_vr_style): Ironwail's menus, and the port's own VR
// pages, as they are, made for a headset by what draws them rather than by menus of their own.
//
// - The menu canvas (CANVAS_MENU) fills the panel's height, whatever scr_menuscale says, and
//   scales y by vr_menu_spacing more than x (VR_MenuCanvas): every row the menus lay out 8 pixels
//   apart comes out further apart, and hit tests, which go through the same transform (the menus'
//   mouse code), follow. The 2D layer draws characters and pictures at their own size there,
//   centred on where they were (gl_draw.c, Draw_KeepMenuGlyphSize), so the text is not stretched.
//   Menus drawn from pictures with a cursor stepping over them (the main, single player and
//   multiplayer menus...) keep Quake's spacing: their rows are already 20 pixels apart.
// - The widgets are drawn anew (the menus call Quake's M_Draw* functions, which hand over): the
//   slider is a track filled up to a round thumb, the checkbox a switch, the text box a panel with
//   a thin border (and a list's scrollbar thumb a pill), and the selected row gets a highlight bar.
// - A laser from the pointing hand (the main hand, or the one whose trigger was pressed last)
//   meets the panel: that spot is the menu's mouse (M_Mousemove), so rows under it are selected as
//   with a mouse, and the trigger is the left mouse button there: it picks an item, sets a slider
//   where it points and drags it, drags a list's scrollbar. The sticks, A and B work as
//   before.
// - "Back to game": a button at the panel's top left (or holding the menu button, vr_input.cpp)
//   closes the menu from whatever page it is on; opening it again (the menu button, Escape,
//   togglemenu) returns to that page, its selection and scroll as they were (vr_menu_remember).
//   Only that way of closing it is remembered: Escape from the main menu, or a menu closing
//   because a game started or loaded, opens the main menu next time, as Quake does.
// - The main hand's stick scrolls a page with a scrollbar (the VR pages, and Ironwail's lists: the
//   options, maps, mods, key bindings), a row at a time at a rate growing with the push, the
//   selection kept where it is while it stays in view.

#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_gfx.hpp"
#include "vr_main.hpp"
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

// Canvas (2D) units per menu pixel across: the 320 x 200 menu, its rows spaced out as much as they
// can be, fits the canvas; the same for every menu, so that the text is the same size in all.
[[nodiscard]] float canvasScale()
{
    return std::fmin(vid.guiwidth / 320.f, vid.guiheight / (200.f * spacingSetting()));
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
};

// ----------------------------------------------------------------------------
// The pointer
// ----------------------------------------------------------------------------

struct Hit
{
    bool valid{false};
    glm::vec3 point{0.f};
    glm::vec2 uv{0.f}; // on the canvas, v up
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

// A hand's pointing ray in the world: the controller as tracked, aimed as a gun is (vr_gunangle,
// vr_offhandpitch), placed from the head. Not the hands' state's pose, which follows a held
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
    const float pitch = hand == HAND_MAIN ? vr_gunangle.value : vr_offhandpitch.value;
    const float yaw = hand == HAND_MAIN ? vr_gunyaw.value : vr_offhandyaw.value;
    const glm::quat aim = t.hands[hand].orientation * glm::angleAxis(glm::radians(yaw), glm::vec3{0.f, 1.f, 0.f}) *
                          glm::angleAxis(glm::radians(-pitch), glm::vec3{1.f, 0.f, 0.f});

    origin = s.head + hands::rotateYaw(quakeFromTracking(t.hands[hand].position - t.head.position) * units::metresToUnits(), turn);
    dir = glm::normalize(hands::rotateYaw(quakeFromTracking(aim * glm::vec3{0.f, 0.f, -1.f}), turn));
    return true;
}

[[nodiscard]] Hit intersect(const hands::State& s, int hand)
{
    Hit hit;
    glm::vec3 corner, xAxis, yAxis, origin, dir;
    if(!pointerRay(s, hand, origin, dir) || !panel::menuQuad(s, corner, xAxis, yAxis))
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
// Back to game
// ----------------------------------------------------------------------------

// The button as last drawn: for which menu, and where (menu coordinates, y down).
struct BackButton
{
    int menu{m_none};
    float x0{0.f}, y0{0.f}, x1{0.f}, y1{0.f};
    bool hovered{false}; // by the laser
};
BackButton backButton;

[[nodiscard]] bool overBackButton(float x, float y)
{
    const BackButton& b = backButton;
    return b.menu == m_state && x >= b.x0 && x <= b.x1 && y >= b.y0 && y <= b.y1;
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

void update(const hands::State& s)
{
    const bool on = active() && s.valid;
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

    // The "Back to game" button lights up under the laser, with a tick.
    const bool hovered = on && hits[pointingHand].valid && overBackButton(m_mousex, m_mousey);
    if(hovered && !backButton.hovered)
    {
        haptic(pointingHand, 0.01f, 0.15f);
    }
    backButton.hovered = hovered;
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

    static std::vector<gfx::Vertex> vertices;
    vertices.clear();
    const float bright = mouseHeld[h] ? 1.f : 0.8f;
    appendStrip(vertices, start, end, 0.3f, {1.f, 0.75f, 0.4f, 0.9f * bright},
        {1.f, 0.8f, 0.5f, hit.valid ? 0.5f * bright : 0.f});
    if(hit.valid)
    {
        appendStrip(vertices, hit.point, hit.point, 1.6f, {1.f, 0.9f, 0.7f, 1.f}, {});
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

// Quake's slider: 10 cells from x, the thumb's middle at x + 4 .. x + 76 (as the menus' mouse
// code maps it), the value's text at x + 96.
extern "C" int VR_MenuDrawSlider(int x, int y, float range, float marker, const char* desc)
{
    if(!styled())
    {
        return 0;
    }

    const Painter p;
    const float yc = y + 4.f;
    const float thumb = x + 4.f + 72.f * CLAMP(0.f, range, 1.f);

    p.rounded(x - 1.f, x + 81.f, yc, 1.75f, 1.75f, colors::track);
    p.rounded(x - 1.f, thumb, yc, 1.75f, 1.75f, colors::fill);
    if(marker >= 0.f)
    {
        const float m = x + 4.f + 72.f * CLAMP(0.f, marker, 1.f);
        p.rect(m - 0.6f, m + 0.6f, yc, 4.f, colors::marker);
    }
    p.disc(thumb, yc, 4.5f, colors::thumbRing);
    p.disc(thumb, yc, 3.25f, colors::thumb);

    if(x + 96 + 5 * 8 < glcanvas.right)
    {
        M_Print(x + 96, y, desc);
    }
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
// label is), as far to the other side of the menu's middle.
extern "C" void VR_MenuDrawHighlight(int cx, int cy)
{
    if(!styled())
    {
        return;
    }

    drawnHighlight = {m_state, cy};

    const Painter p;
    const float left = std::fmin(cx - 4.f, 8.f);
    const float right = 320.f - left;
    const float yc = cy + 4.f;
    p.rounded(left, right, yc, 5.5f, 2.f, colors::highlight);
    p.rect(left, left + 1.5f, yc, 4.5f, colors::highlightEdge);
}

// "Back to game": a button in the panel's top-left corner, over every menu (not while a key is
// being bound). Its label shows where it fits left of Quake's plaque (x 16), else only the arrow.
extern "C" void VR_MenuDrawOverlay()
{
    backButton.menu = m_none;
    if(!styled() || M_WaitingForKeyBinding())
    {
        return;
    }

    const Painter p;
    const bool hot = backButton.hovered;
    constexpr const char* label = "Back to game";
    constexpr float corner = 4.f; // true pixels from the panel's edges
    constexpr float half = 8.f;   // half its height
    constexpr float arrow = 9.f;
    const float labelWidth = 8.f * static_cast<float>(strlen(label));

    const float x0 = glcanvas.left + corner;
    const bool withLabel = x0 + 4.f + arrow + 4.f + labelWidth + 5.f <= 16.f;
    const float x1 = x0 + (withLabel ? 4.f + arrow + 4.f + labelWidth + 5.f : 4.f + arrow + 4.f);
    const float yc = glcanvas.top + (corner + half) / p.k;

    p.rounded(x0, x1, yc, half, 3.f, hot ? colors::highlightEdge : colors::boxBorder);
    p.rounded(x0 + 1.f, x1 - 1.f, yc, half - 1.f, 2.f, hot ? colors::buttonHover : colors::boxFill);

    // A left arrow.
    const glm::vec4& ink = hot ? colors::thumb : colors::fill;
    const float ax = x0 + 4.f;
    p.arrowHead(ax, 5.f, yc, 4.5f, ink);
    p.rect(ax + 4.f, ax + arrow, yc, 1.25f, ink);

    if(withLabel)
    {
        float x = ax + arrow + 4.f;
        for(const char* c = label; *c; c++, x += 8.f)
        {
            Draw_CharacterEx(x, yc - 4.f, 8.f, 8.f, hot ? *c : (*c | 128));
        }
    }

    // It takes the clicks as far as the panel's corner, and a little round it.
    backButton = {m_state, glcanvas.left, glcanvas.top, x1 + 2.f, yc + (half + 2.f) / p.k, hot};
}

extern "C" int VR_MenuClick()
{
    if(!menuui::active() || !overBackButton(m_mousex, m_mousey))
    {
        return 0;
    }
    menuui::backToGame(pointingHand);
    return 1;
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
