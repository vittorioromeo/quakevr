# Quake VR performance review — 3 October 2026

Static review of `C:\OHWorkspace\quakevr-iw`, started at `9c77111f` (ragdoll render interpolation) and checked against `d57d397a` before delivery. Concurrent commits added two-handed ragdoll throw handling and a spectator reminder; their diffs were reviewed and relevant melee references updated. No engine/gameplay changes, game runs, or new benchmarks were performed by this review. References below are repository-relative paths and one-based line numbers in the delivery snapshot.

The strongest general opportunities are reducing headset pixel/sample work, reducing the cost of the world/model shaders, and avoiding repeated skeletal-model work across shadow views. Crowded combat additionally warrants measuring physics synchronization, precise collision, and QuakeC searches. There is also a particularly inexpensive, concrete QuakeC fix: pickup rotation uses a modulo implementation whose work increases with level time.

These are ranked hypotheses, not measured speedups. “High” means potentially substantial in the stated workload; it does not mean every map will benefit. Quality-setting reductions are diagnostic experiments and possible performance presets; the implementation suggestions seek to preserve quality where practical.

## First resolve the frame-pacing evidence

At 120 Hz the frame interval is 8.33 ms; at 144 Hz it is 6.94 ms. CPU and GPU execution overlap, so do not add their times to estimate the frame interval. Leave margin for variance and runtime/compositor work, and measure actual headset refresh, missed application frames, and reprojection alongside engine timers.

Existing captures provide a useful starting point, but predate this source revision and have no reliable association with its exact code/configuration. I computed frame-count-weighted averages from `quakevr/profile/systems_2026-09-30_11-28-21.csv`:

| Map | Frames | Frame period | CPU busy, excluding classified waits | Classified waits | GPU eye L + eye R | QuakeC CPU | Box3D CPU |
|---|---:|---:|---:|---:|---:|---:|---:|
| start | 4,242 | 8.409 ms | 0.621 ms | 7.788 ms | 2.060 ms | 0.039 ms | 0.014 ms |
| e2m1 | 61,701 | 8.889 ms | 0.839 ms | 8.050 ms | 2.158 ms | 0.066 ms | 0.055 ms |
| e2m2 | 2,076 | 9.688 ms | 0.841 ms | 8.847 ms | 2.196 ms | 0.071 ms | 0.029 ms |

The same capture attributes roughly 0.66–0.69 ms of GPU time per frame to post-processing and 0.56–0.62 ms to world rendering across these maps. These are descriptive historical values, not predictions for this checkout. The capture may include different gameplay phases; averages hide spikes. The system logger also excludes very long frames from some aggregates (`vr_profile_systems.cpp:1036`), so inspect its long-frame counters and hitch log.

`quakevr/profile/hitches_2026-09-30_11-28-15.csv` repeatedly records large `xr submit` / `xrWaitFrame` times. Earlier September 26 captures likewise have substantial submission waits. OpenGL timestamps surrounding runtime calls can include GPU idle time while the CPU blocks; they are not exclusively shader execution time. The profiler explicitly classifies these calls as waiting (`vr_profile_systems.cpp:61`, `vr_stereo.cpp:694`). Consequently, “10 ms GPU frame” alone does not establish a 10 ms rendering workload.

**Diagnostic priority before implementing a large optimization:** reproduce with the current build, record compositor/application timing, verify the selected OpenXR runtime and refresh rate, and compare runtime/streaming configurations. Check whether the problem follows headset transport/compositor load rather than scene load. CPU time inside `xrEndFrame` is a symptom to investigate, not work that can simply be deleted.

Code already disables desktop vsync when starting OpenXR (`vr_backend_openxr.cpp:161`). `host_maxfps 0` explicitly bypasses the desktop frame cap in active VR (`host.c:808`) and retains the 72 Hz renderer/network isolation (`host.c:118`). Compare it with the saved cap of 250 to exclude an unnecessary second pacing mechanism; do not assume the cap currently causes the observed waits. The server already runs separately from eye rendering (`host.c:1303`); increasing it to headset frequency would increase work and change simulation behavior.

## Ranked opportunities

