// vr_foegrab.hpp -- holding enemies (vr_foegrab, experimental, on; Combat > Holding Enemies; ROUND21.md, "Holding
// enemies"): an empty hand whose fist touches a living monster's model as drawn (precise hits' model, vr_hitmodel.cpp,
// within vr_foegrab_leniency cm) as its grip is pressed takes hold of that spot of the model. While the grip stays pressed:
// - the drawn hand stays on the spot as the model moves and animates (the palm where it was on it, in the frame of the
//   triangle it touched: hitmodel::anchorFrame), the controller's turn kept;
// - the monster is held by its hold (QC VR_FoeGrab_Strength: a small enemy's vr_foegrab_strength_small, a big one's
//   _medium, a huge one's _large; or by its mass, vr_foegrab_by_mass), two hands' together 1 - (1 - a)(1 - b): its own
//   movement (its steps, a slide, a leap; flat only) is cut by vr_foegrab_slow times its hold, it follows the holding
//   hands' own move (flat: a pull, the player walking off with it) by vr_foegrab_follow times its hold, and the hand pulls
//   the spot towards it (vr_foegrab_drag times the hold, at most vr_foegrab_drag_speed): a grunt comes along, an ogre a
//   little, a shambler hardly;
// - the dummy (vr_dummy.qc) is held as any enemy but never moved (it stands on its spot);
// - both hands holding one enemy and turning it over hard (vr_foegrab_throw_twist degrees/s, the hands at
//   vr_foegrab_throw_speed m/s) throw it down that way (QC vr_foegrab_throw.qc: a knockdown, for sure, by its kind and
//   health: vr_foegrab_throw_always, _when_hurt, _hurt; or by mass), and both let go (throwCheck);
// - its shoves (grunts, enforcers) are resisted (QC vr_enemyshove.qc: vr_foegrab_shove_resist times its hold) or, with
//   vr_foegrab_shove 1, push you fully and break the hold;
// - the holding hand deals no melee blows (QC .vr_foegrab_hands); the other hand fights as ever;
// - taking hold of a monster wakes it as a touch does (QC VR_FoeGrab_Taken: the stealth AI's touch rule).
// It lets go when the grip is released, when the tracked palm is more than vr_foegrab_break cm from the spot it holds
// (pulled away: the monster ran, or you moved), when the monster dies, is knocked down or is moved away (a teleport),
// when it breaks free (its shove with vr_foegrab_shove 1), or the hand takes something else.
//
// The QC never sees the grip of a hand that holds (its grab bits are masked in .vrbits0 from the take until the grip's
// release, as the climb's holds: vr_climb.cpp), so the press picks nothing else up.
//
// Server: preThink (each player's frame, after the climb's: a hand on a ledge takes no enemy) takes and lets go;
// serverFrame (the frame's end, before the precise hits keep the poses) slows and drags the held monsters; afterPoses
// (after them) places the held spots and lets go of a hand pulled too far. The client gets the spots as stats
// (STAT_QVR_FOEGRAB*) and draws the hands on them.

#pragma once

#include "vr_engine.hpp"

extern "C" void VR_FoeGrabPreThink(edict_t* ent); // SV_Physics_Client, after VR_ClimbPreThink

namespace qvr::hands
{
struct State;
}

namespace qvr::foegrab
{

void init(); // registers vr_foegrab_status, vr_foegrab_walk_test

// Server: every hold forgotten (a map loaded, a saved game loaded: their entities are another world's).
void reset();

// Server: a shove's knockdown (QC VR_Knockdown_Try: ragdollshovetopple), `m` just made a ragdoll: turned over about its
// feet along the level `dir` as the throw's (box3d::ragdollTopple) at vr_knockdown_shove_topple times `strength` (the
// shove's push: two hands 1, one 0.7, a counter more, tired less), no spin, its top keeping vr_knockdown_shove_topple_push
// of the shove's launch (the throw 1), its feet held vr_knockdown_shove_feet_hold s (or swept back,
// vr_knockdown_shove_feet_speed). With vr_knockdown_debug or vr_foegrab_debug its fall is printed as the
// throw's ("shove trace:"). False: not toppled (topple 0, no ragdoll).
bool shoveTopple(edict_t* m, const glm::vec3& dir, float strength);

void preThink(edict_t* ent);
void serverFrame(); // VR_ServerFrameEnd, before hitmodel::serverFrame
void afterPoses();  // VR_ServerFrameEnd, after it

// Server: the player's holds as stats (STAT_QVR_FOEGRAB*), for the drawn hands.
void calcStats(edict_t* ent, int* statsi);

// Client: whether `hand` holds an enemy now.
[[nodiscard]] bool holding(int hand);

// Client: the drawn `hand`'s place (its controller's `pos`, turned `rot`) moved so that its palm is on the spot it holds,
// eased on and off. Returns how far it is on it, 0 (the controller's) .. 1.
float drawnHand(const hands::State& s, int hand, glm::vec3& pos, const glm::vec3& rot);

} // namespace qvr::foegrab
