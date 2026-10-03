# Quake VR / Ironwail: physics scalability review

Date: 2026-10-03. Source inspection began at `eace5bf4`, with final revision verification at `055a772800acaa3ead0374c19990e54690169aee`. The Box3D integration, library implementation, and engine physics/entity code inspected below were unchanged. Concurrent edits cleaned up retired cvars/prop-setting registration and changed the vendored README; the active settings and model-pointer cache used in this review remained present. Source links were checked against the working checkout. This is a static review: no game runs, timing measurements, or implementation changes. Expected benefits are hypotheses for the benchmarking agent, not measured speedups.

Scope: `vr_box3d.cpp`, the vendored Box3D implementation and public API, physics integration in `sv_phys.c`, `world.c`, `vr_physics.cpp`, entity allocation/layout, prop settings lookup, and physics build/task configuration. The repository vendors Box3D 0.1.0 alpha; recommendations below use its actual `b3*` API, rather than assuming Box2D settings exist here.

## Main conclusion

Box3D already has most of the useful baseline settings enabled. The larger opportunities are in the integration: keeping water props awake, scanning wide entity/slot arrays, rewriting sleeping ragdolls, tracing moving props through Quake's collision system, and searching collision-event vectors repeatedly. Those costs can remain substantial even when Box3D itself steps a mostly sleeping world quickly.

For many props, distinguish three workloads:

* **Many settled props:** reduce integration scans, ragdoll publication, and work that prevents sleeping.
* **Many simultaneously moving props:** optimize collision geometry, unnecessary Quake traces/event processing, and solver substeps.
* **Large spawn/blast bursts:** reduce allocation, shape/body recreation, repeated mass calculation, and quadratic event processing. Tail latency matters more than average FPS here.

At 120 Hz the entire frame has 8.33 ms. Physics only gets part of that budget. There is no source-only basis for promising a particular prop count or percentage improvement.

## Ranked opportunities

Ranks prioritize likely practical benefit across those workloads. “High” describes conditional potential; it does not mean every scene benefits. Confidence applies to the observed code behavior, not its measured cost.

| Rank | Change | Expected effectiveness | Main workload | Confidence |
|---|---|---|---|---|
| 1 | Let settled water props sleep; avoid perpetual simulated bobbing | High where props accumulate in water | Water / settled scenes | High |
| 2 | Add dense physics lists and use Box3D body move events | High for large entity populations with few active props | Settled / sparse-active scenes | High |
| 3 | Skip sleeping ragdoll pose rebuilds and relinks | High with many corpses | Corpse-heavy scenes | High |
| 4 | Restrict enlarged Quake damage traces to props that need them | Medium–high with many moving props | Active piles / debris | High |
| 5 | Remove quadratic impact/shock searches | Medium–high during collision bursts | Collapse / explosion frames | High |
| 6 | Benchmark fewer substeps at high refresh rates | Potentially high solver saving, quality tradeoff | Many awake bodies | High |
| 7 | Reduce hull/compound/contact complexity for bulk props | Potentially high if narrow phase dominates | Dense active piles | Medium |
| 8 | Avoid rebuilding bodies for irrelevant model-frame/mass/role changes | Medium; potentially high for animated populations | Animated props / pickup bursts | High |
| 9 | Make begin/hit events selective; consolidate hit passes | Medium in contact-heavy scenes | Active piles | High |
| 10 | Size reserves from actual body/shape/contact high-water marks; batch shape mass updates | Medium for stalls; smaller steady-state gain | Spawn / pile bursts | High |
| 11 | Improve application hot-data locality without relocating Quake edicts | Medium after scan/list changes | Large prop populations | High |
| 12 | Cache inertia eigenvectors and reduce repeated body/contact getters | Small–medium, inexpensive to try | Throws / active props | High |
| 13 | Separate relinking/trigger checks from unchanged transform updates | Medium if linkage/touches dominate; semantic care needed | Slow-moving props / corpses | High |
| 14 | Tune workers by workload and hardware; stabilize worker-count transitions | Medium on suitable CPUs | Large active worlds / threshold crossings | High |
| 15 | Index exceptional collision-ignore pairs when their lists become large | Small normally; medium during gib/corpse bursts | Large ignore-pair populations | High |

