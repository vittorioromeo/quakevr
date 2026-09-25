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
};
constexpr int configVersion = 9;

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
    Cvar_SetValueQuick(&vr_cfg_version, static_cast<float>(configVersion));
}

} // namespace

void registerCvars()
{
#define QVR_CVAR(name, def, flags) Cvar_RegisterVariable(&name);
#include "vr_cvars.inc"
#undef QVR_CVAR

    Cvar_RegisterVariable(&vr_backend);
    Cmd_AddCommand("vr_migrate_config", migrateConfig_f);
}

} // namespace qvr
