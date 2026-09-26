// vr_weapons.cpp -- see vr_weapons.hpp.

#include "vr_weapons.hpp"
#include "vr_engine.hpp"
#include "vr_cvars.hpp"
#include "vr_protocol.hpp"

#include <array>
#include <cstring>
#include <string>
#include <unordered_map>

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

[[nodiscard]] cvar_t& cvarAt(int slot, Key key)
{
    return cvars[slot * numKeys + static_cast<int>(key)];
}

// Model -> slot, and the fist's slot (-2: not looked up yet), found again whenever a
// vr_wofs_id_NN cvar changes.
std::unordered_map<const qmodel_t*, int> slotCache;
int fistCache = -2;

void onIdChanged(cvar_t* /* var */)
{
    slotCache.clear();
    fistCache = -2;
}

[[nodiscard]] bool isHandPart(const char* name)
{
    return !strcmp(name, "progs/hand_base.mdl") || !strncmp(name, "progs/finger_", 13);
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
// Misc/quakevr/improve_weapons.py and improve_weapons3.py).
constexpr int settingsVersion = 11;

void resetSlot(int slot)
{
    for(int key = 0; key < numKeys; key++)
    {
        cvar_t& var = cvarAt(slot, static_cast<Key>(key));
        Cvar_SetQuick(&var, var.default_string);
    }
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
    Cvar_SetValueQuick(&vr_wofs_version, settingsVersion);
}

// ModelTransform::k: the weapon and hand models' scale.
[[nodiscard]] float modelScale()
{
    // The first Quake VR releases used a 0.75 world scale; weapon settings are relative to it.
    return (vr_world_scale.value / 0.75f) * vr_gunmodelscale.value;
}

// The "Weapon Only" sliders: the slot they move (-1: the main hand's weapon), and their values
// already applied.
int weaponOnlySlot = -1;
glm::vec3 weaponOnlyApplied{0.f};
bool weaponOnlyZeroing = false;

cvar_t* const weaponOnlyCvars[3] = {&vr_weapon_only_x, &vr_weapon_only_y, &vr_weapon_only_z};

void onWeaponOnlyChanged(cvar_t* var)
{
    if(weaponOnlyZeroing)
    {
        return;
    }
    for(int axis = 0; axis < 3; axis++)
    {
        if(var == weaponOnlyCvars[axis])
        {
            glm::vec3 d{0.f};
            d[axis] = var->value - weaponOnlyApplied[axis];
            weaponOnlyApplied[axis] = var->value;
            moveWeaponOnly(weaponOnlySlot >= 0 ? weaponOnlySlot : heldSlot(1), d);
        }
    }
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

#define QVR_WEAPON_DEFAULT(slot, key, value) cvarAt(slot, Key::key).string = value;
#include "vr_weapons.inc"
#undef QVR_WEAPON_DEFAULT

    for(cvar_t& var : cvars)
    {
        Cvar_RegisterVariable(&var);
    }

    for(int slot = 0; slot < numSlots; slot++)
    {
        Cvar_SetCallback(&cvarAt(slot, Key::ID), onIdChanged);
    }

    for(cvar_t* var : weaponOnlyCvars)
    {
        Cvar_SetCallback(var, onWeaponOnlyChanged);
    }
}

void moveWeaponOnly(int slot, const glm::vec3& d)
{
    if(slot < 0 || slot >= numSlots || slot == fistSlot() || d == glm::vec3{0.f})
    {
        return;
    }

    // In the weapon entity's (mirrored, turned) frame, before Ironwail's model matrix, a point `a`
    // of the model (its anchor vertex: scale_origin + scale * Scale * vertex) is at
    //   k * (Offset + a)                                  (the weapon: applyPre's S(k) * T(Offset))
    // and the drawn hand, held at the hand anchor vertex,
    //   offsetScale() * HandOffset + k * (Offset + a)     (anchorPosition's extra, before S(k)).
    // Offset + d moves every point of the weapon by k * d; the hand stays if HandOffset takes
    // back k * d / offsetScale(). Both are in the same frame (mirrored alike for the off hand).
    const float s = offsetScale();
    const float back = s > 1e-6f ? modelScale() / s : 0.875f / 0.75f;
    constexpr Key offsets[3] = {Key::OffsetX, Key::OffsetY, Key::OffsetZ};
    constexpr Key handOffsets[3] = {Key::HandOffsetX, Key::HandOffsetY, Key::HandOffsetZ};
    for(int axis = 0; axis < 3; axis++)
    {
        if(d[axis] != 0.f)
        {
            cvar_t& offset = cvarAt(slot, offsets[axis]);
            cvar_t& hand = cvarAt(slot, handOffsets[axis]);
            Cvar_SetValueQuick(&offset, offset.value + d[axis]);
            Cvar_SetValueQuick(&hand, hand.value - d[axis] * back);
        }
    }
}

void setWeaponOnlyTarget(int slot)
{
    weaponOnlySlot = slot;
    weaponOnlyZeroing = true;
    for(cvar_t* var : weaponOnlyCvars)
    {
        Cvar_SetValueQuick(var, 0.f);
    }
    weaponOnlyZeroing = false;
    weaponOnlyApplied = glm::vec3{0.f};
}

cvar_t* cvar(int slot, Key key)
{
    return slot >= 0 && slot < numSlots ? &cvarAt(slot, key) : nullptr;
}

void resetSlotToDefaults(int slot)
{
    if(slot >= 0 && slot < numSlots)
    {
        resetSlot(slot);
    }
}

// The slot's settings that differ from the shipped defaults, as vr_weapons.inc lines.
void printSlot(int slot)
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
        if(strcmp(var.string, var.default_string))
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

float value(int slot, Key key)
{
    return slot >= 0 ? cvarAt(slot, key).value : 0.f;
}

glm::vec3 vec(int slot, Key x, Key y, Key z)
{
    return {value(slot, x), value(slot, y), value(slot, z)};
}

float offsetScale()
{
    return (vr_world_scale.value / 1.25f) * (vr_gunmodelscale.value / 0.7f);
}

ModelTransform modelTransform(const qmodel_t* model)
{
    ModelTransform t;
    if(!model || model->type != mod_alias || !(cl.protocolflags & PRFL_QUAKEVR))
    {
        return t;
    }

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

} // namespace qvr::weapons
