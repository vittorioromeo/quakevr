// vr_bindpanel.cpp -- see vr_bindpanel.hpp.

#include "vr_bindpanel.hpp"

#include "vr_api.h"
#include "vr_bindsetup.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_flatholster.hpp"
#include "vr_flatplay.hpp"
#include "vr_menuui.hpp"

#include "Zancle/Base/GetArraySize.hpp"
#include "Zancle/Base/Strlen.hpp"
#include "Zancle/Math/MinMax.hpp"

#include <ctype.h>

namespace qvr::bindpanel
{

namespace
{

// An action: its flat key cvar (its keys, up to two), or a bind's command (the first two keys bound to it: fire and aim
// are binds), or the weapon wheel's pair (the first keys of the two cycle keys, held).
// panelfix: the main actions first (drawn large), then the minor ones (smaller) in groups, a group's first row carrying
// its title.
struct Action
{
    const char* label;
    const cvar_t* key;    // nullptr: by `command`
    const char* command;  // the bind's command ("" none)
    const cvar_t* second; // a left / right pair (the wheel, cycle, holster, drop): the right hand's key (each its first)
    bool minor;
    const char* header;
};
const Action actions[] = {
    {"Fire", nullptr, "+attack", nullptr, false, ""},
    {"Aim / off hand", nullptr, "+offhandattack", nullptr, false, ""},
    {"Reload", &vr_flat_key_reload, "", nullptr, false, ""},
    {"Wheel (hold)", &vr_flat_key_cycle_left, "", &vr_flat_key_cycle_right, false, ""},
    {"Holster menu", &vr_flat_key_holster_menu, "", nullptr, false, ""},
    {"Grab / pull", &vr_flat_key_pull, "", nullptr, false, ""},
    {"Throw / drop", &vr_flat_key_throw, "", nullptr, false, ""},
    {"Grenade", &vr_flat_key_grenade, "", nullptr, false, ""},
    {"Crouch / slide", &vr_flat_key_crouch, "", nullptr, false, ""},
    {"Cycle L / R", &vr_flat_key_cycle_left, "", &vr_flat_key_cycle_right, true, "Hands"},
    {"Holster L / R", &vr_flat_key_holster_left, "", &vr_flat_key_holster_right, true, ""},
    {"Drop L / R", &vr_flat_key_drop_left, "", &vr_flat_key_drop_right, true, ""},
    {"Swap hands", &vr_flat_key_swap, "", nullptr, true, ""},
    {"Next ammo", &vr_flat_key_ammo, "", nullptr, true, ""},
    {"Attach / detach", &vr_flat_key_attach, "", nullptr, true, ""},
    {"Force grab", &vr_flat_key_grab, "", nullptr, true, "More"},
    {"Shove", &vr_flat_key_shove, "", nullptr, true, ""},
    {"Punch", &vr_flat_key_punch, "", nullptr, true, ""},
    {"Lie down", &vr_flat_key_prone, "", nullptr, true, ""},
    {"Flashlight", &vr_flat_key_flashlight, "", nullptr, true, ""},
};
static_assert(za::getArraySize(actions) <= maxRows, "maxRows holds them");

// The keys' names as shown (their texts: rows()'s; a controller's rows: their actions cut to fit, their glyphs' files),
// and the image over the holster menu or the weapon wheel as last made (the flat rows' or the headset's pictures: one
// gadget target, the two never show together).
struct Texts
{
    char keys[maxRows][2][16]{};
    char actions[maxRows][20]{};
    const char* glyphNames[maxRows]{};
};
Texts texts;

struct Image
{
    gadget::VisorImage image;
    int frame{-1};        // host_framecount it was made in
    bool headset{false};  // the headset's panel (else the flat rows)
};
Image panelImage;

// bindpanel2: the controller the panels show (vr_bindpanel_device: a model forced, Count: the detected one), and the
// headset's panel as last made (vr_bindpanel_report, the weapon wheel's test).
struct Device
{
    bindsetup::Model forced{bindsetup::Model::Count};
};
Device device;

constexpr int maxCallouts = 32;
struct Headset
{
    bindsetup::PanelButton buttons[maxCallouts];
    gadget::BindPanelCallout callouts[maxCallouts];
    char texts[maxCallouts][20]{};
    bindsetup::PanelPictures pictures;
    int count{0};
    bindsetup::Model model{bindsetup::Model::Count};
    int renders{0};     // images made
    int lastFrame{-10}; // host_framecount of the last
    int glyphs{0};      // the last one's callouts with a glyph
};
Headset headset;

// `text` into `out`, cut to its size ("..": cut).
template <int N>
void fitInto(const char* text, char (&out)[N])
{
    q_strlcpy(out, text, N);
    if(static_cast<int>(ZA_STRLEN(text)) >= N)
    {
        out[N - 3] = '.';
        out[N - 2] = '.';
    }
}

// The flat rows' device: a gamepad when it is the device last used (a model forced, or SDL's type), else the keyboard.
[[nodiscard]] bindsetup::Model flatDevice()
{
    if(device.forced != bindsetup::Model::Count)
    {
        return device.forced;
    }
    if(IN_GamepadLastUsed())
    {
        return bindsetup::modelForPad(IN_GetGamepadSdlType());
    }
    return bindsetup::Model::Keyboard;
}

// The headset's: the controllers detected (bindsetup::detect), or a model forced.
[[nodiscard]] bindsetup::Model headsetDevice()
{
    return device.forced != bindsetup::Model::Count ? device.forced : bindsetup::detect();
}

// A controller's rows: each bound button (the menu button left out), its action and its glyph (its name the fallback).
int controllerRows(bindsetup::Model m, gadget::BindPanelRow* out, int max)
{
    bindsetup::PanelPictures pics;
    bindsetup::PanelButton buttons[maxCallouts];
    const int count = bindsetup::panelButtons(m, pics, buttons, maxCallouts);
    int n = 0;
    for(int i = 0; i < count && n < max && n < maxRows; i++)
    {
        const bindsetup::PanelButton& b = buttons[i];
        if(b.fixed || !b.action[0])
        {
            continue;
        }
        fitInto(b.action, texts.actions[n]);
        q_strlcpy(texts.keys[n][0], b.name, sizeof(texts.keys[n][0]));
        texts.keys[n][1][0] = 0;
        texts.glyphNames[n] = b.glyphName;
        gadget::BindPanelRow& r = out[n++];
        r.action = texts.actions[n - 1];
        r.keys[0] = texts.keys[n - 1][0];
        r.keys[1] = "";
        r.glyphs[0] = b.glyph;
        r.glyphs[1] = nullptr;
        r.minor = false; // (a pad's bound buttons: one group, all large)
        r.header = "";
    }
    return n;
}

// A key's name on its cap: Quake's, upper case, the mouse's buttons short.
void keyName(int key, char (&out)[16])
{
    out[0] = 0;
    if(key < 0)
    {
        return;
    }
    const char* name = Key_KeynumToString(key);
    if(!name || !name[0] || !q_strcasecmp(name, "<UNKNOWN KEYNUM>"))
    {
        return;
    }
    constexpr const char* shortNames[][2] = {{"MOUSE1", "LMB"}, {"MOUSE2", "RMB"}, {"MOUSE3", "MMB"}, {"MOUSE4", "M4"},
        {"MOUSE5", "M5"}, {"MWHEELUP", "WHEEL UP"}, {"MWHEELDOWN", "WHEEL DN"}, {"CAPSLOCK", "CAPS"}};
    for(const auto& s : shortNames)
    {
        if(!q_strcasecmp(name, s[0]))
        {
            q_strlcpy(out, s[1], sizeof(out));
            return;
        }
    }
    q_strlcpy(out, name, sizeof(out));
    for(char* c = out; *c; c++)
    {
        *c = static_cast<char>(toupper(static_cast<unsigned char>(*c)));
    }
}

// The first `n` keys bound to `command` (-1 past them).
void boundKeys(const char* command, int (&out)[2])
{
    out[0] = out[1] = -1;
    int n = 0;
    for(int k = 0; k < MAX_KEYS && n < 2; k++)
    {
        if(keybindings[k] && !q_strcasecmp(keybindings[k], command) && (Key_GetDeviceMaskForKeynum(k) & KDM_KEYBOARD_AND_MOUSE))
        {
            out[n++] = k;
        }
    }
}

} // namespace

int rows(gadget::BindPanelRow* out, int max)
{
    const bindsetup::Model m = flatDevice();
    if(m != bindsetup::Model::Keyboard)
    {
        const int n = controllerRows(m, out, max);
        if(n > 0)
        {
            return n;
        }
    }
    int n = 0;
    for(const Action& a : actions)
    {
        if(n >= max || n >= maxRows)
        {
            break;
        }
        int keys[2] = {-1, -1};
        if(a.second)
        {
            keys[0] = flatplay::keyNum(*a.key, 0);
            keys[1] = flatplay::keyNum(*a.second, 0);
        }
        else if(a.key)
        {
            keys[0] = flatplay::keyNum(*a.key, 0);
            keys[1] = flatplay::keyNum(*a.key, 1);
        }
        else
        {
            boundKeys(a.command, keys);
        }
        gadget::BindPanelRow& r = out[n];
        texts.glyphNames[n] = nullptr;
        r.action = a.label;
        r.minor = a.minor;
        r.header = a.header;
        for(int s = 0; s < 2; s++)
        {
            keyName(keys[s], texts.keys[n][s]);
            r.keys[s] = texts.keys[n][s];
        }
        if(!r.keys[0][0] && r.keys[1][0]) // (the second alone: first)
        {
            r.keys[0] = r.keys[1];
            r.keys[1] = "";
        }
        n++;
    }
    return n;
}

bool shown()
{
    return vr_flat_bind_panel.value != 0.f && flatplay::active();
}

namespace
{
[[nodiscard]] const gadget::VisorImage& image()
{
    if(panelImage.frame != host_framecount || panelImage.headset)
    {
        panelImage.frame = host_framecount;
        panelImage.headset = false;
        gadget::BindPanelRow r[maxRows];
        gadget::BindPanelView v;
        v.rows = r;
        v.rowCount = rows(r, maxRows);
        v.columns = 2;
        panelImage.image = gadget::renderBindPanel(v);
    }
    return panelImage.image;
}
} // namespace

glm::vec2 size()
{
    gadget::BindPanelRow r[maxRows];
    gadget::BindPanelView v;
    v.rows = r;
    v.rowCount = rows(r, maxRows);
    v.columns = 2;
    return gadget::bindPanelSize(v);
}

void drawOnWindow(float x, float y, float scale, float alpha)
{
    const gadget::VisorImage& img = image();
    if(!img.shown || alpha <= 0.f)
    {
        return;
    }
    flatholster::drawOnWindow(img.texture, img.size, {x, y, img.size.x * scale, img.size.y * scale}, alpha);
}

bool inMainMenu()
{
    return vr_flat_bind_panel.value != 0.f;
}

namespace
{
// The headset's panel's view: the controller's pictures, each bound button's glyph and action (the menu button left
// out). False when there is no controller to show.
[[nodiscard]] bool headsetView(gadget::BindPanelView& v)
{
    Headset& h = headset;
    h.model = headsetDevice();
    const int count = bindsetup::panelButtons(h.model, h.pictures, h.buttons, maxCallouts);
    h.count = 0;
    h.glyphs = 0;
    for(int i = 0; i < count; i++)
    {
        const bindsetup::PanelButton& b = h.buttons[i];
        if(b.fixed || !b.action[0])
        {
            continue;
        }
        gadget::BindPanelCallout& c = h.callouts[h.count];
        fitInto(b.action, h.texts[h.count]);
        c.picture = b.picture;
        c.u = b.u;
        c.v = b.v;
        c.glyph = b.glyph;
        c.text = h.texts[h.count];
        h.glyphs += b.glyph ? 1 : 0;
        h.buttons[h.count] = b; // (kept in the callouts' order: the report's)
        h.count++;
    }
    if(h.pictures.count <= 0 || h.count <= 0)
    {
        return false;
    }
    v.title = bindsetup::modelName(h.model);
    v.pictureCount = h.pictures.count;
    for(int p = 0; p < h.pictures.count; p++)
    {
        v.pictures[p] = h.pictures.pic[p];
        v.aspects[p] = h.pictures.aspect[p];
    }
    v.callouts = h.callouts;
    v.calloutCount = h.count;
    return true;
}
} // namespace

gadget::VisorImage headsetImage()
{
    if(vr_wheel_bind_panel.value == 0.f)
    {
        return {};
    }
    if(panelImage.frame != host_framecount || !panelImage.headset)
    {
        panelImage.frame = host_framecount;
        panelImage.headset = true;
        gadget::BindPanelView v;
        panelImage.image = headsetView(v) ? gadget::renderBindPanel(v) : gadget::VisorImage{};
        if(panelImage.image.shown)
        {
            headset.renders++;
            headset.lastFrame = host_framecount;
        }
    }
    return panelImage.image;
}

void setupBindings()
{
    // (the device the panel shows: a pad's screen while a pad is in use, else the keyboard's keys; bindpanel2. The
    // headset's controllers are not asked here: the main menu's panel is a flat screen's.)
    const bindsetup::Model m = flatDevice();
    if(m != bindsetup::Model::Keyboard && bindsetup::open(m))
    {
        Con_DPrintf("bindpanel: Setup Bindings: the controller binding screen\n");
        return;
    }
    M_Menu_Keys_At("vr_flat_key_lean_left"); // (the flat keys' first row: their group)
    Con_DPrintf("bindpanel: Setup Bindings: Options > Controls, flat keys\n");
}

namespace
{
// vr_bindpanel_report: every row and its keys, then "bindpanel: N rows, shown <0/1>, menu <m_state>" (the tests'
// line; bindpanel_test.sh).
void report_f()
{
    gadget::BindPanelRow r[maxRows];
    const int n = rows(r, maxRows);
    int glyphs = 0;
    for(int i = 0; i < n; i++)
    {
        glyphs += r[i].glyphs[0] ? 1 : 0;
        Con_Printf("bindpanel: row %d \"%s\" keys \"%s\" \"%s\"%s%s%s%s%s%s\n", i, r[i].action, r[i].keys[0], r[i].keys[1],
            texts.glyphNames[i] ? " glyph \"" : "", texts.glyphNames[i] ? texts.glyphNames[i] : "", texts.glyphNames[i] ? "\"" : "",
            r[i].minor ? " minor" : "", r[i].header[0] ? " group " : "", r[i].header);
    }
    menuui::bindPanelReport();
    // The headset's panel (bindpanel2): its view now (the controller the weapon wheel would show) and how often it was made.
    gadget::BindPanelView v;
    const bool has = headsetView(v);
    for(int i = 0; i < headset.count; i++)
    {
        const bindsetup::PanelButton& b = headset.buttons[i];
        Con_Printf("bindpanel: callout %d hand %d \"%s\" glyph \"%s\"%s \"%s\"\n", i, b.picture, b.name, b.glyphName,
            b.glyph ? "" : " (missing)", headset.texts[i]);
    }
    Con_Printf("bindpanel: headset panel %s: %s, pictures %d, callouts %d, glyphs %d; made %d times, last %d frames ago\n",
        vr_wheel_bind_panel.value != 0.f && has ? "on" : "off", bindsetup::modelId(headset.model), headset.pictures.count,
        headset.count, headset.glyphs, headset.renders, host_framecount - headset.lastFrame);
    const char* menu = key_dest != key_menu ? "none" : m_state == m_main ? "main" : m_state == m_keys ? "keys" : "other";
    Con_Printf("bindpanel: %d rows, shown %d, menu %s, device %s, glyphs %d\n", n, shown() ? 1 : 0, menu,
        bindsetup::modelId(flatDevice()), glyphs);
}

// vr_bindpanel_device [model | auto | next]: the controller the reminder panels show (a model id as menu_bindsetup's:
// quest3, index, xboxone, playstation, keyboard...; auto: the detected one; next: the next model). The tests', and the
// Debug menu's (a pad's or another headset's panel without one).
void device_f()
{
    using bindsetup::Model;
    const char* arg = Cmd_Argc() >= 2 ? Cmd_Argv(1) : "";
    Model m = Model::Count;
    if(!q_strcasecmp(arg, "next"))
    {
        const int at = device.forced == Model::Count ? -1 : static_cast<int>(device.forced);
        device.forced = static_cast<Model>((at + 1) % static_cast<int>(Model::Count));
    }
    else if(!q_strcasecmp(arg, "auto"))
    {
        device.forced = Model::Count;
    }
    else if(arg[0] && bindsetup::modelFromId(arg, m))
    {
        device.forced = m;
    }
    else if(arg[0])
    {
        Con_Printf("bindpanel: no controller \"%s\"\n", arg);
        return;
    }
    Con_Printf("bindpanel: device %s (%s)\n", device.forced == Model::Count ? "auto" : bindsetup::modelId(device.forced),
        device.forced == Model::Count ? "detected" : bindsetup::modelName(device.forced));
}
} // namespace

void init()
{
    Cmd_AddCommand("vr_bindpanel_report", report_f);
    Cmd_AddCommand("vr_bindpanel_device", device_f);
    Cmd_AddCommand("vr_bindpanel_setup", setupBindings); // (the main menu's Setup Bindings, for the tests)
}

} // namespace qvr::bindpanel
