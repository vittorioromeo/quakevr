// vr_kickcancel.hpp -- the recoil baked into the weapon models' firing frames taken out (ROUND23.md, "Baked kick out,
// procedural recoil in"; "Kick cancel in the poses").
//
// Quake's view weapons kick back and tip their muzzles up in their firing frames (the whole gun moves in the model); the
// programmatic recoil (vr_weaponfx.cpp) does that now, from the hand. The model files are never changed: for each of
// their poses, the whole gun's rigid motion from its rest pose (frame 0) is fitted to its body (the vertices that keep
// their places relative to one another in every pose: a pump, barrels, a slide or a muzzle flash moving on the gun is
// left out by the fit), and the poses in memory are made again without it (ensure): the body's vertices are the rest
// pose's own bytes in every pose (no rounding left to shimmer: drawn exactly still), a part's are its pose's put through
// the pose's cancel. The grenade launchers are drawn whole at rest (their upper shells only lurch). The model is
// uploaded again, and everything that reads its poses (the renderer, the anchors, the dequantiser) reads the cancelled
// ones: nothing is undone as it is drawn.
//
// The weapons: the listed slots' models (bakedSlots: Quake's and the mission packs' guns; not the Widowmaker, whose own
// model's firing frames only turn its barrels). vr_weapon_kick_cancel 0 puts their own poses back (for comparison:
// Debug > Weapons; the held guns' at the next frame). vr_kickcancel_info [model] prints a model's fit (each pose's
// motion, its body's residual); vr_kickcancel_test checks every listed model's (its firing frames drawn at 72 and 90 Hz:
// the body still).

#pragma once

#include "vr_engine.hpp"
#include "vr_testkit.hpp"

#include <glm/glm.hpp>

namespace qvr::kickcancel
{

// `e`'s body (its fit's) as drawn this frame through the anchors' path (its poses and their blend), in its model space
// (scale and scale_origin applied: view::modelPoint's): the centre of the body as drawn and as the rest pose draws it,
// and the farthest a body vertex (any vertex, for a model drawn whole at rest) is from its rest place (model units).
// False: not a listed weapon's model.
[[nodiscard]] bool drawnBody(const entity_t& e, glm::vec3& now, glm::vec3& rest, float& worst);

// The model's poses made ready now (fitted the first time, cancelled or put back by vr_weapon_kick_cancel): a map's load
// (view::prepareWorldWeapons), and each frame for the held weapons (weaponfx::frame: a toggle, a model read again).
void prepare(const qmodel_t* model);

// Every listed slot's model (those found): its fit loaded, and its firing frames drawn through the anchors' path at 72
// and 90 Hz keep its body at rest, still from one drawn frame to the next (the parts may move). vr_recoil_test's part
// and vr_kickcancel_test's (verdict "vr_kickcancel_test: N passed, M failed").
void check(testkit::Tally& t);

// vr_kickcancel_info, vr_kickcancel_test.
void registerCommands();

} // namespace qvr::kickcancel
