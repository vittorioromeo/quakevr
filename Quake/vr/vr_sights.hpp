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
