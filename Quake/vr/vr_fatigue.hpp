// vr_fatigue.hpp -- tired arms (docs/vr-port/ROUND21.md, "Tired arms: shaking and heavy hands").
//
// Low on stamina (the one pool parries, shoves, blows and hanging from a hold spend: vr_melee.qc, vr_climb.cpp):
// - the arms shake (vr_fatigue_shake): below vr_fatigue_shake_from of the stamina the drawn hands, arms and what they
//   hold tremble, more the less is left. Looks only: the game's hands, the aim, the muzzle and the melee stay where the
//   controllers have them (the drawn hands are moved for the view's setup and put back, as a parry's knock is; the muzzle
//   placed on the shaking weapon is moved back onto the steady one). While climbing only, as shipped
//   (vr_fatigue_shake_always: whenever tired);
// - the hands get heavy: the weight (vr_weight.cpp, "Tired, things weigh more") makes what they hold heavier, and an
//   empty hand follows its controller as if it held vr_weight_stamina_empty kg.
// Tests: vr_stamina_set puts the game's stamina at a share, vr_debug_stamina_hold keeps it there (a local server);
// vr_debug_fatigue prints the shake.

#pragma once

#include "vr_hands.hpp"

namespace qvr::fatigue
{

// The stamina left as the client sees it, 0..1 (vr_debug_weight_stamina when set; 1 without stamina).
[[nodiscard]] float staminaLeft();

// The shake's strength now, 0..1 (0: none), eased in and out.
[[nodiscard]] float shakeLevel();

// Hand `hand`'s shake this frame: world units and degrees (pitch, yaw, roll) added to the drawn hand. Zero without it.
void shake(int hand, glm::vec3& pos, glm::vec3& angles);

// Once a server frame (a local server): vr_debug_stamina_hold.
void serverFrame();

// vr_stamina_set.
void registerCommands();

} // namespace qvr::fatigue
