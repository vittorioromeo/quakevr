// vr_cvars.inc -- every Quake VR cvar: QVR_CVAR(name, default, flags).
// Names carried over from the QSS-based Quake VR engine; defaults are the values the Quake VR
// releases shipped in id1/config.cfg.

// Gameplay rule changes (opt-in, enabled by quakevr/quakevr.cfg)
QVR_CVAR(vr_gameplayfix_droptofloor, "0", CVAR_NONE)  // corner traces instead of a box sweep
QVR_CVAR(vr_gameplayfix_touchsolids, "0", CVAR_NONE)  // .touch fires for solids, not just triggers
QVR_CVAR(vr_gameplayfix_missilesize, "0", CVAR_NONE)  // missile-vs-monster box extent (0: vanilla 15)
QVR_CVAR(vr_hull_width, "0", CVAR_ARCHIVE)             // Movement > Player Hitbox: the player's width against the map and brush models (8-32; 0: Quake's hull 1). vr_hull.cpp, docs/vr-port/HULLS.md
QVR_CVAR(vr_hull_method, "1", CVAR_ARCHIVE)            // how that is done: 0 the brush sweep (Quake 2's), 1 a clipping hull compiled for the width at load (qbsp's)
QVR_CVAR(vr_hull_ent_width, "-1", CVAR_ARCHIVE)        // the player's width against monsters, players and solid boxes (8-32; 0: Quake's 32; -1: vr_hull_width's)
QVR_CVAR(vr_hull_brushmodels, "1", CVAR_ARCHIVE)       // the narrow width against brush models too (doors, lifts, func_walls, external .bsp models), not only the world
QVR_CVAR(vr_hull_monsters, "1", CVAR_ARCHIVE)          // the narrow entity width between the player and monsters (both ways)
QVR_CVAR(vr_hull_players, "1", CVAR_ARCHIVE)           // ... between players
QVR_CVAR(vr_hull_boxes, "1", CVAR_ARCHIVE)             // ... between the player and other solid boxes (explosive boxes, solid items)
QVR_CVAR(vr_compat_muzzle, "1", CVAR_ARCHIVE)          // with another mod's progs, its shots start at the gun, not the player's head
QVR_CVAR(vr_gameplayfix_tossfall, "0", CVAR_NONE)     // resting items and weapons fall when their support goes
QVR_CVAR(vr_pickup_scale, "1", CVAR_NONE)             // weapons, armour, powerups and keys drawn smaller, on the floor
QVR_CVAR(vr_item_float_height, "26", CVAR_ARCHIVE)    // units above the floor the map's weapons, armour, powerups and keys float at (0: on the floor); next map
QVR_CVAR(vr_unstick, "0", CVAR_ARCHIVE)             // Debug > Tests > Unstick: a player found inside the map or a door, button or lift is moved to the nearest free spot (vr_unstick.cpp); 0: Quake's small nudge only

// Precise hit detection (vr_hitmodel.cpp; ROUND21 "Precise hit detection (models, not boxes)"): shots, missiles, the
// grappling hook, melee blows and thrown things meet a monster's model as drawn (grown by the tolerance), not Quake's box.
QVR_CVAR(vr_hit_precise, "1", CVAR_ARCHIVE)           // Combat > Hit Detection > Precise Hit Detection; 0: Quake's boxes, as before
QVR_CVAR(vr_hit_tolerance_guns, "4", CVAR_ARCHIVE)    // units the model is grown by for shots and projectiles (nails, rockets, grenades, lasers...)
QVR_CVAR(vr_hit_tolerance_grapple, "2", CVAR_ARCHIVE) // for the grappling hook
QVR_CVAR(vr_hit_tolerance_melee, "6", CVAR_ARCHIVE)   // for melee blows and shoves (on top of the striking points' own thickness, 3 units)
QVR_CVAR(vr_hit_tolerance_thrown, "2", CVAR_ARCHIVE)  // for thrown things (on top of their hit box, vr_throw_hitbox: 6)
QVR_CVAR(vr_debug_hits, "0", CVAR_NONE)               // Debug > Show Hits: each precise hit drawn (the model as drawn then, the triangle hit, the point on the model) and printed; 2: also the shots through a box that missed the model

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
QVR_CVAR(vr_throw_pitch, "0", CVAR_ARCHIVE)        // degrees every throw's direction is tilted up (negative: down), its speed kept
QVR_CVAR(vr_throw_gravity, "9.81", CVAR_ARCHIVE)   // m/s^2 on thrown weapons, bounces included (0: sv_gravity)
QVR_CVAR(vr_debug_throw, "0", CVAR_NONE)           // print each throw's estimate (2: and its timing)
QVR_CVAR(vr_debug_shots, "0", CVAR_NONE)           // print each hitscan shot: where it starts, its direction, what its pellets hit; each prop a shot pushes
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
// Thrown weapons and backpacks as rigid bodies (Box3D, vr_box3d.cpp): bounciness and friction of
// their contacts, their spin at most, and how fast the air slows it.
QVR_CVAR(vr_throw_restitution, "0.25", CVAR_ARCHIVE)
QVR_CVAR(vr_throw_friction, "0.6", CVAR_ARCHIVE)
QVR_CVAR(vr_throw_spin_max, "20", CVAR_ARCHIVE)    // rad/s
QVR_CVAR(vr_throw_spin_drag, "0.3", CVAR_ARCHIVE)  // per second, in the air

// Version of quakevr/vr_bindings.cfg last applied (vr_checkbindings).
QVR_CVAR(vr_bindings_version, "0", CVAR_ARCHIVE)

// Mock backend: swing the main hand in an overhand arc with this period, in seconds (0: still).
QVR_CVAR(vr_mock_swing, "0", CVAR_NONE)
// Mock backend: 1 reports the grip's (the palm's) velocity too, as the OpenXR backend does for a Touch controller with
// vr_controller_legacy_pose (throws then go with the palm, as in the headset); 0 (the default): the raw pose's only.
QVR_CVAR(vr_mock_grip_velocity, "0", CVAR_NONE)
// Test runs: 1 makes every host frame 1/72 s of game time with a server frame (vr_motion_play.cpp hostFrameTime), so a
// scripted run (vr_mock_play, "wait") logs the same numbers every time, whatever the machine's pace.
QVR_CVAR(vr_fixed_frames, "0", CVAR_NONE)
// Test runs: 1 runs the mock headset's frames as fast as the machine makes them while the game's clock doesn't follow the
// wall clock (vr_fixed_frames 1, a motion take played back not in watch mode): no frame cap, no vsync (VR_Unpaced). The
// same frames and the same numbers as at the frame cap's pace, in less time. Other runs keep their real-time pace.
QVR_CVAR(vr_mock_fast, "0", CVAR_NONE)

// Rigid-body physics: Box3D (docs/vr-port/ROUND21.md, "Box3D physics"; vr_box3d.cpp: props also collide with each
// other, so they stack and pile).
QVR_CVAR(vr_physics_engine, "1", CVAR_NONE)          // retired (Box3D is the only choice: ROUND21.md, "Simplification: Box3D only"): kept so saved configs load silently
QVR_CVAR(vr_box3d_substeps, "4", CVAR_ARCHIVE)       // Box3D's sub-steps a server frame (1..8)
QVR_CVAR(vr_box3d_player_push, "1", CVAR_ARCHIVE)    // the players' bodies push props (a capsule each, one way)
QVR_CVAR(vr_box3d_hand_push_speed, "3", CVAR_ARCHIVE) // m/s, the most a hand shoves a solid prop at (the rest of its move it passes into it, eased out)
QVR_CVAR(vr_box3d_hand_push, "1", CVAR_ARCHIVE)      // the players' hands push solid props (an explosive box) as bodies: a sphere at each fist, one way (vr_box3d.cpp syncHands)
QVR_CVAR(vr_box3d_hand_props, "1", CVAR_ARCHIVE)     // the players' empty hands are bodies (the open hand, or the fist) at full speed: they push loose props (not solid ones, not grenades) and hold them up (vr_box3d.cpp syncReach)
QVR_CVAR(vr_box3d_weapon_push, "1", CVAR_ARCHIVE)    // the players' held weapons are bodies (the drawn model's hull) at full speed: they push loose props (grenades too: batted) and hold them up
QVR_CVAR(vr_box3d_hand_hold_mass, "20", CVAR_ARCHIVE) // kg, the heaviest prop an open hand or a weapon holds up (heavier: it slips through, pushed still); 0: no limit
QVR_CVAR(vr_box3d_hand_mass, "3", CVAR_ARCHIVE)      // kg, what an empty hand pushes with (and each hand holding a prop, with its own mass): a prop keeps hand / (hand + its mass) of the kinematic push (vr_box3d.cpp pushShare); 0: no limit
QVR_CVAR(vr_box3d_push_force, "200", CVAR_ARCHIVE)   // N, the most an arm shoves a prop with once it has hit it (a hand's body pushing on: vr_box3d.cpp limitPushes); 0: no limit
QVR_CVAR(vr_box3d_weapon_arm_mass, "1", CVAR_ARCHIVE) // kg, the arm behind a held weapon: it pushes with this and its own Mass (Weapon Weights: the shotgun 3 kg, 4 in all; the rocket launcher 9; vr_box3d.cpp reachMass); this and vr_box3d_hand_mass 0: no limit
QVR_CVAR(vr_box3d_weapon_mass, "4", CVAR_NONE)       // retired (round 21, "Held props against weapons, monsters and walls": each weapon's own mass, vr_box3d_weapon_arm_mass): kept so saved configs load silently
QVR_CVAR(vr_shot_push, "1", CVAR_ARCHIVE)            // shots push the loose props they hit (QC VR_ShotPush): times every weapon's push below; 0: off
QVR_CVAR(vr_shot_push_pellet, "6", CVAR_ARCHIVE)     // N s, each shotgun and double shotgun pellet's push (monsters' too)
QVR_CVAR(vr_shot_push_nail, "12", CVAR_ARCHIVE)       // N s, a nail's (the nailgun's, scrags', knights', enforcers' lasers)
QVR_CVAR(vr_shot_push_supernail, "24", CVAR_ARCHIVE) // N s, a super nailgun nail's
QVR_CVAR(vr_shot_push_lightning, "6", CVAR_ARCHIVE)  // N s, each bolt of the lightning gun's beam (10 a second; a shambler's by its damage)
QVR_CVAR(vr_shot_push_speed, "10", CVAR_ARCHIVE)     // m/s, the most a shot makes what it hits go (light things: a gib, a small gun)
QVR_CVAR(vr_box3d_player_radius, "15", CVAR_ARCHIVE) // cm, that capsule's radius
QVR_CVAR(vr_box3d_player_push_speed, "2.5", CVAR_ARCHIVE) // m/s, the most a player's body shoves a prop at (the rest of its move it passes into it, eased out)
QVR_CVAR(vr_box3d_mesh_junctions, "1", CVAR_NONE)  // the world mesh with its T-junctions joined (no bumps at the seams between faces); 0: the faces as the BSP has them
QVR_CVAR(vr_debug_box3d, "0", CVAR_NONE)             // 1: bodies made, woken and put to sleep; 2: every awake body every frame

// Desktop mirror of the headset view: 0 off, 1 left eye, 2 both eyes side by side.
QVR_CVAR(vr_mirror, "1", CVAR_ARCHIVE)
// Recording (vr_window.cpp; ROUND21.md, "Recording: smoothed mirror and spectator camera"): what the window shows
// (vr_mirror 0 still turns it off). 0 the left eye as it is (vr_mirror 2: both eyes); 1 the smoothed mirror: the
// left eye's image turned to a steadied head orientation and cropped to the window; 2 the spectator camera: the
// scene rendered a third time for the window, from the head, steadied, with its own field of view.
QVR_CVAR(vr_window_view, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_window_smooth, "0.15", CVAR_ARCHIVE)      // seconds: the steadied orientation's lag (0 none .. 0.5); both modes
QVR_CVAR(vr_window_level, "0", CVAR_ARCHIVE)          // 0 .. 1: how much of the head's roll is taken out (1: the horizon level); both modes
QVR_CVAR(vr_window_zoom, "1.2", CVAR_ARCHIVE)         // smoothed mirror: 1 the widest crop of the eye's image .. 2 (the margin the steadying turns in)
QVR_CVAR(vr_spectator_fov, "90", CVAR_ARCHIVE)        // spectator camera: horizontal field of view, degrees (40 .. 130)
QVR_CVAR(vr_spectator_scale, "0.75", CVAR_ARCHIVE)    // spectator camera: its resolution, times the window's (0.25 .. 2)
QVR_CVAR(vr_spectator_rate, "60", CVAR_ARCHIVE)        // spectator camera: drawn every frame (1), every 2nd or 3rd (2, 3), or at most this many times a second (more than 3); the window shows the last between
QVR_CVAR(vr_spectator_aa, "1", CVAR_ARCHIVE)           // spectator camera: 1 the window's MSAA (vid_fsaa), as the eyes; 0 none
QVR_CVAR(vr_spectator_pos_smooth, "0.05", CVAR_ARCHIVE) // spectator camera: seconds, the steadied position's lag (0 none .. 0.3)
QVR_CVAR(vr_window_log, "0", CVAR_NONE)               // 1: print the head's and the window camera's angles every frame (tests)

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
// Mock backend: its eye images' size (square, 256..2048; read when the images are made, before the first frame in VR).
QVR_CVAR(vr_mock_eye_size, "1024", CVAR_NONE)
// Mock backend: a shaky head (recording tests): small quick turns of this many degrees (pitch, yaw and roll, sums of
// sines at 5 to 13 Hz) over a slow turn of vr_mock_shake_turn degrees a second (from when it starts), both from the head's vr_mock_look.
QVR_CVAR(vr_mock_shake, "0", CVAR_NONE)
QVR_CVAR(vr_mock_shake_turn, "0", CVAR_NONE)
// Upscaling and foveated rendering (round 20; vr_upscale.cpp, vr_foveated.cpp, docs/vr-port/ROUND20.md).
QVR_CVAR(vr_upscale, "1", CVAR_ARCHIVE)                // below vr_render_scale 1, the upscale to the headset's size: 0 bilinear, 1 AMD FSR 1 (EASU + RCAS), 2 NVIDIA Image Scaling
QVR_CVAR(vr_upscale_sharpness, "0.5", CVAR_ARCHIVE)    // ... its sharpening, 0 none .. 1 most (FSR: RCAS 2 stops below its most at 0.1, its most at 1; NIS: its slider)
QVR_CVAR(vr_upscale_radius, "40", CVAR_ARCHIVE)        // ... only within this many degrees of the lens centre, bilinear beyond (0: the whole image)
QVR_CVAR(vr_upscale_sharpen_native, "0", CVAR_ARCHIVE) // at vr_render_scale 1: FSR's RCAS sharpening alone (with vr_upscale 1 or 2 and some sharpness)
QVR_CVAR(vr_foveated, "0", CVAR_ARCHIVE)               // fixed foveated rendering (NVIDIA variable-rate shading): 0 off, 1 conservative, 2 balanced, 3 aggressive
QVR_CVAR(vr_foveated_inner, "0", CVAR_ARCHIVE)         // ... full-rate shading within this many degrees of the lens centre (0: the preset's: 45, 35, 25)
QVR_CVAR(vr_foveated_outer, "0", CVAR_ARCHIVE)         // ... 2x2 up to this many degrees, 4x4 beyond (0: the preset's: 60, 50, 40)
QVR_CVAR(vr_foveated_debug, "0", CVAR_NONE)            // show the shading rates (yellow 2x2, red 4x4) and vr_upscale_radius (cyan) in the eyes and the mirror

