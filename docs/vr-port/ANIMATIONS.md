# Recorded animations for the flat-screen hands

Motions recorded in the headset (a melee strike with a one-handed or two-handed weapon, a reload of a given gun) to be
played later on the flat-screen player's hands (`-flatscreenmode`, FLATPLAY.md), in place of or on top of the
procedural pose. Phase 1 (this page, `agent/animrec`): the recorder, the file, the smoothing and a preview. Phase 2:
wiring the recordings to actions (below).

The animation recorder is its own feature (`Quake/vr/vr_animrec.cpp`, `vr_animrec_data.cpp`). It is not the motion
recorder (MOTIONS.md: takes of the melee for its tests); the two share the record button's choices and the countdown
(`Quake/vr/vr_recbutton.hpp`).

## Recording

1. **Debug > Animation Recorder** (in the headset). Set the **Countdown** (`vr_ar_countdown`, 3 s), and either press
   **Start / Stop Recording** or turn on **Arm Record Button** (`vr_ar_armed`) and use the **Record Button**
   (`vr_ar_button`: the motion recorder's choices, off stick click by default; don't arm both recorders on one button).
2. The countdown beeps and buzzes each second (a big number in view; centre-printed on a flat screen); then `REC` and
   the seconds. Do the motion, then press again (or it ends by itself after **Longest**, `vr_ar_maxtime`, 60 s).
3. It is saved at once as `quakevr/anims/<name>.anim`: `<name>` is **`vr_ar_name`** when set (no keyboard in the
   headset: set it from the console or a config beforehand), else the two hands' weapon models and the date
   (`v_shot-empty_2026-11-06_14-03-11.anim`). Never over another file (`-2`, `-3`, ...).

Console: `vr_ar_record [name] [seconds]` (a countdown, then a recording of that long, or until `vr_ar_stop`),
`vr_ar_list`.

**Workflow with the AI.** Record a few motions (say `vr_ar_name melee_1h_down`, record; `vr_ar_name reload_shotgun`,
record), then tell the AI what each file is. Each file's header says the map, the weapons in each hand and every event
with its time, so the AI can check what was recorded before wiring it.

## What a recording holds

Every host frame (72-120 a second): the time; the head relative to the body; the crouch; for each hand, its pose
relative to the head **and** relative to the body (so a playback can choose), the five finger curls as drawn
(`view::fingerCurl`), the controller's grip and trigger values and the held weapon's id. Plus, once: the weapon (id and
model) in each hand at the start and each change; the recorder's own hotspots on his body (the ammo and grenade
pouches, the six holsters); and the **events**:

