# Vulkan path for Steam Frame frame synthesis

This is the implementation roadmap derived from the Steam Frame OpenXR probe,
the Quake renderer audit, and Unreal/vkQuake source references. It separates a
small runtime proof from the much larger task of porting the game renderer.

## Evidence and limits

The Steam Frame Vulkan OpenXR probe enabled `XR_EXT_frame_synthesis`, enumerated
`VK_FORMAT_R16G16B16A16_SFLOAT` and `VK_FORMAT_D32_SFLOAT`, and created and
enumerated per-eye motion and depth swapchains. A follow-up standalone Vulkan
frame-loop fixture rendered a moving rectangle with color, FP16 motion, and
depth, then submitted 120 frames with synthesis metadata on both eyes. It
generated opposite signed X/Y motion in the two eyes;
GPU readback verified those FP16 vectors, static zero vectors, and 0.35/1.0
depth samples. This proves data generation, image transport, and API submission
for a controlled test pattern. It does not prove that SteamVR used the EXT data
to generate/display intermediate frames. See the [format probe](../../../performance-results/spacewarp-vulkan-format-probe-20261013/RESULTS.md)
and [frame-loop result](../../../performance-results/spacewarp-vulkan-frame-loop-20261014/RESULTS.md).

The later isolated OpenGL matrix changes the renderer decision: on the tested
Frame runtime, an sRGB color chain and the experimental sRGB motion chain
create/enumerate without shared-image failures, but every OpenGL depth chain
tested emits three SteamVR depth-stencil/shared-image errors **during**
`xrCreateSwapchain`. This persists for D16, D24, D32F, attachment-only usage,
and with the synthesis extension disabled. In the Vulkan control, both-eye
FP16 motion and D32 depth chains create/enumerate without those errors. The
OpenXR creation functions return success in both cases, so runtime diagnostics
must be captured independently from API results. See the
[isolated GL/Vulkan swapchain report](SPACEWARP_GL_OPENXR_SWAPCHAIN_PROBE_2026-10-14.md).
Khronos' `XrFrameSynthesisInfoEXT` requires a valid depth subimage, so dropping
depth is not a conformant workaround. On this tested stack, the native Vulkan
OpenXR binding is required to avoid the failing GL depth-swapchain import, but a
native Vulkan game renderer is not yet required. A later pixel-level test
proved that a GL/Zink-produced RGBA8 image can be synchronized through shared
Vulkan memory and copied into an acquired OpenXR-owned Vulkan color image. This
reopens a hybrid architecture: keep Quake's OpenGL renderer, bind the OpenXR
session to Vulkan, and transfer color plus synthesis inputs through explicit
shared images. RGBA8 color and RGBA16F motion have pixel-checked transport into
acquired OpenXR-owned Vulkan images, including an asymmetric origin check. An
earlier D32 transfer probe also matched pixels, but its captured destination
image omitted transfer usage, so it does not validate a legal depth transport
path. The depth-attachment fallback subsequently passed a rasterized GL
`gl_FragDepth` fixture: three acquired XR D32 images, 4,096 sampled pixels each,
zero mismatch, with no runtime-image transfer copy. See the
[raster result](../../../performance-results/spacewarp-depth-raster-20261014/RESULTS.md).
The later live color bridge is described below; synthesis consumption remains
unproven. See the [cross-API interop report](FRAME_SYNTHESIS_CROSS_API_INTEROP.md)
and [dated format probe](SPACEWARP_GL_FORMAT_INTEROP_PROBE_2026-10-14.md).

The current Quake renderer is OpenGL. After adding finite guards to depth and
flow generation, the staged game submitted the sRGB motion path on Steam Frame.
This confirms app-side OpenXR submission, not headset consumption. The early
on/off/on/off captures reported 24.071 ms versus 13.889 ms average frame time,
but a later audit found the scripted single-player simulation had been paused
for every `gore_headpop_24` run (`vr_xr_unfocused_pause 1`, game time fixed at
1.300 s). The counters show no kills, particles or decals. The timing remains
evidence of producer/synchronization overhead on a frozen-world render path; it
does not represent gore or combat performance. Profile again with gameplay
advancing and event counters asserted before making a renderer decision. See
[SpaceWarp test status](SPACEWARP.md) and the
[corrected A/B report](../../../performance-results/spacewarp-gore-abba-20261014/RESULTS.md).

