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
// numbers tuned once that no player sets. A release build makes each a constexpr Tunable, read as a cvar is (.value,
// .string); a tuning build (MSBuild -p:QvrTuning=1: QVR_TUNING) makes each an archived cvar again, for the console. The
// QC reads them by name either way (cvar(), cvar_hmake: tunableFind), and an old config's line for one is ignored.
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

#ifdef QVR_TUNING
#define QVR_TUNABLE(name, def) extern cvar_t name;
#else
#define QVR_TUNABLE(name, def) inline constexpr Tunable name{#name, def, tunableValue(def)};
#endif
#include "vr_tunables.inc"
#undef QVR_TUNABLE

// A tuning constant's stand-in cvar by name (not registered: the console and the config never see it), for the QC's
// cvar() and cvar_hmake; nullptr for any other name (and for all in a tuning build, where they are cvars).
[[nodiscard]] const cvar_t* tunableFind(const char* name);

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
