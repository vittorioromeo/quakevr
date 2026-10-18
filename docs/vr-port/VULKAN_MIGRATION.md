# Vulkan migration (living document)

Status: **no decision, no code.** This page collects what we know about moving Quake VR: UNLEASHED (Ironwail, OpenGL
4.x) to Vulkan, so the decision can be made later with facts. Update it when a spike or a measurement adds one.
Written 2026-10-18 (branch `agent/vulkandoc`).

Legend: **[V]** verified (source named: a file in this repo, a measurement in our docs, or a log), **[I]** inferred or
from memory (the coordinator's earlier answers, or the brief for this page; not re-checked), **[?]** unknown.

## 1. Where the question came from (the author's discussions)

From the coordinator transcript (`d630ff27...jsonl`), 2026-10-16 to 2026-10-17:

- **Proton VR fails today [V].** A player's log: `err:openxr:wine_xrCreateSession Unhandled graphics binding type:
  1000023000 ... Argument chain does not contain any known graphics bindings`. 1000023000 is
  `XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR`, our binding. wineopenxr has no GL binding. Flatscreen under Proton
  should work. Recorded in `LINUX.md` and ROUND22.md ("Native Linux build").
- **Answer given: Proton's OpenXR bridge supports Vulkan (and D3D) only [I].** Not checked against Proton source.
  A later log note (ROUND22.md ~l.440) suspects an implicit layer or hook enumerating the swapchain as D3D11/Vulkan
  images, not proven.
- **Native Linux is the shipped workaround [V]:** GLX (Xlib) binding, builds with clang, tested on WSL2 Arch with
  llvmpipe + Monado only; **no real headset has run it** (`LINUX.md`).
- **"Would Vulkan make it faster?" (2026-10-17) [I, but based on our measurements]:** no, not by itself. We are
  GPU-bound on pixel shading (world shading 54-62% of the GPU frame, then particles, body, post) and Ironwail's GL is
  already low-overhead (indirect draws, bindless, compute, persistent buffers); the main thread idles about 2/3 of the
  frame. Where it would help: the `xrEndFrame` stall on Virtual Desktop's GL path (a D3D interop step that waits for
  the GPU, about 1 ms CPU a frame plus spikes), Linux/Proton VR, standalone Quest (native multiview), foveation
  (but `GL_NV_shading_rate_image` already exists on NVIDIA GL), modest async-compute gains.
  Effort: "months of full-time engine work with a long tail of regressions", "very large".
- **"Does vkQuake make it easier?" [I]:** somewhat. It solves the Vulkan infrastructure and the base Quake passes
  (a guess: a fifth to a third of the work). Not the larger part: our own passes, and the GL glue. Ironwail's renderer
  (GPU-driven indirect, bindless, compute culling, its own transparency) diverged from QuakeSpasm's, so vkQuake's world
  and model code does not drop in. Rebasing onto vkQuake instead was judged worse than porting Ironwail.
  Recommendation then: no port now; do the foveation check and a GL-calls-per-file audit first; if Vulkan becomes a
  goal, use vkQuake as the backend reference. The same wording is in `BACKLOG.md` l.40-41 [V].
- **Single-pass stereo did not help on PC [V: transcript 2026-10-17, `STEREO.md`, ROUND22.md l.8818+]:** on the RTX
  4090 the CPU is not the limit, and NVIDIA desktop GL even added GPU time. Kept as dormant code for a standalone
  port, where CPU submission is the bottleneck. This is the single reason multiview is on the Vulkan list.
- **Steam Frame (Snapdragon, Adreno) [V: transcript 2026-10-16]:** kblood's PR #75 branches (`staging/frame`,
  `feature/frame-portability`, both merged with next on 2026-10-17) carry ARM64/Linux portability and a controller
  profile. Left out of our tree for now: user presence, **frame synthesis and "the Vulkan bridge"**
  (`vr_openxr_vulkan.hpp`, a GL-rendered image handed to a Vulkan OpenXR runtime), "the Vulkan BSP code", profiler
  changes. Their findings that carry over: depth prepass matters a lot on the Frame (removing it made the GPU eye 109%
  slower), 4 Box3D workers best, "force sysmem" and "decals as meshes" are Adreno driver workarounds, a decal-related
  GPU hang. PR #75 has no multiview, no array swapchains (`arraySize = 1`). The `GraphicsImage` boundary
  (`vr_backend.hpp:149`) and pure `overrideEyeProjection` (`vr_synthesis_math.hpp:65`) there would be reusable.
- **Standalone Quest is a backlog item (`BACKLOG.md` l.272) [V]:** "GLES/Vulkan renderer path, performance budget".

## 2. What our docs already say

- `PORTING.md` [V, but **stale**]: written for a vkQuake port when the module was ~14,000 lines; claims
  `vr_gfx_gl.cpp` is ~400 lines (it is now 2396) and "about 2,000 lines of adapter code". Useful parts still valid:
  the hook list (`VR_RenderView`, `VR_RenderingEye`, `VR_PostProcessTarget`, `VR_OverrideProjection`,
  `VR_DrawSceneOpaque/Translucent`, alias/brush transform hooks), the OpenXR-on-Vulkan checklist (create the XR
  instance/system before the Vulkan device with `xrCreateVulkanInstanceKHR/DeviceKHR`; `XrGraphicsBindingVulkan2KHR`;
  `XrSwapchainImageVulkan2KHR`; release eye images only after the frame's queue submit; do not race the runtime's
  queue), and the warning that vkQuake records on worker threads so hooks must only read state. **Needs rewriting
  before use** (decision for the author: DOCS_REVIEW.md already flags it).
- `LIGHTING.md` l.168 [V]: `vr_lighting.cpp` "is renderer-specific (OpenGL, Ironwail's buffers); a vkQuake port"
  would redo it. l.44: "Multiview matters little for a Quake engine" (older note, before the stereo work).
- `GRAPHICS.md` l.44, 85 [V]: vkQuake RT and path tracing are not viable in stereo at 90 Hz.
- `CREDITS.md` l.326 [V]: we already credit vkQuake (Novum) for the GPU lightmap dynamic-light idea; NVIDIA for VRSS
  and multiview samples.
- Perf facts [V: `BENCHMARKS.md`, ROUND21/22]: draw calls ~300 a frame (mean 303 in captures), shadow layered path cut
  ~7700 draw calls to a few hundred; typical `combat_48` frame 2.0-2.5 ms CPU of an 11.1 ms budget. CPU draw
  submission is not what limits PC.
- `TRACY.md` [V]: GPU zones use GL timer queries (`vr_tracy_gpu`, `vr_tracy_gpu_detail` = every pass a zone, per eye);
  `vr_profile.cpp` uses `glPushDebugGroup` markers (`-glmarkers`, `-gldebug`). Both would be redone with Vulkan
  timestamp queries and `VK_EXT_debug_utils`.

## 3. Inventory: GL-specific code a migration must port

Counts are `wc -l` on the tree at `origin/next` (511226e0e) **[V]**. "GL calls" = rough count of lines matching
`gl*(` / `GL_Use` / `GL_Bind` (a crude measure of how much is GL glue; many files are mostly CPU logic).

Whole-repo scale: ~278k lines in `Quake/vr/`. Of that, files that touch GL are a small minority.

### 3.1 Passes and shaders of ours (all need a Vulkan pipeline and rewrite of the GL glue)

| Area | Files | Lines | Notes |
|---|---|---|---|
| Shader sources | `vr_glsl.h` | 3313 | GLSL strings for our passes. Vulkan takes GLSL via glslang to SPIR-V, but code using `GL_ARB_bindless_texture`, `image2D`, `layout(binding)` conventions, clustered-light images needs changes |
| Module's own draws (the `vr_gfx` implementation) | `vr_gfx_gl.cpp`, `vr_gfx.hpp` | 2396 + 351 | The engine-free interface already exists; the GL implementation grew far beyond the 400 lines in PORTING.md. Most GL calls of any file (156). **The best seam for a backend split** |
| Stereo | `vr_stereo.cpp`, `vr_stereo_single.cpp` | 1265 + 655 | Per-eye `V_RenderView`; multiview twins of 59 programs (`QVR_MULTIVIEW`, `GL_OVR_multiview2`, texture views of array layers, `QvrStereoUBO` binding 5). Vulkan multiview is the same idea (`VK_KHR_multiview`, `gl_ViewIndex`) |
| Relight / lighting | `vr_relight*.cpp`, `vr_lighting.cpp`, `vr_shadows.cpp`, `vr_retrolight.cpp`, `vr_flashlight.cpp` | ~4200 relight (+ maps/vis/tool ~1500), 2731, 157, 136, 3142 | Shadow maps/atlas (layered path, ~8 lights), light volumes; lighting.cpp 48 GL calls |
| Retro look | `vr_retro.cpp` | 1587 | Retro/pixel pass and shading |
| Decals | `vr_decals.cpp`, `vr_permdecals.cpp` | 3136, 1518 | Parallax and carved marks; permdecals 35 GL calls; Adreno hang reported on decals (kblood) |
| Gore, blood, wounds | `vr_gore.cpp`, `vr_blood*.cpp`, `vr_bloodstream.cpp`, `vr_bodyblood.cpp`, `vr_wounds.cpp` | 789, 1353 stream, wounds 3533 (47 GL calls) | Mostly CPU spawn logic plus a few draws |
| Particles | `vr_particles.cpp`, `vr_fireparticles.cpp`, `vr_fgfx.cpp` | 3656, 129, 314 | Incl. half-res and retro particles; mostly CPU, draw glue small |
| Body, gloves, avatar | `vr_body.cpp`, `vr_avatar.cpp`, `vr_view_hands.cpp` | 705, 2824, 2399 | Body pre-pass, glove shaders; use engine alias path (hooks) |
| Post-processing | `vr_bloom.cpp` (402, 22 GL), `vr_tonemap.cpp` (229), `vr_haze.cpp` (714), `vr_temporal.cpp` (1026, 52 GL, TAA), `vr_upscale.cpp` (611, 43), `vr_dynres.cpp` (289), `vr_detail.cpp` (713), `vr_ao.cpp` (2232), `vr_envmap.cpp` (1079, 57 GL), `vr_bttrails.cpp` (817) | ~8200 | Each is FBO + fullscreen or compute pass |
| Water, mirrors, portals | `vr_water.cpp`, `vr_portals.cpp`, mirror code in `vr_stereo`/`vr_view` | 2121 (31 GL), 3275 | Mirror/teleporter views are extra scene renders |
| Foveation | `vr_foveated.cpp` | 421 (35 GL) | `GL_NV_shading_rate_image`, NVIDIA only; shading-rate image per eye |
| 3D text, menu panels | `vr_text3d.cpp`, `vr_menuui.cpp`, `vr_gadget.cpp` | 1458, 3175, 2853 | Draw through `vr_gfx` and the engine 2D path |
| Profiling | `vr_profile.cpp` (1305), Tracy `vr_tracy.cpp` (864) | | GL timer queries, `glPushDebugGroup` |

### 3.2 Backend and swapchain

- `vr_backend_openxr.cpp` (2582): runtime selection, WGL graphics binding `XrGraphicsBindingOpenGLWin32KHR`
  (`XR_KHR_opengl_enable`), swapchain images as GL textures, visibility mask (`XR_KHR_visibility_mask`, l.959+, used by
  `vr_visibility_mask`), `xrEndFrame`. `vr_xr_glx.cpp` (74): the Linux GLX binding. `vr_backend_mock.cpp` (1941):
  headless mock whose "eye images" are GL textures (the kit's tests depend on it; a Vulkan backend needs a mock too).
- Visibility mask draws a hidden-area stencil/depth in the eye pass (also in `vr_stereo*`, see `vr_backend_mock.cpp`
  l.1743 for the test mask).

### 3.3 Ironwail's own GL renderer (the part that would be replaced or wrapped)

All in `Quake/` **[V, wc -l]**: `gl_vidsdl.c` 1967 (context, extensions, `-glmarkers/-gldebug`), `gl_rmain.c` 2393,
`r_world.c` 1217 (GPU-driven world, indirect), `r_alias.c` 2072 (alias instancing), `r_brush.c` 1062, `gl_draw.c`
1533 (2D), `gl_screen.c` 2329, `gl_texmgr.c` 2691 (bindless textures), `gl_shaders.c` 679 + `gl_shaders.h` 2434
(GLSL, compute culling, clustered lights, OIT, world/alias variants), `r_part.c` 781, `gl_sky.c` 838, `gl_warp.c`,
`gl_fog.c` 392, `gl_rlight.c` 435, `gl_rmisc.c` 1132, `r_sprite.c` 405, `gl_mesh.c` 446. ~25k lines excluding
`gl_model.c` (5479, mixed CPU/GL). Our fork has local changes in many of these (`IRONWAIL_DIFF.md` lists them).
Features we depend on that have no one-line Vulkan equivalent: bindless textures (`VK_EXT_descriptor_indexing`),
persistent-mapped buffers, indirect multi-draw, compute culling, image load/store for clusters, OIT accum/revealage,
GL_KHR_parallel_shader_compile (replace by pipeline cache + background creation).

### 3.4 Rough size [I]

The GL glue worth rewriting is on the order of **8-12k lines** of ours (rows above with GL call counts in the tens)
plus **~25k lines** of Ironwail renderer, on top of the 3.3k GLSL. A fresh Vulkan device layer (memory, descriptors,
pipelines, barriers, swapchain, frame graph) is more. The earlier "months" estimate stands. Not measured: a count of
GL call sites per file with `grep -c` is above; no per-pass effort estimate exists.

## 4. Reference implementations

- **Quakespasm-VR** (https://github.com/ObeseCatLord/Quakespasm-VR) **[I, from the brief, not yet read by us]:**
  vkQuake 2.0 base, GPL-2.0-or-later; OpenXR `XR_KHR_vulkan_enable2` with legacy fallback (`Quake/vr_openxr.cpp`);
  Vulkan multiview single-pass stereo; `XR_KHR_visibility_mask`; foveation via `VK_KHR_fragment_shading_rate` +
  `XR_EXT_eye_gaze_interaction`, or runtime-managed `XR_FB_foveation` / `XR_META_foveation_eye_tracked`; policy in
  `vr_foveation_policy.h`; upscale pass documented in `docs/migration-vr-upscale.md`. Same repo is cited in BACKLOG
  (multiplayer section) [V]. Code can be borrowed with copyright headers kept and credit (`BACKLOG.md` l.168) [V].
  Best single source for the Vulkan-OpenXR plumbing (instance/device creation order, swapchain, multiview) and the
  fragment-shading-rate foveation that would replace our NVIDIA-only path.
- **vkQuake** (Novum, GPL-2.0) [I]: Vulkan infrastructure, world/alias/sky/warp/particles/2D passes, GPU lightmaps
  with dynamic lights and optional ray-query shadows (`r_rtshadows`, we already credit it). Records on worker threads.
- **kblood's Vulkan bridge** (`feature/frame-synthesis` plan, PR #75) [V that it exists, not read in detail]:
  GL-rendered images handed to a Vulkan OpenXR runtime; relevant to option (d) and Steam Frame.

## 5. Options

| | (a) Rebase onto vkQuake | (b) Vulkan backend under Ironwail | (c) Stay on GL, targeted fixes | (d) GL on Vulkan (Zink / ANGLE / GL-VK interop) |
|---|---|---|---|---|
| Idea | Take vkQuake (or Quakespasm-VR) as the engine, port our VR layer and all passes | Introduce an RHI-like layer, implement Vulkan beside GL, port passes progressively | Keep GL; fix specific costs (foveation, stalls, Linux native) | Run the same GL code through Zink (Mesa) / ANGLE, or render GL into Vulkan-shared images (kblood bridge) |
| Effort [I] | Largest: re-port ~all Ironwail features and our ~278k-line fork's engine edits; lose Ironwail's GPU-driven world | Very large, months; two backends to maintain for a long period | Small, incremental | Small to medium; spike-able |
| Risk | Regressions everywhere; community divergence; but Quakespasm-VR gives a head start for VR plumbing | Highest design risk (RHI over bindless/indirect/compute); long dual maintenance | Low | Performance and correctness unknown; Zink needs Mesa (Linux) so no help on Windows; on Proton the app's GL still goes through Wine's WGL; the OpenXR binding is still GL, so wineopenxr still rejects it unless we bind Vulkan ourselves [I] |
| Proton / wineopenxr VR | Yes (Vulkan binding) [I] | Yes, once Vulkan backend works [I] | No; native Linux build instead | Only if the XR session is created with a Vulkan binding and GL is interoped into it; unproven |
| Steam Frame (ARM, Adreno) | Good: Vulkan is the natural API on Adreno, GL drivers are the source of the workarounds kblood needed [I] | Same | GL works with workarounds (current branches) | Zink on Adreno: unknown [?] |
| Foveation on non-NVIDIA | Yes: `VK_KHR_fragment_shading_rate` / runtime foveation | Yes | No (GL has only NV extension; AMD/Intel GL has none) [I] | Zink exposes it only if the Vulkan driver does and Mesa maps it [?] |
| Multiview | Native `VK_KHR_multiview` | Native | `GL_OVR_multiview2` (done, `vr_stereo_single`; NVIDIA desktop, gains nil on PC) | Maps to Vulkan multiview in Zink [?] |
| Async compute | Possible | Possible | No | No |
| CPU overhead | Lower per draw, but we are not CPU-bound on PC [V]; matters for Quest/Frame [I] | Same | Unchanged; targeted fixes (xrEndFrame wait) | Zink adds overhead |
| Fits our fork | Poorly | Best long-term | Best short-term | Stopgap |

What each unlocks, in short: **Proton VR** (a, b, maybe d), **Steam Frame ARM** with fewer driver workarounds (a, b),
**foveation off NVIDIA** (a, b), **multiview where it pays** on mobile (a, b; GL ES multiview in c), **async compute**
(a, b), **lower CPU/driver overhead** (a, b; modest on PC). Only (c) and (d) cost little now. **Native Linux (done,
GLX) already covers desktop Linux VR** without Vulkan; Proton VR is the gap only.

## 6. Open questions

1. Is Proton VR worth the cost, given the native Linux build? How many players need it? (Not measured.)
2. Does an implicit layer or hook enumerate our swapchain as D3D11/Vulkan, as the Proton log suggests, and does
   wineopenxr offer any GL path at all (e.g. D3D interop)? Read the wineopenxr source before assuming.
3. What does the `xrEndFrame` stall on Virtual Desktop cost per frame exactly (the ~1 ms is an estimate), and is it
   present with Vulkan runtimes? Needs a Tracy comparison of a Vulkan-bound test app.
4. How much does fragment shading rate buy over our NVIDIA GL foveation (`vr_foveated`), and on Adreno?
   (Transcript claim "20-40% of shading" is general, not ours [I].)
5. Does Quakespasm-VR actually run well on Frame/Quest hardware, and how does its quality compare? Read it, run it.
6. License and structure: how much of Quakespasm-VR's VR/OpenXR/foveation code is directly liftable into a Zancle,
   no-`std::`, no-function-local-static C++23 module (our `CODE_STYLE.md`)? Their code is C.
7. Test infrastructure: the kit's headless mock and screenshots assume GL; what is the Vulkan equivalent (lavapipe,
   mock with software Vulkan)?
8. Shader language: keep GLSL via glslang, or move to Slang/HLSL? Affects multiview twins and the 3.3k lines of GLSL.
9. Does bindless-heavy Ironwail rendering map to descriptor indexing on Adreno and Intel?
10. Maintenance: can we stay mergeable with upstream Ironwail (`IRONWAIL_DIFF.md`) if the renderer forks?

## 7. Suggested first spike (cheap, informative, reversible)

1. **Audit first (the backlog item, `BACKLOG.md` l.40):** produce a table of GL calls per file (the crude counts in 3
   made precise), flag what goes through `vr_gfx.hpp` and what is raw GL. Sets the real size of option (b).
2. **Read Quakespasm-VR** (a worker, no code changes in our tree): build it, list its VR files and what is portable
   (instance/device ordering, swapchain, multiview, visibility mask, shading-rate policy), and note GPL headers to keep.
3. **Smallest working proof, new branch, not merged:** a standalone Vulkan OpenXR sample or the mock backend with a
   Vulkan swapchain that receives our GL frame via `GL_EXT_memory_object` / `GL_EXT_semaphore` interop (the "kblood
   bridge" idea) to answer: does a Vulkan binding make wineopenxr (Proton) and Virtual Desktop's `xrEndFrame` happy,
   and what does it cost. This tests option (d) and the stall question without porting the renderer.
4. Decide after that: if interop works and costs little, ship it as the Proton/Frame path; if not, plan (b) around
   `vr_gfx.hpp` and a per-pass port starting with the passes that matter most for Frame and Quest (world, shadows,
   decals), keeping GL as the PC default until Vulkan matches it.
