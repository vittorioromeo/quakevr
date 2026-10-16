// Application-owned GL staging -> an already acquired XR Vulkan image.
// This optional Linux helper does not create XR instances/sessions or release
// runtime images. The caller owns the binding, device, queue and XR lifecycle.
#pragma once
#ifdef QVR_HAVE_GL_VULKAN_BRIDGE
#include <vulkan/vulkan.h>
#include <openxr/openxr.h>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace qvr::gl_vulkan
{
enum class Format { SrgbColorBytes, MotionRGBA16F, DepthD32F };
enum class Operation { TransferCopy, DepthAttachment };

struct DeviceContext
{
    VkInstance instance{VK_NULL_HANDLE};
    VkPhysicalDevice physicalDevice{VK_NULL_HANDLE};
    VkDevice device{VK_NULL_HANDLE};
    VkQueue queue{VK_NULL_HANDLE};
    std::uint32_t queueFamily{~0u};
    std::uint32_t queueIndex{0};
    std::uint32_t apiVersion{0}; // Enabled Vulkan instance/device contract >= 1.1.
    // These are logical-device enablement assertions, not physical support.
    // Bootstrap must enable FD memory/semaphores, image format lists,
    // MSRTSS and attachment feedback-loop extensions/features; D32 also
    // requires VK_EXT_sample_locations. They default false intentionally.
    bool externalImageFeaturesEnabled{false};
    bool sampleLocationsEnabled{false};
    bool synchronization2Enabled{false}; // Required for preserving shared D32.
};

struct Destination
{
    VkImage image{VK_NULL_HANDLE};
    VkFormat format{VK_FORMAT_UNDEFINED};
    std::uint32_t width{0}, height{0};
    VkImageLayout layout{VK_IMAGE_LAYOUT_UNDEFINED};
    // Normative Vulkan usage guarantees from the successful XR swapchain
    // request, not native VkImage creation metadata inferred from an API dump.
    VkImageUsageFlags guaranteedUsage{0};
    VkSampleCountFlagBits samples{static_cast<VkSampleCountFlagBits>(0)};
    // Only set after xrAcquireSwapchainImage and xrWaitSwapchainImage returned
    // XR_SUCCESS on the session bound to DeviceContext's exact queue/device.
    bool acquiredAndWaited{false};
};

// Pure contract factory. Call only with an enumerated image from that exact
// successful Vulkan XR swapchain request. Failed creation/unsupported shape
// returns an empty destination. Acquire/wait is a later, independent latch.
// Mutable-format creation flags are not usage bits and are not inferred here.
[[nodiscard]] Destination destinationFromSwapchainRequest(VkImage, const XrSwapchainCreateInfo&, XrResult creationResult);
// Operation-aware acceptance: production D32 only renders a depth attachment
// in the unchanged XR attachment layout; color/motion require transfer usage.
[[nodiscard]] bool acceptsDestination(Format, Operation, std::uint32_t width, std::uint32_t height, const Destination&);
// A later submission fence covers earlier work only on this exact device/queue.
[[nodiscard]] bool sameSubmissionQueue(const DeviceContext&, const DeviceContext&);

class Bridge final
{
public:
    Bridge();
    ~Bridge();
    Bridge(const Bridge&) = delete;
    Bridge& operator=(const Bridge&) = delete;

    // Current GL context must be the tested Zink/Turnip Frame profile; UUIDs
    // must match the supplied XR-bound Vulkan device. One dedicated staging
    // image is owned by this helper. No GPU content is read before init succeeds.
    [[nodiscard]] bool initialize(const DeviceContext&, Format, std::uint32_t width, std::uint32_t height);
    // Wait for the Vulkan -> GL semaphore before renderer writes. The returned
    // GL texture is staging, never a cast of the destination VkImage.
    [[nodiscard]] bool beginGlWrite();
    [[nodiscard]] unsigned glTexture() const;
    [[nodiscard]] unsigned glInternalFormat() const;
    // Detach renderer FBO attachments first. This signals GL, transfers external
    // ownership, copies with Y reversal, restores destination layout/ownership,
    // and waits a bounded fence. XR release/endFrame remain the caller's job.
    // SrgbColorBytes copies gamma-encoded RGBA8 bytes into compatible UNORM/SRGB
    // XR storage: GL_FRAMEBUFFER_SRGB must stay disabled during the producer's
    // writes; it may be restored after detachment, before deferred submission.
    [[nodiscard]] bool copyTo(const Destination&);
    // Complete the same semaphore/ownership cycle without reading pixels or
    // touching an XR destination after a rejected/partially acquired export.
    [[nodiscard]] bool cancelGlWrite();
    // Paired export: enqueue both bridges on their shared bound queue, wait the
    // terminal bridge, then acknowledge the earlier bridge's signaled fence.
    // The default synchronous caller releases only after completion. An
    // explicitly deferred Vulkan XR caller may release after submitted(), but
    // pending bridges cannot begin another GL write or reset/reuse commands.
    [[nodiscard]] bool enqueueCopyTo(const Destination&);
    [[nodiscard]] bool enqueueCancelGlWrite();
    [[nodiscard]] bool waitForCompletion();
    [[nodiscard]] bool completeAfter(const Bridge& terminal);
    [[nodiscard]] bool canPairWith(const Bridge&) const;
    [[nodiscard]] bool healthy() const;
    // The bound-queue submission succeeded, but its fence is still pending.
    // Vulkan OpenXR images may be released in this state. Staging/command
    // resources must still waitForCompletion before reuse or destruction.
    [[nodiscard]] bool submitted() const;
    [[nodiscard]] const char* error() const;
    // Both contexts/devices must remain alive and the owning GL context current.
    // Returns false and retains resources if queued work cannot safely drain.
    [[nodiscard]] bool shutdown();
private:
    struct State;
    std::unique_ptr<State> state;
};

// Shared ordering contract, also exercised with CPU mocks. Caller releases
// neither XR image unless this succeeds; failures retain any pending work for
// bounded teardown. Enqueue callbacks must only submit, not release XR images.
template<class BridgeType, class EnqueueFirst, class EnqueueTerminal>
[[nodiscard]] bool completePairedExports(BridgeType& first, BridgeType& terminal,
    EnqueueFirst enqueueFirst, EnqueueTerminal enqueueTerminal)
{
    return first.canPairWith(terminal) && enqueueFirst() && enqueueTerminal() &&
        terminal.waitForCompletion() && first.completeAfter(terminal);
}

// Batch any number of transfers known to use one ordered queue. A fence on the
// last successfully enqueued bridge proves completion for the whole prefix.
// On a partial enqueue failure, drain that prefix but still reject the frame.
// DeferUntilReuse only skips the fence wait after full submission success;
// the caller must wait before staging reuse and before swapchain destruction.
enum class ExportCompletion { Wait, DeferUntilReuse };

template<class BridgeType, class Enqueue>
[[nodiscard]] bool completeQueuedExports(BridgeType* const* bridges, std::size_t count, Enqueue enqueue,
    ExportCompletion completion = ExportCompletion::Wait)
{
    if(!bridges || !count) return count == 0;
    for(std::size_t i = 0; i < count; ++i)
    {
        if(!bridges[i] || (i && !bridges[0]->canPairWith(*bridges[i]))) return false;
    }
    std::size_t queued = 0;
    for(; queued < count; ++queued)
        if(!enqueue(*bridges[queued])) break;
    if(!queued) return false;
    // XR_KHR_vulkan_enable2 permits release after submission on the bound
    // queue while execution is pending. Never defer a failed partial batch:
    // drain its prefix and retain the existing fail-closed frame behavior.
    if(queued == count && completion == ExportCompletion::DeferUntilReuse) return true;
    BridgeType& terminal = *bridges[queued - 1];
    const bool terminalComplete = terminal.waitForCompletion();
    bool acknowledged = terminalComplete;
    if(terminalComplete)
        for(std::size_t i = 0; i + 1 < queued; ++i)
            if(!bridges[i]->completeAfter(terminal)) acknowledged = false;
    return queued == count && terminalComplete && acknowledged;
}
} // namespace qvr::gl_vulkan
#endif
