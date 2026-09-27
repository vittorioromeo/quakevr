# Round 21

## Motion recorder

Your proposal: record your own motions, each labelled with what it should do, so that the melee can be tuned
against them, replayed in the mock headset. `docs/vr-port/MOTIONS.md` has the steps, the categories, every column,
playback, the evaluation and the synthetic takes.

- **Where**: VR Settings > Advanced VR Options > **Motion Recorder** (under Melee). Pick the **Category** (named by
  the result expected: Expected Slash, Expected Stab, No Hit, Expected Bash, Expected Parry Pose, Not Parry Pose,
  Expected Parry Bash, Expected Hilt/Pommel, Expected Punch, Expected Palm Shove 1H / 2H, Expected Gun Strike, Other)
  and optionally a **Detail** (the swing's direction, the weapon, ...), turn **Arm Recorder** on, go back to the game.
- **Recording**: in the firing range, in front of the training dummy, **click the off hand's stick** to start a
  take (a beep, a buzz, "REC slash #4" in view), do the motion, **click again** to end it ("SAVED slash #4 (slash:
  4)"). Many in a row: the category stays chosen, and each take is a new file,
  `quakevr/motions/<label>_<date>_<time>.csv`, counted per category in the menu (the files, so across sessions).
  **Delete Last Take** moves a bad one into `motions/discarded/`. A double click (under 0.2 s) is not kept (a
  buzzer). A take that can't be written says so loudly and is kept in memory (`vr_motion_save_unsaved`).
- **What a take holds**, every frame at the headset's rate: the head and both hands as the runtime reported them
  and as the game placed them, in the world, relative to you and relative to the dummy; the buttons, the analog
  trigger and grip, the fingers; the weapons, the two-handed grip, the striking points and the handle's end as the
  melee computes them; your position (the client's and the server's), velocity, yaw and view; the dummy's position,
  angles, box and class; the parry and bash-guard state; every melee event the game registered (hits with their
  damage, kind and striking point, strokes, pushes, parries, batting); every setting that places the hands and the
  weapons, and the melee's. 0.5 s before the click (the lead-in) and 0.3 s after (late hits) too.
- **The button**: the off hand's stick click, the one control free during melee on every controller (Touch
  through SteamVR or VDXR, Index, Vive, WMR); while armed its binding (run) rests. `vr_motion_button 1`: the main
  hand's.

**Playback** (`vr_motion_play <take>`, mock headset): the take's tracking and controls frame by frame at its
recorded frame times, the server frames where they ran, the settings as recorded, the player put where the take has
them relative to the dummy, weapons and grips (two-handed too) taken again, then a report against the take's own
events. **Evaluation** (`vr_motion_eval [folder|pattern]`): every take in the map loaded afresh, judged against
`quakevr/motions/expect.cfg`, a table in `motions/eval_<date>.csv`; deterministic (two runs write the same table)
and about 1.6 s a take. `Misc/quakevr/motion_synth.py` writes synthetic takes from curves.

**Checked on your 398 takes** (Quest 3, VDXR, 120 Hz, world scale 1.25), replayed with their own settings
(`vr_motion_eval ... recorded`): **383 register exactly the hits they registered live** (the same kind, hand,
target, striking point, damage and time): all 82 recorded with the full settings header, and 301 of the 316 before
it (their weapon offsets taken from your config). The 15 others are mostly guns swung as clubs (a hit with "the
grip" appears or vanishes), whose placement in the hand the round's fitted hands changed after they were recorded.
The replayed hands stay within 0.1 units (3 mm) of the take's, relative to the dummy, median; 0.6 units (2 cm) at
the 95th percentile.

What today's melee makes of them (your settings, against `quakevr/motions/expect.cfg`), for the melee's rework:
palm shoves 17/17, punches 24/28 (most "punches" register as slaps or overhead blows), gun strikes 30/35, slashes
117/133, parry poses 48/55, no hit 24/39, and no stab (1/28: thrusts or swings), hilt/pommel strike (0/27) or parry
bash (0/36) registers as such. Resampled at 72 Hz (`rate 72`), 2 of 8 punches change kind: the melee is frame-rate
sensitive.

Keep recording as you were; the takes are sufficient for exact replays.

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

## Fitted hands

BACKLOG's "Fitted hands", steps 1 to 4, with the weapon offset simplification and the two-handed hotspots you asked
for during the round.

| Step | Result |
|---|---|
| 1. Jointed hand | One skinned model per hand: three joints a finger, an opposable thumb. At the old curl frames it matches the six models exactly, and the curl moves smoothly between them. Skins, blood, powerups, lighting, shadows and the flashlight behave as before. `vr_hand_rig 0` brings back the six models. |
| 3. Weapons | The hand sits where the controller is. The weapon has one transform in the hand. The fingers wrap its grip on their own. Five finger bias sliders and the thumb placement remain as overrides. 30 keys are retired. Muzzles did not move (0.0000 units); foregrips moved at most 0.0001 units. |
| Hotspots | Up to 4 two-handed grips per weapon, each typed Grip or Blade, with a bias. The helping hand takes the nearest one, less its bias, and the fingers wrap it. There is a menu editor, a Show Hotspots view and a QC query. Today's foregrips and sword blade grips were migrated into them exactly. |
| 2. Physics objects | The same solver closes the fingers on boxes, backpacks, gibs, heads and armour, using their real triangles. No finger passes through, and none floats. Limitation: objects are large next to the hand, and the server fit presses them against the fist. Many grips therefore stay close to a fist resting on the object. |
| 4. Palm placement | The palm slides flush with what it holds (at most 5 cm for things, 3 cm for weapons) and turns up to 20° toward its surface. The change eases in and out over about 0.1 s. |

Costs: 0.01 ms more on the main thread per frame than the six models, and no GPU change. Each solve takes 3–28 ms,
but it runs on a worker thread, and the main thread's share is 0.003–0.04 ms. See [Costs](#costs).

### How it works

**The jointed hand** (`Misc/quakevr/make_hand_rig.py` → `quakevr/progs/hand_rig.md5mesh/.md5anim/.mdl`,
`hand_rig_NN_00.lmp`, `Quake/vr/vr_handrig_data.inc`; drawn by `Quake/vr/vr_handrig.cpp`):

- The script reads the six hand models (the palm, the thumb and four fingers, six curl frames each).
- It splits each finger into three segments along its rings of vertices. It fits the joint pivots by least squares
  and each frame's joint turns as quaternions.
- It stores every vertex in its segment's frame, per frame, so each old frame is reproduced exactly (checked to
  1e-14 in Python).
