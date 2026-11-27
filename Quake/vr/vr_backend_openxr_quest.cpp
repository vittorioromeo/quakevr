// vr_backend_openxr_quest.cpp -- the Quest's runtime extensions (QuestExtensions, vr_backend_openxr_internal.hpp):
// the display's refresh rate (XR_FB_display_refresh_rate), the CPU and GPU levels (XR_EXT_performance_settings) and the
// eye swapchains' fixed foveation (XR_FB_foveation, XR_FB_foveation_configuration, XR_FB_swapchain_update_state).
// Requested on Android alone, each when the runtime offers it (docs/vr-port/QUEST_PLAN.md, C4a); on the PC every
// function here finds nothing enabled and does nothing.

#include "vr_backend_openxr_internal.hpp"
#include "Zancle/Base/Strcmp.hpp"

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

// vr_xr_foveation 4, High (Top): Meta's level past XR_FOVEATION_LEVEL_HIGH_FB (questfovmusic); where the runtime refuses
// it, High with the pattern this many degrees lower (verticalOffset: positive up), the top reduced more.
constexpr int highTopLevel = 4;
constexpr float highTopFallbackOffset = -10.f;

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
    if(!ZA_STRCMP(offered, XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME))
    {
        enabled.pushBack(XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME);
        refreshRate = true;
    }
    else if(!ZA_STRCMP(offered, XR_EXT_PERFORMANCE_SETTINGS_EXTENSION_NAME))
    {
        enabled.pushBack(XR_EXT_PERFORMANCE_SETTINGS_EXTENSION_NAME);
        perfSettings = true;
    }
    else if(!ZA_STRCMP(offered, XR_FB_FOVEATION_EXTENSION_NAME))
    {
        enabled.pushBack(XR_FB_FOVEATION_EXTENSION_NAME);
        foveationParts |= foveationBit;
    }
    else if(!ZA_STRCMP(offered, XR_FB_FOVEATION_CONFIGURATION_EXTENSION_NAME))
    {
        enabled.pushBack(XR_FB_FOVEATION_CONFIGURATION_EXTENSION_NAME);
        foveationParts |= foveationConfigurationBit;
    }
    else if(!ZA_STRCMP(offered, XR_FB_SWAPCHAIN_UPDATE_STATE_EXTENSION_NAME))
    {
        enabled.pushBack(XR_FB_SWAPCHAIN_UPDATE_STATE_EXTENSION_NAME);
        foveationParts |= swapchainUpdateStateBit;
    }
    else if(!ZA_STRCMP(offered, XR_FB_SPACE_WARP_EXTENSION_NAME))
    {
        enabled.pushBack(XR_FB_SPACE_WARP_EXTENSION_NAME);
        spaceWarp = true;
    }
    foveation = foveationParts == foveationAll;
}

