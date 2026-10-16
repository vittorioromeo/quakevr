#include "vr_gl_vulkan_bridge.hpp"
#ifdef QVR_HAVE_GL_VULKAN_BRIDGE
#include "vr_gl_vulkan_bridge_shaders.hpp"
#ifndef __linux__
#error The validated GL/Vulkan bridge requires Linux OPAQUE_FD
#endif
#define GL_GLEXT_PROTOTYPES
#include <SDL.h>
#include <SDL_opengl.h>
#include <GL/glext.h> // FD interop declarations may postdate SDL's bundled GL headers.
#include <unistd.h>
#include <array>
#include <cstdio>
#include <cstring>
#include <new>
#include <vector>

namespace qvr::gl_vulkan
{
Destination destinationFromSwapchainRequest(VkImage image, const XrSwapchainCreateInfo& request, XrResult result)
{
    Destination d{};
    if(result != XR_SUCCESS || !image || request.type != XR_TYPE_SWAPCHAIN_CREATE_INFO ||
        (request.createFlags & XR_SWAPCHAIN_CREATE_PROTECTED_CONTENT_BIT) ||
        !request.width || !request.height || !request.sampleCount || request.faceCount != 1 ||
        request.arraySize != 1 || request.mipCount != 1) return d;
    // XR_KHR_vulkan_enable swapchain-flags: returned images may be used as
    // if created with at least each corresponding requested Vulkan usage bit.
    // Vulkan2 shares the same swapchain image/usage contract.
    if(request.usageFlags & XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT) d.guaranteedUsage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if(request.usageFlags & XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) d.guaranteedUsage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if(request.usageFlags & XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT) d.guaranteedUsage |= VK_IMAGE_USAGE_STORAGE_BIT;
    if(request.usageFlags & XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT) d.guaranteedUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if(request.usageFlags & XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT) d.guaranteedUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if(request.usageFlags & XR_SWAPCHAIN_USAGE_SAMPLED_BIT) d.guaranteedUsage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if(request.usageFlags & XR_SWAPCHAIN_USAGE_INPUT_ATTACHMENT_BIT_MND) d.guaranteedUsage |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    d.image = image;
    d.format = static_cast<VkFormat>(request.format);
    d.width = request.width;
    d.height = request.height;
    d.samples = static_cast<VkSampleCountFlagBits>(request.sampleCount);
    d.layout = d.format == VK_FORMAT_D32_SFLOAT ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    return d;
}
bool sameSubmissionQueue(const DeviceContext& a, const DeviceContext& b)
{
    return a.device && a.queue && a.device == b.device && a.queue == b.queue &&
        a.instance == b.instance && a.physicalDevice == b.physicalDevice &&
        a.queueFamily == b.queueFamily && a.queueIndex == b.queueIndex;
}
bool acceptsDestination(Format format, Operation operation, unsigned width, unsigned height, const Destination& d)
{
    const bool depth = format == Format::DepthD32F;
    const VkFormat expected = depth ? VK_FORMAT_D32_SFLOAT : format == Format::MotionRGBA16F ?
        VK_FORMAT_R16G16B16A16_SFLOAT : VK_FORMAT_R8G8B8A8_UNORM;
    const bool compatible = d.format == expected || (format == Format::SrgbColorBytes && d.format == VK_FORMAT_R8G8B8A8_SRGB);
    const VkImageUsageFlags attachment = depth ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if(!width || !height || !d.image || !d.acquiredAndWaited || !compatible ||
        d.width != width || d.height != height || d.samples != VK_SAMPLE_COUNT_1_BIT || !(d.guaranteedUsage & attachment) ||
        d.layout != (depth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)) return false;
    if(depth) return operation == Operation::DepthAttachment;
    return operation == Operation::TransferCopy && (d.guaranteedUsage & VK_IMAGE_USAGE_TRANSFER_DST_BIT);
}

struct Bridge::State
{
    DeviceContext context;
    Format format{Format::SrgbColorBytes};
    unsigned width{}, height{}, texture{}, memoryObject{};
    GLenum internalFormat{};
    VkFormat nativeFormat{};
    VkImageAspectFlags aspect{};
    VkImage image{};
    VkDeviceMemory memory{};
    VkCommandPool pool{};
    VkCommandBuffer command{};
    VkFence fence{};
    std::array<VkSemaphore, 2> semaphores{}; // GL->VK, VK->GL
    std::array<GLuint, 2> glSemaphores{};
    int fd{-1};
    bool ready{}, glWriting{}, pending{}, poisoned{};
    SDL_GLContext owner{};
    GLsync cleanupSync{};
    PFN_vkCmdPipelineBarrier2KHR barrier2{};
    VkImageView sourceView{}, destinationView{};
    VkImage attachmentImage{};
    VkSampler sampler{};
    VkDescriptorSetLayout descriptorLayout{};
    VkDescriptorPool descriptorPool{};
    VkDescriptorSet descriptor{};
    VkPipelineLayout pipelineLayout{};
    VkRenderPass pass{};
    VkPipeline pipeline{};
    VkFramebuffer framebuffer{};
    VkShaderModule vertexShader{}, fragmentShader{};
    char message[256]{};
    PFNGLDELETEMEMORYOBJECTSEXTPROC deleteMemory{};
    PFNGLDELETESEMAPHORESEXTPROC deleteSemaphores{};
    PFNGLWAITSEMAPHOREEXTPROC wait{};
    PFNGLSIGNALSEMAPHOREEXTPROC signal{};

