// vr_ragactive.cpp -- active ragdolls (vr_ragactive.hpp; ROUND22.md, "Active ragdolls: core and deaths").

#include "vr_ragactive.hpp"

#include "vr_cvars.hpp"
#include "vr_lines.hpp"
#include "vr_ragpose.hpp"

#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"

#include <cstring>

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
constexpr float stumbleTau = 0.4f;   // s: a stumble's carried motion dies away so
constexpr float stepTime = 0.32f;    // s: a stumble's step
constexpr float strokeTime = 1.1f;   // s: a crawl's stroke (one arm reaching, then pulling)
constexpr float reachShare = 0.45f;  // of a stroke: reaching (then pulling)
constexpr float crawlSpeed = 0.7f;   // m/s: a crawl's pull at most
constexpr float rollTime = 1.6f;     // s: rolling onto his front to crawl, at most
constexpr float rollRate = 4.f;       // rad/s: its turn
constexpr float handThick = 2.f;     // units: a hand on a wound, off its surface

[[nodiscard]] b3Quat toB3(const glm::quat& q)
{
    return b3Quat{b3Vec3{q.x, q.y, q.z}, q.w};
}

[[nodiscard]] glm::quat fromB3(const b3Quat& q)
{
    return glm::normalize(glm::quat{q.s, q.v.x, q.v.y, q.v.z});
}

[[nodiscard]] b3Vec3 b3v(const glm::vec3& v)
{
    return b3Vec3{v.x, v.y, v.z};
}

[[nodiscard]] glm::vec3 glmv(const b3Vec3& v)
{
    return glm::vec3{v.x, v.y, v.z};
}

[[nodiscard]] float smooth(float x)
{
    x = za::clamp(x, 0.f, 1.f);
    return x * x * (3.f - 2.f * x);
}

[[nodiscard]] bool named(const char* name, const char* part)
{
    return std::strstr(name, part) != nullptr;
}

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
           !cut(p, bone.parent) && B3_IS_NON_NULL(j) && b3Joint_IsValid(j);
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
        ragpose::bonePoseLerp(rig, pa, pb, frac, b, out.rot[i], out.pos[i]);
        glm::vec3 unused;
        ragdoll::bonePose(rig, pa, b, ra[i], unused);
        ragdoll::bonePose(rig, pb, b, rb[i], unused);
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
            r.goal = glm::mix(d.stepFrom, d.stepTo, smooth(u)) + up * (0.15f * len * za::sin(3.14159265f * u));
            r.weight = 1.f - smooth((t - stepTime - 0.25f) / 0.35f);
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
            r.weight = smooth((t - 0.1f) / 0.35f) * (1.f - smooth((t - 1.6f * d.length) / (0.6f * d.length)));
            r.strength = 0.8f;
        }
    }
    // Crawling.
    if(!d.crawl)
    {
        return;
    }
    const int chest = [&] {
        for(int b = 0; b < p.count; b++)
        {
            if(p.rig->bones[b].role == modelmeta::BoneRole::Chest && !cut(p, b))
            {
                return b;
            }
        }
        return 0;
    }();
    const glm::vec3 pelvis = glmv(b3Body_GetWorldCenter(p.body[0]));
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
            rollOver(p, c, d.rollAxis, d.rollSign, pelvis, floor);
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
        const float f = along >= crawlSpeed ? 0.f
                                            : za::clamp(0.9f * s.totalMass * c.gravity + s.totalMass * (crawlSpeed - along) / 0.2f, 0.f, most);
        const glm::vec3 force = d.crawlDir * (f * 0.5f);
        b3Body_ApplyForceToCenter(p.body[0], b3v(force), true);
        b3Body_ApplyForceToCenter(p.body[chest], b3v(force), true);
        if(vr_debug_ragdoll.value >= 3.f)
        {
            Con_Printf("ragdoll: %d pulls %.0f N (most %.0f) at %.2f m/s along, the hand %.2f m off its hold\n", c.num, f, most,
                along, glm::distance(limbEnd(s, p, arm), d.planted));
        }
    }
    d.crawled = glm::length(glm::vec2{pelvis - d.crawlStart});
}

// Its chest's part (its pelvis's if none).
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

// Its middle (centre of mass of its parts but loose ones: his gun), world metres; and its motion (m/s) into `vel`.
[[nodiscard]] glm::vec3 massCentre(const Parts& p, glm::vec3* vel = nullptr)
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

