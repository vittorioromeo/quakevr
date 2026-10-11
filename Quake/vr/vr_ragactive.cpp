// vr_ragactive.cpp -- active ragdolls (vr_ragactive.hpp; ROUND22.md, "Active ragdolls: core and deaths").

#include "vr_ragactive.hpp"

#include "vr_cvars.hpp"
#include "vr_lines.hpp"
#include "vr_ragpose.hpp"

#include "Zancle/Math/Abs.hpp"
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

void weaken(State& s, float keep)
{
    s.base *= za::clamp(keep, 0.f, 1.f);
}

namespace
{
void dismemberFrame(State& s, const Parts& p, const Context& c, float t); // (below: "Dismemberment")
} // namespace

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
    if(s.dismember.on)
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

// ---- Dismemberment (ROUND22.md, "Active ragdolls: dismemberment"; vr_ragdoll_active_dismember) ----
// A driven death losing a limb (cutReaction, from vr_box3d.cpp ragdollCutLimb: the killing blow's cut, his ragdoll made by
// it from the frame he stood in and driven by his death animation from its start; or a cut on one still dying): his free
// hand to the stump (the cut joint's end on the part it was cut from), stronger and longer than a wound's hold, his torso
// and legs writhing a little for a few seconds once down. On one leg (killed standing by the cut) a balance beat first: his
// pelvis pinned near its height (at most balancePin body weights) while the leg left plants under him, his arms flung out
// and flailing, the pin's anchor drifting to the side lost and his torso bending to it, until his lean passes Balance
// Lean (or the time is up): the pin lets go, his death animation plays from there, he falls. No legs: the crawl on his
// arms (deathFrame's). The head: a collapse, as a headshot. A lost arm never holds anything (its reach stops).

