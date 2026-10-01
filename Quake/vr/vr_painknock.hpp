// vr_painknock.hpp -- being hit knocks the drawn hands (docs/vr-port/ROUND21.md, "Pain feedback: hits knock the hands").
//
// Each hit the client is told of (svc_damage: V_ParseDamage, the damage and where it came from) pushes both drawn hands
// away from where it came from, sized by the damage (vr_pain_knock_strength cm a point, at most vr_pain_knock_max cm),
// more the hand on the side it came from, and eases them back over vr_pain_knock_time (vr_pain_knock_seen: turned across
// the line from the eyes to each hand, so that size is what is seen; tipped about the wrist); each controller buzzes with the
// same weight (vr_pain_haptics). Looks only, as a parried blow's knock and the tired arms' shake: the drawn hands are
// moved for the view's setup and put back, the muzzle placed on the knocked weapon put back on the steady one, so the
// aim, the shots and the melee stay where the controllers are.
// Tests: vr_debug_pain prints each hit and the knock; vr_pain_test <damage> <degrees> hits you from a direction.

#pragma once

#include <glm/glm.hpp>

namespace qvr::painknock
{

// V_ParseDamage: `armor` and `blood` points taken, from `from` (the inflictor's middle; the world's for falls, lava,
// drowning: no direction).
void onDamage(int armor, int blood, const float from[3]);

// Hand `hand`'s knock this frame: world units and degrees (pitch, yaw, roll) added to the drawn hand. Zero without one.
void offset(int hand, glm::vec3& pos, glm::vec3& angles);

// A new map (VR_OnClientClearState): the client's time starts over, the knocks are forgotten.
void reset();

// vr_pain_test.
void registerCommands();

} // namespace qvr::painknock
