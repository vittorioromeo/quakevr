// vr_ragactive.cpp -- active ragdolls (vr_ragactive.hpp; ROUND22.md, "Active ragdolls: core and deaths").

#include "vr_ragactive.hpp"

#include "vr_cvars.hpp"
#include "vr_lines.hpp"
#include "vr_ragpose.hpp"

#include "Zancle/Math/Abs.hpp"
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

void clipPose(const State& s, const Parts& p, double now, ClipPose& out)
{
    const Clip& c = s.clip;
    const ragdoll::Rig& rig = *p.rig;
    if(c.first < 0 || !s.model)
    {
        out.valid = false;
        return;
    }
    float f = c.start + static_cast<float>(now - c.t0) * c.fps;
    const float span = static_cast<float>(c.last - c.first + 1);
    if(c.loop && span > 0.f)
    {
        while(f >= static_cast<float>(c.last + 1))
        {
            f -= span;
        }
    }
    f = za::clamp(f, static_cast<float>(c.first), static_cast<float>(c.last));
    const int fa = static_cast<int>(f);
    const float frac = f - static_cast<float>(fa);
    const int fb = fa < c.last ? fa + 1 : (c.loop ? c.first : c.last);
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
        if(parent < 0 || pa == pb)
        {
            out.spin[i] = glm::vec3{0.f};
            continue;
        }
        const auto pi = static_cast<za::SizeT>(parent);
        const glm::quat relA = glm::inverse(ra[pi]) * ra[i], relB = glm::inverse(rb[pi]) * rb[i];
        out.spin[i] = ragpose::rotationVector(relB * glm::inverse(relA)) * c.fps;
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
            b3Body_SetAwake(p.body[0], true);
            // (About the edge he rolls over, on the floor on the side going down: about his spine the floor holds
            // that side up and takes the turn.)
            glm::vec3 side = glm::cross(up, d.rollAxis);
            if(glm::dot(glm::cross(d.rollAxis, side) * d.rollSign, up) > 0.f)
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
                const float add = (d.rollSign * rollRate - glm::dot(w, d.rollAxis)) * g;
                glm::vec3 arm = glmv(b3Body_GetWorldCenter(p.body[b])) - pivot;
                arm -= d.rollAxis * glm::dot(arm, d.rollAxis);
                b3Body_SetAngularVelocity(p.body[b], b3v(w + d.rollAxis * add));
                b3Body_SetLinearVelocity(p.body[b],
                    b3v(glmv(b3Body_GetLinearVelocity(p.body[b])) + glm::cross(d.rollAxis * add, arm)));
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

// ---- Liquids ----
// (vr_ragactive.hpp, Swim.) Its pelvis, chest and head looked at each frame: a swim begins once its pelvis or chest is
// in water with water still deepEnough over it (not lying in a puddle), or in lava or slime at all, within liquidWindow
// of its death; it stops half a second after all three are out of every liquid. In water: a dog paddle (the hands
// circling ahead of the chest, pulling down and back), a bicycle kick, the torso straight, the head tipped back and held
// up (a lift while it is under) for its first part, all slowing and weakening as it drowns, then limp (it floats face
// down, or sinks, as before). In lava or slime: each hand and foot thrown to a new random place every 0.13-0.18 s, the
// torso arching and twisting, the head thrashing, all strong, fading fast; the burning is the QC's (vr_burning.qc).
constexpr float liquidWindow = 6.f;   // s since the death: a body falling in later is a corpse, it doesn't swim
constexpr float deepEnough = 10.f;    // units of water over the pelvis or chest for a swim to begin
constexpr float headLiftShare = 0.3f; // body weights: the head (and chest) held up at most, while the head is under
constexpr float twoPi = 6.2831853f;

[[nodiscard]] glm::vec3 centre(const Parts& p, int b)
{
    return glmv(b3Body_GetWorldCenter(p.body[b]));
}

// The parts the swim looks at besides the pelvis: the chest and the head (-1: none, or cut off).
struct SwimParts
{
    int chest{-1}, head{-1};
};

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
    return vr_ragdoll_active_liquids.value != 0.f;
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

[[nodiscard]] const char* liquidName(Liquid l)
{
    return l == Liquid::Water ? "water" : l == Liquid::Slime ? "slime" : l == Liquid::Lava ? "lava" : "none";
}

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
    const auto set = [&](Channel ch, float from, float a, float b) {
        s.fade[static_cast<za::SizeT>(ch)] = Fade{from, 0.f, t + a * T, t + b * T};
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
    if(vr_debug_ragdoll.value)
    {
        Con_Printf("ragdoll: %d in %s %.2f s after its death: %s for %.1f s\n", c.num, liquidName(l), t,
            water ? "paddles" : "flails", T);
    }
}

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
    bool in = false;
    bool headIn = false;
    if(c.liquidAt && liquidsOn())
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
    const float hz = water ? 0.6f + 0.7f * v : 2.2f + 1.6f * v;
    w.phase += twoPi * hz * c.dt;
    if(w.phase > 64.f * twoPi)
    {
        w.phase -= 64.f * twoPi;
    }
    const glm::quat qc = bodyRot(p, sp.chest >= 0 ? sp.chest : 0), qp = bodyRot(p, 0);
    const glm::vec3 x{1.f, 0.f, 0.f}, y{0.f, 1.f, 0.f}, z{0.f, 0.f, 1.f};
    // The torso and head (the limbs' bones beyond their reaches: as he stood).
    const float head = water ? -0.6f * (1.f - smooth((ts - 0.3f * T) / (0.4f * T))) : 0.f;
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
                      : glm::angleAxis(0.5f * v * za::sin(twoPi * 2.7f * ts), z) *
                            glm::angleAxis(-0.25f * v * (1.f + za::sin(twoPi * 1.9f * ts)), y);
        }
        else if(ch == Channel::Torso && !water)
        {
            q = glm::angleAxis(0.35f * v * za::sin(twoPi * 2.f * ts), y) * glm::angleAxis(0.3f * v * za::sin(twoPi * 1.6f * ts + 1.3f), x) *
                glm::angleAxis(0.25f * v * za::sin(twoPi * 1.3f * ts + 2.1f), z);
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
            const float hop = 0.13f + 0.05f * roll(w.seed, 10u + static_cast<uint32_t>(limb));
            const float f = ts / hop;
            const auto k = static_cast<uint32_t>(f);
            const glm::vec3 d0 = flailDir(w.seed, limb, k), d1 = flailDir(w.seed, limb, k + 1u);
            const glm::vec3 d = glm::normalize(glm::mix(d0, d1, smooth(f - static_cast<float>(k))) + glm::vec3{1e-4f});
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
        const float keep = 1.f - smooth((ts - 0.4f * T) / (0.3f * T));
        w.headLift = headLiftShare * s.totalMass * c.gravity * keep * s.base;
        if(w.headLift > 0.f)
        {
            const b3Vec3 f{0.f, 0.f, 0.5f * w.headLift};
            b3Body_ApplyForceToCenter(p.body[sp.head], f, true);
            b3Body_ApplyForceToCenter(p.body[sp.chest >= 0 ? sp.chest : 0], f, true);
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
        Con_Printf("ragswim %d %.3f l %d v %.2f head %d %.1f lift %.0f limbs %.2f\n", c.num, static_cast<float>(c.now - c.born),
            static_cast<int>(w.liquid), v, headIn ? 1 : 0, hz2, w.headLift, limbs / static_cast<float>(za::max(n, 1)));
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
constexpr float slipTime = 0.25f;   // s: a hold at its most this long slips off
constexpr float reachTime = 1.f;    // s: a hand not on its surface by then gives up
constexpr float handOff = 1.5f;     // units: a hand's goal off its surface (its thickness)
constexpr float smallThing = 8.f;   // kg: a loose thing lighter is nothing to brace on,
constexpr float smallHeight = 0.4f; // m: or lower (a backpack, a dropped gun)

// The ledges near a falling body (findLedge's list).
za::Vector<int> envNearby;

[[nodiscard]] int chestBone(const Parts& p)
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

[[nodiscard]] const char* gripName(Grip g)
{
    switch(g)
    {
        case Grip::Catch: return "catch";
        case Grip::Brace: return "brace";
        case Grip::Lean: return "lean";
        case Grip::Ledge: return "ledge";
        case Grip::Curl: return "curl";
        default: return "none";
    }
}

[[nodiscard]] const char* armName(int arm)
{
    return arm == 0 ? "left" : "right";
}

[[nodiscard]] glm::vec3 level(const glm::vec3& v)
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

[[nodiscard]] glm::vec3 shoulderOf(const State& s, const Parts& p, int arm)
{
    const int u = s.chain[static_cast<za::SizeT>(arm)][0];
    return worldPoint(p, u, p.rig->bones[u].pivot);
}

// The most a hand's hold takes: Grip body weights (a lean's a third), less for a heavy monster (an ogre slips sooner).
[[nodiscard]] float holdCap(const State& s, const Context& c, Grip g)
{
    const float heavy = za::clamp(za::sqrt(90.f / za::max(s.totalMass, 1.f)), 0.6f, 1.2f);
    return za::max(vr_ragdoll_active_env_grip.value, 0.f) * (g == Grip::Ledge ? 1.f : 1.f / 3.f) * heavy * s.totalMass *
           c.gravity;
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
        const bool busy = h.mode == Grip::Lean || h.mode == Grip::Ledge || h.mode == Grip::Brace;
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
    const bool hand = roll(e.seed, 21u) >= 0.2f && limbWhole(s, p, arm) && arm != d.woundArm;
    if(hand)
    {
        EnvHand& h = e.hand[static_cast<za::SizeT>(arm)];
        const glm::vec3 shoulder = shoulderOf(s, p, arm);
        glm::vec3 sideDir = shoulder - glmv(b3Body_GetWorldCenter(p.body[chestBone(p)]));
        sideDir -= r.n * glm::dot(sideDir, r.n);
        sideDir = glm::length(sideDir) > 1e-4f ? glm::normalize(sideDir) : glm::vec3{0.f};
        h.mode = r.loose ? Grip::Brace : Grip::Lean;
        h.goal = surfaceGoal(p, shoulder, r.at, r.n, glm::vec3{0.f, 0.f, -0.05f * L}, sideDir * (0.3f * L), 0.95f * L);
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
            r.dist * p.m2u, hand ? va("the %s hand on it", armName(arm)) : "no hand");
    }
}

// A ledge's lip near a hand as he falls past it: that hand (and the other, if it reaches the same lip) to its hold.
void findLedge(State& s, const Parts& p, const Context& c, const glm::vec3& pelvis, const glm::vec3& chestAt, float L)
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
                    hu.x, hu.y, hu.z, su.x, su.y, su.z, glm::distance(q, su), 1.15f * L * u, glm::dot(level(chestAt * u - q), E.out));
            }
            // (The hand not far over the lip, the shoulder fallen near it (not standing at the edge), the lip within the
            // arm's reach stretched (the hold pulls the hand the rest of the way), him out over its drop.)
            if(q.z < hu.z - 0.5f * L * u || q.z < su.z - 0.8f * L * u || q.z > su.z + 0.6f * L * u ||
                glm::distance(q, su) > 1.15f * L * u || glm::dot(level(chestAt * u - q), E.out) < 0.3f)
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
        return;
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
            if(glm::distance(E.point(tt), su) > 1.15f * L * u)
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
        // On its way: grabs once near the goal, or near the surface near it (the hold pulls it the rest of the way). A
        // lean's hand that hasn't got there (an arm can't reach far behind him) goes straight to the wall from where
        // it is.
        const float off = glm::dot(hand - h.goal, h.normal);
        const glm::vec3 along = (hand - h.goal) - h.normal * off;
        if(!ledge && c.now - h.since > 0.3)
        {
            h.goal = hand - h.normal * off;
        }
        if(glm::length(hand - h.goal) < 0.25f * L || (off < 0.2f * L && glm::length(along) < 0.5f * L))
        {
            const glm::vec3 at = hand - h.normal * (off - handOff / p.m2u);
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
        const float speed = leanSlide * smooth(static_cast<float>(c.now - h.touched) / 1.2f);
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
    h.strength = ledge ? 0.9f : 0.75f - 0.35f * smooth(static_cast<float>(c.now - h.touched) / 1.5f);
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

// Each frame after the death's own reaches: the hands' modes kept up, the probes every few frames, the curl, and the
// hands' (and a curl's feet's) reaches applied over the targets.
void envFrame(State& s, const Parts& p, const Context& c, float t, za::Array<glm::vec3, ragdoll::maxBones>& spin)
{
    Env& e = s.env;
    const Death& d = s.death;
    if(!e.started)
    {
        e.started = true;
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
    const int chest = chestBone(p);
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
    const bool brace = !done && vr_ragdoll_active_env_brace.value != 0.f;
    const bool lean = !done && vr_ragdoll_active_env_lean.value != 0.f;
    const bool ledge = !done && vr_ragdoll_active_env_ledge.value != 0.f && c.ledges;
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
                h.strength = 0.8f * (1.f - smooth(k)) + 0.1f;
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
            h.strength = 0.8f - 0.6f * smooth((k - 0.4f) / 0.8f);
            if(k > 1.4f)
            {
                letGo(s, arm, &c, "its time");
            }
        }
        else if(h.mode == Grip::Lean || h.mode == Grip::Ledge)
        {
            keepHold(s, p, c, arm, L);
        }
    }
    // The lean: the pelvis drawn weakly to the wall (not a prop's), fading; his knees give (startLean).
    if(e.lean && !done && c.now - e.leanFrom < leanPull)
    {
        const float gap = glm::dot(pelvis - e.wallAt, e.wallN);
        const glm::vec3 to = -level(e.wallN);
        if(gap > 0.2f * L && glm::length(to) > 0.f)
        {
            const float f = 0.25f * s.totalMass * c.gravity * (1.f - smooth(static_cast<float>(c.now - e.leanFrom) / leanPull));
            b3Body_ApplyForceToCenter(p.body[0], b3v(to * f), true);
        }
    }
    // Sense: every few frames.
    if((brace || lean || ledge) && e.frame % envEvery == 1)
    {
        e.probes = 0;
        const bool holding = B3_IS_NON_NULL(e.hand[0].joint) || B3_IS_NON_NULL(e.hand[1].joint);
        const bool reaching = e.hand[0].mode == Grip::Catch || e.hand[1].mode == Grip::Catch;
        // (Still up, for a lean: his chest an arm's length over the floor at least, his pelvis not far over it: on his
        // feet, not falling past a wall.)
        const float under = c.floorBelow ? c.floorBelow(pelvis, 3.f * L) : -1e30f;
        const bool upright = chestAt.z - under > 0.9f * L && pelvis.z - under < 1.6f * L;
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
                else if(wall && lean && upright && !e.leaned && t < leanWithin)
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
            glm::vec3 back = -level(front);
            back = glm::length(back) > 0.f ? back : glm::vec3{1.f, 0.f, 0.f};
            const glm::vec3 dir = glm::angleAxis(static_cast<float>(k) * 1.0471976f, up) * back;
            // (From a little inside his body: his back against the wall, the chest's middle may be past its face.)
            const float in = 0.3f * L;
            const float len = 1.5f * L + in; // (the shoulder is beside the chest: an arm reaches that far from its middle)
            const EnvRay r = envCast(e, p, c, chestAt - dir * in, dir * len);
            if(r.hit && za::abs(r.n.z) < 0.5f && glm::dot(r.n, -dir) > 0.5f)
            {
                const float side = glm::dot(dir, level(left));
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
    const bool airborne = pelvis.z - (c.floorBelow ? c.floorBelow(pelvis, 3.f * L) : -1e30f) > 0.8f * L;
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
        if(e.curl > 0.f && (h.mode == Grip::None || h.mode == Grip::Curl) && limbWhole(s, p, arm))
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
            apply(arm, h.goal, h.weight, h.strength);
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
    // A limb reached last frame and not now: its reach gone (unless the death's own: the wound's hand, a step).
    for(int limb = 0; limb < limbCount; limb++)
    {
        const uint8_t bit = static_cast<uint8_t>(1u << limb);
        if((e.reached & bit) && !(reached & bit) && limb != d.woundArm && limb != d.stepLeg)
        {
            s.reach[static_cast<za::SizeT>(limb)].weight = 0.f;
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

// Integration (ragenv + ragliquid): what is round him is ignored while he swims (swimFrame leads every limb). Each
// frame of a swim: the hands' holds let go (their joints destroyed), no lean or curl, and the limbs left to the swim's
// reaches (envFrame doesn't clear them); envFrame takes up again once the swim stops.
void envSwimming(State& s, const Context& c)
{
    for(int arm = 0; arm < 2; arm++)
    {
        letGo(s, arm, &c, "swimming");
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

bool liquidPending(const State& s, double now)
{
    return liquidsOn() && s.death.on && !s.death.collapse && s.swim.from < 0.0 && now - s.begun <= liquidWindow;
}

void weaken(State& s, float keep)
{
    s.base *= za::clamp(keep, 0.f, 1.f);
}

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
    const uint32_t held = heldLimbs(rig, p.count, c.held);
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
    swimFrame(s, p, c, t);
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
    if(s.swim.liquid != Liquid::None)
    {
        envSwimming(s, c); // (swimming: what is round him waits, its holds let go)
    }
    else
    {
        envFrame(s, p, c, t, spin); // (what is round him: the hands' goals over the clip's and the death's)
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
        if(s.death.stumble)
        {
            offset = s.death.walk * (stumbleTau * (1.f - za::exp(-t / stumbleTau)));
        }
        const glm::vec3 at = s.origin + p.turn * (pose.pos[0] * p.k) + offset;
        const glm::quat turn = glm::normalize(p.turn * pose.rot[0]);
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
            jd.angularHertz = s.pin.hertz;
            jd.angularDampingRatio = 1.f;
            jd.maxSpringForce = pinNow * weight;
            jd.maxSpringTorque = pinNow * weight * 0.25f;
            s.pin.joint = b3CreateMotorJoint(c.world, &jd);
        }
        else
        {
            b3Body_SetTargetTransform(s.pin.anchor, b3WorldTransform{b3v(at), toB3(turn)}, za::max(c.dt, 1e-4f), true);
            b3MotorJoint_SetMaxSpringForce(s.pin.joint, pinNow * weight);
            b3MotorJoint_SetMaxSpringTorque(s.pin.joint, pinNow * weight * 0.25f);
        }
    }
    const bool crawlPending = s.death.on && s.death.crawl;
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
    Con_Printf("  %d: %s %.2f s, strength legs %.2f torso %.2f arms %.2f head %.2f (base %.2f), pin %s, %d parts held "
               "(none)%s%s%s%s\n", num,
        s.on ? "driven" : "limp", t, str[0], str[1], str[2], str[3], s.base,
        B3_IS_NON_NULL(s.pin.anchor) ? "on" : "off", held, d.collapse ? ", collapse" : "", d.stumble ? ", stumbled" : "",
        d.woundArm >= 0 ? (d.woundArm == 0 ? ", left hand on the wound" : ", right hand on the wound") : "",
        d.crawl ? (d.crawlFrom >= 0.0 ? ", crawling" : ", to crawl") : "");
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
    for(int arm = 0; arm < 2; arm++)
    {
        for(int j = 0; j < 2; j++)
        {
            const int b = s.chain[static_cast<za::SizeT>(arm)][static_cast<za::SizeT>(j)];
            if(b >= 0 && B3_ID_EQUALS(p.body[b], part) && s.env.hand[static_cast<za::SizeT>(arm)].mode != Grip::None)
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
