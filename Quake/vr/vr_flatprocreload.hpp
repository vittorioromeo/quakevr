// vr_flatprocreload.hpp -- flatscreen play's procedural reload (flatprocreload; his words 2026-11-28: "adding actual
// procedural reloading animations to flatscreen play, gated behind an option, on by default"): the shotgun held in one
// hand, the other empty, reloaded as a headset reloads it, the free hand driven between the ammo pouch and the gun's
// loading port (vr_flatprocreload.cpp; the game's side is QC vr_flatreload.qc's procedural reload, which takes and loads
// the rounds by VR's own paths in vr_reload.qc; docs/vr-port/FLATPLAY.md, "Reloading").

#pragma once

#include "vr_hands.hpp"

#include <glm/glm.hpp>

namespace qvr::flatprocreload
{

// vr_flatprocreload_report (Debug > Reports > Flatscreen Play; the tests).
void init();
// Once a frame, from flatplay::frame (`on`: the mode runs, the fly camera isn't flying): the server's bits read (the
// procedural reload of a hand's gun up or down), the free hand's trip stepped, the pouch's take sent (impulse 99) as the
// hand gets there, a punch's or the failsafe's cancel sent (impulse 100).
void frame(bool on);
// After flatplay's pose and the force grab's (flatplay::poseHands): the gun tilted to present its port, the free hand moved
// and turned along its trip.
void apply(hands::State& s);
// Whether hand `h` is the free hand of a trip (going, at the pouch, carrying, coming back) or of the key-pose lock: it lets go
// of a two-handed grip (vr_twohand.cpp, vr_flatsight.cpp), the gun keeps its stance, and neither hand pushes the other
// (vr_selfcollide.cpp, vr_view_hands.cpp pushOut): its way is drawn through the gun.
[[nodiscard]] bool busy(int h);
// The key-pose lock (vr_flat_prl_lock, prlfix: to tune the poses) holding a taped pair in the free hand: that hand and the
// pair's size (vr_view.cpp prlPair draws it); false: no pair to draw.
[[nodiscard]] bool lockPair(int& hand, float& scale);
// Where that pair is for the hand drawn at `handPos` turned `handRot` (hands::State angles), and its model's axes (forward,
// left, up): as it sat in the hand at the last real take (a default before one).
[[nodiscard]] glm::vec3 lockPairPlace(const glm::vec3& handPos, const glm::vec3& handRot, glm::mat3& axes);
// Whether flat-screen play shows the ammo pouch (the server says the procedural reload is on: STAT_QVR_FLATRELOAD's 64),
// vr_body.cpp ammoPouchEnabled.
[[nodiscard]] bool pouchShown();
// Flat-screen play's own nudge of the pouch (vr_flat_ammo_pouch_x/y/z in the body's frame of yaw `bodyYaw`) while it is
// shown there; zero otherwise (vr_body.cpp ammoPouchPosition).
[[nodiscard]] glm::vec3 pouchOffset(float bodyYaw);
// And its turn (vr_flat_ammo_pouch_pitch/yaw/roll) and its size (vr_flat_ammo_pouch_scale in place of the headset's
// `headsetScale`), vr_view.cpp setupAmmoPouch.
[[nodiscard]] glm::vec3 pouchTurn();
[[nodiscard]] float pouchScale(float headsetScale);

} // namespace qvr::flatprocreload
