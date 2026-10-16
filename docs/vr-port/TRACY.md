# Profiling with Tracy

Quake VR vendors the official Tracy **v0.14.1** client, BSD licensed. Use matching v0.14.1 viewer and capture/export
tools from https://github.com/wolfpld/tracy/releases/tag/v0.14.1. The viewer/tools are not bundled with the game.

## Build and connect

In Visual Studio 2022, open `Windows/VisualStudio/ironwail.sln`, select **Release-Tracy | x64** in the
configuration toolbar, and build the solution. This configuration derives its optimization, runtime library,
ThinLTO and PDB settings from Release and enables Tracy automatically. All three projects (ironwail, zancle
and qvr_doom) have matching, isolated Release-Tracy outputs. The engine build also compiles QC.

From the repository root, the helper selects the same configuration:

```powershell
./Misc/quakevr/build_tracy.ps1
```

The equivalent direct MSBuild command is:

```powershell
MSBuild Windows/VisualStudio/ironwail.sln -p:Configuration=Release-Tracy -p:Platform=x64 -m
```

No extra `QvrTracy` property is required. It uses separate
`Windows/VisualStudio/Build-ironwail/bin/x64/Release-Tracy/ironwail.exe` and `Release-Tracy` intermediate folders.
Normal Release/Debug outputs are preserved. Those configurations compile Tracy out by default: no client, listener,
queries, Box3D instrumentation or wrapper state. Normal builds keep the existing in-game profiler. Disabled C argument hooks are non-evaluating macros,
so QC string lookups, resource expressions and conversions disappear even without optimization; resource walks
and capture metadata are also guarded by the profiling build/connection. An -O0 side-effect fixture passed.

Launch the profiling executable with the same game arguments as the normal one (and from the same working directory).
Open Tracy v0.14.1 and connect to **127.0.0.1:8086**. The client is on demand and localhost only. By default, system tracing,
statistical sampling and callstacks are disabled; administrator privileges are unnecessary. Tracy's crash handler
remains disabled in every configuration, so the game's crash reporting stays in charge. Only one game can use the default port: close other profiling builds first.

Capture is independent of `vr_profile`; leave that at **0** to avoid duplicating expensive detailed profiling.
Existing `vr_profile`, panel, CSV captures and benchmark commands remain functional in either build.

| Profiling-build control | Default | Purpose |
|---|---|---|
| `vr_tracy_status` | command | Connection, nesting depth, drops, GPU pending queries and context generation |
| `vr_tracy_gpu` | 1 | Enable GPU zones; 0 keeps CPU tracing and drains pending GPU timestamps |
| `vr_tracy_gpu_detail` | 0 | 0: whole 3D, eyes, shadow maps and window view; 1: every existing GPU scope |
| `vr_tracy_qc_detail` | 0 | QC zones in server/client VMs (see "QC zones"): 0 off (coarse `QC VM` only); 1 every QC function; 2 + builtins except the trivial ones; 3 every builtin too |
| `vr_tracy_memory_detail` | 0 | Live baseline and allocation/free/reset events for Zone, Hunk and Cache |
| `vr_tracy_test_memory` | command | Realloc, forced cache relocation, hunk reset, detail changes and independent byte/count checks |

GPU queries are allocated lazily after the first connection. Collection only reads available results, never waits
per frame. Context/connection initialization synchronizes a timestamp once; exclude that warmup from measurements.
The existing always-on phase timers remain enabled for in-game stats. Default Tracy GPU coverage is coarse to avoid
duplicating all their queries. Detailed GPU zones and many tiny CPU zones can change performance: measure the
profiling overhead with the workload and settings you are investigating.

### QC zones (`vr_tracy_qc_detail`)