    bool fail(const char* text)
    {
        std::snprintf(message, sizeof(message), "%s", text);
        poisoned = true;
        return false;
    }
    bool checked(VkResult result, const char* text)
    {
        if(result == VK_SUCCESS) return true;
        std::snprintf(message, sizeof(message), "%s (VkResult=%d)", text, result);
        poisoned = true;
        return false;
    }
    bool glChecked(const char* text)
    {
        const auto result = glGetError();
        if(result == GL_NO_ERROR) return true;
        std::snprintf(message, sizeof(message), "%s (GLerror=0x%x)", text, result);
        poisoned = true;
        return false;
    }
    template<class T> T proc(const char* name)
    {
        auto result = reinterpret_cast<T>(SDL_GL_GetProcAddress(name));
        if(!result) fail(name);
        return result;
    }
    void barrier(VkImage target, VkImageLayout oldLayout, VkImageLayout newLayout,
        unsigned sourceFamily, unsigned destinationFamily, VkAccessFlags sourceAccess,
        VkAccessFlags destinationAccess, VkPipelineStageFlags sourceStage, VkPipelineStageFlags destinationStage)
    {
        if(format == Format::DepthD32F) {
            // Equal layouts in synchronization2 do not define a transition.
            // Preserve shared D32 in GENERAL and runtime D32 in its attachment
            // layout, without inferring native XR image creation flags.
            VkImageMemoryBarrier2 b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
            b.image = target; b.subresourceRange = {aspect, 0, 1, 0, 1};
            b.oldLayout = oldLayout; b.newLayout = newLayout;
            b.srcQueueFamilyIndex = sourceFamily; b.dstQueueFamilyIndex = destinationFamily;
            b.srcAccessMask = sourceAccess; b.dstAccessMask = destinationAccess;
            b.srcStageMask = sourceStage; b.dstStageMask = destinationStage;
            VkDependencyInfo d{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
            d.imageMemoryBarrierCount = 1; d.pImageMemoryBarriers = &b;
            barrier2(command, &d);
            return;
        }
        VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        b.image = target; b.subresourceRange = {aspect, 0, 1, 0, 1};
        b.oldLayout = oldLayout; b.newLayout = newLayout;
        b.srcQueueFamilyIndex = sourceFamily; b.dstQueueFamilyIndex = destinationFamily;
        b.srcAccessMask = sourceAccess; b.dstAccessMask = destinationAccess;
        vkCmdPipelineBarrier(command, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &b);
    }
    bool begin()
    {
        if(pending) return fail("cannot reset commands while a bridge submission is pending");
        if(!checked(vkResetCommandBuffer(command, 0), "reset commands")) return false;
        VkCommandBufferBeginInfo b{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        b.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        return checked(vkBeginCommandBuffer(command, &b), "begin commands");
    }
    bool complete()
    {
        if(!pending) return true;
        if(!checked(vkWaitForFences(context.device, 1, &fence, VK_TRUE, 5'000'000'000ULL), "bridge fence timeout/error")) return false;
        pending = false;
        return true;
    }
    bool submit(bool waitForGl, bool completeNow = true)
    {
        if(pending) return fail("cannot reset/reuse a pending bridge fence or submission");
        if(!checked(vkEndCommandBuffer(command), "end commands") ||
            !checked(vkResetFences(context.device, 1, &fence), "reset fence")) return false;
        const VkPipelineStageFlags stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        VkSubmitInfo s{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        s.commandBufferCount = 1; s.pCommandBuffers = &command;
        s.waitSemaphoreCount = waitForGl ? 1 : 0; s.pWaitSemaphores = semaphores.data(); s.pWaitDstStageMask = &stage;
        s.signalSemaphoreCount = 1; s.pSignalSemaphores = &semaphores[1];
        if(!checked(vkQueueSubmit(context.queue, 1, &s, fence), "submit bridge copy")) return false;
        pending = true;
        return !completeNow || complete();
    }
    bool depthTarget(const Destination& d)
    {
        if(!pipeline) {
            VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            view.image = image; view.viewType = VK_IMAGE_VIEW_TYPE_2D; view.format = VK_FORMAT_D32_SFLOAT;
            view.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
            if(!checked(vkCreateImageView(context.device, &view, nullptr, &sourceView), "staged depth view")) return false;
            VkSamplerCreateInfo sample{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
            sample.magFilter = sample.minFilter = VK_FILTER_NEAREST; sample.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            sample.addressModeU = sample.addressModeV = sample.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            if(!checked(vkCreateSampler(context.device, &sample, nullptr, &sampler), "depth sampler")) return false;
            VkDescriptorSetLayoutBinding binding{}; binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            binding.descriptorCount = 1; binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            VkDescriptorSetLayoutCreateInfo dl{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO}; dl.bindingCount = 1; dl.pBindings = &binding;
            if(!checked(vkCreateDescriptorSetLayout(context.device, &dl, nullptr, &descriptorLayout), "depth descriptor layout")) return false;
            VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1};
            VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO}; poolInfo.maxSets = 1; poolInfo.poolSizeCount = 1; poolInfo.pPoolSizes = &size;
            if(!checked(vkCreateDescriptorPool(context.device, &poolInfo, nullptr, &descriptorPool), "depth descriptor pool")) return false;
            VkDescriptorSetAllocateInfo alloc{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO}; alloc.descriptorPool = descriptorPool; alloc.descriptorSetCount = 1; alloc.pSetLayouts = &descriptorLayout;
            if(!checked(vkAllocateDescriptorSets(context.device, &alloc, &descriptor), "depth descriptor")) return false;
            VkDescriptorImageInfo imageInfo{sampler, sourceView, VK_IMAGE_LAYOUT_GENERAL};
            VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}; write.dstSet = descriptor; write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; write.descriptorCount = 1; write.pImageInfo = &imageInfo;
            vkUpdateDescriptorSets(context.device, 1, &write, 0, nullptr);
            VkPipelineLayoutCreateInfo layout{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO}; layout.setLayoutCount = 1; layout.pSetLayouts = &descriptorLayout;
            if(!checked(vkCreatePipelineLayout(context.device, &layout, nullptr, &pipelineLayout), "depth pipeline layout")) return false;
            VkAttachmentDescription attachment{}; attachment.format = VK_FORMAT_D32_SFLOAT; attachment.samples = VK_SAMPLE_COUNT_1_BIT;
            attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachment.initialLayout = attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            VkAttachmentReference reference{0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
            VkSubpassDescription sub{}; sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; sub.pDepthStencilAttachment = &reference;
            VkRenderPassCreateInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO}; rp.attachmentCount = 1; rp.pAttachments = &attachment; rp.subpassCount = 1; rp.pSubpasses = &sub;
            if(!checked(vkCreateRenderPass(context.device, &rp, nullptr, &pass), "depth render pass")) return false;
            VkShaderModuleCreateInfo shader{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
            shader.codeSize = sizeof(shaders::vert); shader.pCode = shaders::vert;
            if(!checked(vkCreateShaderModule(context.device, &shader, nullptr, &vertexShader), "depth vertex shader")) return false;
            shader.codeSize = sizeof(shaders::frag); shader.pCode = shaders::frag;
            if(!checked(vkCreateShaderModule(context.device, &shader, nullptr, &fragmentShader), "depth fragment shader")) return false;
            VkPipelineShaderStageCreateInfo stages[2]{};
            for(auto& stage : stages) { stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; stage.pName = "main"; }
            stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT; stages[0].module = vertexShader;
            stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; stages[1].module = fragmentShader;
            VkPipelineVertexInputStateCreateInfo vertex{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
            VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO}; assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO}; viewport.viewportCount = viewport.scissorCount = 1;
            VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO}; raster.polygonMode = VK_POLYGON_MODE_FILL; raster.cullMode = VK_CULL_MODE_NONE; raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; raster.lineWidth = 1.f;
            VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO}; ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
            VkPipelineDepthStencilStateCreateInfo depth{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO}; depth.depthTestEnable = depth.depthWriteEnable = VK_TRUE; depth.depthCompareOp = VK_COMPARE_OP_ALWAYS;
            VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
            const VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
            VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO}; dynamic.dynamicStateCount = 2; dynamic.pDynamicStates = dynamicStates;
            VkGraphicsPipelineCreateInfo pi{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO}; pi.stageCount = 2; pi.pStages = stages;
            pi.pVertexInputState = &vertex; pi.pInputAssemblyState = &assembly; pi.pViewportState = &viewport; pi.pRasterizationState = &raster;
            pi.pMultisampleState = &ms; pi.pDepthStencilState = &depth; pi.pColorBlendState = &blend; pi.pDynamicState = &dynamic;
            pi.layout = pipelineLayout; pi.renderPass = pass;
            if(!checked(vkCreateGraphicsPipelines(context.device, VK_NULL_HANDLE, 1, &pi, nullptr, &pipeline), "depth graphics pipeline")) return false;
        }
        if(attachmentImage != d.image) {
            // Previous submission completed at its bounded fence before reuse.
            if(framebuffer) vkDestroyFramebuffer(context.device, framebuffer, nullptr);
            if(destinationView) vkDestroyImageView(context.device, destinationView, nullptr);
            framebuffer = VK_NULL_HANDLE; destinationView = VK_NULL_HANDLE; attachmentImage = VK_NULL_HANDLE;
            VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO}; view.image = d.image;
            view.viewType = VK_IMAGE_VIEW_TYPE_2D; view.format = VK_FORMAT_D32_SFLOAT; view.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
            if(!checked(vkCreateImageView(context.device, &view, nullptr, &destinationView), "XR depth attachment view")) return false;
            VkFramebufferCreateInfo fb{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO}; fb.renderPass = pass; fb.attachmentCount = 1; fb.pAttachments = &destinationView;
            fb.width = width; fb.height = height; fb.layers = 1;
            if(!checked(vkCreateFramebuffer(context.device, &fb, nullptr, &framebuffer), "XR depth framebuffer")) return false;
            attachmentImage = d.image;
        }
        return true;
    }
    void drawDepth()
    {
        VkRenderPassBeginInfo b{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO}; b.renderPass = pass; b.framebuffer = framebuffer; b.renderArea.extent = {width, height};
        vkCmdBeginRenderPass(command, &b, VK_SUBPASS_CONTENTS_INLINE);
        VkViewport viewport{0.f, 0.f, static_cast<float>(width), static_cast<float>(height), 0.f, 1.f};
        VkRect2D scissor{{0, 0}, {width, height}};
        vkCmdSetViewport(command, 0, 1, &viewport); vkCmdSetScissor(command, 0, 1, &scissor);
        vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptor, 0, nullptr);
        vkCmdDraw(command, 3, 1, 0, 0); vkCmdEndRenderPass(command);
    }
};

