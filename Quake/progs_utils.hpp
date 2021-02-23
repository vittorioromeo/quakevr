#pragma once

#include "progs.hpp"
#include "quakeglm_qvec3.hpp"
#include "macros.hpp"
#include "qcvm.hpp"

#define RETURN_EDICT(e) (((int*)qcvm->globals)[OFS_RETURN] = EDICT_TO_PROG(e))

[[nodiscard]] QUAKE_FORCEINLINE static qvec3 extractVector(
    const int parm) noexcept
{
    float* const ptr = G_VECTOR(parm);
    return {ptr[0], ptr[1], ptr[2]};
}

QUAKE_FORCEINLINE static void returnVector(const qvec3& v) noexcept
{
    G_VECTOR(OFS_RETURN)[0] = v[0];
    G_VECTOR(OFS_RETURN)[1] = v[1];
    G_VECTOR(OFS_RETURN)[2] = v[2];
}