## Milestone 1: prove Vulkan data and frame submission — fixture complete

The preceding bootstrap gate is also verified on Steam Frame: a separate
enable2 probe used `xrGetVulkanGraphicsDevice2KHR`,
`xrCreateVulkanInstanceKHR` and `xrCreateVulkanDeviceKHR` to build the
runtime-selected Vulkan device and OpenXR session, then created and enumerated
FP16 motion and D32 depth chains for both eyes. This is the required device
creation pattern for the eventual renderer integration, and is now wired into
Quake's opt-in hybrid color-bridge startup. It does not switch the renderer to
native Vulkan. See the dated
[enable2 probe record](SPACEWARP_VULKAN_ENABLE2_PROBE_2026-10-14.md).

The standalone fixture is `Misc/quakevr/tests/vr_openxr_vulkan_frame_loop.cpp`.
It is now an explicit CMake target: configure with
`-DQVR_BUILD_OPENXR_VULKAN_FIXTURE=ON`, then build
`qvr_openxr_vulkan_frame_loop`. Vulkan and OpenXR headers/loaders are checked
when this option is enabled. The target is intentionally not registered in
CTest because executing it requires a running OpenXR runtime and a compatible
Vulkan device; see [TESTING.md](TESTING.md) for the manual run and evidence
limits. The target has been configured and built successfully on the Steam
Frame development host.
It runs independently of Quake's renderer and has completed 120 frames on the
Steam Frame runtime. It uses the runtime-recommended 1728×1728 synthesis size,
per-eye FP16 motion and D32 depth swapchains, opaque environment blending, and
attaches one `XrFrameSynthesisInfoEXT` to each projection view. It now draws a
moving indexed quad in each eye through the new `vr_vulkan_bsp` device-local
upload and material-range draw recorder. Separate graphics passes write color,
FP16 motion and depth; one-pixel GPU readback verifies opposing signed vectors,
expected foreground/background depth, and distinct eye color output. The CPU
packet is currently synthetic in this fixture; Quake's loaded BSP packet is not
yet connected to this draw path.

The CMake target was rerun on 2026-10-14 through the connected Steam Frame
runtime: 120/120 frames carried EXT metadata, the one-frame-history warm-up
left 119 frames with nonzero motion, and the process reported zero failures.
See the [captured rerun](../../../performance-results/spacewarp-vulkan-frame-loop-20261014/cmake-target-rerun.txt).
This confirms the build target and runtime submission path together; it is not
evidence that SteamVR generated or displayed intermediate frames.

The remaining fixture gate is runtime consumption and visible output:

1. Select SteamVR's **Draw Performance Assessment Game Textures in Headset**
   diagnostic and verify the runtime-displayed motion/depth fields in both eyes.
2. Capture compositor/performance diagnostics with synthesis enabled and
   disabled; `xrEndFrame == XR_SUCCESS` and successful GPU readback alone do not
   prove that an intermediate frame was generated.
3. Test headset-visible camera motion, artificial locomotion and recovery, then
   connect the renderer to Quake only after the standalone runtime behavior is
   understood.

## Queue and image synchronization

Use the Vulkan queue bound to the OpenXR session. Record the required barriers
for each acquired image and return the image to the layout/ownership required
by the runtime before release. Submit image-writing command buffers to the
bound queue before `xrReleaseSwapchainImage` and `xrEndFrame`.

The OpenXR Vulkan concurrency rules permit released swapchain images to still
be referenced by already-submitted work on that queue. A CPU fence wait before
every release is therefore not the default solution and can unnecessarily
serialize CPU and GPU. Serialize OpenXR calls that touch the queue against the
engine's queue submission/presentation lock. Follow the exact enabled OpenXR
spec version and runtime requirements; do not permit unsynchronized concurrent
queue access.

