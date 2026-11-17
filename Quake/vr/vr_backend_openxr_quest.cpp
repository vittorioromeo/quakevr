// vr_backend_openxr_quest.cpp -- the Quest's runtime extensions (QuestExtensions, vr_backend_openxr_internal.hpp):
// the display's refresh rate (XR_FB_display_refresh_rate), the CPU and GPU levels (XR_EXT_performance_settings) and the
// eye swapchains' fixed foveation (XR_FB_foveation, XR_FB_foveation_configuration, XR_FB_swapchain_update_state).
// Requested on Android alone, each when the runtime offers it (docs/vr-port/QUEST_PLAN.md, C4a); on the PC every
// function here finds nothing enabled and does nothing.

#include "vr_backend_openxr_internal.hpp"

#ifdef QVR_HAVE_OPENXR

namespace qvr::oxr
{
namespace
{

#ifdef __ANDROID__
constexpr bool questRuntime = true;
#else
constexpr bool questRuntime = false;
#endif

constexpr uint8_t foveationBit = 1;
constexpr uint8_t foveationConfigurationBit = 2;
constexpr uint8_t swapchainUpdateStateBit = 4;
constexpr uint8_t foveationAll = foveationBit | foveationConfigurationBit | swapchainUpdateStateBit;

template <typename F>
bool lookUp(XrInstance instance, const char* name, F& f)
{
    f = nullptr;
    xrGetInstanceProcAddr(instance, name, reinterpret_cast<PFN_xrVoidFunction*>(&f));
    return f != nullptr;
}

// vr_xr_cpu_level/vr_xr_gpu_level 1..4 as XR_EXT_performance_settings' levels (0: none).
constexpr XrPerfSettingsLevelEXT perfLevels[] = {XR_PERF_SETTINGS_LEVEL_POWER_SAVINGS_EXT,
    XR_PERF_SETTINGS_LEVEL_SUSTAINED_LOW_EXT, XR_PERF_SETTINGS_LEVEL_SUSTAINED_HIGH_EXT, XR_PERF_SETTINGS_LEVEL_BOOST_EXT};
constexpr const char* perfLevelNames[] = {"power savings", "sustained low", "sustained high", "boost"};

} // namespace

void QuestExtensions::want(const char* offered, za::Vector<const char*>& enabled)
{
    if(!questRuntime)
    {
        return;
    }
    if(!strcmp(offered, XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME))
    {
        enabled.pushBack(XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME);
        refreshRate = true;
    }
    else if(!strcmp(offered, XR_EXT_PERFORMANCE_SETTINGS_EXTENSION_NAME))
    {
        enabled.pushBack(XR_EXT_PERFORMANCE_SETTINGS_EXTENSION_NAME);
        perfSettings = true;
    }
    else if(!strcmp(offered, XR_FB_FOVEATION_EXTENSION_NAME))
    {
        enabled.pushBack(XR_FB_FOVEATION_EXTENSION_NAME);
        foveationParts |= foveationBit;
    }
    else if(!strcmp(offered, XR_FB_FOVEATION_CONFIGURATION_EXTENSION_NAME))
    {
        enabled.pushBack(XR_FB_FOVEATION_CONFIGURATION_EXTENSION_NAME);
        foveationParts |= foveationConfigurationBit;
    }
    else if(!strcmp(offered, XR_FB_SWAPCHAIN_UPDATE_STATE_EXTENSION_NAME))
    {
        enabled.pushBack(XR_FB_SWAPCHAIN_UPDATE_STATE_EXTENSION_NAME);
        foveationParts |= swapchainUpdateStateBit;
    }
    foveation = foveationParts == foveationAll;
}

void QuestExtensions::bind(XrInstance instance)
{
    refreshRate = refreshRate && lookUp(instance, "xrEnumerateDisplayRefreshRatesFB", enumerateRefreshRates) &&
                  lookUp(instance, "xrRequestDisplayRefreshRateFB", requestRefreshRate);
    perfSettings = perfSettings && lookUp(instance, "xrPerfSettingsSetPerformanceLevelEXT", setPerformanceLevel);
    foveation = foveation && lookUp(instance, "xrCreateFoveationProfileFB", createFoveationProfile) &&
                lookUp(instance, "xrDestroyFoveationProfileFB", destroyFoveationProfile) &&
                lookUp(instance, "xrUpdateSwapchainFB", updateSwapchain);
}

const void* QuestExtensions::swapchainNext(XrSwapchainCreateInfoFoveationFB& storage) const
{
    if(!foveation)
    {
        return nullptr;
    }
    storage = XrSwapchainCreateInfoFoveationFB{XR_TYPE_SWAPCHAIN_CREATE_INFO_FOVEATION_FB};
    // (OpenGL ES: the driver's scaled bins; 0, vr_xr_foveation_flags 0: Meta's GLES sample's, C4q)
    storage.flags = vr_xr_foveation_flags.value != 0.f ? XR_SWAPCHAIN_CREATE_FOVEATION_SCALED_BIN_BIT_FB : 0;
    return &storage;
}

void QuestExtensions::log(XrSession session) const
{
    if(!questRuntime)
    {
        return;
    }
    za::String rates;
    uint32_t count = 0;
    if(refreshRate && XR_SUCCEEDED(enumerateRefreshRates(session, 0, &count, nullptr)) && count > 0 && count <= 16)
    {
        float offered[16]{};
        if(XR_SUCCEEDED(enumerateRefreshRates(session, count, &count, offered)))
        {
            for(uint32_t i = 0; i < count; i++)
            {
                rates += va("%s%g", rates.empty() ? "" : " ", static_cast<double>(offered[i]));
            }
        }
    }
    xrruntime::logLine(va("OpenXR (Quest): refresh rate %s (%s Hz), performance levels %s, fixed foveation %s\n",
        refreshRate ? "on" : "not offered", rates.empty() ? "none listed" : rates.cStr(), perfSettings ? "on" : "not offered",
        foveation ? "on" : foveationParts ? "partly offered (off)" : "not offered"));
}

void QuestExtensions::apply(XrSession session, const XrSwapchain (&eyes)[2])
{
    if(!refreshRate && !perfSettings && !foveation)
    {
        return; // (the PC: always)
    }

    // The refresh rate: the nearest offered (0: the runtime's choice again, after one was asked for).
    const float refresh = vr_xr_refresh_rate.value;
    if(refreshRate && refresh != refreshApplied)
    {
        const bool asked = refreshApplied > 0.f;
        refreshApplied = refresh;
        float rate = 0.f;
        uint32_t count = 0;
        float offered[16]{};
        if(refresh > 0.f && XR_SUCCEEDED(enumerateRefreshRates(session, 0, &count, nullptr)) && count > 0 && count <= 16 &&
            XR_SUCCEEDED(enumerateRefreshRates(session, count, &count, offered)))
        {
            for(uint32_t i = 0; i < count; i++)
            {
                const float d = offered[i] - refresh;
                const float best = rate - refresh;
                rate = (rate == 0.f || d * d < best * best) ? offered[i] : rate;
            }
        }
        if(rate > 0.f || asked)
        {
            const XrResult result = requestRefreshRate(session, rate);
            xrruntime::note("OpenXR: refresh rate %g Hz requested (vr_xr_refresh_rate %g): %s\n", static_cast<double>(rate),
                static_cast<double>(refresh), XR_SUCCEEDED(result) ? "done" : va("failed (%d)", static_cast<int>(result)));
        }
        else
        {
            // (his adb session, 2026-11-17: a saved vid_refreshrate 90 ran at 72 with no line saying why)
            xrruntime::note("OpenXR: refresh rate left to the runtime (vr_xr_refresh_rate 0; 72 Hz unless the headset's "
                            "setting says otherwise); vid_refreshrate %g is the desktop window's and is ignored in the headset\n",
                static_cast<double>(Cvar_VariableValue("vid_refreshrate")));
        }
    }

    // The CPU and GPU levels (0: left as they are; the runtime's own from the next VR start).
    if(perfSettings)
    {
        const struct
        {
            float value;
            float& applied;
            XrPerfSettingsDomainEXT domain;
            const char* name;
        } domains[] = {{vr_xr_cpu_level.value, cpuApplied, XR_PERF_SETTINGS_DOMAIN_CPU_EXT, "CPU"},
            {vr_xr_gpu_level.value, gpuApplied, XR_PERF_SETTINGS_DOMAIN_GPU_EXT, "GPU"}};
        for(const auto& d : domains)
        {
            if(d.value == d.applied)
            {
                continue;
            }
            d.applied = d.value;
            const int level = static_cast<int>(d.value);
            if(level < 1 || level > 4)
            {
                continue;
            }
            const XrResult result = setPerformanceLevel(session, d.domain, perfLevels[level - 1]);
            xrruntime::note("OpenXR: %s level %s: %s\n", d.name, perfLevelNames[level - 1],
                XR_SUCCEEDED(result) ? "set" : va("failed (%d)", static_cast<int>(result)));
        }
    }

    // The fixed foveation, on both eye swapchains (again for new ones: a resized eye image).
    // Guarded (C4p, QUEST_PLAN.md 6w): with foveated eye images the Quest's runtime crashed in xrEndFrame within a few
    // hundred frames (set at launch or mid-game, drawn through the post-process or straight into them), so a level is
    // only applied when the game was launched with -xrfoveation (to try it).
    const bool allowed = COM_CheckParm("-xrfoveation") != 0;
    const float level = allowed ? vr_xr_foveation.value : 0.f;
    if(foveation && !allowed && vr_xr_foveation.value > 0.f && !foveationRefusedNoted)
    {
        foveationRefusedNoted = true;
        xrruntime::note("OpenXR: vr_xr_foveation %g not applied: it crashed the Quest's runtime (launch with -xrfoveation to "
                        "try it)\n", static_cast<double>(vr_xr_foveation.value));
    }
    const bool newEyes = eyes[0] != foveated[0] || eyes[1] != foveated[1];
    if(foveation && eyes[0] != XR_NULL_HANDLE && eyes[1] != XR_NULL_HANDLE &&
        (level != foveationApplied || (newEyes && level > 0.f)))
    {
        if(!newEyes && foveationApplied >= 0.f)
        {
            wantNewEyes = true; // a change on swapchains in use: on new ones (C4p; the backend calls again with them)
            return;
        }
        foveationApplied = level;
        foveated[0] = eyes[0];
        foveated[1] = eyes[1];
        XrFoveationLevelProfileCreateInfoFB levelInfo{XR_TYPE_FOVEATION_LEVEL_PROFILE_CREATE_INFO_FB};
        levelInfo.level = static_cast<XrFoveationLevelFB>(CLAMP(0, static_cast<int>(level), 3));
        levelInfo.verticalOffset = 0.f;
        levelInfo.dynamic = XR_FOVEATION_DYNAMIC_DISABLED_FB;
        XrFoveationProfileCreateInfoFB profileInfo{XR_TYPE_FOVEATION_PROFILE_CREATE_INFO_FB};
        profileInfo.next = &levelInfo;
        XrFoveationProfileFB profile = XR_NULL_HANDLE;
        XrResult result = createFoveationProfile(session, &profileInfo, &profile);
        for(int eye = 0; eye < 2 && XR_SUCCEEDED(result); eye++)
        {
            XrSwapchainStateFoveationFB state{XR_TYPE_SWAPCHAIN_STATE_FOVEATION_FB};
            state.profile = profile;
            result = updateSwapchain(eyes[eye], reinterpret_cast<const XrSwapchainStateBaseHeaderFB*>(&state));
        }
        if(profile != XR_NULL_HANDLE)
        {
            destroyFoveationProfile(profile); // (the swapchains keep its state)
        }
        xrruntime::note("OpenXR: fixed foveation level %d (vr_xr_foveation): %s\n", static_cast<int>(levelInfo.level),
            XR_SUCCEEDED(result) ? "set" : va("failed (%d)", static_cast<int>(result)));
    }
}

} // namespace qvr::oxr

#endif // QVR_HAVE_OPENXR
