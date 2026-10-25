# Struggling in the hands

A living knocked-down humanoid that a player's hand holds fights the hold: lifted, dragged, held by its head, its
legs or an arm. This is one layer of blended behaviours, not one move per hold. Code: `Quake/vr/vr_ragstruggle.cpp`
(the behaviours, in `ragactive`), `Quake/vr/vr_box3d_struggle.cpp` (what holds it, the class profiles, the overlay, the
status command). Settings: Combat > Holding Enemies > Struggle. Notes and numbers: ROUND22.md, "Struggling in the
hands".

## When it runs

`box3d::livingStruggle` runs each frame for each ragdoll from `driveActive`, before the drive. It wants a struggle when
all of these hold:

- `vr_struggle` is on and `vr_ragdoll_active` is on.
- The enemy is alive and knocked down (its ragdoll stands for it). Its class has a struggle profile and its monster group's Struggle is on (`vr_mgrp_<group>_struggle`).
- A player's hand holds a part of it (`World::ragdollGrabs`). That covers the generic limb hold, a throw held on, a
  lift and a leg lift (`handOver`).
- Nothing else leads its body: no shove topple, stagger, get-up, ledge climb, death, dismemberment or liquid. A throw's
  fall leads until 0.3 s after it lands.

When it wants one, the drive is begun if needed (no clip: each joint holds its own turn) and
`ragactive::beginStruggle` seeds it. A lifted one's flail (`Liquid::Air`) stays as a flag: the struggle stands in for
its thrash (`drive()` skips `swimFrame` while both are on). Once it stops wanting one (let go, dead, a liquid, the
setting off), `struggleFrame` ends it. The strengths fade over 0.35 s and the drive ends as usual. A lifted one still
held when the setting goes off keeps the flail's strengths. Standing enemies held by `foegrab` are not ragdolls and do
not struggle (their AI moves them).

## The context (each frame)

- **What holds it:** each hold's part sorts into torso (pelvis, chest, an upper arm: a shoulder), head, legs or arms
  (forearm, hand). The hold's lag is the hand anchor's distance from the held point, in units.
- **The floor:** every 3rd frame the floor under the pelvis, chest and head (`floorBelow`) gives `contact` (0..1, a
  part touching at 0.14 m or less, off at 0.36 m). `lift` is 1 - contact.
- **Its motion:** `drag` is the level speed of the pelvis and chest, from 0.1 to 0.6 m/s, times contact. `dragWay` is
  that speed's direction.
- **Its orientation:** `prone` is how far the chest's forward points down. `inverted` is how far its spine points
  down (head below pelvis).
- **The player:** his head and feet, and each holding hand. The goals that aim at them follow them live.
- **Its state:** health over its class's full health, a hit (`ragactive::struck` makes it flinch), its class's
  strength and speed.

## The behaviours

