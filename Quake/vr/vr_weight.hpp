// vr_weight.hpp -- how heavy what the hands hold feels (docs/vr-port/ROUND21.md, "Weight: spring model, stamina, held
// object offsets; explosive boxes" and "Spring only; Weapon Weights and Held Object Weights; weight and damage").
//
// What a hand holds (a weapon, a carried prop, the flashlight) is a rigid body pulled to the tracked hand by a damped
// spring on its place and on its turn, with its mass, its centre of mass off the grip and its inertia about the grip (a
// weapon's Mass, Balance and Length, Weapon Weights; a prop's from its model and Held Object Weights). The arm's
// stiffness is the same for everything, so a heavier thing is slower to follow (it lags, overshoots a little and
// settles), a longer one slower to turn; the arm's force and the wrist's torque are limited, so a heavy thing can't be
// swung as fast as the hand goes. Gravity makes it droop a little (more at arm's length), a hand's acceleration swings
// its centre of mass. Two hands on it are much stiffer and stronger. The drawn hand stays on it: the whole hand moves
// (what it holds, the aim, the melee's points, the arm's IK). Each setting of the spring (Aiming: Spring) is multiplied
// by the thing's own (Weapon Weights, Held Object Weights: 1 by default).
//
// Tired, things weigh more (vr_weight_stamina): below vr_weight_stamina_from of the parry stamina the weight grows
// smoothly to vr_weight_stamina_max times, and vr_weight_stamina_add kg more, at none; an empty hand weighs up to
// vr_weight_stamina_empty kg (a fist: it lags its controller as a held thing does; not on a hold or steadying a weapon).
// See vr_fatigue.hpp for the tired arms' shake.
//
// The mass also changes the melee's and the throws' damage (damageMultiplier) and, for heavy things, lowers their speed
// thresholds (leniency): the QC reads both (weightdamage, weightleniency builtins).

#pragma once

#include "vr_hands.hpp"

namespace qvr::weight
{

// The stamina's weight multiplier (1 at or above the threshold, vr_weight_stamina_max with none left), eased over
// a quarter of a second (the stamina is sent in whole percent).
[[nodiscard]] float staminaMultiplier();
// The multiplier for a stamina left of `left` (0..1), as the settings are: the curve itself.
[[nodiscard]] float staminaCurve(float left);
// The curve's share, 0 (at or above the threshold, or off) .. 1 (none left): for a stamina left of `left`, and now (eased).
[[nodiscard]] float staminaShareFor(float left);
[[nodiscard]] float staminaShare();

// After the two-handed aim (vr_handpose.cpp), each hand holding something with a mass moved and turned to
// where its spring has it (in the body's frame: walking and turning are not the hand's motion). `dt`: the frame's real
// time; `newFrame`: false when the hands are worked out again within a frame (the spring advances once a frame).
void spring(hands::State& s, float turnYaw, float dt, bool newFrame);

// The spring's multipliers of the global settings for one thing (Weapon Weights, Held Object Weights).
struct Tuning
{
    float stiffness{1.f}, damping{1.f}, strength{1.f}, sag{1.f}, swing{1.f}, twoHanded{1.f}, snap{1.f};
};

// What a hand holds, as the weight sees it (the menu's readout, the trace).
struct Load
{
    bool valid{false};
    bool prop{false};          // a carried thing (else a weapon or the flashlight)
    int entity{0};             // the prop's client entity
    const char* model{""};
    float mass{0.f};           // kg, before the stamina
    float staminaMult{1.f};
    float staminaAdd{0.f};     // kg
    bool empty{false};         // an empty hand, tired (its mass is the stamina's: vr_weight_stamina_empty)
    float twoHanded{0.f};      // 0..1: a two-handed grip (a weapon's transition; a prop in both hands: 1)
    glm::vec3 com{0.f};        // metres from the grip, in the hand's frame (forward, left, up)
    glm::vec3 inertia{0.f};    // kg m^2 about the grip, the hand's axes (forward: roll; left: pitch; up: yaw)
    Tuning tune;
};
[[nodiscard]] Load load(int hand);
// The mass the spring moves: with the stamina's.
[[nodiscard]] float effectiveMass(const Load& l);

// The spring's state of a hand last frame: how far the held thing was off the tracked hand (cm, degrees).
struct Offset
{
    bool active{false};
    float distance{0.f};
    float angle{0.f};
};
[[nodiscard]] Offset offset(int hand);

void reset();

// Weight and damage: a melee blow's or a throw's damage multiplier for a thing of `mass` kg (vr_weight_damage_*: 1 between
// the light and heavy masses and for no mass; more for heavier, less for lighter, sublinear, clamped).
[[nodiscard]] float damageMultiplier(float mass);
// Heavy leniency: the factor on the speed thresholds of a melee strike and a throw's hit for a thing of `mass` kg
// (vr_weight_lenient*: 1 up to vr_weight_lenient_from, lower for heavier things, at least vr_weight_lenient_min).
[[nodiscard]] float leniency(float mass);

// vr_weight_test: the spring offline (vr_weight.cpp).
void registerCommands();

} // namespace qvr::weight
