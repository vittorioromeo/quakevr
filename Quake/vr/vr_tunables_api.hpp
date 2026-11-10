// vr_tunables_api.hpp -- the tuning constants' type and functions, without their table: what the code that lists, finds
// or sets them by number or by name needs (the Tunables page, the config's reader, the QC's cvar()). Their names
// (vr_x.value) are in vr_tunables.hpp, which includes vr_tunables.inc: only the files that read one include it, so an
// edit of the table recompiles those alone.

#pragma once

// Only the cvar type: modules that need nothing else of the engine stay free of its headers.
extern "C" {
#include "q_stdinc.h"
#include "cvar.h"
}

namespace qvr
{

// Tuning constants (the settings' tiers, ROUND23.md "Settings tiers"): vr_tunables.inc's QVR_TUNABLE(name, default),
// numbers tuned once that no player sets: no menu row of their own, not in the config. Read as a cvar is (name.value,
// name.string), the same code in every build:
// - A development build (every build but a release: the agents', the author's) tunes them live (vr_tunables.cpp,
//   "Live tuning"): each name is a reference into tunables[] (by its TunableId), set by `tune <name> <value>`, the
//   Debug > Tunables page or tunables.cfg (the changed ones, written at quit, read after the config); a read is one
//   load from a fixed address.
// - A release build (QVR_RELEASE: make_release.ps1's /p:QvrReleaseVersion, or /p:QvrRelease=1) makes each a constexpr
//   Tunable, its default folded in; no tune, no page, no tunables.cfg.
// The QC reads them by name in both (cvar(), cvar_hmake: tunableFind's stand-in cvars), so progs.dat is the same; an
// old config's line for one is ignored. New defaults: Misc/quakevr/tune_promote.py writes a tunables.cfg's values
// into vr_tunables.inc (a source edit, on a worker's tree; the game never writes sources).
struct Tunable
{
    const char* name;
    const char* string;
    float value;
};

// Q_atof's value of a decimal default, at compile time (its digits, then one division by 10 a decimal; the same float).
[[nodiscard]] constexpr float tunableValue(const char* s)
{
    double val = 0.0;
    double sign = 1.0;
    if(*s == '-')
    {
        sign = -1.0;
        ++s;
    }
    int decimal = -1;
    int total = 0;
    for(;; ++s)
    {
        if(*s == '.')
        {
            decimal = total;
            continue;
        }
        if(*s < '0' || *s > '9')
        {
            break;
        }
        val = val * 10 + (*s - '0');
        ++total;
    }
    if(decimal != -1)
    {
        for(; total > decimal; --total)
        {
            val /= 10;
        }
    }
    return static_cast<float>(val * sign);
}

// A tuning constant's stand-in cvar by name (not registered: the console and the config never see it), for the QC's
// cvar() and cvar_hmake (its string and value are the constant's now); nullptr for any other name.
[[nodiscard]] const cvar_t* tunableFind(const char* name);

// A setting by name, or (a development build) a tuning constant's stand-in, which Cvar_SetQuick sets live: what the
// rows named by text (the menus' by-name rows, the Ragdoll Tool's registry) show and set. nullptr if neither.
[[nodiscard]] cvar_t* settingByName(const char* name);

void tunablesInit(); // the stand-ins' check; in a development build tune, tune_reset and the Tunables page's settings
void tunablesLoad(); // tunables.cfg, once, after the config (vr_migrate_config; nothing in a release build)
void tunablesTakeConfigLines(const char* path); // a config's lines for settings that are constants now (vr_cvars.cpp)
void tunableTakeConfigLine(const char* name, const char* value); // one: kept as if tuned, and said (nothing in a release)

#ifndef QVR_RELEASE
// Live tuning (a development build; vr_tunables.cpp).
struct TunableChoice
{
    float value;
    const char* label;
};

// A constant's row on the Tunables page (vr_tunables_rows.inc): the menu row it had, or one made from its default.
struct TunableRow
{
    enum Kind : char
    {
        Group, // a header: the rows below came from this page ("Combat > Melee")
        Slider,
        Cycle,
        Text // a text or a vector: set in the console only
    };
    Kind kind;
    int id; // TunableId (-1: a Group)
    const char* label;
    float min, max, step;
    const char* format;
    float hardMin, hardMax; // left and right go on past the bar's ends as far as these
    const char* help;
    const TunableChoice* choices;
    int choiceCount;
};

[[nodiscard]] int tunableIndex(const char* name);         // its TunableId, -1 if none
[[nodiscard]] const char* tunableDefault(int id);         // its default (vr_tunables.inc)
[[nodiscard]] bool tunableChanged(int id);                // set off its default
[[nodiscard]] int tunablesChanged();                      // how many are
[[nodiscard]] const TunableRow& tunableRow(int id);       // its row
[[nodiscard]] const char* tunableGroup(int id);           // the page it came from ("Other": none)
[[nodiscard]] const TunableRow* tunableRows(int* count);  // the whole table, groups and rows in order
[[nodiscard]] cvar_t* tunableStandIn(int id);             // the Tunables page's rows show and set these
void tunableSet(int id, const char* value);               // (its default: no longer changed)
void tunableReset(int id);
void tunablesResetAll();
[[nodiscard]] cvar_t& tunablesShowChangedCvar(); // vr_tunables_show_changed: only the ones off their defaults
#endif

} // namespace qvr
