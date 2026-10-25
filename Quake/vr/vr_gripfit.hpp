// vr_gripfit.hpp -- the grip fit (round 22, "Grip fit: the hand out of what it grips"; its second pass, "the drawn
// hand on the surface"): once per grab, how far the drawn hand sinks into the prop it takes, and the prop moved in the
// hand, the least way that leaves the hand resting on its surface.
//
// The hand is the drawn mesh's points (vr_handrig.hpp: its vertices, posed as the hand draws them): the palm, the back of
// the hand and the wrist (the palm joint's vertices: the rest of the hand, rigid), and each finger and the thumb (with the
// ball of the thumb) at three curls of its closing path (open, half, the tightest fist), each part thinned to a few
// points spread over it (farthest-point sampling, once per rig: 48 for the palm, 6 for each finger). A finger is taken as
// the grasp solve draws it (vr_grasp.hpp): closed from open until it touches, or, sunk in open, closed from the tightest
// curl it is clear at; so it sinks in only if it is in at every curl, and by its least sunk curl's depth. Its points may
// press vr_grip_fit_finger_depth into the prop (a contact), the palm's vr_grip_fit_palm_depth.
//
// The shape is the prop's Box3D hulls (each face's plane), each face pushed out to the drawn model where the model stands
// out of it (its drawn vertices within 6 cm of the hull: a crate's planks, a barrel's rims), or the drawn box without a
// body. The overlap: every point's depth past its allowance, summed (the fingers' at their least sunk curl).
//
// The move: the ways out are the faces the sunk points are nearest to (the 4 most weighted by depth, the pair of the two
// most, and the depth-weighted sum of all). Along each, how far the hand must go to clear (each point's distance out of
// each hull it is in, solved at once from the face distances); the ways tried in order of that distance, at most
// vr_grip_fit_reach hand lengths, 3 of them measured there: the first that clears is kept (the least move), else the one
// that sinks in least (never deeper than before, else not moved). No allocation (fixed arrays), at most about 12 passes
// over the points and faces; nothing at all between grabs (the hand's rig frame is kept every frame: a matrix). Only the
// local player's hands are known (a listen server). Not for weapons, Fixed or Along the Handle grips; a pouch's grenade or
// round is placed again by the pouch.

#pragma once

#include "vr_engine.hpp"

#include "Zancle/Container/Vector.hpp"


namespace qvr::handrig
{
struct Pose;
}

namespace qvr::gripfit
{

// What the last fit did (overlaps: cm, the points' depths past their allowances, summed).
struct Result
{
    bool ran{false};     // measured (on, the hand known, a shape)
    int num{0};          // the prop's entity
    int hand{0};         // 0 off, 1 main
    bool twoHand{false}; // the second hand taking hold of what the first holds
    bool hull{false};    // its Box3D hulls (else its drawn box)
    float before{0.f}, beforeFingers{0.f}, deepestBefore{0.f};
    float after{0.f}, afterFingers{0.f}, deepestAfter{0.f};
    glm::vec3 move{0.f}; // the hand's move on it (the prop's frame, units): the prop goes the other way
    float cap{0.f};      // the most move (units)
    int directions{0}, measured{0};
    int points{0}, planes{0};
    float grown{0.f};    // cm, the most a face was pushed out to the drawn model
    double micros{0.0}, shapeMicros{0.0}, poseMicros{0.0}; // in all; of which the shape, the hand posed
};

// Client side, every frame where the hand is measured (vr_view.cpp updateGripFrame): `hand`'s (0 off, 1 main) rig
// space to its frame (the move's place and angles; world units), and its fingers' shifts (`pose`); nullptr: not known.
void setHandRig(int hand, const glm::mat4* rigToHand, const handrig::Pose& pose);

// Server side, at a grab: the hand `hand` of the local player (`local`), taken to the prop's frame by `handToProp` and
// `handInProp` (p_prop = handToProp * p_hand + handInProp); the prop entity `num` (its Box3D hulls, pushed out to its
// drawn vertices `drawn`, its frame; nullptr: read here), or its drawn box `lo`..`hi`. True if it moved: `move`, the
// hand's move on the prop (its frame, units).
bool fit(int num, int hand, bool local, bool twoHand, const glm::mat3& handToProp, const glm::vec3& handInProp, const glm::vec3& lo,
    const glm::vec3& hi, const za::Vector<glm::vec3>* drawn, glm::vec3& move);

// What the last fit did.
[[nodiscard]] const Result& last();

// vr_grip_fit_last: the last fit, then its inputs fitted again 1000 times as the worst case (every point sunk in every
// hull), timed.
void last_f();

// The hand `hand` as drawn this frame (every vertex of its mesh, posed) against the prop it holds (listen server), its
// hulls pushed out to its drawn model as the fit's are: how many of its points are in it and how deep (cm), the fingers
// and thumb apart from the rest. False: nothing held, no hull.
struct DrawnOverlap
{
    struct Part
    {
        int points{0}, inside{0};
        float deepest{0.f}, sum{0.f}; // cm
    };
    int num{0};
    Part body, fingers;
};
bool drawnOverlap(int hand, DrawnOverlap& out);

// vr_grip_fit_drawn: drawnOverlap for each hand.
void drawn_f();

} // namespace qvr::gripfit
