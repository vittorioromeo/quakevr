// vr_props.hpp -- per-prop settings (vr_props.inc): what each kind of thing a hand carries weighs and how it is held,
// keyed by the model it is drawn with (a box's "maps/b_explob.bsp", a gib's "progs/gib1.mdl"). Stored as the weapons'
// are (cvars vr_prop_<key>_NN, archived; vr_props_version for changes to the shipped defaults) and edited on the
// Held Object Offsets and Held Object Weights pages (vr_menu_props.inc). See docs/vr-port/ROUND21.md, "Weight: spring
// model, stamina, held object offsets; explosive boxes" and "Spring only; Weapon Weights and Held Object Weights".
//
// Who reads what:
// - the hands' weight (vr_weight.cpp, client): Mass, Inertia, Com* and the Spring* multipliers of what each hand holds;
// - Box3D (vr_box3d.cpp, server): a Mass set here is the body's mass (its density scaled to it);
// - QC (the propvalue and propgrip builtins, vr_carry.qc, vr_melee.qc): Throw, GripMode and Grip*, TwoHands, ForceGrab,
//   Tip* and Butt*, MeleeDamage and ThrowDamage;
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
[[nodiscard]] int slotsUsed(); // the slots holding a model's settings (vr_limits)

// The slot of the model named `model` (-1: none; its keys are then their defaults).
[[nodiscard]] int slotForModel(const char* model);
// The same for a model, found once per model (by its pointer: resetModelCache at each map and model reload).
[[nodiscard]] int slotForModel(const qmodel_t* model);
void resetModelCache();
// Counts the changes to any prop setting (vr_prop_*): what was made from them (Box3D's props' mass) is looked at again
// only when it changed.
[[nodiscard]] unsigned settingsGeneration();

// A key of a slot (slot -1: the key's default).
[[nodiscard]] float value(int slot, Key key);
[[nodiscard]] float valueFor(const char* model, Key key);
[[nodiscard]] float valueFor(const qmodel_t* model, Key key);
[[nodiscard]] cvar_t* cvar(int slot, Key key);

// A prop's Size (round 21, "Prop size"): its model drawn, its Box3D body and its drawn box scaled about its origin by
// this, and its lengths (lengthKey) with it. size: the slot's (1 for none), within 0.05..10. drawnSize: the model's, as
// everything drawn or made from its drawn shape takes it (vr_render.cpp, vr_held.cpp's DrawnTransform, vr_box3d.cpp):
// 1 unless on Quake VR's protocol (as weapons::modelTransform), or not a brush or alias model.
[[nodiscard]] float size(int slot);
[[nodiscard]] float drawnSize(const qmodel_t* model);
// Centre of Mass, Tip, Butt, Handle From and To: lengths in its model's axes, which grow with its Size (scaledValue).
[[nodiscard]] bool lengthKey(Key key);
[[nodiscard]] float scaledValue(int slot, Key key); // value(), times size(slot) for a lengthKey
[[nodiscard]] const char* keyName(Key key); // as in its cvar and propvalue's argument ("mass")
[[nodiscard]] Key keyByName(const char* name); // Key::Count if none

// The model's slot, a free one given to it if it has none (-1: the table is full). The menu edits a prop through it.
[[nodiscard]] int claimSlot(const char* model);
// Each menu page resets and prints its own (Part): Held Object Weights the weight's keys (weightKey), Held Object Offsets
// the others.
enum class Part
{
    All,
    Offsets,
    Weights,
};
[[nodiscard]] bool weightKey(Key key); // Mass, Inertia, Com*, Throw, the spring's multipliers, the damage multipliers
void resetSlotToDefaults(int slot, Part part = Part::All);
// A config saved before `slot` was given a shipped prop: the slot takes its shipped settings (a model the menu had put
// there moves to a free slot with its settings). vr_cfg_version's migrations (vr_cvars.cpp).
void takeShippedSlot(int slot);
inline constexpr int wallTorchSlot = 16; // progs/vrtorch.mdl (vr_props.inc; vr_cfg_version 25)
inline constexpr int fleshSlotsFirst = 33, fleshSlotsLast = 47; // gibs and heads (vr_props.inc; vr_cfg_version 27)
void printSlot(int slot, Part part = Part::All); // the settings that differ from the defaults, as vr_props.inc lines

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
