// vr_menu.cpp -- the "VR Settings" pages (Options > VR Settings), drawn like Ironwail's options
// pages: scrolling lists of labelled settings, changed with left/right (the sticks in VR), with
// actions on enter (A). "Advanced VR Options" at the bottom opens a list of further pages: the old
// Quake VR settings pages (vr_menu_pages.inc) and the new body, throwing and force grab tweaks.
// Escape (B) goes back a page.

#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_main.hpp"
#include "vr_menu.hpp"

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

    // Action: a function, or a page to open.
    void (*action)(){nullptr};
    int page{-1};

    // Shown under the list while selected.
    const char* helpText{nullptr};

    [[nodiscard]] Item help(const char* text) const
    {
        Item i = *this;
        i.helpText = text;
        return i;
    }
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

[[nodiscard]] Item slider(const char* label, cvar_t* cvar, float min, float max, float step, const char* format)
{
    Item i{Item::Slider, label, cvar};
    i.min = min;
    i.max = max;
    i.step = step;
    i.format = format;
    return i;
}

[[nodiscard]] Item slider(const char* label, cvar_t& cvar, float min, float max, float step, const char* format)
{
    return slider(label, &cvar, min, max, step, format);
}

// By name, for settings of Ironwail's too: left out when there is no such cvar.
[[nodiscard]] Item slider(const char* label, const char* cvar, float min, float max, float step, const char* format)
{
    return slider(label, Cvar_FindVar(cvar), min, max, step, format);
}

[[nodiscard]] Item cycle(const char* label, cvar_t* cvar, std::vector<Choice> choices)
{
    Item i{Item::Cycle, label, cvar};
    i.choices = std::move(choices);
    return i;
}

[[nodiscard]] Item cycle(const char* label, cvar_t& cvar, std::vector<Choice> choices)
{
    return cycle(label, &cvar, std::move(choices));
}

[[nodiscard]] Item cycle(const char* label, const char* cvar, std::vector<Choice> choices)
{
    return cycle(label, Cvar_FindVar(cvar), std::move(choices));
}

[[nodiscard]] Item toggle(const char* label, cvar_t& cvar)
{
    return cycle(label, cvar, {{0.f, "Off"}, {1.f, "On"}});
}

[[nodiscard]] Item toggle(const char* label, const char* cvar)
{
    return cycle(label, cvar, {{0.f, "Off"}, {1.f, "On"}});
}

[[nodiscard]] Item action(const char* label, void (*fn)())
{
    Item i{Item::Action, label};
    i.action = fn;
    return i;
}

[[nodiscard]] Item open(const char* label, int page)
{
    Item i{Item::Action, label};
    i.page = page;
    return i;
}

#include "vr_menu_pages.inc"

// ----------------------------------------------------------------------------
// Pages of the port's own tweaks
// ----------------------------------------------------------------------------