### 1. Wet props are explicitly prevented from sleeping

Evidence: [water handling](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_box3d.cpp:5257). When a prop becomes wet, `b3Body_EnableSleep(s.body, !wet)` disables sleep. Every wet awake prop also receives a force with `wake=true` and velocity updates. Floating props receive a time-varying sine bob; sinking props also have sleep disabled, even though the bob only applies to floating props.

Consequences: props resting on an underwater floor remain active, and floating props never settle out of simulation. They keep paying solver, water sampling, writeback, collision linkage, and possibly audio costs. The effect can extend to connected contact islands.

Suggested change:

* First allow sinking props resting on underwater support to sleep.
* For floating props, define an equilibrium policy: low velocity/rotation, stable submergence, no recent interaction, and a sufficiently small buoyancy error for a dwell time. Preserve physical simulation during settling and interaction.
* Wake when an impulse/contact, changed support, teleport, liquid condition, or gameplay edit requires it. A sleeping float still needs a way to detect relevant liquid changes; simply skipping all future water checks is insufficient.
* If continual bobbing is desired, an optional visual bob for settled floats can avoid continual solver motion. That changes physical behavior and must be a quality option or a deliberate design choice.

Do not just re-enable sleep while retaining unconditional wake/velocity updates: that does not implement equilibrium sleeping. Validate floating props under a player's weight, moving platforms, stacked floats, grab/release, saves, and map liquids.

**Related API correctness issue to fix before comparing water behavior:** buoyancy is applied in `beforeStep(dt)` once, but a slow frame runs multiple `b3World_Step` calls. Box3D [clears force and torque after each call](C:/OHWorkspace/quakevr-iw/Quake/vr/external/box3d/src/solver.c:756). Thus the later calls in that frame do not receive the original buoyancy force. The integration already reapplies standing loads per call, but not buoyancy. See [the step loop](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_box3d.cpp:9823). Store the required buoyancy force and reapply it for each piece, or recompute the water force at each piece. Account separately for drag currently integrated over the full frame; do not apply full-frame drag repeatedly. This is a confirmed force-lifetime mismatch, with an unmeasured performance consequence from settling/jitter.

### 2. Replace population scans with dense lists and body move events

Evidence: [entity synchronization](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_box3d.cpp:4905), [corpse watcher](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_box3d.cpp:1624), [before-step pass](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_box3d.cpp:5185), and [writeback](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_box3d.cpp:9862). Sleeping props are generally excluded from expensive writeback, but the application still scans the population to discover that fact. The corpse watcher even walks and resets entries when both corpse physics and ragdolls are off.

Box3D provides [a contiguous body move-event array](C:/OHWorkspace/quakevr-iw/Quake/vr/external/box3d/include/box3d/types.h:1221), with transform, body ID, user data, and `fellAsleep`. The integration does not consume `b3World_GetBodyEvents`.

Suggested sequence:

1. Maintain dense lists of prop, kinematic, corpse/ragdoll, and currently awake physics entities. Keep stable edict numbers as identities and a reverse membership index for removal.
2. Use body events to drive writeback and sleep transitions, rather than querying each body's transform and awake state across the whole slot array.
3. Track physics-affecting QC edits through hooks/dirty flags where feasible; initially retain a compatibility synchronization scan for arbitrary QC field writes.
4. Gate corpse watching entirely when its features are disabled, including clearing old state once on a feature transition.

Move events need careful handling here: consume/copy them after **each** step piece, before any body destruction or next step invalidates them. Accumulate latest transforms and sleep transitions in application-owned scratch storage. `touches` and later QC callbacks can mutate the world. Preserve the current edict-order writeback/trigger behavior when needed by sorting the compact affected list. Track explicit API teleports, disabled/destroyed bodies, and QC edits separately; move events are not a general-purpose entity dirty notification.

