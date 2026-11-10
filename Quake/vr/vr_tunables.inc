// vr_tunables.inc -- Quake VR's tuning constants: QVR_TUNABLE(name, default) (vr_cvars.hpp, "Tuning constants").
// Numbers tuned once that no player sets: no menu row of their own, not in the config. Read as name.value, name.string
// (or by name from the QC). Tuned live in a development build (tune <name> <value>, Debug > Tunables, tunables.cfg;
// vr_tunables.cpp), constexpr in a release (QVR_RELEASE). Each has its Tunables page row in vr_tunables_rows.inc (same
// order; the build checks). Misc/quakevr/tune_promote.py writes a tunables.cfg's values here as the new defaults. A
// player setting is a QVR_CVAR in vr_cvars.inc; to make one of these a setting again, move its line back there and give
// it a menu row. The list per page and why: ROUND23.md, "Settings tiers".

// Weapons > Other
QVR_TUNABLE(vr_compat_muzzle, "1") // with another mod's progs, its shots start at the gun, not the player's head
QVR_TUNABLE(vr_2h_spread_reduction, "0.5")
QVR_TUNABLE(vr_2h_cup_stock, "1")
QVR_TUNABLE(vr_weaponstudio_fly_speed, "120") // units a second: the Weapon Studio's camera flown out of his eyes while its window is open (W A S D, Q E; Shift 3x; its orbit and wheel reuse vr_rigstudio_orbit_speed and vr_rigstudio_zoom_speed)

// Carrying and Throwing > Other
QVR_TUNABLE(vr_throw_peak_span, "0.017")
QVR_TUNABLE(vr_throw_ang_threshold, "6")
QVR_TUNABLE(vr_throw_ang_factor, "0.7")
QVR_TUNABLE(vr_angvel_local_pitch, "-57.5")
QVR_TUNABLE(vr_throw_release_drop, "0.3")
QVR_TUNABLE(vr_throw_release_floor, "0.35")
QVR_TUNABLE(vr_throw_release_speed, "1.5")
QVR_TUNABLE(vr_throw_gain_lo, "1.5")
QVR_TUNABLE(vr_throw_gain_hi, "6")
QVR_TUNABLE(vr_throw_weight_influence, "0.25")
QVR_TUNABLE(vr_throw_assist_full, "4")
QVR_TUNABLE(vr_throw_assist_range, "1200")
QVR_TUNABLE(vr_throw_assist_speed, "0.15")
QVR_TUNABLE(vr_box3d_substeps, "4") // Box3D's sub-steps a server frame (1..8)
QVR_TUNABLE(vr_box3d_player_push, "1") // the players' bodies push props (a capsule each, one way)
QVR_TUNABLE(vr_box3d_hand_push_speed, "3") // m/s, the most a hand shoves a solid prop at (the rest of its move it passes into it, eased out)
QVR_TUNABLE(vr_box3d_player_push_speed, "2.5") // m/s, the most a player's body shoves a prop at (the rest of its move it passes into it, eased out)

// Sound > Other
QVR_TUNABLE(vr_snd_hrtf_sofa, "") // an HRTF of your own: a .sofa file in the game folder (empty: Steam Audio's)
QVR_TUNABLE(vr_snd_limiter_release, "0.15") // s, the limiter's gain back up after a peak (0.02-1)

// Motion Recorder > Other
QVR_TUNABLE(vr_obs_host, "127.0.0.1") // obs-websocket's address (OBS on another PC: its address)

// Combat > Other
QVR_TUNABLE(vr_bullettime_fx_tint, "0.85 0.95 1.15") // ... the colour it multiplies by (red green blue)

// Headset > Other
QVR_TUNABLE(vr_xr_res_warn, "1.3")
QVR_TUNABLE(vr_xr_res_warn_mpx, "6")

// Graphics > Other
QVR_TUNABLE(vr_upscale_radius, "40") // ... only within this many degrees of the lens centre, bilinear beyond (0: the whole image)
QVR_TUNABLE(vr_upscale_sharpen_native, "0") // at vr_render_scale 1: FSR's RCAS sharpening alone (with vr_upscale 1 or 2 and some sharpness)
QVR_TUNABLE(vr_foveated_inner, "0") // ... full-rate shading within this many degrees of the lens centre (0: the preset's: 45, 35, 25)
QVR_TUNABLE(vr_foveated_outer, "0") // ... 2x2 up to this many degrees, 4x4 beyond (0: the preset's: 60, 50, 40)
QVR_TUNABLE(vr_shadow_precision, "1") // texels per unit of a light's radius, close by
QVR_TUNABLE(vr_extmaps_green, "0") // ... their normal maps' green: 0 turned to ours where their heights (alpha) say it runs the other way (Quetoo's does), 1 as it is, 2 turned (next map)
QVR_TUNABLE(vr_mirror_unfocused_hz, "4")

// HUD and Menus > Doom on the Wrist Gadget
QVR_TUNABLE(vr_touch_depth, "4") // ... or this far through it
QVR_TUNABLE(vr_touch_hover, "4") // in front within this: hovering (the widget under it lights up, a cursor shows)
QVR_TUNABLE(vr_touch_margin, "0.5") // a touch counts this far past the screen's edges (on the edge)
QVR_TUNABLE(vr_touch_debounce, "0.06") // s: a new touch no sooner after the last ended
QVR_TUNABLE(vr_doom_args, "") // more of Doom's command line (-file my.wad, -skill 4, -warp 1 1, -fast)
QVR_TUNABLE(vr_doom_attenuation, "2") // how fast they fade with distance (Quake's: 1 normal, 3 a small source; 0 everywhere)
QVR_TUNABLE(vr_doom_focus_angle, "25") // degrees off the view's middle the screen may be and count as looked at
QVR_TUNABLE(vr_doom_panel_distance, "0.6") // m: how far in front it is placed

// Motion Recorder > Other
QVR_TUNABLE(vr_note_replay, "1") // with vr_notes on: the last seconds kept (poses, buttons, the player, nearby entities) and saved beside each note as a motion take (vr_notectx.cpp, vr_motion.cpp)
QVR_TUNABLE(vr_note_replay_seconds, "30") // how many seconds that instant replay keeps (and the console lines and frame times saved with a note)

// Weapons > Other
QVR_TUNABLE(vr_heavy_auto_from, "10") // kg: a weapon without its own Heft (-1) lighter than this is light (0)
QVR_TUNABLE(vr_heavy_auto_full, "20") // kg: and this heavy the heaviest (1)

// Body > Other
QVR_TUNABLE(vr_ghost_hands_solid, "1") // 1 each ghost hand is one solid translucent object (its nearest surface only), 0 every face blended
QVR_TUNABLE(vr_seated_crouch_ease, "0.2") // ... seconds it eases over

// World > Other
QVR_TUNABLE(vr_debris_area_size, "384") // units: the side of an area
QVR_TUNABLE(vr_debris_exclude, "vrfiringrange vrclimb vrexample") // maps with none (names, spaces between)
QVR_TUNABLE(vr_debris_edicts_left, "2048") // entities left free for the game at least (max_edicts less the map's): fewer pieces if needed
QVR_TUNABLE(vr_crates_exclude, "vrfiringrange vrclimb vrexample vrcalibration") // maps with none (names, spaces between)

// Body > Other
QVR_TUNABLE(vr_bodytrack_fade, "0.25") // seconds a region's tracking eases in or out (lost, low confidence, switched)

// Graphics > Other
QVR_TUNABLE(vr_water_splash_speed, "150") // units/s a thing (not a player) must move at to splash entering or leaving water
QVR_TUNABLE(vr_shadow_head, "1") // your shadow (vr_shadow_self) has your head, which the eyes never see (not for a light at the head: the head-mounted flashlight)

// Gore > Other
QVR_TUNABLE(vr_decal_melee_prop_speed, "7") // ... a prop (crate, rock, brick, weapon) dents the level hitting it this fast or faster (m/s: a hard throw; 0: never)
QVR_TUNABLE(vr_gore_streams_pools, "4") // most pools one stream spreads (a new one each time it moves on)
QVR_TUNABLE(vr_teeth_speed, "3") // m/s they fly out at (along the blow, scattering)
QVR_TUNABLE(vr_teeth_mass, "0.05") // kg each
QVR_TUNABLE(vr_smallgibs_brains_jiggle_speed, "0.8") // m/s: a change of their speed faster than this between two frames kicks it (less: wobblier)

// Weapons > Other
QVR_TUNABLE(vr_weapon_relief, "1") // weapons made from other models as they load (AD's #vr, #gun, #melee; our rifle's #adv variants): 1 their relief made from the skin like our baked weapons' (vr_skinrelief.cpp, drawn as authored), 0 the plain made map; on their next load (a restart)

// Graphics > Other
QVR_TUNABLE(vr_parallax_depth_distance, "0") // round 22: how far ahead (view depth, units) the depth write reaches; beyond, the surfaces' flat depth (0, the default: as far as the parallax, vr_parallax_distance, the same picture)
QVR_TUNABLE(vr_detail_fine, "1") // ... a second, finer grain within half that distance (one more texture read there)
QVR_TUNABLE(vr_taa_blend, "0.1") // the current frame's weight in the history (lower: smoother, more ghosting)
QVR_TUNABLE(vr_taa_clip, "1") // the history's variance clip, in standard deviations of the 3x3 (lower: less ghosting, more flicker)
QVR_TUNABLE(vr_taa_reactive, "0.6") // the current frame's weight on translucency, particles and liquids (no motion vectors)
QVR_TUNABLE(vr_water_geo_cell, "16") // ... the grid's cell in units (smaller: smoother, more triangles; remade on change)

// Weapons > Other
QVR_TUNABLE(vr_flaregun_cooldown, "0.3") // s at least from a shot to the next (each a new pull)
QVR_TUNABLE(vr_flaregun_volume, "1") // its shot's volume (0..4)
QVR_TUNABLE(vr_nailpistol_volume, "1") // its shot's volume (0..4)
QVR_TUNABLE(vr_nailpistol_smoke, "1") // light smoke out of its side vents with each shot (vr_gunfx.cpp; 0 none, up to 3x; longer after a quick string)
QVR_TUNABLE(vr_revolver_volume, "1") // its shot's volume (0..4)
QVR_TUNABLE(vr_revolver_smoke, "1") // a wisp of smoke from its muzzle after a shot (0 none)

// Combat > Other
QVR_TUNABLE(vr_agarms_shoulder, "0.5") // vr_agarms: a long gun aimed is raised from its rest pose's hold (0: as it held its own rifle, its wrists as made) to its shoulder (1: the butt in its pocket, at its cheek) by this
QVR_TUNABLE(vr_agarms_clavicle, "0.6") // vr_agarms: the shoulder joint carried by a clavicle that takes this share of the arm's lift above level and of its reach forward and across (the shoulder shrugs and comes forward; its top turns with it); 0 none (before)
QVR_TUNABLE(vr_agarms_clavicle_most, "25") // vr_agarms: ... the clavicle turns at most this much up and this much forward (degrees)
QVR_TUNABLE(vr_agarms_shoulder_cone, "105") // vr_agarms_limits: the upper arm within this many degrees of the middle of its range (forward, out and a little down from the shoulder)
QVR_TUNABLE(vr_agarms_shoulder_twist, "80") // vr_agarms_limits: ... and twisted about itself at most this many degrees either way from its rest pose's
QVR_TUNABLE(vr_agrunt_second_reach, "1") // vr_agarms: 1 a long gun carried or aimed is brought in towards the off shoulder until its second grip is within the off arm's reach (as a chainsaw's front handle is: the laser cannon's); 0 where its rest pose's rifle is (before: the off hand slid back to the main handle or let go)
QVR_TUNABLE(vr_agrunt_atk_stand, "1") // armed humanoids whose own aiming frames crouch and lean back (the enforcer's: a rifle at the hip) stand as they aim and fire, their posed arms holding the weapon (with vr_agarms); 0 their aiming frames (before: the enforcer's belly pushed forward, its legs bent)
QVR_TUNABLE(vr_posestudio_reach, "2.5") // the Pose Studio (Grip Editor > Drag Mode): the armed humanoid nearest within this many metres of your head is the one shown and edited

// Combat > Arcane Dimensions' Guns
QVR_TUNABLE(vr_adgun_tank_y, "0") // ... right

// Carrying and Throwing > Other
QVR_TUNABLE(vr_carry_two_hands_detach, "3") // with it: cm past the drift a hand may be off its grip before it lets go (its fist off the prop: pulled away, not sliding on it); both off: it drops (vr_carry2h.cpp detached)
QVR_TUNABLE(vr_held_fit_gaps, "") // ... and per model, added: "name=cm ...", a name matching the end of the model's (e.g. "backpack.mdl=0.5 b_bh25.bsp=-0.3")

// Combat > Other
QVR_TUNABLE(vr_hit_stagger_throw_ref, "60") // kg m/s: a thrown prop's or weapon's momentum (mass x speed into it) that makes impact 1 (a 6 kg crate at 10 m/s)
QVR_TUNABLE(vr_hit_stagger_melee_ref, "30") // a melee blow's damage (its zone's multiplier in) that makes impact 1
QVR_TUNABLE(vr_hit_stagger_damage, "1") // impact plus this x the hit's damage as a share of its full health
QVR_TUNABLE(vr_hit_stagger_push, "1") // its push, times a shove's (impact 1: a two-hand shove's; 0.5 .. 1.25)

// Carrying and Throwing > Other
QVR_TUNABLE(vr_ragimpact_body, "0.1") // share of the rest of the body's mass behind the struck part (0: the part alone)
QVR_TUNABLE(vr_ragimpact_whip, "2") // a part's speed counts up to this times its whole body's (a limb whipping on a throw is no blow), unless a hand holds it
QVR_TUNABLE(vr_ragimpact_held_mass, "20") // kg of your arm behind a part your hand holds (a head held and slammed: the hand drives it)
QVR_TUNABLE(vr_ragimpact_time, "2") // s: a body (alive or dead) you hold, or let go of (threw) this long ago, takes impacts (0: only while held)
QVR_TUNABLE(vr_ragimpact_crush_mass, "50") // kg of you counted behind your feet

// Gore > Other
QVR_TUNABLE(vr_decap_pop_projectile, "0.5") // any other projectile's kill at the head or a limb (a nail, a spike, AD's crossbow bolt, a lump of flesh; every game's: QC VR_Proj_Begin) pops it at this chance, as a laser bolt's (0: never; explosives pop by their blast, vr_limbs_blast_chance)

// Combat > Other
QVR_TUNABLE(vr_parry_muzzle_angle, "45") // degrees: a weapon whose far end (a gun's muzzle, a blade's tip) points within this of the attacker doesn't parry

// Carrying and Throwing > Other
QVR_TUNABLE(vr_forcegrab_miss_speed, "0.2") // share of its speed a missed object keeps
QVR_TUNABLE(vr_forcegrabbable_ammo_boxes, "1")
QVR_TUNABLE(vr_forcegrabbable_health_boxes, "1")
QVR_TUNABLE(vr_forcegrabbable_return_time_deathmatch, "4")
QVR_TUNABLE(vr_forcegrabbable_return_time_singleplayer, "0")

// Weapons > Fingers and Collisions
QVR_TUNABLE(vr_finger_blending_speed, "50")

// Weapons > Reloading
QVR_TUNABLE(vr_spinreload_pitch_speed, "1100")
QVR_TUNABLE(vr_spinreload_x_angular_threshold, "6.5")

// Movement > Other
QVR_TUNABLE(vr_player_stepsize, "18.0")

// Weapons > Fingers and Collisions
QVR_TUNABLE(vr_fingers_and_base_x, "1.925")
QVR_TUNABLE(vr_fingers_and_base_y, "-2.825")
QVR_TUNABLE(vr_fingers_and_base_z, "-2.075")
QVR_TUNABLE(vr_fingers_and_base_offhand_x, "0.0")
QVR_TUNABLE(vr_fingers_and_base_offhand_y, "0.0")
QVR_TUNABLE(vr_fingers_and_base_offhand_z, "0.0")
QVR_TUNABLE(vr_fingers_x, "-5.05")
QVR_TUNABLE(vr_fingers_y, "-0.1")
QVR_TUNABLE(vr_fingers_z, "-0.1875")
QVR_TUNABLE(vr_finger_thumb_x, "-0.3625")
QVR_TUNABLE(vr_finger_thumb_y, "3.2625")
QVR_TUNABLE(vr_finger_thumb_z, "-1.9375")
QVR_TUNABLE(vr_finger_index_x, "-0.325")
QVR_TUNABLE(vr_finger_index_y, "0.6125")
QVR_TUNABLE(vr_finger_index_z, "-1.825")
QVR_TUNABLE(vr_finger_middle_x, "-0.3625")
QVR_TUNABLE(vr_finger_middle_y, "0.5125")
QVR_TUNABLE(vr_finger_middle_z, "-0.3125")
QVR_TUNABLE(vr_finger_ring_x, "-0.3625")
QVR_TUNABLE(vr_finger_ring_y, "0.7")
QVR_TUNABLE(vr_finger_ring_z, "0.65")
QVR_TUNABLE(vr_finger_pinky_x, "-0.325")
QVR_TUNABLE(vr_finger_pinky_y, "1.15")
QVR_TUNABLE(vr_finger_pinky_z, "1.6375")
QVR_TUNABLE(vr_finger_base_x, "0.0")
QVR_TUNABLE(vr_finger_base_y, "0.0")
QVR_TUNABLE(vr_finger_base_z, "0.0")

// Play > Other
QVR_TUNABLE(vr_maps_fetch, "1") // the index is fetched at start-up (0: never; a cached copy is still read). -nomapindex turns it off for one run

// Graphics > Other
QVR_TUNABLE(vr_brush_max, "256") // how many a map may have at once (its slots in the brush buffers, made at its load: 52 bytes x 64 corners each)
QVR_TUNABLE(vr_brush_bake_ms, "3") // ... how many milliseconds a frame the bake takes at most
QVR_TUNABLE(vr_brush_luxel, "16") // ... luxels this many texture units apart (Quake's 16; doubled for a brush whose faces don't fit its lightmap rectangle)
QVR_TUNABLE(vr_brush_bake_fill, "1") // ... the map's light the lights don't explain (bounce, sky, glowing textures: the world's under the face less theirs) added, times this
QVR_TUNABLE(vr_brush_bake_reach, "512") // ... a brush made, changed or removed: the brushes within this many units of it baked again (its shadows on them)

// Body > Body
QVR_TUNABLE(vr_death_view_turn_speed, "30") // ... at most this fast (degrees a second)
QVR_TUNABLE(vr_death_view_smooth, "0.25") // immersive death view: seconds the view takes to follow the head (smoothed; 0 at once)
QVR_TUNABLE(vr_death_view_fade, "0.2") // immersive death view: the view fades in from black over this long as it goes into the head and back on respawning (0 none)
QVR_TUNABLE(vr_death_view_menu_time, "0.3") // immersive death view: seconds the view takes out to Third Person's place as a menu or the console opens, and back into the head as it closes (eased; 0 at once)
QVR_TUNABLE(vr_death_ragdoll_push, "1") // your ragdoll body takes the killing blow's push times this (1 as Quake throws the player, 0 none: only the motion you had), at most 400 units/s (QC vr_deathdoll.qc)
QVR_TUNABLE(vr_holster_leg_follow, "0.4") // hip holsters ride the legs' animation (full body): 0 fixed on the body, 1 all the way with the thighs
QVR_TUNABLE(vr_quad_glow, "2") // the glow's strength (0 none .. 2)
QVR_TUNABLE(vr_quad_glow_light, "2") // the blue light the hands and body cast on the world (0 none .. 2)
QVR_TUNABLE(vr_pent_glow, "2")
QVR_TUNABLE(vr_pent_glow_light, "2")
QVR_TUNABLE(vr_suit_glow, "1")
QVR_TUNABLE(vr_suit_glow_light, "1")
QVR_TUNABLE(vr_torso_head, "1") // its weights: the head's yaw now,
QVR_TUNABLE(vr_torso_head_history, "1") // ... where the head faced lately (a glance turns the torso less),
QVR_TUNABLE(vr_torso_hands, "2.5") // ... both hands' midpoint, when both are in front,
QVR_TUNABLE(vr_torso_one_hand, "0.3") // ... one hand alone in front (the other behind the back or hanging down)
QVR_TUNABLE(vr_torso_hands_down, "2") // ... and both hands hanging by the sides (square to the line between them)
QVR_TUNABLE(vr_torso_head_lag, "0.3") // seconds "lately" spans (the eased head's time constant)
QVR_TUNABLE(vr_torso_side_angle, "45") // a hand further off to the torso's side than this (degrees from the chest) counts less, none 35 degrees on
QVR_TUNABLE(vr_torso_deadzone, "8") // degrees the estimate must be off before the torso turns (then it turns all the way)
QVR_TUNABLE(vr_torso_speed, "12") // how quickly it turns then (1/s; 0: at once)
QVR_TUNABLE(vr_torso_neck_max, "70") // degrees the head turns from the torso at most: past it the torso turns with the head at once
QVR_TUNABLE(vr_bodytrack_torso, "1") // the torso's share: its lean (the spine's tilt), the chest's twist, the hips' facing, and the lean detection (0 .. 1)
QVR_TUNABLE(vr_bodytrack_arm_fit_stretch, "1.5") // how far that fit may lengthen the collarbone and the arm's bones (and 1 / it shorten them) to put the shoulder and the elbow on the tracked ones (1 .. 3; was 1.25)
QVR_TUNABLE(vr_bodytrack_legs, "1") // the legs' share: the feet and knees towards the generated legs (Quest 3 guesses them) (0 .. 1; climbing and swimming feet keep theirs)
QVR_TUNABLE(vr_bodytrack_confidence, "0.5") // below this confidence (0 .. 1) the tracking eases out to the guesses (vr_bodytrack_stats' "gate" line: when it did)
QVR_TUNABLE(vr_bodytrack_smooth, "0.04") // seconds the tracked joints are smoothed over (0: raw)

// Body > Body - Crouched Pose
QVR_TUNABLE(vr_hip_holster_crouch_z, "0")
QVR_TUNABLE(vr_hip_holster_crouch_pitch, "0")
QVR_TUNABLE(vr_hip_holster_crouch_yaw, "0")
QVR_TUNABLE(vr_upper_holster_crouch_x, "0")
QVR_TUNABLE(vr_upper_holster_crouch_y, "0")
QVR_TUNABLE(vr_upper_holster_crouch_z, "0")
QVR_TUNABLE(vr_upper_holster_crouch_pitch, "0")
QVR_TUNABLE(vr_upper_holster_crouch_yaw, "0")
QVR_TUNABLE(vr_upper_holster_crouch_roll, "0")
QVR_TUNABLE(vr_shoulder_holster_crouch_x, "0")
QVR_TUNABLE(vr_shoulder_holster_crouch_y, "0")
QVR_TUNABLE(vr_shoulder_holster_crouch_z, "0")
QVR_TUNABLE(vr_shoulder_holster_crouch_pitch, "0")
QVR_TUNABLE(vr_shoulder_holster_crouch_yaw, "0")
QVR_TUNABLE(vr_shoulder_holster_crouch_roll, "0")
QVR_TUNABLE(vr_ammo_pouch_crouch_y, "0")
QVR_TUNABLE(vr_ammo_pouch_crouch_yaw, "0")
QVR_TUNABLE(vr_ammo_pouch_crouch_roll, "0")

// Flashlight > Flashlight
QVR_TUNABLE(vr_flashlight_brightness, "1")
QVR_TUNABLE(vr_flashlight_range, "1000") // units the beam reaches
QVR_TUNABLE(vr_flashlight_beam, "0.5") // how strong its soft cone of light in the air is, 0..1 (0: none)
QVR_TUNABLE(vr_flashlight_flick_max_move, "1.5") // m/s: the hand moving faster than this (a swing, a punch) never flicks
QVR_TUNABLE(vr_flashlight_flick_cooldown, "0.4") // real seconds after a flick before the next counts (the wrist coming back is not a second flick)
QVR_TUNABLE(vr_flashlight_hand_forward, "-0.05") // metres the torch sits forward in the fist (negative: back),
QVR_TUNABLE(vr_flashlight_hand_up, "-0.04") // and up (negative: down; round 21: on top of the fitted grip)
QVR_TUNABLE(vr_flashlight_saturation, "0.4") // its saturation (0: white, 1: full colour): the light, the visible beam and the lens

// Flashlight > Flashlight - Low Grip
QVR_TUNABLE(vr_flashlight_low_x, "-1")
QVR_TUNABLE(vr_flashlight_low_y, "-1.5")
QVR_TUNABLE(vr_flashlight_low_z, "-1.5")
QVR_TUNABLE(vr_flashlight_low_pitch, "-55")
QVR_TUNABLE(vr_flashlight_low_yaw, "0")
QVR_TUNABLE(vr_flashlight_low_roll, "0")

// Flashlight > Flashlight - On a Gun or Head
QVR_TUNABLE(vr_flashlight_gun_forward, "0") // metres the torch clipped on a gun (B/Y) sits further forward (its lens: 1 cm behind the muzzle),
QVR_TUNABLE(vr_flashlight_gun_up, "0") // up (from under the gun, or beside it: fitted per gun),
QVR_TUNABLE(vr_flashlight_gun_out, "0") // and out (away from the body)
QVR_TUNABLE(vr_flashlight_head_forward, "0") // round 21: metres the torch clipped on the head (B/Y) sits further forward (its lens: level with the eyes),
QVR_TUNABLE(vr_flashlight_head_up, "0") // up (3.5 cm over the eyes),
QVR_TUNABLE(vr_flashlight_head_out, "0") // and out (8.5 cm to the side)
QVR_TUNABLE(vr_flashlight_gun_zone_forward, "0")
QVR_TUNABLE(vr_flashlight_gun_zone_up, "0")
QVR_TUNABLE(vr_flashlight_gun_zone_out, "0")
QVR_TUNABLE(vr_flashlight_gun_zone_radius, "0.12")

// Flashlight > Flashlight - Overhead Grip
QVR_TUNABLE(vr_flashlight_high_x, "-1.5")
QVR_TUNABLE(vr_flashlight_high_y, "-1")
QVR_TUNABLE(vr_flashlight_high_z, "-2.5")
QVR_TUNABLE(vr_flashlight_high_pitch, "0")
QVR_TUNABLE(vr_flashlight_high_yaw, "0")
QVR_TUNABLE(vr_flashlight_high_roll, "0")

// World > World
QVR_TUNABLE(vr_liquid_fall_cushion, "48") // units of water, slime or lava over where a monster lands (standing or knocked down) that take all of its fall damage; shallower takes that share of its speed; 0: liquid takes none
QVR_TUNABLE(vr_enemy_drops_chance_mult, "1.0")
QVR_TUNABLE(vr_ammobox_drops_chance_mult, "1.0")
QVR_TUNABLE(vr_item_float_height, "26") // units above the floor the map's weapons, armour, powerups and keys float at (0: on the floor); next map

// Carrying and Throwing > Carrying
QVR_TUNABLE(vr_box3d_hand_hold_mass, "20") // kg, the heaviest prop an open hand or a weapon holds up (heavier: it slips through, pushed still); 0: no limit
QVR_TUNABLE(vr_box3d_throw_grace_speed, "0.75") // m/s, the throw (the hand's throw speed, or its spin at the thing's reach) from which vr_box3d_throw_grace applies: a slower release (opening the hand held still) keeps meeting the hand, so it rests on the palm
QVR_TUNABLE(vr_box3d_hand_mass, "3") // kg, what an empty hand pushes with (and each hand holding a prop, with its own mass): a prop keeps hand / (hand + its mass) of the kinematic push (vr_box3d.cpp pushShare); 0: no limit
QVR_TUNABLE(vr_box3d_press_max, "50") // with it: cm the hand is held back at most; pressed deeper, it goes into that prop by the rest (still not pushing it)
QVR_TUNABLE(vr_box3d_push_force, "200") // N, the most an arm shoves a prop with once it has hit it (a hand's body pushing on: vr_box3d.cpp limitPushes); 0: no limit
QVR_TUNABLE(vr_box3d_weapon_arm_mass, "1") // kg, the arm behind a held weapon: it pushes with this and its own Mass (Weapon Weights: the shotgun 3 kg, 4 in all; the rocket launcher 9; vr_box3d.cpp reachMass); this and vr_box3d_hand_mass 0: no limit
QVR_TUNABLE(vr_physsound_impact, "1") // props hitting the level, doors and each other: volume (0 off)
QVR_TUNABLE(vr_physsound_min_speed, "1.5") // m/s, the slowest hit that knocks (a 4 cm drop; quietly)
QVR_TUNABLE(vr_physsound_full_speed, "10") // m/s, a hit this hard or harder at full volume (a 1 m drop: 7.8)
QVR_TUNABLE(vr_physsound_interval, "0.12") // s, a prop knocks at most this often (a harder hit, twice as loud, sooner)
QVR_TUNABLE(vr_physsound_bodies, "1") // ragdolls and pushable corpses hitting the level, props and each other (flesh): volume (0 off, up to 2)
QVR_TUNABLE(vr_physsound_body_min_speed, "2") // m/s, the slowest hit of a body's that knocks (a settling pile's twitches are slower)
QVR_TUNABLE(vr_physsound_scrape, "0.8") // props sliding with friction (dragged, shoved, skidding): volume (0 off)
QVR_TUNABLE(vr_physsound_scrape_min, "0.3") // m/s, the slowest slide that scrapes
QVR_TUNABLE(vr_physsound_scrape_full, "3") // m/s, a slide this fast or faster at full volume
QVR_TUNABLE(vr_held_collide_max, "5") // with it: cm, the most each hand is drawn moved back (pressed further, they overlap by the rest)
QVR_TUNABLE(vr_held_collide_wall_max, "40") // with it: cm, the most it and the hand are drawn held back (pushed further, it goes in by the rest; the server drops it 32 units past)
QVR_TUNABLE(vr_held_fit_gap, "-4.5") // with it: cm between the curled fingers and what they hold (negative: sunk in)
QVR_TUNABLE(vr_grip_fit_finger_depth, "0.4") // with it: cm the fingers and thumb may press into the prop (contact)
QVR_TUNABLE(vr_grip_fit_palm_depth, "0.2") // with it: cm the palm, the back of the hand and the wrist may press into it
QVR_TUNABLE(vr_grip_fit_reach, "0.75") // with it: the most it is moved, in hand lengths (the least overlap within that if it can't clear)
QVR_TUNABLE(vr_carry_grab_bias, "0") // cm: a hand takes a box, gib, backpack or armour if its fist (the palm and the curled fingers) is within this of the thing's drawn surface (negative: pressed that far into it; vr_held.cpp grabTouch)
QVR_TUNABLE(vr_weapon_catch_window, "0.25") // s (the game's): a weapon in the air is caught by a grip closed up to this long before it reaches the fist (0.125 for one lying about), and the hand that tossed it may close again at once: it catches it once it comes back down (not within 0.05 s of letting go); 0: as a weapon lying about (QC wpnthrow_midair_catch)
QVR_TUNABLE(vr_carry_two_hands_reach, "2") // with it: cm past Grab Distance Bias the other hand may be off the surface of what one hand holds and still take it (the hand that let go of it grips it again where it is: a fitted prop sits about 1 cm off the fist test; vr_carry2h.cpp reaches)
QVR_TUNABLE(vr_carry_two_hands_drift, "8") // with it: cm each drawn hand may be off its controller to stay on its grip (hands pulled apart or pushed together)
QVR_TUNABLE(vr_armor_scale, "0.5") // with it: the armour's size (1: Quake's, a metre tall; next map)
QVR_TUNABLE(vr_carry_nudge, "1") // how hard hands and guns push them
QVR_TUNABLE(vr_pickup_float_grace_time, "0.4") // with it: how long (seconds from the hand reaching it) before a hand still pushing into it knocks it loose
QVR_TUNABLE(vr_pickup_float_grace_swat, "2") // with it: a hand faster than this (m/s) knocks it at once (a swat)
QVR_TUNABLE(vr_carry_throw_mult, "1") // how fast they leave the hand
QVR_TUNABLE(vr_carry_throw_damage, "8") // damage of a box thrown hard (at about 6 m/s; more the faster)
QVR_TUNABLE(vr_throw_heavy_from, "8") // kg: props heavier than this hurt more thrown (0: off; weapons never)
QVR_TUNABLE(vr_throw_heavy_exp, "1") // the curve's exponent (1: in proportion to the mass)
QVR_TUNABLE(vr_throw_heavy_max, "5") // the most it multiplies by
QVR_TUNABLE(vr_gun_wall_max, "40") // cm, the most a held weapon and its hand are held out of the level (pushed further, they go in by the rest; the weapon keeps resting where it was held)
QVR_TUNABLE(vr_hand_collide, "5") // cm a free hand and what the other hand holds (its weapon, or the prop it holds alone) are each drawn moved back at most as the hand meets its surface (vr_hand_collide_props_margin off it), by half how deep it would be, as a prop held there and that thing (vr_held_collide_max); pressed further, the hand sinks in by the rest (0: it always passes through)
QVR_TUNABLE(vr_hand_collide_props_margin, "1") // cm off the surface of the prop or the weapon the other hand holds the free hand's palm and knuckles stop (0..2: the drawn skin about at it)

// Carrying and Throwing > Crates
QVR_TUNABLE(vr_crates_chance, "0.06") // the chance a spot by a wall (one every 24 units along it) gets a crate, before the limits
QVR_TUNABLE(vr_crates_corner, "4") // times the chance in a corner (a wall beside the spot along the wall)
QVR_TUNABLE(vr_crates_max, "24") // most crates in a map (a stack counts two)
QVR_TUNABLE(vr_crates_spacing, "224") // units between crate spots
QVR_TUNABLE(vr_crates_stack, "0.25") // the chance a crate has another on top (turned and off centre a little)
QVR_TUNABLE(vr_crates_large, "0.4") // the share of large crates (the rest small)
QVR_TUNABLE(vr_crates_barrels, "0.1") // the roll for a barrel (a barrel fits more often: 0.1 comes out at about 13% of the crates and barrels; 0.15 at 19%; the author 2026-10-11: fewer barrels); was: the share of barrels (upright; a stack's top too, on a crate or a barrel: crates and barrels mixed; the NOTES.md note 2026-10-07 23:01:54)
QVR_TUNABLE(vr_crates_barrel_lying, "0.15") // the share of barrels standing alone that lie on their side along the wall
QVR_TUNABLE(vr_crates_clearance, "72") // units of open floor kept in front of a crate (a passage stays passable)
QVR_TUNABLE(vr_crates_margin, "56") // units from any other entity (doors, lifts and teleporters more, the start most)
QVR_TUNABLE(vr_crates_seed, "0") // mixed with the map's name: another layout (the same each load)
QVR_TUNABLE(vr_crate_impact, "16") // m/s: a crate hitting something this hard breaks (less: damaged from 70% of it); 0: never
QVR_TUNABLE(vr_crate_piece_impact, "8") // m/s: a crate's piece thrown into something this hard bursts (from half, damaged); 0: never
QVR_TUNABLE(vr_carry_grab_ease, "0.12") // s: a prop just taken in one hand is drawn from where it was onto its grip's place over this (a Fixed or In the Palm grip turns and moves it there; 0: at once)
QVR_TUNABLE(vr_carry_slam_height, "10") // cm: the hands' middle at least this far over the eyes is over the head
QVR_TUNABLE(vr_carry_slam_speed, "3") // m/s: the hands swung down at least this fast (mostly downwards) break it on what they meet
QVR_TUNABLE(vr_carry_slam_window, "1") // s after it was last over the head a swing down still counts
QVR_TUNABLE(vr_carry_slam_damage, "2") // what it is swung onto takes this times a punch with it in hand (times its speed over the least, at most twice)
QVR_TUNABLE(vr_crate_slam_throw_damage, "0.55") // let go of from over the head, its first hard landing takes this share of its full health (0.55: two such throws break it)
QVR_TUNABLE(vr_crate_slam_throw_speed, "3") // m/s: ... a landing at least this fast
QVR_TUNABLE(vr_crate_hit_volume, "2.5") // a crate knocked (hit, not broken): its crack of wood times this (1: as before, 2026-11-02; up to 4, over full: SND_LOUD)
QVR_TUNABLE(vr_crate_break_volume, "2") // a crate breaking: its sound times this (1: as before; up to 4)
QVR_TUNABLE(vr_crate_pieces, "12") // pieces a small crate breaks into (a large one more, by its size)
QVR_TUNABLE(vr_crate_piece_max, "48") // most pieces lying about at once (the oldest turn to dust)
QVR_TUNABLE(vr_crate_piece_time, "60") // s a piece lies about before it fades away (0: for ever)
QVR_TUNABLE(vr_crate_ammo, "0.35") // the chance a crate holds a small box of ammo for a weapon you have (or the shotgun)
QVR_TUNABLE(vr_crate_health_box, "0.2") // the chance a crate holds a small health box
QVR_TUNABLE(vr_crate_item_pop, "3.5") // how hard what a crate held pops out when it breaks (m/s: up, and a little outwards)
QVR_TUNABLE(vr_crate_crowbar, "0.1") // the chance a crate placed about the map (the top of a stack) has a crowbar lying on it (next map)
QVR_TUNABLE(vr_crate_crowbar_max, "4") // most crowbars on crates in a map
QVR_TUNABLE(vr_crate_flaregun, "0.1") // the chance a top placed about the map (a crate's or an upright barrel's) has a loaded flare gun lying on it, rolled apart from the crowbars (next map)
QVR_TUNABLE(vr_crate_flaregun_max, "2") // most flare guns on crates in a map

