#pragma once
// vr_findradius.hpp -- findradius() through the area grid, exact (vr_findradius.cpp); the engine's hooks are in
// vr_api.h (VR_FindRadius_*).

namespace qvr::findradius
{

void registerCommands(); // vr_findradius_stats

// Server edict e's .origin, .mins, .maxs or .solid was written (QuakeC's store, through the edict index's field
// watch): findradius tests it by itself until it is linked again.
void markDirty(int e);

} // namespace qvr::findradius
