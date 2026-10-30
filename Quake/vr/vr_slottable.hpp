#pragma once

// vr_slottable.hpp -- a table of per-slot cvars (`Slots` slots, each with the same `Keys` keys): the weapons'
// vr_wofs_<key>_NN (vr_weapons.cpp) and the props' vr_prop_<key>_NN (vr_props.cpp). It keeps the cvars and their names
// (a cvar's name must outlive it). docs/vr-port/CODE_STYLE.md, "Canonical helpers".

#include "vr_engine.hpp"

#include "Zancle/Base/PtrDiffT.hpp"
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

} // namespace qvr
