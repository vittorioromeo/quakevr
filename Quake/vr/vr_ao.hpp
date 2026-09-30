// vr_ao.hpp -- dynamic ambient occlusion (round 20; docs/vr-port/ROUND20.md, "Dynamic ambient occlusion").
//
// The static world has its ambient occlusion baked (ericw-tools' -dirt in the relight) and models get the world's
// occlusion through their ambient cube (vr_ambient.cpp). What moves has none; three cheap world-space terms (the same
// in both eyes: no screen-space pass) add it:
// - Ellipsoids round monsters, items, gibs, heads, backpacks and your body's parts (vr_ao_dynamic): each frame the
//   nearest 64 occluders (their frame's box, oriented and scaled as drawn; your body from its posed bones) go to the
//   shaders in a small uniform block, binned per eye into the light clusters' 32 x 16 screen tiles and 32 depth
//   slices. The world and model shaders darken the baked light (and the models' own) by each one's analytic
//   occlusion (a sphere's form factor in the ellipsoid's unit-sphere space), fading out at vr_ao_dynamic_range radii.
//   An occluder never darkens its own model (a group id per model; your body, hands and weapons are one group).
// - Boxes round brush models (vr_ao_brush): doors, lifts, platforms, trains, the item boxes; the form factor of the
//   box's faces seen from the pixel (Lambert's polygon formula). Brush models cast no shadows and no -dirt in the
//   map's lighting unless the mapper set "_shadow" (ericw-tools' ltface.cc), so they get it wherever they are; those
//   with "_shadow" only as far as they have moved from where the map lit them.
// - Per-vertex occlusion baked into every Quake model (.mdl) as it loads (vr_ao_models): for each pose, rays from each
//   vertex over its hemisphere against the model's own triangles (armpits, under guns, weapon grooves), kept in the
//   pose buffer's spare byte, applied to the model's own light (half to dynamic lights). Baked on a worker thread
//   (the model has none for the first second or two), cached per model.
// The GPU side is the AO_FUNCTIONS block of vr_glsl.h.

#pragma once

// The engine's calls (VR_AliasVertexAO, VR_AliasAO, VR_BrushAOSelf) are declared in vr_api_render.h.


namespace qvr::ao
{

// The game directory changed (VR_OnGameDirChanged): Mod_ResetAll reuses the models' slots for other models, and the
// files are another game's; the baked occlusion by model name, the brush models' by model, the frame's occluders dropped.
void onGameDirChanged();

void init();
// Host_Shutdown (VR_Shutdown): the bake worker stopped and joined (a bake under way given up).
void shutdown();

// VR_PushMapLights (each eye's R_PushDlights): the occluders, chosen once a frame, binned into this eye's tiles and
// bound as the shaders' uniform block (binding 2). Always binds one (empty when off).
void upload();

} // namespace qvr::ao

