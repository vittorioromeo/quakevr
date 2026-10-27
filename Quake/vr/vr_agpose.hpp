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

// The armed kinds (QC vr_armedmon.qc VR_AM_*: its table's rows, VR_AM_COUNT; aggrip and agholds the same).
inline constexpr int numKinds = 2;
[[nodiscard]] const char* kindName(int kind);  // "grunt", "enforcer"
[[nodiscard]] const char* kindsName(int kind); // "grunts", "enforcers"
[[nodiscard]] const char* kindTitle(int kind); // "Grunt", "Enforcer"

// The weapon classes (QC VR_AGK_*: the same numbers). A single gun's class is its pose category (the Weapon Holds'
// Pose Category per kind and weapon, Auto its table's: vr_agholds.inc; agholds::poseClass): Gun the rifle (a long gun
// shouldered: every gun's class before the categories, his tuned keys), the others from 17. 7..9 unused; 10..16 never a
// class: a code whose class digits are 10..16 is an old-format enforcer key (codeFromOld).
enum Class : int
{
    Fists = 0,
    Gun = 1,       // a rifle: one long gun (the shotguns, the nailguns, the laser rifle, the plasma guns ...)
    Melee = 2,     // a one-handed melee weapon
    Melee2 = 3,    // a two-handed one (the left hand on its second grip)
    Saw = 4,       // a chainsaw
    TwoMelee = 5,  // a melee weapon in each hand
    TwoGuns = 6,   // a gun in each hand
    Pistol = 17,   // a pistol (the nail pistol, the revolver, the flare gun: the off hand free or cupping, the Weapon Holds')
    Launcher = 18, // a shoulder launcher (the rocket launchers)
    Sprayer = 19,  // a sprayer (the flamethrower, the poison sprayer)
    LaserCannon = 20, // the laser cannon (its own)
    Widowmaker = 21,  // the Widowmaker (its own)
    NumClasses = 22   // (the numbers' bound: isClass says which are classes)
};
[[nodiscard]] constexpr bool isClass(int cls)
{
    return (cls >= Fists && cls <= TwoGuns) || (cls >= Pistol && cls <= Widowmaker);
}
// A single gun's class (the categories, Gun first).
[[nodiscard]] constexpr bool isGunCategory(int cls)
{
    return cls == Gun || (cls >= Pistol && cls <= Widowmaker);
}
// One gun or two (aimed: the Aim stance).
[[nodiscard]] constexpr bool isGunClass(int cls)
{
    return isGunCategory(cls) || cls == TwoGuns;
}
// The pose categories as the Weapon Holds' Pose Category setting numbers them (1.., 0 Auto).
inline constexpr int gunCategories[] = {Gun, Pistol, Launcher, Sprayer, LaserCannon, Widowmaker};
inline constexpr int numGunCategories = 6;
// The classes in the windows' order (the guns, the melee weapons, the fists).
inline constexpr int classOrder[] = {Gun, Pistol, Launcher, Sprayer, LaserCannon, Widowmaker, TwoGuns, Melee, Melee2,
    Saw, TwoMelee, Fists};
inline constexpr int numClassOrder = 12;

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

// A key's code: kind * 100000 + class * 1000 + pose * 10 + phase (the class two digits; before 2026-10-27 kind * 10000,
// one digit: the grunt's codes the same, an enforcer's 1xxxx now 10xxxx: codeFromOld).
inline constexpr int kindStride = 100000;
[[nodiscard]] constexpr int code(int kind, int cls, int pose, int phase)
{
    return kind * kindStride + cls * 1000 + pose * 10 + phase;
}
[[nodiscard]] constexpr int kindOf(int c)
{
    return c / kindStride;
}
[[nodiscard]] constexpr int classOf(int c)
{
    return (c / 1000) % 100;
}
[[nodiscard]] constexpr int poseOf(int c)
{
    return (c / 10) % 100;
}
[[nodiscard]] constexpr int phaseOf(int c)
{
    return c % 10;
}
[[nodiscard]] constexpr int withinKind(int c)
{
    return c % kindStride;
}
// An old-format code (kind * 10000: an enforcer's, its 1 and its class read as the class digits 10..16, never a class's:
// see Class) in the new format; any other code as it is (the grunt's the same in both).
[[nodiscard]] constexpr int codeFromOld(int c)
{
    return c >= 10000 && c < kindStride && classOf(c) >= 10 && classOf(c) <= 16 ? (c / 10000) * kindStride + c % 10000 : c;
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
// configVersion 202 and 210 (the shipped keys became his): the archived keys equal to their shipped ones (or to none, where
// none ships) dropped from vr_agrunt_poses, as set() would; the text left as it is if any entry in it did not parse.
// Returns how many were dropped.
int dropShippedEqual();
// configVersion 211 (the gun categories, the wider code): the archived keys rewritten in the new code (an enforcer's
// 1xxxx now 10xxxx; `widened` how many), and each archived rifle (Gun) key laid on every gun category of its kind, pose
// and phase that has none (as the shipped ones start: nothing changes until a category is tuned). Returns how many
// laid on.
int migrateCategories(int* widened);

[[nodiscard]] const char* className(int cls);  // the dumps' ("a rifle")
[[nodiscard]] const char* classTitle(int cls); // the windows' ("Rifle")
[[nodiscard]] const char* poseName(int cls, int pose);
[[nodiscard]] const char* phaseName(int phase);
// The poses class `cls` steps through (the Pose Studio's < Pose >), in order; returns how many (at most `max`).
int posesOf(int cls, int* out, int max);

void registerCommands(); // vr_agrunt_pose_dump

} // namespace qvr::agpose
