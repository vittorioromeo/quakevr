# Frame synthesis support across engines and runtimes

Status snapshot: 2026-10-14. This survey separates runtime extension availability
from engine integration and from proof that a runtime actually synthesizes
displayed frames. “SpaceWarp” is often used informally for both
`XR_EXT_frame_synthesis` and `XR_FB_space_warp`; their submission structures
and data contracts must be treated separately.

## Findings at a glance

| Layer / engine | What the source or registry confirms | What still needs checking in a port |
|---|---|---|
| OpenXR runtimes | The OpenXR runtime inventory currently lists `XR_EXT_frame_synthesis` for SteamVR (Steam Frame), Android XR, several PICO devices and Meta Quest runtimes. | Inventory presence is not a format report, application enablement, or proof of runtime frame generation. Query the actual runtime and selected graphics binding at startup. |
| Unreal Engine 5.7 / 5.8 | Epic has an opt-in OpenXR path that hands dedicated runtime motion/depth targets to the mobile renderer's velocity/depth passes. The audited path is constrained to Vulkan mobile, multiview and direct velocity rendering. | Treat those renderer gates and RHI mappings as Unreal-specific. Validate each target engine's format mapping, image lifetime and renderer velocity convention. |
| Godot | The OpenXR settings expose Frame Synthesis. The audited implementation couples swapchain lifecycle to the extension wrapper and passes the images to renderer velocity/depth outputs. Current documentation says it is stereo-only and unavailable in Forward+. | Source callback order and renderer support vary by release. Check the exact engine commit and renderer; a development-branch lifecycle regression has occurred and was fixed through follow-up changes. |
| Unity | This investigation did not establish a built-in Unity OpenXR implementation of `XR_EXT_frame_synthesis`. Unity's OpenXR packages and provider extensions are versioned separately, so absence from this audit is not proof that no third-party or vendor integration exists. | Inspect the exact Unity version, OpenXR package, target provider and render pipeline. If no integration exists, the portable OpenXR adapter and renderer handoff described here are implementation starting points, not a drop-in Unity feature. |
| Quake VR / Steam Frame | A native Vulkan fixture submitted 120 known color/motion/depth frames with EXT metadata. GL/Zink probes pixel-checked RGBA8 and FP16 transport. A separate rasterized D32 attachment-write fixture passed 3×4,096 sampled pixels without transferring runtime depth. The opt-in color-only Vulkan2/GL bridge passed ARM64 OFF/ON builds and short worn stereo tests: the user reported correct 864² output and somewhat better 1728² output. Source integration now connects the GL motion/depth producer to per-eye Vulkan RGBA16F/D32 chains with current-frame metadata gates. | The new synthesis integration has not been run on the headset. At unchanged internal render scale 0.45, color-only 1728² output throttled to ~36–37 rendered fps versus ~71–72 at 864², with substantial bridge/fence cost. Startup fence and post-disconnect low-scale import warnings remain unresolved. Runtime synthesis consumption, generated cadence, and dynamic-scene quality are unverified. |

