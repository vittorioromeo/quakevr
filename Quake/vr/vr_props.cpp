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
#include "Zancle/Container/Array.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/StringView.hpp"
#include "Zancle/String/ToString.hpp"

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
// says which changes a config has seen (as vr_wofs_version for the weapons). 1: the table's first version; 26: the rocks
// and bricks' slots; 39: the bricks two-handed; 40: the grip modes; 44: the grenade's; 45: the author's bricks and torch
// (the round's agents number their changes apart); 48: the bricks' grip offsets back to 0; 49: the crates' slots; 50:
// the rocks and bricks at Size 1.25; 51: the crates' small pieces in the palm; 53: the multi-grenade's as the grenade's;
// 54: the author's grenade and multi-grenade fits; 55: the author's weights and sizes (slots 6-16 his items); 56: every
// prop in both hands; 57: the gibs' and heads' sizes; 58: the author's lighter gibs and heads (and the gremlin's head);
// 59: the monsters' heads weigh what a head cut off their ragdoll does (Mass -1); 60: immersive reloading's shells
// (slots 49-50); 61: reloading's magazines; 62: vrstart's barrel (slot 61); 63: the magazines' sizes; 64: the live
// shell's grip; 65: the author's gremlin head at Size 0.6; 66: the launchers' rounds (slots 54-56); 67: the proximity
// grenade's (slot 56: Quake's progs/proxbomb.mdl, the pouches' again); 68: the author's grenade fit of 2026-10-08.
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
                const za::SizeT n = strlen(id);
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
                za::fabs(static_cast<float>(atof(var.string)) - c.before) < 1e-4f)
            {
                Cvar_SetQuick(&var, var.default_string);
                Con_DPrintf("Held Object Offsets: %s: %s %s (was %g)\n", cvarAt(c.slot, Key::ID).string, var.name, var.string,
                    static_cast<double>(c.before));
            }
        }
    }
    // 49: the wooden crates and their pieces (vr_crates.qc) have slots 26-31 (vr_prop_*_27 to _32), which a config saved
    // before has empty: they take their defaults, as the rocks' and bricks' did (26). A slot the config gave another
    // model keeps it.
    if(from < 49)
    {
        for(int slot = 26; slot <= 31; slot++)
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
    // 50: every rock and brick (slots 17-25) drawn at 1.25 times its model's size (the author's, 2026-10-01, NOTES.md
    // vrfiringrange_2026-10-01_00-24-47 and 00-25-50). A slot still its model's at the old default (1) takes it.
    if(from < 50)
    {
        for(int slot = 17; slot <= 25; slot++)
        {
            cvar_t& var = cvarAt(slot, Key::Size);
            if(!strcmp(cvarAt(slot, Key::ID).string, cvarAt(slot, Key::ID).default_string) && atof(var.string) == 1.0)
            {
                Cvar_SetQuick(&var, var.default_string);
                Con_DPrintf("Held Object Offsets: %s: Size %s (was 1)\n", cvarAt(slot, Key::ID).string, var.string);
            }
        }
    }
    // 51: a crate's small pieces (the broken board, the splinter, the broken batten: slots 29-31, vr_prop_*_30 to _32)
    // are held in the palm (Grip Mode 2, as a rock); the whole board keeps where it was taken (NOTES.md
    // e1m1_2026-10-01_02-49-27). A config that still has the old default (0) and the slot's own model takes it.
    if(from < 51)
    {
        for(const int slot : {29, 30, 31})
        {
            cvar_t& var = cvarAt(slot, Key::GripMode);
            if(!strcmp(cvarAt(slot, Key::ID).string, cvarAt(slot, Key::ID).default_string) && atof(var.string) == 0.0)
            {
                Cvar_SetQuick(&var, var.default_string);
                Con_DPrintf("Held Object Offsets: %s: %s %s (was 0)\n", cvarAt(slot, Key::ID).string, var.name, var.string);
            }
        }
    }
    // 53: the mission pack's multi-grenade (slot 5, progs/mervup.mdl: one from the pouch with B/Y held, an ogre's caught)
    // is held as the grenade is (round 21, "Multi-grenades from the pouch"): In the Palm, in one hand, if the slot is
    // still the multi-grenade's and they are still every prop's (as 44 for the grenade).
    if(from < 53)
    {
        constexpr int multiGrenadeSlot = 4;
        if(!strcmp(cvarAt(multiGrenadeSlot, Key::ID).string, cvarAt(multiGrenadeSlot, Key::ID).default_string) &&
            atof(cvarAt(multiGrenadeSlot, Key::GripMode).string) == 0.0 &&
            atof(cvarAt(multiGrenadeSlot, Key::TwoHands).string) == 1.0)
        {
            for(const Key key : {Key::GripMode, Key::TwoHands})
            {
                Cvar_SetQuick(&cvarAt(multiGrenadeSlot, key), cvarAt(multiGrenadeSlot, key).default_string);
            }
            Con_DPrintf("Held Object Offsets: progs/mervup.mdl: In the Palm, one hand\n");
        }
    }
    // 54: the author's grenade and multi-grenade fits (NOTES.md vrfiringrange_2026-10-01_16-53-53 and 17-17-58, "make
    // those the defaults"). A slot still its model's that holds the old default (every prop's) takes the new one.
    if(from < 54)
    {
        struct Change
        {
            int slot;
            Key key;
        };
        constexpr Change changes[] = {{3, Key::GripRoll}, {3, Key::GripZ}, {3, Key::Overlap}, {4, Key::GripYaw},
            {4, Key::Overlap}};
        for(const Change& c : changes)
        {
            cvar_t& var = cvarAt(c.slot, c.key);
            const char* before = keyDefaults[static_cast<int>(c.key)];
            if(!strcmp(cvarAt(c.slot, Key::ID).string, cvarAt(c.slot, Key::ID).default_string) &&
                atof(var.string) == atof(before))
            {
                Cvar_SetQuick(&var, var.default_string);
                Con_DPrintf("Held Object Offsets: %s: %s %s (was %s)\n", cvarAt(c.slot, Key::ID).string, var.name, var.string,
                    before);
            }
        }
    }
    // 55: the author's weights and sizes (NOTES.md vrfiringrange_2026-10-02_00-55-03, "make them the defaults"). The
    // items he gave slots 6-16 (vr_prop_*_06 to _16): a config with the slot free, and the item in no other slot, takes
    // them (one that gave the slot or the item another place keeps its own). The shipped props' new masses, Mass x and
    // Handle Tilt: a slot still its model's that holds the old default takes the new one.
    if(from < 55)
    {
        for(int slot = 5; slot <= 15; slot++)
        {
            const char* shipped = cvarAt(slot, Key::ID).default_string;
            bool elsewhere = false;
            for(int other = 0; other < numSlots && !elsewhere; other++)
            {
                elsewhere = other != slot && !strcmp(cvarAt(other, Key::ID).string, shipped);
            }
            if(freeId(cvarAt(slot, Key::ID).string) && !elsewhere)
            {
                resetSlot(slot);
            }
            else if(strcmp(cvarAt(slot, Key::ID).string, shipped) != 0)
            {
                Con_Printf("Held Object Offsets: slot %d is %s's in this config; %s keeps its own settings\n", slot + 1,
                    cvarAt(slot, Key::ID).string, shipped);
            }
        }
        struct Change
        {
            int slot;
            Key key;
            float before;
        };
        constexpr Change changes[] = {{0, Key::Mass, 40.f}, {1, Key::Mass, 25.f}, {16, Key::Mass, 0.9f},
            {17, Key::MassScale, 1.f}, {18, Key::MassScale, 1.f}, {19, Key::MassScale, 1.f}, {20, Key::MassScale, 1.f},
            {21, Key::MassScale, 1.f}, {22, Key::MassScale, 1.f}, {23, Key::MassScale, 1.f}, {25, Key::MassScale, 1.f},
            {22, Key::HandleTilt, 25.f}, {23, Key::HandleTilt, 25.f}, {26, Key::Mass, 25.f}, {27, Key::Mass, 40.f},
            {28, Key::Mass, 0.6f}, {29, Key::Mass, 0.4f}, {31, Key::Mass, 0.45f}, {33, Key::Mass, 0.8f},
            {34, Key::Mass, 2.5f}, {35, Key::Mass, 2.f}, {37, Key::Mass, 5.f}, {38, Key::Mass, 3.f}, {39, Key::Mass, 5.f},
            {40, Key::Mass, 5.f}, {41, Key::Mass, 6.f}, {42, Key::Mass, 9.f}, {43, Key::Mass, 3.f}, {44, Key::Mass, 4.f},
            {45, Key::Mass, 6.f}, {46, Key::Mass, 15.f}, {47, Key::Mass, 12.f}};
        for(const Change& c : changes)
        {
            cvar_t& var = cvarAt(c.slot, c.key);
            if(!strcmp(cvarAt(c.slot, Key::ID).string, cvarAt(c.slot, Key::ID).default_string) &&
                za::fabs(static_cast<float>(atof(var.string)) - c.before) < 1e-4f)
            {
                Cvar_SetQuick(&var, var.default_string);
                Con_DPrintf("Held Object Offsets: %s: %s %s (was %g)\n", cvarAt(c.slot, Key::ID).string, var.name, var.string,
                    static_cast<double>(c.before));
            }
        }
    }
    // 56: every prop may be held in both hands (the author's, 2026-10-03; ROUND21.md, "Every prop in both hands": the
    // other hand takes a small thing too, to grip it again). The grenade and multi-grenade (slots 3, 4), the rocks and the
    // half brick (17-21, 24) and the crate's broken board and splinter (29, 30) were one hand only: a slot still its
    // model's that holds that old default takes the new one (a config's own choice for another model is kept).
    if(from < 56)
    {
        for(const int slot : {3, 4, 17, 18, 19, 20, 21, 24, 29, 30})
        {
            cvar_t& var = cvarAt(slot, Key::TwoHands);
            if(!strcmp(cvarAt(slot, Key::ID).string, cvarAt(slot, Key::ID).default_string) && atof(var.string) == 0.0)
            {
                Cvar_SetQuick(&var, var.default_string);
                Con_DPrintf("Held Object Offsets: %s: %s %s (was 0)\n", cvarAt(slot, Key::ID).string, var.name, var.string);
            }
        }
    }
    // 57: October 5 playtest gib/head sizes; preserve custom sizes and reassigned slots.
    if(from < 57)
    {
        if(modelmeta::identifyPath(cvarAt(34, Key::ID).string) == modelmeta::Id::Gib2 &&
            za::fabs(value(34, Key::Size) - 1.0f) < 1e-4f)
            Cvar_SetQuick(&cvarAt(34, Key::Size), cvarAt(34, Key::Size).default_string);
        if(modelmeta::identifyPath(cvarAt(35, Key::ID).string) == modelmeta::Id::Gib3 &&
            za::fabs(value(35, Key::Size) - 1.0f) < 1e-4f)
            Cvar_SetQuick(&cvarAt(35, Key::Size), cvarAt(35, Key::Size).default_string);
        if(modelmeta::identifyPath(cvarAt(37, Key::ID).string) == modelmeta::Id::HGuard &&
            za::fabs(value(37, Key::Size) - 1.0f) < 1e-4f)
            Cvar_SetQuick(&cvarAt(37, Key::Size), cvarAt(37, Key::Size).default_string);
        if(modelmeta::identifyPath(cvarAt(38, Key::ID).string) == modelmeta::Id::HDog &&
            za::fabs(value(38, Key::Size) - 1.0f) < 1e-4f)
            Cvar_SetQuick(&cvarAt(38, Key::Size), cvarAt(38, Key::Size).default_string);
        if(modelmeta::identifyPath(cvarAt(39, Key::ID).string) == modelmeta::Id::HMega &&
            za::fabs(value(39, Key::Size) - 1.0f) < 1e-4f)
            Cvar_SetQuick(&cvarAt(39, Key::Size), cvarAt(39, Key::Size).default_string);
        if(modelmeta::identifyPath(cvarAt(40, Key::ID).string) == modelmeta::Id::HKnight &&
            za::fabs(value(40, Key::Size) - 1.0f) < 1e-4f)
            Cvar_SetQuick(&cvarAt(40, Key::Size), cvarAt(40, Key::Size).default_string);
        if(modelmeta::identifyPath(cvarAt(41, Key::ID).string) == modelmeta::Id::HHellkn &&
            za::fabs(value(41, Key::Size) - 1.0f) < 1e-4f)
            Cvar_SetQuick(&cvarAt(41, Key::Size), cvarAt(41, Key::Size).default_string);
        if(modelmeta::identifyPath(cvarAt(42, Key::ID).string) == modelmeta::Id::HOgre &&
            za::fabs(value(42, Key::Size) - 1.0f) < 1e-4f)
            Cvar_SetQuick(&cvarAt(42, Key::Size), cvarAt(42, Key::Size).default_string);
        if(modelmeta::identifyPath(cvarAt(45, Key::ID).string) == modelmeta::Id::HShal &&
            za::fabs(value(45, Key::Size) - 1.0f) < 1e-4f)
            Cvar_SetQuick(&cvarAt(45, Key::Size), cvarAt(45, Key::Size).default_string);
        if(modelmeta::identifyPath(cvarAt(46, Key::ID).string) == modelmeta::Id::HShams &&
            za::fabs(value(46, Key::Size) - 1.0f) < 1e-4f)
            Cvar_SetQuick(&cvarAt(46, Key::Size), cvarAt(46, Key::Size).default_string);
        if(modelmeta::identifyPath(cvarAt(47, Key::ID).string) == modelmeta::Id::HDemon &&
            za::fabs(value(47, Key::Size) - 1.0f) < 1e-4f)
            Cvar_SetQuick(&cvarAt(47, Key::Size), cvarAt(47, Key::Size).default_string);
    }
    // 58: the author's gib and head weights (his config, 2026-10-07; INSTALLER.md, Appendix A): a slot still its model's
    // that holds the old default takes the new one (a config's own weight, or a slot given another model, is kept).
    if(from < 58)
    {
        struct Change
        {
            int slot;
            float before;
        };
        constexpr Change changes[] = {{7, 18.f}, {34, 20.f}, {35, 12.f}, {37, 10.f}, {38, 12.f}, {39, 16.f}, {40, 12.f},
            {41, 17.f}, {42, 30.f}, {45, 12.f}, {46, 70.f}, {47, 28.f}};
        for(const Change& c : changes)
        {
            cvar_t& var = cvarAt(c.slot, Key::Mass);
            if(!strcmp(cvarAt(c.slot, Key::ID).string, cvarAt(c.slot, Key::ID).default_string) &&
                za::fabs(static_cast<float>(atof(var.string)) - c.before) < 1e-4f)
            {
                Cvar_SetQuick(&var, var.default_string);
                Con_DPrintf("Held Object Offsets: %s: %s %s (was %g)\n", cvarAt(c.slot, Key::ID).string, var.name, var.string,
                    static_cast<double>(c.before));
            }
        }
    }
    // 59: a monster's head weighs its share of its ragdoll (Mass -1: box3d::headPropMass, the 7% of its class's
    // vr_ragdoll_<class>_mass a head cut off it weighs; the author, 2026-10-07: "bring the head's weights closer to the
    // ragdoll's"): a slot still its model's that holds 58's default takes -1 (a config's own weight is kept).
    if(from < 59)
    {
        struct Change
        {
            int slot;
            float before;
        };
        constexpr Change changes[] = {{7, 9.f}, {8, 50.f}, {37, 8.f}, {38, 9.f}, {39, 9.f}, {40, 8.f}, {41, 11.f},
            {42, 15.f}, {43, 10.f}, {44, 8.f}, {45, 10.f}, {46, 65.f}, {47, 18.f}};
        for(const Change& c : changes)
        {
            cvar_t& var = cvarAt(c.slot, Key::Mass);
            if(!strcmp(cvarAt(c.slot, Key::ID).string, cvarAt(c.slot, Key::ID).default_string) &&
                za::fabs(static_cast<float>(atof(var.string)) - c.before) < 1e-4f)
            {
                Cvar_SetQuick(&var, var.default_string);
                Con_DPrintf("Held Object Offsets: %s: %s %s (was %g)\n", cvarAt(c.slot, Key::ID).string, var.name, var.string,
                    static_cast<double>(c.before));
            }
        }
    }
    // 60: immersive reloading's rounds (slots 49 and 50: the shell from the ammo pouch and the taped pair; vr_reload.qc)
    // take their shipped settings, a model the menu had put there moving to a free slot.
    if(from < 60)
    {
        takeShippedSlot(48);
        takeShippedSlot(49);
    }
    // 61: the magazines (phase 2: slots 51-53, the nailgun's, the super nailgun's, the thunderbolt's cell).
    if(from < 61)
    {
        for(const int slot : {50, 51, 52})
        {
            takeShippedSlot(slot);
        }
    }
    // 62: vrstart's barrel (vr_barrel; make_crates.py) has slot 60 (vr_prop_*_61): it takes its shipped settings, a
    // model the menu had put there moving to a free slot.
    if(from < 62)
    {
        takeShippedSlot(60);
    }
    // 63: the magazines' sizes (the author's) and their grip (top up, as for loading) in slots 51-53.
    if(from < 63)
    {
        for(const int slot : {50, 51, 52})
        {
            takeShippedSlot(slot);
        }
    }
    // 64: the live shell's grip (slot 49) the author's: up 0.5 units, centred.
    if(from < 64)
    {
        takeShippedSlot(48);
    }
    // 65: the gremlin's head (slot 7) at Size 0.6, the author's (2026-10-07), where a config still has the old 1.
    if(from < 65 && !strcmp(cvarAt(7, Key::ID).string, cvarAt(7, Key::ID).default_string) &&
       atof(cvarAt(7, Key::Size).string) == 1.0)
    {
        Cvar_SetQuick(&cvarAt(7, Key::Size), cvarAt(7, Key::Size).default_string);
    }
    // 66: the launchers' rounds (immersive reloading's front loading: slots 54-56, the rocket, the grenade, the
    // proximity grenade).
    if(from < 66)
    {
        for(const int slot : {53, 54, 55})
        {
            takeShippedSlot(slot);
        }
    }
    // 67: the proximity grenade (slot 56: progs/proxbomb.mdl, in the palm; it held make_rounds.py's vr_round_prox.mdl,
    // no longer drawn: its settings go, not to a free slot).
    if(from < 67)
    {
        if(cvar_t& id = cvarAt(55, Key::ID); !strcmp(id.string, "progs/vr_round_prox.mdl"))
        {
            Cvar_SetQuick(&id, id.default_string);
        }
        takeShippedSlot(55);
    }
    // 68: the author's grenade fit (slot 4, progs/grenade.mdl: note vrfiringrange_2026-10-08_14-16-19, "make them the new
    // defaults"): a slot still its model's that holds the old value takes the new one.
    if(from < 68 && !strcmp(cvarAt(3, Key::ID).string, cvarAt(3, Key::ID).default_string))
    {
        struct Change
        {
            Key key;
            float before;
        };
        constexpr Change changes[] = {{Key::GripX, 0.f}, {Key::GripY, 0.f}, {Key::Overlap, 0.75f}};
        for(const Change& c : changes)
        {
            cvar_t& var = cvarAt(3, c.key);
            if(atof(var.string) == c.before)
            {
                Cvar_SetQuick(&var, var.default_string);
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

// A model's own size, as a Size (times its slot's): Quake's grenades (vr_grenade_scale: the grenade, the multi-grenade;
// vr_prox_scale: the proximity grenade) and the rocket in flight (vr_rocket_scale), by its name.
[[nodiscard]] float ownScale(const char* name)
{
    if(!name)
    {
        return 1.f;
    }
    if(!strcmp(name, "progs/proxbomb.mdl"))
    {
        return za::clamp(vr_prox_scale.value, 0.2f, 2.f);
    }
    if(!strcmp(name, "progs/grenade.mdl") || !strcmp(name, "progs/mervup.mdl"))
    {
        return za::clamp(vr_grenade_scale.value, 0.25f, 2.f);
    }
    if(!strcmp(name, "progs/missile.mdl"))
    {
        return za::clamp(vr_rocket_scale.value, 0.25f, 2.f);
    }
    return 1.f;
}

} // namespace

float size(int slot)
{
    const float own = slot >= 0 && slot < numSlots ? ownScale(cvarAt(slot, Key::ID).string) : 1.f;
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
    return slot >= 0 ? size(slot) : size(slot) * ownScale(model->name);
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

const char* keyName(Key key)
{
    return static_cast<int>(key) < numKeys ? keyNames[static_cast<int>(key)] : "";
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

float throwScale(int slot, float mass)
{
    if(const float own = value(slot, Key::Throw); own > 0.f)
    {
        return own;
    }
    if(vr_throw_mass_model.value)
    {
        return 1.f; // throws by weight limit it by its mass (weight::throwVelocity)
    }
    const float free = za::max(vr_weight_throw_mass.value, 0.1f);
    return mass > free ? za::sqrt(free / mass) : 1.f;
}

} // namespace qvr::props
