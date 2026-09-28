// vr_props.hpp -- per-prop settings (vr_props.inc): what each kind of thing a hand carries weighs and how it is held,
// keyed by the model it is drawn with (a box's "maps/b_explob.bsp", a gib's "progs/gib1.mdl"). Stored as the weapons'
// are (cvars vr_prop_<key>_NN, archived; vr_props_version for changes to the shipped defaults) and edited on the
// Held Object Offsets page (vr_menu_props.inc). See docs/vr-port/ROUND21.md, "Weight: spring model, stamina, held
// object offsets; explosive boxes".
//
// Who reads what:
// - the hands' weight (vr_weight.cpp, client): Mass, Inertia, Com* of what each hand holds;
// - Box3D (vr_box3d.cpp, server): a Mass set here is the body's mass (its density scaled to it);
// - QC (the propvalue and propgrip builtins, vr_carry.qc): Throw, GripMode and Grip*, TwoHands, ForceGrab;
// - the view's grasp (vr_view.cpp, client): the Finger* keys and Overlap.

#pragma once

#include "vr_engine.hpp"

namespace qvr::props
{

inline constexpr int numSlots = 48;

enum class Key : int
{
#define QVR_PROP_KEY(e, k, d) e,
#include "vr_props.inc"
#undef QVR_PROP_KEY
    Count
};

void registerCvars();

// The slot of the model named `model` (-1: none; its keys are then their defaults).
[[nodiscard]] int slotForModel(const char* model);

// A key of a slot (slot -1: the key's default).
[[nodiscard]] float value(int slot, Key key);
[[nodiscard]] float valueFor(const char* model, Key key);
[[nodiscard]] cvar_t* cvar(int slot, Key key);
[[nodiscard]] const char* keyName(Key key); // as in its cvar and propvalue's argument ("mass")
[[nodiscard]] Key keyByName(const char* name); // Key::Count if none

// The model's slot, a free one given to it if it has none (-1: the table is full). The menu edits a prop through it.
[[nodiscard]] int claimSlot(const char* model);
void resetSlotToDefaults(int slot);
void printSlot(int slot); // the settings that differ from the defaults, as vr_props.inc lines

// Densities (kg/m^3) of what things are made of, by their model (a brush model: an ammo or health box, a crate; a
// weapon; armour; a backpack; flesh: gibs and heads). Box3D's (vr_box3d.cpp) for its props, and the estimate below.
[[nodiscard]] float density(const qmodel_t* model, bool weaponLike);

// Stone's density (kg/m^3) for the rocks and bricks lying about (vr_debris.cpp: progs/vr_rock*.mdl, vr_brick*.mdl), 0 for
// anything else. Their mass is left estimated (no Mass setting): a bigger piece weighs more.
[[nodiscard]] float stoneDensity(const qmodel_t* model);

// A model's mass (kg) estimated from its drawn box (units, its size) and what it is made of, when neither Box3D (a
// listen server's body) nor a Mass setting says: its box's volume less what a box has round a rounded shape.
[[nodiscard]] float estimateMass(const qmodel_t* model, const glm::vec3& boxSize);

// How much of the hand's throw a prop of `mass` kg keeps: its Throw setting, else all of it up to
// vr_weight_throw_mass kg, less beyond (by the square root: a thing twice as heavy leaves at 71% the speed).
[[nodiscard]] float throwScale(int slot, float mass);

} // namespace qvr::props
