# Cross-API transport for OpenXR frame synthesis

This note covers engines whose renderer and OpenXR session use different
graphics APIs, such as an OpenGL renderer paired with a Vulkan OpenXR binding.
It records a Steam Frame investigation as a portability case study. The methods
apply to other engines, but device results must not be generalized to other
drivers or runtimes.

## Keep OpenXR images in their owning API

OpenXR swapchain images are created for the graphics binding used by the
session. A renderer using another API should not treat those image handles as
textures from its own API. A candidate adapter can instead:

1. Render color, motion and depth into application-owned shareable images.
2. Signal an external semaphore after renderer writes complete.
3. Wait in the OpenXR-bound API and copy into acquired OpenXR swapchain images.
4. Release the OpenXR images and submit the projection views with fresh motion
   and depth metadata.

This adds copies, synchronization and format conversions. Measure their GPU
cost against a native backend. A direct Vulkan renderer that writes the OpenXR
images can avoid this transport layer, but is a larger engine project.

## Prove each layer separately

Do not use a single “interop supported” boolean. Validate in this order:

1. Compare the renderer and OpenXR API's physical-device UUID **and** driver
   UUID. Device names are not identity checks.
2. Query external memory support for each exact format, tiling, image create
   flags, usage bits and handle type. Record dedicated-only requirements and
   allocation size/type.
3. Allocate and export one small dedicated image; import the FD and create the
   receiving API's texture/storage object. Verify framebuffer or resource
   completeness.
4. Write a known pattern in API A, signal an imported external semaphore, wait
   in API B, and compare every pixel. Then perform the reverse handoff and
   repeat across resource reuse.
5. Only after that, test OpenXR-owned swapchain copies, exact layouts and queue
   ownership, then runtime motion/depth consumption in the headset.

At each step record the failing API call and native error, not only extension
strings or device logs. Ensure FD ownership/closure follows both APIs'
contracts. Test image origin, channel order, sRGB/linear interpretation,
quantization and motion encoding as part of pixel validation.

## Steam Frame result (2026-10-14)

On the tested Steam Frame software image, OpenGL used Mesa Zink over Vulkan
Turnip on an Adreno 750. GL and Vulkan device UUIDs matched, as did driver
UUIDs. Vulkan advertised OPAQUE_FD import/export for the queried RGBA8,
RGBA16F and D32 image configurations and for binary semaphores. The images
required dedicated allocations. The OpenXR Vulkan runtime also accepted
`XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT` on color, FP16 motion and D32 depth
swapchains, and a separate fixture submitted 120 synthesis frames with that
usage.

These capability and swapchain checks passed. The first actual shared-image
storage test did **not**: after Vulkan created and exported a dedicated RGBA8
image and GL successfully imported its memory FD, `glTexStorageMem2DEXT`
returned `GL_OUT_OF_MEMORY`. Zink logged failure to allocate a 20,480-byte
heap region. The fixture stopped at this point. No semaphore handoff, GPU pixel
transfer, OpenXR copy, or compositor synthesis was tested by that run.

The allocation is small, so the error does not establish ordinary VRAM
exhaustion. A follow-up diagnostic narrowed the mismatch: Vulkan reported a
16,384-byte RGBA8 image allocation (4,096-byte alignment, memory type 0),
while Zink tried to allocate 20,480 bytes when creating GL texture storage.
Separately, a dedicated 16,384-byte OPAQUE_FD Vulkan buffer imported into GL
and accepted by `glBufferStorageMemEXT`; GL reported the expected buffer size.
This shows that FD import and dedicated-memory-object handling work for the
buffer case, while image reconstruction/storage remains the failing stage.

