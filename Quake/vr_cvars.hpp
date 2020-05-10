#pragma once

#include "cvar.hpp"
#include "quakeglm.hpp"

//
//
//
// ----------------------------------------------------------------------------
// CVar Declarations
// ----------------------------------------------------------------------------

extern cvar_t vr_aimmode;
extern cvar_t vr_crosshair_alpha;
extern cvar_t vr_crosshair_depth;
extern cvar_t vr_crosshair_size;
extern cvar_t vr_crosshair;
extern cvar_t vr_crosshairy;
extern cvar_t vr_deadzone;
extern cvar_t vr_enabled;
extern cvar_t vr_lefthanded;
extern cvar_t vr_floor_offset;
extern cvar_t vr_gun_z_offset;
extern cvar_t vr_gunangle;
extern cvar_t vr_gunmodelpitch;
extern cvar_t vr_gunmodelscale;
extern cvar_t vr_gunmodely;
extern cvar_t vr_gunyaw;
extern cvar_t vr_hud_scale;
extern cvar_t vr_melee_threshold;
extern cvar_t vr_menu_scale;
extern cvar_t vr_movement_mode;
extern cvar_t vr_msaa;
extern cvar_t vr_enable_joystick_turn;
extern cvar_t vr_snap_turn;
extern cvar_t vr_turn_speed;
extern cvar_t vr_viewkick;
extern cvar_t vr_world_scale;
extern cvar_t vr_sbar_mode;
extern cvar_t vr_sbar_offset_x;
extern cvar_t vr_sbar_offset_y;
extern cvar_t vr_sbar_offset_z;
extern cvar_t vr_sbar_offset_pitch;
extern cvar_t vr_sbar_offset_yaw;
extern cvar_t vr_sbar_offset_roll;
extern cvar_t vr_roomscale_jump;
extern cvar_t vr_height_calibration;
extern cvar_t vr_roomscale_jump_threshold;
extern cvar_t vr_menu_distance;
extern cvar_t vr_melee_dmg_multiplier;
extern cvar_t vr_melee_range_multiplier;
extern cvar_t vr_body_interactions;
extern cvar_t vr_roomscale_move_mult;
extern cvar_t vr_teleport_enabled;
extern cvar_t vr_teleport_range;
extern cvar_t vr_2h_mode;
extern cvar_t vr_2h_angle_threshold;
extern cvar_t vr_virtual_stock_thresh;
extern cvar_t vr_show_virtual_stock;
extern cvar_t vr_shoulder_offset_x;
extern cvar_t vr_shoulder_offset_y;
extern cvar_t vr_shoulder_offset_z;
extern cvar_t vr_2h_virtual_stock_factor;
extern cvar_t vr_wpn_pos_weight;
extern cvar_t vr_wpn_pos_weight_offset;
extern cvar_t vr_wpn_pos_weight_mult;
extern cvar_t vr_wpn_pos_weight_2h_help_offset;
extern cvar_t vr_wpn_pos_weight_2h_help_mult;
extern cvar_t vr_wpn_dir_weight;
extern cvar_t vr_wpn_dir_weight_offset;
extern cvar_t vr_wpn_dir_weight_mult;
extern cvar_t vr_wpn_dir_weight_2h_help_offset;
extern cvar_t vr_wpn_dir_weight_2h_help_mult;
extern cvar_t vr_offhandpitch;
extern cvar_t vr_offhandyaw;
extern cvar_t vr_show_hip_holsters;
extern cvar_t vr_hip_offset_x;
extern cvar_t vr_hip_offset_y;
extern cvar_t vr_hip_offset_z;
extern cvar_t vr_hip_holster_thresh;
extern cvar_t vr_show_shoulder_holsters;
extern cvar_t vr_shoulder_holster_offset_x;
extern cvar_t vr_shoulder_holster_offset_y;
extern cvar_t vr_shoulder_holster_offset_z;
extern cvar_t vr_shoulder_holster_thresh;
extern cvar_t vr_show_upper_holsters;
extern cvar_t vr_upper_holster_offset_x;
extern cvar_t vr_upper_holster_offset_y;
extern cvar_t vr_upper_holster_offset_z;
extern cvar_t vr_upper_holster_thresh;
extern cvar_t vr_fakevr;
extern cvar_t vr_novrinit;
extern cvar_t vr_vrtorso_debuglines_enabled;
extern cvar_t vr_vrtorso_enabled;
extern cvar_t vr_vrtorso_x_offset;
extern cvar_t vr_vrtorso_y_offset;
extern cvar_t vr_vrtorso_z_offset;
extern cvar_t vr_vrtorso_head_z_mult;
extern cvar_t vr_vrtorso_x_scale;
extern cvar_t vr_vrtorso_y_scale;
extern cvar_t vr_vrtorso_z_scale;
extern cvar_t vr_vrtorso_pitch;
extern cvar_t vr_vrtorso_yaw;
extern cvar_t vr_vrtorso_roll;
extern cvar_t vr_holster_haptics;
extern cvar_t vr_player_shadows;
extern cvar_t vr_positional_damage;
extern cvar_t vr_debug_print_handvel;
extern cvar_t vr_debug_print_headvel;
extern cvar_t vr_debug_show_hand_pos_and_rot;
extern cvar_t vr_leg_holster_model_enabled;
extern cvar_t vr_leg_holster_model_scale;
extern cvar_t vr_leg_holster_model_x_offset;
extern cvar_t vr_leg_holster_model_y_offset;
extern cvar_t vr_leg_holster_model_z_offset;
extern cvar_t vr_holster_mode;
extern cvar_t vr_weapon_throw_mode;
extern cvar_t vr_weapon_throw_damage_mult;
extern cvar_t vr_weapon_throw_velocity_mult;
extern cvar_t vr_weapon_cycle_mode;
extern cvar_t vr_melee_bloodlust;
extern cvar_t vr_melee_bloodlust_mult;
extern cvar_t vr_enemy_drops;
extern cvar_t vr_enemy_drops_chance_mult;
extern cvar_t vr_ammobox_drops;
extern cvar_t vr_ammobox_drops_chance_mult;
extern cvar_t vr_menumode;
extern cvar_t vr_forcegrab_powermult;
extern cvar_t vr_forcegrab_mode;
extern cvar_t vr_forcegrab_range;
extern cvar_t vr_forcegrab_radius;
extern cvar_t vr_weapondrop_particles;
extern cvar_t vr_forcegrab_eligible_particles;
extern cvar_t vr_forcegrab_eligible_haptics;
extern cvar_t vr_2h_spread_reduction;
extern cvar_t vr_2h_throw_velocity_mult;
extern cvar_t vr_headbutt_velocity_threshold;
extern cvar_t vr_headbutt_damage_mult;
extern cvar_t vr_activestartpaknameidx;

