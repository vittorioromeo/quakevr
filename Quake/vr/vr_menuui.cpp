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
// - Under it, "Quicksave" and "Quickload" use the keyboard bindings' quick slot; "Advanced VR" and
//   "Levels" jump to their pages from any menu. The laser clicks the shortcuts; the sticks reach them too: a stick's
//   click on any menu, or up from a VR page's first setting (down from its last), then up and down, A to press, B back.
// - The main hand's stick scrolls a page with a scrollbar (the VR pages, and Ironwail's lists: the
//   options, maps, mods, key bindings), a row at a time at a rate growing with the push, the
//   selection kept where it is while it stays in view. It never changes a setting: its left and
//   right do nothing in menus, only the off hand's stick (and the laser) change values
//   (vr_input.cpp).
// - On a flat screen with vr_menu_flat_vr_style the same layout draws on a panel in the window (FlatPanel: the menu
//   canvas's viewport), the desktop mouse as the laser; its corners dragged move it, the wheel while dragging resizes
//   it. The "-" and "+" buttons under the corner's buttons zoom the menu in every layout (zoomMenu).

#include "vr_checklist.hpp"
#include "vr_cvars.hpp"
#include "vr_tunables.hpp"
#include "vr_engine.hpp"
#include "vr_gfx.hpp"
#include "vr_hue.hpp"
#include "vr_main.hpp"
#include "vr_mem.hpp"
#include "vr_menu.hpp"
#include "vr_menuui.hpp"
#include "vr_menupaint.hpp"
#include "vr_ragtool.hpp"
#include "vr_obs.hpp"
#include "vr_renderdoc.hpp"
#include "vr_panel.hpp"
#include "vr_propose_defaults.hpp"
#include "vr_progs.hpp"
#include "vr_stereo.hpp"
#include "vr_protocol.hpp"
#include "vr_toolgun.hpp"
#include "vr_units.hpp"
#include "vr_window.hpp"

#include "Zancle/Base/Exchange.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Asin.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Ceil.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/Fmax.hpp"
#include "Zancle/Math/Fmin.hpp"
#include "Zancle/Math/Lround.hpp"
#include "Zancle/String/String.hpp"
#include "vr_api.h"
#include "vr_api_render.h"

#include <string.h>

extern "C" {
extern float m_mousex, m_mousey; // menu.c: the menus' mouse, in menu coordinates
extern qboolean keydown[MAX_KEYS]; // keys.c: the left button still held (the flat panel's corner dragged)
extern cvar_t ui_mouse;           // menu.c: the desktop mouse in the menus
int Key_StringToKeynum(const char* str); // keys.c: a key's number from its name (vr_mock_key)
void Char_Event(int key);                // keys.c: a letter typed (vr_mock_key text)
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
    return static_cast<int>(za::lround(200.f * CLAMP(1.f, vr_menu_height.value, 2.f) / 8.f)) * 8;
}

// The current menu's row spacing: 1 for the menus drawn from pictures, whose cursor steps over a
// picture's rows (spacing would move the cursor off them).
[[nodiscard]] float rowSpacing()
{
    // A confirmation dialog: as the menu under it (drawn under it, not laid out again).
    switch(m_state == m_confirm ? M_Confirm_PrevState() : m_state)
    {
        case m_main:
        case m_singleplayer:
        case m_multiplayer:
        case m_net:
        case m_skill:
        case m_help:
        case m_autosaves: // (two lines a row, laid out in its own 20-unit rows)
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
    return za::fmin(vid.guiwidth / 320.f, vid.guiheight / (heightSetting() * spacingSetting()));
}

// The styled widgets are drawn only into the menu canvas while it is the VR one.
[[nodiscard]] bool styled()
{
    return menuui::active() && glcanvas.type == CANVAS_MENU;
}

// ----------------------------------------------------------------------------
// Drawing, in menu coordinates
// ----------------------------------------------------------------------------

namespace colors = qvr::menupaint::colors;
using qvr::menupaint::Painter;


// ----------------------------------------------------------------------------
// The pointer
// ----------------------------------------------------------------------------

struct Hit
{
    bool valid{false};
    glm::vec3 point{0.f};
    glm::vec2 uv{0.f};           // on the canvas, v up
    bool onRuntimePanel{false}; // on the runtime's own panel (the menus before a map): no world to draw the laser in
    bool frame{false};          // not valid: just outside the panel, on its frame (a grip there moves it: PanelGrab)
    int menu{0};                // which menu's panel (menu::instanceCount: the nearest the ray meets)
};

// The panel's frame: a band this much of its height wide round it, where a grip takes hold of it.
constexpr float frameBand = 0.08f;

int pointingHand = HAND_MAIN;
Hit hits[HAND_COUNT];
bool mouseHeld[HAND_COUNT]{};
int heldMenu[HAND_COUNT]{}; // the menu a hand's trigger clicked (its release goes there)

// Each menu's mouse as the laser last moved it (window position, and the menus' mouse it made).
struct SentMouse
{
    glm::ivec2 window{-100000};
    glm::vec2 menu{0.f};
};
SentMouse sentMouse[menu::instanceCount];

// The selected row's highlight as last drawn (the menu, its y), and as the pointer last saw it: a
// tick in the pointing hand when the pointer moves the selection. Each menu panel's.
struct Highlight
{
    int menu{m_none};
    int y{0};
    bool operator==(const Highlight&) const = default;
};
Highlight drawnHighlight[menu::instanceCount], seenHighlight[menu::instanceCount];

// A hand's pointing ray in the world: the controller as tracked, aimed as a gun is (the hand calibration's pitch and
// yaw: vr_gunangle, vr_offhandpitch), placed from the head. Not the hands' state's pose, which follows a held
// weapon's weight in game time (frozen while a menu pauses the game).
// With its whole orientation (`basis`: the pointing direction, left and up as columns), for holding the panel.
[[nodiscard]] bool pointerFrame(const hands::State& s, int hand, glm::vec3& origin, glm::mat3& basis)
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
    const glm::vec3 fwd = glm::normalize(hands::rotateYaw(quakeFromTracking(aim * glm::vec3{0.f, 0.f, -1.f}), turn));
    const glm::vec3 up = glm::normalize(hands::rotateYaw(quakeFromTracking(aim * glm::vec3{0.f, 1.f, 0.f}), turn));
    basis = glm::mat3{fwd, glm::cross(up, fwd), up};
    return true;
}

[[nodiscard]] bool pointerRay(const hands::State& s, int hand, glm::vec3& origin, glm::vec3& dir)
{
    glm::mat3 basis;
    if(!pointerFrame(s, hand, origin, basis))
    {
        return false;
    }
    dir = basis[0];
    return true;
}

// vr_mock_laser (tests): the main hand's laser on a spot of the menu (menu coordinates), whatever
// the hand's pose.
struct MockLaser
{
    bool on{false};
    glm::vec2 spot{0.f};
    bool frame{false}; // (vr_mock_laser frame: on the panel's frame, above its top edge's middle)
    int menu{-1};      // on that menu's panel (vr_mock_laser ... panel <n>; -1: the focused one's as it was set)
};
MockLaser mockLaser; // (the main hand's; the toolgun's panel has its own: vr_toolgun_mock_laser)
MockLaser mockOffLaser; // (the off hand's: vr_mock_laser ... hand off, for both hands at once)

[[nodiscard]] MockLaser* mockFor(int hand)
{
    MockLaser* m = hand == HAND_MAIN ? &mockLaser : &mockOffLaser;
    return m->on ? m : nullptr;
}

// The mock laser's spot on the canvas (v up).
[[nodiscard]] glm::vec2 mockLaserUv(const MockLaser& m = mockLaser)
{
    drawtransform_t t;
    Draw_GetCanvasTransform(CANVAS_MENU, &t);
    return {(m.spot.x * t.scale[0] + t.offset[0] + 1.f) * 0.5f, (m.spot.y * t.scale[1] + t.offset[1] + 1.f) * 0.5f};
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
    if(za::fabs(along) < 1e-6f)
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

// A spot `uv` of a panel (corner, axes): on it, on its frame, or neither.
[[nodiscard]] Hit hitAt(const glm::vec3& p, const glm::vec2& uv, const glm::vec3& xAxis, const glm::vec3& yAxis, int menu)
{
    Hit hit;
    hit.menu = menu;
    if(uv.x >= 0.f && uv.x <= 1.f && uv.y >= 0.f && uv.y <= 1.f)
    {
        hit.valid = true;
        hit.point = p;
        hit.uv = uv;
        return hit;
    }

    // Outside it: on its frame?
    const float bandX = frameBand * glm::length(yAxis) / za::fmax(1e-3f, glm::length(xAxis));
    if(uv.x >= -bandX && uv.x <= 1.f + bandX && uv.y >= -frameBand && uv.y <= 1.f + frameBand)
    {
        hit.frame = true;
        hit.point = p;
        hit.uv = uv;
    }
    return hit;
}

// Where a hand's laser meets the menus' panels (several: MULTI_MENU.md): the nearest it is on, else the nearest whose
// frame it is on.
[[nodiscard]] Hit intersect(const hands::State& s, int hand)
{
    Hit hit;
    hit.menu = menu::currentInstance();
    glm::vec3 corner, xAxis, yAxis, origin, dir;
    if(!s.valid || !panel::menuQuad(s, 0, corner, xAxis, yAxis))
    {
        hit = intersectRuntimePanel(hand); // (the menu is not in the eyes: the focused menu's, the one drawn)
        hit.menu = menu::currentInstance();
        return hit;
    }
    if(const MockLaser* m = mockFor(hand))
    {
        // On its panel (the one it was set on, while open), wherever it is.
        const int target = m->menu >= 0 && menu::instanceOpen(m->menu) ? m->menu : 0;
        const int was = menu::currentInstance();
        menu::selectInstance(target); // (its menu's canvas transform)
        const glm::vec2 uv = m->frame ? glm::vec2{0.5f, 1.f + frameBand * 0.5f} : mockLaserUv(*m);
        menu::selectInstance(was);
        if(!panel::menuQuad(s, target, corner, xAxis, yAxis))
        {
            return hit;
        }
        const glm::vec3 p = corner + xAxis * uv.x + yAxis * uv.y;
        if(!m->frame)
        {
            hit.valid = true; // (wherever it is, as before)
            hit.point = p;
            hit.uv = uv;
            hit.menu = target;
            return hit;
        }
        return hitAt(p, uv, xAxis, yAxis, target);
    }
    if(!pointerRay(s, hand, origin, dir))
    {
        return hit;
    }

    float best = 0.f;
    for(int i = 0; i < menu::instanceCount; i++)
    {
        if(!panel::menuQuad(s, i, corner, xAxis, yAxis))
        {
            continue;
        }
        const glm::vec3 normal = glm::cross(xAxis, yAxis);
        const float along = glm::dot(dir, normal);
        if(za::fabs(along) < 1e-6f)
        {
            continue;
        }
        const float t = glm::dot(corner - origin, normal) / along;
        if(t <= 0.f)
        {
            continue;
        }
        const glm::vec3 p = origin + dir * t;
        const glm::vec2 uv{glm::dot(p - corner, xAxis) / glm::dot(xAxis, xAxis), glm::dot(p - corner, yAxis) / glm::dot(yAxis, yAxis)};
        const Hit h = hitAt(p, uv, xAxis, yAxis, i);
        const bool better = (h.valid && (!hit.valid || t < best)) || (h.frame && !hit.valid && (!hit.frame || t < best));
        if(better)
        {
            hit = h;
            best = t;
        }
    }
    return hit;
}

// ----------------------------------------------------------------------------
// Moving the panel (MULTI_MENU.md, stage 1): a grip with the laser on the panel's frame takes hold of it at once; on its
// face (vr_menu_grab 2), once the hand has moved vr_menu_grab_deadzone cm with the grip held, the trigger not held and
// no stick pushed (else the grip is the sliders' fine modifier, as before). Held, the panel keeps where it is from the
// hand's laser (moved and turned with it, kept upright); let go, it stays there while the menus are open.
// ----------------------------------------------------------------------------

struct PanelGrab
{
    int hand{-1};                       // the hand holding the panel (-1: none)
    int menu{0};                        // which menu's panel (menu::currentInstance when taken)
    bool pressTaken[HAND_COUNT]{};      // the grip's press took hold (its key not sent): its release is the grab's too
    int armedHand{-1};                  // a grip pressed on the panel's face, not yet moved
    int armedMenu{0};                   // on that menu's panel
    glm::vec3 armedAt{0.f};             // where that hand was then
    glm::vec3 armedSpot{0.f};           // and its laser on the panel
    bool sticksMoved{false};            // a stick was pushed while it was armed: the fine modifier, not a grab
    glm::vec3 centre{0.f}, right{0.f}, up{0.f}, spot{0.f}; // the panel and the laser's spot, in the hand's frame
    glm::vec3 dirAtTake{1.f, 0.f, 0.f}; // from the head to the panel's middle when taken
    glm::vec3 handFromHead{0.f};        // the hand's place from the head then
    float distAtTake{0.f};              // the panel's middle's distance from the head then (units)
};
PanelGrab panelGrab;


[[nodiscard]] int grabMode()
{
    return static_cast<int>(vr_menu_grab.value);
}

// The panel taken by `hand`, its laser at `spot` (world).
[[nodiscard]] bool takeHold(const hands::State& s, int hand, int menuIndex, const glm::vec3& spot)
{
    glm::vec3 origin, corner, xAxis, yAxis;
    glm::mat3 basis;
    if(!pointerFrame(s, hand, origin, basis) || !panel::menuQuad(s, menuIndex, corner, xAxis, yAxis))
    {
        return false;
    }
    const glm::mat3 toHand = glm::transpose(basis);
    panelGrab.hand = hand;
    panelGrab.menu = menuIndex;
    panelGrab.centre = toHand * (corner + (xAxis + yAxis) * 0.5f - origin);
    panelGrab.right = toHand * glm::normalize(xAxis);
    panelGrab.up = toHand * glm::normalize(yAxis);
    panelGrab.spot = toHand * (spot - origin);
    const glm::vec3 toPanel = corner + (xAxis + yAxis) * 0.5f - s.head;
    panelGrab.distAtTake = glm::length(toPanel);
    panelGrab.dirAtTake = panelGrab.distAtTake > 1.f ? toPanel / panelGrab.distAtTake : glm::vec3{1.f, 0.f, 0.f};
    panelGrab.handFromHead = origin - s.head;
    pointingHand = hand;
    return true;
}

// The held panel where the hand has it now, upright (its right edge level).
void followHold(const hands::State& s)
{
    glm::vec3 origin;
    glm::mat3 basis;
    if(!pointerFrame(s, panelGrab.hand, origin, basis))
    {
        return;
    }
    glm::vec3 right = basis * panelGrab.right;
    glm::vec3 up = basis * panelGrab.up;
    const glm::vec3 facing = glm::normalize(glm::cross(up, right)); // (away from the viewer)
    if(za::fabs(facing.z) < 0.98f)
    {
        right = glm::normalize(glm::cross(facing, glm::vec3{0.f, 0.f, 1.f}));
        up = glm::cross(right, facing);
    }
    // Pushed and pulled: the hand's move towards or away from where the panel was (from the head) counts
    // vr_menu_grab_reach times, so that an arm's length is the room's worth; the distance kept within
    // vr_menu_grab_dist_min..max cm (never pinched in from where it was taken).
    glm::vec3 centre = origin + basis * panelGrab.centre;
    const float reach = za::fmax(1.f, vr_menu_grab_reach.value);
    if(reach > 1.f)
    {
        const glm::vec3 toPanel = centre - s.head;
        const float dist = glm::length(toPanel);
        if(dist > 1.f)
        {
            const float radial = glm::dot((origin - s.head) - panelGrab.handFromHead, panelGrab.dirAtTake);
            const float lo = za::fmin(panelGrab.distAtTake, vr_menu_grab_dist_min.value * 0.01f * units::metresToUnits());
            const float hi = za::fmax(panelGrab.distAtTake, vr_menu_grab_dist_max.value * 0.01f * units::metresToUnits());
            const float want = za::fmax(lo, za::fmin(hi, dist + (reach - 1.f) * radial));
            centre = s.head + toPanel * (want / dist);
        }
    }
    const int was = menu::currentInstance(); // (the menu held, whichever the menu code works on)
    menu::selectInstance(panelGrab.menu);
    panel::placeMenu(s, centre, right, up);
    menu::selectInstance(was);
}

void updateGrab(const hands::State& s, bool on)
{
    if(!on)
    {
        panelGrab.hand = -1;
        panelGrab.armedHand = -1;
        return;
    }
    if(const int h = panelGrab.armedHand; h >= 0)
    {
        glm::vec3 origin;
        glm::mat3 basis;
        if(panelGrab.sticksMoved || mouseHeld[h] || panelGrab.hand >= 0)
        {
            panelGrab.armedHand = -1;
        }
        else if(pointerFrame(s, h, origin, basis) &&
                glm::distance(origin, panelGrab.armedAt) > za::fmax(0.f, vr_menu_grab_deadzone.value) * 0.01f * units::metresToUnits())
        {
            panelGrab.armedHand = -1;
            const bool onIt = hits[h].valid && hits[h].menu == panelGrab.armedMenu;
            if(takeHold(s, h, panelGrab.armedMenu, onIt ? hits[h].point : panelGrab.armedSpot))
            {
                haptics::pulse(h, 0.03f, 0.3f, 200.f);
            }
        }
    }
    if(panelGrab.hand >= 0)
    {
        followHold(s);
    }
}

// The spot as a window position (the canvas covers the 2D layer's viewport) to M_Mousemove: when
// it moved past the hand's tremor (so that it does not undo the sticks' moves), at all while the
// button is held (dragging), or when the desktop's mouse moved the menus' since. The hit's menu is selected first (the
// panel pointed at is the focused one: MULTI_MENU.md).
void moveMouse(const Hit& hit, bool force, bool held)
{
    menu::selectInstance(hit.menu);
    SentMouse& sent = sentMouse[menu::currentInstance()];
    const glm::ivec2 p{glx + static_cast<int>(hit.uv.x * glwidth), gly + static_cast<int>((1.f - hit.uv.y) * glheight)};
    const glm::ivec2 d = glm::abs(p - sent.window);
    const int threshold = held ? 0 : 3;
    if(force || d.x > threshold || d.y > threshold || glm::vec2{m_mousex, m_mousey} != sent.menu)
    {
        sent.window = p;
        M_Mousemove(p.x, p.y);
        sent.menu = {m_mousex, m_mousey};
    }
}

// A camera-facing strip from `a` to `b` (soft edges), or a disc at `a` (b == a).
void appendStrip(za::Vector<gfx::Vertex>& v, const glm::vec3& a, const glm::vec3& b, float width,
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
            v.pushBack(q[i]);
        }
        return;
    }

    const glm::vec3 side = glm::normalize(glm::cross(b - a, eye - a)) * h;
    const gfx::Vertex q[4]{{a - side, {-1.f, 0.f}, colorA}, {a + side, {1.f, 0.f}, colorA},
        {b + side, {1.f, 0.f}, colorB}, {b - side, {-1.f, 0.f}, colorB}};
    for(const int i : {0, 1, 2, 0, 2, 3})
    {
        v.pushBack(q[i]);
    }
}

