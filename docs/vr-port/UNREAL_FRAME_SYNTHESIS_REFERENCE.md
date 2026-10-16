# Unreal Engine OpenXR frame synthesis reference

Source audit of Epic's Unreal Engine `5.7` branch at commit
`0bcfaffa52e9557258dbd388835ac7668fe864a4`, with a cross-check against the
`5.8` branch at commit `4dced763583b8fa05d214b133c2c21b73da300d4`. Both are
used as concrete engine integration references for other renderers. This is a
source-code audit, not a claim that an Unreal game was tested on this Steam
Frame or that the OpenGL Steam Frame path supports the requested formats.

The `5.7` audit was checked against the local sparse Unreal source checkout at
`C:\Modding\_ue-spacewarp-audit-5.7`, including the OpenXR plugin, mobile
renderer, and velocity shaders. The checkout's configured Epic remote was also
used to inspect the `5.8` source tree. The main architecture is
unchanged: opt-in OpenXR extension handling, runtime-sized float motion and
depth swapchains, renderer-produced stereo velocity, and per-view synthesis
metadata. This cross-version check makes the porting guidance less dependent on
one UE release. Commit-pinned 5.8 source: [`OpenXRHMD.cpp`](https://github.com/EpicGames/UnrealEngine/blob/4dced763583b8fa05d214b133c2c21b73da300d4/Engine/Plugins/Runtime/OpenXR/Source/OpenXRHMD/Private/OpenXRHMD.cpp),
[`OpenXRHMDModule.cpp`](https://github.com/EpicGames/UnrealEngine/blob/4dced763583b8fa05d214b133c2c21b73da300d4/Engine/Plugins/Runtime/OpenXR/Source/OpenXRHMD/Private/OpenXRHMDModule.cpp),
and [`MobileShadingRenderer.cpp`](https://github.com/EpicGames/UnrealEngine/blob/4dced763583b8fa05d214b133c2c21b73da300d4/Engine/Source/Runtime/Renderer/Private/MobileShadingRenderer.cpp).

I also verified the findings against Epic's authenticated `release` branch at
the `5.8.3 release` commit `396c9f059903aed5fec78ecd3d437a40c6415368` on
2026-10-14, using the `kblood` GitHub credential. The implementation remains
consistent with the pinned 5.8 audit: EXT and FB are mutually exclusive
selection paths; the enabled extension supplies the recommended motion-vector
extent; UE allocates paired float-motion and depth swapchains; the Vulkan
mobile multiview renderer writes both through a dedicated geometry pass; and
the matching per-view projection chains synthesis metadata. The 5.8.3 source
also confirms the color and motion/depth subimages use the matching array
layer, while each swapchain retains its own acquired image index. See the
[5.8.3 extension selection](https://github.com/EpicGames/UnrealEngine/blob/396c9f059903aed5fec78ecd3d437a40c6415368/Engine/Plugins/Runtime/OpenXR/Source/OpenXRHMD/Private/OpenXRHMDModule.cpp#L568-L594),
[recommended target setup and per-view metadata](https://github.com/EpicGames/UnrealEngine/blob/396c9f059903aed5fec78ecd3d437a40c6415368/Engine/Plugins/Runtime/OpenXR/Source/OpenXRHMD/Private/OpenXRHMD.cpp#L1913-L1975),
[swapchain allocation](https://github.com/EpicGames/UnrealEngine/blob/396c9f059903aed5fec78ecd3d437a40c6415368/Engine/Plugins/Runtime/OpenXR/Source/OpenXRHMD/Private/OpenXRHMD.cpp#L2965-L3005),
and [mobile velocity pass](https://github.com/EpicGames/UnrealEngine/blob/396c9f059903aed5fec78ecd3d437a40c6415368/Engine/Source/Runtime/Renderer/Private/MobileShadingRenderer.cpp#L1964-L2025).

## Enablement and renderer constraints

Unreal's `xr.OpenXRFrameSynthesis` console variable is off by default. The source
description says it currently requires the Vulkan mobile renderer, mobile
multiview, and `r.Velocity.DirectlyRenderOpenXRMotionVectors=True`. Because this
mode bypasses normal velocity rendering, Unreal disables temporal anti-aliasing
and motion blur. See [`OpenXRHMD.cpp`](https://github.com/EpicGames/UnrealEngine/blob/0bcfaffa52e9557258dbd388835ac7668fe864a4/Engine/Plugins/Runtime/OpenXR/Source/OpenXRHMD/Private/OpenXRHMD.cpp#L159-L165).

The plugin prefers `XR_EXT_frame_synthesis`; when both EXT and FB are enabled,
`xr.PreferFBSpaceWarp` can select the FB path. The render gate also checks the
shader platform and renderer capability before enabling stereo motion output.
See [`OpenXRHMDModule.cpp`](https://github.com/EpicGames/UnrealEngine/blob/0bcfaffa52e9557258dbd388835ac7668fe864a4/Engine/Plugins/Runtime/OpenXR/Source/OpenXRHMD/Private/OpenXRHMDModule.cpp#L608-L635)
and [`OpenXRHMD.cpp`](https://github.com/EpicGames/UnrealEngine/blob/0bcfaffa52e9557258dbd388835ac7668fe864a4/Engine/Plugins/Runtime/OpenXR/Source/OpenXRHMD/Private/OpenXRHMD.cpp#L1374-L1385).

The OpenXR plugin enables stereo motion-vector rendering only when the renderer
supports OpenXR motion vectors, mobile multiview is active, the cvar is enabled,
and `XR_EXT_frame_synthesis` or `XR_FB_space_warp` was enabled. The renderer then
asks the OpenXR stereo render-target manager for the motion-vector and depth
textures. This connects runtime swapchains to the engine's renderer velocity and
depth outputs instead of estimating flow from final-color images. See
[`OpenXRHMD.cpp`](https://github.com/EpicGames/UnrealEngine/blob/0bcfaffa52e9557258dbd388835ac7668fe864a4/Engine/Plugins/Runtime/OpenXR/Source/OpenXRHMD/Private/OpenXRHMD.cpp#L1374-L1392)
and its renderer texture handoff around lines 3088–3150.

The renderer path is a useful example to adapt to engines other than Unreal:
`VelocityCommon.ush` calculates current-minus-previous screen-space X/Y and
device-depth Z from current and previous transforms, including both camera and
object motion. `VelocityShader.usf` writes those signed float values directly
for stereo motion vectors. `MobileShadingRenderer.cpp` renders opaque and
translucent velocity passes into the dedicated motion/depth targets. Relevant
sources: [`VelocityCommon.ush`](https://github.com/EpicGames/UnrealEngine/blob/0bcfaffa52e9557258dbd388835ac7668fe864a4/Engine/Shaders/Private/VelocityCommon.ush#L9-L21),
[`VelocityShader.usf`](https://github.com/EpicGames/UnrealEngine/blob/0bcfaffa52e9557258dbd388835ac7668fe864a4/Engine/Shaders/Private/VelocityShader.usf#L250-L264),
[`MobileShadingRenderer.cpp`](https://github.com/EpicGames/UnrealEngine/blob/0bcfaffa52e9557258dbd388835ac7668fe864a4/Engine/Source/Runtime/Renderer/Private/MobileShadingRenderer.cpp#L1649-L1711).

## Swapchain allocation and formats

At the runtime-recommended motion-vector size, Unreal requests:

- Motion vectors: `PF_FloatRGBA` (signed floating-point RGBA), renderable and
  shader-readable.
- Motion-vector depth: `PF_DepthStencil`, renderable and shader-readable.
- Two array layers when mobile multiview is active; otherwise one.

See [`OpenXRHMD.cpp`](https://github.com/EpicGames/UnrealEngine/blob/0bcfaffa52e9557258dbd388835ac7668fe864a4/Engine/Plugins/Runtime/OpenXR/Source/OpenXRHMD/Private/OpenXRHMD.cpp#L2914-L2947). The generic OpenXR swapchain helper
queries `xrEnumerateSwapchainFormats`; if the requested format is absent, it
tries `PF_B8G8R8A8` then `PF_R8G8B8A8` for color, and `PF_D24` for depth. It does
not preserve float precision when that requested motion format is missing: the
generic non-depth fallback is an 8-bit RGBA format. The swapchain creation path
has no sRGB-specific motion-vector decode. The OpenGL helper returns no
swapchain if neither fallback is advertised. The Vulkan helper performs its
own RHI-to-`VkFormat` mapping and must be assessed against Vulkan's enumerated
formats. See the [format fallback](https://github.com/EpicGames/UnrealEngine/blob/0bcfaffa52e9557258dbd388835ac7668fe864a4/Engine/Plugins/Runtime/OpenXR/Source/OpenXRHMD/Private/OpenXRHMD_Swapchain.cpp#L127-L187),
[OpenGL creator](https://github.com/EpicGames/UnrealEngine/blob/0bcfaffa52e9557258dbd388835ac7668fe864a4/Engine/Plugins/Runtime/OpenXR/Source/OpenXRHMD/Private/OpenXRHMD_Swapchain.cpp#L377-L399),
and [Vulkan creator](https://github.com/EpicGames/UnrealEngine/blob/0bcfaffa52e9557258dbd388835ac7668fe864a4/Engine/Plugins/Runtime/OpenXR/Source/OpenXRHMD/Private/OpenXRHMD_Swapchain.cpp#L447-L474).

The fallback is an implementation detail, not proof that an 8-bit fallback is
safe motion data. The runtime's actual per-graphics-API format list must be
checked. For the Quake port's tested OpenGL session on Steam Frame, the list had
only sRGB color formats and depth formats. A Vulkan Frame session separately
advertised signed FP16 motion and D32 depth and accepted a moving-pattern
frame-loop fixture. Do not infer a format list from another graphics API.

## Layer submission

When the renderer provides both textures, Unreal marks the frame to submit a
motion-vector layer. In the EXT path it chains `XrFrameSynthesisInfoEXT` onto
each matching projection view, requests the relaxed frame interval, uses
identity motion scale and zero offset, submits the app-space tracking transform,
and provides depth range 0..1 with reversed-depth near/far metadata. See
[`OpenXRHMD.cpp`](https://github.com/EpicGames/UnrealEngine/blob/0bcfaffa52e9557258dbd388835ac7668fe864a4/Engine/Plugins/Runtime/OpenXR/Source/OpenXRHMD/Private/OpenXRHMD.cpp#L1516-L1541). The FB fallback builds a separate
`XrCompositionLayerSpaceWarpInfoFB` structure at lines 1543–1564.

Unreal's code is useful as a renderer-integration pattern:

1. Let the runtime recommend motion/depth resolution.
2. Allocate runtime-owned motion/depth swapchains in renderer-compatible formats.
3. Expose those images through the engine's render-target manager.
4. Reuse the renderer's native velocity and depth passes.
5. Chain the correct EXT or FB structure per projection view at submission.

The important difference from Quake's current prototype is step 4. Quake currently
captures scene depth and estimates dynamic image motion with extra optical-flow
passes. UE's native path gets velocity from the renderer, avoiding that particular
flow-estimation pass and its image-matching failure modes.

Unreal also provides a useful caution: directly-rendered stereo velocity replaces
the normal velocity path, so its TAA and motion blur are switched off. Engines
should audit every renderer feature that consumes or writes velocity before
reusing a native buffer; frame-synthesis vectors may have a different format,
resolution, coordinate convention, and lifetime than an engine's ordinary TAA
velocity texture.

## Cross-version and cross-engine takeaways

The `5.8` source check retains the `xr.OpenXRFrameSynthesis` opt-in and the
Vulkan mobile + multiview + direct-velocity renderer requirement. It still
allocates motion at the runtime-recommended dimensions in `PF_FloatRGBA` and
matching depth, and submits the EXT motion scale/offset, app-space pose delta,
depth range, and near/far values per projection view. The submission path still
keeps a separate `XR_FB_space_warp` branch. See the [5.8 swapchain allocation](https://github.com/EpicGames/UnrealEngine/blob/4dced763583b8fa05d214b133c2c21b73da300d4/Engine/Plugins/Runtime/OpenXR/Source/OpenXRHMD/Private/OpenXRHMD.cpp#L2965-L3005)
and [5.8 per-view submission](https://github.com/EpicGames/UnrealEngine/blob/4dced763583b8fa05d214b133c2c21b73da300d4/Engine/Plugins/Runtime/OpenXR/Source/OpenXRHMD/Private/OpenXRHMD.cpp#L1531-L1552).

For another engine, the portable part is the boundary and data flow, not UE's
RHI classes or its Vulkan-only renderer gate: query the runtime, allocate
runtime-owned images in formats the active graphics binding supports, have the
renderer write current NDC position minus previous NDC position and matching
depth, and attach fresh metadata to each projection view. Keep
implementation-specific format selection, synchronization, multiview, and
temporal-history handling inside that engine's renderer/backend. Unreal is
valuable here because its source is
available for comparison, but its support constraints should not be mistaken
for OpenXR requirements. The 5.8 depth-pass correction is a meaningful
correctness fix even though the overall integration architecture remains
consistent with 5.7.

### Source-audit pitfalls to avoid copying

- The UE 5.7 mobile motion pass clears the depth target again for the
  translucent pass, discarding opaque depth; UE 5.8 uses a shared load action so
  the second pass preserves the first pass's depth. A port that separates
  opaque and translucent velocity passes must clear motion/depth once and load
  them on later passes. Compare the [5.7 pass](https://github.com/EpicGames/UnrealEngine/blob/0bcfaffa52e9557258dbd388835ac7668fe864a4/Engine/Source/Runtime/Renderer/Private/MobileShadingRenderer.cpp#L1649-L1711)
  with the [5.8 pass](https://github.com/EpicGames/UnrealEngine/blob/4dced763583b8fa05d214b133c2c21b73da300d4/Engine/Source/Runtime/Renderer/Private/MobileShadingRenderer.cpp#L1964-L2025).
- Both audited OpenXRHMD revisions update the previous tracking-to-world
  transform inside the per-view submission loop. Since the transform is shared
  by both eyes, this can make the second eye's app-space delta identity. For a
  multi-view engine, compute the delta once from consecutive frame snapshots,
  convert its translation to OpenXR meters, submit the same valid frame delta
  for every view, then advance history once. See the [5.8 submission loop](https://github.com/EpicGames/UnrealEngine/blob/4dced763583b8fa05d214b133c2c21b73da300d4/Engine/Plugins/Runtime/OpenXR/Source/OpenXRHMD/Private/OpenXRHMD.cpp#L1531-L1552)
  and UE's [`ToXrPose` scale default](https://github.com/EpicGames/UnrealEngine/blob/4dced763583b8fa05d214b133c2c21b73da300d4/Engine/Plugins/Runtime/OpenXR/Source/OpenXRHMD/Public/OpenXRCore.h#L93-L108).
- UE 5.8 sizes both-eye motion resources from view 0's recommended rectangle.
  Do not assume every runtime has identical per-view recommendations: validate
  every view, array layer, and rectangle before sharing one allocation. See the
  [5.8 size selection](https://github.com/EpicGames/UnrealEngine/blob/4dced763583b8fa05d214b133c2c21b73da300d4/Engine/Plugins/Runtime/OpenXR/Source/OpenXRHMD/Private/OpenXRHMD.cpp#L1962-L1970).
- UE 5.7 asserts that motion and depth swapchain indices match, while 5.8
  removes that assertion. Treat each swapchain's acquired image index as
  independent unless the runtime/API contract explicitly couples them; pair
  images by frame ownership and matching view rectangle, not by coincidental
  index equality. See the [5.7 assertion](https://github.com/EpicGames/UnrealEngine/blob/0bcfaffa52e9557258dbd388835ac7668fe864a4/Engine/Plugins/Runtime/OpenXR/Source/OpenXRHMD/Private/OpenXRHMD.cpp#L2758-L2766).
- UE's generic swapchain helper may fall back from float motion to an 8-bit
  color format. That fallback is not a signed-vector encoding. If the runtime
  does not advertise a format with a verified vector representation, disable
  synthesis and continue ordinary projection rendering. See the [5.8 format
  fallback](https://github.com/EpicGames/UnrealEngine/blob/4dced763583b8fa05d214b133c2c21b73da300d4/Engine/Plugins/Runtime/OpenXR/Source/OpenXRHMD/Private/OpenXRHMD_Swapchain.cpp#L126-L187).
- The dedicated velocity path includes stationary visible geometry, not just
  moving actors: camera motion still moves the image of a stationary wall.
  Unreal's renderer notes that its differently sized scene-depth buffer cannot
  be reused to derive flattened velocity for stationary objects. Budget the
  full visible-geometry velocity pass, or use a rigorously matched depth-based
  camera-motion path plus dynamic-object motion. See [VelocityRendering.cpp](https://github.com/EpicGames/UnrealEngine/blob/4dced763583b8fa05d214b133c2c21b73da300d4/Engine/Source/Runtime/Renderer/Private/VelocityRendering.cpp#L80-L90).
- Preserve previous animated vertex positions and material deformation, not
  only prior object transforms. UE's shader velocity path uses previous clip
  positions and removes the current and previous frame's projection jitter
  before computing current NDC position minus previous NDC position. This is
  displacement over the submitted frame interval, not units-per-second
  velocity. See [VelocityCommon.ush](https://github.com/EpicGames/UnrealEngine/blob/4dced763583b8fa05d214b133c2c21b73da300d4/Engine/Shaders/Private/VelocityCommon.ush#L9-L21)
  and [VelocityShader.usf](https://github.com/EpicGames/UnrealEngine/blob/4dced763583b8fa05d214b133c2c21b73da300d4/Engine/Shaders/Private/VelocityShader.usf#L80-L105).
- A prepared renderer texture does not prove that this frame wrote valid
  contents. Gate synthesis on successful motion and depth preparation for both
  eyes, including valid clear values where no geometry contributes. Keep the
  ordinary projection path available if a producer pass is skipped.

## Open questions for Steam Frame

- UE's frame-synthesis cvar is explicitly constrained to its Vulkan mobile
  renderer. That makes Vulkan a key reference for a standalone headset path, but
  does not by itself establish any specific runtime format list.
- The user's current Quake/OpenXR test creates an OpenGL session. Its reported
  sRGB-only format list is not a Vulkan capability report.
- A separate Vulkan OpenXR probe on Steam Frame enumerated
  `VK_FORMAT_R16G16B16A16_SFLOAT` and `VK_FORMAT_D32_SFLOAT`, then successfully
  created and enumerated per-eye motion/depth swapchains. A follow-up
  moving-pattern fixture submitted 120 frames with synthesis metadata on both
  eyes and used GPU readback to verify signed motion and matching depth, but did
  not verify visual synthesis. See the [format probe](../../../performance-results/spacewarp-vulkan-format-probe-20261013/RESULTS.md)
  and [frame-loop result](../../../performance-results/spacewarp-vulkan-frame-loop-20261014/RESULTS.md).
- The audited UE fallback code does not document an sRGB vector encoding. We
  should not copy the fallback without verifying the exact RHI format, transfer
  function, and runtime interpretation.
- UE's documentation/source does not establish that the same motion-vector
  producer performs well on every supported renderer or runtime. It is a
  reference for engine plumbing and renderer velocity integration, not a
  blanket quality or performance guarantee.

## Source links

- Epic source, pinned commit: [`OpenXRHMD.cpp`](https://github.com/EpicGames/UnrealEngine/blob/0bcfaffa52e9557258dbd388835ac7668fe864a4/Engine/Plugins/Runtime/OpenXR/Source/OpenXRHMD/Private/OpenXRHMD.cpp)
- Epic source, pinned commit: [`OpenXRHMD_Swapchain.cpp`](https://github.com/EpicGames/UnrealEngine/blob/0bcfaffa52e9557258dbd388835ac7668fe864a4/Engine/Plugins/Runtime/OpenXR/Source/OpenXRHMD/Private/OpenXRHMD_Swapchain.cpp)
- Epic API reference: [`IStereoRenderTargetManager`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/IStereoRenderTargetManager)
- Epic/Meta integration guide: [Application Space Warp / frame synthesis in Unreal](https://developers.meta.com/horizon/documentation/unreal/unreal-asw/)
- Khronos: [`XR_EXT_frame_synthesis`](https://registry.khronos.org/OpenXR/specs/1.1/man/html/XR_EXT_frame_synthesis.html)
