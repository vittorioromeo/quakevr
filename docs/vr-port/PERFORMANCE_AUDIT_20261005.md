# Retro, prop interactions and decal audit — 5 October 2026

The follow-up finds a specific decal CPU problem: regenerating all large marks' cell memberships twice on every insertion. VTune confirms that this dominates the large-blood-mark case. Prop costs are spread across alias preparation, repeated retro classification, player/force-grab queries and active-body relinking. Pre-baking retro textures is useful, particularly for static surfaces, but it cannot by itself solve dense translucent particle overdraw.

## Evidence and method

This adds **63 repeated stereo benchmark runs**, **75,328 measured frames** and **4713 GPU samples**, across 21 variants, plus **four VTune software-hotspots captures with call stacks**: 500 active props, 500 settled props, 1024 streaming bullet chips and 4096 streaming blood splatters. A 360-frame blood pilot is excluded from the repeated results. Hardware and disposable base are the same as [the initial benchmark](PERFORMANCE_BENCHMARK_20261005.md): i9-13900K/RTX 4090, 2048 square pixels per eye, actual stereo mock rendering, 72 Hz simulation, current copied graphics settings, audio off and sparse GPU timestamps. Most figures below are medians of three run means; active prop runs measure whole disturbance cycles.

VTune 2024.3 uses user-mode sampling at its fixed 10 ms interval, with stack collection and matching Release PDBs. Collection starts after four wall-clock seconds; reports also select the last 11-second window ending one second before process exit. Main-thread inclusive percentages are normalized to the resolved `SDL_main` subtree, with recursive ancestors excluded from double counting. They are **sampled CPU-work shares, not milliseconds, frame percentages or additive independent costs**. Child percentages are contained in their parents. GPU-driver spin/wait samples are kept separate. VTune changes execution overhead, so throughput comes from the independent engine benchmark runs.

The whole-process captures show substantial asynchronous `vr_ao.cpp::bakePose` work after loading. That is model-AO preparation on workers, not a per-frame prop interaction. Late main-thread reports avoid ranking that as the steady-state gameplay bottleneck. Short fresh-process throughput runs can still overlap background baking; they represent repeatable cold/warm-up stress, not a fully prewarmed headset session. No headset compositor/pacing or audio cost is measured, and other desktop GPU applications remained running.

The original executable and player configuration retain their original SHA256 hashes. Separate audit executables add only profiling scopes, a debug mark fixture and diagnostics. No retro appearance, interaction rules or decal rendering algorithm was optimized or replaced in this audit.

## 1. How much of retro rendering can be baked?

Your current world/prop settings use snapping, block 0.5 in native texture texels, centre sampling (`average 0`), palette strength 1, dither 0.5, no far smoothing (`fade -1`) and one-pixel edge smoothing. Particles instead use **0.25 world-unit blocks**, and decals 0.5 world-unit blocks: the saved `units 0` does not change this, because these categories force world units in `fillSet`.

For static world textures and rigid models with a stable skin/grid, a load-time/settings-time cache can precompute block colours and appropriate palette/dither variants. Keep the original mip chain and generate the cache from its sampling rules, rather than resizing/quantizing only the base image. Rebuild keys must include texture content/skin frame, effective override settings, palette, grid and auxiliary-map choices. Live menu edits need invalidation or a correct fallback. `fade -1` removes one runtime transition; `average 0` simplifies the near block sample. This makes a near-identical fast path plausible for your settings, but it has not been implemented or visually validated.

A single baked particle image cannot preserve all current behavior:

- Particle growth and stretching change the number of world-unit blocks across the atlas cell. The grid also uses eighth-octave rounding of world texel density.
- Mip selection and one-pixel edge smoothing depend on the current view's derivatives. Both eyes can need different filtering.
- Particle palette conversion happens **after texture × particle colour**, normalized as premultiplied colour. A common opacity fade can cancel in that normalization, but changing tint/hue, additive-versus-covered strength and blood/fire ramps cannot all be baked into one neutral atlas.
- The Bayer dither is tied to the resulting block indices. Changing the world-size grid changes its relationship to texture coordinates.

A workable design is to retain cheap view-dependent edge/filter math, move the grid construction out of each fragment where the instance geometry makes it derivable, specialize the shader for active settings, and cache reusable colour/grid/tint variants. Use a bounded cache with a faithful fallback; do not generate unbounded atlases for every continuously changing particle size or colour. Static world/model caches and dynamic particle fast paths should be evaluated separately.

