# A smaller player hitbox on unmodified maps

Your notes `vrclimb_2026-09-29_20-06-59` and `20-07-38`: Quake's player box is 32x32x56 units, so you stop 16 units
(0.6 m at Quake VR's scale) from every wall and ledge. You asked how to make it smaller on the maps as they are (no
recompiling), with Quake's collision speed, smoothness and accuracy.

This page lists the ways to do it, with their pros and cons, what I recommend, the prototype that is in the build
(on by default since round 21's "Player hitbox defaults": 16 wide, the compiled hull; shots hit a 24-wide box), and the
numbers.

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

## In the game (round 2: everywhere, two methods, a settings page)

Your note after trying 16: the new collision only worked against plain walls; the walls holding vrfiringrange's
buttons still stopped you at 32 (from the side too), and you wanted it against props, monsters and players as well,
with settings to play with.

**Why the button walls stayed at 32.** The first prototype narrowed only the brush models whose hull 0 is the world's
(the world and its `*n` submodels: doors, lifts, func_walls built into the map). vrfiringrange's second row of monster
panels and buttons, the "dummy attacks" button and the prop area's table and wall are *external* brush models
(`maps/vr_spawnpanel.bsp`, `vr_spawnbutton.bsp`, `vr_panel_north.bsp`, `vr_button_north.bsp`, `vr_proptable.bsp`,
`vr_propwall.bsp`: `make_spawn_buttons.py`, `make_prop_area.py`), each a separate .bsp with its own hull 0 and hull 1.
The prototype had built brushes only from the world's hull 0, so for these `clipBSP` found no brushes and fell back
to the model's own hull 1: Quake's 32 box, from every side. Id's ammo and health boxes (`maps/b_*.bsp`) are the same
kind of model (they are triggers, so it did not show there; the explosive boxes are solid boxes, below).

**Now every brush model** gets the narrow box: the world's submodels as before, and each external .bsp model the first
time something meets it (its hull 0 walked into brushes the same way: a few brushes, well under a millisecond; its
leaves get their own range in the brush tables). Func_wall, doors, plats, trains, buttons and rotating brush models all
go through the same code (Quake doesn't turn brush models' collision, and neither does this).

**Against entities**, the player's box is narrowed too (`vr_hull_ent_width`, by default the same width), both ways:

- The player moving: its box against monsters, other players and other solid boxes (the explosive boxes, which are
  SOLID_BBOX, and anything else solid that isn't a brush model) is the narrow one.
- Them moving into the player: a monster's, another player's or a solid box's move (a body's move: a box, not a shot)
  meets the player's box narrowed, so the gap is the same whichever of you moves. Doors, lifts and trains pushing the
  player test the player's position with the narrow box already (`SV_TestEntityPosition` goes through `SV_Move`).
- Loose Box3D props (rocks, bricks, weapons on the floor) never block the player in Quake's movement (they are
  touchable, not solid); the player pushes them with a Box3D capsule of `vr_box3d_player_radius` (15 cm: about 8 units
  wide, already narrower than any of these boxes). It is on the new page as Prop Push Radius; not tied to the width.
- **What shots and missiles hit** on the player is its own width, `vr_hull_hit_width` (Width Shots Hit, 24 by default;
  0 Quake's 32): any move that isn't a body's (hitscan traces, missiles, grenades, gibs: points and MOVE_MISSILE) meets
  the player's box narrowed to it about its centre, its height kept (`hull::hitBox`, `SV_ClipMoveToBoxEntityQVR`).
  The author's note (e1m3_2026-09-30_02-34-18): "still a bit of leniency for enemies to hit you". What doesn't use it:
  monsters' melee (id's `ai_melee`, the dog's bite, the ogre's chainsaw, the fiend's claws: distances between origins,
  60 or 100 units), splash damage (`T_RadiusDamage`: the distance to the box's centre, and `CanDamage`'s nomonsters
  traces), monsters' sight (`visible`: nomonsters) and range checks (origins), and their pathing (`SV_CloseEnough`
  compares the goal's `absmin`/`absmax`, which stay Quake's 32 box). `CheckAttack`'s traceline (a monster's eye to the
  player's, which must hit the player) ends at the box's centre line, inside any width. A fiend's leap is a body's move
  (the entity width). What the player touches (items, triggers: its own `absmin`/`absmax`) stays Quake's 32 box.

