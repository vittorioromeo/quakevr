# Decal bucket deduplication and membership reuse

Implemented directly on `vr-ironwail`, following the [profiling audit](PERFORMANCE_AUDIT_20261005.md). The expensive 4,096 large-splatter streaming workload drops from **7.045 to 1.709 ms per CPU frame**. This is a measured mock-renderer result, not a headset frame-rate guarantee.

## Change

`Quake/vr/vr_decals.cpp` replaces each mark's linear scan of previously visited hash buckets with a reusable generation-stamp table. Membership discovery is now linear in the number of covered cells. It appends each distinct bucket in the same first-cell order as before.

A flat membership array and per-mark offsets retain that result for the fill pass. Counting and filling use the same memberships; the fill pass still visits marks newest first. Grid sizing, hashing, parallax margin, newest-64 cap, record layout, shader, and uploads retain their previous behavior. Scratch capacity is reused across rebuilds. A table-size change resets stamps, and unsigned generation rollover clears old stamps before generation 1 is reused.

The tradeoff is additional retained CPU scratch memory: at most 4 MiB for the stamp table, plus four bytes per cached membership and one `size_t` offset per mark plus the final sentinel, with vector capacity overhead. The dense 4,096-mark correctness fixture uses capacities of 4,194,304 stamp bytes, 4,199,476 membership bytes, and 43,160 offset bytes. Membership memory depends on footprint and mark count; it is not a fixed 4 MiB limit. No new GPU buffers are added.

## Validation

Release MSBuild succeeded into `build-cmake/decal-opt-20261005/bin/`. The build retains an existing MSB8028 shared-intermediate-directory warning. The installed game executable and user configuration hashes are unchanged.

`Misc/quakevr/decal_grid_test.py` extracts and compiles the actual grid-building sections from the working source and baseline commit `5d8fae73`, using vendored GLM and Zancle. **166 comparisons pass**, checking every grid word and every mark's ordered memberships. Cases cover random rotated footprints, negative coordinates, cell boundaries, hash collisions, overlapping marks above the newest-64 cap, maximum and shrinking grids, reused scratch, empty grids, and forced stamp rollover. This verifies the CPU index data exactly rather than relying on screenshots alone. An optimized real-renderer splatter screenshot was also inspected.

Run the comparison with:

```powershell
python Misc/quakevr/decal_grid_test.py --vcvars 'C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat'
```

## Repeated benchmarks

Thirty separate-process runs: five scenes, three repeats per build, 1,200 measured frames per run, **36,000 measured host frames and 2,250 GPU samples**. Both builds use byte-identical scenario configs, the actual user graphics settings, 2,048 × 2,048 pixels per eye, the stereo mock renderer, fixed 72 Hz simulation, and GPU sampling every 16 frames. Hardware is the i9-13900K / RTX 4090 used in the audit. Baseline runs precede optimized runs; scenario order reverses on alternate repeats within each batch. Other applications remain active, so small differences should not be interpreted as regressions or improvements. Fresh-process setup can overlap background model AO work, as described in the audit.

Values below are medians of three run means, in milliseconds. CPU wall time includes driver waits and must not be added to GPU time.

| Scene | CPU before | CPU after | GPU before | GPU after |
| --- | ---: | ---: | ---: | ---: |
| Idle | 0.814 | 0.824 | 0.843 | 0.856 |
| 1,024 streaming bullet chips | 1.046 | 1.049 | 1.054 | 1.061 |
| 4,096 streaming bullet chips | 1.061 | 1.081 | 1.066 | 1.074 |
| 4,096 retained large splatters | 1.474 | 1.483 | 1.482 | 1.501 |
| 4,096 streaming large splatters | **7.045** | **1.709** | 1.508 | 1.489 |

The large-splatter streaming case adds one real size-128 splatter every four frames while retaining 4,096 marks. It rebuilds the index on 25% of measured frames. Its CPU phase means, averaged across **all** measured frames:

| Phase | Before | After |
| --- | ---: | ---: |
| Membership count | 2.836 | 0.327 |
| Grid fill | 2.872 | 0.155 |
| Prefix construction | 0.151 | 0.145 |
| Buffer upload | 0.039 | 0.034 |
| Whole grid build, inclusive | 5.911 | 0.674 |

Count plus fill falls **91.6%**, from 5.708 to 0.482 ms per measured frame, approximately 22.83 to 1.93 ms on a rebuilding frame. Total CPU frame mean falls **75.7%**. The largest observed whole grid build across the three runs drops from 32.916 to 4.179 ms; these maxima are observations, not frame-time percentiles. GPU work remains essentially unchanged. Retained marks have no per-frame index rebuild to optimize, and small bullet footprints previously spent little time in deduplication.

Raw captures, exact configs, logs, screenshots, CSV profiles, binaries, and the generated comparison harness are under `build-cmake/decal-opt-20261005/`. Compact committed evidence:

- [Before/after summary](benchmarks/20261005_decal_optimization.csv)
- [Run manifest, executable/config hashes and frame counts](benchmarks/20261005_decal_optimization_manifest.json)

At this workload the first optimization removes the dominant CPU cost. Incremental bucket updates can remain a later, separately measured change; they are not necessary to obtain this improvement.
