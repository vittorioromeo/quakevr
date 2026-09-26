// vr_shells.hpp -- spent shell casings: client-side debris thrown out of the local player's
// weapons -- the shotgun's port as it cycles a round, the super shotgun's breech as it is
// reloaded (on a flick reload thrown forward as the spin turns the barrels down, to fall in front
// of the player; never at the face) -- falling at real gravity, bouncing off the world with a
// quiet tink, rolling and settling on the floor, then fading out (vr_shells, vr_shells_life,
// vr_shells_sound). The QC says when (ejectcasings(), QVR_SVC_EJECT); where from and which way
// come from the weapon model the client draws in that hand. A pool of at most 64, drawn as
// alias entities (progs/vr_shell.mdl, Misc/quakevr/make_shell.py); resting ones cost nothing.

#pragma once

#include "vr_view.hpp"

namespace qvr::shells
{

// QVR_SVC_EJECT: casings out of a hand's weapon, now or after a delay.
void parseEject();

// Once per frame, from the view setup: follows the weapons' ejection ports (their speed goes
// into the casings), throws out what is due, moves the casings and adds them to the scene.
void frame(const view::ViewEntity (&weapons)[2]);

// Removes them all (a new map, a disconnect).
void clear();

// vr_shells_eject [hand] [count] [flick]: casings out of a hand's weapon, as the QC would (tuning).
void registerCommands();

// Casings in the world (vr_memstats).
[[nodiscard]] int liveCount();

} // namespace qvr::shells
