# Repeatable GPU profiling for frame synthesis

This note records which parts of the Steam Frame profiling loop are automated,
which tools were verified on the headset, and what still needs work before a
GPU API capture can be trusted.

## The test driver is already in the game

`Misc/quakevr/bench/qvrbench.py` generates deterministic console scripts from
fixed maps, seeds, view positions, setup commands, and fixed frame windows.
The engine records frame timings, CPU/GPU phase timings, entity and physics
counts, benchmark marks, and optional monster-death events as JSON/CSV. The
smoke runner copies the OpenXR and console logs, checks that the requested
workload happened, and restores the game configs and `progs.dat` afterward.

Useful scenarios include:

- `spacewarp_motion_fixture`: a moving crate in a clean room, to validate fresh
  motion/depth submissions without combat effects.
- `gore_headpop_24`: a deterministic 24-monster kill-effects workload.
- `play_e1m1_fight`: scripted shotgun combat for a more gameplay-like pass.

For a short app-side SpaceWarp check, run
`Misc/quakevr/tests/run_frame_synthesis_smoke.sh` with the intended scenario and
build. For a controlled ON/OFF comparison, use
`Misc/frame/run-spacewarp-motion-abba.sh`. For runtime proof, use
`Misc/frame/run-spacewarp-headset-abba.sh`; it checks valid HMD tracking and
collects SteamVR compositor timings and performance recordings alongside the
game logs.

The engine's timestamp queries are the current repeatable performance signal.
They identify the GPU cost of the eye render, world, shadows, particles,
decals, and SpaceWarp depth, downsample, optical-flow, and encode passes. These
timings can locate the expensive pass, but do not provide hardware-counter
details such as GPU occupancy or memory bandwidth.

## What was verified on Steam Frame (2026-10-14)

The installed Vulkan device reports `Turnip Adreno (TM) 750`, Mesa
`26.3.0-devel`, and the Mesa Turnip driver. The game renders through Zink and
uses the Vulkan bridge for OpenXR images.

The automated `gore_headpop_24` smoke run completed 180 measured frames and
passed its workload validator: 24 monsters died in the scripted blow window
(benchmark frames 9–15), with 25 kill events recorded. Fourteen sampled OpenXR
records also reported fresh stereo synthesis submissions. This was an unworn
app-side run: the Steam Frame had
no valid HMD pose, no connected controllers, and no SteamVR performance CSV.
It does not prove runtime receipt or generated frames.

I enabled Mesa's `MESA_GPU_TRACES=print_json` on that run. Turnip produced a
36.8 MB trace containing both `ironwail`/Zink and `Quake VR Vulkan bridge`
contexts. On this installed Mesa build, `MESA_GPU_TRACEFILE` kept the documented
`%p`/`%i` filename placeholders literally, so both contexts wrote into the same
file and interleaved their output. The result is not valid JSON. Do not use
durations extracted from that file: tracing also made the measured frames much
slower. Mesa documents per-context `%i` filenames as the way to keep multiple
JSON contexts valid; the installed build needs to demonstrate that behavior
before we automate this capture path.

GFXReconstruct is installed, but its current behavior is not yet suitable for
gameplay profiling. The default page-guard memory mode aborted Quake with status
134. Switching to `assisted` mode produced a valid 482 KB capture of submits
0–5 which `gfxrecon-info` could parse and `gfxrecon-replay` could replay in
headless mode. Inspection showed that this was only three Zink startup frames
from the earlier `build-paired` binary; it contained no Quake Vulkan-bridge
gameplay. The replay's `--validate` option could not load
`VK_LAYER_KHRONOS_validation`, so it was not Vulkan validation evidence. The
assisted mode also relies on the application flushing modified mapped memory;
a replay process exiting successfully does not establish complete capture or
correct replayed pixels.

A current-workload capture exited 139 before scene startup and left a truncated
1.53 MB file. Retrying with both the GFXReconstruct Vulkan and OpenXR layers
confirmed loaded still crashed in the layer during Vulkan2 bootstrap; its
766 KB capture was also truncated. LunarG documents desktop OpenXR capture as
experimental and Linux as unsupported. This is a tool/runtime compatibility
failure before the workload, not evidence of a Quake SpaceWarp or gore defect.
Keep GFXReconstruct optional; see the [capture failure report](../../../performance-results/spacewarp-gfxrecon-gore-20261014/RESULTS.md).

