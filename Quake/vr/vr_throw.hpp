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
    glm::vec3 pos{0.f};    // world position of the object's centre when it left the hand
    double time{0.0};      // when it left the hand
};

// `pos` is the hand's world position; `forward` its (unit) aim direction, along which the held
// object's centre lies.
void sample(int hand, double time, const glm::vec3& pos, const glm::vec3& vel, const glm::vec3& angVel,
    const glm::vec3& forward);

// The estimate as of the newest sample, as if released now (vr_throw_algorithm 0..2 are always
// computed this way).
[[nodiscard]] Estimate estimate(int hand);

// The estimate for a release at `releaseTime`: with vr_throw_algorithm 3, the peak in a window
// around it; otherwise estimate(hand).
[[nodiscard]] Estimate estimateAt(int hand, double releaseTime);

// Time of the newest sample (0 without any).
[[nodiscard]] double latestTime(int hand);

// Replaces the controller grips of `t` with the analog release detection (vr_throw_release),
// before they become keys; remembers when each hand let go.
void filterGrips(TrackingState& t);

// When `hand`'s grip last let go (tracking clock), or < 0 if unknown.
[[nodiscard]] double releaseTime(int hand);

void reset();

} // namespace qvr::throwing
