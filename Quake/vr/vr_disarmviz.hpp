// vr_disarmviz.hpp -- Debug > Show Disarm Volumes (vr_debug_disarm): what the armed humanoids' disarm tests test, drawn.
//
// - The QC (vr_disarmviz.qc VR_DV_Frame, each server frame while it is on) sends each held weapon's volumes as its own
//   tests compute them (VR_AG_WeaponLine, VR_AGS_HandLine, VR_AG_DISARM_HAND...: debugdisarm), its state (a tug, a grab
//   that takes it at once: debugdisarmnote 0) and each disarm-relevant hit (parry, disarm, body blow, shot, blocked:
//   debugdisarmnote 1-5, from the tests' own code). Alive or an active ragdoll alike: a ragdoll's weapon rides one of its
//   limbs (VR_AG_RagProp), that limb drawn too.
// - The engine (serverFrame) measures each fist of the listen player against each of those weapons as the grab does
//   (physics::armedGrabGap: handOn's measure): the weapon a grip would take now is the hovered one.
// - draw (with the view's debug draws; flat: vr_ragtool.cpp's overlay) draws it all; nothing is kept or drawn while off.

#pragma once

#include "vr_engine.hpp"

#include <glm/vec3.hpp>

namespace qvr::disarmviz
{

void PF_debugdisarm();     // debugdisarm(e, kind, a, b, radius): a shape this server frame (vr_disarmviz.qc's kinds)
void PF_debugdisarmnote(); // debugdisarmnote(e, what, at, text): 0 e's state this frame; 1-5 a hit (shown a moment)

void serverFrame();                // VR_ServerFrameEnd: the fists measured against the weapons
void draw(const glm::vec3& eye);   // the view's debug draws
[[nodiscard]] bool on();           // vr_debug_disarm (the flat view's overlay draws the lines then)

} // namespace qvr::disarmviz
