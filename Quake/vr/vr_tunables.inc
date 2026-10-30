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
QVR_TUNABLE(vr_gun_wall_max, "40") // with vr_gun_wall_slide: cm, the most a held weapon and its hand are held out of the level (pushed further, they go in by the rest; the weapon keeps resting where it was held)
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
QVR_TUNABLE(vr_carry_slam_height, "10") // cm: the hands' middle at least this far over the eyes is over the head
QVR_TUNABLE(vr_carry_slam_speed, "3") // m/s: the hands swung down at least this fast (mostly downwards) break it on what they meet
QVR_TUNABLE(vr_carry_slam_window, "1") // s after it was last over the head a swing down still counts
QVR_TUNABLE(vr_carry_slam_damage, "2") // what it is swung onto takes this times a punch with it in hand (times its speed over the least, at most twice)
QVR_TUNABLE(vr_crate_slam_throw_damage, "0.55") // let go of from over the head, its first hard landing takes this share of its full health (0.55: two such throws break it)
QVR_TUNABLE(vr_crate_slam_throw_speed, "3") // m/s: ... a landing at least this fast
QVR_TUNABLE(vr_crate_pieces, "12") // pieces a small crate breaks into (a large one more, by its size)
QVR_TUNABLE(vr_crate_piece_max, "48") // most pieces lying about at once (the oldest turn to dust)
QVR_TUNABLE(vr_crate_piece_time, "60") // s a piece lies about before it fades away (0: for ever)
QVR_TUNABLE(vr_crate_ammo, "0.35") // the chance a crate holds a small box of ammo for a weapon you have (or the shotgun)
QVR_TUNABLE(vr_crate_health_box, "0.2") // the chance a crate holds a small health box
QVR_TUNABLE(vr_crate_item_pop, "3.5") // how hard what a crate held pops out when it breaks (m/s: up, and a little outwards)
QVR_TUNABLE(vr_crate_crowbar, "0.1") // the chance a crate placed about the map (the top of a stack) has a crowbar lying on it (next map)
QVR_TUNABLE(vr_crate_crowbar_max, "4") // most crowbars on crates in a map

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
QVR_TUNABLE(vr_corpse_dmg_props, "1") // times the damage corpses and ragdolls take from thrown or flung boxes, gibs, heads and weapons, deflected shots, a prop in the fist
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
QVR_TUNABLE(vr_ragdoll_hand_rest, "6") // cm: the hand drawn on a held limb slid at most this along the way to its mesh, for its palm to rest on it (not off it, not sunk in it); 0: as the hold has it
QVR_TUNABLE(vr_ragdoll_throw, "1") // times the hand's throw a limb let go of keeps (0: only what the pull gave it)
QVR_TUNABLE(vr_ragdoll_blood, "1") // a ragdoll's limbs flung fast leave blood trails and drops (as gibs do: vr_gib_blood_trail)
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

// Carrying and Throwing > Wall Torches
QVR_TUNABLE(vr_walltorch_reach, "15") // cm from a torch's stick (its axis, butt to flame) to the hand's point or its fist's middle within which a hand's grip takes it off its wall though the fist isn't on it (0: only the fist on it)
QVR_TUNABLE(vr_walltorch_hits, "5") // blows a torch lands before its fire starts dying, even held (0: blows never use it up)
QVR_TUNABLE(vr_walltorch_damage, "12") // a torch's blow: its base damage, times the blow's strength (a gun's is 12, the axe's 20)
QVR_TUNABLE(vr_walltorch_flame, "1") // a taken torch's flame's size (1: the wall torch's)

// Weapons > Aiming
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
QVR_TUNABLE(vr_revolver_eject_tilt, "50") // out, the muzzle tilted up this many degrees drops its shells, spent and live (0: never)
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
