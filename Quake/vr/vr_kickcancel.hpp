// vr_kickcancel.hpp -- the recoil baked into the weapon models' firing frames taken out as they are drawn (ROUND23.md,
// "Baked kick out, procedural recoil in").
//
// Quake's view weapons kick back and tip their muzzles up in their firing frames (the whole gun moves in the model); the
// programmatic recoil (vr_weaponfx.cpp) does that now, from the hand. The models are never changed: at load, for each of
// their poses, the whole gun's rigid motion from its rest pose (frame 0) is fitted to its body (the vertices that keep
// their places relative to one another in every pose: a pump, barrels, a slide or a muzzle flash moving on the gun is
// left out by the fit), and as the gun is drawn that motion is undone (the pose's cancel, blended between the drawn
// poses as R_SetupAliasFrame blends them). Only the parts' own animation is left: the body stays at rest.
//
// The cancel is an affine map of the model's byte space (its vertices before T(scale_origin) S(scale)): the alias
// matrix ends with it (VR_AliasPostTransform), and anchor::posedAnchor/posedVertex/posedVertices put their points
// through it, so muzzles, ports and mounts on the gun stay where they are drawn. A grip hotspot (the gun's model space)
// needs none: the body it is on no longer moves.
//
// The weapons: the listed slots' models (bakedSlots: Quake's and the mission packs' guns; not the Widowmaker, whose own
// model's firing frames only turn its barrels). vr_weapon_kick_cancel 0 draws their own kick again (for comparison: Debug > Weapons).
// vr_kickcancel_info [model] prints a model's fit (each pose's motion, its body's residual); vr_kickcancel_test checks
// every listed model's (the body back at rest within tolerance on every pose, the parts still moving).

#pragma once

#include "vr_engine.hpp"
#include "vr_testkit.hpp"

#include <glm/glm.hpp>

namespace qvr::kickcancel
{

// The cancel for `e` (its model's `hdr`) drawn this frame, as an affine map of the byte space: false (and `out` left)
// when there is none (not a listed weapon's model, vr_weapon_kick_cancel 0, or at rest).
[[nodiscard]] bool byteCancel(const entity_t& e, const aliashdr_t* hdr, glm::mat4& out);

// Whether `model`'s kick is taken out (a listed weapon's model, vr_weapon_kick_cancel on).
[[nodiscard]] bool covers(const qmodel_t* model);

// VR_AliasPostTransform: `matrix` (the byte space to the world) followed by `e`'s cancel, if any.
void applyDrawn(const entity_t* e, float matrix[16]);

// The model's fit made now (a map's load, view::prepareWorldWeapons), not the first time it is drawn.
void prepare(const qmodel_t* model);

// Every listed slot's model (those found): its fit loaded, and each frame drawn through the anchors' path has every body
// vertex back at rest within tolerance (the parts may move). vr_recoil_test's part and vr_kickcancel_test's (verdict
// "vr_kickcancel_test: N passed, M failed").
void check(testkit::Tally& t);

// vr_kickcancel_info, vr_kickcancel_test.
void registerCommands();

} // namespace qvr::kickcancel