- The thumb gets a metacarpal joint that carries the thenar (the palm's vertices within 4.5 units of the thumb's
  base, weighted).
- It is an MD5 enhanced-model replacement, drawn through the body's skinned path (`VR_AliasBonePoses`, now asking
  `view::handBonePoses` first): one draw per hand instead of six.
- **Normals:** the MDL renderer's normals come distorted by the model's header scale. Each vertex has its own joint,
  whose rotation takes the bind normal to that distorted normal, so the shading matches the old models.
- **Blood:** the bloody skins overlap in UV between parts. The script lays them out on four bands of a 512×512 skin,
  so every part's blood pattern stays as it was.
- **Why skinned rather than 15 rigid segments:** one draw call, the exact old shapes at the old frames, and joints
  that bend without cracks.
- **The cost:** the hands no longer get the per-vertex baked ambient occlusion of alias models (`vr_ao.cpp` skips
  skinned hand rigs, as it does for the body). It is not visible at hand size in the tests below.

**Curls:** each finger's joints follow today's curls (trigger, grip, thumb, `vr_finger_grip_bias`,
`vr_finger_auto_close_thumb`, eased at `vr_finger_blending_speed`). A continuous path replaces the old frame
switches: frames 0..4 close, and frame 5 is frame 3's shape. The Weapon Offsets page's per-finger bias is added on
top.

**The grasp solver** (`Quake/vr/vr_grasp.cpp`). It is GraspIt!'s auto-grasp idea (below) on the hand's own
triangles and the held model's current-frame triangles (`grasp::worldTriangles`):

- Each finger closes its three joints together. When a segment touches, the joints before it stop and the rest go
  on. Contact is found by halving the step, to 1/1000 of a step. Contact means the hand's and the model's triangles
  cross (segment/triangle tests over a uniform grid).
- A settle step then lets a distal joint close further while the one before it opens, so a finger wraps rather
  than stopping stiff on its tip.
- A finger that already starts inside the model (a gun grip through the palm) closes from the fist outward instead
  (`fromClosed`).
- The thumb tries 16 turns of its metacarpal: opposition 0/15/30/45° × swing 0/±15/−30°, at a small cost per
  degree. It keeps the best wrap; failing that, the least inside.
