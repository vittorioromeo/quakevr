# OpenXR frame synthesis integration guide

Reusable notes for engine and renderer developers adding `XR_EXT_frame_synthesis`
(or the older `XR_FB_space_warp`) to an OpenXR application. This guide separates
API requirements from Steam Frame observations and from implementation choices.

## What the extension does

The application submits a projection image plus per-view motion-vector and depth
images. The runtime may use them to extrapolate/reproject a future image. This is
an input to runtime synthesis; it does not reduce the application's scene-render
cost. The runtime controls pacing and may decline to synthesize in a given frame.

For `XR_EXT_frame_synthesis`, motion is specified in NDC as current position
minus previous position for the submitted interval; it is a displacement, not
velocity divided by elapsed time. Remove current and previous projection jitter
consistently before subtraction. Vulkan-style NDC conventions apply, even when
the application renders through OpenGL. With the 2D flag, the runtime interprets the
red and green channels and applies `motion = sampledRG * scale.xy + offset.xy`.
The alpha channel is ignored. Signed 16-bit floating-point channels are
recommended by the specification; they are not a validity requirement. The
`XR_FB_space_warp` structure has no equivalent scale/offset fields, so encodings
must not be assumed interchangeable between extensions.

## Renderer data is the preferred source

Prefer motion vectors emitted by the renderer's existing velocity pass, with the
matching depth buffer. This gives the engine access to exact transforms for
moving geometry and avoids an extra image-correspondence solver. Verify the
renderer convention: whether vectors are current-position minus previous-position
or the reverse, whether they are UV or NDC deltas, whether camera motion is
included, how jitter is removed, what happens for newly visible pixels, and
whether the buffer is linear signed float. Preserve previous animated vertex,
material-deformation, and first-person weapon transforms as well as previous
object transforms; object transforms alone do not reproduce deformed geometry.

An optical-flow pass can estimate image motion when a renderer has no velocity
buffer, as this Quake port currently does. Treat it as a quality/performance
tradeoff: textureless regions, lighting changes, translucency, particles,
disocclusions, and fast motion can produce uncertain vectors. It also adds GPU
work. Do not describe it as exact geometry velocity.

Depth and vectors must describe the same rendered image, eye, time interval,
viewport, and projection. Resolve MSAA depth conservatively, use the runtime's
recommended motion/depth dimensions when practical, and define how sky, UI,
particles, portals, distortion, and invalid history are handled.
Prepare both motion and depth for every submitted frame, including a deliberate
valid clear for pixels with no geometry. A texture being allocated or acquired
does not prove its current-frame contents were written.

## OpenXR integration sequence

1. Enumerate instance extensions before `xrCreateInstance`; request
   `XR_EXT_frame_synthesis` only when the application setting is enabled and the
   runtime advertises it. Keep ordinary projection-layer rendering as the
   fallback.
2. When enumerating view configuration views, chain
   `XrFrameSynthesisConfigViewEXT` to obtain the recommended motion-vector image
   dimensions. If unavailable or invalid, use an explicitly chosen conservative
   size.
3. After the OpenXR session exists, enumerate formats for the actual selected
   graphics binding. Choose a runtime-advertised renderable motion format and a
   supported depth format. Extension support alone does not promise a suitable
   format. Signed 16-bit float is the recommended vector representation.
4. Create per-eye (or correctly layered multiview) motion and depth swapchains
   with attachment/sampling usage matching the renderer's actual operations.
   Acquire, wait, write, and release images according to OpenXR image ownership
   rules. If either image fails, submit color only for that frame.
5. Attach one correctly initialized `XrFrameSynthesisInfoEXT` to each matching
   `XrCompositionLayerProjectionView::next`. Submit valid depth range, metric
   near/far values, motion scale/offset and application-space delta. Keep every
   chained structure alive until `xrEndFrame` returns.
6. Reset or warm up history after skipped/held frames, discontinuous display
   times, view-size changes, teleports, world changes, invalid camera tracking,
   or failed eye submission. Never send stale vectors paired with new color.

Keep EXT and FB structure construction separate. EXT vectors are affine-decoded
with scale and offset and can explicitly declare 2D motion. FB Space Warp uses a
different structure and data contract.

## Coordinate and image validation

