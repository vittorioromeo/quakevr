#define GL_GLEXT_PROTOTYPES
#define XR_USE_GRAPHICS_API_VULKAN
#include <SDL.h>
#include <SDL_opengl.h>
#include <openxr/openxr.h>
#include <vulkan/vulkan.h>
#include <openxr/openxr_platform.h>
#include "../../../Quake/vr/vr_openxr_synthesis_compat.hpp"
#include "../../../Quake/vr/vr_vulkan_bsp.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#ifndef _WIN32
#include <unistd.h>
#endif

// Deterministic test pattern: attachment clears write a moving rectangle into
// color, matching 2D motion, and depth targets without a shader dependency.
struct ImageSet
{
    XrSwapchain swapchain{XR_NULL_HANDLE};
    VkFormat format{VK_FORMAT_UNDEFINED};
    VkImageAspectFlags aspect{0};
    uint32_t width{0}, height{0};
    std::vector<XrSwapchainImageVulkanKHR> images;
    std::vector<VkImageView> views;
    std::vector<VkFramebuffer> framebuffers;
    VkRenderPass pass{VK_NULL_HANDLE};
    uint32_t acquired{0};
    bool hasAcquired{false};
    bool waited{false};
};

bool makeImageSet(XrSession session, VkDevice device, ImageSet& set, VkFormat format,
    XrSwapchainUsageFlags usage, VkImageAspectFlags aspect, uint32_t width, uint32_t height)
{
    set.format = format;
    set.aspect = aspect;
    set.width = width;
    set.height = height;
    XrSwapchainCreateInfo info{XR_TYPE_SWAPCHAIN_CREATE_INFO};
    info.usageFlags = usage;
    info.format = format;
    info.sampleCount = 1;
    info.width = width;
    info.height = height;
    info.faceCount = 1;
    info.arraySize = 1;
    info.mipCount = 1;
    if(XR_FAILED(xrCreateSwapchain(session, &info, &set.swapchain))) return false;
    uint32_t count = 0;
    if(XR_FAILED(xrEnumerateSwapchainImages(set.swapchain, 0, &count, nullptr)) || count == 0) return false;
    set.images.resize(count);
    for(auto& image : set.images) image.type = XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR;
    if(XR_FAILED(xrEnumerateSwapchainImages(set.swapchain, count, &count,
        reinterpret_cast<XrSwapchainImageBaseHeader*>(set.images.data())))) return false;

    const bool depth = aspect == VK_IMAGE_ASPECT_DEPTH_BIT;
    VkAttachmentDescription attachment{};
    attachment.format = format;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    // A clear discards prior contents, including on the first acquire when the
    // runtime's initial image layout is not known to the application.
    attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment.finalLayout = depth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference reference{0, attachment.finalLayout};
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    if(depth) subpass.pDepthStencilAttachment = &reference;
    else { subpass.colorAttachmentCount = 1; subpass.pColorAttachments = &reference; }
    VkRenderPassCreateInfo passInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    passInfo.attachmentCount = 1;
    passInfo.pAttachments = &attachment;
    passInfo.subpassCount = 1;
    passInfo.pSubpasses = &subpass;
    if(vkCreateRenderPass(device, &passInfo, nullptr, &set.pass) != VK_SUCCESS) return false;
    for(const auto& image : set.images)
    {
        VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.image = image.image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange = {aspect, 0, 1, 0, 1};
        VkImageView view = VK_NULL_HANDLE;
        if(vkCreateImageView(device, &viewInfo, nullptr, &view) != VK_SUCCESS) return false;
        set.views.push_back(view);
        VkFramebufferCreateInfo framebufferInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        framebufferInfo.renderPass = set.pass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = &set.views.back();
        framebufferInfo.width = width;
        framebufferInfo.height = height;
        framebufferInfo.layers = 1;
        VkFramebuffer framebuffer = VK_NULL_HANDLE;
        if(vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffer) != VK_SUCCESS) return false;
        set.framebuffers.push_back(framebuffer);
    }
    return true;
}

void destroyImageSet(VkDevice device, ImageSet& set)
{
    for(VkFramebuffer framebuffer : set.framebuffers) vkDestroyFramebuffer(device, framebuffer, nullptr);
    for(VkImageView view : set.views) vkDestroyImageView(device, view, nullptr);
    if(set.pass) vkDestroyRenderPass(device, set.pass, nullptr);
    if(set.swapchain) xrDestroySwapchain(set.swapchain);
    set = {};
}

bool acquire(ImageSet& set)
{
    XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
    if(XR_FAILED(xrAcquireSwapchainImage(set.swapchain, &acquireInfo, &set.acquired))) return false;
    set.hasAcquired = true;
    XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
    waitInfo.timeout = XR_INFINITE_DURATION;
    set.waited = xrWaitSwapchainImage(set.swapchain, &waitInfo) == XR_SUCCESS;
    return set.waited;
}

