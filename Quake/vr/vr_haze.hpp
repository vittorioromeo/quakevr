// vr_haze.hpp -- heat haze: the air shimmering over lava, round explosions and over flames.
//
// What is behind hot air is drawn bent: after the translucent pass (VR_DrawHeatHaze, gl_rmain.c), the part of the
// scene the hot air covers on screen is copied (resolved, with multisampling), and invisible volumes are drawn over
// the scene with a shader that reads the copy a little off to the side. The volumes: a layer kLavaHeight units deep
// over every level lava face of the world (its top and, where the lava meets rock, its sides: vr_water.cpp's lava
// tops, made with the geometric waves' mesh), a sphere round each explosion growing and fading over most of a second
// (VR_HazeExplosion, cl_tent.c), and a tall ellipsoid over each flame model (progs/flame*.mdl) near the eye. The shader
// follows the view ray through the volume (the lava layer hotter near the surface, a sphere denser in its middle,
// stopped by the scene: vr_water.cpp's scene distances), and the bend is a shift in the world by a rising 3D noise at
// a point on that ray, projected: the same air in both eyes. What is in front of the hot air is not read into it (by
// the scene's distances), and the particles drawn after it stay sharp. vr_heat_haze (0 off .. 1).

#pragma once

namespace qvr::haze
{

// Sets vr_heat_haze for a graphics preset (0 off .. 4 ultra): off in Off (Quake) and Low.
void applyPreset(int preset);

} // namespace qvr::haze
