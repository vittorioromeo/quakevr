// vr_body.cpp -- see vr_body.hpp. Holster positions and hotspots from the old engine
// (VR_Get*HolsterPos, VR_In*HolsterDistance and VR_Move's computeHotSpot).

#include "vr_body.hpp"
#include "vr_engine.hpp"
#include "vr_avatar.hpp"
#include "vr_backend.hpp"
#include "vr_cvars.hpp"
#include "vr_lines.hpp"
#include "vr_units.hpp"

#include <glm/gtc/quaternion.hpp>

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
// the eyes) is inside the torso, whose chest hid the hips and upper holsters from the eyes. At their
// default X (vr_hip_offset_x, vr_upper_holster_offset_x) they sit on the front of the body (the
// belly, the chest), further forward as needed to be seen past the chest when looking down, by at
// most a little (with the torso right under the eyes, vr_body_torso_back 0, the chest still hides
// part of the hips): frontOfTheBody. X moves them from there, forward or back; behind the front,
// they go round the body's side to its back rather than into it (outOfTheTorso). Until round 21 an
// X behind the front was stopped there (the author's "they stop at some point"); configs keep their
// look (migrateHolsters). In the standing body, which the Follower carries.
[[nodiscard]] bool isOnTheBody(Holster holster)
{
    return vr_body_mode.value >= 1.f && holster != LeftShoulder && holster != RightShoulder;
}

// make_vrbody.py: the spine hangs from the top of the neck (vr_body_eye_forward and
// vr_body_eye_up from the eyes, then vr_body_torso_back), its rings centred 0.01 m behind it;
// the belly (the pelvis ring) is 0.115 m deep and 0.17 m to the side, the chest 0.12-0.13 and
// 0.19 times the build's torso scale, widest 0.22 m below the neck (1.35 m, the neck at 1.57).
struct TorsoShape
{
    float m2w;
    float torsoScale;
    float axis; // the rings' centre, forward from the eyes
};

[[nodiscard]] TorsoShape torsoShape()
{
    const float m2w = units::metresToUnits() * units::bodyScale();
    const int build = static_cast<int>(vr_body_build.value);
    const float torsoScale = build <= 0 ? 0.965f : build >= 2 ? 1.175f : 1.07f;
    return {m2w, torsoScale, -(vr_body_eye_forward.value + vr_body_torso_back.value + 0.01f) * m2w};
}

// The X each holster's default is (vr_cvars.inc): at it, a holster on the body sits on its front.
constexpr float HIP_X_DEFAULT = -3.5f;
constexpr float UPPER_X_DEFAULT = -4.25f;
constexpr float SURFACE_CLEARANCE = 1.5f; // units the holster (a hand's reach target) stands out of the body

// The torso's ring round the spine where a holster is (hips: the belly's; upper: the chest's), as
// far out as the holster stands: its depth (forward) and half width, world units.
[[nodiscard]] glm::vec2 torsoRing(const TorsoShape& t, bool hip)
{
    return glm::vec2{hip ? 0.115f : 0.12f * t.torsoScale, hip ? 0.17f : 0.19f * t.torsoScale} * t.m2w +
           SURFACE_CLEARANCE;
}

// Where the old placement's holster at `pos` goes on the front of the body, forward from the eyes.
[[nodiscard]] float frontOfTheBody(const hands::State& standing, Holster holster, const glm::vec3& pos)
{
    const TorsoShape t = torsoShape();
    const float chestFront = t.axis + 0.13f * t.torsoScale * t.m2w;
    const float chestDrop = (vr_body_eye_up.value + 0.22f) * t.m2w;
    const float onFront = t.axis + torsoRing(t, holster == LeftHip || holster == RightHip).x; // clear of the surface

    // In front of the line from the eyes over the front of the chest.
    const float drop = standing.head.z - pos.z;
    const float seen = drop > chestDrop + 1.f ? chestFront * drop / chestDrop + 1.f : onFront;
    return onFront + CLAMP(0.f, seen - onFront, 2.5f);
}

// A point in the torso goes out to its ring, straight out from the spine: behind the front, a holster
// goes round the side (the hips, the ribs) and on to the back as X goes back, never into the body.
[[nodiscard]] glm::vec3 outOfTheTorso(const hands::State& standing, bool hip, const glm::vec3& pos)
{
    const TorsoShape t = torsoShape();
    const glm::vec2 ring = torsoRing(t, hip);
    glm::vec3 fwd, right, up;
    hands::angleVectors({0.f, standing.bodyYaw, 0.f}, fwd, right, up);
    const float x = glm::dot(pos - standing.head, fwd) - t.axis;
    const float y = glm::dot(pos - standing.head, right);
    const float e = std::sqrt((x / ring.x) * (x / ring.x) + (y / ring.y) * (y / ring.y));
    if(e >= 1.f)
    {
        return pos;
    }
    if(e < 1e-3f)
    {
        return pos + fwd * (ring.x - x); // on the spine: out in front
    }
    return pos + fwd * (x / e - x) + right * (y / e - y);
}

