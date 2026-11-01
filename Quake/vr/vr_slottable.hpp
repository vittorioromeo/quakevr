#pragma once

// vr_slottable.hpp -- the data tables (design review 3): per-slot values (`Slots` slots, each with the same `Keys` keys)
// read from a data file, with no cvars: the weapons' (quakevr/data/weapons.txt, vr_weapons.cpp) and the props'
// (quakevr/data/props.txt, vr_props.cpp). docs/vr-port/CODE_STYLE.md, "Canonical helpers".

#include "vr_engine.hpp"

#include "Zancle/Base/PtrDiffT.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Array.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/StringView.hpp"
#include "Zancle/Vocabulary/Span.hpp"

namespace qvr
{

// A data file: a `defaults { key "value" ... }` block (every slot's value of a key; a key not listed: "0"), then
// `<block> <slot> { key "value" ... }` blocks (a slot's own values; slots from 0). `//` comments anywhere.
struct DataFileSpec
{
    const char* path;                     // in the game folders: "data/weapons.txt"
    const char* block;                    // a slot's block's word: "weapon"
    const char* keysFile;                 // where the keys are defined (its messages): "vr_weapons.inc"
    int slots;                            // the slots 0 .. slots - 1
    za::Span<const char* const> keyNames; // the keys' names in the file, by key
};

struct DataFileValue
{
    int slot;
    int key;
    za::String value;
};

// `spec`'s file read (vr_datafile.cpp): `defaults` each key's value in its `defaults` block ("0" if not there), `own`
// each slot's block's values in the file's order. False: no file. A line it can't take is reported and skipped.
bool readDataFile(const DataFileSpec& spec, za::Span<za::String> defaults, za::Vector<DataFileValue>& own);

// A table of per-slot values read from a data file, with no cvars: each value's text, number and default are kept in an
// unregistered cvar_t, its stand-in. The menus' rows bind the stand-in, the code reads its ->string and ->value, and the
// config, the console and the QC reach it by its old cvar name (Cvar_FindVar's VR_CvarStandIn; Cvar_SetQuick of it
// comes to set(): VR_StandInSet). Not in the cvar list: never registered, completed or written by Cvar_WriteVariables
// (VR_ConfigWriteTables writes the changed ones).
template <typename Key, int Slots, int Keys>
class DataTable
{
public:
    static constexpr int size = Slots * Keys;

    [[nodiscard]] cvar_t& at(int slot, Key key) { return cvars[slot * Keys + static_cast<int>(key)]; }

    // Every slot's values from `spec`'s file (the defaults; the config's lines go over them), each named
    // `<stem><key>_NN` (NN = slot + 1), `changed` called after each change. False: no file (every value "0").
    bool load(const DataFileSpec& spec, const char* stem, cvarcallback_t changed)
    {
        za::Vector<DataFileValue> own;
        const bool found = readDataFile(spec, {keyDefaults.data(), keyDefaults.size()}, own);
        za::Vector<const char*> value;
        value.resize(size, nullptr);
        for(int i = 0; i < size; i++)
        {
            value[i] = keyDefaults[i % Keys].cStr();
        }
        for(const DataFileValue& o : own)
        {
            value[o.slot * Keys + o.key] = o.value.cStr(); // (a key given twice: the last)
        }
        for(int slot = 0; slot < Slots; slot++)
        {
            for(int key = 0; key < Keys; key++)
            {
                define(slot, key, va("%s%s_%02d", stem, spec.keyNames[key], slot + 1), value[slot * Keys + key], changed);
            }
        }
        return found;
    }

    // The data file's `defaults` block's value of `key` (every slot's unless its block has its own).
    [[nodiscard]] const char* keyDefault(int key) const { return keyDefaults[key].cStr(); }

    // Slot `slot`'s value of key `key` named `name` (lower case), its default and value `value`; `changed` is called
    // after each change, as a registered cvar's callback.
    void define(int slot, int key, za::StringView name, za::StringView value, cvarcallback_t changed)
    {
        const int i = slot * Keys + key;
        names[i] = name;
        defaults[i] = value;
        texts[i] = value;
        cvar_t& var = cvars[i];
        var.name = names[i].cStr();
        var.string = texts[i].cStr();
        var.default_string = defaults[i].cStr();
        var.value = Q_atof(var.string);
        var.flags = CVAR_ARCHIVE;
        var.callback = changed;
        byName[names[i]] = i;
    }

    // `var` set to `value` as Cvar_SetQuick sets a registered cvar (nothing on the same text; the callback after a
    // change). False: `var` is not one of the table's.
    bool set(cvar_t* var, const char* value)
    {
        const za::PtrDiffT i = var - cvars.data();
        if(i < 0 || i >= size)
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

    // The stand-in named `name` (any case), or null.
    [[nodiscard]] cvar_t* find(const char* name)
    {
        char lower[96];
        q_strlcpy(lower, name, sizeof(lower));
        q_strlwr(lower);
        const auto it = byName.find(za::String{lower});
        return it == byName.end() ? nullptr : &cvars[it->second];
    }

    [[nodiscard]] int keyOf(const cvar_t* var) const
    {
        const za::PtrDiffT index = var - cvars.data();
        return index >= 0 && index < size ? static_cast<int>(index % Keys) : -1;
    }

    [[nodiscard]] cvar_t* begin() { return cvars.data(); }
    [[nodiscard]] cvar_t* end() { return cvars.data() + size; }

private:
    za::Array<za::String, Slots * Keys> names;
    za::Array<za::String, Slots * Keys> texts;
    za::Array<za::String, Slots * Keys> defaults;
    za::Array<za::String, Keys> keyDefaults;
    za::Array<cvar_t, Slots * Keys> cvars{};
    ankerl::unordered_dense::map<za::String, int> byName;
};

} // namespace qvr
