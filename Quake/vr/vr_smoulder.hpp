// vr_smoulder.hpp -- smouldering bodies: thin smoke rising off the skin of the monsters and bodies the lightning strikes
// (alive or dead) for vr_smoulder_time s from the last bolt, and of burning ones (QC vr_burning.qc) while their flames
// burn and vr_smoulder_burn_time s after they go out, thinning as it ends. A client-side effect: the lightning's own
// QVR_SVC_SHOCK body messages (vr_shock.cpp, each hit and the lasting shock) start it, and a burning body's fire sends
// one when it is lit, burns on longer, or goes out (bodysmoulder(), KindSmoulder). The wisps leave random points of
// the body as drawn now, so they follow it as it moves, dies and lies as a ragdoll (vr_particles.cpp smoulderSmoke).
#pragma once

namespace qvr::smoulder
{
// Lightning struck entity `num` (each hit's arcs or its lasting shock): it smokes vr_smoulder_time s from now.
void struck(int num);
// Entity `num`'s fire (KindSmoulder): its flames go out in `left` s (0: just out); it smokes while they burn, then
// vr_smoulder_burn_time s after. `doused` (put out in a liquid): it stops smoking now, whatever lit it.
void burning(int num, float left, bool doused);
// Once a frame (the view setup): the smoke due from each smouldering body.
void frame();
// Forgets them all (a new map, a disconnect).
void clear();
// vr_smoulder_info: the smouldering bodies, how much each smokes, the wisps made since the last print;
// vr_smoulder_test [seconds]: every monster and body near smokes that long.
void registerCommands();
} // namespace qvr::smoulder
