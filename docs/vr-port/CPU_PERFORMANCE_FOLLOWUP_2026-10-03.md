# Quake VR: CPU costs outside the physics, measured

Date: 2026-10-03. Follow-up to [PHYSICS_PERFORMANCE_RESULTS_2026-10-03.md](PHYSICS_PERFORMANCE_RESULTS_2026-10-03.md): the
other costs its profiles showed, each checked with a benchmark before deciding anything, and the ones worth it fixed.
Same headless setup (Linux, clang 20, mock headset, fixed 120 Hz clock, no drawing; `Misc/quakevr/physbench`), with three
new scenes: standing still on vrfiringrange and on e1m1, and a fight (13 monsters attacking a god-mode player, then
blown up). `physbench.py parse --profile` gives each window's profiler scopes (run with `+vr_profile 1
+vr_profile_interval 0`).

**What this setup cannot see:** the GPU. Mesa's software rasteriser (llvmpipe) does the GL work here on the CPU, so
anything inside the GL driver is left out of the verdicts below; it needs measuring in the headset.

## Summary

| Item | Verdict | Done |
|---|---|---|
| Force grab's search (QC, every frame, each open hand) | **Confirmed, the largest steady cost left** | `findcone` builtin: the server's physics −16..−20% standing still or by a pile; the same targets bit for bit |
| Models loaded in the first frames of every map | **Confirmed hitch**: the grenade pouch 2-21 ms, the torch flame 0.4-15 ms | made in the map's load instead |
| Ragdoll rigs at the map's start | **Confirmed load time**: 410 ms on the firing range (10 models in a row) | rigged in parallel: 160 ms (e1m1: 67 → 38); the same rigs |
| Wound texture re-made every frame | **Refuted**: made once; what showed was Mesa zero-filling its 17 MB on first use | — |
| Wound painting, 0.35 ms a frame in a fight | GPU work (90% inside the driver); the first paint is a 200 ms hitch here (llvmpipe compiling the shader) | check in the headset |
| Avatar arm IK, 0.05 ms every frame | Confirmed, small; no exact shortcut that fires | — |
| Hand-pose traces | 19% of a frame near a pile before the area-tree change, 0.02-0.03 ms now | — (done in the physics round) |
| Torch lights, model collide | 0.02 and 0.02-0.04 ms a frame | — |
| Particle spawn, 0.1 ms a frame standing still | **Transient**: first touches of the reserved particle pool; 0.02 ms after 30 s | — |
| Prop bursts | Realistic ones are small: 8 crates breaking 1.5 ms of server physics once, 3 grunts gibbed 0.4 ms | — |
| Monsters spawned in play (the firing range's dispensers) | **Confirmed hitch**: their heads and missiles precached late, 2-44 ms each (skins, normal maps) | not changed (see below) |

## The force grab's search

`VR_Forcegrab_FindTarget` runs every frame for each open hand. It walks everything `findradius` finds within 200 units
through QC, though nearly all of it is outside the 15 degree cone the hand points along. The physics round put the cone
test first; this round moves the walk out of QC.

`findcone(org, rad, dir, mincos)` (`vr_builtins.cpp`) returns `findradius`'s chain, in the same order (the last entity
first: the QC loop breaks score ties by it), less what has no model and what `modelcentre` puts outside a cone slightly
wider than the hand's (1e-4 on the cosine). QC tests each result again exactly as before, so the target cannot change.

Checked: both hands swept across a pile of 300 rocks (`vr_mock_hand`), 1866 searches choosing 103 different targets:
identical with the old search, bit for bit.

| Scene | Server physics, mean (before → after) |
|---|---|
| Firing range, standing still | 0.156 → 0.124 ms |
| 500 rocks lying in reach | 0.44 → 0.36 ms |
| 1000 pieces lying in reach | 0.59 → 0.49 ms |

## Hitches after a map's load

Every model loaded after the client has signed on was logged with its cost (a temporary build). On every map, two came
in the first frames after the loading screen:

- `progs/vrpouch.mdl` (the grenade pouch, `vr_handgrenade`): 2-21 ms. `view::prepareModels` makes the view's models in
  the load, but the pouch came after it was written.
- `progs/vrtorch_fire.mdl` (the wall torches' flame, made from id's `flame.mdl`): 0.4-15 ms, on maps with torches. It
  was made when first drawn.

Both are now made in the load (`VR_NewMap`'s prewarm: the view's models, and a new `walltorch::prepare` step). After
the change, nothing is loaded at a map's start.

## Ragdoll rigs

`syncEntities` rigs every ragdoll model the map precaches during the load (`SV_SpawnServer`'s two frames), one after
another on the main thread: 13-70 ms each, 410 ms for the firing range's ten models. The rigs are kept for the session,
so this is paid on the first map that uses each model.

`ragdoll::warmRigs` loads the models' data on the main thread, derives the rigs in parallel on the game's thread pool
(each derivation writes only its own rig, and its log, which the main thread prints afterwards in order), and keeps
them exactly as `rigFor` would. On 4 cores: the firing range's ready in 160 ms instead of 410, e1m1's in 38 instead of
67. Every rig's numbers are the same (`developer 1`'s "rigged:" lines). `developer 1` now also prints how long the
map's rigs took.

## Not changed

- **Wounds.** The texture array is made once and kept. The 4% that showed under `ensureTexture` is Mesa zero-filling
  its 17 MB the first time it is touched. The fight's 0.35 ms a frame of wound painting is about 90% inside the driver
  (llvmpipe rasterising the paints), so on a GPU it is draw calls. The first paint stalled 200 ms here while llvmpipe
  compiled the paint shader. Whether a real driver stalls on it too should be checked in the headset (the profiler's
  hitch log names it "wound paint"). If it does, one throwaway paint at map load would move the stall there.
- **Avatar arm IK** (0.05 ms a frame, always). About 240 evaluations of the wrist's cost a frame (two searches of the
  elbow's swing per arm). One exact shortcut was tried: skip the search over all swings when the nearby swing is good
  enough that the other could never win. It never fires, because a relaxed arm's elbow swing already costs more than
  the threshold. Faster options change the drawn arm slightly, for example the search over all swings every few
  frames.
- **The firing range's dispensers** precache only the monster's own model at the map's start (`func_enemy_dispenser`).
  Its head, missiles and sounds are precached when it first spawns: 15 ms for a vore's head, 44 ms for a knight's
  here. That is deliberate (the mission packs' files may be missing). If it matters, each second-row monster's other
  models could be precached at the start when they exist.
- **Particles.** The pool is reserved at start-up, and its pages are touched as it first grows: 0.1 ms a frame in the
  first seconds, 0.02 once warm. A few milliseconds a session in all.
- **Torch lights, model collide, hand traces.** 0.02-0.04 ms a frame each now.
- **Start-up.** Mesa compiling the engine's shaders took 3.1 s of the 4.1 s here. That is driver-specific; real
  drivers keep a shader cache.

## Where a frame goes now (standing still, no drawing)

| | e1m1 | firing range |
|---|---|---|
| Host frame (all the CPU here; the server every second frame) | 0.19 ms | 0.22 ms |
| View setup (avatar IK 0.05 of it) | 0.11 ms | 0.11 ms |
| Server (QC 0.04 of it) | 0.07 ms | 0.09 ms |

The CPU side of a frame with nothing happening is now about a fifth of a millisecond. What remains is the drawing (not
measured here) and, in busy scenes, the physics of piles (see the physics results).