| Rank | Opportunity | Expected effectiveness when applicable | Evidence / implementation confidence | Main cost or tradeoff |
|---:|---|---|---|---|
| 1 | Reduce scene resolution/sample cost; verify foveation | High, GPU-bound scenes | High mechanism confidence; existing controls | Image sharpness/edge quality |
| 2 | Reduce/specialize parallax and analytic AO | High, nearby detailed/occluder-heavy views | High code evidence; benefit needs measurement | Quality tuning or shader variants |
| 3 | Reduce full-screen post-processing, liquid copies, and empty OIT work | Medium–high, bandwidth-bound views | High pass evidence | Pass dependencies and depth correctness |
| 4 | Cull posed skeletal models and reuse bone uploads | Medium–high, bodies plus multiple shadowed lights | High: explicit culling bypass/repeated upload | Conservative animated bounds |
| 5 | Reuse/budget shadow updates and improve caster selection | Medium–high, dynamic-light-heavy combat | High repeated-work evidence | Shadow lag/invalidation complexity |
| 6 | Make `anglemod` constant-time | Small normally; potentially growing in long sessions | Very high; inexpensive | Floating-point edge behavior |
| 7 | Reduce awake physics population, particularly submerged props | Medium–high, debris/corpse piles in water | High: wet props disable sleep | Preserve buoyancy and interaction |
| 8 | Avoid scanning all edicts in Box3D synchronization | Medium, many entities but few moving bodies | High: several full scans | QC-driven state changes |
| 9 | Add acceleration for posed collision and ragdoll hits | Medium–high, close combat with dense meshes/corpses | High: triangle scans present | BVH refit and pose invalidation |
| 10 | Spatially index QC radius queries / gameplay candidates | Medium, many edicts and frequent interactions | High: `findradius` scans every edict | Preserve query order and special solids |
| 11 | Batch native melee work and eliminate QC array-helper traffic | Medium, CPU-bound melee/chainsaw workloads | High call-site evidence | Gameplay semantics/testing |
| 12 | Reduce dormant pickup think frequency / split visual animation | Medium, pickup-heavy maps | High: recurring thinks plus historical QC counts | Pickup shape/return timing |
| 13 | Retain more static shadow cache entries and bound cache misses | Medium for movement hitches; low steady state | High: cache tied to active slots | VRAM and cache policy |
| 14 | Per-effect particle visibility and density budgets | Medium, smoke/blood/explosion overdraw | High: aggregate visibility gate | Effects appearance |
| 15 | Share more stereo preparation; consider multiview only after profiling | Medium if CPU submission-bound; uncertain otherwise | Duplicate view passes confirmed | Large renderer/backend change |
| 16 | Pace spectator work and constrain background audio jobs | Conditional medium; small in historical mirror captures | Existing pacing/async code | Spectator/audio fidelity |
| 17 | Finish first-use shader/target prewarming | High for individual first-use hitches; little steady-state gain | Some lazy creation remains | Load time and memory |
| 18 | PGO and focused VM dispatch optimization | Low current priority; higher only if interpreter dominates | Interpreter verified; benefits speculative | Build complexity / compatibility |

### 1. Pixel/sample work is the broadest GPU lever

**Sources:** `Quake/vr/vr_stereo.cpp:646`, `Quake/vr/vr_foveated.cpp:270`, `Quake/gl_rmain.c:244`.

Both eyes run `V_RenderView`, bloom, and post-processing separately. The older e4m1 capture records 3292 × 3524 per eye: about 23.2 million scene pixels for two eyes before MSAA, overdraw, or auxiliary passes. The saved `ironwail.cfg` currently requests `vid_fsaa 4`, render scale 1, and aggressive fixed foveation. Saved settings are not proof of runtime settings.

Compare render scales 1.0, 0.9, and 0.8: the corresponding scene pixel areas are 100%, 81%, and 64%. Those are area reductions, not promised frame-time reductions. Compare MSAA 0/2/4 independently. MSAA increases attachment storage/traffic and resolve costs; it does not necessarily run the fragment shader four times. Keep the existing native-resolution UI path.

Verify that foveation reports supported and active. This implementation uses `GL_NV_shading_rate_image` and declines to run when `r_refdef.scale != 1`; setting a cvar alone does not prove shading-rate reduction. Its enablement follows the scene/OIT FBOs, so it does not reduce every post-processing or shadow pass. A scalable GPU-budget controller could adjust `vr_render_scale` with hysteresis, a minimum dwell time, and preallocated targets to avoid reallocating large FBOs continuously.

**Verify:** same recorded view, resolution × MSAA × foveation matrix; inspect separate GPU world/model, resolve, water, and post-process times and edge quality. A strong response to resolution indicates pixel/bandwidth pressure; little response directs attention elsewhere.