One ragdoll has several bodies sharing an edict identity. A single sleeping limb must not mark the whole ragdoll asleep; use per-body state and a deduplicated affected-ragdoll list. Include generation validation so callbacks that free/reuse an edict do not act on a different entity.

### 3. Sleeping ragdolls still rebuild and publish every frame

Evidence: [unconditional ragdoll writeback](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_box3d.cpp:9865) calls [writeRagdoll](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_box3d.cpp:2442) every frame. It reads every limb transform/awake flag, computes bounds, derives the entity pose, relinks, checks held limbs, and publishes poses. It computes an aggregate `awake` result, but does not use prior sleeping state to skip this work.

Cache the final sleeping pose and bounds. Rebuild only when any limb moves or a relevant frame, cut, grab, teleport, scale, or gameplay state changes. Maintain limb-level wake tracking so a detached gun/head remains independent. Avoid removing any required renderer/network heartbeat: split pose computation from publication and publish the cached pose if protocol/interpolation timing requires it. A simple `if(s.asleep) return` would miss these cases.

This should be measured independently from Box3D solver time: it remains expensive after the solver has put corpses to sleep.

### 4. Every sufficiently moving prop gets an additional Quake collision trace

Evidence: [beforeStep](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_box3d.cpp:5185) invokes [touchNearby](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_box3d.cpp:5006) for moving props. The latter calls `SV_Move` with an enlarged damage hitbox and `MOVE_HITMODEL`. This is additional gameplay collision work alongside Box3D, including potential model-precise collision.

The trace intentionally catches damageable entities that a thin physical hull would miss, and preserves pre-bounce throw damage. Its purpose is valid. Its eligibility is broader than a hand throw alone: the `throwhit` field changes whether gibs are included, rather than whether tracing happens at all.

Introduce a cheap eligibility predicate based on whether this prop can actually produce the relevant gameplay damage/touch behavior at its current speed and state. Keep a dense eligible-projectile list. If there are no relevant targets in a conservative spatial query, skip the precise trace. Separate inflated damage sweeps from ordinary rigid-body motion; do not trace harmless slow debris automatically.

Do not remove physical collision or change damage thresholds without checking QC behavior. Grenades, buttons, breakable crates, deliberately enlarged throw hitboxes, and player-owned thrown weapons need coverage. Compare trace counts and hit-model time, not just Box3D's `collide` profile.

### 5. Collision-burst processing contains quadratic searches

Evidence: [impact deduplication](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_box3d.cpp:9891) searches the entire preceding prefix for each impact pair: worst-case O(K²) for K events. [Shock accumulation](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_box3d.cpp:5600) linearly searches accumulated shocks for each qualifying hit.

Use a reusable flat set of ordered `(edictA, edictB)` keys plus the existing vector to preserve the first occurrence and callback order. Do not silently canonicalize `(a,b)` to `(b,a)`; the existing code distinguishes ordered pairs. For shocks, a generation-stamped edict-to-shock-index array gives direct lookup while retaining the vector's first-seen order and maximum-speed replacement rule. Reserve both scratch structures for expected bursts.

Measure max input events, unique pairs, qualifying shock hits, and dedup time during explosions/collapses. This targets frame spikes that a solver-only benchmark misses. Preserve callback ordering, entity reuse validation, and pre-impact velocity restoration.

### 6. Four substeps may be more than necessary at 120+ Hz

Evidence: [defaults](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_cvars.inc:213) set `vr_box3d_substeps=4`, clamped to 1–8 in the step loop. At a real 120 Hz server step with one piece per frame, that means 480 solver substeps per second. The exact server cadence must be measured; headset refresh alone does not prove that cadence.

