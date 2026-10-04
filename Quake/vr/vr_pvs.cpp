// vr_pvs.cpp -- the PVS self-leaf switch (vr_pvs_selfleaf).
//
// A compiled PVS names the leafs a portal leads to, never the leaf it is stored in: in start.bsp 392 of the 1128 leafs
// that carry vis data do not name themselves. Everything that culls by the view's PVS therefore drops what is in the
// very leaf the view is in - a brush entity you are standing inside, or touching: the closed `func_episodegate` in
// `start` (its brush resolves to leaf 595 alone, and the eye at the reported spot is in leaf 595 too), and the hidden
// staircase behind it, which appears and disappears as the head moves between that leaf and one that names it
// (Misc/quakevr/pvs/FINDINGS.md).
//
// With the switch on, the leaf a PVS is taken from is named in it. Each reader gets the bit its own convention tests:
// the client packs a leaf L as bit L-1 (gl_refrag.c pushes `idx - 1`, R_AddStaticModels tests it), so Mod_LeafPVS sets
// bit L-1; the server tests the raw leaf number (sv_main.c SV_EdictInPVS, SV_WriteEntitiesToClient), so SV_AddToFatPVS
// sets bit L as well. With the switch off nothing here changes and the engine behaves as it did before this file.
//
// The cost is one bit per leaf a fat-PVS sample point reaches, and what that newly draws: the brush entities of the
// leaf you stand in. See Misc/quakevr/pvs/FINDINGS.md for the measured deltas.

#include "vr_cvars.hpp"

extern "C" int VR_PvsSelfLeaf(void)
{
    return qvr::vr_pvs_selfleaf.value > 0.f ? 1 : 0;
}
