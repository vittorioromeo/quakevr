# Quake VR: many particles and decals at once, measured

Date: 2026-10-03. Follow-up to [CPU_PERFORMANCE_FOLLOWUP_2026-10-03.md](CPU_PERFORMANCE_FOLLOWUP_2026-10-03.md), on slowdowns
with many particles or decals at once. Same headless setup (`Misc/quakevr/physbench`), with drawing on
(`+vr_mock_fast 1`, Mesa's llvmpipe) and two new scenes:

- `fx_particles`: an explosion every 4 server frames, with a puff of smoke, blood or big smoke 64 units in front of the
  eyes.
- `fx_decals`: ten shots at the floor every server frame and a blast every ten, which fills `vr_decal_max`.

Fragment counts came from a temporary build that wrapped the particle draw in `GL_SAMPLES_PASSED` and fragment-shader
invocation queries.

## Summary

| Item | Verdict | Done |
|---|---|---|
| Decals floating over parallax-mapped surfaces | **Confirmed** (the parallax sinks the texture up to 3 units; the meshes sat on the face) | Drawn in the world's shader, following the relief (`vr_decals_world`) |
| Decals: every new mark remade and re-uploaded all the settled marks | **Confirmed**: 94.6 MB uploaded for 853 chips (111 KB a mark, more as the buffer fills); a decal frame's worst 25-33 ms per eye here | Only new marks are added and uploaded: 0.8 MB; worst frame 1-10 ms |
| Particle simulation and records (CPU) | Small: a linear pass, 0.05 + 0.06 ms a frame in the storm | — |
| Particle fill (GPU) | **The cost**: large smoke and blood close to the eyes; 3500 particles drew about 5.8M fragments per eye at 160x160 (about 230 screens of overdraw) | In heavy frames all particles at half resolution (a quarter of the fragments); no per-sample shading with MSAA |
| Cropping the round particles' quads to their disc | Measured, **dropped**: no change in the fragments drawn (distant explosion particles cover less than a pixel each), and the quad's cut corners pick up the next cell's mip | — |

## Decals

`draw()` keeps the settled marks (shown, done spreading and darkening, not fading yet) in their own vertex buffer.
Before this change, the buffer was rebuilt and re-uploaded in full whenever a mark came, went, or began to settle or
fade. With the buffer full (`vr_decal_max` 1024) and marks still coming, that is every frame.

Now a mark's triangles are appended when it settles, and only those vertices are sent (`gfx::update`, a
`glBufferSubData` of that range). A mark that starts to fade, or is dropped, has its vertices collapsed to a point, so it
draws nothing. When more of the buffer is dropped marks than live ones (and at least 4096 vertices), it is packed and
sent whole once. Settled marks draw the same every frame (alpha 1, spreading and darkening done), and the modulate blend
(`dst * (src + 1 - a)`) does not depend on order, so the result looks the same.

Checked every frame in a test build, with short-lived decals and a small `vr_decal_max` (6000 frames, plus a build that
packs much more often, 22 packs):

- each settled mark's vertices equal a fresh `appendDecal`;
- the marks that should be settled are exactly the ones in the buffer;
- the GPU buffer's contents equal the CPU copy.

No mismatches.

| fx_decals (900 frames, decals at the cap) | before | after |
|---|---|---|
| Settled vertices uploaded | 94.6 MB | 0.8 MB |
| "decals" scope, left eye: mean / worst | 1.05 / 25.1 ms | 0.72 / 9.5 ms |
| "decals" scope, right eye: mean / worst | 0.76 / 32.7 ms | 0.63 / 1.1 ms |

(llvmpipe: the remaining time is its rasterising. On a GPU the saving is the per-mark upload and rebuild, which grows
with the number of decals.)

## Decals in the world's own shader (vr_decals_world)

Decals used to be meshes drawn over the world, 0.2 units off each face. The world's parallax occlusion mapping
(`vr_parallax`, `vr_parallax_depth` 3) sinks a texture's dark parts up to 3 units below the face without writing
depth, so in the headset the decals looked as if they floated: in stereo they sat in front of the surface you see,
and they slid over it as the head moved.

Engines handle this in a few ways: decals applied inside the surface's own shader (DOOM 2016's clustered forward
decals; Bevy's clustered decals), projected decals with parallax that writes depth (Unreal's pixel depth offset,
which Epic says conflicts with decals), or decals with their own parallax. The first is done here (`vr_decals_world`,
default 1):

