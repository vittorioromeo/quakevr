# Retro particle path optimization — 5 October 2026

Implemented directly on `vr-ironwail` after `d96800d8`. The default full-resolution path retains the current retro sampling rules while moving planar grid construction out of the fragment shader and specializing the common settings. An optional retro-compatible half-resolution mode addresses overdraw, with an explicit quality tradeoff.

## What changed

- The particle vertex shader derives the UV-to-world density from the final quad, including rotation, streak stretching and the per-eye soft-particle pull. It keeps the original eighth-octave density rounding and atlas-wide grid/dither coordinates. The dual basis handles non-orthogonal axes. Near rounding boundaries or degenerate geometry, the original derivative calculation remains the fallback.
- A bounded shader variant handles snapping with centre sampling, no distance fade and full palette strength: the current saved particle settings. Other effective settings select the general fast shader. The CPU checks the effective GPU set, so live menu edits need no atlas rebuild or shader recompilation for each value.
- Texture filtering, edge smoothing, mip selection and palette conversion after particle tint remain in the fragment shader. Colours, opacity, emission counts, lifetimes, simulation and once-per-frame instance upload retain their existing behavior. This is a faster dynamic rendering path, rather than a single pre-quantized atlas that loses growth/tint-dependent behavior.
- Retro particles can optionally use the existing adaptive half-resolution target. The grid and palette remain retro; large particles render below small full-resolution particles. Covered half-resolution fragments are rejected before expensive texture/palette work. Ceil-sized viewports cover odd target dimensions.
- Seven lazy shader programs bound the variants. A failed full-resolution fast program falls back to the reference program; a failed half pass falls back to full resolution. Wave-conforming liquid particles retain their existing mesh path.

## Controls

Graphics > Models and Effects exposes:

| Control | Cvar | Default | Behavior |
|---|---|---:|---|
| Fast Retro Particles | `vr_particle_retro_fast` | 1 | Fast full-resolution path; 0 selects the original full-resolution fragment path for comparison. Retro half resolution always uses fast shaders. |
| Half-Res Retro Particles | `vr_particle_retro_halfres` | 0 | Opt-in adaptive reduction during heavy overlap. Independent of the existing nonretro half-resolution switch. |
| Retro Half-Res Size | `vr_particle_retro_halfres_pixels` | 64 | Minimum projected half-size in full-resolution pixels. Smaller particles render at full resolution. 0 draws all particles in their existing order at half resolution. |

The half mode uses the existing coverage hysteresis: enabled above 1.5 views of estimated large-particle coverage, disabled below one. Both eyes share the decision, and each eye builds its own stretched/pulled geometry. Splitting sizes changes the large/small blend order; zero retains particle order within the half pass. Half resolution softens edges and changes view-dependent filtering. It stays off by default.

## Measurements

Final measurements cover **42 runs, 50,400 measured frames and 3,150 GPU samples**, using three repeated runs per configuration, 1,200 measured frames per run, alternating scene order, actual stereo mock rendering, fixed 72 Hz simulation, audio off, and GPU timestamps every 16 frames. Hardware: i9-13900K / RTX 4090, driver 610.88. Settings come from the same private copy of the player configuration used by the earlier audits. The reference uses the original fragment grid/colour algorithm in the same executable, with retro half resolution disabled.

The 3072 dense fixture ends with 9,218 live particles in every variant/repeat, below the 32,768 cap. This is a live-count snapshot after measurement, not a guarantee that every particle contributes visible fragments. Projected coverage and overlap matter more than count alone.

| Eye size | Mode | CPU frame ms | GPU frame ms | Particle GPU ms | GPU frame reduction |
|---:|---|---:|---:|---:|---:|
| 2048 | Reference full | 13.552 | 13.395 | 12.330 | � |
| 2048 | Fast full (default) | 11.674 | 11.581 | 10.525 | 13.5% |
| 2048 | Retro half, 64 px threshold | 5.968 | 5.989 | 5.032 | 55.3% |
| 2048 | Retro half, all | 5.929 | 5.928 | 4.981 | 55.7% |
| 3072 | Reference full | 31.324 | 30.992 | 29.374 | � |
| 3072 | Fast full (default) | 26.959 | 26.773 | 25.195 | 13.6% |
| 3072 | Retro half, 64 px threshold | 12.658 | 12.516 | 11.025 | 59.6% |
| 3072 | Retro half, all | 12.603 | 12.530 | 10.984 | 59.6% |

CPU frame time includes GL/driver waiting and is not CPU simulation time. At 2048, simulation remains about 0.049 ms and instance construction about 0.067 ms; reducing GPU work lowers CPU wall time spent in rendering. CPU and GPU columns cannot be added. The shader gains do not imply a physics or particle-simulation speedup.

