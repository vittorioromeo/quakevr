// vr_props.cpp -- see vr_props.hpp.

#include "vr_props.hpp"
#include "vr_cvars.hpp"
#include "vr_units.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <functional>
#include <string>
#include <string_view>
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
// and bricks' slots; 39: the bricks two-handed; 40: the grip modes; 44: the grenade's; 45: the author's bricks and torch
// (the round's agents number their changes apart); 48: the bricks' grip offsets back to 0.
constexpr int settingsVersion = 48;

std::array<std::string, numSlots * numKeys> names;
std::array<cvar_t, numSlots * numKeys> cvars{};

constexpr const char* retiredKeyNames[] = {
#define QVR_PROP_RETIRED(k) k,
#include "vr_props.inc"
#undef QVR_PROP_RETIRED
};
constexpr int numRetired = static_cast<int>(sizeof(retiredKeyNames) / sizeof(retiredKeyNames[0]));
std::array<std::string, numSlots * numRetired> retiredNames;
std::array<cvar_t, numSlots * numRetired> retiredCvars{};

[[nodiscard]] cvar_t& cvarAt(int slot, Key key)
{
    return cvars[slot * numKeys + static_cast<int>(key)];
}

// Model name -> slot (-1: none), and model -> slot, found again whenever a vr_prop_id_NN changes. By name: looked up
// by a model's name without making a std::string of it (a transparent hash).
struct NameHash
{
    using is_transparent = void;
    [[nodiscard]] std::size_t operator()(std::string_view s) const { return std::hash<std::string_view>{}(s); }
};
std::unordered_map<std::string, int, NameHash, std::equal_to<>> slotCache;
std::unordered_map<const qmodel_t*, int> modelSlotCache;

// The keys' defaults as numbers (value() of no slot).
float keyDefaultValues[numKeys];

// Counts the changes to the settings (settingsGeneration).
unsigned generation = 1;

void clearSlotCaches()
{
    slotCache.clear();
    modelSlotCache.clear();
    generation++;
}

