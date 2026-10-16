# OpenXR frame synthesis: cross-engine porting notes

This document set records what we learned while investigating
`XR_EXT_frame_synthesis` (often called SpaceWarp) for Quake VR on Steam Frame.
It is meant as a reusable starting point for other engines. The core rule is
that OpenXR plumbing, renderer motion/depth production, graphics-API transport,
and runtime display behavior are separate problems. Proving one does not prove
the others.

## Read in this order

1. [Engine guide](FRAME_SYNTHESIS_ENGINE_GUIDE.md) explains the extension,
   format discovery, OpenXR submission and runtime limits.
2. [Porting playbook](FRAME_SYNTHESIS_ENGINE_PORTING_PLAYBOOK.md) gives the
   suggested engine integration and verification sequence.
3. [Porting checklist](FRAME_SYNTHESIS_ENGINE_PORTING_CHECKLIST.md) is a short
   implementation and review list for other engine teams.
4. [Renderer data contract](FRAME_SYNTHESIS_RENDERER_DATA_CONTRACT.md) defines
   the color, motion, depth and temporal-history relationship each eye needs.
5. [Cross-API interop notes](FRAME_SYNTHESIS_CROSS_API_INTEROP.md) covers
   engines whose renderer API differs from the OpenXR graphics binding. The
   focused [Vulkan depth interop note](FRAME_SYNTHESIS_VULKAN_DEPTH_INTEROP.md)
   covers sample-location-sensitive depth transitions and runtime image usage.
6. [Unreal source reference](UNREAL_FRAME_SYNTHESIS_REFERENCE.md) audits UE 5.7
   and cross-checks UE 5.8 for runtime-sized targets, geometry-derived motion,
   extension selection, and the limitations of copying engine-specific gates.
7. [Godot source reference](GODOT_FRAME_SYNTHESIS_REFERENCE.md) reviews the
   extension wrapper, renderer target handoff and version-sensitive assumptions.
8. [Engine support survey](SPACEWARP_ENGINE_SUPPORT_SURVEY.md) distinguishes
   runtime availability from engine integration and verified display behavior.
   [Renderer architecture choices](FRAME_SYNTHESIS_RENDERER_ARCHITECTURES.md)
   maps engine renderers and graphics-API combinations to viable producer paths.
9. [Steam Frame status](SPACEWARP.md), the dated [OpenGL swapchain probe](SPACEWARP_GL_OPENXR_SWAPCHAIN_PROBE_2026-10-14.md),
   the dated [Vulkan enable2 probe](SPACEWARP_VULKAN_ENABLE2_PROBE_2026-10-14.md),
   the [GL motion/depth transport probe](SPACEWARP_GL_FORMAT_INTEROP_PROBE_2026-10-14.md),
   and the [Vulkan roadmap](SPACEWARP_VULKAN_ROADMAP.md) contain device-specific
   findings and remaining product gates.
10. [Headset validation procedure](FRAME_SYNTHESIS_HEADSET_VALIDATION.md)
   distinguishes OpenXR submission, runtime texture receipt and displayed reprojection.
11. [Engine adapter template](FRAME_SYNTHESIS_ENGINE_ADAPTER_TEMPLATE.md) is a
   reusable contract for separating engine rendering from OpenXR submission.
12. [Porting validation matrix](FRAME_SYNTHESIS_PORTING_VALIDATION_MATRIX.md)
   turns the lessons into acceptance gates for other engines.
13. [EXT versus FB comparison](FRAME_SYNTHESIS_EXT_FB_COMPARISON.md) explains
   which renderer outputs can be shared and which OpenXR details cannot.

## Reusable design lessons

- Model support as a capability chain: extension advertised, instance enabled,
  usable formats on the selected graphics binding, swapchains created, valid
  per-eye data produced, fresh metadata submitted, then runtime consumption
  confirmed in diagnostics.