**The player inside a monster at 16** (the first prototype's random walk, an ogre in e1m2 and a shambler in e2m2): I
could not reproduce it (random walks with fixed frames take other paths: 0 in 2 runs, e1m2 and e2m2, at 16 with the
entities at 32, the old setup). The likely cause was the mismatch itself: 16 wide against walls but 32 against monsters, the player's
box reached 8 units further than its world clipping, through thin geometry (grates, bars, thin trims under 8 units)
into the space of a monster standing behind it. With the same width against both there is no such reach. Separately,
the walk now closes the level's exits while it runs: in round 2's first runs it walked into e1m3's and e4m7's exits,
and the intermission put the player at its camera spot, in a ceiling (433 "stuck" frames that were not collision).

**Method A, the compiled hull** (`vr_hull_method 1`, the default now): the same brushes as B (hull 0's, with Quake 2's
bevels, and the recovered clip brushes), each grown by the box (its planes moved out by the box's reach: with the
bevels, exactly the brush's Minkowski sum with the box), then compiled into a BSP tree of Quake's own clipnodes and
planes, as qbsp compiles hull 1: the grown brushes' faces are the splitting planes (qbsp3's choice: the plane most
pieces lie on and that splits fewest, axial first); a leaf is solid where one grown brush fills it (every face of that
brush already split on), empty where none reaches. It is traced by Quake's own `SV_RecursiveHullCheck`, as hull 1 is,
in the box centre's space. Each brush model gets its own tree (external ones on first use); the world's is compiled
with the map, and again when the width changes (a hitch of 30-170 ms, once). One build bug found and fixed on the way: a
face lying on a splitting plane goes to both sides of the split, and on the side the piece is not on it made a flat
"piece" that counted as filling its leaf (a solid leaf out in the open, found by the bench at 1% of moves): pieces
reaching less than 0.01 units past the plane are dropped now.

A and B agree on 99.99% of moves (the rest are grazing contacts, which Quake's hull traces have too: A at 32 wide against
hull 1 disagree on 0.07%, mostly clip brushes). A is a little faster per trace (251 ns against B's 276, hull 1 238),
costs about 90 ms more at load (mean; 170 at most, e2m2) and 165 KB (mean). I made A the default method since you
expect it to be more robust for Quake 1: it is Quake's own trace, with its behaviour on edges, corners and
`startsolid`, and the walks found nothing for either.

**Settings**: **Movement > Player Hitbox** (and a link from Debug > Tests):

| Setting | cvar | Default | What |
|---|---|---|---|
| Width Against Walls | `vr_hull_width` | 16 (0 before config 51) | 0 Quake's 32, 8, 12, 16, 20, 24, 28, 32: against the world and brush models |
| Method | `vr_hull_method` | 1 (Compiled Hull) | 0 Brush Sweep (B), 1 Compiled Hull (A) |
| Doors, Lifts and Walls Too | `vr_hull_brushmodels` | 1 | off: brush models other than the world meet Quake's box |
| Width Against Them | `vr_hull_ent_width` | -1 (Same as Walls) | 0 Quake's 32, or 8-32: against monsters, players, solid boxes |
| Monsters / Other Players / Solid Boxes | `vr_hull_monsters`, `vr_hull_players`, `vr_hull_boxes` | 1 | per category, both ways |
| Width Shots Hit | `vr_hull_hit_width` | 24 | 0 Quake's 32, or 8-28: the box shots and missiles hit |
| Prop Push Radius | `vr_box3d_player_radius` | 15 cm | the Box3D capsule that pushes loose props |

Plus Hitbox Stats, Hitbox Approach and Random Walk (60 s) on the page, and Bench and Probe on Debug > Tests.

Files: `Quake/vr/vr_hull.cpp` and `.hpp` (the brushes, both methods, the test aids), `Quake/world.c`
(`SV_ClipMoveToEntityQVR`, `SV_ClipMoveToBoxEntityQVR`, `moveclip_t`'s narrow boxes), `Quake/vr/vr_api.h`,
`vr_cvars.inc`, `vr_menu.cpp` (Movement > Player Hitbox), `vr_trace.cpp` (lean), `vr_physics.cpp` and `vr_progs.cpp`
(hooks).

Test aids (Debug > Tests; the approach and the walk also on the page):

- `vr_hull_stats`: the map's brushes, clip brushes, brush models (external ones), the compiled hull, memory, times.
- `vr_hull_bench [moves] [width]`: random moves through hull 1, B at 32 and at the width, and A at 32 and at the width
  (each compiled fresh and timed): times per trace, build times, memory, and where they disagree.
- `vr_hull_approach [classname [n]]`: how close the player's box gets, through `SV_Move` as play moves it: with no
  argument, from where you stand in 8 directions to what the box meets; with a classname (`door`, `plat`, `func_wall`,
  `monster_army`, `misc_explobox`, ...), round the n-th such entity from 8 directions, and for a monster also the
  monster moved into the player. It prints the gap from the player's centre to the surface (Quake: 16; the width's
  half: 8 at 16; 11.3 at 45 degrees, the box's corner).
- `vr_hull_probe`: which brush the narrow box is in and by how much, and whether the compiled hull is solid there.
- `vr_hull_walktest <seconds> [seed]`: the random walk (with `god; notarget`); the level's exits closed meanwhile.

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
thin wall or grate into a monster standing behind it. Round 2 narrows the box against entities too (above).

### Round 2: both methods at 16 wide, all 32 id1 maps

`vr_hull_bench 20000 16`, one `--exclusive` run. "B build": the brushes (both methods need them). "A build": compiling
the world's hull for the 16 box on top of that. "A32 vs hull1": the hull compiled for Quake's own box against qbsp's
hull 1 (within a unit along the move). "A16 vs B16": the two methods at 16 (moves more than a unit apart, and moves
whose start one calls solid and the other not).

| map | B build ms | A build ms (16) | A nodes (16) / hulls 1-2 clipnodes | A KB | hull1 ns | B16 ns | A16 ns | A32 vs hull1 % | A16 vs B16 % (apart, startsolid) |
|---|---|---|---|---|---|---|---|---|---|
| start | 35.4 | 86.9 | 8726 / 5326 | 173 | 227 | 305 | 267 | 99.99 | 100.00 (1, 0) |
| e1m1 | 45.4 | 81.6 | 8476 / 5408 | 173 | 259 | 337 | 274 | 99.99 | 99.98 (4, 0) |
| e1m2 | 43.8 | 91.2 | 9076 / 8148 | 173 | 254 | 312 | 276 | 99.56 | 99.94 (11, 0) |
| e1m3 | 44.5 | 114.5 | 11306 / 5858 | 189 | 274 | 343 | 272 | 100.00 | 99.98 (4, 0) |
| e1m4 | 60.8 | 120.1 | 12410 / 6948 | 284 | 381 | 333 | 278 | 99.94 | 100.00 (1, 0) |
| e1m5 | 37.5 | 79.7 | 7810 / 5303 | 142 | 294 | 217 | 223 | 99.72 | 99.97 (6, 0) |
| e1m6 | 19.3 | 47.2 | 4474 / 4152 | 77 | 175 | 230 | 230 | 99.98 | 100.00 (0, 0) |
| e1m7 | 9.0 | 17.5 | 1894 / 1907 | 37 | 139 | 141 | 155 | 99.99 | 100.00 (1, 0) |
| e1m8 | 18.2 | 32.6 | 3589 / 3120 | 56 | 138 | 174 | 162 | 100.00 | 99.99 (2, 0) |
| e2m1 | 66.1 | 117.0 | 11522 / 6117 | 173 | 261 | 337 | 276 | 100.00 | 100.00 (0, 0) |
| e2m2 | 76.2 | 169.5 | 15401 / 6710 | 284 | 310 | 342 | 301 | 99.98 | 100.00 (0, 0) |
| e2m3 | 71.1 | 135.5 | 13580 / 7260 | 260 | 207 | 312 | 265 | 99.99 | 100.00 (1, 0) |
| e2m4 | 66.9 | 117.4 | 11249 / 7702 | 173 | 264 | 280 | 260 | 99.98 | 99.99 (2, 0) |
| e2m5 | 43.4 | 90.0 | 9374 / 6687 | 173 | 192 | 227 | 228 | 99.98 | 99.98 (3, 0) |
| e2m6 | 41.4 | 111.9 | 11309 / 6161 | 173 | 214 | 240 | 262 | 100.00 | 100.00 (0, 0) |
| e2m7 | 54.7 | 126.5 | 12129 / 6113 | 189 | 298 | 321 | 288 | 99.94 | 99.97 (6, 0) |
| e3m1 | 60.6 | 84.2 | 8528 / 6170 | 173 | 218 | 292 | 249 | 99.90 | 99.97 (6, 0) |
| e3m2 | 23.5 | 43.6 | 4004 / 3988 | 77 | 191 | 206 | 223 | 100.00 | 100.00 (1, 0) |
| e3m3 | 27.7 | 64.7 | 6757 / 3996 | 116 | 217 | 272 | 271 | 99.99 | 99.99 (2, 0) |
| e3m4 | 54.0 | 91.0 | 9828 / 7472 | 163 | 211 | 275 | 265 | 100.00 | 100.00 (1, 0) |
| e3m5 | 100.4 | 156.4 | 14799 / 8081 | 260 | 211 | 330 | 250 | 100.00 | 100.00 (1, 0) |
| e3m6 | 47.3 | 110.1 | 10270 / 6462 | 189 | 185 | 225 | 221 | 99.50 | 99.95 (9, 0) |
| e3m7 | 34.5 | 86.3 | 8301 / 3905 | 163 | 204 | 315 | 264 | 100.00 | 100.00 (0, 0) |
| e4m1 | 48.6 | 92.4 | 9086 / 5656 | 173 | 444 | 303 | 241 | 99.97 | 100.00 (0, 0) |
| e4m2 | 40.1 | 65.6 | 7704 / 6035 | 126 | 218 | 280 | 258 | 99.88 | 100.00 (1, 0) |
| e4m3 | 45.5 | 77.0 | 7991 / 6168 | 126 | 257 | 346 | 270 | 99.99 | 99.98 (3, 0) |
| e4m4 | 56.1 | 110.9 | 11371 / 7446 | 189 | 224 | 296 | 244 | 99.95 | 100.00 (1, 0) |
| e4m5 | 38.8 | 74.1 | 8513 / 7561 | 173 | 204 | 244 | 266 | 100.00 | 99.99 (2, 0) |
| e4m6 | 37.2 | 55.7 | 6398 / 5486 | 116 | 238 | 239 | 261 | 99.99 | 99.98 (4, 0) |
| e4m7 | 87.0 | 133.1 | 13257 / 7640 | 260 | 377 | 342 | 293 | 99.69 | 100.00 (1, 0) |
| e4m8 | 43.4 | 77.0 | 8320 / 6157 | 173 | 201 | 242 | 266 | 99.96 | 100.00 (1, 0) |
| end | 11.2 | 26.8 | 2951 / 2038 | 63 | 123 | 166 | 169 | 100.00 | 100.00 (1, 0) |

Mean over 32 maps: B build 46.5 ms, A build 90.2 ms, A 165 KB, hull1 238 ns, B16 276 ns, A16 251 ns, A32 254 ns; A32 vs hull1 99.93%, A16 vs B16 99.99%; max A build 169.5 ms

### Round 2: how close the box gets (vr_hull_approach)

The gap from the player's centre to the surface it stopped at, straight on (45 degrees: the box's corner, x1.41).
Both methods gave the same numbers.

| what | Quake (32) | 16 | 20 |
|---|---|---|---|
| e1m1's walls round the start (worldspawn) | 16 | 8.00 | 10.00 |
| a door (e1m2, `*2`) | 16 | 8.00 | 10.00 |
| a grunt (the player moving; e1m1, e1m2) | 16 | 8.00 | 10.00 |
| a dog moved into the player (e1m1) | 16 | 8.00 | 10.00 |
| vrfiringrange's monster panels (`vr_spawnpanel.bsp`), their side and front | 16 | 8.00 | 10.00 |
| the "dummy attacks" panel (`vr_panel_north.bsp`) | 16 | 8.00 | 10.00 |
| a monster button (`vr_spawnbutton.bsp`) | 16 | 8.00 | 10.00 |
| an explosive box (`misc_explobox`, a solid box) | 16 | 8.00 | 10.00 |
| an item (`item_shells`: a trigger) | touched at 16 | touched at 16 | touched at 16 |

### Round 2: random walks at 16

`vr_hull_walktest 90 11` with `god; notarget`, `vr_fixed_frames 1`, both methods, the entities at 16 too: 6481 frames
and 12000-25000 units walked each, 16-20 hops (half of them next to doors, lifts, trains and buttons).

| map | A: stuck / in monsters / embedded / outside | B: the same |
|---|---|---|
| e1m1 | 0 / 0 / 0 / 0 | 0 / 0 / 0 / 0 |
| e1m2 | 0 / 0 / 0 / 0 | 0 / 0 / 0 / 0 |
| e1m3 | 0 / 0 / 0 / 0 | 0 / 0 / 0 / 0 |
| e2m2 | 0 / 0 / 0 / 0 | 0 / 0 / 0 / 0 |
| e3m3 | 0 / 0 / 0 / 0 | 0 / 0 / 0 / 0 |
| e4m3 | 0 / 0 / 0 / 0 | 0 / 0 / 0 / 0 |
| e4m7 | 0 / 0 / 0 / 0 | 0 / 0 / 0 / 0 |

(e1m2 and e2m2 again with the entities at 32, the first prototype's setup: 0 as well.)

### Round 2: the cost a frame

e1m1, a 30 s random walk each, `vr_profile_detail 2`, `--exclusive`: the server's traces took 0.012 ms a frame with
Quake's hull 1, 0.015 with B at 16 and 0.015 with A at 16 (29 traces and 66-73 hull traces a frame; physics 0.020 against
0.021 ms). Neither method allocates while tracing; the builds do (at load, or when the width or method changes).

## What to decide

1. **Width.** 20 is my suggestion to try first (10 units from walls instead of 16: 0.38 m instead of 0.61 m). 16 fits
   through 16-unit gaps and puts you 0.3 m from walls. 8 and 12 are there to try; below 16 the box is narrower than
   many of Quake's gaps (bars, grates, windows) were made to stop.
2. **Height.** Kept at hull 1's 56. The same machinery can make it follow the headset (crouching under things), but
   QuakeC and the maps assume 56 in places (doorways, vents); a separate decision.
3. **The player's box against monsters and players** (round 2): narrowed by default, the same width both ways
   (Width Against Them, per category). Decided (round 21): shots and missiles hit a 24-wide box (Width Shots Hit);
   melee and splash go by distance; item pickups keep Quake's 32 box.
4. **Clip brushes.** Accept the heuristic's residue, or add the hull 1 fallback (a second trace).
5. **Monsters.** The same code works for any box. Monsters' movement and `SV_CheckBottom` would change (they'd fit
   through smaller gaps and stand closer to ledges), so it would be per class and opt-in.
6. **Method.** A (the compiled hull) is the default: Quake's own trace, a little faster, 90 ms more at load (170 at
   most). B stays selectable for comparing.
7. **On by default?** Decided (round 21, NOTES.md e1m1_2026-09-30_02-19-27, e1m3_2026-09-30_02-34-01): 16 wide with
   the compiled hull is the default (config 51 moves a config's old 0 to 16). The load-time build (B's brushes and A's
   hull: 137 ms on average, 257 at most on id's maps) could move to a worker thread before it is on by default.
