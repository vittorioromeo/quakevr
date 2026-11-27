// vr_flatgrenade.hpp -- flatscreen play's quick grenade as drawn: G's key, and the hand to the back pouch, back into view
// with the grenade, up into the overhead wind-up and out along the throw (vr_flatgrenade.cpp; the game's side is QC
// vr_flatgrenade.qc; docs/vr-port/FLATPLAY.md, "The quick grenade").
#pragma once

#include "vr_hands.hpp"

namespace qvr::flatgrenade
{

// vr_flatgrenade_report (Debug > Reports > Flatscreen Play).
void init();

// Once a frame, from flatplay::frame (`on`: the mode runs and the fly camera isn't flying): G's press
// (vr_flat_key_grenade: impulse 57), and the hand's shares stepped from the server's STAT_QVR_FLATGREN.
void frame(bool on);

// After flatplay's pose, the ironsight's and the reload's (flatplay::poseHands): the grenade's hand moved to the pouch, the
// held place, the overhead wind-up or the follow-through by their shares.
void apply(hands::State& s);

// The hand holding a grenade now (the server's phase 2..4), -1 none: its fire button is the grenade's
// (flatplay::routeFire: LMB the main hand, RMB the off hand).
[[nodiscard]] int heldHand();

// Whether hand `h` is busy with a grenade (on its way to the pouch, holding one, its throw's follow-through): it neither
// punches nor takes a weapon's grip (vr_flatsight.cpp, vr_twohand.cpp).
[[nodiscard]] bool busy(int h);

// Whether the free hand `helping` lets go of a two-handed grip of the weapon `holding` holds: either hand busy (above).
// The throwing hand's own throw too (his call 2026-11-23): a throw is one-handed, the other hand lets go and takes the
// grip again after, as after the other let-go moves.
[[nodiscard]] bool letsGo(int holding, int helping);

// Whether hand `h` is on the throw key's throw (wound up or following through): what it holds neither fires nor sights
// (his words 2026-11-23; the game refuses the fire too: QC VR_FlatThrow_WindsUpWeapon).
[[nodiscard]] bool throwsWith(int h);

// Whether hand `h` is the throw key's hand while the key is held (its tap window, its wind-up, a quick grenade wound up
// by the key): its attack button cancels the throw (impulse 41: QC VR_FlatThrow_Cancel; flatthrow2, his words 2026-11-26)
// instead of firing or striking.
[[nodiscard]] bool aimsWith(int h);

// The mouse buttons (1 the left, 2 the right) that cancelled a throw and are still held: they do nothing till let go
// (flatplay::routeFire).
[[nodiscard]] int swallowedButtons();

// How far hand `h` guides the other hand's throw (0..1, eased): free, it goes forward, its palm away, its fingers open
// by vr_flat_throw_guide_open (vr_view.cpp's finger curls).
[[nodiscard]] float guideShare(int h);

// How far hand `h` is reaching out for Grab / Pull (H; 0..1, eased) before taking hold: opens the empty hand's fingers
// slightly while reaching, closing once the grabbed object is in hand (vr_flatplay.cpp fistShare).
[[nodiscard]] float grabOpenShare(int h);

// (xhairmiss) While hand `h` winds up a throw (the throw key's, the quick grenade's), how far the hand will have moved
// when the thing leaves it (world units): the follow-through's first vr_flat_throw_release_time, posed as apply poses it
// (QC VR_FlatThrow_Aim aims from there, through the builtin flatthrowfrom). Zero otherwise.
[[nodiscard]] glm::vec3 releaseShift(int h);

} // namespace qvr::flatgrenade
