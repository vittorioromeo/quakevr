# Quake VR CPU/GPU benchmark — 5 October 2026

Follow-up: [VTune and retro/prop/decal audit](PERFORMANCE_AUDIT_20261005.md) refines the CPU priorities and tests 4096 large marks; mixed rigid props primarily expose classification and interaction queries, rather than skinned-bone uploads.

The largest measured optimization opportunity is **particle pixel cost**, especially the interaction between retro shading and the half-resolution particle path. Dense particle stress takes **13.299 ms GPU at 2048 per eye**, or **30.795 ms at 3072**. Active props are CPU-bound; crowded combat additionally exposes decal-grid rebuilds and model/shadow submissions. The new incandescent explosion debris is a smaller cost than dense smoke overdraw.

Measured source: `vr-ironwail` at `6ca0664a`. No engine/gameplay optimization was applied by this investigation. The benchmark scripts, compact results and this report are the deliverables.

## Method and evidence

- **123 valid separate-process runs**, **147,210 measured host frames**, **10,756 GPU-timed frames**, across **35 scenario/control variants**. Pilot and invalid fixture runs are excluded and retained separately.
- Intel i9-13900K (24 cores/32 logical processors), NVIDIA RTX 4090 (24 GB), driver 610.88, Release engine and matching QuakeC. Binary hashes are in [the manifest](benchmarks/20261005_manifest.json).
- Most results are the **median of three run means**, with a fixed 72 Hz simulation clock and 1200 requested measurement frames. The repeating prop disturbances use 1170 frames (whole 90-frame cycles). Higher-resolution comparisons use two repeats. The every-frame GPU-timer diagnostic and ragdoll-count audit use one run each.
- Stereo mock rendering actually draws both eyes at 2048×2048, with additional 3072×3072 runs. Unpaced rendering (`vr_mock_fast 1`), no audio, no desktop mirror, no VSync, scale 1, current user graphics settings copied into a disposable base, including MSAA 0, aggressive foveation, retro textures, eight dynamic shadow slots and four map-light slots. GPU scopes use OpenGL timestamps every 16th frame; CPU scopes collect every frame.
- Warm-up/setup are dumped as interval 1 and excluded; interval 2 is measured. Simulation samples, actual emitter counts, portal rendering/acquisition and debris spawning are checked by the analyzer. Screenshots confirm that the intended scenes render. A dedicated audit confirms **32 ragdolls**, 602 total Box3D bodies, with most ragdoll parts asleep at the end.
- [Compact results](benchmarks/20261005_summary.csv); raw captures, exact per-run autoexecs, call trees, console logs, screenshots and analysis JSON: `C:/OHWorkspace/quakevr-iw/build-cmake/perf-20261005/results/`. The raw files remain outside Git. Original player configuration is byte-for-byte unchanged.

**CPU wall time is not pure CPU compute.** It includes uncategorized OpenGL/driver waits. The dense-particle case spends about 12.536 ms in `screen` self time while particle simulation costs only 0.053 ms and instance construction 0.071 ms. Together with the GPU result and resolution scaling, this points to GPU backpressure, not expensive particle simulation. CPU and GPU work overlap; do not add their times. GPU scope self times exclude child scopes; the analyzer avoids summing inclusive parent and child costs.

## Primary measurements

Milliseconds per host frame, stereo, 2048 per eye:

| Scenario | CPU wall ms | GPU ms |
|---|---:|---:|
| Range baseline (map already has props/boards) | 0.841 | 0.868 |
| Slipgate doorway, feature off | 0.906 | 0.934 |
| Slipgate doorway, feature on | 1.726 | 1.772 |
| 24 enemies across a slipgate, AI on | 2.422 | 2.377 |
| 500 mixed props, repeatedly disturbed | 2.511 | 1.224 |
| 500 mixed props, settled | 1.646 | 1.278 |
| 48 mixed enemies in combat | 2.731 | 2.708 |
| 32 ragdolls, repeatedly disturbed | 1.573 | 1.339 |
| 32 ragdolls, settled | 1.240 | 1.235 |
| 1024 retained bullet decals | 1.081 | 1.090 |
| 1024 decals with continuing new shots | 1.066 | 1.081 |
| 32 added overlapping dynamic lights | 1.561 | 1.588 |
| 32 added wall torches (35 emitters total) | 1.108 | 1.138 |
| 3 explosions/s, incandescent debris enabled | 1.340 | 1.134 |
| 18 explosions/s, debris enabled | 2.164 | 2.137 |
| Dense nearby smoke/blood/big-smoke emission | 13.459 | 13.299 |
| 300 props + 24 enemies + 16 lights + dense particles | 15.237 | 14.913 |

