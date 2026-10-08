# Performance decisions (profiling run 2026-10-08)

The overnight profiling run's open questions: optimizations with a trade-off (a visual, behaviour, memory or default
change) and leads not done, each with what was measured. The uncontroversial fixes are committed (BENCHMARKS.md,
"Profiling run (2026-10-08)"). Earlier list: PROFILING_2026-10.md, "Decision list".

Setup: i9-13900K, RTX 4090, the author's settings of 2026-10-06 (`his_cfg_20261006_1237.cfg`, through
`bench.sh --settings`), mock eyes 2048 square, paced at 90 Hz, exclusive; CPU = `cpu_busy_ms` (the frame's work less
the runtime's waits), GPU = `gpu_3d_ms` (both eyes and the mirror). Medians of 3 unless said.

## Decisions

### 1. Slipgate recursion default (`vr_portals_recursion`, now 2)

`slipgate_loop_r0..r3` (vrslipgates' loop gate filling the view: it shows itself and T's north gate; ms a frame):

| recursion | CPU before the fix (3 runs) | CPU after (1 run) | GPU 3D after | views an eye |
|---|---|---|---|---|
| 0 | 1.70 | 1.18 | 2.07 | 1 |
| 1 | 2.89 | 1.77 | 3.29 | 1 + 2 |
| 2 (default) | 3.46 | 2.04 | 3.74 | 1 + 2 + 1 |
| 3 | 3.46 | 2.04 | 3.74 | (the fourth gate is out of range here) |

ROUND21's figure (2.15 -> 6.56 ms at recursion 0 -> 2, a busy machine) is now 1.18 -> 2.04 ms (the "after" column; one run each): most of a view's CPU
was the driver validating the view array's framebuffer for every view (fixed: commit "Slipgates: the view array's
framebuffer checked when the array is made"). What is left per view is the scene drawn again (R_RenderView: its
entities, alias models, shadow selection) and its GPU (about 0.4-0.8 ms a view at 2048, deeper views at half the
pixels a side).

- **Win of a lower default**: recursion 1 saves about 0.27 ms CPU and 0.45 ms GPU against 2, only where a
  gate is seen through a gate (vrslipgates' loop, the hub's gate rooms); 0 saves 0.86 / 1.67 ms.
- **Drawback**: a gate seen through a gate shows its shimmer instead of the room behind it (1: one level deeper drawn).
- **Recommendation**: keep 2. With the fix the worst case fits a 90 Hz frame with room to spare on this machine
  (CPU about 2 ms, GPU about 3.7); a "Slipgate Detail" choice in a low graphics preset could set 1.

### 2. Shadow maps drawn again for each slipgate view

`VR_RenderShadowMaps` chooses its lights for the camera; a portal camera's choice differs (lights near the gate's
exit), so the shadow atlas is rendered again whenever the camera switches between a portal view and an ordinary one:
with a gate in view, up to four times a frame (left eye's views, left eye, right eye's views, right eye) instead of
once. `slipgate_loop_r2`: shadow maps CPU 0.26-0.32 ms against 0.17 at recursion 0; GPU 0.53 against 0.23.

- **Option**: a second atlas for the portal cameras (kept for both eyes' views), or the portal cameras' lights chosen
  once a frame from the gates in view.
- **Win**: about 0.15 ms CPU and 0.3 ms GPU a frame with a gate in view (more on a map with many shadowed lights).
- **Drawback**: memory (a second atlas: vr_shadow_dlights' tiles again) or a shared choice that may differ from each
  view's own nearest lights (a light's shadow switching with the eye).
- **Recommendation**: worth it only if slipgate rooms with many shadowed lights show up in his maps; not urgent.

### 3. Explosion debris at rest still traced every frame

Each chunk sweeps seven line traces a step while it lives, also lying still on the floor (gravity, then the floor
stops it again). Before this run's trace fix the debris' traces were 15% of `ai_crowd_64`'s CPU in VTune; the fix
(commit "worldtrace::world: the brush entities' box test before their metadata lookup ...") took view entities from
0.50 to 0.35 ms a frame in `combat_48`; the traces themselves remain.

- **Option**: a chunk at rest (on a floor, speed under a few units a second for a few frames) sleeps until its life
  ends (no traces), as Box3D sleeps bodies.
- **Win**: most of the debris' remaining traces in storms (not measured; bounded by view entities' 0.35 ms).
- **Drawback**: a lift or door moving under a resting chunk leaves it floating (rare; chunks live 2-4 s).
- **Recommendation**: do it, with the wake on a brush entity within its box.

### 4. The enhanced AI's frame (`vr_ai_enhanced`)

`ai_crowd_64` (64 monsters awake, entities not drawn) against `ai_crowd_64_quakeai` (the same with Quake's AI):
CPU 2.57 against 1.76 ms before this run's fixes; the server's share 0.85 against 0.56 ms, the rest is the fight
going differently (more blood particles and debris: vr particles 0.41 against 0.25 ms, view entities 0.67 against
0.42). The stealth scopes themselves are small (STEALTH_PLAN.md: 0.02-0.06 ms a server frame). Not a decision: the
feature's cost, measured; nothing in it stood out in VTune (no stealth function in the top 30).

### 5. Retro particles' fill (still open: PROFILING_2026-10.md item 4)

`particles_dense`: GPU 5.7 ms, the VR particles 4.8 of it; `combined` GPU 7.4, particles 4.3. Nsight Systems on
`combined`: glDrawArrays (particles and full-screen passes) 62% of the GPU's busy time, the world's
glMultiDrawElementsIndirect 34%. A reduced-resolution path for large soft particles with the retro quantisation is
the one large GPU lever left; visual, his call.

## Leads not done (no trade-off; for a next run)

- **Particle lighting on the pool's threads**: `lightParticles` 0.4 ms a frame in `combined` (11000 particles x 21
  dynamic lights), once a frame. Split it: the lightmap reads in order (they share a trace budget, a cache and
  R_LightPoint's globals), then each particle's dynamic lights and retro levels in a `jobs::parallelFor` (pure per
  particle: identical results). About 0.3 ms off the main thread in fights with many lights.
- **The decal grid rebuilt whole** (`decals::buildWorld`) whenever a mark comes or goes: 0.2 ms a frame in a fight
  (`ai_crowd_64`, `combat_48`), mostly zero-filling and re-bucketing every mark. An incremental insert (the new mark's
  buckets only; a full rebuild when the pool wraps or the grid grows) would make it ~0.
- **Framebuffer binds**: Nsight Systems counts ~70 glBindFramebuffer a frame in `combined` (median 9 us each under
  the trace, p90 65 us); in fast mode the GPU is the limit, so part of it is the driver's back-pressure. Worth a look
  for binds of the same target twice in a row.
- **NVML's start**: the first VRAM read (`gpustats::requestVram`) initialises NVML on a worker (0.12 s, once); fine
  as it is, noted because it shows in every window's VTune profile.
</content>
</invoke>
