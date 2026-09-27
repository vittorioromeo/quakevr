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

// Forgets the models' slots found (a game directory change reuses their slots).
void resetCaches();
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

// The weapon's hotspots (round 21): where the other hand may hold it. A grip's point p is drawn at
//   R_EntityMatrix(weapon) * [mirror] * S(k) * T(Offset + (0, 0, vr_gunmodely)) * p
// (the weapon's model space, as its vertices are drawn, before its Scale).
inline constexpr int maxHotspots = 4;
enum class HotspotType : int
{
    None = 0,
    Grip = 1,  // a point the other hand holds (a foregrip, a pump, a magazine): it aims the weapon with the holding hand
    Blade = 2, // the half-sword grip along the blade
    Cup = 3    // a two-handed pistol grip: the other hand under and round the holding hand's grip; no two-handed aim
};
enum class HotspotStyle : int
{
    Wrap = 0,     // the fingers and the thumb wrap round it
    ThumbTop = 1  // the thumb along the top
};
struct Hotspot
{
    HotspotType type{HotspotType::None};
    glm::vec3 pos{0.f};    // a grip's point; a blade's: x the share of the way from the hand to the tip
    float bias{0.f};       // units off the distance it is picked by
    glm::vec3 angles{0.f}; // the helping hand's turn there (pitch, yaw, roll: degrees)
    HotspotStyle style{HotspotStyle::Wrap};
};
// field: 0 type, 1..3 x y z, 4 bias, 5..7 pitch yaw roll, 8 style
[[nodiscard]] Key hotspotKey(int index, int field);
[[nodiscard]] bool isGripType(HotspotType type); // a point the other hand holds: Grip or Cup
[[nodiscard]] Hotspot hotspot(int slot, int index);
void setHotspot(int slot, int index, const Hotspot& h);

// Round 21's migration of a config's two-handed grips (tuned in it: its foregrip or blade grip keys, or the weapon's
// offset or scale, not the defaults) into hotspots needs the model: true once per such slot, for the view to do it.
[[nodiscard]] bool takeHotspotMigration(int slot);

// Keys retired in round 21 (fitted hands: the hand's place and its fingers on the weapon): registered, unused.
[[nodiscard]] bool retired(Key key);


} // namespace qvr::weapons
