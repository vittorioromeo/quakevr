// vr_box3d.hpp -- the rigid bodies in Box3D: thrown weapons, boxes, backpacks, armour, gibs and heads (the
// entities whose QC sets .vr_rigid), which also collide with each other (stacks, piles). See vr_box3d.cpp and
// docs/vr-port/ROUND21.md, "Box3D physics".

#pragma once

#include "vr_engine.hpp"

#include "Zancle/Vocabulary/FunctionRef.hpp"

namespace qvr::box3d
{

// VR_RigidToss's dispatch (vr_rigid.cpp): a rigid body (`ent`, a .vr_rigid toss or bounce entity, after its
// think) is Box3D's: true, and it moves with all the others at the end of the server frame (VR_PhysicsFrameEnd).
// False only without a world to put it in (no .vr_rigid field, no map): Quake's toss moves it.
[[nodiscard]] bool toss(edict_t* ent);

// SV_Physics's turn of the world entity, its first (VR_PhysicsEntityBegin): where vr_physics_frametime's server physics
// phase begins.
void noteServerPhysicsStart();

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

// Where the line from `start` to `end` first meets the shape of edict `num` held in a hand (a held body, following the
// hand), as a fraction of it; 1 if it doesn't, or `num` isn't held. (A crate held up stops monsters' shots: QC
// heldshape, FireBulletsImpl.)
[[nodiscard]] float heldRay(int num, const glm::vec3& start, const glm::vec3& end);

// Whether the shape of edict `num` held in a hand (its held body, as the hand holds it) meets the box `lo`..`hi` (world
// units) grown round by `reach` units: 1 it does, 0 it doesn't, -1 `num` has no held body. (A held prop presses a wall
// button it touches: QC heldbox, buttons.qc VR_Buttons_PropFrame.)
[[nodiscard]] int heldBox(int num, const glm::vec3& lo, const glm::vec3& hi, float reach);

// The point of edict `num`'s shape (its body's: its hull or a gun's convex pieces, as it lies or is held now) nearest the
// segment `a`..`b` (world units), into `out`; false if it has no body (a prop's hit with its surface, not its box: QC
// shapenearest, vr_reload.qc's magazine and super shotgun hits).
[[nodiscard]] bool shapeNearest(int num, const glm::vec3& a, const glm::vec3& b, glm::vec3& out);

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

// Forgets the world and everything made for it (a new server: its bodies are rebuilt from the entities), the toolgun's
// pins and joints too.
void reset();

// A map entity made by its spawn function (`classname`) at `origin`, turned `angles` (set before it: a monster's
// ideal_yaw is taken from them), its "model" `model` and the float field `key` set to `value` first when given (a rock's
// model, func_weapon_grabbable's weapon). Null if there is no such spawn function, or it removed itself. vr_physics_spawn's
// and the toolgun's.
edict_t* spawnClass(const char* classname, const glm::vec3& origin, const glm::vec3& angles, const char* model = nullptr,
    const char* key = nullptr, float value = 0.f);

// The toolgun's (vr_toolgun.cpp). A prop pinned is a kinematic body where its entity is (as a pickup hanging), until
// unpinned: frozen, or moved by the physgun's beam (its entity moved, its body follows and pushes the others). Edict
// numbers; forgotten when the entity goes (toolForget) or the server is new (reset).
void setPinned(int num, bool pinned);
[[nodiscard]] bool isPinned(int num);
int unpinAll(); // how many there were
// Joints between two props' bodies (loose or pinned), kept across their bodies being made again: a weld, a ball and a
// hinge (turning about `axis`) and a slider (along `axis`) at `atB`; a rope (no longer than now) and a spring (that long
// at rest) from `atA` on `a` to `atB` on `b`. World units. False if either has no body (not a prop) or a == b.
enum class ToolJoint : int
{
    Weld,
    Ball,
    Hinge,
    Slider,
    Rope,
    Spring,
    Count
};
bool addToolJoint(int a, int b, ToolJoint kind, const glm::vec3& atA, const glm::vec3& atB, const glm::vec3& axis);
int removeToolJoints(int num); // `num`'s (0: all); how many
[[nodiscard]] int toolJointCount(int num); // made now (0: all)
// Each joint made now: where it holds each body (world units), and its kind (the toolgun draws them: a rope as a rope).
void forEachToolJoint(za::FunctionRef<void(const glm::vec3& atA, const glm::vec3& atB, ToolJoint kind)> fn);
void toolForget(int num);

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

// Where a prop (edict number `num`) touched something in Box3D's last step, in world units: its contact point that pushed
// hardest. False if none pushed (no contact, or only speculative ones), it isn't a prop or without Box3D's world. (A
// thrown axe's first hit: vr_axestick.cpp.)
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

// The loose prop nearest the first player (its edict number; 0 none): the tests' "nearest".
[[nodiscard]] int nearestProp();

// Ragdolls (vr_ragdoll; vr_box3d.cpp, "Ragdolls"). Whether edict `num` is one.
[[nodiscard]] bool isRagdoll(int num);
// Knockdowns (vr_knockdown; vr_box3d.cpp "Knockdowns"; QC vr_knockdown.qc). Whether `ent` can be one (a monster with a rig,
// ragdolls on); its ragdoll made now, alive (.vr_knockdown 1, touchable and not solid); its getting up: the get-up's
// first frame fitted to how it lies (frameA, or frameB if that fits better; -1 none), room for its box mins..maxs found
// within `range`, its ragdoll gone and blended into its animation: 0 no room (it stays down), else 1 or 2 (the frame
// chosen), its origin and yaw set there.
[[nodiscard]] bool canRagdoll(edict_t* ent);
bool ragdollKnockdown(edict_t* ent);
int ragdollGetUp(edict_t* ent, int frameA, int frameB, const glm::vec3& mins, const glm::vec3& maxs, float range,
    bool anywhere = false);
// The two-hand throw's topple (vr_foegrab_throw_topple; ROUND21.md, "Holding enemies"): `ent`'s ragdoll (just made by
// ragdollKnockdown) turned over about its feet towards level `dir`, a sweep: its parts' shared launch (the throw's push and
// lift) shared out by height (the feet none, the top all), `topple` rad/s about the level axis through its feet across
// `dir` (its top's speed), `spin` rad/s about the vertical through its middle. Its feet (its lowest parts) are swept
// back against `dir` at `feet` units/s, the turn about a pivot raised to match (the top as fast as before, the whole
// turning faster: it spins in place); `feet` 0: held on the floor (level motion none) for `hold` s (swept: at most that
// long, until it has turned a quarter). The throw's (vr_foegrab_throw_topple), and a shove's over a ledge (ragdollShove).
// False: no ragdoll.
// `launch`: the share of that launch its top keeps (its parts by height: the feet none), 1 the throw's; less, it topples
// over slower from the launch. (`hold` 0.5 s: the throw's, the author's Feet Held before the sweep replaced it.)
// `whole`: every part keeps its launch, its level part times `launch`, and none is held (`feet` and `hold` unused), the
// turn about the floor under its feet added: it goes on over a ledge, turning over as it falls (a shove's over a ledge:
// ragdollShove).
bool ragdollTopple(edict_t* ent, const glm::vec3& dir, float topple, float spin, float feet, float hold = 0.5f,
    float launch = 1.f, bool whole = false);
// A shove's knockdown (vr_knockdown_shove_*; ROUND21.md, "A shove's knockdown: travel and a quarter turn"): `ent`'s
// ragdoll (just made by ragdollKnockdown) driven for the shove's length (box3d's driveShove), its travel apart from its
// turn: its middle carried `travel` units along level `dir` (the turn about its feet's own share of that included),
// slowing evenly from at most the launch's speed; turned `angle` rad about its feet towards `dir` over `time` s (a
// gravity-like curve, landing flat at a quarter), never faster than `maxSpin` rad/s, its feet `lag` of the travel behind.
// `ledge`: shoved over one: pushed whole (its level launch times `keep`, at least `minSpeed` u/s) and turned `topple`
// rad/s about the floor under its feet (ragdollTopple's whole), kept going so until it has gone `reach` units (past the
// edge) or dropped, its turn kept at `topple` until it leans `angle`, never faster than `maxSpin`, until it lands.
// False: no ragdoll.
struct RagdollShove
{
    float travel{0.f};
    float angle{1.5707963f};
    float time{0.6f};
    float lag{0.3f};
    float maxSpin{5.2f};
    bool ledge{false};
    float keep{1.f};
    float minSpeed{80.f};
    float reach{0.f};
    float topple{0.f};
};
bool ragdollShove(edict_t* ent, const glm::vec3& dir, const RagdollShove& p);
// Tests (the throw's trace): `num`'s ragdoll's pelvis, head (its rig's head, else its highest part) and feet (the middle of
// the parts ragdollTopple held, else of those in its lowest quarter at the first call), units. False: no ragdoll.
bool ragdollStance(int num, glm::vec3& pelvis, glm::vec3& head, glm::vec3& feet);
// Whether hand point `at` (units) is within vr_ragdoll_grab_reach of a limb of edict `num`'s ragdoll (a hand touching it).
[[nodiscard]] bool ragdollReach(int num, const glm::vec3& at);
// The hands and the limbs (QC's builtins: ragdollgrab, ragdollrelease, ragdollheld, ragdollreach). `hand`: QC's (0 the
// off hand, 1 the main). grab: take the limb of `corpse` the hand is on; release: let go (the limb keeps `velocity`,
// units/s).
bool ragdollGrab(edict_t* corpse, edict_t* player, int hand);
void ragdollRelease(edict_t* player, int hand, const glm::vec3& velocity);
// 0 nothing, 1 holding a limb; the distance (units) from the hand to the limb's held point (-1: none).
[[nodiscard]] int ragdollHeld(edict_t* player, int hand);
[[nodiscard]] float ragdollHandReach(edict_t* player, int hand);
// A hand's hold on a limb (not a pull), for the hand drawn on it and its fingers closed round it (vr_view.cpp; a listen
// server's: the client reads it): edict `num`'s part `part`, the hand's place and turn in that part's frame (`at`
// units, `turn`: the world's are rot * at + pos and rot * turn, rot and pos the part's as ragdoll::publish gives them,
// the turn as held::axesFromAngles(handrot, true)'s). Edict `player`'s hand `hand` (QC's: 0 the off hand). False: none.
struct RagdollHold
{
    int num{0};
    int part{-1};
    glm::vec3 at{0.f};
    glm::quat turn{1.f, 0.f, 0.f, 0.f};
};
[[nodiscard]] bool ragdollHold(int player, int hand, RagdollHold& out);
// A limb's point (flames on a ragdoll: vr_burning.qc): the limb nearest `at` (-1: not a ragdoll); a point into a limb's
// space (units) and back.
[[nodiscard]] int ragdollBone(int num, const glm::vec3& at);
// Tests (vr_mock_hand_to ... ragdoll): the middle of part `part` (-1: the one nearest `from`; -2 its head) of the ragdoll nearest `from`
// (units); its edict number,
// 0 if none (or no such part).
int ragdollPartCentre(const glm::vec3& from, int part, glm::vec3& out);
[[nodiscard]] glm::vec3 ragdollPoint(int num, int bone, const glm::vec3& p, bool toWorld);

// Decapitation (ROUND21.md, "Decapitation"; QC vr_decap.qc). `ent`'s ragdoll loses its head (its rig's head bone and the
// bones on it: their bodies and joints go, the mesh's head is drawn at the neck); a dead monster with a rig and no
// ragdoll yet (a live one just killed, a dying or lying corpse) gets one now from the frame it is in, whatever Most
// Ragdolls. `blade`: the blade's velocity at the cut (units/s): the head is launched with vr_decap_head_speed of it and
// vr_decap_head_lift up, spun as the swing turns it. False (nothing done) without a rig with a head, with ragdolls off,
// or already headless. `settle`: its parts' motion (linear and turning) times this at its next step, after this frame's
// knocks and pushes have reached them (a head pop's: vr_decap_pop_body_speed; 1 as it is).
bool ragdollDecap(edict_t* ent, const glm::vec3& blade, float settle = 1.f);
// Limb gore (ROUND21.md, "Limb gore"; QC vr_limbs.qc): as ragdollDecap, the limb at joint `bone` (ragdoll::limbJoint;
// -1 the head) and the bones on it not cut yet.
bool ragdollCutLimb(edict_t* ent, int bone, const glm::vec3& blade, float settle = 1.f);
// After a cut: 0 the piece's middle then (a limb's: its model's origin), 1 its angles (an alias model's), 2 its launch
// velocity (units/s), 3 its spin (rad/s, world: a prop's .vr_spin); the stump of `bone` (-1 the head) now: 4 its joint, 5
// the way out of it (unit); 6 the middle of `bone`'s piece now, before a cut (the tests); 7 (the cut bones, the last cut's
// joint, the parts left); 8 (the bones the last cut took). Zero if none.
[[nodiscard]] glm::vec3 ragdollCut(int num, int what, int bone = -1);
// The joint a hit at `at` on `ent` would cut: the bone of its vertex nearest (as drawn: a ragdoll's bodies, else its frame
// where it stands), then the nearest of that bone's own joint and its children's; its rig's head for the head's bones;
// -1 the torso (or nothing); -2 no rig.
[[nodiscard]] int limbAt(edict_t* ent, const glm::vec3& at);
// `ent`'s limbs: 0 its limb joints not cut (bits; not the head's), 1 its head's bones, 2 its bones cut off, 3 its head
// bone (~0 none), 4 the bones a cut at `bone` would take now, 5 `bone`'s parent.
[[nodiscard]] uint32_t limbInfo(edict_t* ent, int what, int bone);
// The model of the limb at joint `bone` of `ent` (vr_limbmodel.cpp: just cut, the piece cut; else the limb as it is),
// "" if none.
[[nodiscard]] const char* limbModel(edict_t* ent, int bone);
// The piece a cut at `bone` of `ent` takes now (-1 or its head bone: the head; just cut, the piece cut; a body gibbed
// whole, the limb as it is): 0 {kg it weighs, the ragdoll's mass, its share of it}: by the kind of limb (limbMassShare:
// a hand a little, a thigh a lot); 1 its cut end (the joint) in its model's space (rest units,
// unscaled: about the limb model's origin; the head's, its bone's middle). Zero: no rig or no such limb.
[[nodiscard]] glm::vec3 limbPiece(edict_t* ent, int bone, int what);
// Whether `model` is the head a ragdoll class's monster throws (h_guard.mdl: the grunt's; vr_box3d.cpp ragdollClasses).
[[nodiscard]] bool monsterHead(const char* model);
// What the head `model` weighs as its monster's (its prop's Mass -1, Held Object Offsets): as that head cut off the
// class's ragdoll, the head's 7% of vr_ragdoll_<class>_mass times vr_limbs_mass_scale (limbPiece); 0: not a monster's
// head, or vr_limbs_mass_scale 0 (estimated).
[[nodiscard]] float headPropMass(const char* model);
// Where the limb at `bone` of `ent` is now (not cut: a body gibbed whole): 0 its model's origin, 1 its angles, 2 its
// velocity, 3 a point on its own bone's surface.
[[nodiscard]] glm::vec3 limbPlace(edict_t* ent, int bone, int what);
// 1 if `at` (units) is on the head of edict `num`'s ragdoll (its part nearest, or within a few units of the neck: `neck`
// units if more, the melee's vr_decap_neck), 0 not (or no head), -1 not a ragdoll.
[[nodiscard]] int ragdollHeadAt(int num, const glm::vec3& at, float neck = 0.f);

} // namespace qvr::box3d
