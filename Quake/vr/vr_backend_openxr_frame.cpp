// vr_backend_openxr_frame.cpp -- the frame loop and the swapchains (vr_backend_openxr.cpp): xrWaitFrame/xrBeginFrame,
// the views and the head, the eye images, xrEndFrame and its layers, the calls timed, the hidden area, the swapchains
// made and their images.

#include "vr_backend_openxr_internal.hpp"

#ifdef QVR_HAVE_OPENXR

namespace qvr::oxr
{

bool OpenXrBackend::beginFrame(TrackingState& tracking, FrameState& frame)
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
    frameWaitMs = timeCall(CallWait, t0);
    profile::end();
    if(!checkFrameCall(waited, "xrWaitFrame"))
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
    QVR_TRACY_CALL("xrSyncActions", xrSyncActions(session, &syncInfo));
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
    XrResult located = XR_SUCCESS;
    QVR_TRACY_CALL("xrLocateViews", located = xrLocateViews(session, &locateInfo, &viewState, 2, &viewCount, views));
    const bool viewsOk =
        check(located, "xrLocateViews") &&
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
    bodytrack::xr::locate(worldSpace, time); // the tracked body, as the hands (vr_bodytrack_xr.cpp)

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

bool OpenXrBackend::frameActive() const
{
    return frameBegun && sessionRunning;
}

void OpenXrBackend::eyeResolution(int& width, int& height) const
{
    width = swapchains[0].width;
    height = swapchains[0].height;
}

EyeSizes OpenXrBackend::eyeSizes() const
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

const HiddenArea* OpenXrBackend::hiddenArea(int eye) const
{
    return getVisibilityMask ? &hidden[eye] : nullptr;
}

bool OpenXrBackend::runtimeMenuOpen() const
{
    return sessionRunning && sessionState == XR_SESSION_STATE_VISIBLE;
}

const char* OpenXrBackend::systemName() const
{
    return system;
}

unsigned OpenXrBackend::acquireEyeImage(int eye)
{
    return acquireImage(swapchains[eye]);
}

void OpenXrBackend::releaseEyeImage(int eye)
{
    const double t0 = Sys_DoubleTime();
    releaseImage(swapchains[eye]);
    timeCall(CallRelease, t0);
}

void OpenXrBackend::endFrame(bool rendered)
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
    xrstall::debugStall(); // (vr_debug_frame_stall_ms: as if the runtime held the frame)
    XrResult ended = XR_SUCCESS;
    QVR_TRACY_CALL("xrEndFrame", ended = xrEndFrame(session, &endInfo));
    checkFrameCall(ended, "xrEndFrame");
    const double endMs = timeCall(CallEnd, t0);
    timing.frames++;
    // Held frames (the runtime holding them, as at the 2026-10-17 drop): said once as they start and end.
    xrstall::frame(frameWaitMs, endMs, static_cast<double>(frameState.predictedDisplayPeriod) * 1e-6,
        stateName(sessionState));
    frameWaitMs = 0.0;
}

double OpenXrBackend::timeCall(Call call, double since)
{
    CallTime& c = timing.calls[call];
    const double ms = (Sys_DoubleTime() - since) * 1000.0;
    c.sum += ms;
    c.max = za::max(c.max, ms);
    c.count++;
    return ms;
}

bool OpenXrBackend::checkFrameCall(XrResult result, const char* what) const
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
    xrstall::callFailed(what, static_cast<int>(result), text);
    return false;
}

void OpenXrBackend::noteWaited()
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
        // (The desktop window's focus too: a driver's cap on a background window's presents showed as xrEndFrame's
        // time, 10 fps, the session FOCUSED.)
        line += va("; %s", stateName(sessionState));
        // The mirror's presents (GitHub issue #79: a present the display paced held the headset's frames).
        const MirrorPresents m = takeMirrorPresents();
        // (The swap interval as the driver reports it: 1 though the game set 0, the driver forces vsync.)
        line += va("; window %s, %d presents (swap %.2f/%.2f ms, interval %d), %d skipped",
            windowUnfocused() ? "unfocused" : "focused", m.presented, m.presented ? m.swapMsSum / m.presented : 0.0,
            m.swapMsMax, SDL_GL_GetSwapInterval(), m.skipped);
        if(m.capHz > 0.0)
        {
            line += va(" (at most %.0f Hz)", m.capHz);
        }
        line += "\n";
        xrruntime::logLine(line.cStr());
    }
    const XrTime lastDisplay = timing.lastDisplay;
    timing = Timing{};
    timing.since = timing.lastWaited = now;
    timing.lastDisplay = lastDisplay;
}

void OpenXrBackend::fetchHiddenArea(int eye)
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