Benchmark 4 versus 2, then 1 as an aggressive quality tier. Fewer substeps primarily reduce constraint/integration work, not all collision preparation or application passes, so total physics speedup will be less than the substep ratio. Test stack penetration, tall piles, limb joints, fast throws, hand-held support, heavy/light mass ratios, and standing on props.

A high-refresh quality preset may support 2 substeps while slower steps retain more. Any adaptive policy needs bounded changes and testing under frame-time variation. A fixed physics cadence with visual interpolation is another, substantially larger design change: do not lower hand collision responsiveness just to cut solver frequency.

### 7. Use simpler collision proxies where detail does not earn its cost

Evidence: [prop shape creation](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_box3d.cpp:1471) uses cached drawn-model convex hulls with box fallback. Corpse/mover construction can involve multiple hull shapes; ragdolls multiply body/joint counts per entity.

Existing hull caching saves construction work, but repeated collision against a complicated hull still costs more than a suitable simple proxy. Add explicit physics proxies or a bounded hull-detail budget for bulk debris and ordinary crates. Share proxy geometry by model/scale. Keep detailed geometry for long/thin grab-sensitive tools and props whose shape matters to gameplay. Collision proxies need their mass, volume, and inertia reviewed; replacing a hull with a box can alter stacking and throws.

Profile `satCallCount`, `satCacheHitCount`, manifold counts, awake contacts, and constraint colors to determine whether geometry or contact density dominates. Reduce unnecessary compound children and surface detail before touching low-level solver code. Self/prop/corpse collision exclusions for purely decorative debris can save much more, but are deliberate gameplay quality changes, not free optimizations.

### 8. Body recreation can be triggered by rendering changes and role transitions

Evidence: [stale](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_box3d.cpp:3278) returns true for every prop/held/fixture model-frame change before the fast settings-generation test. [Synchronization](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_box3d.cpp:4948) destroys/recreates bodies for stale shapes or changed kinds.

For props whose physical collision shape does not change with the visual frame, key collision geometry independently from animation. This avoids repeatedly losing contacts and rebuilding bodies. For genuinely deforming collision geometry, retain correct updates or offer a coarser proxy policy.

For mass-only edits, investigate `b3Body_SetMassData` or shape density changes plus `b3Body_ApplyMassFromShapes`, with correct inertia scaling. For held/free transitions with identical geometry, investigate `b3Body_SetType`, shape filters/event flags, and velocity restoration instead of total destruction. `SetType` itself destroys/reorganizes contacts and broad-phase state in this version: it is not free and must be benchmarked. Scale/shape changes still require appropriate geometry updates. Preserve birth/grace bookkeeping, force state, collision masks, sleeping behavior, and QC side effects.

### 9. Contact begin events are requested more broadly than they are consumed

Evidence: all free prop shapes enable both begin/end contact events and hit events at [addPropShapes](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_box3d.cpp:1485). The [begin-event consumer](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_box3d.cpp:5532) only queues prop–mover and prop–actor pairs. The integration does not use end events here. Prop–world and prop–prop begin events therefore get generated and discarded.

The vendored engine enables contact events when **either** shape requests them: [contact creation](C:/OHWorkspace/quakevr-iw/Quake/vr/external/box3d/src/contact.c:248). Move begin-event opt-in to the mover/actor shapes that actually need these notifications, rather than all props, after auditing all consumers. This can retain prop–mover/actor notification while suppressing most pile/floor begin/end traffic.

Keep hit events where needed for sounds, prop–prop gameplay touch, and `.vr_impact`; requirements differ from begin events. Selectively suppress hit opt-in only when both endpoints' needs remain covered. Consolidate the three hit-array passes into one decoding pass if profiling shows a cost. Cache shape-to-entity/category resolution once per event.

`b3World_SetHitEventThreshold` is also available. Set it to the minimum threshold required by **all** consumers, converting Quake-unit thresholds correctly. Raising it merely to the breaking-prop threshold would suppress sound or prop–prop touch events. A higher threshold saves event generation, not physical contact solving.

