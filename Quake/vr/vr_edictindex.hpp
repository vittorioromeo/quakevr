#pragma once
// vr_edictindex.hpp -- the server's edicts indexed by classname and by a few flag fields (vr_edictindex.cpp); the
// engine's hooks are in vr_api.h (VR_EdictIndex_*).

void VR_EdictIndex_ProgsLoaded(); // VR_OnProgsLoaded, the server's progs: the fields watched

struct edict_s; // (edict_t: progs.h)

namespace qvr::edictindex
{

void registerCommands(); // vr_edictindex_stats

// A float field of a server edict written in C where QuakeC's store would have told the index (a native of QuakeC:
// vr_propthink.cpp): followed as that STOREP's would be. Nothing for a field the index doesn't watch.
void fieldStored(struct edict_s* ed, int field);

} // namespace qvr::edictindex
