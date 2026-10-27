// vr_aggrip.hpp -- armed monsters' grip offsets (ROUND22.md, "Armed humanoids' arms, hands and grips"): where each
// weapon sits in an armed monster's hand, per monster kind (vr_hitreact.cpp's armed rigs: 0 the grunt, 1 the enforcer),
// weapon (its Weapon Offsets slot) and hand, tuned live in VR Settings > Armed Grunts > Grip Editor (vr_agrunt_grip_*),
// kept in vr_agrunt_grips (the changes from the defaults), printed as code by vr_agrunt_grip_dump.

#pragma once

#include "vr_engine.hpp"
#include "vr_rigpose.hpp"

#include "Zancle/String/String.hpp"

#include <glm/glm.hpp>

namespace qvr::aggrip
{

inline constexpr int numKinds = 2;
enum Hand : int
{
    Main = 0,   // the weapon in its main (right) hand, relative to that hand
    Off = 1,    // the weapon in its off hand (dual wielding), relative to that hand
    Second = 2, // the off hand on a two-handed weapon's second grip, relative to that grip (the weapon's turn)
    NumHands = 3,
};

struct Offset
{
    glm::vec3 pos{0.f};    // units: x forward, y left, z up (the hand's grip frame's: the rest pose's gun's)
    glm::vec3 angles{0.f}; // degrees: pitch (down), yaw (left), roll
};

// A weapon's own offset (its Weapon Offsets slot; vr_agarms: on top of its kind's global one).
[[nodiscard]] Offset offset(int kind, int slot, int hand);

// vr_agarms: a kind's hand's grip frame for every weapon (the Grip Editor's All Weapons entry; its slot allWeapons).
inline constexpr int allWeapons = -2;
[[nodiscard]] Offset global(int kind, int hand);

// The editor's subject this frame: the armed monster nearest the view (relinkArms tells each one; `dist` its distance).
void noteSubject(int kind, int slot, int slotL, float dist);
// Once a frame (after the relink): the editor's sliders to its entry and back.
void frame();
// The subject's line for the menu (the Grip Editor's: its kind, its weapon, the hand).
[[nodiscard]] const char* subjectLine();

// The Pose Studio (vr_posestudio.cpp): an entry's offset while a hand drags it (offset() returns it; one at a time;
// none drawn from the archived text meanwhile), then kept (set: written to vr_agrunt_grips; a default's equal dropped).
void setPreview(int kind, int slot, int hand, const Offset& o);
void clearPreview();
void set(int kind, int slot, int hand, const Offset& o);
// Every hand's entry of (kind, slot) back to its default (slot allWeapons: the kind's global ones).
void resetEntries(int kind, int slot);
// Kind `from`'s changes laid on kind `to` (its own dropped): an Every Weapon entry as its change from `from`'s default on
// top of `to`'s, a weapon's own as it is. Returns how many entries.
int copyKind(int from, int to);
// vr_agrunt_grip_dump's lines (the changes from the defaults; none: a comment saying so).
void dumpText(za::String& out);
// configVersion 202 (the defaults became his): the entries equal to their defaults dropped from vr_agrunt_grips, as
// set() would; the text left as it is if any entry in it did not parse. Returns how many were dropped.
int dropDefaultEqual();
// An offset as a rigid motion (yaw about z, then pitch about y (down), then roll about x) and back.
[[nodiscard]] rigpose::Rigid toRigid(const Offset& o);
[[nodiscard]] Offset fromRigid(const rigpose::Rigid& r);

void registerCommands();

} // namespace qvr::aggrip

// Armed humanoids' weapon holds (vr_agholds.inc; Combat > Armed Grunts > Weapon Holds; QC vr_armedgrunt_pose.qc
// VR_AGH_Hands, through weapondrawnpose(world, kind * 1000 + code, 24)): one or both hands on each weapon, per kind.
namespace qvr::agholds
{

inline constexpr int numKinds = 2; // 0 the grunt, 1 the enforcer (QC VR_AM_*)
[[nodiscard]] int count();                         // the rows (vr_agholds.inc's, in its order)
[[nodiscard]] const char* label(int row);           // its weapon's name
[[nodiscard]] const char* key(int row);             // its settings' suffix
[[nodiscard]] int code(int row);                    // QC's code for its weapon
[[nodiscard]] cvar_t& setting(int kind, int row);   // 0 Auto, 1 One-Handed, 2 Two-Handed
[[nodiscard]] int autoHands(int kind, int row);     // Auto's: 1 or 2
// The hands kind `kind` holds weapon `code` with (1 or 2; 0: no row for it, QC's own rule), and its setting (0 Auto).
[[nodiscard]] int hands(int kind, int code, int* settingOut);
// The gun pose categories (QVR_AGGUN rows): whether row `row` is a gun's; its Pose Category setting for kind `kind`
// (0 Auto, 1.. agpose::gunCategories; a melee row: its hold's, never asked); its Auto (an agpose::Class); and the class
// kind `kind`'s poses take for weapon `code` (agpose::Gun for a gun with no row) and its setting (0 Auto).
[[nodiscard]] bool isGun(int row);
[[nodiscard]] const char* poseLabel(int row); // its Pose Category row's ("Nail Pistol: Poses"; "" a melee row)
[[nodiscard]] cvar_t& categorySetting(int kind, int row);
[[nodiscard]] int autoClass(int row);
[[nodiscard]] int poseClass(int kind, int code, int* settingOut);
// Every setting (holds and pose categories) back to Auto.
void reset();
// vr_agholds_info: the table (each kind's setting and hands per weapon); vr_agholds_report: each armed humanoid in view
// as drawn lately: its kind, weapon, the hold asked and the off hand's (on a second grip, cupping, free).
void registerCommands();

} // namespace qvr::agholds