// ---- Staggers ----
// A shove's stagger: its pelvis pinned up and carried along the shove (easing out over `time`), its top pushed, its
// weight held up by its muscles (support), its feet planted where they stand (each leg's reach) and stepping after its
// pelvis (a capture step: the foot farthest from where it should stand under the pelvis a step's time on, lifted along an
// arc to there, one at a time, at most staggerStepsMost). Its middle leaning past vr_ragdoll_active_stagger_lean over its
// feet, or its pelvis sagging below staggerSag of its height: it falls. Done travelling and stepping, leaning little and
// nearly still (or after staggerMost s still up): it has recovered.
constexpr float staggerStepTime = 0.22f; // s: a step
constexpr float staggerCatch = 0.12f;    // s: leaning past its limit this long, it falls
constexpr int staggerStepsMost = 4;
constexpr float staggerStepAt = 0.2f;    // of a leg's length: a foot this far from its stance under the capture point steps
constexpr float staggerSag = 0.55f;      // of its pelvis's height: it gave way
constexpr float staggerSteady = 12.f;    // deg: leaning less, it is steady
constexpr float staggerMost = 2.f;       // s: up this long, it has recovered
constexpr float staggerUpright = 8.f;    // rad/s per radian its pelvis is pitched off its stance's, steady (turnWhole)
constexpr float staggerUprightMost = 4.f; // rad/s

// The share of its travel its pin's anchor has gone at `t` (easing out: fast at first).
[[nodiscard]] float staggerShare(const Stagger& g, float t)
{
    const float u = za::clamp(t / za::max(g.time, 0.05f), 0.f, 1.f);
    return 1.f - (1.f - u) * (1.f - u);
}

