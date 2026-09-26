# Temporal anti-aliasing and temporal upscaling (TAA, DLSS/DLAA, FSR): scope and design

Status: research and design only, nothing implemented. Written 2026-09-26 against branch `vr-cleanup`
(after round 20). Stage (1) of the agreed plan, FSR 1/NIS spatial upscaling and fixed foveated VRS, is being done
separately; this document scopes stage (2), motion vectors and camera jitter with our own TAA, and stage (3), DLSS/DLAA
through OpenGL-to-Vulkan interop with FSR as the fallback. It also answers the licensing question as far as research
can. It is not legal advice.

Reference machine: Quest 3 over Virtual Desktop (VDXR or SteamVR), RTX 4090, eye images 3292x3524 (11.6 Mpx an eye,
23.2 Mpx a frame), 120 Hz (8.33 ms a frame).

Contents:

1. Summary and recommendation
2. Findings (with sources)
3. Licensing
4. How the eyes are rendered today (the parts that matter)
5. Design: motion vectors
6. Design: jitter
7. Design: our TAA (the validation step)
8. Design: the upscaler slot in the eye pipeline
9. Design: the DLSS path (interop module)
10. Staged plan and effort
11. Risks
12. Open questions for the author
13. Sources

## 1. Summary and recommendation

- **DLSS has no OpenGL API**: NGX supports D3D11, D3D12, Vulkan and CUDA only, and NVIDIA's guide leaves
  cross-API interop to the application. So we need an OpenGL→Vulkan (or →D3D12) interop layer. That is well-trodden
  ground: `GL_EXT_memory_object_win32` + `GL_EXT_semaphore_win32`, with Vulkan allocating and GL importing. It is
  about a round of work, and FSR 3.1's Vulkan backend can reuse it.
- **Licensing**: we **must not ship** `nvngx_dlss.dll`, NVIDIA's static NGX library or NVIDIA's headers as part of the
  GPL engine. NVIDIA's own GPL-2 Quake II RTX declined DLSS for this reason. There are two precedents for doing it
  anyway, both with the proprietary part kept outside the GPL distribution: Quake: Ray Traced (vkquake-rt) ships
  DLSS as a **separate optional renderer DLL** and has the **user download `nvngx_dlss.dll`**, and Blender 5.3
  **loads the driver's NGX at run time** with a user-supplied DLSS DLL and no official builds carrying it. Private use
  raises no issue at all, because the GPL's conditions apply to distribution. Details are in section 3.
- **Recommended plan**:
  1. (2a) Motion vectors, with a debug view and a reprojection test (1 round).
  2. (2b) Jitter plus our own TAA at native resolution, replacing MSAA as an option (1 round, then about half a
     round of tuning on the HMD).
  3. (2c) Split the eye into a render size and a display size, which is the temporal-upscaler slot (half a round).
  4. (3a) The interop helper `qvr_dlss.dll` with the DLSS backend. Build it from our own MIT-licensed source plus
     NVIDIA's header-only loader (DLSS SDK 310.9.1+), load it at run time, and publish it as a separate optional
     download with a user-supplied `nvngx_dlss.dll` (1–1.5 rounds).
  5. (3b, optional) FSR 3.1 through the same helper, built as a separate MIT `qvr_fsr.dll` that can ship with the
     engine (half a round to 1 round).
- **Reality check on speed**: DLSS's cost depends on the output size, and our output is two 11.6 Mpx eyes. From
  NVIDIA's own table (RTX 4090, 4K output), the transformer presets cost about 1.1–1.7 ms per 4K frame, so roughly
  **3–4.7 ms per VR frame** for both eyes, against an 8.33 ms budget. On a Quake scene this is a **quality feature**
  (DLAA instead of MSAA, or a higher internal resolution), not a speed-up. Our TAA should cost about 1 ms per frame.
  Measure both against 4x MSAA with `vr_profile`.

## 2. Findings (with sources)

### 2.1 DLSS SDK today

- **Releases**: the latest release of `NVIDIA/DLSS` is **310.9.1 (8 Sep 2026)**. Earlier: 310.7.0 (June 2026) and
  310.6.0 (April 2026, frame generation 5x/6x). 310.5.0 (Jan 2026) added presets L and M. The transformer model left
  beta in 310.3.0. [DLSS releases]
- **Presets and models**: "DLSS 4.5" is the second-generation transformer (presets **L, M**). DLSS 4's transformer
  is presets **J, K**. The guide (v310.6.0) defaults K for DLAA/Quality/Balanced, M for Performance and L for Ultra
  Performance, and says L and M are "peak performant on RTX 40 series and above". The old CNN presets are
  deprecated. [DLSS PG §2.4, §3.x; NVIDIA DLSS 4.5 news]
- **"DLSS 5"** is a different product: generative "neural rendering" of lighting and materials, not an upscaler. It
  launched in September 2026 on RTX 50 only, through Streamline/Unreal. It is not relevant here, and its runtime DLL
  in circulation is a leak (see 2.3). [NVIDIA DLSS 5 news; TechPowerUp]
- **APIs**: NGX supports D3D11/D3D12/Vulkan (1.1+)/CUDA. Streamline 2.x supports DX11/DX12/Vulkan 1.2+. **No
  OpenGL**. The guide says interop between APIs "must be handled by the game or application outside of the NGX SDK"
  (§5.2). [DLSS PG §2.2, §5.2; Streamline]
- **Inputs**:
  - Colour, depth (any single-channel format, e.g. R32F, or D24S8), and 16/32-bit motion vectors (low-res, with a
    scale).
  - Jitter offsets in render pixels in [-0.5, 0.5], with at least 16 phases (32+ preferred) and Halton recommended.
    The mode table gives 8 for DLAA, 18 Quality, 24 Balanced, 32 Performance and 72 Ultra Performance, which is
    `8·(display/render)²`.
  - Exposure: a 1x1 texture for J/K; L always auto-exposes. Optionally a pre-exposure and a "bias current colour"
    mask.
  - A project ID, and flags for inverted depth, HDR, and jittered or low-res MVs.
  - DLAA is simply input size = output size.
  - [DLSS PG §2.2, §3.3–3.7]
- **Mip bias**: `NativeBias + log2(renderX/displayX) − 1.0 + ε` (e.g. −2 at Performance). If textures flicker, back
  it off, but not beyond `log2(render/display)`. [DLSS PG §3.5]
- **VR**: create **one DLSS feature per view (eye)**. If both eyes share one texture, use input sub-rectangles, and
  set `InEnableOutputSubrects` at creation to use output sub-rectangles. The guide says nothing about using the same
  or different jitter per eye. [DLSS PG §3.17]
- **Requirements**: any RTX GPU, a Windows driver from March 2022 or newer (512.15+), Windows 10 1709+. [DLSS PG §2.1]
- **Packaging**:
  - The application ships the signed `nvngx_dlss.dll`. It normally links `nvsdk_ngx_s.lib`.
  - Since **310.9.1** there is a header-only **`nvsdk_ngx_loader.h`** that finds and loads the driver's NGX core
    (`_nvngx.dll`) at run time, from the app directory, the registry or the driver store. Its SPDX tag is
    `LicenseRef-NvidiaProprietary`.
  - The DLSS DLL's third-party notices (curl, pugixml, …) must go in the product's documentation.
  - [DLSS PG §4.2, §9.6; nvsdk_ngx_loader.h]
