# Quake VR profiling: CPU and GPU, map loading and gameplay (October 2026)

Date: 2026-10-06. Measured before the fixed 72 Hz server tick (`host_fixedtick`, commit `1c021a69`), so its
gameplay numbers are with the old server tick (ROUND21.md, "Server tick rate"). Tree: `vr-ironwail` at `f693dc76` (baseline), then the commits on `agent/profiling`. The question:
where do the CPU and the GPU go, what is worth fixing now (fixed here when the return was high and the change safe),
and what needs a decision. Earlier rounds' findings are not repeated, only referred to:
`PERFORMANCE_BENCHMARK_20261005.md` (particles, props, decals, shadows, portals),
`CPU_PERFORMANCE_FOLLOWUP_2026-10-03.md` and `PHYSICS_PERFORMANCE_RESULTS_2026-10-03.md` (removed 2026-10-06; git history; their open leads
are in [BACKLOG.md](BACKLOG.md)).

**Map loading and gameplay are measured and reported apart** (two sections below): they are optimised separately,
and no load work is in a gameplay number nor the other way round (see "Keeping them apart").

## Summary

- **At 90 Hz on this machine nothing in normal play is CPU-bound.** Every scenario but one holds 11.1 ms (CPU work
  under 2.5 ms a frame on average, GPU under 4.5 ms). The exception is `combined` (everything at once), which is
  **GPU-bound at 15.5 ms**: particles 5.6-7.5 ms, dynamic lights' shadow maps 1.7-6.8 ms.
- **Map loads are the largest waits a player sees**: warden 2.0-3.3 s, ad_grendel 1.8-2.3 s, and a warm reload of the
  same map is no faster (the player's and monsters' hulls are compiled again each time: 1.5-2.6 s of it).
- **Fixed** (each a commit with its numbers, no visible or gameplay change):
  - the hull build's allocations (warden's warm load **-15%**, ad_grendel's **-16 to -20%**, the same trees bit for bit);
  - follow-up (branch `agent/hullsmall`): the hull build's containers sized from measured data and the tree's plane
    index kept across its models' builds (allocations a big-map load 38 M to 4.3 M; warden's warm load **-39%**,
    ad_grendel's **-35%**, the same trees), and the hulls **kept for a reload of the same map** (`vr_hull_keep`:
    warden's `restart` 2134 to **378 ms**, ad_grendel's 1283 to **326 ms**).
  - the particles' retro light levels (that step **-47%**, 0.2 to 0.1 ms a frame with 6500 particles);
  - the benchmark itself: the load's background AO bakes (9 s of 4 threads after the firing range loads) ran inside the
    gameplay windows; now they finish before (`vr_ao_finish`);
  - tooling: VTune collects only the benchmark window or only the map loads (`vr_bench_profiler`), and a script runs a
    scenario under VTune or Nsight Systems (`Misc/quakevr/bench/qvrprof.sh`).
