// vr_gameplaypresets.cpp -- the Gameplay preset and Classic Quake (vr_gameplaypresets.hpp).

#include "vr_gameplaypresets.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_gfxpresets.hpp"
#include "vr_gorepresets.hpp"
#include "vr_presets.hpp"
#include "vr_progs.hpp"
#include "vr_serverrules.hpp"
#include "vr_testkit.hpp"

#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Base/Strstr.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/StringView.hpp"


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
    {&vr_enemy_held_drops,              {"0",     D,     D}}, // a dying monster's own weapon to take (0: scenery)
    {&vr_ammobox_drops,                {"2",     "2",   D}}, // ammo boxes drop weapons (2: never)
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
    // A gun's own reloading mode (-1: follows vr_reload_mode): Classic leaves none to override the "no reloading".
    {&vr_reload_mode_shot,              {"-1",    D,     D}},
    {&vr_reload_mode_sshot,             {"-1",    D,     D}},
    {&vr_reload_mode_nail,              {"-1",    D,     D}},
    {&vr_reload_mode_snail,             {"-1",    D,     D}},
    {&vr_reload_mode_gl,                {"-1",    D,     D}},
    {&vr_reload_mode_prox,              {"-1",    D,     D}},
    {&vr_reload_mode_rl,                {"-1",    D,     D}},
    {&vr_reload_mode_light,             {"-1",    D,     D}},
    {&vr_reload_mode_widow,             {"-1",    D,     D}},
    {&vr_reload_mode_adplasma,          {"-1",    D,     D}},
    {&vr_reload_mode_npistol,           {"-1",    D,     D}},
    {&vr_reload_mode_revolver,          {"-1",    D,     D}},
    // Flat-screen play's own reloading (at the hips, whatever the modes above): Classic has none there either.
    {&vr_flat_reload,                   {"0",     D,     D}},
    // Recoil on everywhere (his call, 2026-11-15): the models' own baked kick is always cancelled (vr_kickcancel.cpp),
    // so at 0 a gun had no kick at all, not Quake's.
    {&vr_weapon_recoil,                 {D,       D,     D}},
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

// A row Classic Quake's press set: its value before and the value Classic gave it.
struct ClassicRow
{
    cvar_t* var{nullptr};
    za::String was;
    za::String set;
};

// Classic Quake's line: what the last press changed.
struct ClassicState
{
    char line[200]{};
    bool busy{false}; // vr_classic_quake set by the game itself (syncClassic): not a press
    // The three presets' rows as the first press found them (empty: none kept): the second press sets back exactly
    // those, not an older My Settings; a row changed by hand since keeps its new value. In memory only: after a
    // restart, Back is each preset's My Settings.
    za::Vector<ClassicRow> before;
};
ClassicState classic;

