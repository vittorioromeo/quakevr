// vr_box3d.hpp -- the rigid bodies in Box3D (vr_physics_engine 1): thrown weapons, boxes, backpacks, armour,
// gibs and heads (the entities whose QC sets .vr_rigid), which then also collide with each other (stacks, piles).
// vr_physics_engine 0 is Quake VR's own solver (vr_rigid.cpp), unchanged. See vr_box3d.cpp and
// docs/vr-port/ROUND21.md, "Box3D physics".

#pragma once

#include "vr_engine.hpp"

namespace qvr::box3d
{

// VR_RigidToss's dispatch (vr_rigid.cpp): with vr_physics_engine 1, a rigid body (`ent`, a .vr_rigid toss or
// bounce entity, after its think) is Box3D's: true, and it moves with all the others at the end of the server
// frame (VR_PhysicsFrameEnd). False with vr_physics_engine 0: vr_rigid.cpp moves it.
[[nodiscard]] bool toss(edict_t* ent);

// Forgets the world and everything made for it (a new server: its bodies are rebuilt from the entities).
void reset();

} // namespace qvr::box3d