- **For a decision** (the list at the end): a cache of the compiled hulls (and world textures) across reloads of the
  same map (a death's reload on warden ~1.8 s to a few hundred ms), the AO bake's disk cache, batching the shadow
  casters' draws, the retro particles' fill rate, gore crowd hitches, the memory log's minute spike.

## Method

- **Machine**: Intel Core i9-13900K (8 P-cores + 16 E-cores, 32 threads), 64 GB, NVIDIA RTX 4090 (driver
  32.0.16.1088), Windows 11. Release build (`/O2`-class, with its PDB).
- **Settings**: the author's own (`his_cfg_20261006_1237.cfg`: his cvars, binds and video lines left out, his
  `vid_fsaa 0` kept: it is the eyes' MSAA), through `bench.sh --settings`. Eyes 2048 x 2048 (the mock headset),
  both eyes drawn, the mirror window on (`vr_mirror 2`). Note his `vr_hull_width 16`, `vr_mhull 1` (monsters' hulls),
  `vr_shadow_dlights 8` at 1024, `vr_retrolight 1`, `vr_particle_retro_halfres 0`.
- **Benchmarks**: the suite ([BENCHMARKS.md](BENCHMARKS.md)), all 60 scenarios once (`--reps 1`, the author's time
  budget), paced at 90 Hz in real time, exclusive (`kit/benchresults/prof_baseline`, `prof_final`). A/B checks of each
  fix: the scenarios it touches, 2-3 repeats, the two builds run back to back (`prof_hull_A/B`, `prof_retro_A/B`).
- **CPU**: Intel VTune 2024.3, hotspots with user-mode sampling (hardware sampling needs its driver and an
  administrator: not available), results reduced with `Misc/quakevr/bench/vtune_attr.py` (self time charged to the
  nearest game function above it, through malloc, glm and za).
- **GPU**: the engine's own timer queries (every frame in the bench JSON's `gpu_phase_ms`; finer scopes with
  `vr_profile 1`, `vr_profile_gpu 1`) and NVIDIA Nsight Systems 2024.5 (OpenGL API and GPU workload trace,
  `nsys stats`/SQLite). Nsight Graphics was not used: its frame capture needs the window driven interactively, and
  the timer queries already give each pass.
- **Noise**: a machine of this class has room to spare, so most scenarios sit at the frame cap and a 5% change is
  inside run-to-run noise; repeated A/B runs and the engine's own per-step timers were used for the fixes. Visual
  Studio was indexing in the background (one core) during part of the session.

### Keeping them apart

- **Gameplay windows exclude loads.** A scenario's map loads, then its set-up (spawns, blasts), then
  `vr_ao_finish` (new: the load's and the set-up's AO bakes finished), then 180 warm-up frames, then the window
  (`vr_bench_begin`, whose first frame is dropped too). The JSON's frame, CPU and GPU statistics are the window's
  frames only. Before this round the AO bakes started by the load (123 models on the firing range: 9.3 s of work on 4
  threads, below normal priority) were still running during the first seconds of the firing-range windows.
- **Load scenarios** (`load_*`) measure the load itself: from the command to the first frame drawn, by stage
  (`vr_startup_times`), and the worst frame of the second after.
- **VTune** starts paused (`-start-paused`); the engine resumes and pauses it (`vr_bench_profiler`: 1 the benchmark
  window only, 2 the map loads only, from the command to the first frame drawn; `vr_profiler_collect 0|1` by hand;
  Debug > Profiling and Memory > External Profiler Collects). It calls `__itt_resume`/`__itt_pause` in the ITT
  collector VTune injects (`INTEL_LIBITTNOTIFY64`): nothing linked, no SDK, nothing happens without VTune. Every
  capture reported below is one or the other; the first, mixed, capture was redone.
- **Nsight Systems** has no such hook here: its capture is a delay and a duration from the launch, checked against
  `vr_walltime` lines printed at the window's start and end (`combined`: window 13.2-37.7 s, capture 16-22 s).

## Map loading

### Baseline (one run each; ms)

| load | total | bsp | qc_spawn | physics_init | server_other | vr_prepare | first frame | hulls (waited) | alias models | images decoded |
|---|---|---|---|---|---|---|---|---|---|---|
| map e1m1 (cold) | 734 | 94 | 358 | 25 | 19 | 115 | 28 | 0 | 339 | 117 |
| map e1m1 (warm) | 241 | 89 | 17 | 58 | 14 | 26 | 7 | 33 | 0 | 42 |
| restart e1m1 | 221 | 88 | 17 | 54 | 12 | 27 | 5 | 28 | 0 | 41 |
| map e1m1, QRP (cold) | 1320 | 557 | 486 | 24 | 19 | 117 | 28 | 0 | 341 | 569 |
| map e1m1, QRP (warm) | 815 | 559 | 152 | 25 | 13 | 29 | 9 | 0 | 0 | 505 |
| changelevel e1m2 | 470 | 68 | 289 | 31 | 22 | 23 | 4 | 0 | 154 | 27 |
| map e4m7 (cold) | 1252 | 65 | 842 | 53 | 32 | 131 | 27 | 0 | 671 | 102 |
| map e4m7 (warm) | 338 | 58 | 17 | 158 | 17 | 40 | 7 | 105 | 0 | 24 |
| map hip1m1 (campaign switch) | 1088 | 111 | 673 | 34 | 34 | 111 | 30 | 0 | 602 | 133 |
| map e1m1 (switch back) | 963 | 95 | 594 | 24 | 20 | 152 | 11 | 0 | 626 | 421 |
| map warden (cold) | 2410 | 98 | 637 | 1073 | 307 | 146 | 43 | 961 | 492 | 86 |
| map warden (warm) | 3291 | 120 | 35 | 2730 | 247 | 55 | 28 | 2613 | 0 | 21 |
| map ad_grendel (cold) | 1808 | 85 | 629 | 515 | 254 | 129 | 35 | 369 | 469 | 103 |
| map ad_grendel (warm) | 2292 | 86 | 53 | 1686 | 351 | 29 | 18 | 1572 | 0 | 25 |

(`physics_init` is the server's first two frames, where the load waits for the hulls built on the pool since the BSP
was read; on a cold load the QuakeC spawn's model loading hides part of that build, so the warm load waits longer.
Warm loads of the big maps vary by a second between runs: 3291 here, 2046-2255 in the A runs below.)

### Where the time goes (VTune, loads only: warden, cold then warm)

Main thread 1.6 s of CPU; the pool's threads 40 s. Self time by the game's function above it:

| | CPU s | |
|---|---|---|
| `ao::bakePose` (models' own occlusion, background, below normal priority) | 13.2 | brute-force rays against every triangle in reach, per vertex, per pose |
| `hull::clipWinding` | 10.3 | **8.1 of it malloc/free**: two temporary vectors a call |
| `hull::TreeBuilder::choose` | 3.1 | the split plane's choice |
| `hull::splitPoly` | 2.3 | |
| `hull::Face::~Face`, `Frag::~Frag`, `Face::Face` | 2.7 | windings copied and freed |
| the pool's waits and spins | 3.2 | |

The hull build (the player's tree at his width, and one tree per monster width, `vr_mhull`) is the load's critical
path on the big maps: three trees of 100-135 thousand nodes each, built in parallel (each split into speculative
subtrees on the pool, merged deterministically). Allocation dominated it: 30 threads freeing and allocating small
vectors through the C runtime's heap.

**Fixed** (`vr_hull.cpp`, commit "Hull build: clipWinding's ..."): the distances and sides on the stack (64 points,
a heap fallback above), the output winding reserved once (a convex winding cut by a plane gains one point at most),
the front pieces' windings moved instead of copied, the split's outputs reserved. Same trees: `vr_hull_stats` hashes
identical on warden, ad_grendel, e4m7 and e1m1 (brushes, the player's tree, every monster tree). Hull CPU on
warden's two loads 22.9 s to 14.9 s (clipWinding 10.3 to 3.4).

| load (median of 3; ms) | before | after | change |
|---|---|---|---|
| warden cold | 2161 | 2007 | -7% |
| warden warm | 2078 | 1770 | -15% |
| ad_grendel cold | 1686 | 1423 | -16% |
| ad_grendel warm | 1523 | 1214 | -20% |
| e1m1 warm | 241 | 220 | -9% |

What is left in the hulls: `choose` (3.3 s CPU), the faces' copies and frees (2-3 s), the pool's spinning (1.2 s),
and "loaded brush models prepared" (230 ms on every warden load, warm too: the brush models' trees built only three
ways parallel, one thread per tree).

### Hull build, follow-up: the containers, and the hulls kept for a reload

(Branch `agent/hullsmall`, after the fix above. Same method; `vr_hull_stats` hashes with `vr_hull_width 26` and the
monsters' trees, identical before and after on warden, ad_grendel, e4m7 and e1m1, on the pool and on one thread.)

**Sizes measured** (an instrumented build, one load of each map, every call; not committed):

| what | warden | ad_grendel | e4m7 | e1m1 |
|---|---|---|---|---|
| a winding's points (clipWinding's output): mean / p99 / max | 4.5 / 9 / 24 | 4.1 / 6 / 24 | 4.1 / 6 / 16 | 4.0 / 5 / 8 |
| windings over 8 points | 1.3% | 0.03% | 0.02% | 0 |
| a piece's faces (splitPoly's input): mean / p90 / max | 10.7 / 19 / 68 | 6.9 / 9 / 35 | 6.6 / 8 / 19 | 6.4 / 7 / 13 |
| a split's sides (pieces): median / mean / p90 | 3 / 6.1 / 12 | 3 / 5.0 / 10 | 3 / 4.4 / 9 | 3 / 4.5 / 8 |
| `choose`'s candidates: median / p90 (a member: no allocation) | 6 / 61 | 5 / 28 | 5 / 20 | 5 / 20 |
| heap allocations, the whole load (all threads) | 38.4 M | 29.5 M | 2.39 M | 1.54 M |

**Changes** (commit "Hull build: windings in place ..."):

- `Winding` = `za::SmallVector<glm::dvec3, 8>` (was `za::Vector`): 98.7% of warden's windings and all but 0.03% of
  the others' in place. A `Face` grows from 64 to 256 bytes; a `Poly` stays a `za::Vector<Face>` (a piece is moved
  three or four times a split and averages 7-11 faces: in place it would be 2-3 KB a move and 13 KB of stack a
  recursion level). `Face` and `Frag` opt into trivial relocation (a `Poly`'s growth copies bytes).
- `clipWinding`'s distances and sides: `za::SmallVector<double, 64>` / `<int, 64>` (the stack/heap split the
  container's). The question asked: the two empty `za::Vector`s it had cost nothing to make (a null pointer) and a
  `delete` of null each to destroy; the change is for clarity, not time. The output's `reserve(count + 1)` is gone
  (it pushed an 8-point input's output to the heap; in place it has room).
- `splitPoly` clips each face straight into its slot in the front and back outputs (no temporary face moved in); the
  cap's two windings alternate instead of being swapped (a swap of in-place windings copies them three times).
- A node's pieces: `Frags` = `za::SmallVector<Frag, 4>` (sides of 4 or fewer: 68-72% of them; 750 bytes of stack a
  recursion level).
- **The big one, found by counting allocations per function**: `TreeBuilder(Tree&)` indexed all of the tree's planes
  again for every model it compiled (each brush model's tree is built against the world tree's table): ad_grendel's
  440 brush models times two trees, 26 thousand planes each, 23 million inserts and **5.2 of its 8.5 million
  allocations**. The index now lives in the `Tree` and is brought up to date (only the planes added since).

Allocations a load (all threads; hulls off, the rest of a load is 0.33-0.35 M on these maps):

| map | before | windings in place | + pieces in place | + plane index kept |
|---|---|---|---|---|
| warden | 38.4 M | 7.13 M | 6.60 M | **4.30 M** |
| ad_grendel | 29.5 M | 9.02 M | 8.54 M | **3.30 M** |
| e4m7 | 2.39 M | 0.64 M | 0.62 M | **0.43 M** |
| e1m1 | 1.54 M | 0.39 M | 0.33 M | **0.26 M** |

What allocates in the hulls now (ad_grendel, ~3 M): the two pieces of each real split (1.8 M: a `Poly` each side),
the units' saved inputs (0.57 M: `Unit::input`, copied in case the merge has to redo the unit, which it rarely does),
the brushes' growth (0.46 M).

Load times (`bench.sh`, his settings, 90 Hz real time, exclusive, 3 runs each, A then B back to back; median, the runs
in brackets; `hsab1_A` / `hsab1_B`):

| load (ms) | before | after | change | brush models prepared (ms) |
|---|---|---|---|---|
| warden cold | 2107 [2038 2107 3804] | 1724 [1948 1712 1724] | -18% | 139-221 to 53-65 |
| warden warm | 2735 [2735 2022 3110] | 1664 [2137 1473 1664] | -39% | 146-240 to 46-62 |
| ad_grendel cold | 1532 [1437 1532 4313] | 1171 [1243 1171 1145] | -24% | 187-210 to 12-19 |
| ad_grendel warm | 1359 [1747 1282 1359] | 883 [888 848 883] | -35% | 215-317 to 15-16 |
| e1m1 warm | 274 [843 274 247] | 223 [223 224 219] | -19% | 2-3 to 0 |
| e1m1 restart | 262 [894 213 262] | 212 [207 212 216] | -19% | |

(The "before" runs are noisier than the "after" ones: a warm load of a big map varies by a second between runs, as
noted above. The hulls' wait in the load, `physics_init`: warden warm 1968 to 1218 ms, ad_grendel warm 758 to 498.)

**Kept for a reload** (commit "Hulls kept for a reload of the same map", decision 1 below, the author's call):
`vr_hull_keep` (default 1; Debug menu: Keep Hitboxes for Reloads): at a map change the map's brushes and trees are
kept in memory under a hash of the world's content (hull 0's and hull 1's nodes, the planes, the models' heads and
bounds, a build version: `keyOf`), and a load whose world hashes the same takes them back instead of building them:
a death's reload (`restart`, or `load` of the autosave), a changelevel back, the same map again from the menu, the
same BSP under another name. They are kept as the load left them (the world's models, each tree's world tree and the
models prepared with the map; external `.bsp` models and models compiled later are let go: they are made again as
on a fresh load), with their pointers into the hunk cleared while kept and set to the new world's when taken back.
Trees are kept by box size: a width changed in between is compiled again, the other trees reused. `vr_hull_keep N`
keeps N other maps (the one being left is kept until the next load has looked for its own, so 1 also covers a
changelevel back); 0 turns it off and frees them. Kept across game folder changes too (a map package mounted and let
go): the key is the content. `vr_hull_keeptest` (Debug menu: Hitbox Keep Test) builds the brushes and every tree's
world tree again from scratch and compares hashes with the server's; `vr_hull_stats` prints what is kept.

Checked: `vr_hull_keeptest` PASS and `vr_hull_stats` hashes equal to a fresh build after `restart` (e1m1, warden),
after a changelevel back (e1m1 - e1m2 - e1m1), after warden - ad_grendel - warden - ad_grendel (map packages
mounted and let go between), after `vr_hull_width` 26 to 20 and a restart (the 20-wide tree compiled, the monsters'
reused); a map file replaced under the same name (`hulltest.bsp`: vrslopes' content, then vrclimb's, `restart`)
builds again (key changed) and matches a fresh load of vrclimb; `vr_hull_keep 0` builds every time. Memory held:
warden 15.9 MB (brushes, the player's tree and two monster trees, their plane indexes), ad_grendel 12.0 MB, e1m2
1.2 MB.

Load times with and without the kept hulls (`bench.sh --scenarios load_reloads`, a new scenario: nine loads in one
run; his settings, 90 Hz real time, exclusive, 3 runs each, A then B; A: the containers' build, B: with
`vr_hull_keep 1`; `hsab3_A` / `hsab3_B`; median, runs in brackets; B's second run was disturbed by something else on
the machine, every load of it 300-4600 ms slower):

| load (ms) | A: built | B: kept | change | hulls waited for, A |
|---|---|---|---|---|
| warden cold | 1884 [3035 1884 1764] | 1878 [1878 2355 1803] | (none: nothing kept yet) | 3-481 |
| warden `restart` (a death's reload) | 2134 [6796 2134 2121] | **378** [378 812 376] | **-82%** | 1209-1658 |
| `map warden` again | 2052 [1528 2152 2052] | **379** [366 669 379] | **-82%** | 1066-1665 |
| ad_grendel cold | 1795 [1490 1871 1795] | 1680 [1680 3945 1417] | (none) | 0-282 |
| ad_grendel `restart` | 1283 [1139 1364 1283] | **326** [317 1004 326] | **-75%** | 688-925 |
| e1m1 `restart` | 227 [215 227 234] | 208 [206 711 208] | -8% | 0-11 |
| `changelevel e1m1` back from e1m2 | 231 [211 238 231] | 230 [230 632 206] | 0 | 0-15 |

The kept loads wait for no hull at all (0 ms, and 0 ms of "brush models prepared"); looking the kept maps up (the
content hash) takes 0.1 ms on warden. e1m1's hulls were already ready before the load needed them (built while it
spawns), so its reloads gain little. The death's reload with an autosave (`sv_autoload`: `load` of the last save) goes
through the same `SV_SpawnServer` path as `restart`; it was not run here (it writes a save).

### `za::Vector` against `za::SmallVector` / `za::InPlaceVector` outside the hulls (audit)

Quake/vr has about 570 `za::Vector` declarations (members and locals). Only the hot ones were looked at, found by
evidence rather than by reading them all: the heap events a frame in every gameplay scenario (the suite's
`heap/fr`: 5-10 a frame in combat and crowds, 39 in `client`, 122 in `gore_first_cuts` while the first cuts make
limb models), the main thread's allocation sites over a map load and over gameplay frames (`vr_alloc_sites`), and
the load's allocations on every thread (a counting build). Outside the hull build a big map's load makes 0.33-0.35
million allocations in all (warden, ad_grendel with the hulls off), a few tens of ms of CPU across the threads; normal
play makes single digits a frame. So no other site is worth a change on its own; these are recommendations.

| file:line | current | size (measured or reasoned) | evidence | recommendation | expected gain |
|---|---|---|---|---|---|
| `vr_hull.cpp` `Winding`, `Frags`, `clipWinding`'s buffers, the plane index | `za::Vector` | see above | 38 M allocations a warden load | **done** (`SmallVector<dvec3, 8>`, `SmallVector<Frag, 4>`, `SmallVector<double/int, 64>`, index kept) | above |
| `vr_hull.cpp` `Poly` (`za::Vector<Face>`) | `za::Vector` | 6-11 faces of 256 bytes | 1.8 M allocations on ad_grendel (two a split) | keep (in place: 2-3 KB copied a move, 13 KB of stack a level); a per-thread free list of `Poly` buffers if ever | ~1.8 M allocations, maybe 5% of the hulls |
| `vr_hull.cpp` `Unit::input` (copy of a unit's pieces) | `Frags` copied | 50-110 pieces a unit, 3-4 thousand units | 0.57 M allocations on ad_grendel | rebuild a unit's input from its parent's only when the merge redoes it (rare) instead of copying every unit's | 0.57 M allocations and their copies |
| `vr_hull.cpp` `TreeBuilder::choose` `seen_`/`facing_` | `za::Vector`, resized to the table's size per builder | 26-44 thousand planes; a builder per unit and per model | zero-fills ~0.5 MB per builder (warden: ~2 GB over a load) | a stamp kept across builders on the same thread (per-thread scratch) | 0.1-0.2 s of CPU on warden (estimate) |
| `vr_debris.cpp:312` `MapFace::pts` (`debris::plan`, at each load) | `za::Vector<glm::vec3>` | a world face's edges: 4-8 typical | 65 K `new` + 45 K `delete` on warden's load (main thread) | `za::SmallVector<glm::vec3, 8>` | ~5 ms of the main thread a big-map load |
| `vr_fscache.cpp:113-123` `VR_FileCacheHas` | three `za::String`s a lookup | paths of 20-60 chars | 48 K allocations a warden load (main thread), 34 K on e4m7 | fold into a stack buffer (`za::InPlaceVector<char, MAX_OSPATH>`) and look the set up by `StringView` (transparent hash) | ~3 ms a load |
| `vr_box3d.cpp:1311` `corners()` | returns `za::Vector<b3Vec3>` | a leaf's corners (8-20) | 5.7 K allocations a warden load; Box3D's own `b3CreateHull` mallocs 120 KB per hull (1124 hulls: 135 MB of requests) | `SmallVector<b3Vec3, 32>`; the hull allocator is Box3D's (vendored) | small; the 135 MB of requests may be worth a look |
| `vr_water.cpp:480` `Poly` (`cutAlong`, `ensureMesh`) | `za::Vector<glm::vec3>` | a water face's points: 4-8 | 9.6 K allocations on e4m7's load | `SmallVector<glm::vec3, 8>` | ~1 ms a load |
| per-frame gameplay | various | | 1.4 heap events a frame on e1m1 (alias flame refs, AO uploads), 5-10 in combat scenarios (suite `heap/fr`) | nothing to do | none measurable |

(`vr_debris.cpp`, `vr_fscache.cpp` and `vr_box3d.cpp` were left alone here: other work is under way in them, and each is
a few ms of a load.)

### Other load findings

- **QRP world textures are decoded again on every load** (e1m1 QRP: 505 ms of image decoding warm, 569 cold; the
  BSP stage 557 ms both times). Ironwail frees the world's textures with the map.
- **A campaign switch reloads every alias model** (hip1m1 and back: alias models 602 and 626 ms, image decoding
  421 ms on the way back), though id1's models are the same files.
- **Cold loads of id maps** (e4m7, hip1m1: 1.1-1.25 s) are mostly alias models (600-670 ms: their skins, normal maps
  made or read from the cache). Paid once a session.
- **AO bakes**: after each map's first load, the models' self-occlusion is baked in the background (4 pool threads at
  below-normal priority): 123 models in 9.3 s of work on the firing range, 13-21 s of CPU on warden. It is redone
  every session (an in-memory cache only), and until a model's bake ends it has none.

## Gameplay

### Baseline (one run each, 900 frames at 90 Hz; ms)

| scenario | CPU work avg | CPU p99 | GPU 3D avg | GPU p99 | frame p99 | worst frame |
|---|---|---|---|---|---|---|
| `combined` | 5.59 | 27.14 | 15.46 | 19.66 | 27.37 | 35.7 |
| `particles_dense` | 0.88 | 1.59 | 7.03 | 8.52 | 11.14 | 23.3 |
| `combat_48_spectator` | 2.41 | 4.63 | 4.37 | 7.33 | 11.17 | 11.3 |
| `combat_48` | 1.82 | 3.69 | 3.46 | 5.88 | 11.14 | 11.8 |
| `teleporter_ai_24` | 1.95 | 3.16 | 3.10 | 3.89 | 11.20 | 21.9 |
| `gore_gib_limbs2_24` | 1.46 | 4.83 | 2.46 | 5.09 | 11.21 | 19.8 |
| `gore_gib_limbs1_24` | 2.06 | 5.95 | 2.45 | 5.43 | 11.19 | 23.6 |
| `gore_dismember_16` | 1.91 | 5.51 | 2.42 | 5.08 | 11.22 | 27.9 |
| `gore_slash_32` | 1.62 | 6.09 | 2.39 | 4.77 | 11.19 | 28.2 |
| `decals_blood_4096` | 0.97 | 3.60 | 2.34 | 3.33 | 11.12 | 12.2 |
| `teleporter_start` | 1.43 | 2.02 | 2.34 | 2.81 | 11.11 | 11.3 |
| `gore_blast_crowd_32` | 1.67 | 4.63 | 2.30 | 4.34 | 11.15 | 11.2 |
| `gore_gib_limbs0_24` | 1.38 | 4.10 | 2.30 | 4.42 | 11.18 | 18.3 |
| `ragdolls_32_active` | 1.40 | 3.62 | 2.25 | 3.79 | 11.18 | 12.3 |
| `gore_headpop_24` | 1.09 | 3.55 | 2.20 | 4.03 | 11.18 | 25.9 |
| `combat_48_bullettime` | 0.84 | 1.91 | 2.15 | 3.80 | 11.20 | 11.2 |
| `lights_32` | 0.94 | 1.82 | 2.13 | 2.80 | 11.13 | 11.2 |
| `ragdolls_32_settled` | 0.94 | 1.42 | 2.12 | 2.68 | 11.22 | 11.3 |
| `gore_first_cuts` | 2.08 | 9.06 | 1.54 | 2.67 | 11.22 | 46.9 |
| `tour_warden` | 1.79 | 3.26 | 1.93 | 3.32 | 11.16 | 11.9 |
| `gore_limbs_cap` | 1.83 | 5.16 | 1.85 | 3.62 | 11.11 | 11.1 |
| `ai_crowd_64` | 1.81 | 4.13 | 1.06 | 2.58 | 11.20 | 14.0 |
| `props_500_active` | 1.74 | 4.02 | 1.31 | 2.13 | 11.16 | 11.6 |
| `melee_punch_8` | 0.81 | 1.63 | 1.70 | 2.37 | 11.21 | 11.8 |
| `explosions_storm` | 1.56 | 2.12 | 1.69 | 2.25 | 11.22 | 11.6 |
| `lights_32_noshadows` | 0.69 | 1.22 | 1.62 | 2.41 | 11.19 | 12.7 |
| `idle_e1m1_qrp` | 0.51 | 1.17 | 1.58 | 2.08 | 11.24 | 19.7 |
| `tour_warden_flat` | 1.53 | 2.75 | 0.43 | 0.94 | 11.15 | 11.3 |
| `tour_ad_grendel` | 0.98 | 1.71 | 1.43 | 3.20 | 11.16 | 11.5 |
| `tour_vanisch01` | 0.70 | 1.23 | 1.43 | 2.09 | 11.16 | 11.5 |
| `props_500_settled` | 1.20 | 1.93 | 1.41 | 1.87 | 11.12 | 11.2 |
| `decals_1024_stream` | 0.68 | 1.36 | 1.34 | 1.88 | 11.20 | 13.2 |
| `explosions_3s` | 1.34 | 1.90 | 1.14 | 1.45 | 11.19 | 11.4 |
| `torches_32` | 0.86 | 1.43 | 1.33 | 1.81 | 11.23 | 12.2 |
| `idle_start_flat` | 1.33 | 2.00 | 0.70 | 0.93 | 11.13 | 11.3 |
| `menu_open_e1m1` | 0.49 | 1.04 | 1.26 | 1.63 | 11.16 | 12.1 |
| `flashlight_e1m1` | 0.54 | 0.97 | 1.24 | 1.69 | 11.14 | 11.5 |
| `idle_e1m1` | 0.51 | 1.13 | 1.23 | 1.72 | 11.20 | 13.5 |
| `gore_limbs_64` | 0.65 | 0.99 | 1.22 | 1.51 | 11.13 | 11.5 |
| `tour_basetohell` | 0.62 | 1.35 | 1.21 | 3.31 | 11.16 | 11.4 |
| `tour_apsp3` | 1.19 | 2.23 | 1.16 | 1.81 | 11.17 | 11.3 |
| `lava_e1m7` | 0.46 | 0.95 | 1.19 | 1.53 | 11.17 | 12.0 |
| `parallax_e1m1_qrp` | 0.45 | 1.01 | 1.13 | 1.43 | 11.15 | 11.3 |
| `slime_e1m1` | 0.49 | 1.03 | 1.11 | 1.57 | 11.17 | 12.0 |
| `tour_ad_soltower1e` | 0.73 | 1.19 | 1.07 | 1.93 | 11.15 | 11.2 |
| `teleporter_start_off` | 0.50 | 1.03 | 1.02 | 1.31 | 11.27 | 12.1 |
| `idle_range` | 0.66 | 1.32 | 0.99 | 1.37 | 11.24 | 11.9 |
| `parallax_e1m1_qrp_off` | 0.45 | 0.84 | 0.91 | 1.19 | 11.16 | 11.5 |
| `teleporter_start_flat` | 0.80 | 1.23 | 0.52 | 0.75 | 11.16 | 11.3 |
| `water_range_under` | 0.47 | 0.88 | 0.71 | 0.93 | 11.17 | 11.4 |
| `water_range_surface` | 0.47 | 0.77 | 0.62 | 0.91 | 11.12 | 11.3 |
| `idle_e1m1_flat` | 0.43 | 0.89 | 0.25 | 0.26 | 11.34 | 13.6 |
| `timedemo_demo1_flat` | 0.20 | 2.97 | 0.25 | 0.39 | 3.26 | 6.0 |

(CPU work: the host frame less the runtime's waits and the swap; GPU 3D: both eyes and the mirror. The worst frame
of a gore scenario is its blow, of `gore_first_cuts` a first spawn: see Hitches.)

### GPU

`combined` (vr_profile, every frame timed; ms a frame): **particles 7.5** (3.7 per eye), dynamic lights' shadow maps
1.7 (bench JSON: 6.8 in the paced run), world and brush models 1.8, alias models 1.1, translucent 0.26, bloom and
postprocess 0.22. `particles_dense`: particles 6.0 of 7.0. Nsight Systems on `combined` (6 s, about 400 frames):
`glDrawArrays` (the particles' and full-screen passes) 44% of the GPU's busy time, the world's
`glMultiDrawElementsIndirect` 20%, `glDrawElements` 11%; the GPU busy the whole capture. The particles' cost is the
retro particles drawn at full resolution (the half-resolution path is closed to retro particles): the 10-05 benchmark
measured the same and its options stand (decision list).

### CPU

Main thread in `combined` (VTune, window only, 900 frames): 8.0 s, of which **4.7 s spinning in
`glClientWaitSync`** (`GL_AcquireFrameResources`: the GPU two frames behind) and 0.7 s in the present; about 3 ms a
frame of real work. The GL driver's own thread 8.3 s. No function above 0.25 s besides those. `vr_profile`'s scopes
(ms a frame): **shadow alias draw 1.37** (48 calls: 8 lights x 6 faces, each drawing the casters in that face), Box3D
step 0.79, particle light 0.42 (now half), Box3D write-back 0.23, alias 0.22 + 0.19 (the eyes), QuakeC 0.16. Nsight
Systems counts about 7900 GL calls a frame, 4500 `glDrawElementsInstanced` (the shadow faces' alias casters) and 3000
`glDrawElements` (their world and brush casters).

**Fixed** (`vr_particles.cpp`, commit "Particles' retro light levels ..."): each lit particle's retro light level
took two `powf` calls (0.23 s of the window in VTune); now a square root and a product (the same formula: levels even
in brightness). `vr_particle_light_report` on `particles_dense` (6500 particles, 2 runs each): 0.197 to 0.105 ms a
frame. Screenshots: the same but for 1/255 on 0.08% of the pixels (a light exactly at a level's edge rounding the
other way; run-to-run noise is 9 pixels).

### Hitches

- **Gore crowd blows** (18-28 ms, one frame each, twice): `vr_gore_test_crowd` cuts 32 bodies in one server frame:
  QuakeC 16.4 ms for the cuts, then the next frame's Box3D step with the 32 new ragdolls and limbs (17-25 ms, "box3d
  step"). A stress test: a rocket into a crowd makes a handful.
- **First spawn of a monster kind in play** (ogre, enforcer 27 ms, death knight 17 ms: `gore_first_cuts`'
  `spawn1_*` marks): their heads and missiles precached at the spawn ("quake physics" in the hitch log), known
  (CPU_PERFORMANCE_FOLLOWUP_2026-10-03.md, "dispensers"); the first cuts themselves are now 10-12 ms.
- **The memory log's row** (`vr_memstats_log`, default every 60 s): 2-4 ms on the main thread ("memory log": its
  GPU memory query waits for the driver's thread). A minute's spike.

## After the fixes (the whole suite again)

`kit/benchresults/prof_final` (one run each, the final build; six scenarios that overlapped another agent's build were
run again). **Map loading** (ms; "hulls" is the time the load waited for them):

| load | before | after | change | hulls before | hulls after |
|---|---|---|---|---|---|
| map e1m1 (cold) | 734 | 724 | -1% | 0 | 0 |
| map e1m1 (warm) | 241 | 221 | -8% | 33 | 9 |
| restart e1m1 | 221 | 208 | -6% | 28 | 15 |
| map e1m1, QRP (cold / warm) | 1320 / 815 | 1317 / 801 | 0 / -2% | 0 | 0 |
| changelevel e1m2 / back to e1m1 | 470 / 234 | 359 / 221 | -24 / -5% | 0 / 29 | 0 / 6 |
| map e4m7 (cold / warm) | 1252 / 338 | 913 / 319 | -27 / -6% | 0 / 105 | 0 / 73 |
| map hip1m1 (cold / warm) / back to e1m1 | 1088 / 345 / 963 | 854 / 318 / 957 | -22 / -8 / -1% | 0 / 101 | 0 / 67 |
| map warden (cold / warm) | 2410 / 3291 | 2048 / 1820 | -15 / -45% | 961 / 2613 | 759 / 1305 |
| map ad_grendel (cold / warm) | 1808 / 2292 | 1415 / 1173 | -22 / -49% | 369 / 1572 | 110 / 612 |

The hull fix accounts for the big maps' and the warm loads' gains (the hulls column; the controlled A/B above is the
measure: -15 to -20%). The cold loads of e4m7, hip1m1 and e1m2 waited for no hulls: their 22-27% is the OS's file
cache and run-to-run spread of a single run, not a change; the baseline's warm warden (3291) was a slow outlier
(2046-2255 in the A runs).

**Gameplay**: no fix changes the GPU's work, and the CPU's changes are small (0.1 ms of particle light), so
before/after differences here are mostly noise, and the GPU's is larger than expected: this RTX 4090 at partial load
(a 90 Hz frame cap, the GPU busy 10-70% of each frame) runs at one of two clock states from run to run, about 13%
apart. `particles_dense` four times in a row with the final build: GPU 8.01, 7.08, 8.05, 7.08 ms (particles 6.9 / 6.1);
CPU work 1.8 / 0.8 ms with it, because a slower GPU makes the frame's fence wait spin longer. The firing-range
scenarios' GPU +10-30% in `prof_final` is that (the final run drew the slow state on most of them; e1m1's and the
tours' are within 2-15% either way). Locking the GPU's clocks (`nvidia-smi -lgc`, administrator) would remove it;
until then compare GPU times from repeats, by phase, or with the engine's own per-step timers, as done for the fixes.
`combined`: CPU work 5.6 to 5.0 ms (p99 27 to 11), frame p99 27.4 to 24.4, GPU 15.5 to 15.7.

## Decision list (larger opportunities; for the author)

Ordered by return for the effort. "Gain" is what was measured or a bounded estimate from it.

| # | what | gain | effort | risk |
|---|---|---|---|---|
| 1 | **Done** (`vr_hull_keep`, "Hull build, follow-up"; was:) **Keep the compiled hulls across a reload of the same map** (a death's reload, `restart`, a load of a save on the map one is on): key `built`, `tree` and the monster trees by the BSP's checksum instead of the hunk's clipnode pointers, keep them over `MapChange` when the next map is the same | warden's reload ~1.8 s to ~0.3 s, ad_grendel ~1.2 s to ~0.3 s; e1m1 ~30 ms | medium (the caches' keys are pointers into the hunk; every holder re-pointed) | medium: a stale pointer is a crash; a test with `vr_hull_cachetest` and the hashes |
| 2 | **AO bakes: a disk cache** keyed by the model's hash (the bake is deterministic: its FNV is printed), like the normal maps' cache; and a faster bake (a grid or BVH over the pose's triangles instead of every triangle in reach) | 9-20 s of 4 threads after each first load a session gone; models get their AO at once | small (cache) / medium (BVH) | low |
| 3 | **Done (2026-10-07): `vr_shadow_layered`, LIGHTING.md "Layered shadow casters"**: `combined` 7818 to 966 draw calls a frame, shadow CPU 1.69 to 1.03 ms, shadow GPU 6.7 to 0.6 ms, frame p50 22.7 to 16.6 ms; the depth bit-identical. Was: **Shadow casters drawn once per light, not per face**: one layered pass (`gl_Layer`/viewport index from a geometry or vertex shader) or a multi-draw of all six faces; at least the casters' per-entity set-up (lerp, matrices, bones) once per light | CPU 1.4 ms a frame in `combined` to ~0.3; 7500 draw calls a frame fewer | medium-large | medium (shadow edges, the head's self-shadow, portals' lights) |
| 4 | **Retro particles' fill**: a reduced-resolution path for large soft particles with the retro quantisation (10-05 benchmark, item 1) | GPU 6-7.5 ms to ~3 in dense scenes (measured controls 13.3 to 3.0 ms at 2048 in the 10-05 run) | medium | visual: the author's call |
| 5 | (Partly done in "Hull build, follow-up": the brush models' prepare 230 to ~60 ms on warden; what is left there.) **Hull build, the rest**: fewer face copies (move pieces between units), `choose` on a sample, the brush models' trees in parallel per model (the merge machinery) | another 20-30% of the hulls; 230 ms to ~60 on every warden load | medium | low with the hashes as the test |
| 6 | **A faster allocator for the pool's threads** (mimalloc or per-thread arenas for the load's builders) | the load's remaining malloc/free (several seconds of CPU on warden) | medium (a vendored library) | low-medium |
| 7 | **World textures kept for a same-map reload** (QRP: the material maps too), or a decoded-image cache | e1m1 QRP warm 815 ms to ~300 | medium | memory (QRP's textures are large) |
| 8 | **Campaign switch keeps id1's alias models** (only the folders that change are flushed) | 600 ms + 400 ms of images on a switch | medium | a mod's own model left stale |
| 9 | **Gore crowd frames**: the ragdolls of a frame's kills made over two or three frames, or Box3D's first step with many new bodies split | 17-25 ms hitch to under 11 | medium | gameplay feel of a crowd kill |
| 10 | **Second-row monsters' models precached at the map's start** where the files exist (dispensers) | 17-27 ms first-spawn hitches | small | none |
| 11 | **Memory log**: `vr_memstats_log 0` by default, or a row only at map loads | a 2-4 ms spike a minute | trivial | loses the minute rows |
| 12 | **The GPU wait without spinning** (`glClientWaitSync` with a short timeout and a yield, or the fence waited for later) | a core freed when GPU-bound (power, the other cores' turbo); no frame time | small | latency if done wrong |

## Not performance, noted

- **The throw's estimate depends on the frame rate** (ROUND21, "Throws in bullet time: the way the controller
  moved", its caveat): the release and the fastest sample land on different points of the arc at 72, 90 or 120 Hz, so
  the same throw's direction moves by up to 14 degrees between 72 and 240 fps (synthetic arcs). Any change of the
  refresh rate (or a dropped frame at the release) changes throws. A fixed-time estimate (the controller's poses
  resampled at fixed times before the release) would make it independent of the rate.
- **VTune crashed the game once** (an unhandled 0x80000001, a guard page, at an address in no module, during a
  warden load under the profiler); the same capture again ran clean. Not seen without the profiler.
- **GPU clock states**: at partial load the GPU's timings move about 13% between runs (two clock states; see "After
  the fixes"). Worth locking the clocks for benchmark runs (administrator) or reading GPU numbers from repeats.
- **Bench runs**: a stopped `bench.sh` keeps running in the background (its loop survives the shell) and left a
  stale exclusive slot for 30 minutes; the partial second repeat it made while other runs were going is set aside
  (not in any table here).

## Reproduce

```
bash kit/bench.sh <agent> --scenarios all --reps 1 --label <name> --settings <his settings>
MODE=2 bash Misc/quakevr/bench/qvrprof.sh <agent> vtune-hotspots load_warden <dir>        # loads only
MODE=1 bash Misc/quakevr/bench/qvrprof.sh <agent> vtune-hotspots combined <dir>           # the window only
vtune -report top-down -r <dir>/vt -format csv -csv-delimiter tab > td.tsv; python Misc/quakevr/bench/vtune_attr.py td.tsv
PRE=pre.cfg POST=post.cfg MODE=0 bash Misc/quakevr/bench/qvrprof.sh <agent> none combined <dir>   # vr_profile 1 around it
NSYS_DELAY=16 NSYS_DUR=6 MODE=0 bash Misc/quakevr/bench/qvrprof.sh <agent> nsys combined <dir>
```
