// vr_water.hpp -- liquids: water, slime, lava and teleports less flat, and the view with your head in them.
//
// The surfaces (gl_shaders.h, LIQUID_FUNCTIONS; both of Ironwail's liquid programs, lit and unlit water): waves as a
// sum of sines in the world (in the normal, the texture's warp and the refraction), and with vr_water_geo_waves long
// swells that move the geometry too (the world's level liquid faces cut into a grid as a map loads, their vertices
// raised in the vertex shaders, held still at the walls: vr_water.cpp's mesh, drawn by r_world.c), a fresnel term (see-through looking down, a dim room
// colour at grazing angles), glints from a light above and from dynamic lights, what is under translucent water bent
// by the waves (read from the opaque scene while translucent things draw into the OIT buffers; only what is behind
// the surface, by the scene's distances: no halo round what is in front of the water), lava
// glowing to bloom, teleports shimmering. Shoreline foam (vr_water_foam; LiquidFoam): where a level liquid meets a
// wall, a step or something standing in it, by the distance to the pool's rim (the mesh's, made with the foam on
// too) and to the scene behind the surface (the scene's distances, made for opaque liquids too, multisampled too);
// water foams white, slime gathers a dark scum, lava glows hot at the rock. The mesh's lava tops are the heat haze's
// (vr_haze.cpp). Caustics: the world shader dapples the light on whatever is in a water or
// slime leaf (a coarse 3D texture of the map's liquids, made as a map loads). Under water, in the eyes: the engine's
// fog in the liquid's colour, Quake's colour shift in it too, and in the eye's post-processing a slow wobble and a
// little blur of the scene (r_waterwarp's screen warp is off then; the HUD, menus, lasers and the wrist's log are
// drawn over the eye's image after it, unwobbled). Everything is in the world or per view direction, the same in
// both eyes but for the view vector. vr_water_* (vr_cvars.inc); 0 is Quake's look. The C hooks are VR_WaterView,
// VR_WaterFog, VR_WaterSceneDepth, VR_PostProcessWater and the mesh's VR_WaterMarkVis,
// VR_WaterMeshActive, VR_WaterMeshRanges and VR_WaterMeshBind (vr_api_render.h).

#pragma once

#include <glm/glm.hpp>

#include <vector>

namespace qvr::water
{

// Sets the vr_water_* cvars for a graphics preset (0 off .. 4 ultra).
void applyPreset(int preset);

// How far the opaque scene is from the eye in this view, along the view (an R32F texture of half the scene's size,
// each texel the nearest of its four; 0: none, as without a depth texture to read): the liquids' refraction's, made
// now if they did not make it this view. After the translucent pass only (the soft particles, vr_particles.cpp): it
// binds the scene's framebuffer again when it makes it.
[[nodiscard]] unsigned opaqueSceneDistances();

// The scene's depth was drawn into after its distances were made this view (the soft sprites, vr_particles.cpp):
// opaqueSceneDistances makes them again.
void sceneDepthChanged();

// The top of a level lava face of the world, for the heat haze over it (vr_haze.cpp): its polygon and the parts of its
// edges on the pool's rim (where the lava meets rock), at height z.
struct LavaTop
{
    int face = 0; // the geometric waves' mesh's
    float z = 0.f;
    std::vector<glm::vec3> poly;
    std::vector<glm::vec2> rim; // pairs of ends
};

// The world's lava tops (made with the geometric waves' mesh, which is made when the waves, the foam or the haze are
// on) and a number that changes when they are made anew.
[[nodiscard]] const std::vector<LavaTop>& lavaTops(unsigned& generation);

// Whether a lava top is in this view's PVS (VR_WaterMarkVis, this view).
[[nodiscard]] bool lavaTopInPvs(const LavaTop& top);

// Ripples (vr_water_ripples): a splash `strength` hard (Preset::Splash's count: 4 a shot, 6-15 a hand or a thrown
// thing, 20-50 a body) at `at` on a liquid's surface, rings of waves spreading from it in the liquid's shading and,
// on the geometric waves' mesh, in its geometry. The client's; each frame's are in the frame data (gl_shaders.h,
// LiquidRipples).
void addRipple(const glm::vec3& at, float strength);

// How fast the ripples spread on a liquid (CONTENTS_*: lava's slow, slime's slower than water's), in units a second:
// the splash's rings ride their crest.
[[nodiscard]] float rippleSpeed(int contents);

// How far this view's geometric waves and ripples raise the world's level liquid surface at p (p.z its flat height;
// CONTENTS_* the liquid), as its mesh is drawn (interpolated between the grid's crossings; 0 where it is not, or
// flat), for things lying on it or going into it. eye: this view's (they fade out far from it).
[[nodiscard]] float surfaceRise(const glm::vec3& p, int contents, const glm::vec3& eye);

} // namespace qvr::water
