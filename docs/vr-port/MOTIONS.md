# Motion takes: the motion recorder

The melee (swords, weapons used as clubs, fists, parries, bashes, shoves) keeps reading some real motions as
the wrong thing. The motion recorder lets you record takes of your own motions in the headset, each labelled with
what it should do ("expected slash", "no hit", ...), in front of the firing range's training dummy. Played back in
the mock headset against the same dummy, the takes test the melee code: each should register what its label
says, and the "no hit" ones nothing.

- `Quake/vr/vr_motion.cpp`: recording, the take file, the menu's helpers (`vr_motion.hpp`, `vr_motion_take.hpp`).
- `Quake/vr/vr_motion_play.cpp`: playback and evaluation.
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

### Categories

| Category (menu) | Label | Meaning | Details |
|---|---|---|---|
| Expected Slash | `slash` | a cut with the blade (sword, axe's head, hammer's head) | overhead, horizontal_ltr, horizontal_rtl, diagonal_down_left, diagonal_down_right, backswing_up_left, backswing_up_right |
| Expected Stab | `stab` | a sword's tip thrust into the target (a sword only) | one_hand, two_hands |
| No Hit | `no_hit` | nothing at all: no blow, bash, shove, headbutt or batting | wiggling, weak, idle, slow_waving, reloading, aiming, walking, reaching |
| Expected Bash | `bash` | the weapon's guard pushed into the target | sword_1h, sword_2h, gun |
| Expected Parry Pose | `parry_pose` | a guard that parries a blow from the target, held (no hit) | sword_1h, sword_2h_blade (a hand on the blade), sword_2h, gun |
| Expected Parry Bash | `parry_bash` | the parry pose, then pushed into the target | sword_1h, sword_2h_blade, sword_2h, gun |
| Expected Hilt/Pommel | `hilt_pommel` | a hit with the hilt, the pommel or the handle's end (sword, axe, hammer), not the blade or head | |
| Expected Punch | `punch` | a fist's blow | straight, jab, hook, uppercut, overhead |
| Expected Palm Shove 1H | `palm_shove_1h` | one open palm shoving | |
| Expected Palm Shove 2H | `palm_shove_2h` | both palms shoving | |
| Expected Gun Strike | `gun_strike` | a gun used as a club | swing, butt |
| Other | `other` | anything else (`vr_motion_note`) | |

A take's label is `<category>` or `<category>_<detail>` (`slash_overhead`, `parry_pose_sword_2h_blade`).

### Console

| Command / cvar | |
|---|---|
| `vr_motion_armed 0/1` | arms the recorder (the menu's Arm Recorder; not saved: off at each start) |
| `vr_motion_category N`, `vr_motion_detail N` | the category and detail (indices, as in the menu) |
| `vr_motion_button 0/1` | the record button: the off hand's stick click (0) or the main hand's (1) |
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
| `vr_world_scale`, `units per metre` | Quake units per real metre (`32.8084 * vr_world_scale`) |
| `vr_height_calibration`, `vr_floor_offset` | the player's calibrated height (metres) and the floor offset (units) |
| `hand angles` | `vr_gunangle`, `vr_gunyaw`, `vr_offhandpitch`, `vr_offhandyaw`, `vr_controller_legacy_pose` |
| `grips` | `vr_weapon_grip_mode`, `vr_2h_mode` |
| `server rate` | `host_maxfps`, and the server's frame time (72 Hz, or every host frame) |
| `yaw0` | the player frame's heading: the head's world yaw at `t` = 0 (degrees) |
| `origin0` | the player's world origin at `t` = 0 |
| `target` | the nearest monster at `t` = 0: classname, entity number, targetname, origin, angles, box |
| `melee settings` | every `vr_melee_*`, `vr_bash*`, `vr_shove*`, `vr_parry*`, `vr_deflect*`, `vr_headbutt*` cvar |
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
| `m_buttons` | 1 trigger, 2 grip, 4 A/X, 8 B/Y, 16 stick click, 32 menu (the recorder's own click taken out) |
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

The runtime's tracking (tracking space, see above):

| Column | |
|---|---|
| `raw_head_px/py/pz`, `raw_head_qw/qx/qy/qz` | the head's position and orientation (a quaternion) |
| `raw_head_vx/vy/vz`, `raw_head_wx/wy/wz` | its velocity and angular velocity |
| `raw_head_valid`, `raw_head_vvalid` | the pose and its velocities are valid |
| `raw_m_*`, `raw_o_*` | the same for each hand's controller, and `gx/gy/gz`, `gvalid`: the palm's velocity (with `vr_controller_legacy_pose`) |

For the QC side (the melee agent): the events come from `VR_Motion_Event` calls in `T_DamageImpl` (the hits: kinds
from `VR_SetHitKind`), `PlayerVRMeleeImpl` (strokes), `VR_Bash` (pushes), `VR_Parry` and `VR_Deflect_Send`, and the
points and values from `VR_Motion_Sample` (`QC/vr_motion.qc`). Keep them when the melee changes (the evaluation's
expectations are written in these kinds), or change both.