- **Palm placement (step 4):** the solver tries palm moves on a grid, flush with the surface along the palm's
  normal. It scores closure plus wrap and keeps the best. It also turns the palm up to `vr_hand_fit_palm_turn`
  toward the surface under it (a ray along the palm's normal, or the nearest point on its side). The move is capped
  by `vr_hand_fit_palm` (things) and `vr_hand_fit_palm_weapon` (weapons).
- **Where it runs** (`vr_view.cpp` `updateGrasp`): once per grip change, on a worker thread (`std::async`).
  Results are cached, 32 of them, per model, frame and pose in the hand. A result stands while the thing stays
  within 0.5° and 0.15 units of where it was solved. A result for a hand that moved a little meanwhile (10°,
  2 units) is used until the next one lands, so nothing flickers while a weapon lags. Joints ease to the solved
  stops at the finger blending speed. The solved joint stops cap the controller's curls; they don't replace them,
  so an open hand still opens.

**Weapons: the simplified model:**

- The hand is always where tracking and the rig put it (`s.pos`, `s.rot`), plus the palm fit.
- The weapon is placed in the hand by one transform: Offset X/Y/Z, Pitch/Yaw/Roll, Scale. This is the same
  transform as before, so the gun, the muzzle and the aim are unchanged.
- The fingers wrap its grip automatically.
- Per weapon, only small overrides remain: a curl bias per finger (`fgr_bias_thumb` … `fgr_bias_pinky`, −1..1 of
  a full curl) and the thumb's place (`fgr_thumb_x/y/z`).
- The "Weapon Only" sliders, the hand anchor vertex and offset, and the fixed finger openness are gone (audit
  below).

**Hotspots:** up to four per weapon, in the weapon's model space.

- The world point of a hotspot `h` is `R_EntityMatrix(weapon) · [mirror] · S(k) · T(Offset + (0,0,vr_gunmodely)) · h`,
  the weapon's frame as drawn, before its Scale.
- **Grip:** a point where the other hand can hold the weapon: a foregrip, a pump, a magazine, the sword's handle.
- **Blade:** the half-sword grip along the blade. Its X is the share of the way from the hand to the tip where the
  grip is centred (0.75 on the swords). The zone runs from `max(0.3, X − 0.3)` to 1.05 of the hand-to-tip line, as
  round 18's blade grip did.
- **Bias** (units) is subtracted from the distance a hotspot is picked by: larger means easier to take.
- The helping hand takes the grip nearest to it, less the bias, and keeps it while it holds (`chosenGrip`).
- Two-handed aim and the take/keep distances use the chosen grip (`vr_twohand.cpp`: distance − bias). The blade
  uses its own share and bias.
- The solver wraps the helping hand's fingers there, like any other grasp.

### The data format (`vr_weapons.inc`, cvars `vr_wofs_<key>_NN`)

| Key | Values |
|---|---|
| `hsN_type` (N = 1..4) | 0 none, 1 grip, 2 blade |
| `hsN_x`, `hsN_y`, `hsN_z` | grip: the point in the weapon's model units (as above); blade: `hsN_x` is the share of the hand-to-tip line (y, z unused) |
| `hsN_bias` | units off its distance (0..10) |

The engine API (`vr_weapons.hpp`) is `weapons::hotspot(slot, i)`, `setHotspot`, `hotspotKey`, `maxHotspots` (4)
and `HotspotType`. The view API (`vr_view.hpp`) is `view::weaponHotspot(hand, i)`, which gives the world position
as drawn this frame, the type, the bias and the share, and `view::hotspotAt(hand, p)`, which maps a world point to
hotspot coordinates.

QC (`QC/builtins.qc`, for the melee code and mods):

    vector(entity player, float hand, float index) weaponhotspot;              // world position ('0 0 0' if none)
    float(entity player, float hand, float index, float what) weaponhotspotinfo; // what 0: type, 1: bias, 2: blade share

`hand` is `cVR_MainHand` or `cVR_OffHand`, and `index` runs 0..3. These are the local player's hotspots, as the view
draws them; other players get none (the view computes them client-side). `impulse 253` prints them. The query
doesn't change the blade/hilt/muzzle queries the melee code uses.

### The audit: every per-weapon key (`vr_wofs_<key>_NN`)

Kept means read as before. Replaced means a new mechanism does its job. Folded means its value is now expressed
elsewhere. Retired keys stay registered and unused (`weapons::retired`), so saved configs load without "unknown
command". **Print Changes to Console** skips them.

| Key(s) | Status | Notes |
|---|---|---|
| `id` | kept | The model to slot map. |
| `x`, `y`, `z` (Offset) | kept | The weapon in the hand. The hand no longer rides on it. |
| `pitch`, `yaw`, `roll` | kept | The weapon's turn in the hand. |
| `scale` | kept | The weapon's size. The fist slot's value still scales the hand. |
| `hand_av`, `hand_x/y/z` (hand anchor vertex and offset) | **retired** | The hand is at the controller. The old anchors were within 0.8 cm of it for every slot (`vr_hotspots_check`: 0.02–0.21 units, most under 0.1). |
| `gunoff_x/y/z` | **retired** | 0 in every slot. The muzzle check below confirms nothing moved. |
| `length` | **retired** | Never read. |
| `2h_fxd_mh_ox/oy/oz` | **retired** | Never read. |
| `w_hvelmult`, `w_htvelmult` | **retired** | Never read. |
| `fgr_x/y/z` (fingers offset per weapon) | **retired** | Replaced by the solver. Its job was to move the fingers off the grip. |
| `fgr_open`, `fgr_thumb`, `fgr_index`, `fgr_middle`, `fgr_ring`, `fgr_pinky` (openness) | **replaced** | The solver wraps the grip. `fgr_bias_*` is the per-finger override (0 by default). Your round-20 openness values described a fixed pose and are not carried over. |
| `2h_fgr_open`, `2h_fgr_thumb` | **replaced** | The helping hand's fingers are solved on the hotspot. |
| `2h_dispmd`, `2h_hand_av`, `2h_fxd_ox/oy/oz` (the fixed foregrip) | **folded** into hotspot 1 (Grip) | The same point (see Migration). |
| `2h_blade` | **folded** into hotspot 2 (Blade) on the swords | `hs2_x` = 0.75. |
| `fgr_thumb_x/y/z` | kept | Thumb placement ("Thumb X/Y/Z" under Fingers on the Weapon). |
| `hide_hand` | kept | |
| `zb`, `zb_2h` | kept | `zb_2h` applies while a grip hotspot is held. |
| `muzzle_av`, `muzzle_x/y/z` | kept | Unchanged. |
| `ch_mode_z` | kept | |
| `2h_mode` | kept | 0 normal, 1 no virtual stock, 2 no two-handed, 3 sword. |
| `2h_x/y/z`, `2h_pitch/yaw/roll` | kept | Renamed in the menu to **Aim Offset X/Y/Z** and **Aim Pitch/Yaw/Roll**: they move and turn the two-handed aim, not anything drawn. Round 20's "Other Hand X/Y/Z" and "Other Hand Pitch/Yaw/Roll" labels were wrong. |
| `2h_fxd_hp/hy/hr` | kept | The helping hand's angles on the foregrip. Pitch is used only on the swords. |
| `weight`, `w_posmult`, `w_dirmult`, `w_2hposmult`, `w_2hdirmult` | kept | |
| `wpntxt*`, `wpnbtn*` | kept | The ammo screen and button. |
| new: `fgr_bias_thumb/index/middle/ring/pinky` | added | −1..1 of a full curl, added to the wrap. |
| new: `hs1..4_type/x/y/z/bias` | added | Hotspots. |

Global cvars:

- `vr_weapon_only_x/y/z` are removed, together with the page's "Weapon Only" section. They were deltas for the
  menu, never saved.
- `vr_finger_grip_open` is retired and still registered.
- The global `vr_fingers_*` / `vr_finger_*_x/y/z` offsets, `vr_gunangle`, `vr_gunmodel*` and the rest are unchanged.
- `vr_gun_z_offset`'s help still says it moves the guns; it only sets the torso height for the wall sweep. I didn't
  change it.

### Migration (`vr_wofs_version` 15)

- **Shipped defaults:** the fixed foregrip of every slot that had one (1–7, 9–15, 18, 19) became hotspot 1 (Grip).
  `vr_hotspots_legacy` worked it out from the round-20 keys, with the weapon at the origin unturned, mirrored as
  the helping hand saw it, and the result was pasted into `vr_weapons.inc`. The swords' blade grip became
  hotspot 2 (Blade, 0.75).
- **Your saved config:** on load, any slot whose foregrip or blade keys, Offset or Scale differ from the shipped
  defaults is marked. The first time that weapon is drawn, its hotspots are rebuilt from your values the same way
  (`weapons::takeHotspotMigration`). This needs the model, so it happens in the view. Tested: a config with a
  custom `vr_wofs_2h_fxd_ox_02` got a hotspot at exactly the old foregrip for those values.
- **Your placements:** Offset, Pitch/Yaw/Roll, Scale and the muzzle are the same keys with the same values (your
  `30580890` placements are merged).
- **Verified** with `vr_hotspots_check`: every slot, 6 random weapon poses, both mirrors
  (`final/hotspots_check.txt`).
  - Foregrip, old against hotspot: at most 0.0001 units apart.
  - Muzzle, round 20's placement (with GunOffset) against now: 0.0000 units.
  - Old hand anchor against the controller: at most 0.21 units (0.79 cm, the grappling hook), most under 0.1.
- **End to end:** the base build (`498e187d`) and this one were run on the same mock pose for each weapon from the
  axe to the lightning gun, plus the sword. The main and off hand muzzles are identical to 3 decimals
  (`final/muzzles_old.txt`, `muzzles_new.txt`).

### The menu

- **Hand/Gun Calibration > Fingers:**
  - Jointed Hand (`vr_hand_rig`).
  - Fit Fingers to What You Hold (`vr_hand_fit`).
  - Palm Fit: Things (`vr_hand_fit_palm`, cm).
  - Palm Fit: Weapons (`vr_hand_fit_palm_weapon`, cm).
  - Palm Turn (`vr_hand_fit_palm_turn`, degrees).
- **Weapon Offsets**, in this order:
  - **Weapon in the Hand:** Offset X/Y/Z, Pitch/Yaw/Roll, Scale, Hide Hand.
  - **Fingers on the Weapon:** the five biases, Thumb X/Y/Z.
  - **Muzzle.**
  - **Other Hand's Grips (Hotspots):**
    - Hotspot 1–4 picks the one to edit.
    - Type: None, Grip or Blade.
    - X/Y/Z, or Along the Blade for a blade.
    - Put It Where the Other Hand Is.
    - Bias.
    - Remove This Hotspot.
    - Show Hotspots.
  - **Two-Handed Aim.**
  - **Ammo Screen.**
  - **This Weapon:** Weight, Print Changes to Console, Reset This Weapon.

  Round 20's Hand X/Y/Z, Weapon Only and openness sections are gone. The page rebuilds itself when another hotspot
  is picked or its type changes.
- **Show Hotspots** (`vr_show_weapon_hotspots`):
  - Grips show as green points and blades as orange lines. The hotspot being edited is white.
  - Each has a faint ball as large as its bias.
- The advanced list's older **Hotspots** page is the body's holster hotspots, not these.

### Settings

| Cvar | Default | |
|---|---|---|
| `vr_hand_rig` | 1 | Jointed hand. 0: the six models (the old path, kept). |
| `vr_hand_fit` | 1 | Fingers wrap what the hand holds. 0: the controller's curls only. |
| `vr_hand_fit_palm` | 5 | cm the palm may move to sit flush on a held thing. |
| `vr_hand_fit_palm_weapon` | 3 | The same on a weapon. At 5 cm (the first try), the hand looked detached from the guns in the before/after shots (3–4.6 cm). Now it stays within 2.8 cm. |
| `vr_hand_fit_palm_turn` | 20 | Degrees the palm may turn toward the surface. |
| `vr_debug_grasp` | 0 | 1 prints each solve (triangles, palm move, thumb turn, ms on the worker, ms from the request, ms on the main thread). 2 adds each finger's stops. |
| `vr_show_weapon_hotspots` | 0 | See above. |
| `vr_weapon_hotspot` | 1 | The hotspot the Weapon Offsets page edits. |

The archived cvars are saved. `vr_weapon_hotspot` and `vr_show_weapon_hotspots` are not.

Other changes:

- `MAX_CVARS` goes from 4096 to 8192 (`Quake/cvar.c`). The new per-weapon keys pushed the registered count over
  the limit: 32 slots × 106 keys, plus the rest.

Test commands:

- `vr_mock_fingers <hand> <trigger> <grip> [<thumb>]`.
- `vr_grasp_dump <hand> <file.obj>`: the drawn hand and the held model's triangles.
- `vr_hotspots_legacy`.
- `vr_hotspots_check`.
- `impulse 252`: a gib or head in the empty off hand, 9 kinds in turn.
- `impulse 253`: prints the held weapons' hotspots through the QC query.

### Files

- New:
  - `Misc/quakevr/make_hand_rig.py`
  - `Quake/vr/vr_handrig.hpp/.cpp`, `vr_handrig_data.inc`
  - `Quake/vr/vr_grasp.hpp/.cpp`
  - `quakevr/progs/hand_rig.*`, `hand_rig_0N_00.lmp`
- Changed:
  - `Quake/vr/vr_view.cpp/.hpp`: rig hands, grasp jobs, palm fit, hotspots, checks.
  - `vr_weapons.cpp/.hpp/.inc`: keys, retired list, hotspot API, migration.
  - `vr_twohand.cpp`: hotspot bias, blade from the hotspot.
  - `vr_hands.hpp`
  - `vr_menu.cpp`, `vr_menu_pages.inc`
  - `vr_cvars.inc`
  - `vr_avatar.cpp`, `vr_ao.cpp`
  - `vr_held.hpp/.cpp` (`heldEntity`)
  - `vr_builtins.cpp`
  - `vr_backend_mock.cpp`
  - `vr_main.cpp`
  - `Quake/cvar.c`
  - `QC/builtins.qc`, `QC/weapons.qc`

### Tests (mock headset; composites in the scratchpad's `hands/final/`)

- **`step1_main_hand_old_vs_jointed.png`, `step1_off_hand_old_vs_jointed.png`:** the empty hand in five finger
  poses (open, fist, trigger, thumb, grip), two views each, old models against the jointed hand, with the
  difference ×4. Silhouettes are identical. The small shading differences come from the old parts being lit as six
  separate entities.
- **`step1_blood.png`:** the bloody skins, the same patterns.
- **`step1_lighting_flashlight_powerups.png`** (e1m1): lit, the flashlight, the quad's glow, and the ring's
  transparency, old against jointed. They are the same.
- **`curl_sweep_old.png` / `curl_sweep_jointed.png` / `curl_sweep_open_jointed.png`:** grip curl 0.00 to 1.00 in
  steps of 0.05.
  - The old models step from frame to frame: jumps at 0.10, 0.30 and 0.50, and at 0.90 the hand opens back to
    frame 5 (frame 3's shape).
  - The jointed hand closes steadily with no jump. Round 20's openness jump is gone with the fixed openness.
- **`guns_before_after.png`:** the shotgun, super shotgun, nailgun, super nailgun, grenade launcher,
  rocket launcher and lightning gun, two views each.
  - Old six models: fingers through the grip, or a fist beside it.
  - Fitted: fingers around the grip. The hand is at the controller.
- **`guns_grasp_views.png`:** the same grips from `vr_grasp_dump` (front, below, oblique), not fitted against
  fitted. Not fitted, the fingers pass through the grips; fitted, they close round them.
- **`sword_blade_grip.png`, `twohanded_grasp_views.png`:**
  - Your screenshot's case: the sword held level across the face by the blade (`vrfiringrange_2026-09-27_01-27-29`).
  - The shotgun's foregrip.
  - Old, jointed not fitted, fitted. Fitted, the helping hand closes round the blade and the barrel. Not fitted,
    the fingers pass through them.
  - `foregrip_hotspot.png` adds Show Hotspots.
- **`objects_item_shells.png`, `objects_item_health.png`, `objects_backpack.png`, `objects_gib.png`,
  `objects_head.png`, `objects_armour.png`:** the off hand gripping each at three angles, before and after.
  - Fingers stop on the surface: none pass through, none float.
  - Where the server's fit has pressed the object against the knuckles, the result stays close to a fist on it.
  - The palm turn helps the armour, the backpack and one gib (20°).
- **`menu_weapon_offsets.png`, `menu_hand_gun_calibration.png`:** the pages.
- **Numbers:** `hotspots_check.txt`, `muzzles_old.txt` / `muzzles_new.txt`, `solves.txt`, `prof_final.txt`.

### Costs

All costs were measured with `run.sh --exclusive` on vrfiringrange. The shotgun was in the main hand and a gib in
the off hand, over 600 frames each.

| | view entities (CPU) | alias (CPU) | GPU |
|---|---|---|---|
| Six models (`vr_hand_rig 0`) | 0.02–0.03 ms | 0.06 ms | 0.52 ms |
| Jointed, not fitted | 0.03–0.04 ms | 0.05–0.06 ms | 0.52 ms |
| Fitted | 0.03–0.04 ms | 0.05–0.06 ms | 0.52 ms |

Solves on the worker thread, exclusive (`vr_debug_grasp 1`):

| Held | Solve time (worker) | Main thread's share |
|---|---|---|
| Guns | 11–17 ms | 0.01–0.03 ms |
| Axe | 21 ms | |
| Sword | 10 ms | |
| Gibs and heads | 3–28 ms | |
| Boxes | 15–30 ms | |

The hand eases to the result 12–32 ms after the grip. Until it lands, the fingers follow the controller or the last
grasp. Solves happen only when the grip, the held thing or its place in the hand changes (cache hits cost
nothing).

### Limitations and risks

- **Physics objects:** the server's `held::surfaceFit` places the object against the drawn fist before the fingers
  close. Most objects are several times the hand's size, so many grips end as a fist against the object. The
  object is not placed into the open hand. Moving the object to the palm, or letting the fingers open more before
  the server fits, would be next. It touches server-side carrying, which I left alone.
- **No finger-touch sensors:** a lifted index doesn't straighten off the trigger unless the trigger value drops.
  OpenXR's touch paths are not read yet.
- **Only the local player:** hotspots from QC are the local player's, and solves happen only for the local
  player's own view.
- **The palm on weapons** moves up to 3 cm, so the hand may look slightly off the controller on some guns. At 0 it
  stays exactly on the controller.
- **Thumbs on pistol grips** sometimes sit along the side rather than over the top. Thumb X/Y/Z per weapon can
  fix that.
- Retired keys stay registered, 30 × 32 cvars, which is why `MAX_CVARS` went up.
- The hands have lost their per-vertex baked AO.
- Picking up a two-handed grip triggers a new solve for the helping hand. For about 20 ms, its fingers show the
  controller's curls.

### In the headset

- [ ] The empty hands look and move as before, with smoother curls. Try grip half pressed and slow squeezes.
- [ ] Every gun: the hand is where your hand is, and the fingers are on the grip, not through it. If one is off,
      use Weapon Offsets > Weapon in the Hand (the gun moves in the hand), then Fingers on the Weapon (bias,
      thumb).
- [ ] Palm Fit: Weapons: at 3 cm, does any gun feel detached? Try 0 and 5.
- [ ] Two hands: the foregrip on the shotguns, launchers and nailguns. Hold the sword by the blade level across
      your face, as in your screenshot.
- [ ] Hotspots: add a second grip (for example the pump or the magazine) with Put It Where the Other Hand Is.
      Turn on Show Hotspots and check that the nearer one is taken. Bias makes one win.
- [ ] Your saved config: the foregrips are where you tuned them (your 2H values were converted once, on load).
- [ ] Boxes, backpacks, gibs, heads and armour: do the fingers stop on them? Does the fist-against-the-object look
      bother you? Palm Fit: Things and Palm Turn change it.
- [ ] Quad, pentagram, ring of shadows and the flashlight on the hands.
- [ ] Jointed Hand off: the old hands, for comparison.

## Wrist gadget, hologram, casings, flashlight

Five of your voice notes: casings splashing into water, a test button for the hologram, messages only on the
gadget (a chime from the wrist and a buzz instead), an FPS counter on the gadget, and a straight flashlight held like
a real one, with a second grip (B/Y) and clipped along the barrel.

| Note | Result |
|---|---|
| Casings in water | a tiny splash, the smallest ripple and a quiet, higher plip where a casing goes in; it then sinks slowly |
| Hologram test | Screens > Messages > **Show a Test Message**: one of the game's own messages, as the game sends them; press again and they stack |
| Messages only on the gadget | Screens > Messages > **Messages Only on the Gadget** (off by default): nothing in front of you; the message waits in the hologram until you look; a chime from the gadget and a double buzz |
| FPS counter | Graphics > Performance > **FPS Counter on the Gadget**: ` 90 FPS CPU  5.2 GPU  8.1` floating just under the gadget |
| Flashlight | a straight 13 cm torch through the fist; B/Y (away from a gun) flips between the low and the overhead grip; on a gun it lies parallel to the barrel |

### Casings into water (`vr_shells.cpp`, `vr_particles.cpp`)

- A casing crossing into water, slime or lava (flying, or rolling in on a sloping floor) finds the surface there and
  makes `particles::shellSplash`: the splash preset's parts at a casing's size. That is 5-9 small drops thrown up 3-12
  cm (half leave a little ring where they fall back), one ring riding the ripple's crest, and a wisp of foam. Lava
  adds an ember or two. It follows vr_water_splash, _size and _ring_size.
- The ripple is `water::addRipple` with a strength under 1. Those now get a lower floor: 1.1-2.3 units at your
  amplitude of 16, against a shot's 7.7. Other splashes are unchanged (they are 1 and up).
- The sound is `shell_plip1..3.wav`: the recorded plips (plip1, 3, 4) pitched up 1.4-1.65 times and cut to 0.2 s, made
  by `make_sounds.py` (credited in CREDITS.md). It plays at the surface at 0.25-0.55 × vr_shells_sound ×
  vr_water_sounds, louder the faster the casing goes in. The mixer has no pitch, so the pitch is in the files.
- The water takes 70% of the casing's speed and half its spin. It then sinks at the old slow rate.
- Cheap: at most 4 splashes and 2 sounds each tenth of a second, however many casings land. `developer 2` prints each
  one.

### Hologram: the test button, messages only on the gadget (`vr_gadget.cpp`)

**Which messages go where.** Before this round:

| Message | Where it shows |
|---|---|
| Centre prints: a key needed, a secret, the maps' trigger texts, runes | the hologram while the gadget faces you; otherwise in front of you as always |
| Server prints: pickups, a powerup running out, deaths, chat | the hologram (and at the top of the view with Console Messages set to In view or Both) |
| The engine's lines: cvars, cheats, errors, the level's name when it loads | the log over the gadget (diagnostics) |
| The intermission's and finale's texts | in front of you (the gadget is put away) |

**Notifications** are centre prints, plus server prints that aren't pickups ("You got/get/receive ..."). A pickup is
not news: the thing is already in your hand. The level's name is an engine line, printed as a map loads, and stays in
the log. It is also on the gadget's screen.

- **Show a Test Message** (`vr_message_test` with no arguments). Each press sends the next of six of the game's own
  messages, the way the game sends them, with its sound: "You need the gold key", e1m1's "You must press the three
  buttons...", "You found a secret area!", "Quad Damage is wearing off" (a print), e4m4's two-line exit text, "A
  secret cave has opened...". It works with the menu open. Raise the gadget beside the panel to see them.
- **Stacking.** The hologram now keeps its own queue of messages. They are taken from the console as they are printed,
  so a line pieced together from several prints comes through whole. Centre prints stack as well: before, a new one
  replaced the last. The same text again, like a locked door touched twice, keeps the shown one on.
- **Messages Only on the Gadget** (`vr_messages_hologram_only` 0/1):
  - The game's messages never show in front of you. That covers centre prints, and server prints when Console
    Messages is set to In view or Both. The engine's lines still go where Console Messages says.
  - A notification waits in the hologram until you look. Its life (Hologram Time) starts when you first see it,
    meaning the hologram faces you and has at least half faded in, and it grows in then. It is dropped after five
    minutes unseen, or on a new map. Waiting messages are kept ahead of pickups when space runs short.
  - A new notification while the gadget is out of view plays Quake's message sound (misc/talk.wav) at the gadget. It
    stays there as the arm moves, so you hear it in stereo from the wrist. The gadget's hand also gets two short soft
    pulses, like a watch.
  - The game's own talk.wav and secret.wav are moved to the gadget then, not doubled. They arrive just before their
    message, so one chime covers a burst.
  - Messages that come while you are looking behave as before.
- **Hooks:** `VR_GameSound` (cl_parse.c's sound packets), `VR_GameLineOnWrist` (Con_DrawNotify), and
  `VR_CenterPrintOnWrist`, which now also covers this mode.

### FPS counter (`vr_gadget.cpp`, `vr_main.cpp`)

- `vr_gadget_fps 1` shows ` 90 FPS CPU  5.2 GPU  8.1` just under the gadget as you see it. It faces you, like the
  hologram above it, and fades in with the gadget's facing.
- The characters are about 5 mm, in the screen's colour, with the hologram's shade: faint scanlines, no flicker or
  glitches, so the figures read steadily. The words are dimmer than the figures.
- **FPS** comes from the frames' own periods.
- **CPU** is our work per frame: the host frame less the runtime's and the swap's waits (the memory log's `busy_ms`).
- **GPU** is the eyes' drawing (`gpu_eyes_ms`), from the timer queries that run every frame anyway.
- All three are averaged over half a second, through a third reader of `profile::takePhases`. There is no new
  measurement code. Its small image is drawn again only when the figures change.
- **Cost** (exclusive runs, mock, e1m1, the gadget raised; 8 alternating 30-second blocks, `vr_memstats`):

  | | Off | On |
  |---|---|---|
  | CPU busy_ms | 0.611 | 0.611 |
  | GPU eyes | 0.465 | 0.473 |

  The difference is within noise. vr_profile agrees:
  - its GPU scopes cost 0.005-0.006 ms per eye;
  - its image costs 0.066 ms twice a second, which is 0.001 ms per frame on average.
- An earlier block order (all Off first) showed +0.07 ms of GPU time. It went away once the blocks alternated, so it
  was the GPU's clocks drifting, not the counter.

### Flashlight (`make_flashlight.py`, `vr_flashlight.cpp`)

**The model.** A straight tactical torch, 13 cm long:
- a knurled tube, 2.6 cm across, with three grip rings;
- a ribbed tail cap with a rubber button, where the cord goes in;
- a rubber switch on the tube just below the head;
- a finned head, 3.7 cm across, with a steel bezel and the lens.

It is flat-shaded like the other props. The origin is in the middle of the grip. `flashlight_flip.wav` is new: the
torch scuffing round in the palm and seating. The other sounds come out byte-identical.

**In the hand.**
- The tube goes through the curled fingers, along the fist's axis. The switch faces the knuckles, under the thumb.
- **Low grip** (the default): the beam comes out of the thumb's side. Pitch the hand forward a quarter turn from a
  pistol's aim to light ahead: the thumb points out and forward, as when you hold a torch at your side.
- **Overhead "searching" grip:** away from a gun, press **B/Y** with the torch in your hand and it turns round in the
  fist. The beam now comes out of the little finger's side: with the fist raised by your head and the thumb towards
  your face, it lights ahead. Press again to go back.
  - Each flip gives a click and a light buzz.
  - Each hand remembers its own grip for the next time it takes the torch, across letting go and across maps.
  - The flip is instant, a regrip. Animated over 0.16 s, the hand's grasp had to be solved again every frame and the
    palm jumped by up to 5 cm.
- **The fitted hands' grasp** wraps the fingers round the tube. At your In Hand Forward/Up (-5 / -4 cm), the palm
  moves 1.2-1.5 cm to fit, low or overhead, in either hand (`vr_debug_grasp`).
- **Voice notes:** while the off hand holds the torch, its Y turns the torch round even at your mouth, instead of
  starting a voice note. The overhead grip is by the head. The other hand's `+vr_note` binding, or putting the torch
  back first, still records.

**On the chest.** The torch points forward, tilted down by Tilt Down, its tail 1.2 cm in front of the chest where the
cord comes out of the clip. It protrudes about 14 cm. Forward/Up/Out move it.

**On a gun.** It lies parallel to the barrel, the lens 1 cm behind the muzzle, running back along the gun, its switch
out to the side:
- It sits under the gun. Where the gun's underside is deep over the torch's length, it goes beside the gun on the side
  away from your body instead.
- This is fitted once per gun and size, from the gun's drawn triangles, in 0.2-0.5 ms: under the shotgun, super
  shotgun, nailgun, rocket launcher and lightning gun (4.7-10 cm below the aim line); beside the super nailgun and
  the grenade launcher.
- The attach rules are as in round 20. It follows the gun as drawn, in either hand, mirrored for the off hand.

**Everything else:**
- The beam comes out of the lens along the tube.
- A hand reaches the torch anywhere along its axis, from the tail to the lens, within 9 cm, both to take it and to
  switch it.
- The switch's clicks come from the switch, and the clamp's from the torch, instead of from inside your head.
- The haptics are as before.

### Tested (mock headset)

Composites are in the scratchpad's `round21_gadget/`:
- **`casings_before_after.png`** (e1m2's pool, every second frame): before, the casing vanishes into the water; after,
  the tiny splash, ring and ripple. The log shows "VR shell into a liquid: ... splash, plip".
- **`hologram_and_fps.png`:**
  - the test message pressed 1, 2, 3 and 5 times: they stack, and are gone 7 s later;
  - Messages Only with the wrist down: nothing in view. Raised 8 s later, the message is still there. It is gone 5 s
    after it was first seen;
  - e1m1's "You can jump up here..." trigger with the wrist down. Its talk.wav is moved to the gadget, with one chime
    and the buzz logged. It waits, and a secret's two messages stack with it;
  - the menu path: the cursor on Show a Test Message, pressed;
  - Console Messages In view: the game's print is left out of view while the engine's line stays;
  - the FPS counter under the gadget;
  - re-checked after merging the fitted hands.
- **`flashlight_in_hand.png`:** before and after, off hand, four sides.
- **`flashlight_grips.png`:** both grips in both hands, close and from 90 cm, with the beam on the wall. In every case
  the beam points ahead, including overhead with the thumb towards the face.
- **`flashlight_on_guns.png`:** every gun before and after, the off hand's guns mirrored.
- **`flashlight_chest.png`:** before and after.
- `fl_model_new.png`: the model on its own.

### In the headset

- [ ] Shoot the shotgun over water, and flick-reload over a pool: a small splash and a quiet plip per casing, nothing
      louder than the shot's own splash.
- [ ] Screens > Messages > Show a Test Message with the wrist raised: change Hologram Text Size, Height and Effect
      and press again. They stack.
- [ ] Messages Only on the Gadget on, wrist down:
  - [ ] walk into a trigger or a secret, or touch a locked door. Nothing in front of you, a chime from your wrist
        (left or right ear, as the arm is) and two soft pulses;
  - [ ] raise the wrist seconds later: the message is there, and fades after Hologram Time;
  - [ ] pick something up: no chime, no buzz, as intended. Tell me if you want pickups to notify too.
- [ ] Graphics > Performance > FPS Counter on the Gadget:
  - [ ] readable at arm's length?
  - [ ] check that the figures match what you feel (GPU is the eyes' drawing, CPU our work, both without the
        runtime's waits).
- [ ] Flashlight in either hand:
  - [ ] the tube in the fist, the fingers round it;
  - [ ] pitch the hand forward to light ahead;
  - [ ] B/Y away from a gun flips it to the overhead grip. Raise it by your head, thumb to your face: it lights
        ahead. Flip back;
  - [ ] let go and take it again: that hand's grip is kept.
- [ ] On the chest: pointing forward, out of the way of your arms? (Forward/Up/Out and Tilt Down move it.)
- [ ] Clip it on each gun: parallel to the barrel, under it (beside the super nailgun and the grenade launcher). Say
      which gun looks off, and use On Gun Forward/Up/Out.
- [ ] Off hand holding the torch at your mouth: Y flips it rather than recording. Is that the right priority?

Not verified: the haptic pulses and the stereo position of the chime were only logged in the mock (it has no
haptics or ears), and the counter's legibility at the Quest 3's resolution was not checked (the mock is 960 × 540).