### Shader feature controls

Same severe nearby smoke/blood fixture, same emission count; these switches deliberately change appearance and measure feature cost, not bake fidelity:

| Particle configuration | CPU wall ms | GPU ms |
|---|---:|---:|
| Current retro settings | 13.502 | 13.373 |
| Palette disabled | 12.131 | 12.014 |
| Edge smoothing disabled | 13.072 | 13.035 |
| Palette and edge smoothing disabled | 11.406 | 11.310 |
| Snapping disabled, palette retained | 11.394 | 11.279 |

The initial fully nonretro/full-resolution control was **9.002 ms GPU**, and nonretro with adaptive half resolution **3.026 ms**. Those are earlier-session comparisons, not interleaved with these five controls. Palette/edge removal still leaves **11.310 ms** here. That is not a measured ceiling for an optimized shader: specialization, grid work and texture/cache behavior also differ. It does establish that palette lookup or four-edge taps alone do not explain the whole gap.

At 3072 per eye the earlier dense retro case reached 30.795 ms. Its pixel scaling confirms substantial fragment/fill pressure. Baking cannot eliminate the cost of repeatedly blending thousands of large overlapping translucent quads. A retro-compatible reduced-resolution path, preferably limited to large soft particles while keeping small sparks crisp, remains necessary to test alongside a baked/specialized fast path. Validate head motion, stereo edges, depth intersections, tint changes and sprite growth against reference screenshots. Particle count alone is not a sufficient capacity metric; projected coverage and overlap are decisive.

Sources: [shared retro math](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_retro.h), [effective settings](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_retro.cpp:344), [post-tint quantization](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_gfx_gl.cpp:226), [half-resolution eligibility](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_particles.cpp:2481).

## 2. Mixed props: real interactions and submission costs

The fixture spawns rocks/bricks (`vr_debris_piece`, rigid .mdl models), with every fourth column made of crates. Active variants receive periodic radius-damage disturbances; breaking crates can add fragments. Thus the count is initially added props, not a guarantee of precisely that many bodies throughout the run. The populated range contributes 145 baseline bodies.

| Added mixed props | CPU wall ms | GPU ms |
|---|---:|---:|
| 100 active | 1.082 | 0.941 |
| 500 active | 2.656 | 1.328 |
| 1000 active | 3.894 | 1.494 |
| 100 settled | 0.993 | 1.028 |
| 500 settled | 1.691 | 1.343 |
| 1000 settled | 2.600 | 1.474 |

Hundreds of props are already within the desktop application's nominal 90/120 Hz CPU budget in this fixture. This is not a guarantee for crowded maps, higher shadow counts, active hand interactions or headset runtime overhead. The audit gives specific targets for further headroom:

| VTune main-thread inclusive CPU share | 500 active | 500 settled |
|---|---:|---:|
| Alias-model drawing/preparation | 29.65% | 39.39% |
| `VR_AliasInstance` (contained in alias) | 15.40% | 21.19% |
| `VR_RetroAlias` (contained in instance work) | 8.51% | 12.09% |
| `PF_findportalcone` | 5.58% | 6.56% |
| Box3D frame end | 16.84% | 2.83% |
| Box3D main-thread step (contained in frame end) | 6.02% | 0.36% |
| `VR_TouchLinks`, all main call paths | 6.14% | 0.45% |

**Repeated model classification is a concrete CPU target.** `entitySet` calls `isBody`, which calls `categoryOf`/`modelCategory`; a normal prop then calls `categoryOf` again. Model filename/prefix classification is repeated across entities and views. Cache the immutable model category/body metadata, preserving entity-specific held/gear/small-gib distinctions and model reload/override invalidation. Overrides and weapon transforms already have caches; do not claim that every lookup or transform is presently uncached.

**Force-grab targeting does repeated transformed-bounds work.** `PF_findportalcone` scans entities and computes `physics::modelCentre` before distance/cone rejection. Its call tree descends through local bounds, drawn model transforms and matrix work. Both hands can request targets. Reuse per-frame centres/bounds and investigate spatial candidates for the local cone and each relevant portal image, followed by the existing exact tests. Preserve edict/chain order, portal images, pickup eligibility and live scale/offset edits. A local-only radius filter would incorrectly lose objects reachable through a slipgate.

