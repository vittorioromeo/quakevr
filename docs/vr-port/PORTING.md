# Porting the VR module to another engine (vkQuake)

Quake VR is an engine-independent module (`Quake/vr/`, C++23 on Zancle and glm, about 200,000 lines in 2026-10; the counts below are from 2026-09) plus a small set of engine
hooks. This page says what a port to another QuakeSpasm-lineage engine — vkQuake is the target in mind — has to
provide, what carries over as is, and what must be written again.

For Steam Frame frame synthesis, Vulkan is now the target path rather than an
optional renderer experiment: the isolated OpenGL test records compositor
shared-image failures during every tested depth-swapchain creation, including
without `XR_EXT_frame_synthesis`, while the native Vulkan test creates both-eye
FP16-motion/D32-depth swapchains and completes its 120-frame data fixture
without those errors. OpenXR's EXT submission structure requires both the
motion and depth subimages, so omitting the failing depth path is not a valid
workaround. See the dated [Steam Frame probe](SPACEWARP_GL_OPENXR_SWAPCHAIN_PROBE_2026-10-14.md)
and [Vulkan roadmap](SPACEWARP_VULKAN_ROADMAP.md).

## The boundary

| Layer | Files | What a port does |
|---|---|---|
| **Engine hooks** | `// QVR` hunks in 91 engine and build files (about 5200 lines in 2026-10; [IRONWAIL_DIFF.md](IRONWAIL_DIFF.md)) | Place the same calls in the other engine |
| **C API, shared hooks** | `vr_api.h` | Unchanged: host, filesystem, QuakeC, protocol, server physics, client effects, view setup, menu |
| **C API, renderer hooks** | `vr_api_render.h` | Re-place in the other renderer: stereo view, 2D canvas, entity transforms, alias extras |
| **Engine symbols** | `vr_engine.hpp` | The only place engine headers are included from, and the list of engine symbols no header declares |
| **Rendering the module does itself** | `vr_gfx.hpp` (interface) + `vr_gfx_gl.cpp` (Ironwail/OpenGL) | Write `vr_gfx_vk.cpp` |
| **Stereo view setup** | `vr_stereo.cpp` | Rewrite for the other renderer's frame structure |
| **Runtime-owned image handles** | `vr_backend.hpp` `GraphicsImage`: API tag, opaque image/view handles, native format and extent | Preserve the API tag and ownership metadata; never reinterpret a Vulkan image handle as a GL texture name |
| **Shadows and per-pixel lights** | `vr_lighting.cpp` + `SHADOW_FUNCTIONS` and the light loops in `vr_glsl.h` (spliced into `gl_shaders.h`) | Rewrite on the other renderer (see [LIGHTING.md](LIGHTING.md)) |
| **OpenXR graphics binding** | `vr_backend_openxr.cpp` (WGL / `XR_KHR_opengl_enable`) | `XR_KHR_vulkan_enable2` |
| **Everything else** | about 70 files: hands, body IK, throwing, two-handed aiming, holsters, weapons, particles, panel, gadget, menu, protocol, server, physics, rigid bodies, force-grab support, OpenXR session/input | Recompile |

`GraphicsImage` is interface groundwork only. Current stereo, panel and synthesis
consumers explicitly accept OpenGL images and safely reject another API; this
does not add a Vulkan backend or make the existing GL renderer usable with
Vulkan swapchains. The brush renderer exposes `R_BuildBModelVertexData()` for
CPU-side BSP vertices, and `vr_bsp_mesh.*` builds a bounds-checked fan-index
packet with per-surface material ranges for opaque world surfaces. It also
builds a compact owned position/UV packet with local indices for later native
renderer upload. Neither packet is connected to Quake's running renderer yet.
The generic Vulkan buffer uploader and indexed draw recorder exist in
`vr_vulkan_bsp.*`; the standalone Steam Frame fixture exercises them with a
synthetic packet in the same format. `-DQVR_ENABLE_NATIVE_VULKAN_BSP=ON`
compiles and links that component into the engine when the Vulkan SDK is
installed. The Quake gameplay renderer does not call that code yet; its runtime
graphics binding, production shader pipeline/material upload, stereo passes,
depth retention and OpenXR frame submission remain to be integrated.

Every engine edit is marked `// QVR` (a comment in each hunk), so
`git diff v0.8.2 HEAD -- Quake ':!Quake/vr'` or a search for `QVR` lists the whole engine side ([IRONWAIL_DIFF.md](IRONWAIL_DIFF.md):
the files, what each change is for, and what stays inline).

## Engine hooks

