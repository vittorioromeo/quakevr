# A smaller player hitbox on unmodified maps

Your notes `vrclimb_2026-09-29_20-06-59` and `20-07-38`: Quake's player box is 32x32x56 units, so you stop 16 units
(0.6 m at Quake VR's scale) from every wall and ledge. You asked how to make it smaller on the maps as they are (no
recompiling), with Quake's collision speed, smoothness and accuracy.

This page lists the ways to do it, with their pros and cons, what I recommend, the prototype that is in the build
(off by default), and the numbers.

## Why Quake can't just use a smaller box

Quake never sweeps a box against the map. The map compiler (qbsp) builds three BSP trees for every map:

- **Hull 0**: the drawing tree. It holds the real walls, floors, water and sky. Traces through it are for points.
- **Hull 1**: every solid brush grown by a 32x32x56 box, then compiled into its own tree ("clipnodes").
- **Hull 2**: the same, grown by a 64x64x88 box (shamblers and other big monsters).

To move a box, the engine traces a single *point* (the box's corner-aligned origin) through the hull grown by that
box (`SV_HullForEntity`: width under 3 uses hull 0, up to 32 uses hull 1, larger uses hull 2). That is why collision
is so fast and so exact: a trace is one walk down a small tree. But it also means there are only those sizes. A 20-wide
box still uses hull 1.

Two more details matter for anything that replaces hull 1:

- **Clip brushes** (the `clip` texture) are in hulls 1 and 2 only. Hull 0 knows nothing about them. id's maps use them
  (below, hull 1 blocks moves that nothing in hull 0 blocks on 31 of the 32 maps). They smooth out stairs and trim,
  and block ledges.
- **Bevels.** Growing a brush by a box is only correct at slanted edges and corners if extra "bevel" planes are added
  (Quake 2's compiler adds them). Without them a box snags on invisible extensions of the brush's faces.

## The candidates

For each: correctness (sealed corners, bevels, slopes, moving brush models), load-time cost, per-trace cost, memory,
compatibility (saves, demos, network), and risk to QuakeC and monster movement.

### A. Build a new clipping hull at map load

Rebuild the solid space of hull 0 as convex pieces, grow each by the new box (with bevels), and run qbsp's hull
compiler on the result at load time, into a new set of clipnodes (the BSP format even has an unused fourth hull slot,
`headnode[3]`).

- Correctness: as good as qbsp's own hull 1 (qbsp does exactly this). Sealed, because the input pieces tile the solid
  space exactly. Slopes and bevels as in qbsp. Moving brush models: each submodel's tree is built the same way.
  Clip brushes are missing (not in hull 0), so they need recovering from hull 1 (see B).
- Load time: the expensive part. It is a CSG and BSP build (qbsp's `brush.c`, `csg4.c`, `solidbsp.c`, `merge.c`,
  about 4-6k lines to port) on thousands of pieces: I estimate 0.1-1 s a map on a modern CPU, per width. Changing the
  width means building again.
- Per trace: identical to Quake's (the same `SV_RecursiveHullCheck`).
- Memory: one more clipnode tree per width (hull 1 is some 20-40 KB on id's maps).
- Compatibility: nothing changes outside the engine's collision; saves and demos unaffected.
- Risk: the CSG's own numerical problems (qbsp's hulls have sticky spots and leaks of their own), and a large port. Its
  input is exactly what B builds, so B is the first half of A.

### B. Box-against-brush collision, the brushes rebuilt from hull 0 (Quake 2 / Quake 3 style) — recommended

Walk hull 0's tree once at load, cutting a big box by each node's plane on the way down. What is left at each solid
(or sky) leaf is a convex brush. Add Quake 2's bevels (axial planes at its bounds, and planes through each slanted edge
that have the whole brush behind them). Then sweep any box against the brushes the way Quake 2's `CM_BoxTrace` does:
walk hull 0's tree with the box's extent, and clip the box against the brush of each solid leaf it reaches.

- Correctness: exact for any box. The solid leaves of a BSP tree tile the solid space with no gaps or overlaps, so
  corners are sealed; with the bevels, each brush is met exactly where the box meets it (Minkowski sum). Slopes: a
  brush face at any angle is a plane like any other; ground detection (`plane.normal[2] > 0.7`) works as before.
  Moving brush models: each submodel's brushes are built from its own head node; the sweep is done in its space, as
  Quake does (Quake doesn't rotate them either). Clip brushes: recovered from hull 1 (below). Quake's trace rules are
  kept: a box starting exactly on a surface is outside, moves stop 1/32 unit short, `allsolid` keeps fraction 1.
  One Quake 2 detail had to change (Quake 3 changed it too): a box starting less than 1/32 off a face and moving into
  it got a large negative fraction, missed the brush and ended up inside it by a hair; the random walk found it on a
  sloped wall in e4m3 (stuck for 11 frames). The fraction is clamped at 0 now.
- Load time: 2-13 ms for the brushes on id's maps, plus 6-86 ms for recovering clip brushes (mean 46 ms in all).
- Per trace: 1.3-1.4 times hull 1 (about 90 ns more: 330 ns against 238 ns on average, 32 maps). A player's frame does
  a few dozen traces, so a few microseconds a frame.
- Memory: 100-400 KB a map (planes, brushes, leaf tables).
- Compatibility: nothing is saved or sent. A save stores the same player box; demos record the results; clients
  don't predict in Quake, so network play follows the server's setting. One catch: a player standing where only the
  narrow box fits, when the setting is turned off (or the save is loaded without it), starts stuck; Quake's
  `SV_CheckStuck` nudges them out.
- Risk: floating-point seams between neighbouring brushes (Quake 2 and 3 live with the same, and the 1/32 back-off
  handles it); the clip brush recovery is a heuristic (below). QuakeC sees the same traces (only the player's world
  clipping is narrower); monsters are untouched until we choose to use it for them.

### C. Point or capsule traces against hull 0, plus checks

Trace the box's centre (or several parallel lines at its corners and edges) through hull 0, or sweep a capsule.

- Correctness: poor. Lines miss anything thinner than their spacing (pillars, railings, ledge corners), so the box
  clips into geometry between them, and stepping and ground detection on ledges become unreliable. A capsule needs its
  own sweep against the map's planes, which needs the same expanded planes and bevels as B, so it is B with a rounder
  shape.
- Load time: none. Per trace: several hull-0 traces (hull 0's tree is about as big as hull 1's, so each costs about
  as much as a hull 1 trace).
- Not recommended.

### D. Fixed intermediate sizes built from the existing hulls

Only point, 32 and 64 exist exactly. A 20-wide hull can't be made by combining hull 0's and hull 1's results: neither
their union nor their intersection is the map grown by a 20 box. The one real option is to shrink hull 1: cut it into
its convex leaves (as in B) and move each leaf's faces inward by 6 units. That is only exact for a leaf that is a
whole grown brush. Where hull 1's leaves meet at an inside corner, shrinking each leaf leaves a strip between them
that is solid in truth: the box would sink into the wall at every inside corner (a fall-through risk on floors). I use
this shrinking only for clip brushes (below), where a small error is harmless. Hexen II and Half-Life have more
intermediate hulls, but only because their compilers build them into their own maps.

### E. What other engines and ports do

- **Quake 2 and Quake 3**: the compiler stores the brushes (with bevels) in the BSP, and the engine sweeps any box
  against them (`CM_BoxTrace`). Quake 3 can also sweep capsules. B is this, with the brushes rebuilt from hull 0.
- **Half-Life (GoldSrc)**: four precompiled hulls (point, standing 32x32x72, large 64x64x64, crouching 32x32x36).
  Same approach as Quake, more sizes, all compiled in.
- **Hexen II**: its BSP has room for 8 hulls and its compiler builds extra sizes (a crouch hull among them).
- **FTEQW**: reads a BSPX `BRUSHLIST` lump (ericw-tools' qbsp writes it with `-wrbrushes`) and then sweeps any box
  against brushes, Quake 2 style. That is the same as B, but needs the map recompiled.
- **DarkPlaces**: Quake 3 maps get brush collision; for Quake maps it has an experimental polygon collision option.
- **Rebuilding brushes from hull 0**: ericw-tools' `bsputil --decompile` and Quake 3's `bspc` (which converts Quake 1
  maps for bot navigation) do the same walk as B, cutting a box by the tree down to each leaf.

For Quake VR's own maps (vrclimb and so on) we could later read a `BRUSHLIST` lump when there is one (exact clip
brushes, no recovery) and fall back to the rebuild otherwise.

## Clip brushes

Hull 0 has no clip brushes, so B alone would let the player through them. The prototype recovers them from hull 1:
it cuts hull 1 into its convex solid leaves the same way, and a leaf where Quake's 32 box somewhere meets none of
hull 0's brushes (tested at its centre, near its corners, just inside each face that faces the open, and on a
lattice) is kept as a clip brush. It is kept as it is in hull 1 (already grown by the 32 box), and a narrower box meets
it shrunk on the faces that face the open, by the difference (6 units a side for a 20-wide box). A leaf that is only
partly clip is kept whole (harmless: the rest of it is inside real walls anyway).

This also picks up the places where qbsp's hull 1 is rounder than the exact shape at slanted corners, so those keep
Quake's slight rounding.

What is left: 0-26 random moves in 20000 per map (0.13% at worst, e4m2) still stop sooner in hull 1 than in the
rebuilt brushes, where a clip brush shares a leaf with real geometry and none of the samples found it. Worst case, the
player can get a few units further into a clip brush there than Quake allows. A fallback that would close it: when
hull 1 stops a move sooner at a spot where the 32 box meets no brush, take hull 1's answer (a second trace, about 250
ns, only while the player is 16 units or more from walls).