**Active write-back incurs interaction work, not just body copying.** `writeProp` calls `SV_LinkEdict(ent, true)`, which leads into `VR_TouchLinks`, area-tree candidate gathering, sorting/deduplication and QuakeC touch callbacks. Existing `propTouchIsNothing` and gib pair shortcuts already suppress known no-op callbacks. Extend candidate/state reuse selectively; do not disable touches wholesale. Damageable crates, flung props, buttons, pickups and trigger behavior must still work.

`syncEntities` still visits edicts and reconciles bodies; awake props get swept-hit, water, contact, rolling/flight and anti-stuck work. However the late samples put `syncEntities` near 1% of main CPU, so it is a lower first target than alias preparation and targeting in these cases. Settled write-back/solver cost is already small. The legacy rigid path also checks containment before returning control to Box3D: `keepInWorld` is about 2.05% of active main CPU, 0.54% settled.

The prior bone-upload suggestion was broad: **these mixed props are rigid .mdl rocks/bricks/crates, not hundreds of skinned actors**. Their sampled bone-pose checks/uploads do not dominate. Prioritize repeated classification/instance/transform work here; investigate bone reuse separately for genuinely skinned enemy/ragdoll scenes. Likewise, the earlier worker comparison improved solver tails without whole-frame throughput gains; this audit does not justify more solver threads as the first change.

Sources: [fixture](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_box3d.cpp:7066), [classification](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_retro.cpp:528), [duplicate category path](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_retro.cpp:675), [cone query](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_builtins.cpp:142), [write-back](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_box3d.cpp:6031), [interaction gathering](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_physics.cpp:1769).

## 3. Decals: membership construction, size and churn

Two fixtures both verify **4096 actual retained marks**. Bullet chips are small, spread by deterministic shots. Blood splatters are 128 units across, laid on a permuted 64×64 floor lattice through the real `place/add/clipToWorld` path. Streaming adds one mark every four simulated frames (18/s). Blood lifetime is 1200 seconds to isolate insertion/eviction churn from expiry. This is a deliberately dense stress case; normal combat also has delayed/growing/darkening marks, drops and rays that this fixture does not reproduce.

| Decal fixture | CPU wall ms | GPU ms |
|---|---:|---:|
| 4096 retained bullet chips | 1.089 | 1.089 |
| 4096 streaming bullet chips | 1.086 | 1.088 |
| Streaming chips, decal retro off | 1.055 | 1.062 |
| Streaming chips, mesh path | 0.910 | 0.929 |
| 4096 retained large blood marks | 1.473 | 1.485 |
| 4096 streaming large blood marks | 7.131 | 1.523 |
| Streaming blood, decal retro off | 7.013 | 1.317 |
| Streaming blood, mesh path | 14.182 | 13.996 |

**Retention alone is affordable; repeatedly rebuilding large footprints is not.** Streaming blood takes 7.131 ms CPU versus 1.473 ms retained. Disabling decal retro shading leaves CPU at 7.013 ms. The six new build scopes isolate the cost, in ms averaged over every host frame, including frames without a rebuild:

| Rebuild phase | 4096 streaming chips | 4096 streaming blood |
|---|---:|---:|
| Records | 0.007 | 0.009 |
| Grid sizing | 0.006 | 0.006 |
| Count memberships | 0.037 | 2.872 |
| Prefix/allocate grid | 0.018 | 0.155 |
| Fill memberships | 0.041 | 2.887 |
| Upload both buffers | 0.005 | 0.039 |

In the blood fixture each insertion triggers a whole rebuild, so the two membership passes cost roughly **23 ms combined on a rebuilding frame**, amortized to **5.76 ms per host frame**. VTune's late-window call stacks independently assign **80.82% of main-thread sampled CPU** to `worldBuckets` inclusive (72.87% self); all `buildWorld` work is 87.02%. Streaming chips put `buildWorld` at only 2.12% of sampled main CPU. Upload bandwidth is not the main cause here.

### Why it scales badly

`worldBuckets` enumerates every grid cell reached by each mark's bounding box, including a fixed 12-unit margin. For every enumerated cell it linearly scans the growing `worldMarkBuckets` vector to suppress duplicate hash buckets. With M cells/unique buckets this can approach **O(M²) work per mark**. `buildWorld` does that for every retained mark **twice**, first to count, then to fill newest-first lists, even when only one mark changed. Large rotated footprints touch many more cells than chips.