// vr_classic_roundtrip_test's state between its start and its end (frames apart).
struct RoundTrip
{
    struct Value
    {
        za::String name;
        za::String value;
    };
    bool started{false};
    int waits{0}; // frames waited for the map to load
    za::Vector<Value> cvars;
    za::Vector<Value> textures; // (value: the texels' hash)
};
RoundTrip roundTrip;

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
    Cvar_SetQuick(&vr_reload_mode_rl, "3"); // a gun's own override must not keep reloading alive at Classic
    t.check(!presetKind.hasMine(), name, "no My Settings before the first apply");
    presetKind.apply(levelClassic);
    t.check(presetKind.hasMine(), name, "the first apply keeps My Settings");
    t.check(presetKind.differing(levelClassic, true) == 0 && presetKind.matchingLevel() == levelClassic, name,
        "Classic sets every row");
    t.check(vr_agrunt_repl.value == 0.f && vr_portals_walk.value == 0.f && vr_reload_mode.value == 0.f, name,
        "Classic: no armed grunts, Quake's teleporters, no reloading");
    t.check(reloadModeBest() == 0, name, "Classic: no gun reloads, even with a per-gun override set before");
    t.check(vr_flat_reload.value == 0.f, name, "Classic: no reloading in flat-screen play either (vr_flat_reload 0)");
    t.check(vr_grunt_burst.value == 0.f && vr_agknight.value == 0.f && vr_agdknight.value == 0.f, name,
        "Classic: plain grunts (no burst rifle), plain knights");
    t.check(vr_weapon_recoil.value != 0.f, name, "Classic: the guns recoil (his call 2026-11-15)");
    t.check(vr_enemy_held_drops.value == 0.f && vr_enemy_drops.value == 2.f, name, "Classic: enemies drop no weapons");
    presetKind.apply(levelLight);
    t.check(presetKind.differing(levelLight, true) == 0 && presetKind.matchingLevel() == levelLight, name, "Light sets every row");
    t.check(vr_knockdown.value == 0.f && vr_portals_walk.value != 0.f && vr_reload_mode.value != 0.f && vr_crates.value != 0.f,
        name, "Light: no knockdowns, seamless teleporters, reloading and crates kept");
    t.check(vr_flat_reload.value != 0.f, name, "Light: flat-screen reloading kept");
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
    t.check(presetKind.matchingLevel() < 0 && !ZA_STRCMP(presetKind.stateName(), "Custom"), name, "a change by hand shows Custom");

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
    t.check(ZA_STRSTR(classic.line, "Graphics Off (Quake)") && ZA_STRSTR(classic.line, "Gore Minimal") &&
                ZA_STRSTR(classic.line, "Gameplay Classic"),
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
    // His report 2026-11-15: Back sets back exactly what the first press found, not an older My Settings; a row changed
    // by hand while Classic was on keeps its new value.
    Cvar_SetQuick(&vr_bloom, "0.37");
    Cvar_SetValue("vr_classic_quake", 1.f);
    Cvar_SetValue("vr_classic_quake", 0.f);
    t.check(ZA_STRCMP(vr_bloom.string, "0.37") == 0 && presetKind.matchingLevel() == levelUnleashed, name,
        "Back: a setting changed after My Settings was kept is as the press found it");
    Cvar_SetValue("vr_classic_quake", 1.f);
    Cvar_SetQuick(&vr_bloom, "0.21");
    Cvar_SetValue("vr_classic_quake", 0.f);
    t.check(ZA_STRCMP(vr_bloom.string, "0.21") == 0 && presetKind.matchingLevel() == levelUnleashed, name,
        "Back: a setting changed by hand while Classic was on keeps its value");

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
    classic.before.clear(); // (the test's presses: none kept for the player's next)
    gorepresets::sync();
    syncClassic();
    t.verdict(name);
}

void roundTripCvar(RoundTrip& r)
{
    r.cvars.clear();
    for(const cvar_t* var = Cvar_FindVarAfter("", 0); var; var = var->next)
    {
        r.cvars.pushBack(RoundTrip::Value{za::String{var->name}, za::String{var->string}});
    }
}

void roundTripTexture(void* ctx, const char* texName, unsigned hash)
{
    char text[16];
    q_snprintf(text, sizeof(text), "%08x", hash);
    static_cast<za::Vector<RoundTrip::Value>*>(ctx)->pushBack(RoundTrip::Value{za::String{texName}, za::String{text}});
}

// The values of `now` that differ from `was`'s of the same name (the k-th of a name with the k-th), each printed; `seen`
// counts the names in both.
int roundTripDiffer(const za::Vector<RoundTrip::Value>& was, const za::Vector<RoundTrip::Value>& now, const char* what,
    int& seen)
{
    int differ = 0;
    seen = 0;
    za::Vector<bool> used;
    used.resize(now.size(), false);
    for(const RoundTrip::Value& a : was)
    {
        for(za::SizeT j = 0; j < now.size(); j++)
        {
            if(used[j] || ZA_STRCMP(now[j].name.cStr(), a.name.cStr()) != 0)
            {
                continue;
            }
            used[j] = true;
            seen++;
            if(ZA_STRCMP(now[j].value.cStr(), a.value.cStr()) != 0)
            {
                differ++;
                Con_Printf("classicroundtrip: %s %s was \"%s\", now \"%s\"\n", what, a.name.cStr(), a.value.cStr(),
                    now[j].value.cStr());
            }
            break;
        }
    }
    return differ;
}