void staggerFrame(State& s, const Parts& p, const Context& c, float t, const ClipPose& pose)
{
    Stagger& g = s.stagger;
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
    g.lean = glm::degrees(za::atan2(glm::length(level(com - support)), za::max(com.z - footZ, 0.05f)));
    g.leanMost = za::max(g.leanMost, g.lean);
    // (Its pelvis over the floor under it, against how high it stood: its feet's ends may lie below their soles.)
    const float below = c.floorBelow ? c.floorBelow(pelvis, 2.f) : -1e30f;
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
        if(g.stepLeg < 0 && g.steps < staggerStepsMost)
        {
            const float h = za::max(com.z - footZ, 0.1f);
            const glm::vec3 then = level(com) + level(comVel) * za::sqrt(h / za::max(c.gravity, 1.f));
            int best = -1;
            float bestD = 0.f;
            for(int leg = 2; leg < 4; leg++)
            {
                const auto i = static_cast<za::SizeT>(leg - 2);
                if(!limbWhole(s, p, leg))
                {
                    continue;
                }
                const float d = glm::length(level(g.planted[i] - (then + g.stance[i])));
                if(d > bestD)
                {
                    bestD = d;
                    best = leg;
                }
            }
            if(best >= 0 && bestD > staggerStepAt * limbLength(s, p, best))
            {
                const auto i = static_cast<za::SizeT>(best - 2);
                const float len = limbLength(s, p, best);
                const glm::vec3 way = level(then - level(g.planted[i] - g.stance[i]));
                const float wl = glm::length(way);
                glm::vec3 to = level(then + g.stance[i]) + (wl > 1e-3f ? way / wl : g.dir) * (0.06f * len);
                // (As high over the floor there as it is here: a foot's end may lie below its sole, the ogre's 8 units.)
                const glm::vec3 from = g.planted[i];
                to.z = from.z + floorAt(c, glm::vec3{to.x, to.y, from.z}, len, from.z) - floorAt(c, from, len, from.z);
                g.stepLeg = best;
                g.stepFrom = c.now;
                g.stepA = g.planted[i];
                g.stepB = to;
            }
        }
        const float limit = za::clamp(vr_ragdoll_active_stagger_lean.value, 5.f, 80.f);
        const float speed = glm::length(level(glmv(b3Body_GetLinearVelocity(p.body[0]))));
        // (Past its lean a moment, not stepping: a step may still catch it; far past, or sagging, at once.)
        g.over = g.lean > limit && g.stepLeg < 0 ? (g.over < 0.f ? t : g.over) : -1.f;
        if((g.over >= 0.f && t - g.over >= staggerCatch) || g.lean > 1.5f * limit || g.sag < staggerSag)
        {
            g.outcome = Outcome::Fell;
            g.decided = c.now;
            if(vr_debug_ragdoll.value || vr_knockdown_debug.value)
            {
                Con_Printf("stagger: %d fell at %.2f s: leaning %.0f deg (at most %.0f), its pelvis at %.0f%% of its height, %d "
                           "steps\n",
                    c.num, t, g.lean, limit, g.sag * 100.f, g.steps);
            }
            beginFall(s, p, g.dir, 0u, 0.f, false, c.now);
            return;
        }
        if((t > g.time + 0.2f && g.stepLeg < 0 && g.lean < staggerSteady && speed < 0.4f) || t > staggerMost)
        {
            g.outcome = Outcome::Recovered;
            g.decided = c.now;
            if(vr_debug_ragdoll.value || vr_knockdown_debug.value)
            {
                Con_Printf("stagger: %d recovered at %.2f s: leaning %.0f deg (at most %.0f), its pelvis at least %.0f%% of its "
                           "height, %d steps\n",
                    c.num, t, g.lean, g.leanMost, g.sag * 100.f, g.steps);
            }
        }
    }
    // Kept upright (as steady as its balance: the shove's pitch of its pelvis worked off), towards its stance.
    if(pose.valid)
    {
        turnWhole(p, c, glm::normalize(s.turn * pose.rot[0]), staggerUpright * g.balance * s.base,
            staggerUprightMost, 1.f);
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
        // (A foot not stepping is left to its joints and the floor's friction: a reach to where it stood straightens a
        // short-legged one's knees, lifting it off its pin.)
        r.weight = leg == g.stepLeg ? 1.f : 0.f;
        r.strength = 1.f;
        g.planted[i] = leg == g.stepLeg ? g.planted[i] : limbEnd(s, p, leg);
        r.goal = g.planted[i];
        if(leg == g.stepLeg)
        {
            const float len = limbLength(s, p, leg);
            const float u = za::clamp(static_cast<float>(c.now - g.stepFrom) / staggerStepTime, 0.f, 1.f);
            r.goal = glm::mix(g.stepA, g.stepB, smooth(u)) + up * (0.12f * len * za::sin(3.14159265f * u));
            if(u >= 1.f)
            {
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
        const float in = smooth((t - f.since) / fallReach);
        const float out = f.landed < 0.f ? 1.f : 1.f - smooth((t - f.landed - 0.25f) / 0.4f);
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
        Con_Printf("getup: %d %.2f s %s frame %.1f: its pelvis off %.1f %.1f %.1f units, turned off %.0f %.0f %.0f deg\n", c.num,
            static_cast<float>(c.now - s.begun), c.now < r.rollUntil ? "rolling" : c.now < s.clip.t0 ? "pulled in" : "rising",
            pose.framePos, e.x, e.y, e.z, a.x, a.y, a.z);
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
    return f.from + (f.to - f.from) * smooth((t - f.t0) / za::max(f.t1 - f.t0, 1e-4f));
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
    const float pin = za::max(vr_ragdoll_active_pin.value, 0.f);
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
    if(di.wound && roll(di.seed, 1u) < vr_ragdoll_active_wound.value)
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
    d.crawl = di.crawler && roll(di.seed, 2u) < vr_ragdoll_active_crawl.value && limbWhole(s, p, 0) && limbWhole(s, p, 1);
}

void beginStagger(State& s, const Parts& p, const StaggerInfo& info, double now)
{
    const ragdoll::Rig& rig = *p.rig;
    const float t = static_cast<float>(now - s.begun);
    Stagger& g = s.stagger;
    g = Stagger{};
    g.on = true;
    g.dir = glm::length(level(info.dir)) > 1e-3f ? glm::normalize(level(info.dir)) : glm::vec3{1.f, 0.f, 0.f};
    g.balance = za::clamp(info.balance, 0.f, 1.f);
    g.travel = za::max(info.travel, 0.f);
    // (Its travel slowing to none over a few steps' time: 1.25 m over 0.9 s, its first speed 2.8 m/s.)
    g.time = za::clamp(0.35f + 0.45f * g.travel, 0.3f, 1.f);
    s.clip = Clip{info.first, info.last >= info.first ? info.last : info.first, static_cast<float>(info.first), now, 10.f, false};
    s.base = 1.f;
    // Its legs strong (planted, stepping), its torso by its balance, its arms and head with some give.
    s.fade[static_cast<za::SizeT>(Channel::Legs)] = Fade{1.f, 1.f, 0.f, 0.f};
    s.fade[static_cast<za::SizeT>(Channel::Torso)] = Fade{0.5f + 0.5f * g.balance, 0.5f + 0.5f * g.balance, 0.f, 0.f};
    s.fade[static_cast<za::SizeT>(Channel::Arms)] = Fade{0.6f, 0.6f, 0.f, 0.f};
    s.fade[static_cast<za::SizeT>(Channel::Head)] = Fade{0.8f, 0.8f, 0.f, 0.f};
    s.support = Fade{0.3f + 0.65f * g.balance, 0.3f + 0.65f * g.balance, 0.f, 0.f};
    s.pin.force = za::max(vr_ragdoll_active_stagger_pin.value, 0.f) * (0.1f + 0.9f * g.balance);
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
        const float v = v0 * smooth(share * 2.f) + extra * share * share;
        b3Body_SetLinearVelocity(p.body[b], b3v(g.dir * v));
        b3Body_SetAngularVelocity(p.body[b], b3Vec3_zero);
        b3Body_SetAwake(p.body[b], true);
    }
    s.on = true;
    (void)t;
}

void beginFall(State& s, const Parts& p, const glm::vec3& dir, uint32_t slack, float slackTime, bool curl, double now)
{
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
    s.fade[static_cast<za::SizeT>(Channel::Legs)] = Fade{za::min(legs, 0.6f), 0.05f, t, t + 0.25f};
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

void drive(State& s, const Parts& p, const Context& c)
{
    if(!p.rig)
    {
        return;
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
    const uint32_t held = heldLimbs(rig, p.count, c.held | (c.now < s.slackUntil ? s.slack : 0u));
    for(int b = 0; b < p.count; b++)
    {
        float& k = s.partScale[static_cast<za::SizeT>(b)];
        k = (held & (1u << b)) ? 0.f : za::min(1.f, k + c.dt * 2.f);
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
    if(s.death.on)
    {
        deathFrame(s, p, c, t);
    }
    if(s.stagger.on)
    {
        staggerFrame(s, p, c, t, pose);
    }
    if(s.fall.on)
    {
        fallFrame(s, p, c, t);
    }
    // The reaches over the clip's pose (and no speed fed forward where they lead).
    for(int limb = 0; limb < limbCount; limb++)
    {
        const Reach& r = s.reach[static_cast<za::SizeT>(limb)];
        if(r.weight <= 0.f)
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
    // The strengths, and the joints set.
    const float global = za::clamp(vr_ragdoll_active_strength.value, 0.f, 3.f);
    const float hertzAt1 = za::clamp(vr_ragdoll_active_hertz.value, 0.f, maxHertz);
    const float damping = za::clamp(vr_ragdoll_active_damping.value, 0.f, 4.f);
    const float torque = za::max(vr_ragdoll_active_torque.value, 0.f);
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
        const glm::quat frame = ragpose::jointFrame(bone);
        const glm::quat qp = bodyRot(p, bone.parent);
        const float hertz = za::min(hertzAt1 * za::sqrt(k), maxHertz);
        const float most = s.friction + k * s.cap[i] * torque;
        if(bone.joint == ragdoll::Joint::Ball)
        {
            const glm::quat q = ragpose::clampSwingTwist(ragpose::toJoint(frame, s.target[i]), s.lower[i] * 0.95f, s.upper[i] * 0.95f);
            b3SphericalJoint_EnableSpring(j, true);
            b3SphericalJoint_SetSpringHertz(j, hertz);
            b3SphericalJoint_SetSpringDampingRatio(j, damping);
            b3SphericalJoint_SetTargetRotation(j, toB3(q));
            b3SphericalJoint_EnableMotor(j, true);
            b3SphericalJoint_SetMotorVelocity(j, b3v(qp * spin[i]));
            b3SphericalJoint_SetMaxMotorTorque(j, most);
        }
        else
        {
            const float lo = za::min(s.lower[i], s.upper[i]) * 0.98f, hi = za::max(s.lower[i], s.upper[i]) * 0.98f;
            const float angle = za::clamp(ragpose::twistZ(ragpose::toJoint(frame, s.target[i])), lo, hi);
            b3RevoluteJoint_EnableSpring(j, true);
            b3RevoluteJoint_SetSpringHertz(j, hertz);
            b3RevoluteJoint_SetSpringDampingRatio(j, damping);
            b3RevoluteJoint_SetTargetAngle(j, angle);
            b3RevoluteJoint_EnableMotor(j, true);
            b3RevoluteJoint_SetMotorSpeed(j, glm::dot(spin[i], glm::normalize(bone.hinge)));
            b3RevoluteJoint_SetMaxMotorTorque(j, most);
        }
    }
    if(s.rise.on && pose.valid)
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
            offset = s.stagger.lift + s.stagger.dir * (s.stagger.travel * staggerShare(s.stagger, t));
        }
        const glm::vec3 at = s.origin + s.turn * (pose.pos[0] * p.k) + offset;
        const glm::quat turn = glm::normalize(s.turn * pose.rot[0]);
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
                b3Body_ApplyForceToCenter(p.body[b], b3Vec3{0.f, 0.f, b3Body_GetMass(p.body[b]) * c.gravity * up}, true);
            }
        }
    }
    const bool crawlPending = (s.death.on && s.death.crawl) || s.fall.on || s.stagger.on || s.rise.on;
    if(any || crawlPending)
    {
        b3Body_SetAwake(p.body[0], true);
    }
    if(c.trace)
    {
        trace(s, p, c, static_cast<float>(c.now - c.born));
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
               "(none)%s%s%s%s%s\n", num,
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
}

void costSince(double& ms, int& frames, int& driven)
{
    ms = cost.seconds * 1000.0;
    frames = cost.frames;
    driven = cost.driven;
    cost = Cost{};
}

} // namespace qvr::ragactive
