# Round 21

## Motion recorder

Your proposal: record your own motions, each labelled with what it should do, so that the melee can be tuned
against them. Recording is in (playback and evaluation follow): `docs/vr-port/MOTIONS.md` has the steps, the
categories and every column.

- **Where**: VR Settings > Advanced VR Options > **Motion Recorder** (under Melee). Pick the **Category** (named by
  the result expected: Expected Slash, Expected Stab, No Hit, Expected Bash, Expected Parry Pose, Expected Parry Bash,
  Expected Hilt/Pommel, Expected Punch, Expected Palm Shove 1H / 2H, Expected Gun Strike, Other) and optionally a
  **Detail** (the swing's direction, the weapon, ...), turn **Arm Recorder** on, go back to the game.
- **Recording**: in the firing range, in front of the training dummy, **click the off hand's stick** to start a
  take (a beep, a buzz, "REC slash #4" in view), do the motion, **click again** to end it ("SAVED slash #4 (slash:
  4)"). Many in a row: the category stays chosen, and each take is a new file,
  `quakevr/motions/<label>_<date>_<time>.csv`, counted per category in the menu (the files, so across sessions).
  **Delete Last Take** in the menu moves a bad one into `motions/discarded/`. A double click (under 0.2 s) is not
  kept (a buzzer).
- **What a take holds**, every frame at the headset's rate: the head and both hands as the runtime reported them
  (positions, orientations, velocities) and as the game placed them, in the world, relative to you (as you faced
  at the start) and relative to the dummy; the buttons, the analog trigger and grip, the fingers; what each hand
  holds, the two-handed grip, the weapon's far end, its striking points and its handle's end as the melee computes
  them (a sword's pommel and hilt, the axe's and the hammer's handle); your position, velocity, yaw and view; the
  dummy's position, angles, box and class; the parry and bash-guard state; and every melee event the game
  registered (hits with their damage, kind and striking point, strokes, pushes, parries, batting). 0.5 s before the
  click is kept too (the lead-in) and 0.3 s after (late hits).
- **The button**: the off hand's stick click is the one control free during melee on every controller (Touch
  through SteamVR or VDXR, Index, Vive, WMR); while the recorder is armed it doesn't run (its binding rests).
  `vr_motion_button 1` uses the main hand's instead.
- Console: `vr_motion_list` (the counts), `vr_motion_note "..."` (a note in each take's header: the Other
  category's description), `vr_motion_record <label>` / `vr_motion_stop`.

Tested in the mock headset (firing range, the dummy 14 units away, a sword swung one-handed and two-handed through
`vr_mock_play`): takes start and end on the clicks, a double click is dropped, the files hold every column
(`MOTIONS.md`), the striking points and weapon lines follow the sword, the two-handed grip is recorded, and the
dummy's hit ("melee: overhead blow with mid-blade") is in the take's events.

**Send me** the `quakevr/motions` folder (without `discarded/`) once you have some takes.
