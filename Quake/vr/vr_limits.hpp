// vr_limits.hpp -- the hardcoded limits' usage (the vr_limits command) and the warnings for the silent ones (vr_limits.cpp).

#pragma once

namespace qvr::limits
{

// A limit whose overflow used to be silent was reached (QVR_LIMIT_*, vr_api.h): counted, warned about once a session.
void hit(int limit);

// vr_limits: every limit's usage against its maximum.
void command_f();

} // namespace qvr::limits