namespace
{

constexpr float holdStrength = 1.f;     // a stump's hold (a wound's: 0.8)
constexpr float holdRamp = 0.25f;       // s: the hand going to it
constexpr float holdLetGo = 0.8f;       // s: and letting go
constexpr float writheRamp = 0.5f;      // s: the writhe eased in
constexpr float writheOut = 1.f;        // s: and out
constexpr float writheTorso = 0.12f;    // rad: its swing in the torso's joints
constexpr float writheLegs = 0.3f;      // rad: in the legs'
constexpr float writheStrength = 0.35f; // their strength while it lasts (a weak stirring)
constexpr float balancePin = 2.f;       // body weights: the pin holding him up on one leg, at most (12 Hz)
constexpr float braceStrength = 2.5f;   // the leg left: its joints bracing him (a dying leg: 1)
constexpr float tipAccel = 0.3f;        // m/s/s: its anchor drifting to the side lost
constexpr float sagDepth = 0.06f;       // m: and sagging
constexpr float tipBend = 0.35f;        // rad: his torso bent to the side lost by the beat's end (over its joints)
constexpr float flailHz = 1.6f;         // his arms' flail for balance
constexpr float pitchRate = 2.5f;       // rad/s: with no legs, killed standing, he pitches forward so

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

// The joint limb `limb` was cut at (a cut bone on it whose parent isn't cut); -1 none: it is whole.
[[nodiscard]] int cutJointOn(const State& s, const Parts& p, int limb)
{
    const int top = s.chain[static_cast<za::SizeT>(limb)][0];
    if(top < 0)
    {
        return -1;
    }
    for(int b = 1; b < p.count; b++)
    {
        const int parent = p.rig->bones[b].parent;
        if(cut(p, b) && parent >= 0 && !cut(p, parent) && onBone(*p.rig, b, top))
        {
            return b;
        }
    }
    return -1;
}

// The stump's end in the world (metres), `off` units out of it.
[[nodiscard]] glm::vec3 stumpPoint(const Dismember& m, const Parts& p, float off)
{
    return worldPoint(p, m.stumpPart, m.stumpAt) + bodyRot(p, m.stumpPart) * m.stumpOut * (off / p.m2u);
}

// His line from `foot` (world, metres) to his pelvis (unit): his lean on it (his pelvis pinned: his chest sways with his
// arms).
[[nodiscard]] glm::vec3 bodyLine(const Parts& p, const glm::vec3& foot)
{
    const glm::vec3 v = glmv(b3Body_GetWorldCenter(p.body[0])) - foot;
    return glm::length(v) > 1e-4f ? glm::normalize(v) : glm::vec3{0.f, 0.f, 1.f};
}

// Once he is down (s since the drive began): the writhe from then, the hold through it.
void planDown(State& s, float down)
{
    Dismember& m = s.dismember;
    const float dur = za::max(vr_ragdoll_active_dismember_time.value, 0.f);
    m.writheFrom = m.writheFrom < 0.f ? down : m.writheFrom;
    m.writheUntil = za::max(m.writheUntil, down + dur);
    m.holdUntil = za::max(m.holdUntil, za::max(m.writheUntil, down + 0.8f * s.death.length));
}

void startBalance(State& s, const Parts& p, const Context& c, float t, int lostLeg, int stand)
{
    Dismember& m = s.dismember;
    m.standLeg = stand;
    // (The side lost: his own, level: the rig's y is his left. A hunched ogre's hips are one ahead of the other.)
    m.lostSide = p.turn * glm::vec3{0.f, lostLeg == 2 ? 1.f : -1.f, 0.f};
    m.origin0 = s.origin;
    m.balanceUntil = t + za::clamp(vr_ragdoll_active_balance_time.value, 0.1f, 5.f);
    // The foot left: kept where it stands, braced (a hop under him lifts what holds him up: he falls at once).
    m.plant = limbEnd(s, p, stand);
    m.line0 = bodyLine(p, m.plant);
    // The pose he stood in, held; every strength full; the pin.
    s.clip = Clip{m.liveFrame, m.liveFrame, static_cast<float>(m.liveFrame), c.now, 10.f, false};
    for(Fade& f : s.fade)
    {
        f = Fade{1.f, 1.f, 0.f, 0.f};
    }
    s.pin.force = balancePin;
    s.pin.hertz = 12.f; // (stiff: its turn holds his pelvis against his torso hanging over the side lost)
    s.pin.fade = Fade{1.f, 0.f, m.balanceUntil, m.balanceUntil + 0.12f};
    s.death.stepLeg = -1;
    s.death.woundArm = -1;
}

// The beat over: the pin lets go, his death animation plays from now (its strengths too); then the hold, the writhe.
void breakBalance(State& s, const Context& c, float t)
{
    Dismember& m = s.dismember;
    m.brokeAt = t;
    s.pin.fade = Fade{1.f, 0.f, t, t + 0.12f};
    s.clip = m.deathClip;
    s.clip.t0 = c.now;
    for(int ch = 0; ch < channelCount; ch++)
    {
        Fade& f = m.deathFade[static_cast<za::SizeT>(ch)];
        f.t0 += t;
        f.t1 += t;
        s.fade[static_cast<za::SizeT>(ch)] = f;
    }
    for(Reach& r : s.reach)
    {
        r.weight = 0.f;
    }
    m.holdFrom = t + 0.1f;
    if(!(s.death.crawl && (m.lost & 12u) == 12u))
    {
        planDown(s, t + 0.8f * s.death.length);
    }
}

void dismemberFrame(State& s, const Parts& p, const Context& c, float t)
{
    Dismember& m = s.dismember;
    const Death& d = s.death;
    if(d.collapse)
    {
        return;
    }
    const ragdoll::Rig& rig = *p.rig;
    const glm::vec3 up{0.f, 0.f, 1.f};
    const float twoPi = 6.2831853f;
    for(glm::vec3& w : m.spin)
    {
        w = glm::vec3{0.f};
    }
    // No legs, killed standing: he pitches forward (his whole body turned about his pelvis).
    if(m.pitch)
    {
        m.pitch = false;
        const glm::vec3 front = bodyRot(p, chestOf(p)) * glm::vec3{1.f, 0.f, 0.f};
        const glm::vec2 level{front.x, front.y};
        const glm::vec3 fwd = glm::length(level) > 1e-3f ? glm::vec3{glm::normalize(level), 0.f} : glm::vec3{1.f, 0.f, 0.f};
        const glm::vec3 w = glm::cross(up, fwd) * pitchRate;
        const glm::vec3 pelvis = glmv(b3Body_GetWorldCenter(p.body[0]));
        for(int b = 0; b < p.count; b++)
        {
            if(!cut(p, b) && rig.bones[b].joint != ragdoll::Joint::Loose)
            {
                const b3BodyId body = p.body[b];
                b3Body_SetAngularVelocity(body, b3v(glmv(b3Body_GetAngularVelocity(body)) + w));
                b3Body_SetLinearVelocity(body,
                    b3v(glmv(b3Body_GetLinearVelocity(body)) + glm::cross(w, glmv(b3Body_GetWorldCenter(body)) - pelvis)));
            }
        }
    }
    // On one leg, keeping his balance.
    if(m.balanceUntil >= 0.f && m.brokeAt < 0.f)
    {
        const int chest = chestOf(p);
        m.lean = glm::degrees(glm::acos(za::clamp(glm::dot(bodyLine(p, limbEnd(s, p, m.standLeg)), m.line0), -1.f, 1.f)));
        if(t >= m.balanceUntil || m.lean > za::max(vr_ragdoll_active_balance_lean.value, 1.f) || !limbWhole(s, p, m.standLeg))
        {
            breakBalance(s, c, t);
            if(vr_debug_ragdoll.value)
            {
                Con_Printf("ragdoll: %d loses his balance after %.2f s, leaning %.0f deg\n", c.num, t, m.lean);
            }
        }
        else
        {
            if(vr_debug_ragdoll.value >= 2.f)
            {
                const glm::vec3 pelvis = glmv(b3Body_GetWorldCenter(p.body[0])) * p.m2u;
                const glm::vec3 foot = limbEnd(s, p, m.standLeg) * p.m2u;
                const glm::vec3 anchor = B3_IS_NON_NULL(s.pin.anchor) && b3Body_IsValid(s.pin.anchor)
                                             ? glmv(b3Body_GetPosition(s.pin.anchor)) * p.m2u
                                             : glm::vec3{0.f};
                Con_Printf("ragdoll: %d balancing %.2f s: lean %.1f deg, pelvis %.1f %.1f %.1f (pin %.1f %.1f %.1f), foot %.1f "
                           "%.1f %.1f (to %.1f %.1f %.1f)\n", c.num, t, m.lean, pelvis.x, pelvis.y, pelvis.z, anchor.x, anchor.y,
                    anchor.z, foot.x, foot.y, foot.z, m.plant.x * p.m2u, m.plant.y * p.m2u, m.plant.z * p.m2u);
            }
            // The pin's anchor drifting to the side lost, sagging a little; his torso bending to it.
            s.origin = m.origin0 + m.lostSide * (0.5f * tipAccel * t * t) - up * (sagDepth * smooth(t / 0.4f));
            int torso = 0;
            for(int b = 1; b < p.count; b++)
            {
                torso += drivable(s, p, b) && channelOf(rig.bones[b]) == Channel::Torso ? 1 : 0;
            }
            // (About the rig's forward axis: +y, his left, is up turned towards it the negative way.)
            const float bend = tipBend * smooth(t / za::max(m.balanceUntil, 0.1f)) / static_cast<float>(za::max(torso, 1)) *
                               (m.standLeg == 3 ? -1.f : 1.f);
            for(int b = 1; b < p.count; b++)
            {
                if(drivable(s, p, b) && channelOf(rig.bones[b]) == Channel::Torso)
                {
                    const auto i = static_cast<za::SizeT>(b);
                    s.target[i] = glm::normalize(glm::angleAxis(bend, glm::vec3{1.f, 0.f, 0.f}) * s.target[i]);
                }
            }
            // The leg left planted, bracing him.
            Reach& leg = s.reach[static_cast<za::SizeT>(m.standLeg)];
            leg.goal = m.plant;
            leg.weight = smooth(t / 0.05f);
            leg.strength = braceStrength;
            // His arms flung out, flailing.
            const glm::quat qc = bodyRot(p, chest);
            for(const int arm : {0, 1})
            {
                if(!limbWhole(s, p, arm))
                {
                    continue;
                }
                const int ua = s.chain[static_cast<za::SizeT>(arm)][0];
                const glm::vec3 shoulder = worldPoint(p, ua, rig.bones[ua].pivot);
                const glm::vec3 out = qc * glm::vec3{0.f, arm == 0 ? 1.f : -1.f, 0.f};
                const glm::vec3 fwd = qc * glm::vec3{1.f, 0.f, 0.f};
                // (The two nearly mirrored: their pull on his chest about cancels.)
                const float ph = twoPi * (flailHz * t + roll(m.seed, 3u)) + (arm == 0 ? 0.f : 0.6f);
                const glm::vec3 dir = glm::normalize(out * 0.75f + up * (0.1f + 0.3f * za::sin(ph)) + fwd * (0.25f * glm::cos(ph)));
                Reach& r = s.reach[static_cast<za::SizeT>(arm)];
                r.goal = shoulder + dir * (0.9f * limbLength(s, p, arm));
                r.weight = smooth(t / 0.4f);
                r.strength = 0.5f;
            }
        }
    }
    const bool balancing = m.balanceUntil >= 0.f && m.brokeAt < 0.f;
    const bool crawling = d.crawlFrom >= 0.0;
    // A hand on the stump.
    float hold = 0.f;
    if(m.holdArm >= 0 && !balancing && !crawling)
    {
        Reach& r = s.reach[static_cast<za::SizeT>(m.holdArm)];
        if(!limbWhole(s, p, m.holdArm) || cut(p, m.stumpPart))
        {
            r.weight = 0.f;
            m.holdArm = -1;
        }
        else
        {
            hold = smooth((t - m.holdFrom) / holdRamp) * (1.f - smooth((t - m.holdUntil) / holdLetGo));
            r.goal = stumpPoint(m, p, handThick);
            r.weight = hold;
            r.strength = holdStrength;
            m.handGap = glm::distance(limbEnd(s, p, m.holdArm), stumpPoint(m, p, 0.f)) * p.m2u;
            if(vr_debug_ragdoll.value >= 2.f)
            {
                const int ua = s.chain[static_cast<za::SizeT>(m.holdArm)][0];
                Con_Printf("ragdoll: %d holding the stump %.2f s: the hand %.1f units off, the stump %.2f arm's lengths from "
                           "the shoulder\n", c.num, t, m.handGap,
                    glm::distance(worldPoint(p, ua, rig.bones[ua].pivot), r.goal) / za::max(limbLength(s, p, m.holdArm), 1e-4f));
            }
        }
    }
    // The writhe: his torso and legs a little strength (over his death's), their targets swung slowly about it.
    float env = 0.f;
    if(!balancing && !crawling && m.writheFrom >= 0.f && m.writheUntil > m.writheFrom)
    {
        env = smooth((t - m.writheFrom) / writheRamp) * (1.f - smooth((t - (m.writheUntil - writheOut)) / writheOut));
        for(const Channel ch : {Channel::Torso, Channel::Legs})
        {
            const auto i = static_cast<za::SizeT>(ch);
            const float v = za::max(fadeAt(m.deathFade[i], t), writheStrength * env);
            s.fade[i] = Fade{v, v, 0.f, 0.f};
        }
        const float w = t - m.writheFrom;
        for(int b = 1; b < p.count && env > 0.f; b++)
        {
            const ragdoll::Bone& bone = rig.bones[b];
            const Channel ch = channelOf(bone);
            if(!drivable(s, p, b) || (ch != Channel::Torso && ch != Channel::Legs))
            {
                continue;
            }
            bool led = false;
            for(int limb = 0; limb < limbCount; limb++)
            {
                const auto& chain = s.chain[static_cast<za::SizeT>(limb)];
                led = led || (s.reach[static_cast<za::SizeT>(limb)].weight > 0.f && (chain[0] == b || chain[1] == b));
            }
            if(led)
            {
                continue;
            }
            const auto ub = static_cast<uint32_t>(b);
            const float amp = (ch == Channel::Legs ? writheLegs : writheTorso) * env;
            const float hz = 0.7f + 0.8f * roll(m.seed, 16u + ub);
            const float phase = twoPi * (hz * w + roll(m.seed, 48u + ub));
            const float angle = amp * za::sin(phase);
            const glm::vec3 axis = bone.joint == ragdoll::Joint::Hinge
                                       ? glm::normalize(bone.hinge)
                                       : glm::normalize(glm::vec3{roll(m.seed, 80u + ub) - 0.5f, 1.f, 0.6f * (roll(m.seed, 112u + ub) - 0.5f)});
            const auto i = static_cast<za::SizeT>(b);
            s.target[i] = glm::normalize(glm::angleAxis(angle, axis) * s.target[i]);
            m.spin[i] = axis * (amp * twoPi * hz * glm::cos(phase));
        }
    }
    if(c.trace)
    {
        // (The tests: the hand from the stump, his lean, the beat (1 on it, 2 broken), when it broke, the hold's weight,
        // the writhe's, how his torso and legs stir (their parts' spin from the pelvis's, mean rad/s), the limbs lost.)
        float stir = 0.f;
        int n = 0;
        const glm::vec3 w0 = glmv(b3Body_GetAngularVelocity(p.body[0]));
        for(int b = 1; b < p.count; b++)
        {
            const Channel ch = channelOf(rig.bones[b]);
            if(drivable(s, p, b) && (ch == Channel::Torso || ch == Channel::Legs))
            {
                stir += glm::length(glmv(b3Body_GetAngularVelocity(p.body[b])) - w0);
                n++;
            }
        }
        Con_Printf("ragdis %d %.3f gap %.1f lean %.1f beat %d %.2f hold %.2f writhe %.2f stir %.2f lost %u\n", c.num,
            static_cast<float>(c.now - c.born), m.handGap, m.lean, balancing ? 1 : (m.brokeAt >= 0.f ? 2 : 0), m.brokeAt, hold,
            env, stir / static_cast<float>(za::max(n, 1)), m.lost);
    }
}

} // namespace

