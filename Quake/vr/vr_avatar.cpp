// vr_avatar.cpp -- see vr_avatar.hpp.

#include "vr_avatar.hpp"
#include "vr_engine.hpp"
#include "vr_units.hpp"
#include "vr_backend.hpp"
#include "vr_cvars.hpp"
#include "vr_lines.hpp"
#include "vr_profile.hpp"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

#include <array>
#include <cmath>
#include <cstring>

namespace qvr::avatar
{
namespace
{

// The modelled body (Misc/quakevr/make_vrbody.py, which these tables must match): model units
// per metre.
constexpr float UNITS = units::perMetre;

// A bone that is not drawn collapses to a point.
constexpr float COLLAPSED = 0.001f;

enum Joint : int
{
    Pelvis,
    Spine,
    Chest,
    Neck,
    Head,
    ClavicleL,
    UpperArmL,
    ForearmL,
    HandL,
    ClavicleR,
    UpperArmR,
    ForearmR,
    HandR,
    ThighL,
    CalfL,
    FootL,
    ThighR,
    CalfR,
    FootR,
    JointCount
};

constexpr const char* jointNames[JointCount] = {"pelvis", "spine", "chest", "neck", "head", "clavicle_l",
    "upperarm_l", "forearm_l", "hand_l", "clavicle_r", "upperarm_r", "forearm_r", "hand_r", "thigh_l", "calf_l",
    "foot_l", "thigh_r", "calf_r", "foot_r"};

constexpr int parentOf[JointCount] = {-1, Pelvis, Spine, Chest, Neck, Chest, ClavicleL, UpperArmL, ForearmL, Chest,
    ClavicleR, UpperArmR, ForearmR, Pelvis, ThighL, CalfL, Pelvis, ThighR, CalfR};

constexpr glm::vec3 UP{0.f, 0.f, 1.f};
constexpr glm::vec3 FWD{1.f, 0.f, 0.f};
constexpr glm::vec3 BACK{-1.f, 0.f, 0.f};

[[nodiscard]] glm::vec3 safeNormalize(const glm::vec3& v, const glm::vec3& fallback = UP)
{
    const float len = glm::length(v);
    return len > 1e-5f ? v / len : fallback;
}

// Columns: x along `dir`, z towards `hint`, y = z x x (make_vrbody.py's frame()).
[[nodiscard]] glm::mat3 basis(const glm::vec3& dir, const glm::vec3& hint)
{
    const glm::vec3 x = safeNormalize(dir);
    glm::vec3 z = hint - x * glm::dot(hint, x);
    if(glm::length(z) < 1e-5f)
    {
        z = glm::cross(x, std::abs(x.z) < 0.9f ? UP : FWD);
    }
    z = glm::normalize(z);
    return glm::mat3(x, glm::cross(z, x), z);
}

// The bind pose, in metres.
struct Bind
{
    std::array<glm::vec3, JointCount> pos;
    std::array<glm::mat3, JointCount> rot;
    std::array<glm::vec3, JointCount> offset; // from the parent, in the parent's frame
    glm::vec3 toe[2];
};

const Bind& bind()
{
    static const Bind b = [] {
        Bind r{};
        r.pos[Pelvis] = {0.f, 0.f, 0.95f};
        r.pos[Spine] = {-0.01f, 0.f, 1.10f};
        r.pos[Chest] = {-0.01f, 0.f, 1.28f};
        r.pos[Neck] = {-0.01f, 0.f, 1.47f};
        r.pos[Head] = {0.f, 0.f, 1.57f};

        const float s30 = std::sin(glm::radians(30.f));
        const float c30 = std::cos(glm::radians(30.f));
        for(int side = 0; side < 2; side++)
        {
            const float sy = side == 0 ? 1.f : -1.f;
            const int clav = side == 0 ? ClavicleL : ClavicleR;
            r.pos[clav] = {0.f, 0.03f * sy, 1.43f};
            r.pos[clav + 1] = {-0.01f, 0.19f * sy, 1.42f};
            r.pos[clav + 2] = r.pos[clav + 1] + glm::vec3{0.f, 0.29f * s30 * sy, -0.29f * c30};
            r.pos[clav + 3] = r.pos[clav + 2] + glm::vec3{0.f, 0.26f * s30 * sy, -0.26f * c30};

            const int thigh = side == 0 ? ThighL : ThighR;
            r.pos[thigh] = {0.f, 0.09f * sy, 0.92f};
            r.pos[thigh + 1] = {0.f, 0.09f * sy, 0.50f};
            r.pos[thigh + 2] = {-0.02f, 0.09f * sy, 0.08f};
            r.toe[side] = {0.15f, 0.09f * sy, 0.02f};
        }

        for(int j : {Pelvis, Spine, Chest, Neck})
        {
            r.rot[j] = basis(r.pos[j + 1] - r.pos[j], FWD);
        }
        r.rot[Head] = basis(UP, FWD);
        for(int side = 0; side < 2; side++)
        {
            const int clav = side == 0 ? ClavicleL : ClavicleR;
            r.rot[clav] = basis(r.pos[clav + 1] - r.pos[clav], UP);
            r.rot[clav + 1] = basis(r.pos[clav + 2] - r.pos[clav + 1], BACK);
            r.rot[clav + 2] = basis(r.pos[clav + 3] - r.pos[clav + 2], BACK);
            r.rot[clav + 3] = basis(r.pos[clav + 3] - r.pos[clav + 2], BACK);

            const int thigh = side == 0 ? ThighL : ThighR;
            r.rot[thigh] = basis(r.pos[thigh + 1] - r.pos[thigh], FWD);
            r.rot[thigh + 1] = basis(r.pos[thigh + 2] - r.pos[thigh + 1], FWD);
            r.rot[thigh + 2] = basis(r.toe[side] - r.pos[thigh + 2], UP);
        }

        for(int j = Pelvis + 1; j < JointCount; j++)
        {
            const int p = parentOf[j];
            r.offset[j] = glm::transpose(r.rot[p]) * (r.pos[j] - r.pos[p]);
        }
        return r;
    }();
    return b;
}

// A joint's offset from its parent, in the parent's bind frame, in metres.
[[nodiscard]] const glm::vec3& localOffset(int joint)
{
    return bind().offset[joint];
}

[[nodiscard]] float boneLength(int from, int to)
{
    return glm::distance(bind().pos[from], bind().pos[to]);
}

struct Bone
{
    glm::vec3 pos{0.f};
    glm::mat3 rot{1.f};
    float stretch{1.f}; // along the bone
    float size{1.f};    // all axes (COLLAPSED: not drawn)
};

struct Body
{
    float m2w{1.f};    // world units per metre of the modelled body
    float floorZ{0.f};
    glm::vec3 fwd{FWD};
    glm::vec3 left{0.f, 1.f, 0.f};
    std::array<Bone, JointCount> bones{};
};

// World position of `joint` from its posed parent.
[[nodiscard]] glm::vec3 childPos(const Body& b, int joint)
{
    const Bone& p = b.bones[parentOf[joint]];
    glm::vec3 o = localOffset(joint) * (b.m2w * p.size);
    o.x *= p.stretch;
    return p.pos + p.rot * o;
}

// The spine from the head. The top of the neck is found behind and below the eyes. A crouch lowers
// the pelvis straight down under the neck (the legs bend) and tilts the back forward about it, as a
// person crouching does; when the pelvis can go no lower, the back bends further.
void solveTorso(const hands::State& s, Body& b)
{
    const Bind& bd = bind();
    b.m2w = units::metresToUnits() * units::bodyScale();
    b.floorZ = s.head.z - s.headHeight * units::metresToUnits();

    glm::vec3 right, up;
    hands::angleVectors({0.f, s.bodyYaw, 0.f}, b.fwd, right, up);
    b.left = -right;

    glm::vec3 hf, hr, hu;
    hands::angleVectors(s.headAngles, hf, hr, hu);
    const glm::vec3 top = s.head - (hf * vr_body_eye_forward.value + hu * vr_body_eye_up.value) * b.m2w;

    const float torsoLen = glm::distance(bd.pos[Pelvis], bd.pos[Head]) * b.m2w;
    // How deep the crouch is: 0 standing, 1 with the pelvis at squatting height.
    const float standZ = b.floorZ + bd.pos[Pelvis].z * b.m2w;
    const float squatZ = b.floorZ + 0.3f * b.m2w;
    const float drop = std::max(0.f, b.floorZ + bd.pos[Head].z * b.m2w - top.z);
    const float crouch = standZ > squatZ ? std::min(1.f, drop / (standZ - squatZ)) : 0.f;
    const float tilt = glm::radians(CLAMP(0.f, vr_body_crouch_tilt.value, 80.f)) * crouch;
    float pelvisZ = std::max(top.z - torsoLen * std::cos(tilt), squatZ);
    pelvisZ = std::min(pelvisZ, top.z - 0.3f * torsoLen); // lying down: keep the back from folding over
    const float dz = top.z - pelvisZ;
    const float lean = dz < torsoLen ? std::sqrt(torsoLen * torsoLen - dz * dz) : 0.f;

    // The torso sits vr_body_torso_back behind the neck (looking down shows the chest rather than
    // the top of the shoulders), the pelvis under it; the back leans forward from there.
    const glm::vec3 back = b.fwd * (vr_body_torso_back.value * b.m2w);
    const glm::vec3 pelvis = glm::vec3{top.x, top.y, pelvisZ} - back;
    const glm::vec3 axis = safeNormalize(b.fwd * lean + UP * std::min(dz, torsoLen));

    Bone& p = b.bones[Pelvis];
    p = Bone{};
    p.pos = pelvis;
    p.rot = basis(glm::mix(UP, axis, 0.4f), b.fwd);

    Bone& sp = b.bones[Spine];
    sp = Bone{};
    sp.pos = childPos(b, Spine);
    sp.rot = basis(glm::mix(UP, axis, 0.75f), b.fwd);

    Bone& c = b.bones[Chest];
    c = Bone{};
    c.pos = childPos(b, Chest);
    c.rot = basis(axis, b.fwd);

    // The neck spans the rest of the way to the top of the neck; the head sits there. Neither is
    // drawn: the eyes are inside them.
    Bone& n = b.bones[Neck];
    n = Bone{};
    n.pos = childPos(b, Neck);
    n.rot = basis(top - n.pos, b.fwd);
    n.size = COLLAPSED;

    Bone& h = b.bones[Head];
    h = Bone{};
    h.pos = top;
    h.rot = basis(hu, hf);
    h.size = COLLAPSED;
}

// Two-bone chain from `root` towards `target`, bending towards `pole`: the middle joint.
[[nodiscard]] glm::vec3 twoBone(const glm::vec3& root, const glm::vec3& target, float a, float b, const glm::vec3& pole,
    const glm::vec3& fallbackPole, glm::vec3& bend)
{
    const glm::vec3 toTarget = target - root;
    const float d = glm::length(toTarget);
    const glm::vec3 dir = d > 1e-4f ? toTarget / d : -UP;
    const float reach = CLAMP(std::abs(a - b) + 1e-3f, d, a + b - 1e-4f);

    const float cosA = CLAMP(-1.f, (a * a + reach * reach - b * b) / (2.f * a * reach), 1.f);
    const float sinA = std::sqrt(std::max(0.f, 1.f - cosA * cosA));

    bend = pole - dir * glm::dot(pole, dir);
    if(glm::length(bend) < 1e-3f)
    {
        bend = fallbackPole - dir * glm::dot(fallbackPole, dir);
    }
    bend = safeNormalize(bend, glm::cross(dir, UP));

    return root + dir * (a * cosA) + bend * (a * sinA);
}

// Shoulder and arm of `side` (0 left, 1 right) to the wrist.
void solveArm(Body& b, int side, const HandPose& handPose)
{
    const Bind& bd = bind();
    const int clav = side == 0 ? ClavicleL : ClavicleR;
    const int upper = clav + 1;
    const int fore = clav + 2;
    const int hand = clav + 3;

    const Bone& chest = b.bones[Chest];
    const glm::vec3 cUp = chest.rot[0];
    const glm::vec3 cFwd = chest.rot[2];
    const glm::vec3& wrist = handPose.wrist;
    const glm::vec3& handUp = handPose.up;

    // The shoulder rises when the hand is above it, and swings forward when the hand reaches far
    // forward (the clavicle turns about the base of the neck).
    Bone& c = b.bones[clav];
    c = Bone{};
    const glm::mat3 rest = chest.rot * glm::transpose(bd.rot[Chest]) * bd.rot[clav];
    const glm::vec3 lateral = rest[0];
    // The shoulders' own offset from the chest (vr_body_shoulders_*): back, up, and outwards along
    // the clavicle, in metres.
    c.pos = childPos(b, clav) +
            (-cFwd * vr_body_shoulders_back.value + cUp * vr_body_shoulders_up.value + lateral * vr_body_shoulders_out.value) *
                b.m2w;

    const float armLen = (boneLength(upper, fore) + boneLength(fore, hand)) * b.m2w;
    const glm::vec3 restShoulder = c.pos + rest * (localOffset(upper) * b.m2w);
    const glm::vec3 reach = wrist - restShoulder;
    const float raiseAmount = CLAMP(0.f, glm::dot(reach, cUp) / (0.6f * armLen), 1.f);
    const float swingAmount = CLAMP(0.f, (glm::dot(reach, cFwd) / armLen - 0.5f) / 0.4f, 1.f);
    const glm::quat raise = glm::angleAxis(glm::radians(vr_body_shoulder_up.value * raiseAmount),
        safeNormalize(glm::cross(lateral, cUp), cFwd));
    const glm::quat swing = glm::angleAxis(glm::radians(vr_body_shoulder_forward.value * swingAmount),
        safeNormalize(glm::cross(lateral, cFwd), cUp));
    c.rot = glm::mat3_cast(raise * swing) * rest;

    // The arm: the elbow points down, somewhat out and back, and away from the back of the
    // hand (turning the palm up brings the elbow in). It may stretch a little to reach.
    Bone& u = b.bones[upper];
    u = Bone{};
    u.pos = childPos(b, upper);

    // Arms of vr_body_arm_length times the model's proportions, stretching up to
    // vr_body_arm_stretch; beyond that, the shoulder reaches out by up to
    // vr_body_shoulder_reach metres. Only past all of it does the hand leave the arm.
    const float length = CLAMP(0.5f, vr_body_arm_length.value, 2.f);
    float a = boneLength(upper, fore) * b.m2w * length;
    float l = boneLength(fore, hand) * b.m2w * length;
    float d = glm::distance(wrist, u.pos);
    const float stretch = CLAMP(1.f, d / (a + l), std::max(1.f, vr_body_arm_stretch.value));
    a *= stretch;
    l *= stretch;
    if(const float excess = d - (a + l); excess > 0.f)
    {
        const float reach = std::min(excess, std::max(0.f, vr_body_shoulder_reach.value) * b.m2w);
        u.pos += (wrist - u.pos) / d * reach;
        d -= reach;
    }

    const glm::vec3 pole = -cUp + lateral * vr_body_elbow_out.value - cFwd * vr_body_elbow_back.value -
                           handUp * vr_body_elbow_hand.value;
    glm::vec3 bend;
    const glm::vec3 elbow = twoBone(u.pos, wrist, a, l, pole, lateral, bend);

    u.rot = basis(elbow - u.pos, bend);
    u.stretch = stretch * length;

    // The forearm twists with the hand: the wrist fully (the hand bone), the forearm by
    // vr_body_forearm_twist. The bind pose has the palms facing the thighs, thumbs forward: the
    // bones' hint axis is the little finger's side, opposite the back of the (gripping) hand's
    // top, where the thumb is.
    const glm::vec3 foreDir = safeNormalize(wrist - elbow, glm::normalize(elbow - u.pos));
    const glm::mat3 untwisted = basis(foreDir, bend);
    const glm::mat3 wristRot = basis(foreDir, -handUp); // the hand's roll only
    const float twist = std::atan2(glm::dot(glm::cross(untwisted[2], wristRot[2]), foreDir),
        glm::dot(untwisted[2], wristRot[2]));

    Bone& f = b.bones[fore];
    f = Bone{};
    f.pos = elbow;
    f.rot = glm::mat3_cast(glm::angleAxis(twist * CLAMP(0.f, vr_body_forearm_twist.value, 1.f), foreDir)) * untwisted;
    f.stretch = stretch * length;

    // The hand bone turns with the hand itself, bent at the wrist too: the wrist and the bracer's
    // cuff over the back of the hand go with it, so that the hand never comes off the arm.
    Bone& h = b.bones[hand];
    h = Bone{};
    h.pos = elbow + foreDir * l;
    h.rot = glm::length(handPose.forward) > 0.5f ? basis(handPose.forward, -handUp) : wristRot;
}

// The legs' clock: seconds since they were last posed (0 the first time, and for a second pose in
// the same frame).
[[nodiscard]] float legsDeltaTime()
{
    static double lastTime = -1.0;
    const double now = realtime;
    const float dt = lastTime >= 0.0 ? static_cast<float>(CLAMP(0.0, now - lastTime, 0.1)) : 0.f;
    lastTime = now;
    return dt;
}

// Walking: a gait cycle driven by the player's own movement (the stick, not the room). The feet
// take turns: one on the ground going back under the body, the other swinging forward, lifted,
// along the direction of travel. The cadence follows the speed over the stride (so the planted
// foot keeps up with the ground), but only up to a natural rate (vr_body_step_rate steps per
// second at full running speed, somewhat fewer walking): Quake moves far faster than anyone
// walks, and legs spinning to keep up look silly. Strides lengthen and the feet lift higher with
// speed, less so sideways and backwards; the whole of it eases in and out as the player starts
// and stops, and the feet tuck up in the air. The legs' IK does the knees.
struct Gait
{
    float phase{0.f};       // radians, a full cycle is two steps
    float amount{0.f};      // 0 standing .. 1 walking
    float air{0.f};         // 0 on the ground .. 1 in the air
    glm::vec3 dir{1.f, 0.f, 0.f};
    float stride{0.f};      // metres, one step
    float lift{0.f};        // metres, the swinging foot at its highest
};

Gait gait;

// Full running speed (Quake's 320 units per second), in metres per second of the body at
// vr_world_scale 1.
constexpr float RUN_SPEED = 320.f / UNITS;

// The offset of a foot (side 0 left, 1 right) from where it stands, in world units. In the first
// half of its cycle the foot is on the ground, going back from half a stride ahead to half a
// stride behind at an even pace; in the second it swings forward again, lifted.
[[nodiscard]] glm::vec3 gaitOffset(const Body& b, int side)
{
    const float u = std::fmod(gait.phase / glm::two_pi<float>() + (side == 0 ? 0.f : 0.5f), 1.f);
    float along = 0.5f - 2.f * u;
    float lift = 0.f;
    if(u >= 0.5f)
    {
        const float s = (u - 0.5f) * 2.f;
        along = -0.5f + s * s * (3.f - 2.f * s);
        lift = std::sin(glm::pi<float>() * s);
    }
    return (gait.dir * (along * gait.stride * gait.amount) + UP * (lift * gait.lift * gait.amount + 0.18f * gait.air)) *
           b.m2w;
}

void updateGait(const Body& b, float dt)
{
    const glm::vec3 vel{cl.velocity[0], cl.velocity[1], 0.f};
    const float speed = glm::length(vel) / b.m2w; // metres per second, of the body
    const bool walking = cl.onground && speed > 0.3f && vr_body_walk.value;

    const float ease = 1.f - std::exp(-8.f * dt);
    gait.amount += ((walking ? 1.f : 0.f) - gait.amount) * ease;
    gait.air += ((cl.onground ? 0.f : 1.f) - gait.air) * ease;
    if(walking)
    {
        gait.dir = glm::normalize(vel);
        const float run = std::min(1.f, speed / RUN_SPEED);
        const float sideways = std::abs(glm::dot(gait.dir, b.left));
        const float backwards = std::max(0.f, -glm::dot(gait.dir, b.fwd));
        gait.stride = CLAMP(0.45f, 0.45f + speed * 0.06f, 0.8f) * (1.f - 0.45f * sideways) * (1.f - 0.2f * backwards);
        gait.lift = 0.07f + 0.07f * run;

        // Steps per second: as many as it takes to cover the ground, up to the cap.
        const float cap = CLAMP(0.5f, vr_body_step_rate.value, 6.f) * (0.7f + 0.3f * run);
        const float rate = std::min(speed / gait.stride, cap);
        gait.phase = std::fmod(gait.phase + rate * dt * glm::pi<float>(), glm::two_pi<float>());
    }
}

// Standing, each foot stays planted where it is, turned its own way, while the body turns and
// sways above it. When the body has turned more than vr_body_turn_step degrees from the feet (a
// snap or smooth turn, or turning round in the room; the foot on the side of the turn leads) or
// moved too far from them, a foot steps back under the body, and the other follows to square up:
// one or two small steps, as a person turning on the spot takes. While walking or in the air, the
// feet stay under the body.
struct Foot
{
    glm::vec2 pos{0.f};     // where it stands, world units
    float yaw{0.f};         // degrees
    glm::vec2 from{0.f};    // stepping: where it left from
    float fromYaw{0.f};
    float step{-1.f};       // stepping: 0 .. 1 through the step; below 0, planted
    float lift{0.f};        // metres above the floor
    glm::vec2 slack{0.f};   // walking: how far from under the body it still is, world units
};

struct Stance
{
    bool valid{false};
    bool moving{false};     // walking or in the air
    std::array<Foot, 2> feet{};
    int follow{-1}; // the foot that squares up after the other's step
    float lastYaw{0.f};
    float yawRate{0.f}; // degrees per second the body turns, smoothed (snap turns left out)
};

Stance stance;

constexpr float STEP_TIME = 0.3f;     // seconds a step takes
constexpr float STEP_LIFT = 0.06f;    // metres
constexpr float STEP_DISTANCE = 0.25f; // metres the body may move from the feet before they step

// Degrees from `from` to `to`, -180 .. 180.
[[nodiscard]] float yawDelta(float to, float from)
{
    return std::remainder(to - from, 360.f);
}

[[nodiscard]] glm::vec3 yawForward(float yaw)
{
    const float r = glm::radians(yaw);
    return {std::cos(r), std::sin(r), 0.f};
}

void updateStance(const Body& b, const glm::vec3& head, float dt)
{
    const Bind& bd = bind();
    const float bodyYaw = glm::degrees(std::atan2(b.fwd.y, b.fwd.x));
    if(dt > 0.f)
    {
        const float change = std::abs(yawDelta(bodyYaw, stance.lastYaw));
        const float rate = change < 20.f ? change / dt : 0.f;
        stance.yawRate += (rate - stance.yawRate) * (1.f - std::exp(-8.f * dt));
    }
    stance.lastYaw = bodyYaw;

    // Where the feet stand under the body: under the head (the balance point: crouching pushes the
    // hips back and the knees forward; vr_body_legs_back further back, to match a posture), as
    // far apart as the hips.
    const glm::vec3 centre = head - b.fwd * (vr_body_legs_back.value * b.m2w);
    std::array<glm::vec2, 2> home;
    for(int side = 0; side < 2; side++)
    {
        const glm::vec3 h = centre + b.left * (bd.pos[side == 0 ? ThighL : ThighR].y * b.m2w);
        home[side] = {h.x, h.y};
    }

    // New, or far away (a teleport, a respawn): stand there.
    bool reset = !stance.valid;
    for(int side = 0; side < 2; side++)
    {
        reset = reset || glm::distance(stance.feet[side].pos, home[side]) > 1.2f * b.m2w;
    }
    if(reset)
    {
        for(int side = 0; side < 2; side++)
        {
            stance.feet[side] = Foot{home[side], bodyYaw};
        }
        stance.follow = -1;
        stance.moving = false;
        stance.valid = true;
        return;
    }

    // Walking or in the air: under the body, easing there from where the feet stood (they go with
    // the body, their distance from where they belong shrinking).
    if(gait.amount > 0.02f || gait.air > 0.02f)
    {
        const float keep = std::exp(-10.f * dt);
        for(int side = 0; side < 2; side++)
        {
            Foot& f = stance.feet[side];
            if(!stance.moving)
            {
                f.slack = f.pos - home[side];
            }
            f.slack *= keep;
            f.pos = home[side] + f.slack;
            f.yaw += yawDelta(bodyYaw, f.yaw) * (1.f - keep);
            f.step = -1.f;
            f.lift = 0.f;
        }
        stance.moving = true;
        stance.follow = -1;
        return;
    }
    stance.moving = false;

    // A planted foot left too far behind a turn pivots round after the body (the rest waits for
    // its step).
    const float turnLimit = CLAMP(10.f, vr_body_turn_step.value, 180.f);
    const float maxLag = std::min(turnLimit + 25.f, 180.f);
    for(Foot& f : stance.feet)
    {
        const float lag = yawDelta(bodyYaw, f.yaw);
        if(f.step < 0.f && std::abs(lag) > maxLag)
        {
            f.yaw = bodyYaw - std::copysign(maxLag, lag);
        }
    }

    // A step under way (one foot at a time) goes to where the foot belongs now.
    for(int side = 0; side < 2; side++)
    {
        Foot& f = stance.feet[side];
        if(f.step < 0.f)
        {
            continue;
        }
        // Quicker steps while turning quickly (a smooth turn), to keep up.
        const float quicken = CLAMP(1.f, 1.f + stance.yawRate / 200.f, 2.5f);
        f.step = std::min(1.f, f.step + dt * quicken / STEP_TIME);
        const float e = f.step * f.step * (3.f - 2.f * f.step);
        f.pos = glm::mix(f.from, home[side], e);
        f.yaw = f.fromYaw + yawDelta(bodyYaw, f.fromYaw) * e;
        f.lift = std::sin(glm::pi<float>() * f.step) * STEP_LIFT;
        if(f.step >= 1.f)
        {
            f.step = -1.f;
            f.lift = 0.f;
            stance.follow = stance.follow == side ? -1 : 1 - side;
        }
        return;
    }

    std::array<float, 2> turned, moved;
    for(int side = 0; side < 2; side++)
    {
        turned[side] = yawDelta(bodyYaw, stance.feet[side].yaw);
        moved[side] = glm::distance(stance.feet[side].pos, home[side]) / b.m2w;
    }

    int stepping = -1;
    if(stance.follow >= 0)
    {
        // Squaring up after the other foot's step, if there is anything to square.
        const int side = stance.follow;
        stance.follow = -1;
        if(std::abs(turned[side]) > 5.f || moved[side] > 0.04f)
        {
            stepping = side;
        }
    }
    else if(std::max(std::abs(turned[0]), std::abs(turned[1])) > turnLimit)
    {
        stepping = turned[0] + turned[1] > 0.f ? 0 : 1; // turned left: the left foot leads
    }
    else if(std::max(moved[0], moved[1]) > STEP_DISTANCE)
    {
        stepping = moved[0] > moved[1] ? 0 : 1;
    }

    if(stepping >= 0)
    {
        Foot& f = stance.feet[stepping];
        f.from = f.pos;
        f.fromYaw = f.yaw;
        f.step = 0.f;
    }
}

// Legs from the hips to the feet where they stand (updateStance) and walk (the gait), the knees
// over the toes; collapsed when not shown.
void solveLeg(Body& b, int side, bool shown)
{
    const Bind& bd = bind();
    const int thigh = side == 0 ? ThighL : ThighR;
    const int calf = thigh + 1;
    const int foot = thigh + 2;

    Bone& t = b.bones[thigh];
    t = Bone{};
    t.pos = childPos(b, thigh);

    Bone& c = b.bones[calf];
    Bone& f = b.bones[foot];
    c = Bone{};
    f = Bone{};

    if(!shown)
    {
        for(Bone* bone : {&t, &c, &f})
        {
            bone->pos = t.pos;
            bone->size = COLLAPSED;
        }
        return;
    }

    const Foot& ft = stance.feet[side];
    const glm::vec3 footFwd = yawForward(ft.yaw);
    const glm::vec3 footLeft{-footFwd.y, footFwd.x, 0.f};
    const glm::vec3 outward = side == 0 ? footLeft : -footLeft;
    const float a = boneLength(thigh, calf) * b.m2w;
    const float l = boneLength(calf, foot) * b.m2w;
    const glm::vec3 target =
        glm::vec3{ft.pos.x, ft.pos.y, b.floorZ + (bd.pos[foot].z + ft.lift) * b.m2w} + gaitOffset(b, side);

    // The knee points between the body's forward and the foot's.
    glm::vec3 bend;
    const glm::vec3 kneeDir = safeNormalize(b.fwd + footFwd, b.fwd);
    const glm::vec3 knee = twoBone(t.pos, target, a, l, kneeDir + outward * 0.1f, kneeDir, bend);
    t.rot = basis(knee - t.pos, bend);

    c.pos = knee;
    const glm::vec3 shinDir = safeNormalize(target - knee, -UP);
    c.rot = basis(shinDir, bend);

    // The foot keeps its bind direction, turned to its own yaw.
    const glm::vec3 toe = bd.toe[side] - bd.pos[foot];
    f.pos = knee + shinDir * l;
    f.rot = basis(footFwd * toe.x + footLeft * toe.y + UP * toe.z, UP);
}

// Model bone index of each joint, for the last model checked.
struct ModelInfo
{
    const qmodel_t* model{nullptr};
    bool usable{false};
    std::array<int, JointCount> boneOf{};
};

ModelInfo info;

// The pose being drawn: skinning matrices in model bone order (the model has JointCount bones).
struct Posed
{
    const entity_t* ent{nullptr};
    float scale{0.f};
    std::array<float, JointCount * 12> skin{};
    glm::vec3 wrist[2]{glm::vec3{0.f}, glm::vec3{0.f}};   // per hand
    glm::vec3 forearm[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    Shoulder shoulders[2];                                // per side
};

Posed posed;
int debugFrame = -1;

void queueDebug(const Body& b)
{
    if(!vr_body_debug.value || debugFrame == host_framecount)
    {
        return;
    }
    debugFrame = host_framecount;

    for(int j = 0; j < JointCount; j++)
    {
        const Bone& bone = b.bones[j];
        lines::point(bone.pos, 0.8f, {1.f, 0.9f, 0.2f, 0.8f});
        if(parentOf[j] >= 0)
        {
            lines::line(b.bones[parentOf[j]].pos, bone.pos, 0.3f, {1.f, 0.5f, 0.1f, 0.8f}, {1.f, 0.9f, 0.2f, 0.8f});
        }
        // The bone's hint axis (forward for the spine, the bend for the limbs).
        lines::line(bone.pos, bone.pos + bone.rot[2] * 3.f, 0.2f, {0.2f, 0.6f, 1.f, 0.8f}, {0.2f, 0.6f, 1.f, 0.f});
    }
}

} // namespace

Torso torso(const hands::State& s)
{
    Body b;
    solveTorso(s, b);
    return {{b.bones[Pelvis].pos, b.bones[Pelvis].rot}, {b.bones[Chest].pos, b.bones[Chest].rot}};
}

hands::State standing(const hands::State& s)
{
    hands::State out = s;
    out.head.z += (units::eyeHeight() - s.headHeight) * units::metresToUnits();
    out.headHeight = units::eyeHeight();
    out.headAngles = {0.f, s.bodyYaw, 0.f};
    out.crouchRatio = 0.f;
    return out;
}

Follower::Follower(const hands::State& s) : now(torso(s)), ref(torso(standing(s)))
{
}

glm::vec3 Follower::operator()(Part part, const glm::vec3& standingPoint) const
{
    const Frame& f = part == Part::Pelvis ? now.pelvis : now.chest;
    const Frame& r = part == Part::Pelvis ? ref.pelvis : ref.chest;
    return f.pos + f.rot * (glm::transpose(r.rot) * (standingPoint - r.pos));
}

bool usable(qmodel_t* model)
{
    if(model == info.model)
    {
        return info.usable;
    }

    info = ModelInfo{};
    info.model = model;
    if(!model || model->type != mod_alias)
    {
        return false;
    }

    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(model));
    if(hdr->poseverttype != aliashdr_t::PV_IQM || hdr->numbones != JointCount)
    {
        return false;
    }

