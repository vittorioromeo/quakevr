# Performance campaign for 1.1 (2026-11-14): Tracy CPU + GPU + QC, hitches, map loads

Branch `agent/profile` (from `next` at `844c8bd48`), worker tree `embedfix`. Autonomous overnight run. Earlier passes
this builds on: [PERF_1102.md](PERF_1102.md) (armed humanoids), [MULTI_SCENARIO_PROFILE.md](MULTI_SCENARIO_PROFILE.md),
BENCHMARKS.md ("Overnight profiling round"), [TRACY.md](TRACY.md).

**Short answer.** At his settings every scene holds 90 Hz at the median except the new 3x-enemies stress on E1M1
(`nav_hunt_e1m1_3x`: 121 awake monsters, CPU busy p50 8.4 / p95 13.4 ms). The GPU is nowhere near its budget (3D 1-5 ms).
The **hitches** are the useful finding: the worst recurring one in fights, "muzzle wisps" 16-23 ms on the main thread,
was not the shader compile it was once blamed on but the **smoke-blow microphone being opened** (WASAPI, 16-23 ms) each
time gun smoke drifts near the mouth after the meter had been closed for 10 s: fixed (commit 1). The rest of the
hitches are first-use or design waits, listed for him below. Per-frame work is spread thin (no single zone over 0.4 ms
a frame in any scene); the remaining per-frame cost is the armed humanoids' QC and the alias models' draw steps,
both already the subject of PERF_1102's list.

## Method

- **Captures**: `Release-Tracy` (callstack depth 0: the timing build; the depth-8 TracyFull adds a stack walk per zone and
  its sampling needs an elevated session), `vr_tracy_preset full` with **QC detail 2** (every QC function and the
  non-trivial builtins named), his `quakevr/ironwail.cfg` of 2026-11-14 exec'd first, the mock headset at 2048 eyes,
  paced 90 Hz (`-RealTime`), exclusive (`run.sh --exclusive`: alone on the machine), the kit's `tracy-capture`
  (external/tracy-v0.14.1-windows), `tracy-csvexport -u -e` (self times) and `-g` (GPU). Zones: the 600-frame bench
  window alone (`Benchmark begin` + the JSON's seconds), main-thread self ms a frame; the worst frames by the gap between
  consecutive `2D` zones and what ran inside them; the GPU zones from 30 ms before each.
- **Scripts** (worktree `scratch/`, kept): `pf_cap.sh <scenario> <label> [qc] [mem] [frames]` (one capture, its CSVs
  to a shared overwritten folder: they are 600 MB each), `pf_camp.sh` (a list), `pf_zones.py` (the window's tables,
  `zones.txt` per capture), `pf_sum.py` / `pf_table.py` (the summaries below), `pf_parents.py` (a zone's time by its
  parent chain, from the inclusive export), `pf_loads.py` (a bench JSON's loads by stage), `pf_run.sh` (a scenario's
  script once with extra commands first), `pf_build.sh` (ThinLTO + Release-Tracy).
- **New scenarios** (group `perf1114`, `Misc/quakevr/bench/qvrbench.py`; validated twice, the same counts):
  `mut_armed3x_e1m2` (E1M2 skill 2, Enemy Count 3x, Armed Humanoids, Wild Weaponry, Armored Foes, Ranged Knights,
  Smarter AI, `vr_run_seed 7`, every monster woken onto the player, shotgun every 50 frames: 112 monsters),
  `mut_random3x_e1m2` (the same with Randomized Enemies and Pickups instead of the armed mutators: 119),
  `nav_hunt_e1m1_3x` (E1M1 3x, Smarter AI, lift and teleporter links on, slots 8, all woken onto the player in the
  computer room: 121 hunting across the map), `menu_maplib_e1m1` (the Map Library page over E1M1), `menu_preview_e1m1`
  (the VR settings with Live Preview: the game runs under them). Ragdolls, gore and props: the existing
  `ragdolls_32_active`, `gore_blast_crowd_32`, `armed_mix16_corpses`, `props_500_active`.

## Scenarios (Release-Tracy, preset full, QC detail 2, ms; heap allocs = main-thread new/malloc a frame)

| scenario | CPU busy p50 / p95 / p99 / max | frame p99 / max | GPU 3D avg | >14 / >33 ms | heap allocs/f p50 / p99 | monsters |
|---|---|---|---|---|---|---|
| ai_crowd_64 | 4.29 / 5.27 / 6.22 / 7.6 | 12.2 / 16.5 | 0.99 | 2 / 0 | 33 / 198 | 65 |
| armed_mix32 | 4.60 / 5.89 / 7.10 / 22.0 | 11.3 / 22.1 | 3.12 | 1 / 0 | 9 / 172 | 33 |
| combat_48 | 4.47 / 5.50 / 6.10 / 8.2 | 11.3 / 11.6 | 3.58 | 0 / 0 | 23 / 194 | 49 |
| combat_48_bullettime | 4.18 / 5.33 / 6.74 / 8.1 | 13.1 / 21.0 | 2.95 | 5 / 0 | 14 / 139 | 49 |
| combat_48_spectator | 4.69 / 5.94 / 6.59 / 9.7 | 11.3 / 12.1 | 3.79 | 0 / 0 | 23 / 195 | 49 |
| crowd_ad_40 | 4.64 / 5.81 / 6.69 / 24.1 | 11.3 / 24.5 | 5.21 | 1 / 0 | 39 / 261 | 37 |
| fight_ad_tears | 4.13 / 5.19 / 6.44 / 106.4 | 12.1 / 106.5 | 2.31 | 4 / 1 | 15 / 142 | 813 |
| gore_blast_crowd_32 | 4.19 / 7.35 / 8.42 / 33.5 | 11.3 / 33.6 | 3.14 | 2 / 1 | 57 / 298 | 33 |
| idle_e1m1 | 1.97 / 2.27 / 2.43 / 3.1 | 11.3 / 11.4 | 1.62 | 0 / 0 | 2 / 128 | 23 |
| menu_maplib_e1m1 | 2.17 / 2.71 / 3.33 / 3.8 | 11.5 / 14.9 | 1.70 | 2 / 0 | 22 / 148 | 23 |
| menu_preview_e1m1 | 2.23 / 2.88 / 3.22 / 3.5 | 13.3 / 16.7 | 1.70 | 4 / 0 | 12 / 138 | 23 |
| mut_armed3x_e1m2 | 5.98 / 8.72 / 9.50 / 24.8 | 11.2 / 25.0 | 2.58 | 1 / 0 | 12 / 256 | 112 |
| mut_random3x_e1m2 | 3.76 / 5.79 / 7.06 / 27.2 | 11.3 / 27.3 | 2.88 | 1 / 0 | 20 / 291 | 119 |
| nav_hunt_e1m1_3x | 8.44 / 13.44 / 15.01 / 30.3 | 15.2 / 30.4 | 3.59 | 22 / 0 | 12 / 454 | 121 |
| ragdolls_32_active | 4.63 / 7.45 / 8.29 / 33.0 | 11.3 / 33.1 | 3.47 | 1 / 0 | 49 / 208 | 1 |

(Frame p50 is the 11.1 ms pacing everywhere. The Tracy build's CPU numbers run ~0.3-0.5 ms over the ThinLTO exe's.)

## Where the time goes (main thread, self ms a frame)

- **Fights** (`combat_48`, `armed_mix32`, `crowd_ad_40`, spectator, bullet time): no zone over 0.35 ms. The alias models'
  per-model CPU steps together (`alias model` 600 calls a frame, `alias flush`, `vr instance`, `transform`, `light point`,
  `smooth`, `shadow model`) are about **0.9-1.1 ms** a frame; the armed humanoids' QC (`VR_AGP_Pose` 0.09-0.12,
  `VR_AGPar_Frame`, `VR_AGK_Keys`, `VR_AGP_TurnDir/TurnPose`, `VR_AM_Row`) about 0.5 ms for 20-30 of them; the job
  pool's dispatch and join 0.12-0.16; `ao occluders` 0.09-0.11; `findradius` 0.07-0.13 (60-90 calls); `text3d layout`
  0.07-0.10 (2.7 a frame: the boards and texts laid out per view). QC in all: 1.2-1.8 ms.
