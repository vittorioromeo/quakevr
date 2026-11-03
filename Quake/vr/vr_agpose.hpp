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
#include "vr_agpose_shared.h"

#include <glm/glm.hpp>

namespace qvr::agpose
{

// The armed kinds (QC vr_armedmon.qc VR_AM_*: its table's rows, VR_AM_COUNT; aggrip and agholds the same).
inline constexpr int numKinds = 4; // 0 the grunt, 1 the enforcer, 2 the knight (vr_agknight), 3 the death knight (vr_agdknight)
[[nodiscard]] const char* kindName(int kind);  // "grunt", "enforcer"
[[nodiscard]] const char* kindsName(int kind); // "grunts", "enforcers"
[[nodiscard]] const char* kindTitle(int kind); // "Grunt", "Enforcer"

// The weapon classes (QC VR_AGK_*: both from vr_agpose_shared.h). A single gun's class is its pose category (the Weapon Holds'
// Pose Category per kind and weapon, Auto its table's: vr_agholds.inc; agholds::poseClass): Gun the rifle (a long gun
// shouldered: every gun's class before the categories, his tuned keys), the others from 17. 7..9 unused; 10..16 never a
// class: a code whose class digits are 10..16 is an old-format enforcer key (codeFromOld).
enum Class : int
{
    Fists = VR_AGK_FISTS,
    Gun = VR_AGK_GUN,       // a rifle: one long gun (the shotguns, the nailguns, the laser rifle, the plasma guns ...)
    Melee = VR_AGK_MELEE,     // a one-handed melee weapon
    Melee2 = VR_AGK_MELEE2,    // a two-handed one (the left hand on its second grip)
    Saw = VR_AGK_SAW,       // a chainsaw
    TwoMelee = VR_AGK_TWOMELEE,  // a melee weapon in each hand
    TwoGuns = VR_AGK_TWOGUNS,   // a gun in each hand
    Pistol = VR_AGK_PISTOL,   // a pistol (the nail pistol, the revolver, the flare gun: the off hand free or cupping, the Weapon Holds')
    Launcher = VR_AGK_LAUNCHER, // a shoulder launcher (the rocket launchers)
    Sprayer = VR_AGK_SPRAYER,  // a sprayer (the flamethrower, the poison sprayer)
    LaserCannon = VR_AGK_LASERCANNON, // the laser cannon (its own)
    Widowmaker = VR_AGK_WIDOWMAKER,  // the Widowmaker (its own)
    NumClasses = VR_AGK_NUM_CLASSES   // (the numbers' bound: isClass says which are classes)
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

// The poses (QC VR_AGK_*: both from vr_agpose_shared.h); a strike's keys are its phases, a stance's phase 0 only.
enum Pose : int
{
    Carry = VR_AGK_CARRY, // idle: carried (a two-handed melee weapon at port arms; the fists hanging)
    Aim = VR_AGK_AIM,   // an enemy ahead: a gun aimed, a melee weapon ready, the fists' guard
    Run = VR_AGK_RUN,   // on top of the stance, running (mid-stride)
    Pickup = VR_AGK_PICKUP, // taking a weapon up from the floor (its lowest frame)
    Cast = VR_AGK_CAST,     // the death knight's fireball volley: his off hand out, its palm at you (a stance key)
    Jab = VR_AGK_JAB,
    Cross = VR_AGK_CROSS,
    Hook = VR_AGK_HOOK, // (the right's; a left hook mirrors it)
    Uppercut = VR_AGK_UPPERCUT,
    Overhead = VR_AGK_OVERHEAD,
    Diagonal = VR_AGK_DIAGONAL,
    Horizontal = VR_AGK_HORIZONTAL,
    Backhand = VR_AGK_BACKHAND, // (two held: a left hand's swing mirrors the right's key)
    SawThrust = VR_AGK_SAWTHRUST,
    Dual = VR_AGK_DUAL,     // two held, both at once (the scissor)
    // The shove (2026-10-28, agent/agshove; ROUND22.md, "Armed humanoids' shove"): every class's push of the player up
    // close (QC vr_enemyshove.qc, VR_AGP_SHOVE): its phases the wind-up, the push (the contact: the push lands as it
    // ends) and the recovery.
    Shove = VR_AGK_SHOVE,
    NumPoses = VR_AGK_NUM_POSES
};

inline constexpr int numPhases = 3; // a strike's: 0 the wind-up, 1 the blow, 2 the follow-through (the shove's: push, recover)

enum Part : int
{
    RPos = 0, // the right target's place (its body's space, x forward, y left, z up; units)
    RTurn,    // ... its turn (a rotation vector in its body's space: its axis, its length in degrees)
    LPos,     // the left's
    LTurn,
    ElbowR,   // the right elbow's pole (its chest's space; its length its share, added to the strike's own)
    ElbowL,
    Torso,    // the torso's turn about its waist: yaw (left), pitch (forward), roll (to its right); degrees
    // The off hand on a single gun (2026-10-27, agent/offhand; ROUND22.md, "Off hand: grip offset and Free"): parts
    // after the first seven, missing from older text (read as zero: no change, Auto). LPos/LTurn above are also the free
    // off hand's place and turn change (on its hanging place, model space) while it is off the gun.
    GripPos,  // its grip target on the gun (a second grip, a cup) moved in the weapon's frame (x forward, y left, z up; units)
    GripTurn, // ... turned about its grip point (a rotation vector in the weapon's frame; degrees)
    OffHand,  // x: OffMode (the rest unused)
    // A strike's key's time (2026-11-02, agent/posekeys; ROUND23.md, "Strike keys: their times and in-betweens"): x its
    // place in its act (actPlace: 0 the act's start, 1 the wind-up's end, 2 the strike's, 3 the hold's, 4 the act's end;
    // the fraction that share of the phase), y 1 when set (0: its phase's own place, defaultPlace; an in-between key
    // with y 0 is none). Missing from older text (read as zero: the phases' own places).
    Time,
    NumParts
};
inline constexpr int numOldParts = 7; // (the parts before 2026-10-27: every text and row has these; the rest may be missing)

// A key's off hand (its OffHand part's x): which hold the off hand takes while this key decides it (QC VR_AGK_Keys: the
// Pickup's when set, else Aim's while aimed, else Run's when set while running, else Carry's).
enum OffMode : int
{
    OffAuto = VR_AGK_OFF_AUTO, // the Weapon Holds' (its second grip, a cup, or free for a one-handed hold)
    OffGun = VR_AGK_OFF_GUN,  // on the gun (its second grip, else cupping the right hand) even when the hold is one-handed
    OffFree = VR_AGK_OFF_FREE, // off the gun, hanging free (moved by LPos/LTurn)
    NumOffModes = VR_AGK_NUM_OFF_MODES
};
[[nodiscard]] const char* offModeName(int mode); // "Auto (the Weapon Holds)", "On the Gun", "Free"
[[nodiscard]] inline int offModeOf(const glm::vec3& part)
{
    const int m = static_cast<int>(part.x + 0.5f);
    return m > 0 && m < NumOffModes ? m : OffAuto;
}

struct Key
{
    glm::vec3 v[NumParts]{};
};

// A key's code: kind * 100000 + class * 1000 + pose * 10 + phase (the class two digits; before 2026-10-27 kind * 10000,
// one digit: the grunt's codes the same, an enforcer's 1xxxx now 10xxxx: codeFromOld).
inline constexpr int kindStride = VR_AGK_KIND;
[[nodiscard]] constexpr int code(int kind, int cls, int pose, int phase)
{
    return kind * kindStride + cls * VR_AGK_CLASS + pose * VR_AGK_POSE + phase;
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

// A strike's keys (2026-11-02, agent/posekeys; ROUND23.md, "Strike keys: their times and in-betweens"): its phase digits.
// 0..2 its named keys (the wind-up's, the blow's, the follow-through's; the shove's push and recover), always there (all
// zero: QC's pose as it is); 3..9 in-between keys added in the Pose Studio Data window (a key there only while its Time
// part's y is set). Each key has a place in its act (Time; unset, defaultPlace); the game blends the two keys either
// side of its time now (the stance before the first and after the last), along the act's own progress (actProgress: as
// the three keys always moved), so a key added where the act is changes nothing until it is edited. The act's phases'
// lengths (QC VR_AGP_Times: the wind-up, the strike, the hold, the recovery) and the blow (its hit, a parry's timing,
// the sounds) are QC's, never the keys': moving a key changes the look, not when the blow lands.
inline constexpr int maxStrikeKeys = 10;
inline constexpr float placeGap = 0.01f; // (keys kept at least this far apart in place)
// The act's phases' lengths (s): the wind-up, the strike, the hold (a chainsaw's cut; the shove's recover), the recovery.
struct ActTimes
{
    float t[4]{};
    [[nodiscard]] float length() const { return t[0] + t[1] + t[2] + t[3]; }
};
// The time `place` is at (s), and the place of time `s` (a phase of no length skipped).
[[nodiscard]] float placeTime(const ActTimes& a, float place);
[[nodiscard]] float actPlace(const ActTimes& a, float s);
// The act's progress at time `s` (QC's phases' shares as they always were): -1 the stance at its start, 0 the wind-up's key
// whole, 1 the blow's, 2 the follow-through's (held through a hold), 3 the stance at its end. A punch's, a swing's (the
// chainsaw's) and the shove's each their own (vr_agrunt_punch_ease the punch's).
[[nodiscard]] float actProgress(int pose, const ActTimes& a, float s);
// Phase `phase`'s own place in pose `pose`'s act (where its key was always whole).
[[nodiscard]] float defaultPlace(int pose, int phase);
struct ActKey
{
    int phase{0};
    float place{0.f};
    float progress{0.f};
    bool timed{false}; // (its Time set)
};
// Strike `pose`'s keys of `kind` and `cls`, in their act's order (times `a`); returns how many (at most maxStrikeKeys).
int actKeys(int kind, int cls, int pose, const ActTimes& a, ActKey* out);
// The two keys either side of time `s` (their phases; -1 the stance) and the second's share.
struct ActBlend
{
    int a{-1}, b{-1};
    float f{0.f};
};
[[nodiscard]] ActBlend actBlend(int kind, int cls, int pose, const ActTimes& a, float s);
// The keys as the game blends them at time `s` (an in-between key's start: its Time left zero).
[[nodiscard]] Key keyAt(int kind, int cls, int pose, const ActTimes& a, float s);
// Key `code`'s place (its Time, else its phase's own) and its time; the Cast's key: past its raise.
[[nodiscard]] float keyPlace(int code);
[[nodiscard]] float keyTime(int code, const ActTimes& a);
// Whether strike key `code` is there (a named key always; an in-between key while its Time is set).
[[nodiscard]] bool hasKey(int code);
// An in-between key added at time `s` (as the keys blend there); its phase, or -1 (all seven in use, or a key there).
int addKey(int kind, int cls, int pose, const ActTimes& a, float s);
// In-between key `code` removed (a named key never: false).
bool removeKey(int code);
// Key `code` moved to `place` (kept between its neighbours, placeGap apart); returns the place it took. clearKeyTime: a
// named key back on its phase's own place.
float setKeyPlace(int code, float place);
void clearKeyTime(int code);

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

// The shared poses: the Pickup's arms (2026-10-31, agent/sharedpickup; ROUND23.md, "One shared Pickup pose") and the
// Cast (2026-11-01, agent/night1101; ROUND23.md, "The death knight's Cast pose"): one key per kind every weapon class
// shares (its code on class 7, no class's), and a class's own key of that pose (its code as every key's) an override,
// used only where one is set: archived or shipped (own row, else the followed kind's, fitted), unless its "no override"
// mark (the class's phase 2 of that pose, all zero: archived, or a shipped row of the kind's own) says to use the shared
// one. get() and set() of a class's code of a shared pose take the shared key when the class has no override (QC, the
// studios: the same codes as before); old archived per-class Pickup keys are overrides as they are.
inline constexpr int sharedClass = VR_AGK_SHARED;
inline constexpr int sharedPoses[] = {Pickup, Cast};
[[nodiscard]] constexpr bool isSharedPose(int pose)
{
    return pose == Pickup || pose == Cast;
}
[[nodiscard]] constexpr int sharedCode(int kind, int pose)
{
    return code(kind, sharedClass, pose, 0);
}
[[nodiscard]] constexpr int noOverrideCode(int kind, int cls, int pose)
{
    return code(kind, cls, pose, 2);
}
[[nodiscard]] constexpr bool isSharedKey(int c)
{
    return c >= 0 && classOf(c) == sharedClass && isSharedPose(poseOf(c)) && phaseOf(c) == 0;
}
// A class's own key of a shared pose (an override's: phase 0 of a class).
[[nodiscard]] constexpr bool isClassShared(int c)
{
    return c >= 0 && isClass(classOf(c)) && isSharedPose(poseOf(c)) && phaseOf(c) == 0;
}
[[nodiscard]] bool hasOverride(int kind, int cls, int pose);
// The code an edit of code `c` keeps: a class's shared pose with no override the shared key's; any other `c` itself.
[[nodiscard]] int editTarget(int c);
// Class `cls`'s key of shared pose `pose` made its own (starting as the shared key is now), or back on the shared key.
void makeOverride(int kind, int cls, int pose);
void removeOverride(int kind, int cls, int pose);
// The Pose Studio's name of pose `pose` for kind `kind` and class `cls`: poseName, a shared pose's "Pickup (shared)",
// "Cast (override)"...
[[nodiscard]] const char* poseTitle(int kind, int cls, int pose);

// The kind whose shipped keys `kind` ships, fitted (itself when it has its own; the knights: the grunt's). The grip
// offsets' Every Weapon rows follow it (vr_aggrip.cpp: a kind with no Main or Off row of its own ships that kind's, fitted).
[[nodiscard]] int shippedKind(int kind);

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

// The IK stretch per kind and class (vr_agrunt_ik_stretch, its changes from shippedStretch; like the player's
// vr_body_arm_stretch): how far its arms may lengthen to put a hand on its goal (1 none, at most stretchMost). The arm
// solve (vr_agarms.cpp) lengthens the upper arm and the forearm alike only as far as the goal needs, eased; the skin and
// the authored rig's arm bones stretch along with them. Copy to the other kind takes it as it is.
inline constexpr float stretchMost = 1.5f;
[[nodiscard]] float stretch(int kind, int cls);
[[nodiscard]] float shippedStretch(int kind, int cls);
void setStretch(int kind, int cls, float by); // (rounded to 0.01; the shipped value's equal dropped)

[[nodiscard]] const char* className(int cls);  // the dumps' ("a rifle")
[[nodiscard]] const char* classTitle(int cls); // the windows' ("Rifle")
[[nodiscard]] const char* poseName(int cls, int pose);
[[nodiscard]] const char* phaseName(int phase);
[[nodiscard]] const char* phaseName(int pose, int phase); // (the Shove's: "wind-up", "push", "recover"; 3..9 "in-between")
// The poses class `cls` steps through (the Pose Studio's < Pose >), in order; returns how many (at most `max`).
int posesOf(int cls, int* out, int max);

void registerCommands(); // vr_agrunt_pose_dump

} // namespace qvr::agpose
