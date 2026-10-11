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
    Con_Printf("  %d: %s %.2f s, strength legs %.2f torso %.2f arms %.2f head %.2f (base %.2f), pin %s, %d parts held "
               "(none)%s%s%s%s\n", num,
        s.on ? "driven" : "limp", t, str[0], str[1], str[2], str[3], s.base,
        B3_IS_NON_NULL(s.pin.anchor) ? "on" : "off", held, d.collapse ? ", collapse" : "", d.stumble ? ", stumbled" : "",
        d.woundArm >= 0 ? (d.woundArm == 0 ? ", left hand on the wound" : ", right hand on the wound") : "",
        d.crawl ? (d.crawlFrom >= 0.0 ? ", crawling" : ", to crawl") : "");
}

void costSince(double& ms, int& frames, int& driven)
{
    ms = cost.seconds * 1000.0;
    frames = cost.frames;
    driven = cost.driven;
    cost = Cost{};
}

} // namespace qvr::ragactive