- **3x crowds** (`nav_hunt_e1m1_3x`, 121 woken: QC **4.5 ms**): `limbpiece` 0.38 (310 calls: the worn backpacks' place,
  4 per wearer, already one computation each), `VR_AGP_Pose` 0.36 (76 humanoids), `findradius` 0.16 (223 calls),
  `VR_AGP_ArmsGun`, `TurnPose`, `TurnDir`, `AGK_Keys` ~0.11 each, `VR_AM_Row` 0.10 (1265 calls), `navsteer` 0.10 (13
  calls, up to 0.9 ms in one frame), `modelanchorpoint` 0.09 (762 calls). Spread over the per-humanoid pose QC: the
  cost is the count. `mut_armed3x_e1m2` (112, 50 armed in view): QC 2.9 ms, the same names.
- **ad_tears** (813 monsters, 4,020 edicts): `SV edicts` 0.23, `dlight brush casters` 0.18 (7 a frame), `rigid bodies` 0.16
  (390 calls), `box3d sync entities` 0.13; QC only 0.57 (most monsters asleep).
- **Menus**: the Map Library and the settings page add ~0.2 ms of CPU (`2D` 0.09) and ~0.08 ms of GPU over idle E1M1.
- **GPU** (eyes 2048, 3D): fights 3.0-3.8 ms (alias 1.2-1.7: world models 1.1-1.5; world+brush 0.9-1.2; the spectator
  view 0.6-0.7; sky 0.34 on the firing range), `crowd_ad_40` 5.2 (world+brush 2.2), ad_tears 2.3, idle 1.6.
- **Other threads**: the armed arms' IK (`armed pose: arms solve`, 0.6-1.0 ms of worker time a frame in fights) and
  Box3D's tasks; nothing waits on them except as listed under hitches.

## Hitches (worst frames, what ran in them)

