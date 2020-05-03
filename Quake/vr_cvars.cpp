#include "vr_cvars.hpp"

#include <vector>

//
//
//
// ----------------------------------------------------------------------------
// CVar definition and registration macros
// ----------------------------------------------------------------------------

static std::vector<cvar_t*> cvarsToRegister;

#define DEFINE_CVAR(name, defaultValue, type)         \
    cvar_t name = {#name, #defaultValue, type};       \
    static struct _cvar_registrar##name##__LINE__##_t \
    {                                                 \
        _cvar_registrar##name##__LINE__##_t()         \
        {                                             \
            cvarsToRegister.emplace_back(&name);      \
        }                                             \
    } _cvar_registrar##name##__LINE__

#define DEFINE_CVAR_ARCHIVE(name, defaultValue) \
    DEFINE_CVAR(name, defaultValue, CVAR_ARCHIVE);

//
//
//
// ----------------------------------------------------------------------------
// Non-archived CVars
// ----------------------------------------------------------------------------

DEFINE_CVAR(vr_enabled, 0, CVAR_NONE);
DEFINE_CVAR(vr_viewkick, 0, CVAR_NONE);
DEFINE_CVAR(vr_lefthanded, 0, CVAR_NONE);
DEFINE_CVAR(vr_fakevr, 1, CVAR_NONE);
DEFINE_CVAR(vr_novrinit, 1, CVAR_NONE);

//
//
//
// ----------------------------------------------------------------------------
// Archived CVars
// ----------------------------------------------------------------------------

DEFINE_CVAR_ARCHIVE(vr_crosshair, 1);
DEFINE_CVAR_ARCHIVE(vr_crosshair_depth, 0);
DEFINE_CVAR_ARCHIVE(vr_crosshair_size, 3.0);
DEFINE_CVAR_ARCHIVE(vr_crosshair_alpha, 0.25);
DEFINE_CVAR_ARCHIVE(vr_aimmode, 7);
DEFINE_CVAR_ARCHIVE(vr_deadzone, 30);
DEFINE_CVAR_ARCHIVE(vr_gunangle, 32);
DEFINE_CVAR_ARCHIVE(vr_gunmodelpitch, 0);
DEFINE_CVAR_ARCHIVE(vr_gunmodelscale, 1.0);
DEFINE_CVAR_ARCHIVE(vr_gunmodely, 0);
DEFINE_CVAR_ARCHIVE(vr_crosshairy, 0);
DEFINE_CVAR_ARCHIVE(vr_world_scale, 1.0);
DEFINE_CVAR_ARCHIVE(vr_floor_offset, -16);
DEFINE_CVAR_ARCHIVE(vr_snap_turn, 0);
DEFINE_CVAR_ARCHIVE(vr_enable_joystick_turn, 1);
DEFINE_CVAR_ARCHIVE(vr_turn_speed, 1);
DEFINE_CVAR_ARCHIVE(vr_msaa, 4);
DEFINE_CVAR_ARCHIVE(vr_movement_mode, 0);
DEFINE_CVAR_ARCHIVE(vr_hud_scale, 0.025);
DEFINE_CVAR_ARCHIVE(vr_menu_scale, 0.13);
DEFINE_CVAR_ARCHIVE(vr_melee_threshold, 7);
DEFINE_CVAR_ARCHIVE(vr_gunyaw, 0);
DEFINE_CVAR_ARCHIVE(vr_gun_z_offset, 0);
DEFINE_CVAR_ARCHIVE(vr_sbar_mode, 0);
DEFINE_CVAR_ARCHIVE(vr_sbar_offset_x, 0);
DEFINE_CVAR_ARCHIVE(vr_sbar_offset_y, 0);
DEFINE_CVAR_ARCHIVE(vr_sbar_offset_z, 0);
DEFINE_CVAR_ARCHIVE(vr_sbar_offset_pitch, 0);
DEFINE_CVAR_ARCHIVE(vr_sbar_offset_yaw, 0);
DEFINE_CVAR_ARCHIVE(vr_sbar_offset_roll, 0);
DEFINE_CVAR_ARCHIVE(vr_roomscale_jump, 1);
DEFINE_CVAR_ARCHIVE(vr_height_calibration, 1.6);
DEFINE_CVAR_ARCHIVE(vr_roomscale_jump_threshold, 1.0);
DEFINE_CVAR_ARCHIVE(vr_menu_distance, 76);
DEFINE_CVAR_ARCHIVE(vr_melee_dmg_multiplier, 1.0);
DEFINE_CVAR_ARCHIVE(vr_melee_range_multiplier, 1.0);
DEFINE_CVAR_ARCHIVE(vr_body_interactions, 0);
DEFINE_CVAR_ARCHIVE(vr_roomscale_move_mult, 1.0);
DEFINE_CVAR_ARCHIVE(vr_teleport_enabled, 1);
DEFINE_CVAR_ARCHIVE(vr_teleport_range, 400);
DEFINE_CVAR_ARCHIVE(vr_2h_mode, 2);
DEFINE_CVAR_ARCHIVE(vr_2h_angle_threshold, 0.65);
DEFINE_CVAR_ARCHIVE(vr_virtual_stock_thresh, 10);
DEFINE_CVAR_ARCHIVE(vr_show_virtual_stock, 0);
DEFINE_CVAR_ARCHIVE(vr_shoulder_offset_x, -1.5);
DEFINE_CVAR_ARCHIVE(vr_shoulder_offset_y, 1.75);
DEFINE_CVAR_ARCHIVE(vr_shoulder_offset_z, 16.0);
DEFINE_CVAR_ARCHIVE(vr_2h_virtual_stock_factor, 0.5);
DEFINE_CVAR_ARCHIVE(vr_wpn_pos_weight, 1);
DEFINE_CVAR_ARCHIVE(vr_wpn_pos_weight_offset, 0.0);
DEFINE_CVAR_ARCHIVE(vr_wpn_pos_weight_mult, 1.0);
DEFINE_CVAR_ARCHIVE(vr_wpn_pos_weight_2h_help_offset, 0.3);
DEFINE_CVAR_ARCHIVE(vr_wpn_pos_weight_2h_help_mult, 1.0);
DEFINE_CVAR_ARCHIVE(vr_wpn_dir_weight, 1);
DEFINE_CVAR_ARCHIVE(vr_wpn_dir_weight_offset, 0.0);
DEFINE_CVAR_ARCHIVE(vr_wpn_dir_weight_mult, 1.0);
DEFINE_CVAR_ARCHIVE(vr_wpn_dir_weight_2h_help_offset, 0.3);
DEFINE_CVAR_ARCHIVE(vr_wpn_dir_weight_2h_help_mult, 1.0);
DEFINE_CVAR_ARCHIVE(vr_offhandpitch, 0.0);
DEFINE_CVAR_ARCHIVE(vr_offhandyaw, 0.0);
DEFINE_CVAR_ARCHIVE(vr_show_hip_holsters, 0);
DEFINE_CVAR_ARCHIVE(vr_hip_offset_x, -1.0);
DEFINE_CVAR_ARCHIVE(vr_hip_offset_y, 7.0);
DEFINE_CVAR_ARCHIVE(vr_hip_offset_z, 4.5);
DEFINE_CVAR_ARCHIVE(vr_hip_holster_thresh, 6.0);
DEFINE_CVAR_ARCHIVE(vr_show_shoulder_holsters, 0);
DEFINE_CVAR_ARCHIVE(vr_shoulder_holster_offset_x, 5.0);
DEFINE_CVAR_ARCHIVE(vr_shoulder_holster_offset_y, 1.5);
DEFINE_CVAR_ARCHIVE(vr_shoulder_holster_offset_z, 0.0);
DEFINE_CVAR_ARCHIVE(vr_shoulder_holster_thresh, 8.0);
DEFINE_CVAR_ARCHIVE(vr_show_upper_holsters, 0);
DEFINE_CVAR_ARCHIVE(vr_upper_holster_offset_x, 2.5);
DEFINE_CVAR_ARCHIVE(vr_upper_holster_offset_y, 6.5);
DEFINE_CVAR_ARCHIVE(vr_upper_holster_offset_z, 2.5);
DEFINE_CVAR_ARCHIVE(vr_upper_holster_thresh, 6.0);
DEFINE_CVAR_ARCHIVE(vr_vrtorso_debuglines_enabled, 0);
DEFINE_CVAR_ARCHIVE(vr_vrtorso_enabled, 1);
DEFINE_CVAR_ARCHIVE(vr_vrtorso_x_offset, -3.25);
DEFINE_CVAR_ARCHIVE(vr_vrtorso_y_offset, 0.0);
DEFINE_CVAR_ARCHIVE(vr_vrtorso_z_offset, -21.0);
DEFINE_CVAR_ARCHIVE(vr_vrtorso_head_z_mult, 32.0);
DEFINE_CVAR_ARCHIVE(vr_vrtorso_x_scale, 1.0);
DEFINE_CVAR_ARCHIVE(vr_vrtorso_y_scale, 1.0);
DEFINE_CVAR_ARCHIVE(vr_vrtorso_z_scale, 1.0);
DEFINE_CVAR_ARCHIVE(vr_vrtorso_pitch, 0.0);
DEFINE_CVAR_ARCHIVE(vr_vrtorso_yaw, 0.0);
DEFINE_CVAR_ARCHIVE(vr_vrtorso_roll, 0.0);
DEFINE_CVAR_ARCHIVE(vr_holster_haptics, 1);
DEFINE_CVAR_ARCHIVE(vr_player_shadows, 2);
DEFINE_CVAR_ARCHIVE(vr_positional_damage, 1);
DEFINE_CVAR_ARCHIVE(vr_debug_print_handvel, 0);
DEFINE_CVAR_ARCHIVE(vr_debug_show_hand_pos_and_rot, 0);
DEFINE_CVAR_ARCHIVE(vr_leg_holster_model_enabled, 1);
DEFINE_CVAR_ARCHIVE(vr_leg_holster_model_scale, 1);
DEFINE_CVAR_ARCHIVE(vr_leg_holster_model_x_offset, 0.0);
DEFINE_CVAR_ARCHIVE(vr_leg_holster_model_y_offset, 0.0);
DEFINE_CVAR_ARCHIVE(vr_leg_holster_model_z_offset, 0.0);
DEFINE_CVAR_ARCHIVE(vr_holster_mode, 0);
DEFINE_CVAR_ARCHIVE(vr_weapon_throw_mode, 0);
DEFINE_CVAR_ARCHIVE(vr_weapon_throw_damage_mult, 1.0);
DEFINE_CVAR_ARCHIVE(vr_weapon_throw_velocity_mult, 1.0);
DEFINE_CVAR_ARCHIVE(vr_weapon_cycle_mode, 0);
DEFINE_CVAR_ARCHIVE(vr_melee_bloodlust, 0);
DEFINE_CVAR_ARCHIVE(vr_melee_bloodlust_mult, 1.0);
DEFINE_CVAR_ARCHIVE(vr_enemy_drops, 0);
DEFINE_CVAR_ARCHIVE(vr_enemy_drops_chance_mult, 1.0);
DEFINE_CVAR_ARCHIVE(vr_ammobox_drops, 0);
DEFINE_CVAR_ARCHIVE(vr_ammobox_drops_chance_mult, 1.0);
DEFINE_CVAR_ARCHIVE(vr_menumode, 0);
DEFINE_CVAR_ARCHIVE(vr_forcegrab_powermult, 1.0);
DEFINE_CVAR_ARCHIVE(vr_forcegrab_mode, 1);
DEFINE_CVAR_ARCHIVE(vr_forcegrab_range, 150.0);
DEFINE_CVAR_ARCHIVE(vr_forcegrab_radius, 18.0);
DEFINE_CVAR_ARCHIVE(vr_forcegrab_eligible_particles, 1);
DEFINE_CVAR_ARCHIVE(vr_forcegrab_eligible_haptics, 1);
DEFINE_CVAR_ARCHIVE(vr_weapondrop_particles, 1);
DEFINE_CVAR_ARCHIVE(vr_openhand_offset_x, 0.0);
DEFINE_CVAR_ARCHIVE(vr_openhand_offset_y, 0.0);
DEFINE_CVAR_ARCHIVE(vr_openhand_offset_z, 0.0);
DEFINE_CVAR_ARCHIVE(vr_openhand_pitch, 0.0);
DEFINE_CVAR_ARCHIVE(vr_openhand_yaw, 0.0);
DEFINE_CVAR_ARCHIVE(vr_openhand_roll, 0.0);

#undef DEFINE_CVAR_ARCHIVE
#undef DEFINE_CVAR

//
//
//
// ----------------------------------------------------------------------------
// Registration
// ----------------------------------------------------------------------------

namespace quake::vr
{
    void register_all_cvars() noexcept
    {
        for(cvar_t* c : cvarsToRegister)
        {
            Cvar_RegisterVariable(c);
        }

        cvarsToRegister.clear();
        cvarsToRegister.shrink_to_fit();
    }
} // namespace quake::vr
