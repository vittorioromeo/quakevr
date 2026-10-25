// vr_bodytrack_xr.cpp -- the OpenXR body tracker: XR_FB_body_tracking's tracker made with Meta's full-body joint set
// (XR_META_body_tracking_full_body: 84 joints, the legs too), high fidelity asked for (XR_META_body_tracking_fidelity:
// Quest 3's inside-out upper body), the calibration's state read (XR_META_body_tracking_calibration). Located each frame
// in the hands' space at the views' time; the joints go to vr_bodytrack.cpp. ROUND22.md, "Body tracking".

#ifdef QVR_HAVE_OPENXR

#include "vr_bodytrack_xr.hpp"
#include "vr_bodytrack.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_xr_runtime.hpp"

#include <string.h>

namespace qvr::bodytrack::xr
{

namespace
{

struct Tracker
{
    XrBodyTrackerFB handle{XR_NULL_HANDLE};
    PFN_xrCreateBodyTrackerFB create{nullptr};
    PFN_xrDestroyBodyTrackerFB destroy{nullptr};
    PFN_xrLocateBodyJointsFB locate{nullptr};
    PFN_xrRequestBodyTrackingFidelityMETA requestFidelity{nullptr};
    XrBodyJointLocationFB joints[XR_FULL_BODY_JOINT_COUNT_META]{};
    uint32_t jointCount{0};
    Raw raw{};
};
Tracker tracker;

void fail(const char* what, XrResult result)
{
    BackendStatus& st = backendStatus();
    q_snprintf(st.error, sizeof(st.error), "%s: %d", what, static_cast<int>(result));
    xrruntime::logLine(va("OpenXR: body tracking: %s failed (%d)\n", what, static_cast<int>(result)));
}

} // namespace

void reset()
{
    destroy();
    backendStatus() = BackendStatus{};
}

const char* wanted(const char* offered)
{
    BackendStatus& st = backendStatus();
    if(!strcmp(offered, XR_FB_BODY_TRACKING_EXTENSION_NAME))
    {
        st.fbExtension = true;
        return XR_FB_BODY_TRACKING_EXTENSION_NAME;
    }
    if(!strcmp(offered, XR_META_BODY_TRACKING_FULL_BODY_EXTENSION_NAME))
    {
        st.fullBodyExtension = true;
        return XR_META_BODY_TRACKING_FULL_BODY_EXTENSION_NAME;
    }
    if(!strcmp(offered, XR_META_BODY_TRACKING_FIDELITY_EXTENSION_NAME))
    {
        st.fidelityExtension = true;
        return XR_META_BODY_TRACKING_FIDELITY_EXTENSION_NAME;
    }
    if(!strcmp(offered, XR_META_BODY_TRACKING_CALIBRATION_EXTENSION_NAME))
    {
        st.calibrationExtension = true;
        return XR_META_BODY_TRACKING_CALIBRATION_EXTENSION_NAME;
    }
    return nullptr;
}

void create(XrInstance instance, XrSystemId system, XrSession session)
{
    BackendStatus& st = backendStatus();
    if(!st.fbExtension || tracker.handle != XR_NULL_HANDLE)
    {
        return;
    }

    XrSystemBodyTrackingPropertiesFB body{XR_TYPE_SYSTEM_BODY_TRACKING_PROPERTIES_FB};
    XrSystemProperties props{XR_TYPE_SYSTEM_PROPERTIES};
    props.next = &body;
    if(!XR_SUCCEEDED(xrGetSystemProperties(instance, system, &props)) || !body.supportsBodyTracking)
    {
        xrruntime::logLine("OpenXR: body tracking: the system does not support it\n");
        return;
    }
    st.systemSupports = true;

    xrGetInstanceProcAddr(instance, "xrCreateBodyTrackerFB", reinterpret_cast<PFN_xrVoidFunction*>(&tracker.create));
    xrGetInstanceProcAddr(instance, "xrDestroyBodyTrackerFB", reinterpret_cast<PFN_xrVoidFunction*>(&tracker.destroy));
    xrGetInstanceProcAddr(instance, "xrLocateBodyJointsFB", reinterpret_cast<PFN_xrVoidFunction*>(&tracker.locate));
    if(st.fidelityExtension)
    {
        xrGetInstanceProcAddr(instance, "xrRequestBodyTrackingFidelityMETA",
            reinterpret_cast<PFN_xrVoidFunction*>(&tracker.requestFidelity));
    }
    if(!tracker.create || !tracker.destroy || !tracker.locate)
    {
        fail("xrGetInstanceProcAddr", XR_ERROR_FUNCTION_UNSUPPORTED);
        return;
    }

    // The full body (the legs too) where offered; the upper body (70 joints) otherwise.
    XrBodyTrackerCreateInfoFB info{XR_TYPE_BODY_TRACKER_CREATE_INFO_FB};
    info.bodyJointSet = st.fullBodyExtension ? XR_BODY_JOINT_SET_FULL_BODY_META : XR_BODY_JOINT_SET_DEFAULT_FB;
    XrResult result = tracker.create(session, &info, &tracker.handle);
    if(!XR_SUCCEEDED(result) && st.fullBodyExtension)
    {
        info.bodyJointSet = XR_BODY_JOINT_SET_DEFAULT_FB;
        result = tracker.create(session, &info, &tracker.handle);
    }
    if(!XR_SUCCEEDED(result))
    {
        tracker.handle = XR_NULL_HANDLE;
        fail("xrCreateBodyTrackerFB", result);
        return;
    }
    st.trackerCreated = true;
    st.fullBodySet = info.bodyJointSet == XR_BODY_JOINT_SET_FULL_BODY_META;
    tracker.jointCount = st.fullBodySet ? uint32_t{XR_FULL_BODY_JOINT_COUNT_META} : uint32_t{XR_BODY_JOINT_COUNT_FB};

    if(tracker.requestFidelity)
    {
        result = tracker.requestFidelity(tracker.handle, XR_BODY_TRACKING_FIDELITY_HIGH_META);
        st.highFidelityRequested = XR_SUCCEEDED(result);
        if(!st.highFidelityRequested)
        {
            fail("xrRequestBodyTrackingFidelityMETA", result);
        }
    }
    xrruntime::logLine(va("OpenXR: body tracking: tracker made (%s, %u joints%s)\n", st.fullBodySet ? "full body" : "upper body",
        tracker.jointCount, st.highFidelityRequested ? ", high fidelity asked for" : ""));
}

void locate(XrSpace base, XrTime time)
{
    if(tracker.handle == XR_NULL_HANDLE || vr_bodytrack.value == 0.f)
    {
        return;
    }
    BackendStatus& st = backendStatus();

    XrBodyJointsLocateInfoFB info{XR_TYPE_BODY_JOINTS_LOCATE_INFO_FB};
    info.baseSpace = base;
    info.time = time;
    XrBodyJointLocationsFB out{XR_TYPE_BODY_JOINT_LOCATIONS_FB};
    out.jointCount = tracker.jointCount;
    out.jointLocations = tracker.joints;
    XrBodyTrackingFidelityStatusMETA fidelity{XR_TYPE_BODY_TRACKING_FIDELITY_STATUS_META};
    XrBodyTrackingCalibrationStatusMETA calibration{XR_TYPE_BODY_TRACKING_CALIBRATION_STATUS_META};
    void** chain = &out.next;
    if(st.fidelityExtension)
    {
        *chain = &fidelity;
        chain = &fidelity.next;
    }
    if(st.calibrationExtension)
    {
        *chain = &calibration;
    }
    const XrResult result = tracker.locate(tracker.handle, &info, &out);
    if(!XR_SUCCEEDED(result))
    {
        if(st.locateFailures++ == 0)
        {
            fail("xrLocateBodyJointsFB", result);
        }
        return;
    }

    Raw& raw = tracker.raw;
    raw.active = out.isActive == XR_TRUE;
    raw.confidence = out.confidence;
    raw.fidelity = st.fidelityExtension ? static_cast<int>(fidelity.fidelity) : 0;
    raw.calibration = st.calibrationExtension ? static_cast<int>(calibration.status) : 0;
    for(uint32_t j = 0; j < JointCount; j++)
    {
        if(j >= tracker.jointCount || !raw.active)
        {
            raw.flags[j] = 0;
            continue;
        }
        const XrBodyJointLocationFB& l = tracker.joints[j];
        const XrSpaceLocationFlags f = l.locationFlags;
        raw.flags[j] = ((f & XR_SPACE_LOCATION_POSITION_VALID_BIT) ? POS_VALID : 0u) |
                       ((f & XR_SPACE_LOCATION_POSITION_TRACKED_BIT) ? POS_TRACKED : 0u) |
                       ((f & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) ? ROT_VALID : 0u) |
                       ((f & XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT) ? ROT_TRACKED : 0u);
        raw.pos[j] = {l.pose.position.x, l.pose.position.y, l.pose.position.z};
    }
    submit(raw);
}

void destroy()
{
    if(tracker.handle != XR_NULL_HANDLE && tracker.destroy)
    {
        tracker.destroy(tracker.handle);
    }
    tracker = Tracker{};
    backendStatus().trackerCreated = false;
}

} // namespace qvr::bodytrack::xr

#endif