- **Cost** (DLSS PG §2.4, "rough estimate", RTX 4090, 3840x2160 output from 1080p): CNN (E/F) 0.53 ms; transformer
  J/K 1.06 ms; M 1.39 ms; L 1.67 ms. VRAM at 4K is 198 / 307 / 448–456 MB. One eye here is 11.6 Mpx, 1.4x a 4K
  frame. Scaling linearly, both eyes cost about **1.5 ms (CNN), 3.0 ms (K), 3.9 ms (M), 4.7 ms (L)** per VR frame.
  This is our estimate, not NVIDIA's; the DLAA cost is not tabulated.
- **Users in VR**:
  - MSFS forum: DLSS 4 (v310.1) "is amazing in VR", with blur and ghosting mostly gone even at Balanced.
  - DCS users report the transformer much better than the CNN's ghosting.
  - MSFS 2024 smeared cockpit displays with preset F until users forced K/M.
  - UE 5.6 had a DLSS-in-VR ghosting regression (TSR/deferred).
  - A UE instanced-stereo bug had wrong velocities in the secondary eye only. That is a reminder to **validate the
    right eye's vectors separately**.
  - [MSFS forum; ED forums; MSFS Addons; NVIDIA dev forum; DisplayXR issue]

### 2.2 OpenGL↔Vulkan interop

- **Pattern**: Vulkan allocates, with export flags (`VkExternalMemoryImageCreateInfo`, `VkExportMemoryAllocateInfo`,
  a dedicated allocation), and GL imports:
  - memory with `glCreateMemoryObjectsEXT`, `glImportMemoryWin32HandleEXT` and `glTextureStorageMem2DEXT`;
  - semaphores with `glImportSemaphoreWin32HandleEXT`, then `glSignalSemaphoreEXT` and `glWaitSemaphoreEXT` with
    per-texture layouts (`GL_LAYOUT_SHADER_READ_ONLY_EXT`, `…COLOR_ATTACHMENT…`, `…TRANSFER_SRC/DST…`). Signalling
    flushes.
  - GL memory objects are import-only.
  - NVIDIA's reference is `nvpro-samples/gl_vk_simple_interop` (Apache-2.0). `gl_vk_raytrace_interop` is the closest
    to "a Vulkan pass on a GL frame".
  - [EXT_external_objects; gl_vk_simple_interop; DLSS5-Feeder notes]
- **Rules**:
  - Match GL's `GL_DEVICE_UUID_EXT`/`GL_DRIVER_UUID_EXT` to `VkPhysicalDeviceIDProperties`. Handles may only be
    imported on the same device.
  - `GL_TEXTURE_TILING_EXT` must match the Vulkan image's tiling and be set before storage.
  - `GL_DEDICATED_MEMORY_OBJECT_EXT` must be set if Vulkan used a dedicated allocation.
  - [EXT_external_objects]