| where | worst | cause | status |
|---|---|---|---|
| fights with guns (`armed_mix32`, `crowd_ad_40`, `mut_*`, `nav_hunt`) | 16-23 ms `muzzle wisps` | `voicenotes::wantMicLevel`: `SDL_OpenAudioDevice` of the capture device for the smoke-blow meter (`vr_smoke_blow`), on the main thread, whenever smoke came within twice the reach of the mouth and the meter had closed (10 s after its last want). The 2026-11-07 note blamed the wisps' shader compile (since precompiled) | **fixed** (commit 1): opened and closed on a worker |
| `fight_ad_tears` | 106-118 ms, `Job blocking wait` 95 ms in `SV physics frame end` | a navmesh size class built in play (`vr_nav.cpp` buildClass, 1.3-1.8 s on a worker on this 85 MB map); `vr_nav_build_frames 18` makes the server take it on exactly its 18th frame (determinism), so the main thread waits out the rest | his call (below) |
| ragdolls / gore (`ragdolls_32_active`, `gore_blast_crowd_32`) | 33-35 ms, `ragdoll create` 6-23 ms | 32 ragdolls made in one frame (0.7 ms each: Box3D bodies, bone hulls), the scenario's mass kill; 2-6 ms for a rocket's 3-8 | his call (spread over frames) |
| `fight_ad_tears` (a new place: the tour's `setpos`) | 21 ms | `SV edict retouch` 5.8 ms (the player's touch of the triggers where he lands), `gadget hologram` 12.5 ms + `Texture upload` 3.6 (the wrist hologram's first draw of a message: its target made, the driver's first draw), the nav disk cache's first write (`disk cache prune` 2.1 ms + `write prepare` 2.4 on the main thread) | listed (first uses) |
| `combat_48` (one capture of two) | 146 ms in `swap` | not in the second capture or the bench runs: the driver at the first frames of a cold shader cache, or the machine (other workers' runs); not reproduced | watch |
| several | 1.1-1.2 ms `blowline`, 0.9-1.1 `half size`, `haze`, `water` | single calls: the blood line's trace walk, the half-size particles' pass | fine |

## Applied (each its own commit; behaviour unchanged)

| # | commit | what | check |
|---|---|---|---|
| 1 | `ab9692a4c` | The smoke-blow meter's capture device opened (and closed) on a pool worker (`jobs::async`; the worker joins COM's multithreaded apartment first, WASAPI's objects being COM's); taken in by `meterFrame` when open; the notes' own recording waits for a pending open or close first (SDL's device table is not guarded between threads); shutdown closes on the main thread. `vr_debug_gunfx 1` prints `gunfx mic meter open (rate N)` | `armed_mix32` with `vr_debug_gunfx 1`: "gunfx mic meter open (rate 16000)". Re-captured (same build kind and settings): CPU busy max **armed_mix32 22.0 -> 11.9 ms, mut_random3x_e1m2 27.2 -> 8.4 ms**, frames over 14 ms 1 -> 0 in both; the open now shows as a 13.7-13.9 ms `Job execute` on a worker. A 4.7-5.8 ms `muzzle wisps` frame remains once a session: the first `ensureAudio` (`SDL_InitSubSystem(SDL_INIT_AUDIO)`), left on the main thread (SDL's init belongs there) |

### Second pass (2026-11-14, branch `agent/profile2`)

| # | commit | what | check |
|---|---|---|---|
| 2 | `010e91e97` | `vr_envmap.cpp` `buildWorld`: every world texture's smallest mip read back into one pixel-pack buffer, mapped once (one GPU wait, not one per texture) | ad_azad (ThinLTO, paced, exclusive), the readbacks: **207-217 ms -> 14-33 ms** (113 textures; cold and warm). The per-vertex colours bit-identical (an FNV of them before and after: `4c3de42e71e36e5a` start map, `329f3e380159a96c` ad_azad, both loads). What remains of the stage is the GPU's backlog from the load (a `glFinish` in front of it took 9-356 ms): waited here or at the next sync |
| 3 | `97aee6b6e` | Per-frame heap events: `avatar::posedMesh`'s per-surface vector kept (`mem::Scratch`, 7.6 a frame: the player's wounds binding); `monsterdata::findStandIn` looks up without a `za::String` (a transparent hash; the lunges' QC) | `vr_alloc_sites` over 300 bench frames (fast mode, his settings): **nav_hunt_e1m1_3x 31.6 -> 16.3**, combat_48 8.9 -> 8.0 main-thread heap events a frame. The rest: first-use cache fills (`grasp::shapeOf`, `barrelmask::geometryOf`, `disarmhull::make`: the 400-700-event peak frames), kept buffers growing, decals' own triangles, and the bench script's `wait` lines (`Cmd_TokenizeString`'s `Z_Strdup` per argument: 0.8-3 a frame) |
| 4 | `be8370be4` | Alias model load: `VR_SkinEdgesModel`'s two passes on the job pool by bands of 16 rows (each band walks every triangle over its own rows; the edge texels then sorted into the serial walk's order) | Tracy (Release-Tracy, E1M1 cold): `alias skin edges` was 109 ms of the main thread's load (0.36 ms a model, 304 models). Identical: a temporary compare against the serial walk in the same run, 304 models on E1M1 and 591 on ad_azad, 0 differ. New Tracy zones in the skins' normal maps (posted / committed / waited for, mips) |

**Alias model load, where the rest goes** (E1M1 cold, Release-Tracy, main thread self ms; `scratch/p2_selfsum.py`):
`skin normal map committed (done)` 215 (1,682 normal maps: their upload, label and bindless residency, median 97 us),
`Texture preparation` 183 (2,230 skins: palette conversion, mips down, upload, residency), `alias skins` 73,
`VR_DerivedModelFile` 77 (a temporary zone: six limb models built, 46 ms; the rest ~0.3 ms each), `alias replacement md5` 48 (the .md5/.md3
probes), `texture mips made` 36, `skin normal map posted` 37, `normal map source prepared` 43. The decode and the normal
maps' making are already on the jobs; what is left on the main thread is GL (uploads, `glMakeTextureHandleResidentARB`)
and per-skin CPU work of 30-100 us each: no single exact win left above ~40 ms.

**ad_tears' first frames after its load (113-476 ms): the props' navmesh batches**, not the class builds. With every
class read from the disk cache, `vr_nav_info` still shows 2-3 "deadlines waited for", worst 216-316 ms: each class's
first props batch (24 crates and barrels blocking: 40-54 tiles rebuilt with them as obstacles) costs **2.8-5.0 s of CPU
(tiles 70-200 ms each in ad_tears' dense rooms) and 270-437 ms on the whole pool**, all three classes' together, and
`vr_nav_job_frames 3` makes the server take each on its 3rd frame, so the main thread waits out the rest. (The class
builds land on frame 18 and are only waited for when their disk file is missing: the first play after a new exe.)
Not changed: the landing frame is the determinism contract (item 1 below).

### His calls (2026-11-14, branch `agent/perfpicks`: items 1-4 of "Needs his call", each a tunable)

| # | commit | what | check |
|---|---|---|---|
| 5 | `1bc1a031a` | Navmesh landing frames on big maps: a class's first props' batch on `vr_nav_job_frames_first` (45); a class's build or disk read on `vr_nav_build_frames` + walkable triangles / `vr_nav_build_frames_tris` (10,000: ad_tears 133 frames, E1M1 ~19). Pure functions of the map and the game's events | Seeded tests' outcome lines byte-identical over 2 runs alone and 4 runs in two lanes at once: nav_tele_metric `3c851856` (17/20), test 43 `8bb14908` (13/0), test 41 `b0a9e67d` (14/1, the dog's fail the same with both tunables 0). **fight_ad_tears max frame 140 / 26 / 24 -> 22 / 24 / 22 ms** |
| 6 | `215487499` | `vr_ragdoll_per_frame` 4: at most 4 new ragdolls a server frame, the rest queued oldest death first; knockdowns at once | ragdoll_test, saveload_test (ROUND25.md "perfpicks"). The bench's 32 kills fall in its set-up, before the window: not measured by it |
| 7 | `220fc2240` | Armed humanoids: unseen, far, idle holders posed every other server frame (`vr_agrunt_pose_lod` 1, `_near` 384, `_cone` 80); far unseen worn packs not placed (`vr_worn_backpack_lod_near` 1500); `VR_AM_Row` cached, `VR_AGP_TurnDir`'s rotation in builtin `turnrpy` | `profile_qc`, nav_hunt_e1m1_3x (fast, ThinLTO): **QuakeC 5.14 -> 4.01 ms a frame** (profiler on), `VR_AGP_Pose` 2.49 -> 1.46 incl. (77 -> 58 calls), `VR_AGP_TurnDir` 0.28 -> 0.02, `VR_AM_Row` 0.10 -> 0.02; `VR_WP_Place` 0.34 -> 0.33 (most packs near or in view). Pose tests' agtest lines identical LOD on / off |
| 8 | `1a95237d3` | The disk cache's first write: folder making and prune in its job (a per-product lock through both) | fight_ad_tears (the session's first write) |

