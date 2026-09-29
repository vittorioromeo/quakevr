// vr_drawblend.hpp -- a gun drawn from a holster eases from its holstered pose into the hand, and one holstered eases
// from the hand into the holster (vr_weapon_draw_blend, vr_weapon_holster_blend; ROUND21.md, "Holster draw blend").

#pragma once

#include "vr_engine.hpp"
#include "vr_hands.hpp"

namespace qvr::drawblend
{

// A hand's gun as the view placed it this frame (`e`: its real pose, the one the aim and the shots use), `gun` false
// when the hand holds no gun of its own (empty, the fist, a carried gun, the posing mode). While it is being drawn
// from a holster, `e` is moved to its drawn pose (visual only).
void hand(const hands::State& s, int hand, entity_t& e, bool gun);

// A holster's gun as the view placed it this frame (`holster`: body::Holster, its stat slot; `e` null or without a
// model when it is empty), `live` false for the Weapon Offsets preview (a stand-in, not what the holster holds).
// While it is being holstered, `e` is moved to its drawn pose.
void holster(const hands::State& s, int holster, entity_t* e, bool live);

// A new map (the entities' poses are the old map's).
void reset();

} // namespace qvr::drawblend
