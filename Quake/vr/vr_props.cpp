// vr_props.cpp -- see vr_props.hpp.

#include "vr_box3d.hpp"
#include "vr_modelmetadata.hpp"
#include "vr_props.hpp"
#include "vr_cvars.hpp"
#include "vr_protocol.hpp"
#include "vr_units.hpp"

#include "Zancle/Base/PtrDiffT.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Container/Array.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/StringView.hpp"
#include "Zancle/String/ToString.hpp"

#include <stdlib.h>
#include <string.h>

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
// says which changes a config has seen (as vr_wofs_version for the weapons). 68 is 1.0.0's: the changes before it were
// dropped with the configs older than 1.0.0 (vr_cvars.cpp resetOldConfig resets those). A config still below it (a
// first start's 50, saved before anything asked for a slot) has 1.0.0's shipped values, today's: it only takes the
// version. A change from now on: a `{version, step}` table as vr_weapons.cpp's.
constexpr int settingsVersion = 68;

za::Array<za::String, numSlots * numKeys> names;
za::Array<cvar_t, numSlots * numKeys> cvars{};


[[nodiscard]] cvar_t& cvarAt(int slot, Key key)
{
    return cvars[slot * numKeys + static_cast<int>(key)];
}

// Model name -> slot (-1: none), and model -> slot, found again whenever a vr_prop_id_NN changes. By name: looked up
// by a model's name without making a za::String of it (a transparent hash).
struct NameHash
{
    using is_transparent = void;
    [[nodiscard]] za::SizeT operator()(za::StringView s) const { return ankerl::unordered_dense::hash<za::StringView>{}(s); }
};
struct NameEqual
{
    using is_transparent = void;
    [[nodiscard]] bool operator()(za::StringView a, za::StringView b) const { return a == b; }
};
ankerl::unordered_dense::map<za::String, int, NameHash, NameEqual> slotCache;
ankerl::unordered_dense::map<const qmodel_t*, int> modelSlotCache;

// Which of the models drawn at their own sizes a model is (ownScale): Quake's grenades and the rocket in flight.
enum class Own : za::U8
{
    None,
    Prox,
    Grenade,
    Rocket,
};

// Which of them the model `name` is.
[[nodiscard]] Own ownOf(const char* name)
{
    if(!name)
    {
        return Own::None;
    }
    if(!strcmp(name, "progs/proxbomb.mdl"))
    {
        return Own::Prox;
    }
    if(!strcmp(name, "progs/grenade.mdl") || !strcmp(name, "progs/mervup.mdl"))
    {
        return Own::Grenade;
    }
    return !strcmp(name, "progs/missile.mdl") ? Own::Rocket : Own::None;
}

// Each slot's, by its vr_prop_id_NN (size(): many times a frame for every entity drawn or touched), found again
// whenever an ID changes (clearSlotCaches, as slotCache) and once the settings are registered: read only otherwise.
za::Array<Own, numSlots> slotOwn{};

void findSlotOwn()
{
    for(int slot = 0; slot < numSlots; slot++)
    {
        slotOwn[static_cast<za::SizeT>(slot)] = ownOf(cvarAt(slot, Key::ID).string);
    }
}

// The keys' defaults as numbers (value() of no slot).
float keyDefaultValues[numKeys];

// Counts the changes to the settings (settingsGeneration).
unsigned generation = 1;

void clearSlotCaches()
{
    slotCache.clear();
    modelSlotCache.clear();
    findSlotOwn();
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
    const za::PtrDiffT index = var - cvars.data();
    if(index >= 0 && index < static_cast<za::PtrDiffT>(cvars.size()) && index % numKeys == static_cast<int>(Key::ID))
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
    if(vr_props_version.value < settingsVersion)
    {
        Cvar_SetValueQuick(&vr_props_version, settingsVersion);
    }
}

// A setting at a key's own default (as numbers when both are): untouched by its config.
[[nodiscard]] bool atKeyDefault(int slot, int key)
{
    const char* s = cvarAt(slot, static_cast<Key>(key)).string;
    const char* d = keyDefaults[key];
    char* endS;
    char* endD;
    const double x = strtod(s, &endS);
    const double y = strtod(d, &endD);
    if(endS != s && !*endS && endD != d && !*endD)
    {
        return za::fabs(x - y) < 1e-6;
    }
    return !strcmp(s, d);
}

// The slots given shipped defaults since a config version (vr_props.inc): {the configVersion that gives it, the slot}.
struct ShippedSlot
{
    int version;
    int slot;
};
constexpr ShippedSlot shippedSlots[] = {
    {183, 62}, // AD's health vial (progs/health_5.mdl): in the palm
    {187, 64}, // AD's death fury's head (progs/h_dfury.mdl): 0.65 of its size
};

} // namespace

