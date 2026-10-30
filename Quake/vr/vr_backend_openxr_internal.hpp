#pragma once

// vr_backend_openxr_internal.hpp -- the OpenXR backend's class (vr_backend_openxr.cpp: see there) and the helpers its
// parts share. Internal: included by vr_backend_openxr.cpp, vr_backend_openxr_actions.cpp, vr_backend_openxr_frame.cpp
// and vr_backend_openxr_panel.cpp alone.

#include "vr_backend.hpp"
#include "vr_cvars.hpp"

#ifdef QVR_HAVE_OPENXR

#include "vr_bodytrack_xr.hpp"
#include "vr_engine.hpp"
#include "vr_input.hpp"
#include "vr_main.hpp"
#include "vr_profile.hpp"
#include "vr_tracy.h"
#include "vr_xr_runtime.hpp"
#include "vr_xr_stall.hpp"

#include "Zancle/Base/InitializerList.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/Vocabulary/UniquePtr.hpp"

#if defined(SDL_FRAMEWORK) || defined(NO_SDL_CONFIG)
#include <SDL2/SDL.h>
#else
#include "SDL.h"
#endif

#ifdef _WIN32
#include <windows.h>
#include <unknwn.h> // IUnknown, which openxr_platform.h needs and lean Windows headers omit

#define XR_USE_PLATFORM_WIN32
#else
// Linux: the GLX binding (XrGraphicsBindingOpenGLXlibKHR) or the EGL one (XrGraphicsBindingEGLMNDX). Their X11, GLX
// and EGL types declared as Xlib.h, glx.h and egl.h have them (without those headers' macros here:
// vr_xr_linuxgl.cpp has them).
#include "vr_xr_linuxgl.hpp"
#include <pthread.h>
#include <unistd.h>
typedef unsigned long XID;
typedef struct _XDisplay Display;
typedef XID GLXDrawable;
typedef struct __GLXcontextRec* GLXContext;
typedef struct __GLXFBConfigRec* GLXFBConfig;
typedef void* EGLDisplay;
typedef void* EGLConfig;
typedef void* EGLContext;
#define XR_USE_PLATFORM_XLIB
#define XR_USE_PLATFORM_EGL
#endif
#define XR_USE_GRAPHICS_API_OPENGL
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#ifndef GL_SRGB8_ALPHA8
#define GL_SRGB8_ALPHA8 0x8C43
#endif

#include <stdarg.h>
#include <string.h>

namespace qvr::oxr
{

// Reads away the GL errors left pending (at most 16): the runtime's own GL calls check glGetError.
inline void drainGlErrors()
{
    for(int i = 0; i < 16 && glGetError() != GL_NO_ERROR; i++)
    {
    }
}

// A crash inside the runtime, caught where the game calls it (vr_xr_guard 1): 1.0.0 died in VDXR 1.1.0's
// xrEnumerateSwapchainImages, a null read (virtualdesktop-openxr.dll+0x9a668): its OpenGL path takes the swapchain's
// GL images as made once its D3D11 images are, and reads the first of none. The D3D11 images are fetched before the
// image structs' type is checked, so an earlier call that failed that check (an API layer's, enumerating the game's
// swapchain as D3D11 or Vulkan ones) leaves the swapchain in that state (ROUND22.md, "VDXR swapchain crash").
#ifndef _WIN32
// Linux: no structured exceptions; a crash inside the runtime is the crash handler's (vr_crash.cpp): never caught here.
struct Fault
{
    unsigned code{0};
};

template <typename Call>
XrResult guardedCall(Call& call, Fault&)
{
    return call();
}

inline void describeFault(const Fault& fault, char* out, size_t size)
{
    q_snprintf(out, size, "fault %u", fault.code);
}
#else
struct Fault
{
    DWORD code{0};
    void* address{nullptr};
    ULONG_PTR access{0}; // 0 read, 1 write, 8 execute
    ULONG_PTR target{0}; // the address read or written
};

inline int faultFilter(const EXCEPTION_POINTERS* ep, Fault& fault)
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
inline void describeFault(const Fault& fault, char* out, size_t size)
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
#endif // _WIN32

class OpenXrBackend final : public Backend
{
public:
    ~OpenXrBackend() override;

    [[nodiscard]] const char* name() const override;

    [[nodiscard]] const char* runtimeName() const override;

    // The runtime vr_xr_runtime chooses (vr_xr_runtime.hpp); with Auto, while one fails (no headset, not running), the
    // next: the loader unloads a runtime with its last instance and reads XR_RUNTIME_JSON again at the next.
    [[nodiscard]] bool start() override;

