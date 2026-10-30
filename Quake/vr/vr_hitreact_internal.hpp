#pragma once

// vr_hitreact_internal.hpp -- what vr_hitreact.cpp (see vr_hitreact.hpp) shares with its parts, vr_hitreact_arms.cpp
// (the armed monsters' arm poses placed on their drawn bodies) and vr_hitreact_skin.cpp (relinkSkinned: their arms
// posed by vr_agarms and their weapons drawn on their hands). Internal: included by those three alone. What was the
// file's anonymous namespace and more than one of them uses is namespace hitreact_detail: its types, constants and
// the armed monsters' table here, its functions and state declared here and defined in the file that owns them.

#include "vr_mdldequant.hpp"
#include "vr_testkit.hpp"
#include "vr_hitreact.hpp"
#include "vr_client.hpp"
#include "vr_agarms.hpp"
#include "vr_agbody.hpp"
#include "vr_aglegs.hpp"
#include "vr_aggrip.hpp"
#include "vr_animsmooth.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_mem.hpp"
#include "vr_poselive.hpp"
#include "vr_posestudio.hpp"
#include "vr_rigseq.hpp"
#include "vr_rig2.hpp"
#include "vr_rigstudio.hpp"
#include "vr_view.hpp"
#include "vr_render.hpp"
#include "vr_weapons.hpp"
#include "vr_geom.hpp"

#include "Zancle/Base/GetArraySize.hpp"

#include "Zancle/Base/Strncmp.hpp"
#include "Zancle/Chrono/Clock.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Acos.hpp"
#include "Zancle/Math/Asin.hpp"
#include "Zancle/Math/Floor.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <string.h>
#include <type_traits>

using namespace qvr;

