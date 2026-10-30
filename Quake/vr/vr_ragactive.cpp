// vr_ragactive.cpp -- active ragdolls (vr_ragactive.hpp; ROUND22.md, "Active ragdolls: core and deaths").

#include "vr_ragactive.hpp"
#include "vr_ragactive_internal.hpp"

#include "vr_b3glm.hpp"
#include "vr_cvars.hpp"
#include "vr_geom.hpp"
#include "vr_lines.hpp"
#include "vr_ragpose.hpp"

#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"

#include <cstring>

#include "vr_ledges.hpp"

namespace qvr::ragactive
{

namespace
{

// The drive's cost (vr_ragdoll_active_list, the tests): seconds and frames since the last look.
struct Cost
{
    double seconds{0.0};
    int frames{0};
    int driven{0};
    double lastFrame{-1.0};
};
Cost cost;

constexpr float maxHertz = 20.f;     // Hz: a joint's spring never stiffer (Box3D doesn't clamp joint springs)
constexpr float stepTime = 0.32f;    // s: a stumble's step
constexpr float strokeTime = 1.1f;   // s: a crawl's stroke (one arm reaching, then pulling)
constexpr float reachShare = 0.45f;  // of a stroke: reaching (then pulling)
constexpr float crawlSpeed = 0.7f;   // m/s: a crawl's pull at most
constexpr float rollTime = 1.6f;     // s: rolling onto his front to crawl, at most
constexpr float rollRate = 4.f;       // rad/s: its turn

[[nodiscard]] bool named(const char* name, const char* part)
{
    return std::strstr(name, part) != nullptr;
}

} // namespace

namespace detail
{

// Its side by its name's end: 0 left (_l), 1 right (_r), -1 neither.
[[nodiscard]] int sideOf(const char* name)
{
    const size_t n = std::strlen(name);
    if(n >= 2 && name[n - 2] == '_')
    {
        return name[n - 1] == 'l' ? 0 : name[n - 1] == 'r' ? 1 : -1;
    }
    return -1;
}

[[nodiscard]] uint32_t hash(uint32_t a, uint32_t b)
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
    return static_cast<float>(hash(seed, what) >> 8) / static_cast<float>(1u << 24);
}

[[nodiscard]] bool cut(const Parts& p, int b)
{
    return (p.cut & (1u << b)) != 0u;
}

// A drive's force `f` (N, world) on part `b` this frame: applied, and kept in `p.force` for the step's later pieces.
void pushPart(const Parts& p, int b, const glm::vec3& f)
{
    b3Body_ApplyForceToCenter(p.body[b], b3v(f), true);
    if(p.force)
    {
        p.force[b] += f;
    }
}

// Its chest's part (its pelvis's if none, or cut off): every stage's one helper.
[[nodiscard]] int chestOf(const Parts& p)
{
    for(int b = 0; b < p.count; b++)
    {
        if(p.rig->bones[b].role == modelmeta::BoneRole::Chest && !cut(p, b))
        {
            return b;
        }
    }
    return 0;
}

// Whether bone `b` is `top` or on it.
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

[[nodiscard]] bool drivable(const State& s, const Parts& p, int b)
{
    const ragdoll::Bone& bone = p.rig->bones[b];
    const b3JointId j = s.joint[static_cast<za::SizeT>(b)];
    return bone.parent >= 0 && (bone.joint == ragdoll::Joint::Ball || bone.joint == ragdoll::Joint::Hinge) && !cut(p, b) &&
           !cut(p, bone.parent) && (p.loose & (1u << b)) == 0u && B3_IS_NON_NULL(j) && b3Joint_IsValid(j);
}

[[nodiscard]] glm::quat bodyRot(const Parts& p, int b)
{
    return fromB3(b3Body_GetRotation(p.body[b]));
}

// Bone `b`'s rest-space point `rest` (units) in the world now (metres).
[[nodiscard]] glm::vec3 worldPoint(const Parts& p, int b, const glm::vec3& rest)
{
    return glmv(b3Body_GetWorldPoint(p.body[b], b3v(rest * p.k)));
}

} // namespace detail

namespace
{

// The clip's pose now: each bone's transform (rest -> pose, model space), and each joint's turn in its parent's rest
// space at the two frames it is between (the frame-to-frame turn: the speed fed forward), per second.
struct ClipPose
{
    bool valid{false};
    za::Array<glm::quat, ragdoll::maxBones> rot{};
    za::Array<glm::vec3, ragdoll::maxBones> pos{};
    za::Array<glm::vec3, ragdoll::maxBones> spin{}; // rad/s, the parent's rest space
    float framePos{0.f};
};

// The clip's frame position now (held at `start` until t0; backwards if first > last; looped forwards).
[[nodiscard]] float clipPosition(const Clip& c, double now)
{
    const int lo = za::min(c.first, c.last), hi = za::max(c.first, c.last);
    const float run = static_cast<float>(za::max(now - c.t0, 0.0)) * c.fps;
    float f = c.first > c.last ? c.start - run : c.start + run;
    const float span = static_cast<float>(hi - lo + 1);
    if(c.loop && c.first <= c.last && span > 0.f)
    {
        while(f >= static_cast<float>(hi + 1))
        {
            f -= span;
        }
    }
    return za::clamp(f, static_cast<float>(lo), static_cast<float>(hi));
}

void clipPose(const State& s, const Parts& p, double now, ClipPose& out)
{
    const Clip& c = s.clip;
    const ragdoll::Rig& rig = *p.rig;
    if(c.first < 0 || c.last < 0 || !s.model)
    {
        out.valid = false;
        return;
    }
    const int lo = za::min(c.first, c.last), hi = za::max(c.first, c.last);
    const float f = clipPosition(c, now);
    const int fa = static_cast<int>(f);
    const float frac = f - static_cast<float>(fa);
    const int fb = fa < hi ? fa + 1 : (c.loop ? lo : hi);
    const float way = c.first > c.last ? -1.f : 1.f; // (backwards: the turn from fb to fa)
    const bool moving = now >= c.t0 && c.fps > 0.f && (c.loop || (f > static_cast<float>(lo) && f < static_cast<float>(hi)));
    const int pa = ragdoll::poseOfFrame(s.model, fa), pb = ragdoll::poseOfFrame(s.model, fb);
    za::Array<glm::quat, ragdoll::maxBones> ra{}, rb{};
    for(int b = 0; b < p.count; b++)
    {
        const auto i = static_cast<za::SizeT>(b);
        ragpose::bonePoseLerp(rig, pa, pb, frac, b, out.rot[i], out.pos[i], ra[i], rb[i]);
    }
    for(int b = 0; b < p.count; b++)
    {
        const int parent = rig.bones[b].parent;
        const auto i = static_cast<za::SizeT>(b);
        if(parent < 0 || pa == pb || !moving)
        {
            out.spin[i] = glm::vec3{0.f};
            continue;
        }
        const auto pi = static_cast<za::SizeT>(parent);
        const glm::quat relA = glm::inverse(ra[pi]) * ra[i], relB = glm::inverse(rb[pi]) * rb[i];
        out.spin[i] = ragpose::rotationVector(relB * glm::inverse(relA)) * (c.fps * way);
    }
    out.framePos = f;
    out.valid = true;
}

// The parts a hand holds, each with its whole limb (up to the torso; the pelvis: all of them).
[[nodiscard]] uint32_t heldLimbs(const ragdoll::Rig& rig, int count, uint32_t held)
{
    uint32_t out = 0;
    for(int b = 0; b < count; b++)
    {
        if((held & (1u << b)) == 0u)
        {
            continue;
        }
        int top = b;
        while(top > 0 && rig.bones[top].parent > 0 && rig.bones[rig.bones[top].parent].role != modelmeta::BoneRole::Chest)
        {
            top = rig.bones[top].parent;
        }
        for(int c = 0; c < count; c++)
        {
            out |= onBone(rig, c, top) ? (1u << c) : 0u;
        }
    }
    return out;
}

void setLimp(const State& s, const Parts& p, int b)
{
    const b3JointId j = s.joint[static_cast<za::SizeT>(b)];
    if(p.rig->bones[b].joint == ragdoll::Joint::Ball)
    {
        b3SphericalJoint_EnableSpring(j, s.limpHertz > 0.f);
        b3SphericalJoint_SetSpringHertz(j, s.limpHertz);
        b3SphericalJoint_SetSpringDampingRatio(j, 1.f);
        b3SphericalJoint_SetTargetRotation(j, b3Quat_identity);
        b3SphericalJoint_EnableMotor(j, s.friction > 0.f);
        b3SphericalJoint_SetMotorVelocity(j, b3Vec3_zero);
        b3SphericalJoint_SetMaxMotorTorque(j, s.friction);
    }
    else
    {
        b3RevoluteJoint_EnableSpring(j, s.limpHertz > 0.f);
        b3RevoluteJoint_SetSpringHertz(j, s.limpHertz);
        b3RevoluteJoint_SetSpringDampingRatio(j, 1.f);
        b3RevoluteJoint_SetTargetAngle(j, 0.f);
        b3RevoluteJoint_EnableMotor(j, s.friction > 0.f);
        b3RevoluteJoint_SetMotorSpeed(j, 0.f);
        b3RevoluteJoint_SetMaxMotorTorque(j, s.friction);
    }
}

} // namespace

namespace detail
{

void dropPin(Pin& pin)
{
    if(B3_IS_NON_NULL(pin.anchor) && b3Body_IsValid(pin.anchor))
    {
        b3DestroyBody(pin.anchor); // (its joint goes with it)
    }
    pin.anchor = b3_nullBodyId;
    pin.joint = b3_nullJointId;
    pin.force = 0.f;
}

} // namespace detail

namespace
{

// A limb's reach (two-bone IK) as its bones' targets: each their child's turn in its parent's rest space, from the
// parts as they are now (the upper bone turned the least way from where it is).
void reachTargets(const State& s, const Parts& p, int limb, za::Array<glm::quat, ragdoll::maxBones>& target)
{
    const Reach& r = s.reach[static_cast<za::SizeT>(limb)];
    const int u = s.chain[static_cast<za::SizeT>(limb)][0], lo = s.chain[static_cast<za::SizeT>(limb)][1];
    if(r.weight <= 0.f || u < 0 || lo < 0 || !drivable(s, p, u) || !drivable(s, p, lo))
    {
        return;
    }
    const ragdoll::Bone& U = p.rig->bones[u];
    const ragdoll::Bone& L = p.rig->bones[lo];
    const glm::quat qp = bodyRot(p, U.parent), qu = bodyRot(p, u);
    const glm::vec3 hip = worldPoint(p, u, U.pivot);
    const glm::vec3 want = r.goal - hip;
    const float reach = glm::length(want) / za::max(p.k, 1e-6f);
    glm::quat relL = target[static_cast<za::SizeT>(lo)];
    glm::vec3 v;
    if(L.joint == ragdoll::Joint::Hinge)
    {
        const glm::vec3 axis = glm::normalize(L.hinge);
        const float lo2 = s.lower[static_cast<za::SizeT>(lo)] * 0.98f, hi2 = s.upper[static_cast<za::SizeT>(lo)] * 0.98f;
        const float bend = ragpose::bendFor(U.pivot, L.pivot, L.end, axis, za::min(lo2, hi2), za::max(lo2, hi2), reach, v);
        relL = glm::angleAxis(bend, axis);
    }
    else
    {
        v = (L.pivot - U.pivot) + relL * (L.end - L.pivot);
    }
    const glm::quat qu2 = ragpose::between(qu * v, want) * qu;
    const glm::quat relU = glm::normalize(glm::inverse(qp) * qu2);
    const float w = za::clamp(r.weight, 0.f, 1.f);
    target[static_cast<za::SizeT>(u)] = glm::slerp(target[static_cast<za::SizeT>(u)], ragpose::nearTo(relU, target[static_cast<za::SizeT>(u)]), w);
    target[static_cast<za::SizeT>(lo)] = glm::slerp(target[static_cast<za::SizeT>(lo)], ragpose::nearTo(relL, target[static_cast<za::SizeT>(lo)]), w);
}

} // namespace

namespace detail
{

// The end of a limb's lower bone (a hand, a foot) in the world now (metres).
[[nodiscard]] glm::vec3 limbEnd(const State& s, const Parts& p, int limb)
{
    const int lo = s.chain[static_cast<za::SizeT>(limb)][1];
    return lo >= 0 ? worldPoint(p, lo, p.rig->bones[lo].end) : glm::vec3{0.f};
}

[[nodiscard]] float limbLength(const State& s, const Parts& p, int limb)
{
    const int u = s.chain[static_cast<za::SizeT>(limb)][0], lo = s.chain[static_cast<za::SizeT>(limb)][1];
    if(u < 0 || lo < 0)
    {
        return 0.f;
    }
    const ragdoll::Bone& U = p.rig->bones[u];
    const ragdoll::Bone& L = p.rig->bones[lo];
    return (glm::length(L.pivot - U.pivot) + glm::length(L.end - L.pivot)) * p.k;
}

[[nodiscard]] bool limbWhole(const State& s, const Parts& p, int limb)
{
    const int u = s.chain[static_cast<za::SizeT>(limb)][0], lo = s.chain[static_cast<za::SizeT>(limb)][1];
    return u >= 0 && lo >= 0 && drivable(s, p, u) && drivable(s, p, lo);
}

[[nodiscard]] float floorAt(const Context& c, const glm::vec3& at, float above, float fallback)
{
    if(!c.floorBelow)
    {
        return fallback;
    }
    const float f = c.floorBelow(at + glm::vec3{0.f, 0.f, above}, above * 3.f);
    return f > -1e29f ? f : fallback;
}

} // namespace detail

namespace
{

// A frame of rolling over: the whole body turned as one at rollRate (`sign`) about level `axis` (along its spine)
// through the edge it rolls over (on the floor, on the side going down: the floor holds that side up and takes the
// turn), eased: its parts' spin about it and their motion round it.
void rollOver(const Parts& p, const Context& c, const glm::vec3& axis, float sign, const glm::vec3& pelvis, float floor)
{
    const glm::vec3 up{0.f, 0.f, 1.f};
    b3Body_SetAwake(p.body[0], true);
    glm::vec3 side = glm::cross(up, axis);
    if(glm::dot(glm::cross(axis, side) * sign, up) > 0.f)
    {
        side = -side;
    }
    float width = 0.1f;
    for(int b = 0; b < p.count; b++)
    {
        if(!cut(p, b) && p.rig->bones[b].joint != ragdoll::Joint::Loose)
        {
            width = za::max(width, glm::dot(glmv(b3Body_GetWorldCenter(p.body[b])) - pelvis, side));
        }
    }
    glm::vec3 pivot = pelvis + side * width;
    pivot.z = floor;
    const float g = 1.f - za::exp(-c.dt / 0.05f);
    for(int b = 0; b < p.count; b++)
    {
        if(cut(p, b) || p.rig->bones[b].joint == ragdoll::Joint::Loose)
        {
            continue;
        }
        const glm::vec3 w = glmv(b3Body_GetAngularVelocity(p.body[b]));
        const float add = (sign * rollRate - glm::dot(w, axis)) * g;
        glm::vec3 arm = glmv(b3Body_GetWorldCenter(p.body[b])) - pivot;
        arm -= axis * glm::dot(arm, axis);
        b3Body_SetAngularVelocity(p.body[b], b3v(w + axis * add));
        b3Body_SetLinearVelocity(p.body[b], b3v(glmv(b3Body_GetLinearVelocity(p.body[b])) + glm::cross(axis * add, arm)));
    }
}

// ---- Deaths ----
// Each frame: where a stumble's step goes, the wounded part's hand, a crawl's strokes (and its pull on the torso); a
// crawl begins once he lies face down, low and nearly still after his death animation.
// A frame of a dying cockroach's flail (on his back, vr_ragdoll_active_crawl_back): his arms paw the air over his chest,
// his legs pedal over his hips, out of step and quicker at first, his chest twisting and his body rocking side to side
// (a capped turn about his spine, its way changing with the strokes), all weakening to nothing by its end (the fades:
// deathFrame), then limp.
void backFlail(State& s, const Parts& p, const Context& c, const glm::vec3& pelvis, int chest)
{
    Death& d = s.death;
    if(c.now >= d.flailUntil)
    {
        d.crawl = false;
        for(Reach& r : s.reach)
        {
            r.weight = 0.f;
        }
        if(vr_debug_ragdoll.value)
        {
            Con_Printf("ragdoll: %d flailed on his back %.1f s, goes limp\n", c.num, d.flailUntil - d.flailFrom);
        }
        return;
    }
    const float age = static_cast<float>(c.now - d.flailFrom);
    const float span = za::max(static_cast<float>(d.flailUntil - d.flailFrom), 0.1f);
    const float left = 1.f - smooth01(age / span); // (its vigour: all at first, none at the end)
    const float seed = static_cast<float>((c.num * 37) % 101) * 0.13f;
    const glm::quat qc = bodyRot(p, chest), qp = bodyRot(p, 0);
    const glm::vec3 front = qc * glm::vec3{1.f, 0.f, 0.f}, side = qc * glm::vec3{0.f, 1.f, 0.f}, spine = qc * glm::vec3{0.f, 0.f, 1.f};
    const glm::vec3 pFront = qp * glm::vec3{1.f, 0.f, 0.f}, pSide = qp * glm::vec3{0.f, 1.f, 0.f}, pDown = qp * glm::vec3{0.f, 0.f, -1.f};
    const float rate = 6.2831853f * (1.3f + 0.9f * left); // rad/s: its strokes, slowing
    const float ph = age * rate;
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
        const float sgn = (limb % 2) == 0 ? 1.f : -1.f;
        // (Each limb its own beat: out of step with its pair, a wobble of its own.)
        const float a = ph * (limb < 2 ? 1.f : 0.8f) + static_cast<float>(limb) * 1.9f + seed;
        const float wob = za::sin(a * 0.37f + seed * 2.f);
        glm::vec3 goal;
        if(limb < 2)
        {
            // Pawing the air over his chest: up, in towards the middle and out, along his body and back.
            goal = root + (front * (0.6f + 0.25f * za::sin(a)) + side * (sgn * (0.15f + 0.3f * za::cos(a + wob))) +
                              spine * (0.25f * za::sin(a * 0.5f + wob))) *
                              len;
        }
        else
        {
            // Pedalling over his hips: knees drawn up and kicked out, down his line.
            goal = root + (pFront * (0.45f + 0.3f * za::sin(a)) + pDown * (0.45f + 0.3f * za::cos(a)) +
                              pSide * (sgn * 0.15f * wob)) *
                              len;
        }
        r.goal = goal;
        r.weight = za::min(1.f, 0.3f + left);
        r.strength = 0.9f * left + 0.1f;
    }
    // His chest twisting against his pelvis, his head tossed.
    for(int b = 1; b < p.count; b++)
    {
        const Channel ch = channelOf(p.rig->bones[b]);
        if(drivable(s, p, b) && (ch == Channel::Torso || ch == Channel::Head))
        {
            const float tw = (ch == Channel::Head ? 0.5f : 0.35f) * left * za::sin(ph * 0.5f + static_cast<float>(b) + seed);
            s.target[static_cast<za::SizeT>(b)] =
                glm::normalize(s.target[static_cast<za::SizeT>(b)] * glm::angleAxis(tw, glm::vec3{0.f, 0.f, 1.f}));
        }
    }
    // Rocking side to side about his spine (its flopping limbs' kick against the floor), never over: a turn that
    // eases towards a rock of a little, flat on his back.
    const glm::vec3 axis = glm::length(glm::vec2{spine}) > 1e-3f ? glm::vec3{glm::normalize(glm::vec2{spine}), 0.f} : pSide;
    // (Its turn about `axis` from face up: front turned by +a about it is -a's sine on this cross.)
    const float tilt = za::atan2(-glm::dot(glm::cross(front, glm::vec3{0.f, 0.f, 1.f}), axis), front.z);
    const float want = 0.35f * left * za::sin(ph * 0.45f + seed);
    const float most = 0.05f * s.totalMass * c.gravity * 0.3f; // N m
    for(const int part : {0, chest})
    {
        const float m = b3Body_GetMass(p.body[part]);
        const float spin = glm::dot(glmv(b3Body_GetAngularVelocity(p.body[part])), axis);
        const float tq = za::clamp(m * 0.0144f * ((want - tilt) * 4.f - spin) / 0.15f, -most, most);
        b3Body_ApplyTorque(p.body[part], b3v(axis * tq), true);
        if(part == chest)
        {
            break;
        }
    }
    (void)pelvis;
}