### 10. Existing preallocation is useful but is based on initial edict count

Evidence: [world capacity](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_box3d.cpp:6119) reserves at least 256 dynamic/kinematic bodies, that count plus 256 shapes, and 4096 contacts. Later spawned piles and multi-limb ragdolls can exceed these limits. One edict is not one body/shape in this integration.

Use [b3World_GetMaxCapacity](C:/OHWorkspace/quakevr-iw/Quake/vr/external/box3d/include/box3d/box3d.h:231) from representative stress workloads to choose a reasonable reserve tier. Also reserve application slots, ragdoll records, impact/shock buffers, and scratch arrays. Avoid blindly allocating the absolute maximum for all users. These reserves reduce growth/copy spikes, not the ongoing cost of solving more bodies.

Shape creation uses the default `updateBodyMass=true`, and no batching override is present in `vr_box3d.cpp`. For a body with many hull shapes, each addition recomputes mass/extents over the growing shape list. Even [kinematic bodies traverse their shapes for extents](C:/OHWorkspace/quakevr-iw/Quake/vr/external/box3d/src/body.c:919). Create a batch with `updateBodyMass=false`, then call `b3Body_ApplyMassFromShapes` once before stepping. Box3D asserts if dirty mass remains. Also evaluate deferring contact creation during batch construction (`invokeContactCreation=false`) where the next step can safely discover pairs; confirm behavior for static/kinematic insertion and spawn-overlap ignore logic before using it universally.

### 11. The locality issue is predominantly application-side

Box3D already partitions bodies into [contiguous solver sets](C:/OHWorkspace/quakevr-iw/Quake/vr/external/box3d/src/solver_set.h:22), separates awake body states from sleeping sets, and has SIMD-friendly state storage. Its core should not be rewritten into a custom entity layout first.

The application uses [one large Slot record per edict number](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_box3d.cpp:389), containing shape metadata, cached QC state, motion, water/sound/throw state, corpse flags, and ragdoll bookkeeping. A vector is contiguous and avoids one heap allocation per prop, but scans stride across wide records and holes for nonphysics/free entities while accessing only a small portion. The actual record size and hardware cache behavior need measurement; no cache-miss rate is inferred from the source alone.

Quake edicts are also an array, not individually allocated nodes. Their stride is [QC-field-count dependent](C:/OHWorkspace/quakevr-iw/Quake/pr_edict.c:2242), so reading a few physics fields touches widely separated records. The array's high-water population includes holes. Traversing both arrays plus body/shape IDs and dynamic QC fields creates extra memory traffic.

Recommended storage direction:

* Keep Quake edicts and their stable addresses/VM offsets intact. Relocating them affects QC entity references, area links, networking, saves, and pointers held by callbacks.
* Add a dense physics sidecar, keyed by stable edict number plus generation. Keep an edict-to-row table for direct lookup; swap-remove rows and repair the reverse map.
* Start with a compact hot AoS: body ID, edict identity, kind, awake/wet/flight flags, and frequently used motion/gravity data. Store hull/settings/debug/corpse metadata in colder storage. SoA is useful only if measurements show field-only bulk passes; it is not automatically faster than a compact AoS here.
* Snapshot read-only callback inputs before the parallel step. Publish changes after tasks join; avoid unordered-map mutation or vector relocation from worker callbacks.
* Retain deliberate callback order even if the dense storage order changes.

The prop settings registry already uses [model-pointer caching and flat maps](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_props.cpp:78), and physics hulls are cached. Replacing those with another cache is lower priority than reducing whole-population scans. A list of edict pointers alone reduces iteration count but still dereferences wide QC records; it is an incremental improvement, not a complete hot-data solution.

### 12. Cache invariant math and request only the contact information needed

