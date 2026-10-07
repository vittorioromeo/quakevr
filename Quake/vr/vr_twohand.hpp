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

// How far the virtual stock steadies the two-handed aim of the weapon in `hand` (vr_2h_mode 2, the hand at the shoulder),
// 0..1: its weapon's Stock Pitch, Yaw and Roll turn the aim this much.
[[nodiscard]] float stock(int hand);

// Whether `hand` is the helping hand of a two-handed grip (it follows the weapon).
[[nodiscard]] bool helping(int hand);

// Whether the weapon in `holding` may be flick-reloaded (vr_flick.cpp): held in that hand alone, or the other hand holds
// it by a cup only (not its barrel, a foregrip or anywhere on it), and not carried off its handle.
[[nodiscard]] bool flickAllowed(int holding);

// Whether the weapon in `hand` is held two-handed by its blade (a sword's half-sword grip: the other
// hand towards the tip), rather than by its foregrip or grip.
[[nodiscard]] bool bladeGrip(int hand);

// Where the helping hand `hand` is drawn on the blade it holds (bladeGrip(1 - hand)): its pose
// (`pos`, `rot`: in, as tracked and drawn; out, on the blade), given the holding hand's drawn pose.
[[nodiscard]] bool bladeGripHand(const hands::State& s, int hand, const glm::vec3& holderPos, const glm::vec3& holderRot,
    glm::vec3& pos, glm::vec3& rot);
// The same for the weapon of `slot` held in `holding` at the angles `holderVisualRot` (as drawn), its tip at `tip`: the
// other hand's pose (`pos`, `rot`) turned onto the blade (either way round) and slid along it into its grip's zone.
void bladeGripOn(int slot, int holding, const glm::vec3& tip, const glm::vec3& holderVisualRot, const glm::vec3& holderPos,
    const glm::vec3& holderRot, glm::vec3& pos, glm::vec3& rot);

// A weapon lying about taken by one of its hotspots (vr_weapon_grab_hotspots; the view's groundSpots, QC
// wpnthrow_handtouch_impl): the empty `hand` near a grip or blade hotspot of the weapon entity `entity` (the client's
// number, the server's edict) records, each frame, how it would carry it by that hotspot: the poses (all in the same
// frame) of the hand as tracked, of the other hand as it would hold the handle (`holderMirrored`: the off hand) and of the
// hand as drawn on the hotspot. The server asks which hotspot (groundSpot: index + 1, 0 none: its handle); a carry that begins
// just after takes those poses (as a hand-off takes its last help's).
void recordGroundSpot(int hand, int entity, int index, const glm::vec3& trackedPos, const glm::vec3& trackedRot,
    const glm::vec3& holderPos, const glm::vec3& holderRot, bool holderMirrored, const glm::vec3& drawnPos,
    const glm::vec3& drawnRot);
void clearGroundSpot(int hand);
// The `index` of a weapon taken anywhere on it (vr_weapon_grab_anywhere: away from its handle and hotspots): carried as it
// lay (groundSpot: anywhereSpot + 1).
inline constexpr int anywhereSpot = 4;
// The handle of the weapon `hand` carries (where the other hand takes it back), as drawn last; false if none.
[[nodiscard]] bool carriedHandle(int hand, glm::vec3& out);
[[nodiscard]] int groundSpot(int hand, int entity);

void reset();

// Hand-off (vr_2h_handoff; QC VRTryHandOff, ROUND16.md (removed 2026-10-06; git history)): the hand holding a gun two-handed
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
// (`holderPos`, `holderRot`, `holderMirrored`: the pose the other hand's weapon is drawn from: its hand's, or the pose
// it is carried at.)
void recordHelp(const hands::State& s, int hand, const glm::vec3& drawnPos, const glm::vec3& drawnRot,
    const glm::vec3& holderPos, const glm::vec3& holderRot, bool holderMirrored);
void setCarriedHandle(int hand, const glm::vec3& pos);

// After the hotspots: the empty hand at the handle of the gun the other hand carries is at
// HS_CARRIED_GRIP (grabbing there takes the gun back); one on the other gun's magazine (hands::State::onMagazine) at
// HS_MAGAZINE.
void updateHotspots(hands::State& s);
// Units from a carried weapon's handle (as drawn) the other, empty hand takes it back.
inline constexpr float carriedGripRadius = 6.f;
// Units from a two-handed grip's point (less its bias) within which the other hand takes hold of it.
inline constexpr float gripTakeUnits = 5.5f;

// Weapons held anywhere (vr_weapon_grab_anywhere; ROUND21.md, "Weapons held anywhere"): the empty hand gripping the
// other hand's weapon away from its handle and hotspots holds it there, as a prop is held. On a weapon held by its handle
// it steers the aim as a foregrip, supports it only, or holds it rigidly with the other hand (vr_weapon_anygrip_mode, the
// weapon's Other Hand Anywhere); on one carried off its handle (the hand-off, a hotspot, a free grip) both hands hold it
// as a prop held in both (carry2h's solve). Letting go of the holding (or carrying) hand leaves it carried by the other,
// where it holds it (the hand-off, QC VRTryHandOff with weaponanygrip 2).
enum FreeMode : int
{
    FREE_FOREGRIP = 0, // the aim from the holding hand through where the other hand holds it
    FREE_SUPPORT = 1,  // the aim stays the holding hand's
    FREE_RIGID = 2,    // both hands hold it rigidly (the weapon turns as a prop held in both)
};

// The view, each frame: whether the empty `hand`'s palm is on the other hand's weapon (held or carried), far enough from
// its handle and (held by its handle) from its grip and blade hotspots (vr_weapon_grab_anywhere_min).
void setFreeCandidate(int hand, bool on);
// Whether `hand` holds the other hand's weapon anywhere (a free grip), and as what (FreeMode; -1 none).
[[nodiscard]] bool freeHelping(int hand);
[[nodiscard]] int freeMode(int holding);
// Where `hand` is drawn holding it: its controller's pose as if it were there (the view then draws it as an empty hand
// there, its fingers wrapping the weapon), given the pose the other hand's weapon is drawn from (`holderPos`,
// `holderRot`: the view's drawnAs). `trackedPos`, `trackedRot`: the hand as tracked (its place on the weapon is taken
// from the first frame drawn). False if it holds none.
[[nodiscard]] bool freeHand(int hand, const glm::vec3& trackedPos, const glm::vec3& trackedRot, const glm::vec3& holderPos,
    const glm::vec3& holderRot, glm::vec3& pos, glm::vec3& rot);
// How `hand` helps hold the other hand's weapon (QC weaponanygrip): 0 not, 1 by a hotspot (or the old anywhere 5-25
// units), 2 anywhere (a free grip; or a moment ago).
[[nodiscard]] int helpKind(int hand);
// Where the other hand holds the weapon in (or carried by) `hand` anywhere on it (world), and how far that grip is
// blended in (`t`): its weight turns about between the hands (vr_weight.cpp), as with a hotspot. False: no free grip.
[[nodiscard]] bool freeGripPoint(const hands::State& s, int hand, glm::vec3& out, float& t);
// How much the weapon in `hand` is held two-handed (its weight shared, vr_weight.cpp): the hotspot grip's transition, a
// free grip's.
[[nodiscard]] float support(int hand);

} // namespace qvr::twohand