- **Pitfalls**:
  - Reported corruption came from missing export/dedicated structures. Some images needed
    `VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT`.
  - Driver bugs have been reported on AMD/Intel, where the extension is advertised but entry points are missing. No
    NVIDIA-specific bugs were found.
  - Sharing depth/stencil formats is legal in the spec, but we found no confirmation for NVIDIA. **Copy depth to an
    R32F colour image** instead.
  - [vulkano PR #1734; AMD forum; EXT_external_objects]
- **Existing GL→DLSS bridges**:
  - **DLSS5-Feeder** (ReShade add-on) added OpenGL in v0.7.0, via a private **D3D12** device and GL imports of shared
    textures and fences. It reports 0.13 ms of CPU overhead a frame and was tested in one game. [DLSS5-Feeder]
  - **`NoelGamer/dlss5-opengl-bridge`** (MIT, 10 stars, 5 commits all on 30 Aug 2026) is a ReShade add-on that feeds
    GL frames to a D3D12 "DLSS 5 neural rendering" add-on. It is "DLAA only", with camera-only vectors and no
    jitter. It depends on a DLSS 5 runtime DLL obtained from a modding Discord, which TechPowerUp reports **leaked
    from an NBA 2K27 early-access build**.
  - The bridge is not DLSS Super Resolution and not a design to copy, and **we should not use or reference its DLL**.
    [dlss5-opengl-bridge; TechPowerUp; heldgames]
  - Minecraft Java's DLSS mods (Radiance, Vulkanfish, VulkanMod-DLSS; GPL-3) replace the GL renderer with Vulkan
    rather than bridge. Vulkanite (LGPL-3) uses GL↔Vulkan interop, but for ray tracing. [repos]

### 2.3 FSR and XeSS

- **AMD FidelityFX SDK** (2.x "Redstone"): **MIT**. FSR 2 and FSR 3.1 upscaling have **DX12 and Vulkan** backends.
  FSR 4 (ML) ships as signed DLLs, DX12 only, RDNA 4 only. [FidelityFX SDK; VideoCardz]
- **FSR 2 on OpenGL**: `JuanDiegoMontoya/FidelityFX-FSR2-OpenGL` (MIT, FSR 2.2.1). It needs `GL_ARB_gl_spirv` and
  subgroup extensions. Its author measured it at about 3x slower than expected on an RTX 3070 and notes a remaining
  depth-convention error. It works in-process with no interop, but it is not the fast path. [FSR2-OpenGL; the
  author's porting write-up]
- FSR 2's recommendations: jitter phases `ceil(8·(display/render)²)`, Halton(2,3); mip bias
  `log2(render/display) − 1`. It has a reactive mask and a transparency-and-composition mask. Cost: 0.7 ms at 4K
  Quality on an RX 7900 XTX. [FSR2 README]
- **XeSS 3**: Intel Simplified Software License. Binary only; redistribution is allowed but "modification or
  alteration" is forbidden. No explicit anti-copyleft clause. DX11/DX12/Vulkan, no GL. It is not worth a third
  backend for us. [XeSS repo, LICENSE]

### 2.4 Temporal AA practice, and VR

- **References**:
  - Karis 2014, "High Quality Temporal Supersampling": 3x3 closest-depth velocity, neighbourhood clamp, `1/(1+L)`
    weights.
  - Playdead INSIDE 2016 (MIT code): YCoCg clipping, velocity dilation.
  - Salvi: variance clipping, μ ± γσ, γ ≈ 1.
  - Yang, Liu and Salvi 2020 survey: YCoCg variance clipping.
  - Tardif's "TAA starter pack": 5-tap Catmull-Rom history, blend 0.05–0.1.
  - [links in section 13]
- **VR practice**:
  - Meta says to "almost always" use MSAA on Quest (standalone).
  - Vlachos (Valve, GDC 2015) called 4x MSAA the minimum and 8x preferred.
  - Half-Life: Alyx uses 4x MSAA at every setting.
  - Unity URP's TAA excludes MSAA and dynamic resolution, and had stereo TAA bugs.
  - PC VR titles with DLSS: No Man's Sky, Into the Radius, Wrench, F1 22, MSFS, DCS.
  - The historical advice against TAA in VR predates good per-eye motion vectors and transformer DLSS. The recent
    user reports above are the change the author read about.
  - [Meta; Vlachos 2015; Alyx analysis; Unity; Road to VR]
- **Runtime reprojection**:
  - **Virtual Desktop's SSW synthesises frames from the video encoder's motion vectors**, not the app's.
  - SteamVR Motion Smoothing likewise works from the last two frames' colour.
  - ASW 2.0 uses submitted depth; `XR_FB_space_warp` (app MVs + depth) is a Quest-native path that VD does not
    consume.
  - So the runtime never sees our jitter as long as we submit the resolved, unjittered image. Raw jitter would look
    like frame-to-frame noise to the encoder's motion search.
  - [UploadVR; Road to VR; Meta]

## 3. Licensing

### 3.1 The terms

**NVIDIA RTX SDKs License** (v. 14 March 2024, `LICENSE.txt` in `NVIDIA/DLSS`):

- It lets you distribute the SDK "as incorporated in object code format into a software application" with "material
  additional functionality", under terms at least as protective (§1.c, §2).
- **§4.e**: you "may not use the SDK in any manner that would cause it to become subject to an open source software
  license". The licence gives examples: terms requiring that it be disclosed in source form, licensed for making
  derivative works, or redistributable at no charge.
- Other obligations:
  - Attribution: the NVIDIA marks on the splash screen, in the about box and in the credits (supplement §7.1).
  - Notice to NVIDIA **before a commercial release** (supplement §4).
  - Use only on NVIDIA GPUs, no reverse engineering, automatic termination on breach.
- NVIDIA's headers, including the new loader, are proprietary (SPDX `LicenseRef-NvidiaProprietary`).
- Streamline's own code is MIT (with NVIDIA-licensed exceptions), but it still loads the same DLSS plug-in.

GPL-2 requires that the whole of a combined work be distributable under the GPL (source available, no extra
restrictions), and its system-library exception excludes any component that "itself accompanies the executable".
NVIDIA's §4.e forbids exactly that combination from NVIDIA's side. The FSF's FAQ treats a dynamically loaded plug-in
that "make[s] function calls to each other and share[s] data structures" as one program with its host, and says an
exception can only come from the copyright holders. For Quake that means id/ZeniMax plus every QuakeSpasm, QSS and
Ironwail contributor, so it is not realistic.

### 3.2 What GPL projects actually did

| project (licence) | what they did |
|---|---|
| **Quake II RTX** (GPL-2, NVIDIA's own) | Added FSR 1 in v1.6. An NVIDIA developer said DLSS can't be added because of Quake's GPL (January 2022). |
| **Quake: Ray Traced / vkquake-rt** (GPL-2, vkQuake/QuakeSpasm based) | Renders through RayTracedGL1 (MIT). DLSS is a **separate `RayTracedGL1-DLSS.zip`** that replaces the renderer DLL, and the **user downloads `nvngx_dlss.dll` from NVIDIA's GitHub** into the game folder. FSR 2 is built in. PrBoom+ RT (GPL) does the same. This is the closest precedent. |
| **Blender** (GPL) | DLSS Ray Reconstruction for the Cycles viewport (PR #153077, for 5.3). It uses the headers and the run-time NGX loader only and loads the driver's `_nvngx.dll`. DLSS stays unsupported unless the **user places `nvngx_dlssd.dll`** next to Blender. Reviewers said Blender cannot ship the full package, and there are to be no official builds with it until licence compatibility is "sufficiently verified". |
| **OBS Studio** (GPL-2) | The NVIDIA audio/video effects activate only if the user installs NVIDIA's redistributable separately. |
| **OptiScaler** (GPL-3) | Never bundles DLSS; the game or the user provides `nvngx_dlss.dll`. |
| **dxvk-nvapi** (MIT) | Doesn't implement DLSS; it forwards to the driver's NGX. |
| **Godot** (MIT) | DLSS is not in core; the proposal discussion points at opt-in run-time loading with a user-installed library. |
| **RBDOOM-3-BFG** (GPL-3) | Uses its own TAA (from NVIDIA's MIT-licensed TAA); no DLSS. |
| **Ironwail itself** (GPL-2) | Loads Steam's proprietary `steam_api` at run time from the user's Steam install (`Quake/steam.c`), and we load `nvml.dll` from the driver (`vr_gpustats.cpp`). This is the same "don't ship it, load it if the system has it" pattern, for a much smaller surface. |

### 3.3 Plain-language risk summary (not legal advice)

- **No risk: private use.** The GPL's conditions attach to distribution. The author can build and use a DLSS
  helper on their own machine today, whatever its licence.
- **Low risk: FSR 3.1 or FSR 2 (MIT).** MIT is GPL-compatible, so it can ship in the main build.
- **Low to moderate risk, with precedent: DLSS as a separate optional helper.**
  - The helper is our own source under a permissive licence, built against NVIDIA's headers fetched at build time,
    not committed to our repository.
  - It links nothing of NVIDIA's statically: it uses `nvsdk_ngx_loader.h` to load the driver's NGX core.
  - It is published as a **separate download**, with NVIDIA's attribution and notices.
  - **Users supply `nvngx_dlss.dll`** from NVIDIA's GitHub. The GPL engine release contains no NVIDIA code or
    binaries, and the engine never requires the helper.
  - This is what vkquake-rt and Blender do. The weak point is the FSF's view that host and plug-in form one combined
    program. In practice this shape has not drawn complaints, and the likely claimants are the Quake source ports'
    contributors, who have tolerated vkquake-rt's DLSS zip for years. **Putting the helper in a separate process**
    strengthens the separation. The FSF's own line is that separate programs communicating over IPC are not one
    work, and every OpenXR runtime already consumes our GL textures across a process boundary. The GPU cost is the
    same; the CPU cost is a per-eye IPC signal of tens of microseconds, plus a process to manage.
- **High risk (don't):**
  - bundling `nvngx_dlss.dll` or the helper in the GPL release zip;
  - linking `nvsdk_ngx_s.lib` into `ironwail.exe`;
  - committing NVIDIA's headers to the GPL tree;
  - anything using the leaked "DLSS 5" DLL.
- Obligations that stay with whoever distributes the helper: NVIDIA marks in the credits/about screen, the DLSS
  DLL's third-party notices, and notification before any *commercial* release (not our case).

## 4. How the eyes are rendered today (the parts that matter)

Read from `Quake/vr/vr_stereo.cpp`, `Quake/gl_rmain.c`, `Quake/r_world.c`, `Quake/r_alias.c`,
`Quake/gl_shaders.h`:

- `VR_RenderView` loops over the two eyes. Each eye swaps in **one shared set** of eye framebuffers
  (`stereo::eyeFramebufs`: scene colour, depth/stencil `GL_DEPTH24_STENCIL8`, OIT accum/revealage, composite), sets
  the eye's asymmetric projection (`VR_OverrideProjection`, reversed Z with `glClipControl`), runs `V_RenderView`
  (`R_SetupView` then `R_RenderScene` then `R_WarpScaleView`), then `bloom::apply`, then `GL_PostProcess` (tone curve,
  grade, gamma, underwater wobble, dither) into the swapchain image, or into a render-scale texture that is
  linear-blitted into the image (`vr_render_scale`). The UI (lasers, HUD panel, menu, wrist log) is drawn **after**
  post-processing, at the image's size. The left eye's scene is then mirrored to the window.
- Both eyes reuse the same scene/depth/composite textures, so nothing of the left eye survives the right eye. A
  temporal technique needs per-eye history (and, for DLSS/FSR batching, per-eye inputs).
- `R_SetupView` runs **per eye** (`r_framecount` increments twice per VR frame). A per-frame counter for "previous
  frame" bookkeeping must be `host_framecount` (or a VR frame counter), not `r_framecount`.
- Opaque order inside `R_RenderScene`: clear, hidden-area mesh (black at the near plane, identity matrix: clip space
  directly), world and brush models (depth pre-pass of `BP_SOLID` with `glprogs.world_depth`, whose vertex shader has
  `invariant gl_Position`; then the shading pass), fences (`BP_ALPHATEST`), alias models, sprites, opaque particles,
  world text, sky, opaque liquids; then translucency (OIT into accum/revealage, **setting stencil bit 2 on every
  translucent pixel**, resolved with a full-screen pass), heat haze (screen-space distortion from a copy of the scene),
  the VR particle system (`gfx::draw`, premultiplied, no depth writes), then the view model (hidden in VR).
- Stencil bits in use: 1 (sky stencil mode, `gl_sky.c`), 2 (OIT coverage). Bits 4..128 are free.
- `R_WarpScaleView` (after the scene): MSAA resolve, Ironwail's integer `r_scale` upscale (nearest), water warp and
  the view blend (`v_blend`, damage/pickup flashes) into the composite.
- Brush entities: per-instance 3x4 matrix in `InstanceBuffer` (`Instance.mat`). Alias models: per-instance
  `WorldMatrix`, `Pose1`, `Pose2`, `Blend` (+ the VR zero-blend in `Padding` and the morph in `Ambient[2].w`); the
  vertex shader blends two poses. IQM models blend bone matrices from a pose buffer.
- Liquids: the swell and splash ripples displace the vertices from `Time`, `Water2.w`, `Ripple*`, `RippleAt[32]`,
  `RippleAmp[8]` (`LIQUID_SWELL`); the texture warp itself is animated in the fragment shader.
- The VR hands, weapons, body, held objects and gadget are ordinary alias entities (`view::ViewEntity::ent`,
  persistent objects added to `cl_visedicts` once per `host_framecount`), placed from the controller poses predicted
  for the frame's display time.
- MSAA (`vid_fsaa`, the "Anti-aliasing" menu entry: Off/2x/4x/8x) is today's anti-aliasing. `gl_lodbias auto` derives a
  mip bias from MSAA (`TexMgr_LodBias_f`), set on Ironwail's samplers.
- Precedents in the tree for loading optional native libraries at run time: `Quake/steam.c` (`steam_api`, Ironwail
  upstream) and `Quake/vr/vr_gpustats.cpp` (`nvml.dll`, installed with NVIDIA's driver). Neither is shipped by us.

## 5. Design: motion vectors

### 5.1 Convention

One `RG16F` texture per eye at the **render** size: `mv = uv_previous - uv_current` (screen UV, 0..1 over the eye),
computed from **unjittered** current and previous view-projections, so the jitter is not in the vectors. DLSS takes
this with `MVScale = (renderWidth, renderHeight)` and "not jittered"; FSR 2/3 takes it with the same scale (its
convention is also previous minus current, in pixels after scaling); our TAA reads it directly. Depth goes with it
as is (reversed Z, `glClipControl` 0..1: DLSS's "depth inverted" flag, FSR's `INVERTED_DEPTH`).

### 5.2 Two sources: camera motion from depth, object motion from a motion pass

Most pixels only move because the eye moves: the world, static brush models, fences, decals, world text, opaque
liquids without waves, gibs at rest. Their motion is fully determined by the depth buffer and the two
view-projections, so it is reconstructed in a full-screen pass; nothing in the heavy shading shaders changes. Only
things that move by themselves write their own vectors, in a separate **motion pass** after the opaque scene.

1. **Clear** the eye's MV texture to a sentinel (e.g. `+65504`, "not written") with the scene's clear.
2. **Motion pass** (after the opaque scene, before translucency; its own FBO: MV texture + the scene's
   depth/stencil, depth test `GL_EQUAL`, no depth writes): redraws only what moves on its own, with small vertex
   shaders that compute both the current and the previous clip position and a fragment shader that writes their
   difference. Because the depth is already final and the test is `EQUAL`, only the visible surface of each moving
   object writes: nothing drawn later in front of it can leave a stale vector (the problem an MRT output in the main
   shaders has: opaque liquids and sky are drawn after alias models). `EQUAL` needs the motion pass's current clip
   position to be bit-identical to the shading pass's: `invariant gl_Position` and the same code path (already done
   for the world pre-pass in round 19; the alias vertex shader needs it too). What it draws:
   - **Alias models** (monsters, items, the VR hands, weapons, body, held objects, shells, gibs): current
     `WorldMatrix`/`Pose1`/`Pose2`/`Blend`/zero-blend, and the **previous frame's drawn values** of the same.
     IQM (skeletal) models: the previous frame's bone palette (keep last frame's pose buffer range per entity), or,
     first cut, the previous model matrix with the current pose.
   - **Brush entities whose matrix changed** since the previous frame (doors, plats, trains, rotators; Quake VR's
     rigid bodies). Static ones are left to the camera pass.
   - **Opaque liquids with geometric waves** (`vr_water_geo_waves`, ripples): `LiquidDisplace` at `Time` and at the
     previous frame's `Time` with the previous ripple set (add `PrevTime` and the previous `Ripple*` block to the frame
     UBO, or accept "ripple ages minus dt" as the previous state). Translucent liquids go to the reactive mask instead.
   - **Sky**: drawn with a direction-only reprojection (w = 0): the sky is rendered as infinitely far, so the depth
     of the sky brushes would give it translational parallax it does not have.
3. **Camera pass** (compute, full screen, per eye): every sentinel pixel gets
   `uv_prev = project(PrevViewProj_unjittered, unproject(InvViewProj_unjittered, uv, depth))`. Pixels at the far clear
   (depth 0 in reversed Z) reproject as directions. Hidden-area pixels (depth at the near plane) get 0. The same pass
   builds the **reactive mask** (5.4) and, for our TAA, the dilated vectors (7).

### 5.3 Previous-frame state

- **Per eye**: `prevViewProj[eye]` and `viewProj[eye]` (both unjittered), saved when the eye's matrices are built
  (`R_SetupView` via `VR_OverrideProjection`/`R_SetFrustum`). OpenXR gives each eye a new pose every frame; the
  previous one is simply the matrix used for that eye one VR frame ago. Locomotion, snap turns, teleports and head
  motion are all in it (the matrices are in Quake world space).
- **Per entity**: a C++ side table in a new `qvr::temporal` module keyed by `entity_t*` with `{model, frame stamp,
  current and previous: matrix 3x4, pose1, pose2, blend, zero-blend word, morph}`. On the first draw of an entity in
  VR frame N the current values shift into previous; both eyes then use the same pair (entity transforms do not
  differ between the eyes). If the stamp is not N-1, the model changed, or Quake's `LERP_RESETMOVE`/`LERP_RESETANIM`
  says the entity jumped, previous = current (no motion rather than a wrong streak). Hooks: one call where
  `r_alias.c` fills an instance (next to `VR_AliasZeroBlend`) and one in `R_InitBModelInstance`.
- **Per frame**: `PrevTime` and the ripple block for liquids; a "history reset" flag (7.4).
- The hands and weapons need nothing special: their previous transform is what was drawn last frame, which is what
  a temporal reprojection wants. (Their poses are predicted for display time, so their frame-to-frame motion is smooth;
  melee swings of 5+ m/s at 120 Hz are 4+ cm a frame, which is where DLSS's quality on fast thin objects will show.)

### 5.4 What gets no vectors: the reactive/transparency mask

An `R8` mask per eye at render size, 1 where the colour is not explained by the motion vectors, which tells the
temporal filter to trust the current frame there (DLSS: `BiasCurrentColor` mask; FSR 2/3: reactive mask, plus the
optional transparency-and-composition mask; our TAA: a higher blend weight):

| source | how it is marked |
|---|---|
| OIT translucency (translucent liquids, `r_wateralpha`, translucent entities and particles) | already: stencil bit 2, set by `R_BeginTranslucency` |
| VR particles, explosions, beams, sprites, decals-in-the-air (`gfx::draw` with blending) | new stencil bit 4 set where a blended `gfx` draw passes the depth test (`gfx::State` gets a `reactive` flag, on by default for blended states) |
| heat haze (screen-space distortion) | stencil bit 4 in its draw |
| liquid surfaces' animated texture warp, lava glow, animated sky layers | a small constant (e.g. 0.25–0.5) from the motion pass or a stencil bit 8 |
| alias skins with fullbright flicker, muzzle flashes | nothing at first; the flash is an alias model and has vectors |

The camera pass reads the stencil through a stencil texture view of the depth/stencil texture
(`GL_DEPTH_STENCIL_TEXTURE_MODE = GL_STENCIL_INDEX`, GL 4.3) and writes the mask. A binary mask is enough to start;
a graded one (max alpha of the particles into an `R8` MRT with `GL_MAX` blending) is a refinement.

### 5.5 Cost and memory

MV `RG16F` 4 bytes a pixel: 46 MB an eye at 3292x3524, written once by the camera pass (plus the few dynamic pixels):
about 0.05–0.1 ms an eye on a 4090 (bandwidth bound). The motion pass redraws a few dozen alias models and moving
brush models: well under 0.05 ms. Mask `R8`: 12 MB an eye.

### 5.6 Debugging aids (part of the stage)

- `vr_temporal_debug 1`: show the vectors as colours in the mirror; `2`: the reactive mask; `3`: "reprojection test":
  the previous frame's final colour fetched through the vectors and shown instead of the current frame (a correct
  vector field makes this almost indistinguishable from the real frame while walking and turning; wrong vectors show
  as tearing on the object that is wrong).
- `vr_eyeshot` already saves the eyes; add the MV and mask textures to it.

## 6. Design: jitter

- **Where**: in `VR_OverrideProjection`, after the asymmetric terms: a sub-pixel offset `(jx, jy)` in render pixels,
  added as `2*jx/renderWidth` and `2*jy/renderHeight` to the clip x/y terms multiplied by the view's depth axis (Quake
  view space is x forward: the `matrix[0*4+0]`/`matrix[0*4+1]` entries already hold the off-axis shift). The
  unjittered matrix is kept for the motion vectors and for the frame's "current" view-projection. Sign conventions
  must be checked against DLSS's jitter overlay (the DLSS SDK's debug overlay draws the jitter it receives) and
  FSR's `ffxFsr3GetJitterOffset` convention.
- **Sequence**: Halton(2,3), centred to (-0.5, 0.5). Phase count: `max(16, ceil(8 * (display/render)^2))` for DLSS
  (its guide asks for 16 or more, 32+ preferred, and tabulates 18/24/32/72 by mode), the same rule without the
  minimum for FSR, 8 for our native TAA. Advance it once per VR frame (`host_framecount`), not per eye.
- **Both eyes: same offset**. Different per-eye patterns would make each eye's residual shimmer differ, which in
  stereo reads as binocular rivalry (the same argument as in LIGHTING.md: "both eyes must see the same thing"). A
  debug cvar can decorrelate them to test.
- **Only while a temporal resolve runs.** Jitter without a resolve is visible as 1-pixel wobble; the submitted
  `XrCompositionLayerProjectionView` keeps the unjittered pose and FOV (the resolved image is unjittered).
- **Not jittered**: shadow maps (their own light matrices; world-space, the same for both eyes); the hidden-area mesh
  (drawn in clip space with an identity matrix; black, never seen); the UI (drawn after post-processing at the image
  size); the mirror; the envmap faces (`envmap::update`, its own cube camera).
- **Consistent with jitter automatically**: everything that works in the eye's screen space from `gl_Position`
  (light clusters' `out_coord`, water refraction reading the opaque scene, the haze copy, soft particles) since it
  is all the same jittered frame.
- **Screen-space noise**: the scene shaders' `SCREEN_SPACE_NOISE`/`SUPPRESS_BANDING` dither (the eyes dither in
  post-processing; `VR_SceneDither` keeps the scene's small) should be off in the scene with a temporal mode, or
  keyed by frame so the filter averages it rather than smearing it. Parallax's per-pixel ray offsets likewise.
- **Mip bias**: with upscaling, textures must be sampled as at the display resolution. Both DLSS and FSR give
  `log2(render/display) - 1` (DLSS: plus the native bias; back off towards `log2(render/display)` if textures
  flicker). Hook it into `gl_lodbias auto` (`TexMgr_LodBias_f`) next to the MSAA term. Native TAA/DLAA: 0, or a
  slight negative bias to taste. Our parallax and detail textures sample with explicit LODs in places
  (`textureLod`), and those need the same bias added by hand.
- **MSAA off**: temporal modes use one sample (DLSS and FSR take single-sample inputs; MSAA resolve before a temporal
  filter only costs time). `vid_fsaa` is ignored in the eyes while a temporal mode is on, and alpha-to-coverage
  fences fall back to alpha test (the temporal filter smooths their edges).

## 7. Design: our TAA (the validation step)

Purpose: prove the vectors, jitter, mask, history and reset logic on our own code, where every intermediate can be
inspected, before a black-box upscaler is fed the same inputs. It is also a real option on its own: DLAA-like
anti-aliasing at native resolution on any GPU.

### 7.1 Algorithm (one compute dispatch per eye, 16x16 tiles with a 1-pixel apron in shared memory)

1. **Inputs**: scene colour (HDR float with `vr_tonemap`, linear), depth, MV (5.2), mask (5.4), history (per eye,
   `RGBA16F`, display size).
2. **MV dilation**: use the vector of the nearest-depth pixel in the 3x3 neighbourhood (Karis 2014), so silhouettes
   of moving objects carry their motion.
3. **History fetch**: at `uv + mv` (previous minus current). Use Catmull-Rom in 5 bilinear taps. Out of bounds
   or across the hidden-area edge, treat it as a disocclusion.
4. **Neighbourhood clamp**: 3x3 in YCoCg, variance clipping (mean ± γ·σ, γ ≈ 1; Salvi 2016), clipping towards the
   mean rather than clamping per channel.
5. **Blend**: `α = 1/8..1/10` by default; raised with the mask (reactive → α up to 1), with disocclusion (1), and a
   little with the pixel's velocity; luminance-weighted blend (`1/(1+luma)`, Karis) against HDR fireflies from
   explosions and specular.
6. **Output**: new history, and the resolved colour into the composite for `R_WarpScaleView`/post-processing.
   Sharpening: an RCAS-style pass in post-processing (the FSR 1 work already brings RCAS).

### 7.2 Where it runs

Between `R_RenderScene` and `R_WarpScaleView` in `R_RenderView`, only for VR eyes (a `VR_TemporalResolve()` hook):
before the water warp, the view blend, bloom, the tone curve and the underwater wobble, so none of those are
reprojected, and after all scene drawing (the view model is hidden in VR).

### 7.3 Cost at 3292x3524

Per eye and frame the pass touches roughly colour 3x3 (shared memory), history 5 bilinear taps, depth 3x3, MV 1,
mask 1, writes history + output: on the order of 40–60 bytes a pixel of real memory traffic, 0.5–0.7 GB per eye.
On a 4090 (about 1 TB/s) that is about **0.3–0.5 ms an eye, 0.6–1.0 ms a frame**, plus the camera/mask pass
(0.1–0.2 ms a frame) and the motion pass (<0.05 ms): roughly **1 ms of the 8.33 ms budget**. Compare with 4x MSAA at
that size (to be measured with `vr_profile`; MSAA's cost is in every scene pass and in the resolve). Memory: history
92 MB an eye (`RGBA16F`), two per eye if ping-ponged (370 MB), MV + mask 58 MB an eye: about 0.5 GB of the 24 GB.

### 7.4 History resets

Reset (α = 1 for the frame) on: map load, teleport (`vr_teleport`), a snap turn larger than a threshold (optional:
rotation-only reprojection is exact, but disocclusion at 45° turns is large), respawn, the menu's live preview
starting or stopping, render size or temporal mode change, `vr_render_scale` change, any frame where an eye was not
rendered (the runtime skipped it) or the previous frame is older than N ms (the headset was taken off). The same
flag drives DLSS's `Reset` and FSR's `reset`.

### 7.5 VR-specific notes

- Reprojection by the runtime runs on the final, unjittered images after submission, so it does not interact with
  the jitter. Virtual Desktop's SSW and SteamVR's motion smoothing estimate motion from the encoded/final colour,
  not from our vectors. It does amplify any ghosting the temporal filter leaves: a ghost trail becomes a warped ghost
  trail. At a steady 120 Hz without SSW this is moot.
- Temporal filters soften: expect to want sharpening (RCAS) and a slight negative mip bias.
- The known complaint about TAA in VR is smear on head motion; with correct per-eye vectors and a clean jitter this
  mostly comes from disocclusion and the clamp. That is precisely what this stage is meant to measure on the HMD.

## 8. Design: the upscaler slot in the eye pipeline

Today, with `vr_render_scale < 1`, the whole eye (scene and post-processing) runs at the render size and the result
is blitted (FSR 1 in stage 1) into the image. Temporal upscalers have to sit **before** post-processing, on the
linear HDR scene, and output at the display size; bloom, the tone curve, the underwater wobble and the grade then
run at the display size. So the eye framebuffers split into two sizes:

- **render size**: scene colour, depth/stencil, OIT accum/revealage, MV, mask (what `R_RenderScene` draws);
- **display size**: the temporal output (= DLSS/FSR output or TAA history), composite, bloom chain, post-processing.

`R_WarpScaleView` then reads the temporal output (display size, no `r_scale`). Stage 1's FSR 1 stays the path for
"no temporal mode" (post-tonemap spatial upscale, where FSR 1 belongs), so the two do not conflict, but both touch
`ensureEyeFramebuffers`/`VR_RenderView` in `vr_stereo.cpp`: whoever lands second rebases. `vr_render_scale` keeps its
meaning (render size = scale × image size); a temporal mode adds presets (DLAA 1.0, Quality 0.667, Balanced 0.58,
Performance 0.5 per axis).

Per-eye inputs: DLSS and FSR keep their own history per feature/context, one per eye. The scene textures can stay
shared between the eyes if the upscaler runs right after each eye's scene (simplest, and what the first cut should
do). Batching both eyes into one upscaler submission (one interop round trip per frame instead of two) needs
per-eye copies of the inputs (or per-eye scene framebuffers, +~200 MB at native size): an optimisation for later.

## 9. Design: the DLSS path (interop module)

### 9.1 Shape

```
ironwail.exe (GPL-2, OpenGL)                       qvr_dlss.dll / qvr_fsr.dll (optional, loaded at run time,
                                                   our own MIT source; one ABI, one backend each)
------------------------------------------         ----------------------------------------------------------
vr_upscale.cpp: LoadLibrary("qvr_dlss.dll"),       C ABI, versioned (qvr_upscale_api.h, a header of ours,
  else "qvr_fsr.dll"                                MIT so both sides may include it):
  queries GL_DEVICE_UUID_EXT/LUID                   create(device uuid/luid, per-eye sizes, mode) -> handle
  imports shared images (GL_EXT_memory_object)      shared images: Win32 NT handles + sizes/formats per eye:
  imports semaphores (GL_EXT_semaphore)              colour RGBA16F, depth R32F, MV RG16F, mask R8 (render),
  per eye: copy/render inputs, signal               output RGBA16F (display)
    -> evaluate(eye, jitter, mv scale, reset, dt)   evaluate(): vkQueueSubmit waiting "gl done", run the
    -> wait, post-process from the output            upscaler, signal "vk done"
                                                    backend: NGX DLSS (Vulkan) | FSR 3.1 (FidelityFX, Vulkan)
```

The shared part (Vulkan device by UUID, exportable images and semaphores, per-eye command buffers) is common MIT
source compiled into both DLLs. Only the backend file differs, so `qvr_fsr.dll` contains nothing of NVIDIA's and can
ship with the engine. `qvr_dlss.dll` is a separate download (section 3.3).

Out-of-process variant (if the author wants the stronger separation): the same module becomes `qvr_dlss_host.exe`.
It creates the images and semaphores as NT handles and duplicates them into the game process. Per eye, the engine
writes a small command (eye, jitter, reset, dt) into shared memory and sets an event, and the host submits and
signals the shared semaphore. The GL side is unchanged apart from where the handles come from. Extra work: process
start and stop with the VR session, crash handling (fall back to TAA), and handle duplication.

- The Vulkan side **creates** the images and semaphores (exportable, dedicated allocations, `VK_IMAGE_TILING_OPTIMAL`)
  and the GL side imports them (`glCreateMemoryObjectsEXT`, `glImportMemoryWin32HandleEXT`,
  `glTextureStorageMem2DEXT`; `glImportSemaphoreWin32HandleEXT`). This is the direction NVIDIA's interop samples use.
- The VkDevice must be on the same GPU as the GL context: match `GL_DEVICE_UUID_EXT`/`GL_DRIVER_UUID_EXT` with
  `VkPhysicalDeviceIDProperties`.
- Depth is copied into an `R32F` colour image rather than sharing `D24S8` (shared depth/stencil formats are the least
  tested corner of interop; a copy is ~0.03 ms). Colour/MV/mask can be the render targets themselves later.
- Per eye: GL renders the scene, writes the inputs, `glSignalSemaphoreEXT` (with the images' layouts),
  `evaluate()` (CPU: records/submits a Vulkan command buffer; GPU: the Vulkan queue waits for GL), then
  `glWaitSemaphoreEXT` and GL continues with post-processing. No CPU waits; the GPU serialises GL → Vulkan → GL,
  which is what we want anyway.
- NGX:
  - Load the driver's NGX core with **`nvsdk_ngx_loader.h`** (header-only, DLSS SDK 310.9.1+) instead of linking
    `nvsdk_ngx_s.lib`, then call `NVSDK_NGX_VULKAN_Init_with_ProjectID`. The Vulkan instance and device must be
    created with the extensions NGX asks for (its feature-requirement query), so the helper owns its own
    VkInstance/VkDevice.
  - Create **one DLSS feature per eye** (DLSS PG §3.17) with the render and display sizes and the flags
    `DepthInverted`, `MVLowRes` (render-size vectors), `IsHDR` (our scene is linear float with `vr_tonemap`), and
    `AutoExposure`. Preset L forces auto exposure; J/K can take a 1x1 exposure texture, and 1.0 is fine for
    `vr_tonemap`'s pre-exposed scene.
  - Evaluate with the jitter offsets, the MV scale, `Reset`, and the bias-current-colour mask.
  - Choose the preset (K for DLAA/Quality, M for Performance, L for Ultra Performance, as the guide defaults) through
    the NGX parameters, with a cvar override.
  - Report the loaded `nvngx_dlss.dll` version and preset to the console and to `vr_profile`.
  - Look for `nvngx_dlss.dll` next to the executable. NGX also searches its own paths.
- FSR 3.1 backend in the same module (FidelityFX SDK, Vulkan backend, MIT): upscaler only (no frame generation: the
  OpenXR runtime owns frame pacing).
- Fallbacks: no module, no NVIDIA GPU, no `nvngx_dlss.dll`, no `GL_EXT_memory_object_win32`: the menu entry is
  disabled with the reason; the engine falls back to our TAA (native) or FSR 1.

### 9.2 Work items

1. `qvr_upscale_api.h`: the C ABI (tiny: create/destroy/resize, get shared handles, evaluate, query caps and
   versions, a log callback).
2. `vr_upscale.cpp` in the engine: loading, GL imports, per-eye input preparation, semaphores, cvars/menu, profile
   scopes ("upscale L/R": GL timer queries around the waits measure the Vulkan work as well).
3. The module: Vulkan instance/device selection by UUID, exportable image/semaphore creation, command buffers per
   eye, the NGX backend, the FSR 3.1 backend, a test harness (the module can be exercised by a small GL test program).
4. Build: separate CMake targets, off by default (`QVR_BUILD_DLSS=ON` with `DLSS_SDK_DIR`, `QVR_BUILD_FSR=ON` with
   `FIDELITYFX_SDK_DIR`), not part of the engine's default build. NVIDIA's headers are never copied into our tree.
   Distribution follows section 3.3.

## 10. Staged plan and effort

Effort is given in agent rounds (a round is one of our feedback rounds, like ROUND19/20). Each stage ends with
something the author can see on the HMD.

| stage | content | effort | done when |
|---|---|---|---|
| **2a Motion vectors** | new `vr_temporal.cpp/.hpp`; per-eye current and previous unjittered view-projections; the per-entity side table and its hooks in `r_alias.c`/`r_world.c`; the MV and mask textures in the eye framebuffers; the motion pass (alias, moved brush entities, opaque swelling liquids, sky); the camera/mask compute pass; stencil bit 4 for blended `gfx` draws and haze; `invariant gl_Position` in the alias vertex shader; `vr_temporal_debug` views 1–3; MV/mask in `vr_eyeshot` | 1 round | the reprojection test (debug 3) holds still while walking, turning, riding a lift, swinging a weapon, watching a monster and standing in waves, **in each eye separately** |
| **2b Jitter + TAA** | Halton jitter in `VR_OverrideProjection` (only with a temporal mode); the TAA compute pass and per-eye history; resets; MSAA off and alpha-to-coverage fallback in temporal modes; scene-dither off; `vr_aa_mode` (MSAA / TAA) in the Graphics menu; profile scopes; RCAS sharpening strength | 1 round, plus 0.5 round of HMD tuning | TAA at native looks at least as clean as 4x MSAA on e1m1's railings and fences at comparable cost, with no ghosting on the hands |
| **2c Upscaler slot** | the render-size/display-size split of the eye framebuffers (section 8); `R_WarpScaleView` from the temporal output; mip bias from the upscale ratio; presets; rebase on stage 1's FSR 1 | 0.5 round | TAA runs with render scale 1.0 and the post-processing at display size; FSR 1 still works without a temporal mode |
| **3a Interop + DLSS** | `qvr_upscale_api.h`; `vr_upscale.cpp` (load, import, semaphores, per-eye evaluate); the helper's Vulkan core and NGX backend; `DLAA`/`Quality`/`Balanced`/`Performance` modes and a preset cvar; failure reasons in the menu; a CMake target off by default; documentation of how to get `nvngx_dlss.dll` | 1–1.5 rounds | DLAA and DLSS Quality in the HMD; per-eye GPU time of the DLSS work in `vr_profile` |
| **3b FSR 3.1 (optional)** | the FidelityFX Vulkan backend in `qvr_fsr.dll`, shipped with the engine | 0.5–1 round | FSR 3.1 Quality in the HMD on the same inputs |
| 3c Out-of-process helper (optional) | section 9.1's variant | 1 round | same as 3a, with the helper in its own process |

The total for the recommended path (2a, 2b, 2c, 3a) is about **4–4.5 rounds**, plus about 1 round for FSR 3.1 and
another for out-of-process, if wanted.

Why this order:

- The vectors are the part everyone gets wrong, and every upscaler needs them. Our own TAA shows mistakes plainly
  and is useful on its own on any GPU.
- The render/display split is needed by every temporal upscaler, so it comes before the helper.
- DLSS before FSR because it is the author's target and developing it privately raises no licence issue. FSR 3.1 is
  cheap once the interop exists.
- The FSR 2 OpenGL port is not recommended: it is slow, has a known depth bug, and is an older FSR.

## 11. Risks

- **DLSS cost at our resolution.** 3–4.7 ms for both eyes with the transformer presets (scaled from NVIDIA's 4K
  table) is a large part of 8.33 ms. At 120 Hz, DLAA may not fit next to the scene. Mitigations: DLSS Quality or
  Balanced (the cost follows the output size, so it barely drops), the CNN preset if still offered (about 1.5 ms),
  90 Hz, or DLSS only where the scene is heavy. Measure it early in 3a with a stub scene.
- **Interop and synchronisation.** Semaphore signal/wait per eye serialises GL and Vulkan. It stays GPU-side, but a
  mistake stalls the CPU (e.g. `glFinish` or waiting on a fence the wrong way) and shows up as a missed frame at
  120 Hz. Depth/stencil sharing is untested territory (we copy depth instead). Two GL memory-object imports per eye
  per size change, and VRAM for two sets of images (inputs, outputs, DLSS's own ~300–450 MB per 4K-equivalent
  feature, times two eyes): about 1.5 GB in all, fine on 24 GB.
- **Quality on Quake content.** Quake's textures are low-frequency, but its lighting flickers (light styles),
  liquids warp and many effects are additive. Where the mask is wrong, expect ghosting on particles, lava and
  teleporters, and smearing on the wrist gadget's CRT screens and ammo counters (animated textures, no vectors). The
  mask needs care: those screens may want the reactive bit too.
- **Hands and weapons.** They move fastest across the view and are closest to the eye. Wrong or missing previous
  transforms (a weapon switch, a morph, two-handed grip hand-offs, the recoil zero-blend) show as streaks exactly
  where the player looks. Every VR-specific vertex modifier in the alias shader (`ZeroBlend`, morph) must be
  evaluated for the previous frame too, or the entity reset.
- **Stereo consistency.** Different reconstruction per eye is visible as rivalry, sparkle in one eye only. Use the
  same jitter in both eyes and validate each eye's vectors on its own (cf. the UE instanced-stereo bug).
- **Interaction with the other agent's stage 1.** Both touch `vr_stereo.cpp`'s eye framebuffers and the
  render-scale path. Land stage 1 first; 2c rebases on it.
- **Licence drift.** NVIDIA can change the licence; Blender's official position is still pending. Keep the helper
  separable so a takedown or change affects only the optional download.
- **Driver/runtime variety.** VDXR vs SteamVR (both GL), non-NVIDIA GPUs (DLSS unavailable; FSR/TAA only), GPUs
  without `GL_EXT_memory_object_win32` (TAA only).

## 12. Open questions for the author

1. **Distribution**: is DLSS for your own use for now (then no licence constraint applies until release), or do you
   want it in public releases? If public: in-process `qvr_dlss.dll` as a separate download (the vkquake-rt/Blender
   precedent), or the stronger out-of-process helper (+1 round)?
2. **Helper licence**: MIT for the helper's own source (and `qvr_upscale_api.h`)? You would be its only author, so
   you can choose it.
3. **Is FSR 3.1 worth a round?** It matters for AMD/Intel users, and our TAA plus FSR 1 may be enough for them.
4. **Default anti-aliasing**: should TAA replace 4x MSAA as the default once it passes, or stay an option? (Meta's
   and Valve's MSAA advice was for sharper, lower-resolution panels and older TAA.)
5. **Performance target**: must DLAA hold 120 Hz on e1m1-class scenes, or is 90 Hz acceptable with DLSS? This
   decides whether DLAA or Quality is the headline mode.
6. **Snap turn and teleport**: reset the history (a one-frame aliased flash) or keep it (brief disocclusion
   ghosting)? This is a comfort and taste call to try on the HMD.
7. **Scope of motion vectors for liquids**: are opaque waving lava and slime common enough in the maps you play to
   justify the previous-frame ripple state? Otherwise liquids go in the reactive mask.
8. **Same jitter in both eyes** (recommended), or also try decorrelated jitter as an experiment?

## 13. Sources

DLSS and NVIDIA:

- DLSS SDK releases: https://github.com/NVIDIA/DLSS/releases
- DLSS Programming Guide (v310.6.0 in the repo): https://github.com/NVIDIA/DLSS/blob/main/doc/DLSS_Programming_Guide_Release.pdf
- NVIDIA RTX SDKs License (14 Mar 2024): https://raw.githubusercontent.com/NVIDIA/DLSS/main/LICENSE.txt
- NGX run-time loader header: https://raw.githubusercontent.com/NVIDIA/DLSS/main/include/nvsdk_ngx_loader.h
- Streamline (licence MIT with exceptions): https://github.com/NVIDIA-RTX/Streamline , https://raw.githubusercontent.com/NVIDIA-RTX/Streamline/main/license.txt
- DLSS 4.5 announcement: https://www.nvidia.com/en-us/geforce/news/dlss-4-5-dynamic-multi-frame-gen-6x-2nd-gen-transformer-super-res/
- DLSS 5: https://nvidianews.nvidia.com/news/nvidia-dlss-5-delivers-ai-powered-breakthrough-in-visual-fidelity-for-games , https://www.nvidia.com/en-us/geforce/news/dlss-5-3d-guided-neural-rendering/
- DLSS 5 DLL leak: https://www.techpowerup.com/352033/nvidia-dlss-5-dll-leaked-by-nba-2k27-early-access-build-heres-our-analysis , https://heldgames.com/guides/dlss-5-swapper-emulators

Licensing precedents:

- Quake II RTX and DLSS/GPL: https://wccftech.com/quake-ii-rtx-patch-adds-amd-fsr-hdr-support-dlss-cant-be-added/ , https://www.techspot.com/news/93124-quake-ii-rtx-receives-support-hdr-amd-fsr.html
- vkquake-rt releases (DLSS zip, user-downloaded `nvngx_dlss.dll`): https://github.com/sultim-t/vkquake-rt/releases , https://github.com/sultim-t/RayTracedGL1 , https://github.com/sultim-t/prboom-plus-rt/releases
- Blender DLSS PR and discussion: https://projects.blender.org/blender/blender/pulls/153077 , https://www.phoronix.com/news/NVIDIA-DLSS-Blender , https://www.creativebloq.com/3d/nvidias-game-changing-dlss-is-finally-coming-to-blender
- OBS NVIDIA effects: https://github.com/obsproject/obs-studio/pull/7466
- OptiScaler: https://github.com/optiscaler/OptiScaler ; dxvk-nvapi: https://github.com/jp7677/dxvk-nvapi
- Godot DLSS proposal: https://github.com/godotengine/godot-proposals/issues/2239
- RBDOOM-3-BFG 1.6.0: https://github.com/RobertBeckebans/RBDOOM-3-BFG/releases/tag/v1.6.0
- GPL-2 text (system library exception): https://www.gnu.org/licenses/old-licenses/gpl-2.0.html
- FSF GPL FAQ (plug-ins, incompatible libraries, system library exception, aggregation): https://www.gnu.org/licenses/gpl-faq.html#GPLPlugins , https://www.gnu.org/licenses/gpl-faq.html#GPLIncompatibleLibs , https://www.gnu.org/licenses/gpl-faq.html#SystemLibraryException , https://www.gnu.org/licenses/gpl-faq.html#MereAggregation

Interop and bridges:

- EXT_external_objects (+ _win32): https://registry.khronos.org/OpenGL/extensions/EXT/EXT_external_objects.txt
- nvpro-samples: https://github.com/nvpro-samples/gl_vk_simple_interop , https://github.com/nvpro-samples/gl_vk_raytrace_interop , https://github.com/nvpro-samples/gl_vk_threaded_cadscene
- Interop pitfalls: https://github.com/vulkano-rs/vulkano/pull/1734 , https://community.amd.com/t5/opengl-vulkan/vulkan-opengl-interoperability-problems/td-p/153904
- DLSS5-Feeder (GL via D3D12): https://github.com/jlrouzies-fr/DLSS5-Feeder/releases/tag/v0.7.0
- dlss5-opengl-bridge: https://github.com/NoelGamer/dlss5-opengl-bridge
- Minecraft: https://github.com/Minecraft-Radiance/Radiance , https://github.com/SimonVitzethum/Vulkanfish , https://github.com/Quiet-Wolfe/VulkanMod-DLSS , https://github.com/MCRcortex/vulkanite

FSR and XeSS:

- FidelityFX SDK: https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK ; FSR 4 status: https://videocardz.com/newz/amd-releases-fidelityfx-sdk-2-0
- FSR 2 README (jitter, mip bias, masks, cost): https://github.com/GPUOpen-Effects/FidelityFX-FSR2/blob/master/README.md
- FSR 2 OpenGL port: https://github.com/JuanDiegoMontoya/FidelityFX-FSR2-OpenGL , https://juandiegomontoya.github.io/porting_fsr2.html
- XeSS: https://github.com/intel/xess , https://raw.githubusercontent.com/intel/xess/main/LICENSE.txt

TAA and VR:

- Karis 2014, High Quality Temporal Supersampling: https://advances.realtimerendering.com/s2014/
- Playdead INSIDE TAA (code and GDC 2016 slides): https://github.com/playdeadgames/temporal
- Tardif, TAA starter pack: https://alextardif.com/TAA.html
- Yang, Liu, Salvi 2020 survey: https://onlinelibrary.wiley.com/doi/abs/10.1111/cgf.14018 ; notes: https://interplayoflight.wordpress.com/2020/05/30/a-survey-of-temporal-antialiasing-techniques-presentation-notes/
- Vlachos, Advanced VR Rendering (GDC 2015): https://media.steampowered.com/apps/valve/2015/Alex_Vlachos_Advanced_VR_Rendering_GDC2015.pdf
- Meta MSAA guidance: https://developers.meta.com/horizon/documentation/native/android/mobile-msaa-analysis/
- Half-Life: Alyx analysis: https://petrakeas.medium.com/half-life-alyx-performance-analysis-or-why-low-graphic-settings-produce-a-sharper-image-4d17fb8c19bb
- Unity URP anti-aliasing: https://docs.unity3d.com/6000.1/Documentation/Manual/urp/anti-aliasing.html
- UE DLSS VR ghosting: https://forums.developer.nvidia.com/t/dlss-plugin-in-unreal-engine-5-6-causing-smearing-and-ghosting-in-vr/337856 ; secondary-eye velocity bug: https://github.com/DisplayXR/displayxr-unreal/issues/55
- VR DLSS titles and reports: https://roadtovr.com/into-the-radius-vr-nvidia-dlss/ , https://www.nvidia.com/en-us/geforce/news/computex-2022-rtx-dlss-game-updates/ , https://forums.flightsimulator.com/t/new-dlss-4-v310-1-is-amazing-in-vr/700364 , https://forum.dcs.world/topic/368163-dlss-4/ , https://msfsaddons.com/2026/01/17/how-to-finally-fix-msfs-2024-cockpit-smearing-with-dlss-4-5/
- Runtime reprojection: https://www.uploadvr.com/virtual-desktop-synchronous-spacewarp/ , https://roadtovr.com/steamvr-motion-smoothing-asw-alex-vlachos/ , https://developers.meta.com/horizon/documentation/unity/unity-asw/