// Carrying and Throwing > Force Grab
QVR_TUNABLE(vr_forcegrab_outline, "1") // the glow round what a hand's force grab points at (strength, 0 off)
QVR_TUNABLE(vr_forcegrab_distance, "200") // units objects can be pulled from,
QVR_TUNABLE(vr_forcegrab_cone, "15") // degreesoff where the hand points
QVR_TUNABLE(vr_forcegrab_flick_speed, "1.0") // m/s back or up the hand flicks to pull,
QVR_TUNABLE(vr_forcegrab_flick_turn, "250") // or degrees/s the fingers swing that way
QVR_TUNABLE(vr_forcegrab_time, "0.35") // seconds of flight, plus
QVR_TUNABLE(vr_forcegrab_speed, "1000") // a second per this many units
QVR_TUNABLE(vr_forcegrab_arc, "0.2") // height of the arc, of the distance
QVR_TUNABLE(vr_forcegrab_catch_radius, "14") // units from the hand it can be caught,
QVR_TUNABLE(vr_forcegrab_catch_early, "0.35") // the grip pressed at most this early,
QVR_TUNABLE(vr_forcegrab_catch_blend, "0.15") // seconds a caught weapon takes to ease from how it flew into the hand (0 at once; visual only)

// Carrying and Throwing > Gibs and Corpses
QVR_TUNABLE(vr_gib_throw_damage, "4") // and of a gib or head
QVR_TUNABLE(vr_gib_health, "12") // the damage that destroys a gib (a head takes half as much again)
QVR_TUNABLE(vr_gib_overkill_melee, "0.5") // the share of a hand's blow's overkill (damage past its health) that counts towards gibbing a monster; Quad's all of it (QC combat.qc VR_Gib_Overkill; 1 before his note 2026-11-02)
QVR_TUNABLE(vr_gib_overkill_prop, "0.1") // and of a thrown, flung or fallen prop's (a limb or head it hit still pops; 1 before his note 2026-11-02)
QVR_TUNABLE(vr_gib_splat_speed, "250") // units/s a thrown gib or head must hit a wall or a monster at to burst
QVR_TUNABLE(vr_gib_splat_throw, "7") // m/s of the throwing hand from which a thrown gib or head bursts on a wall instead of sticking, however heavy; 0 off
QVR_TUNABLE(vr_gib_burst_speed, "5") // m/s: a hand's or weapon's hit on a gib, or one held and smashed into something, this fast (relative) bursts it; from half as fast it wears it (and bounces off the hand)
QVR_TUNABLE(vr_gib_burst_head, "1.3") // times vr_gib_burst_speed for a head
QVR_TUNABLE(vr_gib_burst_small, "0.7") // times vr_gib_burst_speed for a small gib or brain chunk
QVR_TUNABLE(vr_gib_bounce, "0.4") // how much a gib a hand hits short of bursting bounces off it (restitution: 0 dead, 1 elastic)
QVR_TUNABLE(vr_corpse_health_mult, "2") // times every monster's corpse health (vr_corpse_health_<monster>): the damage that gibs a corpse or a ragdoll
QVR_TUNABLE(vr_corpse_collide_mass, "150") // kg: a pushable corpse's mass (heavier: harder to push; a player walking into one shoves it under about 60)
QVR_TUNABLE(vr_corpse_collide_friction, "1") // a pushable corpse's friction (with the floor's: 0.6)

// Carrying and Throwing > Gibs and Corpses - Corpse Damage and Health
QVR_TUNABLE(vr_corpse_health_grunt, "80") // the damage that gibs a grunt's corpse or ragdoll (times vr_corpse_health_mult)
QVR_TUNABLE(vr_corpse_health_enforcer, "80") // the damage that gibs a enforcer's corpse or ragdoll (times vr_corpse_health_mult)
QVR_TUNABLE(vr_corpse_health_dog, "80") // the damage that gibs a dog's corpse or ragdoll (times vr_corpse_health_mult)
QVR_TUNABLE(vr_corpse_health_fiend, "180") // the damage that gibs a fiend's corpse or ragdoll (times vr_corpse_health_mult)
QVR_TUNABLE(vr_corpse_health_ogre, "140") // the damage that gibs a ogre's corpse or ragdoll (times vr_corpse_health_mult)
QVR_TUNABLE(vr_corpse_health_knight, "80") // the damage that gibs a knight's corpse or ragdoll (times vr_corpse_health_mult)
QVR_TUNABLE(vr_corpse_health_hellknight, "160") // the damage that gibs a hellknight's corpse or ragdoll (times vr_corpse_health_mult)
QVR_TUNABLE(vr_corpse_health_vore, "220") // the damage that gibs a vore's corpse or ragdoll (times vr_corpse_health_mult)
QVR_TUNABLE(vr_corpse_health_shambler, "280") // the damage that gibs a shambler's corpse or ragdoll (times vr_corpse_health_mult)
QVR_TUNABLE(vr_corpse_health_scrag, "80") // the damage that gibs a scrag's corpse or ragdoll (times vr_corpse_health_mult)
QVR_TUNABLE(vr_corpse_health_fish, "80") // the damage that gibs a fish's corpse or ragdoll (times vr_corpse_health_mult)
QVR_TUNABLE(vr_corpse_health_zombie, "60") // the damage that gibs a beheaded zombie's corpse (its ragdoll; times vr_corpse_health_mult)
QVR_TUNABLE(vr_corpse_health_mummy, "120") // the damage that gibs a beheaded mummy's corpse (Rogue; its ragdoll; times vr_corpse_health_mult)
QVR_TUNABLE(vr_corpse_health_gremlin, "100") // the damage that gibs a gremlin's corpse or ragdoll (times vr_corpse_health_mult)
QVR_TUNABLE(vr_corpse_health_scourge, "180") // the damage that gibs a scourge's corpse or ragdoll (times vr_corpse_health_mult)
QVR_TUNABLE(vr_corpse_health_eel, "80") // the damage that gibs a eel's corpse or ragdoll (times vr_corpse_health_mult)
QVR_TUNABLE(vr_corpse_dmg_shots, "1") // times the damage corpses and ragdolls take from shotguns and the grunts' guns
QVR_TUNABLE(vr_corpse_dmg_nails, "1") // times the damage corpses and ragdolls take from nails, an enforcer's laser, knights' and scrags' spikes
QVR_TUNABLE(vr_corpse_dmg_explosions, "1") // times the damage corpses and ragdolls take from rockets, grenades, plasma and their blasts
QVR_TUNABLE(vr_corpse_dmg_lightning, "1") // times the damage corpses and ragdolls take from the lightning gun, Mjolnir, a discharge
QVR_TUNABLE(vr_corpse_dmg_blunt, "1") // times the damage corpses and ragdolls take from a crowbar, a gun or Mjolnir swung, a pommel
QVR_TUNABLE(vr_corpse_dmg_fists, "1") // times the damage corpses and ragdolls take from an empty fist
QVR_TUNABLE(vr_corpse_dmg_blades, "1") // times the damage corpses and ragdolls take from the axe's and a sword's edge or point, swung or thrown; a stuck axe bleeding it
QVR_TUNABLE(vr_corpse_dmg_chainsaw, "1") // times the damage corpses and ragdolls take from the chainsaw
QVR_TUNABLE(vr_corpse_dmg_props, "0.25") // times the damage corpses and ragdolls take from thrown or flung boxes, gibs, heads and weapons, deflected shots, a prop in the fist (0.25: one crate or barrel thrown does not burst a body, his note 2026-11-02; 1 before)
QVR_TUNABLE(vr_corpse_dmg_fire, "1") // times the damage corpses and ragdolls take from burning (vr_burn_corpse_damage first)
QVR_TUNABLE(vr_corpse_dmg_bash, "1") // times the damage corpses and ragdolls take from bashes, shoves and headbutts
QVR_TUNABLE(vr_corpse_dmg_other, "1") // times the damage corpses and ragdolls take from anything else: monsters' blows, kicks, crushers

// Carrying and Throwing > Gibs and Corpses - Ragdolls
QVR_TUNABLE(vr_corpse_placed_wake, "160") // units: a placed corpse a player comes this near becomes a ragdoll (vr_corpse_placed); one that got none (no room) sleeps again past 1.5 times this, to try once more// Knockdowns (QC vr_knockdown.qc; vr_box3d.cpp "Knockdowns"; Combat > Knockdowns): a shove can knock a monster with a
QVR_TUNABLE(vr_ragdoll_float_water, "1.05") // a ragdoll's (or corpse's) lift in water at full depth, times its weight: above 1 it floats, below it sinks (the torso floats more than the limbs, an armoured knight less); 0: none
QVR_TUNABLE(vr_ragdoll_float_slime, "1.15") // ... in slime
QVR_TUNABLE(vr_ragdoll_float_lava, "1.4") // ... in lava (it floats on it, burning: QC vr_burn_lava_bodies)
QVR_TUNABLE(vr_ragdoll_drag_water, "1.5") // 1/s: how fast water damps a ragdoll's motion and spin (twice that at 60 units/s, more the faster: a fall into it is braked, a sink is slow); 0: none
QVR_TUNABLE(vr_ragdoll_drag_slime, "3") // ... slime's
QVR_TUNABLE(vr_ragdoll_drag_lava, "5") // ... lava's
QVR_TUNABLE(vr_ragdoll_grab, "1") // take a ragdoll's limb: 0 never, 1 by hand (an old config's 2: the same)
QVR_TUNABLE(vr_ragdoll_grab_force, "4000") // N: the most a hand pulls a held limb with (3000: a grunt lifted by his chest sags a unit; 1500: about 18)
QVR_TUNABLE(vr_ragdoll_grab_fit, "1") // a taken limb's surface brought onto the palm (as a carried prop's: vr_held_surface_fit); 0: held where the hand's middle was
QVR_TUNABLE(vr_ragdoll_hand_stick, "30") // cm: the most the hand holding a limb is drawn off its controller to stay on the limb (it lags and sags); 0: on the controller
QVR_TUNABLE(vr_ragdoll_hand_turn, "70") // degrees: and the most it is drawn turned off it, with the limb twisting in the hand (the hand turns about its fingers on the limb)
QVR_TUNABLE(vr_ragdoll_hand_rest, "6") // cm: the hand drawn on a held limb slid at most this along the way to its mesh, for its palm to rest on it (not off it, not sunk in it; a tug's on the weapon too); 0: as the hold has it
QVR_TUNABLE(vr_ragdoll_throw, "1") // times the hand's throw a limb let go of keeps (0: only what the pull gave it)
QVR_TUNABLE(vr_ragdoll_blood, "1") // a ragdoll's limbs flung fast leave blood trails and drops (as gibs do: vr_gib_blood_trail)
QVR_TUNABLE(vr_ragdoll_sleep_skip, "1") // a ragdoll asleep and unchanged is not written again each step (box3d writeRagdoll: only its message time moves on); 0 written every step (the A/B and determinism check)
QVR_TUNABLE(vr_hitmodel_skin_keep, "1") // the precise hits keep a ragdoll's and an armed humanoid's skinned body (and its hierarchy's bounds) while unchanged, not made or copied again for each trace (the same vertices bit for bit); 0 as before (the A/B check)
QVR_TUNABLE(vr_ragdoll_smooth, "1") // drawn between the server's last two steps, as props are (smooth at a headset's 90 Hz over the server's 72); 0 the latest step (judders)
QVR_TUNABLE(vr_ragdoll_held_local, "1") // a held limb's ragdoll follows your hand at the frame rate, as a held prop does (the server's step lags it); 0 the server's pose only
QVR_TUNABLE(vr_ragdoll_active_headshot, "0.05") // the strength a headshot or a beheading leaves (an almost pure collapse)
QVR_TUNABLE(vr_ragdoll_active_env_brace, "1") // falling onto a floor, a wall or a crate within reach: his arms go out to it and take the landing, giving
QVR_TUNABLE(vr_ragdoll_active_env_lean, "1") // dying with a wall behind or beside him: a hand braced on it, he slumps down it (a crate: braced, not held)
QVR_TUNABLE(vr_ragdoll_active_env_slide, "0.6") // body weights a wall he leans on (env_lean) holds his body up at most, fading: he slides down it slowly, not dropping (0: off)
QVR_TUNABLE(vr_ragdoll_active_env_ledge, "1") // falling past a ledge's lip near a hand: he grabs it and hangs a moment before dropping
QVR_TUNABLE(vr_ragdoll_active_env_grip, "1.2") // body weights a hand holding a ledge takes at most (a lean's: a third of it; a heavy monster less); past it he slips off
QVR_TUNABLE(vr_ragdoll_active_env_hang, "1.8") // s: about how long he hangs from a ledge (each 0.45 to 1.65 times it)
QVR_TUNABLE(vr_ragdoll_active_env_arms, "1") // times the strength of his arms reaching for what is round him (a catch, a lean, a ledge's hold, a brace; 0..2)
QVR_TUNABLE(vr_ragdoll_mass, "80") // kg: a ragdoll's whole mass (its parts by their volume); each class has its own by default (vr_ragdoll_<class>_mass), this is for a class set to -1
QVR_TUNABLE(vr_ragdoll_friction, "0.8") // a ragdoll's friction (with the floor's: 0.6)
QVR_TUNABLE(vr_ragdoll_joint_friction, "1.5") // N m: the joints' friction (a motor holding each still up to this torque); 0 limp
QVR_TUNABLE(vr_ragdoll_damping, "0.4") // its parts' angular damping (1/s): less flailing
QVR_TUNABLE(vr_ragdoll_blast, "2") // times the throw a blast gives a prop of a ragdoll's mass (explosions throw ragdolls)
QVR_TUNABLE(vr_ragdoll_joint_stiffness, "0") // Hz: a spring in each joint pulling it back towards how he stood (0 none; 1-3 a body with some tone left)
QVR_TUNABLE(vr_ragdoll_limits, "1") // the joints' limits (cone, twist, the knees' and elbows' range) times this (0.25 stiff .. 1.5 loose)
QVR_TUNABLE(vr_ragdoll_active_strength, "1") // times the joints' strength (0 limp .. 3)
QVR_TUNABLE(vr_ragdoll_active_hertz, "10") // Hz: a joint's spring towards its pose at full strength (at most 20)
QVR_TUNABLE(vr_ragdoll_active_damping, "1") // its damping (1: critical)
QVR_TUNABLE(vr_ragdoll_active_torque, "2") // a joint's motor at full strength: at most this many times the weight of what hangs on it times its lever (a blow or a hand overpowers it)
QVR_TUNABLE(vr_ragdoll_active_fade, "1") // times how long the strength lasts (his death animation's length: the legs give first, then the torso, the arms, the head)
QVR_TUNABLE(vr_ragdoll_active_pin, "0.6") // body weights: his pelvis pulled along his death animation's at first (its throw back or forward kept), at most this force; 0 none
QVR_TUNABLE(vr_ragdoll_active_pin_time, "0.3") // s: and that pull fading out over this
QVR_TUNABLE(vr_ragdoll_active_stumble, "90") // units/s: dying on the move at least this fast, he stumbles on (carried a moment, a leg stepping ahead); 0 never
QVR_TUNABLE(vr_ragdoll_active_crawl_force, "1.3") // body weights: his arms' pull along the floor at most
QVR_TUNABLE(vr_ragdoll_active_dismember_time, "3") // s: how long he writhes holding the stump once down
QVR_TUNABLE(vr_ragdoll_active_writhe, "1") // times how far his torso and legs stir as he writhes (0 still .. 3)
QVR_TUNABLE(vr_ragdoll_active_balance_time, "1.2") // s: on one leg, how long he tries to keep his balance at most
QVR_TUNABLE(vr_ragdoll_active_balance_lean, "15") // degrees: and he falls once leaning this far from how he stood
QVR_TUNABLE(vr_ragdoll_active_paddle, "4.5") // s: in water he paddles about this long (each 0.8 to 1.2 times it), weakening from a third of it on
QVR_TUNABLE(vr_ragdoll_active_flail, "1.5") // s: in lava he flails about this long (slime: 1.4 times it; each 0.8 to 1.2 times it)
QVR_TUNABLE(vr_ragdoll_active_stagger_lean, "30") // degrees: its middle leaning over its feet past this for 0.36 s (or twice it at once), the stagger falls whatever its balance
QVR_TUNABLE(vr_ragdoll_active_stagger_push, "2") // m/s: a fresh two-hand shove's push on its top (its pelvis carried by its pin, its feet none)
QVR_TUNABLE(vr_ragdoll_active_stagger_pin, "2") // body weights: its pelvis's pin at most, steady (times 0.2 .. 1 by its balance: the knockdown's chance against the roll; its muscles' hold on its weight and its upright too)
QVR_TUNABLE(vr_ragdoll_active_stagger_blend, "0.2") // s: recovered, its ragdoll blended into the animated monster over this
QVR_TUNABLE(vr_ragdoll_active_stagger_settle, "1") // m/s: its backpedal near done (its pin's carry 90% over), slower than this, steady and not mid-step: it has recovered (0.4, from 0.2 s past its carry, before: it froze a moment at its end)
QVR_TUNABLE(vr_ragdoll_active_stagger_settle_time, "0.35") // s: still up this long past its carry, it has recovered however it moves (1.2 before)
QVR_TUNABLE(vr_ragdoll_active_stagger_drop, "20") // units: the floor under its pelvis this much lower than where it stood (stepped off a ledge, the top of stairs) a moment: it falls, sure-footed or not (no backpedalling on air); 0: never
QVR_TUNABLE(vr_knockdown_ledge_backpedal, "1") // a shove over a ledge makes one that can stagger backpedal on its feet to the edge and fall over it (VR_Knockdown_LedgeBackpedal); 0: launched whole past the edge (vr_knockdown_shove_ledge_*), as those that can't stagger are
QVR_TUNABLE(vr_knockdown_ledge_backpedal_speed, "0.4") // its first speed, times the shove's own speed (vr_push's, a running shove's, its weight's)
QVR_TUNABLE(vr_knockdown_ledge_backpedal_least, "70") // u/s: its first speed at least
QVR_TUNABLE(vr_knockdown_ledge_backpedal_most, "300") // u/s: its first speed at most (its legs keep up to about this)
QVR_TUNABLE(vr_knockdown_ledge_backpedal_push, "0.35") // the shove's push on its top, times this (more: it leans back further and sags)
QVR_TUNABLE(vr_knockdown_ledge_backpedal_hertz, "10") // its pelvis's pin, its stiffness (a stagger's 5): less sits it down on a long way back
QVR_TUNABLE(vr_knockdown_ledge_backpedal_tip, "100") // u/s: gone over the lip, carried on at the speed it has, at least this
QVR_TUNABLE(vr_knockdown_ledge_backpedal_past, "32") // units: ... this far on or until it has dropped
QVR_TUNABLE(vr_knockdown_ledge_backpedal_vary, "0.2") // its first speed and how far it goes, each times 1 plus or minus up to this, by chance
QVR_TUNABLE(vr_knockdown_ledge_backpedal_carry, "2") // how far it goes at least, times the way to past the edge (its speed slows evenly to none over it: more goes over the edge faster), times up to the vary's more
QVR_TUNABLE(vr_knockdown_ledge_backpedal_balance, "0.7") // its balance, from this to 1 by chance (its torso's tone: how it flails going back; under about this it sags too far to keep going)
QVR_TUNABLE(vr_knockdown_ledge_topple, "56") // units: a shove with a ledge this near ahead of its middle topples it over at once (the old launch); further, up to vr_knockdown_ledge_backpedal_reach, it backpedals (his note, 2026-11-05)
QVR_TUNABLE(vr_knockdown_ledge_backpedal_reach, "92") // units: ... with a ledge further than this ahead of its middle, the shove is an ordinary one (no ledge)
QVR_TUNABLE(vr_knockdown_ledge_backpedal_yaw, "6") // degrees: its way off the shove's, up to this either way, by chance
QVR_TUNABLE(vr_ragdoll_active_getup_pin, "2.5") // body weights: its pelvis's pin to the get-up's at most
QVR_TUNABLE(vr_ragdoll_active_getup_blend, "0.2") // s: risen, its ragdoll blended into the animated monster over this
QVR_TUNABLE(vr_ragdoll_active_getup_pace, "0.8") // its get-up's frames' speed, times the get-up's own (vr_knockdown_getup_speed)
QVR_TUNABLE(vr_ragdoll_active_stagger_steps, "6") // a shoved monster's backpedal: at most this many steps back (1..10); one whose balance the shove beat (the knockdown's roll under its chance) gives way after 3 or 4 of them
QVR_TUNABLE(vr_hit_react_strength, "1") // times the kicks' size (0..3)
QVR_TUNABLE(vr_hit_react_time, "1") // times how long a kick lasts (its peak at ~74 ms, gone by ~0.5 s; 0.25..4)

// Carrying and Throwing > Rocks and Bricks
QVR_TUNABLE(vr_debris_chance, "0.1") // the chance a spot at a wall's foot gets a piece (or a few), every 16 units along it, before the limits
QVR_TUNABLE(vr_debris_dark, "0.3") // times the chance where the floor is dark (its lightmap under 8; full chance from 40 up)
QVR_TUNABLE(vr_debris_corner, "2.5") // times the chance in a corner (a wall beside the spot along the wall)
QVR_TUNABLE(vr_debris_cluster, "3") // most pieces together at one spot (1: always alone)
QVR_TUNABLE(vr_debris_max, "160") // most pieces in a map
QVR_TUNABLE(vr_debris_area_max, "8") // most pieces in an area (vr_debris_area_size units square)
QVR_TUNABLE(vr_debris_spacing, "56") // units between spots (the pieces of one spot lie together)
QVR_TUNABLE(vr_debris_size, "0.15") // a piece's size varies this much either way (bricks 40% of it: they are made to a size)
QVR_TUNABLE(vr_debris_seed, "0") // mixed with the map's name: another layout (the same each load)

// Carrying and Throwing > Throwing and Physics
QVR_TUNABLE(vr_throw_window, "0.12")
QVR_TUNABLE(vr_throw_lever_arm, "0.1") // metres from the palm to the held object's centre
QVR_TUNABLE(vr_throw_dir_lookback, "0.04") // seconds before the peak the direction is averaged over
QVR_TUNABLE(vr_throw_slowmo_max_speed, "15") // m/s: the most the stroke's gain (vr_throw_slowmo_stroke_time) raises a slow-motion throw to (never under its real speed); 0: no cap
QVR_TUNABLE(vr_throw_pitch, "0") // degrees every throw's direction is tilted up (negative: down), its speed kept
QVR_TUNABLE(vr_throw_gravity, "9.81") // m/s^2 on thrown weapons, bounces included (0: sv_gravity)
QVR_TUNABLE(vr_throw_gain_max, "1.5")
QVR_TUNABLE(vr_throw_max_speed, "28") // m/s: the most any throw leaves the hand at (a light thing's)
QVR_TUNABLE(vr_throw_mass_light, "1.5") // kg: up to this heavy, the whole vr_throw_max_speed (the author's 1.5 since 2026-10-02: the axe, 2 kg, a little less)
QVR_TUNABLE(vr_throw_mass_exp, "0.9") // how fast the limit falls with mass beyond it (the author's 0.9, 2026-10-02)
QVR_TUNABLE(vr_throw_mass_knee, "0.7") // 0..1: below this share of the limit a throw is untouched
QVR_TUNABLE(vr_throw_2h_strength, "2") // two hands: as if the thing weighed 1 / this (1: no help)
QVR_TUNABLE(vr_throw_2h_heave, "250") // J: two hands throw a heavy thing at up to sqrt(2 x this / its mass) m/s when that is more than the mass limit (40 kg: 3.5 m/s, 80 kg: 2.5); 0: the mass limit alone (before 2026-10-22: 2.7, 1.4)
QVR_TUNABLE(vr_throw_flick_mass, "2.5") // kg: up to this heavy, a wrist flick's whole speed
QVR_TUNABLE(vr_throw_spin_mass_exp, "1")
QVR_TUNABLE(vr_throw_hitbox, "6") // half-size (units) of a thrown weapon's box against monsters
QVR_TUNABLE(vr_throw_hit_min_speed, "200") // units/s a thrown thing must go at to hurt what it touches (slower: the throw is over)
QVR_TUNABLE(vr_button_weapon_reach, "6") // cm round the weapon's line (its thickness and some slack) within which it touches a button
QVR_TUNABLE(vr_button_prop, "1") // 1: so does a held prop touching it (a health or ammo box, a crate, a rock, a gib): its shape as the hand holds it
QVR_TUNABLE(vr_button_prop_reach, "6") // cm round the held prop's shape within which it touches a button (a held body stops 4 cm short of a wall's)
QVR_TUNABLE(vr_button_throw_speed, "150") // units/s it must hit the button at, at least
QVR_TUNABLE(vr_throw_hit_top, "15") // m/s
QVR_TUNABLE(vr_throw_hit_ramp, "0.5") // share of the hit speed the damage eases in over
QVR_TUNABLE(vr_throw_hit_ramp_floor, "0.6") // share of the damage at the hit speed
QVR_TUNABLE(vr_throw_2h_nomelee, "0.3")
QVR_TUNABLE(vr_throw_2h_melee_immune, "0.35")
QVR_TUNABLE(vr_gib_letgo_spare, "0.2")
QVR_TUNABLE(vr_gib_burst_on_thrower, "0") // a thrown gib turned by its thrower's body or hands (no wall ahead) bursts as on a wall (0: kept, the same notes)
QVR_TUNABLE(vr_axestick_speed, "2.5") // m/s the blade's edge must go at into what it meets (relative to it)
QVR_TUNABLE(vr_axestick_angle, "45") // degrees at most the blade may go in out of its own plane (its side first, a flat throw: more)
QVR_TUNABLE(vr_axestick_incidence, "65") // degrees at most between the way the blade goes in and straight into the surface (a glancing blow: more)
QVR_TUNABLE(vr_axestick_leniency, "1") // 0..2: how much of the blade behind its edge cuts when it goes in first (vr_axestick.cpp): 0 the edge and its corners; 1 45% of its width; 2 80%; also met a little ahead, a little more glancing
QVR_TUNABLE(vr_axestick_depth, "5") // cm the edge goes in at twice the least speed (half as deep at it; at most 60% of the blade)
QVR_TUNABLE(vr_axestick_bleed, "5") // health a second a monster an axe is stuck in loses (0: none), the thrower's damage
QVR_TUNABLE(vr_axestick_tug, "0.3") // seconds a force grab tugs a stuck axe before it comes free
QVR_TUNABLE(vr_axestick_metal, "1") // 1: thrown axes stick in the explosive boxes too (the author, 2026-10-06: props in general); 0: they bounce off them (metal drums)
QVR_TUNABLE(vr_throw_assist, "1")
QVR_TUNABLE(vr_throw_assist_cone, "15")
QVR_TUNABLE(vr_throw_assist_strength, "0.35")
QVR_TUNABLE(vr_throw_restitution, "0.25")
QVR_TUNABLE(vr_throw_friction, "0.5")
QVR_TUNABLE(vr_throw_spin_max, "20") // rad/s
QVR_TUNABLE(vr_throw_spin_align, "8")
QVR_TUNABLE(vr_shot_push_pellet, "6") // N s, each shotgun and double shotgun pellet's push (monsters' too)
QVR_TUNABLE(vr_shot_push_nail, "12") // N s, a nail's (the nailgun's, scrags', knights', enforcers' lasers)
QVR_TUNABLE(vr_shot_push_supernail, "24") // N s, a super nailgun nail's
QVR_TUNABLE(vr_shot_push_lightning, "6") // N s, each bolt of the lightning gun's beam (10 a second; a shambler's by its damage)
QVR_TUNABLE(vr_shot_push_speed, "10") // m/s, the most a shot makes what it hits go (light things: a gib, a small gun)
QVR_TUNABLE(vr_throw_self_grace, "0.35") // s after you throw or let go of a thing before it can hurt you (thrown up, falling back on you; 0: at once)
QVR_TUNABLE(vr_throw_grab_press, "0.7")
QVR_TUNABLE(vr_throw_2h_window, "0.2")

// Carrying and Throwing > Wall Torches
QVR_TUNABLE(vr_walltorch_reach, "15") // cm from a torch's stick (its axis, butt to flame) to the hand's point or its fist's middle within which a hand's grip takes it off its wall though the fist isn't on it (0: only the fist on it)
QVR_TUNABLE(vr_walltorch_hits, "5") // blows a torch lands before its fire starts dying, even held (0: blows never use it up)
QVR_TUNABLE(vr_walltorch_damage, "12") // a torch's blow: its base damage, times the blow's strength (a gun's is 12, the axe's 20)
QVR_TUNABLE(vr_walltorch_flame, "1") // a taken torch's flame's size (1: the wall torch's)

// Weapons > Aiming
QVR_TUNABLE(vr_2h_sticky, "1") // 2H grip stickiness: times the let-go distance and angle, always (vr_2h_sticky_fast*: more while swinging)
QVR_TUNABLE(vr_weight_spring_stiffness, "1") // Spring: times the arm's and wrist's stiffness (2: 41% quicker)
QVR_TUNABLE(vr_weight_spring_damping, "1") // Spring: the damping ratio (1 critical: settles without swinging back; less: overshoots)
QVR_TUNABLE(vr_weight_spring_strength, "1") // Spring: times the arm's force and the wrist's torque (0: unlimited)
QVR_TUNABLE(vr_weight_spring_sag, "1") // Spring: times the droop of heavy things under gravity (more at arm's length)
QVR_TUNABLE(vr_weight_spring_inertia, "1") // Spring: times the swing a moving hand gives what it holds (its centre of mass trailing)
QVR_TUNABLE(vr_weight_spring_2h, "4") // Spring: times the stiffness and strength with a two-handed grip
QVR_TUNABLE(vr_weight_spring_roll, "2.5") // Spring: times the inertia about the forward axis (a roll of the wrist: 2, 41% slower)
QVR_TUNABLE(vr_weight_spring_snap, "60") // Spring: cm (or 3 times as many degrees; past 165 always) off the hand beyond which it is put back in it at once
QVR_TUNABLE(vr_weight_stamina_from, "0.5") // the stamina left (0..1) below which it starts
QVR_TUNABLE(vr_weight_stamina_max, "2.5") // times the weight with none left
QVR_TUNABLE(vr_weight_stamina_curve, "2") // the curve's exponent from the threshold to none (1 straight; 2 gentle at first)
QVR_TUNABLE(vr_weight_stamina_add, "15") // kg added to what a hand holds with none left (on the same curve; with the times above)
QVR_TUNABLE(vr_weight_stamina_empty, "15") // kg an empty hand weighs with none left (on the same curve): it follows its controller as if it held that (0: an empty hand never lags; not a hand on a hold or steadying a weapon)

// Weapons > Fingers and Collisions
QVR_TUNABLE(vr_hand_fit_palm, "5") // with it: cm the drawn hand may move off the controller to sit flush on what it holds (0: stays on the controller)
QVR_TUNABLE(vr_hand_fit_palm_weapon, "3") // the same on a weapon (its own grip, a foregrip, a blade): cm (the weapon is placed in the hand by its settings)
QVR_TUNABLE(vr_hand_fit_palm_turn, "20") // and degrees it may turn to face the surface it holds (0: not at all)
QVR_TUNABLE(vr_hand_fit_overlap, "0.3") // round 21, second pass: cm the fingers and palm may sink into what they hold (snug, no gap); third pass: things only (a weapon, its hotspots: their own overlap sliders)
QVR_TUNABLE(vr_hand_fit_overlap_hands, "0.6") // and cm a hand cupping the other (a two-handed pistol grip) may sink into it
QVR_TUNABLE(vr_hand_walls, "1") // round 21, second pass: the jointed hand's fingers bend (curl) where they would go into walls and floors
QVR_TUNABLE(vr_model_collide_hands, "1") // and so do empty hands (fists)
QVR_TUNABLE(vr_model_collide_max, "20") // cm a weapon or hand is held out of a model; deeper, less and less, and none at twice as deep (it lets go and passes through)
QVR_TUNABLE(vr_hand_fit_blend, "0.12") // seconds the fingers, thumb and palm take to move to a new grasp (0: at once)
QVR_TUNABLE(vr_hand_fit_resolve, "0.3") // cm (and that many times 2 degrees) what the hand holds may move in it before the grasp is solved again (0: every frame)

// Weapons > Hip Holsters
QVR_TUNABLE(vr_hip_holster_pitch, "0")
QVR_TUNABLE(vr_hip_holster_yaw, "40")
QVR_TUNABLE(vr_hip_holster_roll, "0")
QVR_TUNABLE(vr_grenade_pouch_pitch, "0") // degrees: its top away from the back (the holsters' axes)
QVR_TUNABLE(vr_grenade_pouch_yaw, "0")
QVR_TUNABLE(vr_grenade_pouch_roll, "0")

// Weapons > Holster Zones
QVR_TUNABLE(vr_holster_zone_scale, "0.95") // every zone's size, times
QVR_TUNABLE(vr_holster_zone_hysteresis, "0.15") // the zone a hand is at keeps it this much (a share of its size) further out, and wins until another is this much deeper

// Weapons > Hotspots
QVR_TUNABLE(vr_virtual_stock_thresh, "10")
QVR_TUNABLE(vr_shoulder_offset_x, "-1")
QVR_TUNABLE(vr_shoulder_offset_y, "1.75")
QVR_TUNABLE(vr_shoulder_offset_z, "16.0")
QVR_TUNABLE(vr_upper_holster_pitch, "40")
QVR_TUNABLE(vr_upper_holster_yaw, "20")
QVR_TUNABLE(vr_upper_holster_roll, "-60")
QVR_TUNABLE(vr_shoulder_holster_pitch, "0")
QVR_TUNABLE(vr_shoulder_holster_yaw, "0")
QVR_TUNABLE(vr_shoulder_holster_roll, "0")

