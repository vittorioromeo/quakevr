// vr_water.hpp -- liquids: water, slime, lava and teleports less flat, and the view with your head in them.
//
// The surfaces (gl_shaders.h, LIQUID_FUNCTIONS; both of Ironwail's liquid programs, lit and unlit water): waves as a
// sum of sines in the world (the faces stay flat: Ironwail does not subdivide them, so the waves are in the normal,
// the texture's warp and the refraction, not the geometry), a fresnel term (see-through looking down, a dim room
// colour at grazing angles), glints from a light above and from dynamic lights, what is under translucent water bent
// by the waves (read from the opaque scene while translucent things draw into the OIT buffers: no copy), lava
// glowing to bloom, teleports shimmering. Caustics: the world shader dapples the light on whatever is in a water or
// slime leaf (a coarse 3D texture of the map's liquids, made as a map loads). Under water, in the eyes: the engine's
// fog in the liquid's colour, Quake's colour shift in it too, and in the eye's post-processing a slow wobble and a
// little blur (r_waterwarp's screen warp is off then). Everything is in the world or per view direction, the same in
// both eyes but for the view vector. vr_water_* (vr_cvars.inc); 0 is Quake's look. The C hooks are VR_WaterView,
// VR_WaterFog and VR_PostProcessWater (vr_api_render.h).

#pragma once

namespace qvr::water
{

// Sets the vr_water_* cvars for a graphics preset (0 off .. 4 ultra).
void applyPreset(int preset);

} // namespace qvr::water
