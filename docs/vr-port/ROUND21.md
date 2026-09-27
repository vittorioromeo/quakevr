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

## Melee, redesigned

Voice notes vrfiringrange 01-27 (2026-09-27, three notes and a screenshot): the sword's two-handed "bench press"
bash (the blade level across the face, one hand on the handle and one on the blade, pulled to the chest and pushed
out) never registers; backswings (bottom left to top right) almost never land while forehand swings do; a gun held
by its pump in the off hand after the two-handed hand-off can't strike at all. And the whole thing: one system for
swords, guns and bare hands, where any part of a blade hits in a swing or a stab, the pommel and hilt strike as their
own kind of blow, a weapon held level across in front (one hand or two) is a parry stance that bashes when pushed,
two hands bash harder, punches in any direction hit as hard as a straight one, and open palms pushed shove.

### Audit: the old melee, path by path

Everything is in `QC/vr_juice.qc` ("Blows", "Striking points", bashes and shoves), `QC/client.qc`
(`PlayerVRMelee`, `PlayerVRMeleeImpl`, headbutts), `QC/weapons.qc` (`W_*Melee`, `VRMeleeDmg`) and `QC/combat.qc`
(parries, corpse strikes). Each server frame: headbutt, heartbeat, each hand's stroke and striking points, a gib held
in one hand struck by the other, the bash, then each hand's blow.

| Path | How it was detected |
|---|---|
| Blow (punch, swing) | A *stroke* of the estimated wrist (13 cm behind the controller): it starts when the wrist moves faster than 0.7 m/s and ends when it slows down, turns more than ~65 degrees from its line, or after 0.5 s. A stroke is a blow when the **wrist** has gone `vr_melee_distance` (0.2 m), reached `vr_melee_speed` (2.75 m/s; 1.25x, 3.4 m/s, for the sword, axe and Mjolnir), with a mean acceleration to its peak of 25 m/s2, is still moving at half that, is not going back (more than 117 degrees from the facing; any backward part raises the distance needed), and (fists) the fist didn't turn more than 2.6 rad per metre |
| Contact | Points along the held thing (sword: pommel, hilt, guard, mid-blade, outer blade, tip; axe: hand, handle, head x2; gun: grip, barrel, muzzle; fist: fist, knuckles), swept from pose to pose against boxes (3 units thick) and walls. The stroke's first contact is where the blow lands, once |
| Kind and damage | Base (fist 10, gun 12, axe 20, sword 20 x `vr_sword_damage_mult`, Mjolnir 25) x strength: wrist speed (0.25 at the minimum, 1 at 1.6x/1.8x), reach (1 to 1.2), shape (straight punch 1.25, uppercut 1, overhead 0.7, slap/hook 0.6; swung weapons: arcs 1, thrust 0.8, straight up 0.5), the head's snap (0.85-1.2) and the point (hilt 0.6, handle 0.5) |
| Hilt and pommel | A hilt/pommel/guard contact lands only when the weapon moves along whole (turning under 150 deg/s, the tip no faster than 1.5x the wrist); in a swing it waits `vr_melee_hilt_window` (0.2 s) for the blade and never lands itself |
| Stab | A sword stroke along the blade (within 37 degrees), the tip leading, the tip no faster than 1.4x the wrist, the blade turning under 150 deg/s: 0.8x the speed and a 12 m/s2 snap |
| Bash | The parry's pose (`VR_Parry_Guard`: the weapon's line, or the hands' line held two-handed, within `vr_parry_angle` of level, across, in front), **held still** (wrist under 1 m/s, the weapon turning under 60 deg/s) for `vr_bash_hold` (0.15 s), then within 0.75 s the **controller** pushed forward (within 53 degrees) faster than `vr_bash_speed` (your config: 1.5 m/s), the weapon turning under `vr_bash_swing_rate` (120 deg/s) for 0.05 s, in a stroke that never turned that fast. Hits monsters within 48 units of a point 24 units ahead of the hands |
| Shove | An empty, open hand (grip not held, not steadying, not carrying or climbing), its palm within 53 degrees of ahead, moving along the palm (45 degrees), forward faster than `vr_shove_speed` (1.8 m/s), a snap of 8 m/s2 and 10 cm straight ahead. Two hands: full; one: half |
| Parry-bash | A weapon bash within 1.5 s of a parry: 1.3x |
| Batting | A stroke whose wrist reached 0.6x `vr_melee_speed` keeps the weapon's line for 0.2 s; projectiles within 14 units fly back. A bash (or a shove with a gun in hand) bats within 24 units for 0.3 s |
| Headbutt | The head lunging at `vr_headbutt_speed` towards where it looks |
| Parry | A monster's melee blow on a weapon line held across in front, or crossed forearms |
| Corpse, held gib | The hand faster than `vr_melee_speed` through a corpse; the other hand struck at that speed through a gib held in one hand |
| Exclusions | A climbing hand; the empty hand steadying a two-handed weapon; **a gun carried by its foregrip (no blow, no bash guard, no corpse strike)** |

