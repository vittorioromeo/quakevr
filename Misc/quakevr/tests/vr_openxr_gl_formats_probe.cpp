// Isolated Linux GL/Zink -> XR-bound Vulkan motion/depth image diagnostic.
// No game code, projection layers, synthesis metadata, or headset configuration.
// See vulkan_interop_probe/README.md for the required image-definition gate.
#define GL_GLEXT_PROTOTYPES
#define XR_USE_GRAPHICS_API_VULKAN
#include <SDL.h>
#include <SDL_opengl.h>
#include <GL/glext.h> // SDL's bundled declarations can predate FD interop.
#include <vulkan/vulkan.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <unistd.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr unsigned extent = 64;
void need(bool ok, const char* what) { if(!ok) throw std::runtime_error(what); }
void vk(VkResult r, const char* what) { if(r != VK_SUCCESS) throw std::runtime_error(std::string(what)+": VkResult="+std::to_string(r)); }
void xr(XrResult r, const char* what) { if(r != XR_SUCCESS) throw std::runtime_error(std::string(what)+": XrResult="+std::to_string(r)); }
void gl(const char* what) { const auto e=glGetError(); if(e) throw std::runtime_error(std::string(what)+": GLerror="+std::to_string(e)); }
template<class T> T gp(const char* name) { auto p=reinterpret_cast<T>(SDL_GL_GetProcAddress(name)); need(p!=nullptr,name); return p; }
template<class T> T xp(XrInstance instance,const char* name) { T p{}; xr(xrGetInstanceProcAddr(instance,name,reinterpret_cast<PFN_xrVoidFunction*>(&p)),name); need(p!=nullptr,name); return p; }
uint32_t number(const char* s) { char* end{}; const auto n=std::strtoull(s,&end,0); need(end!=s && !*end && n<=UINT32_MAX,"bad numeric argument"); return static_cast<uint32_t>(n); }
struct Options {
    bool depth=false, discover=false, storage=false, verified=false, flagsSet=false, usageSet=false, viewsSet=false;
    VkImageCreateFlags flags{}; VkImageUsageFlags usage{}; std::vector<VkFormat> views;
};
Options options(int argc,char** argv) {
    Options o; bool formatSet=false;
    for(int i=1;i<argc;++i) {
        const std::string a=argv[i];
        if(a=="--format" && i+1<argc) { const std::string f=argv[++i]; need(f=="motion" || f=="depth","format must be motion or depth"); o.depth=f=="depth"; formatSet=true; }
        else if(a=="--discover") o.discover=true;
        else if(a=="--storage-only") o.storage=true;
        else if(a=="--verified-image-definition") o.verified=true;
        else if(a=="--flags" && i+1<argc) { o.flags=number(argv[++i]); o.flagsSet=true; }
        else if(a=="--usage" && i+1<argc) { o.usage=number(argv[++i]); o.usageSet=true; }
        else if(a=="--views" && i+1<argc) {
            const std::string value=argv[++i]; o.viewsSet=true;
            if(value!="none") { std::istringstream in(value); std::string part; while(std::getline(in,part,',')) o.views.push_back(static_cast<VkFormat>(number(part.c_str()))); }
        } else throw std::runtime_error("usage: --format motion|depth [--discover | --flags N --usage N --views none|N,N (--storage-only | --verified-image-definition)]");
    }
    need(formatSet,"--format is required");
    if(!o.discover) {
        need(o.flagsSet && o.usageSet && o.viewsSet,"explicit captured flags, usage and view list are required");
        need(o.storage || o.verified,"GPU access requires --verified-image-definition after comparing the storage API dump");
        const VkImageUsageFlags required=VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
            (o.depth?VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT:VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
        need((o.usage & required)==required,"captured image must support sampled, transfer and matching attachment usage");
    }
    return o;
}
uint32_t memoryType(VkPhysicalDevice p,uint32_t bits,VkMemoryPropertyFlags flags) {
    VkPhysicalDeviceMemoryProperties props{}; vkGetPhysicalDeviceMemoryProperties(p,&props);
    for(unsigned i=0;i<props.memoryTypeCount;++i) if((bits&(1u<<i)) && (props.memoryTypes[i].propertyFlags&flags)==flags) return i;
    throw std::runtime_error("required memory type unavailable");
}
std::vector<std::string> words(const std::string& s) { std::istringstream in(s); std::vector<std::string> out; std::string v; while(in>>v) out.push_back(v); return out; }
std::vector<const char*> names(const std::vector<std::string>& in) { std::vector<const char*> out; for(const auto& s:in) out.push_back(s.c_str()); return out; }
float half(uint16_t h) {
    const float sign=(h&0x8000)?-1.f:1.f; const unsigned e=(h>>10)&31,m=h&1023;
    return sign*(e? (e==31? (m?NAN:INFINITY):std::ldexp(1.f+m/1024.f,static_cast<int>(e)-15)):std::ldexp(static_cast<float>(m),-24));
}
std::array<float,4> expected(unsigned q,unsigned cycle) {
    const float sign=(cycle&1)?-1.f:1.f;
    return {sign*((q&1)?0.25f:-0.125f),sign*((q&2)?0.5f:-0.0625f),q/4.f,1.f};
}
float expectedDepth(unsigned q,unsigned cycle) { const float values[]={0.f,0.125f,0.5f,1.f}; return values[(q+cycle)%4]; }

// Deliberately separate graphics context from the engine. Only one XR session.
struct State {
    SDL_Window* window{}; SDL_GLContext context{};
    XrInstance instance{}; XrSystemId system{}; XrSession session{}; XrSwapchain chain{};
    bool running=false,frame=false,waited=false,pending=false;
    XrTime display{}; VkInstance vi{}; VkPhysicalDevice physical{}; VkDevice device{}; VkQueue queue{}; unsigned family{};
    VkImage image{}; VkDeviceMemory imageMemory{}; VkBuffer buffer{}; VkDeviceMemory bufferMemory{}; void* mapped{};
    VkCommandPool pool{}; VkCommandBuffer command{}; VkFence fence{}; std::array<VkSemaphore,2> sem{};
    GLuint texture{},fbo{},gm{}; std::array<GLuint,2> gs{}; int fd=-1;
    ~State() {
        // A timed-out external wait cannot safely destroy resources still in use.
        if(pending) { std::fprintf(stderr,"pending GPU work: process teardown owns reclamation\n"); return; }
        if(device) vkDeviceWaitIdle(device);
        if(context) {
            glFinish(); if(fbo) glDeleteFramebuffers(1,&fbo); if(texture) glDeleteTextures(1,&texture);
            if(gm) gp<PFNGLDELETEMEMORYOBJECTSEXTPROC>("glDeleteMemoryObjectsEXT")(1,&gm);
            if(gs[0]||gs[1]) gp<PFNGLDELETESEMAPHORESEXTPROC>("glDeleteSemaphoresEXT")(2,gs.data());
        }
        if(fd>=0) close(fd);
        if(waited && chain) { XrSwapchainImageReleaseInfo r{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO}; xrReleaseSwapchainImage(chain,&r); }
        if(frame) { XrFrameEndInfo e{XR_TYPE_FRAME_END_INFO}; e.displayTime=display; e.environmentBlendMode=XR_ENVIRONMENT_BLEND_MODE_OPAQUE; xrEndFrame(session,&e); }
        if(chain) xrDestroySwapchain(chain); if(session) xrDestroySession(session);
        if(mapped) vkUnmapMemory(device,bufferMemory);
        if(buffer) vkDestroyBuffer(device,buffer,nullptr); if(bufferMemory) vkFreeMemory(device,bufferMemory,nullptr);
        if(image) vkDestroyImage(device,image,nullptr); if(imageMemory) vkFreeMemory(device,imageMemory,nullptr);
        for(auto s:sem) if(s) vkDestroySemaphore(device,s,nullptr);
        if(fence) vkDestroyFence(device,fence,nullptr); if(pool) vkDestroyCommandPool(device,pool,nullptr);
        if(device) vkDestroyDevice(device,nullptr); if(vi) vkDestroyInstance(vi,nullptr); if(instance) xrDestroyInstance(instance);
        if(context) SDL_GL_DeleteContext(context); if(window) SDL_DestroyWindow(window); SDL_Quit();
    }
    void begin() { vk(vkResetCommandBuffer(command,0),"reset commands"); VkCommandBufferBeginInfo b{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; b.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; vk(vkBeginCommandBuffer(command,&b),"begin commands"); }
    void submit(bool wait,bool signal) {
        vk(vkEndCommandBuffer(command),"end commands"); vk(vkResetFences(device,1,&fence),"reset fence");
        const VkPipelineStageFlags stage=VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        VkSubmitInfo s{VK_STRUCTURE_TYPE_SUBMIT_INFO}; s.commandBufferCount=1; s.pCommandBuffers=&command;
        s.waitSemaphoreCount=wait?1:0; s.pWaitSemaphores=sem.data(); s.pWaitDstStageMask=&stage;
        s.signalSemaphoreCount=signal?1:0; s.pSignalSemaphores=&sem[1];
        vk(vkQueueSubmit(queue,1,&s,fence),"submit"); pending=true;
        vk(vkWaitForFences(device,1,&fence,VK_TRUE,5'000'000'000ULL),"GPU wait (5 seconds)"); pending=false;
    }
};
void initGl(State& s) {
    // Hidden diagnostics need no display-mode management. Keep SDL's XRandR
    // mode restore out of this process (including its CRTC warning on exit).
    SDL_SetHint(SDL_HINT_VIDEO_X11_XRANDR,"0");
    need(SDL_Init(SDL_INIT_VIDEO)==0,SDL_GetError());
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,4); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,5);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    s.window=SDL_CreateWindow("Quake isolated motion/depth transport probe",SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,extent,extent,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
    need(s.window!=nullptr,SDL_GetError()); s.context=SDL_GL_CreateContext(s.window); need(s.context!=nullptr,SDL_GetError());
    std::printf("GL renderer=%s version=%s\n",glGetString(GL_RENDERER),glGetString(GL_VERSION));
}
void bootstrap(State& s,const Options& o) {
    uint32_t count{}; xr(xrEnumerateInstanceExtensionProperties(nullptr,0,&count,nullptr),"XR extension count");
    std::vector<XrExtensionProperties> ex(count,{XR_TYPE_EXTENSION_PROPERTIES}); xr(xrEnumerateInstanceExtensionProperties(nullptr,count,&count,ex.data()),"XR extensions");
    auto offered=[&](const char* n) { return std::any_of(ex.begin(),ex.end(),[&](const auto& e){return !std::strcmp(e.extensionName,n);}); };
    need(offered(XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME),"Vulkan2 unavailable");
    std::vector<const char*> enabled{XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME};
    const bool legacy=offered(XR_KHR_VULKAN_ENABLE_EXTENSION_NAME); if(legacy) enabled.push_back(XR_KHR_VULKAN_ENABLE_EXTENSION_NAME);
    XrInstanceCreateInfo xi{XR_TYPE_INSTANCE_CREATE_INFO}; std::strcpy(xi.applicationInfo.applicationName,"Quake motion depth format probe"); xi.applicationInfo.apiVersion=XR_MAKE_VERSION(1,0,0);
    xi.enabledExtensionCount=enabled.size(); xi.enabledExtensionNames=enabled.data(); xr(xrCreateInstance(&xi,&s.instance),"create XR instance");
    XrSystemGetInfo sys{XR_TYPE_SYSTEM_GET_INFO}; sys.formFactor=XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY; xr(xrGetSystem(s.instance,&sys,&s.system),"XR system");
    XrGraphicsRequirementsVulkan2KHR req{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR};
    xr(xp<PFN_xrGetVulkanGraphicsRequirements2KHR>(s.instance,"xrGetVulkanGraphicsRequirements2KHR")(s.instance,s.system,&req),"graphics requirements");
    std::vector<std::string> ins,dev;
    if(legacy) {
        auto ie=xp<PFN_xrGetVulkanInstanceExtensionsKHR>(s.instance,"xrGetVulkanInstanceExtensionsKHR"); uint32_t n{}; xr(ie(s.instance,s.system,0,&n,nullptr),"instance extensions count"); std::string text(n,'\0'); xr(ie(s.instance,s.system,n,&n,text.data()),"instance extensions"); ins=words(text);
    }
    const uint32_t min=VK_MAKE_VERSION(XR_VERSION_MAJOR(req.minApiVersionSupported),XR_VERSION_MINOR(req.minApiVersionSupported),0);
    const uint32_t max=VK_MAKE_VERSION(XR_VERSION_MAJOR(req.maxApiVersionSupported),XR_VERSION_MINOR(req.maxApiVersionSupported),XR_VERSION_PATCH(req.maxApiVersionSupported));
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO}; app.pApplicationName="Quake isolated formats"; app.apiVersion=std::max(min,VK_API_VERSION_1_1); need(app.apiVersion<=max,"requires Vulkan 1.1 within XR range");
    auto in=names(ins); VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO}; ci.pApplicationInfo=&app; ci.enabledExtensionCount=in.size(); ci.ppEnabledExtensionNames=in.data();
    XrVulkanInstanceCreateInfoKHR vi{XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR}; vi.systemId=s.system; vi.pfnGetInstanceProcAddr=vkGetInstanceProcAddr; vi.vulkanCreateInfo=&ci;
    VkResult vr{}; xr(xp<PFN_xrCreateVulkanInstanceKHR>(s.instance,"xrCreateVulkanInstanceKHR")(s.instance,&vi,&s.vi,&vr),"XR Vulkan instance"); vk(vr,"Vulkan instance");
    XrVulkanGraphicsDeviceGetInfoKHR gd{XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR}; gd.systemId=s.system; gd.vulkanInstance=s.vi;
    xr(xp<PFN_xrGetVulkanGraphicsDevice2KHR>(s.instance,"xrGetVulkanGraphicsDevice2KHR")(s.instance,&gd,&s.physical),"XR physical device");
    VkPhysicalDeviceIDProperties ids{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES}; VkPhysicalDeviceProperties2 p{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2}; p.pNext=&ids; vkGetPhysicalDeviceProperties2(s.physical,&p);
    std::array<uint8_t,VK_UUID_SIZE> gu{},du{}; gp<PFNGLGETUNSIGNEDBYTEI_VEXTPROC>("glGetUnsignedBytei_vEXT")(GL_DEVICE_UUID_EXT,0,gu.data()); gp<PFNGLGETUNSIGNEDBYTEVEXTPROC>("glGetUnsignedBytevEXT")(GL_DRIVER_UUID_EXT,du.data()); gl("GL UUIDs");
    need(!std::memcmp(gu.data(),ids.deviceUUID,VK_UUID_SIZE) && !std::memcmp(du.data(),ids.driverUUID,VK_UUID_SIZE),"GL/XR Vulkan device or driver UUID mismatch");
    if(legacy) { auto de=xp<PFN_xrGetVulkanDeviceExtensionsKHR>(s.instance,"xrGetVulkanDeviceExtensionsKHR"); uint32_t n{}; xr(de(s.instance,s.system,0,&n,nullptr),"device extensions count"); std::string text(n,'\0'); xr(de(s.instance,s.system,n,&n,text.data()),"device extensions"); dev=words(text); }
    uint32_t n{}; vk(vkEnumerateDeviceExtensionProperties(s.physical,nullptr,&n,nullptr),"device extension count"); std::vector<VkExtensionProperties> available(n); vk(vkEnumerateDeviceExtensionProperties(s.physical,nullptr,&n,available.data()),"device extensions");
    std::vector<const char*> requested{VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME,VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME,VK_EXT_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_EXTENSION_NAME,VK_EXT_ATTACHMENT_FEEDBACK_LOOP_LAYOUT_EXTENSION_NAME};
    if(o.flags & VK_IMAGE_CREATE_SAMPLE_LOCATIONS_COMPATIBLE_DEPTH_BIT_EXT)
        requested.push_back(VK_EXT_SAMPLE_LOCATIONS_EXTENSION_NAME);
    for(const char* e:requested) {
        need(std::any_of(available.begin(),available.end(),[&](const auto& a){return !std::strcmp(a.extensionName,e);}),e);
        if(std::find(dev.begin(),dev.end(),e)==dev.end()) dev.emplace_back(e);
    }
    VkPhysicalDeviceAttachmentFeedbackLoopLayoutFeaturesEXT feedback{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ATTACHMENT_FEEDBACK_LOOP_LAYOUT_FEATURES_EXT};
    VkPhysicalDeviceMultisampledRenderToSingleSampledFeaturesEXT ms{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_FEATURES_EXT}; ms.pNext=&feedback;
    VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2}; features.pNext=&ms; vkGetPhysicalDeviceFeatures2(s.physical,&features);
    need(ms.multisampledRenderToSingleSampled && feedback.attachmentFeedbackLoopLayout,"required Zink features unavailable");
    vkGetPhysicalDeviceQueueFamilyProperties(s.physical,&n,nullptr); std::vector<VkQueueFamilyProperties> queues(n); vkGetPhysicalDeviceQueueFamilyProperties(s.physical,&n,queues.data()); s.family=UINT32_MAX;
    for(unsigned i=0;i<n;++i) if(queues[i].queueCount && (queues[i].queueFlags&VK_QUEUE_GRAPHICS_BIT)) { s.family=i; break; } need(s.family!=UINT32_MAX,"graphics queue absent");
    float priority=1; VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO}; qi.queueFamilyIndex=s.family; qi.queueCount=1; qi.pQueuePriorities=&priority;
    auto dn=names(dev); VkDeviceCreateInfo dc{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO}; dc.pNext=&ms; dc.queueCreateInfoCount=1; dc.pQueueCreateInfos=&qi; dc.enabledExtensionCount=dn.size(); dc.ppEnabledExtensionNames=dn.data();
    XrVulkanDeviceCreateInfoKHR vd{XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR}; vd.systemId=s.system; vd.pfnGetInstanceProcAddr=vkGetInstanceProcAddr; vd.vulkanPhysicalDevice=s.physical; vd.vulkanCreateInfo=&dc;
    xr(xp<PFN_xrCreateVulkanDeviceKHR>(s.instance,"xrCreateVulkanDeviceKHR")(s.instance,&vd,&s.device,&vr),"XR Vulkan device"); vk(vr,"Vulkan device"); vkGetDeviceQueue(s.device,s.family,0,&s.queue);
    XrGraphicsBindingVulkan2KHR binding{XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR}; binding.instance=s.vi; binding.physicalDevice=s.physical; binding.device=s.device; binding.queueFamilyIndex=s.family;
    XrSessionCreateInfo sc{XR_TYPE_SESSION_CREATE_INFO}; sc.next=&binding; sc.systemId=s.system; xr(xrCreateSession(s.instance,&sc,&s.session),"XR Vulkan session");
    std::printf("uuidMatch=1 queueFamily=%u Vulkan2Device=%s EXTenabled=0\n",s.family,p.properties.deviceName);
}
void barrier(State& s,VkImage image,VkImageAspectFlags aspect,VkImageLayout old,VkImageLayout next,
    uint32_t src,uint32_t dst,VkAccessFlags access,VkAccessFlags after,VkPipelineStageFlags from,VkPipelineStageFlags to) {
    VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER}; b.image=image; b.subresourceRange={aspect,0,1,0,1};
    b.oldLayout=old; b.newLayout=next; b.srcQueueFamilyIndex=src; b.dstQueueFamilyIndex=dst; b.srcAccessMask=access; b.dstAccessMask=after;
    vkCmdPipelineBarrier(s.command,from,to,0,0,nullptr,0,nullptr,1,&b);
}
void setupImage(State& s,const Options& o) {
    const VkFormat format=o.depth?VK_FORMAT_D32_SFLOAT:VK_FORMAT_R16G16B16A16_SFLOAT;
    VkImageFormatListCreateInfo list{VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO}; list.viewFormatCount=o.views.size(); list.pViewFormats=o.views.data();
    VkPhysicalDeviceExternalImageFormatInfo eq{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO}; eq.handleType=VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT; eq.pNext=o.views.empty()?nullptr:&list;
    VkPhysicalDeviceImageFormatInfo2 query{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2}; query.pNext=&eq; query.format=format; query.type=VK_IMAGE_TYPE_2D; query.tiling=VK_IMAGE_TILING_OPTIMAL; query.usage=o.usage; query.flags=o.flags;
    VkExternalImageFormatProperties ep{VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES}; VkImageFormatProperties2 props{VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2}; props.pNext=&ep;
    vk(vkGetPhysicalDeviceImageFormatProperties2(s.physical,&query,&props),"exact external image query");
    const auto required=VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT|VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT;
    need((ep.externalMemoryProperties.externalMemoryFeatures&required)==required,"format is not exportable/importable");
    VkExternalMemoryImageCreateInfo external{VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO}; external.handleTypes=VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT; external.pNext=o.views.empty()?nullptr:&list;
    VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO}; ci.pNext=&external; ci.flags=o.flags; ci.imageType=VK_IMAGE_TYPE_2D; ci.format=format; ci.extent={extent,extent,1}; ci.mipLevels=ci.arrayLayers=1; ci.samples=VK_SAMPLE_COUNT_1_BIT; ci.tiling=VK_IMAGE_TILING_OPTIMAL; ci.usage=o.usage; ci.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    std::printf("EXPORT_IMAGE_BEGIN format=%u flags=0x%x usage=0x%x views=",format,o.flags,o.usage); for(auto v:o.views) std::printf("%u,",v); std::printf(" extent=64x64 mip=1 layer=1 sample=1 tiling=OPTIMAL sharing=EXCLUSIVE initial=UNDEFINED\n");
    vk(vkCreateImage(s.device,&ci,nullptr,&s.image),"create export image");
    VkMemoryRequirements req{}; vkGetImageMemoryRequirements(s.device,s.image,&req);
    VkMemoryDedicatedAllocateInfo dedicated{VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO}; dedicated.image=s.image;
    VkExportMemoryAllocateInfo exp{VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO}; exp.pNext=&dedicated; exp.handleTypes=VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
    VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO}; alloc.pNext=&exp; alloc.allocationSize=req.size; alloc.memoryTypeIndex=memoryType(s.physical,req.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vk(vkAllocateMemory(s.device,&alloc,nullptr,&s.imageMemory),"allocate dedicated image"); vk(vkBindImageMemory(s.device,s.image,s.imageMemory,0),"bind image");
    std::printf("dedicatedSize=%llu alignment=%llu memoryType=%u bits=0x%x externalFeatures=0x%x\n",static_cast<unsigned long long>(req.size),static_cast<unsigned long long>(req.alignment),alloc.memoryTypeIndex,req.memoryTypeBits,ep.externalMemoryProperties.externalMemoryFeatures);
    auto get= reinterpret_cast<PFN_vkGetMemoryFdKHR>(vkGetDeviceProcAddr(s.device,"vkGetMemoryFdKHR")); need(get!=nullptr,"memory FD function absent");
    VkMemoryGetFdInfoKHR fd{VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR}; fd.memory=s.imageMemory; fd.handleType=VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT; vk(get(s.device,&fd,&s.fd),"export memory FD");
    gp<PFNGLCREATEMEMORYOBJECTSEXTPROC>("glCreateMemoryObjectsEXT")(1,&s.gm); const GLint dedicatedGL=GL_TRUE;
    gp<PFNGLMEMORYOBJECTPARAMETERIVEXTPROC>("glMemoryObjectParameterivEXT")(s.gm,GL_DEDICATED_MEMORY_OBJECT_EXT,&dedicatedGL);
    gp<PFNGLIMPORTMEMORYFDEXTPROC>("glImportMemoryFdEXT")(s.gm,req.size,GL_HANDLE_TYPE_OPAQUE_FD_EXT,s.fd); gl("GL dedicated memory import"); s.fd=-1;
    glGenTextures(1,&s.texture); glBindTexture(GL_TEXTURE_2D,s.texture); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_TILING_EXT,GL_OPTIMAL_TILING_EXT);
    std::printf("ZINK_IMPORT_IMAGE_BEGIN\n");
    gp<PFNGLTEXSTORAGEMEM2DEXTPROC>("glTexStorageMem2DEXT")(GL_TEXTURE_2D,1,o.depth?GL_DEPTH_COMPONENT32F:GL_RGBA16F,extent,extent,s.gm,0); gl("GL image storage");
    std::printf("ZINK_IMPORT_IMAGE_END storage=PASS GPUAccess=%s\n",o.storage?"DISABLED":"explicitly-enabled");
    glGenFramebuffers(1,&s.fbo); glBindFramebuffer(GL_FRAMEBUFFER,s.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER,o.depth?GL_DEPTH_ATTACHMENT:GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,s.texture,0);
    glDrawBuffer(o.depth?GL_NONE:GL_COLOR_ATTACHMENT0); glReadBuffer(o.depth?GL_NONE:GL_COLOR_ATTACHMENT0);
    need(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"GL framebuffer incomplete"); gl("GL framebuffer");
}
void resources(State& s,size_t bytes) {
    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO}; bi.size=bytes*2; bi.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT; bi.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    vk(vkCreateBuffer(s.device,&bi,nullptr,&s.buffer),"readback buffer"); VkMemoryRequirements r{}; vkGetBufferMemoryRequirements(s.device,s.buffer,&r);
    VkMemoryAllocateInfo a{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO}; a.allocationSize=r.size; a.memoryTypeIndex=memoryType(s.physical,r.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vk(vkAllocateMemory(s.device,&a,nullptr,&s.bufferMemory),"readback memory"); vk(vkBindBufferMemory(s.device,s.buffer,s.bufferMemory,0),"bind readback"); vk(vkMapMemory(s.device,s.bufferMemory,0,r.size,0,&s.mapped),"persistent readback mapping");
    VkCommandPoolCreateInfo pi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO}; pi.queueFamilyIndex=s.family; pi.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; vk(vkCreateCommandPool(s.device,&pi,nullptr,&s.pool),"command pool");
    VkCommandBufferAllocateInfo cb{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO}; cb.commandPool=s.pool; cb.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; cb.commandBufferCount=1; vk(vkAllocateCommandBuffers(s.device,&cb,&s.command),"command buffer");
    VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO}; vk(vkCreateFence(s.device,&fi,nullptr,&s.fence),"fence");
    VkExportSemaphoreCreateInfo exp{VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO}; exp.handleTypes=VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
    VkSemaphoreCreateInfo si{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO}; si.pNext=&exp;
    gp<PFNGLGENSEMAPHORESEXTPROC>("glGenSemaphoresEXT")(2,s.gs.data());
    auto get=reinterpret_cast<PFN_vkGetSemaphoreFdKHR>(vkGetDeviceProcAddr(s.device,"vkGetSemaphoreFdKHR")); need(get!=nullptr,"semaphore FD function absent");
    for(unsigned i=0;i<2;++i) {
        vk(vkCreateSemaphore(s.device,&si,nullptr,&s.sem[i]),"external semaphore"); VkSemaphoreGetFdInfoKHR fd{VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR}; fd.semaphore=s.sem[i]; fd.handleType=VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
        vk(get(s.device,&fd,&s.fd),"export semaphore"); gp<PFNGLIMPORTSEMAPHOREFDEXTPROC>("glImportSemaphoreFdEXT")(s.gs[i],GL_HANDLE_TYPE_OPAQUE_FD_EXT,s.fd); gl("GL semaphore import"); s.fd=-1;
    }
}
void startSession(State& s) {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    while(std::chrono::steady_clock::now()<deadline && !s.running) {
        XrEventDataBuffer e{XR_TYPE_EVENT_DATA_BUFFER}; const auto r=xrPollEvent(s.instance,&e); need(r==XR_SUCCESS || r==XR_EVENT_UNAVAILABLE,"XR event failure");
        if(r==XR_SUCCESS && e.type==XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
            const auto& event=*reinterpret_cast<const XrEventDataSessionStateChanged*>(&e);
            if(event.state==XR_SESSION_STATE_READY) { XrSessionBeginInfo b{XR_TYPE_SESSION_BEGIN_INFO}; b.primaryViewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO; xr(xrBeginSession(s.session,&b),"begin XR session"); s.running=true; }
            need(event.state!=XR_SESSION_STATE_EXITING && event.state!=XR_SESSION_STATE_LOSS_PENDING,"XR session exiting");
        }
        if(!s.running) SDL_Delay(10);
    }
    need(s.running,"XR READY timeout");
}
void frameBegin(State& s) {
    XrFrameWaitInfo w{XR_TYPE_FRAME_WAIT_INFO}; XrFrameState f{XR_TYPE_FRAME_STATE}; xr(xrWaitFrame(s.session,&w,&f),"wait XR frame");
    XrFrameBeginInfo b{XR_TYPE_FRAME_BEGIN_INFO}; xr(xrBeginFrame(s.session,&b),"begin XR frame"); s.frame=true; s.display=f.predictedDisplayTime;
}
void frameEnd(State& s) {
    XrFrameEndInfo e{XR_TYPE_FRAME_END_INFO}; e.displayTime=s.display; e.environmentBlendMode=XR_ENVIRONMENT_BLEND_MODE_OPAQUE; xr(xrEndFrame(s.session,&e),"end XR frame (zero layers)"); s.frame=false;
}
unsigned acquire(State& s) {
    unsigned index{}; XrSwapchainImageAcquireInfo a{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO}; xr(xrAcquireSwapchainImage(s.chain,&a,&index),"acquire XR image");
    XrSwapchainImageWaitInfo w{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO}; w.timeout=100'000'000;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    XrResult r;
    do { r=xrWaitSwapchainImage(s.chain,&w); } while(r==XR_TIMEOUT_EXPIRED && std::chrono::steady_clock::now()<deadline);
    xr(r,"wait XR image"); s.waited=true; return index;
}
void validate(const Options& o,const void* data,unsigned cycle,const char* label,bool xrRows) {
    unsigned mismatches=0;
    for(unsigned y=0;y<extent;++y) for(unsigned x=0;x<extent;++x) {
        // Shared memory preserves GL's raw rows: raw row 0 is the GL bottom.
        // The destination's row 0 represents the Vulkan/XR top. Our explicit
        // row-copy reverses Y, so XR row y must contain GL row H-1-y.
        // The motion payload already uses Vulkan NDC signs; flipping its
        // spatial location must not negate its Y component a second time.
        const unsigned glY=xrRows?extent-1-y:y;
        const unsigned q=(x>=extent/2?1:0)+(glY>=extent/2?2:0), pixel=y*extent+x;
        if(o.depth) { const float v=static_cast<const float*>(data)[pixel]; mismatches+=!std::isfinite(v)||std::abs(v-expectedDepth(q,cycle))>1e-6f; }
        else {
            const auto expectedValue=expected(q,cycle); const auto* h=static_cast<const uint16_t*>(data)+pixel*4;
            bool bad=false; for(unsigned c=0;c<4;++c) { const float v=half(h[c]); bad|=!std::isfinite(v)||std::abs(v-expectedValue[c])>1e-5f; } mismatches+=bad;
        }
    }
    std::printf("%s cycle=%u pixels=%u mismatches=%u row0=%s signedXY=%d\n",label,cycle,extent*extent,mismatches,xrRows?"XR-top(GL-top)":"raw-GL-bottom",!o.depth);
    need(!mismatches,"pixel mismatch including channel signs or row orientation");
}
int run(const Options& o) {
    State s; initGl(s);
    if(o.discover) {
        std::printf("DISCOVER_IMAGE_BEGIN format=%s\n",o.depth?"GL_DEPTH_COMPONENT32F":"GL_RGBA16F"); glGenTextures(1,&s.texture); glBindTexture(GL_TEXTURE_2D,s.texture);
        glTexStorage2D(GL_TEXTURE_2D,1,o.depth?GL_DEPTH_COMPONENT32F:GL_RGBA16F,extent,extent); gl("ordinary GL discovery texture");
        std::printf("DISCOVER_IMAGE_END noXR=1 GPUAccess=0; capture actual import separately using --storage-only\n"); return 0;
    }
    bootstrap(s,o); setupImage(s,o); if(o.storage) { std::printf("STORAGE_ONLY_DONE noFrame=1 noGPUAccess=1 verify exporter/importer definitions before handoff\n"); return 0; }
    const size_t bytes=extent*extent*(o.depth?4:8); resources(s,bytes);
    const VkFormat format=o.depth?VK_FORMAT_D32_SFLOAT:VK_FORMAT_R16G16B16A16_SFLOAT;
    const VkImageAspectFlags aspect=o.depth?VK_IMAGE_ASPECT_DEPTH_BIT:VK_IMAGE_ASPECT_COLOR_BIT;
    uint32_t count{}; xr(xrEnumerateSwapchainFormats(s.session,0,&count,nullptr),"XR format count"); std::vector<int64_t> formats(count); xr(xrEnumerateSwapchainFormats(s.session,count,&count,formats.data()),"XR formats"); need(std::find(formats.begin(),formats.end(),format)!=formats.end(),"required XR format unavailable");
    XrSwapchainCreateInfo sc{XR_TYPE_SWAPCHAIN_CREATE_INFO}; sc.format=format; sc.width=sc.height=extent; sc.sampleCount=sc.faceCount=sc.arraySize=sc.mipCount=1;
    sc.usageFlags=XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT|XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT|XR_SWAPCHAIN_USAGE_SAMPLED_BIT|(o.depth?XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT:XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT);
    xr(xrCreateSwapchain(s.session,&sc,&s.chain),"XR swapchain"); xr(xrEnumerateSwapchainImages(s.chain,0,&count,nullptr),"XR image count");
    std::vector<XrSwapchainImageVulkanKHR> images(count,{XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR}); xr(xrEnumerateSwapchainImages(s.chain,count,&count,reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data())),"XR images");
    startSession(s);
    s.begin(); barrier(s,s.image,aspect,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_GENERAL,s.family,VK_QUEUE_FAMILY_EXTERNAL,0,0,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT); s.submit(false,true);
    const GLenum general=GL_LAYOUT_GENERAL_EXT;
    gp<PFNGLWAITSEMAPHOREEXTPROC>("glWaitSemaphoreEXT")(s.gs[1],0,nullptr,1,&s.texture,&general); gl("initial GL wait");
    for(unsigned cycle=0;cycle<3;++cycle) {
        glBindFramebuffer(GL_FRAMEBUFFER,s.fbo); glDisable(GL_FRAMEBUFFER_SRGB); glEnable(GL_SCISSOR_TEST); glDepthMask(GL_TRUE);
        for(unsigned q=0;q<4;++q) {
            glScissor((q&1)*(extent/2),(q>>1)*(extent/2),extent/2,extent/2);
            if(o.depth) { const float d=expectedDepth(q,cycle); glClearBufferfv(GL_DEPTH,0,&d); }
            else { const auto value=expected(q,cycle); glClearBufferfv(GL_COLOR,0,value.data()); }
        }
        glDisable(GL_SCISSOR_TEST); gl("GL asymmetric pattern"); gp<PFNGLSIGNALSEMAPHOREEXTPROC>("glSignalSemaphoreEXT")(s.gs[0],0,nullptr,1,&s.texture,&general); gl("GL signal"); glFlush();
        frameBegin(s); const unsigned index=acquire(s); need(index<images.size(),"XR image index out of range"); const VkImage target=images[index].image;
        const auto runtimeLayout=o.depth?VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        const auto runtimeStage=o.depth?VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT:VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        const auto runtimeAccess=o.depth?VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT:VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        s.begin(); barrier(s,s.image,aspect,VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_GENERAL,VK_QUEUE_FAMILY_EXTERNAL,s.family,0,VK_ACCESS_TRANSFER_READ_BIT,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
        barrier(s,s.image,aspect,VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_QUEUE_FAMILY_IGNORED,VK_QUEUE_FAMILY_IGNORED,VK_ACCESS_MEMORY_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
        VkBufferImageCopy copy{}; copy.imageSubresource={aspect,0,0,1}; copy.imageExtent={extent,extent,1}; vkCmdCopyImageToBuffer(s.command,s.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,s.buffer,1,&copy);
        barrier(s,target,aspect,runtimeLayout,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_QUEUE_FAMILY_IGNORED,VK_QUEUE_FAMILY_IGNORED,runtimeAccess,VK_ACCESS_TRANSFER_WRITE_BIT,runtimeStage,VK_PIPELINE_STAGE_TRANSFER_BIT);
        // Correct the origin with exact same-format copies, including D32.
        // A copy cannot reverse offsets. One region per row avoids requiring
        // optional depth-blit support and makes the origin test unambiguous.
        // Production should perform this flip in its export shader instead.
        std::array<VkImageCopy,extent> rows{};
        for(unsigned y=0;y<extent;++y) {
            auto& row=rows[y]; row.srcSubresource=row.dstSubresource={aspect,0,0,1};
            row.srcOffset.y=extent-1-y; row.dstOffset.y=y; row.extent={extent,1,1};
        }
        vkCmdCopyImage(s.command,s.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,target,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,rows.size(),rows.data());
        barrier(s,target,aspect,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_QUEUE_FAMILY_IGNORED,VK_QUEUE_FAMILY_IGNORED,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
        copy.bufferOffset=bytes; vkCmdCopyImageToBuffer(s.command,target,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,s.buffer,1,&copy);
        VkBufferMemoryBarrier host{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER}; host.buffer=s.buffer; host.size=VK_WHOLE_SIZE; host.srcQueueFamilyIndex=host.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED; host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT; host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
        vkCmdPipelineBarrier(s.command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&host,0,nullptr);
        barrier(s,target,aspect,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,runtimeLayout,VK_QUEUE_FAMILY_IGNORED,VK_QUEUE_FAMILY_IGNORED,VK_ACCESS_TRANSFER_READ_BIT,runtimeAccess,VK_PIPELINE_STAGE_TRANSFER_BIT,runtimeStage);
        barrier(s,s.image,aspect,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_GENERAL,s.family,VK_QUEUE_FAMILY_EXTERNAL,VK_ACCESS_TRANSFER_READ_BIT,0,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT); s.submit(true,true);
        validate(o,s.mapped,cycle,"GL-to-Vulkan",false); validate(o,static_cast<const uint8_t*>(s.mapped)+bytes,cycle,"GL-to-XR",true);
        XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO}; xr(xrReleaseSwapchainImage(s.chain,&release),"release XR image"); s.waited=false; frameEnd(s);
        gp<PFNGLWAITSEMAPHOREEXTPROC>("glWaitSemaphoreEXT")(s.gs[1],0,nullptr,1,&s.texture,&general); gl("return GL ownership"); glFlush();
    }
    glFinish(); std::printf("PASS format=%s cycles=3 checkedPixelsEachPath=%u projectionLayers=0 synthesisMetadata=0 runtimeConsumption=UNPROVEN\n",o.depth?"D32F":"RGBA16F",3*extent*extent); return 0;
}
} // namespace
int main(int argc,char** argv) {
    std::setvbuf(stdout,nullptr,_IONBF,0);
    try { return run(options(argc,argv)); }
    catch(const std::exception& e) { std::fprintf(stderr,"FAIL %s\n",e.what()); return 2; }
}