void deathFrame(State& s, const Parts& p, const Context& c, float t)
{
    Death& d = s.death;
    const glm::vec3 up{0.f, 0.f, 1.f};
    // A stumble: the leg behind steps along his motion, its foot lifted on the way.
    if(d.stepLeg >= 0)
    {
        const auto leg = static_cast<za::SizeT>(d.stepLeg);
        if(!limbWhole(s, p, d.stepLeg))
        {
            d.stepLeg = -1;
        }
        else
        {
            const float len = limbLength(s, p, d.stepLeg);
            if(d.stepFrom == glm::vec3{0.f})
            {
                const glm::vec3 foot = limbEnd(s, p, d.stepLeg);
                const glm::vec3 pelvis = glmv(b3Body_GetWorldCenter(p.body[0]));
                const float speed = glm::length(glm::vec2{d.walk});
                const glm::vec3 dir = speed > 1e-3f ? glm::vec3{d.walk.x, d.walk.y, 0.f} / speed : glm::vec3{1.f, 0.f, 0.f};
                const float stride = za::clamp(speed * 0.3f, 0.35f * len, 0.9f * len);
                // (Ahead of the pelvis, as far to its side as the foot is.)
                const glm::vec3 side = foot - pelvis - dir * glm::dot(foot - pelvis, dir);
                glm::vec3 to = pelvis + dir * stride + glm::vec3{side.x, side.y, 0.f};
                to.z = floorAt(c, to, len, foot.z);
                d.stepFrom = foot;
                d.stepTo = to;
            }
            const float u = za::clamp(t / stepTime, 0.f, 1.f);
            Reach& r = s.reach[leg];
            r.goal = glm::mix(d.stepFrom, d.stepTo, smooth01(u)) + up * (0.15f * len * za::sin(3.14159265f * u));
            r.weight = 1.f - smooth01((t - stepTime - 0.25f) / 0.35f);
            r.strength = 0.9f;
            if(r.weight <= 0.f)
            {
                d.stepLeg = -1;
            }
        }
    }
    // Holding the wound: a hand to it (a little off its surface, outward), until the arms fade.
    if(d.woundArm >= 0 && d.woundPart >= 0)
    {
        Reach& r = s.reach[static_cast<za::SizeT>(d.woundArm)];
        if(!limbWhole(s, p, d.woundArm) || cut(p, d.woundPart) || d.crawlFrom >= 0.0)
        {
            r.weight = 0.f;
            d.woundArm = -1;
        }
        else
        {
            const b3BodyId body = p.body[d.woundPart];
            const glm::vec3 out = fromB3(b3Body_GetRotation(body)) * d.woundOut;
            r.goal = glmv(b3Body_GetWorldPoint(body, b3v(d.woundAt))) + out * (handThick / p.m2u);
            r.weight = smooth01((t - 0.1f) / 0.35f) * (1.f - smooth01((t - 1.6f * d.length) / (0.6f * d.length)));
            r.strength = 0.8f;
        }
    }
    // Crawling.
    if(!d.crawl)
    {
        return;
    }
    const int chest = chestOf(p);
    const glm::vec3 pelvis = glmv(b3Body_GetWorldCenter(p.body[0]));
    // A hand on his pelvis or chest: no roll and no pull (his joints already go limp; these would drag him out of it).
    const bool grabbed = (heldLimbs(*p.rig, p.count, c.held) & (1u | (1u << chest))) != 0u;
    if(d.crawlFrom < 0.0)
    {
        if(t < 1.1f * d.length)
        {
            return;
        }
        const glm::vec3 front = bodyRot(p, chest) * glm::vec3{1.f, 0.f, 0.f}; // (the rig's x: forward)
        const float speed = glm::length(glmv(b3Body_GetLinearVelocity(p.body[0])));
        const float floor = floorAt(c, pelvis, 0.6f, pelvis.z - 0.2f);
        const bool prone = glm::dot(front, -up) > 0.35f;
        const bool down = speed < 0.6f && pelvis.z - floor < 0.45f;
        const glm::vec3 spine = glmv(b3Body_GetWorldCenter(p.body[chest])) - pelvis;
        // Lying on his back (vr_ragdoll_active_crawl_back): no roll onto his front (a glide, turned by no limb): he
        // flails there, a dying cockroach, then goes limp.
        if(d.flailFrom < 0.0 && d.rollFrom < 0.0 && !prone && down && vr_ragdoll_active_crawl_back.value != 0.f &&
            glm::dot(front, up) > 0.2f)
        {
            d.flailFrom = c.now;
            d.flailUntil = c.now + za::max(vr_ragdoll_active_crawl_time.value, 0.5f);
            const float end = static_cast<float>(d.flailUntil - s.begun);
            const float from = static_cast<float>(c.now - s.begun);
            s.fade[static_cast<za::SizeT>(Channel::Arms)] = Fade{0.9f, 0.f, from, end};
            s.fade[static_cast<za::SizeT>(Channel::Legs)] = Fade{0.8f, 0.f, from, end};
            s.fade[static_cast<za::SizeT>(Channel::Torso)] = Fade{0.5f, 0.f, from, end};
            s.fade[static_cast<za::SizeT>(Channel::Head)] = Fade{0.5f, 0.f, from, end};
            if(vr_debug_ragdoll.value)
            {
                Con_Printf("ragdoll: %d lies on his back (face up %.2f): flails %.1f s instead of rolling over to crawl\n",
                    c.num, glm::dot(front, up), d.flailUntil - d.flailFrom);
            }
        }
        if(d.flailFrom >= 0.0)
        {
            backFlail(s, p, c, pelvis, chest);
            return;
        }
        // Lying on his back or side, still: he rolls onto his front first (his torso turned about his spine, level,
        // at rollRate: the rest follows by the joints), then crawls.
        if(!prone && down && d.rollFrom < 0.0 && glm::length(glm::vec2{spine}) > 1e-3f)
        {
            d.rollFrom = c.now;
            d.rollAxis = glm::vec3{glm::normalize(glm::vec2{spine}), 0.f};
            // (The way that takes his front down; flat on his back either way: towards his wound's side.)
            const float way = glm::dot(glm::cross(d.rollAxis, front), -up);
            d.rollSign = za::abs(way) > 0.1f ? (way > 0.f ? 1.f : -1.f) : (d.woundArm == 1 ? -1.f : 1.f);
        }
        if(d.rollFrom >= 0.0 && !prone && c.now - d.rollFrom < rollTime)
        {
            // (The whole body turned as one about the line along his spine through his pelvis, eased: its parts' spin
            // about it and their motion round it.)
            if(vr_debug_ragdoll.value >= 2.f)
            {
                Con_Printf("ragdoll: %d rolling %.2f s: face down %.2f, spin %.2f rad/s about his spine (%s)\n", c.num,
                    c.now - d.rollFrom, glm::dot(front, -up), glm::dot(glmv(b3Body_GetAngularVelocity(p.body[chest])), d.rollAxis),
                    b3Body_IsAwake(p.body[0]) ? "awake" : "asleep");
            }
            if(!grabbed)
            {
                rollOver(p, c, d.rollAxis, d.rollSign, pelvis, floor);
            }
            return;
        }
        if(prone && down && limbWhole(s, p, 0) && limbWhole(s, p, 1))
        {
            const glm::vec3 head = glmv(b3Body_GetWorldCenter(p.body[chest])) - pelvis;
            const glm::vec2 level{head.x, head.y};
            d.crawlDir = glm::length(level) > 1e-3f ? glm::vec3{glm::normalize(level), 0.f} : glm::vec3{front.x, front.y, 0.f};
            d.crawlFrom = c.now;
            d.crawlUntil = c.now + za::max(vr_ragdoll_active_crawl_time.value, 0.5f);
            d.crawlArm = 0;
            d.strokeFrom = c.now;
            d.pulling = false;
            d.crawlStart = pelvis;
            const float end = static_cast<float>(d.crawlUntil - s.begun);
            s.fade[static_cast<za::SizeT>(Channel::Arms)] = Fade{0.9f, 0.f, end, end + 1.f};
            s.fade[static_cast<za::SizeT>(Channel::Torso)] = Fade{0.45f, 0.f, end, end + 1.f};
            s.fade[static_cast<za::SizeT>(Channel::Head)] = Fade{0.5f, 0.f, end, end + 1.f};
            s.fade[static_cast<za::SizeT>(Channel::Legs)] = Fade{0.1f, 0.f, end, end + 1.f};
        }
        else if(t > 1.1f * d.length + 1.5f + (d.rollFrom >= 0.0 ? rollTime + 0.5f : 0.f))
        {
            d.crawl = false; // (not face down: he dies where he lies)
            if(vr_debug_ragdoll.value)
            {
                Con_Printf("ragdoll: %d won't crawl: face down %.2f (0.35), %.2f m/s (0.6), %.2f m above the floor (0.45)\n",
                    c.num, glm::dot(front, -up), speed, pelvis.z - floor);
            }
        }
        return;
    }
    for(Reach& r : s.reach)
    {
        r.weight = 0.f;
    }
    if(c.now > d.crawlUntil || !limbWhole(s, p, 0) || !limbWhole(s, p, 1))
    {
        d.crawl = false;
        if(vr_debug_ragdoll.value)
        {
            Con_Printf("ragdoll: %d crawled %.1f units in %.1f s%s, goes limp\n", c.num, d.crawled * p.m2u,
                c.now - d.crawlFrom, d.rollFrom >= 0.0 ? " (rolled onto his front first)" : "");
        }
        return;
    }
    const float phase = static_cast<float>(c.now - d.strokeFrom) / strokeTime;
    if(phase >= 1.f)
    {
        d.crawlArm = 1 - d.crawlArm;
        d.strokeFrom = c.now;
        d.pulling = false;
    }
    const float ph = static_cast<float>(c.now - d.strokeFrom) / strokeTime;
    const int arm = d.crawlArm;
    const int u = s.chain[static_cast<za::SizeT>(arm)][0];
    const glm::vec3 shoulder = worldPoint(p, u, p.rig->bones[u].pivot);
    const float len = limbLength(s, p, arm);
    Reach& r = s.reach[static_cast<za::SizeT>(arm)];
    r.strength = 0.9f;
    r.weight = 1.f;
    if(ph < reachShare)
    {
        // Reaching ahead to the floor, the hand lifted on the way.
        const float k = ph / reachShare;
        glm::vec3 to = shoulder + d.crawlDir * (0.8f * len);
        to.z = floorAt(c, to, len, shoulder.z - 0.5f * len) + 0.02f;
        r.goal = to + up * (0.25f * len * za::sin(3.14159265f * k));
    }
    else
    {
        // Put down: the hand held where it is, the arm drawn in; the torso pulled along (at most Crawl Force body
        // weights, to crawlSpeed).
        if(!d.pulling)
        {
            d.pulling = true;
            d.planted = limbEnd(s, p, arm);
        }
        r.goal = d.planted;
        const glm::vec3 v = glmv(b3Body_GetLinearVelocity(p.body[0]));
        const float along = glm::dot(v, d.crawlDir);
        const float most = za::max(vr_ragdoll_active_crawl_force.value, 0.f) * s.totalMass * c.gravity;
        // (Enough to drag him over the floor, most of his weight on it: its friction, then the rest to crawlSpeed.)
        const float f = grabbed || along >= crawlSpeed ? 0.f
                                            : za::clamp(0.9f * s.totalMass * c.gravity + s.totalMass * (crawlSpeed - along) / 0.2f, 0.f, most);
        const glm::vec3 force = d.crawlDir * (f * 0.5f);
        pushPart(p, 0, force);
        pushPart(p, chest, force);
        if(vr_debug_ragdoll.value >= 3.f)
        {
            Con_Printf("ragdoll: %d pulls %.0f N (most %.0f) at %.2f m/s along, the hand %.2f m off its hold\n", c.num, f, most,
                along, glm::distance(limbEnd(s, p, arm), d.planted));
        }
    }
    d.crawled = glm::length(glm::vec2{pelvis - d.crawlStart});
}

// ---- Staggers ----
// A shove's stagger: its pelvis pinned up and carried along the shove (easing out over `time`), its top pushed, its
// weight held up by its muscles (support), its feet planted where they stand (each leg's reach) and stepping after its
// pelvis (a capture step: the foot farthest from where it should stand under the pelvis a step's time on, lifted along an
// arc to there, one at a time, at most staggerStepsMost). Its middle leaning past vr_ragdoll_active_stagger_lean over its
// feet, or its pelvis sagging below staggerSag of its height: it falls. Done travelling and stepping, leaning little and
// nearly still (or after staggerMost s still up): it has recovered.
// Walked (ROUND22.md "Stagger balance walk"): its pin let down (its knees bent), its
// planted feet reaching for and held where they were put down (no glide) until its hip is far from them, its feet
// stepping in turn and lifted high (vr_ragdoll_active_stagger_lift), its arms flailing out to its sides.
constexpr float staggerStepTime = 0.22f; // s: a step
constexpr float staggerStepQuickest = 0.13f; // s: a fast backpedal's step
constexpr int staggerCatchSteps = 3;     // steps past its backpedal's to catch itself, still leaning
constexpr float staggerStride = 0.75f;   // of a leg's length: a step lands at most this far (level) from its pelvis then
constexpr float staggerCatch = 0.12f;    // s: leaning past its limit three times this long, it falls
constexpr float staggerFar = 0.1f;       // s: past twice its limit this long, it falls
constexpr float staggerStepAt = 0.2f;    // of a leg's length: a foot this far from its stance under the capture point steps
constexpr float staggerSag = 0.55f;      // of its pelvis's height: it gave way
constexpr float staggerSteady = 12.f;    // deg: leaning less, it is steady
constexpr float staggerMost = 1.2f;      // s: vr_ragdoll_active_stagger_settle_time at most
constexpr float staggerSettleShare = 0.9f; // of its carry: gone this far, it may recover (vr_ragdoll_active_stagger_settle)
constexpr float staggerAir = 0.06f;      // s: no floor under it this long (vr_ragdoll_active_stagger_drop), it falls
constexpr float staggerFloorOverFeet = 12.f; // units: the floor under its pelvis higher than its feet by more than this is
                                             // something it stands beside (a step, a crate), not its floor (the ogre's
                                             // feet's ends lie up to 8 units below their soles)
constexpr float staggerPlant = 0.6f;     // a planted leg's reach to where its foot is: its knee bends as its hip comes down
                                         // (the clip's stand pose held them straight: tiptoeing on stilts)
constexpr float staggerReachStep = 0.45f; // of a leg's length: a planted foot this far (level) from its hip steps
constexpr float staggerHoldReach = 0.97f; // of a leg's length: a planted foot within this of its hip is held (no glide)
constexpr float staggerHoldTime = 0.03f;  // s: its level motion worked off over
constexpr float staggerWheel = 1.7f;     // Hz: its arms wheeling for its balance (the other arm's a little quicker)
constexpr float staggerUpright = 8.f;    // rad/s per radian its pelvis is pitched off its stance's, steady (turnWhole)
constexpr float staggerUprightMost = 4.f; // rad/s

// The share of its travel its pin's anchor has gone at `t` (easing out: fast at first).
[[nodiscard]] float staggerShare(const Stagger& g, float t)
{
    const float u = za::clamp(t / za::max(g.time, 0.05f), 0.f, 1.f);
    return 1.f - (1.f - u) * (1.f - u);
}

// Its pin let down at `t` (metres): its knees bent for its backpedal (vr_ragdoll_active_stagger_crouch), eased in at
// once and out as its carry ends.
[[nodiscard]] float staggerDip(const Stagger& g, float t)
{
    // (A parry's, up at its time: risen by then, not popping up as it is handed back.)
    const float end = g.recoverAt > 0.f ? za::min(g.time, g.recoverAt - 0.35f) : g.time;
    return za::clamp(vr_ragdoll_active_stagger_crouch.value, 0.f, 0.3f) * g.height * smooth01(t / 0.15f) *
           (1.f - smooth01((t - end) / 0.35f));
}

// Its walk measured a frame (Stagger's measures): the knees' bend, the planted feet's slide, the stepping foot's lift,
// the hands about the shoulders.
void staggerMeasure(Stagger& g, const State& s, const Parts& p, float t)
{
    for(int limb = 0; limb < 4; limb++)
    {
        if(!limbWhole(s, p, limb))
        {
            continue;
        }
        const int u = s.chain[static_cast<za::SizeT>(limb)][0], lo = s.chain[static_cast<za::SizeT>(limb)][1];
        const glm::vec3 root = worldPoint(p, u, p.rig->bones[u].pivot);
        const glm::vec3 end = limbEnd(s, p, limb);
        if(limb < 2)
        {
            const auto i = static_cast<za::SizeT>(limb);
            // (Its motion about where it holds them, once thrown out: their spread, rms.)
            const glm::vec3 off = end - root;
            if(t > 0.2f)
            {
                g.handSum[i] += off;
                g.handSq[i] += off * off;
                g.handFrames += limb == 0 ? 1 : 0;
            }
            continue;
        }
        const auto i = static_cast<za::SizeT>(limb - 2);
        const glm::vec3 knee = worldPoint(p, lo, p.rig->bones[lo].pivot);
        const glm::vec3 a = knee - root, b = end - knee;
        if(glm::length(a) > 1e-4f && glm::length(b) > 1e-4f)
        {
            // (Its bend from its stand's: a rig's shin may run on to its toe.)
            const float raw = glm::degrees(za::acos(za::clamp(glm::dot(glm::normalize(a), glm::normalize(b)), -1.f, 1.f)));
            g.knee0[i] = g.measured ? g.knee0[i] : raw;
            const float bend = raw - g.knee0[i];
            g.kneeLo = za::min(g.kneeLo, bend);
            g.kneeHi = za::max(g.kneeHi, bend);
            g.kneeSum += bend;
            g.kneeFrames++;
        }
        if(limb == g.stepLeg)
        {
            g.stepLift = za::max(g.stepLift, end.z - za::min(g.stepA.z, g.stepB.z));
        }
        else if(g.measured)
        {
            g.glide += glm::length(level(end - g.footWas[i]));
        }
        g.footWas[i] = end;
    }
    g.measured = true;
}

void staggerGaitLine(const Stagger& g, const Parts& p, int num)
{
    const float m = p.m2u;
    const float steps = static_cast<float>(za::max(g.steps, 1));
    float arms = 0.f;
    for(za::SizeT i = 0; i < 2; i++)
    {
        const float n = static_cast<float>(za::max(g.handFrames, 1));
        const glm::vec3 mean = g.handSum[i] / n;
        const glm::vec3 var = g.handSq[i] / n - mean * mean;
        arms += 0.5f * za::sqrt(za::max(var.x + var.y + var.z, 0.f));
    }
    Con_Printf("gait: %d steps %d, stride %.1f u, lift %.1f u (most %.1f), knee %.0f-%.0f deg (mean %.0f), glide %.1f u (%.1f a step), "
               "arms %.1f u\n",
        num, g.steps, g.strideSum / steps * m, g.liftSum / steps * m, g.liftMost * m, g.kneeLo, g.kneeHi,
        g.kneeSum / static_cast<float>(za::max(g.kneeFrames, 1)), g.glide * m,
        g.glide / steps * m, arms * m);
}