[[nodiscard]] std::vector<Item> pageBody()
{
    return {
        cycle("Body", vr_body_mode, {{0.f, "Off"}, {2.f, "Torso and arms"}, {3.f, "Full body"}}),
        cycle("Build", vr_body_build, {{0.f, "Lean"}, {1.f, "Athletic"}, {2.f, "Brawny"}}),
        toggle("Walking Legs", vr_body_walk).help("The legs (full body) walk as you move with the stick."),
        toggle("Show Armour and Wounds", vr_body_state)
            .help("The armour you wear plates your torso; your arms get bloodier as you are hurt."),
        toggle("Show Powerups", vr_body_powerups)
            .help("Quad damage sparks around your hands, the pentagram makes you glow, the ring fades you."),
        toggle("Anchors Follow Body", vr_body_anchors)
            .help("Holsters, the virtual stock and hand collisions follow the body's lean and crouch."),
        slider("Torso Offset", vr_body_torso_back, -0.15f, 0.3f, 0.01f, "%.2f m")
            .help("How far the torso sits behind your neck (negative: in front)."),
        slider("Legs Offset", vr_body_legs_back, -0.15f, 0.3f, 0.01f, "%.2f m")
            .help("How far the feet stand behind your head (negative: in front)."),
        slider("Eyes Forward", vr_body_eye_forward, 0.f, 0.2f, 0.01f, "%.2f m")
            .help("From the top of the neck to the eyes, forward."),
        slider("Eyes Up", vr_body_eye_up, 0.f, 0.2f, 0.01f, "%.2f m").help("From the top of the neck to the eyes, up."),
        slider("Crouch: Knees", vr_body_crouch_knees, 0.f, 0.6f, 0.05f, "%.2f m")
            .help("How much of a crouch the legs take entirely before the back bends."),
        slider("Crouch: Legs Share", vr_body_crouch_legs, 0.f, 1.f, 0.05f, "%.2f")
            .help("Beyond that, the legs' share of the crouch (the rest bends the back)."),
        slider("Arm Length", vr_body_arm_length, 0.8f, 1.3f, 0.01f, "%.2f"),
        slider("Arm Stretch", vr_body_arm_stretch, 1.f, 1.5f, 0.05f, "%.2f")
            .help("How far arms may stretch to reach the hands (1: not at all)."),
        slider("Shoulder Reach", vr_body_shoulder_reach, 0.f, 0.2f, 0.01f, "%.2f m")
            .help("How far the shoulders may move out beyond that."),
        slider("Forearm Twist", vr_body_forearm_twist, 0.f, 1.f, 0.05f, "%.2f")
            .help("Share of the wrist's roll the forearm follows."),
        slider("Elbow Out", vr_body_elbow_out, 0.f, 1.f, 0.05f, "%.2f"),
        slider("Elbow Back", vr_body_elbow_back, 0.f, 1.f, 0.05f, "%.2f"),
        slider("Elbow From Hand", vr_body_elbow_hand, 0.f, 1.f, 0.05f, "%.2f")
            .help("How much the elbow points away from the back of the hand."),
        slider("Shoulders Up", vr_body_shoulder_up, 0.f, 45.f, 1.f, "%.0f deg")
            .help("How far the shoulders rise when reaching up."),
        slider("Shoulders Forward", vr_body_shoulder_forward, 0.f, 45.f, 1.f, "%.0f deg")
            .help("How far the shoulders swing forward when reaching far forward."),
    };
}

[[nodiscard]] std::vector<Item> pageGadget()
{
    return {
        cycle("HUD", vr_hud_mode, {{1.f, "Wrist gadget"}, {0.f, "Status bar"}}),
        cycle("Arm", vr_gadget_hand, {{0.f, "Off hand"}, {1.f, "Main hand"}}),
        slider("Size", vr_gadget_scale, 0.5f, 2.f, 0.05f, "%.2fx"),
        header("Placement"),
        slider("Along the Arm", vr_gadget_x, -10.f, 10.f, 0.5f, "%.1f cm"),
        slider("Across the Arm", vr_gadget_y, -5.f, 5.f, 0.25f, "%.2f cm"),
        slider("Height", vr_gadget_z, -3.f, 5.f, 0.25f, "%.2f cm").help("How far it stands out of the forearm."),
        slider("Pitch", vr_gadget_pitch, -90.f, 90.f, 5.f, "%.0f deg"),
        slider("Yaw", vr_gadget_yaw, -90.f, 90.f, 5.f, "%.0f deg"),
        slider("Roll", vr_gadget_roll, -180.f, 180.f, 15.f, "%.0f deg")
            .help("Turns the screen: 90 reads along the arm, 180 turns the text the other way."),
        header("Colours"),
        slider("Screen Hue", vr_gadget_screen_hue, 0.f, 355.f, 5.f, "%.0f")
            .help("The screen's colour: 128 green, 40 amber, 200 blue, 0 red."),
        slider("Screen Brightness", vr_gadget_screen_brightness, 0.3f, 1.5f, 0.05f, "%.2f"),
        slider("Screen Background", vr_gadget_screen_background, 0.f, 4.f, 0.1f, "%.1f"),
        slider("Casing Tint", vr_gadget_tint, 0.f, 1.f, 0.05f, "%.2f").help("0 keeps the casing's own olive drab."),
        slider("Casing Tint Hue", vr_gadget_tint_hue, 0.f, 355.f, 5.f, "%.0f"),
        header("Screen"),
        toggle("Level and Stats", vr_gadget_show_level),
    };
}

