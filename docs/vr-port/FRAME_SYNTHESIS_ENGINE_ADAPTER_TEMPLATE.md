# Engine adapter template for OpenXR frame synthesis

Use this document when adding `XR_EXT_frame_synthesis` to a renderer or engine.
The design keeps OpenXR lifecycle and submission separate from the engine's
motion/depth production. It is a contract, not copy-and-paste API code: graphics
formats, image ownership, queue synchronization, render graph access and
per-view metadata must be implemented for the target engine and runtime.

## Ownership boundaries

The OpenXR adapter owns:

- Extension discovery before instance creation and optional enablement.
- The session's selected graphics binding and per-binding format discovery.
- Runtime-recommended motion dimensions and creation/destruction of OpenXR
  motion/depth swapchains.
- Per-image acquire/wait/release, subimage rectangles, array layers and image
  indices. Motion and depth swapchain indices are tracked independently.
- Fresh per-view composition metadata and its lifetime through `xrEndFrame`.
- Diagnostic state and a clean projection-only fallback.

The renderer owns:

- Motion and depth for the same color image, eye, projection, viewport and
  submitted display interval.
- The conversion from its own velocity/depth convention into the OpenXR EXT
  convention and the chosen runtime-supported formats.
- Clearing, MSAA resolution, dynamic-resolution mapping, jitter treatment,
  animated geometry history and policies for sky, transparency, particles, UI,
  disocclusion and invalid history.
- An explicit indication that both eyes contain fresh valid data for this frame.

The frame scheduler owns the two sides' agreement: it gives the renderer the
current frame token, predicted display time, view state and acquired targets;
it submits metadata only if the renderer completed fresh matching output and
the adapter can release every acquired image legally.

## Suggested engine-facing interface

Map this sketch to the engine's render graph/resource handles. Avoid exposing
raw OpenXR structures inside generic renderer code.

```cpp
struct SynthesisFrameContext {
    FrameToken frame;
    XrTime displayTime;
    Span<const ViewInfo> views;
    Span<const MotionTarget> motionTargets;
    Span<const DepthTarget> depthTargets;
};

struct SynthesisRenderResult {
    FrameToken frame;
    bool everyViewWritten;
    bool motionMatchesColor;
    bool depthMatchesColor;
    RendererHistoryGeneration history;
};

class FrameSynthesisRenderer {
public:
    SynthesisRenderResult renderMotionAndDepth(
        const SynthesisFrameContext& context,
        const ColorFrame& color,
        const RendererHistory& previous);
    void invalidateHistory(HistoryResetReason reason);
};

class OpenXRSynthesisAdapter {
public:
    CapabilityReport initialize(XrInstance, XrSession, GraphicsBinding);
    AcquiredSynthesisTargets acquire(FrameToken);
    void releaseAfterRender(FrameToken, const SynthesisRenderResult&);
    bool appendPerViewMetadata(FrameToken, ProjectionView&);
    void disableAndFallback(SynthesisFailure reason);
};
```

`FrameToken` is an engine-side identity, not a display timestamp. It should bind
the color image, each eye's matrices and poses, acquired OpenXR images, renderer
history generation, and metadata. Reject any result whose token or history
generation is stale. Keep extension-specific structures out of a generic
renderer interface so EXT and legacy FB implementations cannot accidentally
share incompatible encodings.

## Per-frame sequence

1. At startup, enumerate extensions and enable EXT only when requested and
   advertised. If the extension is unavailable, start normally without it.
2. Chain `XrFrameSynthesisConfigViewEXT` while enumerating view configuration
   views. Store the recommendation per view; do not assume every eye has the
   same size unless the runtime says so.
3. After session creation, enumerate swapchain formats for the graphics binding
   actually used by the session. Choose a supported renderable motion format
   and depth format. Prefer signed FP16 motion. A fallback format needs a
   measured encoding and transfer path; format presence alone is insufficient.
4. Acquire, wait for, and prepare motion/depth images using the API-specific
   synchronization and ownership rules. Keep their swapchain indices separate
   from each other and from the color swapchain index.
5. Render current color, motion and depth from one coherent renderer snapshot.
   Motion for EXT is current NDC position minus previous NDC position, in
   Vulkan-style NDC. It is displacement over the submitted frame interval, not
   world-units-per-second velocity. Depth describes the same projection and
   must declare correct depth range, metric near/far values and reversed-Z
   ordering.
6. Release only images whose writes and transitions are complete according to
   the selected graphics API. Fill a fresh `XrFrameSynthesisInfoEXT` per
   projection view, attach it to that view's `next` chain, and keep every
   chained object alive through `xrEndFrame`.
7. Commit temporal history only after successful stereo rendering and
   submission. On a one-eye failure, submit ordinary color for both eyes and
   invalidate synthesis history; never pair fresh color with old motion.

`appSpaceDeltaPose` describes the incremental application transform between
frames. It is identity when the app did not move the projection layer's space.
When there is artificial locomotion, calculate it once from consecutive
application-space transforms, express translation relative to the current app
space in meters, and use the same delta for all views. Tracked head motion is
already reflected in the per-view projection and should not be counted again as
artificial locomotion.

## History invalidation

Invalidate or warm up renderer history after tracking loss, a held/skipped
frame, session focus recovery, map or scene changes, teleports, snap turns,
projection or target-size changes, dynamic-resolution discontinuities, camera
cuts, renderer restart, or any failed stereo submission. Avoid chaining
metadata while targets are being recreated. If temporal anti-aliasing or motion
blur consumes the same velocity buffer, document whether its jittered vectors
are compatible; do not assume a TAA velocity texture matches OpenXR's contract.

## Format fallback policy

- Prefer a runtime-advertised signed floating-point motion format.
- Do not use a generic 8-bit UNORM fallback unless the renderer explicitly
  encodes signed motion, the EXT scale/offset decode is correct, and GPU
  round-trip tests quantify zero bias, precision, range and color-space
  conversion.
- Detect non-finite and out-of-range vectors before encoding. Choose a
  deliberate valid fallback or submit color only; never silently clamp.
- Do not infer OpenGL support from Vulkan enumeration or vice versa.
- If renderer API and OpenXR binding differ, treat cross-API sharing as a
  separate capability project. Require identical image definitions, valid
  external ownership transitions, semaphore synchronization and a real pixel
  round trip. On the tested Steam Frame Zink/Turnip setup, imported shared-image
  storage was not enough to make a valid GL-to-Vulkan path.

## Code references

- [Khronos `XR_EXT_frame_synthesis` structure](https://registry.khronos.org/OpenXR/specs/1.1/man/html/XrFrameSynthesisInfoEXT.html)
  defines per-view metadata, affine motion decoding, NDC displacement, depth
  fields and `appSpaceDeltaPose`.
- [Unreal Engine source audit](UNREAL_FRAME_SYNTHESIS_REFERENCE.md) demonstrates
  the extension adapter handing dedicated targets into the renderer's velocity
  and depth passes, plus version-specific pitfalls.
- [Godot source audit](GODOT_FRAME_SYNTHESIS_REFERENCE.md) is a second engine
  example for extension registration, recommended sizes, swapchain lifecycle,
  renderer texture handoff and per-view chaining. Its code is version-sensitive;
  review the engine branch being ported.
- [Renderer data contract](FRAME_SYNTHESIS_RENDERER_DATA_CONTRACT.md) gives the
  detailed coordinate, timing, jitter and depth contract.
- [Cross-API interop investigation](FRAME_SYNTHESIS_CROSS_API_INTEROP.md) records
  why API and runtime image compatibility must be proved end to end.
