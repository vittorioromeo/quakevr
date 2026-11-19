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
};

bool never() { return false; }

constexpr Ruleset rulesets[] = {
    {qvr::rules::ID, "Quake", nullptr, false, 0, never, {}, {}, {}},
    {qvr::rules::MG, "Machine Games", nullptr, false, 0, never, {}, {}, {}},
    {qvr::rules::AD, "Arcane Dimensions", "ad_", true, 2, qvr::ad::folderMounted, names(adNotSignals),
        names(adNotSignalPrefixes), names(adWorldSignals)},
    // Copper's (DWELL.md 5.2): its signals (its 48 own classnames, the worldspawn _tb_mod key) come with its import (P1a).
    {qvr::rules::COPPER, "Copper", "cu_", false, 2, never, {}, {}, {}},
};
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

void registerCommands()
{
    Cmd_AddCommand("vr_ruleset_info", infoCommand);
}

} // namespace qvr::rules