### 2. World/model shader cost: parallax and dynamic AO

**Sources:** `Quake/vr/vr_glsl.h:459` (`DynamicAO`), `:522` (`ParallaxUV`), `Quake/gl_shaders.h:676`, `Quake/vr/vr_ao.cpp:632`, `Quake/gl_shaders.c:344`.

Parallax walks a height-field ray, normally up to the configured 16 steps, plus refinement. There are already distance, projected-size, grazing-angle, and texture-crossing early outs; preserve them. Authored model heights can use the world's full step budget. Screen-filling hands, weapons, body, and close walls are worthwhile targets even when distant geometry is cheap.

Dynamic AO loops over tile/depth masks and evaluates ellipsoids or box form factors. Box faces involve multiple normalizations and edge terms per surviving occluder. The CPU already picks occluders once per frame and bins them per eye. Its separate screen-tile and global depth-slice masks are conservative; a true per-cluster mask can reject additional candidates at the cost of a larger buffer and binning work. Measure actual candidate counts before choosing that redesign.

Try parallax 16 → 8 steps, shorter distance, and separate authored/model/item disabling. Compare `vr_ao_dynamic 0` and `vr_ao_brush 0` separately; `vr_ao_models` controls baked vertex AO, not this per-fragment loop. Reduce AO reach and prioritize nearby substantial occluders over small debris. A few precompiled feature variants could remove inactive AO/parallax/light blocks and reduce register pressure; current world variants primarily cover OIT/dither/material mode. Avoid a combinatorial explosion or compiling variants during gameplay.

**Verify:** GPU world/model time versus close-wall views, dense crates, body visible/hidden, and authored-height weapons. Test combinations as well as isolated toggles because occupancy and shader interactions can be nonlinear.

### 3. Full-screen passes and liquid/OIT bandwidth

**Sources:** `Quake/vr/vr_stereo.cpp:722`, `Quake/vr/vr_bloom.cpp:224`, `Quake/gl_rmain.c:1906`, `:1931`, `:2040`, `Quake/vr/vr_water.cpp:298`, `:395`, `Quake/vr/vr_gfx_gl.cpp:1244`, `Quake/r_world.c:799`.

The scene goes through resolve/warp, bloom, final post-processing, and sometimes upscale. Bloom is already a quarter-size pyramid rather than a full-resolution blur. Investigate combining the scene-to-composite and final display transform when their inputs permit it, or fusing the final transform with the selected upscaler. Preserve tone mapping, gamma, underwater effects, and the UI ordering; FSR/NIS expect the current display-color placement. Removing all bloom will likely save less than eliminating an unnecessary native-resolution copy.

MSAA refraction resolves opaque color before liquids; the completed scene may need another resolve later. The half-resolution distance texture can be produced before opaque liquids and invalidated afterward. This is an intentional depth dependency, not two identical copies safe to remove blindly. Candidate improvements are conservative liquid screen rectangles, quality-adjustable refraction resolution, or a common depth resource with explicit production/consumer stages. Heat haze already limits its copy to covered bounds (`vr_haze.cpp:586`).

`R_BeginTranslucency` clears OIT attachments and `R_EndTranslucency` issues the resolve whenever OIT is effective, with no local “anything actually contributed” gate. Its resolve is already stencil-masked. Track pending contributors and skip the entire pair for empty views, or restrict clears/resolve to their conservative screen bounds. Compare sorted blending only as a visual/performance experiment; overlapping liquid correctness can change.

**Verify:** dry empty corridor, water across most of the eye, opaque versus transparent liquids, and underwater views; vary MSAA. Split the aggregated post-process scope into bloom/resolve/display/upscale and count pixels copied. In the September 30 capture the aggregate was roughly 0.67 ms, making this a better historical candidate than quiet-scene shadow-map rendering.

### 4. Posed models bypass culling and upload bones repeatedly

**Sources:** `Quake/r_alias.c:672`, `:399`, `:555`, `Quake/vr/vr_lighting.cpp:529`, `:581`.

An entity with `VR_AliasBonePoses` skips ordinary alias frustum culling. Shadow face selection includes every such entity via `posed[i] || ...`. Consequently, a posed body/ragdoll collected for a light can be submitted to each relevant cube face despite occupying a much smaller region. Ordinary animated models already have face culling.

