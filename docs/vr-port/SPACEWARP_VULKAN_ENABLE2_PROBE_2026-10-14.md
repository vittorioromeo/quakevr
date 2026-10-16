# Steam Frame OpenXR Vulkan enable2 probe — 2026-10-14

## Result

The runtime-selected Vulkan bootstrap path succeeded on Steam Frame. The probe
enabled `XR_KHR_vulkan_enable2` and `XR_EXT_frame_synthesis`, obtained the
runtime-selected physical device with `xrGetVulkanGraphicsDevice2KHR`, created
the Vulkan instance and device through `xrCreateVulkanInstanceKHR` and
`xrCreateVulkanDeviceKHR`, and created an OpenXR Vulkan2 session. It then created
and enumerated both-eye `R16G16B16A16_SFLOAT` motion and `D32_SFLOAT` depth
swapchains (three images each). Exit status was zero.

The runtime required Vulkan API 1.0 and reported a maximum of 1.2. The probe
used 432×432 test images. Its compositor-log delta contains only the test
process's connection and disconnect messages; no swapchain or format errors.

## Evidence and limits

- [Raw probe output](../../../performance-results/spacewarp-vulkan-enable2-20261014/stdout.txt).
- [Status and compositor line range](../../../performance-results/spacewarp-vulkan-enable2-20261014/status.txt).
- [Isolated compositor log delta](../../../performance-results/spacewarp-vulkan-enable2-20261014/vrcompositor-delta.txt).
- [Probe executable SHA-256](../../../performance-results/spacewarp-vulkan-enable2-20261014/probe.sha256).

This proves that SteamVR can supply the Vulkan physical device and create the
Vulkan OpenXR session/swapchains through enable2. It does not prove Quake's
renderer uses that device, that a projection frame was submitted, or that
SteamVR consumed synthesis data or displayed generated frames. The Quake
integration must create its renderer device through the same runtime-provided
path before creating Vulkan resources, then render into and submit the acquired
OpenXR images.
