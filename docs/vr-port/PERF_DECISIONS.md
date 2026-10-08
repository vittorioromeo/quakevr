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

### 6. Decals on the world: the grid made twice in a frame

Marks placed during the left eye's scene (blood landing in the particles' step, gore::frame in decals::draw) come
after that eye's `decals::buildWorld`, so the right eye makes the grid again (the profile's "decals on the world"
under eye R: 0.03-0.07 ms a frame in `ai_crowd_64` before this follow-up's grid fix, about half that after). The right
eye shows the new marks a frame before the left.

- **Option**: one grid a frame: the left eye's kept for the right (new marks in both eyes from the next frame), or the
  marks' producers run before the build.
- **Win**: the second build, 0.02-0.04 ms a frame in fights (after the fix).
- **Drawback**: a new mark shows a frame later in the right eye (or earlier in the left): both eyes then agree.
- **Recommendation**: do it (the eyes agreeing is better than now); a visual timing change, so his call.

### 7. The decal grid's size from its occupied cells

The grid's buckets are twice the cells its marks list (a power of two): in fights 131072 buckets with 900-1100
occupied (`vr_decal_count`), 580-600 KB uploaded at every build, and the prefix pass walks every bucket (0.04 ms of a
0.14 ms build with 1024 large marks).

- **Option**: size it from the distinct cells (counted with the stamps), not every mark's cells.
- **Win**: most of the prefix pass and the upload (about 0.05 ms a build; less GPU upload).
- **Drawback**: fewer buckets, more cells sharing one: a bucket keeps its newest 64 marks, so a crowded one could drop
  marks the larger grid shows (a visual change where marks pile up; `decals_1024_stream` already has 52 capped).
- **Recommendation**: worth measuring the capped buckets with it before deciding; not urgent.

## Leads (2026-10-08 follow-up)

Done in the follow-up (BENCHMARKS.md, "Follow-up: the leads with no trade-off"): particle lighting on the pool's
threads (`combined` vr particles 0.79 -> 0.50 ms), the decal grid's buckets kept with each mark (a build 0.26 -> 0.14
ms with 1024 large marks), worldtrace::world's brush entities listed once a message (`explosions_storm` view entities
0.46 -> 0.33 ms). Measured and left as they are:

- **Framebuffer binds**: 88 a frame in `combined`, 12 redundant (8 of them each shadowed light's atlas bind,
  vr_lighting.cpp renderLight); 72 redundant binds more a frame changed nothing measurable (shadow maps 0.900 ->
  0.896 ms): the driver drops them. A bind cache is not worth its risk (state set behind its back).
- **weapons::modelTransform** (its map lookup shows under the shadow maps' alias draws in VTune): a last-model memo
  measured nothing (`combined` shadow maps 0.890 -> 0.893 ms, CPU p50 4.44 -> 4.44): not done.

Not done (no trade-off; for a next run):

- **The props' touch links**: Box3D's writeProp relinks each awake prop with its touches (SV_LinkEdict(ent, true) ->
  VR_TouchLinks -> SV_AreaEdicts): `SV_AreaEdictsR` 2% of `combined`'s samples (300 props awake in a small area:
  each walk tests the others' boxes, an edict's cache line each). A compact box array per area node would cut the
  misses, if kept exactly in step with absmin/absmax.
- **Shadow casters set up per light**: in `combined` the shadow maps' alias draws are 0.6 ms of the main thread; each
  caster's set-up (R_EntityMatrix's sines, the alias pre/post transforms, lerp) is made again for each of the 8
  shadowed lights; once a frame per entity would keep most of it.
- **NVML's start**: the first VRAM read (`gpustats::requestVram`) initialises NVML on a worker (0.12 s, once); fine
  as it is, noted because it shows in every window's VTune profile.
