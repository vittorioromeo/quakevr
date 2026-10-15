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
