// vr_ad_monsters.cpp -- Arcane Dimensions' monsters with Quake VR's treatment, the engine's side: see vr_ad_monsters.hpp.

#include "vr_ad_monsters.hpp"

#include <cstring>

namespace qvr::ad
{

namespace
{

struct ClassAlias
{
    const char* ad;
    const char* vr;
};

// Each AD monster class and the Quake VR class it is tuned as: the nearest build (QC vr_ad_monsters.qc picks the same
// one for its knockdown chance and corpse health). A class not here keeps its own name (the globals).
constexpr ClassAlias classAliases[] = {
    // Knights in plate: AD's death knights and their kin
    {"monster_dknight", "monster_hell_knight"},
    {"monster_dknightghost", "monster_hell_knight"},
    {"monster_dprince", "monster_hell_knight"},
    {"monster_dfury", "monster_hell_knight"},
    {"monster_dsergeant", "monster_hell_knight"},
    {"monster_dguard", "monster_hell_knight"},
    {"monster_death_guard", "monster_hell_knight"},
    {"monster_death_lord", "monster_hell_knight"},
    {"monster_hell_pointknight", "monster_hell_knight"},
    {"monster_justice", "monster_hell_knight"},
    {"monster_judicator", "monster_hell_knight"},
    {"monster_dcrossbow", "monster_knight"},
    // Soldiers and enforcers
    {"monster_army_rocket", "monster_army"},
    {"monster_army_grenade", "monster_army"},
    {"monster_army_plasma", "monster_army"},
    {"monster_skullwiz", "monster_army"},
    {"monster_defender", "monster_enforcer"},
    {"monster_eliminator", "monster_enforcer"},
    {"monster_centurion", "monster_enforcer"},
    {"monster_pyro", "monster_enforcer"},
    {"monster_fumigator", "monster_enforcer"},
    {"monster_seeker", "monster_enforcer"},
    {"monster_deflector", "monster_enforcer"},
    // Ogres
    {"monster_ogre_flak", "monster_ogre"},
    {"monster_hogre", "monster_ogre"},
    {"monster_hogremac", "monster_ogre"},
    {"monster_hogreham", "monster_ogre"},
    {"monster_ogre_fishing", "monster_ogre"},
    {"monster_freddie", "monster_ogre"},
    // Zombies
    {"monster_zombiek", "monster_zombie"},
    {"monster_boil", "monster_zombie"},
    // Brutes
    {"monster_minotaur", "monster_shambler"},
    {"monster_golem", "monster_shambler"},
    {"monster_drole", "monster_shambler"},
    {"monster_boglord", "monster_shambler"},
    // Crawlers
    {"monster_spider", "monster_scourge"},
    {"monster_wraithminion", "monster_scourge"},
    {"monster_scorpion", "monster_scourge"},
    {"monster_wraithminion2", "monster_scourge"},
    {"monster_swampling", "monster_dog"},
    {"monster_voreling", "monster_dog"},
    {"monster_shalrathminion", "monster_dog"},
    // Flyers
    {"monster_gargoyle", "monster_wizard"},
    {"monster_minotaurminion", "monster_wizard"},
    {"monster_gaunt", "monster_wizard"},
    {"monster_judgeminion", "monster_wizard"},
    {"monster_wraith", "monster_wizard"},
};

// AD's heads held as props (vr_props.inc, Mass -1) and the class that throws each (QC ad_mon_*.qc ad_headmdl).
constexpr ClassAlias headClasses[] = {
    {"progs/h_dknight.mdl", "monster_dknight"},
    {"progs/h_dcrossbow.mdl", "monster_dcrossbow"},
    {"progs/h_dguard.mdl", "monster_dguard"},
    {"progs/h_dfury.mdl", "monster_dfury"},
    {"progs/h_soldier.mdl", "monster_army"},
    {"progs/h_enforcer.mdl", "monster_enforcer"},
    {"progs/h_hogre.mdl", "monster_hogre"},
    {"progs/h_hogreham.mdl", "monster_hogreham"},
};

} // namespace

const char* headVrClass(const char* model)
{
    if(!model)
    {
        return nullptr;
    }
    for(const ClassAlias& h : headClasses)
    {
        if(!std::strcmp(model, h.ad))
        {
            return vrClass(h.vr);
        }
    }
    return nullptr;
}

const char* vrClass(const char* classname)
{
    if(!classname || std::strncmp(classname, "monster_", 8) != 0)
    {
        return classname;
    }
    for(const ClassAlias& a : classAliases)
    {
        if(!std::strcmp(classname, a.ad))
        {
            return a.vr;
        }
    }
    return classname;
}

} // namespace qvr::ad
