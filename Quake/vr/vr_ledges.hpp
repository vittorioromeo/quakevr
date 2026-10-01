// vr_ledges.hpp -- the ledge map: every ledge of each brush model (the world, and doors, plats, trains, walls in
// their own space), found once from the BSP, for climbing to look holds up in (vr_climb.cpp). See vr_ledges.cpp.

#pragma once

#include "vr_engine.hpp"

#include "Zancle/Container/Vector.hpp"
#include "vr_zancle.hpp"


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
    [[nodiscard]] size_t bytes() const;

    void finish(); // the grid, once the edges are in

private:
    static constexpr float cellSize = 64.f;
    za::Vector<qza::Pair<uint64_t, int>> cells; // (cell, edge), sorted
    mutable za::Vector<uint32_t> seen;         // nearby(): an edge already listed this query
    mutable uint32_t stamp{0};
};

// The ledge map of a brush model (the world, or one of its inline models, "*N"); nullptr for any other model. Made
// the first time it's asked for (the world's and its inline models' when the map loads, with Climbing on), kept until
// the map changes.
[[nodiscard]] const Map* of(const qmodel_t* model);

// Counts the times the maps were dropped (a new world model): a Map* kept from an older count is gone.
[[nodiscard]] int generation();

// Server: the map has loaded (VR_OnSpawnServerAfterLoad).
void afterLoad();

// vr_debug_ledges: the ledges near the player, drawn (the view's frame).
void debugDraw();

void init(); // registers vr_ledges

} // namespace qvr::ledges