// Weapons > Immersion
QVR_TUNABLE(vr_shells_life, "20") // seconds they lie on the floor before fading out
QVR_TUNABLE(vr_shells_sound, "1") // volume of their tink as they land (0 off)
QVR_TUNABLE(vr_holster_haptic_time, "0.25") // seconds of the buzz as a hand reaches a holster (vr_holster_haptics 2: once)
QVR_TUNABLE(vr_weapon_throw_velocity_mult, "1.0")
QVR_TUNABLE(vr_weapon_draw_blend, "0.3") // seconds a gun drawn from a holster takes to ease from its holstered pose into the hand (0 at once; visual only)
QVR_TUNABLE(vr_weapon_holster_blend, "0.3") // seconds a gun put in a holster takes to ease from the hand into its holstered pose (0 at once)
QVR_TUNABLE(vr_weapon_pass_blend, "0.1") // seconds a gun passed to the other, empty hand takes to ease from the old hand's hold into the new hand's, following that hand (0 at once; visual only)

// Weapons > Incandescent Blades
QVR_TUNABLE(vr_hotblade_heat_time, "2") // s the blade must stay in a flame to become incandescent (out of it, the build-up falls off as fast)
QVR_TUNABLE(vr_hotblade_damage, "10") // base damage added to every blow of an incandescent weapon (times the blow's strength, as its own base)
QVR_TUNABLE(vr_hotblade_light_radius, "72") // its reach in units at full glow
QVR_TUNABLE(vr_hotblade_ember_rate, "12") // puffs of embers a second at full glow (fewer as it fades: times the glow squared)
QVR_TUNABLE(vr_hotblade_trail_linger, "0.5") // s they linger in the air
QVR_TUNABLE(vr_hotblade_trail_speed, "1.5") // m/s the blade's tip must move for a trail (slower: none)
QVR_TUNABLE(vr_hotblade_hue, "18") // the glow's hue in degrees (0 red, 18 orange-red, 30 orange, 55 yellow)
QVR_TUNABLE(vr_hotblade_sound, "1") // volume of the sizzle in a flame, the ring as it turns incandescent and the quench's hiss (0: off)

// Weapons > Lightning Gun in Water
QVR_TUNABLE(vr_lg_water_self_damage, "40") // the shock's damage to you
QVR_TUNABLE(vr_lg_water_damage, "60") // its damage to what else is in the water, at its middle (the muzzle)
QVR_TUNABLE(vr_lg_water_radius, "300") // how far through the water it (and the electrified water) reaches, units
QVR_TUNABLE(vr_lg_water_falloff, "1") // how it fades out to the radius: the power of the share of the radius left (1 evenly, 2 sooner, 0 not at all)
QVR_TUNABLE(vr_lg_water_tick_damage, "10") // each bolt into water from the open: damage at the point it goes in (fading out the same way)
QVR_TUNABLE(vr_lightning_burn_size, "20") // the lasting burn where lightning strikes a surface: units across (0 none)
QVR_TUNABLE(vr_lightning_burn_gap, "0.25") // at most one burn this often, seconds (a held beam: a bolt every 0.1 s)
QVR_TUNABLE(vr_lightning_burn_stack, "3") // burns on one spot among the last 24; more only as the beam moves on
QVR_TUNABLE(vr_lg_water_arcs, "1") // electrified water's arcs: their width (0 none, the light stays)
QVR_TUNABLE(vr_shock_self_flat_arcs, "3") // struck by lightning, on a flat screen: how many more arcs on your hands and forearms, x (a flat view shows one hand and little arm; a headset's number is left alone)
QVR_TUNABLE(vr_shock_self_flat_hand, "0.14") // on a flat screen: how far the arcs on a hand reach on from its drawn wrist, metres (the free hand is drawn away from where it is tracked)
QVR_TUNABLE(vr_lg_water_flash, "1") // the shock's blue flash over the view and arcs in front of the eyes (0 none .. 1)

// Weapons > Reloading
QVR_TUNABLE(vr_ammo_pouch_counter_x, "2.25")
QVR_TUNABLE(vr_ammo_pouch_counter_y, "0")
QVR_TUNABLE(vr_ammo_pouch_counter_z, "2")
QVR_TUNABLE(vr_ammo_pouch_counter_pitch, "0")
QVR_TUNABLE(vr_ammo_pouch_counter_yaw, "0")
QVR_TUNABLE(vr_ammo_pouch_counter_roll, "0")
QVR_TUNABLE(vr_ammo_pouch_counter_scale, "0.6")
QVR_TUNABLE(vr_ammo_pouch_wobble, "1")
QVR_TUNABLE(vr_ammo_pouch_wobble_freq, "2") // springiness: how fast they sway, Hz (0.2..8)
QVR_TUNABLE(vr_ammo_pouch_wobble_settle, "0.5") // how long a swing takes to die down to 5%, s (0.1..10) (his 0.5, 1.3 before vr_cfg_version 202)
QVR_TUNABLE(vr_ammo_pouch_wobble_intensity, "0.8") // how hard a move swings them (0..5; Wobble caps how far) (his 0.8, 1 before vr_cfg_version 202)
QVR_TUNABLE(vr_reload_haptics, "1") // times the reload's vibrations (taking, putting back, loading, the empty pouch); 0 none
QVR_TUNABLE(vr_reload_volume, "1") // times the reload's sounds; 0 none
QVR_TUNABLE(vr_reload_loose_max, "24") // rounds lying about at most (dropped, thrown): the oldest not in a hand goes for a new one
QVR_TUNABLE(vr_reload_grab_slack, "3") // cm from a round lying about within which a gripping fist takes it (they are small and lie flat)
QVR_TUNABLE(vr_reload_loose_time, "90") // seconds a round lies about before it fades away (0: never)
QVR_TUNABLE(vr_reload_contact_angle, "35")
QVR_TUNABLE(vr_reload_pouch_contact, "4")
QVR_TUNABLE(vr_reload_pouch_angle, "35")
QVR_TUNABLE(vr_reload_pouch_clearance, "4")
QVR_TUNABLE(vr_reload_pouch_mag_contact, "6")
QVR_TUNABLE(vr_reload_pouch_mag_angle, "45")

// Weapons > Reloading - Cell Cords
QVR_TUNABLE(vr_cellcord_laser_x, "0") // the cord's place on the laser cannon off its bottom, the model's units: forward
QVR_TUNABLE(vr_cellcord_laser_y, "0") // left
QVR_TUNABLE(vr_cellcord_laser_z, "0") // up
QVR_TUNABLE(vr_cellcord_hammer_x, "0") // the same on Mjolnir (off the end of its handle)
QVR_TUNABLE(vr_cellcord_hammer_y, "0")
QVR_TUNABLE(vr_cellcord_hammer_z, "0")
QVR_TUNABLE(vr_cellcord_superaxe_x, "0") // the same on the Super Axe (off the end of its handle)
QVR_TUNABLE(vr_cellcord_superaxe_y, "0")
QVR_TUNABLE(vr_cellcord_superaxe_z, "0")

// Weapons > Reloading - Heavy Revolver
QVR_TUNABLE(vr_revolver_close_flick_time, "0.25") // the same for the flick shut
QVR_TUNABLE(vr_revolver_flick_sustain, "0.3") // a flick goes on while the roll stays above this share of its speed (both flicks)
QVR_TUNABLE(vr_revolver_close_push_speed, "0.8") // m/s that push must reach (the hands' relative speed, along the cylinder's way home)
QVR_TUNABLE(vr_revolver_flick_hand_weight, "150") // the flicks' force: degrees a second of roll each m/s of the hand's own motion the same way adds (0: the roll alone)
QVR_TUNABLE(vr_revolver_flick_hand_cap, "1") // and that help at most, times the roll itself (so the hand alone, with no roll, is no force)
QVR_TUNABLE(vr_revolver_eject_travel, "0.05") // out, the hand's motion the way the chambers open, accumulated, must come to this (m), having reached vr_revolver_eject_speed
QVR_TUNABLE(vr_revolver_eject_window, "0.2") // seconds that motion fades over (what it accumulates over)
QVR_TUNABLE(vr_revolver_eject_cone, "60") // a frame further off that way than this adds nothing, resets nothing (degrees)
QVR_TUNABLE(vr_revolver_eject_turn, "600") // and one with the hand turning faster than this (degrees a second: a flick of the wrist adds nothing; 0: any)
QVR_TUNABLE(vr_revolver_spin_mult, "1") // how fast a swipe spins it, times
QVR_TUNABLE(vr_revolver_spin_friction, "760") // degrees a second, each second, a spin slows by (it stops on a chamber)
QVR_TUNABLE(vr_revolver_spin_max, "900") // degrees a second at most
QVR_TUNABLE(vr_revolver_hand_reach, "0.25") // units from the swung-out cylinder's middle (beyond its radius) the other hand spins it or pushes it home within

// Weapons > Reloading - Launchers
QVR_TUNABLE(vr_reload_front_hold_yaw, "0")
QVR_TUNABLE(vr_reload_front_hold_roll, "0")
QVR_TUNABLE(vr_grenade_scale, "0.78")
QVR_TUNABLE(vr_rocket_scale, "0.65")

// Weapons > Reloading - Nailgun
QVR_TUNABLE(vr_reload_pull_speed, "2") // m/s a hand holding a gun's magazine (a two-handed grip) must pull it out of its well at (along it) to take it out; or,
QVR_TUNABLE(vr_reload_pull_snap, "225") // its wrist turning against the gun's hand at this many degrees/s or more; or the hands this much further apart:
QVR_TUNABLE(vr_reload_pull_apart, "10") // units further apart than as it took hold (any one of the three; a gentle pull only holds it)

// Weapons > Reloading - Shotgun
QVR_TUNABLE(vr_ad_widowmaker_shot_volume, "3")

// Weapons > Reloading - Super Shotgun
QVR_TUNABLE(vr_reload_ssg_flick_rest, "180") // degrees a second under which the wrist holding it counts as still: a flick swings the barrel towards its up then (at most 3/4 of the flick's speed; the classic flick reload: 86). The gun hand still moving after a load no longer delays the flick shut (the author's note of 2026-10-08)
QVR_TUNABLE(vr_reload_ssg_lift_speed, "250") // degrees a second it must be moving at
QVR_TUNABLE(vr_reload_ssg_hit_reach, "6") // units from the front 40% of its barrels a hit lands within (a fist, the nearest point of a held prop's box, the other gun's line)
QVR_TUNABLE(vr_reload_ssg_hit_open_speed, "3") // m/s a hit from above must come at to break it open (the hands' relative speed)
QVR_TUNABLE(vr_reload_ssg_hit_open_angle, "40") // degrees off straight down (the gun's own down, as held) it may come from
QVR_TUNABLE(vr_reload_ssg_hit_close_speed, "2.5") // m/s a hit from below must come at to snap it shut
QVR_TUNABLE(vr_reload_ssg_hit_close_angle, "35") // degrees off straight up (the gun's own up) it may come from
QVR_TUNABLE(vr_reload_pouch_sshot_contact, "10")
QVR_TUNABLE(vr_reload_pouch_sshot_angle, "180")

// Weapons > Reloading - Thunderbolt
QVR_TUNABLE(vr_reload_battery_smoke_alpha, "0.45") // each wisp's opacity as it leaves the cell (0..1)

// Weapons > Reloading - Widowmaker
QVR_TUNABLE(vr_ad_widowmaker_tube, "16") // the shells its tube holds
QVR_TUNABLE(vr_ad_widowmaker_handful, "2") // the shells the pouch gives a hand for it (all go in at once, as many as fit; his 2)
QVR_TUNABLE(vr_ad_widowmaker_spin, "1")
QVR_TUNABLE(vr_ad_widowmaker_spinup, "0.75") // (also the wait before the first shot: the barrels must reach firing speed; QC AD_WidowmakerSpinFrame)
QVR_TUNABLE(vr_ad_widowmaker_spindown, "0.9")

// Weapons > Reloading - Plasma Gun
QVR_TUNABLE(vr_ad_plasmagun_cell, "16") // the cells its cell holds (the thunderbolt's holds 36; his 16, 2026-10-31; QC VR_ClipSizeOf)

// Weapons > Weapon Effects
QVR_TUNABLE(vr_autopump_travel, "2.5") // how far back the fore-end goes, in the gun model's units (0-3.2; about 3.8 cm each, drawn)
QVR_TUNABLE(vr_muzzle_flash_enemy_size, "1") // their flashes' size (times a third of their gun's length)
QVR_TUNABLE(vr_tracer_speed, "150") // m/s they fly (times a weapon's Tracer Speed)
QVR_TUNABLE(vr_tracer_length, "1.5") // m long
QVR_TUNABLE(vr_tracer_width, "1.2") // cm thick
QVR_TUNABLE(vr_tracer_chance, "0.3") // the chance a pellet (or a round) shows one, 0..1 (times a weapon's)
QVR_TUNABLE(vr_tracer_r, "1") // their colour (a weapon may have its own), 0..1
QVR_TUNABLE(vr_tracer_g, "0.8")
QVR_TUNABLE(vr_tracer_b, "0.45")
QVR_TUNABLE(vr_tracer_brightness, "1") // times the colour (they add light onto the scene)
QVR_TUNABLE(vr_gunfx_intensity, "1") // how many of them, all (0.25-3; fewer at high fire rates anyway)
QVR_TUNABLE(vr_gunfx_turbulence, "1") // how much the gunfire smoke swirls as it spreads (0 none: straight drift; 0-3)
QVR_TUNABLE(vr_gunfx_shell_trail, "1.6") // every spent casing's smoke trail, times (0 none; 1 the old faint one)
QVR_TUNABLE(vr_gunfx_heat_light_radius, "48") // a glowing barrel's light's reach in units at full heat (vr_hotblade.cpp)
QVR_TUNABLE(vr_gunfx_wisp_pixels, "1.5") // the thinnest they are drawn, in pixels (fainter instead: no shimmer far off; 0.5-8)
QVR_TUNABLE(vr_gunfx_wisp_near, "5") // cm from the eye within which the smoke wisps fade out (from a third of it; 0.5-30; 18 until 2026-11-08: a gun at your mouth hid its smoke)
QVR_TUNABLE(vr_smoke_blow_threshold, "0.015") // blowing on smoke: the microphone's level (RMS, 0..1) that counts as a blow
QVR_TUNABLE(vr_smoke_blow_hold, "0.06") // ... seconds above it before it does
QVR_TUNABLE(vr_smoke_blow_reach, "0.375") // ... metres from your mouth the smoke feels it (fading from half of this)
QVR_TUNABLE(vr_smoke_blow_push, "20") // ... metres a second squared it pushes the smoke the way you face, at twice the threshold (half at the threshold)
QVR_TUNABLE(vr_smoke_blow_fade, "2") // ... extra seconds the smoke ages each second, likewise

// Weapons > Weapon Offsets - Flashlight
QVR_TUNABLE(vr_flashlight_mount_preview, "0") // Weapon Offsets > Flashlight: the torch's place on the page's weapon drawn while the page is shown

// Weapons > Weapon Offsets - Hand and Grip
QVR_TUNABLE(vr_show_controller_x, "0")
QVR_TUNABLE(vr_show_controller_y, "0")
QVR_TUNABLE(vr_show_controller_z, "0")
QVR_TUNABLE(vr_show_controller_pitch, "0")
QVR_TUNABLE(vr_show_controller_yaw, "0")
QVR_TUNABLE(vr_show_controller_roll, "0")
QVR_TUNABLE(vr_show_controller_off_y, "0")
QVR_TUNABLE(vr_show_controller_off_z, "0")
QVR_TUNABLE(vr_show_controller_off_pitch, "0")
QVR_TUNABLE(vr_show_controller_off_yaw, "0")

// Weapons > Weapon Offsets - Muzzle and Sights
QVR_TUNABLE(vr_sight_align_captures, "4")

// Weapons > Weapon Weights
QVR_TUNABLE(vr_weight_drop_2h, "1.5") // times the speed with the other hand on it too
QVR_TUNABLE(vr_weight_drop_window, "0.06") // seconds the turn is measured over
QVR_TUNABLE(vr_heavy, "1") // 0: none of it (every weapon as light)
QVR_TUNABLE(vr_heavy_struggle, "1") // one-handed struggle: 0 off, 1 full (times the Heft)
QVR_TUNABLE(vr_heavy_struggle_stiffness, "0.25") // the wrist's stiffness on it at Heft 1 in one hand, times (lower: lags further, slower back)
QVR_TUNABLE(vr_heavy_struggle_damping, "0.5") // its damping at Heft 1 in one hand, times (lower: springier, overshoots)
QVR_TUNABLE(vr_heavy_droop, "1") // 1: heavy weapons droop held in one hand
QVR_TUNABLE(vr_heavy_droop_deg, "9") // degrees the barrel (head) sags at Heft 1 in one hand (times the weapon's Droop x)
QVR_TUNABLE(vr_heavy_recoil, "1") // 1: heavy weapons fired in one hand kick up (the aim with them)
QVR_TUNABLE(vr_heavy_recoil_kick, "100") // deg/s the muzzle is flung up a shot at Heft 1 in one hand (times the weapon's Heavy Recoil x)
QVR_TUNABLE(vr_heavy_recoil_recovery, "0.5") // the wrist's stiffness on it just after a shot at Heft 1 in one hand, times (slower back)
QVR_TUNABLE(vr_heavy_recoil_time, "0.4") // s the slower recovery fades over
QVR_TUNABLE(vr_heavy_melee_2h, "0.45") // two-handed: the speeds a blow needs at Heft 1, times (a deliberate arc is required instead)
QVR_TUNABLE(vr_heavy_swing_arc, "0.3") // m: the far end's smooth arc a slow two-handed blow needs (steady way, no reversal)
QVR_TUNABLE(vr_heavy_swing_time, "0.12") // s: and how long it must have been under way
QVR_TUNABLE(vr_heavy_swing_strength, "1.3") // the strength of a deliberate two-handed arc at Heft 1 (a blow of strength 1 is a normal one; more with a longer arc)

// Weapons > Weight and Damage
QVR_TUNABLE(vr_weight_damage_exp, "0.375") // the curve's exponent (0: mass changes no damage; 1: in proportion)
QVR_TUNABLE(vr_weight_damage_heavy, "8") // kg: heavier things deal more damage
QVR_TUNABLE(vr_weight_damage_light, "1.5") // kg: lighter things deal less damage
QVR_TUNABLE(vr_weight_damage_min, "0.5") // the least multiplier
QVR_TUNABLE(vr_weight_damage_max, "2.5") // the most multiplier
QVR_TUNABLE(vr_weight_lenient, "0.515") // the exponent (0: off)
QVR_TUNABLE(vr_weight_lenient_from, "5") // kg: lighter things (the fists, the axe, swords, the shotguns, the nailgun) keep the thresholds
QVR_TUNABLE(vr_weight_lenient_min, "0.25") // the least factor

// Combat > Arcane Dimensions' Guns
QVR_TUNABLE(vr_adgun_tank_x, "-9") // ... the tank's middle carried by the chest (body anchor units: forward, as the holsters' offsets)
QVR_TUNABLE(vr_adgun_tank_z, "8") // ... up (times vr_height_calibration)
QVR_TUNABLE(vr_adgun_tank_mass, "3") // ... the tank's mass, kg (a gun dragging it, it dragging the gun)
QVR_TUNABLE(vr_adgun_tank_health, "15") // ... the tank's health (a grunt's): shot, blasted or struck enough it bursts (QC VR_FuelTank_Burst), its gun spent for good; 0: it never bursts (his 15, 30 before vr_cfg_version 202)
QVR_TUNABLE(vr_adgun_tank_burst_radius, "160") // ... its burst's reach at a full tank, units (40% of it near empty; an empty tank only breaks with a puff of smoke)
QVR_TUNABLE(vr_adgun_tank_burst_damage, "30") // ... its burst's blast at its middle at a full tank (as the fuel left; the poison sprayer's half; weaker at the edge)
QVR_TUNABLE(vr_adgun_tank_burst_burn, "6") // ... the flamethrower's: seconds what it reaches burns at a full tank (40% near empty)
QVR_TUNABLE(vr_adgun_tank_burst_poison, "8") // ... the poison sprayer's: the poison it leaves, damage a second at a full tank (40% near empty; weaker at the edge)
QVR_TUNABLE(vr_adgun_tank_burst_poisontime, "6") // ... and for how many seconds at a full tank (40% near empty)
QVR_TUNABLE(vr_adgun_tank_field_time, "6") // ... the field a burst leaves (flames on the ground, a poison cloud: QC VR_TankField_*): seconds at a full tank (40% near empty); 0 none
QVR_TUNABLE(vr_adgun_tank_field_radius, "96") // ... its reach at a full tank, units (40% near empty)
QVR_TUNABLE(vr_adgun_tank_field_damage, "1") // ... its damage times this: the cloud's poison a second (the burst's), the flames' own burning damage a second on the living (vr_burn_damage's) as well as setting them on fire
QVR_TUNABLE(vr_adgun_tank_field_spread, "0.4") // ... seconds it takes to spread from the tank out to its full reach (its look and its damage's reach together); 0 at once
QVR_TUNABLE(vr_adgun_tank_field_flames, "1") // ... the fire field's flames: how many tongues of fire rising off the ground, times this (the poison cloud's green wisps too); 0 none
QVR_TUNABLE(vr_adgun_tank_field_models, "10") // ... the fire field's 3D flames (id's large torch flame, progs/flame2.mdl) at a full tank (40% near empty): thrown out from the tank to random spots in it, fading out over its life; 0 none
QVR_TUNABLE(vr_adgun_tank_field_smoke, "1") // ... the fire field's dark smoke rising off it, times this; 0 none
QVR_TUNABLE(vr_adgun_tank_field_cloud, "1") // ... the poison field's cloud: how many puffs of green smoke over it, times this; 0 none
QVR_TUNABLE(vr_adgun_tank_pickup_time, "0.25") // ... a tank lying about flying to your back as you take its gun, seconds (vr_adtank.cpp); 0 at once
QVR_TUNABLE(vr_adgun_hosecut_speed, "3") // ... the blade's least speed where it meets the hose, m/s (a cut, not a touch)
QVR_TUNABLE(vr_adgun_rocket_ammo, "4") // the rocket soldier's dropped launcher (Quake VR's rockets: vr_dmg_rocket): the rockets it holds (its own: never refilled; the author's 4, 5 before vr_cfg_version 187)
QVR_TUNABLE(vr_adgun_rocket_refire, "1") // ... seconds from a shot to the next
QVR_TUNABLE(vr_adgun_grenade_ammo, "6") // the grenade soldier's dropped launcher (Quake VR's grenades: vr_dmg_grenade): the grenades it holds
QVR_TUNABLE(vr_adgun_grenade_refire, "0.8") // ... seconds from a shot to the next
QVR_TUNABLE(vr_adgun_plasma_damage, "18") // the plasma soldier's dropped rifle: a plasma bolt's hit (AD's monsters' bolts: 15 and up to 5)
QVR_TUNABLE(vr_adgun_plasma_splash, "15") // ... its small blast (0: none; AD's: 20)
QVR_TUNABLE(vr_adgun_plasma_speed, "900") // ... its bolts' speed (AD's soldier's: 600; your Plasma Gun's: 900)
QVR_TUNABLE(vr_adgun_plasma_ammo, "20") // ... the bolts it holds
QVR_TUNABLE(vr_adgun_plasma_refire, "0.35") // ... seconds from a bolt to the next
QVR_TUNABLE(vr_adgun_defender_damage, "4") // the defender's dropped shotgun: a pellet's damage (AD's: 4)
QVR_TUNABLE(vr_adgun_defender_pellets, "10") // ... the pellets of a shot (AD's defender's: 10)
QVR_TUNABLE(vr_adgun_defender_spread, "0.18") // ... how wide they spread (across; half as much up and down: AD's 0.18 by 0.1)
QVR_TUNABLE(vr_adgun_defender_ammo, "8") // ... the shots it holds
QVR_TUNABLE(vr_adgun_defender_refire, "0.9") // ... seconds from a shot to the next
QVR_TUNABLE(vr_adgun_centurion_damage, "18") // the centurion's (and the eliminator's) dropped plasma gun: a bolt's hit
QVR_TUNABLE(vr_adgun_centurion_splash, "15") // ... its small blast (0: none)
QVR_TUNABLE(vr_adgun_centurion_speed, "900") // ... its bolts' speed (AD's centurion's: 600)
QVR_TUNABLE(vr_adgun_centurion_ammo, "30") // ... the bolts it holds
QVR_TUNABLE(vr_adgun_centurion_refire, "0.25") // ... seconds from a bolt to the next
QVR_TUNABLE(vr_adgun_pyro_damage, "4.5") // the pyro's dropped flamethrower: a flame's damage to what it touches (each flame once; AD's: 4; his 4.5, 3 before vr_cfg_version 180)
QVR_TUNABLE(vr_adgun_pyro_burn, "3") // ... seconds what a flame touches burns on (Quake VR's burning: Combat > Burning's Burn Damage a second; 0: none; not under water)
QVR_TUNABLE(vr_adgun_pyro_speed, "600") // ... its flames' speed
QVR_TUNABLE(vr_adgun_pyro_range, "300") // ... how far its flames reach, units (at most about 0.39 times their speed: they slow and die in 0.7 s, as AD's)
QVR_TUNABLE(vr_adgun_pyro_fuel, "100") // ... its fuel: the flames it holds
QVR_TUNABLE(vr_adgun_pyro_rate, "0.1") // ... seconds between its flames, the trigger held
QVR_TUNABLE(vr_adgun_fumigator_damage, "3") // the fumigator's dropped poison sprayer: a puff's damage to what it touches (each puff once; AD's: 2)
QVR_TUNABLE(vr_adgun_fumigator_speed, "500") // ... its puffs' speed
QVR_TUNABLE(vr_adgun_fumigator_range, "260") // ... how far its puffs reach, units (at most about 0.39 times their speed)
QVR_TUNABLE(vr_adgun_fumigator_fuel, "100") // ... its fuel: the puffs it holds
QVR_TUNABLE(vr_adgun_fumigator_rate, "0.1") // ... seconds between its puffs, the trigger held
QVR_TUNABLE(vr_adgun_crossbow_damage, "30") // the crossbow knight's dropped crossbow: a bolt's damage (AD's bolt: 15; positional: a head more)
QVR_TUNABLE(vr_adgun_crossbow_speed, "1050") // ... your bolts' speed: 1.5 times the knights' (AD's on normal skill: 700; his notes 2026-10-19; 700 before vr_cfg_version 161)
QVR_TUNABLE(vr_adgun_crossbow_ammo, "10") // ... the bolts it holds as it drops (its own: never refilled)
QVR_TUNABLE(vr_adgun_crossbow_refire, "1") // ... seconds from a shot to the next (the knight reloads in 2)
QVR_TUNABLE(vr_admelee_sword_damage, "1.25") // ... their swords' damage, times the knight's sword's (vr_sword_damage_mult; the hell knight's: 1.25)
QVR_TUNABLE(vr_admelee_heavy_damage, "1.6") // ... the hammer's and the mace's, times the knight's sword's, held in both hands (their weight: Weapon Weights, slots 45-46)
QVR_TUNABLE(vr_admelee_heavy_1h, "0.5") // ... held in one hand: their damage's share (heavy: one hand swings them slowly and weakly)
QVR_TUNABLE(vr_ad_healthvial_scale, "2.5") // AD support: AD's health vials and gems drawn this many times bigger (QC vr_ad_items.qc item_healthvial; as objects times vr_forcegrabbable_box_scale: 0.5 of AD's model; author's note 2026-10-22: far too small)
QVR_TUNABLE(vr_admelee_heavy_stamina, "1.5") // ... their blows' stamina, times a weapon's (vr_strike_stamina_cost, _2h; QC vr_melee.qc VR_Stamina_Strike; author's note 2026-10-22)
QVR_TUNABLE(vr_admelee_heavy_speed, "0.8") // ... held in either hand, you move at this times the most speed (sv_maxspeed; QC .vr_heavy_move, sv_user.c SV_AirMove)
QVR_TUNABLE(vr_adgun_stream_alpha, "0.25") // AD support: AD's flame and poison sprites' opacity (the pyros' and fumigators' streams, theirs and the player's; 1: AD's) (his 0.25, 0.5 before vr_cfg_version 182)
QVR_TUNABLE(vr_adgun_stream_fire, "2") // AD support: our torches' fire tongues (green: poison) carried with each of AD's flames, every tenth of a second; a poisoned body's green puffs (0: AD's alone, its motes)
QVR_TUNABLE(vr_adgun_fx_rate, "1.5") // AD support: how often our tongues are made along each flame (1..4): each time's spread back along its way, a denser stream without gaps (each time as many as Our Fire With Them) (his 1.5, 1 before vr_cfg_version 182)
QVR_TUNABLE(vr_adgun_fx_embers, "1") // AD support: embers thrown off the flamethrowers' fire and wisps of dark smoke from it, this many times (0..3; 0 none)
QVR_TUNABLE(vr_adgun_fx_mist, "1") // AD support: a faint green mist lingering where the poison sprayers' poison went, this many times (0..3; 0 none)

// Combat > Armed Grunts
QVR_TUNABLE(vr_agrunt_reload_time, "1.5") // seconds a reload takes
QVR_TUNABLE(vr_agrunt_reload_ssg, "0.85") // the double-barrelled shotgun's reload: this share of vr_agrunt_reload_time (his voice note 2026-11-01 18:11: 15% faster)
QVR_TUNABLE(vr_agrunt_disarm_damage, "10") // a blow's least damage there to disarm it
QVR_TUNABLE(vr_agrunt_disarm_swing, "4.5") // m/s a blow (your hand or what it holds) landing on its held weapon must reach to knock it out
QVR_TUNABLE(vr_agrunt_disarm_stagger, "0.4") // s a disarm (a blow, a shot, a throw) staggers it on its feet (0 none)
QVR_TUNABLE(vr_agrunt_disarm_throw, "20") // kg m/s a thrown weapon, prop or gib landing on its held weapon must carry (mass x speed) to knock it out
QVR_TUNABLE(vr_agrunt_down_swing, "3.2") // m/s: lying with its weapon in its hand, a deliberate blow this fast knocks it away (with vr_agrunt_disarm_deliberate; else 0.8)
QVR_TUNABLE(vr_agrunt_strike_beat, "0.4") // a deliberate strike on its weapon or hand into its raised parry: the chance it beats the parry and disarms it (times the two below)
QVR_TUNABLE(vr_agrunt_strike_precise, "4") // units: a strike that met its weapon this near its grip or its hand and forearm is a precise one
QVR_TUNABLE(vr_agrunt_strike_precise_mult, "2") // ... its chance against a raised parry times this
QVR_TUNABLE(vr_agrunt_strike_raised, "0.5") // the share of its parry pose that counts as raised (under it, or not yet up, a deliberate strike disarms it)
QVR_TUNABLE(vr_agrunt_tug_hurt, "0.35") // a grab takes the weapon at once from a grunt down to this share of its health (or staggered, down, stunned)
QVR_TUNABLE(vr_agrunt_tug_hold_max, "6") // s the tug lasts at most: then it wrenches its weapon free (0: no limit)
QVR_TUNABLE(vr_agrunt_tug_ik_stretch, "1.2") // the tug: its arms' IK stretch (vr_agrunt_ik_stretch's) times this while a tug is on, eased in and out with it (its arm reaches for the weapon held in your hand; 1: none)
QVR_TUNABLE(vr_agrunt_grab_slack, "0") // cm: your fist takes its weapon within this of the weapon's drawn surface (vr_physics.cpp handOn; never by its box, nor on its arm or chest)
QVR_TUNABLE(vr_agrunt_grab_sweep, "2") // frames (0..2): your fist is also measured against its weapon's places this many frames back and midway (the weapon as drawn lags the server's; a swung blade moves 5-20 cm a frame)
QVR_TUNABLE(vr_agrunt_fall_impact_speed, "3") // m/s: the impact rule (vr_agrunt_fall_keep_impact) counts only a body that has kept up this speed ...
QVR_TUNABLE(vr_agrunt_fall_impact_time, "0.1") // s: ... for this long (a shove's or your body's one-frame spike never does) ...
QVR_TUNABLE(vr_agrunt_fall_impact_window, "0.085") // s: ... and loses vr_agrunt_fall_keep_impact over this long, on a surface it flew into (a limp body takes about 6 frames at 72 Hz: measured 6.5, 3.8, 4.9, 3.5, 3.9, 2.4, 1.4 m/s on a landing, ROUND23.md)
QVR_TUNABLE(vr_agrunt_fall_impact_held, "0.6") // your hands' bang: your swing must drive the body at this share of vr_agrunt_fall_keep_impact or faster as your hand reaches a surface, having kept up this share of vr_agrunt_fall_impact_speed (your grip drives it at about 5 m/s at most)
QVR_TUNABLE(vr_agrunt_fall_impact_reach, "8") // units: your hands' bang: the swinging hand holding it this near a surface ahead (your hand stops at the wall, the body softly at your hand)
QVR_TUNABLE(vr_agrunt_rag_grab_weapon, "8") // cm: on a living (active) ragdoll your fist this near its weapon's drawn surface takes the weapon, not the limb beside it (the grab query: vr_physics.cpp armedAllowance, box3d grabRagdoll)
QVR_TUNABLE(vr_agrunt_rag_limb_grab, "1") // a grab on a living ragdoll's weapon hand or forearm is a grab on its weapon (a tug); 0: the limb is held, as any
QVR_TUNABLE(vr_agrunt_tug_pull_force, "270") // N (was 200: a still hold peaked at 211): the physical tug's pull that counts: only above it does its grip give (its force x time, the time below)
QVR_TUNABLE(vr_agrunt_tug_snap, "2.5") // a yank this many times the break force tears it out at once, before that time (0: never)
QVR_TUNABLE(vr_agrunt_tug_hurt_ease, "0.4") // how much its wounds weaken its grip (0: not at all; 1: in proportion to its health lost)
QVR_TUNABLE(vr_agrunt_tug_won_stumble, "0.1") // s it stumbles as its weapon is torn away before it stands (as an active ragdoll; then its AI)
QVR_TUNABLE(vr_agrunt_tug_measure, "2") // the physical tug's pull: 0 the hold's joint force, 1 its stretch (your controller from the held point) x vr_agrunt_tug_stiffness, 2 the larger
QVR_TUNABLE(vr_agrunt_tug_stiffness, "1500") // N/m: the stretch's pull (18 cm: 270 N, the least pull that counts)
QVR_TUNABLE(vr_agrunt_tug_two_hand_mult, "0.6") // both hands on its weapon: the second hand's pull counts times this (1: summed; 0.6: two hands win in about 0.6-0.7x one hand's time, test 92)
QVR_TUNABLE(vr_agrunt_tug_two_hand_ease, "0.75") // ... and every threshold of the tug times this (the old tug: its time)
QVR_TUNABLE(vr_agrunt_tug_strike_swing, "0.6") // in a tug, the other fist's strike needs this share of vr_agrunt_disarm_swing (4.5 m/s: 2.7)
QVR_TUNABLE(vr_agrunt_shotdisarm_weapon_w, "1") // a hit on the weapon counts its damage times this
QVR_TUNABLE(vr_agrunt_shotdisarm_hand_w, "1") // a hit on the hand or forearm counts its damage times this (the grunt is hurt as usual)
QVR_TUNABLE(vr_agrunt_shotdisarm_near, "0") // units beyond the volumes a shot still counts, at half (0: on it only)
QVR_TUNABLE(vr_agrunt_shotdisarm_damage, "16") // the damage adding up to a roll (each crossing rolls once, then starts over)
QVR_TUNABLE(vr_agrunt_shotdisarm_window, "0.5") // s the damage adds up over (from the first hit; then it starts over)
QVR_TUNABLE(vr_agrunt_shotdisarm_cooldown, "0.3") // s after a failed roll before hits add up again
QVR_TUNABLE(vr_agrunt_shotdisarm_w_pellet, "1") // the shotguns' pellets count times this
QVR_TUNABLE(vr_agrunt_shotdisarm_w_nail, "1") // the nailguns' nails
QVR_TUNABLE(vr_agrunt_shotdisarm_w_lightning, "0.3") // the lightning gun's bolts (30 a tenth of a second: a roll every 0.4 s held on it)
QVR_TUNABLE(vr_agrunt_shotdisarm_w_blast, "1") // a rocket's or grenade's direct hit on it (its flight through the weapon or hand)
QVR_TUNABLE(vr_agrunt_shotdisarm_w_splash, "0") // an explosion's damage to the grunt (0: splash never counts)
QVR_TUNABLE(vr_agrunt_w_nailpistol, "1") // the nail pistol (one in each hand when it pairs)
QVR_TUNABLE(vr_agrunt_w_revolver, "0.25") // the heavy revolver (rare)
QVR_TUNABLE(vr_agrunt_strike_hips, "0.3") // ... the share of the torso's twist the hips take (the legs stay planted; client)
QVR_TUNABLE(vr_agrunt_strike_head, "0.5") // ... the share of the torso's turn the head takes back (its eyes kept on its target; client)

