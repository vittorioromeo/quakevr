# Motion takes: the motion recorder

The melee (swords, weapons used as clubs, fists, parries, bashes, shoves) keeps reading some real motions as
the wrong thing. The motion recorder lets you record takes of your own motions in the headset, each labelled with
what it should do ("expected slash", "no hit", ...), in front of the firing range's training dummy. Played back in
the mock headset against the same dummy, the takes test the melee code: each should register what its label
says, and the "no hit" ones nothing.

- `Quake/vr/vr_motion.cpp`: recording, the take file, the menu's helpers (`vr_motion.hpp`, `vr_motion_take.hpp`).
- `Quake/vr/vr_motion_play.cpp`: playback and evaluation.
- `Quake/vr/vr_motion_review.cpp`: reviewing the takes in the game (the Review Takes page, the ghost, the verdicts'
  file); `vr_motion_child.cpp`: its re-evaluation in a second copy of the game.
- `QC/vr_motion.qc`: what the QC tells the recorder (the melee's events, striking points and parry state).
- Takes: `quakevr/motions/<label>_<YYYY-MM-DD_HH-MM-SS>.csv`: your own data, like `notes/`: not in git, not in
  the package.

## Recording in the headset

1. **Go to the firing range**: VR Settings > Advanced VR Options > Play > Firing Range (`map vrfiringrange`).
2. **Stand in front of the training dummy** as you would fight it. Where you stand and which way you face,
   relative to the dummy, is recorded every frame (and playback puts you there again), so a swing that reaches
   it in the take reaches it in the replay. Move as you like during a take: steps and the stick are recorded too.
3. **Take the weapon** the motion is for (the sword, the axe, a gun, or empty hands), one hand or two.
4. **Open the menu**: VR Settings > Advanced VR Options > **Motion Recorder** (under Melee):
   - **Category**: what the motion should do (the list below). **Detail** (optional): which kind, e.g. the
     swing's direction; its takes also count with the category's.
   - **Arm Recorder**: On.
   - The line under Detail shows how many takes the label and the category have so far.
5. **Close the menu** (Back to Game). "armed: slash #4 (click the off stick)" floats low in view.
6. **Click the off hand's stick** (press it down): a beep and a buzz, and "**REC** slash #4  0.0 s" counts up. Do the
   motion. **Click again** to end it: 0.3 s more is recorded (hits that land late), then a click sound, a buzz,
   and "SAVED slash #4 (slash: 4)". The console (the wrist log) names the file.
7. **Repeat**: the category stays chosen, and every take is a new file (none is ever overwritten). The counts
   are the files in `quakevr/motions`, so they carry on across sessions.
8. **A take that went wrong**: Motion Recorder > **Delete Last Take** moves it into `motions/discarded/` (no longer
   counted); again, the one before. A take shorter than 0.2 s (a double click) isn't kept: a buzzer sound and
   "NOT KEPT: too short".
9. **Disarm** when done (Arm Recorder: Off): the off stick's click runs again. While armed its own binding (run)
   rests.

If a take could not be written (a full disk), the recorder says so loudly ("NOT SAVED!", a buzzer, a long buzz, a
console warning): it tries the game folder, and failing that keeps the take in memory: `vr_motion_save_unsaved`
tries again.

Hints: several takes of each, at different speeds and strengths, close and farther away, and the one-handed and
two-handed versions (Detail tells them apart, and the weapon is in the file anyway). For **No Hit**, the motions
that must do nothing: wiggles, weak or slow swings, reloading, aiming, walking with a weapon, reaching past the
dummy. **Other** is for anything else: say what it is with `vr_motion_note "..."` in the console (it goes into
every take's header until you clear it: `vr_motion_note ""`).

**The dummy's attacks** (the Dummy Attacks button beside it) are off unless you turn them on, and off again at every
map load. With them on, your takes can hold real parries and counters (a counter bash is a `parrybash`); the take
says so (`dummy attacks: on`) and records each of its blows, and its replays have the dummy strike at the same
moments. Takes that should not depend on the dummy (all the categories but Parry Bash and Other) are best recorded
with them off.

### Categories

| Category (menu) | Label | Meaning | Details |
|---|---|---|---|
| Expected Slash | `slash` | a cut with the blade (sword, axe's head, hammer's head) | overhead, horizontal_ltr, horizontal_rtl, diagonal_down_left, diagonal_down_right, backswing_up_left, backswing_up_right |
| Expected Stab | `stab` | a sword's tip thrust into the target (a sword only) | one_hand, two_hands |
| No Hit | `no_hit` | nothing at all: no blow, bash, shove, headbutt or batting | wiggling, weak, idle, slow_waving, reloading, aiming, walking, reaching |
| Expected Bash | `bash` | the weapon's guard pushed into the target | sword_1h, sword_2h, gun |
| Expected Parry Pose | `parry_pose` | a guard that parries a blow from the target, held (no hit) | sword_1h, sword_2h_blade (a hand on the blade), sword_2h, gun |
| Not Parry Pose | `not_parry_pose` | a pose that must not count as a parry stance (near a guard but not one), and no melee event | weapon_angled, hands_up, resting, aiming, other |
| Expected Parry Bash | `parry_bash` | the parry pose, then pushed into the target | sword_1h, sword_2h_blade, sword_2h, gun |
| Expected Hilt/Pommel | `hilt_pommel` | a hit with the hilt, the pommel or the handle's end (sword, axe, hammer), not the blade or head | |
| Expected Punch | `punch` | a fist's blow | straight, jab, hook, uppercut, overhead |
| Expected Palm Shove 1H | `palm_shove_1h` | one open palm shoving | |
| Expected Palm Shove 2H | `palm_shove_2h` | both palms shoving | |
| Expected Gun Strike | `gun_strike` | a gun used as a club | swing, butt |
| Other | `other` | anything else (`vr_motion_note`) | |

The "parry state" the evaluation reads, each server frame of the replay's labelled part (phase `rec`): the parry test
is `m_parry`, `o_parry` or `parry_arms` (QC `VR_Parry_Blocks` towards the target for either hand's weapon, or
crossed empty arms, `VR_Parry_ArmsCrossed`); the bash guard is `guard` >= 0 (QC `VR_Bash_Guard`). Expected Parry Pose
needs the parry test in at least half of those frames; Not Parry Pose in none of them, nor the guard, nor any melee
event.

A take's label is `<category>` or `<category>_<detail>` (`slash_overhead`, `parry_pose_sword_2h_blade`).

### Console

| Command / cvar | |
|---|---|
| `vr_motion_armed 0/1` | arms the recorder (the menu's Arm Recorder; not saved: off at each start) |
| `vr_motion_category N`, `vr_motion_detail N` | the category and detail (indices, as in the menu) |
| `vr_motion_button 0..9` | the record button: the off hand's stick click (0, the default) or the main hand's (1); A (2), B (3), X (4), Y (5); the off grip (6) or the main grip (7); hold B and pull the main trigger (8), hold Y and pull the off trigger (9) |
| `vr_motion_preroll 0.5` | seconds kept from before the take starts (phase `pre`: the lead-in for the melee's trackers) |
| `vr_motion_tail 0.3` | seconds recorded after the take ends (phase `tail`) |
| `vr_motion_note "..."` | a note written into each take's header |
| `vr_motion_record [<label>]`, `vr_motion_stop` | a take from the console (by default the menu's label) |
| `vr_motion_list [<category or label>]` | the takes of each category and detail, or the files of one |
| `vr_motion_discard` | the menu's Delete Last Take |
| `vr_motion_save_unsaved` | tries again to write takes that could not be written |

**Why the stick click.** It is the one control that is free during melee on every controller Quake VR supports
(Touch through SteamVR and Virtual Desktop's VDXR, Index, Vive wands' trackpad click, WMR): the grips hold the
weapons, the triggers fire, A/B/X/Y jump, change weapons and reload, Y at the mouth records voice notes. The off
hand's click is only "run" (the speed key), which a take doesn't need; it is taken from the key bindings while
armed (`vr_input.cpp` asks `motion::stickClick` first), so it never reaches the game. It needs no aim: a thumb press,
also in the middle of a two-handed grip. Press to start and press to end (not hold), so the thumb is free during
the motion.

**Other record buttons** (2026-09-30, "the thumbstick can be annoying to press"): `vr_motion_button` (VR Settings >
Advanced > Motion Recorder, Record Button) also picks A, B, X, Y, either grip, or a combination: hold B (Y) and pull the
main (off) trigger. Whichever it is, while armed it is the recorder's: its key never reaches the game (A: no jump; B,
Y: no weapon change; X: no reload; a grip: no grab), and a press it took has its release taken too (`vr_input.cpp` asks
`motion::button` for every button). For a combination the face button alone does nothing, and the trigger pulled
without it fires as usual. Y at the mouth still records a voice note. The HUD's prompt names the button ("armed: slash
#4 (hold B, pull the main trigger)"). A take recorded with a grip bound keeps that grip out of its rows (`m_grip` 0 too),
so a replay never grabs with it.

## The take file

A CSV file: `#` header lines, then one header row and one row per host frame (the headset's rate). Columns are
found by name (readers must not rely on their order: more may be added; `format` changes when one changes its
meaning).

### Header lines

| Line | |
|---|---|
| `format` | the file format's version (1) |
| `label`, `category`, `detail` | what the take should be (`category` empty for a label of `vr_motion_record`'s own) |
| `note` | `vr_motion_note` as the take ended |
| `take` | its number among the label's takes (the count + 1 as it started) |
| `date` | when it started (local time) |
| `source` | the backend and runtime (`openxr (Oculus ...)`, `mock`), or `replay of <file>` |
| `map` | the map |
| `dominant hand` | `vr_lefthanded` |
| `main weapon`, `off weapon` | at the take's start: QC weapon id (`wid`, below), its flags, the model, and the two-handed grip |
| `vr_world_scale`, `units per metre` | Quake units per real metre (`26.2467 * vr_world_scale`: 32.81 at 1.25) |
| `vr_height_calibration`, `vr_floor_offset` | the player's calibrated height (metres) and the floor offset (units) |
| `hand angles` | `vr_gunangle`, `vr_gunyaw`, `vr_offhandpitch`, `vr_offhandyaw`, `vr_controller_legacy_pose` |
| `grips` | `vr_weapon_grip_mode`, `vr_2h_mode` |
| `server rate` | `host_maxfps`, and the server's frame time (72 Hz, or every host frame) |
| `yaw0` | the player frame's heading: the head's world yaw at `t` = 0 (degrees) |
| `origin0` | the player's world origin at `t` = 0 |
| `target` | the nearest monster at `t` = 0: classname, entity number, targetname, origin, angles, box |
| `dummy attacks` | `on`: the training dummy was striking back at some moment of the take (`vr_dummy_attacks`, the firing range's button; ROUND21.md, "Dummy attacks"). Its blows are the `strike` events, and a replay has it strike exactly then. Not written when it wasn't |
| `melee settings` | every `vr_melee_*`, `vr_bash*`, `vr_shove*`, `vr_parry*`, `vr_deflect*`, `vr_headbutt*` cvar |
| `settings` | every archived `vr_*` setting (`name=value`, spaces as `_`) and `host_maxfps`: what playback sets again |
| `weapon settings` | the weapon offsets (`vr_wofs_*`) of the empty hand's slot and of the weapons in the hands as the take is saved |
| `rows` | how many, and how many in each phase |

### Frames and units

- **World**: Quake's (x, y horizontal, z up), in units. Columns ending in `_w_x` .. `_w_z`, `org_w_*`.
- **Player frame** (most columns): from the player's origin *of that row* (`org_w_*`: the middle of the player's
  box, which follows the head as you walk; the hands and head are placed from it), turned by `yaw0`: **x forward**
  (where the head faced as the take started), **y left**, **z up**. Positions in units (`_u`) and metres (`_m`);
  yaws relative to `yaw0`.
- **Dummy frame** (`_d`): from the monster's origin (`mon_w_*`), turned by its yaw (`mon_yaw_w`): **x its facing**,
  y its left, z up. Units and metres. Empty without a monster.
- **Tracking space** (`raw_*`): the runtime's own, metres, **+x right, +y up, -z forward**, origin on the play
  space's floor. Exactly what the runtime reported (after `vr_controller_legacy_pose`'s conversion), which playback
  feeds back in.
- Angles are Quake's: pitch (down positive for the view, as `vr_dumpview` prints), yaw (left positive), roll, in
  degrees. Velocities in metres (radians) per second.
- To turn a player-frame position into the dummy frame: `world = org_w + R(yaw0) * p_u`, then
  `d = R(-mon_yaw_w) * (world - mon_w)`, R a turn about z.

### Columns

Timing and phase:

| Column | |
|---|---|
| `frame` | the row's index |
| `t` | seconds from the take's start (the click); negative in the lead-in |
| `phase` | `pre` (the lead-in, before the click), `rec` (the labelled motion), `tail` (after the second click) |
| `dt` | the engine's frame time (seconds since the previous frame) |
| `realtime` | the engine's clock |
| `xr_time` | the runtime's time of the poses (its clock) |
| `sv_tick`, `sv_dt` | 1 when a server frame ran in this host frame, and its frame time (the melee runs there, at 72 Hz) |
| `sv_time` | the server's time of the last server frame (the QC's columns are from it) |

The player:

| Column | |
|---|---|
| `org_w_x/y/z` | the player's origin (world units) |
| `yaw` | the head's world yaw |
| `play_yaw` | the play space's turn (smooth or snap turning; degrees) |
| `pvel_*_u/_m` | the player's velocity (the server's; player frame, units/s and m/s) |
| `sv_org_w_x/y/z`, `sv_onground` | the server's origin of the player (the client's, `org_w`, lags it a little while moving), and whether it stands on the ground |
| `lean_*_u/_m` | the head's lean off the box's middle (`vr_lean_radius`; player frame) |
| `body_yaw` | the torso's yaw (between the head and the hands; relative to `yaw0`) |
| `crouch` | 0 standing .. 1 crouched |
| `view_pitch/yaw/roll` | the head's angles (yaw relative to `yaw0`) |
| `aim_pitch/yaw/roll` | the view angles sent to the server (the main hand's aim) |
| `org_d_*_u/_m`, `view_yaw_d` | the player's origin in the dummy frame, and the head's yaw from the dummy's facing |

The head:

| Column | |
|---|---|
| `head_*_u/_m` | the eyes' middle (player frame) |
| `head_w_x/y/z` | world |
| `head_d_*_u/_m` | dummy frame |
| `head_vx/vy/vz` | the head's velocity (the runtime's; player frame, m/s) |

Each hand, `m_` the main hand and `o_` the off hand:

| Column | |
|---|---|
| `m_pos_*_u/_m`, `m_pos_w_x/y/z`, `m_pos_d_*` | the hand where the game has it (the controller's point, after collisions with walls and a heavy weapon's lag): player frame, world, dummy frame |
| `m_pitch/yaw/roll` | its angles as the game uses them (the gun angle offsets and the two-handed grip's turn included; yaw relative to `yaw0`); `m_yaw_d` from the dummy's facing |
| `m_vx/vy/vz`, `m_avx/avy/avz` | the runtime's velocity and angular velocity (player frame, m/s and rad/s) |
| `m_trigger`, `m_grip` | the analog trigger and grip (0..1) |
| `m_thumb` | the thumb on a button, the stick or the thumb rest |
| `m_stick_x/y` | the stick (x right, y forward) |
| `m_buttons` | 1 trigger, 2 grip, 4 A/X, 8 B/Y, 16 stick click, 32 menu (the recorder's own buttons taken out; a grip it took also has `m_grip` 0) |
| `m_curl_thumb/index/middle/ring/pinky` | the drawn fingers' curl, 0 open .. 1 curled |
| `m_wid` | the QC weapon id: 0 empty, 1 grapple, 2 axe, 3 Mjolnir, 4 shotgun, 5 super shotgun, 6 nailgun, 7 super nailgun, 8 grenade launcher, 9 proximity gun, 10 rocket launcher, 11 lightning gun, 12 laser cannon, 13 sword |
| `m_wflags` | its flags (1 the other ammo: the hell knight's sword; 2 carried by the foregrip) |
| `m_model` | its view model |
| `m_helping` | this hand steadies the other hand's weapon (two-handed) |
| `m_2h`, `m_2h_t` | this hand's weapon held two-handed: 0 no, 1 by the foregrip / grip, 2 by the blade; and how far into the grip (0..1) |
| `m_carried` | a gun hanging from this hand by its foregrip (the hand-off) |
| `m_hotspot` | where the hand is (0 none, 1/2 the other hand's two-handed grab, 3..6, 8, 9 holsters, 7 the hand switch, 10 a carried gun's handle) |
| `m_muzzle_ok`, `m_muzzle_*`, `m_muzzle_d_*` | the weapon's far end as the engine finds it on the model (a sword's tip, the axe's head, a muzzle): player and dummy frames |
| `m_pt_names`, `m_pt0..5_*_u/_m` | the striking points as the melee computes them (QC `VR_Blow_Points`), names separated by `\|`: a sword's pommel, hilt, guard, mid-blade, blade, tip; the axe's and the hammer's hand, handle, head; a gun's grip, barrel, muzzle; a fist's fist and knuckles. Player frame |
| `m_butt_*`, `m_end_*` (and `_d_`) | the weapon's line (QC `VR_Parry_WeaponLine`): its butt (a sword's pommel, the axe's or the hammer's handle end, a gun's grip) and its far end. Empty for an empty hand |
| `m_parry` | the QC's parry test: 1 while this hand's weapon would parry a blow from the monster (`VR_Parry_Blocks`; from straight ahead without one) |

The melee's state (QC, the last server frame):

| Column | |
|---|---|
| `parry_arms` | crossed empty arms would parry |
| `guard` | the bash's guard held now (`VR_Bash_Guard`: the hand, 0 off, 1 main, -1 none) |
| `guard_hand`, `guard_ready` | the guard the bash follows, and 1 while it is ready to push (held still long enough) |
| `qc` | more values from `VR_Motion_Sample` as `key=x:y:z;...`: `m_blow`, `o_blow` = the hand's stroke: wrist speed (m/s), the stroke's peak (0: none), how fast the weapon turns (degrees/s) |

The nearest monster (the training dummy):

| Column | |
|---|---|
| `mon_ent`, `mon_class`, `mon_targetname`, `mon_health` | which (the entity number, for finding it again) |
| `mon_w_x/y/z` | its origin (world) |
| `mon_*_u/_m` | its origin (player frame) |
| `mon_pitch`, `mon_yaw_w`, `mon_roll`, `mon_yaw` | its angles (world), and its yaw relative to `yaw0` |
| `mon_mins_*`, `mon_maxs_*` | its box, from its origin (world axes: the box doesn't turn) |
| `mon_dist_u` | from the player's origin to the nearest point of its box (units) |

The melee's events (`events`): what the melee registered in this frame (its server frame), `;` between events, each
`kind:sub:hand:value:x:y:z:target:detail` (x y z where, in the player frame, units; empty when unknown):

| kind | sub | value | target, detail |
|---|---|---|---|
| `melee` | the blow's kind: `punch`, `slap`, `overhead_blow`, `uppercut`, `backhand`, `swing`, `thrust`, `rising_swing`, `stab` | damage | what was hit; the striking point (`the tip`, `mid-blade`, `the pommel`, `the knuckles`, ...) |
| `bash`, `parrybash` | the hand: `main`, `off`, `both` | damage | what was hit |
| `shove` | `main`, `off`, `both` | damage | what was hit |
| `headbutt` | | damage | what was hit |
| `stroke` | the blow's kind | its strength (damage multiplier) | a stroke became a blow, whether it hit or not |
| `push` | `bash` or `shove` | 0 | a bash's or shove's push, whether it met anything or not; `one hand` / `two hands` |
| `parry` | `main`, `off`, `arms` | the blow's damage | a monster's blow parried; its attacker |
| `deflect` | the projectile's class | 0 | a projectile batted back; its thrower; `swing` or `bash` |
| `strike` | the training dummy's attack (`dummy attacks: on`): `windup` (the tell begins), `blow` (it struck you), `miss` (nobody within reach at the blow) | a blow's damage (before a parry) | the dummy; a blow's `parried` or `parried with the arms` (its `parry` event is in the same frame) |

The runtime's tracking (tracking space, see above):

| Column | |
|---|---|
| `raw_head_px/py/pz`, `raw_head_qw/qx/qy/qz` | the head's position and orientation (a quaternion) |
| `raw_head_vx/vy/vz`, `raw_head_wx/wy/wz` | its velocity and angular velocity |
| `raw_head_valid`, `raw_head_vvalid` | the pose and its velocities are valid |
| `raw_m_*`, `raw_o_*` | the same for each hand's controller, and `gx/gy/gz`, `gvalid`: the palm's velocity (with `vr_controller_legacy_pose`) |

For the QC side (the melee agent): the events come from `VR_Motion_Event` calls in `T_DamageImpl` (the hits: kinds
from `VR_SetHitKind`), `PlayerVRMeleeImpl` (strokes), `VR_Bash` (pushes), `VR_Parry`, `VR_Deflect_Send` and the
training dummy's attacks (`QC/vr_dummy.qc`: strikes), and the
points and values from `VR_Motion_Sample` (`QC/vr_motion.qc`). Keep them when the melee changes (the evaluation's
expectations are written in these kinds), or change both.

## Playback (the mock headset)

`vr_motion_play <take> [options]` plays a take in the mock headset (`vr_backend mock`), in a map already loaded
(`map vrfiringrange`). `<take>` is a file name in `quakevr/motions`, a path relative to the game folder, or an
absolute path (`.csv` optional).

1. **Setup** (1.3 s of game time, holding the take's first pose): the settings that place the hands and the body are
   set as the take has them (`vr_world_scale`, `vr_height_calibration`, `vr_floor_offset`, the hand angles, the grip
   and two-handed settings, the lean, the body, the weapons' offsets: the header's `settings` and `weapon settings`
   lines, or for the first takes their older lines), and put back afterwards. The player is put where the take has
   them relative to its monster: the same offset from the map's monster of the same class (the training dummy), not
   turned (a monster's box doesn't turn, and the dummy's yaw changes as it is hit), so the geometry of the contacts is
   the take's; another kind of target (or `yaw`), the take turned so the player faces its front. A spot in solid (a take recorded in noclip) lifts the player until it is free. The
   main hand's grip is pressed, the weapons given (QC `VR_Motion_Equip`: the recorded weapon ids and flags), the off
   hand's grip pressed (the two-handed grip is taken again), then every control as the take starts, and the player
   set moving as the take starts (its velocity and, in takes from after the first ones, the server's own origin). A
   take whose player is still moving as it starts (walking into place, pushed back) starts later in its lead-in,
   where the player stood still for a few frames, when there is such a moment.
2. **Play**: frame by frame, the take's tracking (the `raw_*` columns: head and hands, velocities) and controls (`*_buttons`,
   the analog trigger and grip, the thumb, the sticks; never the menu button) replace the mock's, each host frame
   lasting the take's `dt`, the server running exactly where it ran in the take (`sv_tick`, `sv_dt`), the play space
   turned as it was (`play_yaw`; the main stick's own turning is taken out, it is in `play_yaw`), the lean as it was.
   So the melee sees the same poses at the same times, whatever the machine's speed; the game runs faster than real
   time (`watch`: at the take's pace). The controllers' key commands (the grips' `+grabmain`, the trigger's
   `+attack`) run in the frame of their press, as in the headset, ahead of anything waiting in the command buffer.
3. **After** it, 0.3 s holding the last pose (late events), then a report: the take's events and the replay's
   (from the take's start; strokes left out), how many of the hits match (the same kind, sub, hand, target and
   striking point; their damage and time differences), and how far the replay's hands were from the take's,
   relative to the target (from its origin; a synthetic take's in the dummy's frame).

`rand()` is seeded the same at each start, the setup runs a server frame with every host frame at 72 Hz, and the play
follows the take's frames: a replay is the same every time. `vr_motion_eval` goes further: from its `map` command to
the take, every frame is a fixed 1/72 s with a server frame (and `rand()` seeded before the load), so each take starts
at the same server time in the same state; two evaluations of the same takes write the same table, digit for digit
(checked: the replays' 386 columns identical but `t`'s last digit, from the engine's absolute clock). The map loads
run at `developer 0` (their thousands of "can't find" texture lines cost seconds), the takes at yours, and
`host_maxfps` is raised to 1000 meanwhile (the game time is the takes' own either way): about 1.6 s a take.

Options: `target <classname|#entity>` (another target), `yaw <degrees>` (the player's heading: another placement),
`rate <hz>` (the take resampled at another headset's frame rate: poses slerped, velocities interpolated, the server
frames left to the engine; to check the melee at 72, 90, 120 or 144 Hz),
`noplace` (where the player is), `watch` (the recorded pace), `save` (the replay as a take:
`motions/replays/<take>_replay.csv`, the same columns, to compare with the take), `recorded` (the take's melee
settings too: `vr_melee_*`, `vr_bash*`, `vr_shove*`, `vr_parry*`, `vr_deflect*`, `vr_headbutt*`; without it the
current ones: what a change of the melee's settings does), `quiet`. `vr_motion_play stop` stops it.

**The training dummy's attacks** (`vr_dummy_attacks`, ROUND21.md "Dummy attacks"): a replay turns them off (and back
as they were afterwards), so the dummy never strikes on its own timer during a replay or an evaluation (the map loads
turn them off too). A take recorded with them on (`dummy attacks: on`) has the dummy strike when the take says
instead: each of its `strike` events (a wind-up, a blow with its damage, a miss) is done by the take's target (QC
`VR_Dummy_Replay`) in the server frame that plays the event's frame, after the player's, as when it was recorded; the
report says how many. So the parries, the stamina, the counter's window and the knockback come as they came live (a
counter bash replays as `parrybash`, not as a plain `bash`). Against another kind of target they are left out (a
note says so).

A take without a monster (recorded far from any) is played with the target 40 units ahead, facing the player. A
take whose spot is inside the target (recorded in noclip, or a synthetic take whose box meets the target's turned
another way) steps the player back until the boxes are apart, and says so.

Takes recorded before the header's `settings` line (the first ones) take the placing settings they don't list (the
weapons' offsets, the lean) from the current config: play them with the config they were recorded with (the
author's `ironwail.cfg`, or `exec` a file of its `vr_wofs_*`, `vr_2h_*`, `vr_lean_*` ... lines first).

## Evaluation

`vr_motion_eval [<folder, pattern or take>] [options]` plays every take (by default all of `quakevr/motions`; a
folder: its `.csv` takes; a pattern: `punch_*`, `no_hit_*`, in `motions/` or a folder), each in the map loaded
afresh (`vrfiringrange`, or `map <name>`: the same start every time), judges each against
`quakevr/motions/expect.cfg`, and writes the table `motions/eval_<date>_<time>.csv` (or `out <file>`):

| Column | |
|---|---|
| `file`, `label`, `weapons` | the take, its label, the weapons in its hands (fist, axe, mjolnir, sword, gun) |
| `expected` | its expectation (expect.cfg: the label's line, else the category's) |
| `verdict` | `PASS`, `FAIL`, `N/A` (the category means nothing with this weapon: a stab with a gun), `-` (no expectation), `ERROR` |
| `reason` | what failed: `no melee/stab`, `unexpected shove/main main 4.0`, `parry pose held 3 of 390 frames` |
| `events` | the replay's events (hits, pushes, parries, batting, the dummy's strikes), with their times |
| `recorded_events` | the take's own, as registered live |
| `same_hits_as_recorded` | `yes` when the replay hit exactly as the take did live (`-` for a synthetic take) |
| `frames`, `hand_error_u` | the take's frames, and the hands' largest distance from the take's (relative to the dummy, units) |

The console prints a line per take, the totals, the pass rate of each category, and how many replays hit as their
takes did live. Options: `recorded` (the takes' melee settings: to check that the replays reproduce the live
events), `rate <hz>` (every take resampled, as above), `save` (every replay as a take), `verbose` (each playback's report), `watch`, `map <name>`, `out <file>`,
`quit` (quits when done: for scripts), `list <file>` (the takes named in a file, a path a line; relative to `motions/`),
`progress <file>` (a file told `k/N <take>` after each take and `done N` at the end: the review's re-evaluation).
`vr_motion_eval stop` stops it (and writes what it has).

Besides the table, each take's verdict goes into **`eval_status.csv` next to the takes** (`motions/eval_status.csv`;
for a folder, in that folder): the verdict, the reason, the expectation, the replay's and the take's own events, when it
was evaluated. Merged: an evaluation of some takes (`punch_*`) updates theirs and keeps the others'. Only from an
evaluation that judges the takes as they are: not with `rate`, `recorded` or another `map` (the console says so). The
takes themselves are never written. The review (below) reads it.

```
map vrfiringrange
vr_motion_eval                          // every take in quakevr/motions
vr_motion_eval punch_*                  // one category
vr_motion_eval C:/some/folder recorded  // a folder, with the melee settings as recorded
```

From the agent kit: `bash <kit>/run.sh <agent> -Script "map vrfiringrange;wait60;vr_motion_eval <folder> quit" -Timeout 900 -Filter "eval:|PASS|FAIL"`.

### expect.cfg

`quakevr/motions/expect.cfg` (in git; the rest of the folder is not) says what each category should do. See its
comments for the grammar: required events (any of them: `melee`, `melee/stab`, `shove/both`,
`melee@the_pommel|the_hilt`), forbidden ones (`!push`), `none` (no melee event at all: no hit, stroke, push or
batting), poses held for half the take (`pose:parry`: the parry test, a weapon's or crossed arms'; `pose:guard`) or
never (`!pose:parry`, `!pose:guard`: not one frame), and
the weapons a category is for (`weapon:sword|axe|mjolnir`: with another weapon the take is N/A, reported apart). A
hit whose kind isn't required fails the take. A line for a label (`slash_overhead melee/overhead_blow`) wins over its
category's.

## Reviewing failing takes

Takes that fail the evaluation are either the melee's fault or the take's (recorded under the wrong category, a motion
that isn't what its label says). **VR Settings > Advanced VR Options > Review Takes** (under Motion Recorder) lists
them, plays each one in front of the dummy as a ghost, and keeps, discards or relabels it.

- **The list.** At the top: the takes, how many fail, how many are suspect, how many you reviewed; when they were
  evaluated (and how many are newer than that). **Show**: To Review (failing or suspect, not yet reviewed), Failing,
  Suspect, Not Evaluated, Reviewed, All, Discarded; **Category**: All or one. A row is `FAIL* slash_backswing_up_left
  04:03:12 k`: the verdict (`FAIL`, `PASS`, `ERR`, `N/A`; `new`: not evaluated yet; `old`: relabelled since its
  evaluation), `*` suspect, the label, the time it was recorded (with the day when the takes span several), `k` kept
  or `r` relabelled. The row selected shows under the list why it fails, what the replay registered, the suspicion
  and the expectation. Enter (A) opens the take.
- **The take** (its own page): the label, when it was recorded, the verdict and the reason, **expected** (the
  expectation, `expect.cfg`), **replay** (what the evaluation's replay registered, with times from the take's start),
  **live** (what the take registered as it was recorded, and whether the replay hit the same), the weapons, when it
  was evaluated, why it is suspect, whether you reviewed it. Then:
  - **Play Ghost**: the take replayed in front of the training dummy, looping: its weapons (or empty hands)
    translucent and tinted blue where the take had them relative to the dummy (moved with it, not turned, as playback
    places the player), each weapon's line from its handle's end to its far end, its striking points (yellow), the far
    end's trail over the last 0.3 s, the head and where it looked. The events flash as they happen: the take's own
    (`live: ...`, red, where they hit) and the evaluation's replay (`replay: ...`, over the dummy); over the dummy
    too, the label, the time in the take, the phase (lead-in, take, tail) and the verdict. **Ghost Speed** 1x, 0.5x,
    0.25x, 0.1x. Nothing is driven: your tracking, body and weapons stay yours (it plays in the headset as in the mock
    headset). Without a dummy (another map, a take recorded without one) it is shown around you. **Next Take** and
    **Previous Take** go on playing the next one.
  - **Replay (Mock Headset)**: `vr_motion_play <take> watch`: the take drives the tracking and the melee plays it
    again. The mock headset only (it would move your view).
  - **Keep (Reviewed)**: it leaves To Review (`k`); again: unmarked.
  - **Discard**: into `motions/discarded/` (no longer played, evaluated or counted), as the recorder's Delete Last
    Take. **Restore** (Show: Discarded) brings it back.
  - **Relabel**: **Relabel Category** and **Relabel Detail** (they start at the take's own), then **Relabel**: the file
    renamed to the new label (its time kept: `punch_2026-09-27_02-20-55.csv`), its header's `label`, `category` and
    `detail` lines changed and a `relabelled: from no_hit on <date>` line added; the rest byte for byte. The original
    is kept in `motions/review/relabelled/`. The take counts as reviewed (`r`); its verdict shows `old` until it is
    evaluated again.
  - **Undo Last**: takes back the last keep, discard, restore or relabel (again: the one before; also after a
    restart). A relabel undone puts the original back byte for byte and removes the relabelled copy.
  - **Re-evaluate This Take** (a few seconds), and on the list **Re-evaluate Shown** (the takes listed: about 1.7 s
    each; **Stop Re-evaluation**).
- **Suspect** takes are listed in `quakevr/motions/suspects.cfg` (in git, like `expect.cfg`): a file name (or a
  pattern) and why, from the melee's analysis (`ROUND21.md`, "Suspect takes"). Add lines to it as you find more.

**Re-evaluate runs a second copy of the game.** The evaluation replaces the tracking of the head and the hands
with the take's and loads the map afresh for each take: it can't run in the game you play in the headset. So the
review starts this game again in the background: `-vrmock` (the mock headset, whatever `vr_backend` says: it never
opens the runtime's session), `-noconfigwrite` (it doesn't write `ironwail.cfg`), `-noautoexec` (your `autoexec.cfg`
could start anything), `-evalcopy` (the copy never starts a copy of its own), no sound, a small window that doesn't take
the focus, below normal priority, no autosaves. Its script is the first thing on its command line (the engine keeps
only the first 256 characters for `+` commands). It runs `motions/review/eval_job.cfg` (the map, then
`vr_motion_eval list review/eval_job.txt progress review/eval_progress.txt out review/eval_job_table.csv quit`), with
your settings: the review writes the config first (as quitting would), so the copy judges with the melee settings you
have now. The list shows its progress; when it quits the list reads `eval_status.csv` again. It shares the GPU with
the game in the headset: a few takes cost nothing noticeable, a long list may drop frames (take the headset off, or
run it on the desktop). From the console, with the mock headset: `vr_motion_eval` as usual (it writes `eval_status.csv`
too).

**Files** (all in `quakevr/motions/`, none in the takes): `eval_status.csv` (the verdicts), `suspects.cfg`,
`review/reviewed.csv` (kept and relabelled takes), `review/undo.csv` (every change, newest last: what Undo Last takes
back), `review/relabelled/` (the originals), `review/eval_job.*` and `eval_progress.txt` (the last re-evaluation),
`discarded/`. Missing files, takes renamed or moved by hand and takes newer than the evaluation are listed as they are
(`new`; `old` for a take whose time matches an evaluated one under another label); an undo whose file is gone says so
and is dropped.

Console: `vr_motion_review [list | pick <row or file> | show | play | stop | keep | discard | restore | relabel
<category> [detail] | undo | next | prev | reeval [take|shown] | stopeval]`, the page's actions (`list` prints the list
as shown, `show` the picked take's details). Cvars: `vr_motion_review_show` (0..6), `vr_motion_review_category` (-1
all, else `vr_motion_category`'s index), `vr_motion_review_speed` (0.5), `vr_motion_relabel_category`,
`vr_motion_relabel_detail`.

## Synthetic takes (`Misc/quakevr/motion_synth.py`)

Takes written from simple curves, in the same format (the columns playback reads: the tracking, the controls,
the weapons, the target's offset), to build test cases before (and besides) real ones. They play like real takes
(`vr_motion_play`, `vr_motion_eval`; `same_hits_as_recorded` is `-`: they have no live events).

```
python Misc/quakevr/motion_synth.py --list                 # the presets
python Misc/quakevr/motion_synth.py all                    # every preset into quakevr/motions/synth/
python Misc/quakevr/motion_synth.py slash_overhead --two-handed --duration 0.35 --distance 0.8
```

Options: `--out <folder>`, `--duration <s>` (the motion; 0.3, a thrust 0.15), `--distance <m>` (from the head to the
dummy's middle, straight ahead), `--rate <Hz>` (90), `--world-scale` (1.25), `--eye-height` (1.646),
`--two-handed` (a sword with the off hand on its grip, 12 cm below the main hand), `--weapon` (the chop presets'),
`--settings-from <ironwail.cfg>` (the takes written for that config's hand calibration: its `vr_gunangle`,
`vr_handcal_*`... go into a `settings` line, which playback sets), `--mock` (also a `vr_mock_play` script of each, for
tests a take can't carry: the flashlight in a hand), `--name` (the file's name). The melee fixes' presets (`chop_*`,
`punch_down_gib`, `chop_down_gib`, `punch_straight_off`, `palm_shove_2h_torch`, `palm_shove_torch_only`) and how to
play them: TESTING.md, "Melee fixes".

As a module (`import motion_synth`): a `Take(label, main_weapon=..., target=(ahead, left))`, then segments:
`hold(seconds)`, `glide({hand: (pos, quat)}, seconds)`, `move(seconds, fn)` (fn(s) -> {hand: (pos, quat)}, s eased
0..1), each with a `phase` (`pre`, `rec`, `tail`), and `write(path)`. Positions are in the player's frame, metres
(x forward, y left, z up from the floor under the head; the head at `eye_height`). Orientations:
`hand_pose(hand, forward, up)` (the game's hand pointing `forward`, its thumb towards `up`: a fist `(1,0,0), (0,0,1)`;
the main hand's palm ahead, fingers up `(0,0,1), (0,1,0)`), `blade_pose(hand, blade, right)` (a sword's blade along
`blade`, the back of the main hand towards `right`: `sword_swing` puts the edge, the thumb's side, leading). They
turn the game's hand into a controller orientation with the shipped hand offsets (`vr_gunangle 39.5`, ...) and, for
the sword, the blade's direction on the controller measured in the mock headset (`SWORD_BLADE`); another weapon
points along the hand's forward (approximate).

Velocities are the curves' own (central differences); the server frames are the engine's (72 Hz).