Bench (ThinLTO, `--fast --reps 3`, exclusive; `kit/benchresults/pp_before`, `pp_after`, `pp_after2`; ms, the 3 runs pooled):

| scenario | frame p50 | frame p99 | frame max | CPU avg |
|---|---|---|---|---|
| fight_ad_tears | 2.85 -> 2.72 | 5.72 -> 4.27 | 140.1 -> 23.8 | 2.70 -> 2.56 |
| nav_hunt_e1m1_3x | 6.69 -> 6.08 | 12.71 -> 11.82 | 15.6 -> 17.2 | 6.38 -> 5.96 |
| combat_48 (*) | 3.78 -> 3.62-3.74 | 7.01 -> 6.34-7.19 | 12.7 -> 9.3-21.5 | 3.79 -> 3.67-3.75 |
| ragdolls_32_active (*) | 3.78 -> 3.35-3.53 | 12.59 -> 9.14-9.71 | 29.4 -> 15.3-26.3 | 4.05 -> 3.63-3.85 |

(*) after: only the runs with the GPU at its usual speed (3D 3.0-3.2 ms); 5 of 9 ran with every GPU pass ~70% slower
(3D 5.3-5.7 ms, CPU too: something else on the machine, OBS was recording), left out.

## Needs his call (each changes behaviour or is a bigger job)

Items 1-4 applied (above, "His calls").

1. **The navmesh's landing frames on big maps.** (a) Props' batches (`vr_nav_job_frames`, 3): on ad_tears the first
   batch per class needs 270-437 ms of the whole pool, so the main thread waits **216-316 ms** (fast mode; 113-476 ms paced).
   Proposal: a separate landing frame for a class's *first* props batch, `vr_nav_job_frames_first 45` (0.5 s at 90 Hz),
   later batches staying at 3: still a fixed frame (the same game with or without the cache), the waits gone on a
   16-thread machine, at the price of map-start crates blocking walkers' paths 0.5 s later (the monsters mostly asleep
   then). (b) Class builds in play (`vr_nav_build_frames`, 18; 1.1-1.8 s each on ad_tears, only when the disk file is
   missing: the first play of a map after a new build): proposal `vr_nav_build_frames` scaled by the map's walkable
   triangle count (e.g. 18 + count / 10,000: ad_tears' 1.15 M -> 133 frames, 1.5 s; E1M1 stays near 18), deterministic
   per map; gain the ~100 ms first-play hitch.
2. **The 3x enemies stress** (`nav_hunt_e1m1_3x`, p95 13.4 ms CPU, Tracy build: the one scene over budget). Proposal:
   the unseen armed humanoids' arms IK and pose keys every other frame (PERF_1102's lever, alternating halves by edict
   number): measured cost of the pose QC is `VR_AGP_Pose` 0.36 + `ArmsGun`/`TurnPose`/`TurnDir`/`AGK_Keys` ~0.44 ms
   a frame for 76 humanoids, about half of them unseen at any time, so **~0.4 ms a frame**; plus the worn backpacks'
   place (`limbpiece`, 0.38 ms) only for packs within 1,500 units or in view: **~0.25 ms**. Price: unseen humanoids'
   arms a frame stale (not visible), packs far away placed late when they come into view (one frame).
3. **Mass ragdoll creation** (0.7 ms a ragdoll on its death frame: 32 in one frame = 23 ms, a rocket into a crowd 2-6
   ms). Proposal: `vr_ragdoll_per_frame 4` (the rest made over the next frames, oldest death first): the 32-kill frame
   becomes 8 frames of ~3 ms (**worst frame 33 -> ~14 ms** in `ragdolls_32_active`); bodies 5-8 of a frame go limp
   1-7 frames (11-78 ms) late, playing their death frame meanwhile.
4. **The disk cache's first write of a session prunes on the main thread** (`disk cache prune` 2.1 ms + `write prepare`
   2.4 ms once a session, seen in `fight_ad_tears`). Proposal: the prune inside the first write's job, before its own
   write, with the folder's lock the writes already take; **gain 2-4.5 ms once a session**. Price: none visible; care
   that the prune never removes the file being written (it runs before, in the same job).
5. (New) **`Cmd_TokenizeString` allocates every argument** (`Z_Strdup`, freed at the next command): 0.8-3 heap events a
   frame in the benches (their `wait` lines), a few a frame in play (binds). Reusing per-slot buffers is safe for every
   correct caller but changes what a stale `Cmd_Argv` pointer held across a nested command reads: left as it is.

## Map loads

