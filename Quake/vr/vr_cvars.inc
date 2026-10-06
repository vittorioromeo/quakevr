// vr_cvars.inc -- every Quake VR cvar: QVR_CVAR(name, default, flags).
// Names carried over from the QSS-based Quake VR engine; defaults are the values the Quake VR
// releases shipped in id1/config.cfg.

// Gameplay rule changes (opt-in, enabled by quakevr/quakevr.cfg)
QVR_CVAR(vr_gameplayfix_droptofloor, "0", CVAR_NONE)  // corner traces instead of a box sweep
QVR_CVAR(vr_gameplayfix_touchsolids, "0", CVAR_NONE)  // .touch fires for solids, not just triggers
QVR_CVAR(vr_gameplayfix_missilesize, "0", CVAR_NONE)  // missile-vs-monster box extent (0: vanilla 15)
QVR_CVAR(vr_hull_width, "16", CVAR_ARCHIVE)            // Movement > Player Hitbox: the player's width against the map and brush models (8-32; 0: Quake's hull 1). vr_hull.cpp, docs/vr-port/HULLS.md
QVR_CVAR(vr_hull_method, "1", CVAR_ARCHIVE)            // how that is done: 0 the brush sweep (Quake 2's), 1 a clipping hull compiled for the width at load (qbsp's)
QVR_CVAR(vr_hull_ent_width, "-1", CVAR_ARCHIVE)        // the player's width against monsters, players and solid boxes (8-32; 0: Quake's 32; -1: vr_hull_width's)
QVR_CVAR(vr_hull_brushmodels, "1", CVAR_ARCHIVE)       // the narrow width against brush models too (doors, lifts, func_walls, external .bsp models), not only the world
QVR_CVAR(vr_hull_monsters, "1", CVAR_ARCHIVE)          // the narrow entity width between the player and monsters (both ways)
QVR_CVAR(vr_hull_players, "1", CVAR_ARCHIVE)           // ... between players
QVR_CVAR(vr_hull_boxes, "1", CVAR_ARCHIVE)             // ... between the player and other solid boxes (explosive boxes, solid items)
QVR_CVAR(vr_hull_hit_width, "24", CVAR_ARCHIVE)        // the player's width that shots, missiles and splash traces hit (not bodies moving: vr_hull_ent_width) (8-32; 0: Quake's 32)
QVR_CVAR(vr_hull_hit_head, "1", CVAR_ARCHIVE)          // ... and their box reaches only to the top of his head (the headset's, 5 units over it): crouched behind cover, shots over it miss; monsters' bullets aim no higher than 70% of it; 0: Quake's height, standing or not
QVR_CVAR(vr_mhull, "1", CVAR_ARCHIVE)                  // Movement > Monster Hitbox: monsters move against walls with the widths below (compiled hulls, vr_hull.cpp; HULLS.md "Monsters"); 0: Quake's hulls 1 and 2
QVR_CVAR(vr_mhull_ents, "1", CVAR_ARCHIVE)              // ... and a width narrower than a monster's own box meets other bodies (the player, monsters, solid boxes) too, both ways
QVR_CVAR(vr_mhull_ledges, "1", CVAR_ARCHIVE)            // ... and SV_CheckBottom's ledge test takes the narrow box's corners (0: Quake's box's)
QVR_CVAR(vr_mhull_army, "-1", CVAR_ARCHIVE)            // a class's width against walls: -1 its own box's (QC setsize; the default), 0 Quake's hull, or 16 up
QVR_CVAR(vr_mhull_dog, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_mhull_ogre, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_mhull_knight, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_mhull_hknight, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_mhull_zombie, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_mhull_wizard, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_mhull_demon, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_mhull_shambler, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_mhull_shalrath, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_mhull_enforcer, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_mhull_fish, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_mhull_tarbaby, "-1", CVAR_ARCHIVE)
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
QVR_CVAR(vr_hit_melee_scale, "1", CVAR_ARCHIVE)        // Combat > Hit Detection > Melee Hitbox Size: the melee probe (vr_hit_tolerance_melee) times this, 0.25..4. Not the striking points' own 3 units, not the broad-phase box.
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
// A throw's spin (and a wrist flick's speed through the lever arm), 1: from the controller's turn between tracking samples
// (vr_hands.cpp poseSpin); 0: the runtime's angular velocity, which OpenXR gives in the tracking space but
// VirtualDesktopXR 1.0.10 gives in the controller's own frame (takes of 2026-09-29: 0.97 against the turn in the
// controller's frame, 0.3-0.5 in the tracking space): a flick facing anywhere but the play space's front then spun
// the throw about another axis (an upright axe flicked end over end left the hand cartwheeling sideways). vr_angvel_frame
// now puts the runtime's in the tracking space too (both agree on any runtime; the turn leads the runtime's by about 2 samples).
QVR_CVAR(vr_throw_spin_from_pose, "1", CVAR_ARCHIVE)
// The frame of the controllers' angular velocity from the runtime (vr_angvel.cpp; ROUND21.md, "The runtime's angular
// velocity frame"): -1 auto (the runtime's name, then its samples: VirtualDesktopXR 1.0.10 gives the controller's own
// frame), 0 the tracking space as OpenXR says (as before 2026-10-01), 1 the controller's frame, 2 none: from the
// controller's turn between samples. Every consumer (melee's hand velocity, the hand calibration, flick reload, throws)
// gets it in the tracking space.
QVR_CVAR(vr_angvel_frame, "-1", CVAR_ARCHIVE)
// The controller's frame (vr_angvel_frame 1): the grip pose turned this many degrees about its x axis (fitted on the
// author's VirtualDesktopXR takes, Quest Touch).
QVR_CVAR(vr_angvel_local_pitch, "-57.5", CVAR_ARCHIVE)
// Prints each hand's angular velocity fix while it turns faster than 4 rad/s: the frame in use, the detection's
// evidence, the runtime's vector and the fixed one against the pose's turn (cosines), the hand's speed before and after.
QVR_CVAR(vr_debug_angvel, "0", CVAR_NONE)
QVR_CVAR(vr_throw_pitch, "0", CVAR_ARCHIVE)        // degrees every throw's direction is tilted up (negative: down), its speed kept
QVR_CVAR(vr_throw_gravity, "9.81", CVAR_ARCHIVE)   // m/s^2 on thrown weapons, bounces included (0: sv_gravity)
QVR_CVAR(vr_debug_throw, "0", CVAR_NONE)           // print each throw's estimate (2: and its timing)
QVR_CVAR(vr_debug_axestick, "0", CVAR_NONE)       // print each thrown axe's blade meeting something: stuck (how deep, at what angles) or why not
QVR_CVAR(vr_debug_missiles, "0", CVAR_NONE)        // print each missile's hit (rockets, nails, grenades...): what it met, where, the surface's normal; a solid prop met: its drawn box as it stands
QVR_CVAR(vr_debug_shots, "0", CVAR_NONE)           // print each hitscan shot: where it starts, its direction, what its pellets hit; each prop a shot pushes
QVR_CVAR(vr_debug_damage_numbers, "0", CVAR_NONE)  // every hit on anything that takes damage: its number floating where it struck, and a line in the console (as the training dummy's)
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
// Throws by weight (weight::throwVelocity; ROUND21.md, "Throws by weight"): a thing's release speed is limited by its mass,
// as a human's: up to vr_throw_mass_light kg (a grenade, the axe) at most vr_throw_max_speed m/s, heavier things less, as
// (light / mass)^vr_throw_mass_exp (0.5: the same energy; 1: the same impulse). The limit is soft: the speed rises as the
// hand's up to vr_throw_mass_knee of it, then eases towards it. A wrist flick (the part of the throw the hand's turn
// about the wrist gives, vr_throw_wrist_dist metres behind the controller's point) keeps all of its speed up to
// vr_throw_flick_mass kg, (flick mass / mass) of it beyond. Two hands throw as if the thing weighed
// 1 / vr_throw_2h_strength as much (a heavy thing farther, the light ones no faster). 0: off (only the gain and
// vr_throw_weight_influence, as before 2026-10-02).
QVR_CVAR(vr_throw_mass_model, "1", CVAR_ARCHIVE)
QVR_CVAR(vr_throw_max_speed, "28", CVAR_ARCHIVE)      // m/s: the most any throw leaves the hand at (a light thing's)
QVR_CVAR(vr_throw_mass_light, "1.5", CVAR_ARCHIVE)      // kg: up to this heavy, the whole vr_throw_max_speed (the author's 1.5 since 2026-10-02: the axe, 2 kg, a little less)
QVR_CVAR(vr_throw_mass_exp, "0.9", CVAR_ARCHIVE)     // how fast the limit falls with mass beyond it (the author's 0.9, 2026-10-02)
QVR_CVAR(vr_throw_mass_knee, "0.7", CVAR_ARCHIVE)     // 0..1: below this share of the limit a throw is untouched
QVR_CVAR(vr_throw_2h_strength, "2", CVAR_ARCHIVE)     // two hands: as if the thing weighed 1 / this (1: no help)
QVR_CVAR(vr_throw_flick_mass, "2.5", CVAR_ARCHIVE)    // kg: up to this heavy, a wrist flick's whole speed
QVR_CVAR(vr_throw_wrist_dist, "0.07", CVAR_ARCHIVE)   // metres from the wrist to the controller's point (the flick's lever)
// The spin a throw gives (the hand's turn; a prop held in both, its own), limited by mass as its speed is (weight::throwSpin):
// up to vr_throw_flick_mass kg the whole vr_throw_spin_max, heavier things (flick mass / mass)^this of it (two hands: as if
// vr_throw_2h_strength times lighter), soft from vr_throw_mass_knee. 1: a wrist's torque turning its inertia (the explosive
// box, 40 kg, 1.3 rad/s one-handed, 2.5 two); 0: off.
QVR_CVAR(vr_throw_spin_mass_exp, "1", CVAR_ARCHIVE)
QVR_CVAR(vr_throw_hitbox, "6", CVAR_ARCHIVE)       // half-size (units) of a thrown weapon's box against monsters
QVR_CVAR(vr_throw_hit_min_speed, "200", CVAR_ARCHIVE) // units/s a thrown thing must go at to hurt what it touches (slower: the throw is over)
// Wall buttons (QC buttons.qc; ROUND21.md, "Wall buttons: weapons and thrown things press them"): a hand presses one, and
QVR_CVAR(vr_button_weapon, "1", CVAR_ARCHIVE)          // 1: so does a held weapon touching it, all of it as its blows strike (pommel or butt to tip or muzzle); 0: only the line from the hand to a gun's muzzle (as before)
QVR_CVAR(vr_button_weapon_reach, "6", CVAR_ARCHIVE)    // cm round the weapon's line (its thickness and some slack) within which it touches a button
QVR_CVAR(vr_button_prop, "1", CVAR_ARCHIVE)            // 1: so does a held prop touching it (a health or ammo box, a crate, a rock, a gib): its shape as the hand holds it
QVR_CVAR(vr_button_prop_reach, "6", CVAR_ARCHIVE)      // cm round the held prop's shape within which it touches a button (a held body stops 4 cm short of a wall's)
QVR_CVAR(vr_button_throw, "1", CVAR_ARCHIVE)           // 1: a thing thrown at a button (a weapon, a rock, a box, a gib) presses it; shot buttons are shot, as in Quake
QVR_CVAR(vr_button_throw_speed, "150", CVAR_ARCHIVE)   // units/s it must hit the button at, at least
QVR_CVAR(vr_debug_wallbuttons, "0", CVAR_NONE)         // print each wall button pressed: what pressed it and how (hand, weapon, thrown thing and its speed, stepped on); 2: also each held weapon's line every frame; needs developer
// Throws' hits by weight (weight::throwHitSpeed, throwDamage; ROUND21.md, "Heavy throws' hits, spin and two-handed
// throws"): the hit speed above and the damage's speed scale (a thrown weapon's base at 700/120 m/s, in proportion) are
// times its heavy leniency (vr_weight_lenient*) and, for a thing too heavy to be thrown vr_throw_hit_top m/s one-handed
// (vr_throw_mass_model), its top speed over that (at least vr_weight_lenient_min): a hard throw of a heavy thing hurts
// as a hard throw of a light one. Just over the hit speed, the damage eases in: vr_throw_hit_ramp_floor of it at the hit
// speed, all of it vr_throw_hit_ramp (a share of the hit speed) above. 0 and 0: as before 2026-10-02 (heavy leniency
// only, the whole damage at once).
QVR_CVAR(vr_throw_hit_top, "15", CVAR_ARCHIVE)        // m/s
QVR_CVAR(vr_throw_hit_ramp, "0.5", CVAR_ARCHIVE)      // share of the hit speed the damage eases in over
QVR_CVAR(vr_throw_hit_ramp_floor, "0.6", CVAR_ARCHIVE) // share of the damage at the hit speed
// Two-handed throws (NOTES.md vrfiringrange_2026-10-02_00-54-03: a gib thrown with both hands burst on one's follow-through
// punch): neither hand strikes for vr_throw_2h_nomelee s after a two-handed throw (the hand let go first from just before,
// within vr_carry_two_hands_window), and what was thrown can't be struck by the thrower's hands for
// vr_throw_2h_melee_immune s. 0: off.
QVR_CVAR(vr_throw_2h_nomelee, "0.3", CVAR_ARCHIVE)
QVR_CVAR(vr_throw_2h_melee_immune, "0.35", CVAR_ARCHIVE)
// A gib let go of by one hand: the other hand's touch doesn't burst it for vr_gib_letgo_spare s (NOTES.md
// vrfiringrange_2026-10-02_15-38-26: the other hand alongside the throw; the hand that let go: 0.4 s). 0: off.
QVR_CVAR(vr_gib_letgo_spare, "0.2", CVAR_ARCHIVE)
QVR_CVAR(vr_gib_burst_on_thrower, "0", CVAR_ARCHIVE) // a thrown gib turned by its thrower's body or hands (no wall ahead) bursts as on a wall (0: kept, the same notes)
// Thrown axes stick (vr_axestick.cpp, QC/vr_axestick.qc): one whose blade goes into something first, fast enough and
// square enough, sticks in it (the level, a door, a prop, a monster: it rides with it); a hand or a force grab pulls it out.
QVR_CVAR(vr_axestick, "1", CVAR_ARCHIVE)             // on
QVR_CVAR(vr_axestick_speed, "2.5", CVAR_ARCHIVE)     // m/s the blade's edge must go at into what it meets (relative to it)
QVR_CVAR(vr_axestick_angle, "45", CVAR_ARCHIVE)      // degrees at most the blade may go in out of its own plane (its side first, a flat throw: more)
QVR_CVAR(vr_axestick_incidence, "65", CVAR_ARCHIVE)  // degrees at most between the way the blade goes in and straight into the surface (a glancing blow: more)
QVR_CVAR(vr_axestick_leniency, "1", CVAR_ARCHIVE)   // 0..2: how much of the blade behind its edge cuts when it goes in first (vr_axestick.cpp): 0 the edge and its corners; 1 45% of its width; 2 80%; also met a little ahead, a little more glancing
QVR_CVAR(vr_axestick_depth, "5", CVAR_ARCHIVE)       // cm the edge goes in at twice the least speed (half as deep at it; at most 60% of the blade)
QVR_CVAR(vr_axestick_bleed, "5", CVAR_ARCHIVE)       // health a second a monster an axe is stuck in loses (0: none), the thrower's damage
QVR_CVAR(vr_axestick_tug, "0.3", CVAR_ARCHIVE)       // seconds a force grab tugs a stuck axe before it comes free
QVR_CVAR(vr_axestick_metal, "0", CVAR_ARCHIVE)     // 1: thrown axes stick in the explosive boxes too; 0: they bounce off them (metal drums)
// Aim assist (QC; on, the author's values, NOTES.md vrfiringrange_2026-09-30_11-25-52): bends a throw by up to vr_throw_assist_strength onto a monster or
// breakable within vr_throw_assist_cone degrees of it (fully within vr_throw_assist_full), in
// range and in sight, on a ballistic arc, allowing vr_throw_assist_speed more speed.
QVR_CVAR(vr_throw_assist, "1", CVAR_ARCHIVE)
QVR_CVAR(vr_throw_assist_cone, "15", CVAR_ARCHIVE)
QVR_CVAR(vr_throw_assist_full, "4", CVAR_ARCHIVE)
QVR_CVAR(vr_throw_assist_strength, "0.35", CVAR_ARCHIVE)
QVR_CVAR(vr_throw_assist_range, "1200", CVAR_ARCHIVE)
QVR_CVAR(vr_throw_assist_speed, "0.15", CVAR_ARCHIVE)
// Thrown weapons and backpacks as rigid bodies (Box3D, vr_box3d.cpp): bounciness and friction of
// their contacts, their spin at most, and how fast the air slows it.
QVR_CVAR(vr_throw_restitution, "0.25", CVAR_ARCHIVE)
QVR_CVAR(vr_throw_friction, "0.6", CVAR_ARCHIVE)
QVR_CVAR(vr_throw_spin_max, "20", CVAR_ARCHIVE)    // rad/s
QVR_CVAR(vr_throw_spin_drag, "0.3", CVAR_ARCHIVE)  // per second, in the air
// A throw's spin in the air turned towards end over end about its steadiest axis (vr_box3d.cpp's spinAlign): per second,
// times how long the thing is ((1 - least / most inertia)^2) and its Weapon Weights' or Held Object Weights' Spin
// Alignment; 0 off. Until it first touches anything.
QVR_CVAR(vr_throw_spin_align, "8", CVAR_ARCHIVE)
QVR_CVAR(vr_debug_spin_align, "0", CVAR_NONE)   // 1: print each throw's spin as it leaves and when its flight ends; 2: every step

// Version of quakevr/vr_bindings.cfg last applied (vr_checkbindings).
QVR_CVAR(vr_bindings_version, "0", CVAR_ARCHIVE)

// Mock backend: swing the main hand in an overhand arc with this period, in seconds (0: still).
QVR_CVAR(vr_mock_swing, "0", CVAR_NONE)
// Mock backend: 1 moves the off hand with the swing too, rigidly (as it was from the main hand): both hands on one weapon.
QVR_CVAR(vr_mock_swing_both, "0", CVAR_NONE)
// Mock backend: 1 reports the grip's (the palm's) velocity too, as the OpenXR backend does for a Touch controller with
// vr_controller_legacy_pose (throws then go with the palm, as in the headset); 0 (the default): the raw pose's only.
QVR_CVAR(vr_mock_grip_velocity, "0", CVAR_NONE)
// Mock backend: 1 reports the hands' angular velocity in the controller's own frame (vr_angvel_local_pitch), and the
// legacy pose's velocity made with it, as VirtualDesktopXR 1.0.10 with the OpenXR backend's toLegacyPose does; its
// runtime name then says VirtualDesktopXR (vr_angvel_frame -1); 0 (the default): in the tracking space, as OpenXR says.
QVR_CVAR(vr_mock_angvel_local, "0", CVAR_NONE)
// Test runs: 1 makes every host frame 1/72 s of game time with a server frame (vr_motion_play.cpp hostFrameTime), so a
// scripted run (vr_mock_play, "wait") logs the same numbers every time, whatever the machine's pace.
QVR_CVAR(vr_fixed_frames, "0", CVAR_NONE)
// Test runs with vr_fixed_frames 1: the host frames a second of game time (72: a server frame with each, as before; 90: a
// headset's frame rate, the server's frames on its own 72 Hz clock between them: the drawn motion's smoothness tests,
// vr_drawn_motion_test).
QVR_CVAR(vr_fixed_frames_rate, "72", CVAR_NONE)
// Test runs: 1 runs the mock headset's frames as fast as the machine makes them while the game's clock doesn't follow the
// wall clock (vr_fixed_frames 1, a motion take played back not in watch mode): no frame cap, no vsync (VR_Unpaced). The
// same frames and the same numbers as at the frame cap's pace, in less time. Other runs keep their real-time pace.
QVR_CVAR(vr_mock_fast, "0", CVAR_NONE)