Do not infer Y direction from a horizontal-only fixture. Add independent tests
for positive/negative X and Y movement, camera-only movement, object-only
movement, static pixels, both eyes, both depth conventions, and OpenGL/Vulkan
image origins. The OpenXR EXT specification describes Vulkan-style NDC; OpenGL
render targets commonly use a bottom-left origin and positive-up NDC. Convert the
renderer vectors into the OpenXR convention explicitly. Some engine/runtime
implementations expose a Y-flip option because this convention has caused
cross-runtime differences; keep a diagnostic flip until the target runtime is
validated.

For normalized formats, test quantization and saturation, not just the affine
identity in floating point. Any vector outside the chosen encodable interval
must be detected and handled; silently clamping motion gives the runtime false
data. A color-space format also requires a verified write/read transfer path.
In the Quake prototype, a GPU reduction followed by synchronous readback
measured 9.48 ms and 8.03 ms of CPU wait for the two eyes. Replacing it with a
per-pixel finite/range check in the export shader removed that synchronous
readback. Later 450-frame captures measured 8.585 ms median GPU eye time enabled
and 7.359 ms disabled, but an audit found the single-player simulation was
paused throughout: these are frozen-world render-path costs, not combat/gore
measurements. They are application-side measurements, not a wearer or
generated-frame result. The fallback preserves
valid optical flow; for invalid or unrepresentable flow it uses camera
reprojection if that is finite and in range, otherwise neutral zero motion. It
does not clamp. When an alpha channel exists, zero marks fallback pixels for
diagnostics; the EXT runtime ignores alpha. This is a Quake-tested design option
for normalized formats, not a universal best choice. Prefer native signed-float
motion where available, and validate the fallback's image quality and range at
target resolution before shipping. See the [original readback profile](../../../performance-results/spacewarp-gore-detail-20261014/RESULTS.md),
[safe-path A/B report](../../../performance-results/spacewarp-gore-safe-abba-20261014/RESULTS.md),
and [current two-iteration replay report](../../../performance-results/spacewarp-flow-2iter-abba-20261014/RESULTS.md).

For example, naïvely mapping a signed component with `q = 0.5 + v/(2M)` to an
sRGB8 swapchain does not encode an exact zero: sRGB byte 188 decodes to about
0.502886. With `M=1` that creates roughly five color-image pixels of zero bias
at 1728-pixel width, and about ten pixels per adjacent code step near zero.
Anchor zero to a representable decoded value and use an explicit scale/offset if
testing this path. Tightening the range improves precision but clips more
motion; reject or flag out-of-range frames instead of silently saturating. These
numbers describe the transfer/quantization model and still require GPU/runtime
round-trip validation.

## Runtime and performance verification

Record, separately, whether the runtime advertised the extension, whether the
instance enabled it, which graphics API created the session, enumerated formats,
swapchain creation results, per-eye image acquisition/release results, and
whether fresh projection frames actually carry the synthesis structure. An
extension-enabled log line is not evidence that vectors were accepted or that
the runtime synthesized an image.

Use the runtime's vector/depth diagnostic view when available. Check the
Performance Assessment overlay and SteamVR `perfcriteria.txt`; compare the same
recorded workload with synthesis off/on and record application GPU time,
compositor behavior, displayed frame cadence, and vector/depth imagery. Test
animated enemies, weapon effects, transparency, head turns, artificial movement,
teleports, dashboard focus changes, and recovery.

## Steam Frame findings so far

The tested Steam Frame SteamVR/OpenXR 2.18.2 OpenGL session advertised and
enabled `XR_EXT_frame_synthesis`, and enumerated `GL_SRGB8`,
`GL_SRGB8_ALPHA8`, and depth-component formats, but no signed floating-point
motion format. Quake therefore added an **experimental** affine sRGB8 encoding
using EXT scale/offset, guarded against non-finite and out-of-range vectors.
The game created per-eye motion/depth swapchains and submitted fresh
`XrFrameSynthesisInfoEXT` metadata. This establishes application-side writes
and submission, not compositor consumption or display synthesis. Any engine
considering normalized motion targets must validate transfer functions,
quantization near zero, range, filtering and runtime interpretation on its own
device/API combination. Unreal's float target and Godot's GL_RGBA16F target
are references for their respective renderers, not evidence that Frame OpenGL
offers those formats.

