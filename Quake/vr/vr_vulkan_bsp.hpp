#pragma once

#ifdef QVR_HAVE_VULKAN

#include "vr_bsp_mesh.hpp"

#include <vulkan/vulkan.h>

namespace qvr::bsp::vulkan
{

struct GpuMesh
{
    VkDevice device{VK_NULL_HANDLE};
    VkBuffer buffer{VK_NULL_HANDLE}; // vertices at offset 0, indices at vertexBytes
    VkDeviceMemory memory{VK_NULL_HANDLE};
    VkDeviceSize vertexBytes{0};
    za::Vector<DrawRange> ranges;
};

// Uploads an owned CPU packet to device-local memory. The caller supplies the
// OpenXR-session device, queue and command pool, and must serialize this queue
// submission with other XR queue access. This is intended for map changes, not
// per-frame uploads.
[[nodiscard]] bool upload(const OpaqueWorldUpload& source, VkPhysicalDevice physicalDevice,
    VkDevice device, VkQueue queue, VkCommandPool commandPool, GpuMesh& out, VkResult* result = nullptr);

void destroy(GpuMesh& mesh);

using BindMaterial = void (*)(VkCommandBuffer command, za::U32 material, void* userData);

// Caller binds a compatible graphics pipeline before this call. Its vertex
// binding must use UploadVertex's position[3], uv[2] layout and UINT32 indices.
void recordOpaqueWorld(VkCommandBuffer command, const GpuMesh& mesh,
    BindMaterial bindMaterial = nullptr, void* userData = nullptr);

} // namespace qvr::bsp::vulkan

#endif // QVR_HAVE_VULKAN
