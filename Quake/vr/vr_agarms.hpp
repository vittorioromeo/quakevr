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
inline constexpr int blockRows = 22; // the instance's arm rows after its bones' kicks (vr_animsmooth.cpp, RigArm)

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
    glm::vec3 pole[2]{glm::vec3{0.f}, glm::vec3{0.f}}; // an elbow's pole wanted (model space; its length the share): a strike's (agent/agstrike)
};

// Where the hands put their weapons (model space): drawn there.
struct Placed
{
    bool on[2]{false, false};
    rigpose::Rigid weapon[2];
    bool held2{false}; // the off hand on the second grip (else free)
    // The arms as solved (model space; the Pose Studio's handles): each hand's grip frame, elbow and shoulder; the chest.
    rigpose::Rigid hand[2];
    glm::vec3 elbow[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    glm::vec3 shoulder[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    rigpose::Rigid chest;
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

// kickInstance: `e`'s arm rows (blockRows, the poses' units), its model's skin's. False: none (its rest data has no skin).
bool rows(const entity_t* e, const aliashdr_t* hdr, glm::vec4* out);

// A ragdoll's weapon on its hand (a stagger, a fall): its hand's grip frame (rest, model units) and the rig bone its hand
// is on. False: none.
bool ragdollGrip(const qmodel_t* model, int side, rigpose::Rigid& grip, int& bone);

// vr_agrunt_ik_err with vr_agarms 1: the arm numbers under vr_agrunt_ik_tag.
void clearErr();
void printErr();

void registerCommands();

} // namespace qvr::agarms
