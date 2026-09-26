// vr_lighting.hpp -- real-time shadows and per-pixel dynamic lights on Ironwail's renderer.
//
// Research and design: docs/vr-port/LIGHTING.md. In short:
// - Shadowed dynamic lights (explosions, rockets, muzzle flashes; vr_shadow_dlights): the most
//   important few get six cube faces in one depth atlas, sized by distance, with the world, doors
//   and lifts, and monsters as casters. The world's clustered light loop looks their shadow up.
//   A spot light (dlightSpot: the flashlight) lights only its cone and takes one tile round it.
// - Map lights' shadows of moving things (vr_shadow_maplights): the few map lights nearest the
//   player cast the shadows of monsters and the player onto the baked light. Each has the world's
//   depth cached once and the moving things' depth redrawn every frame; the world shader removes
//   the light's baked share where only a moving thing blocks it.
// - Dynamic lights on models per pixel (vr_dlight_models), shaded by angle and shadowed.
// Everything is rendered once per frame and shared by both eyes. This is the renderer-specific
// part of the module (OpenGL, Ironwail's shaders): a vkQuake port rewrites it.

#pragma once

#include "vr_engine.hpp"

namespace qvr::lighting
{

// Apply a quality preset (vr_graphics_preset): 0 off (Quake's own look) .. 4 ultra.
void applyPreset(int preset);

// VR_TuneDlight, for DarkPlaces' lights (vr_dlight_falloff): the share of the light that reaches
// what faces away from it (an explosion's), and the seconds its colour fades out over before it
// dies (a muzzle flash's; 0 none). Set after its key and death time.
void dlightLook(const dlight_t* dl, float ambient, float fade);

// A small glow that casts no shadow (projectiles, the weapons' ammo screens: vr_emissive.cpp): it
// never takes one of vr_shadow_dlights' shadows. Set after its key and death time.
void dlightNoShadow(const dlight_t* dl);

// A spot light (the flashlight): its light only within a cone along `dir`, full inside
// `innerDegrees` of it and smoothly down to none at `outerDegrees` (half angles). Shadowed, it takes
// one square tile round the cone (a perspective shadow map) rather than six cube faces. Set after
// its key and death time, every frame it is kept.
void dlightSpot(const dlight_t* dl, const glm::vec3& dir, float innerDegrees, float outerDegrees);

void init();

// Lights given shadows this frame: dynamic ones, and map lights (vr_memstats).
void shadowCounts(int& dlights, int& mapLights);

} // namespace qvr::lighting