// Rigid-body physics: Box3D (docs/vr-port/ROUND21.md, "Box3D physics"; vr_box3d.cpp: props also collide with each
// other, so they stack and pile).
QVR_CVAR(vr_box3d_substeps, "4", CVAR_ARCHIVE)       // Box3D's sub-steps a server frame (1..8)
QVR_CVAR(vr_box3d_threads, "1", CVAR_ARCHIVE)        // Box3D's step shared between the game's thread pool and the main thread (vr_box3d.cpp stepWorkers); 0: the main thread alone (ROUND21.md, "Physics threads benchmarked": 25-70% less step time from 150 awake bodies)
QVR_CVAR(vr_box3d_threads_bodies, "150", CVAR_ARCHIVE) // with it: only while at least this many bodies are awake (fewer: the main thread alone, which is faster then; 0: always; measured: even at ~100, a gain from ~125)
QVR_CVAR(vr_box3d_workers, "4", CVAR_ARCHIVE)        // with it: at most this many threads step Box3D, the main thread one of them (0: as many as the pool has, up to 32; measured: 4 as fast as 8 up to ~500 awake, leaving cores to the render thread)
QVR_CVAR(vr_box3d_player_push, "1", CVAR_ARCHIVE)    // the players' bodies push props (a capsule each, one way)
QVR_CVAR(vr_box3d_hand_push_speed, "3", CVAR_ARCHIVE) // m/s, the most a hand shoves a solid prop at (the rest of its move it passes into it, eased out)
QVR_CVAR(vr_box3d_hand_push, "1", CVAR_ARCHIVE)      // the players' hands push solid props (an explosive box) as bodies: a sphere at each fist, one way (vr_box3d.cpp syncHands)
QVR_CVAR(vr_box3d_hand_push_fist, "1", CVAR_ARCHIVE) // with it: your own hands (a listen server) push solid props with their drawn fists (the grab's spheres, turned with the hand), so a box is pushed and taken where the fist meets it; 0: a 4.5 cm sphere at the hand's point (the old, which met a box up to 7 cm before the fist)
QVR_CVAR(vr_box3d_hand_props, "1", CVAR_ARCHIVE)     // the players' empty hands are bodies (the open hand, or the fist) at full speed: they push loose props (not solid ones, not grenades) and hold them up (vr_box3d.cpp syncReach)
QVR_CVAR(vr_box3d_weapon_push, "1", CVAR_ARCHIVE)    // the players' held weapons are bodies (the drawn model's hull) at full speed: they push loose props (grenades too: batted) and hold them up
QVR_CVAR(vr_box3d_hand_hold_mass, "20", CVAR_ARCHIVE) // kg, the heaviest prop an open hand or a weapon holds up (heavier: it slips through, pushed still); 0: no limit
QVR_CVAR(vr_box3d_throw_grace, "0.2", CVAR_ARCHIVE) // s, how long a thrown thing (a weapon, a prop) passes through its thrower's hands' bodies (the open hand, the fist, the held weapon, the fists' spheres): it leaves the hand without a knock; 0: off (vr_box3d.cpp noteThrows)
QVR_CVAR(vr_box3d_throw_grace_speed, "0.75", CVAR_ARCHIVE) // m/s, the throw (the hand's throw speed, or its spin at the thing's reach) from which vr_box3d_throw_grace applies: a slower release (opening the hand held still) keeps meeting the hand, so it rests on the palm
QVR_CVAR(vr_box3d_throw_grace_body, "1", CVAR_ARCHIVE) // during vr_box3d_throw_grace, the thrown thing also passes through its thrower's body (the player's capsule: vr_box3d_player_push)
QVR_CVAR(vr_box3d_hand_mass, "3", CVAR_ARCHIVE)     // kg, what an empty hand pushes with (and each hand holding a prop, with its own mass): a prop keeps hand / (hand + its mass) of the kinematic push (vr_box3d.cpp pushShare); 0: no limit
QVR_CVAR(vr_box3d_push_force, "200", CVAR_ARCHIVE)   // N, the most an arm shoves a prop with once it has hit it (a hand's body pushing on: vr_box3d.cpp limitPushes); 0: no limit
QVR_CVAR(vr_box3d_weapon_arm_mass, "1", CVAR_ARCHIVE) // kg, the arm behind a held weapon: it pushes with this and its own Mass (Weapon Weights: the shotgun 3 kg, 4 in all; the rocket launcher 9; vr_box3d.cpp reachMass); this and vr_box3d_hand_mass 0: no limit
QVR_CVAR(vr_shot_push, "1", CVAR_ARCHIVE)            // shots push the loose props they hit (QC VR_ShotPush): times every weapon's push below; 0: off
QVR_CVAR(vr_shot_push_pellet, "6", CVAR_ARCHIVE)     // N s, each shotgun and double shotgun pellet's push (monsters' too)
QVR_CVAR(vr_shot_push_nail, "12", CVAR_ARCHIVE)       // N s, a nail's (the nailgun's, scrags', knights', enforcers' lasers)
QVR_CVAR(vr_shot_push_supernail, "24", CVAR_ARCHIVE) // N s, a super nailgun nail's
QVR_CVAR(vr_shot_push_lightning, "6", CVAR_ARCHIVE)  // N s, each bolt of the lightning gun's beam (10 a second; a shambler's by its damage)
QVR_CVAR(vr_shot_push_speed, "10", CVAR_ARCHIVE)     // m/s, the most a shot makes what it hits go (light things: a gib, a small gun)
QVR_CVAR(vr_box3d_player_radius, "15", CVAR_ARCHIVE) // cm, that capsule's radius
QVR_CVAR(vr_box3d_player_push_speed, "2.5", CVAR_ARCHIVE) // m/s, the most a player's body shoves a prop at (the rest of its move it passes into it, eased out)
QVR_CVAR(vr_box3d_monster_push_speed, "3", CVAR_ARCHIVE) // m/s, the same for a monster's body (its steps jump 8-30 units a frame: kicked at that, a crate broke and hurt it); 0: unlimited
QVR_CVAR(vr_box3d_player_stand, "1", CVAR_ARCHIVE)   // players stand on solid props (explosive boxes): ground to walk and jump from, carried as it moves, the body not shoving what it stands on (vr_box3d.cpp, "Standing on props")
QVR_CVAR(vr_box3d_player_slope, "30", CVAR_ARCHIVE)  // degrees, the steepest face of a solid prop that is ground to a player (vr_box3d_player_stand); steeper, he slides off it as off a slope too steep to walk (Quake's own limit, about 45.6: he stood on it, creeping down at a few units a second)
QVR_CVAR(vr_box3d_player_mass, "80", CVAR_ARCHIVE)   // kg, the weight a player standing on a prop presses it down with (a box in water sinks); 0: none
QVR_CVAR(vr_box3d_player_jump_push, "0.25", CVAR_ARCHIVE) // the share of a jump's push (the player's mass times its speed) the prop jumped from takes, as a force over 0.15 s; 0: none
QVR_CVAR(vr_box3d_player_shove, "0.6", CVAR_ARCHIVE) // a player walking into a solid prop's side shoves it with at most this share of his weight (vr_box3d_player_mass), towards his speed shared by their masses (at most vr_box3d_player_push_speed); 0: it stops him like a wall
QVR_CVAR(vr_box3d_player_shape, "1", CVAR_ARCHIVE) // a player's box meets a solid prop's drawn box as it is turned (not the upright box round it), as a round column his width (the same distance from every side and from a wall); 0: Quake's box against its box
QVR_CVAR(vr_box3d_shot_shape, "1", CVAR_ARCHIVE) // shots and missiles (rockets, nails, grenades, lasers, Vore pods, gibs; the guns' and the grappling hook's traces) meet a solid prop's drawn box as it is turned, not the upright box round it: a turned box's empty corners let them by; 0: its box (the hook still meets its shape)
QVR_CVAR(vr_box3d_player_unstick, "1", CVAR_ARCHIVE) // a solid prop overlapping a player's box (toppled onto him) doesn't block his moves, and is pushed out of his body
QVR_CVAR(vr_box3d_player_hold, "1", CVAR_ARCHIVE) // ... but a player inside one only moves out of it, never deeper, and one with his feet a little into its top (landed on it, or it rocked up into him) is put back on top; and one he threw (its .owner) still meets his traces; 0: every move let through (he fell through a toppled box), his own thrown box passed
QVR_CVAR(vr_box3d_mesh_junctions, "1", CVAR_NONE)  // the world mesh with its T-junctions joined (no bumps at the seams between faces); 0: the faces as the BSP has them
QVR_CVAR(vr_debug_box3d, "0", CVAR_NONE)             // 1: bodies made, woken and put to sleep; 2: every awake body every frame
// Physics sounds (vr_physsound.cpp; ROUND21.md, "Physics sounds"): props knocking and scraping, a climbing hand's grab.
QVR_CVAR(vr_physsound, "2", CVAR_ARCHIVE)             // all of them: their volume (0 off; up to 2: over 1 the client multiplies them, vr_audio.cpp)
QVR_CVAR(vr_physsound_impact, "1", CVAR_ARCHIVE)      // props hitting the level, doors and each other: volume (0 off)
QVR_CVAR(vr_physsound_min_speed, "1.5", CVAR_ARCHIVE) // m/s, the slowest hit that knocks (a 4 cm drop; quietly)
QVR_CVAR(vr_physsound_full_speed, "10", CVAR_ARCHIVE) // m/s, a hit this hard or harder at full volume (a 1 m drop: 7.8)
QVR_CVAR(vr_physsound_interval, "0.12", CVAR_ARCHIVE) // s, a prop knocks at most this often (a harder hit, twice as loud, sooner)
QVR_CVAR(vr_physsound_scrape, "0.8", CVAR_ARCHIVE)    // props sliding with friction (dragged, shoved, skidding): volume (0 off)
QVR_CVAR(vr_physsound_scrape_min, "0.3", CVAR_ARCHIVE) // m/s, the slowest slide that scrapes
QVR_CVAR(vr_physsound_scrape_full, "3", CVAR_ARCHIVE) // m/s, a slide this fast or faster at full volume
QVR_CVAR(vr_physsound_grab, "0.7", CVAR_ARCHIVE)      // a climbing hand taking a hold (by the hold's texture: wood, metal, stone): volume (0 off)
QVR_CVAR(vr_debug_net, "0", CVAR_NONE)                // 1: each frame the unreliable broadcast (sv.datagram) was full: what QuakeC lost, what a client was not sent; 2: also each second the most sent (vr_server.cpp, VR_BroadcastDest)
QVR_CVAR(vr_debug_physsound, "0", CVAR_NONE)          // 1: each impact, scrape and grab played; 2: also the hits skipped and each scrape grain
// Spatial audio (vr_audio.cpp, Steam Audio; ROUND21.md, "Spatial audio (Steam Audio)"). Without phonon.dll: Quake's panning.
QVR_CVAR(vr_snd_spatial, "1", CVAR_ARCHIVE)           // 0 Quake's mixer as it was; 1 the features below in VR; 2 also on the desktop (headphones)
QVR_CVAR(vr_snd_hrtf, "1", CVAR_ARCHIVE)              // binaural (HRTF) from the head's pose; 0: Quake's left/right panning
QVR_CVAR(vr_snd_hrtf_interp, "1", CVAR_ARCHIVE)       // the HRTF between its measured directions: 1 bilinear (smooth), 0 nearest (cheaper)
QVR_CVAR(vr_snd_hrtf_gain, "1.5", CVAR_ARCHIVE)       // the binaural voices' volume against Quake's panned channels (1.25: as loud as its panning, all round; 1.5 the author's)
QVR_CVAR(vr_snd_hrtf_sofa, "", CVAR_ARCHIVE)          // an HRTF of your own: a .sofa file in the game folder (empty: Steam Audio's)
QVR_CVAR(vr_snd_fullband, "1", CVAR_ARCHIVE)        // Quake's 11 kHz lowpass (sndspeed 11025) kept off: 1 the binaural voices (their HRTF's highs: ahead/behind, up/down), 2 every sound; each read from a band-limited copy of its sound (not Quake's held samples, whose images would be hiss); 0 on everything, as Quake
QVR_CVAR(vr_snd_antialias, "1", CVAR_ARCHIVE)        // with vr_snd_fullband 0: the voices low-passed before Quake's 11 kHz lowpass (sndspeed 11025), whose every-fourth-sample trick folds their highs into the lows and mids (a sound at the side much more one-sided than its HRTF); 0 as before, to compare
QVR_CVAR(vr_snd_voices, "32", CVAR_ARCHIVE)           // how many of the loudest channels are rendered so (1-64); the rest are panned as ever
QVR_CVAR(vr_snd_limiter, "1", CVAR_ARCHIVE)          // the mix's look-ahead peak limiter (loud sounds piling up, such as boxes blowing up together, brought down smoothly, not clipped: no crackle), and the effects allowed the 6 dB Quake kept for the music; 0: Quake's hard clip of the effects
QVR_CVAR(vr_snd_limiter_ceiling, "-1", CVAR_ARCHIVE) // dB of the output's full scale, the limiter's ceiling (-12-0; Quake clipped the effects at -6)
QVR_CVAR(vr_snd_limiter_release, "0.15", CVAR_ARCHIVE) // s, the limiter's gain back up after a peak (0.02-1)
QVR_CVAR(vr_snd_frame, "256", CVAR_ARCHIVE)           // samples a voice renders at a time (256, 512, 1024): larger is cheaper, direction changes coarser
QVR_CVAR(vr_snd_reverb_beside, "1", CVAR_NONE)        // the reverb rendered on the calling thread while the pool does the voices' HRTF (1), or after them (0); the same sound, a matter of speed (vr_snd_bench)
QVR_CVAR(vr_snd_occlusion, "0.8", CVAR_ARCHIVE)       // walls and doors between a sound and the head make it quieter and duller: strength (0 off, 1 full)
QVR_CVAR(vr_snd_occlusion_samples, "8", CVAR_ARCHIVE) // rays per sound: 1 on or off; more: partly hidden (a sphere's points)
QVR_CVAR(vr_snd_occlusion_radius, "0.5", CVAR_ARCHIVE) // m, the sphere a sound comes from (partial occlusion)
QVR_CVAR(vr_snd_air, "1", CVAR_ARCHIVE)               // far sounds lose their highs in the air
QVR_CVAR(vr_snd_falloff, "0.75", CVAR_ARCHIVE)        // Quake's distance falloff, scaled (1 Quake's; 0.75 the author's, a bit further; 0.5: sounds carry twice as far; 0 none): every sound, spatial or not
QVR_CVAR(vr_snd_reverb, "0.5", CVAR_ARCHIVE)          // the room's reverb, simulated from the map where the head is: wet mix (0 off)
QVR_CVAR(vr_snd_reverb_quality, "1", CVAR_ARCHIVE)    // 0 parametric (cheapest), 1 convolution (2 s of the simulated response), 2 convolution (3 s, more rays)
QVR_CVAR(vr_snd_reverb_interval, "0.25", CVAR_ARCHIVE) // s between the room's simulations (a pool task)
QVR_CVAR(vr_snd_hands, "1", CVAR_ARCHIVE)             // your weapons' sounds from the hand that holds them, not the middle of the head
QVR_CVAR(vr_snd_follow, "1", CVAR_ARCHIVE)            // sounds follow their thing as it moves (a rocket, a monster, a door)
QVR_CVAR(vr_snd_doppler, "1", CVAR_ARCHIVE)           // the pitch of things coming and going: scale (0 off, 1 physical)
QVR_CVAR(vr_snd_nearfield, "1.2", CVAR_ARCHIVE)         // a sound within a metre of the head: the nearer ear louder, the farther shadowed: strength (0 off)
QVR_CVAR(vr_debug_snd, "0", CVAR_NONE)                // 1: the voices and the simulations' times each second; 2: also each voice

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
QVR_CVAR(vr_spectator_fov, "90", CVAR_ARCHIVE)        // spectator camera: horizontal field of view, degrees (40 .. 160; the slider 60 .. 150)
QVR_CVAR(vr_spectator_scale, "0.75", CVAR_ARCHIVE)    // spectator camera: its resolution, times the window's (0.25 .. 2)
QVR_CVAR(vr_spectator_rate, "60", CVAR_ARCHIVE)        // spectator camera: drawn every frame (1), every 2nd or 3rd (2, 3), or at most this many times a second (more than 3); the window shows the last between
QVR_CVAR(vr_spectator_bullettime_fx, "1", CVAR_ARCHIVE) // 1: bullet time's colour, tint and darkened edges in the spectator camera; 0: its normal look (headset unchanged)
QVR_CVAR(vr_spectator_preview, "1", CVAR_ARCHIVE)      // spectator camera: a small preview of its image in the headset's menus while it is on (bottom left, above its switch; vr_menuui.cpp)
QVR_CVAR(vr_spectator_aa, "1", CVAR_ARCHIVE)           // spectator camera: 1 the window's MSAA (vid_fsaa), as the eyes; 0 none
QVR_CVAR(vr_spectator_pos_smooth, "0.05", CVAR_ARCHIVE) // spectator camera: seconds, the steadied position's lag (0 none .. 0.3)
// Slow motion (vr_timescale.cpp; ROUND21.md, "Slow motion"): the whole simulation slowed (the server, the client's time,
// VR's own simulations on vr_gametime); the headset's view in real time; the hands follow their controllers slowed.
QVR_CVAR(vr_timescale, "1", CVAR_NONE)                 // the game's time per real second (0.05 .. 4; 1 normal); single player. Not saved: a game never starts slowed
QVR_CVAR(vr_slowmo_scale, "0.25", CVAR_ARCHIVE)        // the time scale vr_slowmo toggles to (bindable: bind x vr_slowmo)
QVR_CVAR(vr_timescale_ramp, "0.3", CVAR_ARCHIVE)       // real seconds the time scale takes to go all the way from 1 to 0 (or back), linearly (0: at once)
QVR_CVAR(vr_timescale_sound, "1", CVAR_ARCHIVE)        // 1: sounds play slowed and lower with the time scale (resampled); 0: at their normal speed and pitch
QVR_CVAR(vr_timescale_wav, "0", CVAR_ARCHIVE)          // 1: while Log Highlights runs, the game's sound also mixed a second time at normal speed in the game's time (no music; what you hear is unchanged) to highlights/<log>_gametime.wav (24-bit), a new file at each sync mark
QVR_CVAR(vr_timescale_hand_speed, "8", CVAR_ARCHIVE)   // m/s of the game's time: the most a hand moves in slow motion (a real-speed swing is no supersonic blow); 0: no limit
QVR_CVAR(vr_timescale_hand_spin, "20", CVAR_ARCHIVE)   // rad/s of the game's time: the most a hand turns in slow motion; 0: no limit
QVR_CVAR(vr_timescale_turn_realtime, "1", CVAR_ARCHIVE) // 1: the smooth turn at its real-time speed in slow motion (footage sped up shows it faster); 0: slowed with the world
QVR_CVAR(vr_timescale_move_realtime, "0", CVAR_ARCHIVE) // 1: the player's own movement (the stick, jumps, falls) and its weapons' timing in real time in slow motion; 0: slowed with the world
QVR_CVAR(vr_sandevistan, "0", CVAR_ARCHIVE)            // 1: in any slow motion the player runs in its own time (moves, turns, hands, weapons' timing, its missiles: vr_sandevistan_missiles), only the world slowed
QVR_CVAR(vr_sandevistan_missiles, "1", CVAR_ARCHIVE)   // in Sandevistan, the player's missiles (rockets, grenades, nails, thrown weapons, the hook) fly and think in its time too; 0: they join the slowed world
// Bullet time (vr_bullettime.cpp; ROUND21.md, "Slow motion: bullet time and Sandevistan"): the gadget's button slows the
// world for as long as the meter lasts.
QVR_CVAR(vr_bullettime_enabled, "1", CVAR_ARCHIVE)     // 1: a tap on the gadget's wrist (vr_bullettime_tap), the gadget's button (the inner one on its lower edge: vr_bullettime_button) and vr_bullettime start and stop it; 0: off
QVR_CVAR(vr_bullettime_scale, "0.3", CVAR_ARCHIVE)     // the world's time scale while it runs (0.05 .. 1)
QVR_CVAR(vr_bullettime_duration, "6", CVAR_ARCHIVE)    // real seconds a full meter lasts
QVR_CVAR(vr_bullettime_recharge, "20", CVAR_ARCHIVE)   // real seconds the meter takes to fill from empty (after the cooldown)
QVR_CVAR(vr_bullettime_cooldown, "2", CVAR_ARCHIVE)    // real seconds after it ends before it can start again (the meter waits too)
QVR_CVAR(vr_bullettime_min, "0.25", CVAR_ARCHIVE)      // the least meter it starts with (0.01 .. 1)
QVR_CVAR(vr_bullettime_sandevistan, "0", CVAR_ARCHIVE) // 1: while it runs, the player runs in its own time (as vr_sandevistan)
QVR_CVAR(vr_bullettime_fx, "1", CVAR_ARCHIVE)          // its look's strength (0 off .. 1; spectator: vr_spectator_bullettime_fx); never for the recording's vr_timescale
QVR_CVAR(vr_bullettime_fx_desat, "0.7", CVAR_ARCHIVE)  // ... how much of the colour it takes out (0 .. 1)
QVR_CVAR(vr_bullettime_fx_vignette, "0.5", CVAR_ARCHIVE) // ... how dark the view's edges (0 .. 1)
QVR_CVAR(vr_bullettime_fx_tint, "0.85 0.95 1.15", CVAR_ARCHIVE) // ... the colour it multiplies by (red green blue)
QVR_CVAR(vr_bullettime_button_radius, "1.5", CVAR_ARCHIVE) // units: how near the fingertip must come to the button's middle to press it
QVR_CVAR(vr_bullettime_button_reach, "1", CVAR_ARCHIVE) // units: the fingertip, ahead of the hand's place
QVR_CVAR(vr_bullettime_button, "1", CVAR_ARCHIVE)     // 1: the gadget's button starts and stops it; 0: not
// The wrist tap: the other hand (its own middle, never what it holds) struck against the gadget's wrist, hard enough.
QVR_CVAR(vr_bullettime_tap, "1", CVAR_ARCHIVE)        // 1: a tap on the gadget's wrist starts and stops it; 0: not
QVR_CVAR(vr_bullettime_tap_radius, "15", CVAR_ARCHIVE) // cm: how near the gadget's middle the other hand's middle must come
QVR_CVAR(vr_bullettime_tap_speed, "1", CVAR_ARCHIVE)  // m/s: the least speed of the hands towards each other (resting and brushing hands don't tap)
QVR_CVAR(vr_bullettime_tap_stop, "0.5", CVAR_ARCHIVE) // the impact: the hands' speed must fall to this much of its peak in the zone (a hand passing by doesn't tap); 1: on arrival
QVR_CVAR(vr_bullettime_tap_window, "0.25", CVAR_ARCHIVE) // seconds after the peak speed the impact may come
QVR_CVAR(vr_bullettime_tap_holding, "1", CVAR_ARCHIVE) // 1: the tapping hand may hold something (its hand taps, not what it holds); 0: only an empty hand
QVR_CVAR(vr_bullettime_tap_twohanded, "0", CVAR_ARCHIVE) // 1: taps count while both hands hold one weapon; 0: ignored then (the off hand on a foregrip)
QVR_CVAR(vr_bullettime_trigger_cooldown, "0.5", CVAR_ARCHIVE) // real seconds after a press or tap before the next counts (no double triggers)
QVR_CVAR(vr_bullettime_haptic, "1", CVAR_ARCHIVE)     // the tick in both hands when a press or tap counts (0 off .. 2)
QVR_CVAR(vr_bullettime_sound_on, "items/inv1.wav", CVAR_ARCHIVE)     // its sounds (empty: none)
QVR_CVAR(vr_bullettime_sound_off, "items/inv2.wav", CVAR_ARCHIVE)
QVR_CVAR(vr_bullettime_sound_denied, "misc/menu2.wav", CVAR_ARCHIVE) // pressed with the meter too low or cooling down
QVR_CVAR(vr_debug_bullettime, "0", CVAR_NONE)          // 1: prints it starting, stopping and refused, and each press and tap; 2: also the fingertip's distance to the button every frame; 3: the wrist tap's distance and speeds every frame
// The flat HUD (the status bar, hudstyle) over the window's view while in VR: over the mirror (Left Eye, Smoothed
// Mirror) and over the spectator camera (off: the headset's view, the wrist gadget its HUD, for recording).
QVR_CVAR(vr_window_hud_mirror, "1", CVAR_ARCHIVE)
QVR_CVAR(vr_window_hud_spectator, "0", CVAR_ARCHIVE)
// The head-locked text (centre prints, notify lines: pickups, messages) out of the window's view, for recording; the
// headset still shows it. The spectator camera's (on: trailers) and the mirror's (Left Eye, Smoothed Mirror; off).
QVR_CVAR(vr_spectator_hide_hud_text, "1", CVAR_ARCHIVE)
QVR_CVAR(vr_mirror_hide_hud_text, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_window_log, "0", CVAR_NONE)               // 1: print the head's and the window camera's angles every frame (tests)
// Highlight markers for trailers (vr_highlights.cpp; docs/vr-port/TRAILER.md): the game's cool moments logged in real
// time from a sync mark to <game dir>/highlights/ (CSV as they happen; JSON and Resolve's marker EDL as the log stops).
QVR_CVAR(vr_highlights, "0", CVAR_NONE)               // 1: logging (set: a new log and its sync mark; cleared: its JSON and EDL written). Not saved
QVR_CVAR(vr_highlights_flash, "0.2", CVAR_ARCHIVE)    // the sync mark's white flash over the desktop window (never the headset): real seconds (0: none)
QVR_CVAR(vr_highlights_beep, "1", CVAR_ARCHIVE)       // the sync mark's beep (vr/sync_beep.wav, 1 kHz): its volume (0: none)
QVR_CVAR(vr_highlights_fps, "60", CVAR_ARCHIVE)       // the marker EDL's frame rate: the recording's (OBS's) and the Resolve timeline's
QVR_CVAR(vr_highlights_min_score, "2", CVAR_ARCHIVE)  // the least score a moment needs to be a marker in the EDL (the CSV and JSON keep every one)
QVR_CVAR(vr_highlights_multikill, "3", CVAR_ARCHIVE)  // game seconds at most between two kills of one multi-kill
QVR_CVAR(vr_debug_highlights, "0", CVAR_NONE)         // 1: each highlight printed as it is logged

