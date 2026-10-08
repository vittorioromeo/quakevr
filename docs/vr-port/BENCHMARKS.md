# Benchmarks: the scenario suite

A repeatable benchmarking and profiling round for every part of Quake VR: idle scenes, slipgates, combat, physics,
gore (crowds dismembered at once, limbs lying about, first-cut hitches), particles and decals, dynamic lights,
liquids, texture packs, menus, map loads (by phase, cold and warm) and complex custom maps, in VR (the
mock headset, both eyes drawn) and flat. Each scenario is a fixed script; each run writes one JSON; the runner repeats
them, summarises, and compares a baseline with new results.

- Engine: `vr_bench_begin` / `vr_bench_end` / `vr_bench_seed` (`Quake/vr/vr_bench.cpp`), fed by the profiler's
  always-on phases (no `vr_profile` needed, so the numbers are the game's, not the profiler's).
- Scenarios, summaries, comparison: `Misc/quakevr/bench/qvrbench.py` (versioned with the engine and the QuakeC: a
  scenario's commands are this tree's).
- Runner: the kit's `bench.sh` (`C:/OHWorkspace/qvr-kit/bench.sh`, with `bench_maps.ps1` and `bench_sheet.ps1`).

The older one-off suite (`Misc/quakevr/perf_suite.py`, `PERFORMANCE_BENCHMARK_20261005.md`, removed 2026-10-06; git history)
is the origin of the firing-range fixtures here; it needs a disposable base and parses the call tree's CSV. This suite
runs in the kit's game folders and records whole-frame percentiles itself. (Some of perf_suite's switches are stale:
`vr_retro_particles` no longer exists.)

## Running it

```
bash kit/bench.sh <agent> --validate [--scenarios ...]          # each scenario twice, fast, hidden, 120 frames
bash kit/bench.sh <agent> [--scenarios core] [--reps 3] [--frames 900] [--hz 90] [--eye 2048] [--label baseline]
bash kit/bench.sh compare kit/benchresults/baseline kit/benchresults/new [--threshold 5]
bash kit/bench.sh list [<group>]
```

- `<agent>`: any worktree with a kit game folder (`new_agent.sh <agent>`), or `cleanup`. The build in that tree is
  measured; build it first (`build.sh`).
- `--scenarios`: `all`, a group (`core`, `idle`, `slipgates`, `combat`, `melee`, `server`, `physics`, `gore`, `vfx`,
  `lights`, `liquids`, `textures`, `features`, `loading`, `maps`, `flat`, `control`), scenario names, or a comma list
  of them. Default `core` (13 scenarios, the round's quick picture).
- Timing runs (the default) are **exclusive** (`run.sh --exclusive`: nothing else runs on the machine) and **paced**
  like a 90 Hz headset (`-RealTime`, `host_maxfps` = `--hz`). `--fast` runs the frames unpaced (throughput, the way
  perf_suite measured), `--shared` drops the exclusivity (then the numbers mean nothing: for trying a scenario).
- `--eye`: the mock headset's eye size (2048 square by default; a Quest 3 at 1.0 is about 2064 x 2208).
- `--settings <cfg>`: a graphics profile exec'd before the map (copied into the game folder as
  `qvrbench_settings.cfg`), e.g. the author's own settings or a `vr_graphics_preset`. Without it the agent's baseline
  config is used. Every run's JSON records the settings that change the frame's cost (`profile::settingCvars`).
