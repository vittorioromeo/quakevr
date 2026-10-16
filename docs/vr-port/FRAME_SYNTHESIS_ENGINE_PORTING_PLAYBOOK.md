# Porting OpenXR frame synthesis to another engine

This playbook turns the Steam Frame and Unreal source investigation into an
engine-neutral implementation sequence for `XR_EXT_frame_synthesis`. It is
written for engine maintainers deciding where the feature belongs and how to
prove each layer independently. Keep `XR_FB_space_warp` as a separate backend:
the extension structures and motion encoding contracts are not interchangeable.

## Start with capability, not a renderer assumption

Treat support as a chain of capabilities rather than one boolean:

1. The runtime advertises the extension.
2. The application opts in and enables it when creating the OpenXR instance.
3. The selected graphics binding exposes a usable motion format and depth format.
4. Swapchains can be created at supported dimensions and usage flags.
5. The renderer can populate matching color, motion, and depth data for each eye.
6. Fresh per-view metadata reaches `xrEndFrame` with correct history and pose data.
7. Runtime diagnostics show that the runtime consumes the data and produces the
   intended display behavior.

Log each stage independently. Keep ordinary projection submission working if
any optional stage fails. An extension-enabled log or successful `xrEndFrame`
only proves part of this chain.

## Recommended implementation sequence

### 1. Keep the OpenXR layer thin

Add an opt-in feature at the OpenXR integration boundary. Enumerate extensions
before instance creation, query the per-view recommended motion dimensions,
enumerate formats after creating the graphics-bound session, and allocate
runtime-owned images. Do not assume that support on one graphics API implies
support on another. The Steam Frame investigation found signed FP16 motion and
D32 depth swapchains on Vulkan; its tested OpenGL session exposed only normalized
color formats for motion.

Have the runtime adapter own swapchain acquire/wait/release and the composition
chain. Have the renderer own production of the data. The adapter should not
guess or convert engine velocity conventions silently. Keep the EXT and FB
composition builders separate, and preserve all chained structs until
`xrEndFrame` returns.

### 2. Reuse the renderer's velocity and depth passes

The engine needs current NDC position minus previous NDC position for the image
being submitted, plus depth from that same image. This is displacement over the
submitted interval, not velocity divided by elapsed time. Use the renderer's native velocity
pass where possible. An existing TAA velocity texture is a candidate, not an
automatic match: inspect its coordinate space, sign, jitter treatment, object
motion coverage, resolution, clear value, and lifetime first.

Unreal 5.7 and 5.8 provide a concrete example. Their OpenXR plugin requests
runtime-sized motion targets and hands them to the mobile renderer. The renderer
computes current clip-space position minus previous clip-space position and
device-depth delta from current and previous transforms. This includes camera
and object motion, rather than estimating all motion from final-color images.
See the [source audit](UNREAL_FRAME_SYNTHESIS_REFERENCE.md). The specific
Vulkan-mobile and multiview requirements are Unreal implementation choices;
another engine should use the same renderer/OpenXR separation while querying its
own runtime and graphics binding.

If the engine has no velocity pass, a lower-quality image-space estimator may
be used as a prototype. It must be measured and validated against dynamic
objects, low-texture regions, disocclusions, transparency, particles, and large
motion. It adds rendering work and can cost more GPU time than synthesis saves.

### 3. Add a deterministic proof before real scenes

Build a standalone OpenXR test that renders a stationary background and a
moving opaque shape with analytically known motion and depth. Give the eyes
visibly different colors. Test positive and negative X and Y motion, camera-only
motion, object-only motion, static pixels, and both depth conventions used by
the renderer. Start with a clear-only submission to validate frame lifecycle
and swapchain transport; then add the moving geometry before claiming vector
correctness.

Use SteamVR's **Draw Performance Assessment Game Textures in Headset** option
to inspect the submitted depth and motion textures, then save a headset view
of both eyes. Capture the SteamVR Assessment result and
`~/.local/share/Steam/logs/perfcriteria.txt` with synthesis enabled and
disabled; the file records the frame times that lead to a performance
threshold violation. Also collect compositor timing and displayed cadence.
The texture overlay establishes that the runtime received the supplied data;
it does not by itself prove that intermediate frames were generated. An
API-success-only test is a transport smoke test, not feature validation.

### 4. Integrate behind explicit history rules

Submit synthesis only when all eyes have fresh color, motion, and depth for the
same display interval. Reset or warm up history after tracking loss, focus
changes, skipped frames, map/scene changes, teleports, abrupt display-time
changes, and render-size changes. Never pair new color with stale vectors. If
one eye fails, submit a normal color frame for both eyes and reset history.

