// vr_avatar.cpp -- see vr_avatar.hpp. The legs are vr_avatar_legs.cpp, the climbing feet vr_avatar_climb.cpp; what
// they share is vr_avatar_internal.hpp.

#include "vr_modelmetadata.hpp"
#include "vr_trace.hpp"
#include "vr_progs.hpp"
#include "vr_avatar.hpp"
#include "vr_avatar_internal.hpp"
#include "vr_agarmor.hpp"
#include "vr_selfcollide.hpp"
#include "vr_engine.hpp"
#include "vr_units.hpp"
#include "vr_backend.hpp"
#include "vr_bodycal.hpp"
#include "vr_bodytrack.hpp"
#include "vr_box3d.hpp"
#include "vr_climb.hpp"
#include "vr_cvars.hpp"
#include "vr_lines.hpp"
#include "vr_motion.hpp"
#include "vr_profile.hpp"
#include "vr_ragdoll.hpp"
#include "vr_seated.hpp"
#include "vr_view.hpp"

#include "Zancle/Container/Array.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Acos.hpp"
#include "Zancle/Math/Asin.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Copysign.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/Floor.hpp"
#include "Zancle/Math/Fmod.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Remainder.hpp"
#include "Zancle/Math/Round.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/String/String.hpp"
#include "vr_zancle.hpp"
#include "vr_rig2.hpp"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

#include <string.h>
#include "vr_geom.hpp"

