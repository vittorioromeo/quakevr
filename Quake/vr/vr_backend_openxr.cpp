// vr_backend_openxr.cpp -- OpenXR runtime backend (OpenGL, Windows).
//
// Session lifecycle, stage reference space, the controllers' actions (poses, buttons, sticks,
// haptics), one swapchain per eye and one for the 2D panel, and the wait/begin/locate/end frame
// loop. The graphics binding (XR_KHR_opengl_enable, WGL, GL swapchain formats and images) is
// the renderer-bound part: a Vulkan engine binds with XR_KHR_vulkan_enable2 instead
// (docs/vr-port/PORTING.md).

#include "vr_backend.hpp"
#include "vr_cvars.hpp"

#ifdef QVR_HAVE_OPENXR

#include "vr_engine.hpp"
#include "vr_input.hpp"
#include "vr_profile.hpp"
#include "vr_tracy.h"
#include "vr_xr_runtime.hpp"
#include "vr_synthesis_math.hpp"
#include "vr_synthesis_submission.hpp"

#include "Zancle/Base/InitializerList.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/Vocabulary/UniquePtr.hpp"

#ifdef _WIN32
#include <windows.h>
#include <unknwn.h> // IUnknown, which openxr_platform.h needs and lean Windows headers omit
#define XR_USE_PLATFORM_WIN32
#else
#include <X11/Xlib.h>
#include <GL/glx.h>
#include <sys/time.h>
#include <time.h>
#define XR_USE_PLATFORM_XLIB
#endif
#define XR_USE_GRAPHICS_API_OPENGL
#if defined(QVR_HAVE_GL_VULKAN_BRIDGE) && defined(__linux__)
#define QVR_OPENXR_INTEROP
#define XR_USE_GRAPHICS_API_VULKAN
#include "vr_openxr_vulkan.hpp"
#endif
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include "vr_openxr_synthesis_compat.hpp"

#ifndef GL_SRGB8_ALPHA8
#define GL_SRGB8_ALPHA8 0x8C43
#endif

#include <stdarg.h>
#include <string.h>
#include <cmath>

namespace qvr
{
namespace
{
#ifdef QVR_OPENXR_INTEROP
// A discarded backend must not permit another runtime/device while failed
// native work still owns retained session/device/image resources.
bool interopParentsRetained = false;
#endif

// A crash inside the runtime, caught where the game calls it (vr_xr_guard 1): 1.0.0 died in VDXR 1.1.0's
// xrEnumerateSwapchainImages, a null read (virtualdesktop-openxr.dll+0x9a668): its OpenGL path takes the swapchain's
// GL images as made once its D3D11 images are, and reads the first of none. The D3D11 images are fetched before the
// image structs' type is checked, so an earlier call that failed that check (an API layer's, enumerating the game's
// swapchain as D3D11 or Vulkan ones) leaves the swapchain in that state (ROUND22.md, "VDXR swapchain crash").
#ifdef _WIN32
struct Fault
{
    DWORD code{0};
    void* address{nullptr};
    ULONG_PTR access{0}; // 0 read, 1 write, 8 execute
    ULONG_PTR target{0}; // the address read or written
};

int faultFilter(const EXCEPTION_POINTERS* ep, Fault& fault)
{
    const DWORD code = ep->ExceptionRecord->ExceptionCode;
    // A crash's exceptions only (a C++ exception, a debugger's breakpoint: the runtime's own or not ours).
    if(code != EXCEPTION_ACCESS_VIOLATION && code != EXCEPTION_IN_PAGE_ERROR && code != EXCEPTION_ILLEGAL_INSTRUCTION &&
        code != EXCEPTION_PRIV_INSTRUCTION && code != EXCEPTION_INT_DIVIDE_BY_ZERO && code != EXCEPTION_ARRAY_BOUNDS_EXCEEDED)
    {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    fault.code = code;
    fault.address = ep->ExceptionRecord->ExceptionAddress;
    if(ep->ExceptionRecord->NumberParameters >= 2)
    {
        fault.access = ep->ExceptionRecord->ExceptionInformation[0];
        fault.target = ep->ExceptionRecord->ExceptionInformation[1];
    }
    return EXCEPTION_EXECUTE_HANDLER;
}
#else
struct Fault // (no structured exceptions here: guardedCall just calls)
{
    unsigned long code{0};
    void* address{nullptr};
};
#endif

// call() under a structured exception handler (no object to unwind in here); fault.code nonzero when it crashed.
template <typename Call>
XrResult guardedCall(Call& call, Fault& fault)
{
#ifdef _MSC_VER
    __try
    {
        return call();
    }
    __except(faultFilter(GetExceptionInformation(), fault))
    {
        return XR_ERROR_RUNTIME_FAILURE;
    }
#else
    (void)fault;
    return call();
#endif
}

// "an access violation reading 0x0 in virtualdesktop-openxr.dll+0x9a668"
#ifndef _WIN32
void describeFault(const Fault&, char* out, size_t size)
{
    q_strlcpy(out, "a crash (not caught on this platform)", size);
}
#else
void describeFault(const Fault& fault, char* out, size_t size)
{
    char module[MAX_PATH] = "an unknown module";
    uintptr_t offset = reinterpret_cast<uintptr_t>(fault.address);
    HMODULE m = nullptr;
    if(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
           static_cast<LPCSTR>(fault.address), &m) &&
        m)
    {
        char path[MAX_PATH];
        if(GetModuleFileNameA(m, path, sizeof(path)))
        {
            const char* slash = strrchr(path, '\\');
            q_strlcpy(module, slash ? slash + 1 : path, sizeof(module));
        }
        offset -= reinterpret_cast<uintptr_t>(m);
    }
    if(fault.code == EXCEPTION_ACCESS_VIOLATION || fault.code == EXCEPTION_IN_PAGE_ERROR)
    {
        q_snprintf(out, size, "an access violation %s 0x%llx in %s+0x%llx",
            fault.access == 0 ? "reading" : fault.access == 1 ? "writing" : "executing",
            static_cast<unsigned long long>(fault.target), module, static_cast<unsigned long long>(offset));
    }
    else
    {
        q_snprintf(out, size, "exception 0x%08lx in %s+0x%llx", static_cast<unsigned long>(fault.code), module,
            static_cast<unsigned long long>(offset));
    }
}
#endif

// Diagnostics mode's messenger (any thread): the message to openxr_debug.log (vr_diagnostics.cpp).
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

class OpenXrBackend final : public Backend
{
public:
    ~OpenXrBackend() override
    {
        stop();
    }

    [[nodiscard]] const char* name() const override
    {
        return "openxr";
    }

    [[nodiscard]] const char* runtimeName() const override
    {
        return runtime[0] ? runtime : "openxr";
    }

    // The runtime vr_xr_runtime chooses (vr_xr_runtime.hpp); with Auto, while one fails (no headset, not running), the
    // next: the loader unloads a runtime with its last instance and reads XR_RUNTIME_JSON again at the next.
    [[nodiscard]] bool start() override
    {
#ifdef QVR_OPENXR_INTEROP
        if(interopParentsRetained)
        {
            Con_Warning("OpenXR: prior interop teardown could not drain; new runtime start blocked until process restart\n");
            return false;
        }
        interopMode = vr_xr_vulkan_interop.value > 0.f; // latched before enabled graphics extension selection
#endif
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
            }
            stop(); // (all of it: the next runtime starts from nothing)
#ifdef QVR_OPENXR_INTEROP
            if(interopTeardownBlocked) return false; // native parents/resources stay alive; no fallback/reset
#endif
            resetRuntimeState();
        }
        xrruntime::setOutcome(plan, -1);
        return false;
    }

    [[nodiscard]] bool startRuntime()
    {
        if(!(createInstance() && createSystem() && createSession() && createSpaces() &&
               createActions() && createSwapchains()))
        {
            return false;
        }

        // The runtime paces frames (xrWaitFrame): the desktop mirror must not wait for vsync.
        SDL_GL_SetSwapInterval(0);
        swapIntervalChanged = true;
        return true;
    }

    void stop() override
    {
#ifdef QVR_OPENXR_INTEROP
        if(interopMode)
        {
            traceInteropTeardown("begin");
            frameInvalid = true;
            if(!settleInteropFrame()) return; // close/release XR ownership before bridge drain
            traceInteropTeardown("XR frame/images settled");
            for(Swapchain* sc : {&swapchains[0], &swapchains[1], &panel,
                    &synthesisMotion[0], &synthesisMotion[1], &synthesisDepth[0], &synthesisDepth[1]})
                restoreInteropSrgb(*sc);
            bool drained = true;
            // Imports and app Vulkan children must drain before native parents.
            for(gl_vulkan::Bridge* bridge : {&eyeBridges[0], &eyeBridges[1], &panelBridge,
                    &motionBridges[0], &motionBridges[1], &depthBridges[0], &depthBridges[1]})
                if(!bridge->shutdown())
                {
                    xrruntime::warn("OpenXR interop: bridge drain failed: %s\n", bridge->error());
                    drained = false;
                }
            if(!drained)
            {
                blockInteropTeardown("bridge work/imports could not drain");
                return;
            }
            traceInteropTeardown("all staging bridges drained/destroyed");
        }
#endif
        if(instance != XR_NULL_HANDLE)
        {
            xrruntime::keepGraphicsModules();
            xrruntime::logLine(va("stopping %s\n", runtime[0] ? runtime : "the runtime"));
        }
        if(frameBegun)
        {
            endFrame(false);
        }

        for(Swapchain* sc : {&swapchains[0], &swapchains[1], &panel,
                 &synthesisMotion[0], &synthesisMotion[1], &synthesisDepth[0], &synthesisDepth[1]})
        {
            if(sc->handle != XR_NULL_HANDLE)
            {
#ifdef QVR_OPENXR_INTEROP
                if(interopMode) traceInteropTeardown(va("destroy swapchain %p begin", reinterpret_cast<void*>(sc->handle)));
                if(interopMode && xrDestroySwapchain(sc->handle) != XR_SUCCESS)
                {
                    blockInteropTeardown("xrDestroySwapchain failed");
                    return;
                }
                if(interopMode) traceInteropTeardown("destroy swapchain success");
                if(!interopMode)
#endif
                {
                    const XrSwapchain handle = sc->handle;
                    guarded("xrDestroySwapchain", [handle] { return xrDestroySwapchain(handle); });
                }
                *sc = Swapchain{};
            }
        }
        panelPending = panelShown = false;
        lastProjectionTime = 0;
        synthesisAvailable = false;
        fatalSwapchainError = false;
        for(int eye = 0; eye < 2; ++eye)
        {
            synthesisMotionAcquired[eye] = synthesisDepthAcquired[eye] = synthesisReady[eye] = false;
        }
        haveLastViews = holding = false;
        timing = Timing{};
        for(int eye = 0; eye < 2; eye++)
        {
            hidden[eye] = HiddenArea{};
            hiddenStale[eye] = true;
        }
        getVisibilityMask = nullptr;

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
#ifdef QVR_OPENXR_INTEROP
            if(interopMode) traceInteropTeardown("destroy session begin");
            if(interopMode && xrDestroySession(session) != XR_SUCCESS)
            {
                blockInteropTeardown("xrDestroySession failed");
                return;
            }
            if(interopMode) traceInteropTeardown("destroy session success");
            if(!interopMode)
#endif
            {
                const XrSession handle = session;
                guarded("xrDestroySession", [handle] { return xrDestroySession(handle); });
            }
            session = XR_NULL_HANDLE;
        }

#ifdef QVR_OPENXR_INTEROP
        if(interopMode)
        {
            traceInteropTeardown("terminal bound-queue fence begin");
            if(!vulkanBootstrap.drainAfterSessionDestroy(session == XR_NULL_HANDLE))
            {
                blockInteropTeardown(vulkanBootstrap.error());
                return;
            }
            traceInteropTeardown("terminal bound-queue fence complete; destroy Vulkan device begin");
            if(!vulkanBootstrap.shutdown(true))
            {
                blockInteropTeardown(vulkanBootstrap.error());
                return;
            }
            traceInteropTeardown("Vulkan device/instance destroyed");
        }
#endif
        if(instance != XR_NULL_HANDLE)
        {
#ifdef QVR_OPENXR_INTEROP
            if(interopMode && xrDestroyInstance(instance) != XR_SUCCESS)
            {
                blockInteropTeardown("xrDestroyInstance failed");
                return;
            }
            if(interopMode) traceInteropTeardown("XR instance destroyed");
            if(!interopMode)
#endif
            {
                const XrInstance handle = instance;
                guarded("xrDestroyInstance", [handle] { return xrDestroyInstance(handle); });
            }
            instance = XR_NULL_HANDLE;
            q_snprintf(crashBase, sizeof(crashBase), "OpenXR stopped (%s)", runtime[0] ? runtime : "no runtime");
            VR_SetCrashVr(crashBase);
        }
#ifdef QVR_OPENXR_INTEROP
        if(interopMode) interopTeardownBlocked = interopParentsRetained = false;
#endif

