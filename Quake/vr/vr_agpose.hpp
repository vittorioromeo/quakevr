// vr_agpose.hpp -- armed humanoids' pose keys (the Pose Studio, stage 2; ROUND22.md, "Pose Studio"): changes laid on
// top of QC's procedural poses (QC/vr_armedgrunt_pose.qc VR_AGP_Keys), set by hand in the Pose Studio. One key per
// monster kind (0 the grunt, 1 the enforcer), weapon class, pose and phase: the right target's place and turn (its
// weapon, or its fist), the left's (its weapon, its free hand, its fist), each elbow's pole and the torso's turn. QC
// reads them through weapondrawnpose(e, code, 11..18) (the builtin slots are full) and blends them by its pose's
// weights: the stance's (carry or aim), the run's, the pickup's, a strike's phases (its wind-up, its blow, its follow
// through). Kept in vr_agrunt_poses (the changes from the shipped keys below, `shipped`); vr_agrunt_pose_dump prints
// them as code.

#pragma once

#include "Zancle/String/String.hpp"

#include <glm/glm.hpp>

namespace qvr::agpose
{

inline constexpr int numKinds = 2;

// The weapon classes (QC VR_AGP_KeyClass: the same numbers)
enum Class : int
{
    Fists = 0,
    Gun = 1,       // one gun
    Melee = 2,     // a one-handed melee weapon
    Melee2 = 3,    // a two-handed one (the left hand on its second grip)
    Saw = 4,       // a chainsaw
    TwoMelee = 5,  // a melee weapon in each hand
    TwoGuns = 6,   // a gun in each hand
    NumClasses = 7
};

// The poses (QC: the same numbers); a strike's keys are its phases, a stance's phase 0 only.
enum Pose : int
{
    Carry = 0, // idle: carried (a two-handed melee weapon at port arms; the fists hanging)
    Aim = 1,   // an enemy ahead: a gun aimed, a melee weapon ready, the fists' guard
    Run = 2,   // on top of the stance, running (mid-stride)
    Pickup = 3, // taking a weapon up from the floor (its lowest frame)
    Jab = 10,
    Cross = 11,
    Hook = 12, // (the right's; a left hook mirrors it)
    Uppercut = 13,
    Overhead = 14,
    Diagonal = 15,
    Horizontal = 16,
    Backhand = 17, // (two held: a left hand's swing mirrors the right's key)
    SawThrust = 18,
    Dual = 19,     // two held, both at once (the scissor)
    NumPoses = 20
};

inline constexpr int numPhases = 3; // a strike's: 0 the wind-up, 1 the blow, 2 the follow-through

enum Part : int
{
    RPos = 0, // the right target's place (its body's space, x forward, y left, z up; units)
    RTurn,    // ... its turn (a rotation vector in its body's space: its axis, its length in degrees)
    LPos,     // the left's
    LTurn,
    ElbowR,   // the right elbow's pole (its chest's space; its length its share, added to the strike's own)
    ElbowL,
    Torso,    // the torso's turn about its waist: yaw (left), pitch (forward), roll (to its right); degrees
    NumParts
};

struct Key
{
    glm::vec3 v[NumParts]{};
};

[[nodiscard]] constexpr int code(int kind, int cls, int pose, int phase)
{
    return kind * 10000 + cls * 1000 + pose * 10 + phase;
}

[[nodiscard]] constexpr bool isStrike(int pose)
{
    return pose >= Jab && pose < NumPoses;
}

// A pose's legs (vr_aglegs.cpp; the Pickup's kneel): a key of its own, the pose's phase 1 on class 0 (the legs the same
// with any weapon; QC never reads it), its parts so (its body's space as the arms' keys: changes on the procedural
// pose, the places in units; for a right hand's reach, mirrored for a left's):
enum LegPart : int
{
    LegPelvis = 0,     // the pelvis's place (forward, left, up)
    LegPelvisTurn = 1, // ... its turn: yaw (left), pitch (forward), roll (to its right); degrees
    LegPlantFoot = 2,  // the planted foot's place (its ankle)
    LegKneelKnee = 3,  // the kneeling knee's pole (where it goes, as near as the leg allows)
    LegKneelFoot = 4,  // the kneeling foot's place (its ankle; its toes stay on the floor)
    LegPlantKnee = 5,  // the planted knee's pole
};
[[nodiscard]] constexpr int legCode(int kind, int pose)
{
    return code(kind, 0, pose, 1);
}
[[nodiscard]] constexpr bool isLegCode(int c)
{
    return c >= 0 && c % 10 == 1 && (c / 10) % 100 == Pickup;
}

// Key `code` now (the studio's preview, else the archived change, else the shipped key); false: none (all zero).
bool get(int code, Key& out);
[[nodiscard]] Key value(int code);

// The Pose Studio's drag: key `code` while a hand moves it (get returns it), then kept (set: written to
// vr_agrunt_poses, rounded as the dump prints; a shipped key's equal dropped).
void setPreview(int code, const Key& k);
void clearPreview();
void set(int code, const Key& k);
// Every phase of (kind, cls, pose) back to the shipped keys.
void resetPose(int kind, int cls, int pose);
// The Pickup's legs (legCode) of `kind` back to the shipped key.
void resetLegs(int kind);
// Every change of kind `from` laid on kind `to` (its own dropped): the places scaled by their fits (QC VR_AM_Fit's
// scale), the turns as they are. Returns how many keys.
int copyKind(int from, int to);

// The changes as code: rows for `shipped` (and, for the stances whose place is one QC constant, that constant's line).
void dumpText(za::String& out);

[[nodiscard]] const char* className(int cls);
[[nodiscard]] const char* poseName(int cls, int pose);
[[nodiscard]] const char* phaseName(int phase);
// The poses class `cls` steps through (the Pose Studio's < Pose >), in order; returns how many (at most `max`).
int posesOf(int cls, int* out, int max);

void registerCommands(); // vr_agrunt_pose_dump

} // namespace qvr::agpose
