// The published XR_EXT_frame_synthesis revision 1 ABI for SDKs predating it.
// Native Steam Frame currently has OpenXR 1.1.40; Windows headers already have it.
#pragma once
#include <cmath>

#ifndef XR_EXT_frame_synthesis
#define XR_EXT_frame_synthesis 1
#define XR_EXT_FRAME_SYNTHESIS_EXTENSION_NAME "XR_EXT_frame_synthesis"
inline constexpr XrStructureType XR_TYPE_FRAME_SYNTHESIS_INFO_EXT = static_cast<XrStructureType>(1000211000);
inline constexpr XrStructureType XR_TYPE_FRAME_SYNTHESIS_CONFIG_VIEW_EXT = static_cast<XrStructureType>(1000211001);
using XrFrameSynthesisInfoFlagsEXT = XrFlags64;
inline constexpr XrFrameSynthesisInfoFlagsEXT XR_FRAME_SYNTHESIS_INFO_USE_2D_MOTION_VECTOR_BIT_EXT = 1;
inline constexpr XrFrameSynthesisInfoFlagsEXT XR_FRAME_SYNTHESIS_INFO_REQUEST_RELAXED_FRAME_INTERVAL_BIT_EXT = 2;
struct XrFrameSynthesisInfoEXT
{
    XrStructureType type;
    const void* next;
    XrFrameSynthesisInfoFlagsEXT layerFlags;
    XrSwapchainSubImage motionVectorSubImage;
    XrVector4f motionVectorScale;
    XrVector4f motionVectorOffset;
    XrPosef appSpaceDeltaPose;
    XrSwapchainSubImage depthSubImage;
    float minDepth;
    float maxDepth;
    float nearZ;
    float farZ;
};
struct XrFrameSynthesisConfigViewEXT
{
    XrStructureType type;
    void* next;
    uint32_t recommendedMotionVectorImageRectWidth;
    uint32_t recommendedMotionVectorImageRectHeight;
};
#endif

namespace qvr::xr
{
// Quake producer contract, narrower than EXT: finite clip distances and positive
// affine scales. EXT itself also permits infinite near/far and other encodings.
[[nodiscard]] inline bool validSynthesisMetadata(float nearZ, float farZ, const XrPosef& pose,
    const XrVector2f& scale, const XrVector2f& offset)
{
    const float norm = pose.orientation.x * pose.orientation.x + pose.orientation.y * pose.orientation.y +
        pose.orientation.z * pose.orientation.z + pose.orientation.w * pose.orientation.w;
    return std::isfinite(nearZ) && std::isfinite(farZ) && nearZ > 0.f && farZ > 0.f && nearZ != farZ &&
        std::isfinite(scale.x) && std::isfinite(scale.y) && scale.x > 0.f && scale.y > 0.f &&
        std::isfinite(offset.x) && std::isfinite(offset.y) &&
        std::isfinite(pose.position.x) && std::isfinite(pose.position.y) && std::isfinite(pose.position.z) &&
        std::isfinite(norm) && std::abs(norm - 1.f) < 0.001f;
}

[[nodiscard]] inline XrFrameSynthesisInfoEXT frameSynthesisInfo(XrSwapchain motion, XrSwapchain depth,
    int width, int height, float nearZ, float farZ, const XrPosef& applicationDelta, bool relaxed)
{
    XrFrameSynthesisInfoEXT info{};
    info.type = XR_TYPE_FRAME_SYNTHESIS_INFO_EXT;
    info.layerFlags = XR_FRAME_SYNTHESIS_INFO_USE_2D_MOTION_VECTOR_BIT_EXT;
    if(relaxed) info.layerFlags |= XR_FRAME_SYNTHESIS_INFO_REQUEST_RELAXED_FRAME_INTERVAL_BIT_EXT;
    info.motionVectorSubImage.swapchain = motion;
    info.motionVectorSubImage.imageRect.extent = {width, height};
    info.depthSubImage.swapchain = depth;
    info.depthSubImage.imageRect.extent = {width, height};
    info.motionVectorScale = {1.f, 1.f, 1.f, 1.f};
    info.appSpaceDeltaPose = applicationDelta;
    info.minDepth = 0.f;
    info.maxDepth = 1.f;
    info.nearZ = nearZ;
    info.farZ = farZ;
    return info;
}
} // namespace qvr::xr
