// vr_menu_pages_vrsettings.cpp -- the VR menu's Stealth and VR Settings pages (split from vr_menu_pages_settings.cpp
// for the build's speed); what they share is in vr_menu_internal.hpp.

#include "vr_flatplay.hpp"
#include "vr_foveated.hpp"
#include "vr_menu_internal.hpp"
#include "vr_cvars.hpp"
#include "vr_api.h" // (VR_Android: the Scaling page's foveation rows)

namespace menu_detail
{

#include "vr_menu_stealth.inc"
#include "vr_menu_settings.inc"

} // namespace menu_detail
