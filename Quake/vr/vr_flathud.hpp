// vr_flathud.hpp -- flatscreen play's status bar at hudstyle 1 (vr_flathud.cpp; sbar.c draws it).
#pragma once

namespace qvr::flathud
{

// vr_flathud_report (each hand's counter as the bar reads it, and bullet time's bar).
void init();

// The weapon whose ammo `hand`'s (HAND_OFF, HAND_MAIN) readouts show: the weapon prop it drives (STAT_QVR_PROPHUD: a gun
// stuck in a body by its bayonet, the Crystal Lance; the hand itself empty), else its own (STAT_QVR_WEAPON / 2).
[[nodiscard]] int hudWeapon(int hand);

} // namespace qvr::flathud
