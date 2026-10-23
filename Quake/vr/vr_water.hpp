// vr_water.hpp -- liquids: water, slime, lava and teleports less flat, and the view with your head in them.
//
// The surfaces (vr_glsl.h, LIQUID_FUNCTIONS; both of Ironwail's liquid programs, lit and unlit water): waves as a
// sum of sines in the world (in the normal, the texture's warp and the refraction), and with vr_water_geo_waves long
// swells that move the geometry too (the world's level liquid faces cut into a grid as a map loads, their vertices
// raised in the vertex shaders, held still at the walls: vr_water.cpp's mesh, drawn by r_world.c), a fresnel term (see-through looking down, the room
// reflected at grazing angles: vr_envmap.cpp's water cube, vr_water_reflections, or a dim colour), glints from a light above and from dynamic lights, what is under translucent water bent
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

#include "Zancle/Container/Vector.hpp"

#include <glm/glm.hpp>


namespace qvr::water
{

// How far the opaque scene is from the eye in this view, along the view (an R32F texture of half the scene's size,
// each texel the nearest of its four; 0: none, as without a depth texture to read): the liquids' refraction's, made
// now if they did not make it this view. After the translucent pass only (the soft particles, vr_particles.cpp): it
// binds the scene's framebuffer again when it makes it.
[[nodiscard]] unsigned opaqueSceneDistances();
// The size of the distances opaqueSceneDistances last returned (half the scene's target, rounded up).
void opaqueSceneDistancesSize(int& width, int& height);

// The scene's depth was drawn into after its distances were made this view (the soft sprites, vr_particles.cpp):
// opaqueSceneDistances makes them again.
void sceneDepthChanged();

// The top of a level lava face of the world, for the heat haze over it (vr_haze.cpp): its polygon and the parts of its
// edges on the pool's rim (where the lava meets rock), at height z.
struct LavaTop
{
    int face = 0; // the geometric waves' mesh's
    float z = 0.f;
    za::Vector<glm::vec3> poly;
    za::Vector<glm::vec2> rim; // pairs of ends
};

// The world's lava tops (made with the geometric waves' mesh, which is made when the waves, the foam or the haze are
// on) and a number that changes when they are made anew.
[[nodiscard]] const za::Vector<LavaTop>& lavaTops(unsigned& generation);

// Whether a lava top is in this view's PVS (VR_WaterMarkVis, this view).
[[nodiscard]] bool lavaTopInPvs(const LavaTop& top);

// The shoreline foam's settings for one liquid (0 water: vr_water_foam_*, 1 lava: vr_lava_foam_*, 2 slime:
// vr_slime_foam_*), clamped to their ranges; `on` false when it is off (or vr_water_foam is 0). The shaders get them in
// the frame data (Foam, FoamColor, FoamBreath), the shore's particles (vr_particles.cpp) from here.
struct FoamLook
{
    bool on = false;
    float opacity = 0.f, frequency = 1.f, strength = 1.f;
    float breathe = 0.f, breatheSpeed = 1.f, breatheIrregularity = 0.f;
    glm::vec3 color{1.f};
    float particles = 0.f, particleLife = 1.f, particleOpacity = 0.f, particleSize = 1.f;
    float particleShore = 0.f; // where the particles are born: units out from the shore's line
    float edge = 0.f;          // the wavy edge (LiquidFoam): how far the liquid's edge is drawn back, units (0 off)
};
[[nodiscard]] FoamLook foamLook(int liquid);
[[nodiscard]] int foamLiquid(int contents); // CONTENTS_LAVA 1, CONTENTS_SLIME 2, else (water) 0
[[nodiscard]] int foamContents(int liquid); // and back

// The world's shore (made with the geometric waves' mesh): the parts of the level liquid tops' edges where the liquid
// meets rock or another liquid (the foam's rim), a, b their ends at height z, `in` the way into the liquid (a unit
// vector), `face` the mesh's face (shoreSeen), `liquid` foamLiquid's. A number that changes when they are made anew.
struct ShoreSegment
{
    glm::vec2 a{0.f}, b{0.f}, in{0.f};
    float z = 0.f;
    int face = 0;
    int liquid = 0;
};
[[nodiscard]] const za::Vector<ShoreSegment>& shoreSegments(unsigned& generation);
// Whether a shore segment's face was in a view's PVS lately (the last few views: VR_WaterMarkVis).
[[nodiscard]] bool shoreSeen(const ShoreSegment& s);

// Ripples (vr_water_ripples): a splash `strength` hard (Preset::Splash's count: 4 a shot, 6-15 a hand or a thrown
// thing, 20-50 a body) at `at` on a liquid's surface, rings of waves spreading from it in the liquid's shading and,
// on the geometric waves' mesh, in its geometry. The client's; each frame's are in the frame data (gl_shaders.h,
// LiquidRipples).
void addRipple(const glm::vec3& at, float strength);

// Blood in the liquids (vr_water_blood; vr_glsl.h, LiquidBlood): blood let at `at`, `amount` of it (1 a hard hit's,
// 1.5 at most), leaves a cloud spreading and fading on the surface of the water or slime it is in (up to it, 192 units
// at most) or falls onto (`above` units at most below `at`; 0: none). False: no liquid there (nothing done); true:
// in or over one (a cloud added, or none: lava, too deep, a teleport). A few point tests (CL_PointContents' leaf walk).
// surfaceZ: the surface's height, when one was found.
bool addBlood(const glm::vec3& at, float amount, float above = 24.f, float* surfaceZ = nullptr);
// ... a cloud with its surface already known (`surface` on it: the wounds' waterline).
void addBloodAt(const glm::vec3& surface, float amount);
// Whether blood let at `at` is in a liquid (one point test), with vr_water_blood on: it slows to a cloud there (the
// particles' and the streams' blood).
[[nodiscard]] bool bloodInLiquid(const glm::vec3& at);
// vr_water_blood_test [distance] [amount]: a cloud on the liquid below a point that far ahead (96); vr_water_blood_info:
// the clouds now.
void bloodTest_f();
void bloodInfo_f();

// How fast the ripples spread on a liquid (CONTENTS_*: lava's slow, slime's slower than water's), in units a second:
// the splash's rings ride their crest.
[[nodiscard]] float rippleSpeed(int contents);

// How far this view's geometric waves and ripples raise the world's level liquid surface at p (p.z its flat height;
// CONTENTS_* the liquid), as its mesh is drawn (interpolated between the grid's crossings; 0 where it is not, or
// flat), for things lying on it or going into it. eye: this view's (they fade out far from it).
[[nodiscard]] float surfaceRise(const glm::vec3& p, int contents, const glm::vec3& eye);

// The last view's underwater wobble and blur, as the eyes' post-processing does them (VR_PostProcessWater), for the
// desktop window's view of it (vr_stereo.cpp): the time, the wobble, the blur (both 0: not under water).
[[nodiscard]] glm::vec3 viewWobble();

// A map's load (VR_NewMap, after its lightmaps and vertices): the liquids' volume (caustics, wetness) and wave mesh
// built for it, 20-100 ms that the map's first frame took.
void prepare();

} // namespace qvr::water