[[nodiscard]] std::vector<Item> pageThrowing()
{
    return {
        slider("Throw Speed", vr_weapon_throw_velocity_mult, 0.5f, 3.f, 0.1f, "%.1fx"),
        slider("Two-Hand Throw Speed", vr_2h_throw_velocity_mult, 0.5f, 3.f, 0.1f, "%.1fx"),
        cycle("Throw Gravity", vr_throw_gravity, {{9.81f, "Real"}, {0.f, "Quake"}}),
        slider("Velocity Window", vr_throw_window, 0.04f, 0.3f, 0.01f, "%.2f s")
            .help("Around the release, where the hand's fastest moment sets the throw."),
        slider("Direction Lookback", vr_throw_dir_lookback, 0.f, 0.1f, 0.005f, "%.3f s")
            .help("How far back from that moment the throw's direction is averaged."),
        slider("Lever Arm", vr_throw_lever_arm, 0.f, 0.3f, 0.01f, "%.2f m")
            .help("From the palm to the held object's centre: wrist flicks add speed through it."),
        cycle("Analog Release", vr_throw_release, {{0.f, "Off"}, {1.f, "On"}})
            .help("A throw lets go as the grip starts to open, not only once it is released."),
        slider("Max Speed Gain", vr_throw_gain_max, 1.f, 3.f, 0.05f, "%.2fx")
            .help("Extra speed for fast throws, which feel weak at true speed."),
        cycle("Aim Assist", vr_throw_assist, {{0.f, "Off"}, {1.f, "On"}})
            .help("Throws close to an enemy's direction bend towards it."),
        slider("Assist Cone", vr_throw_assist_cone, 2.f, 30.f, 1.f, "%.0f deg"),
        slider("Assist Strength", vr_throw_assist_strength, 0.f, 1.f, 0.05f, "%.2f"),
        header("Physics"),
        slider("Bounciness", vr_throw_restitution, 0.f, 0.8f, 0.05f, "%.2f"),
        slider("Friction", vr_throw_friction, 0.f, 1.5f, 0.05f, "%.2f"),
        slider("Max Spin", vr_throw_spin_max, 0.f, 40.f, 1.f, "%.0f rad/s"),
        slider("Spin Drag", vr_throw_spin_drag, 0.f, 2.f, 0.05f, "%.2f"),
        slider("Hitbox", vr_throw_hitbox, 1.f, 12.f, 0.5f, "%.1f").help("Half-size of a thrown weapon's box against monsters."),
    };
}

[[nodiscard]] std::vector<Item> pageForceGrab()
{
    return {
        toggle("Force Grab", vr_forcegrab_mode)
            .help("Point an empty hand at an object, pull the trigger, flick the hand: it flies to you. Grip as it arrives "
                  "to catch it."),
        slider("Distance", vr_forcegrab_distance, 100.f, 1500.f, 25.f, "%.0f"),
        slider("Aim Cone", vr_forcegrab_cone, 3.f, 45.f, 1.f, "%.0f deg").help("How far off where the hand points an object may be."),
        slider("Flick Speed", vr_forcegrab_flick_speed, 0.3f, 3.f, 0.1f, "%.1f m/s")
            .help("How fast the hand moves back or up to pull."),
        slider("Flick Turn", vr_forcegrab_flick_turn, 50.f, 800.f, 25.f, "%.0f deg/s")
            .help("Or how fast the fingers swing back or up."),
        slider("Flight Time", vr_forcegrab_time, 0.15f, 1.f, 0.05f, "%.2f s"),
        slider("Flight Speed", vr_forcegrab_speed, 300.f, 3000.f, 100.f, "%.0f")
            .help("Units per extra second of flight: longer pulls fly longer."),
        slider("Arc Height", vr_forcegrab_arc, 0.f, 0.5f, 0.05f, "%.2f"),
        slider("Catch Radius", vr_forcegrab_catch_radius, 4.f, 32.f, 1.f, "%.0f"),
        slider("Catch Early", vr_forcegrab_catch_early, 0.05f, 1.f, 0.05f, "%.2f s")
            .help("How long before it arrives the grip may close to catch it."),
        slider("Catch Late", vr_forcegrab_catch_late, 0.f, 0.5f, 0.05f, "%.2f s"),
        toggle("Pointing Particles", vr_forcegrab_eligible_particles),
        toggle("Pointing Haptics", vr_forcegrab_eligible_haptics),
        slider("Ammo/Health Box Size", vr_forcegrabbable_box_scale, 0.1f, 1.f, 0.05f, "%.2f")
            .help("Takes effect on the next map."),
    };
}