The range baseline is a populated level, not an empty rendering test. Props and corpse scenes also include the map's existing bodies and resulting debris. The combat scenes evolve (including friendly fire, deaths and gore), so run-to-run differences are reported rather than treated as bit-identical combat replays. The dense particle fixture emits 80 smoke/blood/big-smoke particles every four host frames, 64 units in front of the view. It is a deliberately severe overdraw case, not a claim about an ordinary torch or a single explosion.

## Measured controls and optimization priorities

### 1. Preserve the retro look while reducing particle fragment work

The same dense fixture costs **13.299 ms GPU** with current retro particles, **9.002 ms** with only particle retro shading disabled but full resolution retained, and **3.026 ms** with particle retro disabled and the existing adaptive half-resolution path enabled. These are measured configuration comparisons, not claimed gains from an implemented optimization. At 3072 per eye the corresponding current/half-resolution figures are **30.795/6.185 ms**.

The cause is visible in [vr_particles.cpp](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_particles.cpp:2481): `retroSet == 0` is required for the half-resolution path, so `vr_particle_halfres 1` does not help retro particles. Simulation and instance upload are already shared across views; duplicating that optimization would miss the bottleneck.

**Highest-priority implementation:** make large soft smoke/fire eligible for a reduced-resolution path while retaining desired retro quantization, or offer a deliberate mixed quality mode with crisp small sparks and reduced-resolution large translucent particles. Recheck blend order, depth edges, temporal stability and the pixelated appearance in VR. Also investigate the full-resolution retro particle shader: its cost remains substantial even before resolution changes. The shader/style comparison saves roughly 4.3 ms in this synthetic case.

### 2. Reduce repeated CPU model work and sleeping-object bookkeeping

500 active mixed props cost **2.511 ms CPU versus 1.224 ms GPU**. Server physics averages **1.144 ms**, but Box3D's step itself is only about **0.269 ms**. Alias-model submission is about **0.601 ms** across the views; Box3D write-back about **0.245 ms**, QuakeC about **0.337 ms**, and rigid-body bookkeeping about **0.124 ms**. With props settled, alias submission stays near **0.604 ms**, while the step becomes much cheaper.

Audit always-on entity/interaction queries, dirty-state propagation, render batching and pose/bone upload reuse for sleeping objects and repeated eye/shadow views. Existing batching and some sleep fast paths already exist; extend those selectively rather than adding a second physics system. [r_alias.c](C:/OHWorkspace/quakevr-iw/Quake/r_alias.c:402) uploads custom bone matrices at alias-batch flush; investigate caching GPU bone-buffer ranges per entity/frame across eyes and shadow faces. Use smaller named measurements before changing this path; the aggregate `alias` scope is not a measurement of skinning alone.

Worker control validates the current approach: automatic workers reduce step p95 from **0.945 to 0.598 ms**, but whole-frame means are essentially unchanged (**2.504 vs 2.511 ms**). More solver parallelism is not the first optimization to pursue here.

### 3. Make streaming world-decal updates incremental

With 48 fighting enemies, decals on/off compare at **2.731/2.122 ms CPU** and **2.708/2.138 ms GPU**. This includes changed visual work and some evolving combat differences, so the entire difference cannot be assigned to one function. The named `decals on the world` scope nevertheless accounts for about **0.317 ms CPU** in the normal combat capture.

