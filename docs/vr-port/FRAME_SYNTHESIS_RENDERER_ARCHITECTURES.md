# Choosing a frame-synthesis renderer architecture

This note helps an engine team select a producer architecture for
`XR_EXT_frame_synthesis`. The OpenXR adapter is broadly reusable; the motion and
depth producer depends on what the renderer already exposes and on whether the
renderer uses the same graphics API as the OpenXR session.

## First classify the renderer

### Renderer already has geometry motion and depth

Use a dedicated pass or extend the render graph to write into the acquired
OpenXR targets. Before reusing a TAA/velocity texture, verify its sign, units,
projection jitter, render resolution, dynamic-resolution mapping, depth
convention, clear values, object coverage and history lifetime. TAA velocity
often serves a different purpose and may not satisfy OpenXR's contract.

For each visible point, EXT motion is current NDC position minus previous NDC
position over the submitted frame interval. Use Vulkan-style NDC conventions
for the exported vector even if the main renderer is OpenGL. Export matching
depth from the same frame and projection. Preserve camera motion, moving object
transforms, skeletal animation and vertex/material deformation.

### Renderer has depth but no motion pass

For a small prototype, combine depth reprojection for camera motion with
previous/current object transforms for dynamic geometry. This can work for
simple opaque scenes, but needs a defined policy for disocclusions, particles,
transparency, first-person weapons, sky and newly spawned objects. Test those
classes directly.

An image-space optical-flow pass can estimate motion from consecutive color
images when geometry history is unavailable. It is an approximate producer,
not equivalent to engine motion vectors: lighting changes, flat regions,
occlusion boundaries and effects can produce ambiguous flow. It also adds GPU
work. Profile it separately and keep it behind a clear quality/performance
tradeoff.

### Renderer has neither reusable depth nor motion

Add a minimal opaque depth and velocity pass before attempting general content.
A diagnostic scene with one static wall and one translating shape gives known
answers and exposes sign, axis and origin errors. Do not begin by estimating
flow from a complex game frame; that combines producer correctness with scene
ambiguity and makes failures hard to localize.

## Then classify graphics-API compatibility

| Renderer API vs OpenXR session API | Recommended route | Required proof |
|---|---|---|
| Same API | Render directly into acquired OpenXR motion/depth images. | Correct format/usage, image transitions, ownership, per-eye output and runtime diagnostic receipt. |
| Different APIs, engine can select session binding | Prefer a native renderer backend for the API used by OpenXR. | Session binding and all rendering/resource operations use the selected API; profile the native path. |
| Different APIs, neither can be changed | Prototype shared-image transport only when both APIs expose compatible external memory and semaphore support. | Identical image definitions, allocation size/type, tiling, layout/ownership transitions, synchronization and a pixel round trip. Then test the OpenXR-owned destination image. |

Matching device names or UUIDs and successful external-memory import are not
enough to prove that two APIs are using the same compatible image. The tested
Steam Frame Zink path eventually passed exact image-definition, synchronization,
asymmetric pixel, row-origin, and OpenXR-destination checks for RGBA8, RGBA16F,
and D32. This is a useful hybrid reference, but it does not prove compositor
consumption or establish identical behavior on another device. See the
[dated results](SPACEWARP_GL_FORMAT_INTEROP_PROBE_2026-10-14.md) and full
[interop investigation](FRAME_SYNTHESIS_CROSS_API_INTEROP.md). If transport
cannot be proven on a target engine/runtime, use the session's native API or
disable synthesis cleanly.

## OpenXR adapter and frame ownership

Keep engine-independent renderer code free of raw OpenXR structures. An
adapter should provide acquired targets associated with a frame token and
per-view metadata only after the renderer confirms that every eye wrote fresh
motion and depth. Track color, motion and depth indices independently. A
failed eye, render-graph skip, invalid history or target resize should submit
ordinary color for the stereo frame and invalidate synthesis history.

Treat the engine render thread as the owner of GPU writes and API-specific
synchronization. The OpenXR submission thread owns projection-layer chaining.
The frame scheduler binds these sides to the same predicted display time,
projection, eye poses, target rectangles and renderer-history generation.

Reset or warm up history after tracking loss, skipped/held frames, focus
recovery, teleports, snap turns, scene changes, camera cuts, target-size
changes, renderer restart or any incomplete stereo frame. Compute an
application-space locomotion delta once from consecutive frame snapshots and
reuse it for every eye; tracked head motion already belongs in the eye views.

## Minimal validation ladder

1. **Lifecycle:** extension absent/disabled, session restart, resize, failed
   allocation and one-eye failure all preserve normal projection rendering.
2. **Transport:** acquire, wait, write, transition, release and submit a
   known clear/pattern image in the exact runtime-owned swapchains.
3. **Vectors:** test +X/-X/+Y/-Y, camera-only movement, object-only movement,
   static pixels, both eyes, render-target Y origin and NDC/depth conventions.
4. **Content:** test opaque geometry, skeletal animation, particles,
   translucency, first-person weapon, sky and disocclusion policies.
5. **Runtime:** inspect received motion/depth textures, then compare displayed
   cadence, compositor timing and image quality with the extension on/off.
6. **Cost:** profile simulation, producer pass, synchronization, compositor
   and total displayed behavior separately on a repeatable workload.

OpenXR's recommendation of signed 16-bit floating-point motion channels is a
useful default, not a substitute for querying the active binding's enumerated
formats. If only normalized formats are available, define and GPU-test an
explicit signed encoding, affine decode, transfer function, range, precision
and out-of-range policy. Never silently clamp vectors or interpret a generic
8-bit color fallback as motion.

Related references: [renderer data contract](FRAME_SYNTHESIS_RENDERER_DATA_CONTRACT.md),
[adapter template](FRAME_SYNTHESIS_ENGINE_ADAPTER_TEMPLATE.md),
[porting playbook](FRAME_SYNTHESIS_ENGINE_PORTING_PLAYBOOK.md), and
[headset validation procedure](FRAME_SYNTHESIS_HEADSET_VALIDATION.md).
