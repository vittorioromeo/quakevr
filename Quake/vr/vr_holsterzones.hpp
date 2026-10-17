// vr_holsterzones.hpp -- the holsters' and pouches' zones (ROUND22.md, "Holster zones: boxes where they are drawn"):
// an oriented box for each holster and pouch, in the frame it is drawn in (vr_view.cpp publishes it each frame), at the
// holster as drawn, sized from its reach (vr_*_holster_thresh, vr_ammo_pouch_thresh, vr_grenade_pouch_thresh) times
// vr_holster_zone_<kind>_depth/width/height, stretched to take in both the holster as drawn and its reach point (the
// hips' are drawn on the hips a hand's width in from it), moved by vr_holster_zone_<kind>_x/y/z (out, outwards, up).
// A hand's hotspot (body::updateHotspots) is the zone it is deepest in: the held gun's grip and body (its handle, the
// way to its muzzle, the muzzle) for the holsters, the hand for the pouches, which only a hand without a gun can have
// (a gun goes to a holster; the pouches give and take rounds, magazines and grenades). The zone a hand had keeps it
// while within vr_holster_zone_hysteresis more of its size, and wins until another is that much deeper. A hand carrying
// an ammo box, a health box or a backpack (STAT_QVR_CARRYITEM) has every box vr_holster_zone_item_<kind> times larger
// about its middle; a hand without a gun also has a sphere of vr_holster_zone_handle_radius round each holstered gun's
// grip as drawn (publishGrip) as that holster's zone.
// vr_holster_zones 0: the old spheres (body.cpp's hotspot: the hand's distance over the reach). vr_holster_zones_show:
// every zone as a wireframe (Debug > Views; Immersion > Holster Zones).

#pragma once

#include "vr_body.hpp"
#include "vr_hands.hpp"

namespace qvr::zones
{

// Zone indices: the holsters (body::Holster), then the pouches.
enum Zone : int
{
    ZoneAmmoPouch = body::HolsterCount,
    ZoneGrenadePouch,
    ZoneCount
};

// The way a zone's holster or pouch is drawn this frame (vr_view.cpp): `pivot` its reach point (body::holsterPosition,
// the pouches' positions), its turned frame (`out` away from the body, `side` outwards, `up`), `at` the holster model's
// origin (the loops a gun hangs in; a pouch's back) and whether a weapon is in it.
void publish(int zone, const glm::vec3& pivot, const glm::vec3& out, const glm::vec3& side, const glm::vec3& up,
    const glm::vec3& at, bool occupied);

[[nodiscard]] bool enabled(); // vr_holster_zones

// The zone a hand is deepest in (zone index, or -1), the holsters at `holsters`, the pouches at their positions when
// present; remembered for the hysteresis and the wireframes.
[[nodiscard]] int pick(const hands::State& s, int hand, const body::HolsterPositions& holsters,
    const glm::vec3* grenadePouch, const glm::vec3* ammoPouch);

// Zone `zone`'s holster model origin as drawn (the pivot if not drawn) plus `offset` in its frame (out, side, up), for the
// mock hands' tests (vr_mock_hand_to <hand> zone). False for no such zone, or a pouch not there.
[[nodiscard]] bool drawnPoint(const hands::State& s, int zone, const glm::vec3& offset, glm::vec3& out);

// `boxMiddle`: the zone's box's middle instead (its offsets, vr_holster_zone_<kind>_x/y/z, applied; vr_mock_hand_to
// <hand> zonebox), plus `offset` along its axes (out, side, up).
[[nodiscard]] bool boxMiddle(const hands::State& s, int zone, glm::vec3& out, const glm::vec3& offset = glm::vec3{0.f});

// The holstered gun's grip as drawn in holster `zone` (publishGrip; vr_mock_hand_to <hand> zonehandle). False: none.
[[nodiscard]] bool gripPoint(const hands::State& s, int zone, glm::vec3& out);

// The holstered gun's grip as drawn (its model's origin: where the hand holding it is), after the holster's publish:
// a sphere of vr_holster_zone_handle_radius round it is that holster's zone too, for a hand without a gun.
void publishGrip(int zone, const glm::vec3& grip);

// The wireframes (vr_holster_zones_show): idle grey, occupied blue, a candidate (a point of the hand's in it, not the
// deepest) yellow, the one a hand is at green (holding a gun: where it would go); the hands' points as dots.
void queueDebug(const hands::State& s);

// vr_holster_zones_report: each zone (its middle in the body's frame from the eyes, its size) and each hand's points and
// their depth in each zone, the one it is at.
void report_f();

} // namespace qvr::zones
