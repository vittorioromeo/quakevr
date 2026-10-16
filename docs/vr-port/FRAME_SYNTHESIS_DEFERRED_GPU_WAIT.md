# Deferred CPU fence wait for the GL/Vulkan bridge

`vr_xr_vulkan_defer_wait 1` is an experimental, default-off scheduling option.
It submits the full stereo color/motion/depth batch to the session-bound
Vulkan queue, releases the OpenXR images and submits the frame without an
immediate CPU fence wait. Before the next use of each bridge, a bounded fence
wait still protects its staging image, command buffer, descriptors and fence.
The GL/Vulkan external semaphores and image barriers are unchanged.

## Why release before GPU completion is legal

The Khronos `XR_KHR_vulkan_enable2` specification explicitly allows runtime
images at release to be referenced by incomplete command buffers submitted
on the graphics binding's queue. The application must restore the required
attachment layout and queue ownership in those commands. Queue submission
and queue-accessing OpenXR calls must be externally synchronized; this backend
performs them serially on its render thread.

Sources: [Khronos specification source, Swapchain Image Layout and Concurrency](https://github.com/KhronosGroup/OpenXR-Docs/blob/main/specification/sources/chapters/extensions/khr/khr_vulkan_enable2.adoc)
and [published extension specification](https://registry.khronos.org/OpenXR/specs/1.1/html/xrspec.html#XR_KHR_vulkan_enable2).

This permits deferring the host wait. It does not permit writing a released XR
image, resetting an in-flight command buffer/fence, overwriting staging while
it is being copied, or destroying an image still used by submitted commands.

## Guards and failure paths

- The default remains synchronous (`0`) and the panel uses the synchronous path.
- Only a successfully submitted *entire* batch can skip its terminal wait.
  A partially failed batch drains its submitted prefix and rejects the frame.
- Pending release requires a ready, unpoisoned bridge whose enqueue returned
  successfully and which no longer has an open GL write.
- Next acquisition waits for that bridge's completion before resource reuse,
  including when the option has since been switched off.
- Shutdown drains every bridge before destroying XR swapchains. Failed drains
  retain the dependent objects and native parents.
- `FRAME_LAYERS defer_wait=1` identifies the requested active transport mode;
  `synthesis reuse wait` is a separate CPU profile scope. A smaller current-frame
  `synthesis transport` span alone is not a speedup: compare total frame time
  and the new reuse-wait scope because waiting may only have moved.

## Reproduce

Use the normal smoke runner with the same source, binary, scene and synthesis
setting throughout. Set `QVR_VULKAN_INTEROP=1`, `QVR_FRAME_SYNTHESIS=1`, and
vary `QVR_VULKAN_DEFER_WAIT=0/1/1/0` in an A-B-B-A sequence. The runner restores
both game configuration files and `progs.dat` after every run.

Native build, CPU contract and unworn replay results are recorded in the
workspace's `performance-results/spacewarp-batch-state-review-20261014/RESULTS.md`.
None of these establish wearer-visible correctness or runtime-generated frames.
The tracked-headset motion/depth and display tests in
[headset validation](FRAME_SYNTHESIS_HEADSET_VALIDATION.md) remain required
before changing the default.