// ----------------------------------------------------------------------------
// The corner's buttons: Back to game, Search, Console, VR Settings, Advanced VR, Levels, Map Library, Relighting,
// Checklist
// ----------------------------------------------------------------------------

// A column at the panel's top left, over every menu: "Back to game" (closes the menu, which reopens where it was),
// "Quicksave" and "Quickload" (the same quick slot as the keyboard bindings), "Main Menu" (Quake's main menu),
// "Search" and "Console" (Quake's console with a keyboard: commands typed in the headset), "VR Settings" (the VR
// Settings page), "Advanced VR" (the Advanced VR Options page), "Levels" (Ironwail's level list), "Map Library",
// "Relighting" (Graphics > Relighting) and, at Menu Detail: Developer, "Toolgun", "Save Default" (the settings changed
// from their defaults written for review, a second press confirming: vr_propose_defaults.hpp) and "Checklist" (the
// playtest checklist, its open items counted on it; vr_checklist.hpp), from any page. The laser clicks them; the sticks reach them too (focus): a click of either stick
// on any menu, or on a VR page up from its first setting (down from its last).
enum Tool
{
    ToolBack,
    ToolQuicksave,
    ToolQuickload,
    ToolMainMenu, // the main menu, from any menu (the VR pages' Back stack started again)
    ToolSearch, // the VR menus' Search page (vr_menu_search.inc)
    ToolConsole, // Quake's console with a keyboard (vr_menu_console.inc)
    ToolSettings, // the VR Settings page
    ToolAdvanced,
    ToolLevels,
    ToolMaps, // the map browser: the external map index, installed and played from here (vr_menu_maps.inc)
    ToolRelighting, // Graphics > Relighting (the map's lights baked again: vr_relight)
    ToolNewPanel, // another menu panel (MULTI_MENU.md): in the headset only
    ToolToolgun, // a toolgun in the main hand, the off hand when the main hand holds something (Developer only)
    ToolSaveDefaults, // the settings changed from their defaults written for review (vr_propose_defaults; Developer only)
    ToolChecklist, // (the last: shown at Menu Detail: Developer only)
    ToolCount
};

// (The checklist's count after its label: "Checklist 99" at most, as wide as "Back to game": "Save Default" too.)
constexpr const char* toolLabels[ToolCount]{"Back to game", "Quicksave", "Quickload", "Main Menu", "Search", "Console",
    "VR Settings", "Advanced VR", "Levels", "Map Library", "Relighting", "New Panel", "Toolgun", "Save Default",
    "Checklist 99"};

// Their names for the tests' commands (vr_mock_laser, vr_mock_mouse).
constexpr const char* toolNames[ToolCount]{"back", "quicksave", "quickload", "main", "search", "console", "settings",
    "advanced", "levels", "maps", "relighting", "newpanel", "toolgun", "savedefaults", "checklist"};

// Save Default: a first press arms it ("Press again", saveDefaultsConfirm seconds), a second writes the proposal file
// (vr_propose_defaults.hpp); then it says "Saved N" for saveDefaultsShown seconds.
constexpr double saveDefaultsConfirm = 3.0;
constexpr double saveDefaultsShown = 5.0;
struct SaveDefaultsButton
{
    double armedAt{-1e9}; // realtime of the first press
    double savedAt{-1e9}; // of the save
    int saved{0};         // settings written (-1: the file could not be written)
    char label[16]{};     // "Saved 123", "Not saved"
};
SaveDefaultsButton saveDefaultsButton;

// The buttons shown: the Checklist (the last) only at Menu Detail: Developer (the playtest checklist is the author's).
[[nodiscard]] int toolsShown()
{
    return menu::developerLevel() ? ToolCount : ToolToolgun;
}

// The Capture Frame button (vr_renderdoc.cpp): only at Menu Detail: Developer, and only while RenderDoc is attached.
[[nodiscard]] bool renderdocShown()
{
    return menu::developerLevel() && qvr::renderdoc::attached();
}

// The bottom left corner's top row: 1 the Recording button, 2 OBS's row while it shows, the RenderDoc button above them.
// Those rows from the bottom: the flat screen's VR layout has no spectator camera's switch (its window shows no headset's
// mirror), so the rest are a row lower.
[[nodiscard]] int cornerRowShift()
{
    return qvr::menuui::inHeadset() ? 0 : 1;
}

[[nodiscard]] int cornerTopRow()
{
    return (qvr::obs::banner().shown ? 2 : 1) + (renderdocShown() ? 1 : 0); // (as bannerRowLayout takes it)
}

// New Panel only in the headset (its panels are): on a flat screen's row it is left out, the buttons after it a place
// nearer (toolSlot).
[[nodiscard]] bool toolHidden(int tool)
{
    return tool == ToolNewPanel && !qvr::menuui::inHeadset();
}

[[nodiscard]] int toolSlot(int tool)
{
    return tool > ToolNewPanel && toolHidden(ToolNewPanel) ? tool - 1 : tool;
}

// Menu Look: the buttons in groups, vr_menu_toolbar_groups pixels between them: the game (Back to game), saves, menus
// (Main Menu, Search, Console), settings, levels (Levels, Map Library), tools (Relighting, New Panel), the developer's.
[[nodiscard]] bool toolGroupStart(int tool)
{
    return tool == ToolQuicksave || tool == ToolMainMenu || tool == ToolSettings || tool == ToolLevels ||
           tool == ToolRelighting || tool == ToolToolgun;
}

[[nodiscard]] int toolGroupsBefore(int tool)
{
    int groups = 0;
    for(int t = 1; t <= tool; t++)
    {
        groups += toolGroupStart(t) ? 1 : 0;
    }
    return groups;
}

[[nodiscard]] float toolGroupGap()
{
    return CLAMP(0.f, vr_menu_toolbar_groups.value, 12.f);
}

// The last button shown.
[[nodiscard]] int lastTool()
{
    return toolHidden(toolsShown() - 1) ? toolsShown() - 2 : toolsShown() - 1;
}

// A button's label: on a menu panel after the first (MULTI_MENU.md), Back to game closes that panel only.
[[nodiscard]] const char* toolLabel(int tool)
{
    if(tool == ToolSaveDefaults)
    {
        SaveDefaultsButton& s = saveDefaultsButton;
        if(realtime - s.armedAt <= saveDefaultsConfirm)
        {
            return "Press again";
        }
        if(realtime - s.savedAt <= saveDefaultsShown)
        {
            if(s.saved < 0)
            {
                return "Not saved";
            }
            q_snprintf(s.label, sizeof(s.label), "Saved %d", s.saved);
            return s.label;
        }
    }
    return tool == ToolBack && menu::currentInstance() > 0 ? "Close Panel" : toolLabels[tool];
}

// Whether a quick.sav is there (load quick's lookup: the game's folder, then the base folders): looked for on disk at
// most once a second for the buttons drawn each frame, and afresh when `fresh` (the button pressed).
struct QuickSaveSeen
{
    double at{-1e9}; // realtime it was looked for
    bool found{false};
};
QuickSaveSeen quickSaveSeen;

[[nodiscard]] bool quickSaveThere(bool fresh)
{
    QuickSaveSeen& q = quickSaveSeen;
    if(!fresh && realtime >= q.at && realtime - q.at < 1.0)
    {
        return q.found;
    }
    q.at = realtime;
    q.found = Sys_FileExists(va("%s/quick.sav", com_gamedir));
    for(int i = com_numbasedirs - 1; i >= 0 && !q.found; i--)
    {
        q.found = Sys_FileExists(va("%s/quick.sav", com_basedirs[i]));
    }
    return q.found;
}

// Match save quick's restrictions and load quick's file lookup. Leave unavailable buttons in the same place, so
// opening a menu before a game or after dying never moves the other shortcuts under the laser. `fresh`: the files
// looked at now (a press), not as seen up to a second ago (the buttons drawn).
[[nodiscard]] const char* toolUnavailable(int tool, bool fresh = false)
{
    if(tool == ToolNewPanel)
    {
        glm::vec3 corner, xAxis, yAxis;
        if(!qvr::menuui::inHeadset() || !panel::menuQuad(hands::current(), 0, corner, xAxis, yAxis))
        {
            return "Only with the menu in the headset";
        }
        if(menu::openInstances() >= menu::instanceCount)
        {
            return "Three panels open at most";
        }
    }
    if(tool == ToolToolgun)
    {
        if(!sv.active || svs.maxclients != 1 || deathmatch.value)
        {
            return "Only in a local single-player game";
        }
        if(cl.stats[STAT_HEALTH] <= 0)
        {
            return "Not while dead";
        }
        return nullptr;
    }
    if(tool == ToolQuicksave)
    {
        if(!sv.active)
        {
            return "Not playing a local game";
        }
        if(VR_RunIronman())
        {
            return "Ironman: no saving";
        }
        if(sv.nomonsters)
        {
            return "Cannot save with nomonsters";
        }
        if(cl.intermission)
        {
            return "Cannot save in intermission";
        }
        if(svs.maxclients != 1)
        {
            return "Cannot save multiplayer games";
        }
        if(const edict_t* player = progs::localPlayer(); player && player->v.health <= 0)
        {
            return "Cannot save a dead player";
        }
    }
    else if(tool == ToolQuickload)
    {
        if(Host_IsSaving())
        {
            return "Saving game; try again shortly";
        }
        return quickSaveThere(fresh) ? nullptr : "No quicksave in this game";
    }
    return nullptr;
}

// Where the column goes: across, menu pixels; up and down, from the canvas's top in true pixels
// (menu pixels as wide as they are across: `k` menu rows' pixels each, the canvas's row spacing).
// On a flat screen a row of icons along the canvas's top edge instead: the desktop's menu canvas has no room
// above or beside the menus for the column (its pages and lists reach its left edge), only that strip above them.
struct ToolbarLayout
{
    static constexpr float corner = 4.f;    // true pixels from the panel's edges
    static constexpr float labelReserve = 8.f; // the build label's line at the very bottom (vr_menubrand.cpp: its 5 px and a gap), under the corner buttons
    static constexpr float rowCorner = corner; // the row's from the canvas's top edge (as the status box's and the version
                                               // box's from theirs)
    // The headset's column (and the bottom left corner's rows: OBS's, the spectator camera's switch) large for the
    // laser: 20 tall, 4 apart (the author's note vrstart_2026-10-09_14-57-46: they were 14 and 2, easy to miss and to
    // press the one next to it); the flat screen's row of icons, under a desktop mouse, as it was.
    static constexpr float half = 10.f;     // half a button's height (the column's)
    static constexpr float rowHalf = 6.f;   // the row's
    static constexpr float gap = 4.f;       // between two buttons (the column's)
    static constexpr float rowGap = 2.f;    // the row's
    static constexpr float pad = 6.f;       // the column's icon and label from its buttons' ends
    static constexpr float labelGap = 5.f;  // a label from its icon
    static constexpr float icon = 9.f;      // an icon's width
    static constexpr float iconButton = 4.f + icon + 4.f; // the row's button (no label)
    static constexpr float iconColumnButton = pad + icon + pad; // the column's without its label
    static constexpr float columnRight = -136.f; // the labelled column's right edge (menu x): clear of the VR pages'
                                                 // labels up to 38 characters (from x -128) and Ironwail's lists
    static constexpr float columnGap = 8.f;      // at least this clear of the menu's leftmost text (menu x)

    float left{0.f}, top{0.f}; // the canvas's corner
    float bottom{0.f};         // the canvas's bottom edge (menu y)
    float k{1.f};
    float columnHalf{half}, buttonGap{gap}; // smaller only when the column would reach the bottom corner's rows
    float groupGap{0.f};       // between groups of buttons (toolGroupGap; the column's, as smaller)
    float x0{0.f}, x1{0.f};    // the column's edges (the row: its first button's)
    float edge{0.f};           // where the column's right edge goes: columnRight, or left of a wider menu
    float limit{0.f};          // the nearest the column may come to the menu (menu::contentLeft, less columnGap)
    bool labels{false};
    bool row{false};           // the flat screen's row of icons

    // Half a button's height (true pixels), and a button's middle (menu y).
    [[nodiscard]] float bh() const { return row ? rowHalf : columnHalf; }
    [[nodiscard]] float yc(int tool) const
    {
        return row ? top + (rowCorner + rowHalf) / k :
                     top + (corner + columnHalf + toolSlot(tool) * (2.f * columnHalf + buttonGap) + toolGroupsBefore(tool) * groupGap) / k;
    }

    // A button's left and right edges.
    [[nodiscard]] float bx0(int tool) const
    {
        return row ? x0 + toolSlot(tool) * (iconButton + rowGap) + toolGroupsBefore(tool) * groupGap : x0;
    }
    [[nodiscard]] float bx1(int tool) const { return row ? bx0(tool) + iconButton : x1; }

    // The zoom buttons at the list's end (zoomOut "-", zoomIn "+"): small squares, side by side in a slot of their own
    // under the column's last button, or after the row's last icon. Their middle (menu y), left edge, side (menu x).
    [[nodiscard]] float zoomYc() const { return row ? yc(0) : yc(lastTool()) + (2.f * columnHalf + buttonGap) / k; }
    [[nodiscard]] float zoomSide() const { return 2.f * bh(); }
    [[nodiscard]] float zoomGap() const { return row ? rowGap : buttonGap; }
    [[nodiscard]] float zoomX0(int button) const
    {
        const float first = row ? bx1(lastTool()) + rowGap : x0;
        return first + static_cast<float>(button) * (zoomSide() + zoomGap());
    }
    // What a click on one takes: half the gap round it.
    [[nodiscard]] bool zoomHit(int button, float x, float y) const
    {
        const float g = zoomGap() * 0.5f;
        const float half = (bh() + g) / k;
        return x >= zoomX0(button) - g && x <= zoomX0(button) + zoomSide() + g && y >= zoomYc() - half &&
               y <= zoomYc() + half;
    }

    // The last button's bottom edge (menu y: the zoom buttons' in the column), and the buttons' right edge (the
    // row's zoom buttons').
    [[nodiscard]] float buttonsBottom() const { return zoomYc() + bh() / k; }
    [[nodiscard]] float right() const { return row ? zoomX0(1) + zoomSide() : bx1(lastTool()); }

    // What a click on `tool` takes: as far as the panel's edges, split halfway between buttons.
    [[nodiscard]] bool hit(int tool, float x, float y) const
    {
        const bool last = tool == lastTool();
        if(row)
        {
            const float rx0 = tool == 0 ? left : bx0(tool) - rowGap * 0.5f;
            const float rx1 = bx1(tool) + (last ? 2.f : rowGap * 0.5f);
            return x >= rx0 && x <= rx1 && y >= top && y <= yc(tool) + (rowHalf + 2.f) / k;
        }
        const float y0 = tool == 0 ? top : yc(tool) - (columnHalf + buttonGap * 0.5f) / k;
        const float y1 = yc(tool) + (columnHalf + (last ? 2.f : buttonGap * 0.5f)) / k;
        return x >= left && x <= x1 + 2.f && y >= y0 && y <= y1;
    }
};

// From the menu canvas's transform (as the menus lay out, and whether or not it is the one set).
[[nodiscard]] ToolbarLayout toolbarLayout()
{
    drawtransform_t t;
    Draw_GetCanvasTransform(CANVAS_MENU, &t);
    float right;
    ToolbarLayout l;
    Draw_GetTransformBounds(&t, &l.left, &l.top, &right, &l.bottom);
    l.k = za::fmax(1.f, -t.scale[1] * vid.guiheight / (t.scale[0] * vid.guiwidth)); // as Painter's

    // All as wide as the widest label, their right edges at columnRight, clear left of what the menus draw (the VR
    // pages' labels reach left of Quake's 320 columns, right-aligned to the values' column; Ironwail's lists span
    // the canvas's middle), the same on almost every page; further left on a page whose text reaches further
    // (menu::contentLeft). On a narrower panel as far left as it goes (no nearer the menu than `limit`); where the
    // labels do not fit even so, only the icons, in the corner.
    float widest = 0.f;
    for(const char* label : toolLabels)
    {
        widest = za::fmax(widest, 8.f * static_cast<float>(strlen(label)));
    }
    const float width = ToolbarLayout::pad + ToolbarLayout::icon + ToolbarLayout::labelGap + widest + ToolbarLayout::pad;
    l.limit = menu::contentLeft() - ToolbarLayout::columnGap;
    l.edge = za::fmin(ToolbarLayout::columnRight, l.limit);
    l.x1 = l.edge;
    l.x0 = l.x1 - width;
    if(l.x0 < l.left + ToolbarLayout::corner)
    {
        l.x0 = l.left + ToolbarLayout::corner;
        l.x1 = l.x0 + width;
    }
    l.labels = l.x1 <= l.limit;
    l.row = !qvr::menuui::active(); // (a flat screen's own menus)
    l.groupGap = l.row ? toolGroupGap() * 0.5f : toolGroupGap();
    if(!l.row)
    {
        // Keep every shortcut above Recording/OBS even with Menu Height and Row Spacing at their smallest.
        // Normal panels keep the author's large targets; constrained panels fit the column as a whole.
        const int bottomRow = cornerTopRow() - cornerRowShift();
        const float reserve = ToolbarLayout::corner + ToolbarLayout::labelReserve + 2.f * ToolbarLayout::half +
                              bottomRow * (2.f * ToolbarLayout::half + ToolbarLayout::gap) + ToolbarLayout::gap;
        const float room = (l.bottom - l.top) * l.k - ToolbarLayout::corner - reserve;
        const float height = (toolsShown() + 1) * (2.f * ToolbarLayout::half + ToolbarLayout::gap) - ToolbarLayout::gap + // (+1: the zoom buttons)
                             static_cast<float>(toolGroupsBefore(lastTool())) * l.groupGap;
        const float fit = za::fmin(1.f, za::fmax(0.1f, room / height));
        l.columnHalf *= fit;
        l.buttonGap *= fit;
        l.groupGap *= fit;
    }
    if(!l.labels || l.row)
    {
        l.labels = false;
        l.x0 = l.left + ToolbarLayout::corner;
        l.x1 = l.x0 + (l.row ? ToolbarLayout::iconButton : ToolbarLayout::iconColumnButton);
    }
    return l;
}

