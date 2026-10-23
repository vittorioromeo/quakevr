// vr_mgroups.cpp -- monster groups (vr_mgroups.hpp; the groups and their defaults: vr_mgroups.inc).

#include "vr_mgroups.hpp"
#include "vr_cvars.hpp"
#include "vr_progs.hpp"

#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"

#include <cmath>
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
    // its ghost, Quoth's death knight), death guard (Quoth's too), death lord. (The Dark Prince, the death knight's mesh
    // and size: AD's FGD calls it a "mini boss", as the baron, but its code is a plain monster's: 400 health, no boss
    // flags or script, placed in no map of AD's.)
    {"monster_hell_knight", hknightGroup}, {"monster_hell_knight_infected", hknightGroup},
    {"monster_hell_pointknight", hknightGroup}, {"monster_ranged_knight", hknightGroup},
    {"monster_dknight", hknightGroup}, {"monster_dknightghost", hknightGroup}, {"monster_death_knight", hknightGroup},
    {"monster_dguard", hknightGroup}, {"monster_death_guard", hknightGroup}, {"monster_death_lord", hknightGroup},
    // Mini Bosses (the author, 2026-10-23): the Dark Prince, the minotaur (AD's big walker, a humanoid's rig), the
    // justice knight (AD's normal judicator; the baron, its old name). Elite Knights: the death fury and the death sergeant.
    {"monster_dprince", minibossGroup}, {"monster_minotaur", minibossGroup}, {"monster_justice", minibossGroup},
    {"monster_baron", minibossGroup}, {"monster_dfury", eliteknightGroup}, {"monster_dsergeant", eliteknightGroup},
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
    // AD's big walkers: the drole, the stone golem (the minotaur: a Mini Boss).
    {"monster_drole", giantGroup}, {"monster_golem", giantGroup},
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

// Each group's settings, in Param's order (vr_cvars.inc's QVR_MGROUP makes them).
cvar_t* const settings[groupCount][paramCount] = {
#define QVR_MGROUP(g, ...)                                                                                                \
    {&vr_mgrp_##g##_kdtrig, &vr_kdtrig_resist_##g, &vr_mgrp_##g##_ledge, &vr_mgrp_##g##_stairs, &vr_mgrp_##g##_slope,    \
        &vr_mgrp_##g##_shotgun, &vr_mgrp_##g##_wounded, &vr_mgrp_##g##_blow, &vr_mgrp_##g##_blast,                       \
        &vr_mgrp_##g##_thrown, &vr_mgrp_##g##_tackle, &vr_knockdown_chance_##g, &vr_mgrp_##g##_push, &vr_mgrp_##g##_stagger, &vr_mgrp_##g##_sweep,                     \
        &vr_mgrp_##g##_leggrab, &vr_mgrp_##g##_lift, &vr_mgrp_##g##_necksnap, &vr_mgrp_##g##_manhandle,                  \
        &vr_mgrp_##g##_throw, &vr_mgrp_##g##_hold, &vr_mgrp_##g##_poise, &vr_mgrp_##g##_health},
#include "vr_mgroups.inc"
#undef QVR_MGROUP
};

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
        Con_Printf("mgroup class %s -> %s\n", m.classname, groupInfo[m.group].id);
    }
    if(!sv.active)
    {
        return;
    }
    qcvm_t* old = nullptr;
    PR_PushQCVM(&sv.qcvm, &old);
    int count = 0;
    for(int num = 1; num < qcvm->num_edicts; ++num)
    {
        edict_t* e = EDICT_NUM(num);
        if(e->free || !(static_cast<int>(e->v.flags) & FL_MONSTER))
        {
            continue;
        }
        const char* c = PR_GetString(e->v.classname);
        const int g = groupOfClass(c);
        bool known = false;
        for(const Member& m : memberTable)
        {
            known = known || std::strcmp(m.classname, c) == 0;
        }
        Con_Printf("mgroup live %d %s -> %s%s\n", num, c, groupInfo[g].id, known ? "" : " (not in the table)");
        ++count;
    }
    Con_Printf("mgroup live: %d monsters\n", count);
    PR_PopQCVM(old);
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
    return *settings[gi][static_cast<int>(p)];
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
    for(int num = svs.maxclients + 1; num < qcvm->num_edicts; ++num)
    {
        edict_t* e = EDICT_NUM(num);
        if(e->free || !(static_cast<int>(e->v.flags) & FL_MONSTER) || e->v.health <= 0.f)
        {
            continue;
        }
        float& done = progs::fieldFloat(e, ofs);
        if(done != 0.f)
        {
            continue;
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
            Con_Printf("mhealth %d %s %s: %g -> %g (full %g, x%.3f)\n", num, classname,
                mgroups::id(mgroups::groupOfClass(classname)), before, e->v.health, e->v.max_health, m);
        }
    }
}
