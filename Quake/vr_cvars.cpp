#include "vr_cvars.hpp"

#include <vector>

//
//
//
// ----------------------------------------------------------------------------
// CVar definition and registration macros
// ----------------------------------------------------------------------------

static std::vector<cvar_t*> cvarsToRegister;

#define DEFINE_FCVAR(name, defaultValue, type)        \
    cvar_t name = {#name, #defaultValue, type};       \
    static struct _cvar_registrar##name##__LINE__##_t \
    {                                                 \
        _cvar_registrar##name##__LINE__##_t()         \
        {                                             \
            cvarsToRegister.emplace_back(&name);      \
        }                                             \
    } _cvar_registrar##name##__LINE__

#define DEFINE_FCVAR_ARCHIVE(name, defaultValue) \
    DEFINE_FCVAR(name, defaultValue, CVAR_ARCHIVE);

//
//
//
// ----------------------------------------------------------------------------
// Non-archived CVars
// ----------------------------------------------------------------------------

DEFINE_FCVAR(vr_enabled, 0, CVAR_NONE);
DEFINE_FCVAR(vr_viewkick, 0, CVAR_NONE);
DEFINE_FCVAR(vr_lefthanded, 0, CVAR_NONE);
DEFINE_FCVAR(vr_fakevr, 0, CVAR_NONE);
DEFINE_FCVAR(vr_novrinit, 0, CVAR_NONE);

//
//
//
// ----------------------------------------------------------------------------
// Archived CVars
// ----------------------------------------------------------------------------

DEFINE_FCVAR_ARCHIVE(vr_crosshair, 1);
DEFINE_FCVAR_ARCHIVE(vr_crosshair_depth, 0);
DEFINE_FCVAR_ARCHIVE(vr_crosshair_size, 3.0);
DEFINE_FCVAR_ARCHIVE(vr_crosshair_alpha, 0.25);
DEFINE_FCVAR_ARCHIVE(vr_aimmode, 7);
DEFINE_FCVAR_ARCHIVE(vr_deadzone, 30);
DEFINE_FCVAR_ARCHIVE(vr_gunangle, 32);
DEFINE_FCVAR_ARCHIVE(vr_gunmodelpitch, 0);
DEFINE_FCVAR_ARCHIVE(vr_gunmodelscale, 1.0);
DEFINE_FCVAR_ARCHIVE(vr_gunmodely, 0);
DEFINE_FCVAR_ARCHIVE(vr_crosshairy, 0);
DEFINE_FCVAR_ARCHIVE(vr_world_scale, 1.0);
DEFINE_FCVAR_ARCHIVE(vr_floor_offset, -16);
DEFINE_FCVAR_ARCHIVE(vr_snap_turn, 0);
DEFINE_FCVAR_ARCHIVE(vr_enable_joystick_turn, 1);
DEFINE_FCVAR_ARCHIVE(vr_turn_speed, 1);
DEFINE_FCVAR_ARCHIVE(vr_msaa, 4);
DEFINE_FCVAR_ARCHIVE(vr_movement_mode, 0);
DEFINE_FCVAR_ARCHIVE(vr_hud_scale, 0.025);
DEFINE_FCVAR_ARCHIVE(vr_menu_scale, 0.13);
DEFINE_FCVAR_ARCHIVE(vr_melee_threshold, 7);
DEFINE_FCVAR_ARCHIVE(vr_gunyaw, 0);
DEFINE_FCVAR_ARCHIVE(vr_gun_z_offset, 0);
DEFINE_FCVAR_ARCHIVE(vr_sbar_mode, 0);
DEFINE_FCVAR_ARCHIVE(vr_sbar_offset_x, 0);
DEFINE_FCVAR_ARCHIVE(vr_sbar_offset_y, 0);
DEFINE_FCVAR_ARCHIVE(vr_sbar_offset_z, 0);
DEFINE_FCVAR_ARCHIVE(vr_sbar_offset_pitch, 0);
DEFINE_FCVAR_ARCHIVE(vr_sbar_offset_yaw, 0);
DEFINE_FCVAR_ARCHIVE(vr_sbar_offset_roll, 0);
DEFINE_FCVAR_ARCHIVE(vr_roomscale_jump, 1);
DEFINE_FCVAR_ARCHIVE(vr_height_calibration, 1.6);
DEFINE_FCVAR_ARCHIVE(vr_roomscale_jump_threshold, 1.0);
DEFINE_FCVAR_ARCHIVE(vr_menu_distance, 76);
DEFINE_FCVAR_ARCHIVE(vr_melee_dmg_multiplier, 1.0);
DEFINE_FCVAR_ARCHIVE(vr_melee_range_multiplier, 1.0);
DEFINE_FCVAR_ARCHIVE(vr_body_interactions, 0);
DEFINE_FCVAR_ARCHIVE(vr_roomscale_move_mult, 1.0);
DEFINE_FCVAR_ARCHIVE(vr_teleport_enabled, 1);
DEFINE_FCVAR_ARCHIVE(vr_teleport_range, 400);
DEFINE_FCVAR_ARCHIVE(vr_2h_mode, 2);
DEFINE_FCVAR_ARCHIVE(vr_2h_angle_threshold, 0.65);
DEFINE_FCVAR_ARCHIVE(vr_virtual_stock_thresh, 10);
DEFINE_FCVAR_ARCHIVE(vr_show_virtual_stock, 0);
DEFINE_FCVAR_ARCHIVE(vr_shoulder_offset_x, -1.5);
DEFINE_FCVAR_ARCHIVE(vr_shoulder_offset_y, 1.75);
DEFINE_FCVAR_ARCHIVE(vr_shoulder_offset_z, 16.0);
DEFINE_FCVAR_ARCHIVE(vr_2h_virtual_stock_factor, 0.5);
DEFINE_FCVAR_ARCHIVE(vr_wpn_pos_weight, 1);
DEFINE_FCVAR_ARCHIVE(vr_wpn_pos_weight_offset, 0.0);
DEFINE_FCVAR_ARCHIVE(vr_wpn_pos_weight_mult, 1.0);
DEFINE_FCVAR_ARCHIVE(vr_wpn_pos_weight_2h_help_offset, 0.3);
DEFINE_FCVAR_ARCHIVE(vr_wpn_pos_weight_2h_help_mult, 1.0);
DEFINE_FCVAR_ARCHIVE(vr_wpn_dir_weight, 1);
DEFINE_FCVAR_ARCHIVE(vr_wpn_dir_weight_offset, 0.0);
DEFINE_FCVAR_ARCHIVE(vr_wpn_dir_weight_mult, 1.0);
DEFINE_FCVAR_ARCHIVE(vr_wpn_dir_weight_2h_help_offset, 0.3);
DEFINE_FCVAR_ARCHIVE(vr_wpn_dir_weight_2h_help_mult, 1.0);
DEFINE_FCVAR_ARCHIVE(vr_offhandpitch, 0.0);
DEFINE_FCVAR_ARCHIVE(vr_offhandyaw, 0.0);
DEFINE_FCVAR_ARCHIVE(vr_show_hip_holsters, 0);
DEFINE_FCVAR_ARCHIVE(vr_hip_offset_x, -1.0);
DEFINE_FCVAR_ARCHIVE(vr_hip_offset_y, 7.0);
DEFINE_FCVAR_ARCHIVE(vr_hip_offset_z, 4.5);
DEFINE_FCVAR_ARCHIVE(vr_hip_holster_thresh, 6.0);
DEFINE_FCVAR_ARCHIVE(vr_show_shoulder_holsters, 0);
DEFINE_FCVAR_ARCHIVE(vr_shoulder_holster_offset_x, 5.0);
DEFINE_FCVAR_ARCHIVE(vr_shoulder_holster_offset_y, 1.5);
DEFINE_FCVAR_ARCHIVE(vr_shoulder_holster_offset_z, 0.0);
DEFINE_FCVAR_ARCHIVE(vr_shoulder_holster_thresh, 8.0);
DEFINE_FCVAR_ARCHIVE(vr_show_upper_holsters, 0);
DEFINE_FCVAR_ARCHIVE(vr_upper_holster_offset_x, 2.5);
DEFINE_FCVAR_ARCHIVE(vr_upper_holster_offset_y, 6.5);
DEFINE_FCVAR_ARCHIVE(vr_upper_holster_offset_z, 2.5);
DEFINE_FCVAR_ARCHIVE(vr_upper_holster_thresh, 6.0);
DEFINE_FCVAR_ARCHIVE(vr_vrtorso_debuglines_enabled, 0);
DEFINE_FCVAR_ARCHIVE(vr_vrtorso_enabled, 1);
DEFINE_FCVAR_ARCHIVE(vr_vrtorso_x_offset, -3.25);
DEFINE_FCVAR_ARCHIVE(vr_vrtorso_y_offset, 0.0);
DEFINE_FCVAR_ARCHIVE(vr_vrtorso_z_offset, -21.0);
DEFINE_FCVAR_ARCHIVE(vr_vrtorso_head_z_mult, 32.0);
DEFINE_FCVAR_ARCHIVE(vr_vrtorso_x_scale, 1.0);
DEFINE_FCVAR_ARCHIVE(vr_vrtorso_y_scale, 1.0);
DEFINE_FCVAR_ARCHIVE(vr_vrtorso_z_scale, 1.0);
DEFINE_FCVAR_ARCHIVE(vr_vrtorso_pitch, 0.0);
DEFINE_FCVAR_ARCHIVE(vr_vrtorso_yaw, 0.0);
DEFINE_FCVAR_ARCHIVE(vr_vrtorso_roll, 0.0);
DEFINE_FCVAR_ARCHIVE(vr_holster_haptics, 1);
DEFINE_FCVAR_ARCHIVE(vr_player_shadows, 2);
DEFINE_FCVAR_ARCHIVE(vr_positional_damage, 1);
DEFINE_FCVAR_ARCHIVE(vr_debug_print_handvel, 0);
DEFINE_FCVAR_ARCHIVE(vr_debug_print_headvel, 0);
DEFINE_FCVAR_ARCHIVE(vr_debug_show_hand_pos_and_rot, 0);
DEFINE_FCVAR_ARCHIVE(vr_leg_holster_model_enabled, 1);
DEFINE_FCVAR_ARCHIVE(vr_leg_holster_model_scale, 1);
DEFINE_FCVAR_ARCHIVE(vr_leg_holster_model_x_offset, 0.0);
DEFINE_FCVAR_ARCHIVE(vr_leg_holster_model_y_offset, 0.0);
DEFINE_FCVAR_ARCHIVE(vr_leg_holster_model_z_offset, 0.0);
DEFINE_FCVAR_ARCHIVE(vr_holster_mode, 0);
DEFINE_FCVAR_ARCHIVE(vr_weapon_throw_mode, 0);
DEFINE_FCVAR_ARCHIVE(vr_weapon_throw_damage_mult, 1.0);
DEFINE_FCVAR_ARCHIVE(vr_weapon_throw_velocity_mult, 1.0);
DEFINE_FCVAR_ARCHIVE(vr_weapon_cycle_mode, 0);
DEFINE_FCVAR_ARCHIVE(vr_melee_bloodlust, 0);
DEFINE_FCVAR_ARCHIVE(vr_melee_bloodlust_mult, 1.0);
DEFINE_FCVAR_ARCHIVE(vr_enemy_drops, 0);
DEFINE_FCVAR_ARCHIVE(vr_enemy_drops_chance_mult, 1.0);
DEFINE_FCVAR_ARCHIVE(vr_ammobox_drops, 0);
DEFINE_FCVAR_ARCHIVE(vr_ammobox_drops_chance_mult, 1.0);
DEFINE_FCVAR_ARCHIVE(vr_menumode, 0);
DEFINE_FCVAR_ARCHIVE(vr_forcegrab_powermult, 1.0);
DEFINE_FCVAR_ARCHIVE(vr_forcegrab_mode, 1);
DEFINE_FCVAR_ARCHIVE(vr_forcegrab_range, 150.0);
DEFINE_FCVAR_ARCHIVE(vr_forcegrab_radius, 18.0);
DEFINE_FCVAR_ARCHIVE(vr_forcegrab_eligible_particles, 1);
DEFINE_FCVAR_ARCHIVE(vr_forcegrab_eligible_haptics, 1);
DEFINE_FCVAR_ARCHIVE(vr_weapondrop_particles, 1);
DEFINE_FCVAR_ARCHIVE(vr_2h_spread_reduction, 0.5); // TODO VR: (P2) add to menu
DEFINE_FCVAR_ARCHIVE(
    vr_2h_throw_velocity_mult, 1.4); // TODO VR: (P2) add to menu
DEFINE_FCVAR_ARCHIVE(
    vr_headbutt_velocity_threshold, 2.02);         // TODO VR: (P2) add to menu
DEFINE_FCVAR_ARCHIVE(vr_headbutt_damage_mult, 32); // TODO VR: (P2) add to menu
DEFINE_FCVAR_ARCHIVE(vr_activestartpaknameidx, 0);
DEFINE_FCVAR_ARCHIVE(vr_verbosebots, 0);
DEFINE_FCVAR_ARCHIVE(vr_finger_grip_bias, 0.0);
DEFINE_FCVAR_ARCHIVE(vr_2h_disable_angle_threshold, 0);
DEFINE_FCVAR_ARCHIVE(vr_autosave_seconds, 240);
DEFINE_FCVAR_ARCHIVE(vr_autosave_on_changelevel, 1);
DEFINE_FCVAR_ARCHIVE(vr_throw_up_center_of_mass, 0.1);
DEFINE_FCVAR_ARCHIVE(vr_forcegrabbable_ammo_boxes, 1);
DEFINE_FCVAR_ARCHIVE(vr_forcegrabbable_health_boxes, 1);
DEFINE_FCVAR_ARCHIVE(vr_forcegrabbable_return_time_deathmatch, 4);
DEFINE_FCVAR_ARCHIVE(vr_forcegrabbable_return_time_singleplayer, 0);
DEFINE_FCVAR_ARCHIVE(vr_finger_auto_close_thumb, 1);
DEFINE_FCVAR_ARCHIVE(vr_autosave_show_message, 0);
DEFINE_FCVAR_ARCHIVE(vr_finger_blending, 1);
DEFINE_FCVAR_ARCHIVE(vr_finger_blending_speed, 50);

//
//
//
// ----------------------------------------------------------------------------
// Finger tracking offsets
// ----------------------------------------------------------------------------

// All fingers and base
DEFINE_FCVAR_ARCHIVE(vr_fingers_and_base_x, 0.0);
DEFINE_FCVAR_ARCHIVE(vr_fingers_and_base_y, 0.0);
DEFINE_FCVAR_ARCHIVE(vr_fingers_and_base_z, 0.0);

// All fingers and base (off-hand only)
DEFINE_FCVAR_ARCHIVE(vr_fingers_and_base_offhand_x, 0.0);
DEFINE_FCVAR_ARCHIVE(vr_fingers_and_base_offhand_y, 0.0);
DEFINE_FCVAR_ARCHIVE(vr_fingers_and_base_offhand_z, 0.0);

// All fingers
DEFINE_FCVAR_ARCHIVE(vr_fingers_x, 0.0);
DEFINE_FCVAR_ARCHIVE(vr_fingers_y, 0.0);
DEFINE_FCVAR_ARCHIVE(vr_fingers_z, 0.0);

// Thumb
DEFINE_FCVAR_ARCHIVE(vr_finger_thumb_x, 0.0);
DEFINE_FCVAR_ARCHIVE(vr_finger_thumb_y, 0.0);
DEFINE_FCVAR_ARCHIVE(vr_finger_thumb_z, 0.0);

// Index
DEFINE_FCVAR_ARCHIVE(vr_finger_index_x, 0.0);
DEFINE_FCVAR_ARCHIVE(vr_finger_index_y, 0.0);
DEFINE_FCVAR_ARCHIVE(vr_finger_index_z, 0.0);

// Middle
DEFINE_FCVAR_ARCHIVE(vr_finger_middle_x, 0.0);
DEFINE_FCVAR_ARCHIVE(vr_finger_middle_y, 0.0);
DEFINE_FCVAR_ARCHIVE(vr_finger_middle_z, 0.0);

// Ring
DEFINE_FCVAR_ARCHIVE(vr_finger_ring_x, 0.0);
DEFINE_FCVAR_ARCHIVE(vr_finger_ring_y, 0.0);
DEFINE_FCVAR_ARCHIVE(vr_finger_ring_z, 0.0);

// Pinky
DEFINE_FCVAR_ARCHIVE(vr_finger_pinky_x, 0.0);
DEFINE_FCVAR_ARCHIVE(vr_finger_pinky_y, 0.0);
DEFINE_FCVAR_ARCHIVE(vr_finger_pinky_z, 0.0);

// Base
DEFINE_FCVAR_ARCHIVE(vr_finger_base_x, 0.0);
DEFINE_FCVAR_ARCHIVE(vr_finger_base_y, 0.0);
DEFINE_FCVAR_ARCHIVE(vr_finger_base_z, 0.0);

#undef DEFINE_FCVAR_ARCHIVE
#undef DEFINE_FCVAR

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

//
//
//
// ----------------------------------------------------------------------------
// Getters
// ----------------------------------------------------------------------------

#define QVR_CVAR_VEC3_XYZ(cvar_family_prefix)                       \
    {                                                               \
        cvar_family_prefix##_x.value, cvar_family_prefix##_y.value, \
            cvar_family_prefix##_z.value                            \
    }

namespace quake::vr
{
[[nodiscard]] qvec3 get_fingers_and_base_xyz() noexcept
{
    return QVR_CVAR_VEC3_XYZ(vr_fingers_and_base);
}

[[nodiscard]] qvec3 get_fingers_and_base_offhand_xyz() noexcept
{
    return QVR_CVAR_VEC3_XYZ(vr_fingers_and_base_offhand);
}

[[nodiscard]] qvec3 get_fingers_xyz() noexcept
{
    return QVR_CVAR_VEC3_XYZ(vr_fingers);
}

[[nodiscard]] qvec3 get_finger_thumb_xyz() noexcept
{
    return QVR_CVAR_VEC3_XYZ(vr_finger_thumb);
}

[[nodiscard]] qvec3 get_finger_index_xyz() noexcept
{
    return QVR_CVAR_VEC3_XYZ(vr_finger_index);
}

[[nodiscard]] qvec3 get_finger_middle_xyz() noexcept
{
    return QVR_CVAR_VEC3_XYZ(vr_finger_middle);
}

[[nodiscard]] qvec3 get_finger_ring_xyz() noexcept
{
    return QVR_CVAR_VEC3_XYZ(vr_finger_ring);
}

[[nodiscard]] qvec3 get_finger_pinky_xyz() noexcept
{
    return QVR_CVAR_VEC3_XYZ(vr_finger_pinky);
}

[[nodiscard]] qvec3 get_finger_base_xyz() noexcept
{
    return QVR_CVAR_VEC3_XYZ(vr_finger_base);
}
} // namespace quake::vr

#undef QVR_CVAR_VEC3_XYZ
