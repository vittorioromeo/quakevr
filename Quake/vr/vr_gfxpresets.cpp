// vr_gfxpresets.cpp -- graphics presets and My Settings (vr_gfxpresets.hpp), on vr_presets.cpp's machinery.

#include "vr_gfxpresets.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_presets.hpp"

namespace qvr::gfxpresets
{
namespace
{

using presetcore::D;

// A row of the table: a setting and its value at Off, Low, Medium, High and Ultra. D: the setting's default (its
// default_string, vr_defaults.cfg's included), so High, all D, is the game as shipped.
// Every setting a preset sets (and My Settings keeps). Off is Quake's own look (flat dynamic lights, no shadows, no
// bumps, no post-processing); Low and Medium trade the costliest effects for frame time (ROUND22.md, "Graphics
// presets": what each costs); High is the defaults; Ultra goes past them where a fast card has more to give.
// clang-format off
constexpr presetcore::Row presetRows[] = {
    //                                   Off      Low      Medium   High  Ultra
    // Shadows and dynamic lights (vr_lighting.cpp)
    {&vr_shadow_dlights,              {"0",     "2",     "4",     D,    "12"}},
    {&vr_shadow_dlight_size,          {"256",   "256",   "512",   D,    D}},
    {&vr_shadow_maplights,            {"0",     "0",     "2",     D,    "6"}},
    {&vr_shadow_maplight_size,        {"512",   "512",   "512",   D,    D}},
    {&vr_shadow_filter,               {"1",     "1",     "1",     D,    D}},
    {&vr_shadow_atlas,                {"4096",  "4096",  "4096",  D,    D}},
    {&vr_shadow_distance,             {"1024",  "1024",  "1536",  D,    "4096"}},
    {&vr_shadow_muzzleflash,          {"0",     "0",     "0",     D,    D}},
    {&vr_dlight_models,               {"0",     D,       D,       D,    D}},
    {&vr_dlight_angle,                {"0",     D,       D,       D,    D}},
    {&vr_entity_shadows,              {"0",     D,       D,       D,    D}},
    {&vr_model_lighting,              {"0",     D,       D,       D,    D}},
    // The look: Quake's in Off, the defaults from Low up
    {&vr_light_contrast,              {"1",     D,       D,       D,    D}},
    {&vr_ambient_light,               {"0",     D,       D,       D,    D}}, // Quake's maps have no fill light of their own (Off: none; the default, 0.1, from Low up)
    {&vr_bloom,                       {"0",     D,       D,       D,    D}},
    {&vr_tonemap,                     {"0",     D,       D,       D,    D}}, // the float scene and tone curve (vr_tonemap.cpp)
    {&vr_grade,                       {"0",     D,       D,       D,    D}},
    {&vr_flash_scale,                 {"1",     D,       D,       D,    D}},
    {&vr_explosion_light_scale,       {"1",     D,       D,       D,    D}},
    {&vr_colored_lights,              {"0",     D,       D,       D,    D}},
    {&vr_dlight_uncapped,             {"0",     D,       D,       D,    D}},
    {&vr_dlight_falloff,              {"0",     D,       D,       D,    D}},
    {&vr_specular,                    {"0",     D,       D,       D,    D}},
    {&vr_model_light_parity,          {"0",     D,       D,       D,    D}},
    {&vr_model_ambient_dir,           {"0",     D,       D,       D,    D}}, // directional ambient on models (vr_ambient.cpp)
    {&vr_model_light_grid,            {"0",     D,       D,       D,    D}}, // models lit by the baked light grid (Off: Quake's floor light)
    {&vr_rim_light,                   {"0",     D,       D,       D,    D}}, // (vr_envmap.cpp)
    {&vr_viewmodel_minlight,          {"24",    D,       D,       D,    D}},
    {&vr_texture_smooth,              {"0",     D,       D,       D,    D}},
    {&vr_alpha_coverage,              {"0",     D,       D,       D,    D}}, // fences' mips as Quake's
    // Lights from things (vr_emissive.cpp)
    {&vr_projectile_lights,           {"0",     D,       D,       D,    D}}, // hell knights' flames, plasma bolts...
    {&vr_lavanail_lights,             {"0",     "4",     D,       D,    D}},
    {&vr_lavagun_light,               {"0",     D,       D,       D,    D}},
    {&vr_beam_lights,                 {"0",     "0",     D,       D,    D}}, // the lightning's stream of lights
    {&vr_flame_lights,                {"0",     "1",     D,       D,    D}}, // the flamethrowers'
    {&vr_torch_lights,                {"0",     "4",     D,       D,    "12"}},
    {&vr_weapon_screen_light,         {"0",     D,       D,       D,    D}},
    {&vr_gadget_light,                {"0",     D,       D,       D,    D}},
    {&vr_screen_glow,                 {"0",     D,       D,       D,    D}},
    {&vr_screen_text_glow,            {"0",     D,       D,       D,    D}},
    {&vr_weapon_glow,                 {"0",     D,       D,       D,    D}},
    {&vr_weapon_reflections,          {"0",     "0",     D,       D,    D}}, // (vr_envmap.cpp)
    {&vr_adgun_plasma_fx,             {"0",     D,       D,       D,    D}}, // AD's plasma bolts' arcs
    // Surfaces: bumps, parallax, detail, carved holes, marks
    {&vr_normalmaps,                  {"0",     "0",     D,       D,    D}}, // (next map)
    {&vr_parallax,                    {"0",     "0",     D,       D,    D}},
    // The relief's own depth written only in Ultra (off by default since config 192): its programs lose early depth
    // testing, 1.3 ms of an AD fight's 3.3 ms eye frame even split (ROUND22.md, "Parallax depth write's cost")
    {&vr_parallax_depth_write,        {D,       D,       D,       D,    "1"}},
    {&vr_parallax_steps,              {D,       D,       D,       D,    "24"}},
    {&vr_detail,                      {"0",     "0",     D,       D,    D}}, // (off by default)
    {&vr_decal_carve,                 {"0",     "0",     D,       D,    D}},
    {&vr_adgun_fx_scorch,             {"0",     D,       D,       D,    D}}, // the flamethrowers' scorch marks
    // Ambient occlusion (vr_ao.cpp)
    {&vr_ao_dynamic,                  {"0",     "0",     D,       D,    D}},
    {&vr_ao_brush,                    {"0",     "0",     D,       D,    D}},
    {&vr_ao_models,                   {"0",     "0",     D,       D,    D}},
    // Particles (vr_particles.cpp)
    {&vr_water_splash,                {"0",     D,       D,       D,    D}},
    {&vr_particle_light,              {"0",     D,       D,       D,    D}},
    {&vr_soft_particles,              {"0",     "0",     D,       D,    D}},
    {&vr_particle_halfres,            {D,       D,       D,       D,    "0"}}, // Ultra: all at full resolution
    {&vr_particle_max_screen,         {"0.1",   "0.1",   "0.12",  D,    D}},
    // Quake VR's own smoke and debris (release 1.1 audit): none in Quake's look; fewer glowing chunks' lights on Low,
    // as its torch and lava nail lights
    {&vr_gunfx_smoke,                 {"0",     D,       D,       D,    D}}, // the muzzle's smoke cone
    {&vr_gunfx_wisps,                 {"0",     D,       D,       D,    D}}, // the barrels' smoke wisps after firing
    {&vr_muzzle_smoke_enemies,        {"0",     D,       D,       D,    D}}, // grunts' and enforcers' muzzle smoke
    {&vr_explosion_debris,            {"0",     D,       D,       D,    D}},
    {&vr_explosion_debris_lights,     {"0",     "4",     D,       D,    D}},
    // Liquids (vr_water.cpp), teleporters (vr_portals.cpp), heat haze (vr_haze.cpp)
    {&vr_water_waves,                 {"0",     D,       D,       D,    D}},
    {&vr_water_fresnel,               {"0",     D,       D,       D,    D}},
    {&vr_water_reflections,           {"0",     D,       D,       D,    D}},
    {&vr_water_glints,                {"0",     D,       D,       D,    D}},
    {&vr_water_lava_glow,             {"0",     D,       D,       D,    D}},
    {&vr_water_underwater,            {"0",     D,       D,       D,    D}},
    {&vr_water_wobble,                {"0",     D,       D,       D,    D}},
    {&vr_water_refraction,            {"0",     "0",     D,       D,    D}},
    {&vr_water_caustics,              {"0",     "0",     D,       D,    D}},
    {&vr_water_geo_waves,             {"0",     "0",     D,       D,    D}},
    {&vr_water_foam,                  {"0",     "0",     D,       D,    D}},
    {&vr_water_ripples,               {"0",     "0",     D,       D,    D}},
    {&vr_portals,                     {"0",     "0",     D,       D,    D}},
    {&vr_heat_haze,                   {"0",     "0",     D,       D,    D}},
    {&vr_heat_haze_half,              {"1",     "1",     "1",     D,    D}}, // full size from High (his notes 2026-10-26)
};
// clang-format on

constexpr const char* levelNames[levelCount] = {"Off (Quake)", "Low", "Medium", "High", "Ultra"};

constexpr presetcore::Table table{"graphics", "VR Settings > Graphics Presets", presetRows,
    static_cast<int>(sizeof(presetRows) / sizeof(presetRows[0])), levelNames, levelCount};
presetcore::Kind presetKind{table};

// vr_graphics_preset: 0..4 applies that preset, 5 My Settings; then back to -1 (a button: the state is
// vr_graphics_preset_status's, and the menu's).
void onPresetCvar(cvar_t* var)
{
    const int level = static_cast<int>(var->value);
    if(var->value < 0.f)
    {
        return;
    }
    if(level == levelMine)
    {
        if(!presetKind.restoreMine())
        {
            Con_Printf("vr_graphics_preset: no My Settings kept yet\n");
        }
    }
    else
    {
        presetKind.apply(level);
    }
    Cvar_SetQuick(var, "-1");
}

void saveCmd()
{
    presetKind.saveMine();
    Con_Printf("graphics preset: My Settings saved (%d settings, %s)\n", presetKind.table().rowCount, presetKind.minePath().cStr());
}

// vr_graphics_preset_status: the state, and the rows that differ from the nearest preset (tests, and to see what a
// Custom is made of).
void statusCmd()
{
    presetKind.printStatus();
}

} // namespace

presetcore::Kind& kind()
{
    return presetKind;
}

const char* levelName(int level)
{
    return presetKind.levelName(level);
}

void apply(int level)
{
    presetKind.apply(level);
}

bool restoreMine()
{
    return presetKind.restoreMine();
}

void saveMine()
{
    presetKind.saveMine();
}

bool hasMine()
{
    return presetKind.hasMine();
}

int matchingLevel()
{
    return presetKind.matchingLevel();
}

bool matchesMine()
{
    return presetKind.matchesMine();
}

const char* stateName()
{
    return presetKind.stateName();
}

const char* menuStateLine()
{
    return presetKind.menuStateLine();
}

const char* menuNoteLine()
{
    return presetKind.menuNoteLine();
}

void menuApply(int level)
{
    presetKind.menuApply(level);
}

void menuRestoreMine()
{
    presetKind.menuRestoreMine();
}

void menuSaveMine()
{
    presetKind.menuSaveMine();
}

void init()
{
    Cvar_SetCallback(&vr_graphics_preset, onPresetCvar);
    Cmd_AddCommand("vr_graphics_preset_save", saveCmd);
    Cmd_AddCommand("vr_graphics_preset_status", statusCmd);
}

} // namespace qvr::gfxpresets