// Near clipping plane in the headset, in units (the desktop's is up to 4; a unit is 3 cm at World Scale 1.25):
// 0.02 .. 4 with the eyes' float depth (vr_depth_float), 1 .. 4 without (a 24-bit depth's precision far away).
QVR_CVAR(vr_nearclip, "0.1", CVAR_ARCHIVE)
// What you hold (weapons, hands) right at the eyes: 0 cut by the near plane like the rest, 1 never cut (depth clamp:
// parts nearer than the plane are drawn at it), 2 also both sides of it while an eye is inside its bounds (the
// inside of a gun pushed into your face blocks the view instead of showing through).
QVR_CVAR(vr_nearclip_held, "2", CVAR_ARCHIVE)
// The eyes' depth buffer: 1 32-bit float (with reversed Z, precise at any distance with a small near plane), 0 24-bit
// fixed point as the window's. Applies when the eyes' framebuffers are next made (vr_restart).
QVR_CVAR(vr_depth_float, "1", CVAR_ARCHIVE)

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
// Mock backend: its eye images' size (square, 256..4096; read when the images are made, before the first frame in VR).
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
// No main hand setting (ROUND21.md, "Handedness: separate options"): the hands are the physical ones, and each thing
// that has a side has its own setting: vr_stick_swap (which stick moves), vr_gadget_arm, vr_flashlight_side.
QVR_CVAR(vr_stick_swap, "0", CVAR_ARCHIVE) // 0: the left stick moves (and, Move Towards: Hand, the left hand steers), the right turns; 1: the other way round (hands::moveHand)
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
QVR_CVAR(vr_melee_speed, "3", CVAR_ARCHIVE)          // m/s the striking hand must reach (relative to the head) to hit: a punch, a pommel strike; a weapon's swing 1.25x, a stab 0.75x; twice this is a strength-1 blow (QC vr_melee.qc)
QVR_CVAR(vr_melee_punch_mult, "1.25", CVAR_ARCHIVE)   // damage of every punch (a fist's base damage times this)
QVR_CVAR(vr_melee_wrist_speed, "1.1", CVAR_ARCHIVE)   // m/s the hand itself (its wrist: the hand's turn taken out) must have moved on average over the last 0.12 s to hit (a punch twice this): a wiggle flicks the tip fast but barely moves the wrist (QC vr_melee.qc)
QVR_CVAR(vr_melee_pommel_wait, "0.05", CVAR_ARCHIVE)  // s a pommel's (a handle's end's, a gun butt's) touch waits for the blade or head to reach the same thing, so a cut from close by is a cut; only pommel strikes wait (QC vr_melee.qc)
QVR_CVAR(vr_melee_butt_run, "0.45", CVAR_ARCHIVE)   // m a gun's grip must have come towards a butt strike within 0.3 s (a gun wiggled spins in place, its butt fast: 0.34 at most; the author's butt strikes 0.54-0.73; 0.2: as every blow)
QVR_CVAR(vr_gunyaw, "4", CVAR_ARCHIVE)
QVR_CVAR(vr_gun_z_offset, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_sbar_mode, "1", CVAR_ARCHIVE)
QVR_CVAR(vr_hud_mode, "1", CVAR_ARCHIVE)              // 0 the status bar on a hand (vr_sbar_mode), 1 the wrist gadget
QVR_CVAR(vr_gadget_arm, "0", CVAR_ARCHIVE)            // the wrist gadget's arm: 0 the left, 1 the right (hands::gadgetHand)
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
QVR_CVAR(vr_tips, "1", CVAR_ARCHIVE)                  // tips for new players (vr_tips.cpp), each shown once, the first time you come near what it is about and can see it: 0 off, 1 a CRT screen floating by it (as the maps' text boards) with a cable to it, 2 in the wrist gadget's hologram (it waits to be seen, chiming from the gadget; the panel when there is no hologram)
QVR_CVAR(vr_tips_distance, "150", CVAR_ARCHIVE)       // how near (units, 32 a metre or so) you must come to what a tip is about
QVR_CVAR(vr_tips_line_of_sight, "1", CVAR_ARCHIVE)    // 1: only when nothing hides it from your eyes (the world's walls)
QVR_CVAR(vr_tips_view_angle, "35", CVAR_ARCHIVE)      // degrees from where you look it must be within (0: anywhere)
QVR_CVAR(vr_tips_delay, "0.5", CVAR_ARCHIVE)          // seconds it must stay near and seen before the tip shows
QVR_CVAR(vr_tips_time, "10", CVAR_ARCHIVE)            // seconds a tip shows (the hologram's: once seen)
QVR_CVAR(vr_tips_facing, "0", CVAR_ARCHIVE)           // the floating screen: 0 turned towards your eyes and level, 1 square to your view (flat in front of you, tilting with your head)
QVR_CVAR(vr_tips_size, "1", CVAR_ARCHIVE)             // the floating panel's text size (1: about 1.5 degrees a character)
QVR_CVAR(vr_tips_seen, "", CVAR_ARCHIVE)              // the tips shown already (their names, by spaces); vr_tips_reset empties it
QVR_CVAR(vr_sbar_offset_x, "-12", CVAR_ARCHIVE)
QVR_CVAR(vr_sbar_offset_y, "1", CVAR_ARCHIVE)
QVR_CVAR(vr_sbar_offset_z, "-3", CVAR_ARCHIVE)
QVR_CVAR(vr_sbar_offset_pitch, "1", CVAR_ARCHIVE)
QVR_CVAR(vr_sbar_offset_yaw, "1.6", CVAR_ARCHIVE)
QVR_CVAR(vr_sbar_offset_roll, "-0.3", CVAR_ARCHIVE)
QVR_CVAR(vr_height_calibration, "1.646099", CVAR_ARCHIVE)
QVR_CVAR(vr_menu_distance, "80", CVAR_ARCHIVE)
QVR_CVAR(vr_menu_status, "1", CVAR_ARCHIVE)            // a status box in the menus' top right corner (vr_menuui.cpp): VR / mock / flat, the runtime, the resolution rendered, the target rate, fps, CPU/GPU ms, RAM/VRAM
QVR_CVAR(vr_menu_vr_style, "1", CVAR_ARCHIVE)          // menus made for the headset (vr_menuui.cpp): filling the panel, rows spaced out, modern sliders and switches, a laser pointer
QVR_CVAR(vr_menu_flat_shortcuts, "1", CVAR_ARCHIVE)    // the VR menus' corner buttons (Back to game, Search, Console, Advanced VR, Levels, Map Library) on a flat screen too, for the desktop mouse (vr_menuui.cpp)
QVR_CVAR(vr_menu_laser_hue, "-1", CVAR_ARCHIVE)        // that laser's colour: hue (degrees; 35 amber; -1 vr_player_hue)
QVR_CVAR(vr_menu_bigfont, "1", CVAR_ARCHIVE)          // the main menu's rows as text in Quake's own lettering (vr_bigfont.cpp; 0: Quake's picture, VR Calibration above it in the console's letters)
QVR_CVAR(vr_menu_spacing, "1.5", CVAR_ARCHIVE)         // with it, the rows' spacing over Quake's (1 to 2)
QVR_CVAR(vr_menu_height, "1.35", CVAR_ARCHIVE)         // with it, the menus' height over Quake's 200 pixels (1 to 2): the VR pages and Ironwail's lists show more rows (1.35: about 35 degrees up and down at the shipped distance, scale and spacing)
QVR_CVAR(vr_menu_remember, "1", CVAR_ARCHIVE)          // opening the menu after "Back to game" (the panel's top-left button, or holding the menu button) returns to the page it closed on
QVR_CVAR(vr_menu_dropdown, "4", CVAR_ARCHIVE)          // a VR page's choice row with this many choices or more opens a list of them on a click or Enter / A (vr_menu.cpp; left and right still step; 0: never, a click steps)
QVR_CVAR(vr_setup_pending, "0", CVAR_ARCHIVE)         // 1: VR Calibration starts the next time the headset is on and no game runs (set at a first start, no saved config: vr_migrate_config new; cleared as it starts; vr_setup.cpp)
QVR_CVAR(vr_comfort_preset, "0", CVAR_NONE)          // VR Settings' Comfort (vr_menu.cpp presets; not saved: the settings it sets are): 1 Comfortable, 2 Moderate, 3 Full Freedom; 0 Custom (shown when the settings match none)
QVR_CVAR(vr_handedness, "0", CVAR_NONE)              // VR Settings' Handedness (likewise): 1 right-handed, 2 left-handed (vr_stick_swap, vr_gadget_arm, vr_flashlight_side together); 0 Custom
QVR_CVAR(vr_menu_level, "0", CVAR_ARCHIVE)             // Menu Detail (vr_menu.cpp MenuLevel): 0 Standard (what every player sets), 1 Advanced (every gameplay, display and graphics setting), 2 Developer (tuning, recording, debug and tests too)
QVR_CVAR(vr_menu_positions, "", CVAR_ARCHIVE)          // where each VR Settings page was left, for the next start (vr_menu.cpp, written with the config): "title|section|row|index|line;..."
QVR_CVAR(vr_checklist_hide_ticked, "1", CVAR_ARCHIVE)   // Checklist page (the menu's corner button): ticked items left out
QVR_CVAR(vr_melee_dmg_multiplier, "1.0", CVAR_ARCHIVE)
QVR_CVAR(vr_melee_range_multiplier, "1.0", CVAR_ARCHIVE)
// Quad Damage on melee, on top of the x4 every damage gets while Quad lasts (T_Damage): extra damage and extra reach.
// Quake VR v0.0.3 had x2.75 damage (11x in all) and 1.35/1.10 reach; 1 = melee is like any other weapon under Quad.
QVR_CVAR(vr_quad_melee_damage, "1", CVAR_ARCHIVE)
QVR_CVAR(vr_quad_melee_range, "1", CVAR_ARCHIVE)
// The lightning gun in water (QC weapons.qc VR_LGWater_*; vr_shock.cpp draws it). Fired with the body waist deep, or the
// hand or muzzle in a liquid, it shocks you and both hands drop what they hold; the shock spreads through the water to
// what is in it. Fired from the open into water, each bolt electrifies the water round where it goes in. Off: Quake's
// discharge (every cell at once, round you).
QVR_CVAR(vr_lg_water, "1", CVAR_ARCHIVE)
QVR_CVAR(vr_lg_water_self_damage, "40", CVAR_ARCHIVE) // the shock's damage to you
QVR_CVAR(vr_lg_water_damage, "60", CVAR_ARCHIVE)      // its damage to what else is in the water, at its middle (the muzzle)
QVR_CVAR(vr_lg_water_radius, "300", CVAR_ARCHIVE)     // how far through the water it (and the electrified water) reaches, units
QVR_CVAR(vr_lg_water_falloff, "1", CVAR_ARCHIVE)      // how it fades out to the radius: the power of the share of the radius left (1 evenly, 2 sooner, 0 not at all)
QVR_CVAR(vr_lg_water_tick_damage, "10", CVAR_ARCHIVE) // each bolt into water from the open: damage at the point it goes in (fading out the same way)
QVR_CVAR(vr_lg_water_flash, "1", CVAR_ARCHIVE)        // the shock's blue flash over the view and arcs in front of the eyes (0 none .. 1)
QVR_CVAR(vr_body_interactions, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_notes, "1", CVAR_ARCHIVE)                  // voice notes: off hand at the mouth, hold Y to talk (vr_voicenotes.cpp)
QVR_CVAR(vr_note_device, "Virtual Desktop", CVAR_ARCHIVE) // the microphone: part of its name (none such, or empty: the default; vr_note_devices lists them)
// The motion recorder (vr_motion.cpp, docs/vr-port/MOTIONS.md): takes of the player's motions for tuning the melee.
QVR_CVAR(vr_motion_armed, "0", CVAR_NONE)          // 1: the record button (vr_motion_button) starts and ends takes of the chosen category
QVR_CVAR(vr_motion_category, "0", CVAR_ARCHIVE)    // the category recorded, named by the result expected (an index: VR Settings > Advanced > Motion Recorder)
QVR_CVAR(vr_motion_detail, "0", CVAR_ARCHIVE)      // and its optional detail (an index into the category's; 0: none)
QVR_CVAR(vr_motion_note, "", CVAR_NONE)            // a free note written into each take's header (the "other" label's description)
QVR_CVAR(vr_motion_button, "0", CVAR_ARCHIVE)      // the record button: 0 the off stick click, 1 the main's, 2 A, 3 B, 4 X, 5 Y, 6 off grip, 7 main grip, 8 hold B + main trigger, 9 hold Y + off trigger (vr_motion.cpp recordBindings)
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
// Sticky two-handed grips (ROUND21.md, "Heavy weapons: wrenched out, sticky grips, heavy melee"; vr_twohand.cpp): once
// the helping hand holds a weapon's hotspot, how far off it it may go (20 units plus the hotspot's own Stickiness), the
// angle it may turn the aim off (vr_2h_angle_threshold) and how far past the muzzle it may go, before it lets go, are
// times vr_2h_sticky, and times 1 + vr_2h_sticky_fast more while the hands move fast (a swing: the faster hand's speed
// from vr_2h_sticky_fast_from to vr_2h_sticky_fast_full m/s), fading over vr_2h_sticky_fast_hold seconds once they slow.
QVR_CVAR(vr_2h_sticky, "1", CVAR_ARCHIVE)              // times the let-go distance and angle, always (1: as before)
QVR_CVAR(vr_2h_sticky_fast, "3.5", CVAR_ARCHIVE)       // and up to this many times more while swinging (0: never)
QVR_CVAR(vr_2h_sticky_fast_from, "1.5", CVAR_ARCHIVE)  // m/s: the faster hand's speed it starts from
QVR_CVAR(vr_2h_sticky_fast_full, "4", CVAR_ARCHIVE)    // m/s: all of it from this speed
QVR_CVAR(vr_2h_sticky_fast_hold, "0.6", CVAR_ARCHIVE)  // seconds it fades over once the hands slow
QVR_CVAR(vr_debug_2h_grip, "0", CVAR_NONE)             // 1: prints a helping hand taking a weapon's grip, and why it let go (and the stickiness then); 2: and where the grip is, four times a second
// Weight (vr_weight.cpp; ROUND21.md, "Weight: spring model, stamina, held object offsets; explosive boxes" and "Spring
// only; Weapon Weights and Held Object Weights; weight and damage"): what a hand holds (a weapon, a carried prop, the
// flashlight) follows it through a damped spring, by its mass, balance and length. The Spring is the only model.
QVR_CVAR(vr_weight_spring_stiffness, "1", CVAR_ARCHIVE)  // Spring: times the arm's and wrist's stiffness (2: 41% quicker)
QVR_CVAR(vr_weight_spring_damping, "1", CVAR_ARCHIVE)    // Spring: the damping ratio (1 critical: settles without swinging back; less: overshoots)
QVR_CVAR(vr_weight_spring_strength, "1", CVAR_ARCHIVE)   // Spring: times the arm's force and the wrist's torque (0: unlimited)
QVR_CVAR(vr_weight_spring_sag, "1", CVAR_ARCHIVE)        // Spring: times the droop of heavy things under gravity (more at arm's length)
QVR_CVAR(vr_weight_spring_inertia, "1", CVAR_ARCHIVE)    // Spring: times the swing a moving hand gives what it holds (its centre of mass trailing)
QVR_CVAR(vr_weight_spring_2h, "4", CVAR_ARCHIVE)         // Spring: times the stiffness and strength with a two-handed grip
QVR_CVAR(vr_weight_spring_roll, "2.5", CVAR_ARCHIVE)       // Spring: times the inertia about the forward axis (a roll of the wrist: 2, 41% slower)
QVR_CVAR(vr_weight_spring_snap, "60", CVAR_ARCHIVE)      // Spring: cm (or 3 times as many degrees; past 165 always) off the hand beyond which it is put back in it at once
QVR_CVAR(vr_weight_stamina, "1", CVAR_ARCHIVE)           // tired, things weigh more: below vr_weight_stamina_from of the stamina (Parry Stamina) the weight grows
QVR_CVAR(vr_weight_stamina_from, "0.5", CVAR_ARCHIVE)    // the stamina left (0..1) below which it starts
QVR_CVAR(vr_weight_stamina_max, "2.5", CVAR_ARCHIVE)     // times the weight with none left
QVR_CVAR(vr_weight_stamina_curve, "2", CVAR_ARCHIVE)     // the curve's exponent from the threshold to none (1 straight; 2 gentle at first)
QVR_CVAR(vr_weight_stamina_add, "15", CVAR_ARCHIVE)      // kg added to what a hand holds with none left (on the same curve; with the times above)
QVR_CVAR(vr_weight_stamina_empty, "15", CVAR_ARCHIVE)    // kg an empty hand weighs with none left (on the same curve): it follows its controller as if it held that (0: an empty hand never lags; not a hand on a hold or steadying a weapon)
QVR_CVAR(vr_stamina_speed, "1", CVAR_ARCHIVE)           // tired, you run slower: the most walking speed (sv_maxspeed) drops on the weight's curve (vr_weight_stamina_from, _curve)
QVR_CVAR(vr_stamina_speed_min, "0.5", CVAR_ARCHIVE)     // times the most speed with no stamina left (1: no slower)
QVR_CVAR(vr_debug_stamina_speed, "0", CVAR_NONE)        // tests: 1 prints the stamina, the speed cap and the player's ground speed twice a second (a local game)
QVR_CVAR(vr_weight_throw_mass, "12", CVAR_ARCHIVE)       // vr_throw_mass_model 0 only: kg: props up to this heavy are thrown with the hand's whole speed, heavier ones slower (Throw: Held Object Offsets)
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
// Round 21, "Heavy weapons: wrenched out, sticky grips, heavy melee" (config 59): from 5 kg (was 10) and 0.5 (was 0.75),
// so heavy weapons strike slower: the rocket launcher (8 kg) at 0.79 of the speeds, the laser cannon and the chainsaw
// (15 kg) at 0.58, a 25 kg box 0.45 (0.50 before).
QVR_CVAR(vr_weight_lenient, "0.5", CVAR_ARCHIVE)         // the exponent (0: off)
QVR_CVAR(vr_weight_lenient_from, "5", CVAR_ARCHIVE)      // kg: lighter things (the fists, the axe, swords, the shotguns, the nailgun) keep the thresholds
QVR_CVAR(vr_weight_lenient_min, "0.25", CVAR_ARCHIVE)    // the least factor
// Wrenched out (experimental; ROUND21.md, "Heavy weapons: wrenched out, sticky grips, heavy melee"; vr_weight.cpp): a
// weapon at least vr_weight_drop_from kg (its own mass) falls out of the hand when the controller turns faster than
// vr_weight_drop_speed degrees a second (over vr_weight_drop_window s: a snap or a twist of the wrist) times
// (from / mass)^vr_weight_drop_curve, times vr_weight_drop_2h held in both hands. The controller's own turn as tracked:
// the stick's turning, walking and the drawn weapon's lag don't count.
QVR_CVAR(vr_weight_drop, "1", CVAR_ARCHIVE)            // 1: on
QVR_CVAR(vr_weight_drop_from, "10", CVAR_ARCHIVE)      // kg: lighter weapons never fall (the laser cannon and the chainsaw weigh 15)
QVR_CVAR(vr_weight_drop_speed, "1900", CVAR_ARCHIVE)   // degrees a second a weapon of vr_weight_drop_from kg may turn at
QVR_CVAR(vr_weight_drop_curve, "1", CVAR_ARCHIVE)      // heavier, times (from / mass)^this (1: 15 kg, 2/3 of it: 1267)
QVR_CVAR(vr_weight_drop_2h, "1.5", CVAR_ARCHIVE)       // times the speed with the other hand on it too
QVR_CVAR(vr_weight_drop_window, "0.06", CVAR_ARCHIVE)  // seconds the turn is measured over
QVR_CVAR(vr_weight_drop_velocity, "0.4", CVAR_ARCHIVE) // times a throw's velocity and spin, wrenched out (the grip lost: no throw)
QVR_CVAR(vr_debug_weight_drop, "0", CVAR_NONE)         // 1: prints each hand's fastest turn against its limit (above half of it), twice a second
QVR_CVAR(vr_debug_weight_stamina, "-1", CVAR_NONE)      // tests: the stamina the weight takes as left (0..1) instead of the game's (-1: the game's)
QVR_CVAR(vr_debug_stamina_hold, "0", CVAR_NONE)          // tests: 1 keeps the game's stamina where it is, or where vr_stamina_set put it (a local game; vr_fatigue.cpp)
QVR_CVAR(vr_debug_fatigue, "0", CVAR_NONE)               // 1: the tired arms' shake printed four times a second; 2: and each frame the aim, the muzzle and the drawn weapon ("fatigueaim")
QVR_CVAR(vr_debug_pain, "0", CVAR_NONE)                  // 1: each hit ("painhit": damage, side, knock, buzz) and the hands' knock each frame ("painknock", cm along the body); 2: also where each hand and its weapon are drawn and the eye ("painview"; Misc/quakevr/painknock/painsum.py)
QVR_CVAR(vr_debug_weight, "0", CVAR_NONE)                // 1: each hand's target and drawn pose, a line a frame in weight_trace.txt; 2: and printed
QVR_CVAR(vr_props_version, "50", CVAR_ARCHIVE)           // the held object settings' changes the config has taken (vr_props.cpp)
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
// Wooden crates (vr_crates.cpp, QC vr_crates.qc; ROUND21.md, "Wooden crates"): placed at map load by walls and in
// corners, heavy physics props that break into light pieces, sometimes holding ammo or health; they hide you from monsters.
QVR_CVAR(vr_crates, "1", CVAR_ARCHIVE)                   // 1: crates placed about the maps (next map; single player); 0: none
QVR_CVAR(vr_crates_chance, "0.06", CVAR_ARCHIVE)         // the chance a spot by a wall (one every 24 units along it) gets a crate, before the limits
QVR_CVAR(vr_crates_corner, "4", CVAR_ARCHIVE)            // times the chance in a corner (a wall beside the spot along the wall)
QVR_CVAR(vr_crates_max, "24", CVAR_ARCHIVE)              // most crates in a map (a stack counts two)
QVR_CVAR(vr_crates_spacing, "224", CVAR_ARCHIVE)         // units between crate spots
QVR_CVAR(vr_crates_stack, "0.25", CVAR_ARCHIVE)          // the chance a crate has another on top (turned and off centre a little)
QVR_CVAR(vr_crates_large, "0.4", CVAR_ARCHIVE)           // the share of large crates (the rest small)
QVR_CVAR(vr_crates_clearance, "72", CVAR_ARCHIVE)        // units of open floor kept in front of a crate (a passage stays passable)
QVR_CVAR(vr_crates_margin, "56", CVAR_ARCHIVE)           // units from any other entity (doors, lifts and teleporters more, the start most)
QVR_CVAR(vr_crates_seed, "0", CVAR_ARCHIVE)              // mixed with the map's name: another layout (the same each load)
QVR_CVAR(vr_crates_exclude, "vrfiringrange vrclimb vrexample vrcalibration", CVAR_ARCHIVE) // maps with none (names, spaces between)
QVR_CVAR(vr_crate_health, "75", CVAR_ARCHIVE)            // a small crate's health (a large one's by its size: 1.6 times); 0: can't be broken
QVR_CVAR(vr_crate_impact, "16", CVAR_ARCHIVE)            // m/s: a crate hitting something this hard breaks (less: damaged from 70% of it); 0: never
QVR_CVAR(vr_crate_pieces, "12", CVAR_ARCHIVE)             // pieces a small crate breaks into (a large one more, by its size)
QVR_CVAR(vr_crate_piece_max, "48", CVAR_ARCHIVE)         // most pieces lying about at once (the oldest turn to dust)
QVR_CVAR(vr_crate_piece_time, "60", CVAR_ARCHIVE)        // s a piece lies about before it fades away (0: for ever)
QVR_CVAR(vr_crate_ammo, "0.35", CVAR_ARCHIVE)            // the chance a crate holds a small box of ammo for a weapon you have (or the shotgun)
QVR_CVAR(vr_crate_health_box, "0.2", CVAR_ARCHIVE)       // the chance a crate holds a small health box
QVR_CVAR(vr_crate_item_pop, "3.5", CVAR_ARCHIVE)        // how hard what a crate held pops out when it breaks (m/s: up, and a little outwards)
QVR_CVAR(vr_crate_crowbar, "0.1", CVAR_ARCHIVE)          // the chance a crate placed about the map (the top of a stack) has a crowbar lying on it (next map)
QVR_CVAR(vr_crate_crowbar_max, "4", CVAR_ARCHIVE)        // most crowbars on crates in a map
QVR_CVAR(vr_crate_sight, "1", CVAR_ARCHIVE)              // 1: crates and explosive boxes lying about hide you from monsters (not one you carry)
QVR_CVAR(vr_crate_shield, "2", CVAR_ARCHIVE)             // a crate you hold stops monsters' bullets (it takes them); 2: explosive boxes too; 2: an explosive box too (it blows up); 0: they pass through what you hold
QVR_CVAR(vr_debug_crates, "0", CVAR_NONE)                // 1: a line per map (crates, spots, rejections, the layout's hash); 2: each crate and its clearance; 3: each spot rejected
QVR_CVAR(vr_debug_item_sizes, "0", CVAR_NONE)            // 1: each ammo or health box as it is first drawn, and each change of its drawn scale (console)
// Wall torches taken off their walls (QC vr_walltorch.qc, vr_walltorch.cpp; ROUND21.md, "Wall torches you can take").
QVR_CVAR(vr_walltorch, "1", CVAR_ARCHIVE)                // 1: id's wall torches can be pulled out (by hand or force grab) and held as a burning club; 0: fixed, as id's (next map)
QVR_CVAR(vr_walltorch_pull, "8", CVAR_ARCHIVE)           // cm a hand gripping a torch on its wall pulls it before it comes out
QVR_CVAR(vr_walltorch_reach, "15", CVAR_ARCHIVE)         // cm from a torch's stick (its axis, butt to flame) to the hand's point or its fist's middle within which a hand's grip takes it off its wall though the fist isn't on it (0: only the fist on it)
QVR_CVAR(vr_walltorch_grab_time, "0.6", CVAR_ARCHIVE)    // s: a grip pressed with the hand empty keeps reaching for a torch on its wall while it stays held, this long after the press, the hand slower than a punch (a fist closed on the way takes the torch as it gets there); 0: only the press there (0.2 s, as a box)
QVR_CVAR(vr_walltorch_forcegrab, "1", CVAR_ARCHIVE)      // 1: torches are force grabbed off their walls, and lying about
QVR_CVAR(vr_walltorch_hits, "5", CVAR_ARCHIVE)           // blows a torch lands before its fire starts dying, even held (0: blows never use it up)
QVR_CVAR(vr_walltorch_die_time, "6", CVAR_ARCHIVE)       // s a dying torch's fire takes to go out (dropped, thrown, or out of blows); held, it never dies otherwise
QVR_CVAR(vr_walltorch_damage, "12", CVAR_ARCHIVE)        // a torch's blow: its base damage, times the blow's strength (a gun's is 12, the axe's 20)
QVR_CVAR(vr_walltorch_relight, "1", CVAR_ARCHIVE)        // 1: a dying or burnt-out torch held in a burning torch's flame, the map's flames (light_flame_*) or lava lights again
QVR_CVAR(vr_walltorch_shot, "1", CVAR_ARCHIVE)           // 1: a wall torch takes damage so that what you shoot meets it: a hitscan shot, a nail, a rocket, a grenade, a laser or a thing thrown into it hard knocks it off its wall (it falls lit; melee blows and your hand take it as before). 0: shots go through it, as id's (next map)
QVR_CVAR(vr_walltorch_flame, "1", CVAR_ARCHIVE)          // a taken torch's flame's size (1: the wall torch's)
QVR_CVAR(vr_walltorch_shadows, "1", CVAR_ARCHIVE)        // 1: a taken torch's light casts shadows (one of vr_shadow_dlights' slots), whatever vr_torch_light_shadows says
// A torch's flame (vr_walltorch.cpp; ROUND21.md, "Torch flames: upright from the head's top, swings, smoke, burning you"; Combat > Burning > Torch Flame).
QVR_CVAR(vr_walltorch_lean, "1", CVAR_ARCHIVE)           // how far a swung torch's flame leans and trails behind its motion, times the default (0: always straight up)
QVR_CVAR(vr_walltorch_flatten, "1", CVAR_ARCHIVE)        // how much a fast-swung torch's flame flattens and stretches back, times the default (0: never)
QVR_CVAR(vr_walltorch_hand_motion, "3", CVAR_ARCHIVE) // strength of hand motion relative to locomotion in the flame's lean/stretch
QVR_CVAR(vr_walltorch_motion_smooth, "0.08", CVAR_ARCHIVE) // seconds of flame velocity smoothing
QVR_CVAR(vr_walltorch_inv_size, "0.15", CVAR_ARCHIVE)    // fully inverted flame height; clamp 0.05..0.3 keeps it short and visible
QVR_CVAR(vr_walltorch_inv_light, "1.3", CVAR_ARCHIVE)    // a torch held upside down: its light's brightness, times the upright one's
QVR_CVAR(vr_walltorch_drips, "5", CVAR_ARCHIVE)          // burning drips a second falling off a torch held upside down (0: none)
QVR_CVAR(vr_walltorch_smoke, "1", CVAR_ARCHIVE)          // smoke off every lit torch (on its wall, held, lying), times the default (3 puffs a second; 0: none)
QVR_CVAR(vr_walltorch_smoke_alpha, "0.45", CVAR_ARCHIVE) // how opaque a torch's smoke is (0..1)
QVR_CVAR(vr_walltorch_inv_smoke, "2.5", CVAR_ARCHIVE)    // a torch held upside down smokes this much more
QVR_CVAR(vr_walltorch_debug, "0", CVAR_NONE)             // 1: each taken lit torch's flame in the console twice a second, 2 every frame (wtflame: how far upside down, its foot, speed, flattening, what of you it touches)
// Burning (QC vr_burning.qc; ROUND21.md, "Burning"; Combat > Burning): a lit torch's blow, a thrown lit torch, a lava nail.
QVR_CVAR(vr_burn_damage, "4", CVAR_ARCHIVE)              // damage a second a fire does, however many flames or hits (0: nothing alive is set on fire)
QVR_CVAR(vr_burn_time, "3", CVAR_ARCHIVE)                // s a monster burns from the last hit that lit it
QVR_CVAR(vr_burn_flames, "4", CVAR_ARCHIVE)              // flames a fire spreads to over the body on its own (1: just the first)
QVR_CVAR(vr_burn_flames_max, "12", CVAR_ARCHIVE)         // most flames on one body: more hits add one each up to this (the damage stays one fire's; 7 before config 89)
QVR_CVAR(vr_burn_spread, "0.5", CVAR_ARCHIVE)            // s between flames spreading
QVR_CVAR(vr_burn_flame_size, "0.8", CVAR_ARCHIVE)        // the first flame's scale (id's small flame, progs/flame2.mdl)
QVR_CVAR(vr_burn_flame_small, "0.7", CVAR_ARCHIVE)       // the other flames' size, times the first's (each a little more or less)
QVR_CVAR(vr_burn_flame_tilt, "12", CVAR_ARCHIVE)         // degrees the other flames lean, at most (upright: 0)
QVR_CVAR(vr_burn_corpses, "1", CVAR_ARCHIVE)             // 1: corpses catch fire too (Gibs and Corpses' corpses), and a monster dying burning burns on
QVR_CVAR(vr_burn_corpse_time, "8", CVAR_ARCHIVE)         // s a corpse burns
QVR_CVAR(vr_burn_corpse_damage, "0", CVAR_ARCHIVE)       // a burning corpse's damage, times a monster's (it gibs at its corpse health; 0: it just burns)
QVR_CVAR(vr_burn_touch, "1", CVAR_ARCHIVE)               // 1: lit torch touch ignites monsters/corpses; mapper flames touching your body use the same burn
QVR_CVAR(vr_burn_lava_nails, "1", CVAR_ARCHIVE)          // 1: your lava nails (the nailguns' lava ammo) set what they hit on fire
QVR_CVAR(vr_burn_crates, "1", CVAR_ARCHIVE)              // 1: wooden crates catch fire (a torch's blow or touch, a lava nail, a burning crate touching them)
QVR_CVAR(vr_burn_crate_time, "10", CVAR_ARCHIVE)         // s a crate burns before it breaks (vr_burn_crate_break) or goes out
QVR_CVAR(vr_burn_crate_break, "1", CVAR_ARCHIVE)         // 1: a crate breaks when it has burnt its time (0: its fire goes out)
QVR_CVAR(vr_burn_crate_spread, "3", CVAR_ARCHIVE)        // s a burning crate takes to set the crates touching it on fire (0: fire doesn't spread)
QVR_CVAR(vr_burn_crate_gap, "2", CVAR_ARCHIVE)           // units apart two crates still count as touching
QVR_CVAR(vr_burn_crate_flames, "6", CVAR_ARCHIVE)        // flames a crate's fire spreads to (Most Flames caps it)
QVR_CVAR(vr_burn_crate_flame_size, "1.25", CVAR_ARCHIVE) // a crate's flames' size, times a body's
QVR_CVAR(vr_burn_crate_char, "1", CVAR_ARCHIVE)          // 1: a crate burnt through breaks into charred pieces that don't burn (0: plain pieces, which do)
QVR_CVAR(vr_burn_pieces, "1", CVAR_ARCHIVE)              // 1: a broken crate's pieces catch fire as crates do (a torch, a lava nail, burning wood near them)
QVR_CVAR(vr_burn_piece_time, "5", CVAR_ARCHIVE)          // s a piece burns before it burns away (charred from half of it)
QVR_CVAR(vr_burn_piece_gap, "8", CVAR_ARCHIVE)           // units apart a burning piece and a piece or a crate catch from each other
QVR_CVAR(vr_burn_piece_flames, "3", CVAR_ARCHIVE)        // flames a piece's fire spreads to (Most Flames caps it)
QVR_CVAR(vr_burn_piece_flame_size, "0.7", CVAR_ARCHIVE)  // a piece's flames' size, times a body's
QVR_CVAR(vr_burn_nail_convert, "1", CVAR_ARCHIVE)        // 1: a nail passing through a lit torch's flame (held, thrown, lying, on its wall) becomes a lava nail
QVR_CVAR(vr_burn_nail_reach, "10", CVAR_ARCHIVE)         // units from a torch's flame a nail must pass to catch fire
QVR_CVAR(vr_burn_nail_sound, "0.5", CVAR_ARCHIVE)        // the volume of the short fizz a nail makes catching fire in a torch's flame (0: none)
QVR_CVAR(vr_burn_self, "1", CVAR_ARCHIVE)                // 1 (the default since config 89; 0 before): your held torch burns you after vr_burn_self_time; mapper flames use the same hand/arm warnings and delay
QVR_CVAR(vr_burn_self_time, "0.6", CVAR_ARCHIVE)         // s a held torch's flame must stay on you before you catch fire (haptic warnings till then)
QVR_CVAR(vr_burn_self_haptic, "1", CVAR_ARCHIVE)         // the warnings' strength (the hand buzzing as the flame stays on you; 0: none)
QVR_CVAR(vr_burn_drop, "1", CVAR_ARCHIVE)                // 1 (the default since config 89; 0 before): a torch held upside down whose flame stays on the hand holding it for vr_burn_drop_time is dropped (a burn on the hand)
QVR_CVAR(vr_burn_drop_time, "1", CVAR_ARCHIVE)           // s the flame of a torch held upside down must stay on its hand before it is dropped
QVR_CVAR(vr_burn_test, "0", CVAR_NONE)                   // the QC sets the nearest monster or corpse on fire: 1 as a torch's blow, 2 a lava nail, 3 a touch; 4 prints how it burns; 5 loads lava nails in the main hand's nailgun; 6 shoots a nail through the nearest lit torch's flame (Debug)
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
QVR_CVAR(vr_grenade_pouch_hold_pitch, "90", CVAR_ARCHIVE)
QVR_CVAR(vr_grenade_pouch_hold_yaw, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_grenade_pouch_hold_roll, "0", CVAR_ARCHIVE)
// The player's body (vr_avatar.cpp): 0 none, 2 torso and arms, 3 torso, arms and legs: the skinned
// progs/vrbody model, posed by IK from the head and hands (1, the old floating torso, is now 2).
QVR_CVAR(vr_body_mode, "3", CVAR_ARCHIVE)           // 3 shipped (vr_defaults.cfg too): the full body, legs and all
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
QVR_CVAR(vr_wounds_own_res, "0", CVAR_ARCHIVE)     // your own body's and hands' wound masks: texels on a side, smooth (512, 1024, 2048: 5, 20, 80 MB: per side, and the blood not yours); 0 in the pool, chunky as the monsters'
QVR_CVAR(vr_wounds_bump_burns, "3", CVAR_ARCHIVE)     // ... the relief of their burns and char (crusted, cracked), times the default (0 flat)
QVR_CVAR(vr_wounds_bump_blood, "1.5", CVAR_ARCHIVE)     // ... and of their bleeding wounds (sunk into the skin), times the default (0 flat)
QVR_CVAR(vr_wounds_blood_alpha, "0.8", CVAR_ARCHIVE)   // how opaque painted blood is over the skins (0..1)
QVR_CVAR(vr_wounds_debug, "0", CVAR_NONE)           // 1: print each wound painted
// Gore: bloody hands and washing (vr_wounds.cpp; ROUND21.md, "Gore: bloody hands, washing, dying bodies, blood mist").
QVR_CVAR(vr_gore_hands, "1", CVAR_ARCHIVE)            // grabbing a gib or a head bloodies the hand holding it: how much (0 none; the wound skins: 1 a bloody hand)
QVR_CVAR(vr_gore_wash, "1", CVAR_ARCHIVE)             // water washes the blood off your body and hands, where they are under it
QVR_CVAR(vr_gore_wash_time, "1.5", CVAR_ARCHIVE)      // seconds under water to wash the blood off fully
QVR_CVAR(vr_gore_reopen, "1", CVAR_ARCHIVE)           // hurt, your wounds re-open after a wash (healed ones don't; a gib's blood doesn't come back)
QVR_CVAR(vr_gore_reopen_delay, "4", CVAR_ARCHIVE)     // seconds out of the water, clean, before they re-open
QVR_CVAR(vr_gore_reopen_time, "1.5", CVAR_ARCHIVE)    // seconds over which they re-open, one after another (0: all at once)
// Gore: blood on you and your gear (vr_wounds.cpp; ROUND21.md, "Blood on you, your weapons and props").
QVR_CVAR(vr_gore_spatter, "1", CVAR_ARCHIVE)          // blood thrown from hits near you onto your hands, arms, body and what you hold: how much (0 none)
QVR_CVAR(vr_gore_spatter_melee, "1", CVAR_ARCHIVE)    // ... from your blows (fists, blades, props, guns as clubs): times
QVR_CVAR(vr_gore_spatter_saw, "2", CVAR_ARCHIVE)      // ... from a chainsaw's cuts: times
QVR_CVAR(vr_gore_spatter_shots, "1", CVAR_ARCHIVE)    // ... from shots hitting close to you: times
QVR_CVAR(vr_gore_spatter_range, "64", CVAR_ARCHIVE)   // ... a shot's hit this close to your hands or eyes at most (units)
QVR_CVAR(vr_gore_spatter_gibs, "1", CVAR_ARCHIVE)     // gibs flying into you bloody you where they strike: how much (0 none)
QVR_CVAR(vr_gore_gear, "1", CVAR_ARCHIVE)             // the weapons and props you hold take blood: kept dropped and taken again, until water washes it
QVR_CVAR(vr_gore_clean_skins, "1", CVAR_ARCHIVE)      // weapons whose skins have blood painted in (the axe, the swords, the chainsaw) start clean (vr_cleanskins.cpp)
QVR_CVAR(vr_gore_gear_holstered, "1", CVAR_ARCHIVE)   // ... your holstered weapons too, near enough to the blood (as your body and hands are)
QVR_CVAR(vr_gore_gear_nearby, "1", CVAR_ARCHIVE)      // weapons and props lying about take blood from hits, gibbings and bursts near them: how much (0 none)
QVR_CVAR(vr_gore_gear_drops, "1", CVAR_ARCHIVE)       // what a gibbed monster drops (its gun, its backpack) comes out soaked: how much (0 as anything lying near)
QVR_CVAR(vr_gore_spread, "1", CVAR_ARCHIVE)           // blood over your arms (a little your body and legs) when hurt, holding gibs, as wounds re-open: how much (0 none)
// Gore: blood mist (vr_particles.cpp): large, faint clouds wherever something bleeds (every blood particle effect).
QVR_CVAR(vr_gore_mist, "1", CVAR_ARCHIVE)             // how many clouds a bleed makes (0 none)
QVR_CVAR(vr_gore_mist_size, "1", CVAR_ARCHIVE)        // how big they are (1: about 1.5 m across as they fade)
QVR_CVAR(vr_gore_mist_alpha, "0.06", CVAR_ARCHIVE)    // how opaque they start (0..1)
QVR_CVAR(vr_gore_mist_life, "2.5", CVAR_ARCHIVE)      // seconds they take to fade
QVR_CVAR(vr_gore_mist_grow, "1", CVAR_ARCHIVE)        // how fast they spread as they fade
QVR_CVAR(vr_gore_mist_speed, "1", CVAR_ARCHIVE)       // how fast they drift along the blow and apart
QVR_CVAR(vr_gore_mist_rise, "-2", CVAR_ARCHIVE)       // units/s they rise (negative: sink)
QVR_CVAR(vr_gore_mist_dark, "0.4", CVAR_ARCHIVE)      // how dark their red is (0 bright, 1 nearly black)
QVR_CVAR(vr_gore_mist_near, "40", CVAR_ARCHIVE)       // none closer to the eyes than this many units (your own bleeding not in your face)
QVR_CVAR(vr_corpse_dying, "1", CVAR_ARCHIVE)          // a monster dying takes a corpse's damage during its death animation (QC combat.qc: gibbed by it, never killed again)
QVR_CVAR(vr_body_anchors, "1", CVAR_ARCHIVE)          // holsters, virtual stock and hand collisions follow the body's lean and crouch
QVR_CVAR(vr_body_debug, "0", CVAR_NONE)               // 1 draw the skeleton; 2, 3 also show the body in front of you, facing you or from its left
QVR_CVAR(vr_body_eye_forward, "0.08", CVAR_ARCHIVE)   // metres from the top of the neck to the eyes, forward and up
QVR_CVAR(vr_body_eye_up, "0.08", CVAR_ARCHIVE)
QVR_CVAR(vr_body_crouch_tilt, "25", CVAR_ARCHIVE)    // degrees the back tilts forward in a full crouch
QVR_CVAR(vr_body_torso_back, "0", CVAR_ARCHIVE)       // metres the torso sits further behind the neck (negative: in front)
// Which way the torso faces (vr_torso.cpp; ROUND21.md, "Torso direction"): guessed from the head and the hands.
QVR_CVAR(vr_torso_mode, "1", CVAR_ARCHIVE)            // 1 mostly the head, the hands in front pulling a little; 0 the old engine's (0.8 towards the hands)
QVR_CVAR(vr_torso_head, "1", CVAR_ARCHIVE)            // its weights: the head's yaw now,
QVR_CVAR(vr_torso_head_history, "1", CVAR_ARCHIVE)    // ... where the head faced lately (a glance turns the torso less),
QVR_CVAR(vr_torso_hands, "2.5", CVAR_ARCHIVE)         // ... both hands' midpoint, when both are in front,
QVR_CVAR(vr_torso_one_hand, "0.3", CVAR_ARCHIVE)      // ... one hand alone in front (the other behind the back or hanging down)
QVR_CVAR(vr_torso_hands_down, "2", CVAR_ARCHIVE)     // ... and both hands hanging by the sides (square to the line between them)
QVR_CVAR(vr_torso_head_lag, "0.3", CVAR_ARCHIVE)      // seconds "lately" spans (the eased head's time constant)
QVR_CVAR(vr_torso_side_angle, "45", CVAR_ARCHIVE)     // a hand further off to the torso's side than this (degrees from the chest) counts less, none 35 degrees on
QVR_CVAR(vr_torso_deadzone, "8", CVAR_ARCHIVE)        // degrees the estimate must be off before the torso turns (then it turns all the way)
QVR_CVAR(vr_torso_speed, "12", CVAR_ARCHIVE)          // how quickly it turns then (1/s; 0: at once)
QVR_CVAR(vr_torso_neck_max, "70", CVAR_ARCHIVE)       // degrees the head turns from the torso at most: past it the torso turns with the head at once
QVR_CVAR(vr_body_legs_back, "0", CVAR_ARCHIVE)        // metres the feet stand behind the head (negative: in front)
QVR_CVAR(vr_body_pauldrons, "1", CVAR_ARCHIVE)        // pauldrons on the body's shoulders, after the Quake ranger's (make_pauldron.py)
QVR_CVAR(vr_body_pauldron_style, "0", CVAR_ARCHIVE)   // 0 the ranger's brown leather, 1 the colour of the armour worn (leather without), 2 steel
QVR_CVAR(vr_body_pauldron_size, "1", CVAR_ARCHIVE)    // their size (scaled with the build too)
QVR_CVAR(vr_body_pauldron_follow, "0.35", CVAR_ARCHIVE) // how much the cap turns with the upper arm (0: with the shoulder; the lames follow the arm)
QVR_CVAR(vr_body_pauldron_forward, "0", CVAR_ARCHIVE) // metres they sit further forward,
QVR_CVAR(vr_body_pauldron_up, "0", CVAR_ARCHIVE)      // up,
QVR_CVAR(vr_body_pauldron_out, "0", CVAR_ARCHIVE)     // and out
// The flashlight (vr_flashlight.cpp): stored hanging on the belt, on the left or right hip (vr_flashlight_side) (round 21; on the chest before); only deliberate presses (an open, still hand) take or switch it; trigger at it switches it, an empty hand's grip takes it; held near the other hand's gun, B/Y clips it on; held at the head, B/Y clips it on there.
QVR_CVAR(vr_flashlight, "1", CVAR_ARCHIVE)            // 0: no flashlight
QVR_CVAR(vr_flashlight_brightness, "1", CVAR_ARCHIVE)
QVR_CVAR(vr_flashlight_range, "1000", CVAR_ARCHIVE)   // units the beam reaches
QVR_CVAR(vr_flashlight_shadows, "1", CVAR_ARCHIVE)    // its spot light casts shadows (takes one of vr_shadow_dlights: one tile, not six faces)
QVR_CVAR(vr_flashlight_cord, "1", CVAR_ARCHIVE)      // the retracting cord from the belt clip to the torch while it is off the belt: 0 none, 1 a rusty low-poly iron chain (chunky faceted links, Quake's look; vr_coil.cpp)
QVR_CVAR(vr_flashlight_beam, "0.35", CVAR_ARCHIVE)    // how strong its soft cone of light in the air is, 0..1 (0: none)
QVR_CVAR(vr_flashlight_beam_quality, "1", CVAR_ARCHIVE) // how closely the visible beam finds the walls that cut it: 0 low, 1 medium, 2 high (traces a frame, vr_flashlight.cpp)
QVR_CVAR(vr_flashlight_tilt, "8", CVAR_ARCHIVE)       // degrees the stored lamp (hanging on the chest, lens down) leans its lens out from the body
QVR_CVAR(vr_flashlight_forward, "0", CVAR_ARCHIVE)    // metres the belt clip sits further forward,
QVR_CVAR(vr_flashlight_up, "0", CVAR_ARCHIVE)         // up,
QVR_CVAR(vr_flashlight_out, "0", CVAR_ARCHIVE)        // and out (towards its side)
QVR_CVAR(vr_flashlight_side, "0", CVAR_ARCHIVE)       // the hip it hangs on: 0 the left, 1 the right
QVR_CVAR(vr_flashlight_grab_range, "1", CVAR_ARCHIVE) // times how far from the torch a hand reaches it (lit, taken; 9 cm from its axis at 1), 0.25..4
QVR_CVAR(vr_flashlight_head_range, "1", CVAR_ARCHIVE) // times the head's zone's radius (vr_flashlight_head_zone_radius: clipping it on, taking it off), 0.25..4
QVR_CVAR(vr_flashlight_mount_preview, "1", CVAR_ARCHIVE) // Weapon Offsets > Flashlight: the torch's place on the page's weapon drawn while the page is shown
QVR_CVAR(vr_flashlight_gun_range, "1", CVAR_ARCHIVE)  // times the gun's zone's radius (vr_flashlight_gun_zone_radius: clipping it on with B/Y or on letting go), 0.25..4
QVR_CVAR(vr_flashlight_auto_head, "0", CVAR_ARCHIVE)  // 1: the held torch let go in the head's zone clips on the head (no B/Y)
QVR_CVAR(vr_flashlight_auto_gun, "0", CVAR_ARCHIVE)   // 1: the held torch let go in the other hand's gun's zone clips on the gun (no B/Y)
QVR_CVAR(vr_flashlight_clip_time, "0.15", CVAR_ARCHIVE) // seconds the drawn torch eases from where it was onto a gun or the head as it clips on (on at once; 0: jumps), 0..1
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
// the other hand's gun (B/Y, or let go): a ball round the middle of the torch as it would sit on that gun (its mount:
// vr_flashlight_gun_* and the weapon's own vr_wofs_torch_*; round 21, 2026-10-02: a capsule along the whole gun before),
// moved along the gun, up and out (away from the body) by these metres, _radius across (times vr_flashlight_gun_range).
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
// Weapon effects (vr_weaponfx.cpp; ROUND21.md, "Weapon effects: recoil, muzzle flash, tracers"): each weapon's own
// settings are Weapon Offsets > Effects (vr_wofs_fx_*: which weapons kick and flash, their tracers' multipliers).
QVR_CVAR(vr_weapon_recoil, "1", CVAR_ARCHIVE)          // 0: no programmatic recoil (the models' own recoil animations stay)
QVR_CVAR(vr_recoil_kick, "1.5", CVAR_ARCHIVE)          // cm the drawn weapon is kicked back at a weapon's Recoil Strength 1 (looks only)
QVR_CVAR(vr_recoil_rise, "4", CVAR_ARCHIVE)            // degrees its muzzle tips up at Strength 1 (looks only: the aim stays)
QVR_CVAR(vr_muzzle_flash, "1", CVAR_ARCHIVE)           // 0: no programmatic muzzle flashes (the models' own stay)
QVR_CVAR(vr_muzzle_flash_enemies, "1", CVAR_ARCHIVE)   // the grunts' and enforcers' guns flash at their muzzles as they fire
QVR_CVAR(vr_muzzle_smoke_enemies, "1", CVAR_ARCHIVE)   // puffs of gun smoke out of an enforcer's rifle's and a grunt's gun's muzzle as they fire (as your guns'; 0 none; NOTES.md vrfiringrange_2026-10-02_01-21-23, grunts _15-52-01)
QVR_CVAR(vr_muzzle_flash_enemy_size, "1", CVAR_ARCHIVE) // their flashes' size (times a third of their gun's length)
QVR_CVAR(vr_tracers, "1", CVAR_ARCHIVE)                // bullet tracers for hitscan shots (the shotguns, the burst rifle; a weapon's Tracers may differ)
QVR_CVAR(vr_tracers_enemies, "1", CVAR_ARCHIVE)        // the monsters' hitscan shots (the grunts') too
QVR_CVAR(vr_tracer_speed, "150", CVAR_ARCHIVE)         // m/s they fly (times a weapon's Tracer Speed)
QVR_CVAR(vr_tracer_length, "1.5", CVAR_ARCHIVE)        // m long
QVR_CVAR(vr_tracer_width, "1.2", CVAR_ARCHIVE)         // cm thick
QVR_CVAR(vr_tracer_chance, "0.3", CVAR_ARCHIVE)        // the chance a pellet (or a round) shows one, 0..1 (times a weapon's)
QVR_CVAR(vr_tracer_r, "1", CVAR_ARCHIVE)               // their colour (a weapon may have its own), 0..1
QVR_CVAR(vr_tracer_g, "0.8", CVAR_ARCHIVE)
QVR_CVAR(vr_tracer_b, "0.45", CVAR_ARCHIVE)
QVR_CVAR(vr_tracer_brightness, "1", CVAR_ARCHIVE)      // times the colour (they add light onto the scene)
QVR_CVAR(vr_debug_weaponfx, "0", CVAR_NONE)            // 1: each shot's effects ("weaponfx fired/tracer"); 2: and the recoil each frame ("weaponfx recoil"); 3: as 1, and the eyes' view of the next grunt's flash (vr_eyeshot, once)
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
// VR Calibration (vr_setup.hpp), tests: a motion take of raw tracking played (vr_motion_play ... watch noplace) from the
// body step's first frame, the synthetic person doing the poses (TESTING.md, "Body calibration"). Not saved.
QVR_CVAR(vr_setup_test_take, "", CVAR_NONE)
// Tests: the next confirmation dialog (SCR_ModalMessage: New Game, VR Calibration) answered by itself half a second after
// it shows: 1 yes (A), 0 no (B); -1 not (the player answers). Set back to -1 once used.
QVR_CVAR(vr_test_modal_answer, "-1", CVAR_NONE)
QVR_CVAR(vr_body_shoulder_reach, "0.08", CVAR_ARCHIVE) // metres the shoulders may reach out towards a hand past the arm
QVR_CVAR(vr_body_forearm_twist, "0.5", CVAR_ARCHIVE)  // share of the wrist's roll the middle of the forearm follows (none at the elbow, all at the wrist)
QVR_CVAR(vr_body_wrist_limits, "1", CVAR_ARCHIVE)   // the elbow swings round to keep the wrist within a real one's bend and roll, times this (0: never)
QVR_CVAR(vr_body_elbow_lift, "4", CVAR_ARCHIVE)    // that swing keeps the elbow out of the torso, from crossing the chest and from rising over the shoulder across it, times this (0: free)
QVR_CVAR(vr_body_elbow_spread, "1", CVAR_ARCHIVE)  // a folded arm (hand at the face or chest) with the palm down turns its elbow out to the side, this much (0: as before, 1: fully)
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
// Rising particles over low-poly fire models (static fires, burning bodies, taken torches).
QVR_CVAR(vr_fire_particles, "1", CVAR_ARCHIVE)
QVR_CVAR(vr_fire_particles_count, "1", CVAR_ARCHIVE) // exact particles in each emission burst
QVR_CVAR(vr_fire_particles_frequency, "10", CVAR_ARCHIVE) // bursts per second per flame model
QVR_CVAR(vr_fire_particles_origin, "0.25", CVAR_ARCHIVE) // emission height along flame: 0 base, 1 tip
QVR_CVAR(vr_fire_particles_speed_min, "0.1", CVAR_ARCHIVE) // horizontal outward speed, m/s
QVR_CVAR(vr_fire_particles_speed_max, "0.4", CVAR_ARCHIVE)
QVR_CVAR(vr_fire_particles_up_min, "0.3", CVAR_ARCHIVE) // upward speed in world axes, regardless of torch tilt
QVR_CVAR(vr_fire_particles_up_max, "0.9", CVAR_ARCHIVE)
QVR_CVAR(vr_fire_particles_life_min, "0.25", CVAR_ARCHIVE)
QVR_CVAR(vr_fire_particles_life_max, "0.65", CVAR_ARCHIVE)
QVR_CVAR(vr_fire_particles_size, "1", CVAR_ARCHIVE)
QVR_CVAR(vr_fire_particles_alpha, "0.55", CVAR_ARCHIVE)
QVR_CVAR(vr_fire_particles_range, "1200", CVAR_ARCHIVE)