    const auto* bones = reinterpret_cast<const boneinfo_t*>(reinterpret_cast<const byte*>(hdr) + hdr->boneinfo);
    for(int j = 0; j < JointCount; j++)
    {
        int found = -1;
        for(int i = 0; i < hdr->numbones; i++)
        {
            if(!strcmp(bones[i].name, jointNames[j]))
            {
                found = i;
            }
        }
        if(found < 0)
        {
            Con_Printf("%s: no bone \"%s\", not usable as the VR body\n", model->name, jointNames[j]);
            return false;
        }
        info.boneOf[j] = found;

        // The bind position (the inverse bind matrix is [R^T | -R^T p]) should be ours.
        const float* m = bones[found].inverse.mat;
        const glm::vec3 t{m[3], m[7], m[11]};
        const glm::vec3 pos{-(m[0] * t.x + m[4] * t.y + m[8] * t.z), -(m[1] * t.x + m[5] * t.y + m[9] * t.z),
            -(m[2] * t.x + m[6] * t.y + m[10] * t.z)};
        if(glm::distance(pos, bind().pos[j] * UNITS) > 0.5f)
        {
            Con_DPrintf("%s: bone \"%s\" is not where vr_avatar.cpp expects it\n", model->name, jointNames[j]);
        }
    }

