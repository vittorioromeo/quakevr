// vr_hosterror.hpp -- what a Host_Error leaves open, put back before its longjmp (VR_OnHostError, vr_api.h), and its
// test, vr_hosterror_test.

#pragma once

namespace qvr::hosterror
{

void registerCommands(); // vr_hosterror_test (VR_Init)

} // namespace qvr::hosterror
