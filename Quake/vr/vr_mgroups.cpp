// vr_mgroups.cpp -- monster groups (vr_mgroups.hpp; the groups: vr_mgroups.inc; their values: quakevr/data/groups.txt).

#include "vr_mgroups.hpp"
#include "vr_cvars.hpp"
#include "vr_progs.hpp"
#include "vr_slottable.hpp"

#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"

#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>

namespace qvr::mgroups
{

namespace
{

struct Member
{
    const char* classname;
    int group;
};

// Every monster's classname in the games Quake VR plays (id1, Scourge of Armagon, Dissolution of Eternity, Honey, Dawn
// of the Machine, Arcane Dimensions and the Quoth classes AD plays: AD's own ad_monster_* spawns, their classnames kept),
// by group. Dawn of the Machine's infected and the training dummy have their stock monster's classname. Not here: Others.
constexpr Member memberTable[] = {
    // Grunts: id's, Honey's, the infected; AD's rocket, grenade and plasma soldiers.
    {"monster_army", armyGroup}, {"monster_soldier", armyGroup}, {"monster_army_infected", armyGroup},
    {"monster_army_rocket", armyGroup}, {"monster_army_grenade", armyGroup}, {"monster_army_plasma", armyGroup},
    // Enforcers: id's, the infected; AD's defender, eliminator, pyro, fumigator, deflector and centurion (on its hover
    // board: a flyer).
    {"monster_enforcer", enforcerGroup}, {"monster_base_enforcer", enforcerGroup},
    {"monster_enforcer_infected", enforcerGroup}, {"monster_defender", enforcerGroup},
    {"monster_eliminator", enforcerGroup}, {"monster_pyro", enforcerGroup}, {"monster_fumigator", enforcerGroup},
    {"monster_deflector", enforcerGroup}, {"monster_centurion", enforcerGroup},
    // Knights: id's, the infected; AD's crossbow knight.
    {"monster_knight", knightGroup}, {"monster_knight_infected", knightGroup}, {"monster_dcrossbow", knightGroup},
    // Death knights: id's (AD's point knight), the infected, Dawn of the Machine's ranged knight; AD's death knight (and
    // its ghost, Quoth's death knight), death guard (Quoth's too). (The Dark Prince, the death knight's mesh
    // and size: AD's FGD calls it a "mini boss", as the baron, but its code is a plain monster's: 400 health, no boss
    // flags or script, placed in no map of AD's.)
    {"monster_hell_knight", hknightGroup}, {"monster_hell_knight_infected", hknightGroup},
    {"monster_hell_pointknight", hknightGroup}, {"monster_ranged_knight", hknightGroup},
    {"monster_dknight", hknightGroup}, {"monster_dknightghost", hknightGroup}, {"monster_death_knight", hknightGroup},
    {"monster_dguard", hknightGroup}, {"monster_death_guard", hknightGroup},
    // Mini Bosses (the author, 2026-10-23): the Dark Prince, AD's big walkers (the minotaur, the stone golem, the drole:
    // active ragdolls, their rigs a humanoid's legs), the justice knight (AD's normal judicator; the baron, its old name).
    // Elite Knights: the death fury, the death sergeant and the death lord (400 health each).
    {"monster_dprince", minibossGroup}, {"monster_minotaur", minibossGroup}, {"monster_golem", minibossGroup},
    {"monster_drole", minibossGroup}, {"monster_justice", minibossGroup}, {"monster_baron", minibossGroup},
    {"monster_dfury", eliteknightGroup}, {"monster_dsergeant", eliteknightGroup}, {"monster_death_lord", eliteknightGroup},
    // Ogres: id's, Scourge of Armagon's marksman, Dawn of the Machine's rocket ogre; AD's flak ogre, hunter ogres (cleaver,
    // mace, hammer), the fishing ones (scenery), Freddie (Quoth's Edie).
    {"monster_ogre", ogreGroup}, {"monster_ogre_real", ogreGroup}, {"monster_ogre_marksman", ogreGroup},
    {"monster_ogre_rocket", ogreGroup}, {"monster_ogre_flak", ogreGroup}, {"monster_hogre", ogreGroup},
    {"monster_hogremac", ogreGroup}, {"monster_hogreham", ogreGroup}, {"monster_ogre_fishing", ogreGroup},
    {"monster_ogre_fishing_ghost", ogreGroup}, {"monster_freddie", ogreGroup}, {"monster_edie", ogreGroup},
    // Zombies: id's (AD's), AD's zombie knight, Dawn of the Machine's Shub zombie.
    {"monster_zombie", zombieGroup}, {"monster_zombiek", zombieGroup}, {"monster_szombie", zombieGroup},
    // Mummies: Dissolution of Eternity's (AD's).
    {"monster_mummy", mummyGroup},
    // Rottweilers: id's; Dawn of the Machine's demon dog.
    {"monster_dog", dogGroup}, {"monster_demodog", dogGroup},
    {"monster_demon1", demonGroup},
    // Shamblers: id's, Dawn of the Machine's super shambler.
    {"monster_shambler", shamblerGroup}, {"monster_super_shambler", shamblerGroup},
    {"monster_gremlin", gremlinGroup},
    // AD's skull wizard (a robe, no legs) and its minions' pool.
    {"monster_skullwiz", skullwizGroup}, {"monster_skullwizminion", skullwizGroup},
    // AD's spiders, scorpion, steel claw, swampling and voreling (the vore's voreling pool).
    {"monster_spider", crawlerGroup}, {"monster_scorpion", crawlerGroup}, {"monster_steelclaw", crawlerGroup},
    {"monster_swampling", crawlerGroup}, {"monster_voreling", crawlerGroup}, {"monster_shalrathminion", crawlerGroup},
    // Scrags (id's, AD's, Nour's minions) and AD's gargoyle (the minotaur's minions) and gaunt (the judicator's, Quoth's
    // polyp).
    {"monster_wizard", wizardGroup}, {"monster_nourminion", wizardGroup}, {"monster_gargoyle", wizardGroup},
    {"monster_minotaurminion", wizardGroup}, {"monster_gaunt", wizardGroup}, {"monster_judgeminion", wizardGroup},
    {"monster_polyp", wizardGroup},
    {"monster_shalrath", shalrathGroup},
    // Spawns: id's, Dawn of the Machine's slime; AD's boil.
    {"monster_tarbaby", tarbabyGroup}, {"monster_slime", tarbabyGroup}, {"monster_boil", tarbabyGroup},
    // Rotfish (id's, AD's), eels (Dissolution of Eternity's, AD's).
    {"monster_fish", fishGroup}, {"monster_eel", fishGroup},
    // Floating and flying: Dissolution of Eternity's wrath, overlord and sword; Scourge of Armagon's spike mine; Dawn of
    // the Machine's orb, ghost and Shub's seekers; AD's wraith (and its minions), lost soul, sentinel, sword, Jim
    // (Quoth's Bob).
    {"monster_wrath", floaterGroup}, {"monster_super_wrath", floaterGroup}, {"monster_sword", floaterGroup},
    {"monster_spikemine", floaterGroup}, {"monster_orb", floaterGroup}, {"monster_ghost", floaterGroup},
    {"monster_shub_seeker", floaterGroup}, {"monster_wraith", floaterGroup}, {"monster_wraithminion", floaterGroup},
    {"monster_wraithminion2", floaterGroup}, {"monster_lostsoul", floaterGroup}, {"monster_sentinel", floaterGroup},
    {"monster_jim", floaterGroup}, {"monster_bob", floaterGroup},
    // Machines: Scourge of Armagon's centroid; AD's seeker (the juggernaut), Floyd, sweeper, ball turret.
    {"monster_scourge", machineGroup}, {"monster_seeker", machineGroup}, {"monster_juggernaut", machineGroup},
    {"monster_floyd", machineGroup}, {"monster_sweeper", machineGroup}, {"monster_turretbal", machineGroup},
    // Bosses: Chthon and Shub (id's, Dawn of the Machine's, AD's), Armagon, the dragon, Hephaestus (the lava man; Dawn of
    // the Machine's too), the morph, Honey's spear knight, the bog lord (Quoth's gug); AD's eidolon, fire top, ice golem, judicator, mammoth, Nour
    // (Quoth's vermis).
    {"monster_boss", bossGroup}, {"monster_boss_final", bossGroup}, {"monster_chthon", bossGroup},
    {"monster_oldone", bossGroup}, {"monster_oldone_new", bossGroup}, {"monster_shub", bossGroup},
    {"monster_shubupsd", bossGroup}, {"monster_shub_eye", bossGroup}, {"monster_armagon", bossGroup},
    {"monster_dragon", bossGroup}, {"monster_lava_man", bossGroup}, {"monster_lava_man_mg3", bossGroup},
    {"monster_morph", bossGroup}, {"monster_sknight", bossGroup}, {"monster_eidolon", bossGroup},
    {"monster_eidolon_wavetrig", bossGroup}, {"monster_firetopboss", bossGroup}, {"monster_icegolem", bossGroup},
    {"monster_icegolem_wavetrig", bossGroup}, {"monster_judicator", bossGroup}, {"monster_mammoth", bossGroup},
    {"monster_nour", bossGroup}, {"monster_nouronihar", bossGroup}, {"monster_vermis", bossGroup},
    {"monster_boglord", bossGroup}, {"monster_gug", bossGroup}, {"monster_vomit", bossGroup}, // (Dissolution of Eternity's Vomitus: cut, named still)
    // Others: AD's Christmas ones, Scourge of Armagon's decoy (a player's double), AD's fade.
    {"monster_xmas_elf", otherGroup}, {"monster_xmas_raindeer", otherGroup}, {"monster_xmas_santa", otherGroup},
    {"monster_santa", otherGroup}, {"monster_xmas_snowman", otherGroup}, {"monster_snowman", otherGroup},
    {"monster_decoy", otherGroup}, {"monster_fade", otherGroup},
};

struct GroupInfo
{
    const char* id;
    const char* label;
};

constexpr GroupInfo groupInfo[] = {
#define QVR_MGROUP(g, label, ...) {#g, label},
#include "vr_mgroups.inc"
#undef QVR_MGROUP
};

// The groups' ids: groups.txt's blocks' names.
constexpr const char* groupIds[groupCount] = {
#define QVR_MGROUP(g, ...) #g,
#include "vr_mgroups.inc"
#undef QVR_MGROUP
};

// Each group's values (quakevr/data/groups.txt; ROUND23.md, "Monster groups as data"): no cvars, a stand-in each
// (vr_slottable.hpp DataTable) named as its old cvar (oldName), reached by that name by the config, the console and the
// QC's cvar handles; the menus' rows bind them (setting).
constexpr const char* dataPath = "data/groups.txt";

// groups.txt's keys, in Param's order (Misc/quakevr/groups_data.py KEYS: keep them alike).
constexpr const char* paramKeys[paramCount] = {"kdtrig", "resist", "ledge", "stairs", "slope", "shotgun", "wounded",
    "blow", "blast", "thrown", "tackle", "shove", "push", "stagger", "sweep", "leggrab", "lift", "necksnap", "manhandle",
    "throw", "hold", "poise", "health", "kdscale", "downtime", "getup", "snapkills", "struggle", "struggle_strength",
    "struggle_speed", "struggle_arms", "struggle_hurt"};

cvar_t standIns[groupCount * paramCount]{};
DataTable<Param, groupCount, paramCount> values{standIns};

// A value's old cvar name: vr_mgrp_<id>_<key>; the resistance's vr_kdtrig_resist_<id>, the shove's
// vr_knockdown_chance_<id> (older than the groups).
[[nodiscard]] const char* oldName(int g, int p)
{
    if(p == static_cast<int>(Param::Resist))
    {
        return va("vr_kdtrig_resist_%s", groupIds[g]);
    }
    if(p == static_cast<int>(Param::Shove))
    {
        return va("vr_knockdown_chance_%s", groupIds[g]);
    }
    return va("vr_mgrp_%s_%s", groupIds[g], paramKeys[p]);
}

[[nodiscard]] bool changed(const cvar_t& v)
{
    return strcmp(v.string, v.default_string) != 0;
}

// vr_mgroup_dump: each group's values in groups.txt and how many of them differ from it; vr_mgroup_dump <id> ... (or
// all): each value, its old cvar name and groups.txt's default.
void dump_f()
{
    const bool list = Cmd_Argc() < 2;
    const bool all = !list && !q_strcasecmp(Cmd_Argv(1), "all");
    for(int g = 0; g < groupCount; g++)
    {
        if(list)
        {
            int own = 0;
            for(int p = 0; p < paramCount; p++)
            {
                own += changed(values.at(g, static_cast<Param>(p)));
            }
            Con_Printf("%-12s %-22s %d values, %d changed\n", groupIds[g], groupInfo[g].label, paramCount, own);
            continue;
        }
        bool wanted = all;
        for(int a = 1; a < Cmd_Argc() && !wanted; a++)
        {
            wanted = !q_strcasecmp(Cmd_Argv(a), groupIds[g]);
        }
        if(!wanted)
        {
            continue;
        }
        Con_Printf("group %s // %s\n", groupIds[g], groupInfo[g].label);
        for(int p = 0; p < paramCount; p++)
        {
            const cvar_t& v = values.at(g, static_cast<Param>(p));
            Con_Printf("    %s \"%s\" // %s, default \"%s\"%s\n", paramKeys[p], v.string, v.name, v.default_string,
                changed(v) ? " *" : "");
        }
    }
    if(list)
    {
        Con_Printf("vr_mgroup_dump <id> ... | all: every value (%s)\n", dataPath);
    }
}

// Monster health (healthMultiplier): the bosses whose health is not a pool to shoot down, left as they are. Killed by a
// trick (id's Chthon by lightning, Shub by telefrag, and Arcane Dimensions' and Dawn of the Machine's own), or fought in
// waves and phases whose health their code sets as they go (Arcane Dimensions' eidolon, ice golem, judicator, mammoth,
// Nour, fire top; Dawn of the Machine's Chthon and Shub, set as they wake). The other bosses (Armagon, the dragon,
// Hephaestus, the morph, the spear knight, the bog lord) are cut as any monster.
constexpr const char* healthExempt[] = {
    "monster_boss", "monster_chthon", "monster_boss_final", "monster_oldone", "monster_oldone_new", "monster_shub",
    "monster_shubupsd", "monster_shub_eye", "monster_eidolon", "monster_eidolon_wavetrig", "monster_icegolem",
    "monster_icegolem_wavetrig", "monster_judicator", "monster_mammoth", "monster_nour", "monster_nouronihar",
    "monster_vermis", "monster_firetopboss",
};

// Health this high is a scripted invulnerability (Arcane Dimensions' 50000), not a pool: left alone.
constexpr float healthScripted = 20000.f;

[[nodiscard]] bool healthExemptClass(const char* classname)
{
    for(const char* c : healthExempt)
    {
        if(classname && std::strcmp(c, classname) == 0)
        {
            return true;
        }
    }
    return false;
}

// The menu's examples (healthExampleLine): a monster of each size, with its usual health (normal skill).
struct HealthExample
{
    const char* classname;
    const char* label;
    float health;
};
constexpr HealthExample healthExamples[] = {
    {"monster_army", "Grunt", 30.f}, {"monster_knight", "Knight", 75.f}, {"monster_ogre", "Ogre", 200.f},
    {"monster_hell_knight", "Death Knight", 250.f}, {"monster_demon1", "Fiend", 300.f},
    {"monster_shalrath", "Vore", 400.f}, {"monster_minotaur", "Minotaur (AD)", 500.f},
    {"monster_shambler", "Shambler", 600.f}, {"monster_lava_man", "Hephaestus", 1500.f},
    {"monster_armagon", "Armagon", 2000.f}, {"monster_boss", "Chthon (left alone)", 3.f},
};
constexpr int healthExampleCount = static_cast<int>(sizeof(healthExamples) / sizeof(healthExamples[0]));
char healthExampleText[healthExampleCount][96];

// The menu's member lists (memberText), made once.
char memberText[groupCount][768];
bool memberTextMade = false;

void makeMemberText()
{
    for(int g = 0; g < groupCount; ++g)
    {
        char* out = memberText[g];
        size_t used = 0;
        out[0] = '\0';
        for(const Member& m : memberTable)
        {
            if(m.group != g)
            {
                continue;
            }
            const int n = std::snprintf(out + used, sizeof(memberText[g]) - used, "%s%s", used ? ", " : "",
                m.classname + (std::strncmp(m.classname, "monster_", 8) == 0 ? 8 : 0));
            if(n < 0 || used + static_cast<size_t>(n) >= sizeof(memberText[g]))
            {
                break;
            }
            used += static_cast<size_t>(n);
        }
        if(g == otherGroup)
        {
            std::snprintf(out + used, sizeof(memberText[g]) - used, "%sany monster in no other group", used ? "; " : "");
        }
        else if(!used)
        {
            // (Giants: its drole and stone golem are Mini Bosses since 2026-10-23; kept for its settings' names.)
            std::snprintf(out, sizeof(memberText[g]), "no monster now");
        }
    }
    memberTextMade = true;
}

// "vr_mgroup_list": every classname's group, then this map's monsters' (each its edict, classname, group, and whether
// its classname is in the table).
void list_f()
{
    for(int g = 0; g < groupCount; ++g)
    {
        Con_Printf("mgroup %s (%s):", groupInfo[g].id, groupInfo[g].label);
        for(const Member& m : memberTable)
        {
            if(m.group == g)
            {
                Con_Printf(" %s", m.classname);
            }
        }
        Con_Printf("\n");
    }
    for(const Member& m : memberTable)
    {
        // (and its struggle as its group sets it: strength, speed, full health, arm drive)
        StruggleProfile s{};
        if(struggleOfClass(m.classname, s))
        {
            Con_Printf("mgroup class %s -> %s; struggles %g %g %g %g\n", m.classname, groupInfo[m.group].id,
                static_cast<double>(s.strength), static_cast<double>(s.speed), static_cast<double>(s.health),
                static_cast<double>(s.arms));
        }
        else
        {
            Con_Printf("mgroup class %s -> %s\n", m.classname, groupInfo[m.group].id);
        }
    }
    if(!sv.active)
    {
        return;
    }
    progs::ServerVm vm;
    int count = 0;
    progs::forEachEdict(1, [&](edict_t* e) {
        if(!(static_cast<int>(e->v.flags) & FL_MONSTER))
        {
            return;
        }
        const char* c = PR_GetString(e->v.classname);
        const int g = groupOfClass(c);
        bool known = false;
        for(const Member& m : memberTable)
        {
            known = known || std::strcmp(m.classname, c) == 0;
        }
        Con_Printf("mgroup live %d %s -> %s%s\n", NUM_FOR_EDICT(e), c, groupInfo[g].id, known ? "" : " (not in the table)");
        ++count;
    });
    Con_Printf("mgroup live: %d monsters\n", count);
}

// The classes that struggle in a player's hands, each its own (STRUGGLE.md, "Classes"; vr_struggle_classes's last
// default, configVersion 183): strength, speed, full health (0: always full effort), arm drive. A member of a struggling
// group missing here struggles with its group's base profile (struggleBases, since 2026-10-23), a few left out. Zombies since configVersion 175; mummies, AD's zombie
// knight, deflector, death lord and Quoth's death guard since 181; AD's Dark Prince since 183. Arms: an enforcer's
// forearms (his gun hands, 5 kg) hang on 1-2 kg upper arms and a zombie's whole arm is 2.4 kg, and a Box3D joint's spring
// acts on its two bodies' own inertia: their hands lagged their goals.
constexpr StruggleProfile struggleProfiles[] = {
    {"monster_army", 1.f, 1.f, 30.f, 1.f},
    {"monster_enforcer", 1.1f, 0.9f, 80.f, 2.5f},
    {"monster_knight", 1.f, 1.1f, 75.f, 2.f},
    {"monster_hell_knight", 1.25f, 1.f, 250.f, 1.5f},
    {"monster_zombie", 1.f, 0.9f, 60.f, 1.5f},
    {"monster_army_grenade", 1.f, 1.f, 30.f, 1.f},
    {"monster_army_plasma", 1.f, 1.f, 30.f, 1.f},
    {"monster_army_rocket", 1.f, 1.f, 30.f, 1.f},
    {"monster_defender", 1.1f, 0.95f, 70.f, 2.5f},
    {"monster_eliminator", 1.15f, 0.9f, 120.f, 2.5f},
    {"monster_pyro", 1.1f, 0.9f, 80.f, 1.f},
    {"monster_fumigator", 1.1f, 0.9f, 80.f, 1.f},
    {"monster_dcrossbow", 1.f, 1.05f, 90.f, 1.f},
    {"monster_dfury", 1.2f, 1.15f, 150.f, 1.f},
    {"monster_dguard", 1.2f, 1.f, 150.f, 2.5f},
    {"monster_dknight", 1.2f, 1.f, 150.f, 1.f},
    {"monster_dsergeant", 1.15f, 1.f, 120.f, 1.f},
    {"monster_mummy", 1.1f, 0.85f, 0.f, 1.5f},
    {"monster_zombiek", 1.1f, 0.9f, 60.f, 1.5f},
    {"monster_deflector", 1.1f, 0.9f, 250.f, 2.5f},
    {"monster_death_lord", 1.25f, 1.f, 400.f, 1.f},
    {"monster_death_guard", 1.2f, 1.f, 125.f, 1.f},
    {"monster_dprince", 1.3f, 1.f, 400.f, 1.f},
};
// A struggling group's base profile: the one its members without a profile of their own struggle with (the author,
// 2026-10-23: the minotaur, golem, drole and justice knight as the Dark Prince; AD's point knight, Dawn of the Machine's
// ranged knight and Quoth's death knight as a death knight; Dawn of the Machine's infected as the class they look like;
// Honey's soldier as a grunt). Groups not here (ogres, fiends...) have no humanoid rig to struggle with.
struct StruggleBase
{
    int group;
    const char* classname;
};
constexpr StruggleBase struggleBases[] = {
    {armyGroup, "monster_army"}, {enforcerGroup, "monster_enforcer"}, {knightGroup, "monster_knight"},
    {hknightGroup, "monster_hell_knight"}, {minibossGroup, "monster_dprince"},
    {zombieGroup, "monster_zombie"}, {mummyGroup, "monster_mummy"}, {eliteknightGroup, "monster_dsergeant"},
};
// Members of a struggling group left without a struggle: AD's centurion (on its hover board, a flyer), the death knight
// ghost (AD's: no body to hold), Dawn of the Machine's Shub zombie (rising from the floor, scripted).
constexpr const char* struggleLeftOut[] = {"monster_centurion", "monster_dknightghost", "monster_szombie"};

// The menu's lists of each group's profiles (struggleProfilesText), made once.
char struggleText[groupCount][512];
bool struggleTextMade = false;

// vr_struggle_classes's earlier defaults (before configVersion 175, 181 and 183): a config still holding one is at its
// default (it was moved to the next at each of those), so nothing to move.
constexpr const char* formerStruggleDefaults[] = {
    // (175's before)
    "monster_army:1:1:30 monster_enforcer:1.1:0.9:80 monster_knight:1:1.1:75 "
    "monster_hell_knight:1.25:1:250 monster_army_grenade:1:1:30 monster_army_plasma:1:1:30 "
    "monster_army_rocket:1:1:30 monster_defender:1.1:0.95:70 monster_eliminator:1.15:0.9:120 "
    "monster_pyro:1.1:0.9:80 monster_fumigator:1.1:0.9:80 monster_dcrossbow:1:1.05:90 "
    "monster_dfury:1.2:1.15:150 monster_dguard:1.2:1:150 monster_dknight:1.2:1:150 "
    "monster_dsergeant:1.15:1:120",
    // (181's before)
    "monster_army:1:1:30 monster_enforcer:1.1:0.9:80:2.5 monster_knight:1:1.1:75 "
    "monster_hell_knight:1.25:1:250:1.5 monster_zombie:1:0.9:60:1.5 monster_army_grenade:1:1:30 "
    "monster_army_plasma:1:1:30 monster_army_rocket:1:1:30 monster_defender:1.1:0.95:70 "
    "monster_eliminator:1.15:0.9:120 monster_pyro:1.1:0.9:80 monster_fumigator:1.1:0.9:80 "
    "monster_dcrossbow:1:1.05:90 monster_dfury:1.2:1.15:150 monster_dguard:1.2:1:150 "
    "monster_dknight:1.2:1:150 monster_dsergeant:1.15:1:120",
    // (183's before)
    "monster_army:1:1:30 monster_enforcer:1.1:0.9:80:2.5 monster_knight:1:1.1:75:2 "
    "monster_hell_knight:1.25:1:250:1.5 monster_zombie:1:0.9:60:1.5 monster_army_grenade:1:1:30 "
    "monster_army_plasma:1:1:30 monster_army_rocket:1:1:30 monster_defender:1.1:0.95:70:2.5 "
    "monster_eliminator:1.15:0.9:120:2.5 monster_pyro:1.1:0.9:80 monster_fumigator:1.1:0.9:80 "
    "monster_dcrossbow:1:1.05:90 monster_dfury:1.2:1.15:150 monster_dguard:1.2:1:150:2.5 "
    "monster_dknight:1.2:1:150 monster_dsergeant:1.15:1:120 monster_mummy:1.1:0.85:0:1.5 "
    "monster_zombiek:1.1:0.9:60:1.5 monster_deflector:1.1:0.9:250:2.5 monster_death_lord:1.25:1:400 "
    "monster_death_guard:1.2:1:125",
};


// vr_struggle_classes's entries (classname[:strength[:speed[:health[:arms]]]], space or comma separated), as it parsed
// them; the most it took.
constexpr int oldStruggleMost = 64;
struct OldStruggleList
{
    StruggleProfile entry[oldStruggleMost]{};
    char names[oldStruggleMost][40]{};
    int count = 0;
};

void parseOldStruggle(const char* src, OldStruggleList& out)
{
    out.count = 0;
    const char* p = src ? src : "";
    while(*p && out.count < oldStruggleMost)
    {
        while(*p == ' ' || *p == ',' || *p == '\t')
        {
            p++;
        }
        char word[96];
        int n = 0;
        while(*p && *p != ' ' && *p != ',' && *p != '\t')
        {
            if(n < static_cast<int>(sizeof(word)) - 1)
            {
                word[n++] = *p;
            }
            p++;
        }
        word[n] = 0;
        if(n == 0)
        {
            continue;
        }
        char* field = std::strchr(word, ':');
        if(field)
        {
            *field++ = 0;
        }
        const int i = out.count++;
        q_strlcpy(out.names[i], word, sizeof(out.names[i]));
        StruggleProfile& e = out.entry[i];
        e = StruggleProfile{out.names[i], 1.f, 1.f, 0.f, 1.f};
        float* values[4] = {&e.strength, &e.speed, &e.health, &e.arms};
        for(int k = 0; k < 4 && field && *field; k++)
        {
            char* next = std::strchr(field, ':');
            if(next)
            {
                *next++ = 0;
            }
            *values[k] = static_cast<float>(std::atof(field));
            field = next;
        }
    }
}

[[nodiscard]] const StruggleProfile* findOld(const OldStruggleList& l, const char* classname)
{
    for(int i = 0; i < l.count; i++)
    {
        if(std::strcmp(l.entry[i].classname, classname) == 0)
        {
            return &l.entry[i];
        }
    }
    return nullptr;
}

} // namespace

int groupOfClass(const char* classname)
{
    if(classname)
    {
        for(const Member& m : memberTable)
        {
            if(std::strcmp(m.classname, classname) == 0)
            {
                return m.group;
            }
        }
    }
    return otherGroup;
}

int groupOf(edict_t* m)
{
    return m ? groupOfClass(PR_GetString(m->v.classname)) : otherGroup;
}

const char* label(int g)
{
    return g >= 0 && g < groupCount ? groupInfo[g].label : "";
}

const char* id(int g)
{
    return g >= 0 && g < groupCount ? groupInfo[g].id : "";
}

const char* members(int g)
{
    if(!memberTextMade)
    {
        makeMemberText();
    }
    return g >= 0 && g < groupCount ? memberText[g] : "";
}

cvar_t& setting(int g, Param p)
{
    const int gi = g >= 0 && g < groupCount ? g : static_cast<int>(otherGroup);
    return values.at(gi, p);
}

void registerCvars()
{
    const DataFileSpec spec{
        dataPath, "group", "vr_mgroups.inc", groupCount, {paramKeys, paramCount}, {groupIds, groupCount}};
    if(!values.loadNamed(spec, oldName, nullptr))
    {
        Con_Printf("\x02No %s in the game folder: every monster group's value is 0 (reinstall the game)\n", dataPath);
    }
    Cmd_AddCommand("vr_mgroup_dump", dump_f);
}

cvar_t* findStandIn(const char* name)
{
    return values.find(name);
}

bool setStandIn(cvar_t* var, const char* text)
{
    return values.set(var, text);
}

za::Span<cvar_t> standIns()
{
    return {values.begin(), static_cast<za::SizeT>(DataTable<Param, groupCount, paramCount>::size)};
}

float value(edict_t* m, Param p)
{
    return setting(groupOf(m), p).value;
}

bool on(edict_t* m, Param p)
{
    return value(m, p) != 0.f;
}

float healthCut(float full)
{
    // (from, 0), then the three points: straight lines between them, the last's cut beyond; a point not past the one
    // before it is a step there.
    const float xs[] = {vr_monster_health_from.value, vr_monster_health_hp1.value, vr_monster_health_hp2.value,
        vr_monster_health_hp3.value};
    const float ys[] = {0.f, vr_monster_health_cut1.value, vr_monster_health_cut2.value, vr_monster_health_cut3.value};
    constexpr int n = 4;
    float cut = 0.f;
    if(full > xs[0])
    {
        cut = ys[n - 1];
        for(int i = 1; i < n; ++i)
        {
            if(full < xs[i])
            {
                const float span = xs[i] - xs[i - 1];
                const float t = span > 0.f ? (full - xs[i - 1]) / span : 1.f;
                cut = ys[i - 1] + (ys[i] - ys[i - 1]) * za::clamp(t, 0.f, 1.f);
                break;
            }
        }
    }
    const float most = za::clamp(vr_monster_health_max.value, 0.f, 95.f);
    return za::clamp(cut, 0.f, most) / 100.f;
}

float healthMultiplier(const char* classname, float full)
{
    if(vr_monster_health.value == 0.f || full >= healthScripted || healthExemptClass(classname))
    {
        return 1.f;
    }
    const float group = za::max(setting(groupOfClass(classname), Param::Health).value, 0.01f);
    return (1.f - healthCut(full)) * group;
}

const char* healthExampleLine(int i)
{
    if(i < 0 || i >= healthExampleCount)
    {
        return nullptr;
    }
    const HealthExample& e = healthExamples[i];
    const float m = healthMultiplier(e.classname, e.health);
    const float now = za::max(1.f, std::round(e.health * m));
    std::snprintf(healthExampleText[i], sizeof(healthExampleText[i]), "%s: %.0f -> %.0f (%+.0f%%)", e.label, e.health,
        now, (now / e.health - 1.f) * 100.f);
    return healthExampleText[i];
}

void PF_monstergroup()
{
    G_FLOAT(OFS_RETURN) = static_cast<float>(groupOf(G_EDICT(OFS_PARM0)));
}

void PF_monstergroupget()
{
    const int p = static_cast<int>(G_FLOAT(OFS_PARM1));
    G_FLOAT(OFS_RETURN) = p >= 0 && p < paramCount ? value(G_EDICT(OFS_PARM0), static_cast<Param>(p)) : 0.f;
}

const StruggleProfile* struggleProfile(const char* classname)
{
    for(const StruggleProfile& p : struggleProfiles)
    {
        if(classname && std::strcmp(p.classname, classname) == 0)
        {
            return &p;
        }
    }
    return nullptr;
}

// The profile a class struggles with: its own, else its group's base (struggleBases) unless it is left out.
const StruggleProfile* struggleProfileOrBase(const char* classname, int g)
{
    if(const StruggleProfile* own = struggleProfile(classname))
    {
        return own;
    }
    for(const char* c : struggleLeftOut)
    {
        if(classname && std::strcmp(c, classname) == 0)
        {
            return nullptr;
        }
    }
    for(const StruggleBase& b : struggleBases)
    {
        if(b.group == g && classname)
        {
            return struggleProfile(b.classname);
        }
    }
    return nullptr;
}

bool struggleOfClass(const char* classname, StruggleProfile& out)
{
    const int g = groupOfClass(classname);
    const StruggleProfile* p = struggleProfileOrBase(classname, g);
    if(!p || setting(g, Param::Struggle).value == 0.f)
    {
        return false;
    }
    out.classname = p->classname;
    out.strength = za::clamp(p->strength * setting(g, Param::StruggleStrength).value, 0.f, 3.f);
    out.speed = za::clamp(p->speed * setting(g, Param::StruggleSpeed).value, 0.2f, 3.f);
    out.arms = za::clamp(p->arms * setting(g, Param::StruggleArms).value, 1.f, 2.5f);
    out.health = setting(g, Param::StruggleHurt).value != 0.f ? p->health : 0.f;
    return true;
}

const char* struggleProfilesText(int g)
{
    if(g < 0 || g >= groupCount)
    {
        return "";
    }
    if(!struggleTextMade)
    {
        struggleTextMade = true;
        for(int k = 0; k < groupCount; ++k)
        {
            char* out = struggleText[k];
            size_t used = 0;
            out[0] = '\0';
            for(const StruggleProfile& p : struggleProfiles)
            {
                if(groupOfClass(p.classname) != k)
                {
                    continue;
                }
                const int n = std::snprintf(out + used, sizeof(struggleText[k]) - used, "%s%s %g/%g/%g/%g", used ? ", " : "",
                    p.classname + (std::strncmp(p.classname, "monster_", 8) == 0 ? 8 : 0), static_cast<double>(p.strength),
                    static_cast<double>(p.speed), static_cast<double>(p.health), static_cast<double>(p.arms));
                if(n < 0 || used + static_cast<size_t>(n) >= sizeof(struggleText[k]))
                {
                    break;
                }
                used += static_cast<size_t>(n);
            }
            for(const StruggleBase& b : struggleBases)
            {
                if(b.group == k && used)
                {
                    std::snprintf(out + used, sizeof(struggleText[k]) - used, "; the rest as %s", b.classname + 8);
                }
            }
        }
    }
    return struggleText[g];
}

void migrateStruggleClasses(const char* classes)
{
    OldStruggleList old;
    parseOldStruggle(classes, old);
    // A list of exactly the profiles (as the last default had it): nothing to move.
    bool same = old.count == static_cast<int>(sizeof(struggleProfiles) / sizeof(struggleProfiles[0]));
    for(const StruggleProfile& p : struggleProfiles)
    {
        const StruggleProfile* o = findOld(old, p.classname);
        same = same && o && std::fabs(o->strength - p.strength) < 1e-3f && std::fabs(o->speed - p.speed) < 1e-3f &&
               std::fabs(o->health - p.health) < 1e-3f && std::fabs(o->arms - p.arms) < 1e-3f;
    }
    for(const char* former : formerStruggleDefaults)
    {
        OldStruggleList f;
        parseOldStruggle(former, f);
        bool match = f.count == old.count;
        for(int i = 0; i < f.count && match; i++)
        {
            const StruggleProfile* o = findOld(old, f.entry[i].classname);
            match = o && std::fabs(o->strength - f.entry[i].strength) < 1e-3f && std::fabs(o->speed - f.entry[i].speed) < 1e-3f &&
                    std::fabs(o->health - f.entry[i].health) < 1e-3f && std::fabs(o->arms - f.entry[i].arms) < 1e-3f;
        }
        same = same || match;
    }
    if(same)
    {
        return;
    }
    Con_Printf("VR: vr_struggle_classes (retired) moved into the monster groups' Struggle settings:\n");
    for(int i = 0; i < old.count; i++)
    {
        if(!struggleProfile(old.entry[i].classname))
        {
            Con_Printf("  %s: no struggle of its own (ogres, fiends...); left out\n", old.entry[i].classname);
        }
    }
    for(int g = 0; g < groupCount; g++)
    {
        int profiled = 0, present = 0, hurtless = 0;
        float sums[3] = {0.f, 0.f, 0.f}, lows[3] = {1e9f, 1e9f, 1e9f}, highs[3] = {-1e9f, -1e9f, -1e9f};
        for(const StruggleProfile& p : struggleProfiles)
        {
            if(groupOfClass(p.classname) != g)
            {
                continue;
            }
            profiled++;
            const StruggleProfile* o = findOld(old, p.classname);
            if(!o)
            {
                continue;
            }
            present++;
            const float ratios[3] = {p.strength > 0.f ? o->strength / p.strength : 1.f, p.speed > 0.f ? o->speed / p.speed : 1.f,
                p.arms > 0.f ? o->arms / p.arms : 1.f};
            for(int k = 0; k < 3; k++)
            {
                sums[k] += ratios[k];
                lows[k] = za::min(lows[k], ratios[k]);
                highs[k] = za::max(highs[k], ratios[k]);
            }
            hurtless += o->health <= 0.f && p.health > 0.f ? 1 : 0;
        }
        if(profiled == 0)
        {
            continue;
        }
        Cvar_SetValueQuick(&setting(g, Param::Struggle), present > 0 ? 1.f : 0.f);
        if(present == 0)
        {
            Con_Printf("  %s: none of its classes listed: Struggle off\n", label(g));
            continue;
        }
        if(present < profiled)
        {
            Con_Printf("  %s: %d of its %d struggling classes listed: Struggle on for all of them\n", label(g), present, profiled);
        }
        const Param params[3] = {Param::StruggleStrength, Param::StruggleSpeed, Param::StruggleArms};
        const char* names[3] = {"strength", "speed", "arms"};
        for(int k = 0; k < 3; k++)
        {
            const float mean = sums[k] / static_cast<float>(present);
            Cvar_SetValueQuick(&setting(g, params[k]), std::round(mean * 100.f) / 100.f);
            if(highs[k] - lows[k] > 1e-3f)
            {
                Con_Printf("  %s: its classes' %s changed unevenly (x%.2f .. x%.2f): x%.2f for all\n", label(g), names[k],
                    static_cast<double>(lows[k]), static_cast<double>(highs[k]), static_cast<double>(mean));
            }
        }
        Cvar_SetValueQuick(&setting(g, Param::StruggleHurt), hurtless == present ? 0.f : 1.f);
    }
}

void registerCommands()
{
    Cmd_AddCommand("vr_mgroup_list", list_f);
}

} // namespace qvr::mgroups

