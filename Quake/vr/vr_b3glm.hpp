// vr_b3glm.hpp -- conversions between Box3D's and glm's vectors and quaternions (header only; vr_box3d.cpp,
// vr_ragactive.cpp).

#pragma once

#include <box3d/box3d.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace qvr::b3glm
{

[[nodiscard]] inline b3Quat toB3(const glm::quat& q)
{
    return b3Quat{b3Vec3{q.x, q.y, q.z}, q.w};
}

// (Normalized: Box3D's quaternions drift a little from unit length.)
[[nodiscard]] inline glm::quat fromB3(const b3Quat& q)
{
    return glm::normalize(glm::quat{q.s, q.v.x, q.v.y, q.v.z});
}

[[nodiscard]] inline b3Vec3 b3v(const glm::vec3& v)
{
    return b3Vec3{v.x, v.y, v.z};
}

[[nodiscard]] inline glm::vec3 glmv(const b3Vec3& v)
{
    return glm::vec3{v.x, v.y, v.z};
}

} // namespace qvr::b3glm
