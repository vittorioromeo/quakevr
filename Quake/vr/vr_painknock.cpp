// vr_painknock.cpp -- see vr_painknock.hpp.

#include "vr_painknock.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_hands.hpp"
#include "vr_main.hpp"
#include "vr_units.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace qvr::painknock
{
namespace
{

constexpr int maxKnocks = 4;         // hits in quick succession add up (the oldest replaced)
constexpr float riseShare = 0.2f;    // of the knock's time, the push out (then the ease back)
constexpr float riseMax = 0.05f;     // s, at most
constexpr float pitchPerCm = 0.8f;   // degrees the hand tips up for each cm it is pushed
constexpr float sideWeight = 0.3f;   // each hand's share: 0.7 +- this, more for the hand on the side the hit came from
constexpr float noDirection = 2.f;   // units: a source this near the world's middle (or the player) has no direction

struct Knock
{
    double time = -1.0;
    glm::vec3 dir{0.f};        // world, unit: the way the hands are pushed
    float size[2]{0.f, 0.f};   // units at the peak, per hand
    float duration = 0.f;      // s
};
Knock knocks[maxKnocks];
int nextKnock = 0;
double printedAt = -1.0;

// 0..1..0 over the knock: a quick push out, then an ease back ending at `duration`.
[[nodiscard]] float envelope(float t, float duration)
{
    if(t < 0.f || t >= duration || duration <= 0.f)
    {
        return 0.f;
    }
    const float rise = std::min(riseMax, duration * riseShare);
    if(t < rise)
    {
        const float u = t / rise;
        return u * u * (3.f - 2.f * u);
    }
    const float u = (t - rise) / (duration - rise);
    return (1.f - u) * (1.f - u);
}

// +1 for the hand on the body's right, -1 for the one on its left: by where they are (crossed arms: the hand that is
// there), the main hand on the right when level.
[[nodiscard]] float handSide(const hands::State& s, int hand, const glm::vec3& right)
{
    const float rel = glm::dot(s.pos[hand] - s.pos[1 - hand], right);
    if(std::fabs(rel) > 1.f)
    {
        return rel > 0.f ? 1.f : -1.f;
    }
    const bool rightHand = (hand == HAND_MAIN) != (vr_lefthanded.value != 0.f);
    return rightHand ? 1.f : -1.f;
}

// A hit of `damage` points from `from` (world), or from nowhere in particular (`directed` false: straight down).
void hit(float damage, const glm::vec3& from, bool directed, const char* what)
{
    const hands::State& s = hands::current();
    if(damage <= 0.f || !s.valid)
    {
        return;
    }
    glm::vec3 fwd, right, up;
    hands::angleVectors({0.f, s.bodyYaw, 0.f}, fwd, right, up);

    // Where it came from, level (a shot from above or below still knocks the hands back from it), and which side.
    glm::vec3 toSource = from - s.playerOrigin;
    glm::vec3 level{toSource.x, toSource.y, 0.f};
    glm::vec3 dir{0.f, 0.f, -1.f};
    float side = 0.f; // -1: from the left, 1: from the right
    if(directed && glm::length(level) > noDirection)
    {
        level = glm::normalize(level);
        dir = -level;
        side = std::clamp(glm::dot(level, right), -1.f, 1.f);
    }
    else if(directed && glm::length(toSource) > noDirection)
    {
        dir = -glm::normalize(toSource); // straight above or below
    }

    float weight[2];
    for(int hand = 0; hand < 2; hand++)
    {
        weight[hand] = 1.f - sideWeight + sideWeight * side * handSide(s, hand, right);
    }

    // The knock.
    const float cm = std::min(std::max(vr_pain_knock_max.value, 0.f), std::max(vr_pain_knock_strength.value, 0.f) * damage);
    const float duration = std::max(vr_pain_knock_time.value, 0.f);
    const bool knocked = vr_pain_knock.value != 0.f && cm > 0.f && duration > 0.f;
    if(knocked)
    {
        Knock& k = knocks[nextKnock];
        nextKnock = (nextKnock + 1) % maxKnocks;
        k.time = cl.time;
        k.dir = dir;
        k.duration = duration;
        for(int hand = 0; hand < 2; hand++)
        {
            k.size[hand] = cm * 0.01f * units::metresToUnits() * weight[hand];
        }
    }

    // The buzz: harder and longer the harder the hit, more in the hand on its side.
    const float amp = std::min(1.f, 0.35f + damage / 40.f) * std::max(vr_pain_haptics.value, 0.f);
    const float seconds = std::min(0.5f, 0.12f + damage / 60.f);
    if(amp > 0.f && !vr_disablehaptics.value)
    {
        if(Backend* be = backend())
        {
            for(int hand = 0; hand < 2; hand++)
            {
                be->haptic(hand, seconds, 30.f, std::min(1.f, amp * weight[hand]));
            }
        }
    }

    if(vr_debug_pain.value)
    {
        Con_Printf("painhit %s t %.3f damage %.0f side %.2f dir %.2f %.2f %.2f knock %.2f cm (%s) weights off %.2f main %.2f "
                   "buzz %.2f for %.2f s\n",
            what, cl.time, damage, side, dir.x, dir.y, dir.z, knocked ? cm : 0.f, knocked ? "on" : "off", weight[HAND_OFF],
            weight[HAND_MAIN], amp, seconds);
    }
}

void test_f()
{
    if(Cmd_Argc() < 2)
    {
        Con_Printf("vr_pain_test <damage> [degrees]: a hit from that far to your left of ahead (negative: right; "
                   "\"none\": no direction)\n");
        return;
    }
    const hands::State& s = hands::current();
    const float damage = static_cast<float>(std::atof(Cmd_Argv(1)));
    const bool directed = Cmd_Argc() < 3 || std::strcmp(Cmd_Argv(2), "none") != 0;
    const float degrees = Cmd_Argc() < 3 || !directed ? 0.f : static_cast<float>(std::atof(Cmd_Argv(2)));
    const glm::vec3 from = s.playerOrigin + hands::rotateYaw(hands::forward({0.f, s.bodyYaw, 0.f}), degrees) * 100.f;
    hit(damage, from, directed, "test");
}

} // namespace

void onDamage(int armor, int blood, const float from[3])
{
    // The world's middle (falls, lava, slime, drowning: the world is what hurts) has no direction.
    glm::vec3 f{from[0], from[1], from[2]};
    bool directed = glm::length(f) > noDirection;
    if(directed && cl.worldmodel)
    {
        const glm::vec3 middle = 0.5f * (glm::vec3{cl.worldmodel->mins[0], cl.worldmodel->mins[1], cl.worldmodel->mins[2]}
                                            + glm::vec3{cl.worldmodel->maxs[0], cl.worldmodel->maxs[1], cl.worldmodel->maxs[2]});
        directed = glm::length(f - middle) > noDirection;
    }
    hit(static_cast<float>(armor + blood), f, directed, "damage");
}

void offset(int hand, glm::vec3& pos, glm::vec3& angles)
{
    pos = glm::vec3{0.f};
    angles = glm::vec3{0.f};
    for(const Knock& k : knocks)
    {
        if(k.time < 0.0)
        {
            continue;
        }
        const float e = envelope(static_cast<float>(cl.time - k.time), k.duration);
        pos += k.dir * (k.size[hand] * e);
    }
    // Hits adding up go no further than one at the cap.
    const float cap = std::max(vr_pain_knock_max.value, 0.f) * 0.01f * units::metresToUnits();
    const float len = glm::length(pos);
    if(len > cap)
    {
        pos *= len > 0.f ? cap / len : 0.f;
    }
    const float cmNow = glm::length(pos) / (0.01f * units::metresToUnits());
    angles.x = -pitchPerCm * cmNow; // tipped up (pitch down is positive)

    if(vr_debug_pain.value && hand == HAND_MAIN && cmNow > 0.f && realtime != printedAt)
    {
        printedAt = realtime;
        const hands::State& s = hands::current();
        glm::vec3 fwd, right, up;
        hands::angleVectors({0.f, s.bodyYaw, 0.f}, fwd, right, up);
        glm::vec3 off{0.f};
        for(const Knock& k : knocks)
        {
            if(k.time >= 0.0)
            {
                off += k.dir * (k.size[HAND_OFF] * envelope(static_cast<float>(cl.time - k.time), k.duration));
            }
        }
        const float toCm = 1.f / (0.01f * units::metresToUnits());
        Con_Printf("painknock t %.3f main %.2f cm (right %.2f fwd %.2f up %.2f) off %.2f cm (right %.2f fwd %.2f up %.2f)\n",
            cl.time, cmNow, glm::dot(pos, right) * toCm, glm::dot(pos, fwd) * toCm, pos.z * toCm, glm::length(off) * toCm,
            glm::dot(off, right) * toCm, glm::dot(off, fwd) * toCm, off.z * toCm);
    }
}

void reset()
{
    for(Knock& k : knocks)
    {
        k = Knock{};
    }
    nextKnock = 0;
}

void registerCommands()
{
    Cmd_AddCommand("vr_pain_test", test_f);
}

} // namespace qvr::painknock

extern "C" void VR_OnDamage(int armor, int blood, const float* from)
{
    qvr::painknock::onDamage(armor, blood, from);
}
