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

} // namespace qvr
