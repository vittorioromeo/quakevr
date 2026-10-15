# Read-only compositor timing collector

`frame-compositor-timings.cpp` is a bounded background OpenVR client for Steam
Frame. It neither submits a scene nor changes settings. It refuses to initialize
unless `vrserver` is already running, uses `VRApplication_Background`, and shuts
down only its own client. Build/run it in scratch storage; no installed files
need changing.

```sh
clang++ -std=c++17 -Wall -Wextra -Werror frame-compositor-timings.cpp -ldl -o frame-compositor-timings
./frame-compositor-timings --self-test
./frame-compositor-timings --run 10 > timings.jsonl
```

Place the existing `openvr_capi.h` alongside the source. `--run` accepts 1–60
seconds. The loader path is `/opt/steamvr/bin/linuxarm64/libopenvr_api.so`.
No display or `XR_RUNTIME_JSON` is needed. This tool calls OpenVR, not OpenXR.
The collector uses the header's exact `IVRCompositor_029` function table and
the native Linux 188-byte, four-byte-packed timing ABI; the generated C header
does not reproduce the native struct's packing directive.

It polls 128 timing records every 100 ms, excludes the initial history, and
keeps the latest observation of each `(system time, frame index)` pair. The
summary's `unique_app_frames` is a count of timing records, including empty
scene/dashboard records when the reported focus/renderer PID is zero. Counts
are not a per-display-present census. The last observed frame may still be
updated by the compositor after collection ends. A focus/renderer change while
the initially focused process is still alive makes attribution unreliable and
produces exit code 3, even when the runner also requested stop. A normal return
to Dashboard after that process exits is recorded separately and does not
invalidate the captured measurement. Exit 0 proves collection only.

`run-spacewarp-headset-abba.sh` starts collection after Quake owns scene focus,
stops it when the launched `ironwail` process exits (not when the smoke script
finishes later artifact validation), and rejects exit 3. It snapshots the SteamVR
recording directory before each leg, toggles recording around each individual
leg so each run gets its own finalized CSV, then waits for one new stable CSV whose
mtime overlaps the actual game-process interval and whose `appName` identifies Ironwail/Quake. Missing,
ambiguous, unfinished, or mismatched recordings are marked indeterminate. The
recording identity JSON saves app ID/name, timestamps, and the tested binary's
SHA-256. SteamVR's per-recording rows also expose the headset refresh rate; the
collector preserves those values through the CSV for the validator.

Valve defines `VRCompositor_ReprojectionMotion` (`0x08`) as motion smoothing
triggered for that frame. `0x04` only reports asynchronous reprojection mode;
it does not establish that a scene was reused or interpolated. See the
[official header's flags, timing ABI and interface comments](https://github.com/ValveSoftware/openvr/blob/master/headers/openvr.h).
Neither `0x08` nor `IsMotionSmoothingEnabled` identifies whether the runtime
used an app's `XR_EXT_frame_synthesis` motion/depth inputs.

For the next coordinated wearer test, run matched short synthesis OFF/ON
captures while Quake remains the reported focus/renderer PID and poses are
valid. Preserve app EXT/freshness logs, raw timing flags, presents and frame
intervals. Compare motion-triggered records and cadence, and inspect runtime
motion/depth diagnostics plus visible dynamic-object interpolation. If both
OFF and ON set `0x08`, that alone cannot distinguish ordinary SteamVR motion
smoothing from EXT input consumption. Invalid poses, PID zero or changing
scenes cannot close the gameplay evidence gate.

The 2026-10-14 one-second smoke ran with no scene and no valid HMD poses:
65 new records, all `0x04`, zero `0x08`, smoothing supported but disabled.
The updated collector repeated that one-second smoke after adding the
`scene_changed_while_app_alive` field; it still observed 65 records and zero
valid poses. These runs validate the collector call path and JSON schema only.
The updated binary compiled with warnings-as-errors and passed its ABI
self-test. The latest exact output is in
[`spacewarp-collector-attribution-20261014`](../../../performance-results/spacewarp-collector-attribution-20261014/RESULTS.md); the earlier raw smoke is in
`performance-results/spacewarp-batch-state-review-20261014/collector-runtime.jsonl`.
