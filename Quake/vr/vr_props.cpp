// vr_props.cpp -- see vr_props.hpp.

#include "vr_props.hpp"
#include "vr_cvars.hpp"
#include "vr_units.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <string>
#include <unordered_map>

namespace qvr::props
{
namespace
{

constexpr int numKeys = static_cast<int>(Key::Count);

constexpr const char* keyNames[numKeys] = {
#define QVR_PROP_KEY(e, k, d) k,
#include "vr_props.inc"
#undef QVR_PROP_KEY
};

constexpr const char* keyEnumNames[numKeys] = {
#define QVR_PROP_KEY(e, k, d) #e,
#include "vr_props.inc"
#undef QVR_PROP_KEY
};

constexpr const char* keyDefaults[numKeys] = {
#define QVR_PROP_KEY(e, k, d) d,
#include "vr_props.inc"
#undef QVR_PROP_KEY
};

// Configs archive every slot, so a slot whose shipped defaults change keeps a config's old values: vr_props_version
// says which changes a config has seen (as vr_wofs_version for the weapons). 1: the table's first version; 26: the rocks
// and bricks' slots (the round's agents number their changes apart).
constexpr int settingsVersion = 26;

std::array<std::string, numSlots * numKeys> names;
std::array<cvar_t, numSlots * numKeys> cvars{};

[[nodiscard]] cvar_t& cvarAt(int slot, Key key)
{
    return cvars[slot * numKeys + static_cast<int>(key)];
}

// Model name -> slot (-1: none), found again whenever a vr_prop_id_NN changes: Box3D asks for every prop's each frame.
std::unordered_map<std::string, int> slotCache;

void onIdChanged(cvar_t* /* var */)
{
    slotCache.clear();
}

[[nodiscard]] bool freeId(const char* id)
{
    return !id[0] || !strcmp(id, "-1");
}

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
    if(vr_props_version.value >= settingsVersion)
    {
        return;
    }
    const int from = static_cast<int>(vr_props_version.value);
    // 26: the rocks and bricks lying about (vr_debris.cpp) have slots 17-25 (vr_prop_*_18 to _26), which a config saved before has empty:
    // they take their defaults. A slot the config gave another model (Held Object Offsets) keeps it; that piece then
    // has the defaults of any prop (its mass still estimated, but no Blunt: it hits as a box).
    if(from < 26)
    {
        for(int slot = 17; slot <= 25; slot++)
        {
            if(freeId(cvarAt(slot, Key::ID).string))
            {
                resetSlot(slot);
            }
            else if(strcmp(cvarAt(slot, Key::ID).string, cvarAt(slot, Key::ID).default_string) != 0)
            {
                Con_Printf("Held Object Offsets: slot %d is %s's in this config; %s keeps the defaults\n", slot + 1,
                    cvarAt(slot, Key::ID).string, cvarAt(slot, Key::ID).default_string);
            }
        }
    }
    Cvar_SetValueQuick(&vr_props_version, settingsVersion);
}

} // namespace

void registerCvars()
{
    for(int slot = 0; slot < numSlots; slot++)
    {
        for(int key = 0; key < numKeys; key++)
        {
            names[slot * numKeys + key] = std::string("vr_prop_") + keyNames[key] + (slot + 1 < 10 ? "_0" : "_") + std::to_string(slot + 1);
            cvar_t& var = cvars[slot * numKeys + key];
            var.name = names[slot * numKeys + key].c_str();
            var.string = keyDefaults[key];
            var.flags = CVAR_ARCHIVE;
        }
    }

#define QVR_PROP_DEFAULT(slot, key, value) cvarAt(slot, Key::key).string = value;
#include "vr_props.inc"
#undef QVR_PROP_DEFAULT

    for(cvar_t& var : cvars)
    {
        Cvar_RegisterVariable(&var);
    }
    for(int slot = 0; slot < numSlots; slot++)
    {
        Cvar_SetCallback(&cvarAt(slot, Key::ID), onIdChanged);
    }
}

int slotForModel(const char* model)
{
    if(!model || !model[0])
    {
        return -1;
    }
    migrate();
    thread_local std::string key;
    key = model;
    if(const auto it = slotCache.find(key); it != slotCache.end())
    {
        return it->second;
    }
    int found = -1;
    for(int slot = 0; slot < numSlots; slot++)
    {
        if(!strcmp(cvarAt(slot, Key::ID).string, model))
        {
            found = slot;
            break;
        }
    }
    slotCache.emplace(key, found);
    return found;
}

float value(int slot, Key key)
{
    if(slot < 0 || slot >= numSlots)
    {
        return static_cast<float>(atof(keyDefaults[static_cast<int>(key)]));
    }
    return cvarAt(slot, key).value;
}

float valueFor(const char* model, Key key)
{
    return value(slotForModel(model), key);
}

cvar_t* cvar(int slot, Key key)
{
    return slot >= 0 && slot < numSlots ? &cvarAt(slot, key) : nullptr;
}

const char* keyName(Key key)
{
    return static_cast<int>(key) < numKeys ? keyNames[static_cast<int>(key)] : "";
}

Key keyByName(const char* name)
{
    for(int key = 0; key < numKeys; key++)
    {
        if(!q_strcasecmp(keyNames[key], name))
        {
            return static_cast<Key>(key);
        }
    }
    return Key::Count;
}

