#pragma once
// vr_avatar_internal.hpp -- what vr_avatar.cpp (see vr_avatar.hpp) shares with its legs (vr_avatar_legs.cpp) and
// climbing feet (vr_avatar_climb.cpp). Internal: included by those three alone. Namespace avatar::detail: the
// skeleton's types and constants and the legs' and climbing's state (once vr_avatar.cpp's anonymous namespace), and
// the functions each file calls in another.
#include "vr_avatar.hpp"
#include "vr_engine.hpp"
#include "vr_geom.hpp"
#include "vr_hands.hpp"
#include "vr_units.hpp"

#include "Zancle/Container/Array.hpp"

#include <glm/glm.hpp>

namespace qvr::avatar::detail
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
    // The forearms' twist and the wrists' bend, spread (make_vrbody.py): four joints along each forearm, turning
    // with a growing share of the hand's roll, and one at the wrist with all of it and half of the wrist's bend.
    ForeTwist1L,
    ForeTwist2L,
    ForeTwist3L,
    ForeTwist4L,
    WristL,
    ForeTwist1R,
    ForeTwist2R,
    ForeTwist3R,
    ForeTwist4R,
    WristR,
    JointCount
};

constexpr glm::vec3 UP{0.f, 0.f, 1.f};
constexpr glm::vec3 FWD{1.f, 0.f, 0.f};
constexpr glm::vec3 BACK{-1.f, 0.f, 0.f};

using geom::safeNormalize;

// The bind pose, in metres.
struct Bind
{
    za::Array<glm::vec3, JointCount> pos;
    za::Array<glm::mat3, JointCount> rot;
    za::Array<glm::vec3, JointCount> offset; // from the parent, in the parent's frame
    glm::vec3 toe[2];
};

struct Bone
{
    glm::vec3 pos{0.f};
    glm::mat3 rot{1.f};
    float stretch{1.f}; // along the bone
    float size{1.f};    // all axes (COLLAPSED: not drawn)
    glm::mat3 shape{1.f}; // in the bone's own axes, before the rest (the wrist's mitre)
};

struct Body
{
    float m2w{1.f};    // world units per metre of the modelled body
    float legM2w{1.f}; // ... of its legs, hip to ankle (m2w times the leg fit: legFit)
    float floorZ{0.f};
    glm::vec3 fwd{FWD};
    glm::vec3 left{0.f, 1.f, 0.f};
    float crouchPose{0.f}; // the crouched pose's share (crouchPoseWeight; solveArm's shoulders and elbows)
    za::Array<Bone, JointCount> bones{};
};

// vr_avatar.cpp: the bind pose, its bones, two-bone IK.
[[nodiscard]] glm::mat3 basis(const glm::vec3& dir, const glm::vec3& hint);
const Bind& bind();
[[nodiscard]] float boneLength(int from, int to);
[[nodiscard]] glm::vec3 childPos(const Body& b, int joint);
[[nodiscard]] glm::vec3 twoBone(const glm::vec3& root, const glm::vec3& target, float a, float b, const glm::vec3& pole,
    const glm::vec3& fallbackPole, glm::vec3& bend);

// Climbing's weight (Climbing::weight, 0 .. 1), for solveTorso: the lean taken out while climbing.
extern float climbHang;

// ---- The legs (vr_avatar_legs.cpp) ----

struct Gait
{
    float phase{0.f};       // radians, a full cycle is two steps
    float amount{0.f};      // 0 standing .. 1 walking
    float air{0.f};         // 0 on the ground .. 1 in the air
    glm::vec3 dir{1.f, 0.f, 0.f};
    float stride{0.f};      // metres, one step
    float lift{0.f};        // metres, the swinging foot at its highest
};

struct Water
{
    float wade{0.f};     // 0 .. 1
    float swim{0.f};     // 0 .. 1
    float stick{0.f};    // 0 .. 1, how far the stick pushes, smoothed
    glm::vec3 dir{0.f};  // where the stick moves the player (world), smoothed: its length fades with the stick
    float phase{0.f};    // radians, the kicks' cycle
};

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
    za::Array<Foot, 2> feet{};
    int follow{-1}; // the foot that squares up after the other's step
    float lastYaw{0.f};
    float yawRate{0.f}; // degrees per second the body turns, smoothed (snap turns left out)
};

extern Gait gait;
extern unsigned footSteps[2];
extern Water water;
extern Stance stance;

[[nodiscard]] float legsDeltaTime();
void updateWater(const Body& b, const hands::State& s, float dt);
void updateGait(const Body& b, float dt);
void updateStance(const Body& b, const glm::vec3& stand, float dt);
void solveLeg(Body& b, const glm::vec3& stand, int side, bool shown, glm::mat3& still);

// ---- Climbing (vr_avatar_climb.cpp) ----

struct ClimbFoot
{
    glm::vec3 at{0.f};            // the hold: where the ankle goes (world)
    glm::vec3 from{0.f};          // stepping: where the ankle left from
    glm::vec3 now{0.f};           // where the ankle is aimed now
    glm::vec3 fwd{1.f, 0.f, 0.f}; // on a hold: the foot's forward (into the face)
    float step{-1.f};             // stepping: 0 .. 1; below 0 on its hold or hanging
    bool held{false};             // on a hold (or stepping to one); false: hanging
    int kind{0};                  // its hold: 1 the face, 2 a top (a ledge, a rung, the floor)
    float retry{0.f};             // hanging: seconds before it looks again
    int serial{0};                // counts its holds (the logs: one hold's slip)
    float flat{0.f};              // 1 flat on a hold .. 0 pointed, hanging (stepping off a hold: easing)
};

struct Climbing
{
    float weight{0.f};            // 0 .. 1, eased in and out
    glm::vec3 in{1.f, 0.f, 0.f};  // into the face (horizontal), from the holds
    glm::vec3 holdMid{0.f};       // the holds' middle
    float rise{0.f};              // metres a second the hips rise, smoothed
    glm::vec3 lastHips{0.f};
    bool hipsValid{false};
    float sway{0.f};              // radians, the hanging legs' slow swing
    glm::vec3 lag{0.f};           // the hanging feet's lag behind the body's motion (world)
    za::Array<ClimbFoot, 2> feet{};
    int lastStep{-1};             // the foot that took the last hold
    int traces{0};                // this frame's (the logs)
    double updateMs{0.0};         // this frame's updateClimbing (vr_debug_climb_feet 2)
};

extern Climbing climbing;

[[nodiscard]] float toeLength(int side);
[[nodiscard]] float ankleHeight(int side);
[[nodiscard]] glm::vec3 climbOutward(int side);
void updateClimbing(const Body& b, float dt, bool legs);
void logClimbing(const Body& b);

} // namespace qvr::avatar::detail
