// vr_menu.cpp -- the "VR Settings" page (Options > VR Settings), drawn like Ironwail's
// options pages: a scrolling list of labelled settings, changed with left/right (the sticks in
// VR), with actions on enter (A).

#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_main.hpp"

#include <cmath>
#include <cstring>
#include <vector>

using namespace qvr;

namespace
{

struct Choice
{
    float value;
    const char* label;
};

struct Item
{
    enum Kind
    {
        Header,
        Slider,
        Cycle,
        Action
    };

    Kind kind;
    const char* label;
    cvar_t* cvar{nullptr};

    // Slider.
    float min{0.f};
    float max{1.f};
    float step{0.1f};
    const char* format{"%.2f"};

    // Cycle.
    std::vector<Choice> choices;

    // Action.
    void (*action)(){nullptr};
};

void calibrateHeight()
{
    const TrackingState& t = tracking();
    if(vrActive() && t.head.valid)
    {
        Cvar_SetValueQuick(&vr_height_calibration, t.head.position.y);
        Con_Printf("VR: height calibrated to %.2f m\n", t.head.position.y);
    }
}

void restartVr()
{
    Cbuf_AddText("vr_restart\n");
}

[[nodiscard]] Item header(const char* label)
{
    return {Item::Header, label};
}

[[nodiscard]] Item slider(const char* label, cvar_t& cvar, float min, float max, float step, const char* format)
{
    Item i{Item::Slider, label, &cvar};
    i.min = min;
    i.max = max;
    i.step = step;
    i.format = format;
    return i;
}

[[nodiscard]] Item cycle(const char* label, cvar_t& cvar, std::vector<Choice> choices)
{
    Item i{Item::Cycle, label, &cvar};
    i.choices = std::move(choices);
    return i;
}

[[nodiscard]] Item toggle(const char* label, cvar_t& cvar)
{
    return cycle(label, cvar, {{0.f, "Off"}, {1.f, "On"}});
}

[[nodiscard]] Item action(const char* label, void (*fn)())
{
    Item i{Item::Action, label};
    i.action = fn;
    return i;
}

[[nodiscard]] const std::vector<Item>& items()
{
    static const std::vector<Item> list = {
        header("Comfort"),
        cycle("Turning", vr_snap_turn, {{0.f, "Smooth"}, {30.f, "Snap 30"}, {45.f, "Snap 45"}, {90.f, "Snap 90"}}),
        slider("Turn Speed", vr_turn_speed, 1.f, 8.f, 0.25f, "%.2f"),
        cycle("Move Towards", vr_movement_mode, {{1.f, "Head"}, {0.f, "Off hand"}}),
        slider("Stick Deadzone", vr_deadzone, 0.f, 50.f, 5.f, "%.0f%%"),
        toggle("Teleport", vr_teleport_enabled),
        slider("Teleport Range", vr_teleport_range, 100.f, 800.f, 50.f, "%.0f"),
        slider("Room Scale", vr_roomscale_move_mult, 0.5f, 2.f, 0.1f, "%.1fx"),

        header("Body"),
        toggle("Left Handed", vr_lefthanded),
        slider("Height", vr_height_calibration, 1.2f, 2.2f, 0.01f, "%.2f m"),
        action("Set Height Now", calibrateHeight),
        slider("World Scale", vr_world_scale, 0.75f, 1.5f, 0.05f, "%.2f"),
        slider("Floor Offset", vr_floor_offset, -40.f, 10.f, 1.f, "%.0f"),

        header("Weapons"),
        slider("Gun Angle", vr_gunangle, -30.f, 90.f, 2.5f, "%.1f"),
        slider("Off Hand Angle", vr_offhandpitch, -30.f, 90.f, 2.5f, "%.1f"),
        cycle("Weapon Grip", vr_weapon_grip_mode, {{0.f, "Hold"}, {1.f, "Sticky"}}),
        cycle("Two-Handed", vr_2h_mode, {{0.f, "Off"}, {1.f, "Basic"}, {2.f, "Virtual stock"}}),
        slider("Throw Speed", vr_weapon_throw_velocity_mult, 0.5f, 3.f, 0.1f, "%.1fx"),
        cycle("Throw Gravity", vr_throw_gravity, {{9.81f, "Real"}, {0.f, "Quake"}}),
        cycle("Haptics", vr_disablehaptics, {{0.f, "On"}, {1.f, "Off"}}),
        cycle("Crosshair", vr_crosshair, {{0.f, "Off"}, {1.f, "Dot"}, {2.f, "Laser"}, {3.f, "Soft laser"}}),
        slider("Crosshair Size", vr_crosshair_size, 0.5f, 8.f, 0.5f, "%.1f"),

        header("Display"),
        cycle("HUD", vr_hud_mode, {{1.f, "Wrist gadget"}, {0.f, "Status bar"}}),
        cycle("Status Bar", vr_sbar_mode, {{1.f, "Off hand"}, {0.f, "Main hand"}}),
        slider("HUD Scale", vr_hud_scale, 0.01f, 0.05f, 0.0025f, "%.4f"),
        slider("Menu Distance", vr_menu_distance, 40.f, 150.f, 5.f, "%.0f"),
        slider("Menu Scale", vr_menu_scale, 0.08f, 0.3f, 0.01f, "%.2f"),
        cycle("Desktop Mirror", vr_mirror, {{0.f, "Off"}, {1.f, "Left eye"}, {2.f, "Both eyes"}}),
        cycle("Body", vr_body_mode, {{0.f, "Off"}, {1.f, "Torso"}, {2.f, "Torso and arms"}, {3.f, "Full body"}}),
        cycle("Build", vr_body_build, {{0.f, "Lean"}, {1.f, "Athletic"}, {2.f, "Brawny"}}),
        slider("Torso Offset", vr_body_torso_back, -0.15f, 0.3f, 0.01f, "%.2f m back"),
        slider("Legs Offset", vr_body_legs_back, -0.15f, 0.3f, 0.01f, "%.2f m back"),
        toggle("Holster Models", vr_leg_holster_model_enabled),

        header("Headset"),
        toggle("VR", vr_enabled),
        action("Restart VR", restartVr),
    };
    return list;
}

int cursor = 1;
int scroll = 0;

constexpr int listTop = 36;
constexpr int midPos = 204; // as Ironwail's OPTIONS_MIDPOS

[[nodiscard]] int visibleRows()
{
    return (200 - listTop - 8) / 8;
}

void moveCursor(int dir)
{
    const auto& list = items();
    const int n = static_cast<int>(list.size());
    int i = cursor;
    do
    {
        i = (i + dir + n) % n;
    } while(list[i].kind == Item::Header);
    cursor = i;
}

[[nodiscard]] int currentChoice(const Item& item)
{
    int best = 0;
    for(int i = 0; i < static_cast<int>(item.choices.size()); i++)
    {
        if(std::fabs(item.choices[i].value - item.cvar->value) < std::fabs(item.choices[best].value - item.cvar->value))
        {
            best = i;
        }
    }
    return best;
}

void change(const Item& item, int dir)
{
    switch(item.kind)
    {
        case Item::Slider:
        {
            float v = item.cvar->value + dir * item.step;
            v = std::round(v / item.step) * item.step;
            Cvar_SetValueQuick(item.cvar, CLAMP(item.min, v, item.max));
            break;
        }
        case Item::Cycle:
        {
            const int n = static_cast<int>(item.choices.size());
            Cvar_SetValueQuick(item.cvar, item.choices[(currentChoice(item) + dir + n) % n].value);
            break;
        }
        case Item::Action:
            if(dir > 0)
            {
                item.action();
            }
            break;
        default: break;
    }
    S_LocalSound("misc/menu3.wav");
}

void drawItem(const Item& item, int y, bool selected)
{
    if(item.kind == Item::Header)
    {
        M_PrintWhite((320 - 8 * static_cast<int>(strlen(item.label))) / 2, y, item.label);
        return;
    }

    M_Print(midPos - 28 - 8 * static_cast<int>(strlen(item.label)), y, item.label);

    char buf[64];
    switch(item.kind)
    {
        case Item::Slider:
        {
            q_snprintf(buf, sizeof(buf), item.format, item.cvar->value);
            const float range = (item.cvar->value - item.min) / (item.max - item.min);
            M_DrawSlider(midPos, y, CLAMP(0.f, range, 1.f), buf);
            break;
        }
        case Item::Cycle: M_Print(midPos, y, item.choices[currentChoice(item)].label); break;
        case Item::Action: M_Print(midPos - 4, y, "..."); break;
        default: break;
    }

    if(selected)
    {
        M_DrawArrowCursor(midPos - 20, y);
    }
}

} // namespace