**Why the three failures happen** (reproduced in the mock with natural synthetic takes, below, and the old code):

- **The bench-press bash.** RESULTS_BENCH
- **Backswings.** RESULTS_SWING
- **The off-hand gun after the hand-off.** `PlayerVRMeleeImpl` returns at once for a hand carrying a gun by its
  foregrip (round 16: "carried, not wielded"), `VR_Parry_Guard` refuses it as a guard and `VR_Corpse_StrikeFrame`
  skips it. And the client sends no muzzle for a carried gun (the view marks it "no aim"), so the server only knew a
  stub 8 units ahead of the hand, not the gun. RESULTS_GUN

The common root: the old model decides *whether* there is a blow from the wrist's motion alone (a stroke's speed,
distance, snap and direction), and only then asks what the weapon touched. A sword's reach and strength are the
blade's, not the wrist's; the rules layered on since round 14 (strokes, back-bias, hilt windows, stab tests, guard
holds, swing-rate limits) each patched one case of that mismatch and made others fail.

### The design: what strikes, how it moves

One model for everything a hand holds. Settings and constants are in one table at the top of `QC/vr_melee.qc`.

**1. Held things have parts.** Every server frame with a new pose, each hand's held thing is laid out as a line
from its *near end* through the *grip* (the hand) to its *far end*, with four kinds of part along it:

| Held | Near end | Grip | Shaft | Head (striking) |
|---|---|---|---|---|
| Sword | pommel | hilt, guard | the blade, guard to tip | the blade and the tip (the whole blade strikes) |
| Axe, Mjolnir | handle end (pommel) | the hand | handle | the head |
| Gun (held, or carried by its pump) | butt | grip | barrel | muzzle end (a gun is blunt all along) |
| Fist | - | the fist | - | knuckles |
| Open hand | - | the palm | - | the palm |

The far end is the weapon's muzzle point (its model's anchor: the sword's tip, the axe's head, a gun's muzzle), as
drawn; a carried gun's is now sent too (the client takes it from the pose the gun is drawn in). Two-handed, the
weapon is where the engine draws it between the hands. Where the helping hand holds a weapon comes from one small
function (`VR_Melee_HelpGrip`), today the helping hand's tracked position, so the grip hotspots of the hand-IK work
drop in there.