| behaviour | weight wants | limbs | torso, head |
|---|---|---|---|
| Swat | torso held x (0.55 + 0.45 lift) | free arms: wind up (raised, back, out), strike at your head or the holding hand (each cycle's roll), recover. The favourite arm leads, the other at 0.75 | twist with the swing, look at you |
| Pry | head held + 0.8 arm held + 0.25 torso held on the floor | free arms to the holding hand nearest each shoulder, tugging it back 5-10 cm. Out of reach, they still reach | hunch, look at you |
| Crunch | legs held x lift x inverted | free arms grasp for your hands and middle | waist flexed forward 0.8-1 rad, held hips flexed 1.4 rad (at Held Limb Keeps). An internal pull draws chest and head up and thighs down, 0.3 body weights in pulses, net zero on the hold |
| Claw | legs held x contact x (0.35 + 0.65 drag) | free arms stroke ahead of the head along the floor, pressed to it | head up. A capped torque about the spine turns it belly down |
| Brace | (arm held + 0.6 head held) x contact x (0.35 + 0.65 drag), legs not held | free legs straight along the body, heels 5 cm into the floor | slight hunch |
| Kick | legs free x (0.35 + 0.65 lift) x Flail, less as it braces | free legs kick between seeded places, two a cycle | - |
| Thrash | (0.5 + 0.5 lift) x Flail | - | seeded noise twist, lean, nod; the head tossed |

Each want is also multiplied by its own setting (0..2). The weights ease towards the wants (`vr_struggle_blend`,
0.25 s). Each limb's goal is the weighted mean of its behaviours' goals, with a reach weight of min(1, sum). The torso
and head bends are blended the same way. So a change of hold (lifted, then put down and dragged) moves smoothly from
one struggle into the next.

Muscles: each channel's strength is the sum of weight x the behaviour's share (table in `behaviourData`), capped (arms
and legs 1.4, torso 1.2, head 0.9). It is then multiplied by effort, the grip guard and `vr_struggle_strength`. Through
the drive it gets `vr_ragdoll_active_strength`, the joint torque caps and the 20 Hz spring cap, as for any active
ragdoll.

Round 22's second pass (his VR notes; ROUND22.md, "Struggle 2"):

- **Slap at Your Hands** (`vr_struggle_frantic`, 1): held by the head (Pry), the legs (Crunch) or an arm (Pry, at half),
  the free hands slap at the holding hand on a quicker beat of their own, drawn back 0.4 of the arm and struck in, the
  beat and the aim jittered by seeded noise (`slapAt`). On its head they aim at your wrist and forearm over it, so
  they come up over its head. Their arms get +0.5 x Frantic strength and keep half against the grip guard.
- **Arm Reach** (`vr_struggle_arm_reach`, 150 deg): the rig's shoulder cone is 85 deg about the hanging arm, so a
  hand never got above the shoulder (the arms stuck at the torso). While it struggles the shoulders' Box3D cone and
  the drive's clamp are widened to this; after, they ease back at 1.5 rad/s, never tighter than the arm is turned
  (no snap), the drive kept on until they are back (at most 3 s, then back at once as it ends: `shoulderReach`).
- **Squirm** (`vr_struggle_squirm`, 1): Thrash wants +0.8 x Squirm more while its head or legs are held.
- **Legs held on the floor, on its back**: Crunch also wants 0.6 x contact x (1 - prone) (it sits up at the hands),
  and Claw is x (0.4 + 0.6 prone) (less of the glide onto its belly).
- **Dragged by an arm**: Swat also wants 0.7 x arm held (the free hand swats at the holding one). **Stay Upright**
  (`vr_struggle_upright`, 1): its feet planted under its hip, knees bent (over Brace, which eases by 0.7), Kick less;
  its back rights its chest (a capped turn, 0.045 body weights x m); its legs hold up to 0.8 of its weight once its
  chest is up past 0.2 and its middle sinks below a crouch (`stayUp`). Not eased by the grip guard (holding its own
  weight eases the hold). Lying flat it can only try to sit up: it is not lifted.
- **Voice** (`vr_struggle_voice` 1, `vr_struggle_voice_gap` 1.8 s): its own class's samples, never a sight sound (AD's
  `ad_pain_sound`, `ad_pain_sound2`, `ad_idle_sound` fields; id1's from a table; only precached ones) on CHAN_AUTO,
  ATTN_NORM: one as it is taken, then every gap x (0.6..1.4) / (0.45 + 0.55 effort), one as a burst begins (0.7 s
  apart at least), none within 0.6 s of a hit (the QC's pain sound), never the same sample twice running, idles only
  once its effort is under 0.5, volume 0.4 + 0.6 effort, pitch 0.94..1.06 (`struggleVoice`).

Rhythm: each limb has a phase. It advances at its behaviours' blended rate, times `vr_struggle_speed`, the class speed,
its own rate (seeded, +-35% x Variation), a burst's quickening and its effort. The arms alternate and so do the legs.

## Effort

effort = ramp in (0.35 s) x (1 - fatigue x (1 - Effort When Tired)) x burst x flinch x health x class strength.

- Fatigue builds at effort / Tires After per second. A new struggle on the same drive keeps it, recovered meanwhile.
- Bursts: seeded fits of 0.35-0.8 s every 0.9-3.1 s. A fit adds up to 0.5 x Bursts to effort and quickens the rate.
  Between fits there is a lull of 0.2 x Bursts.
- Flinch: a hit drops effort to 0.2, recovering over 0.5 s.
- Health: 0.45 + 0.55 x health share.

## Limits and the hold

- **Slack:** a held part and everything hanging from it is slack, as before. A held torso part alone is slack, so its
  arms and head still fight (before, a chest hold left every joint above the chest limp). The joints between a held
  part and the torso (the hip of a held shin) keep Held Limb Keeps (0.5) of their strength.
- **Grip guard:** past Grip Guard units of lag (6), every strength eases towards a quarter, reached at 2.5 x the guard.
  It eases down in 0.05 s and back up in 0.4 s, so the struggle never tears the hold loose by itself. A drag's own lag
  (the hold's spring lagging a body on the floor) also eases it. That is wanted: the hold's let-go (72 units, half a
  second) stays the hold's own business.
- No break-free beyond the lift's existing timer.

## Classes

The monster groups' Struggle settings (Combat > Monster Groups; ROUND22.md, "Struggle in the monster groups"): on/off,
Strength, Speed and Arm Drive (times each class's own) and Weaker When Hurt, per group; each class's own strength, speed,
full health and arm drive are `vr_mgroups.cpp`'s `struggleProfiles` (`vr_struggle_classes` until configVersion 190).
The profiles: grunt, enforcer, knight, death knight, zombie, mummy, and AD's soldiers and knights (defender, eliminator,
pyro, fumigator, deflector, crossbow, fury, guard, dknight, sergeant, zombie knight, death lord, Dark Prince; Quoth's
death guard). A struggling group's member with no profile struggles with its group's base profile (`struggleBases`,
2026-10-23: the minotaur, golem, drole and justice knight as the Dark Prince, the point and ranged knights as the death
knight, Honey's soldier as the grunt...); left out: the centurion, the death knight ghost, the Shub zombie
(`struggleLeftOut`). Not ogres, fiends, shamblers or vores. Health is its full health, used for its
effort. Arms (1..2.5, 1 if left out): while it struggles its arms' joint springs (and their 20 Hz cap) are that many
times stiffer. A Box3D joint's spring acts on its two bodies' own inertia, so an arm whose forearm outweighs its upper
arm lags its goals: the enforcer's forearms (his gun hands) are 5 kg on 0.9-2.3 kg upper arms and a 58 kg chest (the
grunt's 0.9 kg on 3-4 kg). Shipped: enforcer 2.5, death knight 1.5, zombie 1.5 (ROUND22.md, "Struggle 3").

A lift (`foegrab` handOver, Why::Lifted) holds its chest or pelvis rather than an arm across it when the torso's
surface is within 10 units more of the hand (`ragdollHoldSoft`'s `torso`): the enforcer's forearms are before his chest
(the rifle in both hands), so a chest lift took a forearm and that arm hung slack.

Voices never use a sight sound (its cry on noticing you): the table has none, AD's `ad_sight_sound` is not read, and a
sample named `sight` is skipped.

## Debug and tests

- `vr_struggle_debug 1`: a panel over each struggling enemy with the seven weights (bars), effort, fatigue, what is
  held, contact, drag, prone, inverted, the hold's lag and the guard. `2` also prints a `struggle:` line 4 times a
  second. The same rows are under Debug > Tests > Holding Enemies.
- `vr_struggle_status`: each struggling enemy's weights and measures. For each hold, struggling or not, it prints the
  lag and the pose: waist bend, belly down, head over pelvis, the nearest forearm to your head and to the holding hand,
  legs' straightness. It also prints the struggles' cost since the last call and a hash of every ragdoll part's
  position. `vr_struggle_status hurt <n>` takes health from the held ones, for the death fallback test.
- `vr_struggle_force 1`: every living knockdown of a struggling class struggles as if held at its chest where it lies,
  for the cost with many.
- `Misc/quakevr/foegrab/struggle_test.py <agent> [regex]`: lift, leglift, legpull (both shins pulled over and walked back:
  dragged by the legs, its free arms' flail speed against the struggle off's), legdrag (INFO: the mock cannot take a lying leg), headdrag, armdrag (each also with the
  struggle off) and dead.
- `vr_struggle_status` ends each struggling line with `flail <u/s> miss <u>`: its free hands' speed in its chest's
  frame and their mean distance from their goals since the last call (the flailing's measure). `STRUGGLE_CLASS`,
  `STRUGGLE_SPAWN` (its dispenser number), `STRUGGLE_EXTRA` (commands) and `STRUGGLE_LABEL` run struggle_test.py on
  another class; it also has `headlift` (held up by its head before you).
- `vr_struggle_status` also prints its free hands from its own head and over it, Upright, the force holding it up,
  its chest's uprightness and its voices; each hold line its nearest hand from its head and its chest up.
  `vr_struggle_debug 1` prints each voice.

## Cost

Measured in ROUND22.md: about 0.004 ms per struggling enemy per frame for the layer (0.01 ms for its whole drive). The floor rays run every 3rd frame. There is no
allocation and no per-frame string work; the class list is parsed only when it changes.
