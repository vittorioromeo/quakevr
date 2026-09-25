// vr_cvars.inc -- every Quake VR cvar: QVR_CVAR(name, default, flags).
// Names carried over from the QSS-based Quake VR engine; defaults are the values the Quake VR
// releases shipped in id1/config.cfg.

// Gameplay rule changes (opt-in, enabled by quakevr/quakevr.cfg)
QVR_CVAR(vr_gameplayfix_droptofloor, "0", CVAR_NONE)  // corner traces instead of a box sweep
QVR_CVAR(vr_gameplayfix_touchsolids, "0", CVAR_NONE)  // .touch fires for solids, not just triggers
QVR_CVAR(vr_gameplayfix_missilesize, "0", CVAR_NONE)  // missile-vs-monster box extent (0: vanilla 15)
QVR_CVAR(vr_compat_muzzle, "1", CVAR_ARCHIVE)          // with another mod's progs, its shots start at the gun, not the player's head
QVR_CVAR(vr_gameplayfix_tossfall, "0", CVAR_NONE)     // resting items and weapons fall when their support goes
QVR_CVAR(vr_pickup_scale, "1", CVAR_NONE)             // weapons, armour, powerups and keys drawn smaller, on the floor
QVR_CVAR(vr_item_float_height, "26", CVAR_ARCHIVE)    // units above the floor the map's weapons, armour, powerups and keys float at (0: on the floor); next map

// Throwing (vr_throw.cpp, docs/vr-port/THROWING.md): the controller's own speed at its peak, in a
// window around the moment of release (vr_throw_window before it, vr_throw_lookahead after),
// averaged over vr_throw_peak_span seconds; the wrist's spin adds to it only above
// vr_throw_ang_threshold rad/s, by vr_throw_ang_factor.
QVR_CVAR(vr_throw_window, "0.12", CVAR_ARCHIVE)
QVR_CVAR(vr_throw_lookahead, "0.01", CVAR_ARCHIVE)
QVR_CVAR(vr_throw_peak_span, "0.017", CVAR_ARCHIVE)
QVR_CVAR(vr_throw_lever_arm, "0.1", CVAR_ARCHIVE)   // metres from the palm to the held object's centre
QVR_CVAR(vr_throw_dir_lookback, "0.04", CVAR_ARCHIVE) // seconds before the peak the direction is averaged over
QVR_CVAR(vr_throw_ang_threshold, "6", CVAR_ARCHIVE)
QVR_CVAR(vr_throw_ang_factor, "0.7", CVAR_ARCHIVE)
QVR_CVAR(vr_throw_gravity, "9.81", CVAR_ARCHIVE)   // m/s^2 on thrown weapons, bounces included (0: sv_gravity)
QVR_CVAR(vr_debug_throw, "0", CVAR_NONE)           // print each throw's estimate (2: and its timing)
// Release: the runtime's grip button lets go late. With vr_throw_release 1 the analog grip lets
// go once it drops vr_throw_release_drop below its peak while the hand moves faster than
// vr_throw_release_speed m/s (a throw), or below vr_throw_release_floor anyway; it grabs again
// above vr_throw_grab_press. Controllers without an analog grip keep the button.
QVR_CVAR(vr_throw_release, "1", CVAR_ARCHIVE)
QVR_CVAR(vr_throw_release_drop, "0.3", CVAR_ARCHIVE)
QVR_CVAR(vr_throw_release_floor, "0.35", CVAR_ARCHIVE)
QVR_CVAR(vr_throw_release_speed, "1.5", CVAR_ARCHIVE)
QVR_CVAR(vr_throw_grab_press, "0.7", CVAR_ARCHIVE)
// Gameplay scaling (QC): speeds up to vr_throw_gain_lo m/s leave the hand as they are (drops,
// passes), rising smoothly to vr_throw_gain_max times at vr_throw_gain_hi; a weapon's weight
// slows it by vr_throw_weight_influence of its old multiplier.
QVR_CVAR(vr_throw_gain_max, "1.5", CVAR_ARCHIVE)
QVR_CVAR(vr_throw_gain_lo, "1.5", CVAR_ARCHIVE)
QVR_CVAR(vr_throw_gain_hi, "6", CVAR_ARCHIVE)
QVR_CVAR(vr_throw_weight_influence, "0.25", CVAR_ARCHIVE)
QVR_CVAR(vr_throw_hitbox, "6", CVAR_ARCHIVE)       // half-size (units) of a thrown weapon's box against monsters
// Aim assist (QC, opt-in): bends a throw by up to vr_throw_assist_strength onto a monster or
// breakable within vr_throw_assist_cone degrees of it (fully within vr_throw_assist_full), in
// range and in sight, on a ballistic arc, allowing vr_throw_assist_speed more speed.
QVR_CVAR(vr_throw_assist, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_throw_assist_cone, "12", CVAR_ARCHIVE)
QVR_CVAR(vr_throw_assist_full, "4", CVAR_ARCHIVE)
QVR_CVAR(vr_throw_assist_strength, "0.8", CVAR_ARCHIVE)
QVR_CVAR(vr_throw_assist_range, "1200", CVAR_ARCHIVE)
QVR_CVAR(vr_throw_assist_speed, "0.15", CVAR_ARCHIVE)
// Thrown weapons and backpacks as rigid bodies (vr_rigid.cpp): bounciness and friction of
// their contacts, their spin at most, and how fast the air slows it.
QVR_CVAR(vr_throw_restitution, "0.25", CVAR_ARCHIVE)
QVR_CVAR(vr_throw_friction, "0.6", CVAR_ARCHIVE)
QVR_CVAR(vr_throw_spin_max, "20", CVAR_ARCHIVE)    // rad/s
QVR_CVAR(vr_throw_spin_drag, "0.3", CVAR_ARCHIVE)  // per second, in the air