Sources: [Khronos OpenXR runtime extension inventory](https://github.khronos.org/OpenXR-Inventory/runtime_extension_support.html),
[Godot OpenXR settings](https://docs.godotengine.org/en/latest/tutorials/xr/openxr_settings.html),
[Godot source audit](GODOT_FRAME_SYNTHESIS_REFERENCE.md),
[Unreal source audit](UNREAL_FRAME_SYNTHESIS_REFERENCE.md), and the
[Steam Frame Vulkan fixture report](../../../performance-results/spacewarp-vulkan-frame-loop-20261014/RESULTS.md).
The isolated GL-to-Vulkan/OpenXR transport tests are recorded in the
[dated interop report](SPACEWARP_GL_FORMAT_INTEROP_PROBE_2026-10-14.md).
The legal depth attachment fixture is recorded in the
[rasterized depth result](../../../performance-results/spacewarp-depth-raster-20261014/RESULTS.md);
the live worn color tests, exact commands, warnings and recovery evidence are in
the [color bridge result](../../../performance-results/spacewarp-color-interop-20261014/RESULTS.md).
The current synthesis adapter passed host x86-64 bridge-ON/OFF full builds and
CTest 4/4 and 3/3. These are separate from earlier Frame fixture results; the
new Quake synthesis path and cancellation/reuse cases remain device-untested.
See the [host integration checkpoint](../../../performance-results/spacewarp-synthesis-interop-20261014/RESULTS.md).

## What transfers between engines

The portable part is the boundary and data flow, not an engine's internal
classes or renderer feature flags:

1. Discover and optionally enable the extension before creating the OpenXR
   instance. Preserve an ordinary projection-only path.
2. Ask OpenXR for per-view recommended motion dimensions, then query formats
   for the graphics API actually bound to the session.
3. Create and own motion/depth swapchains alongside the color swapchains. Hand
   acquired images explicitly to the render thread and track each swapchain's
   image index independently.
4. Produce motion and depth for the exact color frame, eye, pose, projection
   and viewport. Prefer renderer-generated previous/current clip-space data.
5. Attach a fresh, correctly initialized EXT or FB structure to each matching
   projection view and retain its storage through `xrEndFrame`.
6. Confirm runtime receipt and generated display behavior with diagnostics;
   successful swapchain calls and `xrEndFrame` are only application-side gates.

For a renderer/API bridge, keep the producer's texture API separate from the
runtime's image API. Acquire every runtime image independently, return producer
staging handles with a current-frame token, and publish freshness only after
both the export and runtime release succeed. A failed or partial export needs
an explicit cancellation path that returns staging semaphore ownership without
reading undefined pixels. Depth attachment rendering can avoid a transfer
usage dependency and unknown native image flags. See the implemented contracts
and pending device gates in the [Vulkan roadmap](SPACEWARP_VULKAN_ROADMAP.md).

See the [engine-neutral playbook](FRAME_SYNTHESIS_ENGINE_PORTING_PLAYBOOK.md)
for the full sequence and [EXT versus FB comparison](FRAME_SYNTHESIS_EXT_FB_COMPARISON.md)
for the non-interchangeable data contracts.

## Lessons from the engine audits

- Unreal demonstrates a direct renderer route: the OpenXR layer allocates
  targets and the mobile renderer writes geometry-derived velocity and depth.
  The renderer path also makes visible stationary geometry respond to camera
  movement. Its velocity source includes previous transforms; that history must
  cover animated vertices and material deformation too.
- Unreal's version comparison found implementation pitfalls worth checking in
  any engine: clearing depth again between opaque and translucent passes can
  erase valid depth, updating a shared app-space history inside the per-eye
  loop can leave the second eye with an identity delta, and a generic 8-bit
  swapchain fallback is not automatically a valid signed motion encoding.
- Godot demonstrates a wrapper/callback route: extension lifecycle hooks own
  extension enablement, recommended dimensions, swapchain lifetime and
  per-view metadata, while the renderer owns writing the supplied images.
  Callback ordering is a real compatibility surface; prepare safely if render
  hooks arrive before swapchains are ready.
- Neither engine's implementation details establish support on every runtime.
  On Steam Frame, tested OpenGL and Vulkan format lists differed. Query each
  API independently rather than borrowing another engine's format assumption.

## Evidence vocabulary for support claims

Use these terms in bug reports and release notes so “supported” has a precise
meaning:

- **Advertised:** runtime reports the extension name.
- **Enabled:** application successfully enables it during instance creation.
- **Transport-ready:** required swapchains exist for the session's graphics
  binding and their images can be acquired and released.
- **Data-ready:** both eyes contain fresh, validated motion and depth for the
  submitted color frame.
- **Submitted:** metadata is chained to each projection view and `xrEndFrame`
  succeeds.
- **Runtime-consumed:** runtime diagnostics show the submitted textures.
- **Display-verified:** a repeatable headset test confirms generated cadence
  and acceptable image quality with synthesis enabled versus disabled.

Do not shorten these stages to one boolean. In particular, do not describe a
transport fixture as a working game implementation or a metadata submission
as proof of reprojection.
