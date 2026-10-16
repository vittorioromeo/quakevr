// vr_dents.hpp -- flesh dents (vr_gore_dents; ROUND22.md, "Flesh dents"): where a bullet, a pellet, a nail, a blade or
// a blast struck a monster or a corpse, its surface pushed in, a crater of raw flesh with a bloody rim.
//
// Each body keeps its dents (vr_gore_dents_max, the oldest merged away) in its model's rest pose (pose 0, model units):
// its centre (the struck triangle's point, carried there by its barycentrics), radius, the struck surface's rest normal,
// depth, and a blade's long axis. The alias vertex shader measures each vertex in the same rest pose (a Quake .mdl's
// pose 0; a ragdoll's skinned model's bind pose, the same) and pushes it in along its normal as drawn, so a dent follows
// every animation, the smoothing and its rebuild from the skeleton, a hit reaction and a ragdoll. The fragment shader
// measures each pixel in that pose too: the crater's raw flesh and rim at their exact size whatever the mesh's density
// (Quake's are coarse: a small dent may move no vertex), and its bowl in the light (gl_shaders.h's DentsAt, DentBump).
// The depth is held to a share of the body's thickness there (no see-through holes); dents face their surface (an arm
// lying against the chest in the rest pose is not dented with it).
//
// The rows live in one GL buffer (SSBO 12), a fixed block of rows per body, written when a body's dents change.

#pragma once

#include "Zancle/Vocabulary/Span.hpp"

#include <glm/glm.hpp>

struct entity_s;

namespace qvr::dents
{

// The blows (QC vr_wounds.qc QVR_WOUND_*: vr_wounds.cpp's kinds).
inline constexpr int kindShot = 1;  // a bullet or a pellet
inline constexpr int kindNail = 2;
inline constexpr int kindMelee = 3; // a blade (drawn out along the swing) or a blunt blow
inline constexpr int kindBlast = 4; // the crater nearest an explosion
inline constexpr int kindBlastBleed = 5; // a blast's lesser wounds round it

// A blow struck entity `e` (a Quake .mdl, or a ragdoll of one) at `at` (the world), the surface's outward normal `n`
// there, going `way`; `tris` the model's triangles as drawn now (vr_modelcollide.cpp's drawnTriangles: three corners
// each, in the world). `amount` the blow's damage.
void struck(const entity_s* e, za::Span<const glm::vec3> tris, const glm::vec3& at, const glm::vec3& n, const glm::vec3& way,
    int kind, float amount);

// The entity was removed on the server (its slot's next one starts clean).
void forget(const entity_s* e);

// A new map, a disconnect, a game directory change: every body's dents forgotten.
void clear();

// How many bodies have dents now, and how many dents in all (vr_scene_count, the info).
[[nodiscard]] int bodyCount();

// vr_gore_dents_info: the bodies with dents, their dents, the buffer's writes.
void info_f();

} // namespace qvr::dents
