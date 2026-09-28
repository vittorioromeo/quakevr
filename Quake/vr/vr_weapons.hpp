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
// The slot whose ID is the model named `name` (-1: none), and a key by its name in the cvars ("w_mass"; Key::Count:
// none): the QC's weaponvalue builtin (vr_builtins.cpp).
[[nodiscard]] int slotForName(const char* name);
[[nodiscard]] Key keyByName(const char* name);

// The slot holding progs/hand.mdl: the empty hand, whose settings also place and scale the
// hand and finger models.
[[nodiscard]] int fistSlot();

// The weapon model a hand holds (0 off hand, 1 main hand), from the VR stats, and its slot.
[[nodiscard]] qmodel_t* heldModel(int hand);
[[nodiscard]] int heldSlot(int hand);

[[nodiscard]] float value(int slot, Key key);

// A first start (no saved config): its settings are the defaults, of this version (no migration to run on them).
void markCurrent();

// For the Weapon Offsets and Weapon Weights menu pages: a slot's cvar for a key, its settings back to their defaults,
// and its settings printed as vr_weapons.inc lines (to make them the shipped defaults). Each page resets and prints its
// own (Part): Weapon Weights the weight's keys (weightKey), Weapon Offsets the others.
enum class Part
{
    All,
    Offsets,
    Weights,
};
[[nodiscard]] bool weightKey(Key key); // Mass, Balance, Length (Span), the spring's multipliers, the damage multipliers
[[nodiscard]] cvar_t* cvar(int slot, Key key);
void resetSlotToDefaults(int slot, Part part = Part::All);

// Forgets the models' slots found (a game directory change reuses their slots).
void resetCaches();
void printSlot(int slot, Part part = Part::All);
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

// Round 21, third pass: the overlap sliders (GripOverlap, a hotspot's) are shares of this many centimetres, the depth
// the fingers and palm may sink into a weapon; their default (today's snug fit, 0.3 cm).
inline constexpr float maxOverlapCm = 1.f;
inline constexpr float defaultOverlap = 0.3f;

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
    // Round 21, third pass: how far the hand holding it may sink into the weapon (0..1: 0 none, 1 maxOverlapCm), and
    // that hand's drawn pose moved (x y z) and turned (pitch yaw roll) once it holds it: visual only.
    float overlap{defaultOverlap};
    glm::vec3 visualPos{0.f};
    glm::vec3 visualAngles{0.f};
    // The fingers set by hand there (no solve): each finger's curl (thumb, index, middle, ring, little: 0 open .. 1
    // closed) and the thumb across the palm (0 .. 1).
    bool manual{false};
    float curl[5]{0.f, 0.f, 0.f, 0.f, 0.f};
    float thumbAcross{0.f};
};
// field: 0 type, 1..3 x y z, 4 bias, 5..7 pitch yaw roll, 8 style, 9 overlap, 10..12 visual x y z, 13..15 visual pitch
// yaw roll, 16 manual, 17..21 the fingers' curls, 22 the thumb across
inline constexpr int hotspotFields = 23;
[[nodiscard]] Key hotspotKey(int index, int field);
[[nodiscard]] bool isGripType(HotspotType type); // a point the other hand holds: Grip or Cup
[[nodiscard]] Hotspot hotspot(int slot, int index);
void setHotspot(int slot, int index, const Hotspot& h);

// Round 21's migration of a config's two-handed grips (tuned in it: its foregrip or blade grip keys, or the weapon's
// offset or scale, not the defaults) into hotspots needs the model: true once per such slot, for the view to do it.
[[nodiscard]] bool takeHotspotMigration(int slot);

// Round 21, third pass: a cup hotspot is where the helping hand's palm sits (it was a point the hand's grip channel
// was aligned round, often far from where the hand ended up). A config's cups made before are moved, once, to where
// their hands were drawn: the slots owning such a cup (their own values, not inherited), for the view to do it (it needs
// the hands drawn); `done` once it has.
[[nodiscard]] bool cupMigrationPending(int slot);
void cupMigrationDone(int slot);

// The slot whose own value of `key` `slot` uses (itself, or what it inherits it from).
[[nodiscard]] int ownerSlot(int slot, Key key);

// Round 21, second pass: the slot whose settings `slot` inherits (InheritFrom; -1 none), whether a key is inherited
// (all but the model's name, InheritFrom and the models' vertex indices), and every inherited value made the slot's
// own, inheriting no more.
[[nodiscard]] int inheritsFrom(int slot);
[[nodiscard]] bool inheritable(Key key);
void stopInheriting(int slot);

// The direction a weapon's shots go (its projectiles and beams too): the hand's aim `aimRot` (Quake angles) turned by the
// slot's Shot Pitch (up) and Shot Yaw (left), in the aim's own frame, the yaw mirrored for a weapon held `mirrored` (the
// off hand). `aimRot` itself for no slot or the empty hand, or with both 0. Nothing drawn moves with it.
[[nodiscard]] glm::vec3 shotAngles(const glm::vec3& aimRot, int slot, bool mirrored);

// The weapon as it sits in a holster, per kind of holster (the HipHolster*, UpperHolster*, ShoulderHolster* keys): moved
// (units: x off the body, y outwards, z up) and turned about its grip (pitch, yaw, roll: degrees), in the holster's frame
// after the holster's own turn; the view mirrors y, yaw and roll for the left holsters. field 0..5: x y z pitch yaw roll.
enum class HolsterKind : int
{
    Hip = 0,
    Upper = 1,
    Shoulder = 2
};
inline constexpr int holsterKinds = 3;
inline constexpr int holsteredFields = 6;
struct HolsteredPose
{
    glm::vec3 offset{0.f};
    glm::vec3 angles{0.f}; // pitch, yaw, roll
};
[[nodiscard]] Key holsteredKey(HolsterKind kind, int field);
[[nodiscard]] HolsteredPose holsteredPose(int slot, HolsterKind kind); // zero for no slot

// Keys retired in round 21 (fitted hands: the hand's place and its fingers on the weapon): registered, unused.
[[nodiscard]] bool retired(Key key);


} // namespace qvr::weapons
