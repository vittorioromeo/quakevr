// Probe whether the Steam Frame GL and Vulkan drivers expose compatible
// external-memory/semaphore paths for a future OpenXR Vulkan transport adapter.
// This checks capability and physical-device UUIDs only; it does not allocate,
// import, render, or synchronize shared resources.

#include <SDL.h>
#include <SDL_opengl.h>
#include <vulkan/vulkan.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace
{
bool hasExtension(const std::vector<VkExtensionProperties>& extensions, const char* name)
{
    for(const auto& extension : extensions)
        if(std::strcmp(extension.extensionName, name) == 0)
            return true;
    return false;
}

bool hasGlExtension(const char* extensions, const char* name)
{
    const size_t nameLength = std::strlen(name);
    const char* current = extensions;
    while((current = std::strstr(current, name)) != nullptr)
    {
        const bool leftBoundary = current == extensions || current[-1] == ' ';
        const bool rightBoundary = current[nameLength] == '\0' || current[nameLength] == ' ';
        if(leftBoundary && rightBoundary)
            return true;
        current += nameLength;
    }
    return false;
}

struct ImageSupport
{
    VkFormat format;
    VkImageUsageFlags usage;
    const char* label;
};

bool queryImageSupport(VkPhysicalDevice physicalDevice, const ImageSupport& request)
{
    const VkPhysicalDeviceExternalImageFormatInfo externalInfo{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO,
        nullptr,
        VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT};
    const VkPhysicalDeviceImageFormatInfo2 imageInfo{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
        &externalInfo,
        request.format,
        VK_IMAGE_TYPE_2D,
        VK_IMAGE_TILING_OPTIMAL,
        request.usage,
        VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT};
    VkExternalImageFormatProperties externalProperties{};
    externalProperties.sType = VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES;
    VkImageFormatProperties2 properties{};
    properties.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2;
    properties.pNext = &externalProperties;
    const VkResult result = vkGetPhysicalDeviceImageFormatProperties2(physicalDevice, &imageInfo, &properties);
    if(result != VK_SUCCESS)
    {
        std::printf("image %-18s external query failed: %d\n", request.label, result);
        return false;
    }

    const VkExternalMemoryFeatureFlags features = externalProperties.externalMemoryProperties.externalMemoryFeatures;
    const bool dedicatedOnly = (features & VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT) != 0;
    const bool exportable = (features & VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT) != 0;
    const bool importable = (features & VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT) != 0;
    const bool opaqueFdCompatible =
        (externalProperties.externalMemoryProperties.compatibleHandleTypes & VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT) != 0;
    std::printf("image %-18s export=%d import=%d opaqueFdCompatible=%d dedicatedOnly=%d max=%ux%u\n",
        request.label, exportable, importable, opaqueFdCompatible, dedicatedOnly,
        properties.imageFormatProperties.maxExtent.width, properties.imageFormatProperties.maxExtent.height);
    return exportable && importable && opaqueFdCompatible;
}
}

