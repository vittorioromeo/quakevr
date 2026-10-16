# Paired motion and rasterized D32 bridge fixture

This optional Linux/Steam Frame diagnostic tests the existing `Bridge` depth
attachment fallback and paired handoff independently of Quake. It opens a Vulkan2 OpenXR session,
ends four frames with zero layers, and never enables frame synthesis. Run only
in a coordinated idle session. It does not install or launch the game.

```sh
cmake -S Misc/quakevr/tests/raster_depth_probe -B /tmp/qvr-raster-build \
  -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/qvr-raster-build -j2
spirv-val --target-env vulkan1.1 /tmp/qvr-raster-build/bridge_depth_readback.comp.spv
DISPLAY=:1 SDL_VIDEODRIVER=x11 XDG_RUNTIME_DIR=/run/user/1000 \
  XR_RUNTIME_JSON=/opt/steamvr/steamxr_linuxarm64.json \
  VK_INSTANCE_LAYERS=VK_LAYER_LUNARG_api_dump timeout -s INT -k 5s 45s \
  /tmp/qvr-raster-build/qvr_raster_depth_probe --run \
  --api-dump /tmp/qvr-raster-depth-api-dump.txt \
  > /tmp/qvr-raster-depth-api-dump.txt 2>&1
```

Use the Frame user's running X11 session and readable SteamVR ARM64 runtime
manifest; the loader, Vulkan ICD, API-dump layer and GL/Zink driver must be
installed and accessible in that user's environment. The fixture creates a
hidden SDL window, but an XR session can still take compositor focus. Run only
when the device session is available; the build-only runner never invokes it.
Do not run the Linux/Frame-profile fixture against this PC's SteamVR runtime.
The test's UUID and extension checks reject an incompatible GL/Vulkan stack.

The `--api-dump` argument must identify this process's stdout capture. The
fixture reads its flushed capture to match each returned XR image handle to
its actual `vkCreateImage` definition; it rejects missing or unexpected
definitions. The API dump layer is required. This diagnostic parser is not a
production eligibility check. Production destinations derive their usage
guarantees from the successful OpenXR swapchain request. The capture is
separate test evidence for this fixture's additional sampled-readback operation.

The fixture requests only sampled and depth-attachment XR usage (`0x22`). It
derives production `guaranteedUsage` from that successful request, logged as
`XR_REQUEST_CONTRACT`. D32 always uses attachment rendering, even if a runtime
image happens to have transfer usage. The fixture separately requires a
flags-zero, single-sample captured definition for its sampled-readback layout
transitions; captured flags are never fed into Bridge acceptance. Those
observations are logged as `XR_CAPTURED_EVIDENCE productionEligibility=0`.
Vulkan API >=1.1, synchronization2 and
the staging-image features are queried and enabled on the OpenXR-selected
device; the GL and Vulkan device/driver UUIDs must match.

Before rasterization, the current fixture completes three unwritten
`beginGlWrite -> cancelGlWrite` cycles for each of RGBA8, RGBA16F and D32.
Each next begin consumes the reverse semaphore from the preceding cancel.
No XR destination is accessed by cancellation. The raster portion now draws
and cancels a paired motion/depth pattern, reacquires both bridges, draws a
different pattern and exports both images through `completePairedExports`.
The fixture acquires RGBA16F and D32 XR chains independently, logs both indices,
waits the terminal depth fence and checks the earlier motion fence before
readback or either XR release. Motion is transfer-read into persistently mapped
host memory; the test-only motion request explicitly guarantees TRANSFER_SRC.
Depth uses sampled readback. Three cycles verify 12,288 FP16 pixels (including
signed X/Y and reversed rows) and 12,288 depth pixels. A fourth zero-layer frame
deliberately refuses the second enqueue, asserts pending bridges reject reuse,
then drains/cancels before releasing either image. Total cancellations: 17.
A bridge failure retains borrowed native parents rather than entering unbounded
generic device cleanup. This paired revision has compiled on the host but has
**not been executed on Frame**. The refusal is injected on the host; it does
not simulate a GPU timeout or device loss.

GL rasterizes a changing asymmetric quadrant pattern with `gl_FragDepth`,
using depths 0, 0.125, 0.5 and 1, then detaches the imported D32 target.
`Bridge::copyTo` samples staging in GENERAL and writes the XR image through
a fullscreen depth-attachment pass. Its row reversal maps GL bottom-origin
to XR/Vulkan top-origin. A compute shader samples the runtime image into an
application-owned storage buffer. The fixture never transfer-copies XR D32.
It restores depth-attachment layout before release and validates all 4,096
pixels in each of three cycles.

The preceding synchronous-source Frame rerun passed all 12 cancellation cycles and all
12,288 depth pixels. Runtime images had Vulkan usage `0x24` (sampled + depth
attachment), flags 0 and one sample. The same-run API dump is
[frame-raster-cancel-api-dump.txt](../../../../../performance-results/spacewarp-synthesis-interop-20261014/frame-raster-cancel-api-dump.txt)
(SHA-256 `8b231efa20c1092f77503d0e6a11ef4ae33b8bee5107aa44bd24ade53112f2f6`).
See the matching [build and fixture results](../../../../../performance-results/spacewarp-synthesis-interop-20261014/RESULTS.md).

SteamVR logged `Failed to signal gpu fence 74093!` at 08:26:20.083 +02 during
the zero-layer session. It then logged 19 startup presents, 0 dropped and 7
reprojected before the test process disconnected. The cause remains
unclassified. A D32 1D image-format capability query also returned
`VK_ERROR_FORMAT_NOT_SUPPORTED`; the fixture's 2D D32 swapchain and all
readbacks succeeded. CMake accepts either `glslc` or `glslangValidator` and
honors `Vulkan_INCLUDE_DIR`. This diagnostic does not test Quake synthesis,
runtime-generated frames, wearer quality or performance.

This proves the isolated rasterized-depth transport path for the captured
driver/runtime. It does not prove gameplay integration, synthesis consumption,
generated-frame cadence, wearer image quality or performance. The API dump
is not a substitute for Vulkan validation. The test reuses bootstrap helpers
from `vr_openxr_gl_formats_probe.cpp` without running that probe's main.