The app-space pose delta and projection metadata are part of the temporal
contract; they are not just optional tuning knobs. Keep transforms in the
coordinate spaces expected by OpenXR and verify sign/order with known movement.

### 5. Measure the complete cost

Record application CPU and GPU time, compositor timing, submitted and displayed
frame counts, and vector/depth images for the same repeatable workload. Test
the feature both enabled and disabled. Frame synthesis may improve perceived
motion when the app misses native refresh, but it does not lower scene-render
cost. Extra velocity, depth, resolve, or optical-flow passes can worsen the
bottleneck.

## Renderer ownership checklist

- Color, motion, and depth identify the same eye, pose, projection, viewport,
  and displayed frame interval.
- Motion sign and units are measured with a known translation; X/Y axes and
  render-target origin are explicit.
- Camera and animated-object motion are covered, with a policy for particles,
  translucency, UI, sky, and newly visible pixels.
- Stationary visible geometry still receives correct camera-induced motion;
  depth and motion targets are cleared once and preserved across subsequent
  opaque/translucent passes.
- App-space delta is computed once from consecutive frame poses, converted to
  meters, and reused consistently for every projection view. Acquired motion
  and depth swapchain indices are tracked independently.
- TAA jitter is removed or reproduced consistently; MSAA depth is resolved
  deliberately; dynamic resolution and multiview layer mapping are tested.
- Runtime-recommended synthesis dimensions are respected. If downsampling is
  used, the filter preserves vector/depth meaning rather than averaging across
  object boundaries.
- All image formats, transfer functions, attachment usage, layouts, queue
  ownership, and synchronization are validated for the chosen graphics API.
- Failure of an optional image or metadata path returns to ordinary projection
  rendering without stale synthesis data.

## Evidence in this repository

The Steam Frame Vulkan probe created per-eye FP16 motion and D32 depth
swapchains. A subsequent Vulkan frame-loop fixture submitted 120 frames with
`XrFrameSynthesisInfoEXT`; GPU readback verified nonzero signed motion in the
moving shape, zero motion on the static background, and matching depth. This
does not establish that SteamVR used those fields to generate intermediate
frames. See the
[Vulkan roadmap](SPACEWARP_VULKAN_ROADMAP.md) and the dated
[Frame-loop result](../../../performance-results/spacewarp-vulkan-frame-loop-20261014/RESULTS.md).
The Quake OpenGL flow path and its limitations are recorded in
[SpaceWarp status](SPACEWARP.md).

Quake's current OpenGL implementation created experimental sRGB motion/depth
swapchains and submitted EXT metadata during scripted tests on Steam Frame.
The later corrected 450-frame `gore_headpop_24` ON/OFF/A/B replay asserted 25
deaths in every run. It estimated about 1.44 ms extra average frame time and
0.78 ms extra GPU eye time with synthesis enabled. That is producer overhead;
there is no measured benefit to subtract because SteamVR receipt and generated
frames remain unverified. HMD presence was absent, so these numbers are not a
wearer performance result. The earlier frozen-world replay and its limitations
remain documented separately in the
[older A/B report](../../../performance-results/spacewarp-flow-2iter-abba-20261014/RESULTS.md);
use the [validated gore replay](../../../performance-results/spacewarp-gore-real-abba-20261014/RESULTS.md)
and [death-event profile](../../../performance-results/spacewarp-gore-real-detail-20261014/RESULTS.md)
for the corrected event evidence.

## Primary references

- Khronos: [`XR_EXT_frame_synthesis`](https://registry.khronos.org/OpenXR/specs/1.1/man/html/XR_EXT_frame_synthesis.html),
  [`XrFrameSynthesisInfoEXT`](https://registry.khronos.org/OpenXR/specs/1.1/man/html/XrFrameSynthesisInfoEXT.html),
  and [`XR_KHR_vulkan_enable_concurrency`](https://registry.khronos.org/OpenXR/specs/1.1/man/html/XR_KHR_vulkan_enable-concurrency.html).
- Valve: [Steam Frame performance assessment](https://partner.steamgames.com/doc/steamhardware/steamframe/compat/perf_criteria)
  and [SteamVR debugging](https://partner.steamgames.com/doc/steamhardware/steamframe/debugging).
- Epic: [Unreal 5.7 source reference](UNREAL_FRAME_SYNTHESIS_REFERENCE.md).
- Godot: [source audit](GODOT_FRAME_SYNTHESIS_REFERENCE.md) of OpenXR lifecycle,
  render-target handoff and version-specific assumptions.