The Vulkan follow-up probe succeeded: a Vulkan OpenXR session on Frame enumerated
`VK_FORMAT_R16G16B16A16_SFLOAT` and `VK_FORMAT_D32_SFLOAT`, then created per-eye
motion and depth swapchains with attachment/sampled usage and enumerated their
images. A separate Vulkan frame-loop fixture rendered a moving rectangle into
those swapchains and submitted 120 frames with EXT metadata on both eyes. GPU
readback verified signed FP16 foreground motion, zero static
motion, and matching foreground/background depth in both eyes. Together these
prove high-precision data production, transport, and API submission on the test
pattern. The same fixture also created both-eye color, FP16 motion, and D32
depth swapchains with `XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT` and completed 120
submissions; this is useful for a cross-API copy bridge, though it does not
prove actual image copies. They do not prove compositor synthesis or headset
image quality. See
the [format probe](../../../performance-results/spacewarp-vulkan-format-probe-20261013/RESULTS.md)
and [frame-loop result](../../../performance-results/spacewarp-vulkan-frame-loop-20261014/RESULTS.md).

The corrected 450-frame Quake replay asserted 25 enemy deaths (monster count
25 to 1) in every ON/OFF run. Its short unworn A/B/A/B capture estimated an
enabled-minus-disabled cost of about 1.44 ms/frame, 1.39 ms CPU busy time, and
0.78 ms GPU eye time. A detailed event profile found large individual CPU
spikes in game physics/QuakeC plus decal upload and particle preparation. These
are app-side measurements with no valid wearer pose; they do not demonstrate
runtime-generated frames or headset performance. Optical-flow work is extra
producer cost, not a reduction in scene-render cost. See the validated
[replay report](../../../performance-results/spacewarp-gore-real-abba-20261014/RESULTS.md)
and [event detail](../../../performance-results/spacewarp-gore-real-detail-20261014/RESULTS.md).

For a Vulkan renderer, use the queue bound to the OpenXR session and obey the
runtime's required image layouts and queue-family ownership. Submit commands
that write acquired synthesis images before releasing them, then serialize
queue-touching OpenXR operations with the engine's queue mutex. OpenXR's Vulkan
concurrency rules allow a released image to remain referenced by already
submitted work on that bound queue; a CPU fence wait before every release is
not required and would add avoidable latency. Confirm the exact requirements
for the OpenXR version/runtime and avoid concurrent unsynchronized queue access.

When an engine's renderer and OpenXR binding use different graphics APIs, a
shared-image bridge may be an intermediate architecture: render into
application-owned images shareable by both APIs, transfer ownership with
external semaphores, then copy into runtime-owned OpenXR swapchain images on
the bound queue. Keep OpenXR-owned images in the API that created them. Query
physical-device identity, exact image formats/usages, dedicated-allocation
requirements, and external semaphore support, then prove a pixel round trip
before integrating. Extension strings, matching device UUIDs, successful FD
import, and matching device names alone are not enough. Early Frame attempts
failed: Zink initially rejected an incompatible RGBA8 allocation, and later
storage success still had mismatched internal image definitions and no proven
cross-API ownership transfer. Those failures showed why a pixel test and API
trace are required. A subsequent exact-definition test passed RGBA8 transport
into an OpenXR-owned Vulkan color image. The newer isolated probe then passed
the same definition, ownership, synchronization and pixel gates for RGBA16F
motion and D32 depth. The Steam Frame Zink/Vulkan bridge is therefore a tested
transport candidate for these exact formats, not an invalid path. See the
[dated format report](SPACEWARP_GL_FORMAT_INTEROP_PROBE_2026-10-14.md),
[Vulkan roadmap](SPACEWARP_VULKAN_ROADMAP.md),
[cross-API interop notes](FRAME_SYNTHESIS_CROSS_API_INTEROP.md), and earlier
[capability results](../../../performance-results/spacewarp-gl-vk-capabilities-20261014/RESULTS.md).

The bridge fixture does not submit projection layers or `XR_EXT_frame_synthesis`
metadata. It has not established SteamVR texture receipt, generated-frame
cadence, headset image quality, live Quake integration or production cost. A
different engine/runtime/driver must query and match its own image definitions,
then repeat the asymmetric pixel and reuse checks; Frame's flags and usage bits
are not reusable defaults. The direct OpenGL depth-swapchain failure remains a
separate limitation: this hybrid route uses a Vulkan OpenXR binding and copies
into Vulkan-owned runtime images.

