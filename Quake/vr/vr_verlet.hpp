// vr_verlet.hpp -- a chain of points (a rope, a chain) stepped by Verlet integration, held to its pieces' lengths and
// kept out of the world: the grappling hook's drawn rope (vr_rope.cpp stepChain) and the magnetic holsters' tether
// (vr_magholster.cpp). Client-side, world units. The points are given with last step's (`prev`: their velocity) and
// which are pinned (moved by the caller only).

#pragma once

#include "vr_engine.hpp"

#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Container/Vector.hpp"


namespace qvr::verlet
{

// The free points moved on by their velocity (kept by the step's length over the last one's, `drag` of it lost to the
// air a second) and fallen by `gravity` (units/s/s) over `dt` seconds.
void fall(glm::vec3* p, glm::vec3* prev, const char* pinned, za::SizeT n, float dt, float lastDt, float drag, float gravity);

// Each piece (p[j], p[j + 1]) pulled back to `rest[j]` units when longer (a rope pulls, it doesn't push: bunched up, its
// slack lies still rather than shoving), `iterations` passes from the first point, `stiffness` (0 .. 1) of each
// correction made a pass.
void holdLengths(glm::vec3* p, const char* pinned, za::SizeT n, const float* rest, int iterations, float stiffness);

// The world: each free point traced from where it was before this step (`from`: stopped on what it meets, the rest of
// its move slid along it; its speed into it gone, `slide` of its speed along it kept; held `rad` units off a floor it
// lies on), then each piece (one through the world: its free ends pushed out past where it met it). `traces`, when
// given, counts the traces made.
void collide(glm::vec3* p, glm::vec3* prev, const glm::vec3* from, const char* pinned, za::SizeT n, float rad, float slide,
    int* traces = nullptr);

// The polyline `pts[0 .. count - 1]` as `n` + 1 points evenly along it (its ends kept), appended to `out`.
void resample(const glm::vec3* pts, int count, int n, za::Vector<glm::vec3>& out);

} // namespace qvr::verlet
