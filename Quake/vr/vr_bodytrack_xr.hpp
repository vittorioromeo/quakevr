// vr_bodytrack_xr.hpp -- the OpenXR body tracker (XR_FB_body_tracking, Meta's full body, fidelity and calibration) for
// vr_backend_openxr.cpp; the data goes to vr_bodytrack.cpp (bodytrack::submit).

#pragma once

#include <openxr/openxr.h>

namespace qvr::bodytrack::xr
{

// createInstance, for each offered extension: the name to enable (a string literal) if it is one of body tracking's,
// else nullptr. A new instance first: reset().
void reset();
[[nodiscard]] const char* wanted(const char* offered);

// After the session and its spaces are made: the tracker (full body, high fidelity), if the system supports it.
void create(XrInstance instance, XrSystemId system, XrSession session);

// Each frame, with the hands: the joints in `base` at `time`, submitted (nothing without a tracker).
void locate(XrSpace base, XrTime time);

// Before the session is destroyed.
void destroy();

} // namespace qvr::bodytrack::xr
