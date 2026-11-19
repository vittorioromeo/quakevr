// vr_rules.cpp -- the ruleset a map plays by (vr_rules.hpp; docs/vr-port/DWELL.md section 8, S1).
//
// One question, one answer: which ruleset does the map now spawning speak? Found once, before its entities spawn:
// - the campaign's own ruleset (campaignRulesets: Machine Games' campaigns MG's, Arcane Dimensions' AD's);
// - else the map's entity text, scored for every detecting ruleset together (rulesets[]): a ruleset's signals are its
//   own classnames (a spawn function of its imported code, <prefix><classname>, and none of Quake VR's; a name two
//   detecting rulesets both have is nobody's signal) and its worldspawn keys, counted while its data is mounted; the
//   highest score at its threshold or above wins (a tie: neither);
// - else Quake's.
// Published as vr_ruleset (QC VR_Ruleset(); vr_ad_rules is AD's row of it, for AD's code and tests). A new ruleset is a
// row here (with its spawnflag column, QC vr_spawnflags.qc); Copper's row is defined and detects nothing yet.

#include "vr_rules.hpp"
#include "vr_ad.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_strings.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"
#include "vr_api.h"

#include <cstdio>
#include <cstring>

extern "C" {
int VR_ActiveCampaign(void);
}

namespace
{

using qvr::rules::Id;

// A list of names (a row's detection data); {} none.
struct Names
{
    const char* const* items = nullptr;
    size_t count = 0;

    [[nodiscard]] bool has(const char* s) const
    {
        for(size_t i = 0; i < count; ++i)
        {
            if(!strcmp(items[i], s)) { return true; }
        }
        return false;
    }