        if(sessionRunning || swapIntervalChanged)
        {
            SDL_GL_SetSwapInterval(Cvar_VariableValue("vid_vsync") != 0.f ? 1 : 0);
        }
        sessionRunning = false;
    }

    [[nodiscard]] bool beginFrame(TrackingState& tracking, FrameState& frame) override
    {
        frame.shouldRender = false;
        frame.hold = false;

        if(frameBegun)
        {
            endFrame(false); // the previous frame was not rendered (e.g. while loading)
        }
        synthesisReady[0] = synthesisReady[1] = false;
        synthesisContent.begin();
        synthesisExportSeconds[0] = synthesisExportSeconds[1] = 0.0;
        batchExportSeconds = 0.0;
        synthesisFrameRejected = false;
        submitSucceeded = false;
        frameInvalid = false;
#ifdef QVR_OPENXR_INTEROP
        interopEyeReleased[0] = interopEyeReleased[1] = false;
        interopEyeQueued[0] = interopEyeQueued[1] = false;
        synthesisMotionReleased[0] = synthesisMotionReleased[1] = false;
        synthesisDepthReleased[0] = synthesisDepthReleased[1] = false;
        synthesisExportPending[0] = synthesisExportPending[1] = false;
        synthesisPendingValid[0] = synthesisPendingValid[1] = false;
#endif
        if(fatalSwapchainError)
        {
            xrruntime::warn("OpenXR: swapchain image lifecycle failed; stopping the session before another acquire\n");
            return false;
        }

        double t0 = Sys_DoubleTime();
        const bool polled = pollEvents();
        timeCall(CallPoll, t0);
        if(!polled)
        {
            return false;
        }

        if(!sessionRunning && sessionState == XR_SESSION_STATE_READY)
        {
            beginSession(); // the first attempt (on the READY event) failed: retry
        }
        if(!sessionRunning)
        {
            return true;
        }

        for(int eye = 0; eye < 2; eye++)
        {
            if(hiddenStale[eye])
            {
                fetchHiddenArea(eye);
            }
        }

        XrFrameWaitInfo waitInfo{XR_TYPE_FRAME_WAIT_INFO};
        frameState = XrFrameState{XR_TYPE_FRAME_STATE};
        profile::begin("xrWaitFrame", false); // the runtime's pacing alone (vr_memstats_log)
        t0 = Sys_DoubleTime();
        const XrResult waited = xrWaitFrame(session, &waitInfo, &frameState);
        timeCall(CallWait, t0);
        profile::end();
        if(!check(waited, "xrWaitFrame"))
        {
            return true;
        }
        profile::noteDisplayPeriod(static_cast<double>(frameState.predictedDisplayPeriod) * 1e-6);
        noteWaited();

        XrFrameBeginInfo beginInfo{XR_TYPE_FRAME_BEGIN_INFO};
        profile::begin("xrBeginFrame", false);
        t0 = Sys_DoubleTime();
        const XrResult begun = xrBeginFrame(session, &beginInfo);
        timeCall(CallBegin, t0);
        profile::end();
        if(!check(begun, "xrBeginFrame"))
        {
            return true;
        }
        frameBegun = true;
        QVR_PROFILE("tracking"); // the actions' sync, the views, the controllers

        t0 = Sys_DoubleTime();
        const XrActiveActionSet active{actionSet, XR_NULL_PATH};
        XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
        syncInfo.countActiveActionSets = 1;
        syncInfo.activeActionSets = &active;
        xrSyncActions(session, &syncInfo);
        timeCall(CallSync, t0);

        const XrTime time = frameState.predictedDisplayTime;
        tracking.time = static_cast<double>(time) * 1e-9;

        XrViewLocateInfo locateInfo{XR_TYPE_VIEW_LOCATE_INFO};
        locateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        locateInfo.displayTime = time;
        locateInfo.space = worldSpace;

        XrViewState viewState{XR_TYPE_VIEW_STATE};
        uint32_t viewCount = 0;
        for(XrView& v : views)
        {
            v = XrView{XR_TYPE_VIEW};
        }
        const bool viewsOk =
            check(xrLocateViews(session, &locateInfo, &viewState, 2, &viewCount, views), "xrLocateViews") &&
            viewCount == 2 &&
            (viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT);

        for(int eye = 0; eye < 2; eye++)
        {
            frame.eyes[eye].pose = toPose(views[eye].pose, viewsOk);
            frame.eyes[eye].fov = {views[eye].fov.angleLeft, views[eye].fov.angleRight,
                views[eye].fov.angleUp, views[eye].fov.angleDown};
        }

        readInput(tracking.input);

        tracking.head = locate(viewSpace, time);
        tracking.head.valid = tracking.head.valid && userPresent();
        if(tracking.head.valid)
        {
            lastHead = tracking.head;
        }
        for(int h = 0; h < HAND_COUNT; h++)
        {
            const int side = handSide(h);
            tracking.hands[h] = locate(handSpaces[side], time);
            tracking.hands[h].valid = tracking.hands[h].valid && userPresent();
            tracking.gripInHand[h] = GripInRaw{};
            if(vr_controller_legacy_pose.value)
            {
                tracking.hands[h] = toLegacyPose(tracking.hands[h], side);
                if(tracking.hands[h].valid && controller[side] != Controller::Other)
                {
                    tracking.gripInHand[h] = legacyGripInRaw(controller[side] == Controller::Touch, side);
                }
            }
        }

        if(!userPresent())
        {
            tracking.input = InputState{};
        }
        frame.shouldRender = viewsOk && frameState.shouldRender;
        // The runtime's menu has the focus (SteamVR's dashboard...): its last frame again, nothing rendered.
        holding = frameState.shouldRender && haveLastViews &&
                  (sessionState == XR_SESSION_STATE_VISIBLE || !userPresent()) &&
                  vr_xr_unfocused.value != 0.f;
        frame.hold = holding;
        if(!frameState.shouldRender)
        {
            timing.noRender++;
        }
        if(COM_CheckParm("-qvr-frame-diagnostics") && Sys_DoubleTime() - diagnosticAt >= 1.0)
        {
            diagnosticAt = Sys_DoubleTime();
            xrruntime::logLine(va("FRAME_DIAG state=%s present=%d shouldRender=%d hold=%d views=%u flags=0x%llx "
                                 "head=%d head_xyz=%.3f,%.3f,%.3f head_q=%.3f,%.3f,%.3f,%.3f\n",
                stateName(sessionState), userPresent(), frame.shouldRender, frame.hold, viewCount,
                static_cast<unsigned long long>(viewState.viewStateFlags), tracking.head.valid,
                tracking.head.position.x, tracking.head.position.y, tracking.head.position.z,
                tracking.head.orientation.w, tracking.head.orientation.x,
                tracking.head.orientation.y, tracking.head.orientation.z));
            for(int h = 0; h < HAND_COUNT; ++h)
            {
                const int side = handSide(h);
                XrActionStateGetInfo get{XR_TYPE_ACTION_STATE_GET_INFO};
                get.action = gripPose;
                get.subactionPath = handPaths[side];
                XrActionStatePose poseState{XR_TYPE_ACTION_STATE_POSE};
                const XrResult poseResult = xrGetActionStatePose(session, &get, &poseState);
                XrInteractionProfileState profileState{XR_TYPE_INTERACTION_PROFILE_STATE};
                xrGetCurrentInteractionProfile(session, handPaths[side], &profileState);
                char profileName[XR_MAX_PATH_LENGTH]{};
                uint32_t length = 0;
                if(profileState.interactionProfile != XR_NULL_PATH)
                    xrPathToString(instance, profileState.interactionProfile,
                        sizeof(profileName), &length, profileName);
                const HandInput& input = tracking.input.hands[h];
                xrruntime::logLine(va("FRAME_HAND side=%s profile=%s pose_action_active=%d pose_result=%d valid=%d "
                                     "xyz=%.3f,%.3f,%.3f trigger=%d/%.3f grip=%d/%.3f stick=%.3f,%.3f\n",
                    side == 0 ? "left" : "right", profileName[0] ? profileName : "none",
                    poseState.isActive, static_cast<int>(poseResult), tracking.hands[h].valid,
                    tracking.hands[h].position.x, tracking.hands[h].position.y, tracking.hands[h].position.z,
                    input.trigger, input.triggerValue, input.grip, input.gripValue, input.stick.x, input.stick.y));
            }
        }
        return true;
    }

    [[nodiscard]] bool frameActive() const override
    {
        return frameBegun && sessionRunning;
    }

    void eyeResolution(int& width, int& height) const override
    {
        width = swapchains[0].width;
        height = swapchains[0].height;
    }

    [[nodiscard]] EyeSizes eyeSizes() const override
    {
        EyeSizes s;
        s.recommendedWidth = static_cast<int>(configViews[0].recommendedImageRectWidth);
        s.recommendedHeight = static_cast<int>(configViews[0].recommendedImageRectHeight);
        s.maxWidth = static_cast<int>(configViews[0].maxImageRectWidth);
        s.maxHeight = static_cast<int>(configViews[0].maxImageRectHeight);
        s.width = swapchains[0].width;
        s.height = swapchains[0].height;
        return s;
    }

    [[nodiscard]] const HiddenArea* hiddenArea(int eye) const override
    {
        return getVisibilityMask ? &hidden[eye] : nullptr;
    }

    [[nodiscard]] bool runtimeMenuOpen() const override
    {
        return sessionRunning && sessionState == XR_SESSION_STATE_VISIBLE;
    }

    [[nodiscard]] bool userPresent() const override
    {
        return !supportsUserPresence || (userPresenceKnown && isUserPresent);
    }

    [[nodiscard]] const char* systemName() const override
    {
        return system;
    }

    [[nodiscard]] GraphicsImage acquireEyeImage(int eye) override
    {
#ifdef QVR_OPENXR_INTEROP
        if(interopMode)
        {
            if(eye < 0 || eye >= 2 || !frameBegun) return {};
            const unsigned texture = acquireInteropImage(swapchains[eye], eyeBridges[eye]);
            captureTexture[eye] = texture;
            const Swapchain& sc = swapchains[eye];
            return texture ? GraphicsImage{GraphicsApi::OpenGL, texture, 0, GL_RGBA8, sc.width, sc.height} : GraphicsImage{};
        }
#endif
        const unsigned texture = acquireImage(swapchains[eye]);
        captureTexture[eye] = texture;
        const Swapchain& sc = swapchains[eye];
        return texture ? GraphicsImage{GraphicsApi::OpenGL, texture, 0, sc.format, sc.width, sc.height} : GraphicsImage{};
    }

    [[nodiscard]] bool releaseEyeImage(int eye) override
    {
#ifdef QVR_OPENXR_INTEROP
        if(interopMode && (eye < 0 || eye >= 2)) return false;
#endif
        // Opt-in native-port diagnostic: capture the actual XR color image, not
        // the desktop mirror. Preserve the renderer's current texture binding.
        if(COM_CheckParm("-qvr-frame-capture") && sessionState == XR_SESSION_STATE_FOCUSED &&
            ++captureFrames[eye] == 120 && captureTexture[eye])
        {
            const Swapchain& sc = swapchains[eye];
            za::Vector<unsigned char> pixels(static_cast<za::SizeT>(sc.width) * sc.height * 4);
            GLint oldTexture = 0;
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldTexture);
            glBindTexture(GL_TEXTURE_2D, captureTexture[eye]);
            glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(oldTexture));
            const GLenum error = glGetError();
            const char* filename = va("%s/native-eye-%d.ppm", com_gamedir, eye);
            FILE* file = fopen(filename, "wb");
            if(file)
            {
                fprintf(file, "P6\n%d %d\n255\n", sc.width, sc.height);
                for(int y = sc.height - 1; y >= 0; --y)
                    for(int x = 0; x < sc.width; ++x)
                        fwrite(pixels.data() + (static_cast<za::SizeT>(y) * sc.width + x) * 4, 1, 3, file);
                fclose(file);
                Con_Printf("OpenXR native capture: eye %d, %dx%d, GL error %x, %s\n", eye, sc.width, sc.height, error, filename);
            }
        }
        const double t0 = Sys_DoubleTime();
#ifdef QVR_OPENXR_INTEROP
        bool released;
        if(interopMode)
        {
            const Swapchain& sc = swapchains[eye];
            released = sc.acquired && sc.waited && eyeBridges[eye].healthy();
            interopEyeQueued[eye] = released;
            interopEyeReleased[eye] = false;
            restoreInteropSrgb(swapchains[eye]);
        }
        else
        {
            released = releaseImage(swapchains[eye]);
            timeCall(CallRelease, t0);
        }
#else
        const bool released = releaseImage(swapchains[eye]);
        timeCall(CallRelease, t0);
#endif
        if(!released) frameInvalid = true;
        else haveLastViews = false; // A new release invalidates cached held-frame image/pose pairs.
        return released;
    }

    [[nodiscard]] bool frameSynthesisAvailable() const override
    {
        return synthesisAvailable && vr_xr_frame_synthesis.value > 0.f;
    }

    [[nodiscard]] FrameSynthesisImages synthesisImages(int eye, unsigned motion, unsigned depth) const
    {
        FrameSynthesisImages images;
        images.contentFrameId = motion && depth ? synthesisContent.frameId() : 0;
        const Swapchain& motionChain = synthesisMotion[eye];
        const Swapchain& depthChain = synthesisDepth[eye];
        images.motion = {GraphicsApi::OpenGL, motion, 0, motionChain.format, motionChain.width, motionChain.height};
        images.depth = {GraphicsApi::OpenGL, depth, 0, depthChain.format, depthChain.width, depthChain.height};
        images.srgb8Motion = synthesisSrgbMotion;
        if(images.srgb8Motion)
        {
            synthesis::Srgb8MotionEncoding encoding;
            const float range = CLAMP(0.0625f, vr_xr_frame_synthesis_motion_range.value, 0.5f);
            if(synthesis::srgb8MotionEncoding(range, encoding))
            {
                images.motionScaleX = images.motionScaleY = encoding.scale;
                images.motionOffsetX = images.motionOffsetY = encoding.offset;
            }
        }
        return images;
    }

    [[nodiscard]] bool lastFrameSubmitted() const override { return submitSucceeded; }
    [[nodiscard]] double previousProjectionTime() const override
    {
        return lastProjectionTime ? static_cast<double>(lastProjectionTime) * 1e-9 : -1.0;
    }

    [[nodiscard]] FrameSynthesisImages frameSynthesisConfiguration(int eye) const override
    {
        if(!frameSynthesisAvailable() || eye < 0 || eye > 1) return {};
        return synthesisImages(eye, 0, 0);
    }

    [[nodiscard]] FrameSynthesisImages acquireFrameSynthesisImages(int eye) override
    {
        if(!frameSynthesisAvailable() || !frameBegun || eye < 0 || eye > 1 ||
            synthesisMotionAcquired[eye] || synthesisDepthAcquired[eye])
        {
            return {};
        }
#ifdef QVR_OPENXR_INTEROP
        const unsigned motion = interopMode ? acquireInteropImage(synthesisMotion[eye], motionBridges[eye]) :
            acquireSynthesisImage(synthesisMotion[eye]);
#else
        const unsigned motion = acquireSynthesisImage(synthesisMotion[eye]);
#endif
        synthesisMotionAcquired[eye] = motion != 0;
#ifdef QVR_OPENXR_INTEROP
        const unsigned depth = !motion ? 0 : interopMode ? acquireInteropImage(synthesisDepth[eye], depthBridges[eye]) :
            acquireSynthesisImage(synthesisDepth[eye]);
#else
        const unsigned depth = motion ? acquireSynthesisImage(synthesisDepth[eye]) : 0;
#endif
        synthesisDepthAcquired[eye] = depth != 0;
        if(!motion || !depth)
        {
            releaseFrameSynthesisImages(eye, false, {});
            return {};
        }
        if(!synthesisContent.acquire(eye, synthesisMotion[eye].acquiredIndex, synthesisDepth[eye].acquiredIndex))
        {
            releaseFrameSynthesisImages(eye, false, {});
            return {};
        }
        return synthesisImages(eye, motion, depth);
    }

    void releaseFrameSynthesisImages(int eye, bool valid, const FrameSynthesisMetadata& metadata) override
    {
        if(eye < 0 || eye > 1) return;
#ifdef QVR_OPENXR_INTEROP
        if(interopMode)
        {
            synthesisMetadata[eye] = metadata;
            synthesisPendingValid[eye] = valid;
            synthesisExportPending[eye] = true;
            return;
        }
#endif
        const double exportBegan = Sys_DoubleTime();
        bool motionReleased = false, depthReleased = false;
        // Even an invalid producer export completes the staging semaphore cycle,
        // then releases the XR images without attaching synthesis to the frame.
        {
            if(synthesisMotionAcquired[eye]) motionReleased = releaseImage(synthesisMotion[eye]);
            if(synthesisDepthAcquired[eye]) depthReleased = releaseImage(synthesisDepth[eye]);
        }
        const bool released = motionReleased && depthReleased;
        synthesisMotionAcquired[eye] = synthesisDepthAcquired[eye] = false;
        if(!released) synthesisAvailable = false;
        synthesisMetadata[eye] = metadata;
        const auto& pose = metadata.appSpaceDelta;
        const XrPosef delta{{pose.orientation.x, pose.orientation.y, pose.orientation.z, pose.orientation.w},
            {pose.position.x, pose.position.y, pose.position.z}};
        synthesisReady[eye] = valid && released && pose.valid && xr::validSynthesisMetadata(
            metadata.nearZ, metadata.farZ, delta, {metadata.motionScaleX, metadata.motionScaleY},
            {metadata.motionOffsetX, metadata.motionOffsetY});
        synthesisReady[eye] = synthesisContent.complete(eye, metadata.contentFrameId,
            synthesisReady[eye], motionReleased, depthReleased);
        synthesisExportSeconds[eye] = Sys_DoubleTime() - exportBegan;
    }

    [[nodiscard]] bool completeFrameExports(bool stereoRendered) override
    {
#ifdef QVR_OPENXR_INTEROP
        if(!interopMode) return true;
        QVR_PROFILE("synthesis transport");
        const double exportBegan = Sys_DoubleTime();
        struct Export
        {
            Swapchain* swapchain;
            gl_vulkan::Bridge* bridge;
            bool valid;
        } exports[6]{};
        gl_vulkan::Bridge* bridges[6]{};
        std::size_t count = 0;
        const auto add = [&](Swapchain& sc, gl_vulkan::Bridge& bridge, bool valid)
        {
            if(!sc.acquired) return;
            if(count < 6)
            {
                exports[count] = {&sc, &bridge, valid};
                bridges[count] = &bridge;
                ++count;
            }
        };
        for(int eye = 0; eye < 2; ++eye)
        {
            add(swapchains[eye], eyeBridges[eye], stereoRendered && interopEyeQueued[eye]);
            add(synthesisMotion[eye], motionBridges[eye], stereoRendered && synthesisExportPending[eye] &&
                synthesisPendingValid[eye] && synthesisMotionAcquired[eye] && synthesisDepthAcquired[eye]);
            add(synthesisDepth[eye], depthBridges[eye], stereoRendered && synthesisExportPending[eye] &&
                synthesisPendingValid[eye] && synthesisMotionAcquired[eye] && synthesisDepthAcquired[eye]);
        }
        const bool deferWait = vr_xr_vulkan_defer_wait.value > 0.f;
        const bool copied = gl_vulkan::completeQueuedExports(bridges, count, [&](gl_vulkan::Bridge& bridge)
        {
            QVR_PROFILE("synthesis enqueue");
            for(std::size_t i = 0; i < count; ++i)
                if(exports[i].bridge == &bridge)
                    return enqueueInteropImage(*exports[i].swapchain, bridge, exports[i].valid);
            return false;
        }, deferWait ? gl_vulkan::ExportCompletion::DeferUntilReuse : gl_vulkan::ExportCompletion::Wait);
        if(!copied)
        {
            invalidateInteropFrame("batched frame export/fence failed; acquired native images retained");
            return false;
        }

        bool released = true;
        for(std::size_t i = 0; i < count; ++i)
        {
            QVR_PROFILE("synthesis release");
            const bool oneReleased = releaseCompletedInteropImage(*exports[i].swapchain, *exports[i].bridge, deferWait);
            released = oneReleased && released;
            for(int eye = 0; eye < 2; ++eye)
            {
                if(exports[i].swapchain == &swapchains[eye]) interopEyeReleased[eye] = oneReleased;
                if(exports[i].swapchain == &synthesisMotion[eye])
                {
                    synthesisMotionReleased[eye] = oneReleased;
                    if(oneReleased) synthesisMotionAcquired[eye] = false;
                }
                if(exports[i].swapchain == &synthesisDepth[eye])
                {
                    synthesisDepthReleased[eye] = oneReleased;
                    if(oneReleased) synthesisDepthAcquired[eye] = false;
                }
            }
        }
        for(int eye = 0; eye < 2; ++eye)
        {
            if(!synthesisExportPending[eye]) continue;
            const bool motionReleased = synthesisMotionReleased[eye];
            const bool depthReleased = synthesisDepthReleased[eye];
            const FrameSynthesisMetadata& metadata = synthesisMetadata[eye];
            const auto& pose = metadata.appSpaceDelta;
            const XrPosef delta{{pose.orientation.x, pose.orientation.y, pose.orientation.z, pose.orientation.w},
                {pose.position.x, pose.position.y, pose.position.z}};
            synthesisReady[eye] = stereoRendered && motionReleased && depthReleased && synthesisPendingValid[eye] &&
                pose.valid && xr::validSynthesisMetadata(metadata.nearZ, metadata.farZ, delta,
                    {metadata.motionScaleX, metadata.motionScaleY}, {metadata.motionOffsetX, metadata.motionOffsetY});
            synthesisReady[eye] = synthesisContent.complete(eye, metadata.contentFrameId,
                synthesisReady[eye], motionReleased, depthReleased);
            synthesisExportPending[eye] = false;
            if(!motionReleased || !depthReleased) synthesisAvailable = false;
        }
        batchExportSeconds = Sys_DoubleTime() - exportBegan;
        return released;
#else
        (void)stereoRendered;
        return true;
#endif
    }

    void rejectFrameSynthesis() override
    {
        synthesisFrameRejected = true;
        synthesisReady[0] = synthesisReady[1] = false;
    }

    void endFrame(bool rendered) override
    {
        if(!frameBegun)
        {
            return;
        }
#ifdef QVR_OPENXR_INTEROP
        if(interopMode && rendered && !holding && (!interopEyeReleased[0] || !interopEyeReleased[1]))
        {
            frameInvalid = true;
            haveLastViews = false;
        }
#endif
        frameBegun = false;

        XrCompositionLayerProjectionView projViews[2]{};
        XrFrameSynthesisInfoEXT synthesisInfo[2]{};
        XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};

        XrFrameEndInfo endInfo{XR_TYPE_FRAME_END_INFO};
        endInfo.displayTime = frameState.predictedDisplayTime;
        endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;

        const XrCompositionLayerBaseHeader* submitted[2];
        uint32_t count = 0;
        XrCompositionLayerProjectionView candidateLastViews[2]{};

        const bool held = rendered && holding && haveLastViews;
        const bool candidateProjection = rendered && frameState.shouldRender && !frameInvalid && !held;
        // Verify the per-eye renderer mappings describe one rigid app-space
        // transform, then serialize exactly the same pose into both views.
        const auto applicationMatrix = [](const Pose& pose) {
            glm::mat4 matrix = glm::mat4_cast(pose.orientation);
            matrix[3] = glm::vec4{pose.position, 1.f};
            return matrix;
        };
        const bool sharedApplication = synthesisReady[0] && synthesisReady[1] && synthesis::sameApplicationDelta(
            applicationMatrix(synthesisMetadata[0].appSpaceDelta), applicationMatrix(synthesisMetadata[1].appSpaceDelta));
        const bool withSynthesis = rendered && frameState.shouldRender && !held && !frameInvalid && !synthesisFrameRejected &&
            sharedApplication && synthesisContent.ready() && frameSynthesisAvailable();
        holding = false;
        if(rendered && frameState.shouldRender && !frameInvalid)
        {
            for(int eye = 0; eye < 2; eye++)
            {
                if(held)
                {
                    // The swapchains' last released images, with the poses they were rendered for (the spec: a
                    // layer shows its swapchain's last released image).
                    projViews[eye] = lastViews[eye];
                    continue;
                }
                projViews[eye] = XrCompositionLayerProjectionView{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
                projViews[eye].pose = views[eye].pose;
                projViews[eye].fov = views[eye].fov;
                projViews[eye].subImage.swapchain = swapchains[eye].handle;
                projViews[eye].subImage.imageRect.extent = {swapchains[eye].width, swapchains[eye].height};
                candidateLastViews[eye] = projViews[eye];
                if(withSynthesis)
                {
                    XrFrameSynthesisInfoEXT& info = synthesisInfo[eye];
                    const Pose& pose = synthesisMetadata[0].appSpaceDelta;
                    const XrPosef delta{{pose.orientation.x, pose.orientation.y, pose.orientation.z, pose.orientation.w},
                        {pose.position.x, pose.position.y, pose.position.z}};
                    info = xr::frameSynthesisInfo(synthesisMotion[eye].handle, synthesisDepth[eye].handle,
                        synthesisMotion[eye].width, synthesisMotion[eye].height,
                        synthesisMetadata[eye].nearZ, synthesisMetadata[eye].farZ, delta,
                        vr_xr_frame_synthesis_relaxed.value > 0.f);
                    info.motionVectorScale.x = synthesisMetadata[eye].motionScaleX;
                    info.motionVectorScale.y = synthesisMetadata[eye].motionScaleY;
                    info.motionVectorOffset.x = synthesisMetadata[eye].motionOffsetX;
                    info.motionVectorOffset.y = synthesisMetadata[eye].motionOffsetY;
                    projViews[eye].next = &info;
                }
            }
            layer.space = worldSpace;
            layer.viewCount = 2;
            layer.views = projViews;
            submitted[count++] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&layer);
            (held ? timing.held : timing.rendered)++;
        }
        else
        {
            timing.empty += panelPending ? 0 : 1;
        }

        XrCompositionLayerQuad quad{XR_TYPE_COMPOSITION_LAYER_QUAD};
        if(panelPending && frameState.shouldRender
#ifdef QVR_OPENXR_INTEROP
            && (!interopMode || !frameInvalid)
#endif
            )
        {
            // Placed anew only after it was away for a while, not across a frame or two without
            // it (loading, a skipped 2D pass).
            if(!panelShown && realtime - panelLastShown > 1.0)
            {
                placePanel();
            }
            panelLastShown = realtime;

            quad.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT; // premultiplied
            quad.space = worldSpace;
            quad.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
            quad.subImage.swapchain = panel.handle;
            quad.subImage.imageRect.extent = {panel.width, panel.height};
            quad.pose = panelPose;
            quad.size = {panelSize().x, panelSize().y};
            submitted[count++] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&quad);
            timing.panels++;
        }
        panelShown = panelPending && frameState.shouldRender
