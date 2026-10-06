// vr_shock.hpp -- electric arcs: Quad Damage's over the hands and forearms, and the lightning gun
// in water's (QC weapons.qc, VR_LGWater_*; vr_lg_water*): fired under water it shocks the player (a
// flickering blue flash over the view, arcs in front of the eyes, over the hands, forearms and
// body), fired into water from the open it electrifies it (arcs crawling over the surface round the
// point the beam goes in, a flickering light). The QC says when (watershock(), QVR_SVC_SHOCK); the
// client draws them as soft lines (vr_lines.hpp), reshaped every frame. Quad's arcs also crackle along the lightning's
// beams (vr_beam_arcs: CL_UpdateTEnts hands their ends over, VR_BeamDrawn), depth-tested in the scene.

#pragma once

#include "vr_hands.hpp"

namespace qvr::shock
{

// QVR_SVC_SHOCK's kinds.
enum Kind : int
{
    KindSelf = 0,    // the receiving player is shocked (sent to that client only)
    KindSurface = 1, // arcs over a liquid's surface round a point
    KindBody = 3,    // radius encodes the struck entity; arcs follow its current surface
    KindBurst = 2,   // arcs out from a point in a liquid (the shock's source)
    KindBodyDeath = 4, // as KindBody, lasting: a body the lightning struck, alive or dead (QC vr_shock.qc; duration in 1/4 s)
};

// QVR_SVC_SHOCK.
void parse();

// Once per frame, from the view setup: the effects due drawn, the screen flash set.
void frame(const hands::State& s);

// Arcs crawling over the hands and forearms (Quad Damage's, and the shock's): `bolts` for each hand, and a longer one
// from the fingers to the elbow at `longChance`; `seed` shapes them (the same in both eyes: once per frame).
void armArcs(const hands::State& s, unsigned seed, int bolts, float longChance);

// Forgets them all (a new map, a disconnect).
void clear();

// vr_shock_test <0 self | 1 surface | 2 burst> [radius] [duration]: the effect as the QC would send it, in front of
// the player (the surface's: on the liquid below the point 128 units ahead, if any).
void registerCommands();

} // namespace qvr::shock
