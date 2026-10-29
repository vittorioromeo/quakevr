// vr_agarms.hpp -- armed humanoids' arms (ROUND22.md, "Armed humanoids' arms: posed and skinned"; vr_agarms 1): the arms
// of an armed monster (vr_hitreact.cpp's armed rigs: the grunt, the enforcer) taken off its animation and posed by the
// weapon it holds, then drawn skinned.
//
// - The pose: the server (QC vr_armedgrunt_pose.qc) puts each weapon where a player would hold it (a long gun shouldered
//   and aimed, a pistol out in one hand, a blade ready or swung) and sends it (QVR_SVC_ARMPOSE); the weapon's entity frame
//   is the hand's, as your controller is to the weapon you hold (its Weapon Offsets draw the weapon off it). The hand is
//   put there (its grip frame: one per monster kind, the Grip Editor's global offsets, a weapon's own on top), the off
//   hand on a two-handed weapon's second grip (its Weapon Offsets hotspot) or its own weapon, target or hanging free.
// - The arm: two-bone IK from the shoulder (carried by the chest as drawn, its hit reactions too) to the wrist, its elbow
//   where the forearm keeps the wrist as the rest pose has it, low and out, out of the torso; joints limited by reach.
// - The skin: each arm vertex weighed once a model between its upper arm, forearm and hand (the rest pose's bones, the
//   rig's fit; smooth over the elbow and the wrist, its shoulder blended into the body as animated), drawn by the alias
//   shader's RigArm (blends about the joint: no gaps, no stretching; the hand rigid).
//
// The animation keeps the legs, hips, torso and head; ragdolls (staggers, falls, deaths) keep their own paths.

#pragma once