- Results: `kit/benchresults/<label>/` (`<agent>_<date>_<time>` by default): `manifest.txt` (tree, commit, exe and
  progs hashes, options), `<scenario>/r<k>.json` (one per run), `r<k>.log` (the run's bench lines and errors),
  `script.cfg` (exactly what ran), `summary.md` and `summary.csv` (medians over the repeats; `summary.md` ends with
  the window parts' worst frames and the map loads by phase: `qvrbench.py loads <dir>` prints those alone). `--validate` also writes a
  screenshot per scenario and `sheet.png`, a contact sheet of them all.
- Repeats alternate the order (1..n, then n..1) against thermal and driver drift.
- `compare` prints, for each scenario in both, the frame's median and 99th percentile, the CPU's work (mean, p99), the
  eyes' GPU time (mean, p99), the whole 3D refresh's GPU time and the heap events a frame, each window part's worst
  frame (`mark.<label>.max_ms`) and each map load's total, first frame and worst frame after (`load<k>.*`), with the change; a change of
  at least `--threshold` percent and 0.05 ms (0.5 for counts) is marked `**`.
- Time: a timing run is ~20-40 s (start-up, map load, set-up, 180 frames of warm-up, 900 measured at 90 Hz); `core` x 3
  is ~20 minutes, `all` x 3 about an hour and a half.

## What a run records

`vr_bench_begin <name> [frames | <seconds>s]` records every frame from the next one on, until `vr_bench_end`, the
frame count or the seconds; then writes `<gamedir>/profile/bench/<name>.json` and prints one `vr_bench:` line (and
one per map load in the window). `vr_bench_mark <label>` splits the window into parts (fixed storage, at most 128).
Debug > Profiling and Memory > **Benchmark Capture (10 s)** does the same in the headset (`manual.json`).

| field | what |
|---|---|
| `frame.frame_ms` | each frame's period (its start to the next's): n, avg, p50, p95, p99, max |
| `frame.host_ms` | `_Host_Frame`'s time (the rest is the frame cap's sleep) |
| `frame.cpu_busy_ms` | the CPU's work: the host frame less the runtime's waits (`xr wait`, acquire, release, submit) and the swap |
| `frame.gpu_eyes_ms` | both eyes' GPU time (timer queries every frame, read back a few frames later; the first 6 dropped) |
| `frame.gpu_3d_ms` | the whole 3D refresh's GPU time (`3D`: both eyes and the mirror in VR, the one view flat) |
| `frame.traces`, `draw_calls`, `alias_drawn` | a frame's collision traces, OpenGL draw calls and alias model instances |
| `frame.heap_allocs` | the main thread's heap events a frame (new, malloc, calloc, realloc); `heap_requested_kib` the total |
| `frame.main_mcycles` | the main thread's CPU cycles a frame, millions (`QueryThreadCycleTime`: a wait that spins counts, a sleep does not) |
| `frame.pose_to_submit_ms` | the latency's proxy: from the frame's pose (the runtime's frame begun, the tracking sampled) to its submit (`xrEndFrame` returned); frames that submitted |
| `hitches` | frames over 11.1 ms (a 90 Hz refresh), 13.9 (72 Hz), 33.3, 100, 250 ms, and over twice the median |
| `cpu_phase_ms` | each always-on phase's CPU time a frame (server, SV_Physics, client read, view entities, screen, eyes, sound, rigid bodies, shadow maps, world+brush, alias, particles, vr particles, decals, 3D) |
| `gpu_phase_ms` | each GPU phase a frame: the eyes, the runtime's calls, shadow maps, world+brush, alias, particles, vr particles, decals, 3D |
| `counts` | at the start, every 16th frame (avg, max) and the end: edicts, monsters alive, Box3D bodies / awake / contacts, Ironwail's and Quake VR's particles, decals, dynamic lights, shadowed lights |
| `marks` | the window's parts (`vr_bench_mark <label>`: from the next recorded frame to the next mark or the end): each one's frames, first, worst and mean frame, worst CPU work, frames over 33 ms |
| `loads` | each map load that ended (its first frame drawn) inside the window: what (`map e1m1: e1m1`, `changelevel e1m2: e1m2`, `restart e1m1: e1m1`), the map, `total_ms` from the command to the first frame drawn, `frames_to_signon`, its `first_frame` and the worst of the second `after` it (90 frames, as `marks`), its `stages` in order (`vr_startup_times`' marks: `[name, ms]`) and its `work` across them (`[name, ms, count]`: textures, image files, alias models, normal maps, external material maps, ragdoll rigs, hulls...) |
| `settings`, `eye_resolution`, `gl_renderer`, `map`, `date` | the context |

Percentiles are nearest-rank over all the window's frames (hitches included). The recorder stores a few numbers a
frame into vectors reserved at `vr_bench_begin` (no allocation while recording, so the heap counts are the game's).

**Per-scope detail**: the suite measures whole frames and the phases above. For a call tree of a scenario, run it with
`vr_profile 1` added (or the CSV capture, `vr_profile_csv 1`, a row a second per system): `qvrbench.py script <name>
--out x.cfg`, add the line before `vr_bench_begin`, and run it with `run.sh --exclusive`. The profiler's own cost then
shows in the numbers (its timer queries every `vr_profile_gpu` frames).

## Determinism

The game's clock is fixed (`vr_fixed_frames 1`, `vr_fixed_frames_rate` = `--hz`): every frame advances the same game
time, so a scenario's simulation is the same however fast the frames come (paced, unpaced, a slower machine).
`vr_bench_seed 7` restarts the C library's random numbers (QuakeC's `random()`) before the map and again before the
set-up; `vr_particle_seed 7` the particles'. The set-ups use the test commands' fixed positions (relative to the
player's spawn or a `setpos`), and the effects' random positions come from Python's `random.Random(7)`/`(11)`
when the script is written. `qvrbench.py validate` checks that every repeat started its window with the same edicts,
monsters alive and Box3D bodies, and decals within 5% (a fight's blood and bullet marks during the set-up land a few
more or fewer: `combined` 51/53, `tour_warden` 353-363, its monsters shooting each other on load). Things that still vary: the threads' timing (Box3D's step is deterministic; the
pool's work order is not, nor is anything that reads the wall clock), so combat scenarios drift apart over their
windows (PERFORMANCE_BENCHMARK_20261005.md: "run-to-run differences are reported rather than treated as
bit-identical combat replays"). Their means are the measure, with three repeats or more.

## The scenarios

`python Misc/quakevr/bench/qvrbench.py list` prints them with their groups; `script <name> --out x.cfg` the exact
commands. The firing range's (`vrfiringrange`) fixtures stand at its spawn (316 -556); `vr_physics_spawn` puts
monsters and torches ahead of the player.

| scenario | groups | map | purpose and set-up |
|---|---|---|---|
| `idle_e1m1` | idle/core | e1m1 | The floor every other scenario adds to: E1M1's start, nothing moving (notarget), both eyes drawn. |
| `idle_e1m1_flat` | idle/flat | e1m1, flat | The same view in flat mode (vr_enabled 0): what the desktop game costs, and the VR overhead by difference. |
| `idle_e1m1_qrp` | idle/textures | e1m1 (QRP) | idle_e1m1 on the QRP base: texture memory and sampling cost of the replacement pack. |
| `idle_range` | idle/core | vrfiringrange | The firing range at its spawn: Quake VR's content (props, text boards, decals) without action. |
| `idle_start_flat` | idle/flat | start, flat | The start map's hall in flat mode (its slipgates in view), for the flat build's own baseline. |
| `slipgate_start` | slipgates/core | start | Looking through the episode slipgate from its doorway: the destination scene, lights and entities per eye. |
| `slipgate_start_off` | slipgates/control | start | slipgate_start with vr_slipgates 0: the portal's whole cost by difference. |
| `slipgate_start_flat` | slipgates/flat | start, flat | slipgate_start in flat mode. |
| `slipgate_ai_24` | slipgates/combat | start | Monsters' one-hop portal perception and fire across the gate (the perf suite's portal_enemies). |
| `slipgate_loop_r0`..`r3` | slipgates/recursion (r2: core) | vrslipgates | Gates within gates: the loop's west gate 40 units out filling the view (it shows itself and T's north gate) at `vr_portals_recursion` 0-3: each level's views by difference. |
| `combat_48` | combat/core | vrfiringrange | Grunts, ogres, knights and scrags against a god-mode player: AI, traces, missiles, gore, decals, sounds. |
| `ai_crowd_64` | combat/server | vrfiringrange | The server's side alone: 64 awake monsters chasing and shooting (r_drawentities 0, looking up): AI, movetogoal, traces, QuakeC; the rendering left out. |
| `ai_crowd_64_quakeai` | combat/control | vrfiringrange | ai_crowd_64 with `vr_ai_enhanced 0` (Quake's AI: no stealth senses, noise, investigation): the enhanced AI's cost by difference. |
| `combat_48_spectator` | combat/features | vrfiringrange | combat_48 with vr_window_view 2 (the recording camera drawn every frame): the spectator view's cost. |
| `combat_48_bullettime` | combat/features | vrfiringrange | combat_48 with bullet time running all through: the time scale, the slowed sounds and the colour pass. |
| `melee_punch_8` | combat/melee | vrfiringrange | Scripted jabs (vr_mock_play, no recorded takes needed) into a ring of grunts: the melee and hit systems, knockdowns, wounds. |
| `props_500_active` | physics/core | vrfiringrange | vr_physics_bigpile mixed 500 with a blast every 90 frames: Box3D step, write-back, prop rendering. |
| `props_500_settled` | physics | vrfiringrange | The same pile left to settle (900 frames): what sleeping bodies still cost. |
| `ragdolls_32_active` | physics/core | vrfiringrange | 32 grunts killed by blasts (vr_ragdoll_max 32), then blasted every 90 frames: ragdoll joints, gore. |
| `ragdolls_32_settled` | physics | vrfiringrange | The same ragdolls left to settle. |
| `particles_dense` | vfx/core | vrfiringrange | 80 particles of smoke, blood or big smoke every 4 frames plus small blasts: particle fill rate, the retro particle path. |
| `explosions_3s` | vfx | vrfiringrange | vr_explosion_debris_test every 24 frames: explosion sprites, debris chunks and their lights. |
| `explosions_storm` | vfx/physics | vrfiringrange | vr_explosion_debris_test every 4 frames: the debris pool full, many short lights. |
| `decals_1024_stream` | vfx/core | vrfiringrange | 1400 pellets fill the 1024-mark pool, then a shot every 4 frames: decal insertion, the world decal grid. |
| `decals_blood_4096` | vfx | vrfiringrange | vr_decal_stress fills a 4096-mark pool with large blood marks, then adds one every 4 frames. |
| `torches_32` | vfx/lights | vrfiringrange | 32 more wall torches (35 emitters): fire particles, their lights. |
| `lights_32` | lights/core | vrfiringrange | vr_light_test x32 ahead: dynamic lights, their shadow maps (vr_shadow_dlights' slots), lit models. |
| `lights_32_noshadows` | lights/control | vrfiringrange | lights_32 with vr_shadow_dlights 0 and vr_shadow_maplights 0: the shadows' share. |
| `flashlight_e1m1` | lights/features | e1m1 | E1M1's start with the flashlight in the off hand, on and pointed ahead (its spot light's shadow tile: shadow_dlights 1). |
| `water_range_surface` | liquids | vrfiringrange | Standing at the firing range's pool, looking down at the water. |
| `water_range_under` | liquids | vrfiringrange | Under the firing range's pool (setpos 600 450 -150). |
| `slime_e1m1` | liquids | e1m1 | At round 21's slime test spot (setpos 200 2820 -60: the pool and two grunts in view; not under the surface). |
| `lava_e1m7` | liquids | e1m7 | Facing E1M7's lava (setpos -50 48 20). |
| `parallax_e1m1_qrp` | textures | e1m1 (QRP) | QRP base, parallax steps 32, looking along a wall: the parallax shader's worst case. |
| `parallax_e1m1_qrp_off` | textures/control | e1m1 (QRP) | parallax_e1m1_qrp with vr_parallax 0. |
| `menu_open_e1m1` | features | e1m1 | idle_e1m1 with the VR settings menu open: the 3D text and panels' cost. |
| `gore_slash_32` | gore/core | vrfiringrange | `vr_limb_test 4` on 32 grunts at once (`vr_gore_test_crowd`): 32 limbs cut, 32 ragdolls made in one frame; then the falling. Marks `before`, `blow` (6 frames), `after`. |
| `gore_dismember_16` | gore | vrfiringrange | 16 corpses (slashed in the set-up, settled) cut apart in one frame (`vr_limb_test 3`: whole limbs, then the head). |
| `gore_blast_crowd_32` | gore/physics | vrfiringrange | Four 120-damage blasts (`vr_physics_blast`) among 32 grunts, knights, enforcers and ogres: kills, limbs popped (`vr_limbs_blast 1`), gibs, ragdolls. |
| `gore_gib_limbs1_24` | gore | vrfiringrange | 24 grunts gibbed at once (`vr_limb_test 10`) with `vr_gib_limbs 1`: their limbs besides Quake's gibs. |
| `gore_gib_limbs2_24` | gore | vrfiringrange | The same with `vr_gib_limbs 2`: the limbs instead of the meat gibs. |
| `gore_gib_limbs0_24` | gore/control | vrfiringrange | The same with `vr_gib_limbs 0`: Quake's gibs only (the limbs' share by difference). |
| `gore_headpop_24` | gore | vrfiringrange | `vr_decap_test 14` on 24 grunts at once (a super shotgun headshot each, `vr_decap_pop_roll 0`: every head pops). |
| `gore_limbs_64` | gore/physics | vrfiringrange | `vr_limb_test 11` with `vr_limbs_max 64`: 128 limbs thrown, 64 kept, settled: what many limbs lying about cost. |
| `gore_limbs_cap` | gore/physics | vrfiringrange | gore_limbs_64, then 128 more thrown every 45 frames: the cap removing the oldest (churn). |
| `gore_first_cuts` | gore | vrfiringrange | Each of 14 rigged kinds spawned (`impulse 241`) and slashed at a limb, twice round: marks `spawn<r>_<kind>`, `cut<r>_<kind>`: the first cut's hitch against the second's. |
| `load_e1m1` | loading/core | (none) | `map e1m1` cold (nothing loaded before), again warm, then `restart`: each load by phase (see "Map loads"). |
| `load_e1m1_qrp` | loading/textures | (none, QRP) | load_e1m1's cold and warm loads on the QRP base: replacement textures, their material maps. |
| `load_e4m7` | loading | (none) | `map e4m7` (id's biggest BSP, 1.5 MB) cold and warm. |
| `load_hip1m1` | loading | (none) | `map hip1m1` from Quake's campaign (the campaign switch rebuilds the game folders), again, then `map e1m1` (the switch back). |
| `load_changelevel` | loading | e1m1 | `changelevel e1m2` from E1M1 (the in-game path), then `changelevel e1m1`. Replaces `mapload_e1m2`. |
| `load_reloads` | loading, maps | (none) | warden cold, `restart`, `map warden` again; ad_grendel cold, `restart`; e1m1 cold, `restart`, `changelevel e1m2` and back (one run: nine loads). The loads that get the last load's hulls back (`vr_hull_keep`) against the cold ones. |
| `load_warden` | loading/maps | (none) | `map warden` (15.4 MB BSP) cold and warm. |
| `load_ad_grendel` | loading/maps | (none) | `map ad_grendel` cold (its map package mounted) and warm. |
| `timedemo_demo1_flat` | loading/flat | start, flat | The classic `timedemo demo1` in flat mode inside the window: a replayed game's frames (its own fps line too). |
| `tour_warden` | maps | warden | warden: the spawn and five pickups' places, each looked at four ways: heavy geometry, many lights and entities. Monsters asleep (notarget). |
| `tour_apsp3` | maps | apsp3 | apsp3: the spawn and five pickups' places, each looked at four ways: heavy geometry, many lights and entities. Monsters asleep (notarget). |
| `tour_ad_grendel` | maps | ad_grendel | ad_grendel: the spawn and five pickups' places, each looked at four ways: heavy geometry, many lights and entities. Monsters asleep (notarget). |
| `tour_ad_soltower1e` | maps | ad_soltower1e | ad_soltower1e: the spawn and five pickups' places, each looked at four ways: heavy geometry, many lights and entities. Monsters asleep (notarget). |
| `tour_basetohell` | maps | basetohell | basetohell: the spawn and five pickups' places, each looked at four ways: heavy geometry, many lights and entities. Monsters asleep (notarget). |
| `tour_vanisch01` | maps | vanisch01 | vanisch01: the spawn and five pickups' places, each looked at four ways: heavy geometry, many lights and entities. Monsters asleep (notarget). |
| `tour_warden_flat` | maps/flat | warden, flat | tour_warden, flat. |
| `tour_mg3_<map>` | mg3/maps | map1, map2, secret2 (MG3) | Dawn of the Machine's map at skill 2: the spawn and five pickups' places, four ways each, monsters asleep. Needs the owned rerelease MG3 data (`vr_campaign_native mg3` in the script). |
| `mg3_<map>_awake` | mg3/combat | map1, map2, secret2 (MG3) | The map's whole count (`vr_test_monsters 4`: the deferred ones brought in) woken at once (`vr_test_monsters 2`), hunting the god-mode player at the spawn. |
| `mg3_<map>_kill`, `mg3_map2_kill_r32` | mg3/gore | map1, map2, secret2 (MG3) | As `_awake`, then every monster killed in one frame (`vr_test_monsters 3`), `vr_ragdoll_max` 8 (shipped) or 32: the deaths' frame (`blow`) and the bodies falling (`after`). |
| `combined` | combat/physics/vfx/lights/core | vrfiringrange | Everything at once (the perf suite's combined): the frame's worst realistic mix. |

### Tours of complex custom maps

`tour_<map>`: the spawn and five pickups spread over the map (`bsp_waypoints.py`: every n-th item or weapon in the
BSP's entity order, raised to a standing player's origin), each looked at in four directions for an equal share of the
window; monsters asleep (`notarget`). The waypoints are pasted into `qvrbench.py` (`TOURS`), so a tour never changes
unless the map does. The maps are only those already on this machine; `bench_maps.ps1` (run by `bench.sh`) makes them
playable in the agent's game folders with links, no copies:

| map | from | BSP | edicts at the start |
|---|---|---|---|
| `warden` | the author's checkout, `quakevr/maps` (not in git) | 15.4 MB | 706 (74 monsters) |
| `apsp3` | the author's checkout, `quakevr/maps` (not in git) | 6.8 MB | 818 (132 monsters) |
| `ad_grendel` | Map Library package (Steam Quake's `qvr_addons`) | 11.8 MB | 1164 (91 monsters) |
| `ad_soltower1e` | Map Library package | 2.9 MB | 576 (43 monsters) |
| `basetohell` | Map Library package | 1.4 MB | 306 (77 monsters) |
| `vanisch01` | Map Library package | 6.3 MB | 304 (42 monsters) |

`vrwip` (the author's work in progress) is linked too but has no tour (23 edicts). Candidates worth installing
through the Map Library for heavier geometry (not downloaded; sizes from the Quaddicted index): **Ter Shibboleth**
(Orl, 69 MB, id1 maps: famously dense brushwork), **Honey** (czg, 20 MB), **The Forgotten Sepulcher** (Giftmacher and
sock, 29 MB, needs Arcane Dimensions), **Arcane Dimensions 1.81** (306 MB, a mod: compatibility mode), **Alkaline 1.2**
(383 MB), **Coppertone Summer Jam 2** (195 MB). Once installed, `bsp_waypoints.py <bsp>` gives the tour's waypoints.

### Gore

`gore_*`: limb gore and head pops at a crowd's scale, on the firing range. `vr_gore_test_crowd <units>` (Debug > Gore
Tests > Crowd Gore Tests) makes `vr_limb_test` and `vr_decap_test` act on every monster that near you at once, in one
server frame, each as if it were the nearest (shots fired from just before it, not through the others), quietly: one
`goretest: crowd` line (cuts, pops, heads, limbs lying). The blow scenarios mark the window `before` (10 frames),
`blow` (the blow's frame and the 5 after) and `after` (the parts falling): compare `mark.blow.max_ms` (the blow's
hitch) and the `after` part's mean. `gore_first_cuts` spawns each rigged kind in turn (the firing range's dispenser
numbers, `vr_test_spawn`: grunt, ogre, zombie, shambler, scrag, knight, death knight, dog, enforcer, fiend, vore,
gremlin, scourge, mummy) and slashes it at a limb 25 frames later, twice round: `cut1_<kind>`'s worst frame is the
first cut of that kind (its limb models, its ragdoll's first use: whatever the build does then; limb models
precached at the map's start would move it into the load), `cut2_<kind>` the same cut warm, `spawn<r>_<kind>` the
spawn's own frames.

### Map loads

`load_*`: the window spans one or more loads (`loads_body`: a mark, the command, 150 frames), as long as its body
(`vr_bench_begin` without a count, `vr_bench_end` after it: a load's frames are not known in advance; the VR runtime's
loading frames count among them). Each load that ends inside the window is in the JSON's `loads`: the engine's load
timing (`vr_startup_times`, Debug > Profiling and Memory > Load Times) from the command (`map`, `changelevel`,
`restart`, `load`: `VR_TimeLoadCommand`) to the first frame drawn, stage by stage, and the kinds of work summed across
the stages. `qvrbench.py loads <dir>` (and `summary.md`) gathers the stages into phases (medians over the repeats):

| phase | stages |
|---|---|
| `command` | `map:` the map package's folders mounted, the campaign's game folders (a switch rebuilds them), the old server shut down; the rest to the spawn |
| `bsp` | `server: world model (BSP)`: the BSP read, its world textures loaded and uploaded (with their external maps) |
| `qc_spawn` | `server: entities spawned`: the QuakeC spawn functions, the models and sounds they precache (alias models loaded, their skins and normal maps) |
| `physics_init` | `server: 2 frames`: the first two server frames (Box3D's world, the ledges, the ragdoll rigs) |
| `server_other` | clear memory, progs, submodels, baseline and serverinfo, the VR after-load work |
| `models`, `sounds` | the client's precache (mostly already loaded by the server) |
| `lightmaps`, `renderer` | `R_NewMap`: lightmaps, brush model buffers, sky and fog |
| `vr_prepare` | `VR_NewMap`: the decal and particle atlases, detail textures, liquids, the view's models, the torch, casings, debris, muzzle flash, wall torches, the GL object count |
| `signon` | the load's frames before the signon, the rest of them |
| `first_frame` | the first frame drawn (its shaders' and textures' first use) |
| `work_*` | across the stages: textures processed and uploaded, image files decoded, alias models, normal maps made, external material maps, ragdoll rigs, hulls, Box3D's mesh |

Cold and warm: a scenario with no map before its window makes the process's first load of its map cold for the
engine (its caches empty: models, skins, normal maps; the shaders' first use); the OS's file cache is whatever earlier
runs left (a true cold disk is not reproducible here). The second load of the same map is warm: alias models and
their skins stay loaded between maps, the world's textures do not. `restart` reloads the server only (the client
reconnects). Loads run unpaced (`host_maxfps 0`) so the load's frames hold no frame cap's waits; the second after
each (`after`) is the worst frame of the first 90 frames drawn (first-use hitches). A saved game's load
(`load`) is timed by the engine too (the stage `load: the saved game's entities`) but has no scenario: it would write a
save into the kit's game folder.

### Validation (2026-10-06)

`bench.sh benchprep --validate --scenarios all` (fast, hidden, shared, 512-pixel eyes, 30 frames of warm-up, 120
measured, two runs each): all 44 scenarios load, set up the same counts in both runs and write their JSON; the contact
sheet (`sheet.png`) shows each one's view as intended (the combat views are tinted red: the god-mode player is being
hit). No numbers from it are benchmarks (other agents' games ran at the same time). A paced run
(`--scenarios idle_e1m1 --frames 90 --shared --settings <cfg>`) checked the real-time path: frames at 11.11 ms
(`host_maxfps 90`), the settings profile applied before the suite's own pins.

The gore and loading scenarios (`bench.sh benchadd --validate --scenarios loading,gore`, the same way): all 18 load,
set up the same counts, marks and loads in both runs (`validate`: 0 failing; `timedemo_demo1_flat` records its demo's
map load too). Not benchmarks either, but what stood out (a shared machine; medians of 2): `gore_gib_limbs2_24`'s blow
frame 144 ms against 52 (`vr_gib_limbs 1`) and 23 (Quake's gibs); `gore_first_cuts` one 1 s cut (`cut2_hknight`, one
run of two); `load_warden`'s loads 3.0-3.6 s, 1.5-2.9 s of it waiting for the hulls' build on the pool (`work`: "hull:
the map's brushes and compiled hulls"), the warm load no faster; `load_e1m1_qrp`'s warm load (2.4 s) about as slow as
the cold (2.7 s), its BSP stage slower (1.7 s against 0.9: the world's replacement textures and their material maps
decoded again); `load_hip1m1`'s campaign switch (`command` 343 ms) and Quake's alias models loaded again after it (QuakeC
spawn 1.0-1.6 s).

### Gaps

- **The headset's runtime.** The mock has no compositor: `xr wait` and `xr submit` are near zero, there is no
  reprojection, no transport (Virtual Desktop, Link). Real-headset pacing needs a run in the headset
  (Benchmark Capture in the Debug menu writes the same JSON there).
- **Sound** is off (`-nosound`, as every kit run): the spatial audio has its own benchmark (`vr_snd_bench`,
  TESTING.md "Profiling").
- **Recorded melee takes**: `melee_punch_8` uses a scripted jab (`vr_mock_play`), not the author's takes, which are not
  in `quakevr/motions/` now (eval.sh fails the same way). Replaying takes (`vr_motion_play`) inside a bench window is the
  natural next scenario once they are back.
- **GPU per pass** comes from the always-on phases (shadow maps, world+brush, alias, particles, vr particles, decals,
  the eyes, 3D). Finer GPU scopes (sky, water, translucent, bloom, postprocess, portal) exist only in `vr_profile`'s call
  tree, not in the JSON.

**Gameplay apart from loading** (2026-10-06, [PROFILING_2026-10.md](PROFILING_2026-10.md)): a gameplay scenario's
set-up ends with `vr_ao_finish` (waits for the models' occlusion bakes the load and the set-up's spawns started: 9 s
of 4 threads after the firing range loads), so no load work runs inside its window; the `load_*` scenarios keep their
loads (that is what they measure). For VTune, `vr_bench_profiler 1` resumes its collection for each window alone, `2`
for each map load alone (the command to the first frame drawn), with VTune started paused:
`Misc/quakevr/bench/qvrprof.sh` runs a scenario that way (or under Nsight Systems).
- **Flat mode** frames are paced by the window's swap (`swap` ~2.4 ms of the frame in fast mode): compare flat
  scenarios by `cpu_busy_ms` and `gpu_3d_ms`, not `frame_ms`.

### Profiling run (2026-10-08)

The overnight profiling run (agent `perf`): 24 scenarios (`core`, `slipgate_loop_r0..r3`, `ai_crowd_64` and its
Quake-AI control, three tours, `torches_32`, `explosions_storm`, `idle_e1m1_flat`), the author's settings of 2026-10-06
(`--settings his_cfg_20261006_1237.cfg`), paced at 90 Hz, exclusive, 900 frames. Before: `kit/benchresults/perf_base`
(3 repeats, medians); after: `perf_final` (one run each, the final build). The controlled measure of each fix is its
A/B (`perf_ab1_A` / `perf_ab1_B`: the two builds back to back, 3 repeats, 600 frames; in the commits). ms a frame;
"n/a": the GPU timers of a frame with gates within gates read 0 before the profiler's fix (commit "Profiler: the
always-on GPU phases read inside slipgate views").

| scenario | CPU avg before | after | CPU p99 before | after | GPU 3D before | after | frame p99 before | after |
|---|---|---|---|---|---|---|---|---|
| `combined` | 5.13 | 5.14 | 11.78 | 10.40 | 7.42 | 7.39 | 11.82 | 11.18 |
| `slipgate_loop_r2` | 3.46 | **2.04** | 4.25 | 2.68 | n/a | 3.74 | 11.15 | 11.20 |
| `slipgate_loop_r3` | 3.46 | **2.04** | 4.24 | 2.63 | n/a | 3.74 | 11.16 | 11.18 |
| `slipgate_loop_r1` | 2.89 | **1.77** | 3.63 | 2.26 | n/a | 3.29 | 11.15 | 11.17 |
| `ai_crowd_64` | 2.57 | 2.57 | 4.49 | 4.96 | 1.09 | 1.21 | 11.17 | 11.17 |
| `combat_48` | 2.52 | **2.02** | 4.66 | 3.53 | 3.54 | 3.40 | 11.17 | 11.16 |
| `load_e1m1` | 2.50 | 2.47 | 25.16 | 26.91 | 1.24 | 1.23 | 25.16 | 26.91 |
| `props_500_active` | 2.47 | 2.45 | 4.50 | 4.50 | 1.28 | 1.28 | 11.15 | 11.15 |
| `explosions_storm` | 2.05 | **1.70** | 2.44 | 2.11 | 2.12 | 2.12 | 11.17 | 11.20 |
| `gore_slash_32` | 2.01 | 2.03 | 4.75 | 4.90 | 2.49 | 2.46 | 11.15 | 11.15 |
| `tour_warden` | 1.90 | 1.91 | 3.48 | 3.42 | 1.71 | 1.71 | 11.17 | 11.20 |
| `ai_crowd_64_quakeai` | 1.76 | 1.79 | 4.57 | 3.49 | 0.92 | 0.96 | 11.16 | 11.19 |
| `slipgate_loop_r0` | 1.70 | **1.18** | 2.07 | 1.60 | 2.08 | 2.07 | 11.18 | 11.19 |
| `ragdolls_32_active` | 1.69 | 1.72 | 3.76 | 3.70 | 2.25 | 2.22 | 11.16 | 11.25 |
| `slipgate_start` | 1.68 | **1.18** | 2.06 | 1.56 | 2.31 | 2.32 | 11.18 | 11.24 |
| `tour_apsp3` | 1.62 | 1.66 | 2.40 | 2.46 | 1.10 | 1.10 | 11.19 | 11.18 |
| `tour_ad_grendel` | 1.26 | 1.25 | 1.99 | 2.01 | 1.24 | 1.24 | 11.20 | 11.22 |
| `particles_dense` | 1.15 | 1.18 | 1.62 | 1.61 | 5.72 | 5.71 | 11.19 | 11.19 |
| `lights_32` | 1.07 | 1.07 | 1.45 | 1.43 | 1.76 | 1.76 | 11.20 | 11.16 |
| `torches_32` | 0.98 | 1.01 | 1.34 | 1.50 | 1.29 | 1.29 | 11.16 | 11.18 |
| `decals_1024_stream` | 0.88 | 0.88 | 1.25 | 1.29 | 1.33 | 1.32 | 11.19 | 11.19 |
| `idle_range` | 0.83 | 0.83 | 1.18 | 1.17 | 0.97 | 0.97 | 11.16 | 11.17 |
| `idle_e1m1` | 0.66 | 0.64 | 1.12 | 1.00 | 1.22 | 1.22 | 11.24 | 11.20 |
| `idle_e1m1_flat` | 0.55 | 0.57 | 0.82 | 0.87 | 0.24 | 0.24 | 11.22 | 11.22 |

- **Every scenario holds 90 Hz** (frame p99 at the cap; the GPU's heaviest, `combined`, 7.4 ms of 11.1).
- **Fixed**: the slipgate view array's framebuffer validated for every view (`slipgate_*` CPU -30 to -41% in the
  A/B, any gate in view); `worldtrace::world`'s metadata lookup before its box test and the debris' doubled centre
  sweep (`explosions_storm` -19%, `combat_48` -16%, `ai_crowd_64` -15% in the A/B: one run of the crowd's fight
  varies more than that, see its row).
- **Heaviest CPU** (VTune, fast and window-only, `Misc/quakevr/bench/qvrprof.sh`): main thread mostly the GPU wait
  (`GL_AcquireFrameResources`, the GPU the limit when unpaced); then in `combined` the shadow maps' layered draw
  (0.6 ms), particle lighting (`lightParticles`, 0.4 ms), alias models, the props' touch links (SV_AreaEdicts 0.2 ms);
  in fights the debris' traces (before the fix), the decal grid's rebuild (`decals::buildWorld`, 0.2 ms), particles.
- **GPU** (Nsight Systems on `combined`, OpenGL trace): glDrawArrays (particles, full-screen passes) 62% of the busy
  time, the world's glMultiDrawElementsIndirect 34%.
- Trade-offs and leads: [PERF_DECISIONS.md](PERF_DECISIONS.md) (the recursion default among them).

#### Follow-up: the leads with no trade-off (2026-10-08, later)

The same set-up (his settings of 2026-10-06, paced 90 Hz, exclusive), 3 x 600 frames each, medians: before, the
profiling run's build (2102a6da, `kit/benchresults/perf2_final_A`); after, this follow-up's commits
(`perf2_final_B`; `combined`, `explosions_storm` and `particles_dense` from `perf2_ab_light_B5`, the last commit's
build). ms a frame.

| scenario | CPU avg before | after | CPU p50 before | after | CPU p99 before | after | vr particles before | after | view entities before | after |
|---|---|---|---|---|---|---|---|---|---|---|
| `combined` | 4.70 | **4.38** | 4.85 | **4.49** | 6.76 | 7.36 | 0.79 | **0.50** | 0.15 | 0.15 |
| `explosions_storm` | 1.66 | **1.51** | 1.71 | **1.56** | 2.14 | 2.11 | 0.32 | 0.33 | 0.46 | **0.33** |
| `combat_48` | 1.94 | 1.75 | 1.77 | 1.51 | 3.84 | 4.00 | 0.23 | 0.27 | 0.36 | **0.23** |
| `ai_crowd_64` | 2.14 | 1.92 | 1.98 | 2.07 | 4.52 | 4.19 | 0.37 | 0.38 | 0.47 | **0.32** |
| `particles_dense` | 1.12 | 1.13 | 1.16 | 1.15 | 1.60 | 1.61 | 0.29 | 0.30 | 0.12 | 0.12 |
| `decals_1024_stream` | 0.89 | 0.87 | 0.94 | 0.91 | 1.33 | 1.32 | 0.04 | 0.04 | 0.12 | 0.12 |
| `slipgate_loop_r2` | 2.03 | 2.02 | 2.05 | 2.06 | 2.60 | 2.55 | 0.08 | 0.08 | 0.12 | 0.12 |
| `idle_e1m1` | 0.66 | 0.64 | 0.70 | 0.68 | 1.18 | 1.15 | 0.01 | 0.01 | 0.12 | 0.12 |

- **Particle lighting** on the game's threads (the lightmap reads still in order on the main thread; the dynamic
  lights after them in a parallelFor when the last frame's work is large): `combined` vr particles -0.29 ms. Smaller
  loads stay in one pass (`particles_dense` +0.01 ms, within noise to slight).
- **Decal grid**: each mark keeps its buckets (only new marks hashed), the counts and fill in the grid's headers.
  `decals::buildWorld` alone (300 builds): 949 small marks 0.114 -> 0.067 ms, 1024 large 0.258 -> 0.140 ms a build;
  the grid word for word the same (`Misc/quakevr/decal_grid_test.py`: 766 exact comparisons; in game, 3090 builds
  compared). The bench's phases do not show it (it is in the eye's view set-up).
- **worldtrace::world** (debris, shells, ropes): the brush entities listed once a message instead of every entity
  looped for each line, and their submodel test by name: view entities -0.12 ms in `explosions_storm`, -0.13 to
  -0.15 in the fights (2.05 million lines traced both ways: the same).
- **Framebuffer binds**: 88 a frame in `combined`, 12 of them redundant (8: each shadowed light's atlas bind); 72
  redundant binds more a frame cost nothing measurable (shadow maps 0.900 -> 0.896 ms): no change.
- The fights (`combat_48`, `ai_crowd_64`) go differently run to run (3000-8700 particles): their CPU rows are not a
  measure; the phase rows are. `combined`'s p99 moves 1-4 ms between runs.

#### Second follow-up: shadow casters, touch links, boards, gate exits (2026-10-08, later)

The same set-up (his settings of 2026-10-06, paced 90 Hz, exclusive), 3 x 600 frames each, medians: before, b74f13a4
(`kit/benchresults/perf3_final_A`); after, this follow-up's commits (`perf3_final_B4`). ms a frame. Each commit's own
A/B is in its message (`perf3_cast_*`, `perf3_touch_*`, `perf3_text_*`, `perf3_exit_*`).

| scenario | CPU avg before | after | CPU p50 before | after | CPU p99 before | after | shadow maps before | after | SV_Physics before | after | screen before | after |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| `combined` | 4.28 | **3.98** | 4.41 | **4.18** | 7.09 | 5.65 | 0.877 | **0.689** | 1.598 | **1.550** | 2.52 | **2.16** |
| `slipgate_loop_r2` | 2.03 | **1.55** | 2.04 | **1.57** | 2.68 | 2.21 | 0.266 | 0.260 | 0.182 | 0.172 | 1.67 | **1.21** |
| `slipgate_loop_r0` | 1.19 | **0.93** | 1.21 | **0.95** | 1.79 | 1.42 | 0.147 | 0.141 | 0.181 | 0.171 | 0.85 | **0.59** |
| `slipgate_start` | 1.17 | 1.14 | 1.19 | 1.16 | 1.75 | 1.64 | 0.222 | 0.222 | 0.160 | 0.151 | 0.84 | 0.82 |
| `props_500_active` | 2.46 | **2.37** | 2.49 | 2.42 | 4.53 | 4.36 | 0.053 | 0.054 | 1.123 | **1.071** | 1.06 | 1.03 |
| `combat_48` | 1.47 | 1.50 | 1.42 | 1.44 | 2.86 | 2.78 | 0.094 | 0.091 | 0.363 | 0.352 | 0.94 | 0.97 |
| `idle_e1m1` | 0.65 | 0.64 | 0.69 | 0.67 | 1.18 | 1.20 | 0.038 | 0.034 | 0.173 | 0.172 | 0.34 | 0.32 |

- **Shadow casters** set up once a pass over the lights (r_alias.c's depth draws: lerp, matrices, zero blend kept by
  entity for the pass): `combined` 3228 -> 406 set-ups a pass, shadow maps 0.88 -> 0.69 ms;
  `vr_shadow_layered_check 20 cache`: both atlases 0 texels differ.
- **Touch links**: the area nodes' edicts also in arrays, walked by `SV_AreaEdictsUnordered` (VR_TouchLinks sorts
  them into edict order as before): SV_Physics -0.05 ms in `combined` and `props_500_active`; both walks compared on
  393,216 calls: the same edicts.
- **CRT world text boards** laid out only in the views whose frustum they reach (each eye and each gate view laid out
  every board of the map): vrslipgates' loop -0.26 ms (r0) and -0.33 ms (r2, in its own A/B); screenshots equal.
- **Gate exits** (`reverseSide`) kept with the gates when they are built: 23,000 made a frame in the loop at recursion
  2; alias -0.09 ms there; 20.9 million kept exits compared with ones made in full: equal.
- `combat_48`'s +0.03 ms is the fight's run-to-run spread (its phases: shadow maps and SV_Physics down).
- Left (PERF_DECISIONS.md, "Still open"): a compact copy of the touch walk's boxes (not exact: QC writes abs boxes
  without a relink), the static dynamic lights' world casters (`collectWorld`, ~0.12 ms in `combined`).

### Dawn of the Machine (MG3 M3-28, 2026-10-08)

MG3_PLAN.md M3-28: the three heaviest Dawn of the Machine maps (BSP 12.8, 14.8 and 35.1 MB) with their full monster
counts (skill 2, the deferred monsters brought in: map1 93 + 14 = 107, map2 120 + 98 = 218, secret2 102 + 64 = 166;
1070-1800 entities) and the ragdoll cap, group `mg3` (scenarios above; test aid `vr_test_monsters`, Debug > Tests >
Whole-Map Monsters). The author's settings of 2026-10-06, paced 90 Hz, mock eyes 2048, exclusive, 3 x 900 frames,
medians (`kit/benchresults/m328_base`, before this pass's fixes). ms a frame.

| scenario | frame p50 | frame p99 | frame max | CPU avg | CPU p99 | GPU 3D | edicts | awake bodies |
|---|---|---|---|---|---|---|---|---|
| `tour_mg3_map1` | 11.11 | 11.24 | 15.40 | 1.18 | 1.90 | 1.28 | 535 | |
| `tour_mg3_map2` | 11.11 | 11.25 | 11.76 | 1.58 | 2.42 | 1.15 | 1071 | 74 |
| `tour_mg3_secret2` | 11.11 | 11.21 | 16.63 | 2.08 | 3.01 | 1.59 | 1497 | 96 |
| `mg3_map1_awake` | 11.11 | 11.20 | 12.05 | 1.34 | 2.67 | 1.55 | 535 | |
| `mg3_map2_awake` | 11.11 | 11.22 | 11.77 | 1.99 | 3.97 | 1.16 | 1142 | 280 |
| `mg3_secret2_awake` | 11.11 | 11.27 | 64.16 | 4.14 | 9.80 | 1.36 | 1828 | 434 |
| `mg3_map1_kill` | 11.11 | 11.21 | 13.39 | 1.80 | 4.58 | 1.59 | | |
| `mg3_map2_kill` | 11.11 | 11.21 | 35.46 | 3.28 | 9.99 | 1.37 | | |
| `mg3_map2_kill_r32` | 11.11 | 11.50 | 33.23 | 3.52 | 10.65 | 1.21 | | |
| `mg3_secret2_kill` | 11.11 | 12.34 | 36.21 | 4.95 | 12.22 | 1.59 | | |

- **Every MG3 scenario holds 90 Hz at the median**; the worst steady load is secret2's whole count awake (CPU 4.1 ms
  of 11.1, p99 9.8: its 166 monsters, 430-540 awake Box3D bodies, many of them floating). One of its three runs had 4
  frames over 33 ms (the fight goes differently each run: QuakeC's `random()` shares the CRT's `rand()` with the
  client, so no two fights are the same, see `vr_bench_statehash`).
- **Kill-all frames** (`blow`): 31-36 ms for 160-218 deaths in one frame, about **0.15-0.2 ms a death** (its corpse or
  ragdoll, limbs and gibs, the gun dropped, the caps' walks of every entity); the frames after (`after`) 11-25 ms
  while the bodies fall. A real fight's worst (a rocket into a group: 5-10 deaths) is 1-2 ms.
- **Ragdoll cap**: 32 instead of the shipped 8 (`mg3_map2_kill_r32`): CPU +0.24 ms avg, the `after` part's worst 24.5
  against 14.3 ms. The shipped 8 stays (PERF_DECISIONS.md, 8).
- **Where the time goes** (VTune, secret2's fight, fast and window-only, `qvrprof.sh`): the server 2.5 ms a frame of
  3.7: Box3D's frame end 1.0 (the water test 0.26, the step 0.29, the props' relink 0.25), QuakeC 0.86 (builtins:
  `findflags` 0.19 ms: the stealth AI's look about for bodies and torches, two walks of every entity per monster; the
  force grab's `findportalcone` 0.14; `MG_WorldFrame`'s walk), the movers' `SV_PushMove` 0.21, monster movement 0.5.
  The kill frame (temporary builtin timers): `VR_Limb_MakeRoom` 2.7 ms, small gibs 2.1, `WeaponInst_Find` 1.4,
  `VR_EnemyWeapons_MakeRoom` 0.5.

Fixed (identical output; each commit's message has its check): the MG campaigns' frame ticks skip the unregistered
entities in the engine (`MG_WorldFrame`, findflags); a body's water tests walk the hull from its column's node (2.7
million columns compared with the old walk: equal); `SV_PushMove` tests an entity's box before its VR checks (26.8
million decisions compared: equal); the limb and dropped-gun caps count and find the oldest in one walk. The A/B
(`m328ab_A` / `m328ab_B`: the two builds' binaries alternated run by run, 3 x 600 frames each, medians; **taken while
other workers built and ran on the machine**, so the paired medians, not the absolute values, are the measure):

| scenario | CPU avg A | B | CPU p99 A | B | SV_Physics A | B | kill `after` max A | B |
|---|---|---|---|---|---|---|---|---|
| `mg3_secret2_awake` | 3.82 | **3.60** | 7.97 | 7.54 | 2.53 | **2.34** | | |
| `tour_mg3_secret2` | 1.90 | 1.86 | 3.28 | 2.89 | 0.95 | **0.87** | | |
| `mg3_map2_awake` | 1.84 | **1.70** | 3.42 | 3.22 | 0.97 | **0.89** | | |
| `mg3_map2_kill` | 3.35 | 3.21 | 9.47 | 9.59 | 1.98 | **1.85** | 15.8 | 11.5 |

Left (PERF_DECISIONS.md, "Dawn of the Machine"): the per-monster entity walks of the stealth AI and the enemy shove,
the force grab's search, the kill frame's spawns. Not covered: map loads (secret2's 35 MB BSP: `load_*` has no MG3
scenario), skills 0, 1 and 3, co-op.
