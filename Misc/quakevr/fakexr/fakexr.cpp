// fakexr.cpp -- a fake OpenXR runtime for the headless tests of the runtime choice (vr_xr_runtime Auto, its fallback):
// the loader loads it from a manifest like any runtime. It has no headset: xrCreateInstance works (or fails, as asked)
// and xrGetSystem fails, so the game goes on to the next runtime (unless the copy has the fake headset). Copies of the DLL under other names
// (fakexr_a.dll, fakexr_b.dll) are other runtimes: the name it reports is its file's.
//
//   FAKEXR_LOG            a file each copy appends to: loaded, instance made, instance destroyed, unloaded
//   FAKEXR_FAIL_INSTANCE  copies (file names without .dll, comma-separated) whose xrCreateInstance fails
//   FAKEXR_HEADSET        copies with a headset: a session, OpenGL swapchains (3 images each, made in the game's
//                         context), frames (90 Hz times, not paced unless FAKEXR_PERIOD_MS), no input; at
//                         xrDestroySession the log gets the frames' layers (projection ones without an image released
//                         since the last frame: a frame shown again)
//   FAKEXR_EYE            WxH, the eyes' recommended size (400x440)
//   FAKEXR_SYSTEM         the headset's name (systemName: "FakeXR headset"), e.g. "Meta Quest 3" (its panel known)
//   FAKEXR_UNFOCUS        a-b: the session VISIBLE (not focused: as with SteamVR's dashboard) from xrWaitFrame a to b
//   FAKEXR_END            n:kind, the session ended at xrWaitFrame n as the runtimes end it: exit (the player quit the
//                         game from the runtime's menu: VISIBLE, SYNCHRONIZED, STOPPING; after xrEndSession IDLE, then
//                         EXITING), restart (the same, then READY again: the headset back on), loss (LOSS_PENDING),
//                         instanceloss (XrEventDataInstanceLossPending)
//   FAKEXR_LOST           n:call:code, the runtime gone without a loss event (a crashed compositor): from xrWaitFrame n
//                         on, call (poll: xrPollEvent, wait: xrWaitFrame, begin: xrBeginFrame, end: xrEndFrame) returns
//                         code (session: XR_ERROR_SESSION_LOST, instance: XR_ERROR_INSTANCE_LOST), no event queued; the
//                         log says when it starts, and at xrDestroySession how many calls it answered so
//   FAKEXR_IDLE_MS        with FAKEXR_END n:restart, READY comes that long (wall clock) after xrEndSession's IDLE, not
//                         at once: frames with no session running (no xrWaitFrame), as a headset taken off; the log
//                         gives the frames the game polled meanwhile (each frame's xrPollEvent finding no event) and
//                         their rate
//   FAKEXR_FAIL_PANEL_IMAGES  copies whose xrEnumerateSwapchainImages fails filling a transfer-destination swapchain's
//                         images (XR_ERROR_RUNTIME_FAILURE, no crash: the game's panel, its eyes not); logged each time,
//                         with the acquires of such a swapchain and its destruction
//   FAKEXR_FAULT_IMAGES   copies whose xrEnumerateSwapchainImages crashes filling the images (a null read, as VDXR
//                         1.1.0's did: the game's vr_xr_guard catches it); FAKEXR_FAULT_SRGB: only sRGB swapchains'
//   FAKEXR_DEBUG_UTILS    copies that offer XR_EXT_debug_utils: a messenger that sends 3 messages once made
//   FAKEXR_EXTENSIONS     more extensions every copy offers (comma-separated names, e.g. XR_BD_controller_interaction:
//                         the game's vendor controller profiles); each xrSuggestInteractionProfileBindings is logged
//                         (its profile and binding count)
//   FAKEXR_REJECT_PATH    a binding path's text: a suggestion holding a path that contains it is turned down
//                         (XR_ERROR_PATH_UNSUPPORTED), as a runtime turns down a path its profile lacks; several
//                         separated by ';', each one "profile=path" for one profile's only (texts contained in them,
//                         e.g. "oculus/touch=thumbstick/touch;left/input/menu/click")
//   FAKEXR_PROFILE        the profile the hands are played as (a text contained in it), if it was suggested; else the
//                         first suggested of Touch, WMR, Index, Vive, simple (as VDXR picks). Said at
//                         xrAttachSessionActionSets (logged with its bindings), by xrGetCurrentInteractionProfile and
//                         by an XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED
//   FAKEXR_D3D11          copies that load d3d11.dll at xrCreateInstance and free it at xrDestroyInstance (as VDXR
//                         does), logging whether it is still loaded after (the game keeps it: vr_backend_openxr.cpp)
//   FAKEXR_UNWORN         a-b, with XR_EXT_user_presence in FAKEXR_EXTENSIONS (its system then has the sensor:
//                         XrSystemUserPresencePropertiesEXT): the headset taken off at xrWaitFrame a, back on at b
//                         (XrEventDataUserPresenceChangedEXT; the session stays FOCUSED, as SteamVR's on the Frame)
//
// Built by Misc/quakevr/fakexr/build.sh into <worktree>/scratch/fakexr (docs/vr-port/TESTING.md, "OpenXR runtime
// choice"): a DLL on Windows, a shared object on Linux (fakexr_<copy>.so; its GL calls go to the game's current GLX or
// EGL context, through the libGL it loaded; FAKEXR_D3D11 and the FAULT_* crashes are Windows' alone: the game's
// vr_xr_guard catches a runtime's crash on Windows only).

#ifdef _WIN32
#include <windows.h>
#include <unknwn.h>
#define XR_USE_PLATFORM_WIN32
#else
#include <dlfcn.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#endif

#include <GL/gl.h>

#define XR_USE_GRAPHICS_API_OPENGL
#include <openxr/openxr.h>
#include <openxr/openxr_loader_negotiation.h>
#include <openxr/openxr_platform.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define FAKEXR_EXPORT __declspec(dllexport)
#else
#ifndef APIENTRY
#define APIENTRY
#endif
#define FAKEXR_EXPORT __attribute__((visibility("default")))
#define strtok_s strtok_r
#endif