void cutReaction(State& s, const Parts& p, int joint, bool crawler, const Context& c)
{
    if(!s.on || !s.death.on || !p.rig)
    {
        return;
    }
    const ragdoll::Rig& rig = *p.rig;
    Death& d = s.death;
    Dismember& m = s.dismember;
    const float t = static_cast<float>(c.now - s.begun);
    // (What is left of him weighs less: the crawl's pull.)
    s.totalMass = 0.f;
    for(int b = 0; b < p.count; b++)
    {
        s.totalMass += !cut(p, b) && rig.bones[b].joint != ragdoll::Joint::Loose ? b3Body_GetMass(p.body[b]) : 0.f;
    }
    // Beheaded: a collapse, as a headshot (his strength almost none; no pin, hand, step or crawl).
    if(joint == rig.head || d.collapse)
    {
        if(!d.collapse && vr_debug_ragdoll.value)
        {
            Con_Printf("ragdoll: %d beheaded %.2f s into his death: a collapse\n", c.num, t);
        }
        s.base = za::min(s.base, za::clamp(vr_ragdoll_active_headshot.value, 0.f, 1.f));
        d.collapse = true;
        d.stepLeg = d.woundArm = -1;
        d.crawl = false;
        m.on = false;
        for(Reach& r : s.reach)
        {
            r.weight = 0.f;
        }
        dropPin(s.pin);
        return;
    }
    if(vr_ragdoll_active_dismember.value == 0.f)
    {
        return;
    }
    uint32_t lost = 0;
    for(int limb = 0; limb < limbCount; limb++)
    {
        lost |= cutJointOn(s, p, limb) >= 0 ? 1u << limb : 0u;
    }
    if(!lost)
    {
        return;
    }
    const bool first = !m.on;
    if(first)
    {
        const int live = m.liveFrame;
        m = Dismember{};
        m.on = true;
        m.liveFrame = live;
        m.seed = hash(static_cast<uint32_t>(c.num), static_cast<uint32_t>(c.now * 72.0)) ^
                 static_cast<uint32_t>(za::max(vr_ragdoll_active_seed.value, 0.f)) * 40503u;
        m.deathFade = s.fade; // (his death's strengths: the writhe's floor over them)
        m.deathClip = s.clip;
    }
    m.lost = lost;
    const auto isLost = [&](int limb) { return (lost & (1u << limb)) != 0u; };
    const int legsLost = (isLost(2) ? 1 : 0) + (isLost(3) ? 1 : 0);
    // One leg lost as the cut killed him standing: he tries to keep his balance on the other. (A later cut taking that
    // leg ends it.)
    if(first && m.liveFrame >= 0 && legsLost == 1 && t < 0.05f && vr_ragdoll_active_balance_time.value > 0.f &&
        limbWhole(s, p, isLost(2) ? 3 : 2))
    {
        startBalance(s, p, c, t, isLost(2) ? 2 : 3, isLost(2) ? 3 : 2);
    }
    else if(m.balanceUntil >= 0.f && m.brokeAt < 0.f && (legsLost != 1 || !limbWhole(s, p, m.standLeg)))
    {
        breakBalance(s, c, t);
    }
    m.liveFrame = -1;
    const bool balancing = m.balanceUntil >= 0.f && m.brokeAt < 0.f;
    // No legs: he crawls on his arms (his kind crawls, both arms whole) once he lies face down.
    const bool legless = legsLost == 2 && crawler && limbWhole(s, p, 0) && limbWhole(s, p, 1);
    d.crawl = d.crawl || legless;
    if(legless)
    {
        m.writheFrom = m.writheUntil = -1.f; // (his arms crawl; no writhe)
        // No roll onto his front for him (the roll turns his whole body as one; a legless ogre's was thrown 85 units):
        // killed so standing, he pitches forward onto his front, his pose held (no death animation: it falls back).
        d.rollFrom = c.now - 100.0;
        if(t < 0.1f)
        {
            dropPin(s.pin);
            s.clip.first = -1;
            m.pitch = true; // (at his first drive: after the frame's knock is scaled, vr_limbs_body_speed)
        }
    }
    // The stump a hand holds: an arm's (the other hand), else a leg's (the hand on its side, else the other).
    int stumpLimb = -1, arm = -1;
    for(const int limb : {0, 1})
    {
        if(!legless && stumpLimb < 0 && isLost(limb) && limbWhole(s, p, 1 - limb))
        {
            stumpLimb = limb;
            arm = 1 - limb;
        }
    }
    for(const int limb : {2, 3})
    {
        if(legless || stumpLimb >= 0 || !isLost(limb))
        {
            continue;
        }
        const int same = limb - 2;
        arm = limbWhole(s, p, same) ? same : (limbWhole(s, p, 1 - same) ? 1 - same : -1);
        stumpLimb = arm >= 0 ? limb : -1;
    }
    if(arm >= 0)
    {
        const int j = cutJointOn(s, p, stumpLimb);
        const int part = rig.bones[j].parent;
        glm::vec3 mid{0.f};
        for(const glm::vec3& v : rig.bones[part].points)
        {
            mid += v;
        }
        mid /= static_cast<float>(za::max<za::SizeT>(rig.bones[part].points.size(), 1));
        if(m.holdArm != arm || m.stumpPart != part)
        {
            m.holdFrom = balancing ? 1e9f : t + 0.05f;
        }
        m.holdArm = arm;
        m.stumpPart = part;
        // (On its surface, the joint itself is inside it: the part's vertex nearest the joint, leaning towards the
        // holding arm's shoulder and his front, where a hand reaches it without going through him; within 0.75 of that
        // arm's length of its shoulder if any is: an ogre's arms don't reach across his chest.)
        const glm::vec3 shoulder = rig.bones[s.chain[static_cast<za::SizeT>(arm)][0]].pivot;
        const float reach = 0.75f * limbLength(s, p, arm) / za::max(p.k, 1e-6f);
        m.stumpAt = rig.bones[j].pivot;
        float best = 1e30f;
        for(const glm::vec3& v : rig.bones[part].points)
        {
            const float toShoulder = glm::distance(v, shoulder);
            const float score = glm::distance(v, rig.bones[j].pivot) + 0.5f * toShoulder - 0.3f * (v.x - mid.x) +
                                4.f * za::max(toShoulder - reach, 0.f);
            m.stumpAt = score < best ? v : m.stumpAt;
            best = za::min(best, score);
        }
        m.stumpOut = glm::length(m.stumpAt - mid) > 1e-3f ? glm::normalize(m.stumpAt - mid) : glm::vec3{0.f, 0.f, 1.f};
        if(d.woundArm == arm)
        {
            s.reach[static_cast<za::SizeT>(arm)].weight = 0.f;
            d.woundArm = -1; // (the stump first)
        }
    }
    else if(m.holdArm >= 0)
    {
        s.reach[static_cast<za::SizeT>(m.holdArm)].weight = 0.f;
        m.holdArm = -1;
    }
    // Once down (the end of his death animation; after the beat, from its break): the writhe, the hold through it.
    if(!balancing && !legless)
    {
        planDown(s, za::max(t + 0.3f, 0.8f * d.length));
    }
    if(vr_debug_ragdoll.value)
    {
        Con_Printf("ragdoll: %d lost his %s%s%s%s (%s now) %.2f s into his death:%s%s%s%s, writhe %.1f-%.1f s\n", c.num,
            isLost(0) ? "left arm " : "", isLost(1) ? "right arm " : "", isLost(2) ? "left leg " : "", isLost(3) ? "right leg " : "",
            rig.bones[joint].name, t,
            arm >= 0 ? (arm == 0 ? " the left hand to the stump" : " the right hand to the stump") : " no hand to the stump",
            balancing ? ", balancing on one leg" : "", legless ? ", to crawl on his arms" : "", d.woundArm >= 0 ? ", a hand on his wound" : "",
            m.writheFrom, m.writheUntil);
    }
}

int limbJointOf(const ragdoll::Rig& rig, int limb, bool lower)
{
    const bool leg = limb >= 2;
    for(int b = 0; b < rig.numBones; b++)
    {
        const char* n = rig.bones[b].name;
        const bool upper = leg ? std::strncmp(n, "thigh_", 6) == 0 : (std::strncmp(n, "upperarm_", 9) == 0 || std::strncmp(n, "arm_", 4) == 0);
        if(!upper || sideOf(n) != (limb & 1))
        {
            continue;
        }
        if(!lower)
        {
            return b;
        }
        for(int c = 0; c < rig.numBones; c++)
        {
            const char* cn = rig.bones[c].name;
            const bool below = leg ? std::strncmp(cn, "shin_", 5) == 0 : (std::strncmp(cn, "forearm_", 8) == 0 || std::strncmp(cn, "hand_", 5) == 0);
            if(rig.bones[c].parent == b && below)
            {
                return c;
            }
        }
    }
    return -1;
}

} // namespace qvr::ragactive
