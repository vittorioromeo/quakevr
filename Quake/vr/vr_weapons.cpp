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

// Cvar names must outlive the cvars.
std::array<std::string, numSlots * numKeys> names;
std::array<cvar_t, numSlots * numKeys> cvars{};

[[nodiscard]] cvar_t& cvarAt(int slot, Key key)
{
    return cvars[slot * numKeys + static_cast<int>(key)];
}

// Model -> slot, rebuilt whenever a vr_wofs_id_NN cvar changes.
std::unordered_map<const qmodel_t*, int> slotCache;

void onIdChanged(cvar_t* /* var */)
{
    slotCache.clear();
}

[[nodiscard]] bool isHandPart(const char* name)
{
    return !strcmp(name, "progs/hand_base.mdl") || !strncmp(name, "progs/finger_", 13);
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
}

int slotForModel(const qmodel_t* model)
{
    if(!model)
    {
        return -1;
    }

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
    for(int slot = 0; slot < numSlots; slot++)
    {
        if(!strcmp(cvarAt(slot, Key::ID).string, "progs/hand.mdl"))
        {
            return slot;
        }
    }

    return -1;
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

    // The first Quake VR releases used a 0.75 world scale; weapon settings are relative to it.
    t.k = (vr_world_scale.value / 0.75f) * vr_gunmodelscale.value;

    const char* name = model->name;
    if(!strcmp(name, "progs/vrtorso.mdl"))
    {
        t.active = true;
        t.scale = {vr_vrtorso_x_scale.value, vr_vrtorso_y_scale.value, vr_vrtorso_z_scale.value};
        return t;
    }

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
