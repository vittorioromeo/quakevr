// vr_tunables.inc -- Quake VR's tuning constants: QVR_TUNABLE(name, default) (vr_cvars.hpp, "Tuning constants").
// Numbers tuned once that no player sets: no menu row, not in the config. Constexpr in a release build (read as
// name.value, name.string, or by name from the QC); an archived cvar in a tuning build (MSBuild -p:QvrTuning=1). A player
// setting is a QVR_CVAR in vr_cvars.inc; to make one of these a setting again, move its line back there and give it a
// menu row. The list per page and why: ROUND23.md, "Settings tiers".

// (no menu row) > (no menu row)
QVR_TUNABLE(vr_compat_muzzle, "1") // with another mod's progs, its shots start at the gun, not the player's head
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
QVR_TUNABLE(vr_snd_hrtf_sofa, "") // an HRTF of your own: a .sofa file in the game folder (empty: Steam Audio's)
QVR_TUNABLE(vr_snd_limiter_release, "0.15") // s, the limiter's gain back up after a peak (0.02-1)
QVR_TUNABLE(vr_obs_host, "127.0.0.1") // obs-websocket's address (OBS on another PC: its address)
QVR_TUNABLE(vr_bullettime_fx_tint, "0.85 0.95 1.15") // ... the colour it multiplies by (red green blue)
QVR_TUNABLE(vr_xr_res_warn, "1.3")
QVR_TUNABLE(vr_xr_res_warn_mpx, "6")
QVR_TUNABLE(vr_upscale_radius, "40") // ... only within this many degrees of the lens centre, bilinear beyond (0: the whole image)
QVR_TUNABLE(vr_upscale_sharpen_native, "0") // at vr_render_scale 1: FSR's RCAS sharpening alone (with vr_upscale 1 or 2 and some sharpness)
QVR_TUNABLE(vr_foveated_inner, "0") // ... full-rate shading within this many degrees of the lens centre (0: the preset's: 45, 35, 25)
QVR_TUNABLE(vr_foveated_outer, "0") // ... 2x2 up to this many degrees, 4x4 beyond (0: the preset's: 60, 50, 40)
QVR_TUNABLE(vr_touch_depth, "4") // ... or this far through it
QVR_TUNABLE(vr_touch_hover, "4") // in front within this: hovering (the widget under it lights up, a cursor shows)
QVR_TUNABLE(vr_touch_margin, "0.5") // a touch counts this far past the screen's edges (on the edge)
QVR_TUNABLE(vr_touch_debounce, "0.06") // s: a new touch no sooner after the last ended
QVR_TUNABLE(vr_doom_args, "") // more of Doom's command line (-file my.wad, -skill 4, -warp 1 1, -fast)
QVR_TUNABLE(vr_doom_attenuation, "2") // how fast they fade with distance (Quake's: 1 normal, 3 a small source; 0 everywhere)
QVR_TUNABLE(vr_doom_focus_angle, "25") // degrees off the view's middle the screen may be and count as looked at
QVR_TUNABLE(vr_doom_panel_distance, "0.6") // m: how far in front it is placed
QVR_TUNABLE(vr_note_replay, "1") // with vr_notes on: the last seconds kept (poses, buttons, the player, nearby entities) and saved beside each note as a motion take (vr_notectx.cpp, vr_motion.cpp)
QVR_TUNABLE(vr_note_replay_seconds, "30") // how many seconds that instant replay keeps (and the console lines and frame times saved with a note)
QVR_TUNABLE(vr_heavy_auto_from, "10") // kg: a weapon without its own Heft (-1) lighter than this is light (0)
QVR_TUNABLE(vr_heavy_auto_full, "20") // kg: and this heavy the heaviest (1)
QVR_TUNABLE(vr_ghost_hands_solid, "1") // 1 each ghost hand is one solid translucent object (its nearest surface only), 0 every face blended
QVR_TUNABLE(vr_debris_area_size, "384") // units: the side of an area
QVR_TUNABLE(vr_debris_exclude, "vrfiringrange vrclimb vrexample") // maps with none (names, spaces between)
QVR_TUNABLE(vr_debris_edicts_left, "2048") // entities left free for the game at least (max_edicts less the map's): fewer pieces if needed
QVR_TUNABLE(vr_crates_exclude, "vrfiringrange vrclimb vrexample vrcalibration") // maps with none (names, spaces between)
QVR_TUNABLE(vr_walltorch_pull, "8") // cm a hand gripping a torch on its wall pulls it before it comes out
QVR_TUNABLE(vr_bodytrack_fade, "0.25") // seconds a region's tracking eases in or out (lost, low confidence, switched)
QVR_TUNABLE(vr_water_splash_speed, "150") // units/s a thing (not a player) must move at to splash entering or leaving water
QVR_TUNABLE(vr_shadow_head, "1") // your shadow (vr_shadow_self) has your head, which the eyes never see (not for a light at the head: the head-mounted flashlight)
QVR_TUNABLE(vr_decal_melee_prop_speed, "7") // ... a prop (crate, rock, brick, weapon) dents the level hitting it this fast or faster (m/s: a hard throw; 0: never)
QVR_TUNABLE(vr_gore_streams_pools, "4") // most pools one stream spreads (a new one each time it moves on)
QVR_TUNABLE(vr_teeth_speed, "3") // m/s they fly out at (along the blow, scattering)
QVR_TUNABLE(vr_teeth_mass, "0.05") // kg each
QVR_TUNABLE(vr_smallgibs_brains_jiggle_speed, "0.8") // m/s: a change of their speed faster than this between two frames kicks it (less: wobblier)
QVR_TUNABLE(vr_weapon_relief, "1") // weapons made from other models as they load (AD's #vr, #gun, #melee; our rifle's #adv variants): 1 their relief made from the skin like our baked weapons' (vr_skinrelief.cpp, drawn as authored), 0 the plain made map; on their next load (a restart)
QVR_TUNABLE(vr_parallax_depth_distance, "0") // round 22: how far ahead (view depth, units) the depth write reaches; beyond, the surfaces' flat depth (0, the default: as far as the parallax, vr_parallax_distance, the same picture)
QVR_TUNABLE(vr_detail_fine, "1") // ... a second, finer grain within half that distance (one more texture read there)
QVR_TUNABLE(vr_taa_blend, "0.1") // the current frame's weight in the history (lower: smoother, more ghosting)
QVR_TUNABLE(vr_taa_clip, "1") // the history's variance clip, in standard deviations of the 3x3 (lower: less ghosting, more flicker)
QVR_TUNABLE(vr_taa_reactive, "0.6") // the current frame's weight on translucency, particles and liquids (no motion vectors)
QVR_TUNABLE(vr_water_geo_cell, "16") // ... the grid's cell in units (smaller: smoother, more triangles; remade on change)
QVR_TUNABLE(vr_flaregun_cooldown, "0.3") // s at least from a shot to the next (each a new pull)
QVR_TUNABLE(vr_flaregun_volume, "1") // its shot's volume (0..4)
QVR_TUNABLE(vr_nailpistol_volume, "1") // its shot's volume (0..4)
QVR_TUNABLE(vr_nailpistol_smoke, "1") // light smoke out of its side vents with each shot (vr_gunfx.cpp; 0 none, up to 3x; longer after a quick string)
QVR_TUNABLE(vr_revolver_volume, "1") // its shot's volume (0..4)
QVR_TUNABLE(vr_revolver_smoke, "1") // a wisp of smoke from its muzzle after a shot (0 none)
QVR_TUNABLE(vr_agarms_shoulder, "0.5") // vr_agarms: a long gun aimed is raised from its rest pose's hold (0: as it held its own rifle, its wrists as made) to its shoulder (1: the butt in its pocket, at its cheek) by this
QVR_TUNABLE(vr_agarms_clavicle, "0.6") // vr_agarms: the shoulder joint carried by a clavicle that takes this share of the arm's lift above level and of its reach forward and across (the shoulder shrugs and comes forward; its top turns with it); 0 none (before)
QVR_TUNABLE(vr_agarms_clavicle_most, "25") // vr_agarms: ... the clavicle turns at most this much up and this much forward (degrees)
QVR_TUNABLE(vr_agarms_shoulder_cone, "105") // vr_agarms_limits: the upper arm within this many degrees of the middle of its range (forward, out and a little down from the shoulder)
QVR_TUNABLE(vr_agarms_shoulder_twist, "80") // vr_agarms_limits: ... and twisted about itself at most this many degrees either way from its rest pose's
QVR_TUNABLE(vr_agrunt_second_reach, "1") // vr_agarms: 1 a long gun carried or aimed is brought in towards the off shoulder until its second grip is within the off arm's reach (as a chainsaw's front handle is: the laser cannon's); 0 where its rest pose's rifle is (before: the off hand slid back to the main handle or let go)
QVR_TUNABLE(vr_agrunt_atk_stand, "1") // armed humanoids whose own aiming frames crouch and lean back (the enforcer's: a rifle at the hip) stand as they aim and fire, their posed arms holding the weapon (with vr_agarms); 0 their aiming frames (before: the enforcer's belly pushed forward, its legs bent)
QVR_TUNABLE(vr_posestudio_reach, "2.5") // the Pose Studio (Grip Editor > Drag Mode): the armed humanoid nearest within this many metres of your head is the one shown and edited
QVR_TUNABLE(vr_adgun_tank_y, "0") // ... right
QVR_TUNABLE(vr_carry_two_hands_detach, "3") // with it: cm past the drift a hand may be off its grip before it lets go (its fist off the prop: pulled away, not sliding on it); both off: it drops (vr_carry2h.cpp detached)
QVR_TUNABLE(vr_held_fit_gaps, "") // ... and per model, added: "name=cm ...", a name matching the end of the model's (e.g. "backpack.mdl=0.5 b_bh25.bsp=-0.3")
QVR_TUNABLE(vr_hit_stagger_throw_ref, "60") // kg m/s: a thrown prop's or weapon's momentum (mass x speed into it) that makes impact 1 (a 6 kg crate at 10 m/s)
QVR_TUNABLE(vr_hit_stagger_melee_ref, "30") // a melee blow's damage (its zone's multiplier in) that makes impact 1
QVR_TUNABLE(vr_hit_stagger_damage, "1") // impact plus this x the hit's damage as a share of its full health
QVR_TUNABLE(vr_hit_stagger_push, "1") // its push, times a shove's (impact 1: a two-hand shove's; 0.5 .. 1.25)
QVR_TUNABLE(vr_ragimpact_body, "0.1") // share of the rest of the body's mass behind the struck part (0: the part alone)
QVR_TUNABLE(vr_ragimpact_whip, "2") // a part's speed counts up to this times its whole body's (a limb whipping on a throw is no blow), unless a hand holds it
QVR_TUNABLE(vr_ragimpact_held_mass, "20") // kg of your arm behind a part your hand holds (a head held and slammed: the hand drives it)
QVR_TUNABLE(vr_ragimpact_time, "2") // s: a body (alive or dead) you hold, or let go of (threw) this long ago, takes impacts (0: only while held)
QVR_TUNABLE(vr_ragimpact_crush_mass, "50") // kg of you counted behind your feet
QVR_TUNABLE(vr_decap_pop_projectile, "0.5") // any other projectile's kill at the head or a limb (a nail, a spike, AD's crossbow bolt, a lump of flesh; every game's: QC VR_Proj_Begin) pops it at this chance, as a laser bolt's (0: never; explosives pop by their blast, vr_limbs_blast_chance)
QVR_TUNABLE(vr_parry_muzzle_angle, "45") // degrees: a weapon whose far end (a gun's muzzle, a blade's tip) points within this of the attacker doesn't parry
QVR_TUNABLE(vr_forcegrab_miss_speed, "0.2") // share of its speed a missed object keeps
QVR_TUNABLE(vr_forcegrabbable_ammo_boxes, "1")
QVR_TUNABLE(vr_forcegrabbable_health_boxes, "1")
QVR_TUNABLE(vr_forcegrabbable_return_time_deathmatch, "4")
QVR_TUNABLE(vr_forcegrabbable_return_time_singleplayer, "0")
QVR_TUNABLE(vr_finger_blending_speed, "50")
QVR_TUNABLE(vr_spinreload_pitch_speed, "1100")
QVR_TUNABLE(vr_spinreload_x_angular_threshold, "6.5")
QVR_TUNABLE(vr_player_stepsize, "18.0")
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
QVR_TUNABLE(vr_maps_fetch, "1") // the index is fetched at start-up (0: never; a cached copy is still read). -nomapindex turns it off for one run
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
