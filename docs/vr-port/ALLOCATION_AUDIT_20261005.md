# Heap allocation audit — 5 October 2026

Follow-up: [exact combat peak attribution and the 128-size hull cache](COMBAT_ALLOCATION_BURSTS_20261005.md).

Implemented on `vr-ironwail` after `70e369c3`, preserving the shared model-metadata work committed during the audit as `8312e1ff`. The audit covers engine-owned C heap calls as well as C++ allocation, identifies the combat bursts, and removes the recurring portal/wound temporary allocations previously attributed to particle scenes.

## Monitor

- `vr_profile 1` / `vr_profile_report` retain the existing `allocations` counter for main-thread C++ `new`. New counters show C++ deletes, C heap requests/frees and requested KiB. The systems CSV adds `cpp_deletes`, `c_heap_requests`, `c_heap_frees`, and `heap_requested_kib`.
- `vr_alloc_sites 300 50`, also under Debug > Profiling and Memory > Allocation Sites, now traces `new/delete/malloc/calloc/realloc/free` separately, including request bytes, first caller outside the allocators, totals by kind, and the frame with the most allocation requests. A fixed 4,096-stack table captures without allocating; symbol resolution happens after tracing stops.
- Engine C calls use explicit CRT-compatible wrappers, declared in `vr_alloccount.h`; image libraries use their supported allocator hooks. Box3D uses its public allocator callbacks, preserving alignment. Global C++ new/delete include aligned forms and use raw CRT allocation internally, avoiding double counts.
- Counters are thread-local; profiling/tracing reads the main thread. Worker calls are wrapped but do not enter these main-thread totals. External DLL/private allocators and zone/hunk suballocations are not intercepted. The zone/hunk's CRT backing allocations are counted. This is wrapper coverage, not a process-wide CRT detour.
- Allocation counters count requests, including failed attempts; frees count non-null pointers. Request bytes measure traffic, not live memory or retained capacity; realloc counts the full requested size. CSV KiB is floored per frame; the trace retains byte totals. `vr_memstats` remains the tool for retained scratch/cache memory.

## What caused the recurring allocations?

The particle pool and instance buffers already retain their storage. The three compulsory C++ allocations per frame in the dense particle fixture came from unrelated systems:

| Source | Before, allocations/frame | Change |
|---|---:|---|
| Player wound capsules | 2 | Six-capsule inline storage (also included in `8312e1ff`); preserves local ownership and reentrancy. |
| Portal server source PVS | 1 | Registered scratch vector, retaining capacity between calls. |
| Portal destination deduplication | 1 when a destination is included | Separate scratch member cleared between calls. |
| Portal mask geometry | 12 in the stereo portal fixture | Scratch vertex buffer reserved for four quads, uploaded synchronously before reuse. |

All four sites disappear from the post-change traces once warmed. Portal scratch is released on map change and included in memory accounting. PVS generation is sequential and does not recursively invoke its hook; mask rendering uploads immediately through `gfx::draw`. Separate members avoid overwriting data across these functions. The capsule maximum is two torso/leg capsules plus two per hand.

The benchmark command buffer does have malloc paths for large command lines and inserted text, but those were not recurring costs in these captures. Continued allocations in the particle fixture predominantly came from background AO results causing `GLMesh_LoadVertexBuffer` reuploads and `VR_AliasFlameRefs`, plus periodic profiling/logging. They are not allocations per particle or obligatory per render frame.

## Combat bursts

A 360-frame early-combat capture with 48 mixed enemies recorded **2,392 allocation requests and 1,346,684 requested bytes in one frame**. Across that window, the leading C++ site was `hull::buildTree` (10,378 requests), followed by `clipWinding` (1,518 requests at each of its two initial arrays), and new decal geometry (800 requests). The hull path is `treeFor` cache miss → brush/fragments construction → recursive polygon clipping/tree construction. Different collision box extents need different compiled trees. These construction paths own recursive and worker data, so replacing them with one global scratch vector would be unsafe.

The collision cache also has a **12-tree limit which clears the whole monster-tree cache when full**. That is a potential source of repeated construction; these captures establish cache-miss construction as the allocation source but do not measure how many misses came from that full-cache clear. A follow-up should count miss/eviction reasons, preserve useful entries, prewarm known dimensions, and use builder/worker-owned arenas for transient clipping data without changing collision results.

The later combat capture had only 0.91 C++ allocations/frame, but the newly exposed C/Box3D path added 2.85 allocation requests/frame. Those include island merge/link/sleep storage, bodies/contacts, and bitset growth. Wound/decal creation and posed triangles also remain event-driven allocation sources. Background AO mesh uploads account for some larger byte bursts outside combat as well. Historical maxima from the earlier report cannot be assigned precisely from aggregate counters alone; these fresh call-stack captures provide the direct evidence above.

With 1,000 periodically blasted mixed props, Box3D allocates/frees island and solver-set storage as islands merge, wake and sleep. `b3MergeIslands`, `b3LinkContact`, `b3TrySleepIsland`, and `b3DestroyIsland` dominate. Much of this is persistent storage with object/island ownership, not one function's disposable temporary array. Capacity retention or allocator pools would need to respect those lifetimes; scratch is appropriate for genuinely temporary contact work.

## Captures and validation

22 trace runs, 360 traced frames each (7,920 total), stereo mock at 1,024 pixels per eye, fixed 72 Hz simulation, audio off, private benchmark configuration. Early scenes use their existing warmup; later/final scenes add 900 frames. Simulation and AO scheduling can differ across runs, so counts are observations, not deterministic performance guarantees. Call-stack tracing changes CPU timing; no CPU/GPU speedup is claimed from these runs.

Final longer-warmed observations:

| Fixture | C++ new/frame | C requests/frame | C frees/frame | Peak combined requests/frame |
|---|---:|---:|---:|---:|
| Portal | 0.114 | 0.042 | 0.042 | 17 |
| Dense retro particles | 0.764 | 0.303 | 0.300 | 45 |
| 48 enemies, later combat | 0.911 | 2.850 | 3.297 | 51 |
| 1,000 mixed props, periodic blasts | 0.692 | 4.650 | 6.917 | 105 |
| Active ragdolls, periodic blasts | 0.128 | 0.364 | 0.386 | 32 |

- Release x64 builds pass. The existing MSB8028 shared-intermediate warning remains.
- `vr_alloc_test` passed in all 14 instrumented post-change runs: allocation, calloc zeroing, realloc content preservation, alignment, frees, counter deltas and absence of double counting. No stack-table overflows were reported.
- Existing live regressions passed: mixed props/model reload/map change; portal pulling and toggles; seven hand transfers; 32 ragdolls; vanilla map. They performed 17 cached/uncached model sweeps and 1,632 force-grab chain comparisons.
- Tests use isolated executables under `build-cmake/alloc-20261005/` and a disposable game base. Player `ironwail.cfg` and `progs.dat` hashes match the earlier audit.

[Capture totals](benchmarks/20261005_allocations.json) record every run, including coverage differences in the initial C-wrapper baseline. Raw console traces, CSVs, executables/PDBs, configs and build logs remain under `build-cmake/alloc-20261005/`. The baseline C-wrapper build predates Box3D/image hooks and peak/per-kind summaries; the final build includes them.

Reproduce with `python Misc/quakevr/allocation_test.py --base build-cmake/perf-20261005/base --exe build-cmake/alloc-20261005/after-bin/ironwail.exe --output build-cmake/alloc-repeat --self-test --extra-warm 900`.
