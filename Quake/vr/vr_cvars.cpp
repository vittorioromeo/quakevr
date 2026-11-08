// vr_cvars.cpp -- Quake VR cvar definitions and registration.

#include "vr_cvars.hpp"
#include "vr_aggrip.hpp"
#include "vr_agpose.hpp"
#include "vr_bodycal.hpp"
#include "vr_bodyclone.hpp"
#include "vr_engine.hpp"
#include "vr_gfxpresets.hpp"
#include "vr_hands.hpp"
#include "vr_mgroups.hpp"
#include "vr_monsterdata.hpp"
#include "vr_props.hpp"
#include "vr_seated.hpp"
#include "vr_weapons.hpp"

#include "Zancle/Base/Strncmp.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/StringView.hpp"
#include "vr_zancle.hpp"

#include <algorithm>

extern "C" cvar_t gl_texture_anisotropy; // gl_texmgr.c

namespace qvr
{

#define QVR_CVAR(name, def, flags) cvar_t name = {#name, def, flags};
#include "vr_cvars.inc"
#undef QVR_CVAR

// (the tuning constants, their stand-ins and live tuning: vr_tunables.cpp)

cvar_t vr_backend = {"vr_backend", "openxr", CVAR_NONE}; // not saved: "mock" is for testing sessions

namespace
{

// Defaults changed while configs saved every archived cvar (configVersion 217 and before: a new default never reached
// an existing player). A config still holding a setting's old default takes the new one, once: vr_cfg_version records
// the changes a config has seen. A setting the player changed is left alone.
// FROZEN at 218: configs are sparse since (VR_ConfigSaves: a vr_ setting's line only when it differs from its default),
// so a changed default reaches every player who left the setting alone with no row. These rows stay for the configs
// written before 218; a new row only to override a player's own choice on purpose (CODE_STYLE.md, "Settings and the
// config"). The rows up to 126 (1.0.1's version: 123 the loading notice, 125 the knocked body's launch, 126 his 1.0.1
// values) were dropped with the cut at 1.0.1 (2026-11-08): an older config is reset (oldestMigrated).
struct DefaultChange
{
    int version;
    cvar_t* var;
    const char* before;
};

const DefaultChange defaultChanges[] = {
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
    {133, &vr_melee_phase_time, "0.35"},             // 0.2
    {133, &vr_foegrab_lift_time, "2"},               // 3.5
    {133, &vr_foegrab_necksnap_angle, "30"},         // 80
    {133, &vr_foegrab_necksnap_window, "0.3"},       // 0.2
    {133, &vr_foegrab_necksnap_neck, "3"},           // 1.5
    {133, &vr_reload_port_sshot_radius, "3"},        // 3.15
    {133, &vr_reload_reinsert_clearance, "4"},       // 1.5
    {133, &vr_decal_max, "768"},                     // 512
    {133, &vr_decal_bucket_max, "32"},               // 24
    {133, &vr_body_bracers, "1"},                    // 0
    {133, &vr_pbr_strength, "1"},                    // 2.5
    // (134 and 157: vr_foegrab_lift_classes's, retired for the monster groups' Lift.)
    // (135: the biosuit glow 1 -> 2, later back to 1 with no row: its rows did nothing, dropped; 216 moves a 2 to 1.)
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
    {139, &vr_gore_dents_depth, "1"},                // 3
    {139, &vr_gore_dents_max, "8"},                  // 12
    {139, &vr_gore_dents_size, "1"},                 // 1.25
    {139, &vr_gore_streams_width, "0.35"},           // 0.75
    {139, &vr_ragimpact_crush_speed, "200"},         // 350
    {139, &vr_teeth_per_enemy, "10"},                // 8
    // 140: his knockdown values (2026-10-16: "make my five knockdown values the defaults").
    {140, &vr_knockdown_chance, "1"},               // 0.6
    {140, &vr_knockdown_getup_speed, "1"},          // 0.8
    {140, &vr_knockdown_time_min, "2.5"},           // 1.75
    {140, &vr_knockdown_time_max, "4.5"},           // 3.5
    // 141: his ragdoll hand-follow value (2026-10-16: "I like if the hand follows the ragdoll more accurately").
    // 142: one throw for props and weapons (2026-10-16, R2 review #13; ROUND22.md, "One throw for props and weapons"):
    // one two-handed window (vr_carry_two_hands_window was the props', 0.1; the weapons' a hard-coded 0.3) and one share
    // of the thrower's velocity (vr_carry_throw_inherit was the props', 1; the weapons' all of it). The old names set
    // the new settings (cvarAliases) as an old config is executed; at their old defaults they take the new ones (the
    // window is a tuning constant now, vr_throw_2h_window: its old name is retired, an old line ignored).
    {142, &vr_throw_inherit, "1"},                  // 0.9
    // 143: a jump from the same level crushes (2026-10-16: "jump from no extra height and still damage and pop the
    // limbs ... one or two jumps to kill a grunt by jumping on his head"); a hit keeps one down at least this after it.
    {143, &vr_knockdown_hit_time, "0.5"},           // 1
    {143, &vr_ragimpact_crush_speed, "350"},        // 180
    // 144: the menu panel grip drag reaches farther (2026-10-17: "push the menu away too").
    // 145: the magnetic holsters' tether the default chain (2026-10-17: "the nicer one").
    {145, &vr_magholster_style, "0"},               // 1
    // 146: the leg sweep (2026-10-17): short weapons and props sweep too ("pretty much all of them should sweep"), and
    // his fall values ("direction and fall basically perfect (I tweaked some fall values)").
    // 147: the firing range's kinds' limbs made at its load, not in play (ROUND22.md, "Mid-game model loads"): ten
    // ~8 ms frames for each kind a dispenser or a dummy first makes.
    {147, &vr_limbs_prebuild, "1"},                 // 2
    // 148: more of his values (2026-10-17: "I've also tweaked more settings (e.g. leg sweep values), make those the
    // defaults too"; ROUND22.md, "His settings, third pass"). A config at the old defaults takes them.
    {148, &vr_decal_carve_depth, "1"},       // 0.75
    {148, &vr_decal_carve_max, "256"},       // 128
    {148, &vr_decal_carve_rim, "1"},         // 1.25
    {148, &vr_dummy_gib, "1"},               // 0
    {148, &vr_knockdown_keep_max, "6.5"},    // 7
    {148, &vr_mirror_hide_hud_text, "0"},    // 1
    {148, &vr_relight_reload, "1"},          // 0
    // 149: his holster zones, positions, sizes and depths (2026-10-17: "I've tweaked all the holster positions and
    // sizes and depths. Make all the values the new defaults"), and the shoulder holsters' reach.
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
    {150, &vr_body_blood, "1"},                         // 1.25
    {150, &vr_body_blood_amount, "1"},                  // 1.25
    {150, &vr_body_blood_mark_size, "1"},               // 1.5
    // 151: tracked legs on by default (2026-10-17: his value).
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
    {159, &vr_reload_port_widow_radius, "2.5"},
    // 160: his swing sounds and wet sheen (his config of 2026-10-19).
    {160, &vr_wounds_wet_coat, "1"},                    // 2
    // 160 too: the player's chainsaw a fifth stronger (his request, 2026-10-19).
    // 161: his flame wobble (his config of 2026-10-19); your AD crossbow's bolts 1.5 times the knights' (his notes).
    {161, &vr_flame_wobble_strength, "1"},              // 2.5
    // 162: his Widowmaker and AD crossbow numbers (his config of 2026-10-19).
    // 163: a projectile's kill pops a head or limb less often (his notes of 2026-10-19).
    // (164's monster group rows: namedDefaultChanges, the groups' values being data now, quakevr/data/groups.txt.)
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
    // 168: his shallower dents (his config of 2026-10-20).
    {168, &vr_gore_dents_depth, "3"},
    // 169: his Thrown Bodies tuning (his config of 2026-10-20); the tackle redone as the shoulder charge, off for now
    // (the old guard/shove tackle fired unintentionally), its bounce always (feedback).
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
    // 174: the shoulder charge tuned on the author's takes of 2026-10-21 (ROUND22.md, "Shoulder charge tuning"): his runs
    // with the head down were slower and his hands higher than the first guesses.
    // 175: the author's values (his config of 2026-10-21): a timed parry's reduction, the mantle's head height, one
    // hand's leg pull as strong as two; the struggle's classes: zombies struggle, the enforcer's, death knight's and
    // zombie's arms driven harder (their 5th field; ROUND22.md, "Struggle 3").
    {175, &vr_climb_mantle_height, "18"},          // 20
    {175, &vr_foegrab_legpull_onehand, "1.3"},     // 1
    // 176: a two-hand shove goes 1.25 times as far (the author's 2026-10-21 notes; its distance goes by the push's square).
    {176, &vr_shove_push_twohand, "1"}, // 1.12
    // 177: the author's Shoulder Charges (his config of 2026-10-22: "it feels better"): on (vr_kdtrig_tackle_on 1, retired
    // since: its chance 0 is off), knocked farther, gentler pose.
    // 178: the author's values (his config of 2026-10-22): bigger bloody footprints and handprints, the gore sounds'
    // kinds quieter (ROUND22.md, "Bloody prints 2; drip sounds").
    // 179: the author's values (his config of 2026-10-22): a body shoved over a ledge keeps more of its speed past the
    // edge (ROUND22.md, "Prop pushback, heavy throws, light ammo").
    {179, &vr_knockdown_shove_ledge_over, "0.1"}, // 0.45
    // 180: the author's value (his config of 2026-10-22): the pyro's dropped flamethrower burns harder (ROUND22.md,
    // "Gore hitches and flamethrower looks").
    // 181: the author's value (his config of 2026-10-22, approved): the fumigator's dropped poison sprayer's puffs hurt
    // more (ROUND22.md, "Leg-drag struggle for every humanoid; the sprayer's damage").
    // (181's vr_struggle_classes row: dropped at 190, the setting retired; mgroups::migrateStruggleClasses.)
    // 182: the author's values (his config of 2026-10-22: the new flamethrower and sprayer effects, the drip sounds).
    {182, &vr_adgun_ad_copies, "0"},      // 2
    {182, &vr_gore_sound_drips, "1"},     // 0.65
    // (183's vr_struggle_classes row: dropped at 190, the setting retired; mgroups::migrateStruggleClasses.)
    // 184: the author's values (his config of 2026-10-22 evening): his shoulder charge settings, more bloody footprints
    // (ROUND22.md, "Shoulder charge 3: body contact, run-up, standing tall").
    // 185: the author's value (his config of 2026-10-22, approved): more bloody handprints from a soaked hand (ROUND22.md,
    // "Item Pickup Mode; running legs; handprints").
    // 186: the flamethrowers' scorch marks on (cheap now: baked straight into the permanent layer; ROUND22.md, "Cheap
    // scorch marks").
    {186, &vr_adgun_fx_scorch, "0"}, // 1
    // 187: the rocket soldier's dropped launcher holds 4 rockets (the author's 2026-10-23 note; ROUND22.md, "Mini bosses").
    // 188: the author's values (his config of 2026-10-23, voice notes vrfiringrange_2026-10-23_01-16-39 and
    // vrtesthall_2026-10-23_01-47-31): the death menu later, the seated hip holsters a little out (ROUND22.md, "Death
    // menu delay, Vomit Mode, Holster Calibration on VR Settings, smoothed view text, bullet time's empty notice").
    {188, &vr_death_menu_delay, "2"},      // 5
    {188, &vr_hip_holster_seated_y, "-3"}, // 1.25
    // 189: the shoulder charge with the hands a bit above the chest (his takes of 2026-10-23: the hands low, the head
    // down less, side-on as the two meet) and his run-up (ROUND22.md, "Shoulder charge: hands above the chest").
    {189, &vr_kdtrig_tackle_runup, "1.5"}, // 0.5
    // 191: the author's shoulder charge values (his config of 2026-10-23): more lenient, a little less tall (ROUND22.md,
    // "Charge defaults, struggling humanoids, AD's flames in water, armed grunts' monster swords").
    // 192: the relief's depth write off by default, on in the Ultra preset (his choice: it cost 1.3 ms of an AD fight's
    // 3.3 ms eye frame even split; ROUND22.md, "Parallax depth write's cost").
    {192, &vr_parallax_depth_write, "1"}, // 0
    // 193: the author's water foam (his config of 2026-10-23; ROUND22.md, "Shoreline foam: per liquid, breathing,
    // particles"): fainter, finer, reaching further. Water's own now (lava and slime have their own vr_lava_foam_*,
    // vr_slime_foam_*).
    {193, &vr_water_foam_opacity, "1"},   // 0.5
    // 194: the death sergeant's flail heavier (the author's note vrfiringrange_2026-10-23_17-33-56): its head 3 kg, its
    // blows at most 120 (with its new least damage, speed limit and drag; ROUND22.md, "The flail: heavier, smaller").
    // 195: crates need two strong hits (his note: one hard punch or swing broke one; ROUND22.md, "Crates: two strong
    // hits"): a blow breaks a small crate from 18 m/s (with the new vr_prop_blow_wear 0.25, two strong ones do).
    {195, &vr_prop_break_speed, "9"}, // 18
    // 196: the author's shoreline foam (his config of 2026-10-23, notes start_2026-10-23_21-30-29,
    // vrfiringrange_2026-10-23_21-28-18, e1m1_2026-10-23_21-33-57; ROUND22.md, "Shoreline foam: at the shore's edge"):
    // every liquid breathing more and faster, more and bigger particles, slime's scum greener and reaching further.
    {196, &vr_water_foam_particles, "1"},          // 2
    {196, &vr_lava_foam_particles, "0.6"},         // 1.5
    {196, &vr_slime_foam_particles, "0.6"},        // 1.5
    // 197: AD's metal and stone breakables as before 195 (his note: metal took ~8 axe blows; ROUND22.md, "The blade's
    // start; the axe's head box", Breakables): a 200-health metal piece 3-4 full axe blows, a rubble wall one.
    {197, &vr_prop_break_metal, "1.5"}, // 1
    {197, &vr_prop_break_stone, "1.5"}, // 0.75
    // 198: the author's fist blow line (his config of 2026-10-24, edited with Edit Blade Shape; ROUND22.md, "His blade
    // shapes and fist line as defaults"; Misc/quakevr/blade_defaults.py writes these rows).
    {198, &vr_melee_fist_from_x, "0"}, // -0.34
    {198, &vr_melee_fist_from_y, "0"}, // 0.5039
    {198, &vr_melee_fist_from_z, "0"}, // -1.0026
    {198, &vr_melee_fist_to_x, "4"},   // -0.4123
    {198, &vr_melee_fist_to_y, "0"},   // 0.7656
    {198, &vr_melee_fist_to_z, "0"},   // -4.2209
    {198, &vr_melee_fist_width, "1"},  // 0.291
    // 199: the tug's least pull 200 -> 270 N (a still hold read up to 211 N and tore the gun out; ROUND22.md, "Tug least pull").
    // 202: logging off by default (the author's decision of 2026-10-25, cvar audit A1.4: vr_gl_markers 1 -> 0 too, not
    // archived), and the author's values (his config of 2026-10-25 17:50, approved 2026-10-25; ROUND22.md, "His config's
    // tuning as defaults"): the flail and the shoreline foam as he tuned them after 194 and 196, the fuel tank, bullet
    // time, the struggle's look, the ammo pouch's wobble.
    {202, &vr_memstats_log, "60"},                  // 0
    {202, &vr_xr_log_timing, "1"},                  // 0
    {202, &vr_flail_chain, "0.45"},                 // 0.6
    {202, &vr_bullettime_duration, "20"},           // 15
    {202, &vr_struggle_look, "0.9"},                // 1
    {202, &vr_struggle_look_strength, "0.6"},       // 0.8
    // 204: Model Animation Smooth by default (approved 2026-10-25; ROUND22.md, "Defaults of 2026-10-25"): a config
    // still at Quake's Blend (1) takes Smooth (3); Stepped (0) stays.
    {204, &r_lerpmodels, "1"}, // 3
    // 206: Ambient Light 0.1 by default (his config, approved 2026-10-26): a config still at 0 takes it; any other
    // value stays.
    {206, &vr_ambient_light, "0"}, // 0.1
    // 207 (the flare gun on crates off by default for beta4): gone with its setting, a tuning constant since 2026-11-03.
    // 208: Soften Flickering Lights off by default again (round 22, "AD flicker": AD's flicker was a NaN fog frame, not
    // its lamps; beta4 shipped 0.5): a config still at 0.5 takes 0, any other value stays.
    {208, &vr_lightstyle_comfort, "0.5"}, // 0
    // 209: his new weapons' tuning (his config of 2026-10-27, NOTES.md vrragtriggers_2026-10-27_02-01-22: "make all of
    // them the new defaults"): the heavy revolver's damage, flicks, ejector rod, spin and hand reach; the nail pistol's
    // well. And the revolver's swipe a little more deliberate (spun by accident while loading). A config still at the
    // old default takes the new; any other value stays.
    {209, &vr_revolver_open_flick_speed, "350"},     // 400
    {209, &vr_revolver_open_flick_angle, "30"},      // 120
    {209, &vr_revolver_close_flick_speed, "300"},    // 350
    {209, &vr_revolver_close_flick_angle, "25"},     // 75
    {209, &vr_revolver_eject_rod_reach, "2.5"},      // 1
    {209, &vr_revolver_eject_rod_speed, "0.3"},      // 0.35
    {209, &vr_revolver_spin_speed, "0.5"},           // 0.8
    {214, &vr_agrunt_shove, "2"},                    // 1 (guns only; it shipped with 2 for a few hours)
    {215, &vr_nailpistol_speed, "2600"},             // 3070 (nails 18% faster)
    // 216: rows that did nothing (135's suit glow, 146's topple: their defaults went back without a row) dropped, and
    // a config still at those passing values takes today's (round 23, his decision D2).
    {209, &vr_reload_port_npistol_z, "0"},           // 1.5
    {209, &vr_reload_port_npistol_radius, "1.5"},    // 1
};
// Default changes of settings with no cvar of their own to point at (the held objects' slots, vr_props.cpp): by name.
// Frozen at 218 as defaultChanges is (sparse configs).
struct NamedDefaultChange
{
    int version;
    const char* name;
    const char* before;
};
const NamedDefaultChange namedDefaultChanges[] = {
    // 164: monster groups: bosses can't be shoved, machines aren't knocked down by shoves, a skull wizard has no legs
    // (the groups' values: quakevr/data/groups.txt).
    {164, "vr_mgrp_boss_push", "0"},           // 1
    {164, "vr_knockdown_chance_machine", "0"}, // 0.3
    {164, "vr_mgrp_skullwiz_sweep", "0"},      // 1
    {164, "vr_mgrp_skullwiz_leggrab", "0"},    // 1
    // 179: the nailgun magazine's fitted fingers sink less into it (his config of 2026-10-22: the pinky showed through).
    {179, "vr_prop_overlap_51", "0.4"}, // 0.385
    // 179: the shells and magazines thrown deal a quarter of a thing's damage (a magazine broke a crate in two throws).
    {179, "vr_prop_throw_dmg_49", "1"}, // 0.25
    {179, "vr_prop_throw_dmg_50", "1"},
    {179, "vr_prop_throw_dmg_51", "1"},
    {179, "vr_prop_throw_dmg_52", "1"},
    {179, "vr_prop_throw_dmg_53", "1"},
    // 202: the author's values (his config of 2026-10-25 17:50, approved 2026-10-25): the ogre's Parry Poise; the
    // zombie's head held smaller; the nailgun magazine's grip, in both hands; the nail pistol magazine's size, In the
    // Palm (its Grip X..Z 0), its handle tilt. (After props::takeShippedSlots: a model of another slot in slot 56 has
    // moved out first, with its own values.)
    {202, "vr_mgrp_ogre_poise", "1.6"},    // 1.4
    {202, "vr_prop_size_45", "1"},         // 0.8
    {202, "vr_prop_grip_x_51", "-2.13"},   // -2.1
    {202, "vr_prop_grip_z_51", "-3.05"},   // -3.35
    {202, "vr_prop_two_hands_51", "0"},    // 1
    {202, "vr_prop_size_57", "0.52"},      // 0.55
    {202, "vr_prop_grip_57", "1"},         // 2
    {202, "vr_prop_grip_x_57", "-1.8"},    // 0
    {202, "vr_prop_grip_y_57", "0.05"},    // 0
    {202, "vr_prop_grip_z_57", "-2.6"},    // 0
    {202, "vr_prop_handle_tilt_57", "25"}, // 30.5
    // 204: AD's heads weigh as every other monster's head (Mass -1, a share of their ragdoll's; approved 2026-10-25):
    // were estimated (0). After props::takeShippedSlots, as 202's: a head brought from another slot comes with its 0.
    {204, "vr_prop_mass_33", "0"}, // -1 h_dknight
    {204, "vr_prop_mass_58", "0"}, // -1 h_dcrossbow
    {204, "vr_prop_mass_59", "0"}, // -1 h_soldier
    {204, "vr_prop_mass_60", "0"}, // -1 h_enforcer
    {204, "vr_prop_mass_62", "0"}, // -1 h_hogreham
    {204, "vr_prop_mass_64", "0"}, // -1 h_dguard
    {204, "vr_prop_mass_71", "0"}, // -1 h_hogre
    // 209: the nail pistol magazine's grip his (his config of 2026-10-27): Grip Mode 1 at -3.4 -0.5 -3.3, overlap 0.37.
    {209, "vr_prop_grip_57", "2"},       // 1
    {209, "vr_prop_grip_x_57", "0"},     // -3.4
    {209, "vr_prop_grip_y_57", "0"},     // -0.5
    {209, "vr_prop_grip_z_57", "0"},     // -3.3
    {209, "vr_prop_overlap_57", "0.385"}, // 0.37
};
// (200: the nail pistol's magazine's prop slot 56; 201: a shipped prop slot another model had taken moves that model to
// another slot, so that the shipped one's tuning reaches its model: props::takeShippedSlots; 202: AD's heads' slots,
// and the armed grunts' archived pose keys and grips equal to the new shipped ones dropped: agpose::dropShippedEqual,
// aggrip::dropDefaultEqual; 204: r_lerpmodels 1 -> 3, AD's heads Mass 0 -> -1; 205: vr_heat_haze_half 1 -> 0 unless
// a graphics preset below High; 206: vr_ambient_light 0 -> 0.1; 207: vr_crate_flaregun 0.05 -> 0; 208:
// vr_lightstyle_comfort 0.5 -> 0; 209: his new weapons' tuning of 2026-10-27, the revolver's swipe 0.5 -> 0.8 m/s;
// 210: his armed grunts' pose keys of 2026-10-27 shipped: the archived keys equal to them dropped, as at 202; 211: the
// gun pose categories: the pose keys' code wider (an enforcer's 1xxxx now 10xxxx) and the archived rifle keys laid on
// every category, agpose::migrateCategories; 212: his pose keys of 2026-10-27 evening shipped (the rifle Aim, the
// Pistol, Launcher, Sprayer and Laser Cannon keys): the archived keys equal to them dropped, after 211's migration)
// 214: vr_agrunt_shove 2 -> 1 (only humanoids with guns shove by default)
// 215: vr_nailpistol_speed 2600 -> 3070 (round 23: every nail 18% faster; the nailgun's vr_nail_speed new, 2065)
// 216: vr_suit_glow(_light) 2 -> 1, vr_legsweep_topple 180 -> 200 (the no-op rows 135, 146 and 169 dropped)
// 217: vr_agarms_clavicle 0.35 -> 0.6 (vr_rig2_clavicle merged into it at the value both armed rigs used: refactor C1)
// 218: sparse configs (Cvar_WriteVariables writes "vr_config_defaults", then only the vr_ settings changed from their
// defaults: VR_ConfigSaves). The last version: the rows above are frozen, a changed default needs none from now on.
// 219: a vr_seated_height at or over 0.9 of the standing eye height (vr_height_calibration) back to 0 (not measured):
// measured standing, no seated height (his config of 2026-11-02: 1.574 seated, 1.561 standing).
// 220: vr_body_step_rate's meaning changed (a cap that rarely counted -> the steps a second at a full run): the old number
// can't be converted, so whatever was saved goes back to the default, the player's own choice too; vr_body_step_run to
// its new range, 0 .. 1.
// 221: the worn pack's one fit (vr_worn_backpack_x, _y, _z, _scale, _pitch, _yaw, _roll, retired) split per kind: a config line's
// value goes on each kind's slider once (the death knight's Up with the 6 his pack always sat higher: the look kept).
// 222: the 16 weapon-store keys nothing reads (vr_wofs_fgr_open/fgr_thumb/index/middle/ring/pinky/fgr_x/_y/_z,
// 2h_fgr_open, 2h_fgr_thumb, 2h_blade, length, 2h_fxd_mh_ox/_oy/_oz: round 21's fitted hands, vr_grasp.cpp) are out of
// the store; their stand-ins are gone, so an old config's lines for them are dropped by VR_RetiredCvar (weapons::
// removedKeyLine) rather than printed as unknown commands. Nothing to convert: nothing read their values.
// 223: vr_prone_crawl_plant_height's meaning changed (the controller's place over the feet, a pull on under twice it ->
// the hand's lowest point, the knuckles or fingers, a pull on 1.5 over it): whatever was saved goes back to the default
// (his 4 of 2026-11-08 too: "I can sort of swim in the air", his note vrtesthall_2026-11-08_16-54-47).
constexpr int configVersion = 223;
// 190: vr_struggle_classes retired for the monster groups' Struggle settings: the value of its line in the config just
// executed (VR_RetiredCvar keeps it), moved into the groups by mgroups::migrateStruggleClasses if it was custom.
za::String retiredStruggleClasses;
bool retiredStruggleSeen = false;
// 203: Gun Model Pitch (vr_gunmodelpitch, retired 2026-11-02) moved into Hand Pitch: its line's value in the config just
// executed (VR_RetiredCvar keeps it; its default 7 when there is none).
float retiredGunModelPitch = weapons::gunModelPitchBase;
// 221: the worn pack's one fit: its config lines' values (VR_RetiredCvar keeps them), each kind's slider, what the death knight adds.
struct RetiredPackFit
{
    const char* name;
    cvar_t& grunt;
    cvar_t& knight;
    cvar_t& dknight;
    float dknightAdds;
    bool seen;
    float value;
};
RetiredPackFit retiredPackFits[] = {
    {"vr_worn_backpack_x", vr_worn_backpack_grunt_x, vr_worn_backpack_knight_x, vr_worn_backpack_dknight_x, 0.f, false, 0.f},
    {"vr_worn_backpack_y", vr_worn_backpack_grunt_y, vr_worn_backpack_knight_y, vr_worn_backpack_dknight_y, 0.f, false, 0.f},
    {"vr_worn_backpack_z", vr_worn_backpack_grunt_z, vr_worn_backpack_knight_z, vr_worn_backpack_dknight_z, 6.f, false, 0.f},
    {"vr_worn_backpack_scale", vr_worn_backpack_grunt_scale, vr_worn_backpack_knight_scale, vr_worn_backpack_dknight_scale, 0.f, false, 0.f},
    {"vr_worn_backpack_pitch", vr_worn_backpack_grunt_pitch, vr_worn_backpack_knight_pitch, vr_worn_backpack_dknight_pitch, 0.f, false, 0.f},
    {"vr_worn_backpack_yaw", vr_worn_backpack_grunt_yaw, vr_worn_backpack_knight_yaw, vr_worn_backpack_dknight_yaw, 0.f, false, 0.f},
    {"vr_worn_backpack_roll", vr_worn_backpack_grunt_roll, vr_worn_backpack_knight_roll, vr_worn_backpack_dknight_roll, 0.f, false, 0.f},
};

// Defaults changed after the last full configs (configVersion 217, 1.0.x and every 1.1 beta), with each one's value
// then. defaultChanges is frozen at 218 because a sparse config's line is the player's choice; but a full config holds
// every setting, so without these a player upgrading from 1.0.1 or a beta kept the 217 defaults (armed replacements off,
// the relight's old ambient...). Applied only to a full config (vr_cfg_version < sparseConfigVersion), to a setting still
// at its 217 value. (Release pass 2026-11-08: the defaults of 155989a1d^ against today's; vr_body_step_rate is 220's
// own step in migrateConfig.) A default changed later, of a setting a 217 config already held, needs its row here too
// (`python scratch/diff217.py`-style: 155989a1d^'s vr_cvars.inc against today's).
// Independent of defaultChanges' rows (those move what changed up to 217; these only what changed after), so the cut
// at 1.0.1 (oldestMigrated 126) leaves them whole: they reach configs 126 to 217.
struct FullConfigChange
{
    cvar_t* var;
    const char* before;
};
const FullConfigChange fullConfigChanges[] = {
    {&vr_ad_upgrades, "1"},
    {&vr_agarmor_debris_time, "10"},
    {&vr_agarmor_rule, "0"},
    {&vr_agarmor_skill1_pieces, "2"},
    {&vr_agarmor_skill2_pieces, "5"},
    {&vr_agrunt_dual_chance, "0.25"},
    {&vr_agrunt_dual_pickup, "0"}, // Pick Up a Second on (next, 2026-11-08)
    {&vr_agrunt_fall_keep_impact, "4.5"},
    {&vr_agrunt_parry_disarm, "0.35"},
    {&vr_agrunt_repl, "0"}, // armed replacements on by default in 1.1 (his decision, 2026-11-06)
    {&vr_agrunt_repl_ad, "0"},
    {&vr_agrunt_repl_common, "0"},
    {&vr_agrunt_repl_dual, "0"},
    {&vr_agrunt_repl_gunaxe, "0"},
    {&vr_agrunt_repl_rare, "0"},
    {&vr_agrunt_repl_uncommon, "0"},
    {&vr_agrunt_strafe_turn, "28"},
    {&vr_agrunt_tug_break_force, "900"},
    {&vr_agrunt_tug_break_impulse, "350"},
    {&vr_agrunt_tug_let_go, "50"},
    {&vr_agrunt_tug_pull_time, "0.5"},
    {&vr_agrunt_tug_strength, "1.5"},
    {&vr_ammo_low_frac, "0.25"},
    {&vr_ammo_pouch_leg_follow, "0.5"},
    {&vr_deadzone_outer, "5"},
    {&vr_flat_key_grab_alt, "r"}, // R reloads in flat-screen play now (2026-11-08); a sparse config never held the old default
    {&vr_ghost_hands_fade_from, "6"},
    {&vr_relight_ao, "1.5"},
    {&vr_relight_bounce, "0"},
    {&vr_relight_minlight, "0"},
    {&vr_relight_reload, "0"},
    {&vr_revolver_eject_rod_reach, "1"},
    {&vr_revolver_eject_rod_speed, "0.35"},
    {&vr_revolver_spin_hold, "0.6"},
    {&vr_revolver_spin_speed, "0.8"},
    {&vr_water_foam_particles, "2"},
};

// 1.0.1's version (the author's decision of 2026-11-08: migration cut at 1.0.1): an older config is reset
// (resetOldConfig), not migrated; its changes' rows were dropped (122 was the cut until then, 1.0.0's).
constexpr int oldestMigrated = 126;

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

// Sparse configs (configVersion 218): the Quake VR settings (vr_*, the weapons' and props' slot tables too) are written
// only when changed from their defaults, after a "vr_config_defaults" line that sets the others to theirs. A line in the
// config is the player's choice, and a changed default reaches everyone else with no defaultChanges row. Ironwail's own
// settings (r_*, gl_*, ...) are all written, as before.
constexpr int sparseConfigVersion = 218;

[[nodiscard]] bool inSparseConfig(const cvar_t* var)
{
    return !ZA_STRNCMP(var->name, "vr_", 3);
}

// The version markers (always written): a missing one would read as its default, which a later version can pass.
[[nodiscard]] bool alwaysSaved(const cvar_t* var)
{
    return var == &vr_cfg_version || var == &vr_wofs_version || var == &vr_props_version || var == &vr_bindings_version;
}

// "vr_config_defaults" (a sparse config's first settings line): every Quake VR setting back to its default, so the lines
// after it give the config's settings as a full config did (a game switch, a config executed again: none of the
// previous values left over).
void configDefaults_f()
{
    for(cvar_t* var = Cvar_FindVarAfter("", CVAR_ARCHIVE); var; var = Cvar_FindVarAfter(var->name, CVAR_ARCHIVE))
    {
        if(inSparseConfig(var) && var->default_string && !(var->flags & (CVAR_ROM | CVAR_LOCKED)) &&
           !sameValue(var->string, var->default_string))
        {
            Cvar_SetQuick(var, var->default_string);
        }
    }
    for(const za::Span<cvar_t> table : standInTables()) // (the weapons' and props' values: their data files')
    {
        for(cvar_t& var : table)
        {
            if(!sameValue(var.string, var.default_string))
            {
                Cvar_SetQuick(&var, var.default_string);
            }
        }
    }
}

// A config file's `name "value"` lines (as Cvar_WriteVariables writes them), in order: fn(name, value). False: no file.
template <typename Fn>
[[nodiscard]] bool forEachConfigLine(const char* path, Fn&& fn)
{
    FILE* f = Sys_fopen(path, "rb"); // (UTF-8 path: a non-ASCII game folder)
    if(!f)
    {
        return false;
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
            const za::String value{line.substrByPosLen(open + 1, close - open - 1)};
            fn(name, value);
        }
        line.clear();
    }
    fclose(f);
    return true;
}

// The archived settings a config file sets (a sparse config's vr_ settings without a line: their defaults).
[[nodiscard]] ankerl::unordered_dense::map<za::String, za::String> readConfigSettings(const char* path)
{
    ankerl::unordered_dense::map<za::String, za::String> settings;
    const bool read = forEachConfigLine(path,
        [&settings](const za::String& name, const za::String& value)
        {
            const cvar_t* var = Cvar_FindVar(name.cStr());
            if(var && (var->flags & CVAR_ARCHIVE))
            {
                settings[za::String{var->name}] = value; // (an old name: the new)
            }
        });
    if(!read)
    {
        return settings;
    }
    // A sparse config (218) has no line for a vr_ setting at its default: it sets that default (vr_config_defaults).
    for(const cvar_t* var = Cvar_FindVarAfter("", CVAR_ARCHIVE); var; var = Cvar_FindVarAfter(var->name, CVAR_ARCHIVE))
    {
        if(inSparseConfig(var))
        {
            settings.try_emplace(za::String{var->name}, var->default_string ? var->default_string : "");
        }
    }
    for(const za::Span<cvar_t> table : standInTables()) // (the data tables' values: a line only when changed)
    {
        for(const cvar_t& var : table)
        {
            settings.try_emplace(za::String{var.name}, var.default_string);
        }
    }
    return settings;
}

[[nodiscard]] ankerl::unordered_dense::map<za::String, za::String> archivedSettings()
{
    ankerl::unordered_dense::map<za::String, za::String> settings;
    for(const cvar_t* var = Cvar_FindVarAfter("", CVAR_ARCHIVE); var; var = Cvar_FindVarAfter(var->name, CVAR_ARCHIVE))
    {
        settings[var->name] = var->string;
    }
    for(const za::Span<cvar_t> table : standInTables())
    {
        for(const cvar_t& var : table)
        {
            settings[var.name] = var.string;
        }
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

// A config older than 1.0.1 (vr_cfg_version < 126; the author's decisions of 2026-10-16, "ignore compatibility with any
// pre-1.0.0 configs", and of 2026-11-08, the cut moved to 1.0.1): its Quake VR settings go back to the shipped defaults instead of being migrated, except the
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
    Con_Printf("Quake VR: this config is older than 1.0.1 (version %d): its VR settings are reset to the defaults\n", from);
}

// Right after the saved config is executed (Cmd_Exec_f queues it).
void migrateConfig_f()
{
    // (the version it was saved with: migrateConfig moves it on)
    const bool sparse = Cmd_Argc() == 1 && static_cast<int>(vr_cfg_version.value) >= sparseConfigVersion;
    migrateConfig();
    const char* path = va("%s/%s", com_gamedir, CONFIG_NAME);
    trackConfig(path);
    if(sparse)
    {
        tunablesTakeConfigLines(path); // (an older config's lines are mostly old defaults: not the player's)
    }
    tunablesLoad(); // tunables.cfg, after the config (vr_tunables.cpp)
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
    if(from < sparseConfigVersion)
    {
        for(const FullConfigChange& c : fullConfigChanges)
        {
            if(sameValue(c.var->string, c.before))
            {
                Con_DPrintf("VR: %s: new default %s (was %s)\n", c.var->name, c.var->default_string, c.before);
                Cvar_SetQuick(c.var, c.var->default_string);
            }
        }
    }
    if(from < 190 && retiredStruggleSeen)
    {
        mgroups::migrateStruggleClasses(retiredStruggleClasses.cStr()); // (a custom list into the groups' Struggle)
    }
    retiredStruggleSeen = false;
    props::takeShippedSlots(from); // (the held objects' slots shipped since: 183 AD's health vial)
    // (after the slots: a shipped slot's rows meet its own model's values, not those of a model that had taken it)
    for(const NamedDefaultChange& c : namedDefaultChanges)
    {
        cvar_t* var = Cvar_FindVar(c.name);
        if(var && c.version > from && sameValue(var->string, c.before))
        {
            Con_DPrintf("VR: %s: new default %s (was %s)\n", var->name, var->default_string, c.before);
            Cvar_SetQuick(var, var->default_string);
        }
    }
    if(from < 210)
    {
        // (202, and 210: his keys and grips shipped; an archived one equal to its new default no longer kept)
        const int poses = agpose::dropShippedEqual();
        const int grips = aggrip::dropDefaultEqual();
        Con_DPrintf("VR: armed grunts: %d pose keys and %d grips equal to the new defaults dropped\n", poses, grips);
    }
    if(from < 211)
    {
        int widened = 0;
        const int laid = agpose::migrateCategories(&widened);
        Con_DPrintf("VR: armed grunts' pose keys: %d in the wider code, %d rifle keys laid on the gun categories\n", widened,
            laid);
    }
    if(from < 212)
    {
        // (212: his keys of 2026-10-27 evening shipped; after 211's: the codes the new ones, the categories laid on. The
        // off-hand parts of 2026-10-27 need none: an entry without them reads them as zero, Auto)
        const int poses = agpose::dropShippedEqual();
        Con_DPrintf("VR: armed grunts: %d pose keys equal to the new defaults dropped\n", poses);
    }
    if(from < 213)
    {
        // (213: his keys of 2026-10-27 night shipped (the enforcer's fists, one-handed melee, two guns, pistol, launcher,
        // sprayer, laser cannon, Widowmaker); an archived key equal to its new default no longer kept)
        const int poses = agpose::dropShippedEqual();
        Con_DPrintf("VR: armed grunts: %d pose keys equal to the new defaults dropped\n", poses);
    }
    if(from < 203 && retiredGunModelPitch != weapons::gunModelPitchBase)
    {
        // configVersion 203: Gun Model Pitch lost its menu row (Hand Pitch turns both hands, the guns and the aim
        // together, as it did). What it was past 7 goes into both hands' pitch, subtracted (measured with vr_aim_check: Gun Model
        // Pitch +10 aims where vr_gunangle/vr_offhandpitch -10 do; Hand Pitch adds to both, a higher one aims lower).
        const float extra = retiredGunModelPitch - weapons::gunModelPitchBase;
        Cvar_SetValueQuick(&vr_gunangle, vr_gunangle.value - extra);
        Cvar_SetValueQuick(&vr_offhandpitch, vr_offhandpitch.value - extra);
        Con_DPrintf("VR: Gun Model Pitch %g moved into Hand Pitch (-%g)\n", retiredGunModelPitch, extra);
    }
    retiredGunModelPitch = weapons::gunModelPitchBase;
    if(from < 205 && sameValue(vr_heat_haze_half.string, "1"))
    {
        // configVersion 205: Heat Haze Detail full size by default, as High and Ultra set it; the presets below High half
        // size (his notes 2026-10-26). A config at the old default 1 that is one of those presets as it is (with the 1:
        // their row) keeps it; any other (High, Ultra, its own mix) takes the new default.
        const int level = gfxpresets::matchingLevel();
        if(level < 0 || level >= gfxpresets::levelHigh)
        {
            Cvar_SetQuick(&vr_heat_haze_half, vr_heat_haze_half.default_string);
        }
        Con_DPrintf("VR: vr_heat_haze_half: %s (graphics preset %d)\n", vr_heat_haze_half.string, level);
    }
    if(from < 219 && vr_seated_height.value > 0.f &&
        vr_seated_height.value >= seated::MAX_SHARE_OF_STANDING * vr_height_calibration.value)
    {
        Con_Printf("VR: Seated Height %.2f m is not a seated one (standing %.2f m): back to not measured\n",
            vr_seated_height.value, vr_height_calibration.value);
        Cvar_SetValueQuick(&vr_seated_height, 0.f);
    }
    if(from < 220)
    {
        Con_DPrintf("VR: vr_body_step_rate: %s for %s (its meaning changed)\n", vr_body_step_rate.default_string,
            vr_body_step_rate.string);
        Cvar_SetQuick(&vr_body_step_rate, vr_body_step_rate.default_string);
        const float run = CLAMP(0.f, vr_body_step_run.value, 1.f);
        if(run != vr_body_step_run.value)
        {
            Cvar_SetValueQuick(&vr_body_step_run, run);
        }
    }
    if(from < 221)
    {
        for(RetiredPackFit& r : retiredPackFits)
        {
            if(r.seen)
            {
                Cvar_SetValueQuick(&r.grunt, r.value);
                Cvar_SetValueQuick(&r.knight, r.value);
                Cvar_SetValueQuick(&r.dknight, r.value + r.dknightAdds);
                Con_DPrintf("VR: %s %g on each kind's pack fit\n", r.name, r.value);
            }
        }
    }
    if(from < 223)
    {
        Con_DPrintf("VR: vr_prone_crawl_plant_height: %s for %s (its meaning changed)\n",
            vr_prone_crawl_plant_height.default_string, vr_prone_crawl_plant_height.string);
        Cvar_SetQuick(&vr_prone_crawl_plant_height, vr_prone_crawl_plant_height.default_string);
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
    return var == &vr_cfg_version || var == &vr_installer_applied || var == &vr_tutorial_started|| var == &vr_bindings_version || var == &vr_wofs_version || var == &vr_height_calibration
        || var == &vr_seated_height // (one's chair)
        || var == &vr_props_version || var == &vr_menu_positions
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

// A setting that became a tuning constant: a line in a sparse config is the player's value (sparseConfigVersion), and it
// would go at the next write (the name is no setting now): a development build keeps it as the constant's, in
// tunables.cfg, where tune_promote.py and make_release.ps1's gate see it. Read after the config ran (which ignored the
// line: VR_RetiredCvar), not from the line itself: a test script or a console command sets nothing this way.
void tunablesTakeConfigLines(const char* path)
{
    (void)forEachConfigLine(path,
        [](const za::String& name, const za::String& value)
        {
            if(!Cvar_FindVar(name.cStr()))
            {
                tunableTakeConfigLine(name.cStr(), value.cStr());
            }
        });
}

bool sameSettingValue(const char* a, const char* b)
{
    return sameValue(a, b);
}

// "vr_installer_settings" (quake.rc, right after the saved config): runs installer_settings.cfg, the settings picked on
// Setup's settings page (docs/vr-port/INSTALLER.md, "Settings page"), once. Its "// stamp <id>" line is kept in
// vr_installer_applied (saved with the config), so it runs again only when Setup writes a new file. After the saved
// config, the page's picks win over it once; before quakevr.cfg, the forced rules still win; a later change in the
// game is saved over it and the file is not run again. Setup writes only what was changed on its page.
void installerSettings_f()
{
    byte* file = COM_LoadMallocFile("installer_settings.cfg", nullptr);
    if(!file)
    {
        return;
    }
    const za::StringView text{reinterpret_cast<const char*>(file)};
    constexpr za::StringView stampTag = "// stamp ";
    const size_t at = text.find(stampTag);
    za::String stamp;
    if(at != za::StringView::nPos)
    {
        size_t end = at + stampTag.size();
        while(end < text.size() && text[end] != '\n' && text[end] != '\r' && text[end] != ' ')
        {
            end++;
        }
        stamp = za::String{text.substrByPosLen(at + stampTag.size(), end - at - stampTag.size())};
    }
    if(stamp.empty())
    {
        Con_Printf("installer_settings.cfg: no \"// stamp\" line, not run\n");
    }
    else if(stamp == za::StringView{vr_installer_applied.string})
    {
        Con_DPrintf("installer_settings.cfg: stamp %s already applied\n", stamp.cStr());
    }
    else
    {
        // (Inserted: it runs now, before the rest of quake.rc.)
        za::String run{"\n"};
        run.append(text);
        run += va("\nvr_installer_applied \"%s\"\necho \"installer settings applied: stamp %s\"\n", stamp.cStr(), stamp.cStr());
        Cbuf_InsertText(run.cStr());
    }
    VR_HeapFree(file);
}

void registerCvars()
{
#define QVR_CVAR(name, def, flags) Cvar_RegisterVariable(&name);
#include "vr_cvars.inc"
#undef QVR_CVAR
    tunablesInit(); // (vr_tunables.cpp)

    Cvar_RegisterVariable(&vr_backend);
    Cmd_AddCommand("vr_migrate_config", migrateConfig_f);
    Cmd_AddCommand("vr_installer_settings", installerSettings_f);
    Cmd_AddCommand("vr_menu_body_clone_info", bodyclone::info_f);
    Cmd_AddCommand("vr_default", default_f);
    Cmd_AddCommand("vr_savedefaults", saveDefaults_f);
    Cmd_AddCommand("vr_config_defaults", configDefaults_f);
    registerConfigDiag(); // vr_config_test (vr_config_diag.cpp)
}

} // namespace qvr

// Host_WriteConfigurationToFile, before the game folder's config is written over: the settings another copy of the
// game changed in it since this copy read or wrote it, and this copy left alone, are taken (configTrack).
// Settings removed (docs/vr-port/CVAR_AUDIT.md): a config, a shipped .cfg or a take that still sets one is not an
// "Unknown command" (cmd.c asks here); the line is dropped quietly and the next config write leaves it out. Names gone
// before 1.0.1 (in its own list; no 1.0.1 config has them) went with the migration's cut at 1.0.1 (2026-11-08).
constexpr const char* retiredCvars[] = {
    // 2026-10-16: the props' two-handed throw window, now the tuning constant vr_throw_2h_window (0.2; no cvar to alias)
    "vr_carry_two_hands_window",
    // Removed without a line here (a beta 4 config printed "Unknown command" for them; release pass 2026-11-08):
    "vr_walltorch_pull",      // 2026-10-31: a wall torch's pull (the grab query's weapon tug took it)
    "vr_revolver_eject_tilt", // 2026-11-03: the revolver ejects by a push of the hand, not a tilt
    // 2026-11-08: split into _along and _across (and a rocket kind)
    "vr_proj_inherit_fast",
    "vr_proj_inherit_lobbed",
    "vr_proj_inherit_spray",
    // 2026-11-08: switches whose other side was retired (cruft_report H2, H3, H7): the default kept.
    "vr_gunsfx_punch",
    "vr_agrunt_aim_converge",
    "vr_agrunt_strike_first",
    "vr_knockdown_weld",
    "vr_pickup_float_grace",
    "vr_body_step_sounds",
    "vr_agarms_twist_share",
    "vr_agbody_torso_sides",
    "vr_test_dispenser",
    // 2026-11-07: A/B and compare switches of finished fixes (cruft_report S2-S4): the default kept, the other side
    // gone.
    "vr_agrunt_punch_ease",
    "vr_agrunt_lift_turn",
    "vr_agrunt_run_key_stance",
    "vr_agrunt_saw_reach",
    "vr_agrunt_swing_one_hand",
    "vr_agarms_lod_check",
    "vr_weapon_kick_cancel",
    "vr_flail_frame",
    "vr_flail_smooth",
    "vr_debug_ribbon_overlap",
    "vr_pose_solve",
    "vr_ragdoll_weapon_collide",
    "vr_ad_widowmaker_art",
    // 2026-11-07: finished perf A/B switches (the same results either way; cruft_report S1): their default kept, the
    // old path gone.
    "vr_place_entities_first",
    "vr_setup_screens_cache",
    "vr_load_prewarm",
    "vr_gun_pieces_ahead",
    "vr_oit_lazy_clear",
    "vr_body_prepass",
    "vr_alias_prepass",
    "vr_box3d_mesh_junctions",
    "vr_snd_reverb_beside",
    // 2026-11-05: the free hand's rest on a barrel, at a point of its own (FLATPLAY.md): the weapon's two-handed hotspot now
    // (vr_flat_stance). tunables.cfg's and old configs' lines stay quiet:
    "vr_flat_rest",
    "vr_flat_rest_along",
    "vr_flat_rest_below",
    "vr_flat_rest_relief",
    // 2026-11-03: the directional parry the only parry (ROUND23.md, "Directional parry: the only parry"): its switch, on since 2026-11-02.
    "vr_dirparry",
    // 2026-11-02: the armed death knight's share of casts with a gun, a cooldown now (vr_agdknight_cast_cooldown).
    "vr_agdknight_spell_share",
    // 2026-11-02: the option audit's removals (ROUND23.md, "Option audit: removals"): the hitbox category switches (the
    // width, vr_hull_ent_width, holds against all), the old sides of legacy switches (their "on" kept), the status
    // duplicates (vr_*_status 1), the retired migrations' cvars, Shoulder Charges' switch (its chance 0 is off), the
    // armed grunts' old fall rules (the impact rule kept) and the world marks as meshes (the world's shader kept).
    "vr_hull_monsters",
    "vr_hull_players",
    "vr_hull_boxes",
    "vr_climb_mantle_lenient",
    "vr_weapon_grab_drawn",
    "vr_particle_retro_fast",
    "vr_bloom_fast",
    "vr_hipnotic_available",
    "vr_rogue_available",
    "vr_tips_seen",
    "vr_gunmodelpitch", // (an old config's value moved into Hand Pitch by the migration to 203: retiredGunModelPitch)
    "vr_kdtrig_tackle_on", // (an old config's 0: vr_kdtrig_tackle 0)
    "vr_agrunt_fall_keep_window",
    "vr_agrunt_fall_keep_height",
    "vr_agrunt_fall_keep_speed",
    "vr_agrunt_fall_keep_fling",
    "vr_decals_world",
    "vr_font_shadow_size", // 2026-11-02: the shadow's offset, now vr_font_shadow_x and _y (vr_font_shadow_grow its size)
    "vr_worn_backpacks", // 2026-11-02: armed grunts always wear their backpack (ROUND23.md, "Worn backpacks always on")
    // 2026-11-02: option round 2's removals, their "on" kept (ROUND23.md, "Option round 2: removals"): armed bodies hit
    // as drawn, casings' sound per surface, precise hit detection, permanent decals, dequantised .mdl vertices.
    "vr_hit_drawn_pose",
    "vr_gunsfx_casings",
    "vr_hit_precise",
    "vr_permdecals",
    "vr_mdl_dequant",
    // 2026-11-02: legacy options removed, their defaults kept (ROUND23.md, "Legacy options removed, group 1").
    "vr_hand_rig",
    "vr_gun_wall_slide",
    "vr_box3d_hand_push_fist",
    "vr_ragdoll_active_getup_hold",
    "vr_menu_banner_beside",
    "vr_worn_backpack_tug",
    // 2026-11-03: the pack's fit split per kind (his note vrragtriggers_2026-11-03_02-21-38; configVersion 221 moves their values).
    "vr_worn_backpack_x",
    "vr_worn_backpack_y",
    "vr_worn_backpack_z",
    "vr_worn_backpack_scale",
    "vr_worn_backpack_pitch",
    "vr_worn_backpack_yaw",
    "vr_worn_backpack_roll",
    // 2026-11-02: a hand's pull only ever lifts a worn pack's wearer (ROUND23.md, "Lift only"); the tear by pulling and its
    // options went, and its two tunables, still used by the lift, are named for it now.
    "vr_worn_backpack_pull",
    "vr_worn_backpack_rip_force",
    "vr_worn_backpack_tug_stretch",
    "vr_worn_backpack_tug_time",
    "vr_worn_backpack_topple",
    "vr_worn_backpack_topple_push",
    "vr_worn_backpack_rip_time",
    "vr_worn_backpack_rip_stiffness",
    "vr_nailpistol_model",
    // 2026-11-02: the old melee options, their "on" kept (ROUND23.md, "Old melee options removed"): the swept blade, the
    // swing through enemies, the strike past walls and floor, one swing hitting everything, the curved blades, the
    // positional melee damage.
    "vr_melee_swept",
    "vr_melee_phase",
    "vr_melee_world_pass",
    "vr_melee_multi",
    "vr_melee_curved_blades",
    "vr_melee_positional",
    // 2026-10-31: the shot volumes' size, a multiplier of the old capsules, replaced by the hulls of the drawn triangles
    // grown by vr_agrunt_vol_* units (ROUND23.md, "Disarm volumes as hulls of the drawn triangles").
    "vr_agrunt_shotdisarm_size",
    // 2026-10-19: the class lists replaced by the monster groups' Lift, Neck Snap, Leg Sweep, Leg Grabs, Manhandle and
    // Two-Hand Throw (vr_mgrp_<group>_*; ROUND22.md, "Monster groups").
    "vr_foegrab_lift_classes",
    "vr_foegrab_throw_always",
    "vr_foegrab_throw_when_hurt",
    // 2026-10-23: the struggling classes' list, replaced by the monster groups' Struggle settings (vr_mgrp_<group>_struggle*;
    // ROUND22.md, "Struggle in the monster groups"); a config older than 190 has its value moved (retiredStruggleClasses).
    "vr_struggle_classes",
    // 2026-10-27: the enforcer's pistols in one hand, now the Weapon Holds' Auto (vr_agrunt_hold_enforcer_*; its default
    // 1 was Auto's, it was never in his config; ROUND22.md, "Armed humanoids' weapon holds").
    "vr_agrunt_enforcer_one_hand",
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
    "vr_agrunt_shotdisarm_absorb", // 2026-10-27: vr_agrunt_shotdisarm_block (on by default: a shot on the weapon stops there)
    // The old derived rig of the grunt and the enforcer (ROUND22.md, "The authored rig the only one"): the authored rig
    // always theirs, its choice and the old rig's own looks gone.
    "vr_rig2",
    "vr_rig2_game",
    "vr_rig2_look",
    "vr_ragdoll_arm_skin",
    // 2026-10-30 (refactor wave C1, his D1): the armed humanoids' old arms (vr_agarms 0: the animation's arms bent onto
    // the weapons, the rigid pieces) and the posed arms' old sides; vr_rig2_clavicle into vr_agarms_clavicle, the held
    // wrist's clearance one constant (vr_agarms.cpp armClearHeldMost).
    "vr_agarms",
    "vr_agrunt_ik_rigid",
    "vr_agrunt_ik_hands",
    "vr_agrunt_ik_wrist",
    "vr_agrunt_ik_twist",
    "vr_agrunt_ik_elbow",
    "vr_agrunt_ik_second",
    "vr_agrunt_ik_drawn",
    "vr_agrunt_ik_misfit",
    "vr_agrunt_ik_reach",
    "vr_agarms_body",
    "vr_agarms_second_hold",
    "vr_agarms_reach_ease",
    "vr_agarms_jump_ease",
    "vr_agarms_aim_line",
    "vr_agarms_second_release",
    "vr_agarms_shoulder_skin",
    "vr_agarms_limits",
    "vr_agarms_arm_clear",
    "vr_agarms_cup_handle",
    "vr_rig2_clavicle",
    "vr_rig2_arm_clear",
    "vr_rig2_arm_limits",
    // 2026-10-30 (round 23, his decision D1: the old sides of the A/B switches removed, his config on the new side of
    // every one): each now always as its default was.
    "vr_torso_mode",                  // the torso mostly the head's (0: the old engine's, towards the hands)
    "vr_ragdoll_active_stagger_walk", // a shoved monster's backpedal walked (0: the old glide)
    "vr_ragdoll_posed_arms",          // an armed ragdoll starts with its posed arms (0: its frame's)
    "vr_box3d_hulls_ahead",           // the props' hulls built ahead on the pool (0: in the sync; 2: compared)
    "vr_struggle",                    // held knockdowns struggle as their monster group says (0: none at all)
    "vr_gore_wash_once",              // a wash takes each texel once (0: the triangles' edges two or three times)
    "vr_decap_own_motion",            // a beheaded body keeps its own motion (0: not carried)
    "vr_limbs_blast",                 // explosions pop limbs (vr_limbs_blast_chance 0: none)
    "vr_water_foam_particle_spray",   // the shore's foam particles rising puffs (1: the spray, never shipped on)
    "vr_water_foam_particle_retro",   // (the spray's retro look)
    "vr_snd_antialias",               // the voices anti-aliased before Quake's 11 kHz lowpass (0: folded)
    "vr_particle_halfres_dense",      // half resolution also in the densest frames (0: full there)
    "vr_parallax_plane",              // the world at its relief's bottom (0: at its top, carved down)
    "vr_parallax_depth_split",        // the depth write only on relief near the eye (0: everywhere)
    "vr_magholster_chain_sim",        // the tether simulated (0: the old chain through the world)
    "vr_2h_grip_edge",                // a two-handed grip only as the grip closes there (0: a closed fist moved on)
    "vr_toolgun_physgun_ragdoll",     // the physgun holds a ragdoll on a spring (0: its entity dragged)
    "vr_autopump",                    // the shotgun's auto pump (0: none, the shell after 0.22 s)
    "vr_ad_death_precache",           // AD's death assets precached early outside AD (0: at first use)
    "vr_nailpistol_slide_time",       // the nail pistol's slide held back after a shot (now vr_nailpistol_slide_cycle)
    // 2026-11-01: the tug-of-war always physical (his note 23:07): the old distance-and-time tug's switch, time and
    // fights back gone (ROUND23.md, "The tug always physical").
    "vr_agrunt_tug_ragdoll",          // the tug physical (0: the old tug)
    "vr_agrunt_tug_time",             // the old tug's pull time
    "vr_agrunt_tug_fight",            // the old tug: how often it fought back
    "vr_agrunt_tug_pull",             // the old tug's pull distance (a tuning constant)
};

// Settings renamed: the old name still reads and sets the new one (Cvar_FindVar asks here when a name is not found):
// an old config's lines set the new settings as it is executed (the next config write has the new names), as do
// binds, scripts and benchmark setups. Never listed, completed or written. (The vr_slipgate* names, renamed to
// vr_teleporter* on 2026-10-08, before 1.0.0, went with the cut at 1.0.1: no 1.0.1 config has them.)
struct CvarAlias
{
    const char* old;
    const char* now;
};
constexpr CvarAlias cvarAliases[] = {
    // 2026-10-16: one throw for props and weapons (config 142; ROUND22.md, "One throw for props and weapons").
    // (vr_carry_two_hands_window's new home, vr_throw_2h_window, is a tuning constant, not a cvar: an alias to it found
    // nothing and an old config's line printed "Unknown command"; the old name is in retiredCvars now.)
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

extern "C" const cvar_t* VR_TunableFind(const char* name)
{
    return qvr::tunableFind(name);
}

extern "C" int VR_RetiredCvar(const char* name)
{
    // 2026-10-30: the weapons' blend towards frame 0 (one hand, two hands: vr_wofs_zb_NN, vr_wofs_zb_2h_NN), which
    // steadied the models' own kick; the kick is taken out now (vr_kickcancel.cpp).
    if(!q_strncasecmp(name, "vr_wofs_zb_", 11))
    {
        return 1;
    }
    // 2026-11-04 (the migration to 222): the store keys that are gone (their stand-ins with them): a config saved while
    // they were registered still carries their lines; each is dropped whole, its value setting nothing.
    if(qvr::weapons::removedKeyLine(name))
    {
        return 1;
    }
    if(qvr::tunableFind(name))
    {
        return 1; // a tuning constant now (vr_tunables.inc): the line sets nothing (a sparse config's value: tunablesTakeConfigLines)
    }
    // 2026-10-31: the one two-handed recoil slider split in four: an old config's value set on each.
    if(qvr::weapons::retiredTwoHandLine(name, Cmd_Argc() > 1 ? Cmd_Argv(1) : nullptr))
    {
        return 1;
    }
    // 2026-10-30: the recoil multipliers (vr_wofs_fx_recoil*_NN, vr_recoil_kick, vr_recoil_rise): an old config's values
    // kept for vr_wofs_version 74's conversion to cm, degrees and seconds (vr_weapons.cpp takeRecoilUnits74).
    if(qvr::weapons::retiredRecoilLine(name, Cmd_Argc() > 1 ? Cmd_Argv(1) : nullptr))
    {
        return 1;
    }
    for(const char* retired : retiredCvars)
    {
        if(!q_strcasecmp(name, retired))
        {
            if(!q_strcasecmp(name, "vr_struggle_classes") && Cmd_Argc() > 1)
            {
                qvr::retiredStruggleClasses = Cmd_Argv(1); // (its config line's value, for the migration to 190)
                qvr::retiredStruggleSeen = true;
            }
            if(!q_strcasecmp(name, "vr_kdtrig_tackle_on") && Cmd_Argc() > 1 && Q_atof(Cmd_Argv(1)) == 0.f &&
                qvr::vr_cfg_version.value >= 177.f)
            {
                // (Shoulder Charges turned off: now its chance 0. Before 177 off was the default, which 177 turned on.)
                Cvar_SetQuick(&qvr::vr_kdtrig_tackle, "0");
            }
            for(qvr::RetiredPackFit& r : qvr::retiredPackFits)
            {
                if(Cmd_Argc() > 1 && !q_strcasecmp(name, r.name))
                {
                    r.seen = true;
                    r.value = static_cast<float>(Q_atof(Cmd_Argv(1)));
                }
            }
            if(!q_strcasecmp(name, "vr_gunmodelpitch") && Cmd_Argc() > 1)
            {
                qvr::retiredGunModelPitch = static_cast<float>(Q_atof(Cmd_Argv(1))); // (for the migration to 203)
            }
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

// Cvar_WriteVariables: a sparse config's line for an archived setting (inSparseConfig).
extern "C" int VR_ConfigSaves(const cvar_s* var)
{
    using namespace qvr;
    return !inSparseConfig(var) || alwaysSaved(var) || !var->default_string || !sameValue(var->string, var->default_string);
}

// The data tables (design review 3; the weapons' quakevr/data/weapons.txt, vr_weapons.cpp; the props'
// quakevr/data/props.txt, vr_props.cpp; the monsters' quakevr/data/monsters.txt, vr_monsterdata.cpp; the monster
// groups' quakevr/data/groups.txt, vr_mgroups.cpp): their values by their old cvar names (vr_wofs_<key>_NN,
// vr_prop_<key>_NN, vr_ragdoll_army_mass, vr_mgrp_ogre_poise, ...: old configs, the console, motion takes, the QC's
// cvar() and cvar handles), their sets, and the config's lines for those changed from the data file (as a sparse
// config's vr_ settings, in the names' order).
za::Array<za::Span<cvar_t>, 4> qvr::standInTables()
{
    return {weapons::standIns(), props::standIns(), monsterdata::standIns(), mgroups::standIns()};
}

extern "C" cvar_s* VR_CvarStandIn(const char* name)
{
    cvar_t* var = qvr::weapons::findStandIn(name);
    var = var ? var : qvr::props::findStandIn(name);
    var = var ? var : qvr::monsterdata::findStandIn(name);
    return var ? var : qvr::mgroups::findStandIn(name);
}

extern "C" int VR_StandInSet(cvar_s* var, const char* value)
{
    return qvr::weapons::setStandIn(var, value) || qvr::props::setStandIn(var, value)
                   || qvr::monsterdata::setStandIn(var, value) || qvr::mgroups::setStandIn(var, value)
               ? 1
               : 0;
}

extern "C" void VR_ConfigWriteTables(FILE* f)
{
    using namespace qvr;
    za::Vector<const cvar_t*> changed;
    for(const za::Span<cvar_t> table : standInTables())
    {
        for(const cvar_t& var : table)
        {
            if(!sameValue(var.string, var.default_string))
            {
                changed.pushBack(&var);
            }
        }
    }
    std::sort(changed.begin(), changed.end(), [](const cvar_t* a, const cvar_t* b) { return strcmp(a->name, b->name) < 0; });
    for(const cvar_t* var : changed)
    {
        fprintf(f, "%s \"%s\"\n", var->name, var->string);
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
    // Nor in the kit's test runs (QVR_TEST_BACKGROUND): their settings are the test's, written only at quit (and put back
    // by the kit at once). A save mid-run wrote them into the worktree's config, which the parallel copies of other tests
    // read at start-up (run.ps1 -Instance): a stuck run's Position: Seated left it there 18 minutes and raised every
    // climb test's hands by 13 units (nightly 2026-11-01, prop_climb 2/10).
    if(getenv("QVR_TEST_BACKGROUND") != nullptr)
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

cvars::Override::Override(cvar_t& var, float value) : var(var), was(var.string)
{
    Cvar_SetValueQuick(&var, value);
}

cvars::Override::Override(cvar_t& var, const char* value) : var(var), was(var.string)
{
    Cvar_SetQuick(&var, value);
}

cvars::Override::~Override()
{
    Cvar_SetQuick(&var, was.cStr());
}

void cvars::Snapshot::keep(cvar_t& var)
{
    saved.emplaceBack(Saved{&var, za::String{var.string}});
}

void cvars::Snapshot::restore()
{
    for(int i = size() - 1; i >= 0; --i)
    {
        Cvar_SetQuick(saved[i].var, saved[i].value.cStr());
    }
    saved.clear();
}

} // namespace qvr