Every QC function and builtin of a VM gets one persistent source location when its progs load (server and client
VMs, rebuilt at every load), interned by name: map changes reuse the same rows. The zone's name is the function's;
its function field says `QC function` or `QC builtin` (the capture checker counts QC zones by it); its file is the
progs' source file when the compiler kept it. Builtins whose names fteqcc stripped take the engine's table's name
(else `builtin #N`). A call costs a table lookup and Tracy's plain zone begin/end: no allocation, no string copy, no
zone text, and no callstack in any build (it would be the interpreter's loop; a QC zone's callers are its parents).

| Level | Zones | Use it for |
|---|---|---|
| 0 | coarse `QC VM` entries only | timing anything that is not QuakeC |
| 1 | + every QC function call | timing with QuakeC named: cheap enough to leave on during captures |
| 2 | + builtins except the trivial list | which builtins (traces, finds, sounds, movement) cost QuakeC its time |
| 3 | + the trivial builtins too (the old coverage) | call counts of everything; the trivial zones inflate QuakeC's times |

Trivial builtins (vector and scalar math, conversions, cached lookups: `qcTrivialBuiltins` in `vr_tracy.cpp`) are
called often and finish in well under a microsecond; a zone costs about what they measure. They were picked from
level-3 captures of `combat_48`, `play_e1m1_fight` and `play_ad_tfuma_fight` by calls a frame x mean time: `cvar_hget`
(about 100 calls a frame, 16 ns), `max`/`min`/`vlen`/`fabs`/`normalize` (14-24 ns), `makevectors` (69 ns), `isragdoll`,
`nextent`, `anglemod`, `random`, `rint`, `vectoyaw`, `sin`/`cos`, `ceil`, `makeforward`, `ChangeYaw`, plus their
unmeasured siblings. `cvar` (163 ns, a string lookup) stays at level 2. Edit the list there and its comment.

Overhead, 2026-10-16: Release-Tracy at callstack depth 8, his config, 2048 eyes, 90 Hz paced, exclusive, connected
`tracy-capture`, GPU and memory detail 0, 900-frame window, 2 reps. CPU busy p50 / p95 ms (difference from level 0's
p50); "old" is the previous implementation (an allocated, copied source location, a zone text and, in callstack builds,
a stack walk per call). QC VM is the capture's `QC VM` zone time a frame (load, warm-up and window), the QC zones'
own cost showing directly; whole-frame numbers move by +-0.3 ms between runs of the fights on their own.

| scenario | level 0 | level 1 | level 2 | level 3 | old |
|---|---|---|---|---|---|
| `combat_48` CPU | 4.45 / 6.17 | 4.85 / 7.38 (+0.40 *) | 4.30 / 6.05 (-0.15) | 4.25 / 5.78 (-0.20) | 4.91 / 6.84 (+0.47) |
| `play_e1m1_fight` CPU | 2.45 / 3.11 | 2.46 / 3.17 (+0.01) | 2.60 / 3.31 (+0.15) | 2.53 / 3.29 (+0.08) | 3.25 / 4.21 (+0.80) |
| `play_ad_tfuma_fight` CPU | 5.57 / 7.73 | 5.84 / 8.35 (+0.27) | 5.86 / 8.55 (+0.29) | 5.68 / 8.29 (+0.11) | 6.41 / 12.46 (+0.84) |
| `combat_48` QC VM ms, QC zones a frame | 0.540, 0 | 0.641, 1034 | 0.574, 1239 | 0.585, 1684 | 1.215, 1684 |
| `play_e1m1_fight` QC VM, zones | 0.413, 0 | 0.445, 968 | 0.454, 1147 | 0.461, 1549 | 1.027, 1549 |
| `play_ad_tfuma_fight` QC VM, zones | 1.064, 0 | 1.144, 1527 | 1.155, 1961 | 1.175, 2515 | 2.083, 2518 |