// SV_Physics, after every entity has thought: each monster seen for the first time (FL_MONSTER, alive, its QC
// .vr_hpscale still 0) gets Monster Health's multiplier on its health and full health (.max_health), and .vr_hpscale
// the multiplier (1 unchanged): once, saved with it (a loaded game's monsters are not cut again; an older save's are,
// by their full health). Its full health sets the cut (its health where it has none).
extern "C" void VR_MonsterHealth_Frame(void)
{
    using namespace qvr;
    const int ofs = progs::fields().vr_hpscale;
    if(ofs < 0 || qcvm != &sv.qcvm)
    {
        return;
    }
    progs::forEachEdict(svs.maxclients + 1, [&](edict_t* e) {
        if(!(static_cast<int>(e->v.flags) & FL_MONSTER) || e->v.health <= 0.f)
        {
            return;
        }
        float& done = progs::fieldFloat(e, ofs);
        if(done != 0.f)
        {
            return;
        }
        const char* classname = PR_GetString(e->v.classname);
        const float full = e->v.max_health >= e->v.health ? e->v.max_health : e->v.health;
        const float m = progs::isTrainingDummy(e) ? 1.f : mgroups::healthMultiplier(classname, full);
        done = m;
        const float before = e->v.health;
        if(m != 1.f)
        {
            e->v.health = za::max(1.f, std::round(e->v.health * m));
            if(e->v.max_health > 0.f)
            {
                e->v.max_health = za::max(1.f, std::round(e->v.max_health * m));
            }
        }
        if(vr_monster_health_debug.value != 0.f)
        {
            Con_Printf("mhealth %d %s %s: %g -> %g (full %g, x%.3f)\n", NUM_FOR_EDICT(e), classname,
                mgroups::id(mgroups::groupOfClass(classname)), before, e->v.health, e->v.max_health, m);
        }
    });
}