void takeShippedSlots(int fromConfigVersion)
{
    for(const ShippedSlot& s : shippedSlots)
    {
        if(fromConfigVersion >= s.version)
        {
            continue;
        }
        cvar_t& id = cvarAt(s.slot, Key::ID);
        if(!freeId(id.string) && strcmp(id.string, id.default_string) != 0)
        {
            continue; // another model's (claimed by the config): kept; the shipped one's model claims a free slot as before
        }
        const za::String model = id.default_string;
        // The slot: each setting the config left at its key's default takes the slot's own (a free slot: all of them).
        for(int key = 0; key < numKeys; key++)
        {
            if(key == static_cast<int>(Key::ID) || atKeyDefault(s.slot, key))
            {
                cvar_t& var = cvarAt(s.slot, static_cast<Key>(key));
                Cvar_SetQuick(&var, var.default_string);
            }
        }
        // The model claimed by another slot before (by a hand: only its ID set): that slot freed, so this one is found.
        for(int other = 0; other < numSlots; other++)
        {
            if(other == s.slot || strcmp(cvarAt(other, Key::ID).string, model.cStr()) != 0 ||
                !freeId(cvarAt(other, Key::ID).default_string))
            {
                continue;
            }
            bool untouched = true;
            for(int key = 0; key < numKeys && untouched; key++)
            {
                untouched = key == static_cast<int>(Key::ID) || atKeyDefault(other, key);
            }
            if(untouched)
            {
                resetSlot(other);
            }
        }
        Con_DPrintf("props: slot %d (%s) takes its shipped defaults\n", s.slot, model.cStr());
    }
}

