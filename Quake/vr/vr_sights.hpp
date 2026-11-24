// vr_sights.hpp -- the weapons' glowing iron sights in a colour of the player's choosing
// (vr_sight_hue, vr_sight_saturation). The sights are painted into the skins of the shotgun and the
// double shotgun (and the shotgun's pickup) in the fullbright "fire" palette indices, 224..239, 252
// and 253 (a gradient from deep red through orange to pale yellow; nothing else those skins show
// uses them: Misc/quakevr/recolor_shotgun_sight.py). Those skins are uploaded with a palette whose
// fire entries are turned to the chosen hue, keeping each entry's brightness and relative saturation
// (so the gradient keeps its shape), and uploaded again when the cvars change. The glow
// (vr_weapon_glow, the bloom) comes from those texels, so it takes the same colour.
//

#pragma once

// VR_SightPalette (gl_texmgr.c calls it) is declared in vr_api_render.h, TexMgr_ReloadImagesNamed in vr_engine.hpp.
//
// The front post (vr_sight_front_hue_shift, by default on): its sight texels (each skin's patch the front post's
// triangles map, found from the models' geometry: Misc/quakevr/sight_front_posts.py) take a palette turned that much
// further round the hue circle, set by VR_SightFront after the skin's conversion, so the front post reads apart from
// the rear notches. Shotgun (and its pump parts), double shotgun (and its break-open parts), lightning gun and plasma
// gun, nail pistol.

namespace qvr::sights
{

void init(); // vr_sight_front_test: the front post table against the models' skins and the colours they upload with

} // namespace qvr::sights