namespace qvr::avatar
{
using namespace detail;

namespace
{

// The twist joints' places along the forearm, from the elbow (make_vrbody.py's TWISTS: at its rings).
constexpr int twistJoints = 4;
constexpr float twistPlaces[twistJoints] = {0.05f / 0.26f, 0.12f / 0.26f, 0.19f / 0.26f, 0.225f / 0.26f};

constexpr const char* jointNames[JointCount] = {"pelvis", "spine", "chest", "neck", "head", "clavicle_l",
    "upperarm_l", "forearm_l", "hand_l", "clavicle_r", "upperarm_r", "forearm_r", "hand_r", "thigh_l", "calf_l",
    "foot_l", "thigh_r", "calf_r", "foot_r", "foretwist1_l", "foretwist2_l", "foretwist3_l", "foretwist4_l", "wrist_l",
    "foretwist1_r", "foretwist2_r", "foretwist3_r", "foretwist4_r", "wrist_r"};

constexpr int parentOf[JointCount] = {-1, Pelvis, Spine, Chest, Neck, Chest, ClavicleL, UpperArmL, ForearmL, Chest,
    ClavicleR, UpperArmR, ForearmR, Pelvis, ThighL, CalfL, Pelvis, ThighR, CalfR, ForearmL, ForearmL, ForearmL,
    ForearmL, ForearmL, ForearmR, ForearmR, ForearmR, ForearmR, ForearmR};

// The first of a side's forearm helper joints (ForeTwist1 .. 4, then Wrist).
[[nodiscard]] constexpr int foreHelpers(int side)
{
    return side == 0 ? ForeTwist1L : ForeTwist1R;
}

} // namespace

namespace detail
{

// Columns: x along `dir`, z towards `hint`, y = z x x (make_vrbody.py's frame()).
[[nodiscard]] glm::mat3 basis(const glm::vec3& dir, const glm::vec3& hint)
{
    const glm::vec3 x = safeNormalize(dir);
    glm::vec3 z = hint - x * glm::dot(hint, x);
    if(glm::length(z) < 1e-5f)
    {
        z = glm::cross(x, za::abs(x.z) < 0.9f ? UP : FWD);
    }
    z = glm::normalize(z);
    return glm::mat3(x, glm::cross(z, x), z);
}

} // namespace detail

namespace
{

// Built before main (no first-call guard): read by any thread.
const Bind bindPose = [] {
    Bind r{};
    r.pos[Pelvis] = {0.f, 0.f, 0.95f};
    r.pos[Spine] = {-0.01f, 0.f, 1.10f};
    r.pos[Chest] = {-0.01f, 0.f, 1.28f};
    r.pos[Neck] = {-0.01f, 0.f, 1.47f};
    r.pos[Head] = {0.f, 0.f, 1.57f};

    const float s30 = za::sin(glm::radians(30.f));
    const float c30 = za::cos(glm::radians(30.f));
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
        for(int k = 0; k <= twistJoints; k++)
        {
            const int j = foreHelpers(side) + k;
            r.pos[j] = glm::mix(r.pos[clav + 2], r.pos[clav + 3], k < twistJoints ? twistPlaces[k] : 1.f);
            r.rot[j] = r.rot[clav + 2];
        }

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

} // namespace

namespace detail
{

const Bind& bind()
{
    return bindPose;
}

} // namespace detail

namespace
{

// A joint's offset from its parent, in the parent's bind frame, in metres.
[[nodiscard]] const glm::vec3& localOffset(int joint)
{
    return bind().offset[joint];
}

} // namespace

namespace detail
{

[[nodiscard]] float boneLength(int from, int to)
{
    return glm::distance(bind().pos[from], bind().pos[to]);
}

} // namespace detail

namespace
{

// A turn in the body's frame: `pitch` degrees tips its up towards its forward (about its left), `yaw` turns its forward
// towards its left (about the vertical), `roll` tips its up towards its right (about its forward).
[[nodiscard]] glm::mat3 bodyTurn(const Body& b, float pitch, float yaw, float roll)
{
    return glm::mat3_cast(glm::angleAxis(glm::radians(yaw), UP) * glm::angleAxis(glm::radians(pitch), b.left) *
                          glm::angleAxis(glm::radians(roll), b.fwd));
}

} // namespace

namespace detail
{

// Climbing's weight (Climbing::weight, 0 .. 1), for solveTorso: the lean taken out while climbing.
float climbHang = 0.f;

// World position of `joint` from its posed parent.
[[nodiscard]] glm::vec3 childPos(const Body& b, int joint)
{
    const Bone& p = b.bones[parentOf[joint]];
    glm::vec3 o = localOffset(joint) * (b.m2w * p.size);
    o.x *= p.stretch;
    return p.pos + p.rot * o;
}

} // namespace detail

namespace
{

// The legs fitted to the height you stand at (vr_body_leg_fit; ROUND22.md, "Body and Equipment"). The body is scaled to
// the Height setting (vr_height_calibration) and hangs from the head: standing taller than it (a Height set low), the
// legs were too short and the feet hung in the air (eyes at 1.70 m, Height 1.55: the ankles 23 cm up), standing lower
// (a Height set high) the knees stayed bent and the body crouched; Body Calibration fixed it only by measuring the
// height again. Here the legs (hip to ankle; the feet keep the body's size) are lengthened or shortened so that, at the
// height the headset sees you stand at (hands::State::standingHeight, learnt as Leaning Detection learns it: it rises
// quickly as you stand up straighter, settles slowly), the feet reach the floor with the knees straight, between 0.75
// and 1.3 times; the crouch is measured from there too. Sets b.legM2w; returns how much higher than the scaled body
// that stands (world units). Seated, with the reference poses (standingHeight their own height) or off: none.
[[nodiscard]] float legFit(const hands::State& s, Body& b)
{
    b.legM2w = b.m2w;
    const float m2u = units::metresToUnits();
    const float scaledFor = b.m2w / m2u * units::modelEyeHeight; // the eye height the body is scaled to
    if(!vr_body_leg_fit.value || seated::active() || s.standingHeight <= 0.f)
    {
        return 0.f;
    }
    const float leg = (boneLength(ThighL, ThighL + 1) + boneLength(ThighL + 1, ThighL + 2)) * b.m2w;
    if(leg <= 0.f)
    {
        return 0.f;
    }
    const float fit = CLAMP(0.75f, 1.f + (s.standingHeight - scaledFor) * m2u / leg, 1.3f);
    b.legM2w = b.m2w * fit;
    return (fit - 1.f) * leg;
}

// The spine from the head. The top of the neck is found behind and below the eyes. A crouch lowers
// the pelvis straight down under the neck (the legs bend) and tilts the back forward about it, as a
// person crouching does; when the pelvis can go no lower, the back bends further.
//
// Leaning (the head off the box's middle, hands::State::lean, where the body stands: vr_lean_radius,
// vr_lean_detect): the back tilts towards the lean about the hips, which stay over the feet, as far as
// the head has gone down for it (a tilt swings the head down on an arc); the hips shift the rest of the
// way (a lean with the head kept high: the legs slant).
//
// The crouched pose (vr_body_crouch_*, `crouchPose`; off for the reference poses): as deep as the body crouches
// (crouchPoseWeight), the neck sits further from the eyes, the trunk (pelvis, spine, chest) moves and the spine and chest
// turn about the spine's base, the pelvis moves and tips on top; the shoulders and the elbows in solveArm.
void solveTorso(const hands::State& s, Body& b, float scale = 0.f, bool crouchPose = true)
{
    const Bind& bd = bind();
    b.m2w = units::metresToUnits() * (scale > 0.f ? scale : units::bodyScale());
    b.floorZ = s.head.z - s.headHeight * units::metresToUnits();
    const float standLift = legFit(s, b);

    glm::vec3 right, up;
    hands::angleVectors({0.f, s.bodyYaw, 0.f}, b.fwd, right, up);
    b.left = -right;

    glm::vec3 hf, hr, hu;
    hands::angleVectors(s.headAngles, hf, hr, hu);
    const glm::vec3 standingTop = s.head - (hf * vr_body_eye_forward.value + hu * vr_body_eye_up.value) * b.m2w;

    const float torsoLen = glm::distance(bd.pos[Pelvis], bd.pos[Head]) * b.m2w;

    // The lean's tilt: how far out it swings the top of the neck (at most as far as the head has gone down allows, and
    // a fifth of the lean always in the hips), and how much of the drop below the calibrated height that takes; the
    // rest is a crouch.
    // Climbing (updateClimbing, last frame's), the body hangs under the head, by the face, not leant back over the box's
    // middle: its feet within reach of footholds (not the reference poses).
    const float leanKeep = crouchPose ? 1.f - climbHang : 1.f;
    const glm::vec3 leanOff{s.lean.x * leanKeep, s.lean.y * leanKeep, 0.f};
    const float leanLen = glm::length(leanOff);
    float tiltReach = 0.f;
    if(leanLen > 0.01f)
    {
        // Down from where the player stands up straight (learnt: the calibrated height is only close to it).
        const float leanDrop = za::max(0.f, s.standingHeight - s.headHeight) * units::metresToUnits();
        const float h = za::min(leanDrop, torsoLen);
        const float allowed = za::sqrt(za::max(0.f, torsoLen * torsoLen - (torsoLen - h) * (torsoLen - h)));
        // Only as sure as it is a lean (vr_lean_detect): the body lagging the head as the player walks in the room
        // stays upright, the hips with the head, the feet catching up.
        const float sure = vr_lean_detect.value > 0.f ? s.leanHold : 1.f;
        tiltReach = sure * za::min(leanLen * 0.8f, allowed, 0.7f * torsoLen);
    }
    // Body tracking (vr_bodytrack.cpp; not the reference poses): the tracked spine's tilt blended in, which then also
    // tells the crouch from the lean (the drop it does not explain is a crouch) and replaces the crouch's own tilt.
    const bodytrack::Trunk tracked = crouchPose ? bodytrack::trunk() : bodytrack::Trunk{};
    glm::vec3 leanDir = leanLen > 0.01f ? leanOff / leanLen : b.fwd;
    if(tracked.weight > 0.f)
    {
        const glm::vec3 v = glm::mix(leanDir * tiltReach, tracked.dir * (torsoLen * za::sin(tracked.tilt)), tracked.weight);
        tiltReach = za::min(glm::length(v), 0.95f * torsoLen);
        leanDir = tiltReach > 1e-4f ? glm::normalize(v) : leanDir;
    }
    const float tiltDrop = torsoLen - za::sqrt(torsoLen * torsoLen - tiltReach * tiltReach);

    // How deep the crouch is: 0 standing, 1 with the pelvis at squatting height.
    // (Standing: as high as the height you stand at, the legs fitted to it: legFit.)
    const float standZ = b.floorZ + standLift + bd.pos[Pelvis].z * b.m2w;
    const float squatZ = b.floorZ + 0.3f * b.legM2w;
    const auto depthUnder = [&](const glm::vec3& neck) {
        const float headDrop = za::max(0.f, b.floorZ + standLift + bd.pos[Head].z * b.m2w - neck.z);
        const float drop = za::max(0.f, headDrop - tiltDrop);
        return standZ > squatZ ? za::min(1.f, drop / (standZ - squatZ)) : 0.f;
    };

    // The crouched pose's share, from the crouch under the standing neck (the neck's own crouched offset then lowers it
    // a little more, which the pose below follows).
    b.crouchPose = crouchPose ? crouchPoseWeight(depthUnder(standingTop)) : 0.f;
    const float w = b.crouchPose;
    const glm::vec3 top = standingTop - (hf * vr_body_crouch_eye_forward.value + hu * vr_body_crouch_eye_up.value) * (w * b.m2w);
    const glm::vec3 top0 = top + UP * tiltDrop; // the neck as it would be without the tilt
    const float crouch = depthUnder(top);
    const float tilt = glm::radians(CLAMP(0.f, vr_body_crouch_tilt.value, 80.f)) * crouch * (1.f - tracked.weight);
    float pelvisZ = za::max(top0.z - torsoLen * za::cos(tilt), squatZ);
    pelvisZ = za::min(pelvisZ, top0.z - 0.3f * torsoLen); // lying down: keep the back from folding over
    const float dz = top0.z - pelvisZ;
    const float lean = dz < torsoLen ? za::sqrt(torsoLen * torsoLen - dz * dz) : 0.f;

    // The torso sits vr_body_torso_back behind the neck (looking down shows the chest rather than
    // the top of the shoulders), the pelvis under it; the back leans forward from there.
    const glm::vec3 back = b.fwd * (vr_body_torso_back.value * b.m2w);
    const glm::vec3 pelvis0 = glm::vec3{top0.x, top0.y, pelvisZ} - back;
    const glm::vec3 axis = safeNormalize(b.fwd * lean + UP * za::min(dz, torsoLen));

    // Tilted about the hips towards the lean, so that the neck comes to where it is: the hips end up over the feet
    // (less what the tilt could not reach).
    glm::mat3 turn{1.f};
    if(tiltReach > 0.f)
    {
        const glm::vec3 spine = top0 - pelvis0;
        const float angle = za::asin(za::min(1.f, tiltReach / za::max(glm::length(spine), 1e-3f)));
        turn = glm::mat3_cast(glm::angleAxis(angle, safeNormalize(glm::cross(UP, leanDir), b.left)));
    }
    const glm::vec3 pelvis = top - turn * (top0 - pelvis0);

    Bone& p = b.bones[Pelvis];
    p = Bone{};
    p.pos = pelvis;
    p.rot = turn * basis(glm::mix(UP, axis, 0.4f), b.fwd);

    Bone& sp = b.bones[Spine];
    sp = Bone{};
    sp.pos = childPos(b, Spine);
    sp.rot = turn * basis(glm::mix(UP, axis, 0.75f), b.fwd);

    Bone& c = b.bones[Chest];
    c = Bone{};
    c.pos = childPos(b, Chest);
    c.rot = turn * basis(axis, b.fwd);

    // The crouched pose: the trunk moves (the legs follow the pelvis), the spine and chest turn about the spine's base,
    // the pelvis moves and tips on top (in the body's facing; Body Calibration's upright chest never has it).
    if(w != 0.f)
    {
        const glm::vec3 bodyRight = -b.left;
        const glm::vec3 trunk = (-b.fwd * vr_body_crouch_torso_back.value + UP * vr_body_crouch_torso_up.value +
                                    bodyRight * vr_body_crouch_torso_right.value) *
                                (w * b.m2w);
        const glm::vec3 hips = (-b.fwd * vr_body_crouch_pelvis_back.value + UP * vr_body_crouch_pelvis_up.value) * (w * b.m2w);
        const glm::mat3 torsoTurn = bodyTurn(b, vr_body_crouch_torso_pitch.value * w, vr_body_crouch_torso_yaw.value * w,
            vr_body_crouch_torso_roll.value * w);
        const glm::vec3 chestFromSpine = c.pos - sp.pos;
        p.pos += trunk + hips;
        p.rot = bodyTurn(b, vr_body_crouch_pelvis_pitch.value * w, 0.f, 0.f) * p.rot;
        sp.pos += trunk;
        sp.rot = torsoTurn * sp.rot;
        c.pos = sp.pos + torsoTurn * chestFromSpine;
        c.rot = torsoTurn * c.rot;
    }
    // The tracked chest's twist from the hips (the hips face bodyYaw: bodytrack::bodyYaw), half in the spine.
    if(tracked.weight > 0.f && tracked.twist != 0.f)
    {
        const glm::vec3 axis = safeNormalize(c.pos - p.pos, UP);
        const glm::mat3 half = glm::mat3_cast(glm::angleAxis(glm::radians(0.5f * tracked.twist * tracked.weight), axis));
        const glm::vec3 chestFromSpine = c.pos - sp.pos;
        sp.rot = half * sp.rot;
        c.pos = sp.pos + half * chestFromSpine;
        c.rot = half * half * c.rot;
    }
    // And the trunk swung about the top of the neck until the spine (the pelvis to the neck) has the tracked tilt from its
    // upright direction (what the crouched pose's own turns and moves left), by the torso weight.
    if(tracked.weight > 0.f)
    {
        const glm::vec3 upright = safeNormalize(UP * torsoLen + back);
        const glm::vec3 tiltAxis = safeNormalize(glm::cross(UP, tracked.dir), b.left);
        const glm::vec3 want = glm::angleAxis(tracked.tilt, tiltAxis) * upright;
        const glm::vec3 now = safeNormalize(top - p.pos);
        const glm::vec3 axis = glm::cross(now, want);
        const float s = glm::length(axis);
        if(s > 1e-5f)
        {
            const float angle = za::atan2(s, glm::dot(now, want)) * tracked.weight;
            const glm::mat3 swing = glm::mat3_cast(glm::angleAxis(angle, axis / s));
            for(Bone* bone : {&p, &sp, &c})
            {
                bone->pos = top + swing * (bone->pos - top);
                bone->rot = swing * bone->rot;
            }
        }
    }

    // The neck spans the rest of the way to the top of the neck; the head sits there. Neither is
    // drawn: the eyes are inside them.
    Bone& n = b.bones[Neck];
    n = Bone{};
    n.pos = childPos(b, Neck);
    n.rot = basis(top - n.pos, turn * b.fwd);
    n.size = COLLAPSED;

    Bone& h = b.bones[Head];
    h = Bone{};
    h.pos = top;
    h.rot = basis(hu, hf);
    h.size = COLLAPSED;
}

} // namespace

namespace detail
{

// Two-bone chain from `root` towards `target`, bending towards `pole`: the middle joint.
[[nodiscard]] glm::vec3 twoBone(const glm::vec3& root, const glm::vec3& target, float a, float b, const glm::vec3& pole,
    const glm::vec3& fallbackPole, glm::vec3& bend)
{
    const glm::vec3 toTarget = target - root;
    const float d = glm::length(toTarget);
    const glm::vec3 dir = d > 1e-4f ? toTarget / d : -UP;
    const float reach = CLAMP(za::abs(a - b) + 1e-3f, d, a + b - 1e-4f);

    const float cosA = CLAMP(-1.f, (a * a + reach * reach - b * b) / (2.f * a * reach), 1.f);
    const float sinA = za::sqrt(za::max(0.f, 1.f - cosA * cosA));

    bend = pole - dir * glm::dot(pole, dir);
    if(glm::length(bend) < 1e-3f)
    {
        bend = fallbackPole - dir * glm::dot(fallbackPole, dir);
    }
    bend = safeNormalize(bend, glm::cross(dir, UP));

    return root + dir * (a * cosA) + bend * (a * sinA);
}

} // namespace detail

namespace
{

// The wrist's turn from a forearm (along `foreDir`, its hint `hint`) to the hand (`handRot`, the hand bone's axes) of
// the arm on `side` (0 left, 1 right): its roll about the forearm (radians, -pi .. pi) and, in the rolled forearm's
// axes, the bend's flexion (positive towards the palm, negative towards the back of the hand: extension) and deviation
// (positive towards the little finger: ulnar, negative towards the thumb: radial) (radians, the bend's angle split by
// its direction). The hint is the side of the forearm away from the elbow's bend, where the little finger is with the
// thumb up (no roll).
struct WristTurn
{
    float twist, flexion, deviation;
};

[[nodiscard]] WristTurn wristTurn(const glm::vec3& foreDir, const glm::vec3& hint, const glm::mat3& handRot, int side)
{
    const glm::mat3 untwisted = basis(foreDir, hint);
    const glm::quat turn = glm::normalize(glm::quat_cast(handRot * glm::transpose(untwisted)));
    float twist = 2.f * za::atan2(glm::dot(glm::vec3{turn.x, turn.y, turn.z}, foreDir), turn.w);
    twist -= glm::two_pi<float>() * za::round(twist / glm::two_pi<float>());
    const glm::vec3 h = glm::transpose(glm::mat3_cast(glm::angleAxis(twist, foreDir)) * untwisted) * handRot[0];
    const float bend = za::acos(CLAMP(-1.f, h.x, 1.f));
    const float across = za::sqrt(h.y * h.y + h.z * h.z);
    // The rolled forearm's y (its z, the little finger's side, times its x) is the back of a right hand and the palm
    // of a left one (the hands mirror each other).
    const float toPalm = side == 0 ? 1.f : -1.f;
    return {twist, across > 1e-6f ? toPalm * bend * h.y / across : 0.f, across > 1e-6f ? bend * h.z / across : 0.f};
}

// vr_debug_arm 1: the swing's cost every 15 degrees from -180 (easeWrist), printed with the arm (traceArm).
struct ArmCostTrace
{
    static constexpr int count = 24;
    bool want = false;
    float cost[count]{};
};
ArmCostTrace costTrace;

// How far past a wrist's reach a turn is (0 within it; vr_body_wrist_limits scales the reach). A wrist's range is
// lopsided: about 85 degrees of flexion and 80 of extension, 45 towards the little finger and 30 towards the thumb (a
// hand gripping something rests in a little extension and ulnar deviation). The bend is measured against that range,
// quadrant by quadrant (an ellipse's quarter in each), and strains only 15% past it, reaching 1 at 45% past: a real
// elbow doesn't move while the wrist bends, even hard; only a bend no wrist makes says the forearm is elsewhere. The roll strains from 60 degrees (a forearm turns about
// 85 either way; past that, the upper arm turns and the elbow swings), reaching 1 at 85. A strain of 1 is worth
// swinging the elbow 45 degrees (easeWrist).
[[nodiscard]] float wristStrain(const WristTurn& t, float limits)
{
    const float flexRange = t.flexion >= 0.f ? 85.f : 80.f;
    const float devRange = t.deviation >= 0.f ? 45.f : 30.f;
    const float bend = za::sqrt(glm::pow(t.flexion / glm::radians(flexRange * limits), 2.f) +
                                 glm::pow(t.deviation / glm::radians(devRange * limits), 2.f));
    const float bendStrain = za::max(0.f, bend - 1.15f) / 0.3f;
    const float twistStrain = za::max(0.f, za::abs(t.twist) - glm::radians(60.f * limits)) / glm::radians(25.f);
    return bendStrain * bendStrain + twistStrain * twistStrain;
}

// The elbow's swing round the line from the shoulder to the wrist (radians) that best eases the wrist (the hand bone's
// axes `handRot`) from the arm's `elbow` and its bend `bend`: the least of the wrist's strain plus the swing's own
// cost (so that within the wrist's reach the elbow stays where the pole puts it). The swing nearest the last one
// (`last`) is kept unless another is clearly better, so that where two are about as good (the hand upside down) the
// elbow does not flick between them.
// With the hand near the face or the chest the arm is folded, the elbow far off the line from the shoulder to the wrist,
// and the swing that eases the wrist most could put it by the neck or across the chest, or into the torso (where the
// torso then pushes it out the shorter way, up or down, whatever that does to the wrist). So a swing costs more
// (vr_body_elbow_lift: `lift`) that takes the elbow (its joint `elbowRadius` thick) into the torso, or across the chest
// past 50 degrees from straight ahead (a shoulder's horizontal reach across), or over the shoulder while across it;
// `up`, `outward` and `forward` are the chest's up, the clavicle's way out and the chest's forward, `upperArm` its
// length. Crossing and rising count only past where the pole puts the elbow: an arm the pole already raises or crosses
// (reaching up, or across the body) swings as before. Each costs 1 (as a 45 degree swing) 0.4 of the upper arm (10 cm)
// past it, times `lift`; the torso four times that. An elbow the pole spreads out to the side (`spread`, 0 .. 1:
// solveArm) also costs for rising past where the pole puts it, across the chest or not.
// A tucked arm (`tuck`, 0 .. 1: elbowTuck) keeps its elbow where the pole puts it (down and back): the wrist's strain
// counts for less (down to 0.15 of it) and the swing for more (up to 7 times), so that the elbow swings out or across
// only a little for a hand turned past a wrist's reach by the face, and never round to the neck.
[[nodiscard]] float easeWrist(const glm::vec3& shoulder, const glm::vec3& wrist, const glm::vec3& elbow,
    const glm::vec3& bend, const glm::mat3& handRot, int side, float limits, float last, const glm::vec3& up,
    const glm::vec3& outward, const glm::vec3& forward, float upperArm, float lift, float elbowRadius,
    float spread, float tuck)
{
    const float strainWeight = 1.f - 0.85f * tuck;
    const float swingWeight = 1.f + 6.f * tuck;
    const glm::vec3 axis = safeNormalize(wrist - shoulder);
    const glm::vec3 centre = shoulder + axis * glm::dot(elbow - shoulder, axis);
    const float most = glm::radians(150.f);
    const float freeRise = za::max(0.f, glm::dot(elbow - shoulder, up));
    const float freeIn = za::max(0.f, -glm::dot(elbow - shoulder, outward));
    const float liftScale = 0.4f * upperArm;
    constexpr float crossing = 1.192f; // tan 50 degrees: how far across, per its distance ahead, an elbow goes freely
    const auto cost = [&](float swivel) {
        const glm::quat q = glm::angleAxis(swivel, axis);
        const glm::vec3 swung = centre + q * (elbow - centre);
        const glm::vec3 foreDir = safeNormalize(wrist - swung, axis);
        const float s = swivel / glm::radians(45.f);
        float c = strainWeight * wristStrain(wristTurn(foreDir, q * bend, handRot, side), limits) + swingWeight * s * s;
        if(lift > 0.f)
        {
            const glm::vec3 e = swung - shoulder;
            const float across = -glm::dot(e, outward);
            const float in = za::max(0.f, across - za::max(freeIn, za::max(0.f, glm::dot(e, forward)) * crossing)) / liftScale;
            const float rise = za::max(0.f, glm::dot(e, up) - freeRise) / liftScale *
                               za::max(spread, CLAMP(0.f, across / liftScale, 1.f));
            const float inside = selfcollide::elbowDepth(swung, elbowRadius) / liftScale;
            c += lift * (rise * rise + in * in + 4.f * inside * inside);
        }
        return c;
    };

    // A swing's cost is at least its own term (the wrist's strain and the lift's are never negative): a swing whose own
    // term is no less than the best cost so far cannot beat it, and is not tried (the same result, far fewer tries while
    // the wrist is within its reach). The same expression as cost's: rounding keeps the sum no less than it.
    const bool prune = strainWeight >= 0.f;
    const auto swingAlone = [&](float swivel) {
        const float s = swivel / glm::radians(45.f);
        return swingWeight * s * s;
    };

    // The least cost within `reach` steps of `step` from `from`, then finer about it, and last the least of the
    // parabola through it and its neighbours (so that the elbow moves smoothly as the hand does).
    const auto search = [&](float from, float step, int reach, float& found) {
        float best = CLAMP(-most, from, most);
        float bestCost = cost(best);
        for(int stage = 0; stage < 3; stage++, step *= 0.2f, reach = 5)
        {
            const float at = best;
            for(int i = -reach; i <= reach; i++)
            {
                const float swivel = at + step * static_cast<float>(i);
                if(i == 0 || za::abs(swivel) > most || (prune && swingAlone(swivel) >= bestCost))
                {
                    continue;
                }
                if(const float c = cost(swivel); c < bestCost)
                {
                    best = swivel;
                    bestCost = c;
                }
            }
        }
        step *= 5.f; // the last stage's
        const float below = cost(best - step);
        const float above = cost(best + step);
        if(const float curve = below + above - 2.f * bestCost; curve > 1e-6f)
        {
            best = CLAMP(-most, best + CLAMP(-0.5f, 0.5f * (below - above) / curve, 0.5f) * step, most);
        }
        found = best;
        return cost(best);
    };
    if(costTrace.want)
    {
        for(int i = 0; i < ArmCostTrace::count; i++)
        {
            costTrace.cost[i] = cost(glm::radians(15.f * static_cast<float>(i - ArmCostTrace::count / 2)));
        }
    }
    float anywhere, nearby;
    const float anywhereCost = search(0.f, glm::radians(10.f), 15, anywhere);
    const float nearbyCost = search(last, glm::radians(2.f), 15, nearby);
    return anywhereCost < 0.8f * nearbyCost - 0.05f ? anywhere : nearby;
}

// Shoulder and arm of `side` (0 left, 1 right) to the wrist.
// Whether the arm IK swings the elbow to ease the wrist (vr_body_wrist_limits): for the drawn arms. The arms solved
// with no body drawn (solveArms: the wrist gadget's forearm) keep the elbow where its pole puts it, as a real forearm
// stays while the hand bends; there is no drawn arm to spare a strained wrist.
bool easeWrists = true;

// The body being posed has bare forearms (vrbody*_bare: vr_body_bracers 0), their ends fitted round the jointed hand
// (make_vrbody.py: hand_ring), which the wrist and hand bones scale with the hand as drawn (solveArm).
bool bareArms = false;

// make_vrbody.py's HAND_UNIT: metres of the modelled body a hand unit at the default sizes, the size the bare
// forearms' ends are fitted to the hand at.
// The bare forearms' constants are generated by make_vrbody.py (vr_bodybare.inc).
namespace bodybare
{
#include "vr_bodybare.inc"
} // namespace bodybare
constexpr float handUnit = bodybare::handUnit;

// vr_debug_arm: a drawn arm's IK, in the body's axes (forward, left, up; cm from the chest) and in the tracking
// space's (metres from the head: right, up, back; to set up vr_mock_hand poses): the shoulder, the elbow (and where
// the pole alone put it), the wrist, the elbow's angle and swing, the hand's axes, and the wrist's turn against the
// solved forearm (and against the pole's), with its strain, how tucked the elbow is (elbowTuck) and its swing out of
// the torso (degrees). 1: printed once; 2: every frame into arm_trace.txt (the
// game directory), whether a take plays (vr_motion_play) and the client's time.
glm::vec3 debugHead{0.f}; // the head this frame

// The debug traces' open files (vr_debug_arm 2, vr_debug_lean: the main thread's frame).
struct Traces
{
    FILE* arm = nullptr;  // arm_trace.txt (traceArm)
    FILE* lean = nullptr; // lean_trace.txt (traceLean)
};
Traces traces;

void traceArm(const Body& b, int side, const glm::vec3& shoulder, const glm::vec3& poleElbow, const glm::vec3& poleBend,
    const glm::vec3& elbow, const glm::vec3& bend, const glm::vec3& wrist, const glm::mat3& handRot, float swivel,
    float tuck, float out)
{
    const glm::vec3 o = b.bones[Chest].pos;
    const auto dir = [&](const glm::vec3& v) { return glm::vec3{glm::dot(v, b.fwd), glm::dot(v, b.left), glm::dot(v, UP)}; };
    const auto cm = [&](const glm::vec3& v) { return dir(v - o) * (100.f / b.m2w); };
    const auto tr = [&](const glm::vec3& v) {
        return glm::vec3{-glm::dot(v, b.left), glm::dot(v, UP), -glm::dot(v, b.fwd)};
    };
    const auto trm = [&](const glm::vec3& v) { return tr(v - debugHead) / units::metresToUnits(); };
    const auto turnFrom = [&](const glm::vec3& e, const glm::vec3& h) {
        return wristTurn(safeNormalize(wrist - e, glm::normalize(e - shoulder)), h, handRot, side);
    };
    const float limits = vr_body_wrist_limits.value > 0.f ? vr_body_wrist_limits.value : 1.f; // (0: as at 1)
    const WristTurn t = turnFrom(elbow, bend), t0 = turnFrom(poleElbow, poleBend);
    const float angle = glm::degrees(za::acos(
        CLAMP(-1.f, glm::dot(glm::normalize(shoulder - elbow), safeNormalize(wrist - elbow, UP)), 1.f)));
    const glm::vec3 S = cm(shoulder), E = cm(elbow), E0 = cm(poleElbow), W = cm(wrist), hf = dir(handRot[0]),
                    hu = dir(-handRot[2]);
    const char* line = va("%s S %.1f %.1f %.1f E %.1f %.1f %.1f E0 %.1f %.1f %.1f W %.1f %.1f %.1f elbow %.1f swivel %.1f "
                          "hand f %.3f %.3f %.3f u %.3f %.3f %.3f wrist flex %.1f dev %.1f twist %.1f strain %.3f "
                          "pole flex %.1f dev %.1f twist %.1f strain %.3f tuck %.2f torso %.1f",
        side == 0 ? "L" : "R", S.x, S.y, S.z, E.x, E.y, E.z, E0.x, E0.y, E0.z, W.x, W.y, W.z, angle,
        glm::degrees(swivel), hf.x, hf.y, hf.z, hu.x, hu.y, hu.z, glm::degrees(t.flexion), glm::degrees(t.deviation),
        glm::degrees(t.twist), wristStrain(t, limits), glm::degrees(t0.flexion), glm::degrees(t0.deviation),
        glm::degrees(t0.twist), wristStrain(t0, limits), tuck, glm::degrees(out));
    if(vr_debug_arm.value >= 2.f)
    {
        FILE*& file = traces.arm;
        if(!file && !(file = fopen(va("%s/arm_trace.txt", com_gamedir), "w")))
        {
            return;
        }
        fprintf(file, "%d %.4f %s\n", motion::playing() ? 1 : 0, cl.time, line);
        return;
    }
    Con_Printf("arm %s\n", line);
    if(costTrace.want)
    {
        Con_Printf("armcost %s", side == 0 ? "L" : "R");
        for(const float c : costTrace.cost)
        {
            Con_Printf(" %.2f", c);
        }
        Con_Printf("\n");
    }
    const glm::vec3 ts = trm(shoulder), te = trm(elbow), tw = trm(wrist), tf = tr(handRot[0]), tu = tr(-handRot[2]);
    Con_Printf("armT %s S %.4f %.4f %.4f E %.4f %.4f %.4f W %.4f %.4f %.4f F %.4f %.4f %.4f U %.4f %.4f %.4f\n",
        side == 0 ? "L" : "R", ts.x, ts.y, ts.z, te.x, te.y, te.z, tw.x, tw.y, tw.z, tf.x, tf.y, tf.z, tu.x, tu.y, tu.z);
    if(side == 1)
    {
        Cvar_SetValueQuick(&vr_debug_arm, 0.f);
    }
}

// The arms' lengths in world units: the upper arm (the shoulder joint to the elbow) and the forearm (the elbow to the
// drawn hand's wrist). Measured (Body Calibration: vr_bodycal_upper_arm and vr_bodycal_forearm, real centimetres), or the
// model's proportions times vr_body_arm_length; the player's tweak on top (vr_body_tweak_*, cm).
struct ArmLengths
{
    float upper{0.f}, fore{0.f};
    bool calibrated{false};
};
[[nodiscard]] ArmLengths armLengths(float m2w)
{
    const float length = CLAMP(0.5f, vr_body_arm_length.value, 2.f);
    const float cm = units::metresToUnits() * 0.01f;
    ArmLengths r;
    const float mu = bodycal::measuredArmCm(0), mf = bodycal::measuredArmCm(1);
    r.upper = za::max(5.f * cm, (mu > 0.f ? mu * cm : boneLength(UpperArmL, ForearmL) * m2w * length) + bodycal::armTweakCm(0) * cm);
    r.fore = za::max(5.f * cm, (mf > 0.f ? mf * cm : boneLength(ForearmL, HandL) * m2w * length) + bodycal::armTweakCm(1) * cm);
    r.calibrated = bodycal::calibrated();
    return r;
}

// The clavicle's turn about the base of the neck for a wrist `reach` from the shoulder at rest (the clavicle along
// `lateral`, the chest's up and forward; the arm `armLen` long): it rises by up to `upDegrees` as the hand goes up, and
// swings forward by up to `forwardDegrees` as the hand reaches far forward (from half the arm's length in front of the
// shoulder to 0.9 of it). The model's shoulders rise from the hand level with the shoulder to 0.6 of the arm above it;
// calibrated ones (Body Calibration), continuously from the hand 0.6 of the arm below the shoulder (the arm about 37
// degrees below level) to straight up, as a real shoulder rises through the arm's whole lift (the clavicle's elevation
// with the humerus's, after Ludewig et al. 2009): the calibration's fit measures its amount with this same model.
[[nodiscard]] glm::mat3 clavicleTurn(const glm::vec3& reach, const glm::vec3& lateral, const glm::vec3& cUp, const glm::vec3& cFwd,
    float armLen, float upDegrees, float forwardDegrees, bool calibrated)
{
    const float from = calibrated ? -0.6f : 0.f;
    const float to = calibrated ? 1.f : 0.6f;
    const float raiseAmount = CLAMP(0.f, (glm::dot(reach, cUp) / armLen - from) / (to - from), 1.f);
    const float swingAmount = CLAMP(0.f, (glm::dot(reach, cFwd) / armLen - 0.5f) / 0.4f, 1.f);
    const glm::quat raise = glm::angleAxis(glm::radians(upDegrees * raiseAmount), safeNormalize(glm::cross(lateral, cUp), cFwd));
    const glm::quat swing =
        glm::angleAxis(glm::radians(forwardDegrees * swingAmount), safeNormalize(glm::cross(lateral, cFwd), cUp));
    return glm::mat3_cast(raise * swing);
}

// How tucked a folded arm's elbow is (0 .. 1, vr_body_elbow_tuck): the wrist within vr_body_elbow_tuck_near of the arm's
// length `armLen` from the shoulder (the hand at the face or the chest) fully, fading out by vr_body_elbow_tuck_far
// (the arm out in front). Only with the hand in front of the shoulder (fading in over its first 0.15 of the arm's
// length) and not raised to the forehead or over it (fading out from 0.15 of the arm's length above the shoulder to
// 0.32): a hand there, or behind the head, lifts the elbow up and out, as a person's does. `up` and `forward` are the
// chest's.
[[nodiscard]] float elbowTuck(const glm::vec3& shoulder, const glm::vec3& wrist, float armLen, const glm::vec3& up,
    const glm::vec3& forward)
{
    const float strength = CLAMP(0.f, vr_body_elbow_tuck.value, 1.f);
    if(strength <= 0.f || armLen <= 0.f)
    {
        return 0.f;
    }
    const auto smooth = [](float from, float to, float x) {
        const float t = CLAMP(0.f, (x - from) / (to - from), 1.f);
        return t * t * (3.f - 2.f * t);
    };
    const glm::vec3 r = (wrist - shoulder) / armLen;
    const float fullAt = CLAMP(0.1f, vr_body_elbow_tuck_near.value, 1.5f);
    const float noneAt = za::max(fullAt + 0.05f, vr_body_elbow_tuck_far.value);
    const float folded = 1.f - smooth(fullAt, noneAt, glm::length(r));
    const float ahead = smooth(0.f, 0.15f, glm::dot(r, forward));
    const float below = 1.f - smooth(0.15f, 0.32f, glm::dot(r, up));
    return strength * folded * ahead * below;
}

// The arms' easing from one frame to the next, per side (solveArm: the main thread's frame).
struct ArmEase
{
    float swivel[2]{0.f, 0.f};         // the elbow's swing about the shoulder-wrist line (easing the wrist)
    double swivelTime[2]{-1.0, -1.0};  // when it was set (-1: never)
    float out[2]{0.f, 0.f};            // the elbow's swing out of the torso
    double outTime[2]{-1.0, -1.0};
    float twist[2]{0.f, 0.f};          // the forearm's twist (kept continuous past a half turn)
};
ArmEase armEase;

// Body tracking's fit of the arm (vr_bodytrack_arm_fit): the clavicle turns at most this far (degrees) towards the
// tracked shoulder, and it and each arm bone keep within 1 / vr_bodytrack_arm_fit_stretch to vr_bodytrack_arm_fit_stretch
// of their lengths to reach the tracked shoulder and elbow (ROUND22.md, "Elbow fit on real data": Meta's skeleton is
// its own, wider and longer-armed than ours, and its wrists are not ours: 0.8 to 1.25 left the elbow 7 to 12 cm off).
constexpr float trackedShoulderTurn = 60.f;
[[nodiscard]] float trackedBoneLong()
{
    return CLAMP(1.f, vr_bodytrack_arm_fit_stretch.value, 3.f);
}

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
    glm::mat3 rest = chest.rot * glm::transpose(bd.rot[Chest]) * bd.rot[clav];
    const glm::vec3 lateral = rest[0];
    // The shoulders' own offset from the chest (bodycal::shoulderOffset: measured, or the default body's, and the
    // tweaks): back, up, and outwards along the clavicle, in metres.
    // Crouched (vr_body_crouch_shoulders_*): further back, up and out, and the collarbone swung back and up about the
    // base of the neck, as deep as the crouch (Body::crouchPose).
    const float crouched = b.crouchPose;
    const glm::vec3 so = bodycal::shoulderOffset() + glm::vec3{vr_body_crouch_shoulders_back.value,
                                                         vr_body_crouch_shoulders_up.value, vr_body_crouch_shoulders_out.value} *
                                                         crouched;
    c.pos = childPos(b, clav) + (-cFwd * so.x + cUp * so.y + lateral * so.z) * b.m2w;
    if(crouched != 0.f && (vr_body_crouch_shoulders_swing.value != 0.f || vr_body_crouch_shoulders_shrug.value != 0.f))
    {
        const glm::vec3 swingAxis = safeNormalize(glm::cross(lateral, -cFwd), cUp);
        const glm::vec3 shrugAxis = safeNormalize(glm::cross(lateral, cUp), cFwd);
        rest = glm::mat3_cast(glm::angleAxis(glm::radians(vr_body_crouch_shoulders_shrug.value * crouched), shrugAxis) *
                              glm::angleAxis(glm::radians(vr_body_crouch_shoulders_swing.value * crouched), swingAxis)) *
               rest;
    }