bool OpenXrBackend::createSwapchain(Swapchain& sc, int32_t width, int32_t height, XrSwapchainUsageFlags usage, const char* what)
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
    bool offered = offeredFormats.empty(); // (none listed: the runtime's word is all there is)
    for(int64_t f : offeredFormats)
    {
        offered = offered || f == colorFormat;
    }
    const uint32_t maxW = maxImageWidth ? maxImageWidth : 16384u;
    const uint32_t maxH = maxImageHeight ? maxImageHeight : 16384u;
    if(!offered || width <= 0 || height <= 0 || info.width > maxW || info.height > maxH || runtimeFaulted)
    {
        xrruntime::warn("OpenXR: %s not requested: %dx%d %s (%s)\n", what, width, height, glFormatName(colorFormat),
            runtimeFaulted ? "the runtime crashed before"
            : !offered     ? "a format the runtime doesn't offer"
                           : va("a size outside 1..%ux%u", maxW, maxH));
        return false;
    }
    step("%s: requesting %dx%d %s (0x%llx), usage 0x%llx, %u sample, %u face, array %u, %u mip", what, width, height,
        glFormatName(colorFormat), static_cast<unsigned long long>(colorFormat), static_cast<unsigned long long>(usage),
        info.sampleCount, info.faceCount, info.arraySize, info.mipCount);
    if(vr_xr_test_drop_context.value != 0.f) // (the test of contextReady: no context current, once)
    {
        Cvar_SetValueQuick(&vr_xr_test_drop_context, 0.f);
#ifdef _WIN32
        wglMakeCurrent(nullptr, nullptr);
#else
        xrgl::release();
#endif
    }
    if(!contextReady(what))
    {
        return false;
    }
    // SteamVR checks glGetError after its own GL calls in here: an error the engine left
    // pending fails the swapchain ("SXR_GL_CHECK ... glGenTextures", GL_INVALID_VALUE).
    drainGlErrors();
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
    drainGlErrors();
    xrruntime::logLine(va("%s OpenXR: %s: %dx%d %s, %u images, %u sample, usage 0x%llx\n", wallClock(), what, width, height,
        glFormatName(colorFormat), imageCount, info.sampleCount, static_cast<unsigned long long>(usage)));
    return true;
}

const char* OpenXrBackend::glFormatName(int64_t format)
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

unsigned OpenXrBackend::acquireImage(const Swapchain& sc)
{
    XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
    uint32_t index = 0;
    double t0 = Sys_DoubleTime();
    XrResult acquired = XR_SUCCESS;
    QVR_TRACY_CALL("xrAcquireSwapchainImage", acquired = xrAcquireSwapchainImage(sc.handle, &acquireInfo, &index));
    timeCall(CallAcquire, t0);
    if(!check(acquired, "xrAcquireSwapchainImage"))
    {
        return 0;
    }

    XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
    waitInfo.timeout = XR_INFINITE_DURATION;
    t0 = Sys_DoubleTime();
    XrResult waited = XR_SUCCESS;
    QVR_TRACY_CALL("xrWaitSwapchainImage", waited = xrWaitSwapchainImage(sc.handle, &waitInfo));
    timeCall(CallWaitImage, t0);
    if(!check(waited, "xrWaitSwapchainImage"))
    {
        XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        xrReleaseSwapchainImage(sc.handle, &releaseInfo);
        return 0;
    }
    return sc.images[index].image;
}

bool OpenXrBackend::releaseImage(const Swapchain& sc) const
{
    XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    XrResult released = XR_SUCCESS;
    QVR_TRACY_CALL("xrReleaseSwapchainImage", released = xrReleaseSwapchainImage(sc.handle, &releaseInfo));
    return check(released, "xrReleaseSwapchainImage");
}

Pose OpenXrBackend::toPose(const XrPosef& p, bool valid)
{
    Pose pose;
    pose.position = {p.position.x, p.position.y, p.position.z};
    pose.orientation = glm::quat{p.orientation.w, p.orientation.x, p.orientation.y, p.orientation.z};
    pose.valid = valid;
    return pose;
}

Pose OpenXrBackend::locate(XrSpace space, XrTime time) const
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

bool OpenXrBackend::createSwapchains()
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
    return createEyeSwapchains();
}

bool OpenXrBackend::createEyeSwapchains()
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
                const XrSwapchain handle = sc.handle;
                guarded("xrDestroySwapchain", [handle] { return xrDestroySwapchain(handle); });
                sc = Swapchain{};
            }
        }
    };
    const auto createBoth = [this, w, h, &destroyBoth]
    {
        destroyBoth();
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

} // namespace qvr::oxr

#endif // QVR_HAVE_OPENXR
