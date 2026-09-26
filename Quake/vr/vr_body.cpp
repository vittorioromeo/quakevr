// vr_body.cpp -- see vr_body.hpp. Holster positions and hotspots from the old engine
// (VR_Get*HolsterPos, VR_In*HolsterDistance and VR_Move's computeHotSpot).

#include "vr_body.hpp"
#include "vr_engine.hpp"
#include "vr_avatar.hpp"
#include "vr_backend.hpp"
#include "vr_cvars.hpp"
#include "vr_lines.hpp"
#include "vr_units.hpp"

namespace qvr::body
{
namespace
{

[[nodiscard]] glm::vec3 negateY(glm::vec3 v)
{
    v.y = -v.y;
    return v;
}

// Holsters move forward as the player crouches.
[[nodiscard]] glm::vec3 crouchAdjustment(const hands::State& s, float mult)
{
    const float heightRatio = CLAMP(0.f, s.crouchRatio - 0.2f, 0.6f);
    return hands::forward({0.f, s.bodyYaw, 0.f}) * (heightRatio * mult);
}

[[nodiscard]] float threshold(Holster holster)
{
    switch(holster)
    {
        case LeftShoulder:
        case RightShoulder: return vr_shoulder_holster_thresh.value;
        case LeftHip:
        case RightHip: return vr_hip_holster_thresh.value;
        default: return vr_upper_holster_thresh.value;
    }
}

// The old engine's placement, relative to the player origin, the body's yaw and crouch.
[[nodiscard]] glm::vec3 legacyHolsterPosition(const hands::State& s, Holster holster)
{
    const glm::vec3 shoulder{vr_shoulder_offset_x.value, vr_shoulder_offset_y.value, vr_shoulder_offset_z.value};
    const glm::vec3 shoulderHolster{vr_shoulder_holster_offset_x.value, vr_shoulder_holster_offset_y.value,
        vr_shoulder_holster_offset_z.value};
    const glm::vec3 hip{vr_hip_offset_x.value, vr_hip_offset_y.value, vr_hip_offset_z.value};
    const glm::vec3 upper{vr_upper_holster_offset_x.value, vr_upper_holster_offset_y.value,
        vr_upper_holster_offset_z.value};

    switch(holster)
    {
        case LeftShoulder:
            return hands::bodyAnchor(s, negateY(shoulder + shoulderHolster)) + crouchAdjustment(s, 1.5f);
        case RightShoulder: return hands::bodyAnchor(s, shoulder + shoulderHolster) + crouchAdjustment(s, 1.5f);
        case LeftHip: return hands::bodyAnchor(s, negateY(hip)) + crouchAdjustment(s, -9.5f);
        case RightHip: return hands::bodyAnchor(s, hip) + crouchAdjustment(s, -9.5f);
        case LeftUpper: return hands::bodyAnchor(s, negateY(upper)) + crouchAdjustment(s, -1.5f);
        default: return hands::bodyAnchor(s, upper) + crouchAdjustment(s, -1.5f);
    }
}

// With the body drawn (vr_body_mode), the old placement (made for no body: a hand's width behind
// the eyes) is inside the torso, whose chest hid the hips and upper holsters from the eyes. They
// come forward onto the front of the body (the belly, the chest), and further as needed to be seen
// past the chest when looking down, by at most a little (with the torso right under the eyes,
// vr_body_torso_back 0, the chest still hides part of the hips). Further forward offsets
// (vr_hip_offset_x, vr_upper_holster_offset_x) still apply. In the standing body, which the
// Follower carries.
[[nodiscard]] glm::vec3 onTheBody(const hands::State& standing, Holster holster, const glm::vec3& pos)
{
    if(vr_body_mode.value < 1.f || holster == LeftShoulder || holster == RightShoulder)
    {
        return pos;
    }

    // make_vrbody.py: the spine hangs from the top of the neck (vr_body_eye_forward and
    // vr_body_eye_up from the eyes, then vr_body_torso_back), its rings centred 0.01 m behind it;
    // the belly (the pelvis ring) is 0.115 m deep, the chest 0.12-0.13 times the build's torso
    // scale, widest 0.22 m below the neck (1.35 m, the neck at 1.57).
    const float m2w = units::metresToUnits() * units::bodyScale();
    const int build = static_cast<int>(vr_body_build.value);
    const float torsoScale = build <= 0 ? 0.965f : build >= 2 ? 1.175f : 1.07f;
    const float axis = -(vr_body_eye_forward.value + vr_body_torso_back.value + 0.01f) * m2w; // from the eyes
    const float chestFront = axis + 0.13f * torsoScale * m2w;
    const float chestDrop = (vr_body_eye_up.value + 0.22f) * m2w;

    glm::vec3 fwd, right, up;
    hands::angleVectors({0.f, standing.bodyYaw, 0.f}, fwd, right, up);
    const float now = glm::dot(pos - standing.head, fwd);
    const bool hip = holster == LeftHip || holster == RightHip;
    const float onFront = axis + (hip ? 0.115f : 0.12f * torsoScale) * m2w + 1.5f; // clear of the surface

    // In front of the line from the eyes over the front of the chest.
    const float drop = standing.head.z - pos.z;
    const float seen = drop > chestDrop + 1.f ? chestFront * drop / chestDrop + 1.f : onFront;
    const float target = onFront + CLAMP(0.f, seen - onFront, 2.5f);
    return now < target ? pos + fwd * (target - now) : pos;
}

// With vr_body_anchors: where the holster is for the standing body, carried by the pelvis (hips)
// or the chest.
[[nodiscard]] glm::vec3 followingHolsterPosition(
    const avatar::Follower& follow, const hands::State& standing, Holster holster)
{
    const avatar::Part part = holster == LeftHip || holster == RightHip ? avatar::Part::Pelvis : avatar::Part::Chest;
    return follow(part, onTheBody(standing, holster, legacyHolsterPosition(standing, holster)));
}

[[nodiscard]] Hotspot hotspot(const hands::State& s, int hand, const HolsterPositions& holsters)
{
    const glm::vec3& pos = s.pos[hand];

    for(int h = 0; h < HolsterCount; h++)
    {
        const auto holster = static_cast<Holster>(h);
        if(glm::distance(pos, holsters[h]) < threshold(holster))
        {
            return holsterHotspot(holster);
        }
    }

    // Close enough to the other hand to steady its weapon (the "dynamic" 2H distance), or to
    // take the weapon from it.
    const float handDist = glm::distance(s.pos[HAND_OFF], s.pos[HAND_MAIN]);
    if(handDist > 5.f && handDist < 25.f)
    {
        return hand == HAND_OFF ? HS_OFFHAND_2H_GRAB : HS_MAINHAND_2H_GRAB;
    }

    if(handDist < 5.f)
    {
        return HS_HAND_SWITCH;
    }

    return HS_NONE;
}

} // namespace

glm::vec3 holsterPosition(const hands::State& s, Holster holster)
{
    if(!vr_body_anchors.value)
    {
        return legacyHolsterPosition(s, holster);
    }

    return followingHolsterPosition(avatar::Follower{s}, avatar::standing(s), holster);
}

HolsterPositions holsterPositions(const hands::State& s)
{
    HolsterPositions out;
    if(!vr_body_anchors.value)
    {
        for(int h = 0; h < HolsterCount; h++)
        {
            out[h] = legacyHolsterPosition(s, static_cast<Holster>(h));
        }
        return out;
    }

    const avatar::Follower follow{s};
    const hands::State standing = avatar::standing(s);
    for(int h = 0; h < HolsterCount; h++)
    {
        out[h] = followingHolsterPosition(follow, standing, static_cast<Holster>(h));
    }
    return out;
}

glm::vec3 chestAnchor(const hands::State& s, const glm::vec3& offsets)
{
    if(!vr_body_anchors.value)
    {
        return hands::bodyAnchor(s, offsets);
    }

    return avatar::Follower{s}(avatar::Part::Chest, hands::bodyAnchor(avatar::standing(s), offsets));
}

Hotspot holsterHotspot(Holster holster)
{
    constexpr Hotspot hotspots[HolsterCount] = {HS_LEFT_SHOULDER_HOLSTER, HS_RIGHT_SHOULDER_HOLSTER,
        HS_LEFT_HIP_HOLSTER, HS_RIGHT_HIP_HOLSTER, HS_LEFT_UPPER_HOLSTER, HS_RIGHT_UPPER_HOLSTER};
    return hotspots[holster];
}

void queueDebug(const hands::State& s)
{
    if(!s.valid)
    {
        return;
    }

    const cvar_t* shown[HolsterCount] = {&vr_show_shoulder_holsters, &vr_show_shoulder_holsters,
        &vr_show_hip_holsters, &vr_show_hip_holsters, &vr_show_upper_holsters, &vr_show_upper_holsters};

    for(int h = 0; h < HolsterCount; h++)
    {
        if(!shown[h]->value)
        {
            continue;
        }

        const auto holster = static_cast<Holster>(h);
        const int hs = holsterHotspot(holster);
        const bool hovered = s.hotspot[HAND_OFF] == hs || s.hotspot[HAND_MAIN] == hs;
        const glm::vec4 color = hovered ? glm::vec4{0.2f, 1.f, 0.2f, 0.35f} : glm::vec4{1.f, 0.9f, 0.2f, 0.25f};
        lines::point(holsterPosition(s, holster), threshold(holster) * 2.f, color);
    }

    if(vr_show_virtual_stock.value)
    {
        glm::vec3 shoulder{vr_shoulder_offset_x.value, vr_shoulder_offset_y.value, vr_shoulder_offset_z.value};
        for(int side = 0; side < 2; side++)
        {
            const glm::vec3 pos = chestAnchor(s, shoulder);
            lines::point(pos, vr_virtual_stock_thresh.value * 2.f, {0.3f, 0.6f, 1.f, 0.25f});
            shoulder.y = -shoulder.y;
        }
    }
}

void updateHotspots(hands::State& s)
{
    const HolsterPositions holsters = holsterPositions(s);
    for(int hand = 0; hand < HAND_COUNT; hand++)
    {
        s.hotspot[hand] = hotspot(s, hand, holsters);
    }
}

} // namespace qvr::body
