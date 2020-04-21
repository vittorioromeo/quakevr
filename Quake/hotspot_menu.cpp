#include "quakedef.hpp"
#include "vr.hpp"
#include "hotspot_menu.hpp"
#include "cmd.hpp"
#include "util.hpp"
#include "menu_util.hpp"

#include <string>
#include <cassert>
#include <array>

[[nodiscard]] static quake::menu make_menu()
{
    constexpr quake::menu_bounds<float> bPos{0.5f, -50.f, 50.f};
    constexpr quake::menu_bounds<float> bDst{0.1f, 0.f, 30.f};

    quake::menu m{"VR Hotspot Customization", &M_Menu_QuakeVRSettings_f};

    // ------------------------------------------------------------------------

    m.add_cvar_getter_enum_entry<int>(
        "Show Shoulder (Virtual Stock)",
        [] { return &vr_show_virtual_stock; },       //
        "Off", "Main Hand", "Off Hand", "Both Hands" //
    );

    m.add_cvar_entry<float>("Shoulder X", vr_shoulder_offset_x, bPos);
    m.add_cvar_entry<float>("Shoulder Y", vr_shoulder_offset_y, bPos);
    m.add_cvar_entry<float>("Shoulder Z", vr_shoulder_offset_z, bPos);
    m.add_cvar_entry<float>(
        "Virtual Stock Threshold", vr_virtual_stock_thresh, bDst);

    // ------------------------------------------------------------------------

    m.add_separator();

    // ------------------------------------------------------------------------

    m.add_cvar_getter_enum_entry<VrOptionHandSelection>( //
        "Show Shoulder Holsters",                        //
        [] { return &vr_show_shoulder_holsters; },       //
        "Off", "Main Hand", "Off Hand", "Both Hands"     //
    );

    m.add_cvar_entry<float>("Shoulder X", vr_shoulder_holster_offset_x, bPos);
    m.add_cvar_entry<float>("Shoulder Y", vr_shoulder_holster_offset_y, bPos);
    m.add_cvar_entry<float>("Shoulder Z", vr_shoulder_holster_offset_z, bPos);
    m.add_cvar_entry<float>(
        "Shoulder Threshold", vr_shoulder_holster_thresh, bDst);

    // ------------------------------------------------------------------------

    m.add_separator();

    // ------------------------------------------------------------------------

    m.add_cvar_getter_enum_entry<VrOptionHandSelection>( //
        "Show Hip Holsters",                             //
        [] { return &vr_show_hip_holsters; },            //
        "Off", "Main Hand", "Off Hand", "Both Hands"     //
    );

    m.add_cvar_entry<float>("Hip X", vr_hip_offset_x, bPos);
    m.add_cvar_entry<float>("Hip Y", vr_hip_offset_y, bPos);
    m.add_cvar_entry<float>("Hip Z", vr_hip_offset_z, bPos);
    m.add_cvar_entry<float>("Hip Threshold", vr_hip_holster_thresh, bDst);

    // ------------------------------------------------------------------------

    m.add_separator();

    // ------------------------------------------------------------------------

    m.add_cvar_getter_enum_entry<VrOptionHandSelection>( //
        "Show Upper Holsters",                           //
        [] { return &vr_show_upper_holsters; },          //
        "Off", "Main Hand", "Off Hand", "Both Hands"     //
    );

    m.add_cvar_entry<float>("Upper X", vr_upper_holster_offset_x, bPos);
    m.add_cvar_entry<float>("Upper Y", vr_upper_holster_offset_y, bPos);
    m.add_cvar_entry<float>("Upper Z", vr_upper_holster_offset_z, bPos);
    m.add_cvar_entry<float>("Upper Threshold", vr_upper_holster_thresh, bDst);

    // TODO VR: (P1) menu tooltips

    // ------------------------------------------------------------------------

    return m;
}

static quake::menu g_menu = make_menu();

quake::menu& M_Hotspot_Menu()
{
    return g_menu;
}

void M_Hotspot_Key(int key)
{
    g_menu.key(key);
}

void M_Hotspot_Draw()
{
    g_menu.draw();
}