struct Toolbar
{
    int menu{m_none};  // the menu it was last drawn over (m_none: not drawn)
    int hovered{-1};   // the button under the laser
    int zoomHovered{-1}; // the zoom button under it (0 "-", 1 "+")
    int focused{-1};   // the button the sticks selected (-1: the menu has the selection)
    bool bannerHovered{false}; // the spectator camera's switch under the laser
    bool obsHovered{false};    // OBS's row above it (vr_obs.cpp)
    bool renderdocHovered{false}; // the RenderDoc button above that (vr_renderdoc.cpp)
    bool recordingHovered{false}; // the Recording button (between them in the headset; on a flat screen, the mouse)
    bool linkHovered{false};   // the version box's Ko-fi link under the laser
    int focusMenu{m_none};
    glm::vec2 focusMouse{0.f}; // the laser's spot when they did: moving it on gives the selection back
};
Toolbar toolbars[menu::instanceCount]; // each menu panel's (MULTI_MENU.md): the current menu's, tb()

[[nodiscard]] Toolbar& tb()
{
    return toolbars[menu::currentInstance()];
}

// The status box (VR_MenuDrawStatus) as last drawn: its lines' count and the longest's length, for the menus' rows to
// keep clear of it (menuui::statusBottom).
struct StatusBox
{
    int lines{0};
    int widest{0};
};
StatusBox statusBox;

// Its size: small on a flat screen (the menu's canvas is near the window's size); nearer the corner buttons' 8 in the
// headset. Measured in true pixels down (the box, the rows' tops), as the Painter draws: on a page whose rows are
// spaced out (vr_menu_spacing, the canvas's y scaled by k) the characters keep their size (Draw_KeepMenuGlyphSize),
// so the rows are placed in true pixels too, and the box fits them on every page.
struct StatusMetrics
{
    float size, step, pad;
    [[nodiscard]] float width(int widest) const { return size * static_cast<float>(widest) + 2.f * pad; }
    [[nodiscard]] float height(int lines) const { return step * static_cast<float>(lines) + 2.f * pad - (step - size); }
};
[[nodiscard]] StatusMetrics statusMetrics()
{
    const bool headset = menuui::active();
    return {headset ? 7.f : 5.f, headset ? 9.f : 7.f, 4.f};
}
constexpr int statusReserve = 40; // the box taken as this many characters wide at least, for the rows below it (so
                                  // that a line growing by a digit does not move them)

// On a flat screen the desktop mouse lights a button up only once it has moved over the menus (until then the menus'
// mouse is where it was last, or where the laser left it).
struct FlatMouse
{
    glm::vec2 seen{0.f};
    int frame{-10}; // the frame the buttons were last drawn in (host_framecount)
    bool moved{false};
};
FlatMouse flatMouse;

// The zoom button at a spot of the menu (0 "-", 1 "+"; -1: none).
[[nodiscard]] int zoomAt(float x, float y)
{
    if(tb().menu != m_state)
    {
        return -1;
    }
    const ToolbarLayout l = toolbarLayout();
    for(int z = 0; z < 2; z++)
    {
        if(l.zoomHit(z, x, y))
        {
            return z;
        }
    }
    return -1;
}

// The button at a spot of the menu (-1: none; the zoom buttons come first, the row's last icon's click reaching them).
[[nodiscard]] int toolAt(float x, float y)
{
    if(tb().menu != m_state || zoomAt(x, y) >= 0)
    {
        return -1;
    }
    const ToolbarLayout l = toolbarLayout();
    for(int t = 0; t < toolsShown(); t++)
    {
        if(!toolHidden(t) && l.hit(t, x, y))
        {
            return t;
        }
    }
    return -1;
}

void focusTool(int tool)
{
    tb().focused = tool;
    tb().focusMenu = m_state;
    tb().focusMouse = {m_mousex, m_mousey};
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
    else if(state == m_confirm)
    {
        state = M_Confirm_PrevState();
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
        case m_quit:
        case m_confirm: return m_none;
        default: return state;
    }
}

// The spectator camera's switch, in the bottom left corner of every menu in the headset (the VR menu style): whether
// the window shows the spectator camera (a whole extra render of the scene: so that it is not left on after
// recording), a click switches it (Graphics > Recording > Window View, and the desktop mirror on if it was off).
[[nodiscard]] bool spectatorOn()
{
    return vr_mirror.value > 0.f && static_cast<int>(vr_window_view.value) == 2;
}

struct BannerLayout
{
    float x0{0.f}, x1{0.f}, yc{0.f};
    const char* text{""};
};

// A row's light (its middle) and text from its left end, as the column's icons and labels.
constexpr float bannerLightX = ToolbarLayout::pad + 3.f;
constexpr float bannerTextX = ToolbarLayout::pad + 6.f + ToolbarLayout::labelGap;

// Its right edge as the buttons' (left of the menu and its help, as far as the panel lets them), the long text where
// it fits, else the short; else the short in the corner.
// `row` 0 the spectator camera's switch, 1 the Recording button above it, 2 OBS's row above that; its text `textX` from
// its left end (after the light, or an icon).
[[nodiscard]] BannerLayout bannerRowLayout(const ToolbarLayout& l, const char* const (&texts)[2], int row,
    float textX = bannerTextX)
{
    BannerLayout b;
    row -= cornerRowShift();
    b.yc = l.bottom - (ToolbarLayout::corner + ToolbarLayout::labelReserve + ToolbarLayout::half + static_cast<float>(row) * (2.f * ToolbarLayout::half +
                                                                                                    ToolbarLayout::gap)) /
                          l.k;
    float width = 0.f;
    for(const char* text : texts)
    {
        b.text = text;
        width = textX + 8.f * static_cast<float>(strlen(text)) + ToolbarLayout::pad;
        b.x1 = l.edge;
        b.x0 = b.x1 - width;
        if(b.x0 < l.left + ToolbarLayout::corner)
        {
            b.x0 = l.left + ToolbarLayout::corner; // (as far left as the panel goes, as the column)
            b.x1 = b.x0 + width;
        }
        if(b.x1 <= l.limit)
        {
            return b;
        }
    }
    b.x0 = l.left + ToolbarLayout::corner;
    b.x1 = b.x0 + width;
    return b;
}

[[nodiscard]] BannerLayout bannerLayout(const ToolbarLayout& l)
{
    const bool on = spectatorOn();
    const char* const texts[2]{on ? "Spectator camera: On" : "Spectator camera: Off", on ? "Spectator: On" : "Spectator: Off"};
    return bannerRowLayout(l, texts, 0);
}

// The Recording button (the author, 2026-10-10: a way to Graphics > Recording from any menu), above the switch, as it.
constexpr int recordingRow = 1;
constexpr float recordingTextX = ToolbarLayout::pad + ToolbarLayout::icon + ToolbarLayout::labelGap; // (an icon's width)
[[nodiscard]] BannerLayout recordingBannerLayout(const ToolbarLayout& l)
{
    const char* const texts[2]{"Recording Settings", "Recording"};
    return bannerRowLayout(l, texts, recordingRow, recordingTextX);
}

// OBS's row (vr_obs.cpp: shown while OBS answers, or is found and says why not), above the Recording button, as it.
constexpr int obsRow = 2;
[[nodiscard]] BannerLayout obsBannerLayout(const ToolbarLayout& l, const qvr::obs::Banner& o)
{
    const char* const texts[2]{o.text, o.shortText};
    return bannerRowLayout(l, texts, obsRow);
}

// Whether a spot of the menu is on the bottom left corner's row `row` laid out as `b` (from the panel's left edge, half
// the gap above and below it, the switch's down to the panel's bottom; drawn over this menu: the VR style's).
[[nodiscard]] bool cornerRowAt(const ToolbarLayout& l, const BannerLayout& b, int row, float x, float y)
{
    const float reach = (ToolbarLayout::half + ToolbarLayout::gap * 0.5f) / l.k;
    return tb().menu == m_state && qvr::menuui::active() && x >= l.left && x <= b.x1 + 2.f && y >= b.yc - reach &&
           (row == cornerRowShift() ? y <= l.bottom : y < b.yc + reach);
}

// The spectator camera's switch (in the headset only).
[[nodiscard]] bool bannerAt(float x, float y)
{
    if(!qvr::menuui::inHeadset())
    {
        return false;
    }
    const ToolbarLayout l = toolbarLayout();
    return cornerRowAt(l, bannerLayout(l), 0, x, y);
}

// OBS's row.
[[nodiscard]] bool obsBannerAt(float x, float y)
{
    const qvr::obs::Banner o = qvr::obs::banner();
    const ToolbarLayout l = toolbarLayout();
    return o.shown && cornerRowAt(l, obsBannerLayout(l, o), obsRow, x, y);
}

// The RenderDoc button (Capture Frame), above OBS's row while it shows, else above the Recording button.
[[nodiscard]] int renderdocRow()
{
    return qvr::obs::banner().shown ? obsRow + 1 : obsRow;
}
constexpr float renderdocTextX = ToolbarLayout::pad + 6.f + ToolbarLayout::labelGap;
[[nodiscard]] BannerLayout renderdocBannerLayout(const ToolbarLayout& l)
{
    const char* const texts[2]{"RenderDoc: Capture Frame", "Capture Frame"};
    return bannerRowLayout(l, texts, renderdocRow(), renderdocTextX);
}
[[nodiscard]] bool renderdocAt(float x, float y)
{
    if(!renderdocShown())
    {
        return false;
    }
    const ToolbarLayout l = toolbarLayout();
    return cornerRowAt(l, renderdocBannerLayout(l), renderdocRow(), x, y);
}

// A flat screen's Recording button (VR off, with the corner's buttons: vr_menu_flat_shortcuts), for the desktop mouse:
// in the canvas's bottom left corner, small, a box like the version box's in the bottom right (vr_menubrand.cpp: as far
// from the corner, its text as far inside it, the same letters), only where nothing the menu draws reaches under it
// (menu::contentLeftBelow) nor the flat banner comes down to it. As last placed (by VR_MenuDrawOverlay).
struct FlatRecording
{
    static constexpr float corner = 4.f; // from the canvas's left and bottom edges (true pixels; the version box's)
    static constexpr float pad = 4.f;    // the text from the box's edges (true pixels)
    static constexpr float size = 5.f;   // the letters (the version box's on a flat screen)
    static constexpr float gap = 8.f;    // clear of what the menu draws (menu x) and of the banner (true pixels)
    static constexpr const char* text = "Recording";

    float x0{0.f}, x1{-1.f}, y0{0.f}, y1{-1.f}; // the box (menu x and y): a click anywhere on it, or on to the corner
    float contentLeft{0.f};                     // what the menu draws left to beside it
    const char* leftOut{"no menu drawn yet"};   // why it was not drawn (nullptr: drawn)
    int menu{m_none};
    int frame{-10};
};
FlatRecording flatRecording;

void placeFlatRecording(const ToolbarLayout& l)
{
    FlatRecording& f = flatRecording;
    f.x0 = l.left + FlatRecording::corner;
    f.x1 = f.x0 + FlatRecording::pad + FlatRecording::size * static_cast<float>(strlen(FlatRecording::text)) +
           FlatRecording::pad;
    f.y1 = l.bottom - (FlatRecording::corner + ToolbarLayout::labelReserve) / l.k;
    f.y0 = f.y1 - (FlatRecording::pad + FlatRecording::size + FlatRecording::pad) / l.k;
    f.contentLeft = menu::contentLeftBelow(f.y0 - FlatRecording::gap / l.k);
    float bx0, bx1, by0, by1;
    qvr::menuui::bannerRect(bx0, bx1, by0, by1);
    if(f.x1 + FlatRecording::gap > f.contentLeft)
    {
        f.leftOut = "the menu reaches beside it";
    }
    else if(bx1 > bx0 && bx0 < f.x1 + FlatRecording::gap && by1 + FlatRecording::gap / l.k > f.y0)
    {
        f.leftOut = "the banner comes down to it";
    }
    else
    {
        f.leftOut = nullptr;
        f.menu = m_state;
        f.frame = host_framecount;
    }
}

[[nodiscard]] bool flatRecordingShown()
{
    const FlatRecording& f = flatRecording;
    return !f.leftOut && f.menu == m_state && key_dest == key_menu && host_framecount - f.frame <= 2 &&
           !qvr::menuui::active();
}

// Whether a spot of the menu is on the Recording button: the headset's row, or a flat screen's box (to the canvas's
// corner).
[[nodiscard]] bool recordingAt(float x, float y)
{
    if(flatRecordingShown())
    {
        const ToolbarLayout l = toolbarLayout();
        const FlatRecording& f = flatRecording;
        return x >= l.left && x <= f.x1 && y >= f.y0 && y <= l.bottom;
    }
    const ToolbarLayout l = toolbarLayout();
    return cornerRowAt(l, recordingBannerLayout(l), recordingRow, x, y);
}

// The spot vr_mock_laser and vr_mock_mouse take for it (false: not shown).
[[nodiscard]] bool recordingSpot(glm::vec2& spot)
{
    const ToolbarLayout l = toolbarLayout();
    if(qvr::menuui::active())
    {
        const BannerLayout b = recordingBannerLayout(l);
        spot = {(b.x0 + b.x1) * 0.5f, b.yc};
        return tb().menu == m_state;
    }
    const FlatRecording& f = flatRecording;
    spot = {(f.x0 + f.x1) * 0.5f, (f.y0 + f.y1) * 0.5f};
    return flatRecordingShown();
}

// The spectator camera's preview (vr_spectator_preview), while it is on: above its switch, as wide as the switch (within
// the column left of Quake's plaque), the window's shape; smaller where the corner's buttons leave less room, none
// where too small to make out. Placed by the 2D pass in the canvas (u0, v0, u1, v1), drawn by the eyes over the panel
// (drawPreview): not into the canvas, which the camera sees in the world (it would show itself, a frame late, over
// and over).
struct PreviewPlace
{
    glm::vec4 image{0.f};
    glm::vec4 frame{0.f};
    int placed{-10}; // host_framecount when placed
};
PreviewPlace previewPlace;

void placePreview(const ToolbarLayout& l, const BannerLayout& b)
{
    constexpr float border = 1.5f; // the frame round it (true pixels)
    constexpr float smallest = 48.f;
    if(!vr_spectator_preview.value || !spectatorOn() || vid.width <= 0 || vid.height <= 0)
    {
        return;
    }
    const float aspect = static_cast<float>(vid.height) / static_cast<float>(vid.width); // the camera's: the window's
    const float x0 = za::fmax(b.x0, l.left + ToolbarLayout::corner);
    float x1 = za::fmin(b.x1, l.limit);
    const float bottom = b.yc - (ToolbarLayout::half + 2.f * ToolbarLayout::gap) / l.k;
    const float room = (bottom - (l.buttonsBottom() + 2.f * ToolbarLayout::gap / l.k)) * l.k; // true pixels
    float height = (x1 - x0 - 2.f * border) * aspect + 2.f * border;
    if(height > room)
    {
        height = room;
        x1 = x0 + (room - 2.f * border) / aspect + 2.f * border;
    }
    if(x1 - x0 < smallest)
    {
        return;
    }
    const float top = bottom - height / l.k;
    const drawtransform_t& t = glcanvas.transform;
    const auto uv = [&](float ax, float ay, float bx, float by) {
        const float u0 = (ax * t.scale[0] + t.offset[0] + 1.f) * 0.5f, u1 = (bx * t.scale[0] + t.offset[0] + 1.f) * 0.5f;
        const float v0 = (ay * t.scale[1] + t.offset[1] + 1.f) * 0.5f, v1 = (by * t.scale[1] + t.offset[1] + 1.f) * 0.5f;
        return glm::vec4{za::fmin(u0, u1), za::fmin(v0, v1), za::fmax(u0, u1), za::fmax(v0, v1)};
    };
    previewPlace.frame = uv(x0, top, x1, bottom);
    previewPlace.image = uv(x0 + border, top + border / l.k, x1 - border, bottom - border / l.k);
    previewPlace.placed = host_framecount;
}

[[nodiscard]] bool previewPlaced()
{
    return vr_spectator_preview.value && host_framecount - previewPlace.placed <= 2;
}

// What the switch changed, to put back when it is switched off: the window's view before, and the desktop mirror's
// setting when it was off (-1: it was on).
struct SpectatorSwitch
{
    float view{0.f};
    float mirror{-1.f};
};
SpectatorSwitch spectatorSwitch;

void toggleSpectator(int hand)
{
    SpectatorSwitch& s = spectatorSwitch;
    if(spectatorOn())
    {
        Cvar_SetValueQuick(&vr_window_view, s.view);
        if(s.mirror >= 0.f)
        {
            Cvar_SetValueQuick(&vr_mirror, s.mirror);
        }
        s = {};
    }
    else
    {
        s.view = static_cast<int>(vr_window_view.value) == 2 ? 0.f : vr_window_view.value;
        s.mirror = vr_mirror.value > 0.f ? -1.f : vr_mirror.value;
        Cvar_SetValueQuick(&vr_window_view, 2.f);
        if(vr_mirror.value <= 0.f)
        {
            Cvar_SetValueQuick(&vr_mirror, 1.f);
        }
    }
    Con_DPrintf("spectator camera switched %s (vr_window_view %g, vr_mirror %g)\n", spectatorOn() ? "on" : "off",
        vr_window_view.value, vr_mirror.value);
    qvr::menuui::uiSound(qvr::menuui::UiSound::Adjust);
    haptics::pulse(hand, 0.02f, 0.3f, 200.f);
}

// ----------------------------------------------------------------------------
// Scrolling with the stick
// ----------------------------------------------------------------------------

// Scrolls the current menu's list by `rows` (0: only whether it can).
[[nodiscard]] bool scrollMenu(int rows)
{
    return m_state == m_vr ? menu::scroll(rows) : M_ScrollList(rows) != 0;
}

// The laser's strips and the spectator camera's preview, each eye (the main thread).
struct MenuUiScratch
{
    za::Vector<gfx::Vertex> laser;
    za::Vector<gfx::Vertex> frame; // (the panel's frame, lit under the laser or held)
    za::Vector<gfx::Vertex> preview;
    za::Vector<za::String> status;
    za::Vector<za::String> statusOwn; // (statusLines's, after the unfocused window's warning)
    auto members() { return qvr::mem::list(laser, frame, preview, status, statusOwn); }
};
mem::Scratch<MenuUiScratch> scratch{"menu laser"};

