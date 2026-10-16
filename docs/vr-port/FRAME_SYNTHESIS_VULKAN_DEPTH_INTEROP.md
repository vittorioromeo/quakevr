# OpenXR frame synthesis: Vulkan depth interop pitfalls

This engine-neutral note records a depth-image synchronization issue found
while transporting renderer outputs into OpenXR `XR_EXT_frame_synthesis`
swapchains. It applies to any Vulkan engine or cross-API adapter. The concrete
flags below came from one Steam Frame / Mesa Zink / Turnip / SteamVR trace and
are evidence for that configuration, not defaults for another runtime.

## Core rule

Treat depth image creation flags, usage, layout, ownership and sample-location
metadata as one contract. A same-format copy that returns matching pixels in a
small probe does not by itself prove the Vulkan operations obey the image's
creation contract.

In the captured Zink image definition, D32 used flags `0x41000`, including
`VK_IMAGE_CREATE_SAMPLE_LOCATIONS_COMPATIBLE_DEPTH_BIT_EXT` and
`VK_IMAGE_CREATE_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_BIT_EXT`. The image had
one sample, but its compatible-depth flag still makes layout transitions
sample-location-sensitive. A legacy `VkImageMemoryBarrier` layout transition
does not carry the last rasterization sample locations. Do not guess a
single-sample center location: the captured device advertised only 2x and 4x
sample-location counts, so a 1x `VkSampleLocationsInfoEXT` is invalid there.
The initial `UNDEFINED` → `GENERAL` transition may discard the never-written
image contents; Vulkan treats missing sample-location state as undefined
contents, which is valid for that initialization step. After the renderer writes
depth, preserve the contents across ownership handoffs with same-layout
synchronization2 barriers.

The Quake GL/Vulkan staging helper now enables and queries
`VK_KHR_synchronization2` (or Vulkan 1.3 core where available) and uses
`VkImageMemoryBarrier2` with `oldLayout == newLayout ==
VK_IMAGE_LAYOUT_GENERAL` for shared D32 ownership handoffs. Synchronization2 specifies that equal layouts do not
perform a layout transition, while the barrier can still express memory
dependencies and queue-family ownership transfer. Preserve the required
external queue-family handoff and semaphore order. Fail closed if the feature
is unavailable or if the actual producer/consumer layout contract requires a
transition. The source compiled on the Steam Frame ARM64 toolchain and a CPU
guard test passed. Production runtime D32 uses a successful OpenXR
depth-attachment request as its normative contract. The helper
samples its shared staging D32 image and writes the acquired XR image
through a single-sample fullscreen depth-attachment pass. The fragment shader
uses `texelFetch` and writes `gl_FragDepth`; it reverses rows to match the
GL-to-Vulkan transfer path. This route requires single-sample storage,
guaranteed depth-attachment usage and the expected attachment layout, without
knowing native runtime image creation flags. Runtime D32 remains in
`DEPTH_STENCIL_ATTACHMENT_OPTIMAL`; its memory dependencies also use sync2
equal-layout barriers, avoiding sample-location-sensitive transitions. D32
transfer is rejected even if transfer usage was requested. Runtime sampling is
not required for production depth output; the fragment shader samples the
application-owned staging image. Color and motion transfer copies require both
guaranteed transfer-destination and color-attachment usage.

