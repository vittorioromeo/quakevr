// vr_weapons.cpp -- see vr_weapons.hpp.

#include "vr_weapons.hpp"
#include "vr_engine.hpp"
#include "vr_cvars.hpp"
#include "vr_hands.hpp"
#include "vr_protocol.hpp"

#include <array>
#include <cmath>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace qvr::weapons
{
namespace
{

constexpr int numKeys = static_cast<int>(Key::Count);

constexpr const char* keyNames[numKeys] = {
#define QVR_WEAPON_KEY(e, k) k,
#include "vr_weapons.inc"
#undef QVR_WEAPON_KEY
};

constexpr const char* keyEnumNames[numKeys] = {
#define QVR_WEAPON_KEY(e, k) #e,
#include "vr_weapons.inc"
#undef QVR_WEAPON_KEY
};

// Cvar names must outlive the cvars.
std::array<std::string, numSlots * numKeys> names;
std::array<cvar_t, numSlots * numKeys> cvars{};

constexpr const char* retiredKeyNames[] = {
#define QVR_WEAPON_RETIRED(k) k,
#include "vr_weapons.inc"
#undef QVR_WEAPON_RETIRED
};
constexpr int numRetired = static_cast<int>(sizeof(retiredKeyNames) / sizeof(retiredKeyNames[0]));
std::array<std::string, numSlots * numRetired> retiredNames;
std::array<cvar_t, numSlots * numRetired> retiredCvars{};

[[nodiscard]] cvar_t& cvarAt(int slot, Key key)
{
    return cvars[slot * numKeys + static_cast<int>(key)];
}

// Model -> slot, and the fist's slot (-2: not looked up yet), found again whenever a
// vr_wofs_id_NN cvar changes.
std::unordered_map<const qmodel_t*, int> slotCache;
int fistCache = -2;

// Counts the changes to the settings (settingsGeneration).
unsigned generation = 1;

// Model -> its drawn transform (modelTransform), with what it was made from besides the model.
struct TransformMemo
{
    unsigned generation{0};
    float inputs[7]{};
    ModelTransform t;
};
std::unordered_map<const qmodel_t*, TransformMemo> transformCache;

void onIdChanged(cvar_t* /* var */)
{
    slotCache.clear();
    fistCache = -2;
    transformCache.clear();
    generation++;
}

// Any setting's change: what was made from the settings is looked at again (an ID's: the slots found again too).
void onChanged(cvar_t* var);

} // namespace

void resetCaches()
{
    onIdChanged(nullptr);
}

unsigned settingsGeneration()
{
    return generation;
}

namespace
{

[[nodiscard]] bool isHandPart(const char* name)
{
    // The palm and finger models, and the jointed hand drawn instead of them (vr_handrig.cpp): the fist slot's scale.
    return !strcmp(name, "progs/hand_base.mdl") || !strncmp(name, "progs/finger_", 13) || !strcmp(name, "progs/hand_rig.mdl");
}

// Configs archive every slot's settings, so a slot whose defaults change keeps a config's old
// values: vr_wofs_version says which of these changes a config has seen, and the slots are reset to
// their new defaults once. 1: slots 19 and 20 (the knights' swords; they were unused placeholders);
// 2: the same (the swords remade with grips, held as the axe); 3: the same (the grips centred on
// the blades, thicker); 4: the same (new hilts: crossguard, grip and pommel on the blades' axes);
// 5: the same (longer grips, held just under the crossguard, and two-handed: TwoHMode 3);
// 6: slot 5 (the super nailgun: its body re-triangulated for real grooves, which moved its muzzle
// and two-handed grip anchors in the vertex order); 7: slots 3 and 7 (the double shotgun and the
// rocket launcher remade with pistol grips: the hand on the new grip, the offsets following the
// models' new bounds; Misc/quakevr/improve_weapons.py); 8: slot 8 (the lightning gun: a pistol grip
// under the hand, the gun 2.5 model units higher over it, the offsets following the model's new
// bounds; Misc/quakevr/improve_weapons2.py); 9: slots 2, 6, 10, 11 and 14 (the shotgun, the grenade
// launcher, the laser cannon, the proximity gun and the multi-grenade launcher: pistol and spade
// grips with trigger guards; the three launchers sit higher and further forward over the hand,
// the offsets follow the models' new bounds; Misc/quakevr/improve_weapons3.py); 10: slots 12..16
// (the alternate models the secondary ammo switches to: the lava nailguns, the multi-grenade and
// multi-rocket launchers, the plasma gun, placed exactly as the normal guns, their grips, guards and
// grooves too; Misc/quakevr/improve_weapons_alt.py); 11: slots 2, 3 and 18 (the shotguns' muzzle
// flashes widen the models' bounds, the offsets follow them; the grappling hook's pistol grip:
// Misc/quakevr/improve_weapons.py and improve_weapons3.py); 12: slots 4 and 12 (the nailgun and the
// lava nailgun: the shotguns' pistol grip reaches below the models' old bounds, the offsets follow
// them; Misc/quakevr/improve_weapons2.py); 13: slots 2, 3, 6, 7, 8, 10, 11, 14, 15, 16 and 18 (the
// author's placements tuned in the headset, round 20: the alternates moved by the same amounts as
// their normal guns); 14: slots 1..3, 5..7, 9..11, 13..15, 17 (the author's placements after round 20); 15: none
// reset (round 21: the hand is where the controller is, the fingers wrap the weapon; the two-handed grips became
// hotspots: a config's own grips, or a weapon it moved or scaled, are turned into hotspots where they were:
// takeHotspotMigration). 16: the alternates inherit (InheritFrom). 17: a hotspot allows two-handed use. 18: none reset
// (round 21, third pass: a cup hotspot is the helping hand's palm: a config's cups are moved to where their hands were
// drawn, by the view: cupMigrationPending). 19: slots 0..3, 7 and 17 (the author's poses after the posing mode). 20: slots
// 0..3, 5..10 and 17..19 (the author's offsets, hotspots, two-handed aim and Hand and Weapon Together, 2026-09-28, set
// over his hand calibration, which ships with them: vr_cvars.cpp, config version 16). 21: slots 1..3 (the author's
// hotspots, second pass, 2026-09-28 afternoon: the shotgun's cup moved, a cup and a grip on the super shotgun, a cup on
// the nailgun). 22: the author's holstered poses (2026-09-29: 94 hip and chest Holstered values in 13 weapons) and the
// axe's thumb bias: only those keys are reset, each slot's other settings kept. A first start (no saved config) takes this version as it is: its settings are these defaults
// (markCurrent).
constexpr int settingsVersion = 22;

// Slots whose hotspots the view is to derive from the config's two-handed grip keys (round 21).
bool hotspotMigration[numSlots]{};
// Slots owning a cup hotspot made before round 21's third pass (a config's), to move to where its hand was drawn.
bool cupMigration[numSlots]{};

void resetSlot(int slot)
{
    for(int key = 0; key < numKeys; key++)
    {
        cvar_t& var = cvarAt(slot, static_cast<Key>(key));
        Cvar_SetQuick(&var, var.default_string);
    }
    // The defaults are in today's form: nothing of the config's left to turn into hotspots or cups.
    hotspotMigration[slot] = false;
    cupMigration[slot] = false;
}

// The configs are read before anything asks for a slot.
void migrate()
{
    if(vr_wofs_version.value >= settingsVersion)
    {
        return;
    }
    if(vr_wofs_version.value < 5)
    {
        resetSlot(18);
        resetSlot(19);
    }
    if(vr_wofs_version.value < 6)
    {
        resetSlot(4);
    }
    if(vr_wofs_version.value < 7)
    {
        resetSlot(2);
        resetSlot(6);
    }
    if(vr_wofs_version.value < 8)
    {
        resetSlot(7);
    }
    if(vr_wofs_version.value < 9)
    {
        resetSlot(1);
        resetSlot(5);
        resetSlot(9);
        resetSlot(10);
        resetSlot(13);
    }
    if(vr_wofs_version.value < 10)
    {
        for(int slot = 11; slot <= 15; slot++)
        {
            resetSlot(slot);
        }
    }
    if(vr_wofs_version.value < 11)
    {
        resetSlot(1);
        resetSlot(2);
        resetSlot(17);
    }
    if(vr_wofs_version.value < 12)
    {
        resetSlot(3);
        resetSlot(11);
    }
    if(vr_wofs_version.value < 13)
    {
        for(const int slot : {1, 2, 5, 6, 7, 9, 10, 13, 14, 15, 17})
        {
            resetSlot(slot);
        }
    }
    if(vr_wofs_version.value < 14) // the author's placements and finger settings after round 20
    {
        for(const int slot : {1, 2, 3, 5, 6, 7, 9, 10, 11, 13, 14, 15, 17})
        {
            resetSlot(slot);
        }
    }
    if(vr_wofs_version.value < 15)
    {
        constexpr Key tuned[] = {Key::TwoHDisplayMode, Key::TwoHHandAnchorVertex, Key::TwoHFixedOffsetX, Key::TwoHFixedOffsetY,
            Key::TwoHFixedOffsetZ, Key::TwoHBladeGrip, Key::OffsetX, Key::OffsetY, Key::OffsetZ, Key::Scale};
        for(int slot = 0; slot < numSlots; slot++)
        {
            for(const Key key : tuned)
            {
                const cvar_t& var = cvarAt(slot, key);
                if(strcmp(var.string, var.default_string) != 0)
                {
                    hotspotMigration[slot] = true;
                }
            }
        }
    }
    if(vr_wofs_version.value < 16)
    {
        // Round 21, second pass: the other ammo's models inherit their base's settings: their own (the same as their
        // base's, tuned twice) go back to their defaults, so that they inherit.
        for(int slot = 0; slot < numSlots; slot++)
        {
            if(Q_atoi(cvarAt(slot, Key::InheritFrom).default_string) <= 0)
            {
                continue;
            }
            for(int key = 0; key < numKeys; key++)
            {
                if(inheritable(static_cast<Key>(key)))
                {
                    cvar_t& var = cvarAt(slot, static_cast<Key>(key));
                    Cvar_SetQuick(&var, var.default_string);
                }
            }
        }
    }
    if(vr_wofs_version.value < 17)
    {
        // A weapon given a hotspot while its two-handed use was forbidden (the grappling hook, the axe, Mjolnir by
        // default) never let the other hand take it: a hotspot allows it (the menu now does so as it is given).
        for(int slot = 0; slot < numSlots; slot++)
        {
            cvar_t& mode = cvarAt(slot, Key::TwoHMode);
            if(static_cast<int>(mode.value) != 2) // WPN_2H_FORBIDDEN
            {
                continue;
            }
            for(int i = 0; i < maxHotspots; i++)
            {
                if(static_cast<int>(cvarAt(slot, hotspotKey(i, 0)).value) != 0)
                {
                    Cvar_SetValueQuick(&mode, 0.f);
                    break;
                }
            }
        }
    }
    if(vr_wofs_version.value < 18)
    {
        for(int slot = 0; slot < numSlots; slot++)
        {
            for(int i = 0; i < maxHotspots; i++)
            {
                if(static_cast<int>(cvarAt(slot, hotspotKey(i, 0)).value) == static_cast<int>(HotspotType::Cup))
                {
                    cupMigration[slot] = true;
                }
            }
        }
    }
    if(vr_wofs_version.value < 19) // the author's poses after the posing mode (axe, shotguns, nailgun, lightning gun, hook)
    {
        for(const int slot : {0, 1, 2, 3, 7, 17})
        {
            resetSlot(slot);
        }
    }
    if(vr_wofs_version.value < 20) // the author's offsets and hotspots, 2026-09-28
    {
        for(const int slot : {0, 1, 2, 3, 5, 6, 7, 8, 9, 10, 17, 18, 19})
        {
            resetSlot(slot);
        }
    }
    if(vr_wofs_version.value < 21) // the author's hotspots, second pass, 2026-09-28 afternoon
    {
        for(const int slot : {1, 2, 3})
        {
            resetSlot(slot);
        }
    }
    if(vr_wofs_version.value < 22) // the author's holstered poses, 2026-09-29: these keys only
    {
        struct Keys
        {
            int slot;
            std::vector<Key> keys;
        };
        const Keys changed[] = {
            {0, {Key::FingerThumbBias, Key::HipHolsterRoll, Key::HipHolsterY, Key::HipHolsterZ, Key::UpperHolsterRoll,
                     Key::UpperHolsterY, Key::UpperHolsterZ}},
            {1, {Key::HipHolsterRoll, Key::HipHolsterX, Key::HipHolsterY, Key::HipHolsterZ, Key::UpperHolsterRoll,
                     Key::UpperHolsterY, Key::UpperHolsterYaw}},
            {2, {Key::HipHolsterY, Key::HipHolsterZ, Key::UpperHolsterY, Key::UpperHolsterYaw, Key::UpperHolsterZ}},
            {3, {Key::HipHolsterY, Key::HipHolsterZ, Key::UpperHolsterPitch, Key::UpperHolsterX, Key::UpperHolsterY,
                     Key::UpperHolsterYaw, Key::UpperHolsterZ}},
            {4, {Key::HipHolsterX, Key::HipHolsterZ, Key::UpperHolsterPitch, Key::UpperHolsterRoll, Key::UpperHolsterY,
                     Key::UpperHolsterYaw, Key::UpperHolsterZ}},
            {5, {Key::HipHolsterX, Key::HipHolsterY, Key::HipHolsterYaw, Key::HipHolsterZ, Key::UpperHolsterPitch,
                     Key::UpperHolsterRoll, Key::UpperHolsterY, Key::UpperHolsterYaw, Key::UpperHolsterZ}},
            {6, {Key::HipHolsterX, Key::HipHolsterZ, Key::UpperHolsterRoll, Key::UpperHolsterY, Key::UpperHolsterYaw,
                     Key::UpperHolsterZ}},
            {7, {Key::HipHolsterZ, Key::UpperHolsterPitch, Key::UpperHolsterRoll, Key::UpperHolsterX,
                     Key::UpperHolsterY, Key::UpperHolsterYaw, Key::UpperHolsterZ}},
            {8, {Key::HipHolsterRoll, Key::HipHolsterY, Key::HipHolsterZ, Key::UpperHolsterRoll, Key::UpperHolsterY,
                     Key::UpperHolsterYaw, Key::UpperHolsterZ}},
            {10, {Key::HipHolsterX, Key::HipHolsterY, Key::HipHolsterYaw, Key::HipHolsterZ, Key::UpperHolsterPitch,
                     Key::UpperHolsterRoll, Key::UpperHolsterY, Key::UpperHolsterYaw, Key::UpperHolsterZ}},
            {17, {Key::HipHolsterY, Key::HipHolsterZ, Key::UpperHolsterY, Key::UpperHolsterZ}},
            {18, {Key::HipHolsterRoll, Key::HipHolsterX, Key::HipHolsterY, Key::HipHolsterZ, Key::UpperHolsterPitch,
                     Key::UpperHolsterRoll, Key::UpperHolsterX, Key::UpperHolsterY, Key::UpperHolsterYaw,
                     Key::UpperHolsterZ}},
            {19, {Key::HipHolsterRoll, Key::HipHolsterX, Key::HipHolsterY, Key::HipHolsterZ, Key::UpperHolsterPitch,
                     Key::UpperHolsterRoll, Key::UpperHolsterX, Key::UpperHolsterY, Key::UpperHolsterYaw,
                     Key::UpperHolsterZ}},
        };
        for(const Keys& k : changed)
        {
            for(const Key key : k.keys)
            {
                cvar_t& var = cvarAt(k.slot, key);
                Cvar_SetQuick(&var, var.default_string);
            }
        }
    }
    Cvar_SetValueQuick(&vr_wofs_version, settingsVersion);
}

// ModelTransform::k: the weapon and hand models' scale.
[[nodiscard]] float modelScale()
{
    // The first Quake VR releases used a 0.75 world scale; weapon settings are relative to it.
    return (vr_world_scale.value / 0.75f) * vr_gunmodelscale.value;
}

} // namespace

void registerCvars()
{
    for(int slot = 0; slot < numSlots; slot++)
    {
        for(int key = 0; key < numKeys; key++)
        {
            names[slot * numKeys + key] = va("vr_wofs_%s_%02d", keyNames[key], slot + 1);
            cvar_t& var = cvars[slot * numKeys + key];
            var.name = names[slot * numKeys + key].c_str();
            var.string = "0";
            var.flags = CVAR_ARCHIVE;
        }
    }

    // Round 21, third pass: the overlap sliders' default (weapons::defaultOverlap: the snug fit the global
    // vr_hand_fit_overlap gave before), every slot and hotspot.
    for(int slot = 0; slot < numSlots; slot++)
    {
        cvarAt(slot, Key::GripOverlap).string = "0.3";
        for(int i = 0; i < maxHotspots; i++)
        {
            cvarAt(slot, hotspotKey(i, 9)).string = "0.3";
        }
    }

    // The weapon's multipliers of the spring's and the damage's global settings: 1 (as the global ones are).
    for(int slot = 0; slot < numSlots; slot++)
    {
        for(const Key key : {Key::SpringStiffness, Key::SpringDamping, Key::SpringStrength, Key::SpringSag, Key::SpringSwing,
                Key::SpringTwoHanded, Key::SpringSnap, Key::MeleeDamage, Key::ThrowDamage})
        {
            cvarAt(slot, key).string = "1";
        }
    }

#define QVR_WEAPON_DEFAULT(slot, key, value) cvarAt(slot, Key::key).string = value;
#include "vr_weapons.inc"
#undef QVR_WEAPON_DEFAULT

    for(cvar_t& var : cvars)
    {
        Cvar_RegisterVariable(&var);
        Cvar_SetCallback(&var, onChanged);
    }

    // Retired keys (vr_weapons.inc): registered so that a config setting them loads silently; not saved, read by nothing.
    for(int slot = 0; slot < numSlots; slot++)
    {
        for(int key = 0; key < numRetired; key++)
        {
            retiredNames[slot * numRetired + key] = va("vr_wofs_%s_%02d", retiredKeyNames[key], slot + 1);
            cvar_t& var = retiredCvars[slot * numRetired + key];
            var.name = retiredNames[slot * numRetired + key].c_str();
            var.string = "0";
            var.flags = CVAR_NONE;
            Cvar_RegisterVariable(&var);
        }
    }

}

int slotsUsed()
{
    int n = 0;
    for(int slot = 0; slot < numSlots; slot++)
    {
        const char* id = cvarAt(slot, Key::ID).string;
        n += id[0] && strcmp(id, "-1") != 0;
    }
    return n;
}

cvar_t* cvar(int slot, Key key)
{
    return slot >= 0 && slot < numSlots ? &cvarAt(slot, key) : nullptr;
}

void markCurrent()
{
    if(vr_wofs_version.value < 22) // the author's holstered poses, 2026-09-29: these keys only
    {
        struct Keys
        {
            int slot;
            std::vector<Key> keys;
        };
        const Keys changed[] = {
            {0, {Key::FingerThumbBias, Key::HipHolsterRoll, Key::HipHolsterY, Key::HipHolsterZ, Key::UpperHolsterRoll,
                     Key::UpperHolsterY, Key::UpperHolsterZ}},
            {1, {Key::HipHolsterRoll, Key::HipHolsterX, Key::HipHolsterY, Key::HipHolsterZ, Key::UpperHolsterRoll,
                     Key::UpperHolsterY, Key::UpperHolsterYaw}},
            {2, {Key::HipHolsterY, Key::HipHolsterZ, Key::UpperHolsterY, Key::UpperHolsterYaw, Key::UpperHolsterZ}},
            {3, {Key::HipHolsterY, Key::HipHolsterZ, Key::UpperHolsterPitch, Key::UpperHolsterX, Key::UpperHolsterY,
                     Key::UpperHolsterYaw, Key::UpperHolsterZ}},
            {4, {Key::HipHolsterX, Key::HipHolsterZ, Key::UpperHolsterPitch, Key::UpperHolsterRoll, Key::UpperHolsterY,
                     Key::UpperHolsterYaw, Key::UpperHolsterZ}},
            {5, {Key::HipHolsterX, Key::HipHolsterY, Key::HipHolsterYaw, Key::HipHolsterZ, Key::UpperHolsterPitch,
                     Key::UpperHolsterRoll, Key::UpperHolsterY, Key::UpperHolsterYaw, Key::UpperHolsterZ}},
            {6, {Key::HipHolsterX, Key::HipHolsterZ, Key::UpperHolsterRoll, Key::UpperHolsterY, Key::UpperHolsterYaw,
                     Key::UpperHolsterZ}},
            {7, {Key::HipHolsterZ, Key::UpperHolsterPitch, Key::UpperHolsterRoll, Key::UpperHolsterX,
                     Key::UpperHolsterY, Key::UpperHolsterYaw, Key::UpperHolsterZ}},
            {8, {Key::HipHolsterRoll, Key::HipHolsterY, Key::HipHolsterZ, Key::UpperHolsterRoll, Key::UpperHolsterY,
                     Key::UpperHolsterYaw, Key::UpperHolsterZ}},
            {10, {Key::HipHolsterX, Key::HipHolsterY, Key::HipHolsterYaw, Key::HipHolsterZ, Key::UpperHolsterPitch,
                     Key::UpperHolsterRoll, Key::UpperHolsterY, Key::UpperHolsterYaw, Key::UpperHolsterZ}},
            {17, {Key::HipHolsterY, Key::HipHolsterZ, Key::UpperHolsterY, Key::UpperHolsterZ}},
            {18, {Key::HipHolsterRoll, Key::HipHolsterX, Key::HipHolsterY, Key::HipHolsterZ, Key::UpperHolsterPitch,
                     Key::UpperHolsterRoll, Key::UpperHolsterX, Key::UpperHolsterY, Key::UpperHolsterYaw,
                     Key::UpperHolsterZ}},
            {19, {Key::HipHolsterRoll, Key::HipHolsterX, Key::HipHolsterY, Key::HipHolsterZ, Key::UpperHolsterPitch,
                     Key::UpperHolsterRoll, Key::UpperHolsterX, Key::UpperHolsterY, Key::UpperHolsterYaw,
                     Key::UpperHolsterZ}},
        };
        for(const Keys& k : changed)
        {
            for(const Key key : k.keys)
            {
                cvar_t& var = cvarAt(k.slot, key);
                Cvar_SetQuick(&var, var.default_string);
            }
        }
    }
    Cvar_SetValueQuick(&vr_wofs_version, settingsVersion);
}

bool weightKey(Key key)
{
    switch(key)
    {
        case Key::Mass:
        case Key::Balance:
        case Key::Span:
        case Key::SpringStiffness:
        case Key::SpringDamping:
        case Key::SpringStrength:
        case Key::SpringSag:
        case Key::SpringSwing:
        case Key::SpringTwoHanded:
        case Key::SpringSnap:
        case Key::MeleeDamage:
        case Key::ThrowDamage: return true;
        default: return false;
    }
}

namespace
{
[[nodiscard]] bool inPart(Key key, Part part)
{
    return part == Part::All || (part == Part::Weights) == weightKey(key);
}
} // namespace

void resetSlotToDefaults(int slot, Part part)
{
    if(slot < 0 || slot >= numSlots)
    {
        return;
    }
    if(part == Part::All)
    {
        resetSlot(slot);
        return;
    }
    for(int key = 0; key < numKeys; key++)
    {
        if(inPart(static_cast<Key>(key), part))
        {
            cvar_t& var = cvarAt(slot, static_cast<Key>(key));
            Cvar_SetQuick(&var, var.default_string);
        }
    }
    if(part == Part::Offsets)
    {
        // The defaults are in today's form: nothing of the config's left to turn into hotspots or cups (resetSlot).
        hotspotMigration[slot] = false;
        cupMigration[slot] = false;
    }
}

// The slot's settings that differ from the shipped defaults, as vr_weapons.inc lines.
Key hotspotKey(int index, int field)
{
    if(field < 5)
    {
        return static_cast<Key>(static_cast<int>(Key::Hotspot1Type) + 5 * index + field);
    }
    if(field < 9)
    {
        return static_cast<Key>(static_cast<int>(Key::Hotspot1Pitch) + 4 * index + (field - 5));
    }
    if(field < 16)
    {
        return static_cast<Key>(static_cast<int>(Key::Hotspot1Overlap) + 7 * index + (field - 9));
    }
    return static_cast<Key>(static_cast<int>(Key::Hotspot1Manual) + 7 * index + (field - 16));
}

bool isGripType(HotspotType type)
{
    return type == HotspotType::Grip || type == HotspotType::Cup;
}

Hotspot hotspot(int slot, int index)
{
    Hotspot h;
    if(slot < 0 || slot >= numSlots || index < 0 || index >= maxHotspots)
    {
        return h;
    }
    const int type = static_cast<int>(value(slot, hotspotKey(index, 0)));
    h.type = type == 1 ? HotspotType::Grip : type == 2 ? HotspotType::Blade : type == 3 ? HotspotType::Cup : HotspotType::None;
    h.pos = vec(slot, hotspotKey(index, 1), hotspotKey(index, 2), hotspotKey(index, 3));
    h.bias = value(slot, hotspotKey(index, 4));
    h.angles = vec(slot, hotspotKey(index, 5), hotspotKey(index, 6), hotspotKey(index, 7));
    h.style = value(slot, hotspotKey(index, 8)) >= 0.5f ? HotspotStyle::ThumbTop : HotspotStyle::Wrap;
    h.overlap = value(slot, hotspotKey(index, 9));
    h.visualPos = vec(slot, hotspotKey(index, 10), hotspotKey(index, 11), hotspotKey(index, 12));
    h.visualAngles = vec(slot, hotspotKey(index, 13), hotspotKey(index, 14), hotspotKey(index, 15));
    h.manual = value(slot, hotspotKey(index, 16)) >= 0.5f;
    for(int f = 0; f < 5; f++)
    {
        h.curl[f] = value(slot, hotspotKey(index, 17 + f));
    }
    h.thumbAcross = value(slot, hotspotKey(index, 22));
    return h;
}

void setHotspot(int slot, int index, const Hotspot& h)
{
    if(slot < 0 || slot >= numSlots || index < 0 || index >= maxHotspots)
    {
        return;
    }
    Cvar_SetValueQuick(&cvarAt(slot, hotspotKey(index, 0)), static_cast<float>(static_cast<int>(h.type)));
    for(int k = 0; k < 3; k++)
    {
        Cvar_SetValueQuick(&cvarAt(slot, hotspotKey(index, 1 + k)), h.pos[k]);
    }
    Cvar_SetValueQuick(&cvarAt(slot, hotspotKey(index, 4)), h.bias);
    for(int k = 0; k < 3; k++)
    {
        Cvar_SetValueQuick(&cvarAt(slot, hotspotKey(index, 5 + k)), h.angles[k]);
    }
    Cvar_SetValueQuick(&cvarAt(slot, hotspotKey(index, 8)), static_cast<float>(static_cast<int>(h.style)));
    Cvar_SetValueQuick(&cvarAt(slot, hotspotKey(index, 9)), h.overlap);
    for(int k = 0; k < 3; k++)
    {
        Cvar_SetValueQuick(&cvarAt(slot, hotspotKey(index, 10 + k)), h.visualPos[k]);
        Cvar_SetValueQuick(&cvarAt(slot, hotspotKey(index, 13 + k)), h.visualAngles[k]);
    }
    Cvar_SetValueQuick(&cvarAt(slot, hotspotKey(index, 16)), h.manual ? 1.f : 0.f);
    for(int f = 0; f < 5; f++)
    {
        Cvar_SetValueQuick(&cvarAt(slot, hotspotKey(index, 17 + f)), h.curl[f]);
    }
    Cvar_SetValueQuick(&cvarAt(slot, hotspotKey(index, 22)), h.thumbAcross);
}

bool takeHotspotMigration(int slot)
{
    if(slot < 0 || slot >= numSlots || !hotspotMigration[slot])
    {
        return false;
    }
    hotspotMigration[slot] = false;
    return true;
}

bool cupMigrationPending(int slot)
{
    return slot >= 0 && slot < numSlots && cupMigration[slot];
}

void cupMigrationDone(int slot)
{
    if(slot >= 0 && slot < numSlots)
    {
        cupMigration[slot] = false;
    }
}

int ownerSlot(int slot, Key key)
{
    for(int depth = 0; depth < 4 && slot >= 0 && inheritable(key); depth++)
    {
        const cvar_t& own = cvarAt(slot, key);
        const int from = inheritsFrom(slot);
        if(from < 0 || strcmp(own.string, own.default_string) != 0)
        {
            break;
        }
        slot = from;
    }
    return slot;
}

bool retired(Key key)
{
    switch(key)
    {
        case Key::HandAnchorVertex:
        case Key::HandOffsetX:
        case Key::HandOffsetY:
        case Key::HandOffsetZ:
        case Key::GunOffsetX:
        case Key::GunOffsetY:
        case Key::GunOffsetZ:
        case Key::Length:
        case Key::TwoHFixedMainHandOffsetX:
        case Key::TwoHFixedMainHandOffsetY:
        case Key::TwoHFixedMainHandOffsetZ:
        case Key::FingersX:
        case Key::FingersY:
        case Key::FingersZ:
        case Key::FingerOpen:
        case Key::FingerThumbOpen:
        case Key::FingerIndexOpen:
        case Key::FingerMiddleOpen:
        case Key::FingerRingOpen:
        case Key::FingerPinkyOpen:
        case Key::TwoHFingerOpen:
        case Key::TwoHFingerThumbOpen:
        case Key::TwoHDisplayMode: // the two-handed grips: hotspots now
        case Key::TwoHHandAnchorVertex:
        case Key::TwoHFixedOffsetX:
        case Key::TwoHFixedOffsetY:
        case Key::TwoHFixedOffsetZ:
        case Key::TwoHBladeGrip: return true;
        default: return false;
    }
}

glm::vec3 shotAngles(const glm::vec3& aimRot, int slot, bool mirrored)
{
    if(slot < 0 || slot >= numSlots || slot == fistSlot())
    {
        return aimRot;
    }
    float pitch = value(slot, Key::ShotPitch);
    float yaw = value(slot, Key::ShotYaw) * (mirrored ? -1.f : 1.f);
    pitch = std::isfinite(pitch) ? pitch : 0.f;
    yaw = std::isfinite(yaw) ? yaw : 0.f;
    if(pitch == 0.f && yaw == 0.f)
    {
        return aimRot;
    }
    // In the aim's frame (x forward, y left, z up): pitched up by `pitch`, then turned left by `yaw` about the aim's up.
    const float p = glm::radians(pitch), y = glm::radians(yaw);
    const glm::vec3 localFwd{std::cos(p) * std::cos(y), std::cos(p) * std::sin(y), std::sin(p)};
    const glm::vec3 localUp{-std::sin(p) * std::cos(y), -std::sin(p) * std::sin(y), std::cos(p)};
    glm::vec3 f, r, u;
    hands::angleVectors(aimRot, f, r, u);
    const auto toWorld = [&](const glm::vec3& l) { return f * l.x - r * l.y + u * l.z; };
    return hands::anglesFromVectors(glm::normalize(toWorld(localFwd)), glm::normalize(toWorld(localUp)));
}

Key holsteredKey(HolsterKind kind, int field)
{
    static_assert(static_cast<int>(Key::ShoulderHolsterRoll) ==
                  static_cast<int>(Key::HipHolsterX) + holsterKinds * holsteredFields - 1);
    return static_cast<Key>(static_cast<int>(Key::HipHolsterX) + holsteredFields * static_cast<int>(kind) + field);
}

HolsteredPose holsteredPose(int slot, HolsterKind kind)
{
    if(slot < 0)
    {
        return {};
    }
    return {vec(slot, holsteredKey(kind, 0), holsteredKey(kind, 1), holsteredKey(kind, 2)),
        vec(slot, holsteredKey(kind, 3), holsteredKey(kind, 4), holsteredKey(kind, 5))};
}

void printSlot(int slot, Part part)
{
    if(slot < 0 || slot >= numSlots)
    {
        return;
    }
    Con_Printf("// %s (slot %d, cvars _%02d): the settings changed from the defaults\n", cvarAt(slot, Key::ID).string, slot,
        slot + 1);
    for(int key = 0; key < numKeys; key++)
    {
        const cvar_t& var = cvarAt(slot, static_cast<Key>(key));
        if(!retired(static_cast<Key>(key)) && inPart(static_cast<Key>(key), part) && strcmp(var.string, var.default_string))
        {
            Con_Printf("QVR_WEAPON_DEFAULT(%d, %s, \"%s\") // was %s\n", slot, keyEnumNames[key], var.string,
                var.default_string);
        }
    }
}

int slotForModel(const qmodel_t* model)
{
    if(!model)
    {
        return -1;
    }

    migrate();
    if(const auto it = slotCache.find(model); it != slotCache.end())
    {
        return it->second;
    }

    int found = -1;
    for(int slot = 0; slot < numSlots; slot++)
    {
        if(!strcmp(cvarAt(slot, Key::ID).string, model->name))
        {
            found = slot;
            break;
        }
    }

    slotCache.emplace(model, found);
    return found;
}

int slotForName(const char* name)
{
    if(!name || !name[0])
    {
        return -1;
    }
    migrate();
    for(int slot = 0; slot < numSlots; slot++)
    {
        if(!strcmp(cvarAt(slot, Key::ID).string, name))
        {
            return slot;
        }
    }
    return -1;
}

Key keyByName(const char* name)
{
    for(int key = 0; key < numKeys; key++)
    {
        if(!strcmp(keyNames[key], name))
        {
            return static_cast<Key>(key);
        }
    }
    return Key::Count;
}

qmodel_t* heldModel(int hand)
{
    const int index = hand == 1 ? cl.stats[STAT_WEAPON] : cl.stats[protocol::STAT_QVR_WEAPONMODEL2];
    return index > 0 && index < MAX_MODELS ? cl.model_precache[index] : nullptr;
}

int heldSlot(int hand)
{
    return slotForModel(heldModel(hand));
}

int fistSlot()
{
    if(fistCache != -2)
    {
        return fistCache;
    }

    fistCache = -1;
    for(int slot = 0; slot < numSlots; slot++)
    {
        if(!strcmp(cvarAt(slot, Key::ID).string, "progs/hand.mdl"))
        {
            fistCache = slot;
            break;
        }
    }
    return fistCache;
}

bool inheritable(Key key)
{
    switch(key)
    {
        case Key::ID:
        case Key::InheritFrom:
        case Key::HandAnchorVertex:
        case Key::MuzzleAnchorVertex:
        case Key::TwoHHandAnchorVertex:
        case Key::WpnTextAnchorVertex:
        case Key::WpnButtonAnchorVertex: return false;
        default: return !retired(key);
    }
}

int inheritsFrom(int slot)
{
    if(slot < 0 || slot >= numSlots)
    {
        return -1;
    }
    const int from = static_cast<int>(cvarAt(slot, Key::InheritFrom).value) - 1;
    return from >= 0 && from < numSlots && from != slot && cvarAt(from, Key::ID).string[0] && strcmp(cvarAt(from, Key::ID).string, "-1")
               ? from
               : -1;
}

float value(int slot, Key key)
{
    if(slot < 0)
    {
        return 0.f;
    }
    // Inherited (a chain of at most a few: no loops) where this slot's own is its default.
    for(int depth = 0; depth < 4 && inheritable(key); depth++)
    {
        const cvar_t& own = cvarAt(slot, key);
        const int from = inheritsFrom(slot);
        if(from < 0 || strcmp(own.string, own.default_string) != 0)
        {
            break;
        }
        slot = from;
    }
    return cvarAt(slot, key).value;
}

void stopInheriting(int slot)
{
    if(inheritsFrom(slot) < 0)
    {
        return;
    }
    // Every inherited value made this slot's own, then no parent.
    float values[numKeys];
    for(int key = 0; key < numKeys; key++)
    {
        values[key] = value(slot, static_cast<Key>(key));
    }
    for(int key = 0; key < numKeys; key++)
    {
        const cvar_t& own = cvarAt(slot, static_cast<Key>(key));
        if(inheritable(static_cast<Key>(key)) && !strcmp(own.string, own.default_string))
        {
            Cvar_SetValueQuick(&cvarAt(slot, static_cast<Key>(key)), values[key]);
        }
    }
    Cvar_SetValueQuick(&cvarAt(slot, Key::InheritFrom), 0.f);
}

glm::vec3 vec(int slot, Key x, Key y, Key z)
{
    return {value(slot, x), value(slot, y), value(slot, z)};
}

float offsetScale()
{
    return (vr_world_scale.value / 1.25f) * (vr_gunmodelscale.value / 0.7f);
}

namespace
{

void onChanged(cvar_t* var)
{
    generation++;
    const std::ptrdiff_t index = var - cvars.data();
    if(index >= 0 && index < static_cast<std::ptrdiff_t>(cvars.size()) && index % numKeys == static_cast<int>(Key::ID))
    {
        onIdChanged(var);
    }
}

[[nodiscard]] ModelTransform makeModelTransform(const qmodel_t* model)
{
    ModelTransform t;

    t.k = modelScale();

    const char* name = model->name;
    if(!strcmp(name, "progs/legholster.mdl"))
    {
        t.active = true;
        t.scale = glm::vec3{vr_leg_holster_model_scale.value};
        t.offset = {vr_leg_holster_model_x_offset.value, vr_leg_holster_model_y_offset.value,
            vr_leg_holster_model_z_offset.value};
        return t;
    }

    const int slot = isHandPart(name) ? fistSlot() : slotForModel(model);
    if(slot < 0)
    {
        return t;
    }

    t.active = true;
    t.scale = glm::vec3{value(slot, Key::Scale)};
    t.offset = vec(slot, Key::OffsetX, Key::OffsetY, Key::OffsetZ) +
               glm::vec3{0.f, 0.f, vr_gunmodely.value};
    return t;
}

} // namespace

ModelTransform modelTransform(const qmodel_t* model)
{
    // Quake VR's protocol: the client's, or the local server's. The server's physics shapes (vr_box3d.cpp's hulls,
    // vr_rigid.cpp's boxes, vr_held.cpp's drawn surfaces) are made from these too, and a map's first frames run before
    // the client has connected (cl.protocolflags then still the last map's, or none): its thrown weapons' hulls were
    // made at the models' own size, three times the drawn guns', and kept.
    const bool quakevr = (cl.protocolflags & PRFL_QUAKEVR) || (sv.active && (sv.protocolflags & PRFL_QUAKEVR));
    if(!model || model->type != mod_alias || !quakevr)
    {
        return ModelTransform{};
    }
    // Made once per model (the name's slot, its settings), again when a setting it is made from changes.
    const float inputs[7] = {vr_world_scale.value, vr_gunmodelscale.value, vr_gunmodely.value, vr_leg_holster_model_scale.value,
        vr_leg_holster_model_x_offset.value, vr_leg_holster_model_y_offset.value, vr_leg_holster_model_z_offset.value};
    if(const auto it = transformCache.find(model); it != transformCache.end() && it->second.generation == generation &&
                                                   memcmp(it->second.inputs, inputs, sizeof(inputs)) == 0)
    {
        return it->second.t;
    }
    const ModelTransform t = makeModelTransform(model); // (may find the slots again: the cache emptied)
    TransformMemo& memo = transformCache[model];
    memo.generation = generation;
    memcpy(memo.inputs, inputs, sizeof(inputs));
    memo.t = t;
    return t;
}

} // namespace qvr::weapons
