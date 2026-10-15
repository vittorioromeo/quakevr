// Isolated rasterized GL depth -> Bridge attachment -> sampled XR depth.
// Reuses the diagnostic lifecycle only; no reference probe entry point is run.
#define main qvr_reference_format_probe_main
#include "vr_openxr_gl_formats_probe.cpp"
#undef main
#include "vr_gl_vulkan_bridge.hpp"
#include <fstream>
#include <regex>
#include <iterator>

namespace {
void bridgeResult(State& state,qvr::gl_vulkan::Bridge& bridge,bool ok) {
    if(ok) return;
    // Bridge timeouts retain native children. The borrowed-device fixture must
    // then retain their GL/XR/Vulkan parents too; its generic destructor cannot
    // safely vkDeviceWaitIdle/destroy them after an external semaphore failure.
    state.pending=true;
    need(false,bridge.error());
}
struct BridgeCleanup {
    State& state;
    qvr::gl_vulkan::Bridge& bridge;
    ~BridgeCleanup() { if(!bridge.shutdown()) state.pending=true; }
};
std::string handle(VkImage image) { std::uint64_t value{}; static_assert(sizeof(value)==sizeof(image)); std::memcpy(&value,&image,sizeof(value)); char text[32]; std::snprintf(text,sizeof(text),"0x%llx",static_cast<unsigned long long>(value)); return text; }
unsigned field(const std::string& block,const char* pattern) { std::smatch match; need(std::regex_search(block,match,std::regex(pattern)),"creation metadata absent from same-run API dump"); return number(match[1].str().c_str()); }
// Test evidence only. Production Destination is derived from XR_SUCCESS and
// the requested usage/shape, never from these unqueryable native create flags.
struct NativeImageEvidence { unsigned flags{},usage{},format{},samples{},width{},height{}; };
NativeImageEvidence creation(const char* file,VkImage image) {
    std::fflush(nullptr); std::ifstream stream(file); need(stream.good(),"cannot read same-run API dump");
    const std::string text((std::istreambuf_iterator<char>(stream)),{}); const std::string needle="VkImage* = "+handle(image);
    size_t begin=0; std::string found;
    while((begin=text.find("vkCreateImage(",begin))!=std::string::npos) {
        const size_t end=text.find("\nThread ",begin); const auto block=text.substr(begin,end==std::string::npos?text.size()-begin:end-begin);
        if(block.find(needle)!=std::string::npos) found=block;
        begin+=14;
    }
    need(!found.empty(),"returned XR image was not found in same-run vkCreateImage capture");
    NativeImageEvidence out;
    out.flags=field(found,"flags:\\s+VkImageCreateFlags = ([0-9]+)");
    out.usage=field(found,"usage:\\s+VkImageUsageFlags = ([0-9]+)");
    out.samples=field(found,"samples:\\s+VkSampleCountFlagBits = ([0-9]+)");
    out.format=field(found,"format:\\s+VkFormat = [^\\n]+\\(([0-9]+)\\)");
    out.width=field(found,"width:\\s+uint32_t = ([0-9]+)"); out.height=field(found,"height:\\s+uint32_t = ([0-9]+)");
    std::printf("XR_CAPTURED_EVIDENCE image=%s format=%u flags=0x%x usage=0x%x samples=%u extent=%ux%u source=same-run-api-dump productionEligibility=0\n",handle(image).c_str(),out.format,out.flags,out.usage,out.samples,out.width,out.height);
    // Unlike production attachment rendering, this fixture transitions XR D32
    // for its independent sampled readback. Require the captured flags-zero
    // profile only for that test operation, not for Bridge::copyTo eligibility.
    need(out.format==VK_FORMAT_D32_SFLOAT && out.flags==0 && out.samples==VK_SAMPLE_COUNT_1_BIT && out.width==extent && out.height==extent,"unexpected sampled-readback test definition");
    return out;
}
GLuint shader(GLenum stage,const char* source) {
    GLuint result=glCreateShader(stage); glShaderSource(result,1,&source,nullptr); glCompileShader(result); GLint ok{}; glGetShaderiv(result,GL_COMPILE_STATUS,&ok);
    if(!ok) { char log[2048]{}; glGetShaderInfoLog(result,sizeof(log),nullptr,log); std::fprintf(stderr,"GL shader %s\n",log); } need(ok,"GL raster shader compilation failed"); return result;
}
GLuint rasterProgram() {
    const char* vs="#version 450\nvoid main(){vec2 p[3]=vec2[](vec2(-1,-1),vec2(3,-1),vec2(-1,3));gl_Position=vec4(p[gl_VertexID],0,1);}";
    const char* fs="#version 450\nlayout(location=0) uniform int Cycle;layout(location=0) out vec4 Motion;void main(){int q=(gl_FragCoord.x>=32?1:0)+(gl_FragCoord.y>=32?2:0);float d[4]=float[](0.0,0.125,0.5,1.0);gl_FragDepth=d[(q+Cycle)%4];float s=(Cycle%2==1)?-1.0:1.0;Motion=vec4(s*((q%2==1)?0.25:-0.125),s*((q>=2)?0.5:-0.0625),float(q)/4.0,1.0);}";
    const GLuint v=shader(GL_VERTEX_SHADER,vs),f=shader(GL_FRAGMENT_SHADER,fs); GLuint program=glCreateProgram(); glAttachShader(program,v); glAttachShader(program,f); glLinkProgram(program); GLint ok{}; glGetProgramiv(program,GL_LINK_STATUS,&ok); need(ok,"GL raster shader link failed"); glDeleteShader(v); glDeleteShader(f); return program;
}
struct Sampling {
    State& s; VkBuffer buffer{}; VkDeviceMemory memory{}; void* mapped{}; VkDescriptorSetLayout dl{}; VkDescriptorPool pool{}; VkDescriptorSet set{};
    VkPipelineLayout layout{}; VkPipeline pipeline{}; VkShaderModule shaderModule{}; VkSampler sampler{}; std::vector<VkImageView> views;
    explicit Sampling(State& state):s(state){}
    ~Sampling() {
        if(s.pending) return;
        for(auto view:views) if(view) vkDestroyImageView(s.device,view,nullptr);
        if(pipeline) vkDestroyPipeline(s.device,pipeline,nullptr); if(layout) vkDestroyPipelineLayout(s.device,layout,nullptr);
        if(pool) vkDestroyDescriptorPool(s.device,pool,nullptr); if(dl) vkDestroyDescriptorSetLayout(s.device,dl,nullptr);
        if(sampler) vkDestroySampler(s.device,sampler,nullptr); if(shaderModule) vkDestroyShaderModule(s.device,shaderModule,nullptr);
        if(mapped) vkUnmapMemory(s.device,memory); if(buffer) vkDestroyBuffer(s.device,buffer,nullptr); if(memory) vkFreeMemory(s.device,memory,nullptr);
    }
    void init(const std::vector<XrSwapchainImageVulkanKHR>& images) {
        uint32_t n{}; vkGetPhysicalDeviceQueueFamilyProperties(s.physical,&n,nullptr); std::vector<VkQueueFamilyProperties> qs(n); vkGetPhysicalDeviceQueueFamilyProperties(s.physical,&n,qs.data()); need(qs[s.family].queueFlags&VK_QUEUE_COMPUTE_BIT,"bound queue cannot compute sampled readback");
        VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO}; bi.size=extent*extent*sizeof(float); bi.usage=VK_BUFFER_USAGE_STORAGE_BUFFER_BIT; bi.sharingMode=VK_SHARING_MODE_EXCLUSIVE; vk(vkCreateBuffer(s.device,&bi,nullptr,&buffer),"sampled readback buffer"); VkMemoryRequirements r{}; vkGetBufferMemoryRequirements(s.device,buffer,&r);
        VkMemoryAllocateInfo a{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO}; a.allocationSize=r.size; a.memoryTypeIndex=memoryType(s.physical,r.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        vk(vkAllocateMemory(s.device,&a,nullptr,&memory),"readback allocation"); vk(vkBindBufferMemory(s.device,buffer,memory,0),"readback binding"); vk(vkMapMemory(s.device,memory,0,r.size,0,&mapped),"persistent readback map");
        VkDescriptorSetLayoutBinding bindings[2]{}; bindings[0].binding=0; bindings[0].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; bindings[1].binding=1; bindings[1].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        for(auto& b:bindings) {b.descriptorCount=1;b.stageFlags=VK_SHADER_STAGE_COMPUTE_BIT;}
        VkDescriptorSetLayoutCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO}; info.bindingCount=2; info.pBindings=bindings; vk(vkCreateDescriptorSetLayout(s.device,&info,nullptr,&dl),"sampling descriptor layout");
        VkDescriptorPoolSize sizes[]={{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1},{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1}}; VkDescriptorPoolCreateInfo pi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO}; pi.maxSets=1; pi.poolSizeCount=2; pi.pPoolSizes=sizes; vk(vkCreateDescriptorPool(s.device,&pi,nullptr,&pool),"sampling descriptor pool");
        VkDescriptorSetAllocateInfo da{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO}; da.descriptorPool=pool; da.descriptorSetCount=1; da.pSetLayouts=&dl; vk(vkAllocateDescriptorSets(s.device,&da,&set),"sampling descriptor");
        VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO}; si.magFilter=si.minFilter=VK_FILTER_NEAREST; si.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST; si.addressModeU=si.addressModeV=si.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; vk(vkCreateSampler(s.device,&si,nullptr,&sampler),"sampling sampler");
        std::ifstream file(QVR_DEPTH_READBACK_SHADER,std::ios::binary|std::ios::ate); need(file.good(),"compute SPIR-V missing"); const auto bytes=file.tellg(); need(bytes>0 && bytes%4==0,"invalid SPIR-V size"); std::vector<uint32_t> code(static_cast<size_t>(bytes)/4); file.seekg(0); file.read(reinterpret_cast<char*>(code.data()),bytes); need(file.good(),"SPIR-V read failed");
        VkShaderModuleCreateInfo sm{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO}; sm.codeSize=static_cast<size_t>(bytes); sm.pCode=code.data(); vk(vkCreateShaderModule(s.device,&sm,nullptr,&shaderModule),"sampling compute shader");
        VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT,0,8}; VkPipelineLayoutCreateInfo li{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO}; li.setLayoutCount=1; li.pSetLayouts=&dl; li.pushConstantRangeCount=1; li.pPushConstantRanges=&push; vk(vkCreatePipelineLayout(s.device,&li,nullptr,&layout),"sampling pipeline layout");
        VkComputePipelineCreateInfo ci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO}; ci.stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; ci.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT; ci.stage.module=shaderModule; ci.stage.pName="main"; ci.layout=layout; vk(vkCreateComputePipelines(s.device,VK_NULL_HANDLE,1,&ci,nullptr,&pipeline),"sampling pipeline");
        for(auto image:images) { VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO}; vi.image=image.image; vi.format=VK_FORMAT_D32_SFLOAT; vi.viewType=VK_IMAGE_VIEW_TYPE_2D; vi.subresourceRange={VK_IMAGE_ASPECT_DEPTH_BIT,0,1,0,1}; VkImageView view{}; vk(vkCreateImageView(s.device,&vi,nullptr,&view),"XR sampled view"); views.push_back(view); }
        VkCommandPoolCreateInfo cp{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO}; cp.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; cp.queueFamilyIndex=s.family; vk(vkCreateCommandPool(s.device,&cp,nullptr,&s.pool),"sampling command pool");
        VkCommandBufferAllocateInfo cb{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO}; cb.commandPool=s.pool; cb.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; cb.commandBufferCount=1; vk(vkAllocateCommandBuffers(s.device,&cb,&s.command),"sampling commands"); VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO}; vk(vkCreateFence(s.device,&fi,nullptr,&s.fence),"sampling fence");
    }
    void read(unsigned index,VkImage image) {
        VkDescriptorImageInfo ii{sampler,views[index],VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL}; VkDescriptorBufferInfo bi{buffer,0,extent*extent*sizeof(float)};
        VkWriteDescriptorSet writes[2]{}; for(auto& w:writes) {w.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;w.dstSet=set;w.descriptorCount=1;}
        writes[0].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;writes[0].pImageInfo=&ii;writes[1].dstBinding=1;writes[1].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;writes[1].pBufferInfo=&bi;vkUpdateDescriptorSets(s.device,2,writes,0,nullptr);
        s.begin(); VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER}; b.image=image;b.subresourceRange={VK_IMAGE_ASPECT_DEPTH_BIT,0,1,0,1};b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
        b.oldLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;b.newLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;b.srcAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;b.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(s.command,VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,0,nullptr,0,nullptr,1,&b);
        vkCmdBindPipeline(s.command,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline);vkCmdBindDescriptorSets(s.command,VK_PIPELINE_BIND_POINT_COMPUTE,layout,0,1,&set,0,nullptr); const unsigned dimensions[]={extent,extent}; vkCmdPushConstants(s.command,layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(dimensions),dimensions); vkCmdDispatch(s.command,extent/8,extent/8,1);
        VkBufferMemoryBarrier host{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};host.buffer=buffer;host.size=VK_WHOLE_SIZE;host.srcQueueFamilyIndex=host.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;host.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
        vkCmdPipelineBarrier(s.command,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&host,0,nullptr);
        b.oldLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;b.newLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;b.srcAccessMask=VK_ACCESS_SHADER_READ_BIT;b.dstAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        vkCmdPipelineBarrier(s.command,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,0,0,nullptr,0,nullptr,1,&b); s.submit(false,false);
    }
};
// The second XR chain keeps its own acquired index. TRANSFER_SRC is a test-only
// request for independent FP16 readback; production motion needs no readback.
struct MotionTarget {
    State& s; qvr::gl_vulkan::Bridge& bridge; XrSwapchain chain{};
    XrSwapchainCreateInfo request{XR_TYPE_SWAPCHAIN_CREATE_INFO};
    std::vector<XrSwapchainImageVulkanKHR> images;
    bool acquired{},waited{}; unsigned index{};
    VkBuffer buffer{}; VkDeviceMemory memory{}; void* mapped{};
    MotionTarget(State& state,qvr::gl_vulkan::Bridge& b):s(state),bridge(b){}
    ~MotionTarget() {
        if(s.pending) return;
        if(!bridge.waitForCompletion()) {s.pending=true;return;}
        if(waited) {
            XrSwapchainImageReleaseInfo r{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
            if(xrReleaseSwapchainImage(chain,&r)!=XR_SUCCESS) {s.pending=true;return;}
        }
        if(mapped) vkUnmapMemory(s.device,memory);
        if(buffer) vkDestroyBuffer(s.device,buffer,nullptr);
        if(memory) vkFreeMemory(s.device,memory,nullptr);
        if(chain && xrDestroySwapchain(chain)!=XR_SUCCESS) s.pending=true;
    }
    void init() {
        request.format=VK_FORMAT_R16G16B16A16_SFLOAT;request.width=request.height=extent;
        request.sampleCount=request.faceCount=request.arraySize=request.mipCount=1;
        request.usageFlags=XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT|XR_SWAPCHAIN_USAGE_SAMPLED_BIT|
            XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT|XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT;
        xr(xrCreateSwapchain(s.session,&request,&chain),"create paired motion XR chain");
        unsigned count{};xr(xrEnumerateSwapchainImages(chain,0,&count,nullptr),"motion image count");need(count>0,"empty motion chain");
        images.resize(count,{XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR});
        xr(xrEnumerateSwapchainImages(chain,count,&count,reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data())),"enumerate paired motion images");
        VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};bi.size=extent*extent*8;bi.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;bi.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
        vk(vkCreateBuffer(s.device,&bi,nullptr,&buffer),"motion readback buffer");VkMemoryRequirements r{};vkGetBufferMemoryRequirements(s.device,buffer,&r);
        VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};ai.allocationSize=r.size;ai.memoryTypeIndex=memoryType(s.physical,r.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        vk(vkAllocateMemory(s.device,&ai,nullptr,&memory),"motion readback memory");vk(vkBindBufferMemory(s.device,buffer,memory,0),"motion readback binding");vk(vkMapMemory(s.device,memory,0,r.size,0,&mapped),"motion persistent readback map");
        std::printf("PAIRED_MOTION_CHAIN images=%u usage=0x%llx readbackOnlyTransferSrc=1\n",count,static_cast<unsigned long long>(request.usageFlags));
    }
    qvr::gl_vulkan::Destination acquireImage() {
        need(!acquired,"motion chain acquired twice");XrSwapchainImageAcquireInfo ai{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        xr(xrAcquireSwapchainImage(chain,&ai,&index),"acquire paired motion");acquired=true;
        XrSwapchainImageWaitInfo wi{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};wi.timeout=100'000'000;
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);XrResult r;
        do {r=xrWaitSwapchainImage(chain,&wi);} while(r==XR_TIMEOUT_EXPIRED && std::chrono::steady_clock::now()<deadline);
        xr(r,"wait paired motion");waited=true;need(index<images.size(),"motion acquired index invalid");
        auto d=qvr::gl_vulkan::destinationFromSwapchainRequest(images[index].image,request,XR_SUCCESS);d.acquiredAndWaited=true;return d;
    }
    void release() {
        need(acquired && waited && bridge.healthy(),"paired motion release before proven completion");
        releaseAfterDrain();
    }
    void releaseAfterDrain() {
        need(acquired && waited,"motion teardown release without acquire/wait");
        // An intentional pending-reuse rejection poisons the bridge. Teardown
        // may still release its XR destination once its fence has drained.
        bridgeResult(s,bridge,bridge.waitForCompletion());
        XrSwapchainImageReleaseInfo r{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};xr(xrReleaseSwapchainImage(chain,&r),"release paired motion");acquired=waited=false;
    }
    void read(unsigned cycle) {
        need(waited && bridge.healthy(),"motion readback before export completion");s.begin();
        VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.image=images[index].image;b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
        b.oldLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;b.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;b.srcAccessMask=VK_ACCESS_MEMORY_WRITE_BIT;b.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
        vkCmdPipelineBarrier(s.command,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&b);
        VkBufferImageCopy region{};region.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};region.imageExtent={extent,extent,1};vkCmdCopyImageToBuffer(s.command,b.image,b.newLayout,buffer,1,&region);
        VkBufferMemoryBarrier host{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};host.buffer=buffer;host.size=VK_WHOLE_SIZE;host.srcQueueFamilyIndex=host.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
        vkCmdPipelineBarrier(s.command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&host,0,nullptr);
        b.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;b.newLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;b.srcAccessMask=VK_ACCESS_TRANSFER_READ_BIT;b.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        vkCmdPipelineBarrier(s.command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,0,0,nullptr,0,nullptr,1,&b);s.submit(false,false);
        Options motion;validate(motion,mapped,cycle,"PAIRED_MOTION_READBACK",true);
    }
};
void rasterBootstrap(State& s,const Options& o,unsigned& api) {
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
    api=app.apiVersion; auto in=names(ins); VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO}; ci.pApplicationInfo=&app; ci.enabledExtensionCount=in.size(); ci.ppEnabledExtensionNames=in.data();
    XrVulkanInstanceCreateInfoKHR vi{XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR}; vi.systemId=s.system; vi.pfnGetInstanceProcAddr=vkGetInstanceProcAddr; vi.vulkanCreateInfo=&ci;
    VkResult vr{}; xr(xp<PFN_xrCreateVulkanInstanceKHR>(s.instance,"xrCreateVulkanInstanceKHR")(s.instance,&vi,&s.vi,&vr),"XR Vulkan instance"); vk(vr,"Vulkan instance");
    XrVulkanGraphicsDeviceGetInfoKHR gd{XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR}; gd.systemId=s.system; gd.vulkanInstance=s.vi;
    xr(xp<PFN_xrGetVulkanGraphicsDevice2KHR>(s.instance,"xrGetVulkanGraphicsDevice2KHR")(s.instance,&gd,&s.physical),"XR physical device");
    VkPhysicalDeviceIDProperties ids{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES}; VkPhysicalDeviceProperties2 p{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2}; p.pNext=&ids; vkGetPhysicalDeviceProperties2(s.physical,&p);
    std::array<uint8_t,VK_UUID_SIZE> gu{},du{}; gp<PFNGLGETUNSIGNEDBYTEI_VEXTPROC>("glGetUnsignedBytei_vEXT")(GL_DEVICE_UUID_EXT,0,gu.data()); gp<PFNGLGETUNSIGNEDBYTEVEXTPROC>("glGetUnsignedBytevEXT")(GL_DRIVER_UUID_EXT,du.data()); gl("GL UUIDs");
    need(!std::memcmp(gu.data(),ids.deviceUUID,VK_UUID_SIZE) && !std::memcmp(du.data(),ids.driverUUID,VK_UUID_SIZE),"GL/XR Vulkan device or driver UUID mismatch");
    if(legacy) { auto de=xp<PFN_xrGetVulkanDeviceExtensionsKHR>(s.instance,"xrGetVulkanDeviceExtensionsKHR"); uint32_t n{}; xr(de(s.instance,s.system,0,&n,nullptr),"device extensions count"); std::string text(n,'\0'); xr(de(s.instance,s.system,n,&n,text.data()),"device extensions"); dev=words(text); }
    uint32_t n{}; vk(vkEnumerateDeviceExtensionProperties(s.physical,nullptr,&n,nullptr),"device extension count"); std::vector<VkExtensionProperties> available(n); vk(vkEnumerateDeviceExtensionProperties(s.physical,nullptr,&n,available.data()),"device extensions");
    std::vector<const char*> requested{VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME,VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME,VK_EXT_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_EXTENSION_NAME,VK_EXT_ATTACHMENT_FEEDBACK_LOOP_LAYOUT_EXTENSION_NAME,VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME};
    if(api<VK_API_VERSION_1_2) {
        requested.push_back(VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME);
        requested.push_back(VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME);
    }
    if(o.flags & VK_IMAGE_CREATE_SAMPLE_LOCATIONS_COMPATIBLE_DEPTH_BIT_EXT)
        requested.push_back(VK_EXT_SAMPLE_LOCATIONS_EXTENSION_NAME);
    for(const char* e:requested) {
        need(std::any_of(available.begin(),available.end(),[&](const auto& a){return !std::strcmp(a.extensionName,e);}),e);
        if(std::find(dev.begin(),dev.end(),e)==dev.end()) dev.emplace_back(e);
    }
    VkPhysicalDeviceAttachmentFeedbackLoopLayoutFeaturesEXT feedback{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ATTACHMENT_FEEDBACK_LOOP_LAYOUT_FEATURES_EXT};
    VkPhysicalDeviceMultisampledRenderToSingleSampledFeaturesEXT ms{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_FEATURES_EXT}; ms.pNext=&feedback; VkPhysicalDeviceSynchronization2Features sync2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES}; feedback.pNext=&sync2;
    VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2}; features.pNext=&ms; vkGetPhysicalDeviceFeatures2(s.physical,&features);
    need(ms.multisampledRenderToSingleSampled && feedback.attachmentFeedbackLoopLayout && sync2.synchronization2,"required Zink features unavailable");
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
int runRaster(const char* capture)
{
    State s; initGl(s); Options o; o.depth=true; o.flags=0x41000; unsigned api{}; rasterBootstrap(s,o,api);
    std::printf("RASTER_BOOTSTRAP api=%u sync2=enabled sampleLocations=enabled EXTenabled=0\n",api);
    qvr::gl_vulkan::Bridge bridge,motionBridge; BridgeCleanup cleanup{s,bridge},motionCleanup{s,motionBridge}; qvr::gl_vulkan::DeviceContext context;
    context.instance=s.vi;context.physicalDevice=s.physical;context.device=s.device;context.queue=s.queue;context.queueFamily=s.family;context.apiVersion=api;
    context.externalImageFeaturesEnabled=context.sampleLocationsEnabled=context.synchronization2Enabled=true;
    // Exercise unwritten/rejected exports without touching an XR destination.
    // Three cycles require reuse of both external binary semaphores, including
    // the Vulkan->GL signal left by cancellation. Do not infer pixel validity
    // from these cycles; the D32 raster/readback below checks fresh content.
    for(auto format : {qvr::gl_vulkan::Format::SrgbColorBytes,
                       qvr::gl_vulkan::Format::MotionRGBA16F,
                       qvr::gl_vulkan::Format::DepthD32F}) {
        qvr::gl_vulkan::Bridge cancelled;
        BridgeCleanup cancelledCleanup{s,cancelled};
        bridgeResult(s,cancelled,cancelled.initialize(context,format,extent,extent));
        for(unsigned cycle=0;cycle<3;++cycle) {
            bridgeResult(s,cancelled,cancelled.beginGlWrite());
            bridgeResult(s,cancelled,cancelled.cancelGlWrite());
            need(cancelled.healthy(),"cancelled bridge became unavailable");
            std::printf("BRIDGE_CANCEL_REUSE format=%u cycle=%u written=0 cancelled=1 destinationAccess=0\n",static_cast<unsigned>(format),cycle);
        }
        bridgeResult(s,cancelled,cancelled.shutdown());
    }
    bridgeResult(s,bridge,bridge.initialize(context,qvr::gl_vulkan::Format::DepthD32F,extent,extent));
    bridgeResult(s,motionBridge,motionBridge.initialize(context,qvr::gl_vulkan::Format::MotionRGBA16F,extent,extent));
    MotionTarget motionTarget{s,motionBridge};motionTarget.init();
    XrSwapchainCreateInfo info{XR_TYPE_SWAPCHAIN_CREATE_INFO};info.format=VK_FORMAT_D32_SFLOAT;info.width=info.height=extent;info.sampleCount=info.faceCount=info.arraySize=info.mipCount=1;
    info.usageFlags=XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
    std::printf("XR_CREATE_D32_BEGIN requested_usage=0x%llx format=%lld extent=64x64 sample=1 transfer_requested=0\n",static_cast<unsigned long long>(info.usageFlags),static_cast<long long>(info.format));
    const XrResult created=xrCreateSwapchain(s.session,&info,&s.chain);xr(created,"create sampled depth XR swapchain"); unsigned count{};xr(xrEnumerateSwapchainImages(s.chain,0,&count,nullptr),"XR image count");need(count>0,"empty XR image set");
    std::vector<XrSwapchainImageVulkanKHR> images(count,{XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR});xr(xrEnumerateSwapchainImages(s.chain,count,&count,reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data())),"enumerate XR depth images");
    std::printf("XR_CREATE_D32_END imageCount=%u\n",count);
    std::vector<qvr::gl_vulkan::Destination> destinations;
    for(auto image:images) {
        auto d=qvr::gl_vulkan::destinationFromSwapchainRequest(image.image,info,created);
        need(d.image && (d.guaranteedUsage&VK_IMAGE_USAGE_SAMPLED_BIT),"sampled readback not guaranteed by XR request");
        std::printf("XR_REQUEST_CONTRACT image=%s guaranteed_usage=0x%x source=xr-success-request depthRoute=attachment-only\n",handle(d.image).c_str(),d.guaranteedUsage);
        const auto evidence=creation(capture,image.image);
        need((evidence.usage&d.guaranteedUsage)==d.guaranteedUsage,"captured usage contradicts successful XR request");
        destinations.push_back(d);
    }
    Sampling sampling(s);sampling.init(images);
    const GLuint program=rasterProgram();GLuint vao{};glGenVertexArrays(1,&vao);glBindVertexArray(vao);glGenFramebuffers(1,&s.fbo);glBindFramebuffer(GL_FRAMEBUFFER,s.fbo);glDrawBuffer(GL_COLOR_ATTACHMENT0);glReadBuffer(GL_NONE);
    startSession(s);
    for(unsigned cycle=0;cycle<3;++cycle) {
        // First render a different pattern and reject it. Reacquire and redraw
        // before copying, so the readback also detects stale cancelled content.
        for(unsigned attempt=0;attempt<2;++attempt) {
            bridgeResult(s,motionBridge,motionBridge.beginGlWrite());
            bridgeResult(s,bridge,bridge.beginGlWrite());glBindFramebuffer(GL_FRAMEBUFFER,s.fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,bridge.glTexture(),0);
            glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,motionBridge.glTexture(),0);
            need(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"raster staging FBO incomplete");
            glViewport(0,0,extent,extent);glDisable(GL_SCISSOR_TEST);glDisable(GL_STENCIL_TEST);glDisable(GL_CULL_FACE);glEnable(GL_DEPTH_TEST);glDepthFunc(GL_ALWAYS);glDepthMask(GL_TRUE);glUseProgram(program);glUniform1i(0,cycle+(attempt==0?1:0));
            glDrawArrays(GL_TRIANGLES,0,3);gl("GL paired motion/gl_FragDepth pattern draw");glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,0,0);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,0,0);glBindFramebuffer(GL_FRAMEBUFFER,0);gl("detach staging before handoff");
            if(attempt==0) {
                const bool cancelled=qvr::gl_vulkan::completePairedExports(motionBridge,bridge,
                    [&]{return motionBridge.enqueueCancelGlWrite();},[&]{return bridge.enqueueCancelGlWrite();});
                bridgeResult(s,bridge,cancelled);
                need(motionBridge.healthy() && bridge.healthy(),"paired cancellation incomplete");
                std::printf("BRIDGE_PAIRED_CANCEL cycle=%u written=1 cancelled=2 destinationAccess=0\n",cycle);
            }
        }
        frameBegin(s);const unsigned index=acquire(s);need(index<images.size(),"bad XR index");auto d=destinations[index];d.acquiredAndWaited=true;
        const auto motion=motionTarget.acquireImage();
        const bool copied=qvr::gl_vulkan::completePairedExports(motionBridge,bridge,
            [&]{return motionBridge.enqueueCopyTo(motion);},[&]{return bridge.enqueueCopyTo(d);});
        bridgeResult(s,bridge,copied);need(motionBridge.healthy() && bridge.healthy(),"paired completion incomplete");
        std::printf("BRIDGE_PAIRED_COPY cycle=%u motion_index=%u depth_index=%u bothComplete=1\n",cycle,motionTarget.index,index);
        motionTarget.read(cycle);
        sampling.read(index,d.image);const float* data=static_cast<const float*>(sampling.mapped);unsigned mismatches{};
        for(unsigned y=0;y<extent;++y)for(unsigned x=0;x<extent;++x){const unsigned glY=extent-1-y;const unsigned q=(x>=extent/2?1:0)+(glY>=extent/2?2:0);const float expectedValue=expectedDepth(q,cycle);const float value=data[y*extent+x];mismatches+=!std::isfinite(value)||std::abs(value-expectedValue)>1e-6f;}
        std::printf("RASTER_READBACK cycle=%u pixels=4096 mismatches=%u row0=XR-top value0=%g valueLast=%g\n",cycle,mismatches,data[0],data[extent*extent-1]);need(!mismatches,"rasterized depth/origin mismatch");
        motionTarget.release();XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};xr(xrReleaseSwapchainImage(s.chain,&release),"release depth image");s.waited=false;frameEnd(s);
    }
    // Inject a host-side refusal of the second enqueue, without provoking a
    // driver fault. Both XR images remain acquired until explicit cleanup.
    frameBegin(s);(void)acquire(s);(void)motionTarget.acquireImage();
    bridgeResult(s,motionBridge,motionBridge.beginGlWrite());bridgeResult(s,bridge,bridge.beginGlWrite());
    const bool partial=qvr::gl_vulkan::completePairedExports(motionBridge,bridge,
        [&]{return motionBridge.enqueueCancelGlWrite();},[]{return false;});
    need(!partial && !motionBridge.healthy() && s.waited && motionTarget.waited,"partial enqueue lost tracked ownership");
    need(!motionBridge.beginGlWrite() && !motionBridge.enqueueCancelGlWrite(),"pending bridge allowed command/semaphore reuse");
    bridgeResult(s,motionBridge,motionBridge.waitForCompletion());bridgeResult(s,bridge,bridge.cancelGlWrite());
    motionTarget.releaseAfterDrain();XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};xr(xrReleaseSwapchainImage(s.chain,&release),"release depth after partial pair");s.waited=false;frameEnd(s);
    std::printf("PAIRED_PARTIAL_ENQUEUE rejectedSecond=1 pendingReuseRejected=1 cleanupComplete=1 releasedAfterDrain=1\n");
    glDeleteProgram(program);glDeleteVertexArrays(1,&vao);bridgeResult(s,bridge,bridge.shutdown());
    std::printf("PASS pairedMotionDepth=1 rasterizedDepth=1 attachmentFallback=1 cycles=3 depthPixels=12288 motionPixels=12288 cancellationCycles=17 cancelFormats=3 freshAfterCancel=1 partialEnqueueCleanup=1 transferAccessToXRDepth=0 projectionLayers=0 synthesisMetadata=0\n");return 0;
}
} // namespace
int main(int argc,char** argv)
{
    std::setvbuf(stdout,nullptr,_IONBF,0);
    if(argc!=4 || std::strcmp(argv[1],"--run") || std::strcmp(argv[2],"--api-dump")) {std::fprintf(stderr,"usage: --run --api-dump SAME_RUN_STDOUT_FILE (requires VK_LAYER_LUNARG_api_dump)\n");return 2;}
    try{return runRaster(argv[3]);}
    catch(const std::exception& e){std::fprintf(stderr,"RASTER_FAIL %s\n",e.what());return 2;}
}
