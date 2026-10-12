# Steam Frame native port: development and verification

Experimental ARM64/Linux port of **Quake VR: Unleashed 1.0.0** by Vittorio Romeo.
Tested on a physical Steam Frame on 2026-10-12. The wearer confirmed that Quake
entered the game in VR. **Performance still needs improvement in some locations.**
This is a community port, not an official upstream release or a general claim
that every gameplay feature has been tested.

![Native Steam Frame gameplay, captured from the compositor](images/steam-frame-native-gameplay.png)

## Working configuration

- Native aarch64 engine, OpenGL/GLX, SteamVR OpenXR 2.18.2.
- Upstream base: `v1.0.0` (`e844de35f4854dc661ad2d697895048d80bb4b40`).
- Steam shortcut marked as VR with `SteamClient.Apps.SetShortcutIsVR(appid, true)`.
- Hidden desktop mirror: `-window -vr-hide-mirror`, `+vr_mirror 0`.
- `SDL_VIDEODRIVER=x11`; runtime JSON `/opt/steamvr/steamxr_linuxarm64.json`.
- Baseline graphics preset 0, render scale 0.45, no MSAA, `-nobindless`.
- Steam dashboard closed before testing the immersive scene.

The combined configuration was verified. The tests do **not** isolate which
Steam integration change alone fixed the earlier flat/theater presentation.
Do not globally disable other games' desktop theater or bypass the playspace.

## What changed in the engine

The Windows release uses an OpenGL WGL graphics binding. This port adds a Linux
`XrGraphicsBindingOpenGLXlibKHR`, using the actual SDL GLX context, drawable,
visual and FBConfig. CMake links the native OpenXR loader, OpenGL GLX and X11.
Windows retains its own binding.

ARM64 builds use vendored SIMDe 0.8.2 for existing SSE/AVX kernels. AO/audio
disable FMA contraction to preserve scalar operation order. Other portability
changes cover the profiling clock, a standard-library include and duplicate
C++ allocation operators. `XR_EXT_user_presence` pauses input/gameplay and
calibration when the headset is unworn. First-run calibration stays pending
until it actually finishes.

Frame controller bindings use the advertised
`/interaction_profiles/valve/frame_controller_valve` profile. The real runtime
selected it for both hands; real pose actions, grip and sticks reached the game.

## Build on Frame

Use the native ARM64 toolchain and SDL2, X11, GL/GLX, OpenXR headers and the
engine's audio dependencies. The tested toolchain was Clang 19.1.7 with C++23.
No root filesystem modification was needed on the tested device.

```sh
cmake -S . -B build-frame -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DENABLE_USERDIRS=OFF \
  -DQVR_OPENXR_LIBRARY=/opt/steamvr/bin/linuxarm64/libopenxr_loader.so
cmake --build build-frame -j4
```

The Frame's ordinary OpenXR library search found a dummy static library during
development; explicitly selecting SteamVR's real loader avoided that problem.
Use `ldd build-frame/ironwail` to check runtime dependencies on the target device.

## Separate installation

The port package contains the engine and installation tools. Obtain upstream's
official **v1.0.0 QuakeVR.zip** separately and supply your own Quake `id1` directory
with `PAK0.PAK` and `PAK1.PAK`. No commercial Quake data is distributed.
The tested upstream archive's SHA-256 is
`dab3a2adcd3081512e850b696000b2531e64621878b8565919e230d919619ace`.

```sh
python3 install-frame.py --mod-archive /path/to/QuakeVR.zip \
  --quake-data /path/to/your/Quake/id1
```

The default destination is `~/Games/QuakeVR-Unleashed-native-release/game`.
The installer requires a fresh destination, copies owned PAK files and leaves
the original installation untouched. It validates inputs before installing.
`--check` validates without copying or registering anything. Add `--register`
to add the VR shortcut through the running Steam client when its local DevTools
endpoint is available. Otherwise add `run-native.sh` as a non-Steam game and
mark the shortcut for the VR library in its properties.
The new shortcut is named **Quake VR: Unleashed (Steam Frame)**, so registering
it does not replace the development installation's existing shortcut.

Complete calibration intentionally standing or seated. Putting on the headset
while still settling can initially produce a low automatic height measurement;
the tested body calibration subsequently measured 1.516 m and completed all
seven poses. Use the menu's VR Calibration entry to repeat it if necessary.

## Prove the actual VR state

A VR shortcut checkbox or side-by-side desktop mirror is insufficient evidence.
Use several independent signals:

1. `python3 frame-vr-state.py`: OpenVR's current scene PID, application key and
   compositor scene focus must identify the native engine. This helper starts
   as a **Utility**, never as a Scene, so it does not replace the running game.
