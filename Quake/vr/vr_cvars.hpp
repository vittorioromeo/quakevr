// vr_cvars.hpp -- Quake VR cvars (definitions generated from vr_cvars.inc).

#pragma once

// Only the cvar type: modules that need nothing else of the engine stay free of its headers.
extern "C" {
#include "q_stdinc.h"
#include "cvar.h"
}

#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"

#include <initializer_list>

namespace qvr
{

#define QVR_CVAR(name, def, flags) extern cvar_t name;
#include "vr_cvars.inc"
#undef QVR_CVAR

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

// Each constant's number: its line in vr_tunables.inc (tunableId_<name>).
enum TunableId : int
{
#define QVR_TUNABLE(name, def) tunableId_##name,
#include "vr_tunables.inc"
#undef QVR_TUNABLE
    tunableCount
};

#ifdef QVR_RELEASE
#define QVR_TUNABLE(name, def) inline constexpr Tunable name{#name, def, tunableValue(def)};
#else
extern Tunable tunables[tunableCount]; // (vr_tunables.cpp) the values now: the defaults, or what tune set
#define QVR_TUNABLE(name, def) inline constexpr Tunable& name = tunables[tunableId_##name];
#endif
#include "vr_tunables.inc"
#undef QVR_TUNABLE

// A tuning constant's stand-in cvar by name (not registered: the console and the config never see it), for the QC's
// cvar() and cvar_hmake (its string and value are the constant's now); nullptr for any other name.
[[nodiscard]] const cvar_t* tunableFind(const char* name);

void tunablesInit(); // the stand-ins' check; in a development build tune, tune_reset and the Tunables page's settings
void tunablesLoad(); // tunables.cfg, once, after the config (vr_migrate_config; nothing in a release build)

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
[[nodiscard]] cvar_t& tunablesPageCvar();        // vr_tunables_page: the Tunables page shows one page's (from 1; 0 all)
[[nodiscard]] cvar_t& tunablesShowChangedCvar(); // vr_tunables_show_changed: only the ones off their defaults
#endif

extern cvar_t vr_backend;

// Recording mode (vr_recording_clean, the player's; vr_recording_clean_map, the map's: the trailer scene): no tips, no
// messages on the wrist gadget and no head-locked text, in the headset too.
[[nodiscard]] inline bool recordingClean()
{
    return vr_recording_clean.value != 0.f || vr_recording_clean_map.value != 0.f;
}

// The highest reloading mode of any gun: the global vr_reload_mode and the Per-Weapon Reload Overrides (vr_reload_mode_<gun>,
// -1 follows the global). 3 (Immersive) when any gun is loaded by hand.
[[nodiscard]] inline int reloadModeBest()
{
    int best = static_cast<int>(vr_reload_mode.value);
    for(const cvar_t* gun : {&vr_reload_mode_shot, &vr_reload_mode_sshot, &vr_reload_mode_nail, &vr_reload_mode_snail,
            &vr_reload_mode_gl, &vr_reload_mode_prox, &vr_reload_mode_rl, &vr_reload_mode_light, &vr_reload_mode_widow,
            &vr_reload_mode_adplasma})
    {
        best = static_cast<int>(gun->value) > best ? static_cast<int>(gun->value) : best;
    }
    return best;
}

void registerCvars();

// Two settings' values the same (as numbers when both are): what a sparse config's "changed from the default" means.
[[nodiscard]] bool sameSettingValue(const char* a, const char* b);
// vr_config_test: the sparse config's round trip (vr_config_diag.cpp).
void registerConfigDiag();

// The game folder's config written now if an archived setting changed since it was last read or written (not in a copy
// started with -noconfigwrite); configFrame: so, each time the menu closes. The config was written only when the game
// quit, and a game ended otherwise (stopped from the debugger, killed) lost the session's settings.
void saveConfigNow();
void configFrame();

} // namespace qvr

namespace qvr::cvars
{

// fn(cvar_t*) for every cvar the engine has, in its list's order (by name).
template <typename F>
void forEach(F&& fn)
{
    for(cvar_t* var = Cvar_FindVarAfter("", 0); var; var = var->next)
    {
        fn(var);
    }
}

// A cvar set for a scope (RAII): `value` now, its string before put back when the Override goes (a test's setting).
class Override
{
public:
    Override(cvar_t& var, float value);
    Override(cvar_t& var, const char* value);
    ~Override();
    Override(const Override&) = delete;
    Override& operator=(const Override&) = delete;

private:
    cvar_t& var;
    za::String was;
};

// Cvars' values kept before a run changes them (a playback, a scenario) and put back by restore(): the latest kept
// first, so a cvar kept twice ends at its first kept value. Not on destruction (a file's own one lives to the exit).
class Snapshot
{
public:
    void keep(cvar_t& var);
    void restore();
    [[nodiscard]] bool empty() const { return saved.empty(); }
    [[nodiscard]] int size() const { return static_cast<int>(saved.size()); }

private:
    struct Saved
    {
        cvar_t* var;
        za::String value;
    };
    za::Vector<Saved> saved;
};

} // namespace qvr::cvars