#ifdef QVR_OPENXR_INTEROP
            && (!interopMode || !frameInvalid)
#endif
            ;
        panelPending = false;

        endInfo.layerCount = count;
        endInfo.layers = count ? submitted : nullptr;
        const double t0 = Sys_DoubleTime();
        const XrResult ended = xrEndFrame(session, &endInfo);
        submitSucceeded = check(ended, "xrEndFrame");
#ifdef QVR_OPENXR_INTEROP
        if(interopMode) submitSucceeded = ended == XR_SUCCESS;
        if(interopMode && !submitSucceeded)
        {
            haveLastViews = false;
            panelShown = false;
        }
#endif
        if(submitSucceeded && candidateProjection)
        {
            for(int eye = 0; eye < 2; ++eye) lastViews[eye] = candidateLastViews[eye];
            haveLastViews = true;
        }
        lastProjectionTime = submitSucceeded && layer.viewCount == 2 && !held ? frameState.predictedDisplayTime : 0;
        timeCall(CallEnd, t0);
        if(COM_CheckParm("-qvr-frame-diagnostics") && Sys_DoubleTime() - diagnosticLayersAt >= 1.0)
        {
            diagnosticLayersAt = Sys_DoubleTime();
            bool deferredExports = false;
#ifdef QVR_OPENXR_INTEROP
            deferredExports = interopMode && vr_xr_vulkan_defer_wait.value > 0.f;
#endif
            xrruntime::logLine(va("FRAME_LAYERS projection=%d stereo_views=%u quad=%d total=%u held=%d synthesis=%d synthesis_relaxed=%d defer_wait=%d\n",
                layer.viewCount == 2, layer.viewCount, panelShown, count, held, withSynthesis,
                withSynthesis && vr_xr_frame_synthesis_relaxed.value > 0.f,
                deferredExports));
            if(synthesisAvailable)
            {
                const auto& left = synthesisContent.eye(0);
                const auto& right = synthesisContent.eye(1);
                xrruntime::logLine(va("FRAME_SYNTHESIS_CONTENT frame=%llu fresh=%d/%d motion_index=%u/%u depth_index=%u/%u "
                    "app_delta_shared=%d submitted=%d near_far=%g/%g,%g/%g native_gl_export_cpu_ms=%.3f/%.3f batch_export_cpu_ms=%.3f\n",
                    static_cast<unsigned long long>(synthesisContent.frameId()), synthesisReady[0], synthesisReady[1],
                    left.motionIndex, right.motionIndex, left.depthIndex, right.depthIndex, sharedApplication,
                    withSynthesis && submitSucceeded, synthesisMetadata[0].nearZ, synthesisMetadata[0].farZ,
                    synthesisMetadata[1].nearZ, synthesisMetadata[1].farZ,
                    synthesisExportSeconds[0] * 1000.0, synthesisExportSeconds[1] * 1000.0, batchExportSeconds * 1000.0));
            }
        }
        timing.frames++;
    }

    [[nodiscard]] GraphicsImage acquirePanelImage(int width, int height) override
    {
#ifdef QVR_OPENXR_INTEROP
        if(interopMode && (fatalSwapchainError || interopTeardownBlocked)) return {};
#endif
        if(!frameBegun || panelPending || width <= 0 || height <= 0 || !ensurePanelSwapchain(width, height))
        {
            return {};
        }
#ifdef QVR_OPENXR_INTEROP
        const unsigned texture = interopMode ? acquireInteropImage(panel, panelBridge) : acquireImage(panel);
#else
        const unsigned texture = acquireImage(panel);
#endif
        return texture ? GraphicsImage{GraphicsApi::OpenGL, texture, 0, panel.format, panel.width, panel.height} : GraphicsImage{};
    }

    void releasePanelImage() override
    {
#ifdef QVR_OPENXR_INTEROP
        panelPending = interopMode ? releaseInteropImage(panel, panelBridge) : releaseImage(panel);
#else
        panelPending = releaseImage(panel);
#endif
    }

    [[nodiscard]] bool runtimePanel(Pose& pose, glm::vec2& size) const override
    {
        if(!panelShown)
        {
            return false;
        }
        pose = panelPlaced;
        size = panelSize();
        return true;
    }