(*) one rep 4.32, the other 5.37: the fight's spread. A QC zone now costs about 25-45 ns (old: about 400 ns), so the
levels add 0.03-0.11 ms of QC time a frame against the old 0.6-1.0 ms (and the old AD p95 +4.7 ms). Level 3 records
exactly the old zones (same counts). Guidance: capture with **1** whenever QuakeC might matter, also for timing; **2**
to see which builtins cost; **3** only for complete call counts, not for timing QuakeC's leaves.

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
qvr::tracyprof::Scope scope{"load texture"}; // worker-safe, copied name, caller file/line
VR_TracyText(resourceName);                  // copied dynamic annotation of this zone
```

The C equivalent with return cleanup is `VR_TRACY_SCOPE("load texture");` (one per lexical block); Clang's
cleanup attribute handles ordinary returns, and the host recovery boundary handles longjmp. Legacy explicit
`VR_TracyBegin("load texture", 0); ...; VR_TracyEnd();` still requires balanced scopes and literal/process-lifetime names. Dynamic annotations are copied and can be transient. The legacy in-game
profiler remains main-thread-only: use the Tracy-only wrapper, not `QVR_PROFILE`, in worker jobs.
Existing bridged scopes point at the bridge source in Tracy; new C scopes and C++ `Scope` use their caller file/line.
Native Box3D zones keep their original file/line. QC zones use persistent per-function source locations built when
progs load (see "QC zones" below); optimized QC may omit source filenames, and QC zones carry no line numbers.

CPU stacks are per-thread and bounded at 256, with a bounded process-lifetime source-location cache (1024 names).
Overflow suppresses entire zones while preserving nesting; `vr_tracy_status` reports drops. Shared source locations
survive worker exit. GPU timestamps use a 4096-entry ring and reserve every open zone's end before accepting another
zone; a full ring drops whole zones rather than blocking. Connection IDs suppress cross-session ends. Main-thread
non-local Host_Error/Host_EndGame exits explicitly close outstanding bridge zones at the setjmp recovery boundary.
Before GL destruction, queries are deleted with the context current; the next connection/frame recreates them.
Tracy's 8-bit GPU context identifier is never wrapped: after 254 reconnect/recreation generations, restart the game
to restore GPU tracing (CPU tracing still works).

Stage 2 adds the following targeted coverage, independent of `vr_profile`:

| Area | Captured data and scope |
|---|---|
| QuakeC | Coarse `QC VM` with server/client annotation; detail (levels 1-3, see "QC zones") adds per-function and builtin zones, including integrated AD and simple CSQC. Nested entry/reentry and Host_Error use the same bounded recovery stack. No automatic instruction/statement tracing. |
| Jobs | Enqueue, actual execute (including a claiming main/waiting thread), wait-or-claim, blocking atomic wait, parallel dispatch/join and chunks. Workers retain names; source locations identify new call sites. Queue entries, executed/claimed tasks and parallel-loop counters. |
| Loading | File open/load, model preparation, texture preparation and GL upload, image decode/decompression, map load and save/load. Resource paths use copied annotations where available; vendor decoder inner loops are not individually instrumented. |
| Simulation/CPU | Server physics, network receive and audio update, plus all existing gameplay, ragdoll, rendering, OpenXR and native Box3D CPU regions. Wait regions are explicit CPU zones; there is no automatic lock ownership graph. |
| Frame plots | Draw calls over all views, alias triangles, world/sky polygons, classic active particles. These are engine counters, not an automatic GPU hardware counter set. |
| Periodic plots | Server edicts, visible client entities, published ragdolls/bodies, named pool bytes, registered VR scratch/cache totals and summed capacity by system; sampled once per 60 connected frames. Texture bytes are an RGBA estimate including mip/cubemap/depth multipliers, not measured VRAM or all framebuffer/buffer allocations. |
| VR timing | OpenXR predicted display period and estimated skipped display periods from predicted-time deltas in the current reporting window. They are not runtime-reported dropped/compositor frames. Mock starts reset unknown values to 0 ms and -1 respectively; otherwise plots retain the last reported runtime values. |
| Memory | Detail-controlled exact physical Zone/Hunk/Cache block events; native Box3D/default events remain available in coarse capture, with a worker-safe live mirror for late attach/reconnect. General heap allocation tracing, total process RSS and allocator internals are outside this coverage. |

Pool detail starts with a live baseline and handles frees, zone realloc, cache relocation and hunk low-mark resets.
Turning detail off closes its recorded baseline using MemoryDiscard; re-enabling snapshots current live blocks.
Only pools already announced in the current session are discarded: the v0.14.1 server does not advance its serial
clock for an unknown discard pool. Fresh/reconnected sessions start with allocations. Connection/detail snapshots are deferred while a hunk header
is temporarily uninitialized during cache relocation; cross-session frees are rejected, and Host_Error clears the
transition guard. The Box3D adapter changes
only its two allocation profiling macros, preserves native requested-byte sizes/default-pool events, and replays
surviving allocations before later native frees. Its independent reference is `b3GetByteCount`.

Named pool bytes include block headers/alignment. Hunk contains the Zone backing allocation, so pool totals are
not additive. Registered VR capacities use the existing `vr_memstats` accounting, including nested held capacity;
they are resource accounting rather than a complete malloc trace. Live blocks at a capture boundary are surviving
engine allocations, not automatically leaks. Heavy walks run at session/detail transitions or periodic samples.

`Capture context` annotates map, active campaign/native-launch flag, game directory, renderer, backend/runtime,
eye/window resolution and the existing benchmark settings catalogue, plus all PBR/parallax controls.
`native_launch` is the existing launch-detection API for `-game quakevr`, not an assertion that external campaign progs were loaded. QC/memory/GPU
detail flags are included. Take a new capture/context after changing graphics settings within the same map.
Tracy shows our CPU time in OpenXR and our GPU commands; compositor internals, Virtual Desktop encoding/transport,
and native headset runtime drop statistics remain outside the trace.

### Optional callstacks and Windows system tracing

```powershell
./Misc/quakevr/build_tracy.ps1 -CallstackDepth 8
./Misc/quakevr/build_tracy.ps1 -SystemTracing
./Misc/quakevr/build_tracy.ps1 # restore the default no-admin build before measurements
```

`-CallstackDepth` accepts 0–32 (default 0); it records stacks at instrumented CPU zones, not every function call.
QC zones never record one (it would be the interpreter's own loop): a QC zone's callers are its parent zones.
Use the matching optimized executable/PDB for useful symbols. The engine pool adapters emit allocation addresses/sizes/lifetimes, not allocation callstacks. Baselines cannot
reconstruct original allocation callsites from before attachment. `-SystemTracing` enables the official Windows ETW scheduler/context
switch and statistical sampling paths; Tracy requires administrator privileges for its kernel session. It was
compile-tested only; unattended tests never launched an elevated tracing session. Explicit CPU zones/wait markers,
optional zone callstacks, statistical samples and OS context switches are different datasets. The default build
has the first dataset, with zero statistical samples and zero scheduler context-switch events.

Equivalent MSBuild properties are `QvrTracyCallstackDepth=8` and `QvrTracySystemTracing=true`, alongside
the `Release-Tracy` configuration. The legacy `Configuration=Release` plus `QvrTracy=true` build remains supported. All variants use the separate Release-Tracy output; normal outputs remain separate. Debug > Reports
contains Tracy status/detail/self-test controls in profiling builds and a build-information row in normal builds.

### Headless memory/nesting validation

`Misc/quakevr/tracy_capture_check` builds a test-only reader against pre-staged official v0.14.1 source, Capstone
6.0.0-Alpha10, zstd v1.5.7 and PPQSort v1.0.6. Set `TRACY_SOURCE`, `CAPSTONE_SOURCE`, `ZSTD_SOURCE` and
`PPQSORT_SOURCE` to existing directories in CMake; it never downloads dependencies. Apply Tracy's official
`cmake/ppqsort-semaphore.patch` to the private PPQSort source (Release also disables its debug instrumentation).
Run `qvr_tracy_capture_check capture.tracy --stage2` for required CPU/QC/jobs/loading/GPU/pool coverage, nesting,
allocation lifetime and decoded byte/checkpoint reconciliation. Omit `--stage2` for a coarse/partial capture.
The official `tracy-csvexport -u -p capture.tracy` additionally exports plot samples.

Independent live allocator walks validate Zone/Hunk/Cache counts/bytes, and Box3D validates against its native byte
counter. The reader reconstructs actual allocation/free/discard events and matches those bytes at each captured
checkpoint. No IGNORE_MEMORY_FAULTS option is used. Raw test captures stay private in scratch.

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


## Validation of stage 2 (2026-10-14)

Normal/default profiling optimized builds and kit QC/FGD/statics checks passed. Normal e1m1 retained the original
CPU/GPU profiler report; both build variants report `menu paths: 5 found, 0 missing`. The no-viewer profiling state
has zero drops, GPU context/pending queries 0. The legacy melee runner returned 0 with 111 hits in 111 takes (6 statistical spread flags; no melee tuning).

Matching official tools plus the native reader decoded these real captures (all have zero nesting/invalid allocation
errors and zero truncated zones):

| Workload | CPU events | GPU events | Named QC events/functions | Exact decoded checkpoints |
|---|---:|---:|---:|---|
| e1m1/e1m2, save/load, cache move/realloc/hunk reset, detail changes and Host_Error | 1,114,502 | 4,512 | 565,761 / 782 | 11 each Zone/Hunk/Cache, 1 native default |
| Hipnotic hip1m1 + native AD campaign ad_crucial (this build's integrated AD QC) | 1,464,919 | 1,713 | 802,931 / 1,301 | 21 each Zone/Hunk/Cache, 2 native default |
| Simple-CSQC Init/Shutdown, nested leaf/builtin and deliberate unsupported-client-builtin error/reload | 545,522 | 2,097 | 269,045 / 683 | 1 each named pool |
| Late connection with 32 existing bodies; first capture | 159,711 | 1,377 | detail off | 1 native default; named checkpoints also match |
| Same bodies survive disconnect/reconnect, +64 then +128, remove 224, world reload, GPU recreation | 733,505 | 4,055 | detail off | 3 native default; named checkpoints also match |

Survivor default-pool event counts were 868 alloc/130 frees and 4,553 alloc/3,427 frees in the two captures. Every
recorded checkpoint matched both the independent engine state and decoded events. Contexts advanced 1→3 with
zero drops. The CSQC fixture validates the supported Init/Shutdown path; this VR workload did not render its custom
DrawHud callback. AD uses `vr_campaign_native ad` with Quake VR progs highest; it is not unmodified external AD progs.

The optimized callstack build decoded 178,919 stacked CPU zones and 1,506 stack payloads; statistical samples and
context switches stayed zero. ETW was compile-tested only. Earlier scratch captures before the pool-announcement
fix have bad memory timestamps and are superseded by the clock/survivor/campaign captures above.


### Optimized stage 2 overhead

RTX 4090, three sequential exclusive repetitions of each state: e1m1 spawn, `notarget`, seed 7, 1024×1024 mock eyes, 4× MSAA,
`vr_profile 0`, `host_maxfps 0`, fixed game frames at 90 Hz, mock fast mode, 300 warmup and 1200 recorded frames.
These runs are uncapped throughput measurements of this scene, not a headset frame-budget/compositor measurement.
No callstacks or ETW. Detailed capture enables QC, named pool and detailed GPU events together. Native Box3D/default
allocation instrumentation and its survivor mirror are included in all profiling variants. No compile ran during
measurement. Every run recorded 1200 frames with no >33 ms hitches and zero Tracy drops. Raw per-run values:
[TRACY_STAGE2_OVERHEAD.csv](TRACY_STAGE2_OVERHEAD.csv); full JSONs/scripts/captures stay private in scratch/profile.

| State | Mean CPU busy ms | CPU range ms | Mean frame ms | Mean GPU eyes ms |
|---|---:|---:|---:|---:|
| Normal Release | 1.0758 | 1.0266–1.1536 | 1.1031 | 0.7070 |
| Profiling, no viewer | 1.0288 | 1.0077–1.0519 | 1.0545 | 0.6748 |
| Connected coarse | 1.1434 | 1.1305–1.1512 | 1.1735 | 0.7053 |
| Connected QC + pool + GPU detail | 1.5053 | 1.4632–1.5600 | 1.5416 | 0.7693 |

Mean CPU differences versus normal were -0.0470 ms idle,
+0.0676 ms coarse capture and
+0.4295 ms detailed capture. Idle being faster and the
normal run range show run-to-run variability; do not call these universal instrumentation penalties. Detail has
a clear cost here; repeat with the actual workload/headset when deciding which detail modes to enable. The final
campaign annotation, non-evaluating compile-out macros and allocator transition guard were added afterward; the
annotation/hunk work runs before warmup, and disabled Release hooks already optimized away, so recorded steady frame paths are unchanged.


## Ragdoll callstack experiment (2026-10-14)

An exclusive optimized Release-Tracy depth-8 e1m1 capture at 250 FPS, QC/memory detail off and coarse GPU,
recorded 5,607,014 completed stacked CPU zones, 1,096 stack payloads and 16,188 decoded source frames.
The matching reader found zero nesting errors, unresolved frames, truncation, samples or context switches.
Eight grunts were spawned/killed with seed 7, then observed while driven, settled and awakened by a blast;
a separate mock capture proved a held shin. These are explicit zone timings, not sampled functions or scheduler data.

Narrow regions now isolate ragdoll creation/bone hulls/active updates/publication and body publication/after callbacks.
Benchmark begin annotates existing vr_bench_begin names for exact phase boundaries. No gameplay/cvar defaults changed.
Each death took 0.148-0.286 ms, including 0.082-0.132 ms for 11 bone hulls. The 35.496 ms sync hitch occurred
0.653 s before the first ragdoll creation and reproduced without callstacks (36.810 ms): bone hulls are not its cause.
Decoded stack 527 follows syncEntities -> createBody -> addPropShapes -> gunPieces -> cutGun -> convex::decompose -> jobs::parallelFor.
Six nested dispatches are annotated convex cuts; 30.414 ms remains exclusive to the coarse sync zone, so this proves
gun decomposition executes there without assigning all unscoped time to its internals. Next targeted scopes would bracket
whole decomposition/voxelisation/hull work and record cache hit/key reasons; preserve the existing concave collision shapes.

| Phase | State | Box3D mean / p99 ms per physics step (depth 8) | Mean CPU busy ms, depth 8 / depth 0 |
|---|---|---:|---:|
| Active | 8 ragdolls, 3 driven initially; 117.2 world bodies awake on average | 0.737 / 0.904 | 0.7424 / 0.5579 |
| Settled | all 88 ragdoll parts asleep, 16 other world bodies awake | 0.134 / 0.156 | 0.5686 / 0.4672 |
| Blast/contact | 95.4 world bodies awake on average | 0.659 / 0.754 | 0.7251 / 0.5340 |

The 600-render-frame active benchmark contains 171 physics steps; distinguish per-render-frame and per-step costs.
Native Solve Constraints dominates active/contact physics; pose publication and after-step callbacks remain small.
One held corpse raises Box3D mean/p99 from 0.0657/0.0831 to 0.1325/0.1899 ms per step.
Depth-8 CPU-busy overhead versus the identical connected depth-0 scenario is approximately 7% empty, 18% creation,
33% active, 22% settled and 36% contact. This is one sequential pair, not a universal overhead or tail-regression estimate.
Normal Release/QC/FGD/statics/precedence checks and e1m1 smoke passed; both optimized builds and three captures passed.
Local evidence in retained adpbrload/scratch: RAGDOLL_PROFILE.md, rag_stack_flat.tracy, rag_stack_mock.tracy,
rag_nostack_flat.tracy, analysis JSON/stack exports, and reusable rag_capture.ps1/rag_analyze.py/rag_reader.cpp.
