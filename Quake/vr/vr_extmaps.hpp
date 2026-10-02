// vr_extmaps.hpp -- external material maps for the world's textures (vr_extmaps; docs/vr-port/ROUND21.md, "External
// material maps"). A texture pack made for Quake's textures may come with maps of its own beside its pictures: Quetoo's
// (jdolan's quetoo-data, textures/quake: CC-BY-SA 4.0) has <name>_norm (a normal map, its height in alpha), <name>_spec
// (a specular colour), <name>_luma / _glow and <name>.mat (Quetoo's material: specularity, hardness, and which maps).
// Quake VR ships a subset of them, unchanged (quakevr/textures_quetoo/: its README.md has the credits and licence;
// Misc/quakevr/select_quetoo_maps.py picks them), the default vr_extmaps_dir. Image_LoadImage reads a "vrext/<file>" path from that folder
// (VR_ExtMapsOpen), so the textures made from them reload like any other.
//
// A pack's name for a texture: lower case, a liquid's '*' dropped, an animation's frame moved to the end (+0button is
// button+0). Its maps are used only where its own picture matches the texture drawn (vr_extmaps_match: the correlation
// of their detail at 64 x 64; a repainted picture's bumps don't fit the art on the wall), on regular textures (not
// liquids, not the sky). The normal map is kept beside the one made from the shading, the specular map and the .mat's
// numbers per texture (texture_t), so vr_extmaps_normals, vr_extmaps_spec, vr_extmaps_luma and the Debug menu's A/B
// (vr_extmaps_ab) change at once; vr_extmaps itself takes effect on the next map.

#pragma once

namespace qvr::extmaps
{

// VR_Init: the vr_extmaps_stats command.
void init();

} // namespace qvr::extmaps