// Version of quakevr/vr_bindings.cfg last applied (vr_checkbindings).
QVR_CVAR(vr_bindings_version, "0", CVAR_ARCHIVE)

// Mock backend: swing the main hand in an overhand arc with this period, in seconds (0: still).
QVR_CVAR(vr_mock_swing, "0", CVAR_NONE)

// Desktop mirror of the headset view: 0 off, 1 left eye, 2 both eyes side by side.
QVR_CVAR(vr_mirror, "1", CVAR_ARCHIVE)

// Near clipping plane in the headset, in units (the desktop's is up to 4).
QVR_CVAR(vr_nearclip, "1", CVAR_ARCHIVE)

QVR_CVAR(vr_enabled, "0", CVAR_NONE)
QVR_CVAR(vr_lefthanded, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_fakevr_handroll, "0", CVAR_NONE)
QVR_CVAR(vr_crosshair, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_crosshair_depth, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_crosshair_size, "1", CVAR_ARCHIVE)
QVR_CVAR(vr_crosshair_alpha, "0.85", CVAR_ARCHIVE)
QVR_CVAR(vr_deadzone, "25", CVAR_ARCHIVE)
// Controller rotation offsets, the released values: tuned for OpenVR's raw controller pose, which
// the OpenXR backend reproduces from the grip pose with vr_controller_legacy_pose 1 (Touch, Quest
// and Index controllers), so that they and the weapon offsets line up as in the old engine.
QVR_CVAR(vr_controller_legacy_pose, "1", CVAR_ARCHIVE)
QVR_CVAR(vr_gunangle, "39.5", CVAR_ARCHIVE)
QVR_CVAR(vr_gunmodelpitch, "7", CVAR_ARCHIVE)
QVR_CVAR(vr_gunmodelscale, "0.7", CVAR_ARCHIVE)
QVR_CVAR(vr_gunmodely, "1.3", CVAR_ARCHIVE)
QVR_CVAR(vr_crosshairy, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_world_scale, "1.25", CVAR_ARCHIVE)
QVR_CVAR(vr_floor_offset, "-21", CVAR_ARCHIVE)
QVR_CVAR(vr_snap_turn, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_enable_joystick_turn, "1", CVAR_ARCHIVE)
QVR_CVAR(vr_turn_speed, "3.25", CVAR_ARCHIVE)
QVR_CVAR(vr_movement_mode, "1", CVAR_ARCHIVE)
QVR_CVAR(vr_hud_scale, "0.025", CVAR_ARCHIVE)
QVR_CVAR(vr_menu_scale, "0.15", CVAR_ARCHIVE)
QVR_CVAR(vr_melee_speed, "3", CVAR_ARCHIVE)          // m/s a hand swings at to hit (QC: weapons.qc, VRMeleeDmg)
QVR_CVAR(vr_melee_punch_mult, "1.25", CVAR_ARCHIVE)   // damage of a punch (knuckles first) over any other swing
QVR_CVAR(vr_gunyaw, "4", CVAR_ARCHIVE)
QVR_CVAR(vr_gun_z_offset, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_sbar_mode, "1", CVAR_ARCHIVE)
QVR_CVAR(vr_hud_mode, "1", CVAR_ARCHIVE)              // 0 the status bar on a hand (vr_sbar_mode), 1 the wrist gadget
QVR_CVAR(vr_gadget_hand, "0", CVAR_ARCHIVE)           // the wrist gadget's arm: 0 the off hand's, 1 the main hand's
QVR_CVAR(vr_gadget_scale, "1", CVAR_ARCHIVE)          // its size
QVR_CVAR(vr_gadget_x, "0", CVAR_ARCHIVE)              // cm it moves along the forearm (towards the fingers on the left arm),
QVR_CVAR(vr_gadget_y, "0", CVAR_ARCHIVE)              // across it (the screen's up),
QVR_CVAR(vr_gadget_z, "0", CVAR_ARCHIVE)              // and out of it
QVR_CVAR(vr_gadget_pitch, "0", CVAR_ARCHIVE)          // degrees it turns about the screen's right,
QVR_CVAR(vr_gadget_yaw, "0", CVAR_ARCHIVE)            // its up,
QVR_CVAR(vr_gadget_roll, "0", CVAR_ARCHIVE)           // and out of the screen (180: the text runs the other way)
QVR_CVAR(vr_gadget_tint_hue, "30", CVAR_ARCHIVE)      // the casing's tint: hue (degrees),
QVR_CVAR(vr_gadget_tint, "0", CVAR_ARCHIVE)           // and strength (0: its own olive drab)
QVR_CVAR(vr_gadget_screen_hue, "128", CVAR_ARCHIVE)   // the screen's colour: hue (degrees; 128 green, 40 amber, 200 blue),
QVR_CVAR(vr_gadget_screen_brightness, "1", CVAR_ARCHIVE) // its text and lines' brightness,
QVR_CVAR(vr_gadget_screen_background, "1", CVAR_ARCHIVE) // and its background's
QVR_CVAR(vr_gadget_show_level, "1", CVAR_ARCHIVE)     // the level's name, kills and secrets on the screen
QVR_CVAR(vr_sbar_offset_x, "-12", CVAR_ARCHIVE)
QVR_CVAR(vr_sbar_offset_y, "1", CVAR_ARCHIVE)
QVR_CVAR(vr_sbar_offset_z, "-3", CVAR_ARCHIVE)
QVR_CVAR(vr_sbar_offset_pitch, "1", CVAR_ARCHIVE)
QVR_CVAR(vr_sbar_offset_yaw, "1.6", CVAR_ARCHIVE)
QVR_CVAR(vr_sbar_offset_roll, "-0.3", CVAR_ARCHIVE)
QVR_CVAR(vr_height_calibration, "1.646099", CVAR_ARCHIVE)
QVR_CVAR(vr_menu_distance, "80", CVAR_ARCHIVE)
QVR_CVAR(vr_melee_dmg_multiplier, "1.0", CVAR_ARCHIVE)
QVR_CVAR(vr_melee_range_multiplier, "1.0", CVAR_ARCHIVE)
QVR_CVAR(vr_body_interactions, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_roomscale_jump, "1", CVAR_ARCHIVE)          // jumping in the room jumps in the game: the head rising faster than
QVR_CVAR(vr_roomscale_jump_threshold, "0.8", CVAR_ARCHIVE) // this (m/s) above the standing height
QVR_CVAR(vr_roomscale_move_mult, "1.0", CVAR_ARCHIVE)
QVR_CVAR(vr_teleport_enabled, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_teleport_range, "400", CVAR_ARCHIVE)
QVR_CVAR(vr_2h_mode, "2", CVAR_ARCHIVE)
QVR_CVAR(vr_2h_angle_threshold, "0.65", CVAR_ARCHIVE)
QVR_CVAR(vr_virtual_stock_thresh, "10", CVAR_ARCHIVE)
QVR_CVAR(vr_show_virtual_stock, "0", CVAR_NONE)
QVR_CVAR(vr_shoulder_offset_x, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_shoulder_offset_y, "1.75", CVAR_ARCHIVE)
QVR_CVAR(vr_shoulder_offset_z, "16.0", CVAR_ARCHIVE)
QVR_CVAR(vr_2h_virtual_stock_factor, "0.5", CVAR_ARCHIVE)
QVR_CVAR(vr_wpn_pos_weight, "1", CVAR_ARCHIVE)
QVR_CVAR(vr_wpn_pos_weight_offset, "0.0", CVAR_ARCHIVE)
QVR_CVAR(vr_wpn_pos_weight_mult, "1.0", CVAR_ARCHIVE)
QVR_CVAR(vr_wpn_pos_weight_2h_help_offset, "0.3", CVAR_ARCHIVE)
QVR_CVAR(vr_wpn_pos_weight_2h_help_mult, "1.0", CVAR_ARCHIVE)
QVR_CVAR(vr_wpn_dir_weight, "1", CVAR_ARCHIVE)
QVR_CVAR(vr_wpn_dir_weight_offset, "0.05", CVAR_ARCHIVE)
QVR_CVAR(vr_wpn_dir_weight_mult, "1.0", CVAR_ARCHIVE)
QVR_CVAR(vr_wpn_dir_weight_2h_help_offset, "0.3", CVAR_ARCHIVE)
QVR_CVAR(vr_wpn_dir_weight_2h_help_mult, "1.0", CVAR_ARCHIVE)
QVR_CVAR(vr_offhandpitch, "40.25", CVAR_ARCHIVE)
QVR_CVAR(vr_offhandyaw, "-4", CVAR_ARCHIVE)
QVR_CVAR(vr_show_hip_holsters, "0", CVAR_NONE)
QVR_CVAR(vr_hip_offset_x, "-3.5", CVAR_ARCHIVE)
QVR_CVAR(vr_hip_offset_y, "7.0", CVAR_ARCHIVE)
QVR_CVAR(vr_hip_offset_z, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_hip_holster_thresh, "6.5", CVAR_ARCHIVE)
QVR_CVAR(vr_show_shoulder_holsters, "0", CVAR_NONE)
QVR_CVAR(vr_shoulder_holster_offset_x, "-0.5", CVAR_ARCHIVE)
QVR_CVAR(vr_shoulder_holster_offset_y, "2.25", CVAR_ARCHIVE)
QVR_CVAR(vr_shoulder_holster_offset_z, "-0.25", CVAR_ARCHIVE)
QVR_CVAR(vr_shoulder_holster_thresh, "7.8", CVAR_ARCHIVE)
QVR_CVAR(vr_show_upper_holsters, "0", CVAR_NONE)
QVR_CVAR(vr_upper_holster_offset_x, "-4.25", CVAR_ARCHIVE)
QVR_CVAR(vr_upper_holster_offset_y, "7", CVAR_ARCHIVE)
QVR_CVAR(vr_upper_holster_offset_z, "8.5", CVAR_ARCHIVE)
QVR_CVAR(vr_upper_holster_thresh, "6.5", CVAR_ARCHIVE)
// The player's body (vr_avatar.cpp): 0 none, 2 torso and arms, 3 torso, arms and legs: the skinned
// progs/vrbody model, posed by IK from the head and hands (1, the old floating torso, is now 2).
QVR_CVAR(vr_body_mode, "2", CVAR_ARCHIVE)
QVR_CVAR(vr_body_build, "1", CVAR_ARCHIVE)            // 0 lean, 1 athletic, 2 brawny (progs/vrbody_lean, vrbody, vrbody_brawny)
QVR_CVAR(vr_body_walk, "1", CVAR_ARCHIVE)             // legs (vr_body_mode 3) walk as the player moves
QVR_CVAR(vr_body_state, "1", CVAR_ARCHIVE)            // the body shows the armour worn and, bloodier, the damage taken
QVR_CVAR(vr_body_powerups, "1", CVAR_ARCHIVE)         // and the powerups: quad damage arcs, pentagram glow, ring translucency, biosuit tint
QVR_CVAR(vr_body_anchors, "1", CVAR_ARCHIVE)          // holsters, virtual stock and hand collisions follow the body's lean and crouch
QVR_CVAR(vr_body_debug, "0", CVAR_NONE)               // 1 draw the skeleton; 2, 3 also show the body in front of you, facing you or from its left
QVR_CVAR(vr_body_eye_forward, "0.08", CVAR_ARCHIVE)   // metres from the top of the neck to the eyes, forward and up
QVR_CVAR(vr_body_eye_up, "0.08", CVAR_ARCHIVE)
QVR_CVAR(vr_body_crouch_tilt, "25", CVAR_ARCHIVE)    // degrees the back tilts forward in a full crouch
QVR_CVAR(vr_body_torso_back, "0", CVAR_ARCHIVE)       // metres the torso sits further behind the neck (negative: in front)
QVR_CVAR(vr_body_legs_back, "0", CVAR_ARCHIVE)        // metres the feet stand behind the head (negative: in front)
QVR_CVAR(vr_body_shoulders_back, "0", CVAR_ARCHIVE)   // metres the shoulders sit behind the chest (negative: in front),
QVR_CVAR(vr_body_shoulders_up, "0", CVAR_ARCHIVE)     // above it,
QVR_CVAR(vr_body_shoulders_out, "0", CVAR_ARCHIVE)    // and further out (negative: in)
QVR_CVAR(vr_body_arm_length, "1", CVAR_ARCHIVE)      // arm length, relative to the proportions of vr_height_calibration
QVR_CVAR(vr_body_arm_stretch, "1.2", CVAR_ARCHIVE)    // how far arms may stretch to reach the hands (1: not at all)
QVR_CVAR(vr_body_shoulder_reach, "0.08", CVAR_ARCHIVE) // metres the shoulders may reach out beyond that
QVR_CVAR(vr_body_forearm_twist, "0.5", CVAR_ARCHIVE)  // share of the wrist's roll the forearm follows
QVR_CVAR(vr_body_elbow_out, "0.35", CVAR_ARCHIVE)     // elbow direction: down, plus this much outwards,
QVR_CVAR(vr_body_elbow_back, "0.25", CVAR_ARCHIVE)    // this much backwards,
QVR_CVAR(vr_body_elbow_hand, "0.4", CVAR_ARCHIVE)     // and this much away from the back of the hand
QVR_CVAR(vr_body_shoulder_up, "25", CVAR_ARCHIVE)     // degrees the shoulders rise reaching up,
QVR_CVAR(vr_body_shoulder_forward, "20", CVAR_ARCHIVE) // and swing forward reaching far forward
QVR_CVAR(vr_holster_haptics, "2", CVAR_ARCHIVE)
QVR_CVAR(vr_model_lighting, "1", CVAR_ARCHIVE)        // models shaded from the map's lights (0: Quake's fixed direction; 0..1 blends)
QVR_CVAR(vr_relit_maps, "1", CVAR_ARCHIVE)           // maps re-lit by Misc/quakevr/relight_maps.py, when present (next map)
QVR_CVAR(vr_particles, "1", CVAR_ARCHIVE)             // Quake VR's textured particles for the QC's effects (0: Quake's own)
QVR_CVAR(vr_particle_mult, "1", CVAR_ARCHIVE)         // how many of them
QVR_CVAR(vr_player_shadows, "2", CVAR_ARCHIVE)
QVR_CVAR(vr_entity_shadows, "1", CVAR_ARCHIVE)        // blob shadows under monsters and items
QVR_CVAR(vr_water_splash_speed, "150", CVAR_ARCHIVE)  // units/s a thing (not a player) must move at to splash entering or leaving water
// Real-time shadows and per-pixel dynamic lights (vr_lighting.cpp, docs/vr-port/LIGHTING.md).
QVR_CVAR(vr_graphics_preset, "-1", CVAR_NONE)          // set 0 off .. 4 ultra to apply a preset to the settings below
QVR_CVAR(vr_shadow_dlights, "4", CVAR_ARCHIVE)         // dynamic lights (explosions, rockets) casting shadows, the most important first (0 off)
QVR_CVAR(vr_shadow_dlight_size, "512", CVAR_ARCHIVE)   // their largest face size, texels (smaller farther away)
QVR_CVAR(vr_shadow_precision, "1", CVAR_ARCHIVE)       // texels per unit of a light's radius, close by
QVR_CVAR(vr_shadow_muzzleflash, "0", CVAR_ARCHIVE)     // your own muzzle flashes cast shadows too (brief, and costly)
QVR_CVAR(vr_shadow_maplights, "2", CVAR_ARCHIVE)       // map lights near you casting the shadows of monsters and you (0 off)
QVR_CVAR(vr_shadow_maplight_size, "512", CVAR_ARCHIVE) // their face size, texels
QVR_CVAR(vr_shadow_maplight_strength, "0.7", CVAR_ARCHIVE) // how much of their light those shadows take away
QVR_CVAR(vr_shadow_self, "2", CVAR_ARCHIVE)            // map lights' shadows of you: 0 none, 1 your body, 2 body and hands
QVR_CVAR(vr_shadow_filter, "1", CVAR_ARCHIVE)          // shadow edge softening: 0 hard (1 tap), 1 (4 taps), 2 (9), 3 (16)
QVR_CVAR(vr_shadow_bias, "1", CVAR_ARCHIVE)            // shadow acne vs. light leaking under objects
QVR_CVAR(vr_shadow_distance, "1536", CVAR_ARCHIVE)     // lights farther than this from you cast no shadows
QVR_CVAR(vr_shadow_atlas, "4096", CVAR_ARCHIVE)        // shadow map atlas size (2048, 4096, 8192)
QVR_CVAR(vr_shadow_stats, "0", CVAR_NONE)              // print what is shadowed and the GPU time, once a second
QVR_CVAR(vr_dlight_models, "1", CVAR_ARCHIVE)          // dynamic lights light models per pixel, by angle and shadowed (0: Quake's flat)
QVR_CVAR(vr_dlight_angle, "1", CVAR_ARCHIVE)           // dynamic lights fall off with the angle they reach surfaces at (0: Quake's)
QVR_CVAR(vr_dlight_uncapped, "0", CVAR_ARCHIVE)        // dynamic lights add fully to bright walls (Quake caps them)
QVR_CVAR(vr_positional_damage, "1", CVAR_ARCHIVE)
QVR_CVAR(vr_headshot_sound, "0.5", CVAR_ARCHIVE)      // volume of the tick on a headshot (0 off)
QVR_CVAR(vr_leg_holster_model_enabled, "1", CVAR_ARCHIVE)
QVR_CVAR(vr_leg_holster_model_scale, "0.5", CVAR_ARCHIVE)
QVR_CVAR(vr_leg_holster_model_x_offset, "1", CVAR_ARCHIVE)
QVR_CVAR(vr_leg_holster_model_y_offset, "1.25", CVAR_ARCHIVE)
QVR_CVAR(vr_leg_holster_model_z_offset, "2.25", CVAR_ARCHIVE)
QVR_CVAR(vr_holster_mode, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_weapon_throw_mode, "0", CVAR_ARCHIVE)
// 0: a weapon is dropped (thrown) as soon as its grip is let go; 1: it stays in the hand, and
// is thrown (or holstered) when the grip is let go after gripping it again.
QVR_CVAR(vr_weapon_grip_mode, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_weapon_throw_damage_mult, "1.0", CVAR_ARCHIVE)
QVR_CVAR(vr_weapon_throw_velocity_mult, "1.0", CVAR_ARCHIVE)
QVR_CVAR(vr_weapon_cycle_mode, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_melee_bloodlust, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_melee_bloodlust_mult, "1.0", CVAR_ARCHIVE)
QVR_CVAR(vr_enemy_drops, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_enemy_drops_chance_mult, "1.0", CVAR_ARCHIVE)
QVR_CVAR(vr_ammobox_drops, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_ammobox_drops_chance_mult, "1.0", CVAR_ARCHIVE)
QVR_CVAR(vr_forcegrab_mode, "1", CVAR_ARCHIVE)             // force grab (QC vr_wpnforcegrab.qc): 0 off, 1 on
QVR_CVAR(vr_forcegrab_distance, "200", CVAR_ARCHIVE)       // units objects can be pulled from,
QVR_CVAR(vr_forcegrab_cone, "15", CVAR_ARCHIVE)            // degrees off where the hand points
QVR_CVAR(vr_forcegrab_flick_speed, "1.0", CVAR_ARCHIVE)    // m/s back or up the hand flicks to pull,
QVR_CVAR(vr_forcegrab_flick_turn, "250", CVAR_ARCHIVE)     // or degrees/s the fingers swing that way
QVR_CVAR(vr_forcegrab_time, "0.35", CVAR_ARCHIVE)          // seconds of flight, plus
QVR_CVAR(vr_forcegrab_speed, "1000", CVAR_ARCHIVE)         // a second per this many units
QVR_CVAR(vr_forcegrab_arc, "0.15", CVAR_ARCHIVE)           // height of the arc, of the distance
QVR_CVAR(vr_forcegrab_catch_radius, "14", CVAR_ARCHIVE)    // units from the hand it can be caught,
QVR_CVAR(vr_forcegrab_catch_early, "0.35", CVAR_ARCHIVE)   // the grip pressed at most this early,
QVR_CVAR(vr_forcegrab_catch_late, "0.15", CVAR_ARCHIVE)    // or this late
QVR_CVAR(vr_forcegrab_miss_speed, "0.2", CVAR_ARCHIVE)     // share of its speed a missed object keeps
QVR_CVAR(vr_forcegrab_eligible_particles, "1", CVAR_ARCHIVE)
QVR_CVAR(vr_forcegrab_eligible_haptics, "1", CVAR_ARCHIVE)
QVR_CVAR(vr_weapondrop_particles, "1", CVAR_ARCHIVE)
QVR_CVAR(vr_2h_spread_reduction, "0.5", CVAR_ARCHIVE)
QVR_CVAR(vr_2h_throw_velocity_mult, "1.0", CVAR_ARCHIVE)
QVR_CVAR(vr_headbutt_velocity_threshold, "2.02", CVAR_ARCHIVE)
QVR_CVAR(vr_headbutt_damage_mult, "32", CVAR_ARCHIVE)
QVR_CVAR(vr_activestartpaknameidx, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_verbosebots, "0", CVAR_NONE)
QVR_CVAR(vr_finger_grip_bias, "0.0", CVAR_ARCHIVE)
QVR_CVAR(vr_forcegrabbable_ammo_boxes, "1", CVAR_ARCHIVE)
QVR_CVAR(vr_forcegrabbable_health_boxes, "1", CVAR_ARCHIVE)
QVR_CVAR(vr_forcegrabbable_box_scale, "0.25", CVAR_ARCHIVE) // size of those ammo and health boxes
QVR_CVAR(vr_forcegrabbable_return_time_deathmatch, "4", CVAR_ARCHIVE)
QVR_CVAR(vr_forcegrabbable_return_time_singleplayer, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_finger_auto_close_thumb, "1", CVAR_ARCHIVE)
QVR_CVAR(vr_finger_blending_speed, "50", CVAR_ARCHIVE)
QVR_CVAR(vr_reload_mode, "2", CVAR_ARCHIVE)
QVR_CVAR(vr_show_weapon_text, "1", CVAR_ARCHIVE)
QVR_CVAR(vr_weapon_screen, "1", CVAR_ARCHIVE)          // the weapons' ammo counters on a small screen (colours: vr_gadget_screen_*)
QVR_CVAR(vr_weapon_screen_padding, "1", CVAR_ARCHIVE)  // room around the text on it
QVR_CVAR(vr_disablehaptics, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_spinreload_pitch_speed, "1100", CVAR_ARCHIVE)
QVR_CVAR(vr_spinreload_x_angular_threshold, "6.5", CVAR_ARCHIVE)
QVR_CVAR(vr_player_stepsize, "18.0", CVAR_ARCHIVE)
// All fingers and base
QVR_CVAR(vr_fingers_and_base_x, "1.925", CVAR_ARCHIVE)
QVR_CVAR(vr_fingers_and_base_y, "-2.825", CVAR_ARCHIVE)
QVR_CVAR(vr_fingers_and_base_z, "-2.075", CVAR_ARCHIVE)
// All fingers and base (off-hand only)
QVR_CVAR(vr_fingers_and_base_offhand_x, "0.0", CVAR_ARCHIVE)
QVR_CVAR(vr_fingers_and_base_offhand_y, "0.0", CVAR_ARCHIVE)
QVR_CVAR(vr_fingers_and_base_offhand_z, "0.0", CVAR_ARCHIVE)
// All fingers
QVR_CVAR(vr_fingers_x, "-5.05", CVAR_ARCHIVE)
QVR_CVAR(vr_fingers_y, "-0.1", CVAR_ARCHIVE)
QVR_CVAR(vr_fingers_z, "-0.1875", CVAR_ARCHIVE)
// Thumb
QVR_CVAR(vr_finger_thumb_x, "-0.3625", CVAR_ARCHIVE)
QVR_CVAR(vr_finger_thumb_y, "3.2625", CVAR_ARCHIVE)
QVR_CVAR(vr_finger_thumb_z, "-1.9375", CVAR_ARCHIVE)
// Index
QVR_CVAR(vr_finger_index_x, "-0.325", CVAR_ARCHIVE)
QVR_CVAR(vr_finger_index_y, "0.6125", CVAR_ARCHIVE)
QVR_CVAR(vr_finger_index_z, "-1.825", CVAR_ARCHIVE)
// Middle
QVR_CVAR(vr_finger_middle_x, "-0.3625", CVAR_ARCHIVE)
QVR_CVAR(vr_finger_middle_y, "0.5125", CVAR_ARCHIVE)
QVR_CVAR(vr_finger_middle_z, "-0.3125", CVAR_ARCHIVE)
// Ring
QVR_CVAR(vr_finger_ring_x, "-0.3625", CVAR_ARCHIVE)
QVR_CVAR(vr_finger_ring_y, "0.7", CVAR_ARCHIVE)
QVR_CVAR(vr_finger_ring_z, "0.65", CVAR_ARCHIVE)
// Pinky
QVR_CVAR(vr_finger_pinky_x, "-0.325", CVAR_ARCHIVE)
QVR_CVAR(vr_finger_pinky_y, "1.15", CVAR_ARCHIVE)
QVR_CVAR(vr_finger_pinky_z, "1.6375", CVAR_ARCHIVE)
// Base
QVR_CVAR(vr_finger_base_x, "0.0", CVAR_ARCHIVE)
QVR_CVAR(vr_finger_base_y, "0.0", CVAR_ARCHIVE)
QVR_CVAR(vr_finger_base_z, "0.0", CVAR_ARCHIVE)
