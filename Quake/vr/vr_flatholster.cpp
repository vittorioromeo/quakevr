// vr_flatholster.cpp -- see vr_flatholster.hpp.

#include "vr_flatholster.hpp"

#include "vr_api.h"
#include "vr_backend.hpp" // HAND_OFF / HAND_MAIN
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_flatplay.hpp"
#include "vr_gadget.hpp"
#include "vr_gfx.hpp"
#include "vr_notectx.hpp"
#include "vr_protocol.hpp"
#include "vr_testkit.hpp"
#include "vr_timescale.hpp"
#include "vr_toolgun.hpp"
#include "vr_weapon_ids.h"

#include "Zancle/Base/Exchange.hpp"
#include "Zancle/Base/GetArraySize.hpp"
#include "Zancle/Container/Array.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Round.hpp"
#include "vr_zancle.hpp"

#include <stdio.h>

extern "C" qboolean keydown[MAX_KEYS];             // keys.c
extern "C" int Key_StringToKeynum(const char* str); // keys.c
extern "C" kbutton_t in_attack;                     // cl_input.c: +attack (the left mouse button)
extern "C" int VR_FlatHudClip(int left, int* clip, int* clipSize); // vr_flathud.cpp

namespace qvr::flatholster
{

namespace
{

// The menu's virtual screen (its image's pixels: vr_gadget_holster.inc), and where each thing on it is.
constexpr float panelW = 480.f;
constexpr float panelH = 248.f;
constexpr int holsterCount = 6;

// The holsters as the stats have them (STAT_QVR_HOLSTERWEAPON0 + i: QC holsterweapon i, vr_flatholster.qc), each its
// place on the figure: the shoulders high, the chest's in the middle, the hips low; the player's left on the left.
struct HolsterPlace
{
    const char* title;
    float x, y;           // the slot's corner
    float anchorX, anchorY; // where its stroke meets the figure
};
constexpr HolsterPlace places[holsterCount] = {
    {"L SHOULDER", 8.f, 18.f, 106.f, 40.f}, {"R SHOULDER", 172.f, 18.f, 150.f, 40.f},
    {"LEFT HIP", 8.f, 162.f, 116.f, 118.f}, {"RIGHT HIP", 172.f, 162.f, 140.f, 118.f},
    {"LEFT CHEST", 8.f, 90.f, 121.f, 70.f}, {"RIGHT CHEST", 172.f, 90.f, 135.f, 70.f}};
constexpr float slotW = 76.f;
constexpr float slotH = 56.f;
constexpr float buttonH = 12.f;

// The figure: head, shoulders, spine, hips, arms, legs.
constexpr glm::vec4 figure[] = {{121.f, 20.f, 135.f, 20.f}, {135.f, 20.f, 135.f, 34.f}, {135.f, 34.f, 121.f, 34.f},
    {121.f, 34.f, 121.f, 20.f}, {104.f, 40.f, 152.f, 40.f}, {128.f, 34.f, 128.f, 118.f}, {112.f, 118.f, 144.f, 118.f},
    {104.f, 40.f, 96.f, 100.f}, {152.f, 40.f, 160.f, 100.f}, {116.f, 118.f, 110.f, 200.f}, {140.f, 118.f, 146.f, 200.f}};

constexpr glm::vec4 handRect[2] = {{262.f, 40.f, 88.f, 100.f}, {384.f, 40.f, 88.f, 100.f}}; // the left hand's, the right's
constexpr glm::vec4 swapRect{354.f, 76.f, 26.f, 26.f};
// DROP BOTH: under the hands' DROPs, in the middle between them (his words 2026-11-10: "add a 'drop both' button under
// the hands at the center").
constexpr glm::vec4 dropBothRect{317.f, 162.f, 100.f, 14.f};
// UNLOAD under each hand, below DROP BOTH (his words 2026-11-10: "add a per-hand 'unload button' that empties a gun's
// magazine (if applicable) and returns the ammunition to the ammo storage").
constexpr float unloadY = 180.f;

enum class Act : int
{
    HolsterLeft,  // holster `index` <-> the left hand (impulse 60 + 3i)
    HolsterDrop,  // holster `index` to the ground (61 + 3i)
    HolsterRight, // holster `index` <-> the right hand (62 + 3i)
    HandDrop,     // hand `index` (0 the left, 1 the right) drops (78, 79)
    DropBoth,     // both hands drop, the off hand first (81)
    Swap,         // the hands swap (48: 3's)
    Unload        // hand `index`'s gun's live rounds back into the reserve (82, 83)
};
struct Button
{
    Act act;
    int index;
    glm::vec4 rect;
    char name[16];
};
constexpr int buttonCount = holsterCount * 3 + 6;

struct State
{
    bool open{false};      // open this frame (vr_flat_holster_menu_mode 0: the key held; 1: toggled on)
    float fade{0.f};       // 0 closed .. 1 open
    double lastTime{-1.0}; // realtime at the last frame
    bool escLatch{false};  // Esc closed it: shut until the key is let go (mode 0)
    bool cmdHeld{false};   // +flatholsters
    bool keyWas{false};    // the key (or +flatholsters) down, last frame
    bool toggled{false};   // mode 1: a press opened it, the next one closes it
    bool lmbClicked{false}; // mode 0: the left button clicked during this hold (then letting go only closes)
    bool lmb{false};       // the left button, last frame
    glm::vec2 cursor{panelW * 0.5f, panelH * 0.5f};
    float scale{2.f};      // window pixels a panel pixel, as last drawn
    glm::vec4 screen{0.f}; // the panel on the window, as last drawn (x, y, w, h; y down)
    int clicks{0};
    za::Array<Button, buttonCount> buttons{};
    bool laidOut{false};
};
State st;

// What the menu shows (the server's stats) this frame.
struct Contents
{
    int holster[holsterCount]{};
    int holsterModel[holsterCount]{};
    int hand[2]{};      // weapon ids, the left (off) and the right (main)
    int handModel[2]{}; // model indices
    int carry[2]{};     // the entity each carries (a box, a prop), 0 none
    int clip[2]{};      // the live rounds in each hand's gun's clip (the flat HUD's clip: VR_FlatHudClip), 0 none or no clip
};

[[nodiscard]] Contents contents()
{
    Contents c;
    for(int i = 0; i < holsterCount; i++)
    {
        c.holster[i] = cl.stats[protocol::STAT_QVR_HOLSTERWEAPON0 + i];
        c.holsterModel[i] = cl.stats[protocol::STAT_QVR_HOLSTERWEAPONMODEL0 + i];
    }
    c.hand[0] = cl.stats[protocol::STAT_QVR_WEAPON2];
    c.hand[1] = cl.stats[protocol::STAT_QVR_WEAPON];
    c.handModel[0] = cl.stats[protocol::STAT_QVR_WEAPONMODEL2];
    c.handModel[1] = cl.stats[STAT_WEAPON];
    c.carry[0] = cl.stats[protocol::STAT_QVR_CARRYOFF];
    c.carry[1] = cl.stats[protocol::STAT_QVR_CARRYMAIN];
    for(int h = 0; h < 2; h++)
    {
        int clip = 0, size = 0;
        // (The HUD's rule for a gun with a clip, not one whose clip is its only ammo: VR_FlatHudClip's `left` 0 the main hand.)
        if(c.hand[h] != WID_FIST && VR_FlatHudClip(h == 0 ? 1 : 0, &clip, &size))
        {
            c.clip[h] = clip;
        }
    }
    return c;
}

[[nodiscard]] const char* modelName(int index)
{
    return index > 0 && index < MAX_MODELS && cl.model_precache[index] ? cl.model_precache[index]->name : "";
}

void layOut()
{
    if(st.laidOut)
    {
        return;
    }
    st.laidOut = true;
    int n = 0;
    const auto add = [&n](Act act, int index, const glm::vec4& r, const char* name) {
        Button& b = st.buttons[n++];
        b.act = act;
        b.index = index;
        b.rect = r;
        q_strlcpy(b.name, name, sizeof(b.name));
    };
    char name[16];
    for(int i = 0; i < holsterCount; i++)
    {
        const float x = places[i].x, y = places[i].y + slotH + 1.f;
        q_snprintf(name, sizeof(name), "h%d l", i);
        add(Act::HolsterLeft, i, {x, y, 22.f, buttonH}, name);
        q_snprintf(name, sizeof(name), "h%d drop", i);
        add(Act::HolsterDrop, i, {x + 24.f, y, 28.f, buttonH}, name);
        q_snprintf(name, sizeof(name), "h%d r", i);
        add(Act::HolsterRight, i, {x + 54.f, y, 22.f, buttonH}, name);
    }
    add(Act::HandDrop, 0, {handRect[0].x, handRect[0].y + handRect[0].w + 4.f, handRect[0].z, 14.f}, "left drop");
    add(Act::HandDrop, 1, {handRect[1].x, handRect[1].y + handRect[1].w + 4.f, handRect[1].z, 14.f}, "right drop");
    add(Act::DropBoth, 0, dropBothRect, "both drop");
    add(Act::Swap, 0, swapRect, "swap");
    add(Act::Unload, 0, {handRect[0].x, unloadY, handRect[0].z, 14.f}, "left unload");
    add(Act::Unload, 1, {handRect[1].x, unloadY, handRect[1].z, 14.f}, "right unload");
}

[[nodiscard]] bool inside(const glm::vec4& r, const glm::vec2& p)
{
    return p.x >= r.x && p.y >= r.y && p.x < r.x + r.z && p.y < r.y + r.w;
}

// Whether the button does something now (else drawn dim, and a click on it does nothing).
[[nodiscard]] bool enabled(const Button& b, const Contents& c)
{
    const auto handHas = [&c](int h) { return c.hand[h] != WID_FIST || c.carry[h] != 0; };
    switch(b.act)
    {
    case Act::HolsterLeft: return (c.holster[b.index] != WID_FIST || c.hand[0] != WID_FIST) && !c.carry[0];
    case Act::HolsterRight: return (c.holster[b.index] != WID_FIST || c.hand[1] != WID_FIST) && !c.carry[1];
    case Act::HolsterDrop: return c.holster[b.index] != WID_FIST;
    case Act::HandDrop: return handHas(b.index);
    case Act::DropBoth: return handHas(0) || handHas(1);
    case Act::Swap: return (c.hand[0] != WID_FIST || c.hand[1] != WID_FIST) && !c.carry[0] && !c.carry[1];
    case Act::Unload: return c.clip[b.index] > 0;
    }
    return false;
}

[[nodiscard]] int impulseOf(const Button& b)
{
    switch(b.act)
    {
    case Act::HolsterLeft: return 60 + 3 * b.index;
    case Act::HolsterDrop: return 61 + 3 * b.index;
    case Act::HolsterRight: return 62 + 3 * b.index;
    case Act::HandDrop: return b.index == 0 ? 78 : 79;
    case Act::DropBoth: return 81;
    case Act::Swap: return 48;
    case Act::Unload: return b.index == 0 ? 82 : 83;
    }
    return 0;
}

[[nodiscard]] const Button* buttonAt(const glm::vec2& p)
{
    layOut();
    for(const Button& b : st.buttons)
    {
        if(inside(b.rect, p))
        {
            return &b;
        }
    }
    return nullptr;
}

// A click where the cursor is: the button's impulse (QC vr_flatholster.qc), as the console's `impulse N` would send it.
void click()
{
    const Button* b = buttonAt(st.cursor);
    const Contents c = contents();
    if(!b || !enabled(*b, c))
    {
        if(vr_flatplay_debug.value)
        {
            Con_Printf("flat holsters: click at %.0f %.0f: %s\n", st.cursor.x, st.cursor.y, b ? "that button does nothing now" : "no button");
        }
        return;
    }
    if(b->act == Act::Swap)
    {
        flatplay::swapHands("the holster menu's SWAP"); // (3's own: the hands meet first, vr_flatswap.cpp)
    }
    else
    {
        char cmd[32];
        q_snprintf(cmd, sizeof(cmd), "impulse %d\n", impulseOf(*b));
        Cbuf_InsertText(cmd);
    }
    st.clicks++;
    Con_DPrintf("flat holsters: %s (impulse %d)\n", b->name, impulseOf(*b));
}

[[nodiscard]] bool keyHeld()
{
    const char* k = vr_flat_key_holster_menu.string;
    const int key = k[0] ? Key_StringToKeynum(k) : -1;
    return key > 0 && key < MAX_KEYS && keydown[key] != 0;
}

// vr_flat_holster_menu_mode 1: a press opens it, clicks, the next press closes it; 0: held, let go on a button.
[[nodiscard]] bool toggleMode()
{
    return vr_flat_holster_menu_mode.value != 0.f;
}

// vr_flatholster_test's run (below), stepped by frame().
void testStep();

} // namespace

void frame(bool on)
{
    const double now = realtime;
    const float dt = st.lastTime >= 0.0 ? za::clamp(static_cast<float>(now - st.lastTime), 0.f, 0.1f) : 0.f;
    st.lastTime = now;
    const bool held = keyHeld() || st.cmdHeld;
    const bool pressed = held && !st.keyWas, letGo = !held && st.keyWas;
    st.keyWas = held;
    if(!held)
    {
        st.escLatch = false;
    }
    const bool can = on && vr_flat_holsters.value != 0.f && key_dest == key_game && cls.signon == SIGNONS &&
                     cl.stats[STAT_HEALTH] > 0 && !cl.intermission && toolgun::heldHand() != HAND_MAIN;
    // vr_flat_holster_menu_mode (his words 2026-11-10): 0 open while held, and letting go clicks the button under the
    // cursor ("so that I can quickly hover what I want and release the mouse wheel"; over nothing, or after a left click
    // in this hold, it only closes); 1 a press opens it, the left button clicks as often as wanted, the next press closes.
    const bool toggle = toggleMode();
    if(!toggle || !can)
    {
        st.toggled = false;
    }
    else if(pressed)
    {
        st.toggled = !st.toggled;
    }
    if(!toggle && letGo && st.open && !st.lmbClicked)
    {
        click();
    }
    const bool want = can && (toggle ? st.toggled : held && !st.escLatch);
    if(want && !st.open)
    {
        st.cursor = {panelW * 0.5f, panelH * 0.5f}; // (each time in the middle: between the holsters and the hands)
        st.lmbClicked = false;
        Con_DPrintf("flat holsters: the menu opens\n");
    }
    else if(!want && st.open)
    {
        Con_DPrintf("flat holsters: the menu closes\n");
    }
    st.open = want;
    const float fadeTime = za::clamp(vr_flat_holster_menu_fade_time.value, 0.f, 1.f);
    const float step = fadeTime > 0.f ? dt / fadeTime : 1.f;
    st.fade = za::clamp(st.fade + (st.open ? step : -step), 0.f, 1.f);
    if(!on)
    {
        st.fade = 0.f;
    }

    // The left button: a click on the menu (flatplay::routeFire fires nothing while it is open).
    const bool lmb = (in_attack.state & 1) != 0;
    if(st.open && lmb && !st.lmb)
    {
        click();
        st.lmbClicked = true;
    }
    st.lmb = lmb;
    testStep();
}

bool shown()
{
    return st.open || st.fade > 0.f;
}

bool open()
{
    return st.open;
}

float timeScale()
{
    return st.open ? za::clamp(vr_flat_holster_menu_timescale.value, 0.05f, 1.f) : 1.f;
}

float crosshairAlpha()
{
    return 1.f - st.fade * za::clamp(vr_flat_holster_menu_xhair_fade.value, 0.f, 1.f);
}

float blurRadius()
{
    return st.fade * za::clamp(vr_flat_holster_menu_blur.value, 0.f, 40.f);
}

bool mouse(int dx, int dy)
{
    if(!st.open)
    {
        return false;
    }
    // vr_flat_holster_menu_cursor_speed 1: the cursor keeps up with the desktop's pointer (a count a window pixel).
    const float k = za::max(vr_flat_holster_menu_cursor_speed.value, 0.f) / za::max(st.scale, 0.1f);
    st.cursor.x = za::clamp(st.cursor.x + static_cast<float>(dx) * k, 0.f, panelW - 1.f);
    st.cursor.y = za::clamp(st.cursor.y + static_cast<float>(dy) * k, 0.f, panelH - 1.f);
    return true;
}

bool escape()
{
    if(!st.open)
    {
        return false;
    }
    st.open = false;
    st.escLatch = true;
    st.toggled = false;
    Con_DPrintf("flat holsters: the menu closes (Esc)\n");
    return true;
}

bool countsTo(const char* name, int& dx, int& dy)
{
    layOut();
    for(const Button& b : st.buttons)
    {
        if(!q_strcasecmp(b.name, name))
        {
            const float k = za::max(st.scale, 0.1f) / za::max(vr_flat_holster_menu_cursor_speed.value, 0.01f);
            dx = static_cast<int>(za::round((b.rect.x + b.rect.z * 0.5f - st.cursor.x) * k));
            dy = static_cast<int>(za::round((b.rect.y + b.rect.w * 0.5f - st.cursor.y) * k));
            return true;
        }
    }
    return false;
}

void draw()
{
    if(!shown() || !flatplay::active() || vid.width <= 0 || vid.height <= 0)
    {
        return;
    }
    layOut();
    const Contents c = contents();

    // The image: the slots, the buttons (lit under the cursor), the figure.
    za::Array<gadget::HolsterMenuSlot, holsterCount + 2> slots{};
    za::Array<gadget::HolsterMenuButton, buttonCount> buttons{};
    za::Array<glm::vec4, za::getArraySize(figure) + holsterCount + 1> lines{};
    int nl = 0;
    for(const glm::vec4& l : figure)
    {
        lines[nl++] = l;
    }
    for(int i = 0; i < holsterCount; i++)
    {
        const HolsterPlace& p = places[i];
        const bool left = p.x < 128.f;
        lines[nl++] = {left ? p.x + slotW : p.x, p.y + slotH * 0.5f, p.anchorX, p.anchorY};
        gadget::HolsterMenuSlot& s = slots[i];
        s.rect = {p.x, p.y, slotW, slotH};
        s.title = p.title;
        s.name = c.holster[i] != WID_FIST ? notectx::weaponName(c.holster[i]) : "";
        s.model = c.holster[i] != WID_FIST ? modelName(c.holsterModel[i]) : "";
        s.hovered = inside(s.rect, st.cursor);
    }
    lines[nl++] = {252.f, 20.f, 252.f, panelH - 16.f}; // (the holsters' half, the hands')
    for(int h = 0; h < 2; h++)
    {
        gadget::HolsterMenuSlot& s = slots[holsterCount + h];
        s.rect = handRect[h];
        s.title = h == 0 ? "LEFT HAND" : "RIGHT HAND";
        if(c.hand[h] != WID_FIST)
        {
            s.name = notectx::weaponName(c.hand[h]);
            s.model = modelName(c.handModel[h]);
        }
        else if(c.carry[h] > 0 && c.carry[h] < cl.num_entities)
        {
            const qmodel_t* m = cl_entities[c.carry[h]].model;
            s.name = "carried";
            s.model = m && m->type == mod_alias ? m->name : "";
        }
        s.hovered = inside(s.rect, st.cursor);
    }
    for(int i = 0; i < buttonCount; i++)
    {
        const Button& b = st.buttons[i];
        gadget::HolsterMenuButton& g = buttons[i];
        g.rect = b.rect;
        g.enabled = enabled(b, c);
        g.hovered = inside(b.rect, st.cursor);
        switch(b.act)
        {
        case Act::HolsterLeft: g.icon = gadget::MenuIcon::HandLeft; g.label = "L"; break;
        case Act::HolsterRight: g.icon = gadget::MenuIcon::HandRight; g.label = "R"; break;
        case Act::HolsterDrop: g.icon = gadget::MenuIcon::Drop; break;
        case Act::HandDrop: g.icon = gadget::MenuIcon::Drop; g.label = "DROP"; break;
        case Act::DropBoth: g.icon = gadget::MenuIcon::Drop; g.label = "DROP BOTH"; break;
        case Act::Swap: g.icon = gadget::MenuIcon::Swap; break;
        case Act::Unload: g.icon = gadget::MenuIcon::None; g.label = "UNLOAD"; break;
        }
    }
    gadget::HolsterMenuView v;
    v.size = {panelW, panelH};
    v.slots = slots.data();
    v.slotCount = holsterCount + 2;
    v.buttons = buttons.data();
    v.buttonCount = buttonCount;
    v.lines = lines.data();
    v.lineCount = nl;
    v.header = "HOLSTERS";
    v.footer = toggleMode() ? "Click   wheel / Esc: close   L/R: trade with that hand   DROP: to the ground"
                            : "Point, let go of the wheel   L/R: trade with that hand   DROP: to the ground";
    const gadget::VisorImage img = gadget::renderHolsterMenu(v);
    if(!img.shown)
    {
        return;
    }

    // On the window: vr_flat_holster_menu_size of its height (narrowed to fit its width), in its middle.
    const float W = static_cast<float>(vid.width), H = static_cast<float>(vid.height);
    float h = za::clamp(vr_flat_holster_menu_size.value, 0.2f, 1.f) * H;
    float w = h * panelW / panelH;
    if(w > W * 0.96f)
    {
        w = W * 0.96f;
        h = w * panelH / panelW;
    }
    st.scale = h / panelH;
    const float x0 = (W - w) * 0.5f, y0 = (H - h) * 0.5f;
    st.screen = {x0, y0, w, h};
    const float a = st.fade;
    const auto ndc = [W, H](float x, float y) { return glm::vec3{x / W * 2.f - 1.f, 1.f - y / H * 2.f, 0.f}; };
    const auto quad = [&](float qx, float qy, float qw, float qh, const glm::vec4& col, gfx::Blend blend) {
        const gfx::Vertex q[6] = {{ndc(qx, qy + qh), {0.f, 0.f}, col}, {ndc(qx + qw, qy + qh), {1.f, 0.f}, col},
            {ndc(qx + qw, qy), {1.f, 1.f}, col}, {ndc(qx, qy + qh), {0.f, 0.f}, col}, {ndc(qx + qw, qy), {1.f, 1.f}, col},
            {ndc(qx, qy), {0.f, 1.f}, col}};
        gfx::draw(q, glm::mat4{1.f}, {.shade = gfx::Shade::Color, .blend = blend, .depthTest = false, .depthWrite = false});
    };
    const gadget::VisorColors colors = gadget::visorColors();
    const float px = H / 1080.f;
    // The mount's glow round it (the visor's), then its face and the screen on it, then its frame.
    for(int r = 4; r >= 1; r--)
    {
        const float o = static_cast<float>(r) * 3.f * px;
        const glm::vec4 g{colors.line, 0.05f * a * static_cast<float>(5 - r)};
        quad(x0 - o, y0 - o, w + 2.f * o, o, g, gfx::Blend::Additive);
        quad(x0 - o, y0 + h, w + 2.f * o, o, g, gfx::Blend::Additive);
        quad(x0 - o, y0, o, h, g, gfx::Blend::Additive);
        quad(x0 + w, y0, o, h, g, gfx::Blend::Additive);
    }
    quad(x0, y0, w, h, {colors.face, 0.86f * a}, gfx::Blend::Alpha);
    {
        const glm::vec4 white{gadget::visorPhosphor().x, gadget::visorPhosphor().y, gadget::visorPhosphor().z, 0.f};
        const gfx::Vertex c4[4] = {{ndc(x0, y0 + h), {0.f, 0.f}, white}, {ndc(x0 + w, y0 + h), {1.f, 0.f}, white},
            {ndc(x0 + w, y0), {1.f, 1.f}, white}, {ndc(x0, y0), {0.f, 1.f}, white}};
        const gfx::Vertex q[6] = {c4[0], c4[1], c4[2], c4[0], c4[2], c4[3]};
        gfx::draw(q, glm::mat4{1.f},
            {.shade = gfx::Shade::Screen, .blend = gfx::Blend::Premultiplied, .depthTest = false, .depthWrite = false,
                .params = {static_cast<float>(realtime), za::clamp(vr_flat_holster_menu_crt.value, 0.f, 2.f), 0.f, gadget::textGlow()},
                .screen = {panelW, panelH, 0.5f}, .trueColor = true, .inkAlpha = a},
            img.texture);
    }
    const float t = za::max(1.f, 2.f * px);
    const glm::vec4 line{colors.line, a};
    quad(x0, y0, w, t, line, gfx::Blend::Alpha);
    quad(x0, y0 + h - t, w, t, line, gfx::Blend::Alpha);
    quad(x0, y0, t, h, line, gfx::Blend::Alpha);
    quad(x0 + w - t, y0, t, h, line, gfx::Blend::Alpha);

    // The cursor: an arrow, its tip where it points, dark round its light.
    if(st.open)
    {
        const float cx = x0 + st.cursor.x * st.scale, cy = y0 + st.cursor.y * st.scale, s = 22.f * px;
        const auto arrow = [&](float grow, const glm::vec4& col) {
            const gfx::Vertex tri[3] = {{ndc(cx - grow * 0.5f, cy - grow), {0.f, 0.f}, col},
                {ndc(cx - grow * 0.5f, cy + s + grow), {0.f, 0.f}, col}, {ndc(cx + s * 0.7f + grow, cy + s * 0.68f + grow * 0.5f), {0.f, 0.f}, col}};
            gfx::draw(tri, glm::mat4{1.f}, {.shade = gfx::Shade::Color, .blend = gfx::Blend::Alpha, .depthTest = false, .depthWrite = false});
        };
        arrow(2.5f * px, {0.f, 0.f, 0.f, 0.85f * a});
        arrow(0.f, {glm::min(colors.value * 1.4f, glm::vec3{1.f}), a});
    }
}

namespace
{

// vr_flatholster_test (Misc/quakevr/flatholster_test.sh; flat-screen play with vr_flat_holsters 1 and the script's own
// weapons: the shotgun in the right hand, the nailgun in the left): the menu held open (+flatholsters) and driven by the
// mouse (the cursor moved onto each button by its counts, vr_flat_mouse_move's, and the left button's click there):
// slow motion at vr_flat_holster_menu_timescale; every holster emptied by its DROP; L and R trading the hands' guns
// with a holster; SWAP; the weapon keys doing nothing; a hand's DROP; the menu let go and time back at 1. Each step
// waits on the stats it changes (at most `giveUp` frames: a FAIL "timed out"). Ends "flatholster_test: N passed, M
// failed".
struct Test
{
    int step{-1}; // -1 not running
    int frames{0};
    testkit::Tally tally;
    int holster{0};
    int clicks{0};     // st.clicks before a step
    float mode{0.f};   // vr_flat_holster_menu_mode before the test (put back at its end)
    int shells{0};     // the shells before the right hand's UNLOAD
    int clip{0};       // ... and the shotgun's clip
};
Test test;
constexpr int giveUp = 400;

void result(bool ok, const char* what)
{
    if(test.tally.check(ok, "flatholster_test", what))
    {
        Con_Printf("flatholster_test: PASS %s\n", what);
    }
}

// The cursor brought onto `name` by the mouse's counts, then the left button's click there (as a real click: click()).
void press(const char* name)
{
    int dx = 0, dy = 0;
    if(!countsTo(name, dx, dy))
    {
        result(false, name);
        return;
    }
    (void)mouse(dx, dy);
    click();
}

// The cursor brought onto panel point `p` by the mouse's counts (no click).
void pointAt(const glm::vec2& p)
{
    const float k = za::max(st.scale, 0.1f) / za::max(vr_flat_holster_menu_cursor_speed.value, 0.01f);
    (void)mouse(static_cast<int>(za::round((p.x - st.cursor.x) * k)), static_cast<int>(za::round((p.y - st.cursor.y) * k)));
}

void next()
{
    test.step++;
    test.frames = 0;
}

void testStep()
{
    if(test.step < 0)
    {
        return;
    }
    test.frames++;
    const Contents c = contents();
    const auto wait = [](bool done, const char* what) {
        if(done)
        {
            return true;
        }
        if(test.frames > giveUp)
        {
            char line[96];
            q_snprintf(line, sizeof(line), "%s (timed out)", what);
            result(false, line);
            next();
        }
        return false;
    };
    switch(test.step)
    {
    case 0: // open
        st.cmdHeld = true;
        if(wait(st.open && st.fade >= 1.f && timescale::current() <= timeScale() + 0.001f, "the menu open, time slowed"))
        {
            char line[96];
            q_snprintf(line, sizeof(line), "open, time at %.2f (vr_flat_holster_menu_timescale %.2f)", timescale::current(),
                vr_flat_holster_menu_timescale.value);
            result(true, line);
            next();
        }
        break;
    case 1: // every holster emptied, one DROP at a time
        if(test.frames == 1)
        {
            while(test.holster < holsterCount && c.holster[test.holster] == WID_FIST)
            {
                test.holster++;
            }
            if(test.holster >= holsterCount)
            {
                result(true, "every holster empty (DROP)");
                next();
                break;
            }
            char name[16];
            q_snprintf(name, sizeof(name), "h%d drop", test.holster);
            press(name);
        }
        else if(wait(c.holster[test.holster] == WID_FIST, "a holster's DROP"))
        {
            test.frames = 0; // (the next one)
        }
        break;
    case 2:
        result(c.hand[1] == WID_SHOTGUN && c.hand[0] == WID_NAILGUN, "the script's weapons: the shotgun right, the nailgun left");
        press("h0 l");
        next();
        break;
    case 3:
        if(wait(c.holster[0] == WID_NAILGUN && c.hand[0] == WID_FIST, "h0 L: the left hand's nailgun into the left shoulder"))
        {
            result(true, "h0 L: the left hand's nailgun into the left shoulder");
            next();
        }
        break;
    case 4:
        // The menu with a holster full, still for a moment (its thumbnails made, a few a frame): its picture, then h0 R.
        if(test.frames == 20)
        {
            Cbuf_AddText("screenshot\n");
        }
        if(test.frames == 40)
        {
            press("h0 r");
        }
        if(test.frames > 40 && wait(c.holster[0] == WID_SHOTGUN && c.hand[1] == WID_NAILGUN, "h0 R: the shoulder's nailgun and the right hand's shotgun traded"))
        {
            result(true, "h0 R: the shoulder's nailgun and the right hand's shotgun traded");
            press("swap");
            next();
        }
        break;
    case 5:
        if(wait(c.hand[0] == WID_NAILGUN && c.hand[1] == WID_FIST, "SWAP: the nailgun to the left hand"))
        {
            result(true, "SWAP: the nailgun to the left hand");
            press("h0 r");
            next();
        }
        break;
    case 6:
        if(wait(c.hand[1] == WID_SHOTGUN && c.holster[0] == WID_FIST, "h0 R: the shotgun drawn into the empty right hand"))
        {
            result(true, "h0 R: the shotgun drawn into the empty right hand");
            // Both hands full: the weapon keys do nothing, a weapon walked over is left lying (QC prints its line).
            Cbuf_AddText("impulse 2; impulse 4; impulse 10; impulse 80\n");
            next();
        }
        break;
    case 7:
        // (Counted frames: the four impulses reach the server over the next frames.)
        if(test.frames >= 30)
        {
            result(c.hand[1] == WID_SHOTGUN && c.hand[0] == WID_NAILGUN, "2, 4 and the wheel's cycle change nothing");
            Cbuf_AddText("impulse 47\n"); // H (vr_flat_key_drop's impulse)
            next();
        }
        break;
    case 8:
        if(wait(c.hand[0] == WID_FIST, "H: the off (left) hand's nailgun dropped first"))
        {
            result(c.hand[1] == WID_SHOTGUN, "H: the off (left) hand's nailgun dropped first, the right's shotgun kept");
            Cbuf_AddText("impulse 47\n");
            next();
        }
        break;
    case 9:
        if(wait(c.hand[1] == WID_FIST, "H again: then the main (right) hand's shotgun"))
        {
            result(true, "H again: then the main (right) hand's shotgun");
            const int was = st.clicks;
            press("both drop");
            result(st.clicks == was, "DROP BOTH dim (does nothing) with both hands empty");
            Cbuf_AddText("impulse 154; wait; impulse 176\n"); // (the script's guns again: the shotgun right, the nailgun left)
            next();
        }
        break;
    case 10:
        // (Loaded as a pickup loads them: VR_FlatReload_CycleIn's clip.)
        if(wait(c.hand[1] == WID_SHOTGUN && c.hand[0] == WID_NAILGUN && c.clip[1] > 0 && c.clip[0] > 0, "both hands full again, loaded"))
        {
            // UNLOAD (82, 83): the right hand's shotgun's shells back into the reserve, then the left hand's DROP.
            test.shells = cl.stats[STAT_SHELLS];
            test.clip = c.clip[1];
            press("right unload");
            next();
        }
        break;
    case 11:
        // (Two phases, one impulse a frame: the UNLOAD's result, then the left hand's DROP.)
        if(test.clip > 0)
        {
            if(wait(c.clip[1] == 0, "the right hand's UNLOAD"))
            {
                Con_Printf("flatholster_test: UNLOAD: shells %d -> %d, the shotgun's clip %d -> %d\n", test.shells,
                    cl.stats[STAT_SHELLS], test.clip, c.clip[1]);
                result(cl.stats[STAT_SHELLS] == test.shells + test.clip, "UNLOAD: the shotgun's shells back into the reserve");
                const int was = st.clicks;
                press("right unload");
                result(st.clicks == was, "UNLOAD dim (does nothing) with the clip empty");
                test.clip = 0;
                test.frames = 0;
                press("left drop");
            }
        }
        else if(wait(c.hand[0] == WID_FIST, "the left hand's DROP"))
        {
            result(c.hand[1] == WID_SHOTGUN && c.clip[1] == 0, "the left hand's DROP (the unloaded shotgun not reloaded)");
            Cbuf_AddText("impulse 176\n");
            next();
        }
        break;
    case 12:
        if(wait(c.hand[0] == WID_NAILGUN, "the nailgun in the left hand again"))
        {
            press("both drop");
            next();
        }
        break;
    case 13:
        if(wait(c.hand[0] == WID_FIST && c.hand[1] == WID_FIST, "DROP BOTH: both hands empty"))
        {
            result(true, "DROP BOTH: both hands empty");
            Cbuf_AddText("impulse 154\n");
            next();
        }
        break;
    // vr_flat_holster_menu_mode 0 (the test's): letting go of the key clicks the button under the cursor.
    case 14:
        if(wait(c.hand[1] == WID_SHOTGUN, "the shotgun in the right hand again"))
        {
            pointAt({240.f, 124.f}); // (the right chest's slot: no button)
            test.clicks = st.clicks;
            st.cmdHeld = false;
            next();
        }
        break;
    case 15:
        if(wait(!st.open, "let go over no button: closed"))
        {
            result(st.clicks == test.clicks && c.hand[1] == WID_SHOTGUN && c.holster[1] == WID_FIST, "let go over no button: just closed");
            st.cmdHeld = true;
            next();
        }
        break;
    case 16:
        if(wait(st.open, "held again: open"))
        {
            int dx = 0, dy = 0;
            if(countsTo("h1 r", dx, dy))
            {
                (void)mouse(dx, dy); // (only pointed at: no click)
            }
            st.cmdHeld = false;
            next();
        }
        break;
    case 17:
        if(wait(c.holster[1] == WID_SHOTGUN && c.hand[1] == WID_FIST, "let go over h1 R: the right hand's shotgun into the right shoulder"))
        {
            result(!st.open, "let go over h1 R: the right hand's shotgun into the right shoulder, the menu closed");
            Cvar_SetValueQuick(&vr_flat_holster_menu_mode, 1.f);
            st.cmdHeld = true; // (a press: it opens)
            next();
        }
        break;
    // vr_flat_holster_menu_mode 1: a press opens it, as many clicks as wanted, the next press closes it.
    case 18:
        st.cmdHeld = false;
        if(test.frames >= 5 && wait(st.open, "toggle: a press opens it"))
        {
            result(true, "toggle: a press opens it, letting go keeps it open");
            press("h1 r");
            next();
        }
        break;
    case 19:
        if(wait(c.hand[1] == WID_SHOTGUN && c.holster[1] == WID_FIST, "toggle: click 1 (h1 R)"))
        {
            result(st.open, "toggle: click 1 (h1 R: the shotgun drawn), still open");
            press("swap");
            next();
        }
        break;
    case 20:
        if(wait(c.hand[0] == WID_SHOTGUN && c.hand[1] == WID_FIST, "toggle: click 2 (SWAP)"))
        {
            result(st.open, "toggle: click 2 (SWAP), still open");
            press("left drop");
            next();
        }
        break;
    case 21:
        if(wait(c.hand[0] == WID_FIST, "toggle: click 3 (the left hand's DROP)"))
        {
            result(st.open, "toggle: click 3 (the left hand's DROP), still open");
            st.cmdHeld = true; // (the next press)
            next();
        }
        break;
    case 22:
        st.cmdHeld = false; // (let go a few frames before the press that opens it again)
        if(test.frames >= 3 && wait(!st.open, "toggle: the next press closes it"))
        {
            result(true, "toggle: the next press closes it");
            st.cmdHeld = true;
            next();
        }
        break;
    case 23:
        st.cmdHeld = false;
        if(wait(st.open, "toggle: opened again"))
        {
            (void)escape();
            next();
        }
        break;
    case 24:
        // (Counted frames: it must stay shut, not open again on the next frame.)
        if(test.frames >= 10)
        {
            result(!st.open && !st.toggled, "toggle: Esc closes it");
            Cvar_SetValueQuick(&vr_flat_holster_menu_mode, test.mode);
            next();
        }
        break;
    case 25:
        if(wait(!shown() && timescale::current() >= 1.f, "the menu closed, time back at 1"))
        {
            result(true, "the menu closed, time back at 1");
            next();
        }
        break;
    default:
        test.tally.verdict("flatholster_test");
        test.step = -1;
        break;
    }
}

void test_f()
{
    test = Test{};
    test.step = 0;
    test.mode = vr_flat_holster_menu_mode.value;
    Cvar_SetValueQuick(&vr_flat_holster_menu_mode, 0.f);
    Con_Printf("flatholster_test: started\n");
}

void hold_f()
{
    st.cmdHeld = true;
}

void release_f()
{
    st.cmdHeld = false;
}

void dump_f()
{
    layOut();
    const Contents c = contents();
    Con_Printf("flat holsters: vr_flat_holsters %g, menu %s (fade %.2f), cursor %.0f %.0f, %d clicks, time scale %.2f\n",
        vr_flat_holsters.value, st.open ? "open" : "closed", st.fade, st.cursor.x, st.cursor.y, st.clicks, timescale::current());
    for(int i = 0; i < holsterCount; i++)
    {
        Con_Printf("  holster %d (%s): %s\n", i, places[i].title, c.holster[i] != WID_FIST ? notectx::weaponName(c.holster[i]) : "empty");
    }
    Con_Printf("  left hand: %s%s, right hand: %s%s\n", notectx::weaponName(c.hand[0]), c.carry[0] ? " (carries)" : "",
        notectx::weaponName(c.hand[1]), c.carry[1] ? " (carries)" : "");
    for(const Button& b : st.buttons)
    {
        Con_Printf("  button \"%s\" at %.0f %.0f %.0f x %.0f: %s, impulse %d\n", b.name, b.rect.x, b.rect.y, b.rect.z, b.rect.w,
            enabled(b, c) ? "on" : "off", impulseOf(b));
    }
}

} // namespace

void init()
{
    Cmd_AddCommand("+flatholsters", hold_f); // the wheel's button held, the console's way (a bind or a script)
    Cmd_AddCommand("-flatholsters", release_f);
    Cmd_AddCommand("vr_flat_holster_menu_dump", dump_f);
    Cmd_AddCommand("vr_flatholster_test", test_f);
}

} // namespace qvr::flatholster
