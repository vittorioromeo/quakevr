// vr_gameplaypresets.cpp -- the Gameplay preset and Classic Quake (vr_gameplaypresets.hpp).

#include "vr_gameplaypresets.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_gfxpresets.hpp"
#include "vr_gorepresets.hpp"
#include "vr_presets.hpp"
#include "vr_testkit.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/StringView.hpp"

#include <string.h>

namespace qvr::gameplaypresets
{
namespace
{

using presetcore::D;

// Every gameplay switch a preset sets: Classic, Light, Unleashed (D: the default, so Unleashed, all D, is the game as
// shipped). The author's decision row by row: docs/vr-port/GAMEPLAY_PRESET.md (a "?" there marks a taste call). Most of
// the QC's are read as a monster spawns or a map starts: a level applied mid-map shows fully from the next map.
// clang-format off
constexpr presetcore::Row rows[] = {
    //                                   Classic  Light  Unleashed
    // Monsters: who they are and what they carry
    {&vr_agrunt_repl,                   {"0",     "0",   D}}, // armed grunts and enforcers
    {&vr_agknight,                      {"0",     "0",   D}}, // armed knights
    {&vr_agdknight,                     {"0",     "0",   D}}, // armed death knights
    {&vr_grunt_burst,                   {"0",     "0",   D}}, // grunts' 3-round bursts (0: id's shotgun)
    {&vr_enemy_drops,                   {"2",     "2",   D}}, // monsters drop their weapons (2: never)
    {&vr_ammobox_drops,                 {"2",     "2",   D}}, // ammo boxes drop weapons (2: never)
    {&vr_adgun_drops,                   {"0",     "0",   D}}, // AD soldiers drop their guns
    {&vr_admelee_drops,                 {"0",     "0",   D}}, // AD melee monsters drop their weapons
    {&vr_monster_health,                {"0",     "0",   D}}, // big monsters' health cut (0: Quake's)
    // Monsters: how they fight and move
    {&vr_ai_enhanced,                   {"0",     "0",   D}}, // stealth AI (0: Quake's sight and hearing)
    {&vr_nav,                           {"0",     D,     D}}, // navmesh paths
    {&vr_mhull,                         {"0",     D,     D}}, // monsters' narrower hulls
    {&vr_enemy_shove,                   {"0",     "0",   D}}, // grunts, enforcers and vores shove you
    {&vr_enemy_melee_arc,               {"0",     D,     D}}, // a melee blow lands only in its arc
    {&vr_enemy_melee_space,             {"0",     D,     D}}, // melee attackers kept off your head
    {&vr_enemy_aim_cone,                {"0",     D,     D}}, // ranged shots leave along their facing
    {&vr_enemy_liquid_damage,           {"0",     D,     D}}, // monsters burn in lava, drown
    {&vr_portals_monsters,              {"0",     D,     D}}, // monsters walk through teleporters
    {&vr_portals_ai,                    {"0",     D,     D}}, // monsters see you through teleporters
    // Hits, knockdowns and bodies
    {&vr_knockdown,                     {"0",     "0",   D}},
    {&vr_kdtrig,                        {"0",     "0",   D}}, // ragdoll triggers (stairs, ledges, blasts)
    {&vr_hit_stagger,                   {"0",     "0",   D}},
    {&vr_ragimpact,                     {"0",     "0",   D}}, // flung bodies hurt what they hit
    {&vr_legsweep,                      {"0",     "0",   D}},
    {&vr_hit_push,                      {"0",     D,     D}}, // heavy hits shove monsters
    {&vr_kill_push,                     {"0",     D,     D}}, // killing blows throw the bodies
    {&vr_positional_damage,             {"0",     D,     D}}, // head and limb damage
    {&vr_ragdoll,                       {"0",     D,     D}},
    {&vr_ragdoll_active,                {"0",     "0",   D}}, // dying bodies stumble as active ragdolls
    {&vr_corpse_collide,                {"0",     D,     D}}, // corpses in the physics
    // Your fighting
    {&vr_parry,                         {"0",     D,     D}},
    {&vr_bash,                          {"0",     D,     D}},
    {&vr_counter,                       {"0",     D,     D}},
    {&vr_headbutt,                      {"0",     D,     D}},
    {&vr_deflect,                       {"0",     D,     D}}, // batting projectiles back
    {&vr_parry_stamina,                 {"0",     "0",   D}},
    {&vr_shove_stamina,                 {"0",     "0",   D}},
    {&vr_stamina_speed,                 {"0",     "0",   D}},
    {&vr_weight_stamina,                {"0",     "0",   D}},
    {&vr_climb_stamina,                 {"0",     "0",   D}},
    {&vr_foegrab,                       {"0",     D,     D}}, // holding living enemies
    {&vr_embed,                         {"0",     D,     D}}, // blades stick in bodies (0: thrown axes alone)
    {&vr_grenade_catch,                 {"0",     D,     D}}, // grenades as physics (0: Quake's)
    {&vr_grenade_shoot,                 {"0",     D,     D}}, // shots set grenades off
    {&vr_bullettime_enabled,            {"0",     "0",   D}},
    {&vr_reload_mode,                   {"0",     D,     D}}, // 0: the guns fire from the reserve
    {&vr_weapon_recoil,                 {"0",     D,     D}},
    // The world
    {&vr_portals_walk,                  {"0",     D,     D}}, // seamless teleporters (0: Quake's teleport)
    {&vr_teleporter_pair_exits,         {"0",     D,     D}},
    {&vr_crates,                        {"0",     D,     D}},
    {&vr_explobox_physics,              {"0",     D,     D}}, // explosive boxes pushed and carried (0: fixed, id's)
    {&vr_debris_rocks,                  {"0",     D,     D}},
    {&vr_debris_bricks,                 {"0",     D,     D}},
    {&vr_closequarters,                 {"0",     "0",   D}}, // slowed walking into a monster
    {&vr_lootbeams,                     {"0",     D,     D}},
};
// clang-format on

constexpr int rowCount = static_cast<int>(sizeof(rows) / sizeof(rows[0]));
constexpr const char* levelNames[levelCount] = {"Classic", "Light", "Unleashed"};
constexpr presetcore::Table table{"gameplay", "VR Settings > Gameplay Presets", rows, rowCount, levelNames, levelCount};
presetcore::Kind presetKind{table};

// Classic Quake's line: what the last press changed.
struct ClassicState
{
    char line[200]{};
    bool busy{false}; // vr_classic_quake set by the game itself (syncClassic): not a press
};
ClassicState classic;

void onPresetCvar(cvar_t* var)
{
    if(var->value < 0.f)
    {
        return;
    }
    const int level = static_cast<int>(var->value);
    if(level == levelMine)
    {
        if(!presetKind.restoreMine())
        {
            Con_Printf("vr_gameplay_preset: no My Settings kept yet\n");
        }
    }
    else
    {
        presetKind.apply(level);
    }
    Cvar_SetQuick(var, "-1");
}

void onClassicCvar(cvar_t* var)
{
    if(classic.busy)
    {
        return;
    }
    if(var->value != 0.f)
    {
        classicApply();
    }
    else
    {
        classicBack();
    }
    Con_Printf("%s\n", classic.line);
}

void saveCmd()
{
    presetKind.saveMine();
    Con_Printf("gameplay preset: My Settings saved (%d settings, %s)\n", rowCount, presetKind.minePath().cStr());
}

void statusCmd()
{
    presetKind.printStatus();
}

// vr_gameplay_preset_test: every level sets every row; Unleashed is each default; a change by hand shows Custom; My
// Settings kept on the first apply and set back exactly; Save as My Settings; the console's button; Classic Quake sets
// the three presets and Back to My Settings sets the three back. The players' own My Settings untouched (the test's in
// <what>_my_settings_test.cfg), every setting as it was after.
void testCmd()
{
    testkit::Tally t;
    constexpr const char* name = "gameplaypreset";
    presetcore::Kind* kinds[] = {&presetKind, &gfxpresets::kind(), &gorepresets::kind()};

    // As they are now, to set back after.
    za::Vector<za::String> before;
    for(presetcore::Kind* k : kinds)
    {
        k->useTestFile(true);
        for(int i = 0; i < k->table().rowCount; i++)
        {
            before.pushBack(za::String{k->table().rows[i].var->string});
        }
    }

    // The table: no setting of another kind's (a preset never touches graphics or gore), none of comfort,
    // calibration, the body, controls or sound.
    int overlap = 0;
    int forbidden = 0;
    constexpr const char* forbiddenPrefixes[] = {"vr_comfort", "vr_snap", "vr_turn", "vr_holster", "vr_body", "vr_handcal",
        "vr_height", "vr_world_scale", "vr_snd", "vr_gore", "vr_blood", "vr_menu_hands", "vr_stick", "vr_movement"};
    for(const presetcore::Row& row : rows)
    {
        for(int k = 1; k < 3; k++)
        {
            for(int i = 0; i < kinds[k]->table().rowCount; i++)
            {
                overlap += kinds[k]->table().rows[i].var == row.var ? 1 : 0;
            }
        }
        for(const char* p : forbiddenPrefixes)
        {
            if(za::StringView{row.var->name}.startsWith(za::StringView{p}))
            {
                forbidden++;
                Con_Printf("gameplaypreset: %s is not a gameplay setting\n", row.var->name);
            }
        }
    }
    t.check(overlap == 0, name, "no row of the Graphics or Gore preset's");
    t.check(forbidden == 0, name, "no comfort, calibration, body, controls, sound or gore row");

    // A setting of your own, then the first apply keeps it as My Settings.
    Cvar_SetQuick(&vr_knockdown, "0");
    Cvar_SetQuick(&vr_crates, "0");
    t.check(!presetKind.hasMine(), name, "no My Settings before the first apply");
    presetKind.apply(levelClassic);
    t.check(presetKind.hasMine(), name, "the first apply keeps My Settings");
    t.check(presetKind.differing(levelClassic, true) == 0 && presetKind.matchingLevel() == levelClassic, name,
        "Classic sets every row");
    t.check(vr_agrunt_repl.value == 0.f && vr_portals_walk.value == 0.f && vr_reload_mode.value == 0.f, name,
        "Classic: no armed grunts, Quake's teleporters, no reloading");
    presetKind.apply(levelLight);
    t.check(presetKind.differing(levelLight, true) == 0 && presetKind.matchingLevel() == levelLight, name, "Light sets every row");
    t.check(vr_knockdown.value == 0.f && vr_portals_walk.value != 0.f && vr_reload_mode.value != 0.f && vr_crates.value != 0.f,
        name, "Light: no knockdowns, seamless teleporters, reloading and crates kept");
    presetKind.apply(levelUnleashed);
    int notDefault = 0;
    for(const presetcore::Row& row : rows)
    {
        if(!presetcore::same(row.var->string, row.var->default_string ? row.var->default_string : "0"))
        {
            notDefault++;
            Con_Printf("gameplaypreset: %s %s, default %s\n", row.var->name, row.var->string, row.var->default_string);
        }
    }
    t.check(notDefault == 0 && presetKind.matchingLevel() == levelUnleashed, name, "Unleashed is every row's default");

    // Custom: one row changed by hand.
    Cvar_SetQuick(&vr_bash, "0");
    t.check(presetKind.matchingLevel() < 0 && !strcmp(presetKind.stateName(), "Custom"), name, "a change by hand shows Custom");

    // My Settings back exactly.
    t.check(presetKind.restoreMine() && presetKind.matchesMine(), name, "My Settings set back");
    t.check(vr_knockdown.value == 0.f && vr_crates.value == 0.f && presetcore::same(vr_bash.string, vr_bash.default_string),
        name, "My Settings are the settings before the first apply");

    // Save as My Settings, and the console's button.
    Cvar_SetQuick(&vr_bash, "0");
    presetKind.saveMine();
    Cvar_SetValue("vr_gameplay_preset", static_cast<float>(levelLight));
    t.check(presetKind.matchingLevel() == levelLight && vr_gameplay_preset.value == -1.f, name,
        "vr_gameplay_preset 1 applies Light and goes back to -1");
    Cvar_SetValue("vr_gameplay_preset", static_cast<float>(levelMine));
    t.check(vr_bash.value == 0.f && presetKind.matchesMine(), name, "vr_gameplay_preset 3: the saved My Settings");

    // Classic Quake: the three presets, the line naming them; Back to My Settings: the three back.
    gfxpresets::apply(gfxpresets::levelHigh);
    gfxpresets::kind().saveMine();
    gorepresets::apply(gorepresets::choiceFull);
    gorepresets::kind().saveMine();
    Cvar_SetQuick(&vr_gameplay_preset, "-1");
    presetKind.apply(levelUnleashed);
    presetKind.saveMine();
    Cvar_SetValue("vr_classic_quake", 1.f);
    Con_Printf("gameplaypreset: Classic Quake: %s\n", classic.line);
    t.check(gfxpresets::matchingLevel() == 0, name, "Classic Quake: Graphics Off (Quake)");
    t.check(gorepresets::matching() == gorepresets::choiceMinimal, name, "Classic Quake: Gore Minimal");
    t.check(presetKind.matchingLevel() == levelClassic, name, "Classic Quake: Gameplay Classic");
    syncClassic();
    t.check(classicActive() && vr_classic_quake.value == 1.f, name, "Classic Quake shown on");
    t.check(strstr(classic.line, "Graphics Off (Quake)") && strstr(classic.line, "Gore Minimal") &&
                strstr(classic.line, "Gameplay Classic"),
        name, "its line names the three");
    Cvar_SetQuick(&vr_crates, "1");
    syncClassic();
    t.check(!classicActive() && vr_classic_quake.value == 0.f, name, "a change by hand: Classic Quake shown off");
    Cvar_SetValue("vr_classic_quake", 1.f);
    Cvar_SetValue("vr_classic_quake", 0.f);
    Con_Printf("gameplaypreset: Back: %s\n", classic.line);
    t.check(gfxpresets::matchesMine() && gfxpresets::matchingLevel() == gfxpresets::levelHigh, name, "Back: Graphics My Settings");
    t.check(gorepresets::kind().matchesMine() && gorepresets::matching() == gorepresets::choiceFull, name, "Back: Gore My Settings");
    t.check(presetKind.matchesMine() && presetKind.matchingLevel() == levelUnleashed, name, "Back: Gameplay My Settings");

    // As they were, and the players' own My Settings again.
    int i = 0;
    for(presetcore::Kind* k : kinds)
    {
        for(int r = 0; r < k->table().rowCount; r++)
        {
            Cvar_SetQuick(k->table().rows[r].var, before[i++].cStr());
        }
        k->useTestFile(false);
    }
    gorepresets::sync();
    syncClassic();
    t.verdict(name);
}

} // namespace

const char* levelName(int level)
{
    return presetKind.levelName(level);
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

void classicApply()
{
    char was[3][64];
    q_strlcpy(was[0], gfxpresets::stateName(), sizeof(was[0]));
    q_strlcpy(was[1], gorepresets::stateName(), sizeof(was[1]));
    q_strlcpy(was[2], presetKind.stateName(), sizeof(was[2]));
    gfxpresets::apply(0);
    gorepresets::apply(gorepresets::choiceMinimal);
    presetKind.apply(levelClassic);
    q_snprintf(classic.line, sizeof(classic.line), "Set Graphics Off (Quake), Gore Minimal, Gameplay Classic (were %s, %s, %s).",
        was[0], was[1], was[2]);
    syncClassic();
}

void classicBack()
{
    const bool g = gfxpresets::restoreMine();
    const bool b = gorepresets::restoreMine();
    const bool p = presetKind.restoreMine();
    q_snprintf(classic.line, sizeof(classic.line), "My Settings back: Graphics %s, Gore %s, Gameplay %s.",
        g ? gfxpresets::stateName() : "(none kept)", b ? gorepresets::stateName() : "(none kept)",
        p ? presetKind.stateName() : "(none kept)");
    syncClassic();
}

bool classicActive()
{
    return gfxpresets::matchingLevel() == 0 && gorepresets::matching() == gorepresets::choiceMinimal &&
           presetKind.matchingLevel() == levelClassic;
}

const char* classicLine()
{
    if(classic.line[0])
    {
        return classic.line;
    }
    return classicActive() ? "On: Graphics Off (Quake), Gore Minimal, Gameplay Classic."
                           : "Sets Graphics Off (Quake), Gore Minimal and Gameplay Classic.";
}

void syncClassic()
{
    const float shown = classicActive() ? 1.f : 0.f;
    if(vr_classic_quake.value != shown)
    {
        classic.busy = true;
        Cvar_SetValueQuick(&vr_classic_quake, shown);
        classic.busy = false;
    }
}

void init()
{
    Cvar_SetCallback(&vr_gameplay_preset, onPresetCvar);
    Cvar_SetCallback(&vr_classic_quake, onClassicCvar);
    Cmd_AddCommand("vr_gameplay_preset_save", saveCmd);
    Cmd_AddCommand("vr_gameplay_preset_status", statusCmd);
    Cmd_AddCommand("vr_gameplay_preset_test", testCmd);
}

} // namespace qvr::gameplaypresets
