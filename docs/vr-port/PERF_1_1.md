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
| 1 | `ab9692a4c` | The smoke-blow meter's capture device opened (and closed) on a pool worker (`jobs::async`; the worker joins COM's multithreaded apartment first, WASAPI's objects being COM's); taken in by `meterFrame` when open; the notes' own recording waits for a pending open or close first (SDL's device table is not guarded between threads); shutdown closes on the main thread. `vr_debug_gunfx 1` prints `gunfx mic meter open (rate N)` | `armed_mix32` with `vr_debug_gunfx 1`: "gunfx mic meter open (rate 16000)", no main-thread spike (after-capture below) |

## Needs his call (each changes behaviour or is a bigger job)

1. **Navmesh class builds in play on big maps** (`vr_nav_build_frames`, 18): on ad_tears a class's build takes 1.3-1.8 s,
   so taking it on frame 18 blocks the main thread ~100 ms (once per class per map when its disk cache file is missing:
   the first play of a map). Options: scale the landing frame with the map's size (e.g. the build's own measured time
   rounded up, kept deterministic per map) or let classes built in play land "when done" (`vr_nav_build_frames 0`, loses
   the same-game-with-or-without-cache guarantee). Trade: determinism of the first seconds against a 100 ms hitch.
2. **The 3x enemies stress**: `nav_hunt_e1m1_3x` is the one scene over budget (p95 13.4 ms CPU, Tracy build). The cost
   is the armed humanoids' per-frame pose QC times 76 plus 121 walkers. The levers, all quality trades: PERF_1102's list
   (the arms' IK of unseen humanoids every other frame; the pose keys once a frame), and the worn backpacks' place
   (`limbpiece` 30, 0.38 ms) only for packs in view or near.
3. **Mass ragdoll creation**: 0.7 ms a ragdoll on the frame of death; a cap of ragdolls made per frame (the rest a frame
   later) would flatten the 20-35 ms frames of a rocket into a crowd, at the price of bodies going limp a frame or two late.
4. **The disk cache's first write of a session prunes on the main thread** (2 ms per product folder, once a session):
   could move into the write job (care: it scans the folder the writes go into).

## Map loads

(pending: the load bench is running; see "What is left")

## What is left (for the coordinator)

- The ThinLTO before/after bench of the full set (`bash kit/bench.sh <agent> --scenarios combat_48,ai_crowd_64,
  idle_e1m1,fight_ad_tears,fight_ad_sepulcher,combat_48_bullettime,combat_48_spectator,crowd_ad_40,armed_mix32,
  ragdolls_32_active,gore_blast_crowd_32,play_e1m1_fight,perf1114 --reps 3 --settings <his cfg>`): not run (the
  captures above took the night's exclusive time; the fix touches no per-frame path, only the hitch).
- An allocation capture (`pf_cap.sh <scenario> <label> 1 1`: memory detail on) of the fights: the main thread makes
  10-60 heap allocations a frame at the median and 140-450 at p99 (`nav_hunt` 454); `vr_alloc_sites 300 40` names them.
