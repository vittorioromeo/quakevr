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
A twin is made the first time the pass draws with its program (not at every start). While the multiview pass
draws, `GL_UseProgram` swaps a program for its twin; anything else drawn there (a program without a twin) would be an
error, so the passes not ported are not drawn there at all (below).

## Shared per-view data

One uniform block (binding 5, `QvrStereoUBO`): both eyes' view-projection and position. Under `QVR_MULTIVIEW` the
frame data's and the alias instance buffer's `ViewProj` / `EyePos` (and the alias frame data's `FrameViewProj` /
`FrameEyePos`) are renamed in their blocks and become macros reading the view's entry (`QVR_VIEW`: `gl_ViewID_OVR`).
Without the define the shaders are the same text as before: the toggle off compiles and draws exactly what it did.

The clustered lights are per view (the clusters are screen tiles): the multiview pass clusters the lights twice, eye 0
into the usual 3D image and eye 1 into a second one (image unit 1), and the shaders load the view's
(`QVR_LOAD_CLUSTER`). The light list is the same for both (culled by the union frustum).

## Targets and the per-pass fallback

The multiview pass renders into a 2-layer colour array (the eyes' scene format) and a 2-layer depth/stencil array
(the eyes' depth format), at the eyes' render size. Each eye then continues as today with its own `V_RenderView`, but
its scene starts from a copy of its layer (colour and depth/stencil, `glCopyImageSubData`) instead of a clear, and the
passes the multiview pass drew are skipped. So every pass not ported yet renders per eye as before, after the ported
ones, and the image is complete whatever is ported. This is the "copy-out" for now; once the later passes (and the
post-processing) are ported, the eyes' targets become views of the array layers and the copy goes (with OpenXR array
swapchains where the runtime has them).

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

With MSAA (`vid_fsaa`) the arrays are multisample arrays (GL_EXT_multiview_texture_multisample) and the copy is
multisample to multisample.

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

1. Infrastructure: arrays (MSAA too), the views' block, the twins, the toggle, the copy, the per-layer hidden area,
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

## Debugging

- `vr_stereo_single_info`: frames single-pass since the last call and why not, the multiview twins made, any twin that
  failed to compile (its name and uses: its pass draws nothing in the multiview pass until fixed).
- `vr_stereo_single_passes` (bits, default 3): 1 the world and brush models, 2 the alias models in the multiview pass;
  a bit off draws that pass per eye again (to find which pass a difference comes from).
- Both are on Debug > Profiling and Memory.
- The pixel check: `vr_eyeshot 1` with and without the toggle at the same fixed frame (`vr_fixed_frames 1`,
  `sv_random_seed 1`, `vr_particle_seed 1`, `vr_dither 0`), every eye image compared (ROUND22.md has the numbers).
