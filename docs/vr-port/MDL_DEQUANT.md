# .mdl vertex dequantisation (vr_mdl_dequant)

Backlog "Model vertex wobble", item (1). Code: `Quake/vr/vr_mdldequant.cpp` (its header comment is the design).

## Status (2026-10-15)

- [x] Load-time solver, offsets in the model's VBO (SSBO 11 `PoseFrac`), shader, cvar `vr_mdl_dequant` (default 1),
  menu row (Graphics > Models and Effects > Steady Model Vertices), Debug > Reports > Steady Vertices Check
  (`vr_mdl_dequant_check [model ...]`; one model: its runs too). Merged into `next` (1226e5357).
- [x] `vr_mdl_dequant 0` draws exactly as before (screenshots below), computes nothing at load.
- [x] Numbers (max offset, jitter, roughness, near-still wobble), load time (optimised to 114 ms for 139 models).
- [x] r_lerpmodels 3 / vr_anim_smooth_rig: draws and checks as before (pinned ogre, `vr_anim_smooth_check`).
- [ ] In the headset: the wobble on idle monsters and view weapons, gone? (see below)

## How

- Per vertex and axis, along each animation run, the path with the least squared second differences that stays within
  +/- half a byte step of every stored value (projected accelerated gradient, FISTA with adaptive restart; stops when no
  value moves 0.001 step). Still vertices are skipped.
- Runs: a frame group is a loop; single frames run on while their names' stems (name less trailing digits) match and
  their poses follow; split where a step between two frames is over 3x the median of the four steps round it. QVR's own
  monster models (quakevr/progs: soldier, dog, ogre, shambler, player...) have **empty frame names**, so for them the
  splitting by jumps does all the work (dog: 4 runs, shambler: 2). A run whose last pose is within 1.5x its median step
  of its first loops.
- Storage: 3 snorm8 per pose and vertex (1/254 step), in the model's data (`aliashdr_t::dequant`, computed on the first
  VBO upload with the cvar on: `dequantready`) and in the VBO after the poses (zeros while off). The shader adds them
  through a select (`frac == 0u ? pos : pos + ...`), so off is the bytes exactly. Toggling re-uploads every loaded alias
  model's VBO (`Mod_ReuploadAliasModels`).
- Normals: kept (the stored normal index per pose). Positions move by half a step at most, which turns no face
  noticeably; the normals' own 162-direction coarseness is a separate (shading) flicker.
- CPU readers: anchors (`vr_anchor.cpp`, your weapons' flashes/mounts/shells on vertices) follow the drawn positions
  (`mdldequant::poseVertex`; their vertex binding stays on the stored rest pose). Rig fits (r_lerpmodels 3 adds its
  swings to the dequantised poses on the GPU), hit zones, collisions, AO bakes keep the stored bytes: half a step is far
  inside their tolerances.
- Per-frame cost: none on the CPU; on the GPU one more uint read per pose read and one extra SSBO bind per alias batch.

## Numbers (e1m1, `vr_mdl_dequant_check`)

Max offset 0.500 steps on every model (the bound). Near-still wobble = RMS second difference (steps) on the axes a
vertex moves 3 steps or less along over a run (the rounding noise alone); jitter = mean |second difference| (units, all
motion); load = solver time.

| model | runs/loops | near-still wobble | jitter (units) | load |
|---|---|---|---|---|
| soldier | 6/3 | 1.362 -> 0.674 (-50%) | 3.88 -> 3.57 | 10.4 ms |
| knight | 4/0 | 0.819 -> 0.224 (-73%) | 3.87 -> 3.52 | 13.9 ms |
| ogre | 17/9 | 1.629 -> 0.764 (-53%) | 8.92 -> 8.10 | 9.0 ms |
| shambler | 2/0 | (no near-still axes) | 13.94 -> 13.33 | 16.9 ms |
| player | 10/1 | 2.206 -> 1.186 (-46%) | 7.39 -> 6.93 | 18.0 ms |
| v_shot | 3/0 | 0.888 -> 0.096 (-89%) | 0.51 -> 0.29 | 2.3 ms |
| v_nail | 1/1 | 3.651 -> 1.826 (-50%) | 1.32 -> 1.17 | 0.2 ms |
| dog | 4/0 | 0.535 -> 0.002 (-100%) | 6.90 -> 6.55 | 7.1 ms |
| demon | 8/2 | 1.530 -> 0.706 (-54%) | 5.56 -> 4.97 | 11.0 ms |
| zombie | 13/9 | 1.068 -> 0.408 (-62%) | 2.58 -> 2.31 | 14.8 ms |

Whole game start + e1m1: 139 models in 114 ms (first version: 356 ms; tolerance 2e-4 -> 1e-3 changed no figure above
in its third decimal). Memory: 4 bytes per pose and vertex in the model data (3.9 MB for 139 models; allocated also
when off) and 4 bytes per pose and VBO vertex on the GPU. v_axe is static (no offsets).

## Proof that 0 is the old look

Seeded scene (`vr_dither 0; sv_random_seed 7; vr_particle_seed 7; vr_fixed_frames 1`, e1m1, test ogre at 110 units,
`vr_anim_smooth_pin 41 42 0.5`). The base commit's own binary gives two images run to run (md5 355b30a9... and
b36b2b38..., 169 pixels apart by 1/255 on the ogre): builds of this branch with `vr_mdl_dequant 0` gave each of those
exact files (byte-identical PNGs) in different runs; the final build's runs land in the same 1/255 run-to-run noise
(97-376 pixels, never more than 1/255). With 1: 38679 pixels differ (up
to 70/255: sub-pixel shifts all over the ogre). r_lerpmodels 3 with the pin's four poses draws normally both ways.

## To try in the headset

Graphics > Models and Effects > Steady Model Vertices, on and off: watch an idle grunt's or knight's torso and head
(the breathing frames), a zombie, and the shotgun and nailgun in your hand: the surface ripple should be gone, while
the motion stays the same. With Model Animation on Smooth too.
