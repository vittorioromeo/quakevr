// Standalone Linux GL/Vulkan external-memory round trip. No OpenXR or game.
// Build: clang++ -std=c++17 -O2 -Wall -Wextra -Werror
//   -Wno-missing-field-initializers vr_gl_vk_interop_roundtrip.cpp
//   $(pkg-config --cflags --libs sdl2 gl vulkan) -o interop_roundtrip
// Run under an external timeout: DISPLAY=:1 timeout 30s ./interop_roundtrip
// Default mode checks compatible image storage only. The opt-in
// --gpu-handoff path uses exact captured Zink image parameters and paired
// Vulkan EXTERNAL ownership barriers; it is a diagnostic, not a production
// transport path until full pixel and OpenXR swapchain tests pass.
// Diagnostic readbacks intentionally serialize each iteration; NOT a benchmark.
#define GL_GLEXT_PROTOTYPES
#include <SDL.h>
#include <SDL_opengl.h>
#include <vulkan/vulkan.h>
#include <unistd.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr uint32_t width = 64, height = 64, iterations = 16;
constexpr size_t bytes = width * height * 4;
void require(bool value, const char* what) { if(!value) throw std::runtime_error(what); }
void vkCheck(VkResult value, const char* what)
{
    if(value != VK_SUCCESS) throw std::runtime_error(std::string(what) + ": VkResult=" + std::to_string(value));
}
void glCheck(const char* what)
{
    const GLenum error = glGetError();
    if(error != GL_NO_ERROR) throw std::runtime_error(std::string(what) + ": GLerror=" + std::to_string(error));
}
template<class T> T proc(const char* name)
{
    auto value = reinterpret_cast<T>(SDL_GL_GetProcAddress(name));
    require(value != nullptr, name);
    return value;
}
void uuid(const char* label, const uint8_t* value)
{
    std::printf("%s=", label);
    for(unsigned i = 0; i < VK_UUID_SIZE; ++i) std::printf("%02x", value[i]);
    std::printf("\n");
}
bool extension(const char* name) { return SDL_GL_ExtensionSupported(name) == SDL_TRUE; }
uint32_t memoryType(const VkPhysicalDeviceMemoryProperties& properties, uint32_t bits, VkMemoryPropertyFlags flags)
{
    for(uint32_t i = 0; i < properties.memoryTypeCount; ++i)
        if((bits & (1u << i)) && (properties.memoryTypes[i].propertyFlags & flags) == flags) return i;
    throw std::runtime_error("no matching memory type");
}
std::array<uint8_t, 4> pattern(unsigned iteration, unsigned quadrant)
{
    return {{static_cast<uint8_t>((17 + iteration * 13 + quadrant * 47) % 256),
             static_cast<uint8_t>((83 + iteration * 29 + quadrant * 71) % 256),
             static_cast<uint8_t>((191 + iteration * 7 + quadrant * 31) % 256), 255}};
}
void printPixel(const char* label, const uint8_t* value)
{
    std::printf("%s=%u,%u,%u,%u", label, value[0], value[1], value[2], value[3]);
}

