# Particle margins and active-prop callback work — 5 October 2026

The prop optimization is enabled. Conservative particle geometry trimming is available under Graphics > Models and Effects, but remains **off by default**: measurements did not justify enabling it universally. This work builds on the earlier [retro particle path](PARTICLE_OPTIMIZATION_20261005.md) and [prop query caches](PROP_OPTIMIZATION_20261005.md), and retains the shared ModelMetadata/allocation work committed by the other agent.

## Active props

VTune software hotspots with stacks on the 1,000-active-mixed-prop fixture identified `writeProp → SV_LinkEdict → VR_TouchLinks → callField → PR_ExecuteProgramRun` as a significant source of work. Within the captured `writeProp` tree, inclusive sampled CPU totals were 4.619 s for `writeProp`, 4.362 s for linking, 4.265 s for touch dispatch and 3.028 s for `callField`, compared with 1.008 s for area queries. These are nested sampled totals, **not frame times or additive costs**. Fine profiling instrumentation also contributes to the callback totals. A deeper three-dimensional area-tree prototype increased cost and was discarded.

The retained changes address callbacks instead:

- QuakeC computes the two nearest-box-point approach speeds only after the flung-prop eligibility, grace, cooldown and mass checks pass. Slow velocity lengths reject contacts before those calculations, with a rounding margin; potentially damaging contacts still use the original exact approach calculations.
- The engine skips known rigid-prop touch callbacks outside an active throw when impact damage is disabled or either velocity cannot reach even the most lenient damage threshold. It uses the global lower bound on mass leniency, with a 1% rounding margin. Gib grace bookkeeping, hands, active throws, unknown callbacks and Box3D's separate hard-impact callbacks retain their original path.
- Entity linking, candidate ordering and spatial queries are unchanged. No contact, throw, bounce or damage parameter is reduced to obtain the saving.

`vr_prop_touch_fast 0` disables the new engine guard for comparison. It does not undo the QuakeC early gates. `vr_prop_touch_verify 1` samples each recognized skipped callback type once per host frame, executes its original callback, and checks all entity bytes, declared QC globals and broadcast message bytes/lengths for changes. Compiler scratch and argument registers are excluded. `vr_prop_touch_stats` prints the number checked. Verification is off by default and is deliberately expensive.

### Paired measurements

The user confirmed that local inference was running during the final measurements. Historical absolute frame comparisons were therefore abandoned. The following results use **the same integrated executable and optimized QC**, six alternating on/off phases in one process, three repeats per state, 3,600 measured frames per phase, fixed 72 Hz simulation, 1,000 mixed props blasted every 90 frames, 128-pixel stereo eyes and GPU timestamps disabled. This isolates the new engine guard; it is not a whole-change before/after comparison.

Median of repeated phase statistics, milliseconds:

| Scope | Guard off | Guard on |
|---|---:|---:|
| Server physics mean | 1.3075 | 1.0554 |
| Server physics p95 | 2.9543 | 2.4966 |
| Server physics p99 | 5.5952 | 5.1808 |
| Prop write mean | 0.3360 | 0.2215 |
| Prop write p99 | 2.1730 | 1.7805 |
| QuakeC self CPU scope | 0.444 | 0.291 |
| Box3D step p95 | 0.5771 | 0.5768 |

Server physics mean fell about 19%, with all three paired means lower when the guard was enabled. Concurrent inference still affects scheduling and unrelated rendering, so these are provisional throughput measurements. Solver work remains; the largest physics spike was 8.311 ms off versus 9.617 ms on. This does not eliminate all hitches or establish a stable end-to-end frame improvement.

## Particle margins

`vr_particle_trim 1` enables a separate conservative vertex path. At atlas creation, the engine finds bounds of every nonzero premultiplied RGBA texel, including a 16-texel neighboring-cell guard. It uses no opacity cutoff. A sprite must offer at least 10% potential area savings even with the maximum accepted padding; batches without useful candidates select the existing shader directly.

