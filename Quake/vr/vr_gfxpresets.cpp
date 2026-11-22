// vr_gfxpresets.cpp -- graphics presets and My Settings (vr_gfxpresets.hpp), on vr_presets.cpp's machinery.

#include "vr_gfxpresets.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_presets.hpp"
#include "vr_api.h"

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

// The Quest (standalone, OpenGL ES; QUEST_PLAN.md section 6c, C6's groundwork): the Low preset, then these rows, at
// three targets. A draft from the PC's numbers under the ES profile scaled to the XR2 Gen 2 (estimates, not device
// measurements: C6 sets them from the headset's). Only under -glescompat (and on Android): the PC's settings are
// never touched by it. The resolution: the share of the runtime's recommended eye size, the rest of the frame from
// dynamic resolution (vr_dynres) once C6 ties it to the GPU's frame time; no upscaling pass (the compositor scales).
// clang-format off
constexpr presetcore::Row questRows[] = {
    //                                   Q3 72 Hz  Q3 90 Hz  Q2 72 Hz
    // The eyes' pixels: the GPU's biggest lever (every pass scales with it)
    {&vr_render_scale,                {"0.9",    "0.75",   "0.65"}},
    {&vr_dynres,                      {"1",      "1",      "1"}},
    {&vr_dynres_min,                  {"0.65",   "0.5",    "0.5"}},   // his call 2026-11-22 (C5i): 0.55 -> 0.65 now the world's lights are Quake's way (C5g/C5h: the fight 11.3 -> 7.0 ms); C4o had cut 0.65 -> 0.55
    {&vr_upscale,                     {"0",      "0",      "0"}},     // bilinear: no full-size pass
    {&vid_fsaa,                       {"0",      "0",      "0"}},     // off: 2x cost the Quest 3 ~100 ms a frame (C4e: e1m1 138 -> 41 ms; the float scene's MSAA resolved through memory, not on chip)
    {&vr_taa,                         {"0",      "0",      "0"}},     // its history reads and writes: bandwidth
    {&r_oit,                          {"0",      "0",      "0"}},     // translucent things sorted, not order-independent: OIT's own target is a render pass more an eye (-0.6 to -0.9 ms of the Quest 3's GPU, C4j)
    // Shadows: no shadowed dynamic light, so the world's lights take the plain loop (vr_gl_lights_plain: a big light
    // ~3.5 ms of world shading with the shadow code compiled in, ~1 without; C4p, QUEST_PLAN.md 6w); the atlas a quarter
    // of Low's (16 MB of depth instead of 64) for whoever turns them on
    {&vr_shadow_dlights,              {"0",      "0",      "0"}},
    {&vr_shadow_dlight_size,          {"256",    "256",    "256"}},
    {&vr_shadow_maplights,            {"0",      "0",      "0"}},
    {&vr_shadow_atlas,                {"2048",   "2048",   "2048"}},
    {&vr_shadow_distance,             {"768",    "640",    "512"}},
    // Post-processing: bloom's chain of half-size passes off at 90 Hz and on the Quest 2 (the tone curve kept: the look)
    {&vr_bloom,                       {"0",      "0",      "0"}},     // off: the Quest 3 showed its chain as squares (C4f)
    {&vr_tonemap,                     {"2",      "2",      "2"}},     // the float scene in 32 bits (R11F_G11F_B10F), not 64: -1.4 ms of the Quest 3's GPU, the same picture (C4h)
    {&vr_heat_haze,                   {"0",      "0",      "0"}},
    {&vr_water_reflections,           {"0",      "0",      "0"}},     // (the envmap)
    {&vr_rim_light,                   {"0",      "0",      "0"}},     // (the envmap)
    // Lights from things (each one a cluster entry every pixel near it walks)
    {&vr_torch_lights,                {"2",      "2",      "1"}},
    {&vr_lavanail_lights,             {"2",      "2",      "1"}},
    {&vr_explosion_debris_lights,     {"2",      "2",      "0"}},
    {&vr_rig_lights_world,            {"1",      "1",      "1"}},     // the rig's small lights on the walls: his look call (0 saves the world's lights' loop near walls; C4v, QUEST_PLAN.md 6zb)
    // The world's dynamic lights Quake's way, summed once a frame into a texture at the lightmap's grid, as bright as
    // the loop (his look call: OK, flatter): e1m1's frozen fight with 3 big lights 11.33 -> 7.00 ms of the Quest 3's GPU,
    // 72 Hz held (the loop dropped to 24), +1.5 ms CPU (C5g, QUEST_PLAN.md 6zr). Your body's Quake's way too
    // (vr_gl_quake_dlights: both at once): drawn by its usual form with the loop skipped, posed head 0.7 pitch 70 11.65 ->
    // 10.63 ms, pitch 35 8.71 -> 7.98, the fight the same (6.90 / 7.02) (C5h, 6zs).
    {&vr_gl_world_dlmap,              {"1",      "1",      "1"}},
    {&vr_gl_quake_dlights,            {"1",      "1",      "1"}},
    // The eyes' depth 24-bit fixed point, not 32-bit float: posed head 0.7, 4 rounds interleaved in one session, pitch 0
    // 5.30 -> 4.93 ms, pitch 35 8.93 -> 7.74 (C5h, 6zs); the near plane 1 unit, not 0.02 (held items are never cut).
    {&vr_depth_float,                 {"0",      "0",      "0"}},
    // Models: the world's models shaded as drawn, no depth pre-pass (the tiler's own early depth test does it): e1m1's
    // frozen fight -0.49 ms of the Quest 3's GPU, 20 draws fewer, the same picture (C4w, QUEST_PLAN.md 6zc)
    {&vr_alias_prepass_world,         {"0",      "0",      "0"}},
    // Particles, marks, bodies: fill rate, the decal walk, and the CPU's physics (the big cores ~3x slower than a PC's)
    {&vr_particle_max_screen,         {"0.08",   "0.06",   "0.05"}},
    {&vr_particle_halfres,            {"1",      "1",      "1"}},
    {&vr_decal_max,                   {"256",    "192",    "128"}},
    {&vr_decal_bucket_max,            {"48",     "32",     "32"}},
    {&vr_ragdoll_max,                 {"4",      "3",      "3"}},
    {&vr_smallgibs_max,               {"32",     "24",     "16"}},
    {&vr_limbs_max,                   {"12",     "8",      "8"}},
    {&vr_explosion_debris_max,        {"32",     "24",     "16"}},
    {&vr_gore_dents_max,              {"6",      "4",      "4"}},
};
// clang-format on