2. Start the game with `-qvr-frame-diagnostics`. In `quakevr/qvr_openxr.txt`,
   `FRAME_DIAG` records focus, wearer presence, head pose and view flags.
   `FRAME_HAND` records the selected profiles, actual pose action activity,
   valid hand poses and grip/trigger/stick values delivered to Quake.
3. `FRAME_LAYERS projection=1 stereo_views=2` establishes stereo projection
   submission. A quad is a flat panel; a projection uses the tracked views.
   Verify moving head orientation and actual scene images as well.
4. Capture the **composited headset output**, not the game's desktop mirror:

```sh
ffmpeg -hide_banner -loglevel error -f v4l2 -i /dev/video99 \
  -frames:v 3 -update 1 -y headset.png
/opt/steamvr/bin/linuxarm64/vrcmd --stats
/opt/steamvr/bin/linuxarm64/vrcmd --overlays
```

On this Frame, video99 produced usable images of the VR training area and a
handheld weapon filling the headset view. Passthrough appeared during some
calibration samples; later gameplay samples showed the game without it.
`IVRScreenshots_001` wrote files but produced a dark image in an awake sample,
so it was not accepted as visual proof. Standby captures are inconclusive too.

OpenVR device poses alone do not prove delivery to a particular game: compare
them with that game's OpenXR action states. The one-second input log can miss
brief taps. It is diagnostic evidence, not a comprehensive button test.

If the dashboard is still visible over a focused Quake scene, check
`IsDashboardVisible` and close the Steam menu. During development a single
`vrcmd --hidedashboard` changed the verified state from true to false. Do not
use a repeated force-focus loop or change global safety settings.

## Verified results and remaining work

Verified on hardware: native Steam launch, stereo projection, moving head
orientation/position, both hands' valid tracking, grip and stick delivery,
completed body calibration, transition to the VR training hub, shotgun/nailgun
hits and physical axe attacks against the training dummy. The wearer confirmed
that the game entered VR. Native settings and autosaves were written.

Automated isolated engine tests exited 0, loaded the Unleashed VM (CRC 12927),
matched nine AO models against the scalar reference with zero differences,
spawned a shotgun eligible for force-grab and exercised Box3D worlds in e1m1/e1m2.
Those mock tests do not prove physical reload, throwing or climbing.

Calibration samples ran near 72 fps at 778x778 rendered pixels per eye,
upscaled into 1728x1728 XR images. **These are not representative combat
benchmarks.** The wearer reports slow locations; performance is a known issue.
Manual reload, throwing, climbing, representative campaign combat, higher
quality settings and post-calibration restart need further validation.

Steam Audio ARM64 is not bundled; the normal Quake audio fallback works.
GL_NV_shading_rate_image hardware foveation is unavailable in this GL path.
Optional campaigns/music depend on the player's owned data.

## Performance work after publication

Record the same map, route, settings and headset refresh rate before and after
each change. Separate startup/calibration, CPU physics and GPU rendering costs.
Use runtime frame submissions/missed periods and engine profiling rather than
long averages polluted by hours of standby. Keep the initial preset and scale
fixed for comparisons. Check input, physics and weapons after changing SIMD,
threading, render targets or update cadence. Publish measured gains and losses;
do not present a lower render scale alone as an engine optimization.

The reproducible `vr_bench` capture writes JSON to
`<gamedir>/profile/bench/frame_<name>.json`. Its `cpu_phase_stats_ms` entries
give per-phase average, p50, p95, p99 and maximum from the same captured frames;
`cpu_phase_ms` remains the phase-average map. Nested timings such as `screen`
and `swap` overlap and must not be added. Compare renderer GPU times with the
runtime's submitted-frame/missed-period counters before attributing a hitch to
the engine. The mock benchmark can find engine CPU/GPU costs on Frame hardware,
but it is not evidence of OpenXR compositor pacing or physical controller use.

On the tested Frame, the released engine was also measured with mock tracking
and real stereo rendering (Turnip/Zink OpenGL, 1728x1728 eye images, 72 Hz
fixed-step `combat_48`, 180 warm-up and 720 measured frames, two opposite-order
runs). At the release baseline scale 0.45, GPU eye time averaged 30.138 ms and
frame median was 31.942 ms. A separate quality/performance preview uses scale
0.25 and zero retro edge softness; on the same binary it measured 12.559 ms GPU
eyes and a 13.888 ms median. The p99 remained 20.765 ms, so it does not establish
sustained 72 fps in campaign play. Scale 0.25 renders 432x432 pixels per eye
before upscaling, and the sharper retro edges change the intended look. The
normal launcher and its 0.45 default are unchanged. This is a measured optional
quality profile, not an engine speedup claim; physical headset validation is
still required before distribution. See [Steam Frame performance measurements](STEAM-FRAME-PERFORMANCE.md)
for the comparison controls and limitations.

