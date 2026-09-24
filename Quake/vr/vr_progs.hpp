// vr_progs.hpp -- Quake VR's view of the server QuakeC VM.
//
// The VR progs keep the vanilla system-defs layout (CRC 5927); everything VR-specific is an
// ordinary QC field, global or function that the engine resolves by name when progs load.
// A progs without them (e.g. vanilla id1) simply runs with VR gameplay disabled.

#pragma once

#include "vr_engine.hpp"

namespace qvr::progs
{

inline constexpr int firstExtSpawnParm = 17;
inline constexpr int lastExtSpawnParm = 40;
inline constexpr int numExtSpawnParms = lastExtSpawnParm - firstExtSpawnParm + 1;

// Resolved for sv.qcvm each time progs are loaded.
struct Bindings
{
    bool isVrProgs{false};

    func_t OnSpawnServerBeforeLoad{0};
    func_t OnSpawnServerAfterLoad{0};
    func_t OnLoadGame{0};

    float* spawnServerFromSaveFile{nullptr};
    float* extSpawnParms[numExtSpawnParms]{};
};

[[nodiscard]] const Bindings& bindings();

// Lookups on the current qcvm (nullptr / 0 when absent).
[[nodiscard]] ddef_t* findGlobalDef(const char* name);
[[nodiscard]] func_t findFunction(const char* name);

// Binds the VR builtins declared "= #0" in QC (vr_builtins.cpp).
void bindBuiltins();

// Clears per-map builtin state (world text, ...) when a new server spawns.
void resetBuiltinState();

} // namespace qvr::progs
