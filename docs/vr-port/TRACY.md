# Profiling with Tracy

Quake VR vendors the official Tracy **v0.14.1** client, BSD licensed. Use matching v0.14.1 viewer and capture/export
tools from https://github.com/wolfpld/tracy/releases/tag/v0.14.1. The viewer/tools are not bundled with the game.

## Build and connect

Build the usual QC first. From the repository root, run:

```powershell
./Misc/quakevr/build_tracy.ps1
```

This builds optimized Release with PDB symbols using MSBuild `-p:QvrTracy=true`. It uses separate
`Windows/VisualStudio/Build-ironwail/bin/x64/Release-Tracy/ironwail.exe` and `Release-Tracy` intermediate folders.
Normal Release/Debug outputs are preserved. Without the property, Tracy is compiled out: no client, listener,
queries, Box3D instrumentation or wrapper state. Normal builds keep the existing in-game profiler.

Launch the profiling executable with the same game arguments as the normal one (and from the same working directory).
Open Tracy v0.14.1 and connect to **127.0.0.1:8086**. The client is on demand and localhost only. System tracing,
sampling/callstacks and Tracy's crash handler are disabled; administrator privileges are unnecessary and the game's
crash reporting remains in charge. Only one game can use the default port: close other profiling builds first.

Capture is independent of `vr_profile`; leave that at **0** to avoid duplicating expensive detailed profiling.
Existing `vr_profile`, panel, CSV captures and benchmark commands remain functional in either build.

| Profiling-build control | Default | Purpose |
|---|---|---|
| `vr_tracy_status` | command | Connection, nesting depth, drops, GPU pending queries and context generation |
| `vr_tracy_gpu` | 1 | Enable GPU zones; 0 keeps CPU tracing and drains pending GPU timestamps |
| `vr_tracy_gpu_detail` | 0 | 0: eyes, shadow maps and window view; 1: every existing GPU scope |

GPU queries are allocated lazily after the first connection. Collection only reads available results, never waits
per frame. Context/connection initialization synchronizes a timestamp once; exclude that warmup from measurements.
The existing always-on phase timers remain enabled for in-game stats. Default Tracy GPU coverage is coarse to avoid
duplicating all their queries. Detailed GPU zones and many tiny CPU zones can change performance: measure the
profiling overhead with the workload and settings you are investigating.

## Command-line capture

Start capture before launching the profiling game, or attach to a running one:

```powershell
tracy-capture.exe -a 127.0.0.1 -o quakevr.tracy -s 30
tracy-csvexport.exe quakevr.tracy > cpu.csv
tracy-csvexport.exe -g quakevr.tracy > gpu.csv
python Misc/quakevr/check_tracy_capture.py cpu.csv gpu.csv --box3d
```

Omit `-s` to record until the game exits. `-f` explicitly permits overwriting your own capture output.
The capture checker verifies engine CPU scopes, both GPU eyes, valid GPU durations and optionally Box3D zones.
For headless kit tests, `run.sh --exclusive <name> -Tracy ...` selects the separate build while preserving run slots.
Use `--exclusive` for timing comparisons. A capture taken during shutdown can contain truncated final zones;
compare completed zones and warm frames. Use the viewer for frame, thread, call-tree and GPU timeline analysis.

## Coverage and integration API

The foundation routes all existing C `VR_ProfileBegin/BeginGPU/End` and C++ `QVR_PROFILE/QVR_GPU_PROFILE` scopes into
Tracy: rendering, named VR/OpenXR phases and existing gameplay/physics scopes. Box3D's native CPU instrumentation
is enabled, including its worker solver zones. Main and job-pool threads are named. The unnamed frame set is the
host frame; Box3D uses a separate **Box3D** frame set, so substeps do not pollute host-frame counts.

`Quake/vr/vr_tracy.h` exposes wrappers that compile to no-ops in normal builds:

```cpp
qvr::tracyprof::Scope scope{"load texture"}; // CPU scope, safe on workers
VR_TracyText(resourceName);                  // copied dynamic annotation of this zone
```

