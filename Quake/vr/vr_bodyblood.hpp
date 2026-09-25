// vr_bodyblood.hpp -- blood dripping from the player's wounds (vr_body_blood). While the arms and
// hands show wounds (vr_body_state's damage skins), drops gather under the forearms and the hands,
// grow, fall and splash on the floor. They come faster the
// more hurt the player is and right after a hit, run to the lower end of a sloping arm, and are
// flung off an arm swung hard.
//
// The drops are a few dozen points simulated here (real gravity, a line through the world each
// frame) and drawn as soft lines (vr_lines): a round drop while it hangs, a short streak as it falls.

#pragma once

#include "vr_avatar.hpp"

namespace qvr::bodyblood
{

// Once per frame, after the body is posed (vr_view.cpp): the drawn hands (null where a hand is not
// drawn) and how hurt the player looks, 0..3 (the damage skins; 0 when vr_body_state is off). The
// forearms drip too when the body is posed (avatar::forearm).
void update(const hands::State& s, const avatar::HandPose* const drawnHands[2], int damage);

// No drops left (a new map, the view gone).
void clear();

} // namespace qvr::bodyblood
