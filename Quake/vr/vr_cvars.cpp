// vr_cvars.cpp -- Quake VR cvar definitions and registration.

#include "vr_cvars.hpp"
#include "vr_body.hpp"
#include "vr_bodycal.hpp"
#include "vr_engine.hpp"
#include "vr_hands.hpp"
#include "vr_props.hpp"
#include "vr_retro.hpp"
#include "vr_weapons.hpp"

#include "Zancle/Base/Strncmp.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/StringView.hpp"
#include "vr_zancle.hpp"

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
    {1, &vr_swim_stick_speed, "0.1"},
    {2, &vr_dlight_uncapped, "0"},
    {3, &vr_bloom, "0.8"},
    {3, &vr_bloom_threshold, "0.6"},
    {3, &vr_headbutt_speed, "1.5"},
    {4, &vr_carry_throw_damage, "25"},
    {5, &vr_flash_scale, "1.8"},           // sizes over DarkPlaces' lights now (vr_dlight_falloff)
    {5, &vr_explosion_light_scale, "1.5"},
    {6, &vr_flashlight_shadows, "0"},
    {6, &vr_flashlight_beam, "0"},         // a soft cone of light now, not a line over everything
    {7, &vr_melee_speed, "3"},             // the wrist's speed now (a blow must also travel vr_melee_distance)
    {9, &vr_parallax_models, "0.75"},      // off: parallax on 8-bit skins bends their texels (the bumps give models relief now)
    {10, &vr_parry_angle, "50"},          // degrees off level now (was off square to the blow, by the hand's forward)
    {11, &vr_bash_speed, "1.6"},          // a gentler push bashes (round 18: the guard is the parry's now)
    {12, &vr_sight_hue, "30"},            // their own orange: they follow the player's hue now (vr_player_hue)
    {13, &vr_shove_speed, "1.8"},         // the author's shoves go 3.2-4.8 m/s, his hands waved at the dummy 2.2 (round 21)
    {14, &vr_counter_glow, "1"},          // off: the author would rather play without it (round 21, "Stamina on the gadget; the glow")
    {34, &vr_spectator_scale, "1"},       // 0.75: the spectator camera's cost (ROUND21.md, "Performance fixes (review, 2026-09-28)")
    {41, &vr_grenade_catch, "1"},         // 2: your own grenades too, the author's choice (ROUND21.md, "Debug menu; quad sound; grenade catch default; no empty-hand deflection")
    // 45: the author's liquid transparency (NOTES.md start_2026-09-29_18-40-33; ROUND21.md, "Defaults: the author's
    // liquids, bricks, torch and holsters"): water 0.4, lava, slime and teleporters 0.9 (vr_defaults.cfg).
    {45, &r_wateralpha, "0.6"},
    {45, &r_lavaalpha, "1"},
    {45, &r_slimealpha, "0"},
    {45, &r_telealpha, "0"},
    {46, &vr_climb_leniency, "10"},       // 6: the author's (ROUND21.md, "Defaults: the author's climbing values")
    {46, &vr_climb_hand_side, "0"},       // 7.5: the author's, the drawn hand outwards along the edge
    // 47: the author's tired arms, "more intense" (NOTES.md vrfiringrange_2026-09-29_21-26-37; ROUND21.md, "Defaults: the
    // author's tired arms; the tired run").
    {47, &vr_weight_stamina_max, "2"},    // 2.5
    {47, &vr_weight_stamina_add, "0"},    // 15 kg
    {47, &vr_weight_stamina_empty, "3"},  // 15 kg
    // 48: the author's turn of a grenade from the pouch (NOTES.md vrfiringrange_2026-09-29_22-58-53).
    {48, &vr_grenade_pouch_hold_pitch, "0"}, // -180
    {48, &vr_grenade_pouch_hold_yaw, "0"},   // 90
    // 49: Ironwail's own HUD style (NOTES.md vrfiringrange_2026-09-30_00-53-14): vr_bindings.cfg set the classic status
    // bar before; flung props hurt players too (vrfiringrange_2026-09-30_00-56-20).
    {49, &scr_hudstyle, "0"},             // 2
    {49, &vr_prop_impact_players, "0"},   // 1
    // 50: the author's rope and hang values (NOTES.md vrfiringrange_2026-09-30_00-44-28 and 00-45-06; ROUND21.md, "Grapple
    // round 3").
    {50, &vr_grapple_reel_speed, "450"},       // 300
    {50, &vr_grapple_prop_speed, "650"},       // 300
    {50, &vr_grapple_rope_spacing, "12"},      // 4
    {50, &vr_grapple_rope_iterations, "8"},    // 32
    {50, &vr_grapple_rope_radius, "1"},        // 4
    {50, &vr_grapple_loose_slack, "24"},       // 8
    {50, &vr_grapple_hang_drag, "3"},          // 1
    {50, &vr_grapple_load_max_speed, "700"},   // 500
    // 51: the player's narrower box against walls, brush models and entities, the compiled hull's way (NOTES.md
    // e1m1_2026-09-30_02-19-27, e1m3_2026-09-30_02-34-01; ROUND21.md, "Player hitbox defaults").
    {51, &vr_hull_width, "0"},                 // 16
    // 52: the author's turn of a grenade from the pouch, tweaked again ("now quite good", NOTES.md
    // vrfiringrange_2026-09-30_02-08-07).
    {52, &vr_grenade_pouch_hold_pitch, "-180"}, // 90
    {52, &vr_grenade_pouch_hold_yaw, "90"},     // 0
    // 53: the author's melee speed: soft punches landed too rarely at 4 (NOTES.md vrfiringrange_2026-09-30_02-00-22 and
    // 02-02-15; ROUND21.md, "Melee speed 3; reloads aren't blows").
    {53, &vr_melee_speed, "4"},
    // 57: the author's (NOTES.md 2026-09-30): Narrower Monsters on (e1m1_2026-09-30_10-57-09), a little throw aim assist
    // (vrfiringrange_2026-09-30_11-25-52), and his melee pushes and bloodlust, compiled in now (vr_defaults.cfg had them
    // since round 15, without a change for the configs saved before: vrfiringrange_2026-09-30_11-20-10).
    {57, &vr_mhull, "0"},                     // 1
    {57, &vr_throw_assist, "0"},              // 1
    {57, &vr_throw_assist_cone, "12"},        // 15
    {57, &vr_throw_assist_strength, "0.8"},   // 0.35
    {57, &vr_melee_push, "1"},                // 0.5
    {57, &vr_melee_push_player, "1"},         // 0.6
    {57, &vr_melee_bloodlust_mult, "1.0"},    // 0.5
    // 59: heavy weapons strike slower (ROUND21.md, "Heavy weapons: wrenched out, sticky grips, heavy melee").
    {59, &vr_weight_lenient, "0.75"},
    {59, &vr_weight_lenient_from, "10"},
    // 60: the author's (NOTES.md 2026-09-30): his water and slime transparency again (vrcalibration_2026-09-30_16-18-35;
    // vr_defaults.cfg), and the empty hand firmer against the prop the other hand holds ("a little bit stronger",
    // vrfiringrange 16:24:29). ROUND21.md, "Prop size; Mjolnir in water; chainsaw pulls; defaults".
    {60, &r_wateralpha, "0.4"},               // 0.3
    {60, &r_slimealpha, "0.9"},               // 0.6
    // 61: the grunts' guns fire 3-round bursts (ROUND21.md, "The grunts' burst rifles"): a round's damage, not a
    // pellet's, and rounds in threes.
    {61, &vr_gruntgun_damage, "4"},           // 5
    {61, &vr_gruntgun_ammo, "10"},            // 30
    // 62: no cap on crowbars on crates (the author, 2026-10-01).
    {62, &vr_crate_crowbar_max, "1"},       // 64
    // 63: the crowbar a little softer, "its damage is a bit high" (NOTES.md vrfiringrange_2026-10-01_02-33; ROUND21.md,
    // "Weapon Damage menu"): the axe's base now.
    {63, &vr_crowbar_damage, "25"},           // 20
    // 64: the author's Sound page (NOTES.md e1m1_2026-10-01_00-41-15, "tweaked just a few values"; ROUND21.md, "Spatial
    // audio: bilinear crackle, distance, physics volume"). His HRTF smoothing (nearest) was the crackle's workaround:
    // below, not a default.
    {64, &vr_snd_reverb, "0.4"},              // 0.5
    {64, &vr_snd_nearfield, "1"},             // 1.2
    // 65: the author's (NOTES.md 2026-10-01): Grab Leniency 2 cm, "you have to touch the thing" (vrclimb 00-32-03), and
    // his two-handed grips' stickiness while swinging (vrfiringrange 00-08-01).
    {65, &vr_climb_leniency, "6"},          // 2
    {65, &vr_2h_sticky_fast, "1"},          // 3.5
    {65, &vr_2h_sticky_fast_hold, "0.4"},   // 0.6
    // and his pain knock (NOTES.md vrfiringrange_2026-10-01_00-19-04; ROUND21.md, "Pain feedback, second pass").
    {65, &vr_pain_knock_strength, "0.3"},   // 0.75
    {65, &vr_pain_knock_max, "5"},          // 7.5
    {65, &vr_pain_knock_time, "0.35"},      // 0.6
    // 66: the author's crates (NOTES.md e1m2_2026-10-01_02-58-14, "make those the defaults").
    {66, &vr_crates_max, "16"},             // 24
    {66, &vr_crate_crowbar, "0.05"},        // 0.1
    {66, &vr_crate_crowbar_max, "64"},      // 4
    // 67: thrown axes stick by which part goes in first (ROUND21.md, "Thrown axes: the blade decides"). Stick Angle and
    // Stick Incidence mean something new (the blade's way out of its plane, its way off straight in): a config opened
    // all the way (90, the author's, to make the old test pass) takes the defaults, or flat throws would stick.
    {67, &vr_axestick_angle, "90"},          // 45
    {67, &vr_axestick_incidence, "90"},      // 65
    // 68: explosive boxes held up as shields too ("it's the player's fault if they use an explosive as a shield", the
    // author, 2026-10-01).
    {68, &vr_crate_shield, "1"},            // 2
    // 69: the author's sound options (NOTES.md start_2026-10-01_11-47-28, "Distance falloff helps") and Mid-Air
    // Leniency (vrclimb_2026-10-01_11-49-25).
    {69, &vr_snd_falloff, "1"},             // 0.75
    {69, &vr_snd_hrtf_gain, "1.25"},        // 1.5
    {69, &vr_physsound, "1"},               // 2
    {69, &vr_physsound_scrape, "0.7"},      // 0.8
    {69, &vr_physsound_grab, "0.5"},        // 0.7
    {69, &vr_climb_leniency_air, "2.5"},    // 4
    // 70: the empty hand meets the other hand's weapon as a prop held there does (NOTES.md
    // vrfiringrange_2026-10-01_12-12-28): each drawn moved back up to this, not the hand alone held up to 4 cm.
    {70, &vr_hand_collide, "4"},            // 5
    // 71: the author's (NOTES.md 2026-10-01): gentler gibs from melee and light weapons (vrfiringrange 16-40-58, "make
    // those the defaults"), and his pain knock after its third pass (vrfiringrange 16-45-39). ROUND21.md, "Defaults:
    // gibs, pain knock, grenade grips; mantle grunt; prop pile; one empty-hand collision".
    {71, &vr_gib_speed_melee, "0.45"},      // 0.15
    {71, &vr_gib_speed_light, "0.65"},      // 0.25
    {71, &vr_pain_knock_strength, "0.75"},  // 1
    {71, &vr_pain_knock_max, "7.5"},        // 15
    {71, &vr_pain_knock_tip, "2"},          // 3
    // 72: the author's enemy shove settings (NOTES.md vrfiringrange_2026-10-01_16-59-13).
    {72, &vr_enemy_shove_range, "52"},      // 50
    {72, &vr_enemy_shove_delay, "0.6"},     // 0.5
    {72, &vr_enemy_shove_cooldown, "3"},    // 1.5
    {72, &vr_enemy_shove_distance, "64"},   // 128
    // 73: the author's chainsaw smoke and shake (NOTES.md vrfiringrange_2026-10-01_22-34-43).
    {73, &vr_chainsaw_smoke, "8"},          // 12
    {73, &vr_chainsaw_smoke_alpha, "0.12"}, // 0.6
    {73, &vr_chainsaw_shake, "2"},          // 2.5
    {73, &vr_chainsaw_shake_2h, "0.8"},     // 1.25
    {73, &vr_chainsaw_shake_ground, "0.3"}, // 0.5
    // 74: the author's axe sticking, tuned again (NOTES.md
    // vrfiringrange_2026-10-01_23-04-14): a slower throw sticks.
    {74, &vr_axestick_speed, "4"},          // 2.5
    // 75: the author's (NOTES.md 2026-10-02): the ripcord pull's smoke and sparks (vrfiringrange 00-17-30), the pain
    // grunt as the mantle's (start 00-18-45), throws by weight with his heavier props (vrfiringrange 00-55-03) and the
    // crates (vrfiringrange 01-41-30). ROUND21.md, "Defaults: chainsaw pull, mantle grunt, prop weights, crates".
    {75, &vr_chainsaw_pull_smoke, "4"},     // 12
    {75, &vr_chainsaw_pull_sparks, "6"},    // 24
    {75, &vr_climb_mantle_grunt_sound, "1"}, // 4
    {75, &vr_throw_mass_light, "2"},        // 1.5
    {75, &vr_throw_mass_exp, "0.75"},       // 0.9
    {75, &vr_crate_health, "25"},           // 75
    {75, &vr_crate_impact, "14"},           // 16
    {75, &vr_crate_pieces, "6"},            // 12
    {75, &vr_crate_piece_time, "30"},       // 60
    // 76: the author's flashlight (NOTES.md vrfiringrange_2026-10-02_15-46-47, _15-50-31): clipping on when let go (head
    // and gun), the head's zone and its range, the low grip's place and fingers. ROUND21.md, "Flashlight: the gun's zone
    // at its mount; a grab sound; the author's defaults".
    {76, &vr_flashlight_auto_head, "0"},            // 1
    {76, &vr_flashlight_auto_gun, "0"},             // 1
    {76, &vr_flashlight_head_range, "1"},           // 1.5
    {76, &vr_flashlight_head_zone_forward, "0.015"}, // 0.05
    {76, &vr_flashlight_head_zone_up, "0.175"},     // 0.19
    {76, &vr_flashlight_head_zone_radius, "0.1"},   // 0.105
    {76, &vr_flashlight_low_x, "0.5"},              // -1
    {76, &vr_flashlight_low_y, "-1"},               // -1.5
    {76, &vr_flashlight_low_z, "-0.5"},             // -1.5
    {76, &vr_flashlight_low_pitch, "-50"},          // -65
    {76, &vr_flashlight_low_overlap, "0.45"},       // 0.6
    {76, &vr_flashlight_low_bias_thumb, "-0.06"},   // -0.02
    {76, &vr_flashlight_low_bias_index, "0.02"},    // 0.08
    {76, &vr_flashlight_low_bias_pinky, "-0.36"},   // 0
    {76, &vr_flashlight_low_thumb_x, "0.5"},        // 0.6
    {76, &vr_flashlight_low_thumb_y, "0"},          // 0.15
    // 77: the near clip for a gun at the eye (ROUND21.md, "Near clip: a gun at the eye").
    {77, &vr_nearclip, "1"},                // 0.1 (float depth for the eyes)
    // 78: the author's (NOTES.md 2026-10-02): his pain knock, "happy with them now" (vrfiringrange 15-21-40); throws hurt
    // half as much, "way too high" (vrfiringrange 15-30-01); heavier rolls of the wrist for every weapon, "the default
    // value of 1 is way too small" (vrfiringrange 15-40-14). ROUND21.md, "Defaults: pain knock, throw damage, roll weight".
    {78, &vr_pain_knock_strength, "1"},     // 0.75
    {78, &vr_pain_knock_max, "15"},         // 10
    {78, &vr_pain_knock_time, "0.6"},       // 0.35
    {78, &vr_weapon_throw_damage_mult, "1.0"}, // 0.5 (the old default as written)
    {78, &vr_weapon_throw_damage_mult, "1"}, // 0.5 (as the menu writes it)
    {78, &vr_weight_spring_roll, "1"},      // 2.5
    // 79: the author's settings (his config of 2026-10-02 19:34; ROUND21.md, "Defaults: the author's config, 2026-10-02
    // evening"): breath, swimming, the chainsaw on the ground, the wrist FPS counter off, his wounds and small gibs (the
    // melee and chainsaw small gibs' speeds are not his yet: the sliders' ends, being fixed), the checklist's ticked
    // items hidden.
    {79, &vr_air_supply, "1.5"},                 // 2
    {79, &vr_swim_look, "0.2"},                  // 0.3 (vr_defaults.cfg)
    {79, &vr_swim_stroke_pitch, "0"},            // -8
    {79, &vr_chainsaw_shake_ground, "0.5"},      // 0.85
    {79, &vr_gadget_fps, "2"},                   // 0 (vr_defaults.cfg had 2)
    {79, &vr_checklist_hide_ticked, "0"},        // 1
    {79, &vr_wounds_blood_alpha, "0.8"},         // 0.75
    {79, &vr_wounds_bump_blood, "1"},            // 1.5
    {79, &vr_smallgibs_blades, "1.5"},           // 1.7
    {79, &vr_smallgibs_burst, "3"},              // 4
    {79, &vr_smallgibs_curve, "1.5"},            // 1.7
    {79, &vr_smallgibs_damage_per_gib, "40"},    // 30
    {79, &vr_smallgibs_destroy, "0"},            // 1
    {79, &vr_smallgibs_full_damage, "60"},       // 35
    {79, &vr_smallgibs_gibbing, "6"},            // 12
    {79, &vr_smallgibs_grace, "0.15"},           // 0.1
    {79, &vr_smallgibs_max, "40"},               // 64
    {79, &vr_smallgibs_min_damage, "12"},        // 10
    {79, &vr_smallgibs_nails, "0"},              // 0.4
    {79, &vr_smallgibs_per_hit, "4"},            // 6
    {79, &vr_smallgibs_player, "0"},             // 1
    {79, &vr_smallgibs_props, "1"},              // 1.2
    {79, &vr_smallgibs_saw_interval, "0.2"},     // 0.1
    {79, &vr_smallgibs_size_max, "1"},           // 1.15
    {79, &vr_smallgibs_size_min, "0.5"},         // 0.65
    {79, &vr_smallgibs_speed, "4"},              // 3
    {79, &vr_smallgibs_up, "5"},                 // 7
    {79, &vr_smallgibs_speed_guns, "1"},         // 0.85
    {79, &vr_smallgibs_up_guns, "1"},            // 0.9
    // 80: the author's answer on the outliers (2026-10-02): his wound detail and burns relief.
    {80, &vr_wounds_own_res, "1024"},          // 0 (the chunky masks)
    {80, &vr_wounds_bump_burns, "1"},          // 3
    // 81: Quetoo's material maps shipped and on (quakevr/textures_quetoo; ROUND21.md, "Quetoo's maps shipped").
    {81, &vr_extmaps, "0"},                    // 1
    {81, &vr_extmaps_dir, "textures_ext"},     // textures_quetoo (an absolute Quetoo folder: below)
    // 82: the author's blood opacity (NOTES.md vrfiringrange_2026-10-03_02-15-04: "a little below the blood").
    {82, &vr_wounds_blood_alpha, "0.75"},      // 0.8
    // 83: the flashlight's chain the default cord (NOTES.md vrfiringrange_2026-10-03_02-29-04).
    {83, &vr_flashlight_cord, "1"},            // 3
    // 84: ragdolls on (the author, 2026-10-03; ROUND21.md, "Ragdolls on; corpse health and damage by kind").
    {84, &vr_ragdoll, "0"},                    // 1
    // 85: each monster's ragdoll its own mass (the author, 2026-10-03; ROUND21.md, "Ragdoll masses per monster"): a class
    // still on Global (-1, the 80 kg of vr_ragdoll_mass) takes its own; one set keeps it.
    {85, &vr_ragdoll_army_mass, "-1"},         // 80
    {85, &vr_ragdoll_knight_mass, "-1"},       // 90
    {85, &vr_ragdoll_ogre_mass, "-1"},         // 200
    {85, &vr_ragdoll_enforcer_mass, "-1"},     // 100
    {85, &vr_ragdoll_hknight_mass, "-1"},      // 130
    {85, &vr_ragdoll_dog_mass, "-1"},          // 40
    {85, &vr_ragdoll_wizard_mass, "-1"},       // 40
    // 86: brain chunks' own throw, and each monster's small gib and brain chunk counts (the author, 2026-10-04;
    // ROUND21.md, "Brain chunks' own throw; per-enemy gib counts"). Every one is a new setting: no config of 85 or
    // before holds it, so it takes its compiled default, which is what happened then (brain chunks flew at Speed and
    // Up; every count times 1). Listed so this version's additions are named one by one.
    {86, &vr_smallgibs_brains_speed, "3"},           // 3 (new: what vr_smallgibs_speed was then)
    {86, &vr_smallgibs_brains_up, "7"},              // 7 (new: what vr_smallgibs_up was then)
    {86, &vr_smallgibs_mult_grunt, "1"},             // 1 (new)
    {86, &vr_smallgibs_mult_enforcer, "1"},          // 1 (new)
    {86, &vr_smallgibs_mult_dog, "1"},               // 1 (new)
    {86, &vr_smallgibs_mult_fiend, "1"},             // 1 (new)
    {86, &vr_smallgibs_mult_ogre, "1"},              // 1 (new)
    {86, &vr_smallgibs_mult_knight, "1"},            // 1 (new)
    {86, &vr_smallgibs_mult_hellknight, "1"},        // 1 (new)
    {86, &vr_smallgibs_mult_vore, "1"},              // 1 (new)
    {86, &vr_smallgibs_mult_shambler, "1"},          // 1 (new)
    {86, &vr_smallgibs_mult_scrag, "1"},             // 1 (new)
    {86, &vr_smallgibs_mult_fish, "1"},              // 1 (new)
    {86, &vr_smallgibs_mult_gremlin, "1"},           // 1 (new)
    {86, &vr_smallgibs_mult_scourge, "1"},           // 1 (new)
    {86, &vr_smallgibs_mult_eel, "1"},               // 1 (new)
    {86, &vr_smallgibs_mult_zombie, "1"},            // 1 (new)
    {86, &vr_smallgibs_mult_mummy, "1"},             // 1 (new)
    {86, &vr_smallgibs_brains_mult_grunt, "1"},      // 1 (new)
    {86, &vr_smallgibs_brains_mult_enforcer, "1"},   // 1 (new)
    {86, &vr_smallgibs_brains_mult_dog, "1"},        // 1 (new)
    {86, &vr_smallgibs_brains_mult_fiend, "1"},      // 1 (new)
    {86, &vr_smallgibs_brains_mult_ogre, "1"},       // 1 (new)
    {86, &vr_smallgibs_brains_mult_knight, "1"},     // 1 (new)
    {86, &vr_smallgibs_brains_mult_hellknight, "1"}, // 1 (new)
    {86, &vr_smallgibs_brains_mult_vore, "1"},       // 1 (new)
    {86, &vr_smallgibs_brains_mult_shambler, "1"},   // 1 (new)
    {86, &vr_smallgibs_brains_mult_scrag, "1"},      // 1 (new)
    {86, &vr_smallgibs_brains_mult_fish, "1"},       // 1 (new)
    {86, &vr_smallgibs_brains_mult_gremlin, "1"},    // 1 (new)
    {86, &vr_smallgibs_brains_mult_scourge, "1"},    // 1 (new)
    {86, &vr_smallgibs_brains_mult_eel, "1"},        // 1 (new)
    {86, &vr_smallgibs_brains_mult_zombie, "1"},     // 1 (new)
    {86, &vr_smallgibs_brains_mult_mummy, "1"},      // 1 (new)
    // October 4 recorded tuning: migrate only untouched prior defaults.
    {87, &vr_unstick, "0"},
    {87, &vr_hit_tolerance_guns, "4"},
    {87, &vr_hit_tolerance_melee, "6"},
    {87, &vr_snd_hrtf_gain, "1.5"},
    {87, &vr_mirror, "1"},
    {87, &vr_window_view, "0"},
    {87, &vr_window_smooth, "0.15"},
    {87, &vr_window_level, "0"},
    {87, &vr_spectator_fov, "90"},
    {87, &vr_spectator_scale, "0.75"},
    {87, &vr_bullettime_duration, "6"},
    {87, &vr_window_hud_mirror, "1"},
    {87, &vr_world_scale, "1.25"},
    {87, &vr_floor_offset, "-21"},
    {87, &vr_menu_level, "0"},
    {87, &vr_burn_touch, "0"},
    {87, &vr_flashlight_cord, "3"},
    {87, &vr_flashlight_mount_preview, "1"},
    {87, &vr_flashlight_low_pitch, "-65"},
    {87, &vr_flashlight_low_bias_index, "0.08"},
    {87, &vr_fire_particles_count, "1"},
    {87, &vr_fire_particles_size, "1"},
    {87, &vr_fire_particles_range, "1200"},
    {87, &vr_explosion_debris_count, "12"},
    {87, &vr_explosion_debris_speed_min, "3"},
    {87, &vr_explosion_debris_speed_max, "9"},
    {87, &vr_explosion_debris_up, "0.65"},
    {87, &vr_ao_dynamic, "1"},
    {87, &vr_ao_dynamic_range, "3"},
    {87, &vr_retrolight, "0"},
    {87, &vr_retrolight_world_dither, "0"},
    {87, &vr_retrolight_world_luxel, "16"},
    {87, &vr_retrolight_world_dyn_steps, "8"},
    {87, &vr_retrolight_world_dyn_block, "4"},
    {87, &vr_retrolight_model_steps, "16"},
    {87, &vr_retrolight_model_dither, "0"},
    {87, &vr_retrolight_shadow_block, "0"},
    {87, &vr_light_contrast, "2.3"},
    {87, &vr_smallgibs_speed_melee, "0.35"},
    {87, &vr_smallgibs_up_melee, "0.5"},
    {87, &vr_smallgibs_speed_saw, "0.5"},
    {87, &vr_smallgibs_up_saw, "0.6"},
    {87, &vr_smallgibs_speed_guns, "0.85"},
    {87, &vr_smallgibs_up_guns, "0.9"},
    {87, &vr_smallgibs_grace, "0.1"},
    {87, &vr_smallgibs_brains_burst, "6"},
    {87, &vr_smallgibs_brains_pop, "8"},
    {87, &vr_smallgibs_brains_speed, "3"},
    {87, &vr_smallgibs_brains_up, "7"},
    {87, &vr_smallgibs_mult_enforcer, "1"},
    {87, &vr_smallgibs_mult_fiend, "1"},
    {87, &vr_smallgibs_mult_ogre, "1"},
    {87, &vr_smallgibs_mult_hellknight, "1"},
    {87, &vr_smallgibs_mult_vore, "1"},
    {87, &vr_smallgibs_mult_shambler, "1"},
    {87, &vr_smallgibs_brains_mult_enforcer, "1"},
    {87, &vr_smallgibs_brains_mult_fiend, "1"},
    {87, &vr_smallgibs_brains_mult_ogre, "1"},
    {87, &vr_smallgibs_brains_mult_vore, "1"},
    {87, &vr_smallgibs_brains_mult_shambler, "1"},
    {87, &vr_gore_stick_thrown, "0.5"},
    {87, &vr_gore_stick_speed, "180"},
    {87, &vr_extmaps_spec_scale, "4"},
    {87, &vr_retro, "0"},
    {87, &vr_teleporter_surface_size, "1.12"},
    {87, &vr_teleporter_surface_opacity, "1"},
    {87, &vr_knockdown_chance, "1"},
    {87, &vr_knockdown_damage, "2"},
    {87, &vr_knockdown_bash, "0"},
    {87, &vr_knockdown_time_min, "2.5"},
    {87, &vr_knockdown_time_max, "4.5"},
    {87, &vr_knockdown_getup_speed, "1"},
    {87, &vr_decap_speed, "5"},
    {87, &vr_decap_head_size, "1.25"},
    {87, &vr_decap_neck, "6"},
    {87, &vr_decap_pop_body_speed, "0.1"},
    {87, &vr_decap_fountain, "2.5"},
    {87, &vr_walltorch_inv_size, "1.25"}, // legacy three-flame size
    {87, &vr_ragdoll_grab, "2"}, // ragdolls are taken by hand only
    {88, &vr_particle_retro_halfres_pixels, "64"}, // 0: all particles in their order (the split drew small over large)
    // 89: the author's tuned parallax (2026-10-06): further, no grazing fade, its depth written
    {89, &vr_parallax_distance, "512"},     // 1024
    {89, &vr_parallax_grazing, "86"},       // 90
    {89, &vr_parallax_depth_write, "0"},    // 1
    // 89: the author's combat tweaks (2026-10-06): the hands' knock from hits, burning, struggling knockdowns
    {89, &vr_pain_knock_strength, "0.75"},  // 0.6
    {89, &vr_pain_knock_max, "10"},         // 8.5
    {89, &vr_pain_knock_time, "0.35"},      // 0.3
    {89, &vr_burn_flames_max, "7"},         // 12
    {89, &vr_burn_self, "0"},               // 1
    {89, &vr_burn_drop, "0"},               // 1
    {89, &vr_knockdown_wiggle, "0.7"},      // 1
    {89, &vr_knockdown_wiggle_frequency, "1.2"}, // 2.2
    {89, &vr_knockdown_wiggle_pause, "1"},  // 0
    {89, &vr_parry_stagger, "0.35"},        // 0.75
    {89, &vr_counter_damage, "1.2"},        // 1.5 (vr_defaults.cfg's 1.2 dropped: the compiled default again)
    // 90: the training dummy dies (Combat > Gore > Dummy Dies; the author, 2026-10-06: "make gib mode the default")
    {91, &vr_dummy_gib, "0"},               // 1
    // 92: the author's decisions (2026-10-06; ROUND21.md, "The author's combat decisions"): thrown axes stick in
    // explosive boxes as in every other prop.
    {92, &vr_axestick_metal, "0"},          // 1
    // 92: thrown gibs almost always stick to walls (the author, 2026-10-06): Thrown Gibs Stick his 0.75 to 1
    // (vr_defaults.cfg; VR_Gib_Think2 also tells a lobbed gib's hit on a wall now).
    {92, &vr_gore_stick_thrown, "0.75"},    // 1
    // 93: Relighting's Light Textures 1.2 (the author relit hip1m1 so, 2026-10-06: "Maybe those should be the new
    // defaults"); relight_maps.py's --light-texture-strength too.
    {93, &vr_relight_strength, "1"},        // 1.2
    // 94: rocks and bricks in multiplayer as many as in single player (the author, 2026-10-06; MULTIPLAYER.md, "Rocks
    // and bricks in multiplayer"): Most in Multiplayer's old 0 (none) to -1 (Single Player's: Most in a Map).
    {94, &vr_debris_mp_max, "0"},           // -1
    // 95: the author's own gameplay and look values (INSTALLER.md, Appendix A, "Gameplay and look: promote?"; his
    // config, 2026-10-07): fire particles, head pops, limb grabs, the messages' hologram. The All Categories retro panel's
    // values (migrateConfig: retro::migrateAllPanel), the grappling hook's flashlight (vr_wofs_version 35) and the gibs'
    // and heads' weights (vr_props_version 58) too.
    {95, &vr_fire_particles_alpha, "0.55"},         // 1
    {95, &vr_fire_particles_count, "6"},            // 8
    {95, &vr_fire_particles_origin, "0.25"},        // 0.2
    {95, &vr_fire_particles_size, "2"},             // 2.5
    {95, &vr_decap_pop_always_range, "3"},          // 2
    {95, &vr_decap_pop_never_range, "15"},          // 12
    {95, &vr_decap_pop_thrown_light_chance, "0"},   // 0.25
    {95, &vr_ragdoll_grab_reach, "6"},              // 2
    {95, &vr_ragdoll_hand_stick, "12"},             // 2
    {95, &vr_messages_hologram_height, "5"},        // 10
    // 96: the menus' red as the author has it (his config, 2026-10-07; hue 0 and strength 1 as shipped already).
    {96, &vr_menu_recolor_saturation, "1.25"},      // 3
    // 97: immersive manual reloading is the shipped mode (the author, 2026-10-07; docs/vr-port/RELOAD_PLAN.md): a config with
    // the old default (Hip Holsters) takes it; one that chose Off or All Holsters keeps its choice.
    {97, &vr_reload_mode, "2"},                     // 3
    // 98: the author's reloading values (ROUND21.md, "Immersive reloading: magazines, both grips, the pull").
    {98, &vr_ammo_pouch_counter_x, "2.5"}, // 2.25
    {98, &vr_ammo_pouch_counter_z, "3.2"}, // 2
    {98, &vr_ammo_pouch_counter_scale, "0.8"}, // 0.6
    {98, &vr_ammo_pouch_leg_follow, "1"}, // 0.5
    {98, &vr_reload_bump_speed, "2"},   // 3
    {98, &vr_reload_pull_speed, "2.5"}, // 2
    {98, &vr_reload_pull_snap, "600"},  // 300
    {98, &vr_reload_hit_reach, "4"},    // 2
    {98, &vr_reload_pull_reach, "6"},   // 2
    {98, &vr_reload_collide_leniency, "30"}, // 4
    {98, &vr_reload_port_shot_radius, "4"}, // 1.5
    {98, &vr_reload_port_nail_radius, "1"}, // 1.5
    {98, &vr_reload_port_snail_radius, "1"}, // 1.5
    {98, &vr_reload_port_light_radius, "1"}, // 1.5
    {98, &vr_reload_bump_speed, "6.5"},         // 3: the author's, raised while the hand reaching to hold the magazine knocked it out (fixed)
    // 100: the author's settings at the firing range, 2026-10-07 (his note vrfiringrange_2026-10-07_22-08-51: "make all
    // the tweaks I'm making the new defaults"; ROUND21.md, "Reloading: the firing range notes of 10-07").
    {100, &vr_ammo_pouch_counter, "1"}, // 0
    {100, &vr_ammo_pouch_x, "0"}, // 3.02
    {100, &vr_decap_pop_sg_falloff, "4"}, // 1.5
    {100, &vr_flashlight_flick_speed, "600"}, // 800
    {100, &vr_knockdown_ledge_drop, "64"}, // 16
    {100, &vr_knockdown_ledge_margin, "16"}, // 24
    {100, &vr_knockdown_ledge_reach, "1"}, // 1.25
    {100, &vr_reload_pull_snap, "300"}, // 225
    {100, &vr_reload_ssg_flick_close_speed, "650"}, // 400
    {100, &vr_reload_ssg_open_flick, "1"}, // 0
    {100, &vr_weapon_button_cone, "80"}, // 50
    {100, &vr_weapon_throw_damage_mult, "0.5"}, // 0.35
    // 101: detail textures off by default (the author, 2026-10-07).
    {101, &vr_detail, "1"}, // 0
    {101, &vr_menu_fine_step, "0.0999"}, // 0.1: the author's, slider noise from plain steps landing off the grid (fixed)
    // 102: Swing Through Enemies on, the author's values (his note vrfiringrange 23-52-45: "more responsive").
    {102, &vr_melee_phase, "0"}, // 1
    {102, &vr_melee_phase_speed, "2.25"}, // 4 (his 3.996: slider noise)
    {102, &vr_melee_phase_time, "0.15"}, // 0.35 (his 0.34965)
    // 104: the author's settings of 2026-10-08 (his note vrfiringrange_2026-10-08_14-16-19: "I tweaked quite a few
    // settings and hotspots, please make them the new defaults"; ROUND21.md, "Out of the water by hand; ...").
    {104, &vr_enemygun_spent_crackle, "2.5"}, // 1
    {104, &vr_enemygun_spent_volume, "0.25"}, // 0.5
    {104, &vr_snd_pitch_jitter, "4"}, // 10
    {104, &vr_ssg_fire_anim_speed, "1.4"}, // 1.75
    {104, &vr_stealth_corpses, "600"}, // 700
    {104, &vr_stealth_meter_time, "1.5"}, // 1
    {104, &vr_stealth_run_speed, "250"}, // 280
    {104, &vr_stealth_torch, "400"}, // 500
    {104, &vr_teleporter_surface_opacity, "0.3"}, // 0.5 (vr_defaults.cfg)
    {104, &vr_reload_port_shot_radius, "1.5"}, // 1.6
    // 105: a smaller stick deadzone (the author, 2026-10-08; worn sticks that drift can raise it again).
    {105, &vr_deadzone, "25"}, // 10
    // 106: the Immersive Death View by default (the author, NOTES.md vrfiringrange_2026-10-08_22-44-28).
    {106, &vr_death_view, "1"}, // 2
};
constexpr int configVersion = 106;

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
    FILE* f = fopen(path, "rb");
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

