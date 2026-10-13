# Flatscreen play (`-flatscreenmode`)

Quake VR's gameplay played on the desktop screen, with no headset. Everything here is behind the command-line flag
`-flatscreenmode` and only while there is no headset session (`VR_IsActive() == 0`): start the game without the flag and
none of it happens, and a headset that takes the game over turns it off (the game says so in the console).

Engine side: `Quake/vr/vr_flatplay.cpp`. Game-code side: `QC/vr_util.qc` (`VR_FlatPlay`), `QC/weapons.qc`,
`QC/vr_wpnforcegrab.qc`.

## Turning it on

    ironwail.exe -flatscreenmode

With no headset attached the game has no VR backend, which is what the mode asks for. To see it on a machine that does
have a headset, or in a test run: `vr_backend none; vr_restart`.

`vr_flatplay_status` (console, or Debug > Reports > Flatscreen Play Status) says whether the flag was given, whether a
headset session is up, and whether the mode is on.

## What the mode does

**The hands are tracked by the game, not by a headset.** The client marks the player's hands as tracked
(`QVR_BUTTON_HANDSTRACKED`, `Quake/vr/vr_client.cpp`), which is what lets the game run its VR gameplay for a flatscreen
player at all: held weapons, the force grab, melee, the holsters, body touches.

**The arms are posed by the game** (`flatplay::poseHands`, called from `Quake/vr/vr_hands.cpp` in place of the old
straight-out-in-front pose). The pose is built from the view angles, the weapon in each hand and the player's own motion:

- a held weapon sits out in front of the chest, on the centre line; an empty hand hangs at its side; the free hand
  rests near the other hand's gun;
- the hands are turned so the weapon in them shoots along the crosshair, whatever that weapon's Shot Pitch and Yaw
  (`aimRotAtCrosshair` inverts `weapons::shotAngles`); the drawn gun and its muzzle follow the hand, so the shot and the
  viewmodel agree;
- the arms trail a turn and a step (`vr_flat_sway`), bob on a walk and breathe at rest (`vr_flat_bob`);
- the whole pose is measured from the shoulders (`hands::bodyAnchor` with `vr_shoulder_offset`), so it follows the body
  whatever the height calibration; `vr_flat_reach` scales how far the hands sit from them.

These three are plain (not saved) cvars, and they are in the Debug menu: **Debug > Reports > Flatscreen Play** — Status,
Print (`vr_flatplay_debug`: prints each E and V press and where the hands are), Hand Reach, Turn Lag, Walk Bob.

**The keys.** Read from the keyboard state, not from bindings — nothing is rebound and no config changes; they only work
while the game has the keys (`key_dest == key_game`).

| Key | |
|---|---|
| mouse LMB | fires the right hand (as in VR) |
| mouse RMB | fires the left hand (as in VR) |
| `E` | force-grabs what the view is pointed at, with the free hand (`impulse 46` → `VR_Forcegrab_FlatGrab`; the search, the flight and the catch are the game's own force grab). With both hands holding weapons there is no hand to collect with and E does nothing. |
| `1` | cycles the **left** hand's weapon through the weapons collected |
| `2` | cycles the **right** hand's weapon |
| `V` | a punch with the right hand: a real motion of the hand (wound up, thrown, drawn back), so the game's own melee rules decide whether it lands |

`1` and `2` go through the existing weapon cycling (`CycleWeaponCommand`, the controller's B and Y); `getWeaponCycleMode`
returns ALLOWED while the mode is on, whatever `vr_weapon_cycle_mode` says. The other number keys keep Quake's own
weapon select.

**No reloading.** `VRIsWeaponReloadingEnabled` (`QC/vr_util.qc`) is off: a magazine is refilled from the reserve as it
empties, with no handling step.

**The flashlight is worn and always lit.** `vr_flashlight`'s "Always on Head" behaviour (`vr_flashlight 2`) is what the
mode stands in for: `enabled()` and `headMode()` in `Quake/vr/vr_flashlight.cpp` answer as they do for it. There is no
belt to reach for on the screen.

**The body presses the buttons it walks into.** `Quake/vr/vr_physics.cpp` lets a flatscreen player's body do the hand
touch it does for an untracked player, whatever `vr_body_interactions` says.

## Tuning

`vr_flat_reach` (1), `vr_flat_sway` (1), `vr_flat_bob` (1) — all not saved, all in the Debug menu. The pose's own
numbers (where a hand sits, how far a punch throws the fist, how long it takes) are named constants at the top of
`vr_flatplay.cpp`.
