// vr_sights.hpp -- the weapons' glowing iron sights in a colour of the player's choosing
// (vr_sight_hue, vr_sight_saturation). The sights are painted into the skins of the shotgun and the
// double shotgun (and the shotgun's pickup) in the fullbright "fire" palette indices, 224..239, 252
// and 253 (a gradient from deep red through orange to pale yellow; nothing else those skins show
// uses them: Misc/quakevr/recolor_shotgun_sight.py). Those skins are uploaded with a palette whose
// fire entries are turned to the chosen hue, keeping each entry's brightness and relative saturation
// (so the gradient keeps its shape), and uploaded again when the cvars change. The glow
// (vr_weapon_glow, the bloom) comes from those texels, so it takes the same colour.
//
// A C header: gl_texmgr.c calls these.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// TexMgr_LoadImage8: the palette the 8-bit texture `texname` is uploaded with, `palette` (the one
// Quake would use) or, for a sighted weapon's skin with a hue other than the sights' own, a copy of
// it with the sight colours recoloured (valid until the next call).
unsigned int* VR_SightPalette(const char* texname, unsigned int* palette);

// gl_texmgr.c: uploads again the textures whose names start with `prefix` (from their source).
void TexMgr_ReloadImagesNamed(const char* prefix);

#ifdef __cplusplus
}
#endif
