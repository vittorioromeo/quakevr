// vr_backend_openxr.cpp -- OpenXR runtime backend (OpenGL, Windows).
//
// Session lifecycle, stage reference space, the controllers' actions (poses, buttons, sticks,
// haptics), one swapchain per eye and one for the 2D panel, and the wait/begin/locate/end frame
// loop. The graphics binding (XR_KHR_opengl_enable, WGL, GL swapchain formats and images) is
// the renderer-bound part: a Vulkan engine binds with XR_KHR_vulkan_enable2 instead
// (docs/vr-port/PORTING.md).

#include "vr_backend_openxr_internal.hpp"

#ifdef QVR_HAVE_OPENXR

namespace qvr::oxr
{
namespace
{

// The debug messenger (any thread; every run, vr_diagnostics_xr): the message to the run's openxr_debug.log
// (vr_runlogs.cpp, rate-limited).
XRAPI_ATTR XrBool32 XRAPI_CALL debugMessage(XrDebugUtilsMessageSeverityFlagsEXT severity, XrDebugUtilsMessageTypeFlagsEXT,
    const XrDebugUtilsMessengerCallbackDataEXT* data, void*)
{
    const char* level = (severity & XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)     ? "error"
                        : (severity & XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) ? "warning"
                        : (severity & XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)    ? "info"
                                                                                       : "verbose";
    VR_DiagnosticsXr(level, data ? data->messageId : nullptr, data ? data->functionName : nullptr, data ? data->message : nullptr);
    return XR_FALSE;
}

} // namespace


OpenXrBackend::~OpenXrBackend()
{
    stop();
}

const char* OpenXrBackend::name() const
{
    return "openxr";
}

const char* OpenXrBackend::runtimeName() const
{
    return runtime[0] ? runtime : "openxr";
}

bool OpenXrBackend::start()
{
    xrruntime::beginLog();
    const xrruntime::Plan plan = xrruntime::plan();
    xrruntime::note("OpenXR runtime choice: %s\n", plan.summary.cStr());
    for(size_t i = 0; i < plan.attempts.size(); i++)
    {
        const xrruntime::Attempt& attempt = plan.attempts[i];
        if(i > 0 && !plan.fallback)
        {
            break;
        }
        xrruntime::use(plan, attempt);
        if(xrruntime::simulatedFailure(attempt))
        {
            xrruntime::warn("OpenXR: %s failed (simulated: vr_xr_test_fail, or no manifest in the test environment)\n",
                attempt.label.cStr());
            continue;
        }
        currentAttempt = attempt;
        q_snprintf(crashBase, sizeof(crashBase), "OpenXR, starting %s", attempt.label.cStr());
        VR_SetCrashVr(crashBase);
        if(startRuntime())
        {
            xrruntime::setOutcome(plan, static_cast<int>(i));
            step("started");
            return true;
        }
        xrruntime::warn("OpenXR: %s failed to start\n", attempt.label.cStr());
        if(runtimeFaulted)
        {
            // Said in the game too: what to change (the log has the layers and overlays).
            Con_Printf("\x02VR: %s crashed (%s): the game caught it and %s.\n", runtime[0] ? runtime : attempt.label.cStr(),
                faultText, i + 1 < plan.attempts.size() && plan.fallback ? "tries the next runtime" : "plays flat");
            Con_Printf("VR: %svr_xr_api_layers 0 turns off the OpenXR API layers installed (overlays, toolkits); "
                       "qvr_openxr.txt has the details\n",
                vdxr ? "set Virtual Desktop's OpenXR runtime to SteamVR (the Streamer's settings), or " : "");
            VR_LogsIncident(va("%s crashed (%s), caught", runtime[0] ? runtime : attempt.label.cStr(), faultText));
        }
        stop(); // (all of it: the next runtime starts from nothing)
        resetRuntimeState();
    }
    xrruntime::setOutcome(plan, -1);
    return false;
}

bool OpenXrBackend::startRuntime()
{
    if(!(createInstance() && createSystem() && createSession() && createSpaces() &&
           createActions() && createSwapchains()))
    {
        return false;
    }
    bodytrack::xr::create(instance, systemId, session); // (vr_bodytrack_xr.cpp: when the runtime offers it)

    // The runtime paces frames (xrWaitFrame): the desktop mirror must not wait for vsync.
    SDL_GL_SetSwapInterval(0);
    swapIntervalChanged = true;
    return true;
}

void OpenXrBackend::stop()
{
    if(instance != XR_NULL_HANDLE)
    {
        xrruntime::keepGraphicsModules();
        xrruntime::logLine(va("stopping %s\n", runtime[0] ? runtime : "the runtime"));
    }
    if(frameBegun)
    {
        endFrame(false);
    }

    for(Swapchain* sc : {&swapchains[0], &swapchains[1], &panel})
    {
        if(sc->handle != XR_NULL_HANDLE)
        {
            const XrSwapchain handle = sc->handle;
            guarded("xrDestroySwapchain", [handle] { return xrDestroySwapchain(handle); });
            *sc = Swapchain{};
        }
    }
    panelPending = panelShown = false;
    haveLastViews = holding = false;
    timing = Timing{};
    for(int eye = 0; eye < 2; eye++)
    {
        hidden[eye] = HiddenArea{};
        hiddenStale[eye] = true;
    }
    getVisibilityMask = nullptr;

    bodytrack::xr::destroy();
    for(XrSpace& space : handSpaces)
    {
        destroySpace(space);
    }
    destroySpace(viewSpace);
    destroySpace(worldSpace);

    if(messenger != XR_NULL_HANDLE)
    {
        PFN_xrDestroyDebugUtilsMessengerEXT destroy = nullptr;
        xrGetInstanceProcAddr(instance, "xrDestroyDebugUtilsMessengerEXT", reinterpret_cast<PFN_xrVoidFunction*>(&destroy));
        if(destroy)
        {
            destroy(messenger);
        }
        messenger = XR_NULL_HANDLE;
    }

    if(actionSet != XR_NULL_HANDLE)
    {
        xrDestroyActionSet(actionSet);
        actionSet = XR_NULL_HANDLE;
    }

    if(session != XR_NULL_HANDLE)
    {
        const XrSession handle = session;
        guarded("xrDestroySession", [handle] { return xrDestroySession(handle); });
        session = XR_NULL_HANDLE;
    }

    if(instance != XR_NULL_HANDLE)
    {
        const XrInstance handle = instance;
        guarded("xrDestroyInstance", [handle] { return xrDestroyInstance(handle); });
        instance = XR_NULL_HANDLE;
        q_snprintf(crashBase, sizeof(crashBase), "OpenXR stopped (%s)", runtime[0] ? runtime : "no runtime");
        VR_SetCrashVr(crashBase);
    }

    if(sessionRunning || swapIntervalChanged)
    {
        SDL_GL_SetSwapInterval(Cvar_VariableValue("vid_vsync") != 0.f ? 1 : 0);
    }
    sessionRunning = false;
}

const char* OpenXrBackend::wallClock()
{
#ifdef _WIN32
    SYSTEMTIME t;
    GetLocalTime(&t);
    return va("%02d:%02d:%02d.%03d", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
#else
    timespec ts{};
    clock_gettime(CLOCK_REALTIME, &ts);
    tm local{};
    localtime_r(&ts.tv_sec, &local);
    return va("%02d:%02d:%02d.%03ld", local.tm_hour, local.tm_min, local.tm_sec, ts.tv_nsec / 1000000);
#endif
}

const char* OpenXrBackend::stateName(XrSessionState state)
{
    switch(state)
    {
        case XR_SESSION_STATE_IDLE: return "IDLE";
        case XR_SESSION_STATE_READY: return "READY";
        case XR_SESSION_STATE_SYNCHRONIZED: return "SYNCHRONIZED";
        case XR_SESSION_STATE_VISIBLE: return "VISIBLE";
        case XR_SESSION_STATE_FOCUSED: return "FOCUSED";
        case XR_SESSION_STATE_STOPPING: return "STOPPING";
        case XR_SESSION_STATE_LOSS_PENDING: return "LOSS_PENDING";
        case XR_SESSION_STATE_EXITING: return "EXITING";
        default: return "UNKNOWN";
    }
}

void OpenXrBackend::noteState(const char* name)
{
    if(stateTrailCount == stateTrailMax)
    {
        for(int i = 1; i < stateTrailMax; i++)
        {
            stateTrail[i - 1] = stateTrail[i];
        }
        stateTrailCount--;
    }
    stateTrail[stateTrailCount++] = name;
    stateTrailText[0] = '\0';
    for(int i = 0; i < stateTrailCount; i++)
    {
        q_strlcat(stateTrailText, i ? " > " : "", sizeof(stateTrailText));
        q_strlcat(stateTrailText, stateTrail[i], sizeof(stateTrailText));
    }
}

OpenXrBackend::SessionEnd OpenXrBackend::sessionEnd() const
{
    return endReason;
}

const char* OpenXrBackend::sessionStates() const
{
    return stateTrailText;
}

bool OpenXrBackend::check(XrResult result, const char* what) const
{
    if(XR_SUCCEEDED(result))
    {
        return true;
    }

    char text[XR_MAX_RESULT_STRING_SIZE] = "";
    if(instance != XR_NULL_HANDLE)
    {
        xrResultToString(instance, result, text);
    }
    xrruntime::warn("OpenXR: %s failed: %s (%d)\n", what, text, static_cast<int>(result));
    return false;
}

void OpenXrBackend::step(const char* format, ...)
{
    char text[512];
    va_list args;
    va_start(args, format);
    q_vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    VR_GLStep("OpenXR: %s", text);
    VR_DiagnosticsNote(va("step: %s\n", text));
    xrruntime::logLine(va("%s OpenXR: %s\n", wallClock(), text));
    setCrashVr(text);
}

void OpenXrBackend::destroySpace(XrSpace& space)
{
    if(space != XR_NULL_HANDLE)
    {
        xrDestroySpace(space);
        space = XR_NULL_HANDLE;
    }
}

void OpenXrBackend::setCrashVr(const char* lastStep)
{
    char controllers[2 * XR_MAX_PATH_LENGTH + 32] = "";
    if(profileName[0][0] || profileName[1][0])
    {
        q_snprintf(controllers, sizeof(controllers), "; controllers %s / %s",
            profileName[0][0] ? shortProfile(profileName[0]) : "none", profileName[1][0] ? shortProfile(profileName[1]) : "none");
    }
    VR_SetCrashVr(lastStep ? va("%s%s; last OpenXR step: %s", crashBase, controllers, lastStep)
                           : va("%s%s", crashBase, controllers));
}

void OpenXrBackend::resetRuntimeState()
{
    systemId = XR_NULL_SYSTEM_ID;
    sessionState = XR_SESSION_STATE_UNKNOWN;
    stateTrailCount = 0;
    stateTrailText[0] = '\0';
    endReason = SessionEnd::Lost;
    visibilityMaskExtension = false;
#ifndef _WIN32
    eglExtension = false;
#endif
    for(bool& v : vendorProfile)
    {
        v = false;
    }
    runtime[0] = '\0';
    vdxr = false;
    runtimeFaulted = false;
    debugUtils = false;
    faultText[0] = '\0';
    offeredFormats.clear();
    maxImageWidth = maxImageHeight = 0;
}

bool OpenXrBackend::createInstance()
{
    // Quest 3 controllers get their own profile with this extension (Touch otherwise).
    za::Vector<const char*> extensions{XR_KHR_OPENGL_ENABLE_EXTENSION_NAME};
    bodytrack::xr::reset();
    uint32_t available = 0;
    xrEnumerateInstanceExtensionProperties(nullptr, 0, &available, nullptr);
    za::Vector<XrExtensionProperties> extensionList(available, XrExtensionProperties{XR_TYPE_EXTENSION_PROPERTIES});
    xrEnumerateInstanceExtensionProperties(nullptr, available, &available, extensionList.data());
    za::String offered;
    for(const XrExtensionProperties& p : extensionList)
    {
        offered += offered.empty() ? "" : " ";
        offered += p.extensionName;
    }
    xrruntime::logLine(va("OpenXR: %u extensions offered: ", available));
    xrruntime::logLine(offered.cStr()); // (longer than va's buffer)
    xrruntime::logLine("\n");
    for(const XrExtensionProperties& p : extensionList)
    {
        if(!strcmp(p.extensionName, "XR_META_touch_controller_plus"))
        {
            extensions.pushBack("XR_META_touch_controller_plus");
        }
        for(int v = 0; v < VendorCount; v++)
        {
            if(vr_xr_vendor_profiles.value != 0.f && !strcmp(p.extensionName, vendorExtensions[v]))
            {
                extensions.pushBack(vendorExtensions[v]);
                vendorProfile[v] = true;
                xrruntime::logLine(va("OpenXR: the %s controller profile enabled (vr_xr_vendor_profiles)\n", vendorExtensions[v]));
            }
        }
#ifndef _WIN32
        // Linux: the EGL binding, for an EGL context (SDL's Wayland driver, or X11 on EGL): Monado and WiVRn
        // have it, SteamVR doesn't (createSession).
        if(!strcmp(p.extensionName, XR_MNDX_EGL_ENABLE_EXTENSION_NAME))
        {
            extensions.pushBack(XR_MNDX_EGL_ENABLE_EXTENSION_NAME);
            eglExtension = true;
        }
#endif
        // The lenses' hidden area (vr_visibility_mask).
        if(!strcmp(p.extensionName, XR_KHR_VISIBILITY_MASK_EXTENSION_NAME))
        {
            extensions.pushBack(XR_KHR_VISIBILITY_MASK_EXTENSION_NAME);
            visibilityMaskExtension = true;
        }
        // Body tracking (vr_bodytrack_xr.cpp): the FB tracker, Meta's full body, fidelity and calibration.
        if(const char* body = bodytrack::xr::wanted(p.extensionName))
        {
            extensions.pushBack(body);
        }
        // The loader's and the runtime's own messages (every run: logs/<date>_<time>/openxr_debug.log).
        if(vr_diagnostics_xr.value != 0.f && !strcmp(p.extensionName, XR_EXT_DEBUG_UTILS_EXTENSION_NAME))
        {
            extensions.pushBack(XR_EXT_DEBUG_UTILS_EXTENSION_NAME);
            debugUtils = true;
        }
    }
    // Diagnostics mode: Khronos' core validation layer, an explicit layer: shipped in quakevr/xr_layers/, which
    // diagnostics puts in XR_API_LAYER_PATH (vr_diagnostics.cpp, useValidationLayer: its output goes to
    // openxr_validation.log); or one the player installed. Never outside diagnostics mode (ROUND22.md).
    za::Vector<const char*> layers;
    if(VR_DiagnosticsOn())
    {
        uint32_t layerCount = 0;
        xrEnumerateApiLayerProperties(0, &layerCount, nullptr);
        za::Vector<XrApiLayerProperties> layerList(layerCount, XrApiLayerProperties{XR_TYPE_API_LAYER_PROPERTIES});
        if(layerCount)
        {
            xrEnumerateApiLayerProperties(layerCount, &layerCount, layerList.data());
        }
        za::String names;
        for(const XrApiLayerProperties& l : layerList)
        {
            names += va("%s%s", names.empty() ? "" : " ", l.layerName);
            if(vr_diagnostics_xr_validation.value != 0.f && !strcmp(l.layerName, "XR_APILAYER_LUNARG_core_validation"))
            {
                layers.pushBack("XR_APILAYER_LUNARG_core_validation");
            }
        }
        const char* validation = !layers.empty()                             ? "ON (openxr_validation.log)"
                                 : vr_diagnostics_xr_validation.value != 0.f ? "not found (not used)"
                                                                             : "off (vr_diagnostics_xr_validation 0)";
        VR_DiagnosticsNote(va("OpenXR API layers the loader lists: %s; core validation: %s; debug messenger: %s\n",
            names.empty() ? "none" : names.cStr(), validation,
            debugUtils ? "on (XR_EXT_debug_utils)" : "not offered"));
    }

    XrInstanceCreateInfo info{XR_TYPE_INSTANCE_CREATE_INFO};
    q_strlcpy(info.applicationInfo.applicationName, "Quake VR", XR_MAX_APPLICATION_NAME_SIZE);
    info.applicationInfo.applicationVersion = 1;
    q_strlcpy(info.applicationInfo.engineName, "Ironwail", XR_MAX_ENGINE_NAME_SIZE);
    info.applicationInfo.engineVersion = 1;
    info.applicationInfo.apiVersion = XR_API_VERSION_1_0;
    info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    info.enabledExtensionNames = extensions.data();
    info.enabledApiLayerCount = static_cast<uint32_t>(layers.size());
    info.enabledApiLayerNames = layers.empty() ? nullptr : layers.data();
    // The messenger chained to the instance's creation (the loader's and the runtime's messages while it is made),
    // then one of its own for the instance's life (createDebugMessenger).
    XrDebugUtilsMessengerCreateInfoEXT messengerInfo{XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    // (Verbose only in diagnostics mode: a runtime's per-call chatter.)
    messengerInfo.messageSeverities = (VR_DiagnosticsOn() ? XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT : 0) |
                                      XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                                      XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                      XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    // (Not XR_DEBUG_UTILS_MESSAGE_TYPE_CONFORMANCE_BIT_EXT: the loader fails the instance for that bit.)
    messengerInfo.messageTypes = XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                 XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    messengerInfo.userCallback = debugMessage;
    if(debugUtils && vr_diagnostics_xr.value >= 2.f)
    {
        info.next = &messengerInfo;
    }
    za::String enabled;
    for(const char* e : extensions)
    {
        enabled += enabled.empty() ? "" : " ";
        enabled += e;
    }
    xrruntime::logLine(va("OpenXR: extensions enabled: %s\n", enabled.cStr()));

    step("xrCreateInstance (%s)", currentAttempt.label.cStr());
    XrResult created = xrCreateInstance(&info, &instance);
    if(XR_FAILED(created) && (debugUtils || !layers.empty()))
    {
        // The logs must not cost the VR start: again without the messenger and the validation layer.
        VR_DiagnosticsNote(va("xrCreateInstance failed (%d) with the debug messenger/validation layer (instance %p left): "
                              "again without\n",
            static_cast<int>(created), static_cast<void*>(instance)));
        if(instance != XR_NULL_HANDLE)
        {
            xrDestroyInstance(instance); // (the loader's own failure after the runtime's instance was made)
            instance = XR_NULL_HANDLE;
        }
        xrruntime::warn("OpenXR: xrCreateInstance failed (%d) with the debug messenger: again without it\n",
            static_cast<int>(created));
        debugUtils = false;
        za::Vector<const char*> kept;
        for(const char* e : extensions)
        {
            if(strcmp(e, XR_EXT_DEBUG_UTILS_EXTENSION_NAME) != 0)
            {
                kept.pushBack(e);
            }
        }
        extensions = static_cast<za::Vector<const char*>&&>(kept);
        info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        info.enabledExtensionNames = extensions.data();
        info.enabledApiLayerCount = 0;
        info.enabledApiLayerNames = nullptr;
        info.next = nullptr;
        created = xrCreateInstance(&info, &instance);
    }
    if(!check(created, "xrCreateInstance"))
    {
        return false;
    }

    XrInstanceProperties props{XR_TYPE_INSTANCE_PROPERTIES};
    if(XR_SUCCEEDED(xrGetInstanceProperties(instance, &props)))
    {
        q_snprintf(runtime, sizeof(runtime), "%s %u.%u.%u", props.runtimeName, XR_VERSION_MAJOR(props.runtimeVersion),
            XR_VERSION_MINOR(props.runtimeVersion), XR_VERSION_PATCH(props.runtimeVersion));
        vdxr = !strncmp(props.runtimeName, "VirtualDesktopXR", 16);
        Con_Printf("OpenXR runtime: %s\n", runtime);
    }
    xrruntime::loaded(currentAttempt, runtime[0] ? runtime : "a runtime without a name");
    if(debugUtils)
    {
        PFN_xrCreateDebugUtilsMessengerEXT create = nullptr;
        xrGetInstanceProcAddr(instance, "xrCreateDebugUtilsMessengerEXT", reinterpret_cast<PFN_xrVoidFunction*>(&create));
        const bool made = create && XR_SUCCEEDED(create(instance, &messengerInfo, &messenger));
        VR_DiagnosticsNote(va("OpenXR %s: the debug messenger %s\n", runtime, made ? "on" : "FAILED"));
    }
    // The crash report's VR line from here: which runtime, and what sits between it and the game.
    const za::String around = xrruntime::layersAndOverlays();
    xrruntime::logLine(va("OpenXR: %s\n", around.cStr()));
    VR_LogsSystemLine(va("VR: OpenXR %s (%s); %s\n", runtime[0] ? runtime : "a runtime without a name",
        currentAttempt.label.cStr(), around.cStr())); // (the run's system.txt: vr_runlogs.cpp)
    q_snprintf(crashBase, sizeof(crashBase), "OpenXR %s (%s); %s", runtime[0] ? runtime : "a runtime without a name",
        currentAttempt.label.cStr(), around.cStr());
    VR_SetCrashVr(crashBase);

    return true;
}

bool OpenXrBackend::createSystem()
{
    XrSystemGetInfo info{XR_TYPE_SYSTEM_GET_INFO};
    info.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    step("xrGetSystem");
    XrResult result = xrGetSystem(instance, &info, &systemId);
    // SteamVR just started (by this instance, or a moment before) says there is no headset until its driver
    // (Virtual Desktop's, the Link's) finds it: asked again for a while (vr_xr_steamvr_wait).
    const bool steamVR = xrruntime::isSteamVR(currentAttempt) || strstr(runtime, "SteamVR");
    if(result == XR_ERROR_FORM_FACTOR_UNAVAILABLE && steamVR && vr_xr_steamvr_wait.value > 0.f)
    {
        xrruntime::note("OpenXR: SteamVR has no headset yet: asking again for up to %g s (vr_xr_steamvr_wait)\n",
            vr_xr_steamvr_wait.value);
        const double begin = Sys_DoubleTime();
        int tries = 1;
        while(result == XR_ERROR_FORM_FACTOR_UNAVAILABLE && Sys_DoubleTime() - begin < vr_xr_steamvr_wait.value)
        {
#ifdef _WIN32
            Sleep(250);
#else
            usleep(250 * 1000);
#endif
            result = xrGetSystem(instance, &info, &systemId);
            tries++;
        }
        xrruntime::note("OpenXR: SteamVR %s after %.1f s (%d tries)\n",
            XR_SUCCEEDED(result) ? "found the headset" : "still has no headset", Sys_DoubleTime() - begin, tries);
        if(!XR_SUCCEEDED(result))
        {
            // (A SteamVR the game's start started can take longer: then Auto goes on to the next runtime.)
            xrruntime::note("OpenXR: start SteamVR (from Virtual Desktop's menu, with it) before the game, or raise "
                            "vr_xr_steamvr_wait\n");
        }
    }
    if(!check(result, "xrGetSystem (is the headset connected?)"))
    {
        return false;
    }
    XrSystemProperties system{XR_TYPE_SYSTEM_PROPERTIES};
    if(XR_SUCCEEDED(xrGetSystemProperties(instance, systemId, &system)))
    {
        q_strlcpy(this->system, system.systemName, sizeof(this->system));
        maxImageWidth = system.graphicsProperties.maxSwapchainImageWidth;
        maxImageHeight = system.graphicsProperties.maxSwapchainImageHeight;
        const size_t len = strlen(crashBase);
        q_snprintf(crashBase + len, sizeof(crashBase) - len, "; headset \"%s\"", system.systemName);
        xrruntime::logLine(va("OpenXR: system \"%s\" (vendor 0x%x): at most %u layers, swapchain images up to %ux%u\n",
            system.systemName, system.vendorId, system.graphicsProperties.maxLayerCount,
            system.graphicsProperties.maxSwapchainImageWidth, system.graphicsProperties.maxSwapchainImageHeight));
    }

    // Required before creating an OpenGL session.
    PFN_xrGetOpenGLGraphicsRequirementsKHR getRequirements = nullptr;
    xrGetInstanceProcAddr(instance, "xrGetOpenGLGraphicsRequirementsKHR",
        reinterpret_cast<PFN_xrVoidFunction*>(&getRequirements));
    XrGraphicsRequirementsOpenGLKHR requirements{XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR};
    return getRequirements &&
           check(getRequirements(instance, systemId, &requirements), "xrGetOpenGLGraphicsRequirementsKHR");
}
#ifndef _WIN32

bool OpenXrBackend::contextReady(const char* what)
{
    const char* why = "";
    const xrgl::Binding now = xrgl::current(&why);
    const void* nowSurface = now.api == xrgl::Api::Egl ? now.eglDraw : reinterpret_cast<const void*>(now.drawable);
    const void* boundSurface = glctx.api == xrgl::Api::Egl ? glctx.eglDraw : reinterpret_cast<const void*>(glctx.drawable);
    xrruntime::logLine(va("%s OpenXR: %s: %s context %p, surface %p current on thread %lu (the session's: %s %p, %p)\n",
        wallClock(), what, xrgl::apiName(now.api), now.context, nowSurface, static_cast<unsigned long>(pthread_self()),
        xrgl::apiName(glctx.api), glctx.context, boundSurface));
    if(!glctx.context || (glctx.api == xrgl::Api::Glx && !glctx.drawable))
    {
        xrruntime::warn("OpenXR: %s not called: no GL context bound to the session (%s)\n", what,
            now.api != xrgl::Api::None ? "" : why);
        return false;
    }
    if(xrgl::same(now, glctx))
    {
        return true;
    }
    const bool made = xrgl::makeCurrent(glctx);
    xrruntime::warn("OpenXR: %s: the game's %s context wasn't current on this thread (%s %p instead of %p): made "
                    "current again: %s\n",
        what, xrgl::apiName(glctx.api), xrgl::apiName(now.api), now.context, glctx.context,
        made ? "done" : "FAILED: the call not made");
    return made;
}

void OpenXrBackend::logGlInterop()
{
    constexpr const char* names[] = {"glCreateMemoryObjectsEXT", "glImportMemoryFdEXT", "glTextureStorageMem2DEXT",
        "glImportSemaphoreFdEXT"};
    za::String text;
    for(const char* name : names)
    {
        text += va("%s%s %s", text.empty() ? "" : ", ", name, xrgl::proc(glctx.api, name) ? "yes" : "NO");
    }
    const char* extensions = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
    const bool fd = extensions ? strstr(extensions, "GL_EXT_memory_object_fd") != nullptr : true; // (core: not listed)
    xrruntime::logLine(va("OpenXR: GL interop: %s%s\n", text.cStr(), fd ? "" : " (GL_EXT_memory_object_fd not listed)"));
}
#else

bool OpenXrBackend::contextReady(const char* what)
{
    const HGLRC rc = wglGetCurrentContext();
    const HDC dc = wglGetCurrentDC();
    const HWND window = glDC ? WindowFromDC(glDC) : nullptr;
    const bool windowOk = window && IsWindow(window);
    xrruntime::logLine(va("%s OpenXR: %s: GL context %p, DC %p current on thread %lu (the session's: %p, %p; its window %p%s)\n",
        wallClock(), what, static_cast<void*>(rc), static_cast<void*>(dc), GetCurrentThreadId(), static_cast<void*>(glRC),
        static_cast<void*>(glDC), static_cast<void*>(window), windowOk ? "" : ", GONE"));
    if(!glRC || !glDC || !windowOk)
    {
        xrruntime::warn("OpenXR: %s not called: the GL context bound to the session (%p, DC %p) has no window any more\n",
            what, static_cast<void*>(glRC), static_cast<void*>(glDC));
        return false;
    }
    if(rc == glRC && dc == glDC)
    {
        return true;
    }
    const BOOL made = wglMakeCurrent(glDC, glRC);
    xrruntime::warn("OpenXR: %s: the game's GL context wasn't current on this thread (%p, DC %p instead of %p, %p): "
                    "made current again: %s\n",
        what, static_cast<void*>(rc), static_cast<void*>(dc), static_cast<void*>(glRC), static_cast<void*>(glDC),
        made ? "done" : va("FAILED (error %lu): the call not made", GetLastError()));
    return made != FALSE;
}

void OpenXrBackend::logGlInterop()
{
    constexpr const char* names[] = {"glCreateMemoryObjectsEXT", "glImportMemoryWin32HandleEXT", "glTextureStorageMem2DEXT",
        "glImportSemaphoreWin32HandleEXT", "wglDXOpenDeviceNV", "wglDXRegisterObjectNV"};
    za::String text;
    bool memoryObjects = true;
    for(size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
    {
        const intptr_t p = reinterpret_cast<intptr_t>(wglGetProcAddress(names[i]));
        const bool has = p != 0 && p != 1 && p != 2 && p != 3 && p != -1; // (some drivers' failure values)
        memoryObjects = memoryObjects && (i >= 3 || has);
        text += va("%s%s %s", text.empty() ? "" : ", ", names[i], has ? "yes" : "NO");
    }
    xrruntime::logLine(va("OpenXR: GL interop: %s\n", text.cStr()));
    if(vdxr && !memoryObjects)
    {
        xrruntime::warn("OpenXR: the GL driver lacks GL_EXT_memory_object_win32, which VDXR's OpenGL path needs\n");
    }
}
#endif

bool OpenXrBackend::createSession()
{
#ifdef _WIN32
    XrGraphicsBindingOpenGLWin32KHR binding{XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR};
    binding.hDC = wglGetCurrentDC();
    binding.hGLRC = wglGetCurrentContext();
    glDC = binding.hDC;
    glRC = binding.hGLRC;
    void* const boundContext = static_cast<void*>(glRC);
    logGlInterop();
    step("xrCreateSession (OpenGL: hDC %p, hGLRC %p)", static_cast<void*>(glDC), static_cast<void*>(glRC));
#else
    // Linux: SDL's context, GLX (XR_KHR_opengl_enable's Xlib binding: the context and its window's drawable) or
    // EGL (XR_MNDX_egl_enable: the display, config and context; SDL's Wayland driver, or X11 on EGL).
    const char* driver = SDL_GetCurrentVideoDriver();
    driver = driver ? driver : "none";
    const char* why = "";
    glctx = xrgl::current(&why);
    if(glctx.api == xrgl::Api::None)
    {
        xrruntime::warn("OpenXR: xrCreateSession not called: %s (SDL video driver %s)\n", why, driver);
        return false;
    }
    if(glctx.api == xrgl::Api::Egl && !eglExtension)
    {
        xrruntime::warn("OpenXR: xrCreateSession not called: SDL's GL context is EGL (SDL video driver %s) and %s "
                        "doesn't offer XR_MNDX_egl_enable (the EGL binding; it has the GLX one only): run with "
                        "SDL_VIDEODRIVER=x11 SDL_VIDEO_X11_FORCE_EGL=0 (X11 or XWayland: GLX), or with a runtime that "
                        "has it (Monado, WiVRn)\n",
            driver, runtime[0] ? runtime : "the runtime");
        glctx = xrgl::Binding{};
        return false;
    }
    if(why[0])
    {
        xrruntime::warn("OpenXR: %s\n", why);
    }
    XrGraphicsBindingOpenGLXlibKHR glxBinding{XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR};
    XrGraphicsBindingEGLMNDX eglBinding{XR_TYPE_GRAPHICS_BINDING_EGL_MNDX};
    const void* binding = nullptr;
    const char* bindingName = "";
    if(glctx.api == xrgl::Api::Glx)
    {
        glxBinding.xDisplay = static_cast<Display*>(glctx.display);
        glxBinding.visualid = glctx.visualId;
        glxBinding.glxFBConfig = static_cast<GLXFBConfig>(glctx.fbConfig);
        glxBinding.glxDrawable = glctx.drawable;
        glxBinding.glxContext = static_cast<GLXContext>(glctx.context);
        binding = &glxBinding;
        bindingName = "xrCreateSession (GLX binding, XR_KHR_opengl_enable)";
        logGlInterop();
        step("xrCreateSession (OpenGL, GLX binding XrGraphicsBindingOpenGLXlibKHR: SDL video driver %s; display %p, "
             "drawable 0x%lx, GLX context %p, FBConfig %p, visual 0x%x)",
            driver, glctx.display, glctx.drawable, glctx.context, glctx.fbConfig, glctx.visualId);
    }
    else
    {
        eglBinding.getProcAddress = reinterpret_cast<PFN_xrEglGetProcAddressMNDX>(xrgl::eglGetProcAddressFn());
        eglBinding.display = glctx.eglDisplay;
        eglBinding.config = glctx.eglConfig;
        eglBinding.context = glctx.context;
        binding = &eglBinding;
        bindingName = "xrCreateSession (EGL binding, XR_MNDX_egl_enable)";
        logGlInterop();
        step("xrCreateSession (OpenGL, EGL binding XrGraphicsBindingEGLMNDX: SDL video driver %s, %s; display %p, "
             "config %p, context %p, surface %p)",
            driver, xrgl::eglDescription(glctx), glctx.eglDisplay, glctx.eglConfig, glctx.context, glctx.eglDraw);
    }
    void* const boundContext = glctx.context;
#endif
    // (SDL's idea of the current context: the same one, or the engine's context is not what is current here.)
    if(static_cast<void*>(SDL_GL_GetCurrentContext()) != boundContext)
    {
        xrruntime::warn("OpenXR: the current GL context %p is not SDL's (%p)\n", boundContext,
            static_cast<void*>(SDL_GL_GetCurrentContext()));
    }
    if(!contextReady("xrCreateSession"))
    {
        return false;
    }

    XrSessionCreateInfo info{XR_TYPE_SESSION_CREATE_INFO};
#ifdef _WIN32
    info.next = &binding;
    const char* const bindingName = "xrCreateSession";
#else
    info.next = binding;
#endif
    info.systemId = systemId;
    const XrInstance inst = instance;
    XrSession made = XR_NULL_HANDLE;
    const XrResult created = guarded("xrCreateSession", [inst, &info, &made] { return xrCreateSession(inst, &info, &made); });
    session = made;
    if(runtimeFaulted || !check(created, bindingName))
    {
        return false;
    }

    getVisibilityMask = nullptr;
    if(visibilityMaskExtension)
    {
        xrGetInstanceProcAddr(instance, "xrGetVisibilityMaskKHR", reinterpret_cast<PFN_xrVoidFunction*>(&getVisibilityMask));
    }
    Con_Printf("OpenXR: hidden area mesh %s\n", getVisibilityMask ? "available (XR_KHR_visibility_mask)" : "not available");
    return true;
}

bool OpenXrBackend::createSpaces()
{
    // Stage (origin on the floor) if the runtime has it, local otherwise.
    uint32_t count = 0;
    xrEnumerateReferenceSpaces(session, 0, &count, nullptr);
    za::Vector<XrReferenceSpaceType> types(count);
    xrEnumerateReferenceSpaces(session, count, &count, types.data());

    XrReferenceSpaceCreateInfo info{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    info.poseInReferenceSpace.orientation.w = 1.f;
    info.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    for(XrReferenceSpaceType t : types)
    {
        if(t == XR_REFERENCE_SPACE_TYPE_STAGE)
        {
            info.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
        }
    }

    if(info.referenceSpaceType == XR_REFERENCE_SPACE_TYPE_LOCAL)
    {
        // Local space has its origin at the head; put the floor a standing height below.
        info.poseInReferenceSpace.position.y = -vr_height_calibration.value;
        Con_Printf("OpenXR: no stage space, using local space\n");
    }

    if(!check(xrCreateReferenceSpace(session, &info, &worldSpace), "xrCreateReferenceSpace"))
    {
        return false;
    }
    xrruntime::logLine(va("OpenXR: layers: a projection layer in %s space (2 views, a swapchain an eye, no depth layer), "
                          "and a quad layer for the panel while the world isn't drawn (menus, loading)\n",
        info.referenceSpaceType == XR_REFERENCE_SPACE_TYPE_STAGE ? "stage" : "local"));

    XrReferenceSpaceCreateInfo viewInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    viewInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
    viewInfo.poseInReferenceSpace.orientation.w = 1.f;
    return check(xrCreateReferenceSpace(session, &viewInfo, &viewSpace), "xrCreateReferenceSpace (view)");
}

void OpenXrBackend::beginSession()
{
    XrSessionBeginInfo beginInfo{XR_TYPE_SESSION_BEGIN_INFO};
    beginInfo.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    sessionRunning = check(xrBeginSession(session, &beginInfo), "xrBeginSession");
    if(sessionRunning)
    {
        // (Usually none yet: runtimes pick them at the first xrSyncActions, and say so with an event.)
        updateControllers();
        xrruntime::logLine(va("OpenXR: controller profiles at the session's start: left %s, right %s\n",
            profileName[0][0] ? profileName[0] : "none yet", profileName[1][0] ? profileName[1] : "none yet"));
        // What the buttons' keys do (a config's gamepad bindings over the VR ones look like the wrong profile).
        char keys[640];
        qvr::input::describeBindings(keys, sizeof(keys));
        xrruntime::logLine(va("OpenXR: the controller keys: %s\n", keys));
        nextProfileCheck = 0.0;
    }
}

bool OpenXrBackend::pollEvents()
{
    // vr_debug_buttons turned on: the profiles the runtime picked, now (they print when they change too).
    if(vr_debug_buttons.value != debugButtonsWas)
    {
        debugButtonsWas = vr_debug_buttons.value;
        if(debugButtonsWas && sessionRunning)
        {
            updateControllers();
        }
    }
    checkControllers();
    XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
    while(xrPollEvent(instance, &event) == XR_SUCCESS)
    {
        switch(event.type)
        {
            case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
                // The runtime is going away (SteamVR shut down, updated...): flat (vr_restart starts VR again).
                noteState("INSTANCE_LOSS_PENDING");
                endReason = SessionEnd::Lost;
                xrruntime::logLine(va("%s instance loss pending: the runtime is going away (states: %s)\n", wallClock(),
                    stateTrailText));
                return false;
            case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED: updateControllers(); break;
            case XR_TYPE_EVENT_DATA_VISIBILITY_MASK_CHANGED_KHR:
            {
                const auto& changed = reinterpret_cast<const XrEventDataVisibilityMaskChangedKHR&>(event);
                if(changed.viewIndex < 2)
                {
                    hiddenStale[changed.viewIndex] = true;
                }
                break;
            }
            case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
            {
                const auto& changed = reinterpret_cast<const XrEventDataSessionStateChanged&>(event);
                sessionState = changed.state;
                noteState(stateName(sessionState));
                xrstall::note(va("%s session %s%s\n", wallClock(), stateName(sessionState), // (Tracy too)
                    sessionState == XR_SESSION_STATE_VISIBLE
                        ? (vr_xr_unfocused.value != 0.f ? " (no input focus, e.g. the runtime's menu: the last frame "
                                                          "shown again, not rendered; vr_xr_unfocused 1)"
                                                        : " (no input focus, e.g. the runtime's menu; vr_xr_unfocused 0: "
                                                          "rendered)")
                        : ""));
                if(sessionState == XR_SESSION_STATE_READY)
                {
                    beginSession();
                }
                else if(sessionState == XR_SESSION_STATE_STOPPING)
                {
                    // Ended as the runtime asks; what comes next says why: READY (it starts again: the headset
                    // back on, the runtime restarted), EXITING (the player quit the game from the runtime's menu),
                    // LOSS_PENDING (the headset or runtime gone).
                    if(sessionRunning)
                    {
                        check(xrEndSession(session), "xrEndSession");
                    }
                    sessionRunning = false;
                    xrruntime::logLine(va("%s session ended (xrEndSession): waiting for the runtime: READY starts it "
                                          "again, EXITING quits the game (vr_xr_exit_quits)\n",
                        wallClock()));
                }
                else if(sessionState == XR_SESSION_STATE_EXITING)
                {
                    endReason = SessionEnd::Exit;
                    xrruntime::logLine(va("%s the runtime asks the game to exit (states: %s)\n", wallClock(),
                        stateTrailText));
                    return false;
                }
                else if(sessionState == XR_SESSION_STATE_LOSS_PENDING)
                {
                    endReason = SessionEnd::Lost;
                    xrruntime::logLine(va("%s session loss pending: the headset or the runtime is going away "
                                          "(states: %s)\n",
                        wallClock(), stateTrailText));
                    return false;
                }
                break;
            }
            default: break;
        }
        event = XrEventDataBuffer{XR_TYPE_EVENT_DATA_BUFFER};
    }

    return true;
}

} // namespace qvr::oxr

namespace qvr
{

za::UniquePtr<Backend> makeOpenXrBackend()
{
    return za::makeUnique<oxr::OpenXrBackend>();
}

} // namespace qvr

#else

namespace qvr
{

za::UniquePtr<Backend> makeOpenXrBackend()
{
    return nullptr;
}

} // namespace qvr

#endif