Compute conservative current-pose bounds once, using per-bone influenced-vertex bounds or the available posed vertices, then use them for eye and shadow-face culling. Include network scaling, offsets, ragdoll interpolation, held-ragdoll prediction, and near-eye behavior. Never substitute the unposed model bounds: that is why this exception exists.

`R_FlushAliasInstances` uploads custom bone matrices each flush, and custom-bone entities deliberately break batching. Store bone palettes once per entity/pose version per frame in the existing upload ring and reuse their buffer ranges across eyes and shadow faces. This saves repeated CPU copies/bind work, not skinning executions by itself. If vertex shading dominates, benchmark skinning once into a reusable vertex buffer; Quake's small meshes may make the additional pass/barriers costlier than repeated skinning.

**Verify:** 0/4/8 ragdolls, avatar and hands, 1/4/8 shadowed point lights; count skeletal submissions and bone bytes per frame. Check extreme limb poses in each eye and cube face for clipping.

### 5. Dynamic shadow updates and caster collection

**Sources:** `Quake/vr/vr_lighting.cpp:970`, `:1005`, `:1153`, `:1180`, `:1194`, `:449`.

Shadow maps already render once per host frame for both eyes; static map-light world depth already has a cache. The remaining repeated work is dynamic-light world/brush/model collection and rendering, and moving-caster maps for selected map lights. Light size/selection hysteresis and view-facing cube-face rejection already exist.

Keep stable atlas placements, cache an unchanged light's static world contribution, and redraw moving-caster contributions when relevant pose/transform versions change. A stationary light over sleeping props should not require the same work as a moving flashlight or explosion. An update budget can prioritize the held flashlight, nearby changing casters, and strong new lights, updating distant/unchanged lights less often. Use bounded staleness and invalidation for moving doors, light movement, material alpha, and entity lifetime.

`viewHasCasters` produces a map-light face mask for shader lookups, but the surrounding loop still processes view-visible faces. After stable placement, empty moving-caster faces can skip rendering altogether once their old depth is cleared correctly. Brushes are also conservatively treated as present in all faces; tighter per-face bounds are another incremental change.

**Verify:** rockets/muzzle flashes around a debris pile; moving versus stationary flashlight; compare light counts, tile sizes, and filter levels separately. Shadow filtering is already optimized: the nominal 3×3/4×4 filters use four/nine bilinear comparison fetches (`vr_glsl.h:183`), so do not assume nine/sixteen independent fetches or propose an optimization already present. Historical quiet-scene shadow rendering was small; prioritize this only when combat measurements support it.

### 6. Constant-time `anglemod`: cheap, concrete, session-length dependent

**Sources:** `QC/ai.qc:36`, `QC/items.qc:1351`.

`anglemod` repeatedly subtracts/adds 360. Hanging object pickups call `anglemod(100 * time)` in `forcegrabbable_item_think`. After 30 minutes of level time, one such call performs approximately 500 loop iterations; many pickups repeat it frequently. This increases VM work as time grows even if the scene does not change. September 30 e2m1 QC instruction rankings include `anglemod` around 10–12% in several rows, although total QC time was small there.

Replace the loop with a constant-time float wrap such as `v - 360 * floor(v / 360)`, or an equivalent dedicated builtin, after checking numeric behavior. Test negative angles, exact multiples, values near 0/360, and large finite values. If using a native path, account for changed rounding relative to repeated subtraction. This is an unusually favorable effort-to-benefit fix even though it need not be the biggest immediate FPS improvement.

**Verify:** identical pickup-heavy scene at level times 0, 600, 1800, and 3600 seconds; compare total QC time and executed instruction counts without advancing all unrelated gameplay state accidentally.

### 7. Sleeping and awake-body population, especially in liquids

**Sources:** `Quake/vr/vr_box3d.cpp:4911`, `:4989`, `:9534`, `:9573`, `:722`.

Awake props undergo thrown-hit checks, contact queries, buoyancy/drag work, and the solver. On entering water, `b3Body_EnableSleep(s.body, !wet)` disables sleep, and buoyancy applies force with wake enabled. A large pile of submerged/floating debris can therefore remain expensive even after its visible movement becomes negligible.

Introduce a wet-rest state for stable buoyancy, or move only the tiny visual bob to rendering once the body is physically at rest. Wake it on grab, impact, changed support, or a meaningful external force. Limit simulation density for cosmetic debris and simplify distant settled corpse/prop representation while preserving nearby interactable objects. Ragdolls already have a configured cap (`vr_ragdoll_max`, saved value 8), so measure their actual population rather than proposing an absent cap.

