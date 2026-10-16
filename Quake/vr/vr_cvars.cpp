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
    // 134: AD's fumigator and centurion are humanoids for the lift and the neck snap (ROUND22.md, "Neck snap: strict
    // zone, diagnostic, slow motion, crack").
    {134, &vr_foegrab_lift_classes,
        "monster_army monster_enforcer monster_zombie monster_knight monster_hell_knight monster_army_grenade "
        "monster_army_plasma monster_army_rocket monster_defender monster_eliminator monster_pyro monster_dcrossbow "
        "monster_dfury monster_dguard monster_dknight monster_dsergeant monster_zombiek monster_mummy monster_skullwiz "
        "monster_ranged_knight infected"},
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
};
constexpr int configVersion = 142;
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