## Tested candidate: keep Quake's GL renderer with a Zink/Vulkan bridge

An on-device capability fixture now confirms that Frame's OpenGL context is
Zink/Turnip on the same Adreno 750 physical device as Vulkan: `GL_EXT_memory_object_fd`
and `GL_EXT_semaphore_fd` are advertised, and OpenGL's device and driver UUIDs
match Vulkan's `VkPhysicalDeviceIDProperties` values exactly. Vulkan reports
OPAQUE_FD export/import support for RGBA8, RGBA16F and D32 images with
`VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT` at the tested attachment/sampled/transfer
usages, and for binary semaphores. The image formats report `DEDICATED_ONLY`,
so each shared image must use a dedicated Vulkan memory allocation and
`GL_DEDICATED_MEMORY_OBJECT_EXT` on import. See the
[capability probe](../../../performance-results/spacewarp-gl-vk-capabilities-20261014/RESULTS.md).

The OpenXR Vulkan fixture also created color, FP16 motion and D32 depth
swapchains for both eyes with `XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT` added, then
completed 120 EXT submissions. The runtime therefore accepts transfer-destination
usage for each tested format; an actual `vkCmdCopyImage` into acquired XR images
remains to be tested. See the [frame-loop record](../../../performance-results/spacewarp-vulkan-frame-loop-20261014/RESULTS.md).

The early capability, storage, and API-dump tests exposed a real Zink image-
definition mismatch and no Vulkan `EXTERNAL` queue-family handoff; allocation
size or matching UUIDs alone were insufficient. A later exact-definition test
then established a working, narrowly scoped path: on this Frame image, a
dedicated 64×64 RGBA8 image configured with Zink's captured flags, usage, and
view formats passed bidirectional pixel checks, explicit Vulkan `EXTERNAL`
ownership transfers, and a copy into an acquired OpenXR-owned Vulkan color
image. This supersedes the earlier conclusion that no shared-image pixel path
was proven, but it does not establish equivalent behavior for RGBA16F or D32.
See the [exact result](../../../performance-results/gl-vulkan-interop-roundtrip-20261014/RESULTS.txt)
and [OpenXR color-copy result](../../../performance-results/spacewarp-vulkan-frame-loop-20261014/openxr-gl-xr-copy.txt).

The isolated asymmetric-format gate has passed. The fixture matched Zink's
RGBA16F and D32 image definitions exactly, tested signed X/Y motion and a depth
ramp, copied each format into acquired OpenXR swapchain images, checked all
pixels over three resource-reuse cycles, and explicitly validated GL-bottom to
Vulkan/XR-top row mapping. This led to a Quake-owned, opt-in Vulkan2 color
bridge in `vr_backend_openxr.cpp`: Quake's existing GL renderer writes to
GL_RGBA8 staging textures, then a Zink/Vulkan bridge copies eye and panel color
into acquired Vulkan OpenXR swapchain images. The mode is selected with
`vr_xr_vulkan_interop 1` and the Linux bridge build option; ordinary OpenGL
remains the default. The integration has compiled in full-engine ARM64 ON and
OFF builds, with 2/2 and 1/1 CTest cases respectively. Astra reviewed the
failure and teardown paths. The later worn color tests and the subsequently
implemented synthesis adapter are recorded below; they have separate evidence
levels. The color test did not enable synthesis.

The first color gate subsequently passed short worn tests. The backend now
contains the opt-in Vulkan motion/depth adapter described below, requesting
sampled usage for compositor reads and exporting GL producer targets. The
separate asymmetric transport fixture does not substitute for live Quake
synthesis checks. Profile bridge cost and confirm runtime texture receipt before
deciding whether a native Vulkan renderer is warranted. The probe report is
[here](SPACEWARP_GL_FORMAT_INTEROP_PROBE_2026-10-14.md).

## Alternative product direction: native Vulkan renderer port

A native Vulkan renderer remains a viable long-term option, especially if the
GL-to-Vulkan bridge proves too costly in the live game or blocks needed
rendering features. It is not required for the next integration step: color,
FP16 motion and D32 depth transport are now proven in isolated tests on this
Frame device. Choose the renderer path after integrating the bridge and
measuring its cost rather than treating a full renderer port as already
selected.

