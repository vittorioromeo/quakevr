// vr_grasp.hpp -- fitted hands (round 21): the jointed hand's fingers (vr_handrig.hpp) wrap what the hand holds --
// a weapon's grip, the other hand's weapon's foregrip or blade, a carried box, backpack, gib, head or armour, the
// flashlight -- instead of closing into their fixed fist.
//
// The solve (once per grip: when what is held, or where it sits in the hand, changes): each finger closes along its
// own curl path from open (curl 0) to the tightest fist (curl 4), all its joints together; as soon as one of its
// segments would pass into the held thing's triangles (the drawn model's, as drawn), the joints that move that segment
// stop there (found to a hundredth of a degree by halving), and the joints past it go on closing (Miller et al.,
// GraspIt!'s "auto-grasp"): the finger wraps round what it meets, joint by joint. What the solve keeps, per joint, is
// the curl it stopped at.
//
// Each frame (no new solve): a finger closes as far as the controller says (its curl), but never past where it
// stopped; the more the controller closes it (grip, trigger, thumb), the more it is drawn to the stop even past the
// controller's curl (a gripping hand holds on), up to wrapping fully at a full press. With nothing met, the curl is
// the controller's as before. Nothing of the solve runs per frame.

#pragma once

#include "vr_handrig.hpp"

#include <vector>

namespace qvr::grasp
{

struct Triangle
{
    glm::vec3 p[3];
};

// The triangles `e` is drawn with, in world space: an alias model's pose (`frame`, or the entity's if < 0) as the
// renderer places it (vr_render.cpp's transforms, mirrored or not), a brush model's faces. False if it has none.
bool worldTriangles(const entity_t& e, bool mirrored, int frame, std::vector<Triangle>& out);

// What the solve found for a finger: per joint, the curl it stops at (4, the tightest fist, if nothing stops it),
// and whether anything was met at all.
struct FingerStop
{
    float stop[handrig::jointsPerFinger]{4.f, 4.f, 4.f};
    bool met{false};
    bool startsInside{false}; // in it at every curl (left as the controller has it)
    bool fromClosed{false};   // in it open: closed from the tightest curl it is clear at
};

struct Solution
{
    FingerStop finger[handrig::FingerCount];
    glm::vec3 palm{0.f};  // the hand's move (rig space) to hold it flush, the fingers solved there
    glm::quat thumbTurn{1.f, 0.f, 0.f, 0.f}; // the thumb's metacarpal turn (Pose::metacarpal) it closes at
    int places{0};        // the hand's places tried
    double seconds{0.0};
    int triangles{0}; // the held thing's, near the fingers
};

// Solves the hand of `pose` (its shifts and metacarpal; its curls are ignored) against `tris` (rig space): first
// the palm, moved along its normal to sit flush on what it holds (out of it, or in to touch it; at most
// `palmLimit` hand units, 0 not at all), then the fingers there.
void solve(const handrig::Pose& pose, const std::vector<Triangle>& tris, Solution& out, float palmLimit);

// A finger's joint curls this frame: `curl` the controller's (0..5, vr_view.cpp's), `engage` how much it grips
// (0..1: drawn to the stops past `curl`).
void curls(const FingerStop& stop, float curl, float engage, float out[handrig::jointsPerFinger]);

// The curl path's place for the controller's curl: 0..4 closing, then 5 back to 3's shape (the old frames').
[[nodiscard]] float pathCurl(float curl);

} // namespace qvr::grasp