[[nodiscard]] glm::vec3 onTheBody(const hands::State& standing, Holster holster, const glm::vec3& pos)
{
    if(!isOnTheBody(holster))
    {
        return pos;
    }

    glm::vec3 fwd, right, up;
    hands::angleVectors({0.f, standing.bodyYaw, 0.f}, fwd, right, up);
    const float now = glm::dot(pos - standing.head, fwd);
    const bool hip = holster == LeftHip || holster == RightHip;
    const float x = hip ? vr_hip_offset_x.value : vr_upper_holster_offset_x.value;
    const float at = frontOfTheBody(standing, holster, pos) + (x - (hip ? HIP_X_DEFAULT : UPPER_X_DEFAULT));
    return outOfTheTorso(standing, hip, pos + fwd * (at - now));
}

// The body's surface under a holster on it (onTheBody), in the standing body. The torso's ring
// there is an ellipse round the spine, whose normal the holster's plate faces: the belly's at the
// hips, the chest's at the upper holsters, which sit above its widest, by the collarbones, where
// it slopes back towards the neck.
[[nodiscard]] HolsterPlate plateOnTheBody(const hands::State& standing, Holster holster, const glm::vec3& pos)
{
    const auto [m2w, torsoScale, axis] = torsoShape();
    const bool hip = holster == LeftHip || holster == RightHip;
    const float depth = (hip ? 0.115f : 0.13f * torsoScale) * m2w;
    const float width = (hip ? 0.17f : 0.19f * torsoScale) * m2w;
    const float slope = glm::radians(hip ? 0.f : 20.f);

    glm::vec3 fwd, right, up;
    hands::angleVectors({0.f, standing.bodyYaw, 0.f}, fwd, right, up);
    const float x = glm::dot(pos - standing.head, fwd) - axis; // (behind the spine: the plate faces back)
    const float y = glm::dot(pos - standing.head, right);
    const glm::vec3 flat = std::abs(x) + std::abs(y) > 1e-3f
                               ? glm::normalize(fwd * (x / (depth * depth)) + right * (y / (width * width)))
                               : fwd;

    HolsterPlate p;
    p.out = flat * std::cos(slope) + up * std::sin(slope);
    p.up = up * std::cos(slope) - flat * std::sin(slope);
    const float e = std::sqrt((x / depth) * (x / depth) + (y / width) * (y / width));
    p.clearance = std::sqrt(x * x + y * y) * (1.f - 1.f / e); // along the ring's radius
    return p;
}

// With the full body's legs (vr_body_mode 3), the hip holsters ride the thighs, by
// vr_holster_leg_follow (0 fixed on the body, 1 all the way): they go with the legs' animation
// (walking, stepping round, tucking up in the air, kicking in the water), from where they are with
// the legs standing still. A holster moves as the point of the thigh it is strapped to: under it,
// but at least THIGH_STRAP down the thigh (the hips' holsters are at the top of the thighs, by the
// hip joint, where the thigh barely moves), and turns with the thigh. Where it is drawn is where the
// hands find it (updateHotspots).
constexpr float THIGH_STRAP = 0.2f; // metres of the body

void onTheThigh(const avatar::Follower& follow, Holster holster, glm::vec3& pos, HolsterPlate* plate)
{
    const float amount = CLAMP(0.f, vr_holster_leg_follow.value, 1.f);
    avatar::ThighMotion m;
    if((holster != LeftHip && holster != RightHip) || amount <= 0.f || vr_body_mode.value < 3.f ||
        !follow.thigh(holster == LeftHip ? 0 : 1, m))
    {
        return;
    }

    const float strap = THIGH_STRAP * units::metresToUnits() * units::bodyScale();
    const glm::vec3 anchor = pos + m.down * std::max(0.f, strap - glm::dot(pos - m.joint, m.down));
    pos += (m.joint + m.turn * (anchor - m.joint) - anchor) * amount;
    if(plate)
    {
        const glm::quat turn = glm::slerp(glm::quat{1.f, 0.f, 0.f, 0.f}, glm::normalize(glm::quat_cast(m.turn)), amount);
        plate->out = turn * plate->out;
        plate->up = turn * plate->up;
    }
}

