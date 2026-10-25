# .mdl vertex dequantisation (vr_mdl_dequant)

Backlog "Model vertex wobble", item (1). Code: `Quake/vr/vr_mdldequant.cpp` (its header comment is the design).

## Status (2026-10-15)

- [x] Load-time solver, offsets in the model's VBO (SSBO 11 `PoseFrac`), shader, cvar `vr_mdl_dequant` (default 1),
  menu row (Graphics > Models and Effects > Steady Model Vertices), Debug > Reports > Steady Vertices Check
  (`vr_mdl_dequant_check [model ...]`; one model: its runs too). Merged into `next` (1226e5357).
- [x] `vr_mdl_dequant 0` draws exactly as before (screenshots below), computes nothing at load.
- [x] Numbers (max offset, jitter, roughness, near-still wobble), load time (optimised to 114 ms for 139 models).
- [x] r_lerpmodels 3 / vr_anim_smooth_rig: draws and checks as before (pinned ogre, `vr_anim_smooth_check`).
- [x] His first look (2026-10-15): "Much better!", but an itch at the loop point (grunt, knight idle) and some
  pulsating left.
- [x] Loops from the progs, smoothed round the loop; normals smoothed within their table's step (below).
- [ ] In the headset: the idle grunt's and knight's loop point, and the shading's flicker, gone? (see below)
- [x] Item (2), rigged monsters rebuilt from the skeleton every frame: `vr_anim_smooth_rebuild` (default 1), below.
- [ ] In the headset: Rebuild From Skeleton on and off on an idle grunt and knight: less wobble, the same shape?

## How

- Per vertex and axis, along each animation run, the path with the least squared second differences that stays within
  +/- half a byte step of every stored value (projected accelerated gradient, FISTA with adaptive restart; stops when no
  value moves 0.001 step). Still vertices are skipped.
- Loops from the progs (`QcLoop`): nothing in a .mdl says which animations loop, and QVR's soldier, knight, dog and
  player have no frame names (knight: "1".."97"; shambler: "From ASE"), so the idle loop used to be merged into the next
  animation and smoothed with free ends: the seam got a kink (the itch). The server's QuakeC knows: its frame functions
  (`void() knight_stand9 = [$stand9, knight_stand1]`) compile to OP_STATE (frame, next function); following the chain from
  each function that a model's code names (any function naming the model's string immediate, `"progs/knight.mdl"`, e.g.
  monster_knight's `th_stand = knight_stand1`) comes round to the loop. Only string immediates count (not named
  variables, locals or the parameter/return globals: their live values are whatever they last held, and gave every model
  every loop). A loop goes to the models naming it at least half as often as the most (a spawner naming several
  monsters' loops once is outvoted; mon_soldier.mdl shares soldier.mdl's); overlapping loops of a model: the most named.
  Only consecutive frames (some rotation of f, f+1, ...), three or more, all single poses. Built once per progs (3-5 ms,
  575 loops for QVR's), on the first model dequantised with it loaded; a model dequantised with no server progs keeps
  the runs below. These runs are smoothed cyclically (the last pose into the first).
- Other runs: a frame group is a loop; single frames run on while their names' stems (name less trailing digits) match
  and their poses follow; split where a step between two frames is over 3x the median of the four steps round it. A run
  whose last pose is within 1.5x its median step of its first loops.
- Storage: 3 snorm8 per pose and vertex (1/254 step) and the normal, in the model's data (`aliashdr_t::dequant`, computed on the first
  VBO upload with the cvar on: `dequantready`) and in the VBO after the poses (zeros while off). The shader adds them
  through a select (`frac == 0u ? pos : pos + ...`), so off is the bytes exactly. Toggling re-uploads every loaded alias
  model's VBO (`Mod_ReuploadAliasModels`).
