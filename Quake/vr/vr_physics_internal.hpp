#pragma once
// vr_physics_internal.hpp -- what vr_physics.cpp (see vr_physics.hpp) shares with its water half, vr_physics_water.cpp.
// Internal: included by those two alone. The small helpers both use (once the file's anonymous namespace) are
// namespace physics::detail, defined here (inline); the water functions vr_physics.cpp calls are declared here and
// defined in vr_physics_water.cpp.
#include "vr_geom.hpp"
#include "vr_progs.hpp"
#include "Zancle/Base/IsFinite.hpp"

namespace qvr::physics::detail
{

[[nodiscard]] inline bool active()
{
    return progs::bindings().isVrProgs;
}

[[nodiscard]] inline const progs::FieldOffsets& f()
{
    return progs::fields();
}

using geom::vec;

[[nodiscard]] inline bool finite(const glm::vec3& v)
{
    return ZA_ISFINITE(v.x) && ZA_ISFINITE(v.y) && ZA_ISFINITE(v.z);
}

[[nodiscard]] inline bool hasFlag(edict_t* ent, int flag)
{
    return (static_cast<int>(ent->v.flags) & flag) != 0;
}

[[nodiscard]] inline int solidOf(edict_t* ent)
{
    return static_cast<int>(ent->v.solid);
}

[[nodiscard]] inline bool isClient(edict_t* ent)
{
    return hasFlag(ent, FL_CLIENT);
}

// vr_physics_water.cpp: hands and guns slapping the water, wading (VR_ClientPreMove, every frame).
void waterFeedback(edict_t* ent);

} // namespace qvr::physics::detail