// Physical explosion chunks (client model objects; harmless, bounded, swept world/brush collisions).
QVR_CVAR(vr_explosion_particles, "1", CVAR_ARCHIVE) // large fireball sprites per blast, exact count (0..16)
QVR_CVAR(vr_explosion_debris, "1", CVAR_ARCHIVE)
QVR_CVAR(vr_explosion_debris_count, "12", CVAR_ARCHIVE)
QVR_CVAR(vr_explosion_debris_speed_min, "3", CVAR_ARCHIVE) // m/s
QVR_CVAR(vr_explosion_debris_speed_max, "9", CVAR_ARCHIVE)
QVR_CVAR(vr_explosion_debris_up, "0.65", CVAR_ARCHIVE) // upward bias of the launch direction, 0..2
QVR_CVAR(vr_explosion_debris_size_min, "1.4", CVAR_ARCHIVE) // diameter in Quake units at world scale 1
QVR_CVAR(vr_explosion_debris_size_max, "3.2", CVAR_ARCHIVE)
QVR_CVAR(vr_explosion_debris_life_min, "2", CVAR_ARCHIVE)
QVR_CVAR(vr_explosion_debris_life_max, "4", CVAR_ARCHIVE)
QVR_CVAR(vr_explosion_debris_bounce, "0.45", CVAR_ARCHIVE) // normal speed retained on bounce, 0..1
QVR_CVAR(vr_explosion_debris_max, "96", CVAR_ARCHIVE) // oldest retired first, hard cap 256
QVR_CVAR(vr_explosion_debris_trail, "1", CVAR_ARCHIVE) // trail density, 0 off
QVR_CVAR(vr_explosion_debris_lights, "8", CVAR_ARCHIVE) // nearest glowing chunks, cap 16 of the shared dlights
QVR_CVAR(vr_explosion_debris_light_radius, "48", CVAR_ARCHIVE) // units at world scale 1