int claimSlot(const char* model)
{
    if(!model || !model[0])
    {
        return -1;
    }
    if(const int slot = slotForModel(model); slot >= 0)
    {
        return slot;
    }
    for(int slot = 0; slot < numSlots; slot++)
    {
        if(freeId(cvarAt(slot, Key::ID).string))
        {
            resetSlot(slot); // (a free slot's other keys at their defaults)
            Cvar_SetQuick(&cvarAt(slot, Key::ID), model);
            return slot;
        }
    }
    return -1;
}

void takeShippedSlot(int slot)
{
    if(slot < 0 || slot >= numSlots)
    {
        return;
    }
    cvar_t& id = cvarAt(slot, Key::ID);
    if(!freeId(id.string) && strcmp(id.string, id.default_string) != 0)
    {
        // Another model the menu gave this slot: its settings move to a free one first.
        int to = -1;
        for(int other = 0; other < numSlots && to < 0; other++)
        {
            if(other != slot && freeId(cvarAt(other, Key::ID).string) && freeId(cvarAt(other, Key::ID).default_string))
            {
                to = other;
            }
        }
        if(to < 0)
        {
            Con_Printf("Held Object Offsets: no free slot for %s's settings; %s keeps its defaults\n", id.string,
                id.default_string);
            return;
        }
        for(int key = 0; key < numKeys; key++)
        {
            Cvar_SetQuick(&cvarAt(to, static_cast<Key>(key)), cvarAt(slot, static_cast<Key>(key)).string);
        }
        Con_DPrintf("Held Object Offsets: %s's settings moved from slot %d to %d\n", id.string, slot + 1, to + 1);
    }
    for(int key = 0; key < numKeys; key++)
    {
        cvar_t& var = cvarAt(slot, static_cast<Key>(key));
        Cvar_SetQuick(&var, var.default_string);
    }
    slotCache.clear();
}

void resetSlotToDefaults(int slot)
{
    if(slot < 0 || slot >= numSlots)
    {
        return;
    }
    // A slot the menu gave a model keeps it (its defaults are a free slot's: every key at the key's default).
    const std::string id = cvarAt(slot, Key::ID).string;
    resetSlot(slot);
    if(freeId(cvarAt(slot, Key::ID).string))
    {
        Cvar_SetQuick(&cvarAt(slot, Key::ID), id.c_str());
    }
}

void printSlot(int slot)
{
    if(slot < 0 || slot >= numSlots)
    {
        return;
    }
    Con_Printf("// %s (prop slot %d, cvars _%02d): the settings changed from the defaults\n", cvarAt(slot, Key::ID).string, slot,
        slot + 1);
    for(int key = 0; key < numKeys; key++)
    {
        const cvar_t& var = cvarAt(slot, static_cast<Key>(key));
        if(strcmp(var.string, var.default_string) != 0)
        {
            Con_Printf("QVR_PROP_DEFAULT(%d, %s, \"%s\") // was %s\n", slot, keyEnumNames[key], var.string, var.default_string);
        }
    }
}

float stoneDensity(const qmodel_t* model)
{
    if(!model || model->type != mod_alias)
    {
        return 0.f;
    }
    if(!strncmp(model->name, "progs/vr_rock", 13))
    {
        return 2600.f; // granite, sandstone: 2300-2700
    }
    if(!strncmp(model->name, "progs/vr_brick", 14))
    {
        return 1900.f; // fired clay brick: 1800-2000 (a whole one, 19 x 9 x 6 cm: 1.9 kg)
    }
    return 0.f;
}

float density(const qmodel_t* model, bool weaponLike)
{
    if(!model)
    {
        return 1000.f;
    }
    if(model->type == mod_brush)
    {
        return 400.f; // ammo and health boxes (full of shells, nails, cells, medkits), crates
    }
    if(const float stone = stoneDensity(model); stone > 0.f)
    {
        return stone;
    }
    if(weaponLike || !strncmp(model->name, "progs/g_", 8) || !strncmp(model->name, "progs/w_", 8) ||
        strstr(model->name, "key") || !strncmp(model->name, "progs/v_", 8))
    {
        return 700.f; // guns and blades (their hulls are partly air), keys
    }
    if(strstr(model->name, "armor"))
    {
        return 600.f;
    }
    if(strstr(model->name, "backpack"))
    {
        return 250.f;
    }
    return 1000.f; // gibs and heads: flesh
}

float estimateMass(const qmodel_t* model, const glm::vec3& boxSize)
{
    const float u2m = 1.f / std::max(units::metresToUnits(), 1.f);
    const glm::vec3 m = glm::max(boxSize, glm::vec3{0.5f}) * u2m;
    // A brush model is its box; an alias model's hull fills about half of it (Box3D's hulls of the gibs and the
    // backpack: 45 to 60%).
    const float fill = model && model->type == mod_brush ? 1.f : 0.5f;
    return m.x * m.y * m.z * fill * density(model, false);
}

float throwScale(int slot, float mass)
{
    if(const float own = value(slot, Key::Throw); own > 0.f)
    {
        return own;
    }
    const float free = std::max(vr_weight_throw_mass.value, 0.1f);
    return mass > free ? std::sqrt(free / mass) : 1.f;
}

} // namespace qvr::props
