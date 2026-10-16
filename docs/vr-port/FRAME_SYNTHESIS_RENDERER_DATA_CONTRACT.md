# Frame synthesis renderer data contract

An engine-neutral contract for the renderer data supplied to OpenXR
`XR_EXT_frame_synthesis`. Use this when adapting an existing engine velocity
buffer or designing a dedicated synthesis pass. API plumbing cannot repair
motion vectors that describe a different image, eye, or time interval.

## Per-view inputs

For each projection view, provide:

| Input | Meaning | Must match |
| --- | --- | --- |
| Color image | Current rendered eye image | Current pose, projection, viewport, and display interval |
| Motion image | Screen-space displacement from the prior rendered image to this image | The exact color image and the prior-history frame |
| Depth image | Depth corresponding to the current color sample | The same projection, resolve, viewport, and eye |
| App-space delta pose | Change in application tracking space since the prior submitted frame | The same temporal interval as motion/history |

The exact OpenXR metadata fields and motion interpretation are specified by
Khronos. This contract is for engine-side data production; check the extension
spec for the enabled revision and graphics binding.

## Motion-vector convention

The EXT path describes current screen position minus previous screen position
in NDC. Validate that convention empirically; engines commonly store the
opposite sign, UV-scaled values, jittered values, or camera-only motion. The
renderer should document:

- X and Y orientation, including whether the API's image origin is top-left or
  bottom-left.
- Motion units and the conversion to NDC over the actual render viewport.
- Whether camera movement and object movement are both represented.
- How current and previous projections, view transforms, and TAA jitter are
  handled.
- How history is initialized, invalidated, and committed when submission fails.
- What invalid or newly visible pixels contain.

Use a small test with known translations to verify each axis and sign. A
horizontal-only test does not establish Y orientation. EXT's 2D motion mode
uses the two spatial channels and affine scale/offset metadata; a signed-float
buffer can use identity scale and zero offset. Do not assume the same metadata
or encoding applies to `XR_FB_space_warp`.

## Depth, projection, and sampling

Motion, depth, and color must refer to the same projection. Depth values are
not automatically linear distance: identify whether the renderer uses
conventional or reversed Z and set OpenXR's depth range and metric near/far
values accordingly. Confirm the runtime's expected image sampling orientation.

If MSAA is enabled, select a covered sample deliberately. A naïve average can
blend foreground and background depth across edges. When downsampling from
full-resolution color/depth/velocity, avoid mixing samples across motion or
depth discontinuities. Query the runtime's recommended motion image dimensions
and keep per-view sizes and array-layer mapping consistent with its response.

## Temporal lifecycle

Only publish a motion/depth pair when it represents the submitted color frame.
Warm up with color-only output after history reset. Invalidate history after
tracking loss, focus transitions, skipped/held frames, map or camera cuts,
teleports, view-size changes, or failed stereo submission. Commit prior-frame
transforms and image history only after the corresponding fresh frame has been
submitted successfully.

If one eye's synthesis target cannot be written or released, fall back to a
normal stereo color submission. Do not submit one eye's stale vector field next
to the other eye's fresh field.

## Scene content policy

Make explicit choices for surfaces that do not behave like opaque geometry:

- **Sky:** derive camera rotation motion; translation has no finite surface
  depth.
- **Opaque animated geometry:** use previous and current object transforms.
- **Particles/translucency:** use a compatible velocity pass or mark/omit
  regions knowingly; compositing order can make one depth value ambiguous.
- **UI and weapon overlays:** decide whether they live in the projection image,
  a separate composition layer, or receive their own motion policy.
- **Portals, refraction, heat distortion, and time effects:** transformed rays
  can invalidate ordinary surface motion and depth.
- **Disocclusion:** a newly visible pixel has no true previous correspondence;
  use a renderer/runtime-supported validity policy rather than fabricated
  high-confidence motion.

Unreal 5.7 is a useful reference because it renders stereo vectors directly
from current/previous transforms into runtime-provided targets. Its renderer
calculates current-minus-previous clip-space XY and device-depth delta, and it
disables TAA and motion blur in that mode. This is a design example, not a
requirement that every engine use identical passes. See the
[Unreal source notes](UNREAL_FRAME_SYNTHESIS_REFERENCE.md).

## Format and graphics API contract

Enumerate formats for the session's actual graphics API. Prefer the runtime's
advertised signed floating-point motion format and a compatible depth format.
Confirm swapchain usage, sample count, dimensions, format features, color-space
conversion, layout, queue-family ownership, and release synchronization.

Do not silently fall back from signed float to 8-bit normalized RGBA. An
unsigned encoding needs a tested scale/offset, exact-zero behavior,
quantization/range analysis, and transfer-function validation. Never silently
clamp an invalid vector to the representable endpoint. A GPU-side bounded
fallback can preserve a known-good camera vector or a neutral vector per pixel,
then mark degraded pixels through an ignored diagnostic channel when available;
this trades motion accuracy for bounded data and must be validated in the
runtime. A blocking per-frame readback can erase the performance benefit: in
the Quake test it added over 17 ms of CPU wait across both eyes. See the
[measured profile](../../../performance-results/spacewarp-gore-detail-20261014/RESULTS.md)
and [bounded fallback result](../../../performance-results/spacewarp-gore-safe-abba-20261014/RESULTS.md).

## Validation matrix

Before real gameplay, use a deterministic test scene and cover:

| Case | What it catches |
| --- | --- |
| Static scene and static pixels | Nonzero bias, wrong zero encoding, stale history |
| Positive/negative X and Y | Sign, axis, and image-origin mistakes |
| Camera translation and rotation | Projection and application-space delta errors |
| Moving opaque foreground over static background | Missing object motion and silhouette errors |
| Both eyes with distinct colors | Eye/layer/subimage swaps and cross-eye reuse |
| Conventional and reversed depth | Incorrect depth range or near/far metadata |
| MSAA and downsampled targets | Edge contamination and mismatched resolutions |
| Focus loss, skipped frames, teleport | History reset and recovery behavior |
| Particles, UI, transparency, distortion | Unsupported scene-content assumptions |

First prove swapchain writes and ordinary OpenXR frame submission. Then prove
known motion/depth in runtime diagnostics. Finally check compositor behavior
and visible output in the headset. Record application GPU time separately from
compositor timing, with the same workload with the feature on and off.

## Related material

- [Engine porting playbook](FRAME_SYNTHESIS_ENGINE_PORTING_PLAYBOOK.md)
- [OpenXR engine integration guide](FRAME_SYNTHESIS_ENGINE_GUIDE.md)
- [Unreal 5.7 source reference](UNREAL_FRAME_SYNTHESIS_REFERENCE.md)
- [Steam Frame Vulkan roadmap](SPACEWARP_VULKAN_ROADMAP.md)
