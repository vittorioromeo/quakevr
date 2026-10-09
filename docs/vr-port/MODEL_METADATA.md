# Shared model metadata

The model-only cache previously private to `vr_retro.cpp` now lives in
`Quake/vr/vr_modelmetadata.hpp` and `.cpp`. The shared service supplies exact identities, path traits,
retro classification, bone-name indices, packed body-part masks and packed right-side bone masks.
The C engine uses `vr_modelmetadata.h`, included by `vr_api.h`.

## API and ownership

```cpp
const auto& info = qvr::modelmeta::get(model);
if(info.is(qvr::modelmeta::Id::Flame2)) { /* frame-dependent flame behavior */ }
if(info.has(qvr::modelmeta::Trait::Rock)) { /* retain live settings checks */ }
const int chest = qvr::modelmeta::boneIndex(model, "chest");
```

`get`, `is`, `has`, `category`, `boneIndex`, `bodyParts` and `rightBones` are main-thread APIs.
`get` does not load model data. `boneIndex` and the skeletal masks load alias data when required and
copy derived facts; they retain no pointers into the engine's evictable alias cache.

`identifyPath` and `describePath` are pure, worker-safe APIs for hooks that run before a model exists.
Exact identity lookup uses an immutable compile-time hash table with string verification on collisions.
The 77 identities and 38 traits have a single definition in `vr_modelmetadata.inc`, shared by C and C++.
`path(Id)` supplies the loading/display path for built-in model tables.

Identities are exact and case-sensitive. `Id::Unknown` does not identify a custom model:
`info.is(Id::Unknown)` returns false. Custom models still receive applicable prefix, substring and
basename traits. Case-insensitive predicates have separate traits where the original code required them.
For example, lighting's `ContainsBody`, wounds' `Body` path prefix, and retro's `BodyFile` basename prefix
retain their different matching rules.

The cache uses stable individually owned entries in a dense map. Returned references survive other
insertions and erasures; map changes, game-directory changes and model-reload events release the cache.
`VR_ModelMetadataChanged(mod)` resets a model's copied facts before every actual model load, including
alias-cache eviction and synthetic model loading. This also handles queries made during loading.

Model type and flags are read live by `category`; notably, `EF_ROTATE` is not frozen in the name cache.
Entity ownership, scale, current animation frame, pose, skin, damage state and settings remain in consumers.
The bone map preserves both first- and last-match policies for duplicate names: AO uses the first,
avatar setup uses the last. Ragdoll workers use the pure `boneRole` helper while constructing their rigs;
physics subsequently reads the copied role instead of comparing names every step.

## Migration audit

Fixed model-name checks and model tables now use the shared identities/traits across:

- Fire emitters, flame haze, torch/projectile lights, emissive boosts and particle trails.
- Reflections, shadow casters, parallax, brush traces, spatial-audio brush tracking and ledges.
- Physics density/material rules, grenade handling, crates and placed debris.
- Weapon morphs, shell ejection, enemy muzzle anchors, chainsaws, grappling hooks and holsters.
- Ragdoll selection, monster model preprocessing, avatar bone lookup, AO and wound masks.
- Bullet-hole sprite recognition, menu model checks, known settings migrations and profiling counts.
- C model loading's normal-map item predicate and flame/fullbright model rules.

Remaining name comparisons serve different purposes and intentionally keep strings:

| Remaining comparisons | Reason |
|---|---|
| Weapon/prop slot IDs, retro overrides and model-name lists | User-editable mappings and patterns; existing settings-generation caches remain authoritative. |
| `Mod_FindName`, precache lookup, view-model lookup and dropped-model matching | Bind arbitrary requested paths, including custom models. Unknown built-in IDs cannot replace path identity. |
| World caches in water, detail, physics and ledges | Name plus geometry/generation guards detect reused model pointers. |
| Wounds' base-model/`#rag` relationship | A relationship between arbitrary paths, including custom models; body eligibility uses metadata. |
| Reload selectors and model/skin/animation file parsing | Interpret dynamic names, extensions, tokens and frame data. |
| Hand-rig joint validation | Validate input ordering against the rig specification before accepting it. |
| Texture, sound, classname and QuakeC identifiers | Separate namespaces; they are not model identity queries. |

Bounds, animation poses and geometry used by interactions remain live. Replacing those reads with
name classification would change behavior rather than remove repeated name checks.

## Validation

The exact task patch was built from a clean `70e369c3` source snapshot with separate intermediate files
and output. Concurrent allocator edits in the shared checkout were excluded from that snapshot and
from this task's commit. The clean executable SHA-256 is recorded in the validation artifacts.

- Release x64 build passed with warnings treated as errors.
- `modelmetadata_test.py` (removed 2026-10-09 with the perf_suite scripts it built on; git history): five fixtures, 17 sweeps, 1,002,084 metadata checks, 27,291 prop transform
  cases and 1,632 exact ordered force-grab comparisons. Fixtures cover mixed props, portals,
  hand transfers, active ragdolls, vanilla models, map transitions and explicit model reload.
- The engine's new `vr_modelmetadata_test` also checks null/unknown paths, custom prefixes,
  case sensitivity, synthetic suffixes, live type/flag changes, 512-entry cache growth and erasure,
  duplicate bone names, the first 48 packed bone slots, and per-model mask/bone invalidation.
- Fourteen stereo particle fixtures passed their existing reference/repeat-control comparisons,
  covering fire-related smoke, explosions, blood, sparks, dense particles and retro settings.
- All seven hand-transfer outcomes matched the pre-refactor executable, including their movement tolerances.
- The new metadata files pass `check_statics.py`. The whole-tree scan still reports the pre-existing
  `vr_decals.cpp` test command's local `serial`; it was present in the baseline.

Timings are regression samples, not a speedup claim. CPU/GPU timings varied substantially during
concurrent work; initial mixed-checkout runs were excluded from the final clean-build results.
Private configs were used throughout, with `-noconfigwrite`; this task did not install an executable
or modify the player's config or `progs.dat`.

Compact results were in `benchmarks/modelmetadata_20261005/` (removed 2026-10-06; git history). Raw build logs, configs, screenshots and
CSV captures remain under `build-cmake/modelmetadata-20261005/`.