namespace
{

// ---- What differs between Windows and Linux

// An environment variable's text into `out` (false, `out` empty: not set or empty).
bool envText(const char* name, char* out, size_t size)
{
#ifdef _WIN32
    const DWORD n = GetEnvironmentVariableA(name, out, static_cast<DWORD>(size));
    if(n == 0 || n >= size)
    {
        out[0] = '\0';
        return false;
    }
    return true;
#else
    const char* value = getenv(name);
    if(!value || !value[0])
    {
        out[0] = '\0';
        return false;
    }
    snprintf(out, size, "%s", value);
    return true;
#endif
}

template <size_t N>
void copyText(char (&out)[N], const char* text)
{
    snprintf(out, N, "%s", text);
}

[[nodiscard]] bool sameName(const char* a, const char* b)
{
#ifdef _WIN32
    return !_stricmp(a, b);
#else
    return !strcasecmp(a, b);
#endif
}

void sleepMs(int ms)
{
#ifdef _WIN32
    Sleep(static_cast<DWORD>(ms));
#else
    usleep(static_cast<useconds_t>(ms) * 1000u);
#endif
}

// A GL context current on this thread (WGL's; on Linux GLX's or EGL's, looked up in what the game loaded).
[[nodiscard]] bool glContextCurrent()
{
#ifdef _WIN32
    return wglGetCurrentContext() != nullptr;
#else
    using Current = void* (*)();
    static constexpr const char* names[] = {"glXGetCurrentContext", "eglGetCurrentContext"};
    for(const char* name : names)
    {
        const auto current = reinterpret_cast<Current>(dlsym(RTLD_DEFAULT, name));
        if(current && current())
        {
            return true;
        }
    }
    return false;
#endif
}

[[nodiscard]] void* glProc(const char* name)
{
#ifdef _WIN32
    return reinterpret_cast<void*>(wglGetProcAddress(name));
#else
    return dlsym(RTLD_DEFAULT, name);
#endif
}

char moduleName[64] = "fakexr";

void log(const char* what)
{
    char path[1024];
    if(!envText("FAKEXR_LOG", path, sizeof(path)))
    {
        return;
    }
    FILE* f = fopen(path, "a");
    if(f)
    {
        fprintf(f, "%s %s\n", moduleName, what);
        fclose(f);
    }
}

bool listed(const char* variable)
{
    char list[1024];
    if(!envText(variable, list, sizeof(list)))
    {
        return false;
    }
    for(char* token = strtok(list, ","); token; token = strtok(nullptr, ","))
    {
        if(sameName(token, moduleName))
        {
            return true;
        }
    }
    return false;
}

#ifdef _WIN32
HMODULE d3d11 = nullptr; // FAKEXR_D3D11
#endif

const XrInstance fakeInstance = reinterpret_cast<XrInstance>(static_cast<uintptr_t>(0x5eed));

XRAPI_ATTR XrResult XRAPI_CALL enumerateInstanceExtensionProperties(
    const char* layerName, uint32_t capacity, uint32_t* count, XrExtensionProperties* properties)
{
    if(layerName)
    {
        return XR_ERROR_API_LAYER_NOT_PRESENT;
    }
    const bool debugUtils = listed("FAKEXR_DEBUG_UTILS");
    char extra[1024] = "";
    envText("FAKEXR_EXTENSIONS", extra, sizeof(extra));
    const char* names[16];
    uint32_t n = 0;
    names[n++] = "XR_KHR_opengl_enable"; // (the game's graphics binding)
    if(debugUtils)
    {
        names[n++] = "XR_EXT_debug_utils";
    }
    for(char* token = strtok(extra, ","); token && n < 16; token = strtok(nullptr, ","))
    {
        names[n++] = token;
    }
    *count = n;
    if(capacity == 0)
    {
        return XR_SUCCESS;
    }
    for(uint32_t i = 0; i < n && i < capacity; i++)
    {
        copyText(properties[i].extensionName, names[i]);
        properties[i].extensionVersion = 1;
    }
    return XR_SUCCESS;
}

// FAKEXR_DEBUG_UTILS: a messenger that says hello once made (the game's diagnostics mode logs it).
XRAPI_ATTR XrResult XRAPI_CALL createDebugUtilsMessenger(
    XrInstance, const XrDebugUtilsMessengerCreateInfoEXT* info, XrDebugUtilsMessengerEXT* messenger)
{
    *messenger = reinterpret_cast<XrDebugUtilsMessengerEXT>(static_cast<uintptr_t>(0xd1a6));
    XrDebugUtilsMessengerCallbackDataEXT data{XR_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT};
    data.messageId = "FAKEXR-hello";
    data.functionName = "xrCreateDebugUtilsMessengerEXT";
    data.message = "the fake runtime's messenger is on";
    for(int i = 0; i < 3; i++)
    {
        info->userCallback(XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT, XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, &data,
            info->userData);
    }
    log("xrCreateDebugUtilsMessengerEXT");
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL destroyDebugUtilsMessenger(XrDebugUtilsMessengerEXT)
{
    log("xrDestroyDebugUtilsMessengerEXT");
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL createInstance(const XrInstanceCreateInfo* /* info */, XrInstance* instance)
{
    if(listed("FAKEXR_FAIL_INSTANCE"))
    {
        log("xrCreateInstance failed (FAKEXR_FAIL_INSTANCE)");
        return XR_ERROR_RUNTIME_UNAVAILABLE;
    }
    log("xrCreateInstance");
#ifdef _WIN32
    if(listed("FAKEXR_D3D11"))
    {
        d3d11 = LoadLibraryA("d3d11.dll");
        log(d3d11 ? "d3d11.dll loaded" : "d3d11.dll not loaded");
    }
#endif
    *instance = fakeInstance;
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL destroyInstance(XrInstance /* instance */)
{
    log("xrDestroyInstance");
#ifdef _WIN32
    if(d3d11)
    {
        FreeLibrary(d3d11);
        d3d11 = nullptr;
        log(GetModuleHandleA("d3d11.dll") ? "d3d11.dll freed: still loaded" : "d3d11.dll freed: unloaded");
    }
#endif
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL getInstanceProperties(XrInstance /* instance */, XrInstanceProperties* properties)
{
    properties->runtimeVersion = XR_MAKE_VERSION(1, 0, 0);
    snprintf(properties->runtimeName, XR_MAX_RUNTIME_NAME_SIZE, "FakeXR %s", moduleName);
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL getSystem(XrInstance /* instance */, const XrSystemGetInfo* /* info */, XrSystemId* id)
{
    if(listed("FAKEXR_HEADSET"))
    {
        log("xrGetSystem: the fake headset");
        *id = 1;
        return XR_SUCCESS;
    }
    log("xrGetSystem: no headset");
    return XR_ERROR_FORM_FACTOR_UNAVAILABLE;
}

// ---- The fake headset (FAKEXR_HEADSET): a session, OpenGL swapchains, frames ----

constexpr GLenum glSrgb8Alpha8 = 0x8C43;
constexpr GLenum glRgba16f = 0x881A;
constexpr GLenum glPixelUnpackBuffer = 0x88EC;
constexpr GLenum glPixelUnpackBufferBinding = 0x88EF;
constexpr XrDuration displayPeriod = 11111111; // 90 Hz
constexpr int imageCount = 3;

struct FakeSwapchain
{
    uint64_t handle{0};
    GLuint textures[imageCount]{};
    uint32_t next{0};
    bool everReleased{false};
    bool releasedSinceEnd{false};
    int64_t format{0};
    XrSwapchainUsageFlags usage{0};
    uint32_t width{0}, height{0};
    bool imagesFailed{false}; // FAKEXR_FAIL_PANEL_IMAGES failed its images
};

struct Headset
{
    uint64_t nextHandle{0x1000};
    uint64_t viewSpace{0};
    FakeSwapchain swapchains[16];
    XrSessionState queue[32]{};
    int queueHead{0}, queueTail{0};
    XrSessionState state{XR_SESSION_STATE_UNKNOWN};
    XrSession session{XR_NULL_HANDLE};
    int frame{0}; // xrWaitFrame calls
    int unfocusFrom{-1}, unfocusTo{-1};
    int unwornFrom{-1}, unwornTo{-1}; // FAKEXR_UNWORN
    int endAt{-1};      // FAKEXR_END's frame
    char endKind[16]{}; // and kind
    bool exitAfterStop{false}, readyAfterStop{false};
    uint32_t eyeWidth{400}, eyeHeight{440};
    int periodMs{0};
    int frames{0}, projections{0}, held{0}, quads{0}, empty{0}, focusLost{0};
    // FAKEXR_LOST: from xrWaitFrame lostAt on, lostCall returns lostCode; the calls answered so.
    int lostAt{-1};
    char lostCall[16]{};
    XrResult lostCode{XR_SUCCESS};
    int lostAnswered{0};
    // FAKEXR_IDLE_MS: READY held back until readyAt (nowNs), the frames polled since idleSince.
    int idleMs{0};
    XrTime idleSince{0}, readyAt{0};
    int idlePolls{0};
    // FAKEXR_FAIL_PANEL_IMAGES: image fills failed, acquires of such swapchains.
    int imageFailures{0}, brokenAcquires{0};
    char paths[512][128]{};
    int pathCount{0};
    // The profiles' last accepted suggestions (xrSuggestInteractionProfileBindings), and the one the hands are played as.
    struct Suggestion
    {
        XrPath profile{XR_NULL_PATH};
        XrPath bindings[128]{};
        uint32_t count{0};
    };
    Suggestion suggestions[24];
    int suggestionCount{0};
    XrPath current{XR_NULL_PATH};
};
Headset hs;

template <typename T>
T newHandle()
{
    return reinterpret_cast<T>(static_cast<uintptr_t>(hs.nextHandle++));
}

[[nodiscard]] XrTime nowNs()
{
#ifdef _WIN32
    LARGE_INTEGER count, frequency;
    QueryPerformanceCounter(&count);
    QueryPerformanceFrequency(&frequency);
    return static_cast<XrTime>(static_cast<double>(count.QuadPart) * 1e9 / static_cast<double>(frequency.QuadPart));
#else
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<XrTime>(ts.tv_sec) * 1000000000 + ts.tv_nsec;
#endif
}

void pushState(XrSessionState state)
{
    hs.queue[hs.queueTail] = state;
    hs.queueTail = (hs.queueTail + 1) % 32;
}

[[nodiscard]] FakeSwapchain* findSwapchain(XrSwapchain handle)
{
    for(FakeSwapchain& sc : hs.swapchains)
    {
        if(sc.handle && sc.handle == reinterpret_cast<uintptr_t>(handle))
        {
            return &sc;
        }
    }
    return nullptr;
}

XRAPI_ATTR XrResult XRAPI_CALL getSystemProperties(XrInstance, XrSystemId, XrSystemProperties* properties)
{
    properties->systemId = 1;
    properties->vendorId = 0xfa4e;
    char name[XR_MAX_SYSTEM_NAME_SIZE];
    strcpy(properties->systemName, envText("FAKEXR_SYSTEM", name, sizeof(name)) ? name : "FakeXR headset");
    properties->graphicsProperties.maxLayerCount = 16;
    properties->graphicsProperties.maxSwapchainImageWidth = 8192;
    properties->graphicsProperties.maxSwapchainImageHeight = 8192;
    properties->trackingProperties.orientationTracking = XR_TRUE;
    properties->trackingProperties.positionTracking = XR_TRUE;
    // XR_EXT_user_presence offered (FAKEXR_EXTENSIONS): the system has the sensor.
    char extra[1024];
    const bool presence = envText("FAKEXR_EXTENSIONS", extra, sizeof(extra)) && strstr(extra, "XR_EXT_user_presence");
    for(auto* p = static_cast<XrBaseOutStructure*>(properties->next); p; p = p->next)
    {
        if(p->type == XR_TYPE_SYSTEM_USER_PRESENCE_PROPERTIES_EXT)
        {
            reinterpret_cast<XrSystemUserPresencePropertiesEXT*>(p)->supportsUserPresence = presence ? XR_TRUE : XR_FALSE;
        }
    }
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL getOpenGLGraphicsRequirements(XrInstance, XrSystemId, XrGraphicsRequirementsOpenGLKHR* r)
{
    r->minApiVersionSupported = XR_MAKE_VERSION(3, 3, 0);
    r->maxApiVersionSupported = XR_MAKE_VERSION(4, 6, 0);
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL createSession(XrInstance, const XrSessionCreateInfo*, XrSession* session)
{
    char text[64];
    hs = Headset{};
    if(envText("FAKEXR_EYE", text, sizeof(text)))
    {
        sscanf(text, "%ux%u", &hs.eyeWidth, &hs.eyeHeight);
    }
    if(envText("FAKEXR_UNFOCUS", text, sizeof(text)))
    {
        sscanf(text, "%d-%d", &hs.unfocusFrom, &hs.unfocusTo);
    }
    if(envText("FAKEXR_UNWORN", text, sizeof(text)))
    {
        sscanf(text, "%d-%d", &hs.unwornFrom, &hs.unwornTo);
    }
    if(envText("FAKEXR_END", text, sizeof(text)))
    {
        sscanf(text, "%d:%15s", &hs.endAt, hs.endKind);
    }
    if(envText("FAKEXR_PERIOD_MS", text, sizeof(text)))
    {
        hs.periodMs = atoi(text);
    }
    if(envText("FAKEXR_LOST", text, sizeof(text)))
    {
        char code[16] = "";
        if(sscanf(text, "%d:%15[^:]:%15s", &hs.lostAt, hs.lostCall, code) == 3)
        {
            hs.lostCode = !strcmp(code, "instance") ? XR_ERROR_INSTANCE_LOST : XR_ERROR_SESSION_LOST;
        }
        else
        {
            hs.lostAt = -1;
            log("FAKEXR_LOST: not n:call:code");
        }
    }
    if(envText("FAKEXR_IDLE_MS", text, sizeof(text)))
    {
        hs.idleMs = atoi(text);
    }
    hs.session = newHandle<XrSession>();
    *session = hs.session;
    pushState(XR_SESSION_STATE_IDLE);
    pushState(XR_SESSION_STATE_READY);
    log("xrCreateSession");
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL destroySession(XrSession)
{
    char text[256];
    snprintf(text, sizeof(text),
        "xrDestroySession: %d frames: %d projection layers (%d without an image released since the last frame), "
        "%d quad layers, %d without layers; the focus lost %d times",
        hs.frames, hs.projections, hs.held, hs.quads, hs.empty, hs.focusLost);
    log(text);
    if(hs.lostAt >= 0)
    {
        snprintf(text, sizeof(text), "FAKEXR_LOST: %d calls answered %s", hs.lostAnswered,
            hs.lostCode == XR_ERROR_INSTANCE_LOST ? "XR_ERROR_INSTANCE_LOST" : "XR_ERROR_SESSION_LOST");
        log(text);
    }
    if(listed("FAKEXR_FAIL_PANEL_IMAGES"))
    {
        snprintf(text, sizeof(text), "FAKEXR_FAIL_PANEL_IMAGES: %d image fills failed, %d acquires of such a swapchain",
            hs.imageFailures, hs.brokenAcquires);
        log(text);
    }
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL beginSession(XrSession, const XrSessionBeginInfo*)
{
    pushState(XR_SESSION_STATE_SYNCHRONIZED);
    pushState(XR_SESSION_STATE_VISIBLE);
    pushState(XR_SESSION_STATE_FOCUSED);
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL endSession(XrSession)
{
    if(hs.state != XR_SESSION_STATE_STOPPING)
    {
        log("xrEndSession: not STOPPING (XR_ERROR_SESSION_NOT_STOPPING)");
        return XR_ERROR_SESSION_NOT_STOPPING;
    }
    log("xrEndSession");
    pushState(XR_SESSION_STATE_IDLE);
    if(hs.exitAfterStop)
    {
        pushState(XR_SESSION_STATE_EXITING);
    }
    if(hs.readyAfterStop && hs.idleMs > 0)
    {
        hs.idleSince = nowNs();
        hs.readyAt = hs.idleSince + static_cast<XrTime>(hs.idleMs) * 1000000;
        hs.idlePolls = 0;
    }
    else if(hs.readyAfterStop)
    {
        pushState(XR_SESSION_STATE_READY);
    }
    hs.exitAfterStop = hs.readyAfterStop = false;
    return XR_SUCCESS;
}

// In the state queue: an XrEventDataInstanceLossPending rather than a state (FAKEXR_END instanceloss); an
// XrEventDataInteractionProfileChanged (xrAttachSessionActionSets); an XrEventDataUserPresenceChangedEXT, the user
// gone or back (FAKEXR_UNWORN).
constexpr XrSessionState instanceLossEvent = XR_SESSION_STATE_MAX_ENUM;
constexpr XrSessionState profileChangedEvent = static_cast<XrSessionState>(XR_SESSION_STATE_MAX_ENUM - 1);
constexpr XrSessionState userAbsentEvent = static_cast<XrSessionState>(XR_SESSION_STATE_MAX_ENUM - 2);
constexpr XrSessionState userPresentEvent = static_cast<XrSessionState>(XR_SESSION_STATE_MAX_ENUM - 3);

// FAKEXR_LOST: whether `call` answers lost now (counted).
[[nodiscard]] bool lostNow(const char* call)
{
    if(hs.lostAt < 0 || hs.frame < hs.lostAt || strcmp(hs.lostCall, call))
    {
        return false;
    }
    hs.lostAnswered++;
    return true;
}

XRAPI_ATTR XrResult XRAPI_CALL pollEvent(XrInstance, XrEventDataBuffer* buffer)
{
    if(lostNow("poll"))
    {
        return hs.lostCode;
    }
    if(hs.readyAt && nowNs() >= hs.readyAt)
    {
        char text[160];
        const double ms = static_cast<double>(nowNs() - hs.idleSince) * 1e-6;
        snprintf(text, sizeof(text), "the idle over (FAKEXR_IDLE_MS): %.0f ms, %d frames polled (%.0f a second)", ms,
            hs.idlePolls, ms > 0.0 ? hs.idlePolls * 1000.0 / ms : 0.0);
        log(text);
        hs.readyAt = 0;
        pushState(XR_SESSION_STATE_READY);
    }
    if(hs.queueHead == hs.queueTail)
    {
        hs.idlePolls += hs.readyAt ? 1 : 0;
        return XR_EVENT_UNAVAILABLE;
    }
    if(hs.queue[hs.queueHead] == instanceLossEvent)
    {
        hs.queueHead = (hs.queueHead + 1) % 32;
        auto* loss = reinterpret_cast<XrEventDataInstanceLossPending*>(buffer);
        loss->type = XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING;
        loss->next = nullptr;
        loss->lossTime = nowNs() + 1000000000;
        return XR_SUCCESS;
    }
    if(hs.queue[hs.queueHead] == profileChangedEvent)
    {
        hs.queueHead = (hs.queueHead + 1) % 32;
        auto* profile = reinterpret_cast<XrEventDataInteractionProfileChanged*>(buffer);
        profile->type = XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED;
        profile->next = nullptr;
        profile->session = hs.session;
        return XR_SUCCESS;
    }
    if(hs.queue[hs.queueHead] == userAbsentEvent || hs.queue[hs.queueHead] == userPresentEvent)
    {
        auto* presence = reinterpret_cast<XrEventDataUserPresenceChangedEXT*>(buffer);
        presence->type = XR_TYPE_EVENT_DATA_USER_PRESENCE_CHANGED_EXT;
        presence->next = nullptr;
        presence->session = hs.session;
        presence->isUserPresent = hs.queue[hs.queueHead] == userPresentEvent ? XR_TRUE : XR_FALSE;
        hs.queueHead = (hs.queueHead + 1) % 32;
        return XR_SUCCESS;
    }
    auto* changed = reinterpret_cast<XrEventDataSessionStateChanged*>(buffer);
    changed->type = XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED;
    changed->next = nullptr;
    changed->session = hs.session;
    changed->state = hs.queue[hs.queueHead];
    changed->time = nowNs();
    hs.state = changed->state;
    hs.queueHead = (hs.queueHead + 1) % 32;
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL enumerateViewConfigurationViews(
    XrInstance, XrSystemId, XrViewConfigurationType, uint32_t capacity, uint32_t* count, XrViewConfigurationView* views)
{
    *count = 2;
    for(uint32_t i = 0; i < capacity && i < 2; i++)
    {
        views[i].recommendedImageRectWidth = hs.eyeWidth;
        views[i].recommendedImageRectHeight = hs.eyeHeight;
        views[i].maxImageRectWidth = hs.eyeWidth * 2;
        views[i].maxImageRectHeight = hs.eyeHeight * 2;
        views[i].recommendedSwapchainSampleCount = 1;
        views[i].maxSwapchainSampleCount = 4;
    }
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL enumerateSwapchainFormats(XrSession, uint32_t capacity, uint32_t* count, int64_t* formats)
{
    const int64_t offered[] = {GL_RGBA8, glSrgb8Alpha8, glRgba16f}; // (SteamVR's first is not sRGB either)
    *count = 3;
    for(uint32_t i = 0; i < capacity && i < 3; i++)
    {
        formats[i] = offered[i];
    }
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL createSwapchain(XrSession, const XrSwapchainCreateInfo* info, XrSwapchain* swapchain)
{
    for(FakeSwapchain& sc : hs.swapchains)
    {
        if(sc.handle)
        {
            continue;
        }
        sc = FakeSwapchain{};
        sc.handle = hs.nextHandle++;
        sc.format = info->format;
        sc.usage = info->usageFlags;
        sc.width = info->width;
        sc.height = info->height;
        if(!glContextCurrent())
        {
            log("xrCreateSwapchain: NO GL CONTEXT CURRENT");
        }
        // In the game's context (current on this thread), its bindings kept.
        GLint texture = 0, unpack = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
        glGetIntegerv(glPixelUnpackBufferBinding, &unpack);
        using BindBuffer = void(APIENTRY*)(GLenum, GLuint);
        const auto bindBuffer = reinterpret_cast<BindBuffer>(glProc("glBindBuffer"));
        if(unpack && bindBuffer)
        {
            bindBuffer(glPixelUnpackBuffer, 0);
        }
        glGenTextures(imageCount, sc.textures);
        for(GLuint t : sc.textures)
        {
            glBindTexture(GL_TEXTURE_2D, t);
            glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(info->format), static_cast<GLsizei>(info->width),
                static_cast<GLsizei>(info->height), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        }
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture));
        if(unpack && bindBuffer)
        {
            bindBuffer(glPixelUnpackBuffer, static_cast<GLuint>(unpack));
        }
        *swapchain = reinterpret_cast<XrSwapchain>(static_cast<uintptr_t>(sc.handle));
        char text[128];
        snprintf(text, sizeof(text), "xrCreateSwapchain %ux%u format 0x%llx", info->width, info->height,
            static_cast<unsigned long long>(info->format));
        log(text);
        return XR_SUCCESS;
    }
    return XR_ERROR_LIMIT_REACHED;
}

XRAPI_ATTR XrResult XRAPI_CALL destroySwapchain(XrSwapchain swapchain)
{
    FakeSwapchain* sc = findSwapchain(swapchain);
    if(!sc)
    {
        return XR_ERROR_HANDLE_INVALID;
    }
    if(sc->imagesFailed)
    {
        char text[128];
        snprintf(text, sizeof(text), "xrDestroySwapchain %ux%u: the one whose images failed", sc->width, sc->height);
        log(text);
    }
    glDeleteTextures(imageCount, sc->textures);
    *sc = FakeSwapchain{};
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL enumerateSwapchainImages(
    XrSwapchain swapchain, uint32_t capacity, uint32_t* count, XrSwapchainImageBaseHeader* images)
{
    FakeSwapchain* sc = findSwapchain(swapchain);
    if(!sc)
    {
        return XR_ERROR_HANDLE_INVALID;
    }
    *count = imageCount;
    // VDXR 1.1.0's crash (1.0.1): a null read filling the images, in the runtime.
    if(capacity > 0 && (listed("FAKEXR_FAULT_IMAGES") || (sc->format == glSrgb8Alpha8 && listed("FAKEXR_FAULT_SRGB"))))
    {
        log("xrEnumerateSwapchainImages: crashing (FAKEXR_FAULT_*)");
        volatile const unsigned* nowhere = nullptr;
        return static_cast<XrResult>(*nowhere);
    }
    if(capacity > 0 && (sc->usage & XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT) && listed("FAKEXR_FAIL_PANEL_IMAGES"))
    {
        sc->imagesFailed = true;
        hs.imageFailures++;
        char text[128];
        snprintf(text, sizeof(text), "xrEnumerateSwapchainImages %ux%u: failing (FAKEXR_FAIL_PANEL_IMAGES)", sc->width, sc->height);
        log(text);
        return XR_ERROR_RUNTIME_FAILURE;
    }
    auto* gl = reinterpret_cast<XrSwapchainImageOpenGLKHR*>(images);
    for(uint32_t i = 0; i < capacity && i < imageCount; i++)
    {
        gl[i].image = sc->textures[i];
    }
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL acquireSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageAcquireInfo*, uint32_t* index)
{
    FakeSwapchain* sc = findSwapchain(swapchain);
    if(!sc)
    {
        return XR_ERROR_HANDLE_INVALID;
    }
    if(sc->imagesFailed && hs.brokenAcquires++ == 0)
    {
        log("xrAcquireSwapchainImage: a swapchain whose images failed");
    }
    *index = sc->next;
    sc->next = (sc->next + 1) % imageCount;
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL waitSwapchainImage(XrSwapchain, const XrSwapchainImageWaitInfo*)
{
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL releaseSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageReleaseInfo*)
{
    FakeSwapchain* sc = findSwapchain(swapchain);
    if(!sc)
    {
        return XR_ERROR_HANDLE_INVALID;
    }
    sc->everReleased = sc->releasedSinceEnd = true;
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL waitFrame(XrSession, const XrFrameWaitInfo*, XrFrameState* state)
{
    hs.frame++;
    if(hs.frame == hs.lostAt)
    {
        char text[128];
        snprintf(text, sizeof(text), "lost from now (FAKEXR_LOST): xrWaitFrame %d, %s answers %s, no event", hs.frame,
            hs.lostCall, hs.lostCode == XR_ERROR_INSTANCE_LOST ? "XR_ERROR_INSTANCE_LOST" : "XR_ERROR_SESSION_LOST");
        log(text);
    }
    if(lostNow("wait"))
    {
        return hs.lostCode;
    }
    if(hs.frame == hs.unfocusFrom)
    {
        pushState(XR_SESSION_STATE_VISIBLE);
        hs.focusLost++;
        log("the focus lost (FAKEXR_UNFOCUS)");
    }
    if(hs.frame == hs.unfocusTo)
    {
        pushState(XR_SESSION_STATE_FOCUSED);
        log("the focus back");
    }
    if(hs.frame == hs.unwornFrom)
    {
        pushState(userAbsentEvent);
        log("the headset taken off (FAKEXR_UNWORN)");
    }
    if(hs.frame == hs.unwornTo)
    {
        pushState(userPresentEvent);
        log("the headset back on");
    }
    if(hs.frame == hs.endAt)
    {
        char text[64];
        snprintf(text, sizeof(text), "the session ends: %s (FAKEXR_END)", hs.endKind);
        log(text);
        if(!strcmp(hs.endKind, "loss"))
        {
            pushState(XR_SESSION_STATE_LOSS_PENDING);
        }
        else if(!strcmp(hs.endKind, "instanceloss"))
        {
            pushState(instanceLossEvent);
        }
        else
        {
            hs.exitAfterStop = !strcmp(hs.endKind, "exit");
            hs.readyAfterStop = !strcmp(hs.endKind, "restart");
            if(hs.state == XR_SESSION_STATE_FOCUSED)
            {
                pushState(XR_SESSION_STATE_VISIBLE);
            }
            pushState(XR_SESSION_STATE_SYNCHRONIZED);
            pushState(XR_SESSION_STATE_STOPPING);
        }
    }
    if(hs.periodMs > 0)
    {
        sleepMs(hs.periodMs);
    }
    state->predictedDisplayPeriod = displayPeriod;
    state->predictedDisplayTime = nowNs() + displayPeriod;
    state->shouldRender = hs.state == XR_SESSION_STATE_VISIBLE || hs.state == XR_SESSION_STATE_FOCUSED;
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL beginFrame(XrSession, const XrFrameBeginInfo*)
{
    return lostNow("begin") ? hs.lostCode : XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL endFrame(XrSession, const XrFrameEndInfo* info)
{
    if(lostNow("end"))
    {
        return hs.lostCode;
    }
    hs.frames++;
    hs.empty += info->layerCount == 0 ? 1 : 0;
    for(uint32_t i = 0; i < info->layerCount; i++)
    {
        const XrCompositionLayerBaseHeader* layer = info->layers[i];
        if(layer->type == XR_TYPE_COMPOSITION_LAYER_QUAD)
        {
            hs.quads++;
            continue;
        }
        if(layer->type != XR_TYPE_COMPOSITION_LAYER_PROJECTION)
        {
            continue;
        }
        hs.projections++;
        const auto* projection = reinterpret_cast<const XrCompositionLayerProjection*>(layer);
        bool fresh = false;
        for(uint32_t v = 0; v < projection->viewCount; v++)
        {
            const FakeSwapchain* sc = findSwapchain(projection->views[v].subImage.swapchain);
            if(!sc || !sc->everReleased)
            {
                log("xrEndFrame: a projection view without a released image: XR_ERROR_LAYER_INVALID");
                return XR_ERROR_LAYER_INVALID;
            }
            fresh = fresh || sc->releasedSinceEnd;
        }
        hs.held += fresh ? 0 : 1;
    }
    for(FakeSwapchain& sc : hs.swapchains)
    {
        sc.releasedSinceEnd = false;
    }
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL enumerateReferenceSpaces(XrSession, uint32_t capacity, uint32_t* count, XrReferenceSpaceType* types)
{
    const XrReferenceSpaceType offered[] = {XR_REFERENCE_SPACE_TYPE_VIEW, XR_REFERENCE_SPACE_TYPE_LOCAL, XR_REFERENCE_SPACE_TYPE_STAGE};
    *count = 3;
    for(uint32_t i = 0; i < capacity && i < 3; i++)
    {
        types[i] = offered[i];
    }
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL createReferenceSpace(XrSession, const XrReferenceSpaceCreateInfo* info, XrSpace* space)
{
    *space = newHandle<XrSpace>();
    if(info->referenceSpaceType == XR_REFERENCE_SPACE_TYPE_VIEW)
    {
        hs.viewSpace = reinterpret_cast<uintptr_t>(*space);
    }
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL createActionSpace(XrSession, const XrActionSpaceCreateInfo*, XrSpace* space)
{
    *space = newHandle<XrSpace>();
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL succeed()
{
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL locateSpace(XrSpace space, XrSpace, XrTime, XrSpaceLocation* location)
{
    location->locationFlags = XR_SPACE_LOCATION_ORIENTATION_VALID_BIT | XR_SPACE_LOCATION_POSITION_VALID_BIT |
                              XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT | XR_SPACE_LOCATION_POSITION_TRACKED_BIT;
    location->pose.orientation = {0.f, 0.f, 0.f, 1.f};
    const bool head = reinterpret_cast<uintptr_t>(space) == hs.viewSpace;
    location->pose.position = head ? XrVector3f{0.f, 1.7f, 0.f} : XrVector3f{0.2f, 1.2f, -0.3f};
    for(auto* next = static_cast<XrBaseOutStructure*>(location->next); next; next = next->next)
    {
        if(next->type == XR_TYPE_SPACE_VELOCITY)
        {
            reinterpret_cast<XrSpaceVelocity*>(next)->velocityFlags = 0;
        }
    }
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL locateViews(
    XrSession, const XrViewLocateInfo*, XrViewState* state, uint32_t capacity, uint32_t* count, XrView* views)
{
    state->viewStateFlags = XR_VIEW_STATE_ORIENTATION_VALID_BIT | XR_VIEW_STATE_POSITION_VALID_BIT |
                            XR_VIEW_STATE_ORIENTATION_TRACKED_BIT | XR_VIEW_STATE_POSITION_TRACKED_BIT;
    *count = 2;
    for(uint32_t i = 0; i < capacity && i < 2; i++)
    {
        views[i].pose.orientation = {0.f, 0.f, 0.f, 1.f};
        views[i].pose.position = {i == 0 ? -0.032f : 0.032f, 1.7f, 0.f};
        views[i].fov = {-0.785f, 0.785f, 0.785f, -0.785f};
    }
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL stringToPath(XrInstance, const char* text, XrPath* path)
{
    for(int i = 0; i < hs.pathCount; i++)
    {
        if(!strcmp(hs.paths[i], text))
        {
            *path = static_cast<XrPath>(i + 1);
            return XR_SUCCESS;
        }
    }
    if(hs.pathCount >= 512)
    {
        return XR_ERROR_PATH_COUNT_EXCEEDED;
    }
    copyText(hs.paths[hs.pathCount], text);
    *path = static_cast<XrPath>(++hs.pathCount);
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL pathToString(XrInstance, XrPath path, uint32_t capacity, uint32_t* count, char* buffer)
{
    if(path == XR_NULL_PATH || path > static_cast<XrPath>(hs.pathCount))
    {
        return XR_ERROR_PATH_INVALID;
    }
    const char* text = hs.paths[path - 1];
    *count = static_cast<uint32_t>(strlen(text) + 1);
    if(capacity >= *count)
    {
        strcpy(buffer, text);
    }
    return XR_SUCCESS;
}

// FAKEXR_REJECT_PATH's entries: one turning this binding of this profile down.
[[nodiscard]] bool rejected(const char* profile, const char* binding)
{
    char list[1024] = "";
    if(!envText("FAKEXR_REJECT_PATH", list, sizeof(list)) || !list[0])
    {
        return false;
    }
    char* context = nullptr;
    for(char* entry = strtok_s(list, ";", &context); entry; entry = strtok_s(nullptr, ";", &context))
    {
        char* path = entry;
        if(char* eq = strchr(entry, '='))
        {
            *eq = '\0';
            path = eq + 1;
            if(!strstr(profile, entry))
            {
                continue;
            }
        }
        if(path[0] && strstr(binding, path))
        {
            return true;
        }
    }
    return false;
}

// Logged with its profile and binding count; FAKEXR_REJECT_PATH turns down one holding a path it names. Accepted, it
// replaces the profile's last (as the specification has it).
XRAPI_ATTR XrResult XRAPI_CALL suggestBindings(XrInstance instance, const XrInteractionProfileSuggestedBinding* info)
{
    char profile[256] = "?";
    uint32_t len = 0;
    pathToString(instance, info->interactionProfile, sizeof(profile), &len, profile);
    for(uint32_t i = 0; i < info->countSuggestedBindings; i++)
    {
        char binding[256] = "";
        pathToString(instance, info->suggestedBindings[i].binding, sizeof(binding), &len, binding);
        if(rejected(profile, binding))
        {
            char text[600];
            snprintf(text, sizeof(text), "xrSuggestInteractionProfileBindings %s: %s turned down", profile, binding);
            log(text);
            return XR_ERROR_PATH_UNSUPPORTED;
        }
    }
    char text[400];
    snprintf(text, sizeof(text), "xrSuggestInteractionProfileBindings %s: %u bindings", profile,
        info->countSuggestedBindings);
    log(text);

    Headset::Suggestion* kept = nullptr;
    for(int i = 0; i < hs.suggestionCount && !kept; i++)
    {
        kept = hs.suggestions[i].profile == info->interactionProfile ? &hs.suggestions[i] : nullptr;
    }
    if(!kept && hs.suggestionCount < 24)
    {
        kept = &hs.suggestions[hs.suggestionCount++];
    }
    if(kept)
    {
        kept->profile = info->interactionProfile;
        kept->count = info->countSuggestedBindings < 128 ? info->countSuggestedBindings : 128;
        for(uint32_t i = 0; i < kept->count; i++)
        {
            kept->bindings[i] = info->suggestedBindings[i].binding;
        }
    }
    return XR_SUCCESS;
}

// The profile the hands are played as (FAKEXR_PROFILE, or VDXR's order), logged with its bindings; the game told.
XRAPI_ATTR XrResult XRAPI_CALL attachSessionActionSets(XrSession, const XrSessionActionSetsAttachInfo*)
{
    char wanted[256] = "";
    envText("FAKEXR_PROFILE", wanted, sizeof(wanted));
    const char* order[] = {wanted, "/interaction_profiles/oculus/touch_controller",
        "/interaction_profiles/microsoft/motion_controller", "/interaction_profiles/valve/index_controller",
        "/interaction_profiles/htc/vive_controller", "/interaction_profiles/khr/simple_controller"};
    const Headset::Suggestion* chosen = nullptr;
    for(const char* name : order)
    {
        for(int i = 0; name[0] && i < hs.suggestionCount && !chosen; i++)
        {
            char profile[256] = "";
            uint32_t len = 0;
            pathToString(fakeInstance, hs.suggestions[i].profile, sizeof(profile), &len, profile);
            const bool match = name == wanted ? strstr(profile, name) != nullptr : !strcmp(profile, name);
            chosen = match ? &hs.suggestions[i] : nullptr;
        }
        if(chosen)
        {
            break;
        }
    }
    hs.current = chosen ? chosen->profile : XR_NULL_PATH;
    char text[8192];
    uint32_t len = 0;
    char profile[256] = "none";
    if(chosen)
    {
        pathToString(fakeInstance, chosen->profile, sizeof(profile), &len, profile);
    }
    int at = snprintf(text, sizeof(text), "xrAttachSessionActionSets: the hands played as %s, bound:", profile);
    for(uint32_t i = 0; chosen && i < chosen->count && at > 0 && at < static_cast<int>(sizeof(text)) - 140; i++)
    {
        char binding[128] = "";
        pathToString(fakeInstance, chosen->bindings[i], sizeof(binding), &len, binding);
        at += snprintf(text + at, sizeof(text) - at, " %s", binding);
    }
    log(text);
    if(chosen)
    {
        pushState(profileChangedEvent);
    }
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL createActionSet(XrInstance, const XrActionSetCreateInfo*, XrActionSet* set)
{
    *set = newHandle<XrActionSet>();
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL createAction(XrActionSet, const XrActionCreateInfo*, XrAction* action)
{
    *action = newHandle<XrAction>();
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL syncActions(XrSession, const XrActionsSyncInfo*)
{
    return hs.state == XR_SESSION_STATE_FOCUSED ? XR_SUCCESS : XR_SESSION_NOT_FOCUSED;
}

XRAPI_ATTR XrResult XRAPI_CALL getActionStateBoolean(XrSession, const XrActionStateGetInfo*, XrActionStateBoolean* state)
{
    state->isActive = XR_FALSE;
    state->currentState = XR_FALSE;
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL getActionStateFloat(XrSession, const XrActionStateGetInfo*, XrActionStateFloat* state)
{
    state->isActive = XR_FALSE;
    state->currentState = 0.f;
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL getActionStateVector2f(XrSession, const XrActionStateGetInfo*, XrActionStateVector2f* state)
{
    state->isActive = XR_FALSE;
    state->currentState = {0.f, 0.f};
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL getCurrentInteractionProfile(XrSession, XrPath, XrInteractionProfileState* state)
{
    state->interactionProfile = hs.current;
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL resultToString(XrInstance /* instance */, XrResult value, char buffer[XR_MAX_RESULT_STRING_SIZE])
{
    snprintf(buffer, XR_MAX_RESULT_STRING_SIZE, "XR_RESULT_%d", static_cast<int>(value));
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL getInstanceProcAddr(XrInstance instance, const char* name, PFN_xrVoidFunction* function);

XRAPI_ATTR XrResult XRAPI_CALL unsupported()
{
    log("an unsupported function called");
    return XR_ERROR_FUNCTION_UNSUPPORTED;
}

struct Entry
{
    const char* name;
    PFN_xrVoidFunction function;
};

const Entry entries[] = {
    {"xrGetInstanceProcAddr", reinterpret_cast<PFN_xrVoidFunction>(getInstanceProcAddr)},
    {"xrEnumerateInstanceExtensionProperties", reinterpret_cast<PFN_xrVoidFunction>(enumerateInstanceExtensionProperties)},
    {"xrCreateInstance", reinterpret_cast<PFN_xrVoidFunction>(createInstance)},
    {"xrCreateDebugUtilsMessengerEXT", reinterpret_cast<PFN_xrVoidFunction>(createDebugUtilsMessenger)},
    {"xrDestroyDebugUtilsMessengerEXT", reinterpret_cast<PFN_xrVoidFunction>(destroyDebugUtilsMessenger)},
    {"xrDestroyInstance", reinterpret_cast<PFN_xrVoidFunction>(destroyInstance)},
    {"xrGetInstanceProperties", reinterpret_cast<PFN_xrVoidFunction>(getInstanceProperties)},
    {"xrGetSystem", reinterpret_cast<PFN_xrVoidFunction>(getSystem)},
    {"xrResultToString", reinterpret_cast<PFN_xrVoidFunction>(resultToString)},
};

// The fake headset's (FAKEXR_HEADSET).
const Entry headsetEntries[] = {
    {"xrGetSystemProperties", reinterpret_cast<PFN_xrVoidFunction>(getSystemProperties)},
    {"xrGetOpenGLGraphicsRequirementsKHR", reinterpret_cast<PFN_xrVoidFunction>(getOpenGLGraphicsRequirements)},
    {"xrCreateSession", reinterpret_cast<PFN_xrVoidFunction>(createSession)},
    {"xrDestroySession", reinterpret_cast<PFN_xrVoidFunction>(destroySession)},
    {"xrBeginSession", reinterpret_cast<PFN_xrVoidFunction>(beginSession)},
    {"xrEndSession", reinterpret_cast<PFN_xrVoidFunction>(endSession)},
    {"xrPollEvent", reinterpret_cast<PFN_xrVoidFunction>(pollEvent)},
    {"xrEnumerateViewConfigurationViews", reinterpret_cast<PFN_xrVoidFunction>(enumerateViewConfigurationViews)},
    {"xrEnumerateSwapchainFormats", reinterpret_cast<PFN_xrVoidFunction>(enumerateSwapchainFormats)},
    {"xrCreateSwapchain", reinterpret_cast<PFN_xrVoidFunction>(createSwapchain)},
    {"xrDestroySwapchain", reinterpret_cast<PFN_xrVoidFunction>(destroySwapchain)},
    {"xrEnumerateSwapchainImages", reinterpret_cast<PFN_xrVoidFunction>(enumerateSwapchainImages)},
    {"xrAcquireSwapchainImage", reinterpret_cast<PFN_xrVoidFunction>(acquireSwapchainImage)},
    {"xrWaitSwapchainImage", reinterpret_cast<PFN_xrVoidFunction>(waitSwapchainImage)},
    {"xrReleaseSwapchainImage", reinterpret_cast<PFN_xrVoidFunction>(releaseSwapchainImage)},
    {"xrWaitFrame", reinterpret_cast<PFN_xrVoidFunction>(waitFrame)},
    {"xrBeginFrame", reinterpret_cast<PFN_xrVoidFunction>(beginFrame)},
    {"xrEndFrame", reinterpret_cast<PFN_xrVoidFunction>(endFrame)},
    {"xrEnumerateReferenceSpaces", reinterpret_cast<PFN_xrVoidFunction>(enumerateReferenceSpaces)},
    {"xrCreateReferenceSpace", reinterpret_cast<PFN_xrVoidFunction>(createReferenceSpace)},
    {"xrCreateActionSpace", reinterpret_cast<PFN_xrVoidFunction>(createActionSpace)},
    {"xrDestroySpace", reinterpret_cast<PFN_xrVoidFunction>(succeed)},
    {"xrLocateSpace", reinterpret_cast<PFN_xrVoidFunction>(locateSpace)},
    {"xrLocateViews", reinterpret_cast<PFN_xrVoidFunction>(locateViews)},
    {"xrStringToPath", reinterpret_cast<PFN_xrVoidFunction>(stringToPath)},
    {"xrPathToString", reinterpret_cast<PFN_xrVoidFunction>(pathToString)},
    {"xrCreateActionSet", reinterpret_cast<PFN_xrVoidFunction>(createActionSet)},
    {"xrDestroyActionSet", reinterpret_cast<PFN_xrVoidFunction>(succeed)},
    {"xrCreateAction", reinterpret_cast<PFN_xrVoidFunction>(createAction)},
    {"xrDestroyAction", reinterpret_cast<PFN_xrVoidFunction>(succeed)},
    {"xrSuggestInteractionProfileBindings", reinterpret_cast<PFN_xrVoidFunction>(suggestBindings)},
    {"xrAttachSessionActionSets", reinterpret_cast<PFN_xrVoidFunction>(attachSessionActionSets)},
    {"xrSyncActions", reinterpret_cast<PFN_xrVoidFunction>(syncActions)},
    {"xrGetActionStateBoolean", reinterpret_cast<PFN_xrVoidFunction>(getActionStateBoolean)},
    {"xrGetActionStateFloat", reinterpret_cast<PFN_xrVoidFunction>(getActionStateFloat)},
    {"xrGetActionStateVector2f", reinterpret_cast<PFN_xrVoidFunction>(getActionStateVector2f)},
    {"xrApplyHapticFeedback", reinterpret_cast<PFN_xrVoidFunction>(succeed)},
    {"xrGetCurrentInteractionProfile", reinterpret_cast<PFN_xrVoidFunction>(getCurrentInteractionProfile)},
};

XRAPI_ATTR XrResult XRAPI_CALL getInstanceProcAddr(XrInstance /* instance */, const char* name, PFN_xrVoidFunction* function)
{
    for(const Entry& e : entries)
    {
        if(!strcmp(e.name, name))
        {
            *function = e.function;
            return XR_SUCCESS;
        }
    }
    for(const Entry& e : headsetEntries)
    {
        if(!strcmp(e.name, name))
        {
            *function = e.function;
            return XR_SUCCESS;
        }
    }
    // Any other function: one that fails (the loader wants every core function; the game calls none of them without
    // a system).
    *function = reinterpret_cast<PFN_xrVoidFunction>(unsupported);
    return XR_SUCCESS;
}

} // namespace

extern "C" FAKEXR_EXPORT XrResult XRAPI_CALL xrNegotiateLoaderRuntimeInterface(
    const XrNegotiateLoaderInfo* loaderInfo, XrNegotiateRuntimeRequest* request)
{
    if(!loaderInfo || !request || loaderInfo->minInterfaceVersion > XR_CURRENT_LOADER_RUNTIME_VERSION)
    {
        return XR_ERROR_INITIALIZATION_FAILED;
    }
    request->runtimeInterfaceVersion = XR_CURRENT_LOADER_RUNTIME_VERSION;
    request->runtimeApiVersion = XR_MAKE_VERSION(1, 0, 0);
    request->getInstanceProcAddr = getInstanceProcAddr;
    log("negotiated");
    return XR_SUCCESS;
}

namespace
{

// The copy's name: its file's, without the folder and the extension (fakexr_steam.dll, fakexr_steam.so).
void nameFromPath(const char* path)
{
    const char* slash = strrchr(path, '/');
#ifdef _WIN32
    const char* backslash = strrchr(path, '\\');
    slash = backslash && (!slash || backslash > slash) ? backslash : slash;
#endif
    copyText(moduleName, slash ? slash + 1 : path);
    if(char* dot = strrchr(moduleName, '.'))
    {
        *dot = '\0';
    }
}

} // namespace

#ifdef _WIN32
BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID /* reserved */)
{
    if(reason == DLL_PROCESS_ATTACH)
    {
        char path[MAX_PATH];
        GetModuleFileNameA(module, path, sizeof(path));
        nameFromPath(path);
        log("loaded");
    }
    else if(reason == DLL_PROCESS_DETACH)
    {
        log("unloaded");
    }
    return TRUE;
}
#else
__attribute__((constructor)) void fakexrLoaded()
{
    Dl_info info{};
    if(dladdr(reinterpret_cast<void*>(&fakexrLoaded), &info) && info.dli_fname)
    {
        nameFromPath(info.dli_fname);
    }
    log("loaded");
}

__attribute__((destructor)) void fakexrUnloaded()
{
    log("unloaded");
}
#endif
