#pragma once

// vr_slottable.hpp -- a table of per-slot cvars (`Slots` slots, each with the same `Keys` keys): the props'
// vr_prop_<key>_NN (vr_props.cpp). It keeps the cvars and their names (a cvar's name must outlive it).
// docs/vr-port/CODE_STYLE.md, "Canonical helpers". DataTable (below): the same table with no cvars, its values from a
// data file (the weapons': quakevr/data/weapons.txt, vr_weapons.cpp).

#include "vr_engine.hpp"

#include "Zancle/Base/PtrDiffT.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Array.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/StringView.hpp"

namespace qvr
{

template <typename Key, int Slots, int Keys>
class SlotTable
{
public:
    [[nodiscard]] cvar_t& at(int slot, Key key) { return cvars[slot * Keys + static_cast<int>(key)]; }

    // Slot `slot`'s cvar of key `key` set up: named `name`, its default `value`, archived (registered later).
    cvar_t& define(int slot, int key, za::StringView name, const char* value)
    {
        const int i = slot * Keys + key;
        names[i] = name;
        cvar_t& var = cvars[i];
        var.name = names[i].cStr();
        var.string = value;
        var.flags = CVAR_ARCHIVE;
        return var;
    }

    // `var`'s key in the table (-1: not one of its cvars).
    [[nodiscard]] int keyOf(const cvar_t* var) const
    {
        const za::PtrDiffT index = var - cvars.data();
        return index >= 0 && index < static_cast<za::PtrDiffT>(cvars.size()) ? static_cast<int>(index % Keys) : -1;
    }

    [[nodiscard]] cvar_t* begin() { return cvars.data(); }
    [[nodiscard]] cvar_t* end() { return cvars.data() + cvars.size(); }

private:
    za::Array<za::String, Slots * Keys> names;
    za::Array<cvar_t, Slots * Keys> cvars{};
};

// A table of per-slot values read from a data file (design review 3: the weapons' quakevr/data/weapons.txt), with no
// cvars: each value's text, number and default are kept in an unregistered cvar_t, its stand-in. The menus' rows bind
// the stand-in, the code reads its ->string and ->value, and the config, the console and the QC reach it by its old cvar
// name (Cvar_FindVar's VR_CvarStandIn; Cvar_SetQuick of it comes to set(): VR_StandInSet). Not in the cvar list: never
// registered, completed or written by Cvar_WriteVariables (VR_ConfigWriteTables writes the changed ones).
template <typename Key, int Slots, int Keys>
class DataTable
{
public:
    static constexpr int size = Slots * Keys;

    [[nodiscard]] cvar_t& at(int slot, Key key) { return cvars[slot * Keys + static_cast<int>(key)]; }

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
    za::Array<cvar_t, Slots * Keys> cvars{};
    ankerl::unordered_dense::map<za::String, int> byName;
};

} // namespace qvr
