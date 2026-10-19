// vr_weapons.cpp -- see vr_weapons.hpp.

#include "vr_modelmetadata.hpp"
#include "vr_weapons.hpp"
#include "vr_adguns.hpp"
#include "vr_anchor.hpp"
#include "vr_engine.hpp"
#include "vr_cvars.hpp"
#include "vr_hands.hpp"
#include "vr_protocol.hpp"

#include "Zancle/Base/IsFinite.hpp"
#include "Zancle/Base/PtrDiffT.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Array.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/Vocabulary/Span.hpp"
#include "vr_zancle.hpp"

#include <string.h>

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
za::Array<za::String, numSlots * numKeys> names;
za::Array<cvar_t, numSlots * numKeys> cvars{};


[[nodiscard]] cvar_t& cvarAt(int slot, Key key)
{
    return cvars[slot * numKeys + static_cast<int>(key)];
}

// An inheriting slot's defaults its own (vr_weapons.inc QVR_WEAPON_OWN): at its default such a key is still the slot's,
// never its parent's. Every other default of an inheriting slot is a placeholder that gives way to the parent's (an own
// value is one that differs from the default: ROUND21's design).
za::Array<bool, numSlots * numKeys> ownDefaults{};

// Whether `slot`'s value of `key` gives way to its parent's (the slot inherits it, the value is the default, and the
// default is not marked the slot's own).
[[nodiscard]] bool takesParents(int slot, Key key, int from)
{
    if(from < 0)
    {
        return false;
    }
    const cvar_t& own = cvarAt(slot, key);
    return !strcmp(own.string, own.default_string) && !ownDefaults[slot * numKeys + static_cast<int>(key)];
}

// Model -> slot, and the fist's slot (-2: not looked up yet), found again whenever a
// vr_wofs_id_NN cvar changes.
ankerl::unordered_dense::map<const qmodel_t*, int> slotCache;
// A part drawn on its gun (slotForPart) with no slot of its own -> its gun's slot, likewise.
ankerl::unordered_dense::map<const qmodel_t*, int> partSlotCache;
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
ankerl::unordered_dense::map<const qmodel_t*, TransformMemo> transformCache;

void onIdChanged(cvar_t* /* var */)
{
    slotCache.clear();
    partSlotCache.clear();
    fistCache = -2;
    transformCache.clear();
    generation++;
}

// Any setting's change: what was made from the settings is looked at again (an ID's: the slots found again too).
void onChanged(cvar_t* var);

// Whether a slot's ID names the model `name`: the same name, or (AD support) an AD soldier's gun's base name, whatever
// its skin (adguns::slotNames): one gun, one slot.
[[nodiscard]] bool namesModel(const char* id, const char* name)
{
    return !strcmp(id, name) || adguns::slotNames(id, name); // AD support (vr_adguns.cpp)
}

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

[[nodiscard]] bool isHandPart(const modelmeta::ModelMetadata& info)
{
    // The palm and finger models, and the jointed hand drawn instead of them (vr_handrig.cpp): the fist slot's scale.
    return info.is(modelmeta::Id::HandBase) || info.has(modelmeta::Trait::Finger) || info.is(modelmeta::Id::HandRig);
}