    const ArmLengths lengths = armLengths(b.m2w);
    const float armLen =
        lengths.calibrated ? lengths.upper + lengths.fore : (boneLength(upper, fore) + boneLength(fore, hand)) * b.m2w;
    const glm::vec3 restShoulder = c.pos + rest * (localOffset(upper) * b.m2w);
    c.rot = clavicleTurn(wrist - restShoulder, lateral, cUp, cFwd, armLen, bodycal::shoulderRise(),
                bodycal::shoulderSwing(), lengths.calibrated) *
            rest;

    // Body tracking (vr_bodytrack_arm_fit, by the arm weight): the clavicle turns on, about the base of the neck, to take
    // the shoulder towards the tracked one (a shrug, the shoulder forward or back), by up to trackedShoulderTurn.
    glm::vec3 trackedShoulder{0.f};
    glm::vec3 trackedElbow{0.f};
    float trackedFit = 0.f;
    bodytrack::ArmFit fitReport; // (vr_bodytrack_stats, vr_bodytrack_dump)
    if(easeWrists && bodytrack::trackedArm(side, trackedShoulder, trackedElbow, trackedFit))
    {
        fitReport.weight = trackedFit;
        const glm::vec3 from = safeNormalize(c.rot * localOffset(upper), lateral);
        const glm::vec3 to = trackedShoulder - c.pos;
        const glm::vec3 axis = glm::cross(from, to);
        if(glm::length(to) > 1e-4f && glm::length(axis) > 1e-6f)
        {
            const float angle = glm::acos(CLAMP(-1.f, glm::dot(from, glm::normalize(to)), 1.f));
            fitReport.turnLimited = angle > glm::radians(trackedShoulderTurn);
            const float turn = za::min(angle, glm::radians(trackedShoulderTurn)) * trackedFit;
            c.rot = glm::mat3_cast(glm::angleAxis(turn, glm::normalize(axis))) * c.rot;
        }
        // ... and lengthens or shortens (within the arm bones' shares) to put it there: the tracked shoulders may
        // be wider or narrower than ours.
        const glm::vec3 o = localOffset(upper) * b.m2w;
        const glm::vec3 along = c.rot * glm::vec3{o.x, 0.f, 0.f};
        const glm::vec3 fixedPart = c.pos + c.rot * glm::vec3{0.f, o.y, o.z};
        if(const float l2 = glm::dot(along, along); l2 > 1e-8f)
        {
            const float want = glm::dot(trackedShoulder - fixedPart, along) / l2;
            const float s = CLAMP(1.f / trackedBoneLong(), want, trackedBoneLong());
            fitReport.clavicleClamped = s != want;
            c.stretch = glm::mix(1.f, s, trackedFit);
        }
    }