For cross-engine portability, isolate the bridge behind a renderer-independent
transport interface. Require a startup proof for each exact format and usage:
allocate/import, render or write a known pattern, signal and wait on an external
semaphore, return ownership, and compare pixels in the other API. Record
allocation size/type, dedicated-allocation metadata, tiling, layout, queue-family
ownership, image origin, color encoding, and synchronization result. If any
stage fails, use the OpenXR binding already selected by the renderer or plan a
native renderer backend; do not silently fall back to lossy motion formats.

The isolated OpenGL probe found that every tested OpenGL depth swapchain
variant caused SteamVR shared-image errors during creation, even though
OpenXR returned success. The Vulkan OpenXR control created motion and depth
swapchains without those errors. The later shared-image tests established a
hybrid option: keep the renderer on GL/Zink, bind OpenXR to Vulkan, and copy
verified color, motion and depth images into runtime-owned Vulkan targets.
Whether this is preferable to a direct Vulkan renderer depends on live
integration correctness and measured transfer cost. Runtime texture diagnostics
and a controlled enabled/disabled headset comparison remain required. See the
[isolated swapchain probe](SPACEWARP_GL_OPENXR_SWAPCHAIN_PROBE_2026-10-14.md),
[dated transport report](SPACEWARP_GL_FORMAT_INTEROP_PROBE_2026-10-14.md),
[Vulkan roadmap](SPACEWARP_VULKAN_ROADMAP.md), and
[cross-API interop notes](FRAME_SYNTHESIS_CROSS_API_INTEROP.md).

## References

- Khronos: [XR_EXT_frame_synthesis overview and semantics](https://registry.khronos.org/OpenXR/specs/1.1/man/html/XR_EXT_frame_synthesis.html),
  [frame synthesis info](https://registry.khronos.org/OpenXR/specs/1.1/man/html/XrFrameSynthesisInfoEXT.html),
  [motion vector flags](https://registry.khronos.org/OpenXR/specs/1.1/man/html/XrFrameSynthesisInfoFlagBitsEXT.html),
  [swapchain formats](https://registry.khronos.org/OpenXR/specs/1.1/man/html/xrEnumerateSwapchainFormats.html).
- Khronos: [Vulkan enable concurrency rules](https://registry.khronos.org/OpenXR/specs/1.1/man/html/XR_KHR_vulkan_enable_concurrency.html).
- Khronos cross-API sharing: [OpenGL external memory](https://registry.khronos.org/OpenGL/extensions/EXT/EXT_memory_object.txt),
  [OpenGL external objects](https://registry.khronos.org/OpenGL/extensions/EXT/EXT_external_objects.txt),
  [OpenGL FD import](https://registry.khronos.org/OpenGL/extensions/EXT/EXT_memory_object_fd.txt),
  [OpenGL external semaphores](https://registry.khronos.org/OpenGL/extensions/EXT/EXT_semaphore.txt),
  [OpenGL semaphore FD import](https://registry.khronos.org/OpenGL/extensions/EXT/EXT_semaphore_fd.txt),
  [Vulkan external memory FD](https://registry.khronos.org/Vulkan/specs/1.0-extensions/man/html/VK_KHR_external_memory_fd.html),
  [Vulkan external semaphore FD](https://registry.khronos.org/Vulkan/specs/1.0-extensions/man/html/VK_KHR_external_semaphore_fd.html),
  and [foreign queue-family ownership](https://registry.khronos.org/Vulkan/specs/1.3-extensions/man/html/VK_EXT_queue_family_foreign.html).
- Valve: [Steam Frame performance assessment and depth/motion data](https://partner.steamgames.com/doc/steamhardware/steamframe/compat/perf_criteria).
- Godot: [OpenXR frame synthesis extension implementation](https://github.com/godotengine/godot/blob/master/modules/openxr/extensions/openxr_frame_synthesis_extension.cpp) (including its runtime Y-flip diagnostic).
- This port: [Steam Frame test record](SPACEWARP.md).
- Engine-specific reference: [Unreal Engine 5.7 implementation](UNREAL_FRAME_SYNTHESIS_REFERENCE.md).
- Reusable implementation docs: [cross-engine porting index](FRAME_SYNTHESIS_PORTING_INDEX.md), [engine porting playbook](FRAME_SYNTHESIS_ENGINE_PORTING_PLAYBOOK.md), and [renderer data contract](FRAME_SYNTHESIS_RENDERER_DATA_CONTRACT.md).