// Any setting's change: what was made from the settings is looked at again; an ID's: the slots are found again.
void onChanged(cvar_t* var)
{
    generation++;
    if((vr_debug_carry.value || developer.value) && host_initialized)
    {
        Con_Printf("props: %s %s (generation %u, frame %d)\n", var->name, var->string, generation, host_framecount);
    }
    const std::ptrdiff_t index = var - cvars.data();
    if(index >= 0 && index < static_cast<std::ptrdiff_t>(cvars.size()) && index % numKeys == static_cast<int>(Key::ID))
    {
        clearSlotCaches();
    }
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
    // has the defaults of any prop (its mass still estimated, and it hits as a box).
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
    // 39: the whole, chipped and broken bricks (slots 22, 23, 25) may be held in both hands (round 21, "Hands: both work;
    // props through teleporters; climbing stamina"). A config that still has the old default (one hand) takes it; one
    // whose slot is another model's keeps its own.
    if(from < 39)
    {
        for(const int slot : {22, 23, 25})
        {
            cvar_t& id = cvarAt(slot, Key::ID);
            cvar_t& two = cvarAt(slot, Key::TwoHands);
            if(!strcmp(id.string, id.default_string) && !strcmp(two.string, "0"))
            {
                Cvar_SetQuick(&two, two.default_string);
            }
        }
    }
    // 40: the grip modes (round 21, "Held props: grip modes, live offsets, palm grip, torch handle"). The wall torch is
    // held Along the Handle and the rocks and the half brick In the Palm: their grips take the new defaults (a config's
    // fixed grip for them was a way round the old ones). Grip X..Roll were read only by a fixed grip; they are an offset
    // in the other modes now, so a Where Taken slot's (which did nothing) go back to 0.
    if(from < 40)
    {
        constexpr Key gripKeys[] = {Key::GripMode, Key::GripX, Key::GripY, Key::GripZ, Key::GripPitch, Key::GripYaw, Key::GripRoll,
            Key::HandleFrom, Key::HandleTo, Key::HandleTilt};
        for(int slot = 0; slot < numSlots; slot++)
        {
            const bool shipped = slot == wallTorchSlot || (slot >= 17 && slot <= 21) || slot == 24;
            const bool own = !strcmp(cvarAt(slot, Key::ID).string, cvarAt(slot, Key::ID).default_string);
            if(shipped && own)
            {
                for(const Key key : gripKeys)
                {
                    Cvar_SetQuick(&cvarAt(slot, key), cvarAt(slot, key).default_string);
                }
                Con_DPrintf("Held Object Offsets: %s: the new grip (mode %s)\n", cvarAt(slot, Key::ID).string,
                    cvarAt(slot, Key::GripMode).string);
            }
            else if(atof(cvarAt(slot, Key::GripMode).string) == 1.0)
            {
                // A fixed grip's Pitch is up for every model now: a brush model's (a box: "maps/....bsp") was down.
                const char* id = cvarAt(slot, Key::ID).string;
                const std::size_t n = strlen(id);
                if(n > 4 && !strcmp(id + n - 4, ".bsp") && atof(cvarAt(slot, Key::GripPitch).string) != 0.0)
                {
                    Cvar_SetValueQuick(&cvarAt(slot, Key::GripPitch), static_cast<float>(-atof(cvarAt(slot, Key::GripPitch).string)));
                }
            }
            else if(atof(cvarAt(slot, Key::GripMode).string) == 0.0)
            {
                for(const Key key : {Key::GripX, Key::GripY, Key::GripZ, Key::GripPitch, Key::GripYaw, Key::GripRoll})
                {
                    Cvar_SetQuick(&cvarAt(slot, key), "0");
                }
            }
        }
    }
    // 44: the grenade (slot 4, progs/grenade.mdl: a hand grenade from the pouch, a caught one) is held In the Palm, in one
    // hand (round 21, "Hand grenades from the back pouch"): its grip and Two Hands take the new defaults, if the slot is
    // still the grenade's and they are still every prop's (Where Taken, two hands: a config's own choice is kept).
    if(from < 44)
    {
        constexpr int grenadeSlot = 3;
        if(!strcmp(cvarAt(grenadeSlot, Key::ID).string, cvarAt(grenadeSlot, Key::ID).default_string) &&
            atof(cvarAt(grenadeSlot, Key::GripMode).string) == 0.0 && atof(cvarAt(grenadeSlot, Key::TwoHands).string) == 1.0)
        {
            for(const Key key : {Key::GripMode, Key::TwoHands})
            {
                Cvar_SetQuick(&cvarAt(grenadeSlot, key), cvarAt(grenadeSlot, key).default_string);
            }
            Con_DPrintf("Held Object Offsets: progs/grenade.mdl: In the Palm, one hand\n");
        }
    }
    // 45: the author's Held Object Offsets (2026-09-29, NOTES.md start_2026-09-29_18-37-53 and 18-43-49): every brick In
    // the Palm, as the rocks and the half brick were (the whole, chipped and broken bricks, slots 22, 23, 25, were fixed),
    // and the wall torch (slot 16) in both hands too. A slot still its model's, holding the old default, takes the new one.
    if(from < 45)
    {
        struct Change
        {
            int slot;
            Key key;
            const char* before;
        };
        constexpr Change changes[] = {{22, Key::GripMode, "1"}, {23, Key::GripMode, "1"}, {25, Key::GripMode, "1"},
            {wallTorchSlot, Key::TwoHands, "0"}};
        for(const Change& c : changes)
        {
            cvar_t& var = cvarAt(c.slot, c.key);
            if(!strcmp(cvarAt(c.slot, Key::ID).string, cvarAt(c.slot, Key::ID).default_string) && atof(var.string) == atof(c.before))
            {
                Cvar_SetQuick(&var, var.default_string);
                Con_DPrintf("Held Object Offsets: %s: %s %s (was %s)\n", cvarAt(c.slot, Key::ID).string, var.name, var.string,
                    c.before);
            }
        }
    }
    // 48: the whole, chipped and broken bricks (slots 22, 23, 25) In the Palm at the default offsets, 0 (2026-09-29,
    // NOTES.md start_2026-09-29_23-01-20: the author reset them and they looked better). A slot still its model's whose
    // Grip X or Z is the old default takes the new one.
    if(from < 48)
    {
        struct Change
        {
            int slot;
            Key key;
            float before;
        };
        constexpr Change changes[] = {{22, Key::GripX, 0.8f}, {22, Key::GripZ, -1.6f}, {23, Key::GripX, 0.8f},
            {23, Key::GripZ, -1.6f}, {25, Key::GripZ, -1.6f}};
        for(const Change& c : changes)
        {
            cvar_t& var = cvarAt(c.slot, c.key);
            if(!strcmp(cvarAt(c.slot, Key::ID).string, cvarAt(c.slot, Key::ID).default_string) &&
                std::fabs(static_cast<float>(atof(var.string)) - c.before) < 1e-4f)
            {
                Cvar_SetQuick(&var, var.default_string);
                Con_DPrintf("Held Object Offsets: %s: %s %s (was %g)\n", cvarAt(c.slot, Key::ID).string, var.name, var.string,
                    static_cast<double>(c.before));
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
        Cvar_SetCallback(&var, onChanged);
    }
    for(int key = 0; key < numKeys; key++)
    {
        keyDefaultValues[key] = static_cast<float>(atof(keyDefaults[key]));
    }

    // Retired keys (vr_props.inc): registered so that a config setting them loads silently; not saved, read by nothing.
    for(int slot = 0; slot < numSlots; slot++)
    {
        for(int key = 0; key < numRetired; key++)
        {
            std::string& name = retiredNames[slot * numRetired + key];
            name = std::string("vr_prop_") + retiredKeyNames[key] + (slot + 1 < 10 ? "_0" : "_") + std::to_string(slot + 1);
            cvar_t& var = retiredCvars[slot * numRetired + key];
            var.name = name.c_str();
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

int slotForModel(const char* model)
{
    if(!model || !model[0])
    {
        return -1;
    }
    migrate();
    if(const auto it = slotCache.find(std::string_view{model}); it != slotCache.end())
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
    slotCache.emplace(model, found);
    return found;
}

int slotForModel(const qmodel_t* model)
{
    if(!model)
    {
        return -1;
    }
    migrate();
    if(const auto it = modelSlotCache.find(model); it != modelSlotCache.end())
    {
        return it->second;
    }
    const int found = slotForModel(model->name);
    modelSlotCache.emplace(model, found);
    return found;
}

void resetModelCache()
{
    clearSlotCaches();
}

unsigned settingsGeneration()
{
    return generation;
}

float value(int slot, Key key)
{
    if(slot < 0 || slot >= numSlots)
    {
        return keyDefaultValues[static_cast<int>(key)];
    }
    return cvarAt(slot, key).value;
}

float valueFor(const char* model, Key key)
{
    return value(slotForModel(model), key);
}

float valueFor(const qmodel_t* model, Key key)
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
    clearSlotCaches();
}

bool weightKey(Key key)
{
    switch(key)
    {
        case Key::Mass:
        case Key::Inertia:
        case Key::ComX:
        case Key::ComY:
        case Key::ComZ:
        case Key::Throw:
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
// (The ID is in both: the model a slot is for.)
[[nodiscard]] bool inPart(Key key, Part part)
{
    return part == Part::All || key == Key::ID || (part == Part::Weights) == weightKey(key);
}
} // namespace

void resetSlotToDefaults(int slot, Part part)
{
    if(slot < 0 || slot >= numSlots)
    {
        return;
    }
    // A slot the menu gave a model keeps it (its defaults are a free slot's: every key at the key's default).
    const std::string id = cvarAt(slot, Key::ID).string;
    if(part == Part::All)
    {
        resetSlot(slot);
    }
    else
    {
        for(int key = 0; key < numKeys; key++)
        {
            if(static_cast<Key>(key) != Key::ID && inPart(static_cast<Key>(key), part))
            {
                cvar_t& var = cvarAt(slot, static_cast<Key>(key));
                Cvar_SetQuick(&var, var.default_string);
            }
        }
    }
    if(freeId(cvarAt(slot, Key::ID).string))
    {
        Cvar_SetQuick(&cvarAt(slot, Key::ID), id.c_str());
    }
}

void printSlot(int slot, Part part)
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
        if(inPart(static_cast<Key>(key), part) && strcmp(var.string, var.default_string) != 0)
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
