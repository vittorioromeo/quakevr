// vr_cvars.hpp -- Quake VR cvars by name (definitions generated from vr_cvars.inc): include it only where one is read
// (vr_x.value), so an edit of the list recompiles those files alone. The rest that was here has headers of its own:
// - vr_tunables.hpp: the tuning constants by name (vr_tunables.inc); their type and functions: vr_tunables_api.hpp;
// - vr_monstervalues.hpp, vr_weaponvalues.hpp: the data tables' named values (vr_monsterdata.inc, vr_weaponvalues.inc);
// - vr_cvars_util.hpp: cvars::forEach, cvars::Override, cvars::Snapshot;
// - vr_config.hpp: registerCvars, the config's writer (saveConfigNow, configFrame), standInTables.

#pragma once

// Only the cvar type: modules that need nothing else of the engine stay free of its headers.
extern "C" {
#include "q_stdinc.h"
#include "cvar.h"
}

#include <initializer_list>

namespace qvr
{

#define QVR_CVAR(name, def, flags) extern cvar_t name;
#include "vr_cvars.inc"
#undef QVR_CVAR

extern cvar_t vr_backend;

// The weapon table's slots (vr_weapons.hpp numSlots; vr_weaponvalues.hpp's table). Here, not there:
// gen_vr_cvars_qc reads it from this file.
inline constexpr int weaponSlotCount = 52;

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

} // namespace qvr