Grid sizing sums estimated footprint cells, rather than unique occupied cells. The blood case allocates **1,048,576 buckets**, but only about **681 are occupied**; roughly 590 exceed the 64-candidate cap, with about 2346 uncapped memberships in the densest bucket. The mark buffer is 320 KB, the grid about 4251 KB. Clearing/prefixing the huge table is secondary to duplicate-membership scans, but also unnecessarily broad.

### Optimization order and correctness constraints

1. Replace the per-cell linear duplicate scan with an O(1) generation-stamped bucket visitation table or another bounded dedup mechanism. Reuse the resulting memberships between count and fill. This targets the measured function without changing which buckets a mark reaches or which newest 64 are selected. Handle mask/table changes and generation wrap correctly.
2. Cache immutable footprints/memberships and update touched buckets on insert/evict. Stable decal slots and stable time origins avoid rewriting every mark when the ring changes. Size the index by occupied regions/mark count or use true-cell sparse entries, with controlled load/collision behavior, instead of a million mostly empty buckets.
3. Batch partial buffer updates once per frame and preserve delayed appearance, growth, darkening, expiry, newest ordering and parallax coverage. Growing marks currently use their full footprint in the index; do not assume they need their memberships regenerated each animation frame.
4. For still heavier GPU overlap, consider static decal compositing into bounded surface/world tiles, retaining a separate dynamic path for changing marks. Surface/parallax mapping, lighting multiplication, view-dependent retro filtering and drying/fade behavior make this a larger visual-design change than fixing memberships first.

Keep the analytic world-shader path. Switching all marks to meshes saves time for tiny chips, but dense blood becomes **13.996 ms GPU**, versus **1.523 ms** analytically, due to many overlapping mesh fragments. Existing static triangle uploads are already cached; replacing that cache does not fix the measured world-index problem.

The analytic path bounds each hashed bucket to the newest 64 candidates. That explains why total stored count need not produce linear GPU growth, and also means this test does **not** prove that all thousands of overlapping old marks are simultaneously composited at one pixel. Hash collisions can introduce irrelevant candidates, rejected by plane/rectangle tests. The performance/correctness of dense local marks and widely spread marks must both be considered when changing the index.

Sources: [membership enumeration](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_decals.cpp:1330), [rebuild scopes](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_decals.cpp:1360), [world shader candidate loop](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_glsl.h:1220), [orphan/upload path](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_gfx_gl.cpp:835).

## Reproduce and retained artifacts

[Repeated measurements](benchmarks/20261005_audit_summary.csv), [VTune main-thread summary](benchmarks/20261005_vtune_summary.json) and [capture manifest](benchmarks/20261005_audit_manifest.json) are committed. Raw VTune projects/PDB references, full CSV call trees, autoexecs, profiler captures, console logs and screenshots remain under `build-cmake/perf-audit-20261005/` outside Git. Original player settings and installed Release executable are unchanged.

Build Release with an isolated output directory. `perf_suite.py` now supports the particle feature controls, 100/1000 prop counts and 4096 chip/blood controls listed in the CSV. `vr_decal_stress [count 1..64] [size]` adds deterministic floor splatters; `vr_decal_count` reports grid occupancy, cap pressure and bytes. The extra scopes distinguish records, sizing, count, prefix, fill and upload without timing each cell individually.

```powershell
python Misc/quakevr/perf_suite.py --base build-cmake/perf-20261005/base --exe build-cmake/perf-audit-20261005/blood-bin/ironwail.exe --output build-cmake/audit-repeat/results --scenes decals_blood_4096,decals_blood_4096_stream --reps 3 --frames 1200
python Misc/quakevr/perf_analyze.py build-cmake/audit-repeat/results
python Misc/quakevr/perf_vtune.py --base build-cmake/perf-20261005/base --exe build-cmake/perf-audit-20261005/blood-bin/ironwail.exe --output build-cmake/audit-repeat/vtune-blood --scene decals_blood_4096_stream --frames 7200
python Misc/quakevr/perf_vtune_analyze.py build-cmake/audit-repeat
```

The installed VTune path is the runner's default; override `--vtune` elsewhere. WPR could not enable system-performance profiling on this host, so the working VTune user-mode profiler was used. No profiling policy was changed. All scenarios ran sequentially; VTune throughput is not mixed into the benchmark medians.