    // The arm: the elbow points down, somewhat out and back, and away from the thumb (turning the palm up brings the
    // elbow in). It may stretch a little to reach.
    Bone& u = b.bones[upper];
    u = Bone{};
    u.pos = childPos(b, upper);

    // The model's arms (vr_body_arm_length times its proportions) stretch up to vr_body_arm_stretch, then the shoulder
    // reaches out by up to vr_body_shoulder_reach metres. Calibrated arms (Body Calibration) are the player's own
    // length: past it the shoulder reaches first, as a real one does, and only then does the arm stretch. Only past all
    // of it does the hand leave the arm.
    float a = lengths.upper;
    float l = lengths.fore;
    float d = glm::distance(wrist, u.pos);
    const float maxStretch = za::max(1.f, vr_body_arm_stretch.value);
    const auto reachOut = [&] {
        if(const float excess = d - (a + l); excess > 0.f)
        {
            const float reach = za::min(excess, za::max(0.f, vr_body_shoulder_reach.value) * b.m2w);
            u.pos += (wrist - u.pos) / d * reach;
            d -= reach;
        }
    };
    if(lengths.calibrated)
    {
        reachOut();
    }
    const float stretch = CLAMP(1.f, d / (a + l), maxStretch);
    a *= stretch;
    l *= stretch;
    if(!lengths.calibrated)
    {
        reachOut();
    }

