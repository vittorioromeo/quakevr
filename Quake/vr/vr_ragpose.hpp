// vr_ragpose.hpp -- a rig's poses as a ragdoll's joints see them (header only; ROUND22.md, "Active ragdolls: core and
// deaths"): a bone between two of the rig's poses, the frame its joint has in both parts (vr_box3d.cpp createRagdoll),
// the joint's rotation in a pose as Box3D's target (frame B relative to frame A), a quaternion's swing and twist about
// its frame's z, a two-bone limb bent to reach a point. Shared by the active drive (vr_ragactive.cpp) and the ragdolls'
// making; the rig itself (vr_ragdoll.cpp) is unchanged.

#pragma once

#include "vr_ragdoll.hpp"

#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Acos.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace qvr::ragpose
{

// A rotation that takes +z to `d` (unit).
[[nodiscard]] inline glm::quat zTo(const glm::vec3& d)
{
    const glm::vec3 z{0.f, 0.f, 1.f};
    const float c = glm::dot(z, d);
    if(c < -0.9999f)
    {
        return glm::quat{0.f, 1.f, 0.f, 0.f};
    }
    const glm::vec3 axis = glm::cross(z, d);
    return glm::normalize(glm::quat{1.f + c, axis.x, axis.y, axis.z});
}

// The frame bone `bone`'s joint has in its parent's and its own rest space (both alike: the parts are made in a pose of
// the rig, each turned by its bone's rotation): z along the hinge's axis (a hinge's angle is its turn about z), or along
// the bone's rest direction (a ball's cone is about z).
[[nodiscard]] inline glm::quat jointFrame(const ragdoll::Bone& bone)
{
    if(bone.joint == ragdoll::Joint::Hinge)
    {
        return zTo(glm::normalize(bone.hinge));
    }
    const glm::vec3 along = bone.end - bone.pivot;
    return zTo(glm::length(along) > 1e-3f ? glm::normalize(along) : glm::vec3{0.f, 0.f, 1.f});
}

// `q` or `-q`, whichever is nearer `ref` (a shortest blend).
[[nodiscard]] inline glm::quat nearTo(const glm::quat& q, const glm::quat& ref)
{
    return glm::dot(q, ref) < 0.f ? -q : q;
}

// Bone `b`'s transform (rest -> pose, model space) a share `t` of the way from pose `a` to pose `b2`: its turn blended
// the short way (normalised lerp: frames are close), its place lerped.
// (Also the two poses' own turns, `ra` and `rb`: a caller that wants them too doesn't look them up again.)
inline void bonePoseLerp(const ragdoll::Rig& rig, int a, int b2, float t, int b, glm::quat& rot, glm::vec3& pos,
    glm::quat& ra, glm::quat& rb)
{
    glm::vec3 pa, pb;
    ragdoll::bonePose(rig, a, b, ra, pa);
    ragdoll::bonePose(rig, b2, b, rb, pb);
    rot = glm::normalize(ra * (1.f - t) + nearTo(rb, ra) * t);
    pos = pa + (pb - pa) * t;
}

inline void bonePoseLerp(const ragdoll::Rig& rig, int a, int b2, float t, int b, glm::quat& rot, glm::vec3& pos)
{
    glm::quat ra, rb;
    bonePoseLerp(rig, a, b2, t, b, rot, pos, ra, rb);
}

// A joint's rotation (the child's turn in its parent's rest space) as Box3D's target: frame B relative to frame A.
[[nodiscard]] inline glm::quat toJoint(const glm::quat& frame, const glm::quat& relative)
{
    return glm::normalize(glm::inverse(frame) * relative * frame);
}

// And back: the child's turn in its parent's rest space a joint target stands for.
[[nodiscard]] inline glm::quat fromJoint(const glm::quat& frame, const glm::quat& target)
{
    return glm::normalize(frame * target * glm::inverse(frame));
}

// The turn of `q` about z (rad, -pi..pi: Box3D's b3GetTwistAngle, a hinge's angle).
[[nodiscard]] inline float twistZ(const glm::quat& q)
{
    return 2.f * (q.w < 0.f ? za::atan2(-q.z, -q.w) : za::atan2(q.z, q.w));
}

// `q` (a joint's target) kept inside a cone of `cone` about z and a twist of +-`twist` (rad): its swing (the turn that
// takes z where it goes) and its twist about z clamped apart, put back together.
[[nodiscard]] inline glm::quat clampSwingTwist(const glm::quat& q, float cone, float twist)
{
    const float tw = za::clamp(twistZ(q), -twist, twist);
    const glm::quat twistQ = glm::angleAxis(twistZ(q), glm::vec3{0.f, 0.f, 1.f});
    glm::quat swing = glm::normalize(q * glm::inverse(twistQ));
    if(swing.w < 0.f)
    {
        swing = -swing;
    }
    const float swingAngle = 2.f * za::acos(za::clamp(swing.w, -1.f, 1.f));
    if(swingAngle > cone && swingAngle > 1e-5f)
    {
        const glm::vec3 axis = glm::normalize(glm::vec3{swing.x, swing.y, swing.z});
        swing = glm::angleAxis(cone, axis);
    }
    return glm::normalize(swing * glm::angleAxis(tw, glm::vec3{0.f, 0.f, 1.f}));
}

// The rotation vector (axis times angle, rad) of `q`, the short way.
[[nodiscard]] inline glm::vec3 rotationVector(glm::quat q)
{
    if(q.w < 0.f)
    {
        q = -q;
    }
    const glm::vec3 im{q.x, q.y, q.z};
    const float s = glm::length(im);
    if(s < 1e-7f)
    {
        return im * 2.f;
    }
    return im * (2.f * za::atan2(s, q.w) / s);
}

// The shortest rotation taking direction `a` to direction `b` (neither need be unit; none if either is ~0).
[[nodiscard]] inline glm::quat between(const glm::vec3& a, const glm::vec3& b)
{
    const float la = glm::length(a), lb = glm::length(b);
    if(la < 1e-6f || lb < 1e-6f)
    {
        return glm::quat{1.f, 0.f, 0.f, 0.f};
    }
    const glm::vec3 u = a / la, v = b / lb;
    const float c = glm::dot(u, v);
    if(c < -0.9999f)
    {
        glm::vec3 axis = glm::cross(u, glm::vec3{1.f, 0.f, 0.f});
        if(glm::length(axis) < 1e-3f)
        {
            axis = glm::cross(u, glm::vec3{0.f, 1.f, 0.f});
        }
        return glm::angleAxis(3.14159265f, glm::normalize(axis));
    }
    const glm::vec3 axis = glm::cross(u, v);
    return glm::normalize(glm::quat{1.f + c, axis.x, axis.y, axis.z});
}

// A two-bone limb's bend to reach: the upper bone from `pivotU` (its joint), the lower one jointed at `pivotL` and ending
// at `end` (rest space, units), the lower turning about `hinge` (rest space, unit) by an angle in [lo, hi]. The angle
// whose reach (pivotU to the lower bone's end) is nearest `reach` (units), and that reach's vector in the upper bone's
// rest space (the lower bent so).
inline float bendFor(const glm::vec3& pivotU, const glm::vec3& pivotL, const glm::vec3& end, const glm::vec3& hinge,
    float lo, float hi, float reach, glm::vec3& vector)
{
    const auto at = [&](float angle) { return (pivotL - pivotU) + glm::angleAxis(angle, hinge) * (end - pivotL); };
    // A coarse look over the range, then halving round the best (the reach need not grow evenly with the bend: the
    // rig's limbs are not straight at rest).
    constexpr int steps = 16;
    float best = lo, bestMiss = 1e30f;
    for(int i = 0; i <= steps; i++)
    {
        const float a = lo + (hi - lo) * static_cast<float>(i) / static_cast<float>(steps);
        const float miss = za::abs(glm::length(at(a)) - reach);
        if(miss < bestMiss)
        {
            bestMiss = miss;
            best = a;
        }
    }
    float span = (hi - lo) / static_cast<float>(steps);
    for(int i = 0; i < 8; i++)
    {
        span *= 0.5f;
        for(const float a : {best - span, best + span})
        {
            if(a < lo || a > hi)
            {
                continue;
            }
            const float miss = za::abs(glm::length(at(a)) - reach);
            if(miss < bestMiss)
            {
                bestMiss = miss;
                best = a;
            }
        }
    }
    vector = at(best);
    return best;
}

} // namespace qvr::ragpose
