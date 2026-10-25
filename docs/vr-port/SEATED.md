# Seated play

Seated play is on with **Position: Seated** (`vr_bodycal_seated`). It is the first row of VR Calibration and Body
Calibration, and also **VR Settings > Height and Hand Calibration > Seated Play**. The code is in `Quake/vr/vr_seated.cpp`.
Standing play doesn't change: with Position: Standing every new path is skipped or adds zero.

## What seated mode does

- **The game stands you up.** The head, the eyes, the hands and the tracked body are raised by the standing eye height
  (`vr_height_calibration`) minus the seated one (`vr_seated_height`). This happens where `hands.cpp` places the
  tracking (`seated::raise`). OpenXR's poses aren't changed, so the composition layer and the recorder stay raw.
  Your in-game eyes are at your standing height. The avatar's legs stand planted on the floor and walk with the
  walk/run animation. The crouch, the lean detection and the server's crouched box all see a standing player.
  `vr_seated_stand 0` turns the lift off.
- **Seated height.** VR Calibration's seated height step now writes `vr_seated_height`, not `vr_height_calibration`
  (before, a seated calibration made the body as small as a seated person). The standing height stays as it is. Set it
  standing with Set Height Now, or with the Height slider. Seated Play has **Set Seated Height Now**
  (`vr_seated_height_now`). If the seated height was never measured, it is taken as 0.74 of the standing height.
  `vr_seated_height` is personal and never shipped.
- **Leg tracking is off.** Body tracking's leg weight is 0 when seated (`bodytrack.cpp`). The torso and the arms still
  blend in.
- **Seated holster/pouch layout.** This is an offset set on top of the standing layout (and the crouched one), with
  all of it applied when seated:
  - `vr_hip_holster_seated_*`: x 6, y 1.25, z 3, which puts them forward onto the thighs, a little out, and up (y was -3, in
    from the chair's sides, until the author's values of 2026-10-23).
  - `vr_shoulder_holster_seated_*`: x 3, z 1, which puts them forward over the shoulders, off the chair's back.
  - `vr_upper_holster_seated_*`: 0.
  - `vr_ammo_pouch_seated_*`: x 2, z 3, which puts it up off the lap.
  - `vr_grenade_pouch_seated_*`: x 6, y -5, z 3. The X moves it round the hips from the small of the back to the left
    side.

  Every pair also has a pitch, yaw and roll, all 0. You can edit them in **Holster Calibration > Seated Holsters**
  (also under Seated Play). The body clone is shown while you are on that page.
- **Floor reach** (`vr_seated_reach`, on). Below `vr_seated_reach_start` (0.55 m real, under the thighs), a hand loses
  the lift progressively. At `vr_seated_reach_floor` (0.1 m real) it is as low in the game as in the room, so the game
  floor is in reach from a chair. It is off while a calibration measures you (VR Calibration, Body Calibration's
  capture).
- **Crouch.** Ducking in the chair still crouches, as far as the head goes down. With the mock, the head at 0.85 m gives
  box 44. `vr_crouch_toggle` (seated only) lowers you by `vr_seated_crouch_depth` (0.5 m), eased over
  `vr_seated_crouch_ease` (0.2 s). It has no default button yet.
- **Jumping in the room is off** when seated, because sitting up fast would trigger it.
- **Debug.** `vr_seated_status` (Debug > Views, Print Seated Status) prints the lift, the head and the drawn legs over
  the floor, the leg weights and the holster/pouch positions on the body.

## Headless checks (mock, e1m1)

| | head over floor | ankles (units) | leg weights | right hip holster (fwd/right/up) | server box |
|---|---|---|---|---|---|
| standing (mock head 1.70 m) | 1.700 m | 7.3 | 0 | 0.2 / 11.5 / 28.8 | none (eye 55.5) |
| seated, head 1.2 m, synthetic body tracking on | 1.547 m (= standing height) | 3.1 | 0.00 | 6.0 / 8.5 / 26.9 | 52 (eye 50.7, the same as standing at that height) |
| seated, head ducked to 0.85 m | | | | | 44 |
| seated + `vr_crouch_toggle` | 1.055 m | 2.4 | 0 | | 36 |

A seated hand at 0.2 m real is 7.6 units over the game floor.

## Left to do / to check in VR

- The seated layout defaults are guesses. Tune them seated (Seated Holsters) and promote your values.
- The crouch toggle needs a button: every stick click and face button is bound already. Choose one, or a chord.
- The floor-reach band (0.55 m to 0.1 m) depends on the chair. Check that the thighs and the hip holsters stay outside it.
- Snap or smooth turning is unchanged (smooth by default, the stick turns). A fixed chair needs the stick; a swivel chair
  works as standing does. There is no seated comfort preset.
- Climbing and mantling (hand over hand) and body collision from the head's position work as standing, since the
  lifted head is a standing player's. Not checked in VR.
- In a deep crouch (the toggle at 0.5 m), the drawn knees reach the floor (-0.7 units). This is the existing crouch pose,
  not specific to seated play.
- Torso body tracking stays on when seated. The tracked hips on the chair are lifted with the rest and land near the
  standing hips. Watch for the torso tilting back when you recline.
