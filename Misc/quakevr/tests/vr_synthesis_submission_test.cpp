// Production composition-builder/SDK compatibility test, with no XR instance.
#include <openxr/openxr.h>
#include "vr_openxr_synthesis_compat.hpp"
#include "vr_synthesis_submission.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <type_traits>
#include <limits>

static_assert(std::is_standard_layout_v<XrFrameSynthesisInfoEXT>);
static_assert(sizeof(void*) != 8 || sizeof(XrFrameSynthesisInfoEXT) == 168);
static_assert(sizeof(void*) != 8 || sizeof(XrFrameSynthesisConfigViewEXT) == 24);
static_assert(sizeof(void*) != 8 || offsetof(XrFrameSynthesisInfoEXT, motionVectorScale) == 56);
static_assert(sizeof(void*) != 8 || offsetof(XrFrameSynthesisInfoEXT, depthSubImage) == 120);

static void require(bool ok, const char* what)
{
    if(!ok)
    {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

int main()
{
    const XrSwapchain motion = reinterpret_cast<XrSwapchain>(std::uintptr_t{1});
    const XrSwapchain depth = reinterpret_cast<XrSwapchain>(std::uintptr_t{2});
    const XrPosef pose{{0.f, 0.f, 0.f, 1.f}, {0.f, 0.f, 1.f}};
    require(qvr::xr::validSynthesisMetadata(156.f, 0.0381f, pose, {1.f, 1.f}, {0.f, 0.f}), "valid reversed metadata");
    require(!qvr::xr::validSynthesisMetadata(1.f, 1.f, pose, {1.f, 1.f}, {0.f, 0.f}), "equal depth endpoints rejected");
    require(!qvr::xr::validSynthesisMetadata(-1.f, 1.f, pose, {1.f, 1.f}, {0.f, 0.f}), "negative metric endpoint rejected");
    require(!qvr::xr::validSynthesisMetadata(1.f, 100.f, pose, {0.f, 1.f}, {0.f, 0.f}), "zero decoding scale rejected");
    XrPosef invalidPose = pose;
    invalidPose.position.x = std::numeric_limits<float>::quiet_NaN();
    require(!qvr::xr::validSynthesisMetadata(1.f, 100.f, invalidPose, {1.f, 1.f}, {0.f, 0.f}), "nonfinite app delta rejected");
    invalidPose = pose; invalidPose.orientation.w = 2.f;
    require(!qvr::xr::validSynthesisMetadata(1.f, 100.f, invalidPose, {1.f, 1.f}, {0.f, 0.f}), "nonunit quaternion rejected");

    qvr::synthesis::SubmissionContent content;
    require(!content.ready() && !content.acquire(0, 0, 0), "no content before a frame begins");
    content.begin();
    const auto firstFrame = content.frameId();
    require(content.acquire(0, 2, 0) && content.acquire(1, 1, 2), "eyes acquire independent motion and depth indices");
    require(content.eye(0).motionIndex == 2 && content.eye(0).depthIndex == 0 &&
        content.eye(1).motionIndex == 1 && content.eye(1).depthIndex == 2, "no shared color/motion/depth index assumption");
    require(content.complete(0, firstFrame, true, true, true) && !content.ready(), "one fresh eye cannot enable stereo synthesis");
    require(content.complete(1, firstFrame, true, true, true) && content.ready(), "both completed fresh exports enable synthesis");
    require(!content.acquire(0, 0, 1), "cannot replace a released eye in the same submitted frame");
    content.begin();
    require(!content.ready() && content.frameId() != firstFrame, "new/loading/held frames start without fresh synthesis");
    require(content.acquire(0, 0, 2), "new frame acquires normally");
    require(!content.complete(0, firstFrame, true, true, true), "stale producer token rejected");
    require(content.acquire(1, 2, 1), "right eye acquire remains independent");
    require(!content.complete(1, content.frameId(), true, true, false) && !content.ready(), "failed depth release rejects stereo synthesis");
    content.begin();
    require(!content.complete(0, content.frameId(), true, true, true), "release without acquire cannot publish freshness");
    require(content.acquire(1, 0, 0) && !content.complete(1, content.frameId(), false, true, true), "cancelled export cannot publish freshness");
    const auto info = qvr::xr::frameSynthesisInfo(motion, depth, 432, 432, 156.f, 0.0381f, pose, false);
    require(info.type == XR_TYPE_FRAME_SYNTHESIS_INFO_EXT && info.next == nullptr, "composition type and terminating chain");
    require(info.layerFlags == XR_FRAME_SYNTHESIS_INFO_USE_2D_MOTION_VECTOR_BIT_EXT, "signed RG producer declares 2D vectors");
    require(info.motionVectorSubImage.swapchain == motion && info.depthSubImage.swapchain == depth, "distinct owned vector/depth swapchains");
    require(info.motionVectorSubImage.imageRect.extent.width == 432 && info.depthSubImage.imageRect.extent.height == 432, "matching per-eye rects");
    require(info.motionVectorSubImage.imageRect.offset.x == 0 && info.depthSubImage.imageRect.offset.y == 0 &&
        info.motionVectorSubImage.imageArrayIndex == 0 && info.depthSubImage.imageArrayIndex == 0, "initialized rect origins and eye-local array indices");
    require(info.motionVectorScale.x == 1.f && info.motionVectorScale.y == 1.f &&
        info.motionVectorOffset.x == 0.f && info.motionVectorOffset.y == 0.f, "signed float producer has identity decoding");
    require(info.nearZ == 156.f && info.farZ == 0.0381f && info.minDepth == 0.f && info.maxDepth == 1.f, "metric reversed-Z depth metadata");
    require(info.appSpaceDeltaPose.orientation.w == 1.f && info.appSpaceDeltaPose.position.z == 1.f, "application-space delta preserved");
    const auto relaxed = qvr::xr::frameSynthesisInfo(motion, depth, 432, 432, 0.0381f, 156.f, pose, true);
    require((relaxed.layerFlags & XR_FRAME_SYNTHESIS_INFO_REQUEST_RELAXED_FRAME_INTERVAL_BIT_EXT) != 0 &&
        relaxed.nearZ < relaxed.farZ, "optional interval hint and conventional depth metadata");
    std::printf("PASS: composition builder, initialized metadata and published ABI (SDK %u.%u.%u)\n",
        XR_VERSION_MAJOR(XR_CURRENT_API_VERSION), XR_VERSION_MINOR(XR_CURRENT_API_VERSION), XR_VERSION_PATCH(XR_CURRENT_API_VERSION));
}
