// vr_bindpanel.cpp -- see vr_bindpanel.hpp.

#include "vr_bindpanel.hpp"

#include "vr_api.h"
#include "vr_bindsetup.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_flatholster.hpp"
#include "vr_flatplay.hpp"
#include "vr_menuui.hpp"
#include "vr_seat.hpp"

#include "Zancle/Base/GetArraySize.hpp"
#include "Zancle/Math/MinMax.hpp"

#include <ctype.h>

namespace qvr::bindpanel
{

namespace
{

// An action: its flat key cvar (its keys, up to two), or a bind's command (the first two keys bound to it: fire and aim
// are binds), or the weapon wheel's pair (the first keys of the two cycle keys, held).
struct Action
{
    const char* label;
    const cvar_t* key;    // nullptr: by `command`
    const char* command;  // the bind's command ("" none)
    const cvar_t* second; // the wheel: the other hand's key (its first)
};
const Action actions[] = {
    {"Fire", nullptr, "+attack", nullptr},
    {"Aim / off hand", nullptr, "+offhandattack", nullptr},
    {"Reload", &vr_flat_key_reload, "", nullptr},
    {"Wheel (hold)", &vr_flat_key_cycle_left, "", &vr_flat_key_cycle_right},
    {"Holster menu", &vr_flat_key_holster_menu, "", nullptr},
    {"Swap hands", &vr_flat_key_swap, "", nullptr},
    {"Grab / pull", &vr_flat_key_pull, "", nullptr},
    {"Force grab", &vr_flat_key_grab, "", nullptr},
    {"Throw / drop", &vr_flat_key_throw, "", nullptr},
    {"Shove", &vr_flat_key_shove, "", nullptr},
    {"Punch", &vr_flat_key_punch, "", nullptr},
    {"Grenade", &vr_flat_key_grenade, "", nullptr},
    {"Crouch / slide", &vr_flat_key_crouch, "", nullptr},
    {"Flashlight", &vr_flat_key_flashlight, "", nullptr},
};
static_assert(za::getArraySize(actions) <= maxRows, "maxRows holds them");

// The keys' names as shown (their texts: rows()'s), and the image over the holster menu as last made: each seat's
// (split20, seat::Part::BindPanel: each tile's panel shows its own seat's binds, made once a frame for that seat).
struct Texts
{
    char keys[maxRows][2][16]{};
};

struct Image
{
    gadget::VisorImage image;
    int frame{-1}; // host_framecount it was made in
};

struct BindPanelSeat
{
    Texts texts;
    Image panelImage;
};

[[nodiscard]] BindPanelSeat& panelSeat()
{
    return seat::part<BindPanelSeat>(seat::current(), seat::Part::BindPanel);
}

// split20: a controller seat's (seats 2-4: a pad, no keyboard) rows: its controller layout (seat::padLayout), each with
// the buttons the seat's own bindings give its command (its Controller Bindings screen edits them).
[[nodiscard]] bool padSeat()
{
    return seat::current().index > 0;
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

// The first `n` keys bound to `command` (-1 past them), the current seat's (its bindings).
void boundKeys(const char* command, int (&out)[2])
{
    out[0] = out[1] = -1;
    int n = 0;
    const seatkeys_t* sk = Key_Seat();
    for(int k = 0; k < MAX_KEYS && n < 2; k++)
    {
        if(sk->bindings[k] && !q_strcasecmp(sk->bindings[k], command) && (Key_GetDeviceMaskForKeynum(k) & KDM_KEYBOARD_AND_MOUSE))
        {
            out[n++] = k;
        }
    }
}

// A pad button's name on its cap (the friendly one: "A", "RT"...).
void padKeyName(int key, char (&out)[16])
{
    out[0] = 0;
    if(key >= 0)
    {
        const char* name = Key_KeynumToFriendlyString(key);
        q_strlcpy(out, name ? name : "", sizeof(out));
    }
}

// The pad seat's rows (padSeat): its layout's actions with its own buttons for them.
int padRows(gadget::BindPanelRow* out, int max)
{
    int count = 0;
    const seat::PadAction* layout = seat::padLayout(count);
    Texts& texts = panelSeat().texts;
    int n = 0;
    for(int i = 0; i < count && n < max && n < maxRows; i++)
    {
        int keys[2] = {-1, -1};
        (void)Key_GetKeysForCommand(layout[i].command, keys, 2, KDM_GAMEPAD);
        gadget::BindPanelRow& r = out[n];
        r.action = layout[i].label;
        for(int s = 0; s < 2; s++)
        {
            padKeyName(keys[s], texts.keys[n][s]);
            r.keys[s] = texts.keys[n][s];
        }
        n++;
    }
    return n;
}

} // namespace

int rows(gadget::BindPanelRow* out, int max)
{
    if(padSeat())
    {
        return padRows(out, max);
    }
    Texts& texts = panelSeat().texts;
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
        r.action = a.label;
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
    Image& panelImage = panelSeat().panelImage;
    if(panelImage.frame != host_framecount)
    {
        panelImage.frame = host_framecount;
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

void setupBindings()
{
    if(bindsetup::open())
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
    for(int i = 0; i < n; i++)
    {
        Con_Printf("bindpanel: row %d \"%s\" keys \"%s\" \"%s\"\n", i, r[i].action, r[i].keys[0], r[i].keys[1]);
    }
    menuui::bindPanelReport();
    const char* menu = key_dest != key_menu ? "none" : m_state == m_main ? "main" : m_state == m_keys ? "keys" : "other";
    Con_Printf("bindpanel: %d rows, shown %d, menu %s\n", n, shown() ? 1 : 0, menu);
}
} // namespace

void init()
{
    Cmd_AddCommand("vr_bindpanel_report", report_f);
    Cmd_AddCommand("vr_bindpanel_setup", setupBindings); // (the main menu's Setup Bindings, for the tests)
}

} // namespace qvr::bindpanel