RenderDoc was tested on the current 2026-10-14 build as a possible alternative.
Capturing from launch caused the existing GL/Vulkan UUID guard to reject the
bridge. A second test let Quake initialize normally, then attempted
`renderdoccmd inject`; this SteamOS/Linux RenderDoc build explicitly does not
support injecting into an already running process, and a preload trigger could
not resolve RenderDoc's API export. No `.rdc` was produced. The Quake smoke
itself exited 0 after 600 frames and validated 17 fresh stereo submissions;
the crate moved and settled. It observed zero tracked-head samples, so this is
unworn OpenXR workload evidence only, not runtime receipt or generated-frame
evidence. The latest one-second compositor probe likewise found dashboard focus
and 0/65 valid poses. See [RenderDoc attach results](../../../performance-results/spacewarp-renderdoc-attach-20261014/RESULTS.md).
The headset runner's pose gate was exercised in this state and refused with
status 5 before touching game files or SteamVR settings.

Raw captures and logs from these probes are in:

- [`Turnip gore trace results`](../../../performance-results/spacewarp-turnip-gore-trace-20261014/RESULTS.md)
- [`GFXReconstruct probe results`](../../../performance-results/spacewarp-gfxreconstruct-probe-20261014/RESULTS.md)
- [`RenderDoc attach results`](../../../performance-results/spacewarp-renderdoc-attach-20261014/RESULTS.md)
- The later assisted startup and gore captures are in the Frame's
  `performance-results/spacewarp-gfxreconstruct-assisted-20261014/` and
  `performance-results/spacewarp-gfxrecon-gore-20261014/` directories.
- [Collector attribution and CSV correlation test results](../../../performance-results/spacewarp-collector-attribution-20261014/RESULTS.md)

## Tool choices

| Tool | Good use here | Current result |
|---|---|---|
| `qvrbench` + engine GPU timestamp queries | Repeated scenarios and per-pass timings | Verified on Frame; use for routine comparisons |
| SteamVR compositor timing API and performance CSV | Presented, dropped, and compositor frame data | Collector now distinguishes focus loss during Quake from normal Dashboard return; headset test still needs valid tracking |
| Mesa Turnip `u_trace` / Perfetto | GPU render-stage timing and GPU counters | Driver supports the trace hooks, but current JSON file handling was not valid across contexts |
| GFXReconstruct | Capture Vulkan API commands for later replay/debug | Small Zink startup capture replays; current bridge/gameplay capture still fails or is truncated |
| Qualcomm Snapdragon Profiler | Adreno hardware counters and graphics snapshots | Promising for Snapdragon; compatibility with Frame's Mesa Turnip/Zink stack has not been tested |
| Arm Streamline | Arm CPU profiling; GPU counters for supported Mali/Immortalis GPUs | Not the first GPU tool to try on Frame's Adreno GPU |
| Android GPU Inspector | Android Vulkan/OpenGL ES application profiling | Not a direct fit for SteamOS/Linux Frame runs |

## Next capture steps

1. Keep `qvrbench` as the test driver. Add or adjust scenarios when a new
   workload needs isolation; never compare traces from different seeds,
   settings, render scales, or frame windows as if they were the same test.
2. Complete the tracked headset precheck with no Vulkan capture layer. Check
   runtime-reported depth and motion input, both in-headset texture views,
   object motion, actual refresh, and SteamVR compositor timings. App-side
   submission logs alone do not prove runtime receipt or synthesis.
3. Make Turnip trace output context-safe on Frame. First establish whether a
   Mesa update or a supported context-filtered output can write separate
   `ironwail` and bridge traces. Validate JSON parsing and trace overhead on a
   small fixture before capturing the gore workload again.
4. If a single-frame API capture is still needed, reproduce the GFXReconstruct
   failure with the exact current binary, inspect the core, then capture and
   replay a known moving frame. Verify resource/pixel equivalence; a successful
   process exit alone is insufficient.
5. Test Snapdragon Profiler against the actual Frame driver only if its agent
   can attach to Mesa Turnip. The product supports Snapdragon targets and
   graphics snapshots in general, but that alone does not establish support
   for this SteamOS driver stack.

## References

- [Steam Frame Steamworks documentation](https://partner.steamgames.com/doc/steamhardware/steamframe)
- [Mesa u_trace GPU tracing](https://docs.mesa3d.org/u_trace.html)
- [Mesa Perfetto tracing](https://docs.mesa3d.org/perfetto.html)
- [LunarG GFXReconstruct Vulkan capture](https://github.com/LunarG/gfxreconstruct/blob/dev/USAGE_desktop_Vulkan.md)
- [Qualcomm Snapdragon Profiler](https://www.qualcomm.com/developer/software/snapdragon-profiler)
- [Arm Streamline Performance Analyzer](https://developer.arm.com/tools-and-software/streamline-performance-analyzer)
- [SteamVR compositor frame timing](https://github.com/ValveSoftware/openvr/wiki/Compositor_FrameTiming)
- [Steam Frame performance recordings and depth/motion inspection](https://partner.steamgames.com/doc/steamhardware/steamframe/debugging)