A Vulkan OpenXR binding cannot consume the existing OpenGL texture names. The
backend interface must expose opaque Vulkan image/view handles, and the engine
must render to the OpenXR images or copy/resolve compatible engine targets into
them. Preserve the OpenXR frame lifecycle across both eyes so images are not
released before their GPU writes are submitted.

The VR backend boundary now describes runtime images with a `GraphicsImage`
record: graphics API tag, opaque image and optional view handles, native format,
and extent. Eye, panel, motion and depth images use this descriptor. Current
render consumers explicitly accept OpenGL and reject other APIs while releasing
acquired images correctly. This prevents accidental GL-name/Vulkan-handle
reinterpretation and gives a future Vulkan renderer a typed handoff point. The
current opt-in bridge does create Vulkan resources and bind an OpenXR Vulkan2
session, but renderer consumers still receive GL staging textures; Quake does
not render its scene through native Vulkan yet. The earlier OpenGL consumer
build was verified on Steam Frame in the isolated
`build-spacewarp-api-images-20261014` build tree.

The next CPU-side seam now exists in `Quake/vr/vr_bsp_mesh.*`: after the
existing brush CPU builder assigns each surface's `vbo_firstvert`, the adapter
can collect only the loaded world's opaque faces, fan-triangulate them into
32-bit indices, and retain per-surface material/range boundaries. Invalid
surface offsets, vertex spans, and texture indices fail the packet build. An
additional upload packet now owns compact position/UV vertices and local
indices, so a Vulkan buffer upload need not borrow the transient CPU array used
by the GL renderer. The CPU-only fixture checks source-attribute copying,
non-opaque filtering, material boundaries, bounds failures, and unchanged
output on failure. The actual loaded-world packet is not called by the running
renderer yet. A generic `vr_vulkan_bsp` uploader/draw recorder is exercised by
the shader fixture on Steam Frame using a synthetic packet with the same
layout. The remaining gate is feeding the loaded-world packet into Quake's
Vulkan OpenXR frame path.

The module can now be compiled into the engine with
`-DQVR_ENABLE_NATIVE_VULKAN_BSP=ON`; this is off by default and requires the
Vulkan SDK. It links the GPU uploader/draw recorder into `ironwail` but does not
select a Vulkan OpenXR session or change the active renderer. The next product
gate remains the runtime-selected Vulkan device/session and dispatch from
Quake's per-eye render loop into the native path.

For this Quake branch, vkQuake is the practical renderer base. Its source
creates the Vulkan device and submits command buffers in `Quake/gl_vidsdl.c`;
its frame-pass description and recording in `Quake/r_passes.c` currently
describe a single scene/UI frame. Stereo needs per-eye or multiview targets and
recording, plus VR-specific projection and entity hooks. The existing porting
guide maps the gameplay module hooks, but its line-count estimate is optimistic:
the backend handles, frame lifecycle, render pass graph, and synthesis shader
pipeline all need architectural work.

### vkQuake source audit and first spike

The source audit used Novum/vkQuake commit
`33ad15e6c9a5303c1af1050f63391baad289b4e7` as a read-only reference. It is a
renderer donor, not a drop-in replacement for this Ironwail-derived game fork.
The most useful starting seam is `render_framebuffer_images_t` and
`R_RecordFrame` in `Quake/r_passes.h` / `Quake/r_passes.c`, but vkQuake still
assumes a window swapchain and frame-global render state. Its final
`PRESENT_SRC_KHR` transition must be changed for OpenXR-owned render targets.

OpenXR device selection also has to happen before Vulkan resources are created:
vkQuake's `GL_InitDevice` currently chooses a physical device itself, while its
SDL path creates the Vulkan instance/device directly. The XR session and
renderer must agree on the exact instance, physical device, logical device, and
queue. The current Quake VR module initializes lazily after video setup, so this
ordering is a core architectural change, not just another eye-render callback.