Box3D already sleeps dry bodies, limits catch-up pieces to three, and has an adaptive multithreading threshold. Sweep substeps 4/2/1 and worker settings only after separating solver time from sync/collision callbacks. Fewer substeps or indiscriminate distance skipping can break fast throws, stacks, and standing on props.

**Verify:** same 100/300-prop pile on dry ground and in water; track awake bodies, contacts, solver versus water/hit time, and p99 step time. Check buoyancy, collision wakeups, stacked stability, and high-speed throws.

### 8. Physics synchronization still visits inactive entities

**Sources:** `Quake/vr/vr_box3d.cpp:4631`, `:4673`, `:1604`, `:4914`, `:9577`, `Quake/sv_phys.c:1307`.

Every server frame builds/zeros a carried array, scans edicts to classify/synchronize bodies, watches corpse state, scans slots for pre-step work, and scans them again for writeback. Sleep suppresses substantial inner work, but not all scans or classification. Cost scales with the edict/slot high-water mark, not just the awake set.

Maintain dense body-kind and active-body lists, use move/sleep events for writeback, and cache classifications until their inputs change. Start with writeback/pre-step lists, which are easier to make reliable than replacing QC entity classification. Arbitrary QC field assignments can change solidity, movement, model, health, ownership, or rigid state; a complete dirty-entity implementation needs VM/builtin integration or a conservative reconciliation scan. Keep a fallback for foreign progs.

Ragdoll writeback currently visits every ragdoll because an individual limb may remain awake. Track body move events or an aggregate ragdoll pose version rather than relying only on one body's sleep flag.

**Verify:** fixed small awake population while adding hundreds of dormant items/entities and increasing the edict high-water mark. Measure `box3d sync`, `box3d write`, and `box3d step` independently; spawning/removing/changing models must remain correct.

### 9. Accelerate posed mesh collision and ragdoll precise hits

**Sources:** `Quake/vr/vr_modelcollide.cpp:912`, `:717`, `:976`, `Quake/vr/vr_hitmodel.cpp:560`, `:668`.

Weapon model collision scans client entities for nearby bounds, poses candidate meshes lazily, and `gather` scans each candidate's triangles. The gathering is repeated during iterative push-out rounds. Existing chunk bounds help later ray tests, but do not avoid every triangle visit during gathering.

Use an entity spatial index and a refittable per-pose BVH or per-bone triangle groups. Reuse the same pose/bounds data across both hands and push-out rounds; query only triangles overlapping the shifted weapon bounds. The current pose cache already avoids some repeated vertex posing, so target the remaining gathering/traversal costs.

Normal precise-hit models already have a triangle hierarchy and pose bounds. The exception is a skinned ragdoll: `hitmodel.cpp:668` explicitly tests every triangle because animation-frame hierarchy bounds no longer apply. Refit that hierarchy for the current ragdoll pose, or use bone-local groups, then amortize it over shotgun pellets/melee rays. Rebuilding/refitting on every individual query can lose on small meshes.

**Verify:** shots/melee near multiple corpses, dual-handed contact, chainsaw sustained contact, and dense replacement models; count candidate entities, triangles gathered/tested, push-out rounds, and refit time. Ordinary Quake monster hits should not regress.

### 10. QC entity searches scale with all edicts

**Sources:** `Quake/pr_cmds.c:978`, `QC/vr_melee.qc:1621`, `QC/vr_chainsaw.qc:335`, `QC/vr_wpnforcegrab.qc:183`, `QC/vr_walltorch.qc:303`, `QC/vr_grenade.qc:717`.

`PF_findradius` scans every edict, skips free/SOLID_NOT entities, and performs a squared center-distance check. It already avoids square roots. Melee, force grab, grenade selection, torch contact, and chainsaw paths call it, sometimes multiple times in the same server frame.

Use a conservative spatial candidate query followed by the exact current test. The existing server area tree may help, but ensure it contains special touchable solids and accounts for bounding-box centers; otherwise maintain an interaction index. Preserve returned chain ordering and the `.chain` mutation semantics for existing QC. Another route is new VR-specific queries returning filtered, stable candidates, while leaving stock `findradius` untouched. Cache common nearby-player candidate sets within a simulation tick when mutations cannot invalidate them.

