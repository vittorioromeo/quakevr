// vr_shadows.hpp -- soft blob shadows under the player and the hands (vr_player_shadows), and
// under monsters and items (vr_entity_shadows).

#pragma once

namespace qvr::shadows
{

// In the scene pass, after the opaque entities (VR_DrawSceneOpaque).
void draw();

} // namespace qvr::shadows