Evidence: [spinAlign](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_box3d.cpp:5124) retrieves local inertia and runs a Jacobi eigendecomposition during each active throw. Local principal axes/moments remain fixed until geometry or mass changes. Cache them when mass/shape data changes; recompute only then. Keep configurable alignment strength separate from the invariant inertia terms.

[Rolling resistance](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_box3d.cpp:5233) fetches up to eight `b3ContactData` records merely to test whether any touching contact exists. `b3Body_GetContactData(..., capacity=1)` suffices for that predicate in this version: [the implementation returns touching contacts only](C:/OHWorkspace/quakevr-iw/Quake/vr/external/box3d/src/body.c:478). Contact capacity alone is conservative and includes non-touching contacts, so it is not a replacement predicate.

Important distinction: `b3ContactData` contains pointers to manifolds; this API does **not** deep-copy every manifold. Savings here are from fewer contact-list visits and record writes, not a supposed giant manifold memcpy. Do not retain manifold pointers across stepping/destruction.

Support and scrape logic legitimately need contact details. Cache repeated mass/velocity/category getters within each pass where safe. Reuse a post-step contact snapshot for compatible consumers only if its lifetime and phase match; pre-step rolling contacts and post-step sound contacts cannot simply share yesterday's results. Fixed-capacity support/scrape arrays can truncate contacts in dense/compound scenes; check saturation instead of introducing a smaller cap as a performance trick.

### 13. Linkage and touch work remain outside the solver

Evidence: [writeProp](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_box3d.cpp:5479) calls `SV_LinkEdict(ent, true)` for every written prop. Ragdoll/corpse writes also relink. [VR_TouchLinks](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_physics.cpp:1716) already uses spatial area queries, then sorts/deduplicates candidates to preserve edict order. It does not do the old full entity-pair scan. Its scratch allocation is proportional to `num_edicts * 3`, even for small local candidate sets; this is a hunk scratch allocation, not a fresh OS heap allocation each time.

Split transform/bounds changes from touch eligibility and relevant gameplay changes. Avoid redundant unlink/relink and repeated bounds reconstruction when the spatial box is unchanged, while still delivering recurring trigger touches required by Quake. An unchanged position does not automatically permit skipping touch callbacks: triggers may move around the prop or implement repeated effects.

[solidBox](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_box3d.cpp:5411) reconstructs a rotation from Euler angles and transforms eight corners. An exact OBB AABB can instead use transformed local center and `abs(rotation) * localHalfExtents`. If using the Box3D quaternion directly, preserve the brush/model-axis conventions. This is a modest arithmetic optimization, best combined with avoiding unchanged bounds work.

### 14. Threading is already enabled and sensibly capped

Current defaults are `vr_box3d_threads=1`, `vr_box3d_threads_bodies=150`, and `vr_box3d_workers=4` including the main thread. [stepWorkers](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_box3d.cpp:747) already has a 75% return threshold to reduce toggling. Repository comments record prior benchmark results; this review did not reproduce them.

Benchmark 1/2/4 workers and the adaptive policy in full gameplay, not just the isolated solver. Box3D's [worker documentation](C:/OHWorkspace/quakevr-iw/Quake/vr/external/box3d/include/box3d/types.h:188) explicitly warns about efficiency cores, SMT, and cache topology. The shared pool and render/audio work can compete. A cap controls task concurrency, but does not guarantee which physical cores run those tasks. Investigate scheduling/topology only after observing a problem; avoid hardcoded affinity across different CPUs.

Awake body count is a cheap proxy, but contact/joint density can differ greatly at the same count. Consider a coarse hysteretic policy incorporating contact density if benchmarks show the current threshold chooses poorly.

Changing worker count is not free: [b3World_SetWorkerCount](C:/OHWorkspace/quakevr-iw/Quake/vr/external/box3d/src/physics_world.c:2326) destroys/recreates worker contexts. Count transitions can introduce allocation spikes despite the existing hysteresis. Measure transitions and consider a minimum dwell time or stable mode per workload. Do not rebuild worker contexts every frame or remove the current same-count fast path.