ThinLTO exe, his settings, exclusive, `bash kit/bench.sh <agent> --scenarios load_e1m1,load_e4m7,load_ad_azad,
load_ad_tears,load_ad_grendel --reps 2` (`kit/benchresults/pf_loads`; the disk caches warm from earlier runs of this
game folder, so "cold" is the process's first load, not an empty cache). Stages and kinds of work over 150 ms
(`scratch/pf_loads.py`), ms:

| load | total | biggest stages | biggest work | after the load: worst frame |
|---|---|---|---|---|
| e1m1 cold / warm / restart | 1782 / 192 / 149 | entities spawned 1105, view models 153 | alias models (.mdl) 1038 (307), their normal maps 497, skins 219, textures 170 (3569) | 18 / 6 / 6 |
| e4m7 cold / warm | 1941 / 417 | entities spawned 1078 | alias models 1008 (352), normal maps 470, skins 216 | 19 / 5 |
| ad_azad cold / warm (2 reps) | 2034-2551 / 947-1388 | entities spawned 516-525, **model light grid 1206** (rep 1), **env cube world 525-770 cold, 327-770 warm**, 2 frames (Box3D world) 150-161, smooth rigs 164 | alias models 341-351 (239), limb models 174-210 | 7-16 / 7-25 |
| ad_tears cold (2 reps) | 2958-3165 | world model (BSP) 457-491, entities spawned 716-867, baseline + VR after load 382-429, 2 frames 330-337, smooth rigs 257, liquids 174-189, light grid 186-222 | alias models 534-631 (387), PBR companion maps 337-353, image decodes 268-275, limb models 226-291, ledges 216-219, navmesh 156 | **113 / 476** |
| ad_grendel cold / warm | (its JSON holds the marks only: worst frame 64 ms) | | | |

Earlier today on the same tree with empty caches (`kit/benchresults/vb_jobs1`, ad_tears cold): 17.3 s, of it **the
hulls' build waited for 9.3 s** (`server: 2 frames`) and the **model light grid 5.1 s**, normal maps made from skins 3.2 s
on the jobs: all three are disk-cached afterwards (CACHES.md: `hulls`, `lightgrid`), so they are the first-ever load's.

Top costs and what could go:
1. **`env cube world` (vr_envmap.cpp `buildWorld`): 330-770 ms on every ad_azad load, warm ones too.** `averageColor` read
   each world texture's smallest mip back with `glGetTexImage` (a GPU sync per texture, ~2 ms each). **Done** (second
   pass, commit 2): one pixel-pack buffer, one map: the readbacks 207-217 -> 14-33 ms; the GPU backlog of the load
   (9-356 ms, measured with a `glFinish` in front) is still waited for at the first sync.
2. **Entities spawned** (QC spawn: 0.5-1.1 s; on e1m1 mostly the models it precaches: `alias models (.mdl) loaded`
   ~3.3 ms each with their normal maps). Warm loads keep them (192 ms).
3. **The first frames after ad_tears' load: 113-476 ms** (navmesh classes and other work landing in play: the hitch
   table's navmesh row).
4. The first-ever load of a big map (hulls 9.3 s, light grid 5.1 s, normal maps 3.2 s): disk-cached after it.

## Before/after bench of the day's perf work (2026-11-14, branch `agent/bench11`)

