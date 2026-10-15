// CPU contract test. No SDL video initialization, GL context, XR instance or
// GPU device is created. Link the optional bridge implementation and API libs.
#include "vr_gl_vulkan_bridge.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

namespace {
void require(bool condition, const char* message)
{
    if(!condition) { std::fprintf(stderr, "FAIL %s\n", message); std::abort(); }
}

struct MockBridge
{
    int& order;
    int queue{1};
    bool queued{false}, completed{false}, submitOk{true}, waitOk{true}, acknowledgeOk{true};
    bool canPairWith(const MockBridge& other) const { return queue == other.queue && !queued && !other.queued; }
    bool enqueue(int expected)
    {
        require(++order == expected, "both submissions precede the terminal wait");
        if(!submitOk) return false;
        queued = true;
        return true;
    }
    bool waitForCompletion()
    {
        require(++order == 3 && queued, "wait only the submitted terminal bridge");
        completed = waitOk;
        return completed;
    }
    bool completeAfter(const MockBridge& terminal)
    {
        require(++order == 4 && queued && terminal.completed, "acknowledge earlier fence only after terminal completion");
        completed = acknowledgeOk;
        return completed;
    }
};

void pairedContract()
{
    using qvr::gl_vulkan::completePairedExports;
    for(int failure = 0; failure < 6; ++failure)
    {
        int order = 0;
        MockBridge motion{order}, depth{order};
        if(failure == 1) depth.queue = 2;
        if(failure == 2) motion.submitOk = false;
        if(failure == 3) depth.submitOk = false;
        if(failure == 4) depth.waitOk = false;
        if(failure == 5) motion.acknowledgeOk = false;
        const bool releasable = completePairedExports(motion, depth,
            [&] { return motion.enqueue(1); }, [&] { return depth.enqueue(2); });
        require(releasable == (failure == 0), "release both only after both proven complete");
        require(order == (failure == 0 || failure == 5 ? 4 : failure == 4 ? 3 : failure == 3 ? 2 : failure == 2 ? 1 : 0),
            "failed queue/submit/wait stops before unsafe later operations");
        if(failure == 3 || failure == 4)
            require(motion.queued && !motion.completed, "partial submission remains retained for teardown");
    }
}

struct BatchMockBridge
{
    int& order;
    int queue{1};
    bool queued{false}, completed{false}, submitOk{true}, waitOk{true}, acknowledgeOk{true};
    bool canPairWith(const BatchMockBridge& other) const { return queue == other.queue && !queued && !other.queued; }
    bool waitForCompletion() { ++order; completed = waitOk; return completed; }
    bool completeAfter(const BatchMockBridge& terminal)
    {
        require(queued && terminal.completed, "batch acknowledgement follows enqueue and terminal completion");
        ++order; completed = terminal.completed && acknowledgeOk; return completed;
    }
};

void batchContract()
{
    using qvr::gl_vulkan::completeQueuedExports;
    require(completeQueuedExports<BatchMockBridge>(nullptr, 0, [](auto&) { return false; }),
        "empty frame has no work to drain");
    require(!completeQueuedExports<BatchMockBridge>(nullptr, 1, [](auto&) { return true; }),
        "missing nonempty batch fails closed");
    for(int failure = 0; failure < 6; ++failure)
    {
        int order = 0;
        BatchMockBridge a{order}, b{order}, c{order};
        BatchMockBridge* bridges[]{&a, &b, &c};
        if(failure == 1) c.queue = 2;
        if(failure == 2) b.submitOk = false;
        if(failure == 3) c.waitOk = false;
        if(failure == 4) a.submitOk = false;
        if(failure == 5) a.acknowledgeOk = false;
        const bool complete = completeQueuedExports(bridges, 3, [&](BatchMockBridge& bridge)
        {
            ++order;
            if(!bridge.submitOk) return false;
            bridge.queued = true;
            return true;
        });
        require(complete == (failure == 0), "batch releases only when every queue submission and fence succeeds");
        require(order == (failure == 0 || failure == 5 ? 6 : failure == 1 ? 0 : failure == 2 ? 3 : failure == 4 ? 1 : 4),
            "batch waits one terminal fence then acknowledges earlier queued work");
        if(failure == 2)
            require(a.completed && !b.queued && !c.queued, "partial batch drains its queued prefix before rejecting");
        if(failure == 3)
            require(!a.completed && !b.completed && !c.completed, "failed terminal wait retains the entire pending batch");
        if(failure == 5)
            require(!a.completed && b.completed && c.completed, "one failed acknowledgement retains that bridge but still checks the remaining prefix");
    }
    for(const auto completion : {qvr::gl_vulkan::ExportCompletion::Wait,
            qvr::gl_vulkan::ExportCompletion::DeferUntilReuse})
    for(int failedEnqueue = -1; failedEnqueue < 6; ++failedEnqueue)
    {
        int order = 0, enqueued = 0;
        BatchMockBridge a{order}, b{order}, c{order}, d{order}, e{order}, f{order};
        BatchMockBridge* bridges[]{&a, &b, &c, &d, &e, &f};
        const bool complete = completeQueuedExports(bridges, 6, [&](BatchMockBridge& bridge)
        {
            if(enqueued == failedEnqueue) return false;
            ++enqueued;
            bridge.queued = true;
            return true;
        }, completion);
        require(complete == (failedEnqueue == -1), "full stereo batch succeeds only with all six exports");
        const bool deferred = failedEnqueue == -1 &&
            completion == qvr::gl_vulkan::ExportCompletion::DeferUntilReuse;
        require(order == (deferred ? 0 : enqueued),
            "only successful deferred batches skip waits; partial batches drain their queued prefix");
        for(int i = 0; i < 6; ++i)
            require(bridges[i]->queued == (i < enqueued) && bridges[i]->completed == (!deferred && i < enqueued),
                "every possible six-image partial prefix drains; untouched suffix stays unqueued");
    }
}
}
int main()
{
    using namespace qvr::gl_vulkan;
    pairedContract();
    batchContract();
    Bridge bridge;
    require(!bridge.healthy() && bridge.glTexture() == 0 && bridge.glInternalFormat() == 0,
        "uninitialized bridge must not expose a renderer target");
    require(!bridge.submitted(), "uninitialized bridge cannot authorize a pending XR release");
    require(!bridge.beginGlWrite(), "uninitialized GL handoff must fail");
    require(!bridge.cancelGlWrite(), "uninitialized cancellation must fail without touching GPU state");
    require(!bridge.copyTo(Destination{}), "uninitialized runtime copy must fail");
    require(!bridge.enqueueCopyTo(Destination{}) && !bridge.enqueueCancelGlWrite() && !bridge.waitForCompletion(),
        "uninitialized deferred submission/completion cannot touch GPU state");
    Bridge other;
    require(!bridge.canPairWith(other) && !bridge.completeAfter(other), "uninitialized bridges cannot prove paired completion");
    DeviceContext queueA{}, queueB{};
    require(!sameSubmissionQueue(queueA, queueB), "empty handles do not prove a shared queue");
    queueA.device = queueB.device = reinterpret_cast<VkDevice>(std::uintptr_t{1});
    queueA.queue = queueB.queue = reinterpret_cast<VkQueue>(std::uintptr_t{2});
    require(sameSubmissionQueue(queueA, queueB), "exact non-null device/queue identities match");
    queueB.device = reinterpret_cast<VkDevice>(std::uintptr_t{3});
    require(!sameSubmissionQueue(queueA, queueB), "matching queue handle on another device cannot prove ordering");
    queueB = queueA;
    queueB.queue = reinterpret_cast<VkQueue>(std::uintptr_t{4});
    require(!sameSubmissionQueue(queueA, queueB), "a different queue cannot provide the terminal fence proof");
    require(!bridge.initialize(DeviceContext{}, Format::MotionRGBA16F, 64, 64),
        "missing XR-bound Vulkan device must fail before calling GL/Vulkan");
    require(!bridge.healthy() && bridge.error()[0] != '\0',
        "partial initialization records an error and stays unavailable");
    require(bridge.glTexture() == 0 && !bridge.beginGlWrite() && !bridge.copyTo(Destination{}),
        "failed initialization cannot be rendered/copied");
    require(bridge.shutdown() && bridge.shutdown(), "empty/partial cleanup is idempotent");
    require(!bridge.initialize(DeviceContext{}, Format::DepthD32F, 0, 64),
        "invalid extent/context fails closed after cleanup");
    require(bridge.shutdown(), "cleanup remains valid after repeated rejected initialization");
    Destination target{};
    const std::uint64_t token = 1; // Pure predicate only: never sent to a native API.
    static_assert(sizeof(target.image) == sizeof(token));
    std::memcpy(&target.image, &token, sizeof(token));
    const VkImage image = target.image;
    XrSwapchainCreateInfo request{XR_TYPE_SWAPCHAIN_CREATE_INFO};
    request.format = VK_FORMAT_D32_SFLOAT; request.width = request.height = 64;
    request.sampleCount = request.faceCount = request.arraySize = request.mipCount = 1;
    request.usageFlags = XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    target = destinationFromSwapchainRequest(image, request, XR_SUCCESS);
    require(target.guaranteedUsage == VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        "D32 production request requires neither sampled nor transfer usage");
    require(!acceptsDestination(Format::DepthD32F, Operation::DepthAttachment, 64, 64, target),
        "successful creation alone does not imply successful acquire/wait");
    target.acquiredAndWaited = true;
    // No native creation flags are supplied or knowable in this contract.
    // An unknown/sample-compatible runtime definition cannot block unchanged
    // attachment rendering based on a successful normative XR request.
    require(acceptsDestination(Format::DepthD32F, Operation::DepthAttachment, 64, 64, target),
        "D32 attachment rendering accepts unknown native flags without capture metadata");
    require(!acceptsDestination(Format::DepthD32F, Operation::TransferCopy, 64, 64, target),
        "D32 transfer operation rejected");
    target.guaranteedUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    require(!acceptsDestination(Format::DepthD32F, Operation::TransferCopy, 64, 64, target),
        "D32 transfer rejected even with transfer guarantee");
    target.guaranteedUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    require(!acceptsDestination(Format::DepthD32F, Operation::DepthAttachment, 64, 64, target),
        "D32 requires depth attachment guarantee");
    target.guaranteedUsage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    target.samples = VK_SAMPLE_COUNT_4_BIT;
    require(!acceptsDestination(Format::DepthD32F, Operation::DepthAttachment, 64, 64, target), "multisampled destination rejected");
    target.samples = VK_SAMPLE_COUNT_1_BIT; target.layout = VK_IMAGE_LAYOUT_GENERAL;
    require(!acceptsDestination(Format::DepthD32F, Operation::DepthAttachment, 64, 64, target), "D32 requires unchanged attachment layout");
    require(!destinationFromSwapchainRequest(image, request, XR_ERROR_RUNTIME_FAILURE).image,
        "failed XR request creates no usage contract");
    require(!destinationFromSwapchainRequest(image, request, XR_TIMEOUT_EXPIRED).image,
        "qualified non-success is not swapchain creation success");
    request.createFlags = XR_SWAPCHAIN_CREATE_PROTECTED_CONTENT_BIT;
    require(!destinationFromSwapchainRequest(image, request, XR_SUCCESS).image,
        "unprotected bridge rejects protected runtime images");
    request.createFlags = 0;
    request.arraySize = 2;
    require(!destinationFromSwapchainRequest(image, request, XR_SUCCESS).image, "unsupported array shape rejected");
    request.arraySize = 1; request.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    request.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
    target = destinationFromSwapchainRequest(image, request, XR_SUCCESS); target.acquiredAndWaited = true;
    require(!acceptsDestination(Format::MotionRGBA16F, Operation::TransferCopy, 64, 64, target), "motion requires requested transfer usage");
    request.usageFlags |= XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
    target = destinationFromSwapchainRequest(image, request, XR_SUCCESS); target.acquiredAndWaited = true;
    require(acceptsDestination(Format::MotionRGBA16F, Operation::TransferCopy, 64, 64, target), "guaranteed single-sample motion copy accepted");
    require(!acceptsDestination(Format::MotionRGBA16F, Operation::DepthAttachment, 64, 64, target), "motion depth-attachment operation rejected");
    target.guaranteedUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    require(!acceptsDestination(Format::MotionRGBA16F, Operation::TransferCopy, 64, 64, target), "motion requires color attachment guarantee");
    target.guaranteedUsage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    require(!acceptsDestination(Format::MotionRGBA16F, Operation::TransferCopy, 32, 64, target), "extent mismatch rejected");
    target.format = VK_FORMAT_R8G8B8A8_SRGB;
    require(acceptsDestination(Format::SrgbColorBytes, Operation::TransferCopy, 64, 64, target), "compatible sRGB bytes copy accepted");
    target.format = VK_FORMAT_R8G8B8A8_UNORM;
    require(acceptsDestination(Format::SrgbColorBytes, Operation::TransferCopy, 64, 64, target), "compatible UNORM bytes copy accepted");
    target.format = VK_FORMAT_B8G8R8A8_SRGB;
    require(!acceptsDestination(Format::SrgbColorBytes, Operation::TransferCopy, 64, 64, target), "incompatible byte format rejected");
    request.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
        XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT |
        XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT |
        XR_SWAPCHAIN_USAGE_INPUT_ATTACHMENT_BIT_MND | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT;
    const auto mapped = destinationFromSwapchainRequest(image, request, XR_SUCCESS);
    require(mapped.guaranteedUsage == (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT), "normative usage mapping does not confuse mutable format with usage");
    std::puts("PASS bridge CPU contracts: normative XR usage, operation guards, unknown native flags; no XR/GPU resources");
}
