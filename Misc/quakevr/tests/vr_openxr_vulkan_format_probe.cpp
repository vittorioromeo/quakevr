#define XR_USE_GRAPHICS_API_VULKAN
#include <openxr/openxr.h>
#include <vulkan/vulkan.h>
#include <openxr/openxr_platform.h>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

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

int main()
{
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
    if(!hasVulkan2) return 2;

    const char* enabled[3]{};
    uint32_t enabledCount = 0;
    if(hasVulkan) enabled[enabledCount++] = XR_KHR_VULKAN_ENABLE_EXTENSION_NAME;
    enabled[enabledCount++] = XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME;
    if(hasSynthesis) enabled[enabledCount++] = "XR_EXT_frame_synthesis";
    XrInstanceCreateInfo instanceInfo{XR_TYPE_INSTANCE_CREATE_INFO};
    strcpy(instanceInfo.applicationInfo.applicationName, "QuakeVR Vulkan format probe");
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
    if(hasVulkan)
    {
        uint32_t chars = 0;
        result = getInstanceExtensions(xrInstance, system, 0, &chars, nullptr);
        if(XR_FAILED(result)) { std::cerr << "Vulkan instance extension size=" << result << '\n'; xrDestroyInstance(xrInstance); return 7; }
        std::string instanceExtensionString(chars, '\0');
        result = getInstanceExtensions(xrInstance, system, chars, &chars, instanceExtensionString.data());
        if(XR_FAILED(result)) { std::cerr << "Vulkan instance extensions=" << result << '\n'; xrDestroyInstance(xrInstance); return 7; }
        instanceNames = splitNames(instanceExtensionString, instanceNamesOwned);
    }

    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "QuakeVR Vulkan format probe";
    const auto toVkVersion = [](XrVersion version)
    {
        return VK_MAKE_API_VERSION(0, XR_VERSION_MAJOR(version), XR_VERSION_MINOR(version),
            XR_VERSION_PATCH(version));
    };
    uint32_t loaderApiVersion = VK_API_VERSION_1_0;
    if(vkEnumerateInstanceVersion(&loaderApiVersion) != VK_SUCCESS)
        loaderApiVersion = VK_API_VERSION_1_0;
    const uint32_t minApiVersion = toVkVersion(requirements.minApiVersionSupported);
    const uint32_t maxApiVersion = toVkVersion(requirements.maxApiVersionSupported);
    if(loaderApiVersion < minApiVersion)
    {
        std::cerr << "Vulkan loader version " << VK_VERSION_MAJOR(loaderApiVersion) << '.'
            << VK_VERSION_MINOR(loaderApiVersion) << " is below OpenXR minimum "
            << VK_VERSION_MAJOR(minApiVersion) << '.' << VK_VERSION_MINOR(minApiVersion) << '\n';
        xrDestroyInstance(xrInstance);
        return 8;
    }
    app.apiVersion = minApiVersion;
    std::cout << "Vulkan apiVersion=" << VK_VERSION_MAJOR(app.apiVersion) << '.'
        << VK_VERSION_MINOR(app.apiVersion) << " runtimeMax=" << VK_VERSION_MAJOR(maxApiVersion)
        << '.' << VK_VERSION_MINOR(maxApiVersion) << '\n';
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
    VkPhysicalDeviceProperties physicalProperties{};
    vkGetPhysicalDeviceProperties(physicalDevice, &physicalProperties);
    if(physicalProperties.apiVersion < minApiVersion)
    {
        std::cerr << "Vulkan physical device version " << VK_VERSION_MAJOR(physicalProperties.apiVersion)
            << '.' << VK_VERSION_MINOR(physicalProperties.apiVersion) << " is below OpenXR minimum "
            << VK_VERSION_MAJOR(minApiVersion) << '.' << VK_VERSION_MINOR(minApiVersion) << '\n';
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
        uint32_t chars = 0;
        result = getDeviceExtensions(xrInstance, system, 0, &chars, nullptr);
        if(XR_FAILED(result)) { std::cerr << "Vulkan device extension size=" << result << '\n'; vkDestroyInstance(vkInstance, nullptr); xrDestroyInstance(xrInstance); return 11; }
        std::string deviceExtensionString(chars, '\0');
        result = getDeviceExtensions(xrInstance, system, chars, &chars, deviceExtensionString.data());
        if(XR_FAILED(result)) { std::cerr << "Vulkan device extensions=" << result << '\n'; vkDestroyInstance(vkInstance, nullptr); xrDestroyInstance(xrInstance); return 11; }
        deviceNames = splitNames(deviceExtensionString, deviceNamesOwned);
    }
    float priority = 1.f;
    VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queueInfo.queueFamilyIndex = queueFamily; queueInfo.queueCount = 1; queueInfo.pQueuePriorities = &priority;
    VkDeviceCreateInfo vkDeviceInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    vkDeviceInfo.queueCreateInfoCount = 1; vkDeviceInfo.pQueueCreateInfos = &queueInfo;
    vkDeviceInfo.enabledExtensionCount = static_cast<uint32_t>(deviceNames.size());
    vkDeviceInfo.ppEnabledExtensionNames = deviceNames.data();
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
    if(XR_FAILED(result) || viewCount < 2)
    {
        std::cerr << "stereo view count=" << viewCount << " result=" << result << '\n';
        xrDestroySession(session); vkDeviceWaitIdle(device); vkDestroyDevice(device, nullptr);
        vkDestroyInstance(vkInstance, nullptr); xrDestroyInstance(xrInstance); return 14;
    }
    std::vector<XrViewConfigurationView> views(viewCount, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
    result = xrEnumerateViewConfigurationViews(xrInstance, system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
        viewCount, &viewCount, views.data());
    if(XR_FAILED(result))
    {
        std::cerr << "stereo view details=" << result << '\n';
        xrDestroySession(session); vkDeviceWaitIdle(device); vkDestroyDevice(device, nullptr);
        vkDestroyInstance(vkInstance, nullptr); xrDestroyInstance(xrInstance); return 14;
    }
    const uint32_t width = std::max(32u, views[0].recommendedImageRectWidth / 4);
    const uint32_t height = std::max(32u, views[0].recommendedImageRectHeight / 4);
    std::vector<XrSwapchain> synthesisSwapchains;
    bool hasMotionFormat = false, hasDepthFormat = false;
    for(int64_t format : formats)
    {
        hasMotionFormat |= format == VK_FORMAT_R16G16B16A16_SFLOAT;
        hasDepthFormat |= format == VK_FORMAT_D32_SFLOAT;
    }
    auto createImages = [&](int64_t format, XrSwapchainUsageFlags usage, const char* label)
    {
        XrSwapchainCreateInfo info{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        info.usageFlags = usage;
        info.format = format;
        info.sampleCount = 1;
        info.width = width;
        info.height = height;
        info.faceCount = 1;
        info.arraySize = 1;
        info.mipCount = 1;
        for(uint32_t eye = 0; eye < 2; ++eye)
        {
            XrSwapchain swapchain = XR_NULL_HANDLE;
            const XrResult createResult = xrCreateSwapchain(session, &info, &swapchain);
            std::cout << label << " eye=" << eye << " create=" << createResult;
            if(XR_FAILED(createResult)) { std::cout << '\n'; return false; }
            uint32_t imageCount = 0;
            XrResult imageResult = xrEnumerateSwapchainImages(swapchain, 0, &imageCount, nullptr);
            std::vector<XrSwapchainImageVulkanKHR> images(imageCount);
            for(auto& image : images) image.type = XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR;
            if(XR_SUCCEEDED(imageResult))
                imageResult = xrEnumerateSwapchainImages(swapchain, imageCount, &imageCount,
                    reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data()));
            std::cout << " enumerateImages=" << imageResult << " count=" << imageCount << '\n';
            if(XR_FAILED(imageResult)) { xrDestroySwapchain(swapchain); return false; }
            synthesisSwapchains.push_back(swapchain);
        }
        return true;
    };
    bool created = hasSynthesis && hasMotionFormat && hasDepthFormat;
    if(!hasSynthesis) std::cerr << "XR_EXT_frame_synthesis is unavailable\n";
    if(!hasMotionFormat) std::cerr << "no VK_FORMAT_R16G16B16A16_SFLOAT motion format\n";
    if(!hasDepthFormat) std::cerr << "no VK_FORMAT_D32_SFLOAT depth format\n";
    if(hasMotionFormat)
        created &= createImages(VK_FORMAT_R16G16B16A16_SFLOAT,
            XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT, "motion-fp16");
    if(hasDepthFormat)
        created &= createImages(VK_FORMAT_D32_SFLOAT,
            XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT, "depth-d32");
    for(XrSwapchain swapchain : synthesisSwapchains) xrDestroySwapchain(swapchain);
    std::cout << "testExtent=" << width << 'x' << height << " synthesisSwapchainsCreated=" << created << '\n';

    if(XR_SUCCEEDED(result))
    {
        std::cout << "Vulkan OpenXR formats (count=" << count << "):";
        for(int64_t format : formats)
        {
            std::cout << ' ' << format;
            if(format == VK_FORMAT_R16G16B16A16_SFLOAT) std::cout << "(R16G16B16A16_SFLOAT)";
            if(format == VK_FORMAT_R16G16_SFLOAT) std::cout << "(R16G16_SFLOAT)";
            if(format == VK_FORMAT_D32_SFLOAT) std::cout << "(D32_SFLOAT)";
            if(format == VK_FORMAT_D24_UNORM_S8_UINT) std::cout << "(D24S8)";
        }
        std::cout << '\n';
    }
    else std::cerr << "xrEnumerateSwapchainFormats=" << result << '\n';
    xrDestroySession(session); vkDeviceWaitIdle(device); vkDestroyDevice(device, nullptr);
    vkDestroyInstance(vkInstance, nullptr); xrDestroyInstance(xrInstance);
    return XR_FAILED(result) || !created ? 15 : 0;
}