namespace hitreact_detail
{

// vr_hitreact.cpp
[[nodiscard]] glm::vec3 drawnOrigin(const entity_t* e, float* yawDeg);
[[nodiscard]] int chestOf(const ragdoll::Rig& rig);
rigpose::Rigid composeRigid(const rigpose::Rigid& a, const rigpose::Rigid& b);
[[nodiscard]] rigpose::Rigid gripRigid(int kind, int slot, int hand);
[[nodiscard]] bool lowCarry(const agarms::Targets& t);
void placeProp(entity_t& p, const entity_t* e, const rigpose::Rigid& w, const rigpose::Rigid* kick);
[[nodiscard]] bool heldProp(int num);
[[nodiscard]] float viewDistance(const entity_t* e);
[[nodiscard]] bool placeable(const rigpose::Rigid& w);

// ---------------------------------------------------------------------------------------------------------------------
// Armed monsters' arms on their weapon (armsWanted; QC vr_armedgrunt.qc, vr_armedmon.qc)

constexpr int heldEffect = 128;                // QC VR_AG_EF_HELD: a weapon in a monster's hand

// The armed monsters' table (QC vr_armedmon.qc VR_AM_Row; docs/vr-port/ARMED_MONSTERS.md): the model it is drawn with
// (its gun's triangles dropped: Misc/quakevr/make_vrgrunt.py), its right hand's vertices (where the server puts its
// weapon's grip), its hidden gun's vertices (never a hand's) and their line (the carry's turn), its arms' bones.
struct VertRange
{
    int first;
    int last;
};
struct ArmedRig
{
    modelmeta::Id model;
    int numVerts;
    int rightHand[3];
    VertRange gun[4];
    int numGun;
    int gunRear[2];
    int gunMuzzle[2];
    int gunTop;
    int gunBottom;
    const char* upperR;
    const char* foreR;
    const char* upperL;
    const char* foreL;
    int anchor; // vr_agarms: its chest's vertex the poses are placed from (QC vr_am_anchor; vr_agarms_info finds it)
    float torsoSides; // vr_agarms: its torso proxy's sides and back fitted from this share of the chest from the waist
    float twistShare; // ... the hand's twist about the forearm the forearm takes
    float wristMore;  // ... degrees its wrist bends past vr_agarms_wrist_most
};
// (the enforcer's: its torso's sides from its lower 0.6, under its shoulder pads; its wrist 20 degrees more, its guns
// held turned further on its long forearms; its twist share as the grunt's, 0.75 left elbows stuck the wrong way:
// ROUND22.md, "The enforcer's rig fixed")
inline constexpr ArmedRig armedRigs[] = {
    {modelmeta::Id::VrGrunt, 555, {32, 26, 31}, {{463, 548}}, 1, {497, 545}, {498, 514}, 529, 496, "upperarm_r", "forearm_r",
        "upperarm_l", "forearm_l", 148, 1.f, 0.5f, 0.f},
    {modelmeta::Id::VrEnforcer, 479, {41, 364, 365}, {{22, 23}, {100, 100}, {400, 430}, {455, 478}}, 4, {425, 420},
        {407, 100}, 409, 412, "upperarm_r", "forearm_r", "upperarm_l", "forearm_l", 322, 0.6f, 0.5f, 20.f},
};

// `model`'s row (nullptr: not an armed monster's model).
[[nodiscard]] inline const ArmedRig* armedRigOf(const qmodel_t* model)
{
    if(!model || model->type != mod_alias)
    {
        return nullptr;
    }
    const modelmeta::ModelMetadata& meta = modelmeta::get(model);
    for(const ArmedRig& a : armedRigs)
    {
        if(meta.is(a.model))
        {
            return &a;
        }
    }
    return nullptr;
}

[[nodiscard]] inline bool isGunVertex(const ArmedRig& a, int v)
{
    for(int i = 0; i < a.numGun; i++)
    {
        if(v >= a.gun[i].first && v <= a.gun[i].last)
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] inline glm::vec3 toWorld(const entity_t* e, const glm::vec3& local)
{
    float yawDeg = 0.f;
    const glm::vec3 o = drawnOrigin(e, &yawDeg);
    const float yaw = yawDeg * (3.14159265f / 180.f);
    const float c = za::cos(yaw), s = za::sin(yaw);
    const glm::vec3 d = local * za::max(ENTSCALE_DECODE(e->scale), 0.01f);
    return o + glm::vec3{c * d.x - s * d.y, s * d.x + c * d.y, d.z};
}

[[nodiscard]] inline int boneNamed(const ragdoll::Rig& rig, const char* name)
{
    for(int b = 0; b < rig.numBones; b++)
    {
        if(strcmp(rig.bones[b].name, name) == 0)
        {
            return b;
        }
    }
    return -1;
}

// ---------------------------------------------------------------------------------------------------------------------
// Armed grunts' arm poses (QVR_SVC_ARMPOSE, QC VR_AG_SendPose; ROUND22.md, "Armed grunts: presentation"): where the server
// wants its hands and its weapon, relative to its drawn body and its drawn right hand, every server frame. The client puts
// them on the body as drawn now (its step lerp, its poses' lerp), between the last two messages: the weapon drawn there
// (the held entity's origin and angles replaced after the relink; the server's own is for the game) and the hands on it.

constexpr int maxArmPoses = 64;
constexpr double armPoseStale = 0.3; // seconds without a message: back to finding its weapon by its hand
constexpr int apLeft = 2;            // the left hand to its own target (else on the weapon's second grip, or free)
constexpr int apLeftFree = 4;        // ... the left as the animation has it
constexpr int apLeftWeapon = 8;      // ... the left holding its own weapon at its target (dual wielding: propL, its turn)
constexpr int apAnchor = 64;         // vr_agarms: wr and wl weigh its chest's anchor vertex as drawn, not its hands (QC VR_AGP_ANCHOR)
constexpr int apBody = 32;           // its body's turns in a strike follow (torso, elbows: bodyTurn)
constexpr int apFixed = 1;           // ... its hands' targets not turned with them (a take's bend: QC VR_AGP_FIXED)
constexpr int apReach = 128;         // vr_agarms: the right hand reaching for a weapon lying (none sent): its grip to fw, up (QC VR_AGP_REACH)
constexpr int apTwo = 16;            // the Weapon Holds' two-handed hold: a weapon with no second grip cupped (QC VR_AGP_TWO); apLeftFree its one-handed (VR_AGP_FREE)
constexpr float handEase = 0.04f;    // seconds: the animation's hand followed so (its jumps smoothed)

struct ArmPoseMsg
{
    double t{-1.0}; // cl.mtime[0] it came with
    int prop{0};
    int flags{0};
    float wr{1.f};  // the right hand's target: wr * the drawn right hand + v (its body's space, units)
    glm::vec3 v{0.f};
    glm::vec3 fwB{1.f, 0.f, 0.f}, upB{0.f, 0.f, 1.f}; // the weapon's turn in its body's space,
    glm::vec3 fwG{1.f, 0.f, 0.f}, upG{0.f, 0.f, 1.f}; // ... in its drawn rifle line's,
    float wo{1.f};                                     // ... the second's share
    float wl{0.f};  // the left hand's target: wl * the drawn left hand + vl
    glm::vec3 vl{0.f};
    int propL{0};   // apLeftWeapon: the left hand's weapon, its turn in its body's space
    glm::vec3 fwL{1.f, 0.f, 0.f}, upL{0.f, 0.f, 1.f};
    glm::vec3 torso{0.f};                  // apBody: its torso's turn in a strike (yaw, pitch, roll; degrees)
    glm::vec3 elbowR{0.f}, elbowL{0.f};    // ... its elbows' poles (its body's space; their length the share)
    glm::vec3 waist{0.f};                  // ... the point QC turned its targets about (its body's space, units)
    float take{0.f};                       // apFixed: a take's weight (vr_aglegs: 0 standing .. 1 at its lowest)
    int takeSide{0};                       // ... its reaching hand (0 right, 1 left)
    // The pose keys' off hand (QC's off-hand block, told by apLeftFree and apTwo both set: agpose GripPos/GripTurn,
    // LPos/LTurn): its grip on the right weapon moved and turned (the weapon's frame), the free hand's (its body's space).
    bool offKeyed{false};
    bool keyGun{false}; // its key's Off Hand On the Gun (the block's 32): cupped even with a pistol carried low
    glm::vec3 gripPos{0.f}, gripTurn{0.f};
    glm::vec3 freePos{0.f}, freeTurn{0.f};
    float stretch{1.f}; // its kind and class's IK stretch (agpose::stretch; the block's 16)
};

struct ArmPose
{
    int ent{0};
    ArmPoseMsg prev, cur;
    int frame{-1};   // host_framecount its targets were placed on
    int prop{0};
    int flags{0};
    glm::vec3 hand{0.f};                         // the animation's right hand, eased (handEase)
    double handTime{0.0};
    glm::vec3 right{0.f}, left{0.f};             // the hands' targets (model space, units)
    glm::vec3 fw{1.f, 0.f, 0.f}, up{0.f, 0.f, 1.f}; // the weapon's turn (model space)
    int propL{0};                                // the left hand's weapon (apLeftWeapon), drawn at `left`
    glm::vec3 fwL{1.f, 0.f, 0.f}, upL{0.f, 0.f, 1.f};
    glm::vec3 torso{0.f};                        // its torso's turn now (degrees: bodyTurn), its elbows' poles
    glm::vec3 elbowR{0.f}, elbowL{0.f};
    glm::vec3 waist{0.f};                        // (model space: the message's over its scale)
    glm::vec3 anchorAt{0.f};                     // vr_agarms: its chest's anchor as the frames draw it (model space; apAnchor),
    float wr{0.f}, wl{0.f};                      // ... its weight in the right and left targets (the upper body moves them with it)
    float take{0.f};                             // a take's weight, its reaching hand (vr_aglegs)
    int takeSide{0};
    bool offKeyed{false};                        // the pose keys' off hand (ArmPoseMsg's; the free hand's in model space)
    bool keyGun{false};                          // its key's Off Hand On the Gun (ArmPoseMsg's; in offKeyed's padding)
    glm::vec3 gripPos{0.f}, gripTurn{0.f}, freePos{0.f}, freeTurn{0.f};
    float stretch{1.f};                          // its IK stretch (ArmPoseMsg's: the arms' Targets::stretch)
    // vr_agholds_report: as its arms were last solved (vr_agarms): its kind, its weapon's slot, its off hand's goal (0
    // free, 1 the second grip, 2 cupping the right hand, 3 its own point, 4 its own weapon), the grip held, its point (the
    // weapon's frame, units).
    int seenFrame{-1};
    int seenKind{-1};
    int seenSlot{-1};
    int seenLeft{0};
    bool seenHeld2{false};
    glm::vec3 seenSecond{0.f};
};

struct ArmPoses
{
    ArmPose slots[maxArmPoses];
};
extern ArmPoses armPoses; // (vr_hitreact_arms.cpp)

// vr_agrunt_ik_err's ragdoll numbers (ragdollProps): the server's placing of a ragdoll's weapons against its drawn hand.
struct RagErr
{
    int n{0};
    float offMax{0.f}, offSum{0.f}, turnMax{0.f};
    float propFar{0.f}; // vr_agarms: a held weapon drawn this far from its holder's origin, most (units)
};
extern RagErr ragErr; // (vr_hitreact.cpp)

// vr_hitreact_arms.cpp
[[nodiscard]] ArmPose* armPoseOf(int ent, bool make);
void drawnPoses(const entity_t* e, const aliashdr_t* hdr, int& pose1, int& pose2, float& blend);
[[nodiscard]] glm::vec3 weaponAngles(const glm::vec3& fw, const glm::vec3& up);
[[nodiscard]] glm::vec3 turnYaw(float yawDeg, const glm::vec3& d);
void layOnChest(const ragdoll::Rig& rig, int chest, const rigpose::Rigid& corr, rigpose::Rigid* bones);
bool placeArmPose(const entity_t* e, const aliashdr_t* hdr, int pose1, int pose2, float blend, ArmPose& s);
[[nodiscard]] const ArmPose* armPoseNow(const entity_t* e);
bool bodyTurn(const ragdoll::Rig& rig, const rigpose::Rigid* posed, const ArmPose& s, rigpose::Rigid* out);
[[nodiscard]] rigpose::Rigid fitRigid(const glm::vec3* a, const glm::vec3* b, int n);
[[nodiscard]] rigpose::Rigid inverseRigid(const rigpose::Rigid& a);
[[nodiscard]] glm::quat frameRot(const glm::vec3& fw, const glm::vec3& upHint);
bool specOf(const ArmedRig& a, const ragdoll::Rig& rig, const aliashdr_t* hdr, agarms::Spec& s);

// vr_hitreact_skin.cpp
void relinkSkinned(const ArmPose& s, const entity_t* e, const aliashdr_t* hdr, int pose1, int pose2, float blend);

} // namespace hitreact_detail

using namespace ::hitreact_detail;
