// vr_detail.hpp -- detail textures (vr_detail; docs/vr-port/ROUND17.md, "Detail textures"). In VR walls are always
// close, and Quake's textures (QRP's too) turn into big blurry texels there. As in DarkPlaces and Quake 3, a fine
// tiled grain (stone, brushed metal, wood, grime, organic cells, plaster) is multiplied over the world's and brush
// models' textures around mid-grey (the average brightness is unchanged), faded in within a metre or two of the eye.
//
// The grains are images, quakevr/textures/vr/detail_<kind>.png (Misc/quakevr/make_detail.py), in one texture array
// on unit 12. Which one a texture gets, at what size and strength, comes from quakevr/textures/vr/detail.cfg (texture
// name patterns, documented there), then from the texture's colours; wood's and brushed metal's grain is turned to
// run along the texture's own. The world shader (DetailFactor in gl_shaders.h) reads the array at the texture's own
// coordinates (after parallax), scaled to world units, with a finer second octave very close; nothing past
// vr_detail_distance.

#pragma once

namespace qvr::detail
{

// VR_Init: the vr_detail_reload command.
void init();

} // namespace qvr::detail
