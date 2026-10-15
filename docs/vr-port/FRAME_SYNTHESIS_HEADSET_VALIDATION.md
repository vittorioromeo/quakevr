# SteamVR headset validation for frame synthesis

This procedure separates OpenXR submission, SteamVR's receipt of motion/depth,
and actual headset-visible reprojection. It is the remaining verification gate
for the Quake VR `XR_EXT_frame_synthesis` path.

## Enable SteamVR diagnostics

With SteamVR running and the headset on:

1. Open the SteamVR Dashboard and turn **Advanced Settings** on in the lower
   left. In **Developer**, enable **Draw Performance Assessment in Headset**.
   This shows the assessment and performance graph while a VR title runs.
2. In **VR Settings > Developer**, choose **Draw Performance Criteria Game
   Textures in Headset** and select depth or motion vectors. Valve documents
   this menu as the way to inspect the actual submitted textures.
3. From the Quick Access Menu open **Clock > Performance > VR Settings** and
   enable **Record VR Performance**. SteamVR writes a `perfrecording-*.csv`
   under `~/.local/share/Steam/logs` while a local VR app runs. Quake's smoke
   runner copies fresh recording CSVs and a fresh `perfcriteria.txt` when they
   are produced.

These setting names and paths are from Valve's current
[performance assessment documentation](https://partner.steamgames.com/doc/steamhardware/steamframe/compat/perf_criteria)
and [Steam Frame debugging guide](https://partner.steamgames.com/doc/steamhardware/steamframe/debugging).

## Run the prepared A-B-B-A capture

`Misc/frame/run-spacewarp-headset-abba.sh` automates four 450-frame
OFF/ON/ON/OFF captures and runs the compositor timing collector alongside
each focused Quake session (bounded by app exit, with a 60-second safety cap).
It refuses to start unless SteamVR is already running,
the HMD pose is valid, and Quake is not already running. It waits for Quake's
PID to become the compositor scene focus before collecting timings. The
collector runs concurrently and is stopped when the Quake process exits, so
later smoke-script validation and a return to Dashboard do not extend the
measured interval. A
60-second cap prevents a hung app from collecting indefinitely. If focus does
not arrive, the wrapper interrupts the scratch run and the smoke runner restores
the temporary configuration and QuakeC files. It holds Vulkan interop and
deferred queue waits constant in all four legs (deferred waits default on,
override with `QVR_VULKAN_DEFER_WAIT=0` only for a separate transport test),
so the A-B-B-A comparison isolates synthesis. The default
`spacewarp_motion_fixture` spawns one crate in the monster-free calibration
room, 80 units ahead, and reverses a horizontal impulse every 30 frames.
Position samples verify repeated movement while the crate stays on the floor and
inside the test lane instead of settling or falling out of the room. It has no
map monsters, enemy AI or player fire. The
`spacewarp_one_mover` firing-range case remains available, but its map actors
and ambient VR particles make it a less isolated probe.
`QVR_SCENARIO=gore_headpop_24` selects the dynamic
kill-effects replay when that specific workload is desired.

For an in-game enemy death that is not yet covered by the scripted workload,
Quake's built-in demo recorder can preserve the world/server event stream:
start `record spacewarp_death_test` after loading the map, reproduce the kill,
then run `stop` after about ten seconds. Keep the clip around 15 seconds total,
with the shot near its start. This is useful for replaying the enemy,
ragdoll, dropped items and world effects while profiling. A `.dem` does not save
HMD or controller tracking, so it cannot reproduce the wearer's physical aim
or head motion. Keep the headset worn during `playdemo` when collecting live
OpenXR and compositor evidence; use the existing motion fixture or a separate
pose take when repeatable headset/controller movement is needed. The smoke
runner accepts a recorded basename through `QVR_DEMO` and measures its playback
for a fixed frame window. For the example above, copy
`spacewarp_death_test.dem` into the Frame's
`/home/steamos/Games/QuakeVR-Unleashed-native/game/quakevr/` directory and run:

```sh
QVR_PRECHECK=1 \
QVR_DEMO=spacewarp_death_test \
QVR_FRAMES=1080 \
QVR_TEXTURE_VIEW=2 \
QVR_SOURCE=/path/to/source \
QVR_BIN=/path/to/arm64/ironwail \
QVR_COMPOSITOR_COLLECTOR=/path/to/frame-compositor-timings \
QVR_OUT_BASE=/path/to/results/spacewarp-death-precheck \
bash Misc/frame/run-spacewarp-headset-abba.sh
```

The 1080-frame measurement is 15 seconds at 72 Hz. The runner checks that Quake
reports opening the requested demo and that the complete frame window was
recorded. For OFF/ON A-B-B-A comparisons, omit `QVR_PRECHECK=1`; each leg
replays the same demo with the same frame count.

SteamVR's installed defaults set `recordStats` and `perfGraphInHMD` to false.
The runner now reads, enables, verifies, and restores `recordStats` and the
depth/motion texture-view setting through SteamVR's `vrcmd` CLI. Set
`QVR_TEXTURE_VIEW=1` for depth or `=2` for motion; `=0` disables the texture
overlay. The short `QVR_PRECHECK=1` defaults to motion view (`2`), while the full
performance A-B-B-A defaults to the normal image (`0`) so the diagnostic view
does not affect its measurements. The ON legs stop as indeterminate unless
exactly one fresh SteamVR performance CSV is available and validates. A missing
CSV can no longer look like a completed SpaceWarp verification.

On the Frame, after copying the latest source/build and collector into scratch
storage, run the short ON-only precheck first. It defaults to 180 frames and the
controlled crate-motion fixture; set `QVR_TEXTURE_VIEW=1` or `=2` to inspect
depth or motion in the headset. The smoke runner writes
`motion-fixture-validation.json`, which checks repeated crate impulses and
position changes, workload counts, and app submissions separately from
HMD/controller tracking.
An app-valid result without tracked headset samples remains unworn evidence:

```sh
QVR_PRECHECK=1 \
QVR_TEXTURE_VIEW=2 \
QVR_SOURCE=/path/to/source \
QVR_BIN=/path/to/arm64/ironwail \
QVR_COMPOSITOR_COLLECTOR=/path/to/frame-compositor-timings \
QVR_OUT_BASE=/path/to/results/spacewarp-precheck \
bash Misc/frame/run-spacewarp-headset-abba.sh
```

When headset tracking is unavailable, an app-only OFF/ON/ON/OFF profile can
compare producer and interop cost on the exact same moving crate. This runner
does not change SteamVR settings and cannot validate runtime receipt or
generated display frames:

```sh
QVR_SOURCE=/path/to/source \
QVR_BIN=/path/to/arm64/ironwail \
QVR_OUT_BASE=/path/to/results/spacewarp-motion-abba \
bash Misc/frame/run-spacewarp-motion-abba.sh
```

After the precheck reports valid head tracking, successful stereo submissions,
and runtime depth/motion counts—and the wearer has inspected the texture—run
the full A-B-B-A comparison:

```sh
QVR_SOURCE=/path/to/source \
QVR_BIN=/path/to/arm64/ironwail \
QVR_COMPOSITOR_COLLECTOR=/path/to/frame-compositor-timings \
QVR_OUT_BASE=/path/to/results/spacewarp-headset-$(date -u +%Y%m%d-%H%M%S) \
bash Misc/frame/run-spacewarp-headset-abba.sh
```

Keep the headset on and Quake focused throughout the sequence. The runner
records pose/focus snapshots, app GPU/CPU profiles, OpenXR logs and raw
compositor timing flags for each leg. It restores SteamVR's original recording
and texture-view settings on normal exit, test failure, or interruption. The
texture overlay is visible in the headset; wearer or Steam Link inspection is
still needed to assess whether the image is coherent.

## Evidence to collect

For each test, capture the same deterministic workload at the same resolution,
refresh rate and eye scale with synthesis disabled and enabled. Run the enabled
case twice if time permits. Keep the headset worn and the VR app in focus.

- Quake's `FRAME_LAYERS` must show synthesis enabled, and
  `FRAME_SYNTHESIS_CONTENT` must repeatedly show `fresh=1/1`,
  `app_delta_shared=1`, and `submitted=1`. The latter is emitted only after
  `xrEndFrame` succeeds.
- `FRAME_DIAG` must show `present=1`, a valid head pose, and valid left/right
  controller poses. This proves the app is tracking the headset/controllers,
  rather than rendering a held desktop frame.
- SteamVR's performance graph reports submitted depth/motion formats as
  `Dxx`/`Mxx`. Its recording CSV includes `framesWithDepth`,
  `framesWithMotionVectors`, and `reprojectedFrames` totals.
- The in-headset texture view must show a coherent depth field and changing
  motion vectors in both eyes while turning the head and moving past a static
  scene. Check a moving enemy and weapon effects separately; the Quake flow
  fallback marks degraded pixels with alpha zero in formats that provide an
  alpha channel, but SteamVR may not visualize that channel.
- Inspect the SteamVR overlay's status and target frame time, and save the
  fresh `perfcriteria.txt` plus the performance CSV.

An `Mxx`/`Dxx` format and nonzero buffer counts prove that frames included
motion/depth buffers. They do not prove those vectors are geometrically correct
or that an intermediate frame was generated. Valve's `reprojectedFrames` also
counts compositor reprojection events due to slow or throttled application
frames; it is not by itself proof that `XR_EXT_frame_synthesis` caused them.
The enabled/disabled comparison, texture display and wearer-visible motion must
be assessed together.

After the run, validate the OpenXR log against the newly written Valve recording:

```sh
python3 Misc/quakevr/tests/validate_frame_synthesis_headset.py \
  /path/to/qvr_openxr.txt.test \
  /path/to/perfrecording-steam.app.*.csv \
  --require-controllers \
  --output headset-validation.json
```

The validator requires repeated `present=1`/valid-head samples, fresh successful
stereo synthesis-content submissions, and nonzero runtime `framesWithDepth` and
`framesWithMotionVectors` totals. `--require-controllers` also checks for valid
controller-pose diagnostics. A pass proves runtime receipt of both texture
classes during a present headset session; inspect the depth/motion debug
textures and the image in the headset to assess correctness.

## Exercise the relaxed-interval hint

The OpenXR `XR_FRAME_SYNTHESIS_INFO_REQUEST_RELAXED_FRAME_INTERVAL_BIT_EXT`
flag is only a hint: the runtime may choose another interval regardless of the flag. Compare two otherwise identical
enabled captures with `vr_xr_frame_synthesis_relaxed` set to 0 and 1. The smoke
headset ABBA runner keeps the hint off for both OFF and ON legs by default, so
that comparison isolates synthesis. Override with `QVR_RELAXED_INTERVAL_ON=0|1` and
`QVR_RELAXED_INTERVAL_OFF=0|1`. The single-run smoke runner exposes this as
`QVR_RELAXED_INTERVAL=0|1`, and the `FRAME_LAYERS` log records
`synthesis_relaxed=0|1`. `QVR_HZ=72` sets the simulation/host cap; it does not
set the headset display refresh. Record actual submitted FPS, refresh,
compositor reprojection, CPU/GPU frame time, and perceived image quality. Do
not infer synthesis cadence from the requested flag.

## Current gate

The corrected automated 450-frame combat replay now validates the workload
(24 target deaths) with both synthesis OFF and ON in an A-B-B-A sequence. ON
sessions enabled `XR_EXT_frame_synthesis`, created RGBA16F motion and D32 depth
swapchains, and repeatedly submitted fresh per-eye synthesis metadata. These
are application-side results only: the headset-presence sensor still reported
`present=0`, no Valve `perfrecording-*.csv` or runtime texture-view capture was
produced, and no generated frame was confirmed. In this logger, `head` is
gated on the headset-presence sensor; it is not a direct copy of
`xrLocateViews` validity. Some `VISIBLE` samples still had valid orientation
and position bits (`flags=0xf`) while `present=0`.

The unworn ON replay also cost more than OFF: mean frame time rose from 34.59
to 41.17 ms and mean `gpu_eyes_ms` from 26.29 to 34.71 ms. This is a warning
that synthesis/interop overhead needs investigation; it does not measure the
wearer-visible cadence or generated-frame benefit. See
`performance-results/spacewarp-vulkan2-gore-replay-20261014/RESULTS.md` in the
workspace for run metrics and captured logs.

**Latest reviewed-source replay (2026-10-14):** a fresh 450-frame
OFF/ON/ON/OFF sequence passed all four deterministic workloads (24 deaths
each). ON runs repeatedly submitted fresh motion/depth for both eyes; mean
frame time was 32.08 ms OFF and 35.03 ms ON. The current Frame state probe
reported no valid HMD pose, no focused scene, and SteamVR Dashboard visible.
No Valve performance CSV or headset texture capture was generated, so this is
still app-side evidence only. See
`performance-results/spacewarp-batch-state-review-20261014/RESULTS.md`.

**Controlled-motion fixture (2026-10-14):** the runner now defaults to
`spacewarp_motion_fixture`, using the monster-free `vrcalibration` room. One
crate was flung during a 180-frame capture and moved 105 units before settling.
The scene had zero live monsters, VR particles and decals; mean app frame time
was 22.56 ms and mean GPU-eye time 8.66 ms. Eight OpenXR log samples showed
fresh stereo synthesis content submitted successfully. The run was still
unworn (`present=0`, `head=0`) and had no SteamVR performance CSV, so it only
validates the app-side motion path and the controlled workload. Details and raw
artifacts are in
`performance-results/spacewarp-motion-fixture-probe-20261014/RESULTS.md`.
The app-only OFF/ON/ON/OFF comparison found ON added 15.6% to mean GPU-eye time
and 31.0% to mean CPU-busy time, with almost unchanged frame p99. OFF retains
the Vulkan color-image path, so this measures synthesis motion/depth overhead
beyond shared color transport. No tracked HMD was present; see
`performance-results/spacewarp-motion-abba-20261014/RESULTS.md`.

The SteamVR compositor log also recorded `Failed to signal gpu fence` once at
each XR app connection. A separate color-only OpenGL control reproduced the
same startup entry with synthesis and Vulkan interop disabled. This is not
isolated to the SpaceWarp path; its cause with a tracked wearer remains open.

## References

- Khronos [`XR_EXT_frame_synthesis`](https://registry.khronos.org/OpenXR/specs/1.1/man/html/XR_EXT_frame_synthesis.html)
  and [`XrFrameSynthesisInfoFlagBitsEXT`](https://registry.khronos.org/OpenXR/specs/1.1/man/html/XrFrameSynthesisInfoFlagBitsEXT.html).
- Valve [Performance Assessment Overlay](https://partner.steamgames.com/doc/steamhardware/steamframe/compat/perf_criteria)
  and [Steam Frame Debugging](https://partner.steamgames.com/doc/steamhardware/steamframe/debugging).
- [SpaceWarp implementation status](SPACEWARP.md) and
  [repeatable test runner](../../Misc/quakevr/tests/run_frame_synthesis_smoke.sh).
