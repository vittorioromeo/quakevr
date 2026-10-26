// vr_gfxpresets.cpp -- graphics presets and My Settings (vr_gfxpresets.hpp).

#include "vr_gfxpresets.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_files.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/StringView.hpp"

#include <stdlib.h>
#include <string.h>

namespace qvr::gfxpresets
{
namespace
{

// A row of the table: a setting and its value at Off, Low, Medium, High and Ultra. D: the setting's default (its
// default_string, vr_defaults.cfg's included), so High, all D, is the game as shipped.
struct Row
{
    cvar_t* var;
    const char* values[levelCount];
};

constexpr const char* D = nullptr;

// Every setting a preset sets (and My Settings keeps). Off is Quake's own look (flat dynamic lights, no shadows, no
// bumps, no post-processing); Low and Medium trade the costliest effects for frame time (ROUND22.md, "Graphics
// presets": what each costs); High is the defaults; Ultra goes past them where a fast card has more to give.
// clang-format off
constexpr Row presetRows[] = {
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
    {&vr_bloom_fast,                  {D,       D,       D,       D,    "0"}}, // Ultra: the four-level glow
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
    {&vr_permdecals,                  {"0",     D,       D,       D,    D}},
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

[[nodiscard]] const char* target(const Row& row, int level)
{
    const char* v = row.values[level];
    return v ? v : row.var->default_string ? row.var->default_string : "0";
}

// Two values the same: as numbers when both are ("1" and "1.0"), else as text.
[[nodiscard]] bool same(const char* a, const char* b)
{
    char* endA = nullptr;
    char* endB = nullptr;
    const double x = strtod(a, &endA);
    const double y = strtod(b, &endB);
    if(endA != a && *endA == '\0' && endB != b && *endB == '\0')
    {
        return static_cast<float>(x) == static_cast<float>(y);
    }
    return strcmp(a, b) == 0;
}

[[nodiscard]] const Row* findRow(za::StringView name)
{
    for(const Row& row : presetRows)
    {
        if(name == za::StringView{row.var->name})
        {
            return &row;
        }
    }
    return nullptr;
}

// My Settings: <gamedir>/graphics_my_settings.cfg, a row's `name "value"` a line. Read the first time it is asked for in
// a game folder; written whole through a temporary file.
struct MineEntry
{
    za::String name;
    za::String value;
};

struct Mine
{
    za::String dir; // the game folder it was read from (another one: read again)
    bool loaded{false};
    bool present{false};
    za::Vector<MineEntry> entries;
};
Mine mine;

// The menu's note (what the last press did) and Save's first press.
struct MenuState
{
    char note[160]{};
    double noteTime{-100.0};
    double saveArmedTime{-100.0};
    char line[96]{};
};
MenuState menu;

[[nodiscard]] za::String minePath()
{
    return za::String{com_gamedir} + "/graphics_my_settings.cfg";
}

void loadMine()
{
    if(mine.loaded && mine.dir == za::StringView{com_gamedir})
    {
        return;
    }
    mine = Mine{};
    mine.loaded = true;
    mine.dir = za::String{com_gamedir};
    za::String text;
    if(!files::readText(minePath().cStr(), text))
    {
        return;
    }
    files::forLines(za::StringView{text.cStr()}, [](za::StringView line) {
        za::SizeT i = 0;
        while(i < line.size() && static_cast<unsigned char>(line[i]) <= ' ')
        {
            i++;
        }
        za::SizeT j = i;
        while(j < line.size() && static_cast<unsigned char>(line[j]) > ' ')
        {
            j++;
        }
        const za::StringView name = line.substrByPosLen(i, j - i);
        if(name.empty() || line.substrByPosLen(i, line.size() - i).startsWith(za::StringView{"//"}) || !findRow(name))
        {
            return;
        }
        const za::SizeT open = line.find('"', j);
        const za::SizeT close = open == za::StringView::nPos ? za::StringView::nPos : line.find('"', open + 1);
        if(close == za::StringView::nPos)
        {
            return;
        }
        mine.entries.pushBack(MineEntry{za::String{name}, za::String{line.substrByPosLen(open + 1, close - open - 1)}});
    });
    mine.present = !mine.entries.empty();
}

void writeMine()
{
    za::String text{"// Quake VR: your own graphics settings (VR Settings > Graphics Presets > My Settings), kept the first "
                    "time a preset was applied or by Save as My Settings.\n"};
    for(const MineEntry& e : mine.entries)
    {
        text += e.name;
        text += " \"";
        text += e.value;
        text += "\"\n";
    }
    const za::String path = minePath();
    const za::String tmp = path + ".tmp";
    if(!files::writeText(tmp.cStr(), za::StringView{text.cStr()}) || !files::rename(tmp.cStr(), path.cStr()))
    {
        files::remove(tmp.cStr());
        Con_Printf("VR: could not write %s\n", path.cStr());
    }
}

// The current values of every row kept as My Settings, and written.
void captureMine()
{
    loadMine();
    mine.entries.clear();
    for(const Row& row : presetRows)
    {
        mine.entries.pushBack(MineEntry{za::String{row.var->name}, za::String{row.var->string}});
    }
    mine.present = true;
    writeMine();
}

[[nodiscard]] bool matches(int level)
{
    for(const Row& row : presetRows)
    {
        if(!same(row.var->string, target(row, level)))
        {
            return false;
        }
    }
    return true;
}

void setNote(const char* text)
{
    q_strlcpy(menu.note, text, sizeof(menu.note));
    menu.noteTime = realtime;
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
        if(!restoreMine())
        {
            Con_Printf("vr_graphics_preset: no My Settings kept yet\n");
        }
    }
    else
    {
        apply(level);
    }
    Cvar_SetQuick(var, "-1");
}

void saveCmd()
{
    saveMine();
    Con_Printf("graphics preset: My Settings saved (%d settings, %s)\n", static_cast<int>(mine.entries.size()),
        minePath().cStr());
}

// vr_graphics_preset_status: the state, and the rows that differ from the nearest preset (tests, and to see what a
// Custom is made of).
void statusCmd()
{
    Con_Printf("graphics preset: %s (rows %d, My Settings %s)\n", stateName(), static_cast<int>(sizeof(presetRows) /
        sizeof(presetRows[0])), hasMine() ? "kept" : "none");
    int nearest = 0;
    int nearestDiff = 1 << 30;
    for(int level = 0; level < levelCount; level++)
    {
        int diff = 0;
        for(const Row& row : presetRows)
        {
            diff += same(row.var->string, target(row, level)) ? 0 : 1;
        }
        if(diff < nearestDiff)
        {
            nearestDiff = diff;
            nearest = level;
        }
    }
    if(nearestDiff == 0)
    {
        return;
    }
    Con_Printf("  nearest %s, %d differ:\n", levelName(nearest), nearestDiff);
    for(const Row& row : presetRows)
    {
        if(!same(row.var->string, target(row, nearest)))
        {
            Con_Printf("    %s %s (%s: %s)\n", row.var->name, row.var->string, levelName(nearest), target(row, nearest));
        }
    }
}

} // namespace

const char* levelName(int level)
{
    return levelNames[za::clamp(level, 0, levelCount - 1)];
}

void apply(int level)
{
    level = za::clamp(level, 0, levelCount - 1);
    loadMine();
    if(!mine.present)
    {
        captureMine(); // your own settings, before the first preset changes them
        Con_DPrintf("graphics preset: your settings kept as My Settings (%s)\n", minePath().cStr());
    }
    for(const Row& row : presetRows)
    {
        Cvar_SetQuick(row.var, target(row, level));
    }
}

bool restoreMine()
{
    loadMine();
    if(!mine.present)
    {
        return false;
    }
    for(const MineEntry& e : mine.entries)
    {
        if(const Row* row = findRow(za::StringView{e.name.cStr()}))
        {
            Cvar_SetQuick(row->var, e.value.cStr());
        }
    }
    return true;
}

void saveMine()
{
    captureMine();
}

bool hasMine()
{
    loadMine();
    return mine.present;
}

int matchingLevel()
{
    for(int level = 0; level < levelCount; level++)
    {
        if(matches(level))
        {
            return level;
        }
    }
    return -1;
}

bool matchesMine()
{
    loadMine();
    if(!mine.present)
    {
        return false;
    }
    for(const MineEntry& e : mine.entries)
    {
        const Row* row = findRow(za::StringView{e.name.cStr()});
        if(row && !same(row->var->string, e.value.cStr()))
        {
            return false;
        }
    }
    return true;
}

const char* stateName()
{
    const int level = matchingLevel();
    const bool isMine = matchesMine();
    if(level >= 0)
    {
        return isMine ? va("%s (My Settings)", levelName(level)) : levelName(level);
    }
    return isMine ? "My Settings" : "Custom";
}

const char* menuStateLine()
{
    q_snprintf(menu.line, sizeof(menu.line), "Now: %s", stateName());
    return menu.line;
}

const char* menuNoteLine()
{
    if(realtime - menu.noteTime > 5.0)
    {
        return "";
    }
    return menu.note;
}

void menuApply(int level)
{
    const bool first = !hasMine();
    apply(level);
    setNote(first ? va("%s set. Your own settings were kept as My Settings.", levelName(level))
                  : va("%s set.", levelName(level)));
}

void menuRestoreMine()
{
    setNote(restoreMine() ? "My Settings set back."
                          : "None kept yet: they are kept the first time you pick a preset, or with Save as My Settings.");
}

void menuSaveMine()
{
    if(hasMine() && realtime - menu.saveArmedTime > 3.0)
    {
        menu.saveArmedTime = realtime;
        setNote("Press Save as My Settings again to replace the ones kept.");
        return;
    }
    menu.saveArmedTime = -100.0;
    saveMine();
    setNote("The settings as they are now kept as My Settings.");
}

void init()
{
    Cvar_SetCallback(&vr_graphics_preset, onPresetCvar);
    Cmd_AddCommand("vr_graphics_preset_save", saveCmd);
    Cmd_AddCommand("vr_graphics_preset_status", statusCmd);
}

} // namespace qvr::gfxpresets
