// vr_ragactive.cpp -- active ragdolls (vr_ragactive.hpp; ROUND22.md, "Active ragdolls: core and deaths").

#include "vr_ragactive.hpp"

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
constexpr float stumbleTau = 0.4f;   // s: a stumble's carried motion dies away so
constexpr float stepTime = 0.32f;    // s: a stumble's step
constexpr float strokeTime = 1.1f;   // s: a crawl's stroke (one arm reaching, then pulling)
constexpr float reachShare = 0.45f;  // of a stroke: reaching (then pulling)
constexpr float crawlSpeed = 0.7f;   // m/s: a crawl's pull at most
constexpr float rollTime = 1.6f;     // s: rolling onto his front to crawl, at most
constexpr float rollRate = 4.f;       // rad/s: its turn
constexpr float handThick = 2.f;     // units: a hand on a wound, off its surface

using b3glm::b3v;
using b3glm::fromB3;
using b3glm::glmv;
using b3glm::toB3;

using geom::smooth01;

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
    const glm::quat qp = r.upright ? r.from : bodyRot(p, U.parent), qu = bodyRot(p, u);
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

// The floor under `q` (level), traced down from `top` to `bottom` (vr_flinch 2: from its pelvis's height, the real
// floor under a foot: a stair's step below a hanging foot, not missed; a foot by a riser, not traced from inside the
// step above).
[[nodiscard]] float floorUnder(const Context& c, const glm::vec3& q, float top, float bottom, float fallback)
{
    if(!c.floorBelow)
    {
        return fallback;
    }
    const float f = c.floorBelow(glm::vec3{q.x, q.y, top}, za::max(top - bottom, 0.01f));
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
constexpr float swimThrustDying = 0.05f;  // body weights: a dying paddle's strokes' push at most (at full vigour)
constexpr float swimThrustLiving = 0.2f;  // ... a living knockdown's, towards where it can stand
constexpr float swimArrived = 0.25f;      // m: that near its goal, it only treads water
constexpr float panicTime = 3.f;          // s: a drowning one's frantic strokes ease to its usual over this
constexpr float panicRate = 0.9f;         // ... its strokes' rate times 1 + this at first,
constexpr float panicThrust = 0.5f;       // ... and their push 1 + this

void turnWhole(const Parts& p, const Context& c, const glm::quat& want, float rate, float most, float k); // ("Staggers")
[[nodiscard]] glm::vec3 level(const glm::vec3& v); // (below)

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

[[nodiscard]] const char* liquidName(Liquid l)
{
    return l == Liquid::Water ? "water" : l == Liquid::Slime ? "slime" : l == Liquid::Lava ? "lava" :
           l == Liquid::Air   ? "the air" : "none";
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

// ---- Staggers ----
// A shove's stagger: its pelvis pinned up and carried along the shove (easing out over `time`), its top pushed, its
// weight held up by its muscles (support), its feet planted where they stand (each leg's reach) and stepping after its
// pelvis (a capture step: the foot farthest from where it should stand under the pelvis a step's time on, lifted along an
// arc to there, one at a time, at most staggerStepsMost). Its middle leaning past vr_ragdoll_active_stagger_lean over its
// feet, or its pelvis sagging below staggerSag of its height: it falls. Done travelling and stepping, leaning little and
// nearly still (or after staggerMost s still up): it has recovered.
constexpr float staggerStepTime = 0.22f; // s: a step
constexpr float staggerStepQuickest = 0.13f; // s: a fast backpedal's step
constexpr int staggerCatchSteps = 3;     // steps past its backpedal's to catch itself, still leaning
constexpr float staggerStride = 0.75f;   // of a leg's length: a step lands at most this far (level) from its pelvis then
constexpr float staggerCatch = 0.12f;    // s: leaning past its limit three times this long, it falls
constexpr float staggerFar = 0.1f;       // s: past twice its limit this long, it falls
constexpr float staggerStepAt = 0.2f;    // of a leg's length: a foot this far from its stance under the capture point steps
constexpr float staggerSag = 0.55f;      // of its pelvis's height: it gave way
constexpr float staggerSteady = 12.f;    // deg: leaning less, it is steady
constexpr float staggerMost = 1.2f;      // s: up this long after its travel, it has recovered
constexpr float staggerWheel = 2.f;      // Hz: its arms wheeling for its balance
constexpr float staggerUpright = 8.f;    // rad/s per radian its pelvis is pitched off its stance's, steady (turnWhole)
constexpr float staggerUprightMost = 4.f; // rad/s

// The share of its travel its pin's anchor has gone at `t` (easing out: fast at first).
[[nodiscard]] float staggerShare(const Stagger& g, float t)
{
    const float u = za::clamp(t / za::max(g.time, 0.05f), 0.f, 1.f);
    return 1.f - (1.f - u) * (1.f - u);
}

// Its spine's tilt off the vertical (deg): its pelvis's middle to its chest's.
[[nodiscard]] float spineTilt(const Parts& p)
{
    const glm::vec3 spine = glmv(b3Body_GetWorldCenter(p.body[chestOf(p)])) - glmv(b3Body_GetWorldCenter(p.body[0]));
    return glm::degrees(za::atan2(glm::length(level(spine)), za::max(spine.z, 1e-3f)));
}

// A flinch's recovery test (vr_flinch; FLINCH.md, "Recovery test"): its middle over its feet's support (each foot a
// disc of footR round its end, and the strip between them), its middle nearly still, its spine nearly as upright as it
// stood when struck, no step under way, and (asked last: two rays) each foot on firm ground no steeper than
// vr_flinch_recover_slope. Fills in g's measures; true if all hold.
[[nodiscard]] bool flinchSteady(Stagger& g, const State& s, const Parts& p, const Context& c, const glm::vec3& com,
    const glm::vec3& comVel, const ClipPose& pose)
{
    const glm::vec3 up{0.f, 0.f, 1.f};
    za::Array<glm::vec2, 2> foot{};
    int n = 0;
    for(int leg = 2; leg < 4; leg++)
    {
        if(limbWhole(s, p, leg))
        {
            const glm::vec3 f = limbEnd(s, p, leg);
            foot[static_cast<za::SizeT>(n++)] = glm::vec2{f.x, f.y};
        }
    }
    const glm::vec2 m{com.x - g.supportOffset.x, com.y - g.supportOffset.y}; // (its stance's own lean aside)
    float d = 1e30f;
    if(n == 2)
    {
        const glm::vec2 ab = foot[1] - foot[0];
        const float l2 = glm::dot(ab, ab);
        const float u = l2 > 1e-8f ? za::clamp(glm::dot(m - foot[0], ab) / l2, 0.f, 1.f) : 0.f;
        d = glm::length(m - (foot[0] + ab * u));
    }
    else if(n == 1)
    {
        d = glm::length(m - foot[0]);
    }
    g.margin = g.footR - d;
    g.speedNow = glm::length(level(comVel));
    // Its pelvis's and chest's tilt off how its stance frame holds them (their up as the frame has it; the yaw aside).
    g.tilt = 0.f;
    for(const int b : {0, chestOf(p)})
    {
        if(!pose.valid)
        {
            g.tilt = za::max(spineTilt(p) - g.tilt0, 0.f);
            break;
        }
        const glm::vec3 local = glm::inverse(glm::normalize(s.turn * pose.rot[static_cast<za::SizeT>(b)])) * up;
        const glm::vec3 now = bodyRot(p, b) * local;
        g.tilt = za::max(g.tilt, glm::degrees(za::acos(za::clamp(now.z, -1.f, 1.f))));
    }
    g.failed = 0u;
    g.failed |= g.margin < 0.f ? 1u : 0u;
    g.failed |= g.speedNow > za::max(vr_flinch_recover_speed.value, 0.01f) ? 2u : 0u;
    g.failed |= g.tilt > za::max(vr_flinch_recover_tilt.value, 1.f) ? 4u : 0u;
    g.failed |= g.stepLeg >= 0 ? 16u : 0u;
    if(g.failed == 0u && c.rayHit)
    {
        const float flattest = za::cos(glm::radians(za::clamp(vr_flinch_recover_slope.value, 0.f, 89.f)));
        // Each foot over floor within a stair and a half (its end over a stair's edge, its sole on the stair above),
        // none steeper than the slope allowed, and one of them on it (within flinchFirm).
        constexpr float above = 0.08f, below = 0.75f, flinchFirm = 0.3f; // m
        g.ground = true;
        bool firm = false;
        for(int leg = 2; leg < 4; leg++)
        {
            if(!limbWhole(s, p, leg))
            {
                continue;
            }
            const glm::vec3 f = limbEnd(s, p, leg);
            glm::vec3 at{0.f}, normal{0.f};
            // (From just over it: under a stair's riser.)
            float share = c.rayHit(f + up * above, f - up * below, at, normal);
            // (The physics world's floor missed (a thin brush): the floor's own height below, level enough for all we
            // know.)
            if(share >= 1.f && c.floorBelow)
            {
                const float z = c.floorBelow(f + up * above, above + below);
                share = z > -1e29f ? (f.z + above - z) / (above + below) : 1.f;
                normal = up;
            }
            // (Neither saw it: Quake's own world, as its box stands on it.)
            if(share >= 1.f && c.traceHit)
            {
                float normalUp = 1.f;
                share = c.traceHit(f + up * above, f - up * below, normalUp);
                normal = glm::vec3{0.f, 0.f, normalUp};
            }
            if(share >= 1.f || normal.z < flattest)
            {
                g.groundShare = share;
                g.groundAt = f;
                g.groundUp = normal.z;
            }
            g.ground = g.ground && share < 1.f && normal.z >= flattest;
            firm = firm || (share < 1.f && share * (above + below) - above <= flinchFirm);
        }
        g.ground = g.ground && firm;
        g.failed |= g.ground ? 0u : 8u;
    }
    return g.failed == 0u;
}

// ---- vr_flinch 2: balance from the legs (FLINCH.md, "Balance from the legs") ----
// No pin and no lift on every part. Its weight is borne only while a foot is on the floor (a push up its legs, holding
// its pelvis as high over its feet as it stood, never a pull down): struck up, it falls back at once. Its centre of
// pressure (where the floor pushes back) stays within its bearing feet: within them it draws its capture point back over
// its feet (its ankles), past them it cannot, and it steps where its capture point will be as the foot lands.
constexpr float legsCop = 0.6f;         // of Foot Support: each bearing foot's reach for its centre of pressure (its sole)
constexpr float legsStepPast = 0.06f;   // m: its capture point this far past that reach: a step
constexpr float legsBears = 0.15f;      // of its leg: a foot's end this little over its floor (as it stood) bears (a shin's tilt
                                        // lifts its end)
constexpr float legsHertz = 3.f;        // its legs' hold on its pelvis's height over its feet
constexpr float legsPushMost = 1.6f;    // its weights: its legs' push up at most
constexpr float legsDrawn = 1.f;        // its capture point drawn back over its feet's middle at this times its pace
constexpr float legsUpright = 8.f;      // rad/s per radian its pelvis is off its stance's (its hips over its feet)
constexpr float legsUprightMost = 4.f;  // rad/s
constexpr float legsStance = 0.5f;      // a bearing leg's tone (its reach's strength)
constexpr float legsStepSpeed = 0.25f;  // m/s: slower, it steps only once its capture point is past Foot Support
constexpr float legsLow = 0.4f;         // of its leg: with no foot within Bears, its lowest within this bears
constexpr float legsOvershoot = 0.1f;   // of its leg: a step lands this far past its capture point
constexpr float legsStride = 0.75f;     // of its leg: a step lands at most this far (level) from its hip then
constexpr float legsSwingMost = 8.f;    // m/s: a stepping foot carried at most this fast
constexpr float legsSwingGrip = 0.6f;   // of the way to that, a frame
constexpr float legsSwingClear = 0.08f; // of its leg: a stepping foot kept this far over the floor on its way
constexpr float legsHang = 1.f;         // of its leg: a foot struck hanging this far over its floor bears where it stood

// The point within the capsule round segment a..b of radius r nearest q (level); `past`: how far q lies outside it.
[[nodiscard]] glm::vec2 inCapsule(const glm::vec2& q, const glm::vec2& a, const glm::vec2& b, float r, float& past)
{
    const glm::vec2 ab = b - a;
    const float l2 = glm::dot(ab, ab);
    const float u = l2 > 1e-8f ? za::clamp(glm::dot(q - a, ab) / l2, 0.f, 1.f) : 0.f;
    const glm::vec2 on = a + ab * u;
    const glm::vec2 d = q - on;
    const float l = glm::length(d);
    past = za::max(l - r, 0.f);
    return l > r ? on + d * (r / l) : q;
}

// turnWhole's turn on all but its legs' parts (`legs`, bits): its legs are left to its feet (their reaches).
void turnUpper(const Parts& p, const Context& c, uint32_t legs, const glm::quat& want, float rate, float most, float k)
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
    // (A turn only: their motion as a whole kept, the hit's travel not taken back by it.)
    glm::vec3 moved{0.f};
    float mass = 0.f;
    for(int b = 0; b < p.count; b++)
    {
        if(!cut(p, b) && (legs & (1u << b)) == 0u)
        {
            const float m = b3Body_GetMass(p.body[b]);
            moved += glm::cross(add, glmv(b3Body_GetWorldCenter(p.body[b])) - pivot) * m;
            mass += m;
        }
    }
    moved /= za::max(mass, 1e-3f);
    for(int b = 0; b < p.count; b++)
    {
        if(cut(p, b) || (legs & (1u << b)))
        {
            continue;
        }
        b3Body_SetAngularVelocity(p.body[b], b3v(glmv(b3Body_GetAngularVelocity(p.body[b])) + add));
        b3Body_SetLinearVelocity(p.body[b], b3v(glmv(b3Body_GetLinearVelocity(p.body[b])) +
                                                glm::cross(add, glmv(b3Body_GetWorldCenter(p.body[b])) - pivot) - moved));
    }
}

// A frame of it: which feet bear, its legs' push, its centre of pressure's force and its upright. Fills in g.bearing,
// g.bearAt, g.capture and g.captureOff (staggerFrame's steps).
void legsBalance(Stagger& g, const State& s, const Parts& p, const Context& c, const glm::vec3& com, const glm::vec3& comVel,
    const ClipPose& pose)
{
    const glm::vec3 pelvis = glmv(b3Body_GetWorldCenter(p.body[0]));
    const bool first = g.pelvisOver < 0.f;
    float baseSum = 0.f, feetEnd = 0.f;
    za::Array<float, 2> gapNow{};
    float lowGap = 1e30f, lowBase = 0.f;
    glm::vec3 lowAt{0.f};
    int n = 0, whole = 0;
    for(int leg = 2; leg < 4; leg++)
    {
        const auto i = static_cast<za::SizeT>(leg - 2);
        if(!limbWhole(s, p, leg))
        {
            continue;
        }
        const glm::vec3 f = limbEnd(s, p, leg);
        const float len = limbLength(s, p, leg);
        const float floor = floorAt(c, f, 0.3f, f.z - g.footGap[i]);
        // (As it stood: an ogre's foot's end 8 units below its sole; on a slope its downhill foot over the floor, its
        // stand frame's feet level.)
        if(first)
        {
            g.footGap[i] = za::clamp(f.z - floor, -0.25f, za::max(0.3f, legsHang * len));
            feetEnd += f.z;
            whole++;
        }
        gapNow[i] = f.z - floor - g.footGap[i];
        if(leg != g.stepLeg && gapNow[i] < legsBears * len)
        {
            g.bearAt[static_cast<za::SizeT>(n++)] = f;
            baseSum += floor + g.footGap[i];
        }
        // (None within that: its lowest foot not stepping still bears unless its pelvis is going up (struck up, a
        // jump): a shin's tilt as its middle runs on over the foot lifts the foot's end.)
        else if(leg != g.stepLeg && gapNow[i] < legsLow * len && gapNow[i] < lowGap)
        {
            lowGap = gapNow[i];
            lowAt = f;
            lowBase = floor + g.footGap[i];
        }
    }
    if(n == 0 && lowGap < 1e29f && glmv(b3Body_GetLinearVelocity(p.body[0])).z < 0.3f &&
       pelvis.z < lowBase + g.pelvisOver + 0.05f)
    {
        g.bearAt[static_cast<za::SizeT>(n++)] = lowAt;
        baseSum = lowBase;
    }
    if(first)
    {
        g.pelvisOver = whole > 0 ? za::max(pelvis.z - feetEnd / static_cast<float>(whole), 0.1f) : g.height;
    }
    g.bearing = n;
    g.capture = level(com);
    g.captureOff = 0.f;
    if(n == 0)
    {
        if(vr_flinch_debug.value >= 4.f)
        {
            Con_Printf("legs: %d %.2f s, in the air: its pelvis %.1f over as it stood, feet %.1f %.1f up, step %d (%d done), "
                       "lean %.0f\n",
                c.num, static_cast<float>(c.now - s.begun), (pelvis.z - g.pelvis0.z) * p.m2u, gapNow[0] * p.m2u,
                gapNow[1] * p.m2u, g.stepLeg, g.steps, g.lean);
        }
        return; // (in the air: gravity alone)
    }
    const float gz = za::max(c.gravity, 1.f);
    const float baseZ = baseSum / static_cast<float>(n);
    // Its legs' push: holding its pelvis as high over its feet as it stood (critically damped), never pulling it down.
    const float w = 2.f * 3.14159265f * legsHertz;
    const float vz = glmv(b3Body_GetLinearVelocity(p.body[0])).z;
    const float push = za::clamp(1.f + (w * w * (baseZ + g.pelvisOver - pelvis.z) - 2.f * w * vz) / gz, 0.f, legsPushMost);
    // Its centre of pressure: towards drawing its capture point back over its feet, kept within their soles.
    const float h = za::max(com.z - baseZ, 0.2f);
    const float omega = za::sqrt(gz / h);
    const glm::vec2 off{g.supportOffset.x, g.supportOffset.y}; // (its stance's own lean aside)
    const glm::vec2 x = glm::vec2{com.x, com.y} - off;
    const glm::vec2 xi = x + glm::vec2{comVel.x, comVel.y} / omega;
    const glm::vec2 a{g.bearAt[0].x, g.bearAt[0].y};
    const glm::vec2 b = n == 2 ? glm::vec2{g.bearAt[1].x, g.bearAt[1].y} : a;
    const float r = legsCop * g.footR;
    float past = 0.f;
    (void)inCapsule(xi, a, b, r, past);
    g.captureOff = past;
    g.capture = glm::vec3{xi.x + off.x, xi.y + off.y, 0.f};
    float unused = 0.f;
    const glm::vec2 cop = inCapsule(xi + (xi - 0.5f * (a + b)) * legsDrawn, a, b, r, unused);
    // (Its whole body's: on the parts its legs bear, by their mass.)
    const glm::vec2 acc = (x - cop) * (omega * omega) * (za::max(s.totalMass, g.upperMass) / g.upperMass);
    for(int b2 = 0; b2 < p.count; b2++)
    {
        if(cut(p, b2) || (g.legParts & (1u << b2)))
        {
            continue;
        }
        const float m = b3Body_GetMass(p.body[b2]);
        pushPart(p, b2, glm::vec3{acc.x * m, acc.y * m, m * gz * push});
    }
    if(vr_flinch_debug.value >= 4.f)
    {
        const glm::vec2 mid = 0.5f * (a + b);
        const glm::vec2 dir{g.dir.x, g.dir.y};
        Con_Printf("legs: %d %.2f s, %d bearing, push %.2f, middle %.1f along %.1f up, speed %.2f along, capture %.1f along "
                   "(%.1f past), pressure %.1f along, feet %.1f apart along %.1f, step %d (%d done), lean %.0f, legs %x, "
                   "pose %d, off upright %.0f deg; at %.1f, com %.1f, foot %.1f %.1f\n",
            c.num, static_cast<float>(c.now - s.begun), n, push, glm::dot(x - mid, dir) * p.m2u, (com.z - baseZ) * p.m2u,
            glm::dot(glm::vec2{comVel.x, comVel.y}, dir), glm::dot(xi - mid, dir) * p.m2u, past * p.m2u,
            glm::dot(cop - mid, dir) * p.m2u, glm::length(b - a) * p.m2u, glm::dot(b - a, dir) * p.m2u, g.stepLeg, g.steps,
            g.lean, g.legParts, pose.valid ? 1 : 0,
            pose.valid ? glm::degrees(glm::length(ragpose::rotationVector(glm::normalize(s.turn * pose.rot[0]) *
                                                                          glm::inverse(bodyRot(p, 0)))))
                       : -1.f,
            glm::dot(glm::vec2{pelvis.x, pelvis.y}, dir) * p.m2u, glm::dot(glm::vec2{com.x, com.y}, dir) * p.m2u,
            glm::dot(glm::vec2{limbEnd(s, p, 2).x, limbEnd(s, p, 2).y}, dir) * p.m2u,
            glm::dot(glm::vec2{limbEnd(s, p, 3).x, limbEnd(s, p, 3).y}, dir) * p.m2u);
        if(vr_flinch_debug.value >= 5.f && c.now - s.begun < 0.06)
        {
            for(int b2 = 0; b2 < p.count; b2++)
            {
                const glm::vec3 v = glmv(b3Body_GetLinearVelocity(p.body[b2]));
                const glm::vec3 at = glmv(b3Body_GetWorldCenter(p.body[b2]));
                Con_Printf("legs: body %d (%s) parent %d at %.1f along %.1f up, v %.2f along %.2f up, mass %.1f%s\n", b2,
                    p.rig->bones[b2].name, p.rig->bones[b2].parent, glm::dot(glm::vec2{at.x, at.y}, dir) * p.m2u,
                    at.z * p.m2u, glm::dot(glm::vec2{v.x, v.y}, dir), v.z, b3Body_GetMass(p.body[b2]),
                    (g.legParts & (1u << b2)) ? " leg" : "");
            }
        }
    }
    // Upright over its feet, as its hips and ankles hold it: the less, the farther its capture point is past their reach.
    if(pose.valid)
    {
        const float hold = 1.f - 0.5f * smooth01(past / za::max(2.f * g.footR, 0.05f));
        const float k = za::max(vr_flinch_legs_upright.value, 0.f);
        turnUpper(p, c, 0u, glm::normalize(s.turn * pose.rot[0]), legsUpright * k * s.base, legsUprightMost * k, hold);
    }
}

// A flinch's measures as it ends (vr_flinch_debug): the `flinch: <num> result` line (the tests' scripts read it).
void flinchResult(const Stagger& g, const Parts& p, const Context& c, float t, const char* what)
{
    if(g.flinch && vr_flinch_debug.value)
    {
        Con_Printf("flinch: %d result %s at %.2f s: %d steps, along the hit %.2f, stepped %.0f units, rise %.1f units, "
                   "legs %d\n",
            c.num, what, t, g.steps, g.steps > 0 ? g.stepAlong / static_cast<float>(g.steps) : 0.f, g.stepWay * p.m2u,
            g.riseMost * p.m2u, g.legs ? 1 : 0);
    }
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
    support += g.flinch ? g.supportOffset : glm::vec3{0.f}; // (a flinch: where its stance's middle stands)
    footZ = feet > 0 ? footZ : pelvis.z - g.height;
    g.lean = glm::degrees(za::atan2(glm::length(level(com - support)), za::max(com.z - footZ, 0.05f)));    g.leanMost = za::max(g.leanMost, g.lean);
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
    g.riseMost = za::max(g.riseMost, com.z - g.comZ0);
    if(g.legs && g.outcome == Outcome::Going)
    {
        legsBalance(g, s, p, c, com, comVel, pose);
    }
    if(g.outcome == Outcome::Going)
    {
        // A step: the foot farthest from its stance under its middle's capture point (where its middle comes to rest over
        // a base: its level motion times the square root of its height over g, ahead of it).
        // (Beaten, its steps taken: no more, it gives way below. Steady, its backpedal's steps taken and still leaning: a
        // few more to catch itself.)
        // (A flinch steps only while its capture point is off its support (below): a few more to come to rest over its
        // feet, not left standing split and off them once its backpedal's are spent.)
        constexpr int flinchExtraSteps = 4;
        const int stepsCap = g.giveAfter >= 0 ? g.giveAfter
                                              : g.stepsMost + (g.lean > staggerSteady ? staggerCatchSteps : 0) +
                                                    (g.flinch ? flinchExtraSteps : 0);
        // vr_flinch 2: a step once its capture point is past its bearing feet's reach for its centre of pressure, the
        // foot put where its stance has it round where its capture point will be as it lands (its middle running on
        // over the other foot meanwhile), within its stride.
        // (Nearly still and only a little past: its ankles settle it; no stepping in place.)
        if(g.legs && g.stepLeg < 0 && g.steps < stepsCap && g.bearing > 0 && g.captureOff > legsStepPast &&
           (glm::length(level(comVel)) > legsStepSpeed || g.captureOff > g.footR))
        {
            const float h = za::max(com.z - footZ, 0.1f);
            const float omega = za::sqrt(za::max(c.gravity, 1.f) / h);
            const float stepTime = za::clamp(staggerStepTime - 0.03f * glm::length(level(comVel)) -
                                                 0.004f * za::max(g.lean - staggerSteady, 0.f),
                staggerStepQuickest, staggerStepTime);
            // Each foot's step: to its capture point as it lands (past it a little, half its stance's width aside),
            // the other foot bearing meanwhile. Not across the other foot (a sidestep, not a cross-over); of those, the
            // foot farther from its spot (a backpedal's feet take turns).
            const glm::vec3 left = s.turn * glm::vec3{0.f, 1.f, 0.f};
            int best = -1;
            float bestD = -1.f;
            glm::vec3 bestTo{0.f};
            for(int leg = 2; leg < 4; leg++)
            {
                const auto i = static_cast<za::SizeT>(leg - 2);
                if(!limbWhole(s, p, leg))
                {
                    continue;
                }
                const int other = 5 - leg;
                const bool two = limbWhole(s, p, other);
                const glm::vec3 otherAt = level(g.planted[static_cast<za::SizeT>(two ? other - 2 : i)]);
                const glm::vec3 stand = two ? otherAt : level(com - g.supportOffset);
                const glm::vec3 now = level(g.capture - g.supportOffset);
                const glm::vec3 then = stand + (now - stand) * za::exp(omega * stepTime);
                const glm::vec3 run = then - stand;
                const float rl = glm::length(run);
                const glm::vec3 m = rl > 1e-3f ? run / rl : g.dir;
                const glm::vec3 side = g.stance[i] - m * glm::dot(g.stance[i], m);
                const float len = limbLength(s, p, leg);
                const glm::vec3 to = then + m * (legsOvershoot * len) + side * 0.5f;
                const float was = glm::dot(level(g.planted[i]) - otherAt, left);
                const bool crosses = two && was * glm::dot(to - otherAt, left) < 0.f &&
                                     za::abs(glm::dot(to - otherAt, left)) > 0.05f * len;
                const float d = glm::length(level(g.planted[i]) - to) - (crosses ? 1e3f : 0.f);
                if(d > bestD)
                {
                    bestD = d;
                    best = leg;
                    bestTo = to;
                }
            }
            if(best >= 0)
            {
                const auto i = static_cast<za::SizeT>(best - 2);
                const float len = limbLength(s, p, best);
                glm::vec3 to = bestTo;
                const glm::vec3 hipThen = level(pelvis) + level(comVel) * stepTime;
                const glm::vec3 off = to - hipThen;
                const float offL = glm::length(off);
                if(offL > legsStride * len)
                {
                    to = hipThen + off * (legsStride * len / offL);
                }
                const glm::vec3 from = g.planted[i];
                to.z = from.z + floorAt(c, glm::vec3{to.x, to.y, from.z}, len, from.z) - floorAt(c, from, len, from.z);
                g.stepLeg = best;
                g.stepFrom = c.now;
                g.stepTime = stepTime;
                g.stepA = g.planted[i];
                g.stepB = to;
                if(vr_flinch_debug.value >= 4.f)
                {
                    const glm::vec3 d = level(g.dir);
                    Con_Printf("step: %d leg %d %.2f s, from %.1f to %.1f along (%.1f aside), capture %.1f along, com %.1f\n",
                        c.num, best, static_cast<float>(c.now - s.begun), glm::dot(from, d) * p.m2u, glm::dot(to, d) * p.m2u,
                        glm::dot(to - from, glm::cross(up, d)) * p.m2u, glm::dot(g.capture, d) * p.m2u,
                        glm::dot(com, d) * p.m2u);
                }
            }
        }
        if(!g.legs && g.stepLeg < 0 && g.steps < stepsCap)
        {
            const float h = za::max(com.z - footZ, 0.1f);
            const glm::vec3 then = level(com) + level(comVel) * za::sqrt(h / za::max(c.gravity, 1.f));
            // (Quicker steps the faster it goes, and the farther it leans: a backpedal's, a catch.)
            const float stepTime = za::clamp(staggerStepTime - 0.02f * glm::length(level(comVel)) -
                                                 0.004f * za::max(g.lean - staggerSteady, 0.f),
                staggerStepQuickest, staggerStepTime);
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
            // (A flinch steps only while its capture point is off its feet's support: once it would come to rest over
            // them, it stands; a shove's stagger backpedals on.)
            if(g.flinch && best >= 0)
            {
                const glm::vec2 a{g.planted[0].x, g.planted[0].y}, b{g.planted[1].x, g.planted[1].y}, q{then.x - g.supportOffset.x, then.y - g.supportOffset.y};
                const glm::vec2 ab = b - a;
                const float l2 = glm::dot(ab, ab);
                const float u = l2 > 1e-8f ? za::clamp(glm::dot(q - a, ab) / l2, 0.f, 1.f) : 0.f;
                best = glm::length(q - (a + ab * u)) > g.footR ? best : -1;
            }
            if(best >= 0 && bestD > staggerStepAt * (g.lean > staggerSteady ? 0.6f : 1.f) * limbLength(s, p, best))
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
        if(gives || tips || g.sag < staggerSag)
        {
            g.outcome = Outcome::Fell;
            g.decided = c.now;
            if(vr_debug_ragdoll.value || vr_knockdown_debug.value)
            {
                Con_Printf("stagger: %d fell at %.2f s%s: leaning %.0f deg (at most %.0f), its pelvis at %.0f%% of its height, "
                           "%d steps, moved %.0f units (its middle %.0f along the shove from its feet, %.0f aside)\n",
                    c.num, t, gives ? " (gave way)" : "", g.lean, limit, g.sag * 100.f, g.steps,
                    glm::length(level(pelvis - g.pelvis0)) * p.m2u, glm::dot(level(com - support), g.dir) * p.m2u,
                    glm::dot(level(com - support), glm::cross(up, g.dir)) * p.m2u);
            }
            flinchResult(g, p, c, t, "fell");
            beginFall(s, p, g.dir, 0u, 0.f, false, c.now);
            return;
        }
        if(g.flinch)
        {
            // A flinch: handed back only once balanced (the recovery test held a moment), else over at its most.
            // (Beaten, past its balance's edge: no recovery; it gives way after its steps, above.)
            const bool steady = flinchSteady(g, s, p, c, com, comVel, pose) && t >= g.flinchMin && g.giveAfter < 0;
            g.stableFor = steady ? g.stableFor + c.dt : 0.f;
            // At its most, settled though the strict test never held (its middle a little off its feet's support, a
            // bigger rig's stance): still, its middle over its feet within the stagger's steady lean, not bent far, on
            // firm ground, no step under way: it stands. (Else it topples, below.)
            const bool settled = t > g.flinchMost && g.giveAfter < 0 && g.stepLeg < 0 &&
                                 g.speedNow <= za::max(vr_flinch_recover_speed.value, 0.01f) &&
                                 g.lean < staggerSteady * 1.5f && g.tilt < 1.5f * za::max(vr_flinch_recover_tilt.value, 1.f) &&
                                 (g.failed & 8u) == 0u;
            // (A step under way at its most: it lands first, a moment more.)
            const bool landing = t > g.flinchMost && g.stepLeg >= 0 && t < g.flinchMost + 0.3f;
            if((g.stableFor >= za::max(vr_flinch_recover_hold.value, 0.f) && steady) || settled)
            {
                g.outcome = Outcome::Recovered;
                g.decided = c.now;
                flinchResult(g, p, c, t, "recovered");
                if(vr_flinch_debug.value || vr_knockdown_debug.value)
                {
                    Con_Printf("flinch: %d recovered at %.2f s%s: margin %.0f units, speed %.2f m/s, tilt %.0f deg, %d steps, "
                               "moved %.0f units\n",
                        c.num, t, steady ? "" : " (settled)", g.margin * p.m2u, g.speedNow, g.tilt, g.steps, glm::length(level(pelvis - g.pelvis0)) * p.m2u);
                }
            }
            else if(t > g.flinchMost && !landing)
            {
                g.outcome = Outcome::Fell;
                g.decided = c.now;
                flinchResult(g, p, c, t, "toppled");
                if(vr_flinch_debug.value || vr_knockdown_debug.value)
                {
                    Con_Printf("flinch: %d toppled at %.2f s, never steady (failed %s%s%s%s%s): margin %.0f units, speed %.2f "
                               "m/s, tilt %.0f deg, %d steps\n",
                        c.num, t, (g.failed & 1u) ? "support " : "", (g.failed & 2u) ? "speed " : "",
                        (g.failed & 4u) ? "tilt " : "", (g.failed & 8u) ? "ground " : "", (g.failed & 16u) ? "stepping" : "",
                        g.margin * p.m2u, g.speedNow, g.tilt, g.steps);
                    if(g.failed & 8u)
                    {
                        Con_Printf("flinch: %d its ground: ray share %.2f, normal up %.2f, the foot at %.0f %.0f %.0f (its "
                                   "pelvis %.0f %.0f %.0f)\n",
                            c.num, g.groundShare, g.groundUp, g.groundAt.x * p.m2u, g.groundAt.y * p.m2u,
                            g.groundAt.z * p.m2u, pelvis.x * p.m2u, pelvis.y * p.m2u, pelvis.z * p.m2u);
                    }
                }
                beginFall(s, p, g.dir, 0u, 0.f, false, c.now);
                return;
            }
        }
        else if((t > g.time + 0.2f && g.stepLeg < 0 && g.lean < staggerSteady && speed < 0.4f) || t > g.time + staggerMost)
        {
            g.outcome = Outcome::Recovered;
            g.decided = c.now;
            if(vr_debug_ragdoll.value || vr_knockdown_debug.value)
            {
                Con_Printf("stagger: %d recovered at %.2f s: leaning %.0f deg (at most %.0f), its pelvis at least %.0f%% of its "
                           "height, %d steps, moved %.0f units\n",
                    c.num, t, g.lean, g.leanMost, g.sag * 100.f, g.steps, glm::length(level(pelvis - g.pelvis0)) * p.m2u);
            }
        }
    }
    // Kept upright (as steady as its balance: the shove's pitch of its pelvis worked off), towards its stance.
    // (vr_flinch 2: its legs' own, legsBalance.)
    if(pose.valid && !g.legs)
    {
        turnWhole(p, c, glm::normalize(s.turn * pose.rot[0]), staggerUpright * (0.5f + 0.5f * g.balance) * s.base,
            staggerUprightMost, 1.f);
    }
    // The arms thrown out against the shove and wheeling (its balance's), the less the steadier it is.
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
        const float a = 2.f * 3.14159265f * staggerWheel * t + (arm == 0 ? 0.f : 3.14159265f);
        r.goal = shoulder + ahead * (0.5f * len) + left * ((arm == 0 ? 0.35f : -0.35f) * len) +
                 (ahead * za::cos(a) + up * za::sin(a)) * (0.2f * len);
        r.weight = smooth01(t / 0.12f) * (0.4f + 0.4f * (1.f - g.balance)) * g.wheel *
                   (g.flinch ? 1.f - smooth01((t - 0.12f) / 0.2f) : 1.f); // (a flinch's arms out a moment, then back)
        r.strength = 0.7f;
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
        // (vr_flinch 2: a bearing foot held where it stands, its leg following its pelvis over it, not a spring pulling
        // its pelvis back over the foot.)
        // (Its tone low there: its weight is its legs' push, legsBalance; a stiff leg's motors damp its pelvis's travel
        // over the foot, the glued feet of a pin.)
        r.weight = leg == g.stepLeg || g.legs ? 1.f : 0.f;
        r.strength = leg == g.stepLeg || !g.legs ? 1.f : legsStance;
        r.upright = g.legs && leg != g.stepLeg && pose.valid && g.outcome == Outcome::Going;
        r.from = pose.valid ? glm::normalize(s.turn * pose.rot[0]) : r.from;
        g.planted[i] = leg == g.stepLeg || g.legs ? g.planted[i] : limbEnd(s, p, leg);
        r.goal = g.planted[i];
        if(leg == g.stepLeg)
        {
            const float len = limbLength(s, p, leg);
            const float u = za::clamp(static_cast<float>(c.now - g.stepFrom) / za::max(g.stepTime, 0.05f), 0.f, 1.f);
            r.goal = glm::mix(g.stepA, g.stepB, smooth01(u)) + up * (0.12f * len * za::sin(3.14159265f * u));
            // (vr_flinch 2: the foot carried along its way, its shin and thigh moved with it: its joints' springs alone
            // left it short of its spot, and a step that lands short catches nothing.)
            if(g.legs)
            {
                const float u2 = za::min(u + c.dt / za::max(g.stepTime, 0.05f), 1.f);
                glm::vec3 next = glm::mix(g.stepA, g.stepB, smooth01(u2)) + up * (0.12f * len * za::sin(3.14159265f * u2));
                // (Over the floor on its way, clear of it: a step up lifts over the riser, not through it, and its carry
                // does not shove the body through the contact; at its end it comes down onto its spot.)
                const float under = floorUnder(c, next, za::max(pelvis.z, next.z + 0.1f), next.z - len, -1e30f);
                if(under > -1e29f && u2 < 1.f)
                {
                    const float clear = under + za::min(g.footGap[i], 0.f) + legsSwingClear * len * (1.f - smooth01((u2 - 0.8f) / 0.2f));
                    next.z = za::max(next.z, clear);
                    r.goal.z = za::max(r.goal.z, clear);
                }
                glm::vec3 want = (next - limbEnd(s, p, leg)) / za::max(c.dt, 1e-3f);
                if(const float l = glm::length(want); l > legsSwingMost)
                {
                    want *= legsSwingMost / l;
                }
                // (Its hip's doing: the leg's push back on its body, its whole body's motion kept.)
                glm::vec3 moved{0.f};
                for(int j = 0; j < 2; j++)
                {
                    const int b = s.chain[static_cast<za::SizeT>(leg)][static_cast<za::SizeT>(j)];
                    const glm::vec3 v = glmv(b3Body_GetLinearVelocity(p.body[b]));
                    const glm::vec3 dv = (want * (j == 0 ? 0.5f : 1.f) - v) * legsSwingGrip;
                    b3Body_SetLinearVelocity(p.body[b], b3v(v + dv));
                    moved += dv * b3Body_GetMass(p.body[b]);
                }
                for(int b = 0; b < p.count; b++)
                {
                    if(!cut(p, b) && (g.legParts & (1u << b)) == 0u)
                    {
                        b3Body_SetLinearVelocity(p.body[b], b3v(glmv(b3Body_GetLinearVelocity(p.body[b])) - moved / g.upperMass));
                    }
                }
            }
            if(u >= 1.f)
            {
                g.planted[i] = g.stepB;
                g.stepLeg = -1;
                g.steps++;
                const glm::vec3 way = level(g.stepB - g.stepA);
                const float wl = glm::length(way);
                g.stepWay += wl;
                g.stepAlong += wl > 1e-3f ? glm::dot(way / wl, g.dir) : 0.f;
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
constexpr float slipTime = 0.25f;   // s: a hold at its most this long slips off
constexpr float reachTime = 1.f;    // s: a hand not on its surface by then gives up
constexpr float handOff = 1.5f;     // units: a hand's goal off its surface (its thickness)
constexpr float smallThing = 8.f;   // kg: a loose thing lighter is nothing to brace on,
constexpr float smallHeight = 0.4f; // m: or lower (a backpack, a dropped gun)
constexpr float ledgeReach = 1.15f; // (of its arm's length) a lip this near a shoulder is grabbed (the hold pulls the hand the
                                    // rest of the way; a death knight toppling off falls with its shoulders 31-34 units off
                                    // the lip, out of its reach of 27: 1.4 had it reach in vain)
// A living one's climb ("Climbs", below).
constexpr float climbGrip = 2.f;    // its hands' holds take this times a dying one's
constexpr float hangBefore = 0.45f; // s: it hangs this (and up to as much again) before it pulls
constexpr float pullTime = 0.9f;    // s: its pelvis carried up under the lip over this,
constexpr float overUp = 0.5f;      // then up over the lip over this,
constexpr float overIn = 0.6f;      // and in onto the top over this
constexpr float climbPin = 3.f;     // body weights: its pelvis's pin at most
constexpr float climbHertz = 8.f;   // its spring (on its pelvis alone: its muscles hold its weight, climbLift)
constexpr float climbLift = 0.9f;   // its weight held up by its muscles as it pulls and goes over (a share, on every part)
constexpr float climbArms = 0.3f;   // its arms' strength as it pulls and goes over (holding: 0.9)
constexpr float climbChestPull = 0.8f; // body weights: its chest led along at most (climbChest)
constexpr float climbLag = 0.3f;    // (of its arm's length) its pelvis this far behind its way: the way slows to a quarter
constexpr float climbSlack = 1.5f;  // s: a step of it (up under the lip, over it, in) this much later than its time: it gives up

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

[[nodiscard]] const char* armName(int arm)
{
    return arm == 0 ? "left" : "right";
}

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
        glm::vec3 sideDir = shoulder - glmv(b3Body_GetWorldCenter(p.body[chestOf(p)]));
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
        if(!ledge && c.now - h.since > 0.3)
        {
            h.goal = hand - h.normal * off;
        }
        // (A ledge's lip falling past: grabbed farther off the faster it goes, a quarter of an arm per m/s, its hold pulling the hand in.)
        const float within = ledge ? za::clamp(0.25f * glm::length(glmv(b3Body_GetLinearVelocity(p.body[0]))), 0.25f, 1.f) : 0.25f;
        if(glm::length(hand - h.goal) < within * L || (off < 0.2f * L && glm::length(along) < 0.5f * L))
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

// ---- Grabbing others (vr_ragdoll_active_grab) ----
// Falling (a living one's fall, a death's), another monster near a hand (standing: its box; knocked down or dead: its
// ragdoll's parts) above its waist: the nearer hand reaches for the nearest point of it (the way it falls first) and,
// there, holds on a moment (a capped motor joint to that body, as a lean's: it slows the fall, a ragdoll held is dragged
// along), then lets go (its time, its load past the cap, struck, landed, pulled away, the other gone). Once a fall.
constexpr float grabReach = 1.1f;  // (of its arm's length) another's nearest point this near a shoulder is grabbed
constexpr float grabCap = 1.f;      // body weights its hand's hold takes at most (a heavy one slips sooner: holdCap's)
constexpr float grabReachTime = 0.6f; // s: a hand not on it by then gives up

// The shapes round a falling body (findGrab's).
za::Vector<b3ShapeId> envOthers;

// Another monster near a hand as it falls (its chest `chestAt`, its motion `v`): that hand to it. Whether one did.
bool findGrab(State& s, const Parts& p, const Context& c, const glm::vec3& chestAt, const glm::vec3& v, float L)
{
    Env& e = s.env;
    if(!c.graspable || !c.othersMask || L <= 0.f)
    {
        return false;
    }
    b3AABB box;
    box.lowerBound = b3v(chestAt - glm::vec3{1.5f * L});
    box.upperBound = b3v(chestAt + glm::vec3{1.5f * L});
    b3QueryFilter filter = b3DefaultQueryFilter();
    filter.maskBits = c.othersMask;
    envOthers.clear();
    b3World_OverlapAABB(c.world, box, filter,
        [](b3ShapeId shape, void* out) {
            static_cast<za::Vector<b3ShapeId>*>(out)->pushBack(shape);
            return true;
        },
        &envOthers);
    const glm::vec3 pelvis = glmv(b3Body_GetWorldCenter(p.body[0]));
    const glm::vec3 left = bodyRot(p, chestOf(p)) * glm::vec3{0.f, 1.f, 0.f};
    const float speed = glm::length(v);
    const glm::vec3 way = speed > 1e-3f ? v / speed : glm::vec3{0.f};
    int bestArm = -1;
    float best = 1e30f;
    glm::vec3 bestAt{0.f};
    b3ShapeId bestShape = b3_nullShapeId;
    for(const b3ShapeId shape : envOthers)
    {
        const int num = static_cast<int>(reinterpret_cast<intptr_t>(b3Shape_GetUserData(shape)));
        if(num == c.num || num <= 0 || !c.graspable(num))
        {
            continue;
        }
        for(int arm = 0; arm < 2; arm++)
        {
            const EnvHand& h = e.hand[static_cast<za::SizeT>(arm)];
            if(!limbWhole(s, p, arm) || h.mode != Grip::None)
            {
                continue;
            }
            const glm::vec3 shoulder = shoulderOf(s, p, arm);
            // (Its nearest point to the hand: where the arm reaches easily, not one at the shoulder.)
            const glm::vec3 hand = limbEnd(s, p, arm);
            const glm::vec3 q = glmv(b3Shape_GetClosestPoint(shape, b3v(hand)));
            const float d = glm::distance(q, shoulder);
            const float side = glm::dot(q - chestAt, left) * (arm == 0 ? 1.f : -1.f);
            // (Within reach, above its waist (a body lying: low too, it falls onto it), not across its body; the way it
            // falls first.)
            const float low = b3Body_GetType(b3Shape_GetBody(shape)) == b3_dynamicBody ? 0.9f : 0.1f;
            if(d > grabReach * L || d < 1e-3f || q.z < pelvis.z - low * L || side < -0.3f * L)
            {
                continue;
            }
            const float score = glm::distance(q, hand) - 0.4f * L * glm::dot(glm::normalize(q - shoulder), way);
            if(score < best)
            {
                best = score;
                bestArm = arm;
                bestAt = q;
                bestShape = shape;
            }
        }
    }
    if(bestArm < 0)
    {
        return false;
    }
    EnvHand& h = e.hand[static_cast<za::SizeT>(bestArm)];
    h.mode = Grip::Grab;
    h.on = b3Shape_GetBody(bestShape);
    h.local = glmv(b3Body_GetLocalPoint(h.on, b3v(bestAt)));
    h.other = static_cast<int>(reinterpret_cast<intptr_t>(b3Shape_GetUserData(bestShape)));
    h.goal = bestAt;
    h.normal = glm::vec3{0.f, 0.f, 1.f};
    h.since = c.now;
    h.touched = -1.0;
    h.want = 1.f;
    h.strength = 0.9f;
    h.holdUntil = 0.0;
    e.grabs++;
    if(vr_debug_ragdoll.value || vr_knockdown_debug.value)
    {
        Con_Printf("ragenv: %d %s hand grabs at %s %d falling at %.1f m/s: %.0f units from its shoulder, at %.0f %.0f %.0f\n",
            c.num, armName(bestArm), b3Body_GetType(h.on) == b3_dynamicBody ? "the ragdoll of" : "monster", h.other, speed,
            glm::distance(bestAt, shoulderOf(s, p, bestArm)) * p.m2u, bestAt.x * p.m2u, bestAt.y * p.m2u, bestAt.z * p.m2u);
    }
    return true;
}

// A hand reaching for another (Grab) or holding it: held once there (holdOn's motor joint, capped), its upkeep.
void keepGrab(State& s, const Parts& p, const Context& c, int arm, float L)
{
    EnvHand& h = s.env.hand[static_cast<za::SizeT>(arm)];
    if(B3_IS_NULL(h.on) || !b3Body_IsValid(h.on) || (B3_IS_NON_NULL(h.joint) && !b3Joint_IsValid(h.joint)))
    {
        letGo(s, arm, &c, "its hold gone");
        return;
    }
    const glm::vec3 at = glmv(b3Body_GetWorldPoint(h.on, b3v(h.local)));
    const glm::vec3 hand = limbEnd(s, p, arm);
    if(B3_IS_NULL(h.joint))
    {
        h.goal = at;
        if(glm::distance(hand, at) < (s.env.living ? 0.55f : 0.4f) * L) // (held where the hand is: its hold pulls it the rest of the way; a fall is quick)
        {
            h.holdUntil = c.now + 0.5 + 0.6 * static_cast<double>(roll(s.env.seed, 63u + static_cast<uint32_t>(arm)));
            const float heavy = za::clamp(za::sqrt(90.f / za::max(s.totalMass, 1.f)), 0.6f, 1.2f); // (holdCap's)
            holdOn(s, p, c, arm, hand, grabCap * heavy * s.totalMass * c.gravity);
        }
        else if(c.now - h.since > grabReachTime)
        {
            letGo(s, arm, &c, va("out of reach: %.0f units off it", glm::distance(hand, at) * p.m2u));
        }
        return;
    }
    h.load = glm::length(glmv(b3Joint_GetConstraintForce(h.joint)));
    h.overSince = h.load >= 0.95f * h.cap ? (h.overSince < 0.0 ? c.now : h.overSince) : -1.0;
    h.goal = glmv(b3Body_GetWorldPoint(h.on, b3Joint_GetLocalFrameA(h.joint).p));
    h.strength = 0.8f;
    if(h.overSince >= 0.0 && c.now - h.overSince > slipTime)
    {
        letGo(s, arm, &c, "its load");
    }
    else if(c.now > h.holdUntil)
    {
        letGo(s, arm, &c, "its time");
    }
    else if(glm::distance(hand, h.goal) > 0.8f * L)
    {
        letGo(s, arm, &c, va("pulled away: the hand %.0f units off its hold", glm::distance(hand, h.goal) * p.m2u));
    }
}

// ---- Climbs (vr_ragdoll_active_pullup; vr_ragactive.hpp, Climb) ----
// A living knockdown's fall past a lip (livingLedge: drive()'s for a fall, beginClimb's for a body falling undriven):
// findLedge's hands reach for it and hold (the env's Ledge holds, a living one's grip climbGrip times a dying one's). It
// hangs a moment, turning to face the wall, then (by the chance) pulls itself up: its pelvis pinned (the pin, a capped
// motor joint as a get-up's) and carried along a way from where it hangs, up under the lip (its chest over the top),
// then up over the lip and in onto the top, its pelvis turned to face the wall and pitched forwards as it goes over (its
// body lying down on the top); its hands holding until it goes in, a leg reaching up onto the top. Not pulling, it drops
// after its hang as a death's. Its hands both let go (struck, its load) or the pin can't carry it: it falls on (a Fall,
// its arms bracing). Up: its pin dropped, its strength easing off, it lies on the top, a knockdown again.

// Its chest (part `b`) drawn towards `goal` (world, metres) as it climbs: a capped spring (3 Hz, as if 0.4 of its
// weight hung on it; at most climbChestPull body weights), its pin's way being its pelvis's alone.
void climbChest(const State& s, const Parts& p, const Context& c, int b, const glm::vec3& goal)
{
    const float w = twoPi * 3.f;
    const glm::vec3 x = glmv(b3Body_GetWorldCenter(p.body[b]));
    const glm::vec3 v = glmv(b3Body_GetLinearVelocity(p.body[b]));
    glm::vec3 f = (0.4f * s.totalMass) * (w * w * (goal - x) - 2.f * w * v);
    const float most = climbChestPull * s.totalMass * c.gravity * s.base;
    const float n = glm::length(f);
    if(n > most && n > 1e-4f)
    {
        f *= most / n;
    }
    pushPart(p, b, f);
}

// Its climb over (`up`: pulled up, else dropped): its holds let go, its pin dropped, its strength easing off (up: it lies
// on the top, a knockdown), or a fall from here (its arms bracing).
void endClimb(State& s, const Parts& p, const Context& c, bool up, const char* why)
{
    Climb& k = s.climb;
    const float t = static_cast<float>(c.now - s.begun);
    for(int arm = 0; arm < 2; arm++)
    {
        letGo(s, arm, &c, why);
    }
    k.on = false;
    k.pinned = false;
    k.phase = ClimbPhase::Done;
    k.ended = why;
    dropPin(s.pin);
    s.pin.force = 0.f;
    s.support = Fade{fadeAt(s.support, t), 0.f, t, t + 0.25f};
    for(int leg = 2; leg < 4; leg++)
    {
        s.reach[static_cast<za::SizeT>(leg)].weight = 0.f;
    }
    const glm::vec3 pelvis = glmv(b3Body_GetWorldCenter(p.body[0]));
    if(vr_debug_ragdoll.value || vr_knockdown_debug.value)
    {
        Con_Printf("climb: %d %s (%s) after %.2f s: its chest %.0f units over the lip at most; its pelvis %.0f units in from the "
                   "lip, %.0f over the top\n",
            c.num, up ? "pulled itself up" : "dropped", why, c.now - k.since0, k.rose * p.m2u,
            -glm::dot(pelvis - k.lip, k.out) * p.m2u, (pelvis.z - k.top) * p.m2u);
    }
    if(up)
    {
        for(int ch = 0; ch < channelCount; ch++)
        {
            Fade& f = s.fade[static_cast<za::SizeT>(ch)];
            f = Fade{fadeAt(f, t), 0.f, t, t + 0.6f};
        }
    }
    else
    {
        beginFall(s, p, k.out, 0u, 0.f, false, c.now);
    }
}

void climbFrame(State& s, const Parts& p, const Context& c, float L)
{
    Climb& k = s.climb;
    Env& e = s.env;
    const glm::vec3 up{0.f, 0.f, 1.f};
    int holding = 0, reaching = 0;
    glm::vec3 sum{0.f};
    for(const EnvHand& h : e.hand)
    {
        if(h.mode == Grip::Ledge)
        {
            if(B3_IS_NON_NULL(h.joint))
            {
                holding++;
                sum += h.goal;
            }
            else
            {
                reaching++;
            }
        }
    }
    if(holding + reaching == 0 && !k.released)
    {
        endClimb(s, p, c, false, "lost its hold");
        return;
    }
    if(holding > 0)
    {
        k.lip = sum / static_cast<float>(holding);
    }
    const int chest = chestOf(p);
    const glm::vec3 chestAt = glmv(b3Body_GetWorldCenter(p.body[chest])), pelvis = glmv(b3Body_GetWorldCenter(p.body[0]));
    k.rose = za::max(k.rose, chestAt.z - k.top);
    const float pelvisIn = -glm::dot(pelvis - k.lip, k.out);
    if(c.trace)
    {
        // (vr_ragdoll_active_trace: its phase; its chest's and pelvis's height over the top and how far in from the lip,
        // units; the hands holding; the pelvis from its pin's anchor, units.)
        Con_Printf("ragclimb %d %.3f ph %d chest %.1f %.1f pelvis %.1f %.1f h %d pin %.1f\n", c.num,
            static_cast<float>(c.now - c.born), static_cast<int>(k.phase), (chestAt.z - k.top) * p.m2u,
            -glm::dot(chestAt - k.lip, k.out) * p.m2u, (pelvis.z - k.top) * p.m2u, pelvisIn * p.m2u, holding,
            k.pinned ? glm::distance(pelvis, k.pinAt) * p.m2u : -1.f);
    }
    const glm::vec3 wall = -k.out;
    const auto phaseTo = [&](ClimbPhase ph) {
        k.phase = ph;
        k.since = c.now;
        k.from = pelvis;
        k.prog = k.prog2 = 0.f;
        if(vr_debug_ragdoll.value || vr_knockdown_debug.value)
        {
            Con_Printf("climb: %d %s at %.2f s: its chest %.0f units %s the lip, %.0f off the wall\n", c.num,
                ph == ClimbPhase::Pull ? "pulls itself up" : "goes over the lip", c.now - k.since0,
                za::abs(chestAt.z - k.top) * p.m2u, chestAt.z < k.top ? "under" : "over", glm::dot(chestAt - k.lip, k.out) * p.m2u);
        }
    };
    // Its way's points (its pelvis's: its body's origin), off the lip: under it (its chest over the top), over it, in on
    // the top; its pelvis pitched forwards (rad) over the lip and in.
    const glm::vec3 under = k.lip + k.out * (0.5f * L) - up * (0.35f * L);
    const glm::vec3 over = k.lip + k.out * (0.35f * L) + up * (0.6f * L);
    const glm::vec3 in = k.lip + wall * (1.f * L) + up * (0.3f * L);
    const glm::vec3 axis = glm::normalize(glm::cross(up, wall));
    const float lag = k.pinned ? glm::distance(pelvis, k.pinAt) : 0.f; // (its pelvis behind its way)
    switch(k.phase)
    {
        case ClimbPhase::Hang:
        {
            if(holding > 0 && k.pullFrom <= 0.0)
            {
                k.pullFrom = c.now + hangBefore * (1.f + roll(e.seed, 52u));
            }
            // Facing the wall (its chest's front towards it), gently.
            const glm::vec3 ahead = level(bodyRot(p, chest) * glm::vec3{1.f, 0.f, 0.f});
            if(holding > 0 && glm::length(ahead) > 0.3f)
            {
                const glm::vec3 a = glm::normalize(ahead);
                const float off = za::atan2(glm::cross(a, wall).z, glm::dot(a, wall));
                turnWhole(p, c, glm::normalize(glm::angleAxis(off, up) * bodyRot(p, 0)), 2.f, 1.5f, 0.6f);
            }
            // Hanging by one hand: the other reaches up for the lip beside it (again each half second), its pull waiting
            // for it a little (a one-handed hang swings off its middle, the pull failing).
            if(k.pulls && holding == 1 && reaching == 0 && c.now >= k.reachAgain && k.retries < 2)
            {
                const int held = B3_IS_NON_NULL(e.hand[0].joint) ? 0 : 1, free = 1 - held;
                EnvHand& h = e.hand[static_cast<za::SizeT>(free)];
                if(limbWhole(s, p, free) && h.mode == Grip::None)
                {
                    const glm::vec3 along = glm::normalize(glm::cross(up, k.out));
                    const glm::vec3 shoulder = shoulderOf(s, p, free);
                    const glm::vec3 at = k.lip + along * za::clamp(glm::dot(shoulder - k.lip, along), -0.5f * L, 0.5f * L);
                    h.mode = Grip::Ledge;
                    h.goal = at;
                    h.normal = up;
                    h.on = e.hand[static_cast<za::SizeT>(held)].on;
                    h.since = c.now;
                    h.touched = -1.0;
                    h.want = 1.f;
                    h.strength = 1.f;
                    h.holdUntil = c.now + 30.0;
                    k.pullFrom = za::max(k.pullFrom, c.now + 0.4);
                }
                k.reachAgain = c.now + 0.5;
                k.retries++;
            }
            // (Not hanging (its legs still on the top, its back over the lip): it doesn't pull, it drops after its hang.)
            if(k.pulls && holding > 0 && k.pullFrom > 0.0 && c.now >= k.pullFrom && pelvis.z > k.top - 0.25f * L &&
                pelvisIn > 0.f)
            {
                endClimb(s, p, c, true, "on the top already"); // (it caught itself toppling over the lip)
                return;
            }
            if(k.pulls && holding > 0 && k.pullFrom > 0.0 && c.now >= k.pullFrom && pelvis.z > k.top - 0.25f * L)
            {
                k.pulls = false;
                for(EnvHand& h : e.hand)
                {
                    h.holdUntil = za::min(h.holdUntil, c.now + 0.6);
                }
                if(vr_debug_ragdoll.value || vr_knockdown_debug.value)
                {
                    Con_Printf("climb: %d not hanging (its pelvis %.0f units under the lip): it lets go\n", c.num,
                        (k.top - pelvis.z) * p.m2u);
                }
            }
            if(k.pulls && holding > 0 && k.pullFrom > 0.0 && c.now >= k.pullFrom)
            {
                for(EnvHand& h : e.hand)
                {
                    h.holdUntil = za::max(h.holdUntil, c.now + 30.0); // (held until it goes in)
                }
                // Its pin from where its pelvis is, turned as it is, towards facing the wall upright.
                const float t = static_cast<float>(c.now - s.begun);
                s.turn = glm::angleAxis(za::atan2(wall.y, wall.x), up);
                k.pinned = true;
                k.pinAt = pelvis;
                k.rot0 = bodyRot(p, 0);
                k.tilt = glm::quat{1.f, 0.f, 0.f, 0.f};
                k.turnBlend = 0.f;
                dropPin(s.pin);
                s.pin.force = climbPin;
                s.pin.fade = Fade{1.f, 1.f, t, t};
                s.pin.hertz = climbHertz;
                s.pin.turnHertz = -1.f;
                s.pin.turnShare = 0.06f; // (gently: held by a hand off its middle, a strong turn would wrench it about the hand)
                s.support = Fade{climbLift, climbLift, t, t}; // (its muscles hold its weight: the pin guides it)
                s.fade[static_cast<za::SizeT>(Channel::Arms)] = Fade{0.9f, climbArms, t, t + 0.2f};
                phaseTo(ClimbPhase::Pull);
            }
            break;
        }
        case ClimbPhase::Pull:
        {
            // (Its way goes on as fast as it keeps up: a quarter as fast while its pelvis lags past climbLag.)
            k.prog = za::min(1.f, k.prog + c.dt / pullTime * (lag < climbLag * L ? 1.f : 0.25f));
            const float u = smooth01(k.prog);
            k.pinAt = glm::mix(k.from, under, u);
            climbChest(s, p, c, chest, glm::mix(chestAt, k.lip + up * (0.3f * L) + k.out * (0.25f * L), u)); // (off the wall)
            k.turnBlend = u;
            if(k.prog >= 1.f && (glm::distance(pelvis, under) < climbLag * L || pelvis.z > under.z - 0.2f * L))
            {
                phaseTo(ClimbPhase::Over);
            }
            else if(c.now - k.since > pullTime + climbSlack)
            {
                endClimb(s, p, c, false, "too weak to rise");
            }
            break;
        }
        case ClimbPhase::Over:
        {
            // Up over the lip (its hands holding), then (its pelvis over the top) in onto the top, its hands let go,
            // pitched forwards as it goes. Its way goes on as fast as it keeps up (as Pull's).
            const float pace = lag < climbLag * L ? 1.f : 0.25f;
            if(!k.released)
            {
                k.prog = za::min(1.f, k.prog + c.dt / overUp * pace);
            }
            else
            {
                k.prog2 = za::min(1.f, k.prog2 + c.dt / overIn * pace);
            }
            const float u1 = smooth01(k.prog), u2 = smooth01(k.prog2);
            k.pinAt = !k.released ? glm::mix(k.from, over, u1) : glm::mix(over, in, u2);
            // (Its chest led in over the top ahead of its pelvis, not left at the lip's corner as its pelvis rises past it:
            // folded there, it tipped back off.)
            climbChest(s, p, c, chest, k.lip + up * (0.45f * L) + wall * ((!k.released ? 0.3f * u1 : 0.3f + 0.7f * u2) * L));
            k.tilt = glm::angleAxis(0.6f * u1 + 0.9f * u2, axis);
            // (Let go once its pelvis is over the top and its chest over the lip, not tipped back off it.)
            if(!k.released && k.prog >= 1.f && pelvis.z > k.top + 0.1f * L && -glm::dot(chestAt - k.lip, k.out) > -0.1f * L &&
                chestAt.z > k.top - 0.1f * L)
            {
                k.released = true;
                k.since2 = c.now;
                const float t = static_cast<float>(c.now - s.begun);
                s.support = Fade{climbLift, climbLift, t, t}; // (held up until it is in: endClimb lets it down)
                for(int arm = 0; arm < 2; arm++)
                {
                    letGo(s, arm, &c, "going in onto the top");
                }
            }
            // Its legs up onto the top (one leading, by its seed; the other as it goes in), drawn in after its pelvis (their
            // thighs not caught on the lip).
            const int leadLeg = roll(e.seed, 53u) < 0.5f ? 2 : 3;
            for(int leg = 2; leg < 4; leg++)
            {
                Reach& r = s.reach[static_cast<za::SizeT>(leg)];
                const float w = leg == leadLeg ? smooth01((k.prog - 0.4f) / 0.6f) : u2;
                r.goal = k.lip + wall * ((0.15f + 0.5f * u2) * L) + up * (0.1f * L);
                r.weight = limbWhole(s, p, leg) ? 0.8f * w : 0.f;
                r.strength = 0.8f;
            }
            if(k.released && k.prog2 >= 1.f && pelvisIn > 0.5f * L && pelvis.z > k.top - 0.1f * L)
            {
                endClimb(s, p, c, true, "in on the top");
            }
            else if(!k.released && c.now - k.since > overUp + climbSlack)
            {
                endClimb(s, p, c, false, "stuck at the lip");
            }
            else if(k.released && c.now - k.since2 > overIn + climbSlack)
            {
                endClimb(s, p, c, pelvisIn > 0.35f * L, pelvisIn > 0.35f * L ? "slowly in" : "slid back off the lip");
            }
            break;
        }
        default: break;
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
    // The floor under his pelvis (one query a frame: the sense's and the tumble's below).
    const float under = c.floorBelow ? c.floorBelow(pelvis, 3.f * L) : -1e30f;
    // Sense: every few frames.
    if((brace || lean || ledge || grab) && e.frame % envEvery == 1)
    {
        e.probes = 0;
        const bool holding = B3_IS_NON_NULL(e.hand[0].joint) || B3_IS_NON_NULL(e.hand[1].joint);
        const bool reaching = e.hand[0].mode == Grip::Catch || e.hand[1].mode == Grip::Catch;
        // (Still up, for a lean: his chest an arm's length over the floor at least, his pelvis not far over it: on his
        // feet, not falling past a wall.)
        const bool upright = chestAt.z - under > 0.9f * L && pelvis.z - under < 1.6f * L;
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
            glm::vec3 back = -levelDir(front);
            back = glm::length(back) > 0.f ? back : glm::vec3{1.f, 0.f, 0.f};
            const glm::vec3 dir = glm::angleAxis(static_cast<float>(k) * 1.0471976f, up) * back;
            // (From a little inside his body: his back against the wall, the chest's middle may be past its face.)
            const float in = 0.3f * L;
            const float len = 1.5f * L + in; // (the shoulder is beside the chest: an arm reaches that far from its middle)
            const EnvRay r = envCast(e, p, c, chestAt - dir * in, dir * len);
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

float legLength(const State& s, const Parts& p)
{
    float sum = 0.f;
    int n = 0;
    for(int leg = 2; leg < 4; leg++)
    {
        if(limbWhole(s, p, leg))
        {
            sum += limbLength(s, p, leg);
            n++;
        }
    }
    return n > 0 ? sum / static_cast<float>(n) : 0.75f;
}

float flinchSpeed(const State& s, const Parts& p, const glm::vec3& impulse)
{
    float mass = s.totalMass;
    if(mass <= 0.f)
    {
        for(int b = 0; b < p.count; b++)
        {
            mass += cut(p, b) ? 0.f : b3Body_GetMass(p.body[b]);
        }
    }
    return glm::length(level(impulse)) / za::max(mass, 1.f);
}

constexpr float flinchJoltMost = 1.5f; // vr_flinch 2: a struck part's jolt, at most this times its upper body's share's

void beginFlinch(State& s, const Parts& p, const FlinchInfo& info, double now)
{
    const float dv = flinchSpeed(s, p, info.impulse);
    const float local = za::clamp(info.local, 0.f, 1.f);
    const float topple = za::max(info.topple, 0.1f);
    const float tilt0 = spineTilt(p); // (as it stood, before the push)
    // The whole body's share as a stagger's travel: its pin's first speed (beginStagger: 2 travel / time, its time 0.4 +
    // 0.3 travel) this share of the change of speed.
    const float v0 = za::min(dv * (1.f - local), 6.f);
    StaggerInfo si;
    si.dir = info.impulse;
    si.travel = 0.4f * v0 / za::max(2.f - 0.3f * v0, 0.2f);
    si.push = 0.f;
    si.balance = za::clamp(1.f - 0.5f * dv / topple, 0.f, 1.f);
    si.first = si.last = info.first;
    beginStagger(s, p, si, now);
    Stagger& g = s.stagger;
    g.flinch = true;
    g.flinchMin = za::max(vr_flinch_recover_min.value, 0.f);
    g.flinchMost = za::max(info.most, g.flinchMin + 0.1f);
    g.tilt0 = tilt0;
    g.dv = dv;
    g.impulse = info.impulse;
    g.hitAt = info.at;
    g.footR = za::max(info.footR, 0.01f);
    // Its stance frame's centre of mass from its feet's middle (model space: each part's mass at its centre as the
    // frame poses it), turned as it stands.
    {
        const int pose = ragdoll::poseOfFrame(s.model, info.first);
        const auto partAt = [&](int b, const glm::vec3& local, glm::quat& q, glm::vec3& t) {
            ragdoll::bonePose(*p.rig, pose, b, q, t);
            return q * local + t * p.k;
        };
        glm::vec3 com{0.f}, feet{0.f};
        float mass = 0.f;
        int n = 0;
        for(int b = 0; b < p.count; b++)
        {
            if(cut(p, b) || p.rig->bones[b].joint == ragdoll::Joint::Loose)
            {
                continue;
            }
            glm::quat q;
            glm::vec3 t;
            const float m = b3Body_GetMass(p.body[b]);
            com += partAt(b, glmv(b3Body_GetLocalCenter(p.body[b])), q, t) * m;
            mass += m;
        }
        za::Array<glm::vec3, 2> foot{};
        for(int leg = 2; leg < 4; leg++)
        {
            const int lo = s.chain[static_cast<za::SizeT>(leg)][1];
            if(limbWhole(s, p, leg) && lo >= 0)
            {
                glm::quat q;
                glm::vec3 t;
                foot[static_cast<za::SizeT>(leg - 2)] = partAt(lo, p.rig->bones[lo].end * p.k, q, t);
                feet += foot[static_cast<za::SizeT>(leg - 2)];
                n++;
            }
        }
        if(mass > 0.f && n > 0)
        {
            const glm::vec3 off = com / mass - feet / static_cast<float>(n);
            g.supportOffset = level(s.turn * glm::vec3{off.x, off.y, 0.f});
        }
        // Its steps land its feet where its stance frame has them round its middle, not where they stood when struck:
        // struck mid-stride or in an attack (a wide or split stance, its middle well off its feet), stepping to keep
        // that stance would leave its middle as far off its support after every step (a shuffle till it topples).
        if(mass > 0.f && n == 2)
        {
            for(za::SizeT i = 0; i < 2; i++)
            {
                const glm::vec3 d = foot[i] - com / mass;
                g.stance[i] = level(s.turn * glm::vec3{d.x, d.y, 0.f});
            }
        }
        if(vr_flinch_debug.value >= 3.f)
        {
            const glm::vec3 offNow = level(massCentre(p) - 0.5f * (g.planted[0] + g.planted[1]));
            Con_Printf("flinch: struck with its feet %.1f units apart, its middle %.1f off them (its stand frame's %.1f)\n",
                glm::length(level(g.planted[1] - g.planted[0])) * p.m2u, glm::length(offNow) * p.m2u,
                glm::length(g.supportOffset) * p.m2u);
        }
    }
    g.wheel = za::clamp(dv / topple, 0.1f, 1.f);
    // Its torso and head back to their tone once its travel is mostly done: its spine straightened over its feet (the
    // recovery test's tilt), not left bent as the hit left it.
    {
        const float t0 = static_cast<float>(now - s.begun);
        const auto torso = static_cast<za::SizeT>(Channel::Torso), head = static_cast<za::SizeT>(Channel::Head);
        s.fade[torso] = Fade{s.fade[torso].from, 1.f, t0 + 0.5f * g.time, t0 + g.time + 0.2f};
        s.fade[head] = Fade{s.fade[head].from, 1.f, t0 + 0.5f * g.time, t0 + g.time + 0.2f};
    }
    // Beaten (past its edge): it gives way after a step or two (a shove's stagger: three or four).
    g.giveAfter = g.balance < 0.5f ? za::min(1 + static_cast<int>(g.balance * 4.f), g.stepsMost) : -1;
    g.comZ0 = massCentre(p).z;
    // vr_flinch 2 (FLINCH.md, "Balance from the legs"): no pin and no lift on every part. The hit's whole impulse goes
    // into its body (the jolt below; the rest over its parts above its legs, by their mass, upward too), its torso given
    // more at first (it bends at the hit, then straightens).
    if(info.legs)
    {
        g.legs = true;
        dropPin(s.pin);
        s.pin.force = 0.f;
        s.support = Fade{0.f, 0.f, 0.f, 0.f};
        g.legParts = 0u;
        for(int b = 0; b < p.count; b++)
        {
            for(int leg = 2; leg < 4; leg++)
            {
                const int top = s.chain[static_cast<za::SizeT>(leg)][0];
                g.legParts |= top >= 0 && onBone(*p.rig, b, top) ? 1u << b : 0u;
            }
        }
        float upper = 0.f;
        for(int b = 0; b < p.count; b++)
        {
            upper += cut(p, b) || (g.legParts & (1u << b)) ? 0.f : b3Body_GetMass(p.body[b]);
        }
        g.upperMass = za::max(upper, 1.f);
        g.overFloor = 0.f;
        g.pelvisOver = -1.f; // (measured on its first frame: staggerFrame, over the floor)
        s.fade[static_cast<za::SizeT>(Channel::Legs)] = Fade{legsStance, legsStance, 0.f, 0.f}; // (a step's leg: its reach's)
        const glm::vec3 rest = info.impulse * ((1.f - local) * za::max(info.push, 0.f));
        for(int b = 0; b < p.count; b++)
        {
            if(cut(p, b))
            {
                continue;
            }
            b3Body_SetLinearVelocity(p.body[b], b3Vec3_zero);
            b3Body_SetAngularVelocity(p.body[b], b3Vec3_zero);
            if((g.legParts & (1u << b)) == 0u)
            {
                b3Body_ApplyLinearImpulseToCenter(p.body[b], b3v(rest * (b3Body_GetMass(p.body[b]) / g.upperMass)), true);
            }
        }
        const float t0 = static_cast<float>(now - s.begun);
        const auto torso = static_cast<za::SizeT>(Channel::Torso);
        s.fade[torso] = Fade{0.35f + 0.45f * g.balance, 1.f, t0 + 0.15f, t0 + 0.55f};
    }
    // The jolt: its share at the point, on the part nearest it.
    if(local > 0.f)
    {
        int best = -1;
        float bestD = 1e30f;
        for(int b = 0; b < p.count; b++)
        {
            if(cut(p, b))
            {
                continue;
            }
            const float d = glm::distance(glmv(b3Body_GetWorldCenter(p.body[b])), info.at);
            if(d < bestD)
            {
                bestD = d;
                best = b;
            }
        }
        if(best >= 0)
        {
            glm::vec3 jolt = info.impulse * (local * (info.legs ? za::max(info.push, 0.f) : 1.f));
            // (vr_flinch 2: its part's jolt at most flinchJoltMost times its upper body's share's change of speed, the
            // rest over that upper body by mass: a light part struck (an enforcer's 6 kg pelvis, a death knight's) would
            // otherwise take several m/s and drag its legs out from under it, its feet kicked along the hit.)
            if(info.legs && (g.legParts & (1u << best)) == 0u)
            {
                const float m = b3Body_GetMass(p.body[best]);
                const float restDv = (1.f - local) * za::max(info.push, 0.f) * glm::length(info.impulse) / g.upperMass;
                const float most = flinchJoltMost * za::max(restDv, 0.05f) * m;
                if(const float l = glm::length(jolt); l > most)
                {
                    const glm::vec3 over = jolt * (1.f - most / l);
                    jolt -= over;
                    for(int b = 0; b < p.count; b++)
                    {
                        if(!cut(p, b) && (g.legParts & (1u << b)) == 0u)
                        {
                            b3Body_ApplyLinearImpulseToCenter(p.body[b], b3v(over * (b3Body_GetMass(p.body[b]) / g.upperMass)), true);
                        }
                    }
                }
            }
            b3Body_ApplyLinearImpulse(p.body[best], b3v(jolt), b3v(info.at), true);
        }
    }
}

void flinchDraw(const State& s, const Parts& p, double now)
{
    const Stagger& g = s.stagger;
    if(!s.on || !g.flinch || !p.rig)
    {
        return;
    }
    const float u = p.m2u;
    const glm::vec3 up{0.f, 0.f, 1.f};
    // Its state: going (yellow), steady now (green), recovered (cyan), fallen (red).
    const glm::vec4 colour = g.outcome == Outcome::Fell        ? glm::vec4{1.f, 0.2f, 0.2f, 1.f}
                             : g.outcome == Outcome::Recovered ? glm::vec4{0.2f, 0.9f, 1.f, 1.f}
                             : g.failed == 0u                  ? glm::vec4{0.2f, 1.f, 0.3f, 1.f}
                                                               : glm::vec4{1.f, 0.85f, 0.1f, 1.f};
    // The hit: an arrow from the point along the impulse, 1 unit per 4 N s (eased out over a second).
    const float age = static_cast<float>(now - s.begun);
    if(age < 1.f)
    {
        const glm::vec4 white{1.f, 1.f, 1.f, 1.f - age};
        const glm::vec3 a = g.hitAt * u, b = a + g.impulse * 0.25f;
        lines::line(a - (b - a), a, 0.5f, white, white);
        lines::point(a, 1.5f, white);
    }
    // Its middle (a point over the floor, a line down to it) and its feet's support (each foot's disc, the strip).
    glm::vec3 vel{0.f};
    const glm::vec3 com = massCentre(p, &vel);
    za::Array<glm::vec3, 2> foot{};
    int n = 0;
    for(int leg = 2; leg < 4; leg++)
    {
        if(limbWhole(s, p, leg))
        {
            foot[static_cast<za::SizeT>(n++)] = limbEnd(s, p, leg);
        }
    }
    const float floorZ = n > 0 ? za::min(foot[0].z, foot[static_cast<za::SizeT>(n - 1)].z) : com.z - 1.f;
    lines::point(com * u, 2.f, colour);
    lines::line(com * u, glm::vec3{com.x, com.y, floorZ} * u, 0.3f, colour, colour);
    constexpr int sides = 12;
    for(int i = 0; i < n; i++)
    {
        const glm::vec3 c = foot[static_cast<za::SizeT>(i)] * u;
        const float r = g.footR * u;
        for(int k = 0; k < sides; k++)
        {
            const float a0 = 6.2831853f * static_cast<float>(k) / sides, a1 = 6.2831853f * static_cast<float>(k + 1) / sides;
            lines::line(c + r * glm::vec3{za::cos(a0), za::sin(a0), 0.f}, c + r * glm::vec3{za::cos(a1), za::sin(a1), 0.f},
                0.25f, colour, colour);
        }
    }
    if(n == 2)
    {
        lines::line(foot[0] * u + up * 0.5f, foot[1] * u + up * 0.5f, 0.25f, colour, colour);
    }
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

bool climbing(const State& s)
{
    return s.on && s.climb.on;
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

bool beginClimb(State& s, const Parts& p, const Context& c)
{
    if(!p.rig || s.death.on || s.climb.on)
    {
        return false;
    }
    livingLedge(s, p, c);
    return s.climb.on;
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
    // A flinch's torso and head brought back to its stance (their motors turning them towards it as their tone comes
    // back, flinchStraighten of the way a second), not held still: a still clip's motor holds any bend up to twice its
    // weight's torque, so a torso struck bent (an attack's aim, the enforcer's 23-35 degrees) stayed so, its spring too
    // weak to pull it through, and it never passed the recovery test's tilt.
    if(s.stagger.on && s.stagger.flinch && pose.valid)
    {
        constexpr float flinchStraighten = 4.f, flinchStraightenMost = 3.f; // 1/s; rad/s
        for(int b = 1; b < p.count; b++)
        {
            const int parent = rig.bones[b].parent;
            const Channel ch = channelOf(rig.bones[b]);
            if(parent < 0 || cut(p, b) || cut(p, parent) || (ch != Channel::Torso && ch != Channel::Head))
            {
                continue;
            }
            const Fade& f = s.fade[static_cast<za::SizeT>(ch)];
            const float w = f.t1 > f.t0 ? za::clamp((t - f.t0) / (f.t1 - f.t0), 0.f, 1.f) : 1.f;
            const auto i = static_cast<za::SizeT>(b);
            const glm::quat rel = glm::normalize(glm::inverse(bodyRot(p, parent)) * bodyRot(p, b));
            glm::vec3 v = ragpose::rotationVector(s.animTarget[i] * glm::inverse(rel)) * (flinchStraighten * w);
            const float vl = glm::length(v);
            v = vl > flinchStraightenMost ? v * (flinchStraightenMost / vl) : v;
            spin[i] += v;
        }
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
    for(Reach& r : s.reach)
    {
        r.upright = false; // (a flinch's bearing legs' alone, set again each frame: staggerFrame)
    }
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
            offset = s.stagger.lift + s.stagger.dir * (s.stagger.travel * staggerShare(s.stagger, t));
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
        (s.death.on && s.death.crawl) || s.fall.on || s.stagger.on || s.rise.on || s.climb.on || s.struggle.on;
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
constexpr float stumpReach = 0.85f;     // of the holding arm's length: a stump farther from its shoulder, his torso curls
constexpr float stumpCurlMost = 0.6f;   // rad: at most
constexpr float stumpCurlStrength = 0.6f; // his torso's strength while it curls
constexpr float bloodNear = 4.f;        // units: the hand this near the stump is on it (its palm bloodied)
constexpr float bloodEvery = 0.7f;      // s: another mark while it holds
constexpr int bloodMarksMost = 4;
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
constexpr float hopLean = 7.f;          // deg: leaning this far from his line on one leg, he hops (at most hopsMost)
constexpr float hopTime = 0.2f;         // s: a hop
constexpr float hopLift = 0.1f;         // of his leg's length: his foot lifted that high
constexpr float hopReach = 0.4f;        // of his leg's length: a hop's way at most
constexpr int hopsMost = 3;
constexpr float pitchRate = 2.5f;       // rad/s: with no legs, killed standing, he pitches forward so

// (chestOf: the shared helper, above)

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
    // A stumble's carried way (a monster cut as he ran) ends here, kept as far as it got (the pin's anchor doesn't
    // jump): carried on, it dragged his pelvis up to a metre off the planted foot and broke the beat at once.
    Death& d = s.death;
    const glm::vec3 carried = d.stumble ? d.walk * (stumbleTau * (1.f - za::exp(-t / stumbleTau))) : glm::vec3{0.f};
    d.stumble = false;
    d.walk = glm::vec3{0.f};
    m.origin0 = s.origin + carried;
    m.balanceUntil = t + za::clamp(vr_ragdoll_active_balance_time.value, 0.1f, 5.f);
    // The foot left: kept where it stands, braced; leaning, it hops under him (dismemberFrame: hopLean), the pin holding
    // him up meanwhile.
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
        // A hop: leaning past hopLean, the foot lifted and put down under his pelvis (a little towards the side lost), his
        // line taken again from there; at most hopsMost. (His lean isn't judged while the foot is up.)
        bool hopping = m.hopFrom >= 0.f && t - m.hopFrom < hopTime;
        if(m.hopFrom >= 0.f && !hopping)
        {
            m.plant = m.hopB;
            m.line0 = bodyLine(p, limbEnd(s, p, m.standLeg));
            m.hopFrom = -1.f;
            m.hops++;
            m.lean = 0.f;
        }
        if(!hopping && m.hops < hopsMost && m.lean > hopLean && limbWhole(s, p, m.standLeg) && t < m.balanceUntil - hopTime)
        {
            const float len = limbLength(s, p, m.standLeg);
            const glm::vec3 pelvis = glmv(b3Body_GetWorldCenter(p.body[0]));
            glm::vec3 way = level(pelvis - m.plant) + m.lostSide * (0.05f * len);
            const float wl = glm::length(way);
            way = wl > hopReach * len ? way * (hopReach * len / wl) : way;
            m.hopA = m.plant;
            m.hopB = m.plant + way;
            m.hopB.z = m.plant.z + floorAt(c, glm::vec3{m.hopB.x, m.hopB.y, m.plant.z}, len, m.plant.z) -
                       floorAt(c, m.plant, len, m.plant.z);
            m.hopFrom = t;
            hopping = true;
            if(vr_debug_ragdoll.value)
            {
                Con_Printf("ragdoll: %d hops on his leg at %.2f s, leaning %.0f deg: %.1f units\n", c.num, t, m.lean,
                    glm::length(level(way)) * p.m2u);
            }
        }
        if(t >= m.balanceUntil || (!hopping && m.lean > za::max(vr_ragdoll_active_balance_lean.value, 1.f)) ||
            !limbWhole(s, p, m.standLeg))
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
            s.origin = m.origin0 + m.lostSide * (0.5f * tipAccel * t * t) - up * (sagDepth * smooth01(t / 0.4f));
            int torso = 0;
            for(int b = 1; b < p.count; b++)
            {
                torso += drivable(s, p, b) && channelOf(rig.bones[b]) == Channel::Torso ? 1 : 0;
            }
            // (About the rig's forward axis: +y, his left, is up turned towards it the negative way.)
            const float bend = tipBend * smooth01(t / za::max(m.balanceUntil, 0.1f)) / static_cast<float>(za::max(torso, 1)) *
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
            if(hopping)
            {
                const float u = za::clamp((t - m.hopFrom) / hopTime, 0.f, 1.f);
                leg.goal = glm::mix(m.hopA, m.hopB, smooth01(u)) +
                           up * (hopLift * limbLength(s, p, m.standLeg) * za::sin(3.14159265f * u));
            }
            leg.weight = smooth01(t / 0.05f);
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
                r.weight = smooth01(t / 0.4f);
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
            hold = smooth01((t - m.holdFrom) / holdRamp) * (1.f - smooth01((t - m.holdUntil) / holdLetGo));
            r.goal = stumpPoint(m, p, handThick);
            r.weight = hold;
            r.strength = holdStrength;
            m.handGap = glm::distance(limbEnd(s, p, m.holdArm), stumpPoint(m, p, 0.f)) * p.m2u;
            // Its palm bloodied while on the wound: a mark as it gets there, another every bloodEvery while held.
            if(hold > 0.5f && m.handGap < bloodNear && m.bloodMarks < bloodMarksMost &&
                (m.bloodAt < 0.f || t - m.bloodAt >= bloodEvery))
            {
                m.bloodAt = t;
                m.bloodMarks++;
                m.bloodDue++;
            }
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
        env = smooth01((t - m.writheFrom) / writheRamp) * (1.f - smooth01((t - (m.writheUntil - writheOut)) / writheOut));
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
            const float amp = (ch == Channel::Legs ? writheLegs : writheTorso) * env * za::max(vr_ragdoll_active_writhe.value, 0.f);
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
    // The stump past his reach (the ogre's arms, a stump lying under him): his torso curls towards it, bringing the
    // holding shoulder nearer (its turn about his pelvis shared by his torso's joints), held up enough to do it.
    m.curl = 0.f;
    if(hold > 0.f && m.holdArm >= 0)
    {
        const int u = s.chain[static_cast<za::SizeT>(m.holdArm)][0];
        const glm::vec3 shoulder = worldPoint(p, u, rig.bones[u].pivot);
        const glm::vec3 stump = stumpPoint(m, p, 0.f);
        const float len = limbLength(s, p, m.holdArm);
        const float over = glm::distance(shoulder, stump) - stumpReach * len;
        const glm::vec3 mid = glmv(b3Body_GetWorldCenter(p.body[0]));
        const glm::vec3 axis = glm::cross(shoulder - mid, stump - mid);
        int n = 0;
        for(int b = 1; b < p.count; b++)
        {
            n += drivable(s, p, b) && channelOf(rig.bones[b]) == Channel::Torso ? 1 : 0;
        }
        if(over > 0.f && glm::length(axis) > 1e-5f && n > 0)
        {
            m.curl = za::min(over / za::max(glm::distance(shoulder, mid), 0.1f), stumpCurlMost) * hold;
            for(int b = 1; b < p.count; b++)
            {
                if(!drivable(s, p, b) || channelOf(rig.bones[b]) != Channel::Torso)
                {
                    continue;
                }
                const glm::quat qp = bodyRot(p, rig.bones[b].parent);
                const glm::quat turn = glm::angleAxis(m.curl / static_cast<float>(n), glm::normalize(axis));
                const auto i = static_cast<za::SizeT>(b);
                s.target[i] = glm::normalize(glm::inverse(qp) * turn * qp * s.target[i]);
            }
            const auto ti = static_cast<za::SizeT>(Channel::Torso);
            const float v = za::max(za::max(fadeAt(m.deathFade[ti], t), writheStrength * env), stumpCurlStrength * hold);
            s.fade[ti] = Fade{v, v, 0.f, 0.f};
            m.curling = true;
        }
    }
    if(m.curl <= 0.f && m.curling)
    {
        m.curling = false; // (its torso's strength its death's again, or the writhe's: above, next frame)
        const auto ti = static_cast<za::SizeT>(Channel::Torso);
        s.fade[ti] = m.deathFade[ti];
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
        Con_Printf("ragdis %d %.3f gap %.1f lean %.1f beat %d %.2f hold %.2f writhe %.2f stir %.2f lost %u curl %.2f face %d\n",
            c.num,
            static_cast<float>(c.now - c.born), m.handGap, m.lean, balancing ? 1 : (m.brokeAt >= 0.f ? 2 : 0), m.brokeAt, hold,
            env, stir / static_cast<float>(za::max(n, 1)), m.lost, m.curl, m.face ? 1 : 0);
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
        // The cut face itself (the joint, facing along the lost limb's bone) when it is within that reach: the hand cups
        // the wound, not the skin beside it.
        const glm::vec3 along = rig.bones[j].end - rig.bones[j].pivot;
        m.face = false;
        if(glm::length(along) > 1e-3f && glm::distance(rig.bones[j].pivot, shoulder) <= reach)
        {
            m.face = true;
            m.stumpAt = rig.bones[j].pivot;
            m.stumpOut = glm::normalize(along);
        }
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

bool takeHandBlood(State& s, const Parts& p, glm::vec3& at, glm::vec3& dir)
{
    Dismember& m = s.dismember;
    if(m.bloodDue <= 0 || m.holdArm < 0 || !limbWhole(s, p, m.holdArm) || m.stumpPart < 0 || cut(p, m.stumpPart))
    {
        m.bloodDue = 0;
        return false;
    }
    m.bloodDue--;
    at = limbEnd(s, p, m.holdArm);
    const glm::vec3 to = stumpPoint(m, p, 0.f) - at;
    dir = glm::length(to) > 1e-4f ? glm::normalize(to) : glm::vec3{0.f, 0.f, -1.f};
    return true;
}

} // namespace qvr::ragactive