// The preview in the eye being rendered, over the panel: its frame, then the camera's image (not in the camera's own
// view; nothing until vr_stereo.cpp has made it from an image).
void drawPreview(const hands::State& s)
{
    float aspect = 0.f;
    const unsigned texture = previewPlaced() && !stereo::isSpectator() ? stereo::spectatorPreview(aspect) : 0;
    glm::vec3 corner, xAxis, yAxis;
    if(!texture || !panel::menuQuad(s, 0, corner, xAxis, yAxis))
    {
        return;
    }
    za::Vector<gfx::Vertex>& v = scratch.preview;
    const auto quad = [&](const glm::vec4& r, const glm::vec4& color) {
        v.clear();
        const glm::vec2 c[4]{{r.x, r.y}, {r.z, r.y}, {r.z, r.w}, {r.x, r.w}};
        const glm::vec2 st[4]{{0.f, 0.f}, {1.f, 0.f}, {1.f, 1.f}, {0.f, 1.f}};
        for(const int i : {0, 1, 2, 0, 2, 3})
        {
            v.pushBack({corner + xAxis * c[i].x + yAxis * c[i].y, st[i], color});
        }
    };
    const gfx::State state{.shade = gfx::Shade::Color, .blend = gfx::Blend::Alpha, .depthTest = false, .depthWrite = false};
    quad(previewPlace.frame, colors::boxBorder);
    gfx::draw(v, gfx::sceneViewProjection(), state);
    quad(previewPlace.image, glm::vec4{1.f});
    gfx::draw(v, gfx::sceneViewProjection(),
        {.shade = gfx::Shade::Texture, .blend = gfx::Blend::Opaque, .depthTest = false, .depthWrite = false}, texture);
}

// ----------------------------------------------------------------------------
// The zoom buttons, and the flat screen's VR layout (vr_menu_flat_vr_style)
// ----------------------------------------------------------------------------

// The zoom buttons' step, and each layout's limits: the headset's Menu Scale (as far as its slider's row takes it: its
// extent), the flat screen's panel (a share of the window: all of it at most, so that a corner of it is always on the
// window), the flat menus' scr_menuscale (1 up to what their canvas fits: gl_draw.c, CANVAS_MENU).
constexpr float zoomStep = 1.1f;
constexpr float flatZoomStep = 1.05f; // the flat screen's VR-style panel: half the step (his note 2026-11-08: 1.1 was coarse)
constexpr float vrScaleMin = 0.02f;
constexpr float vrScaleMax = 1.5f;
constexpr float flatScaleMin = 0.4f;
constexpr float flatScaleMax = 1.f;

// The flat screen's panel: the window's shape, vr_menu_flat_scale of its size, its middle at vr_menu_flat_x, _y (shares
// of the window, 0 to 1: with the panel no bigger than the window, a corner of it always on the window). The menu canvas
// is laid out as on the whole window and drawn into the panel's rect (gl_draw.c GL_SetCanvas: the viewport), so the
// layout is the headset's whatever its size; the desktop mouse goes through the same rect (menu.c M_Mousemove). Its
// corners take the mouse: pressed, the panel follows it; the wheel while held resizes it round the mouse (the zoom
// buttons' step), as the Weapon Studio's window moves and keeps its rect.
struct FlatPanel
{
    static constexpr float grabOutside = 14.f; // a corner's grab zone: this many window pixels outside the panel's corner
    static constexpr float handle = 18.f;      // a corner handle's arms (window pixels)
    bool dragging{false};
    glm::vec2 grab{0.f};    // the mouse from the panel's middle while dragging (window pixels)
    glm::vec2 mouse{-1e4f}; // the window's mouse as last seen (window pixels, from the 2D viewport's top left)
    int corner{-1};         // the corner under the mouse, or held (0 top left, 1 top right, 2 bottom left, 3 bottom right)
    int cursor{-1};         // the mouse cursor set for it (MOUSECURSOR_MOVE; -1: the engine's own)
    int moves{0};           // the drags' moves, and the wheel's steps while dragging (MENUZOOM, for the tests)
    int wheelZooms{0};
};
FlatPanel flatPanelState;

struct WindowRect
{
    float x{0.f}, y{0.f}, w{0.f}, h{0.f}; // window pixels: its top left, its size
};

[[nodiscard]] float flatScale()
{
    return CLAMP(flatScaleMin, vr_menu_flat_scale.value, flatScaleMax);
}

[[nodiscard]] glm::vec2 flatCentre()
{
    return {CLAMP(0.f, vr_menu_flat_x.value, 1.f) * static_cast<float>(glwidth),
        CLAMP(0.f, vr_menu_flat_y.value, 1.f) * static_cast<float>(glheight)};
}

// In whole pixels (the viewport's).
[[nodiscard]] WindowRect flatRect()
{
    const float s = flatScale();
    const glm::vec2 c = flatCentre();
    WindowRect r;
    r.w = static_cast<float>(za::lround(static_cast<float>(glwidth) * s));
    r.h = static_cast<float>(za::lround(static_cast<float>(glheight) * s));
    r.x = static_cast<float>(za::lround(c.x - r.w * 0.5f));
    r.y = static_cast<float>(za::lround(c.y - r.h * 0.5f));
    return r;
}

// The corner whose grab zone a window spot is in (-1: none): from grabOutside outside the corner to the corner buttons'
// margin inside it (ToolbarLayout::corner: never over a button).
[[nodiscard]] int flatCornerAt(const glm::vec2& m)
{
    const WindowRect r = flatRect();
    drawtransform_t t;
    Draw_GetCanvasTransform(CANVAS_MENU, &t);
    const float inside = za::fmax(4.f, ToolbarLayout::corner * t.scale[0] * r.w * 0.5f);
    for(int c = 0; c < 4; c++)
    {
        const bool right = (c & 1) != 0, bottom = (c & 2) != 0;
        const float out_x = right ? m.x - (r.x + r.w) : r.x - m.x; // (outwards from the corner: positive)
        const float out_y = bottom ? m.y - (r.y + r.h) : r.y - m.y;
        if(out_x >= -inside && out_x <= FlatPanel::grabOutside && out_y >= -inside && out_y <= FlatPanel::grabOutside)
        {
            return c;
        }
    }
    return -1;
}

void setFlatPlace(float scale, const glm::vec2& centre)
{
    Cvar_SetValueQuick(&vr_menu_flat_scale, za::lround(CLAMP(flatScaleMin, scale, flatScaleMax) * 1000.f) / 1000.f);
    Cvar_SetValueQuick(&vr_menu_flat_x, za::lround(CLAMP(0.f, centre.x / static_cast<float>(glwidth), 1.f) * 1000.f) / 1000.f);
    Cvar_SetValueQuick(&vr_menu_flat_y, za::lround(CLAMP(0.f, centre.y / static_cast<float>(glheight), 1.f) * 1000.f) / 1000.f);
}

// The flat panel zoomed by `factor` round a window spot (what is there on the panel stays there, within the limits):
// false if it was at the limit already.
bool zoomFlatAround(const glm::vec2& at, float factor)
{
    const float s0 = flatScale();
    const float s1 = CLAMP(flatScaleMin, s0 * factor, flatScaleMax);
    if(za::fabs(s1 - s0) < 1e-4f)
    {
        return false;
    }
    setFlatPlace(s1, at + (flatCentre() - at) * (s1 / s0));
    return true;
}

// The mouse cursor over a corner, or while dragging one: the move cursor (set only when it changes).
void showFlatCursor(bool move)
{
    FlatPanel& f = flatPanelState;
    if(move ? f.cursor == MOUSECURSOR_MOVE : f.cursor < 0)
    {
        return;
    }
    VID_SetMouseCursor(move ? MOUSECURSOR_MOVE : MOUSECURSOR_DEFAULT);
    f.cursor = move ? MOUSECURSOR_MOVE : -1;
}

// Which layout the zoom buttons step: the headset's (with VR on, either menu style: Menu Scale), the flat screen's panel,
// or the flat menus.
enum class ZoomLayout
{
    Headset,
    Flat,
    Classic
};

[[nodiscard]] ZoomLayout zoomLayoutShown()
{
    return vrActive() ? ZoomLayout::Headset : vr_menu_flat_vr_style.value ? ZoomLayout::Flat : ZoomLayout::Classic;
}

[[nodiscard]] float classicScaleMax()
{
    return za::fmax(1.f, za::fmin(static_cast<float>(vid.guiwidth) / 420.f, static_cast<float>(vid.guiheight) / 320.f));
}

// A zoom step (dir 1 in, -1 out) in that layout, the flat panel's round `at` (window pixels); dir 0 back to the shipped
// size (the flat panel also to the window's middle). False: at the limit already.
bool zoomMenu(ZoomLayout layout, int dir, const glm::vec2& at)
{
    const float by = layout == ZoomLayout::Flat ? flatZoomStep : zoomStep;
    const float f = dir > 0 ? by : 1.f / by;
    const auto step = [&](cvar_t& var, float lo, float hi) {
        // (From the value in effect: a scr_menuscale past what the canvas fits zooms out at the first press.)
        const float v = CLAMP(lo, za::lround(CLAMP(lo, var.value, hi) * f * 1000.f) / 1000.f, hi);
        if(za::fabs(v - var.value) < 1e-4f)
        {
            return false;
        }
        Cvar_SetValueQuick(&var, v);
        return true;
    };
    switch(layout)
    {
        case ZoomLayout::Headset:
            if(dir == 0)
            {
                Cvar_SetQuick(&vr_menu_scale, vr_menu_scale.default_string);
                return true;
            }
            return step(vr_menu_scale, vrScaleMin, vrScaleMax);
        case ZoomLayout::Flat:
            if(dir == 0)
            {
                Cvar_SetQuick(&vr_menu_flat_scale, vr_menu_flat_scale.default_string);
                Cvar_SetQuick(&vr_menu_flat_x, vr_menu_flat_x.default_string);
                Cvar_SetQuick(&vr_menu_flat_y, vr_menu_flat_y.default_string);
                return true;
            }
            return zoomFlatAround(at, f);
        case ZoomLayout::Classic:
            if(dir == 0)
            {
                Cvar_SetQuick(&scr_menuscale, scr_menuscale.default_string);
                return true;
            }
            return step(scr_menuscale, 1.f, classicScaleMax());
    }
    return false;
}

// A press of a zoom button (0 "-", 1 "+"): the panel zoomed round the button (the flat panel's: it stays under the
// mouse, for the next press), a sound, a pulse in the pointing hand.
void pressZoom(int button, int hand)
{
    const bool done = zoomMenu(zoomLayoutShown(), button > 0 ? 1 : -1, flatPanelState.mouse);
    qvr::menuui::uiSound(done ? qvr::menuui::UiSound::Adjust : qvr::menuui::UiSound::Deny);
    if(done)
    {
        haptics::pulse(hand, 0.02f, 0.3f, 200.f);
    }
}

} // namespace