    // The hand turns the elbow by its roll about the forearm only (vr_body_elbow_hand): the side of the thumb across
    // the forearm that the rest of the pole gives. Bending the wrist (the hand tilted along the forearm: deviation, or
    // Hand Calibration's pitch) doesn't move the elbow; a real one stays put.
    // (Crouched, vr_body_crouch_elbow_out and _back more, as deep as the crouch.)
    const glm::vec3 bodyPole = -cUp + lateral * (vr_body_elbow_out.value + vr_body_crouch_elbow_out.value * crouched) -
                               cFwd * (vr_body_elbow_back.value + vr_body_crouch_elbow_back.value * crouched);
    glm::vec3 bend;
    const glm::vec3 foreGuess = safeNormalize(wrist - twoBone(u.pos, wrist, a, l, bodyPole, lateral, bend), cFwd);
    const glm::vec3 thumbAcross = safeNormalize(handUp - foreGuess * glm::dot(handUp, foreGuess), handUp);
    glm::vec3 pole = bodyPole - thumbAcross * vr_body_elbow_hand.value;
    float spreadBy = 0.f; // (easeWrist)

    // A folded arm (the hand at the face or the chest) tucks its elbow down and back by the ribs, as a person aiming
    // down the sights or holding an axe up by the face does (vr_body_elbow_tuck: elbowTuck): the pole turns down and
    // back (vr_body_elbow_tuck_back per down, which can take the elbow behind the chest), half as far out as the
    // rest's (the elbow under the shoulder, not out to the side, with the hand up by the face) and not turned by the
    // hand's roll (which would take it in across the chest, or out). The wrist's ease swings it out or across only a
    // little (easeWrist), and by the face the hand palm down spreads it less (below).
    // (The drawn arms only, as the wrist's ease: the body-off forearm that carries the wrist gadget stays as it was.)
    const float tuck = easeWrists ? elbowTuck(u.pos, wrist, a + l, cUp, cFwd) : 0.f;
    if(tuck > 0.f)
    {
        const glm::vec3 tucked =
            -cUp + lateral * (0.5f * vr_body_elbow_out.value) - cFwd * za::max(0.f, vr_body_elbow_tuck_back.value);
        pole = glm::mix(pole, tucked * (glm::length(pole) / za::max(1e-4f, glm::length(tucked))), tuck);
    }

    // A folded arm (the hand at the face or the chest) with the palm to the floor spreads the elbow out to the side,
    // level with the forearm, as a person's does (wings), instead of standing the forearm up in front of the chest with
    // the wrist bent back 70-80 degrees (within its reach, so nothing else moved it): the pole turns out, by
    // vr_body_elbow_spread. Palm up (facing the face) or on its side (a guard), the pole is as before: the elbow tucked
    // in, under the hand. Folded: the wrist within 0.65 of the arm's length of the shoulder, fading out by 0.8 (the hand
    // out in front or at the side swings as before). Palm down: from 0.3 (a little past on its side) to 0.7 (45 degrees
    // from flat).
    if(const float spread = za::max(0.f, vr_body_elbow_spread.value); spread > 0.f && glm::length(handPose.forward) > 0.5f)
    {
        const float folded = CLAMP(0.f, (0.8f - glm::distance(wrist, u.pos) / (a + l)) / 0.15f, 1.f);
        const glm::vec3 palm = side == 0 ? glm::cross(handPose.forward, handUp) : glm::cross(handUp, handPose.forward);
        const float palmDown = CLAMP(0.f, (-glm::dot(safeNormalize(palm), UP) - 0.3f) / 0.4f, 1.f);
        const glm::vec3 wings = (lateral - cUp * 0.35f) * glm::length(pole);
        // By the face (the wrist from 0.15 of the arm's length below the shoulder up) or with the hand pointing up
        // (from 15 degrees over level to 30), a tucked elbow doesn't spread: a palm down there is a gun canted or an axe
        // raised, not the forearm held level before the chest (a look at the wrist), which still spreads.
        const float rise = glm::dot(wrist - u.pos, cUp) / (a + l);
        const float byFace = CLAMP(0.f, (rise + 0.15f) / 0.15f, 1.f);
        const float pointsUp = CLAMP(0.f, (glm::dot(safeNormalize(handPose.forward), cUp) - 0.25f) / 0.25f, 1.f);
        spreadBy = za::min(1.f, folded * palmDown * spread) * (1.f - tuck * za::max(byFace, pointsUp));
        pole = glm::mix(pole, wings, spreadBy);
    }
    glm::vec3 elbow = twoBone(u.pos, wrist, a, l, pole, lateral, bend);

    // The hand bone's axes: the drawn hand's (or, without one, the wrist's roll only).
    const glm::mat3 handRot = glm::length(handPose.forward) > 0.5f
                                  ? basis(handPose.forward, -handUp)
                                  : basis(safeNormalize(wrist - elbow, glm::normalize(elbow - u.pos)), -handUp);

    // Where the hand would bend or turn the wrist past what a wrist does, the elbow swings round (about the line from
    // the shoulder to the wrist) as far as eases it, as a person's does: out when the palm turns down, in when it
    // turns up, up or down when the hand points away from the forearm (easeWrist); it follows the best swing in about
    // a twentieth of a second.
    const glm::vec3 poleElbow = elbow, poleBend = bend; // (vr_debug_arm)
    float(&lastSwivel)[2] = armEase.swivel;
    double(&lastSwivelTime)[2] = armEase.swivelTime;
    float swivel = 0.f;
    costTrace.want = easeWrists && vr_debug_arm.value == 1.f;
    if(const float limits = easeWrists ? vr_body_wrist_limits.value : 0.f; limits > 0.f && glm::length(handPose.forward) > 0.5f)
    {
        const glm::vec3 axis = safeNormalize(wrist - u.pos);
        const float best = easeWrist(u.pos, wrist, elbow, bend, handRot, side, limits, lastSwivel[side], cUp, lateral, cFwd,
            a, za::max(0.f, vr_body_elbow_lift.value), 0.03f * b.m2w, spreadBy, tuck);
        const double now = vr_gametime;
        const double since = lastSwivelTime[side] >= 0.0 ? CLAMP(0.0, now - lastSwivelTime[side], 0.1) : -1.0;
        swivel = since < 0.0 ? best
                             : glm::mix(lastSwivel[side], best, 1.f - za::exp(-static_cast<float>(since) / 0.05f));
        const glm::vec3 centre = u.pos + axis * glm::dot(elbow - u.pos, axis);
        const glm::quat q = glm::angleAxis(swivel, axis);
        elbow = centre + q * (elbow - centre);
        bend = q * bend;
        lastSwivelTime[side] = now;
    }
    lastSwivel[side] = swivel;

    // Body tracking (vr_bodytrack.cpp): the elbow swung round to the tracked elbow's side (the hand stays).
    if(easeWrists)
    {
        bodytrack::swivelElbow(side, u.pos, wrist, elbow, bend);
    }

    // An elbow in the torso (the hand across the chest, the pole down and back) swings out of it about the same line,
    // as little as takes it out (vr_body_collide_elbows: the torso's capsules, vr_selfcollide.cpp), eased as above.
    float(&lastOut)[2] = armEase.out;
    double(&lastOutTime)[2] = armEase.outTime;
    if(easeWrists)
    {
        // (Its joint kept 2 cm out: 3 cm of its flesh's 5.5, less the centimetre an arm at rest lies against the torso
        // by; a relaxed arm hanging at the side is 2.5 cm out.)
        const float want = selfcollide::elbowSwing(u.pos, elbow, wrist, 0.03f * b.m2w);
        const double now = vr_gametime;
        const double since = lastOutTime[side] >= 0.0 ? CLAMP(0.0, now - lastOutTime[side], 0.1) : -1.0;
        const float out = since < 0.0 ? want : glm::mix(lastOut[side], want, 1.f - za::exp(-static_cast<float>(since) / 0.05f));
        lastOut[side] = za::abs(out) < 1e-4f ? 0.f : out;
        lastOutTime[side] = now;
        if(lastOut[side] != 0.f)
        {
            const glm::vec3 axis = safeNormalize(wrist - u.pos);
            const glm::vec3 centre = u.pos + axis * glm::dot(elbow - u.pos, axis);
            const glm::quat q = glm::angleAxis(lastOut[side], axis);
            elbow = centre + q * (elbow - centre);
            bend = q * bend;
        }
    }