#include "vr_engine.hpp"
#include "vr_ragdoll.hpp"
#include "vr_rigpose.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace qvr::agarms
{

inline constexpr int numBones = 6;   // right upper arm, forearm, hand; left upper arm, forearm, hand
inline constexpr int blockRows = 26; // the instance's arm rows after its bones' kicks (vr_animsmooth.cpp, RigArm)

// What a model's rig tells (vr_hitreact.cpp's armed rig row for it).
struct Spec
{
    int kind{0};                // its row (0 the grunt, 1 the enforcer)
    int upper[2]{-1, -1};       // the rig's bones, right then left
    int fore[2]{-1, -1};
    int chest{-1};
    glm::quat gunRot{1.f, 0.f, 0.f, 0.f}; // the rest pose's gun (x its barrel, z up): the hands' grip frames' turn
    bool (*isGun)(const void* row, int v){nullptr}; // its hidden gun's vertices (never an arm's)
    const void* row{nullptr};
    float torsoSides{1.f};  // its torso proxy's sides fitted from this share of the chest from the waist (agbody::of)
    float twistShare{0.5f}; // the hand's twist about the forearm the forearm takes (the rest at the wrist)
    float wristMore{0.f};   // degrees its wrist may bend past vr_agarms_wrist_most
};

// A hand's goal: where its grip frame goes (model space), or a point it reaches for.
enum class Goal : int
{
    None = 0,  // hanging free by its side
    Frame = 1, // its grip frame to `frame` (a weapon's, a second grip's)
    Point = 2, // its grip to `frame.pos`, the fist pointing from its shoulder (a punch, a guard)
};

struct Targets
{
    Goal goal[2]{Goal::None, Goal::None};
    rigpose::Rigid weapon[2];  // Frame: the weapon's entity frame wanted (model space); Point: .pos the fist's point
    rigpose::Rigid offset[2];  // Frame: the hand's grip frame to its weapon's (the Grip Editor's offsets)
    bool second{false};        // the off hand on the right weapon's second grip (its first Grip hotspot):
    rigpose::Rigid secondAt;   // ... its grip frame there, in the right weapon's frame
    bool cup{false};           // the off hand under and round the right hand (a Cup hotspot)
    // The pose keys' off hand (agpose GripPos/GripTurn, LPos/LTurn; agent/offhand), when `offKeyed`: on the right weapon
    // (its second grip, a cup) its grip frame moved by gripOff.pos and turned by gripOff.rot about its grip point, both in
    // the weapon's frame; hanging free, its grip frame moved by freeOff.pos and turned by freeOff.rot about its grip point
    // (model space). Not keyed: as before, exactly.
    bool offKeyed{false};
    rigpose::Rigid gripOff;
    rigpose::Rigid freeOff;
    // The pose key's Off Hand On the Gun (agpose::OffGun, chosen; QC's off-hand block): with the second grip out of reach
    // (a take's lowest, a gun pushed out) the off hand cups the right hand instead of letting go.
    bool keyGun{false};
    bool reaching{false};      // the right hand reaching for a weapon lying (QC VR_AGP_REACH): eased in as it nears (vr_agarms_reach_ease)
    glm::vec3 pole[2]{glm::vec3{0.f}, glm::vec3{0.f}}; // an elbow's pole wanted (model space; its length the share): a strike's (agent/agstrike)
    // The Rig Studio's animations (vr_rigseq.cpp): a hand's motion (rest -> now: its forearm bone's) given, its goal's
    // place ignored (its weapon still drawn on it as the goal holds it); the arm's IK onto it as onto any goal.
    bool fk[2]{false, false};
    rigpose::Rigid fkHand[2];
    // The IK stretch (agpose::stretch of its kind and pose class; QC sends it): how far both arms may lengthen to reach
    // their goals (1 none). Eased per entity (stretchEase): a change of class never pops.
    float stretch{1.f};
};

// Where the hands put their weapons (model space): drawn there.
struct Placed
{
    bool on[2]{false, false};
    rigpose::Rigid weapon[2];
    bool held2{false}; // the off hand on the second grip (else free)
    float slid{0.f};   // ... slid back along the weapon from it, out of its reach (units)
    bool cupped{false}; // the off hand cupping the right hand (a Cup, or the key's On the Gun past the second grip's reach)
    // The arms as solved (model space; the Pose Studio's handles): each hand's grip frame, elbow and shoulder; the chest.
    rigpose::Rigid hand[2];
    glm::vec3 elbow[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    glm::vec3 shoulder[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    rigpose::Rigid chest;
    // What held each hand off its goal this frame (the Pose Studio names it): its wrist brought into reach (units), its
    // hand pushed out of the body (units), turned back at its wrist's most (degrees); its elbow off its pole (degrees)
    // and why (Limit).
    float reach[2]{0.f, 0.f};
    float pushed[2]{0.f, 0.f};
    float wrist[2]{0.f, 0.f};
    float poleOff[2]{0.f, 0.f};
    int poleWhy[2]{0, 0};
    glm::vec3 clavicle[2]{glm::vec3{0.f}, glm::vec3{0.f}}; // each clavicle's pivot (model space; vr_agarms_clavicle)
    // The arm's bones as solved (rest -> now, model space): its upper arm, forearm and hand (the authored rig's arms are
    // laid on these within its limits: vr_rig2_game.cpp).
    rigpose::Rigid upperM[2], foreM[2], handM[2];
    // The IK stretch used by each arm (its upper arm and forearm lengthened alike; 1 none: in reach) and the most it
    // could (Targets::stretch eased). The bones above take the rest joints to the stretched ones: the upper arm the
    // shoulder (its elbow at the stretched length along it), the forearm the elbow, the hand the wrist.
    float stretch[2]{1.f, 1.f};
    float stretchMost{1.f};
};

// The Rig Studio's: an armed model's shoulder whose upper arm is rig bone `bone` (its pivot the shoulder): its side, its
// chest bone, its clavicle's pivot and the shoulder joint (rest), the shoulder's cone's axis (rest chest space), the
// limits (degrees) and whether the IK keeps them. False: not an armed model's upper arm (or its skin not made yet).
struct ShoulderInfo
{
    int side{0};
    int chest{-1};
    glm::vec3 clavicle{0.f}, shoulder{0.f}, axis{0.f, 0.f, -1.f};
    float cone{0.f}, twist{0.f};
    bool on{false};
};
bool shoulderLimits(const qmodel_t* model, int bone, ShoulderInfo& out);

// Placed::poleWhy: the cost that most held an elbow off its pole.
enum Limit : int
{
    LimitNone = 0,
    LimitWrist,    // its wrist's comfort (the hand's turn on the forearm)
    LimitBody,     // the body's proxies (vr_agarms_body)
    LimitShoulder, // up past its shoulder, in across its chest
    LimitRate,     // vr_agarms_elbow_rate: still swinging there
    LimitJoint,    // vr_agarms_limits: its shoulder's cone and twist
};

[[nodiscard]] bool enabled(); // vr_agarms (read at a map load for the models' rest data)

// The rest data's fourth row for vertex `v` (vr_animsmooth.cpp, SmoothRest): x its chain (0 none; 1 + side * 4 + bone +
// its blend to the next bone), y its share of the body as animated, z its rest normal (packed), w its arm rows' offset
// from the bones' first. False: no arm skin on this model (all zero).
bool restRow(const qmodel_t* model, const aliashdr_t* hdr, const ragdoll::Rig& rig, const Spec& spec, int v, glm::vec4& out);

// relinkArms: `e`'s arms solved now onto `t` (posed: its bones as drawn; kick: their hit reactions, or null), its weapons
// drawn on its hands. False: no skin (then the old way).
bool solve(const entity_t* e, const aliashdr_t* hdr, const ragdoll::Rig& rig, const Spec& spec, const rigpose::Rigid* posed,
    const rigpose::Rigid* kick, const Targets& t, Placed& out);

// Whether `e` is drawn with posed arms now (solved this frame, or easing back to its animation's).
[[nodiscard]] bool active(const entity_t* e);

// A ragdoll made of `num` (vr_box3d_ragdoll.cpp createRagdoll): rig bone `bone`'s frame (as ragdoll::bonePose's: `rot`,
// `pos` given its frame's, model units) moved to where its posed arms were last drawn (an upper arm, a forearm, a hand:
// on its weapons, not the animation's), eased as they were. False (left as given): not one of them, or not posed lately.
bool drawnArmBone(int num, const qmodel_t* model, const ragdoll::Rig& rig, int bone, glm::quat& rot, glm::vec3& pos);

// That ragdoll's hand `side` (0 right) on its forearm as it was drawn when the ragdoll was made (rest model space; a rig
// with no hand bone: its weapon is drawn by it on the forearm, vr_hitreact.cpp ragdollProps). False: none.
bool ragdollWrist(int num, const qmodel_t* model, int side, rigpose::Rigid& out);


// kickInstance: `e`'s arm rows (blockRows, the poses' units), its model's skin's. False: none (its rest data has no skin).
bool rows(const entity_t* e, const aliashdr_t* hdr, glm::vec4* out);

// A ragdoll's weapon on its hand (a stagger, a fall): its hand's grip frame (rest, model units) and the rig bone its hand
// is on. False: none.
bool ragdollGrip(const qmodel_t* model, int side, rigpose::Rigid& grip, int& bone);

// vr_agrunt_ragweapon_check: hand `side`'s vertices (wholly its hand's), the model's rest places (every vertex's) and
// the hand's grip frame at rest (model units), as the posed arms hold a weapon in it. False: none.
bool handSkin(const qmodel_t* model, int side, const za::Vector<int>*& verts, const za::Vector<glm::vec3>*& rest,
    rigpose::Rigid& grip);

// vr_agrunt_ik_err with vr_agarms 1: the arm numbers under vr_agrunt_ik_tag.
void clearErr();
void printErr();

void registerCommands();
// The models' skins and the entities' arms forgotten (a rig file reloaded: rig2::latch; made again as drawn).
void resetModels();

// The Pose Studio's Live tape (vr_poselive.hpp): `ent`'s state here (its eases, its last frame's) as bytes, its length
// returned (0: none); and put back (a length of 0: dropped).
inline constexpr int stateBytes = 1024;
int saveState(int ent, unsigned char* out);
void loadState(int ent, const unsigned char* in, int len);

} // namespace qvr::agarms
