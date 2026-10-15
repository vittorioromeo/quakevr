// vr_gripfit.hpp -- the grip fit (round 22, "Grip fit: the hand out of what it grips"): once per grab, how deep the posed
// hand sits in the prop's collision shape, and the prop moved in the hand, outward, until it sits no deeper than
// vr_grip_fit_depth (or the best place within vr_grip_fit_steps steps and vr_grip_fit_max).
//
// The hand is its grasp spheres (vr_grasp.hpp posedSpheres: the fingers' segments and the palm) as the jointed hand draws
// it, open and in the full fist, in its frame (the move's place and angles). A sphere counts as sunk in by the less of
// its two poses' depths: a finger in it open but clear of it closed (or the other way round) is one the grasp solve wraps
// clear of it (vr_grasp.hpp: closed from where it is clear); one in it both ways phases through it however it curls, as
// does a palm sphere in it. The shape is the prop's Box3D hulls (each face's plane: a sphere's depth in a convex hull
// is its radius less its deepest face distance, exact inside and never too little outside), or its drawn box without
// a body. The escape direction: the sunk spheres' nearest faces' outward normals, weighted by how deep each is (the
// deepest's alone if those cancel). Each step moves the hand along it (the prop the other way) by vr_grip_fit_step at
// most (no more than takes it to the allowed depth), measured again; the shallowest place found is kept.
//
// No allocation (fixed arrays: 160 spheres, 256 faces, 16 hulls); a few microseconds even at 64 steps (vr_grip_fit_last
// times it); nothing at all between grabs. Only the local player's hands are known (a listen server; elsewhere it does
// nothing). Not for weapons (they are held by their own grips), nor a prop whose grip is Fixed or Along the Handle (its
// place in the hand is authored), nor a hand grenade or round from a pouch.

#pragma once

#include "vr_engine.hpp"

#include "Zancle/Container/Vector.hpp"


namespace qvr::gripfit
{

// What the last fit did.
struct Result
{
    bool ran{false};     // measured (on, the hand known, a shape)
    int num{0};          // the prop's entity
    int hand{0};         // 0 off, 1 main
    bool twoHand{false}; // the second hand taking hold of what the first holds
    bool hull{false};    // its Box3D hulls (else its drawn box)
    float before{0.f};   // the hand's deepest sphere's depth in it before (units; negative: clear of it)
    float after{0.f};    // and after
    int steps{0};
    glm::vec3 move{0.f}; // the hand's move on it (the prop's frame, units): the prop goes the other way
    int spheres{0}, planes{0};
    double micros{0.0};
};

// Client side, every frame where it is measured (vr_view.cpp): `hand`'s (0 off, 1 main) open hand's grasp spheres, in its
// frame (world units); vr_view.cpp writes them here as it measures the palm (no copy). Cleared: not known.
[[nodiscard]] za::Vector<glm::vec4>& openHand(int hand);

// Server side, at a grab: the hand `hand` of the local player (`local`), its spheres taken to the prop's frame by
// `handToProp` and `handInProp` (p_prop = handToProp * p_hand + handInProp); the prop entity `num` (its Box3D hulls), or
// its drawn box `lo`..`hi`. True if it moved: `move`, the hand's move on the prop (its frame, units).
bool fit(int num, int hand, bool local, bool twoHand, const glm::mat3& handToProp, const glm::vec3& handInProp, const glm::vec3& lo,
    const glm::vec3& hi, glm::vec3& move);

// What the last fit did.
[[nodiscard]] const Result& last();

// vr_grip_fit_last: the last fit, then its inputs fitted again 1000 times at the most steps (the worst case), timed.
void last_f();

} // namespace qvr::gripfit