Before = `094dac831` (next just before the `nozone` merge; 1.1.0-beta4's tag is not on next), after = next at
`a2a1d5391` (nozone, vkperf, nohunk, hunkfollow, profile, profile2, and the day's other merges: classic2, menudrag,
campvis, iconpick, mpsp, partuniform...) plus the crash fix below. Both ThinLTO, `bench.sh --fast --reps 3`, exclusive,
his `quakevr/ironwail.cfg` (a copy, the same file for both), next's scenario list on both trees (the before tree got
next's `qvrbench.py`: the change adds scenarios only). The full set (PERF_1_1's 12 + `perf1114`) and every `load_*`
scenario; the scenarios that moved over 3% in the first pass were run again (3 more reps each, before and after in
one session each) and the tables are the medians over all 6 (`kit/benchresults/b1_before`, `b1_before2`, `b1_after`,
`b1_after2`; merged and tabled by `scratch/b1_merge.py`, `b1_table.py`). Each cell: before / after (change);
`**+**` worse by 3% or more past a floor (0.05 ms, 0.5 allocs, 1 ms for loads' frames, 10 ms for load totals),
`-` better.

**Headline.** No per-frame regression the repeats confirm: CPU and frame medians are within +-3% everywhere but
`crowd_ad_40` (median -12%, p99 +44%, below) and `fight_ad_tears` (+4%, +-9% between sessions of the same exe). The
worst frames are lower nearly everywhere (idle_e1m1 33 -> 9 ms, play_e1m1_fight 21 -> 8, nav_hunt 34 -> 16,
armed_mix32 47 -> 22: the zone and hunk work). Warm and restart loads are faster (ad_azad warm -33%, ad_sepulcher
restarts -20/-27%, hip1m1 -> e1m1 campaign switch -34%, e4m7 warm -15%). **One crash found and fixed**: `load_reloads`
crashed on every run after (below). Session-to-session noise is large on this machine in fast mode (the same exe's
`menu_maplib_e1m1` 2.26 then 1.55 ms p50; `combat_48_spectator` before 3.85 then 5.57): single 3-rep numbers under
~10% are not evidence.

**The crash (fixed, `host.c`).** `map ad_grendel` after warden (a campaign switch: `VR_CanLoadCampaignMap` ->
`COM_SwitchGameInternal` -> `CL_ForgetModels`) wrote `cl_entities[i].model` into freed memory: `cl_entities` is on the
map's hunk, `Host_ClearMemory` freed it but kept the pointer. Before nohunk the hunk was one block and the write landed
in it; since nohunk its blocks are the heap's and a large one is unmapped when freed (access violation, 3 of 3 runs).
`Host_ClearMemory` now forgets it (`cl_entities = NULL`, `cl_max_edicts = 0`: the start-up state, which
`CL_ForgetModels` already checks). After: load_reloads, load_changelevel, load_hip1m1, load_ad_restarts and
saveload_e1m1 all complete (`kit/benchresults/b1_fixchk`), then load_reloads 3 of 3 in `b1_after2`.

### Frames (ms; heap allocs = main-thread allocations a frame)

| scenario | frame p50 | frame p99 | frame max | CPU busy avg | CPU busy p99 | GPU 3D avg | heap allocs/f avg | heap p99 |
|---|---|---|---|---|---|---|---|---|
| ai_crowd_64 | 3.17 / 3.23 (+2%) | 4.84 / 5.02 (+4%) **+** | 8.4 / 6.6 (-21%) - | 2.89 / 2.97 (+3%) | 4.79 / 4.96 (+3%) **+** | 0.86 / 0.86 (-1%) | 6.7 / 6.3 (-6%) | 105 / 105 (+0%) |
| armed_mix32 | 3.73 / 3.75 (+1%) | 6.00 / 6.14 (+2%) | 46.5 / 22.0 (-53%) - | 3.60 / 3.60 (-0%) | 5.95 / 6.10 (+2%) | 2.91 / 2.76 (-5%) - | 4.2 / 6.2 (+50%) **+** | 97 / 93 (-4%) - |
| combat_48 | 3.62 / 3.59 (-1%) | 11.79 / 10.75 (-9%) - | 17.3 / 14.6 (-16%) - | 3.76 / 3.74 (-0%) | 11.74 / 10.70 (-9%) - | 3.49 / 3.46 (-1%) | 7.3 / 7.5 (+2%) | 106 / 107 (+1%) |
| combat_48_bullettime | 3.02 / 3.07 (+1%) | 6.03 / 5.74 (-5%) - | 10.9 / 9.4 (-14%) - | 2.90 / 2.97 (+2%) | 5.98 / 5.71 (-5%) - | 2.59 / 2.58 (-0%) | 1.4 / 2.1 (+49%) **+** | 17 / 19 (+12%) **+** |
| combat_48_spectator | 3.90 / 3.79 (-3%) | 13.99 / 13.76 (-2%) | 29.2 / 16.2 (-45%) - | 4.06 / 4.02 (-1%) | 13.95 / 13.71 (-2%) | 3.77 / 3.75 (-0%) | 7.3 / 7.5 (+3%) | 108 / 108 (+0%) |
| crowd_ad_40 | 3.90 / 3.43 (-12%) - | 6.64 / 9.59 (+44%) **+** | 23.4 / 17.8 (-24%) - | 3.84 / 3.42 (-11%) - | 6.60 / 9.54 (+45%) **+** | 3.35 / 3.09 (-8%) - | 11.7 / 12.8 (+9%) **+** | 124 / 155 (+26%) **+** |
| fight_ad_sepulcher | 2.51 / 2.45 (-2%) | 4.15 / 4.39 (+6%) **+** | 11.4 / 8.2 (-28%) - | 2.36 / 2.34 (-1%) | 4.09 / 4.34 (+6%) **+** | 1.80 / 1.81 (+0%) | 0.5 / 2.1 (+351%) **+** | 8 / 10 (+18%) **+** |
| fight_ad_tears | 2.87 / 3.00 (+4%) **+** | 4.69 / 4.57 (-3%) | 29.4 / 28.3 (-4%) - | 2.75 / 2.85 (+4%) **+** | 4.63 / 4.52 (-2%) | 1.94 / 1.95 (+0%) | 1.4 / 3.3 (+139%) **+** | 10 / 10 (+0%) |
| gore_blast_crowd_32 | 2.85 / 2.82 (-1%) | 7.18 / 7.29 (+1%) | 33.5 / 31.6 (-6%) - | 3.16 / 3.15 (-0%) | 6.96 / 6.87 (-1%) | 2.49 / 2.49 (+0%) | 10.2 / 10.5 (+3%) | 53 / 42 (-21%) - |
| idle_e1m1 | 1.46 / 1.47 (+1%) | 4.39 / 4.31 (-2%) | 33.0 / 8.9 (-73%) - | 1.47 / 1.47 (-1%) | 4.36 / 4.28 (-2%) | 1.31 / 1.31 (+0%) | 0.0 / 0.8 (+2057%) **+** | 2 / 3 (+50%) **+** |
| load_ad_grendel | 0.05 / 0.05 (+0%) | 0.37 / 0.39 (+6%) | 61.2 / 67.8 (+11%) **+** | 0.24 / 0.26 (+8%) | 0.24 / 0.38 (+59%) **+** | 0.00 / 0.00 (+0%) | 23.1 / 25.7 (+11%) **+** | 0 / 1 (+0%) |
| menu_maplib_e1m1 | 1.54 / 1.57 (+2%) | 4.66 / 4.71 (+1%) | 8.3 / 8.6 (+3%) | 1.56 / 1.64 (+5%) **+** | 4.63 / 4.67 (+1%) | 1.36 / 1.37 (+0%) | 19.9 / 20.7 (+4%) **+** | 24 / 24 (+0%) |
| menu_preview_e1m1 | 1.61 / 1.56 (-3%) - | 4.99 / 4.77 (-5%) - | 9.4 / 9.1 (-3%) | 1.65 / 1.56 (-6%) - | 4.96 / 4.74 (-5%) - | 1.43 / 1.37 (-4%) - | 9.8 / 10.6 (+8%) **+** | 12 / 13 (+8%) **+** |
| mut_armed3x_e1m2 | 4.74 / 4.74 (-0%) | 8.09 / 8.33 (+3%) **+** | 22.3 / 15.3 (-31%) - | 4.55 / 4.57 (+1%) | 8.03 / 8.28 (+3%) **+** | 2.31 / 2.32 (+0%) | 8.3 / 8.5 (+2%) | 224 / 60 (-73%) - |
| mut_random3x_e1m2 | 2.78 / 2.76 (-1%) | 7.48 / 7.61 (+2%) | 22.8 / 14.3 (-37%) - | 2.79 / 2.73 (-2%) | 7.43 / 7.56 (+2%) | 2.49 / 2.43 (-2%) | 6.5 / 8.6 (+32%) **+** | 64 / 66 (+5%) **+** |
| nav_hunt_e1m1_3x | 6.73 / 6.79 (+1%) | 11.98 / 12.16 (+1%) | 34.0 / 16.3 (-52%) - | 6.29 / 6.36 (+1%) | 11.89 / 12.09 (+2%) | 3.12 / 3.10 (-1%) | 10.2 / 7.0 (-32%) - | 217 / 86 (-60%) - |
| play_e1m1_fight | 2.80 / 2.84 (+2%) | 4.50 / 4.51 (+0%) | 21.4 / 8.1 (-62%) - | 2.55 / 2.58 (+1%) | 4.42 / 4.45 (+1%) | 2.06 / 2.03 (-1%) | 4.8 / 4.9 (+2%) | 118 / 47 (-60%) - |
| ragdolls_32_active | 3.30 / 3.31 (+0%) | 7.17 / 7.40 (+3%) **+** | 30.9 / 33.2 (+7%) **+** | 3.44 / 3.53 (+2%) | 7.13 / 7.34 (+3%) | 2.83 / 2.85 (+1%) | 5.9 / 6.8 (+16%) **+** | 44 / 47 (+7%) **+** |

### Map loads (ms; the scenario's loads in order, median of the repeats)

| scenario | load | total ms | first frame ms | worst frame after |
|---|---|---|---|---|
| load_ad_azad | map ad_azad: ad_azad | 2010 / 1771 (-12%) - | 9.8 / 14.7 (+50%) **+** | 12.0 / 24.0 (+99%) **+** |
| load_ad_azad | map ad_azad: ad_azad | 940 / 632 (-33%) - | 10.5 / 16.9 (+60%) **+** | 12.7 / 14.2 (+11%) **+** |
| load_ad_restarts | map ad_sepulcher: ad_sepulcher | 2028 / 1982 (-2%) | 18.9 / 18.4 (-3%) | 17.6 / 17.9 (+1%) |
| load_ad_restarts | restart ad_sepulcher: ad_sepulcher | 1131 / 900 (-20%) - | 18.3 / 16.4 (-11%) - | 8.7 / 10.7 (+22%) **+** |
| load_ad_restarts | restart ad_sepulcher: ad_sepulcher | 1274 / 927 (-27%) - | 16.5 / 16.6 (+0%) | 6.6 / 20.8 (+216%) **+** |
| load_ad_restarts | map ad_tfuma: ad_tfuma | 1277 / 1271 (-0%) | 11.3 / 11.3 (-0%) | 9.1 / 8.9 (-2%) |
| load_ad_restarts | restart ad_tfuma: ad_tfuma | 487 / 482 (-1%) | 7.2 / 7.1 (-1%) | 11.7 / 26.1 (+124%) **+** |
| load_ad_restarts | restart ad_tfuma: ad_tfuma | 416 / 422 (+1%) | 6.9 / 7.0 (+2%) | 29.6 / 13.5 (-54%) - |
| load_ad_sepulcher | map ad_sepulcher: ad_sepulcher | 2055 / 2038 (-1%) | 18.9 / 19.7 (+5%) | 23.5 / 24.6 (+5%) **+** |
| load_ad_sepulcher | map ad_sepulcher: ad_sepulcher | 1003 / 840 (-16%) - | 16.4 / 16.8 (+3%) | 12.2 / 10.6 (-13%) - |
| load_ad_tears | map ad_tears: ad_tears | 2397 / 2369 (-1%) | 21.7 / 21.7 (-0%) | 27.6 / 28.1 (+2%) |
| load_ad_tfuma | map ad_tfuma: ad_tfuma | 1386 / 1409 (+2%) | 12.6 / 12.8 (+1%) | 16.7 / 16.6 (-1%) |
| load_ad_tfuma | map ad_tfuma: ad_tfuma | 460 / 487 (+6%) **+** | 7.1 / 7.4 (+4%) | 9.2 / 8.9 (-3%) |
| load_changelevel | changelevel e1m2: e1m2 | 379 / 375 (-1%) | 4.9 / 4.6 (-6%) | 5.9 / 5.1 (-13%) |
| load_changelevel | changelevel e1m1: e1m1 | 184 / 187 (+2%) | 2.5 / 2.6 (+6%) | 7.7 / 7.0 (-10%) |
| load_e1m1 | map e1m1: e1m1 | 1596 / 1692 (+6%) **+** | 67.4 / 68.3 (+1%) | 18.3 / 18.9 (+3%) |
| load_e1m1 | map e1m1: e1m1 | 188 / 186 (-1%) | 3.3 / 3.4 (+3%) | 4.8 / 5.7 (+19%) |
| load_e1m1 | restart e1m1: e1m1 | 141 / 150 (+6%) | 3.0 / 2.7 (-10%) | 7.7 / 5.5 (-28%) - |
| load_e1m1_qrp | map e1m1: e1m1 | 1836 / 1865 (+2%) | 69.8 / 69.7 (-0%) | 19.4 / 19.1 (-1%) |
| load_e1m1_qrp | map e1m1: e1m1 | 329 / 337 (+3%) | 3.6 / 3.3 (-9%) | 6.4 / 5.4 (-15%) |
| load_e4m7 | map e4m7: e4m7 | 2006 / 1875 (-7%) - | 70.1 / 66.6 (-5%) - | 20.1 / 19.7 (-2%) |
| load_e4m7 | map e4m7: e4m7 | 478 / 407 (-15%) - | 5.6 / 5.7 (+1%) | 3.1 / 5.2 (+68%) **+** |
| load_hip1m1 | map hip1m1: hip1m1 | 1995 / 1807 (-9%) - | 68.7 / 66.2 (-4%) - | 20.3 / 19.6 (-4%) |
| load_hip1m1 | map hip1m1: hip1m1 | 336 / 341 (+2%) | 6.3 / 6.9 (+8%) | 4.6 / 4.6 (+0%) |
| load_hip1m1 | map e1m1: e1m1 | 1284 / 850 (-34%) - | 7.0 / 6.7 (-4%) | 5.7 / 5.5 (-2%) |
| load_reloads | map warden: warden | 2949 / 2846 (-4%) - | 77.3 / 76.5 (-1%) | 31.3 / 30.9 (-1%) |
| load_reloads | restart warden: warden | 675 / 659 (-2%) | 7.7 / 9.8 (+26%) **+** | 6.9 / 7.4 (+7%) |
| load_reloads | map warden: warden | 468 / 586 (+25%) **+** | 12.6 / 10.9 (-13%) - | 12.3 / 12.1 (-2%) |
| load_reloads | map e1m1: e1m1 | 943 / 895 (-5%) - | 6.4 / 6.6 (+3%) | 4.5 / 4.5 (-2%) |
| load_reloads | restart e1m1: e1m1 | 154 / 159 (+4%) | 2.5 / 2.6 (+2%) | 6.9 / 6.6 (-4%) |
| load_reloads | changelevel e1m2: e1m2 | 308 / 315 (+2%) | 4.6 / 4.8 (+4%) | 4.5 / 3.1 (-31%) - |
| load_reloads | changelevel e1m1: e1m1 | 172 / 179 (+4%) | 2.6 / 2.8 (+9%) | 6.9 / 4.5 (-36%) - |
| load_session_qrp | map start: start | 1779 / 1781 (+0%) | 69.5 / 67.0 (-4%) - | 30.9 / 31.6 (+2%) |
| load_session_qrp | changelevel e1m1: e1m1 | 437 / 443 (+1%) | 5.1 / 3.3 (-35%) - | 5.9 / 5.6 (-4%) |
| load_session_qrp | changelevel e1m2: e1m2 | 487 / 493 (+1%) | 4.4 / 4.3 (-1%) | 5.4 / 5.8 (+7%) |
| load_session_qrp | changelevel e1m1: e1m1 | 322 / 334 (+4%) **+** | 2.7 / 2.9 (+6%) | 5.8 / 6.0 (+3%) |
| load_session_qrp_flat | map start: start | 1808 / 1765 (-2%) | 62.3 / 60.8 (-2%) | 6.8 / 7.2 (+6%) |
| load_session_qrp_flat | changelevel e1m1: e1m1 | 438 / 500 (+14%) **+** | 6.7 / 2.7 (-60%) - | 3.5 / 3.6 (+4%) |
| load_session_qrp_flat | changelevel e1m2: e1m2 | 483 / 500 (+4%) **+** | 4.6 / 4.0 (-13%) | 3.4 / 3.1 (-10%) |
| load_session_qrp_flat | changelevel e1m1: e1m1 | 322 / 351 (+9%) **+** | 2.7 / 2.6 (-3%) | 3.0 / 3.0 (+1%) |
| load_vrstart | map vrstart: vrstart | 2720 / 2615 (-4%) - | 76.1 / 71.0 (-7%) - | 22.9 / 22.4 (-2%) |
| load_vrstart | map vrstart: vrstart | 661 / 650 (-2%) | 13.0 / 13.9 (+7%) | 6.3 / 5.9 (-7%) |
| load_vrstart | restart vrstart: vrstart | 582 / 487 (-16%) - | 5.2 / 8.6 (+65%) **+** | 5.9 / 6.5 (+10%) |
| load_warden | map warden: warden | 2968 / 2908 (-2%) | 74.7 / 75.9 (+2%) | 31.0 / 32.4 (+5%) **+** |
| load_warden | map warden: warden | 685 / 700 (+2%) | 8.8 / 9.8 (+12%) **+** | 7.8 / 8.6 (+11%) |

(`load_reloads`' ad_grendel loads are missing on both sides: `Couldn't spawn server maps/ad_grendel.bsp` in that
scenario's sequence, before and after alike; `load_ad_grendel` itself loads it. `load_ad_grendel`'s row in the frames
table is its load window.)

**Flagged, not fixed (listed):**
1. `crowd_ad_40` frame p99 6.6 -> 9.6 ms (both sessions: 6.6 -> 9.1, 6.9 -> 10.0) while its average (-11%), median
   (-12%), max (23 -> 18) and every CPU phase's average are lower. Frames over twice the median 2.5 -> 16 per 900:
   a periodic ~2x-median frame, in this scenario only (the hitch counts of the others are flat or lower). The fight
   also plays differently (particles 7365 -> 5490, awake bodies 201 -> 169, monsters alive 30.5 -> 32.3: a gameplay
   change among the day's merges), so it is not the same work. At 90 Hz pacing the profile capture of the same scene
   had CPU p99 6.7 ms (table above): under the budget. Needs a Tracy capture of the fast run to name it.
2. Heap allocations a frame up by 1-2 in `fight_ad_sepulcher` (0.5 -> 2.1), `fight_ad_tears` (1.4 -> 3.3),
   `armed_mix32` (4.2 -> 6.2), `mut_random3x_e1m2` (6.5 -> 8.6), `idle_e1m1` (0 -> 0.8): the hunk's temporary blocks
   now counted as heap allocations (nohunk) or the day's new per-frame work; p99s are flat or lower. `pf_cap.sh <sc>
   <label> 1 1` with `vr_alloc_sites` names them.
3. Loads, single-session (3 reps): `load_session_qrp_flat`'s changelevels +4-14% (the VR one, 6 reps: +1-4%),
   `load_e1m1` cold +6%, `load_reloads`' second `map warden` 468 -> 586; the first frames after `ad_azad`'s loads
   10 -> 15-17 ms and after `vrstart`'s restart 5 -> 9 ms (small, within a frame's budget at 90 Hz).

## What is left (for the coordinator)

- Second pass done: the env cube's averages in one readback, the allocation trace, the skin edges on the pool, the
  ad_tears post-load hitch's cause (the props' navmesh batches: his call 1a). Tests: `load_models_test.sh`,
  `scenario_check.sh` full and the release-config build after the merge (ROUND25.md "profile2").
- The ThinLTO before/after bench of the full set: done ("Before/after bench of the day's perf work" above; the load_reloads crash fixed; crowd_ad_40's p99 and the per-frame allocations listed).
- An allocation capture (`pf_cap.sh <scenario> <label> 1 1`: memory detail on) of the fights: the main thread makes
  10-60 heap allocations a frame at the median and 140-450 at p99 (`nav_hunt` 454); `vr_alloc_sites 300 40` names them.
