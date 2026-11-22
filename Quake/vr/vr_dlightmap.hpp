#pragma once

#include "vr_cvars.hpp"
#include "vr_tunables.hpp" // (vr_gl_alias_body)

// The dynamic lights in the lightmap (vr_gl_world_dlmap, the ES profile; vr_dlightmap.cpp).
namespace qvr::dlightmap
{
// VR_WorldLightsCheck, each view: the frame's lights summed into the light texture (the frame's first view does it), or
// made dark when off. True: they are there, the view draws the world without the lights' loop.
bool frame();
void registerCommands(); // vr_gl_world_dlmap_info

// C5g: "Quake-Style Dynamic Lights (Quest)" (vr_gl_quake_dlights): the one choice for both of Quake's ways, the world's
// lights summed once a frame into its light texture (as vr_gl_world_dlmap 1) and your body's added to its light once a
// frame on the CPU (as vr_gl_alias_body 2); each light as the per-pixel loop gives it, added after the baked light's curve.
[[nodiscard]] inline bool quakeLights()
{
    return vr_gl_quake_dlights.value != 0.f;
}
[[nodiscard]] inline bool worldWanted()
{
    return quakeLights() || vr_gl_world_dlmap.value != 0.f;
}
// vr_gl_alias_body as it acts: 2 (Quake's way) under the choice.
[[nodiscard]] inline float aliasBodyMode()
{
    return quakeLights() ? 2.f : vr_gl_alias_body.value;
}
} // namespace qvr::dlightmap
