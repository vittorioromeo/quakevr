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
QVR_CVAR(vr_throw_hit_min_speed, "200", CVAR_ARCHIVE) // units/s a thrown thing must go at to hurt what it touches (slower: the throw is over)
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

// Eye image size: the runtime's recommended size (SteamVR's supersampling included) times this,
// per side (0.5 .. 1.5; pixels go with its square), within the runtime's largest.
QVR_CVAR(vr_xr_runtime, "0", CVAR_ARCHIVE)             // OpenXR runtime: 0 the system's (or XR_RUNTIME_JSON), 1 Virtual Desktop (VDXR), 2 SteamVR, 3 vr_xr_runtime_json
QVR_CVAR(vr_xr_runtime_json, "", CVAR_ARCHIVE)        // with vr_xr_runtime 3: the runtime's manifest (.json)
QVR_CVAR(vr_render_scale, "1", CVAR_ARCHIVE)
// Leave out the pixels the lenses never show (the runtime's hidden area, XR_KHR_visibility_mask):
// filled black and at the near plane before the scene, so the scene's shaders skip them.
QVR_CVAR(vr_visibility_mask, "1", CVAR_ARCHIVE)
// Mock backend: a round hidden area (the corners outside a circle), to test vr_visibility_mask.
QVR_CVAR(vr_mock_hidden_area, "0", CVAR_NONE)

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
QVR_CVAR(vr_melee_speed, "3.5", CVAR_ARCHIVE)        // m/s the wrist must reach in a blow, at least (a swung weapon 1.25x; a good blow 1.6x, a good swing 1.8x; QC: vr_juice.qc, "Blows")
QVR_CVAR(vr_melee_distance, "0.25", CVAR_ARCHIVE)    // metres the wrist must travel in a blow (a full punch or swing): flicks, wiggles and whips don't hit
QVR_CVAR(vr_melee_punch_mult, "1.25", CVAR_ARCHIVE)   // damage of a straight punch (where you face) over a slap
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
QVR_CVAR(vr_gadget_light, "1", CVAR_ARCHIVE)          // the screen casts a light in its colour the way it faces: strength (0 off)
QVR_CVAR(vr_gadget_crt, "1", CVAR_ARCHIVE)            // the screen looks like a small CRT (scanlines, a flicker, faint static, now and then a glitch): strength (0 off, up to 2)
QVR_CVAR(vr_notify_wrist, "1", CVAR_ARCHIVE)          // the console's messages (notify lines) with the gadget: 0 in front of the head, 1 in a log floating over the gadget, 2 both
QVR_CVAR(vr_notify_wrist_time, "8", CVAR_ARCHIVE)     // seconds a message stays in that log (0: con_notifytime's)
QVR_CVAR(vr_sbar_offset_x, "-12", CVAR_ARCHIVE)
QVR_CVAR(vr_sbar_offset_y, "1", CVAR_ARCHIVE)
QVR_CVAR(vr_sbar_offset_z, "-3", CVAR_ARCHIVE)
QVR_CVAR(vr_sbar_offset_pitch, "1", CVAR_ARCHIVE)
QVR_CVAR(vr_sbar_offset_yaw, "1.6", CVAR_ARCHIVE)
QVR_CVAR(vr_sbar_offset_roll, "-0.3", CVAR_ARCHIVE)
QVR_CVAR(vr_height_calibration, "1.646099", CVAR_ARCHIVE)
QVR_CVAR(vr_menu_distance, "80", CVAR_ARCHIVE)
QVR_CVAR(vr_menu_vr_style, "1", CVAR_ARCHIVE)          // menus made for the headset (vr_menuui.cpp): filling the panel, rows spaced out, modern sliders and switches, a laser pointer
QVR_CVAR(vr_menu_spacing, "1.5", CVAR_ARCHIVE)         // with it, the rows' spacing over Quake's (1 to 2)
QVR_CVAR(vr_menu_remember, "1", CVAR_ARCHIVE)          // opening the menu after "Back to game" (the panel's top-left button, or holding the menu button) returns to the page it closed on
QVR_CVAR(vr_melee_dmg_multiplier, "1.0", CVAR_ARCHIVE)
QVR_CVAR(vr_melee_range_multiplier, "1.0", CVAR_ARCHIVE)
QVR_CVAR(vr_body_interactions, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_notes, "1", CVAR_ARCHIVE)                  // voice notes: off hand at the mouth, hold Y to talk (vr_voicenotes.cpp)
QVR_CVAR(vr_note_device, "Virtual Desktop", CVAR_ARCHIVE) // the microphone: part of its name (none such, or empty: the default; vr_note_devices lists them)
QVR_CVAR(vr_memstats_log, "60", CVAR_ARCHIVE)      // seconds between rows of quakevr/profile/memstats_<date>.csv (and one after each map load; 0: off)
QVR_CVAR(vr_lean_radius, "12", CVAR_ARCHIVE)          // units the head may lean from the middle of the player's box before the body follows (0: always above it)
QVR_CVAR(vr_lean_recenter, "0.5", CVAR_ARCHIVE)       // metres/second the body slides back under the head where it can (0: never)
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
QVR_CVAR(vr_body_step_rate, "2.2", CVAR_ARCHIVE)      // at most this many steps a second at full running speed (fewer walking)
QVR_CVAR(vr_body_turn_step, "40", CVAR_ARCHIVE)       // degrees the body turns over its planted feet before they step round
QVR_CVAR(vr_body_state, "1", CVAR_ARCHIVE)            // the body shows the armour worn and, bloodier, the damage taken
QVR_CVAR(vr_body_powerups, "1", CVAR_ARCHIVE)         // and the powerups: quad damage arcs, pentagram glow, ring translucency, biosuit tint
QVR_CVAR(vr_body_blood, "1", CVAR_ARCHIVE)            // the wounded arms and hands drip blood (vr_body_state's wounds; above 1, more drops)
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
QVR_CVAR(vr_body_pauldrons, "1", CVAR_ARCHIVE)        // pauldrons on the body's shoulders, after the Quake ranger's (make_pauldron.py)
QVR_CVAR(vr_body_pauldron_style, "0", CVAR_ARCHIVE)   // 0 the ranger's brown leather, 1 the colour of the armour worn (leather without), 2 steel
QVR_CVAR(vr_body_pauldron_size, "1", CVAR_ARCHIVE)    // their size (scaled with the build too)
QVR_CVAR(vr_body_pauldron_follow, "0.35", CVAR_ARCHIVE) // how much the cap turns with the upper arm (0: with the shoulder; the lames follow the arm)
QVR_CVAR(vr_body_pauldron_forward, "0", CVAR_ARCHIVE) // metres they sit further forward,
QVR_CVAR(vr_body_pauldron_up, "0", CVAR_ARCHIVE)      // up,
QVR_CVAR(vr_body_pauldron_out, "0", CVAR_ARCHIVE)     // and out
// The chest flashlight (vr_flashlight.cpp): trigger at it switches it, an empty hand's grip takes it.
QVR_CVAR(vr_flashlight, "1", CVAR_ARCHIVE)            // 0: no flashlight
QVR_CVAR(vr_flashlight_brightness, "1", CVAR_ARCHIVE)
QVR_CVAR(vr_flashlight_range, "1000", CVAR_ARCHIVE)   // units the beam reaches
QVR_CVAR(vr_flashlight_shadows, "1", CVAR_ARCHIVE)    // its spot light casts shadows (takes one of vr_shadow_dlights: one tile, not six faces)
QVR_CVAR(vr_flashlight_beam, "0.35", CVAR_ARCHIVE)    // how strong its soft cone of light in the air is, 0..1 (0: none)
QVR_CVAR(vr_flashlight_tilt, "8", CVAR_ARCHIVE)       // degrees the mounted lamp points below where the torso faces
QVR_CVAR(vr_flashlight_forward, "0", CVAR_ARCHIVE)    // metres the mount sits further forward,
QVR_CVAR(vr_flashlight_up, "0", CVAR_ARCHIVE)         // up,
QVR_CVAR(vr_flashlight_out, "0", CVAR_ARCHIVE)        // and out (towards the off hand's side)
QVR_CVAR(vr_flashlight_hand_forward, "-0.05", CVAR_ARCHIVE) // metres the lamp sits forward in the fist (negative: back),
QVR_CVAR(vr_flashlight_hand_up, "-0.04", CVAR_ARCHIVE)      // and up (negative: down)
// Spent shell casings (vr_shells.cpp): the shotgun throws one out as it cycles, the super shotgun two as it is reloaded.
QVR_CVAR(vr_shells, "1", CVAR_ARCHIVE)                // 0: none
QVR_CVAR(vr_shells_life, "20", CVAR_ARCHIVE)          // seconds they lie on the floor before fading out
QVR_CVAR(vr_shells_sound, "1", CVAR_ARCHIVE)          // volume of their tink as they land (0 off)
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
QVR_CVAR(vr_holster_haptic_time, "0.25", CVAR_ARCHIVE) // seconds of the buzz as a hand reaches a holster (vr_holster_haptics 2: once)
QVR_CVAR(vr_model_lighting, "1", CVAR_ARCHIVE)        // models shaded from the map's lights (0: Quake's fixed direction; 0..1 blends)
QVR_CVAR(vr_relit_maps, "1", CVAR_ARCHIVE)           // maps re-lit by Misc/quakevr/relight_maps.py, when present (next map)
QVR_CVAR(vr_particles, "1", CVAR_ARCHIVE)             // Quake VR's textured particles for the QC's effects (0: Quake's own)
QVR_CVAR(vr_particle_mult, "1", CVAR_ARCHIVE)         // how many of them
QVR_CVAR(vr_blob_shadows, "2", CVAR_ARCHIVE)          // blob shadows (dark discs under things): 0 off, 1 always, 2 only without real shadows
QVR_CVAR(vr_player_shadows, "2", CVAR_ARCHIVE)
QVR_CVAR(vr_entity_shadows, "1", CVAR_ARCHIVE)        // blob shadows under monsters and items
QVR_CVAR(vr_water_splash_speed, "150", CVAR_ARCHIVE)  // units/s a thing (not a player) must move at to splash entering or leaving water
QVR_CVAR(vr_water_splash, "1", CVAR_ARCHIVE)          // splashes (drops, foam, rings) where shots, things, you and your hands hit water, slime and lava: 0 off .. 2 more
QVR_CVAR(vr_water_sounds, "1", CVAR_ARCHIVE)          // volume of the water sounds: splashes, shots into water, wading, swimming strokes, hands slapping (0 off)
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
QVR_CVAR(vr_profile, "0", CVAR_NONE)                   // profiler (vr_profile.cpp): 1 time the frame's scopes, CPU and GPU, into <gamedir>/profile/; 2 also show them over the wrist
QVR_CVAR(vr_profile_interval, "5", CVAR_NONE)          // seconds between its reports (0: only on vr_profile_dump)
QVR_CVAR(vr_dlight_models, "1", CVAR_ARCHIVE)          // dynamic lights light models per pixel, by angle and shadowed (0: Quake's flat)
QVR_CVAR(vr_dlight_angle, "1", CVAR_ARCHIVE)           // dynamic lights fall off with the angle they reach surfaces at (0: Quake's)
QVR_CVAR(vr_dlight_uncapped, "1", CVAR_ARCHIVE)        // dynamic lights add fully to bright walls (Quake caps them)
QVR_CVAR(vr_light_contrast, "2", CVAR_ARCHIVE)         // lightmap contrast about Quake's full light: shade darker, lamps as bright (1: Quake's)
QVR_CVAR(vr_decals, "1", CVAR_ARCHIVE)                 // blood and scorch marks on the world
QVR_CVAR(vr_decal_max, "512", CVAR_ARCHIVE)            // how many are kept (the oldest go first)
QVR_CVAR(vr_decal_life, "120", CVAR_ARCHIVE)           // seconds a mark stays (fading over the last five)
QVR_CVAR(vr_gib_blood, "1", CVAR_ARCHIVE)              // gibs and heads bleed: Quake VR's blood trail, drops on the floor, splats where they hit (0: Quake's trail)
QVR_CVAR(vr_gib_blood_trail, "1", CVAR_ARCHIVE)        // how dense their trail of blood and drops is (0 none)
QVR_CVAR(vr_bloom, "0.3", CVAR_ARCHIVE)                // glow around bright things (lamps, glowing panels, flashes): its strength (0 off)
QVR_CVAR(vr_bloom_threshold, "0.7", CVAR_ARCHIVE)      // how bright a thing must be to glow (0..1)
QVR_CVAR(vr_bloom_radius, "1", CVAR_ARCHIVE)           // how wide the glow spreads
QVR_CVAR(vr_flash_scale, "1", CVAR_ARCHIVE)            // muzzle flashes' light: size over the falloff's own (Quake's or DarkPlaces')
QVR_CVAR(vr_explosion_light_scale, "1", CVAR_ARCHIVE)  // explosions' and rockets' light: size over the falloff's own
QVR_CVAR(vr_colored_lights, "1", CVAR_ARCHIVE)         // muzzle flashes, rockets and explosions coloured (warm; the lightning gun blue)
QVR_CVAR(vr_projectile_lights, "1", CVAR_ARCHIVE)      // glowing projectiles light up the room (hell knight flames, scrag spit, vore balls, lasers; and where they hit): size (0 off)
QVR_CVAR(vr_weapon_screen_light, "1", CVAR_ARCHIVE)    // the weapons' ammo screens cast a small light in their colour the way they face: strength (0 off)
QVR_CVAR(vr_screen_glow, "1", CVAR_ARCHIVE)            // the wrist gadget's and the weapons' screens glow softly round their edges: strength (0 off)
QVR_CVAR(vr_screen_text_glow, "1", CVAR_ARCHIVE)       // the text, numbers and icons on those screens glow: whitish cores, a soft halo in the screen's colour (strength, 0 off, up to 3)
QVR_CVAR(vr_weapon_glow, "1", CVAR_ARCHIVE)            // your weapons' glowing parts (the shotgun's sights) brighter, so that they glow: strength (0: as Quake)
// The DarkPlaces look (round 10; docs/vr-port/LIGHTING.md).
QVR_CVAR(vr_gamma, "1", CVAR_ARCHIVE)                  // the headset's gamma (the desktop's "gamma" is not used in the eyes; 1 neutral)
QVR_CVAR(vr_contrast, "1", CVAR_ARCHIVE)               // the headset's contrast (the desktop's "contrast" is not used in the eyes; 1 neutral)
QVR_CVAR(vr_dlight_falloff, "1", CVAR_ARCHIVE)         // dynamic lights: 1 DarkPlaces' (smooth falloff, strong coloured lights), 0 Quake's (linear)
QVR_CVAR(vr_specular, "0.125", CVAR_ARCHIVE)           // sheen of dynamic lights on the world and models (Blinn, exponent 32; 0 off)
QVR_CVAR(vr_normalmaps, "1", CVAR_ARCHIVE)             // bumps from normal maps made from the textures, lit by the map's light and dynamic lights (next map)
QVR_CVAR(vr_normalmap_strength, "1", CVAR_ARCHIVE)     // how deep those bumps are
QVR_CVAR(vr_normalmap_baked, "1", CVAR_ARCHIVE)        // how much the bumps shade the map's (baked) light, lit from where the light seems to come from (0: only dynamic lights)
QVR_CVAR(vr_normalmap_models, "1", CVAR_ARCHIVE)       // how much the bumps shade models' own light, from its direction (times vr_normalmap_baked; held weapons less; 0: only dynamic lights)
QVR_CVAR(vr_parallax, "1", CVAR_ARCHIVE)               // parallax occlusion mapping on the world: its dark parts sunk in, seen round from the side (needs vr_normalmaps)
QVR_CVAR(vr_parallax_depth, "3", CVAR_ARCHIVE)       // how deep, in units, the darkest parts of a texture sink below its brightest
QVR_CVAR(vr_parallax_distance, "512", CVAR_ARCHIVE)    // how far it reaches, in units (it fades out over the last quarter)
QVR_CVAR(vr_parallax_steps, "16", CVAR_ARCHIVE)        // the most steps it takes along a ray (at grazing angles; half as many looking straight on)
QVR_CVAR(vr_parallax_items, "1.5", CVAR_ARCHIVE)      // parallax depth of the ammo and health boxes, in their own units (scaled with the box; 0 off)
QVR_CVAR(vr_parallax_models, "0", CVAR_ARCHIVE)       // parallax depth of models (weapons, pickups, monsters, your body), in their own units (0 off: on Quake's blocky skins it bends the texels; 0.5 to 0.75 for smooth skins)
QVR_CVAR(vr_model_light_parity, "1", CVAR_ARCHIVE)     // models as bright as the floor under them, on average (0: Quake's brighter models)
QVR_CVAR(vr_viewmodel_minlight, "8", CVAR_ARCHIVE)     // least light on your hands and weapons (Quake's: 24)
QVR_CVAR(vr_texture_smooth, "1", CVAR_ARCHIVE)         // smooth filtering: 1 replacement textures (textures/*.tga), 2 every texture, 0 as gl_texturemode
QVR_CVAR(vr_bloom_white, "0.5", CVAR_ARCHIVE)          // bloom of white and pale lights, times vr_bloom
QVR_CVAR(vr_bloom_color, "1.5", CVAR_ARCHIVE)          // bloom of coloured lights (red buttons, blue panels), times vr_bloom
QVR_CVAR(vr_bloom_adapt, "4", CVAR_ARCHIVE)           // bloom weakens the more of the view glows (0: never)
// Liquids (vr_water.cpp): 0 everywhere is Quake's look.
QVR_CVAR(vr_water_waves, "1", CVAR_ARCHIVE)           // waves on water, slime and lava (normal-mapped, in the shader: the faces stay flat): strength
QVR_CVAR(vr_water_fresnel, "1", CVAR_ARCHIVE)         // water more see-through looking down, reflecting the room at grazing angles
QVR_CVAR(vr_water_refraction, "1", CVAR_ARCHIVE)      // what is under translucent water bent by the waves (needs r_oit 1, no anti-aliasing): strength
QVR_CVAR(vr_water_glints, "1", CVAR_ARCHIVE)          // light glinting off the waves: strength
QVR_CVAR(vr_water_lava_glow, "1", CVAR_ARCHIVE)       // lava glows (brighter where hot, a slow pulse; blooms): strength
QVR_CVAR(vr_water_caustics, "0.6", CVAR_ARCHIVE)      // dappled light on what is under water: strength
QVR_CVAR(vr_water_underwater, "1", CVAR_ARCHIVE)      // with your head in a liquid: its fog and tint, a little blur (0: Quake's colour shift)
QVR_CVAR(vr_water_wobble, "1", CVAR_ARCHIVE)          // ... and a slow wobble of the view (0 off, for comfort)
QVR_CVAR(vr_water_geo_waves, "1", CVAR_ARCHIVE)       // real waves: the level liquid surfaces cut into a grid whose vertices rise and fall with long swells
QVR_CVAR(vr_water_geo_amplitude, "3", CVAR_ARCHIVE)   // ... their height in units (water's; lava's more, slime's less)
QVR_CVAR(vr_water_geo_cell, "16", CVAR_ARCHIVE)       // ... the grid's cell in units (smaller: smoother, more triangles; remade on change)
QVR_CVAR(vr_positional_damage, "1", CVAR_ARCHIVE)
QVR_CVAR(vr_headshot_sound, "0.5", CVAR_ARCHIVE)      // volume of the tick on a headshot (0 off)
QVR_CVAR(vr_climb, "0", CVAR_ARCHIVE)                 // experimental: an empty hand gripping a ledge hangs from it, pulls up and mantles (vr_climb.cpp)
QVR_CVAR(vr_climb_min_height, "30", CVAR_ARCHIVE)     // units above the feet a ledge must be to take hold of
QVR_CVAR(vr_climb_fling, "1", CVAR_ARCHIVE)           // how much letting go flings you with the hands' motion (capped; 0 none)
QVR_CVAR(vr_climb_debug, "0", CVAR_NONE)              // print holds, releases and mantles (2: and every hanging frame)
QVR_CVAR(vr_swim, "1", CVAR_ARCHIVE)                  // VR swimming: the stick slows in water, strokes of the hands move you
QVR_CVAR(vr_swim_shallow_speed, "0.85", CVAR_ARCHIVE) // the stick's speed with the feet in water,
QVR_CVAR(vr_swim_wade_speed, "0.6", CVAR_ARCHIVE)     // waist deep standing on the bottom,
QVR_CVAR(vr_swim_stick_speed, "0.2", CVAR_ARCHIVE)    // and swimming (deeper, or off the bottom)
QVR_CVAR(vr_swim_stroke_assist, "0.5", CVAR_ARCHIVE)  // strokes push this much more towards where the stick points (and as much less against it)
QVR_CVAR(vr_cfg_version, "0", CVAR_ARCHIVE)           // the changed defaults this config has taken (vr_cvars.cpp)
QVR_CVAR(vr_swim_stroke, "10", CVAR_ARCHIVE)          // how strongly a hand pushing the water moves you (per second, at a brisk stroke: slower less)
QVR_CVAR(vr_swim_stroke_min, "0.4", CVAR_ARCHIVE)     // metres/second a hand must move to push
QVR_CVAR(vr_swim_palm, "0.6", CVAR_ARCHIVE)           // how much it matters that the palm, not the edge, meets the water
QVR_CVAR(vr_swim_max_speed, "400", CVAR_ARCHIVE)      // units/second strokes can reach
QVR_CVAR(vr_swim_look, "0.6", CVAR_ARCHIVE)           // how much of a stroke's push sideways of where you look is lost (you swim where you look)
QVR_CVAR(vr_swim_recovery, "0.1", CVAR_ARCHIVE)       // how much a hand going edge first (the recovery, for the next stroke) pushes
QVR_CVAR(vr_swim_glide, "0.5", CVAR_ARCHIVE)          // how much of the water's friction is taken away while swimming, so you glide between strokes
QVR_CVAR(vr_swim_power_threshold, "1.1", CVAR_ARCHIVE) // m/s a stroke's peak hand speed must pass to propel at all (intent: a relaxed return does nothing; 0 off)
QVR_CVAR(vr_swim_power_knee, "0.5", CVAR_ARCHIVE)     // m/s above the threshold over which a stroke fades in to full power (0: all or nothing)
QVR_CVAR(vr_swim_power_whole, "1", CVAR_ARCHIVE)      // 1: a stroke passing the threshold counts whole (its start too); 0: only from the moment it passes
QVR_CVAR(vr_swim_intent_memory, "1.5", CVAR_ARCHIVE)  // seconds a hand remembers its last full stroke's direction (0 off)
QVR_CVAR(vr_swim_reverse_damp, "0.85", CVAR_ARCHIVE)  // how much a weaker stroke against the remembered one is damped (0..1)
QVR_CVAR(vr_swim_reverse_speed, "1", CVAR_ARCHIVE)    // a stroke against the remembered one counts fully from this fraction of its peak speed (1.2: 20% faster); 0.3 less: damped fully
QVR_CVAR(vr_swim_flat_exp, "1", CVAR_ARCHIVE)         // how sharply the palm's flatness matters (1 linear, 2-3: only a really flat palm pushes fully)
QVR_CVAR(vr_swim_speed_exp, "2", CVAR_ARCHIVE)        // push over hand speed: 1 linear, 2 as water's drag (fast strokes outweigh slow ones more), up to 3
QVR_CVAR(vr_swim_palm_dir, "0", CVAR_ARCHIVE)         // 0: the push goes against the hand's motion; 1: along the palm's facing (sweeping palms-in pushes sideways)
QVR_CVAR(vr_swim_debug, "0", CVAR_NONE)               // print each stroke: peak speed, power gate, reverse damping and the push it gave
QVR_CVAR(vr_sword_drop, "1", CVAR_ARCHIVE)            // chance a knight or hell knight drops its sword, a melee weapon
QVR_CVAR(vr_sword_damage_mult, "1.5", CVAR_ARCHIVE)   // its swing's damage over the axe's
QVR_CVAR(vr_wofs_version, "0", CVAR_ARCHIVE)          // the weapon settings changes the config has taken (vr_weapons.cpp)
QVR_CVAR(vr_carry_take, "0", CVAR_ARCHIVE)            // taking a carried box: 0 let go of it at a holster, 1 the trigger, 2 either
QVR_CVAR(vr_carry_local, "1", CVAR_ARCHIVE)           // what you carry is drawn in your hand as it moves this frame (0: where the server has it, a frame or a ping behind)
QVR_CVAR(vr_forcegrab_outline, "1", CVAR_ARCHIVE)     // the glow round what a hand's force grab points at (strength, 0 off)
QVR_CVAR(vr_forcegrab_fx, "1", CVAR_ARCHIVE)          // force grab's beam and energy tendril from the hand, and the pulled object's trail
QVR_CVAR(vr_grab_gibs, "1", CVAR_ARCHIVE)             // gibs and heads: 0 left alone, 1 picked up and thrown by hand, 2 also force-grabbed
QVR_CVAR(vr_carry, "1", CVAR_ARCHIVE)                 // ammo and health boxes: grip to carry, trigger to take, nudge with hands and guns (0: touch takes them)
QVR_CVAR(vr_carry_nudge, "1", CVAR_ARCHIVE)           // how hard hands and guns push them
QVR_CVAR(vr_carry_throw_mult, "1", CVAR_ARCHIVE)      // how fast they leave the hand
QVR_CVAR(vr_carry_melee_mult, "1.5", CVAR_ARCHIVE)    // punches with a box in hand hurt this much more
QVR_CVAR(vr_carry_throw_damage, "8", CVAR_ARCHIVE)    // damage of a box thrown hard (at about 6 m/s; more the faster)
QVR_CVAR(vr_gib_throw_damage, "4", CVAR_ARCHIVE)      // and of a gib or head
QVR_CVAR(vr_gib_destroy, "1", CVAR_ARCHIVE)           // gibs and heads can be destroyed (shot, blown up, struck, thrown hard): they burst in a mist of blood
QVR_CVAR(vr_gib_health, "12", CVAR_ARCHIVE)           // the damage that destroys a gib (a head takes half as much again)
QVR_CVAR(vr_gib_splat_speed, "250", CVAR_ARCHIVE)     // units/s a thrown gib or head must hit a wall or a monster at to burst
QVR_CVAR(vr_corpse_gib, "1", CVAR_ARCHIVE)            // corpses can be gibbed: shot, blown up or struck enough, they burst into gibs
QVR_CVAR(vr_corpse_health, "40", CVAR_ARCHIVE)        // the damage that gibs a corpse (a big monster's takes up to twice as much)
QVR_CVAR(vr_parry, "1", CVAR_ARCHIVE)                 // parry monsters' melee blows with a weapon held sideways in front of you
QVR_CVAR(vr_parry_reduction, "0.75", CVAR_ARCHIVE)    // share of a parried blow's damage taken away
QVR_CVAR(vr_parry_drop_chance, "0.5", CVAR_ARCHIVE)   // chance a one-handed parry knocks the weapon out of your hand (two hands: never)
QVR_CVAR(vr_parry_angle, "50", CVAR_ARCHIVE)          // degrees the weapon may be off square to the blow
QVR_CVAR(vr_parry_unarmed, "1", CVAR_ARCHIVE)          // parry unarmed with the arms crossed in an X in front
QVR_CVAR(vr_parry_unarmed_reduction, "0.5", CVAR_ARCHIVE) // share of a blow taken off by crossed arms
QVR_CVAR(vr_bash, "1", CVAR_ARCHIVE)                   // hold a guard (a weapon across, both hands together), then push it forward to bash; or shove with one open palm
QVR_CVAR(vr_bash_speed, "1.6", CVAR_ARCHIVE)           // m/s the guard must be pushed forward
QVR_CVAR(vr_bash_damage, "8", CVAR_ARCHIVE)            // a bash's damage (small: it knocks back and staggers)
QVR_CVAR(vr_bash_push, "1", CVAR_ARCHIVE)              // how far a bash throws what it hits
QVR_CVAR(vr_parry_wobble, "1", CVAR_ARCHIVE)          // how much a parried blow knocks your hand and arm
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
QVR_CVAR(vr_headbutt, "1", CVAR_ARCHIVE)                 // lunge your head at something to headbutt it
QVR_CVAR(vr_headbutt_speed, "1", CVAR_ARCHIVE)           // m/s the head must lunge (roughly towards where it looks)
QVR_CVAR(vr_headbutt_damage, "32", CVAR_ARCHIVE)         // damage at that speed (up to three times it, faster)
QVR_CVAR(vr_damage_to_enemies, "1", CVAR_ARCHIVE)        // damage the player deals to monsters
QVR_CVAR(vr_damage_to_player, "1", CVAR_ARCHIVE)         // damage monsters (and the world) deal to the player
QVR_CVAR(vr_damage_self, "1", CVAR_ARCHIVE)              // damage the player deals to themselves (rockets, grenades)
QVR_CVAR(vr_headshot_mult, "1.5", CVAR_ARCHIVE)          // positional damage: headshots,
QVR_CVAR(vr_limbshot_mult, "0.35", CVAR_ARCHIVE)         // arms (far from the body's middle),
QVR_CVAR(vr_legshot_mult, "0.6", CVAR_ARCHIVE)           // and legs
QVR_CVAR(vr_melee_push, "1", CVAR_ARCHIVE)               // how far your melee blows push what they hit
QVR_CVAR(vr_melee_push_player, "1", CVAR_ARCHIVE)        // how far monsters' melee blows push you
QVR_CVAR(vr_parry_push_enemy, "1", CVAR_ARCHIVE)         // how far a parry pushes the attacker back
QVR_CVAR(vr_parry_push_player, "1", CVAR_ARCHIVE)        // and you
QVR_CVAR(vr_push, "0.5", CVAR_ARCHIVE)                    // knockback: all of it (the others scale this)
QVR_CVAR(vr_hit_push, "1", CVAR_ARCHIVE)                 // heavy weapon hits shove monsters
QVR_CVAR(vr_kill_push, "1", CVAR_ARCHIVE)                // killing blows throw the bodies
QVR_CVAR(vr_explosion_rumble, "1", CVAR_ARCHIVE)         // explosions near you rumble in your hands
QVR_CVAR(vr_heartbeat, "1", CVAR_ARCHIVE)                // a heartbeat in your hands when your health is low
QVR_CVAR(vr_deflect, "1", CVAR_ARCHIVE)                  // melee swings bat monsters' projectiles back
QVR_CVAR(vr_deflect_radius, "14", CVAR_ARCHIVE)          // units around the weapon's line (or fist) a projectile is batted within
QVR_CVAR(vr_deflect_speed, "0.6", CVAR_ARCHIVE)          // a batting swing's wrist speed, times vr_melee_speed (a blow's least is 1)
QVR_CVAR(vr_deflect_window, "0.2", CVAR_ARCHIVE)         // s a swing's path keeps batting (a swing a little early still bats)
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
QVR_CVAR(vr_weapon_screen_crt, "1", CVAR_ARCHIVE)      // it looks like a small CRT, as the wrist gadget's (scanlines, a flicker, faint static, now and then a glitch): strength (0 off, up to 2)
QVR_CVAR(vr_worldtext_crt, "1", CVAR_ARCHIVE)          // map text boards (world texts) are CRT screens with glowing text, as the wrist gadget's: strength (0 plain text, up to 2)
QVR_CVAR(vr_worldtext_hue, "40", CVAR_ARCHIVE)         // their phosphor's hue (degrees; 40 amber, 128 green, 200 blue)
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
