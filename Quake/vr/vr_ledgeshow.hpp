// vr_ledgeshow.hpp -- Debug > Show Ledge Shove Zones (vr_knockdown_ledge_show): where a monster you shove ahead of you
// goes over a ledge, drawn on the floor (his note, 2026-11-05).
//
// A line from your feet straight ahead along the floor (the way your head faces), to the first ledge (a drop of Ledge
// Height, vr_knockdown_ledge_drop, or more; a wall or a step up ends it with none). Each point of it is where a monster's
// middle could stand, shoved straight on: red within vr_knockdown_ledge_topple of the edge (toppled over it at once, the
// old launch), yellow within vr_knockdown_ledge_backpedal_reach (it backpedals on its feet, then folds over the lip),
// grey further (an ordinary shove: the ledge plays no part). The edge itself: a white tick across. The shove must also
// reach the edge (Ledge Reach: its own slide, times vr_knockdown_ledge_reach, plus the margin): a weak one from the yellow
// may not. The QC's measure is the same (vr_knockdown.qc VR_Knockdown_Ledge: ground looked for every 8 units along the
// way, an edge where none is within the height below the last). Drawn in the scene (a headset's eyes and the flat view).

#pragma once

#include <glm/vec3.hpp>

namespace qvr::ledgeshow
{

// The view's debug draws: from the local player's feet along `yaw` (degrees, the head's).
void draw(float yaw);

} // namespace qvr::ledgeshow
