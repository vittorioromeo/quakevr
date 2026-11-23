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
#include "vr_dwell.hpp"
#include "vr_quoth.hpp"
#include "vr_mapinstall.hpp"
#include "vr_mapindex.hpp"
#include "vr_engine.hpp"
#include "vr_strings.hpp"

#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Base/Strlen.hpp"
#include "Zancle/Base/Strncmp.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"
#include "vr_api.h"

#include <cstdio>

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
    za::SizeT count = 0;

    [[nodiscard]] bool has(const char* s) const
    {
        for(za::SizeT i = 0; i < count; ++i)
        {
            if(!ZA_STRCMP(items[i], s)) { return true; }
        }
        return false;
    }

    [[nodiscard]] bool prefixes(const char* s) const
    {
        for(za::SizeT i = 0; i < count; ++i)
        {
            if(qvr::strings::startsWith(s, items[i])) { return true; }
        }
        return false;
    }
};

template <za::SizeT N>
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

// Quoth's maps play by AD's rules (QUOTH.md: AD re-implements nearly every Quoth classname): its campaign, and a Map
// Library package made for Quoth.
constexpr const char* adMadeFor[] = {"Quoth"};
// Ours under AD's rules too: Quoth's monster spawners (AD's remove themselves; vr_quoth.qc's spawn the monsters).
constexpr const char* adKeptOurs[] = {"info_multispawn", "func_hordespawn"};

// Copper's detection data (DWELL.md 5.2): its own classnames (cu_<classname>, none of Quake VR's) but the compiler's
// and the ones AD has too (nobody's signal anyway); Dwell's own classnames (no spawn function yet: P2), each a signal;
// the worldspawn _tb_mod key (TrenchBroom's game configuration) naming Copper or Dwell.
constexpr const char* copperNotSignals[] = {"func_group", "misc_external_map"};
constexpr const char* copperNotSignalPrefixes[] = {"func_detail"};
constexpr const char* copperExtraSignals[] = {
    "monster_kamikaze", "monster_brute", "monster_excavator", "monster_d1boss", "monster_d2boss", "monster_boss_egypt",
    "weapon_quad_shotgun", "weapon_railgun", "weapon_rifle", "item_artifact_haste", "item_artifact_berserk",
    "misc_crystal", "func_fade", "info_storycamera", "trigger_killboss", "target_bullethell",
};
// A worldspawn key whose value names the ruleset (its value contains one of `values`, any case).
struct WorldValue
{
    const char* key;
    Names values;
};
constexpr const char* copperTbMods[] = {"copper", "dwell"};
// Ours under Copper's rules too: the exploding boxes are our VR props (grabbed, thrown: our physics moves them, which
// Copper's brush-model box does not survive: "SOLID_BSP without MOVETYPE_PUSH").
constexpr const char* copperKeptOurs[] = {"misc_explobox", "misc_explobox2"};

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
    Names extraSignals;     // classnames that are its signals although it has no spawn function for them (Dwell's own)
    WorldValue worldValue;  // a worldspawn key whose value names it ({}: none)
    Names madeFor;          // a Map Library package made for one of these (vr_mapinstall.cpp madeFor) plays by it
    Names keptOurs;         // more shared classnames whose Quake VR version plays under its rules (playsImported)
    Id clashesUnder{qvr::rules::Count}; // a content row (spawnsOutside): the ruleset under which its ownedClashes win
                                        // over Quake VR's version (Dwell's wetsuit in Copper's maps); Count: none
};

bool never() { return false; }
bool always() { return true; }

// Dwell's row (DWELL.md P2a, decisions 3 and 4): dw_<classname> spawns in any map while Dwell's data is on the search
// path (its campaign, or its content in another: vr_gamedir.cpp), with its own behaviour; the toolgun's too.
int dwellSpawns(const char* classname)
{
    return qvr::dwell::folderMounted() && VR_ED_FindFunction(va("dw_%s", classname)) ? 1 : 0;
}
// Dwell's versions of classnames another mod has (Hipnotic's and AD's wetsuit): Dwell's in Copper's (Dwell's) maps,
// the current owner's elsewhere, dw_<name> by the toolgun anywhere. (Its monster_morph, unplaced, is not rebuilt.)
constexpr const char* dwellOwnedClashes[] = {"item_artifact_wetsuit"};

