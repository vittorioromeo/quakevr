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
#include "vr_permdecals.hpp"

#include <glm/glm.hpp>

namespace qvr::decals
{

// A Quake VR particle effect at `org` along `dir` (may be zero).
void fromEffect(const glm::vec3& org, const glm::vec3& dir, particles::Preset preset, int count);

// The melee marks' event (vr_decal_melee), a particle2 preset past the gore's (vr_gore.hpp's 40-42): `org` 2 units out
// from the surface struck, `dir` (times 7) a blunt blow's way into it, or the line a blade cut along; `count` the blow's
// damage x 4 + 1000 x its kind (0 blunt, 1 a slash, 2 a stab, 3 an axe's chop). QC: VR_Melee_WallMark (vr_melee.qc).
inline constexpr int EventMeleeMark = 43;

// A particle2 message (parseParticle2): true if it was the melee marks' event (its mark laid, or none).
bool meleeEvent(const glm::vec3& org, const glm::vec3& dir, int preset, int count);

// Server side: a prop (Box3D's, vr_box3d_step.cpp) of `mass` kg hit the static world at `at` (normal `normal`, out of
// it) at `speed` m/s: at vr_decal_melee_prop_speed or faster, a blunt dent's event sent to every client.
void propHit(const glm::vec3& at, const glm::vec3& normal, float speed, float mass);

// A small drop of blood on the floor (or a gentle slope) just below `org`, `size` units across.
void drop(const glm::vec3& org, float size);

// A bullet's chip on the surface nearest `org` (Hipnotic's bullet hole sprites, VR_BulletHoleSprite).
void chip(const glm::vec3& org);

// A ragdoll's limb's end at `at` this frame (vr_ragdoll.cpp; `key`: its entity's and the limb's): flung fast, a blood
// trail and drops as a gib's (vr_ragdoll_blood).
void limbTrail(int key, const glm::vec3& at);

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
    Splotch,
    Footprint, // a bloody boot print (vr_bloodprints.cpp): `along` its toes' way, `aspect` its length over its width,
    Handprint  // a bloody hand print: `along` its fingers' way; `variant` 0 strong .. the most worn, `mirror` the other hand
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
    int variant = -1;       // which of its kind's cells (-1: one at random; clamped to its count)
    bool mirror = false;    // mirrored across `along` (a print of the other foot or hand)
};

// How many cells `mark` has (its variants: the prints' strengths).
[[nodiscard]] int variants(Mark mark);

// Places a mark `size` units across on the surface at `where` (normal `normal`), shrunk where it
// would hang over an edge; false if none fits (or decals are off).
bool place(Mark mark, const glm::vec3& where, const glm::vec3& normal, float size, const MarkOptions& o = {});

// The static world along `from` -> `to` (as the decals see it): where, its normal, how far along.
[[nodiscard]] bool trace(const glm::vec3& from, const glm::vec3& to, glm::vec3& where, glm::vec3& normal, float& fraction);

// Start-up (VR_Init): the atlas's texels begun on a worker thread (0.4 s of the CPU: made at the first mark's draw,
// it was the first shot's hitch).
void init();

// A map's load (VR_NewMap): the atlas on the GPU, so that the first mark draws at once.
void prepare();

// Drawn in each eye (VR_DrawSceneOpaque).
void draw();

// All gone (a new map).
void clear();
// Those laid on runtime brush `id` gone (it changed or was removed: vr_brushes.cpp, the marks on its faces; the world's
// beside it stay); a mark on several brushes, if its middle is in lo..hi (the brush's box).
void removeOn(int id, const glm::vec3& lo, const glm::vec3& hi);
// How many there are on runtime brush `id`, as removeOn finds them (vr_brush_list: the marks on a brush).
[[nodiscard]] int countOn(int id, const glm::vec3& lo, const glm::vec3& hi);

// vr_decal_count: how many there are, of each kind.
void count_f();

// vr_decal_stress [count 1..64] [size]: deterministic floor splatters ahead, for profiling.
void stress_f();

// vr_decal_carve_test [lattice 1..64]: carved marks on the surface the view's middle looks at, as each weapon makes them
// (vr_decal_carve): pellets, nails, super nails, lava nails and a blast's crater; with a count, that many in a lattice of
// pellets, nails and super nails 12 units apart, continued by the next call (for profiling). vr_decal_carve_test look
// head|graze [dist]: the view moved to look at the last ones laid, head-on or at 15 degrees over their surface.
// vr_decal_carve_test melee [lattice 1..64]: the melee marks (vr_decal_melee): blunt dents of 10, 20 and 40 damage,
// slashes across, slanting and up, a chop and a stab; with a count, a lattice of dents and slashes 20 units apart;
// melee blunt: the three dents in a row; melee slash: one slash (close-ups).
void carveTest_f();

// vr_decal_at x y z dx dy dz [blood|hole|scorch|pool] [size]: a mark where the line from x y z along dx dy dz (512
// units) meets the world or a brush model, as a hit lays it (tests: the same place each run).
void at_f();

// vr_decal_atlas: writes the marks' atlas to <gamedir>/decal_atlas.png, as they look on a grey wall.
void atlas_f();

// Decals on the walls (vr_memstats).
[[nodiscard]] int liveCount();

// A save: the live marks on the static world not among the permanent layer's stamps (as they settle), into `out`.
void liveStamps(za::Vector<permdecals::Stamp>& out);

} // namespace qvr::decals
