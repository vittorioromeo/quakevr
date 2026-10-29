#pragma once

// vr_mdlbuild.hpp -- what the alias-model makers (ad guns, melee, weapons, gun art, limbs) share.

#include "vr_engine.hpp"

namespace qvr::mdlbuild
{

// The index of the engine's 162 vertex normals (the MDL's per-vertex byte) closest to the direction d (the first on
// ties).
[[nodiscard]] inline byte nearestNormal(const glm::vec3& d)
{
    int   best    = 0;
    float bestDot = -2.f;
    for(int i = 0; i < NUMVERTEXNORMALS; i++)
    {
        const float k = d.x * r_avertexnormals[i][0] + d.y * r_avertexnormals[i][1] + d.z * r_avertexnormals[i][2];
        if(k > bestDot)
        {
            bestDot = k;
            best    = i;
        }
    }
    return static_cast<byte>(best);
}

} // namespace qvr::mdlbuild