- **Where:** in the world's fragment shader (`QVR_WORLD_FS_DECALS`, `QVR_DECAL_FUNCTIONS` in `vr_glsl.h`), after a
  held prop's blood and before the light. Each pixel finds the decals over it and multiplies its texture by them
  (texel x colour + 1 - alpha, the meshes' blend). It reads them where the parallax mapping moved its texture (the
  shift turned back into the world through the surface's screen derivatives), so they follow the relief. They take
  the light, the sheen and the fog as the texture does: the meshes darkened all three, fog included.
- **Finding them:** a grid over the world, 32 units a cell, hashed into buckets (twice as many as the cells listed),
  each bucket listing its marks once, at most 64 (the newest). It is built on the CPU only when marks come or go
  (`buildWorld`: 0.07 ms a frame on average with a new mark every frame and 850 of them) and uploaded as two storage
  buffers. Spreading, darkening, showing and fading are worked out in the shader from each mark's age.
- **The same marks:** the same faces as `clipToWorld` laid the meshes on (turned at most 60 degrees from the mark,
  their plane within its depth of its middle, the world only, not brush models). Unlike the meshes it has no limit
  of 32 triangles a mark, so a big spray over broken ground is whole.
- **Filtering:** the shader filters the atlas itself (the mip level of the footprint's narrow way, up to 4 reads
  along the long way). With `textureGrad`, llvmpipe drew a faint line along some rows of pixel quads.
- **Retro textures** (`vr_retro`, the Decals category): each decal is read through the decals' set as the meshes'
  shader read it (blocks, the palette, premultiplied), the world's own set kept for its maps read after. Retro
  lighting needs nothing: the decals are on the texture, under the light. Checked with retro textures, retro lighting,
  texture heights and parallax all on: the decals follow the relief, and with the parallax off they match the meshes
  (106 pixels changed by more than 32). `vr_decals_world 0` keeps the meshes.

Checked against the meshes, paused, the same frame both ways (512x512 an eye, pools, splotches, sprays and chips on
the firing range's floor). Quake's own textures get heights for the parallax mapping only when filtered smoothly
(`vr_texture_smooth 2`, or replacement textures): without that the parallax does nothing, so these match the meshes:

| | pixels changed by more than 32 |
|---|---|
| What the decals change at all | 13,300 |
| Meshes vs the shader | 22-78 |
| The same while fading (vr_decal_life 7) | 3 |

With `vr_texture_smooth 2` and `vr_parallax_depth 8`, at a grazing angle: the meshes stay where they are when the
parallax is turned on (the floor's texture moving under them), and the shader's decals move and bend with the floor's
relief. `vr_decal_count` says which way they are drawn, and whether the textures have heights.

Cost, `fx_decals` on llvmpipe (160x160 an eye, 850 marks): the meshes' draw (0.7 and 0.6 ms of CPU an eye) is gone,
and the world's surfaces took about the same GPU time (15.2 and 16.4 ms with the meshes, 16.8 and 16.7 ms with the
shader's decals). The frame went from 84.0 to 81.7 ms.

## Particles

The CPU side is not the problem: `run()` and `buildInstances()` are simple linear passes over the pool, a fraction of a
millisecond even in the storm. All particles are one instanced draw per eye.

The cost is fill. In `fx_particles`, about 3500 live particles drew about 5.8M fragments per eye at 160x160 (with 4x
MSAA, 23M samples). That is about 1650 fragments per particle, mostly the textured smoke and blood quads near the eyes,
each blended and, when soft, reading the scene's distances. The same explosions without the puffs in front of the eyes
drew about 250 fragments in all: Quake's own explosion particles, a few metres away, cover less than a pixel each. At a
headset's resolution the same overdraw costs more than 100 times as much, which fits slowdowns with heavy smoke near the
player.

## Particles at half resolution (added after: small visual changes are acceptable)

`vr_particle_halfres` (default 1): in a frame whose particles would cover more than one and a half views in all
(estimated on the CPU from each one's size and distance; off again under one view), every particle is drawn into a
target of half the scene's size and blended into the scene in one full-screen pass (`gfx::drawParticlesHalf`). That is
a quarter of their fragments, plus the blend pass and a clear of the half-size target. Lighter frames draw them at full
resolution as before. The decision is made once a frame, so both eyes always match.

- Depth: the half-size target has no depth buffer. A particle is hidden behind the scene's distances (the soft
  particles' texture, already half size, each texel the nearest of its four pixels), and soft ones fade against them
  as before.
- Order: all of them go to half size, in their order. Splitting them (the small ones at full resolution, over the
  large ones: `vr_particle_halfres_pixels`, for tests) reorders them visibly, e.g. a blood puff or debris over a new
  fireball.
- Off with retro textures (their texels' blocks stay sharp) and without a depth texture.
- Both particle passes, and the blend pass, no longer shade each sample with MSAA (`vid_fsaamode 1` made each
  particle fragment run once per sample, for nothing: their textures are soft and their edges are their alpha).

Checked with the game paused (the particles frozen) and the same frame shot both ways, at 512x512 an eye:
- the fireball, smoke and blood look the same;
- sparks and debris are slightly softer and a little dimmer;
- mean difference 1.2/255; 0.9% of the pixels differ by more than 32;
- the same with 4x MSAA in full-sample mode.

llvmpipe cannot show the saving: it rasterises the whole queued scene when the half-size target is bound (the time
moves to the "half size" profiler scope), and its blending is cheap next to a GPU's fill. In the headset, compare the
"vr particles" GPU scope with `vr_particle_halfres 0` and `1` in heavy smoke.

## Other options

Beyond the half-resolution pass, these would cut fill further, at the cost of how particles look:

- **Fade particles near the eye**: fade out a quad that comes within a few units of the eye, and collapse it in the
  vertex shader once fully faded, so it draws no fragments. Many engines do this; it also stops smoke from blinding
  the player. It could be a cvar with a small default.
- **Cap a particle's size on screen**: shrink, or fade, quads past some fraction of the view.

The disc crop was tried and dropped. It cut the round particles' quads to their visible disc, about a third of the
area. It saved nothing measurable: the particles it applies to are far away and tiny. It also changed 1-2% of the
particles' pixels slightly (the cut corners sample neighbouring cells in the atlas's smaller mips).