`Destination::guaranteedUsage` is derived by a pure factory from `XR_SUCCESS`
and the exact successful `XrSwapchainCreateInfo`. The Vulkan OpenXR contract
allows images to be used as if created with at least the usage bits mapped from
that request. API-dump metadata is optional diagnostic evidence, not a
production prerequisite or a queryable Vulkan property. The caller must still
pair the enumerated image with its own request and bound session, and only mark
it acquired/waited after both XR operations return success. See the normative
[swapchain usage mapping](https://registry.khronos.org/OpenXR/specs/1.1/man/html/XR_KHR_vulkan_enable-swapchain-flags.html).

The attachment fallback **passed an isolated GPU run on 2026-10-14**. GL
rasterized asymmetric changing depths through `gl_FragDepth`; the helper wrote
three acquired/waited XR D32 images through its attachment pass. Compute
sampling into an app-owned storage buffer verified all 12,288 pixels with
correct row reversal and zero mismatches, without transfer access to XR D32.
The same hashed run requested XR sampled + depth-attachment usage `0x22` and
matched every returned image to flags 0, Vulkan usage `0x24`, D32, one sample.
See the [fixture](../../Misc/quakevr/tests/raster_depth_probe/README.md) and
[capture](../../../performance-results/spacewarp-depth-raster-20261014/RESULTS.md).
The compositor logged one GPU-fence error during this zero-layer session;
the cause is unclassified. API-dump and pixel checks are not Vulkan validation
or proof of compositor synthesis consumption. Live gameplay, simultaneous
color/motion/depth, generated-frame cadence and wearer quality remain separate
integration gates. This result is isolated transport evidence, not completed
SpaceWarp support. That capture retains the earlier source/binary hashes.
The later normative-contract refactor and additional sync2 runtime-depth
barrier have passed strict Frame compilation and CPU contracts, but have not
been GPU-run again; do not relabel the historical capture as that newer build.

Rechecked on 2026-10-14 against the Frame ARM64 build: the helper, public
header, embedded SPIR-V and CPU test hashes matched the local sources; the CPU
test printed `PASS bridge CPU guards: no native API resources or XR session
created`, and both depth shaders passed `spirv-val --target-env vulkan1.1`.
This is compile/contract evidence only. Keep the device-dependent attachment
test separate from ordinary CTest because it needs the SteamVR OpenXR runtime.
The strict helper/CPU-test command was:

```sh
clang++ -std=c++17 -fno-exceptions -DQVR_HAVE_GL_VULKAN_BRIDGE \
  -Wall -Wextra -Werror -Wno-missing-field-initializers \
  $(pkg-config --cflags sdl2 gl vulkan) -IQuake/vr \
  Quake/vr/vr_gl_vulkan_bridge.cpp \
  Misc/quakevr/tests/vr_gl_vulkan_bridge_test.cpp \
  $(pkg-config --libs sdl2 gl vulkan) -o vr_gl_vulkan_bridge_test
./vr_gl_vulkan_bridge_test
```

References: Khronos [`VkImageMemoryBarrier2`](https://docs.vulkan.org/refpages/latest/refpages/source/VkImageMemoryBarrier2.html),
[`VkImageMemoryBarrier`](https://docs.vulkan.org/refpages/latest/refpages/source/VkImageMemoryBarrier.html),
[`VkSampleLocationsInfoEXT`](https://docs.vulkan.org/refpages/latest/refpages/source/VkSampleLocationsInfoEXT.html),
and the [Vulkan resource/layout rules](https://docs.vulkan.org/spec/latest/chapters/resources.html).

## OpenXR destination images are a separate contract

Do not copy staging-image flags onto runtime-owned swapchain images. In the
captured Frame API dump, runtime D32 destination images had flags `0`; they do
not inherit the staging image's sample-location-compatible-depth requirement.
For Vulkan sessions, a successful `xrWaitSwapchainImage` guarantees the
acquired image is owned by the session's bound queue and has a layout
compatible with color-attachment or depth-attachment use. Before
`xrReleaseSwapchainImage`, return it to that compatible attachment layout and
the bound queue's ownership. The runtime must not access that queue during the
wait call. These lifecycle rules are specified by Khronos'
[`XR_KHR_vulkan_enable-swapchain-image-state`](https://registry.khronos.org/OpenXR/specs/1.1/man/html/XR_KHR_vulkan_enable-swapchain-image-state.html).
The captured D32 trace creates image `0xaaaad9154750` with flags `0` and usage
`0x24` (sampled plus depth/stencil attachment), then copies into that same
handle. The motion trace, by comparison, creates its runtime image with usage
`0x17`, which includes transfer bits, before copying into it. The Vulkan API
dump does not include the `xrCreateSwapchain` request. The current probe source
requests transfer usage, but the saved run has no binary/source hash linking
that source to the capture. Confirm the request and actual native image usage
in one hashed run. OpenXR's Vulkan usage mapping says the runtime must make a
returned image usable as if it had at least each requested Vulkan usage bit.
Therefore, if the app really requested `XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT`
but the same-run image definition omits `VK_IMAGE_USAGE_TRANSFER_DST_BIT`,
investigate a source/binary mismatch or runtime conformance issue; do not
silently accept the mismatch. If the app did not request transfer destination,
the current experimental fallback is the depth-attachment pass described
above. Validate its image definition and GPU behavior before relying on it.
See Khronos' [`XR_KHR_vulkan_enable-swapchain-flags` mapping](https://registry.khronos.org/OpenXR/specs/1.1/man/html/XR_KHR_vulkan_enable-swapchain-flags.html).
Do not infer transfer support from format support or a successful API call.

For both source and destination, query/record the exact format, tiling, create
flags, usage bits, sample count, layout, queue family, ownership and supported
operations. Use Vulkan validation layers when the runtime permits them and
retain an API dump plus pixel-level checks when it does not.

## Portable validation sequence

1. Record the exact source and runtime image definitions from the same run.
2. Query the needed Vulkan features and format/usage support; enable only the
   features actually used.
3. Exercise depth with rasterized geometry and `gl_FragDepth` (or the engine's
   real depth-writing path), not only `vkCmdClearDepthStencilImage`.
4. Test handoff, copy/write, readback and repeated reuse with asymmetric values
   and multiple depths; verify every pixel and row origin.
5. Validate sample-location state wherever a layout transition is required.
6. Only then submit synthesis metadata and separately confirm runtime receipt,
   generated-frame cadence and headset image quality.

Current Quake/Frame status and trace links are maintained in
[`FRAME_SYNTHESIS_CROSS_API_INTEROP.md`](FRAME_SYNTHESIS_CROSS_API_INTEROP.md).
This note describes a correctness gate; it does not claim the live-game bridge
or SteamVR frame synthesis is finished.
