// vr_throw.hpp -- throw velocity estimation.
//
// Every host frame each hand's velocity is sampled; when an object is let go, its velocity is
// estimated from the recent samples. Velocities are metres (radians) per second, in Quake axes
// turned with the play space, and exclude the player's own movement.

#pragma once

#include <glm/glm.hpp>

namespace qvr::throwing
{

struct Estimate
{
    glm::vec3 vel{0.f};    // of the held object's centre
    glm::vec3 angVel{0.f}; // of the hand
};

// `forward` is the hand's (unit) aim direction, along which the held object's centre lies.
void sample(int hand, double time, const glm::vec3& vel, const glm::vec3& angVel, const glm::vec3& forward);

[[nodiscard]] Estimate estimate(int hand);

void reset();

} // namespace qvr::throwing
