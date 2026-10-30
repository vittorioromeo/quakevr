// vr_ragactive_swim.cpp -- active ragdolls in liquids (vr_ragactive.hpp, Swim): a swim begun, its frame (the dog
// paddle, the kick, drowning), a living one's swim and lava or slime's flail. Moved out of vr_ragactive.cpp; what
// they share is vr_ragactive_internal.hpp.

#include "vr_ragactive_internal.hpp"

namespace qvr::ragactive
{

// ---- Liquids ----
// (vr_ragactive.hpp, Swim.) Its pelvis, chest and head looked at each frame: a swim begins once its pelvis or chest is
// in water with water still deepEnough over it (not lying in a puddle), or in lava or slime at all, within liquidWindow
// of its death; it stops half a second after all three are out of every liquid. In water: a dog paddle (the hands
// circling ahead of the chest, pulling down and back), a bicycle kick, the torso straight, the head tipped back and held
// up (a lift while it is under) for its first part, all slowing and weakening as it drowns, then limp (it floats face
// down, or sinks, as before). In lava or slime: each hand and foot thrown to a new random place every 0.13-0.18 s, the
// torso arching and twisting, the head thrashing, all strong, fading fast; the burning is the QC's (vr_burning.qc).

namespace
{

constexpr float deepEnough = 10.f;    // units of water over the pelvis or chest for a swim to begin
constexpr float headLiftShare = 0.3f; // body weights: the head (and chest) held up at most, while the head is under
constexpr float swimThrustDying = 0.05f;  // body weights: a dying paddle's strokes' push at most (at full vigour)
constexpr float swimThrustLiving = 0.2f;  // ... a living knockdown's, towards where it can stand
constexpr float swimArrived = 0.25f;      // m: that near its goal, it only treads water
constexpr float panicTime = 3.f;          // s: a drowning one's frantic strokes ease to its usual over this
constexpr float panicRate = 0.9f;         // ... its strokes' rate times 1 + this at first,
constexpr float panicThrust = 0.5f;       // ... and their push 1 + this

} // namespace

namespace detail
{

void turnWhole(const Parts& p, const Context& c, const glm::quat& want, float rate, float most, float k); // ("Staggers")
[[nodiscard]] glm::vec3 level(const glm::vec3& v); // (below)

} // namespace detail

namespace
{

[[nodiscard]] glm::vec3 centre(const Parts& p, int b)
{
    return glmv(b3Body_GetWorldCenter(p.body[b]));
}

} // namespace

namespace detail
{

[[nodiscard]] SwimParts swimParts(const Parts& p)
{
    SwimParts out;
    for(int b = 1; b < p.count; b++)
    {
        const ragdoll::Bone& bone = p.rig->bones[b];
        if(cut(p, b) || bone.joint == ragdoll::Joint::Loose)
        {
            continue;
        }
        if(bone.role == modelmeta::BoneRole::Chest && out.chest < 0)
        {
            out.chest = b;
        }
        if(b == p.rig->head || (out.head < 0 && bone.role == modelmeta::BoneRole::Head))
        {
            out.head = b;
        }
    }
    return out;
}

[[nodiscard]] bool liquidsOn()
{
    // (vr_ragdoll_active 0 too: a released body lying in water must not be woken again by a pending swim)
    return vr_ragdoll_active.value != 0.f && vr_ragdoll_active_liquids.value != 0.f;
}

// The liquid it is to swim in now (None: not deep enough in any).
[[nodiscard]] Liquid enteredLiquid(const Parts& p, const Context& c, const SwimParts& sp)
{
    const glm::vec3 above{0.f, 0.f, deepEnough / p.m2u};
    for(const int b : {0, sp.chest})
    {
        if(b < 0 || cut(p, b))
        {
            continue;
        }
        const glm::vec3 at = centre(p, b);
        const Liquid l = c.liquidAt(at);
        if(l == Liquid::Lava || l == Liquid::Slime || (l == Liquid::Water && c.liquidAt(at + above) == Liquid::Water))
        {
            return l;
        }
    }
    return Liquid::None;
}

} // namespace detail

namespace
{

[[nodiscard]] const char* liquidName(Liquid l)
{
    return l == Liquid::Water ? "water" : l == Liquid::Slime ? "slime" : l == Liquid::Lava ? "lava" :
           l == Liquid::Air   ? "the air" : "none";
}

} // namespace

namespace detail
{

void startSwim(State& s, const Parts& p, const Context& c, Liquid l, float t)
{
    Swim& w = s.swim;
    w.liquid = l;
    w.from = c.now;
    w.outSince = -1.0;
    w.seed = hash(static_cast<uint32_t>(c.num) * 2654435761u ^ static_cast<uint32_t>(s.begun * 72.0),
        static_cast<uint32_t>(za::max(vr_ragdoll_active_seed.value, 0.f)) * 40503u + 17u);
    w.phase = roll(w.seed, 1u) * twoPi;
    const bool water = l == Liquid::Water;
    const float most = water ? vr_ragdoll_active_paddle.value : vr_ragdoll_active_flail.value * (l == Liquid::Slime ? 1.4f : 1.f);
    const float T = za::clamp(most, 0.2f, 30.f) * (0.8f + 0.4f * roll(w.seed, 2u));
    w.length = T;
    // His death's own pieces given up: the pin, the step, the hand on the wound, the crawl; no clip (the strokes lead).
    Death& d = s.death;
    d.stepLeg = -1;
    d.woundArm = -1;
    d.crawl = false;
    d.stumble = false;
    for(Reach& r : s.reach)
    {
        r = Reach{};
    }
    dropPin(s.pin);
    s.clip.first = -1;
    // (A living one keeps its strength: it fades once it dies, swimDies.)
    const auto set = [&](Channel ch, float from, float a, float b) {
        s.fade[static_cast<za::SizeT>(ch)] = w.living ? Fade{from, from, t, t} : Fade{from, 0.f, t + a * T, t + b * T};
    };
    if(water)
    {
        set(Channel::Arms, 0.9f, 0.35f, 1.f);
        set(Channel::Legs, 0.7f, 0.3f, 0.9f);
        set(Channel::Torso, 0.45f, 0.25f, 0.8f);
        set(Channel::Head, 0.8f, 0.35f, 0.75f);
    }
    else
    {
        set(Channel::Arms, 1.f, 0.4f, 1.f);
        set(Channel::Legs, 1.f, 0.3f, 1.f);
        set(Channel::Torso, 0.8f, 0.3f, 0.9f);
        set(Channel::Head, 0.8f, 0.3f, 0.9f);
    }
    s.on = true;
    if(vr_debug_ragdoll.value || (w.living && vr_knockdown_debug.value))
    {
        if(w.living)
        {
            Con_Printf("ragdoll: %d down alive in %s: %s while it lives\n", c.num, liquidName(l), water ? "paddles" : "flails");
        }
        else
        {
            Con_Printf("ragdoll: %d in %s %.2f s after its death: %s for %.1f s\n", c.num, liquidName(l), t,
                water ? "paddles" : "flails", T);
        }
    }
}

} // namespace detail

namespace
{

// Out of the liquid: its strengths fade from what they are over 0.4 s.
void stopSwim(State& s, const Context& c, float t)
{
    for(Fade& f : s.fade)
    {
        f = Fade{fadeAt(f, t), 0.f, t, t + 0.4f};
    }
    for(Reach& r : s.reach)
    {
        r.weight = 0.f;
    }
    if(vr_debug_ragdoll.value)
    {
        Con_Printf("ragdoll: %d out of the %s after %.2f s: stops\n", c.num, liquidName(s.swim.liquid), c.now - s.swim.from);
    }
    s.swim.liquid = Liquid::None;
}

// A flailing limb's k-th place (its frame: x forward, y left, z up), thrown about its own side.
[[nodiscard]] glm::vec3 flailDir(uint32_t seed, int limb, uint32_t k)
{
    const uint32_t h = hash(seed, static_cast<uint32_t>(limb) * 977u + k);
    const glm::vec3 r{roll(h, 1u) * 2.f - 1.f, roll(h, 2u) * 2.f - 1.f, roll(h, 3u) * 2.f - 1.f};
    const float side = (limb % 2) == 0 ? 1.f : -1.f;
    const glm::vec3 bias = limb < 2 ? glm::vec3{0.3f, 0.6f * side, 0.5f} : glm::vec3{0.3f, 0.3f * side, -0.9f};
    const glm::vec3 v = r + bias;
    return glm::length(v) > 1e-3f ? glm::normalize(v) : glm::vec3{0.f, 0.f, -1.f};
}

} // namespace

namespace detail
{

// Each frame (after deathFrame): begins, stops, or sets the strokes' targets and reaches over the clip's.
void swimFrame(State& s, const Parts& p, const Context& c, float t)
{
    Swim& w = s.swim;
    if(w.liquid == Liquid::None)
    {
        if(w.from >= 0.0 || !c.liquidAt || !liquidsOn() || !s.death.on || s.death.collapse || t > liquidWindow)
        {
            return;
        }
        const Liquid l = enteredLiquid(p, c, swimParts(p));
        if(l == Liquid::None)
        {
            return;
        }
        startSwim(s, p, c, l, t);
    }
    const SwimParts sp = swimParts(p);
    const bool air = w.liquid == Liquid::Air; // (a lifted one's flail: in it until endFlail)
    bool in = air;
    bool headIn = false;
    if(!air && c.liquidAt && liquidsOn())
    {
        for(const int b : {0, sp.chest, sp.head})
        {
            const bool here = b >= 0 && c.liquidAt(centre(p, b)) != Liquid::None;
            in = in || here;
            headIn = headIn || (here && b == sp.head);
        }
    }
    if(!in)
    {
        if(w.outSince < 0.0)
        {
            w.outSince = c.now;
        }
        if(c.now - w.outSince > 0.5 || !c.liquidAt || !liquidsOn())
        {
            stopSwim(s, c, t);
            return;
        }
    }
    else
    {
        w.outSince = -1.0;
    }
    const bool water = w.liquid == Liquid::Water;
    const float T = w.length;
    const float ts = static_cast<float>(c.now - w.from);
    // Its vigour (1 .. 0): the arms' strength over what it began with. Slower strokes as it weakens.
    const Fade& fa = s.fade[static_cast<za::SizeT>(Channel::Arms)];
    const float v = za::clamp(fadeAt(fa, t) / za::max(fa.from, 1e-3f), 0.f, 1.f);
    // A drowning one's struggle: frantic strokes at first (quicker, harder), easing to its usual.
    if(w.panic > 0.f && w.panicFrom < 0.0)
    {
        w.panicFrom = c.now;
    }
    const float frantic = w.panic > 0.f ? w.panic * (1.f - smooth01(static_cast<float>(c.now - w.panicFrom) / panicTime)) : 0.f;
    const float hz = (water ? 0.6f + 0.7f * v : 2.2f + 1.6f * v) * (1.f + panicRate * frantic);
    w.phase += twoPi * hz * c.dt;
    // (Wrapped in water only: a flail's hops count its phase up, and lasts at most ~50 s, a few thousand radians.)
    if(water && w.phase > 64.f * twoPi)
    {
        w.phase -= 64.f * twoPi;
    }
    const glm::quat qc = bodyRot(p, sp.chest >= 0 ? sp.chest : 0), qp = bodyRot(p, 0);
    const glm::vec3 x{1.f, 0.f, 0.f}, y{0.f, 1.f, 0.f}, z{0.f, 0.f, 1.f};
    // The torso and head (the limbs' bones beyond their reaches: as he stood). A flail's thrash (its rates, Hz at full
    // vigour) slows with its strokes: its phase over the full rate's (3.8 Hz), radians per Hz.
    const float fp = w.phase / 3.8f;
    // (A living one's as at its start: its head tipped back and held up while it lives.)
    const float tl = w.living ? 0.f : ts;
    const float head = water ? -0.6f * (1.f - smooth01((tl - 0.3f * T) / (0.4f * T))) : 0.f;
    for(int b = 1; b < p.count; b++)
    {
        if(!drivable(s, p, b))
        {
            continue;
        }
        glm::quat q{1.f, 0.f, 0.f, 0.f};
        const Channel ch = channelOf(p.rig->bones[b]);
        if(ch == Channel::Head)
        {
            q = water ? glm::angleAxis(head, y)
                      : glm::angleAxis(0.5f * v * za::sin(fp * 2.7f), z) *
                            glm::angleAxis(-0.25f * v * (1.f + za::sin(fp * 1.9f)), y);
        }
        else if(ch == Channel::Torso && !water)
        {
            q = glm::angleAxis(0.35f * v * za::sin(fp * 2.f), y) * glm::angleAxis(0.3f * v * za::sin(fp * 1.6f + 1.3f), x) *
                glm::angleAxis(0.25f * v * za::sin(fp * 1.3f + 2.1f), z);
        }
        s.target[static_cast<za::SizeT>(b)] = glm::normalize(q);
    }
    // The limbs: a reach each (no strength of its own: their channels' fades lead).
    const float a = 0.4f + 0.6f * v;
    for(int limb = 0; limb < limbCount; limb++)
    {
        Reach& r = s.reach[static_cast<za::SizeT>(limb)];
        if(!limbWhole(s, p, limb))
        {
            r.weight = 0.f;
            continue;
        }
        const int u = s.chain[static_cast<za::SizeT>(limb)][0];
        const glm::vec3 root = worldPoint(p, u, p.rig->bones[u].pivot);
        const float len = limbLength(s, p, limb);
        const float side = (limb % 2) == 0 ? 1.f : -1.f;
        const bool arm = limb < 2;
        const glm::quat& q = arm ? qc : qp;
        const glm::vec3 F = q * x, L = q * y, U = q * z;
        glm::vec3 goal;
        if(water && arm)
        {
            const float ph = w.phase + (limb == 1 ? 3.14159265f : 0.f);
            goal = root + F * (len * (0.5f + 0.3f * a * za::cos(ph))) + U * (len * (-0.15f - 0.3f * a * za::sin(ph))) +
                   L * (side * 0.15f * len);
        }
        else if(water)
        {
            const float ph = 1.25f * w.phase + (limb == 3 ? 3.14159265f : 0.f);
            goal = root - U * (len * (0.85f - 0.25f * a * (0.5f + 0.5f * za::cos(ph)))) + F * (len * 0.25f * a * za::sin(ph)) +
                   L * (side * 0.08f * len);
        }
        else
        {
            // (A new place every 0.13-0.18 s at full vigour (3.8 Hz), slower as the stroke rate falls to 2.2 Hz.)
            const float hop = 0.13f + 0.05f * roll(w.seed, 10u + static_cast<uint32_t>(limb));
            const float f = w.phase / (twoPi * 3.8f * hop);
            const auto k = static_cast<uint32_t>(f);
            const glm::vec3 d0 = flailDir(w.seed, limb, k), d1 = flailDir(w.seed, limb, k + 1u);
            const glm::vec3 d = glm::normalize(glm::mix(d0, d1, smooth01(f - static_cast<float>(k))) + glm::vec3{1e-4f});
            goal = root + (F * d.x + L * d.y + U * d.z) * (0.85f * len);
        }
        r.goal = goal;
        r.weight = 1.f;
        r.strength = 0.f;
    }
    // The head held up while it is under, for the first part of the paddle (half on the head, half on the chest).
    w.headLift = 0.f;
    if(water && headIn && sp.head >= 0)
    {
        const float keep = 1.f - smooth01((tl - 0.4f * T) / (0.3f * T));
        w.headLift = headLiftShare * s.totalMass * c.gravity * keep * s.base;
        if(w.headLift > 0.f)
        {
            const glm::vec3 f{0.f, 0.f, 0.5f * w.headLift};
            pushPart(p, sp.head, f);
            pushPart(p, sp.chest >= 0 ? sp.chest : 0, f);
        }
    }
    // The strokes' push (water): ahead (its chest's way), or a living one's towards where it can stand, turning to face
    // it; strongest as the hands pull back, weakening with its vigour. Half on the pelvis, half on the chest.
    w.thrust = 0.f;
    if(water && in)
    {
        const glm::vec3 at = centre(p, sp.chest >= 0 ? sp.chest : 0);
        glm::vec3 way = w.living ? (w.hasGoal ? level(w.goal - at) : glm::vec3{0.f}) : level(qc * x);
        const float wl = glm::length(way);
        if(wl > (w.living ? swimArrived : 0.3f))
        {
            way /= wl;
            const float stroke = 0.5f + 0.5f * za::sin(w.phase);
            w.thrust = (w.living ? swimThrustLiving : swimThrustDying) * s.totalMass * c.gravity * v * stroke * s.base *
                       (1.f + panicThrust * frantic);
            pushPart(p, 0, way * (0.5f * w.thrust));
            pushPart(p, sp.chest >= 0 ? sp.chest : 0, way * (0.5f * w.thrust));
            const glm::vec3 ahead = level(qc * x);
            if(w.living && glm::length(ahead) > 0.3f)
            {
                const glm::vec3 a = glm::normalize(ahead);
                const float off = za::atan2(glm::cross(a, way).z, glm::dot(a, way));
                turnWhole(p, c, glm::normalize(glm::angleAxis(off, z) * bodyRot(p, 0)), 1.5f, 1.f, 0.5f);
            }
        }
    }
    if(c.trace)
    {
        // (Its limbs' motion: each hand's or foot's part's speed against the part its limb hangs from, m/s, averaged.)
        float limbs = 0.f;
        int n = 0;
        for(int limb = 0; limb < limbCount; limb++)
        {
            if(limbWhole(s, p, limb))
            {
                const int u = s.chain[static_cast<za::SizeT>(limb)][0], lo = s.chain[static_cast<za::SizeT>(limb)][1];
                const int base = p.rig->bones[u].parent;
                limbs += glm::length(glmv(b3Body_GetLinearVelocity(p.body[lo])) - glmv(b3Body_GetLinearVelocity(p.body[base])));
                n++;
            }
        }
        const float hz2 = sp.head >= 0 ? centre(p, sp.head).z * p.m2u : 0.f;
        Con_Printf("ragswim %d %.3f l %d v %.2f head %d %.1f lift %.0f limbs %.2f thrust %.0f at %.0f %.0f\n", c.num,
            static_cast<float>(c.now - c.born),
            static_cast<int>(w.liquid), v, headIn ? 1 : 0, hz2, w.headLift, limbs / static_cast<float>(za::max(n, 1)), w.thrust,
            centre(p, 0).x * p.m2u, centre(p, 0).y * p.m2u);
    }
}

// Its middle (centre of mass of its parts but loose ones: his gun), world metres; and its motion (m/s) into `vel`.
[[nodiscard]] glm::vec3 massCentre(const Parts& p, glm::vec3* vel)
{
    glm::vec3 sum{0.f}, v{0.f};
    float mass = 0.f;
    for(int b = 0; b < p.count; b++)
    {
        if(cut(p, b) || p.rig->bones[b].joint == ragdoll::Joint::Loose)
        {
            continue;
        }
        const float m = b3Body_GetMass(p.body[b]);
        sum += glmv(b3Body_GetWorldCenter(p.body[b])) * m;
        v += glmv(b3Body_GetLinearVelocity(p.body[b])) * m;
        mass += m;
    }
    if(vel)
    {
        *vel = mass > 0.f ? v / mass : glm::vec3{0.f};
    }
    return mass > 0.f ? sum / mass : glmv(b3Body_GetWorldCenter(p.body[0]));
}

[[nodiscard]] glm::vec3 level(const glm::vec3& v)
{
    return glm::vec3{v.x, v.y, 0.f};
}

// Its whole body turned as one towards its pelvis turned `want` (world): its parts' spin, and their motion about the
// pelvis, eased (share `k` of 1 - e^(-dt / 0.08 s) a frame) towards a turn at `rate` per radian off, at most `most`
// rad/s. (The pin's own turn is the pelvis's alone, mass-normalised, too weak for the body on it; and the floor holds
// its feet: the hand of God keeping it upright, or rising.)
void turnWhole(const Parts& p, const Context& c, const glm::quat& want, float rate, float most, float k)
{
    if(k <= 0.f)
    {
        return;
    }
    glm::vec3 spin = ragpose::rotationVector(want * glm::inverse(bodyRot(p, 0))) * rate;
    if(const float l = glm::length(spin); l > most)
    {
        spin *= most / l;
    }
    const glm::vec3 pivot = glmv(b3Body_GetWorldCenter(p.body[0]));
    const glm::vec3 add = (spin - glmv(b3Body_GetAngularVelocity(p.body[0]))) * ((1.f - za::exp(-c.dt / 0.08f)) * k);
    for(int b = 0; b < p.count; b++)
    {
        if(cut(p, b))
        {
            continue;
        }
        b3Body_SetAngularVelocity(p.body[b], b3v(glmv(b3Body_GetAngularVelocity(p.body[b])) + add));
        b3Body_SetLinearVelocity(p.body[b],
            b3v(glmv(b3Body_GetLinearVelocity(p.body[b])) + glm::cross(add, glmv(b3Body_GetWorldCenter(p.body[b])) - pivot)));
    }
}

// Its whole body held where it is on the floor (level): its middle's level motion eased (share `k` of
// 1 - e^(-dt / 0.05 s) a frame) towards going back to level `anchor` over 0.15 s (at most 1 m/s), every part's motion
// changed alike (its turn and pose untouched). A get-up's roll over and pull-in (vr_ragdoll_active_getup_hold): the roll
// is about the edge it rolls over, and the floor's grip makes it a wheel: the body rolled away (a knight 50 units, an
// arm flung out the edge), then the pin dragged it back.
void holdLevel(const Parts& p, const Context& c, const glm::vec3& anchor, float k)
{
    if(k <= 0.f)
    {
        return;
    }
    glm::vec3 vel;
    const glm::vec3 at = massCentre(p, &vel);
    glm::vec3 want = level(anchor - at) / 0.15f;
    if(const float l = glm::length(want); l > 1.f)
    {
        want *= 1.f / l;
    }
    const glm::vec3 add = (want - level(vel)) * ((1.f - za::exp(-c.dt / 0.05f)) * za::min(k, 1.f));
    for(int b = 0; b < p.count; b++)
    {
        if(!cut(p, b))
        {
            b3Body_SetLinearVelocity(p.body[b], b3v(glmv(b3Body_GetLinearVelocity(p.body[b])) + add));
        }
    }
}

} // namespace detail

bool beginLivingSwim(State& s, const Parts& p, const Context& c)
{
    s.struggle.on = false; // (it leads from here: a struggle gives way)
    if(!p.rig || !c.liquidAt || !liquidsOn() || s.swim.liquid != Liquid::None || s.rise.on || s.stagger.on)
    {
        return false;
    }
    const Liquid l = enteredLiquid(p, c, swimParts(p));
    if(l == Liquid::None)
    {
        return false;
    }
    // (A fall into it ends: the strokes lead from here.)
    s.fall.on = false;
    s.swim = Swim{};
    s.swim.living = true;
    s.support = Fade{};
    startSwim(s, p, c, l, static_cast<float>(c.now - s.begun));
    return true;
}

void swimGoal(State& s, bool hasGoal, const glm::vec3& goal)
{
    s.swim.hasGoal = hasGoal;
    s.swim.goal = goal;
}

void swimDies(State& s, double now)
{
    Swim& w = s.swim;
    if(!w.living || w.liquid == Liquid::None)
    {
        return;
    }
    // Its strokes weaken from now as a death's (startSwim's shares of its length).
    w.living = false;
    w.hasGoal = false;
    const float t = static_cast<float>(now - s.begun);
    const bool water = w.liquid == Liquid::Water;
    const float T = w.length;
    const auto set = [&](Channel ch, float a, float b) {
        Fade& f = s.fade[static_cast<za::SizeT>(ch)];
        f = Fade{f.from, 0.f, t + a * T, t + b * T};
    };
    set(Channel::Arms, water ? 0.35f : 0.4f, 1.f);
    set(Channel::Legs, 0.3f, water ? 0.9f : 1.f);
    set(Channel::Torso, water ? 0.25f : 0.3f, water ? 0.8f : 0.9f);
    set(Channel::Head, water ? 0.35f : 0.3f, water ? 0.75f : 0.9f);
    w.from = now; // (its head's lift and tip from here)
}

bool swimming(const State& s)
{
    return s.on && s.swim.liquid != Liquid::None;
}

bool beginFlail(State& s, const Parts& p, const Context& c)
{
    if(!p.rig || s.swim.liquid != Liquid::None || s.rise.on || s.stagger.on)
    {
        return false;
    }
    // (As a living swim begins: a fall ends, the thrash leads from here.)
    s.fall.on = false;
    s.swim = Swim{};
    s.swim.living = true;
    s.support = Fade{};
    startSwim(s, p, c, Liquid::Air, static_cast<float>(c.now - s.begun));
    return true;
}

void endFlail(State& s, const Context& c)
{
    if(s.swim.liquid == Liquid::Air)
    {
        stopSwim(s, c, static_cast<float>(c.now - s.begun));
    }
}

bool flailing(const State& s)
{
    return s.on && s.swim.liquid == Liquid::Air;
}
} // namespace qvr::ragactive
