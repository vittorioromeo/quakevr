// vr_cvars.cpp -- Quake VR cvar definitions and registration.

#include "vr_cvars.hpp"
#include "vr_bodycal.hpp"
#include "vr_bodyclone.hpp"
#include "vr_engine.hpp"
#include "vr_hands.hpp"
#include "vr_weapons.hpp"

#include "Zancle/Base/Strncmp.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/StringView.hpp"
#include "vr_zancle.hpp"

extern "C" cvar_t gl_texture_anisotropy; // gl_texmgr.c

namespace qvr
{

#define QVR_CVAR(name, def, flags) cvar_t name = {#name, def, flags};
#include "vr_cvars.inc"
#undef QVR_CVAR

cvar_t vr_backend = {"vr_backend", "openxr", CVAR_NONE}; // not saved: "mock" is for testing sessions

namespace
{

// Defaults changed after configs had saved the old ones (configs save every archived cvar, so a
// new default would never reach an existing player). A config still holding a setting's old default
// takes the new one, once: vr_cfg_version records the changes a config has seen. A setting the
// player changed is left alone.
struct DefaultChange
{
    int version;
    cvar_t* var;
    const char* before;
};

const DefaultChange defaultChanges[] = {
    // 123: the "Loading..." notice back on (1.0.1: its campaign-switch crash found and fixed, ROUND21.md, "The
    // Loading... crash, root cause"): a 1.0.0 config (122) at 1.0.0's 0 takes it.
    {123, &vr_loading_notice, "0"},
    // 124: the author's judo throw (1.0.1, his config of 2026-10-11: "I've tweaked the judo throw values a little bit
    // to make them feel even better"): launched further along the throw and less up, toppled slower, the feet swept
    // back slower, the grunt a little louder. A config at the old defaults takes them.
    {124, &vr_foegrab_throw_feet_speed, "60"}, // 30
    {124, &vr_foegrab_throw_grunt, "0.6"}, // 0.75
    {124, &vr_foegrab_throw_lift, "170"}, // 120
    {124, &vr_foegrab_throw_push, "190"}, // 240
    {124, &vr_foegrab_throw_topple, "300"}, // 200
    // 125: the knocked body's launch 1 -> 1.3 (the author, 2026-10-11, with 124's judo throw values: every shove and
    // throw launches further). A config at the old 1 takes it.
    {125, &vr_knockdown_push, "1"},
    // 126: the author's values after his 1.0.1 test (his config of 2026-10-11 15:29: "I tweaked some values, please make
    // them the new defaults"; ROUND22.md, "The author's 1.0.1 values are the defaults"): the flashlight's head zones
    // further forward, lower and wider, its belt clip a little forward, a slightly softer flick; Doom's CRT look
    // stronger; the comfort vignette a little lighter. A config at the old defaults takes them.
    {126, &vr_flashlight_head_zone_forward, "0.05"}, // 0.07 (vr_defaults.cfg)
    {126, &vr_flashlight_head_zone_up, "0.19"},      // 0.15 (vr_defaults.cfg)
    {126, &vr_flashlight_head_zone_radius, "0.105"}, // 0.125 (vr_defaults.cfg)
    {126, &vr_flashlight_forward, "0.02"},           // 0.025 (vr_defaults.cfg)
    {126, &vr_flashlight_flick_speed, "800"},        // 750
    {126, &vr_doom_crt, "1"},                        // 2
    {126, &vr_comfort_vignette_strength, "0.6"},     // 0.5
    // 127: the guns in the shoulder holsters (on the back, the highest pair) not drawn: "Sometimes the holstered model
    // interferes with the view" (the author, 2026-10-11; ROUND22.md, "Hide holstered weapons per pair"). A config with
    // them drawn (1) takes it.
    {127, &vr_holster_weapons_draw_shoulder, "1"},
    // 128: a shoved monster backpedals about as far as the old standing slide took it (the author, 2026-10-11: "they seem
    // to cover much less distance than before"; ROUND22.md, "Shoves: the backpedal").
    {128, &vr_ragdoll_active_stagger_travel, "0.2"},
    // 129: the author's bash, shove and parry values compiled in (ROUND22.md, "His parry and bash tweaks are the
    // defaults"): firmer pushes to bash (3 m/s) and shove (3.5), quieter bash sounds, a weapon's bash knocks down,
    // shorter parry pushes, a softer arm knock, parry stamina on (no random drop), a cheaper two-handed parry. Only the
    // two speeds migrate: the rest shipped at their new values already (vr_defaults.cfg), so a config at an old
    // compiled-in value there (bash sound 1, parry stamina 0, ...) was set by hand and keeps it.
    {129, &vr_bash_speed, "2"},
    {129, &vr_shove_speed, "2.4"},
    // 130: touching a monster no longer gives you away, only grabbing it does (the author, 2026-10-13: "just touching
    // an enemy doesn't alert them -- but grabbing them does"; ROUND22.md, "Stealth: a touch no longer gives you away").
    {130, &vr_stealth_contact, "1"},
    // 131: incandescent blades glow brighter (the author, 2026-10-13: "I have increased the default glow strength";
    // ROUND22.md, "Incandescent blades: his tweaks").
    {131, &vr_hotblade_glow, "1"},
    // 132: the neck snap's twist measured about the neck from the hands' places (a push-pull snaps it; ROUND22.md, "Neck
    // snap: only move at the head, a push-pull twists").
    // (Its twist speed, Velocity's, gone with it: ROUND22.md, "Neck snap: the Hand Line only".)
    {132, &vr_foegrab_necksnap_angle, "45"},
    // 133: the author's values of 2026-10-15 11:06 ("I have tweaked settings for the blood feedback, for the quad
    // effects, and some other damage and combat settings. Please make those the new defaults"; ROUND22.md, "His blood,
    // quad and combat tweaks are the defaults"). A config at the old defaults takes them.
    {133, &vr_blood_health_intensity, "0.25"},       // 0.3
    {133, &vr_blood_health_size, "0.65"},            // 0.75
    {133, &vr_blood_hit_bias, "0.75"},               // 0.9
    {133, &vr_blood_hit_duration, "0.45"},           // 0.4
    {133, &vr_blood_hit_intensity, "0.22"},          // 0.4
    {133, &vr_blood_hit_size, "0.65"},               // 0.6
    {133, &vr_quad_look, "1"},                       // 2
    {133, &vr_quad_glow, "1"},                       // 2
    {133, &vr_quad_glow_light, "1"},                 // 2
    {133, &vr_bash_damage, "10"},                    // 5
    {133, &vr_melee_phase_time, "0.35"},             // 0.2
    {133, &vr_foegrab_lift_time, "2"},               // 3.5
    {133, &vr_foegrab_necksnap_angle, "30"},         // 80
    {133, &vr_foegrab_necksnap_window, "0.3"},       // 0.2
    {133, &vr_foegrab_necksnap_neck, "3"},           // 1.5
    {133, &vr_foegrab_necksnap_strength, "1.2"},     // 1.25
    {133, &vr_foegrab_throw_feet_speed, "30"},       // 20
    {133, &vr_foegrab_throw_push, "240"},            // 220
    {133, &vr_foegrab_throw_speed, "2.5"},           // 3
    {133, &vr_foegrab_throw_spin, "0.55"},           // 0.5
    {133, &vr_foegrab_throw_topple, "200"},          // 180
    {133, &vr_foegrab_throw_twist, "140"},           // 130
    {133, &vr_ragdoll_grab_force, "3000"},           // 4000
    {133, &vr_ragdoll_hand_stick, "2"},              // 4
    {133, &vr_ragdoll_hand_turn, "60"},              // 70
    {133, &vr_reload_port_sshot_radius, "3"},        // 3.15
    {133, &vr_reload_reinsert_clearance, "4"},       // 1.5
    {133, &vr_decal_max, "768"},                     // 512
    {133, &vr_decal_bucket_max, "32"},               // 24
    {133, &vr_body_bracers, "1"},                    // 0
    {133, &vr_pbr_strength, "1"},                    // 2.5
    // (134 and 157: vr_foegrab_lift_classes's, retired for the monster groups' Lift.)
    // 135: the biosuit glow as he tuned it (ROUND22.md, "VR Settings submenus merged").
    {135, &vr_suit_glow, "1"},                       // 2
    {135, &vr_suit_glow_light, "1"},                 // 2
    // 136: the bullet-time throw is the stroke's tempo's alone (ROUND22.md, "Bullet-time throws: the stroke's tempo
    // only"): the older rules' settings are retired (retiredCvars), nothing here to change.
    // 137: the neck snap measured by the hand line only (Velocity and its Twist Speed gone), at the author's values ("I
    // took the neck snap settings ... Those are the ones I want", 2026-10-15; ROUND22.md, "Neck snap: the Hand Line
    // only"): 75 degrees within 0.15 s, the head zone as the headshots' (no margin, no neck below it).
    {137, &vr_foegrab_necksnap_angle, "80"},         // 75
    {137, &vr_foegrab_necksnap_window, "0.2"},       // 0.15
    {137, &vr_foegrab_necksnap_margin, "2"},         // 0
    {137, &vr_foegrab_necksnap_neck, "1.5"},         // 0
    // 138: gloves (vr_gloves, vr_glove_*; ROUND22.md, "Gloves; Player Appearance"): new settings only, nothing to change.
    // 139: the author's gore values (2026-10-16: "I've tweaked many values for the gore features. Please make those the
    // new defaults"; ROUND22.md, "Blood streams diverge; his gore values"): sturdier corpses, deeper dents, more and
    // wider but slower streams, fewer teeth, ragdoll impacts softer per hit but capped and crushing harder.
    {139, &vr_corpse_health_mult, "1"},              // 2
    {139, &vr_gore_dents_depth, "1"},                // 3
    {139, &vr_gore_dents_max, "8"},                  // 12
    {139, &vr_gore_dents_size, "1"},                 // 1.25
    {139, &vr_gore_streams_per_body, "3"},           // 4
    {139, &vr_gore_streams_speed, "70"},             // 50
    {139, &vr_gore_streams_width, "0.35"},           // 0.75
    {139, &vr_ragimpact_crush, "1"},                 // 1.5
    {139, &vr_ragimpact_crush_speed, "200"},         // 350
    {139, &vr_ragimpact_curve, "1"},                 // 1.5
    {139, &vr_ragimpact_curve_ref, "3"},             // 4
    {139, &vr_ragimpact_damage, "0.15"},             // 0.08
    {139, &vr_ragimpact_max, "0"},                   // 40
    {139, &vr_ragimpact_piece_hp, "0.4"},            // 0.65
    {139, &vr_ragimpact_stomp_heavy, "130"},         // 120
    {139, &vr_ragimpact_stomp_speed, "420"},         // 400
    {139, &vr_teeth_per_enemy, "10"},                // 8
    // 140: his knockdown values (2026-10-16: "make my five knockdown values the defaults").
    {140, &vr_knockdown_chance, "1"},               // 0.6
    {140, &vr_knockdown_damage, "2"},               // 3
    {140, &vr_knockdown_getup_speed, "1"},          // 0.8
    {140, &vr_knockdown_time_min, "2.5"},           // 1.75
    {140, &vr_knockdown_time_max, "4.5"},           // 3.5
    // 141: his ragdoll hand-follow value (2026-10-16: "I like if the hand follows the ragdoll more accurately").
    {141, &vr_ragdoll_hand_stick, "4"},             // 30
    // 142: one throw for props and weapons (2026-10-16, R2 review #13; ROUND22.md, "One throw for props and weapons"):
    // one two-handed window (vr_carry_two_hands_window was the props', 0.1; the weapons' a hard-coded 0.3) and one share
    // of the thrower's velocity (vr_carry_throw_inherit was the props', 1; the weapons' all of it). The old names set
    // the new settings (cvarAliases) as an old config is executed; at their old defaults they take the new ones.
    {142, &vr_throw_2h_window, "0.1"},              // 0.2
    {142, &vr_throw_inherit, "1"},                  // 0.9
    // 143: a jump from the same level crushes (2026-10-16: "jump from no extra height and still damage and pop the
    // limbs ... one or two jumps to kill a grunt by jumping on his head"); a hit keeps one down at least this after it.
    {143, &vr_knockdown_hit_time, "0.5"},           // 1
    {143, &vr_ragimpact_crush_speed, "350"},        // 180
    {143, &vr_ragimpact_crush_wear, "0.3"},         // 0.5
    // 144: the menu panel grip drag reaches farther (2026-10-17: "push the menu away too").
    {144, &vr_menu_grab_dist_max, "200"},           // 600
    // 145: the magnetic holsters' tether the default chain (2026-10-17: "the nicer one").
    {145, &vr_magholster_style, "0"},               // 1
    // 146: the leg sweep (2026-10-17): short weapons and props sweep too ("pretty much all of them should sweep"), and
    // his fall values ("direction and fall basically perfect (I tweaked some fall values)").
    {146, &vr_legsweep_min_length, "50"},           // 35
    {146, &vr_legsweep_min_slender, "4"},           // 2
    {146, &vr_legsweep_topple, "200"},              // 180
    {146, &vr_legsweep_feet, "0.5"},                // 0.4
    // 147: the firing range's kinds' limbs made at its load, not in play (ROUND22.md, "Mid-game model loads"): ten
    // ~8 ms frames for each kind a dispenser or a dummy first makes.
    {147, &vr_limbs_prebuild, "1"},                 // 2
    // 148: more of his values (2026-10-17: "I've also tweaked more settings (e.g. leg sweep values), make those the
    // defaults too"; ROUND22.md, "His settings, third pass"). A config at the old defaults takes them.
    {148, &vr_decal_carve_depth, "1"},       // 0.75
    {148, &vr_decal_carve_max, "256"},       // 128
    {148, &vr_decal_carve_rim, "1"},         // 1.25
    {148, &vr_dummy_gib, "1"},               // 0
    {148, &vr_enemy_melee_turn, "4"},        // 6
    {148, &vr_knockdown_keep_max, "6.5"},    // 7
    {148, &vr_legsweep_feet, "0.4"},         // 0.5
    {148, &vr_legsweep_speed, "1.5"},        // 3.25
    {148, &vr_legsweep_topple, "180"},       // 200
    {148, &vr_mirror_hide_hud_text, "0"},    // 1
    {148, &vr_parallax_plane, "0"},          // 1
    {148, &vr_relight_reload, "1"},          // 0
    // 149: his holster zones, positions, sizes and depths (2026-10-17: "I've tweaked all the holster positions and
    // sizes and depths. Make all the values the new defaults"), and the shoulder holsters' reach.
    {149, &vr_holster_zone_scale, "1"},                  // 0.95
    {149, &vr_holster_zone_hip_y, "0"},                  // -2.25
    {149, &vr_holster_zone_hip_z, "-2"},                 // -2.25
    {149, &vr_holster_zone_hip_depth, "0.6"},            // 0.15
    {149, &vr_holster_zone_upper_x, "0"},                // -3.75
    {149, &vr_holster_zone_upper_y, "0"},                // 3.25
    {149, &vr_holster_zone_upper_z, "-2"},               // -0.75
    {149, &vr_holster_zone_upper_height, "1.1"},         // 0.7
    {149, &vr_holster_zone_shoulder_x, "0"},             // -2.5
    {149, &vr_holster_zone_shoulder_y, "0"},             // 1
    {149, &vr_holster_zone_shoulder_z, "0"},             // -0.5
    {149, &vr_holster_zone_shoulder_depth, "0.9"},       // 0.5
    {149, &vr_holster_zone_shoulder_width, "0.9"},       // 0.5
    {149, &vr_holster_zone_shoulder_height, "0.9"},      // 0.5
    {149, &vr_holster_zone_ammo_x, "0"},                 // -1
    {149, &vr_holster_zone_ammo_depth, "1"},             // 0.15
    {149, &vr_holster_zone_ammo_width, "0.8"},           // 0.75
    {149, &vr_holster_zone_ammo_height, "1"},            // 0.95
    {149, &vr_shoulder_holster_thresh, "7.8"},           // 7
    // 150: his blood streams' and his own bleeding's values (2026-10-17: "I've tweaked some of the blood spurts and
    // waterfall settings. Make those the new defaults"; "increased the blood dripping rate and size for the player").
    {150, &vr_gore_streams_width, "0.75"},              // 1
    {150, &vr_gore_streams_speed_var, "0.3"},           // 0.5
    {150, &vr_gore_streams_wobble, "10"},               // 15
    {150, &vr_gore_streams_thickness_var, "0.55"},      // 0.85
    {150, &vr_gore_streams_thickness_rate, "1.1"},      // 1.3
    {150, &vr_gore_streams_spurt_rate, "40"},           // 70
    {150, &vr_gore_streams_spurt_cone, "30"},           // 15
    {150, &vr_gore_streams_spurt_speed, "0.8"},         // 1.35
    {150, &vr_gore_streams_spurt_life, "0.2"},          // 0.4
    {150, &vr_gore_streams_spurt_outlier_life, "0.7"},  // 1.15
    {150, &vr_gore_streams_spurt_size, "0.3"},          // 0.525
    {150, &vr_gore_streams_splash_size, "1"},           // 1.25
    {150, &vr_body_blood, "1"},                         // 1.25
    {150, &vr_body_blood_amount, "1"},                  // 1.25
    {150, &vr_body_blood_mark_size, "1"},               // 1.5
    // 151: tracked legs on by default (2026-10-17: his value).
    {151, &vr_bodytrack_legs, "0"},                     // 1
    // 152: retro particles at half resolution by default (GPU: blood bursts +1.8 ms; renumber at merge).
    {152, &vr_particle_retro_halfres, "0"},             // 1
    // 153: his ammo pouch zone and magnetic holster chain offsets (2026-10-17: "I've tweaked the ammo pouch zones again";
    // "the magnetic holster chain offsets: make those the new defaults"; renumber at merge).
    {153, &vr_holster_zone_ammo_x, "-1"},               // -0.5
    {153, &vr_holster_zone_ammo_depth, "0.15"},         // 0.2
    {153, &vr_holster_zone_ammo_height, "0.95"},        // 1.125
    {153, &vr_magholster_chain_x, "0"},                 // -2
    {153, &vr_magholster_chain_y, "0"},                 // -0.75
    // 154: his player head and loot beam values (2026-10-17: "make all of those the defaults").
    {154, &vr_body_head_side, "0"},                     // 1.75
    {154, &vr_body_head_sink, "4.5"},                   // 9
    {154, &vr_lootbeam_opacity, "0.5"},                 // 0.15
    {154, &vr_lootbeam_length, "1.2"},                  // 1.5
    {154, &vr_lootbeam_thickness, "0.06"},              // 0.03
    {154, &vr_lootbeam_far, "0"},                       // 12
    // 155: the cheaper bloom chain by default (2026-10-17).
    {155, &vr_bloom_fast, "0"},                         // 1
    // 156: the author's holster zones, thresholds, glove thickness and relief (his config of 2026-10-18).
    {156, &vr_glove_relief, "1"},
    {156, &vr_glove_thickness, "2"},
    {156, &vr_holster_zone_ammo_height, "1.125"},
    {156, &vr_holster_zone_ammo_x, "-0.5"},
    {156, &vr_holster_zone_ammo_z, "0"},
    {156, &vr_holster_zone_grenade_depth, "1"},
    {156, &vr_holster_zone_hip_depth, "0.15"},
    {156, &vr_holster_zone_hip_height, "1.1"},
    {156, &vr_holster_zone_hip_width, "0.55"},
    {156, &vr_holster_zone_hip_x, "0"},
    {156, &vr_holster_zone_hip_y, "-2.25"},
    {156, &vr_holster_zone_hip_z, "-2.25"},
    {156, &vr_holster_zone_item_hip, "1.5"},
    {156, &vr_holster_zone_item_shoulder, "1.5"},
    {156, &vr_holster_zone_item_upper, "1.5"},
    {156, &vr_holster_zone_shoulder_height, "0.5"},
    {156, &vr_holster_zone_shoulder_x, "-2.5"},
    {156, &vr_holster_zone_shoulder_y, "1"},
    {156, &vr_holster_zone_shoulder_z, "-0.5"},
    {156, &vr_holster_zone_upper_depth, "0.6"},
    {156, &vr_holster_zone_upper_height, "0.7"},
    {156, &vr_holster_zone_upper_width, "0.6"},
    {156, &vr_holster_zone_upper_x, "-3.75"},
    {156, &vr_holster_zone_upper_y, "3.25"},
    {156, &vr_holster_zone_upper_z, "-0.75"},
    {156, &vr_shoulder_holster_thresh, "7"},
    {156, &vr_upper_holster_thresh, "6.5"},
    // 158: his retuned grenade pouch reach and flamethrower lights (the holster zones and glove values already match).
    {158, &vr_grenade_pouch_thresh, "7"},
    {158, &vr_flame_lights, "3"},
    {158, &vr_flame_light_scale, "1"},
    // 159: the Widowmaker's handful his 2 (was 4), its shell port half its size (his notes 2026-10-19).
    {159, &vr_ad_widowmaker_handful, "4"},
    {159, &vr_reload_port_widow_radius, "2.5"},
    // 160: his swing sounds and wet sheen (his config of 2026-10-19).
    {160, &vr_melee_swish_min, "0.5"},                  // 0.3
    {160, &vr_melee_swish_sub, "0.45"},                 // 0.35
    {160, &vr_melee_swish_full, "2.2"},                 // 1.75
    {160, &vr_melee_swish_gain, "1.6"},                 // 2
    {160, &vr_wounds_wet_coat, "1"},                    // 2
    // 160 too: the player's chainsaw a fifth stronger (his request, 2026-10-19).
    {160, &vr_chainsaw_damage, "100"},                  // 120
    {160, &vr_dmg_chainsaw_swing, "22"},                // 26
    // 161: his flame wobble (his config of 2026-10-19); your AD crossbow's bolts 1.5 times the knights' (his notes).
    {161, &vr_flame_wobble_strength, "1"},              // 2.5
    {161, &vr_adgun_crossbow_speed, "700"},             // 1050
    // 162: his Widowmaker and AD crossbow numbers (his config of 2026-10-19).
    {162, &vr_ad_widowmaker_refire, "0.25"},         // 0.35
    {162, &vr_ad_widowmaker_shot_pellets, "7"},      // 8
    {162, &vr_dmg_ad_widowmaker, "4"},               // 5
    {162, &vr_adgun_crossbow_damage, "15"},          // 30
    // 163: a projectile's kill pops a head or limb less often (his notes of 2026-10-19).
    {163, &vr_decap_pop_projectile, "0.85"}, // 0.5
    // 164: monster groups: bosses can't be shoved, machines aren't knocked down by shoves, a skull wizard has no legs.
    {164, &vr_mgrp_boss_push, "0"},           // 1
    {164, &vr_knockdown_chance_machine, "0"}, // 0.3
    {164, &vr_mgrp_skullwiz_sweep, "0"},      // 1
    {164, &vr_mgrp_skullwiz_leggrab, "0"},    // 1
    // 165: his wet time, the Widowmaker's motor (quieter) and load port (his config of 2026-10-19).
    {165, &vr_wounds_wet_time, "28"},           // 15
    {165, &vr_reload_port_widow_radius, "1.25"}, // 1.4
    {165, &vr_ad_widowmaker_motor, "0.6"},      // 0.4
    // 166: his Ghost Hands fade (the ghost shows once the drawn hand is 6 cm off, over 7 more).
    // (Its rows named the new values until 171, so they changed nothing: 171 below does it again.)
    {166, &vr_ghost_hands_fade_from, "2"},  // 6
    {166, &vr_ghost_hands_fade_range, "4"}, // 7
    // 167: the carving quality 16 -> 8 steps (his config of 2026-10-19); the Widowmaker spins up 0.2 -> 0.75 s before its first shot.
    {167, &vr_decal_carve_steps, "16"},
    {167, &vr_ad_widowmaker_spinup, "0.2"},
    // 168: his shallower dents (his config of 2026-10-20).
    {168, &vr_gore_dents_depth, "3"},
    // 169: his Thrown Bodies tuning (his config of 2026-10-20); the tackle redone as the shoulder charge, off for now
    // (the old guard/shove tackle fired unintentionally), its bounce always (feedback).
    {169, &vr_kdtrig_thrown_speed, "200"}, // 160
    {169, &vr_kdtrig_thrown_least, "0.4"}, // 0.05
    {169, &vr_kdtrig_thrown_push, "1"},    // 2
    {169, &vr_kdtrig_tackle_on, "1"},      // 0
    {169, &vr_kdtrig_tackle_bump, "60"},   // 100
    // 170: the pouch gives taped shell pairs by default (1.1 balance: manual reloading made ogres tanky; ROUND22.md,
    // "1.1 pacing: shell pairs, big monsters' health"). A config at the old default (off) takes it.
    {170, &vr_reload_shell_pairs, "0"},
    // 171: 166's Ghost Hands fade again (its rows had the new values where the old belong: a config of 166-170 still at
    // the old 2 and 4 takes 6 and 7); the retro lighting's model steps 15 -> 16 (1.0.1's vr_defaults.cfg set 15, and the
    // line went without a row when the default moved into vr_cvars.inc, 57ee3adc4: a 1.0.1 config kept 15).
    {171, &vr_ghost_hands_fade_from, "2"},  // 6
    {171, &vr_ghost_hands_fade_range, "4"}, // 7
    {171, &vr_retrolight_model_steps, "15"}, // 16
    // 172: Box3D on the pool from 300 awake bodies (ROUND22.md, "play_gore_fight's cost"): at 150-190 with ragdolls the
    // main thread waited on the workers longer than it took to step alone, and spiked.
    {172, &vr_box3d_threads_bodies, "150"}, // 300
    // 173: the author's values (2026-10-20 notes): more marks kept in one spot (decals vanished mid-fight where the gore
    // piled up), earned bullet time taking more damage to fill (Normal's; the other skills scale it) and less while it runs.
    {173, &vr_decal_bucket_max, "24"},          // 128
    {173, &vr_bullettime_earn_damage, "180"},   // 350
    {173, &vr_bullettime_earn_active, "0.25"},  // 0.2
    // 174: a two-hand shove goes 1.25 times as far (the author's 2026-10-21 notes; its distance goes by the push's square).
    {174, &vr_shove_push_twohand, "1"}, // 1.12
};
constexpr int configVersion = 174;
// 1.0.0's version: an older config is reset (resetOldConfig), not migrated; its changes' rows were dropped.
constexpr int oldestMigrated = 122;

// Two settings' values the same (as numbers when both are).
[[nodiscard]] bool sameValue(const char* a, const char* b)
{
    char* endA;
    char* endB;
    const double x = strtod(a, &endA);
    const double y = strtod(b, &endB);
    if(endA != a && !*endA && endB != b && !*endB)
    {
        return fabs(x - y) < 1e-6;
    }
    return !strcmp(a, b);
}

// Two copies of the game on one config (ROUND21.md, "Debug menu; quad sound; grenade catch default; no empty-hand
// deflection"): each writes every archived setting when it quits, as it has them, so the copy that quits last undid the
// other's changes. The author's Catch Grenades went back to "Ogres'" that way: a copy started from TrenchBroom (vrwip,
// 16:51) stayed open through his session in the headset (17:06), where he chose "Ogres' and yours", and quit after it.
// Now a copy writing the config merges it, setting by setting: one changed in the file since this copy read it (by
// another copy) and left alone here takes the file's value; one changed here is written as it is here (both changed:
// this copy's, the last). Tracked from the load of the game folder's config (vr_migrate_config), and after each write.
struct ConfigTrack
{
    bool on = false;
    za::String gamedir;
    ankerl::unordered_dense::map<za::String, za::String> file; // the archived settings in the config, as read or written
    ankerl::unordered_dense::map<za::String, za::String> here; // this copy's values then
};
ConfigTrack configTrack;

// The archived settings a config file sets (`name "value"` lines, as Cvar_WriteVariables writes them).
[[nodiscard]] ankerl::unordered_dense::map<za::String, za::String> readConfigSettings(const char* path)
{
    ankerl::unordered_dense::map<za::String, za::String> settings;
    FILE* f = Sys_fopen(path, "rb"); // (UTF-8 path: a non-ASCII game folder)
    if(!f)
    {
        return settings;
    }
    za::String line;
    for(int c = 0; c != EOF;)
    {
        c = fgetc(f);
        if(c != '\n' && c != '\r' && c != EOF)
        {
            line += static_cast<char>(c);
            continue;
        }
        const size_t space = line.find(' ');
        const size_t open = space == za::StringView::nPos ? space : line.find('"', space);
        const size_t close = line.rfind('"');
        if(open != za::StringView::nPos && close > open && line.findFirstNotOf(' ', space) == open)
        {
            const za::String name{line.substrByPosLen(0, space)};
            const cvar_t* var = Cvar_FindVar(name.cStr());
            if(var && (var->flags & CVAR_ARCHIVE))
            {
                settings[za::String{var->name}] = line.substrByPosLen(open + 1, close - open - 1); // (an old name: the new)
            }
        }
        line.clear();
    }
    fclose(f);
    return settings;
}

[[nodiscard]] ankerl::unordered_dense::map<za::String, za::String> archivedSettings()
{
    ankerl::unordered_dense::map<za::String, za::String> settings;
    for(const cvar_t* var = Cvar_FindVarAfter("", CVAR_ARCHIVE); var; var = Cvar_FindVarAfter(var->name, CVAR_ARCHIVE))
    {
        settings[var->name] = var->string;
    }
    return settings;
}

void trackConfig(const char* path)
{
    configTrack.on = true;
    configTrack.gamedir = com_gamedir;
    configTrack.file = readConfigSettings(path);
    configTrack.here = archivedSettings();
}

void migrateConfig();
[[nodiscard]] bool personal(const cvar_t* var);

// A config older than 1.0.0 (vr_cfg_version < 122; the author's decision of 2026-10-16: "ignore compatibility with any
// pre-1.0.0 configs"): its Quake VR settings go back to the shipped defaults instead of being migrated, except the
// per-player ones (personal: the body measured, the runtime, the tips seen...). The weapons' and the held objects'
// settings are vr_ settings too: their versions become today's. The engine's own settings (r_*, gl_*...) are kept.
void resetOldConfig(int from)
{
    for(cvar_t* var = Cvar_FindVarAfter("", CVAR_ARCHIVE); var; var = Cvar_FindVarAfter(var->name, CVAR_ARCHIVE))
    {
        if(!ZA_STRNCMP(var->name, "vr_", 3) && !personal(var))
        {
            Cvar_SetQuick(var, var->default_string);
        }
    }
    Cvar_SetValueQuick(&vr_tutorial_started, 1.f); // it has played: the hub, as before, not the tutorial
    Cvar_SetValueQuick(&vr_cfg_version, static_cast<float>(configVersion));
    Cvar_SetQuick(&vr_props_version, vr_props_version.default_string);
    weapons::markCurrent();
    Con_Printf("Quake VR: this config is older than 1.0.0 (version %d): its VR settings are reset to the defaults\n", from);
}

// Right after the saved config is executed (Cmd_Exec_f queues it).
void migrateConfig_f()
{
    migrateConfig();
    trackConfig(va("%s/%s", com_gamedir, CONFIG_NAME));
}

// "vr_migrate_config new": there was no saved config (a first start): the settings are this version's, nothing to
// change.
void migrateConfig()
{
    bodycal::migrate(); // round 21's arm settings, whatever the version (they are moved, not changed in place)
    if(Cmd_Argc() > 1 && !strcmp(Cmd_Argv(1), "new"))
    {
        Cvar_SetValueQuick(&vr_cfg_version, static_cast<float>(configVersion));
        Cvar_SetValueQuick(&vr_setup_pending, 1.f); // a first start: VR Calibration once the headset is on (vr_setup.cpp)
        weapons::markCurrent(); // and the weapons' settings are this version's
        return;
    }
    const int from = static_cast<int>(vr_cfg_version.value);
    if(from >= configVersion)
    {
        return;
    }
    if(from < oldestMigrated)
    {
        resetOldConfig(from);
        return;
    }
    for(const DefaultChange& c : defaultChanges)
    {
        if(c.version > from && !strcmp(c.var->string, c.before))
        {
            Con_DPrintf("VR: %s: new default %s (was %s)\n", c.var->name, c.var->default_string, c.before);
            Cvar_SetQuick(c.var, c.var->default_string);
        }
    }
    Cvar_SetValueQuick(&vr_cfg_version, static_cast<float>(configVersion));
}

// Shipped defaults (quakevr/vr_defaults.cfg, executed by default.cfg): the tuned values over the
// ones compiled in. "vr_default name value" sets a cvar and makes the value its default, so resets
// and presets return to it; the saved config still wins, as it's executed after.
// The engine cvars' own defaults, before vr_default replaced them.
ankerl::unordered_dense::map<za::String, za::String> engineDefaults;

void default_f()
{
    if(Cmd_Argc() != 3)
    {
        Con_Printf("vr_default <cvar> <value>: set a Quake VR setting and make the value its default\n");
        return;
    }
    cvar_t* var = Cvar_FindVar(Cmd_Argv(1));
    if(!var)
    {
        Con_Printf("vr_default: no cvar \"%s\"\n", Cmd_Argv(1));
        return;
    }
    if(ZA_STRNCMP(var->name, "vr_", 3) != 0)
    {
        // The engine's own default, for vr_savedefaults (vr_ cvars have compiledDefaults).
        engineDefaults.try_emplace(var->name, var->default_string ? var->default_string : "");
    }
    Cvar_Set(var->name, Cmd_Argv(2));
    Z_Free(const_cast<char*>(var->default_string));
    var->default_string = Z_Strdup(var->string);
}

// The compiled-in defaults, to tell which settings were tuned.
struct CompiledDefault
{
    const cvar_t* var;
    const char* value;
};

const CompiledDefault compiledDefaults[] = {
#define QVR_CVAR(name, def, flags) {&name, def},
#include "vr_cvars.inc"
#undef QVR_CVAR
};

// Per-player or bookkeeping settings, never shipped.
[[nodiscard]] bool personal(const cvar_t* var)
{
    return var == &vr_cfg_version || var == &vr_tutorial_started || var == &vr_bindings_version || var == &vr_wofs_version || var == &vr_height_calibration
        || var == &vr_seated_height // (one's chair)
        || var == &vr_props_version || var == &vr_tips_seen || var == &vr_menu_positions
        || var == &vr_xr_runtime || var == &vr_xr_runtime_json || var == &vr_note_device || var == &vr_dominant_eye
        || !ZA_STRNCMP(var->name, "vr_motion_", 10) // the motion recorder's (a tool's settings)
        || !ZA_STRNCMP(var->name, "vr_bodycal_", 11) || !ZA_STRNCMP(var->name, "vr_body_tweak_", 14) // one's body
        // and one's arms (Body > Arms, the player's to tweak: the author's decision, 2026-09-28; pauldrons ship)
        || var == &vr_body_arm_length || var == &vr_body_arm_stretch || var == &vr_body_shoulder_reach
        || var == &vr_body_forearm_twist || var == &vr_body_wrist_limits || var == &vr_body_elbow_lift
        || var == &vr_body_elbow_spread
        || var == &vr_body_elbow_out
        || var == &vr_body_elbow_back || var == &vr_body_elbow_hand || var == &vr_body_elbow_tuck
        || var == &vr_body_elbow_tuck_back || var == &vr_body_elbow_tuck_near || var == &vr_body_elbow_tuck_far;
}

// "vr_savedefaults": writes the archived Quake VR settings that differ from the compiled-in
// defaults to vr_defaults.cfg in the game folder (per-weapon offsets and personal settings left out).
void saveDefaults_f()
{
    const char* path = va("%s/vr_defaults.cfg", com_gamedir);
    FILE* f = Sys_fopen(path, "wb");
    if(!f)
    {
        Con_Printf("vr_savedefaults: can't write %s\n", path);
        return;
    }
    fprintf(f, "// Quake VR's shipped settings: the tuned values over the compiled-in defaults.\n"
               "// Executed by default.cfg (so the saved config still wins); written by \"vr_savedefaults\".\n\n");
    int count = 0;
    for(const CompiledDefault& d : compiledDefaults)
    {
        if((d.var->flags & CVAR_ARCHIVE) && !personal(d.var) && !sameValue(d.var->string, d.value))
        {
            fprintf(f, "vr_default %s \"%s\"\n", d.var->name, d.var->string);
            ++count;
        }
    }
    // The engine's graphics settings (r_*, gl_*) changed from its own defaults too.
    fprintf(f, "\n// The engine's graphics settings.\n");
    for(const cvar_t* var = Cvar_FindVarAfter("", CVAR_ARCHIVE); var; var = Cvar_FindVarAfter(var->name, CVAR_ARCHIVE))
    {
        if(ZA_STRNCMP(var->name, "r_", 2) != 0 && ZA_STRNCMP(var->name, "gl_", 3) != 0)
        {
            continue;
        }
        const auto it = engineDefaults.find(var->name);
        const char* def = it != engineDefaults.end() ? it->second.cStr() : var->default_string;
        if(def && !sameValue(var->string, def))
        {
            fprintf(f, "vr_default %s \"%s\"\n", var->name, var->string);
            ++count;
        }
    }
    fclose(f);
    Con_Printf("Wrote %d settings to %s\n", count, path);
}

} // namespace

void registerCvars()
{
#define QVR_CVAR(name, def, flags) Cvar_RegisterVariable(&name);
#include "vr_cvars.inc"
#undef QVR_CVAR

    Cvar_RegisterVariable(&vr_backend);
    Cmd_AddCommand("vr_migrate_config", migrateConfig_f);
    Cmd_AddCommand("vr_menu_body_clone_info", bodyclone::info_f);
    Cmd_AddCommand("vr_default", default_f);
    Cmd_AddCommand("vr_savedefaults", saveDefaults_f);
}

} // namespace qvr

