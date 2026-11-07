#pragma once
// vr_edictindex.hpp -- the server's edicts indexed by classname and by a few flag fields (vr_edictindex.cpp); the
// engine's hooks are in vr_api.h (VR_EdictIndex_*).

void VR_EdictIndex_ProgsLoaded(); // VR_OnProgsLoaded, the server's progs: the fields watched

#include "Zancle/Container/Vector.hpp"

struct edict_s; // (edict_t: progs.h)

namespace qvr::edictindex
{

void registerCommands(); // vr_edictindex_stats

// A float field of a server edict written in C where QuakeC's store would have told the index (a native of QuakeC:
// vr_propthink.cpp): followed as that STOREP's would be. Nothing for a field the index doesn't watch.
void fieldStored(struct edict_s* ed, int field);

// The engine's own loops over the edicts with something set, stepped by the index instead of walking every edict: each
// returns the edict a walk from `from + 1` over the current VM's edicts in use would reach first (0: none). The index
// answers for the server's VM (current), else the walk. A loop calls it again from the edict it got, so it sees what its
// body changed as the walk would.
// - nextFlagged: any of `bits` in .flags (the bits QuakeC sets: FL_MONSTER, FL_CLIENT, FL_ITEM; an engine bit walks).
// - nextNonzero: the float `field` (its offset: progs::fields()) not 0; the index's "not 0" fields only (vr_hosecut),
//   any other walks.
// - nextClassname: the classname `name` (find()'s).
[[nodiscard]] int nextFlagged(int from, int bits);
[[nodiscard]] int nextNonzero(int from, int field);
[[nodiscard]] int nextClassname(int from, const char* name);

// For a loop that keeps state per edict and clears it for the edicts it skips: the edicts it has to visit, in edict order
// into `out`: those after `from` with any of `bits` in .flags (nextFlagged's, as now) and those in `kept` (ascending: the
// ones it left state on last time), below num_edicts. Every other edict's state is cleared already.
void flaggedOrKept(int from, int bits, const za::Vector<int>& kept, za::Vector<int>& out);

} // namespace qvr::edictindex
