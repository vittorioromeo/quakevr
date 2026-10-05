# Combat allocation bursts — 5 October 2026

The monster collision-tree cache now retains up to **128 bounding-box sizes**, up from 12. Trees are shared per size, not per monster. Capacity is reserved before builds to preserve tree pointers. Increasing the cap does not eagerly build 128 trees.

## Method and evidence

Twelve hidden mock runs captured 10,800 frames at fixed 72 Hz and 1,024-pixel eyes. Each retained allocation stack counters from its busiest individual frame. Every peak's stack counts and requested bytes matched independent frame counters; no stack-table overflow occurred. Heap-wrapper self-tests passed. Controls disabled decals, removed combat, added 900 warm-up frames, or prebuilt loaded brush hulls before capture.

Full peak stacks, cache snapshots and matching hull events are in [20261005_combat_bursts.json](benchmarks/20261005_combat_bursts.json). Raw logs are local build artifacts under `build-cmake/combat-bursts-20261005/`.

| Capture | Peak requests | Hull requests | Direct cause |
| --- | ---: | ---: | --- |
| Detailed combat, repetition 1 | 2,392 | 2,230 | First collision with `vr_spawnpanel.bsp`: 387 requests recovering brushes plus 1,843 compiling its 16×16×56 collision hull |
| Detailed combat, repetition 2 | 3,803 | 3,798 | First 24×24×56 collision hulls for `vr_spawnbutton.bsp` and `vr_spawnpanel.bsp`: 1,899 requests each |
| Combat without decals, both repetitions | 2,355 | 2,230 | Same panel first-touch work |
| Idle, both repetitions | 31 | 0 | Model vertex-buffer/AO and flame-reference work |

Detailed peaks occurred at host frames 379 and 451. Hull diagnostics recorded model, dimensions, submodel and allocation counts at those exact frames; captured stacks independently attributed the same counts to brush recovery, `buildTree` and `clipWinding`. Hull request traffic was 276,008 bytes and 509,624 bytes, respectively.

**There were no cache evictions.** Combat retained two monster size entries throughout. These bursts were deferred brush/submodel builds for already-cached dimensions; raising the size limit alone will not remove them. The largest measured examples belong to firing-range helper brush models. Their prevalence in ordinary campaign combat has not been measured here.

## Causal control and later combat

The manual diagnostic `vr_hull_warmcache` built loaded brush hulls for the three currently cached player/monster dimensions before tracing. It performed 81 builds, leaving two monster size entries and 449,864 bytes (about 439 KiB) of combined player/monster tree storage.

Neither subsequent combat capture built any hull. Peaks dropped to **163 and 162 requests**, with zero hull requests and zero cache clears. This isolates deferred collision builds as the cause of the thousands-of-requests bursts. Automatic load-time prewarming is not enabled by this change.

Remaining peaks still requested about 1.07 MB, largely through posed/wound geometry and decals. Request bytes measure traffic, not retained memory; lowering call counts does not necessarily lower traffic proportionally.

With another 900 warm-up frames, neither capture built hulls. Peaks were 162 and 73 requests. The first included wounds, decals and posed triangle/vertex buffers. The second included creation of a 13-part ragdoll, with stacks through `createRagdoll`, `b3CreateHull`, `b3AddHullToDatabase` and `b3CreateBody`. These bodies/shapes have persistent ownership and cannot simply share frame scratch storage.

The next targeted optimization for the largest bursts is to prepare relevant brush submodel hulls during map setup or reuse temporary builder storage. Wound/posed geometry scratch reuse and ragdoll shape caching address the separate smaller events.

## Diagnostics and validation

- `vr_alloc_sites 900 200 1`: window stacks plus busiest-frame `peak_site` counters and host frame. Third argument defaults to zero.
- `vr_hull_audit 1`: opt-in brush recovery/build/eviction logs. `vr_hull_stats` reports slots, limit, clears and runtime builds since map load.
- `vr_hull_warmcache`: synchronous manual first-touch control, run outside the measured window.
- `vr_hull_cachetest 120`: builds distinct real world hulls, revisits hashes and checks no additional builds/clears and stable tree pointers. Requires enough free slots on the current map.

Release build passed. Prop-query regression passed five fixtures, 17 model sweeps and 1,632 force-grab chains, covering reloads, portals, hand transfers, ragdolls and vanilla behavior. Final cache test retained **120 distinct sizes**, with no clears, no revisit builds and stable pointers; retained tree storage was **17,426,336 bytes (16.6 MiB)**. Reloading `e1m1` correctly reset the cache to one active size. Allocation self-test and exact peak-counter reconciliation also passed on the final executable.

Stack tracing affects timings: these results establish allocation causes, not uninstrumented CPU performance. Captured stacks cover the main thread's C++ allocations and owned-engine C heap wrappers, excluding DLL-private heaps and worker threads.
