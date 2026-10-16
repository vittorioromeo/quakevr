#include "vr_vulkan_bsp.hpp"

#ifdef QVR_HAVE_VULKAN

#include <cstring>
#include <limits>

namespace qvr::bsp::vulkan
{
namespace
{
bool memoryType(VkPhysicalDevice physicalDevice, uint32_t typeBits,
    VkMemoryPropertyFlags required, VkMemoryPropertyFlags preferred, uint32_t& typeIndex)
{
    VkPhysicalDeviceMemoryProperties properties{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &properties);
    int bestScore = -1;
    for(uint32_t i = 0; i < properties.memoryTypeCount; ++i)
    {
        if(!(typeBits & (1u << i))) continue;
        const VkMemoryPropertyFlags flags = properties.memoryTypes[i].propertyFlags;
        if((flags & required) != required) continue;
        int score = 0;
        for(VkMemoryPropertyFlags bit = 1; bit; bit <<= 1)
            if((preferred & bit) && (flags & bit)) ++score;
        if(score > bestScore)
        {
            bestScore = score;
            typeIndex = i;
        }
    }
    return bestScore >= 0;
}

void clear(GpuMesh& mesh)
{
    mesh.ranges.clear();
    mesh.vertexBytes = 0;
    mesh.buffer = VK_NULL_HANDLE;
    mesh.memory = VK_NULL_HANDLE;
    mesh.device = VK_NULL_HANDLE;
}

bool validPacket(const OpaqueWorldUpload& source)
{
    if(source.vertices.empty() || source.indices.empty() || source.ranges.empty() ||
        source.vertices.size() > std::numeric_limits<uint32_t>::max() ||
        source.indices.size() > std::numeric_limits<uint32_t>::max())
        return false;
    for(za::U32 index : source.indices)
        if(index >= source.vertices.size()) return false;
    for(const DrawRange& range : source.ranges)
        if(range.firstIndex > source.indices.size() || range.indexCount > source.indices.size() - range.firstIndex ||
            range.indexCount == 0 || range.indexCount % 3 != 0)
            return false;
    return true;
}
} // namespace

bool upload(const OpaqueWorldUpload& source, VkPhysicalDevice physicalDevice, VkDevice device,
    VkQueue queue, VkCommandPool commandPool, GpuMesh& out, VkResult* result)
{
    if(result) *result = VK_ERROR_INITIALIZATION_FAILED;
    if(!validPacket(source) || !physicalDevice || !device || !queue || !commandPool ||
        out.device || out.buffer || out.memory || !out.ranges.empty())
        return false;

    const VkDeviceSize vertexBytes = source.vertices.size() * sizeof(UploadVertex);
    const VkDeviceSize indexBytes = source.indices.size() * sizeof(za::U32);
    if(vertexBytes == 0 || indexBytes == 0 || vertexBytes > std::numeric_limits<VkDeviceSize>::max() - indexBytes)
        return false;
    const VkDeviceSize totalBytes = vertexBytes + indexBytes;

    GpuMesh candidate;
    candidate.device = device;
    candidate.vertexBytes = vertexBytes;
    candidate.ranges = source.ranges;
    VkBuffer staging = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    VkDeviceSize stagingAllocationSize = 0;
    VkCommandBuffer command = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    VkResult status = VK_SUCCESS;

    const auto fail = [&]()
    {
        if(fence) vkDestroyFence(device, fence, nullptr);
        if(command) vkFreeCommandBuffers(device, commandPool, 1, &command);
        if(staging) vkDestroyBuffer(device, staging, nullptr);
        if(stagingMemory) vkFreeMemory(device, stagingMemory, nullptr);
        if(candidate.buffer) vkDestroyBuffer(device, candidate.buffer, nullptr);
        if(candidate.memory) vkFreeMemory(device, candidate.memory, nullptr);
        if(result) *result = status;
        return false;
    };

    VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = totalBytes;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    status = vkCreateBuffer(device, &bufferInfo, nullptr, &candidate.buffer);
    if(status != VK_SUCCESS) return fail();

    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(device, candidate.buffer, &requirements);
    uint32_t deviceType = 0;
    if(!memoryType(physicalDevice, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, deviceType))
        return fail();
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = deviceType;
    status = vkAllocateMemory(device, &allocation, nullptr, &candidate.memory);
    if(status != VK_SUCCESS) return fail();
    status = vkBindBufferMemory(device, candidate.buffer, candidate.memory, 0);
    if(status != VK_SUCCESS) return fail();

    VkBufferCreateInfo stagingInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    stagingInfo.size = totalBytes;
    stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    status = vkCreateBuffer(device, &stagingInfo, nullptr, &staging);
    if(status != VK_SUCCESS) return fail();
    vkGetBufferMemoryRequirements(device, staging, &requirements);
    uint32_t hostType = 0;
    if(!memoryType(physicalDevice, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, hostType))
        return fail();
    stagingAllocationSize = requirements.size;
    allocation.allocationSize = stagingAllocationSize;
    allocation.memoryTypeIndex = hostType;
    status = vkAllocateMemory(device, &allocation, nullptr, &stagingMemory);
    if(status != VK_SUCCESS) return fail();
    status = vkBindBufferMemory(device, staging, stagingMemory, 0);
    if(status != VK_SUCCESS) return fail();

    void* mapped = nullptr;
    status = vkMapMemory(device, stagingMemory, 0, stagingAllocationSize, 0, &mapped);
    if(status != VK_SUCCESS) return fail();
    std::memcpy(mapped, source.vertices.data(), static_cast<size_t>(vertexBytes));
    std::memcpy(static_cast<unsigned char*>(mapped) + vertexBytes, source.indices.data(), static_cast<size_t>(indexBytes));
    VkPhysicalDeviceMemoryProperties properties{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &properties);
    if(!(properties.memoryTypes[hostType].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
    {
        VkMappedMemoryRange flush{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
        flush.memory = stagingMemory;
        flush.offset = 0;
        flush.size = VK_WHOLE_SIZE;
        status = vkFlushMappedMemoryRanges(device, 1, &flush);
        if(status != VK_SUCCESS)
        {
            vkUnmapMemory(device, stagingMemory);
            return fail();
        }
    }
    vkUnmapMemory(device, stagingMemory);

    VkCommandBufferAllocateInfo commandInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    commandInfo.commandPool = commandPool;
    commandInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandInfo.commandBufferCount = 1;
    status = vkAllocateCommandBuffers(device, &commandInfo, &command);
    if(status != VK_SUCCESS) return fail();
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    status = vkBeginCommandBuffer(command, &begin);
    if(status != VK_SUCCESS) return fail();
    VkBufferCopy vertexCopy{0, 0, vertexBytes};
    VkBufferCopy indexCopy{vertexBytes, vertexBytes, indexBytes};
    vkCmdCopyBuffer(command, staging, candidate.buffer, 1, &vertexCopy);
    vkCmdCopyBuffer(command, staging, candidate.buffer, 1, &indexCopy);
    VkBufferMemoryBarrier ready{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    ready.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    ready.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_INDEX_READ_BIT;
    ready.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    ready.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    ready.buffer = candidate.buffer;
    ready.offset = 0;
    ready.size = totalBytes;
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0,
        0, nullptr, 1, &ready, 0, nullptr);
    status = vkEndCommandBuffer(command);
    if(status != VK_SUCCESS) return fail();
    VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    status = vkCreateFence(device, &fenceInfo, nullptr, &fence);
    if(status != VK_SUCCESS) return fail();
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &command;
    status = vkQueueSubmit(queue, 1, &submit, fence);
    if(status != VK_SUCCESS) return fail();
    status = vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
    if(status != VK_SUCCESS) return fail();

    vkDestroyFence(device, fence, nullptr);
    vkFreeCommandBuffers(device, commandPool, 1, &command);
    vkDestroyBuffer(device, staging, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);
    if(result) *result = VK_SUCCESS;
    out = static_cast<GpuMesh&&>(candidate);
    return true;
}

void destroy(GpuMesh& mesh)
{
    if(mesh.device && mesh.buffer) vkDestroyBuffer(mesh.device, mesh.buffer, nullptr);
    if(mesh.device && mesh.memory) vkFreeMemory(mesh.device, mesh.memory, nullptr);
    mesh.ranges.clear();
    clear(mesh);
}

void recordOpaqueWorld(VkCommandBuffer command, const GpuMesh& mesh,
    BindMaterial bindMaterial, void* userData)
{
    if(!command || !mesh.buffer || mesh.ranges.empty()) return;
    const VkDeviceSize vertexOffset = 0;
    vkCmdBindVertexBuffers(command, 0, 1, &mesh.buffer, &vertexOffset);
    vkCmdBindIndexBuffer(command, mesh.buffer, mesh.vertexBytes, VK_INDEX_TYPE_UINT32);
    for(const DrawRange& range : mesh.ranges)
    {
        if(bindMaterial) bindMaterial(command, range.material, userData);
        vkCmdDrawIndexed(command, range.indexCount, 1, range.firstIndex, 0, 0);
    }
}

} // namespace qvr::bsp::vulkan

#endif // QVR_HAVE_VULKAN