private:
    // The panel: runtimePanelWidth wide, the canvas's shape, placed by placeRuntimePanel (vr_backend.hpp).
    [[nodiscard]] glm::vec2 panelSize() const
    {
        return {runtimePanelWidth, panel.width > 0 ? runtimePanelWidth * static_cast<float>(panel.height) / static_cast<float>(panel.width) : 0.f};
    }

    void placePanel()
    {
        panelPlaced = placeRuntimePanel(lastHead);
        const glm::quat& q = panelPlaced.orientation;
        panelPose.position = {panelPlaced.position.x, panelPlaced.position.y, panelPlaced.position.z};
        panelPose.orientation = {q.x, q.y, q.z, q.w};
    }

    bool ensurePanelSwapchain(int width, int height)
    {
        if(panel.handle != XR_NULL_HANDLE && panel.width == width && panel.height == height)
        {
#ifdef QVR_OPENXR_INTEROP
            if(interopMode && (panel.vulkanImages.empty() || !panelBridge.healthy()))
            {
                invalidateInteropFrame("cached panel chain lacks native images or healthy bridge");
                return false;
            }
#endif
            return true;
        }
        if(panel.handle != XR_NULL_HANDLE)
        {
#ifdef QVR_OPENXR_INTEROP
            if(interopMode)
            {
                panelShown = panelPending = false;
                if(panel.acquired)
                {
                    invalidateInteropFrame("panel resize while native image remains acquired");
                    return false;
                }
                restoreInteropSrgb(panel);
                if(!panelBridge.shutdown())
                {
                    blockInteropTeardown(panelBridge.error());
                    return false;
                }
                if(xrDestroySwapchain(panel.handle) != XR_SUCCESS)
                {
                    blockInteropTeardown("panel swapchain destruction failed");
                    return false;
                }
            }
            else
#endif
            {
                const XrSwapchain handle = panel.handle;
                guarded("xrDestroySwapchain", [handle] { return xrDestroySwapchain(handle); });
            }
            panel = Swapchain{};
        }
        return createSwapchain(panel, width, height,
            XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT, "xrCreateSwapchain (panel)");
    }

    struct Swapchain
    {
        XrSwapchain handle{XR_NULL_HANDLE};
        int32_t width{0};
        int32_t height{0};
        int64_t format{0};
        za::Vector<XrSwapchainImageOpenGLKHR> images;
#ifdef QVR_OPENXR_INTEROP
        za::Vector<XrSwapchainImageVulkanKHR> vulkanImages;
        XrSwapchainCreateInfo request{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        SDL_GLContext writeContext{nullptr};
        bool srgbSaved{false}, srgbWasEnabled{false};
#endif
        uint32_t acquiredIndex{0};
        bool acquired{false};
        bool waited{false};
    };

#ifdef QVR_OPENXR_INTEROP
    openxr_vulkan::Bootstrap vulkanBootstrap;
    gl_vulkan::Bridge eyeBridges[2], panelBridge, motionBridges[2], depthBridges[2];
    bool interopMode{false}, interopTeardownBlocked{false};
    bool interopEyeReleased[2]{false, false};
    bool interopEyeQueued[2]{false, false};
    bool synthesisExportPending[2]{false, false};
    bool synthesisPendingValid[2]{false, false};
    bool synthesisMotionReleased[2]{false, false}, synthesisDepthReleased[2]{false, false};

    void traceInteropTeardown(const char* stage)
    {
        xrruntime::logLine(va("%s INTEROP_TEARDOWN t=%.6f %s\n", wallClock(), Sys_DoubleTime(), stage));
    }

    void blockInteropTeardown(const char* reason)
    {
        interopTeardownBlocked = interopParentsRetained = true;
        fatalSwapchainError = frameInvalid = true;
        haveLastViews = false;
        panelShown = panelPending = false;
        xrruntime::warn("OpenXR interop: retained native parents; fallback/recreation blocked: %s\n", reason);
    }
    void invalidateInteropFrame(const char* reason)
    {
        frameInvalid = fatalSwapchainError = true;
        haveLastViews = false;
        panelShown = panelPending = false;
        xrruntime::warn("OpenXR interop: frame rejected: %s\n", reason);
    }
    void restoreInteropSrgb(Swapchain& sc)
    {
        if(sc.srgbSaved && SDL_GL_GetCurrentContext() == sc.writeContext)
        {
            if(sc.srgbWasEnabled) glEnable(GL_FRAMEBUFFER_SRGB);
            else glDisable(GL_FRAMEBUFFER_SRGB);
            sc.srgbSaved = false;
            sc.writeContext = nullptr;
        }
    }
    gl_vulkan::Bridge* bridgeFor(Swapchain& sc)
    {
        if(&sc == &swapchains[0]) return &eyeBridges[0];
        if(&sc == &swapchains[1]) return &eyeBridges[1];
        for(int eye = 0; eye < 2; ++eye)
        {
            if(&sc == &synthesisMotion[eye]) return &motionBridges[eye];
            if(&sc == &synthesisDepth[eye]) return &depthBridges[eye];
        }
        return &sc == &panel ? &panelBridge : nullptr;
    }
    bool settleInteropFrame()
    {
        bool settled = true;
        panelShown = panelPending = false;
        haveLastViews = false;
        for(Swapchain* sc : {&swapchains[0], &swapchains[1], &panel,
                &synthesisMotion[0], &synthesisMotion[1], &synthesisDepth[0], &synthesisDepth[1]})
        {
            if(!sc->acquired) continue;
            if(!sc->waited)
            {
                XrSwapchainImageWaitInfo wait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
                wait.timeout = 100'000'000;
                const double began = Sys_DoubleTime();
                XrResult result;
                do { result = xrWaitSwapchainImage(sc->handle, &wait); }
                while(result == XR_TIMEOUT_EXPIRED && Sys_DoubleTime() - began < 5.0);
                sc->waited = result == XR_SUCCESS;
            }
            if(!sc->waited) { settled = false; continue; }
            if(auto* bridge = bridgeFor(*sc); bridge && !bridge->waitForCompletion())
            {
                // Paired submission may have stopped between motion and depth.
                // Keep completion-before-release even on this failure path.
                settled = false;
                continue;
            }
            // No layer will reference these images. Destination commands have
            // completed on the bound queue with restored layout. Staging GL
            // work has no access to XR destination images until a copy submit.
            XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
            if(xrReleaseSwapchainImage(sc->handle, &release) == XR_SUCCESS)
                sc->acquired = sc->waited = false;
            else settled = false;
        }
        if(frameBegun)
        {
            endFrame(false); // zero layers, even when an acquisition remains tracked
            if(!submitSucceeded) settled = false;
        }
        if(!settled)
        {
            blockInteropTeardown("could not settle acquired XR images; keeping tracked state and native parents");
            return false;
        }
        return true;
    }
#endif
    XrInstance instance{XR_NULL_HANDLE};
    XrSystemId systemId{XR_NULL_SYSTEM_ID};
    XrSession session{XR_NULL_HANDLE};
    XrSpace worldSpace{XR_NULL_HANDLE};
    XrSpace viewSpace{XR_NULL_HANDLE};
    bool formatLogged{false}; // the swapchain's format printed (createSwapchains: once)
    XrActionSet actionSet{XR_NULL_HANDLE};
    XrAction gripPose{XR_NULL_HANDLE};
    XrAction triggerAction{XR_NULL_HANDLE};
    XrAction gripAction{XR_NULL_HANDLE};
    XrAction primaryAction{XR_NULL_HANDLE};
    XrAction secondaryAction{XR_NULL_HANDLE};
    XrAction stickClickAction{XR_NULL_HANDLE};
    XrAction menuAction{XR_NULL_HANDLE};
    XrAction stickAction{XR_NULL_HANDLE};
    XrAction triggerValueAction{XR_NULL_HANDLE};
    XrAction gripValueAction{XR_NULL_HANDLE};
    XrAction thumbTouchAction{XR_NULL_HANDLE};
    XrAction triggerTouchAction{XR_NULL_HANDLE};
    XrAction hapticAction{XR_NULL_HANDLE};
    XrPath handPaths[2]{XR_NULL_PATH, XR_NULL_PATH}; // [0] left, [1] right
    XrSpace handSpaces[2]{XR_NULL_HANDLE, XR_NULL_HANDLE};
    Swapchain swapchains[2];
    Swapchain synthesisMotion[2], synthesisDepth[2];
    XrFrameSynthesisConfigViewEXT synthesisConfig[2]{};
    FrameSynthesisMetadata synthesisMetadata[2];
    bool synthesisMotionAcquired[2]{false, false}, synthesisDepthAcquired[2]{false, false};
    bool synthesisReady[2]{false, false};
    synthesis::SubmissionContent synthesisContent;
    double synthesisExportSeconds[2]{0.0, 0.0};
    double batchExportSeconds{0.0}; // Whole stereo batch once; includes outstanding producer GPU work.
    bool synthesisFrameRejected{false};
    bool synthesisExtension{false}, synthesisAvailable{false}, submitSucceeded{false};
    bool frameInvalid{false};
    bool fatalSwapchainError{false};
    bool synthesisSrgbMotion{false};
    XrTime lastProjectionTime{0};
    Swapchain panel;
    int64_t colorFormat{GL_RGBA8};
    bool panelPending{false}; // an image was released for the frame being finished
    bool panelShown{false};   // the panel was in the last submitted frame (keeps its place)
    double panelLastShown{-10.0};
    XrPosef panelPose{{0.f, 0.f, 0.f, 1.f}, {0.f, 0.f, 0.f}};
    Pose panelPlaced; // the same
    Pose lastHead; // the last valid head pose, which the panel is placed in front of
    XrSessionState sessionState{XR_SESSION_STATE_UNKNOWN};
    // The session's last states as they came (sessionStates()), the oldest dropped; and why the session ended.
    static constexpr int stateTrailMax = 12;
    const char* stateTrail[stateTrailMax]{}; // stateName()s, the instance's loss too (string literals)
    int stateTrailCount{0};
    char stateTrailText[stateTrailMax * 24]{};
    SessionEnd endReason{SessionEnd::Lost};
    bool sessionRunning{false};
    bool swapIntervalChanged{false};
    bool frameBegun{false};
    XrFrameState frameState{XR_TYPE_FRAME_STATE};
    XrView views[2]{{XR_TYPE_VIEW}, {XR_TYPE_VIEW}};
    XrViewConfigurationView configViews[2]{{XR_TYPE_VIEW_CONFIGURATION_VIEW}, {XR_TYPE_VIEW_CONFIGURATION_VIEW}};
    bool visibilityMaskExtension{false}; // XR_KHR_visibility_mask enabled
    bool frameControllerExtension{false};
    bool userPresenceExtension{false};
    bool supportsUserPresence{false};
    bool userPresenceKnown{false};
    bool isUserPresent{true};
    unsigned captureTexture[2]{};
    int captureFrames[2]{};
    // The vendors' controller profiles (vr_xr_vendor_profiles), each enabled when the runtime offers its extension:
    // suggestBindings gives each its buttons, the menu button among them.
    enum VendorProfile
    {
        VendorPico,    // XR_BD_controller_interaction: Pico Neo 3, Pico 4
        VendorCosmos,  // HTC Vive Cosmos
        VendorFocus3,  // HTC Vive Focus 3 (and XR Elite)
        VendorHp,      // HP Reverb G2
        VendorOdyssey, // Samsung Odyssey (a WMR controller)
        VendorCount
    };
    static constexpr const char* vendorExtensions[VendorCount] = {"XR_BD_controller_interaction",
        "XR_HTC_vive_cosmos_controller_interaction", "XR_HTC_vive_focus3_controller_interaction",
        "XR_EXT_hp_mixed_reality_controller", "XR_EXT_samsung_odyssey_controller"};
    bool vendorProfile[VendorCount]{};
    char runtime[XR_MAX_RUNTIME_NAME_SIZE + 32]{}; // its name and version
    char system[XR_MAX_SYSTEM_NAME_SIZE]{};        // the headset's name (systemName)
    bool vdxr{false};                             // Virtual Desktop's own runtime (VDXR)
    bool debugUtils{false};                       // diagnostics mode: XR_EXT_debug_utils enabled
    XrDebugUtilsMessengerEXT messenger{XR_NULL_HANDLE};
    bool runtimeFaulted{false};                   // it crashed inside a guarded call (guarded): this start fails
    char faultText[512]{};                        // "xrEnumerateSwapchainImages: an access violation reading 0x0 in ..."
    char crashBase[1024]{};                       // the crash report's VR line: the runtime, its version, the headset
    za::Vector<int64_t> offeredFormats;           // xrEnumerateSwapchainFormats' (createSwapchain checks against it)
    uint32_t maxImageWidth{0};                    // the system's largest swapchain image (0: not known)
    uint32_t maxImageHeight{0};
#ifdef _WIN32
    HDC glDC{nullptr};                            // the game's GL context, as bound to the session
    HGLRC glRC{nullptr};
#endif
    xrruntime::Attempt currentAttempt;            // the runtime being started (start())
    float debugButtonsWas{0.f};                   // vr_debug_buttons last frame
    PFN_xrGetVisibilityMaskKHR getVisibilityMask{nullptr};
    HiddenArea hidden[2];
    bool hiddenStale[2]{true, true}; // to fetch (again) before the next frame
    XrCompositionLayerProjectionView lastViews[2]{}; // the last rendered frame's views (vr_xr_unfocused)
    bool haveLastViews{false};
    bool holding{false}; // this frame shows them again (FrameState::hold)

    // vr_xr_log_timing: each second's OpenXR calls (CPU ms), frames and display periods, a line in qvr_openxr.txt.
    enum Call
    {
        CallPoll,
        CallWait,
        CallBegin,
        CallSync,
        CallAcquire,
        CallWaitImage,
        CallRelease,
        CallEnd,
        CallCount
    };
    struct CallTime
    {
        double sum{0.0};
        double max{0.0};
        int count{0};
    };
    struct Timing
    {
        CallTime calls[CallCount];
        CallTime period;          // between xrWaitFrame's returns
        double since{-1.0};       // the second's start (Sys_DoubleTime)
        double lastWaited{-1.0};  // the last xrWaitFrame's return
        XrTime lastDisplay{0};    // its predictedDisplayTime
        int frames{0};            // xrEndFrame calls
        int rendered{0};          // with the eyes rendered
        int held{0};              // with the last ones again (vr_xr_unfocused)
        int empty{0};             // without a layer
        int panels{0};            // with the runtime's panel (a quad layer)
        int noRender{0};          // shouldRender false
        int missed{0};            // display periods skipped (predictedDisplayTime moved on by more than one)
        int periodNs{0};          // predictedDisplayPeriod
    };
    Timing timing;
    double diagnosticAt{-1.0};
    double diagnosticLayersAt{-1.0};

    void timeCall(Call call, double since)
    {
        CallTime& c = timing.calls[call];
        const double ms = (Sys_DoubleTime() - since) * 1000.0;
        c.sum += ms;
        c.max = za::max(c.max, ms);
        c.count++;
    }

    // After xrWaitFrame: the frame period, the display periods missed, and once a second the log's line.
    void noteWaited()
    {
        const double now = Sys_DoubleTime();
        if(timing.lastWaited >= 0.0)
        {
            const double ms = (now - timing.lastWaited) * 1000.0;
            timing.period.sum += ms;
            timing.period.max = za::max(timing.period.max, ms);
            timing.period.count++;
        }
        timing.lastWaited = now;
        const XrDuration period = frameState.predictedDisplayPeriod;
        timing.periodNs = static_cast<int>(period);
        if(timing.lastDisplay != 0 && period > 0)
        {
            const XrTime steps = (frameState.predictedDisplayTime - timing.lastDisplay + period / 2) / period;
            timing.missed += steps > 1 ? static_cast<int>(steps - 1) : 0;
        }
        VR_TracyDisplay(static_cast<double>(period) * 1e-6, timing.missed);
        timing.lastDisplay = frameState.predictedDisplayTime;

        if(timing.since < 0.0)
        {
            timing.since = now;
        }
        if(now - timing.since < 1.0)
        {
            return;
        }
        if(vr_xr_log_timing.value != 0.f)
        {
            za::String line = va("%s xr %.1fs: %d frames (%d rendered, %d last again, %d panel, %d empty; shouldRender off %d), "
                                 "%d display periods missed (%.2f ms), period %.2f/%.2f ms; ms avg/max:",
                wallClock(), now - timing.since, timing.frames, timing.rendered, timing.held, timing.panels, timing.empty,
                timing.noRender, timing.missed, timing.periodNs * 1e-6,
                timing.period.count ? timing.period.sum / timing.period.count : 0.0, timing.period.max);
            static constexpr const char* names[CallCount] = {
                "poll", "wait", "begin", "sync", "acquire", "waitimage", "release", "end"};
            for(int i = 0; i < CallCount; i++)
            {
                const CallTime& c = timing.calls[i];
                line += va(" %s %.2f/%.2f", names[i], c.sum / za::max(timing.frames, 1), c.max);
            }
            line += va("; %s\n", stateName(sessionState));
            xrruntime::logLine(line.cStr());
        }
        const XrTime lastDisplay = timing.lastDisplay;
        timing = Timing{};
        timing.since = timing.lastWaited = now;
        timing.lastDisplay = lastDisplay;
    }

    // The local time, for the log (hh:mm:ss.mmm).
    [[nodiscard]] static const char* wallClock()
    {
#ifdef _WIN32
        SYSTEMTIME t;
        GetLocalTime(&t);
        return va("%02d:%02d:%02d.%03d", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
#else
        timeval now{};
        gettimeofday(&now, nullptr);
        tm local{};
        localtime_r(&now.tv_sec, &local);
        return va("%02d:%02d:%02d.%03ld", local.tm_hour, local.tm_min, local.tm_sec, now.tv_usec / 1000);
#endif
    }

    [[nodiscard]] static const char* stateName(XrSessionState state)
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

    // A state (or the instance's loss) into the trail sessionStates() gives, the oldest dropped past stateTrailMax.
    void noteState(const char* name)
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

    [[nodiscard]] SessionEnd sessionEnd() const override
    {
        return endReason;
    }

    [[nodiscard]] const char* sessionStates() const override
    {
        return stateTrailText;
    }

    // Reads an eye's hidden area (the triangles the lenses never show).
    void fetchHiddenArea(int eye)
    {
        hiddenStale[eye] = false;
        if(!getVisibilityMask)
        {
            return;
        }
        HiddenArea& h = hidden[eye];
        h.vertices.clear();
        h.indices.clear();

        XrVisibilityMaskKHR mask{XR_TYPE_VISIBILITY_MASK_KHR};
        if(!check(getVisibilityMask(session, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, static_cast<uint32_t>(eye),
                      XR_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH_KHR, &mask),
               "xrGetVisibilityMaskKHR") ||
            mask.vertexCountOutput == 0 || mask.indexCountOutput < 3)
        {
            return;
        }
        za::Vector<XrVector2f> vertices(mask.vertexCountOutput);
        h.indices.resize(mask.indexCountOutput);
        mask.vertexCapacityInput = mask.vertexCountOutput;
        mask.vertices = vertices.data();
        mask.indexCapacityInput = mask.indexCountOutput;
        mask.indices = h.indices.data();
        if(!check(getVisibilityMask(session, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, static_cast<uint32_t>(eye),
                      XR_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH_KHR, &mask),
               "xrGetVisibilityMaskKHR"))
        {
            h.indices.clear();
            return;
        }
        h.indices.resize(mask.indexCountOutput - mask.indexCountOutput % 3);
        for(za::U32& i : h.indices)
        {
            if(i >= mask.vertexCountOutput)
            {
                h.indices.clear(); // malformed: none rather than garbage
                return;
            }
        }
        h.vertices.reserve(mask.vertexCountOutput);
        for(uint32_t i = 0; i < mask.vertexCountOutput; i++)
        {
            h.vertices.emplaceBack(vertices[i].x, vertices[i].y);
        }
    }

    bool check(XrResult result, const char* what) const
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

    // A line in the start's breadcrumbs before the call it names (the crash report's "Last GL steps", gl_startup.log
    // while the game starts), in qvr_openxr.txt, and in the crash report's VR line.
    void step(const char* format, ...)
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

    // An OpenXR call under vr_xr_guard's handler: a crash inside the runtime is logged and returned as a failure, the
    // runtime marked as crashed (this start fails; the stop's destroy calls, guarded too, are all it gets after).
    template <typename Call>
    XrResult guarded(const char* what, Call&& call)
    {
        if(vr_xr_guard.value == 0.f)
        {
            return call();
        }
        Fault fault;
        const XrResult result = guardedCall(call, fault);
        if(fault.code == 0)
        {
            return result;
        }
        char text[384];
        describeFault(fault, text, sizeof(text));
        q_snprintf(faultText, sizeof(faultText), "%s: %s", what, text);
        runtimeFaulted = true;
        xrruntime::warn("OpenXR: %s crashed inside %s: %s; caught (vr_xr_guard 1), this VR start fails\n",
            runtime[0] ? runtime : "the runtime", what, text);
        VR_GLStep("OpenXR: %s crashed: %s (caught)", what, text);
#ifdef _WIN32
        // The runtime may have left its own GL context current (VDXR switches to the game's around its GL calls).
        if(glRC && wglGetCurrentContext() != glRC)
        {
            const BOOL made = wglMakeCurrent(glDC, glRC);
            xrruntime::warn("OpenXR: the game's GL context made current again after the crash: %s\n", made ? "done" : "FAILED");
        }
#endif
        return XR_ERROR_RUNTIME_FAILURE;
    }

#ifdef QVR_OPENXR_INTEROP
    bool createInteropSwapchain(Swapchain& sc, const XrSwapchainCreateInfo& request, const char* what,
        gl_vulkan::Format stagingFormat = gl_vulkan::Format::SrgbColorBytes)
    {
        const auto failed = [this](const char* reason) { invalidateInteropFrame(reason); return false; };
        gl_vulkan::Bridge* bridge = bridgeFor(sc);
        if(!bridge || sc.handle != XR_NULL_HANDLE || interopTeardownBlocked) return failed("invalid native chain creation state");
        const XrResult created = xrCreateSwapchain(session, &request, &sc.handle);
        if(created != XR_SUCCESS) { check(created, what); return failed("native swapchain creation failed"); }
        sc.request = request;
        sc.width = static_cast<int32_t>(request.width);
        sc.height = static_cast<int32_t>(request.height);
        // Renderer descriptors remain GL; request.format describes the XR VkImage.
        sc.format = stagingFormat == gl_vulkan::Format::MotionRGBA16F ? 0x881A :
            stagingFormat == gl_vulkan::Format::DepthD32F ? 0x8CAC : GL_RGBA8;
        uint32_t count = 0;
        if(xrEnumerateSwapchainImages(sc.handle, 0, &count, nullptr) != XR_SUCCESS || !count)
        {
            xrruntime::warn("OpenXR interop: native swapchain image count failed/empty\n");
            return failed("native swapchain image count failed");
        }
        sc.vulkanImages.resize(count, XrSwapchainImageVulkanKHR{XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR});
        const uint32_t capacity = count;
        if(xrEnumerateSwapchainImages(sc.handle, capacity, &count,
            reinterpret_cast<XrSwapchainImageBaseHeader*>(sc.vulkanImages.data())) != XR_SUCCESS || count != capacity)
        {
            xrruntime::warn("OpenXR interop: native swapchain image enumeration failed/changed\n");
            return failed("native swapchain image enumeration failed");
        }
        for(const auto& image : sc.vulkanImages)
            if(!image.image || !gl_vulkan::destinationFromSwapchainRequest(image.image, request, created).image)
                return failed("native image/usage contract invalid");
        if(!bridge->initialize(vulkanBootstrap.context(), stagingFormat, request.width, request.height))
        {
            xrruntime::warn("OpenXR interop: staging initialization failed: %s\n", bridge->error());
            return failed(bridge->error());
        }
        xrruntime::logLine(va("OpenXR interop: %s %dx%d, XR VkFormat=%lld, GL internal=%s, "
            "depth-attachment-export=%d, usage=0x%llx, %u native images\n",
            what, sc.width, sc.height, static_cast<long long>(request.format),
            glFormatName(sc.format), stagingFormat == gl_vulkan::Format::DepthD32F,
            static_cast<unsigned long long>(request.usageFlags), count));
        return true;
    }
    unsigned acquireInteropImage(Swapchain& sc, gl_vulkan::Bridge& bridge)
    {
        if(fatalSwapchainError || interopTeardownBlocked || !frameBegun || !sc.handle || sc.acquired)
            return 0;
        // A prior successful export can remain in flight after XR release.
        // Drain before reusing staging, command buffers, descriptors or fences.
        // This also handles toggling defer_wait off while work is outstanding.
        {
            QVR_PROFILE("synthesis reuse wait");
            if(!bridge.waitForCompletion() || !bridge.healthy())
            {
                invalidateInteropFrame(bridge.error());
                return 0;
            }
        }
        XrSwapchainImageAcquireInfo acquire{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        uint32_t index = 0;
        const double acquiredAt = Sys_DoubleTime();
        const XrResult acquired = xrAcquireSwapchainImage(sc.handle, &acquire, &index);
        timeCall(CallAcquire, acquiredAt);
        if(acquired != XR_SUCCESS)
        {
            check(acquired, "xrAcquireSwapchainImage (interop)");
            invalidateInteropFrame("swapchain acquire failed");
            return 0;
        }
        sc.acquired = true;
        sc.acquiredIndex = index;
        XrSwapchainImageWaitInfo wait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
        wait.timeout = 100'000'000; // 100 ms attempts, total wait bounded to five seconds
        const double began = Sys_DoubleTime();
        XrResult waited;
        do { waited = xrWaitSwapchainImage(sc.handle, &wait); }
        while(waited == XR_TIMEOUT_EXPIRED && Sys_DoubleTime() - began < 5.0);
        timeCall(CallWaitImage, began);
        if(waited != XR_SUCCESS)
        {
            check(waited, "xrWaitSwapchainImage (interop)");
            invalidateInteropFrame("swapchain wait failed/timed out; image retained without access/release");
            return 0;
        }
        sc.waited = true;
        if(index >= sc.vulkanImages.size() || !sc.vulkanImages[index].image)
        {
            // No app GPU access occurred; a successfully waited image can be returned.
            (void)releaseImage(sc);
            invalidateInteropFrame("native acquired image index/handle invalid");
            return 0;
        }
        sc.writeContext = SDL_GL_GetCurrentContext();
        if(!sc.writeContext)
        {
            invalidateInteropFrame("GL renderer context is not current");
            return 0;
        }
        if(sc.format == GL_RGBA8)
        {
            sc.srgbWasEnabled = glIsEnabled(GL_FRAMEBUFFER_SRGB) != GL_FALSE;
            sc.srgbSaved = true;
            glDisable(GL_FRAMEBUFFER_SRGB); // already gamma-encoded Quake bytes
        }
        if(!bridge.beginGlWrite())
        {
            restoreInteropSrgb(sc);
            invalidateInteropFrame(bridge.error());
            return 0;
        }
        return bridge.glTexture();
    }
    bool enqueueInteropImage(Swapchain& sc, gl_vulkan::Bridge& bridge, bool contentValid)
    {
        if(!sc.acquired || !sc.waited || sc.acquiredIndex >= sc.vulkanImages.size())
        {
            restoreInteropSrgb(sc);
            invalidateInteropFrame("release without a valid acquired/waited native image");
            return false;
        }
        auto destination = gl_vulkan::destinationFromSwapchainRequest(sc.vulkanImages[sc.acquiredIndex].image, sc.request, XR_SUCCESS);
        destination.acquiredAndWaited = true;
        // Invalid producer data still returns staging ownership, without reading
        // uninitialized pixels or modifying the runtime image.
        const bool copied = contentValid ? bridge.enqueueCopyTo(destination) : bridge.enqueueCancelGlWrite();
        restoreInteropSrgb(sc);
        if(!copied)
        {
            invalidateInteropFrame(bridge.error());
            return false; // native image remains acquired; teardown must drain before destroying it
        }
        return true;
    }
    bool releaseCompletedInteropImage(Swapchain& sc, gl_vulkan::Bridge& bridge, bool allowSubmitted = false)
    {
        if(!sc.acquired || !sc.waited || !(bridge.healthy() || (allowSubmitted && bridge.submitted())))
        {
            invalidateInteropFrame("native release requires a healthy completed or permitted submitted export");
            return false;
        }
        XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        const double t0 = Sys_DoubleTime();
        const XrResult result = xrReleaseSwapchainImage(sc.handle, &release);
        timeCall(CallRelease, t0);
        if(result != XR_SUCCESS)
        {
            invalidateInteropFrame("native swapchain release failed");
            return false;
        }
        sc.acquired = sc.waited = false;
        return true;
    }
    bool releaseInteropImage(Swapchain& sc, gl_vulkan::Bridge& bridge, bool contentValid = true)
    {
        if(!enqueueInteropImage(sc, bridge, contentValid) || !bridge.waitForCompletion())
        {
            invalidateInteropFrame(bridge.error());
            return false;
        }
        return releaseCompletedInteropImage(sc, bridge);
    }
#endif
    // A swapchain of width x height images in colorFormat, validated against what the runtime offered (the format,
    // the size, the sample count), each request logged before the call; the images checked (a count, none zero).
    bool createSwapchain(Swapchain& sc, int32_t width, int32_t height, XrSwapchainUsageFlags usage, const char* what,
        int64_t format = 0)
    {
        XrSwapchainCreateInfo info{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        info.usageFlags = usage;
        info.format = format ? format : colorFormat;
        info.sampleCount = 1;
        info.width = static_cast<uint32_t>(width);
        info.height = static_cast<uint32_t>(height);
        info.faceCount = 1;
        info.arraySize = 1;
        info.mipCount = 1;
#ifdef QVR_OPENXR_INTEROP
        if(interopMode)
        {
            const bool depth = info.format == VK_FORMAT_D32_SFLOAT;
            const auto staging = depth ? gl_vulkan::Format::DepthD32F :
                info.format == VK_FORMAT_R16G16B16A16_SFLOAT ? gl_vulkan::Format::MotionRGBA16F :
                gl_vulkan::Format::SrgbColorBytes;
            // Runtime depth is written through its attachment, never a transfer.
            if(!depth) info.usageFlags |= XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
            return createInteropSwapchain(sc, info, what, staging);
        }
#endif
        bool offered = offeredFormats.empty(); // (none listed: the runtime's word is all there is)
        for(int64_t f : offeredFormats)
        {
            offered = offered || f == info.format;
        }
        const uint32_t maxW = maxImageWidth ? maxImageWidth : 16384u;
        const uint32_t maxH = maxImageHeight ? maxImageHeight : 16384u;
        if(!offered || width <= 0 || height <= 0 || info.width > maxW || info.height > maxH || runtimeFaulted)
        {
            xrruntime::warn("OpenXR: %s not requested: %dx%d %s (%s)\n", what, width, height, glFormatName(info.format),
                runtimeFaulted ? "the runtime crashed before"
                : !offered     ? "a format the runtime doesn't offer"
                               : va("a size outside 1..%ux%u", maxW, maxH));
            return false;
        }
        step("%s: requesting %dx%d %s (0x%llx), usage 0x%llx, %u sample, %u face, array %u, %u mip", what, width, height,
            glFormatName(info.format), static_cast<unsigned long long>(info.format), static_cast<unsigned long long>(usage),
            info.sampleCount, info.faceCount, info.arraySize, info.mipCount);
#ifdef _WIN32
        if(vr_xr_test_drop_context.value != 0.f) // (the test of contextReady: no context current, once)
        {
            Cvar_SetValueQuick(&vr_xr_test_drop_context, 0.f);
            wglMakeCurrent(nullptr, nullptr);
        }
#endif
        if(!contextReady(what))
        {
            return false;
        }
        // SteamVR checks glGetError after its own GL calls in here: an error the engine left
        // pending fails the swapchain ("SXR_GL_CHECK ... glGenTextures", GL_INVALID_VALUE).
        for(int i = 0; i < 16 && glGetError() != GL_NO_ERROR; i++)
        {
        }
        const XrSession s = session;
        XrSwapchain handle = XR_NULL_HANDLE;
        const XrResult created = guarded(what, [s, &info, &handle] { return xrCreateSwapchain(s, &info, &handle); });
        if(runtimeFaulted || !check(created, what) || handle == XR_NULL_HANDLE)
        {
            sc.handle = handle;
            return false;
        }
        sc.handle = handle;
        sc.width = width;
        sc.height = height;
        sc.format = info.format;
        uint32_t imageCount = 0;
        step("%s: xrEnumerateSwapchainImages (the count)", what);
        const XrResult counted = guarded("xrEnumerateSwapchainImages", [handle, &imageCount]
            { return xrEnumerateSwapchainImages(handle, 0, &imageCount, nullptr); });
        if(runtimeFaulted || !check(counted, "xrEnumerateSwapchainImages (the count)") || imageCount == 0 || imageCount > 64)
        {
            xrruntime::warn("OpenXR: %s: %u images\n", what, imageCount);
            return false;
        }
        sc.images.clear();
        sc.images.resize(imageCount, XrSwapchainImageOpenGLKHR{XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR});
        step("%s: xrEnumerateSwapchainImages (%u OpenGL images)", what, imageCount);
        if(!contextReady(what))
        {
            return false;
        }
        XrSwapchainImageBaseHeader* images = reinterpret_cast<XrSwapchainImageBaseHeader*>(sc.images.data());
        uint32_t filled = 0;
        const XrResult enumerated = guarded("xrEnumerateSwapchainImages", [handle, imageCount, &filled, images]
            { return xrEnumerateSwapchainImages(handle, imageCount, &filled, images); });
        if(runtimeFaulted || !check(enumerated, "xrEnumerateSwapchainImages (the images)"))
        {
            return false;
        }
        for(const XrSwapchainImageOpenGLKHR& image : sc.images)
        {
            if(image.image == 0)
            {
                xrruntime::warn("OpenXR: %s: the runtime gave %u images, one of them texture 0\n", what, filled);
                return false;
            }
        }
        // Named for vr_vram_report (the runtime's textures, in our context; a name that is not ours: a GL error, cleared).
        for(const XrSwapchainImageOpenGLKHR& image : sc.images)
        {
            GL_ObjectLabelFunc(GL_TEXTURE, image.image, -1, va("xr swapchain %s", what));
        }
        for(int i = 0; i < 16 && glGetError() != GL_NO_ERROR; i++)
        {
        }
        xrruntime::logLine(va("%s OpenXR: %s: %dx%d %s, %u images, %u sample, usage 0x%llx\n", wallClock(), what, width, height,
            glFormatName(info.format), imageCount, info.sampleCount, static_cast<unsigned long long>(usage)));
        return true;
    }

    [[nodiscard]] static const char* glFormatName(int64_t format)
    {
        switch(format)
        {
            case GL_SRGB8_ALPHA8: return "GL_SRGB8_ALPHA8";
            case GL_RGBA8: return "GL_RGBA8";
            case 0x8C41: return "GL_SRGB8";
            case 0x8051: return "GL_RGB8";
            case 0x881A: return "GL_RGBA16F";
            case 0x881B: return "GL_RGB16F";
            case 0x822F: return "GL_RG16F";
            case 0x8230: return "GL_RG32F";
            case 0x8814: return "GL_RGBA32F";
            case 0x8059: return "GL_RGB10_A2";
            case 0x8C3A: return "GL_R11F_G11F_B10F";
            case 0x81A5: return "GL_DEPTH_COMPONENT16";
            case 0x81A6: return "GL_DEPTH_COMPONENT24";
            case 0x8CAC: return "GL_DEPTH_COMPONENT32F";
            case 0x88F0: return "GL_DEPTH24_STENCIL8";
            case 0x8CAD: return "GL_DEPTH32F_STENCIL8";
            default: return va("0x%llx", static_cast<unsigned long long>(format));
        }
    }

    // The GL texture of the swapchain's next image, acquired and waited for; 0 on failure.
    [[nodiscard]] unsigned acquireImage(Swapchain& sc)
    {
        if(sc.handle == XR_NULL_HANDLE || sc.acquired) return 0;
        XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        uint32_t index = 0;
        double t0 = Sys_DoubleTime();
        const XrResult acquired = xrAcquireSwapchainImage(sc.handle, &acquireInfo, &index);
        timeCall(CallAcquire, t0);
        if(acquired != XR_SUCCESS)
        {
            check(acquired, "xrAcquireSwapchainImage");
            return 0;
        }
        sc.acquired = true;
        sc.acquiredIndex = index;

        XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
        waitInfo.timeout = XR_INFINITE_DURATION;
        t0 = Sys_DoubleTime();
        const XrResult waited = xrWaitSwapchainImage(sc.handle, &waitInfo);
        timeCall(CallWaitImage, t0);
        if(waited != XR_SUCCESS)
        {
            check(waited, "xrWaitSwapchainImage");
            // Keep the acquired image tracked and never access or release it
            // without a successful wait. A later acquire is refused until the
            // backend tears this swapchain down.
            fatalSwapchainError = true;
            return 0;
        }
        sc.waited = true;
        if(index >= sc.images.size() || !sc.images[index].image)
        {
            xrruntime::warn("OpenXR: acquired swapchain image index %u is outside the %u enumerated images\n",
                index, static_cast<unsigned>(sc.images.size()));
            releaseImage(sc); // the wait succeeded, so this acquired image can be returned safely
            fatalSwapchainError = true;
            return 0;
        }
        return sc.images[index].image;
    }

    bool releaseImage(Swapchain& sc)
    {
        if(sc.handle == XR_NULL_HANDLE || !sc.acquired || !sc.waited) return false;
        XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        const bool released = check(xrReleaseSwapchainImage(sc.handle, &releaseInfo), "xrReleaseSwapchainImage");
        if(released) sc.acquired = sc.waited = false;
        else
        {
            // The chain still owns an acquired image; never attempt another acquire
            // or reuse a cached held frame after an unsuccessful release.
            fatalSwapchainError = true;
            frameInvalid = true;
            haveLastViews = false;
        }
        return released;
    }

    [[nodiscard]] unsigned acquireSynthesisImage(Swapchain& sc)
    {
        if(sc.handle == XR_NULL_HANDLE || sc.acquired) return 0;
        XrSwapchainImageAcquireInfo acquire{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        uint32_t index = 0;
        double t0 = Sys_DoubleTime();
        const XrResult acquired = xrAcquireSwapchainImage(sc.handle, &acquire, &index);
        timeCall(CallAcquire, t0);
        if(acquired != XR_SUCCESS)
        {
            check(acquired, "xrAcquireSwapchainImage (synthesis)");
            synthesisAvailable = false;
            return 0;
        }
        sc.acquired = true;
        sc.acquiredIndex = index;
        XrSwapchainImageWaitInfo wait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
        wait.timeout = XR_INFINITE_DURATION;
        t0 = Sys_DoubleTime();
        const XrResult waited = xrWaitSwapchainImage(sc.handle, &wait);
        timeCall(CallWaitImage, t0);
        if(waited != XR_SUCCESS)
        {
            xrruntime::warn("OpenXR: synthesis image wait failed (%d); color-only until backend restart\n", waited);
            // An acquired image must not be released before a successful wait.
            // Its swapchain remains owned until stop() destroys it.
            synthesisAvailable = false;
            return 0;
        }
        sc.waited = true;
        if(index >= sc.images.size() || !sc.images[index].image)
        {
            releaseImage(sc);
            synthesisAvailable = false;
            return 0;
        }
        return sc.images[index].image;
    }

    static void destroySpace(XrSpace& space)
    {
        if(space != XR_NULL_HANDLE)
        {
            xrDestroySpace(space);
            space = XR_NULL_HANDLE;
        }
    }

    // Tracked hand -> physical side (0 left): HAND_MAIN is the right controller, left-handed too (vr_backend.hpp).
    [[nodiscard]] static int handSide(int hand)
    {
        return hand == HAND_MAIN ? 1 : 0;
    }

    [[nodiscard]] static Pose toPose(const XrPosef& p, bool valid)
    {
        Pose pose;
        pose.position = {p.position.x, p.position.y, p.position.z};
        pose.orientation = glm::quat{p.orientation.w, p.orientation.x, p.orientation.y, p.orientation.z};
        pose.valid = valid;
        return pose;
    }

    // The controller pose the old (OpenVR) engine used, SteamVR's "raw" device pose, from
    // OpenXR's grip pose: the weapon offsets and gun angles were tuned for it. Per controller,
    // the grip pose is the raw pose moved by T(offset) * R(xyz degrees): SteamVR's
    // "openxr_grip" rendermodel components, as collected by xrizer
    // (src/input/profiles/*.rs, offset_grip_pose). Vive wands and unknown controllers: none.
    enum class Controller
    {
        Other,
        Touch,
        Index
    };
    Controller controller[2]{Controller::Other, Controller::Other}; // per side (0 left)

    // The profile the runtime plays each hand's controller as (xrGetCurrentInteractionProfile): its bindings in
    // suggestBindings are the buttons the game gets. Looked up when the session starts running and at each
    // XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED (and when vr_debug_buttons is turned on); a change is logged in
    // qvr_openxr.txt and the console, and named in the crash report's VR line. VDXR may report Index controllers for
    // Quest ones ("Emulate Index controllers").
    char profileName[2][XR_MAX_PATH_LENGTH]{}; // per side (0 left); "" none yet
    bool limitedWarned{false};                  // the limited-controls warning given (once a start)
    char limitedNotice[320]{};                  // to centre-print once in a game (pollEvents)

    // Profiles suggestBindings has buttons for (the simple controller has only a trigger and a menu button).
    static constexpr const char* fullProfiles[] = {"/interaction_profiles/oculus/touch_controller",
        "/interaction_profiles/meta/touch_plus_controller", "/interaction_profiles/valve/index_controller",
        "/interaction_profiles/htc/vive_controller", "/interaction_profiles/microsoft/motion_controller",
        "/interaction_profiles/bytedance/pico_neo3_controller", "/interaction_profiles/bytedance/pico4_controller",
        "/interaction_profiles/htc/vive_cosmos_controller", "/interaction_profiles/htc/vive_focus3_controller",
        "/interaction_profiles/hp/mixed_reality_controller", "/interaction_profiles/samsung/odyssey_controller"};

    [[nodiscard]] static const char* shortProfile(const char* name)
    {
        const char* prefix = "/interaction_profiles/";
        return strncmp(name, prefix, strlen(prefix)) ? name : name + strlen(prefix);
    }

    // The crash report's VR line: the runtime and headset, the controllers' profiles once known, and the last step.
    void setCrashVr(const char* lastStep)
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

    void updateControllers()
    {
        bool changed = false;
        for(int side = 0; side < 2; side++)
        {
            const char* hand = side == 0 ? "left" : "right";
            XrInteractionProfileState state{XR_TYPE_INTERACTION_PROFILE_STATE};
            controller[side] = Controller::Other;
            char name[XR_MAX_PATH_LENGTH]{};
            const XrResult r = xrGetCurrentInteractionProfile(session, handPaths[side], &state);
            if(XR_SUCCEEDED(r) && state.interactionProfile != XR_NULL_PATH)
            {
                uint32_t len = 0;
                xrPathToString(instance, state.interactionProfile, sizeof(name), &len, name);
            }
            if(strcmp(name, profileName[side]) != 0)
            {
                q_strlcpy(profileName[side], name, sizeof(profileName[side]));
                changed = true;
                char line[XR_MAX_PATH_LENGTH + 96];
                q_snprintf(line, sizeof(line), "OpenXR: the %s hand's controller profile: %s%s\n", hand, name[0] ? name : "none",
                    XR_SUCCEEDED(r) ? "" : " (xrGetCurrentInteractionProfile failed)");
                Con_Printf("%s", line);
                xrruntime::logLine(va("%s %s", wallClock(), line));
            }
            if(vr_debug_buttons.value)
            {
                Con_Printf("VR buttons: the %s hand: %s%s (%s)\n", hand, name[0] ? "interaction profile " : "no interaction profile",
                    name, runtime);
            }
            if(!name[0])
            {
                continue;
            }

            if(!strcmp(name, "/interaction_profiles/oculus/touch_controller") ||
                !strcmp(name, "/interaction_profiles/meta/touch_plus_controller"))
            {
                controller[side] = Controller::Touch;
            }
            else if(!strcmp(name, "/interaction_profiles/bytedance/pico_neo3_controller") ||
                    !strcmp(name, "/interaction_profiles/bytedance/pico4_controller") ||
                    !strcmp(name, "/interaction_profiles/htc/vive_cosmos_controller") ||
                    !strcmp(name, "/interaction_profiles/htc/vive_focus3_controller"))
            {
                // Shaped as Touch controllers (a handle, a stick and two face buttons): Touch's legacy grip is the nearest
                // the released offsets have (ROUND22.md, "Menu on any controller").
                controller[side] = Controller::Touch;
            }
            else if(!strcmp(name, "/interaction_profiles/valve/index_controller"))
            {
                // Virtual Desktop's "Emulate Index controllers" reports Index controllers for what
                // are Quest controllers: VDXR's grip is Meta's Touch one whatever the profile.
                controller[side] = vdxr ? Controller::Touch : Controller::Index;
            }

            // A profile without the game's buttons (the simple controller: a trigger and a menu button; or one the game
            // has no bindings for): said once a start, in the console, qvr_openxr.txt and in the game.
            bool full = false;
            for(const char* known : fullProfiles)
            {
                full = full || !strcmp(name, known);
            }
            if(!full && !limitedWarned)
            {
                limitedWarned = true;
                const char* line = va("OpenXR: the runtime plays the %s controller as %s, which has few of the game's "
                                      "buttons (no grips, no A/B/X/Y): grabbing, climbing and jumping may not work. Its "
                                      "controller bindings for the game were turned down or it does not know the "
                                      "controllers; qvr_openxr.txt has the details\n",
                    hand, name);
                Con_Warning("%s", line);
                xrruntime::logLine(va("WARNING: %s", line)); // (xrruntime::warn's log is the start's only)
                q_snprintf(limitedNotice, sizeof(limitedNotice),
                    "Your controllers are played as \"%s\":\ncontrols may be limited (no grips or A/B/X/Y).\nSee qvr_openxr.txt "
                    "in the quakevr folder.",
                    shortProfile(name));
            }
        }
        if(changed)
        {
            setCrashVr(nullptr);
        }
    }

    [[nodiscard]] Pose toLegacyPose(const Pose& grip, int side) const
    {
        if(!grip.valid || controller[side] == Controller::Other)
        {
            return grip;
        }

        const GripInRaw g = legacyGripInRaw(controller[side] == Controller::Touch, side);
        const glm::vec3 offset = g.offset;
        const glm::quat r = g.turn;

        // raw = grip * (T(offset) R)^-1 = grip * R^-1 T(-offset)
        const glm::quat rInv = glm::inverse(r);
        Pose raw = grip;
        raw.orientation = grip.orientation * rInv;
        raw.position = grip.position + grip.orientation * (rInv * -offset);
        if(grip.velocityValid)
        {
            // With the runtime's angular velocity as it came: vr_angvel.cpp redoes this lever term once it has put that in
            // the tracking space (VirtualDesktopXR gives it in the controller's frame). Takes record it as it is here.
            raw.linearVelocity = grip.linearVelocity + glm::cross(grip.angularVelocity, raw.position - grip.position);
            raw.gripVelocity = grip.linearVelocity;
            raw.gripVelocityValid = true;
        }
        return raw;
    }

    [[nodiscard]] Pose locate(XrSpace space, XrTime time) const
    {
        XrSpaceVelocity velocity{XR_TYPE_SPACE_VELOCITY};
        XrSpaceLocation location{XR_TYPE_SPACE_LOCATION};
        location.next = &velocity;
        if(!XR_SUCCEEDED(xrLocateSpace(space, worldSpace, time, &location)))
        {
            return Pose{};
        }

        constexpr XrSpaceLocationFlags needed =
            XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
        Pose pose = toPose(location.pose, (location.locationFlags & needed) == needed);

        constexpr XrSpaceVelocityFlags velNeeded =
            XR_SPACE_VELOCITY_LINEAR_VALID_BIT | XR_SPACE_VELOCITY_ANGULAR_VALID_BIT;
        if(pose.valid && (velocity.velocityFlags & velNeeded) == velNeeded)
        {
            pose.linearVelocity = {velocity.linearVelocity.x, velocity.linearVelocity.y, velocity.linearVelocity.z};
            pose.angularVelocity = {velocity.angularVelocity.x, velocity.angularVelocity.y, velocity.angularVelocity.z};
            pose.velocityValid = true;
        }
        return pose;
    }

    // What a runtime that failed to start leaves behind (stop() destroys its handles).
    void resetRuntimeState()
    {
        systemId = XR_NULL_SYSTEM_ID;
        sessionState = XR_SESSION_STATE_UNKNOWN;
        stateTrailCount = 0;
        stateTrailText[0] = '\0';
        endReason = SessionEnd::Lost;
        visibilityMaskExtension = false;
        synthesisExtension = synthesisAvailable = false;
        lastProjectionTime = 0;
        frameControllerExtension = false;
        userPresenceExtension = supportsUserPresence = userPresenceKnown = false;
        isUserPresent = true;
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

    bool createInstance()
    {
        // Quest 3 controllers get their own profile with this extension (Touch otherwise).
#ifdef QVR_OPENXR_INTEROP
        za::Vector<const char*> extensions{interopMode ? XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME : XR_KHR_OPENGL_ENABLE_EXTENSION_NAME};
#else
        za::Vector<const char*> extensions{XR_KHR_OPENGL_ENABLE_EXTENSION_NAME};
#endif
        uint32_t available = 0;
        xrEnumerateInstanceExtensionProperties(nullptr, 0, &available, nullptr);
        za::Vector<XrExtensionProperties> extensionList(available, XrExtensionProperties{XR_TYPE_EXTENSION_PROPERTIES});
        xrEnumerateInstanceExtensionProperties(nullptr, available, &available, extensionList.data());
        za::String offered;
        bool frameSynthesisOffered = false;
        bool spaceWarpOffered = false;
#ifdef QVR_OPENXR_INTEROP
        bool vulkan2Offered = false;
#endif
        for(const XrExtensionProperties& p : extensionList)
        {
            offered += offered.empty() ? "" : " ";
            offered += p.extensionName;
            // Names rather than extension structs: native headers may predate frame synthesis.
            frameSynthesisOffered |= !strcmp(p.extensionName, "XR_EXT_frame_synthesis");
            spaceWarpOffered |= !strcmp(p.extensionName, "XR_FB_space_warp");
#ifdef QVR_OPENXR_INTEROP
            vulkan2Offered |= !strcmp(p.extensionName, XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME);
#endif
        }
        xrruntime::logLine(va("OpenXR: %u extensions offered: ", available));
        xrruntime::logLine(offered.cStr()); // (longer than va's buffer)
        xrruntime::logLine("\n");
        synthesisExtension = frameSynthesisOffered && vr_xr_frame_synthesis.value > 0.f;
#ifdef QVR_OPENXR_INTEROP
        if(interopMode)
        {
            if(!vulkan2Offered)
            {
                xrruntime::warn("OpenXR interop: runtime does not offer Vulkan2\n");
                return false;
            }
            xrruntime::logLine("OpenXR interop: Vulkan2 graphics selected; EXT synthesis remains separately opt-in\n");
        }
#endif
        if(synthesisExtension) extensions.pushBack(XR_EXT_FRAME_SYNTHESIS_EXTENSION_NAME);
        xrruntime::logLine(va("OpenXR: app frame synthesis requested=%d EXT_offered=%d FB_offered=%d EXT_enabled=%d\n",
            vr_xr_frame_synthesis.value > 0.f, frameSynthesisOffered, spaceWarpOffered, synthesisExtension));
        for(const XrExtensionProperties& p : extensionList)
        {
            if(!strcmp(p.extensionName, "XR_VALVE_frame_controller_interaction"))
            {
                extensions.pushBack("XR_VALVE_frame_controller_interaction");
                frameControllerExtension = true;
            }
            if(!strcmp(p.extensionName, XR_EXT_USER_PRESENCE_EXTENSION_NAME))
            {
                extensions.pushBack(XR_EXT_USER_PRESENCE_EXTENSION_NAME);
                userPresenceExtension = true;
            }
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
            // The lenses' hidden area (vr_visibility_mask).
            if(!strcmp(p.extensionName, XR_KHR_VISIBILITY_MASK_EXTENSION_NAME))
            {
                extensions.pushBack(XR_KHR_VISIBILITY_MASK_EXTENSION_NAME);
                visibilityMaskExtension = true;
            }
            // Diagnostics mode: the loader's and the runtime's own messages (openxr_debug.log).
            if(VR_DiagnosticsOn() && vr_diagnostics_xr.value != 0.f && !strcmp(p.extensionName, XR_EXT_DEBUG_UTILS_EXTENSION_NAME))
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
        messengerInfo.messageSeverities = XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
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
            // Diagnostics mode must not cost the VR start: again without the messenger and the validation layer.
            VR_DiagnosticsNote(va("xrCreateInstance failed (%d) with the debug messenger/validation layer (instance %p left): "
                                  "again without\n",
                static_cast<int>(created), static_cast<void*>(instance)));
            if(instance != XR_NULL_HANDLE)
            {
                xrDestroyInstance(instance); // (the loader's own failure after the runtime's instance was made)
                instance = XR_NULL_HANDLE;
            }
            xrruntime::warn("OpenXR: xrCreateInstance failed (%d) with diagnostics' debug messenger: again without it\n",
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
        if(XR_FAILED(created) && synthesisExtension)
        {
            xrruntime::warn("OpenXR: frame synthesis instance request failed (%d); retrying color-only\n", created);
            za::Vector<const char*> ordinary;
            for(const char* extension : extensions)
                if(strcmp(extension, XR_EXT_FRAME_SYNTHESIS_EXTENSION_NAME)) ordinary.pushBack(extension);
            synthesisExtension = false;
            info.enabledExtensionCount = static_cast<uint32_t>(ordinary.size());
            info.enabledExtensionNames = ordinary.data();
            created = xrCreateInstance(&info, &instance);
            enabled.clear();
            for(const char* extension : ordinary)
            {
                enabled += enabled.empty() ? "" : " ";
                enabled += extension;
            }
            xrruntime::logLine(va("OpenXR: extensions enabled after color-only retry: %s\n", enabled.cStr()));
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
        q_snprintf(crashBase, sizeof(crashBase), "OpenXR %s (%s); %s", runtime[0] ? runtime : "a runtime without a name",
            currentAttempt.label.cStr(), around.cStr());
        VR_SetCrashVr(crashBase);

        return true;
    }

    bool createSystem()
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
                SDL_Delay(250);
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
        XrSystemUserPresencePropertiesEXT presence{XR_TYPE_SYSTEM_USER_PRESENCE_PROPERTIES_EXT};
        if(userPresenceExtension)
        {
            system.next = &presence;
        }
        if(XR_SUCCEEDED(xrGetSystemProperties(instance, systemId, &system)))
        {
            supportsUserPresence = userPresenceExtension && presence.supportsUserPresence;
            xrruntime::note("OpenXR: user presence sensor %s\n", supportsUserPresence ? "supported" : "not supported");
            q_strlcpy(this->system, system.systemName, sizeof(this->system));
            maxImageWidth = system.graphicsProperties.maxSwapchainImageWidth;
            maxImageHeight = system.graphicsProperties.maxSwapchainImageHeight;
            const size_t len = strlen(crashBase);
            q_snprintf(crashBase + len, sizeof(crashBase) - len, "; headset \"%s\"", system.systemName);
            xrruntime::logLine(va("OpenXR: system \"%s\" (vendor 0x%x): at most %u layers, swapchain images up to %ux%u\n",
                system.systemName, system.vendorId, system.graphicsProperties.maxLayerCount,
                system.graphicsProperties.maxSwapchainImageWidth, system.graphicsProperties.maxSwapchainImageHeight));
        }

#ifdef QVR_OPENXR_INTEROP
        if(interopMode)
        {
            if(!vulkanBootstrap.initialize(instance, systemId, true))
            {
                xrruntime::warn("OpenXR interop: Vulkan bootstrap failed: %s\n", vulkanBootstrap.error());
                return false;
            }
            return true;
        }
#endif
        // Required before creating an OpenGL session.
        PFN_xrGetOpenGLGraphicsRequirementsKHR getRequirements = nullptr;
        xrGetInstanceProcAddr(instance, "xrGetOpenGLGraphicsRequirementsKHR",
            reinterpret_cast<PFN_xrVoidFunction*>(&getRequirements));
        XrGraphicsRequirementsOpenGLKHR requirements{XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR};
        return getRequirements &&
               check(getRequirements(instance, systemId, &requirements), "xrGetOpenGLGraphicsRequirementsKHR");
    }

    // Before a call in which the runtime makes GL calls of its own (xrCreateSwapchain, xrEnumerateSwapchainImages; the
    // session's binding, createSession): the game's context current on this thread, the very one bound to the session,
    // its DC still its window's. Logged with both handles; made current again when it isn't; false (the call not made,
    // the start fails) when that fails or the window is gone. (1.0.1: two runtimes, VDXR and SteamVR, died in the first
    // swapchain on one player's PC: a runtime's GL calls without the game's context read null function pointers.)
#ifndef _WIN32
    bool contextReady(const char*) // (the GLX context stays current on this thread for the session's life; no DC to restore)
    {
        return true;
    }
#else
    bool contextReady(const char* what)
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

    // What the GL driver offers a runtime that shares its D3D11 images with the game, logged before the session: VDXR
    // imports them with GL_EXT_memory_object_win32 (glCreateMemoryObjectsEXT, glImportMemoryWin32HandleEXT,
    // glTextureStorageMem2DEXT: seen in its 1.1.0 code), others with WGL_NV_DX_interop2.
    void logGlInterop()
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

    bool createSession()
    {
#ifdef QVR_OPENXR_INTEROP
        if(interopMode)
        {
            if(!vulkanBootstrap.createSession(session))
            {
                xrruntime::warn("OpenXR interop: Vulkan2 session failed: %s\n", vulkanBootstrap.error());
                return false;
            }
            getVisibilityMask = nullptr;
            if(visibilityMaskExtension)
                xrGetInstanceProcAddr(instance, "xrGetVisibilityMaskKHR", reinterpret_cast<PFN_xrVoidFunction*>(&getVisibilityMask));
            return true;
        }
#endif
#ifdef _WIN32
        XrGraphicsBindingOpenGLWin32KHR binding{XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR};
        binding.hDC = wglGetCurrentDC();
        binding.hGLRC = wglGetCurrentContext();
        glDC = binding.hDC;
        glRC = binding.hGLRC;
        logGlInterop();
        step("xrCreateSession (OpenGL: hDC %p, hGLRC %p)", static_cast<void*>(glDC), static_cast<void*>(glRC));
        // (SDL's idea of the current context: the same one, or the engine's context is not what is current here.)
        if(static_cast<void*>(SDL_GL_GetCurrentContext()) != static_cast<void*>(glRC))
        {
            xrruntime::warn("OpenXR: the current GL context %p is not SDL's (%p)\n", static_cast<void*>(glRC),
                static_cast<void*>(SDL_GL_GetCurrentContext()));
        }
        if(!contextReady("xrCreateSession"))
        {
            return false;
        }
#else
        XrGraphicsBindingOpenGLXlibKHR binding{XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR};
        binding.xDisplay = glXGetCurrentDisplay();
        binding.glxContext = glXGetCurrentContext();
        binding.glxDrawable = glXGetCurrentDrawable();
        if(!binding.xDisplay || !binding.glxContext || !binding.glxDrawable)
        {
            Con_Warning("OpenXR: no current Linux GLX context\n");
            return false;
        }
        int configId = 0, screen = 0, count = 0;
        glXQueryContext(binding.xDisplay, binding.glxContext, GLX_FBCONFIG_ID, &configId);
        glXQueryContext(binding.xDisplay, binding.glxContext, GLX_SCREEN, &screen);
        const int attributes[] = {GLX_FBCONFIG_ID, configId, None};
        GLXFBConfig* configs = glXChooseFBConfig(binding.xDisplay, screen, attributes, &count);
        if(!configs || count == 0)
        {
            if(configs) XFree(configs);
            Con_Warning("OpenXR: GLX framebuffer configuration not found\n");
            return false;
        }
        binding.glxFBConfig = configs[0];
        XVisualInfo* visual = glXGetVisualFromFBConfig(binding.xDisplay, configs[0]);
        XFree(configs);
        if(!visual)
        {
            Con_Warning("OpenXR: GLX visual not found\n");
            return false;
        }
        binding.visualid = static_cast<uint32_t>(visual->visualid);
        XFree(visual);
#endif

        XrSessionCreateInfo info{XR_TYPE_SESSION_CREATE_INFO};
        info.next = &binding;
        info.systemId = systemId;
        const XrInstance inst = instance;
        XrSession made = XR_NULL_HANDLE;
        const XrResult created = guarded("xrCreateSession", [inst, &info, &made] { return xrCreateSession(inst, &info, &made); });
        session = made;
        if(runtimeFaulted || !check(created, "xrCreateSession"))
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

    bool createSpaces()
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

    [[nodiscard]] XrPath path(const char* s) const
    {
        XrPath p = XR_NULL_PATH;
        xrStringToPath(instance, s, &p);
        return p;
    }

    [[nodiscard]] XrAction makeAction(XrActionType type, const char* name, const char* localized, bool perHand)
    {
        XrActionCreateInfo info{XR_TYPE_ACTION_CREATE_INFO};
        info.actionType = type;
        q_strlcpy(info.actionName, name, XR_MAX_ACTION_NAME_SIZE);
        q_strlcpy(info.localizedActionName, localized, XR_MAX_LOCALIZED_ACTION_NAME_SIZE);
        if(perHand)
        {
            info.countSubactionPaths = 2;
            info.subactionPaths = handPaths;
        }

        XrAction action = XR_NULL_HANDLE;
        check(xrCreateAction(actionSet, &info, &action), name);
        return action;
    }

    struct Binding
    {
        XrAction* action;
        const char* path;
    };

    [[nodiscard]] XrResult suggestList(XrPath profile, const za::Vector<XrActionSuggestedBinding>& list) const
    {
        XrInteractionProfileSuggestedBinding info{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
        info.interactionProfile = profile;
        info.suggestedBindings = list.data();
        info.countSuggestedBindings = static_cast<uint32_t>(list.size());
        return xrSuggestInteractionProfileBindings(instance, &info);
    }

    // One profile's bindings (1.0.1, ROUND22.md "Controls as if another controller"). A runtime turns a whole suggestion
    // down for a single path it lacks (XR_ERROR_PATH_UNSUPPORTED), and then plays the controllers as another profile the
    // game bound (Touch as a simple controller: no grips, no A/B). So when the whole is turned down, the bindings are
    // added again one at a time (the grip pose first), each one the runtime turns down dropped and named (qvr_openxr.txt
    // and the console), the rest kept: one bad path costs that path, never the profile. None taken: the runtime does not
    // have the profile at all (another vendor's, an extension's), which is expected and said in qvr_openxr.txt only.
    bool suggest(const char* profile, za::InitializerList<Binding> bindings)
    {
        const XrPath profilePath = path(profile);
        za::Vector<XrActionSuggestedBinding> all;
        for(const Binding& b : bindings)
        {
            all.pushBack({*b.action, path(b.path)});
        }
        const int count = static_cast<int>(all.size());
        const XrResult whole = suggestList(profilePath, all);
        if(XR_SUCCEEDED(whole))
        {
            xrruntime::logLine(va("OpenXR: bindings suggested for %s (%d)\n", profile, count));
            return true;
        }

        char result[XR_MAX_RESULT_STRING_SIZE] = "";
        xrResultToString(instance, whole, result);
        za::Vector<XrActionSuggestedBinding> kept;
        za::Vector<int> dropped; // indexes into bindings
        bool lastTurnedDown = false;
        for(int i = 0; i < count; i++)
        {
            kept.pushBack(all[i]);
            lastTurnedDown = !XR_SUCCEEDED(suggestList(profilePath, kept));
            if(lastTurnedDown)
            {
                kept.popBack();
                dropped.pushBack(i);
            }
        }
        if(kept.empty())
        {
            xrruntime::logLine(va("OpenXR: no bindings for %s: the runtime does not have it (%s)\n", profile, result));
            return false;
        }
        // (A turned-down call changes nothing, but a runtime might not hold to that: the kept ones once more.)
        if(lastTurnedDown && !XR_SUCCEEDED(suggestList(profilePath, kept)))
        {
            xrruntime::warn("OpenXR: %s: its bindings turned down (%s), and its %d accepted one by one too\n", profile,
                result, static_cast<int>(kept.size()));
            return false;
        }
        for(const int i : dropped)
        {
            xrruntime::warn("OpenXR: %s: %s turned down by the runtime (%s): not bound\n", profile, bindings.begin()[i].path,
                result);
        }
        xrruntime::logLine(va("OpenXR: bindings suggested for %s (%d of %d: %d turned down)\n", profile,
            static_cast<int>(kept.size()), count, static_cast<int>(dropped.size())));
        return true;
    }

    // The actions are the controllers' physical controls; the game assigns them through key
    // bindings. Controllers without some control (Vive wands, WMR) map the closest one.
    void suggestBindings()
    {
#define QVR_L(p) "/user/hand/left/" p
#define QVR_R(p) "/user/hand/right/" p
#define QVR_BOTH(action, p) {&action, QVR_L(p)}, {&action, QVR_R(p)}

        if(frameControllerExtension)
        {
            suggest("/interaction_profiles/valve/frame_controller_valve",
                {QVR_BOTH(gripPose, "input/grip/pose"), QVR_BOTH(triggerAction, "input/trigger/value"),
                    QVR_BOTH(gripAction, "input/squeeze/value"), QVR_BOTH(stickAction, "input/thumbstick"),
                    QVR_BOTH(stickClickAction, "input/thumbstick/click"),
                    {&primaryAction, QVR_L("input/dpad_down/click")}, {&primaryAction, QVR_R("input/a/click")},
                    {&secondaryAction, QVR_L("input/dpad_up/click")}, {&secondaryAction, QVR_R("input/b/click")},
                    {&menuAction, QVR_L("input/view/click")}, {&menuAction, QVR_R("input/menu/click")},
                    QVR_BOTH(hapticAction, "output/haptic"),
                    QVR_BOTH(triggerValueAction, "input/trigger/value"), QVR_BOTH(gripValueAction, "input/squeeze/value"),
                    QVR_BOTH(thumbTouchAction, "input/thumbstick/touch"),
                    {&thumbTouchAction, QVR_L("input/dpad_down/touch")}, {&thumbTouchAction, QVR_L("input/dpad_up/touch")},
                    {&thumbTouchAction, QVR_R("input/a/touch")}, {&thumbTouchAction, QVR_R("input/b/touch")},
                    QVR_BOTH(triggerTouchAction, "input/trigger/touch")});
        }

        suggest("/interaction_profiles/khr/simple_controller",
            {QVR_BOTH(gripPose, "input/grip/pose"), QVR_BOTH(triggerAction, "input/select/click"),
                QVR_BOTH(triggerValueAction, "input/select/click"),
                QVR_BOTH(menuAction, "input/menu/click"), QVR_BOTH(hapticAction, "output/haptic")});

        // Touch: the core profile's paths only (OpenXR 1.0, no extension's; VDXR 1.0.x and 1.1 take every one). Quest 3's
        // own profile (XR_META_touch_controller_plus: /interaction_profiles/meta/touch_plus_controller) is not suggested:
        // 1.0.0 asked for it under a wrong name (meta/touch_controller_plus, turned down), so every runtime has played
        // Quest 3 controllers as Touch ones, with these bindings; that stays.
        suggest("/interaction_profiles/oculus/touch_controller",
            {QVR_BOTH(gripPose, "input/grip/pose"), QVR_BOTH(triggerAction, "input/trigger/value"),
                QVR_BOTH(gripAction, "input/squeeze/value"), QVR_BOTH(stickAction, "input/thumbstick"),
                QVR_BOTH(stickClickAction, "input/thumbstick/click"),
                {&primaryAction, QVR_L("input/x/click")}, {&primaryAction, QVR_R("input/a/click")},
                {&secondaryAction, QVR_L("input/y/click")}, {&secondaryAction, QVR_R("input/b/click")},
                {&menuAction, QVR_L("input/menu/click")}, QVR_BOTH(hapticAction, "output/haptic"),
                QVR_BOTH(triggerValueAction, "input/trigger/value"),
                QVR_BOTH(gripValueAction, "input/squeeze/value"),
                QVR_BOTH(thumbTouchAction, "input/thumbstick/touch"),
                {&thumbTouchAction, QVR_L("input/x/touch")}, {&thumbTouchAction, QVR_L("input/y/touch")},
                {&thumbTouchAction, QVR_R("input/a/touch")}, {&thumbTouchAction, QVR_R("input/b/touch")},
                QVR_BOTH(triggerTouchAction, "input/trigger/touch")});

        // No menu button: the left B opens the menu.
        suggest("/interaction_profiles/valve/index_controller",
            {QVR_BOTH(gripPose, "input/grip/pose"), QVR_BOTH(triggerAction, "input/trigger/value"),
                QVR_BOTH(gripAction, "input/squeeze/value"), QVR_BOTH(stickAction, "input/thumbstick"),
                QVR_BOTH(stickClickAction, "input/thumbstick/click"), QVR_BOTH(primaryAction, "input/a/click"),
                {&secondaryAction, QVR_R("input/b/click")}, {&menuAction, QVR_L("input/b/click")},
                QVR_BOTH(hapticAction, "output/haptic"), QVR_BOTH(triggerValueAction, "input/trigger/value"),
                QVR_BOTH(gripValueAction, "input/squeeze/value"),
                QVR_BOTH(thumbTouchAction, "input/thumbstick/touch"), QVR_BOTH(thumbTouchAction, "input/a/touch"),
                QVR_BOTH(thumbTouchAction, "input/b/touch"), QVR_BOTH(thumbTouchAction, "input/trackpad/touch"),
                QVR_BOTH(triggerTouchAction, "input/trigger/touch")});

        // Trackpads as sticks, their clicks as the primary buttons; the right menu button is the
        // secondary button.
        suggest("/interaction_profiles/htc/vive_controller",
            {QVR_BOTH(gripPose, "input/grip/pose"), QVR_BOTH(triggerAction, "input/trigger/click"),
                QVR_BOTH(gripAction, "input/squeeze/click"), QVR_BOTH(stickAction, "input/trackpad"),
                QVR_BOTH(primaryAction, "input/trackpad/click"),
                {&secondaryAction, QVR_R("input/menu/click")}, {&menuAction, QVR_L("input/menu/click")},
                QVR_BOTH(hapticAction, "output/haptic"), QVR_BOTH(triggerValueAction, "input/trigger/value"),
                QVR_BOTH(gripValueAction, "input/squeeze/click"),
                QVR_BOTH(thumbTouchAction, "input/trackpad/touch")});

        // Trackpad clicks are the primary buttons, the right menu button the secondary one.
        suggest("/interaction_profiles/microsoft/motion_controller",
            {QVR_BOTH(gripPose, "input/grip/pose"), QVR_BOTH(triggerAction, "input/trigger/value"),
                QVR_BOTH(gripAction, "input/squeeze/click"), QVR_BOTH(stickAction, "input/thumbstick"),
                QVR_BOTH(stickClickAction, "input/thumbstick/click"),
                QVR_BOTH(primaryAction, "input/trackpad/click"),
                {&secondaryAction, QVR_R("input/menu/click")}, {&menuAction, QVR_L("input/menu/click")},
                QVR_BOTH(hapticAction, "output/haptic"), QVR_BOTH(triggerValueAction, "input/trigger/value"),
                QVR_BOTH(gripValueAction, "input/squeeze/click"),
                QVR_BOTH(thumbTouchAction, "input/trackpad/touch")});

        // The vendors' profiles, when the runtime offers them (vr_xr_vendor_profiles). Each has a menu button for the
        // menu: the left one (the right is the runtime's own system button on most).
        if(vendorProfile[VendorPico])
        {
            for(const char* pico : {"/interaction_profiles/bytedance/pico_neo3_controller",
                    "/interaction_profiles/bytedance/pico4_controller"})
            {
                suggest(pico,
                    {QVR_BOTH(gripPose, "input/grip/pose"), QVR_BOTH(triggerAction, "input/trigger/value"),
                        QVR_BOTH(gripAction, "input/squeeze/value"), QVR_BOTH(stickAction, "input/thumbstick"),
                        QVR_BOTH(stickClickAction, "input/thumbstick/click"),
                        {&primaryAction, QVR_L("input/x/click")}, {&primaryAction, QVR_R("input/a/click")},
                        {&secondaryAction, QVR_L("input/y/click")}, {&secondaryAction, QVR_R("input/b/click")},
                        {&menuAction, QVR_L("input/menu/click")}, QVR_BOTH(hapticAction, "output/haptic"),
                        QVR_BOTH(triggerValueAction, "input/trigger/value"),
                        QVR_BOTH(gripValueAction, "input/squeeze/value"),
                        QVR_BOTH(thumbTouchAction, "input/thumbstick/touch"),
                        {&thumbTouchAction, QVR_L("input/x/touch")}, {&thumbTouchAction, QVR_L("input/y/touch")},
                        {&thumbTouchAction, QVR_R("input/a/touch")}, {&thumbTouchAction, QVR_R("input/b/touch")},
                        QVR_BOTH(triggerTouchAction, "input/trigger/touch")});
            }
        }
        // The HTC ones' grips as clicks (the Focus 3's value came in a later revision: not asked).
        if(vendorProfile[VendorCosmos])
        {
            suggest("/interaction_profiles/htc/vive_cosmos_controller",
                {QVR_BOTH(gripPose, "input/grip/pose"), QVR_BOTH(triggerAction, "input/trigger/value"),
                    QVR_BOTH(gripAction, "input/squeeze/click"), QVR_BOTH(stickAction, "input/thumbstick"),
                    QVR_BOTH(stickClickAction, "input/thumbstick/click"),
                    {&primaryAction, QVR_L("input/x/click")}, {&primaryAction, QVR_R("input/a/click")},
                    {&secondaryAction, QVR_L("input/y/click")}, {&secondaryAction, QVR_R("input/b/click")},
                    {&menuAction, QVR_L("input/menu/click")}, QVR_BOTH(hapticAction, "output/haptic"),
                    QVR_BOTH(triggerValueAction, "input/trigger/value"), QVR_BOTH(gripValueAction, "input/squeeze/click"),
                    QVR_BOTH(thumbTouchAction, "input/thumbstick/touch")});
        }
        if(vendorProfile[VendorFocus3])
        {
            suggest("/interaction_profiles/htc/vive_focus3_controller",
                {QVR_BOTH(gripPose, "input/grip/pose"), QVR_BOTH(triggerAction, "input/trigger/value"),
                    QVR_BOTH(gripAction, "input/squeeze/click"), QVR_BOTH(stickAction, "input/thumbstick"),
                    QVR_BOTH(stickClickAction, "input/thumbstick/click"),
                    {&primaryAction, QVR_L("input/x/click")}, {&primaryAction, QVR_R("input/a/click")},
                    {&secondaryAction, QVR_L("input/y/click")}, {&secondaryAction, QVR_R("input/b/click")},
                    {&menuAction, QVR_L("input/menu/click")}, QVR_BOTH(hapticAction, "output/haptic"),
                    QVR_BOTH(triggerValueAction, "input/trigger/value"), QVR_BOTH(gripValueAction, "input/squeeze/click"),
                    QVR_BOTH(thumbTouchAction, "input/thumbstick/touch"), QVR_BOTH(thumbTouchAction, "input/thumbrest/touch"),
                    QVR_BOTH(triggerTouchAction, "input/trigger/touch")});
        }
        // The Reverb G2: A/B and X/Y as on Touch (the WMR profile has none), a menu button on each hand.
        if(vendorProfile[VendorHp])
        {
            suggest("/interaction_profiles/hp/mixed_reality_controller",
                {QVR_BOTH(gripPose, "input/grip/pose"), QVR_BOTH(triggerAction, "input/trigger/value"),
                    QVR_BOTH(gripAction, "input/squeeze/value"), QVR_BOTH(stickAction, "input/thumbstick"),
                    QVR_BOTH(stickClickAction, "input/thumbstick/click"),
                    {&primaryAction, QVR_L("input/x/click")}, {&primaryAction, QVR_R("input/a/click")},
                    {&secondaryAction, QVR_L("input/y/click")}, {&secondaryAction, QVR_R("input/b/click")},
                    QVR_BOTH(menuAction, "input/menu/click"), QVR_BOTH(hapticAction, "output/haptic"),
                    QVR_BOTH(triggerValueAction, "input/trigger/value"), QVR_BOTH(gripValueAction, "input/squeeze/value")});
        }
        // As the WMR controllers (the same controls).
        if(vendorProfile[VendorOdyssey])
        {
            suggest("/interaction_profiles/samsung/odyssey_controller",
                {QVR_BOTH(gripPose, "input/grip/pose"), QVR_BOTH(triggerAction, "input/trigger/value"),
                    QVR_BOTH(gripAction, "input/squeeze/click"), QVR_BOTH(stickAction, "input/thumbstick"),
                    QVR_BOTH(stickClickAction, "input/thumbstick/click"),
                    QVR_BOTH(primaryAction, "input/trackpad/click"),
                    {&secondaryAction, QVR_R("input/menu/click")}, {&menuAction, QVR_L("input/menu/click")},
                    QVR_BOTH(hapticAction, "output/haptic"), QVR_BOTH(triggerValueAction, "input/trigger/value"),
                    QVR_BOTH(gripValueAction, "input/squeeze/click"),
                    QVR_BOTH(thumbTouchAction, "input/trackpad/touch")});
        }

#undef QVR_BOTH
#undef QVR_L
#undef QVR_R
    }

    bool createActions()
    {
        XrActionSetCreateInfo setInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
        q_strlcpy(setInfo.actionSetName, "gameplay", XR_MAX_ACTION_SET_NAME_SIZE);
        q_strlcpy(setInfo.localizedActionSetName, "Gameplay", XR_MAX_LOCALIZED_ACTION_SET_NAME_SIZE);
        if(!check(xrCreateActionSet(instance, &setInfo, &actionSet), "xrCreateActionSet"))
        {
            return false;
        }

        handPaths[0] = path("/user/hand/left");
        handPaths[1] = path("/user/hand/right");

        gripPose = makeAction(XR_ACTION_TYPE_POSE_INPUT, "grip_pose", "Hand pose", true);
        triggerAction = makeAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "trigger", "Trigger", true);
        gripAction = makeAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "grip", "Grip", true);
        primaryAction = makeAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "primary", "Primary button (A/X)", true);
        secondaryAction = makeAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "secondary", "Secondary button (B/Y)", true);
        stickClickAction = makeAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "stick_click", "Thumbstick click", true);
        menuAction = makeAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "menu", "Menu", true);
        stickAction = makeAction(XR_ACTION_TYPE_VECTOR2F_INPUT, "stick", "Thumbstick", true);
        triggerValueAction = makeAction(XR_ACTION_TYPE_FLOAT_INPUT, "trigger_curl", "Index finger (trigger)", true);
        gripValueAction = makeAction(XR_ACTION_TYPE_FLOAT_INPUT, "grip_curl", "Other fingers (grip)", true);
        thumbTouchAction = makeAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "thumb_touch", "Thumb resting", true);
        triggerTouchAction = makeAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "trigger_touch", "Index finger on the trigger", true);
        hapticAction = makeAction(XR_ACTION_TYPE_VIBRATION_OUTPUT, "haptic", "Haptics", true);

        suggestBindings();

        XrSessionActionSetsAttachInfo attachInfo{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
        attachInfo.countActionSets = 1;
        attachInfo.actionSets = &actionSet;
        if(!check(xrAttachSessionActionSets(session, &attachInfo), "xrAttachSessionActionSets"))
        {
            return false;
        }

        for(int side = 0; side < 2; side++)
        {
            XrActionSpaceCreateInfo spaceInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO};
            spaceInfo.action = gripPose;
            spaceInfo.subactionPath = handPaths[side];
            spaceInfo.poseInActionSpace.orientation.w = 1.f;
            if(!check(xrCreateActionSpace(session, &spaceInfo, &handSpaces[side]), "xrCreateActionSpace"))
            {
                return false;
            }
        }

        return true;
    }

    [[nodiscard]] bool boolState(XrAction action, XrPath subaction) const
    {
        XrActionStateGetInfo info{XR_TYPE_ACTION_STATE_GET_INFO};
        info.action = action;
        info.subactionPath = subaction;
        XrActionStateBoolean state{XR_TYPE_ACTION_STATE_BOOLEAN};
        return XR_SUCCEEDED(xrGetActionStateBoolean(session, &info, &state)) && state.isActive &&
               state.currentState;
    }

    [[nodiscard]] float floatState(XrAction action, XrPath subaction) const
    {
        XrActionStateGetInfo info{XR_TYPE_ACTION_STATE_GET_INFO};
        info.action = action;
        info.subactionPath = subaction;
        XrActionStateFloat state{XR_TYPE_ACTION_STATE_FLOAT};
        if(!XR_SUCCEEDED(xrGetActionStateFloat(session, &info, &state)) || !state.isActive)
        {
            return 0.f;
        }
        return state.currentState;
    }

    [[nodiscard]] glm::vec2 vec2State(XrAction action, XrPath subaction) const
    {
        XrActionStateGetInfo info{XR_TYPE_ACTION_STATE_GET_INFO};
        info.action = action;
        info.subactionPath = subaction;
        XrActionStateVector2f state{XR_TYPE_ACTION_STATE_VECTOR2F};
        if(!XR_SUCCEEDED(xrGetActionStateVector2f(session, &info, &state)) || !state.isActive)
        {
            return glm::vec2{0.f};
        }
        return {state.currentState.x, state.currentState.y};
    }

    void readInput(InputState& in) const
    {
        for(int h = 0; h < HAND_COUNT; h++)
        {
            const XrPath side = handPaths[handSide(h)];
            HandInput& hand = in.hands[h];
            hand.trigger = boolState(triggerAction, side);
            hand.grip = boolState(gripAction, side);
            hand.primary = boolState(primaryAction, side);
            hand.secondary = boolState(secondaryAction, side);
            hand.stickClick = boolState(stickClickAction, side);
            hand.menu = boolState(menuAction, side);
            hand.stick = vec2State(stickAction, side);
            hand.triggerValue = floatState(triggerValueAction, side);
            hand.gripValue = floatState(gripValueAction, side);
            hand.thumbTouch = boolState(thumbTouchAction, side);
            hand.triggerTouch = boolState(triggerTouchAction, side);
        }
    }

public:
    void haptic(int hand, float seconds, float frequency, float amplitude) override
    {
        // Every vibration times Vibration Strength (vr_haptics_strength; 0: none).
        const float strength = CLAMP(0.f, vr_haptics_strength.value, 2.f);
        if(!sessionRunning || hapticAction == XR_NULL_HANDLE || strength <= 0.f)
        {
            return;
        }
        amplitude *= strength;

        XrHapticVibration vibration{XR_TYPE_HAPTIC_VIBRATION};
        vibration.duration = seconds > 0.f ? static_cast<XrDuration>(seconds * 1e9) : XR_MIN_HAPTIC_DURATION;
        vibration.frequency = frequency > 0.f ? frequency : XR_FREQUENCY_UNSPECIFIED;
        vibration.amplitude = CLAMP(0.f, amplitude, 1.f);

        XrHapticActionInfo info{XR_TYPE_HAPTIC_ACTION_INFO};
        info.action = hapticAction;
        info.subactionPath = handPaths[handSide(hand)];
        xrApplyHapticFeedback(session, &info, reinterpret_cast<const XrHapticBaseHeader*>(&vibration));
    }

private:
    bool createSwapchains()
    {
        for(int eye = 0; eye < 2; ++eye)
        {
            synthesisConfig[eye] = {};
            synthesisConfig[eye].type = XR_TYPE_FRAME_SYNTHESIS_CONFIG_VIEW_EXT;
            configViews[eye].next = synthesisExtension ? &synthesisConfig[eye] : nullptr;
        }
        uint32_t viewCount = 0;
#ifdef QVR_OPENXR_INTEROP
        if(interopMode)
        {
            if(xrEnumerateViewConfigurationViews(instance, systemId,
                XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &viewCount, nullptr) != XR_SUCCESS) return false;
        }
        else
#endif
        xrEnumerateViewConfigurationViews(instance, systemId,
            XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &viewCount, nullptr);
        if(viewCount != 2)
        {
            Con_Warning("OpenXR: expected 2 views, got %u\n", viewCount);
            return false;
        }

        XrResult configuration = xrEnumerateViewConfigurationViews(instance, systemId,
            XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 2, &viewCount, configViews);
        if(XR_FAILED(configuration) && synthesisExtension)
        {
            xrruntime::warn("OpenXR: synthesis view configuration failed (%d); using color-only\n", configuration);
            synthesisExtension = false; // enabled instance extension is left unused
            for(auto& view : configViews) view.next = nullptr;
            configuration = xrEnumerateViewConfigurationViews(instance, systemId,
                XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 2, &viewCount, configViews);
        }
        if(!check(configuration, "xrEnumerateViewConfigurationViews")) return false;
#ifdef QVR_OPENXR_INTEROP
        if(interopMode && (configuration != XR_SUCCESS || viewCount != 2)) return false;
#endif
        Con_Printf("OpenXR: recommended %ux%u per eye, largest %ux%u\n", configViews[0].recommendedImageRectWidth,
            configViews[0].recommendedImageRectHeight, configViews[0].maxImageRectWidth, configViews[0].maxImageRectHeight);
        xrruntime::logLine(va("OpenXR: eyes: recommended %ux%u (%.1f Mpx), largest %ux%u, %u samples recommended\n",
            configViews[0].recommendedImageRectWidth, configViews[0].recommendedImageRectHeight,
            configViews[0].recommendedImageRectWidth * configViews[0].recommendedImageRectHeight * 1e-6,
            configViews[0].maxImageRectWidth, configViews[0].maxImageRectHeight, configViews[0].recommendedSwapchainSampleCount));

#ifdef QVR_OPENXR_INTEROP
        if(interopMode)
        {
            uint32_t count = 0;
            if(xrEnumerateSwapchainFormats(session, 0, &count, nullptr) != XR_SUCCESS || !count) return false;
            za::Vector<int64_t> formats(count);
            const uint32_t capacity = count;
            if(xrEnumerateSwapchainFormats(session, capacity, &count, formats.data()) != XR_SUCCESS || count != capacity)
                return false;
            bool srgb = false, unorm = false, motion = false, depth = false;
            for(int64_t format : formats)
            {
                xrruntime::logLine(va("OpenXR interop: offered VkFormat=%lld\n", static_cast<long long>(format)));
                srgb |= format == VK_FORMAT_R8G8B8A8_SRGB;
                unorm |= format == VK_FORMAT_R8G8B8A8_UNORM;
                motion |= format == VK_FORMAT_R16G16B16A16_SFLOAT;
                depth |= format == VK_FORMAT_D32_SFLOAT;
            }
            if(!srgb && !unorm)
            {
                xrruntime::warn("OpenXR interop: no compatible RGBA8 SRGB/UNORM native color format\n");
                return false;
            }
            colorFormat = srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
            if(!srgb) Con_Warning("OpenXR interop: UNORM color fallback; gamma-encoded colors may look brighter\n");
            synthesisAvailable = synthesisSrgbMotion = false;
            xrruntime::logLine(va("OpenXR interop: native color VkFormat=%lld; GL_RGBA8 staging\n",
                static_cast<long long>(colorFormat)));
            if(!createEyeSwapchains()) return false;
            bool extents = true;
            for(const auto& config : synthesisConfig)
                extents &= config.recommendedMotionVectorImageRectWidth > 0 && config.recommendedMotionVectorImageRectHeight > 0;
            if(synthesisExtension && !extents)
                xrruntime::warn("OpenXR interop: EXT returned an empty recommended motion extent; using color-only\n");
            if(synthesisExtension && motion && depth && extents)
            {
                const float scale = CLAMP(0.125f, vr_xr_frame_synthesis_scale.value, 1.f);
                for(int eye = 0; eye < 2; ++eye)
                {
                    const auto& config = synthesisConfig[eye];
                    // EXT supplies paired motion/depth extents; never substitute
                    // the color size when the extension contract is absent.
                    const int w = za::max(32, static_cast<int>(config.recommendedMotionVectorImageRectWidth * scale));
                    const int h = za::max(32, static_cast<int>(config.recommendedMotionVectorImageRectHeight * scale));
                    if(!createSwapchain(synthesisMotion[eye], w, h,
                        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT,
                        "xrCreateSwapchain (Vulkan synthesis motion)", VK_FORMAT_R16G16B16A16_SFLOAT) ||
                       !createSwapchain(synthesisDepth[eye], w, h,
                        XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT,
                        "xrCreateSwapchain (Vulkan synthesis depth)", VK_FORMAT_D32_SFLOAT)) return false;
                }
                synthesisAvailable = true;
            }
            xrruntime::logLine(va("OpenXR interop: synthesis targets=%d EXT=%d RGBA16F=%d D32=%d; "
                "fresh stereo producer exports required; compositor interpolation unverified\n",
                synthesisAvailable, synthesisExtension, motion, depth));
            return true;
        }
#endif
        // Quake renders gamma-encoded colours: an sRGB swapchain written without sRGB
        // conversion hands them to the compositor unchanged.
        if(configViews[0].recommendedImageRectWidth == 0 || configViews[0].recommendedImageRectHeight == 0)
        {
            xrruntime::warn("OpenXR: the runtime recommends %ux%u eye images\n", configViews[0].recommendedImageRectWidth,
                configViews[0].recommendedImageRectHeight);
            return false;
        }
        step("xrEnumerateSwapchainFormats");
        uint32_t formatCount = 0;
        if(!check(xrEnumerateSwapchainFormats(session, 0, &formatCount, nullptr), "xrEnumerateSwapchainFormats"))
        {
            return false;
        }
        za::Vector<int64_t> formats(formatCount);
        if(formatCount &&
            !check(xrEnumerateSwapchainFormats(session, formatCount, &formatCount, formats.data()), "xrEnumerateSwapchainFormats"))
        {
            return false;
        }
        formats.resize(formatCount);
        offeredFormats = formats;

        int64_t format = formats.empty() ? GL_RGBA8 : formats[0];
        za::String offered;
        for(int64_t f : formats)
        {
            offered += va("%s%s", offered.empty() ? "" : " ", glFormatName(f));
        }
        xrruntime::logLine(va("OpenXR: swapchain formats offered (the runtime's order of preference): %s\n", offered.cStr()));
        for(int64_t f : formats)
        {
            if(f == GL_SRGB8_ALPHA8)
            {
                format = f;
                break;
            }
            if(f == GL_RGBA8)
            {
                format = f;
            }
        }

        colorFormat = format;
        // Once: what the eyes are written into. Anything but sRGB makes the compositor take the colours for
        // linear ones, and the image looks brighter and washed out.
        const char* formatName = glFormatName(format);
        xrruntime::logLine(va("OpenXR: swapchain format chosen: %s (the game's colours are gamma-encoded: sRGB, which the "
                              "compositor reads without a conversion of the game's own)\n",
            formatName));
        if(!formatLogged)
        {
            formatLogged = true;
            Con_Printf("OpenXR: swapchain format %s (0x%llx)\n", formatName, static_cast<unsigned long long>(format));
            if(format != GL_SRGB8_ALPHA8 && format != 0x8C41)
            {
                Con_Warning("OpenXR: the swapchain is not sRGB: the headset will show the game brighter and paler than it is\n");
            }
        }
        if(!createEyeSwapchains()) return false;
        if(synthesisExtension)
        {
            int64_t motion = 0, depth = 0;
            synthesisSrgbMotion = false;
            for(int64_t f : formats)
            {
                if(f == 0x881A) motion = f; // GL_RGBA16F: signed, linear RG motion.
                else if(!motion && f == 0x822F) motion = f; // GL_RG16F.
                else if(!motion && (f == 0x881B || f == 0x8230 || f == 0x8814)) motion = f;
                if(f == 0x8CAC) depth = f; // GL_DEPTH_COMPONENT32F.
                else if(!depth && (f == 0x81A6 || f == 0x81A5)) depth = f;
            }
            if(!motion && vr_xr_frame_synthesis_srgb_motion.value > 0.f)
            {
                for(int64_t f : formats)
                {
                    if(f == GL_SRGB8_ALPHA8)
                    {
                        motion = f;
                        synthesisSrgbMotion = true;
                        break;
                    }
                }
            }
            if(motion && depth)
            {
                synthesisAvailable = true;
                const float scale = CLAMP(0.125f, vr_xr_frame_synthesis_scale.value, 1.f);
                for(int eye = 0; eye < 2; ++eye)
                {
                    const uint32_t rw = synthesisConfig[eye].recommendedMotionVectorImageRectWidth;
                    const uint32_t rh = synthesisConfig[eye].recommendedMotionVectorImageRectHeight;
                    const int w = za::max(32, static_cast<int>((rw ? rw : swapchains[eye].width) * scale));
                    const int h = za::max(32, static_cast<int>((rh ? rh : swapchains[eye].height) * scale));
                    if(!createSwapchain(synthesisMotion[eye], w, h,
                        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT,
                        "xrCreateSwapchain (synthesis motion)", motion) ||
                       !createSwapchain(synthesisDepth[eye], w, h,
                        XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT,
                        "xrCreateSwapchain (synthesis depth)", depth))
                    {
                        synthesisAvailable = false;
                        break;
                    }
                    if(synthesisMotion[eye].images.empty() || synthesisDepth[eye].images.empty())
                    {
                        synthesisAvailable = false;
                        break;
                    }
                }
            }
            if(!motion || !depth)
                xrruntime::logLine("OpenXR: synthesis needs a supported motion format and a depth format; try vr_xr_frame_synthesis_srgb_motion 1 only for the experimental sRGB8 path\n");
            else if(synthesisSrgbMotion)
                xrruntime::logLine(va("OpenXR: experimental sRGB8 motion encoding enabled (range %.4f NDC, quantized; motion beyond range clips)\n",
                    CLAMP(0.0625f, vr_xr_frame_synthesis_motion_range.value, 0.5f)));
            if(!synthesisAvailable)
            {
                for(Swapchain* sc : {&synthesisMotion[0], &synthesisMotion[1], &synthesisDepth[0], &synthesisDepth[1]})
                {
                    if(sc->handle != XR_NULL_HANDLE) xrDestroySwapchain(sc->handle);
                    *sc = {};
                }
            }
            xrruntime::logLine(va("OpenXR: frame synthesis images %s (motion format=%s, depth format=%s); color-only fallback when unavailable\n",
                synthesisAvailable ? "available" : "unavailable", glFormatName(motion), glFormatName(depth)));
        }
        return true;
    }

    // The eyes' swapchains, at the recommended size for the whole session: vr_render_scale changes
    // the size the eyes are rendered at, resampled into these (vr_stereo.cpp). Not recreated at
    // another size: SteamVR's OpenGL path keeps copying into textures of the first eye images'
    // size, so larger new images were shown cropped (a corner, magnified: a stretched, wrong
    // projection) and smaller ones not at all (a GL_INVALID_VALUE copy).
    // vr_xr_eye_scale: smaller than recommended (each side; within 64 and the largest), at the start only.
    bool createEyeSwapchains()
    {
        const float scale = CLAMP(0.5f, vr_xr_eye_scale.value, 1.f);
        const auto scaled = [scale](uint32_t recommended, uint32_t largest)
        {
            const int32_t size = static_cast<int32_t>(static_cast<float>(recommended) * scale + 0.5f);
            return za::min(za::max(size, int32_t{64}), static_cast<int32_t>(za::max(largest, recommended)));
        };
        const int32_t w = scaled(configViews[0].recommendedImageRectWidth, configViews[0].maxImageRectWidth);
        const int32_t h = scaled(configViews[0].recommendedImageRectHeight, configViews[0].maxImageRectHeight);
        haveLastViews = holding = false;
        if(scale != 1.f)
        {
            xrruntime::note("OpenXR: eye images %dx%d: %.2f of the runtime's recommended %ux%u (vr_xr_eye_scale)\n", w, h,
                scale, configViews[0].recommendedImageRectWidth, configViews[0].recommendedImageRectHeight);
        }
        const auto destroyBoth = [this]
        {
            for(Swapchain& sc : swapchains)
            {
                if(sc.handle != XR_NULL_HANDLE)
                {
#ifdef QVR_OPENXR_INTEROP
                    if(interopMode)
                    {
                        if(sc.acquired || !bridgeFor(sc)->shutdown())
                        {
                            blockInteropTeardown("eye recreation requires settled image and drained bridge");
                            return false;
                        }
                        if(xrDestroySwapchain(sc.handle) != XR_SUCCESS)
                        {
                            blockInteropTeardown("eye swapchain destruction failed");
                            return false;
                        }
                    }
                    else
#endif
                    {
                        const XrSwapchain handle = sc.handle;
                        guarded("xrDestroySwapchain", [handle] { return xrDestroySwapchain(handle); });
                    }
                    sc = Swapchain{};
                }
            }
            return true;
        };
        const auto createBoth = [this, w, h, &destroyBoth]
        {
            if(!destroyBoth())
            {
                return false;
            }
            return createSwapchain(swapchains[0], w, h, XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT, "xrCreateSwapchain (left eye)") &&
                   createSwapchain(swapchains[1], w, h, XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT, "xrCreateSwapchain (right eye)");
        };
        if(!createBoth())
        {
            // Once more in plain GL_RGBA8 (the colours paler: not sRGB) when the runtime offers it, after a failure or a
            // caught crash: the fresh swapchains are new objects in the runtime.
            bool rgba8 = false;
            for(int64_t f : offeredFormats)
            {
                rgba8 = rgba8 || f == GL_RGBA8;
            }
            if(colorFormat == GL_RGBA8 || !rgba8)
            {
                return false;
            }
            xrruntime::warn("OpenXR: the eye swapchains failed in %s%s: once more in GL_RGBA8 (the headset's colours paler)\n",
                glFormatName(colorFormat), runtimeFaulted ? " (the runtime crashed, caught)" : "");
            const bool faulted = runtimeFaulted;
            const int64_t firstFormat = colorFormat;
            runtimeFaulted = false;
            colorFormat = GL_RGBA8;
            if(!createBoth())
            {
                runtimeFaulted = runtimeFaulted || faulted;
                return false;
            }
            Con_Warning("OpenXR: the eye swapchains are GL_RGBA8 (%s failed%s): the headset shows the game paler than it is\n",
                glFormatName(firstFormat), faulted ? ": the runtime crashed in it, caught" : "");
        }
        Con_Printf("OpenXR: %dx%d per eye\n", w, h);
        xrruntime::logLine(va("OpenXR: eye images %dx%d (%.1f Mpx; vr_xr_eye_scale %g), rendered at vr_render_scale %g, "
                              "vr_xr_late_acquire %g, vr_xr_unfocused %g\n",
            w, h, w * h * 1e-6, scale, vr_render_scale.value, vr_xr_late_acquire.value, vr_xr_unfocused.value));
        return true;
    }

    void beginSession()
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

    double nextProfileCheck{0.0}; // pollEvents: a hand without a profile looked up again every 2 s (an event missed)

    // Each frame: the hands' profiles if a runtime has not said (a hand with none, every 2 s); the limited-controls
    // warning centre-printed once a game is on (updateControllers).
    void checkControllers()
    {
        const double now = Sys_DoubleTime();
        if(sessionRunning && (!profileName[0][0] || !profileName[1][0]) && now >= nextProfileCheck)
        {
            nextProfileCheck = now + 2.0;
            updateControllers();
        }
        if(limitedNotice[0] && cls.signon == SIGNONS && key_dest == key_game)
        {
            SCR_CenterPrint(limitedNotice);
            limitedNotice[0] = '\0';
        }
    }

    // Returns false if the session is lost or the application should exit VR.
    bool pollEvents()
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
                case XR_TYPE_EVENT_DATA_USER_PRESENCE_CHANGED_EXT:
                {
                    const auto& changed = reinterpret_cast<const XrEventDataUserPresenceChangedEXT&>(event);
                    if(changed.session == session && supportsUserPresence)
                    {
                        userPresenceKnown = true;
                        isUserPresent = changed.isUserPresent != XR_FALSE;
                        xrruntime::note("OpenXR: user presence %s\n", isUserPresent ? "present" : "absent");
                    }
                    break;
                }
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
                    xrruntime::logLine(va("%s session %s%s\n", wallClock(), stateName(sessionState),
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
};

} // namespace

za::UniquePtr<Backend> makeOpenXrBackend()
{
#ifdef QVR_OPENXR_INTEROP
    if(interopParentsRetained)
    {
        Con_Warning("OpenXR interop: backend recreation blocked while native parents remain retained\n");
        return nullptr;
    }
#endif
    return za::makeUnique<OpenXrBackend>();
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
