// vr_cvars.cpp -- Quake VR cvar definitions and registration.

#include "vr_cvars.hpp"
#include "vr_body.hpp"
#include "vr_bodycal.hpp"
#include "vr_engine.hpp"
#include "vr_weapons.hpp"

#include <cstring>
#include <map>
#include <string>
#include <utility>

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
    {10, &vr_corpse_health, "40"},        // doubled (big monsters take more again)
    {11, &vr_bash_speed, "1.6"},          // a gentler push bashes (round 18: the guard is the parry's now)
    {12, &vr_sight_hue, "30"},            // their own orange: they follow the player's hue now (vr_player_hue)
    {13, &vr_shove_speed, "1.8"},         // the author's shoves go 3.2-4.8 m/s, his hands waved at the dummy 2.2 (round 21)
    {14, &vr_counter_glow, "1"},          // off: the author would rather play without it (round 21, "Stamina on the gadget; the glow")
};
constexpr int configVersion = 16;

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

// Right after the saved config is executed (Cmd_Exec_f queues it). "vr_migrate_config new": there was no saved config
// (a first start): the settings are this version's, nothing to change.
void migrateConfig_f()
{
    bodycal::migrate(); // round 21's arm settings, whatever the version (they are moved, not changed in place)
    if(Cmd_Argc() > 1 && !strcmp(Cmd_Argv(1), "new"))
    {
        Cvar_SetValueQuick(&vr_cfg_version, static_cast<float>(configVersion));
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
    // 16: the author's hand calibration ships with his weapon offsets, which were set over it (round 21, "Defaults: the
    // author's weapon offsets and settings"; the weapons take his offsets: vr_wofs_version 20). A config that never
    // calibrated its hands (each value the old default, or the new one) takes all of it, or its guns would sit off its
    // hands; one that did keeps its own. A config saved before the calibration existed (13) has no vr_handcal_*: its
    // hands were at 0, not at the new defaults it has now.
    if(from < 16)
    {
        const std::pair<cvar_t*, const char*> angles[] = {
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
    Cvar_SetValueQuick(&vr_cfg_version, static_cast<float>(configVersion));
}

// Shipped defaults (quakevr/vr_defaults.cfg, executed by default.cfg): the tuned values over the
// ones compiled in. "vr_default name value" sets a cvar and makes the value its default, so resets
// and presets return to it; the saved config still wins, as it's executed after.
// The engine cvars' own defaults, before vr_default replaced them.
std::map<std::string, std::string> engineDefaults;

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
    if(std::strncmp(var->name, "vr_", 3) != 0)
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
    return var == &vr_cfg_version || var == &vr_bindings_version || var == &vr_wofs_version || var == &vr_height_calibration
        || var == &vr_xr_runtime || var == &vr_xr_runtime_json || var == &vr_note_device || var == &vr_dominant_eye
        || !std::strncmp(var->name, "vr_motion_", 10) // the motion recorder's (a tool's settings)
        || !std::strncmp(var->name, "vr_bodycal_", 11) || !std::strncmp(var->name, "vr_body_tweak_", 14) // one's body
        // and one's arms (Body > Arms, the player's to tweak: the author's decision, 2026-09-28; pauldrons ship)
        || var == &vr_body_arm_length || var == &vr_body_arm_stretch || var == &vr_body_shoulder_reach
        || var == &vr_body_forearm_twist || var == &vr_body_wrist_limits || var == &vr_body_elbow_out
        || var == &vr_body_elbow_back || var == &vr_body_elbow_hand;
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
        if(std::strncmp(var->name, "r_", 2) != 0 && std::strncmp(var->name, "gl_", 3) != 0)
        {
            continue;
        }
        const auto it = engineDefaults.find(var->name);
        const char* def = it != engineDefaults.end() ? it->second.c_str() : var->default_string;
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
