# Quake VR / Ironwail: physics performance, measured

Date: 2026-10-03. Follow-up to [PHYSICS_PERFORMANCE_REVIEW_2026-10-03.md](PHYSICS_PERFORMANCE_REVIEW_2026-10-03.md) (a
static review): its claims checked against measurements, the costs that actually dominate found by profiling, and the
fixes with the best return implemented and measured. Branch base: `b654816a`.

## Summary

The review's main premise, that the integration costs more than Box3D's solver, holds, and more strongly than it
expected: in every scene measured, `b3World_Step` is 10-30% of the server's physics time. But the costs that dominate
are mostly not the ones it ranked first. The largest, none of them in the review, were:

1. **Quake's area tree was 4 levels deep** (16 leaves for a whole map). Near a pile of props, every trace and every
   touch query walked every prop in the pile: SV_LinkEdict's touches, the hands' pose traces (on the client, every
   frame), the hit boxes, the buried test. O(n²) in the pile.
2. **QC touches between props**: each moving prop relinks every frame and calls the touch function of every prop its
   (hand-grown) box overlaps; for rocks, bricks, thrown weapons and crate pieces that is `forcegrabbable_touch`, which,
   for a toucher that takes no damage, does nothing. Thousands of QC calls a frame in a toppling pile.
3. **The buried test** (`keepInWorld`, every moving rigid body, every frame) rebuilt the body's whole drawn triangle list
   to take its mean.
4. **QC's force-grab target search** (every frame, each open hand, everything within 200 units) looked up each prop's
   settings by name before testing whether the hand even points at it.

