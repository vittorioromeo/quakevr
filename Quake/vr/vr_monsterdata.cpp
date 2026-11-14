// vr_monsterdata.cpp -- see vr_monsterdata.hpp (the monsters' per-class values, quakevr/data/monsters.txt).

#include "vr_monsterdata.hpp"

#include "vr_monstervalues.hpp"
#include "vr_slottable.hpp"

#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Array.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/Vocabulary/Span.hpp"
#include "vr_api.h"

#include <string.h>

namespace qvr
{

// Each value's stand-in: its old cvar's name, archived; its text, number and default from monsters.txt (registerCvars).
cvar_t monsterValues[monsterValueCount] = {
#define QVR_MONSTER_VALUE(name, classname, key) {#name, "0", CVAR_ARCHIVE},
#include "vr_monsterdata.inc"
#undef QVR_MONSTER_VALUE
};

namespace monsterdata
{
namespace
{

struct Entry
{
    const char* classname;
    const char* key;
};

constexpr Entry entries[monsterValueCount] = {
#define QVR_MONSTER_VALUE(name, classname, key) {#classname, #key},
#include "vr_monsterdata.inc"
#undef QVR_MONSTER_VALUE
};

constexpr const char* dataPath = "data/monsters.txt";

za::Array<za::String, monsterValueCount> texts;    // each value's text now
za::Array<za::String, monsterValueCount> defaults; // ... and monsters.txt's
za::Vector<const char*> classnames;                // monsters.txt's blocks' names, in vr_monsterdata.inc's order
za::Vector<const char*> keyNames;                  // ... and its keys
// Looked up by a lowered name without making a za::String of it (findStandIn: the lunges' QC, every few frames).
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
ankerl::unordered_dense::map<za::String, int, NameHash, NameEqual> byName; // a value's index by its old cvar name

[[nodiscard]] int indexOf(za::Vector<const char*>& names, const char* name)
{
    for(za::SizeT i = 0; i < names.size(); i++)
    {
        if(!strcmp(names[i], name))
        {
            return static_cast<int>(i);
        }
    }
    names.pushBack(name);
    return static_cast<int>(names.size() - 1);
}

[[nodiscard]] bool changed(int i)
{
    return strcmp(monsterValues[i].string, monsterValues[i].default_string) != 0;
}

// vr_monster_dump: each classname's values in monsters.txt and how many of them differ from it;
// vr_monster_dump <classname> ... (or all): each value, its old cvar name and monsters.txt's default.
void dump_f()
{
    if(Cmd_Argc() < 2)
    {
        for(const char* cls : classnames)
        {
            int n = 0;
            int own = 0;
            for(int i = 0; i < monsterValueCount; i++)
            {
                if(!strcmp(entries[i].classname, cls))
                {
                    n++;
                    own += changed(i);
                }
            }
            Con_Printf("%-22s %2d values, %d changed\n", cls, n, own);
        }
        Con_Printf("vr_monster_dump <classname> ... | all: every value (%s)\n", dataPath);
        return;
    }
    const bool all = !q_strcasecmp(Cmd_Argv(1), "all");
    for(const char* cls : classnames)
    {
        bool wanted = all;
        for(int a = 1; a < Cmd_Argc() && !wanted; a++)
        {
            wanted = !strcmp(Cmd_Argv(a), cls);
        }
        if(!wanted)
        {
            continue;
        }
        Con_Printf("monster %s\n", cls);
        for(int i = 0; i < monsterValueCount; i++)
        {
            if(!strcmp(entries[i].classname, cls))
            {
                const cvar_t& v = monsterValues[i];
                Con_Printf("    %s \"%s\" // %s, default \"%s\"%s\n", entries[i].key, v.string, v.name, v.default_string,
                    changed(i) ? " *" : "");
            }
        }
    }
}

} // namespace

void registerCvars()
{
    classnames.clear();
    keyNames.clear();
    za::Array<int, monsterValueCount> slotOf{};
    za::Array<int, monsterValueCount> keyOf{};
    for(int i = 0; i < monsterValueCount; i++)
    {
        slotOf[i] = indexOf(classnames, entries[i].classname);
        keyOf[i] = indexOf(keyNames, entries[i].key);
    }
    const int numKeys = static_cast<int>(keyNames.size());
    const DataFileSpec spec{dataPath, "monster", "vr_monsterdata.inc", static_cast<int>(classnames.size()),
        {keyNames.data(), keyNames.size()}, {classnames.data(), classnames.size()}};
    za::Vector<za::String> keyDefaults;
    keyDefaults.resize(keyNames.size());
    za::Vector<DataFileValue> own;
    if(!readDataFile(spec, {keyDefaults.data(), keyDefaults.size()}, own))
    {
        Con_Printf("\x02No %s in the game folder: every monster's per-class value is 0 (reinstall the game)\n", dataPath);
    }
    za::Vector<const char*> value; // by slot * numKeys + key: the block's value, else the defaults block's
    value.resize(classnames.size() * keyNames.size(), nullptr);
    for(const DataFileValue& o : own)
    {
        value[o.slot * numKeys + o.key] = o.value.cStr(); // (a key given twice: the last)
    }
    byName.clear();
    for(int i = 0; i < monsterValueCount; i++)
    {
        const char* v = value[slotOf[i] * numKeys + keyOf[i]];
        defaults[i] = v ? v : keyDefaults[keyOf[i]].cStr();
        texts[i] = defaults[i];
        cvar_t& var = monsterValues[i];
        var.string = texts[i].cStr();
        var.default_string = defaults[i].cStr();
        var.value = Q_atof(var.string); // (its flags, CVAR_ARCHIVE, and a callback set already kept)
        byName[za::String{var.name}] = i;
    }
    Cmd_AddCommand("vr_monster_dump", dump_f);
}

cvar_t* findStandIn(const char* name)
{
    char lower[96];
    q_strlcpy(lower, name, sizeof(lower));
    q_strlwr(lower);
    const auto it = byName.find(za::StringView{lower});
    return it == byName.end() ? nullptr : &monsterValues[it->second];
}

bool setStandIn(cvar_t* var, const char* value)
{
    const za::PtrDiffT i = var - monsterValues;
    if(i < 0 || i >= monsterValueCount)
    {
        return false;
    }
    if(!strcmp(var->string, value))
    {
        return true;
    }
    VR_NoteCvarChanging(var, value);
    za::String text{value};
    texts[i] = ZA_MOVE(text);
    var->string = texts[i].cStr();
    var->value = Q_atof(var->string);
    if(var->callback)
    {
        var->callback(var);
    }
    return true;
}

za::Span<cvar_t> standIns()
{
    return {monsterValues, static_cast<za::SizeT>(monsterValueCount)};
}

} // namespace monsterdata
} // namespace qvr
