// vr_weight.hpp -- how heavy what the hands hold feels (docs/vr-port/ROUND21.md, "Weight: spring model, stamina, held
// object offsets; explosive boxes"). Two models (vr_weight_model):
//
// - Speed Limit (0, the old one, vr_handpose.cpp): each frame a hand moves and turns only part of the way to where it is
//   tracked, by a factor from the weapon's Weight. Kept as it was for weapons; a carried prop (vr_weight_props) trails
//   the hand by a time from its mass, as a weapon of that mass does.
// - Spring (1, here): what a hand holds is a rigid body pulled to the tracked hand by a damped spring on its place and
//   on its turn, with its mass, its centre of mass off the grip and its inertia about the grip (a weapon's Mass, Balance
//   and Length; a prop's from its model and Held Object Offsets). The arm's stiffness is the same for everything, so a
//   heavier thing is slower to follow (it lags, overshoots a little and settles), a longer one slower to turn; the arm's
//   force and the wrist's torque are limited, so a heavy thing can't be swung as fast as the hand goes. Gravity makes it
//   droop a little (more at arm's length), a hand's acceleration swings its centre of mass. Two hands on it are much
//   stiffer and stronger. The drawn hand stays on it: the whole hand moves (what it holds, the aim, the melee's points,
//   the arm's IK), as with the Speed Limit.
//
// Both: tired, things weigh more (vr_weight_stamina): below vr_weight_stamina_from of the parry stamina the weight grows
// smoothly to vr_weight_stamina_max times at none.

#pragma once

#include "vr_hands.hpp"

namespace qvr::weight
{

[[nodiscard]] bool springModel(); // vr_weight_model 1

// The stamina's weight multiplier (1 at or above the threshold, vr_weight_stamina_max with none left), eased over
// a quarter of a second (the stamina is sent in whole percent).
[[nodiscard]] float staminaMultiplier();
// The multiplier for a stamina left of `left` (0..1), as the settings are: the curve itself.
[[nodiscard]] float staminaCurve(float left);

// Speed Limit: the part of the way a hand follows each 1/100 s (`perStep`, from the weapon's Weight), heavier with the
// stamina's multiplier (the lag's time constant times it); a hand carrying a prop (vr_weight_props): from its mass
// (`direction`: its turn). Returned unchanged for a weapon with the stamina's multiplier at 1.
[[nodiscard]] float speedLimitStep(int hand, float perStep, bool direction);

// Spring: after the two-handed aim (vr_handpose.cpp), each hand holding something with a mass moved and turned to
// where its spring has it (in the body's frame: walking and turning are not the hand's motion). `dt`: the frame's real
// time; `newFrame`: false when the hands are worked out again within a frame (the spring advances once a frame).
void spring(hands::State& s, float turnYaw, float dt, bool newFrame);

// vr_debug_weight with the Speed Limit: its target and drawn pose of `hand` (the body's frame: units, angles with the play
// space's turn out), traced as the Spring's are.
void traceSpeedLimit(int hand, const glm::vec3& target, const glm::vec3& drawn, const glm::vec3& targetAngles,
    const glm::vec3& drawnAngles, float dt);

// What a hand holds, as the weight sees it (the menu's readout, the trace).
struct Load
{
    bool valid{false};
    bool prop{false};          // a carried thing (else a weapon or the flashlight)
    int entity{0};             // the prop's client entity
    const char* model{""};
    float mass{0.f};           // kg, before the stamina
    float staminaMult{1.f};
    float twoHanded{0.f};      // 0..1: a two-handed grip (a weapon's transition; a prop in both hands: 1)
    glm::vec3 com{0.f};        // metres from the grip, in the hand's frame (forward, left, up)
    glm::vec3 inertia{0.f};    // kg m^2 about the grip, the hand's axes (forward: roll; left: pitch; up: yaw)
};
[[nodiscard]] Load load(int hand);

// The spring's state of a hand last frame: how far the held thing was off the tracked hand (cm, degrees).
struct Offset
{
    bool active{false};
    float distance{0.f};
    float angle{0.f};
};
[[nodiscard]] Offset offset(int hand);

void reset();

// vr_weight_test: the spring offline (vr_weight.cpp).
void registerCommands();

} // namespace qvr::weight