// Host_WriteConfigurationToFile, before the game folder's config is written over: the settings another copy of the
// game changed in it since this copy read or wrote it, and this copy left alone, are taken (configTrack).
// Settings removed (docs/vr-port/CVAR_AUDIT.md): a config, a shipped .cfg or a take that still sets one is not an
// "Unknown command" (cmd.c asks here); the line is dropped quietly and the next config write leaves it out.
constexpr const char* retiredCvars[] = {
    // 2026-10-19: the class lists replaced by the monster groups' Lift, Neck Snap, Leg Sweep, Leg Grabs, Manhandle and
    // Two-Hand Throw (vr_mgrp_<group>_*; ROUND22.md, "Monster groups").
    "vr_foegrab_lift_classes",
    "vr_foegrab_throw_always",
    "vr_foegrab_throw_when_hurt",
    "vr_throw_lookahead", // 2026-10-06: unused since the throw's window ends at the release (ROUND21.md, "Throws at any frame rate")
    "vr_throw_slowmo_flick_spin", // 2026-10-08: replaced by vr_throw_slowmo_flick_arm (ROUND21.md, "Wrist flicks in bullet time: the arm tells the tempo"), itself retired below
    "vr_foegrab_throw_topple_hold", // 2026-10-09: vr_foegrab_throw_feet_speed (ROUND21.md, "Holding enemies")
    // 2026-10-09: vr_knockdown_shove_travel, _topple_angle, _topple_time, _feet_lag, _max_spin (ROUND21.md, "A shove's
    // knockdown: travel and a quarter turn").
    "vr_knockdown_shove_topple",
    "vr_knockdown_shove_topple_push",
    "vr_knockdown_shove_feet_speed",
    "vr_knockdown_shove_feet_hold",
    "vr_hub_map", // 2026-10-09: the hub is vrstart, the only one (the old hub, vrstart_old, removed)
    // 2026-10-15: the bullet-time throw is the stroke's tempo's alone (ROUND22.md, "Bullet-time throws: the stroke's
    // tempo only"); its switch and the older rules' settings.
    "vr_throw_slowmo_stroke",
    "vr_throw_slowmo_real_time",
    "vr_throw_slowmo_tempo",
    "vr_throw_slowmo_aim",
    "vr_throw_slowmo_flick",
    "vr_throw_slowmo_flick_arm",
    "vr_throw_slowmo_short_travel",
    "vr_throw_slowmo_long_travel",
    // 2026-10-16: the A/B switches settled at their defaults, the other path deleted (ROUND22.md, "A/B switches
    // removed").
    "vr_box3d_caller_solves",
    "vr_carry_grab_drawn",
    "vr_weapon_grab_box",
    "vr_button_weapon",
    "vr_parry_interrupt",
    "vr_weapon_throw_mode",
    "vr_throw_mass_model",
    "vr_weight_throw_mass", // (its old path's setting: props over it thrown slower)
    "vr_prop_think_native",
    "vr_hull_method",
    "vr_decap_pop_chance", // (a switch: off, every headshot kill popped)
    // 2026-10-17: perf attempts that did not pay off, removed (ROUND22.md, "Removed perf attempts"); vr_xr_overlap was
    // archived, the rest only set by hand
    "vr_xr_overlap",
    "vr_debug_xr_overlap_work_ms",
    "vr_mock_end_gpu_wait",
    "vr_parallax_conservative_depth",
    "vr_model_ambient_prefetch",
    "vr_water_pick_once",
    "vr_server_thread_check",
    // 2026-10-19: the flinch removed (ROUND22.md, "The flinch removed; ragdoll triggers"; FLINCH_RETROSPECTIVE.md):
    // hits play the pains again, active ragdolls come from the triggers (vr_kdtrig: Combat > Ragdoll Triggers).
    "vr_foegrab_lift_shove", // 2026-10-19: replaced by vr_foegrab_free_push (metres)
    "vr_flinch",
    "vr_flinch_classes",
    "vr_flinch_min",
    "vr_flinch_cooldown",
    "vr_flinch_attack_guard",
    "vr_flinch_pain",
    "vr_flinch_max",
    "vr_flinch_range",
    "vr_flinch_topple",
    "vr_flinch_uneven",
    "vr_flinch_local",
    "vr_flinch_shotgun",
    "vr_flinch_ssg",
    "vr_flinch_shot_near",
    "vr_flinch_shot_far",
    "vr_flinch_shot_far_share",
    "vr_flinch_shot_other",
    "vr_flinch_nail",
    "vr_flinch_melee",
    "vr_flinch_throw",
    "vr_flinch_blast",
    "vr_flinch_most",
    "vr_flinch_recover_min",
    "vr_flinch_recover_hold",
    "vr_flinch_recover_speed",
    "vr_flinch_recover_tilt",
    "vr_flinch_recover_slope",
    "vr_flinch_legs_push",
    "vr_flinch_legs_upright",
    "vr_flinch_foot",
    "vr_flinch_blend",
    "vr_flinch_debug",
    "vr_flinch_test",
    "vr_flinch_test_pellets",
    "vr_flinch_test_health",
    "vr_flinch_test_x",
    "vr_flinch_test_y",
    "vr_flinch_test_z",
    "vr_flinch_test_yaw",
    "vr_flinch_test_dist",
    "vr_flinch_test_class",
    // The eliminator's plasma gun's own tweaks (1.1 beta 1 and 2; it fires as the centurion's since 944765aa1).
    "vr_adgun_eliminator_ammo",
    "vr_adgun_eliminator_damage",
    "vr_adgun_eliminator_refire",
    "vr_adgun_eliminator_speed",
    "vr_adgun_eliminator_splash",
    "vr_toolgun_menu_height", // 1.0.1's: the toolgun's own panel replaced its VR Settings pages (7baa39660)
};