// Configs archive every slot's settings, so a slot whose defaults change keeps a config's old values: vr_wofs_version
// says which of these changes a config has seen (vr_props_version is the held objects'). The changes before 1.0.0
// (versions 1-40) were dropped with the configs older than 1.0.0 (vr_cvars.cpp resetOldConfig resets those). 41: the
// anchors (round 22: points on the models' surfaces, not vertex indices; vr_anchor.hpp): each of the config's indices to
// its slot's default, that vertex's point (convertAnchors). 42: the toolgun's muzzle (slot 26) on its lens's front middle
// (`15.4 0.0064 5.7941`, make_toolgun.py's point; the old one, a corner of the lens's back, was about 1 cm off), where
// the config still held the old point. 43: slots 27-31 (AD support: Arcane Dimensions' soldiers' guns, vr_adguns.cpp;
// unused placeholders before). 44: slot 33 (the AD crossbow) on its model turned to face +x (vr_adguns.cpp
// turnedModels): the sideways model's offsets would turn it wrong; his pose converted is its default. 45: slots 27-31 again (the AD guns now our burst rifle's
// variants, vr_adgunart.cpp: their IDs and anchors). 46: the AD hunter ogres' hammer and mace (slots 44-45) head-heavy
// (their centre of mass 35 and 30 cm ahead of the grip, was 15), where the config still held 15. 47: AD's Widowmaker and
// Plasma Gun (slots 34-35, vr_adweapons.cpp) remade: the Widowmaker's anchors, front handle hotspot and weight, the Plasma
// Gun's button off and its kick and flash: both slots to their defaults (their own values were the placeholders that
// inherit, and the old models' anchors). 48: no AD weapon inherits (round 22, adnoinherit): slots 27-31, 33-35 own what
// they inherited (their defaults the parents', vr_weapons.inc QVR_WEAPON_UNINHERITED; a config's inherited values made
// theirs, so a Shadow Axe tuned on the axe's slot keeps it); the plain axe back to its size before adweapons2 (where the
// config holds the Shadow Axe's size adweapons2 put there); the AD crossbow (slot 32) his. 49: the AD melee weapons'
// tips (slots 40-44) on their models made anew (vr_admelee.cpp: the fury's sword and the hammer turned half round),
// where the config still held the old tip. 50: the Widowmaker's grips his (slot 34: hotspot 1 a cup round the rear
// handle, hotspot 2 off; takeWidowmakerGrips). 51: the AD fury's sword's, hunter ogre's hammer's and mace's (slots 40,
// 44, 45) poses and grips his (takeAdMeleeTuning). A first start (no saved config) takes this version as it is: its
// settings are these defaults (markCurrent).
constexpr int settingsVersion = 51;

// The AD melee weapons' tips (slots 40-44) before 49.
constexpr const char* oldAdMeleeTips[] = {"41.5783 -0.0077 82.5216", "35.5648 -0.2178 93.7809",
    "34.8005 -0.1473 91.7639", "35.565 -0.2178 93.7812", "18.6015 -0.0368 92.3618"};

// AD support: the AD weapons that inherited another slot's settings until 48, and from which.
struct Uninherited
{
    int slot;
    int from;
};
constexpr Uninherited uninherited[] = {{27, 21}, {28, 21}, {29, 21}, {30, 21}, {31, 21}, {33, 0}, {34, 2}, {35, 7}};
constexpr int numUninherited = sizeof(uninherited) / sizeof(uninherited[0]);

// Their defaults before 48 (registerCvars: vr_weapons.inc without its QVR_WEAPON_UNINHERITED lines): a config's value
// that is still one of these was a placeholder that gave way to the parent's.
za::Array<const char*, numUninherited * numKeys> oldDefaults{};

// Their defaults that were their own then (vr_weapons.inc QVR_WEAPON_OWN until 48): never the parent's.
struct SlotKey
{
    int slot;
    Key key;
};
constexpr SlotKey ownBefore48[] = {{34, Key::Hotspot1Type}, {34, Key::Hotspot1X}, {34, Key::Hotspot1Y}, {34, Key::Hotspot1Z},
    {34, Key::Hotspot1Pitch}, {34, Key::Hotspot1Yaw}, {34, Key::Hotspot1Roll}, {34, Key::Mass}, {34, Key::Balance},
    {34, Key::Span}, {35, Key::WpnButtonMode}, {35, Key::Recoil}, {35, Key::RecoilStrength}, {35, Key::RecoilTime},
    {35, Key::Flash}, {35, Key::FlashSize}};

[[nodiscard]] bool ownBefore(int slot, Key key)
{
    for(const SlotKey& sk : ownBefore48)
    {
        if(sk.slot == slot && sk.key == key)
        {
            return true;
        }
    }
    return false;
}

// A slot's keys whose default changed, with the default before: a config still holding that one takes the new.
struct OldValue
{
    int slot;
    Key key;
    float value;
};
constexpr OldValue changedIn48[] = {
    // The plain axe: the Shadow Axe's size adweapons2 had put on its slot.
    {0, Key::Scale, 0.36f}, {0, Key::OffsetY, 0.84f}, {0, Key::OffsetZ, -0.6f},
    // The AD crossbow: the defaults before his tweaks.
    {32, Key::Pitch, 3.801594f}, {32, Key::Roll, 3.026701f}, {32, Key::Yaw, -7.244246f}, {32, Key::ShotPitch, 7.f},
    {32, Key::ShotYaw, 0.f}, {32, Key::Mass, 6.f}, {32, Key::Balance, 25.f}, {32, Key::RecoilStrength, 0.5f},
    {32, Key::RecoilTime, 0.15f}, {32, Key::Flash, 1.f}, {32, Key::StockPitch, 0.f}, {32, Key::TorchForward, 0.f}};

