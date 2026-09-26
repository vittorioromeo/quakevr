// vr_twohand.hpp -- two-handed aiming: steadying a weapon with the other (empty) hand.

#pragma once

#include "vr_hands.hpp"

namespace qvr::twohand
{

// Turns the holding hands' rotations towards the helping hands, before they become the aim.
void apply(hands::State& s);

// Whether either hand is (mostly) aiming two-handed: VRBITS0_2H_AIMING.
[[nodiscard]] bool aiming();

// How far into a two-handed grip `hand` (holding the weapon) is, 0..1.
[[nodiscard]] float transition(int hand);

// Whether `hand` is the helping hand of a two-handed grip (it follows the weapon).
[[nodiscard]] bool helping(int hand);

void reset();

// Hand-off (vr_2h_handoff; QC VRTryHandOff, docs/vr-port/ROUND16.md): the hand holding a gun two-handed
// lets go, and the gun hangs from the other hand's foregrip, drawn as it was held, until a hand takes
// its handle again.

// Whether the weapon in `hand` hangs from its foregrip (the server's QVR_WPNFLAG_FOREGRIP_CARRIED).
[[nodiscard]] bool carrying(int hand);

// Where a gun carried by `hand` is drawn: as the hand that let it go held it, at the moment it let
// go, carried rigidly by `hand` since. False when `hand` doesn't carry one, or its pose was never seen
// (drawn as held, then).
struct HeldAs
{
    glm::vec3 pos;  // the holding hand's position
    glm::vec3 rot;  // and angles
    bool mirrored;  // held in the off hand
};
[[nodiscard]] bool carriedWeapon(const hands::State& s, int hand, HeldAs& out);

// Where the hand carrying a gun is drawn: on its foregrip, as it was when steadying it.
[[nodiscard]] bool carryingHand(const hands::State& s, int hand, glm::vec3& pos, glm::vec3& rot);

// The view, each frame: the helping hand as drawn on the other's weapon (for the hand-off), and the
// carried gun's handle (where the hand that takes it back closes).
void recordHelp(const hands::State& s, int hand, const glm::vec3& drawnPos, const glm::vec3& drawnRot);
void setCarriedHandle(int hand, const glm::vec3& pos);

// After the hotspots: the empty hand at the handle of the gun the other hand carries is at
// HS_CARRIED_GRIP (grabbing there takes the gun back).
void updateHotspots(hands::State& s);

} // namespace qvr::twohand