Do not transplant projection math by name. vkQuake's `GL_FrustumMatrix` uses a
camera looking down -Z, a Y flip and reversed infinite Z; Quake VR's current eye
projection uses different forward/depth conventions. Begin with a finite,
asymmetric projection whose color, depth and motion conventions agree, and
update frame-synthesis validation and metadata together if changing those
conventions. Likewise, upstream vkQuake lacks this fork's IQM and custom VR
bone-pose path (`PV_IQM` / `VR_AliasBonePoses`), which is a full-game porting
blocker even after an opaque BSP renders.

The lowest-risk first spike keeps the current game fork and adds an optional
native Vulkan renderer target, borrowing vkQuake renderer components where
useful. First render a stock Quake BSP and alias models into two distinct
OpenXR-owned eye color targets with asymmetric projections and serial per-eye
recording. Then add matching native velocity/depth targets and
`XR_EXT_frame_synthesis` metadata, and only after that port the remaining VR
gameplay hooks. This isolates XR binding, device/queue setup and stereo render
graph risks before IQM, weapons, hands, HUD and other fork-specific features.

`R_BuildBModelVertexData()` builds the existing brush vertex array on the CPU,
and the OpenGL path uploads and frees that same array as before. The opaque BSP
packet adapter now turns the world's eligible surfaces into checked fan indices
and draw ranges without depending on GL calls. Neither CPU product is consumed
by Vulkan yet; Vulkan buffers/pipelines, per-eye matrices, retained depth,
material upload, and OpenXR frame submission remain to be implemented. Both the
engine target and the isolated packet fixture compiled on Steam Frame.

## Live color bridge checkpoint: worn tests, 2026-10-14

The default-off Vulkan2/GL color mode now builds in full-engine ARM64 OFF/ON
configurations. Isolated portable builds (`ENABLE_USERDIRS=OFF`) passed matching
mock controls and short native panel/stereo runs without replacing the installed
game. Synthesis was forced unavailable throughout. In the worn firing-range
test, 864² output per eye rendered about 71–72 frames/s; the wearer reported
that the image looked correct. At 1728² output, the wearer reported a somewhat
better image, but rendering dropped to about 36–37 frames/s and the runtime
period became 27.78 ms. Internal render scale remained 0.45 in both tests;
1728² describes XR output size, not full internal scene resolution.

The release/copy instrumentation averaged about 9.8–10 ms per app frame at
864² and 20.3–22.1 ms at 1728², including both-eye bridge/fence/release work.
This cost must be profiled before adding synthesis producer passes. Ordinary
runtime throttling/reprojection with synthesis disabled does not demonstrate
EXT frame generation. Startup GPU-fence warnings remain unresolved. Six
zero-descriptor import errors followed disconnect in the low-scale runs, with
none during steady state; the captured full-output interval had no such import
errors. A runtime teardown race is plausible, not proven harmless. All processes
exited and the final captured SteamVR state was idle with dashboard restored.

Exact builds, binary/source hashes, launch commands, warnings, wearer evidence
and capture limits are in the
[color-bridge results](../../../performance-results/spacewarp-color-interop-20261014/RESULTS.md).
## Live synthesis adapter: source integration, device validation pending

The Vulkan2 backend now negotiates `XR_EXT_frame_synthesis` when both
`vr_xr_vulkan_interop 1` and `vr_xr_frame_synthesis 1` are selected before VR
startup. Both settings remain off by default, and the bridge remains a separate
Linux CMake opt-in. The ordinary OpenGL path remains available.

The adapter requires the runtime's enumerated `R16G16B16A16_SFLOAT` motion and
`D32_SFLOAT` depth formats and nonempty per-eye EXT recommended dimensions.
Each eye owns separate motion/depth swapchains, acquired indices, and staging
bridges. Motion requests color-attachment, sampled, and transfer-destination
usage. Depth requests depth-attachment and sampled usage; its destination is
written through a full-screen depth pass in its unchanged OpenXR attachment
layout. No native depth create flags or transfer capability are inferred.