## The prototype

`vr_hull_width` (Debug > Tests > Hitbox Width), default 0 (off: Quake's hull 1, nothing built). 8 to 32: the player's
width against the map in units. Only the player's world clipping changes:

- A client's move with its own 32-wide box (and QuakeC traces from the player with a 32-wide box) meets SOLID_BSP
  models (the world, doors, lifts, trains, buttons) with a box of that width, the height kept at hull 1's 56 from the
  box's feet. Everything else is Quake's: the player's box against monsters, items, triggers and shots stays 32 wide,
  and monsters, items and missiles use Quake's hulls.
- Doors and lifts: pushing checks use the same narrow box (`SV_TestEntityPosition` goes through `SV_Move`).
- The lean recentring (the body following the headset, `worldtrace::playerBoxFits`) uses the narrow box too.
- 32 is the new collision at Quake's size (for comparing).

Files: `Quake/vr/vr_hull.cpp` and `.hpp` (the build, the sweep, the test aids), `Quake/world.c`
(`SV_ClipMoveToEntityQVR`, `moveclip_t.bspbox`), `Quake/vr/vr_api.h`, `vr_cvars.inc`, `vr_menu.cpp` (Debug > Tests),
`vr_trace.cpp` (lean), `vr_physics.cpp` and `vr_progs.cpp` (hooks).

Test aids (also on Debug > Tests):

- `vr_hull_stats`: the map's brushes, clip brushes, memory, build time.
- `vr_hull_bench [moves] [width]`: random moves from where Quake's player fits, each through hull 1, through the
  brushes with the same 32 box, and with the narrow box: time per trace and where they disagree.
- `vr_hull_probe`: which brush the player's narrow box is in, and by how much (for a "stuck" report).
- `vr_hull_walktest <seconds> [seed]`: drives the player around the map at run speed in random directions, jumping now
  and then, and puts them down somewhere random every few seconds (half the time next to a door, lift, train or
  button). It counts frames where the box starts in solid (stuck), where the player's centre is inside a wall
  (embedded), and where the player is outside the map; stuck in a monster is counted apart. Use with
  `god; notarget`.

## Numbers

### Load and trace cost, and agreement with hull 1 (32 box), all 32 id1 maps

`vr_hull_bench 20000 20`, `--exclusive`. "Agree": the 32-wide brush sweep and hull 1 end within 1 unit of each other.
"Hull 1 sooner": hull 1 stops 1+ units earlier (8+ in brackets): clip brushes not recovered. "Brush sooner": the
other way; almost all of them start inside thick solid where hull 1 has empty pockets that no player can reach (the
bench starts moves wherever hull 1 is empty; at every one I checked, every point of the 32 box was inside hull 0's
solid).

| map | build ms (clips) | brushes | clip brushes | hull1 ns | brush32 ns | brush20 ns | agree % | hull1 sooner (8+) | brush sooner (startsolid) | narrow sooner |
|---|---|---|---|---|---|---|---|---|---|---|
| start | 35.0 (26.5) | 1397 | 45 | 229 | 356 | 323 | 100.00 | 0 (0) | 1 (0) | 0 |
| e1m1 | 45.0 (36.8) | 1278 | 30 | 278 | 418 | 360 | 100.00 | 1 (0) | 0 (0) | 0 |
| e1m2 | 43.3 (35.1) | 1392 | 33 | 248 | 362 | 332 | 99.58 | 4 (4) | 80 (87) | 0 |
| e1m3 | 43.0 (33.3) | 1360 | 43 | 262 | 397 | 363 | 99.99 | 0 (0) | 2 (1) | 0 |
| e1m4 | 57.3 (46.9) | 1469 | 83 | 384 | 387 | 361 | 99.94 | 2 (1) | 9 (9) | 0 |
| e1m5 | 37.4 (29.5) | 1271 | 2 | 295 | 270 | 244 | 99.72 | 2 (2) | 55 (59) | 0 |
| e1m6 | 21.2 (14.0) | 1085 | 8 | 174 | 279 | 257 | 99.98 | 0 (0) | 4 (5) | 0 |
| e1m7 | 8.9 (6.4) | 383 | 10 | 138 | 181 | 168 | 99.99 | 2 (1) | 0 (0) | 0 |
| e1m8 | 18.0 (14.4) | 648 | 59 | 144 | 241 | 213 | 100.00 | 1 (1) | 0 (0) | 0 |
| e2m1 | 68.0 (57.7) | 1588 | 7 | 260 | 381 | 357 | 99.99 | 1 (1) | 1 (0) | 0 |
| e2m2 | 74.3 (63.4) | 1741 | 47 | 308 | 443 | 410 | 99.97 | 4 (4) | 1 (0) | 0 |
| e2m3 | 68.8 (58.7) | 1657 | 70 | 205 | 358 | 334 | 99.99 | 1 (0) | 1 (0) | 0 |
| e2m4 | 69.3 (59.1) | 1672 | 58 | 276 | 352 | 322 | 99.99 | 0 (0) | 2 (0) | 0 |
| e2m5 | 44.6 (36.1) | 1320 | 47 | 195 | 285 | 254 | 99.98 | 3 (2) | 0 (0) | 0 |
| e2m6 | 39.2 (30.0) | 1518 | 34 | 208 | 285 | 260 | 100.00 | 1 (1) | 0 (0) | 0 |
| e2m7 | 51.8 (41.6) | 1577 | 54 | 276 | 365 | 341 | 99.94 | 3 (2) | 10 (10) | 0 |
| e3m1 | 62.6 (53.7) | 1293 | 23 | 213 | 334 | 321 | 99.91 | 4 (4) | 14 (17) | 0 |
| e3m2 | 23.3 (17.1) | 994 | 2 | 185 | 264 | 237 | 100.00 | 1 (1) | 0 (0) | 0 |
| e3m3 | 27.8 (21.0) | 1136 | 15 | 216 | 325 | 298 | 99.99 | 1 (1) | 1 (0) | 0 |
| e3m4 | 55.6 (45.0) | 1634 | 11 | 214 | 349 | 312 | 99.99 | 0 (0) | 2 (0) | 0 |
| e3m5 | 97.9 (85.9) | 1718 | 38 | 214 | 388 | 354 | 99.99 | 2 (2) | 0 (0) | 0 |
| e3m6 | 45.6 (35.5) | 1732 | 15 | 183 | 270 | 249 | 99.48 | 6 (4) | 97 (100) | 0 |
| e3m7 | 33.4 (25.3) | 1380 | 6 | 216 | 372 | 330 | 100.00 | 0 (0) | 0 (0) | 0 |
| e4m1 | 44.2 (34.3) | 1424 | 19 | 471 | 364 | 326 | 99.97 | 0 (0) | 5 (2) | 0 |
| e4m2 | 56.0 (48.1) | 1044 | 81 | 222 | 338 | 299 | 99.86 | 26 (19) | 1 (0) | 0 |
| e4m3 | 45.8 (37.7) | 1175 | 72 | 249 | 359 | 332 | 99.98 | 3 (3) | 1 (0) | 0 |
| e4m4 | 54.1 (45.7) | 1237 | 263 | 209 | 343 | 333 | 99.95 | 9 (7) | 0 (0) | 0 |
| e4m5 | 38.5 (30.5) | 1105 | 69 | 200 | 297 | 268 | 100.00 | 1 (1) | 0 (0) | 0 |
| e4m6 | 35.9 (29.4) | 903 | 22 | 248 | 297 | 271 | 99.98 | 1 (1) | 2 (0) | 0 |
| e4m7 | 83.9 (71.8) | 1554 | 68 | 382 | 405 | 378 | 99.70 | 4 (4) | 56 (60) | 0 |
| e4m8 | 44.7 (36.3) | 1167 | 83 | 201 | 289 | 272 | 99.97 | 3 (3) | 4 (5) | 0 |
| end | 11.4 (8.0) | 376 | 0 | 126 | 216 | 192 | 100.00 | 0 (0) | 0 (0) | 0 |

Mean over the 32 maps: build 46.4 ms, hull1 238 ns, brush32 330 ns (x1.39), brush20 302 ns (x1.27)

### Random walk (stuck and fall-through)

`vr_hull_walktest 90 11` on a fresh load of each map, `god; notarget`, at Quake's hull (32) and at 20 and 16 wide:
about 90 seconds and 20000 units of running into walls, corners, stairs, slopes and ledges on each, and 16-19 drops
somewhere random (half of them next to a door, lift, train or button). Nothing stuck in the map, nothing inside a
wall, nothing outside the map.

| map | width | frames | units walked | hops | stuck in the map | in monsters | embedded | outside |
|---|---|---|---|---|---|---|---|---|
| e1m1 | 32 | 5620 | 19164 | 17 | 0 | 0 | 0 | 0 |
| e1m2 | 32 | 5624 | 23328 | 17 | 0 | 0 | 0 | 0 |
| e1m3 | 32 | 5623 | 19882 | 17 | 0 | 0 | 0 | 0 |
| e2m2 | 32 | 5623 | 12832 | 18 | 0 | 0 | 0 | 0 |
| e3m3 | 32 | 5623 | 17288 | 18 | 0 | 0 | 0 | 0 |
| e4m3 | 32 | 5625 | 18902 | 17 | 0 | 0 | 0 | 0 |
| e4m7 | 32 | 5622 | 18839 | 18 | 0 | 0 | 0 | 0 |
| e1m1 | 20 | 5436 | 21804 | 16 | 0 | 0 | 0 | 0 |
| e1m2 | 20 | 5265 | 21243 | 17 | 0 | 0 | 0 | 0 |
| e1m3 | 20 | 5623 | 18074 | 17 | 0 | 0 | 0 | 0 |
| e2m2 | 20 | 5624 | 17912 | 18 | 0 | 0 | 0 | 0 |
| e3m3 | 20 | 5624 | 19255 | 18 | 0 | 0 | 0 | 0 |
| e4m3 | 20 | 5436 | 19854 | 18 | 0 | 0 | 0 | 0 |
| e4m7 | 20 | 5461 | 20810 | 19 | 0 | 0 | 0 | 0 |
| e1m1 | 16 | 5617 | 20002 | 18 | 0 | 0 | 0 | 0 |
| e1m2 | 16 | 5017 | 21361 | 16 | 0 | 0 | 0 | 0 |
| e1m3 | 16 | 5622 | 19454 | 16 | 0 | 0 | 0 | 0 |
| e2m2 | 16 | 5622 | 18755 | 18 | 0 | 0 | 0 | 0 |
| e3m3 | 16 | 5620 | 17648 | 18 | 0 | 0 | 0 | 0 |
| e4m3 | 16 | 5388 | 17431 | 18 | 0 | 0 | 0 | 0 |
| e4m7 | 16 | 5502 | 22446 | 19 | 0 | 0 | 0 | 0 |

An earlier run (before the fix to Quake 2's fraction above) found the sloped-wall case in e4m3 at 20 wide. It also put
the player inside a monster's box twice at 16 wide (an ogre in e1m2, a shambler in e2m2), never at 32. My guess, not
checked: the player's box against entities stays 32 wide, so at 16 wide it reaches 8 units into walls, and through a
thin wall or grate into a monster standing behind it. That is the case for decision 3 below.

## What to decide

1. **Width.** 20 is my suggestion to try first (10 units from walls instead of 16: 0.38 m instead of 0.61 m). 16 fits
   through 16-unit gaps and puts you 0.3 m from walls.
2. **Height.** Kept at hull 1's 56. The same machinery can make it follow the headset (crouching under things), but
   QuakeC and the maps assume 56 in places (doorways, vents); a separate decision.
3. **The player's box against monsters, items, triggers and shots.** The prototype keeps it 32 wide (only the map is
   narrower), so monsters hit you as before and nothing gets easier. Shrinking it too would let you squeeze between
   monsters and make you harder to hit.
4. **Clip brushes.** Accept the heuristic's residue, or add the hull 1 fallback (a second trace).
5. **Monsters.** The same sweep works for any box. Monsters' movement and `SV_CheckBottom` would change (they'd fit
   through smaller gaps and stand closer to ledges), so it would be per class and opt-in.
6. **On by default?** The load-time build (up to 100 ms on id's maps) could move to a worker thread before it is.
