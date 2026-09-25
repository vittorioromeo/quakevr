// vr_physics.hpp -- server-side Quake VR physics shared with other VR sources.

#pragma once

#include "vr_engine.hpp"

namespace qvr::physics
{

// QC's FL_FORCEGRABBABLE (QC/defs.qc): boxes, gibs, thrown weapons.
inline constexpr int FL_FORCEGRABBABLE = 1 << 15;

// Rigid bodies and held objects (vr_rigid.cpp).

// Whether `p` is inside `ent`'s drawn box (turned with it), grown by `margin`: what a hand must be
// in to take hold of an object (Quake's boxes do not turn, and items' are widened).
[[nodiscard]] bool pointInModelBox(edict_t* ent, const glm::vec3& p, float margin);

// QC's carryangles: a held object's angles (into `out`), turning with the hand at `handAngles`.
// At the grip (`grab`) its turn relative to the hand is kept, and its angles are unchanged.
void carryAngles(edict_t* ent, const float* handAngles, bool grab, float* out);

// Forgets rigid bodies' and held objects' state (a new server).
void resetRigidBodies();

} // namespace qvr::physics