[buildWorld](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_decals.cpp:1360) rebuilds all world decal records and the complete spatial hash, then uploads both buffers when marks change. Target incremental insert/evict updates, dirty buckets and partial GPU uploads while retaining newest-mark ordering and fade behavior. Do not replace the existing static triangle-buffer caching: that is a separate path already present.

1024 retained bullet marks cost **1.090 vs 0.909 ms GPU** with decals off. These marks shade through the world shader, so a nearly empty standalone `decals` draw scope does not mean world decals are free. Continuing bullet-mark insertion compares at **1.081/0.845 ms GPU** and **1.066/0.832 ms CPU**. Large blood marks and combat churn are more expensive than this bullet-chip fixture.

### 4. Bound shadow work in crowded, brightly lit scenes

32 added overlapping lights compare at **1.588 ms GPU** with current shadow caps versus **1.146 ms** with dynamic and map shadows disabled; CPU compares at **1.561/1.123 ms**. In combined stress, dynamic-light shadows alone contribute about **2.085 ms GPU**, with **1.451 ms CPU** in shadow alias submissions. The combined capture averages approximately 8443 draw calls and 5995 alias-model submissions per frame; those counters include repeated shadow/view submissions, not that many unique objects.

[Dynamic caster collection](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_lighting.cpp:1304) collects world, brush and alias casters per light, followed by cube-face rendering. Prioritize caster/bone reuse, invalidation-aware shadow caching for stable lights/objects, per-light update budgets and shadow resolution based on projected importance. Existing shadow selection/caching must remain the starting point. Measured off controls are quality diagnostics, not a recommendation to remove lighting wholesale.

### 5. Keep portal costs proportional to visible aperture and destination load

The doorway's total slipgate feature adds roughly **0.84 ms GPU** and **0.82 ms CPU** over its off control. This includes destination entities, folded lighting and extra rendering, not just compositing the portal texture. The GPU/CPU costs rise to **2.820/2.792 ms** at 3072 per eye. Limiting the view cap to one gives no useful reduction here because this camera sees only one aperture.

The valid 24-enemy fixture logs ordinary visibility/PVS false and reverse gate 8 selected. AI enabled averages **2.422 ms CPU / 2.377 ms GPU**. Its AI-disabled control averages **2.993 / 2.432 ms**, because disabling the route changes movement, collisions and combat; it is not evidence of a negative perception cost. Do not interpret that comparison as an isolated AI microbenchmark. Fine combat scopes put traces and builtins well below the dense-particle cost; no evidence here supports making the new portal perception the first optimization target.

Portal views already rank apertures, use a destination PVS, avoid recursion and mask pixels outside the aperture. Candidate improvements are destination resolution chosen by projected aperture, tighter destination draw lists and reuse of per-frame entity preparation across views. Retain correct eye transforms and the current clipping behavior. These runs do not characterize several simultaneously visible portals or long navigation workloads.

### 6. Debris and torch particles are secondary costs in these fixtures

At three explosions/s, toggling incandescent debris compares at **1.340/1.030 ms CPU** and **1.134/1.038 ms GPU**. The active pool reaches its configured **96 chunks**, with **16 chunks per explosion**, trail emissions, bounces and eight reported debris lights. Increasing large explosion sprites from one to four raises GPU cost to **1.246 ms**. At 18 explosions/s, debris on/off compares at **2.137/1.974 ms GPU**.

The source [debris simulation](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_explosiondebris.cpp:277) traces its small fixed pool in 120 Hz substeps. A resting-chunk/sleep shortcut and trail emission budgets are reasonable follow-ups; add a dedicated debris CPU scope before assigning an exact internal bottleneck. Dense smoke's `particles_no_debris` control is intentionally a null control: that fixture's physics blast helper does not spawn the new explosion effect, so it cannot measure debris.

32 added wall torches produce **35 real emitters**, using the user's eight particles/burst at 10 Hz. Fire particles on/off compare at **1.138/1.054 ms GPU** and **1.108/1.035 ms CPU**. This supports the distinction between particle count and screen coverage: numerous small flame particles are much cheaper than large overlapping smoke quads in this setup.

