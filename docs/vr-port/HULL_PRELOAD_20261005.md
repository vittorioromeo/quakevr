# Load-time brush collision hull preparation

Loaded brush models now have their collision hulls prepared before play for the player and monster bounding-box sizes known during map loading. This extends the existing world-hull preparation to doors, platforms, other inline brush models and external precached `.bsp` models. It fixes first-contact allocation bursts for those combinations.

External brush geometry is recovered on the main thread before any build workers read it. Builds run in parallel across size entries; each worker owns one tree and builds its submodels sequentially because they append to shared node/plane arrays. Existing world-build jobs are settled first. No geometry algorithm or collision dimensions changed.

Width-setting changes and enabling brush-model collision also prepare missing hulls. Disabled brush-model collision skips this work; the brush-sweep method retains its existing behavior. The 128-size cache remains demand-populated and resets on map changes.

## Native-map results

Two repetitions per executable drove the same seeded 12-second player walk, with native monster AI enabled and 900 allocation-traced frames. No manual prewarming command ran in these traversal captures.

| Map | Previous peak allocation requests/frame | New peak | Runtime hull builds, previous → new |
| --- | ---: | ---: | ---: |
| `e1m1` | 1,549 / 1,549 | 37 / 37 | 4 → 0 |
| `e2m2` | 4,628 / 4,629 | 49 / 49 | 4 → 0 |

Both versions reported identical traversal: 2,142 units on `e1m1`, 3,366 on `e2m2`, two successful hops each, and zero stuck, embedded or outside-world frames. Stack capture affects CPU timing; these are allocation comparisons, not FPS benchmarks.

The added preparation phase ran outside allocation stack tracing. Across the loading checks it took roughly **10–27 ms**, with hull audit logging enabled. These are observed phase timings, not guarantees for all maps or machines.

| Map | Brush models prepared | Known sizes | Additional hull builds | Added retained brush/tree storage |
| --- | ---: | ---: | ---: | ---: |
| `vrfiringrange` | 27 | 1 | 27 | 13,884 bytes |
| `e1m1` | 66 | 2 | 131 | 11,976 bytes |
| `e2m2` | 61 | 3 | 183 | 13,576 bytes |
| `start` | 59 | 2 | 118 | 110,932 bytes |

Storage deltas include retained brush geometry and player/monster tree capacities; these maps mostly had spare tree capacity already. One `e1m1` model/size hull had already been needed during server setup, so 131 additional builds complete its 132 combinations. Models count unique loaded submodels, including precached external models, not only currently visible solid entities.

## Runtime fallback remains

Models and dimensions introduced after loading still build on demand. The firing-range combat test spawns previously absent monster sizes after loading: two repetitions still recorded eight deferred submodel builds during measurement, with peaks of 3,801 and 1,929 requests. There were **zero external brush recoveries** in either measured window because those loaded models were already recovered during loading.

This change therefore removes first-contact work for loaded dimensions, not every possible dynamic-spawn allocation burst. The expensive first world build for a newly introduced dimension also remains. It occurs during benchmark setup, outside these measured windows. Preparing anticipated dynamic monster sizes or spreading their preparation across time is a separate possible improvement; making every size eagerly build would undermine the demand-populated cache.

## Validation and reproduction

Release build passed. Four final map checks passed **58,880 collision trace comparisons** and required zero builds when the diagnostic warm-cache command checked completeness. The reference rebuild creates fresh nodes against the same shared plane table: an isolated plane table can choose slightly different near-equal planes under the existing deduplication rules. Solid flags, fraction and endpoint comparisons keep strict tolerances. The earlier baseline/manual-prewarm and automatic-preparation runs also produced identical complete tree hashes on all four maps.

Controls passed for brush collision disabled/enabled, width changes, map resets, serial worker execution and collision-method changes. Prop-query regression passed five fixtures, 17 model sweeps and 1,632 ordered force-grab chains, including portals, hand transfers, reloads, ragdolls and vanilla operation. Allocation wrapper self-tests passed and every captured peak stack reconciled exactly with independent request/byte counters.

Use `Misc/quakevr/hull_preload_test.py` with a disposable benchmark base, final executable, output directory and optional `--baseline-exe`. Defaults check `vrfiringrange,e1m1,e2m2,start`; `--maps e1m1,e2m2 --traverse` captures native traversal without manual prewarming. `vr_hull_preloadtest` performs the fresh-node trace comparison without filling missing cached hulls. `vr_hull_audit 1` prints preparation model/size/build counts, retained bytes before/after and phase time; `vr_startup_times` includes the preparation phase.

Machine-readable evidence: [20261005_hull_preload.json](benchmarks/20261005_hull_preload.json). Local raw logs and executables: `build-cmake/hull-preload-20261005/`. Prior causal investigation: [combat allocation bursts](COMBAT_ALLOCATION_BURSTS_20261005.md).