Use classname lists for frequently repeated gameplay `find` walks only after profiling actual call sites. Several apparent all-entity loops are developer tests, not normal gameplay; for example `VR_Burn_TestPieces` at `vr_burning.qc:1064` should not influence ordinary gameplay priorities.

**Verify:** radius-call counts and scanned-edict counts in melee/force-grab/flamethrower scenarios, with 100/500/1000 edicts. Validate SOLID_NOT_BUT_TOUCHABLE gibs, ordering-sensitive choices, and nested calls.

### 11. Batch melee computation before rewriting the interpreter

**Sources:** `QC/vr_melee.qc:182`, `:901`, `:1515`, `:1550`, `:1639`.

Melee reads previous/current point arrays, traces each moving point, searches candidates, and repeatedly performs segment/box/model tests in QC. In the historical start capture, `VR_Melee_Sweep`, `VR_Melee_Track`, and generated `ArrayGet*mh_cp` / `ArrayGet*mh_pp` helpers occupy meaningful shares of QC instruction counts. These shares are not function wall-clock profiles and do not include native builtin cost accurately.

A VR-specific native builtin can receive the sweep once, perform candidate gathering and geometry tests in batches, and return the same selected contact data. Even a smaller change that loads point/layout data once and eliminates compiler-generated array helper calls can help. Reuse transformed target data for multiple sample rays/pellets within a tick. Keep the existing stationary-point early out; further movement thresholds must not lose slow contact or ongoing-hit state.

**Verify:** identical recorded axe/fist/pommel/parry and chainsaw inputs; compare instruction counts, trace counts, self/builtin time, damage, selected contact, blocked-through-wall behavior, and haptics. Prefer this targeted approach over a VM-wide rewrite when only a few QC functions dominate.

### 12. Dormant pickups still execute frequent thinks

**Sources:** `QC/items.qc:1316`, `:1362`, `QC/weapons.qc:5212`.

Item thinks reschedule at `time + 0.02`, including carried-item early returns. Their shared logic checks water state, return behavior, sparkle emission, and hanging pickup rotation. Actual frequency is quantized by the server tick. Historical e2m1 instruction rankings often put `forcegrabbable_think_impl` / item think among the leading QC functions.

Split periodic maintenance from immediate interaction, stagger maintenance across pickups, and use longer sleeps for unchanged dry/resting items. Decouple visual rotation/sparkles where feasible. Rotation currently also informs the physical pickup shape, so purely client-side rotation is not a drop-in replacement. Process grabbing, return deadlines, deathmatch respawn, and water transitions promptly through events or explicit deadlines. Combining this with constant-time `anglemod` may remove the most obvious cost before scheduler changes are necessary.

**Verify:** many untouched pickups over a long level, carried/returned items, water entry, grab accuracy, and deathmatch respawn; measure thinks per server tick and absolute QC milliseconds.

### 13. Separate static shadow cache capacity from active light count

**Sources:** `Quake/vr/vr_lighting.cpp:801`, `:859`, `:881`, `:1111`.

The static cache has as many slots as wanted map lights. A light that fades out frees its slot and invalidates its cached world depth. Re-entering a recently visited room can therefore rebuild six static faces for a light that was already rendered earlier. Cache-size/quality changes can invalidate all slots.

Keep a larger LRU of static depth entries independent of the currently shaded map lights, retain stable tiles, and limit static-cache misses serviced per frame. Prewarm likely upcoming lights during spare GPU budget or loading. Cache immutable per-light world index lists too if traversal/index construction is significant. The current selection already uses PVS and hysteresis; this is extending cache lifetime, not introducing those features.

**Verify:** repeat room-boundary walks and rapid turns among more lights than active slots; compare first versus subsequent visits, static shadow cache hit rate, and frame-time spikes. The extra VRAM can be substantial at 1024-sized six-face blocks, so budget it explicitly.

### 14. Particle overdraw and coarse visibility

**Sources:** `Quake/vr/vr_particles.cpp:2169`, `:2297`, `:2340`, `Quake/vr/vr_decals.cpp:1247`.

Particle simulation/instance construction/upload already happen once per host frame. Settled decals already retain static GPU geometry, while changing ones are built once for both eyes. These optimizations should not be proposed again.

Particle drawing uses aggregate bounds to skip the batch and scene-distance work when everything is out of view. A wide aggregate box containing one visible puff can still admit many irrelevant instances. Add emitter/chunk-level conservative bounds, compact or GPU-cull the visible instances, and apply distance/projected-size density budgets for smoke/blood. If translucent fill dominates, experiment with lower-resolution soft-particle rendering and depth-aware compositing; the extra composite pass may not pay for small particle counts. Prefer simplification of cosmetic effects to gameplay projectile removal.

