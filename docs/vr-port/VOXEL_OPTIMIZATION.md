# Exact cold gun voxelization optimization (2026-10-14)

Baseline d986e74397fc19a4a59fcf312ba7cab6b2b9eaad, including the shipped hull-reuse change. Worker voxelopt; experiment evidence backed up under C:/OHWorkspace/qvr-kit/scratch/voxelopt. Private frozen input triangles remain in kit/scratch/gunhullopt/corpus and are never committed/distributed.

## Change and exactness

Quake/vr/vr_convex.cpp computes each triangle's original float(i) / steps quotients once, then reuses the original left-associated row sum. It preserves subtraction, multiplication, addition, rounding, barycentric sample order and grid-coordinate division. A 1025-float local array covers the existing steps cap: 4100 bytes of bounded stack storage, no new heap allocation or retained state.

The chamfer transform uses the same 13 neighbours in the original dz/dy/dx order, with the original float weights. Linear offsets are computed once per pass. Interior cells skip redundant neighbour boundary tests; border cells retain them. Cell scan order, each ordered minimum, both passes, triangle sampling density, flood, grid sizing, splitting, hull budgets, tolerance and failure/fallback paths are unchanged. There are no cvar/default, cache-format/key/invalidation, mass reference hull, solver or collision-feature changes.

The standalone fixture now compares every sampled point, all grid dimensions/states/distances, piece point/count/order bytes, all seven non-timing report fields (including volumes), and mass/center/inertia from separately rebuilt budget-64 piece hulls. Padding and timing fields are excluded. Four shifted thin boxes cover negative/positive grid-boundary rounding and +/-100000 coordinates; a long thin box adds another extreme shape.

288 comparisons pass against the current baseline: 11 frozen inputs, all existing 278 cases plus these 10 sequential/four-thread cases. Initial and higher-budget hull failures, all-null hulls, empty/point/flat/box/L-shaped/maxPieces-zero/grid-rejected inputs retain identical outcomes. Hull allocations balance after every comparison. Budget calls are identical: 33,130 at 24, 1,096 at 36, 346 at 44; regionPoints 33,158, gap evaluations 3,864 on each side. Existing peak retained candidate capacity+hulls stays 764,508 bytes.

## Frozen-input timings

Same clang-cl /O2 /MD /DNDEBUG, vendored Box3D and built Zancle, float contraction off. For each original input/settings pair: three warmups, then 16 repetitions per side, alternating baseline/current order. Sequential scoring in this isolated fixture differs from engine job scheduling. Per-sample checking is compiled out with FIXTURE_BENCHMARK; grid-copy/counters are disabled for benchmarking. Timing includes no sample snapshot work. All worker builds/tests/decodes were paused for the granted CPU timing window.

Median milliseconds; voxel savings 22.5-37.4%, full sequential decomposition savings 5.8-19.9%. This attributes the combined patch; stages were not separately ablated. Per-repetition values and min/max ranges are in bench.csv and bench_summary.json.

| Frozen input | Voxel before to after | Saving | Decompose before to after | Saving |
|---|---:|---:|---:|---:|
| 040e452bb7739798 | 6.067 to 4.331 | 28.6% | 14.668 to 13.056 | 11.0% |
| crowbar (1332 corners) | 4.989 to 3.747 | 24.9% | 10.718 to 9.540 | 11.0% |
| shotgun (4728 corners) | 9.332 to 7.232 | 22.5% | 29.161 to 27.430 | 5.9% |
| 8e7a0f99b47d29db | 24.626 to 18.915 | 23.2% | 66.040 to 61.123 | 7.4% |
| 9dab5acd5f582b79 | 33.027 to 24.128 | 26.9% | 68.678 to 60.065 | 12.5% |
| a4d5f0351a595bbe | 15.112 to 10.304 | 31.8% | 58.899 to 55.469 | 5.8% |
| a8e5b60c5446f970 | 15.298 to 10.414 | 31.9% | 59.315 to 55.680 | 6.1% |
| grunt gun (3570 corners) | 29.466 to 19.020 | 35.4% | 73.879 to 63.503 | 14.0% |
| d161a64cf1f33be8 | 16.000 to 10.960 | 31.5% | 52.902 to 48.508 | 8.3% |
| e42b76bf4f43ad62 | 28.960 to 19.120 | 34.0% | 56.979 to 48.818 | 14.3% |
| e6cc830a50488544 | 35.653 to 22.328 | 37.4% | 63.893 to 51.173 | 19.9% |

Reproduce correctness with Misc/quakevr/convex_fixture/run.ps1 -Corpus C:/OHWorkspace/qvr-kit/scratch/gunhullopt/corpus -Baseline d986e74397fc19a4a59fcf312ba7cab6b2b9eaad. Add -BenchmarkRepeats 16 for the bounded benchmark; compilation must finish before timing other work. Build/generated files remain ignored under scratch/gunhullopt.

## Live capture exclusions and limits

Two exclusive game pairs, A/B/B/A, use distinct verified baseline/current optimized Release-Tracy depth-0 executables, system tracing/sampling off, details 0, seeds 7, flatscreen 960x540, host_framerate .004, maxfps 250. Cache mode 0 forces cold generation each new process. Corpus dumping is disabled; decoding happens afterward. All corrected captures connect and exit 0.

The live kill/spawn scene fails whole-state matching: final hashes/time/counts differ even between baseline repeats (5.568 vs 5.632 seconds); optimized repeats end at 5.560 and additionally create an axe. Cache mode 0 annotations have key=0, and no per-call input hashes were recorded, so identical live corner data cannot be established. Live caller timings are retained in game_summary.json and raw CSVs for investigation, and excluded from all speedup claims. This variance also prevents whole-scene/frame-tail conclusions. Frozen bytewise comparisons provide the quality evidence. No game-wide or VR frame-budget speedup is established. Warm cached cuts bypass voxelization.

Further evidence exclusions: before_1/after_1/after_2/before_2 accidentally used the same binary after timestamp-preserving source copies made incremental MSBuild skip a baseline rebuild. Their captures are retained and excluded. The corrected set is before_3/after_3/after_4/before_4; source timestamps were refreshed, both rebuilt and distinct SHA-256 hashes verified before capture. bench_instrumented.csv retains exploratory timings with no-op sample calls and is excluded from the final fixture numbers.

## Validation and VR acceptance

Normal kit build passes QC (0 warnings), FGD (367 entities), static/precedence checks and engine; optimized profiling builds pass. Post-build e1m1 smoke exits 0. Cache-disabled, zero-leniency collision checks pass: shell seats through the shotgun port with 12 pieces (clip 1), stays out with one hull (clip 0); a loose magazine seats by contact in the lying nailgun well. All exits are 0.

The author should confirm held/mirrored and dropped gun ports/wells in VR. Raw captures, exact scripts, fixture sources/executables, source snapshots, logs and summaries are retained locally; no cleanup was performed. Scratch artifacts are optional cleanup after review.
