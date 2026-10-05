# Prop metadata and query optimization

Implemented on `vr-ironwail` after the [decal optimization](DECAL_OPTIMIZATION_20261005.md). The repeated mixed-prop benchmarks reduce CPU frame time by **10–15% with 500 added props** and **14–20% with 1,000 added props**. These are stereo mock-renderer measurements, not headset frame-rate guarantees.

## Changes

- **Retro model metadata:** cache filename-based categories and immutable body bone-part masks per model. Held/holstered ownership, small-gib scale, settings, and overrides remain live. `entitySet` classifies an entity once rather than twice.
- **Shared model bounds:** cache the last exact drawn transform and resulting local bounds per model. Keys include model/header bounds and scale, resolved weapon transform, prop size, and network scale, scale origin, and offset. The original eight-corner calculation remains the reference and miss path.
- **Entity bounds and axes:** retain each entity's local drawn bounds and rotation matrix while their live inputs match. Keys include model identity, collision bounds/type, rigid mode, network transform, prop/weapon settings generations, protocol availability, and the seven global transform settings. Origin is read live when obtaining a world centre. Angles are checked on every axes query. QuakeC changes between the two hands therefore take effect immediately; this does not depend on a once-per-frame snapshot.
- **Portal-aware range filtering:** prepare local and active destination-room search origins once per force-grab query, then conservatively reject drawn centres outside all range boxes before image/visibility work. The final original distance, cone, aperture, occlusion, and closest-image rules still decide eligibility. Edict iteration and returned chain order stay unchanged. Portal inverse point mapping avoids constructing the reverse aperture's eight transformed corners for every candidate.

This step uses a conservative live range filter, **not a persistent spatial tree**. It still scans edicts, but reduces the work per entity and avoids unnecessary portal image tests. A persistent index would need reliable invalidation for direct QC field writes as well as linked movement; reusing collision boxes alone would miss offset drawn centres. The measured gains justify retaining this simpler implementation before introducing that maintenance cost.

The new caches register with `vr_memstats` and release on map changes, game-directory changes, and model reloads. Per-entity state also resets on server-world reset and entity removal. Additional CPU memory scales with entity/model counts; no GPU storage is added. Collision response, touch callbacks, damage, and Box3D solver configuration are unchanged.

## Correctness checks

`vr_prop_query_verify 1` enables uncached-reference comparisons for model bounds, entity bounds/axes, retro category/body-part metadata, portal inverse transforms, and the exact ordered force-grab candidate chain. It is non-archived and defaults off. `vr_prop_query_test` also exercises temporary transform mutations against the original calculations.

The completed suite in `Misc/quakevr/prop_query_test.py` passes **27,291 model transform cases**, each with a centre check and four point-in-model checks, plus **1,632 explicit ordered-chain comparisons**. Verification also runs during ordinary simulation/rendering throughout these fixtures. Cases cover negative/anisotropic scaling, scale origins, large offsets, position/rotation changes between queries, collision-box changes, rigid modes, global transform changes, settings-cache resets, model reload, map change, active mixed piles, and 32 ragdolls. Vanilla id1 runs in compatibility mode with the optional QC fields absent.

The real portal regression selects a grabbable gib in the destination room, launches it through gate 2, and catches it. Turning slipgates off during a second flight drops the object in its room as before. Query comparisons also pass with slipgates or physical portal walking disabled.

Seven hand-transfer fixtures are compared against baseline executable `8a8008ad`: head models, a small gib, rock, grenade, ammo box, and brick. Both-hand/hand-over outcomes match; recorded displacement differs by at most the allowed 0.03 units. Six fixtures perform all three two-hand holds and both transfers. The grunt-head fixture performs one hold/transfer in **both** builds with these settings, with 0.02 units displacement; the optimization does not introduce that fixture limitation.

The Release build passes. Its existing MSB8028 shared-intermediate-directory warning remains. The user's installed executable, progs, and configuration are unchanged; the tested executable is `build-cmake/prop-opt-20261005/bin/ironwail.exe`.

Reproduce against a private base under `build-cmake`:

```powershell
python Misc/quakevr/prop_query_test.py --base build-cmake/prop-opt-20261005/test-base --exe build-cmake/prop-opt-20261005/bin/ironwail.exe --output build-cmake/prop-opt-20261005/new-correctness --reference-exe build-cmake/decal-opt-20261005/bin/ironwail.exe
```

## Repeated timings

