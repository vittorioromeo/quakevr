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

#pragma once

#include "vr_engine.hpp"

namespace qvr::envmap
{

// Brings the cube up to date (a face at most): once a frame, before the eyes are rendered.
void update();

// Frees the GL objects.
void shutdown();

// Its command (vr_envmap_dump).
void init();

} // namespace qvr::envmap