// With vr_body_anchors: where the holster is for the standing body, carried by the pelvis (hips)
// or the chest, and (`plate`) the body's surface there; the hips' on the thighs (onTheThigh).
[[nodiscard]] glm::vec3 followingHolsterPosition(
    const avatar::Follower& follow, const hands::State& standing, Holster holster, HolsterPlate* plate = nullptr)
{
    const avatar::Part part = holster == LeftHip || holster == RightHip ? avatar::Part::Pelvis : avatar::Part::Chest;
    const glm::vec3 pos = onTheBody(standing, holster, legacyHolsterPosition(standing, holster));
    glm::vec3 now = follow(part, pos);
    const bool onBody = plate && isOnTheBody(holster);
    if(onBody)
    {
        // The Follower moves points rigidly: directions follow as the difference of two.
        const HolsterPlate p = plateOnTheBody(standing, holster, pos);
        plate->out = follow(part, pos + p.out) - now;
        plate->up = follow(part, pos + p.up) - now;
        plate->clearance = p.clearance;
    }
    onTheThigh(follow, holster, now, onBody ? plate : nullptr);
    return now;
}

[[nodiscard]] Hotspot hotspot(const hands::State& s, int hand, const HolsterPositions& holsters)
{
    const glm::vec3& pos = s.pos[hand];

    // The holster the hand is most within (its distance over the holster's reach), not the first in the list: the
    // shoulders' reach (vr_shoulder_holster_thresh, 7.8) comes down over the top of the upper holsters' (on the chest,
    // a few units below), and a gun let go at the top of a chest holster went into the shoulder holster over it
    // (round 20: "invisible until I pick it up and put it back").
    int best = -1;
    float bestRatio = 1.f;
    for(int h = 0; h < HolsterCount; h++)
    {
        const auto holster = static_cast<Holster>(h);
        const float reach = threshold(holster);
        const float ratio = reach > 0.f ? glm::distance(pos, holsters[h]) / reach : 2.f;
        if(ratio < bestRatio)
        {
            bestRatio = ratio;
            best = h;
        }
    }
    if(best >= 0)
    {
        return holsterHotspot(static_cast<Holster>(best));
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

float holsterReach(Holster holster)
{
    return threshold(holster);
}

glm::vec3 holsterPosition(const hands::State& s, Holster holster)
{
    if(!vr_body_anchors.value)
    {
        return legacyHolsterPosition(s, holster);
    }

    return followingHolsterPosition(avatar::Follower{s}, avatar::standing(s), holster);
}

HolsterPositions holsterPositions(const hands::State& s, HolsterPlates* plates)
{
    HolsterPositions out;
    if(plates)
    {
        *plates = HolsterPlates{};
    }
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
        out[h] = followingHolsterPosition(follow, standing, static_cast<Holster>(h), plates ? &(*plates)[h] : nullptr);
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

void migrateHolsters()
{
    // Round 21 stopped a hip or upper holster on the body at its front (frontOfTheBody): an X behind
    // that put it there. Now X goes on from the default's place, round the body: an X that was
    // stopped becomes the default (where it was), a further forward one keeps its distance ahead of
    // the front. For the body standing as the settings make it (the head over the box's middle).
    if(!vr_body_anchors.value || vr_body_mode.value < 1.f)
    {
        return; // not on the body: placed as before
    }
    hands::State s;
    s.headHeight = units::eyeHeight();
    s.standingHeight = s.headHeight;
    s.head = {0.f, 0.f, vr_floor_offset.value + s.headHeight * units::metresToUnits()};
    const hands::State standing = avatar::standing(s);
    glm::vec3 fwd, right, up;
    hands::angleVectors({0.f, standing.bodyYaw, 0.f}, fwd, right, up);
    for(const Holster h : {RightHip, RightUpper})
    {
        cvar_t& var = h == RightHip ? vr_hip_offset_x : vr_upper_holster_offset_x;
        const glm::vec3 pos = legacyHolsterPosition(standing, h);
        const float now = glm::dot(pos - standing.head, fwd);
        const float front = frontOfTheBody(standing, h, pos);
        const float x = var.value + (h == RightHip ? HIP_X_DEFAULT : UPPER_X_DEFAULT) - front + std::max(now, front) - now;
        if(std::abs(x - var.value) > 1e-3f)
        {
            Con_DPrintf("VR: %s %s is now %.2f (the same place on the body)\n", var.name, var.string, x);
            Cvar_SetValueQuick(&var, std::round(x * 100.f) / 100.f);
        }
    }
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
