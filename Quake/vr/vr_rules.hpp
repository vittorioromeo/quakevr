#pragma once
// vr_rules.hpp -- the ruleset a map plays by (docs/vr-port/DWELL.md section 8, S1): one id for the map now spawning or
// running, set before its entities spawn and published as vr_ruleset (QC VR_Ruleset()). Quake's (with Hipnotic's,
// Rogue's and Honey's meanings on their own classnames), Machine Games', Arcane Dimensions', Copper's.

namespace qvr::rules
{

// The rulesets (QC vr_packutil.qc VR_RULES_*: the same numbers).
enum Id : int
{
    ID = 0,     // Quake's
    MG = 1,     // Machine Games' (Dimension of the Past, of the Machine, Dawn of the Machine)
    AD = 2,     // Arcane Dimensions'
    COPPER = 3, // Copper's (and Dwell's): defined, not detected yet (DWELL.md P1b)
    Count
};

// SV_SpawnServer, before the map's entities spawn (VR_OnSpawnServerBeforeLoad): the map's ruleset, from the campaign's
// own, else the entity text's signals (each detecting ruleset's, scored together), published as vr_ruleset.
void beforeLoad();

// The ruleset of the map now spawning or running.
[[nodiscard]] Id current();

// How it was found: the campaign's own ruleset (true), or the signals.
[[nodiscard]] bool byCampaign();

// The signals of ruleset `r` in the map's entity text (its own classnames, its worldspawn keys), and the first ones.
[[nodiscard]] int signals(Id r);
[[nodiscard]] const char* firstSignals(Id r);

// vr_ruleset_info: the map's ruleset, why, and every ruleset's signals.
void registerCommands();

} // namespace qvr::rules
