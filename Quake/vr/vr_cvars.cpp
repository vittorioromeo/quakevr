// vr_cvars.cpp -- Quake VR cvar definitions and registration.

#include "vr_cvars.hpp"
#include "vr_engine.hpp"

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
    {8, &vr_melee_distance, "0.2"},        // a full blow: wiggles and whips of the hand were hitting
    {9, &vr_parallax_models, "0.75"},      // off: parallax on 8-bit skins bends their texels (the bumps give models relief now)
    {10, &vr_parry_angle, "50"},          // degrees off level now (was off square to the blow, by the hand's forward)
    {10, &vr_corpse_health, "40"},        // doubled (big monsters take more again)
    {11, &vr_bash_speed, "1.6"},          // a gentler push bashes (round 18: the guard is the parry's now)
    {12, &vr_sight_hue, "30"},            // their own orange: they follow the player's hue now (vr_player_hue)
};
constexpr int configVersion = 12;

// Right after the saved config is executed (Cmd_Exec_f queues it).
void migrateConfig_f()
{
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
    Cvar_SetValueQuick(&vr_cfg_version, static_cast<float>(configVersion));
}

// Shipped defaults (quakevr/vr_defaults.cfg, executed by default.cfg): the tuned values over the
// ones compiled in. "vr_default name value" sets a cvar and makes the value its default, so resets
// and presets return to it; the saved config still wins, as it's executed after.
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

// Per-player or bookkeeping settings, never shipped.
[[nodiscard]] bool personal(const cvar_t* var)
{
    return var == &vr_cfg_version || var == &vr_bindings_version || var == &vr_wofs_version || var == &vr_height_calibration
        || var == &vr_xr_runtime || var == &vr_xr_runtime_json || var == &vr_note_device;
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
