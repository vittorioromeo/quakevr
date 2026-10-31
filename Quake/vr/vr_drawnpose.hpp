#pragma once
// vr_drawnpose.hpp -- where an entity's body is drawn now: the one query the game asks for a bone of a body drawn on a
// rig (ROUND23.md, "Drawn pose: worn backpacks and disarm volumes"). The game used to read the animation frame (the .mdl's
// vertex pose, hitmodel's) or the entity's origin and angles, while the body is drawn otherwise: a live grunt on the
// authored rig with the Rig Studio's takes (bending to take a weapon), its arms' IK and its hit reactions; a ragdoll (an
// active one too) by its parts. Its users: the worn backpack (agarmor::packWhere), the held weapons of an armed humanoid
// (hitreact::drawnWeapon: the disarm volumes and a ragdoll's weapon, QC VR_AG_RagProp), an armed humanoid's body as drawn
// (armedMesh: the precise hits, vr_hitmodel.cpp).

#include "vr_rigpose.hpp"
#include "Zancle/Container/Vector.hpp"

struct edict_s;

namespace qvr::drawnpose
{

// Where the bone came from.
enum class Source : int
{
    None = 0,    // not drawn lately (a dedicated server, out of view, no rig): the caller's own (the frame's)
    Ragdoll = 1, // its ragdoll's part as drawn (ragdoll::drawnPart: between the physics steps, moved with the hands)
    Live = 2,    // the authored rig's bones as drawn lately (rig2::drawnBone), placed as the client draws the entity
};

// Entity `ent`'s rig bone `bone` as drawn now: a rest point p (its model's rest space, units) is drawn in the world at
// out.pos + out.rot * (scale * p). The bone's index is its rig's (the authored rig's are its ragdoll's parts too).
// Main thread; a listen server's own view (the client's drawing of the last frame or this one).
[[nodiscard]] Source bone(edict_s* ent, int bone, rigpose::Rigid& out, float& scale);

// Armed humanoid `ent` (hitreact::armsWanted: its arms posed on its weapons) alive on the authored rig as drawn lately: its
// .mdl's vertices (the .mdl's order) skinned with the bones it is drawn with (rig2::liveVertices), placed as bone() places
// them (world), and their normals turned likewise (`normals`: world, unit length). Not a ragdoll's (ragdoll::
// skinnedVertices). False: not drawn so lately (the caller's own: the frame's). The precise hits on its body (hitmodel:
// shots, blows and holds meet the body as drawn: aiming, guarding, swinging, staggered, not its animation frame).
[[nodiscard]] bool armedMesh(edict_s* ent, za::Vector<glm::vec3>& out, za::Vector<glm::vec3>* normals);

[[nodiscard]] const char* sourceName(Source s);

} // namespace qvr::drawnpose