- Reuse renderer-generated current-to-previous motion and matching depth when
  available. Image-space optical flow is a fallback estimator, not equivalent
  to object velocity; it has ambiguity at disocclusions, transparent effects,
  low-texture regions and animated lighting.
- Keep motion and depth tied to the exact color image, eye, projection,
  viewport and submitted time interval. Make image origin, sign, units, jitter,
  depth convention, MSAA resolve and history invalidation explicit.
- Treat graphics formats as runtime- and API-specific. The Steam Frame OpenGL
  session tested here exposed normalized formats for motion, while Vulkan
  exposed signed FP16 motion and D32 depth. Do not infer one API's formats from
  another API or runtime's support.
- For Vulkan interop, validate each image's full creation and use contract.
  Compatible-depth flags affect legal layout transitions; OpenXR-owned depth
  images can also have different usage bits from motion/color images. See the
  [depth interop note](FRAME_SYNTHESIS_VULKAN_DEPTH_INTEROP.md).
- Keep ordinary projection rendering as a safe fallback. A failed image,
  history reset, stale eye or unsupported format must not yield stale or
  silently-clamped motion metadata.
- Test with a deterministic shape-motion fixture before real game content, then
  inspect the runtime's received motion/depth textures and compositor behavior.
  `xrEndFrame` success alone proves submission, not frame generation.
- Profile the producer, synchronization, and game workload independently. The
  first captures labelled as a gore workload froze Quake's simulation and are
  not combat evidence; the corrected [validated replay report](../../../performance-results/spacewarp-gore-real-abba-20261014/RESULTS.md)
  asserts 25 kill events, a 25-to-1 monster-count change and active particles
  and decals in every run. The short unworn A/B/A/B replay suggests about
  0.78 ms extra average GPU eye time and 1.39 ms CPU time with synthesis enabled,
  while the detailed [death-event profile](../../../performance-results/spacewarp-gore-real-detail-20261014/RESULTS.md)
  locates a worst-frame spike in QuakeC/physics, decal upload and particle-vertex
  work. These are app-side measurements with no HMD present; neither report
  proves runtime-generated frames. A synchronous validation readback also
  showed large CPU waits in its own trace; avoid that design and require
  workload-event assertions before treating a capture as a gameplay profile.

## What the current evidence proves

On the tested Steam Frame software stack, a standalone Vulkan fixture wrote
known signed motion and depth to runtime-created images and submitted 120 frames
with `XR_EXT_frame_synthesis` metadata. A Quake OpenGL prototype also created
motion/depth swapchains and submitted metadata after non-finite optical flow was
guarded. These verify data production/transport and app-side submission in
their respective test paths. They do not verify that SteamVR synthesized an
intermediate frame or validate wearer-visible image quality. Zink image
definitions and pixel transport into OpenXR-owned Vulkan swapchain images were
captured for RGBA8 color, RGBA16F motion and D32 depth. The motion/color probes
have transfer-compatible usage in the captures. The D32 probe's earlier pixel
match copied into an image whose captured creation usage lacks transfer bits,
and the run is not tied to a source/binary hash; treat that result as diagnostic
evidence only, not a validated depth transport path. The replacement
depth-attachment fallback has now passed an isolated Frame GPU test: three
acquired D32 images, 12,288 sampled pixels, zero mismatches, with actual
same-run creation metadata and no transfer use on XR depth. See the
[rasterized depth result](../../../performance-results/spacewarp-depth-raster-20261014/RESULTS.md).
SteamVR logged an unclassified GPU-fence error during that zero-layer fixture;
the app-side pixel/bridge test still passed, and this run did not test synthesis.
There is still no live Quake Vulkan2 renderer bridge. Current evidence therefore
establishes neither compositor consumption, generated display cadence, nor
working Quake SpaceWarp.

The reusable recommendation is therefore: integrate at the OpenXR adapter
boundary, source data from the renderer's native velocity/depth passes, add a
small deterministic runtime fixture, and gate shipping on headset diagnostics
plus a repeatable enabled/disabled performance capture.
