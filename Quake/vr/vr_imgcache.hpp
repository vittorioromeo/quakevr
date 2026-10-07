#pragma once

// vr_imgcache.hpp -- decoded images kept across map loads (vr_image_cache_mb; Image_LoadImage asks first: vr_api.h).

namespace qvr::imgcache
{

void init(); // VR_Init: its commands (vr_image_cache_info, vr_image_cache_clear)

} // namespace qvr::imgcache