**Verify:** repeated explosions/smoke/blood with much of the population offscreen, close smoke filling the eyes, and low-effect baseline. Record visible versus submitted particles, fragment coverage, particle GPU time, and depth-distance pass cost.

### 15. Stereo preparation and multiview: conditional architectural work

**Sources:** `Quake/vr/vr_stereo.cpp:692`, `Quake/gl_rmain.c:1071`, `Quake/vr/vr_view.cpp:5676`, `Quake/vr/vr_backend_openxr.cpp:361`.

Each eye marks surfaces, sorts/draws entities, and computes view-dependent light clusters. Much higher-level work is already shared: VR view entities/IK are set up only for the first eye, shadow maps render once, particle simulation/upload is shared, and AO candidate selection is shared.

First separate reusable entity transforms/pose data/material state from per-eye globals and use a conservative union-eye candidate list with final per-eye culling. Cache PVS decompression only when the leaves/visibility mode match; eyes across a doorway or liquid boundary can need different visibility. AO bins and light clusters remain projection dependent.

A later single-pass stereo/multiview prototype would require suitable GL capability, layered targets, shader view indexing, and swapchain/backend changes: the current backend owns a swapchain per eye. It could reduce submission and geometry setup, but does not halve unique per-eye fragment work or remove per-eye post-processing. It is unlikely to outrank pixel/shader optimization on a GPU-bound configuration with low CPU busy time.

**Verify:** disable spectator, compare CPU scene setup/submission and geometry-bound scenes separately from fragment-heavy ones. Do not commit to this redesign until saved CPU/vertex cost is large enough to justify it.

### 16. Spectator workload and background audio competition

**Sources:** `Quake/vr/vr_stereo.cpp:539`, `:574`, `:789`, `Quake/vr/vr_audiosim.cpp:672`, `Quake/vr/vr_audio.cpp:885`, `Quake/vr/vr_box3d.cpp:694`.

A spectator camera renders another full scene and bloom, already rate-limited by `vr_spectator_rate` and drawn after headset submission. It can still occupy the same GPU queue and CPU between headset frames. Compare mirror/off against spectator 30/60 Hz, lower spectator scale, and spectator AA disabled. Historical mirror costs were small and saved configuration uses a mirror; do not claim the third scene is always active.

Audio direct-path/reflection simulation is already asynchronous and paced; mixing parallelizes through the same jobs infrastructure used by physics. If hitch traces show task waits, measure pool contention rather than moving already-async work off-thread again. Prioritize latency-critical physics tasks, cap reverb workers or job granularity, and consider parametric/lower-quality reverb under overload. Read the existing audio benchmark facilities before changing this subsystem.

**Verify:** loud crowded combat, maximum active voices and reverb, plus active physics pile; compare audio-enabled/disabled and reverb quality/interval while recording task waits, sound mixing, physics p99, and audible dropouts.

### 17. Finish first-use prewarming rather than blaming loading work

**Sources:** `Quake/vr/vr_main.cpp:1090`, `Quake/vr/vr_water.cpp:1566`, `:309`, `Quake/vr/vr_bloom.cpp:152`, `:229`.

The map load already prewarms decals, particles, detail textures, liquid volume/mesh, view models, torch, casings, and muzzle flash. Model AO baking and ragdoll rig preparation also have preload paths. Many expensive source loops belong to these preparation/test paths, not steady gameplay.

Some programs/targets still initialize lazily: the distance shader is compiled in `makeDistances`, bloom programs/targets on application, and related upscaling/foveation resources on use. If cold first-use captures show spikes, create the enabled quality configuration's shaders/targets during loading, including MSAA and non-MSAA variants likely to be used. Warm enough to verify a driver first-draw compilation hitch disappears; simply compiling a program need not exercise every driver specialization.

**Verify:** cold launch/map versus warmed repeat; first water/refraction, first smoke, first bloom/upscale, and first quality change. Separate map-load duration from gameplay p99. Avoid preloading every disabled feature indiscriminately.

### 18. VM dispatch and build optimization are later-stage work

**Sources:** `Quake/pr_exec.c:419`, `:445`, `:667`, `:680`, `Windows/VisualStudio/quakevr.props:47`, `Quake/gl_rmisc.c:1054`.