Eligible larger, nearly image-parallel quads trim their transparent margins while preserving the original UV grid, tint, palette operations and particle order. Bounds include retro block-center taps, bilinear/mip filtering and derivative helper pixels. Only atlas mip footprints through level 2 are accepted. Near-clipped or tilted quads, large sampling footprints, octave boundaries, averaging and general retro settings retain full geometry. Nonretro rendering and the specialized current-setting retro path have trim variants; reference and general retro shaders remain unchanged. The existing half-resolution pass can also use the conservative geometry. Instance uploads remain once per host frame and the instance remains 96 bytes.

This is not a solution for dense smoke by itself. The authored smoke has nonzero opacity across essentially its full bounding rectangle. Preserving those faint wisps leaves little area to remove; fire and gun-smoke sprites offer more opportunity. The existing optional half-resolution path remains the substantial overdraw reduction for thousands of overlapping smoke particles.

Final alternating fire-emitter measurements at 2,048-pixel eyes showed particle GPU time of 0.436 ms off versus 0.508 ms on, while total GPU time moved in the opposite direction (2.480 versus 2.405 ms). Earlier dense-particle and explosion prototypes were also inconsistent or slower. Local inference confounds the absolute measurements, and the added vertex work can exceed the saved fragments. Consequently **trimming stays opt-in**, with its cost explained in the menu; no particle speedup is claimed.

## Validation and reproduction

- Release x64 engine and optimized FTEQCC game program build successfully; QC reports zero warnings. QC precedence and the 248-entity FGD coverage checks pass.
- Live prop comparisons cover mixed piles, model/override reloads, map changes, seven hand-transfer fixtures, slipgate pulls/crossings/cancellation, 32 ragdolls and vanilla gameplay. Exact query ordering comparisons and baseline hand outcomes pass. The integrated mixed-pile diagnostic validates **914 skipped callbacks** without state mutation, also tested against the original compiled QC.
- Five existing gameplay tests compare original versus optimized QC: gibs from melee and shotgun gibbing underfoot, a fresh gib thrown at a monster, a monster killed underfoot and a backpack dropped onto the player. Health outcomes, fresh-gib grace counters and drop-grace counters match exactly; the deliberately thrown gib still damages the monster.
- **18 direct stereo eye-capture fixtures, seven modes each**, pass at odd 1,025-pixel eye dimensions: smoke, blood, sparks, explosion, heavy overlap, mixed effects, averaging, palette off, snapping off, distance fade, wide blocks, angled streaks, soft particles off, nonretro, fire and dense fire in both retro and nonretro modes. Comparisons use repeated same-mode controls for animated scene noise, with absolute limits on control error. Dense fire exercises the trimmed half-resolution variants. These are image regressions, not bit-exact guarantees or headset validation.
- Window-back-buffer screenshots intermittently omitted particles in both the unchanged baseline and the integrated build. The regression helper now reads the final left/right eye targets before runtime submission, eliminating that capture failure in the final checks. Failed window captures and discarded prototypes are retained in raw evidence but excluded from passing results and speed claims.

`Misc/quakevr/perf_pair.py` alternates one numeric VR cvar in a single mock-engine process and resets the scene for each phase. Example:

```powershell
python Misc/quakevr/perf_pair.py --base <disposable-base-under-build-cmake> --exe <release-exe> --output <new-output-directory> --scene props_active_1000 --cvar vr_prop_touch_fast --eye 128 --gpu 0 --frames 3600 --reps 3
python Misc/quakevr/perf_analyze.py <output-directory>
```

The normal suite and VTune helper also accept numeric `--cvar` overrides. All runs use private game configuration and `-noconfigwrite`; player configuration and the installed game program were preserved. Build outputs are isolated under `build-cmake/overdraw-prop-20261005/`. Raw configs, scope CSVs, direct eye images, gameplay comparisons and native VTune data remain there. The normal engine build output also changed during parallel work; this task's builds use isolated output directories.

Committed evidence: [paired timing tables](benchmarks/20261005_overdraw_prop_tails.csv) and [run hashes, repeated measurements and correctness results](benchmarks/20261005_overdraw_prop_tails_manifest.json). Repeating the paired runs on an idle system is needed before making stable absolute performance claims or considering a different particle-trim default.
