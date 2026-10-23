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
// Whether `e` has kicks under way (cheap: the bone path asks it first, before posing its bones).
[[nodiscard]] bool reacting(const entity_t* e);

bool boneKicks(const entity_t* e, const ragdoll::Rig& rig, const rigpose::Rigid* posed, rigpose::Rigid* out);

// Armed grunts' arms on their weapon (vr_agrunt_ik; QC vr_armedgrunt.qc, ROUND22.md "Armed grunts: polish"): an armed
// grunt's model (progs/vrgrunt.mdl) drawn alive, its held weapon found (the weapon model nearest its right hand with the
// held effects bit, 128): its arms bent by two-bone IK (upper arm and forearm, the elbow kept where the animation had it)
// so the right hand is on the weapon's grip (its handle: its origin) and the left on its second grip (its Weapon
// Offsets' first Grip hotspot; under the right hand for a Cup), or, a one-handed or melee weapon, hanging free by its
// side. Layered on the animation as the kicks are (and the kicks on top).
[[nodiscard]] bool armsWanted(const entity_t* e);

// Whether `e` has bone turns now (kicks or arms), and them (boneKicks' form): kickInstance's (vr_animsmooth.cpp).
// `aliashdr`, `pose1`, `pose2`, `blend`: the poses drawn (the hands' vertices).
[[nodiscard]] bool bonesWanted(const entity_t* e);
bool boneTurns(const entity_t* e, const ragdoll::Rig& rig, const rigpose::Rigid* posed, const void* aliashdr, int pose1,
    int pose2, float blend, rigpose::Rigid* out);

// The whole-model tilt (models with no rig, or vr_hit_react 2): `matrix` (R_EntityMatrix's) turned about the model's
// feet. VR_AliasPreTransform's.
void tiltModel(const entity_t* e, float matrix[16]);

// vr_hit_react_trace, vr_hit_react_info (Debug > Tests).
void registerCommands();

} // namespace qvr::hitreact