int main()
{
    if(SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    SDL_Window* window = SDL_CreateWindow("GL/Vulkan external interop capability probe",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 64, 64,
        SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    SDL_GLContext context = window ? SDL_GL_CreateContext(window) : nullptr;
    if(!context)
    {
        std::fprintf(stderr, "OpenGL context creation failed: %s\n", SDL_GetError());
        if(window) SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    const char* glRenderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    const char* glVendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    const char* glVersion = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    const char* glExtensions = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
    const bool glMemoryFd = glExtensions && hasGlExtension(glExtensions, "GL_EXT_memory_object_fd");
    const bool glSemaphoreFd = glExtensions && hasGlExtension(glExtensions, "GL_EXT_semaphore_fd");

    using GetUnsignedBytev = void(APIENTRYP)(GLenum, GLubyte*);
    const auto getUnsignedBytev = reinterpret_cast<GetUnsignedBytev>(SDL_GL_GetProcAddress("glGetUnsignedBytevEXT"));
    std::array<GLubyte, VK_UUID_SIZE> glDeviceUuid{};
    std::array<GLubyte, VK_UUID_SIZE> glDriverUuid{};
    constexpr GLenum GL_DEVICE_UUID_EXT_VALUE = 0x9597;
    constexpr GLenum GL_DRIVER_UUID_EXT_VALUE = 0x9598;
    const bool glHasUuid = getUnsignedBytev && glExtensions && hasGlExtension(glExtensions, "GL_EXT_memory_object");
    if(glHasUuid)
    {
        getUnsignedBytev(GL_DEVICE_UUID_EXT_VALUE, glDeviceUuid.data());
        getUnsignedBytev(GL_DRIVER_UUID_EXT_VALUE, glDriverUuid.data());
    }

    std::printf("GL vendor=%s\nGL renderer=%s\nGL version=%s\nGL memory_fd=%d semaphore_fd=%d UUID=%s",
        glVendor ? glVendor : "unknown", glRenderer ? glRenderer : "unknown", glVersion ? glVersion : "unknown",
        glMemoryFd, glSemaphoreFd, glHasUuid ? "" : "unavailable\n");
    if(glHasUuid)
    {
        for(GLubyte byte : glDeviceUuid)
            std::printf("%02x", byte);
        std::printf(" driverUUID=");
        for(GLubyte byte : glDriverUuid)
            std::printf("%02x", byte);
        std::printf("\n");
    }

    VkApplicationInfo applicationInfo{};
    applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.pApplicationName = "Quake VR GL/Vulkan interop capability probe";
    applicationInfo.apiVersion = VK_API_VERSION_1_1;
    VkInstanceCreateInfo instanceInfo{};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &applicationInfo;
    VkInstance instance = VK_NULL_HANDLE;
    VkResult result = vkCreateInstance(&instanceInfo, nullptr, &instance);
    if(result != VK_SUCCESS)
    {
        std::fprintf(stderr, "vkCreateInstance failed: %d\n", result);
        SDL_GL_DeleteContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    uint32_t deviceCount = 0;
    result = vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if(result != VK_SUCCESS || deviceCount == 0)
    {
        std::fprintf(stderr, "No Vulkan physical devices: %d\n", result);
        vkDestroyInstance(instance, nullptr);
        SDL_GL_DeleteContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    result = vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
    bool matchedDevice = false;
    bool foundUsableDevice = false;

    for(VkPhysicalDevice physicalDevice : devices)
    {
        VkPhysicalDeviceIDProperties idProperties{};
        idProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
        VkPhysicalDeviceProperties2 properties{};
        properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        properties.pNext = &idProperties;
        vkGetPhysicalDeviceProperties2(physicalDevice, &properties);
        const bool uuidMatches = glHasUuid && std::memcmp(glDeviceUuid.data(), idProperties.deviceUUID, VK_UUID_SIZE) == 0;
        const bool driverUuidMatches = glHasUuid && std::memcmp(glDriverUuid.data(), idProperties.driverUUID, VK_UUID_SIZE) == 0;

        uint32_t extensionCount = 0;
        result = vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
        if(result != VK_SUCCESS)
            continue;
        std::vector<VkExtensionProperties> extensions(extensionCount);
        result = vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, extensions.data());
        if(result != VK_SUCCESS)
            continue;

        const bool memoryFd = hasExtension(extensions, VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME);
        const bool semaphoreFd = hasExtension(extensions, VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME);
        std::printf("VK device=%s UUID=", properties.properties.deviceName);
        for(uint8_t byte : idProperties.deviceUUID)
            std::printf("%02x", byte);
        std::printf(" GL_UUID_match=%d driver_UUID_match=%d memory_fd=%d semaphore_fd=%d\n",
            uuidMatches, driverUuidMatches, memoryFd, semaphoreFd);

        if(!uuidMatches || !driverUuidMatches || !memoryFd || !semaphoreFd)
            continue;

        const std::array<ImageSupport, 3> requests{{
            {VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, "RGBA8 color"},
            {VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, "RGBA16F motion"},
            {VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, "D32 depth"}}};
        bool imagesShareable = true;
        for(const auto& request : requests)
            imagesShareable &= queryImageSupport(physicalDevice, request);

        const VkPhysicalDeviceExternalSemaphoreInfo semaphoreInfo{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO,
            nullptr,
            VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT};
        VkExternalSemaphoreProperties semaphoreProperties{};
        semaphoreProperties.sType = VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES;
        vkGetPhysicalDeviceExternalSemaphoreProperties(physicalDevice, &semaphoreInfo, &semaphoreProperties);
        const VkExternalSemaphoreFeatureFlags semaphoreFeatures = semaphoreProperties.externalSemaphoreFeatures;
        const bool semaphoreExportable = (semaphoreFeatures & VK_EXTERNAL_SEMAPHORE_FEATURE_EXPORTABLE_BIT) != 0;
        const bool semaphoreImportable = (semaphoreFeatures & VK_EXTERNAL_SEMAPHORE_FEATURE_IMPORTABLE_BIT) != 0;
        const bool semaphoreCompatible =
            (semaphoreProperties.compatibleHandleTypes & VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT) != 0;
        std::printf("semaphore opaqueFd export=%d import=%d compatible=%d\n",
            semaphoreExportable, semaphoreImportable, semaphoreCompatible);

        matchedDevice = true;
        foundUsableDevice = glMemoryFd && glSemaphoreFd && imagesShareable && semaphoreExportable &&
            semaphoreImportable && semaphoreCompatible;
    }

    vkDestroyInstance(instance, nullptr);
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    std::printf("capability_probe=%s\n", foundUsableDevice ? "PASS" : (matchedDevice ? "PARTIAL" : "NO_MATCHING_DEVICE"));
    return foundUsableDevice ? 0 : 2;
}