The QC interpreter is a switch loop with runaway accounting, optional tracing, call/return bookkeeping, and builtin dispatch. Candidates after profiling include a normal-mode fast loop, decoded operand pointers, superinstructions, or compiler-supported threaded dispatch. These require opcode-level evidence, compatible debug/error behavior, and representative mod testing. A JIT is an expensive architectural project; historical QC CPU times do not currently justify making it the first priority.

Release already enables speed optimization and ThinLTO. “Enable optimization/LTO” is therefore not a new proposal. PGO from representative combat, picking/throwing, and large-map workloads could improve hot CPU paths. Restrict float transformations to proven-safe code; blanket fast-math can alter physics, bounds, and exceptional-value behavior. Local squared-distance substitutions are reasonable only where profiling identifies a hot square root; `findradius` already uses squared distances.

Ironwail already uses persistent mapped frame upload buffers when supported. Before proposing a new upload ring, instrument `GL_AcquireFrameResources` (`gl_rmisc.c:995`) and fallback/reallocation events. It has a CPU fence wait on reused resources and a `glFinish` timeout fallback; ordinary `r_finish`/render-speed timings also intentionally synchronize. Do not remove correctness fences or attribute initialization-only `glGet*` calls to steady frame cost.

**Verify:** Release build, profiler off versus on, opcode/function counts and inclusive builtin/trace timing. Run engine/mod regression checks for any VM change. PGO benefit should be tested on captures not used to train it.

## Benchmark handoff

1. **Freeze the experiment.** Record commit, executable/build configuration, GPU/CPU, headset, refresh, OpenXR runtime, transport/streaming settings, actual eye dimensions, and effective cvars. The saved configuration and historical CSV headers differ in MSAA and feature values. Keep gameplay inputs/camera repeatable using existing motion takes where possible.
2. **Capture a current baseline.** Use `vr_profile_csv 1`, `vr_profile_gpu 4`, `vr_profile_hitch 1.5`, and `vr_profile_report 30`. Use `vr_profile 1` / `vr_profile_dump` when the call tree is needed. Start with `vr_profile_detail 0`; briefly use `vr_profile_detail 2` for trace/builtin attribution. Repeat with profiling disabled to quantify instrumentation effects. See `docs/vr-port/TESTING.md:729`.
3. **Separate work and pacing.** Report CPU busy, eye GPU execution, waits, compositor/app frame timing, missed frames/reprojection, and the frame-period distribution. Summed eye scopes are a practical first GPU measure; inspect other shared/window GPU work too. Don't use `xr submit` GPU scope as shader time. Obtain per-frame traces for p95/p99; a CSV of per-second averages cannot reconstruct those percentiles.
4. **Use multiple workloads.** Quiet dry corridor; close detailed wall/body/weapon; many dynamic lights; water filling the view; smoke/explosions; melee/chainsaw among monsters/corpses; many dormant pickups; large dry/wet physics piles; long-level-time pickups; room-boundary shadow-cache churn; spectator plus spatial audio. Include cold and warm runs.
5. **Make independent A/B changes.** Establish resolution/MSAA/foveation sensitivity first, then parallax/AO, pass costs, and combat shadow/collision/physics bottlenecks. Compare quality tweaks separately from implementation changes. Alternate run order and repeat enough to exclude clock/thermal/runtime variance; use a second set of scenes for validation.
6. **Use built-in physics tools.** `vr_physics_steptime` exposes game-world step distributions; `vr_physics_mtbench [bodies [steps]]` compares worker counts with body-state hashes. The synthetic benchmark does not cover all engine sync/QC/water costs. Existing `Misc/quakevr/box3dmt/physbench.sh` supports gameplay stress tests (`TESTING.md:1233`).
7. **Accept meaningful wins.** Report absolute milliseconds and p99/missed-frame improvements, not just percentage changes in a tiny scope. Validate visual quality and interaction correctness. Preserve both-eye visibility, animated bounds, precise hit selection, thrown-object collision, grab latency, liquid depth dependencies, and physics wake behavior.

Suggested first implementation sequence after the pacing check: fix `anglemod`; test resolution/MSAA and AO/parallax sensitivity; profile/fuse the expensive display passes; implement conservative posed-model bounds and bone-palette reuse; then optimize the CPU subsystem identified by crowded-combat captures. The ranking above is intended to change when new measurements identify the actual limiting workload.