// vr_classic_roundtrip_test: Classic Quake pressed twice, as the hub's board does (vr_setup_option classic), in a map;
// then every setting and every texture read from a file (their texels) as before (his report 2026-11-15: the gloves'
// relief flat after it: vr_alpha_coverage's reload had made it again without its normals). Starts, presses, and ends
// itself a few frames later (each press's changes applied at a frame's start: TexMgr_ApplySettings); prints
// "classicroundtrip: N passed, M failed".
void roundTripCmd()
{
    constexpr const char* name = "classicroundtrip";
    const bool end = Cmd_Argc() > 1 && !q_strcasecmp(Cmd_Argv(1), "end");
    if(!end)
    {
        if(cls.state != ca_connected || !cl.worldmodel || cls.signon != SIGNONS)
        {
            // (a map still loading: tried again next frame, for at most ~10 seconds of frames)
            constexpr int maxWaits = 720;
            if(sv.active && roundTrip.waits++ < maxWaits)
            {
                Cbuf_AddText("wait\nvr_classic_roundtrip_test\n");
                return;
            }
            roundTrip.waits = 0;
            testkit::Tally t;
            t.check(false, name, "a map loaded (timed out)");
            t.verdict(name);
            return;
        }
        roundTrip.waits = 0;
        gorepresets::sync();
        syncClassic();
        roundTripCvar(roundTrip);
        roundTrip.textures.clear();
        TexMgr_FileTextureHashes(roundTripTexture, &roundTrip.textures);
        roundTrip.started = true;
        Cbuf_AddText("vr_setup_option classic\nwait\nwait\nvr_setup_option classic\nwait\nwait\nvr_classic_roundtrip_test end\n");
        return;
    }
    testkit::Tally t;
    t.check(roundTrip.started, name, "started (vr_classic_roundtrip_test, no argument)");
    roundTrip.started = false;
    gorepresets::sync();
    syncClassic();
    RoundTrip now;
    roundTripCvar(now);
    TexMgr_FileTextureHashes(roundTripTexture, &now.textures);
    int cvarsSeen = 0;
    int texSeen = 0;
    const int cvarsDiffer = roundTripDiffer(roundTrip.cvars, now.cvars, "setting", cvarsSeen);
    const int texDiffer = roundTripDiffer(roundTrip.textures, now.textures, "texture", texSeen);
    bool relief = false;
    for(const RoundTrip::Value& v : roundTrip.textures)
    {
        relief = relief || za::StringView{v.name.cStr()}.endsWith(za::StringView{"_glove_height"});
    }
    Con_Printf("classicroundtrip: %d settings, %d textures compared; last press: %s\n", cvarsSeen, texSeen, classic.line);
    t.check(!classicActive() && vr_classic_quake.value == 0.f, name, "Classic Quake off again");
    t.check(cvarsSeen > 1000 && cvarsDiffer == 0, name, "every setting as before");
    t.check(relief, name, "the gloves' relief loaded (progs/hand_rig_0N_00_glove_height)");
    t.check(texSeen > 10 && texDiffer == 0, name, "every texture read from a file as before (the gloves' relief too)");
    roundTrip.cvars.clear();
    roundTrip.textures.clear();
    t.verdict(name);
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
    if(const char* why = serverrules::lockedBecause())
    {
        Con_Printf("vr_gameplay_preset: not here. %s\n", why);
        return;
    }
    presetKind.menuApply(level);
}

void menuRestoreMine()
{
    if(const char* why = serverrules::lockedBecause())
    {
        Con_Printf("My Settings: not here. %s\n", why);
        return;
    }
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
    presetcore::Kind* const kinds[] = {&gfxpresets::kind(), &gorepresets::kind(), &presetKind};
    const bool keep = classic.before.empty(); // (pressed again while shown off: the first press's values stay)
    if(keep)
    {
        for(presetcore::Kind* k : kinds)
        {
            for(int i = 0; i < k->table().rowCount; i++)
            {
                cvar_t* var = k->table().rows[i].var;
                classic.before.pushBack(ClassicRow{var, za::String{var->string}, za::String{}});
            }
        }
    }
    gfxpresets::apply(0);
    gorepresets::apply(gorepresets::choiceMinimal);
    presetKind.apply(levelClassic);
    for(ClassicRow& row : classic.before)
    {
        row.set = za::String{row.var->string};
    }
    q_snprintf(classic.line, sizeof(classic.line), "Set Graphics Off (Quake), Gore Minimal, Gameplay Classic (were %s, %s, %s).",
        was[0], was[1], was[2]);
    syncClassic();
}

