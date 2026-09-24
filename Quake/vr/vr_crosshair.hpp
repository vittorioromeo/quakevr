// vr_crosshair.hpp -- the weapons' aiming aid: a dot or laser from each muzzle (vr_crosshair).

#pragma once

#include "vr_hands.hpp"

namespace qvr::crosshair
{

// Once per frame, before the eyes are drawn: queues the crosshairs (vr_lines).
void queue(const hands::State& s);

} // namespace qvr::crosshair
