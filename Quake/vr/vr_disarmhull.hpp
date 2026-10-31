// vr_disarmhull.hpp -- an armed humanoid's disarm volumes: convex hulls of what is drawn (ROUND23.md, "Disarm volumes
// as hulls of the drawn triangles").
//
// His note (2026-10-31): the volumes were capsules off the weapon model's box (its local x the axis, half its larger
// cross side the radius: an axe's head made it twice the axe, a knight's sword whose blade is not its model's x got a
// near-sphere round half the body and an upright line across a level blade) and a hand line from its grip towards a
// shoulder guessed from its row (floating off the drawn hand). Now each volume is the convex hull of the triangles drawn:
// - the weapon: its model's triangles in their own space (held's drawn mesh: its frame's pose, the networked scale and
//   offset), the hull made once per mesh, placed each query where the weapon is drawn (hitreact::drawnWeapon: after
//   every client step; not drawn lately: the server's place);
// - the hand: the drawn body's vertices on its hand bone and forearm (the authored rig as drawn: drawnpose::armedMesh;
//   a ragdoll's: ragdoll::skinnedVertices; neither: the animation frame's), its hull made once a frame.
// One query (gap) for every user: shots (a path), melee strikes (a sweep, its own thickness) and body blows (a point),
// each with its own signed grow (units: each face moved out, or in, by it; 0 the hull itself).

#pragma once

#include "vr_engine.hpp"

#include "Zancle/Container/Vector.hpp"

#include <box3d/math_functions.h>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace qvr::disarmhull
{

struct Hull
{
    za::Vector<b3Vec3> corners;   // its corners (Box3D's quickhull, at most `budget` of them)
    za::Vector<glm::vec4> planes; // its faces (n, w): n.x <= w for every point it was made of (pushed out to hold them)
    za::Vector<glm::ivec2> edges; // corner pairs (drawing)
    float slack{0.f};             // the most a point lay outside the corners' hull (the planes pushed out by it)
    glm::vec3 lo{0.f}, hi{0.f};   // the points' box
    int points{0};                // how many it was made of
    bool ok{false};
};

// A hull where it is: world = origin + axes * local.
struct Placed
{
    const Hull* hull{nullptr};
    glm::mat3 axes{1.f};
    glm::vec3 origin{0.f};
};

// The hull of the points (none, or all in a plane: their box's).
void make(const glm::vec3* points, int count, Hull& out);

// The gap (units) between the path a..b (world; a point if a == b) and the hull grown by `grow` (signed: each face moved
// out by it; under 0 in). 0 or less: they meet; `along` (if given) the path's share where it first does (or its nearest
// point's, if they do not).
[[nodiscard]] float gap(const Placed& p, const glm::vec3& a, const glm::vec3& b, float grow, float* along = nullptr);

// Spheres (world centre, radius in w: a fist's) against the hull grown by `grow`: the least of each one's gap (gap() of
// its centre less its radius; 0 or less: met), done as soon as one is `stopAt` near or nearer. The grab's (vr_physics).
[[nodiscard]] float spheresGap(const Placed& p, const za::Vector<glm::vec4>& spheres, float grow, float stopAt = -1e30f);

// Armed humanoid's held weapon `p` as drawn now (its drawn model's hull, where drawn). False: no drawn surface.
[[nodiscard]] bool weapon(edict_t* p, Placed& out);

// Armed humanoid `g`'s hand on `side` (0 the main weapon's, 1 the left one's: agarms::ragdollGrip) and its forearm as
// drawn now (world). False: no rig, nothing drawn.
[[nodiscard]] bool hand(edict_t* g, int side, Placed& out);

// The hull's edges grown by `grow` (Show Disarm Volumes).
void draw(const Placed& p, float grow, const glm::vec4& colour, float width);

// vector disarmhull(entity e, float what, vector a, vector b, float grow) (QC; vr_armedgrunt_shot.qc, _parry.qc...).
void PF_disarmhull();

} // namespace qvr::disarmhull