// Combat > Armed Grunts - AI Behaviour
QVR_TUNABLE(vr_agrunt_parry_damage, "0.25") // share of a parried blow's damage it still takes
QVR_TUNABLE(vr_agrunt_parry_cost, "1") // stamina a parry spends (of vr_agrunt_parry_count)
QVR_TUNABLE(vr_agrunt_parry_count, "2") // its stamina: the parry that spends it all knocks its weapon out of its hand (2: the second parry in a row)
QVR_TUNABLE(vr_agrunt_parry_recover, "0.35") // stamina it recovers a second
QVR_TUNABLE(vr_agrunt_seek_timeout, "6") // s it keeps going for a weapon it doesn't reach (no nearer for 2.5 s: sooner) before giving it up
QVR_TUNABLE(vr_agrunt_seek_shun, "20") // s it leaves a weapon it gave up on before trying it again
QVR_TUNABLE(vr_agrunt_second_range, "192") // units: with Pick Up a Second, out of a fight it goes for a second weapon only this near (its own lost one: vr_agrunt_rearm_range)
QVR_TUNABLE(vr_agrunt_second_time, "3.5") // s: ... and only if the trip (walk and reaching down) is estimated at no more than this, and it gives up on it past this
QVR_TUNABLE(vr_agrunt_second_combat_range, "64") // units: with a target in sight it goes for a second weapon only this near (and only one in the target's sight, so it keeps shooting)
QVR_TUNABLE(vr_agrunt_second_combat_time, "1") // s: ... and only if the trip is estimated at no more than this (its downtime not firing); it gives up on it past this
QVR_TUNABLE(vr_agrunt_friendly_giveup, "2.5") // s: held so long for friends in the way (sidestepping), it fires anyway (0: never gives up)
QVR_TUNABLE(vr_agrunt_saw_start, "44") // units (its middle to yours): an armed humanoid with a chainsaw thrusts it only this near (it closes in first; 62 a swing's, before)
QVR_TUNABLE(vr_enemy_melee_ag_reach, "16") // units: an armed humanoid's blow reaches its weapon's real reach (shoulder, arm, weapon length, your body's front) plus this (its lean and step in; never less than 62); the melee arc decides the rest (vr_armedgrunt.qc VR_AG_BlowBase)
QVR_TUNABLE(vr_agrunt_melee_dmg_fist, "14") // an armed humanoid's punch's damage (before vr_agrunt_damage_scale; all its blows were 20: the six weapons' mean is 20)
QVR_TUNABLE(vr_agrunt_melee_dmg_axe, "22") // ... its axe's, crowbar's, great axe's blow
QVR_TUNABLE(vr_agrunt_melee_dmg_sword, "22") // ... its sword's (times vr_sword_damage_mult, as before)
QVR_TUNABLE(vr_agrunt_melee_dmg_hammer, "26") // ... its hammer's (Mjolnir as a hammer)
QVR_TUNABLE(vr_agrunt_melee_dmg_chainsaw, "18") // ... its chainsaw's swing
QVR_TUNABLE(vr_agrunt_melee_dmg_gunbutt, "18") // ... a gun's butt
QVR_TUNABLE(vr_agrunt_aim_exposed_hold, "0.5") // seconds it keeps aiming at the part it picked while that part stays reachable (then the best reachable one again: the chest first)
QVR_TUNABLE(vr_agrunt_range_scale, "1") // armed humanoids' fighting distances (vr_agrunt_range) times this
QVR_TUNABLE(vr_agrunt_range_walkfire, "50") // units/s: with vr_agrunt_range, an armed humanoid walks while it fires (on, back or round you, by its fighting distance); 0: it stands to fire, as before
QVR_TUNABLE(vr_agrunt_friendly_care, "1") // 0..1: how careful an armed humanoid is not to shoot or strike its friends: the chance it holds fire (and sidesteps) when a monster not its enemy is in its line of fire (0: never, the old way)
QVR_TUNABLE(vr_ogre_friendly_care, "1") // 0..1: how careful an ogre is not to lob or fire into a friend: the chance it holds and sidesteps with one on its shot's way (0: never, the old way; vr_ogre_friends.qc)
QVR_TUNABLE(vr_ogre_friendly_giveup, "2.5") // s: held so long for friends in the way (sidestepping), an ogre fires anyway (0: never gives up)
QVR_TUNABLE(vr_ogre_friendly_step, "0.5") // s: an ogre's sidestep for a clear shot, this to twice this
QVR_TUNABLE(vr_ogre_friendly_spread, "4") // degrees: the lines checked either side of an ogre's shot for friends

// Combat > Armed Grunts - Preventive Parry
QVR_TUNABLE(vr_agrunt_preparry_range, "64") // units (its middle to yours, level): you this near, it may raise its guard (times the category's range)
QVR_TUNABLE(vr_agrunt_preparry_chance, "0.5") // the chance a check raises its guard (by skill as the parry's; by what you hold: odds)
QVR_TUNABLE(vr_agrunt_preparry_interval, "0.5") // s between its checks (you near, its guard down)
QVR_TUNABLE(vr_agrunt_preparry_delay_min, "0.1") // s from a won check to its guard up: the least (times the category's delay, by skill as the parry's reaction)
QVR_TUNABLE(vr_agrunt_preparry_delay_max, "0.35") // ... the most (random between)
QVR_TUNABLE(vr_agrunt_preparry_guard_min, "0.8") // s its guard stays up: the least (times the category's guard)
QVR_TUNABLE(vr_agrunt_preparry_guard_max, "1.6") // ... the most (random between)
QVR_TUNABLE(vr_agrunt_preparry_recover_min, "0.6") // s after its guard drops it can't parry at all (the opening): the least
QVR_TUNABLE(vr_agrunt_preparry_recover_max, "1.2") // ... the most (random between)
QVR_TUNABLE(vr_agrunt_preparry_shove, "1") // 1: its shove cooling down (or off) lets it guard preventively
QVR_TUNABLE(vr_agrunt_preparry_melee, "1") // 1: a melee weapon in its hand lets it guard preventively (its shove ready or not)
QVR_TUNABLE(vr_agrunt_guard_holdfire, "1") // 1: its guard up (or rising) it holds its attack

// Combat > Armed Grunts - Wind-up Parry
QVR_TUNABLE(vr_agrunt_windup_range, "110") // units (its middle to yours, level): your raise read this near (times the category's range)
QVR_TUNABLE(vr_agrunt_windup_high, "0.2") // m: your hand this far over your shoulders (20 cm under your eyes) is raised overhead (times the category's wind-up)
QVR_TUNABLE(vr_agrunt_windup_side, "0.4") // m: your hand this far out to your side (across the way to it) is cocked to the side (times the category's wind-up)
QVR_TUNABLE(vr_agrunt_windup_ahead, "0.15") // m: ... and no further ahead of your head than this (not reaching out)
QVR_TUNABLE(vr_agrunt_windup_back, "0.15") // m: your hand this far behind your head is drawn back (times the category's wind-up)
QVR_TUNABLE(vr_agrunt_windup_low, "0.6") // m: a hand further under your eyes than this is at rest (never cocked to the side or drawn back)
QVR_TUNABLE(vr_agrunt_windup_read, "0.08") // s your raise must be held before it reads it (times the category's wind-up)
QVR_TUNABLE(vr_agrunt_windup_chance, "0.6") // the chance a raise read raises its guard (one roll per raise; by skill; by what you hold: odds)
QVR_TUNABLE(vr_agrunt_windup_delay_min, "0.08") // s from the read to its guard up: the least (times the category's delay, by skill)
QVR_TUNABLE(vr_agrunt_windup_delay_max, "0.3") // ... the most (random between)
QVR_TUNABLE(vr_agrunt_windup_guard_min, "0.6") // s its guard stays up: the least (times the category's guard)
QVR_TUNABLE(vr_agrunt_windup_guard_max, "1.2") // ... the most (random between)
QVR_TUNABLE(vr_agrunt_windup_recover_min, "0.5") // s after its guard drops it can't parry at all (the opening): the least
QVR_TUNABLE(vr_agrunt_windup_recover_max, "1.0") // ... the most (random between)
QVR_TUNABLE(vr_agrunt_windup_rearm, "1") // 1: a raise held is read once (lower it to be read again: bait it, wait it out, strike); 0 read again after each recovery

// Combat > Armed Grunts - Reactive Parry
QVR_TUNABLE(vr_agrunt_parry_react_rand, "0") // s: the reactive parry's reaction (vr_agrunt_parry_react) plus a random 0 to this

// Combat > Armed Grunts - Parry by What You Hold
QVR_TUNABLE(vr_agrunt_pcat_melee_chance, "1.5") // you holding a melee weapon: its parries' chances times this, as odds: 1 - (1 - c)^m (never and always kept)
QVR_TUNABLE(vr_agrunt_pcat_melee_delay, "0.7") // you holding a melee weapon: its parries' delays (the reactive parry's reaction too) times this
QVR_TUNABLE(vr_agrunt_pcat_melee_guard, "1.25") // you holding a melee weapon: its guard's time times this
QVR_TUNABLE(vr_agrunt_pcat_melee_range, "1.25") // you holding a melee weapon: the preventive and wind-up ranges times this
QVR_TUNABLE(vr_agrunt_pcat_melee_windup, "0.85") // you holding a melee weapon: the wind-up's thresholds (height, side, back, read time) times this: above 1 a plainer raise
QVR_TUNABLE(vr_agrunt_pcat_melee_pre, "1") // you holding a melee weapon: the preventive parry's chance besides (odds): under 1 it waits for your wind-up
QVR_TUNABLE(vr_agrunt_pcat_gun_chance, "0.6") // you holding a gun: its parries' chances times this, as odds: 1 - (1 - c)^m (never and always kept)
QVR_TUNABLE(vr_agrunt_pcat_gun_delay, "1.5") // you holding a gun: its parries' delays (the reactive parry's reaction too) times this
QVR_TUNABLE(vr_agrunt_pcat_gun_guard, "0.8") // you holding a gun: its guard's time times this
QVR_TUNABLE(vr_agrunt_pcat_gun_range, "0.85") // you holding a gun: the preventive and wind-up ranges times this
QVR_TUNABLE(vr_agrunt_pcat_gun_windup, "1.3") // you holding a gun: the wind-up's thresholds (height, side, back, read time) times this: above 1 a plainer raise
QVR_TUNABLE(vr_agrunt_pcat_gun_pre, "0.25") // you holding a gun: the preventive parry's chance besides (odds): under 1 it waits for your wind-up
QVR_TUNABLE(vr_agrunt_pcat_prop_chance, "0.75") // you holding a prop: its parries' chances times this, as odds: 1 - (1 - c)^m (never and always kept)
QVR_TUNABLE(vr_agrunt_pcat_prop_delay, "1.3") // you holding a prop: its parries' delays (the reactive parry's reaction too) times this
QVR_TUNABLE(vr_agrunt_pcat_prop_guard, "0.9") // you holding a prop: its guard's time times this
QVR_TUNABLE(vr_agrunt_pcat_prop_range, "0.9") // you holding a prop: the preventive and wind-up ranges times this
QVR_TUNABLE(vr_agrunt_pcat_prop_windup, "1.15") // you holding a prop: the wind-up's thresholds (height, side, back, read time) times this: above 1 a plainer raise
QVR_TUNABLE(vr_agrunt_pcat_prop_pre, "0.5") // you holding a prop: the preventive parry's chance besides (odds): under 1 it waits for your wind-up
QVR_TUNABLE(vr_agrunt_pcat_fist_chance, "0.9") // you holding fists: its parries' chances times this, as odds: 1 - (1 - c)^m (never and always kept)
QVR_TUNABLE(vr_agrunt_pcat_fist_delay, "1.15") // you holding fists: its parries' delays (the reactive parry's reaction too) times this
QVR_TUNABLE(vr_agrunt_pcat_fist_guard, "1") // you holding fists: its guard's time times this
QVR_TUNABLE(vr_agrunt_pcat_fist_range, "0.9") // you holding fists: the preventive and wind-up ranges times this
QVR_TUNABLE(vr_agrunt_pcat_fist_windup, "1.1") // you holding fists: the wind-up's thresholds (height, side, back, read time) times this: above 1 a plainer raise
QVR_TUNABLE(vr_agrunt_pcat_fist_pre, "0.6") // you holding fists: the preventive parry's chance besides (odds): under 1 it waits for your wind-up

// Combat > Armed Grunts - Blade Contact
QVR_TUNABLE(vr_agrunt_blade_hand_zone, "6") // units: a blow on a melee weapon met further than this from its hand and forearm is on its blade (sparks, the clang, no disarm, no damage); nearer, the hand's disarm rules
QVR_TUNABLE(vr_agrunt_blade_least, "1.5") // m/s: a blow on its blade slower than this is no contact
QVR_TUNABLE(vr_agrunt_blade_stagger_speed, "6") // m/s: a blow on its blade this fast staggers it, its weapon arm knocked along the blow (0: never)
QVR_TUNABLE(vr_agrunt_blade_stagger_time, "0.5") // s that stagger lasts
QVR_TUNABLE(vr_agrunt_blade_arm, "0.5") // the knock on its weapon arm: the hit reaction's share of its health per 10 m/s of the blow (0: none)
QVR_TUNABLE(vr_agrunt_blade_fx, "1") // the contact's sparks and sound times this (0: none)

// Combat > Directional Parry (QC vr_dirparry.qc)
QVR_TUNABLE(vr_dirparry_partial, "0") // under the threshold, the blow cut by this x score / threshold (0: off, the full blow)
QVR_TUNABLE(vr_dirparry_opening, "0.5") // s: its flinch when the parry balance gave no stagger (its swing stopped: the opening)
QVR_TUNABLE(vr_dirparry_angle, "40") // Directional Parry: degrees your weapon may be off square to its blade and its way and still count fully
QVR_TUNABLE(vr_dirparry_angle_zero, "90") // ... degrees off where the angle's marks reach none (from full at vr_dirparry_angle, falling evenly)
QVR_TUNABLE(vr_dirparry_contact, "6") // ... units: its blade's line and your weapon's within this meet (full marks)
QVR_TUNABLE(vr_dirparry_falloff, "2") // ... the meeting's marks fall to none at this times the contact distance
QVR_TUNABLE(vr_dirparry_threshold, "0.75") // ... the score (angle x meeting, 0..1) that parries
QVR_TUNABLE(vr_dirparry_window, "0.1") // ... s: at its blow, its blade's way followed on this long (where it would meet your weapon in the rest of the swing)
QVR_TUNABLE(vr_dirparry_through_armed, "0") // ... share of a parried blow that still hurts you: an armed humanoid's
QVR_TUNABLE(vr_dirparry_through_monster, "0") // ... a monster's own
QVR_TUNABLE(vr_dirparry_timed_window, "0.3") // ... s: well timed: your weapon became a valid parry (moved into its way, or turned to the angle) at most this long before the contact
QVR_TUNABLE(vr_dirparry_stroke, "0.45") // ... s: a monster's own blow (its line read from its frames): its blow held back at most this long for its blade to get to your weapon

// Combat > Armed Humanoid Replacements
QVR_TUNABLE(vr_agrunt_repl_skill0, "0.5") // the chances above times this on Easy
QVR_TUNABLE(vr_agrunt_repl_skill1, "1") // ... on Normal
QVR_TUNABLE(vr_agrunt_repl_skill2, "1.5") // ... on Hard
QVR_TUNABLE(vr_agrunt_repl_skill3, "2") // ... on Nightmare

// Combat > Batting and Catching
QVR_TUNABLE(vr_bash_deflect_radius, "24") // units around the guard (the weapon, the hands) a bash bats projectiles back within (vr_deflect)
QVR_TUNABLE(vr_deflect_radius, "14") // units around the weapon's line (or fist) a projectile is batted within
QVR_TUNABLE(vr_deflect_wrist, "2") // m/s the hand itself (meleewristspeed: its turn taken out) must move at to bat (vr_deflect_swing)
QVR_TUNABLE(vr_deflect_run, "0.2") // m a batting swing must have come from (meleerun; vr_deflect_swing)
QVR_TUNABLE(vr_deflect_bounce, "0.6") // a batted projectile's restitution off the weapon (0 dead .. 1 elastic): a little livelier than a ball off a bat
QVR_TUNABLE(vr_deflect_aim_assist, "0.5") // 0..1: a batted or bashed projectile going very near a monster (its thrower most) bends that share towards it
QVR_TUNABLE(vr_grenade_held_fuse, "2.5") // s a caught grenade's fuse is set to (never shortened) as a hand takes it
QVR_TUNABLE(vr_grenade_catch_radius, "15") // cm round the palm an empty hand, not striking, catches a grenade in flight in (grip held or closing; QC vr_grenade.qc)
QVR_TUNABLE(vr_grenade_catch_window, "0.15") // s an open palm holds a grenade that flies into it for the grip to close on it (else it drops from the hand)
QVR_TUNABLE(vr_grenade_shoot_pad, "2") // units a grenade's shot box reaches beyond its model's half size (0: the model's box as it is)
QVR_TUNABLE(vr_grenade_melee_speed, "20") // m/s a punch, a gun's swing, a pommel or a butt must go at to set a grenade off (slower: it bats it)
QVR_TUNABLE(vr_grenade_melee_speed_weapon, "34") // m/s for a blade's or a head's blow (a sword, the axe, the crowbar, a club, a stopped chainsaw: swung faster); a running chainsaw always sets it off
QVR_TUNABLE(vr_grenade_melee_damage, "10") // and the least damage the blow must deal (0: the speed alone)
QVR_TUNABLE(vr_handgrenade_fuse, "2.5") // s from arming a hand grenade to its going off (the launcher's is 2.5)

// Combat > Bullet Time
QVR_TUNABLE(vr_bullettime_min, "0.25") // the least meter it starts with (0.01 .. 1)
QVR_TUNABLE(vr_bullettime_fx_desat, "0.7") // ... how much of the colour it takes out (0 .. 1)
QVR_TUNABLE(vr_bullettime_fx_vignette, "0.5") // ... how dark the view's edges (0 .. 1)
QVR_TUNABLE(vr_bullettime_tap_double_window, "0.8") // seconds: a double tap's second tap must come this soon after its first (the volume lifted off the screen between them)
QVR_TUNABLE(vr_bullettime_tap_min_gap, "0.05") // seconds: a double tap's second tap no sooner than this after its first (sooner: a new first tap; an arm wiggled or flicked fast is no double tap)
QVR_TUNABLE(vr_bullettime_tap_double_speed, "0.2") // m/s: a double tap's least speed into the screen, per tap (instead of vr_bullettime_tap_speed)
QVR_TUNABLE(vr_bullettime_tap_speed, "0.5") // m/s: the least speed into the screen (the hands' speed towards each other along its normal: both may move)
QVR_TUNABLE(vr_bullettime_tap_angle, "80") // degrees: how far off straight into the screen the strike may come (a swing across it comes from the side)
QVR_TUNABLE(vr_bullettime_tap_margin, "0.5") // cm round the screen's edges where the strike may land too
QVR_TUNABLE(vr_bullettime_tap_depth, "5") // cm over the screen's face at which the striking volume (the drawn hand's surface, the gun's butt) is on it
QVR_TUNABLE(vr_bullettime_tap_width, "0.95") // the tap zone's width as a share of the screen's (before vr_bullettime_tap_margin) ...
QVR_TUNABLE(vr_bullettime_tap_height, "0.85") // ... and its height
QVR_TUNABLE(vr_bullettime_tap_x, "0") // cm: the tap zone moved along the screen's right ...
QVR_TUNABLE(vr_bullettime_tap_y, "0") // ... its up ...
QVR_TUNABLE(vr_bullettime_tap_z, "-3.5") // ... and out of it (its face: vr_bullettime_tap_depth is measured from there)
QVR_TUNABLE(vr_bullettime_tap_butt_depth, "6") // cm: how far from the gun's rearmost end its butt reaches (the drawn points within it strike)
QVR_TUNABLE(vr_bullettime_tap_stop, "1") // the impact: the speed into the screen must fall to this much of its peak on the screen (a hand passing by doesn't tap); 1: on arrival
QVR_TUNABLE(vr_bullettime_tap_window, "0.5") // seconds after the peak speed the impact may come
QVR_TUNABLE(vr_bullettime_haptic, "1") // the tick in both hands when a tap counts (0 off .. 2)
QVR_TUNABLE(vr_bullettime_tap_glitch, "1") // ... and the gadget's screen glitching for a moment (bands torn sideways, the colours split, static): how hard (0: none .. 2; a double tap's first a little less)
QVR_TUNABLE(vr_bullettime_tap_glitch_time, "0.12") // ... for this many real seconds (the tap that starts or stops it a quarter longer)
QVR_TUNABLE(vr_bullettime_sound_off_max, "1") // real seconds the "off" sound plays at most, then fades out over 0.25 s (it is cut as bullet time starts again; 0: the whole sound)
QVR_TUNABLE(vr_bullettime_trails_strength, "1.25") // how far they bend the view (0 .. 3; 1: up to about the trail's half width)
QVR_TUNABLE(vr_bullettime_trails_life, "2") // real seconds a trail's every point lasts, fading
QVR_TUNABLE(vr_bullettime_trails_length, "15") // metres: the longest a trail is behind its projectile
QVR_TUNABLE(vr_bullettime_trails_width, "8") // cm wide (a bullet's and a nail's; rockets and grenades twice, enemies' balls 1.5 times)
QVR_TUNABLE(vr_bullettime_trails_fade, "1") // real seconds they fade out over after bullet time ends
QVR_TUNABLE(vr_bullettime_sound_on, "items/inv1.wav") // its sounds (empty: none)
QVR_TUNABLE(vr_bullettime_sound_off, "items/inv2.wav")
QVR_TUNABLE(vr_bullettime_sound_denied, "misc/menu2.wav") // pressed with the meter too low or cooling down

// Combat > Burning
QVR_TUNABLE(vr_walltorch_lean, "1") // how far a swung torch's flame leans and trails behind its motion, times the default (0: always straight up)
QVR_TUNABLE(vr_walltorch_flatten, "1") // how much a fast-swung torch's flame flattens and stretches back, times the default (0: never)
QVR_TUNABLE(vr_walltorch_hand_motion, "3") // strength of hand motion relative to locomotion in the flame's lean/stretch
QVR_TUNABLE(vr_walltorch_motion_smooth, "0.08") // seconds of flame velocity smoothing
QVR_TUNABLE(vr_walltorch_inv_size, "0.15") // fully inverted flame height; clamp 0.05..0.3 keeps it short and visible
QVR_TUNABLE(vr_walltorch_inv_light, "1.3") // a torch held upside down: its light's brightness, times the upright one's
QVR_TUNABLE(vr_walltorch_smoke, "1") // smoke off every lit torch (on its wall, held, lying), times the default (3 puffs a second; 0: none)
QVR_TUNABLE(vr_walltorch_smoke_alpha, "0.45") // how opaque a torch's smoke is (0..1)
QVR_TUNABLE(vr_walltorch_inv_smoke, "2.5") // a torch held upside down smokes this much more
QVR_TUNABLE(vr_burn_damage, "4") // damage a second a fire does, however many flames or hits (0: nothing alive is set on fire)
QVR_TUNABLE(vr_burn_time, "3") // s a monster burns from the last hit that lit it
QVR_TUNABLE(vr_burn_flames, "4") // flames a fire spreads to over the body on its own (1: just the first)
QVR_TUNABLE(vr_burn_flames_max, "12") // most flames on one body: more hits add one each up to this (the damage stays one fire's; 7 before config 89)
QVR_TUNABLE(vr_burn_spread, "0.5") // s between flames spreading
QVR_TUNABLE(vr_burn_flame_size, "0.8") // the first flame's scale (id's small flame, progs/flame2.mdl)
QVR_TUNABLE(vr_burn_flame_small, "0.7") // the other flames' size, times the first's (each a little more or less)
QVR_TUNABLE(vr_burn_flame_tilt, "12") // degrees the other flames lean, at most (upright: 0)
QVR_TUNABLE(vr_burn_corpse_time, "8") // s a corpse burns
QVR_TUNABLE(vr_burn_corpse_damage, "0") // a burning corpse's damage, times a monster's (it gibs at its corpse health; 0: it just burns)
QVR_TUNABLE(vr_burn_lava_player, "3") // s: lava sets you on fire, and you burn on this long after getting out (in it only its own damage; not with the pentagram or the biosuit); 0: not
QVR_TUNABLE(vr_burn_crate_time, "10") // s a crate burns before it breaks (vr_burn_crate_break) or goes out
QVR_TUNABLE(vr_burn_crate_spread, "3") // s a burning crate takes to set the crates touching it on fire (0: fire doesn't spread)
QVR_TUNABLE(vr_burn_crate_gap, "2") // units apart two crates still count as touching
QVR_TUNABLE(vr_burn_crate_flames, "6") // flames a crate's fire spreads to (Most Flames caps it)
QVR_TUNABLE(vr_burn_crate_flame_size, "1.25") // a crate's flames' size, times a body's
QVR_TUNABLE(vr_burn_piece_time, "5") // s a piece burns before it burns away (charred from half of it)
QVR_TUNABLE(vr_burn_piece_gap, "8") // units apart a burning piece and a piece or a crate catch from each other
QVR_TUNABLE(vr_burn_piece_flames, "3") // flames a piece's fire spreads to (Most Flames caps it)
QVR_TUNABLE(vr_burn_piece_flame_size, "0.7") // a piece's flames' size, times a body's
QVR_TUNABLE(vr_burn_nail_reach, "10") // units from a torch's flame a nail must pass to catch fire
QVR_TUNABLE(vr_burn_sound, "0.6") // the volume of the crackle a burning body, crate or piece makes (ambience/fire1.wav looped, heard near; QC vr_burning.qc VR_Burn_Crackle; 0 none)
QVR_TUNABLE(vr_burn_sound_max, "4") // at most this many burning things crackle at once (the rest burn silent till one goes out)
QVR_TUNABLE(vr_burn_self_haptic, "1") // the warnings' strength (the hand buzzing as the flame stays on you; 0: none)
QVR_TUNABLE(vr_burn_drop_time, "1") // s the flame of a torch held upside down must stay on its hand before it is dropped
QVR_TUNABLE(vr_smoulder_burn_time, "4") // s a burnt body smokes on after its flames go out, thinning (it smokes while they burn)

// Combat > Chainsaw Engine
QVR_TUNABLE(vr_chainsaw_smoke, "12") // puffs of exhaust smoke a second (0: none)
QVR_TUNABLE(vr_chainsaw_smoke_alpha, "0.6") // each puff's opacity as it leaves the exhaust (0..1), fading out over 2 s
QVR_TUNABLE(vr_chainsaw_shake, "2.5") // mm the hand holding it shakes, held in one hand (and 0.2 degrees a mm); its chain running 1.6x
QVR_TUNABLE(vr_chainsaw_shake_2h, "1.25") // mm, held in both hands (the other steadying it)
QVR_TUNABLE(vr_chainsaw_shake_ground, "0.85") // m/s of the kicks (20 a second) that rock and walk it about as it lies running (0: none)
QVR_TUNABLE(vr_chainsaw_pull_shake, "3") // mm the chainsaw's hand shakes at the pull (as Shake: 0.2 degrees a mm), dying out (0: none)
QVR_TUNABLE(vr_chainsaw_pull_shake_time, "0.35") // seconds it shakes for
QVR_TUNABLE(vr_chainsaw_pull_smoke, "12") // puffs of exhaust smoke at the pull (as Smoke's; 0: none)
QVR_TUNABLE(vr_chainsaw_pull_sparks, "24") // tiny sparks out of the exhaust at the pull (0: none)
QVR_TUNABLE(vr_chainsaw_cord_hint, "1") // 0: never; 1: until you first pull a cord (remembered by the game, its saves and levels); 2: always

// Combat > Damage and Knockback
QVR_TUNABLE(vr_hit_tolerance_guns, "2") // units the model is grown by for shots and projectiles (nails, rockets, grenades, lasers...)
QVR_TUNABLE(vr_hit_tolerance_grapple, "2") // for the grappling hook
QVR_TUNABLE(vr_hit_melee_scale, "1") // Combat > Hit Detection > Melee Hitbox Size: the melee probe (vr_hit_tolerance_melee) times this, 0.25..4. Not the striking points' own 3 units, not the broad-phase box.
QVR_TUNABLE(vr_hit_tolerance_thrown, "2") // for thrown things (on top of their hit box, vr_throw_hitbox: 6)
QVR_TUNABLE(vr_pain_haptics, "1") // strength of the buzz a hit gives each controller (more the harder and the nearer its side; 0: none)

// Combat > Death Sergeant's Flail
QVR_TUNABLE(vr_flail_mass, "3") // ... its spiked head's mass, kg (its swing's weight; its blows' damage goes by it; 2 before vr_cfg_version 194)
QVR_TUNABLE(vr_flail_damage, "3") // ... its head's blow: this times its mass (kg) times its speed into what it hits beyond vr_flail_min_speed (m/s)
QVR_TUNABLE(vr_flail_min_speed, "3") // ... its head hitting slower than this (m/s) deals nothing (a lazy wave), only knocks
QVR_TUNABLE(vr_flail_max_damage, "100") // ... a blow's damage at most (150 before vr_cfg_version 194) (his 100, 120 before vr_cfg_version 202)
QVR_TUNABLE(vr_flail_min_damage, "10") // ... and at least, a blow faster than vr_flail_min_speed (the author's note vrfiringrange_2026-10-23_17-33-56)
QVR_TUNABLE(vr_flail_max_speed, "12") // ... its head never faster than this, m/s (vr_box3d_flail.cpp; was a fixed 40: whirled up without end) (his 12, 16 before vr_cfg_version 202)
QVR_TUNABLE(vr_flail_drag, "0.6") // ... the air slowing its head (Box3D's linear damping, a second's share; its spin 1 + 4x it; was 0.05): whirling it takes work
QVR_TUNABLE(vr_flail_2h, "1.3") // ... held in both hands (the other on its handle): its blows this many times harder
QVR_TUNABLE(vr_flail_rattle, "0.7") // ... its chain's rattle (AD's gibs/gibchain.wav) as its head swings fast and strikes: its volume (0 none)
QVR_TUNABLE(vr_flail_inertia, "0.2") // ... its head carried along by the player's own movement (the stick's walk and turn, a teleport, a push, a lift: vr_box3d_flail.cpp carryWithPlayer): this share of his body's speeding up and slowing down still swings it (0 none, 1 all: a hard stop flings it on)

// Combat > Enemy Melee
QVR_TUNABLE(vr_enemy_melee_arc_angle, "65") // Combat > Enemy Melee > Arc Angle: the arc's half-angle (degrees each side of the monster's facing) for a grunt-sized monster (24 units wide: a knight, a death knight, a rottweiler)
QVR_TUNABLE(vr_enemy_melee_arc_wide, "20") // Combat > Enemy Melee > Wide Swings: degrees more each side for sweeping blows (an ogre's chainsaw sweep, a shambler's or minotaur's claw)
QVR_TUNABLE(vr_enemy_melee_arc_size, "0.1") // Combat > Enemy Melee > Arc Growth with Size: share more Arc Angle per grunt width more of the monster's width (an ogre 40 units: 65 x 1.067 = 69 deg; a shambler 64: 76 deg); 0: the same for all (fight.qc VR_MeleeArc_Angle)
QVR_TUNABLE(vr_enemy_melee_reach_size, "0.1") // Combat > Enemy Melee > Reach Growth with Size: share more reach (Quake's own: 60 a knight's sword, 100 an ogre's chainsaw) per grunt width more (an ogre: 107, a shambler 117); 0: Quake's for all (fight.qc VR_MeleeReach)
QVR_TUNABLE(vr_enemy_melee_turn, "6") // Combat > Enemy Melee > Swing Turn: degrees a frame (10 a second) a monster turns after you while swinging (its own speed if lower; 0: its own speed)
QVR_TUNABLE(vr_enemy_melee_kind_shove, "0.923") // Combat > Enemy Melee > Shove Cone: a shove's cone as a share of the melee arc (grunts, enforcers, vores; 0.923: its old 60 deg at the default 65 arc; fight.qc VR_MeleeArc_ShoveAngle)
QVR_TUNABLE(vr_enemy_aim_angle, "20") // Combat > Enemy Melee > Aim Cone: degrees each side of its facing a shot may bend toward you (hitscan and fast shots; lobbed and slow ones times vr_enemy_aim_wide)
QVR_TUNABLE(vr_enemy_aim_fire, "40") // Combat > Enemy Melee > Fire Cone: degrees each side of its facing you must be inside for it to shoot at once (else it waits, turning; times the kind's multiplier too)
QVR_TUNABLE(vr_enemy_aim_delay, "0.3") // Combat > Enemy Melee > Max Delay: seconds a shot waits at most with you outside its fire cone (turning after you meanwhile), then fires clamped; 0: no wait

// Body > Player Footsteps (vr_playersteps.cpp)
QVR_TUNABLE(vr_footstep_player_volume, "0.85") // all the steps on the drawn feet, times this
QVR_TUNABLE(vr_footstep_player_walk_vol, "0.075") // a step's volume at the walking speed or slower
QVR_TUNABLE(vr_footstep_player_run_vol, "0.35") // ... at the running speed or faster
QVR_TUNABLE(vr_footstep_player_walk_speed, "60") // units/s: a step this slow or slower has the walking volume
QVR_TUNABLE(vr_footstep_player_run_speed, "320") // units/s: this fast or faster the running volume
QVR_TUNABLE(vr_footstep_player_curve, "1.4") // how the volume rises from walk to run: 1 evenly, higher staying quiet longer
QVR_TUNABLE(vr_footstep_player_adjust_vol, "0.15") // the body's small adjustments (standing steps, slow room moves), this volume (0: silent)
QVR_TUNABLE(vr_footstep_player_adjust_gap, "0.6") // s: at most one adjustment's sound in this long
QVR_TUNABLE(vr_footstep_player_room_speed, "60") // units/s: the walk's steps with the stick still (moving in the room) slower than this are adjustments