constexpr int questLevelCount = 3;
constexpr const char* questLevelNames[questLevelCount] = {"Quest 3 (72 Hz)", "Quest 3 (90 Hz)", "Quest 2 (72 Hz)"};
constexpr presetcore::Table questTable{"quest", "vr_quest_preset (-glescompat)", questRows,
    static_cast<int>(sizeof(questRows) / sizeof(questRows[0])), questLevelNames, questLevelCount};
presetcore::Kind questKind{questTable};

// vr_quest_preset [0 Quest 3 72 Hz | 1 Quest 3 90 Hz | 2 Quest 2 72 Hz | mine]: the Low preset, then the Quest's rows.
void questPresetCmd()
{
    if(!VR_GLESCompat() && !VR_Android())
    {
        Con_Printf("vr_quest_preset: the Quest's settings, only under -glescompat (or on Android)\n");
        return;
    }
    if(Cmd_Argc() < 2)
    {
        Con_Printf("vr_quest_preset <level>: the Low graphics preset, then the Quest's rows (\"mine\": as before)\n");
        for(int i = 0; i < questLevelCount; i++)
        {
            Con_Printf("  %d %s\n", i, questLevelNames[i]);
        }
        questKind.printStatus();
        return;
    }
    if(!q_strcasecmp(Cmd_Argv(1), "mine"))
    {
        if(!questKind.restoreMine() || !presetKind.restoreMine())
        {
            Con_Printf("vr_quest_preset: no settings kept from before yet\n");
        }
        return;
    }
    const int level = Q_atoi(Cmd_Argv(1));
    if(level < 0 || level >= questLevelCount)
    {
        Con_Printf("vr_quest_preset: 0 to %d\n", questLevelCount - 1);
        return;
    }
    presetKind.apply(1); // Low
    questKind.apply(level);
    Con_Printf("vr_quest_preset: %s (the Low preset and %d rows)\n", questLevelNames[level], questTable.rowCount);
}

// The Quest's must-haves (vr_quest_check): saved settings that break the headset's frames, whatever the config says.
struct QuestMust
{
    cvar_t* var;
    const char* value;
    const char* why;
    bool atMost; // a ceiling (lower values kept), not the one value
};
const QuestMust questMusts[] = {
    {&vid_fsaa, "0", "MSAA goes through memory on the Quest 3: ~100 ms a frame (QUEST_PLAN.md, C4e)", false},
    {&vr_bloom, "0", "the bloom's chain draws coloured squares or black views on the Quest (C4f)", false},
    // his saved config's two PC values against the Quest preset (adb session of 2026-11-17: 44 fps of 72 with both,
    // 51 with the first set back, 63 with both)
    {&vr_shadow_dlights, "0", "the dynamic lights' shadow maps cost the Quest 3 ~4 ms a frame (44 -> 51 fps, his saved config)", true},
    {&vr_dynres_min, "0.65", "the dynamic resolution's floor above the Quest preset's keeps fights GPU-bound (his saved PC value: 51 -> 63 fps)", true},
};

// vr_quest_check (Android starts, after the saved config; Debug > Startup): the Quest rows that differ from the nearest
// Quest preset printed (the log names what a slow start runs with), and the must-haves above set back, each said; with
// vr_quest_guard 0 only printed.
void questCheckCmd()
{
    if(!VR_GLESCompat() && !VR_Android())
    {
        Con_Printf("vr_quest_check: the Quest's settings, only under -glescompat (or on Android)\n");
        return;
    }
    questKind.printStatus();
    for(const QuestMust& m : questMusts)
    {
        if(m.atMost ? m.var->value <= Q_atof(m.value) : !q_strcasecmp(m.var->string, m.value))
        {
            continue;
        }
        if(vr_quest_guard.value == 0.f)
        {
            Con_Printf("vr_quest_check: %s %s kept (vr_quest_guard 0): %s\n", m.var->name, m.var->string, m.why);
            continue;
        }
        Con_Printf("vr_quest_check: %s %s -> %s: %s (vr_quest_guard 0 keeps it)\n", m.var->name, m.var->string, m.value,
            m.why);
        Cvar_SetQuick(m.var, m.value);
    }
}

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
    Cmd_AddCommand("vr_quest_preset", questPresetCmd);
    Cmd_AddCommand("vr_quest_check", questCheckCmd);
}

} // namespace qvr::gfxpresets
