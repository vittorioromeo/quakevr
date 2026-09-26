// vr_decals.hpp -- blood and scorch marks on the world (vr_decals): blood splats on the floor under
// and the walls beside what bleeds, scorch marks where things explode, chips where bullets and
// nails hit. Gibs and heads bleed too (vr_gib_blood, VR_GibTrail): a trail of drops on the floor
// under them, a splat where they hit a wall or the floor, and Quake VR's blood trail behind them.
//
// Each is a square (or oblong) footprint on the surface found by a trace (the static world only:
// none on doors or lifts), turned at random or along a given way, cut out of the world's faces under
// it (the faces facing about its way near its plane: across a wall's panels and recesses, cut where a
// face ends). They multiply what is under them by 0..2 (a modulating blend that can darken and
// brighten), so they take the light of where they are with no lighting of their own. The marks are
// drawn once at start-up into a mipmapped atlas (splats, drops, scorches, chips, and the gore's
// sprays, runs, pools and splotches; several of each). A chip is a dent: a crater, a rim and cracks,
// their relief baked in as lit from one side, the decal turned so that side faces the map's strongest
// light that reaches it (else up). Hipnotic's low-resolution bullet hole sprites become chips too
// (VR_BulletHoleSprite). The gore's marks (vr_gore.cpp) can show late, spread over seconds and darken
// as they dry. At most vr_decal_max are kept (the oldest go first; drops at most a quarter of them),
// each fading out at the end of vr_decal_life seconds. Drawn in each eye in the opaque pass
// (VR_DrawSceneOpaque), depth-tested: the marks that don't change from a buffer kept between frames,
// the others rebuilt each frame.

#pragma once

#include "vr_particles.hpp"

#include <glm/glm.hpp>

namespace qvr::decals
{

// A Quake VR particle effect at `org` along `dir` (may be zero).
void fromEffect(const glm::vec3& org, const glm::vec3& dir, particles::Preset preset, int count);

// A small drop of blood on the floor (or a gentle slope) just below `org`, `size` units across.
void drop(const glm::vec3& org, float size);

// A bullet's chip on the surface nearest `org` (Hipnotic's bullet hole sprites, VR_BulletHoleSprite).
void chip(const glm::vec3& org);

// The marks the gore places (vr_gore.cpp), besides the splats and drops above: a hit's spray
// (droplets flung along `along`), a run of blood down a wall (from its top along `along`, down), a
// pool, and a gib's big splat with spikes all round.
enum class Mark
{
    Splat,
    Drop,
    Splatter,
    Streak,
    Pool,
    Splotch
};

struct MarkOptions
{
    glm::vec3 along{0.f};   // the mark's length on the surface (projected on it); zero: turned at random
    float aspect = 1.f;     // its length along `along` over its width (a run down a wall: long)
    float delay = 0.f;      // seconds before it shows (the blood flying there)
    float grow = 0.f;       // seconds it takes to spread to its full size (0: at once),
    float growFrom = 1.f;   // from this part of it
    bool fromStart = false; // spreading from `where` along `along` (a run), not from its middle
    float darken = 0.f;     // how much darker it gets as it spreads and dries (0..0.9)
};

// Places a mark `size` units across on the surface at `where` (normal `normal`), shrunk where it
// would hang over an edge; false if none fits (or decals are off).
bool place(Mark mark, const glm::vec3& where, const glm::vec3& normal, float size, const MarkOptions& o = {});

// The static world along `from` -> `to` (as the decals see it): where, its normal, how far along.
[[nodiscard]] bool trace(const glm::vec3& from, const glm::vec3& to, glm::vec3& where, glm::vec3& normal, float& fraction);

// Drawn in each eye (VR_DrawSceneOpaque).
void draw();

// All gone (a new map).
void clear();

// vr_decal_count: how many there are, of each kind.
void count_f();

// vr_decal_atlas: writes the marks' atlas to <gamedir>/decal_atlas.png, as they look on a grey wall.
void atlas_f();

// Decals on the walls (vr_memstats).
[[nodiscard]] int liveCount();

} // namespace qvr::decals