// Combat > Enemy Footsteps (vr_footsteps.cpp)
QVR_TUNABLE(vr_footstep_volume, "1") // all of them, times this (0: none)
QVR_TUNABLE(vr_footstep_walk_speed, "30") // units/s: a pace this slow or slower is a walk's volume and carry
QVR_TUNABLE(vr_footstep_run_speed, "150") // units/s: this fast or faster a run's
QVR_TUNABLE(vr_footstep_walk_vol, "0.25") // a walking step's volume (times its kind's weight)
QVR_TUNABLE(vr_footstep_run_vol, "0.6") // a running step's volume
QVR_TUNABLE(vr_footstep_walk_atten, "1.6") // how fast a walking step dies with distance (Quake's ATTN_NORM is 1)
QVR_TUNABLE(vr_footstep_run_atten, "1") // a running step's
QVR_TUNABLE(vr_footstep_min_speed, "6") // units/s: slower is no step (a shift of weight on the spot)
QVR_TUNABLE(vr_footstep_heavy_volume, "1") // the heavy feet (ogres, fiends, shamblers), times this as well
QVR_TUNABLE(vr_footstep_heavy_speed, "1.25") // their playback rate, times this: pitch and length together (on each kind's own pitch and the jitter)
QVR_TUNABLE(vr_footstep_heavy_carry, "0.6") // their distance counted times this: heard from further, over nearer boots
QVR_TUNABLE(vr_footstep_nearest, "6") // only the nearest this many of those moving are heard (0-16)
QVR_TUNABLE(vr_footstep_range, "1000") // units from a player: farther isn't heard or counted
QVR_TUNABLE(vr_footstep_rate, "14") // steps a second at most, all together
QVR_TUNABLE(vr_footstep_log, "0") // 1: a console line a step (the pace, volume, its error against the drawn contact)

// Combat > Enemy Shoves
QVR_TUNABLE(vr_enemy_shove_parry_reduction, "0.75") // share of a shove's push a parry takes off (0: none, 1: all; was vr_enemy_shove_parried, the share left)
QVR_TUNABLE(vr_enemy_shove_enforcer_damage, "1.5") // an enforcer's shove's damage, times vr_enemy_shove_damage
QVR_TUNABLE(vr_enemy_shove_enforcer_distance, "1.5") // an enforcer's shove's push, times vr_enemy_shove_distance
QVR_TUNABLE(vr_enemy_shove_vore_damage, "2") // a vore's shove's damage, times vr_enemy_shove_damage
QVR_TUNABLE(vr_enemy_shove_vore_distance, "2") // a vore's shove's push, times vr_enemy_shove_distance (harder than anyone's)
QVR_TUNABLE(vr_agrunt_shove_close, "24") // a gun-holding armed humanoid, you this many units beyond its shove's reach (vr_enemy_shove_range) or nearer: it steps in and shoves (its shove ready) instead of backing off; 0 it always backs off (as before; QC vr_armedgrunt.qc VR_AG_ShoveCloseIn)
QVR_TUNABLE(vr_agrunt_armor_crack_volume, "2.5") // an armour plate's crack as it breaks (vr_agarmor.qc; 0..4, over 1 louder than full)
QVR_TUNABLE(vr_agrunt_armor_crack_attn, "0.5") // and how fast it fades with distance (1: as other sounds; 0: everywhere)

// Combat > Enemy Weapons
QVR_TUNABLE(vr_chainsaw_fuel_use, "5") // fuel (percent of a tank) a second the chain uses, the trigger held
QVR_TUNABLE(vr_chainsaw_idle_fuel_use, "0.5") // and the engine idling
QVR_TUNABLE(vr_chainsaw_overlap, "6") // cm the running chain's bar may sink into a monster's model (vr_modelcollide.cpp)
QVR_TUNABLE(vr_chainsaw_start_chance, "0.5") // the chance a good pull of the cord starts the engine
QVR_TUNABLE(vr_chainsaw_fail_pulls_min, "1") // the first good pulls of a start that always fail: at least this many,
QVR_TUNABLE(vr_chainsaw_fail_pulls_max, "2") // up to this many (drawn at random each start); then Start Chance
QVR_TUNABLE(vr_chainsaw_pull_distance, "30") // cm the cord must be pulled out for a pull (vr_chainsaw.cpp)
QVR_TUNABLE(vr_chainsaw_pull_speed, "1.2") // m/s the hand must pull it at, there
QVR_TUNABLE(vr_chainsaw_drop_fuel_min, "40") // an ogre's dropped chainsaw has this much fuel (percent of a tank) or more, up to a full tank
QVR_TUNABLE(vr_enemy_weapon_drop_max, "48") // most single-use enemy weapons lying about (grunts' burst rifles, enforcers' laser rifles, ogres' chainsaws): past it the oldest fades away (not one in a hand, pulled or in flight); 0: no limit
QVR_TUNABLE(vr_chainsaw_drop_run, "3") // seconds a running chainsaw dropped or thrown runs on (QC VR_Saw_PropRuns); taken back meanwhile, still running. 0: it stops at once
QVR_TUNABLE(vr_gruntgun_ammo, "30") // the rounds a grunt's dropped burst rifle holds (its own: never refilled; a multiple of 3)
QVR_TUNABLE(vr_grunt_burst_damage, "5") // a grunt's burst: each round's damage to what it hits (id's shotgun: 4 pellets of 4)
QVR_TUNABLE(vr_enfrifle_speed, "1800") // an enforcer's dropped laser rifle: its lasers' speed, units a second (the enforcer's own: 600)
QVR_TUNABLE(vr_enfrifle_ammo, "20") // the shots an enforcer's dropped rifle holds (its own: never refilled)
QVR_TUNABLE(vr_enemygun_spent_volume, "0.5") // the volume of the corpses' crackle (misc/power.wav) as it starts (0: silent)

// Combat > Holding Enemies
QVR_TUNABLE(vr_foegrab_drag_speed, "300") // units/s: at most this fast
QVR_TUNABLE(vr_foegrab_strength_small, "0.85") // one hand's hold on a small enemy (grunt, enforcer, zombie, knight, mummy...)
QVR_TUNABLE(vr_foegrab_strength_medium, "0.25") // on a big one (fiend, ogre)
QVR_TUNABLE(vr_foegrab_strength_large, "0.04") // on a huge one (shambler, vore and bigger)
QVR_TUNABLE(vr_foegrab_mass_light, "100") // kg (a grunt 80, an enforcer 100, a fiend or an ogre 250, a shambler 600)
QVR_TUNABLE(vr_foegrab_mass_heavy, "650") // kg
QVR_TUNABLE(vr_foegrab_shove_resist, "0.9") // share of the shove's push the hold takes off (times the hold)
QVR_TUNABLE(vr_foegrab_throw_twist, "130") // degrees/s: the hands' turn of the enemy (over, about a level axis through its middle) that throws
QVR_TUNABLE(vr_foegrab_throw_speed, "3") // m/s: and the hands' mean speed then, at least
QVR_TUNABLE(vr_foegrab_throw_hurt, "0.4") // the "when hurt" enemies are thrown below this share of their full health, never above
QVR_TUNABLE(vr_foegrab_throw_mass_always, "140") // kg, by mass: this light or lighter always (a mummy 140)
QVR_TUNABLE(vr_foegrab_throw_mass_hurt, "300") // kg: this light or lighter only when hurt; heavier never (a vore, an overlord never)
QVR_TUNABLE(vr_foegrab_throw_push, "220") // units/s: the thrown enemy's launch along the throw (times vr_knockdown_push)
QVR_TUNABLE(vr_foegrab_throw_lift, "120") // units/s: and up
QVR_TUNABLE(vr_foegrab_throw_topple, "180") // degrees/s: the thrown ragdoll turned over about its feet towards the throw, a sweep (push and lift shared by height); 0: pushed whole, as a shove's
QVR_TUNABLE(vr_foegrab_throw_grunt, "0.75") // volume (0..1) of the player's grunt as his two-hand throw throws an enemy down (which: _grunt_sound); 0 off
QVR_TUNABLE(vr_foegrab_throw_feet_speed, "20") // units/s: its feet swept back against the throw as it topples (the turn faster, about a pivot raised to match: it spins in place); 0: its feet held still 0.5 s
QVR_TUNABLE(vr_foegrab_throw_spin, "0.5") // share of the hands' twist of it about the vertical it spins on with (at most 720 degrees/s)
QVR_TUNABLE(vr_foegrab_throw_hold_strength, "0.3") // the most each hand pulls the thrown ragdoll's limb with, a share of its weight (two at 0.3: they drag it, lowered to it, but never hold it up)
QVR_TUNABLE(vr_foegrab_throw_hold_hertz, "4") // Hz: how quickly the held limb follows the hand (its spring's rate, critically damped)
QVR_TUNABLE(vr_foegrab_lift_strength, "1.2") // the most each hand holds the lifted ragdoll with, a share of its weight (two at 1.2 carry it, flailing, a hand on a forearm: 0.75 let an enforcer sag to the floor)
QVR_TUNABLE(vr_foegrab_lift_up, "160") // units/s: it leaves the floor this fast up as it is lifted
QVR_TUNABLE(vr_foegrab_free_push, "0.15") // metres: breaking free it pushes you back this far, a smooth slide, less than a shove (0: not at all)
QVR_TUNABLE(vr_foegrab_leggrab_heavy, "100") // kg: a humanoid heavier than this needs a faster pull and lift, times the cube root of its mass over this (a grunt 80, an enforcer 100, a death knight 150); 0: all alike
QVR_TUNABLE(vr_foegrab_legpull_speed, "1.2") // m/s: the hands' mean speed towards you (level) that pulls its legs out
QVR_TUNABLE(vr_foegrab_legpull_distance, "8") // cm: and how far they have come towards you in that pull
QVR_TUNABLE(vr_foegrab_legpull_topple, "200") // deg/s: pulled out, its body turns over about its feet away from you this fast (its top's speed)
QVR_TUNABLE(vr_foegrab_legpull_feet, "60") // units/s: its feet slide towards you this fast as it topples
QVR_TUNABLE(vr_foegrab_legpull_drag_strength, "1.5") // pulled down and lying, the most the hands still holding its legs drag it with in all, a share of its weight shared by them (one hand alone drags it too): Pull Hold Strength's soft hold firms up to this over the half second after its fall (two hands at 0.1 or 0.3 each, it lay still, its weight on the floor holding it more than their pull; by both legs 1 in all drags it, by one about 1.5; run from it and the holds still let go)
QVR_TUNABLE(vr_foegrab_legpull_hold_strength, "0.1") // pulled down, the most each hand still holding its legs pulls them with, a share of its weight (softer than a throw's hold, vr_foegrab_throw_hold_strength: held hard, its shins pinned, it pivoted on them and fell the wrong way; Hold On After a Throw off: let go)
QVR_TUNABLE(vr_foegrab_leglift_speed, "1") // m/s: the hands' mean speed up (within 45 degrees of straight up) that lifts it by the legs (you within Lift Reach)
QVR_TUNABLE(vr_foegrab_leglift_topple, "220") // deg/s: lifted by the legs, its body swings over head down this fast (away from you)
QVR_TUNABLE(vr_foegrab_necksnap_volume, "1") // the crack's volume (0 silent; up to 2: the layers in your hands louder too)
QVR_TUNABLE(vr_foegrab_necksnap_strength, "1.25") // the most each hand holds the limp body with afterwards, a share of its weight (above 0.5 two hands carry it)
QVR_TUNABLE(vr_foegrab_necksnap_hertz, "24") // Hz: how stiffly its head follows the hands afterwards (the hold's spring: the light head bears the body's weight, it sags less the stiffer)
QVR_TUNABLE(vr_foegrab_necksnap_cone, "120") // degrees: a snapped neck's swing limit (a living neck's: the rig's, 45 for the grunt; past about 120 the head sinks into its chest, which it never meets)
QVR_TUNABLE(vr_foegrab_necksnap_twist, "140") // degrees: a snapped neck's twist limit either way (a living neck's: 50 for the grunt)
QVR_TUNABLE(vr_foegrab_necksnap_friction, "0.1") // a snapped neck's joint friction, a share of the body's (no spring: nothing holds the head up)
QVR_TUNABLE(vr_foegrab_necksnap_tough, "0.25") // a tough one's (its monster group's Neck Snap Kills off: the Mini Bosses) neck holds: the twist deals this share of its full health (QC VR_FoeGrab_NeckSnap)

// Combat > Holding Enemies - Struggle
QVR_TUNABLE(vr_struggle_pry, "1") // a hold its free hands reach (its head, an arm): they grab at the holding hand and tug (0..2)
QVR_TUNABLE(vr_struggle_crunch, "1") // lifted by the legs: it bends up at the waist, reaching for you (0..2)
QVR_TUNABLE(vr_struggle_claw, "1") // held by the legs on the floor: it turns belly down and claws ahead (0..2)
QVR_TUNABLE(vr_struggle_brace, "1") // dragged by an arm or the head: its legs stiffen, heels dug in (0..2)
QVR_TUNABLE(vr_struggle_grip_guard, "6") // units: a held point this far from the hand (the hold lagging) eases the struggle, to a quarter at 2.5 times it (0: never)
QVR_TUNABLE(vr_struggle_voice, "1") // its own pain, grunt, sight and idle sounds while it struggles: their volume (0..2; 0 silent), quieter as it weakens
QVR_TUNABLE(vr_struggle_voice_gap, "1.8") // s: about this between its cries at full effort (longer as it tires)

// Combat > Knockdowns
QVR_TUNABLE(vr_knockdown_damage, "3") // the chance times 1 + this times the share of its health it has lost
QVR_TUNABLE(vr_knockdown_bash, "1") // and this for a weapon's bash (0: only open-palm shoves)
QVR_TUNABLE(vr_knockdown_stamina, "1") // how much low stamina cuts the chance: times 1 - this x the share spent (0: no effect)
QVR_TUNABLE(vr_knockdown_surprise_angle, "70") // degrees: a shove's way within this of where the monster faces is one from behind (0: only an unaware one is surprised)
QVR_TUNABLE(vr_hit_stagger_chance, "1") // every hit's chance times this (testing: 100, every hit past the least impact)
QVR_TUNABLE(vr_hit_stagger_min, "0.5") // impact (1: the reference throw or blow) under this never unbalances; from it to 1 the chance rises 0 .. 1
QVR_TUNABLE(vr_hit_stagger_head, "1.5") // a melee blow to the head (fist or weapon): impact times this
QVR_TUNABLE(vr_hit_stagger_resist, "0.75") // big monsters resist: the chance times (30 / its full health) ^ this, above 30 health (0: none; shambler 0.1, ogre 0.24)
QVR_TUNABLE(vr_hit_stagger_cooldown, "4") // s after an unbalancing hit before another hit can unbalance it (no stunlock)
QVR_TUNABLE(vr_ragimpact_damage, "0.08") // damage per kg m/s past the least speed (mass behind the part x (its speed - Least Speed))
QVR_TUNABLE(vr_ragimpact_mult, "1.25") // a ragdoll's (knocked down or dead) strike on a wall, the floor or a prop: its damage and its head's or limb's wear (pops) times this (not a landing's crush)
QVR_TUNABLE(vr_ragimpact_min_speed, "2.5") // m/s: slower strikes do nothing
QVR_TUNABLE(vr_ragimpact_held_speed, "1") // m/s: a part your hand holds counts from this speed (the hand slows as it drives a body: 2-4 m/s slams)
QVR_TUNABLE(vr_ragimpact_dent, "2") // a strike dealing this much damage or more dents and wounds the part where it struck, as a blunt melee blow does (a head slammed into a wall: its face dented); 0: never
QVR_TUNABLE(vr_ragimpact_drag_speed, "1.2") // m/s: a body a hand holds strikes nothing unless a hand holding it swung this fast (its own move, not your walk) within 0.4 s: dragged along by the stick it bumps the floor and walls unhurt; a slam hurts (0: every strike counts, as before)
QVR_TUNABLE(vr_ragimpact_head, "1.5") // a strike on the head deals this much more
QVR_TUNABLE(vr_ragimpact_piece_hp, "0.65") // a head's or limb's own health, times its corpse health (vr_corpse_health_<monster>): worn past it, it pops
QVR_TUNABLE(vr_ragimpact_curve, "1.5") // damage vs speed past the least: 1 linear, 2 quadratic, 0.5 square root (pivots at vr_ragimpact_curve_ref: every curve deals the same there)
QVR_TUNABLE(vr_ragimpact_curve_ref, "4") // m/s past the least speed where every curve deals the same (slower: a steeper curve less, faster: more)
QVR_TUNABLE(vr_ragimpact_max, "40") // most damage one strike (or a landing's crush) deals, after the head's and limbs' multipliers (0: no cap)
QVR_TUNABLE(vr_ragimpact_limb, "1") // a strike on an arm or a leg deals this much more (the torso 1, the head vr_ragimpact_head)
QVR_TUNABLE(vr_ragsmash_min_speed, "0") // m/s: a strike sounds and bleeds (vr_ragsmash) only this fast or faster, besides hurting (the impacts' own threshold); 0: every strike that hurts
QVR_TUNABLE(vr_ragsmash_full_speed, "4") // m/s past the impacts' least speed: a strike this hard sounds full and deepest and bleeds its most (softer ones less, from a quarter)
QVR_TUNABLE(vr_ragsmash_volume, "1.35") // the smash sound's volume, times (0: silent; blood still; over full volume past 1: his note 2026-11-01, 30-40% louder)
QVR_TUNABLE(vr_ragsmash_decal_size, "1") // the smash blood's size, times (1: 18..40 units across by the strike, then the gore's size; 0: none)
QVR_TUNABLE(vr_ragsmash_cooldown, "0.25") // s: one body sounds and bleeds at most this often (a body sliding or bouncing along a wall: no machine gun)
QVR_TUNABLE(vr_ragimpact_wear, "1") // a head's or limb's wear from a strike: its damage times this (higher: pops sooner; 0: impacts never pop pieces)
QVR_TUNABLE(vr_ragimpact_crush, "1.5") // landing on a living monster (standing, staggering or down as a ragdoll) from a height crushes the part under your feet, times this (0: off)
QVR_TUNABLE(vr_ragimpact_crush_wear, "0.5") // share of a head's or limb's own health a landing at the least speed wears (a jump from the same level: ~0.55, the second pops it), rising to all of it at vr_ragimpact_crush_pop (0: landings wear nothing)
QVR_TUNABLE(vr_ragimpact_crush_pop, "380") // units/s: a landing this fast pops the head or limb under your feet at once (a drop of about 2 heights onto a body lying down: ~90 units to it)
QVR_TUNABLE(vr_ragimpact_crush_big, "200") // kg: a monster heavier (ogre, fiend, vore, shambler) is only hurt by landings: its head and limbs never worn (0: none counts as big)
QVR_TUNABLE(vr_ragimpact_crush_big_max, "0.25") // most of a big one's full health one landing takes (0: no cap)
QVR_TUNABLE(vr_ragimpact_stomp_speed, "400") // units/s: the least landing speed that knocks a standing one down (about a 115-unit drop)
QVR_TUNABLE(vr_ragimpact_stomp_heavy, "120") // kg: a monster heavier needs a faster landing, the least speed times the cube root of its mass over this (ogre, fiend, vore 250 kg: ~180-unit drop; shambler 600: ~315); 0: all alike
QVR_TUNABLE(vr_ragimpact_stomp_bounce, "200") // units/s up: you bounce off the one you knocked down (0: you drop through as it falls)
QVR_TUNABLE(vr_knockdown_search, "48") // how far (units) from the body it looks for room to stand
QVR_TUNABLE(vr_knockdown_water_search, "256") // ... a knockdown swimming in water, slime or lava: how far it looks for a floor where its head is out to paddle to (shallows, the bank: box3d livingSwim); it gets up within vr_knockdown_search of where it floats
QVR_TUNABLE(vr_knockdown_water_climb, "48") // ... getting up in a liquid: how far (units) over its pelvis the floor it gets up onto may be (higher: carried down there, it swims up first or gets up where it floats)
QVR_TUNABLE(vr_knockdown_water_giveup, "3") // ... none found for this long (s) after its time to get up: it gets up where it floats, a walking monster again (and sinks); 0: never
QVR_TUNABLE(vr_legsweep_min_length, "35") // cm: the least length of a staff-like thing (its drawn shape along its long axis: vr_legsweep_table; the nailgun 47, a brick 20)
QVR_TUNABLE(vr_legsweep_min_slender, "2") // the least slenderness of a staff-like thing: its length over its typical cross-section (a weapon: the median of its drawn slices, so a gun is measured by its barrel, not its grip; a prop: its box. A crate about 1, a brick 2, the lightning gun 2.6)
QVR_TUNABLE(vr_legsweep_speed, "3.25") // m/s: the least level speed of the part that meets the legs, swung (its move in your own frame: walking or turning with it held still is none), held over its stroke (0.08 s) (a heavier humanoid needs more: vr_legsweep_heavy)
QVR_TUNABLE(vr_legsweep_heavy, "100") // kg: a humanoid heavier than this needs a faster sweep, the least speed times the cube root of its mass over this; 0: all alike
QVR_TUNABLE(vr_legsweep_damage, "5") // the damage a sweep deals once it is down
QVR_TUNABLE(vr_legsweep_topple, "200") // deg/s: how fast its body turns over about its feet, against the sweep (its top's speed)
QVR_TUNABLE(vr_legsweep_feet, "0.5") // its feet slide along the sweep at this share of the sweep's speed
QVR_TUNABLE(vr_legsweep_align, "45") // degrees: a sweep within this of its own facing's axes (forward, back, its sides) falls straight along that axis (from its side: forward or backward, as the swing goes); 0: as swept
QVR_TUNABLE(vr_legsweep_legs, "0.5") // s: swept (or its legs pulled) to fall backward, its legs stay straight this long (its knees do not fold under it, its feet stay where they slid); 0: they give at once
QVR_TUNABLE(vr_knockdown_ledge_drop, "16") // how high (units) a drop must be to count as a ledge (stairs and steps do not)
QVR_TUNABLE(vr_knockdown_ledge_reach, "0.5") // how far ahead a ledge is looked for: times the shove's reckoned travel (the hop and the slide; 1: as far as it goes)
QVR_TUNABLE(vr_knockdown_ledge_margin, "24") // units past that travel (times vr_knockdown_ledge_reach) where a ledge still counts
QVR_TUNABLE(vr_knockdown_wiggle_pause, "0.5") // pause between bursts, seconds; 0 continuous (the default since config 89; 1 before)

// Combat > Melee
QVR_TUNABLE(vr_quad_melee_damage, "1.1")
QVR_TUNABLE(vr_quad_melee_range, "1")
QVR_TUNABLE(vr_headbutt_damage, "24") // damage at that speed (up to three times it, faster)
QVR_TUNABLE(vr_melee_swish_min, "0.3") // share of the blow's landing speed from which a swing is heard (slower: silent)
QVR_TUNABLE(vr_melee_swish_sub, "0.35") // volume of the loudest swing short of a blow (just under its landing speed; a blow is 1 and more)
QVR_TUNABLE(vr_melee_swish_full, "1.75") // share of the landing speed at which a blow's whoosh is loudest (vr_melee_swish_gain; the author's punches land at 1.4-1.6, his overhead cuts 1.9-2.7)
QVR_TUNABLE(vr_melee_swish_gain, "2") // volume of the fastest blows' whoosh (1: full volume, as a blow's was; up to 4), a little higher too

// Combat > Monster Health
QVR_TUNABLE(vr_monster_health_from, "150") // no cut up to this health (grunts, knights, enforcers, zombies...)
QVR_TUNABLE(vr_monster_health_hp1, "200") // the curve's first point: an ogre's health ...
QVR_TUNABLE(vr_monster_health_cut1, "10") // ... cut this many percent
QVR_TUNABLE(vr_monster_health_hp2, "600") // a shambler's ...
QVR_TUNABLE(vr_monster_health_cut2, "15")
QVR_TUNABLE(vr_monster_health_hp3, "1000") // and from here on ...
QVR_TUNABLE(vr_monster_health_cut3, "20")
QVR_TUNABLE(vr_monster_health_max, "20") // the most the curve cuts (percent)

// Combat > Parry and Bash
QVR_TUNABLE(vr_parry_stagger_frames, "3") // frames of its pain animation a parried monster steps through (0.1 s each), then rocks back and forth over the last two (1..4; 1: its first pain frame held)
QVR_TUNABLE(vr_parry_stagger_sway, "3") // degrees a parried monster's body sways (roll, pitch 0.6 of it) while it is staggered (0..15; 0 none)
QVR_TUNABLE(vr_parry_angle, "40") // degrees the weapon may be off level (tilted up or down) and still parry (the parry stance)
QVR_TUNABLE(vr_parry_reach, "1.5") // metres in front of you a held weapon still parries
QVR_TUNABLE(vr_parry_unarmed_reduction, "0.45") // share of a blow taken off by crossed arms
QVR_TUNABLE(vr_shove_speed, "3.5") // m/s open palms (facing ahead, the arm extending) must be pushed forward to shove
QVR_TUNABLE(vr_bash_damage, "5") // a bash's damage (small: it knocks back and staggers)
QVR_TUNABLE(vr_bash_push, "0.9") // how far a shove or a bash throws what it hits (its push: 520 u/s and a 140 hop, times Knockback; then the hands below)
QVR_TUNABLE(vr_shove_run_carry, "1") // a shove or a bash while you run (40 u/s or more along it): this share of your speed along it carried into it on top of its push (times its group's Shove Push over its weight, at most all of it; not by Knockback): farther and faster, its stagger's travel limit as far as its slide; 0: standing's push alone
QVR_TUNABLE(vr_parry_cooldown, "0.4") // s: a monster's blow parried within this long of its last parried blow is still parried (its damage cut, both pushed apart) but plays no sound or sparks, opens no new counter window and costs no stamina (one parry for a burst of quick hits: the ogre's chainsaw, a knight's swing; 0: every blow a full parry)
QVR_TUNABLE(vr_parry_tool_hand, "0.4") // a parry's strength: bare hands (the arms crossed)
QVR_TUNABLE(vr_parry_tool_weapon, "1") // ... any weapon (a light one: Heft 0)
QVR_TUNABLE(vr_parry_tool_heavy, "1.3") // ... the heaviest weapon (Heft 1; between: by its Heft)
QVR_TUNABLE(vr_parry_grip_2h, "1.5") // ... times this held in two hands (the arms crossed too); one hand 1
QVR_TUNABLE(vr_parry_timed, "1.3") // ... times this well timed: the guard raised at most vr_parry_timed_window s before the blow
QVR_TUNABLE(vr_parry_timed_window, "0.3") // s: the best part of the parry's window, from raising the guard (the parry pose, facing ahead) on
QVR_TUNABLE(vr_parry_block_below, "0.5") // strength over poise under this: blocked only (no stagger, the attack goes on)
QVR_TUNABLE(vr_parry_flinch, "0.3") // s: a flinch (strength over poise from vr_parry_block_below to 1): its pain pose, no ragdoll
QVR_TUNABLE(vr_parry_repeat, "0.7") // each stagger of the same monster within vr_parry_repeat_window s of its last: times this once more (a heavy group, poise over 1: to the power of its poise)
QVR_TUNABLE(vr_parry_repeat_window, "5") // s: ... within this of its last stagger (0: never shorter)
QVR_TUNABLE(vr_counter_window, "1") // s after a parry a counter may land
QVR_TUNABLE(vr_counter_damage, "1.75") // a counter's damage (and a bash's knockback) multiplier (shipped 1.2 before config 89)
QVR_TUNABLE(vr_counter_haptic, "1") // strength of the soft pulses in the hands while the window is open (0: off)
QVR_TUNABLE(vr_dummy_attack_period, "2.5") // s from one of the dummy's blows to the next (its wind-up included)
QVR_TUNABLE(vr_dummy_attack_jitter, "0.4") // s the period varies by at random, either way
QVR_TUNABLE(vr_dummy_attack_reach, "80") // units (origin to origin) within which it strikes; beyond, it waits (a knight's: 60)
QVR_TUNABLE(vr_dummy_attack_damage, "10") // a blow's damage, before a parry (as a monster's blow: armour, god mode and the parry apply)
QVR_TUNABLE(vr_dummy_attack_windup, "0.6") // s of wind-up (the tell: a sound, the rifle raised, a glow) before each blow

// Combat > Ragdoll Triggers
QVR_TUNABLE(vr_kdtrig_cooldown, "4") // s after it gets up before a ragdoll trigger takes it again

// Combat > Ragdoll Triggers - Explosions
QVR_TUNABLE(vr_kdtrig_blast, "0.6") // Explosions: the chance (A nearby explosion (a rocket, a grenade, a barrel, any blast that leaves it alive) throws it down away from the blast's centre (Least Damage: the damage it took, already less with distance).)
QVR_TUNABLE(vr_kdtrig_blast_min, "15") // ... the least damage
QVR_TUNABLE(vr_kdtrig_blast_push, "1.25") // ... its push over (two hands' shove 1)
QVR_TUNABLE(vr_kdtrig_blast_range, "200") // ... units from the blast's centre: none past it, half the chance at it
QVR_TUNABLE(vr_kdtrig_weight, "80") // kg: a monster of resistance 1's weight (a grunt's); each its resistance times this
QVR_TUNABLE(vr_kdtrig_weight_topple, "1.5") // up to this times the thrower's weight it can be toppled (a similar or lighter class)
QVR_TUNABLE(vr_kdtrig_weight_stagger, "3") // up to this times it only staggers (an ogre at a grunt); heavier: nothing
QVR_TUNABLE(vr_kdtrig_bump_stagger, "0.6") // s: the stagger of a heavier one (a parry's, as a ragdoll where it can), pushed back with the strike

// Combat > Ragdoll Triggers - Ledges
QVR_TUNABLE(vr_kdtrig_look, "40") // units past its hull the ground behind it (along the hit) is looked at for stairs and ledges
QVR_TUNABLE(vr_kdtrig_ledge, "0.7") // Ledges: the chance (Struck with a drop of Least Drop or more right behind it (along the hit, room for its body to drop), it is pushed off.)
QVR_TUNABLE(vr_kdtrig_ledge_min, "12") // ... the least damage
QVR_TUNABLE(vr_kdtrig_ledge_push, "1") // ... its push over (two hands' shove 1)
QVR_TUNABLE(vr_kdtrig_ledge_drop, "48") // ... units: a ledge's least drop (less: a step)

// Combat > Ragdoll Triggers - Shotgun Range
QVR_TUNABLE(vr_kdtrig_shotgun, "0.35") // Shotgun Range: the chance (A shotgun or super shotgun blast of yours from close by knocks it over.)
QVR_TUNABLE(vr_kdtrig_shotgun_push, "1") // ... its push over (two hands' shove 1)
QVR_TUNABLE(vr_kdtrig_shotgun_range, "160") // ... units: blasts this close
QVR_TUNABLE(vr_kdtrig_shotgun_pellets, "8") // ... pellets landing for the full chance (a shotgun's 6, a super shotgun's 14; under half of them none)

// Combat > Ragdoll Triggers - Shoulder Charges
QVR_TUNABLE(vr_kdtrig_tackle_push, "1.3") // ... its push over and back (two hands' shove 1; the knock times it) (his 1.3, 1.25 before vr_cfg_version 184)
QVR_TUNABLE(vr_kdtrig_tackle_speed, "0.75") // ... your ground speed for it, a share of the most (sv_maxspeed; reached within 0.25 s before)
QVR_TUNABLE(vr_kdtrig_tackle_weight, "80") // ... kg: your weight for the weight classes
QVR_TUNABLE(vr_kdtrig_tackle_turn, "60") // ... deg: your hands' middle at least this far round from the run as you meet it (seen from your head: side-on, the torso turned; not held) (60, 50 before vr_cfg_version 189)
QVR_TUNABLE(vr_kdtrig_tackle_low, "0.5") // ... units: both hands at least this far below your head (by the body: a bit above the chest counts) (0.5, 5 before vr_cfg_version 189)
QVR_TUNABLE(vr_kdtrig_tackle_reach, "20") // ... units: both hands within this of your head's line (level: close to the body), neither reaching ahead past 0.3 of it
QVR_TUNABLE(vr_kdtrig_tackle_hold, "0.15") // ... s the pose is held running before it counts
QVR_TUNABLE(vr_kdtrig_tackle_stand, "0.75") // ... your head's height at least this share of your standing (calibrated) height: standing tall, not crouched or half crouched (0 any) (his 0.75, 0.8 before vr_cfg_version 191)
QVR_TUNABLE(vr_kdtrig_tackle_head, "5") // ... deg: your head tipped down at least this much (tucked, as the author charges) (5, 7.5 before vr_cfg_version 189)
QVR_TUNABLE(vr_kdtrig_tackle_face, "60") // ... deg: running within this of the way you look (not a sidestep)
QVR_TUNABLE(vr_kdtrig_tackle_leniency, "0.4") // ... 0..1: the pose's thresholds together (0.5 as set; 1 half as strict: least ones halved, most ones and the speed's shortfall doubled; 0 1.5 times as strict) (his 0.4, 0.2 before vr_cfg_version 191, 0.85 before 184)
QVR_TUNABLE(vr_kdtrig_tackle_knock, "148") // ... units a monster of your weight is knocked back, then toppled (a lighter one up to 1.5 times; a heavier class staggers back 0.6 of it) (his 148, 152 before vr_cfg_version 184)
QVR_TUNABLE(vr_kdtrig_tackle_bump, "120") // ... units/s you bounce back off it (his 120, 100 before vr_cfg_version 184)
QVR_TUNABLE(vr_kdtrig_tackle_stamina, "35") // ... stamina a charge that connects costs (0: none; one that misses costs nothing)
QVR_TUNABLE(vr_kdtrig_tackle_contact, "0") // ... units: it connects with your body this near the monster's as the two meet (the engine's narrowed, inset boxes: QC bodygap; 0 touching; plus this frame's run), not before (his 0, 2 before vr_cfg_version 184)
QVR_TUNABLE(vr_kdtrig_tackle_cooldown, "1.5") // ... s before you charge again

// Combat > Ragdoll Triggers - Slopes
QVR_TUNABLE(vr_kdtrig_slope, "0.4") // Slopes: the chance (Standing on a steep slope, hit down it, it topples down the slope.)
QVR_TUNABLE(vr_kdtrig_slope_min, "15") // ... the least damage
QVR_TUNABLE(vr_kdtrig_slope_push, "1") // ... its push over (two hands' shove 1)
QVR_TUNABLE(vr_kdtrig_slope_angle, "18") // ... deg: the slope's least steepness

// Combat > Ragdoll Triggers - Stairs
QVR_TUNABLE(vr_kdtrig_stairs, "0.5") // Stairs: the chance (Struck at the top of steps going down behind it (two or more, the first right behind its feet), it falls down them.)
QVR_TUNABLE(vr_kdtrig_stairs_min, "15") // ... the least damage
QVR_TUNABLE(vr_kdtrig_stairs_push, "1") // ... its push over (two hands' shove 1)

// Combat > Ragdoll Triggers - Strong Blows
QVR_TUNABLE(vr_kdtrig_blow, "0.25") // Strong Blows: the chance (A strong melee blow or a prop you fling knocks it over (up to twice the chance at twice Least Damage: a hard punch is about 30).)
QVR_TUNABLE(vr_kdtrig_blow_min, "30") // ... the least damage
QVR_TUNABLE(vr_kdtrig_blow_push, "1") // ... its push over (two hands' shove 1)
QVR_TUNABLE(vr_kdtrig_blow_blade, "0.4") // ... a blade's edge (axes, swords, the chainsaw): its chance times this; blunt blows (fists, a gun's butt, a hammer, a mace, a pommel, the crowbar, props) 1

