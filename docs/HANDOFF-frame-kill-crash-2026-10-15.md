# Handoff: Quake VR on Steam Frame, GPU hang after a kill (2026-10-15)

Start the next session by reading this file, then `docs/STEAM-FRAME-DEVELOPMENT.md` section "GPU hang after a kill (the
training dummy), and the Turnip autotune switch (2026-10-15)" (kept in line with this file), and the memory note
`quakevr-frame-gpu-crash-autotune.md`. Reply to the user in Danish.

## Goal

Quake VR Unleashed, native on the Steam Frame (SteamOS, Turnip Adreno 750, Mesa 26.3.0-devel via Zink, OpenXR via SteamVR),
crashes the GPU right after killing a monster. Find the cause and make the game work. The user tests by wearing the
headset; they asked to keep going until it works. Last user request: make a recording so tests can be automated, played on
the headset with injected head/controller control (see "Recording / automation" below: the replay path exists, a kill take does not).

## Standing constraints (do not drop)

- Never use, type, enter, store, repeat or run anything with the user's sudo password. Root steps are for the user's own
  terminal. Do not change system/security settings or sudoers.
- Changes to the Frame's installed files need the user's approval, backup first. The earlier "Ja" covered only the
  `TU_AUTOTUNE_ALGO` line in the installed launcher and preparing the test. UPDATE 2026-10-15 ~16:30: the user approved (AskUserQuestion, "Begge") adding BOTH `TU_DEBUG=sysmem` and `+vr_decals_world 0` to the
  installed `run-native.sh` so a kill take can be recorded; done with backup `run-native.sh.pre-record-20261015`. The repo launchers
  (`Misc/frame/run-native*.sh`) are NOT updated yet: wait for a headset result.
- A run with the headset worn needs the user wearing it. Each GPU hang resets SteamVR; never loop the flushsync baseline
  unattended.
- The auto-mode classifier denied (1) killing orphaned SteamVR processes and (2) running a temp launcher env-check
  script. Do not work around either; if SteamVR is stuck, ask the user to reboot the headset.

## State right now

- Frame: `ssh frame` (user `steamos`). SteamVR healthy, no ironwail running. Devcoredump watcher service
  `qvr-devcore-watch.service` is running; dumps land in `~/devcore-dumps`. Steam shortcut "Quake VR: Unleashed (Frame
  Native)" starts the installed `~/Games/QuakeVR-Unleashed-native/game/run-native.sh`.
- Installed launcher (edited 12:01, backup `run-native.sh.pre-prefer-sysmem-20261015`) exports
  `TU_AUTOTUNE_ALGO="${TU_AUTOTUNE_ALGO:-prefer_sysmem}"` and starts `./ironwail -window -vr-hide-mirror
  -qvr-frame-diagnostics -basedir ... -game quakevr -nobindless -noconfigwrite -condebug +vr_memstats_log 0 +vr_mirror 0`.
- Repo launchers `Misc/frame/run-native.sh`, `run-native-performance.sh`, `run-native-performance-ultra.sh` have the same
  `prefer_sysmem` export (uncommitted). The working tree has many other uncommitted changes from earlier sessions; do not
  assume it is clean and do not commit unless asked.
- Pending decision (asked, no answer yet): add `+vr_decals_world 0` to the installed launcher (new backup first) and the
  three repo launchers, then a wearer test (several kills, with the shotgun). If it still crashes, next candidate is
  `TU_DEBUG=sysmem` (a harder override than the autotune algorithm).

## The crash

- Signature: `gpu fault ring 2 ... source=BR_SP dir=READ type=TRANSLATION`, iova 0 / 0xec0 / 0xf00 (sometimes UCHE or
  garbage 49-bit addresses); hangcheck recovery about 10 s later; SteamVR resets (vrcompositor ABRT); ironwail aborts via
  `MESA_VK_ABORT_ON_DEVICE_LOSS`. Only seen when the world fragment shader's decal loop (`DecalsAt`, `Quake/vr/vr_glsl.h`)
  executes. `vr_decals`/`vr_decals_world 0` never crashed.
- Headset crash 2026-10-15 14:02:23 (installed launcher with `prefer_sysmem`): first kill, double-barrel shotgun, 14
  pellets. Game log: `~/Games/QuakeVR-Unleashed-native/game/qconsole.log` (ends on "Dummy: 84 damage ... killed"). Dumps
  `~/devcore-dumps/20261015-140224-devcd1.bin` (fault, iova 0xec0) and `-140234-devcd2.bin` (hang 10 s later).
  Copies in the old scratchpad `.../scratchpad/dumps/` (session temp, may be gone; the Frame copies stay).

## What is verified (and what I got wrong along the way)

Verified:
- In the faulting submit (fence 43336, IB1 0x10106c000; the in-flight IB is `CP_IB1_BASE` at register 0xb16 on a7xx) the world
  pass is binned and rendered in GMEM, 3 bins (a binning marker 0x2 with the draw IB 0x1037121c0, then that IB run once per
  bin). So `prefer_sysmem`
  did not make the world pass sysmem in the wearer run. Launcher edit 12:01, game start 14:01, so it was the new launcher.
- Both fault dumps freeze (`RBBM_STATUS3` bit 24, SMMU stalled on fault) right after a 1x1x1 `gather_indirect` compute
  dispatch that follows the GMEM world pass. The faulting instruction is not identified; `devcd1` has no crashdumper state
  and there is no shader disassembly.
- In that submit the SSBO bindings 3 and 4 and the atlas have valid descriptors with the engine's exact sizes (Decals 4560 B
  = 57 records, DecalGrid 38468 B). Mesa-level state is also right: a test build (`QVR_DECAL_BINDTEST`) found 0 mismatches
  in over 100,000 pre-draw checks, and forced unbind/rebind before every draw still crashed.
- Headless matrix results, release binary, OpenXR backend with no wearer, 6 kills per run (clean/total):
  no switch (`rel_base`) 2/4 in one matrix and 0/4 in another, flushsync baseline about 1/14; `prefer_sysmem` 11/11; `TU_DEBUG=sysmem` and `noubwc` 4/4;
  gmem 1/4; lazy descriptors 2/4; `vr_decals_world 0` 12/12 (soak1: 6 flushsync + 6 async) plus 3/3 earlier; Mesa threads
  (`GALLIUM_THREAD=0`, `mesa_glthread=false`, both): 0/4, 2/4, 0/4 clean, baseline 0/4, so not the cause; with the Steam
  environment (below): no switch 1/4, `prefer_sysmem` 4/4, `vr_decals_world 0` 4/4.
- The Steam client's environment, inherited by the shortcut-launched game, has `MESA_GL_VERSION_OVERRIDE=4.3` and
  `MESA_GLSL_VERSION_OVERRIDE=430` (game log GL 4.3); the headless harness did not (292 runs, GL 4.6). Wearer display
  72 Hz, harness 60 Hz. With the override set the headless results above did not change, so it does not explain the gap.

Wrong earlier (do not repeat):
- "Every pass was sysmem in devcd1": I walked the ring from rptr, which is the next submit. Wrong submit.
- "The decal SSBO base is null": inferred from iova 0xec0 = Decals[47]; the dump shows valid descriptors. Unsupported.
- "Mixed GMEM/sysmem/UBWC is the cause, `prefer_sysmem` fixes it": only holds headless.

