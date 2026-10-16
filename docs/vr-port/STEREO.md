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
The twins are made the first time the toggle is on (not at every start: about 40 programs). While the multiview pass
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

Single-pass is skipped for a frame (two-pass as before) when: the driver lacks multiview, the eyes have MSAA
(`vid_fsaa`), the frame is the spectator camera or a view through a teleporter (those always render alone), or
`vr_stereo_single` is 0.

## Culling

The scene is walked once with a frustum that holds both eyes': the head's orientation, each side as wide as the
widest eye's edge, its apex moved back so that both eyes are inside it (conservative: never culls what either eye
sees). The PVS is both eyes' leaves' (OR'd when they differ). The world's GPU backface cull keeps a surface either eye
faces (a second view origin in its uniform block; the two-pass path passes the same origin twice).

Per eye still (two-pass for that object): alias models near the eyes (`vr_stereo_single_near`, units from the eyes'
midpoint to the model's bounding sphere): the hands, held weapons and the body, whose near-plane handling
(`VR_AliasNearEye`: depth clamp, both sides when an eye is inside) is decided per eye.

## Stages

Each stage keeps the toggle's fallback: what is not ported renders per eye.

1. Infrastructure: arrays, the per-view block, the twins, the toggle, the copy-out, the per-layer hidden area. Done.
2. World surfaces: the opaque world and brush models (solid and fences, the depth pre-pass, decals, permanent decals,
   parallax, lights). Done. Sky and liquids/teleporter surfaces: not yet (they read per-eye scene copies and the
   teleporters' views).
3. Alias models (smoothing, rebuild, dents, wounds, gloves): done for those away from the eyes; the near ones per eye.
4. Sprites, particles, blood streams, decals' own passes, beams, chains/coils, gfx tubes: not yet.
5. Translucency (OIT) and post (bloom, grade, tone curve per layer): not yet.

Per eye for good: the views through teleporters, mirrors, the desktop mirror and spectator camera, and whatever
differs per eye.

## Foveated rendering

`vr_foveated` shades through NVIDIA's shading-rate image per eye. The multiview pass shades at full rate for now (the
shading-rate image would need a layered image indexed by view: to try). The passes per eye keep it.

## Measurements

See ROUND22.md, "Single-pass stereo".