namespace qvr::menuui
{

void zoom_f()
{
    const char* names[3]{"vr", "flat", "classic"};
    if(Cmd_Argc() >= 2)
    {
        const char* what = Cmd_Argv(1);
        const int dir = !q_strcasecmp(what, "in") ? 1 : !q_strcasecmp(what, "out") ? -1 : !q_strcasecmp(what, "reset") ? 0 : 2;
        ZoomLayout layout = zoomLayoutShown();
        bool named = Cmd_Argc() < 3;
        for(int i = 0; i < 3 && !named; i++)
        {
            if(!q_strcasecmp(Cmd_Argv(2), names[i]))
            {
                layout = static_cast<ZoomLayout>(i);
                named = true;
            }
        }
        if(dir == 2 || !named)
        {
            Con_Printf("vr_menu_zoom [in | out | reset] [vr | flat | classic]\n");
            return;
        }
        // (Round the flat panel's middle: a command has no mouse.)
        const glm::vec2 at = flatCentre();
        const bool done = zoomMenu(layout, dir, at);
        Con_Printf("vr_menu_zoom: %s %s%s\n", names[static_cast<int>(layout)], what, done ? "" : " (at its limit)");
    }
    const FlatPanel& f = flatPanelState;
    const WindowRect r = flatRect();
    Con_Printf("MENUZOOM vr: vr_menu_scale %.3f (%.2f to %.2f, shipped %s)\n", vr_menu_scale.value, vrScaleMin, vrScaleMax,
        vr_menu_scale.default_string);
    Con_Printf("MENUZOOM flat: %s, vr_menu_flat_scale %.3f (%.2f to %.2f), middle %.3f %.3f, rect %.0f %.0f %.0f x %.0f of "
               "%d x %d, %s, moves %d, wheel %d\n",
        vr_menu_flat_vr_style.value ? "on" : "off", vr_menu_flat_scale.value, flatScaleMin, flatScaleMax, vr_menu_flat_x.value,
        vr_menu_flat_y.value, r.x, r.y, r.w, r.h, glwidth, glheight, f.dragging ? "dragging" : "not dragging", f.moves,
        f.wheelZooms);
    Con_Printf("MENUZOOM classic: scr_menuscale %.3f (1 to %.2f)\n", scr_menuscale.value, classicScaleMax());
    Con_Printf("MENUZOOM buttons: %s, shown %s\n", names[static_cast<int>(zoomLayoutShown())],
        tb().menu == m_state && key_dest == key_menu ? "yes" : "no");
}

bool inHeadset()
{
    return vr_menu_vr_style.value && vrActive() && key_dest == key_menu && m_state != m_none;
}

bool flatPanel()
{
    return vr_menu_flat_vr_style.value && !vrActive() && key_dest == key_menu && m_state != m_none;
}

bool active()
{
    return inHeadset() || flatPanel();
}

bool toolbarShown()
{
    if(active())
    {
        return true;
    }
    // On a flat screen (VR off; not the headset's flat-style menus, which have no laser): for the desktop mouse.
    return vr_menu_flat_shortcuts.value && !vrActive() && key_dest == key_menu && m_state != m_none;
}

float panelHeight()
{
    return inHeadset() ? vid.guiheight / canvasScale() * vr_menu_scale.value : 0.f;
}

float styledPanelHeight()
{
    // (canvasScale with the spacing setting: the most a menu's rows are spaced, the panel's height for every menu)
    return vr_menu_vr_style.value && vid.guiheight > 0 ? vid.guiheight / canvasScale() * vr_menu_scale.value : 0.f;
}

int menuHeight()
{
    return active() ? heightSetting() : 320; // (flat: the menu canvas fits 420 x 320, gl_draw.c CANVAS_MENU)
}

// A hand's laser on its menu (hits[hand]): the menu's mouse moved there (that menu selected), a tick when it moves the
// selection, the corner's buttons and the bottom rows lit under it, with a tick. False when it keeps to another panel (a
// held trigger's).
bool pointTo(int hand)
{
    const Hit& hit = hits[hand];
    if(mouseHeld[hand] && hit.menu != heldMenu[hand])
    {
        return false;
    }
    moveMouse(hit, false, mouseHeld[hand]);

    const int i = menu::currentInstance();
    Backend* be = backend();
    if(drawnHighlight[i] != seenHighlight[i] && drawnHighlight[i].menu == seenHighlight[i].menu && be && !vr_disablehaptics.value)
    {
        be->haptic(hand, 0.01f, 200.f, 0.12f);
    }

    const int hovered = toolAt(m_mousex, m_mousey);
    if(hovered >= 0 && hovered != tb().hovered)
    {
        haptics::pulse(hand, 0.01f, 0.15f, 200.f);
    }
    tb().hovered = hovered;
    const int zoomHovered = zoomAt(m_mousex, m_mousey);
    if(zoomHovered >= 0 && zoomHovered != tb().zoomHovered)
    {
        haptics::pulse(hand, 0.01f, 0.15f, 200.f);
    }
    tb().zoomHovered = zoomHovered;

    // And the spectator camera's switch, OBS's row above it, the Recording button between them, the version box's Ko-fi
    // link (or the update notice above it).
    const auto light = [hand](bool now, bool& was) {
        if(now && !was)
        {
            haptics::pulse(hand, 0.01f, 0.15f, 200.f);
        }
        was = now;
    };
    light(bannerAt(m_mousex, m_mousey), tb().bannerHovered);
    light(obsBannerAt(m_mousex, m_mousey), tb().obsHovered);
    light(renderdocAt(m_mousex, m_mousey), tb().renderdocHovered);
    light(recordingAt(m_mousex, m_mousey), tb().recordingHovered);
    light(versionLinkAt(m_mousex, m_mousey), tb().linkHovered);
    return true;
}

void update(const hands::State& s)
{
    menu::tidyInstances(); // (the menu closed, or a panel after the first left the VR pages)
    const bool on = inHeadset(); // (the laser meets the panel in the eyes, or the runtime's: intersect)
    for(int h = 0; h < HAND_COUNT; h++)
    {
        hits[h] = on ? intersect(s, h) : Hit{};
    }
    updateGrab(s, on);

    // Each laser moves the mouse of the menu it is on (a held trigger keeps to its panel). With several panels both hands
    // at once (MULTI_MENU.md, stage 4): the other hand's on a panel the pointing hand's is not on, first; then the
    // pointing hand's, whose menu takes the keys and the sticks (focused).
    bool lit[menu::instanceCount]{};
    const int other = 1 - pointingHand;
    if(on && menu::openInstances() >= 2 && hits[other].valid &&
        (!hits[pointingHand].valid || hits[other].menu != hits[pointingHand].menu))
    {
        lit[hits[other].menu] = pointTo(other);
    }
    if(on && hits[pointingHand].valid)
    {
        lit[hits[pointingHand].menu] = pointTo(pointingHand);
    }
    for(int i = 0; i < menu::instanceCount; i++)
    {
        seenHighlight[i] = drawnHighlight[i];
        if(!lit[i])
        {
            // Nothing lit under a laser on a panel no laser is on.
            Toolbar& t = toolbars[i];
            t.hovered = t.zoomHovered = -1;
            t.bannerHovered = t.obsHovered = t.renderdocHovered = t.recordingHovered = t.linkHovered = false;
        }
    }

    // The sticks' selection on them lasts while the menu stays and the laser is not moved on (a
    // hand's tremor aside).
    if(tb().focused >= 0 &&
        (!toolbarShown() || m_state != tb().focusMenu || M_WaitingForKeyBinding() ||
            (on && hits[pointingHand].valid && glm::distance(glm::vec2{m_mousex, m_mousey}, tb().focusMouse) > 12.f)))
    {
        tb().focused = -1;
    }
}

void mockLaser_f()
{
    // Trailing options: panel <n> (that menu panel, MULTI_MENU.md; else the focused one), hand off (the off hand's laser).
    int argc = Cmd_Argc();
    int target = menu::currentInstance();
    int hand = HAND_MAIN;
    while(argc >= 3)
    {
        if(!q_strcasecmp(Cmd_Argv(argc - 2), "panel"))
        {
            target = CLAMP(0, Q_atoi(Cmd_Argv(argc - 1)) - 1, menu::instanceCount - 1);
        }
        else if(!q_strcasecmp(Cmd_Argv(argc - 2), "hand"))
        {
            hand = q_strcasecmp(Cmd_Argv(argc - 1), "off") ? HAND_MAIN : HAND_OFF;
        }
        else
        {
            break;
        }
        argc -= 2;
    }
    MockLaser& m = hand == HAND_MAIN ? mockLaser : mockOffLaser;
    const int was = menu::currentInstance();
    menu::selectInstance(target); // (its menu's layout: the named spots)
    [&] {
        if(argc == 2 && !q_strcasecmp(Cmd_Argv(1), "off"))
        {
            m = {};
            return;
        }
        if(argc == 2 && !q_strcasecmp(Cmd_Argv(1), "frame"))
        {
            m = {true, {0.f, 0.f}, true};
            m.menu = target;
            pointingHand = hand;
            return;
        }
        if(float x, y; argc == 2 && (!q_strcasecmp(Cmd_Argv(1), "yes") || !q_strcasecmp(Cmd_Argv(1), "no")))
        {
            if(!M_Confirm_ButtonSpot(q_strcasecmp(Cmd_Argv(1), "yes") ? 1 : 0, &x, &y))
            {
                Con_Printf("vr_mock_laser %s: no confirmation dialog is up\n", Cmd_Argv(1));
                return;
            }
            m = {true, {x, y}};
            m.menu = target;
            pointingHand = hand;
            return;
        }
        if(float x, y; argc == 2 && !q_strcasecmp(Cmd_Argv(1), "kofi"))
        {
            if(!versionLinkSpot(x, y))
            {
                Con_Printf("vr_mock_laser kofi: the version box is not shown\n");
            }
            m = {true, {x, y}};
            m.menu = target;
            pointingHand = hand;
            return;
        }
        if(float x, y; argc == 2 && !q_strcasecmp(Cmd_Argv(1), "update"))
        {
            if(!updateLinkSpot(x, y))
            {
                Con_Printf("vr_mock_laser update: the update notice is not shown\n");
            }
            m = {true, {x, y}};
            m.menu = target;
            pointingHand = hand;
            return;
        }
        if(argc == 2 && !q_strcasecmp(Cmd_Argv(1), "obs"))
        {
            const qvr::obs::Banner o = qvr::obs::banner();
            if(!o.shown)
            {
                Con_Printf("vr_mock_laser obs: OBS's row is not shown\n");
            }
            const BannerLayout b = obsBannerLayout(toolbarLayout(), o);
            m = {true, {(b.x0 + b.x1) * 0.5f, b.yc}};
            m.menu = target;
            pointingHand = hand;
            return;
        }
        if(glm::vec2 spot; argc == 2 && !q_strcasecmp(Cmd_Argv(1), "recording"))
        {
            if(!recordingSpot(spot))
            {
                Con_Printf("vr_mock_laser recording: the Recording button is not shown\n");
            }
            m = {true, spot};
            m.menu = target;
            pointingHand = hand;
            return;
        }
        if(argc == 2 && (!q_strcasecmp(Cmd_Argv(1), "zoomin") || !q_strcasecmp(Cmd_Argv(1), "zoomout")))
        {
            const ToolbarLayout l = toolbarLayout();
            const int z = q_strcasecmp(Cmd_Argv(1), "zoomin") ? 0 : 1;
            m = {true, {l.zoomX0(z) + l.zoomSide() * 0.5f, l.zoomYc()}};
            m.menu = target;
            pointingHand = hand;
            return;
        }
        if(argc == 2 && !q_strcasecmp(Cmd_Argv(1), "spectator"))
        {
            const BannerLayout b = bannerLayout(toolbarLayout());
            m = {true, {(b.x0 + b.x1) * 0.5f, b.yc}};
            m.menu = target;
            pointingHand = hand;
            return;
        }
        if(argc == 2)
        {
            for(int t = 0; t < ToolCount; t++)
            {
                if(!q_strcasecmp(Cmd_Argv(1), toolNames[t]))
                {
                    const ToolbarLayout l = toolbarLayout();
                    m = {true, {(l.bx0(t) + l.bx1(t)) * 0.5f, l.yc(t)}};
            m.menu = target;
                    pointingHand = hand;
                    return;
                }
            }
        }
        if(argc == 3)
        {
            m = {true, {Q_atof(Cmd_Argv(1)), Q_atof(Cmd_Argv(2))}};
            m.menu = target;
            pointingHand = hand;
            return;
        }
        Con_Printf("vr_mock_laser <x> <y> | back | quicksave | quickload | main | search | console | settings | advanced | levels | maps | relighting | "
                   "toolgun | savedefaults | checklist | zoomin | zoomout | spectator | recording | obs | kofi | update | yes | no | frame | off [panel <n>] [hand off]: a hand's laser (the main one's; hand off: the other's) on that spot of the menu (panel <n>: of that menu panel) "
                   "(yes, no: a confirmation dialog's buttons)\n");
    }();
    menu::selectInstance(was);
}

void mockMouse_f()
{
    // The window's own mouse (tests of the flat screen's VR layout): press / release (the left button, as the window
    // sends it), wheel <up | down>, window <x> <y> (window pixels), corner <tl | tr | bl | br> (in that corner's grab
    // zone, just outside the panel's corner).
    FlatPanel& f = flatPanelState;
    if(Cmd_Argc() == 2 && (!q_strcasecmp(Cmd_Argv(1), "press") || !q_strcasecmp(Cmd_Argv(1), "release")))
    {
        Key_Event(K_MOUSE1, !q_strcasecmp(Cmd_Argv(1), "press"));
        Con_Printf("vr_mock_mouse: %s, menu %d, %s\n", Cmd_Argv(1), m_state, f.dragging ? "dragging the panel" : "not dragging");
        return;
    }
    if(Cmd_Argc() == 3 && !q_strcasecmp(Cmd_Argv(1), "wheel"))
    {
        const int key = q_strcasecmp(Cmd_Argv(2), "down") ? K_MWHEELUP : K_MWHEELDOWN;
        Key_Event(key, true);
        Key_Event(key, false);
        Con_Printf("vr_mock_mouse: wheel %s, menu %d, panel size %.3f\n", key == K_MWHEELUP ? "up" : "down", m_state,
            vr_menu_flat_scale.value);
        return;
    }
    glm::vec2 window{0.f};
    bool byWindow = false;
    if(Cmd_Argc() == 4 && (!q_strcasecmp(Cmd_Argv(1), "window") || !q_strcasecmp(Cmd_Argv(1), "by")))
    {
        window = glm::vec2{Q_atof(Cmd_Argv(2)), Q_atof(Cmd_Argv(3))} + (q_strcasecmp(Cmd_Argv(1), "by") ? glm::vec2{0.f} : f.mouse);
        byWindow = true;
    }
    else if(Cmd_Argc() == 3 && !q_strcasecmp(Cmd_Argv(1), "corner"))
    {
        const char* names[4]{"tl", "tr", "bl", "br"};
        for(int c = 0; c < 4; c++)
        {
            if(!q_strcasecmp(Cmd_Argv(2), names[c]))
            {
                const WindowRect r = flatRect();
                const float out = FlatPanel::grabOutside * 0.5f;
                window = {(c & 1) ? r.x + r.w + out : r.x - out, (c & 2) ? r.y + r.h + out : r.y - out};
                byWindow = true;
            }
        }
    }
    if(byWindow)
    {
        M_Mousemove(glx + static_cast<int>(za::lround(window.x)), gly + static_cast<int>(za::lround(window.y)));
        Con_Printf("vr_mock_mouse: window %.0f %.0f, menu at %.1f %.1f, corner %d, %s\n", window.x, window.y, m_mousex,
            m_mousey, f.corner, f.dragging ? "dragging the panel" : "not dragging");
        return;
    }

    glm::vec2 spot{0.f};
    int next = 0;
    if(Cmd_Argc() >= 3 && (Cmd_Argv(1)[0] == '-' || (Cmd_Argv(1)[0] >= '0' && Cmd_Argv(1)[0] <= '9')))
    {
        spot = {Q_atof(Cmd_Argv(1)), Q_atof(Cmd_Argv(2))};
        next = 3;
    }
    else if(Cmd_Argc() >= 2 && !q_strcasecmp(Cmd_Argv(1), "kofi"))
    {
        if(!versionLinkSpot(spot.x, spot.y))
        {
            Con_Printf("vr_mock_mouse kofi: the version box is not shown\n");
        }
        next = 2;
    }
    else if(Cmd_Argc() >= 2 && !q_strcasecmp(Cmd_Argv(1), "recording"))
    {
        if(!recordingSpot(spot))
        {
            Con_Printf("vr_mock_mouse recording: the Recording button is not shown\n");
        }
        next = 2;
    }
    else if(Cmd_Argc() >= 2 && !q_strcasecmp(Cmd_Argv(1), "update"))
    {
        if(!updateLinkSpot(spot.x, spot.y))
        {
            Con_Printf("vr_mock_mouse update: the update notice is not shown\n");
        }
        next = 2;
    }
    else if(Cmd_Argc() >= 2 && (!q_strcasecmp(Cmd_Argv(1), "yes") || !q_strcasecmp(Cmd_Argv(1), "no")))
    {
        if(!M_Confirm_ButtonSpot(q_strcasecmp(Cmd_Argv(1), "yes") ? 1 : 0, &spot.x, &spot.y))
        {
            Con_Printf("vr_mock_mouse %s: no confirmation dialog is up\n", Cmd_Argv(1));
        }
        next = 2;
    }
    else if(Cmd_Argc() >= 2 && (!q_strcasecmp(Cmd_Argv(1), "zoomin") || !q_strcasecmp(Cmd_Argv(1), "zoomout")))
    {
        const ToolbarLayout l = toolbarLayout();
        const int z = q_strcasecmp(Cmd_Argv(1), "zoomin") ? 0 : 1;
        spot = {l.zoomX0(z) + l.zoomSide() * 0.5f, l.zoomYc()};
        next = 2;
    }
    else if(Cmd_Argc() >= 2)
    {
        const ToolbarLayout l = toolbarLayout();
        for(int t = 0; t < ToolCount; t++)
        {
            if(!q_strcasecmp(Cmd_Argv(1), toolNames[t]))
            {
                spot = {(l.bx0(t) + l.bx1(t)) * 0.5f, l.yc(t)};
                next = 2;
            }
        }
    }
    if(next == 0)
    {
        Con_Printf("vr_mock_mouse <x> <y> | back | quicksave | quickload | main | search | console | settings | advanced | levels | maps | relighting | "
                   "toolgun | savedefaults | checklist | zoomin | zoomout | recording | kofi | update | yes | no [click]: the desktop mouse on that spot of the menu (yes, no: a "
                   "confirmation dialog's buttons), clicked with click. vr_mock_mouse window <x> <y> | by <dx> <dy> | corner <tl | tr | bl | br> | press | release | wheel <up | down>: "
                   "the window's mouse (window pixels; a corner of the flat screen's VR menu panel), its left button, its wheel\n");
        return;
    }

    // The window position M_Mousemove takes back to that spot (its sums the other way round; the flat screen's VR
    // menu panel: its rect of the window).
    drawtransform_t t;
    Draw_GetCanvasTransform(CANVAS_MENU, &t);
    const float nx = spot.x * t.scale[0] + t.offset[0];
    const float ny = spot.y * t.scale[1] + t.offset[1];
    int px = glx, py = gly, pw = glwidth, ph = glheight, fx, fy, fw, fh;
    if(VR_MenuFlatViewport(&fx, &fy, &fw, &fh))
    {
        px = glx + fx;
        py = gly + fy;
        pw = fw;
        ph = fh;
    }
    M_Mousemove(px + static_cast<int>(za::lround((nx + 1.f) * 0.5f * static_cast<float>(pw))),
        py + static_cast<int>(za::lround((1.f - ny) * 0.5f * static_cast<float>(ph))));
    Con_Printf("vr_mock_mouse: at %.1f %.1f, menu %d\n", m_mousex, m_mousey, m_state);
    if(Cmd_Argc() > next && !q_strcasecmp(Cmd_Argv(next), "click"))
    {
        Key_Event(K_MOUSE1, true);
        Key_Event(K_MOUSE1, false);
        Con_Printf("vr_mock_mouse: clicked, menu %d%s\n", m_state, key_dest == key_menu ? "" : " (closed)");
    }
}

void mockKey_f()
{
    if(Cmd_Argc() == 3 && !q_strcasecmp(Cmd_Argv(1), "text"))
    {
        // Letters typed, as a keyboard's text input gives them (Char_Event: the console's line, Search's box).
        char text[128];
        q_strlcpy(text, Cmd_Argv(2), sizeof(text));
        for(const char* c = text; *c; c++)
        {
            Char_Event(static_cast<unsigned char>(*c));
        }
        Con_Printf("vr_mock_key: typed \"%s\"\n", text);
        return;
    }
    const int key = Cmd_Argc() == 2 || Cmd_Argc() == 3 ? Key_StringToKeynum(Cmd_Argv(1)) : -1;
    if(key < 0)
    {
        Con_Printf("vr_mock_key <key> [down|up] | text <letters>: that key pressed and released, or only pressed (held) or released (a "
                   "key's name as bind takes it: uparrow, enter, shift...)\n");
        return;
    }
    char name[32];
    q_strlcpy(name, Cmd_Argv(1), sizeof(name)); // (a key's action may tokenize another command: Levels)
    const bool down = Cmd_Argc() < 3 || q_strcasecmp(Cmd_Argv(2), "up");
    const bool up = Cmd_Argc() < 3 || q_strcasecmp(Cmd_Argv(2), "down");
    // A bind's command (Key_Event adds it at the end of the command text) runs next, as a real key's does between two
    // frames -- not after the rest of a script already queued (a press, its waits and its release then all ran at the end).
    const int queued = Cbuf_Pending();
    if(down)
    {
        Key_Event(key, true);
    }
    if(up)
    {
        Key_Event(key, false);
    }
    Cbuf_TailToFront(Cbuf_Pending() - queued);
    Con_Printf("vr_mock_key: %s, menu %d, corner button %d%s\n", name, m_state, toolbarFocused() ? tb().focused : -1,
        key_dest == key_menu ? "" : " (menu closed)");
}

void printRecordingButton()
{
    const ToolbarLayout l = toolbarLayout();
    if(active())
    {
        const BannerLayout b = recordingBannerLayout(l);
        Con_Printf("menu_vr pos: recording button \"%s\" x %.0f..%.0f, y %.1f (row %d of the bottom left corner)%s%s\n",
            b.text, b.x0, b.x1, b.yc, recordingRow, tb().menu == m_state ? "" : " (not drawn)",
            tb().recordingHovered ? " (lit)" : "");
        return;
    }
    const FlatRecording& f = flatRecording;
    if(f.leftOut)
    {
        Con_Printf("menu_vr pos: recording button not drawn (%s; x %.0f..%.0f, y %.1f..%.1f, menu from x %.0f)\n", f.leftOut,
            f.x0, f.x1, f.y0, f.y1, f.contentLeft);
        return;
    }
    Con_Printf("menu_vr pos: recording button x %.0f..%.0f, y %.1f..%.1f (menu from x %.0f)%s\n", f.x0, f.x1, f.y0, f.y1,
        f.contentLeft, tb().recordingHovered ? " (lit)" : "");
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

bool pointerOn(bool hover)
{
    if(inHeadset())
    {
        return hits[pointingHand].valid && hits[pointingHand].menu == menu::currentInstance();
    }
    if(vrActive() || !ui_mouse.value)
    {
        return false; // (the headset's flat-style menus: no laser)
    }
    return !hover || (flatMouse.moved && flatMouse.frame >= host_framecount - 1);
}

float cornerRowsTop()
{
    if(!active())
    {
        return 1e9f;
    }
    const ToolbarLayout l = toolbarLayout();
    const int top = cornerTopRow();
    return bannerRowLayout(l, {"", ""}, top).yc - ToolbarLayout::half / l.k;
}

float toolbarBottom()
{
    return toolbarShown() ? toolbarLayout().buttonsBottom() : -1e9f;
}

float toolbarRight()
{
    return toolbarShown() ? toolbarLayout().right() : -1e9f;
}

float toolbarLeft()
{
    return toolbarShown() ? toolbarLayout().bx0(0) : -1e9f;
}

float toolbarTop()
{
    if(!toolbarShown())
    {
        return -1e9f;
    }
    const ToolbarLayout l = toolbarLayout();
    return l.yc(0) - l.bh() / l.k;
}

bool toolbarBeside()
{
    if(!toolbarShown())
    {
        return false;
    }
    const ToolbarLayout l = toolbarLayout();
    return !l.row && l.right() <= l.limit;
}

float statusBottom(float contentRight)
{
    if(statusBox.lines <= 0) // (none drawn: vr_menu_status 0 and the window focused)
    {
        return -1e9f;
    }
    const ToolbarLayout l = toolbarLayout();
    drawtransform_t t;
    Draw_GetCanvasTransform(CANVAS_MENU, &t);
    float left, top, right, bottom;
    Draw_GetTransformBounds(&t, &left, &top, &right, &bottom);
    const StatusMetrics m = statusMetrics();
    const float x0 = right - ToolbarLayout::corner - m.width(q_max(statusBox.widest, statusReserve));
    if(contentRight <= x0 - 2.f)
    {
        return -1e9f;
    }
    return l.top + (ToolbarLayout::corner + m.height(statusBox.lines)) / l.k;
}

float toolbarLimit()
{
    return toolbarShown() ? toolbarLayout().limit : 0.f;
}

bool toolbarRow()
{
    return toolbarShown() && toolbarLayout().row;
}

bool toolbarFocused()
{
    return toolbarShown() && tb().focused >= 0 && tb().focusMenu == m_state;
}

void focusToolbar(int dir)
{
    focusTool(dir > 0 || toolbarRow() ? ToolBack : lastTool()); // (the row: its first, from either end of a page)
    qvr::menuui::uiSound(qvr::menuui::UiSound::Move);
}

void backToGame(int hand)
{
    if(key_dest != key_menu || m_state == m_none)
    {
        return;
    }
    // (The other panels hide with the menu and come back with it, as left: only Close Panel closes one. The menu
    // reopens on the first panel's page, whichever panel this was pressed on.)
    menu::selectInstance(0);

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
    haptics::pulse(hand, 0.04f, 0.4f, 200.f);
}

// A corner button pressed (`hand` its pulse). The jumps go where the menus' own links go, so Back
// from there walks up the menus as always (Advanced VR Options: to the VR Settings; the levels: to
// Single Player), never round in a loop; the page left keeps its selection and scroll for when it is
// shown again.
void useTool(int tool, int hand)
{
    if(const char* reason = toolUnavailable(tool, true))
    {
        Con_Printf("%s: %s.\n", toolLabels[tool], reason);
        qvr::menuui::uiSound(qvr::menuui::UiSound::Move);
        return;
    }
    tb().focused = -1;
    switch(tool)
    {
        case ToolBack:
            if(menu::currentInstance() > 0)
            {
                menu::closeInstance(menu::currentInstance()); // (a panel after the first: Close Panel)
                haptics::pulse(hand, 0.04f, 0.4f, 200.f);
                break;
            }
            backToGame(hand);
            break;
        case ToolQuicksave:
            backToGame(hand);
            Cbuf_InsertText("echo Quicksaving...; wait; save quick\n");
            break;
        case ToolQuickload:
            remembered = {}; // A loaded game reopens the main menu, as the Load menu does.
            IN_Activate();
            key_dest = key_game;
            m_state = m_none;
            haptics::pulse(hand, 0.04f, 0.4f, 200.f);
            Cbuf_InsertText("echo Quickloading...; wait; load quick\n");
            break;
        case ToolMainMenu: menu::jumpToMainMenu(); break;
        case ToolSearch: menu::openSearch(); break;
        case ToolConsole: menu::openConsole(); break;
        case ToolSettings: menu::jumpToSettings(); break;
        case ToolAdvanced: menu::jumpToAdvanced(); break;
        case ToolLevels:
            if(m_state == m_maps)
            {
                qvr::menuui::uiSound(qvr::menuui::UiSound::Move);
                break;
            }
            VR_NavJump(m_maps); // (Back from them: here)
            Cmd_TokenizeString("menu_maps"); // (it looks at its command's arguments)
            M_Menu_Maps_f();
            break;
        case ToolToolgun:
        {
            // (impulse 150 + 19 / 170 + 19: the weapon in that hand; the off hand when the main one holds a weapon)
            const int mainWpn = cl.stats[protocol::STAT_QVR_WEAPON], offWpn = cl.stats[protocol::STAT_QVR_WEAPON2];
            backToGame(hand);
            if(mainWpn == qvr::toolgun::weaponId || offWpn == qvr::toolgun::weaponId)
            {
                Con_Printf("Toolgun: already in a hand.\n");
                break;
            }
            Cbuf_InsertText(mainWpn != 0 ? "wait; impulse 189\n" : "wait; impulse 169\n");
            Con_Printf("Toolgun: in the %s hand.\n", mainWpn != 0 ? "off" : "main");
            break;
        }
        case ToolSaveDefaults:
        {
            SaveDefaultsButton& s = saveDefaultsButton;
            if(realtime - s.armedAt > saveDefaultsConfirm)
            {
                s.armedAt = realtime;
                s.savedAt = -1e9;
                Con_Printf("Save Default: press again within %.0f seconds to save your settings to disk and write the "
                           "changed ones for review.\n",
                    saveDefaultsConfirm);
                qvr::menuui::uiSound(qvr::menuui::UiSound::Move);
                haptics::pulse(hand, 0.04f, 0.4f, 200.f);
                break;
            }
            s.armedAt = -1e9;
            za::String path;
            s.saved = proposedefaults::write(path); // (prints "Saved N settings for review: <path>")
            s.savedAt = realtime;
            S_LocalSound(s.saved >= 0 ? "misc/menu2.wav" : "misc/menu1.wav");
            haptics::pulse(hand, 0.08f, 0.8f, 200.f);
            break;
        }
        case ToolChecklist: menu::jumpToChecklist(); break;
        case ToolMaps: menu::openMaps(); break;
        case ToolRelighting: menu::jumpToRelighting(); break;
        case ToolNewPanel: menu::openInstance(); break;
        default: break;
    }
}

namespace
{
// scrollStick's stick (the menu's frame).
struct ScrollStick
{
    bool pushed = false;
    double carry = 0.0;
    double last = 0.0;
};
ScrollStick scrollStickState;
} // namespace

bool scrollStick(float y)
{
    bool& pushed = scrollStickState.pushed;
    double& carry = scrollStickState.carry;
    double& last = scrollStickState.last;
    const double dt = realtime - last;
    last = realtime;

    if(key_dest != key_menu || !scrollMenu(0))
    {
        pushed = false;
        return false;
    }

    // Past a dead zone, 3 rows a second up to 25 at full push; the first row at once.
    constexpr float deadzone = 0.2f;
    const float t = (za::fabs(y) - deadzone) / (1.f - deadzone);
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
        haptics::pulse(HAND_MAIN, 0.005f, 0.06f, 200.f);
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
            if(menu::instanceOpen(heldMenu[hand]))
            {
                menu::selectInstance(heldMenu[hand]); // (released on the menu it pressed)
            }
            return K_MOUSE1;
        }
        return key;
    }

