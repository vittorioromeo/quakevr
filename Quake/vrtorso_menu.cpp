#include "quakedef.hpp"
#include "vr.hpp"
#include "vrtorso_menu.hpp"
#include "menu_util.hpp"

[[nodiscard]] static quake::menu make_menu()
{
    constexpr quake::menu_bounds<float> vrtOBounds{0.5f, -100.f, 100.f};
    constexpr quake::menu_bounds<float> vrtMBounds{1.f, 0.f, 250.f};
    constexpr quake::menu_bounds<float> vrtSBounds{0.05f, 0.1f, 2.f};
    constexpr quake::menu_bounds<float> vrtRBounds{1.f, -180.f, 180.f};

    quake::menu m{"VR Torso Settings"};

    // ------------------------------------------------------------------------

    m.add_cvar_entry<bool>("Show VR Torso", vr_vrtorso_enabled);

    // ------------------------------------------------------------------------

    m.add_separator();

    // ------------------------------------------------------------------------

    m.add_cvar_entry<float>("VR Torso X", vr_vrtorso_x_offset, vrtOBounds);
    m.add_cvar_entry<float>("VR Torso Y", vr_vrtorso_y_offset, vrtOBounds);
    m.add_cvar_entry<float>("VR Torso Z", vr_vrtorso_z_offset, vrtOBounds);
    m.add_cvar_entry<float>(
        "VR Torso Head Z Mult", vr_vrtorso_head_z_mult, vrtMBounds);

    // ------------------------------------------------------------------------

    m.add_separator();

    // ------------------------------------------------------------------------

    m.add_cvar_entry<float>("VR Torso Scale X", vr_vrtorso_x_scale, vrtSBounds);
    m.add_cvar_entry<float>("VR Torso Scale Y", vr_vrtorso_y_scale, vrtSBounds);
    m.add_cvar_entry<float>("VR Torso Scale Z", vr_vrtorso_z_scale, vrtSBounds);

    // ------------------------------------------------------------------------

    m.add_separator();

    // ------------------------------------------------------------------------

    m.add_cvar_entry<float>("VR Torso Pitch", vr_vrtorso_pitch, vrtRBounds);
    m.add_cvar_entry<float>("VR Torso Yaw", vr_vrtorso_yaw, vrtRBounds);
    m.add_cvar_entry<float>("VR Torso Roll", vr_vrtorso_roll, vrtRBounds);

    // TODO VR: (P1) menu tooltips

    // ------------------------------------------------------------------------

    m.add_separator();

    m.add_cvar_entry<bool>("Show Holster Slots", vr_leg_holster_model_enabled);
    m.add_cvar_entry<float>(
        "Holster Slot Scale", vr_leg_holster_model_scale, vrtSBounds);

    m.add_cvar_entry<float>(
        "Holster Slot X", vr_leg_holster_model_x_offset, vrtOBounds);
    m.add_cvar_entry<float>(
        "Holster Slot Y", vr_leg_holster_model_y_offset, vrtOBounds);
    m.add_cvar_entry<float>(
        "Holster Slot Z", vr_leg_holster_model_z_offset, vrtOBounds);

    return m;
}

static quake::menu g_menu = make_menu();

quake::menu& M_VRTorso_Menu()
{
    return g_menu;
}

void M_VRTorso_Key(int key)
{
    g_menu.key(key);

    // TODO VR: (P2) hackish
    VR_ModVRTorsoModel();
    VR_ModVRLegHolsterModel();
}

void M_VRTorso_Draw()
{
    g_menu.draw();
}
