// vr_flatattach.hpp -- flatscreen play's Attach key as drawn (flatattachanim; his words 2026-11-27: "there needs to be an
// animated transition for the hand attaching/detaching the dagger (and bayonet too), at the moment it happens abruptly"):
// the free hand to the gun's mount, the key's impulse sent on arrival, the blade slid along the barrel, the hand back
// (vr_flatattach.cpp; the game's side is QC vr_attach.qc VR_Attach_Key; docs/vr-port/FLATPLAY.md, "Attachments").
#pragma once

#include "vr_hands.hpp"

#include <glm/glm.hpp>

namespace qvr::flatattach
{

// vr_flatattach_report (Debug > Reports > Flatscreen Play; the tests).
void init();

// Once a frame, from flatplay::frame (`on`: the mode runs, the fly camera isn't flying): J's press (vr_flat_key_attach)
// starts a trip, or sends impulse 52 at once when nothing is foreseen to go on or come off; the trip stepped, its impulse
// sent as the hand arrives, cancelled by a shot, a weapon change, the attachment let go.
void frame(bool on);

// After flatplay's pose, the ironsight's and the reload's (flatplay::poseHands): the trip's hand moved to its place on the
// gun and turned the gun's way by its share of the trip.
void apply(hands::State& s);

// Whether hand `h` is on a trip (going, at the gun, coming back): it lets go of a two-handed grip (vr_twohand.cpp,
// vr_flatsight.cpp).
[[nodiscard]] bool busy(int h);

// J's press (flatplay's key edge; `what` names it for the prints).
void press(const char* what);

// How far (units, along its blade towards its tip) the attachment drawn on the gun in `hand` is still off its mount: one
// just put on by J, sliding into it (vr_view.cpp setupAttachments); 0 otherwise.
[[nodiscard]] float slideAlong(int hand);

} // namespace qvr::flatattach
