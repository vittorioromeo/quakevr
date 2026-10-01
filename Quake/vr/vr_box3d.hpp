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
// there: pushed high, a tall box tips; pushed low, it slides). `pusherMass` (kg, 0: none): what pushes has that mass, so
// the point gets only the share of two masses meeting (a light hand barely moves a heavy box). False if it is not a
// Box3D prop.
bool push(edict_t* ent, const glm::vec3& at, const glm::vec3& velocity, float pusherMass = 0.f);

// A shot from `start` to `end` (world units): the first loose prop it passes through pushed (push) where it goes in, with
// `velocity` and `pusherMass` (a pellet, a nail: Quake's traces pass through the pickups lying about, SOLID_TRIGGER).
// That prop's edict number, 0 if none.
int shot(const glm::vec3& start, const glm::vec3& end, const glm::vec3& velocity, float pusherMass);

// Whether a monster's sight from `start` to `end` is blocked by a solid prop that blocks sight (.vr_blocksight: a crate,
// an explosive box), met by its Box3D shape as it lies (not one in a hand: a held body). Not the edicts `ignoreA` and
// `ignoreB` (who looks and at whom). The blocker's edict number, 0 if none.
[[nodiscard]] int sightRay(const glm::vec3& start, const glm::vec3& end, int ignoreA, int ignoreB);

// Whether edict `num` is one of Box3D's props (its shape is what sightRay meets).
[[nodiscard]] bool isBox3DProp(int num);

// The prop `ent`'s motion slowed where it is now (its body's, so that a push after it this frame adds to it; QC's
// .velocity written would override both at the next step): its velocity relative to `relativeTo` kept by `keep` (0 ..
// 1) and no faster than `maxSpeed` (0: any), then `add` added; its spin kept by `keepSpin`. False if it is not a Box3D
// prop. (The grappling hook's load: vr_grapple.qc VR_Grapple_LoadMotion.)
bool damp(edict_t* ent, const glm::vec3& relativeTo, float keep, float keepSpin, float maxSpeed, const glm::vec3& add);

// A prop's mass (kg): the Mass set for its model (Held Object Offsets), else what Box3D makes it (its hull's volume
// times its density: vr_box3d.cpp). 0 without Box3D's world (no local server) or a model. Also for a prop in a hand.
[[nodiscard]] float propMass(edict_t* ent);

// The map's mesh made on the game's thread pool while the server spawns the map (VR_OnSpawnServerBeforeLoad); the
// world's first look at it waits for it, and so does finishLoads (the map's memory about to go, shutdown).
void beforeLoad();
void finishLoads();

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

// The grappling hook's rope (vr_ropesim.cpp) against what it can't pass through: the world's mesh, the doors and lifts,
// the props (not the entities `skipA` and `skipB`, 0: none; the rope's ends). A sphere of `radius` cast from `from` to
// `to`: its first hit (the fraction of the way, the sphere's centre there and the surface's normal); false if clear, and
// without Box3D's world.
struct RopeHit
{
    float fraction{1.f};
    glm::vec3 centre{0.f};
    glm::vec3 normal{0.f};
};
bool ropeCast(const glm::vec3& from, const glm::vec3& to, float radius, int skipA, int skipB, RopeHit& hit);

// Whether a sphere there overlaps any of them.
[[nodiscard]] bool ropeOverlaps(const glm::vec3& at, float radius, int skipA, int skipB);

// A sphere of `radius` cast from `from` to `to` against the body of the entity `num` alone (its drawn shape, as it is
// turned: a weapon lying about, which its box, round its handle, doesn't hold; a tilted box): the fraction of the way
// where it first meets it; false if it misses (`hasBody` false: the entity has no prop's or fixture's body).
bool castAt(int num, const glm::vec3& from, const glm::vec3& to, float radius, float& fraction, bool& hasBody);

// A ray from `from` to `to` against the loose props (dynamic bodies: not the world, doors, monsters, players, hands, what a
// hand holds), but the entity `skip`: the first it meets (its edict number, the fraction of the way, the point and the
// surface's normal). False if none, and without Box3D's world. (A thrown axe's blade: vr_axestick.cpp.)
struct PropHit
{
    int num{0};
    float fraction{1.f};
    glm::vec3 point{0.f};
    glm::vec3 normal{0.f};
};
bool castProps(const glm::vec3& from, const glm::vec3& to, int skip, PropHit& hit);

// Where a prop (edict number `num`) touches something now, in world units: its touching point that pushed hardest.
// False if none, it isn't a prop or without Box3D's world. (A thrown axe's first hit, for its debug: vr_axestick.cpp.)
bool contactPoint(int num, glm::vec3& point);

// Whether the loose prop `num` rests on hand `hand` ([0] off, [1] main) of client `player`: touches its reach body (the
// open hand, the fist, the held weapon) or its sphere where the contact holds it up (its normal points up into it). For
// the drawn hands (vr_modelcollide.cpp): a thing lying on the palm doesn't push the hand away.
[[nodiscard]] bool restsOnHand(int num, int player, int hand);
// A held prop `num` (its drawn box) moved from `fromPos` turned `fromRot` to `toPos` turned `toRot` (its origin, as its
// entity's) kept out of the level (the world and its brush entities; vr_carry2h.cpp): `toPos` as far along the move as its
// box stays out of it (overlap tests), then slid along what it met axis by axis; `toRot` the old turn if the new one would put it in. Already a
// little in the level where it starts (taken from the floor), no deeper. True if it was stopped (either changed);
// false if not, and without a body or Box3D's world.
bool holdClear(int num, const glm::vec3& fromPos, const glm::quat& fromRot, glm::vec3& toPos, glm::quat& toRot);

} // namespace qvr::box3d