    // Binding a key takes the trigger as itself.
    if(!inHeadset() || M_WaitingForKeyBinding())
    {
        return key;
    }

    pointingHand = hand;
    if(!hits[hand].valid)
    {
        return key;
    }
    // One mouse button: the other hand's click (still held, on another panel or this one) is let go of first.
    const int other = 1 - hand;
    if(mouseHeld[other])
    {
        mouseHeld[other] = false;
        if(menu::instanceOpen(heldMenu[other]))
        {
            menu::selectInstance(heldMenu[other]);
        }
        Key_Event(K_MOUSE1, false);
    }
    moveMouse(hits[hand], true, true); // the click lands where it points now (its menu focused)
    mouseHeld[hand] = true;
    heldMenu[hand] = hits[hand].menu;
    return K_MOUSE1;
}

bool spectatorPreviewWanted()
{
    return inHeadset() && previewPlaced();
}

void drawHandLaser(const hands::State& s, int h); // (a hand's laser and the frame it lights)

void drawInEye(const hands::State& s)
{
    if(!inHeadset() || !s.valid)
    {
        return;
    }
    drawPreview(s);
    drawHandLaser(s, pointingHand);
    // With several panels, the other hand's laser too where it is on (or holds) a panel the pointing hand's is not on: one
    // laser a panel, each moving its own panel's mouse (update).
    const int other = 1 - pointingHand;
    if(menu::openInstances() >= 2 &&
        (panelGrab.hand == other || ((hits[other].valid || hits[other].frame) &&
                                        (!hits[pointingHand].valid || hits[other].menu != hits[pointingHand].menu))))
    {
        drawHandLaser(s, other);
    }
}

void drawHandLaser(const hands::State& s, int h)
{
    // At the pose the eyes are drawn with.
    glm::vec3 start, dir;
    if(!pointerRay(s, h, start, dir))
    {
        return;
    }
    const Hit hit = intersect(s, h);

    // The panel's frame lit while the laser is on it or the panel is held (where a grip takes hold of it).
    const bool holding = panelGrab.hand == h;
    if(glm::vec3 corner, xAxis, yAxis; (holding || hit.frame) && panel::menuQuad(s, holding ? panelGrab.menu : hit.menu, corner, xAxis, yAxis))
    {
        za::Vector<gfx::Vertex>& vertices = scratch.frame;
        vertices.clear();
        const float band = frameBand * glm::length(yAxis);
        const glm::vec3 x = glm::normalize(xAxis) * (band * 0.3f);
        const glm::vec3 y = glm::normalize(yAxis) * (band * 0.3f);
        const glm::vec3 c[4]{corner - x - y, corner + xAxis + x - y, corner + xAxis + yAxis + x + y, corner + yAxis - x + y};
        const glm::vec4 colour = hue::color(vr_menu_laser_hue, 0.5f, 1.f, holding ? 0.9f : 0.6f);
        for(int i = 0; i < 4; i++)
        {
            appendStrip(vertices, c[i], c[(i + 1) % 4], band * (holding ? 0.15f : 0.1f), colour, colour);
        }
        gfx::draw(vertices, gfx::sceneViewProjection(),
            {.shade = gfx::Shade::SoftEdge, .blend = gfx::Blend::Alpha, .depthTest = false, .depthWrite = false});
    }

    if(holding)
    {
        glm::vec3 origin;
        glm::mat3 basis;
        if(pointerFrame(s, h, origin, basis))
        {
            drawLaser(start, origin + basis * panelGrab.spot, true, true, 1.f);
        }
        return;
    }
    drawLaser(start, hit.valid || hit.frame ? hit.point : start + dir * 30.f, hit.valid || hit.frame, mouseHeld[h], 1.f);
}

bool gripButton(int hand, bool down)
{
    if(!down)
    {
        if(panelGrab.armedHand == hand)
        {
            panelGrab.armedHand = -1;
        }
        if(panelGrab.hand == hand)
        {
            panelGrab.hand = -1;
        }
        return za::exchange(panelGrab.pressTaken[hand], false);
    }

    const Hit& hit = hits[hand];
    if(grabMode() <= 0 || !inHeadset() || M_WaitingForKeyBinding() || panelGrab.hand >= 0 || hit.onRuntimePanel)
    {
        return false;
    }
    if(hit.frame)
    {
        if(takeHold(hands::current(), hand, hit.menu, hit.point))
        {
            panelGrab.pressTaken[hand] = true;
            haptics::pulse(hand, 0.03f, 0.3f, 200.f);
            return true;
        }
        return false;
    }
    glm::vec3 origin;
    glm::mat3 basis;
    if(grabMode() >= 2 && hit.valid && !mouseHeld[hand] && pointerFrame(hands::current(), hand, origin, basis))
    {
        panelGrab.armedHand = hand; // (the grip goes on to the menu: the fine modifier until the hand moves)
        panelGrab.armedAt = origin;
        panelGrab.armedSpot = hit.point;
        panelGrab.armedMenu = hit.menu;
        panelGrab.sticksMoved = false;
    }
    return false;
}

void sticksPushed()
{
    if(panelGrab.armedHand >= 0)
    {
        panelGrab.sticksMoved = true;
    }
}

void panelPlace_f()
{
    if(Cmd_Argc() == 2 && !q_strcasecmp(Cmd_Argv(1), "reset"))
    {
        panel::unplaceMenu();
        return;
    }
    glm::vec3 corner, xAxis, yAxis;
    const hands::State& s = hands::current();
    if(!s.valid || !panel::menuQuad(s, corner, xAxis, yAxis))
    {
        Con_Printf("MENUPANEL not shown in the eyes\n");
        return;
    }
    const glm::vec3 centre = corner + (xAxis + yAxis) * 0.5f;
    const glm::vec3 facing = glm::normalize(glm::cross(yAxis, xAxis));
    const glm::vec3 fromHead = centre - s.head;
    Con_Printf("MENUPANEL placed %d held %d armed %d centre %.1f %.1f %.1f from head %.1f %.1f %.1f facing yaw %.1f pitch %.1f "
               "size %.1f x %.1f\n",
        panel::menuPlaced() ? 1 : 0, panelGrab.hand, panelGrab.armedHand, centre.x, centre.y, centre.z, fromHead.x, fromHead.y,
        fromHead.z, glm::degrees(za::atan2(facing.y, facing.x)), glm::degrees(za::asin(CLAMP(-1.f, facing.z, 1.f))),
        glm::length(xAxis), glm::length(yAxis));
}