void registerCvars()
{
    for(int slot = 0; slot < numSlots; slot++)
    {
        for(int key = 0; key < numKeys; key++)
        {
            names[slot * numKeys + key] = za::String("vr_prop_") + keyNames[key] + (slot + 1 < 10 ? "_0" : "_") + za::toString(slot + 1);
            cvar_t& var = cvars[slot * numKeys + key];
            var.name = names[slot * numKeys + key].cStr();
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
    // The grenade pouch's turn in the hand (vr_grip.cpp) places a held grenade as these settings do: its changes count
    // as theirs, so that the drawn grenade is placed again at once too (vr_held.cpp), not at its next take.
    for(cvar_t* var : {&vr_grenade_pouch_hold_pitch, &vr_grenade_pouch_hold_yaw, &vr_grenade_pouch_hold_roll})
    {
        Cvar_SetCallback(var, [](cvar_t*) { generation++; });
    }
    for(int key = 0; key < numKeys; key++)
    {
        keyDefaultValues[key] = static_cast<float>(atof(keyDefaults[key]));
    }
    findSlotOwn(); // (the IDs as registered: a change from now on calls onChanged)
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
    if(const auto it = slotCache.find(za::StringView{model}); it != slotCache.end())
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
    const float v = cvarAt(slot, key).value;
    if(key == Key::Mass && v < 0.f)
    {
        return box3d::headPropMass(cvarAt(slot, Key::ID).string); // (-1: a monster's head, its ragdoll's share)
    }
    return v;
}

float valueFor(const char* model, Key key)
{
    return value(slotForModel(model), key);
}

float valueFor(const qmodel_t* model, Key key)
{
    return value(slotForModel(model), key);
}

namespace
{

// Which of them a loaded model is, by its identity (modelmeta: the same names, kept per model until a map or game
// directory change or its reload).
[[nodiscard]] Own ownOf(const qmodel_t* model)
{
    const modelmeta::ModelMetadata& m = modelmeta::get(model);
    if(m.is(modelmeta::Id::Proxbomb))
    {
        return Own::Prox;
    }
    if(m.is(modelmeta::Id::Grenade) || m.is(modelmeta::Id::Mervup))
    {
        return Own::Grenade;
    }
    return m.is(modelmeta::Id::Missile) ? Own::Rocket : Own::None;
}

// A model's own size, as a Size (times its slot's): Quake's grenades (vr_grenade_scale: the grenade, the multi-grenade;
// vr_prox_scale: the proximity grenade) and the rocket in flight (vr_rocket_scale). (By name, four string compares,
// until 2026-10-12: a slot's is kept by slot, a model's by modelmeta.)
[[nodiscard]] float ownScale(Own own)
{
    switch(own)
    {
        case Own::Prox: return za::clamp(vr_prox_scale.value, 0.2f, 2.f);
        case Own::Grenade: return za::clamp(vr_grenade_scale.value, 0.25f, 2.f);
        case Own::Rocket: return za::clamp(vr_rocket_scale.value, 0.25f, 2.f);
        default: return 1.f;
    }
}

} // namespace

float size(int slot)
{
    const float own = slot >= 0 && slot < numSlots ? ownScale(slotOwn[static_cast<za::SizeT>(slot)]) : 1.f;
    return za::clamp(value(slot, Key::Size), 0.05f, 10.f) * own;
}

float drawnSize(const qmodel_t* model)
{
    if(!model || (model->type != mod_alias && model->type != mod_brush) ||
        !((cl.protocolflags & PRFL_QUAKEVR) || (sv.active && (sv.protocolflags & PRFL_QUAKEVR))))
    {
        return 1.f;
    }
    // Quake's grenades and the rocket in flight at their own sizes too (ownScale: in size() for a model with a slot):
    // everything drawn or made from their drawn shape takes it, as a Size.
    const int slot = slotForModel(model);
    return slot >= 0 ? size(slot) : size(slot) * ownScale(ownOf(model));
}

bool lengthKey(Key key)
{
    switch(key)
    {
        case Key::ComX:
        case Key::ComY:
        case Key::ComZ:
        case Key::TipX:
        case Key::TipY:
        case Key::TipZ:
        case Key::ButtX:
        case Key::ButtY:
        case Key::ButtZ:
        case Key::HandleFrom:
        case Key::HandleTo: return true;
        default: return false;
    }
}

float scaledValue(int slot, Key key)
{
    const float v = value(slot, key);
    return lengthKey(key) ? v * size(slot) : v;
}

cvar_t* cvar(int slot, Key key)
{
    return slot >= 0 && slot < numSlots ? &cvarAt(slot, key) : nullptr;
}

// keyByName's last few names (QC's propvalue: the same literals every frame), by their text.
struct RecentName
{
    char name[32]{};
    Key key{Key::Count};
};
RecentName recentNames[4];
int recentNamesNext = 0;

Key keyByName(const char* name)
{
    // The last few names asked (QC's propvalue: the same literals every frame, "forcegrab" for every prop in a force
    // grab's reach), by their text; else the keys' names in turn.
    for(const RecentName& r : recentNames)
    {
        if(r.name[0] && !strcmp(r.name, name))
        {
            return r.key;
        }
    }
    Key found = Key::Count;
    for(int key = 0; key < numKeys; key++)
    {
        if(!q_strcasecmp(keyNames[key], name))
        {
            found = static_cast<Key>(key);
            break;
        }
    }
    if(strlen(name) < sizeof(recentNames[0].name))
    {
        RecentName& r = recentNames[recentNamesNext];
        recentNamesNext = (recentNamesNext + 1) % static_cast<int>(sizeof(recentNames) / sizeof(recentNames[0]));
        q_strlcpy(r.name, name, sizeof(r.name));
        r.key = found;
    }
    return found;
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
    // None free: a slot the menu gave another model that was never changed (its page opened, nothing set: every key at
    // its default) is given to this one. (Opening the pages with weapons in hand claims a slot for each.)
    for(int slot = 0; slot < numSlots; slot++)
    {
        if(!freeId(cvarAt(slot, Key::ID).default_string))
        {
            continue; // a shipped prop's
        }
        bool untouched = true;
        for(int key = 0; key < numKeys && untouched; key++)
        {
            const cvar_t& var = cvarAt(slot, static_cast<Key>(key));
            untouched = static_cast<Key>(key) == Key::ID || !strcmp(var.string, var.default_string);
        }
        if(untouched)
        {
            Cvar_SetQuick(&cvarAt(slot, Key::ID), model);
            return slot;
        }
    }
    return -1;
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
        case Key::ThrowDamage:
        case Key::SpinAlign:
        case Key::MassScale: return true;
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
    const za::String id = cvarAt(slot, Key::ID).string;
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
        Cvar_SetQuick(&cvarAt(slot, Key::ID), id.cStr());
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
    const auto& info = modelmeta::get(model);
    if(info.has(modelmeta::Trait::Rock))
    {
        return 2600.f; // granite, sandstone: 2300-2700
    }
    if(info.has(modelmeta::Trait::Brick))
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
    const auto& info = modelmeta::get(model);
    if(weaponLike || info.has(modelmeta::Trait::WorldWeapon) || info.has(modelmeta::Trait::WeaponItem) ||
        info.has(modelmeta::Trait::ContainsKey) || info.has(modelmeta::Trait::ViewWeapon))
    {
        return 700.f; // guns and blades (their hulls are partly air), keys
    }
    if(info.has(modelmeta::Trait::ContainsArmor))
    {
        return 600.f;
    }
    if(info.has(modelmeta::Trait::ContainsBackpack))
    {
        return 250.f;
    }
    return 1000.f; // gibs and heads: flesh
}

float estimateMass(const qmodel_t* model, const glm::vec3& boxSize)
{
    const float u2m = 1.f / za::max(units::metresToUnits(), 1.f);
    const glm::vec3 m = glm::max(boxSize, glm::vec3{0.5f}) * u2m;
    // A brush model is its box; an alias model's hull fills about half of it (Box3D's hulls of the gibs and the
    // backpack: 45 to 60%).
    const float fill = model && model->type == mod_brush ? 1.f : 0.5f;
    return m.x * m.y * m.z * fill * density(model, false) * massScale(model);
}

float massScale(const qmodel_t* model)
{
    return za::max(valueFor(model, Key::MassScale), 0.f);
}

float throwScale(int slot, float /*mass*/)
{
    const float own = value(slot, Key::Throw);
    return own > 0.f ? own : 1.f; // (0: throws by weight limit it by its mass, weight::throwVelocity)
}

} // namespace qvr::props
