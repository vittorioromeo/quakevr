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

#include <windows.h>
#include <unknwn.h> // IUnknown, which openxr_platform.h needs and lean Windows headers omit

#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_OPENGL
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#ifndef GL_SRGB8_ALPHA8
#define GL_SRGB8_ALPHA8 0x8C43
#endif

#include <cmath>
#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>

namespace qvr
{
namespace
{

// The OpenXR runtime to load (vr_xr_runtime), through the loader's XR_RUNTIME_JSON, set before it
// first runs. The installed runtimes are the manifests listed under
// HKLM\SOFTWARE\Khronos\OpenXR\1\AvailableRuntimes; Virtual Desktop's and SteamVR's are found by
// their file names there, or in their usual places. With vr_xr_runtime 0 the system's active runtime
// is used (or an XR_RUNTIME_JSON the game was started with).
[[nodiscard]] std::string installedRuntime(const char* fileName)
{
    HKEY key = nullptr;
    std::string found;
    if(RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Khronos\\OpenXR\\1\\AvailableRuntimes", 0, KEY_READ, &key) == ERROR_SUCCESS)
    {
        char name[1024];
        for(DWORD i = 0;; i++)
        {
            DWORD length = sizeof(name);
            if(RegEnumValueA(key, i, name, &length, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
            {
                break;
            }
            if(q_strcasestr(name, fileName))
            {
                found = name;
                break;
            }
        }
        RegCloseKey(key);
    }
    return found;
}

void chooseRuntime()
{
    static bool ours = false; // XR_RUNTIME_JSON is ours to change (not one the game was started with)
    static bool checked = false;
    if(!checked)
    {
        checked = true;
        ours = !getenv("XR_RUNTIME_JSON");
    }
    if(!ours)
    {
        Con_Printf("OpenXR: XR_RUNTIME_JSON is set outside the game (%s): vr_xr_runtime is ignored\n", getenv("XR_RUNTIME_JSON"));
        return;
    }

    std::string manifest;
    switch(static_cast<int>(vr_xr_runtime.value))
    {
        case 1:
            manifest = installedRuntime("virtualdesktop-openxr.json");
            if(manifest.empty())
            {
                manifest = "C:\\Program Files\\Virtual Desktop Streamer\\OpenXR\\virtualdesktop-openxr.json";
            }
            break;
        case 2:
            manifest = installedRuntime("steamxr_win64.json");
            if(manifest.empty())
            {
                manifest = "C:\\Program Files (x86)\\Steam\\steamapps\\common\\SteamVR\\steamxr_win64.json";
            }
            break;
        case 3: manifest = vr_xr_runtime_json.string; break;
        default: break;
    }

    if(manifest.empty())
    {
        SetEnvironmentVariableA("XR_RUNTIME_JSON", nullptr);
        _putenv_s("XR_RUNTIME_JSON", "");
        return;
    }
    if(GetFileAttributesA(manifest.c_str()) == INVALID_FILE_ATTRIBUTES)
    {
        Con_Warning("OpenXR: runtime manifest %s not found: using the system's runtime\n", manifest.c_str());
        SetEnvironmentVariableA("XR_RUNTIME_JSON", nullptr);
        _putenv_s("XR_RUNTIME_JSON", "");
        return;
    }
    Con_Printf("OpenXR: runtime %s\n", manifest.c_str());
    SetEnvironmentVariableA("XR_RUNTIME_JSON", manifest.c_str());
    _putenv_s("XR_RUNTIME_JSON", manifest.c_str());
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

    [[nodiscard]] bool start() override
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
        if(frameBegun)
        {
            endFrame(false);
        }

        for(Swapchain* sc : {&swapchains[0], &swapchains[1], &panel})
        {
            if(sc->handle != XR_NULL_HANDLE)
            {
                xrDestroySwapchain(sc->handle);
                *sc = Swapchain{};
            }
        }
        panelPending = panelShown = false;
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

        if(actionSet != XR_NULL_HANDLE)
        {
            xrDestroyActionSet(actionSet);
            actionSet = XR_NULL_HANDLE;
        }

        if(session != XR_NULL_HANDLE)
        {
            xrDestroySession(session);
            session = XR_NULL_HANDLE;
        }

        if(instance != XR_NULL_HANDLE)
        {
            xrDestroyInstance(instance);
            instance = XR_NULL_HANDLE;
        }

        if(sessionRunning || swapIntervalChanged)
        {
            SDL_GL_SetSwapInterval(Cvar_VariableValue("vid_vsync") != 0.f ? 1 : 0);
        }
        sessionRunning = false;
    }

    [[nodiscard]] bool beginFrame(TrackingState& tracking, FrameState& frame) override
    {
        frame.shouldRender = false;

        if(frameBegun)
        {
            endFrame(false); // the previous frame was not rendered (e.g. while loading)
        }

        if(!pollEvents())
        {
            return false;
        }

        // vr_render_scale: new eye images between frames (none is acquired now).
        if(session != XR_NULL_HANDLE && (requestedWidth != scaledWidth() || requestedHeight != scaledHeight()) &&
            !createEyeSwapchains())
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
        if(!check(xrWaitFrame(session, &waitInfo, &frameState), "xrWaitFrame"))
        {
            return true;
        }

        XrFrameBeginInfo beginInfo{XR_TYPE_FRAME_BEGIN_INFO};
        if(!check(xrBeginFrame(session, &beginInfo), "xrBeginFrame"))
        {
            return true;
        }
        frameBegun = true;

        const XrActiveActionSet active{actionSet, XR_NULL_PATH};
        XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
        syncInfo.countActiveActionSets = 1;
        syncInfo.activeActionSets = &active;
        xrSyncActions(session, &syncInfo);

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
        if(tracking.head.valid)
        {
            lastHead = tracking.head;
        }
        for(int h = 0; h < HAND_COUNT; h++)
        {
            const int side = handSide(h);
            tracking.hands[h] = locate(handSpaces[side], time);
            if(vr_controller_legacy_pose.value)
            {
                tracking.hands[h] = toLegacyPose(tracking.hands[h], side);
            }
        }

        frame.shouldRender = viewsOk && frameState.shouldRender;
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

    [[nodiscard]] unsigned acquireEyeImage(int eye) override
    {
        return acquireImage(swapchains[eye]);
    }

    void releaseEyeImage(int eye) override
    {
        releaseImage(swapchains[eye]);
    }

    void endFrame(bool rendered) override
    {
        if(!frameBegun)
        {
            return;
        }
        frameBegun = false;

        XrCompositionLayerProjectionView projViews[2]{};
        XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};

        XrFrameEndInfo endInfo{XR_TYPE_FRAME_END_INFO};
        endInfo.displayTime = frameState.predictedDisplayTime;
        endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;

        const XrCompositionLayerBaseHeader* submitted[2];
        uint32_t count = 0;

        if(rendered && frameState.shouldRender)
        {
            for(int eye = 0; eye < 2; eye++)
            {
                projViews[eye] = XrCompositionLayerProjectionView{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
                projViews[eye].pose = views[eye].pose;
                projViews[eye].fov = views[eye].fov;
                projViews[eye].subImage.swapchain = swapchains[eye].handle;
                projViews[eye].subImage.imageRect.extent = {swapchains[eye].width, swapchains[eye].height};
            }

            layer.space = worldSpace;
            layer.viewCount = 2;
            layer.views = projViews;
            submitted[count++] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&layer);
        }

        XrCompositionLayerQuad quad{XR_TYPE_COMPOSITION_LAYER_QUAD};
        if(panelPending && frameState.shouldRender)
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
            quad.size = {panelWidth, panelWidth * static_cast<float>(panel.height) / static_cast<float>(panel.width)};
            submitted[count++] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&quad);
        }
        panelShown = panelPending && frameState.shouldRender;
        panelPending = false;

        endInfo.layerCount = count;
        endInfo.layers = count ? submitted : nullptr;
        check(xrEndFrame(session, &endInfo), "xrEndFrame");
    }

    [[nodiscard]] unsigned acquirePanelImage(int width, int height) override
    {
        if(!frameBegun || panelPending || width <= 0 || height <= 0 || !ensurePanelSwapchain(width, height))
        {
            return 0;
        }
        return acquireImage(panel);
    }

    void releasePanelImage() override
    {
        panelPending = releaseImage(panel);
    }

private:
    // The panel: 1.6 m wide, 1.4 m in front of where the head faced when it appeared, upright.
    static constexpr float panelWidth = 1.6f;
    static constexpr float panelDistance = 1.4f;

    void placePanel()
    {
        // The head's forward (-z) direction, flattened.
        glm::vec3 fwd = lastHead.orientation * glm::vec3{0.f, 0.f, -1.f};
        fwd.y = 0.f;
        if(glm::length(fwd) < 1e-3f)
        {
            fwd = {0.f, 0.f, -1.f};
        }
        fwd = glm::normalize(fwd);

        const float yaw = std::atan2(-fwd.x, -fwd.z);
        const glm::quat orientation = glm::angleAxis(yaw, glm::vec3{0.f, 1.f, 0.f});

        panelPose.position = {lastHead.position.x + fwd.x * panelDistance, lastHead.position.y,
            lastHead.position.z + fwd.z * panelDistance};
        panelPose.orientation = {orientation.x, orientation.y, orientation.z, orientation.w};
    }

    bool ensurePanelSwapchain(int width, int height)
    {
        if(panel.handle != XR_NULL_HANDLE && panel.width == width && panel.height == height)
        {
            return true;
        }
        if(panel.handle != XR_NULL_HANDLE)
        {
            xrDestroySwapchain(panel.handle);
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
        std::vector<XrSwapchainImageOpenGLKHR> images;
    };

    XrInstance instance{XR_NULL_HANDLE};
    XrSystemId systemId{XR_NULL_SYSTEM_ID};
    XrSession session{XR_NULL_HANDLE};
    XrSpace worldSpace{XR_NULL_HANDLE};
    XrSpace viewSpace{XR_NULL_HANDLE};
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
    Pose lastHead; // the last valid head pose, which the panel is placed in front of
    XrSessionState sessionState{XR_SESSION_STATE_UNKNOWN};
    bool sessionRunning{false};
    bool swapIntervalChanged{false};
    bool frameBegun{false};
    XrFrameState frameState{XR_TYPE_FRAME_STATE};
    XrView views[2]{{XR_TYPE_VIEW}, {XR_TYPE_VIEW}};
    XrViewConfigurationView configViews[2]{{XR_TYPE_VIEW_CONFIGURATION_VIEW}, {XR_TYPE_VIEW_CONFIGURATION_VIEW}};
    bool visibilityMaskExtension{false}; // XR_KHR_visibility_mask enabled
    char runtime[XR_MAX_RUNTIME_NAME_SIZE + 32]{}; // its name and version
    bool vdxr{false};                             // Virtual Desktop's own runtime (VDXR)
    PFN_xrGetVisibilityMaskKHR getVisibilityMask{nullptr};
    HiddenArea hidden[2];
    int32_t requestedWidth{0}, requestedHeight{0}; // the eye size last asked for (vr_render_scale)
    bool hiddenStale[2]{true, true}; // to fetch (again) before the next frame

    // The eye images' size: the recommended one times vr_render_scale.
    [[nodiscard]] int32_t scaledWidth() const
    {
        return scaledEyeSize(static_cast<int>(configViews[0].recommendedImageRectWidth),
            static_cast<int>(configViews[0].maxImageRectWidth));
    }
    [[nodiscard]] int32_t scaledHeight() const
    {
        return scaledEyeSize(static_cast<int>(configViews[0].recommendedImageRectHeight),
            static_cast<int>(configViews[0].maxImageRectHeight));
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
        std::vector<XrVector2f> vertices(mask.vertexCountOutput);
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
        for(std::uint32_t& i : h.indices)
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
            h.vertices.emplace_back(vertices[i].x, vertices[i].y);
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
        Con_Warning("OpenXR: %s failed: %s (%d)\n", what, text, static_cast<int>(result));
        return false;
    }

    // A swapchain of width x height images in colorFormat.
    bool createSwapchain(Swapchain& sc, int32_t width, int32_t height, XrSwapchainUsageFlags usage, const char* what)
    {
        XrSwapchainCreateInfo info{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        info.usageFlags = usage;
        info.format = colorFormat;
        info.sampleCount = 1;
        info.width = static_cast<uint32_t>(width);
        info.height = static_cast<uint32_t>(height);
        info.faceCount = 1;
        info.arraySize = 1;
        info.mipCount = 1;
        if(!check(xrCreateSwapchain(session, &info, &sc.handle), what))
        {
            return false;
        }

        sc.width = width;
        sc.height = height;
        uint32_t imageCount = 0;
        xrEnumerateSwapchainImages(sc.handle, 0, &imageCount, nullptr);
        sc.images.assign(imageCount, XrSwapchainImageOpenGLKHR{XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR});
        xrEnumerateSwapchainImages(sc.handle, imageCount, &imageCount,
            reinterpret_cast<XrSwapchainImageBaseHeader*>(sc.images.data()));
        return true;
    }

    // The GL texture of the swapchain's next image, acquired and waited for; 0 on failure.
    [[nodiscard]] unsigned acquireImage(const Swapchain& sc) const
    {
        XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        uint32_t index = 0;
        if(!check(xrAcquireSwapchainImage(sc.handle, &acquireInfo, &index), "xrAcquireSwapchainImage"))
        {
            return 0;
        }

        XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
        waitInfo.timeout = XR_INFINITE_DURATION;
        if(!check(xrWaitSwapchainImage(sc.handle, &waitInfo), "xrWaitSwapchainImage"))
        {
            XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
            xrReleaseSwapchainImage(sc.handle, &releaseInfo);
            return 0;
        }
        return sc.images[index].image;
    }

    bool releaseImage(const Swapchain& sc) const
    {
        XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        return check(xrReleaseSwapchainImage(sc.handle, &releaseInfo), "xrReleaseSwapchainImage");
    }

    static void destroySpace(XrSpace& space)
    {
        if(space != XR_NULL_HANDLE)
        {
            xrDestroySpace(space);
            space = XR_NULL_HANDLE;
        }
    }

    // Tracked hand -> physical side: the main hand is the right one unless left-handed.
    [[nodiscard]] static int handSide(int hand)
    {
        const bool leftHanded = vr_lefthanded.value != 0.f;
        const bool main = hand == HAND_MAIN;
        return main != leftHanded ? 1 : 0;
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

    void updateControllers()
    {
        for(int side = 0; side < 2; side++)
        {
            XrInteractionProfileState state{XR_TYPE_INTERACTION_PROFILE_STATE};
            controller[side] = Controller::Other;
            if(!XR_SUCCEEDED(xrGetCurrentInteractionProfile(session, handPaths[side], &state)) ||
                state.interactionProfile == XR_NULL_PATH)
            {
                continue;
            }
            if(state.interactionProfile == path("/interaction_profiles/oculus/touch_controller") ||
                state.interactionProfile == path("/interaction_profiles/meta/touch_controller_plus"))
            {
                controller[side] = Controller::Touch;
            }
            else if(state.interactionProfile == path("/interaction_profiles/valve/index_controller"))
            {
                // Virtual Desktop's "Emulate Index controllers" reports Index controllers for what
                // are Quest controllers: VDXR's grip is Meta's Touch one whatever the profile.
                controller[side] = vdxr ? Controller::Touch : Controller::Index;
            }
        }
    }

    [[nodiscard]] Pose toLegacyPose(const Pose& grip, int side) const
    {
        if(!grip.valid || controller[side] == Controller::Other)
        {
            return grip;
        }

        const float s = side == 0 ? 1.f : -1.f;
        glm::vec3 offset;
        glm::vec3 xyz; // degrees
        if(controller[side] == Controller::Touch)
        {
            offset = {0.007f * s, -0.00182941f, 0.1019482f};
            xyz = {20.6f, 0.f, 0.f};
        }
        else
        {
            offset = {0.f, -0.015f, 0.13f};
            xyz = {15.392f, -2.071f * s, 0.303f * s};
        }
        const glm::quat r = glm::angleAxis(glm::radians(xyz.x), glm::vec3{1.f, 0.f, 0.f}) *
                            glm::angleAxis(glm::radians(xyz.y), glm::vec3{0.f, 1.f, 0.f}) *
                            glm::angleAxis(glm::radians(xyz.z), glm::vec3{0.f, 0.f, 1.f});

        // raw = grip * (T(offset) R)^-1 = grip * R^-1 T(-offset)
        const glm::quat rInv = glm::inverse(r);
        Pose raw = grip;
        raw.orientation = grip.orientation * rInv;
        raw.position = grip.position + grip.orientation * (rInv * -offset);
        if(grip.velocityValid)
        {
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

    bool createInstance()
    {
        chooseRuntime(); // before the loader first runs

        // Quest 3 controllers get their own profile with this extension (Touch otherwise).
        std::vector<const char*> extensions{XR_KHR_OPENGL_ENABLE_EXTENSION_NAME};
        uint32_t available = 0;
        xrEnumerateInstanceExtensionProperties(nullptr, 0, &available, nullptr);
        std::vector<XrExtensionProperties> extensionList(available, XrExtensionProperties{XR_TYPE_EXTENSION_PROPERTIES});
        xrEnumerateInstanceExtensionProperties(nullptr, available, &available, extensionList.data());
        for(const XrExtensionProperties& p : extensionList)
        {
            if(!strcmp(p.extensionName, "XR_META_touch_controller_plus"))
            {
                extensions.push_back("XR_META_touch_controller_plus");
            }
            // The lenses' hidden area (vr_visibility_mask).
            if(!strcmp(p.extensionName, XR_KHR_VISIBILITY_MASK_EXTENSION_NAME))
            {
                extensions.push_back(XR_KHR_VISIBILITY_MASK_EXTENSION_NAME);
                visibilityMaskExtension = true;
            }
        }

        XrInstanceCreateInfo info{XR_TYPE_INSTANCE_CREATE_INFO};
        q_strlcpy(info.applicationInfo.applicationName, "Quake VR", XR_MAX_APPLICATION_NAME_SIZE);
        info.applicationInfo.applicationVersion = 1;
        q_strlcpy(info.applicationInfo.engineName, "Ironwail", XR_MAX_ENGINE_NAME_SIZE);
        info.applicationInfo.engineVersion = 1;
        info.applicationInfo.apiVersion = XR_API_VERSION_1_0;
        info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        info.enabledExtensionNames = extensions.data();

        if(!check(xrCreateInstance(&info, &instance), "xrCreateInstance"))
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

        return true;
    }

    bool createSystem()
    {
        XrSystemGetInfo info{XR_TYPE_SYSTEM_GET_INFO};
        info.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
        if(!check(xrGetSystem(instance, &info, &systemId), "xrGetSystem (is the headset connected?)"))
        {
            return false;
        }

        // Required before creating an OpenGL session.
        PFN_xrGetOpenGLGraphicsRequirementsKHR getRequirements = nullptr;
        xrGetInstanceProcAddr(instance, "xrGetOpenGLGraphicsRequirementsKHR",
            reinterpret_cast<PFN_xrVoidFunction*>(&getRequirements));
        XrGraphicsRequirementsOpenGLKHR requirements{XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR};
        return getRequirements &&
               check(getRequirements(instance, systemId, &requirements), "xrGetOpenGLGraphicsRequirementsKHR");
    }

    bool createSession()
    {
        XrGraphicsBindingOpenGLWin32KHR binding{XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR};
        binding.hDC = wglGetCurrentDC();
        binding.hGLRC = wglGetCurrentContext();

        XrSessionCreateInfo info{XR_TYPE_SESSION_CREATE_INFO};
        info.next = &binding;
        info.systemId = systemId;
        if(!check(xrCreateSession(instance, &info, &session), "xrCreateSession"))
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
        std::vector<XrReferenceSpaceType> types(count);
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

    void suggest(const char* profile, std::initializer_list<Binding> bindings)
    {
        std::vector<XrActionSuggestedBinding> suggested;
        for(const Binding& b : bindings)
        {
            suggested.push_back({*b.action, path(b.path)});
        }

        XrInteractionProfileSuggestedBinding info{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
        info.interactionProfile = path(profile);
        info.suggestedBindings = suggested.data();
        info.countSuggestedBindings = static_cast<uint32_t>(suggested.size());
        if(!XR_SUCCEEDED(xrSuggestInteractionProfileBindings(instance, &info)))
        {
            Con_DPrintf("OpenXR: no bindings for %s\n", profile);
        }
    }

    // The actions are the controllers' physical controls; the game assigns them through key
    // bindings. Controllers without some control (Vive wands, WMR) map the closest one.
    void suggestBindings()
    {
#define QVR_L(p) "/user/hand/left/" p
#define QVR_R(p) "/user/hand/right/" p
#define QVR_BOTH(action, p) {&action, QVR_L(p)}, {&action, QVR_R(p)}

        suggest("/interaction_profiles/khr/simple_controller",
            {QVR_BOTH(gripPose, "input/grip/pose"), QVR_BOTH(triggerAction, "input/select/click"),
                QVR_BOTH(triggerValueAction, "input/select/click"),
                QVR_BOTH(menuAction, "input/menu/click"), QVR_BOTH(hapticAction, "output/haptic")});

        for(const char* touch : {"/interaction_profiles/oculus/touch_controller",
                "/interaction_profiles/meta/touch_controller_plus"})
        {
            suggest(touch,
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
                    {&thumbTouchAction, QVR_R("input/a/touch")}, {&thumbTouchAction, QVR_R("input/b/touch")}});
        }

        // No menu button: the left B opens the menu.
        suggest("/interaction_profiles/valve/index_controller",
            {QVR_BOTH(gripPose, "input/grip/pose"), QVR_BOTH(triggerAction, "input/trigger/value"),
                QVR_BOTH(gripAction, "input/squeeze/value"), QVR_BOTH(stickAction, "input/thumbstick"),
                QVR_BOTH(stickClickAction, "input/thumbstick/click"), QVR_BOTH(primaryAction, "input/a/click"),
                {&secondaryAction, QVR_R("input/b/click")}, {&menuAction, QVR_L("input/b/click")},
                QVR_BOTH(hapticAction, "output/haptic"), QVR_BOTH(triggerValueAction, "input/trigger/value"),
                QVR_BOTH(gripValueAction, "input/squeeze/value"),
                QVR_BOTH(thumbTouchAction, "input/thumbstick/touch"), QVR_BOTH(thumbTouchAction, "input/a/touch"),
                QVR_BOTH(thumbTouchAction, "input/b/touch"), QVR_BOTH(thumbTouchAction, "input/trackpad/touch")});

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
        }
    }

public:
    void haptic(int hand, float seconds, float frequency, float amplitude) override
    {
        if(!sessionRunning || hapticAction == XR_NULL_HANDLE)
        {
            return;
        }

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
        uint32_t viewCount = 0;
        xrEnumerateViewConfigurationViews(instance, systemId,
            XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &viewCount, nullptr);
        if(viewCount != 2)
        {
            Con_Warning("OpenXR: expected 2 views, got %u\n", viewCount);
            return false;
        }

        xrEnumerateViewConfigurationViews(instance, systemId,
            XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 2, &viewCount, configViews);
        Con_Printf("OpenXR: recommended %ux%u per eye, largest %ux%u\n", configViews[0].recommendedImageRectWidth,
            configViews[0].recommendedImageRectHeight, configViews[0].maxImageRectWidth, configViews[0].maxImageRectHeight);

        // Quake renders gamma-encoded colours: an sRGB swapchain written without sRGB
        // conversion hands them to the compositor unchanged.
        uint32_t formatCount = 0;
        xrEnumerateSwapchainFormats(session, 0, &formatCount, nullptr);
        std::vector<int64_t> formats(formatCount);
        xrEnumerateSwapchainFormats(session, formatCount, &formatCount, formats.data());

        int64_t format = formats.empty() ? GL_RGBA8 : formats[0];
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
        const char* formatName = format == GL_SRGB8_ALPHA8 ? "GL_SRGB8_ALPHA8"
            : format == GL_RGBA8                           ? "GL_RGBA8"
            : format == 0x8C41                             ? "GL_SRGB8"
            : format == 0x881A                             ? "GL_RGBA16F"
            : format == 0x8059                             ? "GL_RGB10_A2"
                                                           : "?";
        static bool formatLogged = false;
        if(!formatLogged)
        {
            formatLogged = true;
            Con_Printf("OpenXR: swapchain format %s (0x%llx)\n", formatName, static_cast<unsigned long long>(format));
            if(format != GL_SRGB8_ALPHA8 && format != 0x8C41)
            {
                Con_Warning("OpenXR: the swapchain is not sRGB: the headset will show the game brighter and paler than it is\n");
            }
        }
        return createEyeSwapchains();
    }

    // The eyes' swapchains (again), at the recommended size times vr_render_scale; at the
    // recommended size if that fails.
    bool createEyeSwapchains()
    {
        requestedWidth = scaledWidth();
        requestedHeight = scaledHeight();
        for(int attempt = 0; attempt < 2; attempt++)
        {
            const int32_t w = attempt == 0 ? scaledWidth() : static_cast<int32_t>(configViews[0].recommendedImageRectWidth);
            const int32_t h = attempt == 0 ? scaledHeight() : static_cast<int32_t>(configViews[0].recommendedImageRectHeight);
            bool ok = true;
            for(Swapchain& sc : swapchains)
            {
                if(sc.handle != XR_NULL_HANDLE)
                {
                    xrDestroySwapchain(sc.handle);
                    sc = Swapchain{};
                }
                ok = ok && createSwapchain(sc, w, h, XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT, "xrCreateSwapchain");
            }
            if(ok)
            {
                Con_Printf("OpenXR: %dx%d per eye (vr_render_scale %g)\n", w, h, vr_render_scale.value);
                return true;
            }
        }
        return false;
    }

    void beginSession()
    {
        XrSessionBeginInfo beginInfo{XR_TYPE_SESSION_BEGIN_INFO};
        beginInfo.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        sessionRunning = check(xrBeginSession(session, &beginInfo), "xrBeginSession");
    }

    // Returns false if the session is lost or the application should exit VR.
    bool pollEvents()
    {
        XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
        while(xrPollEvent(instance, &event) == XR_SUCCESS)
        {
            switch(event.type)
            {
                case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING: return false;
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
                    if(sessionState == XR_SESSION_STATE_READY)
                    {
                        beginSession();
                    }
                    else if(sessionState == XR_SESSION_STATE_STOPPING)
                    {
                        xrEndSession(session);
                        sessionRunning = false;
                    }
                    else if(sessionState == XR_SESSION_STATE_EXITING ||
                            sessionState == XR_SESSION_STATE_LOSS_PENDING)
                    {
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

std::unique_ptr<Backend> makeOpenXrBackend()
{
    return std::make_unique<OpenXrBackend>();
}

} // namespace qvr

#else

namespace qvr
{

std::unique_ptr<Backend> makeOpenXrBackend()
{
    return nullptr;
}

} // namespace qvr

#endif