Fixing those four (and the review's item 1, water) halves the server's physics time while piles move, and cuts it by
a third with piles lying still; a pool of sunk rocks costs a quarter of what it did. Every change except the water
sleep is bit-for-bit the same physics (checked by hash).

| Scene (60 Hz server, as with a 120 Hz headset) | Host frame, base → now | Server physics, mean | p99 |
|---|---|---|---|
| 500 rocks toppling (first 3 s) | 2.84 → 1.47 ms (−48%) | 4.68 → 2.30 ms | 9.58 → 5.44 ms |
| 500 rocks and crates toppling | 2.30 → 1.17 ms (−49%) | 3.64 → 1.67 ms | 8.33 → 4.53 ms |
| 1000 pieces of debris toppling | 6.35 → 3.16 ms (−50%) | 11.19 → 5.46 ms | 23.2 → 16.1 ms |
| A pile blown up 6 times | 2.62 → 1.39 ms (−47%) | 4.35 → 1.85 ms | 8.80 → 5.56 ms |
| 500 rocks lying still | 0.70 → 0.49 ms (−30%) | 0.46 → 0.40 ms | 0.53 → 0.47 ms |
| 500 rocks and crates lying still | 0.71 → 0.44 ms (−38%) | 0.42 → 0.31 ms | 0.72 → 0.52 ms |
| 1000 pieces lying still | 1.13 → 0.77 ms (−32%) | 0.64 → 0.54 ms | 0.75 → 0.66 ms |
| 120 rocks sunk in a pool, 24 crates floating | 1.06 → 0.27 ms (−74%) | 1.74 → 0.21 ms | 1.84 → 0.28 ms |
| 24 grunts dying (8 ragdolls) | 1.14 → 0.64 ms (−44%) | 1.65 → 0.71 ms | 4.00 → 1.89 ms |
| 24 grunts dead | 0.49 → 0.37 ms (−24%) | 0.40 → 0.26 ms | 0.50 → 0.35 ms |

Host frame: the whole frame's CPU time (the server every second frame; no drawing), averaged; server physics: SV_Physics
from the world's turn to the end of Box3D's frame, per server frame. Medians of 3 runs each, the variants interleaved.
"Lying still" is the 5 s after 10 s of settling.

## How it was measured

A headless test environment on Linux (no headset, no GPU work):

- **Build:** CMake with clang 20 (`-O2 -g`; the Windows build is clang-cl too). clang 18 and gcc 13 cannot build Zancle
  (C++23 library traits). One Linux-only compile fix (`vr_ao.cpp`). QC built with FTEQCC from fteqw's master
  (Ubuntu's fteqcc is too old for this QC).
- **Data:** the shareware `pak0.pak`, with the registered and mission-pack models the progs precache stubbed by shareware
  ones (outside the repository). The scenes use Quake VR's own models (rocks, bricks, crates) and the grunt (shareware).
- **Running:** Xvfb only for the window at start-up; the mock headset with `vr_fixed_frames 1` (the game's clock fixed:
  the same frames every run, whatever the machine's pace) and `vr_mock_fast 2` (unpaced frames not drawn:
  `VR_HeadlessView`). `vr_fixed_frames_rate 120`: the host at 120 Hz, so the server (`host_netinterval`, 1/72 s at
  most) runs every second frame, at 60 Hz, as in the headset.
- **Timing:** new `vr_physics_frametime`: each server frame's physics by phase (sync, before-step, step, `b3World_Step`
  alone, write-back, after), mean/median/p95/p99/worst, and the wall-clock time of the host frames between two calls.
  Profiles with `perf` (DWARF call graphs), folded per subtree.
- **Scenes:** `Misc/quakevr/physbench/physbench.py` (piles of 500-1000 rocks, bricks, debris and crates toppling and
  lying still, a pile blasted again and again, a pool, grunts dying as ragdolls and corpses). It runs several builds
  interleaved, each with `-noconfigwrite` (the game saves archived cvars as it quits: a run's `+cvar` would leak into
  the next ones; that happened to an early round of these measurements, whose numbers are not used here).
- **Equivalence:** `vr_physics_hash piles`: the spawned props' state at the end, bit for bit. The map's own props are
  left out (their start depends on the wall clock: the thrown weapons on vrfiringrange's tables differ between runs of
  the same build). The rocks and debris scenes are deterministic, threads and all; mixed, water and blasts are not
  run-to-run, so for those only distributions compare.

```
python3 Misc/quakevr/physbench/physbench.py gen quakevr
DISPLAY=:5 python3 Misc/quakevr/physbench/physbench.py run --exe base=<exe> --exe new=<exe> --basedir <quake> --reps 3 > out.txt
python3 Misc/quakevr/physbench/physbench.py parse < out.txt
```

## Where the time went (base, 1000 pieces of debris, whole run)

| Share of the server frame | What |
|---|---|
| 34% | `writeProp` → `SV_LinkEdict(ent, true)` → `VR_TouchLinks`: half the area query (`SV_AreaEdictsR`), half QC touches |
| 24% | `SV_Physics_Client` QC: the force-grab search (`modelcentre`, `propvalue` by name) |
| 13% | `VR_RigidToss` → `keepInWorld` → `buried` → `drawnVertices` |
| 4% | `beforeStep` → `touchNearby` (`SV_Move`) |
| 5% | `b3World_Step` (all of Box3D's solver and collision) |

Outside the server, the client's hand pose (`handpose::gunPlanes` → `SV_Move`) was 19% of a host frame with 1000 props
lying near the player: the area tree again.

## Implemented (branch commits, in order)

| Commit | Change | Effect | Same physics |
|---|---|---|---|
| Linux build | `vr_ao.cpp`: a constructor for a const object off Windows | builds with clang on Linux | — |
| Physics bench | `vr_physics_frametime`, `vr_physics_hash piles`, `physbench.py` | the measurements above | — |
| Area nodes | `world.c`: `AREA_DEPTH` 4 → 8, loose by 32 units (anything under 64 units goes down to a leaf) | lying piles −25..−35% host frame; hand traces near piles 19% → 3% of a frame | bit-identical on rocks/debris; see below |
| Touches | `vr_physics.cpp`: a rigid prop's `forcegrabbable_touch` (or the rock/brick/crate-piece/thrown-weapon wrappers) is not called when the toucher takes no damage and the prop is not in a throw | write-back −60% while piles move (2.4 → 0.9 ms, 500 rocks) | bit-identical |
| Buried test | `vr_held.cpp`: `drawnCentre`, the drawn shape's mean kept by model, pose and drawn transform | −20..−25% host frame while piles move; dying ragdolls −40% | bit-identical |
| Force grab | QC: the cone test before `VR_Forcegrab_IsEligible`; `props::keyByName` remembers the last names | lying piles in reach: server physics −25..−30% (0.57 → 0.39 ms, 500 rocks) | same target (both tests have no side effects) |
| Water sleep | a sunk prop (density over water's) may sleep once it touches something; floating ones still bob | pool of sunk rocks: 116 of 120 asleep (was 0), Box3D 0.51 → 0.14 ms | no: rocks rest within 1-2 units of where they did (they no longer creep) |
| Water lift | the lift applied again in each piece of a slow frame's step | correctness (below) | no (only frames slower than 1/45 s) |

The area tree's order of found edicts differs. Where a trace starts inside two solids at once, Quake reports the last
one it meets: with crates in a pile a different (equally valid) crate takes a hit box's hit, as relinking order could
already change. The mixed (crates) scene's end state is not run-to-run deterministic in either build, so it shows as a
distribution difference, not a regression.

### The water lift bug (review item 1's force-lifetime issue): confirmed, worse than it looked

`b3World_Step` clears forces after each step, and a server frame slower than 1/45 s is stepped in pieces, so the later
pieces had gravity without lift. With a 60 or 72 Hz server that only happens in hitches, but a dedicated server ticks at
20 Hz (three pieces): floating crates in vrcalibration's pool **sank to the bottom** (median height −112; the surface is
at −10). With the fix they float at −11, as at 60 Hz.

## The review's items, checked

| # | Review's claim | Verdict | Measured |
|---|---|---|---|
| 1 | Wet props never sleep | **Confirmed**, fixed for sunk props | 120 sunk rocks: 0 asleep → 116; Box3D 0.51 → 0.14 ms. Floating props still bob awake (by design; an equilibrium sleep would change how they look). |
| 1b | Buoyancy force lost in later step pieces | **Confirmed**, fixed | crates sank at 20-30 Hz servers |
| 2 | Population scans: dense lists, move events | Correct, **low value** | `sync` 0.02-0.05 ms a frame with 640-1140 bodies |
| 3 | Sleeping ragdolls rebuilt every frame | Correct, **low value** | write-back 0.015 ms with 8 dead ragdolls, 0.028 with 32 |
| 4 | Every moving prop gets an extra hit-box trace | Correct, **medium** | `before` 0.2-1.1 ms while piles move; a gated version needs QC care (monster touches) |
| 5 | Quadratic impact/shock searches | Correct, **low value** | `after` ≤ 0.02 ms mean, 0.4 ms p99 with 1000 pieces |
| 6 | 4 substeps may be too many at 120 Hz | **Refuted as a speedup** | see below; and the server runs at 60 Hz at 120 Hz, not 120 |
| 7 | Hull complexity / narrow phase | **Low value** | Box3D's `collide` is a fraction of `solve` |
| 9 | Begin events requested too broadly | Correct, **low value** | step minus solver ≈ 0.01 ms |
| 11 | Data locality | **Low value** now | the scans it would speed up are cheap |
| 13 | Linkage and touch work outside the solver | **Confirmed, and the largest cost**, for other reasons | the touches' QC calls and the area tree, not the bounds math |
| 14 | Threads | see below | |
| — | Not in the review | **The four largest costs** | area tree, prop-prop QC touches, buried test, force-grab search |

**Server cadence.** `host_netinterval` is 1/72 s with `host_maxfps` above 72, and a server frame runs when that much
time has gathered, taking all of it: at 120 Hz frames every second frame (60 Hz, 1/60 s), at 90 Hz every second (45 Hz,
1/45 s), at 144 Hz every second (72 Hz). Four substeps at 120 Hz are 240 substeps a second, not 480; and one step piece
a frame at all of these.

**Substeps 2 instead of 4** (current build, otherwise defaults): each step's solver is cheaper, but piles settle much
worse, so more bodies stay awake for longer and the frame costs more where it matters:

| Scene | Host frame, 4 → 2 substeps | Awake bodies |
|---|---|---|
| 500 rocks toppling | 1.49 → 2.03 ms (+36%) | 268 → 471 |
| 1000 pieces toppling | 3.10 → 4.29 ms (+38%) | 509 → 884 |
| 1000 pieces lying still | 0.68 → 0.82 ms (+20%) | 29 → 82 (still jittering after 15 s) |
| A pile blown up | 1.22 → 1.03 ms (−15%) | 327 → 304 |

Keep 4. A lower-substep quality tier would need the stacking tuned for it first, and would still lose in piles.

**Threads** (`vr_box3d_threads`, 4 workers from 150 awake bodies; this is a 4-core container, so the headset's PC
will differ): the defaults are right. With 1000 pieces toppling the solver takes 0.95 ms a frame threaded against 1.54
on the main thread alone (host frame 3.10 against 3.46 ms); 2 workers fall in between (1.34 ms). Lying piles are below
the threshold and identical. The threads' cost is no longer where the frame goes: the integration's main-thread work
is.

## Other findings

- **vrfiringrange's water has no floor in the level's collision** where it is deep: the big pool north-east of the start
  and the channel along the south edge. Props sinking there pass through and fall out of the world (stopped 1024 units
  below it by `writeProp`'s safety net). The channel's floor is a 20-unit slab with water under it and no top face in
  the BSP; the map looks leaky (water fills what should be the outside). Worth a look in TrenchBroom.
- **Run-to-run variation:** the rocks and debris scenes end bit-identical every run, threaded or not. The crates, water
  and blast scenes vary run to run in either mode (crates and blasts draw QC random numbers, whose sequence also feeds
  the map's own entities). Not attributed further.
- The map's own props start differently each run (wall-clock dependence before the scripted frames begin).

## Not done, and what remains

After these changes, with 1000 pieces toppling, the remaining server time is roughly: Box3D's step a third, the
touch queries' area walk (`VR_TouchLinks`: the pile's neighbours are genuinely in each box) a sixth, the force-grab QC
loop and `findradius` a sixth, the hit-box traces a tenth. Candidates, by return:

1. **Hit-box traces (review item 4):** skip `touchNearby` for props too slow to hurt, after checking the QC touch
   functions of monsters (a leaping dog's or fiend's touch acts on what it meets).
2. **Force-grab search:** an engine builtin for the search (findradius + cone + eligibility in C++), or a coarse cone
   test on the origin before `modelcentre`.
3. **Floating props' equilibrium sleep** (review item 1's second half): a design decision (they would stop bobbing).
4. **Touch queries:** `FL_EASYHANDTOUCH` grows every prop's box for the hands; a prop-to-prop query could use the
   ungrown box.
