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
    KindSmoulder = 5,  // not arcs: a burning body's fire (radius: the entity) goes out in duration s (1/4 s), smoking on
                       // after (vr_smoulder.cpp; QC vr_burning.qc bodysmoulder)
    KindDoused = 6,    // ... put out in a liquid: it stops smoking
    KindSelfHit = 7,   // the receiving player struck by lightning (sent to that client only; radius: the damage): arcs over
                       // its arms, hands and body a while (vr_shock_self_time; QC weapons.qc playershock)
    KindGunShock = 8,  // as KindBodyDeath, over the receiving player's own gun in a hand (radius: the hand, 0 off, 1 main;
                       // sent to that client only; duration in 1/4 s), smaller: a single-use enemy gun spent, crackling
                       // (QC vr_enemyguns.qc VR_EnemyGun_Spent)
};
// Whether a kind's duration goes in 1/4 s (the lasting ones: up to 63 s) rather than 1/50 s (up to 5 s).
[[nodiscard]] constexpr bool quarterSeconds(int kind)
{
    return (kind >= KindBodyDeath && kind <= KindDoused) || kind == KindGunShock;
}
// Whether a kind is sent to one player's client only (the others: to all).
[[nodiscard]] constexpr bool toOneClient(int kind)
{
    return kind == KindSelf || kind == KindSelfHit || kind == KindGunShock;
}
// Whether models `a` and `b` are one body's: the same .mdl, or it and its ragdoll's skinned copy ("<model>#rag": the
// client swaps them as the ragdoll is made and gone).
[[nodiscard]] bool sameBody(const qmodel_s* a, const qmodel_s* b);

// QVR_SVC_SHOCK.
void parse();

// Once per frame, from the view setup: the effects due drawn, the screen flash set; a crackling gun's hand buzzed.
void frame(const hands::State& s);

// The gun in this client's `hand` crackling (a spent enemy rifle: KindGunShock, QC vr_enemyguns.qc): how hard it still
// shakes, 0..1 (fading as its arcs run out; 0: none, or another gun there now). vr_view.cpp setupWeapon jitters it by
// vr_enemygun_spent_shake; frame buzzes the hand by vr_enemygun_spent_haptics (the author's note map1_2026-10-08_13-56-46).
[[nodiscard]] float gunShake(int hand);

// Arcs crawling over the hands and forearms (Quad Damage's, and the shock's): `bolts` for each hand, and a longer one
// from the fingers to the elbow at `longChance`; `seed` shapes them (the same in both eyes: once per frame).
void armArcs(const hands::State& s, unsigned seed, int bolts, float longChance);

// Forgets them all (a new map, a disconnect).
void clear();

// vr_shock_test <0 self | 1 surface | 2 burst> [radius] [duration]: the effect as the QC would send it, in front of
// the player (the surface's: on the liquid below the point 128 units ahead, if any).
void registerCommands();

} // namespace qvr::shock