void resetSlot(int slot)
{
    for(int key = 0; key < numKeys; key++)
    {
        cvar_t& var = cvarAt(slot, static_cast<Key>(key));
        Cvar_SetQuick(&var, var.default_string);
    }
}

// Every anchor of the config that is not a point (vr_wofs_version < 41: a vertex index in the old strip order of the
// slot's model) to the slot's default, which is that vertex's point (the defaults were converted exactly: the same place
// in every pose). A player's own index (no menu sets one; it named a vertex of 1.0's model files) gets the default too.
void convertAnchors()
{
    int converted = 0;
    for(int slot = 0; slot < numSlots; slot++)
    {
        for(int k = 0; k < static_cast<int>(Key::Count); k++)
        {
            const Key key = static_cast<Key>(k);
            if(!anchorKey(key))
            {
                continue;
            }
            cvar_t& var = cvarAt(slot, key);
            if(!anchor::isAnchor(var.string))
            {
                Cvar_SetQuick(&var, var.default_string);
                converted++;
            }
        }
    }
    Con_DPrintf("Weapon settings: %d anchor indices replaced by their points\n", converted);
}

// The toolgun's muzzle on its lens's middle (42): where the config still held the old point.
void takeToolgunMuzzle()
{
    cvar_t& var = cvarAt(26, Key::MuzzleAnchor);
    if(!strcmp(var.string, "15.0245 -0.6162 5.3987"))
    {
        Cvar_SetQuick(&var, var.default_string);
    }
}

// AD support: the AD soldiers' guns (43; unused placeholders before; 45: made from our burst rifle, vr_adgunart.cpp).
void takeAdGuns()
{
    for(int slot = 27; slot <= 31; slot++)
    {
        resetSlot(slot);
    }
}

// AD support: the AD crossbow's slot (44) to its defaults, on the model turned to face +x.
void takeAdCrossbow()
{
    resetSlot(32);
}

// AD support: the hammer's and the mace's balance (46), where the config holds the old default.
void takeAdHeavyBalance()
{
    for(int slot = 44; slot <= 45; slot++)
    {
        cvar_t& var = cvarAt(slot, Key::Balance);
        if(var.value == 15.f)
        {
            Cvar_SetQuick(&var, var.default_string);
        }
    }
}

// AD support: the Widowmaker's and the Plasma Gun's slots (47) to their defaults, on the remade models.
void takeAdUpgrades()
{
    resetSlot(34);
    resetSlot(35);
}

// AD support (48): the AD weapons inherit no more. A slot still inheriting its old parent takes each value it took from
// it (the parent's, where its own was the old placeholder default), then inherits nothing; one the player made stop
// inheriting, or inherit another, keeps its values. Then the plain axe's and the crossbow's changed defaults, where the
// config holds the old ones. (The parents first: the Shadow Axe takes the axe's slot's size before the axe gets back its own.)
void takeUninherited()
{
    for(int i = 0; i < numUninherited; i++)
    {
        const Uninherited& u = uninherited[i];
        cvar_t& inherit = cvarAt(u.slot, Key::InheritFrom);
        if(static_cast<int>(inherit.value) - 1 != u.from)
        {
            continue;
        }
        int taken = 0;
        for(int k = 0; k < numKeys; k++)
        {
            const Key key = static_cast<Key>(k);
            cvar_t& var = cvarAt(u.slot, key);
            const char* old = oldDefaults[i * numKeys + k];
            if(!inheritable(key) || ownBefore(u.slot, key) || !old || strcmp(var.string, old) != 0)
            {
                continue;
            }
            Cvar_SetQuick(&var, cvarAt(ownerSlot(u.from, key), key).string);
            taken++;
        }
        Cvar_SetQuick(&inherit, inherit.default_string);
        Con_DPrintf("Weapon settings: slot %d took %d values it inherited from slot %d\n", u.slot, taken, u.from);
    }
    for(const OldValue& o : changedIn48)
    {
        cvar_t& var = cvarAt(o.slot, o.key);
        if(fabsf(var.value - o.value) < 1e-5f)
        {
            Cvar_SetQuick(&var, var.default_string);
        }
    }
}

