// vr_tunables.hpp -- the tuning constants by name (vr_tunables.inc's QVR_TUNABLE(name, default)): include it only where
// one is read (vr_x.value), so an edit of the table recompiles those files alone. Their type and functions:
// vr_tunables_api.hpp.

#pragma once

#include "vr_tunables_api.hpp"

namespace qvr
{

// Each constant's number: its line in vr_tunables.inc (tunableId_<name>).
enum TunableId : int
{
#define QVR_TUNABLE(name, def) tunableId_##name,
#include "vr_tunables.inc"
#undef QVR_TUNABLE
    tunableCount
};

#ifdef QVR_RELEASE
#define QVR_TUNABLE(name, def) inline constexpr Tunable name{#name, def, tunableValue(def)};
#else
extern Tunable tunables[tunableCount]; // (vr_tunables.cpp) the values now: the defaults, or what tune set
#define QVR_TUNABLE(name, def) inline constexpr Tunable& name = tunables[tunableId_##name];
#endif
#include "vr_tunables.inc"
#undef QVR_TUNABLE

} // namespace qvr
