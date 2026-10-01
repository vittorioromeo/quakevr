// vr_torso.cpp -- which way the torso faces, from a headset and two controllers (ROUND21.md, "Torso direction").
//
// Nothing tracks the torso: it is guessed. The old engine's guess (VR_GetBodyYawAngle, vr_torso_mode 0) turned it
// four fifths of the way towards the hands' average reach from the shoulders, every frame: one hand behind the back
// with the other moving, or one hand swept across in front, turned the whole body after it. This one (mode 1) goes
// mostly with the head, and with its recent past (a glance does not turn the body; facing a way for a while does);
// the hands only pull it, and only those in front of the body and not far off to a side; both hands' midpoint only
// when both are in front (holding a gun, reaching for something), one hand alone a little; both hands hanging by the
// sides, square to the line between them (they turn with the body, not with the head). The result turns only
// once it is more than a deadzone off where it was, then eases there, and never further from the head than a neck
// turns.

#include "vr_torso.hpp"
#include "vr_engine.hpp"
#include "vr_backend.hpp"
#include "vr_cvars.hpp"
#include "vr_units.hpp"

#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Round.hpp"

namespace qvr::torso
{

namespace
{

// The turn from `from` to `to`, -180 to 180 (IEEE remainder: za::remainder truncates, as fmod).
[[nodiscard]] float wrapYaw(float a)
{
    return a - za::round(a / 360.f) * 360.f;
}

[[nodiscard]] float yawDelta(float to, float from)
{
    return wrapYaw(to - from);
}

[[nodiscard]] float yawOf(const glm::vec3& v)
{
    return glm::degrees(za::atan2(v.y, v.x));
}

[[nodiscard]] float smoothStep(float edge0, float edge1, float x)
{
    const float t = za::clamp((x - edge0) / (edge1 - edge0), 0.f, 1.f);
    return t * t * (3.f - 2.f * t);
}

// The estimate's state, in the play space's degrees (world yaw less the play space's turn).
struct Estimate
{
    bool valid{false};
    double time{0.0};
    float headHistory{0.f}; // the head's yaw, eased (vr_torso_head_lag)
    float yaw{0.f};         // the torso's
    bool following{false};  // past the deadzone, easing to the target

