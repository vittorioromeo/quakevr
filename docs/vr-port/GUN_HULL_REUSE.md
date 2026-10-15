# Gun hull local reuse (2026-10-14)

Baseline: `a381daa432260157d4ddfd59d65f0f81298e2337`, plus the same narrow Tracy scopes. Optimized Release-Tracy x64, callstack depth 0, system tracing/sampling off; exclusive kit runs, seeds 7, flatscreen 960x540, fixed host_framerate .004, maxfps 250, QC/memory/GPU detail 0. Baseline source/executable, optimized executable, exact scripts, raw captures, CSV exports, and private corpus are retained in `C:/OHWorkspace/qvr-kit/scratch/gunhullopt`. Corpus capture was separate; timed runs had dumping disabled. No cache files were manually deleted. Compilation changes the existing cache build namespace; normal first-write pruning remains unchanged.

## Change and exactness

`vr_convex.cpp` retains each shortlisted cut's original sorted point vectors and budget-24/fallback hulls in at most nine owned slots. Deterministic reduction still uses candidate order and strict `<`; losing slots clean up on every exit. The winning slot transfers its initial hull into the same shortfall check and budgets 36/44. Whole-hull construction, null/fewer-than-four semantics, fallback budgets, region bounds, erase/append order, and final point order are unchanged. A done piece retains its already computed gap because its hull/grid have not changed.

No settings/defaults, cache keys/formats/invalidation, mass reference hull, collision features/materials/filters, solver or voxelization algorithm changed. Optional scopes cover whole cut/key/read/write, decomposition and its major stages, disk prune/directories/preparation/oldest wait/submit/async; disabled normal builds eliminate the hooks. Disk writes remain asynchronous after unchanged synchronous preparation.

## Frozen output regression

`Misc/quakevr/convex_fixture/run.ps1 -Corpus C:/OHWorkspace/qvr-kit/scratch/gunhullopt/corpus` extracts the pinned baseline with git, compiles isolated baseline/current copies against vendored Box3D and the built Zancle library, and runs both on the same private inputs. Requires the profiling build's Zancle library and local Visual Studio/LLVM tools. Original production files are not modified. Private input triangles are not committed or distributed.

Eleven engine inputs include base held/dropped guns and the exact grunt-gun hitch input. 278 comparisons pass: maxPieces 1/2/6/12/32, tolerance 0/100, cell .08/.25, initial-budget failure/fallback, failed higher-budget refinement, all-null hulls; empty, repeated point, flat, box, L-shaped, maxPieces 0 and grid-rejected inputs; sequential and four-thread scoring. Every piece count, point count/order/byte and every non-timing report field matches. Report padding is ignored; ms is intentionally excluded. Box3D allocated bytes return to their initial value after every comparison.

Across these comparisons: budget-24 attempts 36,726 -> 33,120; regionPoints 36,756 -> 33,148; gap evaluations 3,946 -> 3,854. Higher-budget attempts stay exactly 1,096 at 36 and 346 at 44. The peak sum of retained candidate point capacities plus returned hull allocation sizes is 764,508 bytes (0.729 MiB). This is the total retained candidate heap measured in this corpus, not an incremental RSS delta; grids/work/pool scratch are excluded. Retention ends each scoring iteration; nine slots bound the additional ownership.

## Game measurements and limits

| Grunt gun, same 3,570 corners / most 12 | Before ms | After ms |
|---|---:|---:|
| First cold cut, disk mode 1 | 35.829 | 36.952 |
| Warm disk hit | 0.171 | 0.185 |
| Uncached cut, mode 0, two-run median | 37.162 | 36.747 |
| Uncached cut range | 36.816-37.508 | 36.451-37.044 |
| Winning/whole refinement scopes, uncached median | 1.531 | 1.119 |

For the cold grunt cut, baseline decomposition is 35.517 ms and voxelization 19.565 ms; key/read/write preparation is about 0.30 ms combined, so decomposition accounts for nearly all of the hitch. Baseline first-write crowbar prune is .068 ms; directories .925 ms. No oldest-write wait occurred. Warm cuts bypass decomposition (.171-.370 ms before, .185-.446 ms after).

The local refinement saving repeats (cold 1.514 -> 1.109 ms; uncached ranges 1.483-1.580 -> 1.107-1.132 ms), justifying bounded temporary retention. Whole grunt cuts improve only about 1.1% in two uncached repeats, and the single cold pair increases: these measurements do **not** establish a statistically strong whole-hitch speedup. Voxelization remains dominant. Crowbar uncached medians 9.907 -> 9.603 ms; shotgun medians 24.718 -> 26.825 ms include a post-change voxelization outlier 18.163 ms (other shotgun voxel passes 12.076-12.526 ms). Its origin is unresolved; no scheduler samples were collected. Worker region-point CPU overlaps the main thread and must not be summed as elapsed cut time. Initial/refinement scope boundaries differ because the winning initial hull is now reused; compare whole cut/decompose for overall effects.

Do not infer a broad hitch fix, warm-cache improvement, or a solver/ragdoll change. This patch removes deterministic duplicate work while preserving geometry. Further caching/prewarm/voxelization changes are outside this patch.

## Validation

Both normal kit build (QC, FGD 367 entities, static/precedence checks, engine) and Release-Tracy depth-0 build pass. The standalone frozen fixture passes. Normal e1m1 smoke exits 0. With disk cache disabled and zero loose leniency, a shell dropped onto the held shotgun enters its port with 12 pieces (clip 1) and stays out with one hull (clip 0). A loose magazine seats in the lying nailgun's well by contact. All collision runs exit 0.

The melee canary retains 36/45 passing verdicts; automatic full replay retains 168/177. Both are compared by eval.sh against the retained 177-row `C:/OHWorkspace/qvr-kit/eval_baseline.csv`; the same nine baseline FAIL verdicts remain, with no verdict changes. This is a retained-baseline comparison, not a fresh baseline executable replay. One baseline failing take (`slash_horizontal_ltr_2026-10-12_12-29-35.csv`) has a push/bash event at .391 rather than .375 seconds; its FAIL verdict and .391-second damage event are unchanged. No melee tuning or baseline changes were made.

The author should confirm held/mirrored and dropped guns' ports/wells in VR. Scratch captures/corpus/executables are retained for review; no cleanup was performed.