    info.usable = true;
    return true;
}

glm::vec3 pose(const hands::State& s, qmodel_t* model, const entity_t* ent, const HandPose handPoses[2], bool legs)
{
    QVR_PROFILE("avatar IK");
    Body b;
    solveTorso(s, b);

    // Which hand is on which side.
    const int leftHand = vr_lefthanded.value ? HAND_MAIN : HAND_OFF;
    solveArm(b, 0, handPoses[leftHand]);
    solveArm(b, 1, handPoses[1 - leftHand]);
    const float dt = legsDeltaTime();
    updateGait(b, dt);
    updateStance(b, s.head, dt);
    solveLeg(b, 0, legs);
    solveLeg(b, 1, legs);

    // Preview: the body in front of the player, with its head, facing them (2) or turned to show
    // its left side (3).
    if(vr_body_debug.value >= 2.f)
    {
        const glm::vec3 root{s.head.x, s.head.y, 0.f};
        const glm::vec3 centre = root + b.fwd * (1.8f * b.m2w);
        const float angle = glm::radians(vr_body_debug.value >= 3.f ? -90.f : 180.f);
        const glm::mat3 turn = glm::mat3_cast(glm::angleAxis(angle, UP));
        for(Bone& bone : b.bones)
        {
            bone.pos = centre + turn * (bone.pos - root);
            bone.rot = turn * bone.rot;
        }
        b.bones[Neck].size = b.bones[Head].size = 1.f;
    }

    queueDebug(b);

    // Skinning matrices, relative to the entity (at the pelvis, scaled by k: world units per
    // model unit, so that the bones themselves keep the model's scale and its normals).
    const glm::vec3 origin = b.bones[Pelvis].pos;
    const float k = b.m2w / UNITS;
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(model));
    const auto* bones = reinterpret_cast<const boneinfo_t*>(reinterpret_cast<const byte*>(hdr) + hdr->boneinfo);

