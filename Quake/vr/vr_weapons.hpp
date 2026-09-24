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

[[nodiscard]] float value(int slot, Key key);
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

} // namespace qvr::weapons