A query-only Vulkan comparison reproduced Zink's requested image
size. A mutable RGBA8 image with the original usage and **no format list** needs
16,384 bytes. The same query with format list `[R8G8B8A8_UNORM,
R8G8B8A8_SRGB]` needs 20,480 bytes; a one-format UNORM list and the pair with
`VK_IMAGE_CREATE_EXTENDED_USAGE_BIT` also need 20,480 bytes. Every query
succeeded and retained the external-memory feature mask. This strongly supports
a format-list/image-reconstruction mismatch as the cause of Zink's 20,480-byte
request. It does not yet prove the precise `VkImageCreateInfo` used by the
installed Zink process, nor make the exported/imported images compatible.
For Vulkan-origin OPAQUE_FD imports, the import allocation size and memory type
must match the original allocation. The Vulkan validation rule therefore makes
the observed 20,480-versus-16,384-byte import attempt invalid independently of
available GPU heap space. See Khronos' [`VkMemoryAllocateInfo` requirements](https://docs.vulkan.org/refpages/latest/refpages/source/VkMemoryAllocateInfo.html)
and [`VkMemoryDedicatedAllocateInfo` requirements](https://docs.vulkan.org/refpages/latest/refpages/source/VkMemoryDedicatedAllocateInfo.html).

The next isolated test changed the exporter image to the compatible
`[R8G8B8A8_UNORM, R8G8B8A8_SRGB]` format list, queried its requirements, and made
the required dedicated 20,480-byte allocation. GL then successfully imported
the OPAQUE_FD and created 64×64 RGBA8 texture storage. This verifies that Zink
accepts the memory object and creates GL texture storage. It does not verify a
spec-valid alias: a later API dump showed Zink's internal `VkImage` differs from
the exporter's flags, usage and format list. It did not access image pixels,
transfer data, signal/wait on a semaphore, or interact with an OpenXR swapchain.

A later **GL-only** semaphore test successfully imported an external binary
semaphore, signaled it from GL, waited on it from GL, and read back the expected
pixel values. The Vulkan application submitted zero queue operations in this
test, so it proves GL-side semaphore object handling, not a cross-API handoff.
An installed-driver Vulkan API-dump capture showed Zink transitioning the
imported image from `UNDEFINED` to `GENERAL` with queue-family indices
`IGNORED`; its image access/readback also used `GENERAL`. No `FOREIGN` ownership
transition appeared. This matches the reviewed Mesa code path and gives
concrete evidence that assumptions about `SHADER_READ_ONLY` and foreign queue
ownership cannot be carried over without checking the actual API trace.

The API dump also shows that Zink creates a distinct `VkImage` definition from
the exporter's. The exporter used flags `0x8`, usage `0x9f`, and view formats
`[37, 43]`; Zink's imported GL texture used flags `0x40008`, usage `0x80097`,
and view formats `[37, 57]`. (The format values are respectively
`R8G8B8A8_UNORM`, `R8G8B8A8_SRGB`, and `A8B8G8R8_SRGB_PACK32`.) Both requested
20,480 bytes, but Vulkan's dedicated-allocation import rule requires the
imported image to be **identical** to the exporter image. These differing flags,
usage and format list violate that condition, even though their allocation
sizes match. See Khronos'
[`VkMemoryDedicatedAllocateInfo` VUID-01878](https://docs.vulkan.org/refpages/latest/refpages/source/VkMemoryDedicatedAllocateInfo.html).
GL and the exporter also use separate Vulkan instances/devices (despite
matching device and driver UUIDs); their numeric queue-family index `0` does
not by itself establish shared ownership.

The Zink code path supports this as a concrete hypothesis: importing a GL
memory object creates a new `VkImage`, may attach a format list for compatible
UNORM/sRGB views, then allocates memory from that recreated image's own
requirements. That could differ from the exported image's requirements. The
OpenGL `DEDICATED` flag is accepted and stored by Mesa's GL layer, but in the
reviewed Zink path the import callback's `dedicated` argument is unused; Zink
derives dedicated allocation from the new image's requirements. An immutable
public Mesa snapshot dated 2026-06-12 shows these code paths in
[`zink_resource.c`](https://chromium.googlesource.com/external/gitlab.freedesktop.org/mesa/mesa/+/a4c07ed881968e39a1659631ab4b9c04b0aaa01d/src/gallium/drivers/zink/zink_resource.c),
[`zink_bo.c`](https://chromium.googlesource.com/external/gitlab.freedesktop.org/mesa/mesa/+/a4c07ed881968e39a1659631ab4b9c04b0aaa01d/src/gallium/drivers/zink/zink_bo.c),
and Mesa core's
[`externalobjects.c`](https://chromium.googlesource.com/external/gitlab.freedesktop.org/mesa/mesa/+/a4c07ed881968e39a1659631ab4b9c04b0aaa01d/src/mesa/main/externalobjects.c).
That snapshot predates the tested Frame build and demonstrates a plausible
mechanism, not the exact installed implementation: Frame reports Mesa
`git-abc426bb62`, and the exact source revision could not be retrieved during
this investigation.

At that stage, the application-level bridge test was stopped before GPU access.
Do not pad the old dedicated allocation to 20,480 bytes: imported allocation
metadata must match the original allocation and dedicated image. The later
exact-definition and bidirectional pixel test below supersedes that
storage-only result for RGBA8 on this installed driver. At that point,
OpenXR swapchain copies and motion/depth formats still needed separate tests;
the later format-probe section records their result.

The initial cross-API attempt did not have a spec-valid image alias: Zink's
image definition differed from the dedicated exporter. Equal device UUIDs or
queue-family numbers alone did not fix that. The follow-up test below now
matches the complete image definition and brackets GL access with Vulkan
`EXTERNAL` ownership transfers and external semaphore ordering. It provides
pixel evidence on this exact driver, while runtime validation and OpenXR image
copy remain separate gates. See Khronos'
[`external resource sharing`](https://docs.vulkan.org/spec/latest/chapters/resources.html#external-resource-sharing)
and the [Vulkan ownership sentinel](https://docs.vulkan.org/refpages/latest/refpages/source/VK_QUEUE_FAMILY_EXTERNAL.html).

The first OpenXR-device probe imported an RGBA8 image into the GL/Zink context,
rendered a known color and read its center pixel back through Vulkan after
semaphore and EXTERNAL-ownership synchronization. A follow-up now copies that
GL-produced image into an acquired OpenXR-owned Vulkan color swapchain image
and verifies the exact pixel on the OpenXR-bound device. This proved the color
transport slice at that stage; FP16 motion and D32 depth were tested later as
recorded below. The full bidirectional shared-image round trip still uses an
ordinary standalone Vulkan device. The Khronos validation layer is not
installed in the default Frame image, so retain API-dump and pixel-level checks
for future runs.

The isolated fixture and exact device output are recorded in
[`RESULTS.txt`](../../../performance-results/gl-vulkan-interop-roundtrip-20261014/RESULTS.txt);
the GL-only API dump is
[`interop-gl-only-api-dump.txt`](../../../performance-results/gl-vulkan-interop-roundtrip-20261014/interop-gl-only-api-dump.txt);
the earlier capability probe is in
[`RESULTS.md`](../../../performance-results/spacewarp-gl-vk-capabilities-20261014/RESULTS.md).

## Exact-image and ownership round trip (2026-10-14)

A follow-up test matched the Zink image definition captured from the installed
driver: `VK_FORMAT_R8G8B8A8_UNORM`, 64×64×1, one mip/layer/sample, optimal
tiling, exclusive sharing, `UNDEFINED` initial layout, flags `0x40008`, usage
`0x80097`, and view-format list `[R8G8B8A8_UNORM,
A8B8G8R8_SRGB_PACK32]`. Both the exporter and Zink reported a dedicated
20,480-byte allocation with memory type 0. The API dump confirms Zink recreated
the same image definition and imported the allocation through OPAQUE_FD.

The test then used paired Vulkan `queueFamily 0 ↔ VK_QUEUE_FAMILY_EXTERNAL`
barriers and binary OPAQUE_FD semaphores around GL writes and Vulkan reads,
followed by Vulkan writes and GL reads. Sixteen iterations compared all 64×64
pixels in both directions: 65,536 checked pixels per direction, zero
mismatches. The fixture exits successfully on Mesa 26.3.0-devel
`git-abc426bb62`, Zink over Turnip Adreno 750. This is the first demonstrated
bidirectional pixel handoff for the tested shared RGBA8 image; it supersedes
the earlier storage-only conclusion for that exact image definition.

The Vulkan fixture's opt-in `--gl-import-probe` repeated image creation,
dedicated export and GL import using the Vulkan device created through
`xrCreateVulkanDeviceKHR` and bound to an active OpenXR session. It confirms
matching device/driver UUIDs, a 20,480-byte dedicated allocation, a complete GL
framebuffer, and an exact GL-written pixel read back through that OpenXR-bound
Vulkan device. The captured initial run is
[`openxr-gl-import-probe.txt`](../../../performance-results/spacewarp-vulkan-frame-loop-20261014/openxr-gl-import-probe.txt).

The next probe created and acquired a 64×64 OpenXR-owned color swapchain with
color-attachment, transfer-source and transfer-destination usage on the active
session. It copied the GL-produced shared Vulkan image into the acquired runtime
image and copied the center pixel back to host memory. The first attempt appeared
to fail, returning `64,0,64`; review found that the test had unmapped its
readback allocation before this second CPU read. After keeping the allocation
mapped through all readbacks, the direct black clear returned `0,0,0,255` and
the copied pixel returned `17,83,191` with zero mismatch. The OpenXR frame was
active during acquire, transfer and release, and the swapchain image was
returned in `COLOR_ATTACHMENT_OPTIMAL`. See
[`openxr-gl-xr-copy.txt`](../../../performance-results/spacewarp-vulkan-frame-loop-20261014/openxr-gl-xr-copy.txt).
The Khronos Vulkan binding requires an acquired image to be compatible with
`VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL`, owned by the session queue, and
returned to that layout/ownership before release; it also maps OpenXR transfer
usage bits to Vulkan transfer usage. See the normative
[OpenXR Vulkan binding section](https://registry.khronos.org/OpenXR/specs/1.0/html/xrspec.html#XR_KHR_vulkan_enable).

This proves RGBA8 GL→Vulkan→OpenXR-color-swapchain image transport on this
specific Frame driver/runtime combination. That color-only test did not verify
SteamVR composition of its test image, FP16 motion/depth transport from GL,
EXT motion/depth consumption, or synthesis displayed in the headset. The later
format-specific transport result follows.

## RGBA16F motion and D32 depth transport (2026-10-14)

The isolated [`vr_openxr_gl_formats_probe`](../../Misc/quakevr/tests/vr_openxr_gl_formats_probe.cpp)
now passes the same transport ladder for both motion and depth on the tested
Frame image. Storage-only API dumps confirmed that Zink recreated the exact
exporter image definitions for RGBA16F (`flags=0x40000`, `usage=0x80097`, no
view list) and D32 (`flags=0x41000`, `usage=0x80027`, no view list). The test
then wrote four asymmetric GL quadrants, performed explicit external semaphore
and Vulkan EXTERNAL ownership handoffs, copied same-format images into acquired
OpenXR-owned swapchain images, and checked both readbacks over three reuse
cycles. It checked 12,288 pixels per path and format with zero mismatches,
including signed X/Y motion and depth values 0, 0.125, 0.5 and 1.0.

The XR copy explicitly reversed rows: destination row `y` contains source GL
row `63-y`. This passed the asymmetric-origin checks for both images. The
motion components already use Vulkan NDC signs, so spatial origin conversion
did not add a second Y-component inversion. Raw stdout and API dumps are in the
[dated capture directory](../../../performance-results/spacewarp-gl-formats-probe-20261014/);
the complete test method is in the
[dated report](SPACEWARP_GL_FORMAT_INTEROP_PROBE_2026-10-14.md).

This establishes color, FP16 motion and D32 depth **transport** into
OpenXR-owned Vulkan images on this particular Zink/Turnip/SteamVR combination.
The probe does not attach projection layers or `XR_EXT_frame_synthesis`
metadata. It does not prove runtime consumption, generated-frame cadence,
headset image quality, Quake renderer integration, or production performance.
The captured flags/usages are driver-specific and must not be copied to other
devices without repeating the image-definition and pixel checks.

Quake renderer output, SteamVR texture diagnostics, runtime motion/depth
consumption and headset frame-synthesis verification have not been
demonstrated. Do not enable the bridge in gameplay until those gates pass. The
repeat source supports
`--match-zink-image --gpu-handoff`; complete logs are in
[`gpu-handoff-api-dump.txt`](../../../performance-results/gl-vulkan-interop-roundtrip-20261014/gpu-handoff-api-dump.txt)
and the image-definition trace is in
[`match-zink-image-api-dump.txt`](../../../performance-results/gl-vulkan-interop-roundtrip-20261014/match-zink-image-api-dump.txt).

## Depth barrier and destination-usage audit (2026-10-14)

A follow-up source/API-trace review found an additional Vulkan correctness gate
that the earlier pixel checks did not cover. The Zink D32 shared staging image
uses flags `0x41000`, including
`VK_IMAGE_CREATE_SAMPLE_LOCATIONS_COMPATIBLE_DEPTH_BIT_EXT`. A layout
transition on that image must preserve the applicable rasterization sample
locations. The captured Frame advertises sample-location sample counts `6`
(2x and 4x), not 1x; therefore the suggested 1x center-location chain is
invalid and must not be used. The previously passing clear/transfer checks did
not rasterize depth and do not establish preservation of `gl_FragDepth` data
through the bridge.

The application-owned staging helper now uses Vulkan synchronization2 with
`VkImageMemoryBarrier2` and equal `GENERAL` old/new layouts for D32 ownership
handoffs, preserving the external queue-family transfer without a layout
transition. It checks the synchronization2 feature and entry point and rejects
incompatible Vulkan API/device contracts. This source compiled on the Frame
ARM64 toolchain and its CPU guard test passed without opening an XR session.
The real depth handoff passed the isolated rasterized-depth fixture on
2026-10-14: 12,288 sampled runtime-image pixels matched across three cycles,
with same-run request/creation metadata and no XR transfer access. See the
[results](../../../performance-results/spacewarp-depth-raster-20261014/RESULTS.md).
This does not demonstrate gameplay synthesis, and the zero-layer compositor
session emitted a separately recorded GPU-fence error. The runtime-owned D32
destination has a separate image contract: its
captured create flags were zero, so the sample-location flag does not apply.

The trace links the mismatch to the same D32 handle: in
[`depth-pixel-api-dump.txt`](../../../performance-results/spacewarp-gl-formats-probe-20261014/depth-pixel-api-dump.txt),
lines 16026–16052 create image `0xaaaad9154750` with flags `0` and usage `36`
(`SAMPLED | DEPTH_STENCIL_ATTACHMENT`); lines 17779–17785 copy into that handle;
lines 19290–19294 read it back. The motion counterpart is internally
consistent in the trace: `motion-pixel-api-dump.txt` lines 16058–16084 create
the destination with usage `23` (`0x17`, including transfer bits), and lines
17812–17818 copy into it. The Vulkan API dump does not contain the
`xrCreateSwapchain` request. Current fixture source requests transfer usage,
but the saved run has no executable/source hash, so it is not yet proven that
the captured run used that source. The production helper now requires a
verified destination definition and transfer-destination usage and rejects
the captured D32 definition. Until the exact request is reconciled with the
returned image usage, the D32 copy result is pixel evidence only, not proof
that the copy obeyed the runtime image's usage contract. A depth-attachment
write route is required if the captured usage is confirmed. See the
engine-neutral [Vulkan depth interop note](FRAME_SYNTHESIS_VULKAN_DEPTH_INTEROP.md)
for the portable checks and Khronos specification references.

## Portable fallback decision

When shared-image import cannot be proven, keep the engine on the graphics API
it already supports and investigate a native OpenXR binding/backend for that
API, or budget a renderer port to the API supported by the runtime. Do not
silently convert signed vectors into an unvalidated 8-bit or sRGB format.
Motion vectors, depth and color must describe the same eye, render, projection
and time interval; all transport and conversion steps must preserve that
contract.

For the broader engine integration sequence, see the
[frame synthesis engine guide](FRAME_SYNTHESIS_ENGINE_GUIDE.md),
[porting playbook](FRAME_SYNTHESIS_ENGINE_PORTING_PLAYBOOK.md), and
[renderer data contract](FRAME_SYNTHESIS_RENDERER_DATA_CONTRACT.md).
