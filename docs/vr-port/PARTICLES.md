# Particle rendering: review and measurements (2026-10-22)

The author's question: is the particle renderer instanced, batched, cache-friendly, and what do modern games do
about many overlapping transparent particles (he remembered it GPU-bound by transparent overdraw)?

## What the renderer already does

`vr_particles.cpp` (simulation, records) and `vr_gfx_gl.cpp` (drawing):

| Technique | State | Where / cvar |
|---|---|---|
| Instancing | yes: one record a particle (`gfx::ParticleInstance`), uploaded once a frame, expanded to quads in the vertex shader; a draw per pass, not per particle | `buildInstances`, `gfx::drawParticles` |
| Batching | one batch for every view of the frame (both eyes, the spectator camera) | `batch` |
| Cache locality | the pool is a flat vector of POD particles, stepped in order; lighting on the pool's threads | `run`, `lightParticles` |
| Texture atlas | one 4 x 4 atlas of 128-texel cells (every particle type in one texture: no rebinds) | `ensureAtlas` |
| Quad trimming | opt-in tighter geometry round each cell's opaque part | `vr_particle_trim` (0) |
| Near-camera fade and cull | fade from 30 cm, none drawn within 10 cm | `vr_particle_near_fade`, `vr_particle_near_cull` |
| Screen-size cap | a particle at most 15% of the view's height across (shrunk round its centre) | `vr_particle_max_screen` |
| Soft particles | fade against the opaque scene's depth (no hard intersections) | `vr_soft_particles`... |
| Reduced-resolution offscreen buffer | from 1.5 views of large particles: all drawn at half size into an RGBA16F target, blended back with a depth-aware upsample | `vr_particle_halfres`, `_upsample`, `_dense` |
| Front-to-back "under" compositing | from 10 views of cover: drawn in reverse into their own target, saturated pixels skipped | `vr_particle_saturate` |

Not done: depth sorting (premultiplied blending of mostly-similar colours hides most order errors; a sort of ~3-4k
records a frame is cheap on the CPU but buys little visually), weighted blended OIT (would change the look of
additive fire and the dark smoke over it; nothing measured here pays for it).

## Measurements (mock headset, RTX 4090, `vr_frame_log`)

Close head pop: e1m1, a grunt at 34 units, the mock camera 0.5 m forward (`scratch/gh_closepop.sh`), quad fist pop
(`vr_decap_test 50`), GPU timed every frame (`vr_profile_gpu 1`), 2048 x 2048 an eye (`vr_render_scale 2`):

| | particles live | within 0.5 m of the eye | cover (views) | GPU frame | GPU particles |
|---|---|---|---|---|---|
| before the pop | 600-1150 | 0 | 0.2-0.4 | 3.6 ms | 0.01 ms |
| pop frame | 3700 | 1900-2170 | 5.1 | 5.0 ms | 0.22 ms (wound paint 0.6, models +0.2, world +0.2) |
| next second | 2700 | 1000-1400 | 3.3-4.0 | 3.9-4.2 ms | 0.20-0.25 ms |
| same, `vr_particle_halfres 0` | | | | 3.4-4.7 ms | 0.32-0.41 ms |

The pop frame's CPU: 2.9 ms (QuakeC 0.7, box3d 0.6, decal buffers upload 1.0; 0.6 ms a normal frame here).

Flamethrower held and firing at a wall (`scratch/gh_flame.sh`, 1024 an eye): new effects off 353 particles, GPU
particles 0.04 ms, frame CPU 1.30 / GPU 1.13 ms; embers, smoke, AD copies 2 and our rate 2 on: 468 particles, 0.07 ms,
CPU 1.16 / GPU 1.19 ms. Scorch marks (`vr_adgun_fx_scorch 1`) cost 6 ms of CPU a frame (the decals' rebuild per new
mark): off by default.

## Conclusions

- On this machine the particles are not the bottleneck even in the densest close-up case: the near fade, the size
  cap and the half-size path already hold a 5-view pop to ~0.2 ms (0.4 ms without half size). No larger rework
  (OIT, quarter resolution, re-batching) shows a measurable gain here, so none was built; each would change the look.
- The close pop's frame cost is mostly not particles: wound paint (0.6 ms GPU), the gibs' models, QuakeC and Box3D
  (0.7 + 0.6 ms CPU) and the decals' upload (1.0 ms). If the headset still drops a frame there, capture it with Debug
  > Profiling > Log Next 3 Seconds (`vr_frame_log`, written to `quakevr/profile/framelog_*.txt`) and punch.
- Leads, untried: a near-eye spawn budget (bursts spawned within ~40 cm thinned: 2000 particles there show as one red
  fog anyway); wound paint spread over two frames; the decal grid rebuilt incrementally (also what scorch marks need).