bool release(ImageSet& set)
{
    if(!set.hasAcquired) return true;
    if(!set.waited) return false;
    XrSwapchainImageReleaseInfo info{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    const bool ok = XR_SUCCEEDED(xrReleaseSwapchainImage(set.swapchain, &info));
    if(ok) { set.hasAcquired = false; set.waited = false; }
    return ok;
}

struct PatternRect
{
    float centerX{0.5f};
    float centerY{0.5f};
    int32_t x{0}, y{0}, width{0}, height{0};
};

PatternRect makePatternRect(const ImageSet& set, float centerX, float centerY)
{
    PatternRect rect{};
    rect.centerX = centerX;
    rect.centerY = centerY;
    rect.width = std::min(static_cast<int32_t>(set.width), std::max(8, static_cast<int32_t>(set.width / 10)));
    rect.height = std::min(static_cast<int32_t>(set.height), std::max(8, static_cast<int32_t>(set.height / 10)));
    rect.x = std::clamp(static_cast<int32_t>(centerX * set.width) - rect.width / 2,
        0, static_cast<int32_t>(set.width) - rect.width);
    rect.y = std::clamp(static_cast<int32_t>(centerY * set.height) - rect.height / 2,
        0, static_cast<int32_t>(set.height) - rect.height);
    return rect;
}

struct ScenePipelines
{
    VkDevice device{VK_NULL_HANDLE};
    VkPipelineLayout layout{VK_NULL_HANDLE};
    VkShaderModule vertex{VK_NULL_HANDLE};
    VkShaderModule fragment{VK_NULL_HANDLE};
    VkPipeline color{VK_NULL_HANDLE};
    VkPipeline motion{VK_NULL_HANDLE};
    VkPipeline depth{VK_NULL_HANDLE};
};

bool readShader(const char* filename, std::vector<uint32_t>& code)
{
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if(!file) return false;
    const std::streamsize size = file.tellg();
    if(size <= 0 || size % 4 != 0) return false;
    code.resize(static_cast<size_t>(size) / sizeof(uint32_t));
    file.seekg(0);
    return static_cast<bool>(file.read(reinterpret_cast<char*>(code.data()), size));
}

VkShaderModule makeShader(VkDevice device, const std::vector<uint32_t>& code)
{
    VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    info.codeSize = code.size() * sizeof(uint32_t);
    info.pCode = code.data();
    VkShaderModule shader = VK_NULL_HANDLE;
    return vkCreateShaderModule(device, &info, nullptr, &shader) == VK_SUCCESS ? shader : VK_NULL_HANDLE;
}

VkPipeline makeScenePipeline(const ScenePipelines& shaders, VkRenderPass pass, bool depthOnly)
{
    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = shaders.vertex;
    stages[0].pName = "main";
    stages[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = shaders.fragment;
    stages[1].pName = "main";

    VkVertexInputBindingDescription binding{0, sizeof(qvr::bsp::UploadVertex), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription attributes[] = {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(qvr::bsp::UploadVertex, position)},
        {1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(qvr::bsp::UploadVertex, uv)}};
    VkPipelineVertexInputStateCreateInfo vertexInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = 2;
    vertexInput.pVertexAttributeDescriptions = attributes;
    VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = VK_CULL_MODE_NONE;
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.lineWidth = 1.f;
    VkPipelineMultisampleStateCreateInfo multisample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineDepthStencilStateCreateInfo depthState{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depthState.depthTestEnable = depthOnly;
    depthState.depthWriteEnable = depthOnly;
    depthState.depthCompareOp = VK_COMPARE_OP_LESS;
    VkPipelineColorBlendAttachmentState colorAttachment{};
    colorAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    blend.attachmentCount = depthOnly ? 0u : 1u;
    blend.pAttachments = depthOnly ? nullptr : &colorAttachment;
    const VkDynamicState dynamicValues[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamic.dynamicStateCount = 2;
    dynamic.pDynamicStates = dynamicValues;

    VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    info.stageCount = depthOnly ? 1u : 2u;
    info.pStages = stages;
    info.pVertexInputState = &vertexInput;
    info.pInputAssemblyState = &assembly;
    info.pViewportState = &viewport;
    info.pRasterizationState = &raster;
    info.pMultisampleState = &multisample;
    info.pDepthStencilState = depthOnly ? &depthState : nullptr;
    info.pColorBlendState = &blend;
    info.pDynamicState = &dynamic;
    info.layout = shaders.layout;
    info.renderPass = pass;
    info.subpass = 0;
    VkPipeline pipeline = VK_NULL_HANDLE;
    return vkCreateGraphicsPipelines(shaders.device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline) == VK_SUCCESS
        ? pipeline : VK_NULL_HANDLE;
}

bool makeScenePipelines(VkDevice device, VkRenderPass colorPass, VkRenderPass motionPass,
    VkRenderPass depthPass, ScenePipelines& out)
{
#ifndef QVR_VULKAN_TEST_SHADER_DIR
#error QVR_VULKAN_TEST_SHADER_DIR must name the CMake-generated SPIR-V directory
#endif
    std::vector<uint32_t> vertexCode, fragmentCode;
    const std::string vertexPath = std::string(QVR_VULKAN_TEST_SHADER_DIR) + "/bsp_test.vert.spv";
    const std::string fragmentPath = std::string(QVR_VULKAN_TEST_SHADER_DIR) + "/bsp_test.frag.spv";
    if(!readShader(vertexPath.c_str(), vertexCode) || !readShader(fragmentPath.c_str(), fragmentCode)) return false;
    if(out.device || out.vertex || out.fragment || out.layout || out.color || out.motion || out.depth) return false;
    out.device = device;
    out.vertex = makeShader(device, vertexCode);
    out.fragment = makeShader(device, fragmentCode);
    if(!out.vertex || !out.fragment) return false;
    VkPushConstantRange push{};
    push.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    push.offset = 0;
    push.size = 32;
    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &push;
    if(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &out.layout) != VK_SUCCESS) return false;
    out.color = makeScenePipeline(out, colorPass, false);
    out.motion = makeScenePipeline(out, motionPass, false);
    out.depth = makeScenePipeline(out, depthPass, true);
    return out.color && out.motion && out.depth;
}

void destroyScenePipelines(ScenePipelines& pipelines)
{
    if(pipelines.device && pipelines.color) vkDestroyPipeline(pipelines.device, pipelines.color, nullptr);
    if(pipelines.device && pipelines.motion) vkDestroyPipeline(pipelines.device, pipelines.motion, nullptr);
    if(pipelines.device && pipelines.depth) vkDestroyPipeline(pipelines.device, pipelines.depth, nullptr);
    if(pipelines.device && pipelines.layout) vkDestroyPipelineLayout(pipelines.device, pipelines.layout, nullptr);
    if(pipelines.device && pipelines.vertex) vkDestroyShaderModule(pipelines.device, pipelines.vertex, nullptr);
    if(pipelines.device && pipelines.fragment) vkDestroyShaderModule(pipelines.device, pipelines.fragment, nullptr);
    pipelines = {};
}

struct DrawParams
{
    float transform[4];
    float payload[4];
};

void recordScene(VkCommandBuffer command, const ImageSet& set, VkPipeline pipeline,
    VkPipelineLayout layout, const qvr::bsp::vulkan::GpuMesh& mesh,
    VkClearValue background, const PatternRect& rect, const VkClearValue& payload)
{
    VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    begin.renderPass = set.pass;
    begin.framebuffer = set.framebuffers.at(set.acquired);
    begin.renderArea.extent = {set.width, set.height};
    begin.clearValueCount = 1;
    begin.pClearValues = &background;
    vkCmdBeginRenderPass(command, &begin, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    VkViewport view{0.f, 0.f, static_cast<float>(set.width), static_cast<float>(set.height), 0.f, 1.f};
    VkRect2D scissor{{0, 0}, {set.width, set.height}};
    vkCmdSetViewport(command, 0, 1, &view);
    vkCmdSetScissor(command, 0, 1, &scissor);
    const DrawParams params{{
        2.f * (static_cast<float>(rect.x) + rect.width * 0.5f) / set.width - 1.f,
        2.f * (static_cast<float>(rect.y) + rect.height * 0.5f) / set.height - 1.f,
        static_cast<float>(rect.width) / set.width,
        static_cast<float>(rect.height) / set.height},
        {payload.color.float32[0], payload.color.float32[1], payload.color.float32[2], payload.color.float32[3]}};
    vkCmdPushConstants(command, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0, sizeof(params), &params);
    qvr::bsp::vulkan::recordOpaqueWorld(command, mesh);
    vkCmdEndRenderPass(command);
}

struct EyePattern
{
    PatternRect color, motion, depth;
    XrVector2f motionNdc{};
};

struct Readback
{
    VkBuffer buffer{VK_NULL_HANDLE};
    VkDeviceMemory memory{VK_NULL_HANDLE};
    void* mapped{nullptr};
};

bool makeReadback(VkPhysicalDevice physicalDevice, VkDevice device, Readback& readback)
{
    VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = 80;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if(vkCreateBuffer(device, &bufferInfo, nullptr, &readback.buffer) != VK_SUCCESS) return false;
    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(device, readback.buffer, &requirements);
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
    uint32_t memoryType = ~0u;
    for(uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
    {
        const VkMemoryPropertyFlags flags = memoryProperties.memoryTypes[i].propertyFlags;
        if((requirements.memoryTypeBits & (1u << i)) &&
            (flags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
                (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
        {
            memoryType = i;
            break;
        }
    }
    if(memoryType == ~0u) return false;
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = memoryType;
    if(vkAllocateMemory(device, &allocation, nullptr, &readback.memory) != VK_SUCCESS) return false;
    if(vkBindBufferMemory(device, readback.buffer, readback.memory, 0) != VK_SUCCESS) return false;
    return true;
}

void recordCopyPixel(VkCommandBuffer command, const ImageSet& set, VkBuffer buffer,
    VkDeviceSize bufferOffset, int32_t x, int32_t y)
{
    const bool depth = set.aspect == VK_IMAGE_ASPECT_DEPTH_BIT;
    VkImageMemoryBarrier toCopy{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    toCopy.srcAccessMask = depth ? VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    toCopy.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    toCopy.oldLayout = depth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    toCopy.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    toCopy.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toCopy.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toCopy.image = set.images[set.acquired].image;
    toCopy.subresourceRange = {set.aspect, 0, 1, 0, 1};
    const VkPipelineStageFlags sourceStage = depth
        ? VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    vkCmdPipelineBarrier(command, sourceStage, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
        0, nullptr, 0, nullptr, 1, &toCopy);
    VkBufferImageCopy copy{};
    copy.bufferOffset = bufferOffset;
    copy.imageSubresource = {set.aspect, 0, 0, 1};
    copy.imageOffset = {x, y, 0};
    copy.imageExtent = {1, 1, 1};
    vkCmdCopyImageToBuffer(command, set.images[set.acquired].image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        buffer, 1, &copy);
    VkImageMemoryBarrier toRuntime = toCopy;
    toRuntime.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    toRuntime.dstAccessMask = depth
        ? VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
        : VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    toRuntime.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    toRuntime.newLayout = toCopy.oldLayout;
    const VkPipelineStageFlags destinationStage = depth
        ? VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT
        : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, destinationStage, 0,
        0, nullptr, 0, nullptr, 1, &toRuntime);
}

float halfToFloat(uint16_t value)
{
    const uint32_t sign = static_cast<uint32_t>(value & 0x8000u) << 16;
    uint32_t exponent = (value >> 10) & 0x1fu;
    uint32_t mantissa = value & 0x03ffu;
    uint32_t bits = sign;
    if(exponent == 0)
    {
        if(mantissa)
        {
            int shift = 0;
            while((mantissa & 0x0400u) == 0) { mantissa <<= 1; ++shift; }
            mantissa &= 0x03ffu;
            const uint32_t exp32 = static_cast<uint32_t>(127 - 15 - shift + 1);
            bits |= (exp32 << 23) | (mantissa << 13);
        }
    }
    else if(exponent == 0x1fu)
        bits |= 0x7f800000u | (mantissa << 13);
    else
        bits |= ((exponent + (127 - 15)) << 23) | (mantissa << 13);
    float result = 0.f;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

void printReadback(const Readback& readback)
{
    const auto* bytes = static_cast<const uint8_t*>(readback.mapped);
    for(uint32_t eye = 0; eye < 2; ++eye)
    {
        const size_t base = eye * 40;
        uint16_t motionX = 0, motionY = 0, staticX = 0, staticY = 0;
        float depth = 0.f, backgroundDepth = 0.f;
        std::memcpy(&motionX, bytes + base + 8, sizeof(motionX));
        std::memcpy(&motionY, bytes + base + 10, sizeof(motionY));
        std::memcpy(&depth, bytes + base + 16, sizeof(depth));
        std::memcpy(&staticX, bytes + base + 24, sizeof(staticX));
        std::memcpy(&staticY, bytes + base + 26, sizeof(staticY));
        std::memcpy(&backgroundDepth, bytes + base + 32, sizeof(backgroundDepth));
        std::cout << (eye ? "readback R" : "readback L") << " color="
            << static_cast<unsigned>(bytes[base]) << ',' << static_cast<unsigned>(bytes[base + 1]) << ','
            << static_cast<unsigned>(bytes[base + 2]) << " motion=" << halfToFloat(motionX) << ','
            << halfToFloat(motionY) << " depth=" << depth << " staticMotion="
            << halfToFloat(staticX) << ',' << halfToFloat(staticY) << " staticDepth="
            << backgroundDepth << '\n';
    }
}

template<class T> bool loadXr(XrInstance instance, const char* name, T& fn)
{
    return XR_SUCCEEDED(xrGetInstanceProcAddr(instance, name, reinterpret_cast<PFN_xrVoidFunction*>(&fn)));
}

std::vector<const char*> splitNames(const std::string& source, std::vector<std::string>& owned)
{
    std::istringstream input(source);
    std::string name;
    while(input >> name) owned.push_back(name);
    std::vector<const char*> result;
    for(const auto& item : owned) result.push_back(item.c_str());
    return result;
}

#ifndef _WIN32
bool probeOpenGlImportOnOpenXrDevice(XrInstance xrInstance, XrSession xrSession,
    VkPhysicalDevice physical, VkDevice device, uint32_t queueFamily, VkFormat xrColorFormat)
{
    constexpr uint32_t width = 64, height = 64;
    if(SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        std::cerr << "SDL_Init for GL import probe: " << SDL_GetError() << '\n';
        return false;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    SDL_Window* window = SDL_CreateWindow("OpenXR Vulkan GL-import probe", SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    if(!window)
    {
        std::cerr << "SDL_CreateWindow for GL import probe: " << SDL_GetError() << '\n';
        SDL_Quit();
        return false;
    }
    SDL_GLContext context = SDL_GL_CreateContext(window);
    if(!context)
    {
        std::cerr << "SDL_GL_CreateContext for GL import probe: " << SDL_GetError() << '\n';
        SDL_DestroyWindow(window);
        SDL_Quit();
        return false;
    }

    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkBuffer readbackBuffer = VK_NULL_HANDLE;
    VkDeviceMemory readbackMemory = VK_NULL_HANDLE;
    VkSemaphore vkToGl = VK_NULL_HANDLE, glToVk = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkCommandBuffer command = VK_NULL_HANDLE;
    GLuint glSemaphores[2]{};
    GLuint glMemory = 0, texture = 0, framebuffer = 0;
    XrSwapchain colorSwapchain = XR_NULL_HANDLE;
    uint32_t colorSwapchainIndex = 0;
    bool colorSwapchainAcquired = false, colorSwapchainWaited = false, probeFrameBegun = false;
    XrTime probeDisplayTime = 0;
    int fd = -1;
    bool imported = false;
    const auto deleteMemory = reinterpret_cast<PFNGLDELETEMEMORYOBJECTSEXTPROC>(SDL_GL_GetProcAddress("glDeleteMemoryObjectsEXT"));
    const auto cleanup = [&]
    {
        if(fd >= 0) close(fd);
        if(framebuffer) glDeleteFramebuffers(1, &framebuffer);
        if(texture) glDeleteTextures(1, &texture);
        if(glMemory && deleteMemory) deleteMemory(1, &glMemory);
        if(glSemaphores[0] || glSemaphores[1])
        {
            const auto deleteSemaphores = reinterpret_cast<PFNGLDELETESEMAPHORESEXTPROC>(SDL_GL_GetProcAddress("glDeleteSemaphoresEXT"));
            if(deleteSemaphores) deleteSemaphores(2, glSemaphores);
        }
        if(commandPool) vkDestroyCommandPool(device, commandPool, nullptr);
        if(colorSwapchainAcquired && colorSwapchainWaited)
        {
            XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
            xrReleaseSwapchainImage(colorSwapchain, &releaseInfo);
        }
        if(probeFrameBegun)
        {
            XrFrameEndInfo endInfo{XR_TYPE_FRAME_END_INFO};
            endInfo.displayTime = probeDisplayTime;
            endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
            xrEndFrame(xrSession, &endInfo);
        }
        if(colorSwapchain) xrDestroySwapchain(colorSwapchain);
        if(vkToGl) vkDestroySemaphore(device, vkToGl, nullptr);
        if(glToVk) vkDestroySemaphore(device, glToVk, nullptr);
        if(readbackBuffer) vkDestroyBuffer(device, readbackBuffer, nullptr);
        if(readbackMemory) vkFreeMemory(device, readbackMemory, nullptr);
        if(memory) vkFreeMemory(device, memory, nullptr);
        if(image) vkDestroyImage(device, image, nullptr);
        SDL_GL_DeleteContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
    };
    if(!SDL_GL_ExtensionSupported("GL_EXT_memory_object") ||
        !SDL_GL_ExtensionSupported("GL_EXT_memory_object_fd") ||
        !SDL_GL_ExtensionSupported("GL_EXT_semaphore") ||
        !SDL_GL_ExtensionSupported("GL_EXT_semaphore_fd"))
    {
        std::cerr << "GL external-memory extensions unavailable on the current context\n";
        cleanup();
        return false;
    }
    std::cout << "glExternalFormatQuery:";
    for(const auto [name, format] : {std::pair{"RGBA8", static_cast<GLenum>(GL_RGBA8)},
        std::pair{"RGBA16F", static_cast<GLenum>(GL_RGBA16F)},
        std::pair{"D32F", static_cast<GLenum>(GL_DEPTH_COMPONENT32F)}})
    {
        GLint supported = 0, framebufferRenderable = 0, colorRenderable = 0,
            depthRenderable = 0, tilingCount = 0;
        glGetInternalformativ(GL_TEXTURE_2D, format, GL_INTERNALFORMAT_SUPPORTED, 1, &supported);
        glGetInternalformativ(GL_TEXTURE_2D, format, GL_FRAMEBUFFER_RENDERABLE, 1, &framebufferRenderable);
        glGetInternalformativ(GL_TEXTURE_2D, format, GL_COLOR_RENDERABLE, 1, &colorRenderable);
        glGetInternalformativ(GL_TEXTURE_2D, format, GL_DEPTH_RENDERABLE, 1, &depthRenderable);
        glGetInternalformativ(GL_TEXTURE_2D, format, GL_NUM_TILING_TYPES_EXT, 1, &tilingCount);
        std::vector<GLint> tilings(tilingCount > 0 ? static_cast<size_t>(tilingCount) : 0);
        if(!tilings.empty())
            glGetInternalformativ(GL_TEXTURE_2D, format, GL_TILING_TYPES_EXT,
                static_cast<GLsizei>(tilings.size()), tilings.data());
        std::cout << ' ' << name << "{supported=" << supported << ",fbo=" << framebufferRenderable
            << ",color=" << colorRenderable << ",depth=" << depthRenderable << ",tilings=";
        for(size_t index = 0; index < tilings.size(); ++index)
        {
            if(index) std::cout << ',';
            std::cout << "0x" << std::hex << tilings[index] << std::dec;
        }
        std::cout << '}';
    }
    std::cout << std::endl;
    if(glGetError() != GL_NO_ERROR)
    {
        std::cerr << "OpenGL external-format capability query generated an error\n";
        cleanup();
        return false;
    }
    const auto createMemory = reinterpret_cast<PFNGLCREATEMEMORYOBJECTSEXTPROC>(SDL_GL_GetProcAddress("glCreateMemoryObjectsEXT"));
    const auto memoryParameter = reinterpret_cast<PFNGLMEMORYOBJECTPARAMETERIVEXTPROC>(SDL_GL_GetProcAddress("glMemoryObjectParameterivEXT"));
    const auto importMemory = reinterpret_cast<PFNGLIMPORTMEMORYFDEXTPROC>(SDL_GL_GetProcAddress("glImportMemoryFdEXT"));
    const auto textureStorage = reinterpret_cast<PFNGLTEXSTORAGEMEM2DEXTPROC>(SDL_GL_GetProcAddress("glTexStorageMem2DEXT"));
    const auto getUnsignedByte = reinterpret_cast<PFNGLGETUNSIGNEDBYTEI_VEXTPROC>(SDL_GL_GetProcAddress("glGetUnsignedBytei_vEXT"));
    const auto createGlSemaphores = reinterpret_cast<PFNGLGENSEMAPHORESEXTPROC>(SDL_GL_GetProcAddress("glGenSemaphoresEXT"));
    const auto importGlSemaphore = reinterpret_cast<PFNGLIMPORTSEMAPHOREFDEXTPROC>(SDL_GL_GetProcAddress("glImportSemaphoreFdEXT"));
    const auto signalGlSemaphore = reinterpret_cast<PFNGLSIGNALSEMAPHOREEXTPROC>(SDL_GL_GetProcAddress("glSignalSemaphoreEXT"));
    const auto waitGlSemaphore = reinterpret_cast<PFNGLWAITSEMAPHOREEXTPROC>(SDL_GL_GetProcAddress("glWaitSemaphoreEXT"));
    const auto deleteGlSemaphores = reinterpret_cast<PFNGLDELETESEMAPHORESEXTPROC>(SDL_GL_GetProcAddress("glDeleteSemaphoresEXT"));
    if(!createMemory || !memoryParameter || !importMemory || !textureStorage || !getUnsignedByte || !deleteMemory ||
        !createGlSemaphores || !importGlSemaphore || !signalGlSemaphore || !waitGlSemaphore || !deleteGlSemaphores)
    {
        std::cerr << "GL external-memory entrypoint missing\n";
        cleanup();
        return false;
    }
    VkPhysicalDeviceIDProperties ids{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES};
    VkPhysicalDeviceProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    properties.pNext = &ids;
    vkGetPhysicalDeviceProperties2(physical, &properties);
    std::array<uint8_t, VK_UUID_SIZE> glDevice{}, glDriver{};
    getUnsignedByte(GL_DEVICE_UUID_EXT, 0, glDevice.data());
    const auto getDriverUuid = reinterpret_cast<PFNGLGETUNSIGNEDBYTEVEXTPROC>(SDL_GL_GetProcAddress("glGetUnsignedBytevEXT"));
    if(!getDriverUuid)
    {
        std::cerr << "glGetUnsignedBytevEXT unavailable\n";
        cleanup();
        return false;
    }
    getDriverUuid(GL_DRIVER_UUID_EXT, glDriver.data());
    if(std::memcmp(ids.deviceUUID, glDevice.data(), VK_UUID_SIZE) != 0 ||
        std::memcmp(ids.driverUUID, glDriver.data(), VK_UUID_SIZE) != 0)
    {
        std::cerr << "OpenXR Vulkan and GL device/driver UUIDs differ; refusing memory import\n";
        cleanup();
        return false;
    }

    const auto getMemoryFd = reinterpret_cast<PFN_vkGetMemoryFdKHR>(vkGetDeviceProcAddr(device, "vkGetMemoryFdKHR"));
    const auto getSemaphoreFd = reinterpret_cast<PFN_vkGetSemaphoreFdKHR>(vkGetDeviceProcAddr(device, "vkGetSemaphoreFdKHR"));
    if(!getMemoryFd || !getSemaphoreFd)
    {
        std::cerr << "OpenXR Vulkan device has no external memory/semaphore FD entrypoints\n";
        cleanup();
        return false;
    }
    const VkFormat viewFormats[] = {VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_A8B8G8R8_SRGB_PACK32};
    VkImageFormatListCreateInfo formatList{VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO};
    formatList.viewFormatCount = 2;
    formatList.pViewFormats = viewFormats;
    VkExternalMemoryImageCreateInfo externalInfo{VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO};
    externalInfo.pNext = &formatList;
    externalInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.pNext = &externalInfo;
    imageInfo.flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT |
        VK_IMAGE_CREATE_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_BIT_EXT;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.extent = {width, height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
        VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_ATTACHMENT_FEEDBACK_LOOP_BIT_EXT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkResult vkResult = vkCreateImage(device, &imageInfo, nullptr, &image);
    if(vkResult != VK_SUCCESS)
    {
        std::cerr << "OpenXR device external VkImage creation failed: " << vkResult << '\n';
        cleanup();
        return false;
    }
    VkMemoryRequirements requirements{};
    vkGetImageMemoryRequirements(device, image, &requirements);
    uint32_t memoryType = UINT32_MAX;
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    vkGetPhysicalDeviceMemoryProperties(physical, &memoryProperties);
    for(uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
    {
        if((requirements.memoryTypeBits & (1u << i)) &&
            (memoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
        {
            memoryType = i;
            break;
        }
    }
    if(memoryType == UINT32_MAX)
    {
        std::cerr << "No device-local memory type for external image\n";
        cleanup();
        return false;
    }
    VkMemoryDedicatedAllocateInfo dedicated{VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO};
    dedicated.image = image;
    VkExportMemoryAllocateInfo exportInfo{VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO};
    exportInfo.pNext = &dedicated;
    exportInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocation.pNext = &exportInfo;
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = memoryType;
    vkResult = vkAllocateMemory(device, &allocation, nullptr, &memory);
    if(vkResult == VK_SUCCESS) vkResult = vkBindImageMemory(device, image, memory, 0);
    if(vkResult != VK_SUCCESS)
    {
        std::cerr << "OpenXR device dedicated external allocation failed: " << vkResult << '\n';
        cleanup();
        return false;
    }
    VkMemoryGetFdInfoKHR fdInfo{VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR};
    fdInfo.memory = memory;
    fdInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
    vkResult = getMemoryFd(device, &fdInfo, &fd);
    if(vkResult != VK_SUCCESS)
    {
        std::cerr << "OpenXR device external FD export failed: " << vkResult << '\n';
        cleanup();
        return false;
    }
    createMemory(1, &glMemory);
    const GLint enabled = GL_TRUE;
    memoryParameter(glMemory, GL_DEDICATED_MEMORY_OBJECT_EXT, &enabled);
    importMemory(glMemory, requirements.size, GL_HANDLE_TYPE_OPAQUE_FD_EXT, fd);
    const GLenum importError = glGetError();
    if(importError == GL_NO_ERROR) { fd = -1; imported = true; }
    else
    {
        std::cerr << "GL import of OpenXR-device memory failed: 0x" << std::hex << importError << std::dec << '\n';
        cleanup();
        return false;
    }
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_TILING_EXT, GL_OPTIMAL_TILING_EXT);
    textureStorage(GL_TEXTURE_2D, 1, GL_RGBA8, width, height, glMemory, 0);
    if(glGetError() != GL_NO_ERROR)
    {
        std::cerr << "GL external texture storage on OpenXR VkDevice failed\n";
        cleanup();
        return false;
    }
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if(status != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cerr << "GL framebuffer for OpenXR-device memory incomplete: 0x" << std::hex << status << std::dec << '\n';
        cleanup();
        return false;
    }

    VkExportSemaphoreCreateInfo exportSemaphore{VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO};
    exportSemaphore.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
    VkSemaphoreCreateInfo semaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    semaphoreInfo.pNext = &exportSemaphore;
    if(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &vkToGl) != VK_SUCCESS ||
        vkCreateSemaphore(device, &semaphoreInfo, nullptr, &glToVk) != VK_SUCCESS)
    {
        std::cerr << "OpenXR Vulkan external semaphore creation failed\n";
        cleanup();
        return false;
    }
    createGlSemaphores(2, glSemaphores);
    for(const auto [vkSemaphore, glSemaphore] : {std::pair{vkToGl, glSemaphores[0]},
        std::pair{glToVk, glSemaphores[1]}})
    {
        VkSemaphoreGetFdInfoKHR semaphoreFdInfo{VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR};
        semaphoreFdInfo.semaphore = vkSemaphore;
        semaphoreFdInfo.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
        if(getSemaphoreFd(device, &semaphoreFdInfo, &fd) != VK_SUCCESS)
        {
            std::cerr << "OpenXR Vulkan semaphore FD export failed\n";
            cleanup();
            return false;
        }
        importGlSemaphore(glSemaphore, GL_HANDLE_TYPE_OPAQUE_FD_EXT, fd);
        const GLenum semaphoreImportError = glGetError();
        if(semaphoreImportError != GL_NO_ERROR)
        {
            close(fd);
            fd = -1;
            std::cerr << "GL import of OpenXR-device semaphore failed: 0x" << std::hex << semaphoreImportError << std::dec << '\n';
            cleanup();
            return false;
        }
        fd = -1;
    }

    VkQueue queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(device, queueFamily, 0, &queue);
    VkCommandPoolCreateInfo commandPoolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolInfo.queueFamilyIndex = queueFamily;
    if(!queue || vkCreateCommandPool(device, &commandPoolInfo, nullptr, &commandPool) != VK_SUCCESS)
    {
        std::cerr << "OpenXR Vulkan command-pool creation failed\n";
        cleanup();
        return false;
    }
    VkCommandBufferAllocateInfo commandInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    commandInfo.commandPool = commandPool;
    commandInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandInfo.commandBufferCount = 1;
    if(vkAllocateCommandBuffers(device, &commandInfo, &command) != VK_SUCCESS)
    {
        std::cerr << "OpenXR Vulkan command-buffer allocation failed\n";
        cleanup();
        return false;
    }
    VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = 4;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if(vkCreateBuffer(device, &bufferInfo, nullptr, &readbackBuffer) != VK_SUCCESS)
    {
        std::cerr << "OpenXR Vulkan readback-buffer creation failed\n";
        cleanup();
        return false;
    }
    VkMemoryRequirements bufferRequirements{};
    vkGetBufferMemoryRequirements(device, readbackBuffer, &bufferRequirements);
    VkPhysicalDeviceMemoryProperties readbackProperties{};
    vkGetPhysicalDeviceMemoryProperties(physical, &readbackProperties);
    uint32_t hostMemoryType = UINT32_MAX;
    for(uint32_t i = 0; i < readbackProperties.memoryTypeCount; ++i)
        if((bufferRequirements.memoryTypeBits & (1u << i)) &&
            (readbackProperties.memoryTypes[i].propertyFlags &
                (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
                (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
        { hostMemoryType = i; break; }
    if(hostMemoryType == UINT32_MAX)
    {
        std::cerr << "No coherent host-visible memory for OpenXR Vulkan readback\n";
        cleanup();
        return false;
    }
    VkMemoryAllocateInfo bufferAllocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    bufferAllocation.allocationSize = bufferRequirements.size;
    bufferAllocation.memoryTypeIndex = hostMemoryType;
    if(vkAllocateMemory(device, &bufferAllocation, nullptr, &readbackMemory) != VK_SUCCESS ||
        vkBindBufferMemory(device, readbackBuffer, readbackMemory, 0) != VK_SUCCESS)
    {
        std::cerr << "OpenXR Vulkan readback-buffer allocation failed\n";
        cleanup();
        return false;
    }
    void* readbackMapped = nullptr;
    if(vkMapMemory(device, readbackMemory, 0, bufferRequirements.size, 0, &readbackMapped) != VK_SUCCESS)
    {
        std::cerr << "OpenXR Vulkan readback-buffer map failed\n";
        cleanup();
        return false;
    }
    const auto beginCommands = [&]
    {
        vkResetCommandBuffer(command, 0);
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        return vkBeginCommandBuffer(command, &begin) == VK_SUCCESS;
    };
    const auto endSubmit = [&](VkSemaphore waitSemaphore, VkSemaphore signalSemaphore)
    {
        if(vkEndCommandBuffer(command) != VK_SUCCESS) return false;
        const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.waitSemaphoreCount = waitSemaphore ? 1u : 0u;
        submit.pWaitSemaphores = waitSemaphore ? &waitSemaphore : nullptr;
        submit.pWaitDstStageMask = waitSemaphore ? &waitStage : nullptr;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &command;
        submit.signalSemaphoreCount = signalSemaphore ? 1u : 0u;
        submit.pSignalSemaphores = signalSemaphore ? &signalSemaphore : nullptr;
        return vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE) == VK_SUCCESS && vkQueueWaitIdle(queue) == VK_SUCCESS;
    };
    constexpr uint32_t externalFamily = VK_QUEUE_FAMILY_EXTERNAL;
    if(!beginCommands())
    {
        vkUnmapMemory(device, readbackMemory);
        cleanup();
        return false;
    }
    VkImageMemoryBarrier toExternal{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    toExternal.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    toExternal.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    toExternal.srcQueueFamilyIndex = queueFamily;
    toExternal.dstQueueFamilyIndex = externalFamily;
    toExternal.image = image;
    toExternal.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0, 0, nullptr, 0, nullptr, 1, &toExternal);
    if(!endSubmit(VK_NULL_HANDLE, vkToGl))
    {
        vkUnmapMemory(device, readbackMemory);
        cleanup();
        std::cerr << "OpenXR Vulkan initial external release failed\n";
        return false;
    }
    const GLenum generalLayout = GL_LAYOUT_GENERAL_EXT;
    waitGlSemaphore(glSemaphores[0], 0, nullptr, 1, &texture, &generalLayout);
    if(glGetError() != GL_NO_ERROR)
    {
        vkUnmapMemory(device, readbackMemory);
        cleanup();
        std::cerr << "GL wait for OpenXR Vulkan image ownership failed\n";
        return false;
    }
    constexpr std::array<uint8_t, 4> expected{{17, 83, 191, 255}};
    glDisable(GL_FRAMEBUFFER_SRGB);
    glDisable(GL_DITHER);
    glClearColor(expected[0] / 255.f, expected[1] / 255.f, expected[2] / 255.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    signalGlSemaphore(glSemaphores[1], 0, nullptr, 1, &texture, &generalLayout);
    glFlush();
    if(glGetError() != GL_NO_ERROR || !beginCommands())
    {
        vkUnmapMemory(device, readbackMemory);
        cleanup();
        std::cerr << "GL write or OpenXR Vulkan command begin failed\n";
        return false;
    }
    VkImageMemoryBarrier acquireExternal{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    acquireExternal.srcAccessMask = 0;
    acquireExternal.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    acquireExternal.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    acquireExternal.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    acquireExternal.srcQueueFamilyIndex = externalFamily;
    acquireExternal.dstQueueFamilyIndex = queueFamily;
    acquireExternal.image = image;
    acquireExternal.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &acquireExternal);
    VkBufferImageCopy copy{};
    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.imageOffset = {static_cast<int32_t>(width / 2), static_cast<int32_t>(height / 2), 0};
    copy.imageExtent = {1, 1, 1};
    vkCmdCopyImageToBuffer(command, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readbackBuffer, 1, &copy);
    VkBufferMemoryBarrier hostBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    hostBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    hostBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    hostBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    hostBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    hostBarrier.buffer = readbackBuffer;
    hostBarrier.size = 4;
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
        0, 0, nullptr, 1, &hostBarrier, 0, nullptr);
    VkImageMemoryBarrier releaseExternal = acquireExternal;
    releaseExternal.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    releaseExternal.dstAccessMask = 0;
    releaseExternal.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    releaseExternal.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    releaseExternal.srcQueueFamilyIndex = queueFamily;
    releaseExternal.dstQueueFamilyIndex = externalFamily;
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0, 0, nullptr, 0, nullptr, 1, &releaseExternal);
    if(!endSubmit(glToVk, vkToGl))
    {
        vkUnmapMemory(device, readbackMemory);
        cleanup();
        std::cerr << "OpenXR Vulkan external acquire/readback failed\n";
        return false;
    }
    waitGlSemaphore(glSemaphores[0], 0, nullptr, 1, &texture, &generalLayout);
    glFinish();
    // The semaphore wait above reacquires the shared image for GL. Hand it
    // back explicitly before the second Vulkan use below.
    signalGlSemaphore(glSemaphores[0], 0, nullptr, 1, &texture, &generalLayout);
    glFlush();
    std::array<uint8_t, 4> actual{};
    std::memcpy(actual.data(), readbackMapped, actual.size());
    if(glGetError() != GL_NO_ERROR || actual != expected)
    {
        std::cerr << "OpenXR Vulkan pixel readback mismatch expected="
            << static_cast<unsigned>(expected[0]) << ',' << static_cast<unsigned>(expected[1]) << ','
            << static_cast<unsigned>(expected[2]) << " actual=" << static_cast<unsigned>(actual[0]) << ','
            << static_cast<unsigned>(actual[1]) << ',' << static_cast<unsigned>(actual[2]) << '\n';
        vkUnmapMemory(device, readbackMemory);
        cleanup();
        return false;
    }
    std::cout << "openxrColorCopy=GL-source-verified rgba=" << static_cast<unsigned>(actual[0]) << ','
        << static_cast<unsigned>(actual[1]) << ',' << static_cast<unsigned>(actual[2]) << std::endl;
    // Verify that the same GL-produced image can be copied into an OpenXR-owned
    // Vulkan swapchain image. This still does not prove frame-synthesis use; it
    // isolates the renderer-to-runtime image transport that Quake needs first.
    XrSwapchainCreateInfo swapchainInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
    swapchainInfo.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT |
        XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT;
    swapchainInfo.format = static_cast<int64_t>(xrColorFormat);
    swapchainInfo.sampleCount = 1;
    swapchainInfo.width = width;
    swapchainInfo.height = height;
    swapchainInfo.faceCount = 1;
    swapchainInfo.arraySize = 1;
    swapchainInfo.mipCount = 1;
    if(XR_FAILED(xrCreateSwapchain(xrSession, &swapchainInfo, &colorSwapchain)))
    {
        std::cerr << "OpenXR transfer swapchain creation failed for VkFormat=" << xrColorFormat << '\n';
        vkUnmapMemory(device, readbackMemory);
        cleanup();
        return false;
    }
    std::cout << "openxrColorCopy=swapchain-created format=" << xrColorFormat << std::endl;
    uint32_t xrImageCount = 0;
    XrResult xrResult = xrEnumerateSwapchainImages(colorSwapchain, 0, &xrImageCount, nullptr);
    std::vector<XrSwapchainImageVulkanKHR> xrImages(xrImageCount, {XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR});
    if(XR_FAILED(xrResult) || xrImageCount == 0 ||
        XR_FAILED(xrEnumerateSwapchainImages(colorSwapchain, xrImageCount, &xrImageCount,
            reinterpret_cast<XrSwapchainImageBaseHeader*>(xrImages.data()))))
    {
        std::cerr << "OpenXR transfer swapchain image enumeration failed\n";
        vkUnmapMemory(device, readbackMemory);
        cleanup();
        return false;
    }
    XrFrameWaitInfo probeWaitInfo{XR_TYPE_FRAME_WAIT_INFO};
    XrFrameState probeFrameState{XR_TYPE_FRAME_STATE};
    xrResult = xrWaitFrame(xrSession, &probeWaitInfo, &probeFrameState);
    if(XR_SUCCEEDED(xrResult))
    {
        XrFrameBeginInfo probeBeginInfo{XR_TYPE_FRAME_BEGIN_INFO};
        xrResult = xrBeginFrame(xrSession, &probeBeginInfo);
        probeFrameBegun = XR_SUCCEEDED(xrResult);
        probeDisplayTime = probeFrameState.predictedDisplayTime;
    }
    if(XR_FAILED(xrResult))
    {
        std::cerr << "OpenXR probe frame begin failed: " << xrResult << '\n';
        vkUnmapMemory(device, readbackMemory);
        cleanup();
        return false;
    }
    std::cout << "openxrColorCopy=frame-begun shouldRender=" << probeFrameState.shouldRender << std::endl;
    XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
    xrResult = xrAcquireSwapchainImage(colorSwapchain, &acquireInfo, &colorSwapchainIndex);
    colorSwapchainAcquired = XR_SUCCEEDED(xrResult);
    XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
    waitInfo.timeout = 100'000'000; // Retry in 100ms slices; XrDuration is nanoseconds.
    if(xrResult == XR_SUCCESS)
    {
        const auto waitDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        do
        {
            xrResult = xrWaitSwapchainImage(colorSwapchain, &waitInfo);
            if(xrResult == XR_TIMEOUT_EXPIRED)
                continue;
            colorSwapchainWaited = xrResult == XR_SUCCESS;
            break;
        } while(std::chrono::steady_clock::now() < waitDeadline);
    }
    if(xrResult != XR_SUCCESS)
    {
        std::cerr << "OpenXR transfer swapchain acquire/wait failed: " << xrResult << '\n';
        vkUnmapMemory(device, readbackMemory);
        cleanup();
        return false;
    }
    std::cout << "openxrColorCopy=swapchain-image-ready index=" << colorSwapchainIndex << std::endl;
    if(!beginCommands())
    {
        vkUnmapMemory(device, readbackMemory);
        cleanup();
        return false;
    }
    VkImageMemoryBarrier acquireForCopy = acquireExternal;
    acquireForCopy.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    acquireForCopy.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    acquireForCopy.srcQueueFamilyIndex = externalFamily;
    acquireForCopy.dstQueueFamilyIndex = queueFamily;
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &acquireForCopy);
    VkImage xrImage = xrImages[colorSwapchainIndex].image;
    VkImageMemoryBarrier xrImageReady{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    xrImageReady.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    xrImageReady.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    xrImageReady.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    xrImageReady.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    xrImageReady.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    xrImageReady.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    xrImageReady.image = xrImage;
    xrImageReady.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &xrImageReady);
    VkImageCopy imageCopy{};
    imageCopy.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    imageCopy.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    imageCopy.extent = {width, height, 1};
    vkCmdCopyImage(command, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        xrImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageCopy);
    VkImageMemoryBarrier xrImageReadable = xrImageReady;
    xrImageReadable.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    xrImageReadable.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    xrImageReadable.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    xrImageReadable.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &xrImageReadable);
    vkCmdCopyImageToBuffer(command, xrImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readbackBuffer, 1, &copy);
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
        0, 0, nullptr, 1, &hostBarrier, 0, nullptr);
    VkImageMemoryBarrier releaseAfterCopy = acquireForCopy;
    releaseAfterCopy.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    releaseAfterCopy.dstAccessMask = 0;
    releaseAfterCopy.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    releaseAfterCopy.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    releaseAfterCopy.srcQueueFamilyIndex = queueFamily;
    releaseAfterCopy.dstQueueFamilyIndex = externalFamily;
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0, 0, nullptr, 0, nullptr, 1, &releaseAfterCopy);
    std::cout << "openxrColorCopy=queue-submit\n" << std::flush;
    if(!endSubmit(vkToGl, VK_NULL_HANDLE))
    {
        vkUnmapMemory(device, readbackMemory);
        cleanup();
        std::cerr << "OpenXR swapchain image transfer/readback submit failed\n";
        return false;
    }
    std::cout << "openxrColorCopy=queue-complete\n" << std::flush;
    const std::array<uint8_t, 4> copiedPixel{{
        static_cast<uint8_t*>(readbackMapped)[0], static_cast<uint8_t*>(readbackMapped)[1],
        static_cast<uint8_t*>(readbackMapped)[2], static_cast<uint8_t*>(readbackMapped)[3]}};
    // A direct runtime-image clear/readback separates swapchain image access
    // from the cross-image copy and source-format path.
    if(!beginCommands())
    {
        vkUnmapMemory(device, readbackMemory);
        cleanup();
        return false;
    }
    VkImageMemoryBarrier xrImageForClear = xrImageReadable;
    xrImageForClear.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    xrImageForClear.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    xrImageForClear.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    xrImageForClear.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &xrImageForClear);
    VkClearColorValue clearValue{{0.f, 0.f, 0.f, 1.f}};
    VkImageSubresourceRange clearRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdClearColorImage(command, xrImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue, 1, &clearRange);
    VkImageMemoryBarrier xrImageForReadback = xrImageForClear;
    xrImageForReadback.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    xrImageForReadback.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    xrImageForReadback.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    xrImageForReadback.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &xrImageForReadback);
    vkCmdCopyImageToBuffer(command, xrImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readbackBuffer, 1, &copy);
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
        0, 0, nullptr, 1, &hostBarrier, 0, nullptr);
    VkImageMemoryBarrier xrImageForRelease = xrImageForReadback;
    xrImageForRelease.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    xrImageForRelease.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    xrImageForRelease.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    xrImageForRelease.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &xrImageForRelease);
    if(!endSubmit(VK_NULL_HANDLE, VK_NULL_HANDLE))
    {
        vkUnmapMemory(device, readbackMemory);
        cleanup();
        std::cerr << "OpenXR direct clear/readback submit failed\n";
        return false;
    }
    std::array<uint8_t, 4> clearPixel{};
    std::memcpy(clearPixel.data(), readbackMapped, clearPixel.size());
    std::cout << "openxrColorCopy=direct-clear-readback rgba="
        << static_cast<unsigned>(clearPixel[0]) << ',' << static_cast<unsigned>(clearPixel[1]) << ','
        << static_cast<unsigned>(clearPixel[2]) << ',' << static_cast<unsigned>(clearPixel[3]) << std::endl;
    XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    xrResult = xrReleaseSwapchainImage(colorSwapchain, &releaseInfo);
    colorSwapchainAcquired = colorSwapchainWaited = false;
    const std::array<uint8_t, 4> xrActual = copiedPixel;
    vkUnmapMemory(device, readbackMemory);
    if(XR_FAILED(xrResult) || copiedPixel != expected || clearPixel != std::array<uint8_t, 4>{{0, 0, 0, 255}})
    {
        std::cerr << "OpenXR swapchain pixel readback mismatch result=" << xrResult
            << " expected=" << static_cast<unsigned>(expected[0]) << ',' << static_cast<unsigned>(expected[1])
            << ',' << static_cast<unsigned>(expected[2]) << " copied=" << static_cast<unsigned>(copiedPixel[0])
            << ',' << static_cast<unsigned>(copiedPixel[1]) << ',' << static_cast<unsigned>(copiedPixel[2])
            << " directClear=" << static_cast<unsigned>(clearPixel[0]) << ','
            << static_cast<unsigned>(clearPixel[1]) << ',' << static_cast<unsigned>(clearPixel[2]) << ','
            << static_cast<unsigned>(clearPixel[3]) << '\n';
        cleanup();
        return false;
    }
    std::cout << "openxrVkGlImport=PASS extent=" << width << 'x' << height
        << " allocationSize=" << requirements.size << " memoryType=" << memoryType
        << " uuidMatch=1 framebuffer=complete GLtoVKpixel=" << static_cast<unsigned>(actual[0]) << ','
        << static_cast<unsigned>(actual[1]) << ',' << static_cast<unsigned>(actual[2]) << " zeroMismatch=1\n";
    std::cout << "openxrColorCopy=PASS swapchainOwnedImage=1 format=" << xrColorFormat
        << " GLtoXRpixel=" << static_cast<unsigned>(xrActual[0]) << ','
        << static_cast<unsigned>(xrActual[1]) << ',' << static_cast<unsigned>(xrActual[2]) << " zeroMismatch=1\n";
    cleanup();
    return imported;
}
#endif

int main(int argc, char** argv)
{
    uint32_t frameLimit = 120;
    bool readbackRequested = false;
    bool transferDestinationRequested = false;
    bool glImportProbeRequested = false;
    for(int arg = 1; arg < argc; ++arg)
    {
        if(std::strcmp(argv[arg], "--frames") == 0 && arg + 1 < argc)
        {
            const unsigned long parsed = std::strtoul(argv[++arg], nullptr, 10);
            if(parsed == 0 || parsed > 3600)
            {
                std::cerr << "--frames must be between 1 and 3600\n";
                return 1;
            }
            frameLimit = static_cast<uint32_t>(parsed);
        }
        else if(std::strcmp(argv[arg], "--readback") == 0)
            readbackRequested = true;
        else if(std::strcmp(argv[arg], "--transfer-dst") == 0)
            transferDestinationRequested = true;
        else if(std::strcmp(argv[arg], "--gl-import-probe") == 0)
            glImportProbeRequested = true;
        else
        {
            std::cerr << "usage: frame_loop_moving [--frames count] [--readback] [--transfer-dst] [--gl-import-probe]\n";
            return 1;
        }
    }
    uint32_t count = 0;
    XrResult result = xrEnumerateInstanceExtensionProperties(nullptr, 0, &count, nullptr);
    if(XR_FAILED(result)) { std::cerr << "extension enumeration=" << result << '\n'; return 1; }
    std::vector<XrExtensionProperties> extensions(count, {XR_TYPE_EXTENSION_PROPERTIES});
    result = xrEnumerateInstanceExtensionProperties(nullptr, count, &count, extensions.data());
    if(XR_FAILED(result)) { std::cerr << "extension listing=" << result << '\n'; return 1; }
    bool hasVulkan = false, hasVulkan2 = false, hasSynthesis = false;
    for(const auto& extension : extensions)
    {
        hasVulkan |= !strcmp(extension.extensionName, XR_KHR_VULKAN_ENABLE_EXTENSION_NAME);
        hasVulkan2 |= !strcmp(extension.extensionName, XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME);
        hasSynthesis |= !strcmp(extension.extensionName, "XR_EXT_frame_synthesis");
    }
    std::cout << "XR_KHR_vulkan_enable=" << hasVulkan << " XR_KHR_vulkan_enable2=" << hasVulkan2
        << " XR_EXT_frame_synthesis=" << hasSynthesis << '\n';
    if(!hasVulkan2 || !hasSynthesis) return 2;

    const char* enabled[3]{};
    uint32_t enabledCount = 0;
    if(hasVulkan) enabled[enabledCount++] = XR_KHR_VULKAN_ENABLE_EXTENSION_NAME;
    enabled[enabledCount++] = XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME;
    if(hasSynthesis) enabled[enabledCount++] = "XR_EXT_frame_synthesis";
    XrInstanceCreateInfo instanceInfo{XR_TYPE_INSTANCE_CREATE_INFO};
    strcpy(instanceInfo.applicationInfo.applicationName, "QuakeVR Vulkan frame loop fixture");
    instanceInfo.applicationInfo.apiVersion = XR_MAKE_VERSION(1, 0, 0);
    instanceInfo.enabledExtensionCount = enabledCount;
    instanceInfo.enabledExtensionNames = enabled;
    XrInstance xrInstance = XR_NULL_HANDLE;
    result = xrCreateInstance(&instanceInfo, &xrInstance);
    if(XR_FAILED(result)) { std::cerr << "xrCreateInstance=" << result << '\n'; return 3; }

    XrSystemGetInfo systemInfo{XR_TYPE_SYSTEM_GET_INFO};
    systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    XrSystemId system = XR_NULL_SYSTEM_ID;
    result = xrGetSystem(xrInstance, &systemInfo, &system);
    if(XR_FAILED(result)) { std::cerr << "xrGetSystem=" << result << '\n'; xrDestroyInstance(xrInstance); return 4; }

    PFN_xrGetVulkanGraphicsRequirements2KHR getRequirements = nullptr;
    PFN_xrGetVulkanInstanceExtensionsKHR getInstanceExtensions = nullptr;
    PFN_xrGetVulkanDeviceExtensionsKHR getDeviceExtensions = nullptr;
    PFN_xrGetVulkanGraphicsDevice2KHR getGraphicsDevice = nullptr;
    PFN_xrCreateVulkanInstanceKHR createVulkanInstance = nullptr;
    PFN_xrCreateVulkanDeviceKHR createVulkanDevice = nullptr;
    loadXr(xrInstance, "xrGetVulkanGraphicsRequirements2KHR", getRequirements);
    if(hasVulkan)
    {
        loadXr(xrInstance, "xrGetVulkanInstanceExtensionsKHR", getInstanceExtensions);
        loadXr(xrInstance, "xrGetVulkanDeviceExtensionsKHR", getDeviceExtensions);
    }
    loadXr(xrInstance, "xrGetVulkanGraphicsDevice2KHR", getGraphicsDevice);
    loadXr(xrInstance, "xrCreateVulkanInstanceKHR", createVulkanInstance);
    loadXr(xrInstance, "xrCreateVulkanDeviceKHR", createVulkanDevice);
    if(!getRequirements || !getGraphicsDevice || !createVulkanInstance || !createVulkanDevice ||
        (hasVulkan && (!getInstanceExtensions || !getDeviceExtensions)))
    {
        std::cerr << "missing OpenXR Vulkan enable2 entrypoint\n";
        xrDestroyInstance(xrInstance);
        return 5;
    }

    XrGraphicsRequirementsVulkan2KHR requirements{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR};
    result = getRequirements(xrInstance, system, &requirements);
    if(XR_FAILED(result)) { std::cerr << "Vulkan requirements=" << result << '\n'; xrDestroyInstance(xrInstance); return 6; }
    std::vector<std::string> instanceNamesOwned, deviceNamesOwned;
    std::vector<const char*> instanceNames;
    uint32_t chars = 0;
    if(hasVulkan)
    {
        result = getInstanceExtensions(xrInstance, system, 0, &chars, nullptr);
        if(XR_FAILED(result)) { std::cerr << "Vulkan instance extensions size=" << result << '\n'; xrDestroyInstance(xrInstance); return 7; }
        std::string instanceExtensionString(chars, '\0');
        result = getInstanceExtensions(xrInstance, system, chars, &chars, instanceExtensionString.data());
        if(XR_FAILED(result)) { std::cerr << "Vulkan instance extensions=" << result << '\n'; xrDestroyInstance(xrInstance); return 7; }
        instanceNames = splitNames(instanceExtensionString, instanceNamesOwned);
    }

    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "QuakeVR Vulkan frame loop fixture";
    const auto toVkVersion = [](XrVersion version)
    {
        return VK_MAKE_API_VERSION(0, XR_VERSION_MAJOR(version), XR_VERSION_MINOR(version),
            XR_VERSION_PATCH(version));
    };
    const uint32_t minApiVersion = toVkVersion(requirements.minApiVersionSupported);
    uint32_t loaderApiVersion = VK_API_VERSION_1_0;
    if(vkEnumerateInstanceVersion(&loaderApiVersion) != VK_SUCCESS)
        loaderApiVersion = VK_API_VERSION_1_0;
    if(loaderApiVersion < minApiVersion)
    {
        std::cerr << "Vulkan loader is below OpenXR minimum\n";
        xrDestroyInstance(xrInstance);
        return 8;
    }
    app.apiVersion = minApiVersion;
    VkInstanceCreateInfo vkInstanceInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    vkInstanceInfo.pApplicationInfo = &app;
    vkInstanceInfo.enabledExtensionCount = static_cast<uint32_t>(instanceNames.size());
    vkInstanceInfo.ppEnabledExtensionNames = instanceNames.data();
    XrVulkanInstanceCreateInfoKHR xrVkInstanceInfo{XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR};
    xrVkInstanceInfo.systemId = system;
    xrVkInstanceInfo.pfnGetInstanceProcAddr = vkGetInstanceProcAddr;
    xrVkInstanceInfo.vulkanCreateInfo = &vkInstanceInfo;
    VkInstance vkInstance = VK_NULL_HANDLE;
    VkResult vkResult = VK_SUCCESS;
    result = createVulkanInstance(xrInstance, &xrVkInstanceInfo, &vkInstance, &vkResult);
    if(XR_FAILED(result) || vkResult != VK_SUCCESS)
    {
        std::cerr << "xrCreateVulkanInstance=" << result << " VkResult=" << vkResult << '\n';
        xrDestroyInstance(xrInstance);
        return 8;
    }

    XrVulkanGraphicsDeviceGetInfoKHR graphicsInfo{XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR};
    graphicsInfo.systemId = system;
    graphicsInfo.vulkanInstance = vkInstance;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    result = getGraphicsDevice(xrInstance, &graphicsInfo, &physicalDevice);
    if(XR_FAILED(result))
    {
        std::cerr << "xrGetVulkanGraphicsDevice2KHR=" << result << '\n';
        vkDestroyInstance(vkInstance, nullptr); xrDestroyInstance(xrInstance); return 9;
    }
    uint32_t queueCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, nullptr);
    std::vector<VkQueueFamilyProperties> queues(queueCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, queues.data());
    uint32_t queueFamily = ~0u;
    for(uint32_t i = 0; i < queueCount; ++i)
        if(queues[i].queueCount && (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) { queueFamily = i; break; }
    if(queueFamily == ~0u) { std::cerr << "no Vulkan graphics queue\n"; vkDestroyInstance(vkInstance, nullptr); xrDestroyInstance(xrInstance); return 10; }

    std::vector<const char*> deviceNames;
    if(hasVulkan)
    {
        chars = 0;
        result = getDeviceExtensions(xrInstance, system, 0, &chars, nullptr);
        if(XR_FAILED(result)) { std::cerr << "Vulkan device extensions size=" << result << '\n'; vkDestroyInstance(vkInstance, nullptr); xrDestroyInstance(xrInstance); return 11; }
        std::string deviceExtensionString(chars, '\0');
        result = getDeviceExtensions(xrInstance, system, chars, &chars, deviceExtensionString.data());
        if(XR_FAILED(result)) { std::cerr << "Vulkan device extensions=" << result << '\n'; vkDestroyInstance(vkInstance, nullptr); xrDestroyInstance(xrInstance); return 11; }
        deviceNames = splitNames(deviceExtensionString, deviceNamesOwned);
    }
    VkPhysicalDeviceMultisampledRenderToSingleSampledFeaturesEXT multisampleEnable{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_FEATURES_EXT};
    VkPhysicalDeviceAttachmentFeedbackLoopLayoutFeaturesEXT feedbackEnable{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ATTACHMENT_FEEDBACK_LOOP_LAYOUT_FEATURES_EXT};
    if(glImportProbeRequested)
    {
#ifdef _WIN32
        std::cerr << "--gl-import-probe currently requires Linux OPAQUE_FD interop\n";
        vkDestroyInstance(vkInstance, nullptr); xrDestroyInstance(xrInstance); return 11;
#else
        uint32_t availableDeviceExtensionCount = 0;
        if(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &availableDeviceExtensionCount, nullptr) != VK_SUCCESS)
        {
            std::cerr << "Vulkan device extension count failed\n";
            vkDestroyInstance(vkInstance, nullptr); xrDestroyInstance(xrInstance); return 11;
        }
        std::vector<VkExtensionProperties> availableDeviceExtensions(availableDeviceExtensionCount);
        if(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &availableDeviceExtensionCount,
            availableDeviceExtensions.data()) != VK_SUCCESS)
        {
            std::cerr << "Vulkan device extension enumeration failed\n";
            vkDestroyInstance(vkInstance, nullptr); xrDestroyInstance(xrInstance); return 11;
        }
        const auto hasDeviceExtension = [&](const char* name)
        {
            for(const auto& entry : availableDeviceExtensions)
                if(std::strcmp(name, entry.extensionName) == 0) return true;
            return false;
        };
        const char* optionalExtensions[] = {
            VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
            VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME,
            VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME,
            VK_EXT_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_EXTENSION_NAME,
            VK_EXT_ATTACHMENT_FEEDBACK_LOOP_LAYOUT_EXTENSION_NAME};
        for(const char* name : optionalExtensions)
        {
            if(!hasDeviceExtension(name))
            {
                std::cerr << "--gl-import-probe missing Vulkan device extension " << name << '\n';
                vkDestroyInstance(vkInstance, nullptr); xrDestroyInstance(xrInstance); return 11;
            }
            bool alreadyEnabled = false;
            for(const std::string& enabledName : deviceNamesOwned)
                alreadyEnabled |= enabledName == name;
            if(!alreadyEnabled) deviceNamesOwned.emplace_back(name);
        }
        deviceNames.clear();
        for(const std::string& name : deviceNamesOwned) deviceNames.push_back(name.c_str());
        multisampleEnable.pNext = &feedbackEnable;
        VkPhysicalDeviceFeatures2 supportedFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
        supportedFeatures.pNext = &multisampleEnable;
        vkGetPhysicalDeviceFeatures2(physicalDevice, &supportedFeatures);
        if(multisampleEnable.multisampledRenderToSingleSampled != VK_TRUE ||
            feedbackEnable.attachmentFeedbackLoopLayout != VK_TRUE)
        {
            std::cerr << "--gl-import-probe required image features unavailable\n";
            vkDestroyInstance(vkInstance, nullptr); xrDestroyInstance(xrInstance); return 11;
        }
#endif
    }
    float priority = 1.f;
    VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queueInfo.queueFamilyIndex = queueFamily; queueInfo.queueCount = 1; queueInfo.pQueuePriorities = &priority;
    VkDeviceCreateInfo vkDeviceInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    vkDeviceInfo.queueCreateInfoCount = 1; vkDeviceInfo.pQueueCreateInfos = &queueInfo;
    vkDeviceInfo.enabledExtensionCount = static_cast<uint32_t>(deviceNames.size());
    vkDeviceInfo.ppEnabledExtensionNames = deviceNames.data();
    if(glImportProbeRequested)
    {
        multisampleEnable.multisampledRenderToSingleSampled = VK_TRUE;
        multisampleEnable.pNext = &feedbackEnable;
        feedbackEnable.attachmentFeedbackLoopLayout = VK_TRUE;
        vkDeviceInfo.pNext = &multisampleEnable;
    }
    XrVulkanDeviceCreateInfoKHR xrVkDeviceInfo{XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR};
    xrVkDeviceInfo.systemId = system; xrVkDeviceInfo.pfnGetInstanceProcAddr = vkGetInstanceProcAddr;
    xrVkDeviceInfo.vulkanPhysicalDevice = physicalDevice; xrVkDeviceInfo.vulkanCreateInfo = &vkDeviceInfo;
    VkDevice device = VK_NULL_HANDLE; vkResult = VK_SUCCESS;
    result = createVulkanDevice(xrInstance, &xrVkDeviceInfo, &device, &vkResult);
    if(XR_FAILED(result) || vkResult != VK_SUCCESS)
    {
        std::cerr << "xrCreateVulkanDevice=" << result << " VkResult=" << vkResult << '\n';
        vkDestroyInstance(vkInstance, nullptr); xrDestroyInstance(xrInstance); return 12;
    }

    XrGraphicsBindingVulkan2KHR binding{XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR};
    binding.instance = vkInstance; binding.physicalDevice = physicalDevice; binding.device = device;
    binding.queueFamilyIndex = queueFamily; binding.queueIndex = 0;
    XrSessionCreateInfo sessionInfo{XR_TYPE_SESSION_CREATE_INFO};
    sessionInfo.next = &binding; sessionInfo.systemId = system;
    XrSession session = XR_NULL_HANDLE;
    result = xrCreateSession(xrInstance, &sessionInfo, &session);
    if(XR_FAILED(result))
    {
        std::cerr << "xrCreateSession=" << result << '\n';
        vkDestroyDevice(device, nullptr); vkDestroyInstance(vkInstance, nullptr); xrDestroyInstance(xrInstance); return 13;
    }
    count = 0;
    result = xrEnumerateSwapchainFormats(session, 0, &count, nullptr);
    std::vector<int64_t> formats(count);
    if(XR_SUCCEEDED(result)) result = xrEnumerateSwapchainFormats(session, count, &count, formats.data());
    if(XR_FAILED(result))
    {
        std::cerr << "xrEnumerateSwapchainFormats=" << result << '\n';
        xrDestroySession(session); vkDeviceWaitIdle(device); vkDestroyDevice(device, nullptr);
        vkDestroyInstance(vkInstance, nullptr); xrDestroyInstance(xrInstance); return 14;
    }

    uint32_t viewCount = 0;
    result = xrEnumerateViewConfigurationViews(xrInstance, system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &viewCount, nullptr);
    if(XR_FAILED(result) || viewCount != 2)
    {
        std::cerr << "stereo view count=" << viewCount << " result=" << result << '\n';
        xrDestroySession(session); vkDeviceWaitIdle(device); vkDestroyDevice(device, nullptr);
        vkDestroyInstance(vkInstance, nullptr); xrDestroyInstance(xrInstance); return 14;
    }
    std::vector<XrViewConfigurationView> views(viewCount, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
    std::vector<XrFrameSynthesisConfigViewEXT> synthesisConfig(viewCount);
    for(uint32_t eye = 0; eye < viewCount; ++eye)
    {
        synthesisConfig[eye].type = XR_TYPE_FRAME_SYNTHESIS_CONFIG_VIEW_EXT;
        views[eye].next = &synthesisConfig[eye];
    }
    result = xrEnumerateViewConfigurationViews(xrInstance, system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
        viewCount, &viewCount, views.data());
    if(XR_FAILED(result) || viewCount != 2)
    {
        std::cerr << "stereo view details=" << result << " count=" << viewCount << '\n';
        xrDestroySession(session); vkDeviceWaitIdle(device); vkDestroyDevice(device, nullptr);
        vkDestroyInstance(vkInstance, nullptr); xrDestroyInstance(xrInstance); return 14;
    }
    uint32_t blendCount = 0;
    result = xrEnumerateEnvironmentBlendModes(xrInstance, system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
        0, &blendCount, nullptr);
    std::vector<XrEnvironmentBlendMode> blendModes(blendCount);
    if(XR_SUCCEEDED(result) && blendCount)
        result = xrEnumerateEnvironmentBlendModes(xrInstance, system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
            blendCount, &blendCount, blendModes.data());
    if(XR_FAILED(result) || blendModes.empty())
    {
        std::cerr << "environment blend mode enumeration=" << result << '\n';
        xrDestroySession(session); vkDeviceWaitIdle(device); vkDestroyDevice(device, nullptr);
        vkDestroyInstance(vkInstance, nullptr); xrDestroyInstance(xrInstance); return 14;
    }
    const XrEnvironmentBlendMode blendMode =
        std::find(blendModes.begin(), blendModes.end(), XR_ENVIRONMENT_BLEND_MODE_OPAQUE) != blendModes.end()
            ? XR_ENVIRONMENT_BLEND_MODE_OPAQUE : blendModes.front();
    std::cout << "environmentBlendMode=" << blendMode << '\n';
    const auto offered = [&](VkFormat format)
    {
        return std::find(formats.begin(), formats.end(), static_cast<int64_t>(format)) != formats.end();
    };
    VkFormat colorFormat = VK_FORMAT_UNDEFINED;
    for(VkFormat candidate : {VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_B8G8R8A8_SRGB,
            VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8A8_UNORM})
        if(offered(candidate)) { colorFormat = candidate; break; }
    if(colorFormat == VK_FORMAT_UNDEFINED || !offered(VK_FORMAT_R16G16B16A16_SFLOAT) ||
        !offered(VK_FORMAT_D32_SFLOAT))
    {
        std::cerr << "required color, FP16 motion, or D32 depth format unavailable\n";
        xrDestroySession(session); vkDestroyDevice(device, nullptr);
        vkDestroyInstance(vkInstance, nullptr); xrDestroyInstance(xrInstance); return 15;
    }
    const VkFormat probeColorFormat = offered(VK_FORMAT_R8G8B8A8_UNORM)
        ? VK_FORMAT_R8G8B8A8_UNORM : colorFormat;
    VkQueue queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(device, queueFamily, 0, &queue);
    std::array<std::array<ImageSet, 3>, 2> targets{}; // color, motion, depth per eye
    Readback readback{};
    XrSpace space = XR_NULL_HANDLE;
    VkCommandPool pool = VK_NULL_HANDLE;
    std::array<VkCommandBuffer, 3> commands{};
    std::array<VkFence, 3> fences{};
    qvr::bsp::vulkan::GpuMesh gpuBspMesh{};
    ScenePipelines scenePipelines{};
    auto cleanup = [&]
    {
        vkDeviceWaitIdle(device);
        for(auto& eye : targets) for(ImageSet& set : eye)
            if(set.hasAcquired)
            {
                if(set.waited) release(set);
                else std::cerr << "leaving an acquired, unwaited image for session destruction\n";
        }
        for(VkFence fence : fences) if(fence) vkDestroyFence(device, fence, nullptr);
        destroyScenePipelines(scenePipelines);
        qvr::bsp::vulkan::destroy(gpuBspMesh);
        if(pool) vkDestroyCommandPool(device, pool, nullptr);
        if(space) xrDestroySpace(space);
        for(auto& eye : targets) for(ImageSet& set : eye) destroyImageSet(device, set);
        if(readback.mapped) vkUnmapMemory(device, readback.memory);
        if(readback.buffer) vkDestroyBuffer(device, readback.buffer, nullptr);
        if(readback.memory) vkFreeMemory(device, readback.memory, nullptr);
        xrDestroySession(session);
        vkDestroyDevice(device, nullptr);
        vkDestroyInstance(vkInstance, nullptr);
        xrDestroyInstance(xrInstance);
    };
    bool setup = true;
    for(uint32_t eye = 0; eye < 2 && setup; ++eye)
    {
        const uint32_t colorWidth = views[eye].recommendedImageRectWidth;
        const uint32_t colorHeight = views[eye].recommendedImageRectHeight;
        const uint32_t motionWidth = synthesisConfig[eye].recommendedMotionVectorImageRectWidth;
        const uint32_t motionHeight = synthesisConfig[eye].recommendedMotionVectorImageRectHeight;
        if(!colorWidth || !colorHeight || !motionWidth || !motionHeight) { setup = false; break; }
        const XrSwapchainUsageFlags transferSource = readbackRequested ? XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT : 0;
        const XrSwapchainUsageFlags transferDestination = transferDestinationRequested ? XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT : 0;
        setup = makeImageSet(session, device, targets[eye][0], colorFormat,
            XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT | transferSource | transferDestination,
            VK_IMAGE_ASPECT_COLOR_BIT, colorWidth, colorHeight);
        if(setup) setup = makeImageSet(session, device, targets[eye][1], VK_FORMAT_R16G16B16A16_SFLOAT,
            XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT | transferSource | transferDestination,
            VK_IMAGE_ASPECT_COLOR_BIT, motionWidth, motionHeight);
        if(setup) setup = makeImageSet(session, device, targets[eye][2], VK_FORMAT_D32_SFLOAT,
            XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT | transferSource | transferDestination,
            VK_IMAGE_ASPECT_DEPTH_BIT, motionWidth, motionHeight);
    }
    XrReferenceSpaceCreateInfo spaceInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    spaceInfo.poseInReferenceSpace.orientation.w = 1.f;
    if(setup) setup = XR_SUCCEEDED(xrCreateReferenceSpace(session, &spaceInfo, &space));
    VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamily;
    if(setup) setup = vkCreateCommandPool(device, &poolInfo, nullptr, &pool) == VK_SUCCESS;
    if(setup) setup = makeScenePipelines(device, targets[0][0].pass, targets[0][1].pass,
        targets[0][2].pass, scenePipelines);
    qvr::bsp::OpaqueWorldUpload uploadMesh;
    if(setup)
    {
        uploadMesh.vertices.pushBack({{-1.f, -1.f, 0.35f}, {0.f, 0.f}});
        uploadMesh.vertices.pushBack({{ 1.f, -1.f, 0.35f}, {1.f, 0.f}});
        uploadMesh.vertices.pushBack({{ 1.f,  1.f, 0.35f}, {1.f, 1.f}});
        uploadMesh.vertices.pushBack({{-1.f,  1.f, 0.35f}, {0.f, 1.f}});
        for(za::U32 index : {0u, 1u, 2u, 0u, 2u, 3u}) uploadMesh.indices.pushBack(index);
        uploadMesh.ranges.pushBack({0, 6, 0, 0});
        VkResult uploadResult = VK_SUCCESS;
        setup = qvr::bsp::vulkan::upload(uploadMesh, physicalDevice, device, queue, pool,
            gpuBspMesh, &uploadResult);
        if(!setup) std::cerr << "BSP mesh upload failed: VkResult=" << uploadResult << '\n';
        else std::cout << "bspUpload vertices=" << uploadMesh.vertices.size() << " indices="
            << uploadMesh.indices.size() << " ranges=" << gpuBspMesh.ranges.size() << '\n';
    }
    VkCommandBufferAllocateInfo commandInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    commandInfo.commandPool = pool;
    commandInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandInfo.commandBufferCount = static_cast<uint32_t>(commands.size());
    if(setup) setup = vkAllocateCommandBuffers(device, &commandInfo, commands.data()) == VK_SUCCESS;
    for(VkFence& fence : fences)
    {
        if(!setup) break;
        VkFenceCreateInfo info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        setup = vkCreateFence(device, &info, nullptr, &fence) == VK_SUCCESS;
    }
    if(setup && readbackRequested) setup = makeReadback(physicalDevice, device, readback);
    if(!setup)
    {
        std::cerr << "XR/Vulkan image or command setup failed\n";
        cleanup();
        return 16;
    }
    if(readbackRequested)
        std::cout << "readback=enabled (single final frame, 1x1 samples)\n";
    std::cout << "targets colorFormat=" << colorFormat << " color=" << targets[0][0].width << 'x'
        << targets[0][0].height << " motionDepth=" << targets[0][1].width << 'x'
        << targets[0][1].height << " transferSrc=" << readbackRequested
        << " transferDst=" << transferDestinationRequested << std::endl;

    bool running = false, failed = false, exiting = false, glImportProbeComplete = false;
    bool motionHistoryValid = false, readbackSubmitted = false;
    std::array<std::array<float, 2>, 2> previousCenters{};
    XrTime animationStartTime = 0;
    uint32_t submittedFrames = 0, synthesisFrames = 0, nonzeroMotionFrames = 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(45);
    while(!failed && !exiting && submittedFrames < frameLimit && std::chrono::steady_clock::now() < deadline)
    {
        XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
        XrResult eventResult = XR_SUCCESS;
        while((eventResult = xrPollEvent(xrInstance, &event)) == XR_SUCCESS)
        {
            if(event.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED)
            {
                const auto* changed = reinterpret_cast<const XrEventDataSessionStateChanged*>(&event);
                std::cout << "sessionState=" << changed->state << std::endl;
                if(changed->state == XR_SESSION_STATE_READY)
                {
                    XrSessionBeginInfo begin{XR_TYPE_SESSION_BEGIN_INFO};
                    begin.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                    running = XR_SUCCEEDED(xrBeginSession(session, &begin));
                    if(!running) failed = true;
#ifndef _WIN32
                    else if(glImportProbeRequested && !glImportProbeComplete)
                    {
                        glImportProbeComplete = true;
                        if(probeColorFormat == VK_FORMAT_UNDEFINED ||
                            !probeOpenGlImportOnOpenXrDevice(xrInstance, session, physicalDevice,
                                device, queueFamily, probeColorFormat))
                        {
                            std::cerr << "--gl-import-probe failed\n";
                            xrEndSession(session);
                            running = false;
                            failed = true;
                        }
                    }
#endif
                }
                else if(changed->state == XR_SESSION_STATE_STOPPING)
                {
                    if(running) xrEndSession(session);
                    running = false;
                    exiting = true;
                }
                else if(changed->state == XR_SESSION_STATE_EXITING || changed->state == XR_SESSION_STATE_LOSS_PENDING)
                    exiting = true;
            }
            event = {XR_TYPE_EVENT_DATA_BUFFER};
        }
        if(XR_FAILED(eventResult)) { failed = true; break; }
        if(!running) { std::this_thread::sleep_for(std::chrono::milliseconds(10)); continue; }

        XrFrameWaitInfo waitFrame{XR_TYPE_FRAME_WAIT_INFO};
        XrFrameState frame{XR_TYPE_FRAME_STATE};
        if(XR_FAILED(xrWaitFrame(session, &waitFrame, &frame))) { failed = true; break; }
        XrFrameBeginInfo beginFrame{XR_TYPE_FRAME_BEGIN_INFO};
        if(XR_FAILED(xrBeginFrame(session, &beginFrame))) { failed = true; break; }
        std::array<XrCompositionLayerProjectionView, 2> projectionViews{};
        std::array<XrFrameSynthesisInfoEXT, 2> synthesisInfo{};
        std::array<EyePattern, 2> patterns{};
        std::array<std::array<float, 2>, 2> pendingCenters{};
        bool render = frame.shouldRender == XR_TRUE;
        if(render)
        {
            XrViewLocateInfo locateInfo{XR_TYPE_VIEW_LOCATE_INFO};
            locateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
            locateInfo.displayTime = frame.predictedDisplayTime;
            locateInfo.space = space;
            XrViewState viewState{XR_TYPE_VIEW_STATE};
            std::array<XrView, 2> eyeViews{{{XR_TYPE_VIEW}, {XR_TYPE_VIEW}}};
            uint32_t located = 0;
            render = XR_SUCCEEDED(xrLocateViews(session, &locateInfo, &viewState,
                static_cast<uint32_t>(eyeViews.size()), &located, eyeViews.data())) && located == 2 &&
                (viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) &&
                (viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT);
            if(render)
            {
                for(auto& eye : targets)
                {
                    for(ImageSet& set : eye)
                        if(!acquire(set)) { render = false; failed = true; break; }
                    if(!render) break;
                }
            }
            if(render)
            {
                const uint32_t slot = submittedFrames % static_cast<uint32_t>(commands.size());
                render = vkWaitForFences(device, 1, &fences[slot], VK_TRUE, UINT64_MAX) == VK_SUCCESS &&
                    vkResetFences(device, 1, &fences[slot]) == VK_SUCCESS &&
                    vkResetCommandBuffer(commands[slot], 0) == VK_SUCCESS;
                VkCommandBufferBeginInfo commandBegin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
                if(render) render = vkBeginCommandBuffer(commands[slot], &commandBegin) == VK_SUCCESS;
                if(render)
                {
                    if(animationStartTime == 0) animationStartTime = frame.predictedDisplayTime;
                    const float timeSeconds = static_cast<float>(
                        static_cast<double>(frame.predictedDisplayTime - animationStartTime) * 1.0e-9);
                    for(uint32_t eye = 0; eye < 2; ++eye)
                    {
                        const float phase = 2.f * 3.14159265358979323846f * 0.35f * timeSeconds
                            + eye * 3.14159265358979323846f;
                        const float centerX = 0.5f + 0.18f * std::sin(phase);
                        const float centerY = 0.5f + 0.12f * std::sin(phase * 0.73f + 0.9f);
                        pendingCenters[eye] = {centerX, centerY};
                        const float oldX = motionHistoryValid ? previousCenters[eye][0] : centerX;
                        const float oldY = motionHistoryValid ? previousCenters[eye][1] : centerY;
                        patterns[eye].color = makePatternRect(targets[eye][0], centerX, centerY);
                        patterns[eye].motion = makePatternRect(targets[eye][1], centerX, centerY);
                        patterns[eye].depth = makePatternRect(targets[eye][2], centerX, centerY);
                        const PatternRect oldMotion = makePatternRect(targets[eye][1], oldX, oldY);
                        patterns[eye].motionNdc = {
                            2.f * static_cast<float>(patterns[eye].motion.x - oldMotion.x) / targets[eye][1].width,
                            2.f * static_cast<float>(patterns[eye].motion.y - oldMotion.y) / targets[eye][1].height};

                        VkClearValue colorBackground{}, colorForeground{}, motionBackground{}, motionForeground{},
                            depthBackground{}, depthForeground{};
                        colorBackground.color = eye ? VkClearColorValue{{0.05f, 0.2f, 0.5f, 1.f}}
                            : VkClearColorValue{{0.5f, 0.2f, 0.05f, 1.f}};
                        colorForeground.color = eye ? VkClearColorValue{{0.1f, 1.f, 0.1f, 1.f}}
                            : VkClearColorValue{{1.f, 0.9f, 0.05f, 1.f}};
                        motionBackground.color = {{0.f, 0.f, 0.f, 0.f}};
                        motionForeground.color = {{patterns[eye].motionNdc.x, patterns[eye].motionNdc.y, 0.f, 0.f}};
                        depthBackground.depthStencil = {1.f, 0};
                        depthForeground.depthStencil = {0.35f, 0};
                        recordScene(commands[slot], targets[eye][0], scenePipelines.color,
                            scenePipelines.layout, gpuBspMesh, colorBackground, patterns[eye].color, colorForeground);
                        recordScene(commands[slot], targets[eye][1], scenePipelines.motion,
                            scenePipelines.layout, gpuBspMesh, motionBackground, patterns[eye].motion, motionForeground);
                        recordScene(commands[slot], targets[eye][2], scenePipelines.depth,
                            scenePipelines.layout, gpuBspMesh, depthBackground, patterns[eye].depth, depthForeground);
                        if(readbackRequested && submittedFrames + 1 == frameLimit)
                        {
                            const VkDeviceSize base = eye * 40;
                            recordCopyPixel(commands[slot], targets[eye][0], readback.buffer, base,
                                patterns[eye].color.x + patterns[eye].color.width / 2,
                                patterns[eye].color.y + patterns[eye].color.height / 2);
                            recordCopyPixel(commands[slot], targets[eye][1], readback.buffer, base + 8,
                                patterns[eye].motion.x + patterns[eye].motion.width / 2,
                                patterns[eye].motion.y + patterns[eye].motion.height / 2);
                            recordCopyPixel(commands[slot], targets[eye][2], readback.buffer, base + 16,
                                patterns[eye].depth.x + patterns[eye].depth.width / 2,
                                patterns[eye].depth.y + patterns[eye].depth.height / 2);
                            recordCopyPixel(commands[slot], targets[eye][1], readback.buffer, base + 24, 0, 0);
                            recordCopyPixel(commands[slot], targets[eye][2], readback.buffer, base + 32, 0, 0);
                        }
                    }
                    render = vkEndCommandBuffer(commands[slot]) == VK_SUCCESS;
                }
                VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
                submit.commandBufferCount = 1;
                submit.pCommandBuffers = &commands[slot];
                if(render) render = vkQueueSubmit(queue, 1, &submit, fences[slot]) == VK_SUCCESS;
                if(render && readbackRequested && submittedFrames + 1 == frameLimit) readbackSubmitted = true;
                if(!render) failed = true;
                if(render)
                {
                    for(uint32_t eye = 0; eye < 2; ++eye)
                    {
                        const ImageSet& color = targets[eye][0];
                        const ImageSet& motion = targets[eye][1];
                        const ImageSet& depth = targets[eye][2];
                        projectionViews[eye].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
                        projectionViews[eye].pose = eyeViews[eye].pose;
                        projectionViews[eye].fov = eyeViews[eye].fov;
                        projectionViews[eye].subImage = {color.swapchain, {{0, 0},
                            {static_cast<int32_t>(color.width), static_cast<int32_t>(color.height)}}, 0};
                        XrPosef identity{};
                        identity.orientation.w = 1.f;
                        synthesisInfo[eye] = qvr::xr::frameSynthesisInfo(motion.swapchain, depth.swapchain,
                            static_cast<int>(motion.width), static_cast<int>(motion.height),
                            0.1f, 100.f, identity, false);
                        projectionViews[eye].next = &synthesisInfo[eye];
                    }
                    // Vulkan2 permits submitted writes still executing at release. They must be
                    // ordered on the queue named in the OpenXR graphics binding.
                    for(auto& eye : targets) for(ImageSet& set : eye)
                        if(!release(set)) { render = false; failed = true; }
                }
            }
        }
        // A failed acquire/record/submit can leave earlier images waited. Return
        // every such image before ending this frame; an unwaited image must not
        // be released and remains tracked until session destruction.
        if(!render)
            for(auto& eye : targets) for(ImageSet& set : eye)
                if(set.hasAcquired && set.waited && !release(set)) failed = true;
        XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
        layer.space = space;
        layer.viewCount = render ? 2u : 0u;
        layer.views = render ? projectionViews.data() : nullptr;
        const XrCompositionLayerBaseHeader* layers[] = {
            reinterpret_cast<const XrCompositionLayerBaseHeader*>(&layer)};
        XrFrameEndInfo end{XR_TYPE_FRAME_END_INFO};
        end.displayTime = frame.predictedDisplayTime;
        end.environmentBlendMode = blendMode;
        end.layerCount = render ? 1u : 0u;
        end.layers = render ? layers : nullptr;
        const XrResult endResult = xrEndFrame(session, &end);
        if(XR_FAILED(endResult)) { std::cerr << "xrEndFrame=" << endResult << std::endl; failed = true; }
        if(XR_SUCCEEDED(endResult) && render)
        {
            previousCenters = pendingCenters;
            motionHistoryValid = true;
            if(std::abs(patterns[0].motionNdc.x) + std::abs(patterns[0].motionNdc.y) > 1.0e-6f ||
                std::abs(patterns[1].motionNdc.x) + std::abs(patterns[1].motionNdc.y) > 1.0e-6f)
                ++nonzeroMotionFrames;
        }
        else
            motionHistoryValid = false;
        ++submittedFrames;
        if(render) ++synthesisFrames;
        if(submittedFrames % 60 == 0 || submittedFrames == frameLimit)
            std::cout << "frames=" << submittedFrames << " synthesis=" << synthesisFrames << std::endl;
    }
    std::cout << "completed frames=" << submittedFrames << " synthesis=" << synthesisFrames
        << " nonzeroMotion=" << nonzeroMotionFrames << " failed=" << failed << std::endl;
    if(readbackSubmitted && vkDeviceWaitIdle(device) == VK_SUCCESS &&
        vkMapMemory(device, readback.memory, 0, VK_WHOLE_SIZE, 0, &readback.mapped) == VK_SUCCESS)
        printReadback(readback);
    cleanup();
    return failed || !synthesisFrames ? 17 : 0;
}