### 15. Exceptional pair lookups can become expensive in bursts

Evidence: [bornInside](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_box3d.cpp:3899) linearly searches the whole ignore-pair vector from collision filtering; [pruneInside](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_box3d.cpp:3879) recomputes body AABBs for each pair. Grace/reach and ragdoll-inside paths have related small-list lookups.

Keep vectors for normal tiny populations; a flat hash table can be worse for two hands and a few grace records. If telemetry shows hundreds/thousands of exceptional pairs, add a reserved pair membership index and cache each involved body's AABB once during pruning. Preserve birth/generation checks and thread-safe read-only lookup during Box3D tasks. This is distinct from impact deduplication, which is a stronger optimization because collision bursts already drive its vector size.

## Box3D settings/API checklist

| Setting/API | What the current source does | Recommendation |
|---|---|---|
| World sleeping | Explicitly enabled | Keep enabled; remove unnecessary application wake/disable paths. |
| Per-body sleep threshold | Library default 0.05 m/s; integration uses 0.15 for any prop with positive `.vr_prop_mass` | Make thresholds an explicit prop policy if needed. The current override is not restricted to small gibs despite its comment. Test useful motion/stack settling before raising it globally. |
| Continuous collision | World enabled | Keep for gameplay throws and VR interactions. |
| Bullet flag | Selected by motion > 0.35 of minimum extent per frame; toggled only on changes | Already selective. Profile bullet cost; consider bounded hysteresis and using actual step-piece duration where appropriate. Do not globally disable dynamic–dynamic CCD or label every prop a bullet. Angular motion and thin tools require tests. |
| Contact recycling | Enabled by default on bodies; nonzero world default | Already active. Measure `recycledContactCount / awakeContactCount` on matched scenes before tuning `b3World_SetContactRecycleDistance`. Larger tolerance can produce stale/ghost contact behavior. |
| Warm starting | Enabled by the library | Keep. The local API explicitly says disabling gives no performance gain and hurts stability. |
| Speculative contacts | Enabled by default | Keep for normal simulation; disabling is not a universal speed switch. |
| Restitution propagation | Default false | Already avoids that extra work. |
| Restitution iterations | Default 2 | A 2→1 experiment may save restitution work if its profile is material, but changes rebound/stack response. Zero is a behavior-changing tier. Lower priority than the ranked integration changes. |
| Contact tuning | Default stiffness/damping/pushout parameters | Tune only to address observed instability or to support a tested lower-substep preset. Changing hertz is not intrinsically a speed optimization. |
| Body events | API available, not consumed by integration | Strong missing integration feature; use with the lifetime/order rules above. |
| Capacity hints | Already configured, sized from starting edicts | Improve with `b3World_GetMaxCapacity`, including ragdoll/compound multiplicity. |
| Batch mass computation | Defaults used per created shape | Useful for multi-shape bodies; finish with `b3Body_ApplyMassFromShapes`. |
| Events / callbacks | All free props request events; preSolve/custom filter opt-in is more selective | Reduce event traffic. Keep expensive callback opt-in selective. |
| SIMD / optimized build | Native SSE2/NEON selected by `core.h`; Windows Release has speed optimization and ThinLTO | No missing AVX toggle found in this vendored implementation. Preserve precise floating point and no FMA contraction for determinism. Benchmark Release. |

### Two API details that should not become false findings

* `follow` calls `b3Body_SetTargetTransform(..., true)`, but [this vendored implementation](C:/OHWorkspace/quakevr-iw/Quake/vr/external/box3d/src/body.c:1302) leaves an already sleeping body asleep when the target motion is below its sleep threshold. It also does not reset an awake body's sleep timer merely because `wake=true` was passed. This is **not** evidence that stationary fixtures are forcibly awake every frame. Visually rotating fixtures really do have changing physical targets, however, and may keep neighboring props active.
* Quake's toss path [returns after VR_RigidToss](C:/OHWorkspace/quakevr-iw/Quake/sv_phys.c:1171), following regular QC thinking. It does not also run ordinary Quake gravity/movement for Box3D props. QC thinks, triggers, and damage sweeps still add separate cost.