// Combat > Ragdoll Triggers - Thrown Bodies
QVR_TUNABLE(vr_kdtrig_thrown, "1") // Thrown Bodies: the chance (A body you throw or swing into a monster knocks it over by weight and momentum.)
QVR_TUNABLE(vr_kdtrig_thrown_push, "2") // ... its push over and back (two hands' shove 1), more with the strike
QVR_TUNABLE(vr_kdtrig_thrown_speed, "160") // ... units/s a body of the monster's own weight topples it at (its momentum over the monster's weight at this: 1); a quarter of it the least that counts
QVR_TUNABLE(vr_kdtrig_thrown_least, "0.05") // ... a weaker strike (that momentum share) does nothing; from it to 1 the topple's chance grows to its full (else a stagger back)
QVR_TUNABLE(vr_kdtrig_thrown_window, "1.5") // ... s after a hand lets go it counts as thrown (held and swung: while a hand swings)

// Combat > Ragdoll Triggers - Wounded
QVR_TUNABLE(vr_kdtrig_wounded, "0.25") // Wounded: the chance (Badly hurt, any hit may knock it down.)
QVR_TUNABLE(vr_kdtrig_wounded_min, "5") // ... the least damage
QVR_TUNABLE(vr_kdtrig_wounded_push, "1") // ... its push over (two hands' shove 1)
QVR_TUNABLE(vr_kdtrig_wounded_health, "0.3") // ... its health under this share of its full health

// Combat > Stamina
QVR_TUNABLE(vr_stamina_speed_min, "0.5") // times the most speed with no stamina left (1: no slower)
QVR_TUNABLE(vr_climb_stamina_rate, "5") // with it: stamina a second hanging from one hand
QVR_TUNABLE(vr_climb_stamina_rate_2h, "2") // and from both hands (the two together: less than one hand, the arms share the weight)
QVR_TUNABLE(vr_fatigue_shake_from, "0.4") // the stamina left (0..1) below which they start to shake, more the less is left
QVR_TUNABLE(vr_fatigue_shake_speed, "1") // times the tremor's quickness (5 to 13 Hz)
QVR_TUNABLE(vr_parry_stamina_max, "100") // stamina when rested
QVR_TUNABLE(vr_parry_stamina_cost, "20") // stamina a parry with a weapon in one hand costs
QVR_TUNABLE(vr_parry_stamina_cost_2h, "10") // and with a weapon held in two hands
QVR_TUNABLE(vr_parry_stamina_delay, "2") // s after the last parry before stamina comes back
QVR_TUNABLE(vr_parry_stamina_regen, "25") // stamina a second it then comes back at
QVR_TUNABLE(vr_parry_stamina_warn, "1") // the warning when one more one-handed parry would knock the weapon away, and on the knock: volume of the gasp and strength of the buzz in the hand (0: off)
QVR_TUNABLE(vr_strike_stamina_punch, "6") // stamina a punch costs
QVR_TUNABLE(vr_strike_stamina_cost, "8") // a blow with a weapon held in one hand (a sword, an axe, Mjolnir, a gun; its pommel or butt too)
QVR_TUNABLE(vr_strike_stamina_cost_2h, "12") // and with a weapon held in two hands
QVR_TUNABLE(vr_shove_stamina_cost, "15") // stamina a one-handed shove or bash costs
QVR_TUNABLE(vr_shove_stamina_cost_2h, "20") // and a two-handed one
QVR_TUNABLE(vr_stamina_exhausted_damage, "0.5") // a blow's, a shove's or a bash's damage with no stamina left for it (1: no penalty); with part of its cost left, in between
QVR_TUNABLE(vr_stamina_exhausted_push, "0.5") // a shove's or a bash's knockback and stagger with no stamina left for it (1: no penalty); with part of its cost left, in between

// Combat > Stealth AI
QVR_TUNABLE(vr_stealth_meter_time, "0.5") // s the meter takes to fill: in full light, near, in front, walking
QVR_TUNABLE(vr_stealth_meter_decay, "0.15") // share of the meter lost a second while not seen
QVR_TUNABLE(vr_stealth_glimpse, "0.3") // the meter's share at which the monster turns to look (Alert)
QVR_TUNABLE(vr_stealth_light_dark, "16") // light (128: Quake's full light) at or below which you are not seen
QVR_TUNABLE(vr_stealth_light_bright, "64") // light at or over which you are seen as fast as can be
QVR_TUNABLE(vr_stealth_crouch, "0.4") // the meter's rate in a full crouch (a share)
QVR_TUNABLE(vr_stealth_still, "0.5") // the meter's rate while you stand still (walking 1, running 2)
QVR_TUNABLE(vr_stealth_sight_range, "1500") // units at which a monster no longer makes you out (full rate within 128)
QVR_TUNABLE(vr_stealth_peripheral, "0.3") // the meter's rate out of a monster's front cone (behind it: none)
QVR_TUNABLE(vr_stealth_noise_run, "400") // units your running is heard at (in the open)
QVR_TUNABLE(vr_stealth_noise_props, "1500") // units a prop's loudest knock is heard at (a quieter one: by its volume)
QVR_TUNABLE(vr_stealth_noise_melee, "1") // times a blow's reach (by its strength and the weapon's weight)
QVR_TUNABLE(vr_stealth_noise_wall, "0.6") // a noise's reach kept round a corner (in sight of the place, not in line)
QVR_TUNABLE(vr_stealth_noise_solid, "0.15") // and through solid walls (out of sight of the place)
QVR_TUNABLE(vr_stealth_noise_absorb, "0.6") // and past a prop (a crate) in the way
QVR_TUNABLE(vr_stealth_turn, "0.6") // an Alert monster's turn speed (times its own)
QVR_TUNABLE(vr_stealth_search_time, "5") // s an Alert monster searches the spot before it goes back to its post
QVR_TUNABLE(vr_stealth_sensitive, "1.5") // its meter's rate for a while after (times)
QVR_TUNABLE(vr_stealth_sensitive_time, "30") // s it stays that wary
QVR_TUNABLE(vr_stealth_graze, "72") // units: a shot passing this near a monster alerts it, from any distance (0: off)
QVR_TUNABLE(vr_stealth_torch, "500") // units: a held or thrown torch alerts monsters that see it this near (0: off)
QVR_TUNABLE(vr_stealth_corpses, "700") // units: corpses and gibs seen this near alert monsters (0: off)

// Combat > Survival Arena
QVR_TUNABLE(vr_arena_cards_distance, "34") // the perk cards' panel (vr_arenacards.cpp): how far in front of the head (units; 34 about 1.3 m)
QVR_TUNABLE(vr_arena_cards_width, "32") // ... and how wide (units; its height five eighths of it)

// Combat > Weapon Damage
QVR_TUNABLE(vr_throw_2h_damage, "1.25") // a weapon or prop thrown with both hands: its hit damage times this (QC VR_Thrown_Damage; NOTES.md vrfiringrange_2026-10-09_15-44-39: they hurt no more than one-handed ones)
QVR_TUNABLE(vr_dmg_super_shotgun, "4") // the double-barrelled shotgun: a pellet's (14 a shot)
QVR_TUNABLE(vr_dmg_nailpistol, "12") // the nail pistol (QC vr_nailpistol.qc): a nail's (a grunt, 30: two headshots at x1.5, three in the body; a monster's nail pistol 9)
QVR_TUNABLE(vr_dmg_flaregun, "10") // the flare gun (QC vr_flaregun.qc): a flare's hit (low: its fire does the rest; a monster's flare 6)
QVR_TUNABLE(vr_flaregun_speed, "1300") // a flare's speed, units a second, plus the hand's
QVR_TUNABLE(vr_flaregun_gravity, "0.3") // its gravity in flight (1 a grenade's): its arc
QVR_TUNABLE(vr_flare_air_pitch, "35") // degrees up at least a shot leaves for its flare to drift down from the top of its climb (lower: it flies on until it hits)
QVR_TUNABLE(vr_flare_lights, "4") // most flares lit at once (the nearest; air flares first), 0..16
QVR_TUNABLE(vr_flare_light, "1") // their lights' size and brightness (0: none)
QVR_TUNABLE(vr_flare_sparks, "1") // their sparks and smoke (0: none)
QVR_TUNABLE(vr_nailpistol_mag, "12") // the rounds its magazine holds (a nail each)
QVR_TUNABLE(vr_nailpistol_slide_cycle, "0.14") // s back and home after a shot (a third back, the rest home; a re-rack closes in the home part)
QVR_TUNABLE(vr_nailpistol_mag_travel, "5.25") // model units its magazine slides up the grip, seating and ejecting (5.25: from the grip's bottom, its load point, to home)
QVR_TUNABLE(vr_nailpistol_mag_seat_time, "0.12") // s its magazine is drawn sliding home up the grip once seated (0: at once; drawn only: loaded at once); keep it under vr_nailpistol_slide_delay, so that it is home before the rack
QVR_TUNABLE(vr_nailpistol_mag_eject_time, "0.14") // s its magazine slides out of the grip on the eject button before it drops (0: drops at once; out of the gun's count at once)
QVR_TUNABLE(vr_nailpistol_mag_eject_speed, "120") // u/s it then pops on out of the grip, along the grip's line
QVR_TUNABLE(vr_nailpistol_mag_eject_hand, "0.6") // and the share of the holding hand's velocity it takes with it (never more than 0.45 of the pop: mostly outward)
QVR_TUNABLE(vr_reload_mag_seat_time, "0.07") // s the other magazine guns' magazine is drawn sliding the last bit home once seated (0: at once; drawn only: loaded at once)
QVR_TUNABLE(vr_reload_mag_seat_travel, "1.5") // and how far, model units along the well's line
QVR_TUNABLE(vr_nailpistol_slide_delay, "0.175") // s from a magazine seated to its slide racking closed (0: at once); it cannot fire before
QVR_TUNABLE(vr_dmg_revolver, "28") // the heavy revolver (QC vr_revolver.qc): a slug's (a grunt, 30: one in the head at x1.5, two in the body, or one when its roll reaches 30; vr_revolver_heavy_mult in a big body; his 28 of 2026-10-27, was 26)
QVR_TUNABLE(vr_revolver_dmg_random, "0.15") // up to this share more damage at random, each slug (at damage 28 a grunt's body shot reaches its 30 about half the time)
QVR_TUNABLE(vr_revolver_heavy_health, "150") // the full health from which a monster's body is big (vr_revolver_heavy_mult)
QVR_TUNABLE(vr_revolver_cock_time, "0.05") // s from the pull to the shot: the hammer cocks and the cylinder turns a chamber, then the hammer falls (0: at once)
QVR_TUNABLE(vr_revolver_spread, "0.15") // degrees its slugs stray at most from its aim
QVR_TUNABLE(vr_dmg_lightning, "30") // the lightning gun: a bolt's (10 a second)
QVR_TUNABLE(vr_dmg_proximity, "95") // Scourge of Armagon's proximity gun: a mine's blast
QVR_TUNABLE(vr_dmg_mjolnir_lightning, "80") // Mjolnir's lightning: its first strike (its later ones 3/8 of it)
QVR_TUNABLE(vr_dmg_super_lava_nail, "30") // and the super nailgun's (to a player 3/5)
QVR_TUNABLE(vr_dmg_multi_grenade, "90") // its multi-grenade: each of its 5 bomblets' blast
QVR_TUNABLE(vr_dmg_multi_rocket, "60") // its multi-rocket: a rocket's direct hit (and up to a quarter more); its blast 1.25 times it
QVR_TUNABLE(vr_dmg_plasma, "80") // its plasma gun: a ball's direct hit (and up to a quarter more); its blast 7/8, its arcs 5/8
QVR_TUNABLE(vr_dmg_fist, "10") // a punch's base damage
QVR_TUNABLE(vr_dmg_axe, "20") // the axe's blow
QVR_TUNABLE(vr_dmg_gun_bash, "12") // a gun swung as a club
QVR_TUNABLE(vr_dmg_chainsaw_swing, "26") // an ogre's chainsaw swung (its running chain: vr_chainsaw_damage)
QVR_TUNABLE(vr_dmg_mjolnir, "25") // Mjolnir's blow
QVR_TUNABLE(vr_dmg_superaxe, "40") // Dawn of the Machine's Super Axe: a blow (QC vr_mg3_weapons.qc; zombies x3, a kill x2)
QVR_TUNABLE(vr_superaxe_burst_window, "1.5") // s: a second Super Axe blow on the same monster within it fires the lightning burst (MG3's 0.5)
QVR_TUNABLE(vr_dmg_ad_shadowaxe, "35") // the Shadow Axe's blow (AD's 35, the axe's 20), before its speed and weight
QVR_TUNABLE(vr_dmg_ad_widowmaker, "5") // the Widowmaker's pellet
QVR_TUNABLE(vr_ad_widowmaker_shot_pellets, "8") // its pellets a shot (a shell; AD's 21 were three shells' worth)
QVR_TUNABLE(vr_ad_widowmaker_refire, "0.35") // s between its shots, the trigger held (the super shotgun's 0.7)
QVR_TUNABLE(vr_dmg_ad_plasmagun, "45") // the Plasma Gun's bolt: its hit (AD's 45; a map's worldspawn plasmagun_dmg wins)
QVR_TUNABLE(vr_dmg_ad_plasmagun_splash, "20") // ... its blast (AD's 20; 0 none)
QVR_TUNABLE(vr_ad_plasmagun_speed, "900") // ... its speed (AD's 900)
QVR_TUNABLE(vr_ad_plasma_push, "70") // a plasma bolt's blast (the Plasma Gun's, the plasma monsters'; QC VR_ADGun_BlastPush) throws props and ragdolls as a blast of this much damage (Rogue's plasma 70, a rocket's 120; its own splash, 20, when more): its 20 threw them within 60 units only, weakly
QVR_TUNABLE(vr_chainsaw_damage, "120") // an ogre's chainsaw (QC vr_chainsaw.qc): damage a second its running chain deals what its bar is in
QVR_TUNABLE(vr_enfrifle_damage, "16") // an enforcer's laser rifle: each laser's damage (the enforcer's: 15)
QVR_TUNABLE(vr_weapon_throw_damage_mult, "0.4") // every throw's hit damage times this (weapons, props, boxes, gibs: VR_Thrown_Damage); 1 before 2026-10-02

// Graphics > Liquids - Shoreline Foam
QVR_TUNABLE(vr_water_foam_frequency, "1.75") // ... how finely its edge and flecks are broken up (0.25..4: more, smaller patches)
QVR_TUNABLE(vr_water_foam_strength, "1.75") // ... how far it reaches out from the shore (0.25..3, times its width)
QVR_TUNABLE(vr_water_foam_breathe, "0.45") // ... breathing: how much the band grows and shrinks over time, unevenly along the shore (0 off .. 1, a share of its width)
QVR_TUNABLE(vr_water_foam_breathe_speed, "2") // ... how fast it breathes (0.1..4; 1: a breath in about 4 seconds)
QVR_TUNABLE(vr_water_foam_breathe_irregularity, "0.6") // ... how unevenly along the shore (0: all of it together .. 1: each stretch on its own)
QVR_TUNABLE(vr_water_foam_red, "0.85") // ... its colour, red (0..2; over 1 glows)
QVR_TUNABLE(vr_water_foam_green, "0.9") // ... green
QVR_TUNABLE(vr_water_foam_blue, "0.92") // ... blue
QVR_TUNABLE(vr_water_foam_particle_life, "0.5") // ... how long each lasts, seconds (0.2..4) (0.4 before 2026-10-30, 0.8 before vr_cfg_version 202)
QVR_TUNABLE(vr_water_foam_particle_opacity, "0.5") // ... how opaque (0..1)
QVR_TUNABLE(vr_water_foam_particle_size, "1.25") // ... how big (0.25..4, times about 3 units across growing to 9)
QVR_TUNABLE(vr_water_foam_particle_shore, "-1") // ... where they are born: units out from the shore's line (-4..16; 0 on it, straddling it, a little either side) (his -1, 0 before vr_cfg_version 202)
QVR_TUNABLE(vr_water_foam_edge, "4") // ... the wavy edge: the liquid's edge at the shore drawn back in slow, drifting curves, the foam following them, so it is not one straight cut: how far, units (0 off .. 12)
QVR_TUNABLE(vr_lava_foam_frequency, "1.25")
QVR_TUNABLE(vr_lava_foam_strength, "2.5")
QVR_TUNABLE(vr_lava_foam_breathe, "0.65")
QVR_TUNABLE(vr_lava_foam_breathe_speed, "2.7")
QVR_TUNABLE(vr_lava_foam_breathe_irregularity, "0.7")
QVR_TUNABLE(vr_lava_foam_red, "1.5")
QVR_TUNABLE(vr_lava_foam_green, "1.1")
QVR_TUNABLE(vr_lava_foam_blue, "0.3")
QVR_TUNABLE(vr_lava_foam_particle_life, "1")
QVR_TUNABLE(vr_lava_foam_particle_opacity, "0.55")
QVR_TUNABLE(vr_lava_foam_particle_size, "3.05")
QVR_TUNABLE(vr_lava_foam_particle_shore, "0")
QVR_TUNABLE(vr_lava_foam_edge, "3")
QVR_TUNABLE(vr_slime_foam_frequency, "1.6")
QVR_TUNABLE(vr_slime_foam_strength, "1.9")
QVR_TUNABLE(vr_slime_foam_breathe, "0.7")
QVR_TUNABLE(vr_slime_foam_breathe_speed, "3.4")
QVR_TUNABLE(vr_slime_foam_breathe_irregularity, "0.6")
QVR_TUNABLE(vr_slime_foam_red, "0.38")
QVR_TUNABLE(vr_slime_foam_green, "0.56")
QVR_TUNABLE(vr_slime_foam_blue, "0.16")
QVR_TUNABLE(vr_slime_foam_particle_life, "1")
QVR_TUNABLE(vr_slime_foam_particle_opacity, "0.45")
QVR_TUNABLE(vr_slime_foam_particle_size, "1")
QVR_TUNABLE(vr_slime_foam_particle_shore, "0")
QVR_TUNABLE(vr_slime_foam_edge, "3")

// Gore > Small Gibs
QVR_TUNABLE(vr_smallgibs_min_damage, "10") // a hit's damage (a shotgun blast's pellets summed) under which none come
QVR_TUNABLE(vr_smallgibs_full_damage, "35") // the damage from which one always comes (the chance rises from 0 at the least)
QVR_TUNABLE(vr_smallgibs_curve, "1.7") // the chance's rise from the least damage to the sure one: to this power (1 straight; more: rarer for light hits)
QVR_TUNABLE(vr_smallgibs_damage_per_gib, "30") // one gib more for each this much damage (with a random part)
QVR_TUNABLE(vr_smallgibs_per_hit, "6") // most small gibs from one hit
QVR_TUNABLE(vr_smallgibs_shots, "1") // the chance times this: the shotguns, a grunt's gun
QVR_TUNABLE(vr_smallgibs_nails, "0.4") // nails, lasers
QVR_TUNABLE(vr_smallgibs_blades, "1.7") // a blade's edge or point: the axe, a sword, swung or thrown
QVR_TUNABLE(vr_smallgibs_blunt, "0.3") // punches, a pommel or hilt, a gun or the crowbar swung, Mjolnir, bashes, headbutts
QVR_TUNABLE(vr_smallgibs_props, "1.2") // a rock, brick or other thing thrown or swung in the fist
QVR_TUNABLE(vr_smallgibs_explosions, "1") // rockets, grenades, every blast
QVR_TUNABLE(vr_smallgibs_other, "0.5") // lightning, monsters' blows, the rest
QVR_TUNABLE(vr_smallgibs_saw_interval, "0.1") // s: the chainsaw tears one out of what it cuts at most this often (always: monster or corpse)
QVR_TUNABLE(vr_smallgibs_gibbing, "12") // small gibs with a monster's (or corpse's) gibs when it is gibbed
QVR_TUNABLE(vr_smallgibs_burst, "4") // small gibs a large gib bursts into (a head half as many again)
QVR_TUNABLE(vr_smallgibs_speed, "3") // m/s they fly out from the hit (randomized by a third)
QVR_TUNABLE(vr_smallgibs_up, "7") // m/s up (randomized by a third; Quake falls three times as fast as the world)
QVR_TUNABLE(vr_smallgibs_speed_melee, "1") // Speed times this: a hand's blow: a blade, a punch, a pommel, a thing in the fist; a bash, a headbutt
QVR_TUNABLE(vr_smallgibs_up_melee, "1") // Up times this
QVR_TUNABLE(vr_smallgibs_speed_saw, "1") // Speed times this: the chainsaw's chain or swing
QVR_TUNABLE(vr_smallgibs_up_saw, "1") // Up times this
QVR_TUNABLE(vr_smallgibs_speed_guns, "1") // Speed times this: shotguns, nails, lasers, lightning, a grunt's gun
QVR_TUNABLE(vr_smallgibs_up_guns, "1") // Up times this
QVR_TUNABLE(vr_smallgibs_speed_explosions, "1") // Speed times this: rockets, grenades, every blast
QVR_TUNABLE(vr_smallgibs_up_explosions, "1") // Up times this
QVR_TUNABLE(vr_smallgibs_speed_thrown, "1") // Speed times this: a thrown weapon, rock, brick or gib; a projectile batted back
QVR_TUNABLE(vr_smallgibs_up_thrown, "1") // Up times this
QVR_TUNABLE(vr_smallgibs_speed_other, "1") // Speed times this: monsters' blows, the rest
QVR_TUNABLE(vr_smallgibs_up_other, "1") // Up times this
QVR_TUNABLE(vr_smallgibs_speed_gibbing, "1") // Speed times this: a living monster gibbed (with its gibs' flight at 0.6)
QVR_TUNABLE(vr_smallgibs_up_gibbing, "0.7") // Up times this
QVR_TUNABLE(vr_smallgibs_speed_corpse, "1") // Speed times this: a corpse gibbed (with its gibs' flight at 0.6)
QVR_TUNABLE(vr_smallgibs_up_corpse, "0.7") // Up times this
QVR_TUNABLE(vr_smallgibs_speed_burst, "1") // Speed times this: a large gib or head burst
QVR_TUNABLE(vr_smallgibs_up_burst, "1") // Up times this
QVR_TUNABLE(vr_smallgibs_size_min, "0.65") // their size: from this times a rock's
QVR_TUNABLE(vr_smallgibs_size_max, "1.15") // to this
QVR_TUNABLE(vr_smallgibs_mass, "0.3") // kg each (light: thrown, they hurt little)
QVR_TUNABLE(vr_smallgibs_blow_grace, "0.3") // s after it is made a small gib isn't struck, nudged or pushed by a hand, a weapon or a held thing (the blow that tore it out batted it away)
QVR_TUNABLE(vr_smallgibs_stomp_speed, "200") // ... landing at least this fast (units/s down: a jump from the same level lands at about 270, a step down under 100)
QVR_TUNABLE(vr_smallgibs_brains_burst, "16") // brain chunks from a head gib destroyed
QVR_TUNABLE(vr_smallgibs_brains_pop, "16") // brain chunks from a head pop (a headshot that bursts the head)
QVR_TUNABLE(vr_smallgibs_brains_size, "0.8") // their size: times the small gibs' (vr_smallgibs_size_min/max)
QVR_TUNABLE(vr_smallgibs_brains_mass, "0.15") // kg each (lighter than a small gib of meat)
QVR_TUNABLE(vr_smallgibs_brains_speed, "1.8") // m/s brain chunks fly out of a head burst (their own Speed: vr_smallgibs_speed is the meat gibs'; times vr_smallgibs_speed_burst)
QVR_TUNABLE(vr_smallgibs_brains_up, "8.2") // m/s up (their own Up; times vr_smallgibs_up_burst)
QVR_TUNABLE(vr_smallgibs_brains_gloss, "1") // their wet sheen (0 none .. 2), always on: a gloss without the wet coat's darkening, a warm rim (vr_wounds.cpp VR_AliasWetCoat)
QVR_TUNABLE(vr_smallgibs_brains_jiggle, "1") // they wobble when knocked (a landing, a bounce, a jerk): drawn squashed along the knock and bulged across, a damped spring (vr_brainjiggle.cpp); the kick times this, 0 off

// Debug > Ragdoll Tool - Joint Groups
QVR_TUNABLE(vr_ragdoll_limits_legs, "1") // the legs' joints' limits times this too (with vr_ragdoll_limits; the next ragdoll made)
QVR_TUNABLE(vr_ragdoll_limits_torso, "1") // ... the torso's (the spine, a tail)
QVR_TUNABLE(vr_ragdoll_limits_arms, "1") // ... the arms'
QVR_TUNABLE(vr_ragdoll_limits_head, "1") // ... the neck's and jaw's
QVR_TUNABLE(vr_ragdoll_active_hertz_legs, "1") // times vr_ragdoll_active_hertz for the legs' joints
QVR_TUNABLE(vr_ragdoll_active_hertz_torso, "1") // ... the torso's (the spine, a tail)
QVR_TUNABLE(vr_ragdoll_active_hertz_arms, "1") // ... the arms'
QVR_TUNABLE(vr_ragdoll_active_hertz_head, "1") // ... the neck's and jaw's
QVR_TUNABLE(vr_ragdoll_active_damping_legs, "1") // times vr_ragdoll_active_damping for the legs' joints
QVR_TUNABLE(vr_ragdoll_active_damping_torso, "1") // ... the torso's
QVR_TUNABLE(vr_ragdoll_active_damping_arms, "1") // ... the arms'
QVR_TUNABLE(vr_ragdoll_active_damping_head, "1") // ... the neck's and jaw's
QVR_TUNABLE(vr_ragdoll_active_torque_legs, "1") // times vr_ragdoll_active_torque (a motor's most force) for the legs' joints
QVR_TUNABLE(vr_ragdoll_active_torque_torso, "1") // ... the torso's
QVR_TUNABLE(vr_ragdoll_active_torque_arms, "1") // ... the arms'
QVR_TUNABLE(vr_ragdoll_active_torque_head, "1") // ... the neck's and jaw's

// Movement > Grappling Hook
QVR_TUNABLE(vr_grapple_rope_spacing, "4") // units between the simulated rope's points (finer: truer round edges, more work; a long rope's are farther apart: 96 points at most)
QVR_TUNABLE(vr_grapple_rope_iterations, "32") // passes a frame holding its pieces to their length (more: stiffer, truer, more work)
QVR_TUNABLE(vr_grapple_rope_radius, "4") // the simulated rope's thickness (units, half of it): how far from what it lies on
QVR_TUNABLE(vr_grapple_rope_tail, "6") // units (times the hook's size) the drawn rope goes straight out of the hook's back before it bends (its last pieces point into the hook); 0: none
QVR_TUNABLE(vr_grapple_rope_depth, "2") // units inside the grappling gun's muzzle the rope is drawn from (in a hand, and a gun lying about)
QVR_TUNABLE(vr_grapple_loose_slack, "8") // units of slack the rope gets when the hook comes off (a loose hook lying there is not dragged by a step)
QVR_TUNABLE(vr_grapple_unreel_slack, "256") // units of slack the unreel (A/X) pays out beyond the load's distance when nothing pulls on the rope (you or the load on the ground): the rope comes off the drum and lies there; hanging, the load takes it out as ever. 4: the old unreel (nothing seen on the ground)
QVR_TUNABLE(vr_grapple_reel_home, "5") // units from the rope's end at the muzzle the loose hook's tail is reeled to before it goes back in (it was 9)
QVR_TUNABLE(vr_grapple_load_drag, "1") // a second: how much of its speed a load on the rope loses to the air (its swing relative to you; a gun hanging on its own: all of it), so a swing dies down
QVR_TUNABLE(vr_grapple_hang_drag, "1") // a second: how much of its speed what hangs on a rope with no one holding it loses (a gun left hanging from a ceiling: it stops swinging in a couple of seconds)
QVR_TUNABLE(vr_grapple_load_max_speed, "500") // units/s: the fastest a load on the rope moves relative to you (no rope of death); 0: no limit
QVR_TUNABLE(vr_grapple_move_share, "0.7") // the share of your walking (the thumbstick) a load hanging on the rope takes (the loose hook, a hooked prop): it trails a little rather than lagging far behind and snapping forward; 0: none
QVR_TUNABLE(vr_grapple_front_button_x, "6") // its place: from the back button along the gun (model units; +x towards the muzzle)
QVR_TUNABLE(vr_grapple_front_button_y, "0")
QVR_TUNABLE(vr_grapple_front_button_z, "0")
QVR_TUNABLE(vr_grapple_hook_scale, "0.6") // the size of the hook the grappling gun fires (its model; 1: Rogue's, too big for the gun), on the next shot
QVR_TUNABLE(vr_grapple_prop_light, "8") // kg up to which props come in at that speed; heavier, slower in proportion (40 kg: a fifth)
QVR_TUNABLE(vr_grapple_prop_anchor, "100") // kg from which a prop does not come: the reel pulls you to it
QVR_TUNABLE(vr_grapple_small_mass, "120") // kg (QC VR_Grapple_MonsterMass) up to which a monster is small: pulled fast, staggered
QVR_TUNABLE(vr_grapple_huge_mass, "400") // kg from which a monster is huge: the reel pulls you to it (in between: medium)
QVR_TUNABLE(vr_grapple_tow_light, "10") // kg up to which a hooked prop dragged along on a taut rope doesn't slow you; heavier, you drag it slower in proportion (40 kg, an explosive box: a quarter as fast)
QVR_TUNABLE(vr_grapple_max_length, "1500") // units: the longest the rope is (the hook flies no farther; the unreel pays out no more)
QVR_TUNABLE(vr_grapple_quick_speed, "1800") // units/s the quick release (the button on the gun) brings the hook back at, with vr_grapple_quick_time 0 (and the fastest it goes with it: a long rope takes longer)
QVR_TUNABLE(vr_grapple_reel_speed, "300") // units/s the reel pulls you in (to walls, huge monsters, props too heavy to move)
QVR_TUNABLE(vr_grapple_min_length, "40") // the shortest the reel takes the rope (units)
QVR_TUNABLE(vr_grapple_unreel_speed, "300") // units/s the hand's A/X pays the rope out at (lets you down, lets a load go farther); 0: no unreel
QVR_TUNABLE(vr_grapple_prop_speed, "300") // units/s a light prop comes in at (weapons, pickups, keys)
QVR_TUNABLE(vr_grapple_tow_speed, "320") // units/s you can drag a light prop at on a taut rope (walking away from it); heavier, slower (vr_grapple_tow_light); 0: no limit (it comes at your speed)
QVR_TUNABLE(vr_grapple_small_speed, "400") // units/s a small monster is reeled in at
QVR_TUNABLE(vr_grapple_medium_speed, "160") // and a medium one

// HUD and Menus > Other
QVR_TUNABLE(vr_gear_lights_sound_on, "vr/gadget_click_on.wav") // the button's clicks (empty: none)
QVR_TUNABLE(vr_gear_lights_sound_off, "vr/gadget_click_off.wav")
QVR_TUNABLE(vr_menu_grab_reach, "3") // a held menu panel: the hand's move towards or away from it counts this many times (1: as the hand moves)
QVR_TUNABLE(vr_menu_grab_dist_min, "30") // cm: nearest a held menu panel can be pulled to the head
QVR_TUNABLE(vr_menu_grab_dist_max, "600") // cm: farthest it can be pushed (a panel taken farther stays that far at most)
QVR_TUNABLE(vr_menu_grab_deadzone, "4") // cm a hand moves with a grip held on the panel's face before it takes hold of the panel (vr_menu_grab 2)

// Combat > Armed Grunts - Worn Backpacks
QVR_TUNABLE(vr_worn_backpack_lift_hold, "0.04") // worn backpacks: the pull kept past vr_worn_backpack_lift_force this long (s) lifts its wearer (a yank, not a jolt)
QVR_TUNABLE(vr_worn_backpack_lift_stiffness, "150") // worn backpacks: N a unit of the held point's lag behind the hand (the stretch's share of the pull)

// Debug > Tests
QVR_TUNABLE(vr_climb_prop_pull, "40") // kg: the most weight a hand on a light prop pulls it with (the player's, vr_box3d_player_mass, shared by the holding hands, towards the body; a body over the hold presses it down)
QVR_TUNABLE(vr_climb_prop_tilt, "10") // degrees: a held prop's top turned this far from how it lay when taken lets the hand go (it tipped over)
QVR_TUNABLE(vr_climb_prop_slip, "6") // units: a light prop's hold moved this far from where it was taken lets the hand go (pulled off its stack, slid away)
QVR_TUNABLE(vr_climb_prop_letgo_speed, "200") // units/s: a held prop's hold whose speed changes this much in a frame lets the hand go (knocked away: a blast, a hit)
QVR_TUNABLE(vr_hull_build_mb, "1024") // the memory the world's compiled hulls' builds at a load may hold at once (MB, estimated: 16 KB a brush of the world's a tree; vrstart: about 330 MB a tree, three at once): a build past it waits for one to end (one always runs); 0: no limit (vrstart's seven at once: about 1 GB more working set, 5-10% quicker)

// Debug > Profiling and Memory
QVR_TUNABLE(vr_heap_load_hold, "60000") // ms a map load keeps the memory it frees for its own later allocations (the heap's purge delay while it loads, vr_crtheap.c), all given back at its first frame drawn: 0 off (purged as vr_heap_purge_delay says; vrstart's hull build 1.7x slower)
QVR_TUNABLE(vr_image_cache_mb, "512") // decoded image files (a texture pack's world textures, material maps, skies, skins) kept across map loads, up to this many MB (vr_imgcache.cpp: keyed by the file itself; the least recently used go first; vr_image_cache_info); 0: none kept, each load decodes them again

// Debug > Reports
QVR_TUNABLE(vr_anim_smooth_rig_error, "1.5") // ... a bone whose fit to a frame is off by more than this (rms, units) isn't swung between it and its neighbours; a vertex off its bone by more swings less (none at twice it)
QVR_TUNABLE(vr_anim_smooth_rebuild_error, "0.5") // ... a vertex off its bone's fits by more than this (rms over the frames, units) is rebuilt less (not at twice it); a model with too few vertices rebuilt draws as before

// Gore > Blood Streams
QVR_TUNABLE(vr_gore_streams_chance, "0.6") // the chance a shot or a nail (each pellet) opens one
QVR_TUNABLE(vr_gore_streams_melee_blunt, "0.6") // the chance your blunt blow opens one (the crowbar, a punch, a pommel, a gun)
QVR_TUNABLE(vr_gore_streams_per_body, "4") // most on one body (a new one takes the weakest's place)
QVR_TUNABLE(vr_gore_streams_max, "24") // most in all (0-64)
QVR_TUNABLE(vr_gore_streams_time, "7") // seconds one pours, about (weakening all along)
QVR_TUNABLE(vr_gore_streams_speed, "50") // units/s it leaves the wound at, at its strongest
QVR_TUNABLE(vr_gore_streams_spread, "35") // degrees: each stream's way out turned at random up to this far from the wound's (one shotgun blast's streams diverge)
QVR_TUNABLE(vr_gore_streams_speed_var, "0.5") // each stream's force times a random 1 +- this (0: all alike)
QVR_TUNABLE(vr_gore_streams_wobble, "15") // degrees: each stream's way out wanders this far as it pours (slow noise; 0: steady)
QVR_TUNABLE(vr_gore_streams_wobble_rate, "0.7") // how fast it wanders (about this many swings a second)
QVR_TUNABLE(vr_gore_streams_thickness_var, "0.85") // how much its thickness varies as it pours (and so along it): 0 steady, 1 from a trickle to double
QVR_TUNABLE(vr_gore_streams_thickness_rate, "1.3") // how fast its thickness changes (about this many swells a second)
QVR_TUNABLE(vr_gore_streams_spurt_rate, "70") // drops a second at its strongest (fewer as it weakens)
QVR_TUNABLE(vr_gore_streams_spurt_cone, "15") // degrees round its way out they spurt in
QVR_TUNABLE(vr_gore_streams_spurt_speed, "1.35") // their speed, times the stream's
QVR_TUNABLE(vr_gore_streams_spurt_life, "0.4") // seconds they live (fading all along), about
QVR_TUNABLE(vr_gore_streams_spurt_outliers, "0.12") // the share of them that live longer,
QVR_TUNABLE(vr_gore_streams_spurt_outlier_life, "1.15") // this many seconds, about
QVR_TUNABLE(vr_gore_streams_spurt_size, "0.525") // units across, about (1 unit: 3.8 cm)
QVR_TUNABLE(vr_gore_streams_splash_size, "1.25") // their size, times the spurts'
QVR_TUNABLE(vr_gore_streams_smooth, "0.25") // seconds the body's motion at the wound, and its way out, are smoothed over: the animation's jumps don't fling it into ribbons (0: none)
QVR_TUNABLE(vr_gore_streams_inherit, "0.35") // the share of the body's (smoothed) motion it and its spurts take (0.7 before; 1 as a real one)
QVR_TUNABLE(vr_gore_streams_break, "4") // a piece of it stretched to this many times its length as poured breaks off (fading from half that), and it is cut where the wound jumps (0: never)