void staggerFrame(State& s, const Parts& p, const Context& c, float t, const ClipPose& pose)
{
    Stagger& g = s.stagger;
    if(g.outcome == Outcome::Going)
    {
        staggerMeasure(g, s, p, t);
    }
    const glm::vec3 up{0.f, 0.f, 1.f};
    const glm::vec3 pelvis = glmv(b3Body_GetWorldCenter(p.body[0]));
    glm::vec3 comVel{0.f};
    const glm::vec3 com = massCentre(p, &comVel);
    // Its base: its feet where they stand, the one stepping where it goes down.
    glm::vec3 support{0.f};
    int feet = 0;
    float footZ = 1e30f;
    for(int leg = 2; leg < 4; leg++)
    {
        if(!limbWhole(s, p, leg))
        {
            continue;
        }
        const glm::vec3 foot = leg == g.stepLeg ? g.stepB : limbEnd(s, p, leg);
        support += foot;
        footZ = za::min(footZ, leg == g.stepLeg ? za::min(g.stepA.z, g.stepB.z) : foot.z);
        feet++;
    }
    support = feet > 0 ? support / static_cast<float>(feet) : pelvis;
    footZ = feet > 0 ? footZ : pelvis.z - g.height;
    g.lean = glm::degrees(za::atan2(glm::length(level(com - support)), za::max(com.z - footZ, 0.05f)));    g.leanMost = za::max(g.leanMost, g.lean);
    // (Its pelvis over the floor under it, against how high it stood: its feet's ends may lie below their soles.)
    // (A step or a crate under its pelvis, higher than its feet, is not its floor: else, stepping back off it on level
    // ground read as a drop, "no floor under it", and a sag.)
    const float rawBelow = c.floorBelow ? c.floorBelow(pelvis, 2.f) : -1e30f;
    const float below = rawBelow > -1e29f ? za::min(rawBelow, footZ + staggerFloorOverFeet / p.m2u) : rawBelow;
    const float floor = below > -1e29f ? below : footZ;
    if(g.overFloor <= 0.f)
    {
        g.overFloor = za::max(pelvis.z - floor, 0.1f);
    }
    g.sag = za::min(g.sag, (pelvis.z - floor) / g.overFloor);
    if(vr_debug_ragdoll.value >= 2.f)
    {
        const glm::vec3 anchor = g.pelvis0 + g.dir * (g.travel * staggerShare(g, t));
        const glm::vec3 po = (pelvis - anchor) * p.m2u, co = (com - support) * p.m2u;
        const glm::vec3 torso = glmv(b3Body_GetWorldCenter(p.body[chestOf(p)])) - pelvis, legs = pelvis - support;
        const auto tilt = [](const glm::vec3& v) {
            return glm::degrees(za::atan2(glm::length(glm::vec2{v.x, v.y}), za::max(v.z, 1e-3f)));
        };
        const glm::vec3 ax = glm::degrees(ragpose::rotationVector(
            glm::normalize(s.turn * glm::quat{1.f, 0.f, 0.f, 0.f}) * glm::inverse(bodyRot(p, 0))));
        Con_Printf("stagger: %d %.2f s lean %.0f (torso %.0f, legs %.0f), pelvis from its pin's %.1f along %.1f up, middle "
                   "over its feet %.1f along %.1f up, step %d (%d done); pelvis turned %.0f %.0f %.0f from its standing yaw\n",
            c.num, t, g.lean, tilt(torso), tilt(legs), glm::dot(po, g.dir), po.z, glm::dot(co, g.dir), co.z, g.stepLeg,
            g.steps, ax.x, ax.y, ax.z);
    }
    if(g.outcome == Outcome::Going)
    {
        // A step: the foot farthest from its stance under its middle's capture point (where its middle comes to rest over
        // a base: its level motion times the square root of its height over g, ahead of it).
        // (Beaten, its steps taken: no more, it gives way below. Steady, its backpedal's steps taken and still leaning: a
        // few more to catch itself.)
        const int stepsCap = g.giveAfter >= 0 ? g.giveAfter : g.stepsMost + (g.lean > staggerSteady ? staggerCatchSteps : 0);
        if(g.stepLeg < 0 && g.steps < stepsCap)
        {
            const float h = za::max(com.z - footZ, 0.1f);
            const glm::vec3 then = level(com) + level(comVel) * za::sqrt(h / za::max(c.gravity, 1.f));
            // (Quicker steps the faster it goes, and the farther it leans: a backpedal's, a catch.)
            const float stepTime = za::clamp(staggerStepTime - 0.02f * glm::length(level(comVel)) -
                                                 0.004f * za::max(g.lean - staggerSteady, 0.f),
                staggerStepQuickest, staggerStepTime);
            // (Its feet in turn, as a walk's: the other foot than the last step's when it is near as far off; and a planted
            // foot left far from its hip steps, however its middle goes: the legs carry it, not the pin dragging them.)
            int best = -1;
            float bestD = 0.f;
            za::Array<float, 2> need{};
            for(int leg = 2; leg < 4; leg++)
            {
                const auto i = static_cast<za::SizeT>(leg - 2);
                if(!limbWhole(s, p, leg))
                {
                    continue;
                }
                const float len = limbLength(s, p, leg);
                const float d = glm::length(level(g.planted[i] - (then + g.stance[i])));
                const float atStep = staggerStepAt * (g.lean > staggerSteady ? 0.6f : 1.f) * len;
                const glm::vec3 hip = worldPoint(p, s.chain[i + 2][0], p.rig->bones[s.chain[i + 2][0]].pivot);
                const float fromHip = glm::length(level(g.planted[i] - hip)) / za::max(staggerReachStep * len, 1e-3f);
                need[i] = za::max(d / za::max(atStep, 1e-3f), fromHip);
                const float score = need[i] > 1.f ? need[i] * (leg == g.lastLeg ? 0.7f : 1.f) : 0.f;
                if(score > bestD)
                {
                    bestD = score;
                    best = leg;
                }
            }
            if(best >= 0 && need[static_cast<za::SizeT>(best - 2)] > 1.f)
            {
                const auto i = static_cast<za::SizeT>(best - 2);
                const float len = limbLength(s, p, best);
                const glm::vec3 way = level(then - level(g.planted[i] - g.stance[i]));
                const float wl = glm::length(way);
                glm::vec3 to = level(then + g.stance[i]) + (wl > 1e-3f ? way / wl : g.dir) * (0.06f * len);
                // (Within its leg's stride of where its pelvis will be as the foot lands: past it the leg can't reach and
                // the foot would hang in the air.)
                const glm::vec3 hipThen = level(pelvis) + level(comVel) * stepTime;
                const glm::vec3 off = to - hipThen;
                const float offL = glm::length(off);
                if(offL > staggerStride * len)
                {
                    to = hipThen + off * (staggerStride * len / offL);
                }
                // (As high over the floor there as it is here: a foot's end may lie below its sole, the ogre's 8 units.)
                const glm::vec3 from = g.planted[i];
                to.z = from.z + floorAt(c, glm::vec3{to.x, to.y, from.z}, len, from.z) - floorAt(c, from, len, from.z);
                g.stepLeg = best;
                g.stepFrom = c.now;
                g.stepTime = stepTime;
                g.stepA = g.planted[i];
                g.stepB = to;
            }
        }
        const float limit = za::clamp(vr_ragdoll_active_stagger_lean.value, 5.f, 80.f);
        const float speed = glm::length(level(glmv(b3Body_GetLinearVelocity(p.body[0]))));
        // (Past its lean a while, not stepping, or past twice it a moment: a step may still catch it; sagging, at once.)
        g.over = g.lean > limit && g.stepLeg < 0 ? (g.over < 0.f ? t : g.over) : -1.f;
        g.farSince = g.lean > 2.f * limit ? (g.farSince < 0.f ? t : g.farSince) : -1.f;
        const bool tooFar = g.farSince >= 0.f && t - g.farSince >= staggerFar;
        // Beaten: it gives way once its steps are taken (or its travel is over first), as a step lands.
        const bool gives = g.giveAfter >= 0 && g.stepLeg < 0 && (g.steps >= g.giveAfter || t > g.time);
        // (Over on its lean only far past its limit, or a while past it: a beaten one goes on its steps' count, a steady one
        // takes catch steps; or sagging.)
        const bool tips = g.giveAfter >= 0 ? tooFar
                                           : (g.over >= 0.f && t - g.over >= staggerCatch * 3.f) || tooFar;
        // No floor under it (backpedalled off a ledge, the top of stairs; ROUND22.md, "No backpedalling on air"): the
        // floor under its pelvis vr_ragdoll_active_stagger_drop lower than where it stood, a moment: it goes over, sure-
        // footed or not (its pin would carry it on level, its feet stepping on air).
        const float drop = za::max(vr_ragdoll_active_stagger_drop.value, 0.f) / p.m2u;
        const bool floorGone = drop > 0.f && c.floorBelow && (below < -1e29f || pelvis.z - below > g.overFloor + drop);
        g.noFloor = floorGone ? (g.noFloor < 0.f ? t : g.noFloor) : -1.f;
        const bool offEdge = g.noFloor >= 0.f && t - g.noFloor >= staggerAir;
        // (Sure-footed, a parry's on level ground: never over by itself.)
        if(offEdge || (!g.sure && (gives || tips || g.sag < staggerSag)))
        {
            g.outcome = Outcome::Fell;
            g.decided = c.now;
            if(vr_debug_ragdoll.value || vr_knockdown_debug.value)
            {
                if(offEdge)
                {
                    Con_Printf("stagger: %d no floor at %.0f %.0f %.0f: pelvis %.1f units over %s, it stood %.1f over the "
                               "floor (drop %.0f)\n",
                        c.num, pelvis.x * p.m2u, pelvis.y * p.m2u, pelvis.z * p.m2u,
                        below < -1e29f ? 0.f : (pelvis.z - below) * p.m2u,
                        below < -1e29f ? "nothing (none within 2 m)" : "the floor", g.overFloor * p.m2u, drop * p.m2u);
                }
                Con_Printf("stagger: %d fell at %.2f s%s: leaning %.0f deg (at most %.0f), its pelvis at %.0f%% of its height, "
                           "%d steps, moved %.0f units (its middle %.0f along the shove from its feet, %.0f aside)\n",
                    c.num, t, offEdge ? " (no floor under it)" : gives ? " (gave way)" : "", g.lean, limit, g.sag * 100.f,
                    g.steps,
                    glm::length(level(pelvis - g.pelvis0)) * p.m2u, glm::dot(level(com - support), g.dir) * p.m2u,
                    glm::dot(level(com - support), glm::cross(up, g.dir)) * p.m2u);
                staggerGaitLine(g, p, c.num);
            }
            beginFall(s, p, g.dir, 0u, 0.f, false, c.now);
            return;
        }
        // (A parry's: at its time, as the old pose's stagger was, however it stands. Else its carry near over (its pin's
        // anchor easing out: the last of it a glide), not mid-step, steady and slow (vr_ragdoll_active_stagger_settle),
        // or vr_ragdoll_active_stagger_settle_time past its carry however it moves: handed back then, no wait for it to
        // stand still (it froze, floating, a moment at its end).)
        const float settle = za::max(vr_ragdoll_active_stagger_settle.value, 0.f);
        const float settleMost = za::clamp(vr_ragdoll_active_stagger_settle_time.value, 0.f, staggerMost);
        const bool settled = staggerShare(g, t) >= staggerSettleShare && g.stepLeg < 0 && g.lean < staggerSteady &&
                             speed < settle;
        const bool due = g.recoverAt > 0.f ? t >= g.recoverAt : settled || t > g.time + settleMost;
        if(due)
        {
            g.outcome = Outcome::Recovered;
            g.decided = c.now;
            if(vr_debug_ragdoll.value || vr_knockdown_debug.value)
            {
                Con_Printf("stagger: %d recovered at %.2f s: leaning %.0f deg (at most %.0f), its pelvis at least %.0f%% of its "
                           "height, %d steps, moved %.0f units\n",
                    c.num, t, g.lean, g.leanMost, g.sag * 100.f, g.steps, glm::length(level(pelvis - g.pelvis0)) * p.m2u);
                staggerGaitLine(g, p, c.num);
            }
        }
    }
    // Kept upright (as steady as its balance: the shove's pitch of its pelvis worked off), towards its stance.
    if(pose.valid)
    {
        turnWhole(p, c, glm::normalize(s.turn * pose.rot[0]), staggerUpright * (0.5f + 0.5f * g.balance) * s.base,
            staggerUprightMost, 1.f);
    }
    // The arms thrown out to its sides against the shove and wheeling (its balance's), each at its own pace (not a
    // mirror's), flapping a little; the more the less steady it is.
    const glm::vec3 ahead = -g.dir;
    const glm::vec3 left = s.turn * glm::vec3{0.f, 1.f, 0.f};
    for(int arm = 0; arm < 2; arm++)
    {
        Reach& r = s.reach[static_cast<za::SizeT>(arm)];
        if(!limbWhole(s, p, arm))
        {
            r.weight = 0.f;
            continue;
        }
        const int u = s.chain[static_cast<za::SizeT>(arm)][0];
        const float len = limbLength(s, p, arm);
        const glm::vec3 shoulder = worldPoint(p, u, p.rig->bones[u].pivot);
        if(g.tug)
        {
            // (A tug over its weapon: the arm whose hand holds it pulls it in low before its chest, towards its middle; the
            // other hand reaches for the same hand, both on the weapon. No wheeling: it stands its ground.)
            const bool holds = arm == g.tugArm;
            const glm::vec3 held =
                g.tugArm >= 0 && limbWhole(s, p, g.tugArm) ? limbEnd(s, p, g.tugArm) : shoulder + ahead * (0.5f * len);
            r.goal = holds ? shoulder + ahead * (0.3f * len) - up * (0.3f * len) + left * ((arm == 0 ? -0.25f : 0.25f) * len)
                           : held;
            r.weight = holds ? 1.f : 0.7f;
            r.strength = za::clamp(g.tugStrength * (holds ? 1.f : 0.7f), 0.f, 3.f);
            continue;
        }
        const float flail = za::clamp(vr_ragdoll_active_stagger_arms.value, 0.f, 2.f);
        const glm::vec3 side = left * (arm == 0 ? 1.f : -1.f);
        const float a = 2.f * 3.14159265f * staggerWheel * (arm == 0 ? 1.f : 1.27f) * t + (arm == 0 ? 0.f : 2.1f);
        r.goal = shoulder + side * (0.55f * len) + ahead * (0.3f * len) + up * (0.05f * len) +
                 ((ahead * za::cos(a) + up * za::sin(a)) * (0.3f * len) + side * (0.12f * len * za::sin(2.f * a))) * flail;
        r.weight = smooth01(t / 0.1f) * (0.55f + 0.35f * (1.f - g.balance)) * za::min(flail, 1.f);
        r.strength = 0.8f;
    }
    // The feet: planted, or the one stepping lifted along its way.
    for(int leg = 2; leg < 4; leg++)
    {
        Reach& r = s.reach[static_cast<za::SizeT>(leg)];
        const auto i = static_cast<za::SizeT>(leg - 2);
        if(!limbWhole(s, p, leg))
        {
            r.weight = 0.f;
            continue;
        }
        // (A foot not stepping reaches, staggerPlant, to where it was put down: its knee bends as its pin lets its hip
        // down (staggerDip), and the foot holds its place against the pin's carry rather than gliding; the clip's
        // straight legs alone had it on stilts. Out of its leg's reach the reach is a straight leg, as the clip's.)
        r.weight = leg == g.stepLeg ? 1.f : staggerPlant;
        r.strength = 1.f;
        g.planted[i] = leg == g.stepLeg ? g.planted[i] : limbEnd(s, p, leg);
        r.goal = g.footAt[i];
        if(leg != g.stepLeg)
        {
            // (Its foot held where it was put down, its level motion worked off, while it is down and its leg reaches
            // it: the pin's carry swings the leg out over it till it steps (staggerReachStep), no glide. Past its
            // leg's reach, or lifted, it slides as before.)
            const int lo = s.chain[static_cast<za::SizeT>(leg)][1];
            const int up0 = s.chain[static_cast<za::SizeT>(leg)][0];
            const float len = limbLength(s, p, leg);
            const glm::vec3 hip = worldPoint(p, up0, p.rig->bones[up0].pivot);
            const glm::vec3 end = limbEnd(s, p, leg);
            if(glm::length(g.footAt[i] - hip) < staggerHoldReach * len && end.z - g.footAt[i].z < 0.08f * len)
            {
                const glm::vec3 v = glmv(b3Body_GetLinearVelocity(p.body[lo]));
                const float k = 1.f - za::exp(-c.dt / staggerHoldTime);
                b3Body_SetLinearVelocity(p.body[lo], b3v(v - level(v) * k));
            }
        }
        if(leg == g.stepLeg)
        {
            const float len = limbLength(s, p, leg);
            const float u = za::clamp(static_cast<float>(c.now - g.stepFrom) / za::max(g.stepTime, 0.05f), 0.f, 1.f);
            // (Lifted early and set down late: the knee up through the step, the foot planted at its end.)
            const float lift = za::clamp(vr_ragdoll_active_stagger_lift.value, 0.f, 0.6f);
            r.goal = glm::mix(g.stepA, g.stepB, smooth01(u)) + up * (lift * len * za::sin(3.14159265f * za::sqrt(u)));
            if(u >= 1.f)
            {
                g.footAt[i] = g.stepB;
                g.lastLeg = leg;
                g.strideSum += glm::length(level(limbEnd(s, p, leg) - g.stepA));
                g.liftSum += g.stepLift;
                g.liftMost = za::max(g.liftMost, g.stepLift);
                g.stepLift = 0.f;
                g.planted[i] = g.stepB;
                g.stepLeg = -1;
                g.steps++;
            }
        }
    }
}

// ---- Falls ----
// Going over (a stagger past its lean, a shove's knockdown, a throw): the arms reach for the floor ahead the way it goes
// (each hand held where it touches it), the torso and head keep some tone (more for a throw: a curl), the legs give.
// Landed (its chest low, or a while on): a flinch (the torso and head tensed a moment, the arms letting go), then limp.
constexpr float fallReach = 0.12f; // s: the arms out
constexpr float fallLongest = 1.4f; // s: landed by then, whatever

void fallFrame(State& s, const Parts& p, const Context& c, float t)
{
    Fall& f = s.fall;
    const int chest = chestOf(p);
    const glm::vec3 pelvis = glmv(b3Body_GetWorldCenter(p.body[0]));
    const glm::vec3 top = glmv(b3Body_GetWorldCenter(p.body[chest]));
    const float floor = floorAt(c, pelvis, 0.6f, pelvis.z - 1.f);
    if(f.height <= 0.f)
    {
        f.height = za::max(top.z - floor, 0.2f);
    }
    // Its way: its top's level motion, else the shove's.
    const glm::vec3 move = level(glmv(b3Body_GetLinearVelocity(p.body[chest])));
    const glm::vec3 way = glm::length(move) > 0.4f ? glm::normalize(move) : f.dir;
    if(f.landed < 0.f && (top.z - floor < za::max(0.3f, 0.35f * f.height) || t - f.since > fallLongest))
    {
        f.landed = t;
        const float k = fadeAt(s.fade[static_cast<za::SizeT>(Channel::Torso)], t);
        s.fade[static_cast<za::SizeT>(Channel::Torso)] = Fade{za::max(k, 0.7f), 0.f, t, t + 0.9f};
        s.fade[static_cast<za::SizeT>(Channel::Head)] = Fade{0.7f, 0.f, t, t + 1.f};
        s.fade[static_cast<za::SizeT>(Channel::Arms)] = Fade{0.5f, 0.f, t + 0.2f, t + 0.8f};
        s.fade[static_cast<za::SizeT>(Channel::Legs)] =
            Fade{fadeAt(s.fade[static_cast<za::SizeT>(Channel::Legs)], t), 0.f, t, t + 0.3f};
        if(vr_debug_ragdoll.value)
        {
            Con_Printf("fall: %d landed at %.2f s (%.2f s falling), hands down %d %d\n", c.num, t, t - f.since,
                f.put[0] ? 1 : 0, f.put[1] ? 1 : 0);
        }
    }
    // The arms braced: each hand to the floor ahead (its shoulder's way), held where it touches it.
    for(int arm = 0; arm < 2; arm++)
    {
        Reach& r = s.reach[static_cast<za::SizeT>(arm)];
        const auto i = static_cast<za::SizeT>(arm);
        if(!limbWhole(s, p, arm))
        {
            r.weight = 0.f;
            continue;
        }
        // (A hand grabbing another monster (fallEnv): its reach is the grab's.)
        const EnvHand& eh = s.env.hand[i];
        if(eh.mode == Grip::Grab)
        {
            r.goal = eh.goal;
            r.weight = za::min(1.f, r.weight + c.dt * 12.f); // (out to it at once)
            r.strength = eh.strength;
            continue;
        }
        const int u = s.chain[i][0];
        const float len = limbLength(s, p, arm);
        const glm::vec3 shoulder = worldPoint(p, u, p.rig->bones[u].pivot);
        const glm::vec3 hand = limbEnd(s, p, arm);
        if(!f.put[i] && t - f.since > fallReach && hand.z - floor < 0.06f)
        {
            f.put[i] = true;
            f.hand[i] = glm::vec3{hand.x, hand.y, floor + 0.02f};
        }
        glm::vec3 goal = f.hand[i];
        if(!f.put[i])
        {
            goal = shoulder + way * (0.55f * len);
            goal.z = floor + 0.03f;
            // (A wall the way it goes, within the arm's reach of its shoulder: the hand goes to it instead, a little
            // below the shoulder, and is held where it touches it.)
            glm::vec3 at{0.f}, normal{0.f};
            if(c.rayHit && c.rayHit(shoulder, shoulder + way * len, at, normal) < 1.f && za::abs(normal.z) < 0.5f)
            {
                goal = at + normal * 0.03f - glm::vec3{0.f, 0.f, 0.15f * len};
                if(glm::distance(hand, goal) < 0.06f && t - f.since > fallReach)
                {
                    f.put[i] = true;
                    f.hand[i] = goal;
                }
            }
        }
        const float in = smooth01((t - f.since) / fallReach);
        const float out = f.landed < 0.f ? 1.f : 1.f - smooth01((t - f.landed - 0.25f) / 0.4f);
        r.goal = goal;
        r.weight = in * out;
        r.strength = 0.8f;
    }
    if(f.landed >= 0.f && t > f.landed + 1.2f)
    {
        f.on = false;
        for(Reach& r : s.reach)
        {
            r.weight = 0.f;
        }
    }
}

// ---- Get-ups ----
constexpr float riseTurnRate = 9.f; // rad/s per radian its pelvis is turned off the get-up's
constexpr float riseTurnMost = 4.f; // rad/s
constexpr float riseLost = 1.f;     // m: its pelvis this far off the get-up's once pulled in: it lets go
// Pulled into the get-up's first frame where it will stand (its pin's pull and its muscles rising over the
// preparation), then the get-up played by its body, its pelvis pinned to the clip's (risen by the pin, its weight held
// up), held at its last frame: done.
void riseFrame(State& s, const Parts& p, const Context& c, const ClipPose& pose)
{
    Rise& r = s.rise;
    if(c.now < r.rollUntil)
    {
        // Rolling over first, until its front is round (then the pin takes it).
        const glm::vec3 front = bodyRot(p, chestOf(p)) * glm::vec3{1.f, 0.f, 0.f};
        const glm::vec3 pelvis = glmv(b3Body_GetWorldCenter(p.body[0]));
        if(glm::dot(front, r.front) > 0.7f)
        {
            r.rollUntil = c.now;
        }
        else
        {
            rollOver(p, c, r.rollAxis, r.rollSign, pelvis, floorAt(c, pelvis, 0.6f, pelvis.z - 0.2f));
            holdLevel(p, c, r.lay, vr_ragdoll_active_getup_hold.value);
        }
    }
    // Rolled over: the whole body turned as one towards the get-up's turn of its pelvis (its parts' spin and their motion
    // about the pelvis eased towards a turn at riseTurnRate per radian off, at most riseTurnMost: the pin's own turn is
    // the pelvis's alone, too weak for the body on it, and the floor holds its feet).
    if(c.now >= r.rollUntil)
    {
        const float k = za::clamp(fadeAt(s.pin.fade, static_cast<float>(c.now - s.begun)), 0.f, 1.f);
        turnWhole(p, c, glm::normalize(s.turn * pose.rot[0]), riseTurnRate, riseTurnMost, k);
    }
    // (Each part's place against the clip's: its body's origin is its bone's transform's, as the pin's anchor.)
    const auto off = [&](int b) {
        return glmv(b3Body_GetPosition(p.body[b])) - (s.origin + s.turn * (pose.pos[static_cast<za::SizeT>(b)] * p.k));
    };
    r.error = glm::length(off(0));
    {
        const glm::vec3 at = massCentre(p);
        r.path[c.now < r.rollUntil ? 0 : c.now < s.clip.t0 ? 1 : 2] += glm::length(level(at - r.last));
        r.most = za::max(r.most, glm::length(level(at - r.lay)));
        r.last = at;
    }
    r.errorMost = c.now > r.end - 0.3 ? za::max(r.errorMost, r.error) : r.errorMost; // (as it stands up)
    // (Pulled in and still far off its get-up (caught on something, knocked away): it lets go, and tries again later.)
    if(c.now > s.clip.t0 && r.error > riseLost)
    {
        if(vr_debug_ragdoll.value || vr_knockdown_debug.value)
        {
            Con_Printf("getup: %d lost its get-up (its pelvis %.0f units off): it slumps\n", c.num, r.error * p.m2u);
        }
        lapse(s, p, c.now);
        return;
    }
    if(vr_debug_ragdoll.value >= 2.f && static_cast<int>(c.now * 72.0 + 0.5) % 7 == 0)
    {
        const glm::vec3 e = off(0) * p.m2u;
        const glm::vec3 a = glm::degrees(
            ragpose::rotationVector(glm::normalize(s.turn * pose.rot[0]) * glm::inverse(bodyRot(p, 0))));
        const glm::vec3 moved = (r.last - r.lay) * p.m2u;
        Con_Printf("getup: %d %.2f s %s frame %.1f: its pelvis off %.1f %.1f %.1f units, turned off %.0f %.0f %.0f deg; its "
                   "mass centre %.1f %.1f from where it lay\n",
            c.num, static_cast<float>(c.now - s.begun), c.now < r.rollUntil ? "rolling" : c.now < s.clip.t0 ? "pulled in" : "rising",
            pose.framePos, e.x, e.y, e.z, a.x, a.y, a.z, moved.x, moved.y);
    }
    if(!r.done && c.now >= r.end + 0.12)
    {
        r.done = true;
        if(vr_debug_ragdoll.value || vr_knockdown_debug.value)
        {
            int worst = 0;
            float most = 0.f;
            for(int b = 0; b < p.count; b++)
            {
                const float e = cut(p, b) || p.rig->bones[b].joint == ragdoll::Joint::Loose ? 0.f : glm::length(off(b));
                worst = e > most ? b : worst;
                most = za::max(most, e);
            }
            const glm::vec3 pe = off(0) * p.m2u;
            const auto turned = [&](int b) {
                const glm::quat want = glm::normalize(s.turn * pose.rot[static_cast<za::SizeT>(b)]);
                return glm::degrees(2.f * za::acos(za::clamp(za::abs(glm::dot(want, bodyRot(p, b))), 0.f, 1.f)));
            };
            float turnMost = 0.f;
            int turnWorst = 0;
            for(int b = 0; b < p.count; b++)
            {
                const float e = cut(p, b) || p.rig->bones[b].joint == ragdoll::Joint::Loose ? 0.f : turned(b);
                turnWorst = e > turnMost ? b : turnWorst;
                turnMost = za::max(turnMost, e);
            }
            const glm::vec3 axis = glm::degrees(
                ragpose::rotationVector(glm::normalize(s.turn * pose.rot[0]) * glm::inverse(bodyRot(p, 0))));
            Con_Printf("getup: %d risen at %.2f s: its pelvis %.1f units from the get-up's (%.1f up; at most %.1f as it stood "
                       "up), turned %.0f deg off (%.0f %.0f %.0f); its parts at most %.1f units (%s), %.0f deg (%s)\n",
                c.num, static_cast<float>(c.now - s.begun), r.error * p.m2u, pe.z, r.errorMost * p.m2u, turned(0), axis.x,
                axis.y, axis.z, most * p.m2u, p.rig->bones[worst].name, turnMost, p.rig->bones[turnWorst].name);
            Con_Printf("getup: %d level travel: its mass centre went %.1f units rolling, %.1f pulled in, %.1f rising; ends %.1f "
                       "from where it lay (at most %.1f); the clip's own %.1f\n",
                c.num, r.path[0] * p.m2u, r.path[1] * p.m2u, r.path[2] * p.m2u, glm::length(level(r.last - r.lay)) * p.m2u,
                r.most * p.m2u, r.clipOwn * p.m2u);
        }
    }
}

