#pragma once
// vr_edictindex.hpp -- the server's edicts indexed by classname and by a few flag fields (vr_edictindex.cpp); the
// engine's hooks are in vr_api.h (VR_EdictIndex_*).

void VR_EdictIndex_ProgsLoaded(); // VR_OnProgsLoaded, the server's progs: the fields watched

namespace qvr::edictindex
{

void registerCommands(); // vr_edictindex_stats

} // namespace qvr::edictindex