    [[nodiscard]] bool prefixes(const char* s) const
    {
        for(size_t i = 0; i < count; ++i)
        {
            if(qvr::strings::startsWith(s, items[i])) { return true; }
        }
        return false;
    }
};

template <size_t N>
[[nodiscard]] constexpr Names names(const char* const (&list)[N])
{
    return Names{list, N};
}

// AD's detection data (ARCANE_DIMENSIONS.md section 5.1). Classnames that are not AD signals although only AD has them
// (shared with Quoth, Copper, compilers; id's stray item_weapon).
constexpr const char* adNotSignals[] = {
    "func_group", "info_target", "item_weapon", "misc_external_map", "trigger_ladder",
};
constexpr const char* adNotSignalPrefixes[] = {"func_detail", "misc_particle"};
// AD's worldspawn keys (section 6): each one present is a signal.
constexpr const char* adWorldSignals[] = {
    "bodyfadeaway", "bodyflrcheck", "fog_dpextra", "give_weapons", "knight_defskin", "no_bigprojectiles",
    "no_item_offset", "no_liquiddmg", "no_sgprojectile", "no_zaware", "particlemax", "reset_health",
    "sprite_particles", "take_weapons", "upgrade_axe", "upgrade_ghook", "upgrade_lg", "upgrade_ssg", "water_alpha",
};

// The shared classnames AD has its own version of, whose AD version plays in AD maps although they look like the ones
// Quake VR keeps (monster_, item_): the names that are another entity in Rogue or Hipnotic (ARCANE_DIMENSIONS.md 5.4).
constexpr const char* adOwnedClashes[] = {
    "item_artifact_wetsuit", "item_plasma", "monster_eel", "monster_mummy",
    "weapon_axe", // (Quake VR has a weapon_axe since 2026-11-08: a grabbable axe; AD maps keep AD's pickup)
};

struct Ruleset
{
    Id id;
    const char* name;
    const char* prefix; // its imported code's spawn functions' prefix ("ad_"), or nullptr
    bool detects;       // its signals are counted (else only its campaign sets it)
    int threshold;
    bool (*mounted)();  // its data is on the search path (it is detected only then)
    Names notSignals;   // its own classnames that are not signals (shared with mods and compilers)
    Names notSignalPrefixes;
    Names worldSignals; // its worldspawn keys
    Names ownedClashes; // shared classnames whose imported version plays by its rules although Quake VR keeps the kind
    int (*spawnsOutside)(const char* classname); // outside its rules: its <prefix><classname> spawns this one (or null)
    int (*toolgunOutside)(const char* classname); // outside its rules: the toolgun's explicit <prefix><classname> spawns
};

bool never() { return false; }

constexpr Ruleset rulesets[] = {
    {qvr::rules::ID, "Quake", nullptr, false, 0, never, {}, {}, {}, {}, nullptr, nullptr},
    {qvr::rules::MG, "Machine Games", nullptr, false, 0, never, {}, {}, {}, {}, nullptr, nullptr},
    {qvr::rules::AD, "Arcane Dimensions", "ad_", true, 2, qvr::ad::folderMounted, names(adNotSignals),
        names(adNotSignalPrefixes), names(adWorldSignals), names(adOwnedClashes), VR_AD_SpawnsOutside,
        VR_AD_ToolgunSpawnsOutside},
    // Copper's (DWELL.md 5.2): its code is imported (QC/copper, cu_<name>, P1a) but inert: it detects nothing yet and
    // none of its classnames spawns outside its rules; its signals (its 48 own classnames, the worldspawn _tb_mod
    // key) and its data come in P1b.
    {qvr::rules::COPPER, "Copper", "cu_", false, 2, never, {}, {}, {}, {}, nullptr, nullptr},
};

// Under an imported ruleset (its <prefix><classname> exists), whether its version spawns a classname Quake VR has too:
// Quake VR's stays for the vanilla monsters, items and weapons (but the ruleset's owned clashes), the player's spawn
// points, the intermission, the level exit and the world (whose spawn runs the ruleset's world setup after its own: QC
// worldspawn). AD's decision 1 and Copper's (DWELL.md 11.1): our weapons and monsters, its world logic.
[[nodiscard]] bool playsImported(const Ruleset& r, const char* classname)
{
    using qvr::strings::startsWith;
    return r.ownedClashes.has(classname) ||
           !(startsWith(classname, "monster_") || startsWith(classname, "item_") || startsWith(classname, "weapon_") ||
               startsWith(classname, "info_player_") || !strcmp(classname, "info_intermission") ||
               !strcmp(classname, "trigger_changelevel") || !strcmp(classname, "worldspawn"));
}
static_assert(countof(rulesets) == qvr::rules::Count, "one row a ruleset, in Id order");

// The campaigns' own rulesets (vr_gamedir.cpp campaigns[]: id1, hipnotic, rogue, dopa, mg1, mg3, ad).
constexpr Id campaignRulesets[] = {
    qvr::rules::ID, qvr::rules::ID, qvr::rules::ID, qvr::rules::MG, qvr::rules::MG, qvr::rules::MG, qvr::rules::AD,
};
static_assert(campaignRulesets[qvr::ad::campaignIndex] == qvr::rules::AD, "AD's campaign plays by AD's rules");

struct Detection
{
    int signals = 0;
    char first[256] = {};
};

struct MapRules
{
    Id id = qvr::rules::ID;
    bool campaign = false;
    Detection found[qvr::rules::Count];
    int importedSpawns = 0; // entities spawned with the imported ruleset's function
    int oursShared = 0;     // its shared classnames whose Quake VR version spawned
} mapRules;

void addSignal(Detection& d, const char* what)
{
    ++d.signals;
    if(strlen(d.first) + strlen(what) + 3 < sizeof(d.first))
    {
        if(d.first[0]) { q_strlcat(d.first, ", ", sizeof(d.first)); }
        q_strlcat(d.first, what, sizeof(d.first));
    }
}

// Whether `classname` is ruleset r's own: its prefixed spawn function and none of Quake VR's, not excluded.
[[nodiscard]] bool ownClassname(const Ruleset& r, const char* classname)
{
    if(!r.prefix || r.notSignals.has(classname) || r.notSignalPrefixes.prefixes(classname)) { return false; }
    return !VR_ED_FindFunction(classname) && VR_ED_FindFunction(va("%s%s", r.prefix, classname));
}

// Every detecting ruleset's signals in the map's entity text: its own classnames (each name once; a name two detecting
// rulesets have is nobody's) and its worldspawn keys.
void countSignals(const char* data)
{
    bool counting[qvr::rules::Count] = {};
    for(const Ruleset& r : rulesets) { counting[r.id] = r.detects; }
    for(Detection& d : mapRules.found) { d = Detection{}; }
    za::Vector<za::String> seen;
    bool world = true;
    char key[256];
    while((data = COM_Parse(data)) && com_token[0] == '{')
    {
        while((data = COM_Parse(data)) && com_token[0] != '}')
        {
            q_strlcpy(key, com_token, sizeof(key));
            if(!(data = COM_Parse(data))) { return; }
            for(const Ruleset& r : rulesets)
            {
                if(counting[r.id] && world && r.worldSignals.has(key))
                {
                    addSignal(mapRules.found[r.id], key);
                }
            }
            if(strcmp(key, "classname")) { continue; }
            bool dup = false;
            for(const za::String& s : seen) { dup |= !strcmp(s.cStr(), com_token); }
            if(dup) { continue; }
            seen.pushBack(za::String{com_token});
            int owner = -1;
            for(const Ruleset& r : rulesets)
            {
                if(counting[r.id] && ownClassname(r, com_token)) { owner = owner < 0 ? r.id : -2; }
            }
            if(owner >= 0) { addSignal(mapRules.found[owner], com_token); }
        }
        world = false;
        if(!data) { return; }
    }
}

void infoCommand()
{
    const int c = VR_ActiveCampaign();
    Con_Printf("Ruleset: %s (vr_ruleset %d; %s)\n", rulesets[mapRules.id].name, int(mapRules.id),
        mapRules.campaign ? "the campaign's" : mapRules.id == qvr::rules::ID ? "no ruleset detected" : "detected");
    Con_Printf("Campaign %d: %s\n", c, c >= 0 && c < int(countof(campaignRulesets)) ? rulesets[campaignRulesets[c]].name : "?");
    for(const Ruleset& r : rulesets)
    {
        if(!r.detects) { continue; }
        const Detection& d = mapRules.found[r.id];
        Con_Printf("  %s: %d signals (threshold %d, data %s)%s%s\n", r.name, d.signals, r.threshold,
            r.mounted() ? "mounted" : "not mounted", d.signals ? ": " : "", d.first);
    }
}

} // namespace

