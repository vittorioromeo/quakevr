// vr_footsteps.hpp -- enemies' footsteps (ROUND23.md, "Enemy footsteps"): a biped's foot coming down, as its animation
// draws it, is heard from that foot, soft at a walk and louder at a run. See vr_footsteps.cpp.
//
// VR_Footsteps_Frame (vr_api.h; SV_Physics, after the edicts) is the server's side, each frame.

#pragma once

namespace qvr::footsteps
{

void precache();         // the recordings, with Quake VR's progs, at each map's start (forgets the models and the entities)
void registerCommands(); // vr_footstep_info, vr_footstep_report

} // namespace qvr::footsteps