QVR_CVAR(vr_particle_mult, "1", CVAR_ARCHIVE)         // how many of them
QVR_CVAR(vr_particle_seed, "0", CVAR_NONE)            // tests: the particles' random numbers restarted from this seed at each map (0: random)
QVR_CVAR(vr_particle_light, "1", CVAR_ARCHIVE)        // lit particles: blood, smoke, dust, chips and water take the place's light (the lightmap and dynamic lights: dark in the dark, lit by your flashlight); fire, sparks, glows and explosions stay bright (0: all as bright as their colours)
QVR_CVAR(vr_soft_particles, "1", CVAR_ARCHIVE)        // soft particles: smoke, fire, blood and explosion sprites fade out where they meet walls and floors instead of cutting hard lines (0 off)
QVR_CVAR(vr_soft_particles_scale, "1", CVAR_ARCHIVE)  // how close to the scene they fade, times each kind's own distance (sparks 1.5 units, smoke and fire half their size, at most 32)
QVR_CVAR(vr_particle_halfres, "1", CVAR_ARCHIVE)      // with many large particles in view (smoke, fire and blood near you, over a screen and a half of them), all of them drawn at half resolution and blended in: a quarter of their cost on the GPU, a little softer (0: always at full resolution)
QVR_CVAR(vr_particle_halfres_pixels, "0", CVAR_NONE)  // tests: only those this many pixels or more from middle to edge at half resolution, the others over them at full (0: all of them, in their order)
QVR_CVAR(vr_particle_freeze, "0", CVAR_NONE) // debug: freeze particle simulation for same-state render comparisons
QVR_CVAR(vr_particle_trim, "0", CVAR_ARCHIVE) // opt-in conservative transparent-margin geometry; 0: full quads
QVR_CVAR(vr_particle_retro_fast, "1", CVAR_ARCHIVE) // full resolution: derive the retro grid per vertex; 0: original fragment path for comparison (retro half always uses fast shaders)
QVR_CVAR(vr_particle_retro_halfres, "0", CVAR_ARCHIVE) // opt-in half resolution for heavy retro particles; preserves palette/grid, softer edges
QVR_CVAR(vr_particle_retro_halfres_pixels, "0", CVAR_ARCHIVE) // minimum projected half-size for the retro half-resolution pass; smaller particles stay crisp but draw over the larger ones (0: all, in their order)
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
// Retro lighting (vr_retrolight.cpp, vr_retrolight.h; Graphics > Retro Lighting; ROUND21.md "Retro lighting"): Quake's coarse light, fixed to the surfaces (no swimming in VR)
QVR_CVAR(vr_retrolight, "0", CVAR_ARCHIVE)                  // the switch for all of it
QVR_CVAR(vr_retrolight_ab, "0", CVAR_NONE)                  // Debug > Views: hides it at once, to compare
QVR_CVAR(vr_retrolight_spacing, "1", CVAR_ARCHIVE)          // the light levels' spacing: 0 even in light (Quake's colormap: the shade crushed to black at few levels), 1 even in brightness
QVR_CVAR(vr_retrolight_edge_soft, "1", CVAR_ARCHIVE)        // the blocks' edges (lightmap, dynamic lights, shadows), pixels wide (0: nearest, crawls)
QVR_CVAR(vr_retrolight_world, "1", CVAR_ARCHIVE)            // the world and brush models
QVR_CVAR(vr_retrolight_world_steps, "16", CVAR_ARCHIVE)     // the baked light's levels a unit of the lightmap (0 smooth; 64: Quake's software renderer's)
QVR_CVAR(vr_retrolight_world_soft, "0", CVAR_ARCHIVE)       // the levels' edges blurred over this share of a level (0 hard, anti-aliased; 1 smooth steps)
QVR_CVAR(vr_retrolight_world_dither, "0", CVAR_ARCHIVE)     // an ordered dither between the levels, fixed to the texels (0 none, 1 full)
QVR_CVAR(vr_retrolight_world_dither_size, "1", CVAR_ARCHIVE) // its cells, in Quake texels (1, 2, 4)
QVR_CVAR(vr_retrolight_world_lightmap, "1", CVAR_ARCHIVE)   // the lightmap read on its blocky grid
QVR_CVAR(vr_retrolight_world_luxel, "16", CVAR_ARCHIVE)     // ... its blocks, in Quake texels (16: a luxel, GL nearest's blocks; 1: each texel lit, as the software renderer)
QVR_CVAR(vr_retrolight_world_dyn_steps, "8", CVAR_ARCHIVE)  // dynamic lights' levels a unit (0 smooth)
QVR_CVAR(vr_retrolight_world_dyn_block, "4", CVAR_ARCHIVE)  // dynamic lights found on a grid of blocks this many texels (0: per pixel; 1, 2, 4, 8, 16)
QVR_CVAR(vr_retrolight_models, "1", CVAR_ARCHIVE)           // models (monsters, items, weapons, your body)
QVR_CVAR(vr_retrolight_model_steps, "16", CVAR_ARCHIVE)     // their own light's levels (as the world's: a unit, the lightmap's)
QVR_CVAR(vr_retrolight_model_soft, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_retrolight_model_dither, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_retrolight_model_dither_size, "1", CVAR_ARCHIVE) // skin texels
QVR_CVAR(vr_retrolight_model_dyn_steps, "8", CVAR_ARCHIVE)
QVR_CVAR(vr_retrolight_model_dyn_block, "0", CVAR_ARCHIVE)  // skin texels (0: per pixel)
QVR_CVAR(vr_retrolight_shadow_filter, "1", CVAR_ARCHIVE)    // shadows' edges: 0 as Graphics > Shadows, 1 hard (one tap), 2 blocky (the shadow map's texels)
QVR_CVAR(vr_retrolight_shadow_steps, "0", CVAR_ARCHIVE)     // shadows' darkness in levels (0 smooth, 1 lit or not, 2..)
QVR_CVAR(vr_retrolight_shadow_block, "0", CVAR_ARCHIVE)     // shadows looked up on a grid of blocks this many texels (0: per pixel; 0.25, 0.5, 1, 2, 4, 8, 16)
QVR_CVAR(vr_retrolight_shadow_soft, "0", CVAR_ARCHIVE)      // their levels' edges blurred over this share of a level
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
QVR_CVAR(vr_ambient_light, "0", CVAR_ARCHIVE)          // the room's own fill light: the baked light's floor, as a share of Quake's full light (0 none; the same curve as vr_light_contrast, which still shapes the shade: qvr::lighting::lightCurve)
QVR_CVAR(vr_ambient_light_ab, "0", CVAR_NONE)          // Debug A/B: 1 hides the ambient light (as vr_ambient_light 0), to compare
QVR_CVAR(vr_decals, "1", CVAR_ARCHIVE)                 // blood and scorch marks on the world
QVR_CVAR(vr_decal_max, "1024", CVAR_ARCHIVE)           // how many are kept (the oldest go first)
QVR_CVAR(vr_decal_life, "120", CVAR_ARCHIVE)           // seconds a mark stays (fading over the last five)
QVR_CVAR(vr_decals_world, "1", CVAR_ARCHIVE)         // the marks drawn in the world's own shader, on its texture under its light and following its parallax relief (0: as meshes over it)
QVR_CVAR(vr_gib_blood, "1", CVAR_ARCHIVE)              // gibs and heads bleed: Quake VR's blood trail, drops on the floor, splats where they hit (0: Quake's trail)
QVR_CVAR(vr_gib_blood_trail, "1", CVAR_ARCHIVE)        // how dense their trail of blood and drops is (0 none)
QVR_CVAR(vr_gore, "2", CVAR_ARCHIVE)                   // gore (vr_gore.cpp): 0 Quake VR's blood only, 1 more, 2 over the top (sprays on walls, pools under corpses, gibs stuck to ceilings dripping)
// Gib speed by what gibbed it (player.qc VelocityForDamage, combat.qc VR_GibSpeedScale): every gibbing, monsters' and corpses'.
QVR_CVAR(vr_gib_speed_melee, "0.15", CVAR_ARCHIVE)   // a melee blow (a swing, a bash, a shove, a headbutt, a stomp)
QVR_CVAR(vr_gib_speed_light, "0.25", CVAR_ARCHIVE)   // a thrown thing, the shotguns, nails, a stuck axe's bleeding
QVR_CVAR(vr_gib_speed_heavy, "1", CVAR_ARCHIVE)      // explosions, rockets, grenades, lightning, everything else (Quake's)
QVR_CVAR(vr_gore_spray, "1", CVAR_ARCHIVE)             // how many splats a hit or a gibbing sprays onto the walls, floor and ceiling round it (0 none)
QVR_CVAR(vr_gore_size, "1", CVAR_ARCHIVE)              // how big the gore's splats, pools and runs are
QVR_CVAR(vr_gore_pools, "1", CVAR_ARCHIVE)             // pools of blood spreading under corpses and gibs: their size (0 none)
QVR_CVAR(vr_gore_drips, "1", CVAR_ARCHIVE)             // blood dripping from splats on the ceiling and from hanging gibs: how long and how much (0 none)
QVR_CVAR(vr_gore_stick, "8", CVAR_ARCHIVE)             // gibs flung into a ceiling or a wall may stick there about this many seconds, then fall (0 never)
// Scourge of Armagon's flies round a severed head (player.qc HeadThink: one in ten buzz, looping). Off: the author found
// them pointless as they are (NOTES.md vrfiringrange_2026-10-02_01-29-37); BACKLOG.md, "Flies on corpses and gibs".
QVR_CVAR(vr_head_flies, "0", CVAR_ARCHIVE)
// Small gibs (QC vr_smallgibs.qc; ROUND21.md, "Small gibs"): chunks of meat torn out by hits, the gibs' models scaled
// down to a rock's size or half of it; carried, thrown, bleeding as the gibs do, gone after a while (not while held).
QVR_CVAR(vr_smallgibs, "1", CVAR_ARCHIVE)                  // hits on monsters tear small gibs out of them
QVR_CVAR(vr_smallgibs_player, "1", CVAR_ARCHIVE)           // and hits on you
QVR_CVAR(vr_smallgibs_min_damage, "10", CVAR_ARCHIVE)      // a hit's damage (a shotgun blast's pellets summed) under which none come
QVR_CVAR(vr_smallgibs_full_damage, "35", CVAR_ARCHIVE)     // the damage from which one always comes (the chance rises from 0 at the least)
QVR_CVAR(vr_smallgibs_curve, "1.7", CVAR_ARCHIVE)          // the chance's rise from the least damage to the sure one: to this power (1 straight; more: rarer for light hits)
QVR_CVAR(vr_smallgibs_damage_per_gib, "30", CVAR_ARCHIVE)  // one gib more for each this much damage (with a random part)
QVR_CVAR(vr_smallgibs_per_hit, "6", CVAR_ARCHIVE)          // most small gibs from one hit
QVR_CVAR(vr_smallgibs_shots, "1", CVAR_ARCHIVE)            // the chance times this: the shotguns, a grunt's gun
QVR_CVAR(vr_smallgibs_nails, "0.4", CVAR_ARCHIVE)            // nails, lasers
QVR_CVAR(vr_smallgibs_blades, "1.7", CVAR_ARCHIVE)         // a blade's edge or point: the axe, a sword, swung or thrown
QVR_CVAR(vr_smallgibs_blunt, "0.3", CVAR_ARCHIVE)          // punches, a pommel or hilt, a gun or the crowbar swung, Mjolnir, bashes, headbutts
QVR_CVAR(vr_smallgibs_props, "1.2", CVAR_ARCHIVE)            // a rock, brick or other thing thrown or swung in the fist
QVR_CVAR(vr_smallgibs_explosions, "1", CVAR_ARCHIVE)       // rockets, grenades, every blast
QVR_CVAR(vr_smallgibs_other, "0.5", CVAR_ARCHIVE)          // lightning, monsters' blows, the rest
QVR_CVAR(vr_smallgibs_saw_interval, "0.1", CVAR_ARCHIVE)   // s: the chainsaw tears one out of what it cuts at most this often (always: monster or corpse)
QVR_CVAR(vr_smallgibs_gibbing, "12", CVAR_ARCHIVE)          // small gibs with a monster's (or corpse's) gibs when it is gibbed
QVR_CVAR(vr_smallgibs_burst, "4", CVAR_ARCHIVE)            // small gibs a large gib bursts into (a head half as many again)
QVR_CVAR(vr_smallgibs_speed, "3", CVAR_ARCHIVE)            // m/s they fly out from the hit (randomized by a third)
QVR_CVAR(vr_smallgibs_up, "7", CVAR_ARCHIVE)               // m/s up (randomized by a third; Quake falls three times as fast as the world)
// Speed and Up times these, by what tore them out (QC VR_SmallGib_Situation):
QVR_CVAR(vr_smallgibs_speed_melee, "0.35", CVAR_ARCHIVE)    // Speed times this: a hand's blow: a blade, a punch, a pommel, a thing in the fist; a bash, a headbutt
QVR_CVAR(vr_smallgibs_up_melee, "0.5", CVAR_ARCHIVE)        // Up times this
QVR_CVAR(vr_smallgibs_speed_saw, "0.5", CVAR_ARCHIVE)       // Speed times this: the chainsaw's chain or swing
QVR_CVAR(vr_smallgibs_up_saw, "0.6", CVAR_ARCHIVE)          // Up times this
QVR_CVAR(vr_smallgibs_speed_guns, "0.85", CVAR_ARCHIVE)        // Speed times this: shotguns, nails, lasers, lightning, a grunt's gun
QVR_CVAR(vr_smallgibs_up_guns, "0.9", CVAR_ARCHIVE)           // Up times this
QVR_CVAR(vr_smallgibs_speed_explosions, "1", CVAR_ARCHIVE)  // Speed times this: rockets, grenades, every blast
QVR_CVAR(vr_smallgibs_up_explosions, "1", CVAR_ARCHIVE)     // Up times this
QVR_CVAR(vr_smallgibs_speed_thrown, "1", CVAR_ARCHIVE)      // Speed times this: a thrown weapon, rock, brick or gib; a projectile batted back
QVR_CVAR(vr_smallgibs_up_thrown, "1", CVAR_ARCHIVE)         // Up times this
QVR_CVAR(vr_smallgibs_speed_other, "1", CVAR_ARCHIVE)       // Speed times this: monsters' blows, the rest
QVR_CVAR(vr_smallgibs_up_other, "1", CVAR_ARCHIVE)          // Up times this
QVR_CVAR(vr_smallgibs_speed_gibbing, "1", CVAR_ARCHIVE)     // Speed times this: a living monster gibbed (with its gibs' flight at 0.6)
QVR_CVAR(vr_smallgibs_up_gibbing, "0.7", CVAR_ARCHIVE)      // Up times this
QVR_CVAR(vr_smallgibs_speed_corpse, "1", CVAR_ARCHIVE)      // Speed times this: a corpse gibbed (with its gibs' flight at 0.6)
QVR_CVAR(vr_smallgibs_up_corpse, "0.7", CVAR_ARCHIVE)       // Up times this
QVR_CVAR(vr_smallgibs_speed_burst, "1", CVAR_ARCHIVE)       // Speed times this: a large gib or head burst
QVR_CVAR(vr_smallgibs_up_burst, "1", CVAR_ARCHIVE)          // Up times this
QVR_CVAR(vr_smallgibs_pass_inside, "1", CVAR_ARCHIVE)      // made inside a monster, a player or a hand's or weapon's body (the melee and chainsaw ones), they pass through it until clear (0: Box3D pushes them out, at 2 to 3 m/s)
QVR_CVAR(vr_smallgibs_size_min, "0.65", CVAR_ARCHIVE)       // their size: from this times a rock's
QVR_CVAR(vr_smallgibs_size_max, "1.15", CVAR_ARCHIVE)         // to this
QVR_CVAR(vr_smallgibs_mass, "0.3", CVAR_ARCHIVE)           // kg each (light: thrown, they hurt little)
QVR_CVAR(vr_smallgibs_grace, "0.1", CVAR_ARCHIVE)         // s they pass through the body they came from
QVR_CVAR(vr_smallgibs_blow_grace, "0.3", CVAR_ARCHIVE)   // s after it is made a small gib isn't struck, nudged or pushed by a hand, a weapon or a held thing (the blow that tore it out batted it away)
QVR_CVAR(vr_smallgibs_blow_burst, "1", CVAR_ARCHIVE)    // in that time a blow (the blade that tore it out too) still bursts it, with vr_smallgibs_destroy; never batted or pushed (0: blows pass it)
QVR_CVAR(vr_smallgibs_max, "64", CVAR_ARCHIVE)             // most lying about at once: the oldest go first (not one in a hand)
QVR_CVAR(vr_smallgibs_time, "20", CVAR_ARCHIVE)            // s one lies about before it fades away (held, and until it lands again, it waits)
QVR_CVAR(vr_smallgibs_destroy, "1", CVAR_ARCHIVE)          // they can be shot or struck to a mist (off: shots and blows pass them)
QVR_CVAR(vr_smallgibs_brains, "1", CVAR_ARCHIVE)           // a head burst (a head gib destroyed, a head pop) throws brain chunks too (small gibs of brain matter: make_brains.py)
QVR_CVAR(vr_smallgibs_brains_burst, "6", CVAR_ARCHIVE)     // brain chunks from a head gib destroyed
QVR_CVAR(vr_smallgibs_brains_pop, "8", CVAR_ARCHIVE)       // brain chunks from a head pop (a headshot that bursts the head)
QVR_CVAR(vr_smallgibs_brains_size, "0.8", CVAR_ARCHIVE)    // their size: times the small gibs' (vr_smallgibs_size_min/max)
QVR_CVAR(vr_smallgibs_brains_mass, "0.15", CVAR_ARCHIVE)   // kg each (lighter than a small gib of meat)
QVR_CVAR(vr_smallgibs_brains_speed, "3", CVAR_ARCHIVE)     // m/s brain chunks fly out of a head burst (their own Speed: vr_smallgibs_speed is the meat gibs'; times vr_smallgibs_speed_burst)
QVR_CVAR(vr_smallgibs_brains_up, "7", CVAR_ARCHIVE)        // m/s up (their own Up; times vr_smallgibs_up_burst)
// Per enemy (VR_SmallGib_Mult, vr_smallgibs.qc; the same monsters as vr_corpse_health_<monster>): a monster's small
// gibs and brain chunks, times these. 1 as now, 0 none, 2 twice as many (a hit's, a gibbing's, and a head's bursts).
QVR_CVAR(vr_smallgibs_mult_grunt, "1", CVAR_ARCHIVE)       // a grunt's
QVR_CVAR(vr_smallgibs_mult_enforcer, "1", CVAR_ARCHIVE)    // an enforcer's
QVR_CVAR(vr_smallgibs_mult_dog, "1", CVAR_ARCHIVE)         // a rottweiler's
QVR_CVAR(vr_smallgibs_mult_fiend, "1", CVAR_ARCHIVE)       // a fiend's
QVR_CVAR(vr_smallgibs_mult_ogre, "1", CVAR_ARCHIVE)        // an ogre's
QVR_CVAR(vr_smallgibs_mult_knight, "1", CVAR_ARCHIVE)      // a knight's
QVR_CVAR(vr_smallgibs_mult_hellknight, "1", CVAR_ARCHIVE)  // a hell knight's
QVR_CVAR(vr_smallgibs_mult_vore, "1", CVAR_ARCHIVE)        // a vore's
QVR_CVAR(vr_smallgibs_mult_shambler, "1", CVAR_ARCHIVE)    // a shambler's
QVR_CVAR(vr_smallgibs_mult_scrag, "1", CVAR_ARCHIVE)       // a scrag's
QVR_CVAR(vr_smallgibs_mult_fish, "1", CVAR_ARCHIVE)        // a rotfish's
QVR_CVAR(vr_smallgibs_mult_gremlin, "1", CVAR_ARCHIVE)     // a gremlin's (Hipnotic)
QVR_CVAR(vr_smallgibs_mult_scourge, "1", CVAR_ARCHIVE)     // a centroid's (Rogue)
QVR_CVAR(vr_smallgibs_mult_eel, "1", CVAR_ARCHIVE)         // an eel's (Rogue)
QVR_CVAR(vr_smallgibs_mult_zombie, "1", CVAR_ARCHIVE)      // a zombie's (beheaded: vr_decap)
QVR_CVAR(vr_smallgibs_mult_mummy, "1", CVAR_ARCHIVE)       // a mummy's (Rogue, beheaded)
QVR_CVAR(vr_smallgibs_brains_mult_grunt, "1", CVAR_ARCHIVE)     // a grunt's brain chunks
QVR_CVAR(vr_smallgibs_brains_mult_enforcer, "1", CVAR_ARCHIVE)  // an enforcer's
QVR_CVAR(vr_smallgibs_brains_mult_dog, "1", CVAR_ARCHIVE)      // a rottweiler's
QVR_CVAR(vr_smallgibs_brains_mult_fiend, "1", CVAR_ARCHIVE)    // a fiend's
QVR_CVAR(vr_smallgibs_brains_mult_ogre, "1", CVAR_ARCHIVE)     // an ogre's
QVR_CVAR(vr_smallgibs_brains_mult_knight, "1", CVAR_ARCHIVE)   // a knight's
QVR_CVAR(vr_smallgibs_brains_mult_hellknight, "1", CVAR_ARCHIVE) // a hell knight's
QVR_CVAR(vr_smallgibs_brains_mult_vore, "1", CVAR_ARCHIVE)     // a vore's
QVR_CVAR(vr_smallgibs_brains_mult_shambler, "1", CVAR_ARCHIVE) // a shambler's
QVR_CVAR(vr_smallgibs_brains_mult_scrag, "1", CVAR_ARCHIVE)    // a scrag's
QVR_CVAR(vr_smallgibs_brains_mult_fish, "1", CVAR_ARCHIVE)     // a rotfish's
QVR_CVAR(vr_smallgibs_brains_mult_gremlin, "1", CVAR_ARCHIVE)  // a gremlin's (Hipnotic)
QVR_CVAR(vr_smallgibs_brains_mult_scourge, "1", CVAR_ARCHIVE)  // a centroid's (Rogue)
QVR_CVAR(vr_smallgibs_brains_mult_eel, "1", CVAR_ARCHIVE)      // an eel's (Rogue)
QVR_CVAR(vr_smallgibs_brains_mult_zombie, "1", CVAR_ARCHIVE)   // a zombie's (beheaded: vr_decap)
QVR_CVAR(vr_smallgibs_brains_mult_mummy, "1", CVAR_ARCHIVE)    // a mummy's (Rogue, beheaded)
QVR_CVAR(vr_gore_stick_thrown, "0.5", CVAR_ARCHIVE)        // a gib or head thrown into a wall or a ceiling (slower than vr_gib_splat_speed) sticks there: the chance (0 never)
QVR_CVAR(vr_gore_stick_speed, "180", CVAR_ARCHIVE)         // units/s a gib must strike a wall or a ceiling at to stick (flung or thrown)
QVR_CVAR(vr_smallgibs_test, "0", CVAR_NONE)                // the QC runs small gib test n on the nearest monster or corpse (vr_smallgibs_test.qc; Debug)
QVR_CVAR(vr_smallgibs_trace, "0", CVAR_NONE)             // tests: 1 prints each small gib's first 2.5 s (sgibtrace: lines): where made, its speed, every jump in it, what hands, weapons and held things touch, nudge or strike it, how far it lay
QVR_CVAR(vr_smallgibs_test_n, "0", CVAR_NONE)              // its tests: how many of each (0: 200)
QVR_CVAR(vr_smallgibs_test_dmg, "0", CVAR_NONE)            // a blow's damage (0: 35)
QVR_CVAR(vr_smallgibs_test_dummy, "0", CVAR_NONE)          // 1: its tests' target is the nearest training dummy (to compare its gore with a grunt's)
QVR_CVAR(vr_smallgibs_test_crowd, "0", CVAR_NONE)          // test 14: grunts (0: 24)
QVR_CVAR(vr_smallgibs_test_blasts, "0", CVAR_NONE)         // test 14: blasts in the one frame (0: 1)
QVR_CVAR(vr_smallgibs_test_hand, "0", CVAR_NONE)           // test 13: the melee and chainsaw ones at the main hand, put in the body (the mock)
QVR_CVAR(vr_smallgibs_test_dist, "0", CVAR_NONE)           // test 15: units from the wall ahead (0: 64)
QVR_CVAR(vr_body_blood_amount, "1", CVAR_ARCHIVE)      // how big the drops from your wounds are, and how much they splash
QVR_CVAR(vr_body_blood_marks, "1", CVAR_ARCHIVE)       // the chance a drop from your wounds marks the floor (0..1)
QVR_CVAR(vr_body_blood_mark_size, "1", CVAR_ARCHIVE)   // how big those marks are
QVR_CVAR(vr_body_blood_floor, "1", CVAR_ARCHIVE)       // while wounded, blood drips round your feet (from the body), marking the floor: how often (0 none)
QVR_CVAR(vr_bloom, "0.3", CVAR_ARCHIVE)                // glow around bright things (lamps, glowing panels, flashes): its strength (0 off)
QVR_CVAR(vr_bloom_threshold, "0.7", CVAR_ARCHIVE)      // how bright a thing must be to glow (0..1)
QVR_CVAR(vr_bloom_radius, "1", CVAR_ARCHIVE)           // how wide the glow spreads
QVR_CVAR(vr_bloom_fast, "0", CVAR_ARCHIVE)             // 1: the glow added by one texture tap a pixel instead of four (cheaper; halos differ slightly)
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
// Quad Damage's electric arcs (vr_shock.cpp) crackling along the lightning's beams (gun, shambler, Chthon) round Quake's
// bolt models (NOTES.md vrfiringrange_2026-10-02_01-11-17).
QVR_CVAR(vr_beam_arcs, "1", CVAR_ARCHIVE)            // how many: 1 about one every 24 units of beam (0 off)
QVR_CVAR(vr_beam_arcs_spread, "6", CVAR_ARCHIVE)     // how far they reach from the beam, units (each 0.8..2.4 of it long)
QVR_CVAR(vr_beam_arcs_width, "1", CVAR_ARCHIVE)      // their thickness, x Quad's on the arms
// Lightning's lasting shock (vr_shock.cpp, QC vr_shock.qc, vr_box3d.cpp "Shocked ragdolls"): a monster the lightning
// strikes, alive or dead, keeps Quad Damage's arcs crawling over it a while (on through its death), its ragdoll convulses
// once dead as long as they do, and a killing bolt chars it all over.
QVR_CVAR(vr_shock_death, "1", CVAR_ARCHIVE)          // the lasting shock on the monsters and bodies the lightning strikes (0: only the arcs of each hit)
QVR_CVAR(vr_shock_living, "1", CVAR_ARCHIVE)         // 1: the lasting arcs on the living monsters it strikes too (on through their death); 0: only on the bodies it kills or strikes dead (every hit keeps its own short arcs)
QVR_CVAR(vr_shock_death_time, "3", CVAR_ARCHIVE)     // how long it lasts after the last bolt, s (up to 63)
QVR_CVAR(vr_shock_seizure, "1", CVAR_ARCHIVE)        // how hard a shocked ragdoll's limbs convulse, easing off with the arcs (0 still)
QVR_CVAR(vr_shock_burns, "6", CVAR_ARCHIVE)          // burn marks a lightning kill leaves over the body (0 none; each hit chars where it strikes: vr_wounds_burns)
QVR_CVAR(vr_shock_arcs, "1", CVAR_ARCHIVE)           // how many arcs crawl over a shocked body, x (0 none)
// Smouldering bodies (vr_smoulder.cpp; Gore > Lightning Shock, Combat > Burning): thin smoke off the skin of the monsters
// and bodies the lightning strikes, and of burning ones while they burn and a while after (client-side).
QVR_CVAR(vr_smoulder, "1", CVAR_ARCHIVE)             // how much smoke, x (0 none)
QVR_CVAR(vr_smoulder_time, "5.5", CVAR_ARCHIVE)      // s a body smokes after the last lightning bolt into it, thinning (0 none)
QVR_CVAR(vr_smoulder_burn_time, "4", CVAR_ARCHIVE)   // s a burnt body smokes on after its flames go out, thinning (it smokes while they burn)
QVR_CVAR(vr_smoulder_alpha, "0.6", CVAR_ARCHIVE)     // how opaque a fresh wisp is
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
QVR_CVAR(vr_parallax_distance, "1024", CVAR_ARCHIVE)   // how far it reaches, in units (it fades out over the last quarter; 512 before config 89)
QVR_CVAR(vr_parallax_steps, "16", CVAR_ARCHIVE)        // the steps it takes along a ray: half as many looking straight on, up to twice as many at grazing angles (no more than the texels it crosses)
QVR_CVAR(vr_parallax_refine, "4", CVAR_ARCHIVE)        // round 21: steps refining where a ray meets the relief, after the walk (0 none: stairs; 1 the old look; more: smooth relief at grazing angles, no swimming)
QVR_CVAR(vr_parallax_grazing, "90", CVAR_ARCHIVE)      // round 21: the angle off the surface's normal, in degrees, where parallax has faded out at grazing angles (over the 12 before it; 90, the default: no fade; 86 before config 89; the old look 83)
QVR_CVAR(vr_parallax_depth_write, "1", CVAR_ARCHIVE)  // round 21: the carved surfaces' depth is where the view meets the relief (pixel depth offset): models, hands, particles and liquids meet sunken parts where they really are (on, the default since config 89; 0: the flat surface's; on costs 0.1-0.6 ms a frame: ROUND21.md)
QVR_CVAR(vr_parallax_debug, "0", CVAR_NONE)            // 1: the world drawn as the depth its parallax writes (vr_parallax_depth_write): red how deep below the surface, dark blue the surface's own (Debug menu)
QVR_CVAR(vr_parallax_items, "1.5", CVAR_ARCHIVE)      // parallax depth of the ammo and health boxes, in their own units (scaled with the box; 0 off)
QVR_CVAR(vr_detail, "1", CVAR_ARCHIVE)                 // detail textures: a fine grain (stone, metal, wood, grime...) over the world's textures close by (vr_detail.cpp)
QVR_CVAR(vr_detail_strength, "1", CVAR_ARCHIVE)        // ... how strongly it shows (times each kind's own, textures/vr/detail.cfg)
QVR_CVAR(vr_detail_distance, "144", CVAR_ARCHIVE)      // ... how far it reaches, in units (full within a third of it; nothing past it, no cost)
QVR_CVAR(vr_detail_fine, "1", CVAR_ARCHIVE)            // ... a second, finer grain within half that distance (one more texture read there)
QVR_CVAR(vr_parallax_models, "0", CVAR_ARCHIVE)       // parallax depth of models whose heights are made from their skins (monsters, pickups, id's weapons without a baked map), in their own units (0 off: on Quake's blocky skins it bends the texels; 0.5 to 0.75 for smooth skins)
QVR_CVAR(vr_parallax_authored, "1", CVAR_ARCHIVE)     // round 21: parallax depth of models with authored normal maps carrying heights (our hands, body, gadget, flashlight, weapons: bake_normals.py), times their baked depth (1 as baked, 0 off)
QVR_CVAR(vr_extmaps, "1", CVAR_ARCHIVE)                // external material maps for the world's textures (vr_extmaps.cpp: a pack's <name>_norm, _spec, _luma/_glow and .mat: Quetoo's, shipped) read from vr_extmaps_dir as a map loads (next map; 0 off: none read)
QVR_CVAR(vr_extmaps_dir, "textures_quetoo", CVAR_ARCHIVE) // ... the folder: relative, in the game's folders (quakevr/textures_quetoo/: Quetoo's maps, shipped, CC BY-SA 4.0); or a full path (C:/.../quetoo-data/target/default/textures/quake)
QVR_CVAR(vr_extmaps_normals, "1", CVAR_ARCHIVE)        // ... their normal maps instead of those made from the textures' shading (0: the made ones; at once: both kept)
QVR_CVAR(vr_extmaps_spec, "1", CVAR_ARCHIVE)           // ... their specular maps: the sheen (vr_specular) per texel, its colour, and the .mat's specularity and hardness (0 off: the same sheen everywhere)
QVR_CVAR(vr_extmaps_spec_scale, "4", CVAR_ARCHIVE)     // ... how bright the specular maps make the sheen (Quetoo's average about 0.15: 4 keeps the walls' average about as before)
QVR_CVAR(vr_extmaps_luma, "1", CVAR_ARCHIVE)           // ... their glow maps (_luma, _glow) on textures that glow nowhere (neither a pack's _luma nor Quake's fullbright colours)
QVR_CVAR(vr_extmaps_green, "0", CVAR_ARCHIVE)          // ... their normal maps' green: 0 turned to ours where their heights (alpha) say it runs the other way (Quetoo's does), 1 as it is, 2 turned (next map)
QVR_CVAR(vr_extmaps_match, "0.5", CVAR_ARCHIVE)        // ... used only where the pack's own picture (<name>.jpg/png beside them) matches the texture drawn this well (correlation of their detail, 0..1; 0: always): a repainted picture's bumps would not fit (next map)
QVR_CVAR(vr_anim_surface, "1", CVAR_ARCHIVE)         // an animated texture's frames (the wall buttons' +0basebtn/+1basebtn...) drawn with one surface, its first frame's: normal map, parallax heights, specular map and detail; each frame only its own colours and glow (0: each its own: the relief and sheen pulsed with the frames). At once
QVR_CVAR(vr_extmaps_ab, "0", CVAR_NONE)              // Debug A/B: 1 hides the external maps (as vr_extmaps 0) without reloading
QVR_CVAR(vr_retro, "0", CVAR_ARCHIVE)                  // retro textures (vr_retro.cpp): high-resolution textures drawn in Quake's chunky look (blocks, its palette); each kind's settings are vr_retro_<kind>_* (World: vr_retro_world*)
QVR_CVAR(vr_retro_ab, "0", CVAR_NONE)                  // Debug A/B: 1 hides the retro textures (as vr_retro 0)
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
QVR_CVAR(vr_eyeshot, "0", CVAR_NONE)                  // 1: save the next frame's eye images to <gamedir>/eyeshots (2: also the float scene, .pfm; 3: with the UI, as the headset shows it)
// Liquids (vr_water.cpp): 0 everywhere is Quake's look.
QVR_CVAR(vr_map_liquid_alpha, "0", CVAR_ARCHIVE)      // a map's own wateralpha, lavaalpha, slimealpha, telealpha (its worldspawn keys) win over r_wateralpha and the others (Ironwail's rule); 0: your settings always
QVR_CVAR(vr_water_waves, "1", CVAR_ARCHIVE)           // waves on water, slime and lava (normal-mapped, in the shader: the faces stay flat): strength
QVR_CVAR(vr_water_fresnel, "1", CVAR_ARCHIVE)         // water more see-through looking down, reflecting the room at grazing angles
QVR_CVAR(vr_slipgates, "1", CVAR_ARCHIVE)             // the whole slipgate feature (vr_portals.cpp): 0 none of it runs, no gate is built or looked through and nothing goes through one - slipgates are Quake's teleporters exactly as before the feature (vr_portals and vr_portals_walk then do nothing)
QVR_CVAR(vr_portals_ai_test, "0", CVAR_NONE)          // disposable mock fixture controls (vr_portal_ai_test.qc)
QVR_CVAR(vr_portals_ai, "1", CVAR_ARCHIVE)            // monsters see and fire at one-hop player images; melee/navigation stay local
QVR_CVAR(vr_portals_walk, "1", CVAR_ARCHIVE)          // ... walking and shooting through them is seamless (kept where you are, how you move and look, as the view through the gate shows; missiles and grenades too): 0 Quake's teleport
QVR_CVAR(vr_portals, "1", CVAR_ARCHIVE)               // slipgates show where they lead, for each visible gate and camera (vr_portals.cpp): how much of it, 0 Quake's shimmer
QVR_CVAR(vr_portals_maxviews, "4", CVAR_ARCHIVE) // visible destination scenes per camera, 1..8
QVR_CVAR(vr_slipgate_surface_size, "1.12", CVAR_ARCHIVE) // entrance shimmer's visual size; collision aperture is unchanged
QVR_CVAR(vr_slipgate_surface_opacity, "1", CVAR_ARCHIVE) // entrance shimmer's share over the portal view
QVR_CVAR(vr_water_reflections, "1", CVAR_ARCHIVE)     // water and slime reflect the room round them (a cube map of the world made at the surface near you, vr_envmap.cpp): strength, 0 a plain sheen
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
QVR_CVAR(vr_melee_positional, "1", CVAR_ARCHIVE)       // Combat > Damage and Knockback > Positional Damage: your melee blows take the same head/limb/leg multipliers as your shots, from the point on the model they met (0: flat melee damage, as before)
QVR_CVAR(vr_hit_head_priority, "1", CVAR_ARCHIVE)      // Combat > Hit Detection > Head Priority: where the contacted point lies in more than one body zone, the head's zone decides it (0: the limb/leg zones claim it first)
QVR_CVAR(vr_headshot_sound, "0.5", CVAR_ARCHIVE)      // volume of the tick on a headshot (0 off)
QVR_CVAR(vr_climb, "0", CVAR_ARCHIVE)                 // on as shipped (vr_defaults.cfg): either empty hand gripping a ledge or rung hangs from it; hand over hand, shimmy, mantle (vr_climb.cpp)
QVR_CVAR(vr_climb_min_height, "30", CVAR_ARCHIVE)     // units above the feet a ledge must be to take hold of
QVR_CVAR(vr_climb_fling, "1", CVAR_ARCHIVE)           // how much letting go flings you with the hands' motion (capped; 0 none)
QVR_CVAR(vr_climb_blast_letgo, "30", CVAR_ARCHIVE)   // a blast this strong (damage) or more knocks you off a hold: you let go and it throws you (0: never); an enemy shove always does
QVR_CVAR(vr_climb_leniency, "2", CVAR_ARCHIVE)       // cm: how far from a hold a grip still takes it (the nearest one this close; 0: the hand touching it, vr_climb_touch)
QVR_CVAR(vr_climb_leniency_air, "4", CVAR_ARCHIVE)   // cm: and this much further in mid-air (neither on the ground nor holding on: after a jump, falling)
QVR_CVAR(vr_climb_air_grab_time, "0.3", CVAR_ARCHIVE) // s: a grip pressed at no hold keeps looking for one this long while it stays pressed, in the air (a ledge jumped at: taken as the hand comes over or down onto it; 0: only at the press)
QVR_CVAR(vr_climb_mantle_grunt, "0.6", CVAR_ARCHIVE) // volume (0..1) of the player's grunt as a mantle pulls him up onto a ledge (which: vr_climb_mantle_grunt_sound); 0 off
QVR_CVAR(vr_climb_mantle_grunt_sound, "4", CVAR_ARCHIVE) // the mantle's grunt (vr_climb.cpp mantleGrunts): 0 the jump's (player/plyrjmp8.wav), 1 a hard landing's (player/land2.wav), 2 the jump's deeper, 3 a hard landing's deeper (both made from id's as they load), 4 a short pain grunt (player/pain2.wav)
QVR_CVAR(vr_climb_touch, "4.5", CVAR_ARCHIVE)        // cm: the hand this close to a ledge's top or the face under its lip touches it (the whole reach at leniency 0)
QVR_CVAR(vr_climb_over_top, "16", CVAR_ARCHIVE)      // units: a hand over a ledge's top still holds it this far in from the drop (as far as the top goes)
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
// Hits knock the hands (vr_painknock.cpp; ROUND21.md, "Pain feedback: hits knock the hands"): looks only, both drawn hands
// pushed away from the hit, more the one on its side, and eased back; the aim and the shots stay steady.
QVR_CVAR(vr_pain_knock, "1", CVAR_ARCHIVE)             // being hit knocks the drawn hands, arms and what they hold
QVR_CVAR(vr_pain_knock_strength, "0.6", CVAR_ARCHIVE)     // cm the hands are pushed for each point of damage (armour's share too), for small hits (0.75 before config 89)
QVR_CVAR(vr_pain_knock_max, "8.5", CVAR_ARCHIVE)       // cm harder hits curve towards (a rocket nearly; hits in quick succession add up to no more; 10 before config 89)
QVR_CVAR(vr_pain_knock_time, "0.3", CVAR_ARCHIVE)       // s the knock takes: out, held a moment, eased back (0.35 before config 89)
QVR_CVAR(vr_pain_knock_tip, "3", CVAR_ARCHIVE)         // degrees the hands (and what they hold) tip up for each cm they are knocked
QVR_CVAR(vr_pain_knock_seen, "1", CVAR_ARCHIVE)        // 1: the knock's size is what you see: turned across the line from your eyes to each hand (a hit from ahead lifts them, from the side throws them sideways), its move towards or away from your eyes kept on top; 0: straight away from the hit and up (from ahead nearly all of it towards your eyes, where it barely shows)
QVR_CVAR(vr_pain_haptics, "1", CVAR_ARCHIVE)           // strength of the buzz a hit gives each controller (more the harder and the nearer its side; 0: none)
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
QVR_CVAR(vr_swim_stroke_pitch, "-8", CVAR_ARCHIVE)     // degrees each stroke's push is tilted up (negative: down) in its own vertical plane, last of all (NOTES.md vrstart_2026-10-02_15-17-43)
QVR_CVAR(vr_air_supply, "2", CVAR_ARCHIVE)        // the breath under water before drowning, times Quake's 12 seconds (1 to 4; biosuit and wetsuit as ever)
QVR_CVAR(vr_swim_debug, "0", CVAR_NONE)               // print each stroke: peak speed, power gate, reverse damping and the push it gave
QVR_CVAR(vr_sword_damage_mult, "1.5", CVAR_ARCHIVE)   // a knight's sword: its swing's damage over the axe's
QVR_CVAR(vr_crowbar_damage, "20", CVAR_ARCHIVE)       // the crowbar (QC vr_crowbar.qc): a blow's base damage (the axe's; a knight's sword's 30; 25 before round 21's Weapon Damage), its hook's x1.2
// Weapon Damage (VR Settings > Combat > Weapon Damage; QC VR_WpnDmg): each weapon's base damage, id's (or the mission
// packs') as shipped. A gun's or a projectile's applies to players' (and bots') shots only: monsters and traps firing the
// same keep id's. Melee: before the blow's speed, the weight's curve and Melee's Damage Multiplier.
QVR_CVAR(vr_dmg_shotgun, "4", CVAR_ARCHIVE)          // the shotgun: a pellet's (6 a shot)
QVR_CVAR(vr_dmg_super_shotgun, "4", CVAR_ARCHIVE)    // the double-barrelled shotgun: a pellet's (14 a shot)
QVR_CVAR(vr_dmg_nail, "9", CVAR_ARCHIVE)             // the nailgun: a nail's
QVR_CVAR(vr_dmg_super_nail, "18", CVAR_ARCHIVE)      // the super nailgun: a nail's
QVR_CVAR(vr_dmg_grenade, "120", CVAR_ARCHIVE)        // a grenade's blast (the launcher's, a hand grenade's, a returned ogre grenade's)
QVR_CVAR(vr_dmg_rocket, "100", CVAR_ARCHIVE)         // a rocket's direct hit (and up to a fifth more at random); its blast 1.2 times it
QVR_CVAR(vr_dmg_lightning, "30", CVAR_ARCHIVE)       // the lightning gun: a bolt's (10 a second)
QVR_CVAR(vr_dmg_proximity, "95", CVAR_ARCHIVE)       // Scourge of Armagon's proximity gun: a mine's blast
QVR_CVAR(vr_dmg_laser, "18", CVAR_ARCHIVE)           // its laser cannon: a bolt's (1 in 10: 25/18 of it; halved on a second hit)
QVR_CVAR(vr_dmg_mjolnir_lightning, "80", CVAR_ARCHIVE) // Mjolnir's lightning: its first strike (its later ones 3/8 of it)
QVR_CVAR(vr_dmg_lava_nail, "15", CVAR_ARCHIVE)       // Dissolution of Eternity's lava nails: a nail's to a monster (to a player 3/5)
QVR_CVAR(vr_dmg_super_lava_nail, "30", CVAR_ARCHIVE) // and the super nailgun's (to a player 3/5)
QVR_CVAR(vr_dmg_multi_grenade, "90", CVAR_ARCHIVE)   // its multi-grenade: each of its 5 bomblets' blast
QVR_CVAR(vr_dmg_multi_rocket, "60", CVAR_ARCHIVE)    // its multi-rocket: a rocket's direct hit (and up to a quarter more); its blast 1.25 times it
QVR_CVAR(vr_dmg_plasma, "80", CVAR_ARCHIVE)          // its plasma gun: a ball's direct hit (and up to a quarter more); its blast 7/8, its arcs 5/8
QVR_CVAR(vr_dmg_fist, "10", CVAR_ARCHIVE)            // a punch's base damage
QVR_CVAR(vr_dmg_axe, "20", CVAR_ARCHIVE)             // the axe's blow
QVR_CVAR(vr_dmg_gun_bash, "12", CVAR_ARCHIVE)        // a gun swung as a club
QVR_CVAR(vr_dmg_chainsaw_swing, "20", CVAR_ARCHIVE)  // an ogre's chainsaw swung (its running chain: vr_chainsaw_damage)
QVR_CVAR(vr_dmg_mjolnir, "25", CVAR_ARCHIVE)         // Mjolnir's blow
QVR_CVAR(vr_chainsaw_damage, "80", CVAR_ARCHIVE)      // an ogre's chainsaw (QC vr_chainsaw.qc): damage a second its running chain deals what its bar is in
QVR_CVAR(vr_chainsaw_fuel_use, "5", CVAR_ARCHIVE)     // fuel (percent of a tank) a second the chain uses, the trigger held
QVR_CVAR(vr_chainsaw_idle_fuel_use, "0.5", CVAR_ARCHIVE) // and the engine idling
QVR_CVAR(vr_chainsaw_overlap, "6", CVAR_ARCHIVE)      // cm the running chain's bar may sink into a monster's model (vr_modelcollide.cpp)
QVR_CVAR(vr_chainsaw_start_chance, "0.5", CVAR_ARCHIVE) // the chance a good pull of the cord starts the engine
QVR_CVAR(vr_chainsaw_fail_pulls_min, "1", CVAR_ARCHIVE) // the first good pulls of a start that always fail: at least this many,
QVR_CVAR(vr_chainsaw_fail_pulls_max, "2", CVAR_ARCHIVE) // up to this many (drawn at random each start); then Start Chance
QVR_CVAR(vr_chainsaw_pull_distance, "30", CVAR_ARCHIVE) // cm the cord must be pulled out for a pull (vr_chainsaw.cpp)
QVR_CVAR(vr_chainsaw_pull_speed, "1.2", CVAR_ARCHIVE) // m/s the hand must pull it at, there
QVR_CVAR(vr_chainsaw_drop_fuel_min, "40", CVAR_ARCHIVE) // an ogre's dropped chainsaw has this much fuel (percent of a tank) or more, up to a full tank
QVR_CVAR(vr_chainsaw_drop_run, "3", CVAR_ARCHIVE)  // seconds a running chainsaw dropped or thrown runs on (QC VR_Saw_PropRuns); taken back meanwhile, still running. 0: it stops at once
// A running chainsaw (NOTES.md vrfiringrange_2026-10-01_16-37-54; Combat > Enemy Weapons > Chainsaw Engine): its engine's
// faint exhaust smoke, in a hand and lying running (vr_chainsaw.cpp, QC VR_Saw_PropThink); its shake in the hand (drawn
// only: the aim and its cuts don't move) and lying on the ground (pushed about, physically: Box3D).
QVR_CVAR(vr_chainsaw_smoke, "12", CVAR_ARCHIVE)        // puffs of exhaust smoke a second (0: none)
QVR_CVAR(vr_chainsaw_smoke_alpha, "0.6", CVAR_ARCHIVE) // each puff's opacity as it leaves the exhaust (0..1), fading out over 2 s
QVR_CVAR(vr_chainsaw_shake, "2.5", CVAR_ARCHIVE)        // mm the hand holding it shakes, held in one hand (and 0.2 degrees a mm); its chain running 1.6x
QVR_CVAR(vr_chainsaw_shake_2h, "1.25", CVAR_ARCHIVE)   // mm, held in both hands (the other steadying it)
QVR_CVAR(vr_chainsaw_shake_ground, "0.85", CVAR_ARCHIVE) // m/s of the kicks (20 a second) that rock and walk it about as it lies running (0: none)
// A pull of the cord of a chainsaw not running, with fuel (NOTES.md vrfiringrange_2026-10-01_22-54-54): the engine turns
// over, a feedback. A short shake of the hand holding it, a puff of exhaust smoke and a few sparks (vr_chainsaw.cpp); a
// weak pull (too slow) half as much. With an empty tank, nothing but the sound.
QVR_CVAR(vr_chainsaw_pull_shake, "3", CVAR_ARCHIVE)      // mm the chainsaw's hand shakes at the pull (as Shake: 0.2 degrees a mm), dying out (0: none)
QVR_CVAR(vr_chainsaw_pull_shake_time, "0.35", CVAR_ARCHIVE) // seconds it shakes for
QVR_CVAR(vr_chainsaw_pull_smoke, "12", CVAR_ARCHIVE)      // puffs of exhaust smoke at the pull (as Smoke's; 0: none)
QVR_CVAR(vr_chainsaw_pull_sparks, "24", CVAR_ARCHIVE)     // tiny sparks out of the exhaust at the pull (0: none)
QVR_CVAR(vr_chainsaw_model_handle, "1", CVAR_ARCHIVE)    // the cord's handle in the fist is the model's own, the one seated on the saw (v_chainsaw.mdl frame 10; NOTES.md vrfiringrange_2026-10-02_01-42-48); 0: a plain tube
QVR_CVAR(vr_gruntgun_damage, "5", CVAR_ARCHIVE)      // a grunt's dropped burst rifle (QC vr_enemyguns.qc): each round's damage (3 a trigger pull)
QVR_CVAR(vr_gruntgun_ammo, "30", CVAR_ARCHIVE)       // the rounds a grunt's dropped burst rifle holds (its own: never refilled; a multiple of 3)
QVR_CVAR(vr_grunt_burst, "1", CVAR_ARCHIVE)          // 1: grunts fire 3-round bursts of single bullets (QC orig_mon_soldier.qc army_fire); 0: id's shotgun (4 pellets), their dropped guns too
QVR_CVAR(vr_grunt_burst_damage, "5", CVAR_ARCHIVE)   // a grunt's burst: each round's damage to what it hits (id's shotgun: 4 pellets of 4)
QVR_CVAR(vr_enfrifle_damage, "15", CVAR_ARCHIVE)     // an enforcer's laser rifle: each laser's damage (the enforcer's: 15)
QVR_CVAR(vr_enfrifle_speed, "1800", CVAR_ARCHIVE)    // an enforcer's dropped laser rifle: its lasers' speed, units a second (the enforcer's own: 600)
QVR_CVAR(vr_enfrifle_ammo, "20", CVAR_ARCHIVE)       // the shots an enforcer's dropped rifle holds (its own: never refilled)
QVR_CVAR(vr_debug_chainsaw, "0", CVAR_NONE)           // 1: print the chainsaws' pulls, starts, stalls and cuts; 2: also draw the cord's reach
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
QVR_CVAR(vr_carry_grab_bias, "0", CVAR_ARCHIVE)       // cm: a hand takes a box, gib, backpack or armour if its fist (the palm and the curled fingers) is within this of the thing's drawn surface (negative: pressed that far into it; vr_held.cpp grabTouch)
QVR_CVAR(vr_carry_grab_drawn, "1", CVAR_ARCHIVE)      // 1: the explosive boxes (carried, not force-grabbable) are taken by the fist against their drawn surface too, as everything else carried (vr_carry_grab_bias; with vr_box3d_hand_push_fist, your own hands); 0: by the hand's 5-unit box against their entity box (the old: taken with the fist up to 12 cm off)
QVR_CVAR(vr_weapon_grab_drawn, "1", CVAR_ARCHIVE)     // 1: a weapon lying about (dropped, thrown, a monster's) is taken where the fist touches its drawn shape (vr_carry_grab_bias), a crowbar by its bar, a sword by its blade, or by the box round its handle as before; 0: only by that box
QVR_CVAR(vr_weapon_grab_slack, "5", CVAR_ARCHIVE)     // cm: with vr_weapon_grab_drawn, a weapon is taken with the fist up to this much farther off its drawn surface than vr_carry_grab_bias (a gun lying flat on the floor is thinner than the lowest the fist gets over it: 3 cm)
QVR_CVAR(vr_weapon_grab_hotspots, "1", CVAR_ARCHIVE)  // 1: an empty hand gripping a weapon lying about nearer one of its grip or blade hotspots than its handle (a chainsaw's front handle, a crowbar's bar) carries it by that hotspot, as the two-handed hand-off does, until a hand takes its handle (not a cup hotspot; a force grab always takes the handle); 0: always by its handle
QVR_CVAR(vr_weapon_grab_anywhere, "1", CVAR_ARCHIVE)  // 1: a weapon is held wherever a hand grips it, away from its handle and hotspots (vr_weapon_grab_anywhere_min), as a prop is: taken off the floor there, the other hand on a held one (vr_weapon_anygrip_mode), both hands on a carried one; held off its handle it does not fire (melee and throws work), a hand taking the handle uses it (vr_twohand.cpp, round 21 "Weapons held anywhere"); 0: only by the handle and hotspots
QVR_CVAR(vr_weapon_grab_anywhere_min, "12", CVAR_ARCHIVE) // cm: with it, how far from the weapon's handle and its grip and blade hotspots a hand must grip it to hold it there (nearer: the handle or the hotspot, which take priority)
QVR_CVAR(vr_weapon_anygrip_mode, "0", CVAR_ARCHIVE)  // the other hand gripping a weapon held by its handle away from its hotspots: 0 steers the aim as a foregrip (two-handed aiming from where it holds it), 1 supports its weight only (the aim stays the holding hand's), 2 a rigid two-handed hold (the weapon follows both hands, as a prop held in both); each weapon may override it (vr_wofs_anygrip_mode_NN: -1 this)
QVR_CVAR(vr_debug_buttons, "0", CVAR_NONE)            // prints each controller button press and release (the hand, the button, its key and binding, and what took it: the posing mode, a voice note, the flashlight...), and the interaction profile the runtime picked for each hand when it changes
QVR_CVAR(vr_debug_hands, "0", CVAR_NONE)              // prints each hand's state when it changes (2: every frame): the grip, what it holds (weapon, carried object, force grab, flashlight), its climbing hold and why a grip takes none, the hotspot (vr_climb.cpp debugHands)
QVR_CVAR(vr_debug_carry, "0", CVAR_NONE)              // draws, for each hand near a carriable thing, the grab test: the box the thing is drawn in, the fist tested (its spheres; the nearest bright) and the nearest point of its surface (green: it touches, in reach); 2: also carry_trace.txt, what is carried and both hands every frame, and each grab test printed; 3: and the fists not near
QVR_CVAR(vr_debug_hand_collide, "0", CVAR_NONE)       // Debug > Views > Log Empty Hand Against Held: prints, each frame a free hand meets the weapon or the prop the other hand holds, how near the real and the drawn hand's palm and knuckles are to its surface (cm, negative in it), how far it and the thing are drawn held back (vr_hand_collide); 2: also each point held and its plane
QVR_CVAR(vr_debug_arm, "0", CVAR_NONE)                // prints each drawn arm's IK once (the shoulder, elbow and wrist in the body's axes, cm; the elbow's swing; the wrist's bend and twist against the forearm), then turns itself off; 2: every frame
QVR_CVAR(vr_debug_lean, "0", CVAR_NONE)               // lean_trace.txt (the game directory): the head, the box, the lean, the pelvis and the feet every frame, and the lean's cues (vr_lean_detect)
QVR_CVAR(vr_grapple_test_stuck, "0", CVAR_NONE)        // Debug > Tests > Grappling Hook > Load Stuck: what hangs on the rope (a loose hook, a hooked prop) stays where it is, as if snagged (tests the rope holding you back)
QVR_CVAR(vr_debug_rope, "0", CVAR_NONE)                // Debug > Views > Show Grapple Rope: each grappling hook rope's drawn chain (points white, pieces green), the corners it wraps round (red) and its taut path (yellow) (vr_rope.cpp)
QVR_CVAR(vr_debug_hitzones, "0", CVAR_NONE)          // Debug > Show Hit Zones: 1 animated positional damage surfaces, 2 legacy decapitation reference zones, 3 both; precise hits off: standing box/ray references
QVR_CVAR(vr_debug_hitzones_xray, "0", CVAR_NONE)     // Debug > Hit Zones Through Walls: see animated region surfaces through models and walls
QVR_CVAR(vr_debug_physics_shapes, "0", CVAR_NONE)     // Debug > Views > Show Physics Shapes: Box3D's bodies drawn as wireframes (props awake green, asleep blue, held yellow, doors purple, monsters orange, players cyan, hanging pickups grey; contact points), and the hands' grab probes (as vr_debug_carry)
QVR_CVAR(vr_debug_hand_bones, "0", CVAR_NONE)         // Debug > Views > Show Hand Bones: the jointed hands' joints and bones, the grasp's contact spheres (green touching, yellow near, red sunk in) and the palm's fit (where it was, where the grasp put it)
QVR_CVAR(vr_carry_two_hands, "1", CVAR_ARCHIVE)       // a box, backpack, gib, head or armour carried in one hand can be gripped by the other too: held in both, it follows both hands (vr_carry2h.cpp)
QVR_CVAR(vr_carry_two_hands_reach, "2", CVAR_ARCHIVE) // with it: cm past Grab Distance Bias the other hand may be off the surface of what one hand holds and still take it (the hand that let go of it grips it again where it is: a fitted prop sits about 1 cm off the fist test; vr_carry2h.cpp reaches)
QVR_CVAR(vr_carry_two_hands_drift, "8", CVAR_ARCHIVE) // with it: cm each drawn hand may be off its controller to stay on its grip (hands pulled apart or pushed together)
QVR_CVAR(vr_carry_two_hands_detach, "3", CVAR_ARCHIVE) // with it: cm past the drift a hand may be off its grip before it lets go (its fist off the prop: pulled away, not sliding on it); both off: it drops (vr_carry2h.cpp detached)
QVR_CVAR(vr_carry_two_hands_solid, "1", CVAR_ARCHIVE)  // with it: what is held in both hands stops at the level's solids (walls, floors, doors) and slides along them, however hard the hands push (vr_carry2h.cpp clear)
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
QVR_CVAR(vr_prop_touch_fast, "1", CVAR_NONE) // skip provably inactive slow prop contacts; 0: reference callbacks
QVR_CVAR(vr_prop_touch_verify, "0", CVAR_NONE) // run skipped callbacks and verify their entity/global/network state (sample each touch type per frame)
QVR_CVAR(vr_prop_impact_damage, "1", CVAR_ARCHIVE)        // on
QVR_CVAR(vr_prop_impact_players, "1", CVAR_ARCHIVE)       // 1: players too (never the one who just let go of it), 0: monsters only
QVR_CVAR(vr_gib_spawn_harmless, "0.3", CVAR_ARCHIVE)      // s a gib, head or small gib just made can't hurt anyone as a flung prop (gibbing a body you stand on hurt you)
QVR_CVAR(vr_prop_drop_grace, "0.5", CVAR_ARCHIVE)         // s a weapon or backpack a monster just dropped can't hurt players (vr_prop_impact_players)
QVR_CVAR(vr_prop_drop_falls_only, "1", CVAR_ARCHIVE)     // after it, it hurts a player only falling on him or thrown by a hand (0: as any flung prop)
QVR_CVAR(vr_prop_drop_pass_inside, "1", CVAR_ARCHIVE)    // made inside a player or a monster (one killed underfoot), it passes through it until clear (0: Box3D pushes it out, flinging it)
QVR_CVAR(vr_throw_self_grace, "0.35", CVAR_ARCHIVE)       // s after you throw or let go of a thing before it can hurt you (thrown up, falling back on you; 0: at once)
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
QVR_CVAR(vr_corpse_health_mult, "1", CVAR_ARCHIVE)    // times every monster's corpse health (vr_corpse_health_<monster>): the damage that gibs a corpse or a ragdoll
QVR_CVAR(vr_corpse_nogib, "0", CVAR_ARCHIVE)          // never gibbed (testing): 0 off, 1 corpses and ragdolls (and dying bodies) take damage but never burst, 2 also monsters die whole (their death code never gibs them)
QVR_CVAR(vr_corpse_health_grunt, "80", CVAR_ARCHIVE) // the damage that gibs a grunt's corpse or ragdoll (times vr_corpse_health_mult)
QVR_CVAR(vr_corpse_health_enforcer, "80", CVAR_ARCHIVE) // the damage that gibs a enforcer's corpse or ragdoll (times vr_corpse_health_mult)
QVR_CVAR(vr_corpse_health_dog, "80", CVAR_ARCHIVE) // the damage that gibs a dog's corpse or ragdoll (times vr_corpse_health_mult)
QVR_CVAR(vr_corpse_health_fiend, "180", CVAR_ARCHIVE) // the damage that gibs a fiend's corpse or ragdoll (times vr_corpse_health_mult)
QVR_CVAR(vr_corpse_health_ogre, "140", CVAR_ARCHIVE) // the damage that gibs a ogre's corpse or ragdoll (times vr_corpse_health_mult)
QVR_CVAR(vr_corpse_health_knight, "80", CVAR_ARCHIVE) // the damage that gibs a knight's corpse or ragdoll (times vr_corpse_health_mult)
QVR_CVAR(vr_corpse_health_hellknight, "160", CVAR_ARCHIVE) // the damage that gibs a hellknight's corpse or ragdoll (times vr_corpse_health_mult)
QVR_CVAR(vr_corpse_health_vore, "220", CVAR_ARCHIVE) // the damage that gibs a vore's corpse or ragdoll (times vr_corpse_health_mult)
QVR_CVAR(vr_corpse_health_shambler, "280", CVAR_ARCHIVE) // the damage that gibs a shambler's corpse or ragdoll (times vr_corpse_health_mult)
QVR_CVAR(vr_corpse_health_scrag, "80", CVAR_ARCHIVE) // the damage that gibs a scrag's corpse or ragdoll (times vr_corpse_health_mult)
QVR_CVAR(vr_corpse_health_fish, "80", CVAR_ARCHIVE) // the damage that gibs a fish's corpse or ragdoll (times vr_corpse_health_mult)
QVR_CVAR(vr_corpse_health_zombie, "60", CVAR_ARCHIVE) // the damage that gibs a beheaded zombie's corpse (its ragdoll; times vr_corpse_health_mult)
QVR_CVAR(vr_corpse_health_mummy, "120", CVAR_ARCHIVE) // the damage that gibs a beheaded mummy's corpse (Rogue; its ragdoll; times vr_corpse_health_mult)
QVR_CVAR(vr_corpse_health_gremlin, "100", CVAR_ARCHIVE) // the damage that gibs a gremlin's corpse or ragdoll (times vr_corpse_health_mult)
QVR_CVAR(vr_corpse_health_scourge, "180", CVAR_ARCHIVE) // the damage that gibs a scourge's corpse or ragdoll (times vr_corpse_health_mult)
QVR_CVAR(vr_corpse_health_eel, "80", CVAR_ARCHIVE) // the damage that gibs a eel's corpse or ragdoll (times vr_corpse_health_mult)
QVR_CVAR(vr_corpse_dmg_shots, "1", CVAR_ARCHIVE) // times the damage corpses and ragdolls take from shotguns and the grunts' guns
QVR_CVAR(vr_corpse_dmg_nails, "1", CVAR_ARCHIVE) // times the damage corpses and ragdolls take from nails, an enforcer's laser, knights' and scrags' spikes
QVR_CVAR(vr_corpse_dmg_explosions, "1", CVAR_ARCHIVE) // times the damage corpses and ragdolls take from rockets, grenades, plasma and their blasts
QVR_CVAR(vr_corpse_dmg_lightning, "1", CVAR_ARCHIVE) // times the damage corpses and ragdolls take from the lightning gun, Mjolnir, a discharge
QVR_CVAR(vr_corpse_dmg_blunt, "1", CVAR_ARCHIVE) // times the damage corpses and ragdolls take from a crowbar, a gun or Mjolnir swung, a pommel
QVR_CVAR(vr_corpse_dmg_fists, "1", CVAR_ARCHIVE) // times the damage corpses and ragdolls take from an empty fist
QVR_CVAR(vr_corpse_dmg_blades, "1", CVAR_ARCHIVE) // times the damage corpses and ragdolls take from the axe's and a sword's edge or point, swung or thrown; a stuck axe bleeding it
QVR_CVAR(vr_corpse_dmg_chainsaw, "1", CVAR_ARCHIVE) // times the damage corpses and ragdolls take from the chainsaw
QVR_CVAR(vr_corpse_dmg_props, "1", CVAR_ARCHIVE) // times the damage corpses and ragdolls take from thrown or flung boxes, gibs, heads and weapons, deflected shots, a prop in the fist
QVR_CVAR(vr_corpse_dmg_fire, "1", CVAR_ARCHIVE) // times the damage corpses and ragdolls take from burning (vr_burn_corpse_damage first)
QVR_CVAR(vr_corpse_dmg_bash, "1", CVAR_ARCHIVE) // times the damage corpses and ragdolls take from bashes, shoves and headbutts
QVR_CVAR(vr_corpse_dmg_other, "1", CVAR_ARCHIVE) // times the damage corpses and ragdolls take from anything else: monsters' blows, kicks, crushers
QVR_CVAR(vr_corpse_collide, "4", CVAR_ARCHIVE)        // corpses in the physics (vr_box3d.cpp): 0 none (things pass through), 1 fixed box, 2 heavy pushable box, 3 fixed shape of its pose, 4 heavy pushable shape of its pose
QVR_CVAR(vr_corpse_collide_mass, "150", CVAR_ARCHIVE) // kg: a pushable corpse's mass (heavier: harder to push; a player walking into one shoves it under about 60)
QVR_CVAR(vr_corpse_collide_friction, "1", CVAR_ARCHIVE) // a pushable corpse's friction (with the floor's: 0.6)
QVR_CVAR(vr_corpse_collide_props, "1", CVAR_ARCHIVE)  // loose props (boxes, weapons, gibs, heads lying or falling) meet corpses
QVR_CVAR(vr_corpse_collide_thrown, "1", CVAR_ARCHIVE) // thrown things in flight meet corpses (0: they pass through, their hits still hurt)
QVR_CVAR(vr_corpse_collide_held, "1", CVAR_ARCHIVE)   // held things (props, weapons, the hands' bodies) push pushable corpses, and props held in both hands stop at them
QVR_CVAR(vr_corpse_collide_player, "0", CVAR_ARCHIVE) // the player meets corpses: 0 walks through, 1 steps over them (a 16 unit step), 2 solid (walk round or jump onto)
QVR_CVAR(vr_corpse_collide_monsters, "0", CVAR_ARCHIVE) // monsters step over corpses (16 units) instead of walking through
QVR_CVAR(vr_ragdoll, "1", CVAR_ARCHIVE)               // ragdolls (experimental; vr_ragdoll.cpp, vr_box3d.cpp): 0 off, 1 the grunt, knight, ogre, enforcer, death knight, rottweiler and scrag: dying, his body goes limp as jointed bodies his mesh is skinned to
QVR_CVAR(vr_ragdoll_start, "0.3", CVAR_ARCHIVE)       // when a dying grunt goes limp: a share of his death animation (0 its first frame he isn't solid in, 1 its last: lying still)
QVR_CVAR(vr_ragdoll_max, "8", CVAR_ARCHIVE)           // the most ragdolls at once; one more death retires the oldest body to make room (QC VR_Knockdown_MakeRoom), else it lies as a corpse (Corpse Collision)
// Knockdowns (QC vr_knockdown.qc; vr_box3d.cpp "Knockdowns"; Combat > Knockdowns): a shove can knock a monster with a
// ragdoll rig down as a ragdoll, alive; after a while it gets up (its own fall-and-rise frames, or its death backwards).
QVR_CVAR(vr_knockdown, "1", CVAR_ARCHIVE)                 // the feature: 0 off (shoves stagger as before)
QVR_CVAR(vr_knockdown_chance, "1", CVAR_ARCHIVE)          // every monster's chance times this (testing: 100, every shove)
QVR_CVAR(vr_knockdown_chance_army, "0.45", CVAR_ARCHIVE)  // a shove's base chance to knock one down, by monster
QVR_CVAR(vr_knockdown_chance_enforcer, "0.3", CVAR_ARCHIVE)
QVR_CVAR(vr_knockdown_chance_dog, "0.35", CVAR_ARCHIVE)
QVR_CVAR(vr_knockdown_chance_zombie, "0.35", CVAR_ARCHIVE)
QVR_CVAR(vr_knockdown_chance_mummy, "0.2", CVAR_ARCHIVE)
QVR_CVAR(vr_knockdown_chance_knight, "0.25", CVAR_ARCHIVE)
QVR_CVAR(vr_knockdown_chance_hknight, "0.12", CVAR_ARCHIVE)
QVR_CVAR(vr_knockdown_chance_ogre, "0.15", CVAR_ARCHIVE)
QVR_CVAR(vr_knockdown_chance_demon, "0.1", CVAR_ARCHIVE)
QVR_CVAR(vr_knockdown_chance_shambler, "0.03", CVAR_ARCHIVE)
QVR_CVAR(vr_knockdown_chance_gremlin, "0.25", CVAR_ARCHIVE)
QVR_CVAR(vr_knockdown_damage, "2", CVAR_ARCHIVE)          // the chance times 1 + this times the share of its health it has lost
QVR_CVAR(vr_knockdown_onehand, "1", CVAR_ARCHIVE)         // the chance times this for a one-handed shove,
QVR_CVAR(vr_knockdown_twohand, "1.75", CVAR_ARCHIVE)      // and this for a two-handed one
QVR_CVAR(vr_knockdown_bash, "0", CVAR_ARCHIVE)            // and this for a weapon's bash (0: only open-palm shoves)
QVR_CVAR(vr_knockdown_stamina, "1", CVAR_ARCHIVE)         // how much low stamina cuts the chance: times 1 - this x the share spent (0: no effect)
QVR_CVAR(vr_knockdown_time_min, "2.5", CVAR_ARCHIVE)      // seconds it stays down, at least,
QVR_CVAR(vr_knockdown_time_max, "4.5", CVAR_ARCHIVE)      // and at most (at random between)
QVR_CVAR(vr_knockdown_hit_time, "0.5", CVAR_ARCHIVE)      // seconds each hit while down adds (not past Time Max from the hit)
QVR_CVAR(vr_knockdown_retry, "0.5", CVAR_ARCHIVE)         // seconds between tries to get up where there is no room
QVR_CVAR(vr_knockdown_search, "48", CVAR_ARCHIVE)         // how far (units) from the body it looks for room to stand
QVR_CVAR(vr_knockdown_blend, "0.35", CVAR_ARCHIVE)        // seconds the body blends from the ragdoll into the get-up
QVR_CVAR(vr_knockdown_getup_speed, "1", CVAR_ARCHIVE)     // the get-up's frames' speed (1: Quake's 10 a second)
QVR_CVAR(vr_knockdown_push, "1", CVAR_ARCHIVE)            // the knocked body's launch, times the shove's push
QVR_CVAR(vr_knockdown_ledge, "1", CVAR_ARCHIVE)           // a shove that carries it over a ledge always knocks it down (0: the chance as anywhere)
QVR_CVAR(vr_knockdown_ledge_drop, "64", CVAR_ARCHIVE)     // how high (units) a drop must be to count as a ledge (stairs and steps do not)
QVR_CVAR(vr_knockdown_ledge_reach, "1", CVAR_ARCHIVE)    // how far ahead a ledge is looked for: times the shove's reckoned travel (the hop and the slide; 1: as far as it goes)
QVR_CVAR(vr_knockdown_ledge_margin, "16", CVAR_ARCHIVE)  // units past that travel (times vr_knockdown_ledge_reach) where a ledge still counts
QVR_CVAR(vr_knockdown_weld, "1", CVAR_ARCHIVE)            // its weapon stays in its hand while down (0: it falls loose)
QVR_CVAR(vr_knockdown_debug, "0", CVAR_NONE)              // prints each shove's chance and roll, and the get-ups

