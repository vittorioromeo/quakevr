# Porting the VR module to another engine (vkQuake)

Quake VR is an engine-independent module (`Quake/vr/`, C++20 and glm, about 16,000 lines) plus a small set of engine
hooks. This page says what a port to another QuakeSpasm-lineage engine — vkQuake is the target in mind — has to
provide, what carries over as is, and what must be written again.

## The boundary

| Layer | Files | What a port does |
|---|---|---|
| **Engine hooks** | `// QVR` lines in 36 engine and build files (about 270 lines) | Place the same calls in the other engine |
| **C API, shared hooks** | `vr_api.h` | Unchanged: host, filesystem, QuakeC, protocol, server physics, client effects, view setup, menu |
| **C API, renderer hooks** | `vr_api_render.h` | Re-place in the other renderer: stereo view, 2D canvas, entity transforms, alias extras |
| **Engine symbols** | `vr_engine.hpp` | The only place engine headers are included from, and the list of engine symbols no header declares |
| **Rendering the module does itself** | `vr_gfx.hpp` (interface) + `vr_gfx_gl.cpp` (Ironwail/OpenGL) | Write `vr_gfx_vk.cpp` |
| **Stereo view setup** | `vr_stereo.cpp` | Rewrite for the other renderer's frame structure |
| **OpenXR graphics binding** | `vr_backend_openxr.cpp` (WGL / `XR_KHR_opengl_enable`) | `XR_KHR_vulkan_enable2` |
| **Everything else** | about 70 files: hands, body IK, throwing, two-handed aiming, holsters, weapons, particles, panel, gadget, menu, protocol, server, physics, rigid bodies, force-grab support, OpenXR session/input | Recompile |

Every engine edit is marked `// QVR` (multi-line hunks on every line), so
`git diff v0.8.2 HEAD -- Quake ':!Quake/vr'` or a search for `QVR` lists the whole engine side.

## Engine hooks

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

`vr_gfx_gl.cpp` (about 400 lines) is the whole OpenGL implementation: one shader, one stream buffer, framebuffers.
A Vulkan version needs two or three pipelines (blend × depth variants), a per-frame vertex ring buffer, and render
passes for the targets; estimated 400–600 lines.

## OpenXR on Vulkan

Today: `XR_KHR_opengl_enable` with a WGL binding, GL swapchain formats, `XrSwapchainImageOpenGLKHR`, the backend
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

## Estimated effort

| Piece | Size |
|---|---|
| Engine hooks (shared) | a day: same call sites |
| `vr_gfx_vk.cpp` | 400–600 lines |
| Stereo view setup for vkQuake | 300–500 lines |
| 2D canvas pass | 200–300 lines |
| Alias extras (bone matrices, zero blend, mirroring) in vkQuake's shaders | 150–250 lines, plus engine changes |
| OpenXR Vulkan binding and frame synchronisation | 300 lines + about 80 in vkQuake's device creation |

About 2,000 lines of adapter code; the rest of the module (about 14,000 lines) is reused as is.

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
