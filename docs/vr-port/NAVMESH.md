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

## 11. Round 2, part 4 (2026-11-11): crowds, smoothing, approach slots (all off by default)

- **Separation** (`vr_nav_separation`, a weight; `vr_nav_separation_range` 24): the yaw toward the next corner is bent
  away from walkers whose boxes are within the range (each pushes by its closeness), never turned back on the path, not
  on a link's corner. The walkers are gathered once a frame (`crowdNow`). Not DetourCrowd: our monsters step with
  Quake's walkmove at their own animation speeds, so a velocity planner's output would be thrown away; a steering term
  on the corridor is what fits.
- **Smoothing** (`vr_nav_smooth`, units): within that distance of a corner the monster already aims along the next
  segment (at most half of it), rounding the turn; `ChangeYaw` keeps limiting the turn rate as before. The mesh's
  clearance (1.41 x the half width) leaves room for that cut.
- **Approach slots** (`vr_nav_slots`, `vr_nav_slot_radius` 80, `vr_nav_slot_engage` 120): a walker with no missile
  attack (`th_missile` 0) farther than the engage distance paths to a slot on a ring round its foe, its own (none other
  chasing him holds it this half second), the free one nearest its bearing, kept while free; nearer it is Quake's chase.
  A slot straight ahead on the mesh is still walked to (round 1 handed that case to Quake's chase).
- **vr_nav 0** is untouched (navsteer returns -1 before any of this). Backpedal, sidestep, strafe, the armed humanoids'
  moves don't use movetogoal: unchanged.
- **Measured** (`vr_nav_crowdtest <s>`: every walker woken onto the player; a stall is a half second moved under 8
  units while over 128 from him; verdict when all came within 128) on scene `nav_crowd` (vrarena, 8 knights and 4 dogs,
  you on the dais), 40 s, no-LTO low-impact runs:

  | seed | all off (Quake-like steering on the mesh) | separation 0.8, smooth 24, slots 8 |
  |---|---|---|
  | 1 | 7 alive, 4 came, 160 stalls of 402 samples (40%) | 8 alive, 6 came, 173 of 433 (40%) |
  | 2 | 14 alive, 11 came, 190 of 480 (40%) | 10 alive, 7 came, 195 of 400 (49%) |

  Seed 2 with slots 0 gave exactly the "on" numbers, so the slots likely never took effect in the arena (its knights
  may have a missile function, or the arena's own AI drives the approach); separation 0 gave 191 of 457 (42%). The
  arena's own waves change the walker count between runs, so the test is noisy. No benefit shown, so the three stay off
  by default (the `nav_crowd` scene turns them on to look at). `vr_arena_test 36` with the defaults: 0 capped of 64.
  To do: a crowd scene without the arena's waves (a plain map), check which walkers count as melee, then tune. (Done in
  12: the 40% was two knights placed in walls.)

## 12. Crowd stalls diagnosed (2026-11-11)

**The test now says why.** `vr_nav_crowdtest <s> [near] [dump]` (Debug > Views: Crowd Stall Test) waits a frame at a
time till a scene's monsters have started (none still in `walkmonster_start_go`; it can be the first command after
`vr_scenario`), takes the walkers alive then as its fixed set (the arena's later spawns are left out of the counts),
wakes them onto the player and samples each half second. A sample whose monster is busy (an attack, pain, a leap, a
weapon pickup or pose, knocked down, in the air: by its frame function and state) is left out of the rate. Each stall is
given one cause from the monster's state and its steers in that half second (each steer's outcome is recorded:
`Agent::why`, and whether it moved since): `insolid` (its box in a wall), `idle` (standing, walking, the knight's
pause, an armed one's taunt), `infight` (after another monster), `nochase` (no steer that half second), `offmesh`,
`nopath`, `link`, `end-blocked` / `unreach` / `end-other` (handed to Quake's chase: blocked ahead, path short of him,
else), `monster` / `wall` / `ledge` (steered but did not move: a box trace a step ahead meets a monster, a wall, or
nothing: walkmove's ledge test), `slow` (steered and moved, under 8 units). The end prints `nav crowd causes:` rows by
monster type (samples, busy, stalls, then the causes in that order), the frames of the busy, idle and no-chase ones,
and whether each walker had a whole path to him at the start; each stall is logged (`navstall|type|cause|x|y|z`), and
Show Navmesh 2 draws a post at each (red a monster in the way, blue a wall, green a ledge, black in a wall, purple no
chase, grey the rest). `dump` (or `vr_nav_crowdtest dump` before a scene) logs the walkers' classes' polygons
(`navpoly|...`) for a top-down heatmap (the round's script: scratch `heatmap.py`).

**Why 40%: two of the twelve knights were placed in walls** (the scene's `450 450` and `-450 450` are on a 258-high
ledge's side: "placed in a wall"). Stuck there, each was a stall every half second for the whole 40 s (464-468 of the
470-491 stalls in 3 seeds), and the test never ended early. Also: the scene never started in the earlier runs (its
`cvar` lines named tunables, which are not cvars: "no setting"), and its crowd test ran before the map had loaded, so
part 4's numbers measured the arena's own waves. Fixed: the knights at `600 450` / `-600 450`; a scene's `cvar` line
takes a tuning constant too (set live, put back after; ignored in a release build); the scene runs the test itself.

**Why the slots never acted:** they took a walker with no `th_missile` as melee, and none in the scene was: the armed
knights (on by default, `vr_agknight`) have `VR_AG_Missile`, dogs their leap. Now the QC says (`vr_nav_melee`, set
before each `navsteer`: no missile attack, a dog, or an armed humanoid holding no ranged weapon).

**Measured** (nav_crowd, seeds 1-3, no-LTO low-impact runs; "came" the mean seconds till every walker was within 128
of him; the stall rate without busy samples):

| knights | setting | came | stalls | causes |
|---|---|---|---|---|
| armed (default) | before, all off | 40.4 (2 never) | 54% | insolid 468, monster 6, nochase 6, rest 11 |
| armed | all off | 12.7 | 2% | end-other 4, end-blocked 3, slow 3, monster 1, idle 1 |
| armed | separation 0.8 only | 12.1 | 3% | |
| armed | smoothing 24 only | 14.2 | 2% | |
| armed | slots 8 only | 7.9 | 1% | |
| armed | all three (the scene) | 7.1 | 0% | nochase 1 |
| Quake's (`vr_agknight 0`) | all off | 23.1 | 10% | infight 29, monster 25, slow 13, end-blocked 12 |
| Quake's | separation only / smoothing only | 11.6 / 11.0 | 3% / 1% | |
| Quake's | slots only | 7.7 | 0% | |
| Quake's | all three | 7.8 | 2% | |

With the stuck knights gone, the real stalls are a few percent and are crowding at the foot of the dais' flights
(heatmap: two clusters where the pack files onto one flight: `monster` and `end-blocked` together) and, with Quake's
knights, infighting (one cuts another in the press and they fight: the walker count falls to 4-5 in some runs, which
also makes the samples noisy). No `offmesh`, `nopath`, `unreach` or `link` stall in any run; every walker had a whole
path to him on its class's mesh at the start (the dais is reachable for each walker's size class). The slots are the
biggest gain (the pack takes several flights), the three together the fastest. `vr_arena_test 36` (seed 7): 16
passed, 0 failed with the defaults and with all three on; `vr_nav_cliptest` and `vr_nav_toptest` on e1m1 pass.
**Defaults left off** (the author's call: they change every melee walker's approach);
recommendation: turn on `vr_nav_slots 8` at least, all three for the best numbers.

## 13. The arena's towers (2026-11-11)

Reported: grunts never reached him on a tower's top (narrow, broken mesh on the stairs) and a grunt on the tower across
ran into its own top's corner instead of taking the stair. `vr_arena_test 36` passed: it only tests the dais, its grunts
at their fists. **`vr_arena_test 41`** (`map vrarena; god; vr_arena_test 41`): one foe at a time per
tower, up (him on the top, it on the floor past the tower's far side), down, across (top to the next tower's top);
an armed grunt with a shotgun, an enforcer with a rifle, a knight, an ogre, a dog; done when on his level within a blow
(a gun's holder within 400); 40 s cap; `towerpath: capped N of 60`.

Causes found (hypotheses that held):
- **The stair's top was cut off the tower.** A flight (64 wide, 16 steps of 16 x 16) climbs beside the tower's face;
  only its top two treads (y 460..492: 32 units, a drop beyond) are within a step of the top. Eroded by the clearance
  (12 x 1.41 = 17, rounded up to 5 voxels = 20 a side, plus Recast's ledge filter's voxel) the 32-wide join had nothing
  left: the top, its bridge and deck were an island for every class; a path from the floor ended at the stair's top
  (partial: handed to Quake's chase) and one from the top had no way down (256 drop: over the small classes' 192).
  The stairs themselves were 20 units of mesh for a grunt and none for an ogre (20 x 1.41 = 8 voxels a side).
- **Armed humanoids' range keeping** (`VR_AG_RangeMove`: gaining ground at a slant, strafing, holding a melee
  standoff) are flat moves straight at him that never consult the mesh: with him on the tower across and in range,
  the slant walked it into its own top's edge for good.
- A dog on a stair beside the tower leapt at him on the top into the tower's wall (its leap's check has no line test).

Fixes (tunables, Debug > Tunables):
- **Narrow band** (`vr_nav_narrow` 0.5, `vr_nav_narrow_cost` 3): Recast's erosion is run twice (`erodeWithBand`): the
  spans the clearance takes but `radius x vr_nav_narrow` keeps become their own area (`areaNarrow`, yellow in Show
  Navmesh), with their own regions and polygons at 3x the cost: paths keep to the clearance where there is room and use
  the band only where nothing else leads. The box still collides with the real walls. Drop links still start at the
  clearance's edge (a core polygon's edge facing the band counts as open; none from band polygons), as before.
  At 1.0 (the box's own half width) the join is a single voxel wide: knights and dogs stalled at the stairs' top
  (9 capped of 60), so 0.5. Known cost: the band admits gaps a little narrower than the box (16-24 units for a
  grunt); at 3x it is taken only when nothing else leads there. Meshes: e1m1 1,578 -> 2,924 polygons (1,707 band),
  277 -> 485 KB; e4m7 2,287-2,716 -> 4,072-4,470. `vr_nav_cliptest` and `vr_nav_toptest` pass on e1m1 and e4m7.
  `navVersion` 4.
- **Armed humanoids follow the path to another level** (`vr_nav_level` 64): its foe's feet more than that above or
  below its own and `navsteer` giving a path (not straight ahead on the mesh), the range keeping gives way to
  `VR_NAV_Step` (its speed slider applied); no path: as before. Shots still come first (`CheckAnyAttack`).
- **Dogs leap only on a clear line** (`vr_nav` on): `CheckDogJump` adds a box trace to him (world only).

Measured (`vr_arena_test 41`, seed 7, no-LTO low-impact runs; "there" the mean seconds, capped ones counted 40):

| case | before | band 0.5 + level 64 (default) | band only | band 1.0 |
|---|---|---|---|---|
| armed grunt up / down / across | 10.0 / 34.6 (3 capped) / 19.9 | 8.1 / 13.5 (1) / 16.2 | 5.7 / 4.2 / 12.1 | 5.6 / 5.0 / 13.1 |
| enforcer | 8.7 / 28.0 (2) / 23.1 (1) | 5.8 / 5.3 / 10.9 | 5.7 / 5.1 / 10.8 | 13.5 / 6.6 / 17.7 |
| knight | 26.7 (2) / 31.4 (3) / 21.5 (1) | 4.7 / 4.2 / 16.7 (1) | 5.6 / 4.3 / 8.9 | 18.3 (1) / 9.6 / 16.5 (1) |
| ogre | 40 (4) / 40 (4) / 39.4 (3) | 13.7 / 5.4 / 19.7 | 9.5 / 22.8 (2) / 34.1 (3) | 21.2 (1) / 5.4 / 29.5 (1) |
| dog | 15.1 (1) / 31.5 (3) / 13.9 | 25.5 (2) / 16.9 (1) / 15.5 (1) | 25.8 (2) / 6.5 / 22.1 (2) | 23.0 (2) / 15.4 (1) / 22.1 (2) |
| capped of 60 | 27 | 6 | 9 (old drop links) | 9 |

(The "band only" and "band 1.0" columns were run before the drop links' fix: the ogre's failures there were drops off
the band's edge beside a bridge.) `vr_arena_test 36` with the defaults: 16 passed, 0 failed; `nav_crowd` (seed 1):
12 of 12 within 6.9 s, 0 stalls.

**Open:** the 6 left are one place: a dog or knight on a south-east or north-west stair's step 12 against the tower's
wall (x +-428, y -+296, z 232), standing 30+ s (the crowd test calls it a `wall` stall); the same dog and a knight placed by a scene at the same
floor place came up in 5 s, so it depends on the approach; not found within this round. One armed grunt down the south-east tower stalled at its
stair's foot (242 -320 56). Scene `nav_tower` (you on the north-east top, walkers on the floor and on the tower
across).

## 14. Props as obstacles (2026-11-11)

`vr_nav_props` 1 (Debug > Tunables, "Navmesh: Props Block Paths"). A loose solid prop (Box3D's, SOLID_BBOX, not in a
hand: a crate, a barrel, an explosive box; `box3d::climbShape`, the climbing hands' own view of a prop) that has rested
`vr_nav_prop_rest` (1 s) is cut out of every class's mesh when it blocks a walker: higher than a step (`vr_nav_climb`:
a box under it is stepped over), at least `vr_nav_prop_size` (20) across its narrower side and `vr_nav_prop_mass` (10 kg)
heavy (a lighter one is pushed aside). The mark goes when it moves more than `vr_nav_prop_move` (6 units) or turns 10
degrees from where it was marked, is taken in a hand, broken or removed; it is marked again once it rests.

- **How: our own tile rebuild, not DetourTileCache.** The tile cache rebuilds from compressed heightfield layers
  (`RecastLayers.cpp`, left out of the import) and would need its own build path: our tiles fill brushes as solids,
  run the narrow band's double erosion and carry drop and lift links, none of which the tile cache's builder does. So a
  tile is built again from its own input (its triangles and brushes, binned the first time; `binTiles`, shared with the
  build) with each obstacle's outline marked unwalkable in the compact heightfield before the erosion (`markObstacles`:
  `rcMarkConvexPolyArea` on the spans whose floor is within the class's height under the prop's bottom and more than a
  step under its top), so the clearance keeps the box off a prop as off a wall, the narrow band included. The outline
  is the convex hull of the prop's shape where it lies (a barrel's round, a tipped crate's turned square), at most 16
  corners. A tile keeps its links (taken from the tile as it was).