Bridge::Bridge() : state(new(std::nothrow) State) {}
Bridge::~Bridge()
{
    if(!shutdown()) {
        // A timed-out queue cannot be safely torn down by deleting its objects.
        // Keep the failure closed and leave native resources to device teardown.
        if(state) std::fprintf(stderr, "GL/Vulkan bridge cleanup retained resources: %s\n", state->message);
    }
}
bool Bridge::healthy() const { return state && state->ready && !state->poisoned && !state->pending; }
bool Bridge::submitted() const { return state && state->ready && !state->poisoned && state->pending && !state->glWriting; }
const char* Bridge::error() const { return state ? state->message : "bridge state allocation failed"; }
unsigned Bridge::glTexture() const { return healthy() ? state->texture : 0; }
unsigned Bridge::glInternalFormat() const { return healthy() ? state->internalFormat : 0; }

bool Bridge::initialize(const DeviceContext& context, Format format, unsigned width, unsigned height)
{
    if(!state) return false;
    auto& s = *state;
    if(s.ready || s.image || s.texture) return s.fail("initialize requires a fresh/shutdown bridge");
    s.context = context; s.format = format; s.width = width; s.height = height;
    if(!context.instance || !context.physicalDevice || !context.device || !context.queue ||
        context.queueFamily == ~0u || context.apiVersion < VK_API_VERSION_1_1 ||
        !context.externalImageFeaturesEnabled || !width || !height || width > 16384 || height > 16384)
        return s.fail("invalid XR-bound context, dimensions or logical-device feature assertion");
    s.owner = SDL_GL_GetCurrentContext();
    if(!s.owner) return s.fail("initialize requires the owning SDL GL context current");
    if(format == Format::DepthD32F && !context.sampleLocationsEnabled)
        return s.fail("D32 image definition requires enabled VK_EXT_sample_locations (no 1x metadata is used)");
    if(format == Format::DepthD32F) {
        if(!context.synchronization2Enabled) return s.fail("D32 preservation requires enabled synchronization2");
        VkPhysicalDeviceSynchronization2Features f{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES};
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2}; features.pNext = &f;
        vkGetPhysicalDeviceFeatures2(context.physicalDevice, &features);
        if(!f.synchronization2) return s.fail("physical device does not support synchronization2");
        s.barrier2 = reinterpret_cast<PFN_vkCmdPipelineBarrier2KHR>(vkGetDeviceProcAddr(context.device, "vkCmdPipelineBarrier2KHR"));
        if(!s.barrier2 && context.apiVersion >= VK_API_VERSION_1_3)
            s.barrier2 = reinterpret_cast<PFN_vkCmdPipelineBarrier2KHR>(vkGetDeviceProcAddr(context.device, "vkCmdPipelineBarrier2"));
        if(!s.barrier2) return s.fail("enabled synchronization2 entrypoint unavailable");
    }
    VkQueue actualQueue{};
    vkGetDeviceQueue(context.device, context.queueFamily, context.queueIndex, &actualQueue);
    if(actualQueue != context.queue) return s.fail("supplied Vulkan queue does not match family/index");
    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    if(!renderer || !version || !std::strstr(renderer, "zink") || !std::strstr(version, "git-abc426bb62"))
        return s.fail("unvalidated GL driver profile; requires tested Frame Zink git-abc426bb62");
    auto getDeviceUuid = s.proc<PFNGLGETUNSIGNEDBYTEI_VEXTPROC>("glGetUnsignedBytei_vEXT");
    auto getDriverUuid = s.proc<PFNGLGETUNSIGNEDBYTEVEXTPROC>("glGetUnsignedBytevEXT");
    auto createMemory = s.proc<PFNGLCREATEMEMORYOBJECTSEXTPROC>("glCreateMemoryObjectsEXT");
    auto memoryParameter = s.proc<PFNGLMEMORYOBJECTPARAMETERIVEXTPROC>("glMemoryObjectParameterivEXT");
    auto importMemory = s.proc<PFNGLIMPORTMEMORYFDEXTPROC>("glImportMemoryFdEXT");
    auto storage = s.proc<PFNGLTEXSTORAGEMEM2DEXTPROC>("glTexStorageMem2DEXT");
    auto createSemaphores = s.proc<PFNGLGENSEMAPHORESEXTPROC>("glGenSemaphoresEXT");
    auto importSemaphore = s.proc<PFNGLIMPORTSEMAPHOREFDEXTPROC>("glImportSemaphoreFdEXT");
    s.deleteMemory = s.proc<PFNGLDELETEMEMORYOBJECTSEXTPROC>("glDeleteMemoryObjectsEXT");
    s.deleteSemaphores = s.proc<PFNGLDELETESEMAPHORESEXTPROC>("glDeleteSemaphoresEXT");
    s.wait = s.proc<PFNGLWAITSEMAPHOREEXTPROC>("glWaitSemaphoreEXT");
    s.signal = s.proc<PFNGLSIGNALSEMAPHOREEXTPROC>("glSignalSemaphoreEXT");
    if(s.poisoned) return false;
    VkPhysicalDeviceIDProperties ids{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES};
    VkPhysicalDeviceProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2}; properties.pNext = &ids;
    vkGetPhysicalDeviceProperties2(context.physicalDevice, &properties);
    std::array<unsigned char, VK_UUID_SIZE> deviceUuid{}, driverUuid{};
    getDeviceUuid(GL_DEVICE_UUID_EXT, 0, deviceUuid.data()); getDriverUuid(GL_DRIVER_UUID_EXT, driverUuid.data());
    if(!s.glChecked("GL UUID query")) return false;
    if(std::memcmp(ids.deviceUUID, deviceUuid.data(), VK_UUID_SIZE) || std::memcmp(ids.driverUUID, driverUuid.data(), VK_UUID_SIZE))
        return s.fail("GL/Vulkan physical device or driver UUID mismatch");

    s.nativeFormat = format == Format::DepthD32F ? VK_FORMAT_D32_SFLOAT :
        format == Format::MotionRGBA16F ? VK_FORMAT_R16G16B16A16_SFLOAT : VK_FORMAT_R8G8B8A8_UNORM;
    s.internalFormat = format == Format::DepthD32F ? GL_DEPTH_COMPONENT32F : format == Format::MotionRGBA16F ? GL_RGBA16F : GL_RGBA8;
    s.aspect = format == Format::DepthD32F ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    // Complete installed-Zink image definitions from the passing API dumps.
    // Do not apply the RGBA8 view list or depth flags to other formats.
    const VkImageCreateFlags flags = format == Format::DepthD32F ? 0x41000u : format == Format::MotionRGBA16F ? 0x40000u : 0x40008u;
    const VkImageUsageFlags usage = format == Format::DepthD32F ? 0x80027u : 0x80097u;
    const VkFormat views[] = {VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_A8B8G8R8_SRGB_PACK32};
    VkImageFormatListCreateInfo list{VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO}; list.viewFormatCount = 2; list.pViewFormats = views;
    VkPhysicalDeviceExternalImageFormatInfo externalQuery{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO};
    externalQuery.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
    externalQuery.pNext = format == Format::SrgbColorBytes ? &list : nullptr;
    VkPhysicalDeviceImageFormatInfo2 query{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2};
    query.pNext = &externalQuery; query.format = s.nativeFormat; query.type = VK_IMAGE_TYPE_2D;
    query.tiling = VK_IMAGE_TILING_OPTIMAL; query.usage = usage; query.flags = flags;
    VkExternalImageFormatProperties externalProperties{VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES};
    VkImageFormatProperties2 imageProperties{VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2}; imageProperties.pNext = &externalProperties;
    if(!s.checked(vkGetPhysicalDeviceImageFormatProperties2(context.physicalDevice, &query, &imageProperties), "external image capability query")) return false;
    const VkExternalMemoryFeatureFlags required = VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT | VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT;
    if((externalProperties.externalMemoryProperties.externalMemoryFeatures & required) != required ||
        width > imageProperties.imageFormatProperties.maxExtent.width || height > imageProperties.imageFormatProperties.maxExtent.height)
        return s.fail("exact image definition is not shareable at this extent");
    VkExternalMemoryImageCreateInfo external{VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO};
    external.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT; external.pNext = externalQuery.pNext;
    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO}; imageInfo.pNext = &external;
    imageInfo.flags = flags; imageInfo.format = s.nativeFormat; imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = {width, height, 1}; imageInfo.mipLevels = imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT; imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = usage; imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if(!s.checked(vkCreateImage(context.device, &imageInfo, nullptr, &s.image), "create dedicated staging image")) return false;
    VkMemoryRequirements requirements{}; vkGetImageMemoryRequirements(context.device, s.image, &requirements);
    VkPhysicalDeviceMemoryProperties memories{}; vkGetPhysicalDeviceMemoryProperties(context.physicalDevice, &memories);
    unsigned memoryType = ~0u;
    for(unsigned i = 0; i < memories.memoryTypeCount; ++i)
        if((requirements.memoryTypeBits & (1u << i)) && (memories.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) { memoryType = i; break; }
    if(memoryType == ~0u) return s.fail("no compatible dedicated memory type");
    VkMemoryDedicatedAllocateInfo dedicated{VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO}; dedicated.image = s.image;
    VkExportMemoryAllocateInfo exportInfo{VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO}; exportInfo.pNext = &dedicated;
    exportInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO}; allocation.pNext = &exportInfo;
    allocation.allocationSize = requirements.size; allocation.memoryTypeIndex = memoryType;
    if(!s.checked(vkAllocateMemory(context.device, &allocation, nullptr, &s.memory), "allocate dedicated staging memory") ||
        !s.checked(vkBindImageMemory(context.device, s.image, s.memory, 0), "bind staging memory")) return false;
    auto getMemoryFd = reinterpret_cast<PFN_vkGetMemoryFdKHR>(vkGetDeviceProcAddr(context.device, "vkGetMemoryFdKHR"));
    auto getSemaphoreFd = reinterpret_cast<PFN_vkGetSemaphoreFdKHR>(vkGetDeviceProcAddr(context.device, "vkGetSemaphoreFdKHR"));
    if(!getMemoryFd || !getSemaphoreFd) return s.fail("Vulkan FD entrypoints unavailable");
    VkMemoryGetFdInfoKHR fdInfo{VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR}; fdInfo.memory = s.memory; fdInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
    if(!s.checked(getMemoryFd(context.device, &fdInfo, &s.fd), "export staging FD")) return false;
    // Check earlier errors before every ownership-taking import. Otherwise a
    // successful import could be mistaken for failure and its GL-owned FD closed.
    if(!s.glChecked("GL state before memory-object creation")) return false;
    createMemory(1, &s.memoryObject);
    if(!s.glChecked("GL memory-object creation") || !s.memoryObject) return s.fail("GL memory-object creation failed");
    const GLint yes = GL_TRUE;
    memoryParameter(s.memoryObject, GL_DEDICATED_MEMORY_OBJECT_EXT, &yes);
    if(!s.glChecked("GL dedicated memory parameter")) return false;
    importMemory(s.memoryObject, requirements.size, GL_HANDLE_TYPE_OPAQUE_FD_EXT, s.fd);
    if(!s.glChecked("GL dedicated import")) return false;
    s.fd = -1; // Successful GL import owns the descriptor.
    GLint oldTexture{}; glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldTexture);
    glGenTextures(1, &s.texture); glBindTexture(GL_TEXTURE_2D, s.texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_TILING_EXT, GL_OPTIMAL_TILING_EXT);
    storage(GL_TEXTURE_2D, 1, s.internalFormat, width, height, s.memoryObject, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(oldTexture));
    if(!s.glChecked("GL staging storage")) return false;

    VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO}; pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; pool.queueFamilyIndex = context.queueFamily;
    if(!s.checked(vkCreateCommandPool(context.device, &pool, nullptr, &s.pool), "create copy command pool")) return false;
    VkCommandBufferAllocateInfo commands{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO}; commands.commandPool = s.pool; commands.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; commands.commandBufferCount = 1;
    if(!s.checked(vkAllocateCommandBuffers(context.device, &commands, &s.command), "allocate copy commands")) return false;
    VkFenceCreateInfo fence{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    if(!s.checked(vkCreateFence(context.device, &fence, nullptr, &s.fence), "create copy fence")) return false;
    VkExportSemaphoreCreateInfo semaphoreExport{VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO}; semaphoreExport.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
    VkSemaphoreCreateInfo semaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO}; semaphoreInfo.pNext = &semaphoreExport;
    if(!s.glChecked("GL state before semaphore creation")) return false;
    createSemaphores(2, s.glSemaphores.data());
    if(!s.glChecked("GL semaphore creation") || !s.glSemaphores[0] || !s.glSemaphores[1])
        return s.fail("GL semaphore creation failed");
    for(unsigned i = 0; i < 2; ++i) {
        if(!s.checked(vkCreateSemaphore(context.device, &semaphoreInfo, nullptr, &s.semaphores[i]), "create binary external semaphore")) return false;
        VkSemaphoreGetFdInfoKHR semaphoreFd{VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR}; semaphoreFd.semaphore = s.semaphores[i]; semaphoreFd.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
        if(!s.checked(getSemaphoreFd(context.device, &semaphoreFd, &s.fd), "export semaphore FD")) return false;
        if(!s.glChecked("GL state before semaphore FD import")) return false;
        importSemaphore(s.glSemaphores[i], GL_HANDLE_TYPE_OPAQUE_FD_EXT, s.fd);
        if(!s.glChecked("GL semaphore import")) return false;
        s.fd = -1;
    }
    if(!s.begin()) return false;
    s.barrier(s.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, context.queueFamily, VK_QUEUE_FAMILY_EXTERNAL,
        0, 0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    if(!s.submit(false)) return false;
    s.ready = true;
    std::fprintf(stderr, "GL/Vulkan staging ready: GL=0x%x Vk=%u extent=%ux%u flags=0x%x usage=0x%x dedicated=%llu type=%u\n",
        s.internalFormat, s.nativeFormat, width, height, flags, usage, static_cast<unsigned long long>(requirements.size), memoryType);
    return true;
}

