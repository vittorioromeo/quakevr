// vr_ragstruggle.cpp -- a living knockdown struggling in a player's hands (vr_struggle; vr_ragactive.hpp, "Struggling in
// the hands"; docs/vr-port/STRUGGLE.md).
//
// One layer, not one move per hold: each frame the context (what holds it and with which hands, how much of it lies on
// the floor, how fast and which way it is dragged, which way up it is, where the player and his hands are, its effort)
// gives each behaviour a weight; the weights ease towards it (vr_struggle_blend); each behaviour gives each limb a goal
// (a Reach: two-bone IK over the drive's pose), the torso and the head a bend, and the channels a strength; the limbs'
// goals are the weighted mean of their behaviours', the bends likewise, so a change of hold blends from one struggle into
// the next. The goals that aim at the player or at his hands follow them live. Its own rhythm (seeded), its effort (a
// ramp in, bursts and lulls, fatigue, a flinch when hit, its health) and a guard that eases it off as the hold lags make
// two grunts never move alike and the hold never break from its struggle alone.

#include "vr_ragactive.hpp"

#include "vr_b3glm.hpp"
#include "vr_cvars.hpp"
#include "vr_geom.hpp"
#include "vr_ragpose.hpp"

#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"

namespace qvr::ragactive
{

namespace
{

using b3glm::b3v;
using b3glm::fromB3;
using b3glm::glmv;

using geom::smooth01;

// The struggles' cost (vr_struggle_status): seconds and frames since the last look, how many struggles.
struct StruggleCost
{
    double seconds{0.0};
    int frames{0};
    int count{0};
    double lastFrame{-1.0};
};
StruggleCost struggleCost;

constexpr float twoPi = 6.2831853f;
constexpr float pi = 3.14159265f;
constexpr int senseEvery = 3;       // frames between its looks at the floor
constexpr float rampIn = 0.35f;     // s: its effort up from nothing as it begins (taken by surprise)
constexpr float fadeOut = 0.35f;    // s: its strengths let go over this as it ends
constexpr float flinchTime = 0.5f;  // s: a hit's flinch eases off over this
constexpr float touching = 0.14f;   // m: a part's middle this near the floor lies on it,
constexpr float touchSpan = 0.22f;  // ... and is off it this much higher
constexpr float tugBack = 0.05f;    // m: a prying hand tugs the holder's hand this far back (and as much again)
constexpr int traceEvery = 18;      // frames between the `struggle:` lines (vr_struggle_debug 2)
const glm::vec3 ax{1.f, 0.f, 0.f}, ay{0.f, 1.f, 0.f}, az{0.f, 0.f, 1.f};

// Each behaviour's rate (Hz: a swing, a tug, a grasp, a stroke, a kick's two places) and its muscles' share on the arms,
// the legs, the torso and the head (by weight; summed, then times its effort).
struct BehaviourData
{
    float hz;
    float arms, legs, torso, head;
};
constexpr BehaviourData behaviourData[behaviourCount] = {
    {1.f, 1.f, 0.f, 0.5f, 0.5f},      // Swat
    {1.6f, 0.9f, 0.f, 0.45f, 0.4f},   // Pry
    {1.2f, 0.85f, 1.2f, 1.2f, 0.5f},  // Crunch
    {0.9f, 0.85f, 0.f, 0.55f, 0.5f},  // Claw
    {0.7f, 0.f, 1.1f, 0.5f, 0.45f},   // Brace
    {1.4f, 0.f, 0.7f, 0.f, 0.f},      // Kick
    {1.6f, 0.f, 0.f, 0.45f, 0.4f}     // Thrash
};
constexpr const char* behaviourNames[behaviourCount] = {"swat", "pry", "crunch", "claw", "brace", "kick", "thrash"};

[[nodiscard]] uint32_t mixBits(uint32_t a, uint32_t b)
{
    uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    h *= 0x297A2D39u;
    h ^= h >> 15;
    return h;
}

// A roll in [0, 1) from `seed` and what it is for.
[[nodiscard]] float roll(uint32_t seed, uint32_t what)
{
    return static_cast<float>(mixBits(seed, what) >> 8) / static_cast<float>(1u << 24);
}

// Smooth noise in [-1, 1] at `x` (>= 0; a new value each unit).
[[nodiscard]] float noise(uint32_t seed, float x)
{
    const auto k = static_cast<uint32_t>(za::max(x, 0.f));
    const float f = x - static_cast<float>(k);
    const float a = roll(seed, k) * 2.f - 1.f, b = roll(seed, k + 1u) * 2.f - 1.f;
    return a + (b - a) * smooth01(f);
}

[[nodiscard]] bool cutPart(const Parts& p, int b)
{
    return (p.cut & (1u << b)) != 0u;
}

[[nodiscard]] glm::quat rotOf(const Parts& p, int b)
{
    return fromB3(b3Body_GetRotation(p.body[b]));
}

[[nodiscard]] glm::vec3 middleOf(const Parts& p, int b)
{
    return glmv(b3Body_GetWorldCenter(p.body[b]));
}

// Bone `b`'s rest-space point `rest` (units) in the world now (metres).
[[nodiscard]] glm::vec3 pointOf(const Parts& p, int b, const glm::vec3& rest)
{
    return glmv(b3Body_GetWorldPoint(p.body[b], b3v(rest * p.k)));
}

[[nodiscard]] bool jointOk(const State& s, const Parts& p, int b)
{
    const ragdoll::Bone& bone = p.rig->bones[b];
    const b3JointId j = s.joint[static_cast<za::SizeT>(b)];
    return bone.parent >= 0 && (bone.joint == ragdoll::Joint::Ball || bone.joint == ragdoll::Joint::Hinge) &&
           !cutPart(p, b) && !cutPart(p, bone.parent) && (p.loose & (1u << b)) == 0u && B3_IS_NON_NULL(j) &&
           b3Joint_IsValid(j);
}

[[nodiscard]] int upperOf(const State& s, int limb)
{
    return s.chain[static_cast<za::SizeT>(limb)][0];
}

[[nodiscard]] int lowerOf(const State& s, int limb)
{
    return s.chain[static_cast<za::SizeT>(limb)][1];
}

[[nodiscard]] bool limbOk(const State& s, const Parts& p, int limb)
{
    const int u = upperOf(s, limb), lo = lowerOf(s, limb);
    return u >= 0 && lo >= 0 && jointOk(s, p, u) && jointOk(s, p, lo);
}

[[nodiscard]] float limbLen(const State& s, const Parts& p, int limb)
{
    const int u = upperOf(s, limb), lo = lowerOf(s, limb);
    if(u < 0 || lo < 0)
    {
        return 0.f;
    }
    const ragdoll::Bone& U = p.rig->bones[u];
    const ragdoll::Bone& L = p.rig->bones[lo];
    return (glm::length(L.pivot - U.pivot) + glm::length(L.end - L.pivot)) * p.k;
}

[[nodiscard]] glm::vec3 limbRoot(const State& s, const Parts& p, int limb)
{
    const int u = upperOf(s, limb);
    return pointOf(p, u, p.rig->bones[u].pivot);
}

[[nodiscard]] glm::vec3 limbTip(const State& s, const Parts& p, int limb)
{
    const int lo = lowerOf(s, limb);
    return pointOf(p, lo, p.rig->bones[lo].end);
}

[[nodiscard]] bool onBone(const ragdoll::Rig& rig, int b, int top)
{
    for(int x = b; x >= 0; x = rig.bones[x].parent)
    {
        if(x == top)
        {
            return true;
        }
    }
    return false;
}

// The limb part `b` is on (Limb), -1 none (the torso, the head).
[[nodiscard]] int limbOfPart(const State& s, const Parts& p, int b)
{
    for(int limb = 0; limb < limbCount; limb++)
    {
        const int u = upperOf(s, limb);
        if(u >= 0 && onBone(*p.rig, b, u))
        {
            return limb;
        }
    }
    return -1;
}

[[nodiscard]] int chestPart(const Parts& p)
{
    for(int b = 0; b < p.count; b++)
    {
        if(p.rig->bones[b].role == modelmeta::BoneRole::Chest && !cutPart(p, b))
        {
            return b;
        }
    }
    return 0;
}

[[nodiscard]] int headPart(const Parts& p)
{
    const int h = p.rig->head;
    return h > 0 && h < p.count && !cutPart(p, h) ? h : -1;
}

[[nodiscard]] glm::vec3 level(const glm::vec3& v)
{
    return glm::vec3{v.x, v.y, 0.f};
}

[[nodiscard]] glm::vec3 unit(const glm::vec3& v, const glm::vec3& fallback)
{
    const float l = glm::length(v);
    return l > 1e-4f ? v / l : fallback;
}

// `goal` brought within `most` metres of `root`.
[[nodiscard]] glm::vec3 within(const glm::vec3& root, const glm::vec3& goal, float most)
{
    const glm::vec3 d = goal - root;
    const float l = glm::length(d);
    return l > most && l > 1e-4f ? root + d * (most / l) : goal;
}

// The holding hand nearest `from` (world, metres) but one holding limb `skip`'s arm (-1 none); false: none.
[[nodiscard]] bool nearestHold(const State& s, const Parts& p, const StruggleInput& in, const glm::vec3& from, int skip,
    glm::vec3& out)
{
    float best = 1e30f;
    for(int i = 0; i < in.holds; i++)
    {
        const StruggleHold& h = in.hold[static_cast<za::SizeT>(i)];
        if(skip >= 0 && h.part >= 0 && limbOfPart(s, p, h.part) == skip)
        {
            continue;
        }
        const float d = glm::distance(h.at, from);
        if(d < best)
        {
            best = d;
            out = h.at;
        }
    }
    return best < 1e29f;
}

// A kicking leg's k-th place (its frame: x forward, y left, z up): mostly down its line, thrown forward and aside.
[[nodiscard]] glm::vec3 kickDir(uint32_t seed, int limb, uint32_t k)
{
    const uint32_t h = mixBits(seed, static_cast<uint32_t>(limb) * 977u + k);
    const glm::vec3 r{roll(h, 1u) * 2.f - 1.f, roll(h, 2u) * 2.f - 1.f, roll(h, 3u) * 2.f - 1.f};
    const float side = (limb % 2) == 0 ? 1.f : -1.f;
    return unit(r * 0.8f + glm::vec3{0.45f, 0.2f * side, -0.9f}, glm::vec3{0.f, 0.f, -1.f});
}

void ease(float& v, float to, float dt, float tau)
{
    v += (to - v) * (1.f - za::exp(-dt / za::max(tau, 1e-3f)));
}

// What it senses: its motion and which way up it is each frame, the floor under its pelvis, chest and head every few.
void sense(State& s, const Parts& p, const Context& c)
{
    Struggle& g = s.struggle;
    const int chest = chestPart(p), head = headPart(p);
    const glm::vec3 v = 0.5f * (glmv(b3Body_GetLinearVelocity(p.body[0])) + glmv(b3Body_GetLinearVelocity(p.body[chest])));
    const glm::vec3 lv = level(v);
    const float speed = glm::length(lv);
    if(speed > 0.05f)
    {
        g.dragWay = lv / speed;
    }
    const glm::quat qc = rotOf(p, chest);
    g.prone = smooth01((-(qc * ax).z - 0.3f) / 0.5f);
    g.inverted = smooth01((-(qc * az).z - 0.3f) / 0.5f);
    if(g.frame % senseEvery == 0 && c.floorBelow)
    {
        float on = 0.f;
        int n = 0;
        for(const int b : {0, chest, head})
        {
            if(b < 0 || (n > 0 && b == 0))
            {
                continue;
            }
            const glm::vec3 at = middleOf(p, b);
            const float fl = c.floorBelow(at, 2.f);
            if(b == 0)
            {
                g.floor = fl;
            }
            const float h = fl < -1e29f ? 9.f : at.z - fl;
            on += 1.f - smooth01((h - touching) / touchSpan);
            n++;
        }
        g.contact = n > 0 ? on / static_cast<float>(n) : 0.f;
    }
    g.frame++;
    g.lift = 1.f - g.contact;
    g.drag = smooth01((speed - 0.1f) / 0.5f) * g.contact;
}

// What holds it (0..1 each region) and how far its holds lag (units), from the holds.
void holds(State& s, const Parts& p, const StruggleInput& in, float dt)
{
    Struggle& g = s.struggle;
    float torso = 0.f, head = 0.f, legs = 0.f, arms = 0.f, arm = 0.f, lag = 0.f;
    for(int i = 0; i < in.holds; i++)
    {
        const StruggleHold& h = in.hold[static_cast<za::SizeT>(i)];
        if(h.part < 0 || h.part >= p.count)
        {
            continue;
        }
        lag = za::max(lag, glm::distance(h.at, h.grip) * p.m2u);
        const Channel ch = channelOf(p.rig->bones[h.part]);
        const int limb = limbOfPart(s, p, h.part);
        arm = limb == static_cast<int>(Limb::ArmL) || limb == static_cast<int>(Limb::ArmR) ? 1.f : arm;
        if(ch == Channel::Head)
        {
            head = 1.f;
        }
        else if(ch == Channel::Legs || limb == static_cast<int>(Limb::LegL) || limb == static_cast<int>(Limb::LegR))
        {
            legs = 1.f;
        }
        else if(limb >= 0 && h.part != upperOf(s, limb))
        {
            arms = 1.f; // (a forearm, a hand)
        }
        else
        {
            torso = 1.f; // (the torso, an upper arm: its shoulder)
        }
    }
    g.heldTorso = torso;
    g.heldHead = head;
    g.heldLegs = legs;
    g.heldArms = arms;
    g.heldArm = arm;
    g.gripError = lag;
    g.gripErrorMost = za::max(g.gripErrorMost, lag);
    // The guard: past vr_struggle_grip_guard units it eases off, to a quarter at 2.5 times it; quick down, slow up.
    const float from = za::max(vr_struggle_grip_guard.value, 0.f);
    const float want = from > 0.f ? 1.f - 0.75f * smooth01((lag - from) / (1.5f * from)) : 1.f;
    ease(g.guard, want, dt, want < g.guard ? 0.05f : 0.4f);
}

// Its effort now: its ramp in, its fatigue, its bursts and lulls, a hit's flinch, its health, its class.
void effort(State& s, const StruggleInput& in, const Context& c)
{
    Struggle& g = s.struggle;
    if(g.struck)
    {
        g.hitAt = c.now;
        g.struck = false;
    }
    const float ramp = smooth01(static_cast<float>(c.now - g.from) / rampIn);
    const float tireTime = vr_struggle_fatigue.value;
    if(tireTime > 0.f)
    {
        g.fatigue = za::min(1.f, g.fatigue + c.dt / tireTime * za::clamp(g.effort, 0.f, 1.2f));
    }
    const float base = 1.f - g.fatigue * (1.f - za::clamp(vr_struggle_tired.value, 0.f, 1.f));
    const float bursts = za::clamp(vr_struggle_bursts.value, 0.f, 1.f);
    if(bursts > 0.f && c.now >= g.nextBurst)
    {
        const uint32_t k = g.bursts++;
        g.burstFrom = c.now;
        g.burstUntil = c.now + 0.35 + 0.45 * static_cast<double>(roll(g.seed, 100u + k));
        g.nextBurst = g.burstUntil + (0.9 + 2.2 * static_cast<double>(roll(g.seed, 200u + k))) / static_cast<double>(g.rate);
    }
    float fit = 0.f;
    const bool inBurst = c.now >= g.burstFrom && c.now < g.burstUntil;
    if(inBurst)
    {
        fit = za::sin(pi * static_cast<float>((c.now - g.burstFrom) / za::max(g.burstUntil - g.burstFrom, 1e-3)));
    }
    const float burst = 1.f + bursts * (0.5f * fit - (inBurst ? 0.f : 0.2f));
    const float flinch = 0.2f + 0.8f * smooth01(static_cast<float>(c.now - g.hitAt) / flinchTime);
    const float health = 0.45f + 0.55f * za::clamp(in.health, 0.f, 1.f);
    g.effort = za::clamp(ramp * base * burst * flinch * health * in.strength, 0.f, 1.5f);
}

// The behaviours' weights: what the context wants of each, times its setting, eased.
void weights(State& s, const Context& c)
{
    Struggle& g = s.struggle;
    const float air = 1.f - g.contact;
    za::Array<float, behaviourCount>& w = g.want;
    const auto at = [&](Behaviour b) -> float& { return w[static_cast<za::SizeT>(b)]; };
    // (Held by an arm: its other hand swats at the holding one too; held by the legs lying on its back: it crunches up
    // at the hands rather than only clawing, the claw's roll onto its belly less.)
    at(Behaviour::Swat) = za::min(1.f, g.heldTorso * (0.55f + 0.45f * air) + 0.7f * g.heldArms) *
                          za::clamp(vr_struggle_swat.value, 0.f, 2.f);
    at(Behaviour::Pry) = za::min(1.f, g.heldHead + 0.8f * g.heldArms + 0.25f * g.heldTorso * g.contact) *
                         za::clamp(vr_struggle_pry.value, 0.f, 2.f);
    at(Behaviour::Crunch) = g.heldLegs * za::min(1.f, air * g.inverted + 0.6f * g.contact * (1.f - g.prone)) *
                            za::clamp(vr_struggle_crunch.value, 0.f, 2.f);
    at(Behaviour::Claw) = g.heldLegs * g.contact * (0.35f + 0.65f * g.drag) * (0.4f + 0.6f * g.prone) *
                          za::clamp(vr_struggle_claw.value, 0.f, 2.f);
    // Fighting to stay up (dragged by an arm, not by its head or legs): its legs under it, its chest righted.
    const float up = g.heldArm * (1.f - g.heldLegs) * (1.f - g.heldHead) * za::clamp(vr_struggle_upright.value, 0.f, 2.f);
    at(Behaviour::Brace) = (1.f - g.heldLegs) * za::min(1.f, g.heldArms + 0.6f * g.heldHead) * g.contact *
                           (0.35f + 0.65f * g.drag) * (1.f - 0.7f * za::min(up, 1.f)) *
                           za::clamp(vr_struggle_brace.value, 0.f, 2.f);
    const float flail = za::clamp(vr_struggle_flail.value, 0.f, 2.f);
    at(Behaviour::Kick) = (1.f - g.heldLegs) * (1.f - 0.8f * za::min(at(Behaviour::Brace), 1.f)) * (1.f - 0.6f * za::min(up, 1.f)) *
                          (0.35f + 0.65f * air) * flail;
    // (Held by the head or the legs it squirms: more thrashing.)
    at(Behaviour::Thrash) = (0.5f + 0.5f * air + 0.8f * za::max(g.heldHead, g.heldLegs) * za::clamp(vr_struggle_squirm.value, 0.f, 2.f)) *
                            flail;
    const float tau = za::clamp(vr_struggle_blend.value, 0.02f, 3.f);
    ease(g.upright, up, c.dt, tau);
    for(int b = 0; b < behaviourCount; b++)
    {
        const auto i = static_cast<za::SizeT>(b);
        w[i] = za::max(w[i], 0.f);
        ease(g.weight[i], w[i], c.dt, tau);
    }
}

[[nodiscard]] float weightOf(const Struggle& g, Behaviour b)
{
    return g.weight[static_cast<za::SizeT>(b)];
}

// A frantic slap at `T` (a holding hand; world, metres) by arm `arm` from its shoulder `root`: drawn back along the way
// to it and struck in on a quicker beat of its own, the beat and the aim jittering (its seed's noise), within its reach.
[[nodiscard]] glm::vec3 slapAt(const Struggle& g, int arm, const glm::vec3& root, float len, const glm::vec3& T,
    const glm::vec3& L, const glm::vec3& U, float ph)
{
    const uint32_t sd = g.seed ^ (0x9100u + static_cast<uint32_t>(arm) * 0x35u);
    const float strike = 0.5f + 0.5f * za::sin(ph * 1.35f + 1.4f * noise(sd, ph * 0.45f)); // (0 drawn back, 1 on it)
    const glm::vec3 n = unit(T - root, U);
    const glm::vec3 jitter = (L * noise(sd ^ 1u, ph * 0.9f) + U * noise(sd ^ 2u, ph * 0.9f + 7.f) +
                                 n * (0.5f * noise(sd ^ 3u, ph * 0.7f + 3.f))) *
                             (0.16f * len);
    return within(root, T + jitter - n * (0.4f * len * (1.f - strike)), 1.05f * len);
}

// The arms' goals (each a blend of its behaviours'), their reaches set.
void armGoals(State& s, const Parts& p, const StruggleInput& in, float armsK, uint32_t heldLimbsBits)
{
    Struggle& g = s.struggle;
    const int chest = chestPart(p);
    const glm::quat qc = rotOf(p, chest);
    const glm::vec3 F = qc * ax, L = qc * ay, U = qc * az;
    const glm::vec3 playerChest = in.feet + (in.eye - in.feet) * 0.72f;
    const float var = za::clamp(vr_struggle_variation.value, 0.f, 1.f);
    const float frantic = za::clamp(vr_struggle_frantic.value, 0.f, 2.f);
    for(int arm = 0; arm < 2; arm++)
    {
        Reach& r = s.reach[static_cast<za::SizeT>(arm)];
        if(!limbOk(s, p, arm) || (heldLimbsBits & (1u << arm)) != 0u)
        {
            r.weight = 0.f;
            continue;
        }
        const glm::vec3 root = limbRoot(s, p, arm);
        const float len = limbLen(s, p, arm);
        const float side = arm == 0 ? 1.f : -1.f;
        const float ph = g.phase[static_cast<za::SizeT>(arm)];
        const float cyc = ph / twoPi;
        const auto k = static_cast<uint32_t>(za::max(cyc, 0.f));
        const float u = cyc - static_cast<float>(k);
        glm::vec3 sum{0.f};
        float wsum = 0.f;
        const auto add = [&](const glm::vec3& goal, float w) {
            sum += goal * w;
            wsum += w;
        };
        // (Its aim wanders a little: each enemy its own.)
        const glm::vec3 wander = (L * noise(g.seed ^ 0x51u, ph * 0.4f) + U * noise(g.seed ^ 0x77u, ph * 0.4f + 3.f)) *
                                 (0.12f * len * (0.3f + var));
        glm::vec3 hand{0.f};
        const bool anyHold = nearestHold(s, p, in, root, arm, hand);
        if(const float w = weightOf(g, Behaviour::Swat); w > 0.01f)
        {
            // A swing at the holding hand or the head (the cycle's roll picks), from a wind-up raised back and out.
            const bool atHead = !anyHold || roll(g.seed ^ static_cast<uint32_t>(arm), k) < 0.4f;
            const glm::vec3 T = (atHead ? in.eye : hand) + wander;
            const glm::vec3 to = unit(T - root, F);
            const glm::vec3 rest = root + to * (0.45f * len) - U * (0.25f * len);
            const glm::vec3 wind = root + U * (0.55f * len) - to * (0.3f * len) + L * (side * 0.45f * len);
            const glm::vec3 strike = within(root, T, 1.05f * len);
            glm::vec3 goal;
            if(u < 0.4f)
            {
                goal = glm::mix(rest, wind, smooth01(u / 0.4f));
            }
            else if(u < 0.62f)
            {
                goal = glm::mix(wind, strike, smooth01((u - 0.4f) / 0.22f));
            }
            else
            {
                goal = glm::mix(strike, rest, smooth01((u - 0.62f) / 0.38f));
            }
            add(goal, w * (arm == g.favourite ? 1.f : 0.75f));
        }
        if(const float w = weightOf(g, Behaviour::Pry); w > 0.01f && anyHold)
        {
            // To the holding hand, tugging it back; a hand out of reach still reaches for it (less).
            const float d = glm::distance(hand, root);
            const float can = 1.f - 0.7f * smooth01((d - 1.1f * len) / (0.8f * len));
            const glm::vec3 n = unit(hand - root, F);
            const glm::vec3 tug = within(root, hand - n * (tugBack * (1.f + za::sin(ph))), 1.08f * len);
            // (Held by the head, or an arm: slapping at the hand frantically rather than only tugging at it.)
            const float slap = za::min(1.f, frantic * (g.heldHead + 0.5f * g.heldArms));
            // (On its head: at his wrist and forearm over it, the hands up over its head.)
            const glm::vec3 wrist = hand + unit(in.eye - hand, U) * 0.1f + U * 0.1f;
            const glm::vec3 at = glm::mix(hand, wrist, g.heldHead);
            add(slap > 0.01f ? glm::mix(tug, slapAt(g, arm, root, len, at, L, U, ph), slap) : tug, w * can);
        }
        if(const float w = weightOf(g, Behaviour::Crunch); w > 0.01f)
        {
            // Grasping for the player: his hands holding its legs, his middle; slapping at those hands.
            const glm::vec3 T = anyHold ? glm::mix(hand, playerChest, 0.35f) : playerChest;
            const glm::vec3 grasp = (L * za::cos(ph) + F * za::sin(ph)) * (0.12f * len);
            const glm::vec3 reachFor = within(root, T + grasp + wander, 1.1f * len);
            const float slap = anyHold ? za::min(1.f, 0.6f * frantic) : 0.f;
            add(slap > 0.01f ? glm::mix(reachFor, slapAt(g, arm, root, len, hand, L, U, ph), slap) : reachFor, w);
        }
        if(const float w = weightOf(g, Behaviour::Claw); w > 0.01f)
        {
            // A stroke ahead of its head along the floor (against its drag), pressed down, pulled back.
            const glm::vec3 lu = level(U);
            const glm::vec3 A = glm::length(lu) > 0.3f ? glm::normalize(lu) : -g.dragWay;
            const float e = u < 0.55f ? smooth01(u / 0.55f) : 1.f - smooth01((u - 0.55f) / 0.45f);
            glm::vec3 goal = root + A * (len * (0.35f + 0.6f * e)) + level(L) * (side * 0.15f * len);
            goal.z = g.floor > -1e29f ? g.floor + 0.02f : root.z - 0.3f * len;
            add(goal, w);
        }
        if(wsum <= 0.01f)
        {
            r.weight = 0.f;
            continue;
        }
        r.goal = sum / wsum;
        r.weight = za::min(1.f, wsum);
        r.strength = armsK;
    }
}

// The legs' goals: a kick about (free legs), braced straight along the body, heels in the floor (dragged by an arm).
void legGoals(State& s, const Parts& p, float legsK, uint32_t heldLimbsBits)
{
    Struggle& g = s.struggle;
    const glm::quat qp = rotOf(p, 0);
    const glm::vec3 F = qp * ax, L = qp * ay, U = qp * az;
    for(int leg = 2; leg < 4; leg++)
    {
        Reach& r = s.reach[static_cast<za::SizeT>(leg)];
        if(!limbOk(s, p, leg) || (heldLimbsBits & (1u << leg)) != 0u)
        {
            r.weight = 0.f;
            continue;
        }
        const glm::vec3 root = limbRoot(s, p, leg);
        const float len = limbLen(s, p, leg);
        glm::vec3 sum{0.f};
        float wsum = 0.f;
        if(const float w = weightOf(g, Behaviour::Kick); w > 0.01f)
        {
            const float f = g.phase[static_cast<za::SizeT>(leg)] / twoPi * 2.f;
            const auto k = static_cast<uint32_t>(za::max(f, 0.f));
            const glm::vec3 d = unit(glm::mix(kickDir(g.seed, leg, k), kickDir(g.seed, leg, k + 1u), smooth01(f - static_cast<float>(k))),
                az);
            sum += (root + (F * d.x + L * d.y + U * d.z) * (0.85f * len)) * w;
            wsum += w;
        }
        if(const float w = weightOf(g, Behaviour::Brace); w > 0.01f)
        {
            glm::vec3 goal = root - U * (0.98f * len) + L * (((leg == 2) ? 1.f : -1.f) * 0.06f * len);
            goal.z -= 0.05f; // (the heels dug in)
            sum += goal * w;
            wsum += w;
        }
        if(const float w = za::min(g.upright, 1.f) * 1.2f; w > 0.01f)
        {
            // Staying up: its feet (its knees bent) on the floor under its hip, a little ahead of it and apart.
            const glm::vec3 fl = level(F);
            const glm::vec3 ahead = glm::length(fl) > 0.2f ? glm::normalize(fl) : -g.dragWay;
            glm::vec3 goal = root + ahead * (0.3f * len) + level(L) * (((leg == 2) ? 1.f : -1.f) * 0.12f * len);
            goal.z = g.floor > -1e29f ? za::max(g.floor + 0.03f, root.z - 0.9f * len) : root.z - 0.7f * len;
            sum += within(root, goal, 0.95f * len) * w;
            wsum += w;
        }
        if(wsum <= 0.01f)
        {
            r.weight = 0.f;
            continue;
        }
        r.goal = sum / wsum;
        r.weight = za::min(1.f, wsum);
        r.strength = za::max(legsK, 0.9f * za::min(g.upright, 1.f) * g.effort * za::max(g.guard, 0.5f));
    }
}

// The torso's and head's bends (their targets: each its turn in its parent's rest space, x forward, y left, z up), and
// a held leg's hip drawn up as it crunches.
void bodyTargets(State& s, const Parts& p, const StruggleInput& in, uint32_t heldLimbsBits, float since)
{
    Struggle& g = s.struggle;
    const int chest = chestPart(p), head = headPart(p);
    const float ph = g.phase[static_cast<za::SizeT>(g.favourite)];
    const float nt = since * 1.6f * g.rate * za::clamp(vr_struggle_speed.value, 0.2f, 3.f);
    // (A crunch is always forward: hanging by its legs either way round, its middle folds and its head comes up.)
    constexpr float front = 1.f;
    float flex = 0.f, twist = 0.f, lean = 0.f, sum = 0.f;
    const auto add = [&](Behaviour b, float f, float tw, float ln) {
        const float w = weightOf(g, b);
        flex += f * w;
        twist += tw * w;
        lean += ln * w;
        sum += w;
    };
    add(Behaviour::Swat, 0.15f, 0.4f * za::sin(ph) * (g.favourite == 0 ? -1.f : 1.f), 0.f);
    add(Behaviour::Pry, 0.3f, 0.2f * za::sin(ph * 0.5f), 0.f);
    add(Behaviour::Crunch, front * (0.8f + 0.2f * za::sin(ph)), 0.f, 0.f);
    add(Behaviour::Claw, -0.25f, 0.15f * za::sin(ph), 0.f);
    add(Behaviour::Brace, 0.1f, 0.f, 0.f);
    add(Behaviour::Thrash, 0.3f * noise(g.seed ^ 0x101u, nt), 0.45f * noise(g.seed ^ 0x202u, nt), 0.3f * noise(g.seed ^ 0x303u, nt));
    if(sum > 1e-3f)
    {
        const float n = za::max(sum, 1.f); // (weights under 1 bend it less)
        int spine = 0;
        for(int b = 1; b < p.count; b++)
        {
            spine += channelOf(p.rig->bones[b]) == Channel::Torso && jointOk(s, p, b) ? 1 : 0;
        }
        const float share = 1.f / static_cast<float>(za::max(spine, 1));
        const glm::quat q = glm::angleAxis(twist / n * share, az) * glm::angleAxis(lean / n * share, ax) *
                            glm::angleAxis(flex / n * share, ay);
        for(int b = 1; b < p.count; b++)
        {
            if(channelOf(p.rig->bones[b]) == Channel::Torso && jointOk(s, p, b))
            {
                s.target[static_cast<za::SizeT>(b)] = glm::normalize(q);
            }
        }
    }
    // The head: looking at the player (the aimed behaviours), up ahead (clawing), tossed (thrashing).
    if(head >= 0 && jointOk(s, p, head))
    {
        const glm::vec3 dl = glm::inverse(rotOf(p, chest)) * (in.eye - middleOf(p, head));
        const float yawTo = za::clamp(za::atan2(dl.y, dl.x), -0.8f, 0.8f);
        const float pitchTo = za::clamp(-za::atan2(dl.z, glm::length(glm::vec2{dl.x, dl.y})), -0.6f, 0.6f);
        const float look = weightOf(g, Behaviour::Swat) + weightOf(g, Behaviour::Pry) + weightOf(g, Behaviour::Crunch) +
                           weightOf(g, Behaviour::Brace);
        const float claw = weightOf(g, Behaviour::Claw), thrash = weightOf(g, Behaviour::Thrash);
        const float hs = look + claw + thrash;
        if(hs > 1e-3f)
        {
            const float n = za::max(hs, 1.f);
            const float yaw = (yawTo * look + 0.5f * noise(g.seed ^ 0x404u, nt) * thrash) / n;
            const float pitch = (pitchTo * look - 0.5f * claw + 0.3f * noise(g.seed ^ 0x505u, nt) * thrash) / n;
            s.target[static_cast<za::SizeT>(head)] = glm::normalize(glm::angleAxis(yaw, az) * glm::angleAxis(pitch, ay));
        }
    }
    // A held leg's hip (its joint at vr_struggle_held_share): drawn up towards the belly as it crunches.
    if(const float w = weightOf(g, Behaviour::Crunch); w > 0.01f)
    {
        for(int leg = 2; leg < 4; leg++)
        {
            const int u = upperOf(s, leg);
            if((heldLimbsBits & (1u << leg)) != 0u && u >= 0 && jointOk(s, p, u))
            {
                s.target[static_cast<za::SizeT>(u)] = glm::normalize(glm::angleAxis(-1.4f * za::min(w, 1.f), ay));
            }
        }
    }
}

// Clawing on the floor: its body turned about its spine towards belly down (the floor's friction against it; a hand of
// God's torque on its pelvis and chest, capped).
void rollOver(State& s, const Parts& p, const Context& c)
{
    Struggle& g = s.struggle;
    g.rollTorque = 0.f;
    const float w = weightOf(g, Behaviour::Claw) * g.contact * g.effort * g.guard;
    if(w < 0.02f)
    {
        return;
    }
    const int chest = chestPart(p);
    const glm::quat qc = rotOf(p, chest);
    const glm::vec3 U = qc * az, F = qc * ax;
    const glm::vec3 down{0.f, 0.f, -1.f};
    const glm::vec3 want = down - U * glm::dot(down, U), now = F - U * glm::dot(F, U);
    if(glm::length(want) < 0.2f || glm::length(now) < 0.2f)
    {
        return; // (standing on its head or lying along the vertical: no way to roll)
    }
    const glm::vec3 a = glm::normalize(now), b = glm::normalize(want);
    const float off = za::atan2(glm::dot(glm::cross(a, b), U), glm::dot(a, b));
    const float rate = za::clamp(off * 3.f, -2.5f, 2.5f) * za::min(w, 1.f);
    const float most = 0.06f * s.totalMass * c.gravity * 0.3f; // N m
    for(const int part : {0, chest})
    {
        const float m = b3Body_GetMass(p.body[part]);
        const float spin = glm::dot(glmv(b3Body_GetAngularVelocity(p.body[part])), U);
        const float tq = za::clamp(m * 0.0144f * (rate - spin) / 0.15f, -most, most); // (I ~ m (12 cm)^2)
        b3Body_ApplyTorque(p.body[part], b3v(U * tq), true);
        g.rollTorque += tq;
        if(part == chest)
        {
            break;
        }
    }
}

// A force on part `b` this frame (N, world): applied, and kept in `p.force` for the step's later pieces.
void pushOn(const Parts& p, int b, const glm::vec3& f)
{
    b3Body_ApplyForceToCenter(p.body[b], b3v(f), true);
    if(p.force)
    {
        p.force[b] += f;
    }
}

// Crunching up (hung by its legs): its abdominals' pull, the chest and head drawn up and the held legs' thighs down as
// much (a force between its own parts: the hold carries none of it), in pulses.
void crunchPull(State& s, const Parts& p, const Context& c, uint32_t heldLimbsBits)
{
    Struggle& g = s.struggle;
    const float w = za::min(weightOf(g, Behaviour::Crunch), 1.f) * g.effort * g.guard *
                    za::clamp(vr_struggle_strength.value, 0.f, 2.f);
    if(w < 0.02f)
    {
        return;
    }
    const int chest = chestPart(p), head = headPart(p);
    int thighs[2]{-1, -1};
    int n = 0;
    for(int leg = 2; leg < 4; leg++)
    {
        const int u = upperOf(s, leg);
        if((heldLimbsBits & (1u << leg)) != 0u && u >= 0 && !cutPart(p, u))
        {
            thighs[n++] = u;
        }
    }
    if(n == 0)
    {
        return;
    }
    const float pulse = 0.75f + 0.25f * za::sin(g.phase[static_cast<za::SizeT>(g.favourite)] * 0.5f);
    const float f = 0.3f * s.totalMass * c.gravity * w * pulse; // N: up on its top, down on its thighs
    const glm::vec3 up{0.f, 0.f, f};
    pushOn(p, chest, head >= 0 ? up * 0.6f : up);
    if(head >= 0)
    {
        pushOn(p, head, up * 0.4f);
    }
    for(int i = 0; i < n; i++)
    {
        pushOn(p, thighs[i], -up / static_cast<float>(n));
    }
}

// Staying up, dragged by an arm (Upright): its legs under it hold it up (a force up on its pelvis and chest, at most
// 0.8 of its weight as its middle sinks below a crouch, as far as its chest is up past 0.2: slumped or flat, none), its back righting
// its chest (a capped turn towards up: from flat, trying to sit up). The hold and the floor take the rest: never lifted.
void stayUp(State& s, const Parts& p, const Context& c)
{
    Struggle& g = s.struggle;
    g.uprightForce = 0.f;
    // (Not eased by the grip guard: holding its own weight up eases the hold's load, it never pulls against it.)
    const float w = za::min(g.upright, 1.f) * g.effort * za::clamp(vr_struggle_strength.value, 0.f, 2.f);
    if(w < 0.02f || g.floor < -1e29f || !limbOk(s, p, 2) || !limbOk(s, p, 3))
    {
        return;
    }
    const int chest = chestPart(p);
    const float legs = 0.5f * (limbLen(s, p, 2) + limbLen(s, p, 3));
    const float high = middleOf(p, 0).z - g.floor;
    const glm::vec3 cu = rotOf(p, chest) * az;
    // (Its legs hold it up as far as it is up: its weight on its feet, its middle kept off the floor; lying flat its
    // back can only try to sit it up, below.)
    const float low = smooth01((0.9f * legs - high) / (0.5f * legs)); // (0 at a crouch's height and over)
    const float onFeet = smooth01((cu.z - 0.2f) / 0.5f);
    const float f = 0.8f * s.totalMass * c.gravity * za::min(w, 1.f) * low * onFeet;
    if(f > 1.f)
    {
        pushOn(p, 0, glm::vec3{0.f, 0.f, 0.4f * f});
        pushOn(p, chest, glm::vec3{0.f, 0.f, 0.6f * f});
        g.uprightForce = f;
    }
    const glm::vec3 axis = glm::cross(cu, az);
    const float sn = glm::length(axis);
    if(sn < 1e-3f)
    {
        return;
    }
    const glm::vec3 k = axis / sn;
    const float off = za::atan2(sn, glm::dot(cu, az));
    const float rate = za::clamp(off * 3.f, 0.f, 2.5f) * za::min(w, 1.f);
    const float most = 0.15f * s.totalMass * c.gravity * 0.3f; // N m
    for(const int part : {0, chest})
    {
        const float m = b3Body_GetMass(p.body[part]);
        const float spin = glm::dot(glmv(b3Body_GetAngularVelocity(p.body[part])), k);
        const float tq = za::clamp(m * 0.0144f * (rate - spin) / 0.15f, -most, most); // (I ~ m (12 cm)^2)
        b3Body_ApplyTorque(p.body[part], b3v(k * tq), true);
    }
}

// The traces' measures (each frame: cheap).
void measure(State& s, const Parts& p, const StruggleInput& in, uint32_t heldLimbsBits, float dt)
{
    Struggle& g = s.struggle;
    const int chest = chestPart(p), head = headPart(p);
    g.handGap = 1e9f;
    g.handsToHead = 1e9f;
    g.handsOverHead = -1e9f;
    g.handTime += dt;
    const glm::vec3 headAt = head >= 0 ? middleOf(p, head) : glm::vec3{0.f};
    const float headTop = head >= 0 ? za::max(headAt.z, pointOf(p, head, p.rig->bones[head].end).z) : 0.f;
    for(int arm = 0; arm < 2; arm++)
    {
        if(!limbOk(s, p, arm) || (heldLimbsBits & (1u << arm)) != 0u)
        {
            continue;
        }
        const glm::vec3 tip = limbTip(s, p, arm);
        {
            // (Its flailing's measure: the hand's path in its chest's frame, and how far it is from its goal.)
            const glm::vec3 local = glm::inverse(rotOf(p, chest)) * (tip - middleOf(p, chest)) * p.m2u;
            const auto i = static_cast<za::SizeT>(arm);
            g.handPath += g.handWasSet[i] ? glm::distance(local, g.handWas[i]) : 0.f;
            g.handWas[i] = local;
            g.handWasSet[i] = true;
            const Reach& r = s.reach[i];
            g.goalMiss += r.weight > 0.f ? glm::distance(r.goal, tip) * p.m2u * dt : 0.f;
        }
        if(head >= 0)
        {
            g.handsToHead = za::min(g.handsToHead, glm::distance(tip, headAt) * p.m2u);
            g.handsOverHead = za::max(g.handsOverHead, (tip.z - headTop) * p.m2u);
        }
        float d = glm::distance(tip, in.eye);
        for(int i = 0; i < in.holds; i++)
        {
            d = za::min(d, glm::distance(tip, in.hold[static_cast<za::SizeT>(i)].at));
        }
        g.handGap = za::min(g.handGap, d * p.m2u);
    }
    const glm::quat qp = rotOf(p, 0), qc = rotOf(p, chest);
    const glm::vec3 up = qp * az, cu = qc * az;
    g.chestUp = cu.z;
    const float ang = za::atan2(glm::length(glm::cross(up, cu)), glm::dot(up, cu));
    g.bend = glm::degrees(ang) * (glm::dot(cu, qp * ax) >= 0.f ? 1.f : -1.f);
    float straight = 0.f;
    int n = 0;
    for(int leg = 2; leg < 4; leg++)
    {
        if(limbOk(s, p, leg))
        {
            straight += glm::distance(limbTip(s, p, leg), limbRoot(s, p, leg)) / za::max(limbLen(s, p, leg), 1e-3f);
            n++;
        }
    }
    g.straight = n > 0 ? straight / static_cast<float>(n) : 0.f;
}

void stop(State& s, const Context& c, float t, const char* why)
{
    Struggle& g = s.struggle;
    g.on = false;
    g.endedAt = c.now;
    // A lifted one still flailing (living, the hold kept: vr_struggle turned off) flails on at the flail's strengths;
    // else its strengths let go from what they are.
    const bool flails = s.swim.liquid == Liquid::Air && s.swim.living && c.struggle && c.struggle->health > 0.f &&
                        c.struggle->holds > 0;
    const float flail[channelCount] = {1.f, 0.8f, 1.f, 0.8f}; // (Legs, Torso, Arms, Head: startSwim's)
    for(int ch = 0; ch < channelCount; ch++)
    {
        Fade& f = s.fade[static_cast<za::SizeT>(ch)];
        f = flails ? Fade{flail[ch], flail[ch], t, t} : Fade{fadeAt(f, t), 0.f, t, t + fadeOut};
    }
    for(Reach& r : s.reach)
    {
        r.weight = 0.f;
    }
    if(vr_struggle_debug.value || vr_debug_ragdoll.value)
    {
        Con_Printf("struggle: %d ends after %.2f s (%s): effort %.2f, fatigue %.2f, the hold's lag %.1f units at most\n",
            c.num, c.now - g.from, why, g.effort, g.fatigue, g.gripErrorMost);
    }
}

} // namespace

const char* behaviourName(int b)
{
    return b >= 0 && b < behaviourCount ? behaviourNames[b] : "?";
}

uint32_t struggleSlack(const ragdoll::Rig& rig, int count, uint32_t held, uint32_t& partly)
{
    uint32_t out = 0u;
    partly = 0u;
    for(int b = 0; b < count; b++)
    {
        if((held & (1u << b)) == 0u)
        {
            continue;
        }
        if(b == 0 || channelOf(rig.bones[b]) == Channel::Torso)
        {
            out |= 1u << b; // (a torso part alone: its arms and head still fight)
            continue;
        }
        for(int c = 0; c < count; c++)
        {
            out |= onBone(rig, c, b) ? (1u << c) : 0u;
        }
        for(int x = rig.bones[b].parent; x > 0 && channelOf(rig.bones[x]) != Channel::Torso; x = rig.bones[x].parent)
        {
            partly |= 1u << x;
        }
    }
    partly &= ~out;
    return out;
}

void beginStruggle(State& s, const Parts& p, const Context& c)
{
    if(!p.rig)
    {
        return;
    }
    Struggle& g = s.struggle;
    // (A struggle again on the same drive: its fatigue carried on, recovered meanwhile.)
    const float tireTime = za::max(vr_struggle_fatigue.value, 1.f);
    const float tired = g.endedAt >= 0.0 ? g.fatigue * za::exp(-static_cast<float>(c.now - g.endedAt) / tireTime) : 0.f;
    g = Struggle{};
    g.on = true;
    g.from = c.now;
    g.fatigue = tired;
    g.seed = mixBits(static_cast<uint32_t>(c.num) * 2654435761u ^ static_cast<uint32_t>(s.begun * 72.0),
        static_cast<uint32_t>(za::max(vr_ragdoll_active_seed.value, 0.f)) * 40503u + 29u);
    const float var = za::clamp(vr_struggle_variation.value, 0.f, 1.f);
    g.rate = 1.f + var * (roll(g.seed, 1u) - 0.5f) * 0.7f;
    for(int limb = 0; limb < limbCount; limb++)
    {
        g.phase[static_cast<za::SizeT>(limb)] = roll(g.seed, 2u + static_cast<uint32_t>(limb)) * twoPi * var;
    }
    g.phase[1] = g.phase[0] + pi * (1.f + var * (roll(g.seed, 6u) - 0.5f) * 0.5f); // (the arms alternate)
    g.phase[3] = g.phase[2] + pi;
    g.favourite = roll(g.seed, 7u) < 0.5f * var + (1.f - var) ? 1 : 0; // (the right arm, unless its roll says the left)
    g.nextBurst = c.now + 0.5 + 1.5 * static_cast<double>(roll(g.seed, 8u));
    g.nextVoice = c.now + 0.15 + 0.35 * static_cast<double>(roll(g.seed, 9u)); // (a cry as it is taken)
    // Whatever led it gives way: a fall (landed), the clip (its joints hold their own turns), its muscles' lift.
    s.fall.on = false;
    s.clip.first = -1;
    s.support = Fade{};
    for(Reach& r : s.reach)
    {
        r.weight = 0.f;
    }
    if(vr_struggle_debug.value || vr_debug_ragdoll.value)
    {
        Con_Printf("struggle: %d begins (%d hand(s)%s), its rate x%.2f, favours its %s arm%s\n", c.num,
            c.struggle ? c.struggle->holds : 0, s.swim.liquid == Liquid::Air ? ", lifted: in place of the flail" : "", g.rate,
            g.favourite == 0 ? "left" : "right", tired > 0.f ? va(", tired %.2f", tired) : "");
    }
}

bool struggling(const State& s)
{
    return s.on && s.struggle.on;
}

bool struggleFrame(State& s, const Parts& p, const Context& c, float t)
{
    Struggle& g = s.struggle;
    const StruggleInput* in = c.struggle;
    if(!p.rig || !in || !in->want)
    {
        stop(s, c, t, !in ? "not held" : in->health <= 0.f ? "dead" : in->holds == 0 ? "let go of" : "no longer wanted");
        return false;
    }
    const double t0 = Sys_DoubleTime();
    sense(s, p, c);
    holds(s, p, *in, c.dt);
    g.arms = za::clamp(in->arms, 1.f, 2.5f);
    effort(s, *in, c);
    weights(s, c);
    // Its limbs' rhythm: each limb's phase on at its behaviours' blended rate, quicker in a fit.
    const float fit = c.now >= g.burstFrom && c.now < g.burstUntil ? 0.35f * za::clamp(vr_struggle_bursts.value, 0.f, 1.f) : 0.f;
    const float speed = za::clamp(vr_struggle_speed.value, 0.2f, 3.f) * in->speed * g.rate * (1.f + fit) *
                        (0.6f + 0.4f * za::min(g.effort, 1.f));
    for(int limb = 0; limb < limbCount; limb++)
    {
        float hz = 0.f, ws = 0.f;
        for(int b = 0; b < behaviourCount; b++)
        {
            const bool fits = limb < 2 ? behaviourData[b].arms > 0.f : behaviourData[b].legs > 0.f;
            const float w = fits ? g.weight[static_cast<za::SizeT>(b)] : 0.f;
            hz += behaviourData[b].hz * w;
            ws += w;
        }
        float& ph = g.phase[static_cast<za::SizeT>(limb)];
        ph += twoPi * (ws > 1e-3f ? hz / ws : 1.f) * speed * c.dt;
        if(ph > 4096.f * twoPi)
        {
            ph -= 4096.f * twoPi; // (a whole number of cycles: the swings' rolls repeat after 4096)
        }
    }
    // The muscles: each channel's by its behaviours' shares, times the effort, the hold's guard and the setting.
    float k[channelCount]{};
    for(int b = 0; b < behaviourCount; b++)
    {
        const float w = g.weight[static_cast<za::SizeT>(b)];
        k[static_cast<int>(Channel::Arms)] += behaviourData[b].arms * w;
        k[static_cast<int>(Channel::Legs)] += behaviourData[b].legs * w;
        k[static_cast<int>(Channel::Torso)] += behaviourData[b].torso * w;
        k[static_cast<int>(Channel::Head)] += behaviourData[b].head * w;
    }
    // (Slapping at the hands on its head or legs: its arms harder, and kept at half against the guard: an arm's slap
    // barely loads the hold.)
    const float frantic = za::clamp(vr_struggle_frantic.value, 0.f, 2.f) * za::max(g.heldHead, g.heldLegs);
    k[static_cast<int>(Channel::Arms)] +=
        0.5f * frantic * (weightOf(g, Behaviour::Pry) * g.heldHead + weightOf(g, Behaviour::Crunch));
    const float scale = g.effort * za::clamp(vr_struggle_strength.value, 0.f, 2.f);
    const float most[channelCount] = {1.4f, 1.2f, 1.4f, 0.9f}; // (Legs, Torso, Arms, Head)
    for(int ch = 0; ch < channelCount; ch++)
    {
        // (Its legs keep half against the guard: bracing and kicking on the floor resist a drag by friction, the drag's
        // own lag is no reason to give that up; the hold's let-go stays the hold's.)
        const bool keeps = ch == static_cast<int>(Channel::Legs) || (ch == static_cast<int>(Channel::Arms) && frantic > 0.01f);
        const float guard = keeps ? za::max(g.guard, 0.5f) : g.guard;
        const float v = za::clamp(k[ch], 0.f, most[ch]) * scale * guard;
        s.fade[static_cast<za::SizeT>(ch)] = Fade{v, v, t, t};
    }
    g.armsNow = s.fade[static_cast<za::SizeT>(Channel::Arms)].from;
    g.legsNow = s.fade[static_cast<za::SizeT>(Channel::Legs)].from;
    // The limbs its hands hold (slack: no goal of their own).
    uint32_t heldLimbsBits = 0u;
    for(int i = 0; i < in->holds; i++)
    {
        const int part = in->hold[static_cast<za::SizeT>(i)].part;
        const int limb = part >= 0 && part < p.count ? limbOfPart(s, p, part) : -1;
        heldLimbsBits |= limb >= 0 ? (1u << limb) : 0u;
    }
    armGoals(s, p, *in, g.armsNow, heldLimbsBits);
    legGoals(s, p, g.legsNow, heldLimbsBits);
    bodyTargets(s, p, *in, heldLimbsBits, static_cast<float>(c.now - g.from));
    rollOver(s, p, c);
    crunchPull(s, p, c, heldLimbsBits);
    stayUp(s, p, c);
    measure(s, p, *in, heldLimbsBits, c.dt);
    if(vr_struggle_debug.value >= 2.f && g.frame % traceEvery == 1)
    {
        Con_Printf("struggle: %d %.2f s w %.2f %.2f %.2f %.2f %.2f %.2f %.2f effort %.2f fatigue %.2f held t%.0f h%.0f "
                   "l%.0f a%.0f contact %.2f drag %.2f prone %.2f inverted %.2f grip %.1f most %.1f guard %.2f gap %.1f "
                   "bend %.0f straight %.2f arms %.2f legs %.2f roll %.0f upright %.2f up %.0f N chest up %.2f hands to head "
                   "%.1f over %.1f\n",
            c.num, c.now - g.from, g.weight[0], g.weight[1], g.weight[2], g.weight[3], g.weight[4], g.weight[5],
            g.weight[6], g.effort, g.fatigue, g.heldTorso, g.heldHead, g.heldLegs, g.heldArms, g.contact, g.drag, g.prone,
            g.inverted, g.gripError, g.gripErrorMost, g.guard, g.handGap, g.bend, g.straight, g.armsNow, g.legsNow,
            g.rollTorque, g.upright, g.uprightForce, g.chestUp, g.handsToHead, g.handsOverHead);
    }
    struggleCost.seconds += Sys_DoubleTime() - t0;
    struggleCost.count++;
    if(struggleCost.lastFrame != c.now)
    {
        struggleCost.lastFrame = c.now;
        struggleCost.frames++;
    }
    return true;
}

void struggleCostSince(double& ms, int& frames, int& count)
{
    ms = struggleCost.seconds * 1000.0;
    frames = struggleCost.frames;
    count = struggleCost.count;
    struggleCost = StruggleCost{};
}

bool shoulderReach(State& s, const Parts& p, float dt)
{
    if(!p.rig)
    {
        return false;
    }
    constexpr float narrowRate = 1.5f; // rad/s: the cone eased back after a struggle
    constexpr float narrowMost = 3.f;  // s: then back at once as the drive ends (an arm left lying over its head)
    const float wide = glm::radians(za::clamp(vr_struggle_arm_reach.value, 0.f, 175.f));
    const bool want = s.struggle.on && wide > 0.f;
    bool any = false;
    for(int arm = 0; arm < 2; arm++)
    {
        float& rigCone = s.shoulderCone[static_cast<za::SizeT>(arm)];
        const int u = upperOf(s, arm);
        const b3JointId j = u >= 0 ? s.joint[static_cast<za::SizeT>(u)] : b3_nullJointId;
        if(u < 0 || p.rig->bones[u].joint != ragdoll::Joint::Ball || B3_IS_NULL(j) || !b3Joint_IsValid(j))
        {
            rigCone = -1.f;
            continue;
        }
        const float now = b3SphericalJoint_GetConeLimit(j);
        float to = now;
        if(want)
        {
            rigCone = rigCone < 0.f ? now : rigCone;
            to = za::max(rigCone, wide);
        }
        else if(rigCone >= 0.f)
        {
            // (Eased back, never tighter than the arm is turned now: no snap.)
            const float turned = b3SphericalJoint_GetConeAngle(j);
            to = za::max(rigCone, za::min(now - narrowRate * dt, za::max(turned + 0.03f, rigCone)));
            if(to <= rigCone + 1e-3f)
            {
                to = rigCone;
                rigCone = -1.f;
            }
        }
        if(to != now)
        {
            b3SphericalJoint_SetConeLimit(j, to);
            s.lower[static_cast<za::SizeT>(u)] = to;
        }
        any = any || rigCone >= 0.f;
    }
    s.narrowing = want || !any ? 0.f : s.narrowing + dt;
    return any && s.narrowing < narrowMost;
}

void shoulderRestore(State& s)
{
    for(int arm = 0; arm < 2; arm++)
    {
        float& rigCone = s.shoulderCone[static_cast<za::SizeT>(arm)];
        const int u = upperOf(s, arm);
        if(rigCone < 0.f || u < 0)
        {
            rigCone = -1.f;
            continue;
        }
        const b3JointId j = s.joint[static_cast<za::SizeT>(u)];
        if(B3_IS_NON_NULL(j) && b3Joint_IsValid(j))
        {
            b3SphericalJoint_SetConeLimit(j, rigCone);
            s.lower[static_cast<za::SizeT>(u)] = rigCone;
        }
        rigCone = -1.f;
    }
    s.narrowing = 0.f;
}

} // namespace qvr::ragactive