// ----------------------------------------------------------------------------
// Pages
// ----------------------------------------------------------------------------

enum PageId
{
    PageMain,
    PageAdvanced,
    PageFirstAdvanced
};

struct Page
{
    const char* title;
    std::vector<Item> (*build)();
};

[[nodiscard]] std::vector<Item> pageMain();
[[nodiscard]] std::vector<Item> pageAdvanced();

const Page pages[] = {
    {"VR Settings", pageMain},
    {"Advanced VR Options", pageAdvanced},
    {"Body", pageBody},
    {"Wrist Gadget", pageGadget},
    {"Throwing and Physics", pageThrowing},
    {"Force Grab", pageForceGrab},
    {"Menu", pageMenuSettings},
    {"Crosshair", pageCrosshairSettings},
    {"Particles", pageParticleSettings},
    {"Locomotion", pageLocomotionSettings},
    {"Hand/Gun Calibration", pageHandGunCalibration},
    {"Player Calibration", pagePlayerCalibration},
    {"Melee", pageMeleeSettings},
    {"Aiming", pageAimingSettings},
    {"Immersion", pageImmersionSettings},
    {"Graphics", pageGraphicalSettings},
    {"Status Bar", pageHudConfiguration},
    {"Hotspots", pageHotspotSettings},
    {"Transparency", pageTransparencyOptions},
};
constexpr int pageCount = static_cast<int>(sizeof(pages) / sizeof(pages[0]));

std::vector<Item> pageMain()
{
    return {
        header("Comfort"),
        cycle("Turning", vr_snap_turn, {{0.f, "Smooth"}, {30.f, "Snap 30"}, {45.f, "Snap 45"}, {90.f, "Snap 90"}}),
        slider("Turn Speed", vr_turn_speed, 1.f, 8.f, 0.25f, "%.2f"),
        cycle("Move Towards", vr_movement_mode, {{1.f, "Head"}, {0.f, "Off hand"}}),
        cycle("Default Speed", "cl_alwaysrun", {{1.f, "Run"}, {0.f, "Walk"}}).help("The speed button switches to the other."),
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
        toggle("Force Grab", vr_forcegrab_mode),
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
        cycle("Body", vr_body_mode, {{0.f, "Off"}, {2.f, "Torso and arms"}, {3.f, "Full body"}}),
        cycle("Build", vr_body_build, {{0.f, "Lean"}, {1.f, "Athletic"}, {2.f, "Brawny"}}),
        slider("Torso Offset", vr_body_torso_back, -0.15f, 0.3f, 0.01f, "%.2f m back"),
        slider("Legs Offset", vr_body_legs_back, -0.15f, 0.3f, 0.01f, "%.2f m back"),
        toggle("Holster Models", vr_leg_holster_model_enabled),

        header("Headset"),
        toggle("VR", vr_enabled),
        action("Restart VR", restartVr),

        header("More"),
        open("Advanced VR Options", PageAdvanced),
    };
}

std::vector<Item> pageAdvanced()
{
    std::vector<Item> list{header("The port's own")};
    for(int p = PageFirstAdvanced; p < pageCount; p++)
    {
        if(pages[p].build == pageMenuSettings)
        {
            list.push_back(header("Quake VR's"));
        }
        list.push_back(open(pages[p].title, p));
    }
    return list;
}

// Built on first use (cvars looked up by name exist by then); items without their cvar dropped.
[[nodiscard]] const std::vector<Item>& items(int page)
{
    static std::vector<Item> built[pageCount];
    static bool done[pageCount]{};
    if(!done[page])
    {
        done[page] = true;
        for(Item& item : pages[page].build())
        {
            if(item.kind == Item::Header || item.kind == Item::Action || item.cvar)
            {
                built[page].push_back(std::move(item));
            }
        }
    }
    return built[page];
}

int page = PageMain;
int parentPage[pageCount]{};
int cursors[pageCount]{};
int scrolls[pageCount]{};

constexpr int listTop = 36;
constexpr int helpTop = 164; // four lines of help under the list, on pages with any
constexpr int midPos = 204;  // as Ironwail's OPTIONS_MIDPOS