bool Bridge::beginGlWrite()
{
    if(state && state->pending) return state->fail("cannot reuse GL staging before bridge completion");
    if(!healthy()) return false;
    auto& s = *state;
    if(SDL_GL_GetCurrentContext() != s.owner) return s.fail("GL handoff requires the owning context current");
    if(s.glWriting) return s.fail("staging image is already acquired for GL");
    if(s.format == Format::SrgbColorBytes && glIsEnabled(GL_FRAMEBUFFER_SRGB))
        return s.fail("gamma-encoded color staging requires GL_FRAMEBUFFER_SRGB disabled");
    const GLenum layout = GL_LAYOUT_GENERAL_EXT;
    s.wait(s.glSemaphores[1], 0, nullptr, 1, &s.texture, &layout);
    if(!s.glChecked("Vulkan -> GL semaphore wait")) return false;
    s.glWriting = true;
    return true;
}

bool Bridge::cancelGlWrite()
{
    return enqueueCancelGlWrite() && waitForCompletion();
}

bool Bridge::enqueueCancelGlWrite()
{
    if(!healthy()) return false;
    auto& s = *state;
    if(SDL_GL_GetCurrentContext() != s.owner || !s.glWriting)
        return s.fail("cancel requires an acquired staging image and owning GL context");
    const GLenum general = GL_LAYOUT_GENERAL_EXT;
    s.signal(s.glSemaphores[0], 0, nullptr, 1, &s.texture, &general);
    if(!s.glChecked("cancel GL -> Vulkan semaphore signal")) return false;
    glFlush();
    if(!s.begin()) return false;
    s.barrier(s.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
        VK_QUEUE_FAMILY_EXTERNAL, s.context.queueFamily, 0, 0,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    s.barrier(s.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
        s.context.queueFamily, VK_QUEUE_FAMILY_EXTERNAL, 0, 0,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    if(!s.submit(true, false)) return false;
    s.glWriting = false;
    return true;
}

bool Bridge::copyTo(const Destination& d)
{
    return enqueueCopyTo(d) && waitForCompletion();
}

bool Bridge::enqueueCopyTo(const Destination& d)
{
    if(!healthy()) return false;
    auto& s = *state;
    if(SDL_GL_GetCurrentContext() != s.owner) return s.fail("GL signal requires the owning context current");
    const bool depth = s.format == Format::DepthD32F;
    const VkImageLayout layout = depth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    const Operation operation = depth ? Operation::DepthAttachment : Operation::TransferCopy;
    if(!s.glWriting || d.image == s.image || !acceptsDestination(s.format, operation, s.width, s.height, d))
        return s.fail("invalid/unwaited XR destination or incompatible format/extent/layout");
    const bool attachmentDepth = depth; // Runtime D32 is always attachment-only.
    if(attachmentDepth && !s.depthTarget(d)) return false;
    // The producer may restore its GL state after detaching the staging FBO.
    // FRAMEBUFFER_SRGB at submission time says nothing about earlier writes
    // and cannot affect this Vulkan byte copy. beginGlWrite checks the write
    // contract; deferred submission must not depend on unrelated current GL state.
    const GLenum general = GL_LAYOUT_GENERAL_EXT;
    s.signal(s.glSemaphores[0], 0, nullptr, 1, &s.texture, &general);
    if(!s.glChecked("GL -> Vulkan semaphore signal")) return false;
    glFlush();
    if(!s.begin()) return false;
    const VkAccessFlags sourceAccess = attachmentDepth ? VK_ACCESS_SHADER_READ_BIT : VK_ACCESS_TRANSFER_READ_BIT;
    const VkPipelineStageFlags sourceStage = attachmentDepth ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_TRANSFER_BIT;
    s.barrier(s.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_FAMILY_EXTERNAL, s.context.queueFamily,
        0, sourceAccess, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, sourceStage);
    const auto sourceLayout = depth ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    if(!depth) s.barrier(s.image, VK_IMAGE_LAYOUT_GENERAL, sourceLayout, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
        VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    const VkPipelineStageFlags runtimeStage = depth ? VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    const VkAccessFlags runtimeAccess = depth ? VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT : VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    if(attachmentDepth) {
        // The successful XR depth-attachment request guarantees this use.
        // Keep the required layout unchanged and use sync2 equal-layout memory
        // dependencies; no assumption about native runtime image flags and no
        // runtime depth transfer, even if transfer usage was also requested.
        s.drawDepth();
        s.barrier(d.image, layout, layout, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, runtimeAccess, runtimeStage, runtimeStage);
        s.barrier(s.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, s.context.queueFamily, VK_QUEUE_FAMILY_EXTERNAL,
            sourceAccess, 0, sourceStage, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
        if(!s.submit(true, false)) return false;
        s.glWriting = false;
        return true;
    }
    s.barrier(d.image, layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
        runtimeAccess, VK_ACCESS_TRANSFER_WRITE_BIT, runtimeStage, VK_PIPELINE_STAGE_TRANSFER_BIT);
    std::unique_ptr<VkImageCopy[]> rows(new(std::nothrow) VkImageCopy[s.height]{});
    if(!rows) return s.fail("row-copy storage allocation failed");
    for(unsigned y = 0; y < s.height; ++y) {
        rows[y].srcSubresource = rows[y].dstSubresource = {s.aspect, 0, 0, 1};
        rows[y].srcOffset.y = s.height - 1 - y; rows[y].dstOffset.y = y; rows[y].extent = {s.width, 1, 1};
    }
    // Exact compatible copies preserve encoded color bytes and signed floats.
    // Flip spatial rows only; motion already uses Vulkan NDC component signs.
    vkCmdCopyImage(s.command, s.image, sourceLayout, d.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        s.height, rows.get());
    s.barrier(d.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, layout, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
        VK_ACCESS_TRANSFER_WRITE_BIT, runtimeAccess, VK_PIPELINE_STAGE_TRANSFER_BIT, runtimeStage);
    s.barrier(s.image, sourceLayout, VK_IMAGE_LAYOUT_GENERAL, s.context.queueFamily, VK_QUEUE_FAMILY_EXTERNAL,
        VK_ACCESS_TRANSFER_READ_BIT, 0, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    if(!s.submit(true, false)) return false;
    s.glWriting = false;
    return true;
}

bool Bridge::canPairWith(const Bridge& other) const
{
    return this != &other && healthy() && other.healthy() &&
        sameSubmissionQueue(state->context, other.state->context);
}

bool Bridge::waitForCompletion()
{
    if(!state || !state->ready) return false;
    // Also permit a bounded drain retry after a poisoned/timed-out submission.
    return state->complete() && !state->pending;
}

bool Bridge::completeAfter(const Bridge& terminal)
{
    if(this == &terminal || !state || !state->ready || !state->pending ||
        !terminal.state || terminal.state->pending || !terminal.healthy() ||
        !sameSubmissionQueue(state->context, terminal.state->context)) return false;
    // A completed later fence on this queue covers our submitted commands.
    // Confirm our own fence rather than resetting a possibly pending resource.
    if(!state->checked(vkGetFenceStatus(state->context.device, state->fence),
        "earlier paired export fence not complete")) return false;
    state->pending = false;
    return true;
}

bool Bridge::shutdown()
{
    if(!state) return true;
    auto& s = *state;
    if((s.texture || s.memoryObject || s.glSemaphores[0] || s.glSemaphores[1]) && SDL_GL_GetCurrentContext() != s.owner)
        return s.fail("staging cleanup requires the owning GL context current");
    if(s.pending) {
        if(vkWaitForFences(s.context.device, 1, &s.fence, VK_TRUE, 5'000'000'000ULL) != VK_SUCCESS)
            return s.fail("cannot destroy staging resources while GPU work is pending");
        s.pending = false;
    }
    // GL work never waits on an unsignaled semaphore: each GL wait follows a
    // successful bounded Vulkan submit. Finish before deleting imported aliases.
    if(s.texture || s.glWriting) {
        if(!s.cleanupSync) {
            if(!s.glChecked("GL state before cleanup fence")) return false;
            s.cleanupSync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
            if(!s.glChecked("GL cleanup fence") || !s.cleanupSync) return s.fail("GL cleanup fence failed");
            glFlush();
        }
        const auto result = glClientWaitSync(s.cleanupSync, 0, 5'000'000'000ULL);
        if(result != GL_ALREADY_SIGNALED && result != GL_CONDITION_SATISFIED)
            return s.fail("GL staging cleanup did not complete within five seconds");
        glDeleteSync(s.cleanupSync); s.cleanupSync = nullptr;
    }
    if(s.texture) glDeleteTextures(1, &s.texture);
    if(s.memoryObject && s.deleteMemory) s.deleteMemory(1, &s.memoryObject);
    if((s.glSemaphores[0] || s.glSemaphores[1]) && s.deleteSemaphores) s.deleteSemaphores(2, s.glSemaphores.data());
    if(s.fd >= 0) close(s.fd);
    if(s.context.device) {
        if(s.framebuffer) vkDestroyFramebuffer(s.context.device, s.framebuffer, nullptr);
        if(s.destinationView) vkDestroyImageView(s.context.device, s.destinationView, nullptr);
        if(s.pipeline) vkDestroyPipeline(s.context.device, s.pipeline, nullptr);
        if(s.pass) vkDestroyRenderPass(s.context.device, s.pass, nullptr);
        if(s.pipelineLayout) vkDestroyPipelineLayout(s.context.device, s.pipelineLayout, nullptr);
        if(s.descriptorPool) vkDestroyDescriptorPool(s.context.device, s.descriptorPool, nullptr);
        if(s.descriptorLayout) vkDestroyDescriptorSetLayout(s.context.device, s.descriptorLayout, nullptr);
        if(s.sampler) vkDestroySampler(s.context.device, s.sampler, nullptr);
        if(s.sourceView) vkDestroyImageView(s.context.device, s.sourceView, nullptr);
        if(s.vertexShader) vkDestroyShaderModule(s.context.device, s.vertexShader, nullptr);
        if(s.fragmentShader) vkDestroyShaderModule(s.context.device, s.fragmentShader, nullptr);
        for(auto semaphore : s.semaphores) if(semaphore) vkDestroySemaphore(s.context.device, semaphore, nullptr);
        if(s.fence) vkDestroyFence(s.context.device, s.fence, nullptr);
        if(s.pool) vkDestroyCommandPool(s.context.device, s.pool, nullptr);
        if(s.image) vkDestroyImage(s.context.device, s.image, nullptr);
        if(s.memory) vkFreeMemory(s.context.device, s.memory, nullptr);
    }
    s = State{};
    return true;
}
} // namespace qvr::gl_vulkan
#endif