    // Body tracking (vr_bodytrack_arm_fit): the elbow onto the tracked elbow itself (the swivel above took it only to its
    // side, on our arm's lengths from our shoulder): the upper arm and forearm take the lengths that put it there (each
    // within trackedBoneLong's shares of its own), the hand still on the controller. Last, over the guesses and
    // the torso's push: the tracked elbow is where the real one is.
    if(trackedFit > 0.f)
    {
        const glm::vec3 target = glm::mix(elbow, trackedElbow, trackedFit);
        const float wantA = glm::distance(target, u.pos), wantL = glm::distance(wrist, target);
        const float longest = trackedBoneLong();
        float fa = CLAMP(a / longest, wantA, a * longest);
        float fl = CLAMP(l / longest, wantL, l * longest);
        fitReport.upperClamped = fa != wantA;
        fitReport.foreClamped = fl != wantL;
        const float reach = za::min(glm::distance(wrist, u.pos), a + l); // (as far as the arm reached before)
        if(fa + fl < reach)
        {
            const float k = reach / (fa + fl);
            fa *= k;
            fl *= k;
        }
        glm::vec3 fitBend;
        elbow = twoBone(u.pos, wrist, fa, fl, target - u.pos, bend, fitBend);
        bend = fitBend;
        fitReport.upperShare = fa / za::max(1e-4f, a);
        fitReport.foreShare = fl / za::max(1e-4f, l);
        fitReport.miss = glm::distance(elbow, trackedElbow);
        a = fa;
        l = fl;
    }
    if(easeWrists)
    {
        bodytrack::armFitted(side, fitReport);
    }

    if(easeWrists && vr_debug_arm.value != 0.f)
    {
        traceArm(b, side, u.pos, poleElbow, poleBend, elbow, bend, wrist, handRot, swivel, tuck, lastOut[side]);
    }

    u.rot = basis(elbow - u.pos, bend);
    u.stretch = a / (boneLength(upper, fore) * b.m2w); // (the drawn upper arm and forearm each their own length)

    // The forearm turns with the hand's roll: none of it at the elbow, all of it at the wrist, spread along it by
    // its twist joints (vr_body_forearm_twist: the share at its middle; 0.5 turns it evenly). The hand's turn from
    // the untwisted forearm is split into its roll about the forearm (the twist) and the swing that bends the wrist
    // (whatever the twist, the least turn from the forearm to the hand). The bind pose has the palms facing the
    // thighs, thumbs forward: the bones' hint axis is the little finger's side, opposite the top of the (gripping)
    // hand, where the thumb is.
    const glm::vec3 foreDir = safeNormalize(wrist - elbow, glm::normalize(elbow - u.pos));
    const glm::mat3 untwisted = basis(foreDir, bend);
    const glm::quat turn = glm::normalize(glm::quat_cast(handRot * glm::transpose(untwisted)));
    float twist = 2.f * za::atan2(glm::dot(glm::vec3{turn.x, turn.y, turn.z}, foreDir), turn.w);
    // Kept continuous past a half turn (the nearest to the last frame's, up to 1.25 turns either way): a hand
    // held upside down would otherwise flip the forearm's twist from one side to the other as it shakes.
    float(&lastTwist)[2] = armEase.twist;
    twist -= glm::two_pi<float>() * za::round((twist - lastTwist[side]) / glm::two_pi<float>());
    if(za::abs(twist) > glm::radians(225.f))
    {
        twist -= glm::two_pi<float>() * za::round(twist / glm::two_pi<float>());
    }
    lastTwist[side] = twist;
    const glm::quat roll = glm::angleAxis(twist, foreDir);
    const glm::quat wristSwing = glm::normalize(turn * glm::conjugate(roll));

    Bone& f = b.bones[fore];
    f = Bone{};
    f.pos = elbow;
    f.rot = untwisted;
    f.stretch = l / (boneLength(fore, hand) * b.m2w);

    const float share = CLAMP(0.f, vr_body_forearm_twist.value, 1.f);
    for(int k = 0; k < twistJoints; k++)
    {
        const float at = twistPlaces[k];
        const float s = at <= 0.5f ? share * at / 0.5f : share + (1.f - share) * (at - 0.5f) / 0.5f;
        Bone& t = b.bones[foreHelpers(side) + k];
        t = Bone{};
        t.pos = elbow + foreDir * (l * at);
        t.rot = glm::mat3_cast(glm::angleAxis(twist * s, foreDir)) * untwisted;
        t.stretch = f.stretch;
    }

    // The wrist joint: all of the roll and half of the bend (the bisector of the forearm and the hand), stretched
    // across the bend by 1 / cos(half the bend), as a mitre joint's section is, so that the bent wrist keeps the
    // thickness of the arm on both sides of it.
    const float bendAngle = 2.f * za::acos(CLAMP(-1.f, za::abs(wristSwing.w), 1.f));
    const glm::quat halfWristSwing =
        glm::slerp(glm::quat{1.f, 0.f, 0.f, 0.f}, wristSwing.w < 0.f ? -wristSwing : wristSwing, 0.5f);
    Bone& w = b.bones[foreHelpers(side) + twistJoints];
    w = Bone{};
    w.pos = elbow + foreDir * l;
    w.rot = glm::mat3_cast(halfWristSwing * roll) * untwisted;
    w.stretch = f.stretch; // the forearm's side of the wrist's rings
    if(bendAngle > 1e-3f)
    {
        const glm::vec3 across = glm::transpose(w.rot) * (handRot[0] - foreDir); // in the wrist joint's axes
        const glm::vec3 n = safeNormalize(across - glm::vec3{across.x, 0.f, 0.f}, glm::vec3{0.f, 0.f, 1.f});
        const float mitre = 1.f / za::cos(za::min(bendAngle, glm::radians(120.f)) * 0.5f);
        w.shape = glm::mat3{1.f} + (mitre - 1.f) * glm::outerProduct(n, n);
    }

    // The hand bone turns with the hand itself, the whole bend: the bracer's lip over the base of the hand goes
    // with it, so that the hand never comes off the arm.
    Bone& h = b.bones[hand];
    h = Bone{};
    h.pos = elbow + foreDir * l;
    h.rot = handRot;