void trace(const State& s, const Parts& p, const Context& c, float t)
{
    za::Array<float, channelCount> err{}, str{};
    za::Array<int, channelCount> n{};
    float ke = 0.f, spin = 0.f;
    for(int b = 0; b < p.count; b++)
    {
        if(cut(p, b))
        {
            continue;
        }
        const b3BodyId body = p.body[b];
        const glm::vec3 v = glmv(b3Body_GetLinearVelocity(body)), w = glmv(b3Body_GetAngularVelocity(body));
        const b3Matrix3 i = b3Body_GetLocalRotationalInertia(body);
        const glm::vec3 wl = glm::inverse(bodyRot(p, b)) * w;
        ke += 0.5f * b3Body_GetMass(body) * glm::dot(v, v) +
              0.5f * (i.cx.x * wl.x * wl.x + i.cy.y * wl.y * wl.y + i.cz.z * wl.z * wl.z);
        spin = za::max(spin, glm::length(w));
        if(!drivable(s, p, b))
        {
            continue;
        }
        const auto ch = static_cast<za::SizeT>(channelOf(p.rig->bones[b]));
        const glm::quat actual = glm::inverse(bodyRot(p, p.rig->bones[b].parent)) * bodyRot(p, b);
        err[ch] += glm::degrees(glm::length(ragpose::rotationVector(actual * glm::inverse(s.animTarget[static_cast<za::SizeT>(b)]))));
        str[ch] += s.strength[static_cast<za::SizeT>(b)];
        n[ch]++;
    }
    for(int ch = 0; ch < channelCount; ch++)
    {
        const auto i = static_cast<za::SizeT>(ch);
        err[i] /= static_cast<float>(za::max(n[i], 1));
        str[i] /= static_cast<float>(za::max(n[i], 1));
    }
    const glm::vec3 pelvis = glmv(b3Body_GetWorldCenter(p.body[0])) * p.m2u;
    const float pin = B3_IS_NON_NULL(s.pin.anchor) ? fadeAt(s.pin.fade, t) * s.pin.force : 0.f;
    const Death& d = s.death;
    // (Short: the console wraps long lines, and the tests read them whole.)
    Con_Printf("ragtrace %d %.3f p %.0f %.0f %.1f e %.0f %.0f %.0f %.0f s %.2f %.2f %.2f %.2f ke %.0f w %.1f pin %.2f f %d%d%d %.0f\n",
        c.num, t, pelvis.x, pelvis.y, pelvis.z, err[0], err[1], err[2], err[3], str[0], str[1], str[2], str[3], ke, spin,
        pin, d.stepLeg >= 0 ? 1 : 0, d.woundArm >= 0 && s.reach[static_cast<za::SizeT>(d.woundArm)].weight > 0.f ? 1 : 0,
        d.crawl && (d.crawlFrom >= 0.0 || d.rollFrom >= 0.0) ? 1 : 0, d.crawled * p.m2u);
}

// ---- Environment (vr_ragdoll_active_env; vr_ragactive.hpp, "What is round him") ----
// Sense: every envEvery frames a ray or two from the chest (along his fall; behind him, to his left, to his right in
// turn) against the world, the movers and the props, and while he falls the world's ledges near his hands. A goal per
// hand: catch (a surface he falls onto soon, ahead of him or beside him), lean or brace (a wall or a prop behind or
// beside him as he dies), ledge (a lip he falls past), curl (tumbling, nothing near). Act: the hand's reach (two-bone)
// over the clip's pose and the death's own reaches, its strength; a hold's capped motor joint from the world's (or a
// mover's) body to the hand; a weak pull of the pelvis to the wall he leans on.

constexpr int envEvery = 3;         // frames between a body's probes
constexpr float catchAhead = 0.4f;  // s: a surface he falls onto within this is reached for
constexpr float catchGive = 0.35f;  // s: a landed hand's push gives over this
constexpr float leanWithin = 1.f;   // s from his death: a wall found later is no lean (he is down)
constexpr float leanPull = 1.5f;    // s: the pelvis drawn to the wall he leans on, fading over this
constexpr float leanSlide = 0.45f;  // m/s: a lean's hold slides down at most this fast
constexpr float slideTime = 1.8f;   // s: his body's slide down the wall he leans on (vr_ragdoll_active_env_slide) fades over this
constexpr float slideSpeed = 0.5f;  // m/s: his pelvis and chest sink down it at most this fast (while it holds)
constexpr float slideGive = 0.08f;  // s: the slide's brake takes up a faster sinking over about this
constexpr float slideNear = 0.7f;   // (of its arm's length) his chest this near the wall's plane: against it (he slides)
constexpr float leanAway = 2.5f;    // m/s: blown away from the wall faster than this as he dies: no lean (slower: a body
                                    // slammed into it bouncing off, or pushed out of it, still leans)
constexpr float reachTime = 1.f;    // s: a hand not on its surface by then gives up
constexpr float handOff = 1.5f;     // units: a hand's goal off its surface (its thickness)
constexpr float smallThing = 8.f;   // kg: a loose thing lighter is nothing to brace on,
constexpr float smallHeight = 0.4f; // m: or lower (a backpack, a dropped gun)
constexpr float ledgeReach = 1.15f; // (of its arm's length) a lip this near a shoulder is grabbed (the hold pulls the hand the
                                    // rest of the way; a death knight toppling off falls with its shoulders 31-34 units off
                                    // the lip, out of its reach of 27: 1.4 had it reach in vain)
// A living one's climb ("Climbs", below).
constexpr float climbGrip = 2.f;    // its hands' holds take this times a dying one's

// The ledges near a falling body (findLedge's list).
za::Vector<int> envNearby;

[[nodiscard]] const char* gripName(Grip g)
{
    switch(g)
    {
        case Grip::Catch: return "catch";
        case Grip::Brace: return "brace";
        case Grip::Lean: return "lean";
        case Grip::Ledge: return "ledge";
        case Grip::Curl: return "curl";
        case Grip::Grab: return "grab";
        default: return "none";
    }
}

} // namespace

namespace detail
{

[[nodiscard]] const char* armName(int arm)
{
    return arm == 0 ? "left" : "right";
}

} // namespace detail

namespace
{

// (ragenv's: a level unit direction; ragstagger's level() is the level part alone)
[[nodiscard]] glm::vec3 levelDir(const glm::vec3& v)
{
    const glm::vec2 h{v.x, v.y};
    const float n = glm::length(h);
    return n > 1e-4f ? glm::vec3{h / n, 0.f} : glm::vec3{0.f};
}

// A probe's hit: the point, the surface's normal, its body (a loose one: a prop).
struct EnvRay
{
    bool hit{false};
    glm::vec3 at{0.f}, n{0.f};
    b3BodyId body{b3_nullBodyId};
    bool loose{false};
    float dist{0.f};
    int num{0}; // its entity
};

EnvRay envCast(Env& e, const Parts& p, const Context& c, const glm::vec3& from, const glm::vec3& delta)
{
    EnvRay r;
    b3QueryFilter filter = b3DefaultQueryFilter();
    filter.maskBits = c.envMask;
    const b3RayResult res = b3World_CastRayClosest(c.world, b3v(from), b3v(delta), filter);
    if(res.hit)
    {
        r.body = b3Shape_GetBody(res.shapeId);
        r.loose = b3Body_GetType(r.body) == b3_dynamicBody;
        r.num = static_cast<int>(reinterpret_cast<intptr_t>(b3Shape_GetUserData(res.shapeId))); // (vr_box3d.cpp userOf)
        const b3AABB box = b3Shape_GetAABB(res.shapeId);
        r.hit = !r.loose || (b3Body_GetMass(r.body) >= smallThing && static_cast<float>(box.upperBound.z - box.lowerBound.z) >= smallHeight);
        r.hit = r.hit && r.num != c.num; // (not his own)
        r.at = glmv(res.point);
        r.n = glmv(res.normal);
        r.dist = glm::length(delta) * res.fraction;
    }
    if(e.probes < static_cast<int>(e.probe.size()))
    {
        e.probe[static_cast<za::SizeT>(e.probes++)] = EnvProbe{from, r.hit ? r.at : from + delta, r.hit};
    }
    if(vr_debug_ragdoll.value >= 2.f)
    {
        const glm::vec3 a = from * p.m2u, dd = delta * p.m2u;
        Con_Printf("ragenv: %d probe from %.0f %.0f %.0f by %.0f %.0f %.0f: %s\n", c.num, a.x, a.y, a.z, dd.x, dd.y, dd.z,
            res.hit ? va("%s %.0f units on, normal %.2f %.2f %.2f", r.loose ? (r.hit ? "a prop" : "a small thing") : "met",
                         r.dist * p.m2u, r.n.x, r.n.y, r.n.z)
                    : "nothing");
    }
    return r;
}

} // namespace

namespace detail
{

// A hand's hold gone (its joint destroyed), the hand free; `c` null: quietly (the ragdoll's end).
void letGo(State& s, int arm, const Context* c, const char* why)
{
    EnvHand& h = s.env.hand[static_cast<za::SizeT>(arm)];
    const bool held = B3_IS_NON_NULL(h.joint);
    if(held && b3Joint_IsValid(h.joint))
    {
        b3DestroyJoint(h.joint, true);
    }
    if(c && vr_debug_ragdoll.value && h.mode != Grip::None)
    {
        Con_Printf("ragenv: %d %s hand lets go (%s, %s) after %.2f s%s\n", c->num, armName(arm), gripName(h.mode), why,
            c->now - h.since,
            held ? va(", held %.2f s, load %.0f of %.0f N, slid %.2f m", c->now - h.touched, h.load, h.cap, h.slid) : "");
    }
    h.joint = b3_nullJointId;
    h.on = b3_nullBodyId;
    h.mode = Grip::None;
    h.want = 0.f;
    h.touched = -1.0;
    h.overSince = -1.0;
    h.struck = false;
}

// The hand of `arm` held to its body `on` at `at` (world, metres): a ledge's a ball joint, a lean's a motor joint pulling
// the hand's end there with at most `cap` N; the hand turns freely.
void holdOn(State& s, const Parts& p, const Context& c, int arm, const glm::vec3& at, float cap)
{
    EnvHand& h = s.env.hand[static_cast<za::SizeT>(arm)];
    const int lo = s.chain[static_cast<za::SizeT>(arm)][1];
    if(B3_IS_NULL(h.on) || !b3Body_IsValid(h.on))
    {
        return;
    }
    const b3Transform frameA{b3Body_GetLocalPoint(h.on, b3v(at)), b3Quat_identity};
    const b3Transform frameB{b3v(p.rig->bones[lo].end * p.k), b3Quat_identity};
    if(h.mode == Grip::Ledge)
    {
        // Hanging: a ball joint (no give: he hangs from it; let go when its load stays past the cap). The hand passes
        // into the lip a little rather than fight it.
        b3SphericalJointDef jd = b3DefaultSphericalJointDef();
        jd.base.bodyIdA = h.on;
        jd.base.bodyIdB = p.body[lo];
        jd.base.localFrameA = frameA;
        jd.base.localFrameB = frameB;
        jd.base.collideConnected = false;
        h.joint = b3CreateSphericalJoint(c.world, &jd);
    }
    else
    {
        // Leaning: a motor joint pulling the hand there with at most the cap (the hand slides when he weighs more on it),
        // colliding with the wall (it rests on it).
        b3MotorJointDef jd = b3DefaultMotorJointDef();
        jd.base.bodyIdA = h.on;
        jd.base.bodyIdB = p.body[lo];
        jd.base.localFrameA = frameA;
        jd.base.localFrameB = frameB;
        jd.base.collideConnected = true;
        jd.linearVelocity = b3Vec3_zero;
        jd.angularVelocity = b3Vec3_zero;
        jd.maxVelocityForce = 0.f;
        jd.maxVelocityTorque = 0.f;
        jd.linearHertz = 15.f; // (the hand is light: a soft spring on it would barely pull)
        jd.linearDampingRatio = 1.f;
        jd.angularHertz = 0.f;
        jd.angularDampingRatio = 0.f;
        jd.maxSpringForce = cap;
        jd.maxSpringTorque = 0.f;
        h.joint = b3CreateMotorJoint(c.world, &jd);
    }
    h.cap = cap;
    h.touched = c.now;
    h.goal = at;
    s.env.holds++;
    if(vr_debug_ragdoll.value)
    {
        Con_Printf("ragenv: %d %s hand holds on (%s) at %.0f %.0f %.0f, at most %.0f N (%.2f body weights) for %.2f s\n",
            c.num, armName(arm), gripName(h.mode), at.x * p.m2u, at.y * p.m2u, at.z * p.m2u, cap,
            cap / za::max(s.totalMass * c.gravity, 1.f), h.holdUntil - c.now);
    }
}

} // namespace detail

namespace
{

// A hand's goal on a surface (point `at`, normal `n`) from its shoulder: the nearest point of the surface's plane, led
// by `lead` (metres) along it, moved `side` metres along it towards its own side and `down` metres down it; within
// `most` metres of the shoulder (beyond, towards it); off the surface by the hand's thickness.
[[nodiscard]] glm::vec3 surfaceGoal(const Parts& p, const glm::vec3& shoulder, const glm::vec3& at, const glm::vec3& n,
    const glm::vec3& lead, const glm::vec3& side, float most)
{
    glm::vec3 g = shoulder - n * glm::dot(shoulder - at, n);
    g += lead - n * glm::dot(lead, n);
    g += side - n * glm::dot(side, n);
    const glm::vec3 off = g - shoulder;
    const float len = glm::length(off);
    if(len > most && len > 1e-4f)
    {
        g = shoulder + off * (most / len);
    }
    return g + n * (handOff / p.m2u);
}

[[nodiscard]] float armLength(const State& s, const Parts& p)
{
    float sum = 0.f;
    int n = 0;
    for(int arm = 0; arm < 2; arm++)
    {
        if(limbWhole(s, p, arm))
        {
            sum += limbLength(s, p, arm);
            n++;
        }
    }
    return n ? sum / static_cast<float>(n) : 0.f;
}

} // namespace

namespace detail
{

[[nodiscard]] glm::vec3 shoulderOf(const State& s, const Parts& p, int arm)
{
    const int u = s.chain[static_cast<za::SizeT>(arm)][0];
    return worldPoint(p, u, p.rig->bones[u].pivot);
}

} // namespace detail