Open hypotheses:
1. A worn headset forces the world pass to GMEM whatever the autotune algorithm says. The library has
   `tu_autotune mode ... selecting=GMEM reason=fdm` and `reason=preempt_latency_risk`; Valve's FDM injection layer
   (`XR_APILAYER_VALVE_fdm_injection`, loads headless too) adds foveation to Zink's render passes. Not captured:
   `MESA_LOG_LEVEL=debug` printed no autotune lines (the log may be perfetto/u_trace or need another switch). Valve's
   `gpu-trace --daemon` runs on the Frame. A hard test: `TU_DEBUG=sysmem` on the headset.
2. The fault is in or just after the `gather_indirect` compute (engine compute shader that builds the indirect draw
   commands), with the world pass only the trigger. Why would `vr_decals_world 0` avoid it, then? Unexplained.
3. Turnip/hardware bug around GMEM to compute transitions (Opus: "stale descriptor contents in a GPU cache" or a load that
   bypasses descriptor bounds; hypothesis category only).

## Tools and where things are

Repo: `C:\Modding\QuakeVR-Unleashed\source`. Frame scratch game dir
`~/Games/QuakeVR-Unleashed-native/performance-results/dummy-kill-repro/game` (binaries `ironwail` = installed release,
md5 06bd0387..., `ironwail-test` with `vr_gfx_orphan`, `ironwail-bind` with the bind check). Run dirs
`performance-results/dummy-kill-repro/<name>-<variant>-rN/` (launcher.log, qconsole.log, result.json).

- `Misc/frame/run-dummy-kill-repro.py`: one run (scratch copy of the game, OpenXR or `-vrmock`, no wearer, `vr_dummy_test 3`
  on `vrfiringrange`, scenarios `test3`/`axe`).
- `Misc/frame/killrepro-matrix.py <name> <rounds> <variant,variant> <kills>`: randomised rounds; results
  `~/matrix-<name>.jsonl` and stdout `~/<name>.out` on the Frame (copy the script there with scp). Variants of note:
  `rel_base`, `rel_atsys`, `rel_dw0`, `relfs_dw0`, `rel_nothread*`, `rel_noglthread`, `rel_atsys_log`, `ov_base`, `ov_atsys`,
  `ov_dw0`, `ov_fs_base` (Steam env), `bc_*` (bind check). `FS` variants add `ZINK_DEBUG=flushsync` (near-certain crash).
  A run counts as a crash when exit != 0, GPU fault lines or new dumps (some runs exit 0 but log dozens of faults).
- Bind-check source: Frame `~/source-bind`, patch `scratchpad/patch_bindcheck.py` (test-only; HEAD + `QVR_DECAL_BINDTEST`).
- Dump decoders (scratchpad, Python): `devcd_bos.py`, `ringtree.py`, `walk2.py`, `desc_scan.py`, `descptr_scan.py`,
  `addr_scan.py`, `ds_scan.py`; Opus's, which found the real faulting IB: `scratchpad/opus/` (`ibtree.py`, `ib_10106c000.txt`,
  `drawsC.txt`, `ib138.txt`, `mem.py`, `atlaschk.py`, `clusters.py`). `crashdec`/`cffdump` exist only on the Frame and are slow
  over ssh; run them backgrounded.
