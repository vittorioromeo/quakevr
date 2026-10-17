// vr_handfit.hpp -- the hand fits' shared core (round 22, "One hand-fit core"): a hand's points against a shape, and the
// hand slid along a line until it rests on it.
//
// Three thin callers, each with its own points and line:
// - held::surfaceFit (vr_held.cpp, vr_held_surface_fit): a carried prop on the fist's cells, along the palm's normal;
// - gripfit::fit (vr_gripfit.cpp, vr_grip_fit): the drawn hand out of the prop it takes, along a few ways out of it;
// - the ragdoll's handFit (vr_box3d_ragparts.cpp, vr_ragdoll_grab_fit): a taken limb onto the palm, along its normal.
//
// The shape query is sunk(): how far a point is in the shape past what it is allowed (its probe's w). A shape that can
// say it exactly also answers outAlong(): how far the point must move along the line to be no further in. The solver,
// slide(), takes a set of probes along a line: from that exact answer where the shape has one, else by marching the line
// and halving the step where the probes cross the surface.
#pragma once

#include "vr_engine.hpp"

namespace qvr::handfit
{

inline constexpr float none = 1e30f; // never (out of reach: no move does it)

struct Triangle
{
    glm::vec3 p[3];
};

// A shape, in the frame the probes are given in (a view: it owns nothing).
struct Shape
{
    enum class Kind : unsigned char
    {
        Hulls,    // convex hulls: faces (normal, distance: in a hull when n.p < w for every face), each hull's end
        Surface,  // triangles seen along the line, which is the frame's -y: what of them is nearest -y over a point
        Distance, // a distance to the surface (negative: in it) at any point, nothing more (a Box3D body)
    };
    Kind kind{Kind::Hulls};

    const glm::vec4* planes{nullptr}; // Hulls
    const int* hullEnds{nullptr};
    int hulls{0};

    const Triangle* triangles{nullptr}; // Surface
    int triangleCount{0};

    float (*distance)(const void* context, const glm::vec3& p){nullptr}; // Distance (units)
    const void* context{nullptr};

    [[nodiscard]] static Shape ofHulls(const glm::vec4* planes, const int* hullEnds, int hulls);
    [[nodiscard]] static Shape ofSurface(const Triangle* triangles, int count);
    [[nodiscard]] static Shape ofDistance(float (*distance)(const void*, const glm::vec3&), const void* context);
};

// The probes: points of the hand in the shape's frame, w how deep each may sink in (negative: how far it keeps out).
// Taken in groups of `count` points from `first`, `alternatives` sets of them one after the other (a finger at each of
// its curls): a group is out when one of its sets is (all its points), at the set that comes out soonest.
struct Group
{
    int first{0};
    int count{0};
    int alternatives{1};
};

struct Probes
{
    const glm::vec4* points{nullptr};
    const Group* groups{nullptr};
    int groupCount{0};
};

// The shape query: how far `p` is in the shape past `allowed` (positive: in; a Hulls shape says 0 out of it, and leaves a
// hull at the first face `p` is within `allowed` of). `face`, if given (Hulls): the nearest face of the hull it is
// deepest in, if it is in one. A Surface with nothing over `p`: -none.
[[nodiscard]] float sunk(const Shape& shape, const glm::vec3& p, float allowed, int* face = nullptr);

// How far `p` must move along `e` (unit) to be in the shape no further than `allowed`, where the shape can say it
// exactly: Hulls (in each hull it is in past it, until the first of its faces comes within it; the most of the hulls';
// 0 if it is not in; none if it can't along `e`), Surface (along -y only: negative if it may come that far in; -none
// with nothing over it). A Distance shape can't (none).
[[nodiscard]] float outAlong(const Shape& shape, const glm::vec3& p, const glm::vec3& e, float allowed);

struct Slide
{
    bool found{false};
    float t{0.f};      // how far the probes move along the line
    float before{0.f}; // (marched) the deepest group's sunk() where they were
};

// The solver: the probes moved along `e` (unit) until they rest on the shape, `t` along it. Hulls, Surface (outAlong):
// at once, the least move that leaves no group in past its allowance (the caller caps it; Hulls only ever push out, a
// Surface also draws in to it); found unless no group has anything over it (Surface) or one can't come out (Hulls: t is
// none). Distance (marched, `reach` at most): from clear of it, the first move towards it (positive) where they touch it;
// from in it, the first move back out (negative) where they are all clear; not found if none within the reach.
[[nodiscard]] Slide slide(const Shape& shape, const Probes& probes, const glm::vec3& e, float reach);

} // namespace qvr::handfit