void diagnoseBufferImport(VkPhysicalDevice physical, VkDevice device,
    const VkPhysicalDeviceMemoryProperties& memories, PFN_vkGetMemoryFdKHR getMemoryFd)
{
    const auto createMemory = proc<PFNGLCREATEMEMORYOBJECTSEXTPROC>("glCreateMemoryObjectsEXT");
    const auto memoryParameter = proc<PFNGLMEMORYOBJECTPARAMETERIVEXTPROC>("glMemoryObjectParameterivEXT");
    const auto getMemoryParameter = proc<PFNGLGETMEMORYOBJECTPARAMETERIVEXTPROC>("glGetMemoryObjectParameterivEXT");
    const auto importMemory = proc<PFNGLIMPORTMEMORYFDEXTPROC>("glImportMemoryFdEXT");
    const auto bufferStorage = proc<PFNGLBUFFERSTORAGEMEMEXTPROC>("glBufferStorageMemEXT");
    const auto deleteMemory = proc<PFNGLDELETEMEMORYOBJECTSEXTPROC>("glDeleteMemoryObjectsEXT");
    // Ordinary buffer uses supported by a Vulkan graphics device. No optional
    // transform-feedback/ray-tracing/device-address features are enabled.
    constexpr VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT |
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    VkPhysicalDeviceExternalBufferInfo query{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_BUFFER_INFO};
    query.usage = usage; query.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
    VkExternalBufferProperties external{VK_STRUCTURE_TYPE_EXTERNAL_BUFFER_PROPERTIES};
    vkGetPhysicalDeviceExternalBufferProperties(physical, &query, &external);
    const auto flags = external.externalMemoryProperties.externalMemoryFeatures;
    std::printf("buffer usage=0x%x externalMemoryFeatures=0x%x\n", usage, flags);
    require((flags & (VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT | VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT)) ==
        (VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT | VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT), "buffer is not exportable/importable");
    VkExternalMemoryBufferCreateInfo exportBuffer{VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO};
    exportBuffer.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO}; info.pNext = &exportBuffer;
    info.size = bytes; info.usage = usage; info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkBuffer buffer{}; vkCheck(vkCreateBuffer(device, &info, nullptr, &buffer), "create external buffer");
    VkMemoryRequirements requirements{}; vkGetBufferMemoryRequirements(device, buffer, &requirements);
    VkMemoryDedicatedAllocateInfo dedicated{VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO}; dedicated.buffer = buffer;
    VkExportMemoryAllocateInfo exportMemory{VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO};
    exportMemory.pNext = &dedicated; exportMemory.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO}; allocation.pNext = &exportMemory;
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = memoryType(memories, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    std::printf("buffer logicalSize=%zu requirementsSize=%llu alignment=%llu memoryTypeBits=0x%x allocationSize=%llu memoryType=%u\n",
        bytes, static_cast<unsigned long long>(requirements.size), static_cast<unsigned long long>(requirements.alignment),
        requirements.memoryTypeBits, static_cast<unsigned long long>(allocation.allocationSize), allocation.memoryTypeIndex);
    VkDeviceMemory memory{}; vkCheck(vkAllocateMemory(device, &allocation, nullptr, &memory), "allocate dedicated external buffer");
    vkCheck(vkBindBufferMemory(device, buffer, memory, 0), "bind external buffer");
    VkMemoryGetFdInfoKHR fdInfo{VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR};
    fdInfo.memory = memory; fdInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
    int fd = -1; vkCheck(getMemoryFd(device, &fdInfo, &fd), "export buffer FD");
    GLuint glMemory = 0, glBuffer = 0; createMemory(1, &glMemory);
    const GLint trueValue = GL_TRUE; memoryParameter(glMemory, GL_DEDICATED_MEMORY_OBJECT_EXT, &trueValue);
    glCheck("buffer GL dedicated parameter");
    importMemory(glMemory, allocation.allocationSize, GL_HANDLE_TYPE_OPAQUE_FD_EXT, fd);
    glCheck("buffer GL memory import");
    GLint dedicatedValue = 0; getMemoryParameter(glMemory, GL_DEDICATED_MEMORY_OBJECT_EXT, &dedicatedValue);
    glCheck("buffer GL dedicated query");
    std::printf("buffer GL_importSizeArgument=%llu GL_dedicated=%d\n",
        static_cast<unsigned long long>(allocation.allocationSize), dedicatedValue);
    glGenBuffers(1, &glBuffer); glBindBuffer(GL_ARRAY_BUFFER, glBuffer);
    bufferStorage(GL_ARRAY_BUFFER, bytes, glMemory, 0);
    glCheck("GL external buffer storage");
    GLint64 bufferSize = 0; glGetBufferParameteri64v(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &bufferSize);
    glCheck("GL buffer size query"); require(bufferSize == static_cast<GLint64>(bytes), "GL buffer size mismatch");
    std::printf("buffer GL_BUFFER_SIZE=%lld storageImport=PASS GPUAccess=NOT_TESTED\n", static_cast<long long>(bufferSize));
    glBindBuffer(GL_ARRAY_BUFFER, 0); glDeleteBuffers(1, &glBuffer); deleteMemory(1, &glMemory);
    glCheck("buffer GL cleanup"); vkDestroyBuffer(device, buffer, nullptr); vkFreeMemory(device, memory, nullptr);
}

void diagnoseImageLayouts(VkPhysicalDevice physical, VkDevice device, const VkImageCreateInfo& original)
{
    // Create unbound Vulkan image objects solely to query their requirements.
    // No allocation, GL import, queue submission, or image data access occurs.
    const VkFormat formats[] = {VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_SRGB};
    const char* labels[] = {"no-list", "UNORM-only", "UNORM+SRGB", "UNORM+SRGB-extended-usage"};
    for(unsigned variant = 0; variant < 4; ++variant)
    {
        VkImageFormatListCreateInfo list{VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO};
        list.viewFormatCount = variant == 1 ? 1 : 2; list.pViewFormats = formats;
        VkExternalMemoryImageCreateInfo externalImage{VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO};
        externalImage.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
        externalImage.pNext = variant ? &list : nullptr;
        VkImageCreateInfo info = original; info.pNext = &externalImage;
        if(variant == 3) info.flags |= VK_IMAGE_CREATE_EXTENDED_USAGE_BIT;
        VkPhysicalDeviceExternalImageFormatInfo externalQuery{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO};
        externalQuery.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
        externalQuery.pNext = variant ? &list : nullptr;
        VkPhysicalDeviceImageFormatInfo2 query{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2};
        query.pNext = &externalQuery; query.format = info.format; query.type = info.imageType;
        query.tiling = info.tiling; query.usage = info.usage; query.flags = info.flags;
        VkExternalImageFormatProperties externalProperties{VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES};
        VkImageFormatProperties2 properties{VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2}; properties.pNext = &externalProperties;
        const VkResult queryResult = vkGetPhysicalDeviceImageFormatProperties2(physical, &query, &properties);
        std::printf("imageRequirementsProbe=%s flags=0x%x usage=0x%x queryResult=%d", labels[variant], info.flags, info.usage, queryResult);
        if(queryResult != VK_SUCCESS) { std::printf("\n"); continue; }
        VkImage image{}; vkCheck(vkCreateImage(device, &info, nullptr, &image), "create unbound requirements probe image");
        VkMemoryRequirements requirements{}; vkGetImageMemoryRequirements(device, image, &requirements);
        std::printf(" externalFeatures=0x%x size=%llu alignment=%llu memoryTypeBits=0x%x\n",
            externalProperties.externalMemoryProperties.externalMemoryFeatures,
            static_cast<unsigned long long>(requirements.size), static_cast<unsigned long long>(requirements.alignment), requirements.memoryTypeBits);
        vkDestroyImage(device, image, nullptr);
    }
}