QVR_CVAR(vr_ragdoll_mass, "80", CVAR_ARCHIVE)         // kg: a ragdoll's whole mass (its parts by their volume); each class has its own by default (vr_ragdoll_<class>_mass), this is for a class set to -1
QVR_CVAR(vr_ragdoll_friction, "0.8", CVAR_ARCHIVE)    // a ragdoll's friction (with the floor's: 0.6)
QVR_CVAR(vr_ragdoll_joint_friction, "1.5", CVAR_ARCHIVE) // N m: the joints' friction (a motor holding each still up to this torque); 0 limp
QVR_CVAR(vr_ragdoll_damping, "0.4", CVAR_ARCHIVE)     // its parts' angular damping (1/s): less flailing
QVR_CVAR(vr_ragdoll_blast, "2", CVAR_ARCHIVE)         // times the throw a blast gives a prop of a ragdoll's mass (explosions throw ragdolls)
QVR_CVAR(vr_ragdoll_inherit, "1", CVAR_ARCHIVE)       // share of the death animation's motion its parts start with (0: still)
QVR_CVAR(vr_debug_ragdoll, "0", CVAR_NONE)            // print each ragdoll made, its pushes and blasts
QVR_CVAR(vr_ragdoll_joint_stiffness, "0", CVAR_ARCHIVE) // Hz: a spring in each joint pulling it back towards how he stood (0 none; 1-3 a body with some tone left)
QVR_CVAR(vr_ragdoll_limits, "1", CVAR_ARCHIVE)        // the joints' limits (cone, twist, the knees' and elbows' range) times this (0.25 stiff .. 1.5 loose)
QVR_CVAR(vr_ragdoll_collide_each, "1", CVAR_ARCHIVE)  // ragdolls meet each other (a pile of them lies on itself); 0 they pass through each other
QVR_CVAR(vr_ragdoll_grab, "1", CVAR_ARCHIVE)          // take a ragdoll's limb: 0 never, 1 by hand; legacy 2 also only by hand
QVR_CVAR(vr_ragdoll_grab_force, "3000", CVAR_ARCHIVE) // N: the most a hand pulls a held limb with (3000: a grunt lifted by his chest sags a unit; 1500: about 18)
QVR_CVAR(vr_ragdoll_grab_reach, "6", CVAR_ARCHIVE)    // units: how near a limb's surface the hand must be to take it
QVR_CVAR(vr_ragdoll_grab_fit, "1", CVAR_ARCHIVE)      // a taken limb's surface brought onto the palm (as a carried prop's: vr_held_surface_fit); 0: held where the hand's middle was
QVR_CVAR(vr_ragdoll_hand_stick, "12", CVAR_ARCHIVE)   // cm: the most the hand holding a limb is drawn off its controller to stay on the limb (it lags and sags); 0: on the controller
QVR_CVAR(vr_ragdoll_hand_turn, "60", CVAR_ARCHIVE)    // degrees: and the most it is drawn turned off it, with the limb twisting in the hand (the hand turns about its fingers on the limb)
QVR_CVAR(vr_ragdoll_throw, "1", CVAR_ARCHIVE)         // times the hand's throw a limb let go of keeps (0: only what the pull gave it)
QVR_CVAR(vr_ragdoll_blood, "1", CVAR_ARCHIVE)         // a ragdoll's limbs flung fast leave blood trails and drops (as gibs do: vr_gib_blood_trail)
QVR_CVAR(vr_ragdoll_smooth, "1", CVAR_ARCHIVE)        // drawn between the server's last two steps, as props are (smooth at a headset's 90 Hz over the server's 72); 0 the latest step (judders)
QVR_CVAR(vr_decap, "1", CVAR_ARCHIVE)                 // decapitation (QC vr_decap.qc): a killing slash at the head of a monster with a ragdoll cuts its head off (its ragdoll headless, a blood fountain, the head flies)
QVR_CVAR(vr_decap_speed, "5", CVAR_ARCHIVE)           // m/s the blade's struck point must move at (across the blade: a slash, not a stab) to behead
QVR_CVAR(vr_decap_slash_angle, "45", CVAR_ARCHIVE)   // degrees: the blade's struck point must move at least this far off its line to behead (nearer it: a stab); was 60
QVR_CVAR(vr_decap_head_size, "1.25", CVAR_ARCHIVE)   // melee beheading only (a slash, the chainsaw, a corpse's head): the head zone's radius times this (shots and thrown axes keep the head zone)
QVR_CVAR(vr_decap_neck, "6", CVAR_ARCHIVE)           // units: and the zone drawn down this far below the head's middle (a capsule: the neck); a ragdoll's neck reach at least this
QVR_CVAR(vr_decap_pop_body_speed, "0.1", CVAR_ARCHIVE) // a head pop: the headless ragdoll's knock (the shot's, the frame's) times this; its own motion kept (vr_decap_own_motion)
QVR_CVAR(vr_decap_own_motion, "1", CVAR_ARCHIVE)   // a beheaded (or popped) monster's ragdoll goes on with its own motion (its walk or run: its origin's over its last steps) in full; 0: as before (a walk not carried, a pop scaling all of it)
QVR_CVAR(vr_decap_head_speed, "0.5", CVAR_ARCHIVE)    // share of the blade's velocity the cut head flies off with (and its spin from the swing)
QVR_CVAR(vr_decap_head_lift, "120", CVAR_ARCHIVE)     // units/s the cut head is thrown up besides
QVR_CVAR(vr_decap_corpses, "1", CVAR_ARCHIVE)         // a slash to a ragdoll's (or a dying or dead body's) head beheads it, instead of taking its corpse health
QVR_CVAR(vr_decap_zombies, "1", CVAR_ARCHIVE)         // a slash at a zombie's head beheads it: it dies for good (its ragdoll headless), whatever the damage
QVR_CVAR(vr_decap_fountain, "2.5", CVAR_ARCHIVE)      // s the stump spurts blood for (0: none)
QVR_CVAR(vr_decap_thrown, "1", CVAR_ARCHIVE)          // a thrown axe going in edge first beheads too
QVR_CVAR(vr_decap_chainsaw, "1", CVAR_ARCHIVE)        // the chainsaw beheads too (its running chain at the neck, or a swing)
QVR_CVAR(vr_decap_shotgun, "1", CVAR_ARCHIVE)         // a shotgun blast killing with a headshot pops the head (no head flies: a burst of blood and small gibs), a headless ragdoll
QVR_CVAR(vr_decap_super_shotgun, "1", CVAR_ARCHIVE)   // ... a super shotgun blast
QVR_CVAR(vr_decap_lightning, "1", CVAR_ARCHIVE)       // ... a lightning gun bolt at the head that kills
QVR_CVAR(vr_decap_head_share, "0.5", CVAR_ARCHIVE)    // share of a blast's damage on him (after the headshot multiplier) its head pellets must deal to pop the head
// Head pop chance (QC vr_decap.qc, "Head pop chance"; ROUND21.md): a shotgun's or super shotgun's headshot kill pops the head
// always within _always_range, never beyond _never_range (player lengths, 56 units), and between by chance:
// scale x (1 - t)^falloff x ((1 - pellet_weight) + pellet_weight x the share of the blast's pellets at the head), t the
// way from the one range to the other.
QVR_CVAR(vr_decap_pop_chance, "1", CVAR_ARCHIVE)          // shots' head pops by chance (0: every headshot kill pops, as before)
QVR_CVAR(vr_decap_pop_always_range, "3", CVAR_ARCHIVE)    // player lengths (56 units): a shotgun's or super shotgun's headshot kill this near always pops
QVR_CVAR(vr_decap_pop_never_range, "15", CVAR_ARCHIVE)    // player lengths: this far or farther it never does (the lightning gun's always: aside)
QVR_CVAR(vr_decap_pop_ssg_scale, "1.25", CVAR_ARCHIVE)    // the super shotgun's chance between the ranges, times this
QVR_CVAR(vr_decap_pop_ssg_falloff, "1", CVAR_ARCHIVE)     // its falloff exponent over the way between them (1: straight; higher: dies away sooner)
QVR_CVAR(vr_decap_pop_sg_scale, "0.75", CVAR_ARCHIVE)     // the shotgun's chance between the ranges, times this
QVR_CVAR(vr_decap_pop_sg_falloff, "4", CVAR_ARCHIVE)     // its falloff exponent (stronger than the super shotgun's)
QVR_CVAR(vr_decap_pop_pellet_weight, "0.75", CVAR_ARCHIVE) // how much the share of the blast's pellets at the head weighs (0: none; 1: in proportion)
QVR_CVAR(vr_decap_pop_lightning_always, "1", CVAR_ARCHIVE) // a lightning bolt's headshot kill always pops, at any range (0: the ranges, the super shotgun's falloff, all of it at the head)
QVR_CVAR(vr_decap_pop_thrown, "1", CVAR_ARCHIVE)          // a thrown (or flung) blunt weapon or prop killing with a headshot pops the head
QVR_CVAR(vr_decap_pop_thrown_mass, "4", CVAR_ARCHIVE)     // kg: this heavy or heavier, always
QVR_CVAR(vr_decap_pop_thrown_light_chance, "0", CVAR_ARCHIVE) // a lighter one's chance (0: never)
QVR_CVAR(vr_decap_pop_roll, "-1", CVAR_NONE)              // tests: the head pop roll's value (0..1; below 0: random)
QVR_CVAR(vr_decap_test, "0", CVAR_NONE)               // the QC runs decapitation test n on the nearest monster or corpse (vr_decap.qc; Debug > Gore Tests)
QVR_CVAR(vr_shock_hit_test, "0", CVAR_NONE)          // the QC strikes the nearest monster or corpse ahead with a lightning bolt of this much damage (-1: just enough to kill), once (vr_shock.qc; Debug > Gore Tests)
QVR_CVAR(vr_decap_poptest_dist, "10", CVAR_NONE)       // vr_decap_test 41..46 (head pop chance): shot from this many player lengths (56 units) off
QVR_CVAR(vr_decap_poptest_n, "200", CVAR_NONE)         // vr_decap_test 45, 46: blasts fired
QVR_CVAR(vr_decap_poptest_wid, "10", CVAR_NONE)        // vr_decap_test 47: the weapon thrown (QC WID_*: 10 the rocket launcher)
QVR_CVAR(vr_decap_poptest_body, "0", CVAR_NONE)        // vr_decap_test 47, 48: thrown at the body instead
QVR_CVAR(vr_ragdoll_held_local, "1", CVAR_ARCHIVE)    // a held limb's ragdoll follows your hand at the frame rate, as a held prop does (the server's step lags it); 0 the server's pose only
// Per monster class (Gibs and Corpses > Ragdolls > Grunt, > Knight): -1 (Global) is the setting above, any other value replaces it
// for that class. Each class's mass is its own by default (the author, 2026-10-03: ROUND21.md, "Ragdoll masses per monster").
QVR_CVAR(vr_ragdoll_army_start, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_army_mass, "80", CVAR_ARCHIVE)     // kg: each class its own weight (from its size and look; -1: vr_ragdoll_mass)
QVR_CVAR(vr_ragdoll_army_friction, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_army_joint_friction, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_army_joint_stiffness, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_army_limits, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_army_damping, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_army_blast, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_army_inherit, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_knight_start, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_knight_mass, "90", CVAR_ARCHIVE)   // (in mail)
QVR_CVAR(vr_ragdoll_knight_friction, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_knight_joint_friction, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_knight_joint_stiffness, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_knight_limits, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_knight_damping, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_knight_blast, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_knight_inherit, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_ogre_start, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_ogre_mass, "200", CVAR_ARCHIVE)    // (his ragdoll's hulls 4.8 times the grunt's; 200 so a hand still lifts him: 3000 N grip)
QVR_CVAR(vr_ragdoll_ogre_friction, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_ogre_joint_friction, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_ogre_joint_stiffness, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_ogre_limits, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_ogre_damping, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_ogre_blast, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_ogre_inherit, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_enforcer_start, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_enforcer_mass, "100", CVAR_ARCHIVE) // (in armour)
QVR_CVAR(vr_ragdoll_enforcer_friction, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_enforcer_joint_friction, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_enforcer_joint_stiffness, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_enforcer_limits, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_enforcer_damping, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_enforcer_blast, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_enforcer_inherit, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_hknight_start, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_hknight_mass, "130", CVAR_ARCHIVE)  // (in plate)
QVR_CVAR(vr_ragdoll_hknight_friction, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_hknight_joint_friction, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_hknight_joint_stiffness, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_hknight_limits, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_hknight_damping, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_hknight_blast, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_hknight_inherit, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_dog_start, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_dog_mass, "40", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_dog_friction, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_dog_joint_friction, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_dog_joint_stiffness, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_dog_limits, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_dog_damping, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_dog_blast, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_dog_inherit, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_wizard_start, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_wizard_mass, "40", CVAR_ARCHIVE)   // (a scrag: wings and a tail)
QVR_CVAR(vr_ragdoll_wizard_friction, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_wizard_joint_friction, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_wizard_joint_stiffness, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_wizard_limits, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_wizard_damping, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_wizard_blast, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_wizard_inherit, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_zombie_start, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_zombie_mass, "70", CVAR_ARCHIVE)   // (a zombie: only beheaded, ROUND21.md "Decapitation")
QVR_CVAR(vr_ragdoll_zombie_friction, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_zombie_joint_friction, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_zombie_joint_stiffness, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_zombie_limits, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_zombie_damping, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_zombie_blast, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_zombie_inherit, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_demon_start, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_demon_mass, "140", CVAR_ARCHIVE)   // (a fiend: big and heavy-limbed)
QVR_CVAR(vr_ragdoll_demon_friction, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_demon_joint_friction, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_demon_joint_stiffness, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_demon_limits, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_demon_damping, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_demon_blast, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_demon_inherit, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_shambler_start, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_shambler_mass, "280", CVAR_ARCHIVE)   // (a shambler: the heaviest; a hand's 3000 N grip still lifts him)
QVR_CVAR(vr_ragdoll_shambler_friction, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_shambler_joint_friction, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_shambler_joint_stiffness, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_shambler_limits, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_shambler_damping, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_shambler_blast, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_shambler_inherit, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_gremlin_start, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_gremlin_mass, "20", CVAR_ARCHIVE)   // (a gremlin: small and light)
QVR_CVAR(vr_ragdoll_gremlin_friction, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_gremlin_joint_friction, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_gremlin_joint_stiffness, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_gremlin_limits, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_gremlin_damping, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_gremlin_blast, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_gremlin_inherit, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_mummy_start, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_mummy_mass, "75", CVAR_ARCHIVE)   // (a mummy (Rogue): only beheaded, ROUND21.md "Ragdolls 5")
QVR_CVAR(vr_ragdoll_mummy_friction, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_mummy_joint_friction, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_mummy_joint_stiffness, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_mummy_limits, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_mummy_damping, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_mummy_blast, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_ragdoll_mummy_inherit, "-1", CVAR_ARCHIVE)
QVR_CVAR(vr_parry, "1", CVAR_ARCHIVE)                 // parry monsters' melee blows with a weapon held level across in front of you
QVR_CVAR(vr_parry_interrupt, "1", CVAR_ARCHIVE)       // successful parries cancel the remaining melee attack and briefly stagger the monster; 0: the old reduction/push only
QVR_CVAR(vr_parry_stagger, "0.75", CVAR_ARCHIVE)      // seconds before a parried monster can recover (clamped to 0.1..3; 0.35 before config 89)
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
QVR_CVAR(vr_parry_cooldown, "0.4", CVAR_ARCHIVE)        // s: a monster's blow parried within this long of its last parried blow is still parried (its damage cut, both pushed apart) but plays no sound or sparks, opens no new counter window and costs no stamina (one parry for a burst of quick hits: the ogre's chainsaw, a knight's swing; 0: every blow a full parry)
QVR_CVAR(vr_parry_cooldown_attack, "1", CVAR_ARCHIVE)  // 1: also a blow of the same melee attack as the monster's last parried blow (begun up to 2 s ago) is a cooldown blow, however long between them (the demon's two claws, the Overlord's double smash: one parry, one stamina cost); 0: only vr_parry_cooldown's time
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
QVR_CVAR(vr_counter_damage, "1.5", CVAR_ARCHIVE)        // a counter's damage (and a bash's knockback) multiplier (shipped 1.2 before config 89)
QVR_CVAR(vr_counter_sound, "1", CVAR_ARCHIVE)           // volume of the counter's sounds: a blade's shing as the window opens, a heavy strike as a counter lands
QVR_CVAR(vr_counter_glow, "0", CVAR_ARCHIVE)            // while the window is open, what your hands hold glows gold round its edges and sheds embers, fading as it closes (vr_meleehud.cpp)
QVR_CVAR(vr_counter_haptic, "1", CVAR_ARCHIVE)          // strength of the soft pulses in the hands while the window is open (0: off)
QVR_CVAR(vr_dummy_attacks, "0", CVAR_NONE)               // the training dummy strikes at you in melee now and then (parry practice; the firing range's button). Not saved: off at every map load, and while a motion take plays
QVR_CVAR(vr_dummy_attack_period, "2.5", CVAR_ARCHIVE)    // s from one of the dummy's blows to the next (its wind-up included)
QVR_CVAR(vr_dummy_attack_jitter, "0.4", CVAR_ARCHIVE)    // s the period varies by at random, either way
QVR_CVAR(vr_dummy_attack_reach, "80", CVAR_ARCHIVE)      // units (origin to origin) within which it strikes; beyond, it waits (a knight's: 60)
QVR_CVAR(vr_dummy_attack_damage, "10", CVAR_ARCHIVE)     // a blow's damage, before a parry (as a monster's blow: armour, god mode and the parry apply)
QVR_CVAR(vr_dummy_attack_windup, "0.6", CVAR_ARCHIVE)    // s of wind-up (the tell: a sound, the rifle raised, a glow) before each blow
QVR_CVAR(vr_dummy_gore, "1", CVAR_ARCHIVE)               // the training dummy bleeds as a grunt: gore sprays and mist, wounds on its model, small gibs, blood thrown onto you and your gear (0: clean, as before)
QVR_CVAR(vr_dummy_gib, "1", CVAR_ARCHIVE)                // Dummy Dies: what would kill a grunt (its 30 health over the run of hits) kills the training dummy as one (beheaded, head popped, dying, a ragdoll or gibbed as a grunt would be; no loot), and it stands again after vr_dummy_gib_respawn
QVR_CVAR(vr_dummy_gib_respawn, "2", CVAR_ARCHIVE)        // s a killed training dummy takes to stand again (its body stays, as a grunt's)
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
QVR_CVAR(vr_weapon_throw_damage_mult, "0.5", CVAR_ARCHIVE) // every throw's hit damage times this (weapons, props, boxes, gibs: VR_Thrown_Damage); 1 before 2026-10-02
QVR_CVAR(vr_weapon_throw_velocity_mult, "1.0", CVAR_ARCHIVE)
QVR_CVAR(vr_weapon_cycle_mode, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_melee_bloodlust, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_melee_bloodlust_mult, "0.5", CVAR_ARCHIVE)
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
QVR_CVAR(vr_grapple_reel_speed, "300", CVAR_ARCHIVE)     // units/s the reel pulls you in (to walls, huge monsters, props too heavy to move)
QVR_CVAR(vr_grapple_min_length, "40", CVAR_ARCHIVE)      // the shortest the reel takes the rope (units)
QVR_CVAR(vr_grapple_unreel_speed, "300", CVAR_ARCHIVE)   // units/s the hand's A/X pays the rope out at (lets you down, lets a load go farther); 0: no unreel
QVR_CVAR(vr_grapple_unreel_airborne, "1", CVAR_ARCHIVE)  // the unreel button (A/X) doesn't unreel as it jumps (A, by default: it unreels only in the air); X (the reload) unreels on the ground too; 0: anywhere (its key still pressed)
QVR_CVAR(vr_grapple_prop_speed, "300", CVAR_ARCHIVE)     // units/s a light prop comes in at (weapons, pickups, keys)
QVR_CVAR(vr_grapple_prop_light, "8", CVAR_ARCHIVE)       // kg up to which props come in at that speed; heavier, slower in proportion (40 kg: a fifth)
QVR_CVAR(vr_grapple_prop_anchor, "100", CVAR_ARCHIVE)    // kg from which a prop does not come: the reel pulls you to it
QVR_CVAR(vr_grapple_small_mass, "120", CVAR_ARCHIVE)     // kg (QC VR_Grapple_MonsterMass) up to which a monster is small: pulled fast, staggered
QVR_CVAR(vr_grapple_huge_mass, "400", CVAR_ARCHIVE)      // kg from which a monster is huge: the reel pulls you to it (in between: medium)
QVR_CVAR(vr_grapple_small_speed, "400", CVAR_ARCHIVE)    // units/s a small monster is reeled in at
QVR_CVAR(vr_grapple_medium_speed, "160", CVAR_ARCHIVE)   // and a medium one
QVR_CVAR(vr_grapple_stagger, "1", CVAR_ARCHIVE)          // small monsters reeled in are staggered (their pain, forced; again every second of reeling)
QVR_CVAR(vr_grapple_stamina, "0", CVAR_ARCHIVE)          // stamina a second reeling a monster in costs (the parry/shove/strike pool); short of it, it comes slower. 0: free
QVR_CVAR(vr_grapple_haptics, "1", CVAR_ARCHIVE)          // strength of the rope's haptics (the hook biting, the reel, the rope snapping taut); 0 none
QVR_CVAR(vr_grapple_quick_speed, "1800", CVAR_ARCHIVE)   // units/s the quick release (the button on the gun) brings the hook back at, with vr_grapple_quick_time 0 (and the fastest it goes with it: a long rope takes longer)
QVR_CVAR(vr_grapple_drop_grace, "0.12", CVAR_ARCHIVE)    // seconds the trigger let go waits before the hook comes off: dropping or passing the gun by then keeps it in
QVR_CVAR(vr_grapple_rope_sim, "1", CVAR_ARCHIVE)         // the rope is a physical thing (vr_ropesim.cpp): it hangs, lies and wraps round the world and the props, never through them, and pulls along where it lies; 0: a straight line (drawn sagging)
QVR_CVAR(vr_grapple_rope_spacing, "4", CVAR_ARCHIVE)     // units between the simulated rope's points (finer: truer round edges, more work; a long rope's are farther apart: 96 points at most)
QVR_CVAR(vr_grapple_rope_iterations, "32", CVAR_ARCHIVE) // passes a frame holding its pieces to their length (more: stiffer, truer, more work)
QVR_CVAR(vr_grapple_rope_radius, "4", CVAR_ARCHIVE)      // the simulated rope's thickness (units, half of it): how far from what it lies on
QVR_CVAR(vr_grapple_trigger_release, "0", CVAR_ARCHIVE)  // the trigger let go: 0 nothing (the hook stays in: the detach button on the gun takes it off); 1 it comes straight back (as the reel-in button); 2 it comes loose on its rope (reel it in with B/Y)
QVR_CVAR(vr_grapple_max_length, "1500", CVAR_ARCHIVE)    // units: the longest the rope is (the hook flies no farther; the unreel pays out no more)
QVR_CVAR(vr_grapple_loose_slack, "8", CVAR_ARCHIVE)      // units of slack the rope gets when the hook comes off (a loose hook lying there is not dragged by a step)
QVR_CVAR(vr_grapple_move_share, "0.7", CVAR_ARCHIVE)     // the share of your walking (the thumbstick) a load hanging on the rope takes (the loose hook, a hooked prop): it trails a little rather than lagging far behind and snapping forward; 0: none
QVR_CVAR(vr_grapple_load_drag, "1", CVAR_ARCHIVE)        // a second: how much of its speed a load on the rope loses to the air (its swing relative to you; a gun hanging on its own: all of it), so a swing dies down
QVR_CVAR(vr_grapple_hang_drag, "1", CVAR_ARCHIVE)        // a second: how much of its speed what hangs on a rope with no one holding it loses (a gun left hanging from a ceiling: it stops swinging in a couple of seconds)
QVR_CVAR(vr_grapple_load_max_speed, "500", CVAR_ARCHIVE) // units/s: the fastest a load on the rope moves relative to you (no rope of death); 0: no limit
QVR_CVAR(vr_grapple_shootable, "1", CVAR_ARCHIVE)        // shooting a hook that is in the world or a door (any weapon) takes it off
QVR_CVAR(vr_grapple_front_button, "1", CVAR_ARCHIVE)     // the grappling gun's front button (near the muzzle): the instant reel-in (the back one only detaches)
QVR_CVAR(vr_grapple_front_button_x, "6", CVAR_ARCHIVE)   // its place: from the back button along the gun (model units; +x towards the muzzle)
QVR_CVAR(vr_grapple_front_button_y, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_grapple_front_button_z, "0", CVAR_ARCHIVE)
QVR_CVAR(vr_grapple_rope_depth, "2", CVAR_ARCHIVE)       // units inside the grappling gun's muzzle the rope is drawn from (in a hand, and a gun lying about); -1: the old places (the muzzle's end; a gun lying about: where the server has it)
QVR_CVAR(vr_grapple_sag, "1", CVAR_ARCHIVE)              // a slack rope is drawn hanging (0: always straight)
QVR_CVAR(vr_grapple_rope_tail, "6", CVAR_ARCHIVE)        // units (times the hook's size) the drawn rope goes straight out of the hook's back before it bends (its last pieces point into the hook); 0: none
QVR_CVAR(vr_grapple_hook_scale, "0.6", CVAR_ARCHIVE)     // the size of the hook the grappling gun fires (its model; 1: Rogue's, too big for the gun), on the next shot
QVR_CVAR(vr_grapple_tow_light, "10", CVAR_ARCHIVE)       // kg up to which a hooked prop dragged along on a taut rope doesn't slow you; heavier, you drag it slower in proportion (40 kg, an explosive box: a quarter as fast)
QVR_CVAR(vr_grapple_tow_speed, "320", CVAR_ARCHIVE)      // units/s you can drag a light prop at on a taut rope (walking away from it); heavier, slower (vr_grapple_tow_light); 0: no limit (it comes at your speed)
QVR_CVAR(vr_grapple_debug, "0", CVAR_NONE)               // with developer 1: what the hook bites and each reel (mass, class, speeds); 2 also the rope four times a second; 3 every frame the rope holds you; 4 also every rope drawn, every frame (the client)
QVR_CVAR(vr_grapple_unreel_slack, "256", CVAR_ARCHIVE)   // units of slack the unreel (A/X) pays out beyond the load's distance when nothing pulls on the rope (you or the load on the ground): the rope comes off the drum and lies there; hanging, the load takes it out as ever. 4: the old unreel (nothing seen on the ground)
QVR_CVAR(vr_grapple_reel_home, "5", CVAR_ARCHIVE)        // units from the rope's end at the muzzle the loose hook's tail is reeled to before it goes back in (it was 9)
QVR_CVAR(vr_grapple_quick_time, "0.5", CVAR_ARCHIVE)     // seconds the front button's reel-in takes (the hook reeled in, speeding up then slowing down; a short rope quicker); 0: straight back at vr_grapple_quick_speed
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
QVR_CVAR(vr_melee_push, "0.5", CVAR_ARCHIVE)               // how far your melee blows push what they hit
QVR_CVAR(vr_melee_push_player, "0.6", CVAR_ARCHIVE)        // how far monsters' melee blows push you
QVR_CVAR(vr_parry_push_enemy, "1", CVAR_ARCHIVE)         // how far a parry pushes the attacker back
QVR_CVAR(vr_parry_push_player, "1", CVAR_ARCHIVE)        // and you
QVR_CVAR(vr_enemy_shove, "1", CVAR_ARCHIVE)              // grunts and enforcers shove you away when you stand too close (QC vr_enemyshove.qc)
QVR_CVAR(vr_enemy_shove_range, "50", CVAR_ARCHIVE)       // how close (units between your middles, flat; touching is 32)
QVR_CVAR(vr_enemy_shove_delay, "0.5", CVAR_ARCHIVE)      // seconds that close before it shoves (not while it fires, is staggered or knocked away)
QVR_CVAR(vr_enemy_shove_cooldown, "1.5", CVAR_ARCHIVE)     // seconds from one shove to its next (parried or not)
QVR_CVAR(vr_enemy_shove_damage, "5", CVAR_ARCHIVE)       // a shove's damage (0: none, and it can't be parried)
QVR_CVAR(vr_enemy_shove_distance, "128", CVAR_ARCHIVE)    // how far it pushes you (units, a slide; not scaled by vr_push)
QVR_CVAR(vr_enemy_shove_parry_reduction, "0.75", CVAR_ARCHIVE) // share of a shove's push a parry takes off (0: none, 1: all; was vr_enemy_shove_parried, the share left)
QVR_CVAR(vr_enemy_shove_enforcer_damage, "1.5", CVAR_ARCHIVE) // an enforcer's shove's damage, times vr_enemy_shove_damage
QVR_CVAR(vr_enemy_shove_enforcer_distance, "1.5", CVAR_ARCHIVE) // an enforcer's shove's push, times vr_enemy_shove_distance
QVR_CVAR(vr_push, "0.5", CVAR_ARCHIVE)                    // knockback: all of it (the others scale this)
QVR_CVAR(vr_hit_push, "1", CVAR_ARCHIVE)                 // heavy weapon hits shove monsters
QVR_CVAR(vr_kill_push, "1", CVAR_ARCHIVE)                // killing blows throw the bodies
QVR_CVAR(vr_enemy_liquid_damage, "1", CVAR_ARCHIVE)      // monsters burn in slime/lava and drown underwater (QC vr_liquids.qc)
QVR_CVAR(vr_knockdown_wiggle, "1", CVAR_ARCHIVE) // physical struggling of living knockdowns; 0 disables (0.7 before config 89)
QVR_CVAR(vr_knockdown_wiggle_frequency, "2.2", CVAR_ARCHIVE) // struggling frequency, Hz (1.2 before config 89)
QVR_CVAR(vr_knockdown_wiggle_pause, "0", CVAR_ARCHIVE) // pause between bursts, seconds; 0 continuous (the default since config 89; 1 before)
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
QVR_CVAR(vr_grenade_shoot, "1", CVAR_ARCHIVE)          // shots, blows and blasts set grenades off (live or a hand grenade's dud; the ogres', yours): the one who hit it gets the kills (QC vr_grenade.qc)
QVR_CVAR(vr_grenade_shoot_pad, "2", CVAR_ARCHIVE)     // units a grenade's shot box reaches beyond its model's half size (0: the model's box as it is)
QVR_CVAR(vr_grenade_shoot_melee, "1", CVAR_ARCHIVE)   // melee blows meet grenades: a very strong one sets it off (vr_grenade_melee_*), any other bats it (one in flight of a monster's: a weapon bats, an empty hand catches)
QVR_CVAR(vr_grenade_melee_speed, "20", CVAR_ARCHIVE)  // m/s a punch, a gun's swing, a pommel or a butt must go at to set a grenade off (slower: it bats it)
QVR_CVAR(vr_grenade_melee_speed_weapon, "34", CVAR_ARCHIVE) // m/s for a blade's or a head's blow (a sword, the axe, the crowbar, a club, a stopped chainsaw: swung faster); a running chainsaw always sets it off
QVR_CVAR(vr_grenade_melee_damage, "10", CVAR_ARCHIVE) // and the least damage the blow must deal (0: the speed alone)
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
QVR_CVAR(vr_hand_fit_thumb_wide, "1", CVAR_ARCHIVE)   // with it: a thumb in the In-the-Palm prop open at every usual turn (a hand grenade's side) opens wider, swung up to 60 degrees away from the index, and wraps round its outside instead of lying sunk in it (0: it lies along it, sunk in)
QVR_CVAR(vr_hand_fit_thumb_outside, "1", CVAR_ARCHIVE) // cm: a prop held In the Palm (a rock, a brick, a grenade) has the thumb round its outside, its base sunk into it as far as this, never tucked between the palm and it (0: as any held thing's)
QVR_CVAR(vr_hand_fit_palm_weapon, "3", CVAR_ARCHIVE)   // the same on a weapon (its own grip, a foregrip, a blade): cm (the weapon is placed in the hand by its settings)
QVR_CVAR(vr_hand_fit_palm_turn, "20", CVAR_ARCHIVE)    // and degrees it may turn to face the surface it holds (0: not at all)
QVR_CVAR(vr_hand_fit_overlap, "0.3", CVAR_ARCHIVE)     // round 21, second pass: cm the fingers and palm may sink into what they hold (snug, no gap); third pass: things only (a weapon, its hotspots: their own overlap sliders)
QVR_CVAR(vr_hand_fit_overlap_hands, "0.6", CVAR_ARCHIVE) // and cm a hand cupping the other (a two-handed pistol grip) may sink into it
QVR_CVAR(vr_hand_walls, "1", CVAR_ARCHIVE)            // round 21, second pass: the jointed hand's fingers bend (curl) where they would go into walls and floors
QVR_CVAR(vr_model_collide, "2", CVAR_ARCHIVE)         // round 21: held weapons stop at the models' drawn triangles (drawn only: the game reads the tracked hands; vr_modelcollide.cpp): 0 off, 1 monsters (corpses, other players), 2 things on the ground too
QVR_CVAR(vr_model_collide_hands, "1", CVAR_ARCHIVE)   // and so do empty hands (fists)
QVR_CVAR(vr_model_collide_rest, "1", CVAR_ARCHIVE)    // a thing lying on a hand (held up by it: an open palm, a fist, a gun) never pushes that hand away (the physics keeps them apart); 0: it does, as other things
QVR_CVAR(vr_model_collide_max, "20", CVAR_ARCHIVE)    // cm a weapon or hand is held out of a model; deeper, less and less, and none at twice as deep (it lets go and passes through)
QVR_CVAR(vr_debug_model_collide, "0", CVAR_NONE)      // 1: print each hand's push while a model stops it; 2: and draw its rays (grey as tracked, green/red as drawn) and the push (yellow)
QVR_CVAR(vr_gun_wall_slide, "1", CVAR_ARCHIVE)       // a held weapon against the level: 1 slides along what it meets (rests on a table top, held off a wall along its surface: vr_handpose.cpp); 0 the old push-back along the aim (the hand moved back until the muzzle stops at the wall)
QVR_CVAR(vr_gun_wall_max, "40", CVAR_ARCHIVE)        // with vr_gun_wall_slide: cm, the most a held weapon and its hand are held out of the level (pushed further, they go in by the rest; the weapon keeps resting where it was held)
QVR_CVAR(vr_debug_gun_wall, "0", CVAR_NONE)          // 1: print each frame a held weapon is held out of the level: the hand moved (up, across), its depth left, the muzzle over the surface below; 2: every frame (tests)
QVR_CVAR(vr_debug_hand_offset, "0", CVAR_NONE)       // 1: print each hand drawn away from where it is tracked (0.01 units or more), and by what: the walls, the weight's spring, the models, a prop in the other hand, the body; 2: every frame (tests)
QVR_CVAR(vr_test_spawn, "0", CVAR_NONE)               // impulse 241 (tests): what it puts ahead of you: 0..17 the firing range's dispenser monsters (0 grunt, 1 ogre, 2 zombie, 3 shambler, ...; 18 the Guardian, 19 the dragon, 20 the marksman ogre: tests only), 100 a health box, 101 a shells box, 102/103 an explosive box/a small one, 104 an explosive box that never blows up, 110 the rocks and bricks (a row of each)
QVR_CVAR(vr_test_spawn_dist, "56", CVAR_NONE)         // and how far ahead (units)
QVR_CVAR(vr_test_pile_count, "500", CVAR_NONE)       // Debug > Tests > Physics Stress: the props a pile of rocks, bricks or both has (vr_physics_bigpile without a count)
QVR_CVAR(vr_test_pile_crates, "80", CVAR_NONE)       // and the crates its Wall of Crates has (vr_physics_bigpile crates)
QVR_CVAR(vr_test_spawn_hold, "0", CVAR_NONE)          // 1: a box or a crate (vr_test_spawn 100..104, 107..109) put into the empty main hand (QC weapons.qc)
QVR_CVAR(vr_test_spawn_dead, "0", CVAR_NONE)          // 1: a monster killed at once (a corpse); 2: killed hard enough to gib (QC weapons.qc: 100 damage past its health)
QVR_CVAR(vr_test_spawn_yaw, "0", CVAR_NONE)           // 100..104: a box let loose (a physics prop), turned this many degrees about its upright (grappling hook tests: a turned box's empty corners)
QVR_CVAR(vr_test_spawn_tilt, "0", CVAR_NONE)          // ... and tipped this many degrees about the way you face, its lowest corner on the floor (it topples: sv_gravity 0 holds it so)
QVR_CVAR(vr_test_axe, "0", CVAR_NONE)               // impulse 209 (tests): an axe thrown ahead from the eyes: 0 blade first, 1 flat, 2 handle first, 3 spinning end over end; a hand's throw at random: 4 overhand, 5 sidearm, 6 sloppy, 8 upright, 9 knife-style, 10 spear-like, 11 flat, 12 handle first; 7 as the next set
QVR_CVAR(vr_test_axe_roll, "0", CVAR_NONE)          // ... 7: from blade first, degrees turned about the way it goes (90: the blade level)
QVR_CVAR(vr_test_axe_pitch, "0", CVAR_NONE)         // ... then about your left (its head forward)
QVR_CVAR(vr_test_axe_yaw, "0", CVAR_NONE)           // ... then about the upright
QVR_CVAR(vr_test_axe_spin_f, "0", CVAR_NONE)        // ... 7: its spin, rad/s about the way it goes
QVR_CVAR(vr_test_axe_spin_l, "0", CVAR_NONE)        // ... about your left (end over end, its top going ahead)
QVR_CVAR(vr_test_axe_spin_u, "0", CVAR_NONE)        // ... about the upright
QVR_CVAR(vr_test_axe_side, "0", CVAR_NONE)          // ... from this many units to your left
QVR_CVAR(vr_test_axe_loft, "1", CVAR_NONE)          // ... 4-7: aimed up to meet what is ahead level with your eyes
QVR_CVAR(vr_test_axe_what, "0", CVAR_NONE)          // ... 1: a gib, 2 an explosive box (never blowing up) thrown the same way instead (the spin in the air's tests)
QVR_CVAR(vr_test_axe_speed, "10", CVAR_NONE)        // ... at this many m/s
QVR_CVAR(vr_test_axe_damage, "1", CVAR_NONE)        // ... hurting what it hits (0: no damage, to see a monster bleed)
QVR_CVAR(vr_test_axe_weapon, "0", CVAR_NONE)        // ... this weapon (QC WID_*: 7 the super nailgun, 4 the shotgun) thrown instead of the axe (0: the axe)
QVR_CVAR(vr_test_axe_dist, "0", CVAR_NONE)          // ... you moved first to this many units from what is ahead of your eyes (0: where you are)
QVR_CVAR(vr_test_axe_up, "0", CVAR_NONE)            // ... at a target: this many units above its middle (15: a grunt's head; the decapitation tests)
QVR_CVAR(vr_test_axe_at, "0", CVAR_NONE)            // ... at the nearest live monster (1), door (2), loose prop (3) or wall button (4), from its side facing you, level with its middle
QVR_CVAR(vr_test_weaponinst, "0", CVAR_NONE)          // impulse 120 (tests): the weapons moved by the game's own functions (QC vr_weaponinst_test.qc): 0 list what has which weapon (id, magazine), 1 holster the main hand's, 2 draw it, 3 drop the main hand's, 4 take the nearest lying, 5 hand off to the off hand, 6 take back, 7 switch hands, 8 back, 9 drop the off hand's
QVR_CVAR(vr_test_weaponinst_slot, "0", CVAR_NONE)     // ... the holster (QC QVR_HS_*: 3-6 shoulders and hips, 8-9 upper; 0 the right hip)
QVR_CVAR(vr_test_held_destroy, "0", CVAR_NONE)       // impulse 252 (tests): its gib or head can be destroyed, as a real one (VR_Gib_MakeDestroyable; gib_2h_throw_test.sh)
QVR_CVAR(vr_test_held_pick, "-1", CVAR_NONE)          // impulse 252 (tests): which gib or head (0 gib1, 1 h_player, 2 gib2, 3 gib3, 4-8 heads; 9 the nearest rock, 10 a hand grenade, 11 the nearest shells box, 12 the nearest brick); -1 each gib in turn (gib_2h_throw_test.sh MODELS)
QVR_CVAR(vr_test_fling_speed, "12", CVAR_NONE)        // impulse 232 (tests): the prop nearest you sent at the nearest monster at this many m/s (VR_Test_Fling)
QVR_CVAR(vr_test_fling_at, "0", CVAR_NONE)            // and at: 0 the nearest monster, 1 you (from ahead)
QVR_CVAR(vr_test_fling_away, "0", CVAR_NONE)          // 1: from inside the edge of its box, away from it (a prop batted away, still in your box)
QVR_CVAR(vr_test_projectile, "0", CVAR_NONE)          // impulse 246 (tests): the projectile fired at you: 0 a knight's spike, 1 a laser, 2 a scrag's spit, 3 a vore's ball, 4 an ogre's grenade, 5 a zombie's flesh (6: impulse 240's ogre throws a multi-grenade)
QVR_CVAR(vr_test_walltorch_shot, "-1", CVAR_NONE)      // Debug > Tests: the wall torch nearest you, shot off its wall: 10 a shotgun pellet, 1 a nail, 0 a rocket, 2 a grenade, 3 a super nail, 4 a laser, 20 the loose prop nearest you sent at it at vr_test_fling_speed m/s; -1 nothing (it sets itself back)
QVR_CVAR(vr_test_grenade_shot, "10", CVAR_NONE)       // impulse 210 (tests): what shoots the nearest grenade from your eyes (vr_physics_fire's kinds: 10 a shotgun pellet, 1 a nail, 0 a rocket, 4 a laser)
QVR_CVAR(vr_test_grenade_dist, "256", CVAR_NONE)      // impulse 211 (tests): units ahead a dud hand grenade is put on the floor
QVR_CVAR(vr_test_grenade_height, "0", CVAR_NONE)      // ... and this many units over the floor (in the air: it falls; at hand height, to punch or swing at)
QVR_CVAR(vr_test_grenade_yours, "1", CVAR_NONE)       // ... yours (1: your blows pass it, as your own grenades), or nobody's (0: as an enemy's lying about, your blows meet it)
QVR_CVAR(vr_test_throw_up_body, "0", CVAR_NONE)      // impulse 222 (tests): 1: let go at your chest, inside your box (a gentle release at the body), not over your head
QVR_CVAR(vr_test_throw_up_speed, "10", CVAR_NONE)     // impulse 222 (tests): the loose prop nearest you thrown straight up as yours from over your head at this many m/s
QVR_CVAR(vr_test_projectile_side, "0", CVAR_NONE)     // and from how many degrees to your left of ahead
QVR_CVAR(vr_hand_collide, "5", CVAR_ARCHIVE)          // cm a free hand and what the other hand holds (its weapon, or the prop it holds alone) are each drawn moved back at most as the hand meets its surface (vr_hand_collide_props_margin off it), by half how deep it would be, as a prop held there and that thing (vr_held_collide_max); pressed further, the hand sinks in by the rest (0: it always passes through)
QVR_CVAR(vr_hand_collide_fingers, "1", CVAR_ARCHIVE)  // with it: the free hand's fingers react to the other hand's weapon: they rest on it or bend out of it (0: only the hand is pushed, by its fingertips too)
QVR_CVAR(vr_hand_collide_props_margin, "1", CVAR_ARCHIVE) // cm off the surface of the prop or the weapon the other hand holds the free hand's palm and knuckles stop (0..2: the drawn skin about at it)
QVR_CVAR(vr_body_collide, "1", CVAR_ARCHIVE)         // round 21: the drawn hands and held weapons stop at each other, the other arm, the wrist gadget and the body (drawn only: the game reads the tracked hands; vr_selfcollide.cpp); 0 off
QVR_CVAR(vr_body_collide_pass, "0.7", CVAR_ARCHIVE)  // pushed this share of the way through what stops it (the whole hand or weapon), it lets go and passes through (1: only once all the way through; held out at most this times 15 cm)
QVR_CVAR(vr_body_collide_elbows, "1", CVAR_ARCHIVE)  // with it: the elbows swing out of the torso rather than through it (the arm's IK)
QVR_CVAR(vr_debug_body_collide, "0", CVAR_NONE)      // 1: print each hand's drawn offset and its contacts every frame one is on; 2: and draw the proxies (capsules) and the contacts' pushes
QVR_CVAR(vr_hand_fit_blend, "0.12", CVAR_ARCHIVE)      // seconds the fingers, thumb and palm take to move to a new grasp (0: at once)
QVR_CVAR(vr_hand_fit_resolve, "0.3", CVAR_ARCHIVE)     // cm (and that many times 2 degrees) what the hand holds may move in it before the grasp is solved again (0: every frame)
QVR_CVAR(vr_debug_torch_lights, "0", CVAR_NONE)       // print each torch light lit this frame: which (a wall torch, a taken one), where, its radius and colour, shadowed or not
QVR_CVAR(vr_debug_grasp, "0", CVAR_NONE)               // print each grasp solve (1), and each finger's stops (2)
QVR_CVAR(vr_debug_grasp_trace, "0", CVAR_NONE)         // write the drawn hand's joints every frame to grasp_trace.txt in the game folder: 1 the main hand, 2 the off hand, 3 both
QVR_CVAR(vr_jobs_threads, "0", CVAR_NONE)             // the game's thread pool (vr_jobs.hpp): its workers (0: the hardware's threads less one, at most 31); changed, the pool is made again
QVR_CVAR(vr_jobs_parallel, "1", CVAR_NONE)            // parallelFor (the grasp solve, the liquids' volume, the occlusion bakes) split between the pool's threads; 0: the calling thread runs every chunk (the single-threaded reference: the same results)
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

