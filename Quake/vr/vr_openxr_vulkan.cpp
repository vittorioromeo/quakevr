#include "vr_openxr_vulkan.hpp"
#if defined(QVR_HAVE_GL_VULKAN_BRIDGE) && defined(__linux__)
#define XR_USE_GRAPHICS_API_VULKAN
#include <openxr/openxr_platform.h>
#include <SDL.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <vector>

namespace qvr::openxr_vulkan
{
namespace
{
template<class F> F function(XrInstance instance, const char* name)
{
    PFN_xrVoidFunction result{};
    if(xrGetInstanceProcAddr(instance, name, &result) != XR_SUCCESS) return nullptr;
    return reinterpret_cast<F>(result);
}
std::uint32_t version(XrVersion value)
{
    // XR graphics requirements explicitly ignore the Vulkan patch component.
    return VK_MAKE_VERSION(XR_VERSION_MAJOR(value), XR_VERSION_MINOR(value), 0);
}
}

Bootstrap::~Bootstrap()
{
    // A forgotten caller teardown must not destroy a device with live children.
    const bool destroyed = shutdown();
    (void)destroyed;
}
bool Bootstrap::fail(const char* operation)
{
    std::snprintf(failure, sizeof(failure), "%s", operation);
    return false;
}
bool Bootstrap::shutdown(bool drained)
{
    if(sessionExposed && (!drained || !terminalQueueDrained))
        return fail("destroy children/session and complete terminal bound-queue drain before Vulkan shutdown");
    if(terminalFence) vkDestroyFence(device.device, terminalFence, nullptr);
    if(device.device) vkDestroyDevice(device.device, nullptr);
    if(device.instance) vkDestroyInstance(device.instance, nullptr);
    device = {};
    xrInstance = XR_NULL_HANDLE;
    system = XR_NULL_SYSTEM_ID;
    sessionExposed = false;
    terminalFence = VK_NULL_HANDLE;
    terminalSubmitted = terminalQueueDrained = false;
    return true;
}
bool Bootstrap::drainAfterSessionDestroy(bool destroyed)
{
    if(!destroyed) return fail("terminal Vulkan drain requires destroyed XR session");
    if(!device.device || !sessionExposed) return true; // never exposed a usable session/queue
    if(terminalQueueDrained) return true;
    if(!terminalFence)
    {
        VkFenceCreateInfo info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        if(vkCreateFence(device.device, &info, nullptr, &terminalFence) != VK_SUCCESS)
            return fail("create terminal bound-queue fence failed");
    }
    if(!terminalSubmitted)
    {
        if(vkQueueSubmit(device.queue, 0, nullptr, terminalFence) != VK_SUCCESS)
            return fail("terminal bound-queue submit failed; device retained");
        terminalSubmitted = true;
    }
    if(vkWaitForFences(device.device, 1, &terminalFence, VK_TRUE, 5'000'000'000ULL) != VK_SUCCESS)
        return fail("terminal bound-queue fence timed out/failed; fence/device retained");
    terminalQueueDrained = true;
    return true;
}
bool Bootstrap::initialize(XrInstance instance, XrSystemId systemId, bool enabled)
{
    if(device.instance || xrInstance) return fail("Vulkan bootstrap already initialized; explicit teardown required");
    if(!instance || systemId == XR_NULL_SYSTEM_ID || !enabled) return fail("Vulkan2-enabled XR instance/system required");
    failure[0] = '\0';
    const auto requirements = function<PFN_xrGetVulkanGraphicsRequirements2KHR>(instance, "xrGetVulkanGraphicsRequirements2KHR");
    const auto createInstance = function<PFN_xrCreateVulkanInstanceKHR>(instance, "xrCreateVulkanInstanceKHR");
    const auto getDevice = function<PFN_xrGetVulkanGraphicsDevice2KHR>(instance, "xrGetVulkanGraphicsDevice2KHR");
    const auto createDevice = function<PFN_xrCreateVulkanDeviceKHR>(instance, "xrCreateVulkanDeviceKHR");
    if(!requirements || !createInstance || !getDevice || !createDevice) return fail("Vulkan2 entry points unavailable");
    XrGraphicsRequirementsVulkan2KHR limits{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR};
    if(requirements(instance, systemId, &limits) != XR_SUCCESS) return fail("xrGetVulkanGraphicsRequirements2KHR failed");
    std::uint32_t loaderVersion = VK_API_VERSION_1_0;
    const auto enumerateVersion = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceVersion"));
    if(enumerateVersion && enumerateVersion(&loaderVersion) != VK_SUCCESS) return fail("Vulkan loader version query failed");
    const auto api = std::max(version(limits.minApiVersionSupported), VK_API_VERSION_1_1);
    if(api > version(limits.maxApiVersionSupported) || api > loaderVersion) return fail("Vulkan >=1.1 unavailable within XR/loader requirements");
    VkApplicationInfo application{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    application.pApplicationName = "Quake VR Vulkan bridge";
    application.apiVersion = api;
    VkInstanceCreateInfo info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    info.pApplicationInfo = &application;
    // Vulkan2 wrappers aggregate the runtime's own instance/device requirements
    // with these application requirements. No legacy Vulkan1 extension-list
    // calls or XR_KHR_vulkan_enable dependency are needed on this path.
    XrVulkanInstanceCreateInfoKHR request{XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR};
    request.systemId = systemId;
    request.pfnGetInstanceProcAddr = vkGetInstanceProcAddr;
    request.vulkanCreateInfo = &info;
    VkResult result = VK_ERROR_INITIALIZATION_FAILED;
    if(createInstance(instance, &request, &device.instance, &result) != XR_SUCCESS || result != VK_SUCCESS)
    {
        (void)shutdown();
        return fail("xrCreateVulkanInstanceKHR/Vulkan creation failed");
    }
    // Every later failure releases only resources created here; no queue work
    // or session has been exposed during initialize.
    const auto abort = [this](const char* reason) { (void)shutdown(); return fail(reason); };
    XrVulkanGraphicsDeviceGetInfoKHR selected{XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR};
    selected.systemId = systemId;
    selected.vulkanInstance = device.instance;
    if(getDevice(instance, &selected, &device.physicalDevice) != XR_SUCCESS || !device.physicalDevice)
        return abort("xrGetVulkanGraphicsDevice2KHR failed");
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(device.physicalDevice, &properties);
    if(properties.apiVersion < api) return abort("runtime-selected Vulkan device API too old");
    if(!SDL_GL_GetCurrentContext()) return abort("Vulkan bridge requires renderer GL context current");
    const auto getDeviceUuid = reinterpret_cast<PFNGLGETUNSIGNEDBYTEI_VEXTPROC>(SDL_GL_GetProcAddress("glGetUnsignedBytei_vEXT"));
    const auto getDriverUuid = reinterpret_cast<PFNGLGETUNSIGNEDBYTEVEXTPROC>(SDL_GL_GetProcAddress("glGetUnsignedBytevEXT"));
    if(!getDeviceUuid || !getDriverUuid || glGetError() != GL_NO_ERROR)
        return abort("GL UUID entry points or clean GL error state unavailable");
    std::array<unsigned char, VK_UUID_SIZE> glDevice{}, glDriver{};
    getDeviceUuid(GL_DEVICE_UUID_EXT, 0, glDevice.data());
    getDriverUuid(GL_DRIVER_UUID_EXT, glDriver.data());
    if(glGetError() != GL_NO_ERROR) return abort("GL device/driver UUID query failed");
    VkPhysicalDeviceIDProperties ids{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES};
    VkPhysicalDeviceProperties2 native{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    native.pNext = &ids;
    vkGetPhysicalDeviceProperties2(device.physicalDevice, &native);
    if(std::memcmp(glDevice.data(), ids.deviceUUID, VK_UUID_SIZE) ||
        std::memcmp(glDriver.data(), ids.driverUUID, VK_UUID_SIZE))
        return abort("GL/runtime Vulkan physical-device or driver UUID mismatch");
    std::uint32_t count{};
    if(vkEnumerateDeviceExtensionProperties(device.physicalDevice, nullptr, &count, nullptr) != VK_SUCCESS)
        return abort("Vulkan device extension count failed");
    std::vector<VkExtensionProperties> available(count);
    if(vkEnumerateDeviceExtensionProperties(device.physicalDevice, nullptr, &count, available.data()) != VK_SUCCESS)
        return abort("Vulkan device extension enumeration failed");
    std::vector<const char*> extensions{
        VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME, VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME,
        VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME, VK_EXT_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_EXTENSION_NAME,
        VK_EXT_ATTACHMENT_FEEDBACK_LOOP_LAYOUT_EXTENSION_NAME, VK_EXT_SAMPLE_LOCATIONS_EXTENSION_NAME,
        VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME};
    // Explicitly satisfy MSRTSS's extension dependencies below Vulkan 1.2;
    // do not assume the runtime's aggregated requirements happen to add them.
    if(api < VK_API_VERSION_1_2)
    {
        extensions.push_back(VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME);
        extensions.push_back(VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME);
    }
    for(const char* name : extensions)
        if(!std::any_of(available.begin(), available.end(), [name](const auto& e) { return !std::strcmp(name, e.extensionName); }))
            return abort(name);
    VkPhysicalDeviceSynchronization2Features synchronization{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES};
    VkPhysicalDeviceAttachmentFeedbackLoopLayoutFeaturesEXT feedback{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ATTACHMENT_FEEDBACK_LOOP_LAYOUT_FEATURES_EXT};
    feedback.pNext = &synchronization;
    VkPhysicalDeviceMultisampledRenderToSingleSampledFeaturesEXT msrtss{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_FEATURES_EXT};
    msrtss.pNext = &feedback;
    VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    features.pNext = &msrtss;
    vkGetPhysicalDeviceFeatures2(device.physicalDevice, &features);
    if(!msrtss.multisampledRenderToSingleSampled || !feedback.attachmentFeedbackLoopLayout || !synchronization.synchronization2)
        return abort("required staging/synchronization2 features unavailable");
    vkGetPhysicalDeviceQueueFamilyProperties(device.physicalDevice, &count, nullptr);
    std::vector<VkQueueFamilyProperties> queues(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device.physicalDevice, &count, queues.data());
    for(std::uint32_t i = 0; i < count; ++i)
        if(queues[i].queueCount && (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) { device.queueFamily = i; break; }
    if(device.queueFamily == ~0u) return abort("runtime-selected Vulkan device has no graphics queue");
    const float priority = 1.f;
    VkDeviceQueueCreateInfo queue{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queue.queueFamilyIndex = device.queueFamily;
    queue.queueCount = 1;
    queue.pQueuePriorities = &priority;
    VkDeviceCreateInfo logical{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    logical.pNext = &msrtss;
    logical.queueCreateInfoCount = 1;
    logical.pQueueCreateInfos = &queue;
    logical.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
    logical.ppEnabledExtensionNames = extensions.data();
    XrVulkanDeviceCreateInfoKHR deviceRequest{XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR};
    deviceRequest.systemId = systemId;
    deviceRequest.pfnGetInstanceProcAddr = vkGetInstanceProcAddr;
    deviceRequest.vulkanPhysicalDevice = device.physicalDevice;
    deviceRequest.vulkanCreateInfo = &logical;
    result = VK_ERROR_INITIALIZATION_FAILED;
    if(createDevice(instance, &deviceRequest, &device.device, &result) != XR_SUCCESS || result != VK_SUCCESS)
        return abort("xrCreateVulkanDeviceKHR/Vulkan creation failed");
    vkGetDeviceQueue(device.device, device.queueFamily, 0, &device.queue);
    if(!device.queue) return abort("runtime-bound Vulkan queue unavailable");
    device.apiVersion = api;
    device.externalImageFeaturesEnabled = device.sampleLocationsEnabled = device.synchronization2Enabled = true;
    xrInstance = instance;
    system = systemId;
    return true;
}
bool Bootstrap::createSession(XrSession& output)
{
    if(output != XR_NULL_HANDLE || sessionExposed || !xrInstance || !device.device)
        return fail("Vulkan session requires initialized bootstrap and empty output");
    XrGraphicsBindingVulkan2KHR binding{XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR};
    binding.instance = device.instance;
    binding.physicalDevice = device.physicalDevice;
    binding.device = device.device;
    binding.queueFamilyIndex = device.queueFamily;
    binding.queueIndex = device.queueIndex;
    XrSessionCreateInfo info{XR_TYPE_SESSION_CREATE_INFO};
    info.next = &binding;
    info.systemId = system;
    XrSession created = XR_NULL_HANDLE;
    if(xrCreateSession(xrInstance, &info, &created) != XR_SUCCESS || created == XR_NULL_HANDLE)
        return fail("xrCreateSession Vulkan2 binding failed");
    output = created;
    sessionExposed = true;
    return true;
}
} // namespace qvr::openxr_vulkan
#endif