    for(int j = 0; j < JointCount; j++)
    {
        const Bone& bone = b.bones[j];
        const glm::mat3 r = bone.rot * glm::mat3{glm::vec3{bone.stretch * bone.size, 0.f, 0.f},
                                           glm::vec3{0.f, bone.size, 0.f}, glm::vec3{0.f, 0.f, bone.size}};
        const glm::vec3 t = (bone.pos - origin) / k;

        const int i = info.boneOf[j];
        const float* inv = bones[i].inverse.mat;
        float* out = &posed.skin[i * 12];
        for(int row = 0; row < 3; row++)
        {
            for(int col = 0; col < 4; col++)
            {
                float v = r[0][row] * inv[col] + r[1][row] * inv[4 + col] + r[2][row] * inv[8 + col];
                if(col == 3)
                {
                    v += t[row];
                }
                out[row * 4 + col] = v;
            }
        }
    }

    posed.ent = ent;
    posed.scale = k;
    const Bind& bd = bind();
    for(int side = 0; side < 2; side++)
    {
        const int clav = side == 0 ? ClavicleL : ClavicleR;
        Shoulder& sh = posed.shoulders[side];
        sh.joint = b.bones[clav + 1].pos;
        sh.clavicle = glm::normalize(glm::quat_cast(b.bones[clav].rot * glm::transpose(bd.rot[clav])));
        sh.upperArm = glm::normalize(glm::quat_cast(b.bones[clav + 1].rot * glm::transpose(bd.rot[clav + 1])));
        sh.m2w = b.m2w;

        const int hand = side == 0 ? leftHand : 1 - leftHand;
        posed.wrist[hand] = b.bones[clav + 3].pos;
        posed.forearm[hand] = b.bones[clav + 2].rot[0];
    }
    return origin;
}

void hide()
{
    posed.ent = nullptr;
}

bool forearm(int hand, glm::vec3& wrist, glm::vec3& direction)
{
    if(!posed.ent)
    {
        return false;
    }
    wrist = posed.wrist[hand];
    direction = posed.forearm[hand];
    return true;
}

bool shoulder(int side, Shoulder& out)
{
    if(!posed.ent || side < 0 || side > 1)
    {
        return false;
    }
    out = posed.shoulders[side];
    return true;
}

float modelScale(const entity_t* e)
{
    return e && e == posed.ent ? posed.scale : 0.f;
}

} // namespace qvr::avatar

extern "C" int VR_AliasBonePoses(const entity_t* e, const float** matrices)
{
    using namespace qvr::avatar;
    if(!e || e != posed.ent)
    {
        return 0;
    }
    if(matrices)
    {
        *matrices = posed.skin.data();
    }
    return JointCount;
}
