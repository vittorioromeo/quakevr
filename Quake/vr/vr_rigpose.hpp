// vr_rigpose.hpp -- a ragdoll rig's bones posed by the model's own frames (vr_ragdoll.hpp's Rig::poseRot / posePos), and
// between frames along a spline: r_lerpmodels 3's limbs (vr_animsmooth.cpp). Small helpers shared by the renderer's
// smoothing and the ragdolls (the animated bones an active ragdoll follows); the rig itself is vr_ragdoll.cpp's.
//
// The spline: a cubic segment from key 1 to key 2 of four keys (0 the one before, 3 the one expected after), each a frame
// shown for a span of time (`span`: 0..1, 1..2, 2..3, seconds), its tangents the keys' non-uniform Catmull-Rom ones
// (the speed carried through each key, whatever the spans), scaled by 1 - tension (1: easing to rest at each key). At
// t 0 it is key 1 exactly, at t 1 key 2 exactly: its weights are (0, 1, 0, 0) and (0, 0, 1, 0).

#pragma once

#include "vr_ragdoll.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace qvr::rigpose
{

// A rigid transform, p' = rot * p + pos (the model's units).
struct Rigid
{
    glm::quat rot{1.f, 0.f, 0.f, 0.f};
    glm::vec3 pos{0.f};
};

[[nodiscard]] inline glm::vec3 apply(const Rigid& r, const glm::vec3& p)
{
    return r.rot * p + r.pos;
}

// Bone `b`'s transform from the rest pose to pose `pose` (the rig's fit).
[[nodiscard]] Rigid key(const ragdoll::Rig& rig, int pose, int b);

// The weights of a segment at `t` (0..1) through four keys: a point's place is w[0] p0 + w[1] p1 + w[2] p2 + w[3] p3. A
// rotation's (rotation()) goes by the same tangents: rot[k] scales the turn from key k to key k + 1.
struct Spline
{
    float w[4]{0.f, 1.f, 0.f, 0.f};
    float rot[3]{0.f, 0.f, 0.f};
};
[[nodiscard]] Spline spline(float t, const float span[3], float tension);

// The rotation vector (axis times angle, radians) of `q`, and back.
[[nodiscard]] glm::vec3 toVector(const glm::quat& q);
[[nodiscard]] glm::quat fromVector(const glm::vec3& v);

// Four keys' rotations along the spline (each turn the short way round).
[[nodiscard]] glm::quat rotation(const glm::quat q[4], const Spline& s);

// The rig's bones along the spline through poses `poses` (four): each bone's rotation by rotation(), its joint with its
// parent carried by the parent as posed here, plus the gap the fits leave there (the keys' along the spline: at a key, the
// key's own transform); a root, a loose bone or one whose parent is in `alone` (bits): its pivot along the spline. `out`:
// numBones of them.
void along(const ragdoll::Rig& rig, const int poses[4], const Spline& s, uint32_t alone, Rigid* out);
// ... with the bones' keys from `keys` ([pose * numBones + bone]; nullptr: key()'s).
void along(const ragdoll::Rig& rig, const Rigid* keys, const int poses[4], const Spline& s, uint32_t alone, Rigid* out);

} // namespace qvr::rigpose