- **Batches on the pool.** The props are looked at every `vr_nav_movers` tick (0.25 s) from each server frame
  (`nav::serverFrame`, whether or not anything chases); a mark made or gone flags the tiles it reaches (with the tiles'
  border) in each class. Each class runs one batch at a time (`jobs::async`, its tiles in `parallelFor`), no more
  often than `vr_nav_prop_batch` (0.5 s); the main thread only swaps the finished tiles in (`removeTile`/`addTile`). A
  tile with no obstacle left gets its first data back (kept when the first obstacle reached it), with no rebuild.
- **Paths:** a walker whose path crossed a changed tile (one of its polygons no longer valid) finds its path again at
  once; the doors' and lifts' blocked flags are set again at the next tick.
- **The disk cache stays valid:** the file is written from the build (no obstacles), and a class read or built later
  gets the marks there are as its first batch.
- **Show Navmesh** draws each blocking prop's outline in purple (its foot, its top, the corners joined), over the hole
  it leaves in the mesh. `vr_nav_info` counts them (blocking, marks, batches, tiles rebuilt and given back, the pool's
  and the main thread's time, what is pending). Builtin `navobstacles(what)` (0 pending: 0 when the mesh is current).
- **Measured** (`vr_arena_test 43`, scene `nav_props`; no-LTO, low-impact runs): a knight from the floor east of the dais
  comes up the east flight in 3.9 s; with 7 large crates (4 across, 3 on them) on the dais at the flight's top it takes
  the north flight (7.1 s); cleared, the east flight again (3.9 s); the pile set down as it starts out (marked 1.1-1.3 s
  later, its path found again): another flight (9.8 s); a barrel on its line: its middle never nearer than 32.6 units
  to the barrel's (touching is 23.5), no stall. 60 small crates set down at once west of the dais: marked in one batch
  of 6 tiles, 3.3-6.0 ms on the pool, 0.11-0.14 ms on the main thread; no rebuild in the 5 s they lie. A pile is marked
  1.1 s after it is set down. vrarena's own crates (11, vr_crates' layout) are marked too.