// AD support: the AD melee weapons' tips (49) on their models made anew, where the config holds the old tip.
void takeAdMeleeTips()
{
    for(int slot = 40; slot <= 44; slot++)
    {
        cvar_t& var = cvarAt(slot, Key::MuzzleAnchor);
        if(!strcmp(var.string, oldAdMeleeTips[slot - 40]))
        {
            Cvar_SetQuick(&var, var.default_string);
        }
    }
}

// AD support: the Widowmaker's grips (50): his hotspot 1 (a cup round the rear handle) and hotspot 2 off, where the
// config still holds the defaults before (hotspot 1 the front handle's grip; hotspot 2 the super shotgun's cup, or the
// super shotgun's own hotspot 2 that 48 gave it). Hotspot 1 as a whole: only a config holding all of the old one.
constexpr OldValue widowmakerGrip1Before50[] = {{34, Key::Hotspot1Type, 1.f}, {34, Key::Hotspot1X, 6.78f},
    {34, Key::Hotspot1Y, -2.4406f}, {34, Key::Hotspot1Z, -0.6209f}, {34, Key::Hotspot1Pitch, 7.f}, {34, Key::Hotspot1Yaw, 0.f},
    {34, Key::Hotspot1Roll, 0.f}};

void takeWidowmakerGrips()
{
    bool old1 = true;
    for(const OldValue& o : widowmakerGrip1Before50)
    {
        old1 = old1 && fabsf(cvarAt(o.slot, o.key).value - o.value) < 1e-4f;
    }
    if(old1)
    {
        for(const OldValue& o : widowmakerGrip1Before50)
        {
            cvar_t& var = cvarAt(o.slot, o.key);
            Cvar_SetQuick(&var, var.default_string);
        }
    }
    cvar_t& hs2 = cvarAt(34, Key::Hotspot2Type);
    if(hs2.value == 3.f || hs2.value == cvarAt(2, Key::Hotspot2Type).value)
    {
        Cvar_SetQuick(&hs2, hs2.default_string);
    }
}

// AD support: the author's fury's sword, hunter ogre's hammer and mace (51; his config of 2026-10-19): their poses,
// grips, thumbs and fit, each value where the config still holds the default before.
constexpr OldValue adMeleeBefore51[] = {{40, Key::Hotspot1Pitch, 0.f}, {40, Key::Hotspot1Roll, 0.f},
    {40, Key::Hotspot1X, -3.2833f}, {40, Key::Hotspot1Y, 1.598f}, {40, Key::Hotspot1Yaw, 0.f}, {40, Key::Hotspot1Z, -8.6794f},
    {40, Key::Hotspot2Type, 2.f}, {40, Key::Hotspot2X, 0.7f}, {40, Key::Pitch, 0.f}, {40, Key::Roll, 0.f},
    {40, Key::OffsetX, 2.004f}, {40, Key::OffsetY, 2.253f}, {40, Key::Yaw, 0.79998f}, {40, Key::OffsetZ, 8.628f},
    {44, Key::FingerThumbBias, 0.f}, {44, Key::FingerThumbX, 0.f}, {44, Key::FingerThumbY, 0.f}, {44, Key::FingerThumbZ, 0.f},
    {44, Key::Hotspot1Overlap, 0.3f}, {44, Key::Hotspot1Pitch, 0.f}, {44, Key::Hotspot1Roll, 0.f},
    {44, Key::Hotspot1X, -5.1633f}, {44, Key::Hotspot1Y, 1.684f}, {44, Key::Hotspot1Yaw, 0.f}, {44, Key::Hotspot1Z, -13.6382f},
    {44, Key::Hotspot2Type, 2.f}, {44, Key::GripOverlap, 0.3f}, {44, Key::Pitch, 0.f}, {44, Key::Roll, 0.f},
    {44, Key::OffsetX, 2.004f}, {44, Key::OffsetY, 2.253f}, {44, Key::Yaw, 0.79998f}, {44, Key::OffsetZ, 8.628f},
    {45, Key::FingerThumbBias, 0.f}, {45, Key::FingerThumbY, 0.f}, {45, Key::Hotspot1Overlap, 0.3f},
    {45, Key::Hotspot1Pitch, 0.f}, {45, Key::Hotspot1Roll, 0.f}, {45, Key::Hotspot1X, -5.1633f}, {45, Key::Hotspot1Y, 1.684f},
    {45, Key::Hotspot1Yaw, 0.f}, {45, Key::Hotspot1Z, -13.6382f}, {45, Key::Hotspot2Type, 2.f}, {45, Key::GripOverlap, 0.3f},
    {45, Key::Pitch, 0.f}, {45, Key::Roll, 0.f}, {45, Key::OffsetX, 2.004f}, {45, Key::OffsetY, 2.253f},
    {45, Key::Yaw, 0.79998f}, {45, Key::OffsetZ, 8.628f}};

