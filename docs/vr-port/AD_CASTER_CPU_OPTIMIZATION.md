# Shadow caster CPU collection (2026-10-14)

Worker `adcpuopt`, baseline `origin/next` d986e7439. This narrows the AD caster finding in
MULTI_SCENARIO_PROFILE.md and changes only collection in `Quake/vr/vr_lighting.cpp`.

## Change

`addSurface` reserves the complete triangle fan once per polygon and writes contiguous indices.
It retains sky/water exclusions, polygons smaller than three edges, BSP surface traversal,
triangle winding and exact index order. The count is multiplied in `size_t` rather than signed int.

Brush entity candidates are collected in entity order into a local `SmallVector<entity_t*,64>`
for each `VR_RenderShadowMaps` invocation. More than 64 candidates spill normally; this is not a
limit. Portal cameras own separate lists. The list is passed explicitly to each ordinary and
virtual-light brush collection. It holds only entity pointers selected by model type: model,
world-model, msgtime, submodel/runtime-item, distance and scale checks still run for each light.
Collect-effect copies retain their original per-light loop. The candidate list is destroyed at
the end of each invocation; triangle indices still come from live surfaces on every collection.
Frames with no selected shadow lights skip the candidate scan. Alias collection and shadow
drawing are unchanged. There are no new cvars or changes to graphics defaults or budgets.

Four CPU scopes expose `dlight world casters`, `dlight brush casters`, `dlight alias casters`
and `shadow brush candidates` through the existing profiler/Tracy path.

## Exact regression

`Misc/quakevr/check_shadow_casters.py` extracts the actual current collectors into an engine-fixture
executable and compares them to `shadow_caster_reference.inc`, frozen from d986e7439. It uses the
production Zancle containers/static library. Build normal Release first, then run from a VS x64
Native Tools environment with LLVM clang-cl in PATH:

```
python Misc/quakevr/check_shadow_casters.py
```

The fixture covers ordered world triangles and brush identities/ranges, sky/water flags, negative
and small edge counts, large fans, capacity growth/nonempty prefixes, more than 64 candidates,
ordinary/item-only lights, collect-effect copies, live per-light origins/scales/angles/msgtime,
null/model replacement and entity-count/world-model changes between invocations. A nested-camera
list uses a different entity count before continuing the outer camera. It requires exact index
bytes and entity identity/order/ranges, rather than matching only counts or images.

## Measurements and runtime validation

The exact fixture passes **12,264 comparisons / 216,614,502 generated indices**. Normal Release
QC/FGD/statics/precedence/engine build, Release-Tracy build and normal e1m1 smoke pass.

The same native-mock lifecycle script runs against verified distinct saved baseline/optimized
binaries. All **18 atlas hashes** match exactly before/after, with zero texel differences in each
existing setup-cache comparison. Scenes cover a runtime brush created, moved and rotated; an
alias spawn; flashlight disappearance/reappearance; water; two selected virtual portal lights;
portal rebuild and map reload. The script uses `vr_flashlight 2;vr_flashlight_shadows 1` to ensure
active native dynamic-light coverage. The earlier unlit script is retained as a diagnostic and
is not the evidence for dynamic water/portal coverage. These readback checks are untimed.

AD measurements use the same private `profilemulti` AD steady/action script, fixed 0.004-second
frames, seed 7, 4x MSAA and a 960x540 flatscreen window. Each phase contains 600 frames; the action
phase has 2,353 dynamic-light collection calls in both builds. Release-Tracy has callstack depth 0,
QC/memory/GPU detail 0, `vr_profile 0`; both builds have the same new world/brush/alias subscopes.
Captures ran sequentially with exclusive kit slots and no concurrent builds/decoding. Both
captures are connected with zero dropped or force-closed zones. Counts, recorded settings and
final hash `d65a8a5097b35f67` match exactly. Steady has 206 live monsters; action ends at 212 from 214.

Milliseconds per frame, CPU inclusive costs for the listed scopes; GPU durations are individual
shadow-map timestamp intervals, not sums of nested scope durations:

| Scope | Steady before | Steady after | Action before | Action after |
|---|---:|---:|---:|---:|
| Dynamic world collection | 0.02550 | 0.02562 | 0.05493 | 0.05112 |
| Dynamic brush collection | 0.00819 | 0.00723 | 0.03184 | 0.02639 |
| Dynamic alias collection | 0.00090 | 0.00094 | 0.00388 | 0.00415 |
| Dynamic caster collection total | 0.03484 | 0.03401 | 0.09154 | 0.08265 |
| New brush candidate scan | — | 0.00174 | — | 0.00187 |
| Shadow selection incl. map casters/scan | 0.01318 | 0.01074 | 0.01526 | 0.01177 |
| Full shadow CPU | 0.29631 | 0.29068 | 0.18442 | 0.17140 |
| Shadow GPU | 0.04549 | 0.04572 | 0.05450 | 0.05448 |
| Whole-frame CPU busy | 1.897 | 1.954 | 1.846 | 1.767 |

Including the new scan, action caster collection saves 0.00702 ms/frame (7.7%); full shadow CPU
saves 0.01302 ms/frame (7.1%). This is one sequential pair. Steady world collection is flat and
steady whole-frame busy increases, so there is no claim of a general FPS improvement or universal
tail reduction. The measured scope shares justify the isolated brush/world changes; they do not
prove that caster work explains all AD stalls. Actual headset resolution and compositor costs
are outside this mock/flatscreen measurement.

## Short QC diagnostic: future work only

A separate 120-frame AD window enables QC function detail after loading/warmup and turns it off
again afterward. It finishes connected, exit 0, zero dropped/force-closed zones; 214 monsters and
619 physics bodies remain at both boundaries. It is a diagnostic with additional instrumentation,
not a before/after performance comparison. Main-thread exclusive/inclusive ms/frame:

| Function/builtin | Calls | Exclusive | Inclusive |
|---|---:|---:|---:|
| Generic `QVR` builtin | 39,177 | 0.08911 | 0.08957 |
| `VR_Liquids_Frame` | 120 | 0.01764 | 0.02926 |
| `VR_Melee_Sweep` | 240 | 0.01423 | 0.06309 |
| `VR_Wounds_Frame` | 120 | 0.01347 | 0.02123 |
| `findflags` | 47,308 | 0.01098 | 0.01098 |
| `VR_Melee_Track` | 240 | 0.00800 | 0.02647 |

The named liquids/wounds functions enumerate monsters/players each StartFrame, with each entity's
interval gate inside that scan (`QC/vr_liquids.qc:234`, `QC/vr_wounds.qc:167`). Measuring those scans
and due-entity work separately is now a concrete future target. Their interval/lifecycle/damage
semantics and callbacks must be preserved; no QC caching or reduced gameplay work is proposed here.
The generic `QVR` label aggregates custom builtin calls, so it does not identify one dispatcher or
native operation to optimize. Melee and server/physics code are unchanged by this task.

Raw scripts, baseline executable/source, before/after traces, decoder outputs and full console
logs are retained privately at `C:/OHWorkspace/qvr-kit/scratch/adcpuopt`. No id assets are committed.
Real-headset follow-up should use ordinary AD combat, moving items/lifts and flashlight shadows
through portal gates at the author's actual resolution.