**2. A hit is a part sweeping into something, fast.** Each part's points are swept from the last pose to this one
against the boxes of what bleeds and against walls (as before, now 9 points along a sword so the whole blade
strikes). A contact counts when the part that made it moves at `vr_melee_speed` or faster (relative to the head, so
walking and turning don't count). Which way it moves doesn't matter: overhead, sideways, diagonal, rising,
backhand and follow-ups all count the same. What decides is the part's speed:

- the blade (or the axe's head, a gun's barrel) is as fast as its far end: a swing's speed is its tip's;
- the pommel, hilt, butt and knuckles are as fast as they move themselves.

**3. The kind of blow follows from the part and its motion.**

| Contact by | Motion | Kind | Least speed | Weight |
|---|---|---|---|---|
| Blade / head | the far end sweeping across the axis | slash (sword), chop (axe, Mjolnir), swing (gun) | `vr_melee_speed` | 1 |
| Blade tip / head | driven along the axis, tip first, hardly turning | stab (sword), jab (gun, axe) | 0.75x | 1 |
| Pommel, hilt, handle end, butt | moving with the weapon whole or butt first | pommel strike (sword, axe, Mjolnir), butt strike (gun) | 1x | sword 0.6, axe 0.5, gun 1 |
| Pommel, hilt, handle | while the head sweeps past much faster (a swing whose hands arrive first) | nothing: the blade or head decides | - | - |
| Knuckles | any way | punch (jab, hook, uppercut, overhead named for the readout only) | 1x | 1, x `vr_melee_punch_mult` |
| Open palm | leading, forward | a shove (below), not a punch | - | - |

Damage is the base x weight x a speed factor: 0.5 at the least speed, 1 at twice it, up to 2. A punch in any
direction does the same for the same speed. A hand strikes each thing once per motion: again when it has slowed
below half the least speed, turned more than 100 degrees from the blow, or after 0.6 s (so a swing's follow-up or
backswing lands, and a wide swing can pass through two monsters).

**4. The parry stance is the held thing presented across.** Swords, axes and guns (one hand or two, a carried gun
too): the weapon's line (or the hands' line, held two-handed) level within `vr_parry_angle`, across and in front,
the parry's own test. Bare hands: an open palm facing ahead (within 53 degrees). The same stance parries monsters'
blows (unchanged) and bashes.

**5. A bash is the stance pushed.** The stance's middle moves forward (within 41 degrees of the facing) at
`vr_bash_speed` or faster, *as a whole*: its two ends move alike (their difference under 0.6x the push, plus
0.3 m/s; a palm moves along its facing). After 6 cm of such a push it is a bash, and it lands on what is within
0.75 m ahead of the stance, once. No hold is needed: pulling the sword to the chest and pushing it straight out is
a bash. A swing through level across is not: its blade turns (its tip goes several times faster than its hilt) and
its middle goes down or sideways. Bashing is checked before the blows, and the hands in a bash don't also strike.

- Two hands bash harder: a weapon held two-handed, a one-hand weapon with the other hand's open palm pushing on it,
  or both palms: full damage and knockback (`vr_bash_damage`, `vr_bash_push`). One hand: 0.6 of the damage, 0.7
  of the knockback.
- Within 1.5 s of parrying a blow: a parry-bash, 1.3x (as before).
- A bash with a weapon (or a shove with a gun in the other hand) bats projectiles (as round 20).

**How this avoids the old failures.**
- *Slashes read as bashes* (rounds 14, 18, 20): a slash's blade turns and its middle moves down or across; the bash
  needs the stance moving forward whole. No swing-rate limit or hold is needed.
- *Slow waving hits* (round 14): the striking part must reach `vr_melee_speed`; a waved sword's tip, a slow reach,
  a hand going to a holster don't.
- *The hilt landing first* (round 20): a pommel or hilt contact while the blade sweeps faster never lands; the blade
  does, a frame or two later.
- *Backswings and follow-ups*: nothing depends on the direction or on the arm's share of the swing.
- *Slow shoves* (round 20): the palm must face ahead, lead the motion and move forward whole, as before.

### Settings

| Setting | Before | Now |
|---|---|---|
| `vr_melee_speed` | the wrist's least speed in a blow (2.75 m/s; swung weapons 1.25x) | the striking part's least speed, 3 m/s: the tip for a swing, the fist for a punch (stabs 0.75x) |
| `vr_bash_speed` | the push after the held guard (1.2; yours 1.5 m/s) | the stance's forward push, 1 m/s |
| `vr_melee_punch_mult` | a straight punch over a slap | every punch's damage |
| `vr_melee_distance`, `vr_melee_hilt_window`, `vr_melee_stab_speed`, `vr_bash_hold`, `vr_bash_swing_rate`, `vr_shove_speed` | | removed: the model doesn't need them (no stroke distance, no hold, no swing-rate limit; palms use the bash's push) |

Config version 13 resets `vr_melee_speed` and `vr_bash_speed` to the new defaults whatever they were: their meaning
changed (the part's speed instead of the wrist's; the push alone instead of after a held guard).
