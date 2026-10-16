# Dormant Linux OpenXR Vulkan2 bootstrap

`Quake/vr/vr_openxr_vulkan.hpp/.cpp` implements only Vulkan instance/device/queue
creation and an OpenXR Vulkan2 session binding. The current OpenGL backend
does not invoke it. The existing engine CMake source glob compiles this module
empty when `QVR_ENABLE_FRAME_SYNTHESIS_GL_VULKAN_BRIDGE=OFF`; ON defines
`QVR_HAVE_GL_VULKAN_BRIDGE` and links Vulkan. No backend, swapchain, renderer,
cvar or default-path change accompanies this module.

The isolated compile/CPU tests ran on Frame ARM64 with OFF and ON. They do not
open an XR instance/session, create a GL context or execute GPU commands:

```sh
for option in OFF ON; do
  cmake -S Misc/quakevr/tests/vulkan_bootstrap -B /tmp/qvr-bootstrap-$option \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DQVR_ENABLE_FRAME_SYNTHESIS_GL_VULKAN_BRIDGE=$option
  cmake --build /tmp/qvr-bootstrap-$option -j2
  ctest --test-dir /tmp/qvr-bootstrap-$option --output-on-failure
done
```

Both compile with `-fno-exceptions -Wall -Wextra -Werror
-Wno-missing-field-initializers`. OFF requires no Vulkan/OpenXR/GL dependencies.
ON compiles all bootstrap API calls, links their dependencies, and tests that
uninitialized session creation and invalid initialization fail without changing
the output handle or creating resources. This is isolated component coverage;
it is not a full-engine build or a success-path runtime test.

## Integration interface

1. Latch graphics-mode selection before creating the XR instance. Enable
   `XR_KHR_vulkan_enable2` only for the future Vulkan image path. Do not call
   this module with an ordinary GL-only XR instance.
2. With the renderer GL context current, call
   `Bootstrap::initialize(instance, systemId, vulkan2Enabled)`.
3. Call `createSession(emptySessionHandle)`. The backend owns the returned
   session. `context()` supplies the exact bound Vulkan device/queue for Bridge.
4. Future integration must create/enumerate Vulkan swapchains and provide GL
   staging targets before selecting this session in the live backend.
5. Destroy/drain every Bridge, cached destination view/framebuffer, swapchain
   and returned XR session before `shutdown(true)`. This module submits no GPU
   work and performs no unbounded queue/device-idle wait. After session exposure,
   `shutdown(false)` retains resources; forgetting teardown leaks rather than
   destroying a device that may still have live children.

Initialization queries XR's API range and the Vulkan loader, requires at least
1.1, and checks the runtime-selected physical device supports the negotiated
version. It compares both GL device UUID and driver UUID against Vulkan before
creating the logical device. The Vulkan2 wrappers aggregate the runtime's own
requirements with application requirements, as required by the
[Khronos Vulkan2 specification](https://github.com/KhronosGroup/OpenXR-Docs/blob/main/specification/sources/chapters/extensions/khr/khr_vulkan_enable2.adoc).
Legacy Vulkan1 extension-list calls are not required by this path.

The application enables FD memory/semaphores, image-format-list, MSRTSS,
feedback-loop layout, sample-locations and synchronization2 for the Bridge's
captured RGBA8/FP16/D32 image definitions. These are requirements of this
specific staging profile, not a generic Vulkan renderer. MSRTSS, feedback-loop
and sync2 features are queried, required and enabled explicitly; DeviceContext
assertions are set only after successful device creation. Below API 1.2 the
bootstrap also enables MSRTSS's create-renderpass2/depth-stencil-resolve
extension dependencies explicitly. Bridge separately validates exact image
support, GL driver profile and sharing capabilities.

Same-byte staged sources, test binaries and Frame logs are retained in
[the bootstrap artifacts](../../../../../performance-results/spacewarp-vulkan-bootstrap-20261014/).
No XR success-path execution has been performed for this new module. The earlier
[rasterized D32 fixture](../raster_depth_probe/README.md) exercised its own
bootstrap, not this module.
