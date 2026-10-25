// vr_aglegs.hpp -- armed humanoids' legs posed for a crouch (ROUND22.md, "Armed humanoids' legs: the pickup's kneel";
// vr_aglegs 1): taking a weapon up from the floor, an armed monster (vr_hitreact.cpp's armed rigs: the grunt, the
// enforcer) goes down on one knee instead of its take frames' kneel on both.
//
// - The pose: its pelvis lowered (moderately), a little forward and turned (the kneeling hip back), one foot planted
//   flat ahead (its shin near upright), the other knee down behind it on its toes; the side opposite the reaching hand
//   kneels. Feet on the floor (traced); the pelvis eases down with the reach and up with the lift (the take's weight,
//   QVR_SVC_ARMPOSE). The body above is the rest pose's on the pelvis (the take frames' hunch dropped; the arms
//   solved on it), bent further at the hips as far as the taking hand needs to reach its target.
// - The legs: two-bone IK per leg (hip, knee, ankle; the knee towards its pole), the foot a third bone (flat, or on
//   its toes), from the rig's fit (its thighs and shins; the ankle and toe found in the shin's vertices).
// - The skin: each leg vertex weighed once a model between its thigh, shin and foot (smooth over the knee and the
//   ankle, the hip blended into the pelvis), drawn by the alias shader's RigLeg (agarms' fourth rest row, the codes
//   past the arms').
// - Keys: the pickup's legs are a pose key of their own (vr_agpose.hpp legCode: vr_agrunt_poses, the dump, Copy),
//   dragged in the Pose Studio's Pickup pose (its boxes from `placed`: the pelvis, the feet, the knees' poles).

#pragma once

#include "vr_engine.hpp"
#include "vr_ragdoll.hpp"
#include "vr_rigpose.hpp"

#include <glm/glm.hpp>

namespace qvr::aglegs
{

inline constexpr int blockRows = 22; // the instance's leg rows after the arms' (vr_animsmooth.cpp, RigLeg)

[[nodiscard]] bool enabled(); // vr_aglegs

// A take now (QC VR_AGP_Pose: sent with the body's turn): its weight (0 standing .. 1 at its lowest) and its reaching
// hand (0 right, 1 left).
struct Take
{
    float w{0.f};
    int side{0};
    // The taking hand's target (the weapon lying, then taken up), its arm's shoulder and the chest's joint (rest), the
    // arm's reach to its grip, the torso's turn sent (degrees: yaw, pitch, roll): the pelvis bends further forward as
    // far as the hand needs to land on the handle (the pose keys' torso may straighten it).
    bool reach{false};
    glm::vec3 target{0.f};
    glm::vec3 shoulder{0.f};
    glm::vec3 chest{0.f};
    float armLen{0.f};
    glm::vec3 torso{0.f};
};

// The rest data's fourth row for leg vertex `v` (agarms' row left empty): x 9 + side * 4 + bone (thigh, shin, foot)
// + its blend to the next, y its hip's share on the pelvis, z its rest normal (packed), w `rowsAt` (its rows' offset
// from the bones' first). False: not a leg vertex (or no legs on this model).
bool restRow(const qmodel_t* model, const aliashdr_t* hdr, const ragdoll::Rig& rig, int v, int rowsAt, glm::vec4& out);

// relinkSkinned: `e`'s legs posed now for its take (`kind` agpose's), its frame's bones `posed` (before any turn), its
// drawn place (origin, yaw: the floor's traces). Out: each bone's turn carried by the pelvis (the legs' own: none),
// to compose on `posed`. False: no legs now (all none).
bool solve(const entity_t* e, const aliashdr_t* hdr, const ragdoll::Rig& rig, int kind, const rigpose::Rigid* posed,
    const Take& take, const glm::vec3& origin, float yawDeg, float scale, rigpose::Rigid* carry);

// boneTurns: the same carry for `posed` (as drawn this frame); false: none.
bool carry(const entity_t* e, const ragdoll::Rig& rig, const rigpose::Rigid* posed, rigpose::Rigid* out);

// kickInstance: `e`'s leg rows (blockRows); its on-share 0 when no legs now.
void rows(const entity_t* e, const aliashdr_t* hdr, glm::vec4* out);

// The legs as solved (model space; the Pose Studio's boxes): sides 0 right, 1 left.
struct Placed
{
    float on{0.f};
    float w{0.f};
    int kneel{1};            // the kneeling side
    rigpose::Rigid pelvis;   // its pivot's frame now
    glm::vec3 hip[2]{}, knee[2]{}, ankle[2]{}, toe[2]{}, pole[2]{};
};
bool placed(int ent, Placed& out);

void registerCommands(); // vr_aglegs_info

// The Pose Studio's Live tape (vr_poselive.hpp): `ent`'s state here (its eases, its last frame's) as bytes, its length
// returned (0: none); and put back (a length of 0: dropped).
inline constexpr int stateBytes = 1024;
int saveState(int ent, unsigned char* out);
void loadState(int ent, const unsigned char* in, int len);

} // namespace qvr::aglegs