A precomputed direct-color palette lookup was also tested at the release's 0.45
render scale. The corrected shader preserved both eye images pixel-for-pixel,
but the Frame measured 30.437 ms GPU-eye time versus 29.972 ms for release and
showed a slower frame median. That candidate was reverted. An initial binding
collision removed scene geometry and produced invalid low timings; it was
identified by the image and render-phase checks and was not accepted.

At render scale 0.25, the Frame measured 13.411 ms GPU-eye time with the shipped
retro edge softness and 12.531 ms with softness off. The former keeps softer
retro boundaries at about a 7% GPU cost; both retain the blur from reduced
render resolution, and both exceeded the 13.89 ms frame p99 budget in combat.
The sharper-edge preview now also sets `vr_particle_mult 0.5`: its two-run
combat mock sweep reduced GPU-eye p99 from 18.72 to 16.48 ms and the VR-particle
CPU phase p99 from 2.25 to 0.73 ms, while average live particles fell from about
5,185 to 1,954. A fixed eye-image pair showed fewer particle effects; entity
and physics counts stayed the same. The change is isolated to the preview
profile and remains unvalidated in a wearer OpenXR session.

At the release's 0.45 scale, disabling retro edge softness only for world
materials reduced combat GPU-eye mean by about 2.5% (30.188 to 29.440 ms).
Disabling it for brush materials alone showed no improvement. The world-only
setting changes world-texture edge appearance but preserves softness on models
and weapons; it remains a small optional tradeoff and does not meet the combat
72 Hz budget. The default and performance-preview launcher are unchanged.

An isolated engine candidate lowers the minimum supported render scale from
0.25 to 0.18. At 0.18 with shipped retro edge softness, the Frame measured
9.915 ms mean GPU-eye time; GPU p99 was 13.67 ms, while frame p99 remained
18.49–19.03 ms. This is approximately 311x311 rendered pixels per eye before
upscaling to 1728x1728, with a substantial visible loss of detail. A separate
"Frame Ultra Performance" shortcut and install are prepared for wearer
validation; the shortcut passed a native mock launch with scale 0.18 and shipped
softness enabled, but it has not been launched into OpenXR. The regular game
and .25 preview remain untouched. This result is a low-resolution option, not a
claim of stable 72 Hz or a release-ready improvement.

### Wearer regression checklist for the particle preview

The performance-preview shortcut is a separate installation and starts at
render scale 0.25 with `vr_particle_mult 0.5`. Before comparing density, verify
those values in the in-game console, then keep the same map, route, preset and
headset refresh rate while comparing `vr_particle_mult 1` and `0.5`. Restore
0.5 before leaving the preview. The regular native release and QuakeQuest are
not part of this comparison.

On each value, check that SteamVR's active scene and compositor focus identify
the native engine; `qvr_openxr.txt` reports stereo projection and valid head and
both-hand pose actions; and the captured headset view is stereoscopic rather
than a desktop panel. Turn and translate the head, move both controllers, and
confirm the hands follow them rather than acting as a mouse pointer. In the
training range, fire the shotgun and nailgun, reload manually, hit the dummy
with the physical axe, then throw and catch a weapon and climb a rung. The
particle setting should change effect density only; report any tracking,
interaction, rendering or gameplay regression separately.

Capture the runtime's performance-criteria counters for both runs and note
missed periods alongside the engine's timing log. The earlier 720-frame mock
benchmark is useful for CPU/GPU attribution but does not replace this in-headset
comparison. Do not describe the preview as meeting 72 Hz unless the runtime
data from representative play supports that claim.

## Credits and source

- Original mod and engine source: [Vittorio Romeo's Quake VR](https://github.com/vittorioromeo/quakevr).
- Native GLX reference: [SteamFrameRaylibQuickstart](https://github.com/Dinsmoor/SteamFrameRaylibQuickstart).
- [SIMDe 0.8.2](https://github.com/simd-everywhere/simde/tree/v0.8.2), MIT license preserved.
- Steam shortcut helper: [saphid/chromium-webxr-steam-frame](https://github.com/saphid/chromium-webxr-steam-frame), BSD-3-Clause license preserved.
- Diagnostic interfaces: [Valve's OpenVR C API](https://github.com/ValveSoftware/openvr/blob/master/headers/openvr_capi.h).

Engine distribution follows the repository's GPLv2 license and preserves third
party notices. Maintainers are welcome to integrate or take over the port in
the upstream repository; performance work remains experimental.
