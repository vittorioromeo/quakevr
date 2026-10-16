# OpenXR frame synthesis porting checklist

This is the short implementation and review checklist for adding
`XR_EXT_frame_synthesis` (SpaceWarp) to an engine. It records reusable findings
from the Quake VR, Unreal and Godot investigations. It is deliberately separate
from engine-specific code: OpenXR image formats, renderer handles, synchronization
and coordinate conversions must be implemented and tested for each engine/API.

## Before implementation

- [ ] Pin the OpenXR extension and runtime versions being targeted.
- [ ] Keep `XR_EXT_frame_synthesis` and legacy `XR_FB_space_warp` in separate
  code paths. Their structures and motion encodings are not interchangeable.
- [ ] Identify the renderer's native velocity and depth outputs. Record their
  sign, units, coordinate space, jitter treatment, depth convention, origin,
  resolution, MSAA behavior and lifetime.
- [ ] Confirm that the selected OpenXR graphics binding is the same API/device
  used to access the session's swapchain images.
- [ ] Define an opt-in switch and projection-only fallback before adding the
  producer. Extension absence or any invalid frame must leave the app usable.

## OpenXR adapter

- [ ] Enumerate extensions before `xrCreateInstance`; enable EXT only when both
  requested and advertised.
- [ ] Chain `XrFrameSynthesisConfigViewEXT` when enumerating view configuration
  and retain the motion dimensions for each view.
- [ ] After graphics-bound session creation, enumerate formats for that exact
  binding. Select only formats the runtime advertises and the renderer can
  correctly write. Prefer signed FP16 motion when available.
- [ ] Create motion/depth swapchains with the usage flags the producer actually
  needs. Recreate them with the session/main targets when dimensions change.
- [ ] Track each swapchain's acquired image index independently. Acquire, wait,
  write, submit on the session-compatible queue, and release each image legally.
- [ ] Chain fresh `XrFrameSynthesisInfoEXT` onto every matching projection view.
  Keep its structures alive through `xrEndFrame`.
- [ ] Submit ordinary projection views if acquisition, production or metadata
  setup fails; never reuse stale motion or depth with current color.

## Renderer data contract

- [ ] Motion is current NDC position minus previous NDC position for the exact
  submitted interval. It is displacement, not units-per-second velocity.
- [ ] Include camera-induced motion of stationary visible surfaces and dynamic
  object motion. Preserve previous deformed/animated vertex positions, not only
  previous object transforms.
- [ ] Remove current and previous projection jitter consistently before
  computing vectors, or explicitly reproduce the renderer's jitter convention.
- [ ] Motion, depth and color describe the same eye, pose, projection, viewport
  and frame. Define array-layer and asymmetric-eye mapping explicitly.
- [ ] Convert renderer-specific NDC Y direction, render-target origin, depth
  range and reversed-Z convention explicitly. Do not infer them from an X-only
  test.
- [ ] Clear targets to deliberate valid values. Preserve opaque depth/motion
  when later passes add transparency or effects; do not accidentally clear it.
- [ ] For Vulkan shared depth images, record create flags, usage, sample count,
  layouts and ownership. If `VK_IMAGE_CREATE_SAMPLE_LOCATIONS_COMPATIBLE_DEPTH_BIT_EXT`
  is set, make every required layout transition preserve valid sample-location
  state. Do not assume a 1x center sample is supported. Consider sync2
  same-layout barriers only after enabling/querying the feature and validating
  the real depth-writing path.
- [ ] For OpenXR-owned Vulkan images, check the runtime image's actual usage
  before using transfer commands. A requested swapchain usage bit, returned
  image handle or successful copy call alone is not a substitute for matching
  the binding/runtime contract; use a spec-valid attachment path if transfer
  usage is unavailable.
- [ ] Define behavior for sky, UI, particles, translucency, disocclusion,
  teleports, tracking loss, skipped frames and newly visible pixels.