| kind | from | detail |
|---|---|---|
| `fire` | engine (`weaponfx::playerFired`) | the gun's model |
| `pump` | engine (`autopump::fired`) | when the stroke goes back and comes home |
| `load` | engine (the hand's magazine count going up) | `magazine a -> b` |
| `mag_out` | QC (`VR_Reload_MagFly`, `VR_Reload_MagOutRelease`) | how |
| `mag_in` | QC (`VR_Reload_SeatMagInto`) | held or by contact |
| `pouch_take` | QC (`VR_Reload_Take`) | how many rounds |
| `rounds_in` | QC (`VR_Reload_LoadInto`, the pouch's) | how many |
| `break_open`, `close` | QC (the super shotgun's) | how |

Each event also says which hand, the hotspot that hand was nearest (within 0.4 m; `-` none) with the hand's place from
it, and the hand's place on the body. More QC events: `VR_Anim_Event(player, kind, hand, detail)` (QC `vr_motion.qc`,
builtin `animevent`; only the local player's, only while recording).

### The file

Text, one line each, `#` comments at the top saying all this. Units: metres, degrees, seconds. Axes: forward, left, up.
Angles: Quake's (pitch down positive, yaw left positive, roll), as `hands::basisOf` takes them.

```
anim 1
name reload_shotgun
date 2026-11-06 14:03:11
map vrfiringrange
source openxr (Oculus)
note vr_world_scale 1.2, vr_height_calibration 1.55
frames 431 431                    (frames in the file, frames recorded)
duration 5.9722
smoothed 0                        (1 + the smoothing's parameters and errors in a smoothed file)
spot ammo_pouch 0.0990 -0.0008 0.2021          (his hotspots, on the body)
weapon 0.0000 main 4 progs/v_shot.mdl          (t hand wid model)
event 0.3333 main fire right_shoulder -0.1092 -0.0715 0.2964 -0.1582 -0.1983 1.2198 progs/v_shot.mdl
                                  (t hand kind spot d(from spot) b(on the body) detail)
columns t head_x ... main_head_x ... main_body_x ... main_curl_thumb ... main_grip main_trigger main_wid off_...
f 0.0139 0.00000 0.00000 0.93654 0.419 -0.680 -0.709 0.000 0.55564 ...
```

- **head** frame: the eye position and its whole turn (pitch, yaw, roll). Follows the look.
- **body** frame: `hands::bodyAnchor(s, 0)` (under the head with the lean, lowered as he crouches) turned by the body's yaw
  alone. A look up or down does not move it.
- `head_*` on each frame line: the head in the body frame (so the two frames can be converted; the self-test checks they
  agree to 0.01 mm).

## Smoothing

Real hands shake. **Smooth Last** (or `vr_ar_smooth <name>`) writes `<name>.smooth.anim` beside the recording:

1. a **jitter filter**: exponential smoothing forwards then backwards over time (no lag), time constant
   `vr_ar_smooth_tau` (0.02 s: shakes faster than about 8 Hz go);
2. **keyframe reduction**: the ends, each event's frame and each weapon change are kept; then, again and again, the frame
   where a cubic Hermite spline through the kept keys (Catmull-Rom tangents over the keys' times; rotations as
   quaternions on one side, normalised) strays most from the filtered motion is kept too, until it strays no more than
   `vr_ar_smooth_pos` (4 mm), `vr_ar_smooth_ang` (1.5 degrees) and `vr_ar_smooth_curl` (0.04) anywhere.

The playback samples any file with the same spline, so a raw file plays too. Offline, the same algorithm:
`python Misc/quakevr/anim_smooth.py quakevr/anims/<name>.anim [--tau --pos --ang --curl] [--out] [--check]` (gives
the same keys as the game's: 46 of 145 on the self-test's recording, 3.80 mm, 0.52 degrees).

## The preview

**Play Last** (or `vr_ar_play [name]`, `vr_ar_play_stop`) plays a recording on the flat-screen hands; in the headset it
draws a ghost (the recorded weapon models, a dot and a pointer per hand) at the pose it would give. The smoothed file is
played when there is one (`vr_ar_play_smoothed`). Each event is printed as it passes. The tweaks, all on the page:

- **Placed On** (`vr_ar_play_frame`): Body (0) or Head (1);
- **Mirror** (`vr_ar_play_mirror`): left to right, each hand playing the other's part (its place's left negated, its
  turn reflected in that plane);
- **Forward / Left / Up** (`vr_ar_play_x/y/z`, cm), **Pitch / Yaw / Roll** (`vr_ar_play_pitch/yaw/roll`: the whole
  motion turned about the frame's origin), **Scale** (`vr_ar_play_scale`), **Speed** (`vr_ar_play_speed`), **Loop**
  (`vr_ar_play_loop`);
- **Blend** (`vr_ar_play_blend`, 0.25 s): blended in from the game's own pose at the start and out at the end (each
  loop too, with half a second of the game's pose between): positions mixed, rotations slerped, by a smoothstep weight.

The hands are only moved: the held weapons stay what they are, nothing fires. It is a check of the motion, not yet an
action.

## Phase 2: wiring recordings to actions (the plan)

**An animation table** (a text file, `quakevr/anims/table.cfg`, and a Debug page): one line per action and weapon, e.g.
`melee_swing  wid=axe  file=melee_1h_down  frame=body  x y z pitch yaw roll scale speed  blend_in blend_out  mirror=auto`.
Actions: the flat-screen punch and melee swing (`flatplay` `punchSwing`), each gun's reload (the reload's steps today are
instant in flat), the pump, the weapon draw and holster. Lookups go from the most specific (action + weapon) to the
action's default; per-weapon offsets add to per-animation ones, so a recording made with one shotgun can be fitted to
another without recording again. The author tunes them live from the Debug page (values written back to the table).

**Playback in the game.** The flat pose stays the base (`flatplay::poseHands`); an action's animation is laid over it
with a weight, as the preview does now. The game's own rules still drive the action (the swing's damage, the reload's
ammo): the animation's events are the action's cues. Its design notes (the author, 2026-11-06):

1. **Hotspots differ between players.** His ammo pouch, holsters and the rest may be placed elsewhere than the
   recorder's (they follow each player's settings and body). So a flat animation must not mean "grab at P at time T".
   The file records each step as an event (`pouch_take`, `mag_out`, `mag_in`, `pump`, ...) with its time and the hand's
   place from the recorder's own hotspot. A playback either **retargets** the hand to this player's hotspot over the
   frames around the event (the recorded offset from the spot added to his spot: the hand reaches his pouch, wherever
   it is), or does the step **at time T** without the reach (the round appears in the hand: "magically grab"). The
   table chooses per animation; retargeting is the default for pouches and holsters close to the recorder's.