// Exact uncached-reference checks for prop optimization regression runs; never archived.
QVR_CVAR(vr_prop_query_verify, "0", CVAR_NONE)

// The external map index (Quake/vr/vr_mapindex.cpp): Quaddicted's package index, fetched at start-up on a thread of
// its own and cached in the game dir (quakevr/cache/maps_index.txt). No UI page yet: maps_list / maps_info /
// maps_stats in the console, and Debug > Tools.
QVR_CVAR(vr_maps_fetch, "1", CVAR_ARCHIVE)              // the index is fetched at start-up (0: never; a cached copy is still read). -nomapindex turns it off for one run
QVR_CVAR(vr_maps_index_url, "https://www.quaddicted.com/api/v1/", CVAR_ARCHIVE) // where the index comes from (a paged API: ?q=*:*&rows=200&start=N). Another URL re-fetches: a cached copy of a different one is not used
QVR_CVAR(vr_maps_allow_progs, "0", CVAR_ARCHIVE)       // packages that carry their own progs.dat: 0 left out of the list (they replace the game's code), 1 included. They are in the index either way (maps_info shows them; maps_stats counts them)
QVR_CVAR(vr_maps_cache_mb, "512", CVAR_ARCHIVE)        // the map download cache (cache/maps/<sha256>.zip): at most this many MB kept, the oldest zips removed first when a download would pass it (and at start-up, and when it is lowered). 0: nothing kept once a package is installed