- [ ] Reset/warm history after discontinuities, resize, scene changes, focus
  recovery or failed production. Submit synthesis only when every eye is fresh.

## Format and graphics API checks

- [ ] Query the runtime's formats for the session's active graphics API; do not
  transfer a format list from OpenGL to Vulkan or between runtimes.
- [ ] Verify transfer functions and quantization through the actual GPU target
  if using normalized color formats for signed motion. An 8-bit/sRGB fallback is
  not automatically a valid vector encoding.
- [ ] Detect out-of-range/non-finite motion and reject it or use a documented
  fallback. Do not silently clamp vectors.
- [ ] For Vulkan, use the exact device and queue bound to the OpenXR session;
  implement image layouts, barriers and queue synchronization for the runtime's
  contract. Avoid a CPU fence per eye/frame unless required by that contract.
- [ ] Do not assume OpenGL/Vulkan shared allocation means compatible image
  definitions or ownership transitions. Prove a full round trip before using
  cross-API interop.

## Minimal correctness fixture

Before enabling real game content, run a deterministic stereo fixture containing
a static background and moving opaque shapes with known depth. Give each eye a
distinct color and test:

- [ ] Positive and negative X **and** Y motion.
- [ ] Camera-only movement and object-only movement.
- [ ] Static pixels, motion at two depths, and different left/right patterns.
- [ ] Both depth conventions used by the renderer, plus its MSAA resolve path.
- [ ] Resize, skipped-frame, tracking-loss and recovery history resets.
- [ ] GPU readback or another numeric check of source vectors, converted target
  values and depth before submitting metadata.

## Runtime and performance evidence

Record each state separately: extension advertised, extension enabled, targets
allocated, fresh data produced, metadata submitted, runtime receipt confirmed,
intermediate frames confirmed, and wearer validation completed. A successful
`xrEndFrame` proves submission only.

- [ ] Inspect SteamVR's motion/depth texture diagnostics when targeting SteamVR.
- [ ] Confirm compositor/display cadence or synthesis counters; distinguish
  generated frames from ordinary asynchronous reprojection/timewarp.
- [ ] Compare deterministic enabled/disabled workloads in alternating order.
  Record app CPU/GPU times, compositor timing, refresh/resolution, event counts
  and headset presence separately.
- [ ] Measure the producer's cost too. Frame synthesis does not reduce the
  application's scene-render work, and a velocity or optical-flow pass may add
  GPU cost.
- [ ] Validate wearer-visible artifacts during camera motion, locomotion,
  animation, transparency, particles, teleports and recovery.

## Findings that should guide engine ports

Unreal demonstrates the renderer-native route: runtime-sized targets are handed
to its mobile renderer, which writes geometry-derived motion and matching depth.
Its Vulkan/mobile/multiview restrictions are Unreal implementation decisions,
not universal OpenXR requirements. Godot demonstrates a lifecycle wrapper that
acquires targets, hands them to the renderer, releases them after drawing, and
chains per-view metadata; its format and size assumptions still need checking
against each runtime. Links and source-audit caveats are in the
[Unreal reference](UNREAL_FRAME_SYNTHESIS_REFERENCE.md) and
[Godot reference](GODOT_FRAME_SYNTHESIS_REFERENCE.md).

On the Steam Frame stack tested for this port, Vulkan accepted signed FP16
motion and D32 depth swapchains and a moving-pattern fixture submitted data for
both eyes. OpenGL depth swapchains emitted runtime shared-image errors during
creation, so the Quake implementation direction is a native Vulkan renderer.
The fixture does not prove SteamVR displayed synthesized frames. See the
[engine guide](FRAME_SYNTHESIS_ENGINE_GUIDE.md),
[cross-API findings](FRAME_SYNTHESIS_CROSS_API_INTEROP.md),
[validation matrix](FRAME_SYNTHESIS_PORTING_VALIDATION_MATRIX.md), and
[Steam Frame roadmap](SPACEWARP_VULKAN_ROADMAP.md) for evidence and limits.
