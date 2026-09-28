// vr_box3d.hpp -- the rigid bodies in Box3D: thrown weapons, boxes, backpacks, armour, gibs and heads (the
// entities whose QC sets .vr_rigid), which also collide with each other (stacks, piles). See vr_box3d.cpp and
// docs/vr-port/ROUND21.md, "Box3D physics".

#pragma once

#include "vr_engine.hpp"

namespace qvr::box3d
{

// VR_RigidToss's dispatch (vr_rigid.cpp): a rigid body (`ent`, a .vr_rigid toss or bounce entity, after its
// think) is Box3D's: true, and it moves with all the others at the end of the server frame (VR_PhysicsFrameEnd).
// False only without a world to put it in (no .vr_rigid field, no map): Quake's toss moves it.
[[nodiscard]] bool toss(edict_t* ent);

// A hand's push on the prop `ent` at `at` (world units): that point gets at least `velocity`'s speed along it (an impulse
// there: pushed high, a tall box tips; pushed low, it slides). False if it is not a Box3D prop.
bool push(edict_t* ent, const glm::vec3& at, const glm::vec3& velocity);

// A prop's mass (kg): the Mass set for its model (Held Object Offsets), else what Box3D makes it (its hull's volume
// times its density: vr_box3d.cpp). 0 without Box3D's world (no local server) or a model. Also for a prop in a hand.
[[nodiscard]] float propMass(edict_t* ent);

// Forgets the world and everything made for it (a new server: its bodies are rebuilt from the entities).
void reset();

// An explosion of `damage` at `at` (T_RadiusDamage's, through the physicsblast builtin): the props within its reach
// that it sees are thrown.
void blast(const glm::vec3& at, float damage);

// vr_debug_physics_shapes: every body's shapes as wireframes in the world (this frame's lines), coloured by what it is
// and does: props awake (green; fast, continuous: white) and asleep (blue), held (yellow), doors and plats (purple),
// monsters (orange), players (cyan), pickups hanging (grey); a prop's centre of mass, and an awake one's contact
// points (red: pressed in; pink: apart). The local server's (a listen server: nothing on a client of another).
void debugDraw();

// The profiler's counts: Box3D's bodies, those awake, the contacts the solver works on (0 without a world).
void profileCounts(int& bodies, int& awake, int& contacts);

} // namespace qvr::box3d
