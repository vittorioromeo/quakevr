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

- **Done differently (2026-10-08, the author's decision):** the chunks are the server's Box3D props now (ROUND21.md,
  "Explosion debris as Box3D bodies"): they sleep at rest, ride lifts and doors, later blasts throw them, and the
  client's traces are gone. explosions_storm: view entities 0.34 -> 0.15 ms, the server 0.23 -> 0.40 ms (Box3D's step
  0.017 -> 0.14 ms with 96 chunks in the air): about even in CPU; the cost moved to the server and the network (about
  26 B a frame a chunk in sight; `vr_explosion_debris_mp_max` 24 in multiplayer).
- **Future option:** a separate client-only Box3D world for purely visual physics (these chunks, shell casings, sparks,
  small gore), built from `cl.worldmodel` (its mesh shared read-only with the server's in single player), with kinematic
  proxies for the brush entities and the server's props near the view. No networking, no multiplayer cap, the same in
  single player and multiplayer; the price is a second broadphase and step on the client, and one-way contact with the
  real props.

### 4. The enhanced AI's frame (`vr_ai_enhanced`)

`ai_crowd_64` (64 monsters awake, entities not drawn) against `ai_crowd_64_quakeai` (the same with Quake's AI):
CPU 2.57 against 1.76 ms before this run's fixes; the server's share 0.85 against 0.56 ms, the rest is the fight
going differently (more blood particles and debris: vr particles 0.41 against 0.25 ms, view entities 0.67 against
0.42). The stealth scopes themselves are small (STEALTH_PLAN.md: 0.02-0.06 ms a server frame). Not a decision: the
feature's cost, measured; nothing in it stood out in VTune (no stealth function in the top 30).

### 5. Retro particles' fill (decided 2026-10-08: tried, left off)

**Decided** (the author: "try it; if the visual change is not very noticeable, then yes"): tried, and
`vr_particle_retro_halfres` stays 0 (BENCHMARKS.md, "Half-resolution retro particles"). The half path now blends its
particles in by depth (`vr_particle_halfres_upsample 1`: no fire bled onto an edge in front of it; also for the
non-retro half path, on by default) and leaves retro frames composited in reverse order at full resolution (in the
densest, half size cost 25 against 8 ms of GPU). The change is visible where it would pay: `explosions_storm` 7-12% of
the pixels off by more than 8 levels (the chunks' retro blocks and the square sparks go soft) for 0.5 ms of GPU; in
`combined` and `particles_dense` 0.2-1% of the pixels and no GPU saved. On in the menu (Half-Res Retro Particles) for
whoever prefers the frames.

Before:

`particles_dense`: GPU 5.7 ms, the VR particles 4.8 of it; `combined` GPU 7.4, particles 4.3. Nsight Systems on
`combined`: glDrawArrays (particles and full-screen passes) 62% of the GPU's busy time, the world's
glMultiDrawElementsIndirect 34%. A reduced-resolution path for large soft particles with the retro quantisation is
the one large GPU lever left; visual, his call.

### 6. Decals on the world: the grid made once a frame (decided and done 2026-10-08)

**Decided** (the author: "only one option, but both eyes should agree; if needed delay decals appearing by one frame
so that both eyes see it at the same time"): the grid is made in a frame's first view only (`VR_DecalsFrame`), and every
view of the frame draws with it; a mark made during the frame (blood landing in the left eye's particles, gore::frame
in its decals::draw) shows in both eyes together from the next frame. The other option (the marks' producers moved
before the build) is dropped; no setting. The marks drawn as triangles (`vr_decals_world 0`) likewise take a new mark
from the next frame's build, not the right eye's.

Before: marks placed during the left eye's scene came after that eye's `decals::buildWorld`, so the right eye made the
grid again and showed them a frame before the left (the profile's "decals on the world" under eye R: 0.03-0.07 ms a
frame in `ai_crowd_64` before this follow-up's grid fix, about half that after). Test: `vr_decal_eyes_test 90`
(Debug > Decals in Both Eyes) makes a mark between the eyes' views each frame and compares them: 0 frames different
(39 marks; 39 of 90 frames different before the first view claimed the frame). Builds in a 620-frame window (his
settings, unpaced): `ai_crowd_64` 498 -> 456, `gore_slash_32` 168 -> 148, `decals_1024_stream` 150 -> 150 (its marks
come between frames); at most one a frame now.

### 7. The decal grid's size from its occupied cells

The grid's buckets are twice the cells its marks list (a power of two): in fights 131072 buckets with 900-1100
occupied (`vr_decal_count`), 580-600 KB uploaded at every build, and the prefix pass walks every bucket (0.04 ms of a
0.14 ms build with 1024 large marks).

- **Option**: size it from the distinct cells (counted with the stamps), not every mark's cells.
- **Win**: most of the prefix pass and the upload (about 0.05 ms a build; less GPU upload).
- **Drawback**: fewer buckets, more cells sharing one: a bucket keeps its newest 64 marks, so a crowded one could drop
  marks the larger grid shows (a visual change where marks pile up; `decals_1024_stream` already has 52 capped).
- **Recommendation**: worth measuring the capped buckets with it before deciding; not urgent.

## Dawn of the Machine (MG3 M3-28, 2026-10-08)

BENCHMARKS.md, "Dawn of the Machine": map1, map2 and secret2 hold 90 Hz with their whole counts awake; the fixes with
no trade-off are committed. Measured and left:

### 8. The ragdoll cap with a whole map's deaths (`vr_ragdoll_max`, shipped 8)

`mg3_map2_kill` (218 deaths in a frame) at 8 and 32: CPU avg 3.28 against 3.52 ms, the falling bodies' worst frame
14.3 against 24.5 ms (paced, medians of 3); the deaths' frame itself the same (35 against 33 ms: the deaths, not the
ragdolls). A death past the cap retires the oldest ragdoll to a corpse, so the cap bounds the falling bodies' cost.
- **Recommendation**: keep 8 (the menu's slider goes to 16, 64 extended); more only for set pieces with few monsters.

### 9. Per-monster walks of every entity in QuakeC (done, exactly: the edict index)

secret2's fight, builtin time a frame (temporary timers, 2000 frames): `findflags` 0.19 ms, mostly
`VR_Stealth_LookAbout` (each Idle or Alert monster's FindTarget walks every entity twice: the wall torches, the
bodies); `find(p, classname, "player")` after the first player (`VR_EnemyShove_Target`, each stand/walk/run think,
~11 walks a frame; `VR_Burn_MapFrame` ~5); `VR_Grapple_WorldFrame` ("hook"), `VR_Burn_NailFrame` ("spike") and
`VR_MarksmanTest_Frame` ("ogre_grenade", a test's counter) one walk each a frame when there are none.
- The option weighed here (lists gathered once a frame, players walked as clients) was not exact. **Done instead
  (2026-10-08), exact**: the edict index (`vr_edictindex`, vr_edictindex.cpp; ROUND21.md, "QuakeC's scans through an
  index"). `find()` on .classname and `findflags()` on .flags' QuakeC bits, `wt_state`, `stl_notice`,
  `vr_letgo_fall`, `vr_throw_self` and `MG_registered` step through bitsets kept up to date by every store into those
  fields (OP_ADDRESS/OP_STOREP) and every engine change (free, clear, parse, Box3D's classnames); classnames whose
  text can change under the same string (temp, zoned, engine strings) are compared at the search, as before. No
  QuakeC change; `vr_edictindex_verify 1` walks too and counts differences: 0 in 839,217 searches of secret2's fight
  and in the stealth, map flame, reload, wall torch, enemy shove and grapple tests.
- **Win** (`profile_qc`, medians of 2, exclusive): secret2 awake `find` 0.167 -> 0.005 ms, `findflags` 0.157 ->
  0.019, QuakeC 1.09 -> 0.74 ms a frame (timer on); `combined` find+findflags 0.030 -> 0.006, QuakeC 0.49 -> 0.41;
  `combat_48`, `ai_crowd_64`, `explosions_storm` find+findflags 0.015-0.023 -> 0.004-0.007. Without the timer,
  secret2's server phase 4.44 -> 3.39 ms (medians of 4; the fight differs run to run, so this one is noisy: the walks
  also evicted ~230 KB of cache each).

### 10. The kill frame's spawns (decided 2026-10-08: optimise, identical output; done)

A death costs 0.15-0.2 ms on these maps (BENCHMARKS.md); in map2's kill-all frame the small gibs' spawns 2.1 ms,
`WeaponInst_Find` 1.4 ms (a weapon instance's id: every record walked, by `find`), the caps' walks (done: one walk
each). Vittorio's decision: optimise, with the output identical (the same entities, in the same order, the same
`random()` calls). **Done:**
- **Weapon ids without the walks** (vr_weaponinst.qc): ids are written only by `WeaponInst_Make` (from
  `WeaponInst_FreeUid`), so a bound above every record's id (`qvr_weaponinst_idtop`, `nosave`: worked out by one walk
  after a load or a map, then raised as ids are given) answers "is this id taken?" without walking for any id at or
  past it, which is every new weapon's. The same ids as before: a check run beside the old search (temporary, not
  kept) agreed on all 128 weapons of `mg3_map2_kill` and in a save, load and level change with weapons carried, held
  and holstered; no `random()` or entity change. `mg3_map2_kill`'s kill frame (`profile_qc`, its 9 frames):
  `WeaponInst_Find` 0.69 ms -> gone; the frame's QuakeC 22.7 -> 20.7 ms (one run each; the rest is run-to-run spread).
- **The small gibs' and limbs' `dprint(sprintf(...))`** with `developer 0` (item 12, done there): 1.2 ms of that frame
  was formatting text nobody printed (`VR_SmallGib_MakeRoom` 0.67, `VR_SmallGib_Roll` 0.42, `VR_Limb_MakeRoom` 0.12).
- **`ED_Alloc` needs no hint:** Ironwail takes a free edict from the head of its free list (`qcvm->free_edicts`), not
  by a walk from the clients up as this item first said; nothing to do.
- **`vr_bench_statehash` cannot judge a QuakeC change:** string fields hold offsets into the progs' strings, so a
  progs with one string more hashes differently from its first frame; and the kill frame differs run to run on one
  build (QuakeC's `random()` shares `rand()` with the client). Equality checks run beside the old code are the test.
- Left: `SUB_UseTargets` > `find` 0.46 ms of that frame (MG3's monsters' targets: `find` on .targetname walks every
  edict, ~6 us each; an index of .targetname would be the edict index's classname work again).

### 11. The force grab's search every frame

`findportalcone` (each hand's force-grab target, every frame): 0.07 ms a call on secret2 (1800 entities, each one's
model centre worked out before the portal broad phase). A box-based pre-test would need the centre's bound from the
box (not exact for models whose centre lies outside their box). Small; not done.

### 12. QuakeC left after the index (2026-10-08, `profile_qc` caller > builtin pairs, secret2 awake)

One loop moved to a builtin: the stealth AI's look about tested each lit torch's and body's distance and cone in
QuakeC (0.05 ms a frame on secret2 after the index); `findflagsinview` does it in the engine in QuakeC's own float
steps (`VR_Stealth_LookAbout` with its callees 0.062 -> 0.010 ms; ROUND21.md). What remains is engine work QuakeC asks
for, or behaviour-visible:
- `findportalcone` 0.13 ms (item 11: each model's centre before the broad phase; a bound needs the shape's fields,
  as dear to read as the centre).
- `findradius` 0.03 (`VR_Grenade_CatchCheck`), 0.02 (`VR_Stealth_Gather`): it writes `.chain` on every solid
  entity in range, monsters or not; an index of solids would need the engine's `solid` writes (10 sites, some
  temporary inside SV_PushMove) and would hold nearly every edict anyway.
- `sprintf` 0.034 ms, 21 calls a frame, in `VR_Prop_Flung`'s `dprint(sprintf(...))` for gibs touching monsters
  while still harmless: formatted with `developer 0`. A `developer` test round it saves it, but changes the temp
  strings' rotation (only code holding a temp string too long could see it). **Option**: guard the dprints (or a
  `dprintf` builtin that formats only with developer on).
- `traceline` (`point_visible`, `VR_Stealth_WalkNow`), `movetogoal` (`ai_run`, `VR_Stealth_WalkNow`): the engine's
  traces and steps.

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

Done in the second follow-up (BENCHMARKS.md, "Second follow-up"): the shadow casters set up once a pass over the
lights, the touch links' walk from arrays, the CRT boards culled by view, the gates' exits kept.

Still open (no trade-off unless said):

- **The props' touch links, further**: the walk now reads each node's edicts from an array (`SV_AreaEdictsUnordered`),
  but still reads each one's live `absmin`/`absmax` (`SV_AreaEdictsUnorderedR` ~2% of `combined`'s samples). A
  compact copy of the boxes made at link time would cut that, but is not exact: QC writes `absmin`/`absmax` without a
  relink (hipnotic's `hip_expl.qc`, `hip_subs.qc`; the engine's shot shift in `VR_BeforePlayerPostThink`), and such an
  edict would be found by its old box. A trade-off (touches by a stale box for one frame): not done.
- **The dynamic lights' world casters**: each shadowed dlight walks the BSP for the world's triangles near it every
  frame (`collectWorld`/`addSurface`, ~0.12 ms in `combined`, whose test lights stand still). Kept per light while its
  place, radius and the world are the same it would be the same list; most lights in play move (the flashlight,
  rockets, muzzle flashes), so the win is mostly the benchmark's. Not done.
- **NVML's start**: the first VRAM read (`gpustats::requestVram`) initialises NVML on a worker (0.12 s, once); fine
  as it is, noted because it shows in every window's VTune profile.
