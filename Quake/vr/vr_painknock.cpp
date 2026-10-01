// vr_painknock.cpp -- see vr_painknock.hpp.

#include "vr_painknock.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_hands.hpp"
#include "vr_main.hpp"
#include "vr_units.hpp"

#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"

#include <stdlib.h>

namespace qvr::painknock
{
namespace
{

constexpr int maxKnocks = 4;         // hits in quick succession add up (the oldest replaced)
constexpr float riseShare = 0.15f;   // of the knock's time, the push out
constexpr float riseMax = 0.06f;     // s, at most
constexpr float holdShare = 0.15f;   // then held out this share of it, then eased back (slow, then quicker, then slow)
constexpr float liftShare = 0.5f;    // a directed knock's lift: the hands flinch up as well as away (a share of the away)
constexpr float sideWeight = 0.3f;   // the far hand's share from the side: 1 - 2 * this (from ahead or behind: both all)
constexpr float noDirection = 2.f;   // units: a source this near the world's middle (or the player) has no direction
constexpr float wristBack = 10.f;    // cm from the grip back to the wrist: what the tip turns the hand about

struct Knock
{
    double time = -1.0;
    glm::vec3 dir{0.f};        // world, unit: the way the hands are pushed
    glm::vec3 away{0.f};       // world, level unit: away from where a level-directed hit came from (zero: none)
    float size[2]{0.f, 0.f};   // units at the peak, per hand
    float duration = 0.f;      // s
};
Knock knocks[maxKnocks];
int nextKnock = 0;
double printedAt = -1.0;

// 0..1..0 over the knock: a quick push out, held a moment, then an ease back ending at `duration` (smooth both ends:
// half of it still there halfway back; the old quadratic ease lost half in the return's first 30%, the knock barely
// showed: ROUND21.md, "Pain feedback, second pass").
[[nodiscard]] float envelope(float t, float duration)
{
    if(t < 0.f || t >= duration || duration <= 0.f)
    {
        return 0.f;
    }
    const float rise = za::min(riseMax, duration * riseShare);
    if(t < rise)
    {
        const float u = t / rise;
        return u * u * (3.f - 2.f * u);
    }
    const float hold = duration * holdShare;
    if(t < rise + hold)
    {
        return 1.f;
    }
    const float u = (t - rise - hold) / za::max(duration - rise - hold, 1e-3f);
    return 1.f - u * u * (3.f - 2.f * u);
}

// cm a hit of `damage` knocks the hands: Knock per Damage a point for small hits, curving into Largest Knock (never
// reached: a harder hit always knocks further, where a hard cap made every hit past it the same).
[[nodiscard]] float knockCm(float damage)
{
    const float most = za::max(vr_pain_knock_max.value, 0.f);
    const float per = za::max(vr_pain_knock_strength.value, 0.f);
    if(most <= 0.f || per <= 0.f || damage <= 0.f)
    {
        return 0.f;
    }
    return most * (1.f - za::exp(-per * damage / most));
}

// +1 for the hand on the body's right, -1 for the one on its left: by where they are (crossed arms: the hand that is
// there), the main hand on the right when level.
[[nodiscard]] float handSide(const hands::State& s, int hand, const glm::vec3& right)
{
    const float rel = glm::dot(s.pos[hand] - s.pos[1 - hand], right);
    if(za::fabs(rel) > 1.f)
    {
        return rel > 0.f ? 1.f : -1.f;
    }
    const bool rightHand = hand == HAND_MAIN; // (the right controller)
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
    glm::vec3 away{0.f};
    float side = 0.f; // -1: from the left, 1: from the right
    if(directed && glm::length(level) > noDirection)
    {
        level = glm::normalize(level);
        away = -level;
        dir = glm::normalize(-level + glm::vec3{0.f, 0.f, liftShare});
        side = za::clamp(glm::dot(level, right), -1.f, 1.f);
    }
    else if(directed && glm::length(toSource) > noDirection)
    {
        dir = -glm::normalize(toSource); // straight above or below
    }

    float weight[2];
    for(int hand = 0; hand < 2; hand++)
    {
        weight[hand] = 1.f - sideWeight * (za::fabs(side) - side * handSide(s, hand, right));
    }

    // The knock.
    const float cm = knockCm(damage);
    const float duration = za::max(vr_pain_knock_time.value, 0.f);
    const bool knocked = vr_pain_knock.value != 0.f && cm > 0.f && duration > 0.f;
    if(knocked)
    {
        Knock& k = knocks[nextKnock];
        nextKnock = (nextKnock + 1) % maxKnocks;
        k.time = cl.time;
        k.dir = dir;
        k.away = away;
        k.duration = duration;
        for(int hand = 0; hand < 2; hand++)
        {
            k.size[hand] = cm * 0.01f * units::metresToUnits() * weight[hand];
        }
    }

    // The buzz: harder and longer the harder the hit, more in the hand on its side.
    const float amp = za::min(1.f, 0.35f + damage / 40.f) * za::max(vr_pain_haptics.value, 0.f);
    const float seconds = za::min(0.5f, 0.12f + damage / 60.f);
    if(amp > 0.f && !vr_disablehaptics.value)
    {
        if(Backend* be = backend())
        {
            for(int hand = 0; hand < 2; hand++)
            {
                be->haptic(hand, seconds, 30.f, za::min(1.f, amp * weight[hand]));
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
    const float damage = static_cast<float>(atof(Cmd_Argv(1)));
    const bool directed = Cmd_Argc() < 3 || ZA_STRCMP(Cmd_Argv(2), "none") != 0;
    const float degrees = Cmd_Argc() < 3 || !directed ? 0.f : static_cast<float>(atof(Cmd_Argv(2)));
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

namespace
{

// The way a knock moves hand `hand` this frame, a unit of its size: with vr_pain_knock_seen, turned across the line from
// the eyes to the hand, so all of its size shows as a move (a hit from ahead pushed the hands away and up: for hands held
// ahead of and below the eyes, that is nearly straight towards the eyes, and a hand coming 15 cm nearer barely shows; the
// hit's throw of the whole player the same way hid it further: ROUND21.md, "Pain feedback, third pass"). From ahead or
// behind the hands rise, from the side they go sideways (and up a little); its move towards or away from the eyes is kept
// on top.
struct Sight
{
    bool valid = false;                      // false (vr_pain_knock_seen 0, no headset eyes): the knock's own way
    glm::vec3 los{0.f}, right{0.f}, up{0.f}; // world, unit: from the eyes to the hand, and across it (as seen)
};

[[nodiscard]] Sight sightOf(const hands::State& s, int hand)
{
    Sight v;
    const glm::vec3 eye = 0.5f * (s.eyeOrigin[0] + s.eyeOrigin[1]);
    const glm::vec3 toHand = s.unresolvedPos[hand] - eye; // (the hand before anything draws it elsewhere)
    if(!vr_pain_knock_seen.value || eye == glm::vec3{0.f} || glm::length(toHand) < 1.f)
    {
        return v;
    }
    v.valid = true;
    v.los = glm::normalize(toHand);
    glm::vec3 r = glm::cross(v.los, glm::vec3{0.f, 0.f, 1.f});
    if(glm::length(r) < 0.2f)
    {
        // The hand straight above or below the eyes: across by the body's right.
        glm::vec3 fwd, up;
        hands::angleVectors({0.f, s.bodyYaw, 0.f}, fwd, r, up);
        r -= v.los * glm::dot(r, v.los);
    }
    v.right = glm::normalize(r);
    v.up = glm::cross(v.right, v.los);
    return v;
}

[[nodiscard]] glm::vec3 knockWay(const Knock& k, const Sight& v)
{
    if(!v.valid)
    {
        return k.dir;
    }
    glm::vec3 across;
    if(k.away != glm::vec3{0.f})
    {
        const float side = glm::dot(k.away, v.right);
        across = glm::normalize(side * v.right + za::max(1.f - za::fabs(side), liftShare) * v.up);
    }
    else
    {
        // Straight down (no direction: a fall, lava) or from above or below: across as it goes, else down or up.
        const glm::vec3 c = k.dir - v.los * glm::dot(k.dir, v.los);
        across = glm::length(c) > 0.2f ? glm::normalize(c) : (k.dir.z < 0.f ? -v.up : v.up);
    }
    return across + v.los * glm::dot(k.dir, v.los);
}

// Hand `hand`'s knock this frame (world units), and how much of it is seen (across the line from the eyes, cm).
[[nodiscard]] glm::vec3 knockOf(int hand, float& seenCm)
{
    const hands::State& s = hands::current();
    const Sight v = sightOf(s, hand);
    glm::vec3 pos{0.f};
    for(const Knock& k : knocks)
    {
        if(k.time < 0.0)
        {
            continue;
        }
        const float e = envelope(static_cast<float>(cl.time - k.time), k.duration);
        if(e > 0.f)
        {
            pos += knockWay(k, v) * (k.size[hand] * e);
        }
    }
    // Hits adding up go no further (as seen) than one at the cap.
    const float cap = za::max(vr_pain_knock_max.value, 0.f) * 0.01f * units::metresToUnits();
    const float seen = v.valid ? glm::length(pos - v.los * glm::dot(pos, v.los)) : glm::length(pos);
    if(seen > cap)
    {
        pos *= seen > 0.f ? cap / seen : 0.f;
    }
    seenCm = za::min(seen, cap) / (0.01f * units::metresToUnits());
    return pos;
}

} // namespace

void offset(int hand, glm::vec3& pos, glm::vec3& angles)
{
    float cmNow = 0.f;
    pos = knockOf(hand, cmNow);
    angles = glm::vec3{0.f};
    angles.x = -za::max(vr_pain_knock_tip.value, 0.f) * cmNow; // tipped up (pitch down is positive)
    if(angles.x < 0.f)
    {
        // Tipped about the wrist, not the grip: about the grip, the wrist and the arm swung down as the hand tipped up
        // (his 3 degrees a cm at 15 cm: 45 degrees, the wrist 11 cm down, nearly all the knock's lift undone).
        const hands::State& s = hands::current();
        glm::vec3 f, r, u;
        hands::angleVectors(s.rot[hand], f, r, u);
        const float a = glm::radians(-angles.x);
        pos += (wristBack * 0.01f * units::metresToUnits()) * ((za::cos(a) - 1.f) * f + za::sin(a) * u);
    }

    if(vr_debug_pain.value && hand == HAND_MAIN && cmNow > 0.f && realtime != printedAt)
    {
        printedAt = realtime;
        const hands::State& s = hands::current();
        glm::vec3 fwd, right, up;
        hands::angleVectors({0.f, s.bodyYaw, 0.f}, fwd, right, up);
        float offCm = 0.f;
        const glm::vec3 off = knockOf(HAND_OFF, offCm);
        const float toCm = 1.f / (0.01f * units::metresToUnits());
        Con_Printf("painknock t %.3f main %.2f cm seen of %.2f (right %.2f fwd %.2f up %.2f) off %.2f seen of %.2f (right %.2f "
                   "fwd %.2f up %.2f)\n",
            cl.time, cmNow, glm::length(pos) * toCm, glm::dot(pos, right) * toCm, glm::dot(pos, fwd) * toCm, pos.z * toCm, offCm,
            glm::length(off) * toCm, glm::dot(off, right) * toCm, glm::dot(off, fwd) * toCm, off.z * toCm);
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
