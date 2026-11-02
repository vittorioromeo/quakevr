// vr_playersteps.hpp -- the player's own footsteps on the drawn feet (ROUND23.md, "The player's walk"): the sound of a step
// plays as a drawn foot touches down, from that foot, and QC's old trigger (player_run, by distance) stays silent then.
// See vr_playersteps.cpp.

#pragma once

#include "vr_avatar.hpp"

namespace qvr::playersteps
{

// After the body is posed: the touchdowns since the last pose heard.
void frame(const avatar::Skeleton& k);

// Asked by QC's old trigger for a step of entity `ent` (playerstepsilent): true if that step stays silent.
[[nodiscard]] bool oldStepSilent(int ent);

void registerCommands(); // vr_body_step_report

} // namespace qvr::playersteps
