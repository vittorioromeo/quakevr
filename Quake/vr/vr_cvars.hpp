// vr_cvars.hpp -- Quake VR cvars (definitions generated from vr_cvars.inc).

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
            &vr_reload_mode_gl, &vr_reload_mode_prox, &vr_reload_mode_rl, &vr_reload_mode_light})
    {
        best = static_cast<int>(gun->value) > best ? static_cast<int>(gun->value) : best;
    }
    return best;
}

void registerCvars();

// The game folder's config written now if an archived setting changed since it was last read or written (not in a copy
// started with -noconfigwrite); configFrame: so, each time the menu closes. The config was written only when the game
// quit, and a game ended otherwise (stopped from the debugger, killed) lost the session's settings.
void saveConfigNow();
void configFrame();

} // namespace qvr