// Settings renamed: the old name still reads and sets the new one (Cvar_FindVar asks here when a name is not found):
// an old config's lines set the new settings as it is executed (the next config write has the new names), as do
// binds, scripts and benchmark setups. Never listed, completed or written. The teleporters were "slipgates" until
// 2026-10-08 (in Quake the slipgate is id's machine; ROUND21.md, "Teleporters, not slipgates").
struct CvarAlias
{
    const char* old;
    const char* now;
};
constexpr CvarAlias cvarAliases[] = {
    {"vr_slipgates", "vr_teleporters"},
    {"vr_slipgate_pair_exits", "vr_teleporter_pair_exits"},
    {"vr_slipgate_self_head", "vr_teleporter_self_head"},
    {"vr_slipgate_surface_size", "vr_teleporter_surface_size"},
    {"vr_slipgate_surface_opacity", "vr_teleporter_surface_opacity"},
    {"vr_slipgate_surface_fade", "vr_teleporter_surface_fade"},
    // 2026-10-16: one throw for props and weapons (config 142; ROUND22.md, "One throw for props and weapons").
    {"vr_carry_two_hands_window", "vr_throw_2h_window"},
    {"vr_carry_throw_inherit", "vr_throw_inherit"},
};

extern "C" const char* VR_CvarAlias(const char* name)
{
    for(const CvarAlias& a : cvarAliases)
    {
        if(!strcmp(name, a.old))
        {
            return a.now;
        }
    }
    return nullptr;
}

