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

// Offsets (in floats, from edict_t::v) of the VR entity fields; -1 when absent.
struct FieldOffsets
{
#define QVR_FIELD(name) int name{-1};
#include "vr_fields.inc"
#undef QVR_FIELD
};

// Resolved for sv.qcvm each time progs are loaded.
struct Bindings
{
    bool isVrProgs{false};
    FieldOffsets fields;

    func_t OnSpawnServerBeforeLoad{0};
    func_t OnSpawnServerAfterLoad{0};
    func_t OnLoadGame{0};

    float* spawnServerFromSaveFile{nullptr};
    float* extSpawnParms[numExtSpawnParms]{};
};

[[nodiscard]] const Bindings& bindings();

[[nodiscard]] inline const FieldOffsets& fields()
{
    return bindings().fields;
}

// Field accessors. Callers must check the offset is valid (isVrProgs guarantees the fields
// the engine needs; fall back gracefully for optional ones).
[[nodiscard]] inline float* fieldPtr(edict_t* ent, int ofs)
{
    return reinterpret_cast<float*>(&ent->v) + ofs;
}

[[nodiscard]] inline float& fieldFloat(edict_t* ent, int ofs)
{
    return *fieldPtr(ent, ofs);
}

[[nodiscard]] inline int& fieldInt(edict_t* ent, int ofs) // func_t, string_t, entity
{
    return *reinterpret_cast<int*>(fieldPtr(ent, ofs));
}

[[nodiscard]] inline float fieldFloatOr(edict_t* ent, int ofs, float fallback)
{
    return ofs >= 0 ? fieldFloat(ent, ofs) : fallback;
}

// Lookups on the current qcvm (nullptr / 0 when absent).
[[nodiscard]] ddef_t* findGlobalDef(const char* name);
[[nodiscard]] func_t findFunction(const char* name);

// Binds the VR builtins declared "= #0" in QC (vr_builtins.cpp).
void bindBuiltins();

// Clears per-map builtin state (world text, ...) when a new server spawns.
void resetBuiltinState();

} // namespace qvr::progs