void QuestExtensions::spaceWarpProperties(XrInstance instance, XrSystemId system)
{
    if(spaceWarp)
    {
        XrSystemSpaceWarpPropertiesFB props{XR_TYPE_SYSTEM_SPACE_WARP_PROPERTIES_FB};
        XrSystemProperties properties{XR_TYPE_SYSTEM_PROPERTIES};
        properties.next = &props;
        spaceWarp = XR_SUCCEEDED(xrGetSystemProperties(instance, system, &properties)) &&
                    props.recommendedMotionVectorImageRectWidth > 0 && props.recommendedMotionVectorImageRectHeight > 0;
        spaceWarpWidth = spaceWarp ? static_cast<int>(props.recommendedMotionVectorImageRectWidth) : 0;
        spaceWarpHeight = spaceWarp ? static_cast<int>(props.recommendedMotionVectorImageRectHeight) : 0;
    }
    if(!questRuntime)
    {
        return; // (the PC's runtimes: never asked for)
    }
    Con_Printf("VR: SpaceWarp (XR_FB_space_warp): %s\n",
        spaceWarp ? va("supported by the runtime (motion vectors %dx%d an eye; vr_xr_spacewarp %g)", spaceWarpWidth,
                        spaceWarpHeight, static_cast<double>(vr_xr_spacewarp.value))
                  : "not supported by this runtime (the setting does nothing)");
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

const void* QuestExtensions::swapchainNext(XrSwapchainCreateInfoFoveationFB& storage)
{
    // questcrash: capable only with a level to apply (Off: plain images, as before qfov3; apply makes new swapchains
    // when the level crosses 0)
    eyesCapable = foveation && vr_xr_foveation.value > 0.f;
    if(!eyesCapable)
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

    // The fixed foveation, on both eye swapchains (again for new ones: a resized eye image). No longer behind
    // -xrfoveation (qfov3): C4p's crashes in xrEndFrame were the foveated images changed in use and form 2's framebuffers
    // (qfov2 rebuilt them as Meta's samples); a change mid-game takes new swapchains.
    const float level = vr_xr_foveation.value;
    // vr_xr_foveation_dynamic (qfov): the runtime lowers the level below vr_xr_foveation's while the GPU has room
    // (XR_FOVEATION_DYNAMIC_LEVEL_ENABLED_FB); kept with the level as one value (+10).
    const bool dynamic = vr_xr_foveation_dynamic.value != 0.f && level > 0.f;
    const float wanted = level + (dynamic ? 10.f : 0.f);
    // questfovmusic: the pattern's vertical shift in degrees (XrFoveationLevelProfileCreateInfoFB::verticalOffset)
    const float offset = level > 0.f ? CLAMP(-30.f, vr_xr_foveation_offset.value, 30.f) : 0.f;
    const bool newEyes = eyes[0] != foveated[0] || eyes[1] != foveated[1];
    const bool differs = wanted != foveationApplied || offset != offsetApplied;
    if(foveation && eyes[0] != XR_NULL_HANDLE && eyes[1] != XR_NULL_HANDLE && (differs || newEyes))
    {
        // questcrash: a level over 0 needs capable images, Off plain ones: across 0, new swapchains (made for the level)
        if((level > 0.f) != eyesCapable || (!newEyes && foveationApplied >= 0.f))
        {
            wantNewEyes = true; // a change on swapchains in use: on new ones (C4p; the backend calls again with them)
            return;
        }
        const bool changed = differs;
        foveationApplied = wanted;
        offsetApplied = offset;
        foveated[0] = eyes[0];
        foveated[1] = eyes[1];
        if(!eyesCapable)
        {
            if(changed)
            {
                xrruntime::note("OpenXR: fixed foveation off (vr_xr_foveation 0): plain eye images\n");
            }
            return;
        }
        // questfovmusic: 4 High (Top), Meta's fifth level (OVRPlugin's and VrApi's HighTop, 4: High with more of the top
        // reduced, past Khronos' enum); a runtime that refuses it gets High with the pattern shifted down instead.
        const int wantedLevel = CLAMP(0, static_cast<int>(level), highTopLevel);
        XrFoveationLevelProfileCreateInfoFB levelInfo{XR_TYPE_FOVEATION_LEVEL_PROFILE_CREATE_INFO_FB};
        levelInfo.level = static_cast<XrFoveationLevelFB>(wantedLevel);
        levelInfo.verticalOffset = offset;
        levelInfo.dynamic = dynamic ? XR_FOVEATION_DYNAMIC_LEVEL_ENABLED_FB : XR_FOVEATION_DYNAMIC_DISABLED_FB;
        XrFoveationProfileCreateInfoFB profileInfo{XR_TYPE_FOVEATION_PROFILE_CREATE_INFO_FB};
        profileInfo.next = &levelInfo;
        XrFoveationProfileFB profile = XR_NULL_HANDLE;
        XrResult result = createFoveationProfile(session, &profileInfo, &profile);
        if(XR_FAILED(result) && wantedLevel == highTopLevel)
        {
            xrruntime::note("OpenXR: fixed foveation High (Top) refused (%d): High shifted down %g degrees instead\n",
                static_cast<int>(result), static_cast<double>(-highTopFallbackOffset));
            levelInfo.level = XR_FOVEATION_LEVEL_HIGH_FB;
            levelInfo.verticalOffset = offset + highTopFallbackOffset;
            profile = XR_NULL_HANDLE;
            result = createFoveationProfile(session, &profileInfo, &profile);
        }
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
        xrruntime::note("OpenXR: fixed foveation level %d%s, vertical offset %g (vr_xr_foveation): %s\n",
            static_cast<int>(levelInfo.level), dynamic ? ", dynamic" : "", static_cast<double>(levelInfo.verticalOffset),
            XR_SUCCEEDED(result) ? "set" : va("failed (%d)", static_cast<int>(result)));
    }
}

} // namespace qvr::oxr

#endif // QVR_HAVE_OPENXR
