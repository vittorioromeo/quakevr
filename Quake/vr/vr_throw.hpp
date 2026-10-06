// vr_throw.hpp -- throw velocity estimation and release detection.
//
// Every host frame each hand's velocity is sampled; when an object is let go, its velocity is
// estimated from the samples around the release. Velocities are metres (radians) per second, in
// Quake axes turned with the play space, and exclude the player's own movement. Times are on the
// tracking clock (TrackingState::time) when the runtime gives one.

#pragma once

#include "vr_backend.hpp"

namespace qvr::throwing
{

struct Estimate
{
    glm::vec3 vel{0.f};    // of the held object's centre
    glm::vec3 angVel{0.f}; // of the hand
    glm::vec3 flick{0.f};  // the part of `vel` the hand's turn gives (the wrist's flick: its spin about the wrist, through
                           // vr_throw_wrist_dist and the lever arm); none for two hands. A heavy thing keeps less of it
                           // (weight::throwVelocity).
    glm::vec3 pos{0.f};    // world position of the object's centre when it left the hand
    double time{0.0};      // when it left the hand
    // vr_throw_slowmo_aim: degrees the slowed hand's own estimate went off the way the controller moved (0: the same,
    // or not slow motion), and metres the hand was behind its controller at the peak (vr_debug_throw).
    float aimTurn{0.f};
    float lag{0.f};
    float rate{1.f}; // the windows' seconds in the samples' clock's one (slow motion: vr_throw_slowmo_real_time, _tempo)
};

// A hand's motion at a sample: its world position, velocity, spin and (unit) aim direction, along which the held
// object's centre lies.
struct Motion
{
    glm::vec3 pos{0.f};
    glm::vec3 vel{0.f};
    glm::vec3 angVel{0.f};
    glm::vec3 forward{1.f, 0.f, 0.f};
};

// `hand`: the hand's motion (as drawn: in slow motion the slowed hand, timescale::filterHands); `controller`: its
// controller's own (the same unless the slowed hand lags it), whose estimate gives the throw's direction and spin axis in
// slow motion (vr_throw_slowmo_aim).
void sample(int hand, double time, const Motion& handMotion, const Motion& controller);

// The estimate as of the newest sample, as if released now.
[[nodiscard]] Estimate estimate(int hand);

// The estimate for a release at `releaseTime`: the peak in a window around it.
[[nodiscard]] Estimate estimateAt(int hand, double releaseTime);

// Both hands holding one object (vr_carry2h.hpp), released at `releaseTime`: the estimate of the object's own motion
// from both hands' samples (the peak of its centre's speed, as estimateAt's), its centre `centre` metres from the
// middle of the hands (world axes). Its spin is a rigid body's held at both hands: the hands' own about the line
// between them, and the line's turn.
[[nodiscard]] Estimate estimateBothAt(double releaseTime, const glm::vec3& centre);

// Time of the newest sample (0 without any).
[[nodiscard]] double latestTime(int hand);

// Replaces the controller grips of `t` with the analog release detection (vr_throw_release),
// before they become keys; remembers when each hand let go.
void filterGrips(TrackingState& t);

// When `hand`'s grip last let go (tracking clock), or < 0 if unknown.
[[nodiscard]] double releaseTime(int hand);

void reset();

} // namespace qvr::throwing