void drawLaser(const glm::vec3& start, const glm::vec3& end, bool onPanel, bool held, float size)
{
    za::Vector<gfx::Vertex>& vertices = scratch.laser;
    vertices.clear();
    const float bright = held ? 1.f : 0.8f;
    // In the player's hue (vr_menu_laser_hue; 35 its old amber, the menus' own).
    appendStrip(vertices, start, end, 0.3f * size, hue::color(vr_menu_laser_hue, 0.6f, 1.f, 0.9f * bright),
        hue::color(vr_menu_laser_hue, 0.5f, 1.f, onPanel ? 0.5f * bright : 0.f));
    if(onPanel)
    {
        appendStrip(vertices, end, end, 1.6f * size, hue::color(vr_menu_laser_hue, 0.3f, 1.f, 1.f), {});
        appendStrip(vertices, end, end, 0.7f * size, {1.f, 1.f, 1.f, 1.f}, {});
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

extern "C" int VR_MenuFlatViewport(int* x, int* y, int* w, int* h)
{
    if(!menuui::flatPanel() || glwidth <= 0 || glheight <= 0)
    {
        return 0;
    }
    const WindowRect r = flatRect();
    *x = static_cast<int>(r.x);
    *y = static_cast<int>(r.y);
    *w = static_cast<int>(r.w);
    *h = static_cast<int>(r.h);
    return 1;
}

extern "C" int VR_MenuFlatMouse(int screenx, int screeny)
{
    FlatPanel& f = flatPanelState;
    f.mouse = {static_cast<float>(screenx - glx), static_cast<float>(screeny - gly)};
    if(f.dragging && (!keydown[K_MOUSE1] || !menuui::flatPanel()))
    {
        f.dragging = false; // (let go: VR_MenuDrawFlatPanel notices it too, without a move)
    }
    if(!menuui::flatPanel())
    {
        f.corner = -1;
        showFlatCursor(false);
        return 0;
    }
    if(f.dragging)
    {
        setFlatPlace(flatScale(), f.mouse - f.grab);
        f.moves++;
        return 1;
    }
    f.corner = flatCornerAt(f.mouse);
    showFlatCursor(f.corner >= 0);
    return 0;
}

extern "C" void VR_MenuDrawFlatPanel()
{
    FlatPanel& f = flatPanelState;
    if(f.dragging && (!keydown[K_MOUSE1] || !menuui::flatPanel()))
    {
        f.dragging = false;
        f.corner = flatCornerAt(f.mouse);
        showFlatCursor(f.corner >= 0 && menuui::flatPanel());
    }
    if(!menuui::flatPanel() || glcanvas.type != CANVAS_MENU)
    {
        return;
    }
    // The panel's face: as dark as the headset's panel over the game (the window's fade is under it, all round it).
    const Painter p;
    drawtransform_t t;
    Draw_GetCanvasTransform(CANVAS_MENU, &t);
    float left, top, right, bottom;
    Draw_GetTransformBounds(&t, &left, &top, &right, &bottom);
    p.rect(left, right, (top + bottom) * 0.5f, (bottom - top) * p.k * 0.5f, {0.035f, 0.028f, 0.022f, 0.88f});
}

extern "C" void VR_MenuDrawFlatFrame()
{
    if(!menuui::flatPanel())
    {
        return;
    }
    GL_SetCanvas(CANVAS_DEFAULT); // (the window's viewport again: the handles reach outside the panel)
    const FlatPanel& f = flatPanelState;
    const WindowRect r = flatRect();
    // Window pixels to the default canvas's units (the window's virtual screen).
    const float gx = static_cast<float>(vid.guiwidth) / static_cast<float>(glwidth);
    const float gy = static_cast<float>(vid.guiheight) / static_cast<float>(glheight);
    const auto fill = [gx, gy](float x, float y, float w, float h, const glm::vec4& c) {
        const float rgb[3]{c.r, c.g, c.b};
        Draw_FillEx(x * gx, y * gy, w * gx, h * gy, rgb, c.a);
    };
    // Its frame, a line round it.
    glm::vec4 edge = colors::boxBorder;
    edge.a = 0.8f;
    fill(r.x - 1.f, r.y - 1.f, r.w + 2.f, 1.f, edge);
    fill(r.x - 1.f, r.y + r.h, r.w + 2.f, 1.f, edge);
    fill(r.x - 1.f, r.y, 1.f, r.h, edge);
    fill(r.x + r.w, r.y, 1.f, r.h, edge);
    // Each corner's handle just outside it: an L, lit under the mouse and while held.
    constexpr float thick = 3.f, off = 3.f, arm = FlatPanel::handle;
    for(int c = 0; c < 4; c++)
    {
        const bool right = (c & 1) != 0, bottomSide = (c & 2) != 0;
        const bool lit = f.corner == c && (f.dragging || ui_mouse.value);
        glm::vec4 ink = lit ? colors::highlightEdge : colors::fill;
        ink.a = lit ? 1.f : 0.55f;
        const float ox = right ? r.x + r.w + off : r.x - off; // the L's inner corner (off the panel's)
        const float oy = bottomSide ? r.y + r.h + off : r.y - off;
        fill(right ? ox + thick - arm : ox - thick, bottomSide ? oy : oy - thick, arm, thick, ink); // its arm across
        fill(right ? ox : ox - thick, bottomSide ? oy + thick - arm : oy - thick, thick, arm, ink); // and down
    }
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

bool qvr::menuui::drawProgress(int x0, int x1, int y, float fraction)
{
    if(!styled())
    {
        return false;
    }
    const Painter p;
    const float yc = y + 4.f;
    p.rounded(static_cast<float>(x0), static_cast<float>(x1), yc, 3.f, 3.f, colors::track);
    if(fraction > 0.f)
    {
        // (at least its rounded ends: a sliver shows that it started)
        const float end = x0 + q_max(6.f, (x1 - x0) * CLAMP(0.f, fraction, 1.f));
        p.rounded(static_cast<float>(x0), end, yc, 3.f, 3.f, colors::fill);
    }
    return true;
}

// The death menu opening (menu.c M_CheckDeathMenu): the panel in front of where you look now, not where a hand left it
// (moved with the body, it can be anywhere round a dead player; author's note 2026-10-20: no death menu seen in VR).
extern "C" void VR_MenuPanelInFront(void)
{
    if(panelGrab.hand < 0)
    {
        panel::unplaceMenu();
    }
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
    menuui::paintSwitch(static_cast<float>(x), static_cast<float>(y), on != 0);
    return 1;
}

void qvr::menuui::paintSwitch(float x, float y, bool on)
{
    const Painter p;
    const float yc = y + 4.f;
    p.rounded(x, x + 19.f, yc, 4.25f, 4.25f, on ? colors::fill : colors::switchOff);
    p.disc(on ? x + 14.75f : x + 4.25f, yc, 3.f, on ? colors::thumb : colors::knobOff);
}

void qvr::menuui::paintSlider(int x, int y, float range, const char* desc)
{
    drawStyledSlider(x, y, range, -1.f, 0, desc);
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
    if(menuui::toolbarFocused())
    {
        return 1;
    }
    if(!styled())
    {
        return 0;
    }

    drawnHighlight[menu::currentInstance()] = {m_state, cy};

    const Painter p;
    const float left = za::fmin(cx - 4.f, 8.f);
    const float right = 320.f - left;
    const float yc = cy + 4.f;
    p.rounded(left, right, yc, 5.5f, 2.f, colors::highlight);
    p.rect(left, left + 1.5f, yc, 4.5f, colors::highlightEdge);
    return 0;
}

// A confirmation dialog's button (M_Confirm): x0..x1 across, round its label's row y; the selected one (the laser's, the
// keys') lit.
extern "C" int VR_MenuDrawButton(int x0, int x1, int y, int selected)
{
    if(!styled())
    {
        return 0;
    }
    menuui::paintButton(static_cast<float>(x0), static_cast<float>(x1), y + 4.f, 8.f, selected != 0);
    return 1;
}

void qvr::menuui::paintButton(float x0, float x1, float yc, float half, bool lit)
{
    const Painter p;
    p.rounded(x0, x1, yc, half, 3.f, lit ? colors::highlightEdge : colors::boxBorder);
    p.rounded(x0 + 1.f, x1 - 1.f, yc, half - 1.f, 2.f, lit ? colors::buttonHover : colors::track);
}

bool qvr::menuui::rowHighlight(float x0, float x1, float yc, float half)
{
    if(menuui::toolbarFocused() || !styled())
    {
        return false;
    }
    drawnHighlight[menu::currentInstance()] = {m_state, static_cast<int>(yc - 3.5f)};
    const Painter p;
    p.rounded(x0, x1, yc, half, 2.f, colors::highlight);
    p.rect(x0, x0 + 1.5f, yc, half - 1.f, colors::highlightEdge);
    return true;
}

void qvr::menuui::paintFoldArrow(float xc, float yc, float size, float open)
{
    // An equilateral triangle, its tip at `angle` (0 right, 90 degrees down), filled a canvas unit high at a time.
    const Painter p;
    const float angle = CLAMP(0.f, open, 1.f) * 1.5707963f;
    float vx[3], vy[3];
    for(int v = 0; v < 3; v++)
    {
        const float a = angle + static_cast<float>(v) * 2.0943951f;
        vx[v] = size * cosf(a);
        vy[v] = size * sinf(a);
    }
    const float top = za::fmin(vy[0], za::fmin(vy[1], vy[2]));
    const float bottom = za::fmax(vy[0], za::fmax(vy[1], vy[2]));
    for(float t = top; t < bottom; t += p.step)
    {
        const float t1 = za::fmin(t + p.step, bottom);
        const float ym = (t + t1) * 0.5f;
        float lo = 1e9f, hi = -1e9f;
        for(int e = 0; e < 3; e++)
        {
            const int f = (e + 1) % 3;
            if((vy[e] <= ym) == (vy[f] <= ym))
            {
                continue; // (the line misses this edge)
            }
            const float x = vx[e] + (ym - vy[e]) * (vx[f] - vx[e]) / (vy[f] - vy[e]);
            lo = za::fmin(lo, x);
            hi = za::fmax(hi, x);
        }
        if(lo < hi)
        {
            p.band(xc + lo, xc + hi, yc, t, t1, colors::fill);
        }
    }
}

// Menu Look: the big-letter menus' selected row (main menu, Single Player, Multiplayer, Skill: vr_menu_big_hl 1 and 2):
// a soft bar behind it, x0..x1 round yc (menu y), `half` true pixels each way, fading out at its edges (a glow).
extern "C" void VR_MenuDrawBigHighlight(float x0, float x1, float yc, float half)
{
    if(vr_menu_big_hl.value < 1.f || menuui::toolbarFocused() || !styled())
    {
        return;
    }
    const Painter p;
    constexpr glm::vec4 glow{1.f, 0.62f, 0.25f, 0.07f};
    for(int ring = 3; ring >= 1; ring--)
    {
        const float grow = 2.f * static_cast<float>(ring);
        p.rounded(x0 - grow, x1 + grow, yc, half + grow, 4.f + grow, glow);
    }
    p.rounded(x0, x1, yc, half, 4.f, colors::highlight);
    p.rect(x0, x0 + 2.f, yc, half - 2.f, colors::highlightEdge);
}

// Menu Look: the main menu's selected row's size (vr_menu_big_hl 2: vr_menu_big_hl_scale; 1 otherwise), and the gap
// between its groups over Quake's (vr_menu_main_gap).
extern "C" float VR_MenuBigGrow(void)
{
    return vr_menu_big_hl.value >= 2.f ? CLAMP(1.f, vr_menu_big_hl_scale.value, 1.25f) : 1.f;
}

extern "C" float VR_MenuMainGap(void)
{
    return CLAMP(0.f, vr_menu_main_gap.value, 3.f);
}

void qvr::menuui::drawListHighlight(float x0, float x1, int y)
{
    const Painter p;
    const float yc = y + 4.f;
    p.rounded(x0, x1, yc, 4.5f, 2.f, colors::listHover);
    p.rect(x0, x0 + 1.5f, yc, 3.5f, colors::highlightEdge);
}

// The vertical Quake plaque on the options pages: with the VR style's taller panel the rows reach
// down past it, so it is left out (the corner's buttons stand by the title instead).
extern "C" int VR_MenuHidesPlaque()
{
    return qvr::menuui::active();
}

// The menus that lay out from the canvas's bounds (Ironwail's lists: levels, mods, options, key bindings). In the
// headset (the VR menu style): no wider than leaves the corner's column where it stands on the other menus, left of
// them (columnRight, columnGap), and from the top, beside it (below the buttons only where a narrow panel puts them
// over the lists; below the status box where a list reaches under it). Not under a flat screen's row of icons,
// which stays above their tops (their bounds 10 below the canvas's top at least, their titles 4 more).
extern "C" void VR_MenuBounds(int* left, int* top, int* width, int* height)
{
    if(menuui::toolbarRow())
    {
        return;
    }
    if(menuui::active())
    {
        const int nearest = static_cast<int>(ToolbarLayout::columnRight + ToolbarLayout::columnGap);
        if(*left < nearest)
        {
            const int centre = *left + *width / 2;
            *width = q_max(320, 2 * (centre - nearest)) & ~15;
            *left = centre - *width / 2;
        }
    }
    // (The lists centred: their right edge as far right of x 160 as their left is left of it.)
    const float bottom = za::fmax(menuui::toolbarBeside() ? -1e9f : menuui::toolbarBottom(),
        menuui::active() ? menuui::statusBottom(320.f - menu::contentLeft()) : -1e9f);
    const int below = static_cast<int>(za::ceil(bottom)) + 4;
    if(bottom < -1e8f || below <= *top)
    {
        return;
    }
    const int end = *top + *height;
    *top = below;
    *height = q_max(end - below, 0) & ~7;
}

namespace
{

// A button's icon, `x` its left, `yc` its middle: Back to game's arrow, Search's magnifying glass, the console's
// prompt, VR Settings' headset, Advanced VR's sliders, the levels' flag, the Map Library's map, Relighting's sun, the
// checklist's lines.
void drawToolIcon(const Painter& p, int tool, float x, float yc, const glm::vec4& ink)
{
    const float w = ToolbarLayout::icon;
    switch(tool)
    {
        case ToolBack:
            if(menu::currentInstance() > 0)
            {
                // Close Panel (a menu panel after the first): a cross.
                for(int i = 0; i < 7; i++)
                {
                    const float t = static_cast<float>(i) * 1.2f - 3.6f;
                    p.disc(x + 4.5f + t, yc + t / p.k, 0.9f, ink);
                    p.disc(x + 4.5f + t, yc - t / p.k, 0.9f, ink);
                }
                break;
            }
            p.arrowHead(x, 5.f, yc, 4.5f, ink);
            p.rect(x + 4.f, x + w, yc, 1.25f, ink);
            break;
        case ToolNewPanel:
            // Another panel: a sheet, a plus beside it.
            p.band(x, x + 5.f, yc, -4.5f, 4.5f, ink);
            p.band(x + 1.f, x + 4.f, yc, -3.5f, 3.5f, colors::boxFill);
            p.rect(x + 5.5f, x + w, yc, 0.7f, ink);
            p.band(x + 6.65f, x + 7.85f, yc, -1.75f, 1.75f, ink);
            break;
        case ToolQuicksave:
        case ToolQuickload:
            // A tray with a down arrow to save, an up arrow to load.
            p.band(x, x + 1.3f, yc, 1.5f, 4.5f, ink);
            p.band(x + w - 1.3f, x + w, yc, 1.5f, 4.5f, ink);
            p.rect(x, x + w, yc + 4.f / p.k, 0.6f, ink);
            p.band(x + 3.9f, x + 5.1f, yc, -4.5f, 0.f, ink);
            for(int i = 0; i < 3; i++)
            {
                const float t = static_cast<float>(i);
                const float y = tool == ToolQuicksave ? -1.f + t : -2.5f - t;
                p.rect(x + 2.f + t, x + 7.f - t, yc + y / p.k, 0.6f, ink);
            }
            break;
        case ToolMainMenu:
            // A house: a stepped roof over its walls, a door in them.
            for(int i = 0; i < 4; i++)
            {
                const float t = static_cast<float>(i);
                p.rect(x + t * 1.1f, x + w - t * 1.1f, yc - (0.6f + t * 1.2f) / p.k, 0.6f, ink);
            }
            p.band(x + 1.5f, x + w - 1.5f, yc, 0.f, 4.5f, ink);
            p.band(x + 3.6f, x + w - 3.6f, yc, 1.8f, 4.5f, colors::boxFill);
            break;
        case ToolSearch:
            // A magnifying glass: a ring, its handle down to the right.
            p.disc(x + 3.5f, yc - 1.f / p.k, 3.5f, ink);
            p.disc(x + 3.5f, yc - 1.f / p.k, 2.f, colors::boxFill);
            for(int i = 0; i < 4; i++)
            {
                const float t = static_cast<float>(i);
                p.rect(x + 5.5f + t, x + 7.f + t, yc + (2.f + t) / p.k, 0.9f, ink);
            }
            break;
        case ToolConsole:
            // A prompt: ">" and "_".
            p.arrowHeadRight(x + 4.f, 4.f, yc - 1.f / p.k, 3.f, ink);
            p.rect(x + 5.f, x + w, yc + 3.f / p.k, 0.8f, ink);
            break;
        case ToolSettings:
            // A headset: its visor (two lenses in it, a notch for the nose under them), a strap over it.
            p.rect(x + 1.5f, x + w - 1.5f, yc - 3.6f / p.k, 0.6f, ink);
            p.rounded(x, x + w, yc + 0.5f / p.k, 2.8f, 1.5f, ink);
            p.disc(x + 2.6f, yc + 0.2f / p.k, 1.25f, colors::boxFill);
            p.disc(x + w - 2.6f, yc + 0.2f / p.k, 1.25f, colors::boxFill);
            p.band(x + 3.9f, x + w - 3.9f, yc, 2.f, 3.4f, colors::boxFill);
            break;
        case ToolRelighting:
            // A sun: a disc, its rays round it.
            p.disc(x + 4.5f, yc, 2.f, ink);
            p.rect(x, x + 1.4f, yc, 0.6f, ink);
            p.rect(x + w - 1.4f, x + w, yc, 0.6f, ink);
            p.band(x + 3.9f, x + 5.1f, yc, -4.8f, -3.3f, ink);
            p.band(x + 3.9f, x + 5.1f, yc, 3.3f, 4.8f, ink);
            for(int i = 0; i < 4; i++)
            {
                const float dx = i & 1 ? 3.2f : -3.2f, dy = i & 2 ? 3.2f : -3.2f;
                p.disc(x + 4.5f + dx, yc + dy / p.k, 0.75f, ink);
            }
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
        case ToolMaps:
            // A map: a sheet with a fold down its middle, a place marked on it.
            p.rect(x, x + w, yc - 4.f / p.k, 0.7f, ink);
            p.rect(x, x + w, yc + 4.f / p.k, 0.7f, ink);
            p.band(x, x + 1.2f, yc, -4.7f, 4.7f, ink);
            p.band(x + w - 1.2f, x + w, yc, -4.7f, 4.7f, ink);
            p.band(x + 4.4f, x + 5.6f, yc, -4.7f, 4.7f, ink);
            p.disc(x + 2.6f, yc - 1.2f / p.k, 1.3f, ink);
            break;
        case ToolToolgun:
            // A pistol: a barrel, a grip down from it.
            p.band(x, x + w, yc, -3.f, 0.f, ink);
            p.band(x + 1.f, x + 4.f, yc, 0.f, 4.5f, ink);
            p.band(x + w - 1.2f, x + w, yc, -4.f, -3.f, ink);
            break;
        case ToolSaveDefaults:
            // Two sliders over a tray (settings kept).
            p.band(x, x + 1.3f, yc, 1.5f, 4.5f, ink);
            p.band(x + w - 1.3f, x + w, yc, 1.5f, 4.5f, ink);
            p.rect(x, x + w, yc + 4.f / p.k, 0.6f, ink);
            for(int i = 0; i < 2; i++)
            {
                const float y = yc + (-3.5f + i * 3.f) / p.k;
                const float knob = x + (i == 0 ? 3.f : 6.f);
                p.rect(x + 1.f, x + w - 1.f, y, 0.6f, ink);
                p.rect(knob - 1.f, knob + 1.f, y, 1.3f, ink);
            }
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

// The Recording button's icon (as the corner's buttons' icons, `x` its left): a film camera, its body and its lens's
// hood opening to the right.
void drawRecordingIcon(const Painter& p, float x, float yc, const glm::vec4& ink)
{
    const float w = ToolbarLayout::icon;
    p.rounded(x, x + 6.f, yc, 2.8f, 1.f, ink);
    p.arrowHead(x + 5.f, w - 5.f, yc, 3.2f, ink);
}

} // namespace

// The status box (vr_menu_status), over every menu, in VR and on a flat screen: in the canvas's top right corner, its
// lines (qvr::statusLines) in small letters on a box like the corner's buttons.
extern "C" void VR_MenuDrawStatus()
{
    // The desktop window without the focus: a warning first (white on red), with the box's lines or alone.
    const bool unfocused = windowUnfocused();
    if((!vr_menu_status.value && !unfocused) || M_WaitingForKeyBinding())
    {
        statusBox.lines = M_WaitingForKeyBinding() ? statusBox.lines : 0;
        return;
    }
    GL_SetCanvas(CANVAS_MENU);
    auto& lines = scratch.status;
    auto& own = scratch.statusOwn;
    lines.clear();
    if(unfocused)
    {
        lines.pushBack(za::String{"! Game window not focused:"});
        lines.pushBack(za::String{"! input/performance may suffer;"});
        lines.pushBack(za::String{"! click the game's desktop window"});
    }
    const za::SizeT ownFirst = lines.size(); // (the mode's line)
    if(vr_menu_status.value)
    {
        statusLines(own);
        for(const za::String& line : own)
        {
            lines.pushBack(line);
        }
    }
    if(lines.empty())
    {
        statusBox.lines = 0;
        return;
    }
    const Painter p;
    const ToolbarLayout l = toolbarLayout();
    float right = 0.f;
    {
        drawtransform_t t;
        Draw_GetCanvasTransform(CANVAS_MENU, &t);
        float left, top, bottom;
        Draw_GetTransformBounds(&t, &left, &top, &right, &bottom);
    }
    const StatusMetrics m = statusMetrics();
    const float size = m.size, step = m.step, pad = m.pad;
    za::SizeT widest = 0;
    for(const za::String& line : lines)
    {
        widest = line.size() > widest ? line.size() : widest;
    }
    statusBox.lines = static_cast<int>(lines.size());
    statusBox.widest = static_cast<int>(widest);
    const float width = m.width(statusBox.widest);
    const float x1 = right - ToolbarLayout::corner;
    const float x0 = x1 - width;
    const float height = m.height(statusBox.lines);
    const float y0 = l.top + ToolbarLayout::corner / l.k;
    const float yc = y0 + height * 0.5f / p.k;    p.rounded(x0, x1, yc, height * 0.5f, 3.f, colors::boxBorder);
    p.rounded(x0 + 1.f, x1 - 1.f, yc, height * 0.5f - 1.f, 2.f, colors::boxFill);
    for(za::SizeT i = 0; i < lines.size(); i++)
    {
        // A row's middle (menu y), where its characters are centred at their own size.
        const float ym = y0 + (pad + step * static_cast<float>(i) + size * 0.5f) / p.k;
        float x = x0 + pad;
        // A warning (the eyes too large: statusLines), from its '!': white on a red band (a hue alone is lost under
        // the menus' tint).
        const bool warning = lines[i].cStr()[0] == '!';
        if(warning)
        {
            p.rounded(x0 + 2.f, x1 - 2.f, ym, step * 0.5f, 1.5f, colors::recording);
        }
        for(const char* c = lines[i].cStr(); *c; c++, x += size)
        {
            // The first line (the mode) white, the rest in the menus' tan.
            Draw_CharacterEx(x, ym - size * 0.5f, size, size, i == ownFirst || warning ? *c : (*c | 128));
        }
    }
}

// The corner's buttons (over every menu, not while a key is being bound): their labels where they fit
// left of the menu (toolbarLayout), else only their icons. In the headset with the VR menu style, and on a flat screen
// (vr_menu_flat_shortcuts), where the desktop mouse lights them up.
extern "C" void VR_MenuDrawOverlay()
{
    tb().menu = m_none;
    flatRecording.leftOut = menuui::active() ? "the headset's row instead" : "the corner's buttons not drawn";
    if(!menuui::inHeadset() && key_dest == key_menu)
    {
        // (Whether or not the buttons are shown: the version box's link lights up under the mouse too.)
        const glm::vec2 mouse{m_mousex, m_mousey};
        FlatMouse& fm = flatMouse;
        fm.moved = fm.frame >= host_framecount - 1 && (fm.moved || mouse != fm.seen); // (afresh when the menu opens)
        fm.seen = mouse;
        fm.frame = host_framecount;
    }
    if(!menuui::toolbarShown() || glcanvas.type != CANVAS_MENU || M_WaitingForKeyBinding())
    {
        return;
    }

    const Painter p;
    const ToolbarLayout l = toolbarLayout();
    if(!menuui::inHeadset())
    {
        const FlatMouse& fm = flatMouse;
        tb().hovered = tb().zoomHovered = -1;
        for(int t = 0; fm.moved && ui_mouse.value && t < toolsShown(); t++)
        {
            if(!toolHidden(t) && l.hit(t, m_mousex, m_mousey))
            {
                tb().hovered = t;
            }
        }
        for(int z = 0; fm.moved && ui_mouse.value && z < 2; z++)
        {
            if(l.zoomHit(z, m_mousex, m_mousey))
            {
                tb().zoomHovered = z;
                tb().hovered = -1; // (the row's last icon's click reaches to them: they come first, as in toolAt)
            }
        }
    }
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
    for(int t = 0; t < toolsShown(); t++)
    {
        if(toolHidden(t))
        {
            continue;
        }
        const bool enabled = !toolUnavailable(t);
        const bool selected = tb().hovered == t || (menuui::toolbarFocused() && tb().focused == t);
        const bool hot = enabled && selected;
        const float yc = l.yc(t);
        const float x0 = l.bx0(t), x1 = l.bx1(t);
        p.rounded(x0, x1, yc, l.bh(), 3.f, selected ? colors::highlightEdge : colors::boxBorder);
        p.rounded(x0 + 1.f, x1 - 1.f, yc, l.bh() - 1.f, 2.f, hot ? colors::buttonHover : colors::boxFill);

        const glm::vec4 ink = (hot ? colors::thumb : colors::fill) * (enabled ? 1.f : 0.4f);
        const float ix = x0 + (l.row ? 4.f : ToolbarLayout::pad);
        drawToolIcon(p, t, ix, yc, ink);

        if(l.labels)
        {
            float x = ix + ToolbarLayout::icon + ToolbarLayout::labelGap;
            GL_PushCanvasColor(1.f, 1.f, 1.f, enabled ? 1.f : 0.4f);
            for(const char* c = t == ToolChecklist ? checklistLabel : toolLabel(t); *c; c++, x += 8.f)
            {
                Draw_CharacterEx(x, yc - 4.f, 8.f, 8.f, hot ? *c : (*c | 128));
            }
            GL_PopCanvasColor();
        }
    }

    // The zoom buttons at the list's end: "-" and "+", small squares (the menu panel zoomed out and in: zoomMenu).
    for(int z = 0; z < 2; z++)
    {
        const bool hot = tb().zoomHovered == z;
        const float x0 = l.zoomX0(z), x1 = x0 + l.zoomSide(), yc = l.zoomYc();
        p.rounded(x0, x1, yc, l.bh(), 3.f, hot ? colors::highlightEdge : colors::boxBorder);
        p.rounded(x0 + 1.f, x1 - 1.f, yc, l.bh() - 1.f, 2.f, hot ? colors::buttonHover : colors::boxFill);
        const glm::vec4 ink = hot ? colors::thumb : colors::fill;
        const float cx = (x0 + x1) * 0.5f, arm = l.bh() * 0.5f, bar = za::fmax(0.75f, l.bh() * 0.1f);
        p.rect(cx - arm, cx + arm, yc, bar, ink);
        if(z == 1)
        {
            p.rect(cx - bar, cx + bar, yc, arm, ink);
        }
    }

    // The row's icons have no labels: the one under the mouse (or selected) names itself in a box under it.
    const int named = tb().hovered >= 0 ? tb().hovered : menuui::toolbarFocused() ? tb().focused : -1;
    if(l.row && named >= 0 && named < toolsShown())
    {
        const char* label = named == ToolChecklist ? checklistLabel : toolLabel(named);
        const float x0 = l.bx0(named);
        const float x1 = x0 + 4.f + 8.f * static_cast<float>(strlen(label)) + 4.f;
        const float yc = l.buttonsBottom() + 2.f + ToolbarLayout::rowHalf;
        p.rounded(x0, x1, yc, ToolbarLayout::rowHalf, 3.f, colors::highlightEdge);
        p.rounded(x0 + 1.f, x1 - 1.f, yc, ToolbarLayout::rowHalf - 1.f, 2.f, colors::boxFill);
        float x = x0 + 4.f;
        for(const char* c = label; *c; c++, x += 8.f)
        {
            Draw_CharacterEx(x, yc - 4.f, 8.f, 8.f, *c);
        }
    }
    tb().menu = m_state;

    // The spectator camera's switch (the headset's menus: the window's view is the headset's mirror), bottom left.
    // (The flat screen's VR layout: the rows above it, a row lower; no switch, no preview.)
    if(menuui::active())
    {
        if(menuui::inHeadset())
        {
            const BannerLayout b = bannerLayout(l);
            const bool hot = tb().bannerHovered;
            const bool on = spectatorOn();
            p.rounded(b.x0, b.x1, b.yc, ToolbarLayout::half, 3.f, hot ? colors::highlightEdge : colors::boxBorder);
            p.rounded(b.x0 + 1.f, b.x1 - 1.f, b.yc, ToolbarLayout::half - 1.f, 2.f, hot ? colors::buttonHover : colors::boxFill);
            p.disc(b.x0 + bannerLightX, b.yc, 2.5f, on ? colors::recording : colors::boxBorder); // as a camera's recording light
            float x = b.x0 + bannerTextX;
            const char* state = strchr(b.text, ':');
            for(const char* c = b.text; *c; c++, x += 8.f)
            {
                // The label in the menus' tan, On or Off white (all white under the laser).
                Draw_CharacterEx(x, b.yc - 4.f, 8.f, 8.f, hot || (state && c > state) ? *c : (*c | 128));
            }
        }
        else
        {
            // The flat screen's: lit under the desktop mouse (the headset's laser lights them: pointTo).
            const bool on = menuui::pointerOn(true);
            tb().recordingHovered = on && recordingAt(m_mousex, m_mousey);
            tb().obsHovered = on && obsBannerAt(m_mousex, m_mousey);
            tb().renderdocHovered = on && renderdocAt(m_mousex, m_mousey);
        }

        // The Recording button above it: a camera, the label as the corner's buttons'.
        const BannerLayout rb = recordingBannerLayout(l);
        const bool recHot = tb().recordingHovered;
        p.rounded(rb.x0, rb.x1, rb.yc, ToolbarLayout::half, 3.f, recHot ? colors::highlightEdge : colors::boxBorder);
        p.rounded(rb.x0 + 1.f, rb.x1 - 1.f, rb.yc, ToolbarLayout::half - 1.f, 2.f, recHot ? colors::buttonHover : colors::boxFill);
        drawRecordingIcon(p, rb.x0 + ToolbarLayout::pad, rb.yc, recHot ? colors::thumb : colors::fill);
        float rx = rb.x0 + recordingTextX;
        for(const char* c = rb.text; *c; c++, rx += 8.f)
        {
            Draw_CharacterEx(rx, rb.yc - 4.f, 8.f, 8.f, recHot ? *c : (*c | 128));
        }

        // OBS's row above it (vr_obs.cpp), while OBS answers or is found: its recording's light, red while recording
        // (paused: dimmed); the state white. The spectator camera's preview goes above them all.
        BannerLayout top = rb;
        if(const qvr::obs::Banner o = qvr::obs::banner(); o.shown)
        {
            const BannerLayout ob = obsBannerLayout(l, o);
            const bool obsHot = tb().obsHovered;
            p.rounded(ob.x0, ob.x1, ob.yc, ToolbarLayout::half, 3.f, obsHot ? colors::highlightEdge : colors::boxBorder);
            p.rounded(ob.x0 + 1.f, ob.x1 - 1.f, ob.yc, ToolbarLayout::half - 1.f, 2.f,
                obsHot ? colors::buttonHover : colors::boxFill);
            glm::vec4 light = colors::boxBorder;
            if(o.recording)
            {
                light = colors::recording;
                if(o.paused)
                {
                    light.a *= 0.4f;
                }
            }
            p.disc(ob.x0 + bannerLightX, ob.yc, 2.5f, light);
            float ox = ob.x0 + bannerTextX;
            const char* obsState = strchr(ob.text, ':');
            for(const char* c = ob.text; *c; c++, ox += 8.f)
            {
                Draw_CharacterEx(ox, ob.yc - 4.f, 8.f, 8.f, obsHot || (obsState && c > obsState) ? *c : (*c | 128));
            }
            top.yc = ob.yc; // (as wide as the Recording button still)
        }
        // The RenderDoc button above them (Developer, RenderDoc attached): a capture of the next frame.
        if(renderdocShown())
        {
            const BannerLayout db = renderdocBannerLayout(l);
            const bool dbHot = tb().renderdocHovered;
            p.rounded(db.x0, db.x1, db.yc, ToolbarLayout::half, 3.f, dbHot ? colors::highlightEdge : colors::boxBorder);
            p.rounded(db.x0 + 1.f, db.x1 - 1.f, db.yc, ToolbarLayout::half - 1.f, 2.f,
                dbHot ? colors::buttonHover : colors::boxFill);
            p.disc(db.x0 + bannerLightX, db.yc, 2.5f, dbHot ? colors::thumb : colors::fill);
            float dx = db.x0 + renderdocTextX;
            for(const char* c = db.text; *c; c++, dx += 8.f)
            {
                Draw_CharacterEx(dx, db.yc - 4.f, 8.f, 8.f, dbHot ? *c : (*c | 128));
            }
            top.yc = db.yc;
        }
        if(menu::currentInstance() == 0 && menuui::inHeadset())
        {
            placePreview(l, top); // (on the first panel only)
        }
    }
    else
    {
        // A flat screen's Recording button, bottom left, where the menu leaves room: lit under the mouse.
        placeFlatRecording(l);
        const FlatRecording& f = flatRecording;
        tb().recordingHovered = flatRecordingShown() && recordingAt(m_mousex, m_mousey) && menuui::pointerOn(true);
        if(!f.leftOut)
        {
            const bool hot = tb().recordingHovered;
            const float half = (f.y1 - f.y0) * l.k * 0.5f;
            const float yc = (f.y0 + f.y1) * 0.5f;
            p.rounded(f.x0, f.x1, yc, half, 3.f, hot ? colors::highlightEdge : colors::boxBorder);
            p.rounded(f.x0 + 1.f, f.x1 - 1.f, yc, half - 1.f, 2.f, hot ? colors::buttonHover : colors::boxFill);
            float x = f.x0 + FlatRecording::pad;
            for(const char* c = FlatRecording::text; *c; c++, x += FlatRecording::size)
            {
                Draw_CharacterEx(x, yc - FlatRecording::size * 0.5f, FlatRecording::size, FlatRecording::size,
                    hot ? *c : (*c | 128));
            }
        }
    }

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
    // On a flat screen (VR off), in either layout (Quake's own or vr_menu_flat_vr_style's panel), Escape is Back to game
    // (always): the menu closes from whatever page and resumes, as the menu button does in a headset, and opens again
    // on that page. Not without a game (Quake's main menu then: the demos), nor while Escape is the
    // page's own (a drop-down open, a slider held) or a key binding waits.
    if(key == K_ESCAPE && !repeat && !vrActive() && !M_WaitingForKeyBinding() &&
        key_dest == key_menu && m_state != m_none && cls.state == ca_connected && !menu::escapeLocal())
    {
        tb().focused = -1;
        menuui::uiSound(menuui::UiSound::Move);
        menuui::backToGame(HAND_MAIN);
        return 1;
    }
    // The flat screen's VR layout: a press on a corner of its panel takes hold of it (it follows the mouse while held:
    // VR_MenuFlatMouse); the wheel while held resizes it round the mouse.
    if(FlatPanel& f = flatPanelState; menuui::flatPanel() && !M_WaitingForKeyBinding() && ui_mouse.value)
    {
        if(key == K_MOUSE1 && !repeat && (f.corner = flatCornerAt(f.mouse)) >= 0)
        {
            f.dragging = true;
            f.grab = f.mouse - flatCentre();
            showFlatCursor(true);
            return 1;
        }
        if(f.dragging && (key == K_MWHEELUP || key == K_MWHEELDOWN))
        {
            if(zoomFlatAround(f.mouse, key == K_MWHEELUP ? flatZoomStep : 1.f / flatZoomStep))
            {
                f.wheelZooms++;
            }
            f.grab = f.mouse - flatCentre();
            return 1;
        }
    }
    if(key == K_MOUSE1 && !repeat && !M_WaitingForKeyBinding() && menuui::versionLinkPress())
    {
        tb().focused = -1;
        haptics::pulse(pointingHand, 0.02f, 0.3f, 200.f);
        return 1;
    }
    if(!menuui::toolbarShown() || M_WaitingForKeyBinding())
    {
        tb().focused = -1;
        return 0;
    }

    if(key == K_MOUSE1 && !repeat && recordingAt(m_mousex, m_mousey) && menuui::pointerOn(false))
    {
        tb().focused = -1;
        haptics::pulse(pointingHand, 0.02f, 0.3f, 200.f);
        menu::jumpToRecording();
        return 1;
    }
    if(key == K_MOUSE1 && !repeat && renderdocAt(m_mousex, m_mousey))
    {
        tb().focused = -1;
        if(qvr::renderdoc::captureFromCvars())
        {
            haptics::pulse(pointingHand, 0.02f, 0.3f, 200.f);
        }
        return 1;
    }
    if(key == K_MOUSE1 && !repeat && obsBannerAt(m_mousex, m_mousey))
    {
        tb().focused = -1;
        if(qvr::obs::press())
        {
            haptics::pulse(pointingHand, 0.02f, 0.3f, 200.f);
        }
        return 1;
    }
    if(key == K_MOUSE1 && bannerAt(m_mousex, m_mousey))
    {
        tb().focused = -1;
        toggleSpectator(pointingHand);
        return 1;
    }
    if(const int z = zoomAt(m_mousex, m_mousey); key == K_MOUSE1 && !repeat && z >= 0 && menuui::pointerOn(false))
    {
        tb().focused = -1;
        pressZoom(z, pointingHand);
        return 1;
    }
    if(key == K_MOUSE1)
    {
        const int t = toolAt(m_mousex, m_mousey);
        if(t < 0)
        {
            tb().focused = -1;
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
            tb().focused = -1;
        }
        else
        {
            focusTool(ToolBack);
        }
        qvr::menuui::uiSound(qvr::menuui::UiSound::Move);
        return 1;
    }

    if(!menuui::toolbarFocused())
    {
        tb().focused = -1;
        return 0;
    }

    if(toolbarLayout().row)
    {
        // The row: left and right along it (stopping at its ends), down back to the menu (a VR page: its first setting).
        switch(key)
        {
            case K_LEFTARROW:
            case K_RIGHTARROW:
            {
                const int step = key == K_RIGHTARROW ? 1 : -1;
                int next = tb().focused + step;
                next += toolHidden(next) ? step : 0; // (over New Panel, not on this row)
                if(next >= 0 && next < toolsShown())
                {
                    tb().focused = next;
                    qvr::menuui::uiSound(qvr::menuui::UiSound::Move);
                }
                return 1;
            }
            case K_DOWNARROW:
                tb().focused = -1;
                if(m_state == m_vr)
                {
                    menu::selectEnd(1);
                }
                qvr::menuui::uiSound(qvr::menuui::UiSound::Move);
                return 1;
            case K_UPARROW: return 1; // (nothing above it)
            default: break;
        }
    }

    switch(key)
    {
        case K_UPARROW:
        case K_DOWNARROW:
        {
            const int dir = key == K_DOWNARROW ? 1 : -1;
            const int next = tb().focused + dir;
            if(next >= 0 && next < toolsShown())
            {
                tb().focused = next;
            }
            else if(repeat)
            {
                return 1; // a held stick stops at the column's end: a new push leaves it
            }
            else
            {
                tb().focused = -1;
                if(m_state == m_vr)
                {
                    menu::selectEnd(dir); // on round the page: its first setting below, its last above
                }
            }
            qvr::menuui::uiSound(qvr::menuui::UiSound::Move);
            return 1;
        }
        case K_ENTER:
        case K_KP_ENTER:
        case K_ABUTTON: menuui::useTool(tb().focused, HAND_MAIN); return 1;
        case K_ESCAPE:
        case K_BBUTTON:
        case K_MOUSE2:
        case K_MOUSE4:
            tb().focused = -1;
            qvr::menuui::uiSound(qvr::menuui::UiSound::Move);
            return 1;
        case K_LEFTARROW:
        case K_RIGHTARROW: return 1; // nothing to change here (and nothing under it changed)
        default:
            tb().focused = -1; // another key: the menu's again
            return 0;
    }
}

// One of Ironwail's menus opened as its own way in opens it (the menu's cursor as it was left): "Back to game"'s menu
// reopened, Back from a VR page to the menu it was opened from (vr_menu.cpp, NavStack). m_none: the menu closed.
void qvr::menuui::openMenu(int state)
{
    switch(state)
    {
        case m_none:
            IN_Activate();
            key_dest = key_game;
            m_state = m_none;
            break;
        case m_main: M_Menu_Main_f(); break;
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
        case m_gamepad: M_Options_Init(static_cast<m_state_e>(state)); break;
        case m_keys: M_Menu_Keys_f(); break;
        case m_mods: M_Menu_Mods_f(); break;
        case m_help: M_Menu_Help_f(); break;
        case m_autosaves:
            Cmd_TokenizeString("menu_autosaves"); // it looks at its command's arguments
            M_Menu_Autosaves_f();
            break;
        default: M_Menu_Main_f(); break;
    }
}

// Opening the menu: the page "Back to game" left, over the main menu (where pages that go back
// where they came from lead, and what is left when a page cannot open now: saving outside a single
// player game). Once: the next time, the main menu again unless it closed that way again.
extern "C" int VR_MenuMouseOnButtons(float x, float y)
{
    return (menuui::toolbarShown() && toolAt(x, y) >= 0) || menuui::versionLinkAt(x, y);
}

extern "C" int VR_MenuReopen()
{
    const Remembered r = remembered;
    remembered = {};
    if(!vr_menu_remember.value || r.state == m_none) // (flat screen too: vr_menu_flat_shortcuts' Back to game)
    {
        return 0;
    }

    M_Menu_Main_f();
    if(r.state == m_vr)
    {
        menu::reopen(r.vrPage);
    }
    else if(r.state != m_main)
    {
        menuui::openMenu(r.state);
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
    if(ragtool::active() && key_dest == key_menu && sv.active && svs.maxclients == 1 && !cl.intermission)
    {
        return 1; // (the ragdoll tool: its subject moves on behind its page)
    }
    if(!ui_live_preview.value || !vrActive() || key_dest != key_menu || !sv.active || svs.maxclients != 1 ||
        cl.intermission)
    {
        return 0;
    }
    switch(menu::mainState()) // (the first panel's menu: the others show VR pages only)
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
