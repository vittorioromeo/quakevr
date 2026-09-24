// vr_engine.hpp -- lets the C++ VR module use the (C) engine headers.
//
// Always include this instead of quakedef.h from VR sources. glm is included first because
// the engine defines function-like macros (DotProduct, VectorCopy, ...) that must not leak
// into glm's templates.

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

extern "C" {
#include "quakedef.h"

// Engine globals that no engine header declares.
extern cvar_t sv_gravity;
int ED_FindFieldOffset (const char *name);
extern qboolean scr_drawloading;
extern cvar_t crosshair;
void M_DrawSlider (int x, int y, float range, const char *desc);
void M_DrawArrowCursor (int cx, int cy);
}

#include "vr_api.h"