//
//
//
// ----------------------------------------------------------------------------
// Finger tracking offsets
// ----------------------------------------------------------------------------

// All fingers and base
extern cvar_t vr_fingers_and_base_x;
extern cvar_t vr_fingers_and_base_y;
extern cvar_t vr_fingers_and_base_z;

// All fingers and base (off-hand only)
extern cvar_t vr_fingers_and_base_offhand_x;
extern cvar_t vr_fingers_and_base_offhand_y;
extern cvar_t vr_fingers_and_base_offhand_z;

// All fingers
extern cvar_t vr_fingers_x;
extern cvar_t vr_fingers_y;
extern cvar_t vr_fingers_z;

// Thumb
extern cvar_t vr_finger_thumb_x;
extern cvar_t vr_finger_thumb_y;
extern cvar_t vr_finger_thumb_z;

// Index
extern cvar_t vr_finger_index_x;
extern cvar_t vr_finger_index_y;
extern cvar_t vr_finger_index_z;

// Middle
extern cvar_t vr_finger_middle_x;
extern cvar_t vr_finger_middle_y;
extern cvar_t vr_finger_middle_z;

// Ring
extern cvar_t vr_finger_ring_x;
extern cvar_t vr_finger_ring_y;
extern cvar_t vr_finger_ring_z;

// Pinky
extern cvar_t vr_finger_pinky_x;
extern cvar_t vr_finger_pinky_y;
extern cvar_t vr_finger_pinky_z;

// Base
extern cvar_t vr_finger_base_x;
extern cvar_t vr_finger_base_y;
extern cvar_t vr_finger_base_z;

//
//
//
// ----------------------------------------------------------------------------
// Registration
// ----------------------------------------------------------------------------

namespace quake::vr
{
    void register_all_cvars() noexcept;
}

//
//
//
// ----------------------------------------------------------------------------
// Getters
// ----------------------------------------------------------------------------

namespace quake::vr
{
    [[nodiscard]] qvec3 get_fingers_and_base_xyz() noexcept;
    [[nodiscard]] qvec3 get_fingers_and_base_offhand_xyz() noexcept;
    [[nodiscard]] qvec3 get_fingers_xyz() noexcept;
    [[nodiscard]] qvec3 get_finger_thumb_xyz() noexcept;
    [[nodiscard]] qvec3 get_finger_index_xyz() noexcept;
    [[nodiscard]] qvec3 get_finger_middle_xyz() noexcept;
    [[nodiscard]] qvec3 get_finger_ring_xyz() noexcept;
    [[nodiscard]] qvec3 get_finger_pinky_xyz() noexcept;
    [[nodiscard]] qvec3 get_finger_base_xyz() noexcept;
} // namespace quake::vr
