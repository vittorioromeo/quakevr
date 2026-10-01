// vr_view.hpp -- client-side VR view entities: weapons in both hands, hands and fingers,
// holstered weapons, holster slots, torso and weapon buttons. Built every frame from the
// tracked hands and the VR stats, drawn as ordinary alias entities.

#pragma once

#include "vr_engine.hpp"
#include "vr_hands.hpp"
#include "vr_weapons.hpp"

namespace qvr::view
{

// QVR_SVC_HANDIMPACT: the drawn hand is knocked and wobbles back (a parried blow).
void parseHandImpact();

struct ViewEntity
{
    entity_t ent{};
    bool visible{false};
    bool mirrored{false};  // left-hand versions of right-hand models
    float zeroBlend{0.f};  // blend of the animation towards frame 0 (weapons)
    float morph{0.f};      // a gun morphing into its other ammo's model: + coming in, - going out (VR_AliasMorph)
    bool lightMultiply{false};
    glm::vec3 lightMod{1.f};
    glm::vec3 lightShift{0.f}; // the light sampled this far from the origin (a climbing hand: vr_climb.cpp drawnHand)
    glm::vec3 scale{1.f};  // the model's scale per axis, exact (entity_t's is a byte, in sixteenths; vr_render.cpp)
    const qmodel_t* lastModel{nullptr};
};

// Null if `e` is not a VR view entity.
[[nodiscard]] const ViewEntity* find(const entity_t* e);

// World position of an anchor vertex of `ve`'s model, with `extra` offsets applied in the
// entity's (mirrored) space before the model's own scaling.
[[nodiscard]] glm::vec3 anchorPosition(const ViewEntity& ve, int anchorIndex, const glm::vec3& extra);

// The same for any alias entity (a weapon lying in the world), drawn mirrored or not.
[[nodiscard]] glm::vec3 entityAnchorPosition(
    const entity_t& e, bool mirrored, float zeroBlend, int anchorIndex, const glm::vec3& extra);

// World position of a point given in `ve`'s model space (as its frames' vertices).
[[nodiscard]] glm::vec3 modelPoint(const ViewEntity& ve, const glm::vec3& point);

// QVR flashlight on guns (round 20): the gun drawn in `hand` this frame, for what clips onto it: its
// model, the pose it is drawn from (the hand's; for a gun carried by its foregrip, the hand's that let
// it go) and its muzzle. False when the hand holds no gun (empty, or nothing drawn).
struct WeaponMount
{
    const qmodel_t* model{nullptr};
    glm::vec3 pos{0.f};
    glm::vec3 rot{0.f};
    glm::vec3 muzzle{0.f};
    bool mirrored{false};
};
[[nodiscard]] bool weaponMount(int hand, WeaponMount& out);

// Body Calibration (vr_bodycal.cpp): the empty hand as drawn on the calibrated controller (Hand Calibration's place,
// with the fist's own offsets), whatever the hand holds: its wrist (world) and axes (towards the fingers, the thumb's
// side, the back of the hand). Without the jointed hand, the hand as drawn (jointed false).
struct EmptyHand
{
    glm::vec3 wrist{0.f};
    glm::vec3 forward{1.f, 0.f, 0.f};
    glm::vec3 up{0.f, 0.f, 1.f};
    glm::vec3 back{0.f, 1.f, 0.f};
    bool jointed{false};
};
[[nodiscard]] bool emptyHandPose(const hands::State& s, int hand, EmptyHand& out);

// Align Sights to My Aim (vr_sightalign.cpp): the weapon drawn in `hand` this frame: its model, the world transform of
// its model space (as its frames' vertices: view::modelPoint), and the middle of the fist round its grip (the drawn
// hand's grip channel; its palm without the jointed hand). False when the hand holds no weapon.
struct WeaponFrame
{
    const qmodel_t* model{nullptr};
    glm::mat4 modelToWorld{1.f};
    glm::vec3 fist{0.f};
    bool fistFromRig{false};
};
[[nodiscard]] bool weaponFrame(const hands::State& s, int hand, WeaponFrame& out);

// The counter glow (vr_meleehud.cpp): the weapon drawn in `hand` this frame (null: none, or the hand's own model), and
// which hand an entity belongs to (-1 none): its weapon (or the model a gun morphs out of; `weapon` true) or a part of
// the hand itself.
[[nodiscard]] const ViewEntity* heldWeapon(int hand);
[[nodiscard]] int handOf(const entity_t* e, bool& weapon);
// Whether models `a` and `b` are the same gun (one is the other's other ammo's: its button switched it).
[[nodiscard]] bool sameGun(const qmodel_t* a, const qmodel_t* b);

// How curled a drawn finger is (0 open .. 1 curled): `finger` 0 thumb, 1 index, 2 middle, 3 ring, 4 pinky.
[[nodiscard]] float fingerCurl(int hand, int finger);

// The weapons' hotspots (round 21): the hotspot point (the weapon's model space, weapons::Hotspot) of a world point
// `p` on the weapon in `hand`; false when the hand holds none.
[[nodiscard]] bool hotspotAt(int hand, const glm::vec3& p, glm::vec3& out);

// Hotspot `index` of the weapon in `hand` as drawn this frame: its type (weapons::HotspotType, 0 none), where (a grip's
// point, a blade grip's middle), its bias, and a blade's share of the way from the hand to the tip.
struct WeaponHotspot
{
    int type{0};
    glm::vec3 pos{0.f};
    float bias{0.f};
    float share{0.f};
};
[[nodiscard]] WeaponHotspot weaponHotspot(int hand, int index);

// The weapon in `hand` as drawn last (the local player's): its model, whether mirrored (the off hand's), and its
// entity's place and turn relative to the hand's pose (hands::State pos and rot: held::axesFromAngles' forward, left,
// up) -- rigid; the model's vertices go in it as the view draws them (mirrored, then weapons::ModelTransform). For the
// held weapons' bodies in Box3D (vr_box3d.cpp: they push props). `when`: realtime it was drawn; model nullptr: none.
struct DrawnWeapon
{
    const qmodel_t* model{nullptr};
    bool mirrored{false};
    glm::mat4 inHand{1.f};
    double when{-1.0};
};
[[nodiscard]] const DrawnWeapon& drawnWeapon(int hand);

// The weapon in `hand` drawn pressed back by the other, free hand pushed into it (vr_hand_collide: each drawn moved back
// by half how deep they meet, as a prop in that hand and the weapon do), last frame's: the model collision moves the
// weapon hand by it (vr_modelcollide.cpp, with held::drawnPush).
[[nodiscard]] glm::vec3 handPress(int hand);

// vr_hotspots_legacy [print]: the slots' hotspots worked out from their round-20 two-handed grip keys (their defaults),
// printed as vr_weapons.inc lines (round 21's migration of the shipped defaults).
void hotspotsLegacy_f();

// vr_hotspots_check: the migrated hotspots against the old two-handed grips, every slot, either hand.
void hotspotsCheck_f();

// vr_weapon_hotspot_here <1..4> [<type>] [main|off]: hotspot n of the weapon in the main hand (or the named one) put
// where the other hand is now, as a grip (1) or the type given (3: a cup) -- the Weapon Offsets page's "Put It Where the
// Other Hand Is", for scripts; "vr_weapon_hotspot_here <n> 0" removes it.
void hotspotHere_f();

// vr_hotspot_fit: the weapon in the main hand (any: a sword, the crowbar, a gun): where the hand and the drawn fist are
// on its model (model units: its generator's frame), its tip's distance, and where the off hand must move to take each
// hotspot (a grip: its point; a blade: its zone's ends and middle), in the mock's vr_mock_hand metres (right, up, back of
// the view's yaw) from the off hand as it is. For tests without a headset (Debug > Tests > Hotspot Fit).
void hotspotFit_f();

// Wounds painted on models (vr_wounds.cpp): the player's own drawn this frame, that take its wounds: the body, and
// each jointed hand (null where not drawn).
void woundTargets(entity_t* out[3]);

// The jointed hand (vr_handrig.cpp): the skinning matrices of `e` if it is a drawn hand rig (their count, else 0).
[[nodiscard]] int handBonePoses(const entity_t* e, const float** matrices);

void dumpView_f();

// vr_pose_check (vr_posing.cpp): the hand holding the weapon now (`weaponTarget`: the weapon hand; else the other hand,
// holding it by a hotspot) against the pose confirmed: its rig and drawn palm, and the weapon's muzzle, where the pose put
// them relative to each other (`rigInWeapon`, `palmInWeapon`: in the weapon's model frame; no palm if it wasn't seen
// solved while posing).
void posingCheck(bool weaponTarget, int weaponHand, const glm::mat4& rigInWeapon, const glm::vec3* palmInWeapon,
    const glm::mat4& rigWorld);
// vr_pose_check after posing a weapon in a holster: each holster of that kind holding that weapon now, against the pose
// set (the weapon's place, forward and up in the holster's frame, from its point: posing::Candidate::inHolster*).
void holsterPoseCheck(weapons::HolsterKind kind, const qmodel_t* model, const glm::vec3& inPos, const glm::vec3& inFwd,
    const glm::vec3& inUp);

// A new map (VR_OnClientClearState): the per-hand states timed by the client's time or eased frame to frame start
// afresh (a parried blow's knock, the weapons' button hover and morph, the drawn hands' grasp and curls).
void resetClientState();
void graspDump_f();

// Mod_ForName(name, false) for a model asked for every frame, `name` a string constant (its address is the key): kept
// while it is loaded; a missing one remembered until the next map.
[[nodiscard]] qmodel_t* viewModel(const char* name);

// A map's load (VR_NewMap), in VR: the view's own models loaded (the hands' rig, the body and its pauldrons, the leg
// holsters, the wrist gadget), not in the first frame drawn (0.3 s the first time: the body's 0.2).
void prepareModels();

// Forgets every cache keyed by a model (the view models, clip sizes, the jointed hand's check, the grasp shapes): for
// a game directory change, which reuses the models' slots.
void resetCaches();

// vr_model_reload [model ...]: models edited in Blender (docs/vr-port/MODELS_IN_BLENDER.md) read again from their files,
// and what the engine worked out from them forgotten (the anchors' strip order, the grasp's shapes, the collision
// triangles, the body's bones). Without arguments: the weapons (progs/v_*.mdl), the body (progs/vrbody*.mdl: its
// .md5mesh and skins) and the wrist gadget (progs/vrgadget*.mdl) that are loaded.
void modelReload_f();

// The motion review's ghost (vr_motion_review.cpp): a recorded take's weapon (or empty hand) in `hand`, drawn
// translucent and tinted this frame where the game draws a weapon held at the hand pose `pos`, `rot` (hands::State's
// pos and rot, as a take records them): the weapon's own angle offsets and model transform, mirrored in the off hand.
// Asked for every frame it is shown (from VR_BeginFrame); `model` null or not asked for: not drawn.
void setGhost(int hand, qmodel_t* model, const glm::vec3& pos, const glm::vec3& rot, float alpha);

// vr_grasp_bench [n]: solves each hand's grasp of what it holds n times (1000), and prints the times (min, median,
// max, microseconds).
void graspBench_f();
void graspSweep_f();

// Hand/Gun Calibration > Match Controller Preview: how far (world) the empty hand drawn on `hand`'s calibrated controller
// must move for the middle of its fist (grasp::gripChannel's point: the middle of the circles its fingers close round)
// to be on the Show Controller preview's point (the middle of the handle, where OpenXR puts the fist). False without
// the jointed hand, or before it has been drawn.
[[nodiscard]] bool previewGripMove(const hands::State& s, int hand, glm::vec3& worldMove);

} // namespace qvr::view