void takeAdMeleeTuning()
{
    int taken = 0;
    for(const OldValue& o : adMeleeBefore51)
    {
        cvar_t& var = cvarAt(o.slot, o.key);
        if(fabsf(var.value - o.value) < 1e-4f)
        {
            Cvar_SetQuick(&var, var.default_string);
            taken++;
        }
    }
    Con_DPrintf("Weapon settings: the AD melee weapons took %d of the author's values\n", taken);
}

// The changes since 1.0.0 (vr_wofs_version 40): each runs once, for a config saved before its version.
struct Step
{
    int version;
    void (*run)();
};
constexpr Step steps[] = {
    {41, convertAnchors},
    {42, takeToolgunMuzzle},
    {43, takeAdGuns},
    {44, takeAdCrossbow},
    {45, takeAdGuns}, // (the AD guns made from our burst rifle: their slots' IDs and anchors)
    {46, takeAdHeavyBalance},
    {47, takeAdUpgrades},
    {48, takeUninherited},
    {49, takeAdMeleeTips},
    {50, takeWidowmakerGrips},
    {51, takeAdMeleeTuning},
};
static_assert(steps[sizeof(steps) / sizeof(steps[0]) - 1].version == settingsVersion);

// The configs are read before anything asks for a slot.
void migrate()
{
    if(vr_wofs_version.value >= settingsVersion)
    {
        return;
    }
    for(const Step& step : steps)
    {
        if(vr_wofs_version.value < static_cast<float>(step.version))
        {
            step.run();
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
            var.name = names[slot * numKeys + key].cStr();
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
            cvarAt(slot, hotspotKey(i, 23)).string = "1"; // Stickiness: as every grip
        }
    }

    // The weapon's multipliers of the spring's and the damage's global settings: 1 (as the global ones are).
    for(int slot = 0; slot < numSlots; slot++)
    {
        for(const Key key : {Key::SpringStiffness, Key::SpringDamping, Key::SpringStrength, Key::SpringSag, Key::SpringSwing,
                Key::SpringTwoHanded, Key::SpringSnap, Key::SpringRoll, Key::MeleeDamage, Key::ThrowDamage, Key::SpinAlign, Key::ThrowMinSpeed,
                Key::ThrowCurve, Key::MeleeMinSpeed, Key::MeleeCurve})
        {
            cvarAt(slot, key).string = "1";
        }
    }

    // The weapon effects' (vr_weaponfx.cpp): off, at the shotgun's strength and size; the tracers as Weapon Effects'.
    for(int slot = 0; slot < numSlots; slot++)
    {
        for(const Key key : {Key::RecoilStrength, Key::FlashSize, Key::TracerSpeed, Key::TracerLength, Key::TracerWidth,
                Key::TracerChance, Key::TracerRed})
        {
            cvarAt(slot, key).string = "1";
        }
        cvarAt(slot, Key::RecoilTime).string = "0.15";
        cvarAt(slot, Key::FlashTime).string = "0.06";
        cvarAt(slot, Key::TracerGreen).string = "0.8";
        cvarAt(slot, Key::TracerBlue).string = "0.45";
    }

    // The other hand anywhere on it: as the global setting (vr_weapon_anygrip_mode).
    for(int slot = 0; slot < numSlots; slot++)
    {
        cvarAt(slot, Key::AnyGripMode).string = "-1";
        cvarAt(slot, Key::TorchClip).string = "1"; // the flashlight clips on it (the melee weapons' 0: vr_weapons.inc)
    }

#define QVR_WEAPON_DEFAULT(slot, key, value) cvarAt(slot, Key::key).string = value;
#define QVR_WEAPON_UNINHERITED(slot, key, value)
#define QVR_WEAPON_OWN(slot, key) ownDefaults[(slot) * numKeys + static_cast<int>(Key::key)] = true;
#include "vr_weapons.inc"
#undef QVR_WEAPON_OWN
#undef QVR_WEAPON_UNINHERITED
#undef QVR_WEAPON_DEFAULT

    // The AD weapons' defaults before vr_wofs_version 48 (takeUninherited), then the values they inherited until then,
    // their own defaults now (the second pass reads only those).
    for(int i = 0; i < numUninherited; i++)
    {
        for(int key = 0; key < numKeys; key++)
        {
            oldDefaults[i * numKeys + key] = cvarAt(uninherited[i].slot, static_cast<Key>(key)).string;
        }
    }
#define QVR_WEAPON_DEFAULT(slot, key, value)
#define QVR_WEAPON_UNINHERITED(slot, key, value) cvarAt(slot, Key::key).string = value;
#include "vr_weapons.inc"
#undef QVR_WEAPON_UNINHERITED
#undef QVR_WEAPON_DEFAULT

    for(cvar_t& var : cvars)
    {
        Cvar_RegisterVariable(&var);
        Cvar_SetCallback(&var, onChanged);
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

bool anchorKey(Key key)
{
    return key == Key::HandAnchor || key == Key::MuzzleAnchor || key == Key::TwoHHandAnchor || key == Key::WpnButtonAnchor ||
           key == Key::WpnTextAnchor;
}

const char* anchor(int slot, Key key)
{
    const cvar_t* var = anchorKey(key) ? cvar(slot, key) : nullptr;
    return var ? var->string : "";
}

void markCurrent()
{
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
        case Key::SpringRoll:
        case Key::MeleeDamage:
        case Key::ThrowDamage:
        case Key::SpinAlign:
        case Key::ThrowMinSpeed:
        case Key::ThrowCurve:
        case Key::MeleeMinSpeed:
        case Key::MeleeCurve: return true;
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
    if(field < 23)
    {
        return static_cast<Key>(static_cast<int>(Key::Hotspot1Manual) + 7 * index + (field - 16));
    }
    return static_cast<Key>(static_cast<int>(Key::Hotspot1Sticky) + index);
}

float bladeFrom(const Hotspot& h)
{
    return za::max(0.3f, h.pos.x - 0.3f);
}

float bladeTo(const Hotspot& h)
{
    return h.pos.y > 0.f ? za::max(bladeFrom(h), h.pos.y) : 1.05f;
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
    h.sticky = value(slot, hotspotKey(index, 23));
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
    Cvar_SetValueQuick(&cvarAt(slot, hotspotKey(index, 23)), h.sticky);
}

int ownerSlot(int slot, Key key)
{
    for(int depth = 0; depth < 4 && slot >= 0 && inheritable(key); depth++)
    {
        const int from = inheritsFrom(slot);
        if(!takesParents(slot, key, from))
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
        case Key::HandAnchor:
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
        case Key::TwoHHandAnchor:
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
    pitch = ZA_ISFINITE(pitch) ? pitch : 0.f;
    yaw = ZA_ISFINITE(yaw) ? yaw : 0.f;
    if(pitch == 0.f && yaw == 0.f)
    {
        return aimRot;
    }
    // In the aim's frame (x forward, y left, z up): pitched up by `pitch`, then turned left by `yaw` about the aim's up.
    const float p = glm::radians(pitch), y = glm::radians(yaw);
    const glm::vec3 localFwd{za::cos(p) * za::cos(y), za::cos(p) * za::sin(y), za::sin(p)};
    const glm::vec3 localUp{-za::sin(p) * za::cos(y), -za::sin(p) * za::sin(y), za::cos(p)};
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

int slotForPart(const qmodel_t* model)
{
    int slot = slotForModel(model);
    if(slot >= 0 || !model)
    {
        return slot;
    }
    if(const auto it = partSlotCache.find(model); it != partSlotCache.end())
    {
        return it->second;
    }
    // A part drawn on its gun, made in the gun's model space (make_mags.py's magazine and well; the shotgun's auto pump's
    // parts, vr_view.cpp setupPumps; the super shotgun's two, open: make_ssg_open.py): the gun's slot.
    for(const char* prefix : {"progs/vr_mag_on_", "progs/vr_magwell_on_", "progs/vr_pump_on_", "progs/vr_pumpbody_on_",
             "progs/vr_ssg_frame_on_", "progs/vr_ssg_barrels_on_"})
    {
        const size_t length = strlen(prefix);
        if(!strncmp(model->name, prefix, length))
        {
            char gun[MAX_QPATH];
            q_snprintf(gun, sizeof(gun), "progs/%s", model->name + length);
            slot = slotForName(gun);
            break; // (the prefixes don't overlap)
        }
    }
    partSlotCache.emplace(model, slot);
    return slot;
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
        if(namesModel(cvarAt(slot, Key::ID).string, model->name))
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
        if(namesModel(cvarAt(slot, Key::ID).string, name))
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
        if(modelmeta::identifyPath(cvarAt(slot, Key::ID).string) == modelmeta::Id::Hand)
        {
            fistCache = slot;
            break;
        }
    }
    return fistCache;
}

bool heldAtOrigin(const qmodel_t* model)
{
    if(!model || model->type != mod_alias)
    {
        return false;
    }
    const int slot = slotForModel(model);
    return slot >= 0 && slot != fistSlot();
}

bool inheritable(Key key)
{
    switch(key)
    {
        case Key::ID:
        case Key::InheritFrom:
        case Key::HandAnchor:
        case Key::MuzzleAnchor:
        case Key::TwoHHandAnchor:
        case Key::WpnTextAnchor:
        case Key::WpnButtonAnchor: return false;
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
    // Inherited (a chain of at most a few: no loops) where this slot's own is its default and that default is not
    // marked its own (QVR_WEAPON_OWN: an inheriting slot's own defaults were ignored before, its parent's taken).
    return cvarAt(ownerSlot(slot, key), key).value;
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
        if(inheritable(static_cast<Key>(key)) && takesParents(slot, static_cast<Key>(key), inheritsFrom(slot)))
        {
            Cvar_SetValueQuick(&cvarAt(slot, static_cast<Key>(key)), values[key]);
        }
    }
    Cvar_SetValueQuick(&cvarAt(slot, Key::InheritFrom), 0.f);
}

void dump_f()
{
    migrate(); // the config's settings as the game takes them
    if(Cmd_Argc() < 2)
    {
        int n = 0;
        for(int slot = 0; slot < numSlots; slot++)
        {
            const int from = inheritsFrom(slot);
            if(from >= 0)
            {
                Con_Printf("wofs slot %d (%s) inherits slot %d (%s)\n", slot, cvarAt(slot, Key::ID).string, from,
                    cvarAt(from, Key::ID).string);
                n++;
            }
        }
        Con_Printf("wofs: %d slots inherit (vr_wofs_dump <slot> ... prints a slot's values)\n", n);
        return;
    }
    for(int arg = 1; arg < Cmd_Argc(); arg++)
    {
        const int slot = Q_atoi(Cmd_Argv(arg));
        if(slot < 0 || slot >= numSlots)
        {
            continue;
        }
        for(int k = 0; k < numKeys; k++)
        {
            const Key key = static_cast<Key>(k);
            const int owner = ownerSlot(slot, key);
            if(owner == slot)
            {
                Con_Printf("wofs %d %s \"%s\"\n", slot, keyNames[k], cvarAt(slot, key).string);
            }
            else
            {
                Con_Printf("wofs %d %s \"%s\" <-%d (own \"%s\")\n", slot, keyNames[k], cvarAt(owner, key).string, owner,
                    cvarAt(slot, key).string);
            }
        }
    }
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
    const za::PtrDiffT index = var - cvars.data();
    if(index >= 0 && index < static_cast<za::PtrDiffT>(cvars.size()) && index % numKeys == static_cast<int>(Key::ID))
    {
        onIdChanged(var);
    }
}

[[nodiscard]] ModelTransform makeModelTransform(const qmodel_t* model)
{
    ModelTransform t;

    t.k = modelScale();

    const auto& info = modelmeta::get(model);
    if(info.is(modelmeta::Id::Legholster))
    {
        t.active = true;
        t.scale = glm::vec3{vr_leg_holster_model_scale.value};
        t.offset = {vr_leg_holster_model_x_offset.value, vr_leg_holster_model_y_offset.value,
            vr_leg_holster_model_z_offset.value};
        return t;
    }

    // (A part drawn on its gun: the gun's own Scale and offsets: slotForPart.)
    const int slot = isHandPart(info) ? fistSlot() : slotForPart(model);
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