## Resolution and tail latency

| Scenario | 2048 GPU ms | 3072 GPU ms |
|---|---:|---:|
| Range baseline | 0.868 | 1.326 |
| Slipgate doorway | 1.772 | 2.820 |
| 32 dynamic lights | 1.588 | 2.200 |
| Dense retro particles | 13.299 | 30.795 |
| Dense particles, nonretro + half resolution | 3.026 | 6.185 |

The dense particle cost grows by roughly the 2.25× increase in eye pixels, confirming fill/fragment pressure. The 3072 square mock target is a resolution stress probe, not a particular headset's exact FOV or swapchain dimensions.

Physics distributions below are per simulated server frame, in ms; p95/p99 are medians of the runs' percentiles, not percentiles of all frames pooled together:

| Scenario | Server physics mean | Server p95 | Server p99 | Box3D step p95 |
|---|---:|---:|---:|---:|
| 500 props, automatic workers | 1.144 | 2.405 | 3.111 | 0.598 |
| 500 props, single thread | 1.168 | 2.621 | 3.816 | 0.945 |
| 32 active ragdolls | 0.497 | 1.081 | 1.249 | 0.571 |
| 32 settled ragdolls | 0.251 | 0.301 | 0.690 | 0.026 |
| Combined stress | 2.217 | 3.472 | 4.545 | 1.129 |

Do not confuse those percentiles with whole-frame CPU/GPU p95: the existing scope capture provides means and maxima, not whole-frame percentiles. Raw captures include isolated much longer frames (for example **58.937 ms CPU in combat**, and **134.319 ms in active ragdolls**). Diagnostic hitch runs mostly identify initial ~116–130 ms render/driver work during warm-up; one later 12.81 ms hitch attributes about 9.2 ms to a QuakeC builtin. Ragdoll captures also show ~12 ms sync spikes. Those deserve allocation/asset-creation and driver/residency investigation, but these captures do not establish a repeatable single root cause for every long frame.

GPU-timer perturbation is small at the chosen sampling rate: idle CPU is **0.836 ms without GPU queries** and **0.841 ms with one-in-16 sampling**. The every-frame diagnostic is **0.925 ms** (one run). Portal CPU is **1.730 ms without queries versus 1.726 ms sampled**. These controls support using sparse timing; fine trace/builtin instrumentation is diagnostic and its changing combat runs should not be used to estimate overhead by simple subtraction.

## Limits and reproducibility

The desktop had other GPU applications running and roughly 20–22 GB global VRAM occupied; that is not memory attributed solely to Quake. No unrelated processes were stopped. Short-run drift, driver warm-up and background interference can affect spikes. GPU temperature rose during the sequence; alternate repeat order and the recorded run ranges provide a check against large drift. Stable large differences such as the particle-resolution controls are stronger evidence than tiny deltas.

Audio is disabled. These runs do not measure HRTF/reverb cost, OpenXR pacing, headset transport, compositor load or reprojection. At 90/120 Hz the application budgets are 11.11/8.33 ms before runtime margin; the dense current-style particle and combined cases exceed them on this 4090. Do not infer headset FPS from unpaced mock FPS.

The disposable base is `build-cmake/perf-20261005/base`; its assets are junctions to existing game assets, with copied startup/configuration files and matching `progs.dat`. To rerun into a fresh output directory:

```powershell
python Misc/quakevr/perf_suite.py --base build-cmake/perf-20261005/base --exe Windows/VisualStudio/Build-ironwail/bin/x64/Release/ironwail.exe --output build-cmake/perf-repeat/results --reps 3 --frames 1200 --eye 2048
python Misc/quakevr/perf_analyze.py build-cmake/perf-repeat/results
```

Use `--scenes` for the control names in the summary CSV, `--gpu 0` for CPU timing without GPU queries, and `--eye 3072` for the resolution test. Existing completed run tags are skipped; choose a fresh output directory for a new capture. Every run saves its exact commands, binary hashes and profiler files. No configuration writes are enabled.
