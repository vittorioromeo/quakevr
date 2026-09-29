// vr_cvars.hpp -- Quake VR cvars (definitions generated from vr_cvars.inc).

#pragma once

// Only the cvar type: modules that need nothing else of the engine stay free of its headers.
extern "C" {
#include "q_stdinc.h"
#include "cvar.h"
}

namespace qvr
{

#define QVR_CVAR(name, def, flags) extern cvar_t name;
#include "vr_cvars.inc"
#undef QVR_CVAR

extern cvar_t vr_backend;

void registerCvars();

// The game folder's config written now if an archived setting changed since it was last read or written (not in a copy
// started with -noconfigwrite); configFrame: so, each time the menu closes. The config was written only when the game
// quit, and a game ended otherwise (stopped from the debugger, killed) lost the session's settings.
void saveConfigNow();
void configFrame();

} // namespace qvr