    [[nodiscard]] bool startRuntime();

    void stop() override;

    [[nodiscard]] bool beginFrame(TrackingState& tracking, FrameState& frame) override;

    [[nodiscard]] bool frameActive() const override;

    void eyeResolution(int& width, int& height) const override;

    [[nodiscard]] EyeSizes eyeSizes() const override;

    [[nodiscard]] const HiddenArea* hiddenArea(int eye) const override;

    [[nodiscard]] bool runtimeMenuOpen() const override;

    [[nodiscard]] const char* systemName() const override;

    [[nodiscard]] unsigned acquireEyeImage(int eye) override;

    void releaseEyeImage(int eye) override;

    void endFrame(bool rendered) override;

    [[nodiscard]] unsigned acquirePanelImage(int width, int height) override;

    void releasePanelImage() override;

    [[nodiscard]] bool runtimePanel(Pose& pose, glm::vec2& size) const override;

private:
    // The panel: runtimePanelWidth wide, the canvas's shape, placed by placeRuntimePanel (vr_backend.hpp).
    [[nodiscard]] glm::vec2 panelSize() const;

    void placePanel();

    bool ensurePanelSwapchain(int width, int height);

    struct Swapchain
    {
        XrSwapchain handle{XR_NULL_HANDLE};
        int32_t width{0};
        int32_t height{0};
        za::Vector<XrSwapchainImageOpenGLKHR> images;
    };

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
#else
    xrgl::Binding glctx;                          // the game's GLX or EGL context, as bound to the session
    bool eglExtension{false};                     // XR_MNDX_egl_enable enabled (the EGL binding possible)
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

    double timeCall(Call call, double since);
    double frameWaitMs{0.0}; // this frame's xrWaitFrame (xrstall::frame)

    // check's, for xrWaitFrame and xrEndFrame: each failure timestamped in the log and Tracy, repeats folded.
    bool checkFrameCall(XrResult result, const char* what) const;

    // After xrWaitFrame: the frame period, the display periods missed, and once a second the log's line.
    void noteWaited();

    // The local time, for the log (hh:mm:ss.mmm).
    [[nodiscard]] static const char* wallClock();

    [[nodiscard]] static const char* stateName(XrSessionState state);

    // A state (or the instance's loss) into the trail sessionStates() gives, the oldest dropped past stateTrailMax.
    void noteState(const char* name);

    [[nodiscard]] SessionEnd sessionEnd() const override;

    [[nodiscard]] const char* sessionStates() const override;

    // Reads an eye's hidden area (the triangles the lenses never show).
    void fetchHiddenArea(int eye);

    bool check(XrResult result, const char* what) const;

    // A line in the start's breadcrumbs before the call it names (the crash report's "Last GL steps", gl_startup.log
    // while the game starts), in qvr_openxr.txt, and in the crash report's VR line.
    void step(const char* format, ...);

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
        // The runtime may have left its own GL context current (VDXR switches to the game's around its GL calls).
#ifdef _WIN32
        if(glRC && wglGetCurrentContext() != glRC)
        {
            const BOOL made = wglMakeCurrent(glDC, glRC);
            xrruntime::warn("OpenXR: the game's GL context made current again after the crash: %s\n", made ? "done" : "FAILED");
        }
#endif
        return XR_ERROR_RUNTIME_FAILURE;
    }

    // A swapchain of width x height images in colorFormat, validated against what the runtime offered (the format,
    // the size, the sample count), each request logged before the call; the images checked (a count, none zero).
    bool createSwapchain(Swapchain& sc, int32_t width, int32_t height, XrSwapchainUsageFlags usage, const char* what);

    [[nodiscard]] static const char* glFormatName(int64_t format);

    // The GL texture of the swapchain's next image, acquired and waited for; 0 on failure.
    [[nodiscard]] unsigned acquireImage(const Swapchain& sc);

    bool releaseImage(const Swapchain& sc) const;

    static void destroySpace(XrSpace& space);

    // Tracked hand -> physical side (0 left): HAND_MAIN is the right controller, left-handed too (vr_backend.hpp).
    [[nodiscard]] static int handSide(int hand);

    [[nodiscard]] static Pose toPose(const XrPosef& p, bool valid);

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

    [[nodiscard]] static const char* shortProfile(const char* name);

    // The crash report's VR line: the runtime and headset, the controllers' profiles once known, and the last step.
    void setCrashVr(const char* lastStep);

