# Animated positional damage view

Open **VR Settings > Advanced VR Options > Debug > Views > Show Hit Zones** and choose **Positional Damage** (`vr_debug_hitzones 1`). With precise hit detection enabled, the model's animated surface is colored red for head, green for body, yellow for extremities and blue for legs. **Hit Zones Through Walls** (`vr_debug_hitzones_xray 1`) makes it visible through the model and level geometry; default off shows visible surfaces.

The regions come from `PositionalHead` and the body's lateral threshold sent by QuakeC through `debughitzone`. Each rendered triangle is partitioned in exactly the same standing-pose coordinates as `hitmodel_rest`, then its clipped polygons are carried onto the current rendered triangle by barycentric interpolation. This follows frame blending, movement, rotation, network model transforms and animated corpses. The debug renderer uses the client renderer's triangles, rather than a server animation snapshot. Stale entities outside the current update are skipped.

`vr_hit_head_priority` determines who owns overlapping regions. There is no enlargement for melee tolerance: that decides whether a blow connects, while `vr_melee_positional` uses the contacted surface's ordinary positional region. `PositionalPointRegion` is the shared QC classifier for precise shots, positional melee, and the debug self-check. Turning positional damage or melee positional damage off still lets this view show the regions for tuning; those gameplay toggles continue to determine whether their multipliers apply.

The lateral and leg boundaries are clipped exactly. A head sphere intersects each triangle's standing plane as a circle, drawn with an inscribed 64-sided polygon; the maximum inward boundary approximation is under 0.02 Quake units for the supported heads. Every surface fragment is assigned once, including where the sphere lies entirely within a triangle.

**Decapitation** (`vr_debug_hitzones 2`) retains the older standing beheading reference zones, including magenta melee zones. **Both** (`3`) combines those with the animated positional surfaces. With precise hits disabled or a model unsupported by precise hit detection, positional reference wires describe the box/ray scheme instead. The visualization uses the existing local-server QC debug bridge.

## Validation

Release x64 build, QuakeC build (0 warnings), static-state check and diff whitespace check passed. Rendered mock checks covered standing animation, a nonfatal melee hit/pain pose, head priority on/off, xray, combined/decap-only views, precise-off reference fallback, disabling, an animated corpse and map reset.

`vr_hitzones_check` compares the last view's surface triangle centroids with the actual gameplay `PositionalPointRegion` QC function. It reports colored piece counts, samples, mismatches, sphere-boundary approximations, a hash of animated vertices and fallback models. The tested grunt produced 1,804 checked surface pieces with zero classification mismatches outside the documented sphere approximation; a live grunt plus corpse produced 3,118. The animation and pain hashes differed.

To repeat, use a disposable game base containing id1/hipnotic/rogue and this branch's quakevr assets and compiled progs.dat:

```powershell
Misc/quakevr/hitzones_review.ps1 -Base <isolated-base> -Exe <engine-executable>
python Misc/quakevr/check_hitzones_review.py <isolated-base>/hitzones.log
```

The review replaces the disposable base's quakevr/autoexec.cfg and writes screenshots and logs; config writes are disabled. Physical headset testing remains separate from the mock review.

The development game's executable was running during this change, so the validated build was written to `build-cmake/hitzones-bin/ironwail.exe`. A normal Release rebuild after closing that game updates the usual executable.
