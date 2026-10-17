// vr_bodytrack.hpp -- body tracking (XR_FB_body_tracking with XR_META_body_tracking_full_body / _fidelity /
// _calibration: Quest 3's inside-out upper body and generated legs, through Virtual Desktop's VDXR), blended into the
// avatar's guessed pose (ROUND22.md, "Body tracking").
//
// A source (the OpenXR backend, or the mock's synthetic body: vr_bodytrack_synth) submits the joints in the tracking
// space each frame; hands.cpp places them in the world with the head and the hands (place()); the avatar, the torso
// direction and the lean detection blend towards them by region (vr_bodytrack_torso, _arms, _legs), each region's
// weight eased in and out (no pops) as the tracking becomes good enough (active, confident, its joints valid) or stops.
// The controllers always own the hands: the arms take only the elbows' swivel about the shoulder-to-wrist line.

#pragma once

#include <glm/glm.hpp>

namespace qvr
{
struct TrackingState;
}

namespace qvr::bodytrack
{

// XR_FULL_BODY_JOINT_*_META (openxr.h): the ones used here.
enum Joint : int
{
    Hips = 1,
    SpineLower = 2,
    SpineMiddle = 3,
    SpineUpper = 4,
    Chest = 5,
    Neck = 6,
    Head = 7,
    ArmUpperL = 10, // the shoulder (the upper arm's root)
    ArmLowerL = 11, // the elbow
    WristL = 19,
    ArmUpperR = 15,
    ArmLowerR = 16,
    WristR = 45,
    UpperLegL = 70, // the hip joint
    LowerLegL = 71, // the knee
    AnkleL = 73,
    BallL = 76,
    UpperLegR = 77,
    LowerLegR = 78,
    AnkleR = 80,
    BallR = 83,
    JointCount = 84,
};

// Per joint: XrSpaceLocationFlags' position and orientation bits.
inline constexpr unsigned POS_VALID = 1u, POS_TRACKED = 2u, ROT_VALID = 4u, ROT_TRACKED = 8u;

// One frame from a source, in the tracking space (metres; +x right, +y up, -z forward).
struct Raw
{
    bool active{false};      // XrBodyJointLocationsFB::isActive
    float confidence{0.f};   // 0 .. 1
    int fidelity{0};         // 0 unknown, 1 low, 2 high (XrBodyTrackingFidelityStatusMETA)
    int calibration{0};      // 0 unknown, 1 valid, 2 calibrating, 3 invalid (XrBodyTrackingCalibrationStatusMETA)
    glm::vec3 pos[JointCount]{};
    unsigned flags[JointCount]{};
};

// What the backend found and made (vr_bodytrack_status).
struct BackendStatus
{
    bool fbExtension{false};          // XR_FB_body_tracking enabled
    bool fullBodyExtension{false};    // XR_META_body_tracking_full_body
    bool fidelityExtension{false};    // XR_META_body_tracking_fidelity
    bool calibrationExtension{false}; // XR_META_body_tracking_calibration
    bool systemSupports{false};       // XrSystemBodyTrackingPropertiesFB::supportsBodyTracking
    bool trackerCreated{false};
    bool fullBodySet{false};          // the tracker made with XR_BODY_JOINT_SET_FULL_BODY_META
    bool highFidelityRequested{false};
    int locateFailures{0};
    char error[96]{};                 // the last failure (empty: none)
};
BackendStatus& backendStatus();

// A source's frame (once a frame, before hands.cpp's update).
void submit(const Raw& raw);

// The mock (vr_backend_mock.cpp, after its head and hands are set): with vr_bodytrack_synth, a synthetic body (leans,
// twist, crouch, neck bend, elbows) that also moves the head as that body carries it, submitted as a source would.
void synthesize(TrackingState& t);

// hands.cpp, once a frame in VR after the head is placed: the joints into the world as the hands are (`base` the world
// point under the head's tracking-space floor point `floorBelowHead`, `turnYaw` the play space's turn, `m2u` units per
// metre), smoothed (vr_bodytrack_smooth), and the regions' weights eased.
void place(const glm::vec3& base, const glm::vec3& floorBelowHead, float turnYaw, float m2u);

// The regions' blend weights now (0: today's guesses alone, bit-identical).
[[nodiscard]] float torsoWeight();
[[nodiscard]] float armWeight(int side); // 0 left, 1 right
[[nodiscard]] float legWeight(int side);

// The torso: the spine's tilt (hips to chest) from the vertical, `dir` its horizontal direction in the world (unit;
// zero when upright), `tilt` radians; the chest's twist from the hips (degrees, left positive); the hips' yaw.
struct Trunk
{
    float weight{0.f};
    glm::vec3 dir{0.f};
    float tilt{0.f};
    float twist{0.f};
    float hipsYaw{0.f};
};
[[nodiscard]] Trunk trunk();

// The torso direction (hands::State::bodyYaw) with the tracked hips' yaw blended in (torso weight).
[[nodiscard]] float bodyYaw(float estimated);

// How sure the head's offset from the box (`offMetres`, world axes, metres) is a lean, with the tracked spine's tilt
// blended in: a lean tilts the spine as far as the head went; a step (or a neck bend) does not.
[[nodiscard]] float leanSure(float estimated, const glm::vec2& offMetres);

// solveArm: the elbow swung about the shoulder-to-wrist line towards the tracked elbow's side (arm weight).
void swivelElbow(int side, const glm::vec3& shoulder, const glm::vec3& wrist, glm::vec3& elbow, glm::vec3& bend);

// solveArm: the tracked shoulder (the upper arm's joint) and elbow (world) and the weight the arm takes them by beyond
// the swivel (the arm weight times vr_bodytrack_arm_fit): the clavicle turns the shoulder towards the tracked one, the
// upper arm and forearm take the lengths that put the elbow on the tracked one. False (weight 0) when not.
bool trackedArm(int side, glm::vec3& shoulder, glm::vec3& elbow, float& weight);

// solveArm's fit of the drawn arm, as it went (vr_bodytrack_stats, vr_bodytrack_dump): the weight it took, whether the
// shoulder's turn met its limit, the clavicle's or a bone's length its clamp, and the elbow's miss (world units).
struct ArmFit
{
    float weight{0.f};
    bool turnLimited{false}, clavicleClamped{false}, upperClamped{false}, foreClamped{false};
    float upperShare{1.f}, foreShare{1.f}; // the bones' lengths taken, as shares of their own
    float miss{0.f};
};
void armFitted(int side, const ArmFit& fit);

// solveLeg: the foot's target and the knee's direction towards the tracked leg's, scaled to the avatar's leg (`hip`, its
// `reach`), by the leg weight times `keep` (1 less what climbing and swimming take).
void leg(int side, const glm::vec3& hip, float reach, float floorZ, float keep, glm::vec3& target, glm::vec3& kneeDir);

// The avatar as posed (avatar.cpp's pose()): for the debug skeleton beside it and the error log (vr_bodytrack_stats).
struct Posed
{
    glm::vec3 pelvis, top, neutral; // the spine from the pelvis to the top of the neck; `neutral` its upright direction
    glm::vec3 fwd;
    glm::vec3 shoulder[2], elbow[2], wrist[2]; // [0] left
    glm::vec3 hip[2], knee[2], ankle[2];
    float leanHold;
};
void posed(const Posed& p);

void registerCommands(); // vr_bodytrack_status, vr_bodytrack_stats, vr_bodytrack_dump

} // namespace qvr::bodytrack
