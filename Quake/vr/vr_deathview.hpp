// vr_deathview.hpp -- the death view (vr_death_view; ROUND21.md, "Player ragdolls"): dying (not gibbed), the player's
// body becomes a ragdoll (QC vr_deathdoll.qc spawns it: a body of its own with progs/player.mdl, rigged in
// vr_ragdoll.cpp, ragdolled at once by vr_box3d.cpp as a dead monster's), and the view:
// - 0 Off: as before (no body; the view where the eyes were).
// - 1 Third Person: the view stays where the eyes were; the body lies there to look at, its head on.
// - 2 Immersive: the view in the body's head (its head bone's eyes, headless as drawn for you), your own head's turns
//   and steps on it; for comfort its place is smoothed, it turns with the head only in yaw (vr_death_view_turn: smoothed,
//   at most vr_death_view_turn_speed), never pitches or rolls, and the view fades in from black as it goes there
//   (vr_death_view_fade); the hands and the gear are not drawn.
// All back on respawning or loading a save (health over 0). The ragdoll is drawn by a listen server only (as every
// ragdoll): a remote player sees the body's death animation, and his own Immersive view is Third Person's.
#pragma once

#include "vr_hands.hpp"

namespace qvr::deathview
{

// The view of eye `eye` (applyEyeView: `origin` and `angles` from the tracked eye, changed in place while the
// Immersive view holds). Once a frame its state is worked out (the first eye's call).
void eyeView(const hands::State& s, int eye, glm::vec3& origin, glm::vec3& angles);

// Whether the Immersive view holds now (the hands and the gear not drawn).
[[nodiscard]] bool immersive();

// A new map, a disconnect: nothing held.
void clear();

// vr_death_view_status: what the death view does now (tests: Debug > Tests).
void registerCommands();

} // namespace qvr::deathview