    // For vr_torso_report.
    float head{0.f}, legacy{0.f}, target{0.f};
    float handsDir{0.f}, handsWeight{0.f}; // the hands' direction (from the torso) and weight
    float weight[2]{0.f, 0.f}, both{0.f}, down{0.f};
};

Estimate est;

// The old engine's VR_GetBodyYawAngle (world degrees): the head's yaw, pulled 0.8 of the way towards where the hands
// are from the shoulders (an imagined chest 10 units behind the head), hands behind it pulling only a little.
[[nodiscard]] float legacyYaw(const hands::State& s, float headYaw)
{
    glm::vec3 headFwd, headRight, headUp;
    hands::angleVectors({0.f, headYaw, 0.f}, headFwd, headRight, headUp);

    const glm::vec3 chest = s.playerOrigin + s.lean - headFwd * 10.f;
    const glm::vec3 shoulders[2]{chest - headRight * 6.5f, chest + headRight * 6.5f};

    glm::vec3 handDir{0.f};
    for(int h = 0; h < HAND_COUNT; h++)
    {
        glm::vec3 hand = s.pos[h];
        hand.z = shoulders[HAND_OFF].z;
        handDir += (hand - shoulders[h]) * 0.5f;
    }
    handDir /= 10.f;

    if(glm::dot(handDir, headFwd) < 0.f && glm::length(handDir) > 0.1f)
    {
        handDir = glm::normalize(handDir) * 0.1f;
    }

    const glm::vec3 dir = glm::mix(headFwd, handDir, 0.8f);
    return glm::length(dir) > 0.f ? yawOf(dir) : headYaw;
}

// The hands' votes for the torso's direction: adds to `sum` (each direction, play-space degrees relative to the head,
// times its weight) and `weights`, and fills est's report fields. Hands are judged in the torso's frame (`torsoWorld`,
// its last estimate: what is behind the body or off to its side), from a chest 0.15 m behind the head with shoulders
// 0.18 m to each side of it (HAND_OFF is the left controller, whichever hand is the main one).
void handVotes(const hands::State& s, float torsoWorld, float torsoFromHead, float& sum, float& weights)
{
    const float m2u = units::metresToUnits();
    glm::vec3 fwd, right, up;
    hands::angleVectors({0.f, torsoWorld, 0.f}, fwd, right, up);
    const glm::vec3 chest = s.playerOrigin + s.lean - fwd * (0.15f * m2u);
    const float side = za::max(1.f, vr_torso_side_angle.value);
    const auto angleOf = [&](const glm::vec3& v) { return yawOf({glm::dot(v, fwd), -glm::dot(v, right), 0.f}); }; // left +

    float angle[2];
    for(int h = 0; h < HAND_COUNT; h++)
    {
        const glm::vec3 shoulder = chest + right * ((h == HAND_OFF ? -0.18f : 0.18f) * m2u);
        const float ahead = glm::dot(s.pos[h] - shoulder, fwd) / m2u; // metres in front of its shoulder
        angle[h] = angleOf(s.pos[h] - chest);
        // In front (5 to 25 cm ahead of its shoulder; behind the back or hanging by the side: none), and not far off to
        // a side (full up to vr_torso_side_angle from the chest, none 35 degrees further).
        est.weight[h] = smoothStep(0.05f, 0.25f, ahead) * (1.f - smoothStep(side, side + 35.f, za::fabs(angle[h])));
    }

    // Both hands in front: their midpoint.
    est.both = za::min(est.weight[0], est.weight[1]);
    const float bothWeight = est.both * za::max(0.f, vr_torso_hands.value);
    const float midAngle = angleOf((s.pos[0] + s.pos[1]) * 0.5f - chest);

    // Otherwise the hand most in front, alone, a little.
    const int k = est.weight[0] >= est.weight[1] ? 0 : 1;
    const float oneWeight = (1.f - est.both) * est.weight[k] * za::max(0.f, vr_torso_one_hand.value);

    // Both hands hanging down by the sides: square to the line between them (they turn with the body, not with a
    // glance). Low (35 to 50 cm below the head and more), shoulder width or so apart (25 to 40 cm and more), about
    // level (12 to 25 cm apart in height at most) and close to the body (30 to 42 cm from below the head at most; a
    // hand behind the back is near the middle, too close to the other); the line not turned far from the head's
    // (crossed hands: backwards). Whichever way the torso faces: judged in its frame, a torso that followed a glance
    // would see the hands turned away and lose the vote that holds it.
    const glm::vec3 across = s.pos[HAND_MAIN] - s.pos[HAND_OFF];
    const float apart = glm::length(glm::vec2{across}) / m2u;
    const float downAngle = angleOf({-across.y, across.x, 0.f}); // the line turned a quarter left: forward
    float down = smoothStep(0.25f, 0.4f, apart) * (1.f - smoothStep(70.f, 100.f, za::fabs(torsoFromHead + downAngle)));
    for(int h = 0; h < HAND_COUNT; h++)
    {
        const glm::vec3 fromAxis = s.pos[h] - (s.playerOrigin + s.lean);
        down *= smoothStep(0.35f, 0.5f, (s.head.z - s.pos[h].z) / m2u) *
                (1.f - smoothStep(0.3f, 0.42f, glm::length(glm::vec2{fromAxis}) / m2u));
    }
    down *= 1.f - smoothStep(0.12f, 0.25f, za::fabs(across.z) / m2u); // level with each other
    const float downWeight = down * za::max(0.f, vr_torso_hands_down.value);

    sum += bothWeight * (torsoFromHead + midAngle) + oneWeight * (torsoFromHead + angle[k]) +
           downWeight * (torsoFromHead + downAngle);
    weights += bothWeight + oneWeight + downWeight;
    est.handsWeight = bothWeight + oneWeight + downWeight;
    est.handsDir = est.handsWeight > 0.f ? (bothWeight * midAngle + oneWeight * angle[k] + downWeight * downAngle) / est.handsWeight
                                         : 0.f;
    est.down = down;
}

} // namespace

float estimate(const hands::State& s, float headYaw, float turnYaw, bool handsValid)
{
    const float legacy = handsValid ? legacyYaw(s, headYaw) : headYaw;
    est.legacy = yawDelta(legacy, turnYaw);
    if(vr_torso_mode.value == 0.f)
    {
        est.valid = false;
        est.head = yawDelta(headYaw, turnYaw);
        est.yaw = est.target = est.legacy;
        return legacy;
    }

    const float head = yawDelta(headYaw, turnYaw);
    const double dtRaw = est.valid ? realtime - est.time : -1.0;
    const bool restart = dtRaw < 0.0 || dtRaw > 0.5; // first, or after a pause (a load, the menu): from the head
    const float dt = restart ? 0.f : static_cast<float>(dtRaw);
    est.head = head;
    est.time = realtime;

    // The head, and where it has faced lately.
    const float lag = vr_torso_head_lag.value;
    if(restart || lag <= 0.f)
    {
        est.headHistory = head;
    }
    else
    {
        est.headHistory += yawDelta(head, est.headHistory) * (1.f - za::exp(-dt / lag));
    }

    // A weighted mean of the directions, relative to the head: the head now, where it faced lately, and the hands.
    const float torso = restart ? head : est.yaw;
    const float headWeight = za::max(0.f, vr_torso_head.value);
    const float historyWeight = za::max(0.f, vr_torso_head_history.value);
    float sum = historyWeight * yawDelta(est.headHistory, head);
    float weights = headWeight + historyWeight;
    est.handsWeight = est.handsDir = 0.f;
    est.weight[0] = est.weight[1] = est.both = est.down = 0.f;
    if(handsValid)
    {
        handVotes(s, torso + turnYaw, yawDelta(torso, head), sum, weights);
    }

    const float neck = za::clamp(vr_torso_neck_max.value, 0.f, 180.f);
    est.target = head + za::clamp(weights > 0.f ? sum / weights : 0.f, -neck, neck);

    if(restart)
    {
        est.yaw = est.target;
        est.following = false;
    }
    else
    {
        // A deadzone: still until the target is more than vr_torso_deadzone off, then eased all the way there.
        float off = yawDelta(est.target, est.yaw);
        if(za::fabs(off) > vr_torso_deadzone.value)
        {
            est.following = true;
        }
        if(est.following)
        {
            const float speed = vr_torso_speed.value;
            est.yaw += speed > 0.f ? off * (1.f - za::exp(-speed * dt)) : off;
            off = yawDelta(est.target, est.yaw);
            est.following = za::fabs(off) > 1.f;
        }
    }
    // Never further from the head than the neck turns, eased or not (a quick look round takes the body along).
    est.yaw = wrapYaw(head + za::clamp(yawDelta(est.yaw, head), -neck, neck));
    est.valid = true;
    return est.yaw + turnYaw;
}

void reset()
{
    est.valid = false;
}

void report_f()
{
    const char* label = Cmd_Argc() >= 2 ? Cmd_Argv(1) : "";
    Con_Printf("torso %s (t %.2f): head %.1f old %.1f new %.1f\n", label, realtime, est.head, est.legacy, est.yaw);
    Con_Printf("torso+ target %.1f hist %.1f hands %.1f weight %.2f (off %.2f main %.2f both %.2f down %.2f)\n", est.target,
        est.headHistory, est.handsDir, est.handsWeight, est.weight[0], est.weight[1], est.both, est.down);
}

} // namespace qvr::torso
