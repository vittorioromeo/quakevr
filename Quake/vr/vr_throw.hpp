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
    // Slow motion: degrees the slowed hand's own estimate went off the way the controller moved (0: the same, or not
    // slow motion), and metres the hand was behind its controller at the peak (vr_debug_throw).
    float aimTurn{0.f};
    float lag{0.f};
    float rate{1.f}; // the windows' seconds in the samples' clock's one (slow motion: the real time's times the gain)
    float travel{0.f};     // the stroke's path (metres) and length (real seconds)
    float strokeTime{0.f};
    // The multiple of the controller's real speed thrown (1 to 1/timescale; under strokeGain's when capped), the
    // stroke's tempo (real seconds: its path over its peak) and peak speed (real m/s); 0: no estimate.
    float gain{0.f};
    float tempo{0.f};
    float peak{0.f};
};

// A hand's motion at a sample: its world position, velocity, spin and (unit) aim direction, along which the held
// object's centre lies.
struct Motion
{
    glm::vec3 pos{0.f};
    glm::vec3 vel{0.f};
    glm::vec3 angVel{0.f};
    glm::vec3 forward{1.f, 0.f, 0.f};
    float spinLag{0.f}; // seconds `angVel` is older than the sample (a turn between two frames is its middle's: half one)
};

// `hand`: the hand's motion (as drawn: in slow motion the slowed hand, timescale::filterHands); `controller`: its
// controller's own (the same unless the slowed hand lags it), whose estimate gives the throw's direction and spin axis in
// slow motion (strokeEstimate).
void sample(int hand, double time, const Motion& handMotion, const Motion& controller);

// The estimate as of the newest sample, as if released now.
[[nodiscard]] Estimate estimate(int hand);

// The estimate for a release at `releaseTime`: the peak in the window before it, the samples taken as a signal in time
// (the same throw at any frame rate).
[[nodiscard]] Estimate estimateAt(int hand, double releaseTime);

// Both hands holding one object (vr_carry2h.hpp), released at `releaseTime`: the estimate of the object's own motion
// from both hands' samples (the peak of its centre's speed, as estimateAt's), its centre `centre` metres from the
// middle of the hands (world axes). Its spin is a rigid body's held at both hands: the hands' own about the line
// between them, and the line's turn.
[[nodiscard]] Estimate estimateBothAt(double releaseTime, const glm::vec3& centre);

// The one throw: the estimate for the hands of `handMask` (bit h: hand h; HAND_OFF 0, HAND_MAIN 1) letting go at `at`.
// Both hands: estimateBothAt (`centre`: the object's from the middle of the hands, metres); one: estimateAt.
[[nodiscard]] Estimate release(int handMask, double at, const glm::vec3& centre = glm::vec3{0.f});

// When `hand` let go, for its throw: the grip's own release time (releaseTime) if it is this release (not a key's, nor a
// stale one: within 50 ms after to 250 ms before the newest sample), else the newest sample's.
[[nodiscard]] double releaseAt(int hand);

// What is thrown: each kind's limit on its velocity (kindCap). A hand-held prop or weapon has none here (its weight's,
// weight::throwVelocity, and the QC's).
enum class Kind
{
    Hand,       // a prop or a weapon let go
    Flashlight, // the lamp let go: it flies home from where it goes (3 m/s at most)
    Physgun,    // the toolgun's beam letting go (1000 u/s at most)
};

// `vel` (world units per second) limited to `kind`'s top speed.
[[nodiscard]] glm::vec3 kindCap(Kind kind, const glm::vec3& vel);

// The physgun's held point (one at a time): its samples (world units, units per second, on the drag's own clock) and its
// throw, smoothed as a hand's (the peak in vr_throw_window, fitted over vr_throw_peak_span; not pitched, no flick).
// pointReset when a drag starts.
void pointReset();
void pointSample(double time, const glm::vec3& pos, const glm::vec3& vel);
[[nodiscard]] Estimate pointEstimate(double releaseTime);

// Time of the newest sample (0 without any).
[[nodiscard]] double latestTime(int hand);

// Replaces the controller grips of `t` with the analog release detection (vr_throw_release),
// before they become keys; remembers when each hand let go.
void filterGrips(TrackingState& t);

// When `hand`'s grip last let go (tracking clock), or < 0 if unknown.
[[nodiscard]] double releaseTime(int hand);

void reset();

} // namespace qvr::throwing
