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
//   FAKEXR_UNFOCUS        a-b: the session VISIBLE (not focused: as with SteamVR's dashboard) from xrWaitFrame a to b
//   FAKEXR_D3D11          copies that load d3d11.dll at xrCreateInstance and free it at xrDestroyInstance (as VDXR
//                         does), logging whether it is still loaded after (the game keeps it: vr_backend_openxr.cpp)
//
// Built by Misc/quakevr/fakexr/build.sh into <worktree>/scratch/fakexr (docs/vr-port/TESTING.md, "OpenXR runtime
// choice").

#include <windows.h>
#include <unknwn.h>

#include <GL/gl.h>

#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_OPENGL
#include <openxr/openxr.h>
#include <openxr/openxr_loader_negotiation.h>
#include <openxr/openxr_platform.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace
{

char moduleName[64] = "fakexr";

void log(const char* what)
{
    char path[1024];
    if(GetEnvironmentVariableA("FAKEXR_LOG", path, sizeof(path)) == 0)
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
    if(GetEnvironmentVariableA(variable, list, sizeof(list)) == 0)
    {
        return false;
    }
    for(char* token = strtok(list, ","); token; token = strtok(nullptr, ","))
    {
        if(!_stricmp(token, moduleName))
        {
            return true;
        }
    }
    return false;
}

HMODULE d3d11 = nullptr; // FAKEXR_D3D11

const XrInstance fakeInstance = reinterpret_cast<XrInstance>(static_cast<uintptr_t>(0x5eed));

XRAPI_ATTR XrResult XRAPI_CALL enumerateInstanceExtensionProperties(
    const char* layerName, uint32_t capacity, uint32_t* count, XrExtensionProperties* properties)
{
    if(layerName)
    {
        return XR_ERROR_API_LAYER_NOT_PRESENT;
    }
    *count = 1;
    if(capacity == 0)
    {
        return XR_SUCCESS;
    }
    strcpy(properties[0].extensionName, "XR_KHR_opengl_enable"); // (the game's graphics binding)
    properties[0].extensionVersion = 10;
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
    if(listed("FAKEXR_D3D11"))
    {
        d3d11 = LoadLibraryA("d3d11.dll");
        log(d3d11 ? "d3d11.dll loaded" : "d3d11.dll not loaded");
    }
    *instance = fakeInstance;
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL destroyInstance(XrInstance /* instance */)
{
    log("xrDestroyInstance");
    if(d3d11)
    {
        FreeLibrary(d3d11);
        d3d11 = nullptr;
        log(GetModuleHandleA("d3d11.dll") ? "d3d11.dll freed: still loaded" : "d3d11.dll freed: unloaded");
    }
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL getInstanceProperties(XrInstance /* instance */, XrInstanceProperties* properties)
{
    properties->runtimeVersion = XR_MAKE_VERSION(1, 0, 0);
    _snprintf_s(properties->runtimeName, XR_MAX_RUNTIME_NAME_SIZE, _TRUNCATE, "FakeXR %s", moduleName);
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
    uint32_t eyeWidth{400}, eyeHeight{440};
    int periodMs{0};
    int frames{0}, projections{0}, held{0}, quads{0}, empty{0}, focusLost{0};
    char paths[512][128]{};
    int pathCount{0};
};
Headset hs;

template <typename T>
T newHandle()
{
    return reinterpret_cast<T>(static_cast<uintptr_t>(hs.nextHandle++));
}

[[nodiscard]] XrTime nowNs()
{
    LARGE_INTEGER count, frequency;
    QueryPerformanceCounter(&count);
    QueryPerformanceFrequency(&frequency);
    return static_cast<XrTime>(static_cast<double>(count.QuadPart) * 1e9 / static_cast<double>(frequency.QuadPart));
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
    strcpy(properties->systemName, "FakeXR headset");
    properties->graphicsProperties.maxLayerCount = 16;
    properties->graphicsProperties.maxSwapchainImageWidth = 8192;
    properties->graphicsProperties.maxSwapchainImageHeight = 8192;
    properties->trackingProperties.orientationTracking = XR_TRUE;
    properties->trackingProperties.positionTracking = XR_TRUE;
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
    if(GetEnvironmentVariableA("FAKEXR_EYE", text, sizeof(text)))
    {
        sscanf(text, "%ux%u", &hs.eyeWidth, &hs.eyeHeight);
    }
    if(GetEnvironmentVariableA("FAKEXR_UNFOCUS", text, sizeof(text)))
    {
        sscanf(text, "%d-%d", &hs.unfocusFrom, &hs.unfocusTo);
    }
    if(GetEnvironmentVariableA("FAKEXR_PERIOD_MS", text, sizeof(text)))
    {
        hs.periodMs = atoi(text);
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
    _snprintf_s(text, sizeof(text), _TRUNCATE,
        "xrDestroySession: %d frames: %d projection layers (%d without an image released since the last frame), "
        "%d quad layers, %d without layers; the focus lost %d times",
        hs.frames, hs.projections, hs.held, hs.quads, hs.empty, hs.focusLost);
    log(text);
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
    pushState(XR_SESSION_STATE_IDLE);
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL pollEvent(XrInstance, XrEventDataBuffer* buffer)
{
    if(hs.queueHead == hs.queueTail)
    {
        return XR_EVENT_UNAVAILABLE;
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
        // In the game's context (current on this thread), its bindings kept.
        GLint texture = 0, unpack = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
        glGetIntegerv(glPixelUnpackBufferBinding, &unpack);
        using BindBuffer = void(APIENTRY*)(GLenum, GLuint);
        const auto bindBuffer = reinterpret_cast<BindBuffer>(wglGetProcAddress("glBindBuffer"));
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
        _snprintf_s(text, sizeof(text), _TRUNCATE, "xrCreateSwapchain %ux%u format 0x%llx", info->width, info->height,
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
    if(hs.periodMs > 0)
    {
        Sleep(static_cast<DWORD>(hs.periodMs));
    }
    state->predictedDisplayPeriod = displayPeriod;
    state->predictedDisplayTime = nowNs() + displayPeriod;
    state->shouldRender = hs.state == XR_SESSION_STATE_VISIBLE || hs.state == XR_SESSION_STATE_FOCUSED;
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL beginFrame(XrSession, const XrFrameBeginInfo*)
{
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL endFrame(XrSession, const XrFrameEndInfo* info)
{
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
    strncpy_s(hs.paths[hs.pathCount], text, _TRUNCATE);
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
    state->interactionProfile = XR_NULL_PATH;
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL resultToString(XrInstance /* instance */, XrResult value, char buffer[XR_MAX_RESULT_STRING_SIZE])
{
    _snprintf_s(buffer, XR_MAX_RESULT_STRING_SIZE, _TRUNCATE, "XR_RESULT_%d", static_cast<int>(value));
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
    {"xrSuggestInteractionProfileBindings", reinterpret_cast<PFN_xrVoidFunction>(succeed)},
    {"xrAttachSessionActionSets", reinterpret_cast<PFN_xrVoidFunction>(succeed)},
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

extern "C" __declspec(dllexport) XrResult XRAPI_CALL xrNegotiateLoaderRuntimeInterface(
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

BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID /* reserved */)
{
    if(reason == DLL_PROCESS_ATTACH)
    {
        char path[MAX_PATH];
        GetModuleFileNameA(module, path, sizeof(path));
        const char* base = strrchr(path, '\\');
        base = base ? base + 1 : path;
        strncpy_s(moduleName, base, _TRUNCATE);
        if(char* dot = strrchr(moduleName, '.'))
        {
            *dot = '\0';
        }
        log("loaded");
    }
    else if(reason == DLL_PROCESS_DETACH)
    {
        log("unloaded");
    }
    return TRUE;
}
