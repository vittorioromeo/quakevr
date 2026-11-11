# Navmesh pathfinding for walking monsters (Recast/Detour)

Decision log and design of `Quake/vr/vr_nav.cpp` (brief 2026-11-10: arena enemies stuck in the tower stairs and the
centerpiece's corners; Quake's `movetogoal` only heads at the goal, tries diagonals and random directions when
blocked, and never steps off ledges).

## 1. Does Recast/Detour suit us? Yes (2026-11-10)

Checked https://github.com/recastnavigation/recastnavigation (README, releases):

- License: zlib (`License.txt`): fine for our GPL engine and shipped binaries; the notice stays with the source.
- Plain C++98, no dependencies, any compiler: clang-cl on Windows and gcc/clang on Linux alike.
- What we use: Recast (building) and Detour (runtime mesh, `findPath`, `findStraightPath`, polygon flags and areas).
  Later maybe: DetourCrowd (local avoidance), DetourTileCache (dynamic obstacles). Unused: RecastDemo, Tests,
  DebugUtils (our debug view draws with `vr_lines`).
- Maintained: active (1,184 commits, CI, open PRs). Latest release **v1.6.0** (commit `6dc1667`).

## 2. Import

- Source: GitHub's tag archive for v1.6.0 (1.3 MB, SHA-256
  `8B50C62177249554B226514B307E6C529934A62B3926822777BF6ABB8906A155`; downloaded by the coordinator with the author's
  approval: the worker rules forbid downloads), vendored under `Quake/vr/external/recastnavigation`: only `Recast/`
  and `Detour/` (Include, Source; without `RecastLayers.cpp`, the tile cache's) and `License.txt`; its `README.md`
  records the version and hash. Upstream's files unchanged.
- Built from source as its own static library, always optimised (`/O2`, `NDEBUG`) in every configuration, Debug too;
  only the runtime library follows the configuration (Debug's `/MDd`, as Zancle's project does, so the objects link
  with the engine's); ThinLTO in ReleaseLTO / Release-Tracy / Release-TracyFull like the engine; warnings off for the
  vendored code only; clang-cl.
  - `Windows/VisualStudio/recast.vcxproj`, in `ironwail.sln`, referenced from `quakevr.props`: every configuration,
    the kit's `build.sh` (builds the solution) and `make_release.ps1` (builds ReleaseLTO) get it.
  - `Quake/vr/vr.cmake`: `qvr_recast` (CMake: the Linux and flatpak builds); `Quake/vr/vr.mk`: the Makefiles.
  - Nothing to package (linked into the exe). Credits: `CREDITS.md`.
- Allocations: Recast's and Detour's default allocator is `malloc`, which in this executable is already mimalloc (the C
  heap override, `vr_crtheap.c`), so `rcAllocSetCustom` / `dtAllocSetCustom` are not used. The build allocates per tile
  on the pool threads (mimalloc's thread-local heaps); steering allocates nothing once a monster's path vector grew.

## 3. Building at map load

- **Input geometry: vr_hull's convex brushes** (coordinator's correction, 2026-11-10). `hull::navSolids` hands out the
  brushes `vr_hull.cpp` already rebuilt from hull 0's solid leaves, plus the world's clip brushes it recovered from the
  clipping hulls: no second reconstruction. Each brush's faces become triangles (`brushes::forEachPolygon`, the
  runtime brushes' plane clipper) on the pool; a face is walkable when its normal's z is at least cos(`vr_nav_slope`)
  (45.57 degrees: Quake's 0.7 floor rule).
- **Solids filled.** Rasterising only a closed brush's faces leaves its inside open to Recast: a floor under a func_wall
  or inside a clip brush stayed walkable (e1m1: 6,634 polygons, 3 of them inside clip brushes). Each brush is also
  filled column by column (`fillSolids`: the column through the brush at each cell's middle as one solid span, merged
  with its faces' spans so a walkable top keeps its area): e1m1 1,656 polygons, none inside a clip brush.
- **Clip brushes** are real solids in that input, so they block paths (`vr_nav_cliptest`, below). Known approximation:
  a recovered clip brush's floor and top faces sit at its box centres' height for Quake's 32 box (`hull::navSolids`),
  so a mesh on top of a clip brush is a little off its true height; clip brushes are nearly always walls: left.
- **Brush entities:** still ones (func_wall, func_episodegate, breakables: SOLID_BSP and not a mover) are solid where
  they stand at load (turned ones are left out); func_illusionary and triggers (not SOLID_BSP) don't block. Movers
  (func_door*, secret doors, func_plat, func_train, buttons, rotators, bobbers: by classname prefix) are left out of
  the mesh: see 5.
- **Size classes.** Monsters move with `vr_mhull` widths (their class's width at Quake's height 56 or 88:
  `hull::moveSize`), so one navmesh per (radius, height) class present: the half width rounded up to
  `vr_nav_radius_step` (4). id maps: 12 x 56 (grunt, knight, dog, zombie...), 16 x 56 (enforcer's 28 rounded up),
  20 x 88 (ogre), 32 x 88 (shambler, vore). AD's monsters get their size from `moveSize` (their box when vr_hull's table
  has no class for them). With `vr_mhull 0` `moveSize` gives Quake's hulls: 16 x 56 and 32 x 88. The classes of the
  walkers spawned with the map are built at load; a size first met in play (the arena's waves, a triggered spawn) is
  built on the pool then (`vr_nav_async` 1; that monster chases as Quake does until it is ready).
- **Recast settings** (tunables, Debug > Tunables): voxel 4 x 2 units (`vr_nav_cell`, `vr_nav_cell_height`), climb 18
  (`vr_nav_climb`), slope 45.57, the class's height, erosion its radius times `vr_nav_clearance` (1.41: a square box's
  corner reaches that far at 45 degrees, so a path's corner never puts a box corner over a ledge, which
  `SV_CheckBottom` refuses; at 1.0 monsters stalled at the stairs' sides). Watershed regions, 6-vertex polygons,
  detail mesh.
- **Tiles on the pool:** `vr_nav_tile` (64) voxels a side, each triangle and brush binned into the tiles its bounds
  reach, the tiles built with `jobs::parallelFor` (site "nav tiles"), each class its own `jobs::async` job. Detour refs
  keep 22 bits for tile and polygon: a vast map doubles the tile size until its tiles fit in 12 bits.
- **Areas:** each polygon's by the contents over its middle: ground, water, slime, lava, with costs a unit
  (`vr_nav_water_cost` 2, `vr_nav_slime_cost` 20, `vr_nav_lava_cost` 60): walkers avoid slime and lava unless there is no
  other way. Sky is solid (vr_hull's rule).

## 4. Disk cache

`cache/navmesh` (`diskcache::Product::NavMesh`, `vr_nav_cache` 1; 0 off, 2 built anyway and compared). Key: a hash of
the triangles (which hold the still brush entities and clip brushes), the settings and the class; the build folder
from `navVersion` and Detour's `DT_NAVMESH_VERSION`. Written atomically (`diskcache::write`); read back through the
framed file's checks (magic, layout version, length), our checksum of the payload (FNV-1a, the header's second number)
and a bounds-checked parse: anything amiss counts as rejected and the mesh is built again.

## 5. Doors, lifts, trains

- **Doors** (secret doors too) are not in the mesh; the floor under them is. Every `vr_nav_movers` (0.25) s the
  polygons under each mover's current box are flagged blocked (excluded by the query filter) while it stands between
  the floor's step and a monster's head: a closed door blocks its doorway, an open one (raised, slid aside) doesn't.
  Monsters can't open doors in Quake, so no route through a closed door is planned; once it opens, the next repath
  goes through.
- **Lifts (func_plat) and trains are not ridden yet.** Quake's plats only rise for players (`plat_center_touch`), so
  riding needs QC changes besides navigation. Today a plat's body blocks the floor it stands on and the mesh has no
  polygons on the plat, so no path uses a lift. How it should work: an off-mesh connection per plat from beside its
  bottom position to beside its top (`pos1`/`pos2`), with a "lift" area; a monster whose next corner is such a link
  walks on when the plat is at its end (else waits beside it), the plat's touch accepts it, it stands still while
  carried and walks off at the far end. Trains the same, only where a train stops.
- **Drops** are not in the mesh (walkers never step off ledges, `SV_movestep`): see 7.

## 6. The chase

QC `VR_AI_MoveToGoal` (ai_run's chase; ai_walk and the stealth walks pass too, but only a walker whose goal is its enemy
goes on) first tries `VR_NAV_Step`: builtin `navsteer(self, goal)` (by name: vr_builtins_nav.inc) gives the yaw toward
the next corner of the monster's path; the monster turns (`ChangeYaw`) and steps with Quake's own `walkmove`
(collision, steps, ledges, animation unchanged); a blocked step tries `vr_nav_sidestep` (35) degrees to either side, then
falls through. `navsteer` returns -1 and Quake's chase runs exactly as before (then the arena's `info_arena_nav` stairs
helper and the prop detours) when: `vr_nav 0`, the monster flies or swims, no navmesh for its size yet, it or its goal
is off the mesh, there is no path, or the goal is straight ahead on the mesh (the last corner is the goal: Quake's
chase keeps its melee ranges and dodges).

Engine side: `findNearestPoly` for both feet, `findPath` (256 polygons; a partial path toward an unreachable goal), the
path kept per monster and found again when its polygon is not on it, every `vr_nav_repath` (0.6) s staggered by entity
number (0.75x to 1.25x), or when the goal moved `vr_nav_repath_move` (64) from where the path led; `findStraightPath`
along the rest of the path for the corners; a corner within `vr_nav_corner_reached` (8) counts as reached.
Backpedalling, sidestepping, strafing, melee, retreat and the armed humanoids' own moves don't use `movetogoal`: same.

## 7. Shortcomings and how to do better

- **Crowds:** each monster plans alone; a pack takes one line through a door and bumps (the side step and Quake's chase
  sort it out). DetourCrowd's local avoidance, or velocity obstacles along each path, would spread them.
- **Flanking:** everyone takes the shortest path; goal slots round the player (a ring of points, or extra cost on
  polygons already on another monster's path) would make them surround him and come from several sides.
- **Jumping and drops:** off-mesh links for survivable drops (a max height, found by sampling the mesh's boundary
  edges and tracing down) and for leaps (fiends, dogs), followed with a velocity instead of walkmove: the one place
  that lifts Quake's never-step-off-a-ledge rule.
- **Lifts and trains:** off-mesh links and the QC in 5.
- **Dynamic obstacles:** props, gibs, ragdolls and other monsters are not in the mesh; the side step and the fallback
  handle them locally. DetourTileCache obstacles (or a tile rebuilt on the pool) would let big props reroute paths.
- **Smoothing:** corners are polygon vertices, so a path hugs the inside of a turn at the clearance; steering toward a
  point a little past the corner along the next segment would round turns and look more natural.
- **Flyers and swimmers** keep Quake's movement: a 3D grid of open space could give scrags and fish the same.
- **Doors** are walls while closed; a monster could wait at one it saw the player open.

## 8. Debug view and diagnostics

- `vr_nav_debug 1` (Debug page: Show Navmesh): the navmesh round the view within `vr_nav_debug_range` (1200): polygons
  by area (ground teal, water blue, slime green, lava orange, blocked by a mover red), edges darker, 2 units up,
  depth tested (`lines::triangle`, `lines::sceneLine`), in VR and flat (the view's debug draws, vr_view.cpp). `2` adds
  each monster's path (yellow, 6 units up) and its next corner (a white post). `vr_nav_debug_class`: which class (-1
  the nearest walker's).
- `vr_nav_info`: the input (brushes, clip brushes, triangles, movers), each class's mesh (tiles, polygons, KB, built or
  read, ms), the steering counts. `vr_nav_cliptest`: no polygon inside a clip brush (verdict line). `vr_nav_rebuild`.
- Checklist scenario `nav_arena`: walkers on the arena floor, you on the dais, paths drawn.

## 9. Measurements (2026-11-10, no-LTO build, low-impact runs: indicative)

| map | input | classes | build (each class, on the pool) | cache read |
|---|---|---|---|---|
| e1m1 | 1,238 brushes (30 clip), 15,017 triangles, gathered 1.1 ms | 12 x 56 | 53 ms (135 tiles, 1,592 polygons, 275 KB) | 0.6 ms |
| e4m7 | 1,495 brushes (68 clip), 17,897 triangles, 1.3 ms | 16 x 56, 32 x 88, 12 x 56 | 320, 424, 520 ms (built together; 324 tiles each) | 0.7-0.8 ms each |
| vrarena | 535 brushes (70 clip), 6,276 triangles, 0.8 ms | none at load; 12 x 56 and 20 x 88 in play | 20 x 88: 30 ms | - |

No AD map in the test bases (ad_grendel: "Couldn't spawn server"), so the biggest-AD-map timing is still to do. Cache:
a second load read every class (hits), a changed setting (`tune vr_nav_cell 5`) missed and built again; 0 rejected.
`vr_nav_cliptest` on e1m1: 30 clip brushes, 1,592 polygons, none inside: passes.

Arena stairs (`vr_arena_test 36`: one foe at a time from 8 floor places up to the dais' top and 8 down, a 25 s cap;
"there" is the mean seconds to get within a blow of him, capped trials count 25):

| vr_nav / vr_arena_nav | grunt up / down | knight | ogre | dog | capped of 64 |
|---|---|---|---|---|---|
| 0 / 0 (Quake's chase) | 14.9 / 23.4 | 13.3 / 21.3 | 20.5 / 23.8 | 13.8 / 17.3 | 29 |
| 0 / 1 (the old default) | 6.2 / 9.4 | 4.2 / 6.3 | 9.9 / 14.7 | 2.4 / 3.2 | 2 |
| 1 / 0 (navmesh alone) | 11.7 / 7.7 | 8.4 / 5.4 | 15.1 / 13.7 | 9.8 / 4.0 | 2 |
| 1 / 1 (the new default) | 5.7 / 6.0 | 4.0 / 4.7 | 11.5 / 9.5 | 2.2 / 2.4 | 0 |

(With `vr_arena_nav 0` the stealth AI's lost-target search also runs on the arena, which slows the navmesh-alone row:
`VR_Stealth_Lose` is skipped on maps with arena nav points.)

## 10. Round 2 (2026-11-11): fixes, drop links, lift links

- **Clip tops.** Why they stood high: `recoverClips` keeps a clip brush's faces that lie against more of hull 1's solid
  (not "open") in the centre space of Quake's box (28 over the real face; 56 for a face that moves with the box's
  height), and `hull::navSolids` handed those planes on as a point's. Fixed in `navSolids`: a floor-like (normal z 0.7+)
  non-open face is moved to the brush's own face (the open faces' rule), unless the point just over it is inside another
  clip brush (a tall clip brush cut in two by hull 1's tree: the cut is no real top). e1m1 1,592 -> 1,578 polygons
  (12 x 56), e4m7 unchanged. `vr_nav_toptest` (verdict line): every polygon over a clip brush's or a brush entity's top
  (where Quake's box stops higher than a point, or on an entity) is within the climb of where the box stands: e1m1 80,
  e4m7 262, e2m1 351 such polygons, worst 14 units, pass; func_wall tops come from hull 0's brushes and were right.
- **Debug view** draws in flat-screen play and in VR alike (same `nav::debugDraw` from the view's debug draws).
- **Arcane Dimensions, ad_tears** (no-LTO build, low-impact run): 68,237 brushes (2,223 clip, 440 of brush entities),
  1,150,037 triangles gathered in 72-89 ms (main thread, at the load); 16 x 56 built in 3.5 s and 12 x 56 in 1.8 s on
  the pool (1,122 tiles, ~20,000 polygons, 3.7-3.9 MB each); read back from the cache in 4.6 and 5.7 ms. The join on
  the main thread was 12-23 ms (the file's data, the mesh's tiles): now the job makes the mesh, its query and the file's
  data, the join only hands the write over (0.1 ms). AD's monsters get classes from `moveSize` (dknight 16 x 56, knight
  12 x 56); a walker spawned in play (ad_tears' hogremac) used to wait for its first chase: every `vr_nav_movers` tick
  now starts the classes of the walkers present, so they build before they chase.
- **Drop links** (`vr_nav_drop` 1; `vr_nav_drop_max` 192, `vr_nav_drop_max_big` 256 for the 88-tall classes;
  `vr_nav_drop_cost` 3 per unit of the link's length). Found at the build on the mesh without links: the middle of each
  mesh edge with no neighbour; out past it by the clearance plus the box's reach at any angle; the box (the class's
  width and height) must walk off unhindered (`sweepClear` over the solids: the same brushes the mesh is made of) and
  fall clear to the first floor polygon right under that place, more than a step down and at most the highest drop, not
  slime or lava. Links closer than 32 units at both ends are one. Detour off-mesh connections (one way), so each tile
  with links is made again with them; they are in the cached file (`navVersion` 2; the drop and lift settings and the
  rides are in the key). e1m1 26 drops (12 x 56), e1m2 32-69, e2m2 8-48 by class.
  Following one: at its start the monster commits (`followLink`); `navlink` 1 asks the QC to walk toward the far end,
  and where `walkmove` stops at the edge the QC sets `FL_PARTIALGROUND` (Quake's own "floor pulled out" rule: the step
  may hang over the edge, and once wholly off it falls with Quake's physics), pushing it on at `vr_nav_drop_push` (100)
  as it goes over. In the air it stands (`navlink` 2); on landing its sideways speed is cleared and it finds its path
  again. Not over the edge within `vr_nav_drop_time` (3 s): given up, no links for `vr_nav_link_retry` (5 s).
- **Lift links** (`vr_nav_lifts` 1, `vr_nav_lift_cost` 2): rides gathered at the load from the QC's fields (Quake's
  spawn functions name them "plat" and "train"): a plat between `pos2` and `pos1` (both ways); a train from each corner
  it waits at (`wait` > 0) to the next such (one way). Each stop's boarding places: beside each side of the mover's
  footprint where the mesh comes within reach at the mover's top's height and the way on is clear; a link from each at
  one stop to each at the other (Detour joins a link's far end only to its start tile's neighbours: a longer train leg
  gets none). e1m1 2 plats, 4-6 links. Following: at the start the monster stands (`navlink` 2) till the mover is
  stopped at that stop (else gives up after `vr_nav_lift_wait`, 10 s), walks to its middle, stands while it rides (the
  pusher carries it), and when the mover stops at the far stop walks to the link's end and finds its path again.
  QC `plat_center_touch`: a monster calls the lift as a player does (`vr_nav_plat_monsters` 1, and `vr_nav` 1; players
  unchanged). A lift waiting at the top comes down by itself (3 s after its last touch); one at the bottom is only
  called from it, so a monster at the top waits for someone below or gives up.
- **Tests**: `vr_nav_reachtest <name> <distance> <seconds> [drop|ride] [classname]` ends a scene on its verdict line (the
  monster within the distance of the player after a link of that kind was taken; `nav: <t> s: ...` logs the links'
  events meanwhile; with a classname, those monsters are woken onto the player). `nav_drop` (e1m1, a knight on a ledge
  144 over you): passes (seeds 1-5, and 4 again after the merge; 6-31 s), but the scene says its knight is "placed in a
  wall" and the player does not stay at the scene's start in the headless runs (200-500 units away at the verdict): the
  spots are still to fix. Open: in two seeded runs the knight's first drop failed (the `FL_PARTIALGROUND` step did not
  go over; it slid along the ledge under Quake's chase and gave up after `vr_nav_drop_time`); the retry 5 s later
  dropped. Cause not found. `vr_nav_links [class]` lists a class's rides and links. Debug view: drops orange, rides
  magenta.
- **Lifts off by default** (`vr_nav_lifts 0`, `vr_nav_plat_monsters 0`; the `nav_lift` scene turns both on). Seen once
  end to end on e1m1's plat at -544 2656 (seed 4): the knight waited, boarded at the bottom (7.4 s), rode up as the lift
  rose (7.9 s), stepped off at the top (8.7 s), ride done (9.0 s). The scene's check (the knight within reach of the
  player at the top) never passed: the player does not stay at the top in the headless run (found below, -584 2784
  -104), and the knight then waits at the top for the lift (a lift at the bottom is only called from it) and gives up.
  Before lifts go on: a scene whose player stays put, its reach verdict, the waiting-at-the-top case.