The C equivalent is `VR_TracyBegin("load texture", 0); ...; VR_TracyEnd();`. Use balanced scopes; names must be
literal or process-lifetime strings. Dynamic annotations are copied and can be transient. The legacy in-game
profiler remains main-thread-only: use the Tracy-only wrapper, not `QVR_PROFILE`, in worker jobs.
Existing bridged scopes point at the bridge source in Tracy; native Box3D zones keep their original file/line.

CPU stacks are per-thread and bounded at 256, with a bounded process-lifetime source-location cache (1024 names).
Overflow suppresses entire zones while preserving nesting; `vr_tracy_status` reports drops. Shared source locations
survive worker exit. GPU timestamps use a 4096-entry ring and reserve every open zone's end before accepting another
zone; a full ring drops whole zones rather than blocking. Connection IDs suppress cross-session ends. Main-thread
non-local Host_Error/Host_EndGame exits explicitly close outstanding bridge zones at the setjmp recovery boundary.
Before GL destruction, queries are deleted with the context current; the next connection/frame recreates them.
Tracy's 8-bit GPU context identifier is never wrapped: after 254 reconnect/recreation generations, restart the game
to restore GPU tracing (CPU tracing still works).

The foundation does not yet add per-function QC instrumentation, allocation-pool accounting, queue/wait/lock detail
or every loading/resource operation. Those require targeted instrumentation in the next stage. Tracy shows our
CPU time in OpenXR calls and our GPU commands, not compositor internals, Virtual Desktop encoding or network latency.

## Validation of the foundation

Normal and profiling Release builds passed the kit checks; the normal profiler emitted its CPU/GPU report on e1m1.
A no-viewer profiling smoke exited cleanly with zero drops and no GPU queries/context allocated.

Matching v0.14.1 capture/export tools decoded a real e1m1 + 32 brick props trace with `vr_profile 0`:
1783 host frames; **171 CPU scope names / 443679 events**, **43 GPU names / 128004 events**, and
**39 Box3D scope names / 45307 events**. Two captures from one process both decoded, including an intentional
Host_Error, reload, reconnect and explicit GL query teardown/recreation (context generations 1, 2, 3).
`vr_tracy_test_stack` restored depth 1 to depth 1 after 320 nested begins/ends, overflow 0, and 65 deliberate drops.
`vr_tracy_test_error` intentionally disconnects the map with Host_Error; `vr_tracy_test_gl_reset` exercises query
destruction/recreation. These diagnostics exist only in profiling builds; use them in disposable test sessions.

Raw captures/exports and benchmark JSONs from implementation are private scratch/test outputs, not game assets.
The following measurement section records workload-specific overhead rather than a universal percentage.

### Representative overhead measurement

Three sequential repetitions of each variant, e1m1 at its spawn with `notarget`, 300 warmup frames then 1200 recorded
frames, exclusive kit slots. RTX 4090, 1024x1024 mock eyes, 4x MSAA, same config, `vr_profile 0`, coarse Tracy GPU zones,
mock fast mode. The existing `host_maxfps 250` cap keeps frame period near 4 ms; these runs measure CPU busy/GPU time
and cannot establish an uncapped FPS penalty or real-headset overhead. Raw per-run values: [TRACY_OVERHEAD.csv](TRACY_OVERHEAD.csv).

| Build/capture state | Mean CPU busy (ms) | CPU range across runs (ms) | Mean GPU eyes (ms) |
|---|---:|---:|---:|
| Normal Release, compiled out | 1.0362 | 1.0107–1.0768 | 0.9858 |
| Profiling Release, no viewer | 1.1364 | 1.0821–1.2101 | 0.8484 |
| Profiling Release, connected coarse capture | 1.1015 | 1.0450–1.1815 | 0.7894 |

Mean CPU differences here were +0.1002 ms idle and +0.0653 ms connected versus normal. Connected was lower than idle,
and GPU time varied substantially, indicating run-to-run/system variability: do not attribute those differences
entirely to instrumentation or treat them as universal overhead. All nine runs completed 1200 frames with no >33 ms
hitches; profiling statuses reported zero drops. Repeat with an uncapped, representative workload and real headset
when evaluating a particular optimization; enable detailed GPU/QC scopes only as needed.
