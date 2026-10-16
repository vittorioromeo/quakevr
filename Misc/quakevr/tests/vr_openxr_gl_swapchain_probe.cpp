#define XR_USE_GRAPHICS_API_OPENGL
#define XR_USE_PLATFORM_XLIB
#include <X11/Xlib.h>
#include <GL/glx.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <SDL.h>
#include <SDL_syswm.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
bool xrOk(XrResult result, const char* label)
{
    if(XR_SUCCEEDED(result)) return true;
    std::cerr << label << " failed: " << result << '\n';
    return false;
}

bool hasExtension(const std::vector<XrExtensionProperties>& extensions, const char* name)
{
    return std::any_of(extensions.begin(), extensions.end(), [name](const auto& extension)
    {
        return std::strcmp(extension.extensionName, name) == 0;
    });
}

std::string timestamp()
{
    const auto now = std::chrono::system_clock::now();
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
    localtime_r(&time, &local);
    std::ostringstream output;
    output << std::put_time(&local, "%Y-%m-%d %H:%M:%S") << '.' << std::setfill('0') << std::setw(3)
           << milliseconds.count();
    return output.str();
}

struct ProbeCase
{
    const char* name;
    int64_t format;
    XrSwapchainUsageFlags usage;
};
}

int main(int argc, char** argv)
{
    if(argc != 2)
    {
        std::cerr << "usage: vr_openxr_gl_swapchain_probe color|motion|depth|depth_noext|depth32_attach|depth24|depth16\n";
        return 2;
    }
    const std::string mode = argv[1];
    const ProbeCase cases[] = {
        {"color", GL_SRGB8_ALPHA8, XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT},
        {"motion", GL_SRGB8_ALPHA8, XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT},
        {"depth", GL_DEPTH_COMPONENT32F, XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT},
        {"depth_noext", GL_DEPTH_COMPONENT32F, XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT},
        {"depth32_attach", GL_DEPTH_COMPONENT32F, XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT},
        {"depth24", GL_DEPTH_COMPONENT24, XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT},
        {"depth16", GL_DEPTH_COMPONENT16, XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT}
    };
    const ProbeCase* selected = nullptr;
    for(const auto& candidate : cases)
        if(mode == candidate.name) selected = &candidate;
    if(!selected)
    {
        std::cerr << "unknown mode: " << mode << '\n';
        return 2;
    }

    uint32_t extensionCount = 0;
    if(!xrOk(xrEnumerateInstanceExtensionProperties(nullptr, 0, &extensionCount, nullptr), "extension count")) return 3;
    std::vector<XrExtensionProperties> extensions(extensionCount, {XR_TYPE_EXTENSION_PROPERTIES});
    if(!xrOk(xrEnumerateInstanceExtensionProperties(nullptr, extensionCount, &extensionCount, extensions.data()), "extensions")) return 3;
    const char* glExtension = XR_KHR_OPENGL_ENABLE_EXTENSION_NAME;
    const char* synthesisExtension = "XR_EXT_frame_synthesis";
    const bool enableSynthesis = mode != "depth_noext";
    if(!hasExtension(extensions, glExtension) || (enableSynthesis && !hasExtension(extensions, synthesisExtension)))
    {
        std::cerr << "required extensions unavailable: OpenGL=" << hasExtension(extensions, glExtension)
                  << " frame_synthesis=" << hasExtension(extensions, synthesisExtension) << '\n';
        return 4;
    }

    XrInstanceCreateInfo instanceInfo{XR_TYPE_INSTANCE_CREATE_INFO};
    std::strcpy(instanceInfo.applicationInfo.applicationName, "QuakeVR GL swapchain probe");
    instanceInfo.applicationInfo.applicationVersion = 1;
    std::strcpy(instanceInfo.applicationInfo.engineName, "QuakeVR test fixture");
    instanceInfo.applicationInfo.engineVersion = 1;
    // Match the SteamVR runtime's supported API version; newer headers do not
    // imply that the selected runtime accepts their XR_CURRENT_API_VERSION.
    instanceInfo.applicationInfo.apiVersion = XR_MAKE_VERSION(1, 0, 0);
    const char* enabled[] = {glExtension, synthesisExtension};
    instanceInfo.enabledExtensionCount = enableSynthesis ? 2u : 1u;
    instanceInfo.enabledExtensionNames = enabled;
    XrInstance instance = XR_NULL_HANDLE;
    if(!xrOk(xrCreateInstance(&instanceInfo, &instance), "xrCreateInstance")) return 5;

    XrSystemGetInfo systemInfo{XR_TYPE_SYSTEM_GET_INFO};
    systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    XrSystemId system = XR_NULL_SYSTEM_ID;
    if(!xrOk(xrGetSystem(instance, &systemInfo, &system), "xrGetSystem"))
    {
        xrDestroyInstance(instance);
        return 6;
    }

    PFN_xrGetOpenGLGraphicsRequirementsKHR getRequirements = nullptr;
    if(!xrOk(xrGetInstanceProcAddr(instance, "xrGetOpenGLGraphicsRequirementsKHR",
                                   reinterpret_cast<PFN_xrVoidFunction*>(&getRequirements)), "OpenGL entrypoint") || !getRequirements)
    {
        xrDestroyInstance(instance);
        return 7;
    }
    XrGraphicsRequirementsOpenGLKHR requirements{XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR};
    if(!xrOk(getRequirements(instance, system, &requirements), "OpenGL requirements"))
    {
        xrDestroyInstance(instance);
        return 8;
    }

    if(SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        std::cerr << "SDL_Init: " << SDL_GetError() << '\n';
        xrDestroyInstance(instance);
        return 9;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 0);
    SDL_Window* window = SDL_CreateWindow("OpenXR swapchain probe", SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    if(!window)
    {
        std::cerr << "SDL_CreateWindow: " << SDL_GetError() << '\n';
        SDL_Quit();
        xrDestroyInstance(instance);
        return 10;
    }
    SDL_GLContext context = SDL_GL_CreateContext(window);
    if(!context)
    {
        std::cerr << "SDL_GL_CreateContext: " << SDL_GetError() << '\n';
        SDL_DestroyWindow(window);
        SDL_Quit();
        xrDestroyInstance(instance);
        return 11;
    }
    SDL_SysWMinfo wm{};
    SDL_VERSION(&wm.version);
    if(!SDL_GetWindowWMInfo(window, &wm) || wm.subsystem != SDL_SYSWM_X11)
    {
        std::cerr << "expected SDL X11 window: " << SDL_GetError() << '\n';
        SDL_GL_DeleteContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        xrDestroyInstance(instance);
        return 12;
    }
    XWindowAttributes xattrs{};
    if(!XGetWindowAttributes(wm.info.x11.display, wm.info.x11.window, &xattrs))
    {
        std::cerr << "XGetWindowAttributes failed\n";
        SDL_GL_DeleteContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        xrDestroyInstance(instance);
        return 13;
    }
    XrGraphicsBindingOpenGLXlibKHR binding{XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR};
    binding.xDisplay = wm.info.x11.display;
    binding.visualid = XVisualIDFromVisual(xattrs.visual);
    binding.glxFBConfig = nullptr;
    binding.glxDrawable = wm.info.x11.window;
    binding.glxContext = static_cast<GLXContext>(context);
    XrSessionCreateInfo sessionInfo{XR_TYPE_SESSION_CREATE_INFO};
    sessionInfo.next = &binding;
    sessionInfo.systemId = system;
    XrSession session = XR_NULL_HANDLE;
    if(!xrOk(xrCreateSession(instance, &sessionInfo, &session), "xrCreateSession"))
    {
        SDL_GL_DeleteContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        xrDestroyInstance(instance);
        return 14;
    }

    uint32_t formatCount = 0;
    if(!xrOk(xrEnumerateSwapchainFormats(session, 0, &formatCount, nullptr), "swapchain format count")) return 15;
    std::vector<int64_t> formats(formatCount);
    if(!xrOk(xrEnumerateSwapchainFormats(session, formatCount, &formatCount, formats.data()), "swapchain formats")) return 15;
    std::cout << "mode=" << selected->name << " requested_format=" << selected->format
              << " usage=0x" << std::hex << selected->usage << std::dec << " runtime_formats=";
    for(int64_t format : formats) std::cout << format << ',';
    std::cout << '\n';
    if(std::find(formats.begin(), formats.end(), selected->format) == formats.end())
    {
        std::cerr << "requested format is not advertised for this OpenGL session\n";
        xrDestroySession(session);
        SDL_GL_DeleteContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        xrDestroyInstance(instance);
        return 16;
    }

    XrSwapchainCreateInfo swapchainInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
    swapchainInfo.usageFlags = selected->usage;
    swapchainInfo.format = selected->format;
    swapchainInfo.sampleCount = 1;
    swapchainInfo.width = 432;
    swapchainInfo.height = 432;
    swapchainInfo.faceCount = 1;
    swapchainInfo.arraySize = 1;
    swapchainInfo.mipCount = 1;
    XrSwapchain swapchain = XR_NULL_HANDLE;
    std::cout << "before_xrCreateSwapchain=" << timestamp() << '\n';
    const XrResult createResult = xrCreateSwapchain(session, &swapchainInfo, &swapchain);
    std::cout << "after_xrCreateSwapchain=" << timestamp() << " result=" << createResult << '\n';
    if(XR_SUCCEEDED(createResult))
    {
        uint32_t imageCount = 0;
        std::cout << "before_xrEnumerateSwapchainImages_count=" << timestamp() << '\n';
        const XrResult countResult = xrEnumerateSwapchainImages(swapchain, 0, &imageCount, nullptr);
        std::cout << "after_xrEnumerateSwapchainImages_count=" << timestamp() << " result=" << countResult
                  << " count=" << imageCount << '\n';
        if(XR_SUCCEEDED(countResult))
        {
            std::vector<XrSwapchainImageOpenGLKHR> images(imageCount, {XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR});
            std::cout << "before_xrEnumerateSwapchainImages=" << timestamp() << '\n';
            const XrResult imagesResult = xrEnumerateSwapchainImages(swapchain, imageCount, &imageCount,
                reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data()));
            std::cout << "after_xrEnumerateSwapchainImages=" << timestamp() << " result=" << imagesResult
                      << " count=" << imageCount << " texture_ids=";
            if(XR_SUCCEEDED(imagesResult)) for(const auto& image : images) std::cout << image.image << ',';
            std::cout << '\n';
        }
        const XrResult destroyResult = xrDestroySwapchain(swapchain);
        std::cout << "xrDestroySwapchain_result=" << destroyResult << '\n';
    }
    xrDestroySession(session);
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    xrDestroyInstance(instance);
    return XR_SUCCEEDED(createResult) ? 0 : 17;
}