constexpr Ruleset rulesets[] = {
    {qvr::rules::ID, "Quake", nullptr, false, 0, never, {}, {}, {}, {}, nullptr, nullptr, {}, {}, {}, {}},
    {qvr::rules::MG, "Machine Games", nullptr, false, 0, never, {}, {}, {}, {}, nullptr, nullptr, {}, {}, {}, {}},
    {qvr::rules::AD, "Arcane Dimensions", "ad_", true, 2, qvr::ad::folderMounted, names(adNotSignals),
        names(adNotSignalPrefixes), names(adWorldSignals), names(adOwnedClashes), VR_AD_SpawnsOutside,
        VR_AD_ToolgunSpawnsOutside, {}, {}, names(adMadeFor), names(adKeptOurs)},
    // Copper's (DWELL.md 5.2, P1b): its code is in the progs (QC/copper, cu_<name>), so it is detected with no data
    // mounted; Dwell's campaign and packages made for Copper or Dwell play by it too. None of its classnames spawns
    // outside its rules.
    {qvr::rules::COPPER, "Copper", "cu_", true, 2, always, names(copperNotSignals), names(copperNotSignalPrefixes), {},
        {}, nullptr, nullptr, names(copperExtraSignals), {"_tb_mod", names(copperTbMods)}, names(copperTbMods),
        names(copperKeptOurs)},
    // Dwell's own entities: content, not rules (never detected, no campaign plays by it).
    {qvr::rules::DWELL, "Dwell", "dw_", false, 0, qvr::dwell::folderMounted, {}, {}, {}, names(dwellOwnedClashes),
        dwellSpawns, dwellSpawns, {}, {}, {}, {}, qvr::rules::COPPER},
};

// Under an imported ruleset (its <prefix><classname> exists), whether its version spawns a classname Quake VR has too:
// Quake VR's stays for the vanilla monsters, items and weapons (but the ruleset's owned clashes), the player's spawn
// points, the intermission, the level exit and the world (whose spawn runs the ruleset's world setup after its own: QC
// worldspawn). AD's decision 1 and Copper's (DWELL.md 11.1): our weapons and monsters, its world logic.
[[nodiscard]] bool playsImported(const Ruleset& r, const char* classname)
{
    using qvr::strings::startsWith;
    if(r.keptOurs.has(classname)) { return false; }
    return r.ownedClashes.has(classname) ||
           !(startsWith(classname, "monster_") || startsWith(classname, "item_") || startsWith(classname, "weapon_") ||
               startsWith(classname, "info_player_") || !ZA_STRCMP(classname, "info_intermission") ||
               !ZA_STRCMP(classname, "trigger_changelevel") || !ZA_STRCMP(classname, "worldspawn"));
}
static_assert(countof(rulesets) == qvr::rules::Count, "one row a ruleset, in Id order");

// The campaigns' own rulesets (vr_gamedir.cpp campaigns[]: id1, hipnotic, rogue, dopa, mg1, mg3, ad, dwell, quoth).
constexpr Id campaignRulesets[] = {
    qvr::rules::ID, qvr::rules::ID, qvr::rules::ID, qvr::rules::MG, qvr::rules::MG, qvr::rules::MG, qvr::rules::AD,
    qvr::rules::COPPER, qvr::rules::AD,
};
static_assert(campaignRulesets[qvr::ad::campaignIndex] == qvr::rules::AD, "AD's campaign plays by AD's rules");
static_assert(campaignRulesets[qvr::dwell::campaignIndex] == qvr::rules::COPPER, "Dwell's campaign plays by Copper's");
static_assert(campaignRulesets[qvr::quoth::campaignIndex] == qvr::rules::AD, "Quoth's campaign plays by AD's");

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
    char package[64] = {};  // the active Map Library package's mod, when the map is its own ("" : none)
} mapRules;