namespace
{

// The most a hand's hold takes: Grip body weights (a lean's a third), less for a heavy monster (an ogre slips sooner).
[[nodiscard]] float holdCap(const State& s, const Context& c, Grip g)
{
    const float heavy = za::clamp(za::sqrt(90.f / za::max(s.totalMass, 1.f)), 0.6f, 1.2f);
    const float alive = s.env.living ? climbGrip : 1.f; // (a living one climbing holds on harder)
    return za::max(vr_ragdoll_active_env_grip.value, 0.f) * (g == Grip::Ledge ? 1.f : 1.f / 3.f) * heavy * alive *
           s.totalMass * c.gravity;
}

// A surface he falls onto (ray `r` along his fall): the hands on its side out to it.
void startCatch(State& s, const Parts& p, const Context& c, const EnvRay& r, const glm::vec3& v, const glm::vec3& chestAt,
    const glm::vec3& left, float L, float speed)
{
    Env& e = s.env;
    // (A floor: both hands; a wall well to one side: that side's.)
    const float side = za::abs(r.n.z) >= 0.6f ? 0.f : glm::dot(r.at - chestAt, left);
    int used = 0;
    for(int arm = 0; arm < 2; arm++)
    {
        EnvHand& h = e.hand[static_cast<za::SizeT>(arm)];
        const float sign = arm == 0 ? 1.f : -1.f;
        const bool busy = h.mode == Grip::Lean || h.mode == Grip::Ledge || h.mode == Grip::Brace || h.mode == Grip::Grab;
        if(!limbWhole(s, p, arm) || busy || (za::abs(side) > 0.35f * L && side * sign < 0.f))
        {
            continue;
        }
        const glm::vec3 shoulder = shoulderOf(s, p, arm);
        const glm::vec3 lead = v * 0.12f;
        const glm::vec3 out = left * (sign * 0.12f * L);
        h.mode = Grip::Catch;
        h.goal = surfaceGoal(p, shoulder, r.at, r.n, lead, out, 0.95f * L);
        h.normal = r.n;
        h.since = c.now;
        h.touched = -1.0;
        h.want = 1.f;
        h.strength = 0.9f;
        used |= 1 << arm;
    }
    if(!used)
    {
        return;
    }
    e.catches++;
    e.nextCatch = c.now + 0.6;
    if(vr_debug_ragdoll.value)
    {
        Con_Printf("ragenv: %d catches %s %.0f units off along his fall (%.2f s at %.1f m/s): %s\n", c.num,
            r.loose ? "a prop" : (za::abs(r.n.z) < 0.6f ? "a wall" : "the floor"), r.dist * p.m2u, r.dist / za::max(speed, 0.1f),
            speed, used == 3 ? "both hands" : armName(used == 1 ? 0 : 1));
    }
}

// A wall (or a prop) round him as he dies (`which`: 0 behind, 1 on his left, 2 on his right, 3 his fall into it, 4
// before him): he leans on it, a hand braced on it (not always), his knees buckling slowly.
void startLean(State& s, const Parts& p, const Context& c, const EnvRay& r, int which, float L, float t)
{
    Env& e = s.env;
    const Death& d = s.death;
    e.leaned = true;
    // Blown away from it (the kill's push): no lean (he would hang back towards a wall going away from him).
    const float away = glm::dot(glmv(b3Body_GetLinearVelocity(p.body[0])), levelDir(r.n));
    if(away > leanAway)
    {
        if(vr_debug_ragdoll.value)
        {
            Con_Printf("ragenv: %d no lean: blown away from the wall at %.1f m/s\n", c.num, away);
        }
        return;
    }
    e.lean = !r.loose; // (a prop: braced on, not drawn to)
    e.leanFrom = c.now;
    e.wallAt = r.at;
    e.wallN = r.n;
    // The knees give slowly (they hold some of him as he slides).
    Fade& legs = s.fade[static_cast<za::SizeT>(Channel::Legs)];
    legs = Fade{za::max(fadeAt(legs, t), 0.5f), 0.f, t, t + 1.6f};
    // The hand on the wall's side (the shoulder nearer it; square to it, either); not the one on his wound.
    const float dl = glm::dot(shoulderOf(s, p, 0) - r.at, r.n), dr = glm::dot(shoulderOf(s, p, 1) - r.at, r.n);
    int arm = za::abs(dl - dr) > 0.02f ? (dl < dr ? 0 : 1) : (roll(e.seed, 22u) < 0.5f ? 0 : 1);
    if(arm == d.woundArm || !limbWhole(s, p, arm))
    {
        arm = 1 - arm;
    }
    const bool hand = roll(e.seed, 21u) >= 0.1f && limbWhole(s, p, arm) && arm != d.woundArm;
    // His back to it (behind him, or thrown back into it): often both hands go to it, one each side.
    const int other = 1 - arm;
    const bool both = hand && (which == 0 || which == 3) && !r.loose && roll(e.seed, 23u) < 0.6f && limbWhole(s, p, other) &&
                      other != d.woundArm && B3_IS_NULL(e.hand[static_cast<za::SizeT>(other)].joint) &&
                      (e.hand[static_cast<za::SizeT>(other)].mode == Grip::None || e.hand[static_cast<za::SizeT>(other)].mode == Grip::Curl);
    for(int k = 0; k < (both ? 2 : hand ? 1 : 0); k++)
    {
        const int a = k == 0 ? arm : other;
        EnvHand& h = e.hand[static_cast<za::SizeT>(a)];
        const glm::vec3 shoulder = shoulderOf(s, p, a);
        glm::vec3 sideDir = shoulder - glmv(b3Body_GetWorldCenter(p.body[chestOf(p)]));
        sideDir -= r.n * glm::dot(sideDir, r.n);
        sideDir = glm::length(sideDir) > 1e-4f ? glm::normalize(sideDir) : glm::vec3{0.f};
        h.mode = r.loose ? Grip::Brace : Grip::Lean;
        // (His back to it: the palm out to his side and lower, beside him on the wall, where it is seen; else at the
        // shoulder's height, a little to its side.)
        const bool back = which == 0 || which == 3;
        h.goal = surfaceGoal(p, shoulder, r.at, r.n, glm::vec3{0.f, 0.f, (back ? -0.25f : -0.05f) * L},
            sideDir * ((back ? 0.6f : 0.3f) * L), 0.95f * L);
        h.normal = r.n;
        h.on = r.body;
        h.since = c.now;
        h.touched = -1.0;
        h.want = 1.f;
        h.strength = 0.85f;
    }
    if(vr_debug_ragdoll.value)
    {
        Con_Printf("ragenv: %d leans on %s %s, %.0f units off: %s\n", c.num,
            r.loose ? va("prop %d (%.0f kg, at %.0f %.0f %.0f)", r.num, b3Body_GetMass(r.body), r.at.x * p.m2u, r.at.y * p.m2u,
                          r.at.z * p.m2u)
                    : "the wall",
            which == 0 ? "behind him" : which <= 2 ? "beside him" : which == 3 ? "he falls into" : "in front of him",
            r.dist * p.m2u, both ? "both hands on it" : hand ? va("the %s hand on it", armName(arm)) : "no hand");
    }
}

// A ledge's lip near a hand as he falls past it: that hand (and the other, if it reaches the same lip) to its hold. The
// edge chosen (its index in the map; -1 none).
int findLedge(State& s, const Parts& p, const Context& c, const glm::vec3& pelvis, const glm::vec3& chestAt, float L)
{
    Env& e = s.env;
    const ledges::Map& map = *c.ledges;
    const float u = p.m2u;
    const glm::vec3 lo = (glm::min(pelvis, chestAt) - glm::vec3{1.2f * L}) * u;
    const glm::vec3 hi = (glm::max(pelvis, chestAt) + glm::vec3{1.2f * L}) * u;
    envNearby.clear();
    map.nearby(lo, hi, envNearby);
    int bestArm = -1, bestEdge = -1;
    float best = 1e30f, bestT = 0.f;
    for(const int i : envNearby)
    {
        const ledges::Edge& E = map.edges[static_cast<za::SizeT>(i)];
        for(int arm = 0; arm < 2; arm++)
        {
            const EnvHand& h = e.hand[static_cast<za::SizeT>(arm)];
            if(!limbWhole(s, p, arm) || B3_IS_NON_NULL(h.joint))
            {
                continue;
            }
            const glm::vec3 su = shoulderOf(s, p, arm) * u, hu = limbEnd(s, p, arm) * u;
            const float tt = za::clamp(glm::dot(hu - E.a, E.dir), 0.f, E.len);
            const glm::vec3 q = E.point(tt);
            if(vr_debug_ragdoll.value >= 3.f)
            {
                Con_Printf("ragenv: %d ledge %d lip %.0f %.0f %.0f out %.2f %.2f: %s hand %.0f %.0f %.0f, shoulder %.0f %.0f %.0f "
                           "(%.0f off, reach %.0f), out over it %.2f\n", c.num, i, q.x, q.y, q.z, E.out.x, E.out.y, armName(arm),
                    hu.x, hu.y, hu.z, su.x, su.y, su.z, glm::distance(q, su), ledgeReach * L * u, glm::dot(levelDir(chestAt * u - q), E.out));
            }
            // (The hand not far over the lip, the shoulder fallen near it (not standing at the edge), the lip within the
            // arm's reach stretched (the hold pulls the hand the rest of the way), him out over its drop.)
            if(q.z < hu.z - 0.5f * L * u || q.z < su.z - 0.8f * L * u || q.z > su.z + 0.6f * L * u ||
                glm::distance(q, su) > ledgeReach * L * u || glm::dot(levelDir(chestAt * u - q), E.out) < 0.3f)
            {
                continue;
            }
            const float dd = glm::distance(q, hu);
            if(dd < best)
            {
                best = dd;
                bestArm = arm;
                bestEdge = i;
                bestT = tt;
            }
        }
    }
    if(bestArm < 0)
    {
        return -1;
    }
    const ledges::Edge& E = map.edges[static_cast<za::SizeT>(bestEdge)];
    const float hold = 0.45f + 1.2f * roll(e.seed, 31u);
    const float hang = za::max(vr_ragdoll_active_env_hang.value, 0.1f) * hold;
    for(const int arm : {bestArm, 1 - bestArm})
    {
        EnvHand& h = e.hand[static_cast<za::SizeT>(arm)];
        if(!limbWhole(s, p, arm) || B3_IS_NON_NULL(h.joint))
        {
            continue;
        }
        // (The other hand: the same lip, a shoulder's width along it towards its own side.)
        float tt = bestT;
        if(arm != bestArm)
        {
            const glm::vec3 su = shoulderOf(s, p, arm) * u;
            tt = za::clamp(glm::dot(su - E.a, E.dir), 0.f, E.len);
            if(glm::distance(E.point(tt), su) > ledgeReach * L * u)
            {
                continue;
            }
        }
        const glm::vec3 q = E.point(tt) - E.out * ledges::holdInset + glm::vec3{0.f, 0.f, ledges::holdLift};
        // The body under the hold: the world's, or a mover's over it.
        const EnvRay top = envCast(e, p, c, q / u + glm::vec3{0.f, 0.f, 4.f / u}, glm::vec3{0.f, 0.f, -10.f / u});
        if(!top.hit || top.loose || top.n.z < 0.6f)
        {
            continue;
        }
        h.mode = Grip::Ledge;
        h.goal = q / u + glm::vec3{0.f, 0.f, handOff / u};
        h.normal = glm::vec3{0.f, 0.f, 1.f};
        h.on = top.body;
        h.since = c.now;
        h.touched = -1.0;
        h.want = 1.f;
        h.strength = 1.f;
        h.holdUntil = c.now + hang; // (from the grab: holdOn keeps it)
        e.hung = true;
        if(vr_debug_ragdoll.value)
        {
            Con_Printf("ragenv: %d %s hand reaches for a ledge's lip at %.0f %.0f %.0f (%.0f units from it), to hang %.2f s\n",
                c.num, armName(arm), q.x, q.y, q.z, glm::distance(q, limbEnd(s, p, arm) * u), hang);
        }
    }
    return e.hung ? bestEdge : -1;
}

// A hand on its way to a hold (Lean, Ledge) or holding: grabbing once there, its upkeep (slid down as he weakens, let
// go on its load, timer or at the floor).
void keepHold(State& s, const Parts& p, const Context& c, int arm, float L)
{
    EnvHand& h = s.env.hand[static_cast<za::SizeT>(arm)];
    const glm::vec3 hand = limbEnd(s, p, arm);
    const bool ledge = h.mode == Grip::Ledge;
    if(B3_IS_NULL(h.joint))
    {
        // Its target gone on the way (a mover or fixture destroyed since): it lets go, or it would reach for ever.
        if(B3_IS_NULL(h.on) || !b3Body_IsValid(h.on))
        {
            letGo(s, arm, &c, "its hold gone");
            return;
        }
        // On its way: grabs once near the goal, or near the surface near it (the hold pulls it the rest of the way). A
        // lean's hand that hasn't got there (an arm can't reach far behind him) goes straight to the wall from where
        // it is.
        const float off = glm::dot(hand - h.goal, h.normal);
        const glm::vec3 along = (hand - h.goal) - h.normal * off;
        if(!ledge && c.now - h.since > 0.15)
        {
            h.goal = hand - h.normal * off;
        }
        // (A ledge's lip falling past: grabbed farther off the faster it goes, a quarter of an arm per m/s, its hold pulling the hand in.
        // A wall's: a little farther off than it was, the hold slapping the hand onto it; he is falling meanwhile.)
        const float within = ledge ? za::clamp(0.25f * glm::length(glmv(b3Body_GetLinearVelocity(p.body[0]))), 0.25f, 1.f) : 0.35f;
        if(glm::length(hand - h.goal) < within * L || (off < 0.3f * L && glm::length(along) < 0.6f * L))
        {
            const glm::vec3 at = hand - h.normal * off; // (on the goal's plane: already handOff off the surface)
            if(!ledge)
            {
                h.holdUntil = c.now + 1.1 + 0.9 * static_cast<double>(roll(s.env.seed, 41u + static_cast<uint32_t>(arm)));
            }
            else
            {
                h.holdUntil += c.now - h.since; // (the hang counted from the grab)
            }
            holdOn(s, p, c, arm, at, holdCap(s, c, h.mode));
        }
        else if(c.now - h.since > reachTime)
        {
            letGo(s, arm, &c, va("out of reach: %.0f units off it, %.0f from the surface", glm::length(hand - h.goal) * p.m2u,
                                  off * p.m2u));
        }
        return;
    }
    if(!b3Joint_IsValid(h.joint) || !b3Body_IsValid(h.on))
    {
        letGo(s, arm, &c, "its hold gone");
        return;
    }
    h.load = glm::length(glmv(b3Joint_GetConstraintForce(h.joint)));
    h.overSince = h.load >= 0.95f * h.cap ? (h.overSince < 0.0 ? c.now : h.overSince) : -1.0;
    b3Transform fa = b3Joint_GetLocalFrameA(h.joint);
    if(!ledge)
    {
        // Sliding down as he weakens: faster as the hold goes on.
        const float speed = leanSlide * smooth01(static_cast<float>(c.now - h.touched) / 1.2f);
        const float step = speed * c.dt;
        if(step > 0.f)
        {
            const glm::vec3 down = glm::inverse(fromB3(b3Body_GetRotation(h.on))) * glm::vec3{0.f, 0.f, -step};
            fa.p = b3v(glmv(fa.p) + down);
            b3Joint_SetLocalFrameA(h.joint, fa);
            h.slid += step;
        }
    }
    h.goal = glmv(b3Body_GetWorldPoint(h.on, fa.p));
    // (A climber pulling itself up: its arms give, folding as its pin carries it up (stiff, they would hold it down).)
    const bool pulling = s.climb.on && s.climb.phase != ClimbPhase::Hang;
    h.strength = ledge ? (pulling ? climbArms : 0.9f) : 0.75f - 0.35f * smooth01(static_cast<float>(c.now - h.touched) / 1.5f);
    const float floor = floorAt(c, h.goal, L, h.goal.z - 2.f * L);
    if(h.overSince >= 0.0 && c.now - h.overSince > slipTime)
    {
        letGo(s, arm, &c, "its load");
    }
    else if(c.now > h.holdUntil)
    {
        letGo(s, arm, &c, "its time");
    }
    else if(!ledge && h.goal.z - floor < 0.3f * L)
    {
        letGo(s, arm, &c, "the floor");
    }
    else if(glm::distance(hand, h.goal) > 0.6f * L)
    {
        letGo(s, arm, &c, va("pulled away: the hand %.0f units off its hold", glm::distance(hand, h.goal) * p.m2u));
    }
}

// ---- Guarding the head (vr_ragdoll_active_guard; vr_ragactive.hpp, Guard) ----
// A living one (staggering, falling, down, getting up) a hand swings at (QC VR_Guard_Frame): each arm reaches up before
// its face, a little towards the blow and to its own side, the forearms across it, over what leads its arms (a
// stagger's wheeling, a fall's bracing, a get-up's frames), eased in over guardIn, held, eased out over guardOut.
constexpr float guardIn = 0.12f;  // s
constexpr float guardOut = 0.25f; // s

void guardFrame(State& s, const Parts& p, const Context& c)
{
    Guard& g = s.guard;
    const int head = p.rig->head;
    if(!g.on)
    {
        return;
    }
    if(c.now > g.until + guardOut || head < 0 || cut(p, head) || s.death.on || s.climb.on)
    {
        g.on = false;
        for(int arm = 0; arm < 2; arm++)
        {
            s.reach[static_cast<za::SizeT>(arm)].weight = 0.f;
        }
        return;
    }
    const float w = smooth01(static_cast<float>(c.now - g.from) / guardIn) *
                    (1.f - smooth01(static_cast<float>(c.now - g.until) / guardOut));
    const float L = armLength(s, p);
    const glm::vec3 at = glmv(b3Body_GetWorldCenter(p.body[head]));
    const int chest = chestOf(p);
    const glm::vec3 left = bodyRot(p, chest) * glm::vec3{0.f, 1.f, 0.f};
    const glm::vec3 front = bodyRot(p, chest) * glm::vec3{1.f, 0.f, 0.f};
    glm::vec3 toward = g.threat - at;
    toward = glm::length(toward) > 1e-3f ? glm::normalize(toward) : front;
    // (Mostly before its face: the blow's way bends it, half.)
    glm::vec3 shield = front + toward;
    shield = glm::length(shield) > 1e-3f ? glm::normalize(shield) : front;
    for(int arm = 0; arm < 2; arm++)
    {
        Reach& r = s.reach[static_cast<za::SizeT>(arm)];
        if(!limbWhole(s, p, arm))
        {
            continue;
        }
        r.goal = at + shield * (0.35f * L) + left * ((arm == 0 ? 0.12f : -0.12f) * L);
        r.weight = za::max(r.weight * (1.f - w), w);
        r.strength = za::max(r.strength, 0.9f);
    }
}

// A living one's fall (drive(): a shove's knockdown, a stagger gone over, a throw; fallFrame's arms bracing): another
// monster near a hand as it goes, the first second (by vr_ragdoll_active_grab's chance, rolled once a fall): it grabs at
// it (fallFrame takes that arm's reach from its hand), held a moment, let go once it lands. Its hands' other holds let go
// (envStandDown's).
void fallEnv(State& s, const Parts& p, const Context& c)
{
    Env& e = s.env;
    const Fall& f = s.fall;
    const float L = armLength(s, p);
    if(!e.started)
    {
        e.started = true;
        e.living = true;
        e.on = false;
        e.seed = hash(static_cast<uint32_t>(c.num), static_cast<uint32_t>(s.begun * 72.0)) ^
                 static_cast<uint32_t>(za::max(vr_ragdoll_active_seed.value, 0.f)) * 2246822519u;
        e.baseAt = s.base;
    }
    for(int arm = 0; arm < 2; arm++)
    {
        const EnvHand& h = e.hand[static_cast<za::SizeT>(arm)];
        if(h.mode == Grip::Grab && limbWhole(s, p, arm) && f.landed < 0.f && !h.struck)
        {
            keepGrab(s, p, c, arm, L);
        }
        else if(h.mode != Grip::None)
        {
            letGo(s, arm, &c, !limbWhole(s, p, arm) ? "cut" : h.struck ? "hit" : f.landed >= 0.f ? "landed" : "falling");
        }
    }
    e.lean = false;
    e.curl = 0.f;
    e.reached = 0;
    const float t = static_cast<float>(c.now - s.begun);
    const bool idle = e.hand[0].mode == Grip::None && e.hand[1].mode == Grip::None;
    if(!idle || e.grabs > 0 || f.landed >= 0.f || t - f.since > 1.f || L <= 0.f || vr_ragdoll_active_env.value == 0.f ||
        ++e.frame % envEvery != 1 || (!c.grabSure && roll(e.seed ^ static_cast<uint32_t>(f.since * 72.f), 62u) >= vr_ragdoll_active_grab.value))
    {
        return;
    }
    const int chest = chestOf(p);
    const glm::vec3 chestAt = glmv(b3Body_GetWorldCenter(p.body[chest]));
    const glm::vec3 v = glmv(b3Body_GetLinearVelocity(p.body[chest]));
    if(glm::length(v) > 0.8f && findGrab(s, p, c, chestAt, v, L))
    {
        e.on = true; // (its traces and debug lines)
    }
}

} // namespace

namespace detail
{

// A living one's fall (drive(): a shove's knockdown, a stagger gone over, a throw) past a lip near a hand: it grabs it,
// its fall over, a climb from here (vr_ragdoll_active_pullup above 0; Grab Ledges on). Once a fall.
void livingLedge(State& s, const Parts& p, const Context& c)
{
    Env& e = s.env;
    if(!c.ledges || c.carried || vr_ragdoll_active_pullup.value <= 0.f || vr_ragdoll_active_env.value == 0.f ||
        vr_ragdoll_active_env_ledge.value == 0.f || s.climb.on)
    {
        return;
    }
    const float L = armLength(s, p);
    if(L <= 0.f)
    {
        return;
    }
    if(!e.started)
    {
        e.started = true;
        e.living = true;
        e.on = false;
        e.seed = hash(static_cast<uint32_t>(c.num), static_cast<uint32_t>(s.begun * 72.0)) ^
                 static_cast<uint32_t>(za::max(vr_ragdoll_active_seed.value, 0.f)) * 2246822519u;
        e.baseAt = s.base;
    }
    if(!e.living || e.hung || ++e.frame % envEvery != 1)
    {
        return;
    }
    const int chest = chestOf(p);
    const glm::vec3 pelvis = glmv(b3Body_GetWorldCenter(p.body[0]));
    const glm::vec3 chestAt = glmv(b3Body_GetWorldCenter(p.body[chest]));
    const float m0 = b3Body_GetMass(p.body[0]), mc = b3Body_GetMass(p.body[chest]);
    const float vz = (b3Body_GetLinearVelocity(p.body[0]).z * m0 + b3Body_GetLinearVelocity(p.body[chest]).z * mc) /
                     za::max(m0 + mc, 1e-3f);
    if(vz > -0.8f)
    {
        return;
    }
    e.probes = 0;
    const int edge = findLedge(s, p, c, pelvis, chestAt, L);
    if(edge < 0)
    {
        return;
    }
    const ledges::Edge& E = c.ledges->edges[static_cast<za::SizeT>(edge)];
    Climb& k = s.climb;
    k = Climb{};
    k.on = true;
    k.phase = ClimbPhase::Hang;
    k.since = k.since0 = c.now;
    k.pulls = roll(e.seed, 51u) < za::clamp(vr_ragdoll_active_pullup.value, 0.f, 1.f);
    k.out = levelDir(E.out);
    glm::vec3 sum{0.f};
    int n = 0;
    for(EnvHand& h : e.hand)
    {
        if(h.mode == Grip::Ledge)
        {
            sum += h.goal;
            n++;
            h.holdUntil = k.pulls ? c.now + 30.0 : h.holdUntil; // (pulling: held until it is over)
        }
    }
    k.lip = sum / static_cast<float>(za::max(n, 1));
    k.top = k.lip.z;
    e.on = true;
    // Its fall over; strong while it hangs and climbs (its arms most, its legs hanging), towards its standing frame (the
    // classes' first: stand1): straightened out, not hanging in the crouch or the pain it fell in.
    s.fall.on = false;
    const float t = static_cast<float>(c.now - s.begun);
    s.clip = Clip{0, 0, 0.f, c.now, 10.f, false};
    s.fade[static_cast<za::SizeT>(Channel::Arms)] = Fade{0.9f, 0.9f, t, t};
    s.fade[static_cast<za::SizeT>(Channel::Torso)] = Fade{0.6f, 0.6f, t, t};
    s.fade[static_cast<za::SizeT>(Channel::Head)] = Fade{0.6f, 0.6f, t, t};
    s.fade[static_cast<za::SizeT>(Channel::Legs)] = Fade{0.3f, 0.3f, t, t};
    for(Reach& r : s.reach)
    {
        r.weight = 0.f;
    }
    if(vr_debug_ragdoll.value || vr_knockdown_debug.value)
    {
        Con_Printf("climb: %d falling at %.1f m/s grabs a ledge's lip (%d hands) at %.0f %.0f %.0f: %s\n", c.num, -vz, n,
            k.lip.x * p.m2u, k.lip.y * p.m2u, k.lip.z * p.m2u, k.pulls ? "to pull itself up" : "to hang, then drop");
    }
}

} // namespace detail