At 2048, normal 32-torch fire has GPU frame time 1.135 → 1.125 ms; its particle pass is 0.123 → 0.110 ms. Four large particles per explosion produce 1.205 → 1.154 ms GPU, with the particle pass 0.325 → 0.276 ms. These reference controls were run as a separate sequential batch, so small whole-frame differences can include drift. The existing nonretro half-resolution control remains 3.054 ms GPU; disabling the palette/style changes the look and is not a like-for-like optimization result.

Dense retro particles still dominate GPU time. Full resolution cannot remove the cost of blending thousands of overlapping quads. Even the optional half mode's 3072 worst-case fixture exceeds an 11.1 ms application GPU budget. These unpaced mock runs omit headset compositor/pacing and audio and do not establish headset frame-rate guarantees. Other desktop applications remained running.

## Validation and resources

Release MSBuild succeeds; the pre-existing shared-intermediate-directory MSB8028 warning remains. `particle_path_test.py` captures 14 frozen-particle fixtures at both 1024 and 1025 per eye, five modes each, with both eyes side by side: **140 screenshots / 280 eye images**. Cases cover growing smoke, blood, additive/streaked effects, explosion colour, dense overlap, hard edges/averaging, palette off, snapping off, far fading, larger blocks, angled viewing and soft particles off. It switches settings and shader variants within each process, checking compilation/linking and bounded RGB-error comparisons against a reference-again control.

The heavy full-resolution fixture differs by only 0.0085–0.0087 RGB8 mean absolute error, versus 0.0078–0.0079 for reference-again. Half resolution differs by 0.68–1.11 RGB8 in that fixture, demonstrating its quality tradeoff. The complete results retain maxima and changed-pixel fractions. Animated CRT screens/lighting and the avatar's motion continue outside the frozen particle simulation, so whole-image errors are not bit-exact shader equivalence tests; the angled case particularly includes changing avatar pose. Difference-image inspection locates the larger wide-block changes on animated background screens, rather than the flame. Visual review covers the mixed and heavy smoke scenes. Headset motion/comfort remains a user-side check.

There is no new unbounded texture/tint cache or additional per-instance upload. Default full resolution adds shader variants, not new render targets. Optional half resolution reuses the existing bounded two-target cache (RGBA16F at half size, approximately 8 MiB for a 2048-square scene or 18 MiB for 3072 per cached size). `vr_particle_freeze` is a non-archived simulation diagnostic, off by default.

The installed Release executable, `quakevr/ironwail.cfg` and `quakevr/progs.dat` retain their original SHA256 hashes. Builds and runs use isolated output directories and a disposable base. Preliminary pilot/general-only captures are excluded from final measurements.

## ModelMetadata reuse

The new metadata is currently private to retro rendering. A shared immutable model-traits cache would be useful for the repeated flame checks in `vr_fireparticles.cpp` / `vr_haze.cpp`, model-specific emissive rules in `vr_emissive.cpp`, exact projectile/weapon identities and static physics facts. Keep retro's presentation category distinct from gameplay traits: exact, prefix and substring predicates currently differ between consumers. Entity classname, ownership, flags, scale and settings-dependent eligibility must remain live or follow existing generation/invalidation rules.

Physics sound material already gets stored when a Box3D slot is created; moving those comparisons is a lower performance priority. Parsing, debug commands and load-time bone/model validation also do not need broad replacement. A short `strcmp` is not automatically more expensive than a hash lookup: fetch traits once per entity/pass, reuse them, then profile. This change does not migrate unrelated model-name checks.

## Reproduce

```powershell
python Misc/quakevr/perf_suite.py --base build-cmake/perf-20261005/base --exe build-cmake/particle-opt-20261005/bin/ironwail.exe --output build-cmake/particle-repeat/results --scenes particles_retro_reference,particles_retro_fast,particles_retro_half,particles_retro_half_all --reps 3 --frames 1200 --eye 2048 --gpu 16
python Misc/quakevr/perf_analyze.py build-cmake/particle-repeat/results
python Misc/quakevr/particle_path_test.py --base build-cmake/perf-20261005/base --exe build-cmake/particle-opt-20261005/bin/ironwail.exe --output build-cmake/particle-repeat/visual --eye 1025
```

The visual helper needs NumPy and Pillow; it ran with the app's bundled Python runtime. Repeat timing at `--eye 3072` for the higher-resolution case. The measured executable is retained under `bench-bin`; a final rebuild only clarified menu help and cvar comments. Its particle vertex, fragment and shared retro GLSL literals are byte-identical to the measured build, and the final executable also passes the 14-fixture visual helper. Raw scripts, captures, images and logs remain under `build-cmake/particle-opt-20261005/`.

[Timing summary](benchmarks/20261005_particle_optimization.csv), [run hashes, full scope data and visual comparisons](benchmarks/20261005_particle_optimization_manifest.json).
