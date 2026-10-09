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
#include "vr_profile.hpp"
#include "vr_xr_runtime.hpp"

#include "Zancle/Base/InitializerList.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/Vocabulary/UniquePtr.hpp"

#include <windows.h>
#include <unknwn.h> // IUnknown, which openxr_platform.h needs and lean Windows headers omit

#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_OPENGL
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#ifndef GL_SRGB8_ALPHA8
#define GL_SRGB8_ALPHA8 0x8C43
#endif

#include <string.h>

namespace qvr
{
namespace
{

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
            if(startRuntime())
            {
                xrruntime::setOutcome(plan, static_cast<int>(i));
                return true;
            }
            xrruntime::warn("OpenXR: %s failed to start\n", attempt.label.cStr());
            stop(); // (all of it: the next runtime starts from nothing)
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
                xrDestroySwapchain(sc->handle);
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
        frame.hold = false;

        if(frameBegun)
        {
            endFrame(false); // the previous frame was not rendered (e.g. while loading)
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
        if(tracking.head.valid)
        {
            lastHead = tracking.head;
        }
        for(int h = 0; h < HAND_COUNT; h++)
        {
            const int side = handSide(h);
            tracking.hands[h] = locate(handSpaces[side], time);
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

        frame.shouldRender = viewsOk && frameState.shouldRender;
        // The runtime's menu has the focus (SteamVR's dashboard...): its last frame again, nothing rendered.
        holding = frameState.shouldRender && haveLastViews && sessionState == XR_SESSION_STATE_VISIBLE &&
                  vr_xr_unfocused.value != 0.f;
        frame.hold = holding;
        if(!frameState.shouldRender)
        {
            timing.noRender++;
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

    [[nodiscard]] unsigned acquireEyeImage(int eye) override
    {
        return acquireImage(swapchains[eye]);
    }

    void releaseEyeImage(int eye) override
    {
        const double t0 = Sys_DoubleTime();
        releaseImage(swapchains[eye]);
        timeCall(CallRelease, t0);
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

        const bool held = rendered && holding && haveLastViews;
        holding = false;
        if(rendered && frameState.shouldRender)
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
                lastViews[eye] = projViews[eye];
            }
            haveLastViews = true;

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
            quad.size = {panelSize().x, panelSize().y};
            submitted[count++] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&quad);
            timing.panels++;
        }
        panelShown = panelPending && frameState.shouldRender;
        panelPending = false;

        endInfo.layerCount = count;
        endInfo.layers = count ? submitted : nullptr;
        const double t0 = Sys_DoubleTime();
        check(xrEndFrame(session, &endInfo), "xrEndFrame");
        timeCall(CallEnd, t0);
        timing.frames++;
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
    bool sessionRunning{false};
    bool swapIntervalChanged{false};
    bool frameBegun{false};
    XrFrameState frameState{XR_TYPE_FRAME_STATE};
    XrView views[2]{{XR_TYPE_VIEW}, {XR_TYPE_VIEW}};
    XrViewConfigurationView configViews[2]{{XR_TYPE_VIEW_CONFIGURATION_VIEW}, {XR_TYPE_VIEW_CONFIGURATION_VIEW}};
    bool visibilityMaskExtension{false}; // XR_KHR_visibility_mask enabled
    char runtime[XR_MAX_RUNTIME_NAME_SIZE + 32]{}; // its name and version
    bool vdxr{false};                             // Virtual Desktop's own runtime (VDXR)
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
        SYSTEMTIME t;
        GetLocalTime(&t);
        return va("%02d:%02d:%02d.%03d", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
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
        // SteamVR checks glGetError after its own GL calls in here: an error the engine left
        // pending fails the swapchain ("SXR_GL_CHECK ... glGenTextures", GL_INVALID_VALUE).
        for(int i = 0; i < 16 && glGetError() != GL_NO_ERROR; i++)
        {
        }
        if(!check(xrCreateSwapchain(session, &info, &sc.handle), what))
        {
            return false;
        }

        sc.width = width;
        sc.height = height;
        uint32_t imageCount = 0;
        xrEnumerateSwapchainImages(sc.handle, 0, &imageCount, nullptr);
        sc.images.clear();
        sc.images.resize(imageCount, XrSwapchainImageOpenGLKHR{XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR});
        xrEnumerateSwapchainImages(sc.handle, imageCount, &imageCount,
            reinterpret_cast<XrSwapchainImageBaseHeader*>(sc.images.data()));
        xrruntime::logLine(va("%s OpenXR: %s: %dx%d %s, %u images, %u sample, usage 0x%llx\n", wallClock(), what, width, height,
            glFormatName(colorFormat), imageCount, info.sampleCount, static_cast<unsigned long long>(usage)));
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
    [[nodiscard]] unsigned acquireImage(const Swapchain& sc)
    {
        XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        uint32_t index = 0;
        double t0 = Sys_DoubleTime();
        const XrResult acquired = xrAcquireSwapchainImage(sc.handle, &acquireInfo, &index);
        timeCall(CallAcquire, t0);
        if(!check(acquired, "xrAcquireSwapchainImage"))
        {
            return 0;
        }

        XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
        waitInfo.timeout = XR_INFINITE_DURATION;
        t0 = Sys_DoubleTime();
        const XrResult waited = xrWaitSwapchainImage(sc.handle, &waitInfo);
        timeCall(CallWaitImage, t0);
        if(!check(waited, "xrWaitSwapchainImage"))
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

    void updateControllers()
    {
        for(int side = 0; side < 2; side++)
        {
            XrInteractionProfileState state{XR_TYPE_INTERACTION_PROFILE_STATE};
            controller[side] = Controller::Other;
            if(!XR_SUCCEEDED(xrGetCurrentInteractionProfile(session, handPaths[side], &state)) ||
                state.interactionProfile == XR_NULL_PATH)
            {
                if(vr_debug_buttons.value)
                {
                    Con_Printf("VR buttons: the %s hand: no interaction profile (%s)\n", side == 0 ? "left" : "right", runtime);
                }
                continue;
            }
            if(vr_debug_buttons.value)
            {
                // (Which of suggestBindings' profiles the runtime took: its bindings are the buttons the game gets. VDXR
                // may report Index controllers for Quest ones: "Emulate Index controllers".)
                char name[XR_MAX_PATH_LENGTH]{};
                uint32_t len = 0;
                xrPathToString(instance, state.interactionProfile, sizeof(name), &len, name);
                Con_Printf("VR buttons: the %s hand: interaction profile %s (%s)\n", side == 0 ? "left" : "right", name, runtime);
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
        visibilityMaskExtension = false;
        runtime[0] = '\0';
        vdxr = false;
    }

    bool createInstance()
    {
        // Quest 3 controllers get their own profile with this extension (Touch otherwise).
        za::Vector<const char*> extensions{XR_KHR_OPENGL_ENABLE_EXTENSION_NAME};
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
            // The lenses' hidden area (vr_visibility_mask).
            if(!strcmp(p.extensionName, XR_KHR_VISIBILITY_MASK_EXTENSION_NAME))
            {
                extensions.pushBack(XR_KHR_VISIBILITY_MASK_EXTENSION_NAME);
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
        za::String enabled;
        for(const char* e : extensions)
        {
            enabled += enabled.empty() ? "" : " ";
            enabled += e;
        }
        xrruntime::logLine(va("OpenXR: extensions enabled: %s\n", enabled.cStr()));

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
        xrruntime::loaded(currentAttempt, runtime[0] ? runtime : "a runtime without a name");

        return true;
    }

    bool createSystem()
    {
        XrSystemGetInfo info{XR_TYPE_SYSTEM_GET_INFO};
        info.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
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
                Sleep(250);
                result = xrGetSystem(instance, &info, &systemId);
                tries++;
            }
            xrruntime::note("OpenXR: SteamVR %s after %.1f s (%d tries)\n",
                XR_SUCCEEDED(result) ? "found the headset" : "still has no headset", Sys_DoubleTime() - begin, tries);
        }
        if(!check(result, "xrGetSystem (is the headset connected?)"))
        {
            return false;
        }
        XrSystemProperties system{XR_TYPE_SYSTEM_PROPERTIES};
        if(XR_SUCCEEDED(xrGetSystemProperties(instance, systemId, &system)))
        {
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

    void suggest(const char* profile, za::InitializerList<Binding> bindings)
    {
        za::Vector<XrActionSuggestedBinding> suggested;
        for(const Binding& b : bindings)
        {
            suggested.pushBack({*b.action, path(b.path)});
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
                    {&thumbTouchAction, QVR_R("input/a/touch")}, {&thumbTouchAction, QVR_R("input/b/touch")},
                    QVR_BOTH(triggerTouchAction, "input/trigger/touch")});
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
        xrruntime::logLine(va("OpenXR: eyes: recommended %ux%u (%.1f Mpx), largest %ux%u, %u samples recommended\n",
            configViews[0].recommendedImageRectWidth, configViews[0].recommendedImageRectHeight,
            configViews[0].recommendedImageRectWidth * configViews[0].recommendedImageRectHeight * 1e-6,
            configViews[0].maxImageRectWidth, configViews[0].maxImageRectHeight, configViews[0].recommendedSwapchainSampleCount));

        // Quake renders gamma-encoded colours: an sRGB swapchain written without sRGB
        // conversion hands them to the compositor unchanged.
        uint32_t formatCount = 0;
        xrEnumerateSwapchainFormats(session, 0, &formatCount, nullptr);
        za::Vector<int64_t> formats(formatCount);
        xrEnumerateSwapchainFormats(session, formatCount, &formatCount, formats.data());

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
        return createEyeSwapchains();
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
        for(Swapchain& sc : swapchains)
        {
            if(sc.handle != XR_NULL_HANDLE)
            {
                xrDestroySwapchain(sc.handle);
                sc = Swapchain{};
            }
            if(!createSwapchain(sc, w, h, XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT, "xrCreateSwapchain"))
            {
                return false;
            }
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

za::UniquePtr<Backend> makeOpenXrBackend()
{
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
