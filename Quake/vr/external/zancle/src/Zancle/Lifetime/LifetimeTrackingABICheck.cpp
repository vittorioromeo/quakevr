// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Config.hpp"


////////////////////////////////////////////////////////////
extern "C"
{
#ifdef ZA_ENABLE_LIFETIME_TRACKING
    void zancleInternalAbiCheckLifetimeTrackingEnabled()
    {
    }
#else
    void zancleInternalAbiCheckLifetimeTrackingDisabled()
    {
    }
#endif
}
