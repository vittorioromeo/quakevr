// vr_flathit.hpp -- flatscreen play's feedback when he is hit (vr_flathit.cpp; docs/vr-port/FLATPLAY.md, "Hit feedback").
#pragma once

#include <glm/vec3.hpp>

namespace qvr::flathit
{

// vr_flat_hit_probe, vr_flat_hit_test.
void init();

// A hit on him (VR_OnDamage: V_ParseDamage's armor, blood and where it came from): the view's punch and the screen's mark.
// Nothing outside flatscreen play.
void onDamage(int armor, int blood, const float* from);

// The punch's part this frame put into the aim (cl.viewangles) and the last frame's taken back out of it: flatplay::frame,
// before the pose reads the look. Everything goes when the mode is off, he is dead or the level changes.
void frame();

// A hit's knock of drawn hand `h` (painknock::offset), kept at vr_flat_ads_hitreact of itself as far as the ironsight has
// come (flatsight::adsShare, smoothed); untouched outside the mode. vr_flat_hit_probe reads it.
void keepHitReact(int h, glm::vec3& pos, glm::vec3& angles);

} // namespace qvr::flathit
