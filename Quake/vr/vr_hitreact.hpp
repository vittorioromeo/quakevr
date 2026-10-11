// vr_hitreact.hpp -- living hit reactions (ROUND22.md, "Active ragdolls: living hit reactions"; vr_hit_react): a living
// monster struck flinches where it was struck, drawn by every client. Not a ragdoll: an additive kick layered on its
// animation, springing back in a few tenths of a second; its AI, its hitboxes and the server's frames are untouched.
//
// The server sends each blow (QVR_SVC_HITREACT: QC/vr_hitreact.qc, hitreact): the point struck, the push's direction,
// the part (PositionalDamage's: the head, an arm, a leg, the body), the damage and its share of the monster's health.
// The client draws it:
// - A monster with a ragdoll rig (vr_ragdoll.cpp's: grunts, knights, ogres, ...): the bone struck and its parents turned
//   round their joints by the push's torque (a head snaps back, an arm swings, the torso twists), a leg hit's knee
//   buckling and the torso leaning over it; each bone's turn carried by its children. Drawn through r_lerpmodels 3's
//   bone path (vr_animsmooth.cpp: a 3x4 delta per bone the alias shader adds, RigKick), whatever Model Animation is.
// - Any other model (or vr_hit_react 2): the whole model tilted about its feet by the push (its entity matrix).
// Each kick's turn follows a damped spring's answer to a blow (peak in about 70 ms, a small swing back, gone by about
// half a second: vr_hit_react_time), its size by the share of health the blow took (vr_hit_react_strength); kicks add up.

#pragma once

#include "vr_ragdoll.hpp"
#include "vr_rigpose.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace qvr::hitreact
{

// QVR_SVC_HITREACT (vr_client.cpp's dispatch).
void parseEvent();

// A rigged entity's bones kicked now: for each bone, the rigid motion (posed model space, units) its posed vertices
// take on top of the animation: p' = rot[b] * p + pos[b]. `posed`: each bone's transform from the rest pose to the pose
// drawn. False if `e` reacts to nothing now (outputs untouched).
bool boneKicks(const entity_t* e, const ragdoll::Rig& rig, const rigpose::Rigid* posed, rigpose::Rigid* out);

// The whole-model tilt (models with no rig, or vr_hit_react 2): `matrix` (R_EntityMatrix's) turned about the model's
// feet. VR_AliasPreTransform's.
void tiltModel(const entity_t* e, float matrix[16]);

// vr_hit_react_trace, vr_hit_react_info (Debug > Tests).
void registerCommands();

} // namespace qvr::hitreact