namespace
{

// Each frame after the death's own reaches: the hands' modes kept up, the probes every few frames, the curl, and the
// hands' (and a curl's feet's) reaches applied over the targets.
void envFrame(State& s, const Parts& p, const Context& c, float t, za::Array<glm::vec3, ragdoll::maxBones>& spin)
{
    Env& e = s.env;
    const Death& d = s.death;
    if(!e.started)
    {
        e.started = true;
        e.floorSet = false;
        e.on = vr_ragdoll_active_env.value != 0.f && d.on && !d.collapse && armLength(s, p) > 0.f;
        e.seed = hash(static_cast<uint32_t>(c.num), static_cast<uint32_t>(s.begun * 72.0)) ^
                 static_cast<uint32_t>(za::max(vr_ragdoll_active_seed.value, 0.f)) * 2246822519u;
        e.baseAt = s.base;
    }
    if(!e.on)
    {
        return;
    }
    const glm::vec3 up{0.f, 0.f, 1.f};
    const float L = armLength(s, p);
    e.frame++;
    const int chest = chestOf(p);
    const glm::vec3 pelvis = glmv(b3Body_GetWorldCenter(p.body[0]));
    const glm::vec3 chestAt = glmv(b3Body_GetWorldCenter(p.body[chest]));
    const float m0 = b3Body_GetMass(p.body[0]), mc = b3Body_GetMass(p.body[chest]);
    const glm::vec3 v = (glmv(b3Body_GetLinearVelocity(p.body[0])) * m0 + glmv(b3Body_GetLinearVelocity(p.body[chest])) * mc) /
                        za::max(m0 + mc, 1e-3f);
    const float speed = glm::length(v);
    const glm::quat qc = bodyRot(p, chest);
    const glm::vec3 front = qc * glm::vec3{1.f, 0.f, 0.f}, left = qc * glm::vec3{0.f, 1.f, 0.f};
    // Done with what is round him: env off, crawling or rolling over (his own way), blasted (weaken), nothing to reach.
    const bool off = vr_ragdoll_active_env.value == 0.f || L <= 0.f;
    const bool done = off || d.crawlFrom >= 0.0 || d.rollFrom >= 0.0 || s.base < e.baseAt * 0.6f;
    // (A living one's: its ledge's holds and its climb alone: livingLedge grabbed it.)
    const bool brace = !done && !e.living && vr_ragdoll_active_env_brace.value != 0.f;
    const bool lean = !done && !e.living && vr_ragdoll_active_env_lean.value != 0.f;
    const bool ledge = !done && !e.living && vr_ragdoll_active_env_ledge.value != 0.f && c.ledges;
    // (Another monster near a hand as he falls: by its chance, rolled once a death.)
    const bool grab = !done && !e.living && e.grabs == 0 && (c.grabSure || roll(e.seed, 61u) < vr_ragdoll_active_grab.value);
    // Each hand's mode kept up, or let go.
    for(int arm = 0; arm < 2; arm++)
    {
        EnvHand& h = e.hand[static_cast<za::SizeT>(arm)];
        if(h.mode == Grip::None)
        {
            continue;
        }
        const int lo = s.chain[static_cast<za::SizeT>(arm)][1];
        if(!limbWhole(s, p, arm))
        {
            letGo(s, arm, &c, "cut");
        }
        else if(done)
        {
            letGo(s, arm, &c, off ? "off" : d.crawlFrom >= 0.0 || d.rollFrom >= 0.0 ? "crawling" : "blasted");
        }
        else if(s.partScale[static_cast<za::SizeT>(lo)] < 0.5f)
        {
            letGo(s, arm, &c, "taken by a hand");
        }
        else if(h.struck)
        {
            letGo(s, arm, &c, "hit");
        }
        else if(h.mode == Grip::Catch)
        {
            const glm::vec3 hand = limbEnd(s, p, arm);
            if(h.touched < 0.0)
            {
                // Reaching: landed once the hand is near the surface (or late).
                const float above = glm::dot(hand - h.goal, h.normal);
                if(above < 0.12f * L || c.now - h.since > 2.0 * catchAhead)
                {
                    h.touched = c.now;
                    h.goal = hand - h.normal * za::max(above, 0.f);
                }
            }
            else
            {
                // Landed: the push gives (the arms' strength eased off), then the hand goes back to the death's way.
                const float k = static_cast<float>(c.now - h.touched) / catchGive;
                h.strength = 0.8f * (1.f - smooth01(k)) + 0.1f;
                if(k > 1.6f)
                {
                    letGo(s, arm, &c, "landed");
                }
            }
        }
        else if(h.mode == Grip::Brace)
        {
            // A prop: pressed on, never held; the strength fades over a second. (Not there yet: straight to it from
            // where the hand is.)
            const float k = static_cast<float>(c.now - h.since);
            const glm::vec3 hand = limbEnd(s, p, arm);
            const float above = glm::dot(hand - h.goal, h.normal);
            if(k > 0.3f && above > 0.f)
            {
                h.goal = hand - h.normal * above;
            }
            h.strength = 0.8f - 0.6f * smooth01((k - 0.4f) / 0.8f);
            if(k > 1.4f)
            {
                letGo(s, arm, &c, "its time");
            }
        }
        else if(h.mode == Grip::Grab)
        {
            keepGrab(s, p, c, arm, L);
        }
        else if(h.mode == Grip::Lean || h.mode == Grip::Ledge)
        {
            keepHold(s, p, c, arm, L);
        }
    }
    // A living one hanging from its ledge: its climb.
    if(s.climb.on)
    {
        climbFrame(s, p, c, L);
    }
    // The lean: the pelvis drawn weakly to the wall (not a prop's), fading; his knees give (startLean).
    if(e.lean && !done && c.now - e.leanFrom < leanPull)
    {
        const float gap = glm::dot(pelvis - e.wallAt, e.wallN);
        const glm::vec3 to = -levelDir(e.wallN);
        if(gap > 0.2f * L && glm::length(to) > 0.f)
        {
            const float f = 0.25f * s.totalMass * c.gravity * (1.f - smooth01(static_cast<float>(c.now - e.leanFrom) / leanPull));
            pushPart(p, 0, to * f);
        }
    }
    // The slide: his back (or side) against the wall, his fall slowed to a slide down it (its friction and his legs as
    // they give): his pelvis's and chest's sinking held to slideSpeed by at most vr_ragdoll_active_env_slide body
    // weights, fading. (Not once he has toppled off it: his chest away from the wall.)
    const float slideMost = za::max(vr_ragdoll_active_env_slide.value, 0.f);
    if(e.lean && !done && slideMost > 0.f && c.now - e.leanFrom < slideTime &&
        glm::dot(chestAt - e.wallAt, e.wallN) < slideNear * L)
    {
        const float fade = 1.f - smooth01(static_cast<float>(c.now - e.leanFrom) / slideTime);
        const float cap = slideMost * s.totalMass * c.gravity * fade / za::max(m0 + mc, 1e-3f);
        for(const int b : {0, chest})
        {
            const float vz = b3Body_GetLinearVelocity(p.body[b]).z;
            const float m = b3Body_GetMass(p.body[b]);
            if(vz < -slideSpeed)
            {
                pushPart(p, b, up * za::min(m * (-slideSpeed - vz) / slideGive, cap * m));
            }
        }
    }
    // The floor under his pelvis (one query a frame: the sense's and the tumble's below).
    const float under = c.floorBelow ? c.floorBelow(pelvis, 3.f * L) : -1e30f;
    if(!e.floorSet && under > -1e29f)
    {
        e.floorSet = true;
        e.floorAt = under;
    }
    // Sense: every few frames.
    if((brace || lean || ledge || grab) && e.frame % envEvery == 1)
    {
        e.probes = 0;
        const bool holding = B3_IS_NON_NULL(e.hand[0].joint) || B3_IS_NON_NULL(e.hand[1].joint);
        const bool reaching = e.hand[0].mode == Grip::Catch || e.hand[1].mode == Grip::Catch;
        // (Still up, for a lean: his chest an arm's length over the floor at least, his pelvis not far over it: on his
        // feet, not falling past a wall. Or thrown up off the floor he stood on by the kill (a super shotgun's blast
        // slams him up against the wall): higher, over that same floor.)
        const bool sameFloor = e.floorSet && za::abs(under - e.floorAt) < 0.5f * L;
        const bool upright = chestAt.z - under > 0.9f * L && (pelvis.z - under < 1.6f * L || (sameFloor && pelvis.z - under < 2.6f * L));
        // Another monster near a hand as he falls (the first second of his death): he grabs at it.
        if(grab && speed > 0.8f && !holding && !reaching && t < 1.2f && findGrab(s, p, c, chestAt, v, L))
        {
            e.nextCatch = c.now + 0.6; // (no catch while it reaches)
        }
        // Along his fall: a surface he meets soon.
        if(speed > 0.8f && !holding && !reaching && c.now >= e.nextCatch)
        {
            const glm::vec3 dir = v / speed;
            const float in = 0.3f * L; // (from a little inside him)
            const float len = za::clamp(speed * catchAhead + 0.5f * L, 0.5f * L, 8.f * L) + in;
            EnvRay r = envCast(e, p, c, chestAt - dir * in, dir * len);
            r.dist = za::max(r.dist - in, 0.f);
            if(r.hit && glm::dot(r.n, dir) < -0.35f)
            {
                const bool wall = za::abs(r.n.z) < 0.6f;
                // (Ahead of him or beside him; the floor only falling forwards: not slumping, not on his back.)
                if(glm::dot(front, dir) > (wall ? -0.15f : 0.3f) && brace)
                {
                    startCatch(s, p, c, r, v, chestAt, left, L, speed); // (ahead of him or beside)
                }
                // (Thrown into it by the kill, his pelvis maybe off the floor: not falling down past it but across to it.)
                else if(wall && lean && (upright || (sameFloor && chestAt.z - under > 0.9f * L && v.z > -0.6f * speed)) && !e.leaned &&
                        t < leanWithin)
                {
                    startLean(s, p, c, r, 3, L, t); // (falling back into it)
                }
            }
        }
        // All round him, a sixth of the way round each time from behind him (a dying torso is often twisted from the
        // way he faced): a wall he dies against.
        if(lean && upright && !e.leaned && !holding && t < leanWithin)
        {
            const int k = (e.frame / envEvery) % 6;
            glm::vec3 back = -levelDir(front);
            back = glm::length(back) > 0.f ? back : glm::vec3{1.f, 0.f, 0.f};
            const glm::vec3 dir = glm::angleAxis(static_cast<float>(k) * 1.0471976f, up) * back;
            // (From a little inside his body: his back against the wall, the chest's middle may be past its face.)
            const float in = 0.3f * L;
            const float len = 1.5f * L + in; // (the shoulder is beside the chest: an arm reaches that far from its middle)
            EnvRay r = envCast(e, p, c, chestAt - dir * in, dir * len);
            if(!r.hit)
            {
                // (Slammed into it by the kill, his chest may start inside the wall, where a ray meets nothing: from
                // his pelvis too.)
                r = envCast(e, p, c, pelvis - dir * in, dir * len);
            }
            if(r.hit && za::abs(r.n.z) < 0.5f && glm::dot(r.n, -dir) > 0.5f)
            {
                const float side = glm::dot(dir, levelDir(left));
                startLean(s, p, c, r, k == 0 ? 0 : side > 0.5f ? 1 : side < -0.5f ? 2 : glm::dot(dir, back) > 0.f ? 0 : 4, L, t);
            }
        }
        // Falling past a ledge's lip near a hand.
        if(ledge && !e.hung && v.z < -0.8f)
        {
            findLedge(s, p, c, pelvis, chestAt, L);
        }
    }
    // Tumbling (nothing to catch, nothing held): curled up, the arms in, the knees up.
    const glm::vec3 w = glmv(b3Body_GetAngularVelocity(p.body[chest]));
    bool busy = false;
    for(const EnvHand& h : e.hand)
    {
        busy = busy || (h.mode != Grip::None && h.mode != Grip::Curl);
    }
    // (Off the floor: not the spin of a body settling or thrown down at its death.)
    const bool airborne = pelvis.z - under > 0.8f * L;
    const float curlWant = brace && !busy && airborne && t > 0.3f && glm::length(w) > 5.f && speed > 2.f ? 0.8f : 0.f;
    e.curl += (curlWant - e.curl) * (1.f - za::exp(-c.dt / 0.15f));
    e.curl = e.curl < 0.01f && curlWant == 0.f ? 0.f : e.curl;
    // The reaches: each hand's eased to what it wants, over the targets (the death's reach on that limb replaced).
    uint8_t reached = 0;
    const auto apply = [&](int limb, const glm::vec3& goal, float weight, float strength) {
        Reach& r = s.reach[static_cast<za::SizeT>(limb)];
        r.goal = goal;
        r.weight = weight;
        r.strength = strength;
        reachTargets(s, p, limb, s.target);
        for(int j = 0; j < 2; j++)
        {
            const int b = s.chain[static_cast<za::SizeT>(limb)][static_cast<za::SizeT>(j)];
            if(b >= 0)
            {
                spin[static_cast<za::SizeT>(b)] *= 1.f - za::clamp(weight, 0.f, 1.f);
            }
        }
        reached |= static_cast<uint8_t>(1u << limb);
    };
    for(int arm = 0; arm < 2; arm++)
    {
        EnvHand& h = e.hand[static_cast<za::SizeT>(arm)];
        // (Not once done (crawling, blasted): its hands let go each frame, it would curl them again: a "lets go" a frame.)
        if(e.curl > 0.f && !done && (h.mode == Grip::None || h.mode == Grip::Curl) && limbWhole(s, p, arm))
        {
            h.mode = Grip::Curl;
            h.goal = chestAt + front * (0.3f * L) + up * (0.2f * L) + left * ((arm == 0 ? 0.12f : -0.12f) * L);
            h.want = e.curl;
            h.strength = 0.6f;
        }
        else if(h.mode == Grip::Curl)
        {
            h.mode = Grip::None;
            h.want = 0.f;
        }
        const float rate = h.want > h.weight ? 0.1f : 0.25f;
        h.weight += (h.want - h.weight) * (1.f - za::exp(-c.dt / rate));
        h.weight = h.want == 0.f && h.weight < 0.01f ? 0.f : h.weight;
        if(h.weight > 0.f && limbWhole(s, p, arm))
        {
            apply(arm, h.goal, h.weight, h.strength * za::max(vr_ragdoll_active_env_arms.value, 0.f));
        }
    }
    if(e.curl > 0.f)
    {
        const glm::vec3 spine = glm::length(chestAt - pelvis) > 1e-4f ? glm::normalize(chestAt - pelvis) : up;
        for(int leg = 2; leg < 4; leg++)
        {
            if(d.stepLeg != leg && limbWhole(s, p, leg))
            {
                const float len = limbLength(s, p, leg);
                apply(leg, pelvis + front * (0.35f * len) - spine * (0.45f * len), e.curl, 0.6f);
            }
        }
    }
    // A limb reached last frame and not now: its reach gone (unless the death's own: the wound's hand, a step, which
    // drive() left to this frame: applied here).
    for(int limb = 0; limb < limbCount; limb++)
    {
        const uint8_t bit = static_cast<uint8_t>(1u << limb);
        if(!(e.reached & bit) || (reached & bit))
        {
            continue;
        }
        Reach& r = s.reach[static_cast<za::SizeT>(limb)];
        if(limb != d.woundArm && limb != d.stepLeg)
        {
            r.weight = 0.f;
        }
        else if(r.weight > 0.f)
        {
            apply(limb, r.goal, r.weight, r.strength);
            reached &= static_cast<uint8_t>(~bit); // (the death's own again: not this one's)
        }
    }
    e.reached = reached;
}

void envEnd(State& s)
{
    for(int arm = 0; arm < 2; arm++)
    {
        letGo(s, arm, nullptr, nullptr);
    }
    s.env.lean = false;
}

// Integration (stages 2-4): what is round him stands down while something else leads the limbs (drive(): a swim, a
// stagger, a fall, a get-up; `why` for the traces). Each such frame: the hands' holds let go (their joints destroyed:
// nothing fights a stagger's pin or a get-up), no lean or curl, and the limbs left to that one's reaches (envFrame
// doesn't clear them); envFrame takes up again once it is over.
void envStandDown(State& s, const Context& c, const char* why)
{
    for(int arm = 0; arm < 2; arm++)
    {
        letGo(s, arm, &c, why);
    }
    if(s.climb.on) // (a climber fallen into a liquid, cut: its climb over)
    {
        s.climb.on = false;
        s.climb.ended = why;
    }
    s.env.lean = false;
    s.env.curl = 0.f;
    s.env.reached = 0;
}

// vr_ragdoll_active_trace's `ragenv` line: the pelvis, each hand's mode, hold, distance to its goal, load, slide; the
// curl.
void envTrace(const State& s, const Parts& p, const Context& c, float t)
{
    const Env& e = s.env;
    if(!e.on)
    {
        return;
    }
    const float u = p.m2u;
    const glm::vec3 pelvis = glmv(b3Body_GetWorldCenter(p.body[0])) * u;
    za::Array<float, 2> gap{};
    for(int arm = 0; arm < 2; arm++)
    {
        const EnvHand& h = e.hand[static_cast<za::SizeT>(arm)];
        gap[static_cast<za::SizeT>(arm)] = h.mode != Grip::None && limbWhole(s, p, arm) ? glm::distance(limbEnd(s, p, arm), h.goal) * u : -1.f;
    }
    const EnvHand& l = e.hand[0];
    const EnvHand& r = e.hand[1];
    Con_Printf("ragenv %d %.3f p %.0f %.0f %.1f m %d%d h %d%d g %.1f %.1f ld %.0f %.0f sl %.1f %.1f c %.2f\n", c.num, t,
        pelvis.x, pelvis.y, pelvis.z, static_cast<int>(l.mode), static_cast<int>(r.mode), B3_IS_NON_NULL(l.joint) ? 1 : 0,
        B3_IS_NON_NULL(r.joint) ? 1 : 0, gap[0], gap[1], l.load, r.load, l.slid * u, r.slid * u, e.curl);
}

// vr_ragdoll_active_debug: the probes (cyan, orange where they met something), each hand's goal and hold (magenta), the
// wall leaned on (its normal, blue).
void envDraw(const State& s, const Parts& p)
{
    const Env& e = s.env;
    if(!e.on)
    {
        return;
    }
    const float u = p.m2u;
    const glm::vec4 cyan{0.2f, 0.9f, 1.f, 0.8f}, orange{1.f, 0.55f, 0.1f, 1.f}, magenta{1.f, 0.2f, 0.9f, 1.f};
    for(int i = 0; i < e.probes; i++)
    {
        const EnvProbe& pr = e.probe[static_cast<za::SizeT>(i)];
        lines::line(pr.from * u, pr.to * u, 0.1f, cyan, pr.hit ? orange : cyan);
        if(pr.hit)
        {
            lines::point(pr.to * u, 0.8f, orange);
        }
    }
    for(int arm = 0; arm < 2; arm++)
    {
        const EnvHand& h = e.hand[static_cast<za::SizeT>(arm)];
        if(h.mode == Grip::None || !limbWhole(s, p, arm))
        {
            continue;
        }
        const bool held = B3_IS_NON_NULL(h.joint);
        lines::point(h.goal * u, held ? 1.6f : 1.f, magenta);
        lines::line(limbEnd(s, p, arm) * u, h.goal * u, held ? 0.3f : 0.12f, magenta, magenta);
    }
    if(e.lean)
    {
        const glm::vec4 blue{0.3f, 0.4f, 1.f, 1.f};
        lines::line(e.wallAt * u, (e.wallAt + e.wallN * 0.3f) * u, 0.15f, blue, blue);
    }
}

} // namespace

Channel channelOf(const ragdoll::Bone& bone)
{
    const char* n = bone.name;
    if(named(n, "thigh") || named(n, "shin") || named(n, "foot") || named(n, "leg"))
    {
        return Channel::Legs;
    }
    if(bone.role == modelmeta::BoneRole::Head || named(n, "head") || named(n, "jaw"))
    {
        return Channel::Head;
    }
    if(named(n, "arm") || named(n, "hand"))
    {
        return Channel::Arms;
    }
    return Channel::Torso;
}

const char* channelName(Channel c)
{
    switch(c)
    {
    case Channel::Legs: return "legs";
    case Channel::Torso: return "torso";
    case Channel::Arms: return "arms";
    case Channel::Head: return "head";
    }
    return "?";
}

float fadeAt(const Fade& f, float t)
{
    if(t <= f.t0)
    {
        return f.from;
    }
    if(t >= f.t1)
    {
        return f.to;
    }
    return f.from + (f.to - f.from) * smooth01((t - f.t0) / za::max(f.t1 - f.t0, 1e-4f));
}

void begin(State& s, const Parts& p, const qmodel_t* model, const glm::vec3& originMetres, const b3JointId* joints,
    float friction, float limpHertz, double now)
{
    end(s);
    s = State{};
    s.on = true;
    s.begun = now;
    s.model = model;
    s.origin = originMetres;
    s.turn = p.turn;
    s.friction = friction;
    s.limpHertz = limpHertz;
    const ragdoll::Rig& rig = *p.rig;
    for(int b = 0; b < p.count; b++)
    {
        const auto i = static_cast<za::SizeT>(b);
        s.joint[i] = joints[b];
        s.partScale[i] = 1.f;
        s.target[i] = s.animTarget[i] = glm::quat{1.f, 0.f, 0.f, 0.f};
        if(!drivable(s, p, b))
        {
            continue;
        }
        if(rig.bones[b].joint == ragdoll::Joint::Hinge)
        {
            s.lower[i] = b3RevoluteJoint_GetLowerLimit(s.joint[i]);
            s.upper[i] = b3RevoluteJoint_GetUpperLimit(s.joint[i]);
        }
        else
        {
            s.lower[i] = b3SphericalJoint_GetConeLimit(s.joint[i]);
            s.upper[i] = b3SphericalJoint_GetUpperTwistLimit(s.joint[i]);
        }
    }
    // Each joint's motor's most at strength 1: the weight of what hangs on it (its bone and those on it) times its lever
    // (the joint to their middle), at 1 g.
    for(int b = 0; b < p.count; b++)
    {
        if(rig.bones[b].joint != ragdoll::Joint::Loose && !cut(p, b))
        {
            s.totalMass += b3Body_GetMass(p.body[b]);
        }
        if(!drivable(s, p, b))
        {
            continue;
        }
        float m = 0.f;
        glm::vec3 com{0.f};
        for(int c = 0; c < p.count; c++)
        {
            if(rig.bones[c].joint == ragdoll::Joint::Loose || cut(p, c) || !onBone(rig, c, b))
            {
                continue;
            }
            const float mc = b3Body_GetMass(p.body[c]);
            m += mc;
            com += glmv(b3Body_GetWorldCenter(p.body[c])) * mc;
        }
        com /= za::max(m, 1e-6f);
        const float lever = glm::length(com - worldPoint(p, b, rig.bones[b].pivot));
        s.cap[static_cast<za::SizeT>(b)] = 9.8f * m * za::max(lever, 0.05f);
    }
    // The limbs' chains, by their bones' names.
    for(auto& ch : s.chain)
    {
        ch[0] = ch[1] = -1;
    }
    for(int b = 0; b < p.count; b++)
    {
        const char* n = rig.bones[b].name;
        const int side = sideOf(n);
        if(side < 0)
        {
            continue;
        }
        const bool arm = std::strncmp(n, "upperarm_", 9) == 0 || std::strncmp(n, "arm_", 4) == 0;
        const bool leg = std::strncmp(n, "thigh_", 6) == 0;
        if(!arm && !leg)
        {
            continue;
        }
        const int limb = (leg ? 2 : 0) + side;
        for(int c = 0; c < p.count; c++)
        {
            const char* cn = rig.bones[c].name;
            const bool lower = arm ? (std::strncmp(cn, "forearm_", 8) == 0 || std::strncmp(cn, "hand_", 5) == 0)
                                   : std::strncmp(cn, "shin_", 5) == 0;
            if(rig.bones[c].parent == b && lower)
            {
                s.chain[static_cast<za::SizeT>(limb)][0] = static_cast<int8_t>(b);
                s.chain[static_cast<za::SizeT>(limb)][1] = static_cast<int8_t>(c);
                break;
            }
        }
    }
    for(Fade& f : s.fade)
    {
        f = Fade{0.f, 0.f, 0.f, 0.f};
    }
}