// Mission-pack installation state, derived from the owned data (vr_gamedir.cpp).
QVR_CVAR(vr_hipnotic_available, "0", CVAR_ROM)
QVR_CVAR(vr_rogue_available, "0", CVAR_ROM)
QVR_CVAR(vr_hipnotic_status, "0", CVAR_ROM)
QVR_CVAR(vr_rogue_status, "0", CVAR_ROM)

// Official native campaign ABI: id1, hipnotic, rogue, dopa, mg1, mg3 (0..5).
QVR_CVAR(vr_campaign, "0", CVAR_ROM)
QVR_CVAR(vr_campaign_schema, "0", CVAR_ROM)
QVR_CVAR(vr_dopa_status, "0", CVAR_ROM)
QVR_CVAR(vr_mg1_status, "0", CVAR_ROM)
QVR_CVAR(vr_mg3_status, "0", CVAR_ROM)
QVR_CVAR(vr_honey_context, "0", CVAR_ROM) // actual start map source is Honey, never inferred from its name

QVR_CVAR(vr_mg_world_test, "0", CVAR_NONE) // Debug > Tests: 1 report authored native fog; 2 environment acceptance with a destructive e5m1 barrel test

QVR_CVAR(vr_mg_trigger_test, "0", CVAR_NONE) // Debug > Tests: 1 shared triggers, 2 authored mge5m2 route, 3 lightning damage, 4 slipgate retarget

QVR_CVAR(vr_mg_horde_test, "0", CVAR_NONE) // Debug > Tests: authored Horde waves, currency, pickups, revival and save reports
QVR_CVAR(vr_mg_hub_test, "0", CVAR_NONE) // Debug > Tests: 1 MG1 hub/rune semantics, 2 authored electrode/egg route, 3 progression/inventory report; destructive tests reload afterward
QVR_CVAR(vr_mg_hub_stage, "0", CVAR_NONE) // Private QC transition acceptance continuation index; not archived
