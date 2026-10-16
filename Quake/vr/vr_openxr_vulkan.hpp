// Linux Vulkan2 bootstrap for the opt-in GL/Vulkan OpenXR color bridge.
// Frame synthesis remains disabled until the live motion/depth path is ready.
#pragma once
#if defined(QVR_HAVE_GL_VULKAN_BRIDGE) && defined(__linux__)
#include "vr_gl_vulkan_bridge.hpp"
#include <openxr/openxr.h>

namespace qvr::openxr_vulkan
{
class Bootstrap final
{
public:
    Bootstrap() = default;
    ~Bootstrap();
    Bootstrap(const Bootstrap&) = delete;
    Bootstrap& operator=(const Bootstrap&) = delete;

    // Borrowed XR instance must have enabled XR_KHR_vulkan_enable2. This
    // creates no swapchains, staging images or GPU work, and does not begin
    // the session. Requires the renderer GL context current and verifies both
    // physical-device and driver UUIDs before any sharing can be attempted.
    [[nodiscard]] bool initialize(XrInstance, XrSystemId, bool vulkan2Enabled);
    // Caller owns the returned XR session and must destroy it before shutdown.
    [[nodiscard]] bool createSession(XrSession& emptyOutput);
    // Do not expose a borrowable device until session creation latches the
    // explicit drained-shutdown requirement for every possible bridge child.
    [[nodiscard]] const gl_vulkan::DeviceContext& context() const
    {
        static const gl_vulkan::DeviceContext empty{};
        return sessionExposed ? device : empty;
    }
    [[nodiscard]] const char* error() const { return failure; }
    // Call only after successful xrDestroySession and destruction/draining of
    // bridge children. A terminal fence orders prior work on the bound queue;
    // timeout retains that fence/device for a later retry, without queue-idle.
    [[nodiscard]] bool drainAfterSessionDestroy(bool sessionDestroyed);
    // After exposing a session, caller must positively assert all borrowed
    // sessions/bridges/swapchains are destroyed and queue work has drained.
    // No unbounded vkDeviceWaitIdle is performed here. False retains resources.
    [[nodiscard]] bool shutdown(bool childrenDestroyedAndQueueDrained = false);

private:
    [[nodiscard]] bool fail(const char* operation);
    gl_vulkan::DeviceContext device{};
    XrInstance xrInstance{XR_NULL_HANDLE};
    XrSystemId system{XR_NULL_SYSTEM_ID};
    bool sessionExposed{false};
    VkFence terminalFence{VK_NULL_HANDLE};
    bool terminalSubmitted{false}, terminalQueueDrained{false};
    char failure[192]{};
};
} // namespace qvr::openxr_vulkan
#endif