void beginDeath(State& s, const Parts& p, const DeathInfo& di, double now)
{
    s.struggle.on = false; // (it leads from here: a struggle gives way)
    const ragdoll::Rig& rig = *p.rig;
    if(di.death < 0 || di.death >= rig.deaths)
    {
        return;
    }
    Death& d = s.death;
    d = Death{};
    d.on = true;
    d.collapse = di.collapse;
    const int first = rig.deathFirst[di.death], last = rig.deathLast[di.death];
    const float stretch = za::clamp(vr_ragdoll_active_fade.value, 0.1f, 5.f);
    d.length = za::max(static_cast<float>(last) - di.frame + 1.f, 2.f) * 0.1f * stretch;
    s.clip = Clip{first, last, za::clamp(di.frame, static_cast<float>(first), static_cast<float>(last)), now, 10.f, false};
    s.base = di.collapse ? za::clamp(vr_ragdoll_active_headshot.value, 0.f, 1.f) : 1.f;
    // Legs first (the knees give), then the torso, the arms and the head.
    const float D = d.length;
    s.fade[static_cast<za::SizeT>(Channel::Legs)] = Fade{1.f, 0.f, 0.f, 0.5f * D};
    s.fade[static_cast<za::SizeT>(Channel::Torso)] = Fade{1.f, 0.f, 0.25f * D, 0.9f * D};
    s.fade[static_cast<za::SizeT>(Channel::Arms)] = Fade{1.f, 0.f, 0.3f * D, 1.1f * D};
    s.fade[static_cast<za::SizeT>(Channel::Head)] = Fade{1.f, 0.f, 0.3f * D, 1.2f * D};
    if(di.collapse)
    {
        return; // (a headshot, beheaded: no pin, no stumble, no wound, no crawl)
    }
    const float pin = za::max(di.noPin ? 0.f : vr_ragdoll_active_pin.value, 0.f);
    const float pinTime = za::max(vr_ragdoll_active_pin_time.value, 0.05f);
    s.pin.force = pin;
    s.pin.fade = Fade{1.f, 0.f, 0.f, pinTime};
    // Dying on the move: carried on a moment (the pin, stronger and longer, its anchor going on with his motion), the
    // leg behind stepping ahead; his legs hold a little longer.
    const float speed = glm::length(glm::vec2{di.walk}) * p.m2u;
    if(vr_ragdoll_active_stumble.value > 0.f && speed >= vr_ragdoll_active_stumble.value)
    {
        d.stumble = true;
        d.walk = glm::vec3{di.walk.x, di.walk.y, 0.f};
        s.pin.force = za::max(pin, 0.8f);
        s.pin.fade = Fade{1.f, 0.f, 0.25f, 0.25f + za::max(pinTime, 0.4f)};
        s.fade[static_cast<za::SizeT>(Channel::Legs)] = Fade{1.f, 0.f, stepTime, stepTime + 0.5f * D};
        const glm::vec3 dir = glm::normalize(d.walk);
        float behind = 1e30f;
        for(int leg = 2; leg < 4; leg++)
        {
            if(!limbWhole(s, p, leg))
            {
                continue;
            }
            const float along = glm::dot(limbEnd(s, p, leg), dir);
            if(along < behind)
            {
                behind = along;
                d.stepLeg = leg;
            }
        }
    }
    // Hit in the body or a limb: the part hit and where (the nearest of its vertices), a free hand to hold it.
    if(di.wound && roll(di.seed, 1u) < (di.sureWound ? 1.f : vr_ragdoll_active_wound.value))
    {
        float best = 1e30f;
        glm::vec3 bestLocal{0.f};
        for(int b = 0; b < p.count; b++)
        {
            const ragdoll::Bone& bone = rig.bones[b];
            if(cut(p, b) || bone.joint == ragdoll::Joint::Loose || bone.role == modelmeta::BoneRole::Head)
            {
                continue;
            }
            const glm::vec3 local = glmv(b3Body_GetLocalPoint(p.body[b], b3v(di.woundAt))) / p.k; // (rest units)
            for(const glm::vec3& v : bone.points)
            {
                const float dd = glm::distance(v, local);
                if(dd < best)
                {
                    best = dd;
                    d.woundPart = b;
                    bestLocal = v;
                }
            }
        }
        if(d.woundPart >= 0 && best < 16.f)
        {
            const ragdoll::Bone& bone = rig.bones[d.woundPart];
            glm::vec3 mid{0.f};
            for(const glm::vec3& v : bone.points)
            {
                mid += v;
            }
            mid /= static_cast<float>(za::max<za::SizeT>(bone.points.size(), 1));
            d.woundAt = bestLocal * p.k;
            d.woundOut = glm::length(bestLocal - mid) > 1e-3f ? glm::normalize(bestLocal - mid) : glm::vec3{1.f, 0.f, 0.f};
            // The hand on the wound's side, unless that arm is the one hit (or gone): the other.
            const int prefer = bestLocal.y >= 0.f ? 0 : 1;
            for(const int arm : {prefer, 1 - prefer})
            {
                const int top = s.chain[static_cast<za::SizeT>(arm)][0];
                if(limbWhole(s, p, arm) && !onBone(rig, d.woundPart, top))
                {
                    d.woundArm = arm;
                    break;
                }
            }
        }
        else
        {
            d.woundPart = -1;
        }
    }
    d.crawl = di.crawler && roll(di.seed, 2u) < (di.sureCrawl ? 1.f : vr_ragdoll_active_crawl.value) &&
              limbWhole(s, p, 0) && limbWhole(s, p, 1);
}

bool liquidPending(const State& s, double now)
{
    return liquidsOn() && s.death.on && !s.death.collapse && s.swim.from < 0.0 && now - s.begun <= liquidWindow;
}

void beginStagger(State& s, const Parts& p, const StaggerInfo& info, double now)
{
    s.struggle.on = false; // (it leads from here: a struggle gives way)
    const ragdoll::Rig& rig = *p.rig;
    const float t = static_cast<float>(now - s.begun);
    Stagger& g = s.stagger;
    g = Stagger{};
    g.on = true;
    g.dir = glm::length(level(info.dir)) > 1e-3f ? glm::normalize(level(info.dir)) : glm::vec3{1.f, 0.f, 0.f};
    g.balance = za::clamp(info.balance, 0.f, 1.f);
    g.travel = za::max(info.travel, 0.f);
    // (Its travel slowing to none over its backpedal: 1 m over 0.7 s, 3.5 m (a grunt's slide) over 1.45 s, its first
    // speed 4.8 m/s.)
    g.time = za::clamp(0.4f + 0.3f * g.travel, 0.3f, 1.6f);
    g.stepsMost = za::clamp(static_cast<int>(vr_ragdoll_active_stagger_steps.value + 0.5f), 1, 10);
    // Beaten (its roll under its chance): it backpedals 3 or 4 steps (4 nearer the edge), then gives way.
    g.giveAfter = g.balance < 0.5f ? za::min(3 + static_cast<int>(g.balance * 4.f), g.stepsMost) : -1;
    g.recoverAt = info.recoverAt;
    g.sure = info.sure;
    if(g.sure)
    {
        g.giveAfter = -1;
    }
    s.clip = Clip{info.first, info.last >= info.first ? info.last : info.first, static_cast<float>(info.first), now, 10.f, false};
    s.base = 1.f;
    // Its legs strong (planted, stepping), its torso by its balance, its arms and head with some give.
    s.fade[static_cast<za::SizeT>(Channel::Legs)] = Fade{1.f, 1.f, 0.f, 0.f};
    // (Held up well enough to backpedal whatever its balance: a beaten one gives way after its steps, staggerFrame.)
    const float hold = 0.7f + 0.3f * g.balance;
    s.fade[static_cast<za::SizeT>(Channel::Torso)] = Fade{0.5f + 0.5f * g.balance, 0.5f + 0.5f * g.balance, 0.f, 0.f};
    s.fade[static_cast<za::SizeT>(Channel::Arms)] = Fade{0.6f, 0.6f, 0.f, 0.f};
    s.fade[static_cast<za::SizeT>(Channel::Head)] = Fade{0.8f, 0.8f, 0.f, 0.f};
    s.support = Fade{0.3f + 0.65f * hold, 0.3f + 0.65f * hold, 0.f, 0.f};
    s.pin.force = za::max(vr_ragdoll_active_stagger_pin.value, 0.f) * (0.1f + 0.9f * hold);
    s.pin.fade = Fade{1.f, 1.f, 0.f, 0.f};
    s.pin.hertz = 5.f;
    s.pin.offset = glm::vec3{0.f};
    // Where it stands: its pelvis over its feet (each foot's place and its stance from its middle, level).
    const glm::vec3 pelvis = glmv(b3Body_GetWorldCenter(p.body[0]));
    const glm::vec3 middle = massCentre(p);
    g.pelvis0 = pelvis;
    // (Its pin from where its pelvis is, not where its stand frame has it: a model's feet below its box's floor, a
    // flinch's pelvis, would have it pulled down or aside at once.)
    {
        glm::quat q;
        glm::vec3 pos0;
        ragdoll::bonePose(rig, ragdoll::poseOfFrame(s.model, info.first), 0, q, pos0);
        // (Its stance turned as its pelvis is now, about the vertical: a pain's frame may twist it, the ogre's 60
        // degrees; turning it back would wring its planted feet.)
        const glm::vec3 fwd = bodyRot(p, 0) * glm::inverse(q) * glm::vec3{1.f, 0.f, 0.f};
        if(glm::length(glm::vec2{fwd.x, fwd.y}) > 1e-3f)
        {
            s.turn = glm::angleAxis(za::atan2(fwd.y, fwd.x), glm::vec3{0.f, 0.f, 1.f});
        }
        g.lift = glmv(b3Body_GetPosition(p.body[0])) - (s.origin + s.turn * (pos0 * p.k));
    }
    float footZ = 1e30f;
    for(int leg = 2; leg < 4; leg++)
    {
        const auto i = static_cast<za::SizeT>(leg - 2);
        g.planted[i] = limbWhole(s, p, leg) ? limbEnd(s, p, leg) : pelvis;
        g.footAt[i] = g.planted[i];
        g.stance[i] = level(g.planted[i] - middle);
        footZ = limbWhole(s, p, leg) ? za::min(footZ, g.planted[i].z) : footZ;
    }
    g.height = footZ < 1e29f ? za::max(pelvis.z - footZ, 0.1f) : 0.5f;
    // The shove: its pelvis on at the pin's first speed, its top pushed harder (by its height: the feet none).
    const float v0 = 2.f * g.travel / g.time;
    const float extra = za::max(info.push, 0.f) * za::max(vr_ragdoll_active_stagger_push.value, 0.f);
    for(int b = 0; b < p.count; b++)
    {
        if(cut(p, b))
        {
            continue;
        }
        const float up = glmv(b3Body_GetWorldCenter(p.body[b])).z - footZ;
        const float share = za::clamp(up / za::max(2.f * g.height, 0.1f), 0.f, 1.f); // (its pelvis about halfway)
        const float v = v0 * smooth01(share * 2.f) + extra * share * share;
        b3Body_SetLinearVelocity(p.body[b], b3v(g.dir * v));
        b3Body_SetAngularVelocity(p.body[b], b3Vec3_zero);
        b3Body_SetAwake(p.body[b], true);
    }
    s.on = true;
    (void)t;
}

void beginFall(State& s, const Parts& p, const glm::vec3& dir, uint32_t slack, float slackTime, bool curl, double now,
    float legsHold)
{
    s.struggle.on = false; // (it leads from here: a struggle gives way)
    const float t = static_cast<float>(now - s.begun);
    Fall& f = s.fall;
    f = Fall{};
    f.on = true;
    f.since = t;
    f.height = -1.f;
    f.dir = glm::length(level(dir)) > 1e-3f ? glm::normalize(level(dir)) : s.turn * glm::vec3{1.f, 0.f, 0.f};
    if(s.stagger.on)
    {
        s.stagger.on = false;
        if(s.stagger.outcome == Outcome::Going)
        {
            s.stagger.outcome = Outcome::Fell;
            s.stagger.decided = now;
        }
    }
    s.rise.on = false;
    // Its pin let go, its weight its own again; its legs give, its torso and head keep some tone, its arms brace.
    dropPin(s.pin);
    s.support = Fade{fadeAt(s.support, t), 0.f, t, t + 0.15f};
    const float legs = fadeAt(s.fade[static_cast<za::SizeT>(Channel::Legs)], t);
    const float keep = za::max(legsHold, 0.f);
    s.fade[static_cast<za::SizeT>(Channel::Legs)] =
        Fade{keep > 0.f ? 0.6f : za::min(legs, 0.6f), 0.05f, t + keep, t + keep + 0.25f};
    const float torso = curl ? 0.7f : 0.5f;
    s.fade[static_cast<za::SizeT>(Channel::Torso)] = Fade{torso, torso, t, t};
    s.fade[static_cast<za::SizeT>(Channel::Head)] = Fade{0.6f, 0.6f, t, t};
    s.fade[static_cast<za::SizeT>(Channel::Arms)] = Fade{0.4f, 0.4f, t, t};
    for(int leg = 2; leg < 4; leg++)
    {
        s.reach[static_cast<za::SizeT>(leg)].weight = 0.f;
    }
    s.stagger.stepLeg = -1;
    s.slack = slack;
    s.slackUntil = now + static_cast<double>(za::max(slackTime, 0.f));
    s.base = s.base > 0.f ? s.base : 1.f;
    s.on = true;
    (void)p;
}

void beginRise(State& s, const Parts& p, const RiseInfo& info, double now)
{
    s.struggle.on = false; // (it leads from here: a struggle gives way)
    const float t0 = static_cast<float>(now - s.begun);
    const float prep = za::max(info.prep, 0.1f);
    const float fps = za::max(info.fps, 1.f);
    s.turn = glm::angleAxis(info.yaw, glm::vec3{0.f, 0.f, 1.f});
    s.origin = info.origin;
    // Lying the other way up (its chest's front, the rig's x, up where the frame has it down, or down where up): it
    // rolls over first, about its spine, the way that brings its front round.
    Rise& r = s.rise;
    r = Rise{};
    const int chest = chestOf(p);
    const glm::vec3 x{1.f, 0.f, 0.f};
    const glm::vec3 front = bodyRot(p, chest) * x;
    glm::quat cq;
    glm::vec3 ct;
    ragdoll::bonePose(*p.rig, ragdoll::poseOfFrame(s.model, info.first), chest, cq, ct);
    r.front = s.turn * (cq * x);
    const glm::vec3 spine = level(glmv(b3Body_GetWorldCenter(p.body[chest])) - glmv(b3Body_GetWorldCenter(p.body[0])));
    float roll = 0.f;
    if(front.z * r.front.z < 0.f && za::abs(front.z) > 0.35f && za::abs(r.front.z) > 0.35f && glm::length(spine) > 1e-3f)
    {
        r.rollAxis = glm::normalize(spine);
        r.rollSign = glm::cross(r.rollAxis, front).z * r.front.z > 0.f ? 1.f : -1.f;
        const float angle = za::acos(za::clamp(glm::dot(front, r.front), -1.f, 1.f));
        roll = za::min(angle / rollRate + 0.15f, rollTime);
        r.rollUntil = now + static_cast<double>(roll);
    }
    const float t = t0 + roll; // (the rest after the roll)
    s.clip = Clip{info.first, info.last, static_cast<float>(info.first), now + static_cast<double>(roll + prep), fps, false};
    s.base = 1.f;
    s.death.on = false;
    s.stagger.on = false;
    s.fall.on = false;
    for(Fade& f : s.fade)
    {
        f = Fade{0.25f, 1.f, t, t + 0.5f * prep};
    }
    for(Reach& r : s.reach)
    {
        r.weight = 0.f;
    }
    // (Rolling over: no pin and no support yet, a little tone; then both rising over the preparation.)
    s.support = Fade{0.f, 0.8f, t, t + prep};
    s.pin.force = za::max(vr_ragdoll_active_getup_pin.value, 0.f);
    s.pin.fade = Fade{0.f, 1.f, t, t + 0.7f * prep};
    s.pin.hertz = 5.f;
    // (Its turn's spring is the pelvis's own, mass-normalised: stiff enough to turn the body hanging on it, capped.)
    s.pin.turnHertz = 12.f;
    s.pin.turnShare = 1.f;
    s.pin.offset = glm::vec3{0.f};
    r.on = true;
    r.prep = prep;
    r.end = now + static_cast<double>(roll + prep + static_cast<float>(za::abs(info.last - info.first)) / fps);
    r.polled = now;
    r.lay = massCentre(p);
    r.last = r.lay;
    {
        glm::quat qa, qb;
        glm::vec3 ta, tb;
        ragdoll::bonePose(*p.rig, ragdoll::poseOfFrame(s.model, info.first), 0, qa, ta);
        ragdoll::bonePose(*p.rig, ragdoll::poseOfFrame(s.model, info.last), 0, qb, tb);
        r.clipOwn = glm::length(level(s.turn * ((tb - ta) * p.k)));
    }
    s.on = true;
}

float clipFrame(const State& s, double now)
{
    return s.clip.first < 0 || s.clip.last < 0 ? -1.f : clipPosition(s.clip, now);
}

void lapse(State& s, const Parts& p, double now)
{
    if(!s.on)
    {
        return;
    }
    const glm::vec3 dir = s.stagger.on ? s.stagger.dir : glm::vec3{0.f};
    beginFall(s, p, dir, 0u, 0.f, false, now);
}

void weaken(State& s, float keep)
{
    s.base *= za::clamp(keep, 0.f, 1.f); // (a stagger's pin too: its balance goes with it)
}

void guard(State& s, const glm::vec3& threat, double now, float hold)
{
    Guard& g = s.guard;
    if(!g.on)
    {
        g.from = now;
    }
    g.on = true;
    g.until = now + static_cast<double>(za::max(hold, 0.f));
    g.threat = threat;
}

namespace
{

// A joint group's own share of the drive's spring rate, damping and motor force (vr_ragdoll_active_hertz_legs ... ;
// the ragdoll tool's Joint Groups): 1 each, the global settings alone.
struct GroupGains
{
    float hertz{1.f}, damping{1.f}, torque{1.f};
};
struct GroupGainCvars
{
    cvar_t* hertz;
    cvar_t* damping;
    cvar_t* torque;
};
const GroupGainCvars groupGainCvars[channelCount] = {
    {&vr_ragdoll_active_hertz_legs, &vr_ragdoll_active_damping_legs, &vr_ragdoll_active_torque_legs},
    {&vr_ragdoll_active_hertz_torso, &vr_ragdoll_active_damping_torso, &vr_ragdoll_active_torque_torso},
    {&vr_ragdoll_active_hertz_arms, &vr_ragdoll_active_damping_arms, &vr_ragdoll_active_torque_arms},
    {&vr_ragdoll_active_hertz_head, &vr_ragdoll_active_damping_head, &vr_ragdoll_active_torque_head},
};
// (Read once a drive, not a joint.)
struct GroupGainsNow
{
    za::Array<GroupGains, channelCount> of{};
};
GroupGainsNow groupGainsNow;

void readGroupGains()
{
    for(int ch = 0; ch < channelCount; ch++)
    {
        const GroupGainCvars& v = groupGainCvars[ch];
        groupGainsNow.of[static_cast<za::SizeT>(ch)] = GroupGains{za::clamp(v.hertz->value, 0.f, 4.f),
            za::clamp(v.damping->value, 0.f, 4.f), za::clamp(v.torque->value, 0.f, 4.f)};
    }
}

[[nodiscard]] const GroupGains& groupGains(Channel ch)
{
    return groupGainsNow.of[static_cast<za::SizeT>(ch)];
}
} // namespace