    void updateControllers();

    [[nodiscard]] Pose toLegacyPose(const Pose& grip, int side) const;

    [[nodiscard]] Pose locate(XrSpace space, XrTime time) const;

    // What a runtime that failed to start leaves behind (stop() destroys its handles).
    void resetRuntimeState();

    bool createInstance();

    bool createSystem();

    // Before a call in which the runtime makes GL calls of its own (xrCreateSwapchain, xrEnumerateSwapchainImages; the
    // session's binding, createSession): the game's context current on this thread, the very one bound to the session,
    // its DC still its window's. Logged with both handles; made current again when it isn't; false (the call not made,
    // the start fails) when that fails or the window is gone. (1.0.1: two runtimes, VDXR and SteamVR, died in the first
    // swapchain on one player's PC: a runtime's GL calls without the game's context read null function pointers.)
#ifndef _WIN32
    // Linux: the same with GLX (the context and its drawable, the window's) or EGL (the context and its surfaces).
    bool contextReady(const char* what);

    // What the GL driver offers a runtime that shares its Vulkan images with the game (Monado's GL client:
    // GL_EXT_memory_object_fd and GL_EXT_semaphore_fd), logged before the session.
    void logGlInterop();
#else
    bool contextReady(const char* what);

    // What the GL driver offers a runtime that shares its D3D11 images with the game, logged before the session: VDXR
    // imports them with GL_EXT_memory_object_win32 (glCreateMemoryObjectsEXT, glImportMemoryWin32HandleEXT,
    // glTextureStorageMem2DEXT: seen in its 1.1.0 code), others with WGL_NV_DX_interop2.
    void logGlInterop();
#endif

    bool createSession();

    bool createSpaces();

    [[nodiscard]] XrPath path(const char* s) const;

    [[nodiscard]] XrAction makeAction(XrActionType type, const char* name, const char* localized, bool perHand);

    struct Binding
    {
        XrAction* action;
        const char* path;
    };

    [[nodiscard]] XrResult suggestList(XrPath profile, const za::Vector<XrActionSuggestedBinding>& list) const;

    // One profile's bindings (1.0.1, ROUND22.md "Controls as if another controller"). A runtime turns a whole suggestion
    // down for a single path it lacks (XR_ERROR_PATH_UNSUPPORTED), and then plays the controllers as another profile the
    // game bound (Touch as a simple controller: no grips, no A/B). So when the whole is turned down, the bindings are
    // added again one at a time (the grip pose first), each one the runtime turns down dropped and named (qvr_openxr.txt
    // and the console), the rest kept: one bad path costs that path, never the profile. None taken: the runtime does not
    // have the profile at all (another vendor's, an extension's), which is expected and said in qvr_openxr.txt only.
    bool suggest(const char* profile, za::InitializerList<Binding> bindings);

    // The actions are the controllers' physical controls; the game assigns them through key
    // bindings. Controllers without some control (Vive wands, WMR) map the closest one.
    void suggestBindings();

    bool createActions();

    [[nodiscard]] bool boolState(XrAction action, XrPath subaction) const;

    [[nodiscard]] float floatState(XrAction action, XrPath subaction) const;

    [[nodiscard]] glm::vec2 vec2State(XrAction action, XrPath subaction) const;

    void readInput(InputState& in) const;

public:
    void haptic(int hand, float seconds, float frequency, float amplitude) override;

private:
    bool createSwapchains();

    // The eyes' swapchains, at the recommended size for the whole session: vr_render_scale changes
    // the size the eyes are rendered at, resampled into these (vr_stereo.cpp). Not recreated at
    // another size: SteamVR's OpenGL path keeps copying into textures of the first eye images'
    // size, so larger new images were shown cropped (a corner, magnified: a stretched, wrong
    // projection) and smaller ones not at all (a GL_INVALID_VALUE copy).
    // vr_xr_eye_scale: smaller than recommended (each side; within 64 and the largest), at the start only.
    bool createEyeSwapchains();

    void beginSession();

    double nextProfileCheck{0.0}; // pollEvents: a hand without a profile looked up again every 2 s (an event missed)

    // Each frame: the hands' profiles if a runtime has not said (a hand with none, every 2 s); the limited-controls
    // warning centre-printed once a game is on (updateControllers).
    void checkControllers();

    // Returns false if the session is lost or the application should exit VR.
    bool pollEvents();
};

} // namespace qvr::oxr

#endif // QVR_HAVE_OPENXR