    // Bare forearms: their ends, fitted round the hand at the default sizes, as big as the hand is drawn against the
    // body (the hand bone's axes are the hand's: x its x, y its y either way, z its -z); the wrist's ring across.
    if(bareArms && handPose.unit.x > 0.f)
    {
        const glm::vec3 grow = glm::clamp(handPose.unit / (b.m2w * handUnit), glm::vec3{0.5f}, glm::vec3{2.f});
        h.shape = glm::mat3{glm::vec3{grow.x, 0.f, 0.f}, glm::vec3{0.f, grow.y, 0.f}, glm::vec3{0.f, 0.f, grow.z}};
        w.shape = w.shape * glm::mat3{glm::vec3{1.f, 0.f, 0.f}, glm::vec3{0.f, grow.y, 0.f}, glm::vec3{0.f, 0.f, grow.z}};
    }
}

// Model bone index of each joint, per model checked (a few: the builds' bodies, their fallback): a table, so that a
// missing build's fallback isn't checked again every frame.
struct ModelInfo
{
    const qmodel_t* model{nullptr};
    za::String name; // the model's (its slot is reused by another after a game change)
    bool usable{false};
    bool bare{false}; // bare forearms (progs/vrbody*_bare)
    za::Array<int, JointCount> boneOf{};
};

za::Vector<ModelInfo> infos;

[[nodiscard]] const ModelInfo* infoOf(const qmodel_t* model)
{
    for(const ModelInfo& i : infos)
    {
        if(i.model == model && (!model || i.name == model->name))
        {
            return &i;
        }
    }
    return nullptr;
}

// The pose being drawn: skinning matrices in model bone order (the model has JointCount bones).
struct Posed
{
    const entity_t* ent{nullptr};
    float scale{0.f};
    za::Array<float, JointCount * 12> skin{};
    za::Array<float, JointCount * 12> shadowSkin{}; // skin with the neck and head drawn (the shadow maps': shadowLight)
    glm::vec3 eyes{0.f};                            // the head this frame (world)
    float m2w{0.f};                                 // world units per metre
    bool shadowHead{false};                         // the light being drawn casts the head's shadow (shadowLight)
    glm::vec3 wrist[2]{glm::vec3{0.f}, glm::vec3{0.f}};   // per hand
    glm::vec3 forearm[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    Shoulder shoulders[2];                                // per side
    bool legs{false};
    glm::vec3 thighJoint[2]{glm::vec3{0.f}, glm::vec3{0.f}}; // per side, in the pelvis's frame
    glm::mat3 thighNow[2]{glm::mat3{1.f}, glm::mat3{1.f}};
    glm::mat3 thighStill[2]{glm::mat3{1.f}, glm::mat3{1.f}};
};

Posed posed;
Skeleton posedSkeleton; // (skeleton(): valid while posed.ent is set)

// The forearms' frames as last posed or solved (forearmFrame), per hand: the twist, about the forearm's axis from
// its untwisted axes, at the elbow (none), at each twist joint and at the wrist (the hand's whole roll).
struct ForearmTwist
{
    bool valid{false};
    glm::vec3 elbow{0.f};
    glm::mat3 untwisted{1.f}; // the forearm bone's axes
    glm::mat3 hand{1.f};      // the hand bone's
    float length{0.f};
    float places[twistJoints + 2]{};
    float angles[twistJoints + 2]{}; // radians
};
ForearmTwist forearms[2];

// The turn about `axis` (radians, -pi .. pi) that takes `from` to `to`, less any swing off it.
[[nodiscard]] float twistAbout(const glm::mat3& from, const glm::mat3& to, const glm::vec3& axis)
{
    const glm::quat q = glm::normalize(glm::quat_cast(to * glm::transpose(from)));
    float t = 2.f * za::atan2(glm::dot(glm::vec3{q.x, q.y, q.z}, axis), q.w);
    return t - glm::two_pi<float>() * za::round(t / glm::two_pi<float>());
}

// The arms' forearm frames from a solved body (its twist joints; neighbours are well under a half turn apart, so the
// angles add up past it).
void keepForearms(const Body& b)
{
    for(int side = 0; side < 2; side++)
    {
        const int clav = side == 0 ? ClavicleL : ClavicleR;
        const Bone& fore = b.bones[clav + 2];
        const Bone& hand = b.bones[clav + 3];
        ForearmTwist& f = forearms[side == 0 ? HAND_OFF : HAND_MAIN]; // (the left controller, left-handed too)
        f.valid = true;
        f.elbow = fore.pos;
        f.untwisted = fore.rot;
        f.hand = hand.rot;
        f.length = glm::distance(fore.pos, hand.pos);
        const glm::vec3 axis = fore.rot[0];
        glm::mat3 last = fore.rot;
        f.places[0] = 0.f;
        f.angles[0] = 0.f;
        for(int k = 1; k <= twistJoints + 1; k++)
        {
            const glm::mat3& r = k <= twistJoints ? b.bones[foreHelpers(side) + k - 1].rot : hand.rot;
            f.places[k] = k <= twistJoints ? twistPlaces[k - 1] : 1.f;
            f.angles[k] = f.angles[k - 1] + twistAbout(last, r, axis);
            last = glm::mat3_cast(glm::angleAxis(f.angles[k], axis)) * f.untwisted;
        }
    }
}
int debugFrame = -1;

// vr_debug_lean: one line a frame into lean_trace.txt (the game directory): the time; the head (x y z, world units) and
// its height (metres); the box's middle (x y); the lean (x y); the pelvis (x y z); each foot (left, right: x y, and its
// lift in metres); whether each is stepping; the walk's amount; and the lean's hold and cues (hands::State).
void traceLean(const Body& b, const hands::State& s)
{
    FILE*& file = traces.lean;
    if(!vr_debug_lean.value)
    {
        if(file)
        {
            fclose(file);
            file = nullptr;
        }
        return;
    }
    if(!file && !(file = fopen(va("%s/lean_trace.txt", com_gamedir), "w")))
    {
        return;
    }
    const glm::vec3& p = b.bones[Pelvis].pos;
    const Foot& l = stance.feet[0];
    const Foot& r = stance.feet[1];
    fprintf(file,
        "%.4f %.3f %.3f %.3f %.4f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %d %d %.3f %.3f %.4f "
        "%.4f %.4f %.4f\n",
        vr_gametime, s.head.x, s.head.y, s.head.z, s.headHeight, s.playerOrigin.x, s.playerOrigin.y, s.lean.x, s.lean.y, p.x,
        p.y, p.z, l.pos.x, l.pos.y, l.lift, r.pos.x, r.pos.y, r.lift, l.step >= 0.f, r.step >= 0.f, gait.amount,
        s.leanHold, s.leanCues.x, s.leanCues.y, s.leanCues.z, s.leanCues.w);
    fflush(file);
}

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

void resetMotion()
{
    gait = Gait{};
    stance = Stance{};
    water = Water{};
    climbing = Climbing{};
    climbHang = 0.f;
    armEase = ArmEase{};
    for(ForearmTwist& f : forearms)
    {
        f = ForearmTwist{};
    }
}

Torso torso(const hands::State& s, bool crouchPose)
{
    Body b;
    solveTorso(s, b, 0.f, crouchPose);
    return {{b.bones[Pelvis].pos, b.bones[Pelvis].rot}, {b.bones[Chest].pos, b.bones[Chest].rot}, b.crouchPose};
}

float crouchPoseWeight(float depth)
{
    if(vr_body_crouch_preview.value >= 0.f)
    {
        depth = vr_body_crouch_preview.value;
    }
    depth = CLAMP(0.f, depth, 1.f);
    const float strength = za::max(0.f, vr_body_crouch_pose_strength.value);
    if(strength <= 0.f || depth <= 0.f)
    {
        return 0.f;
    }
    return strength * glm::pow(depth, CLAMP(0.1f, vr_body_crouch_pose_curve.value, 10.f));
}

float crouchPoseShare(const hands::State& s)
{
    return torso(s).crouchPose;
}

hands::State standing(const hands::State& s)
{
    hands::State out = s;
    out.head.z += (units::eyeHeight() - s.headHeight) * units::metresToUnits();
    out.headHeight = units::eyeHeight();
    out.headAngles = {0.f, s.bodyYaw, 0.f};
    out.crouchRatio = 0.f;
    // Standing where the head is, upright over it: the box's middle moved under the head, no lean.
    out.playerOrigin += glm::vec3{s.lean.x, s.lean.y, 0.f};
    out.lean = glm::vec3{0.f};
    out.standingHeight = out.headHeight;
    return out;
}

void armBones(float& upper, float& fore)
{
    upper = boneLength(UpperArmL, ForearmL);
    fore = boneLength(ForearmL, HandL);
}

Frame uprightChest(const hands::State& s, float yaw, float eyeHeight)
{
    hands::State u = s;
    const float rise = (eyeHeight - s.headHeight) * units::metresToUnits();
    u.head.z += rise;
    u.headHeight = eyeHeight;
    u.standingHeight = eyeHeight;
    u.bodyYaw = yaw;
    u.crouchRatio = 0.f;
    u.playerOrigin += glm::vec3{s.lean.x, s.lean.y, 0.f};
    u.lean = glm::vec3{0.f};
    Body b;
    solveTorso(u, b, eyeHeight / units::modelEyeHeight, false);
    return {b.bones[Chest].pos - glm::vec3{0.f, 0.f, rise}, b.bones[Chest].rot};
}

glm::vec3 shoulderInChest(int side, const glm::vec3& wrist, const ShoulderModel& m)
{
    const Bind& bd = bind();
    const int clav = side == 0 ? ClavicleL : ClavicleR;
    // The chest bone's axes (up the spine, right, forward) to forward, left, up.
    const auto flu = [](const glm::vec3& v) { return glm::vec3{v.z, -v.y, v.x}; };
    const glm::mat3 restLocal = glm::transpose(bd.rot[Chest]) * bd.rot[clav];
    const glm::vec3 lateral = flu(restLocal[0]);
    const glm::vec3 up{0.f, 0.f, 1.f}, fwd{1.f, 0.f, 0.f};
    const glm::vec3 offset = -fwd * m.offset.x + up * m.offset.y + lateral * m.offset.z;
    const glm::vec3 pivot = (flu(localOffset(clav)) + offset) * m.scale;
    const glm::vec3 toShoulder = flu(restLocal * localOffset(clav + 1)) * m.scale;
    const glm::mat3 turn = clavicleTurn(wrist - (pivot + toShoulder), lateral, up, fwd, m.armLength, m.upDegrees,
        m.forwardDegrees, m.calibrated);
    return pivot + turn * toShoulder;
}

Follower::Follower(const hands::State& s) : now(torso(s)), ref(torso(standing(s), false))
{
}

glm::vec3 Follower::operator()(Part part, const glm::vec3& standingPoint) const
{
    const Frame& f = part == Part::Pelvis ? now.pelvis : now.chest;
    const Frame& r = part == Part::Pelvis ? ref.pelvis : ref.chest;
    return f.pos + f.rot * (glm::transpose(r.rot) * (standingPoint - r.pos));
}

bool Follower::thigh(int side, ThighMotion& out) const
{
    if(!posed.ent || !posed.legs || side < 0 || side > 1)
    {
        return false;
    }
    const Frame& p = now.pelvis;
    out.joint = p.pos + p.rot * posed.thighJoint[side];
    const glm::mat3 still = p.rot * posed.thighStill[side];
    out.down = still[0];
    out.turn = p.rot * posed.thighNow[side] * glm::transpose(posed.thighStill[side]) * glm::transpose(p.rot);
    return true;
}

void reset()
{
    infos.clear();
}

bool usable(qmodel_t* model)
{
    if(const ModelInfo* known = infoOf(model))
    {
        return known->usable;
    }
    if(infos.size() >= 16)
    {
        infos.clear(); // not expected: a handful of models at most
    }
    infos.emplaceBack();
    ModelInfo& info = infos.back();
    info.model = model;
    info.name = model ? model->name : "";
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
        const int found = modelmeta::boneIndex(model, jointNames[j], true);
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
    info.bare = strstr(model->name, "_bare.") != nullptr; // (progs/vrbody*_bare.mdl)
    return true;
}

glm::vec3 pose(const hands::State& s, qmodel_t* model, const entity_t* ent, const HandPose handPoses[2], bool legs)
{
    QVR_PROFILE("avatar IK");
    Body b;
    solveTorso(s, b);
    debugHead = s.head;
    const ModelInfo* known = infoOf(model);
    bareArms = known && known->bare;

    // The left arm reaches for the left controller (HAND_OFF, left-handed too), the right for the right.
    solveArm(b, 0, handPoses[HAND_OFF]);
    solveArm(b, 1, handPoses[HAND_MAIN]);
    const float dt = legsDeltaTime();
    if(legs)
    {
        updateWater(b, s, dt);
    }
    updateGait(b, dt);
    const glm::vec3 stand = s.head - glm::vec3{s.lean.x, s.lean.y, 0.f}; // where the body stands (the box's middle)
    updateStance(b, stand, dt);
    {
        const double t0 = vr_debug_climb_feet.value >= 2.f ? Sys_DoubleTime() : 0.0;
        updateClimbing(b, dt, legs);
        climbing.updateMs = vr_debug_climb_feet.value >= 2.f ? (Sys_DoubleTime() - t0) * 1000.0 : 0.0;
    }
    glm::mat3 still[2];
    solveLeg(b, stand, 0, legs, still[0]);
    solveLeg(b, stand, 1, legs, still[1]);
    logClimbing(b);
    traceLean(b, s);

    // The thighs relative to the pelvis, as they are and standing still (Follower::thigh).
    posed.legs = legs;
    {
        const Bone& p = b.bones[Pelvis];
        const glm::mat3 toPelvis = glm::transpose(p.rot);
        for(int side = 0; side < 2; side++)
        {
            const Bone& t = b.bones[side == 0 ? ThighL : ThighR];
            posed.thighJoint[side] = toPelvis * (t.pos - p.pos);
            posed.thighNow[side] = toPelvis * t.rot;
            posed.thighStill[side] = toPelvis * still[side];
        }
    }

    // The joints where the body is (before the preview moves it), for the collision proxies (skeleton()).
    {
        Skeleton& k = posedSkeleton;
        k.m2w = b.m2w;
        k.legs = legs;
        for(int side = 0; side < 2; side++)
        {
            const int clav = side == 0 ? ClavicleL : ClavicleR;
            const int hand = side == 0 ? HAND_OFF : HAND_MAIN; // (the left controller, left-handed too)
            k.shoulder[hand] = b.bones[clav + 1].pos;
            k.elbow[hand] = b.bones[clav + 2].pos;
            k.wrist[hand] = b.bones[clav + 3].pos;
            k.hip[side] = b.bones[side == 0 ? ThighL : ThighR].pos;
            k.knee[side] = b.bones[side == 0 ? CalfL : CalfR].pos;
            const Bone& foot = b.bones[side == 0 ? FootL : FootR];
            k.ankle[side] = foot.pos;
            k.toe[side] = foot.pos + foot.rot[0] * (toeLength(side) * b.m2w);
            k.sole[side] = ankleHeight(side) * b.m2w;
            k.steps[side] = footSteps[side];
        }
        // Body tracking's debug skeleton and error log (vr_bodytrack.cpp).
        bodytrack::Posed bt;
        bt.pelvis = b.bones[Pelvis].pos;
        bt.top = b.bones[Head].pos;
        bt.neutral = safeNormalize(UP * (glm::distance(bind().pos[Pelvis], bind().pos[Head]) * b.m2w) +
                                   b.fwd * (vr_body_torso_back.value * b.m2w));
        bt.fwd = b.fwd;
        for(int side = 0; side < 2; side++)
        {
            const int hand = side == 0 ? HAND_OFF : HAND_MAIN;
            bt.shoulder[side] = k.shoulder[hand];
            bt.elbow[side] = k.elbow[hand];
            bt.wrist[side] = k.wrist[hand];
            bt.hip[side] = k.hip[side];
            bt.knee[side] = k.knee[side];
            bt.ankle[side] = k.ankle[side];
        }
        bt.leanHold = s.leanHold;
        bodytrack::posed(bt);
    }

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
    const ModelInfo* modelInfo = infoOf(model);
    if(!modelInfo || !modelInfo->usable)
    {
        return origin; // not checked (usable() first) or not usable
    }
    const za::Array<int, JointCount>& boneOf = modelInfo->boneOf;

    const auto skinBone = [&](const Bone& bone, float size, const float* inv, float* out) {
        const glm::mat3 r = bone.rot * bone.shape * glm::mat3{glm::vec3{bone.stretch * size, 0.f, 0.f},
                                           glm::vec3{0.f, size, 0.f}, glm::vec3{0.f, 0.f, size}};
        const glm::vec3 t = (bone.pos - origin) / k;
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
    };
    for(int j = 0; j < JointCount; j++)
    {
        const Bone& bone = b.bones[j];
        const int i = boneOf[j];
        const float* inv = bones[i].inverse.mat;
        skinBone(bone, bone.size, inv, &posed.skin[i * 12]);
        // The shadow maps': the neck and head as the model has them (the eye views collapse them, solveTorso).
        float* shadow = &posed.shadowSkin[i * 12];
        if(j == Neck || j == Head)
        {
            skinBone(bone, 1.f, inv, shadow);
        }
        else
        {
            memcpy(shadow, &posed.skin[i * 12], 12 * sizeof(float));
        }
    }
    posed.eyes = s.head;
    posed.m2w = b.m2w;

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

        const int hand = side == 0 ? HAND_OFF : HAND_MAIN; // (the left controller, left-handed too)
        posed.wrist[hand] = b.bones[clav + 3].pos;
        posed.forearm[hand] = b.bones[clav + 2].rot[0];
    }
    keepForearms(b);
    return origin;
}

bool skeleton(Skeleton& out)
{
    if(!posed.ent)
    {
        return false;
    }
    out = posedSkeleton;
    return true;
}

void hide()
{
    posed.ent = nullptr;
    forearms[0].valid = forearms[1].valid = false;
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

bool forearmFrame(int hand, float along, ForearmFrame& out)
{
    if(hand < 0 || hand > 1 || !forearms[hand].valid)
    {
        return false;
    }
    const ForearmTwist& f = forearms[hand];
    along = CLAMP(0.f, along, 1.f);
    int k = 1;
    while(k < twistJoints + 1 && f.places[k] < along)
    {
        k++;
    }
    const float t = (along - f.places[k - 1]) / za::max(f.places[k] - f.places[k - 1], 1e-4f);
    const float angle = glm::mix(f.angles[k - 1], f.angles[k], t);
    const glm::vec3 axis = f.untwisted[0];
    out.point = f.elbow + axis * (f.length * along);
    out.axes = glm::mat3_cast(glm::angleAxis(angle, axis)) * f.untwisted;
    out.hand = f.hand;
    out.length = f.length;
    return true;
}

namespace
{
// make_vrbody.py's BUILDS (vrbody_lean, vrbody, vrbody_brawny, vrbody_fem: FEMININE).
constexpr BuildShape buildShapes[bodyBuilds] = {
    {0.9f, 0.965f, 1.f, 1.f, 0.88f, 0.9f, 0.88f, "progs/vrbody_lean.mdl", "progs/vrbody_lean_bare.mdl"},
    {1.2f, 1.07f, 1.f, 1.f, 1.f, 1.f, 1.f, "progs/vrbody.mdl", "progs/vrbody_bare.mdl"},
    {1.5f, 1.175f, 1.f, 1.f, 1.2f, 1.1f, 1.18f, "progs/vrbody_brawny.mdl", "progs/vrbody_brawny_bare.mdl"},
    {0.9f, 1.f, 0.816f, 0.5f, 0.88f, 1.f, 0.8f, "progs/vrbody_fem.mdl", "progs/vrbody_fem_bare.mdl"},
};
} // namespace

int bodyBuild()
{
    return CLAMP(0, static_cast<int>(vr_body_build.value), bodyBuilds - 1);
}

const BuildShape& buildShape(int build)
{
    return buildShapes[CLAMP(0, build, bodyBuilds - 1)];
}

void forearmGirth(int build, float along, float& hint, float& other, bool bare)
{
    // make_vrbody.py: BUILDS' muscularity m (0.9, 1.2, 1.5, 0.9), forearms fm = 1.08 m, wrists wm = 1 + 0.3 (m - 1); the
    // bracer's rings (loft "bracer") at 0.13, 0.19 and 0.225 m from the elbow and at the wrist (0.26).
    const float m = buildShape(build).muscle;
    const float fm = m * 1.08f;
    const float wm = 1.f + (m - 1.f) * 0.3f;
    struct Ring
    {
        float at, u, v;
    };
    Ring rings[6] = {{0.13f, 0.049f * fm, 0.043f * fm},
        {0.19f, za::max(0.041f * wm, 0.040f), za::max(0.033f * wm, 0.0335f)},
        {0.225f, za::max(0.0385f * wm, 0.039f), za::max(0.030f * wm, 0.0325f)},
        {0.26f, za::max(0.036f * wm, 0.037f), za::max(0.027f * wm, 0.0295f)}};
    int count = 4;
    if(bare)
    {
        // make_vrbody.py's bare_rings: the skin's ring at 0.15 m, then smoothly down to the hand's wrist (its fitted
        // ring's extents, the back's and the palm's sides averaged), the jointed hand at the default size.
        constexpr float handU = bodybare::wristU, handV = bodybare::wristV;
        const float u15 = glm::mix(0.046f * fm, 0.037f * wm, 3.f / 7.f);
        const float v15 = glm::mix(0.040f * fm, 0.029f * wm, 3.f / 7.f);
        const float places[] = {0.15f, 0.19f, 0.225f, 0.26f};
        // The elbow lug can lie before the bare taper begins. Fit that thicker skin too, rather than
        // clamping both bands to the smaller 0.15 m ring and burying the elbow band inside the arm.
        rings[0] = {0.05f, 0.052f * fm, 0.050f * fm};
        rings[1] = {0.12f, 0.046f * fm, 0.040f * fm};
        count = 6;
        for(int k = 0; k < 4; k++)
        {
            const float t = (places[k] - 0.15f) / 0.11f;
            const float ease = 1.f - t * t * (3.f - 2.f * t);
            rings[k + 2] = {places[k], handU + (u15 - handU) * ease, handV + (v15 - handV) * ease};
        }
    }
    const float at = CLAMP(rings[0].at, along * 0.26f, rings[count - 1].at);
    int k = 1;
    while(k < count - 1 && rings[k].at < at)
    {
        k++;
    }
    const float t = (at - rings[k - 1].at) / (rings[k].at - rings[k - 1].at);
    hint = glm::mix(rings[k - 1].u, rings[k].u, t);
    other = glm::mix(rings[k - 1].v, rings[k].v, t);
}

void solveArms(const hands::State& s, const HandPose handPoses[2])
{
    QVR_PROFILE("avatar arms");
    Body b;
    solveTorso(s, b);
    easeWrists = false;
    bareArms = false; // (no body drawn: its frames only)
    solveArm(b, 0, handPoses[HAND_OFF]);
    solveArm(b, 1, handPoses[HAND_MAIN]);
    easeWrists = true;
    keepForearms(b);
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
    if(const entity_t* source = view::cloneSource(e))
    {
        e = source; // (the body's mirror image: vr_bodyclone.cpp)
    }
    return e && e == posed.ent ? posed.scale : 0.f;
}

} // namespace qvr::avatar

namespace qvr::avatar
{
void shadowLight(const glm::vec3& light)
{
    // Out to a light 30 cm from the eyes: the head (10 cm round them) and the head-mounted flashlight (9 cm out).
    posed.shadowHead = vr_shadow_head.value != 0.f && glm::distance(light, posed.eyes) > 0.3f * posed.m2w;
}

float personStep(float speed)
{
    constexpr float base = 0.55f;     // metres
    constexpr float perSpeed = 0.19f; // metres per metre a second
    return CLAMP(0.6f, base + perSpeed * speed, 2.2f);
}
} // namespace qvr::avatar

// The shadow maps' (r_alias.c, R_DrawAliasModelsDepth): the body with its head (shadowLight).
extern "C" int VR_AliasShadowBonePoses(const entity_t* e, const float** matrices)
{
    using namespace qvr::avatar;
    const int count = VR_AliasBonePoses(e, matrices);
    if(count && matrices && e && e == posed.ent && posed.shadowHead)
    {
        *matrices = posed.shadowSkin.data();
    }
    return count;
}

extern "C" int VR_AliasBonePoses(const entity_t* e, const float** matrices)
{
    using namespace qvr::avatar;
    if(const int armour = qvr::agarmor::bonePoses(e, matrices))
    {
        return armour; // an armed humanoid's armour: its body's bones as drawn (vr_agarmor.cpp)
    }
    if(const int hand = qvr::view::handBonePoses(e, matrices))
    {
        return hand; // the jointed hands (vr_handrig.cpp)
    }
    if(const int ragdollBones = qvr::ragdoll::bonePoses(e, matrices))
    {
        return ragdollBones; // a ragdoll (vr_ragdoll.cpp)
    }
    if(const int rig2Bones = qvr::rig2::bonePoses(e, matrices))
    {
        return rig2Bones; // the Rig Studio's subject on the authored rig (vr_rig2.cpp)
    }
    if(const entity_t* source = qvr::view::cloneSource(e))
    {
        // The body's mirror image (vr_bodyclone.cpp): its source's bones, the body's with its head (it is seen from
        // outside).
        const int count = VR_AliasBonePoses(source, matrices);
        if(count && matrices && source == posed.ent)
        {
            *matrices = posed.shadowSkin.data();
        }
        return count;
    }
    if(!e || e != posed.ent)
    {
        return 0;
    }
    if(matrices)
    {
        // Seen through a teleporter (the loop's gates show you your own back), the body has its head: the eyes' views
        // collapse it (solveTorso), so as not to see its inside. Not when the view through the gate is drawn from
        // within a head's length of the eyes (an eye half through a gate looking back through the gate it is in). So
        // too any other view from outside the head (vr_mock_camera's spectator view, a third-person camera).
        const bool away = glm::distance(qvr::geom::vec(r_refdef.vieworg),
                              posed.eyes) > 0.3f * posed.m2w;
        const bool head = away && (VR_PortalDrawing() ? qvr::vr_teleporter_self_head.value != 0.f : true);
        *matrices = head ? posed.shadowSkin.data() : posed.skin.data();
    }
    return JointCount;
}
