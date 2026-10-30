#pragma once

// vr_gunsfx.hpp -- punchier gunfire (ROUND23.md, "Punchier gunfire"; Sound > Gunfire; every part off with vr_gunsfx 0):
// - A shot (a gun's sound on a weapon channel, the player's or anyone's: S_StartSoundPitch's VR_SndShot) plays layers
//   with it: a close mechanical one (a hammer, a bolt's tick, a launcher's clunk) and a low thump (vr_gunsfx_layers),
//   and a tail sized to the room it is fired in (vr_gunsfx_tail: a few rays round it, cached a moment: short and tight
//   in a small room, long in a big hall, long with a distant echo under the sky). Each shot and its layers a little
//   higher or lower, louder or quieter (vr_gunsfx_vary). The layers are made from the game's own sounds as they load
//   (vr_sndlayer.cpp; nothing shipped).
// - An empty gun's dry click (gunclick.wav): its volume, a small tick in that hand (vr_gunsfx_dryfire).
// - A gun's actions (breaking open and closing, a magazine in or out, a shell in) heard from the action on the gun, not
//   the muzzle (vr_gunsfx_action; vr_audio.cpp handSoundAt).
// - Spent casings sound of what they land on: stone (the tinks as before), metal, wood (vr_gunsfx_casings; water's
//   plips as before: vr_shells.cpp).
// - A shot's haptic punch (vr_gunsfx_punch): a sharp spike, then a short tail as long as the weapon's recoil takes to
//   come back (its kick up and Recoil Return Time), as hard as its recoil (kick back and rise: weapons::recoilFeel) times
//   its Haptic Punch (Weapon Offsets > Effects); it replaces the QC's flat buzz for that shot.
// vr_gunsfx_test: the checks (layers, variation, room tails, casings, dry click, the punch's curve per weapon).

#include "vr_sndlayer.hpp"

#include <glm/glm.hpp>

namespace qvr::gunsfx
{

// The gunfire's layered sounds (VR_SndLayered, after its own table): the one named `name`, or null.
[[nodiscard]] const sndlayer::LayeredSound* layered(const char* name);

// Where along the gun (0 the hand, 1 the muzzle) a weapon channel's sound named `sfxName` plays from: an action's sound
// at the action; -1 for the others (the muzzle) and with vr_gunsfx_action 0.
[[nodiscard]] float actionPlace(const char* sfxName);

// A spent casing landing at `at` on a surface facing `normal`: its sound's name (variant 0..2), or null for the tinks
// as before (stone, or vr_gunsfx_casings 0).
[[nodiscard]] const char* casingSound(const glm::vec3& at, const glm::vec3& normal, int variant);

// The player's `hand` fired (weaponfx::playerFired): its haptic punch starts.
void fired(int hand);

// Every frame: the punches' tails.
void frame();

// input::runHaptics: true while `hand`'s punch has just started (the QC's flat buzz for the same shot dropped).
[[nodiscard]] bool punchOwns(int hand);

void prepare(); // (map start: the layers loaded)
void clear();
void registerCommands();

} // namespace qvr::gunsfx