extern "C" int VR_RetiredCvar(const char* name)
{
    for(const char* retired : retiredCvars)
    {
        if(!q_strcasecmp(name, retired))
        {
            return 1;
        }
    }
    return 0;
}

extern "C" void VR_ConfigMergeOthers(const char* path)
{
    using namespace qvr;
    bodycal::endPreview(); // a previewed calibration is not what is set (the page shows it again the next frame)
    if(!configTrack.on || configTrack.gamedir != com_gamedir)
    {
        return;
    }
    const auto settings = readConfigSettings(path);
    for(const auto* setting : qza::sortedByKey(settings)) // (in the names' order, as a std::map had them)
    {
        const auto& [name, value] = *setting;
        const auto was = configTrack.file.find(name);
        if(was != configTrack.file.end() && sameValue(was->second.cStr(), value.cStr()))
        {
            continue; // not changed in the file
        }
        cvar_t* var = Cvar_FindVar(name.cStr());
        const auto here = configTrack.here.find(name);
        if(!var || (var->flags & (CVAR_ROM | CVAR_LOCKED)) || here == configTrack.here.end()
           || !sameValue(here->second.cStr(), var->string) || sameValue(var->string, value.cStr()))
        {
            continue; // changed here too (this copy's wins), or already the same
        }
        Con_Printf("%s \"%s\": from another copy of the game (this one left it at \"%s\")\n", name.cStr(), value.cStr(),
            var->string);
        Cvar_SetQuick(var, value.cStr());
    }
}

