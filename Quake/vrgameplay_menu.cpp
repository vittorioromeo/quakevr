#include "quakedef.hpp"
#include "vr.hpp"
#include "vrgameplay_menu.hpp"
#include "cmd.hpp"
#include "util.hpp"
#include "menu_util.hpp"

#include <string>
#include <cassert>
#include <array>

[[nodiscard]] static quake::menu make_menu()
{
    quake::menu m{"VR Gameplay Options"};

    // ------------------------------------------------------------------------

    m.add_cvar_entry<bool>("Positional Damage", vr_positional_damage);

    m.add_cvar_entry<float>(
        "Melee Threshold", vr_melee_threshold, {0.5f, 4.f, 18.f});
    m.add_cvar_entry<bool>("Roomscale Jump", vr_roomscale_jump);
    m.add_cvar_entry<float>("Roomscale Jump Threshold",
        vr_roomscale_jump_threshold, {0.05f, 0.05f, 3.f});

    m.add_action_entry("Calibrate Height", &VR_CalibrateHeight);

    m.add_cvar_entry<float>("Melee Damage Multiplier", vr_melee_dmg_multiplier,
        {0.25f, 0.25f, 15.f});
    m.add_cvar_entry<float>("Melee Range Multiplier", vr_melee_range_multiplier,
        {0.25f, 0.25f, 15.f});
    m.add_cvar_entry<bool>("Body-Item Interactions", vr_body_interactions);
    m.add_cvar_entry<int>("Movement Speed", cl_forwardspeed, {25, 100, 400});
    m.add_cvar_entry<float>(
        "Speed Button Multiplier", cl_movespeedkey, {0.05f, 0.1f, 1.f});
    m.add_cvar_entry<float>(
        "Room-Scale Move Mult.", vr_roomscale_move_mult, {0.25f, 0.25f, 5.f});
    m.add_cvar_entry<bool>("Teleportation", vr_teleport_enabled);
    m.add_cvar_entry<float>(
        "Teleport Range", vr_teleport_range, {10.f, 100.f, 800.f});

    m.add_cvar_getter_enum_entry<Vr2HMode>(  //
        "2H Aiming",                         //
        [] { return &vr_2h_mode; },          //
        "Disabled", "Basic", "Virtual Stock" //
    );

    m.add_cvar_entry<float>(
        "2H Aiming Threshold", vr_2h_angle_threshold, {0.05f, -1.f, 1.f});

    m.add_cvar_entry<float>("2H Virtual Stock Factor",
        vr_2h_virtual_stock_factor, {0.05f, 0.f, 1.f});
    m.add_cvar_entry<bool>("Weighted Weapon Move", vr_wpn_pos_weight);
    m.add_cvar_entry<float>(
        "W. Weapon Move Off.", vr_wpn_pos_weight_offset, {0.05f, 0.05f, 1.f});
    m.add_cvar_entry<float>(
        "W. Weapon Move Mult", vr_wpn_pos_weight_mult, {0.1f, -5.f, 5.f});
    m.add_cvar_entry<float>("W. W. Move 2H Help Off.",
        vr_wpn_pos_weight_2h_help_offset, {0.05f, 0.05f, 1.f});
    m.add_cvar_entry<float>("W. W. Move 2H Help Mult",
        vr_wpn_pos_weight_2h_help_mult, {0.1f, -5.f, 5.f});
    m.add_cvar_entry<bool>("Weighted Weapon Turn", vr_wpn_dir_weight);
    m.add_cvar_entry<float>(
        "W. Weapon Turn Off.", vr_wpn_dir_weight_offset, {0.05f, 0.05f, 1.f});
    m.add_cvar_entry<float>(
        "W. Weapon Turn Mult", vr_wpn_dir_weight_mult, {0.1f, -5.f, 5.f});
    m.add_cvar_entry<float>("W. W. Turn 2H Help Off.",
        vr_wpn_dir_weight_2h_help_offset, {0.05f, 0.05f, 1.f});
    m.add_cvar_entry<float>("W. W. Turn 2H Help Mult",
        vr_wpn_dir_weight_2h_help_mult, {0.1f, -5.f, 5.f});

    // TODO VR: (P1) menu tooltips

    // ------------------------------------------------------------------------

    return m;
}

static quake::menu g_menu = make_menu();

quake::menu& M_VRGameplay_Menu()
{
    return g_menu;
}

void M_VRGameplay_Key(int key)
{
    g_menu.key(key);
}

void M_VRGameplay_Draw()
{
    g_menu.draw();
}