### Contact recycling and dynamic preSolve decisions: validation concern

In this vendored version, [the recycling path](C:/OHWorkspace/quakevr-iw/Quake/vr/external/box3d/src/physics_world.c:653) can skip the normal contact update, which is where [preSolve is invoked](C:/OHWorkspace/quakevr-iw/Quake/vr/external/box3d/src/contact.c:699). The recycling flag is based on the bodies' recycling flags; it is not visibly excluded just because preSolve is enabled. The integration's preSolve answers depend on transient throw grace, contact orientation, velocity, and what a hand may support.

This is a correctness hypothesis requiring a focused stationary-contact test, not a proven gameplay defect or a reason to disable recycling globally. Verify grace expiry and hand/support state changes while relative motion is small. If a callback must be reevaluated every step, disable recycling selectively on the relevant hand/player/reach body, or provide explicit contact invalidation when its state changes. Preserve recycling on ordinary prop–prop/world contacts. Also verify that runtime material changes reach recycled contacts; the library comments explicitly say recycling skips material updates. A global larger recycle-distance experiment would make this concern more important.

## Benchmark handoff

Use the existing commands rather than inventing a solver-only test suite:

* [vr_physics_bigpile](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_box3d.cpp:6357) supports debris/rocks/bricks/crates/mixed and up to 2000 spawned props.
* [vr_physics_mtbench](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_box3d.cpp:6707) compares workers in an isolated deterministic cube world at 1/72 s. It is useful for scheduling, but does not cover complex hulls, water, entity scans, QC/trigger work, or 120 Hz gameplay.
* [vr_physics_steptime bins](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_box3d.cpp:6642) supplies timing distributions by awake count and accumulated Box3D profiling. Clear its window before each matched run.
* The existing hash, stack/pyramid, blast, sink, and shotbench commands can supplement interaction regressions.

Run matched 120/144 Hz cases with 100/500/1000 props where practical: scattered asleep, falling, collapsed/settled, repeatedly blasted, underwater on the floor, floating, animated pickups, and sleeping/active ragdolls. Include sparse worlds with many nonphysics/free edict slots. Vary unique models versus repeated models to separate construction/cache costs.

Measure full frame p50/p95/p99/max, total physics integration time, isolated `b3World_Step` time, sync/water/traces/writeback/trigger/audio/event time, awake bodies and contacts, shapes/joints, SAT/cache/recycling counters, bullet time/count, solver colors, worker transitions, event counts, and allocation growth. Add cache/branch hardware counters where available. Report actual server `dt`, step-piece count, substeps, and worker count.

The current label **“box3d step” is not just the solver**: it includes `notePushed`, standing forces, `limitPushes`, `touches`, and optional gib tracing. The library profile is a different measurement. The slow-frame log reads the last piece's library profile, whereas the accumulated profile sums pieces. Compare like-for-like and add a dedicated timer around `b3World_Step` when diagnosing costs.

Prioritized implementation experiments:

1. Independent low-risk changes: impact/shock indexing, cached inertia eigensystems, one-record rolling-contact predicate, disabled-feature corpse watcher gating.
2. Dense physics memberships plus event-driven writeback, preserving ordering and mutation lifetimes; sleeping ragdoll caching.
3. Water force lifetime fix, then equilibrium sleep policy; precise damage-sweep eligibility.
4. Event opt-in, reserves and batched shape construction.
5. Lower-substep and proxy-geometry quality tiers, worker policy tuning after full-game measurements.

Accept gains only when p95/p99 improve under the relevant workload and interaction behavior remains valid. Solver hashes help detect threading nondeterminism; behavior-changing substep/proxy experiments need gameplay and stability checks instead of expecting an unchanged hash.