// Gore > Bloody Footprints and Handprints
QVR_TUNABLE(vr_footprint_count, "16") // ... this many after it (1..32) (his 16, 8 before vr_cfg_version 184)
QVR_TUNABLE(vr_footprint_size, "1.6") // ... times your foot's size (0.5..2)
QVR_TUNABLE(vr_handprint_count, "8") // ... at most this many from a hand soaked full, fainter as its blood is used up (1..32) (his 8, 6 before vr_cfg_version 185)
QVR_TUNABLE(vr_handprint_size, "1.6") // ... times your hand's size (0.5..2)

// Gore > Sticking
QVR_TUNABLE(vr_gore_stick, "8") // gibs flung into a ceiling or a wall may stick there about this many seconds, then fall (0 never)
QVR_TUNABLE(vr_gore_stick_speed, "20") // units/s a gib must strike a wall or a ceiling at to stick (flung or thrown)

// Combat > Burning - Fire Particles
QVR_TUNABLE(vr_fire_particles_count, "8") // exact particles in each emission burst
QVR_TUNABLE(vr_fire_particles_frequency, "10") // bursts per second per flame model
QVR_TUNABLE(vr_fire_particles_origin, "0.2") // emission height along flame: 0 base, 1 tip
QVR_TUNABLE(vr_fire_particles_speed_min, "0.1") // horizontal outward speed, m/s
QVR_TUNABLE(vr_fire_particles_speed_max, "0.4")
QVR_TUNABLE(vr_fire_particles_up_min, "0.3") // upward speed in world axes, regardless of torch tilt
QVR_TUNABLE(vr_fire_particles_up_max, "0.9")
QVR_TUNABLE(vr_fire_particles_life_min, "0.25")
QVR_TUNABLE(vr_fire_particles_life_max, "0.65")
QVR_TUNABLE(vr_fire_particles_size, "2.5")
QVR_TUNABLE(vr_fire_particles_alpha, "1")
QVR_TUNABLE(vr_fire_particles_range, "1216")

// Graphics > Loot Beams
QVR_TUNABLE(vr_lootbeam_mag_min, "0.25") // a magazine's beam only fuller than this share of its rounds (0..1; 0: any not empty)
QVR_TUNABLE(vr_lootbeam_opacity, "0.15") // its brightness (0..1)
QVR_TUNABLE(vr_lootbeam_length, "1.5") // metres it rises from the thing's middle
QVR_TUNABLE(vr_lootbeam_thickness, "0.03") // metres across its bright core (its soft glow three times that)
QVR_TUNABLE(vr_lootbeam_saturation, "0.75") // its colours' saturation (0 white)
QVR_TUNABLE(vr_lootbeam_fade_in, "0.5") // seconds it takes to fade in once the thing has rested vr_lootbeam_settle seconds
QVR_TUNABLE(vr_lootbeam_settle, "0.3") // seconds a thing must lie still (within a few units) before its beam shows
QVR_TUNABLE(vr_lootbeam_near, "0.5") // metres (across the floor) from your head within which it fades out, not in your face as you pick it up (0: never)
QVR_TUNABLE(vr_lootbeam_far, "12") // metres beyond which it fades out (over the last quarter; 0: never)
QVR_TUNABLE(vr_lootbeam_hue_weapons, "40") // each category's hue (degrees): weapons gold
QVR_TUNABLE(vr_lootbeam_hue_weapons_empty, "0") // a reloadable weapon lying with its magazine empty (or out; reloading on): red, so it is known as needing ammo before it is grabbed (his note 2026-10-29); below 0: the weapons' hue
QVR_TUNABLE(vr_lootbeam_hue_mags, "190") // ... magazines cyan
QVR_TUNABLE(vr_lootbeam_hue_ammo, "55") // ... ammo yellow
QVR_TUNABLE(vr_lootbeam_hue_health, "0") // ... health red
QVR_TUNABLE(vr_lootbeam_hue_armor, "120") // ... armour green
QVR_TUNABLE(vr_lootbeam_hue_powerups, "285") // ... power-ups violet
QVR_TUNABLE(vr_lootbeam_hue_keys, "225") // ... keys and runes blue
QVR_TUNABLE(vr_lootbeam_hue_backpacks, "25") // ... backpacks orange
QVR_TUNABLE(vr_lootbeam_hue_toolgun, "305") // a dropped toolgun's (a weapon, but its own): magenta, found among many weapons (his note vrfiringrange_2026-10-23_21-18-02)
QVR_TUNABLE(vr_lootbeam_toolgun_length, "2") // a dropped toolgun's beam this many times vr_lootbeam_length

// Gore > Gore
QVR_TUNABLE(vr_gore_sound_vol_spurt, "0.75") // a stump's spurts
QVR_TUNABLE(vr_gore_sound_vol_pour, "0.75") // blood streams splattering on the floor
QVR_TUNABLE(vr_gore_sound_vol_tear, "0.75") // limbs and heads torn off
QVR_TUNABLE(vr_gore_sound_vol_squelch, "0.75") // a hand taking a gib or a dead body
QVR_TUNABLE(vr_gore_sound_vol_splash, "0.75") // your foot splashing into a pool of blood
QVR_TUNABLE(vr_gore_sound_vol_step, "0.75") // your wet steps after a pool
QVR_TUNABLE(vr_gore_sound_vol_slap, "0.75") // gibs slapping into walls
QVR_TUNABLE(vr_gore_sound_vol_choke, "0.75") // the chokes (vr_gore_sound_choke 1)
QVR_TUNABLE(vr_gore_sound_vol_drip, "0.35") // drops of blood hitting the floor: yours (vr_body_blood), the ceilings' and stuck gibs' (vr_gore_drips) (his 0.35, 1 before vr_cfg_version 182)

// Graphics > Models and Effects > Light Grid (vr_ambient.cpp, "The light grid")
QVR_TUNABLE(vr_model_light_grid_intensity, "1") // the probes' light on models and what's drawn beside them, times this
QVR_TUNABLE(vr_model_light_grid_cell, "32") // the probes' spacing, in world units (16..256; a change builds the map's grid again; a huge map's doubles until it fits)
QVR_TUNABLE(vr_model_light_grid_near, "64") // each surface's say over a probe is over its distance + this: less, the nearest surfaces count more (a change builds the map's grid again)
QVR_TUNABLE(vr_model_light_grid_mix, "0.5") // 0: only the map's lights' directional shading on models, 0.5: half of it under the light around them, 1: only the light around them
QVR_TUNABLE(vr_model_light_lava, "1") // how much lava lights models and what's drawn beside them, in its colour (0: none)
QVR_TUNABLE(vr_model_light_lava_range, "512") // ... out to this far, fading (up to 768)
QVR_TUNABLE(vr_model_light_slime, "0.35") // ... slime
QVR_TUNABLE(vr_model_light_slime_range, "384")
QVR_TUNABLE(vr_model_light_teleport, "0.35") // ... teleporters
QVR_TUNABLE(vr_model_light_teleport_range, "256")

// Flatscreen > Held Weapon
QVR_TUNABLE(vr_flat_gun_ahead, "16.5") // the held weapon, hipfire, at a level look: how far in front of the shoulder the arm hangs from it sits (-30..30 units; 14 off the old anchor)
QVR_TUNABLE(vr_flat_gun_down, "2") // the held weapon, hipfire, at a level look: how far below that shoulder it sits (-30..30; positive is lower; 12 off the old anchor, which stood 12 higher)
QVR_TUNABLE(vr_flat_gun_out, "1") // the held weapon, hipfire, at a level look: how far out at the holding hand's own side of that shoulder it sits (-30..30; 4 off the old anchor; negative brings it across)
QVR_TUNABLE(vr_flat_gun_out_down, "-4") // Gun Out looking straight down (-30..30): from vr_flat_gun_out at a level look along a smooth curve, the gun in towards the middle
QVR_TUNABLE(vr_flat_gun_out_up, "-4") // Gun Out looking straight up (-30..30), the same
QVR_TUNABLE(vr_flat_gun_out_crouch, "-2") // Gun Out at a level look crouched (-30..30), by how deep the crouch is; looking up or down goes from it to their own
QVR_TUNABLE(vr_flat_gun_down_crouch, "-5") // Gun Down crouched (-30..30; positive is lower): vr_flat_gun_down's place while crouched, blended in by how deep the crouch is, as vr_flat_gun_out_crouch is
QVR_TUNABLE(vr_flat_gun_ahead_crouch, "16.5") // Gun Forward crouched (-30..30): vr_flat_gun_ahead's place while crouched, blended in the same way
QVR_TUNABLE(vr_flat_gun_out_prone, "1") // Gun Out lying down (vr_flat_key_prone), at a level look, by how far down he is; looking down or up goes from it to vr_flat_gun_out_prone_down / _up (-30..30)
QVR_TUNABLE(vr_flat_gun_down_prone, "6") // Gun Down lying down, at a level look, taking over from the standing or crouched one by how far down he is (-30..30; positive is lower)
QVR_TUNABLE(vr_flat_gun_ahead_prone, "16.5") // Gun Forward lying down, at a level look, the same way (-30..30)
QVR_TUNABLE(vr_flat_gun_out_prone_down, "-4") // Gun Out lying down, looking straight down (-30..30): from vr_flat_gun_out_prone along the same smooth curve as vr_flat_gun_out_down
QVR_TUNABLE(vr_flat_gun_out_prone_up, "-4") // Gun Out lying down, looking straight up (-30..30)
QVR_TUNABLE(vr_flat_gun_down_prone_down, "2") // Gun Down lying down, looking straight down (-30..30; positive is lower)
QVR_TUNABLE(vr_flat_gun_down_prone_up, "2") // Gun Down lying down, looking straight up (-30..30; positive is lower)
QVR_TUNABLE(vr_flat_gun_ahead_prone_down, "16.5") // Gun Forward lying down, looking straight down (-30..30)
QVR_TUNABLE(vr_flat_gun_ahead_prone_up, "16.5") // Gun Forward lying down, looking straight up (-30..30)
// Lying down with a gun in each hand (no stance lifts them as it lifts one gun), on top of the prone pose above, by how far
// down he is, each hand its own: units forward, out at its own side and up (-30..30), and the drawn gun's turn in degrees
// (-45..45: pitch muzzle up, yaw muzzle out, roll its top out; drawn only, the shots keep the crosshair). Until 2026-11-09
// none: the guns hung a hand's width over the floor, their grips in it at a level look, tilted up and stopped.
QVR_TUNABLE(vr_flat_dual_prone_main_fwd, "-2") // the main hand's gun, dual wielding lying down: further forward (units)
QVR_TUNABLE(vr_flat_dual_prone_main_out, "0") // ... out at its side (units; negative: in)
QVR_TUNABLE(vr_flat_dual_prone_main_up, "8") // ... up (units)
QVR_TUNABLE(vr_flat_dual_prone_main_pitch, "0") // ... the drawn gun's muzzle turned up (degrees)
QVR_TUNABLE(vr_flat_dual_prone_main_yaw, "0") // ... turned out (degrees)
QVR_TUNABLE(vr_flat_dual_prone_main_roll, "0") // ... its top canted out (degrees)
QVR_TUNABLE(vr_flat_dual_prone_off_fwd, "-2") // the off hand's gun, the same
QVR_TUNABLE(vr_flat_dual_prone_off_out, "0")
QVR_TUNABLE(vr_flat_dual_prone_off_up, "8")
QVR_TUNABLE(vr_flat_dual_prone_off_pitch, "0")
QVR_TUNABLE(vr_flat_dual_prone_off_yaw, "0")
QVR_TUNABLE(vr_flat_dual_prone_off_roll, "0")
QVR_TUNABLE(vr_flat_gun_collide, "1") // a held weapon goes back along the way into the body until it is out of the walls (0: it goes through)
QVR_TUNABLE(vr_flat_gun_near, "24") // the held weapon, hipfire: how far the drawn view may stand from his own head before the hands stop being kept on the drawn view and stay on the body (8..60 units; 24: the mode's camNear). The view at his eyes counts 0, a full crouch counts vr_flat_crouch_drop; the camera's own nudges (vr_flat_cam_fwd / _up / _side) are put back before the measure, so moving the eye does not read as "the view is somewhere else" and the gun keeps following it
QVR_TUNABLE(vr_flat_reach, "1") // times how far the hands sit from the head (0.25..3): the whole viewmodel's size
QVR_TUNABLE(vr_flat_pose_pitch, "65") // how far the hands follow the view up and down, in degrees (0..90; 90: the full look, as before)
QVR_TUNABLE(vr_flat_arm_hold, "1") // how far a drawn hand may sit from its arm's shoulder, times what the arm can hold it at (0..2; 0: no limit, the hands as placed)

// Flatscreen > Reload
QVR_TUNABLE(vr_flat_reload_down, "0.3") // the flat-screen reload (vr_flatreload.cpp): seconds the gun's hand takes to the nearest hip holster (the rounds go in as it gets there)
QVR_TUNABLE(vr_flat_reload_hold, "0.2") // seconds it stays there
QVR_TUNABLE(vr_flat_reload_up, "0.3") // seconds it takes back to its pose (it fires again after it)
QVR_TUNABLE(vr_flat_reload_angle, "90") // degrees the gun turns muzzle-down at the holster (0..150)
QVR_TUNABLE(vr_flat_ssg_break_delay, "0.15") // vr_flat_ssg_break: seconds after the shot that empties it the super shotgun breaks open, its shells thrown out (his words 2026-11-10: "a tiny customizable delay")
QVR_TUNABLE(vr_flat_mag_eject_delay, "0.1") // vr_flat_mag_eject: seconds after the last round a magazine gun drops its empty magazine
QVR_TUNABLE(vr_flat_mag_eject_speed, "40") // and its push out of the well (units a second, on the hand's velocity; VR's eject button's 40)
QVR_TUNABLE(vr_flat_reload_close_delay, "0.1") // seconds after the rounds go in at the hip the super shotgun snaps shut and the revolver's cylinder swings home (within the trip)

// Flatscreen > Hand Swap (vr_flatswap.cpp; vr_flat_swap_anim)
QVR_TUNABLE(vr_flat_swap_in, "0.15") // seconds the hands take to meet (the swap is sent as they do)
QVR_TUNABLE(vr_flat_swap_out, "0.2") // seconds they take back to their poses once swapped
QVR_TUNABLE(vr_flat_swap_gap, "3") // units apart they meet (a weapon in both hands: the free hand this far off the holding one)
QVR_TUNABLE(vr_flat_swap_down, "2") // units below the middle of the two hands they meet (-16..16)

// Flatscreen > Quick Grenade (vr_flatgrenade.cpp, QC vr_flatgrenade.qc; vr_flat_key_grenade). Places off the hand's
// shoulder as shares of the arm's reach: forward, out at the hand's own side, down (or up); angles Quake's (pitch down +).
QVR_TUNABLE(vr_flat_grenade_fetch_time, "0.3") // seconds the hand takes back to the pouch (the grenade is taken as it gets there)
QVR_TUNABLE(vr_flat_grenade_up_time, "0.3") // seconds it takes back into view with the grenade
QVR_TUNABLE(vr_flat_grenade_fetch_fwd, "-0.2") // the pouch trip's place: in front of the shoulder (negative: behind), level with the body
QVR_TUNABLE(vr_flat_grenade_fetch_out, "0.25") // out at its side
QVR_TUNABLE(vr_flat_grenade_fetch_down, "0.9") // below the shoulder
QVR_TUNABLE(vr_flat_grenade_fetch_pitch, "70") // the hand's pitch there (fingers down)
QVR_TUNABLE(vr_flat_grenade_hold_fwd, "0.85") // the held grenade's place: forward along the look
QVR_TUNABLE(vr_flat_grenade_hold_out, "0.1") // out at its side
QVR_TUNABLE(vr_flat_grenade_hold_down, "-0.05") // below the shoulder (negative: above; in view at the screen's lower corner)
QVR_TUNABLE(vr_flat_grenade_hold_pitch, "10") // its pitch against the look's
QVR_TUNABLE(vr_flat_grenade_wind_time, "0.25") // seconds the hand takes up into the overhead throw pose once the fire button winds up
QVR_TUNABLE(vr_flat_grenade_wind_fwd, "0.7") // the overhead pose: forward of the shoulder (negative: behind; 0.7 keeps the raised hand in view at the top corner), level with the body
QVR_TUNABLE(vr_flat_grenade_wind_out, "0.1") // out at its side
QVR_TUNABLE(vr_flat_grenade_wind_up, "0.5") // above the shoulder
QVR_TUNABLE(vr_flat_grenade_wind_pitch, "-45") // the hand's pitch there (negative: tipped back)
QVR_TUNABLE(vr_flat_grenade_wind_full, "1") // seconds of wind-up for the hardest throw (0.1..5)
QVR_TUNABLE(vr_flat_grenade_speed_min, "350") // the throw's speed let go at once (units/s)
QVR_TUNABLE(vr_flat_grenade_speed_max, "900") // and wound up fully (units/s)
QVR_TUNABLE(vr_flat_grenade_throw_up, "10") // degrees above the crosshair the throw leaves at (-30..60)
QVR_TUNABLE(vr_flat_grenade_follow_time, "0.15") // seconds of the throw's follow-through (the arm out along the look)
QVR_TUNABLE(vr_flat_grenade_follow_fwd, "0.9") // the follow-through's place: forward along the look
QVR_TUNABLE(vr_flat_grenade_follow_out, "0.05") // out at its side
QVR_TUNABLE(vr_flat_grenade_follow_down, "-0.05") // below the shoulder
QVR_TUNABLE(vr_flat_grenade_follow_pitch, "25") // its pitch against the look's
QVR_TUNABLE(vr_flat_grenade_back_time, "0.3") // seconds the hand takes from the follow-through back to its pose (an empty hand's rest, a weapon's grip)

// Flatscreen > Free Hands
QVR_TUNABLE(vr_flat_empty_fwd, "7.5") // an empty hand: how far in front of its shoulder it hangs (-30..30; 5 off the old anchor)
QVR_TUNABLE(vr_flat_empty_down, "1") // an empty hand: how far below its shoulder it hangs (-30..30; positive is lower; 13 off the old anchor)
QVR_TUNABLE(vr_flat_empty_out, "4") // an empty hand: how far out at its own side it hangs (-30..30; 7 off the old anchor)
QVR_TUNABLE(vr_flat_empty_hand_pose, "2") // an empty hand at rest: 0 relaxed, the arm down the body; 1 fists up, a guard; 2 the guard with both hands empty, relaxed while the other holds a weapon; -1 as before (Empty Hand Forward/Down/Out, fingers down)
QVR_TUNABLE(vr_flat_relaxed_fwd, "0.06") // the relaxed empty hand: how far in front of its shoulder, a share of the arm's reach (-1..1)
QVR_TUNABLE(vr_flat_relaxed_down, "0.94") // and how far below it (-1..1; positive is lower)
QVR_TUNABLE(vr_flat_relaxed_out, "0.16") // and how far out at its own side (-1..1)
QVR_TUNABLE(vr_flat_relaxed_pitch, "80") // and its fingers this far below level (degrees, -90..90)
QVR_TUNABLE(vr_flat_relaxed_roll, "0") // and rolled this far, its palm turned in towards the body (degrees, -180..180; mirrored for the left hand)
QVR_TUNABLE(vr_flat_guard_fwd, "0.5") // the guard's fist: how far in front of its shoulder, a share of the arm's reach (-1..1)
QVR_TUNABLE(vr_flat_guard_down, "-0.08") // and how far below it (-1..1; positive is lower: negative raises it to the chin)
QVR_TUNABLE(vr_flat_guard_out, "-0.04") // and how far out at its own side (-1..1; negative brings it in before the face; -0.12 until his notes 2026-11-08: the fists too close together)
QVR_TUNABLE(vr_flat_guard_pitch, "-30") // and its knuckles this far below level (degrees, -90..90; negative points them up)
QVR_TUNABLE(vr_flat_guard_roll, "0") // and rolled this far, turned in (degrees, -180..180; mirrored for the left hand)
QVR_TUNABLE(vr_flat_empty_take_time, "0.1") // a weapon taken into a hand at rest: seconds it comes up from the relaxed or guard pose to its own (0: at once, as before)
QVR_TUNABLE(vr_flat_support_fwd, "8.5") // the free hand beside the other hand's gun: how far in front of its shoulder it rests (-30..30; 6 off the old anchor)
QVR_TUNABLE(vr_flat_support_down, "3") // the same free hand: how far below its shoulder (-30..30; positive is lower; 15 off the old anchor)
QVR_TUNABLE(vr_flat_stance, "1") // a long gun with a two-handed hotspot (Weapon Offsets): the torso turns, the free hand's shoulder forward, and the free hand is drawn on that hotspot with its pose, as a look: no grip, two-handed aim, recoil or weight (vr_flatsight.cpp stanceAndLook; 0: the hand hangs, the torso faces the view)
QVR_TUNABLE(vr_flat_stance_turn, "15") // how far the torso turns for it (degrees, 0..45); the view doesn't turn
QVR_TUNABLE(vr_flat_stance_back, "4") // the gun's hand in the stance: this much nearer the shoulder than Gun Forward (units, times the reach)
QVR_TUNABLE(vr_flat_stance_in, "2") // and this much nearer the middle of the chest than Gun Out
QVR_TUNABLE(vr_flat_stance_up, "4") // and this much higher than Gun Down
QVR_TUNABLE(vr_flat_stance_pistol, "1") // a pistol (a gun with a two-handed hotspot, its muzzle under 9.5 from the hand) held with the other hand free: held in both hands too, in its own stance below (0: one hand, the other hangs)
QVR_TUNABLE(vr_flat_sight_learn, "1") // a gun with no known sights (the AD Widowmaker, Plasma Gun, soldiers' rifles): a sight line learnt from its mesh, so the flat stance holds it in both hands and the ironsight works (0: those stay one-handed, no ironsight)
QVR_TUNABLE(vr_flat_saw_autostart, "1") // flatscreen: the chainsaw's engine starts by itself with fuel in it, no ripcord (QC vr_chainsaw.qc; 0: the cord as in VR)
QVR_TUNABLE(vr_flat_melee_two_hand, "1") // flatscreen: a melee weapon with a grip ahead of the hand (the chainsaw's front handle) is held there by the free hand too, even off to its side (0: only grips in line with the hand)
QVR_TUNABLE(vr_flat_punch_aim, "0.7") // flatscreen: how much a punch's fist is thrown at what the crosshair is on (0: straight ahead from its side, kept near its own height, as before; 1: at it)
QVR_TUNABLE(vr_flat_shove_aim, "1") // flatscreen: how much the shove's hand pushes towards what the crosshair is on (0: ahead and a little in, as before)
QVR_TUNABLE(vr_flat_punch_aim_dist, "96") // the point on the crosshair's ray the punch and the shove go for: this far from the eye (units, 8..96)
QVR_TUNABLE(vr_flat_shove_both, "1") // flatscreen: with both hands free the shove pushes with both, the headset's two-handed shove (QC VR_FlatShove; 0: one hand, as before)
QVR_TUNABLE(vr_flat_shove_both_apart, "8") // both hands pushing: their places this far apart across the view at full push (units, 0..24)
QVR_TUNABLE(vr_flat_shove_width, "1.15") // the shove's sweep is this much wider round its line than the headset's (its pad; with Shove Reach 1.15 his notes' 15% more lenient)
QVR_TUNABLE(vr_flat_shove_anim_speed, "1") // the shove's push plays this much faster than Shove Time (0.25..4); it lands as the hand reaches out, and the cooldown is at least its time
QVR_TUNABLE(vr_flat_shove_main_fwd, "0") // the main hand at full push: this much further forward (units)
QVR_TUNABLE(vr_flat_shove_main_out, "0") // and out at its own side (units; negative: in towards the middle)
QVR_TUNABLE(vr_flat_shove_main_up, "0") // and up (units)
QVR_TUNABLE(vr_flat_shove_off_fwd, "0") // the off hand at full push: this much further forward (units)
QVR_TUNABLE(vr_flat_shove_off_out, "0") // and out at its own side (units; negative: in towards the middle)
QVR_TUNABLE(vr_flat_shove_off_up, "0") // and up (units)
QVR_TUNABLE(vr_flat_pistol_turn, "10") // the pistol's stance: how far the torso turns (degrees, 0..45)
QVR_TUNABLE(vr_flat_pistol_back, "2") // the pistol's hand: this much nearer the shoulder than Gun Forward (units, times the reach)
QVR_TUNABLE(vr_flat_pistol_in, "2") // and this much nearer the middle of the chest than Gun Out: both hands in front of him
QVR_TUNABLE(vr_flat_pistol_up, "4") // and this much higher than Gun Down
QVR_TUNABLE(vr_flat_ads_turn, "15") // the ironsight in a two-handed stance: the torso turns this much further (degrees, 0..30), the free hand's shoulder forward to the gun
QVR_TUNABLE(vr_flat_ads_lean, "10") // and the chest leans this far forward into the gun (degrees, 0..30); the head and the eye stay
QVR_TUNABLE(vr_flat_up_elbow, "1") // looking up with the free hand on a gun's hotspot, its elbow goes down under the gun and out instead of up in front of the eye: this share of it (0..1) from 15 deg up, all by 60
QVR_TUNABLE(vr_flat_up_elbow_out, "0.5") // and that elbow's way out at its own side, per its way down (0..2; 0: straight under the gun)
QVR_TUNABLE(vr_flat_wall_arm_keep, "9") // a wall pushes a hipfire gun back towards its shoulder until the hand is this far from it (0..20 units; less: further back)
QVR_TUNABLE(vr_flat_wall_fold, "5") // in the wall state (a hipfire gun pushed back or tilted at a wall) the arm may fold further, to this far from the shoulder (0..20 units; more than vr_flat_wall_arm_keep: no further), the stock to the chest; less: the gun comes nearer the eyes, more: it tilts more
QVR_TUNABLE(vr_flat_wall_margin, "4") // a hipfire gun, and the hand on it, counts as clear of a wall only this far off it: the push back and the tilt start this much sooner (0..24 units; 0: only once in it)
QVR_TUNABLE(vr_flat_mantle_hand_spread, "14") // flatscreen mantle: the hands on the ledge this far apart along its lip (units, 0..32: about his shoulders' width)
QVR_TUNABLE(vr_flat_hand_collide, "1") // flatscreen: empty hands (the guard, at rest) go back off walls and props as a held weapon does, and a punch or a weapon's swing stops at the surface (vr_flatplay.cpp handOffWall, vr_flatsight.cpp pushBack; 0: they go through, as before)
QVR_TUNABLE(vr_flat_punch_wall_margin, "-0.5") // a punch, a shove or a weapon's swing stops this far short of a wall (-12..12 units; negative: this far into it, so the blow still meets it: at 1 short the fist's wall blow never landed)
QVR_TUNABLE(vr_flat_wall_tilt, "85") // what that push back can't clear, the drawn muzzle tilts up out of, at most this far and never past straight up (0..85 degrees; 0: off). Drawn only: the shots still go through the crosshair
QVR_TUNABLE(vr_flat_wall_tilt_fixed, "0") // the tilt's pitch: 0 the least that clears the wall (at most vr_flat_wall_tilt); 1 always all of vr_flat_wall_tilt once the wall is met
QVR_TUNABLE(vr_flat_wall_tilt_yaw, "0") // and the tilted gun also turns this far across the body, muzzle inwards (-60..60 degrees; the left hand mirrored)
QVR_TUNABLE(vr_flat_wall_tilt_roll, "0") // and cants this far, its top inwards (-90..90 degrees; the left hand mirrored)
QVR_TUNABLE(vr_flat_wall_tilt_sighted, "0") // sighted, this share of that tilt (0..1; 0: the sights stay on the ray and the gun's end goes into the wall as before; none looking down past 50)
QVR_TUNABLE(vr_flat_wall_tilt_time, "0.2") // the tilted pose eases in, and back out off the wall, over this many seconds (0..1.5; 0: at once)
QVR_TUNABLE(vr_flat_wall_tilt_pivot, "0") // what it tilts round: 0 the hand, 1 its shoulder (the arm swings the gun up and back, the hand keeps its reach; it also brings the gun up into the face)
QVR_TUNABLE(vr_flat_wall_tilt_drop, "12") // sighted, the tilted gun also comes this far down off the eye (0..20 units; all of it by 45 degrees of tilt)
QVR_TUNABLE(vr_flat_wall_raise, "0.4") // in the wall state (hipfire), a weapon whose muzzle stands lower comes up until it is this share of the view's height up from its bottom edge (0..1; 0: off), up to 12 units and never into a ceiling: a pistol turned up stood below the view
QVR_TUNABLE(vr_flat_wall_elbow, "1") // a long gun pushed back by a wall: its own hand's elbow goes out at its side, the forearm across the chest, not into it (0..1)
QVR_TUNABLE(vr_flat_wall_slide, "1") // a hipfire weapon meeting a wall at a slant lies flat along it (turned level out of it, out along its normal) instead of going back and tilting up (0..1; 0: off)
QVR_TUNABLE(vr_flat_wall_slide_from, "15") // all along the wall up to this angle between the gun and the wall (degrees)
QVR_TUNABLE(vr_flat_wall_slide_to, "40") // and none from this angle (head-on: back and up); one smooth curve between (degrees)
QVR_TUNABLE(vr_flat_wall_slide_time, "0.12") // the slide's share and turn ease over about this many seconds (0..1; 0: at once)
QVR_TUNABLE(vr_flat_wall_block_ads, "1") // no ironsight while the gun is tilted up at a wall: the aim key waits until it comes down (0: the ironsight as ever)
QVR_TUNABLE(vr_flat_prone_floor, "1") // lying down, a gun that would go into the floor (looking down) lies flush on it instead: turned up out of it as at a wall (vr_flat_wall_tilt's turn, its fire and ironsight blocks: vr_flat_wall_block_fire / _ads); 0 off, its end in the floor
QVR_TUNABLE(vr_flat_prone_floor_margin, "1") // the flush gun keeps this far off the floor (0..6 units)
QVR_TUNABLE(vr_flat_prone_eye, "14") // lying down, his eyes this many units over his feet (6..vr_prone_eye less 2; until 2026-11-08 vr_prone_eye less 4, the same 14): the view, and the gun's poses with it (lower, a gun looking down cannot clear the floor)
QVR_TUNABLE(vr_flat_prone_neck_drop, "4.5") // lying down, the drawn trunk hangs from this many units further under the eye (0..10; the head is not drawn): the body turns until its hips rest on the floor, so the chest lies lower, almost on it (his note 2026-11-08: the torso too high; 0 before)
QVR_TUNABLE(vr_flat_flashlight_hide, "1") // flatscreen play: the torch's model is not drawn (its light still is)
QVR_TUNABLE(vr_flat_support_out, "3") // the same free hand: how far out at its own side (-30..30; 6 off the old anchor)

// Flatscreen > Model FOV
QVR_TUNABLE(vr_flat_model_fov, "0") // the player's own models and what leaves them drawn at this field of view, in the same terms as fov (0: off, as the world's; flatplay::modelFov). Looks only
QVR_TUNABLE(vr_flat_model_fov_near, "18") // the model FOV in full up to this far from the eye (units): past the elbows (16.5) and a hipfire muzzle (17.5), short of the hips (22)
QVR_TUNABLE(vr_flat_model_fov_far, "35") // and none past this far (units): the knees (35), so the shins and feet (47) stay on the floor

// Flatscreen > Ironsight
QVR_TUNABLE(vr_flat_ads_relief, "4") // the ironsight: how far from the eye the rear sight stands, along the sight line (3..30 units; the sights stay on the ray at any value), as it is at fov vr_flat_aim_fov_ref with no model fov: another fov or vr_flat_model_fov scales it so the sight picture keeps its size (flatsight::reliefScale). vr_flatsight.cpp solves the hand from the weapon's sight line
QVR_TUNABLE(vr_flat_ads_wall_keep, "8") // the ironsight: a wall or the floor pushes the rear sight no nearer the eye than this (3..30 units); past it the gun's end enters the surface and is hidden there, and no gun looms
QVR_TUNABLE(vr_flat_ads_screen_x, "0") // the ironsight: the whole sight picture moved right on the screen, percent of its height (-25..25; 0: the centre). The crosshair and the shots go with it
QVR_TUNABLE(vr_flat_ads_screen_y, "0") // the same, up (negative: down)
QVR_TUNABLE(vr_flat_ads_near, "24") // vr_flat_gun_near with the ironsight on, eased to it over the same seconds (8..60; 24: the same distance, so the ironsight changes nothing about it by default)
QVR_TUNABLE(vr_flat_aim_fov, "45") // the flatscreen ironsight (vr_flat_aim, the right mouse button with one weapon held; vr_flatsight.cpp): how many degrees the field of view narrows while it is on, at fov vr_flat_aim_fov_ref (0..75; 0: the view stays wide); at any other fov the same magnification (tangents), so a fov change keeps the zoom. The look's speed keeps its measure (in_sdl.c IN_FovScale) and the HUD is unchanged
QVR_TUNABLE(vr_flat_aim_fov_ref, "95") // the fov vr_flat_aim_fov's degrees and vr_flat_ads_relief's units are counted at (60..130; 95: his; at fov 90 vr_flat_aim_fov 12/30/45 take 11.9/29.5/43.7 off)
QVR_TUNABLE(vr_flat_aim_time, "0.15") // the same ironsight: how long its whole travel takes, in seconds -- onto the crosshair, and back off it (0.05..1.00; 0: at once). The field of view is narrowed by vr_flat_aim_fov's degrees times how far that travel has gone, so the view closes and opens with the weapon instead of at the press
QVR_TUNABLE(vr_flat_aim_point, "1") // 1: a gun points from its muzzle at what the ray through the screen's centre hits, every frame, hipfire and ironsight (the shot goes through the crosshair at any range); 0: along the view, beside the crosshair by the muzzle's distance off the ray

// Flatscreen > Eye
QVR_TUNABLE(vr_flat_eye_pivot, "1") // 1: the flatscreen look's pitch turns the eye about a point in his neck, as a head does (a look down carries it forward, clear of his chest; a level look's eye is where it always was); 0: the eye turns about itself
QVR_TUNABLE(vr_flat_eye_fwd, "1") // the eye off that point, forward at a level look (0..8 units)
QVR_TUNABLE(vr_flat_eye_up, "4") // the same, up (0..8 units)
QVR_TUNABLE(vr_flat_eye_lean, "12") // the chest's share of a look carries the head with it, about a point in the spine this far under the neck (0..30 units; with vr_flat_body_pose)
QVR_TUNABLE(vr_flat_cam_fwd, "0") // how far the flatscreen eye is nudged forward, in the view's frame (-20..20 units; + is forward, towards what he looks at)
QVR_TUNABLE(vr_flat_cam_up, "0") // the same, up (-20..20; + is higher; the crouch's own drop (vr_flat_crouch_drop) is added to it, not replaced by it)
QVR_TUNABLE(vr_flat_cam_side, "0") // the same, sideways (-20..20; + is the view's own right, which is his left when he looks at it on the screen)