extern "C" void VR_Menu_Open()
{
    IN_DeactivateForMenu();
    key_dest = key_menu;
    m_state = m_vr;
    m_entersound = true;
}

extern "C" void VR_Menu_Draw()
{
    const auto& list = items();

    M_DrawTransPic(16, 4, Draw_CachePic("gfx/qplaque.lmp"));
    qpic_t* title = Draw_CachePic("gfx/p_option.lmp");
    M_DrawPic((320 - title->width) / 2, 4, title);
    M_PrintWhite((320 - 8 * 11) / 2, 28, "VR Settings");

    const int rows = visibleRows();
    const int n = static_cast<int>(list.size());
    if(cursor < scroll)
    {
        scroll = cursor > 0 && list[cursor - 1].kind == Item::Header ? cursor - 1 : cursor;
    }
    if(cursor >= scroll + rows)
    {
        scroll = cursor - rows + 1;
    }
    scroll = CLAMP(0, scroll, q_max(n - rows, 0));

    for(int i = scroll; i < n && i < scroll + rows; i++)
    {
        drawItem(list[i], listTop + (i - scroll) * 8, i == cursor);
    }
}

extern "C" void VR_Menu_Key(int key)
{
    const auto& list = items();

    switch(key)
    {
        case K_ESCAPE:
        case K_BBUTTON:
        case K_MOUSE2:
        case K_MOUSE4: M_Menu_Options_f(); break;

        case K_UPARROW:
        case K_MWHEELUP:
            S_LocalSound("misc/menu1.wav");
            moveCursor(-1);
            break;

        case K_DOWNARROW:
        case K_MWHEELDOWN:
            S_LocalSound("misc/menu1.wav");
            moveCursor(1);
            break;

        case K_LEFTARROW: change(list[cursor], -1); break;
        case K_RIGHTARROW: change(list[cursor], 1); break;

        case K_ENTER:
        case K_KP_ENTER:
        case K_ABUTTON:
        case K_MOUSE1:
            if(list[cursor].kind != Item::Slider)
            {
                change(list[cursor], 1);
            }
            break;

        default: break;
    }
}
