// vr_body.cpp -- see vr_body.hpp. Holster positions and hotspots from the old engine
// (VR_Get*HolsterPos, VR_In*HolsterDistance and VR_Move's computeHotSpot).

#include "vr_body.hpp"
#include "vr_engine.hpp"
#include "vr_avatar.hpp"
#include "vr_backend.hpp"
#include "vr_cvars.hpp"
#include "vr_lines.hpp"

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

} // namespace

glm::vec3 holsterPosition(const hands::State& s, Holster holster)
{
    if(!vr_body_anchors.value)
    {
        return legacyHolsterPosition(s, holster);
    }

    // Where they are for the standing body, carried by the pelvis (hips) or the chest.
    const avatar::Part part = holster == LeftHip || holster == RightHip ? avatar::Part::Pelvis : avatar::Part::Chest;
    return avatar::follow(s, part, legacyHolsterPosition(avatar::standing(s), holster));
}

glm::vec3 chestAnchor(const hands::State& s, const glm::vec3& offsets)
{
    if(!vr_body_anchors.value)
    {
        return hands::bodyAnchor(s, offsets);
    }

    return avatar::follow(s, avatar::Part::Chest, hands::bodyAnchor(avatar::standing(s), offsets));
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

Hotspot hotspot(const hands::State& s, int hand)
{
    const glm::vec3& pos = s.pos[hand];

    for(int h = 0; h < HolsterCount; h++)
    {
        const auto holster = static_cast<Holster>(h);
        if(glm::distance(pos, holsterPosition(s, holster)) < threshold(holster))
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

} // namespace qvr::body
