// vr_adcrossbow.hpp -- AD support: the crossbow knight's crossbow in a hand, firing as a crossbow (ROUND22 "AD crossbow:
// the bolt laid and fired, the bow animated"; vr_adgun_crossbow_anim):
// - A bolt (AD's progs/proj_bolt1.mdl, the projectile's own model and size) lies in its groove while it is ready to
//   shoot, its back on the string at the latch, along the rail. Fired, it is gone from there (the shot leaves from that
//   very spot: QC VR_ADGun_FireCrossbow); the next one morphs in (the ammo morph's seam: vr_weapon_morph_time's) as the
//   gun is ready again (vr_adgun_crossbow_refire), none once the crossbow is empty.
// - The string and the bow's limbs: two more frames of the gun's model (vr_adguns.cpp turns AD's file and adds them:
//   released, the string straight between the limb tips sprung forward; and the snap past it), blended by the engine
//   as any frames (r_alias.c VR_AliasPoseOverride): a shot snaps the string forward, the limbs flex and wobble back to
//   rest, then the string is drawn back to the latch before the next bolt comes. Cheap: two poses, the GPU's blend.
// Tests: vr_adgun_crossbow_anim_at <s> holds the hands' crossbows <s> after a shot (a frame strip; -1 off);
// vr_adgun_crossbow_info prints each hand's bolt, muzzle and where the shot leaves from (Debug > AD Monsters).

#pragma once

#include "vr_view.hpp"

namespace qvr::adcrossbow
{

// The frames vr_adguns.cpp adds to the turned crossbow (1 released, 2 the snap past it), named so.
inline constexpr int addedFrames = 2;
inline constexpr const char* frameNames[addedFrames] = {"xbow_loose", "xbow_snap"};

// AD's crossbow model as measured (AD 1.80p1: its string's and limbs' vertices are known): the frames are added.
[[nodiscard]] bool knownModel(int numVerts, int numTris);

// Vertex `v` at `p` (the turned model's units: front +x, the bow across y) in added frame `frame` (1 or 2).
[[nodiscard]] glm::vec3 framePoint(int frame, int v, const glm::vec3& p);

// QVR_SVC_FIRED for the player's hand `hand` (vr_weaponfx.cpp playerFired).
void fired(int hand);

// Every frame, the guns placed: the hands' crossbows' frames and their bolts.
void frame(view::ViewEntity (&weapons)[2], view::ViewEntity (&bolts)[2]);

// A new map, a disconnect.
void clear();

// vr_adgun_crossbow_info.
void registerCommands();

} // namespace qvr::adcrossbow