[[nodiscard]] bool hasHelp(const std::vector<Item>& list)
{
    for(const Item& item : list)
    {
        if(item.helpText)
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] int visibleRows(const std::vector<Item>& list)
{
    return ((hasHelp(list) ? helpTop - 4 : 200 - 8) - listTop) / 8;
}

[[nodiscard]] int firstSelectable(const std::vector<Item>& list)
{
    for(int i = 0; i < static_cast<int>(list.size()); i++)
    {
        if(list[i].kind != Item::Header)
        {
            return i;
        }
    }
    return 0;
}

void moveCursor(const std::vector<Item>& list, int dir)
{
    const int n = static_cast<int>(list.size());
    int& cursor = cursors[page];
    int i = cursor;
    do
    {
        i = (i + dir + n) % n;
    } while(list[i].kind == Item::Header && i != cursor);
    cursor = i;
}

void openPage(int target)
{
    parentPage[target] = page;
    page = target;
    const auto& list = items(page);
    if(list[cursors[page]].kind == Item::Header)
    {
        cursors[page] = firstSelectable(list);
    }
    S_LocalSound("misc/menu2.wav");
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
                if(item.page >= 0)
                {
                    openPage(item.page);
                    return;
                }
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

// Word-wrapped to the screen's width, four lines at most.
void drawHelp(const char* text)
{
    constexpr int columns = 38;
    constexpr int maxLines = 4;
    int line = 0;
    const char* p = text;
    while(*p && line < maxLines)
    {
        while(*p == ' ')
        {
            p++;
        }
        int n = static_cast<int>(strlen(p));
        if(n > columns)
        {
            n = columns;
            while(n > 0 && p[n] != ' ')
            {
                n--;
            }
            if(n == 0)
            {
                n = columns;
            }
        }
        char buf[columns + 1];
        memcpy(buf, p, n);
        buf[n] = '\0';
        M_PrintWhite((320 - 8 * n) / 2, helpTop + line * 8, buf);
        p += n;
        line++;
    }
}

} // namespace

extern "C" void VR_Menu_Open()
{
    IN_DeactivateForMenu();
    key_dest = key_menu;
    m_state = m_vr;
    m_entersound = true;
    page = PageMain;
    const auto& list = items(page);
    if(list[cursors[page]].kind == Item::Header)
    {
        cursors[page] = firstSelectable(list);
    }
}

// menu_vr [page]: the VR Settings, or one of its pages (1: Advanced VR Options).
void qvr::menu::command_f()
{
    VR_Menu_Open();
    if(Cmd_Argc() > 1)
    {
        const int target = Q_atoi(Cmd_Argv(1));
        if(target > PageMain && target < pageCount)
        {
            parentPage[PageAdvanced] = PageMain;
            page = PageAdvanced;
            if(target != PageAdvanced)
            {
                openPage(target);
            }
        }
    }
}

extern "C" void VR_Menu_Draw()
{
    const auto& list = items(page);
    int& cursor = cursors[page];
    int& scroll = scrolls[page];

    M_DrawTransPic(16, 4, Draw_CachePic("gfx/qplaque.lmp"));
    qpic_t* title = Draw_CachePic("gfx/p_option.lmp");
    M_DrawPic((320 - title->width) / 2, 4, title);
    const char* name = pages[page].title;
    M_PrintWhite((320 - 8 * static_cast<int>(strlen(name))) / 2, 28, name);

    const int rows = visibleRows(list);
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

    if(cursor < n && list[cursor].helpText)
    {
        drawHelp(list[cursor].helpText);
    }
}

extern "C" void VR_Menu_Key(int key)
{
    const auto& list = items(page);
    const int cursor = cursors[page];

    switch(key)
    {
        case K_ESCAPE:
        case K_BBUTTON:
        case K_MOUSE2:
        case K_MOUSE4:
            if(page == PageMain)
            {
                M_Menu_Options_f();
            }
            else
            {
                page = parentPage[page];
                S_LocalSound("misc/menu2.wav");
            }
            break;

        case K_UPARROW:
        case K_MWHEELUP:
            S_LocalSound("misc/menu1.wav");
            moveCursor(list, -1);
            break;

        case K_DOWNARROW:
        case K_MWHEELDOWN:
            S_LocalSound("misc/menu1.wav");
            moveCursor(list, 1);
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