int run(bool glTraceOnly, bool matchZinkImage, bool gpuHandoffEnabled)
{
    // Deliberately requires a source change before retrying image storage.
    constexpr bool imageRetryEnabled = true;
    std::printf("mode=%s formatList=%s matchZinkImage=%d GPUHandoff=%s\n",
        glTraceOnly ? "GL-only-semaphore-trace" : "image-storage-test",
        matchZinkImage ? "UNORM+A8B8G8R8_SRGB_PACK32" : "UNORM+SRGB", matchZinkImage,
        gpuHandoffEnabled ? "enabled" : "disabled");
    require(!gpuHandoffEnabled || matchZinkImage, "GPU handoff requires --match-zink-image");
    require(SDL_Init(SDL_INIT_VIDEO) == 0, SDL_GetError());
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    SDL_Window* window = SDL_CreateWindow("Quake VR isolated interop fixture", SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    require(window != nullptr, SDL_GetError());
    SDL_GLContext context = SDL_GL_CreateContext(window);
    require(context != nullptr, SDL_GetError());
    std::printf("GL renderer=%s\nGL version=%s\n", glGetString(GL_RENDERER), glGetString(GL_VERSION));
    require(extension("GL_EXT_memory_object") && extension("GL_EXT_memory_object_fd") &&
        extension("GL_EXT_semaphore") && extension("GL_EXT_semaphore_fd"), "required GL extensions missing");

#define GLPROC(type, name) const auto name = proc<type>(#name)
    GLPROC(PFNGLGETUNSIGNEDBYTEVEXTPROC, glGetUnsignedBytevEXT);
    GLPROC(PFNGLGETUNSIGNEDBYTEI_VEXTPROC, glGetUnsignedBytei_vEXT);
    GLPROC(PFNGLCREATEMEMORYOBJECTSEXTPROC, glCreateMemoryObjectsEXT);
    GLPROC(PFNGLMEMORYOBJECTPARAMETERIVEXTPROC, glMemoryObjectParameterivEXT);
    GLPROC(PFNGLIMPORTMEMORYFDEXTPROC, glImportMemoryFdEXT);
    GLPROC(PFNGLTEXSTORAGEMEM2DEXTPROC, glTexStorageMem2DEXT);
    GLPROC(PFNGLDELETEMEMORYOBJECTSEXTPROC, glDeleteMemoryObjectsEXT);
    GLPROC(PFNGLGENSEMAPHORESEXTPROC, glGenSemaphoresEXT);
    GLPROC(PFNGLIMPORTSEMAPHOREFDEXTPROC, glImportSemaphoreFdEXT);
    GLPROC(PFNGLSIGNALSEMAPHOREEXTPROC, glSignalSemaphoreEXT);
    GLPROC(PFNGLWAITSEMAPHOREEXTPROC, glWaitSemaphoreEXT);
    GLPROC(PFNGLDELETESEMAPHORESEXTPROC, glDeleteSemaphoresEXT);
#undef GLPROC
    GLint deviceCountGL = 0;
    glGetIntegerv(GL_NUM_DEVICE_UUIDS_EXT, &deviceCountGL);
    require(deviceCountGL == 1, "fixture requires exactly one GL device");
    std::array<uint8_t, VK_UUID_SIZE> glDevice{}, glDriver{};
    glGetUnsignedBytei_vEXT(GL_DEVICE_UUID_EXT, 0, glDevice.data());
    glGetUnsignedBytevEXT(GL_DRIVER_UUID_EXT, glDriver.data());
    glCheck("GL UUID query");
    uuid("GL deviceUUID", glDevice.data()); uuid("GL driverUUID", glDriver.data());
    GLint tilingCount = 0;
    glGetInternalformativ(GL_TEXTURE_2D, GL_RGBA8, GL_NUM_TILING_TYPES_EXT, 1, &tilingCount);
    require(tilingCount > 0, "no GL external image tiling modes");
    std::vector<GLint> tilings(static_cast<size_t>(tilingCount));
    glGetInternalformativ(GL_TEXTURE_2D, GL_RGBA8, GL_TILING_TYPES_EXT, tilingCount, tilings.data());
    bool optimal = false;
    for(GLint tiling : tilings) optimal |= tiling == GL_OPTIMAL_TILING_EXT;
    require(optimal, "GL optimal external image tiling unavailable"); glCheck("GL tiling query");

    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "Quake VR isolated GL Vulkan interop fixture";
    app.apiVersion = VK_API_VERSION_1_1;
    VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO}; ici.pApplicationInfo = &app;
    VkInstance instance{}; vkCheck(vkCreateInstance(&ici, nullptr, &instance), "vkCreateInstance");
    uint32_t count = 0; vkCheck(vkEnumeratePhysicalDevices(instance, &count, nullptr), "enumerate device count");
    std::vector<VkPhysicalDevice> physicals(count);
    vkCheck(vkEnumeratePhysicalDevices(instance, &count, physicals.data()), "enumerate devices");
    VkPhysicalDevice physical{};
    for(auto candidate : physicals)
    {
        VkPhysicalDeviceIDProperties ids{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES};
        VkPhysicalDeviceProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2}; properties.pNext = &ids;
        vkGetPhysicalDeviceProperties2(candidate, &properties);
        if(std::memcmp(ids.deviceUUID, glDevice.data(), VK_UUID_SIZE) != 0) continue;
        std::printf("VK device=%s\n", properties.properties.deviceName);
        uuid("VK deviceUUID", ids.deviceUUID); uuid("VK driverUUID", ids.driverUUID);
        require(std::memcmp(ids.driverUUID, glDriver.data(), VK_UUID_SIZE) == 0, "GL/Vulkan driver UUID mismatch");
        physical = candidate; break;
    }
    require(physical != VK_NULL_HANDLE, "no Vulkan device matches GL UUID");
    vkCheck(vkEnumerateDeviceExtensionProperties(physical, nullptr, &count, nullptr), "device extension count");
    std::vector<VkExtensionProperties> extensions(count);
    vkCheck(vkEnumerateDeviceExtensionProperties(physical, nullptr, &count, extensions.data()), "device extensions");
    std::vector<const char*> enabled{VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
        VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME, VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME};
    for(const char* name : enabled)
    {
        bool found = false;
        for(const auto& entry : extensions) found |= std::strcmp(name, entry.extensionName) == 0;
        require(found, name);
    }
    // EXT_external_objects issues 13/17: every supported ordinary usage and
    // MUTABLE_FORMAT are needed for GL texture aliasing (GL 4.3+ texture views).
    VkFormatProperties format{}; vkGetPhysicalDeviceFormatProperties(physical, VK_FORMAT_R8G8B8A8_UNORM, &format);
    const VkFormatFeatureFlags features = format.optimalTilingFeatures;
    VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if(features & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if(features & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) usage |= VK_IMAGE_USAGE_STORAGE_BIT;
    if(features & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    require((usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) != 0, "RGBA8 is not renderable");
    const VkFormat viewFormatsStandard[] = {VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_SRGB};
    const VkFormat viewFormatsZink[] = {VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_A8B8G8R8_SRGB_PACK32};
    const VkFormat* viewFormats = matchZinkImage ? viewFormatsZink : viewFormatsStandard;
    if(matchZinkImage)
    {
        const char* extraExtensions[] = {
            VK_EXT_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_EXTENSION_NAME,
            VK_EXT_ATTACHMENT_FEEDBACK_LOOP_LAYOUT_EXTENSION_NAME};
        for(const char* name : extraExtensions)
        {
            bool found = false;
            for(const auto& entry : extensions) found |= std::strcmp(name, entry.extensionName) == 0;
            require(found, name);
            enabled.push_back(name);
        }
        // Exact usage captured from the installed Zink VkImageCreateInfo:
        // 0x80097 (no STORAGE_IMAGE_BIT).
        usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
            VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_ATTACHMENT_FEEDBACK_LOOP_BIT_EXT;
    }
    VkImageFormatListCreateInfo formatList{VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO};
    formatList.viewFormatCount = 2; formatList.pViewFormats = viewFormats;
    VkPhysicalDeviceExternalImageFormatInfo externalQuery{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO};
    externalQuery.pNext = &formatList;
    externalQuery.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
    VkPhysicalDeviceImageFormatInfo2 query{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2};
    query.pNext = &externalQuery; query.format = VK_FORMAT_R8G8B8A8_UNORM; query.type = VK_IMAGE_TYPE_2D;
    query.tiling = VK_IMAGE_TILING_OPTIMAL; query.usage = usage;
    query.flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT |
        (matchZinkImage ? VK_IMAGE_CREATE_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_BIT_EXT : 0);
    VkExternalImageFormatProperties externalProperties{VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES};
    VkImageFormatProperties2 imageProperties{VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2}; imageProperties.pNext = &externalProperties;
    vkCheck(vkGetPhysicalDeviceImageFormatProperties2(physical, &query, &imageProperties), "external mutable RGBA8 query");
    const auto memoryFeatures = externalProperties.externalMemoryProperties.externalMemoryFeatures;
    require((memoryFeatures & (VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT | VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT)) ==
        (VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT | VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT), "RGBA8 is not exportable/importable");
    require(imageProperties.imageFormatProperties.maxExtent.width >= width &&
        imageProperties.imageFormatProperties.maxExtent.height >= height, "image extent unsupported");
    VkPhysicalDeviceExternalSemaphoreInfo semQuery{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO};
    semQuery.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
    VkExternalSemaphoreProperties semProperties{VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES};
    vkGetPhysicalDeviceExternalSemaphoreProperties(physical, &semQuery, &semProperties);
    require((semProperties.externalSemaphoreFeatures & 3) == 3, "binary OPAQUE_FD semaphore unavailable");
    std::printf("RGBA8 mutable=1 usage=0x%x externalMemoryFeatures=0x%x dedicated=1 optimal=1\n", usage, memoryFeatures);

    vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, nullptr);
    std::vector<VkQueueFamilyProperties> queues(count); vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, queues.data());
    uint32_t family = count;
    for(uint32_t i = 0; i < count; ++i) if(queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) { family = i; break; }
    require(family < count, "no graphics queue");
    float priority = 1;
    VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qci.queueFamilyIndex = family; qci.queueCount = 1; qci.pQueuePriorities = &priority;
    VkPhysicalDeviceAttachmentFeedbackLoopLayoutFeaturesEXT feedbackQuery{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ATTACHMENT_FEEDBACK_LOOP_LAYOUT_FEATURES_EXT};
    VkPhysicalDeviceMultisampledRenderToSingleSampledFeaturesEXT multisampleQuery{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_FEATURES_EXT};
    if(matchZinkImage)
    {
        multisampleQuery.pNext = &feedbackQuery;
        VkPhysicalDeviceFeatures2 features2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
        features2.pNext = &multisampleQuery;
        vkGetPhysicalDeviceFeatures2(physical, &features2);
        require(multisampleQuery.multisampledRenderToSingleSampled == VK_TRUE,
            "multisampledRenderToSingleSampled feature unavailable");
        require(feedbackQuery.attachmentFeedbackLoopLayout == VK_TRUE,
            "attachmentFeedbackLoopLayout feature unavailable");
    }
    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO}; dci.queueCreateInfoCount = 1; dci.pQueueCreateInfos = &qci;
    dci.enabledExtensionCount = static_cast<uint32_t>(enabled.size()); dci.ppEnabledExtensionNames = enabled.data();
    VkPhysicalDeviceAttachmentFeedbackLoopLayoutFeaturesEXT feedbackEnable{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ATTACHMENT_FEEDBACK_LOOP_LAYOUT_FEATURES_EXT};
    VkPhysicalDeviceMultisampledRenderToSingleSampledFeaturesEXT multisampleEnable{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_FEATURES_EXT};
    if(matchZinkImage)
    {
        multisampleEnable.multisampledRenderToSingleSampled = VK_TRUE;
        multisampleEnable.pNext = &feedbackEnable;
        feedbackEnable.attachmentFeedbackLoopLayout = VK_TRUE;
        dci.pNext = &multisampleEnable;
    }
    VkDevice device{}; vkCheck(vkCreateDevice(physical, &dci, nullptr, &device), "vkCreateDevice");
    VkQueue queue{}; vkGetDeviceQueue(device, family, 0, &queue);
    const auto getMemoryFd = reinterpret_cast<PFN_vkGetMemoryFdKHR>(vkGetDeviceProcAddr(device, "vkGetMemoryFdKHR"));
    const auto getSemaphoreFd = reinterpret_cast<PFN_vkGetSemaphoreFdKHR>(vkGetDeviceProcAddr(device, "vkGetSemaphoreFdKHR"));
    require(getMemoryFd && getSemaphoreFd, "missing Vulkan FD entrypoints");
    VkPhysicalDeviceMemoryProperties memories{}; vkGetPhysicalDeviceMemoryProperties(physical, &memories);
    VkExternalMemoryImageCreateInfo externalImage{VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO};
    externalImage.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
    // Zink reconstructs GL_RGBA8 as a mutable Vulkan image with this explicit
    // compatible view-format list. Without it Turnip queried 16KB while Zink
    // required 20KB for the same 64x64 texture (different storage layout).
    externalImage.pNext = &formatList;
    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO}; imageInfo.pNext = &externalImage;
    imageInfo.flags = query.flags; imageInfo.imageType = VK_IMAGE_TYPE_2D; imageInfo.format = query.format;
    imageInfo.extent = {width, height, 1}; imageInfo.mipLevels = 1; imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT; imageInfo.tiling = query.tiling; imageInfo.usage = usage;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if(!imageRetryEnabled) diagnoseImageLayouts(physical, device, imageInfo);
    VkImage image{}; vkCheck(vkCreateImage(device, &imageInfo, nullptr, &image), "vkCreateImage");
    VkMemoryRequirements requirements{}; vkGetImageMemoryRequirements(device, image, &requirements);
    VkMemoryDedicatedAllocateInfo dedicated{VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO}; dedicated.image = image;
    VkExportMemoryAllocateInfo exportMemory{VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO};
    exportMemory.pNext = &dedicated; exportMemory.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO}; allocation.pNext = &exportMemory;
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = memoryType(memories, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    std::printf("image requirementsSize=%llu alignment=%llu memoryTypeBits=0x%x allocationSize=%llu memoryType=%u\n",
        static_cast<unsigned long long>(requirements.size), static_cast<unsigned long long>(requirements.alignment),
        requirements.memoryTypeBits, static_cast<unsigned long long>(allocation.allocationSize), allocation.memoryTypeIndex);
    VkDeviceMemory imageMemory{}; vkCheck(vkAllocateMemory(device, &allocation, nullptr, &imageMemory), "allocate dedicated image");
    vkCheck(vkBindImageMemory(device, image, imageMemory, 0), "bind dedicated image");
    VkMemoryGetFdInfoKHR memoryFdInfo{VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR};
    memoryFdInfo.memory = imageMemory; memoryFdInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
    int fd = -1; vkCheck(getMemoryFd(device, &memoryFdInfo, &fd), "export image FD");
    GLuint glMemory = 0, texture = 0, fbo = 0;
    glCreateMemoryObjectsEXT(1, &glMemory);
    const GLint trueValue = GL_TRUE; glMemoryObjectParameterivEXT(glMemory, GL_DEDICATED_MEMORY_OBJECT_EXT, &trueValue);
    glCheck("GL dedicated memory parameter");
    glImportMemoryFdEXT(glMemory, requirements.size, GL_HANDLE_TYPE_OPAQUE_FD_EXT, fd);
    glCheck("GL import memory FD"); // Successful import transfers FD ownership to GL.
    fd = -1;
    if(!imageRetryEnabled)
    {
        const auto getMemoryParameter = proc<PFNGLGETMEMORYOBJECTPARAMETERIVEXTPROC>("glGetMemoryObjectParameterivEXT");
        GLint dedicatedValue = 0; getMemoryParameter(glMemory, GL_DEDICATED_MEMORY_OBJECT_EXT, &dedicatedValue);
        glCheck("image GL dedicated query");
        // EXT_external_objects does not define GL_MEMORY_OBJECT_SIZE_EXT or
        // any memory-object allocation-size query. Do not invent an enum.
        std::printf("image GL_importSizeArgument=%llu GL_dedicated=%d GL_allocationSizeQuery=NOT_DEFINED_BY_EXT\n",
            static_cast<unsigned long long>(requirements.size), dedicatedValue);
        std::printf("image glTexStorageMem2DEXT=SKIPPED (previous import failure)\n");
        glDeleteMemoryObjectsEXT(1, &glMemory); glCheck("image memory diagnostic cleanup");
        vkDestroyImage(device, image, nullptr); vkFreeMemory(device, imageMemory, nullptr);
        diagnoseBufferImport(physical, device, memories, getMemoryFd);
        vkDestroyDevice(device, nullptr); vkDestroyInstance(instance, nullptr);
        SDL_GL_DeleteContext(context); SDL_DestroyWindow(window); SDL_Quit();
        std::printf("import_diagnostics=PASS imageStorage=NOT_RETRIED GPUHandoff=NOT_TESTED\n");
        return 0;
    }
    glGenTextures(1, &texture); glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_TILING_EXT, GL_OPTIMAL_TILING_EXT);
    glTexStorageMem2DEXT(GL_TEXTURE_2D, 1, GL_RGBA8, width, height, glMemory, 0);
    glCheck("GL external texture storage");
    if(!gpuHandoffEnabled && !glTraceOnly)
    {
        GLint textureWidth = 0, textureHeight = 0;
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &textureWidth);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &textureHeight);
        glCheck("GL imported texture extent");
        require(textureWidth == static_cast<GLint>(width) && textureHeight == static_cast<GLint>(height), "GL texture extent mismatch");
        std::printf("imageStorage=PASS GL_extent=%dx%d allocationSize=%llu dedicated=1 GPUHandoff=NOT_TESTED\n",
            textureWidth, textureHeight, static_cast<unsigned long long>(requirements.size));
        glBindTexture(GL_TEXTURE_2D, 0); glDeleteTextures(1, &texture); glDeleteMemoryObjectsEXT(1, &glMemory);
        glCheck("GL image storage cleanup");
        vkDestroyImage(device, image, nullptr); vkFreeMemory(device, imageMemory, nullptr);
        vkDestroyDevice(device, nullptr); vkDestroyInstance(instance, nullptr);
        SDL_GL_DeleteContext(context); SDL_DestroyWindow(window); SDL_Quit();
        return 0;
    }
    glGenFramebuffers(1, &fbo); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    glDrawBuffer(GL_COLOR_ATTACHMENT0); glReadBuffer(GL_COLOR_ATTACHMENT0);
    require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "external GL framebuffer incomplete");
    glDisable(GL_FRAMEBUFFER_SRGB); glDisable(GL_DITHER); glDisable(GL_BLEND);
    glPixelStorei(GL_PACK_ALIGNMENT, 1); glViewport(0, 0, width, height);
    glCheck("GL external framebuffer");

    std::array<VkSemaphore, 2> semaphores{}; // 0: GL->VK, 1: VK->GL.
    std::array<GLuint, 2> glSemaphores{}; glGenSemaphoresEXT(2, glSemaphores.data());
    VkExportSemaphoreCreateInfo exportSemaphore{VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO};
    exportSemaphore.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
    VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO}; sci.pNext = &exportSemaphore;
    for(unsigned i = 0; i < 2; ++i)
    {
        vkCheck(vkCreateSemaphore(device, &sci, nullptr, &semaphores[i]), "create binary semaphore");
        VkSemaphoreGetFdInfoKHR info{VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR};
        info.semaphore = semaphores[i]; info.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
        vkCheck(getSemaphoreFd(device, &info, &fd), "export semaphore FD");
        glImportSemaphoreFdEXT(glSemaphores[i], GL_HANDLE_TYPE_OPAQUE_FD_EXT, fd);
        glCheck("GL import semaphore FD"); fd = -1;
    }
    if(glTraceOnly)
    {
        // Every GPU access and semaphore signal/wait is issued by this single
        // GL context. The exporter VkDevice never submits a queue operation.
        // This observes what Zink emits without assuming an interop contract.
        const GLenum layouts[] = {GL_LAYOUT_GENERAL_EXT, GL_LAYOUT_SHADER_READ_ONLY_EXT};
        const char* labels[] = {"GENERAL", "SHADER_READ_ONLY"};
        for(unsigned iteration = 0; iteration < 2; ++iteration)
        {
            const auto expected = pattern(iteration, 0);
            glClearColor(expected[0] / 255.0f, expected[1] / 255.0f, expected[2] / 255.0f, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            std::printf("GL_ONLY_BEFORE_SIGNAL iteration=%u requestedLayout=%s\n", iteration, labels[iteration]);
            glSignalSemaphoreEXT(glSemaphores[iteration], 0, nullptr, 1, &texture, &layouts[iteration]);
            glCheck("GL-only signal");
            std::printf("GL_ONLY_AFTER_SIGNAL iteration=%u\n", iteration);
            glWaitSemaphoreEXT(glSemaphores[iteration], 0, nullptr, 1, &texture, &layouts[iteration]);
            glCheck("GL-only wait");
            std::array<uint8_t, 4> pixel{};
            glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
            glCheck("GL-only readback"); require(pixel == expected, "GL-only pixel mismatch");
            std::printf("GL_ONLY_AFTER_WAIT iteration=%u ", iteration); printPixel("pixel", pixel.data()); std::printf("\n");
        }
        glFinish(); glCheck("GL-only completion");
        glBindFramebuffer(GL_FRAMEBUFFER, 0); glDeleteFramebuffers(1, &fbo); glDeleteTextures(1, &texture);
        glDeleteMemoryObjectsEXT(1, &glMemory); glDeleteSemaphoresEXT(2, glSemaphores.data()); glCheck("GL-only cleanup");
        for(auto semaphore : semaphores) vkDestroySemaphore(device, semaphore, nullptr);
        vkDestroyImage(device, image, nullptr); vkFreeMemory(device, imageMemory, nullptr);
        vkDestroyDevice(device, nullptr); vkDestroyInstance(instance, nullptr);
        SDL_GL_DeleteContext(context); SDL_DestroyWindow(window); SDL_Quit();
        std::printf("GL_only_trace=PASS applicationVulkanQueueSubmits=0 crossAPI_GPUHandoff=NOT_TESTED\n");
        return 0;
    }
    VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO}; bci.size = bytes; bci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VkBuffer buffer{}; vkCheck(vkCreateBuffer(device, &bci, nullptr, &buffer), "create readback buffer");
    VkMemoryRequirements bufferRequirements{}; vkGetBufferMemoryRequirements(device, buffer, &bufferRequirements);
    allocation.pNext = nullptr; allocation.allocationSize = bufferRequirements.size;
    allocation.memoryTypeIndex = memoryType(memories, bufferRequirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VkDeviceMemory bufferMemory{}; vkCheck(vkAllocateMemory(device, &allocation, nullptr, &bufferMemory), "allocate readback buffer");
    vkCheck(vkBindBufferMemory(device, buffer, bufferMemory, 0), "bind readback buffer");
    void* mapped = nullptr; vkCheck(vkMapMemory(device, bufferMemory, 0, VK_WHOLE_SIZE, 0, &mapped), "map readback buffer");
    VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; pci.queueFamilyIndex = family;
    VkCommandPool pool{}; vkCheck(vkCreateCommandPool(device, &pci, nullptr, &pool), "create command pool");
    VkCommandBufferAllocateInfo cai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cai.commandPool = pool; cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; cai.commandBufferCount = 1;
    VkCommandBuffer command{}; vkCheck(vkAllocateCommandBuffers(device, &cai, &command), "allocate command buffer");
    VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence fence{}; vkCheck(vkCreateFence(device, &fci, nullptr, &fence), "create fence");
    constexpr uint32_t external = VK_QUEUE_FAMILY_EXTERNAL;
    std::printf("ownership=EXTERNAL externalLayout=GENERAL semaphores=OPAQUE_FD binary\n");
    auto begin = [&] {
        vkCheck(vkResetCommandBuffer(command, 0), "reset command buffer");
        VkCommandBufferBeginInfo info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkCheck(vkBeginCommandBuffer(command, &info), "begin command buffer");
    };
    auto barrier = [&](VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t source, uint32_t destination,
        VkAccessFlags srcAccess, VkAccessFlags dstAccess, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage) {
        VkImageMemoryBarrier value{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER}; value.srcAccessMask = srcAccess; value.dstAccessMask = dstAccess;
        value.oldLayout = oldLayout; value.newLayout = newLayout; value.srcQueueFamilyIndex = source; value.dstQueueFamilyIndex = destination;
        value.image = image; value.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCmdPipelineBarrier(command, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &value);
    };
    auto submit = [&](bool waitGL) {
        vkCheck(vkEndCommandBuffer(command), "end command buffer");
        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        VkSubmitInfo info{VK_STRUCTURE_TYPE_SUBMIT_INFO}; info.commandBufferCount = 1; info.pCommandBuffers = &command;
        info.waitSemaphoreCount = waitGL ? 1 : 0; info.pWaitSemaphores = semaphores.data(); info.pWaitDstStageMask = &waitStage;
        info.signalSemaphoreCount = 1; info.pSignalSemaphores = &semaphores[1];
        vkCheck(vkQueueSubmit(queue, 1, &info, fence), "submit Vulkan commands");
    };
    auto waitFence = [&] {
        vkCheck(vkWaitForFences(device, 1, &fence, VK_TRUE, 5000000000ULL), "wait Vulkan completion (5 second limit)");
        vkCheck(vkResetFences(device, 1, &fence), "reset fence");
    };
    const GLenum general = GL_LAYOUT_GENERAL_EXT;
    begin();
    barrier(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, family, external, 0, 0,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    submit(false);
    glWaitSemaphoreEXT(glSemaphores[1], 0, nullptr, 1, &texture, &general);
    glCheck("initial GL wait"); glFlush(); waitFence();

    std::vector<uint8_t> glPixels(bytes);
    for(unsigned iteration = 0; iteration < iterations; ++iteration)
    {
        glEnable(GL_SCISSOR_TEST);
        for(unsigned quadrant = 0; quadrant < 4; ++quadrant)
        {
            const auto color = pattern(iteration, quadrant);
            glScissor((quadrant & 1) * (width / 2), (quadrant >> 1) * (height / 2), width / 2, height / 2);
            glClearColor(color[0] / 255.0f, color[1] / 255.0f, color[2] / 255.0f, 1);
            glClear(GL_COLOR_BUFFER_BIT);
        }
        glDisable(GL_SCISSOR_TEST);
        glSignalSemaphoreEXT(glSemaphores[0], 0, nullptr, 1, &texture, &general);
        glCheck("GL quadrant render and signal"); glFlush();
        begin();
        barrier(VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, external, family, 0, VK_ACCESS_TRANSFER_READ_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
        barrier(VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
            VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
        VkBufferImageCopy copy{}; copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}; copy.imageExtent = {width, height, 1};
        vkCmdCopyImageToBuffer(command, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1, &copy);
        VkBufferMemoryBarrier host{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
        host.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        host.srcQueueFamilyIndex = host.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; host.buffer = buffer; host.size = VK_WHOLE_SIZE;
        vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, 1, &host, 0, nullptr);
        barrier(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
            VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
        const auto reverseColor = pattern(iteration, 4);
        VkClearColorValue clear{};
        for(unsigned channel = 0; channel < 4; ++channel) clear.float32[channel] = reverseColor[channel] / 255.0f;
        const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCmdClearColorImage(command, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear, 1, &range);
        barrier(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, family, external, VK_ACCESS_TRANSFER_WRITE_BIT, 0,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
        submit(true);
        // The reverse GL wait is queued before any Vulkan host fence wait.
        // Reading changed Vulkan pixels proves this return path, not just the
        // persistence of the original GL contents.
        glWaitSemaphoreEXT(glSemaphores[1], 0, nullptr, 1, &texture, &general);
        glCheck("reverse GL wait");
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, glPixels.data());
        glCheck("GL reverse readback"); waitFence();
        const auto* vkPixels = static_cast<const uint8_t*>(mapped);
        size_t vkMismatch = 0, glMismatch = 0;
        for(unsigned y = 0; y < height; ++y) for(unsigned x = 0; x < width; ++x)
        {
            const unsigned quadrant = (x >= width / 2 ? 1 : 0) + (y >= height / 2 ? 2 : 0);
            const auto expected = pattern(iteration, quadrant); const size_t offset = (y * width + x) * 4;
            vkMismatch += std::memcmp(vkPixels + offset, expected.data(), 4) != 0;
            glMismatch += std::memcmp(glPixels.data() + offset, reverseColor.data(), 4) != 0;
        }
        if(iteration == 0 || iteration + 1 == iterations || vkMismatch || glMismatch)
        {
            std::printf("iteration=%u ", iteration + 1);
            printPixel("VK_GL_q0", vkPixels); std::printf(" ");
            printPixel("VK_GL_q3", vkPixels + bytes - 4); std::printf(" ");
            printPixel("GL_VK_return", glPixels.data());
            std::printf(" vkMismatchedPixels=%zu glMismatchedPixels=%zu\n", vkMismatch, glMismatch);
        }
        require(vkMismatch == 0 && glMismatch == 0, "round-trip pixel mismatch (including row orientation)");
    }
    glFinish(); glCheck("final GL completion"); vkCheck(vkDeviceWaitIdle(device), "final Vulkan completion");
    glBindFramebuffer(GL_FRAMEBUFFER, 0); glDeleteFramebuffers(1, &fbo); glDeleteTextures(1, &texture);
    glDeleteMemoryObjectsEXT(1, &glMemory); glDeleteSemaphoresEXT(2, glSemaphores.data()); glCheck("GL cleanup");
    vkDestroyFence(device, fence, nullptr); vkDestroyCommandPool(device, pool, nullptr);
    vkUnmapMemory(device, bufferMemory); vkDestroyBuffer(device, buffer, nullptr); vkFreeMemory(device, bufferMemory, nullptr);
    for(auto semaphore : semaphores) vkDestroySemaphore(device, semaphore, nullptr);
    vkDestroyImage(device, image, nullptr); vkFreeMemory(device, imageMemory, nullptr);
    vkDestroyDevice(device, nullptr); vkDestroyInstance(instance, nullptr);
    SDL_GL_DeleteContext(context); SDL_DestroyWindow(window); SDL_Quit();
    std::printf("roundtrip=PASS iterations=%u pixelsCheckedEachDirection=%u sameMemoryRows=1\n", iterations, iterations * width * height);
    return 0;
}
} // namespace
int main(int argc, char** argv)
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    bool glTraceOnly = false;
    bool matchZinkImage = false;
    bool gpuHandoffEnabled = false;
    for(int i = 1; i < argc; ++i)
    {
        if(std::strcmp(argv[i], "--gl-trace") == 0) glTraceOnly = true;
        else if(std::strcmp(argv[i], "--match-zink-image") == 0) matchZinkImage = true;
        else if(std::strcmp(argv[i], "--gpu-handoff") == 0) gpuHandoffEnabled = true;
        else
        {
            std::fprintf(stderr, "usage: %s [--gl-trace] [--match-zink-image] [--gpu-handoff]\n", argv[0]);
            return 3;
        }
    }
    try { return run(glTraceOnly, matchZinkImage, gpuHandoffEnabled); }
    catch(const std::exception& error)
    {
        std::fprintf(stderr, "roundtrip=FAIL %s\n", error.what());
        // Error-path teardown must not block on an unsignaled external semaphore.
        // This isolated fixture exits immediately; the OS/driver reclaim handles.
        std::_Exit(2);
    }
}
