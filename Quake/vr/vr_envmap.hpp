// vr_envmap.hpp -- rim light and environment reflections on models (docs/vr-port/ROUND17.md, "Rim light and
// weapon reflections").
//
// Rim light (vr_rim_light): a faint light round the edges of models facing away from the eye (fresnel), tinted by
// the light around the model (its ambient cube, vr_ambient.cpp, towards the back), so that monsters stand out from
// dark backgrounds. The alias shader does it per pixel, from each eye's own position; this module gives each
// instance its strength (monsters and items full, held weapons less, your hands a little, your body none).
//
// Weapon reflections (vr_weapon_reflections): a small cube map (6 x 64 x 64, mipmapped) of the world round your
// hands, the lightmapped world only (each texture its average colour, its fullbrights glowing), one face a frame
// while you move and one every few frames while you stand, rendered once for both eyes. The alias shader adds it on
// the grey and blue-grey (metal) texels of held weapons' skins (and of weapon pickups close by), by fresnel, a
// per-model metalness and the light at the model.
//
// Water reflections (vr_water_reflections): another cube (6 x 128 x 128, RGBA16F, mipmapped) of the world round
// the level water or slime nearest your head that you see from above, seen from just over its surface: the faces'
// own textures, lit by the lightmaps, each texel's distance from the cube's centre in alpha. The liquid shaders
// (vr_glsl.h, LiquidReflection) reflect it off the waves, following each reflected ray out to those distances (a
// reflection of where things are, not at infinity), on the faces at that height. Two cubes in turn, two faces a
// frame into the one not read; once a frame for both eyes.

#pragma once

#include "vr_engine.hpp"

namespace qvr::envmap
{

// Brings the cubes up to date (the weapons': a face at most; the water's: two): once a frame, before the eyes are
// rendered.
void update();

// The water's cube in a view's frame data (gl_shaders.h, WaterCube and WaterCube2): where it is seen from and its
// strength (0: none this frame, or the eye in a liquid), the height of the surface it is for, how far across it
// fades out, its mip levels read.
void waterFrameData(float out[4], float out2[4], bool eyeInLiquid);

// Frees the GL objects.
void shutdown();

// Its command (vr_envmap_dump).
void init();

} // namespace qvr::envmap