The existing GL producer receives GL_RGBA16F and GL_DEPTH_COMPONENT32F staging
textures. It exports current-minus-previous motion in Vulkan NDC signs;
transport reverses image rows without negating vector components again. A
rejected export completes the external semaphore/ownership cycle without
reading pixels or writing an XR destination. Every acquired runtime image stays
tracked until successful release or controlled teardown.

A frame token and independently recorded indices gate readiness. Metadata is
attached only when both eyes report current-frame exports and successful
motion/depth releases, finite metric depth/decoding values, and a valid shared
rigid application-space delta. Both projection views receive the same serialized
delta. Held, loading, warm-up, failed, and rejected frames omit synthesis.
Producer history still commits only after successful stereo color submission.
`FRAME_SYNTHESIS_CONTENT` diagnostics expose frame freshness, motion/depth
indices, shared-delta validation, app-side submission success, and per-eye
export/release CPU wait time (not an isolated GPU-copy duration).

CPU tests exercise the production freshness/metadata contracts and native
destination guards, including differing motion/depth indices, stale tokens,
partial releases, and invalid/reversed depth. These checks do not establish
pixel correctness or runtime consumption. The full current Quake engine passed
x86-64 Linux and native Frame AArch64 bridge-ON and bridge-OFF builds; each
passed its corresponding CPU tests. This **Quake producer-to-Vulkan synthesis
integration** has not been run in a Quake OpenXR session on Frame. The earlier
isolated Vulkan fixture's 120 EXT submissions and worn color-only Quake runs
remain valid, separate evidence.
Exact host commands, SDK revisions and logs are in the
[integration results](../../../performance-results/spacewarp-synthesis-interop-20261014/RESULTS.md).

Next device gates are paired live motion/depth texture inspection, producer and
bridge profiling, ordinary color fallback/restart checks, then evidence of
runtime-generated intermediate frames and dynamic-scene quality. Optical flow
remains an approximation with known cost; it is not Unreal's geometry-velocity
pass. Scene depth is captured before postprocess/UI while motion uses final
composited color, so UI, translucency and some effects can lack matching depth
even when the buffers are fresh. Those regions need renderer-specific
depth/velocity coverage or explicit exclusion before a broad quality claim.
Timestamped `INTEROP_TEARDOWN` markers now cover bridge drain, XR resource
destruction, terminal queue drain, and Vulkan destruction. The earlier startup
GPU-fence and post-disconnect import warnings remain unresolved; no blind delay
was added.

The preceding synchronous raster fixture contains three initialized cancellation/reuse
cycles for each staging format, plus D32 draw/cancel/redraw/copy cycles whose
readback must match the new pattern. It passes strict compilation and Vulkan
1.1 shader validation, and that version passed on the Frame GPU: all 12
cycles completed and all 12,288 depth pixels matched. SteamVR logged a GPU-fence
signal warning during the zero-layer test; its cause remains unresolved. See
the [current fixture capture and results](../../../performance-results/spacewarp-synthesis-interop-20261014/RESULTS.md).

## Build, activation and remaining gates

The verified ARM64 route is a native Linux build on the Frame development host,
using its SDL/OpenGL/OpenXR/Vulkan development libraries. The current WSL host
has no ARM64 compiler/sysroot; its x86-64 executables cannot be deployed as
Frame binaries. The build-only runner
[`build_frame_synthesis.sh`](../../Misc/quakevr/tests/build_frame_synthesis.sh)
performs portable OFF/ON full builds, CTest, ELF architecture verification and
binary hashes without opening an XR session. It passed on Frame against the
current source snapshot with a separate build root. The ON ARM64 binary has
also run a deterministic 450-frame Quake combat replay in a scratch game copy,
not the installed Steam title. Both-eye motion/depth swapchains and fresh
synthesis metadata reached the app submission chain, but the HMD presence
sensor was absent; this does not verify compositor-generated frames.

