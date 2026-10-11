// vr_ledges.hpp -- the ledge map: every ledge of each brush model (the world, and doors, plats, trains, walls in
// their own space), found once from the BSP, for climbing to look holds up in (vr_climb.cpp). See vr_ledges.cpp.

#pragma once

#include "vr_engine.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/Vocabulary/Pair.hpp"
#include "vr_zancle.hpp"


namespace qvr::box3d
{
struct ClimbShape;
struct ClimbBody;
} // namespace qvr::box3d

namespace qvr::ledges
{

// What a ledge is (vr_ledges.cpp has the rule in full; vr_climb.cpp's holds are on them).
constexpr float minTopNormal = 0.7f; // a walkable top: its normal's z at least this
constexpr float minDrop = 32.f;      // the drop beyond the lip at least this deep (a stair step isn't a ledge),
constexpr float edgeReach = 16.f;    // starting at most this far out from a hand over the top (so the lip less the
                                     // hand's way in from it)
constexpr float handRoom = 8.f;      // clear space above the top for the hand, at the hold
constexpr float holdInset = 2.f;     // the hold (the palm's middle) is this far in from the lip,
constexpr float holdLift = 1.f;      // and this far above the top
constexpr float maxDepth = 40.f;     // a top's depth from the lip in is measured this far at most
constexpr float sampleStep = 1.f;    // units between a ledge's samples along it

// A ledge: a straight piece of lip, in its model's space.
struct Edge
{
    glm::vec3 a{0.f};                 // one end,
    glm::vec3 dir{1.f, 0.f, 0.f};     // the way to the other (unit),
    float len{0.f};                   // and how far it is
    glm::vec3 out{1.f, 0.f, 0.f};     // horizontal, square to the lip, from the top towards the drop
    glm::vec3 normal{0.f, 0.f, 1.f};  // the top's
    int firstSample{0}, samples{0};   // its samples (Map::samples), every sampleStep along it from
    float sampleStart{0.f};           // this far from `a`
    float spacing{sampleStep};        // (sampleStep, longer along a sloping lip)
    glm::vec3 mins{0.f}, maxs{0.f};   // its box

    [[nodiscard]] glm::vec3 point(float t) const
    {
        return a + dir * t;
    }
    // The top's height at `p` (on its plane, through the lip).
    [[nodiscard]] float topAt(const glm::vec3& p) const
    {
        return a.z - (normal.x * (p.x - a.x) + normal.y * (p.y - a.y)) / normal.z;
    }
};

struct Sample
{
    float dropOut{0.f}; // how far out from the lip the drop starts (the column clear minDrop down)
    float depth{0.f};   // how far the top goes in from the lip (up to maxDepth)
};

// A brush model's ledges, and a grid of them for looking up those near a place.
class Map
{
public:
    za::Vector<Edge> edges;
    za::Vector<Sample> samples;

    // The sample nearest `t` along `e`.
    [[nodiscard]] const Sample& sampleAt(const Edge& e, float t) const;
    // The edges whose boxes touch mins..maxs (each once), appended to `out`.
    void nearby(const glm::vec3& mins, const glm::vec3& maxs, za::Vector<int>& out) const;

    // How it was made: faces looked at, lip pieces found on them, lines they joined into, the ledges, the time.
    int faces{0}, pieces{0}, lines{0};
    double ms{0.0};
    za::U32 serial{0}; // which map of the cache it is (each one made a new number: alive)
    [[nodiscard]] size_t bytes() const;

    void finish(); // the grid, once the edges are in

private:
    static constexpr float cellSize = 64.f;
    za::Vector<za::Pair<uint64_t, int>> cells; // (cell, edge), sorted
    mutable za::Vector<uint32_t> seen;         // nearby(): an edge already listed this query
    mutable uint32_t stamp{0};
};

// A solid prop's ledges as it lies now (vr_climb_props; vr_climb.cpp, "Holds on props"), in world space: each edge of
// its convex pieces' walkable tops (normal z at least minTopNormal) where the face beyond it falls away (not walkable
// too), with one sample: where the drop starts (half a unit out, or as far as its own side flares out under the lip
// over minDrop; past edgeReach: no ledge) and the top's depth at its middle. Those whose box touches mins..maxs,
// appended to `out` (its grid not made: nearby() finds none of them). The checks when a hold is taken see the rest (the
// room over it, the drop, other props). Its edict number `num` per ledge appended to `nums`.
void propLedges(int num, const box3d::ClimbShape& shape, const box3d::ClimbBody& body, const glm::vec3& mins,
    const glm::vec3& maxs, Map& out, za::Vector<int>& nums);

// The heap memory a map holds (a scratch map's: vr_mem.hpp).
[[nodiscard]] inline za::SizeT heldBytes(const Map& m)
{
    return m.bytes() - sizeof(Map);
}

// The ledge map of a brush model (the world, one of its inline models, "*N", or a runtime brush's: vr_brushes.cpp);
// nullptr for any other model. Made the first time it's asked for (the world's and its inline models' when the map
// loads, with Climbing on), kept until the map changes (a runtime brush's: until it changes, forget).
[[nodiscard]] const Map* of(const qmodel_t* model);

// A runtime brush's model made again (vr_brushes.cpp): its map dropped (made again when asked for), and forgets() counts
// it (a Map* kept may be that one: alive says).
void forget(const qmodel_t* model);

// Counts the times all the maps were dropped (a new world model): a Map* kept from an older count is gone.
[[nodiscard]] int generation();
// Counts the times one map was dropped (forget); while it is unchanged, every Map* kept since is still good.
[[nodiscard]] int forgets();
// Whether `m`, made as `serial` (Map::serial), is still one of the maps (a Map* of an older forgets()).
[[nodiscard]] bool alive(const Map* m, za::U32 serial);

// Server: the map has loaded (VR_OnSpawnServerAfterLoad).
void afterLoad();

// vr_debug_ledges: the ledges near the player, drawn (the view's frame).
void debugDraw();

void init(); // registers vr_ledges

} // namespace qvr::ledges
