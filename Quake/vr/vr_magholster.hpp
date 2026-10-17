// vr_magholster.hpp -- magnetic holsters, as RE4VR's (ROUND22.md, "Magnetic holsters"): a weapon bound to a holster
// comes back to it on its own once it has been out of the hands long enough or is far enough from it (QC
// vr_magholster.qc: the bindings, the return, the flight back). Here, the client's part:
// - the bind button (vr_magholster_button) taken from the game while a hand is at a holster with a weapon in it, or one
//   bound, and handed to the QC (the magholsterpoll builtin): the QC binds that holster's weapon, or unbinds it;
// - the chains showing a binding (vr_magholster_style): a lanyard, a short chain hanging off the bound weapon wherever it
//   is (holstered, held, lying about, flying back), a stub of it hanging at its holster while the weapon is away; a
//   tether, a chain from the holster to the weapon whenever it is out of it (held too), however far (links up to
//   vr_magholster_chain_lod along it, a plain line on from there), simulated as a chain of points lying on and wrapping
//   round the world (vr_verlet.hpp, the grappling hook rope's steps; vr_magholster_chain_sim); or both. Their end at the
//   holster moved by vr_magholster_chain_x/y/z and each kind's.
//   The flashlight's chain (vr_coil.cpp, rigid: iron that keeps its length), its links sized by
//   vr_magholster_chain_size, lit and depth-tested in each eye's opaque scene, see-through below
//   vr_magholster_chain_alpha 1.

#pragma once

#include "vr_backend.hpp"
#include "vr_hands.hpp"


namespace qvr::magholster
{

void init(); // vr_magholster_info, vr_magholster_return

// A button of `hand` pressed or let go, before the game gets its key: true when the magnetic holsters take it (the
// press and its release).
[[nodiscard]] bool button(int hand, bool HandInput::*which, bool now);

// What the local player asked since the last call, taken: 1 the off hand's bind press, 2 the main hand's, 4 every
// bound weapon home at once (vr_magholster_return).
[[nodiscard]] int poll();

// Once a frame as the view is set up, after the weapons and the holsters are placed: the chains' ends and lines.
void setupView(const hands::State& s);

// The chains, in each eye's opaque scene.
void drawOpaque();

} // namespace qvr::magholster
