# Single-pass stereo (`vr_stereo_single`)

Today every eye walks the scene and issues every draw call on its own (vr_stereo.cpp `VR_RenderView`: one
`V_RenderView` per eye). Single-pass stereo walks it once and has the GPU write both eyes from each draw. The goal is
the CPU's draw submission halved for what is ported (estimate before measuring: 1-1.5 ms a frame); the GPU's work
stays about the same. It is also the way standalone headsets render (multiview is standard on Quest).

## Mechanism: GL_OVR_multiview2

Two ways to have one draw write two layers of a texture array:

- **GL_OVR_multiview2** (chosen): a framebuffer whose attachments are 2 layers of an array
  (`glFramebufferTextureMultiviewOVR`); every draw runs its vertex shader once per view with `gl_ViewID_OVR`. No draw
  call changes, only the shaders (`layout(num_views = 2) in;` and their view's matrices). NVIDIA exposes it on desktop
  GL (his RTX 4090 and the headless mock run on the same driver; `vr_stereo_single_info` prints what the driver has).
- Instancing with `gl_Layer` from the vertex shader (ARB_shader_viewport_layer_array, also present): every draw's
  instance count doubled and every shader's instance index halved. The engine's draws are instanced and indirect (the
  world's are built on the GPU), so every draw path would change. Kept as the fallback for drivers without multiview,
  not written.

Every program a ported pass uses gets a **multiview twin**: the same sources compiled again with
`#extension GL_OVR_multiview2`, `layout(num_views = 2) in;` and `QVR_MULTIVIEW 1` (gl_shaders.c `GL_CreateShader`).
The twins of every ported program (59: the world's variants and the alias models') are begun when the toggle turns on
(or at the first frame when it is on at start, and again after the programs are made again: `GL_MultiviewPrepare`),
compiled by the driver's threads in the background (GL_KHR_parallel_shader_compile); the frames stay two-pass until
they are all linked (`GL_MultiviewPending`; `vr_stereo_single_info` says "multiview programs compiling"). A world twin
takes about a second to compile cold (the driver's shader cache makes the next start's fast), so on first use they
would hitch; without the extension they are compiled at once, waited for. Only a program made after that (a variant
first made mid-game) still gets its twin on first use. While the multiview pass
draws, `GL_UseProgram` swaps a program for its twin; anything else drawn there (a program without a twin) would be an
error, so the passes not ported are not drawn there at all (below).

## Shared per-view data

One uniform block (binding 5, `QvrStereoUBO`): both eyes' view-projection and position. Under `QVR_MULTIVIEW` the
frame data's and the alias instance buffer's `ViewProj` / `EyePos` (and the alias frame data's `FrameViewProj` /
`FrameEyePos`) are renamed in their blocks and become macros reading the view's entry (`QVR_VIEW`: `gl_ViewID_OVR`).
Without the define the shaders are the same text as before: the toggle off compiles and draws exactly what it did.

The clustered lights are per view (the clusters are screen tiles): the multiview pass clusters the lights twice, eye 0
into the usual 3D image and eye 1 into a second one (image unit 1), and the shaders load the view's
(`QVR_LOAD_CLUSTER`). The light list is the same for both (culled by the union frustum). Each eye's own view after
the pass keeps that list and its clusters (eye 1 reads its own image at unit 0) instead of clustering again, unless
something clustered in between (a serial in gl_rlight.c).

## Targets and the per-pass fallback

The multiview pass renders into a 2-layer colour array (the eyes' scene format) and a 2-layer depth/stencil array
(the eyes' depth format), at the eyes' render size. Each eye then continues as today with its own `V_RenderView`, into
its own layer: each layer has texture views (`glTextureView`: colour and depth/stencil, multisample with MSAA) with a
framebuffer each (the scene's, and the OIT's with the eyes' accum and revealage textures), and in the eye's
`R_SetupGL` they replace the eyes' scene target in `framebufs` (the scene's with MSAA or a scene effect, else the
composite's: `VR_StereoSingleEyeTargets`). The eye's scene is not cleared (`VR_StereoSingleSceneBegun`: cleared in
the pass, with its hidden area), and the passes the multiview pass drew are skipped. So every pass not ported yet
renders per eye as before, after the ported ones, into the same memory (no copy: the depth stays compressed), and the
image is complete whatever is ported. The window's mirror reads the eye's own targets. Caches keyed to the scene's
texture (the liquids' scene size, the haze's scene format, the half-size particles' completeness check) keep one per
eye, as each eye now has its own texture (else each view asks GL again: 0.1-0.3 ms of CPU each).

Order: the ported passes must come before the unported ones in `R_RenderScene`: the clear, the lenses' hidden area,
the opaque world and brush models, the opaque alias models. Everything after (particles, world text, sky, water,
translucency, heat haze, view model, debug draws) follows per eye.

The frame (vr_stereo.cpp `VR_RenderView`): both eyes' `V_SetupView` first (the second eye's after the first's, as
before; each eye's view kept), then the pass with eye 0's view current, then the eyes' loop as before, each eye's
`V_SetupView` skipped (done) and its view put back.

## Culling

The scene is walked once with a frustum that holds both eyes': the head's orientation, each side as wide as the
widest eye's edge, its apex moved back so that both eyes are inside it (conservative: never culls what either eye
sees). The PVS is both eyes' leaves' (OR'd when they differ). The world's GPU backface cull keeps a surface either eye
faces (a second view origin in its uniform block; the two-pass path passes the same origin twice).

Per eye still (drawn in each eye's own view): the view entities (hands, held weapons, the body) and `cl.viewent`,
whose near-plane handling (`VR_AliasNearEye`: depth clamp, both sides when an eye is inside) is decided per eye.

Two views in one draw also need, per view: the clustered lights (two cluster images); the dynamic AO's screen tiles
(vr_ao.cpp: both eyes' tiles OR'd in the pass, each view reads them at its own pixels); the foveation's rate image
(vr_foveated.cpp `beginSceneLayered`: a 2-layer rate image, one layer per eye; NVIDIA's shading-rate image follows the
view's layer, checked in the pixel diff with `vr_foveated` on).

With MSAA (`vid_fsaa`) the arrays are multisample arrays (GL_EXT_multiview_texture_multisample) and the layers' views
multisample textures.

Two-pass for the frame (`vr_stereo_single_info` says why): the driver lacks multiview (or MSAA arrays with MSAA); a
teleporter's view is in sight of either eye (below); `vr_stereo_single` 0. The spectator camera, the views through
teleporters, mirrors and the desktop mirror always render on their own.

Teleporters: each eye renders its views through the gates before its scene (`VR_RenderPortalForView`), and its scene
then hides what lies behind the gates it shows (the frame data's gates, `VR_PortalFrameData`). The pass runs before
any eye's gate views exist, so it drew what should be hidden (the pixel check: the floor through vrteleporters' middle
gate). While `portals::update` finds a candidate gate view for either eye, the frame is two-pass. To lift: both eyes'
gate views rendered before the pass (their textures per eye), the pass given the union of the gates shown.

## Stages

Each stage keeps the toggle's fallback: what is not ported renders per eye.

1. Infrastructure: arrays (MSAA too), the views' block, the twins, the toggle, the layers' texture views (was a
   copy), the per-layer hidden area,
   per-view clusters, AO tiles and foveation. Done.
2. World surfaces: the opaque world and brush models (solid and fences, the depth pre-pass, decals, permanent decals,
   parallax, lights). Done. Sky and liquids/teleporter surfaces: not yet (they read per-eye scene copies and the
   teleporters' views).
3. Alias models (smoothing, rebuild, dents, wounds, gloves): done, except the view entities (per eye).
4. Sprites, particles, blood streams, decals' own passes, beams, chains/coils, gfx tubes: not yet.
5. Translucency (OIT) and post (bloom, grade, tone curve per layer): not yet.

Per eye for good: the views through teleporters, mirrors, the desktop mirror and spectator camera, and whatever
differs per eye.

## What the first measurement says (2026-10-16)

Stages 1-3 make the images right but the frame slower (ROUND22.md, "Single-pass stereo"): draw calls -18%, but the CPU
+0.2 ms and the GPU +2.2 ms a frame in combat_48. Why, and what the next stages must do first:

- **Draw submission was not where the per-eye CPU goes.** The world is drawn from GPU-built indirect commands (its CPU
  0.02 ms an eye) and alias models are instanced (0.4 ms for both eyes, 0.34 with the pass). Halving them saves under
  0.1 ms. The per-eye CPU is the view set-up and the rest of the scene (particles, VR effects, view entities).
- **The pass adds a third view set-up** (`R_SetupView`: marks, light clusters, AO tiles, the frame data) on top of the
  two each eye still does for its own passes. Next: each eye's own view reuses the pass's (its marks hold both eyes'
  surfaces, its clusters are computed: bind eye 1's image at unit 0, upload the frame data only).
- **The copy costs far more GPU than it moves.** Copying the depth/stencil into each eye's target (glCopyImageSubData)
  loses the depth buffer's compression and hierarchical Z for everything drawn after it (the eyes' particles +0.4 ms,
  the eyes' other passes likewise). Next: no copy: the eyes' scene targets as views of the array layers
  (glTextureView of each layer, attached to the eye's composite framebuffers per eye), so the eyes render on into the
  same memory.
- Only then are stages 4-5 worth porting: their CPU (particles, VR effects) is where single-pass can save.
- The twins compile on first use (a hitch the first time each program is drawn in the pass): compile the ported
  programs' twins when the toggle turns on.

## The second run (2026-10-16): no copy, setup reused, twins ahead

Done: the eyes render on into their layers (texture views, no copy), the per-texture caches per eye, the eyes reuse
the pass's lights and clusters (marks not: 0.002 ms an eye), the twins compiled in the background at toggle-on.
Images: toggle off as before (0.001% at most against the old build); toggle on identical to off (MSAA 4 with
`vr_foveated 2`, and no MSAA without foveation: 0.002% at most). Not identical: no MSAA with `vr_foveated` on (his
settings): 0.2-0.8% of the pixels in the foveated rings differ, in the old copy build too (pre-existing, not found).

Bench (Release, 900 frames paced at 90 Hz, exclusive; the machine was shared with other workers' builds, so the
quietest runs; ms):

| | CPU p50 / p95 / p99 | GPU 3D p50 / p95 / p99 |
|---|---|---|
| combat_48, two-pass | 2.60-2.67 / 4.42-4.45 / 4.87-5.19 | 4.15-4.38 / 5.46-5.85 / 6.37-6.74 |
| combat_48, first run (copy) | 2.90 avg / - / 8.6 | 6.63 avg |
| combat_48, single-pass now | 2.51-2.55 / 4.27-4.31 / 4.86-4.91 | 4.45-4.46 / 5.95-6.20 / 7.13-7.27 |
| play_e1m1_fight, two-pass | 1.51 / 1.96-1.98 / 2.76-2.82 | 3.41-3.42 / 4.48-4.81 / 4.90-5.27 |
| play_e1m1_fight, first run (copy) | 1.49 avg | 5.54 avg |
| play_e1m1_fight, single-pass now | 1.49 / 1.97 / 2.76 | 3.51-3.60 / 4.57-4.66 / 4.99-5.06 |

The copy's cost is gone (GPU +2.2 ms -> +0.1-0.3), the CPU is level or 0.1 ms better, the GPU still 0.1-0.3 ms worse.
Where the GPU goes (profiler, combat_48, both eyes): the world in the pass 1.20 ms against 1.25 two-pass (multiview
saves little: it is raster-bound); the alias models 0.89 + 0.08 (view entities) against 0.83; the VR particles after
the pass 0.45 an eye against 0.37 (+0.17 in all), the sky +0.02 an eye. The passes after the pass test against a
depth buffer the multiview pass wrote layered: by every sign the driver's coarse early-Z (ZCULL) is not kept for it, so
blended, depth-tested work after it (particles) rejects less. Porting the particles into the pass would not bring
that back (they would be drawn layered too).

Gate (single-pass >= two-pass on CPU and GPU in both scenes): not met on GPU. Stages 4-5 and TAA under single-pass
were not started. Estimate: the CPU could gain 0.2-0.4 ms with stages 4-5 (the particles' and VR effects' submission,
0.3-0.45 ms in combat_48 for both eyes), the GPU would stay 0.1-0.3 ms behind (or worse, with the particles layered)
on this driver. On a desktop RTX GPU at 90 Hz single-pass does not pay; it would matter on a CPU-bound or tiled
(standalone) GPU, where multiview is native.

## Debugging

- `vr_stereo_single_info`: frames single-pass since the last call and why not, the multiview twins made, any twin that
  failed to compile (its name and uses: its pass draws nothing in the multiview pass until fixed).
- `vr_stereo_single_passes` (bits, default 3): 1 the world and brush models, 2 the alias models in the multiview pass;
  a bit off draws that pass per eye again (to find which pass a difference comes from).
- Both are on Debug > Profiling and Memory.
- The pixel check: `vr_eyeshot 1` with and without the toggle at the same fixed frame (`vr_fixed_frames 1`,
  `sv_random_seed 1`, `vr_particle_seed 1`, `vr_dither 0`), every eye image compared (ROUND22.md has the numbers).
