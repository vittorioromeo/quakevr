// vr_meleeshow.hpp -- Show Melee Blades (vr_debug_melee_blades; Debug > Views): what each hand's blows strike with, live.
//
// QC sends, only while the view is on (vr_melee.qc VR_Melee_Show, from VR_Melee_Sweep; the builtin meleeshow), each
// hand's sweep this server frame: its blow line (the hand to the tip: VR_Melee_Line's, and the near end behind the
// hand), the lines the sweep tested (VR_Melee_Ribs: each striking point's way from the pose before to this one, the
// whole blade's with vr_melee_swept) and a hammer's or mace's head box (admeleehead); and each blow that landed (the
// contact, its damage: VR_Melee_Landed, T_Damage_VRMelee). The view (vr_view.cpp) draws them: the blow line white idle,
// yellow while it moves as a blow (QC VR_Melee_ShowFast: VR_Melee_Decide would land one with its tip now: fast enough,
// the arm driving it, no wiggle), red with a blow just landed; the swept
// surface of the last few frames fading out in the same colours; a red dot where a blow landed, its damage above it.
// Off: nothing is sent or kept, and draw returns at once.

#pragma once

#include <glm/glm.hpp>

namespace qvr::meleeshow
{

// From QC's builtin meleeshow(hand, what, a, b, v) (vr_builtins.cpp): what 0 begins the hand's frame (a the hand, b the
// tip, v 1 fast enough to strike); 1 its near end (a); 2 a swept line (a to b, v its thickness); 3 a head's box (a, b:
// admeleehead's low and high corners, shares of the line); 4 the head's line (a the hand, b the tip); 5 the head's
// model +y (a); 6 a blow landed (a where; v its damage, -1 not known: a wall, or the contact before the damage).
void add(int hand, int what, const glm::vec3& a, const glm::vec3& b, float v);

// Draws the hands' last frames (the view's, facing `eye`); nothing when vr_debug_melee_blades is off.
void draw(const glm::vec3& eye);

} // namespace qvr::meleeshow
