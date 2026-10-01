// vr_angvel.hpp -- the controllers' angular velocity in the tracking space, whatever frame the runtime reports it in.
//
// OpenXR gives XrSpaceVelocity::angularVelocity in the base space (the tracking space); VirtualDesktopXR 1.0.10 gives
// it in the controller's own frame (ROUND21.md, "The runtime's angular velocity frame"). Every consumer (the legacy
// pose's velocity, the calibrated hand's point, the hands' angVel: melee, flick reload, throws without
// vr_throw_spin_from_pose) reads TrackingState::hands[].angularVelocity after fix() has put it in the tracking space.

#pragma once

#include "vr_backend.hpp"

namespace qvr::angvel
{

// The frame vr_angvel_frame picked (-1 auto: what the runtime's name suggests, then what its samples show).
enum class Frame : int
{
    Tracking = 0, // as OpenXR says: used as it comes (the old behavior)
    Controller = 1, // the controller's own frame (VirtualDesktopXR): turned into the tracking space
    Pose = 2 // the controller's turn between samples, the runtime's ignored
};

// Once per frame, after the backend's tracking (and a take's playback, which replays the runtime's own numbers):
// turns the hands' angular velocity into the tracking space and, for a pose moved off the grip
// (vr_controller_legacy_pose), redoes its linear velocity's lever term with it (the backend's toLegacyPose used the
// runtime's). `runtime`: the runtime's name (a take's "source" while it plays): it seeds vr_angvel_frame -1.
void fix(TrackingState& t, const char* runtime);

// The frame in use for the last fix() (vr_angvel_frame -1: the detected one).
[[nodiscard]] Frame current();

// Forgets the detection's evidence and the previous samples (a take starting to play).
void reset();

// The controller's frame's turn from a hand's pose (hands[h], with its grip in it), for the mock backend's
// vr_mock_angvel_local: tracking = q * turn * local.
[[nodiscard]] glm::quat controllerFrame(const Pose& hand, const GripInRaw& gripInHand);

} // namespace qvr::angvel