namespace qvr::rules
{

void beforeLoad()
{
    const int c = VR_ActiveCampaign();
    const Id own = c >= 0 && c < int(countof(campaignRulesets)) ? campaignRulesets[c] : ID;
    countSignals(sv.worldmodel ? sv.worldmodel->entities : "");
    mapRules.importedSpawns = mapRules.oursShared = 0;
    mapRules.campaign = own != ID;
    mapRules.id = own;
    if(!mapRules.campaign)
    {
        int best = 0;
        for(const Ruleset& r : rulesets)
        {
            const int s = mapRules.found[r.id].signals;
            if(!r.detects || s < r.threshold || s < best || !r.mounted()) { continue; }
            mapRules.id = s == best ? ID : r.id; // (a tie at the top: neither)
            best = s;
        }
    }
    char value[8];
    q_snprintf(value, sizeof(value), "%d", int(mapRules.id));
    Cvar_SetROM("vr_ruleset", value);
    if(mapRules.id != ID)
    {
        Con_DPrintf("VR: %s rules on (%s; %d signals)\n", rulesets[mapRules.id].name,
            mapRules.campaign ? "campaign" : "detected", mapRules.found[mapRules.id].signals);
    }
}

Id current()
{
    return mapRules.id;
}

bool byCampaign()
{
    return mapRules.campaign;
}

int signals(Id r)
{
    return mapRules.found[r].signals;
}

const char* firstSignals(Id r)
{
    return mapRules.found[r].first;
}

int importedSpawns()
{
    return mapRules.importedSpawns;
}

int oursShared()
{
    return mapRules.oursShared;
}

void registerCommands()
{
    Cmd_AddCommand("vr_ruleset_info", infoCommand);
}

} // namespace qvr::rules

// pr_edict.c ED_SpawnFunction and the toolgun's spawnClass (vr_box3d_api.cpp): the spawn function that spawns
// `classname` (`ours`: Quake VR has one) by the map's ruleset, or null. Under an imported ruleset its version where it
// has one and plays it (playsImported), else Quake VR's; under the others Quake VR's, or for a classname Quake VR has
// none for, an imported ruleset's that spawns outside its rules (AD's artifacts, pickups, ladders, monsters).
extern "C" const char* VR_Rules_SpawnName(const char* classname, int ours)
{
    const Ruleset& r = rulesets[mapRules.id];
    if(r.prefix)
    {
        const char* imported = va("%s%s", r.prefix, classname);
        if(VR_ED_FindFunction(imported))
        {
            const bool theirs = !ours || playsImported(r, classname);
            ++(theirs ? mapRules.importedSpawns : mapRules.oursShared);
            if(theirs) { return imported; }
        }
        return ours ? classname : nullptr;
    }
    if(ours) { return classname; }
    for(const Ruleset& o : rulesets)
    {
        if(o.prefix && o.spawnsOutside && o.spawnsOutside(classname)) { return va("%s%s", o.prefix, classname); }
    }
    return nullptr;
}

// The toolgun's explicit variants (box3d::spawnClass): a classname with a ruleset's prefix (ad_monster_gaunt,
// cu_func_bobbing) spawns that ruleset's version only under its rules, or where its row lets the toolgun make it
// outside them (AD's toolgunOutside). Returns 0 for a classname with no ruleset's prefix, else 1 (`*plain` the
// classname without it) or -1 (refused).
extern "C" int VR_Rules_ToolgunPrefixed(const char* classname, const char** plain)
{
    for(const Ruleset& r : rulesets)
    {
        const size_t n = r.prefix ? strlen(r.prefix) : 0;
        if(!n || strncmp(classname, r.prefix, n)) { continue; }
        *plain = classname + n;
        return mapRules.id == r.id || (r.toolgunOutside && r.toolgunOutside(classname + n)) ? 1 : -1;
    }
    return 0;
}