void addSignal(Detection& d, const char* what)
{
    ++d.signals;
    if(ZA_STRLEN(d.first) + ZA_STRLEN(what) + 3 < sizeof(d.first))
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
                if(counting[r.id] && world && r.worldValue.key && !ZA_STRCMP(key, r.worldValue.key))
                {
                    for(za::SizeT i = 0; i < r.worldValue.values.count; ++i)
                    {
                        if(!q_strcasestr(com_token, r.worldValue.values.items[i])) { continue; }
                        addSignal(mapRules.found[r.id], key);
                        break;
                    }
                }
            }
            if(ZA_STRCMP(key, "classname")) { continue; }
            bool dup = false;
            for(const za::String& s : seen) { dup |= !ZA_STRCMP(s.cStr(), com_token); }
            if(dup) { continue; }
            seen.pushBack(za::String{com_token});
            int owner = -1;
            for(const Ruleset& r : rulesets)
            {
                if(counting[r.id] && (ownClassname(r, com_token) || r.extraSignals.has(com_token)))
                {
                    owner = owner < 0 ? r.id : -2;
                }
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
        mapRules.campaign ? (mapRules.package[0] ? "the package's mod" : "the campaign's") :
        mapRules.id == qvr::rules::ID ? "no ruleset detected" : "detected");
    if(mapRules.package[0]) { Con_Printf("Map Library package made for %s\n", mapRules.package); }
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
    Id own = c >= 0 && c < int(countof(campaignRulesets)) ? campaignRulesets[c] : ID;
    countSignals(sv.worldmodel ? sv.worldmodel->entities : "");
    mapRules.importedSpawns = mapRules.oursShared = 0;
    mapRules.package[0] = '\0';
    // A Map Library package made for a ruleset's mod (vr_mapinstall.cpp madeFor: "Copper", "Dwell", "Quoth") plays by it, like
    // a campaign, when the map is the package's own.
    const za::String& sha = qvr::mapinstall::active();
    const char* mapName = sv.name;
    if(own == ID && sha.size() && qvr::mapinstall::packageFor(mapName) == sha)
    {
        if(const qvr::mapindex::Entry* e = qvr::mapindex::find(sha))
        {
            const za::String mod = qvr::mapinstall::madeFor(*e);
            for(const Ruleset& r : rulesets)
            {
                for(za::SizeT i = 0; i < r.madeFor.count && own == ID; ++i)
                {
                    if(!q_strcasecmp(mod.cStr(), r.madeFor.items[i])) { own = r.id; }
                }
            }
            q_strlcpy(mapRules.package, mod.cStr(), sizeof(mapRules.package));
        }
    }
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
    }
    // A content row's version (spawnsOutside): of a classname nobody else spawns, or of its clash under its ruleset.
    for(const Ruleset& o : rulesets)
    {
        if(o.id == r.id || !o.prefix || !o.spawnsOutside) { continue; }
        if(ours && !(o.clashesUnder == r.id && o.ownedClashes.has(classname))) { continue; }
        if(o.spawnsOutside(classname)) { return va("%s%s", o.prefix, classname); }
    }
    return ours ? classname : nullptr;
}

// pr_edict.c ED_ParseEdict: the prefix of the fields the map's ruleset's code reads its own map keys through (a key
// Quake VR has no field of that type for: ad_<key>, cu_<key>), or null.
extern "C" const char* VR_Rules_KeyPrefix(void)
{
    return rulesets[mapRules.id].prefix;
}

// The toolgun's explicit variants (box3d::spawnClass): a classname with a ruleset's prefix (ad_monster_gaunt,
// cu_func_bobbing) spawns that ruleset's version only under its rules, or where its row lets the toolgun make it
// outside them (AD's toolgunOutside). Returns 0 for a classname with no ruleset's prefix, else 1 (`*plain` the
// classname without it) or -1 (refused).
extern "C" int VR_Rules_ToolgunPrefixed(const char* classname, const char** plain)
{
    for(const Ruleset& r : rulesets)
    {
        const za::SizeT n = r.prefix ? ZA_STRLEN(r.prefix) : 0;
        if(!n || ZA_STRNCMP(classname, r.prefix, n)) { continue; }
        *plain = classname + n;
        return mapRules.id == r.id || (r.toolgunOutside && r.toolgunOutside(classname + n)) ? 1 : -1;
    }
    return 0;
}