2. **Either hand.** The recording says which hand did what (main and off, every event's hand), and the preview already
   has the mirror (`vr_ar_play_mirror`): left to right, positions and rotations reflected, the hands' parts swapped.
   The table's `mirror=auto` mirrors when the action's weapon is in the other hand than the recorder's.
3. **A smooth start and end from any pose.** Every pose is stored relative to the body and to the head, so a playback
   can start from wherever the flat IK has the hands and blend in, and blend out to wherever it will be. The preview
   blends in and out already; the self-test plays it level, looking straight up (-80), straight down (80) and crouched:
   the hands never step further in a blending frame than the motion itself does (2.26 against 1.41 units a frame at
   most; within 1.5x + 0.5). Body-frame playback ignores the look's pitch, so a reload played looking down stays at the
   chest; phase 2 should give each animation a "pitch follow" share (0 body, 1 head) and clamp it the way
   `vr_flat_pose_pitch` clamps the procedural pose, so a look straight up does not throw a strike over the head.
   Crouching lowers the body frame with the head (the anchor follows the crouch).
4. **What stays procedural** (recording would not help, or would hurt):
   - **aiming and the idle hold**: the gun must point along the crosshair exactly (flatplay's `aimRotAtCrosshair`);
     recorded aim drifts;
   - **sway, bob, turn lag, recoil**: they answer the player's own movement and shots live;
   - **the two-handed support grip** and the empty hand going to a foregrip: they depend on each weapon's grip points,
     already solved (`vr_twohand`);
   - **a plain punch**: short and rule-bound (the game asks the fist for a speed; FLATPLAY.md "punchSwing"), easier to
     tune as a curve than as a recording;
   - **looking at a hotspot / reaching** when the recorder's spot is far from the player's: the reach itself is a
     two-point IK move (point 1 above).
   Recorded: melee strikes with weapons (the arc, the wrist's turn, the follow-through are what reads as "real"), the
   reloads' hand-work (magazine out, pouch, in, the pump, the super shotgun's break), inspections and draws.

## Tests

`bash Misc/quakevr/animrec_test.sh <agent>` (three runs, each ending on its verdict line):

1. mock headset: `vr_ar_selftest record selftest` after the shotgun is put in the main hand, with `vr_mock_swing 1` and
   `vr_mock_shake 1` and one shot: the countdown, 2 s recorded through the real path, saved, read back (145 frames, the
   main hand's 4 m path, head and body frames agreeing to 0.01 mm, the weapons and 8 hotspots, the shot and the pump
   events), smoothed (46 keys of 145, 3.80 mm / 0.52 degrees within 4 mm / 1.5), written and read back;
2. `anim_smooth.py --check` on the same file (the same 46 keys);
3. `-flatscreenmode`: `vr_ar_selftest play selftest`: the smoothed file played four times (level, up, down, crouched):
   at full weight the hands are on the recording (0.000 units off), the blending's steps as above.