| Gate | Verified evidence | Remaining check |
|---|---|---|
| Engine compilation and CPU contracts | Pre-pairing x86-64 and Frame ARM64 OFF/ON full builds; CTest 3/3 and 4/4 | Rebuild latest paired-submit/profiler checkpoint for ARM64 |
| Motion/depth shaders | Production GLSL compile/link; attachment/readback SPIR-V validation | Actual current producer pixels on Frame |
| Isolated Vulkan EXT submission | 120 metadata frames, 119 nonzero-motion frames, known sample checks on Frame | Runtime use/generated intermediates |
| Isolated bridge transport | Historical RGBA8/FP16; Frame synchronous D32 attachment readback and 12 cancel/reuse cycles | Run expanded paired motion/depth fixture, then current producer in Quake |
| Quake color/panel transport | Prior ARM64 and worn color-only runs | Regression check using current binary |
| Quake synthesis integration | Current ARM64 ON replay: live RGBA16F/D32 chains, repeated fresh both-eye metadata, 24-death workload validation; no HMD presence | Worn session, runtime depth/motion receipt, compositor-generated cadence |
| Synthesis performance | A-B-B-A replay on Frame; ON +6.58 ms mean frame and +8.42 ms mean GPU-eyes elapsed; detailed ON profile identifies 21.1 ms/frame in synchronous motion/export scopes | Paired submits are implemented and host-tested; repeat A-B-B-A and wearer check on the new binary |
| Runtime output and lifecycle | Earlier app submissions and clean process exits | Runtime-displayed motion/depth, generated-frame cadence/quality; startup fence and post-exit import warning attribution |

For a scratch portable run, set `vr_backend openxr`,
`vr_xr_vulkan_interop 1` and `vr_xr_frame_synthesis 1` **before VR startup**.
Keep `vr_xr_frame_synthesis_srgb_motion 0` and relaxed interval 0 for the FP16
baseline. The build option must be ON; changing the cvars after instance
creation does not enable an unrequested extension. Retain the known working
X11/runtime environment and `-nobindless`, `-nosteam`, `-noconfigwrite`, explicit
scratch `-basedir`, and `-qvr-frame-diagnostics` from the
[color run](../../../performance-results/spacewarp-color-interop-20261014/RESULTS.md).
`run_frame_synthesis_smoke.sh` can use the Vulkan2 path through
`QVR_VULKAN_INTEROP=1` and `QVR_SRGB_MOTION=0`; it backs up and restores both
configuration files and QuakeC bytecode under the selected scratch game root.

The next wearer test is a bounded color-only baseline followed by the same
static scene with synthesis enabled, valid head poses and both-eye runtime
motion/depth visualization. The automated A-B-B-A profile shows the synthesis
path currently adds synchronization and GPU cost; preserve that result as the
before baseline for the bridge batching change rather than assuming SpaceWarp
already improves smoothness.
Require `synthesis targets=1`, RGBA16F/D32 creation and repeated
`FRAME_SYNTHESIS_CONTENT fresh=1/1` with successful app submission; these are
app-side gates, not proof of interpolation. Capture matching runtime texture
and cadence evidence, then test camera motion and artificial locomotion.
Only after those gates should animated geometry, effects and UI quality be
assessed. Capture `INTEROP_TEARDOWN` alongside runtime logs and return to a
color-only run to check recovery. The current zero-layer fixture session does
not satisfy any of these wearer/runtime-generation gates.

## Motion producer and performance sequence

The first Frame A-B-B-A pass shows a real cost, but its `cpu_busy_ms` field
misclassifies synchronous synthesis bridge waits as busy work: those waits
happen under the synthesis motion scope rather than the named color `xr release`
scope. A detailed ON capture measured about 21.1 ms/frame of motion/export CPU
elapsed for both eyes, including queue/fence waits, and about 3.1 ms of GPU
elapsed in that scope. The optical-flow child accounts for about 0.84 ms GPU
elapsed; depth capture about 0.10 ms. These scopes are frame-wide elapsed
intervals, not isolated shader execution on an otherwise idle GPU.

The paired-submit source now queues both same-eye transfers on the same
XR-bound Vulkan queue, waits once on the later submission's bounded fence,
checks the earlier fence and releases both XR images only after both complete.
Pending bridges reject GL/command reuse; failed pairs retain XR acquisitions
and drain during teardown. Stereo blocking waits fall from six to four, but
there are still six submissions/GL flushes, and no Frame performance improvement
has yet been measured. Smaller targets, row-copy-array caching and D32 attachment
resource caching should be measured after the paired-submit change.

