// vr_convex.hpp -- a drawn model's solid in convex pieces (the guns' bodies in Box3D; ROUND21.md "Guns in convex
// pieces").
//
// One convex hull of a gun spans the air between its parts: the shotgun's fills its shell port and the space between
// its hammer and its barrel, a magazine gun's its well, so a round lying on it rests on nothing drawn and can't get in.
// decompose() cuts the model's solid into pieces whose hulls follow it: the model's triangles are voxelised (a cell a
// little over a tenth of a unit), the space they close found (the outside flooded from the grid's border), and the
// distance of every outside cell from the solid measured. A piece is a box of model space; its hull is that of the
// triangles clipped to the box (and of the solid's cells on its cut faces, so that neighbouring pieces meet). Its gap is
// the most any point of its hull's surface lies off the solid (the outside cells' distance under it). The piece with the
// largest gap is cut in two along the model's axes, where the two halves' hulls hold the least volume, until every gap is
// within the tolerance or the pieces run out.

#pragma once

#include "vr_engine.hpp"

#include "Zancle/Container/Vector.hpp"

#include <box3d/collision.h>

namespace qvr::convex
{

struct Settings
{
    float cell{0.12f};      // the voxel grid's cell (model units; larger for a model over 200 cells long)
    float tolerance{0.3f};  // a piece is done when its hull's surface lies within this of the solid (model units)
    int maxPieces{12};
    bool verbose{false};    // each piece printed
};

// Each piece: its hull's points (model units).
struct Piece
{
    za::Vector<glm::vec3> points;
};

struct Report
{
    float wholeGap{0.f};    // the one hull of the whole model: the most its surface lies off the solid (units)
    float gap{0.f};         // the pieces': the most any piece's hull's surface does
    float wholeVolume{0.f}; // cubic units: the one hull's
    float volume{0.f};      // the pieces' hulls' together
    float solid{0.f};       // the voxelised solid's
    float cell{0.f};        // the grid's cell as used
    int cells{0};           // the grid's cell count
    double ms{0.0};         // the time it took
};

// Box3D's hull of the points with at most `budget` vertices, or fewer when that many would have more edges than it
// takes (128: a hull of triangles has about three edges a vertex); nullptr if none (fewer than four, or flat).
[[nodiscard]] b3HullData* hull(const b3Vec3* points, int count, int budget);

// `corners`: the model's triangles, three corners each (model units). Its solid in convex pieces (at least two), or
// false (no grid, or nothing to cut: the caller keeps one hull). `report` (optional) is filled either way.
[[nodiscard]] bool decompose(const za::Vector<glm::vec3>& corners, const Settings& settings, za::Vector<Piece>& pieces,
    Report* report);

} // namespace qvr::convex
