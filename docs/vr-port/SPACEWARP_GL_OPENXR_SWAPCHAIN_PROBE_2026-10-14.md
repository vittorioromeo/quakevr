# Steam Frame OpenGL swapchain import probe — 2026-10-14

## Result

An isolated OpenXR/OpenGL test narrowed the SteamVR shared-image errors to
depth swapchains. Color and sRGB motion swapchains created and enumerated
without the corresponding compositor errors. Every OpenGL depth variant tested
created/enumerated successfully at the OpenXR API boundary but caused three
`device does not support depth stencil format` / `Failed to create new shared
image: format=43 dimensions=432x432` messages in `vrcompositor.txt`.

This identifies an OpenGL depth-image sharing problem on the tested Steam
Frame/SteamVR stack as the strongest current diagnosis. The compositor's
internal failed image reports format 43 (Vulkan `R8G8B8A8_SRGB`), not the
application's depth format. Why its internal path requests that format is
unknown. A native Vulkan control created both-eye FP16 motion and D32 depth
swapchains without the matching errors. These tests did not submit projection
frames and do not show that SteamVR consumed images or generated SpaceWarp
frames.

## Method

`Misc/quakevr/tests/vr_openxr_gl_swapchain_probe.cpp` creates a small hidden
SDL/GLX context, an OpenXR session, then creates and enumerates one swapchain
at a time at 432×432. Each call records the exact format, usage flags, OpenXR
result and image count. Fresh `vrcompositor.txt` line ranges are captured after
each process. The `depth_noext` case requests the same D32 swapchain without
enabling `XR_EXT_frame_synthesis`. It creates swapchains before calling
`xrBeginSession`, matching Quake's current `createEyeSwapchains()` lifecycle.
`run_frame_gl_swapchain_probe.sh` builds and runs this matrix and then runs the
existing native Vulkan format probe.

The matrix was repeated in a fresh run at
`performance-results/spacewarp-gl-swapchain-probe-20261014-025755/`:

| Case | Requested image / usage | OpenXR result | SteamVR shared-image errors |
|---|---|---|---:|
| GL color | `GL_SRGB8_ALPHA8`, `0x1` color attachment | create + enumerate succeeded; 3 images | 0 |
| GL motion | `GL_SRGB8_ALPHA8`, `0x21` color attachment + sampled | create + enumerate succeeded; 3 images | 0 |
| GL depth | `GL_DEPTH_COMPONENT32F`, `0x22` depth attachment + sampled | create + enumerate succeeded; 3 images | 3 |
| GL depth, attachment only | `GL_DEPTH_COMPONENT32F`, `0x2` depth attachment | create + enumerate succeeded; 3 images | 3 |
| GL depth 24 | `GL_DEPTH_COMPONENT24`, `0x22` | create + enumerate succeeded; 3 images | 3 |
| GL depth 16 | `GL_DEPTH_COMPONENT16`, `0x22` | create + enumerate succeeded; 3 images | 3 |
| GL depth without EXT | `GL_DEPTH_COMPONENT32F`, `0x22`; synthesis extension not enabled | create + enumerate succeeded; 3 images | 3 |
| Vulkan control | Both eyes: `VK_FORMAT_R16G16B16A16_SFLOAT` motion + `VK_FORMAT_D32_SFLOAT` depth, usage `0x21`/`0x22` | create + enumerate succeeded; 3 images per chain | 0 |

OpenXR Vulkan format enumeration identified format 43 as `R8G8B8A8_SRGB`, 97
as `R16G16B16A16_SFLOAT`, and 126 as `D32_SFLOAT`. The standalone Vulkan
control used the same runtime and image extent. No app submitted a frame in
this probe.

A separate fresh run of `vr_openxr_vulkan_frame_loop.cpp` completed 120/120
frames with `synthesis=120`, 119 nonzero-motion checks and zero fixture
failures. SteamVR logged no depth-stencil/shared-image errors during that run,
but did log one separate GPU-fence signal error. Its aggregate was 118 presents
and 2 reprojected frames; those generic counters do not prove EXT output. The
frame-loop stdout, status, and compositor log range are saved under
`performance-results/spacewarp-gl-swapchain-probe-20261014-025927/vulkan_frame_loop/`.

## Interpretation limits

- OpenXR success proves that the runtime accepted swapchain creation and
  returned application API images. It does not prove SteamVR imported those
  images into a usable compositor path.
- The depth-only cases show that OpenGL depth swapchain creation is sufficient
  to trigger the compositor errors on this stack, independently of the EXT
  extension and independently of D16/D24/D32 choice or the sampled usage bit.
- The original Quake log's errors had been tentatively associated with the
  motion swapchain because timestamps were close to its app log line. The
  isolated depth-only result and exact triple-buffer count make OpenGL depth
  sharing the stronger attribution. The old log alone cannot prove which
  original call triggered each error.
- The Vulkan control proves successful swapchain create/enumerate and 120
  app-side frame submissions with tested motion/depth data. Runtime receipt,
  EXT structure consumption, generated-frame cadence, headset image quality,
  and replay-based wearer GPU profiling remain unverified.

A later color-only interop probe verified a known GL/Zink-produced RGBA8 pixel
copied through a shared Vulkan image into an acquired OpenXR-owned Vulkan color
swapchain image on the OpenXR-bound device. A direct XR-image black
clear/readback also passed. At that stage, color transport was proven for the
Frame setup, while GL depth swapchain sharing remained problematic and FP16
motion/D32 depth transport still awaited a dedicated test. The later format
probe passed both motion and depth transport; see the detailed
[cross-API report](FRAME_SYNTHESIS_CROSS_API_INTEROP.md),
[format-probe result](SPACEWARP_GL_FORMAT_INTEROP_PROBE_2026-10-14.md), and
[earlier color pixel-test output](../../../performance-results/spacewarp-vulkan-frame-loop-20261014/openxr-gl-xr-copy.txt).

The probe timestamps each OpenXR call to the nearest millisecond, while the
SteamVR log records microseconds. In the captured run, the depth
`xrCreateSwapchain` call began at **02:59:29.271**; all three shared-image
failures were logged between **.271895** and **.281104**, and the call returned
success at **.281**. The color/motion controls emitted no corresponding
failures. Raw stdout and compositor deltas are kept in the
[timestamped capture](../../../performance-results/spacewarp-gl-swapchain-probe-20261014-025927/RESULTS.md).
This directly associates the messages with OpenGL depth-chain
creation, rather than relying on their coarse proximity to Quake's motion-chain
messages. Together with the six-error/two-chain count in Quake's old capture,
depth import is now the strongest explanation for those original errors.

The next product direction is to carry the verified GL color transport through
the actual Quake renderer and independently prove matching motion and depth
transport, or move Quake's renderer fully onto the OpenXR Vulkan binding. The
GL depth path has no format/usage workaround among the advertised OpenGL depth
formats tested here. Omitting depth is not a valid frame-synthesis fallback:
`XrFrameSynthesisInfoEXT` requires a valid `depthSubImage` as well as motion
data ([Khronos structure contract](https://registry.khronos.org/OpenXR/specs/1.1/man/html/XrFrameSynthesisInfoEXT.html)).
The exact RGBA8 Zink image definition and GL↔Vulkan handoff now pass pixel
validation on Frame, but that does not solve depth/motion interchange or prove
compositor consumption. See the
[Vulkan roadmap](SPACEWARP_VULKAN_ROADMAP.md) and
[cross-API investigation](FRAME_SYNTHESIS_CROSS_API_INTEROP.md).