The expanded fixture now covers real paired RGBA16F/D32 copy/cancel/reuse,
independent acquired indices, 12,288 motion pixels and 12,288 depth pixels,
plus deliberate second-enqueue refusal with drain-before-release. It compiles
on the host; it has not been executed on Frame. The native build-only runner
also compiles this fixture by default (`QVR_BUILD_INTEROP_FIXTURE=0` skips it),
but never executes it. See the
[paired checkpoint](../../../performance-results/spacewarp-paired-export-20261014/RESULTS.md).

Normal benchmark JSON now records CPU phases for synthesis depth, inclusive
motion, downsample/mipmap, optical flow, encode, acquire and transport. It also
records `vr_xr_vulkan_interop` and `vr_profile`. `cpu_busy_ms` retains its
historical definition and still includes synthesis transport waits; use the
separate transport phase when interpreting it. GPU phase breakdown is an
explicit `vr_profile 1` opt-in and adds up to 16 phase timestamps per stereo
frame when `vr_profile_gpu >= 1`, besides the detailed tree's sampled queries.
`vr_profile 0` adds no new GPU queries from this breakdown. For unbiased
frame-time/CPU comparisons, capture a matching `vr_profile_gpu 0` control.
GL elapsed scopes may include stalls or other queue work; no new native Vulkan
timestamp measurement or isolated shader/copy execution claim is made.

The producer performs six optical-flow passes per eye at the current scale;
Vulkan's FP16 swapchains alone will not remove that shader cost. The preferred engine path is a native renderer
velocity pass plus matching depth, as illustrated by Unreal's mobile Vulkan
integration. Reuse renderer-generated motion for animated geometry and camera
motion for static surfaces only when conventions, resolution, jitter and
history match the submitted color.

Unreal's source audit is in [UNREAL_FRAME_SYNTHESIS_REFERENCE.md](UNREAL_FRAME_SYNTHESIS_REFERENCE.md).
General API and cross-engine rules are in [FRAME_SYNTHESIS_ENGINE_GUIDE.md](FRAME_SYNTHESIS_ENGINE_GUIDE.md).

## Batched export review checkpoint — 2026-10-14

Deferred color submission now tolerates restoration of the producer's GL sRGB
state after detachment. Additional CPU-only enqueue/release scopes separate
parts of the batch wall time without adding GPU queries. One batch timing is
logged once, rather than as two per-eye durations. Native scratch ARM64 builds
and tests passed (ON 4/4, OFF 3/3); the raster fixture was rebuilt, not rerun.
See `performance-results/spacewarp-batch-state-review-20261014/RESULTS.md`.

The [read-only compositor collector](../../Misc/frame/FRAME_COMPOSITOR_TIMINGS.md)
passed a one-second empty-runtime smoke: 65 timing records, only async-mode
flag `0x04`, no motion flag, no scene PID and no valid headset poses. Its
`0x08` flag reports generic motion smoothing, not identified EXT input use.
The next evidence gate remains a stable Quake scene with valid wearer poses,
matched OFF/ON capture, runtime diagnostics and displayed cadence/quality.
Existing aggregate reprojection totals do not close this gate. For performance,
measure the new enqueue/release split and XR color extent before attributing
the long batch wait to motion shaders or image copies alone.

## References

- Khronos: [XR_EXT_frame_synthesis](https://registry.khronos.org/OpenXR/specs/1.1/man/html/XR_EXT_frame_synthesis.html),
  [Vulkan enable2](https://registry.khronos.org/OpenXR/specs/1.1/man/html/XR_KHR_vulkan_enable2.html),
  [Vulkan concurrency rules](https://registry.khronos.org/OpenXR/specs/1.1/man/html/XR_KHR_vulkan_enable-concurrency.html).
- Epic: [Unreal Engine 5.7 OpenXR integration](UNREAL_FRAME_SYNTHESIS_REFERENCE.md).
- vkQuake: [renderer source](https://github.com/Novum/vkQuake).