void classicBack()
{
    if(!classic.before.empty())
    {
        // His report 2026-11-15: the round trip sets back exactly what the first press found.
        for(const ClassicRow& row : classic.before)
        {
            if(ZA_STRCMP(row.var->string, row.set.cStr()) == 0)
            {
                Cvar_SetQuick(row.var, row.was.cStr());
            }
        }
        classic.before.clear();
        gorepresets::sync();
        q_snprintf(classic.line, sizeof(classic.line), "Your settings back: Graphics %s, Gore %s, Gameplay %s.",
            gfxpresets::stateName(), gorepresets::stateName(), presetKind.stateName());
        syncClassic();
        return;
    }
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

// vr_gameplay_classic_check: the running game under Classic (QC VR_Classic_Check: no gun reloads, no burst rifle, e1m1-like
// maps' grunts all plain); prints "classiccheck: N passed, M failed".
void classicCheckCmd()
{
    testkit::Tally t;
    constexpr const char* name = "classiccheck";
    t.check(presetKind.matchingLevel() == levelClassic, name, "the Gameplay preset is Classic");
    bool qc = false;
    if(sv.active)
    {
        const progs::ServerVm vm;
        dfunction_t* fn = VR_ED_FindFunction("VR_Classic_Check");
        qc = fn && progs::qcFloat(static_cast<func_t>(fn - qcvm->functions)) != 0.f;
    }
    t.check(qc, name, "the game: no reloading, no armed humanoids, plain grunts");
    // His report 2026-11-15: no recoil in Classic (the models' baked kick is always cancelled: vr_kickcancel.cpp).
    t.check(vr_weapon_recoil.value != 0.f, name, "the guns recoil (vr_weapon_recoil on)");
    t.check(vr_enemy_held_drops.value == 0.f && vr_enemy_drops.value == 2.f, name,
        "enemies drop no weapons (vr_enemy_held_drops 0, vr_enemy_drops 2: never)");
    t.verdict(name);
}

// vr_classic_drop_check: a grunt gibbed by the player's side (QC VR_Classic_DropCheck: its backpack falls, its burst rifle
// to be taken or, at vr_enemy_held_drops 0, as scenery; a burst rifle in the hand bursts at vr_grunt_burst 0); prints
// "classicdrop: N passed, M failed". Any preset: it checks against vr_enemy_held_drops as set.
void classicDropCheckCmd()
{
    if(sv.active)
    {
        const progs::ServerVm vm;
        if(dfunction_t* fn = VR_ED_FindFunction("VR_Classic_DropCheck"))
        {
            // (Its check lines are dprint's: developer 1. It returns fails * 1000 + passes.)
            const int r = static_cast<int>(progs::qcFloat(static_cast<func_t>(fn - qcvm->functions)));
            testkit::verdict("classicdrop", r % 1000, r / 1000);
            return;
        }
    }
    testkit::Tally t;
    t.check(false, "classicdrop", "a game running (and a build with the tests)");
    t.verdict("classicdrop");
}

void init()
{
    Cvar_SetCallback(&vr_gameplay_preset, onPresetCvar);
    Cvar_SetCallback(&vr_classic_quake, onClassicCvar);
    Cmd_AddCommand("vr_gameplay_preset_save", saveCmd);
    Cmd_AddCommand("vr_gameplay_preset_status", statusCmd);
    Cmd_AddCommand("vr_gameplay_preset_test", testCmd);
    Cmd_AddCommand("vr_gameplay_classic_check", classicCheckCmd);
    Cmd_AddCommand("vr_classic_drop_check", classicDropCheckCmd);
    Cmd_AddCommand("vr_classic_roundtrip_test", roundTripCmd);
}

} // namespace qvr::gameplaypresets