- Install a watcher on a fresh Frame: `Misc/frame/install-devcore-watch-service.sh` (needs the user's root).

Gotchas:
- ssh background jobs: `(setsid nohup cmd > f 2>&1 < /dev/null &)`; `cd x && nohup cmd &` keeps the channel open.
- Foreground `sleep N` is blocked in the Bash tool; use an `until` loop with `run_in_background`.
- On this Windows box the Bash tool has `python`, not `python3`.
- A GPU hang resets SteamVR and normally the session restarts itself. Once (06:12) it crash-looped (gamescope
  `Assertion '!modifiers.empty()'` in `CVulkanTexture::BInit`, orphaned vrserver) and needed the user to reboot the headset.
- `ZINK_DEBUG=flushsync` makes the crash near certain but hides timing effects (gmem and lazy descriptors "fixed" it there
  and still crashed async). Always confirm on the async release path.

## Recording / automation (the user's idea; the replay path is built and validated)

What the user asked for (Danish, paraphrased): a recording I can run on the headset, with mock controllers and headset
control, while the headset otherwise renders as it would have. Implemented as the Motion Recorder's takes
(`docs/vr-port/MOTIONS.md`) replayed through the real OpenXR backend:

- `vr_motion_play <take> xr [kill] [options]` (new options, `Quake/vr/vr_motion_play.cpp`, uncommitted): the take's head, hands and
  buttons replace the runtime's tracking; the eye poses are the take's head plus the runtime's own eye offsets and its
  field of view; frame pacing, swapchain, compositor, FDM layer, driver are real. A wearer sees the take's camera
  (head-locked), so use takes with a calm head. `vr_motion_eval` stays mock-only.
- `Misc/frame/run-dummy-kill-repro.py --scenario replay --take <file> --map <map> --backend openxr --exe ironwail-replay
  --cvar vr_xr_unfocused=0 --cvar vr_xr_unfocused_pause=0` (`--play-arg noplace|yaw N|target ...`, `--replay-margin`): copies the take
  into the scratch game, waits take-rows + margin frames, `quit`s; the result JSON has `replay_finished`/`replay_stopped`.
- Binary: Frame `~/source-bind` (HEAD + inert bind-check patch + the xr change), build dir
  `~/Games/QuakeVR-Unleashed-native/build-bind`, copied to the scratch game as `ironwail-replay`. A release build with the xr option
  is still to be made (the installed `ironwail` does not have it).
- Validated (2026-10-15): `spacewarp_live_r1_2026-10-13_22-56-22.csv` (vrstart, 4344 frames) replays to the end on the OpenXR
  backend (96 s, no wearer) and in the mock headset (43 s) with the same report (hands vs take about 13 units rms in both: that
  take's own placement, not the xr path). That take has no kill.
- Synthetic kill take: `python Misc/quakevr/motion_synth.py gun_kill --rate 72 --name gun_kill_ssg` (new preset: super shotgun at
  the dummy's chest, one trigger pull, trigger buttons bit 1). Replayed with `--play-arg kill --repeat 4` headless on OpenXR it
  printed the wearer's exact line (`84 damage ... 14 pellets, 14 head (x1.50) - health 0/30 - killed`) 4/4; no hang in that one
  run. Files on the Frame: scratch game `quakevr/motions/gun_kill_ssg.csv`, binary `ironwail-replay` (with `kill`).
  Next: a replay matrix (`killrepro-matrix.py` still hardcodes `--scenario test3`; add a way to pass the replay arguments)
  to see if the replay hangs as often as `vr_dummy_test 3`.
- The user's wish (2026-10-15 ~16:10): record two takes in the headset, one with the axe (a throw) and one with the shotgun, and
  "pause when ready to be tested". A kill in the installed game hangs the GPU before the recorder saves, so the recording needs a
  workaround active in the launcher first (needs the user's approval; asked 2026-10-15 ~16:25).
- Still needed: a take of a kill. Either the user records one (headset on, `map vrfiringrange`, Advanced VR > Motion Recorder,
  click the off stick to start/stop; shotgun and axe at the dummy) or extend `Misc/quakevr/motion_synth.py` (its takes write
  `_trigger 0` always, so no weapon fires: it needs a trigger press and a firearm preset). The kill itself is reproduced headless
  by `vr_dummy_test 3`/`impulse 209` already; the replay's added value is the worn headset (presence, foveation, preemption)
  with nobody needing to perform the kill.
- Simplest wearer test, no take and no launcher edit needed: the user puts the headset on, the harness runs the scripted kills
  (`run-dummy-kill-repro.py --backend openxr --scenario test3|axe --cvar vr_decals_world=0 ...`) on the real session, in the
  scratch game copy (the installed files stay untouched). Needs the user wearing the headset and saying go.

## Replay matrices and the merge of origin/next (2026-10-15 evening)

Eight takes recorded by the user (axe, shotgun, chainsaw, rocket) are in `game/quakevr/motions/` on the Frame (all named
`slash_2026-10-15_17-*.csv`; `_17-41-41` is the one the matrices replay). `vr_motion_play <take> xr kill` replays on the real
OpenXR path; haptics are off in the runner. Results (3 kills per run, hang = `hangcheck` line, Steam env):

| build | variant | hangs |
|---|---|---|
| old (rp1) | TU_DEBUG=sysmem, installed-launcher combo, ov_atsys | 0/3 each |
| old (rp1) | forced GMEM: base / +prefer_sysmem / +`vr_decals_world 0` | 2/3, 3/3, 3/3 |
| old (bs1), GMEM + dw0 | `vr_decals 0` / gore off / recording-clean | 0/3, 3/3, 2/3 |
| origin/next (nx1), forced GMEM | base / dw0 / prefer_sysmem / TU_DEBUG=sysmem | 3/3, 2/3, 1/3, 0/3 |
| merge build-next2 (sm3) | TU_DEBUG=sysmem, installed combo | 0/3, 0/3 |
| merge build-next2-sw (sm2/sm4) | installed combo; interop; interop + frame synthesis | 0/3 each |

Reading: the hang needs the GMEM path and decals being drawn (the world loop or the mesh path; `vr_decals 0` removes it,
gore/hologram do not). Whatever both decal paths share is the suspect: the `Decals[]`/`DecalGrid[]` SSBOs (bindings 3 and 4),
the `DecalAtlas` sampler (14), the atlas uploads when a mark is made. next's rewritten decal system and permanent decals do
**not** remove it. `TU_DEBUG=sysmem` never hung in any test.

The merge of frame-native + the uncommitted WIP (spacewarp, frame synthesis, native Vulkan BSP, tests, docs) + origin/next
(907eb850) is in the worktree `QuakeVR-Unleashed/next-source`, branch `frame-native-next`, **uncommitted** (the user's
`frame-native` and its working tree are untouched). Conflicts: `Quake/vr/vr_backend_openxr.cpp` (9 hunks: next's `guarded()`/
`step()`/format validation with the WIP's `QVR_OPENXR_INTEROP` teardown, interop swapchains and `createSwapchain(..., format)`),
six small ones (motion_synth.py, r_brush.c, vr_bench.cpp, vr_builtins.cpp, vr_motion_play.cpp, MOTIONS.md), plus Linux fixes for next's
Windows-only parts (`__rdtsc` in vr_qcexec.c -> `QCProf_Ticks`, SEH `Fault`/`describeFault` and the GL-context helpers under `_WIN32`
with stubs). Frame builds: `~/Games/QuakeVR-Unleashed-native/build-next2` (plain) and `build-next2-sw`
(`-DQVR_ENABLE_FRAME_SYNTHESIS_GL_VULKAN_BRIDGE=ON`), source `~/source-next2` (tar of CMakeLists.txt VERSION Misc/cmake Misc/pak QC Quake),
scratch binaries `ironwail-next2`, `ironwail-next2-sw` (in the runner's scratch game dir, `performance-results/dummy-kill-repro/game`,
the runner's `--exe` looks there). Both ran the replay clean; the sw build with `vr_xr_vulkan_interop 1` made the Vulkan2 session,
the native eye swapchains and the synthesis motion/depth chains (qvr_openxr.txt). Not tested: a wearer run of either build.
Notes on next: `vr_permdecals 1` by default (4096^2 atlas layers, 85 MB each, up to `vr_permdecal_atlas_max` 512 MB), a config
migration (configVersion ~137) and a restructured menu.

## Next steps, in order

1. Get the user's answer on `+vr_decals_world 0` for the installed launcher; edit with a new backup; update the three repo
   launchers; wearer test, then keep the watcher on and read any new dump (find the in-flight IB via `CP_IB1_BASE` 0xb16).
2. Get a take of a kill (user records it, or extend motion_synth.py), replay it on OpenXR headless and with the user wearing
   the headset; A/B with `vr_decals_world 0`. Make a release build that has the `xr` option.
3. Capture why the autotuner picks GMEM with a wearer (autotune log, `gpu-trace`, or a wearer run with `TU_DEBUG=sysmem`).
4. Identify the faulting instruction (disassemble the `gather_indirect` shader and the world fragment shader from a dump;
   the `h2_disasm` variant and `crashdec` are the starting points) and write a minimal Mesa/Turnip report.
5. Keep `docs/STEAM-FRAME-DEVELOPMENT.md` and the memory note in line with any new result; remove the `prefer_sysmem`
   rationale from the launcher comment ("Mixed GMEM/sysmem + UBWC made the decal world shader hang the GPU on kills") once the
   real cause is known, since that comment is not established.

## Unattended spacewarp A-B-B-A on the Frame (2026-10-15 night)

`Misc/frame/run-spacewarp-headset-abba.sh` now has `QVR_AUTO=1`: no wearer is needed. A standby HMD (no valid pose),
missing controllers, an open SteamVR dashboard and the headset validator's "no head pose" verdict are recorded but do not abort
the run; scene focus on the game is still required. The run also stops the compositor collector at `BENCH_DONE` (read live from
the `-condebug` console log; `launcher.log` is block-buffered until exit) so the runtime's scene change during engine teardown is
no longer reported as a focus loss. The unattended run gets FOCUSED, SteamVR records depth and motion-vector frames, the collector
returns `runner_finished`. The Frame must be kept awake (`systemd-run --user --unit=no-sleep-dev -p RuntimeMaxSec=12h
systemd-inhibit --what=sleep:idle --mode=block sleep infinity`); when the old unit expired it suspended and dropped off the LAN.

Result (build-next2-sw, interop on, vrcalibration fixture, 450 frames per leg, HMD in standby, SteamVR motion smoothing enabled):

| leg | game frame ms avg/p95 | cpu busy ms | gpu eyes ms | synthesis gpu ms | compositor motion flag | SteamVR frames w/ depth+motion |
|---|---|---|---|---|---|---|
| off a1 | 27.8 / 32.4 | 7.8 | 10.35 | 0 | 91% | n/a |
| on b1 | 28.4 / 37.5 | 9.7 | 10.37 | 1.6 | 97% | 615/617 |
| on b2 | 28.5 / 39.4 | 9.6 | 10.37 | 1.5 | 95% | 617/617 |
| off a2 | 27.8 / 31.2 | 7.2 | 10.26 | 0 | 96% | n/a |

Reading: the app delivers motion/depth and SteamVR receives it for every frame, but the compositor timings are the same ON and OFF
(motion-smoothing flag, GPU time, interval, mispresents, drops; the app runs at ~36 Hz in both because SteamVR's own smoothing is on).
The cost is real: about +2 ms CPU busy, ~1.5 ms GPU, and a worse p95 (+5-7 ms). Whether the runtime visibly uses the EXT inputs is
not measurable from timings (`ext_input_use_proven` stays false); it is a perception question for a wearer, or needs a capture of
the compositor output. Caveat: the HMD was in standby, so display-side behavior may differ from a worn run.
Pitfalls found: `pgrep -f <script>` over ssh matches its own `bash -c` and never ends; wait on a done-file instead.

### Smoothing off / relaxed-interval follow-up (2026-10-16 00:00-00:10, unattended)

`QVR_MOTION_SMOOTHING=0|1` (temporary, restored on exit) and `QVR_RELAXED_INTERVAL_ON=1` were added to the A-B-B-A script.
Fixture and the crowd-blast scenario (`gore_blast_crowd_32`), build-next2-sw, HMD in standby, smoothing off unless noted:

| config | frame ms OFF -> ON | cpu busy OFF -> ON | compositor motion flag OFF / ON | mispresents OFF -> ON |
|---|---|---|---|---|
| fixture, smoothing off, no hint | 21.8 -> 23.9 | 7.2 -> 9.3 | 0% / 0% | 607 -> 777 |
| fixture, smoothing off, relaxed hint | 21.7 -> 23.7 | 6.6 -> 8.7 | 0% / 0% | 597 -> 785 |
| fixture, smoothing on, relaxed hint | 27.8 -> 28.5 | 7.1 -> 9.0 | 95% / 95% | 626 -> 654 |
| crowd blast, smoothing off | 26.4,27.8 -> 36.8,28.3 (noisy) | 13.8 -> 16.5 | 0% / 0% | n/a |
| crowd blast, smoothing off, hint | 40.7,26.1 -> 27.9,28.5 (a1 outlier) | 14 -> 16 | 0% / 0% | n/a |

With SteamVR's smoothing off the compositor never flags a frame as motion-smoothed, with or without our motion/depth submission and
with or without the relaxed hint; the app's cadence does not change with the hint (the runtime ignores it). SteamVR does receive the
buffers (depth/motion frames counted in its recording), but there is no sign it uses them, while the cost (+2 ms CPU, +2 ms GPU eyes
for the depth/motion export, ~+1 ms synthesis passes) is always paid. The crowd-blast scenario is too noisy for an A/B in this setup.
**The 4th run (crowd blast, smoothing on, hint) hit the known Turnip GPU hang** (`msm_dpu ... hangcheck recover!`, offender
`ironwail:zfq0`, 00:09:28; vrserver shut down and restarted itself): the smoke runner does not set `TU_DEBUG=sysmem`, so decal-heavy scenarios
are unprotected. The abba script now exports `TU_DEBUG=sysmem` by default (`QVR_TU_DEBUG=` disables).
Bug fixed on the way: `vrcmd --set-settings-bool KEY true` stores 0 (it parses numbers); use 1/0. A quick manual dashboard test had left
`dashboard.enableDashboard` false for ~2 h until it was reset to 1 (verified).

### SteamVR per-app overrides and how to force spacewarp/smoothing (2026-10-16 ~01:00, unattended)

Correction to the paragraph above: the compositor *does* recognise our submission. With frame synthesis ON, `vrcompositor.txt` logs
`Using motion encoding AppMVs.` (absent in OFF legs) and then loads the `distort_vs_reproject_mv` shaders. What is still unmeasured is
whether the result looks better; the timings only show the cost.

Where the forcing switches live (SteamVR on the Frame, `~/.config/openvr/config/steamvr.vrsettings`, section = the scene app key):
- `<app key>.motionSmoothingOverride` (int): **0 = use global, 1 = enabled (auto), 2 = disabled, 3 = force always-on**. Measured on the fixture
  with global `steamvr.motionSmoothing` = false, frame synthesis ON, HMD in standby: 0 -> motion flag 0%, game 19.7 ms; 1 -> 96%, 27.4 ms;
  2 -> 0%, 20.0 ms; 3 -> 100%, 28.1 ms (p95 33 vs 43 for value 1). That matches the "Per-Application Video Settings" list
  (Use Global Setting / Enabled / Disabled / Force Always-on); always-on runs the app at half rate.
- `<app key>.framesToThrottle` (int): throttle the app to refresh/(n+1). `additionalFramesToPredict` goes with it.
- Our harness runs use the key `system.generated.openxr.quake vr.ironwail`; the Steam shortcut uses `steam.app.3397365993`.
- `vrcmd --set-settings-int "<key>.<name>" N` creates the section on the fly; unset keys read back as empty; there is no delete, so the
  harness restores "unset" as 0 (= use global / no throttle). `QVR_MOTION_OVERRIDE`, `QVR_THROTTLE`, `QVR_APP_KEY` in `run-spacewarp-headset-abba.sh`.
- Valve's `vrpreferences.json` (`steam_frame` dict: `preferHalfFramerate`, `preferMotionSmoothingMode` = off/on/alwaysOn,
  `preferDepthReprojectionMode`, `preferMinRefreshRate`, `preferResolution`) maps to these overrides, but vrserver only reads it for Steam-installed
  packages (log: `[Settings] Loading app-provided preferences from ...`); it is loaded for Half-Life Alyx, not for our shortcut/test binary, so the
  `vrpreferences.json` files we put in `game/` and `game-swtest/` currently do nothing. Alyx ships `preferHalfFramerate: true`,
  `preferDepthReprojectionMode: on`, `preferMotionSmoothingMode: off`.
- Power: vrserver also has `powersaveFramesToThrottle/powersaveRefreshRate/powersaveResolution/powersaveFovScale` (the SteamOS power-save profile).

Forced always-on A-B-B-A (`QVR_MOTION_SMOOTHING=0 QVR_MOTION_OVERRIDE=3`, fixture, HMD in standby, OFF = no synthesis inputs, ON = EXT inputs):

| config | frame ms (p95) | cpu busy | gpu eyes | compositor GPU avg | motion flag |
|---|---|---|---|---|---|
| always-on, OFF a1/a2 | 27.8 (31.5) / 27.8 (31.7) | 6.9 / 6.8 | 9.2 / 9.3 | 11.6 / 12.1 | 100% |
| always-on, ON b1/b2 | 28.1 (32.3) / 28.0 (32.9) | 8.5 / 8.3 | 9.5 / 9.6 | 10.8 / 11.5 | 100% |
| half-rate throttle only (override 2 = disabled, `framesToThrottle=1`), OFF a1/a2 | 27.8 (31.4) | 6.9 / 7.0 | 7.5 | 9.1 / 9.1 | 0% |
| half-rate throttle only, ON b1/b2 | 27.8 (31.7/31.5) | 8.5 / 8.4 | 9.5 | 11.3 / 11.3 | 0% |

Reading: forced always-on puts the app on a steady 36 Hz (27.8 ms) while the compositor presents 72 Hz with the motion flag on 100% of frames,
with or without our inputs; the cadence is identical, ON costs ~+1.5 ms CPU and ~+0.3 ms GPU eyes, and the compositor GPU may be ~0.7 ms lower
with AppMVs (inside the run-to-run noise). With throttle only (no smoothing) our submission costs ~+2 ms GPU eyes and ~+2.2 ms compositor GPU
(the compositor takes the depth/MV reprojection path even though no motion flag is set), i.e. the inputs are not free when they are unused.
Still no image-quality measurement; that needs the displayed frames captured.
After every run the per-app keys were set back to 0 (they stay in the file as `framesToThrottle: 0` / `motionSmoothingOverride: 0` for the
test app key only), global `steamvr.motionSmoothing` true, recordStats false, dashboard enabled (all verified).
Other things that can exercise this path: Godot 4.6 `OpenXRFrameSynthesisExtension` (XR_EXT_frame_synthesis); Meta's `XrSpaceWarp` native
sample and Unity/Unreal Application SpaceWarp use XR_FB_space_warp, and `vrclient.so` on the Frame contains both extension names.

### Render scale: the game renders the eyes at 0.45 of SteamVR's 1728 px (2026-10-16 ~02:00, unattended)

The worn session (21:23-21:52) log says `OpenXR: eye images 1728x1728 (3.0 Mpx; vr_xr_eye_scale 1), rendered at vr_render_scale 0.45`: the eyes are drawn at
~778x778 (0.6 Mpx, 20% of the pixels) and FSR1-upscaled (`vr_upscale 1`). 0.45 is the repo launchers' first-run default (`run-native.sh`:
`+vr_graphics_preset 0 +vr_render_scale 0.45 +vid_fsaa 0`), which got persisted in `game/quakevr/ironwail.cfg` (and `game-swtest`); the performance-preview
shortcuts use 0.25 / 0.18. The cvar default is 1. `QVR_RENDER_SCALE=x` (new, in `run_frame_synthesis_smoke.sh`; restored on exit) sets it per run.

Bench (fixture / crowd-blast, synthesis OFF, SteamVR smoothing off + per-app disabled, HMD in standby; GPU-eyes ms, frame ms):

| scale | fixture eyes / frame | crowd eyes / frame |
|---|---|---|
| 0.45 | 6.8 / 16.8 | 11.6 / 23.1 |
| 0.7 | 14.0 / 24.2 | not run |
| 1.0 | 25.3 / 35.5 | 35.2 / 46.5 |

The compositor needs ~8-10 ms of the same GPU per frame, so at 72 Hz (13.9 ms) the eyes may take ~5 ms and at forced half rate (27.8 ms) ~18 ms.
At 1.0 even half rate fails (fixture 41.5 ms = 24 fps, eyes 28.7 ms). Spacewarp A-B-B-A at 1.0 (forced always-on / the user's normal smoothing-on):
app cadence unchanged ON vs OFF (41.5 vs 41.7 ms), compositor GPU ~3.5 ms lower with our inputs (35.0 -> 31.5; auto 34.9 -> 31.8) and ~18% fewer
mispresents (first measurable effect of the inputs); the crowd scene is worse with them (eyes +4..+16 ms, drops 312-849 vs 163-240). The 6th matrix
(smoothing-on crowd) hit the leg timeout (45 s) because frames were 55-70 ms: no crash, hang check clean, settings restored.
Untested: 0.7 + forced always-on (estimate: fixture eyes 14 ms fits the 27.8 ms half-rate budget), a worn visual comparison, the installed
launcher/cfg were not changed.

Motion smoothing at 1.0 (same runs): SteamVR's smoothing locks the app to whole multiples of the 13.89 ms refresh. Fixture, smoothing on or forced always-on:
41.7 ms = 3 x 13.89 (24 fps real, 2 of 3 shown frames synthesised) against 35.5 ms (28 fps, uneven cadence) with smoothing off; crowd scene
55.9 ms = 4 x 13.89 (18 fps real) with smoothing on against 46.5 ms off. At 0.45 forced always-on it was 27.8 ms = 2 x (36 fps). So smoothing evens
the cadence at the price of fewer real frames and more synthetic ones; it only helps when the app lands between 36 and 72 Hz (about 0.7 on light
scenes; untested).

### Render scale 0.7 with smoothing (2026-10-16 ~02:30, unattended; user: "I think there are lots of artifacts when spacewarp is on")

| 0.7 | frame ms OFF (p95) | frame ms ON | compositor GPU OFF -> ON | mispresents OFF -> ON | notes |
|---|---|---|---|---|---|
| fixture, raw (smoothing off) | 24.2 | - | - | - | eyes 14.0 ms |
| fixture, always-on | 34.2 / 34.3 (43) | 32.7 / 33.9 | 19.3,18.5 -> 17.8,16.9 | 1137,1142 -> 948,975 | eyes 17.4; oscillates between 2x and 3x of 13.89 ms |
| fixture, smoothing on (auto) | 34.7 / 34.3 | 33.6 / 33.7 | 19.5,19.1 -> 18.2,18.2 | 1170,1148 -> 1005,1001 | flag 95-97% |
| crowd, raw | 31.4 (40.8) | - | - | - | eyes 20.1 |
| crowd, always-on / auto | 42.4 / 42.2 (51) = 3 x 13.89 | 41.8 | 27.8,26.3 -> 27.2,27.3 | 1419,1412 -> 1374,1378 | flag 89-100% |

Smoothing makes the real frame rate worse than raw at 0.7 on this bench (fixture 41 -> 29 fps, crowd 32 -> 24 fps) and does not give a clean 36 Hz.
Our EXT inputs again only change the compositor side slightly (~-1.5 ms compositor GPU, ~-16% mispresents on the fixture, ~nothing on the crowd) for +2 ms CPU.
Likely artifact source: `vr_frame_synthesis.cpp` header says it is "camera-seeded pyramidal image motion ... not a replacement for exact per-vertex geometry
velocity" (3-level optical flow on the final image at `vr_xr_frame_synthesis_scale` 0.25); `docs/vr-port/SPACEWARP.md` lists textureless areas, disocclusion,
transparency/particles, overlays, rapid motion as weak spots and "exact geometry motion passes" as the higher-quality follow-up. At render scale 0.45
with FSR the flow also runs on a 20%-pixel image. Display-side artifact quality cannot be captured here (IVRScreenshots returns the submitted eye images).

## Reference app (`Misc/frame/spacewarp-ref`): what SteamVR does with exact motion vectors

No third-party app is known to supply app motion vectors to SteamVR (the Meta XrSpaceWarp sample is Android only;
Alyx, Skyrim VR and the HL2 VR mod installed on the Frame use SteamVR's own reprojection/smoothing and no app MVs).
So `Misc/frame/spacewarp-ref` was written: OpenXR + Vulkan, a spinning-cube scene, **exact analytic motion vectors** and a
matched depth swapchain, `--api none|ext|fb`, a fixed GPU load (`--load-iters K`, about 1 ms per iteration at scale 1).
Its submitted vectors match the analytic values to 0.013 % (half-float rounding). Driver: `Misc/frame/run-ref-abba.py`
(temporary per-app overrides, restored on exit). Raw legs: `performance-results/ref-matrix/` on the Frame.

Windows are 12 s after a 6 s warmup (the headset standby blip lands in the first ~4 s). Compositor GPU is
`total_gpu_ms` from `frame-compositor-timings`.

| run | leg | app fps | compositor motion flag | app GPU ms | compositor GPU ms | mispresented | AppMVs log |
|---|---|---|---|---|---|---|---|
| auto, no load (fits 72 Hz) | none | 72.0 | 0 % | 1.1 | 3.0 | 0 | 0 |
| | ext (x2) | 72.0 | 100 % | 1.3 | 3.6 | 0 | 1 |
| auto, load 7 (app GPU ~5.5) | none | 72.0 | 0 % | 5.5 | 6.7 | 0 | 0 |
| | ext / fb | 72.0 | 100 % | 6.5 | 8.8 | 0 | 1 |
| always-on (override 3), no load | none | 36.0 | 100 % | 0.6 | 3.3 | 321 | 0 |
| | ext / fb | 36.0 | 100 % | 1.5 | 3.7 | 321 | 1 |
| auto, load 14 (over budget) | none / ext / fb | 36.0 | 100 % | 15.3 / 16.5 / 16.5 | 15.3 / 16.5 / 16.5 | 322 | 0 / 1 / 1 |

Findings:
- SteamVR accepts both `XR_EXT_frame_synthesis` and `XR_FB_space_warp`: one `Using motion encoding AppMVs.` line per run.
- Submitting vectors sets the compositor motion flag on 100 % of frames, even when the app already makes 72 Hz.
  With the app inside the budget that is pure cost: +0.6 to +2.1 ms compositor GPU, +0.2 to +1.0 ms app GPU (the motion pass).
- The cadence is chosen by SteamVR (72 Hz, or 2x = 36 fps when the app is over budget or always-on is forced) and does
  **not** depend on whether vectors are supplied: none, ext and fb give the same fps and the same mispresented counts.
- `mispresented` is a by-product of half rate (321 over 12 s in every half-rate leg). It does not measure quality.
- So the counters we can read (cadence, flag, mispresents, GPU ms) cannot show a benefit from app vectors, even with
  perfect vectors. The benefit is visual only (how the half-rate frames look), and nothing here captures synthesized frames.
- In the over-budget run the last `none` leg went back to 72 fps at 11.6 ms app GPU. SteamVR's auto throttle is hysteretic
  and the app's GPU time depends on the rate. Do not read single legs in isolation.
- The runtime alternates each eye's FOV by about +-0.25 px between frames (also with synthesis off). A previous-frame
  projection built from last frame's FOV therefore adds a uniform half-pixel shift. `--mv-fov cur` removes it. Quake's
  camera seed may have the same bias.
- Consequence for Quake: the compositor handles Quake's submission the same way it handles the ideal one. What differs is
  vector accuracy (estimated optical flow vs exact), which the counters cannot see.

### SteamVR's own counter confirms both apps' vectors arrive (2026-10-16)

The header of every `~/.local/share/Steam/logs/perfrecording-*.csv` carries `framesWithDepth` and `framesWithMotionVectors`
(SteamVR counting what the compositor received, independent of our logs). Across all recordings made on 2026-10-16:
- Quake, synthesis ON legs: 644 of 646, 614 of 617, 647 of 649 frames, i.e. essentially every frame had depth **and** motion vectors; OFF legs: 0.
- spacewarp-ref, `--api ext` and `--api fb`: 1323 of 1323, 1348 of 1348 (all frames); `--api none`: 0.
- So Quake's submission is accepted by SteamVR exactly like the exact-vector reference app. The open question is only vector accuracy.
- `run-ref-abba.py` prints `mv-frames ?` because it reads the wrong key from the collector JSON; read the CSV header instead.
  Recordings of back-to-back legs overlap in time, so the recording collector rejects them as ambiguous (the CSVs are still in the logs folder).

### Valve's documentation says why the counters show no gain (research 2026-10-16)

Source: partner.steamgames.com/doc/steamhardware/steamframe/compat/perf_criteria and .../vrpreferences and .../debugging.
- Submitting depth and motion vectors "will not automatically throttle your application". It lets users optionally choose a
  higher display refresh with better reprojection. So a gain only exists when the app is half rate (throttled) or the display rate
  is above the app rate, and then it is visual quality, not a counter.
- Per-app Force Always-On was ignored until SteamVR 2.17.10; throttling selection was fixed in 2.18.2 (our runtime is 2.18.2).
- Debug view: VR Settings > Developer > "Draw Performance Criteria Game Textures in Headset"; the Frame overlay line 3 shows
  D/M formats and "skip" (an app pose discontinuity skips smoothing for one frame).
- `XR_EXT_frame_synthesis` has a `REQUEST_RELAXED_FRAME_INTERVAL` flag (hint that the app renders at a reduced rate), and Godot
  exposes `skip_next_frame()` for teleports. Worth setting in Quake when it intentionally runs at half rate.
- Half-Life: Alyx has a native ARM64 port with depth reprojection, renders 36 fps shown at 72 Hz and probably does not submit
  motion vectors: the best known-good *reprojection* reference, not an MV reference.
- Only confirmed AppSW app on the Frame found: Into the Radius 2 (Quest APK via the third-party tool FramePort, needs approval).

### Web research 2026-10-16: is there a known-good app that supplies motion vectors to SteamVR?

Three independent research passes (Valve/SteamVR side, open source, the user's Steam library). Result: **no.** Nobody has published
proof that app-supplied vectors give a visible benefit on SteamVR or the Steam Frame, and no title is confirmed to submit them.
- Candidates, best first:
  - Godot 4.6+ (MIT) `OpenXRFrameSynthesisExtension`: 3D vectors, current NDC minus previous, R16G16B16A16_SFLOAT, Mobile/Compatibility renderers only
    (not Forward+). Valve documents a Godot "Steam Frame Arm64" export; Godot 4.7.2 runs natively on the Frame (godot issues 124314 / 124520).
    Works on Pico 4 Ultra and Quest 3; SteamVR untested. A second independent vector generator, not a known-good.
  - Half-Life: Alyx (installed on the Frame): native ARM64, 36 fps rendered shown at 72 Hz, depth reprojection. Vectors unconfirmed;
    `framesWithMotionVectors` in its perfrecording CSV answers it directly. Best reprojection reference.
  - Into the Radius 2 via the third-party FramePort tool: the only confirmed AppSW app on the Frame (needs the Quest build and the tool).
  - Meta XrSpaceWarp sample (Android/GLES only), Unreal 5.7+ `xr.OpenXRFrameSynthesis` (needs a source engine build): speculative.
  - Nothing usable in the user's PC library (no source says BONELAB, B&S, GORN, ITR1, Contractors, Zero Caliber, Creed submit vectors in the PC build).
  - Monado, WiVRn, ALVR, Bevy, hello_xr, CTS: no support for either extension.
- Khronos runtime inventory: `XR_EXT_frame_synthesis` supported on "SteamVR (Steam Frame)" only, not on SteamVR Windows or Linux x64.
- Valve's own extension `XR_VALVE_app_space_delta_pose` (advertised by the Frame's SteamVR; ValveSoftware/Unity branch `app-space-delta-pose`): Valve says the
  compositor synthesizes frames from consecutive rendered frames by itself and the one input it cannot infer is how far the app moved the player (locomotion).
  Quake submits `appSpaceDelta` in the EXT struct but does not use the Valve extension.
- Y direction of the vectors is "essentially undefined" in the spec (godot PR 119056: Meta and Pico match FB_space_warp, Android XR Vulkan is reversed).
  SteamVR's expectation is not documented. A wrong sign would look like heavy artifacts and none of our counters would notice.
- OpenVR 2.15.6 added `VRTextureWithMotion_t` / `Submit_TextureWithMotion` (a third, OpenVR path).
- Valve says app vectors "will not automatically throttle your application"; the user can run a higher refresh with better reprojection.
  Unworn headsets throttle apps (standby), so only wearer tests are representative for cadence.
- Open artifact hypotheses, none tested visually yet: (1) Quake's vectors are estimated (optical flow at 0.25 scale), (2) vector Y sign / convention,
  (3) locomotion (appSpaceDelta or the Valve extension), (4) the +-0.25 px FOV jitter, (5) transparents/particles/fast hands (Unity's documented weak spots).
- One fetched Meta documentation page contained text telling an agent to install and run a Meta CLI tool; the research agent ignored it.

### Godot 4.7.2 as an independent engine (2026-10-16): same behaviour, and the real cost of supplying vectors

`Misc/frame/spacewarp-godot` is a small Godot 4.7.2 Mobile-renderer scene (moving cubes, head-locked GPU-load quad, same summary fields as
spacewarp-ref). Godot emits the vectors and depth itself (`xr/openxr/extensions/frame_synthesis=true`; runtime toggle
`OpenXRFrameSynthesisExtension.enabled`). Built with the project-local official arm64 templates of NightfallContractsGodot
(`C:/Devstuff/GameDev/NightfallContractsGodot/.tools/godot/templates`, no download), deployed to `~/Games/spacewarp-godot/`,
run with `run-ref-abba.py --bin ~/Games/spacewarp-godot/run-godot-test.sh --legs "gon=-- --fs on;goff=-- --fs off"`.
SteamVR key: `system.generated.openxr.spacewarpgodot.spacewarp-godot.arm64`. Raw legs: `performance-results/godot-*`.

| run | leg | app fps | motion flag | app GPU ms | compositor GPU ms | mispresented |
|---|---|---|---|---|---|---|
| auto, no load | FS off / on | 72 / 72 | 0 % / 100 % | 1.2 / 0.5 | 3.3 / 2.7 | 0 |
| always-on, no load | FS off | 36 | 100 % | 0.8 | **4.0** | 320 |
| | FS on (x2) | 36 | 100 % | 0.7 | **3.1 to 3.2** | 320 |
| auto, load 60 (~6.7 ms) | FS off / on | 72 / 72 | 0 % / 100 % | 6.6 / 6.7 | 7.6 / **8.8** | 0 |
| auto, load 100 (~10.6 ms) | FS off (x2) | **72** | 0 % | 10.6 | 11.6 | 0 |
| | FS on (x2) | **36** | 100 % | 14.4 | 14.5 | 320 |

SteamVR's own perfrecording counters: `framesWithDepth` and `framesWithMotionVectors` = 1431 of 1431 frames with FS on, 0 with FS off.

Reference app, same question (auto override, `--load-iters`): at 12 iterations `none` stays 72 fps (compositor GPU 11.0, flag 0 %, mispres 0) twice,
while `ext` is throttled to 36 fps twice (flag 100 %, compositor GPU 14.9, mispres 322). At 10 iterations both fit, but `ext` costs
+1.5 ms compositor GPU (10.7 vs 9.2) and its second leg starts to slip (70.5 fps, 49 mispres).

What this means (two independent apps agree, so it is SteamVR/Frame behaviour, not our Quake bridge):
- **Supplying vectors costs +1.2 to +2.2 ms compositor GPU (plus +0.1 to +1 ms for the app's vector pass) even when the app already makes 72 fps**
  and the compositor sets the motion flag on every frame. For an app that only just fits the budget this tips SteamVR into throttling to 36 fps
  (half rate + synthesis) where the same app without vectors stays at a real 72 fps.
- **Once SteamVR's own smoothing is running** (always-on, or already over budget) vectors are cheaper than SteamVR's own motion estimate
  when the content is rich: Godot -0.8 to -0.9 ms (4.0 to 3.1). With trivial content (spacewarp-ref) it is +0.4 ms. Quake's fixture measured -1.5 to -3.5 ms.
- Cadence (72 or 36) and `mispresented` are decided by the load, not by the vectors; the visual quality of the synthesized frames is what the vectors buy.
- Policy implication for Quake: submit vectors only while the app is (or should be) at half rate. Always submitting costs headroom at 72 Hz and can itself cause the
  36 fps throttle. A dynamic mode needs hysteresis because the throttle decision flips with the vectors.
- GPU time depends on the cadence (same load: 11.6 ms at 72 fps vs 15.3 ms at 36 fps in spacewarp-ref, 10.6 vs 14.4 in Godot): the GPU downclocks when it idles.
  Compare legs at the same cadence, or accept the clock effect.

## Godot 4.7.2 source vs. Quake's producer: what Godot does that we do not (2026-10-16)

Purpose: the wearer sees graphical artifacts with `vr_xr_frame_synthesis 1`, and Godot is an open-source engine whose EXT path SteamVR accepts
(see the Godot section above). Our producer was read in full (`vr_frame_synthesis.cpp`, `vr_synthesis_shaders.hpp`, `vr_synthesis_math.hpp`,
`vr_openxr_synthesis_compat.hpp`, `vr_backend_openxr.cpp` submission and swapchain code); Godot's side comes from a research agent that read
`openxr_frame_synthesis_extension.cpp`, `openxr_api.cpp`, `renderer_viewport.cpp`, the Mobile/Compat renderers and the PR history (WebFetch-summarised,
so function-level, not line-level; treat Godot specifics as agent-reported, not independently verified). **Nothing here is a visual result: no wearer has
compared any variant.** Items are ranked by how plausible they are as the artifact cause, not by proof.

| # | Godot | Quake (this branch) | Plausibility |
|---|-------|---------------------|--------------|
| 1 | Vectors are **Y-up**: the Vulkan Mobile shader negates `ndc.y` "to fit XR_FB_space_warp"; the Compat (GL) shader emits plain GL NDC (also Y-up). Scale is `{1,1,1,0}`; a `flip_y` project setting only exists in 4.8 (PR 119056: Meta and Pico match FB_space_warp, Android XR/Vulkan needs the flip). | **Y-down** ("Vulkan NDC"): `toNdcMotion` = `(-2*flow.x, +2*flow.y)`. Confirmed on `vrcalibration_15`: content that moves up in the image exports a *negative* Y. | High: SteamVR's convention is undocumented. If it matches FB_space_warp, every vertical vector is inverted (pitch, bob, vertical parallax). One-line A/B; needs a wearer. |
| 2 | Exact geometry velocity: a separate opaque-only pass computes `ndc_cur - ndc_prev` per vertex with previous object/skin/multimesh transforms. Hands and animated objects are exact. | Camera motion from depth is exact (seed). Everything that moves on its own (viewmodel/hands, monsters, projectiles) is **estimated by Lucas-Kanade** on the final colour image: 3x3 window, luma only, 3 levels x 2 iterations, correction limited to about +-28 px. | High for anything near the hands. The viewmodel always moves relative to the world and is close to the eye. |
| 3 | 3D vectors; the 2D bit is never set, Z carries the NDC depth delta. | Always `USE_2D_MOTION_VECTOR_BIT`, Z written as 0. The spec (and our own SPACEWARP.md) says 2D may lower quality. | Medium. |
| 4 | MV and depth swapchains at the runtime-recommended rect, array size 2. | SteamVR recommends **0x0** (logged by spacewarp-ref: `synthesis_recommended_motion=0x0`), so we use `0.25 x 1728` = **432x432** per eye: one vector per 4x4 eye pixels. Godot's fallback size with a 0x0 recommendation was not logged. | Medium: blocky vectors at silhouettes. `vr_xr_frame_synthesis_scale 1` is the test. |
| 5 | Transparent, sky and UI draw nothing into the MV pass: motion 0, depth cleared to 0 (far). | LK runs over the whole image including UI, particles and translucent surfaces; depth is the opaque scene depth under them. | Medium (effects, particles, UI). |
| 6 | nearZ/farZ come from the camera's own finite planes (always nearZ != farZ; the Quest rejects equal values). | The projection is **infinite-far reversed-Z** (`current_projection` of the capture: z row is the constant near, w = view x), but we submit `farClip` (2080 m) and near (3.2 mm). Runtime linearisation then differs from truth by (f-z)/f: ~5% at 100 m, ~24% at 500 m. | Low (small for ordinary levels). |
| 7 | The synthesis struct is attached every submitted frame except one explicit `skip_next_frame()`. | Dropped whenever history is invalid: water warp, portals, snap turns/teleports (>1 m or >~32 deg), any non-submitted frame (reset costs two frames), `r_norefresh`. Mixed frames may flip SteamVR between app vectors and its own estimate. | Low to medium. The rate is not measured yet. |
| 8 | Previous matrices: previous camera transform with the **current** per-eye projection and eye offset. | True previous per-eye view-projection, including SteamVR's per-frame FOV jitter. | Low (about 0.5 px), different but not suspected. |
| 9 | Keeps `XrCompositionLayerDepthInfoKHR` behind the synthesis struct (PR 114697). D24S8 depth, RGBA16F motion, one array swapchain with MUTABLE_FORMAT. | No depth-info layer; D32F depth, RGBA16F motion, separate per-eye swapchains. | Not suspected: SteamVR accepts both layouts (counters). |

Equivalent behaviour (no action): app-space delta is the origin delta in the previous frame's coordinates in both (Godot `previous_transform.affine_inverse() *
world_transform`; ours `applicationDelta`); relaxed flag optional in both; MV is current minus previous in X in both; both release the images before `xrEndFrame`.

Offline check on the saved fixture (`vrcalibration_15_L/R`, static scene, camera seed = ground truth; analysis scripts only in the scratchpad):
- Flow vs exact camera seed: median 0.05 px, but **89% of the error mass sits within 4 px of depth edges** (9% of pixels, mean 62 px there). The pyramid propagates
  coarse flow across depth discontinuities and never re-anchors to the seed at full resolution. The fixture's motion is exaggerated (mean 133 px) so real play is smaller.
- Backward-warping the previous colour with each field and comparing to the current colour: zero motion 0.021, camera seed alone 0.009, exported seed+LK 0.009
  (edges 0.027-0.033 for both, direction differs per eye). **LK neither helps nor hurts measurably on static content**, and the fixture has no dynamic content,
  so it cannot confirm or clear item 2.

Cheap variants that isolate the causes (each is a cvar; the quality judgement needs one wearer session, because synthesized frames cannot be captured):
1. `flip_y` (item 1), 2. camera-seed-only mode without LK (item 2), 3. `vr_xr_frame_synthesis_scale 1` (item 4), 4. 3D flag with Z delta (item 3).
All four exist now; see the next section for what they are, how they were checked and what checking them turned up.

## Artifact-isolation variants, and two producer defects found while checking them (2026-10-16)

Cvars (all `CVAR_NONE`: read every frame, not saved, settable from the console or a cfg while the game runs; all listed in the profile settings):

| cvar | default | does |
|------|---------|------|
| `vr_xr_frame_synthesis_flip_y` | 0 | 1 negates the exported Y (item 1: Godot/FB_space_warp are Y-up) |
| `vr_xr_frame_synthesis_flow` | 1 | 0 skips the Lucas-Kanade pass and exports the camera seed alone (item 2) |
| `vr_xr_frame_synthesis_3d` | 0 | 1 clears the 2D bit and writes Z = window-depth delta of the camera reprojection (item 3; RGBA16F motion only) |
| `vr_xr_frame_synthesis_scale` | 0.25 | 1 = full-size vector images (item 4; existed before) |
| `vr_xr_frame_synthesis_mask` | 1 | **new default behaviour**, see defect 1; 0 = as before |

The OpenXR log's `FRAME_LAYERS` line carries `synthesis_variant=flip_y:..,flow:..,3d:..,mask:..`, the diagnostic capture JSON has a `variant` object, and
`run_frame_synthesis_smoke.sh` takes `QVR_SYNTH_FLIP_Y`, `QVR_SYNTH_FLOW`, `QVR_SYNTH_3D`, `QVR_SYNTH_MASK` (0/1; unset keeps the default).

Checked without a wearer (one one-shot capture per variant on the Frame, `performance_valid` false): every export equals the field its settings imply, to fp16
storage precision (p99 error <= 0.005 px of the 432x432 grid). `flip_y 1`: export Y is the negated flow Y (correlation -0.99999, default +0.99999). `flow 0`:
the flow file is bit-identical to the camera seed and the export is `(-2*seed.x, +2*seed.y)`. `3d 1`: Z equals the seed's depth delta (0.004% of pixels differ,
fp16). This proves the mechanics, **not** how any of it looks.

### Defect 1: the lenses' hidden area was exported as the nearest surface with screen-wide vectors

`vr_visibility_mask` (default 1) draws the lenses' hidden area at the near plane, so 15.65% of every eye's depth is exactly 1.0 (reversed Z; also present in the
2026-10-14 captures, so it is not new). The depth/seed pass reprojected those pixels like surfaces a few millimetres from the eye: camera-seed magnitude up to
17 NDC (8.5 UV). Three consequences, measured on the live captures:
- the export carried those vectors for the mask (|motion| up to 17 NDC) together with depth 1.0, the nearest value in the frame;
- the flow pyramid blends seeds bilinearly at coarse levels, so the contamination leaked into the **visible** image next to the mask: visible pixels within 3 px
  of the mask had |motion| median 0.99 NDC (right eye), p99 8.7, max 10.6, against a far-field p99 of 0.017 in the same capture (left eye: p99 5.7, max 9.4);
- nothing in the repo's checks looked at it, because the fixture has no hidden area.

Fix (`vr_xr_frame_synthesis_mask 1`): the seed shader writes zero for pixels at exactly the nearest depth, and the export writes zero motion, alpha 0 and the far
depth for them. On the live capture the mask exports exactly 0 / 0 / far, and the band next to it matches the far field (max about the seed's p99). GL fixture
`hiddenAreaFixture` covers both Z conventions and `mask 0`. **Untested with a wearer**: whether this is the artifact they see. It is plausible (screen-wide
vectors and a near-plane depth at the lens edge are what a compositor would smear into the visible periphery) but it is a hypothesis, not a result.

### Defect 2: about 20% of the left eye's flow texels were never written

In the smoke captures of the `next` port (7 of 7 left-eye captures with LK flow; the right eye 0% in the 4 checked), about 20% of the flow texels held NaN in scattered 16x4 blocks, including
the alpha channel that the shader always writes as 1.0, so these are texels the pass did not cover, not shader output. The 2026-10-14 captures (frames 14/15)
had none, and the pre-merge binary shows the same, so `origin/next` is not the cause. The export already fell back to the camera seed on non-finite flow, so no
garbage reached the compositor, but those pixels had no LK estimate, and the repo's own validator reported `all_float_images_finite_and_sized: false`, which is why
the smoke script exited 2 (earlier notes attributed that exit code to the missing tests directory; that was wrong).

Mitigation: each flow pass target is cleared to (0,0,0,0) before it is drawn, and the export trusts a flow texel only where alpha > 0.5 (so finite garbage is rejected
too, not just NaN). Result: 0.0% uncovered texels in all four eyes of two live runs, validator `valid: true`, smoke exit 0. The mechanism is not established: a
clear changing coverage suggests Turnip leaves tiles unwritten when a draw targets undefined content, but that is inference. GL fixture `uncoveredFlowFixture`
covers an unwritten texel and a NaN-alpha texel. The cost of the extra clears was not measured (they are in-pass on a tiled GPU, so it should be small).

### What a wearer session should compare
Same scene, same minute, toggled live: default (`mask 1`), `vr_xr_frame_synthesis_mask 0`, `flip_y 1`, `flow 0`, `3d 1`, `vr_xr_frame_synthesis_scale 1`. Defect 1 and 2 are
fixed in the default, so the first comparison is simply old (`mask 0`) against new. Judge artifacts at the image periphery first, then around the hands.
