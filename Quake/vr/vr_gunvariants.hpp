#pragma once

// One model per gun (vr_gunvariants.inc; ROUND25.md, "onemodel"): a gun's other-ammo look (the lava nailguns, the
// multi-rocket launcher, the plasma Thunderbolt) is its base model with another skin. What the engine keeps per model
// is the base's for a variant: look a model's name up through resolve() where a table is keyed by it.


namespace qvr::gunvariants
{

struct Variant
{
    const char* variant; // the name drawn for the look (QC's weapon table)
    const char* base;    // its gun's model
};

inline constexpr Variant table[] = {
#define QVR_GUN_VARIANT(variant, base, skin) {variant, base},
#include "vr_gunvariants.inc"
#include "Zancle/Base/Strcmp.hpp"
#undef QVR_GUN_VARIANT
};

// The base model's name of the variant model `name`, or nullptr when `name` is no variant.
[[nodiscard]] inline const char* base(const char* name)
{
    for(const Variant& v : table)
    {
        if(!ZA_STRCMP(v.variant, name))
        {
            return v.base;
        }
    }
    return nullptr;
}

// `name`'s base when it is a variant, else `name`.
[[nodiscard]] inline const char* resolve(const char* name)
{
    const char* b = base(name);
    return b ? b : name;
}

} // namespace qvr::gunvariants
