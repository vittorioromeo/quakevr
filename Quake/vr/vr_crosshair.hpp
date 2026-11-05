// vr_crosshair.hpp -- the weapons' aiming aid: a dot or laser from each muzzle (vr_crosshair).

#pragma once

#include "vr_hands.hpp"

namespace qvr::crosshair
{

// Once per frame, before the eyes are drawn: queues the crosshairs (vr_lines).
void queue(const hands::State& s);

// Flatscreen play (VR_SetupViewEntities, once a frame, the weapons placed): the same crosshairs, the same settings, drawn in
// the scene's translucent pass over what is there (flatscreen has no UI pass: lines::sceneOverLine), each laser from its
// gun's drawn muzzle to where its shots go. Beside the flat crosshair (`crosshair`), not instead of it.
void queueFlat(const hands::State& s);

} // namespace qvr::crosshair