// A full path to Quetoo's textures/quake folder (quetoo-data's, downloaded): config version 81 takes the shipped copy.
[[nodiscard]] bool quetooFolder(const char* dir)
{
    za::String s{dir};
    for(size_t i = 0; i < s.size(); i++)
    {
        s[i] = s[i] == '\\' ? '/': static_cast<char>(tolower(static_cast<unsigned char>(s[i])));
    }
    while(!s.empty() && s.back() == '/')
    {
        s.popBack();
    }
    const bool absolute = s.size() > 1 && (s[0] == '/' || s[1] == ':');
    return absolute && s.find("quetoo") != za::StringView::nPos && s.endsWith("/textures/quake");
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
        weapons::markCurrent(); // and the weapons' settings are the shipped ones (no cups or grips of a config to move)
        return;
    }
    const int from = static_cast<int>(vr_cfg_version.value);
    if(from >= configVersion)
    {
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
    // 64: bilinear HRTF smoothing crackled (two voices interpolating one HRTF at once on the pool made not-numbers;
    // each pool lane has its own HRTF now): a config that went to nearest to get away from it goes back to bilinear.
    if(from < 64 && vr_snd_hrtf_interp.value == 0.f)
    {
        Con_DPrintf("VR: vr_snd_hrtf_interp: 1 (bilinear no longer crackles)\n");
        Cvar_SetQuick(&vr_snd_hrtf_interp, "1");
    }
    // 12: one colour for the player's effects (vr_player_hue, vr_hue.hpp). The gadget's screen hue
    // was the one; a config's becomes the player's, and the screen follows it: nothing changes but
    // the effects that had colours of their own (the force grab's, the teleport arc's...) now match.
    if(from < 12 && vr_gadget_screen_hue.value >= 0.f)
    {
        Con_DPrintf("VR: vr_player_hue %s (the gadget's screen hue, which follows it now)\n", vr_gadget_screen_hue.string);
        Cvar_SetQuick(&vr_player_hue, vr_gadget_screen_hue.string);
        Cvar_SetQuick(&vr_gadget_screen_hue, "-1");
    }
    // 13: melee redesigned (round 21, docs/vr-port/ROUND21.md). vr_melee_speed is the striking hand's speed now (the
    // controller's, relative to the head; a weapon's swing 1.25x), not the estimated wrist's with a stroke's distance
    // and snap; vr_bash_speed the parry stance's push after it was held, both its ends going forward, however the
    // weapon turns. Their old values meant something else: both go to the new defaults, whatever they were.
    if(from < 13)
    {
        for(cvar_t* var : {&vr_melee_speed, &vr_bash_speed})
        {
            Con_DPrintf("VR: %s: new default %s (was %s; its meaning changed)\n", var->name, var->default_string,
                var->string);
            Cvar_SetQuick(var, var->default_string);
        }
    }
    // 15: a hip or upper holster on the body goes on round it past its front, where it used to stop (round 21,
    // "Arms options after body calibration; holster limits"): an X that was stopped keeps its place.
    if(from < 15)
    {
        body::migrateHolsters();
    }
    // 45: the hip and upper holsters' X shipped as -7 and -8 (vr_defaults.cfg, 2026-09-28), which the author's config
    // holds as -3.5 and -4.25 since 15 moved it (the same place on the body): the compiled-in defaults, now shipped too. A
    // config saved from 15 on that still has the old shipped value takes it (before 15, 15 has moved it above).
    if(from >= 15 && from < 45)
    {
        struct Old
        {
            cvar_t* var;
            const char* before;
        };
        for(const auto& [var, before] : {Old{&vr_hip_offset_x, "-7"}, Old{&vr_upper_holster_offset_x, "-8"}})
        {
            if(sameValue(var->string, before))
            {
                Con_DPrintf("VR: %s: new default %s (was %s)\n", var->name, var->default_string, before);
                Cvar_SetQuick(var, var->default_string);
            }
        }
    }
    // 16: the author's hand calibration ships with his weapon offsets, which were set over it (round 21, "Defaults: the
    // author's weapon offsets and settings"; the weapons take his offsets: vr_wofs_version 20). A config that never
    // calibrated its hands (each value the old default, or the new one) takes all of it, or its guns would sit off its
    // hands; one that did keeps its own. A config saved before the calibration existed (13) has no vr_handcal_*: its
    // hands were at 0, not at the new defaults it has now.
    if(from < 16)
    {
        const struct
        {
            cvar_t* var;
            const char* before;
        } angles[] = {
            {&vr_gunangle, "39.5"}, {&vr_gunyaw, "4"}, {&vr_offhandpitch, "40.25"}, {&vr_offhandyaw, "-4"}};
        cvar_t* const moves[] = {&vr_handcal_x, &vr_handcal_y, &vr_handcal_z, &vr_handcal_roll, &vr_handcal_off_mirror,
            &vr_handcal_off_x, &vr_handcal_off_y, &vr_handcal_off_z, &vr_handcal_off_roll};
        if(from < 13)
        {
            for(cvar_t* var : moves)
            {
                Cvar_SetQuick(var, "0");
            }
        }
        bool untouched = true;
        for(const auto& [var, before] : angles)
        {
            untouched = untouched && (sameValue(var->string, before) || sameValue(var->string, var->default_string));
        }
        for(const cvar_t* var : moves)
        {
            untouched = untouched && (sameValue(var->string, "0") || sameValue(var->string, var->default_string));
        }
        if(untouched)
        {
            for(const auto& [var, before] : angles)
            {
                Cvar_SetQuick(var, var->default_string);
            }
            for(cvar_t* var : moves)
            {
                Cvar_SetQuick(var, var->default_string);
            }
            Con_DPrintf("VR: the hand calibration: the new defaults (vr_gunangle %s, vr_handcal_x %s...)\n", vr_gunangle.string,
                vr_handcal_x.string);
        }
    }
    // 25: the wall torch taken off its wall (round 21, "Wall torches you can take") has shipped Held Object Offsets in
    // a slot that configs saved empty (every slot is archived).
    if(from < 25)
    {
        props::takeShippedSlot(props::wallTorchSlot);
    }
    // 27: the gibs and heads have shipped Held Object Weights (round 21, "Spring only; Weapon Weights and Held Object
    // Weights; weight and damage": their masses) in slots that configs saved empty.
    if(from < 27)
    {
        for(int slot = props::fleshSlotsFirst; slot <= props::fleshSlotsLast; slot++)
        {
            props::takeShippedSlot(slot);
        }
    }
    // 56: the author's grappling gun buttons (NOTES.md vrfiringrange_2026-09-30_02-55-58): the back button half a unit to
    // the side (vr_wofs_wpnbtn_y_18; the front one is placed from it, its own place unchanged). A weapon setting: a config
    // still holding the old default takes the new one.
    if(from < 56)
    {
        cvar_t* var = weapons::cvar(17, weapons::Key::WpnButtonY);
        if(var && sameValue(var->string, "0"))
        {
            Con_DPrintf("VR: %s: new default %s (was 0)\n", var->name, var->default_string);
            Cvar_SetQuick(var, var->default_string);
        }
    }
    // 89: the author's retro textures (vr_retro.cpp shippedLook): every kind's settings still at the old defaults.
    if(from < 89)
    {
        retro::migrateShippedLook();
    }
    // 95: the All Categories retro panel's values start from the shipped look (the author's vr_retro_all_* values).
    if(from < 95)
    {
        retro::migrateAllPanel();
    }
    // 81: a config pointing vr_extmaps_dir at a downloaded Quetoo folder (quetoo-data/.../textures/quake) takes the
    // shipped copy (relative, in every install).
    if(from < 81 && quetooFolder(vr_extmaps_dir.string))
    {
        Con_DPrintf("VR: vr_extmaps_dir: %s (was %s)\n", vr_extmaps_dir.default_string, vr_extmaps_dir.string);
        Cvar_SetQuick(&vr_extmaps_dir, vr_extmaps_dir.default_string);
    }
    // 90: the flashlight's cord is the low-poly chain or none (the author, 2026-10-06: ROUND21.md, "Flashlight cord:
    // the low-poly chain only"). The coiled cord (1), the plain cable (2), the chain (3) and the low-poly chain (4) are
    // all the low-poly chain, now 1; none (0) stays.
    if(from < 90 && vr_flashlight_cord.value != 0.f && vr_flashlight_cord.value != 1.f)
    {
        Con_DPrintf("VR: vr_flashlight_cord: 1, the low-poly chain (was %s)\n", vr_flashlight_cord.string);
        Cvar_SetQuick(&vr_flashlight_cord, "1");
    }
    // 99: the island hub vrstart2 is vrstart now (the old vrstart is vrstart_old): a config naming vrstart2 names vrstart.
    if(from < 99 && !strcmp(vr_hub_map.string, "vrstart2"))
    {
        Con_DPrintf("VR: vr_hub_map: vrstart (was vrstart2: the island hub's old name)\n");
        Cvar_SetQuick(&vr_hub_map, "vrstart");
    }
    // 103: a new install starts in the tutorial (vrtutorial2) once; a config from before has played: the hub, as before.
    if(from < 103)
    {
        Cvar_SetValueQuick(&vr_tutorial_started, 1.f);
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
    FILE* f = fopen(path, "wb");
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
    Cmd_AddCommand("vr_default", default_f);
    Cmd_AddCommand("vr_savedefaults", saveDefaults_f);
}

} // namespace qvr

// Host_WriteConfigurationToFile, before the game folder's config is written over: the settings another copy of the
// game changed in it since this copy read or wrote it, and this copy left alone, are taken (configTrack).
// Settings removed (docs/vr-port/CVAR_AUDIT.md): a config, a shipped .cfg or a take that still sets one is not an
// "Unknown command" (cmd.c asks here); the line is dropped quietly and the next config write leaves it out.
constexpr const char* retiredCvars[] = {
    "vr_throw_lookahead", // 2026-10-06: unused since the throw's window ends at the release (ROUND21.md, "Throws at any frame rate")
    "vr_throw_slowmo_flick_spin", // 2026-10-08: vr_throw_slowmo_flick_arm (ROUND21.md, "Wrist flicks in bullet time: the arm tells the tempo")
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