void drive(State& s, const Parts& p, const Context& c)
{
    if(!p.rig)
    {
        return;
    }
    if(!s.on && c.liquidAt && liquidPending(s, c.now)) // (limp now, falling into a liquid: a swim wakes it)
    {
        const Liquid l = enteredLiquid(p, c, swimParts(p));
        if(l != Liquid::None)
        {
            startSwim(s, p, c, l, static_cast<float>(c.now - s.begun));
        }
    }
    if(!s.on)
    {
        if(c.trace)
        {
            trace(s, p, c, static_cast<float>(c.now - c.born)); // (limp: its motion alone)
        }
        return;
    }
    const double t0 = Sys_DoubleTime();
    const ragdoll::Rig& rig = *p.rig;
    const float t = static_cast<float>(c.now - s.begun);
    // A limb a hand holds: no strength (the hand's pull isn't fought), easing back over half a second once let go of.
    // (Struggling: only the held part and what hangs from it; the joints above it in its limb kept at a share.)
    uint32_t partly = 0u;
    const uint32_t slackNow = c.held | (c.now < s.slackUntil ? s.slack : 0u);
    const uint32_t held = s.struggle.on ? struggleSlack(rig, p.count, slackNow, partly) : heldLimbs(rig, p.count, slackNow);
    const float share = za::clamp(vr_struggle_held_share.value, 0.f, 1.f);
    for(int b = 0; b < p.count; b++)
    {
        float& k = s.partScale[static_cast<za::SizeT>(b)];
        k = (held & (1u << b)) ? 0.f : (partly & (1u << b)) ? za::min(share, k + c.dt * 2.f) : za::min(1.f, k + c.dt * 2.f);
    }
    // The clip's pose: each joint's target its turn there (none: as it is now).
    ClipPose pose;
    clipPose(s, p, c.now, pose);
    za::Array<glm::vec3, ragdoll::maxBones> spin{};
    for(int b = 0; b < p.count; b++)
    {
        const int parent = rig.bones[b].parent;
        const auto i = static_cast<za::SizeT>(b);
        if(parent < 0)
        {
            continue;
        }
        if(pose.valid)
        {
            s.animTarget[i] = glm::normalize(glm::inverse(pose.rot[static_cast<za::SizeT>(parent)]) * pose.rot[i]);
            spin[i] = pose.spin[i];
        }
        else if(drivable(s, p, b))
        {
            s.animTarget[i] = glm::normalize(glm::inverse(bodyRot(p, parent)) * bodyRot(p, b));
        }
        s.target[i] = s.animTarget[i];
    }
    // Integration (stages 2-5): a dismemberment is a death's (cutReaction needs one), a stagger, a fall or a get-up a
    // living monster's; should one be cut while still in those (killed in a stagger, then cut), the death leads: they
    // end, so only the balance beat pins his pelvis. A swim leads a dismemberment too (it skips while he swims).
    if(s.dismember.on && (s.stagger.on || s.fall.on || s.rise.on))
    {
        s.stagger.on = false;
        s.fall.on = false;
        s.rise.on = false;
    }
    if(s.dismember.on && s.swim.liquid == Liquid::None)
    {
        dismemberFrame(s, p, c, t); // (before deathFrame: it takes his hands and legs from the wound and the step)
        for(int b = 0; b < p.count; b++)
        {
            spin[static_cast<za::SizeT>(b)] += s.dismember.spin[static_cast<za::SizeT>(b)];
        }
    }
    if(s.death.on)
    {
        deathFrame(s, p, c, t);
    }
    // Integration (stages 2-5): who leads the limbs. A swim (a death in a liquid) leads every limb; else a
    // dismemberment (above), a stagger, a fall or a get-up (stage 2); else what is round him (envFrame, below; it stands
    // down for all of these).
    // A struggle in a player's hands (vr_struggle) leads a lifted one's flail (it stands for it), and stands down in a
    // liquid (the box3d side stops wanting it there).
    const bool struggles = s.struggle.on && struggleFrame(s, p, c, t);
    if(!(struggles && s.swim.liquid == Liquid::Air))
    {
        swimFrame(s, p, c, t); // (it begins one too)
    }
    const bool swimming = s.swim.liquid != Liquid::None;
    if(s.stagger.on && !swimming)
    {
        staggerFrame(s, p, c, t, pose);
    }
    if(s.fall.on && !swimming && !s.death.on && !s.dismember.on)
    {
        livingLedge(s, p, c); // (past a lip: it grabs it, its fall over, a climb)
    }
    if(s.fall.on && !swimming)
    {
        fallFrame(s, p, c, t);
    }
    if(s.guard.on && !swimming)
    {
        guardFrame(s, p, c); // (its arms up before its face, over the stagger's, the fall's or the get-up's)
    }
    // The reaches over the clip's pose (and no speed fed forward where they lead). The limbs envFrame reached last frame
    // are left to it when it leads (below): it applies their reaches itself, each arm solved once a frame (applied here
    // too, last frame's goal first, its blend compounded).
    const bool envLeads = !(swimming || s.dismember.on || s.stagger.on || s.fall.on || s.rise.on || struggles);
    const uint8_t envOwns = envLeads ? s.env.reached : 0;
    for(int limb = 0; limb < limbCount; limb++)
    {
        const Reach& r = s.reach[static_cast<za::SizeT>(limb)];
        if(r.weight <= 0.f || (envOwns & (1u << limb)))
        {
            continue;
        }
        reachTargets(s, p, limb, s.target);
        for(int j = 0; j < 2; j++)
        {
            const int b = s.chain[static_cast<za::SizeT>(limb)][static_cast<za::SizeT>(j)];
            if(b >= 0)
            {
                spin[static_cast<za::SizeT>(b)] *= 1.f - za::clamp(r.weight, 0.f, 1.f);
            }
        }
    }
    if(!envLeads && s.fall.on && !swimming && !s.dismember.on && !s.death.on)
    {
        fallEnv(s, p, c); // (a living fall: a hand may grab another; its other holds let go)
    }
    else if(!envLeads)
    {
        // (what is round him waits, its holds let go)
        envStandDown(s, c,
            struggles       ? "struggling"
            : swimming      ? "swimming"
            : s.dismember.on ? "dismembered"
            : s.rise.on      ? "getting up"
            : s.fall.on      ? "falling"
                             : "staggering");
    }
    else
    {
        envFrame(s, p, c, t, spin); // (what is round him: the hands' goals over the clip's and the death's)
    }
    // Its shoulders' reach (a struggle's hands over its head), before the targets are clamped to the joints' ranges.
    const bool wideArms = shoulderReach(s, p, c.dt);
    // The strengths, and the joints set.
    const float global = za::clamp(vr_ragdoll_active_strength.value, 0.f, 3.f);
    const float hertzAt1 = za::clamp(vr_ragdoll_active_hertz.value, 0.f, maxHertz);
    const float damping = za::clamp(vr_ragdoll_active_damping.value, 0.f, 4.f);
    const float torque = za::max(vr_ragdoll_active_torque.value, 0.f);
    readGroupGains();
    bool any = false;
    for(int b = 1; b < p.count; b++)
    {
        const auto i = static_cast<za::SizeT>(b);
        if(!drivable(s, p, b))
        {
            s.strength[i] = 0.f;
            continue;
        }
        const ragdoll::Bone& bone = rig.bones[b];
        float k = s.base * fadeAt(s.fade[static_cast<za::SizeT>(channelOf(bone))], t);
        for(int limb = 0; limb < limbCount; limb++)
        {
            const Reach& r = s.reach[static_cast<za::SizeT>(limb)];
            const auto& ch = s.chain[static_cast<za::SizeT>(limb)];
            if(r.weight > 0.f && (ch[0] == b || ch[1] == b))
            {
                k = za::max(k, r.strength * za::min(1.f, r.weight * 2.f));
            }
        }
        k = za::clamp(k * global * s.partScale[i], 0.f, 3.f);
        const float was = s.strength[i];
        s.strength[i] = k;
        const b3JointId j = s.joint[i];
        if(k < 0.005f)
        {
            if(was >= 0.005f || t <= c.dt * 1.5f)
            {
                setLimp(s, p, b);
            }
            continue;
        }
        any = true;
        // (the joint's frames: ragpose::jointFrameA, jointFrameB; the authored rig's differ)
        const glm::quat qp = bodyRot(p, bone.parent);
        const GroupGains& gg = groupGains(channelOf(bone)); // (vr_ragdoll_active_*_<group>: 1 each by default)
        // (Struggling, its class's arm drive: its arms' springs stiffer, their cap too: struggleFrame's arms.)
        const float armDrive = s.struggle.on && channelOf(bone) == Channel::Arms ? s.struggle.arms : 1.f;
        const float hertz = za::min(hertzAt1 * gg.hertz * za::sqrt(k) * armDrive, maxHertz * armDrive);
        const float most = s.friction + k * s.cap[i] * torque * gg.torque;
        const float jointDamping = damping * gg.damping;
        if(bone.joint == ragdoll::Joint::Ball)
        {
            const glm::quat q = ragpose::clampSwingTwist(ragpose::toJoint(bone, s.target[i]), s.lower[i] * 0.95f, s.upper[i] * 0.95f);
            b3SphericalJoint_EnableSpring(j, true);
            b3SphericalJoint_SetSpringHertz(j, hertz);
            b3SphericalJoint_SetSpringDampingRatio(j, jointDamping);
            b3SphericalJoint_SetTargetRotation(j, toB3(q));
            b3SphericalJoint_EnableMotor(j, true);
            b3SphericalJoint_SetMotorVelocity(j, b3v(qp * spin[i]));
            b3SphericalJoint_SetMaxMotorTorque(j, most);
        }
        else
        {
            const float lo = za::min(s.lower[i], s.upper[i]) * 0.98f, hi = za::max(s.lower[i], s.upper[i]) * 0.98f;
            const float angle = za::clamp(ragpose::twistZ(ragpose::toJoint(bone, s.target[i])), lo, hi);
            b3RevoluteJoint_EnableSpring(j, true);
            b3RevoluteJoint_SetSpringHertz(j, hertz);
            b3RevoluteJoint_SetSpringDampingRatio(j, jointDamping);
            b3RevoluteJoint_SetTargetAngle(j, angle);
            b3RevoluteJoint_EnableMotor(j, true);
            b3RevoluteJoint_SetMotorSpeed(j, glm::dot(spin[i], glm::normalize(bone.hinge)));
            b3RevoluteJoint_SetMaxMotorTorque(j, most);
        }
    }
    if(s.rise.on && pose.valid && !swimming) // (a swim leads: see above)
    {
        any = true;
        riseFrame(s, p, c, pose);
    }
    // The pelvis pin: its anchor to the clip's pelvis (and a stumble's carried way), its pull fading.
    const float pinNow = s.pin.force > 0.f ? fadeAt(s.pin.fade, t) * s.pin.force * s.base * global : 0.f;
    if(s.pin.force > 0.f && ((pinNow <= 1e-3f && t >= s.pin.fade.t1) || !pose.valid || cut(p, 0)))
    {
        dropPin(s.pin);
    }
    else if(s.pin.force > 0.f && pinNow > 1e-3f)
    {
        any = true;
        glm::vec3 offset{0.f};
        if(s.death.on && s.death.stumble)
        {
            offset = s.death.walk * (stumbleTau * (1.f - za::exp(-t / stumbleTau)));
        }
        if(s.stagger.on)
        {
            offset = s.stagger.lift + s.stagger.dir * (s.stagger.travel * staggerShare(s.stagger, t)) -
                     glm::vec3{0.f, 0.f, staggerDip(s.stagger, t)};
        }
        glm::vec3 at = s.origin + s.turn * (pose.pos[0] * p.k) + offset;
        glm::quat turn = glm::normalize(s.turn * pose.rot[0]);
        if(s.climb.on && s.climb.pinned) // (a climb's: its way's point, turned to face the wall and pitched over)
        {
            // (Its way is its pelvis's middle's: the anchor is its body's origin, that far off it as turned.)
            turn = glm::normalize(glm::slerp(s.climb.rot0, glm::normalize(s.climb.tilt * turn), s.climb.turnBlend));
            at = s.climb.pinAt - turn * glmv(b3Body_GetLocalCenter(p.body[0]));
        }
        const float weight = s.totalMass * c.gravity;
        if(B3_IS_NULL(s.pin.anchor) || !b3Body_IsValid(s.pin.anchor))
        {
            b3BodyDef def = b3DefaultBodyDef();
            def.type = b3_kinematicBody;
            def.position = b3v(at);
            def.rotation = toB3(turn);
            s.pin.anchor = b3CreateBody(c.world, &def);
            b3MotorJointDef jd = b3DefaultMotorJointDef();
            jd.base.bodyIdA = s.pin.anchor;
            jd.base.bodyIdB = p.body[0];
            jd.base.localFrameA = b3Transform{b3Vec3_zero, b3Quat_identity};
            jd.base.localFrameB = b3Transform{b3Vec3_zero, b3Quat_identity};
            jd.base.collideConnected = false;
            jd.linearVelocity = b3Vec3_zero;
            jd.angularVelocity = b3Vec3_zero;
            jd.maxVelocityForce = 0.f;
            jd.maxVelocityTorque = 0.f;
            jd.linearHertz = s.pin.hertz;
            jd.linearDampingRatio = 1.f;
            jd.angularHertz = s.pin.turnHertz > 0.f ? s.pin.turnHertz : s.pin.hertz;
            jd.angularDampingRatio = 1.f;
            jd.maxSpringForce = pinNow * weight;
            jd.maxSpringTorque = pinNow * weight * s.pin.turnShare;
            s.pin.joint = b3CreateMotorJoint(c.world, &jd);
        }
        else
        {
            b3Body_SetTargetTransform(s.pin.anchor, b3WorldTransform{b3v(at), toB3(turn)}, za::max(c.dt, 1e-4f), true);
            b3MotorJoint_SetMaxSpringForce(s.pin.joint, pinNow * weight);
            b3MotorJoint_SetMaxSpringTorque(s.pin.joint, pinNow * weight * s.pin.turnShare);
        }
    }
    // Its weight held up by its muscles (a living one standing or rising): a force up on each part.
    if(const float up = fadeAt(s.support, t) * s.base; up > 1e-3f)
    {
        any = true;
        for(int b = 0; b < p.count; b++)
        {
            if(!cut(p, b))
            {
                pushPart(p, b, glm::vec3{0.f, 0.f, b3Body_GetMass(p.body[b]) * c.gravity * up});
            }
        }
    }
    const bool crawlPending =
        (s.death.on && s.death.crawl) || s.fall.on || s.stagger.on || s.rise.on || s.climb.on || s.struggle.on || wideArms;
    if(any || crawlPending)
    {
        b3Body_SetAwake(p.body[0], true);
    }
    if(c.trace)
    {
        trace(s, p, c, static_cast<float>(c.now - c.born));
        envTrace(s, p, c, static_cast<float>(c.now - c.born));
    }
    if(!any && !crawlPending && t > 0.25f)
    {
        end(s); // (all limp: today's ragdoll)
    }
    const double spent = Sys_DoubleTime() - t0;
    cost.seconds += spent;
    cost.driven++;
    if(cost.lastFrame != c.now)
    {
        cost.lastFrame = c.now;
        cost.frames++;
    }
}

void end(State& s)
{
    shoulderRestore(s); // (its shoulders' cones, widened by a struggle)
    envEnd(s); // (its hands' holds)
    dropPin(s.pin);
    s.on = false;
}

void release(State& s, const Parts& p)
{
    if(s.on && p.rig)
    {
        for(int b = 1; b < p.count; b++)
        {
            if(drivable(s, p, b))
            {
                setLimp(s, p, b);
                s.strength[static_cast<za::SizeT>(b)] = 0.f;
            }
        }
    }
    end(s);
}

void debugDraw(const State& s, const Parts& p)
{
    if(!p.rig)
    {
        return;
    }
    const ragdoll::Rig& rig = *p.rig;
    const float u = p.m2u;
    constexpr float width = 0.2f;
    for(int b = 1; b < p.count; b++)
    {
        if(!drivable(s, p, b))
        {
            continue;
        }
        const ragdoll::Bone& bone = rig.bones[b];
        const auto i = static_cast<za::SizeT>(b);
        const glm::vec3 joint = worldPoint(p, b, bone.pivot) * u;
        const glm::vec3 end = worldPoint(p, b, bone.end) * u;
        const glm::quat qp = bodyRot(p, bone.parent);
        const glm::vec3 target = joint + qp * (s.target[i] * ((bone.end - bone.pivot) * p.k)) * u;
        const float k = za::clamp(s.strength[i], 0.f, 1.f);
        const glm::vec4 colour = glm::mix(glm::vec4{0.55f, 0.55f, 0.55f, 0.6f}, glm::vec4{0.2f, 1.f, 0.3f, 1.f}, k);
        lines::line(joint, end, width * 0.5f, glm::vec4{1.f, 1.f, 1.f, 0.7f}, glm::vec4{1.f, 1.f, 1.f, 0.7f});
        lines::line(joint, target, width, colour, colour);
        lines::point(joint, 0.6f + 0.8f * k, colour);
    }
    if(B3_IS_NON_NULL(s.pin.anchor) && b3Body_IsValid(s.pin.anchor))
    {
        const glm::vec3 a = glmv(b3Body_GetPosition(s.pin.anchor)) * u, pelvis = glmv(b3Body_GetPosition(p.body[0])) * u;
        const glm::vec4 yellow{1.f, 0.9f, 0.1f, 1.f};
        lines::line(a, pelvis, width, yellow, yellow);
        lines::point(a, 1.2f, yellow);
    }
    for(int limb = 0; limb < limbCount; limb++)
    {
        const Reach& r = s.reach[static_cast<za::SizeT>(limb)];
        if(r.weight <= 0.f || s.chain[static_cast<za::SizeT>(limb)][1] < 0)
        {
            continue;
        }
        const glm::vec4 red{1.f, 0.15f, 0.1f, za::clamp(r.weight, 0.3f, 1.f)};
        lines::point(r.goal * u, 1.2f, red);
        lines::line(limbEnd(s, p, limb) * u, r.goal * u, width * 0.5f, red, red);
    }
    envDraw(s, p);
}

bool tracking(const State& s, const Parts& p, float& errorDegrees, float& strength)
{
    errorDegrees = strength = 0.f;
    if(!p.rig || !s.on)
    {
        return false;
    }
    int n = 0;
    for(int b = 1; b < p.count; b++)
    {
        const auto i = static_cast<za::SizeT>(b);
        if(!drivable(s, p, b) || s.strength[i] < 0.005f)
        {
            continue;
        }
        const ragdoll::Bone& bone = p.rig->bones[b];
        const glm::vec3 joint = worldPoint(p, b, bone.pivot);
        const glm::vec3 actual = worldPoint(p, b, bone.end) - joint;
        const glm::vec3 target = bodyRot(p, bone.parent) * (s.target[i] * ((bone.end - bone.pivot) * p.k));
        const float la = glm::length(actual), lt = glm::length(target);
        if(la < 1e-5f || lt < 1e-5f)
        {
            continue;
        }
        errorDegrees += glm::degrees(za::acos(za::clamp(glm::dot(actual, target) / (la * lt), -1.f, 1.f)));
        strength += s.strength[i];
        n++;
    }
    if(n == 0)
    {
        return false;
    }
    errorDegrees /= static_cast<float>(n);
    strength /= static_cast<float>(n);
    return true;
}

void describe(const State& s, const Parts& p, int num, double now)
{
    const float t = static_cast<float>(now - s.begun);
    za::Array<float, channelCount> str{};
    za::Array<int, channelCount> n{};
    for(int b = 1; b < p.count; b++)
    {
        if(drivable(s, p, b))
        {
            const auto ch = static_cast<za::SizeT>(channelOf(p.rig->bones[b]));
            str[ch] += s.strength[static_cast<za::SizeT>(b)];
            n[ch]++;
        }
    }
    for(int ch = 0; ch < channelCount; ch++)
    {
        str[static_cast<za::SizeT>(ch)] /= static_cast<float>(za::max(n[static_cast<za::SizeT>(ch)], 1));
    }
    int held = 0;
    for(int b = 0; b < p.count; b++)
    {
        held += s.partScale[static_cast<za::SizeT>(b)] < 0.5f ? 1 : 0;
    }
    const Death& d = s.death;
    const Stagger& g = s.stagger;
    const char* living = s.rise.on ? (s.rise.done ? ", risen" : ", getting up")
                         : s.fall.on ? (s.fall.landed >= 0.f ? ", fallen" : ", falling")
                         : g.on ? (g.outcome == Outcome::Recovered ? ", staggered: recovered" : ", staggering")
                                : "";
    Con_Printf("  %d: %s %.2f s, strength legs %.2f torso %.2f arms %.2f head %.2f (base %.2f), pin %s, %d parts held "
               "%s%s%s%s%s\n", num,
        s.on ? "driven" : "limp", t, str[0], str[1], str[2], str[3], s.base,
        B3_IS_NON_NULL(s.pin.anchor) ? "on" : "off", held, d.collapse ? ", collapse" : "", d.stumble ? ", stumbled" : "",
        d.woundArm >= 0 ? (d.woundArm == 0 ? ", left hand on the wound" : ", right hand on the wound") : "",
        d.crawl ? (d.crawlFrom >= 0.0 ? ", crawling" : ", to crawl") : "", living);
    if(g.on || g.outcome != Outcome::Going)
    {
        Con_Printf("     stagger: leaning %.0f deg (at most %.0f), pelvis at %.0f%% of its height at least, %d steps, balance "
                   "%.2f\n",
            g.lean, g.leanMost, g.sag * 100.f, g.steps, g.balance);
    }
    const Env& e = s.env;
    if(e.on)
    {
        Con_Printf("    round him: left hand %s%s, right hand %s%s, %s, %d catches, %d holds%s\n", gripName(e.hand[0].mode),
            B3_IS_NON_NULL(e.hand[0].joint) ? " (holding)" : "", gripName(e.hand[1].mode),
            B3_IS_NON_NULL(e.hand[1].joint) ? " (holding)" : "", e.leaned ? "leaned" : "no lean", e.catches, e.holds,
            e.curl > 0.f ? ", curled" : "");
    }
}

void struck(State& s, const Parts& p, b3BodyId part)
{
    if(!s.on || !p.rig)
    {
        return;
    }
    s.struggle.struck = s.struggle.on; // (a struggle flinches: struggleFrame)
    for(int arm = 0; arm < 2; arm++)
    {
        if(s.env.hand[static_cast<za::SizeT>(arm)].mode == Grip::None)
        {
            continue;
        }
        // The arm's two parts, or a part on its lower one (a rig's own hand: the ogre's, the death knight's hand_l/_r,
        // which hold onto the lip and are what a hit there finds).
        const int lo = s.chain[static_cast<za::SizeT>(arm)][1];
        for(int b = 0; b < p.count; b++)
        {
            const bool ofArm = b == s.chain[static_cast<za::SizeT>(arm)][0] || b == lo || (lo >= 0 && p.rig->bones[b].parent == lo);
            if(ofArm && B3_ID_EQUALS(p.body[b], part))
            {
                s.env.hand[static_cast<za::SizeT>(arm)].struck = true;
            }
        }
    }
}

void costSince(double& ms, int& frames, int& driven)
{
    ms = cost.seconds * 1000.0;
    frames = cost.frames;
    driven = cost.driven;
    cost = Cost{};
}

} // namespace qvr::ragactive
