// vr_weapons.hpp -- per-weapon settings (vr_wofs_<key>_NN cvars) and per-model transforms.

#pragma once

#include "vr_engine.hpp"

namespace qvr::weapons
{

inline constexpr int numSlots = 32;

enum class Key : int
{
#define QVR_WEAPON_KEY(e, k) e,
#include "vr_weapons.inc"
#undef QVR_WEAPON_KEY
    Count
};

void registerCvars();

// Slot whose vr_wofs_id_NN names `model`, or -1.
[[nodiscard]] int slotForModel(const qmodel_t* model);

// The slot holding progs/hand.mdl: the empty hand, whose settings also place and scale the
// hand and finger models.
[[nodiscard]] int fistSlot();

// The weapon model a hand holds (0 off hand, 1 main hand), from the VR stats, and its slot.
[[nodiscard]] qmodel_t* heldModel(int hand);
[[nodiscard]] int heldSlot(int hand);

[[nodiscard]] float value(int slot, Key key);

// For the Weapon Offsets menu page: a slot's cvar for a key, its settings back to their defaults, and
// its settings printed as vr_weapons.inc lines (to make them the shipped defaults).
[[nodiscard]] cvar_t* cvar(int slot, Key key);
void resetSlotToDefaults(int slot);
void printSlot(int slot);
[[nodiscard]] glm::vec3 vec(int slot, Key x, Key y, Key z);

// Extra transform of an alias model, in the model's own space: Ironwail's
//   T(scale_origin) * S(scale)
// becomes
//   S(k) * T(offset) * T(scale_origin) * S(scale) * S(scale2)
// (only when connected to a Quake VR server) which reproduces the old engine's in-place aliashdr_t rescaling (hdr->scale = scale * s * k,
// hdr->scale_origin = (scale_origin + offset) * k) without touching the model cache.
struct ModelTransform
{
    bool active{false};
    float k{1.f};
    glm::vec3 offset{0.f};
    glm::vec3 scale{1.f};
};

[[nodiscard]] ModelTransform modelTransform(const qmodel_t* model);

// The weapon and hand models' scale (ModelTransform::k) relative to the defaults the offsets were
// tuned at (vr_world_scale 1.25, vr_gunmodelscale 0.7): offsets between the models (fingers, the
// hand on a weapon, muzzles, foregrips) scale with it, so they stay attached at any world scale.
[[nodiscard]] float offsetScale();

} // namespace qvr::weapons
