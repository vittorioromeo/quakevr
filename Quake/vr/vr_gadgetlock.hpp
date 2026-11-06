// vr_gadgetlock.hpp -- the wrist gadget locked while both hands hold one thing (his note 2026-11-06: bullet time's
// double tap turned on by accident in fights, both hands busy; ROUND24.md, "The gadget locked while both hands hold
// one thing").
//
// While both hands hold the same thing -- one weapon two-handed (a foregrip or another hotspot, a free grip, a blade
// grip, a cup: twohand), the same carried prop, body part or crate (the carry stats), the same embedded blade (the embed
// stats), the same enemy (foegrab) or the same ragdoll -- and for vr_gadget_lock_grace real seconds after the second
// hand lets go, the gadget takes no input: no screen tap (bullet time's single or double tap, a double tap's first
// dropped), no side button (the gear lights, Doom's opening presses), no outer button (AD's pages), no touch on its
// screen (an app's: Doom). A finger or hand still on a button or the screen when it ends must leave it first. Different
// things in each hand: unchanged. vr_gadget_lock_twohand 0: off.

#pragma once

namespace qvr::gadgetlock
{

// Whether the gadget ignores input now (computed once a frame; vr_debug_bullettime or vr_debug_gadget_button: its
// changes printed, "gadget lock: ...").
[[nodiscard]] bool locked();

// What both hands hold now ("the weapon two-handed", "the same prop", ...), or null: not the same thing.
[[nodiscard]] const char* bothHold();

} // namespace qvr::gadgetlock
