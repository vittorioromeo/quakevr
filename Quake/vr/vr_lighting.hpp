// vr_lighting.hpp -- real-time shadows and per-pixel dynamic lights on Ironwail's renderer.
//
// Research and design: docs/vr-port/LIGHTING.md. In short:
// - Shadowed dynamic lights (explosions, rockets, muzzle flashes; vr_shadow_dlights): the most
//   important few get six cube faces in one depth atlas, sized by distance, with the world, doors
//   and lifts, and monsters as casters. The world's clustered light loop looks their shadow up.
// - Map lights' shadows of moving things (vr_shadow_maplights): the few map lights nearest the
//   player cast the shadows of monsters and the player onto the baked light. Each has the world's
//   depth cached once and the moving things' depth redrawn every frame; the world shader removes
//   the light's baked share where only a moving thing blocks it.
// - Dynamic lights on models per pixel (vr_dlight_models), shaded by angle and shadowed.
// Everything is rendered once per frame and shared by both eyes. This is the renderer-specific
// part of the module (OpenGL, Ironwail's shaders): a vkQuake port rewrites it.

#pragma once

namespace qvr::lighting
{

// Apply a quality preset (vr_graphics_preset): 0 off (Quake's own look) .. 4 ultra.
void applyPreset(int preset);

void init();

} // namespace qvr::lighting