// Flatscreen > Body Pose
QVR_TUNABLE(vr_flat_body_pose, "1") // 1: the hands stand on the body: on the shoulders the arms hang from, the chest takes a share of the look, the crouch lowers the shoulders, with limits (0: the view-fixed pose before it)
QVR_TUNABLE(vr_flat_chest_down, "0") // the chest's share of a look down (0..1): 0.3 of 85 degrees bends it 25
QVR_TUNABLE(vr_flat_chest_up, "0.2") // the chest's share of a look up (0..1)
QVR_TUNABLE(vr_flat_arm_down, "0.7") // the arm's share of a look down, about its shoulder (0..1.5)
QVR_TUNABLE(vr_flat_arm_up, "0.85") // the arm's share of a look up, about its shoulder (0..1.5): lifts the hand over the shoulder
QVR_TUNABLE(vr_flat_arm_out_down, "3") // a look straight down carries the hand this far out at its own side, past the chest (-10..10 units)
QVR_TUNABLE(vr_flat_arm_out_up, "2.5") // and a look straight up, this far (-10..10 units)
QVR_TUNABLE(vr_flat_stance_fade, "0.9") // how much of the stance's pull towards the shoulder a look up or down takes off (0..1)
QVR_TUNABLE(vr_flat_stance_fade_crouch, "0.6") // and how much a full crouch takes off (0..1)
QVR_TUNABLE(vr_flat_crouch_shoulder_down, "3") // the crouch: his shoulders this much further under the eye than a standing body's (-10..10 units)
QVR_TUNABLE(vr_flat_crouch_gun_out, "2") // the crouch: a held weapon's hand this much further out at its side, the stock off the chest the crouch leans forward (-10..10 units)
QVR_TUNABLE(vr_flat_crouch_shoulder_fwd, "2") // the crouch: his shoulders this much further forward under the eye (-10..10 units)
QVR_TUNABLE(vr_flat_clear_chest, "4") // a hand keeps this far ahead of its shoulder when it is below the shoulder: clear of the chest (0..15 units, eased in over the next 3)
QVR_TUNABLE(vr_flat_clear_eye, "3.5") // a hand keeps this far from the eye (0..15 units, eased in over the next 2)
QVR_TUNABLE(vr_flat_clear_stock, "1") // the held weapon's rear (its stock) keeps this far out of the torso, the gun pushed forward along the view (0..6 units, eased in over the next 1.5)
QVR_TUNABLE(vr_flat_keep_arm, "0.92") // a hand keeps within this share of what its arm reaches, eased in over the last 0.1 (0.3..1)
QVR_TUNABLE(vr_flat_keep_grip, "0.87") // a long gun's hand keeps its hotspot within this share of the free arm's reach, eased in over the last 0.1 (0.3..1.5)
QVR_TUNABLE(vr_flat_screen_angle, "55") // a hand keeps within this angle off the middle of the view (20..85 degrees): on the screen
QVR_TUNABLE(vr_flat_screen_angle_stance, "75") // the same, for a long gun's hand in the stance: by his cheek, the gun comes up out of the corner
QVR_TUNABLE(vr_flat_gun_view_fwd, "0") // on top of all that, a held weapon's hand moved along the view (-10..10 units): fixed to the view at any look
QVR_TUNABLE(vr_flat_gun_view_right, "0") // the same, to the view's right
QVR_TUNABLE(vr_flat_gun_view_up, "0") // the same, the view's up

// Flatscreen > Crouch
QVR_TUNABLE(vr_flat_crouch_time, "0.25") // Left Ctrl / +flatcrouch: the seconds it takes to ease down to a full crouch, and back up (0.05..2)
QVR_TUNABLE(vr_flat_crouch_smooth, "1") // the crouch's ease: 1 starts and ends gently (smoothstep: the view, the body, the arms and the weapon all follow it), 0 at an even speed
QVR_TUNABLE(vr_flat_crouch_drop, "18") // Left Ctrl / +flatcrouch: how far the crouch puts his head and eyes down (0..40 units, about 0.65 m); the crouched boxes (vr_crouch_height, vr_crouch_step) and the body follow
QVR_TUNABLE(vr_flat_slide_min, "220") // the slide (vr_flat_slide): the least speed along the floor (units/s) a crouch starts a slide at; slower, it is a plain crouch (a walk is 200, a run 320)
QVR_TUNABLE(vr_flat_slide_boost, "80") // the slide: units/s added to his speed as it starts (never past vr_flat_slide_max)
QVR_TUNABLE(vr_flat_slide_friction, "180") // the slide: units/s it loses each second on flat ground (a run's 380 slides about a second, 190 units)
QVR_TUNABLE(vr_flat_slide_slope, "900") // the slide: units/s gained each second down a slope, times the slope's sine (a 20 degree slope: 308, more than the friction: faster); up a slope the same is lost
QVR_TUNABLE(vr_flat_slide_max, "560") // the slide: the most speed along the floor it reaches downhill (units/s)
QVR_TUNABLE(vr_flat_slide_end, "130") // the slide: under this speed (units/s) it ends in a crouch
QVR_TUNABLE(vr_flat_slide_cooldown, "0.5") // the slide: seconds after one ends before a crouch starts the next
QVR_TUNABLE(vr_flat_slide_steer, "45") // the slide: degrees a second the move keys turn it (0: straight on)
QVR_TUNABLE(vr_flat_slide_drop, "6") // the slide: how much further than the crouch his eyes go down (units, 0..14)
QVR_TUNABLE(vr_flat_slide_body_angle, "80") // the slide: the trunk leant back at most this many degrees from upright (vr_avatar_pitch.cpp): it leans back until the hips sit vr_flat_slide_hip_floor over the floor, the legs out in front along it (45 until his 80, 2026-11-08: then the lean itself, which floated the body)
QVR_TUNABLE(vr_flat_slide_hip_floor, "4.5") // the slide: units the hips (the pelvis joint) sit over the floor, the buttocks on it: the trunk leans back about the neck until they are there (0..12)
QVR_TUNABLE(vr_flat_slide_turn_smooth, "0.15") // the slide: seconds the hips and legs take to follow a change of the slide's way or of the view (95%; 0: at once, the old steps)
QVR_TUNABLE(vr_flat_gun_ahead_slide, "0") // the held weapon in a slide: moved this far forward of its pose (units, -30..30), by the slide's share; vr_flat_gun_down_slide, vr_flat_gun_out_slide the same down and out
QVR_TUNABLE(vr_flat_gun_down_slide, "6") // the held weapon in a slide: moved this far down (units, -30..30; positive is lower)
QVR_TUNABLE(vr_flat_gun_out_slide, "2") // the held weapon in a slide: moved this far out at the holding hand's side (units, -30..30)
QVR_TUNABLE(vr_flat_slide_neck_back, "10") // the slide: how far behind the eye the leant-back trunk hangs from (units, 0..20): the chest under the view, the legs in front seen past it
QVR_TUNABLE(vr_flat_slide_body_time, "0.2") // the slide: seconds the body and the view take into the slide pose and out of it
QVR_TUNABLE(vr_flat_slide_leg_reach, "0.993") // the slide: the legs out in front reach this share of their length from hip to foot, the foot on the floor (0.85..1; 1: dead straight, less bends the knees up)
QVR_TUNABLE(vr_flat_slide_turn_time, "0.12") // the slide: seconds the two-handed stance's turn of the torso takes to go (and come back after): sliding, the trunk faces the view or the slide's way
QVR_TUNABLE(vr_flat_slide_legs, "1") // the slide: the hips and legs point the slide's way, the chest turned back to the view (at most vr_flat_slide_legs_twist); 0: the whole body faces the view
QVR_TUNABLE(vr_flat_slide_legs_twist, "70") // the slide: the most degrees the hips and legs turn off the view to the slide's way (0..90)
QVR_TUNABLE(vr_flat_slide_bob, "0") // the slide: this share of the walk's bob stays (the view's, the hands', the body's; 0..1); 0: none, the feet stay on the floor
QVR_TUNABLE(vr_flat_slide_sound, "0.6") // the slide: the scrape's volume (0..1): a scuff as it starts, a scrape that follows the speed and fades as it slows; 0 silent
QVR_TUNABLE(vr_flat_slide_stamina, "15") // the slide: stamina it costs (0..100; the parry stamina's pool, the visor's STAM), in vr_flat_slide_stamina_parts parts; with less than one part left a crouch at speed is no slide; 0 free
QVR_TUNABLE(vr_flat_slide_stamina_parts, "3") // the slide: its stamina is taken in this many equal parts (1..6), the first as it starts, the rest vr_flat_slide_stamina_step s apart; a part not yet due when it ends isn't charged
QVR_TUNABLE(vr_flat_slide_stamina_step, "0.2") // the slide: seconds between its stamina's parts (0..1; 0: all as it starts)
QVR_TUNABLE(vr_flat_slide_charge, "1") // the slide: sliding into a monster at vr_flat_slide_charge_speed or faster knocks it as a shoulder charge does (QC VR_KdTrig_SlideCharge: the Shoulder Charges' knock by weight and their chance); 0 off
QVR_TUNABLE(vr_flat_slide_charge_speed, "250") // the slide's charge: the least speed (units/s) it knocks a monster at (100..600); the knock grows with the speed over it
QVR_TUNABLE(vr_flat_slide_charge_stamina, "0") // the slide's charge: stamina one that connects costs on top of the slide's (0..100; the shoulder charge's own is vr_kdtrig_tackle_stamina)
QVR_TUNABLE(vr_flat_slide_prop_push, "1") // the slide: a prop slid into is pushed on at this share of the slide's speed (0..2), shared by his weight and its own (vr_box3d_player_mass); 0: the walk's shove
QVR_TUNABLE(vr_flat_slide_prop_side, "20") // the slide: degrees off the slide's way a prop slid into goes, to the side it is off his line (0..60)
QVR_TUNABLE(vr_flat_slide_foot_roll, "20") // the slide's pose: degrees both feet roll to the right about the slide's way (-60..60; negative: to the left)
QVR_TUNABLE(vr_flat_slide_knee_bend, "0.25") // the slide's pose: one leg's foot drawn in this share of the leg's length, its knee bent up (0..0.6; 0: both legs out alike)
QVR_TUNABLE(vr_flat_slide_knee_leg, "0") // the slide's pose: which leg's knee is bent up (0 the left, 1 the right)
QVR_TUNABLE(vr_flat_lean_angle, "20") // the lean (Q / E, vr_flat_lean): degrees the view and the held weapon roll at a full lean (0..35)
QVR_TUNABLE(vr_flat_lean_dist, "20") // the lean: how far the eye moves sideways at a full lean (units, 0..30); the body tilts from the hips to follow it
QVR_TUNABLE(vr_flat_lean_pivot, "30") // the lean: the eye's distance from the point it swings round (units): it goes down as it goes out, sqrt(pivot^2 - dist^2) short of the pivot
QVR_TUNABLE(vr_flat_lean_arms, "1") // the lean: how much of the upper body's turn the arms and guns take (0..1): 1 the eye, the arms and the guns move as one rigid body; 0 the arms only move with the eye, unturned
QVR_TUNABLE(vr_flat_lean_arms_pivot, "0") // the lean: how far under the eye the upper body turns from (units, 0..120): its turn is the eye's sideways move's angle there, asin(move / pivot); 0 the view's lean roll (vr_flat_lean_angle), so the guns keep their place on the screen; vr_flat_lean_pivot's 30 turns them with the eye's own arc (32 degrees at a full lean)
QVR_TUNABLE(vr_flat_lean_gun_roll, "1") // the lean: the held guns' roll, a share of the view's lean roll (vr_flat_lean_angle; 0..1): 1 they tilt with the screen, 0 they stay level in the world
QVR_TUNABLE(vr_flat_lean_time, "0.18") // the lean: seconds into a full lean and back
QVR_TUNABLE(vr_flat_lean_speed, "0.6") // the lean: his move keys' speed at a full lean, a share of their own (0.2..1)
QVR_TUNABLE(vr_flat_lean_margin, "6") // the lean: units the eye and the weapon keep off a wall at its side (the lean stops shorter)
QVR_TUNABLE(vr_flat_lean_prone_angle, "8") // the lean lying down: degrees the view and the held weapon roll at a full lean, the lying body rolling with them in place (0..35)
QVR_TUNABLE(vr_flat_lean_prone_dist, "3") // the lean lying down: how far the eye moves sideways at a full lean (units, 0..30)
QVR_TUNABLE(vr_flat_lean_hit, "1") // the lean: shots and missiles hit the leant upper body (two boxes from the hips to the head), out of his box; his box against the world stays as it was. 0: his box only

// Flatscreen > Looks
QVR_TUNABLE(vr_flat_sway, "1") // the drawn hands' sway as he starts, stops, jumps and lands (0..3): a lag that settles; 0 none
QVR_TUNABLE(vr_flat_bob, "1") // the drawn hands' bob on a walk, with his speed, and the breathing at rest (0..3); 0 none
QVR_TUNABLE(vr_flat_body_bob, "1") // the drawn body and hands ride this share of the view's own move (the walk's bob, a step's easing) together (0..1; 1: all, locked to the view; less: they move on the screen as one); sighted, all
QVR_TUNABLE(vr_flat_ads_sway, "0.15") // the ironsight keeps this share of the hands' sway (0..1; 0: none, the sights exact while moving)
QVR_TUNABLE(vr_flat_ads_bob, "0.15") // and this share of the walk's bob (0..1); no breathing sighted, so at rest the sights stay exact
QVR_TUNABLE(vr_flat_bob_fwd, "1") // the drawn body, arms and gun bob forward and back on a walk, in step with it, along the view's forward, pitch included (0..3; 1: 0.6 units each way at a run); 0 none. Separate from vr_flat_bob
QVR_TUNABLE(vr_flat_ads_bob_fwd, "0.15") // sighted, this share of it (0..1)
QVR_TUNABLE(vr_flat_ads_recoil, "0.1") // aiming down the sights, a shot's drawn recoil (back, rise, turn) is kept at this share (0..1; 1: as hipfire)
QVR_TUNABLE(vr_flat_turnlag, "2") // the drawn hands trail a turn and catch up (-10..10, at most vr_flat_turnlag_cap degrees each): "weapon inertia"; 0 the hands locked to the view; negative they lead the turn instead (GoldenEye's "turn with the camera")
QVR_TUNABLE(vr_flat_turnlag_speed, "1") // how fast they catch up (0.1..5, times a critically damped spring's 18/s)
QVR_TUNABLE(vr_flat_turnlag_cap, "10") // the most the turn's lag shows, times |vr_flat_turnlag| (0..30 degrees, softly)

// Flatscreen > Hit Feedback (vr_flathit.cpp; FLATPLAY.md, "Hit feedback")
QVR_TUNABLE(vr_flat_ads_hitreact, "0.2") // aiming down the sights, a hit's knock of the drawn arms and weapon (vr_painknock.cpp) is kept at this share (0..1; 1: as hipfire)
QVR_TUNABLE(vr_flat_hitpunch, "2") // a hit's punch of the view and the aim, degrees for a 20-point hit (more or less with the damage, 0.25x..2x; 0..20; 0 none)
QVR_TUNABLE(vr_flat_hitpunch_max, "6") // the most the punches add up to (0..45 degrees)
QVR_TUNABLE(vr_flat_hitpunch_pitch, "1") // its tip of the view along the blow: up from ahead, down from behind or above (-3..3; negative the other way)
QVR_TUNABLE(vr_flat_hitpunch_yaw, "1") // its turn along the blow: a hit from the right turns him left (-3..3; negative: toward the attacker)
QVR_TUNABLE(vr_flat_hitpunch_attack, "0.05") // seconds the punch takes to arrive (0..0.5)
QVR_TUNABLE(vr_flat_hitpunch_time, "0.3") // seconds it takes to ease back, the aim back where it was (0.01..3)
QVR_TUNABLE(vr_flat_hitpunch_ads, "0.5") // aiming down the sights, the punch is kept at this share (0..1)
QVR_TUNABLE(vr_flat_hitdir, "0.8") // the marks round the crosshair toward where a hit came from: their opacity (0..1; 0 none)
QVR_TUNABLE(vr_flat_hitdir_time, "1.5") // seconds a mark takes to fade out (0.05..5)
QVR_TUNABLE(vr_flat_hitdir_radius, "0.15") // how far from the crosshair a mark stands, a share of the screen's height (0.02..0.5)
QVR_TUNABLE(vr_flat_hitdir_width, "0.012") // a mark's thickness, a share of the screen's height (0.002..0.1)
QVR_TUNABLE(vr_flat_hitdir_arc, "40") // a mark's length round the crosshair (5..180 degrees)
QVR_TUNABLE(vr_flat_blood_health_size, "0.5") // low on health, how far in the screen's blood reaches (0..1; vr_blood_health_size in a headset)
QVR_TUNABLE(vr_flat_blood_health_opacity, "0.35") // and its strongest opacity at a beat (0..0.8; vr_blood_health_intensity in a headset)

// Combat > Embedded Blades: held stabs and swung axes (QC/vr_embedhold.qc; ROUND23.md, "Embedded blades, stage 2")
// Embedded Blades: Thrown
QVR_TUNABLE(vr_embed_sword_speed, "2.5") // m/s a thrown sword's tip must go at into what it meets
QVR_TUNABLE(vr_embed_sword_tip, "12") // % of the way from the hand to the tip that counts as its point when it goes in first
QVR_TUNABLE(vr_embed_sword_angle, "35") // degrees at most between the way the tip goes and the blade's line (more: side on, it bounces)
QVR_TUNABLE(vr_embed_sword_incidence, "65") // degrees at most between the way the tip goes and straight into the surface
QVR_TUNABLE(vr_embed_sword_wall, "0.4") // times that depth in walls, doors and props
QVR_TUNABLE(vr_embed_sag_stiffness, "170") // 1/s^2: how hard a wall holds a stuck sword's blade up (its gravity's pull 1.5 g / length: less, it sags further)
QVR_TUNABLE(vr_embed_sag_damping, "5") // 1/s: how fast its swing dies
QVR_TUNABLE(vr_embed_sag_most, "18") // degrees at most it sags
// Embedded Blades: Stab
QVR_TUNABLE(vr_embed_stab_speed, "2.5") // m/s the hand must drive a held sword or pointed blade at (a stab's own need is 2.4)
QVR_TUNABLE(vr_embed_stab_angle, "90") // degrees at most between the way its tip goes and the blade's line
QVR_TUNABLE(vr_embed_stab_tip, "100") // % of the blade from its tip that may be what strikes first
QVR_TUNABLE(vr_embed_sword_depth_min, "30") // % of the blade in at the least speed
QVR_TUNABLE(vr_embed_sword_depth_max, "60") // % of the blade in at vr_embed_sword_depth_speed
QVR_TUNABLE(vr_embed_sword_depth_speed, "8") // m/s
QVR_TUNABLE(vr_embed_enter_time, "0.12") // s a stab takes to go on to its depth (by its speed) once it has struck
QVR_TUNABLE(vr_embed_skewer_most, "3") // gibs a stab may skewer on a blade at once (0: none, a stab bursts them as any blow; QC/vr_skewer.qc)
QVR_TUNABLE(vr_embed_skewer_depth, "8") // units from the tip to the middle of the first gib on it
QVR_TUNABLE(vr_embed_skewer_gap, "7") // units along the blade between gibs on it
QVR_TUNABLE(vr_embed_skewer_throw_burst, "7") // m/s a thrown blade's gib must be going at to burst on what it meets itself (its fall not counted)
QVR_TUNABLE(vr_embed_skewer_swing_burst, "3") // m/s a swing must be going at to burst the gib where it hits something
QVR_TUNABLE(vr_embed_skewer_swing_reach, "6") // units past a gib's own width along the blade a swing's contact still bursts it
QVR_TUNABLE(vr_embed_skewer_thick, "1.5") // units: how thick the blade's line is for what a stab skewers (only what it truly goes through)
QVR_TUNABLE(vr_embed_skewer_tug_speed, "4") // m/s the other hand must yank a gib off the blade's line to tear it off (0: never)
QVR_TUNABLE(vr_embed_skewer_tug_stretch, "30") // cm the other hand must pull away from the blade's line to tear it off (0: never)
QVR_TUNABLE(vr_embed_skewer_mass_gib, "1.2") // kg a gib of a middling size weighs on a blade (a torso's piece)
QVR_TUNABLE(vr_embed_skewer_mass_limb, "0.6") // kg a limb of a middling size does (by size, not its share of the body: a soldier's leg was 12 kg)
QVR_TUNABLE(vr_embed_skewer_mass_head, "1.5") // kg a head does
QVR_TUNABLE(vr_embed_cuton, "1") // a head or limb a held blade's swing cuts off stays on it at the cut (QC VR_Skewer_CutOn): 0 never, 1 an edge's (the axes), 2 a point's too (the swords)
QVR_TUNABLE(vr_embed_cuton_fling, "4") // m/s: a part on the edge flies off at a sharp stop (to half within 0.25 s) from at least this fast: a hard swing, a hard shake (0: never; was 2.5, too easy: his note 2026-11-08)
QVR_TUNABLE(vr_embed_cuton_depth, "0.6") // share of a cut-off part's radius the edge sits in it, its middle ahead of the edge the way the swing went (0: its cut end on the edge, beside the blade; his note 2026-11-08)
// Embedded Blades: Axe
QVR_TUNABLE(vr_embed_swing_speed, "1") // m/s the hand must drive a swung axe at
QVR_TUNABLE(vr_embed_swing_angle, "90") // degrees at most the blow goes off the blade's plane
QVR_TUNABLE(vr_embed_axe_depth, "12") // cm an axe's edge goes in at twice its least speed, vr_embed on (half at it; at most 60% of the blade)
QVR_TUNABLE(vr_embed_hold_axe_stick, "2") // cm before an axe moves
QVR_TUNABLE(vr_embed_hold_axe_rate, "70") // cm/s an axe slides at most
QVR_TUNABLE(vr_embed_stagger_axe_speed, "9") // m/s a swung axe does
QVR_TUNABLE(vr_embed_stagger_axe, "0.75") // s an axe's does (times the body's Skewer, by how deep)
QVR_TUNABLE(vr_embed_hold_axe_tug, "20") // cm the hand pulls on past its depth to tug an axe out of the wound (his 2026-11-05: "I like that I have to pull out that much")
QVR_TUNABLE(vr_embed_hold_axe_tug_speed, "0.8") // m/s the hand's pull out must average over vr_embed_hold_axe_tug_time as it gets there: slower, the hand comes off and the axe stays (0: any)
QVR_TUNABLE(vr_embed_hold_axe_tug_time, "0.15") // s that average is over (not the last frame alone)
QVR_TUNABLE(vr_embed_tug_shake, "2.5") // degrees the axe (and the hand on it) shakes at most as the tug nears its distance (0: none)
QVR_TUNABLE(vr_embed_tug_shake_from, "0.4") // share of the tug distance from which it starts to shake
QVR_TUNABLE(vr_embed_lean_axe, "6") // degrees an axe held in a body turns with the hand on its handle, any way (0: rigid)
QVR_TUNABLE(vr_embed_move_axe_lift, "1.5") // times the body's weight an axe in it lifts at least (Skewer 1)
QVR_TUNABLE(vr_embed_move_axe_force, "600") // N an axe does
// Embedded Blades: Hold
QVR_TUNABLE(vr_embed_hold_follow, "15") // 1/s: how fast the blade follows the hand past the stick
QVR_TUNABLE(vr_embed_hold_sword_stick, "2") // cm the hand moves along the axis before a sword moves, barely in
QVR_TUNABLE(vr_embed_hold_sword_stick_deep, "8") // ... in as far as it goes
QVR_TUNABLE(vr_embed_hold_sword_rate, "100") // cm/s a sword slides at most, barely in
QVR_TUNABLE(vr_embed_hold_sword_rate_deep, "35") // ... in as far as it goes
QVR_TUNABLE(vr_embed_hold_sword_most, "90") // % of the blade it goes in at most
QVR_TUNABLE(vr_embed_hold_corpse, "0.5") // times the stick in a dead body (the rate divided by it)
QVR_TUNABLE(vr_embed_hold_corpse_tug_sword, "0.8") // m/s a pull out of a dead body must average (over vr_embed_hold_axe_tug_time) for a sword to come out 1:1 with the arm; slower, the body comes along (his 2026-11-05, "like the axe"; 0: always 1:1)
QVR_TUNABLE(vr_embed_hold_corpse_tug_axe, "0.8") // ... an axe (its tug distance still to go)
QVR_TUNABLE(vr_embed_hold_corpse_slip, "20") // cm past the stick a slower pull's hand may get ahead of a dead body that won't come along before it slips off (the blade stays in)
QVR_TUNABLE(vr_embed_slide_sound_cm, "3") // cm a blade slides, in or out, between two flesh sounds (0: none)
QVR_TUNABLE(vr_embed_slide_sound_vol, "0.35") // their volume
QVR_TUNABLE(vr_embed_slide_blood, "10") // how much blood spurts out of the wound at each of those steps, going in or out (0: none)
QVR_TUNABLE(vr_embed_wound_stream, "1") // how hard the blood stream (vr_bloodstream.cpp) of a wound with a blade in a living body pours, unending until the blade is out (0: none)
QVR_TUNABLE(vr_embed_hold_break, "45") // cm the hand may go off the axis before the blade comes free
QVR_TUNABLE(vr_embed_hold_yank, "6") // m/s across the axis it must also go at in a ragdoll: else it moves the body
QVR_TUNABLE(vr_embed_touch, "1.8") // cm the blade as drawn may be off the drawn model and still go in (vr_model_collide holds it 1 off; held stabs measured 0.7-1.5: 1.2 failed some 1 in 12)
QVR_TUNABLE(vr_embed_touch_wait, "0.12") // s after a blow that landed short the blade may still come to touch and stay in
QVR_TUNABLE(vr_embed_hold_reach, "90.5") // cm the hand may have gone further from where the blade went in before it comes free (a limb moved away)
QVR_TUNABLE(vr_embed_hold_reach_corpse, "4") // times that in a dead body (it falls away from the hand, the hold goes on)
QVR_TUNABLE(vr_embed_hold_reach_down, "1.5") // times the hold's reach for a body knocked down (it falls away from the hand)
QVR_TUNABLE(vr_embed_hold_slide_damage, "0") // damage per 10 cm slid deeper
QVR_TUNABLE(vr_embed_limb_follow, "6") // 1/s: how fast a blade in an arm or a leg takes the limb's turn (0: rigid)
QVR_TUNABLE(vr_embed_torso_follow, "25") // 1/s: how fast a blade in the torso, the pelvis or the head takes its turn (0: rigid)
// Embedded Blades: Moving the Body
QVR_TUNABLE(vr_embed_stagger_sword_speed, "5") // m/s a held stab goes at to stagger the body
QVR_TUNABLE(vr_embed_stagger_sword, "2.5") // s a sword's stagger lasts, in as deep as a stab goes, on a body of Skewer 1
QVR_TUNABLE(vr_embed_stagger_amount, "1") // how far a stab throws a body off balance, times its Skewer, by how deep
QVR_TUNABLE(vr_embed_stagger_twohand, "1.5") // times its time and its amount with both hands on the blade
QVR_TUNABLE(vr_embed_stagger_topple, "0.55") // the amount from which it may fall (under it, it stands on sure footed)
QVR_TUNABLE(vr_embed_stagger_shallow, "0.5") // times that barely in
QVR_TUNABLE(vr_embed_stagger_push, "0.3") // the stagger's push along the blow (a shove of two hands: 1)
QVR_TUNABLE(vr_embed_light_hold, "0.5") // the least Skewer (groups.txt) of a standing body for walking away to drag it
QVR_TUNABLE(vr_embed_move_twohand, "2.5") // times that force with both hands on the blade
QVR_TUNABLE(vr_melee_body_speed, "1") // of your body's speed towards what you hit, how much a melee blow's or a punch's speed gets for hitting and sticking
QVR_TUNABLE(vr_melee_body_damage, "0.25") // ... and for its damage
QVR_TUNABLE(vr_melee_body_min_part, "0.5") // a blade's blow that lands only for your body's speed counts on this outer share of the blade (0 any part)
QVR_TUNABLE(vr_embed_move_sword_force, "400") // N a sword barely in moves a body with at most, one hand, Skewer 1
QVR_TUNABLE(vr_embed_move_sword_force_deep, "1500") // ... in as far as it goes
QVR_TUNABLE(vr_embed_move_give, "0.4") // 1/s: how readily a staggering body's stance gives way (its mass over this: N for each m/s it is moved at)
QVR_TUNABLE(vr_embed_move_hertz, "24") // Hz: the stiffness of the hand's spring on the body (a limb grab's 8)
QVR_TUNABLE(vr_embed_move_at_wound, "1") // 1: the spring holds the body at the wound (follows closely); 0: at a point out at the hand, as before 2026-11-08 (it hung and swung from there: some 70% of a lift)
QVR_TUNABLE(vr_embed_drag_slack, "4") // cm further from the wound than the hand held it before walking away drags a standing body
QVR_TUNABLE(vr_embed_tether_speed, "0.35") // times your walking speed while a blade ties you to a body
QVR_TUNABLE(vr_embed_brake, "25") // 1/s: running into a stab you hold, your speed along the blade into the body falls to Brake: Speed this fast (0: never; his note 2026-11-08)
QVR_TUNABLE(vr_embed_brake_speed, "0.5") // m/s along the blade into the body you keep at most while braked
QVR_TUNABLE(vr_embed_props, "1") // 1: held stabs and swings stay in wooden crates and barrels as in bodies (his notes 2026-11-08; QC vr_embedhold.qc VR_Embed_PropHost)
QVR_TUNABLE(vr_embed_prop_force, "700") // N a blade held in a crate or barrel pulls it with at most along the floor, one hand (Moving: Two Hands with both; Blade Hold Strength): a small crate (40 kg) weighs some 830 N
QVR_TUNABLE(vr_embed_prop_lift, "0.5") // times its weight that pull lifts at most, one hand (two: Moving: Two Hands' times): dragged and tipped, not hoisted on one blade
QVR_TUNABLE(vr_embed_prop_hertz, "4") // Hz: how stiffly that pull holds the crate to the blade before its cap
QVR_TUNABLE(vr_embed_prop_stick, "2") // times how hard a blade in a crate slides, of in a body (and its slide's speed over it)
QVR_TUNABLE(vr_embed_move_sword_lift, "1") // times the body's weight a sword barely in lifts (Skewer 1; Moving: Sword's Force on top of it)
QVR_TUNABLE(vr_embed_move_sword_lift_deep, "2.5") // ... in as far as it goes: a grunt held up over your head
QVR_TUNABLE(vr_embed_lean_sword, "3") // degrees a sword held in a body turns with the hand on its handle, any way (0: rigid)
QVR_TUNABLE(vr_embed_free_ease, "0.25") // s the hand and the blade take from where it was in the body to the hand's own place once it comes out
QVR_TUNABLE(vr_embed_arm_stretch, "1.3") // how far the arm may stretch to a hand held on a blade in a body (vr_body_arm_stretch elsewhere; eased back)
QVR_TUNABLE(vr_embed_arm_stretch_ease, "0.2") // s it takes to come in and to go back to normal
QVR_TUNABLE(vr_embed_arm_letgo, "1") // cm past that stretch the drawn hand may be off its arm before the hand lets go of the blade
QVR_TUNABLE(vr_embed_handle_free, "1") // 1: a handle stuck in the body's model is turned out of it about the wound (checked 4 times a second), so a hand can take it
QVR_TUNABLE(vr_embed_tether_far, "10") // cm further from a dead body's wound than the hand held it, for that body to count: dragging it
QVR_TUNABLE(vr_drips_rate, "3") // water drops a second off a soaked thing out of its liquid (a monster's size; times the wetness on vr_drips_curve)
QVR_TUNABLE(vr_drips_curve, "2") // the rate's curve over the wetness (1 .. 0 as it dries in vr_wounds_wet_time): rate * wetness^this (1 linear; more: tapering off sooner)
QVR_TUNABLE(vr_drips_dry_motion, "1") // moving dries faster: times (1 + this * speed / 400 u/s, at most 1.5 more)
QVR_TUNABLE(vr_drips_size, "0.3") // a water drop's size (a particle's scale)
QVR_TUNABLE(vr_drips_color, "0.62 0.74 0.88") // a water drop's colour (lit as the place is, a little brighter)
QVR_TUNABLE(vr_drips_slime_color, "0.32 0.55 0.1") // a slime drop's
QVR_TUNABLE(vr_drips_alpha, "0.6") // a water or slime drop's opacity
QVR_TUNABLE(vr_drips_splash, "1") // 1: a water drop landing splashes a few tiny specks (no mark is left)
QVR_TUNABLE(vr_drips_volume, "0.3") // a water drop landing: its soft drip's volume (0 silent)
QVR_TUNABLE(vr_drips_sound_gap, "0.2") // real seconds at least between two water drips heard
QVR_TUNABLE(vr_drips_max, "8") // drops a frame at most from everything wet and wounded (round robin: many wet monsters share)
QVR_TUNABLE(vr_drips_view, "0.4") // your own body, hands and weapons drip this share of the rate (none close under the eyes)
QVR_TUNABLE(vr_drips_fling, "1") // a wet weapon or hand swung fast throws drops off: how many (0 none)
QVR_TUNABLE(vr_drips_blood_rate, "3") // blood drops a second off a wounded living monster at its last breath (vr_gore_wound_drips; less the less hurt: vr_drips_blood_min)
QVR_TUNABLE(vr_drips_blood_min, "0.3") // ... a barely wounded one drips this share of that rate (the rest grows with the health it lost); it never stops while wounded
QVR_TUNABLE(vr_drips_blood_health, "1") // ... it drips while its health is under this share of its full health (1: any wound)
QVR_TUNABLE(vr_drips_blood_size, "1.25") // ... its drops' size, times the gore's drip's
QVR_TUNABLE(vr_drips_blood_ragdoll, "20") // ... seconds its corpse or ragdoll drips on after it dies (tapering; a hit on it starts again; 0 none)
QVR_TUNABLE(vr_drips_blood_hit, "1") // ... a hit adds this many times its damage / 20 for a couple of seconds
QVR_TUNABLE(vr_player_gore_stream_chance, "0.5") // your own gore (vr_player_gore_streams): the chance a shot, a nail or a blade on you opens a stream
QVR_TUNABLE(vr_player_gore_stream_time, "0.6") // ... how long it pours, times an enemy's (vr_gore_streams_time)
QVR_TUNABLE(vr_player_gore_stream_max, "3") // ... streams on you at once (the weakest makes way)
QVR_TUNABLE(vr_player_gore_drip_rate, "0.6") // ... your wounds' drips (vr_player_gore_drips), times an enemy's rate (vr_drips_blood_rate; your arms' own drips too: vr_body_blood)
QVR_TUNABLE(vr_player_gore_splash_amount, "1") // ... drops a hit splashes out of the wound, times 6 (vr_player_gore_splash)
QVR_TUNABLE(vr_player_gore_eye_clear, "0.35") // ... metres: no stream pours, no drop splashes or drips this close to the eyes, nor from over them
QVR_TUNABLE(vr_drips_blood_marks, "0.25") // ... the chance a drop leaves a mark on the floor