**48 separate-process benchmark runs**, eight scenes, three repeats per build: **57,060 measured host frames**, **3,572 GPU samples**. Active scenes measure 1,170 frames to retain whole 90-frame blast cycles; other scenes measure 1,200. Configs are byte-identical between builds. Settings use the actual user config, 2,048 × 2,048 pixels per eye, stereo mock rendering, 72 Hz fixed simulation, no audio, and GPU samples every 16 frames. Hardware is the i9-13900K and RTX 4090 used in the audit. Verification is off during performance measurements.

Before and after batches run separately, reversing scenario order on alternate repeats. Other applications remain active, and fresh-process runs can overlap background model AO baking. Small GPU/CPU differences should not be overinterpreted. CPU wall time includes driver waits and must not be added to GPU time.

Values are medians of three run means, in milliseconds:

| Scene | CPU before | CPU after | CPU reduction | GPU before | GPU after |
| --- | ---: | ---: | ---: | ---: | ---: |
| Idle | 0.951 | 0.947 | 0.4% | 0.955 | 0.945 |
| Active 100 props | 1.188 | 1.174 | 1.2% | 1.083 | 1.043 |
| Active 500 props | **2.611** | **2.350** | **10.0%** | 1.318 | 1.362 |
| Active 1,000 props | **3.999** | **3.453** | **13.7%** | 1.631 | 1.616 |
| Settled 100 props | 1.154 | 1.153 | 0.1% | 1.133 | 1.134 |
| Settled 500 props | **1.727** | **1.469** | **14.9%** | 1.390 | 1.363 |
| Settled 1,000 props | **2.590** | **2.082** | **19.6%** | 1.618 | 1.695 |
| Visible portal | 2.000 | 2.000 | 0.0% | 2.001 | 2.042 |

At 500 active props, alias-rendering CPU self time falls from 0.617 to 0.446 ms and QuakeC self time from 0.337 to 0.262 ms. At 1,000 settled props those figures fall from 1.055 to 0.757 ms and 0.310 to 0.174 ms. Light scenes and the portal scene are largely limited by GPU/driver time, so lower CPU work need not lower total wall time there.

Measured physics summaries report identical average awake/body counts for each corresponding scene across builds. For example, active 500 reports 181.8 awake / 664 bodies; settled 1,000 reports 46.0 / 1,145. Added prop counts exclude existing map objects and subsequent crate fragments. `vr_profile_report` work counters use a rolling time window which can include warmup; the faster build changes that window's frame coverage, so those counters must not be treated as exact measured-interval equivalence checks.

Server physics mean / p95, in milliseconds:

| Scene | Mean before | Mean after | p95 before | p95 after |
| --- | ---: | ---: | ---: | ---: |
| Active 500 | 1.1360 | 1.0387 | 2.3281 | 2.2634 |
| Active 1,000 | 1.8336 | 1.6023 | 4.6627 | 4.4588 |
| Settled 500 | 0.3892 | 0.2725 | 0.4729 | 0.3318 |
| Settled 1,000 | 0.6445 | 0.4394 | 0.7388 | 0.5222 |

Active-pile tails still include solver/contact work and scheduling noise: 1,000-prop physics p99 is 5.9776 before and 6.0155 after, and one optimized 500-prop run has a 10.449 ms physics maximum. This change reduces mean overhead and settled-scene tails; it does not eliminate all physics hitches.

## VTune confirmation

Matching 18,000-frame active-500-prop captures use VTune 2024.3 software hotspots with stacks, 10 ms sampling, a four-second collection delay, and the last 11 seconds excluding the final second. One preliminary baseline capture overlapped regression runs and is excluded; a replacement baseline capture runs without another test process. Game frame timings above come from the independent benchmarks, not profiler runs.

Estimated inclusive shares of late main-thread CPU work:

| Function | Before | After |
| --- | ---: | ---: |
| `VR_RetroAlias` | 9.20% | 2.44% |
| `PF_findportalcone` | 5.44% | 2.72% |
| `VR_AliasInstance` | 15.57% | 11.51% |

These are sampling shares, not milliseconds, and nested entries must not be added. With these costs reduced, alias rendering, Box3D frame-end/write/contact work, entity networking, and remaining model-transform setup are proportionally larger opportunities. The GPU workload is essentially the same.

Raw evidence is under `build-cmake/prop-opt-20261005/`: before/after captures, exact configs, screenshots, CSV profiles, completed correctness logs, and native VTune projects/reports. Committed evidence:

- [Before/after timing summary](benchmarks/20261005_prop_optimization.csv)
- [Hashes, run manifest, correctness results and VTune summary](benchmarks/20261005_prop_optimization_manifest.json)