- Not done: a walker does not push a light prop out of its way on purpose (it walks round a blocking one, and a light
  one still meets Quake's chase and the props' detours, vr_ai_props.qc).

## 15. The towers' last stalls (2026-11-11)

Found with `vr_nav_stuck_log` (Debug > Tunables: a walker chasing on the mesh that stood still that long is logged
once, `navstuck|...`) and the tower test run one kind at a time (`vr_nav_test_kinds`, a bit mask; `vr_arena_test 41`
now also waits for the player before its first spawn: it had a program error when run straight after `map`).

- **The step beside the tower's wall: melee range through the stair.** The dog or knight stood on step 12-14 of a
  stair hugging the tower, him on the top 33-49 above: Quake's melee range is 120 in a straight line (`range()`), so it
  was in range from below. It bit or swung at air, and its attack's charge (`ai_charge`: Quake's `movetogoal`) ran it
  into the tower's wall; still in range, it attacked again, for good (no `navstuck` line: it never steered). The
  "depends on the approach" note of 13 was this: from where it came up the stair it was in range or not. Fix
  (`vr_nav_melee_level` 24, QC `VR_NAV_MeleeOff`, builtin `navreach`): a walker in melee range of a foe whose feet are
  more than that above or below its own doesn't attack when a whole path to him is on its mesh (`wholePath`): it walks
  the path. Applied where Quake decides a melee attack (fight.qc's CheckAttack, the shambler's and ogre's, the dog's and
  the fiend's). No path (a ledge it can't climb, a crate he stands on): as before. 24: a stair's step and the dais' pad
  (16) still count as his level. The leap rule of 13 stays (the dogs' slowness came from this, not from it).
- **Walls the step meets** (`vr_nav_slide` 1): a step toward the next corner whose box meets a wall (traced a step's
  height up, so a riser is not a wall) turns along the wall instead (Quake's walkmove never slides); the narrow band can
  put a corner nearer a wall than the box's half width.
- **The dog that stopped on the floor at -160 -400 going down** is dead: it leaves the south-west tower's top within
  1.3 s of its start and falls 256 to the floor (its box is then a corpse's, 31 wide), and monsters take fall damage
  beyond 150 (combat.qc `VR_Monster_Fall`; a dog has 25 health). Not a drop link: with them capped at 150 (below) it
  still happens. How it goes over the edge (its leap's height test forbids a leap down; a shove?) is not found: the
  one capped trial left. The links' comment said monsters take no fall damage: `vr_nav_drop_max` 192 -> 150 anyway, so a
  drop link never hurts a short walker (the tall classes keep 256: an ogre lives through 42).
- **Measured** (`vr_arena_test 41`, seed 7, no-LTO low-impact run, after the merge): **capped 1 of 60** (was 6). There
  (mean s) up / down / across: armed grunt 7.9 / 4.2 / 12.9, enforcer 5.7 / 4.8 / 10.6, knight 4.4 / 4.2 / 8.9, ogre
  7.8 / 5.4 / 19.4, dog 6.9 / 17.1 (the dead one counted 40) / 8.5. The tests now keep the player god-moded themselves
  (a `god` typed with the map's load can come before the player: he was shot dead and test 41 stood for an hour) and
  wait for him before their first spawn.
- **The armed grunt at the south-east stair's foot** (255 -324): logged steering south with a clear step yet not
  moving, in the earlier (fixed) run; not seen again in the runs below.