- Normals: a pose's normal is one of 162 table directions (14.5 degrees apart at the closest, up to 10.7 from the true
  one), so a near-still vertex flips between neighbours from frame to frame and its shading flickers: the pulsating. Along
  the same runs, each component takes the smoothest path within 0.1 of the stored direction's (the same solver,
  tolerance 1e-3), made unit again: at most 10.2 degrees from the stored one. Kept after the offsets (`dequant`: offsets,
  then normals packed as the VBO holds them) and written into the VBO's poses instead of the table's while the cvar is
  on (gl_mesh.c; off: the table's exactly).
- CPU readers: anchors (`vr_anchor.cpp`, your weapons' flashes/mounts/shells on vertices) follow the drawn positions
  (`mdldequant::poseVertex`; their vertex binding stays on the stored rest pose). Rig fits (r_lerpmodels 3 adds its
  swings to the dequantised poses on the GPU), hit zones, collisions, AO bakes keep the stored bytes: half a step is far
  inside their tolerances.
- Per-frame cost: none on the CPU; on the GPU one more uint read per pose read and one extra SSBO bind per alias batch.

## Numbers 2: loop seams and normals (e1m1, `vr_mdl_dequant_check`)

Second differences (steps, rms) on the progs loops' near-still axes (3 steps or less over the loop), at the seam (the
last pose and the first, played after it) and inside: the stored bytes, the runs found without the progs (the first
version) and now. Normals' turn: degrees rms from pose to pose on near-still vertices, stored -> now.

| model | runs/loops (progs) | loop seam: stored, before -> now | loop inside: stored, before -> now | near-still wobble | normals' turn (deg) |
|---|---|---|---|---|---|
| soldier | 9/4 (3) | 1.095, 1.871 -> 0.399 | 0.879, 0.681 -> 0.270 | 1.165 -> 0.495 | 2.09 -> 0.68 |
| knight | 7/3 (3) | 0.907, 1.246 -> 0.370 | 0.855, 0.405 -> 0.212 | 0.863 -> 0.254 | 7.47 -> 2.16 |
| ogre | 19/10 (3) | 1.394, 0.850 -> 0.498 | 1.151, 0.406 -> 0.416 | 1.668 -> 0.789 | 11.11 -> 4.67 |
| shambler | 5/3 (3) | 1.368, 1.680 -> 0.489 | 0.808, 0.399 -> 0.253 | 0.902 -> 0.295 | 7.34 -> 6.49 |
| player | 11/2 (1) | 1.168, 1.546 -> 0.189 | 0.924, 0.841 -> 0.157 | 1.778 -> 0.885 | 4.85 -> 1.07 |
| dog | 7/4 (3) | 1.032, 1.790 -> 0.315 | 0.839, 0.619 -> 0.242 | 0.981 -> 0.353 | 2.74 -> 0.80 |
| demon | 9/5 (4) | 1.657, 1.312 -> 0.731 | 1.275, 0.518 -> 0.540 | 1.534 -> 0.713 | 9.80 -> 4.39 |
| zombie | 13/9 (4) | 1.340, 0.570 -> 0.570 | 1.100, 0.424 -> 0.424 | 1.068 -> 0.408 | 6.09 -> 1.81 |

The idle loops he named (`vr_mdl_dequant_check progs/knight.mdl`: each loop's own line):

| loop | seam: stored, before -> now (max) | inside: stored, before -> now (max) |
|---|---|---|
| knight stand (poses 0..8) | 0.632, 1.256 -> **0.222** (3.67 -> 1.00) | 0.794, 0.440 -> 0.173 (max 1.00) |
| soldier stand (poses 0..7) | 0.896, 1.848 -> **0.225** (4.67 -> 1.00) | 0.736, 0.685 -> 0.127 (max 0.81) |

The seam is now as smooth as the inside (its largest kink equals the inside's largest); its rms stays a little above
the inside's because the idle's turnaround is at its first frame. Where an artist's loop itself jumps at its seam (knight
walk: stored seam 2.18 against 0.72 inside) the half-step box leaves part of it (1.05). Zombie and ogre have frame names:
their loops were found before, as now.

What the pulsating was not (measured): a third-difference objective gives the idle loops the same second differences
(prototype: knight seam 0.240 against 0.222, inside 0.172 against 0.173); linear blending between smoothed poses departs
from a smooth curve by about d2/8 = 0.02 steps (0.007 units on the knight). The normals were (above).

Load (`--exclusive`, game start + e1m1 + the check's models): 139 models in 202 ms (was 114: the normals add about as
many solves as the positions, 27 sweeps each); the progs' loops 3-5 ms. Memory: 8 bytes per pose and vertex in the
model data (7.6 MB for those 139).

## Numbers 1 (the first version)

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
With the loops and normals (2026-10-15): 0 gave md5 355b30a9... (the base's image) in one run and 429 pixels of 1/255
in another; 1 differs in 45778 pixels (up to 88/255: the shifts and the smoothed shading).

## Item (2): rigged monsters rebuilt from the skeleton (vr_anim_smooth_rebuild)

His words after the loops fix: "there still is a bit of wobble. Ideally this should be implemented in an efficient way
and be toggleable." `vr_anim_smooth_rebuild` (default 1; Graphics > Models and Effects > Rebuild From Skeleton;
Debug: Rebuild Fit Limit, `vr_anim_smooth_rebuild_error` 0.5 units). Code: `vr_animsmooth.cpp` (its header, "C").

- Every frame, with any Model Animation, at the poses too: a rigged monster's vertex is its bone's motion applied to
  one fixed place on the bone, so nothing of the per-frame rounding is left; only the fit error.
- Per model, once (rigData): the stored bone fits follow the rounding (a fit's turn noise times the limb's length), and
  rebuilding on them was worse than the dequantised poses (grunt: vertex most 0.093 against 0.059). So each bone is
  refitted to every pose as drawn (dequantised), two rounds of: places (each vertex's mean, over the poses its bone fits,
  of its place taken back by the fit), then fits (Gauss-Newton on the turn, vertices weighted by their share). A vertex's
  share is 1 within 0.5 units rms of its places, 0 at twice that (it keeps the frames' place: round the joints); a model
  with under half its vertices fully rebuilt isn't rebuilt (the shambler). Normals: the mean normal on the bone, turned.
  Made again when the error cvar or vr_mdl_dequant changes.
- Per instance: the refitted keys along the spline (r_lerpmodels 3) or turned linearly (Quake's blend; Stepped), 9 rows
  a bone, memoised as before; a bone fitted badly in a pose fades out towards it across the lerps (no pop).
- The shader (RigRebuild) mixes the spline's/blend's place (with B's swing) and the bone's by both shares.
- Loop seams: the poses' own, dequantised and smoothed round the loop (above); the refitted keys come from them.

Numbers (`scratch/rb_metrics.txt` in the worktree; `vr_anim_smooth_trace`, idle grunt and knight, second difference
of each drawn vertex, units): at the poses' rate (10 Hz) the worst vertex's p95 0.456 -> 0.314 (grunt; 0.272 with
Smooth), 1.071 -> 0.865 (knight); at 90 Hz the grunt's worst vertex 0.059 -> 0.032 (Quake's blend), 0.034 -> 0.020
(Smooth). The vertices' mean is the motion itself and doesn't change. CPU, 60 idle grunts: +0.07 ms a frame (about
1.1 us a monster); GPU: 9 rows a bone uploaded instead of 3.

## To try in the headset

Graphics > Models and Effects > Steady Model Vertices, on and off: watch an idle grunt's or knight's torso and head
(the breathing frames) through several loops: no itch where the loop starts again, and no flicker in their shading; a
zombie, and the shotgun and nailgun in your hand: the surface ripple should be gone, while the motion stays the same.
With Model Animation on Smooth too.