QVR_CVAR(vr_enabled, "0", CVAR_NONE)
QVR_CVAR(vr_lefthanded, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_fakevr_handroll, "0", CVAR_NONE)
QVR_CVAR(vr_crosshair, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_crosshair_depth, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_crosshair_size, "1", CVAR_ARCHIVE)
QVR_CVAR(vr_crosshair_alpha, "0.85", CVAR_ARCHIVE)
QVR_CVAR(vr_crosshair_hue, "-1", CVAR_ARCHIVE)        // its colour: hue (degrees; 0 red; -1 vr_player_hue)
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
QVR_CVAR(vr_melee_speed, "4", CVAR_ARCHIVE)          // m/s the striking hand must reach (relative to the head) to hit: a punch, a pommel strike; a weapon's swing 1.25x, a stab 0.75x; twice this is a strength-1 blow (QC vr_melee.qc)
QVR_CVAR(vr_melee_punch_mult, "1.25", CVAR_ARCHIVE)   // damage of every punch (a fist's base damage times this)
QVR_CVAR(vr_melee_wrist_speed, "1.1", CVAR_ARCHIVE)   // m/s the hand itself (its wrist: the hand's turn taken out) must have moved on average over the last 0.12 s to hit (a punch twice this): a wiggle flicks the tip fast but barely moves the wrist (QC vr_melee.qc)
QVR_CVAR(vr_melee_pommel_wait, "0.05", CVAR_ARCHIVE)  // s a pommel's (a handle's end's, a gun butt's) touch waits for the blade or head to reach the same thing, so a cut from close by is a cut; only pommel strikes wait (QC vr_melee.qc)
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
QVR_CVAR(vr_gadget_screen_hue, "-1", CVAR_ARCHIVE)    // the screen's colour (and the ammo screens', the wrist log's): hue (degrees; 128 green, 40 amber, 200 blue; -1 vr_player_hue),
QVR_CVAR(vr_gadget_screen_brightness, "1", CVAR_ARCHIVE) // its text and lines' brightness,
QVR_CVAR(vr_gadget_screen_background, "1", CVAR_ARCHIVE) // and its background's
QVR_CVAR(vr_gadget_show_level, "1", CVAR_ARCHIVE)     // the level's name, kills and secrets on the screen
QVR_CVAR(vr_gadget_stamina, "1", CVAR_ARCHIVE)        // the screen's top row shows stamina (with parry, shove or strike stamina on: vr_parry_stamina, vr_shove_stamina, vr_strike_stamina) and flashes COUNTER while a counter's window is open (vr_meleehud.cpp)
QVR_CVAR(vr_gadget_light, "1", CVAR_ARCHIVE)          // the screen casts a light in its colour the way it faces: strength (0 off)
QVR_CVAR(vr_gadget_crt, "1", CVAR_ARCHIVE)            // the screen looks like a small CRT (scanlines, a flicker, faint static, now and then a glitch): strength (0 off, up to 2)
QVR_CVAR(vr_player_hue, "128", CVAR_ARCHIVE)          // the player's colour (vr_hue.hpp): the hue (degrees; 128 green, 40 amber, 200 blue) of the gadget's and weapons' screens, the sights, the force grab, the teleport arc, the crosshair, the menu laser (each: -1 follows it),
QVR_CVAR(vr_player_saturation, "1", CVAR_ARCHIVE)     // and how saturated they are (1 as made, 0 white, up to 2)
QVR_CVAR(vr_sight_hue, "-1", CVAR_ARCHIVE)            // the glowing iron sights of the shotgun and double shotgun (vr_sights.cpp): hue (degrees; 30 their own orange, 0 red, 120 green, 240 blue; -1 vr_player_hue),
QVR_CVAR(vr_sight_saturation, "1", CVAR_ARCHIVE)      // and saturation (1 their own, 0 white, up to 2)
QVR_CVAR(vr_notify_wrist, "1", CVAR_ARCHIVE)          // the console's messages (notify lines) with the gadget: 0 in front of the head, 1 in a log floating over the gadget, 2 both
QVR_CVAR(vr_notify_wrist_time, "8", CVAR_ARCHIVE)     // seconds a message stays in that log (0: con_notifytime's)
QVR_CVAR(vr_notify_wrist_height, "5", CVAR_ARCHIVE)   // cm the log's bottom floats over the gadget as seen (and always over the hologram)
QVR_CVAR(vr_notify_wrist_alpha, "0.7", CVAR_ARCHIVE)  // how bright the log is (its lines are the engine's: dimmer than the game's messages)
QVR_CVAR(vr_messages_hologram, "1", CVAR_ARCHIVE)     // the game's messages (centre prints: a key needed, a secret found, the map's text; the server's prints: pickups, deaths, chat) as a hologram the wrist gadget projects over its screen while it faces you (else the centre prints show in view as before); the engine's lines stay in the log
QVR_CVAR(vr_messages_hologram_time, "5", CVAR_ARCHIVE) // seconds a message stays in it
QVR_CVAR(vr_messages_hologram_size, "1", CVAR_ARCHIVE) // its text's size (1: about 8 mm characters)
QVR_CVAR(vr_messages_hologram_height, "1", CVAR_ARCHIVE) // cm its bottom floats over the gadget as seen
QVR_CVAR(vr_messages_hologram_effect, "1", CVAR_ARCHIVE) // the hologram look: the beam from the screen, scanlines, flicker, glitches, the projection as it appears (0 plain glowing text, up to 2)
QVR_CVAR(vr_messages_hologram_only, "0", CVAR_ARCHIVE) // QVR round 21: 1: the game's messages never show in front of you; they wait in the hologram until you look at the gadget, and a new one (not a pickup) chimes from the gadget and buzzes its hand while it is out of view
QVR_CVAR(vr_sbar_offset_x, "-12", CVAR_ARCHIVE)
QVR_CVAR(vr_sbar_offset_y, "1", CVAR_ARCHIVE)
QVR_CVAR(vr_sbar_offset_z, "-3", CVAR_ARCHIVE)
QVR_CVAR(vr_sbar_offset_pitch, "1", CVAR_ARCHIVE)
QVR_CVAR(vr_sbar_offset_yaw, "1.6", CVAR_ARCHIVE)
QVR_CVAR(vr_sbar_offset_roll, "-0.3", CVAR_ARCHIVE)
QVR_CVAR(vr_height_calibration, "1.646099", CVAR_ARCHIVE)
QVR_CVAR(vr_menu_distance, "80", CVAR_ARCHIVE)
QVR_CVAR(vr_menu_vr_style, "1", CVAR_ARCHIVE)          // menus made for the headset (vr_menuui.cpp): filling the panel, rows spaced out, modern sliders and switches, a laser pointer
QVR_CVAR(vr_menu_laser_hue, "-1", CVAR_ARCHIVE)        // that laser's colour: hue (degrees; 35 amber; -1 vr_player_hue)
QVR_CVAR(vr_menu_spacing, "1.5", CVAR_ARCHIVE)         // with it, the rows' spacing over Quake's (1 to 2)
QVR_CVAR(vr_menu_height, "1.35", CVAR_ARCHIVE)         // with it, the menus' height over Quake's 200 pixels (1 to 2): the VR pages and Ironwail's lists show more rows (1.35: about 35 degrees up and down at the shipped distance, scale and spacing)
QVR_CVAR(vr_menu_remember, "1", CVAR_ARCHIVE)          // opening the menu after "Back to game" (the panel's top-left button, or holding the menu button) returns to the page it closed on
QVR_CVAR(vr_menu_positions, "", CVAR_ARCHIVE)          // where each VR Settings page was left, for the next start (vr_menu.cpp, written with the config): "title|section|row|index|line;..."
QVR_CVAR(vr_checklist_hide_ticked, "0", CVAR_ARCHIVE)   // Checklist page (the menu's corner button): ticked items left out
QVR_CVAR(vr_melee_dmg_multiplier, "1.0", CVAR_ARCHIVE)
QVR_CVAR(vr_melee_range_multiplier, "1.0", CVAR_ARCHIVE)
// Quad Damage on melee, on top of the x4 every damage gets while Quad lasts (T_Damage): extra damage and extra reach.
// Quake VR v0.0.3 had x2.75 damage (11x in all) and 1.35/1.10 reach; 1 = melee is like any other weapon under Quad.
QVR_CVAR(vr_quad_melee_damage, "1", CVAR_ARCHIVE)
QVR_CVAR(vr_quad_melee_range, "1", CVAR_ARCHIVE)
QVR_CVAR(vr_body_interactions, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_notes, "1", CVAR_ARCHIVE)                  // voice notes: off hand at the mouth, hold Y to talk (vr_voicenotes.cpp)
QVR_CVAR(vr_note_device, "Virtual Desktop", CVAR_ARCHIVE) // the microphone: part of its name (none such, or empty: the default; vr_note_devices lists them)
// The motion recorder (vr_motion.cpp, docs/vr-port/MOTIONS.md): takes of the player's motions for tuning the melee.
QVR_CVAR(vr_motion_armed, "0", CVAR_NONE)          // 1: the record button (vr_motion_button) starts and ends takes of the chosen category
QVR_CVAR(vr_motion_category, "0", CVAR_ARCHIVE)    // the category recorded, named by the result expected (an index: VR Settings > Advanced > Motion Recorder)
QVR_CVAR(vr_motion_detail, "0", CVAR_ARCHIVE)      // and its optional detail (an index into the category's; 0: none)
QVR_CVAR(vr_motion_note, "", CVAR_NONE)            // a free note written into each take's header (the "other" label's description)
QVR_CVAR(vr_motion_button, "0", CVAR_ARCHIVE)      // the record button: 0 the off hand's stick click, 1 the main hand's
QVR_CVAR(vr_motion_preroll, "0.5", CVAR_ARCHIVE)   // seconds kept before the button (phase "pre": the lead-in, for the melee's trackers)
QVR_CVAR(vr_motion_tail, "0.3", CVAR_ARCHIVE)      // seconds recorded after the button is let go (phase "tail": late events)
// Reviewing the takes (vr_motion_review.cpp, MOTIONS.md "Reviewing failing takes"): VR Settings > Advanced > Review Takes.
QVR_CVAR(vr_motion_review_show, "0", CVAR_ARCHIVE)      // the takes listed: 0 to review (failing or suspect, not reviewed), 1 failing, 2 suspect, 3 not evaluated, 4 reviewed, 5 all, 6 discarded
QVR_CVAR(vr_motion_review_category, "-1", CVAR_ARCHIVE) // and of which category (-1 all; else vr_motion_category's index)
QVR_CVAR(vr_motion_review_speed, "0.5", CVAR_ARCHIVE)   // the ghost replay's pace (1: as recorded)
QVR_CVAR(vr_motion_relabel_category, "0", CVAR_NONE)    // Relabel: the category (vr_motion_category's index) and detail a take is given
QVR_CVAR(vr_motion_relabel_detail, "0", CVAR_NONE)
QVR_CVAR(vr_memstats_log_gpu, "0", CVAR_ARCHIVE)  // 1: the memory log's GPU columns (clocks, slowdowns, each program's GPU use: a sampling thread) always; 0: only while profiling (vr_profile, vr_profile_overlay, vr_profile_csv)
QVR_CVAR(vr_memstats_log, "60", CVAR_ARCHIVE)      // seconds between rows of quakevr/profile/memstats_<date>.csv (and one after each map load; 0: off)
QVR_CVAR(vr_gadget_fps, "0", CVAR_ARCHIVE)         // QVR round 21: an FPS counter floating just under the wrist gadget: 1 basic (frames a second, the CPU's and the GPU's milliseconds a frame: the memory log's busy_ms and gpu_eyes_ms, over half a second), 2 detailed (now, a quarter second's average, the last 5 s's least and most, frames that missed a refresh, 3 s graphs against the budget)
QVR_CVAR(vr_lean_radius, "12", CVAR_ARCHIVE)          // units the head may lean from the middle of the player's box before the body follows (0: always above it)
QVR_CVAR(vr_lean_recenter, "0.5", CVAR_ARCHIVE)       // metres/second the body slides back under the head where it can (0: never; with vr_lean_detect, faster when further off, and not while leaning)
QVR_CVAR(vr_lean_detect, "1", CVAR_ARCHIVE)          // tells a lean (the head lower and tilted towards it, the hands left by the hips) from walking in the room: while leaning the body stays where it stands (0: off, the body always slides back under the head; above 1, more readily a lean, up to 2)
QVR_CVAR(vr_roomscale_jump, "1", CVAR_ARCHIVE)          // jumping in the room jumps in the game: the head rising faster than
QVR_CVAR(vr_roomscale_jump_threshold, "0.8", CVAR_ARCHIVE) // this (m/s) above the standing height
QVR_CVAR(vr_roomscale_move_mult, "1.0", CVAR_ARCHIVE)
QVR_CVAR(vr_teleport_enabled, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_teleport_range, "400", CVAR_ARCHIVE)
QVR_CVAR(vr_teleport_hue, "-1", CVAR_ARCHIVE)         // the aiming arc's colour where it can land: hue (degrees; 220 blue; -1 vr_player_hue; red where it can't)
QVR_CVAR(vr_2h_mode, "2", CVAR_ARCHIVE)
QVR_CVAR(vr_2h_angle_threshold, "0.65", CVAR_ARCHIVE)
QVR_CVAR(vr_virtual_stock_thresh, "10", CVAR_ARCHIVE)
QVR_CVAR(vr_show_virtual_stock, "0", CVAR_NONE)
QVR_CVAR(vr_shoulder_offset_x, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_shoulder_offset_y, "1.75", CVAR_ARCHIVE)
QVR_CVAR(vr_shoulder_offset_z, "16.0", CVAR_ARCHIVE)
QVR_CVAR(vr_2h_virtual_stock_factor, "0.5", CVAR_ARCHIVE)
// Weight (vr_weight.cpp; ROUND21.md, "Weight: spring model, stamina, held object offsets; explosive boxes" and "Spring
// only; Weapon Weights and Held Object Weights; weight and damage"): what a hand holds (a weapon, a carried prop, the
// flashlight) follows it through a damped spring, by its mass, balance and length. The Spring is the only model.
QVR_CVAR(vr_wpn_pos_weight, "1", CVAR_NONE) // retired (round 21, "Spring only": the Speed Limit weight model is gone): kept so saved configs load silently
QVR_CVAR(vr_wpn_pos_weight_offset, "0.0", CVAR_NONE) // retired (round 21, "Spring only": the Speed Limit weight model is gone): kept so saved configs load silently
QVR_CVAR(vr_wpn_pos_weight_mult, "1.0", CVAR_NONE) // retired (round 21, "Spring only": the Speed Limit weight model is gone): kept so saved configs load silently
QVR_CVAR(vr_wpn_pos_weight_2h_help_offset, "0.3", CVAR_NONE) // retired (round 21, "Spring only": the Speed Limit weight model is gone): kept so saved configs load silently
QVR_CVAR(vr_wpn_pos_weight_2h_help_mult, "1.0", CVAR_NONE) // retired (round 21, "Spring only": the Speed Limit weight model is gone): kept so saved configs load silently
QVR_CVAR(vr_wpn_dir_weight, "1", CVAR_NONE) // retired (round 21, "Spring only": the Speed Limit weight model is gone): kept so saved configs load silently
QVR_CVAR(vr_wpn_dir_weight_offset, "0.05", CVAR_NONE) // retired (round 21, "Spring only": the Speed Limit weight model is gone): kept so saved configs load silently
QVR_CVAR(vr_wpn_dir_weight_mult, "1.0", CVAR_NONE) // retired (round 21, "Spring only": the Speed Limit weight model is gone): kept so saved configs load silently
QVR_CVAR(vr_wpn_dir_weight_2h_help_offset, "0.3", CVAR_NONE) // retired (round 21, "Spring only": the Speed Limit weight model is gone): kept so saved configs load silently
QVR_CVAR(vr_wpn_dir_weight_2h_help_mult, "1.0", CVAR_NONE) // retired (round 21, "Spring only": the Speed Limit weight model is gone): kept so saved configs load silently
QVR_CVAR(vr_weight_model, "1", CVAR_NONE) // retired (round 21, "Spring only": the Speed Limit weight model is gone): kept so saved configs load silently
QVR_CVAR(vr_weight_props, "1", CVAR_NONE) // retired (round 21, "Spring only": carried props always weigh their mass; Held Object Weights tunes each): kept so saved configs load silently
QVR_CVAR(vr_weight_spring_stiffness, "1", CVAR_ARCHIVE)  // Spring: times the arm's and wrist's stiffness (2: 41% quicker)
QVR_CVAR(vr_weight_spring_damping, "1", CVAR_ARCHIVE)    // Spring: the damping ratio (1 critical: settles without swinging back; less: overshoots)
QVR_CVAR(vr_weight_spring_strength, "1", CVAR_ARCHIVE)   // Spring: times the arm's force and the wrist's torque (0: unlimited)
QVR_CVAR(vr_weight_spring_sag, "1", CVAR_ARCHIVE)        // Spring: times the droop of heavy things under gravity (more at arm's length)
QVR_CVAR(vr_weight_spring_inertia, "1", CVAR_ARCHIVE)    // Spring: times the swing a moving hand gives what it holds (its centre of mass trailing)
QVR_CVAR(vr_weight_spring_2h, "4", CVAR_ARCHIVE)         // Spring: times the stiffness and strength with a two-handed grip
QVR_CVAR(vr_weight_spring_snap, "60", CVAR_ARCHIVE)      // Spring: cm (or 3 times as many degrees) off the hand beyond which it is put back in it at once
QVR_CVAR(vr_weight_stamina, "1", CVAR_ARCHIVE)           // tired, things weigh more: below vr_weight_stamina_from of the stamina (Parry Stamina) the weight grows
QVR_CVAR(vr_weight_stamina_from, "0.5", CVAR_ARCHIVE)    // the stamina left (0..1) below which it starts
QVR_CVAR(vr_weight_stamina_max, "2.5", CVAR_ARCHIVE)     // times the weight with none left
QVR_CVAR(vr_weight_stamina_curve, "2", CVAR_ARCHIVE)     // the curve's exponent from the threshold to none (1 straight; 2 gentle at first)
QVR_CVAR(vr_weight_stamina_add, "15", CVAR_ARCHIVE)      // kg added to what a hand holds with none left (on the same curve; with the times above)
QVR_CVAR(vr_weight_stamina_empty, "15", CVAR_ARCHIVE)    // kg an empty hand weighs with none left (on the same curve): it follows its controller as if it held that (0: an empty hand never lags; not a hand on a hold or steadying a weapon)
QVR_CVAR(vr_stamina_speed, "1", CVAR_ARCHIVE)           // tired, you run slower: the most walking speed (sv_maxspeed) drops on the weight's curve (vr_weight_stamina_from, _curve)
QVR_CVAR(vr_stamina_speed_min, "0.5", CVAR_ARCHIVE)     // times the most speed with no stamina left (1: no slower)
QVR_CVAR(vr_debug_stamina_speed, "0", CVAR_NONE)        // tests: 1 prints the stamina, the speed cap and the player's ground speed twice a second (a local game)
QVR_CVAR(vr_weight_throw_mass, "12", CVAR_ARCHIVE)       // kg: props up to this heavy are thrown with the hand's whole speed, heavier ones slower (Throw: Held Object Offsets)
// Weight and damage (round 21, "Spring only; Weapon Weights and Held Object Weights; weight and damage"): a melee blow's
// and a throw's damage times (mass / vr_weight_damage_heavy)^vr_weight_damage_exp for things heavier than it,
// (mass / vr_weight_damage_light)^exp for lighter ones, 1 between (every weapon), within vr_weight_damage_min..max; the
// empty hand (no mass) 1. Set both masses to one to have a single reference.
QVR_CVAR(vr_weight_damage_exp, "0.4", CVAR_ARCHIVE)      // the curve's exponent (0: mass changes no damage; 1: in proportion)
QVR_CVAR(vr_weight_damage_heavy, "8", CVAR_ARCHIVE)      // kg: heavier things deal more damage
QVR_CVAR(vr_weight_damage_light, "1.5", CVAR_ARCHIVE)    // kg: lighter things deal less damage
QVR_CVAR(vr_weight_damage_min, "0.5", CVAR_ARCHIVE)      // the least multiplier
QVR_CVAR(vr_weight_damage_max, "2.5", CVAR_ARCHIVE)      // the most multiplier
// Heavy leniency: things heavier than vr_weight_lenient_from kg strike and hit when thrown at lower speeds: every speed
// threshold of a melee strike and of a throw's hit times (from / mass)^vr_weight_lenient, at least vr_weight_lenient_min.
QVR_CVAR(vr_weight_lenient, "0.75", CVAR_ARCHIVE)        // the exponent (0: off)
QVR_CVAR(vr_weight_lenient_from, "10", CVAR_ARCHIVE)     // kg: lighter things (every weapon) keep the thresholds
QVR_CVAR(vr_weight_lenient_min, "0.25", CVAR_ARCHIVE)    // the least factor
QVR_CVAR(vr_debug_weight_stamina, "-1", CVAR_NONE)       // tests: the stamina the weight takes as left (0..1) instead of the game's (-1: the game's)
QVR_CVAR(vr_debug_stamina_hold, "0", CVAR_NONE)          // tests: 1 keeps the game's stamina where it is, or where vr_stamina_set put it (a local game; vr_fatigue.cpp)
QVR_CVAR(vr_debug_fatigue, "0", CVAR_NONE)               // 1: the tired arms' shake printed four times a second; 2: and each frame the aim, the muzzle and the drawn weapon ("fatigueaim")
QVR_CVAR(vr_debug_weight, "0", CVAR_NONE)                // 1: each hand's target and drawn pose, a line a frame in weight_trace.txt; 2: and printed
QVR_CVAR(vr_props_version, "48", CVAR_ARCHIVE)           // the held object settings' changes the config has taken (vr_props.cpp)
// Explosive boxes (misc_explobox, misc_explobox2) as physics props.
QVR_CVAR(vr_explobox_physics, "1", CVAR_ARCHIVE)         // 1: pushed, tipped, carried by hand (never force grabbed); 0: fixed in place (id's)
QVR_CVAR(vr_explobox_impact, "14", CVAR_ARCHIVE)         // m/s: a box hitting something this hard is damaged, more the harder (0: never): dropped from 85 units or more (Quake's gravity); tipped over, its top lands at 11
// Rocks and bricks lying about the maps (vr_debris.cpp, QC vr_debris.qc): placed at map load, at the foot of walls.
QVR_CVAR(vr_debris, "1", CVAR_ARCHIVE)                   // 1: rocks and bricks where the textures say (next map; single player); 0: none
QVR_CVAR(vr_debris_chance, "0.1", CVAR_ARCHIVE)          // the chance a spot at a wall's foot gets a piece (or a few), every 16 units along it, before the limits
QVR_CVAR(vr_debris_dark, "0.3", CVAR_ARCHIVE)            // times the chance where the floor is dark (its lightmap under 8; full chance from 40 up)
QVR_CVAR(vr_debris_corner, "2.5", CVAR_ARCHIVE)          // times the chance in a corner (a wall beside the spot along the wall)
QVR_CVAR(vr_debris_cluster, "3", CVAR_ARCHIVE)           // most pieces together at one spot (1: always alone)
QVR_CVAR(vr_debris_max, "160", CVAR_ARCHIVE)             // most pieces in a map
QVR_CVAR(vr_debris_area_max, "8", CVAR_ARCHIVE)          // most pieces in an area (vr_debris_area_size units square)
QVR_CVAR(vr_debris_area_size, "384", CVAR_ARCHIVE)       // units: the side of an area
QVR_CVAR(vr_debris_spacing, "56", CVAR_ARCHIVE)          // units between spots (the pieces of one spot lie together)
QVR_CVAR(vr_debris_size, "0.15", CVAR_ARCHIVE)           // a piece's size varies this much either way (bricks 40% of it: they are made to a size)
QVR_CVAR(vr_debris_rocks, "1", CVAR_ARCHIVE)             // rocks (natural floors, rock and stone walls)
QVR_CVAR(vr_debris_bricks, "1", CVAR_ARCHIVE)            // bricks (brick walls)
QVR_CVAR(vr_debris_seed, "0", CVAR_ARCHIVE)              // mixed with the map's name: another layout (the same each load)
QVR_CVAR(vr_debris_exclude, "vrfiringrange vrclimb vrexample", CVAR_ARCHIVE) // maps with none (names, spaces between)
QVR_CVAR(vr_debris_edicts_left, "2048", CVAR_ARCHIVE)    // entities left free for the game at least (max_edicts less the map's): fewer pieces if needed
QVR_CVAR(vr_debug_debris, "0", CVAR_NONE)                // 1: a line per map (pieces, spots, time, the layout's hash); 2: and each piece; 3: and each spot rejected, why
// Wall torches taken off their walls (QC vr_walltorch.qc, vr_walltorch.cpp; ROUND21.md, "Wall torches you can take").
QVR_CVAR(vr_walltorch, "1", CVAR_ARCHIVE)                // 1: id's wall torches can be pulled out (by hand or force grab) and held as a burning club; 0: fixed, as id's (next map)
QVR_CVAR(vr_walltorch_pull, "8", CVAR_ARCHIVE)           // cm a hand gripping a torch on its wall pulls it before it comes out
QVR_CVAR(vr_walltorch_forcegrab, "1", CVAR_ARCHIVE)      // 1: torches are force grabbed off their walls, and lying about
QVR_CVAR(vr_walltorch_hits, "5", CVAR_ARCHIVE)           // blows a torch lands before its fire starts dying, even held (0: blows never use it up)
QVR_CVAR(vr_walltorch_die_time, "6", CVAR_ARCHIVE)       // s a dying torch's fire takes to go out (dropped, thrown, or out of blows); held, it never dies otherwise
QVR_CVAR(vr_walltorch_damage, "12", CVAR_ARCHIVE)        // a torch's blow: its base damage, times the blow's strength (a gun's is 12, the axe's 20)
QVR_CVAR(vr_walltorch_burn, "4", CVAR_ARCHIVE)           // damage a second a lit torch's blow sets burning (0: no burning)
QVR_CVAR(vr_walltorch_burn_time, "3", CVAR_ARCHIVE)      // s it burns (a monster catches fire)
QVR_CVAR(vr_walltorch_relight, "1", CVAR_ARCHIVE)        // 1: a dying or burnt-out torch held in a burning torch's flame or in lava lights again
QVR_CVAR(vr_walltorch_flame, "1", CVAR_ARCHIVE)          // a taken torch's flame's size (1: the wall torch's)
QVR_CVAR(vr_walltorch_shadows, "1", CVAR_ARCHIVE)        // 1: a taken torch's light casts shadows (one of vr_shadow_dlights' slots), whatever vr_torch_light_shadows says
QVR_CVAR(vr_offhandpitch, "40.25", CVAR_ARCHIVE)
QVR_CVAR(vr_offhandyaw, "-4", CVAR_ARCHIVE)
// Hand calibration (Hand/Gun Calibration > Hand Calibration; hands::calibration): each hand's place on its controller,
// so that the drawn hand sits where the real one does. Centimetres along the controller's grip axes (x along the handle,
// y left, z up: Show Controller's red, green, blue) and a roll (degrees, right side down) about where the hand points,
// through the grip; the pitch and yaw are vr_gunangle/vr_gunyaw (vr_offhandpitch/vr_offhandyaw). The off hand's own, or
// with vr_handcal_off_mirror the main hand's mirrored (y, yaw and roll the other way). 0: the hand as before.
QVR_CVAR(vr_handcal_x, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_handcal_y, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_handcal_z, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_handcal_roll, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_handcal_off_mirror, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_handcal_off_x, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_handcal_off_y, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_handcal_off_z, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_handcal_off_roll, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_show_hip_holsters, "0", CVAR_NONE)
QVR_CVAR(vr_hip_offset_x, "-3.5", CVAR_ARCHIVE)
QVR_CVAR(vr_hip_offset_y, "7.0", CVAR_ARCHIVE)
QVR_CVAR(vr_hip_offset_z, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_hip_holster_thresh, "6.5", CVAR_ARCHIVE)
QVR_CVAR(vr_holster_leg_follow, "0.4", CVAR_ARCHIVE) // hip holsters ride the legs' animation (full body): 0 fixed on the body, 1 all the way with the thighs
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
// Each pair of holsters turned (the Hotspots menu), degrees, with the gun in it, about the holster's position (where
// the hand reaches for it), in the body's frame there (vr_view.cpp turnHolster), mirrored for the left one as the
// offsets' Y is: pitch tips its top away from the body, yaw turns its face outwards, roll tips its top outwards (a
// hanging gun's muzzle in). 0 0 0: as before.
QVR_CVAR(vr_hip_holster_pitch, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_hip_holster_yaw, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_hip_holster_roll, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_upper_holster_pitch, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_upper_holster_yaw, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_upper_holster_roll, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_shoulder_holster_pitch, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_shoulder_holster_yaw, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_shoulder_holster_roll, "0", CVAR_ARCHIVE)
// The grenade pouch (vr_handgrenade; docs/vr-port/ROUND21.md, "Hand grenades from the back pouch"): on the belt at the
// small of the back. X forward (negative: back), units: with the body drawn, the default (-7) is on the back and X moves
// it from there (forward, it goes round the hips); Y to the right; Z up (times the height calibration, as the holsters').
QVR_CVAR(vr_show_grenade_pouch, "0", CVAR_NONE)
QVR_CVAR(vr_grenade_pouch_x, "-7", CVAR_ARCHIVE)
QVR_CVAR(vr_grenade_pouch_y, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_grenade_pouch_z, "3", CVAR_ARCHIVE)
QVR_CVAR(vr_grenade_pouch_thresh, "7", CVAR_ARCHIVE)  // units a hand must be within to be at it
QVR_CVAR(vr_grenade_pouch_pitch, "0", CVAR_ARCHIVE)  // degrees: its top away from the back (the holsters' axes)
QVR_CVAR(vr_grenade_pouch_yaw, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_grenade_pouch_roll, "0", CVAR_ARCHIVE)
// A hand grenade's turn in the hand as it is taken from the pouch (vr_grip.cpp serverFromPouch): on top of its grip
// (Held Object Offsets), about its middle, in the hand's frame (pitch up, yaw left, roll right; mirrored for the left hand).
QVR_CVAR(vr_grenade_pouch_hold_pitch, "-180", CVAR_ARCHIVE)
QVR_CVAR(vr_grenade_pouch_hold_yaw, "90", CVAR_ARCHIVE)
QVR_CVAR(vr_grenade_pouch_hold_roll, "0", CVAR_ARCHIVE)
// The player's body (vr_avatar.cpp): 0 none, 2 torso and arms, 3 torso, arms and legs: the skinned
// progs/vrbody model, posed by IK from the head and hands (1, the old floating torso, is now 2).
QVR_CVAR(vr_body_mode, "2", CVAR_ARCHIVE)
QVR_CVAR(vr_body_build, "1", CVAR_ARCHIVE)            // 0 lean, 1 athletic, 2 brawny (progs/vrbody_lean, vrbody, vrbody_brawny)
QVR_CVAR(vr_body_walk, "1", CVAR_ARCHIVE)             // legs (vr_body_mode 3) walk as the player moves
QVR_CVAR(vr_body_step_rate, "2.2", CVAR_ARCHIVE)      // at most this many steps a second at full running speed (fewer walking)
QVR_CVAR(vr_body_wade, "1", CVAR_ARCHIVE)             // wading, the legs walk heavier: shorter, higher, slower steps (0 as on land, up to 2)
QVR_CVAR(vr_body_swim_kick, "1", CVAR_ARCHIVE)        // swimming, how wide the legs kick (0: they only trail behind; up to 2)
QVR_CVAR(vr_body_swim_kick_rate, "1.5", CVAR_ARCHIVE) // and how many more kicks a second at full stick (0.7 treading water)
QVR_CVAR(vr_body_turn_step, "40", CVAR_ARCHIVE)       // degrees the body turns over its planted feet before they step round
QVR_CVAR(vr_body_state, "1", CVAR_ARCHIVE)            // the body shows the armour worn and, bloodier, the damage taken
QVR_CVAR(vr_body_powerups, "1", CVAR_ARCHIVE)         // and the powerups: quad damage arcs, pentagram glow, ring translucency, biosuit tint
QVR_CVAR(vr_body_blood, "1", CVAR_ARCHIVE)            // the wounded arms and hands drip blood (vr_body_state's wounds; above 1, more drops)
QVR_CVAR(vr_wounds, "1", CVAR_ARCHIVE)                // blood painted on models where they are hit (vr_wounds.cpp); your body and hands take it instead of the wound skins
QVR_CVAR(vr_wounds_burns, "1", CVAR_ARCHIVE)          // burns and char painted on models (explosions, fire, lightning, lava, slime), embers while fresh
QVR_CVAR(vr_wounds_wet, "1", CVAR_ARCHIVE)            // models wet up to where water or slime came, drying (and dripping) in about 25 s
QVR_CVAR(vr_wounds_pool, "64", CVAR_ARCHIVE)          // how many models keep their wounds at once (256 KB each; the least seen give theirs up)
QVR_CVAR(vr_wounds_debug, "0", CVAR_NONE)             // 1: print each wound painted
QVR_CVAR(vr_body_anchors, "1", CVAR_ARCHIVE)          // holsters, virtual stock and hand collisions follow the body's lean and crouch
QVR_CVAR(vr_body_debug, "0", CVAR_NONE)               // 1 draw the skeleton; 2, 3 also show the body in front of you, facing you or from its left
QVR_CVAR(vr_body_eye_forward, "0.08", CVAR_ARCHIVE)   // metres from the top of the neck to the eyes, forward and up
QVR_CVAR(vr_body_eye_up, "0.08", CVAR_ARCHIVE)
QVR_CVAR(vr_body_crouch_tilt, "25", CVAR_ARCHIVE)    // degrees the back tilts forward in a full crouch
QVR_CVAR(vr_body_torso_back, "0", CVAR_ARCHIVE)       // metres the torso sits further behind the neck (negative: in front)
QVR_CVAR(vr_body_legs_back, "0", CVAR_ARCHIVE)        // metres the feet stand behind the head (negative: in front)
QVR_CVAR(vr_body_pauldrons, "1", CVAR_ARCHIVE)        // pauldrons on the body's shoulders, after the Quake ranger's (make_pauldron.py)
QVR_CVAR(vr_body_pauldron_style, "0", CVAR_ARCHIVE)   // 0 the ranger's brown leather, 1 the colour of the armour worn (leather without), 2 steel
QVR_CVAR(vr_body_pauldron_size, "1", CVAR_ARCHIVE)    // their size (scaled with the build too)
QVR_CVAR(vr_body_pauldron_follow, "0.35", CVAR_ARCHIVE) // how much the cap turns with the upper arm (0: with the shoulder; the lames follow the arm)
QVR_CVAR(vr_body_pauldron_forward, "0", CVAR_ARCHIVE) // metres they sit further forward,
QVR_CVAR(vr_body_pauldron_up, "0", CVAR_ARCHIVE)      // up,
QVR_CVAR(vr_body_pauldron_out, "0", CVAR_ARCHIVE)     // and out
// The flashlight (vr_flashlight.cpp): stored hanging on the belt, off hand side (round 21; on the chest before); only deliberate presses (an open, still hand) take or switch it; trigger at it switches it, an empty hand's grip takes it; held near the other hand's gun, B/Y clips it on; held at the head, B/Y clips it on there.
QVR_CVAR(vr_flashlight, "1", CVAR_ARCHIVE)            // 0: no flashlight
QVR_CVAR(vr_flashlight_brightness, "1", CVAR_ARCHIVE)
QVR_CVAR(vr_flashlight_range, "1000", CVAR_ARCHIVE)   // units the beam reaches
QVR_CVAR(vr_flashlight_shadows, "1", CVAR_ARCHIVE)    // its spot light casts shadows (takes one of vr_shadow_dlights: one tile, not six faces)
QVR_CVAR(vr_flashlight_cord, "1", CVAR_ARCHIVE)      // the retracting cord from the belt clip to the torch while it is off the belt: 0 none, 1 coiled (vr_coil.cpp), 2 a plain cable
QVR_CVAR(vr_flashlight_beam, "0.35", CVAR_ARCHIVE)    // how strong its soft cone of light in the air is, 0..1 (0: none)
QVR_CVAR(vr_flashlight_beam_quality, "1", CVAR_ARCHIVE) // how closely the visible beam finds the walls that cut it: 0 low, 1 medium, 2 high (traces a frame, vr_flashlight.cpp)
QVR_CVAR(vr_flashlight_tilt, "8", CVAR_ARCHIVE)       // degrees the stored lamp (hanging on the chest, lens down) leans its lens out from the body
QVR_CVAR(vr_flashlight_forward, "0", CVAR_ARCHIVE)    // metres the belt clip sits further forward,
QVR_CVAR(vr_flashlight_up, "0", CVAR_ARCHIVE)         // up,
QVR_CVAR(vr_flashlight_out, "0", CVAR_ARCHIVE)        // and out (towards the off hand's side)
QVR_CVAR(vr_flashlight_hand_forward, "-0.05", CVAR_ARCHIVE) // metres the torch sits forward in the fist (negative: back),
QVR_CVAR(vr_flashlight_hand_up, "-0.04", CVAR_ARCHIVE)      // and up (negative: down; round 21: on top of the fitted grip)
QVR_CVAR(vr_flashlight_gun_forward, "0", CVAR_ARCHIVE) // metres the torch clipped on a gun (B/Y) sits further forward (its lens: 1 cm behind the muzzle),
QVR_CVAR(vr_flashlight_gun_up, "0", CVAR_ARCHIVE)      // up (from under the gun, or beside it: fitted per gun),
QVR_CVAR(vr_flashlight_gun_out, "0", CVAR_ARCHIVE)     // and out (away from the body)
QVR_CVAR(vr_flashlight_head_forward, "0", CVAR_ARCHIVE) // round 21: metres the torch clipped on the head (B/Y) sits further forward (its lens: level with the eyes),
QVR_CVAR(vr_flashlight_head_up, "0", CVAR_ARCHIVE)      // up (3.5 cm over the eyes),
QVR_CVAR(vr_flashlight_head_out, "0", CVAR_ARCHIVE)     // and out (8.5 cm to the side)
// Round 21: the torch's place in the hand, per grip (low: the beam out of the thumb's side; high: overhead, out of the
// little finger's), on top of vr_flashlight_hand_forward/_up: cm forward, towards the palm and up in the hand, degrees
// of pitch, yaw (towards the palm) and roll about the grip's middle; the off hand mirrored.
QVR_CVAR(vr_flashlight_low_x, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_flashlight_low_y, "-0.5", CVAR_ARCHIVE)
QVR_CVAR(vr_flashlight_low_z, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_flashlight_low_pitch, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_flashlight_low_yaw, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_flashlight_low_roll, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_flashlight_high_x, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_flashlight_high_y, "-0.5", CVAR_ARCHIVE)
QVR_CVAR(vr_flashlight_high_z, "2", CVAR_ARCHIVE)
QVR_CVAR(vr_flashlight_high_pitch, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_flashlight_high_yaw, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_flashlight_high_roll, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_flashlight_hue, "40", CVAR_ARCHIVE)        // round 21: the beam's hue (degrees; -1: the player's), with
QVR_CVAR(vr_flashlight_saturation, "0", CVAR_ARCHIVE)  // its saturation (0: white, 1: full colour): the light, the visible beam and the lens
// Round 21, the author's tuning notes: each grip's fingers on the torch, as a weapon's (Weapon Offsets > Fingers on the
// Weapon). _fingers 0 Automatic (they wrap the torch, sinking _overlap into it: 0..1 of a centimetre), 1 Manual (each
// finger at its _curl_*, 0 open to 1 a fist, and the thumb across the palm by _thumb_across, 0..1; no fitting); either
// way the finger tweaks _bias_* (a share of a full curl, negative more open) and the thumb's place _thumb_x/y/z (the
// hand's units, as a weapon's Thumb X/Y/Z). The manual curls default to the automatic grasp's at a full grip (its
// joints' mean stop, the thumb's turn; vr_debug_grasp 2 in the mock, the palm unfitted).
QVR_CVAR(vr_flashlight_low_fingers, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_flashlight_low_overlap, "0.3", CVAR_ARCHIVE)
QVR_CVAR(vr_flashlight_low_curl_thumb, "0.97", CVAR_ARCHIVE)
QVR_CVAR(vr_flashlight_low_curl_index, "0.52", CVAR_ARCHIVE)
QVR_CVAR(vr_flashlight_low_curl_middle, "0.78", CVAR_ARCHIVE)
QVR_CVAR(vr_flashlight_low_curl_ring, "0.8", CVAR_ARCHIVE)
QVR_CVAR(vr_flashlight_low_curl_pinky, "0.93", CVAR_ARCHIVE)
QVR_CVAR(vr_flashlight_low_thumb_across, "1", CVAR_ARCHIVE)
QVR_CVAR(vr_flashlight_low_bias_thumb, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_flashlight_low_bias_index, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_flashlight_low_bias_middle, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_flashlight_low_bias_ring, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_flashlight_low_bias_pinky, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_flashlight_low_thumb_x, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_flashlight_low_thumb_y, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_flashlight_low_thumb_z, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_flashlight_high_fingers, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_flashlight_high_overlap, "0.3", CVAR_ARCHIVE)
QVR_CVAR(vr_flashlight_high_curl_thumb, "0.97", CVAR_ARCHIVE)
QVR_CVAR(vr_flashlight_high_curl_index, "0.72", CVAR_ARCHIVE)
QVR_CVAR(vr_flashlight_high_curl_middle, "0.6", CVAR_ARCHIVE)
QVR_CVAR(vr_flashlight_high_curl_ring, "0.49", CVAR_ARCHIVE)
QVR_CVAR(vr_flashlight_high_curl_pinky, "1", CVAR_ARCHIVE)
QVR_CVAR(vr_flashlight_high_thumb_across, "0.73", CVAR_ARCHIVE)
QVR_CVAR(vr_flashlight_high_bias_thumb, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_flashlight_high_bias_index, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_flashlight_high_bias_middle, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_flashlight_high_bias_ring, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_flashlight_high_bias_pinky, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_flashlight_high_thumb_x, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_flashlight_high_thumb_y, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_flashlight_high_thumb_z, "0", CVAR_ARCHIVE)
// Round 21, the author's tuning notes: the reach zones. The head's: where a held torch's middle clips it on the head (B/Y)
// and a hand takes it off: balls at each temple (3.5 cm over the eyes, 8.5 out, 3 back) and the forehead (7 cm ahead, 6
// up), moved forward, up and out (away from the head: to the side at the temples, ahead at the forehead) by these metres,
// _radius across. Up defaults to 4 cm higher than the first zone (the author: it felt too low). Apart from the
// torch's own place on the head (vr_flashlight_head_forward/_up/_out). The gun's: where a held torch's middle clips it on
// the other hand's gun (B/Y): a capsule round the gun's line from the hand to 3 cm past the muzzle, moved along the
// gun, up and out (away from the body) by these metres, _radius across.
QVR_CVAR(vr_flashlight_head_zone_forward, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_flashlight_head_zone_up, "0.04", CVAR_ARCHIVE)
QVR_CVAR(vr_flashlight_head_zone_out, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_flashlight_head_zone_radius, "0.1", CVAR_ARCHIVE)
QVR_CVAR(vr_flashlight_gun_zone_forward, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_flashlight_gun_zone_up, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_flashlight_gun_zone_out, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_flashlight_gun_zone_radius, "0.12", CVAR_ARCHIVE)
QVR_CVAR(vr_show_flashlight_zones, "0", CVAR_NONE)     // draws those zones (green where the torch or a hand is in one) and the held torch's middle
// Spent shell casings (vr_shells.cpp): the shotgun throws one out as it cycles, the super shotgun two as it is reloaded.
QVR_CVAR(vr_shells, "1", CVAR_ARCHIVE)                // 0: none
QVR_CVAR(vr_shells_life, "20", CVAR_ARCHIVE)          // seconds they lie on the floor before fading out
QVR_CVAR(vr_shells_sound, "1", CVAR_ARCHIVE)          // volume of their tink as they land (0 off)
QVR_CVAR(vr_body_arm_length, "1", CVAR_ARCHIVE)      // arm length, relative to the proportions of vr_height_calibration
QVR_CVAR(vr_body_arm_stretch, "1.2", CVAR_ARCHIVE)    // how far arms may stretch to reach the hands (1: not at all)
// The arms and shoulders (vr_bodycal.hpp, "The arms' settings"): what Body Calibration measured (vr_bodycal_*), and
// the player's tweaks on top (vr_body_tweak_*, the same units, 0: as measured). Uncalibrated, the tweaks go on the
// default body: the model's arms times vr_body_arm_length, its shoulders 0.04 m in front of the chest, rising 25 and
// swinging 20 degrees. Measured: real cm from the shoulder joint to the elbow and from the elbow to the drawn hand's
// wrist (0: not measured). Both set, the arms are calibrated: the shoulders rise from lower down (continuously, as a
// real one does), they reach towards a hand before the arm stretches (vr_body_arm_stretch only makes up what their
// reach can't), and their place (metres from the model's: back, up, out), rise and swing (degrees) are the measured.
QVR_CVAR(vr_bodycal_upper_arm, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_bodycal_forearm, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_bodycal_shoulders_back, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_bodycal_shoulders_up, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_bodycal_shoulders_out, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_bodycal_shoulder_rise, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_bodycal_shoulder_swing, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_body_tweak_upper_arm, "0", CVAR_ARCHIVE)      // cm longer (negative: shorter)
QVR_CVAR(vr_body_tweak_forearm, "0", CVAR_ARCHIVE)        // cm longer
QVR_CVAR(vr_body_tweak_shoulders_back, "0", CVAR_ARCHIVE) // metres further back (negative: forward),
QVR_CVAR(vr_body_tweak_shoulders_up, "0", CVAR_ARCHIVE)   // higher,
QVR_CVAR(vr_body_tweak_shoulders_out, "0", CVAR_ARCHIVE)  // further out (negative: in)
QVR_CVAR(vr_body_tweak_shoulder_rise, "0", CVAR_ARCHIVE)  // degrees more the shoulders rise reaching up,
QVR_CVAR(vr_body_tweak_shoulder_swing, "0", CVAR_ARCHIVE) // and swing forward reaching far forward
// Round 21's settings before the split (absolute: Apply wrote the measurements over them). Not saved: a config's are
// moved to the ones above (bodycal::migrate), and so is one set in the console.
QVR_CVAR(vr_body_upper_arm, "", CVAR_NONE)
QVR_CVAR(vr_body_forearm, "", CVAR_NONE)
QVR_CVAR(vr_body_shoulders_back, "", CVAR_NONE)
QVR_CVAR(vr_body_shoulders_up, "", CVAR_NONE)
QVR_CVAR(vr_body_shoulders_out, "", CVAR_NONE)
QVR_CVAR(vr_body_shoulder_up, "", CVAR_NONE)
QVR_CVAR(vr_body_shoulder_forward, "", CVAR_NONE)
// Body Calibration's page: taken seated (the height left as it is); the body shown in front with the result (0 not, 1
// facing you, 2 from its left); the settings from before the last Apply ("name value;...", for Undo).
QVR_CVAR(vr_bodycal_seated, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_bodycal_preview, "1", CVAR_ARCHIVE)
QVR_CVAR(vr_bodycal_undo, "", CVAR_ARCHIVE)
QVR_CVAR(vr_body_shoulder_reach, "0.08", CVAR_ARCHIVE) // metres the shoulders may reach out towards a hand past the arm
QVR_CVAR(vr_body_forearm_twist, "0.5", CVAR_ARCHIVE)  // share of the wrist's roll the middle of the forearm follows (none at the elbow, all at the wrist)
QVR_CVAR(vr_body_wrist_limits, "1", CVAR_ARCHIVE)   // the elbow swings round to keep the wrist within a real one's bend and roll, times this (0: never)
QVR_CVAR(vr_body_elbow_out, "0.35", CVAR_ARCHIVE)     // elbow direction: down, plus this much outwards,
QVR_CVAR(vr_body_elbow_back, "0.25", CVAR_ARCHIVE)    // this much backwards,
QVR_CVAR(vr_body_elbow_hand, "0.4", CVAR_ARCHIVE)     // and this much away from the back of the hand
QVR_CVAR(vr_holster_haptics, "2", CVAR_ARCHIVE)
QVR_CVAR(vr_holster_haptic_time, "0.25", CVAR_ARCHIVE) // seconds of the buzz as a hand reaches a holster (vr_holster_haptics 2: once)
QVR_CVAR(vr_model_lighting, "1", CVAR_ARCHIVE)        // models shaded from the map's lights (0: Quake's fixed direction; 0..1 blends)
QVR_CVAR(vr_model_ambient_dir, "1", CVAR_ARCHIVE)     // models lit by the baked light around them, brighter on the side facing lit walls (vr_ambient.cpp; 0: one brightness all round)
QVR_CVAR(vr_model_ambient_contrast, "0.75", CVAR_ARCHIVE) // how much the sides differ (0: not at all, 1: as the surroundings do)
QVR_CVAR(vr_rim_light, "0.5", CVAR_ARCHIVE)           // a faint light round models' edges, in the light around them, so they stand out from dark backgrounds (vr_envmap.cpp; 0 off .. 1)
QVR_CVAR(vr_weapon_reflections, "1", CVAR_ARCHIVE)    // your weapons' metal reflects the room round you (a small cube map of the world, vr_envmap.cpp; 0 off)
QVR_CVAR(vr_weapon_reflections_strength, "1", CVAR_ARCHIVE) // how strongly (0..2)
QVR_CVAR(vr_relit_maps, "1", CVAR_ARCHIVE)           // maps re-lit by Misc/quakevr/relight_maps.py, when present (next map)
QVR_CVAR(vr_particles, "1", CVAR_ARCHIVE)             // Quake VR's textured particles for the QC's effects (0: Quake's own)
QVR_CVAR(vr_particle_mult, "1", CVAR_ARCHIVE)         // how many of them
QVR_CVAR(vr_particle_seed, "0", CVAR_NONE)            // tests: the particles' random numbers restarted from this seed at each map (0: random)
QVR_CVAR(vr_soft_particles, "1", CVAR_ARCHIVE)        // soft particles: smoke, fire, blood and explosion sprites fade out where they meet walls and floors instead of cutting hard lines (0 off)
QVR_CVAR(vr_soft_particles_scale, "1", CVAR_ARCHIVE)  // how close to the scene they fade, times each kind's own distance (sparks 1.5 units, smoke and fire half their size, at most 32)
QVR_CVAR(vr_blob_shadows, "2", CVAR_ARCHIVE)          // blob shadows (dark discs under things): 0 off, 1 always, 2 only without real shadows
QVR_CVAR(vr_player_shadows, "2", CVAR_ARCHIVE)
QVR_CVAR(vr_entity_shadows, "1", CVAR_ARCHIVE)        // blob shadows under monsters and items
// Dynamic ambient occlusion (vr_ao.cpp): what moves darkens the baked light round it.
QVR_CVAR(vr_ao_dynamic, "1", CVAR_ARCHIVE)            // ellipsoids round monsters, items, gibs and your body: strength (0 off .. 2)
QVR_CVAR(vr_ao_dynamic_range, "2.5", CVAR_ARCHIVE)    // how far their darkening reaches, in their radii (1.25 .. 5)
QVR_CVAR(vr_ao_brush, "1", CVAR_ARCHIVE)              // boxes round doors, lifts, platforms and the item boxes: strength (0 off .. 2)
QVR_CVAR(vr_ao_models, "1", CVAR_ARCHIVE)             // models' own occlusion, baked per vertex as they load (armpits, under guns): strength (0 off .. 2)
QVR_CVAR(vr_water_splash_speed, "150", CVAR_ARCHIVE)  // units/s a thing (not a player) must move at to splash entering or leaving water
QVR_CVAR(vr_water_splash, "1", CVAR_ARCHIVE)          // splashes (drops, foam, rings) where shots, things, you and your hands hit water, slime and lava: 0 off .. 2 more
QVR_CVAR(vr_water_splash_size, "1", CVAR_ARCHIVE)     // ... how big their drops, spray, foam and rings are (times)
QVR_CVAR(vr_water_splash_ring_speed, "1", CVAR_ARCHIVE) // ... how fast their rings spread, times the ripples' speed (1: riding the ripples' crest)
QVR_CVAR(vr_water_splash_ring_size, "1", CVAR_ARCHIVE) // ... how big the rings start and how long they last (0: none)
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
QVR_CVAR(vr_shadow_self, "2", CVAR_ARCHIVE)            // your shadow from map lights and dynamic lights (the flashlight): 0 none, 1 your body, 2 body, hands, guns
QVR_CVAR(vr_shadow_filter, "1", CVAR_ARCHIVE)          // shadow edge softening: 0 hard (1 tap), 1 (4 taps), 2 (9), 3 (16)
QVR_CVAR(vr_shadow_bias, "1", CVAR_ARCHIVE)            // shadow acne vs. light leaking under objects
QVR_CVAR(vr_shadow_distance, "1536", CVAR_ARCHIVE)     // lights farther than this from you cast no shadows
QVR_CVAR(vr_shadow_atlas, "4096", CVAR_ARCHIVE)        // shadow map atlas size (2048, 4096, 8192)
QVR_CVAR(vr_shadow_stats, "0", CVAR_NONE)              // print what is shadowed and the GPU time, once a second
QVR_CVAR(vr_profile, "0", CVAR_NONE)                   // profiler (vr_profile.cpp): 1 time the frame's scopes, CPU and GPU, into <gamedir>/profile/; 2 also show the panel over the wrist
QVR_CVAR(vr_profile_interval, "5", CVAR_NONE)          // seconds between its reports (0: only on vr_profile_dump)
QVR_CVAR(vr_profile_overlay, "0", CVAR_NONE)           // the profiler's panel (vr_profile_systems.cpp): each system's ms a frame with a bar; 1 over the wrist gadget's hand, 2 floating in front
QVR_CVAR(vr_profile_gpu, "4", CVAR_NONE)               // the profiler's GPU times: its GPU scopes' timer queries on one frame in this many (each stalls the GPU a little: 1 every frame, 4 a quarter of the cost; 0 none, the CPU's times alone)
QVR_CVAR(vr_profile_detail, "1", CVAR_NONE)            // 2: the profiler also times every trace (SV_Move) and every QuakeC builtin call apart (dearer)
QVR_CVAR(vr_profile_csv, "0", CVAR_NONE)               // 1: the profiler writes a row a second, a column per system, to <gamedir>/profile/systems_<date>_<time>.csv
QVR_CVAR(vr_profile_hitch, "1.5", CVAR_NONE)           // while the profiler collects, frames over this many budgets (refresh periods) are logged with their breakdown (console, profile/hitches_*.csv); 0 off
QVR_CVAR(vr_dlight_models, "1", CVAR_ARCHIVE)          // dynamic lights light models per pixel, by angle and shadowed (0: Quake's flat)
QVR_CVAR(vr_dlight_angle, "1", CVAR_ARCHIVE)           // dynamic lights fall off with the angle they reach surfaces at (0: Quake's)
QVR_CVAR(vr_dlight_uncapped, "1", CVAR_ARCHIVE)        // dynamic lights add fully to bright walls (Quake caps them)
QVR_CVAR(vr_light_contrast, "2", CVAR_ARCHIVE)         // lightmap contrast about Quake's full light: shade darker, lamps as bright (1: Quake's)
QVR_CVAR(vr_decals, "1", CVAR_ARCHIVE)                 // blood and scorch marks on the world
QVR_CVAR(vr_decal_max, "1024", CVAR_ARCHIVE)           // how many are kept (the oldest go first)
QVR_CVAR(vr_decal_life, "120", CVAR_ARCHIVE)           // seconds a mark stays (fading over the last five)
QVR_CVAR(vr_gib_blood, "1", CVAR_ARCHIVE)              // gibs and heads bleed: Quake VR's blood trail, drops on the floor, splats where they hit (0: Quake's trail)
QVR_CVAR(vr_gib_blood_trail, "1", CVAR_ARCHIVE)        // how dense their trail of blood and drops is (0 none)
QVR_CVAR(vr_gore, "2", CVAR_ARCHIVE)                   // gore (vr_gore.cpp): 0 Quake VR's blood only, 1 more, 2 over the top (sprays on walls, pools under corpses, gibs stuck to ceilings dripping)
QVR_CVAR(vr_gore_spray, "1", CVAR_ARCHIVE)             // how many splats a hit or a gibbing sprays onto the walls, floor and ceiling round it (0 none)
QVR_CVAR(vr_gore_size, "1", CVAR_ARCHIVE)              // how big the gore's splats, pools and runs are
QVR_CVAR(vr_gore_pools, "1", CVAR_ARCHIVE)             // pools of blood spreading under corpses and gibs: their size (0 none)
QVR_CVAR(vr_gore_drips, "1", CVAR_ARCHIVE)             // blood dripping from splats on the ceiling and from hanging gibs: how long and how much (0 none)
QVR_CVAR(vr_gore_stick, "8", CVAR_ARCHIVE)             // gibs flung into a ceiling or a wall may stick there about this many seconds, then fall (0 never)
QVR_CVAR(vr_body_blood_amount, "1", CVAR_ARCHIVE)      // how big the drops from your wounds are, and how much they splash
QVR_CVAR(vr_body_blood_marks, "1", CVAR_ARCHIVE)       // the chance a drop from your wounds marks the floor (0..1)
QVR_CVAR(vr_body_blood_mark_size, "1", CVAR_ARCHIVE)   // how big those marks are
QVR_CVAR(vr_body_blood_floor, "1", CVAR_ARCHIVE)       // while wounded, blood drips round your feet (from the body), marking the floor: how often (0 none)
QVR_CVAR(vr_bloom, "0.3", CVAR_ARCHIVE)                // glow around bright things (lamps, glowing panels, flashes): its strength (0 off)
QVR_CVAR(vr_bloom_threshold, "0.7", CVAR_ARCHIVE)      // how bright a thing must be to glow (0..1)
QVR_CVAR(vr_bloom_radius, "1", CVAR_ARCHIVE)           // how wide the glow spreads
QVR_CVAR(vr_flash_scale, "1", CVAR_ARCHIVE)            // muzzle flashes' light: size over the falloff's own (Quake's or DarkPlaces')
QVR_CVAR(vr_explosion_light_scale, "1", CVAR_ARCHIVE)  // explosions' and rockets' light: size over the falloff's own
QVR_CVAR(vr_colored_lights, "1", CVAR_ARCHIVE)         // muzzle flashes, rockets and explosions coloured (warm; the lightning gun blue)
QVR_CVAR(vr_projectile_lights, "1", CVAR_ARCHIVE)      // glowing projectiles light up the room (hell knight flames, scrag spit, vore balls, lasers; and where they hit): size (0 off)
QVR_CVAR(vr_lavanail_lights, "8", CVAR_ARCHIVE)       // lava nails light up the room (sized by vr_projectile_lights): how many at once, the nearest (0 none)
QVR_CVAR(vr_lavagun_light, "1", CVAR_ARCHIVE)          // the lava nailguns (their lava ammo) glow from their barrels: a small flickering lava light, its brightness (0 off)
QVR_CVAR(vr_lavagun_light_radius, "72", CVAR_ARCHIVE)  // how far that light reaches
QVR_CVAR(vr_lavagun_light_flicker, "0.35", CVAR_ARCHIVE) // how much it flickers (0 steady, 1 a lot)
QVR_CVAR(vr_lavagun_light_idle, "0.4", CVAR_ARCHIVE)   // the same guns in holsters or lying on the floor, dimmer: their brightness over a held one's (0 none)
QVR_CVAR(vr_beam_lights, "6", CVAR_ARCHIVE)          // the lightning (gun, shambler, Chthon) lights the room along its beam: how many lights per beam, unshadowed (0 off)
QVR_CVAR(vr_torch_lights, "8", CVAR_ARCHIVE)         // torches and flames flicker a small warm light onto the room: how many at once, the nearest in view (0 off)
QVR_CVAR(vr_torch_light_scale, "1", CVAR_ARCHIVE)    // their brightness (on top of the baked light)
QVR_CVAR(vr_torch_light_shadows, "0", CVAR_ARCHIVE)  // how many of them (the nearest) may cast shadows, taking vr_shadow_dlights' slots (0 none)
QVR_CVAR(vr_weapon_screen_light, "1", CVAR_ARCHIVE)    // the weapons' ammo screens cast a small light in their colour the way they face: strength (0 off)
QVR_CVAR(vr_screen_glow, "1", CVAR_ARCHIVE)            // the wrist gadget's and the weapons' screens glow softly round their edges: strength (0 off)
QVR_CVAR(vr_screen_text_glow, "1", CVAR_ARCHIVE)       // the text, numbers and icons on those screens glow: whitish cores, a soft halo in the screen's colour (strength, 0 off, up to 3)
QVR_CVAR(vr_weapon_glow, "1", CVAR_ARCHIVE)            // your weapons' glowing parts (the shotgun's sights) brighter, so that they glow: strength (0: as Quake)
// The DarkPlaces look (round 10; docs/vr-port/LIGHTING.md).
QVR_CVAR(vr_gamma, "1", CVAR_ARCHIVE)                  // the headset's gamma (the desktop's "gamma" is not used in the eyes; 1 neutral)
QVR_CVAR(vr_contrast, "1", CVAR_ARCHIVE)               // the headset's contrast (the desktop's "contrast" is not used in the eyes; 1 neutral)
QVR_CVAR(vr_dlight_falloff, "1", CVAR_ARCHIVE)         // dynamic lights: 1 DarkPlaces' (smooth falloff, strong coloured lights), 0 Quake's (linear)
QVR_CVAR(vr_specular, "0.125", CVAR_ARCHIVE)           // sheen of dynamic lights on the world and models (Blinn, exponent 32; 0 off)
QVR_CVAR(vr_specular_aa, "1", CVAR_ARCHIVE)            // specular anti-aliasing: the sheen (and the water's glints) widened where bumps are smaller than a pixel, so it doesn't sparkle as the head moves (0 off; 2 stronger)
QVR_CVAR(vr_normalmap_cache, "1", CVAR_NONE)       // the normal maps made from skins kept on disk (cache/normalmaps: the same texels, 0.2 s less in the first load); 0 made every time; 2 made and compared with the file (vr_texcache.cpp)
QVR_CVAR(vr_normalmaps, "1", CVAR_ARCHIVE)             // bumps from normal maps made from the textures, lit by the map's light and dynamic lights (next map)
QVR_CVAR(vr_normalmap_strength, "1", CVAR_ARCHIVE)     // how deep those bumps are
QVR_CVAR(vr_normalmap_baked, "1", CVAR_ARCHIVE)        // how much the bumps shade the map's (baked) light, lit from where the light seems to come from (0: only dynamic lights)
QVR_CVAR(vr_deluxemap, "1", CVAR_ARCHIVE)             // the map light's real directions (the relit map's .lux, deluxemaps) for its bumps and sheen (0: guessed from where the lightmap brightens)
QVR_CVAR(vr_normalmap_models, "1", CVAR_ARCHIVE)       // how much the bumps shade models' own light, from its direction (times vr_normalmap_baked; held weapons less; 0: only dynamic lights)
QVR_CVAR(vr_normalmap_authored, "1", CVAR_ARCHIVE)     // round 21: how strongly authored normal maps (a model skin's *_norm or *_bump file) bend models' normals: 1 as authored; held weapons and hands too (not halved)
QVR_CVAR(vr_parallax, "1", CVAR_ARCHIVE)               // parallax occlusion mapping on the world: its dark parts sunk in, seen round from the side (needs vr_normalmaps)
QVR_CVAR(vr_parallax_depth, "3", CVAR_ARCHIVE)       // how deep, in units, the darkest parts of a texture sink below its brightest
QVR_CVAR(vr_parallax_distance, "512", CVAR_ARCHIVE)    // how far it reaches, in units (it fades out over the last quarter)
QVR_CVAR(vr_parallax_steps, "16", CVAR_ARCHIVE)        // the most steps it takes along a ray (at grazing angles; half as many looking straight on)
QVR_CVAR(vr_parallax_items, "1.5", CVAR_ARCHIVE)      // parallax depth of the ammo and health boxes, in their own units (scaled with the box; 0 off)
QVR_CVAR(vr_detail, "1", CVAR_ARCHIVE)                 // detail textures: a fine grain (stone, metal, wood, grime...) over the world's textures close by (vr_detail.cpp)
QVR_CVAR(vr_detail_strength, "1", CVAR_ARCHIVE)        // ... how strongly it shows (times each kind's own, textures/vr/detail.cfg)
QVR_CVAR(vr_detail_distance, "144", CVAR_ARCHIVE)      // ... how far it reaches, in units (full within a third of it; nothing past it, no cost)
QVR_CVAR(vr_detail_fine, "1", CVAR_ARCHIVE)            // ... a second, finer grain within half that distance (one more texture read there)
QVR_CVAR(vr_parallax_models, "0", CVAR_ARCHIVE)       // parallax depth of models whose heights are made from their skins (monsters, pickups, id's weapons without a baked map), in their own units (0 off: on Quake's blocky skins it bends the texels; 0.5 to 0.75 for smooth skins)
QVR_CVAR(vr_parallax_authored, "1", CVAR_ARCHIVE)     // round 21: parallax depth of models with authored normal maps carrying heights (our hands, body, gadget, flashlight, weapons: bake_normals.py), times their baked depth (1 as baked, 0 off)
QVR_CVAR(vr_model_light_parity, "1", CVAR_ARCHIVE)     // models as bright as the floor under them, on average (0: Quake's brighter models)
QVR_CVAR(vr_viewmodel_minlight, "8", CVAR_ARCHIVE)     // least light on your hands and weapons (Quake's: 24)
QVR_CVAR(vr_texture_smooth, "1", CVAR_ARCHIVE)         // smooth filtering: 1 replacement textures (textures/*.tga), 2 every texture, 0 as gl_texturemode
QVR_CVAR(vr_alpha_coverage, "1", CVAR_ARCHIVE)         // fences and grates (alpha-tested textures) keep their coverage in their mips (no thinning out with distance); with MSAA (vid_fsaa) their edges smoothed by alpha to coverage
QVR_CVAR(vr_bloom_white, "0.5", CVAR_ARCHIVE)          // bloom of white and pale lights, times vr_bloom
QVR_CVAR(vr_bloom_color, "1.5", CVAR_ARCHIVE)          // bloom of coloured lights (red buttons, blue panels), times vr_bloom
QVR_CVAR(vr_bloom_adapt, "4", CVAR_ARCHIVE)           // bloom weakens the more of the view glows (0: never)
// Tone mapping, dither, grading (round 17; vr_tonemap.cpp, docs/vr-port/ROUND17.md).
QVR_CVAR(vr_tonemap, "1", CVAR_ARCHIVE)               // the eyes' scene in floats, its brightest rolled off by a filmic shoulder instead of clipped (lights keep their colour); 0: Quake's clamp
QVR_CVAR(vr_exposure, "1", CVAR_ARCHIVE)              // brightness before the tone curve (with vr_tonemap; 1: as Quake)
QVR_CVAR(vr_dither, "1", CVAR_ARCHIVE)                // the eyes' last dither into 8 bits, against banding: 0 off, 1 fixed on the screen (each eye its own), 2 changing each frame, 3 the same in both eyes
QVR_CVAR(vr_grade, "1", CVAR_ARCHIVE)                 // colour grade (quakevr/gfx/vr/grade_*.png): 0 off, 1 Quake VR (film), 2 cold, 3 warm, 4 by episode
QVR_CVAR(vr_grade_strength, "1", CVAR_ARCHIVE)        // how much of the grade (0..1)
QVR_CVAR(vr_eyeshot, "0", CVAR_NONE)                  // 1: save the next frame's eye images to <gamedir>/eyeshots (2: also the float scene, .pfm)
// Liquids (vr_water.cpp): 0 everywhere is Quake's look.
QVR_CVAR(vr_map_liquid_alpha, "0", CVAR_ARCHIVE)      // a map's own wateralpha, lavaalpha, slimealpha, telealpha (its worldspawn keys) win over r_wateralpha and the others (Ironwail's rule); 0: your settings always
QVR_CVAR(vr_water_waves, "1", CVAR_ARCHIVE)           // waves on water, slime and lava (normal-mapped, in the shader: the faces stay flat): strength
QVR_CVAR(vr_water_fresnel, "1", CVAR_ARCHIVE)         // water more see-through looking down, reflecting the room at grazing angles
QVR_CVAR(vr_water_refraction, "1", CVAR_ARCHIVE)      // what is under translucent water bent by the waves (needs r_oit 1; with anti-aliasing the scene is resolved for it first): strength
QVR_CVAR(vr_water_glints, "1", CVAR_ARCHIVE)          // light glinting off the waves: strength
QVR_CVAR(vr_water_lava_glow, "1", CVAR_ARCHIVE)       // lava glows (brighter where hot, a slow pulse; blooms): strength
QVR_CVAR(vr_water_caustics, "0.6", CVAR_ARCHIVE)      // dappled light on what is under water: strength
QVR_CVAR(vr_water_underwater, "1", CVAR_ARCHIVE)      // with your head in a liquid: its fog and tint, a little blur (0: Quake's colour shift)
QVR_CVAR(vr_water_wobble, "1", CVAR_ARCHIVE)          // ... and a slow wobble of the view (0 off, for comfort)
QVR_CVAR(vr_water_geo_waves, "1", CVAR_ARCHIVE)       // real waves: the level liquid surfaces cut into a grid whose vertices rise and fall with long swells
QVR_CVAR(vr_water_geo_amplitude, "3", CVAR_ARCHIVE)   // ... their height in units (water's; lava's more, slime's less)
QVR_CVAR(vr_water_geo_cell, "16", CVAR_ARCHIVE)       // ... the grid's cell in units (smaller: smoother, more triangles; remade on change)
QVR_CVAR(vr_water_foam, "1", CVAR_ARCHIVE)            // shoreline foam where water meets walls, steps and things standing in it (slime: a scum, lava: a hot rim): 0 off .. 2
QVR_CVAR(vr_heat_haze, "1", CVAR_ARCHIVE)             // heat haze (vr_haze.cpp): the air shimmering over lava, round explosions and over flames: 0 off .. 1
QVR_CVAR(vr_water_ripples, "1", CVAR_ARCHIVE)         // ripples: rings of waves spreading from splashes, wading and strokes, in the lighting and (with the real waves' grid) the geometry
QVR_CVAR(vr_water_ripple_amplitude, "8", CVAR_ARCHIVE)   // ... how high they rise in the geometry (with the real waves' grid), units for a hand's slap (a shot's about half, a body's two to three times)
QVR_CVAR(vr_water_ripple_normal, "0.6", CVAR_ARCHIVE)    // ... how steep they look in the lighting, times the geometry's slope (1: as the shape; 0: the shape only)
QVR_CVAR(vr_water_ripple_speed, "32", CVAR_ARCHIVE)   // ... how fast the rings spread, units/s (the crests run through them twice as fast; lava's a third, slime's 0.7)
QVR_CVAR(vr_water_ripple_decay, "2", CVAR_ARCHIVE)    // ... seconds for them to die down to a third
QVR_CVAR(vr_water_ripple_wavelength, "64", CVAR_ARCHIVE) // ... units between crests (in the geometry from 2 grid cells up, fully from 3; the lighting also has ones a third as long)
QVR_CVAR(vr_water_ripple_max, "32", CVAR_ARCHIVE)       // ... how many at once, at most (1..32; a new one takes the weakest's place)
QVR_CVAR(vr_positional_damage, "1", CVAR_ARCHIVE)
QVR_CVAR(vr_headshot_sound, "0.5", CVAR_ARCHIVE)      // volume of the tick on a headshot (0 off)
QVR_CVAR(vr_climb, "0", CVAR_ARCHIVE)                 // on as shipped (vr_defaults.cfg): either empty hand gripping a ledge or rung hangs from it; hand over hand, shimmy, mantle (vr_climb.cpp)
QVR_CVAR(vr_climb_min_height, "30", CVAR_ARCHIVE)     // units above the feet a ledge must be to take hold of
QVR_CVAR(vr_climb_fling, "1", CVAR_ARCHIVE)           // how much letting go flings you with the hands' motion (capped; 0 none)
QVR_CVAR(vr_climb_leniency, "6", CVAR_ARCHIVE)       // cm: a grip that misses a hold takes the nearest one this close to where it would have (0: exact)
QVR_CVAR(vr_climb_slide, "1", CVAR_ARCHIVE)          // a holding hand the body can't follow along the ledge (past the arm's reach, a wall) slides along it, the drawn hand staying on the controller (0: it stays put, the two drift apart)
QVR_CVAR(vr_climb_overhang_stretch, "16", CVAR_NONE) // units a hanging body's arms may stretch past their reach under an overhang (the box under a floating platform: pushing away takes it out from under; 0: no stretch)
QVR_CVAR(vr_climb_hand_out, "10", CVAR_ARCHIVE)      // cm: the drawn hand on its hold moved towards you (off the lip; negative: deeper), display only
QVR_CVAR(vr_climb_hand_up, "1", CVAR_ARCHIVE)        // cm: and up (negative: down), display only
QVR_CVAR(vr_climb_hand_side, "7.5", CVAR_ARCHIVE)    // cm: and along the edge, outwards (the main hand right, the off hand left), display only
QVR_CVAR(vr_climb_hand_turn_blend, "0.15", CVAR_ARCHIVE) // a hand on a hold turned as the hold has it (0: facing it, vr_climb_hand_pitch/yaw/roll) or as its controller (1), display only
QVR_CVAR(vr_climb_hand_pitch, "60", CVAR_ARCHIVE)    // degrees: the hand on a hold (facing it: at 0 the palm down on the top, the knuckles towards the ledge; 90: the palm against the face under the lip, the knuckles up) tilted knuckles up, display only
QVR_CVAR(vr_climb_hand_yaw, "10", CVAR_ARCHIVE)      // degrees: and turned, the knuckles towards the other hand (mirrored), display only
QVR_CVAR(vr_climb_hand_roll, "0", CVAR_ARCHIVE)      // degrees: and rolled about the knuckles' way, the thumb's side up (mirrored), display only
QVR_CVAR(vr_climb_mover_crush, "0", CVAR_ARCHIVE)     // a hold on a moving brush (a plat, a train, a door) whose ride carries you into something: 0 the hand lets go (you fall); 1 you hold on and block it, as anything in its way (Quake's crush rules: it hurts you, a door or plat goes back)
QVR_CVAR(vr_debug_ledges, "0", CVAR_NONE)             // the ledge map (vr_ledges.cpp) drawn round you: lips green (on brush models pink), a tick out every 8 units (yellow: the drop starts further out, past a trim or a rung below), the top's depth in (grey); 1: within 512 units, more: that many
QVR_CVAR(vr_climb_stamina, "1", CVAR_ARCHIVE)         // hanging from a hold tires you: stamina from the parry/shove/strike pool (vr_parry_stamina_max) drains while a hand holds and your feet are on nothing, none comes back meanwhile, and at none the hands let go (vr_climb.cpp "Climbing stamina")
QVR_CVAR(vr_climb_stamina_rate, "5", CVAR_ARCHIVE)    // with it: stamina a second hanging from one hand
QVR_CVAR(vr_climb_stamina_rate_2h, "2", CVAR_ARCHIVE) // and from both hands (the two together: less than one hand, the arms share the weight)
QVR_CVAR(vr_climb_stamina_slip, "0", CVAR_ARCHIVE)    // s: with none left, 0 the hands let go at once; more: they can't pull you up, you sink to straight arms and they slip off after this long
// Tired arms shake (vr_fatigue.cpp; ROUND21.md, "Tired arms: shaking and heavy hands"): looks only, the drawn hands, arms and
// what they hold; the game's hands, the aim and the shots stay steady.
QVR_CVAR(vr_fatigue_shake, "0.6", CVAR_ARCHIVE)       // cm the drawn hands shake by at most with no stamina left (0: no moving shake)
QVR_CVAR(vr_fatigue_shake_angle, "1.5", CVAR_ARCHIVE) // degrees they turn by at most with none left (0: no turning shake)
QVR_CVAR(vr_fatigue_shake_from, "0.4", CVAR_ARCHIVE)  // the stamina left (0..1) below which they start to shake, more the less is left
QVR_CVAR(vr_fatigue_shake_speed, "1", CVAR_ARCHIVE)   // times the tremor's quickness (5 to 13 Hz)
QVR_CVAR(vr_fatigue_shake_always, "0", CVAR_ARCHIVE)  // 0: only while a hand holds a ledge or a rung; 1: whenever tired
QVR_CVAR(vr_climb_debug, "0", CVAR_NONE)              // print holds, releases and mantles (2: and "climbtrace" and "climbreach" lines every frame: the body, the hands, the pull, the holds' reach; 3: the shoulders, estimated and drawn)
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
QVR_CVAR(vr_swim_against_palm, "0.25", CVAR_ARCHIVE) // how much a stroke counts with the back of the hand leading (moving away from where the palm faces; 1: as a palm stroke)
QVR_CVAR(vr_air_supply, "1.5", CVAR_ARCHIVE)        // the breath under water before drowning, times Quake's 12 seconds (1 to 4; biosuit and wetsuit as ever)
QVR_CVAR(vr_swim_debug, "0", CVAR_NONE)               // print each stroke: peak speed, power gate, reverse damping and the push it gave
QVR_CVAR(vr_sword_drop, "1", CVAR_NONE)               // retired (knights and hell knights always drop their sword: ROUND21.md, "Simplification"): kept so saved configs load silently
QVR_CVAR(vr_sword_damage_mult, "1.5", CVAR_ARCHIVE)   // a knight's sword: its swing's damage over the axe's
QVR_CVAR(vr_wofs_version, "0", CVAR_ARCHIVE)          // the weapon settings changes the config has taken (vr_weapons.cpp)
QVR_CVAR(vr_carry_take, "0", CVAR_ARCHIVE)            // taking a carried box: 0 let go of it at a holster, 1 the trigger, 2 either
QVR_CVAR(vr_carry_local, "1", CVAR_ARCHIVE)           // what you carry is drawn in your hand as it moves this frame (0: where the server has it, a frame or a ping behind)
QVR_CVAR(vr_held_surface_fit, "1", CVAR_ARCHIVE)      // a gripped box, gib or backpack sits against the palm, pushed out of the fist, instead of sunk into the hand (0: where it was gripped)
QVR_CVAR(vr_held_collide, "1", CVAR_ARCHIVE)          // props held one in each hand meet, and a prop and the weapon in the other hand: drawn kept apart, each hand moved back with what it holds (looks only; vr_held.cpp meetFrame)
QVR_CVAR(vr_held_collide_max, "5", CVAR_ARCHIVE)      // with it: cm, the most each hand is drawn moved back (pressed further, they overlap by the rest)
QVR_CVAR(vr_held_collide_walls, "1", CVAR_ARCHIVE)    // a prop held in one hand stops at the walls: drawn held out of them, the hand with it (looks only; vr_held.cpp wallFrame)
QVR_CVAR(vr_held_collide_wall_max, "40", CVAR_ARCHIVE) // with it: cm, the most it and the hand are drawn held back (pushed further, it goes in by the rest; the server drops it 32 units past)
QVR_CVAR(vr_held_collide_monsters, "1", CVAR_ARCHIVE) // a prop held in one hand stops at the monsters' drawn models, the hand with it, as weapons do (vr_model_collide, vr_model_collide_max; looks only: vr_modelcollide.cpp); never dropped for it
QVR_CVAR(vr_held_fit_gap, "0", CVAR_ARCHIVE)          // with it: cm between the curled fingers and what they hold (negative: sunk in)
QVR_CVAR(vr_carry_reach, "8", CVAR_NONE)              // retired (grab reach from the fist: vr_carry_grab_bias): kept so saved configs load silently
QVR_CVAR(vr_carry_grab_bias, "0", CVAR_ARCHIVE)       // cm: a hand takes a box, gib, backpack or armour if its fist (the palm and the curled fingers) is within this of the thing's drawn surface (negative: pressed that far into it; vr_held.cpp grabTouch)
QVR_CVAR(vr_debug_hands, "0", CVAR_NONE)              // prints each hand's state when it changes (2: every frame): the grip, what it holds (weapon, carried object, force grab, flashlight), its climbing hold and why a grip takes none, the hotspot (vr_climb.cpp debugHands)
QVR_CVAR(vr_debug_carry, "0", CVAR_NONE)              // draws, for each hand near a carriable thing, the grab test: the box the thing is drawn in, the fist tested (its spheres; the nearest bright) and the nearest point of its surface (green: it touches, in reach); 2: also carry_trace.txt, what is carried and both hands every frame, and each grab test printed; 3: and the fists not near
QVR_CVAR(vr_debug_arm, "0", CVAR_NONE)                // prints each drawn arm's IK once (the shoulder, elbow and wrist in the body's axes, cm; the elbow's swing; the wrist's bend and twist against the forearm), then turns itself off; 2: every frame
QVR_CVAR(vr_debug_lean, "0", CVAR_NONE)               // lean_trace.txt (the game directory): the head, the box, the lean, the pelvis and the feet every frame, and the lean's cues (vr_lean_detect)
QVR_CVAR(vr_grapple_test_stuck, "0", CVAR_NONE)        // Debug > Tests > Grappling Hook > Load Stuck: what hangs on the rope (a loose hook, a hooked prop) stays where it is, as if snagged (tests the rope holding you back)
QVR_CVAR(vr_debug_rope, "0", CVAR_NONE)                // Debug > Views > Show Grapple Rope: each grappling hook rope's drawn chain (points white, pieces green), the corners it wraps round (red) and its taut path (yellow) (vr_rope.cpp)
QVR_CVAR(vr_debug_physics_shapes, "0", CVAR_NONE)     // Debug > Views > Show Physics Shapes: Box3D's bodies drawn as wireframes (props awake green, asleep blue, held yellow, doors purple, monsters orange, players cyan, hanging pickups grey; contact points), and the hands' grab probes (as vr_debug_carry)
QVR_CVAR(vr_debug_hand_bones, "0", CVAR_NONE)         // Debug > Views > Show Hand Bones: the jointed hands' joints and bones, the grasp's contact spheres (green touching, yellow near, red sunk in) and the palm's fit (where it was, where the grasp put it)
QVR_CVAR(vr_carry_two_hands, "1", CVAR_ARCHIVE)       // a box, backpack, gib, head or armour carried in one hand can be gripped by the other too: held in both, it follows both hands (vr_carry2h.cpp)
QVR_CVAR(vr_carry_two_hands_drift, "8", CVAR_ARCHIVE) // with it: cm each drawn hand may be off its controller to stay on its grip (hands pulled apart or pushed together)
QVR_CVAR(vr_carry_two_hands_detach, "3", CVAR_ARCHIVE) // with it: cm past the drift a hand may be off its grip before it lets go (its fist off the prop: pulled away, not sliding on it); both off: it drops (vr_carry2h.cpp detached)
QVR_CVAR(vr_carry_two_hands_window, "0.1", CVAR_ARCHIVE) // seconds between the two hands letting go that still make a two-handed throw
QVR_CVAR(vr_held_fit_gaps, "", CVAR_ARCHIVE)          // ... and per model, added: "name=cm ...", a name matching the end of the model's (e.g. "backpack.mdl=0.5 b_bh25.bsp=-0.3")
QVR_CVAR(vr_forcegrab_outline, "1", CVAR_ARCHIVE)     // the glow round what a hand's force grab points at (strength, 0 off)
QVR_CVAR(vr_forcegrab_fx, "1", CVAR_ARCHIVE)          // force grab's beam and energy tendril from the hand, and the pulled object's trail
QVR_CVAR(vr_forcegrab_hue, "-1", CVAR_ARCHIVE)        // their colour, the glow's and the sparkles': hue (degrees; 215 blue; -1 vr_player_hue)
QVR_CVAR(vr_forcegrab_saturation, "0.7", CVAR_ARCHIVE) // and saturation (1 as made, 0 white, up to 2; times vr_player_saturation while the hue is the player's; 0.7: as the wrist gadget's screen)
QVR_CVAR(vr_grab_gibs, "1", CVAR_ARCHIVE)             // gibs and heads: 0 left alone, 1 picked up and thrown by hand, 2 also force-grabbed
QVR_CVAR(vr_carry, "1", CVAR_ARCHIVE)                 // ammo and health boxes: grip to carry, trigger to take, nudge with hands and guns (0: touch takes them)
QVR_CVAR(vr_armor_wear, "1", CVAR_ARCHIVE)            // armour: 1 a physics object you carry and put on by letting go of it over your torso, 0 touching takes it (the kind: next map)
QVR_CVAR(vr_armor_scale, "0.5", CVAR_ARCHIVE)         // with it: the armour's size (1: Quake's, a metre tall; next map)
QVR_CVAR(vr_item_objects, "1", CVAR_ARCHIVE)          // the map's weapons, keys, runes, the biosuit, the wetsuit, the horn: 1 physics objects once a hand or a force grab takes them (a weapon gripped is your weapon; the rest taken at a holster), 0 touching takes them (the kind: next map)
QVR_CVAR(vr_carry_nudge, "1", CVAR_ARCHIVE)           // how hard hands and guns push them
QVR_CVAR(vr_carry_throw_mult, "1", CVAR_ARCHIVE)      // how fast they leave the hand
QVR_CVAR(vr_carry_melee_mult, "1.5", CVAR_ARCHIVE)    // punches with a box in hand hurt this much more
QVR_CVAR(vr_carry_throw_damage, "8", CVAR_ARCHIVE)    // damage of a box thrown hard (at about 6 m/s; more the faster)
QVR_CVAR(vr_gib_throw_damage, "4", CVAR_ARCHIVE)      // and of a gib or head
// Flung props (vr_carry.qc VR_Prop_Flung): a prop never thrown (flung on the grapple, batted, knocked flying) that meets a
// monster (or a player) fast enough hurts it: its throw's base damage (vr_carry_throw_damage, vr_gib_throw_damage, a
// weapon's) times its Throw Damage x, vr_prop_impact_mult, its speed into it over vr_prop_impact_min_speed (at most
// vr_prop_impact_speed_max) and (mass / vr_prop_impact_weight_ref)^vr_prop_impact_weight_curve (at most _weight_max).
QVR_CVAR(vr_prop_impact_damage, "1", CVAR_ARCHIVE)        // on
QVR_CVAR(vr_prop_impact_players, "1", CVAR_ARCHIVE)       // 1: players too (never the one who just let go of it), 0: monsters only
QVR_CVAR(vr_prop_impact_min_mass, "1", CVAR_ARCHIVE)      // kg: lighter props never hurt so
QVR_CVAR(vr_prop_impact_min_speed, "8", CVAR_ARCHIVE)     // m/s towards what it meets (less for things over 10 kg: heavy leniency)
QVR_CVAR(vr_prop_impact_mult, "1", CVAR_ARCHIVE)          // the damage times this
QVR_CVAR(vr_prop_impact_speed_max, "3", CVAR_ARCHIVE)     // the most its speed multiplies the damage (speed / least speed)
QVR_CVAR(vr_prop_impact_weight_curve, "0.5", CVAR_ARCHIVE) // exponent of mass / reference (0: mass changes nothing; 1: in proportion)
QVR_CVAR(vr_prop_impact_weight_ref, "3", CVAR_ARCHIVE)    // kg: the mass that deals the base damage (lighter less, heavier more)
QVR_CVAR(vr_prop_impact_weight_max, "2.5", CVAR_ARCHIVE)  // the most mass multiplies the damage
QVR_CVAR(vr_gib_destroy, "1", CVAR_ARCHIVE)           // gibs and heads can be destroyed (shot, blown up, struck, thrown hard): they burst in a mist of blood
QVR_CVAR(vr_gib_health, "12", CVAR_ARCHIVE)           // the damage that destroys a gib (a head takes half as much again)
QVR_CVAR(vr_gib_splat_speed, "250", CVAR_ARCHIVE)     // units/s a thrown gib or head must hit a wall or a monster at to burst
QVR_CVAR(vr_corpse_gib, "1", CVAR_ARCHIVE)            // corpses can be gibbed: shot, blown up or struck enough, they burst into gibs
QVR_CVAR(vr_corpse_health, "80", CVAR_ARCHIVE)        // the damage that gibs a corpse (a big monster's takes more: an ogre's 1.75 times, a shambler's 3.5)
QVR_CVAR(vr_parry, "1", CVAR_ARCHIVE)                 // parry monsters' melee blows with a weapon held level across in front of you
QVR_CVAR(vr_parry_reduction, "0.75", CVAR_ARCHIVE)    // share of a parried blow's damage taken away
QVR_CVAR(vr_parry_drop_chance, "0.5", CVAR_ARCHIVE)   // chance a one-handed parry knocks the weapon out of your hand (two hands: never); not used with vr_parry_stamina on
QVR_CVAR(vr_parry_angle, "40", CVAR_ARCHIVE)          // degrees the weapon may be off level (tilted up or down) and still parry (the parry stance)
QVR_CVAR(vr_parry_muzzle_angle, "45", CVAR_ARCHIVE)   // degrees: a weapon whose far end (a gun's muzzle, a blade's tip) points within this of the attacker doesn't parry
QVR_CVAR(vr_parry_reach, "1.5", CVAR_ARCHIVE)         // metres in front of you a held weapon still parries
QVR_CVAR(vr_parry_unarmed, "1", CVAR_ARCHIVE)          // parry unarmed with the arms crossed in an X in front
QVR_CVAR(vr_parry_unarmed_reduction, "0.5", CVAR_ARCHIVE) // share of a blow taken off by crossed arms
QVR_CVAR(vr_bash, "1", CVAR_ARCHIVE)                   // hold a guard (a weapon across, both hands together), then push it forward to bash; or shove with one open palm
QVR_CVAR(vr_bash_speed, "2", CVAR_ARCHIVE)             // m/s the parry stance (a weapon across) must be pushed forward to bash (the author's pushes: 3-6 m/s; his held poses settle at up to 1.6)
QVR_CVAR(vr_shove_speed, "2.4", CVAR_ARCHIVE)          // m/s open palms (facing ahead, the arm extending) must be pushed forward to shove
QVR_CVAR(vr_bash_damage, "8", CVAR_ARCHIVE)            // a bash's damage (small: it knocks back and staggers)
QVR_CVAR(vr_bash_push, "1", CVAR_ARCHIVE)              // how far a bash throws what it hits
QVR_CVAR(vr_bash_sound, "1", CVAR_ARCHIVE)             // volume of the shove, bash, counter bash and parry sounds (0: the old ones)
QVR_CVAR(vr_bash_deflect_radius, "24", CVAR_ARCHIVE)   // units around the guard (the weapon, the hands) a bash bats projectiles back within (vr_deflect)
QVR_CVAR(vr_bash_deflect_window, "0.3", CVAR_ARCHIVE)  // s a bash (or a shove with a weapon in hand) goes on batting after the push
QVR_CVAR(vr_parry_wobble, "1", CVAR_ARCHIVE)          // how much a parried blow knocks your hand and arm
QVR_CVAR(vr_parry_stamina, "0", CVAR_ARCHIVE)          // parries tire you (one pool for both hands): each weapon parry costs stamina, and the one that empties it knocks the weapon out of the hand. On, it replaces vr_parry_drop_chance
QVR_CVAR(vr_parry_stamina_max, "100", CVAR_ARCHIVE)     // stamina when rested
QVR_CVAR(vr_parry_stamina_cost, "30", CVAR_ARCHIVE)     // stamina a parry with a weapon in one hand costs
QVR_CVAR(vr_parry_stamina_cost_2h, "12", CVAR_ARCHIVE)  // and with a weapon held in two hands
QVR_CVAR(vr_parry_stamina_delay, "2", CVAR_ARCHIVE)     // s after the last parry before stamina comes back
QVR_CVAR(vr_parry_stamina_regen, "25", CVAR_ARCHIVE)    // stamina a second it then comes back at
QVR_CVAR(vr_parry_stamina_warn, "1", CVAR_ARCHIVE)      // the warning when one more one-handed parry would knock the weapon away, and on the knock: volume of the gasp and strength of the buzz in the hand (0: off)
QVR_CVAR(vr_parry_stamina_show, "1", CVAR_NONE)         // retired in round 21 (the bar rising from the weapon; stamina shows on the gadget: vr_gadget_stamina): kept so saved configs load silently
QVR_CVAR(vr_strike_stamina, "1", CVAR_ARCHIVE)         // blows tire you too: each blow landing on something that takes damage costs stamina from the one pool (vr_parry_stamina_max; shown on the gadget); short of it, it deals less (vr_stamina_exhausted_damage). Rest: vr_parry_stamina_delay after the last parry, shove or blow
QVR_CVAR(vr_strike_stamina_punch, "4", CVAR_ARCHIVE)   // stamina a punch costs
QVR_CVAR(vr_strike_stamina_cost, "8", CVAR_ARCHIVE)    // a blow with a weapon held in one hand (a sword, an axe, Mjolnir, a gun; its pommel or butt too)
QVR_CVAR(vr_strike_stamina_cost_2h, "6", CVAR_ARCHIVE) // and with a weapon held in two hands
QVR_CVAR(vr_shove_stamina, "1", CVAR_ARCHIVE)          // shoves and bashes tire you: each one that lands costs stamina from the same pool; short of it, it throws back less (vr_stamina_exhausted_push) and hurts less
QVR_CVAR(vr_shove_stamina_cost, "15", CVAR_ARCHIVE)    // stamina a one-handed shove or bash costs
QVR_CVAR(vr_shove_stamina_cost_2h, "20", CVAR_ARCHIVE) // and a two-handed one
QVR_CVAR(vr_stamina_exhausted_damage, "0.5", CVAR_ARCHIVE) // a blow's, a shove's or a bash's damage with no stamina left for it (1: no penalty); with part of its cost left, in between
QVR_CVAR(vr_stamina_exhausted_push, "0.5", CVAR_ARCHIVE)   // a shove's or a bash's knockback and stagger with no stamina left for it (1: no penalty); with part of its cost left, in between
QVR_CVAR(vr_counter, "1", CVAR_ARCHIVE)                 // counter-attacks: after a parry, your next melee attack (a blow, a bash, a shove) within vr_counter_window hits harder
QVR_CVAR(vr_counter_window, "1.5", CVAR_ARCHIVE)        // s after a parry a counter may land
QVR_CVAR(vr_counter_damage, "1.5", CVAR_ARCHIVE)        // a counter's damage (and a bash's knockback) multiplier
QVR_CVAR(vr_counter_sound, "1", CVAR_ARCHIVE)           // volume of the counter's sounds: a blade's shing as the window opens, a heavy strike as a counter lands
QVR_CVAR(vr_counter_glow, "0", CVAR_ARCHIVE)            // while the window is open, what your hands hold glows gold round its edges and sheds embers, fading as it closes (vr_meleehud.cpp)
QVR_CVAR(vr_counter_haptic, "1", CVAR_ARCHIVE)          // strength of the soft pulses in the hands while the window is open (0: off)
QVR_CVAR(vr_dummy_attacks, "0", CVAR_NONE)               // the training dummy strikes at you in melee now and then (parry practice; the firing range's button). Not saved: off at every map load, and while a motion take plays
QVR_CVAR(vr_dummy_attack_period, "2.5", CVAR_ARCHIVE)    // s from one of the dummy's blows to the next (its wind-up included)
QVR_CVAR(vr_dummy_attack_jitter, "0.4", CVAR_ARCHIVE)    // s the period varies by at random, either way
QVR_CVAR(vr_dummy_attack_reach, "80", CVAR_ARCHIVE)      // units (origin to origin) within which it strikes; beyond, it waits (a knight's: 60)
QVR_CVAR(vr_dummy_attack_damage, "10", CVAR_ARCHIVE)     // a blow's damage, before a parry (as a monster's blow: armour, god mode and the parry apply)
QVR_CVAR(vr_dummy_attack_windup, "0.6", CVAR_ARCHIVE)    // s of wind-up (the tell: a sound, the rifle raised, a glow) before each blow
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
QVR_CVAR(vr_forcegrab_catch_blend, "0.15", CVAR_ARCHIVE)   // seconds a caught weapon takes to ease from how it flew into the hand (0 at once; visual only)
QVR_CVAR(vr_forcegrab_miss_speed, "0.2", CVAR_ARCHIVE)     // share of its speed a missed object keeps
QVR_CVAR(vr_forcegrab_eligible_particles, "1", CVAR_ARCHIVE)
QVR_CVAR(vr_forcegrab_eligible_haptics, "1", CVAR_ARCHIVE)
QVR_CVAR(vr_grapple_rope, "1", CVAR_ARCHIVE)             // grappling hook (QC vr_grapple.qc): 1 a rope, held at its length, that reels in only while the hand's B/Y is held; 0 Rogue's grapple, pulling you in as soon as it hooks
QVR_CVAR(vr_grapple_reel_speed, "450", CVAR_ARCHIVE)     // units/s the reel pulls you in (to walls, huge monsters, props too heavy to move)
QVR_CVAR(vr_grapple_min_length, "40", CVAR_ARCHIVE)      // the shortest the reel takes the rope (units)
QVR_CVAR(vr_grapple_unreel_speed, "300", CVAR_ARCHIVE)   // units/s the hand's A/X pays the rope out at (lets you down, lets a load go farther); 0: no unreel
QVR_CVAR(vr_grapple_unreel_airborne, "1", CVAR_ARCHIVE)  // the unreel button (A/X) unreels only in the air (on the ground it jumps); 0: anywhere (its key still pressed)
QVR_CVAR(vr_grapple_prop_speed, "650", CVAR_ARCHIVE)     // units/s a light prop comes in at (weapons, pickups, keys)
QVR_CVAR(vr_grapple_prop_light, "8", CVAR_ARCHIVE)       // kg up to which props come in at that speed; heavier, slower in proportion (40 kg: a fifth)
QVR_CVAR(vr_grapple_prop_anchor, "100", CVAR_ARCHIVE)    // kg from which a prop does not come: the reel pulls you to it
QVR_CVAR(vr_grapple_small_mass, "120", CVAR_ARCHIVE)     // kg (QC VR_Grapple_MonsterMass) up to which a monster is small: pulled fast, staggered
QVR_CVAR(vr_grapple_huge_mass, "400", CVAR_ARCHIVE)      // kg from which a monster is huge: the reel pulls you to it (in between: medium)
QVR_CVAR(vr_grapple_small_speed, "400", CVAR_ARCHIVE)    // units/s a small monster is reeled in at
QVR_CVAR(vr_grapple_medium_speed, "160", CVAR_ARCHIVE)   // and a medium one
QVR_CVAR(vr_grapple_stagger, "1", CVAR_ARCHIVE)          // small monsters reeled in are staggered (their pain, forced; again every second of reeling)
QVR_CVAR(vr_grapple_stamina, "0", CVAR_ARCHIVE)          // stamina a second reeling a monster in costs (the parry/shove/strike pool); short of it, it comes slower. 0: free
QVR_CVAR(vr_grapple_haptics, "1", CVAR_ARCHIVE)          // strength of the rope's haptics (the hook biting, the reel, the rope snapping taut); 0 none
QVR_CVAR(vr_grapple_quick_speed, "1800", CVAR_ARCHIVE)   // units/s the quick release (the button on the gun) brings the hook back at
QVR_CVAR(vr_grapple_drop_grace, "0.12", CVAR_ARCHIVE)    // seconds the trigger let go waits before the hook comes off: dropping or passing the gun by then keeps it in
QVR_CVAR(vr_grapple_rope_sim, "1", CVAR_ARCHIVE)         // the rope is a physical thing (vr_ropesim.cpp): it hangs, lies and wraps round the world and the props, never through them, and pulls along where it lies; 0: a straight line (drawn sagging)
QVR_CVAR(vr_grapple_rope_spacing, "12", CVAR_ARCHIVE)    // units between the simulated rope's points (finer: truer round edges, more work; a long rope's are farther apart: 96 points at most)
QVR_CVAR(vr_grapple_rope_iterations, "8", CVAR_ARCHIVE)  // passes a frame holding its pieces to their length (more: stiffer, truer, more work)
QVR_CVAR(vr_grapple_rope_radius, "1", CVAR_ARCHIVE)      // the simulated rope's thickness (units, half of it): how far from what it lies on
QVR_CVAR(vr_grapple_trigger_release, "0", CVAR_ARCHIVE)  // the trigger let go: 0 nothing (the hook stays in: the detach button on the gun takes it off); 1 it comes straight back (as the reel-in button); 2 it comes loose on its rope (reel it in with B/Y)
QVR_CVAR(vr_grapple_max_length, "1500", CVAR_ARCHIVE)    // units: the longest the rope is (the hook flies no farther; the unreel pays out no more)
QVR_CVAR(vr_grapple_loose_slack, "24", CVAR_ARCHIVE)     // units of slack the rope gets when the hook comes off (a loose hook lying there is not dragged by a step)
QVR_CVAR(vr_grapple_move_share, "0.7", CVAR_ARCHIVE)     // the share of your walking (the thumbstick) a load hanging on the rope takes (the loose hook, a hooked prop): it trails a little rather than lagging far behind and snapping forward; 0: none
QVR_CVAR(vr_grapple_load_drag, "1", CVAR_ARCHIVE)        // a second: how much of its speed a load on the rope loses to the air (its swing relative to you; a gun hanging on its own: all of it), so a swing dies down
QVR_CVAR(vr_grapple_hang_drag, "3", CVAR_ARCHIVE)        // a second: how much of its speed what hangs on a rope with no one holding it loses (a gun left hanging from a ceiling: it stops swinging in a couple of seconds)
QVR_CVAR(vr_grapple_load_max_speed, "700", CVAR_ARCHIVE) // units/s: the fastest a load on the rope moves relative to you (no rope of death); 0: no limit
QVR_CVAR(vr_grapple_shootable, "1", CVAR_ARCHIVE)        // shooting a hook that is in the world or a door (any weapon) takes it off
QVR_CVAR(vr_grapple_front_button, "1", CVAR_ARCHIVE)     // the grappling gun's front button (near the muzzle): the instant reel-in (the back one only detaches)
QVR_CVAR(vr_grapple_front_button_x, "6", CVAR_ARCHIVE)   // its place: from the back button along the gun (model units; +x towards the muzzle)
QVR_CVAR(vr_grapple_front_button_y, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_grapple_front_button_z, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_grapple_rope_depth, "2", CVAR_ARCHIVE)       // units inside the grappling gun's muzzle the rope is drawn from (in a hand, and a gun lying about); -1: the old places (the muzzle's end; a gun lying about: where the server has it)
QVR_CVAR(vr_grapple_sag, "1", CVAR_ARCHIVE)              // a slack rope is drawn hanging (0: always straight)
QVR_CVAR(vr_grapple_debug, "0", CVAR_NONE)               // with developer 1: what the hook bites and each reel (mass, class, speeds); 2 also the rope four times a second; 3 every frame the rope holds you
QVR_CVAR(vr_weapondrop_particles, "1", CVAR_ARCHIVE)
QVR_CVAR(vr_2h_spread_reduction, "0.5", CVAR_ARCHIVE)
QVR_CVAR(vr_2h_throw_velocity_mult, "1.0", CVAR_ARCHIVE)
QVR_CVAR(vr_2h_handoff, "1", CVAR_ARCHIVE)                // letting go of a two-handed weapon leaves it in the other hand: a sword changes hands, a gun hangs from its foregrip
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
QVR_CVAR(vr_enemy_liquid_damage, "1", CVAR_ARCHIVE)      // monsters in slime and lava burn as the player does (QC vr_liquids.qc)
QVR_CVAR(vr_explosion_rumble, "1", CVAR_ARCHIVE)         // explosions near you rumble in your hands
QVR_CVAR(vr_heartbeat, "1", CVAR_ARCHIVE)                // a heartbeat in your hands when your health is low
QVR_CVAR(vr_deflect, "1", CVAR_ARCHIVE)                  // melee swings bat monsters' projectiles back
QVR_CVAR(vr_deflect_radius, "14", CVAR_ARCHIVE)          // units around the weapon's line (or fist) a projectile is batted within
QVR_CVAR(vr_deflect_speed, "0.6", CVAR_ARCHIVE)          // a batting swing's speed (the weapon's far end), times vr_melee_speed (a blow's least is 1)
QVR_CVAR(vr_deflect_window, "0.2", CVAR_ARCHIVE)         // s a swing's path keeps batting (a swing a little early still bats)
QVR_CVAR(vr_deflect_bounce, "0.6", CVAR_ARCHIVE)         // a batted projectile's restitution off the weapon (0 dead .. 1 elastic): a little livelier than a ball off a bat
QVR_CVAR(vr_deflect_aim_assist, "0.5", CVAR_ARCHIVE)     // 0..1: a batted or bashed projectile going very near a monster (its thrower most) bends that share towards it
QVR_CVAR(vr_grenade_catch, "2", CVAR_ARCHIVE)            // grenades as physics you can catch and throw back: 0 off (Quake's), 1 the ogres', 2 also your own (the default since round 21's misc fixes: config 41)
QVR_CVAR(vr_grenade_held_fuse, "2.5", CVAR_ARCHIVE)      // s a caught grenade's fuse is set to (never shortened) as a hand takes it
QVR_CVAR(vr_grenade_fuse_regrab, "0", CVAR_ARCHIVE)      // 1: every catch sets the fuse again; 0: once a throw (the catch of the monster's throw)
QVR_CVAR(vr_grenade_catch_radius, "15", CVAR_ARCHIVE)   // cm round the palm an empty hand, not striking, catches a grenade in flight in (grip held or closing; QC vr_grenade.qc)
QVR_CVAR(vr_grenade_catch_window, "0.15", CVAR_ARCHIVE) // s an open palm holds a grenade that flies into it for the grip to close on it (else it drops from the hand)
QVR_CVAR(vr_grenade_return_full, "1", CVAR_ARCHIVE)     // a grenade you throw or bat back goes off as your grenade launcher's (120 damage, 160 units round), not its thrower's (an ogre's 40)
QVR_CVAR(vr_handgrenade, "1", CVAR_ARCHIVE)             // hand grenades: an empty hand gripping at the grenade pouch (the small of the back) takes one while you have rockets
QVR_CVAR(vr_handgrenade_fuse, "2.5", CVAR_ARCHIVE)      // s from arming a hand grenade to its going off (the launcher's is 2.5)
QVR_CVAR(vr_handgrenade_arm, "0", CVAR_ARCHIVE)         // 0: the trigger pulls the pin (let go of unarmed, it is a dud); 1: letting go arms it (the lever flies off)
QVR_CVAR(vr_ogre_aim_height, "1", CVAR_ARCHIVE)          // ogres (and zombies) lob at your height: a ballistic arc at their throw's speed (0: Quake's flat lob)
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
QVR_CVAR(vr_finger_grip_open, "0", CVAR_ARCHIVE)       // retired in round 21 (the fingers wrap the weapons; its finger tweaks instead): kept so saved configs load silently
QVR_CVAR(vr_hand_fit, "1", CVAR_ARCHIVE)               // round 21: with the jointed hand, the fingers wrap what the hand holds (guns' grips, a foregrip, carried boxes, gibs, the flashlight), closing joint by joint onto its triangles (vr_grasp.cpp)
QVR_CVAR(vr_weapon_hotspot, "1", CVAR_NONE)            // the Weapon Offsets page's hotspot being edited (1..4)
QVR_CVAR(vr_weapon_holster, "1", CVAR_NONE)            // the Weapon Offsets page's kind of holster whose Holstered pose is edited (1 hips, 2 chest, 3 back)
QVR_CVAR(vr_weapon_holster_preview, "1", CVAR_ARCHIVE) // while a Holstered setting is chosen there, the page's weapon is drawn in both holsters of that kind
QVR_CVAR(vr_show_weapon_hotspots, "0", CVAR_NONE)      // marks the held weapons' hotspots, where the other hand may hold them (the edited one white)
QVR_CVAR(vr_show_controller, "0", CVAR_NONE)            // round 21, third pass: draws each controller as tracked (at the runtime's grip pose, before any offset), translucent, with its axes
QVR_CVAR(vr_show_controller_laser, "0", CVAR_NONE)      // and, for a held weapon, lasers: the controller's aim (white), the weapon's aim from the muzzle (red), its barrel (green)
// Align Sights to My Aim (vr_sightalign.cpp): the eye that looks along the sights (0 right, 1 left); each held gun's sight
// line and its rear (yellow) and front (cyan) points; how many times the aim is captured (3..5), averaged.
QVR_CVAR(vr_dominant_eye, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_show_sight_line, "0", CVAR_NONE)
QVR_CVAR(vr_sight_align_captures, "4", CVAR_ARCHIVE)
// After the posing test: the Show Controller preview (a Quest 3 Touch Plus controller drawn at the runtime's grip pose)
// moved and turned to match the real controllers: centimetres along its drawn axes (x red: along the handle, y green:
// left, z blue: up), degrees (pitch up, yaw left, roll). The main hand's; the off hand's mirrors them (y, yaw and roll
// the other way) unless vr_show_controller_off_own is 1, then it takes its own.
QVR_CVAR(vr_show_controller_x, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_show_controller_y, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_show_controller_z, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_show_controller_pitch, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_show_controller_yaw, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_show_controller_roll, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_show_controller_off_own, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_show_controller_off_x, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_show_controller_off_y, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_show_controller_off_z, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_show_controller_off_pitch, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_show_controller_off_yaw, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_show_controller_off_roll, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_pose_weapon_hand, "1", CVAR_NONE)           // the weapon posing mode (vr_posing.cpp): the hand that holds the floating weapon and poses it (1 main, 0 off: mirrored); the other poses its hotspots and confirms
QVR_CVAR(vr_pose_solve, "0", CVAR_NONE)                 // and the posing hand wraps the weapon while posing (1: the grasp solved live, as before the posing test); 0: it passes through, unsolved, and shows the solved grip for a moment after each set
QVR_CVAR(vr_pose_reset_offsets, "0", CVAR_NONE)         // and confirming a pose sets the tuning offsets to 0 (the weapon's Hand and Weapon Together and Hand Only; a hotspot's Held Hand)
QVR_CVAR(vr_hand_fit_palm, "5", CVAR_ARCHIVE)          // with it: cm the drawn hand may move off the controller to sit flush on what it holds (0: stays on the controller)
QVR_CVAR(vr_hand_fit_palm_weapon, "3", CVAR_ARCHIVE)   // the same on a weapon (its own grip, a foregrip, a blade): cm (the weapon is placed in the hand by its settings)
QVR_CVAR(vr_hand_fit_palm_turn, "20", CVAR_ARCHIVE)    // and degrees it may turn to face the surface it holds (0: not at all)
QVR_CVAR(vr_hand_fit_overlap, "0.3", CVAR_ARCHIVE)     // round 21, second pass: cm the fingers and palm may sink into what they hold (snug, no gap); third pass: things only (a weapon, its hotspots: their own overlap sliders)
QVR_CVAR(vr_hand_fit_overlap_hands, "0.6", CVAR_ARCHIVE) // and cm a hand cupping the other (a two-handed pistol grip) may sink into it
QVR_CVAR(vr_hand_walls, "1", CVAR_ARCHIVE)            // round 21, second pass: the jointed hand's fingers bend (curl) where they would go into walls and floors
QVR_CVAR(vr_model_collide, "2", CVAR_ARCHIVE)         // round 21: held weapons stop at the models' drawn triangles (drawn only: the game reads the tracked hands; vr_modelcollide.cpp): 0 off, 1 monsters (corpses, other players), 2 things on the ground too
QVR_CVAR(vr_model_collide_hands, "1", CVAR_ARCHIVE)   // and so do empty hands (fists)
QVR_CVAR(vr_model_collide_max, "20", CVAR_ARCHIVE)    // cm a weapon or hand is held out of a model; deeper, less and less, and none at twice as deep (it lets go and passes through)
QVR_CVAR(vr_debug_model_collide, "0", CVAR_NONE)      // 1: print each hand's push while a model stops it; 2: and draw its rays (grey as tracked, green/red as drawn) and the push (yellow)
QVR_CVAR(vr_test_spawn, "0", CVAR_NONE)               // impulse 241 (tests): what it puts ahead of you: 0..17 the firing range's dispenser monsters (0 grunt, 1 ogre, 2 zombie, 3 shambler, ...), 100 a health box, 101 a shells box, 102/103 an explosive box/a small one, 104 an explosive box that never blows up
QVR_CVAR(vr_test_spawn_dist, "56", CVAR_NONE)         // and how far ahead (units)
QVR_CVAR(vr_test_spawn_dead, "0", CVAR_NONE)          // 1: a monster killed at once (a corpse)
QVR_CVAR(vr_test_fling_speed, "12", CVAR_NONE)        // impulse 232 (tests): the prop nearest you sent at the nearest monster at this many m/s (VR_Test_Fling)
QVR_CVAR(vr_test_fling_at, "0", CVAR_NONE)            // and at: 0 the nearest monster, 1 you (from ahead)
QVR_CVAR(vr_test_fling_away, "0", CVAR_NONE)          // 1: from inside the edge of its box, away from it (a prop batted away, still in your box)
QVR_CVAR(vr_test_projectile, "0", CVAR_NONE)          // impulse 246 (tests): the projectile fired at you: 0 a knight's spike, 1 a laser, 2 a scrag's spit, 3 a vore's ball, 4 an ogre's grenade, 5 a zombie's flesh (6: impulse 240's ogre throws a multi-grenade)
QVR_CVAR(vr_test_projectile_side, "0", CVAR_NONE)     // and from how many degrees to your left of ahead
QVR_CVAR(vr_hand_collide, "4", CVAR_ARCHIVE)          // cm a free hand is held out of the weapon in the other hand (drawn only; deeper, it lets go and passes through; 0: never)
QVR_CVAR(vr_hand_collide_fingers, "1", CVAR_ARCHIVE)  // with it: the free hand's fingers react to the other hand's weapon: they rest on it or bend out of it (0: only the hand is pushed, by its fingertips too)
QVR_CVAR(vr_body_collide, "1", CVAR_ARCHIVE)         // round 21: the drawn hands and held weapons stop at each other, the other arm, the wrist gadget and the body (drawn only: the game reads the tracked hands; vr_selfcollide.cpp); 0 off
QVR_CVAR(vr_body_collide_pass, "0.7", CVAR_ARCHIVE)  // pushed this share of the way through what stops it (the whole hand or weapon), it lets go and passes through (1: only once all the way through; held out at most this times 15 cm)
QVR_CVAR(vr_body_collide_elbows, "1", CVAR_ARCHIVE)  // with it: the elbows swing out of the torso rather than through it (the arm's IK)
QVR_CVAR(vr_debug_body_collide, "0", CVAR_NONE)      // 1: print each hand's drawn offset and its contacts every frame one is on; 2: and draw the proxies (capsules) and the contacts' pushes
QVR_CVAR(vr_hand_fit_blend, "0.12", CVAR_ARCHIVE)      // seconds the fingers, thumb and palm take to move to a new grasp (0: at once)
QVR_CVAR(vr_hand_fit_resolve, "0.3", CVAR_ARCHIVE)     // cm (and that many times 2 degrees) what the hand holds may move in it before the grasp is solved again (0: every frame)
QVR_CVAR(vr_debug_torch_lights, "0", CVAR_NONE)       // print each torch light lit this frame: which (a wall torch, a taken one), where, its radius and colour, shadowed or not
QVR_CVAR(vr_debug_grasp, "0", CVAR_NONE)               // print each grasp solve (1), and each finger's stops (2)
QVR_CVAR(vr_debug_grasp_trace, "0", CVAR_NONE)         // write the drawn hand's joints every frame to grasp_trace.txt in the game folder: 1 the main hand, 2 the off hand, 3 both
QVR_CVAR(vr_hand_rig, "1", CVAR_ARCHIVE)               // round 21: the jointed hand (progs/hand_rig.md5mesh: three joints a finger, an opposable thumb, continuous curls; vr_handrig.cpp). 0: the palm and five finger models as before
QVR_CVAR(vr_reload_mode, "2", CVAR_ARCHIVE)
QVR_CVAR(vr_show_weapon_text, "1", CVAR_ARCHIVE)
QVR_CVAR(vr_weapon_screen, "1", CVAR_ARCHIVE)          // the weapons' ammo counters on a small screen (colours: vr_gadget_screen_*)
QVR_CVAR(vr_weapon_screen_padding, "1", CVAR_ARCHIVE)  // room around the text on it
QVR_CVAR(vr_weapon_screen_crt, "1", CVAR_ARCHIVE)      // it looks like a small CRT, as the wrist gadget's (scanlines, a flicker, faint static, now and then a glitch): strength (0 off, up to 2)
QVR_CVAR(vr_weapon_screen_idle, "1", CVAR_ARCHIVE)     // the weapons in the holsters and lying in the world show their ammo screen and button too (0: only held ones)
QVR_CVAR(vr_weapon_morph_time, "0.4", CVAR_ARCHIVE)    // switching a gun's ammo (the button) morphs it into its other model over this many seconds (0 at once)
QVR_CVAR(vr_weapon_draw_blend, "0.3", CVAR_ARCHIVE)    // seconds a gun drawn from a holster takes to ease from its holstered pose into the hand (0 at once; visual only)
QVR_CVAR(vr_weapon_holster_blend, "0.3", CVAR_ARCHIVE) // seconds a gun put in a holster takes to ease from the hand into its holstered pose (0 at once)
QVR_CVAR(vr_debug_draw_blend, "0", CVAR_NONE)          // print each frame of those blends (the turn and distance left)
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