The shared model metadata service (`vr_modelmetadata.hpp`, C access in `vr_modelmetadata.h`) caches
copied model facts. Call `VR_ModelMetadataChanged(model)` immediately before every actual model load,
including alias-cache eviction and synthetic model loading. Map/game-directory/model-reload events
also release the cache. See [Shared model metadata](MODEL_METADATA.md) for its API and lifetime rules.

`vr_api.h` groups them by engine subsystem, each with the call site it expects (function and position). They are
thin calls: the engine code stays upstream's, with at most an `if (VR_...()) return;`. The hooks that follow
QuakeSpasm's structure (host frame, `COM_AddGameDirectory`, `PR_LoadProgs`, `SV_SpawnServer`, `SV_ReadClientMove`,
`SV_CalcStats`, `SV_WriteEntitiesToClient`, `CL_ParseUpdate`, `CL_ParseBeam`, `SV_Physics_Client`, `SV_Physics_Toss`,
`SV_RunThink`, `SV_TouchLinks`, `SV_LinkEdict`, `SV_Move`, `R_RunParticleEffect`, ...) port almost verbatim to
vkQuake; check there:

- the generic stat channel (`statsi`/`statsf`, `MAX_CL_STATS` 64+): VR stats use slots 64 and up;
- `PROTOCOL_RMQ` with protocol flags: the VR protocol is flags `1 << 16` (VR protocol) and `1 << 17` (Quake VR progs);
- QC builtins bound by name (`vr_builtins.cpp`: Ironwail's by-name binding and builtin numbering from 1000);
- `SV_*` helpers the module calls must not be `static` (`vr_engine.hpp` lists them);
- `com_basedirs` / `com_gamenames` for the game folder layering (`vr_gamedir.cpp`).

Renderer hooks (`vr_api_render.h`) follow Ironwail's renderer and must be redone:

| Hook | Ironwail | In vkQuake |
|---|---|---|
| `VR_RenderView`, `VR_RenderingEye`, `VR_PostProcessTarget`, `VR_OverrideProjection` | replaces `V_RenderView` per eye; swaps Ironwail's scene framebuffers for eye-sized ones; post-process into the swapchain image; asymmetric projection into `r_matproj` (reversed Z) | per-eye render targets for vkQuake's render passes; its projection (Vulkan clip space: z 0..1, y down) |
| `VR_Begin2D`, `VR_End2D`, `VR_CanvasBlend` | the 2D pass into a canvas (premultiplied alpha) | an extra render pass for the 2D layer; a blend pipeline variant |
| `VR_DrawSceneOpaque`, `VR_DrawSceneTranslucent` | after the opaque entities / after the translucency pass | same points in vkQuake's scene recording |
| `VR_AliasPreTransform`/`PostTransform`, `VR_BrushTransform`, `VR_AliasMirrored`, `VR_AliasLightModifier`, `VR_AliasZeroBlend`, `VR_AliasBonePoses`, `VR_IsViewEntity`, `VR_HideViewModel` | per-entity model matrix, mirrored culling, light, a third pose blend in the alias shader, per-entity bone matrices (MD5) | vkQuake's alias and brush draw paths; the zero-blend pose and bone matrices need shader/pipeline support there |

vkQuake records rendering on worker threads: the renderer hooks must only read state. Everything they read is prepared
on the main thread by `VR_SetupViewEntities` (view entities, weapon transforms). One place still caches on first use
(`weapons::slotForModel`, `vr_weapons.cpp`): precompute it in `VR_SetupViewEntities` before porting.

## `vr_gfx`: the module's own rendering

`vr_gfx.hpp` is engine-free. It covers every draw the module makes itself:

- `draw(triangles, mvp, State{shade, blend, depthTest, depthWrite}, texture)`: shades are colour, soft-edged colour
  (lines, blob shadows), texture × colour (particles, panels, the gadget's screen) and font cutout (world text);
  blends are opaque, alpha and premultiplied; no culling;
- `sceneViewProjection()`, `sceneCamera()`, `fontTexture()`, `fontGlyph(c)` (the console font's atlas cells);
- offscreen colour targets, 2D drawing onto them with the engine's 2D functions (`begin2D`/`draw2D::*`/`end2D`), the
  2D pass redirected into the canvas (`beginCanvas`/`endCanvas`/`applyCanvasBlend`), `copy(target, image)` to a
  runtime's swapchain image, and textures (the mock backend's eyes, the particle atlas).

`vr_gfx_gl.cpp` contains the module's OpenGL drawing implementation. In the
2026-10-14 checkout it is about 2,055 lines, not the roughly 400 lines stated
in an earlier estimate. It covers the module's graphics features, not Quake's
world/alias renderer. A Vulkan module renderer needs Vulkan pipelines, buffers,
target/render-pass integration, descriptors, barriers, shader variants and
resource lifetime handling; line estimates are withheld until a compiling
renderer slice exists.

## OpenXR on Vulkan

Today: `XR_KHR_opengl_enable` with WGL/GLX bindings, GL swapchain formats, `XrSwapchainImageOpenGLKHR`, the backend
handing out GL texture names (`vr_backend.hpp`), and acquire → render → release → `xrEndFrame` inline in
`VR_RenderView`. For Vulkan:

1. **Order of creation.** The XR instance and system exist before the Vulkan device: use
   `xrCreateVulkanInstanceKHR`/`xrCreateVulkanDeviceKHR` (or at least the runtime's physical device and extensions)
   when vkQuake creates its device. Split the backend into "before video init" and "start session"; toggling VR
   then needs a `vid_restart`.
2. **Session:** `XrGraphicsBindingVulkan2KHR` (instance, physical device, device, queue family and index).
3. **Images:** `XrSwapchainImageVulkan2KHR` gives `VkImage`s: image views and framebuffers compatible with vkQuake's
   passes. `Backend::acquireEyeImage`/`acquirePanelImage` return an opaque handle instead of a GL name.
4. **sRGB:** keep today's "gamma-encoded passthrough" with a UNORM view (a mutable-format swapchain).
5. **Synchronisation — the biggest change.** Eye images must be released after vkQuake's command buffers are
   submitted, and the queue OpenXR uses must not race vkQuake's submits: a hook after the frame's queue submit
   (`VR_AfterSubmit`: release the images, `xrEndFrame`).
6. The panel copy and the desktop mirror become `vkCmdBlitImage`; the desktop's vsync a present mode.

## Porting scope and size

The earlier estimates of about 2,000 lines of adapter code and 400–600 lines
for `vr_gfx_vk.cpp` are withdrawn as too optimistic. A fresh count of this
Ironwail checkout is about 2,055 lines in `vr_gfx_gl.cpp`, 1,070 in
`vr_stereo.cpp`, 1,897 in `vr_backend_openxr.cpp`, and 2,450 in
`vr_lighting.cpp`; these counts exclude the engine hooks, shader strings and
the actual Quake GL renderer that also needs Vulkan support.

The audited vkQuake snapshot is commit
[`33ad15e6c9a5303c1af1050f63391baad289b4e7`](https://github.com/Novum/vkQuake/tree/33ad15e6c9a5303c1af1050f63391baad289b4e7).
Its Vulkan video/device and frame-submit path is concentrated in a 4,395-line
`Quake/gl_vidsdl.c`; render-pass construction is in a separate 1,127-line
`Quake/r_passes.c`, and brush rendering spans thousands of lines. Its Vulkan
instance/device initialization and queued frame submission are integrated into
the renderer, so adding OpenXR requires device-creation order changes and
render-pass/frame synchronization—not only a new VR graphics adapter.

No defensible total line count or calendar estimate exists yet. The next
engineering gate is a small vkQuake branch that obtains the runtime-selected
Vulkan device, binds an OpenXR session, renders each eye into OpenXR color
images, then adds valid motion/depth targets and submits EXT metadata. Only
after that should the full Quake VR engine hooks, stereo gameplay and module
graphics be ported. The separate Steam Frame Vulkan fixture proves the runtime
API path, but does not make the game-renderer port complete.

## Known engine-specific corners (to hide behind the adapter when porting)

- `vr_menu.cpp` draws with Ironwail's menu helpers (`M_Print`, `M_DrawSlider`, ...) and hooks the options menu's
  item list; vkQuake's options menu is QuakeSpasm's.
- `vr_panel.cpp` reads Ironwail's HUD layout (`hudstyle`, `Draw_GetCanvasTransform(CANVAS_SBAR)`) to cut the status
  bar out of the 2D layer.
- `vr_anchor.cpp` and `vr_avatar.cpp` read Ironwail's alias model data (`aliashdr_t`, pose vertices, MD5 bones).
- `vr_view.cpp` builds `entity_t`s and adds them to `cl_visedicts`.
- `vr_builtins.cpp` copies pr_cmds.c's static `WriteDest`.
- vkQuake's headers use C11 atomics: if C++ cannot include `quakedef.h` there, `vr_engine.hpp` becomes a C adapter
  (a few hundred lines of accessors) and the module includes only that.