// After it is written: what the file holds now is this copy's.
extern "C" void VR_ConfigWritten(const char* path)
{
    using namespace qvr;
    if(configTrack.on && configTrack.gamedir == com_gamedir)
    {
        trackConfig(path);
    }
}

namespace qvr
{

void saveConfigNow()
{
    // Not before the game folder's config is read (it would be written over with the defaults).
    if(!configTrack.on || configTrack.gamedir != com_gamedir)
    {
        return;
    }
    int changed = 0;
    for(const auto& [name, value] : archivedSettings())
    {
        const auto was = configTrack.here.find(name);
        changed += was == configTrack.here.end() || !sameValue(was->second.cStr(), value.cStr()) ? 1 : 0;
    }
    if(changed > 0)
    {
        Con_DPrintf("config: %d setting%s changed, saved\n", changed, changed == 1 ? "" : "s");
        Host_WriteConfiguration();
    }
}

namespace
{
bool configMenuWasOpen = false; // configFrame: the menu or the console was open last frame
} // namespace

void configFrame()
{
    bool& wasOpen = configMenuWasOpen;
    const bool open = key_dest == key_menu || key_dest == key_console;
    if(wasOpen && !open)
    {
        saveConfigNow();
    }
    wasOpen = open;
}

} // namespace qvr
