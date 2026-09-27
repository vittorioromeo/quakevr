# Round 21: melee from your recordings, fitted hands, the flashlight reworked

Your notes after round 20 (finger sliders not good enough, melee still misreading swings, a "no hit" set that
hits), plus the requests made in chat during the round. Melee is now tuned and checked against your own recorded
motions, replayed in the engine, rather than against hand-made test motions.

| Area | Result |
|---|---|
| Motion recorder | VR menu page: pick a category, click the off stick to start and end a take; 474 takes recorded so far. `vr_motion_play` / `vr_motion_eval` replay them on the dummy, deterministic and order-independent, ~0.7 s a take |
| Reviewing takes | the evaluation marks each take (`motions/eval_status.csv`); Review Takes lists the failing and suspect ones, plays one as a ghost in front of the dummy in the headset, keeps, discards or relabels it (Undo Last); Re-evaluate in a second copy of the game |
| Melee | one model for swords, axes, Mjolnir, guns and fists (`QC/vr_melee.qc`): blows by the hand's speed and 20 cm travelled, the kind by which part hit (tip along the blade = stab, far end = slash, near end = pommel), parry bash = stance held 0.5 s then pushed; your 474 takes: 420 pass (the old code: 346), at 120, 90 and 72 Hz alike |
| Fitted hands | jointed hands (3 joints a finger, a real thumb) that close smoothly; fingers wrap what they hold (guns, blades, boxes, gibs, armour), solved on the main thread in 19–127 µs when the grip changes, 0.02 ms a hand a frame; recoil moves the hand again; hotspots (up to 4 per weapon, Grip/Blade/Cup, with a bias); Inherit From for alternate models; fingers stop at walls |
| Weapon offsets | one transform per weapon in the hand (30 of 106 per-weapon keys retired), your placements migrated exactly |
| Hand tuning | Weapon Offsets: hand and weapon moved together (the aim follows), the hand alone, a hotspot's held hand, overlap and manual fingers per weapon and hotspot; the controller and its lasers drawn; cups where the palm sits; the same grip every time a weapon is taken |
| Flashlight | a straight torch held through the fist, two grips (B/Y away from a gun), clipped along the barrel, worn on the head, stored hanging from the belt; only deliberate presses take or switch it (the chest clip was grabbed by guard fists) |
| Wrist gadget | hologram test message; messages only on the gadget (a chime from the wrist and a buzz); an FPS / CPU / GPU counter |
| Casings | a tiny splash, ripple and plip in water, slime and lava |
| C++ audit | 17 fixes: the upscaler's per-frame 60 KB string, NaN-safe network moves, unsigned hashes, flat water-ripple table, decal/gore rings, caches reset on a game directory change, beam quality (Medium default, 0.002–0.010 ms instead of 0.018), O(n²) eviction removed; INSTALL.md requires the VC++ redistributable 14.44+ |
| Defaults | your round-20 test settings and weapon placements (`vr_wofs_version` 14); Fit Gap down to -6 cm |
| Box3D physics | a second rigid-body engine to compare (Throwing and Physics > Physics Engine, live): Erin Catto's Box3D, single-threaded; props collide with each other (stacks, pyramids, piles, knocks), everything else as before; 0.12-0.15 ms a server frame with 52 props settling; your 474 takes identical on both |
| Carrying after a load | a box carried in one hand or both when the game was saved is drawn in the hand(s) again after loading it (it was drawn 20 m away, out of sight) |

Found on the way: fteqcc stores 0 when `a || b` is assigned into an entity field (rewritten; no other code has that
shape); a parried blow's hand knock, timed by `cl.time`, came back after a level change (reset now).

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

**Why the three failures happen.**

- **The bench-press bash.** The old bash wanted the guard held *still* first (the wrist under 1 m/s, the weapon
  turning under 60 deg/s, for 0.15 s) and then a push that turns under 120 deg/s. Your real pushes (36 parry-bash
  takes) turn the weapon by hundreds of degrees a second as the arms extend (665 deg/s in one), and run at 3-6 m/s.
  Live, the old code registered **0 of 36** of them as a bash.
- **Backswings.** The old blow was the estimated wrist's *stroke* (3.4 m/s for a weapon, 0.2 m, a 25 m/s2 snap, not
  going back from the facing; any backward part raised the distance needed). In your recorded takes it did register
  34 of 37 backswings (as swings), but only as the wrist's stroke allowed: a backswing that starts from a
  follow-through, going back towards you, fails the "not going back" test, and a synthetic reproduction of that
  landed 0 of 3. The new model has no direction test at all.
- **The off-hand gun after the hand-off.** `PlayerVRMeleeImpl` returned at once for a hand carrying a gun by its
  foregrip (round 16: "carried, not wielded"), `VR_Parry_Guard` refused it as a guard and `VR_Corpse_StrikeFrame`
  skipped it. And the client sent no muzzle for a carried gun (the view marks it "no aim"), so the server knew only
  a stub 8 units ahead of the hand, not the gun. Now the client sends a carried gun's muzzle
  (`vr_client.cpp`, `handMuzzle`: the weapon's muzzle offset in the hand's frame, placed from the carried pose), and
  the carried gun is a gun like any other: it strikes, parries and bashes.

The common root: the old model decided *whether* there was a blow from the wrist's motion alone (a stroke's speed,
distance, snap and direction), then asked what the weapon touched; the rules layered on since round 14 (strokes,
back-bias, hilt windows, stab tests, guard holds, swing-rate limits) each patched one case of that and broke others.

### The design

One model for everything a hand holds, in `QC/vr_melee.qc` (new; `vr_juice.qc` keeps only batting and the
heartbeat). Its numbers are in one table at the top of the file, each with the takes it came from. It was tuned
only on your recorded takes, replayed in the engine (`vr_motion_eval`); the synthetic takes were smoke tests.

**1. What a hand holds is a line with points.** Each server frame with a new pose, the held thing is laid out from
its *near end* through the *grip* (the hand) to its *far end* (the muzzle point of its model: a sword's tip, an
axe's head, a gun's muzzle), and points of four kinds along it are swept from the last pose to this one against
the boxes of what bleeds (3 units thick) and against walls:

| Held | Near end (pommel strike) | At the hand (strikes nothing) | Striking |
|---|---|---|---|
| Sword | the pommel | the hilt, the guard | the blade's root, mid-blade, the blade, the tip |
| Axe, Mjolnir | the handle's end | the handle | the head |
| Gun, held or carried by its pump | the butt | the grip | the barrel, the muzzle |
| Fist | - | - | the fist, the knuckles |

Two-handed, the helping hand's place is the weapon's nearest grip hotspot within 24 units (`weaponhotspot`, the
fitted hands' list), else the hand itself (`VR_Melee_HelpGrip`). The steadying hand and a climbing hand hold
nothing.

**2. The arm drives every blow.** A contact is a blow when the *grip* (the hand, relative to the head, so walking
and turning don't count) moves at least:

| Blow | Least speed of the hand |
|---|---|
| a weapon's swing (slash, chop, gun swing) | 1.25x `vr_melee_speed` (5 m/s) |
| a stab (a sword's far end moving along the blade, outwards: cosine 0.4 or more) | 0.75x (3 m/s) |
| a pommel or butt strike | 1x (4 m/s) |
| a punch (a closed fist: the grip held) | 1x (4 m/s), a little more straight up or steeply down |

and the hand came at least 20 cm towards where it goes now, within the last 0.3 s (a blow, not a wiggle). The
direction doesn't matter otherwise: overhead, sideways, diagonal, rising, backhand and follow-ups are alike. Your
takes behind the numbers: your cuts move the hand 4.5-13 m/s, your sword and gun waved at the dummy 2-4 (their tips
10-30 m/s, which is why the tip's speed was the wrong measure); your stabs drive the tip along the blade at 0.4-0.87
(cosine), your cuts at most 0.36; your punches come 0.22-0.8 m, your fists waved fast at the dummy 0.05-0.2.

**3. Which part struck decides the kind.** Of a blade's points in the thing, the outermost strikes (the whole blade
hits, and its root, in the thing for a while, doesn't hide the blade proper). The near end's touch waits 0.1 s for
the blade proper (from 0.4 of the way to the tip) to reach the same thing: then the blow is the blade's (a cut from
close by, whose pommel or handle arrives first), else it lands as the pommel strike it was. The points at the hand
strike nothing: your 25 pommel strikes all land with the near end, while your weapons waved at the dummy touched it
with the hilt, the handle or a gun's grip.

Damage is the weapon's base x weight x strength: 0.5 at the least speed, 1 at twice it, at most 2. Weights: a
sword's pommel 0.6, an axe's handle end 0.5, a punch `vr_melee_punch_mult` (every punch alike, whatever its way).
A hand strikes each thing once per motion, again once its hand slowed below half the least speed, or, out of the
thing, turned back more than 100 degrees (a backswing) or after 0.6 s.

**4. The parry stance** is the parry's own test (`VR_Parry_Blocks`, `combat.qc`): the weapon's line (a carried gun
too; two-handed, the line between the hands' grips) within `vr_parry_angle` (40) of level, across (at least 30
degrees off the line to the attacker), in front, its middle within half a metre of your body's midline. Bare hands:
crossed forearms (the unarmed parry), or an open palm facing ahead (within 53 degrees) for the shove.

**5. A bash is the stance, held, pushed.** Once a weapon's stance has been held 0.5 s, both ends of the weapon
moving forward (each within 41 degrees of the facing, each at 0.4x the push or more) with its middle at
`vr_bash_speed` (2 m/s), for 6 cm, is a bash: it lands on what is within 0.75 m ahead, once. However the weapon turns
as the arms extend, it goes on (the stance may tilt past its angle as you push). The hold is what separates your
bashes from your cuts that start from a guard: your cuts from a guard pass through it in 0-0.4 s; your parry bashes
hold it 0.9-2.3 s. Two hands bash harder (a two-handed weapon, an open palm pushing on a one-handed weapon's line
within 20 cm, both palms): full `vr_bash_damage` and `vr_bash_push`; one hand 0.6 of the damage and 0.7 of the
knockback. Within 1.5 s of a parry, a parry-bash (1.3x). A bash with a weapon bats projectiles, as before.

**6. A shove is an open palm pushed out.** An empty open hand (the grip not held), its palm facing ahead (within 53
degrees), moving the way it faces (45 degrees) and out from its shoulder (cosine 0.82: the arm extending), at
`vr_shove_speed` (2.4 m/s), for 6 cm. Both palms: a two-handed shove. Your shoves extend the arm at 0.84-0.98; your
hands waved down or round at the dummy, palm first, at 0.55-0.8.

**What the readout and the recorder see.** The dummy's readout names the blow ("melee: Sword, slash (diagonal,
down) with the blade, main hand, 9.1 m/s (x1.1)"; "bash with the Sword, two hands"). The recorder's events carry
the blow's category (`melee/slash`, `stab`, `pommel`, `punch`, `gun`) and its part; a "stroke" is a hand going as a
blow would (its whoosh), a "stance" the parry stance held 0.2 s.

### Settings

| Setting | Before | Now |
|---|---|---|
| `vr_melee_speed` | the estimated wrist's least speed in a stroke (2.75 m/s; yours 2.75), with `vr_melee_distance` and a snap | the striking hand's least speed, **4 m/s**: a punch, a pommel strike; a weapon's swing 1.25x, a stab 0.75x |
| `vr_bash_speed` | the push after a still guard (yours 1.5 m/s) | the held stance's push, **2 m/s** (your pushes 3-6, your held poses settle at up to 1.6) |
| `vr_shove_speed` | 1.8 m/s | **2.4 m/s** (your shoves 3.2-4.8, your hands waved at the dummy 2.2) |
| `vr_melee_punch_mult` | a straight punch over a slap | every punch alike |
| `vr_parry_angle` | 40 | 40 (unchanged) |
| `vr_melee_distance`, `vr_melee_hilt_window`, `vr_melee_stab_speed`, `vr_bash_hold`, `vr_bash_swing_rate` | | removed, with their menu sliders (Blow Distance, Hilt, Stab, Hold, Swing Limit) |

Menu: Melee > Swing Speed (`vr_melee_speed`), Bash Speed, Shove Speed, with new help. `quakevr/vr_defaults.cfg` no
longer sets `vr_melee_speed`, `vr_melee_distance` or `vr_bash_speed`.

**Config migration, version 13** (`vr_cvars.cpp`): `vr_melee_speed` and `vr_bash_speed` go to the new defaults
whatever they were (their meaning changed); `vr_shove_speed` moves from the old default 1.8 to 2.4 (a value you
changed yourself stays).

### Checked on your 474 takes

`vr_motion_eval` on all 474 takes (vrfiringrange, developer 0, the takes' 120 Hz), against
`quakevr/motions/expect.cfg`. The old code's numbers are what the takes registered live. The takes are split per
category by a hash of the file name: about 70% to tune on ("train"), 30% held out and never looked at while tuning.
"Buckets" count a take right when it registers the right kind of thing (the coordinator's buckets); "strict" is the
eval's verdict (the right kind, and nothing else: a slash that also bashes fails).

| Category | Takes (train / held-out) | Old code, live | New, buckets | New, strict |
|---|---|---|---|---|
| slash | 95 / 38 | 82 / 35 | 91 / 37 | **90 / 37** |
| stab | 20 / 8 | 4 / 1 | 19 / 8 | **19 / 8** |
| hilt_pommel | 23 / 4 | 0 / 0 | 20 / 3 | **20 / 3** |
| parry_bash (sword and gun) | 25 / 11 | 0 / 0 | 23 / 10 | **23 / 10** |
| punch | 21 / 7 | 17 / 7 | 21 / 7 | **21 / 7** |
| gun_strike | 26 / 9 | 21 / 5 | 22 / 4 | **22 / 4** |
| palm_shove_1h | 3 / 6 | 3 / 6 | 3 / 6 | **3 / 6** |
| palm_shove_2h | 4 / 4 | 4 / 4 | 4 / 4 | **4 / 4** |
| parry_pose | 56 / 26 | 52 / 23 (strict) | 56 / 26 | **53 / 24** |
| not_parry_pose | 36 / 13 | 28 / 10 (strict) | 36 / 13 | **29 / 10** |
| no_hit | 24 / 15 | 15 / 11 | 17 / 10 | **17 / 9** |
| all | 333 / 141 | 238 / 108 (buckets) | 312 / 128 | **301 / 122** |

In all, **423 of 474** pass the strict verdict (the old code: 346 right by the buckets). For the poses, the old
code's strict verdict comes from its parry and guard state recorded each frame. Worse than before on the held-out
takes: gun strikes (4 of 9 against 5) and no hit (9 of 15 against 11); on all the takes no hit is 26 of 39 against 26
of 39 and gun strikes 26 of 35 against 26 of 35. The takes behind those are in the list below: guns carried by the
pump whose carry the replay can't place, gun clubs recorded before the fitted hands, and weapons swung through the
dummy at blow speed.

By label (strict, all takes; the old code live):

| Label | Takes | Old | New |
|---|---|---|---|
| slash_overhead | 24 | 19 | 24 |
| slash_horizontal_ltr | 23 | 19 | 20 |
| slash_horizontal_rtl | 17 | 16 | 17 |
| slash_diagonal_down_left | 17 | 16 | 17 |
| slash_diagonal_down_right | 15 | 13 | 14 |
| slash_backswing_up_left | 18 | 17 | 16 |
| slash_backswing_up_right | 19 | 17 | 19 |
| stab_one_hand | 18 | 5 | 18 |
| stab_two_hands | 10 | 0 | 9 |
| hilt_pommel | 27 | 0 | 23 |
| parry_bash | 19 | 0 | 19 |
| parry_bash_gun | 17 | 0 | 14 |
| punch_jab / straight / uppercut / overhead | 6 / 9 / 7 / 6 | 6 / 9 / 7 / 2 | 6 / 9 / 7 / 6 |
| gun_strike_swing | 18 | 14 | 17 |
| gun_strike_butt | 17 | 12 | 9 |

**Frame rate.** The same takes resampled (`vr_motion_eval ... rate <hz>`), strict passes:

| | 120 Hz | 90 Hz | 72 Hz |
|---|---|---|---|
| before the last change (the pommel's wait only while the head swept faster) | 415 | 407 | 398 |
| now | **423** | **423** | **422** |

The difference was close chops: at 72 Hz a frame's sweep is longer, the pommel or handle and the head reached the
dummy in one frame, and the handle's touch didn't look like a swing's (your chops draw the axe back along its line,
as a pommel strike does). Now the near end always waits 0.1 s for the blade, and the points at the hand don't strike.

**Suspect takes** (not bent to; the number that says why):

| Takes | Expected | Registers | Why suspect |
|---|---|---|---|
| gun_strike_butt 02-31-23, -26, -29, -31; parry_bash_gun 02-35-08, -11, -14; no_hit 02-36-26 | gun strike / bash / nothing | nothing / a muzzle swing / a one-hand bash | a gun carried by its pump: the take doesn't record the carry, so the replay holds no gun there (the far end isn't recorded: no muzzle) |
| gun_strike_butt 02-31-19, 02-32-05, 02-32-02 | gun strike | nothing | recorded before the fitted hands (the recorder's 15 gun clubs): only the hand reaches the dummy (3 cm in; the butt 7-12 cm off), 02-32-02 never within 10 cm |
| no_hit 02-36-12, 02-36-36, 02-37-14, 02-43-23, 02-43-28, 04-03-55, 04-04-05 | nothing | slashes, gun swings, a bash | the far end goes 20-34 cm deep into the dummy's box (73 cm wide) with the hand at 5-13 m/s: a blow by any measure |
| no_hit 02-20-55, 02-20-59, 02-21-46 | nothing | punches | a closed fist at 7-10 m/s, 1-3 cm into the box (your punches go 10-29 cm in) |
| no_hit 02-21-55 | nothing | strokes only | fists at 14 m/s, 23 cm off: the whooshes, which "none" forbids |
| no_hit 02-21-03 | nothing | shoves | both open palms pushed out at 11 m/s |
| parry_pose 02-44-36, 02-45-09, 02-45-25, 02-45-34, 05-17-59 | the parry pose | parry in under half the frames | tilted 41-44 degrees, beyond your `vr_parry_angle` 40 (the old code failed them too) |
| not_parry_pose 05-12-50, 05-13-18, -21, -25, -49, 05-14-08, -10, 05-15-42 (and 05-15-38, -52: a few frames) | no parry | the parry pose | the weapon level within 8-40 degrees, across, in front: the parry's pose by its test (the old code failed them too) |
| slash_backswing_up_left 04-03-12 | slash | nothing | the far end never within 10 cm of the dummy |

**Where the model is at fault** (not suspects):

- Cuts that start from a guard held level half a second or more, both ends going forward first (slash 02-39-31,
  -33, -36, 02-41-28): their first 6 cm are a bench press; they bash. The same for one-handed pushes from a held
  gun or sword stance (gun_strike 02-37-32, 02-32-30; hilt_pommel 04-05-59).
- A backswing whose tip moves along the blade at the contact reads as a stab (slash 02-41-57).
- Pommel strikes whose blade follows into the dummy within 0.1 s read as slashes (hilt_pommel 02-44-18, 04-05-33);
  hilt_pommel 04-05-38 and stab_two_hands 02-47-48 register nothing (the hand at 5.2 and 5.8 m/s).

**expect.cfg** (`quakevr/motions/expect.cfg`): the categories that named the old blows' shapes (`melee`,
`melee@the_pommel|the_hilt|...`) now name the new kinds, as strict as before or stricter:

```
slash           melee/slash                                             weapon:sword|axe|mjolnir
stab            melee/stab                                              weapon:sword
no_hit          none
bash            bash parrybash
parry_pose      pose:parry !stroke !push
not_parry_pose  none !pose:parry !pose:guard
parry_bash      bash parrybash
hilt_pommel     melee/pommel                                            weapon:sword|axe|mjolnir
punch           melee/punch                                             weapon:fist
palm_shove_1h   shove/main shove/off                                    weapon:fist
palm_shove_2h   shove/both                                              weapon:fist
gun_strike      melee/gun                                               weapon:gun
other           -
```

**Notes.**

- fteqcc stores `a || b` assigned into an entity field as 0 (into a local it works; `&&` works). It made the bash's
  "stance held" test never pass (parry bash 0 of 36 in one run). Written with if/else; no other such assignment in
  the QC.
- Replay: a weapon in the off hand needs its grip pressed after it is taken (fixed in the recorder's playback);
  the flashlight is off for the eval (`vr_flashlight 0`: a hand reaching to the head grabbed it).

### Refinements after the author's review

Two fixes for edge cases from his notes on the failing takes. Neither has a menu slider.

- **Wiggles** (the hand flicked back and forth in place, registering as hits). A blow already needed the grip's speed
  and a 20 cm run, but the grip is the controller's point, and both it and the tip swing fast when only the hand
  turns: the pivot is well behind them. Fitted on his wiggle takes (fists, sword, axe, gun), the hand turns about a
  point 17-24 cm behind the controller along its own axis. The melee now tracks that point, the "wrist": 20 cm back,
  with the gun angle offsets (`vr_gunangle`/`vr_gunyaw`, `vr_offhandpitch`/`vr_offhandyaw`) taken out, or the hand
  itself for a weapon held two-handed. A blow, and its whoosh, also needs the wrist's net travel over the last 0.12 s
  to average **`vr_melee_wrist_speed` (1.1 m/s)**. A punch needs twice that, since a fist has no lever and the arm
  carries it. His real blows carry the wrist at 1.24 m/s or more (a wrist-heavy backswing 1.24, stabs from 1.34,
  pommel strikes from 1.9, swings and chops from 2.3 to 3, punches from 3.5). His wiggles carry it at 0.2-1.5.
- **The parry's muzzle.** A weapon whose far end (a gun's muzzle, a blade's tip) points within
  **`vr_parry_muzzle_angle` (45 degrees)** of the attacker doesn't parry, whichever line blocks. The angle is measured
  from the weapon's middle to the attacker (`vr_parry_from`, set by `VR_Parry` and the recorder). Without an
  attacker, as for the bash's guard, it is measured against the blow's way. His parry poses point 60 degrees or more
  off the dummy. His gun "not parry" poses that still fail point 80-97 degrees off it as seen from the gun, a spread
  his parry poses share, so no stricter angle takes any of them without breaking parry poses.

All 471 takes (strict; the 3 takes he has since discarded left out): **420 -> 426**. no_hit 26 -> 31 of 37:
02-20-55, 02-20-59 (fists), 02-36-36 (gun) and 02-43-23 (sword) by the wrist, and 02-36-26 (the carried gun's
one-hand bash; its stance now points at the dummy) by the muzzle. not_parry_pose 39 -> 40 of 49 (05-14-08, the
muzzle 30 degrees off the dummy). Every other category is unchanged, with the same events: slashes, stabs, punches,
pommel strikes, gun strikes, shoves, parry poses and bashes. The no_hit takes still failing are real arm motion
(the wrist at 2.5-3.6 m/s: 02-21-46, 02-43-28, 04-03-55, most of 04-04-05), or bashes and shoves (02-21-03,
02-37-14).

### In the headset

- [ ] Sword, one hand: slashes in every direction, backswings (bottom left to top right, and back), from close and
      from far: the blade hits, anywhere along it. A hit's strength follows the hand's speed.
- [ ] Axe and Mjolnir: chops from close by land with the head (not as a handle strike).
- [ ] Stabs, one and two hands: "stab" on the dummy's readout.
- [ ] Pommel strikes (sword pommel, axe handle end): "pommel strike" on the readout, weaker than a slash.
- [ ] Hold the sword level across your face, one hand on the handle and one on the blade, for half a second, and
      push it out: a two-handed bash with knockback. One hand: weaker. Right after parrying a blow: harder still.
- [ ] Cut from a held level guard: does it bash instead? (Known: if both ends go forward first, it does.)
- [ ] The off-hand gun carried by its pump after the two-handed hand-off: swing it, butt-strike with it, hold it
      across and push (a bash), parry with it.
- [ ] Guns held normally: swings and butt strikes; a butt strike lands with the butt, not with the hand on the grip.
- [ ] Punches in every direction (jab, hook, uppercut, overhead): the same damage for the same speed.
- [ ] Open palms pushed out: a shove; both palms: harder. Hands waved or patted at the dummy: nothing.
- [ ] Waving a weapon slowly through the dummy: nothing. Swinging it through at speed: a hit.
- [ ] Settings: Swing Speed (4), Bash Speed (2), Shove Speed (2.4). Your config moves to them once (version 13).
- [ ] Parry poses you use in combat still parry (the parry's test is unchanged: 40 degrees).
- [ ] Wiggle a sword, a gun or a fist in place against the dummy, fast: nothing. Short wrist-heavy cuts and
      backswings still land (`vr_melee_wrist_speed` 1.1).
- [ ] A gun held level but aimed at the enemy doesn't parry. Held across, it does (`vr_parry_muzzle_angle` 45).

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

The [second pass](#fitted-hands-second-pass) replaces the worker thread and the solver described below. Solves now take
20–130 µs on the main thread, and the hand follows the weapon's firing animation again.

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

## Fitted hands, second pass

Your twelve notes on the fitted hands, plus the coordinator's "no threads" rule for the solver and the C++ audit items in
this code. Branch `agent/hands2`. Each fix is its own commit, so an early working state can be merged on its own.
Before/after composites and traces are in the scratchpad's `hands2/final/`.

| # | Your note | Status | Commit |
|---|---|---|---|
| 2 | Recoil: the hand, arm and helping hand move with the gun's firing animation again | Done | `e1bc8c6d` |
| 4 | Stability: solved in the weapon's frame, hysteresis, blending, overlap slider, a tighter thumb | Done | `e1bc8c6d`, `4cf0046d` |
| 12 | The torch jittery in the left hand | Done: the cause was in both hands | `4cf0046d` |
| – | No threads: the solver on the main thread, well under 0.1 ms | Done, with the exceptions in [Costs](#costs-1) | `4cf0046d`, `12e8fcad` |
| 3 | The trigger finger curls with the trigger | Done, and the trigger's touch sensor is read | `4cf0046d`, `97602888` |
| 5 | Hotspot 2 can't be grabbed; a two-handed pistol grip | Done: every index works; new type Cup | `bd6740df` |
| 8 | The hand wraps thin things (a blade); wrap vs thumb on top | Done | `339d817b`, `bd6740df` |
| 10 | The thumb looks distorted (health box) | Improved; the rest is the model | `a308f397` |
| 9 | Things grabbed from far away float off the hand | Done, with `vr_debug_carry` | `3f6f2543` |
| 6 | The free hand should collide a little with the other hand's weapon | Done | `e61b30da` |
| 7 | "Inherit From" for the alternate models | Done | `bc964642` |
| 1 | Fingers stop at world geometry | Done | `a9510c06` |
| 11 | Remake the hand model (optional) | Not done | – |

Also: the merge of vr-cleanup with the game directory hook for this code's caches (`2a79220e`), the menu entries
(`83abfd7b`), and the solver's open-air steps (`12e8fcad`).

### Recoil (#2)

Round 21 placed the hand at the controller, so a firing animation (the shotgun's kick, the super shotgun's, the
grappling hook's) moved the gun through a still hand. Now:

- The hand holding a weapon, and the hand steadying it on a grip or blade, move with the weapon's animation where they
  hold it (`animationMotion`). This is the rigid motion that best fits (least squares: Kabsch, by a polar
  decomposition) the model's vertices near the hand, from the rest pose to the drawn pose. The arm IK follows the
  drawn hand.
- The grasp is still solved on the rest pose, so a kick never causes a re-solve.
- Measured at the kick's peak: the shotgun moves the hand 2.2 units and turns it 9°; the grenade launcher 4.5 units
  and 23°.
- `recoil_all.png`: round 20 (`30580890`), round 21 (`6f8b075a`) and now, at the same frames of the shotguns' kick.

### Stability (#4, #12)

The jitter had four causes, each fixed:

1. **The hand slid on the weapon.** The weapon's Pitch/Yaw/Roll were added to the hand's angles as Euler angles, so
   the hand's pose relative to the gun drifted by a few degrees as the wrist turned. Each drift re-solved the grasp.
   The hand is now carried rigidly in the weapon's frame (`attachedTurn`). The empty hand likewise turns rigidly with
   the controller (the fist's angle offsets were Euler too, which let a torch or a carried box slide in the hand).
2. **The torch was a frame behind** (#12). It was set up after the hands, so its place in the hand changed every
   frame the hand moved. Worse in the off hand, whose Euler offsets are mirrored. It is now placed this frame
   (`flashlight::heldPlace`).
3. **Hysteresis:** the grasp is solved again only when the held thing moves in the hand by more than
   `vr_hand_fit_resolve` (0.3 cm, or 0.6°). A re-solve warm-starts from the solution before: each finger opens from
   where it held until clear, then closes again, and the thumb keeps its turn. Two equally good holds can no longer
   swap.
4. **Blending:** the joints, the thumb's turn, and the palm's move and turn ease to a new grasp over
   `vr_hand_fit_blend` (0.12 s, exponential) instead of jumping.

Also:

- **Overlap:** the fingers and palm may sink `vr_hand_fit_overlap` (0.3 cm) into what they hold. They sit snug, and a
  finger that only just touches doesn't flick between two poses.
- **A tighter thumb:** the thumb keeps its turn between solves and closes onto the surface like the fingers.

**Traces** (`vr_debug_grasp_trace 3` writes every frame's joints, solves, drift and animation motion to
`grasp_trace.txt`; plotted by `trace.py`):

| Scene | Before | After |
|---|---|---|
| The torch in the off hand, wobbling for 10 s (`tr_torch_before.png`, `tr_torch_after.png`) | 26 solves; the joints moved 186 curl units in all | 0 solves, no motion |
| The torch in the main hand, the same | 26 solves; 211 curl units | 0 solves, no motion |
| Your two-handed take `parry_pose_02-33-29`, the helping hand (`tr_sg2h_cmp.png`) | 51 solves, spread over the whole take; 117 curl units; the fingers jump between two holds twice | 14 solves, all in the first 0.3 s while the hand arrives on the grip, eased; 45 curl units; flat afterwards |
| Your parry, bash and stab takes (`tr_parry_*`, `tr_stab_*`) | | solves only when a grip starts |

### No threads (the solver on the main thread)

`std::async` is gone. The grasp is solved synchronously in `updateGrasp`, so the same inputs always give the same
pose. The three audit bugs cannot recur, because the code they lived in no longer exists: no job to starve, no
result cache to go stale after a settings change (the settings are part of what is compared), and no future to block
at exit.

How it is fast enough (`vr_grasp.cpp`, rewritten):

- **The hand as spheres.** Each finger segment is a row of spheres along its palm side, fitted ring to ring to the
  rig's vertices at the bind pose (`vr_grasp_spheres` prints them). The palm and the ball of the thumb have their own.
- **Conservative advancement.** A finger's joints close together, each step as far as no sphere can reach the held
  thing: the sphere's clearance divided by its lever (the joint rates times its distance from the pivots). When a
  segment touches, the joints before it stop and the rest go on; then a settle step lets it wrap.
- **Per model, once.** The held model's triangles are kept in its own units, with a uniform grid (cells of 2 hand
  units at its size in the hand), normals, planes and boxes. Distance queries use box and plane lower bounds and
  stamps to skip a triangle seen in another cell. A query from outside the triangles' box returns that box's
  distance at once, so a finger far from the held thing closes in a step or two (`12e8fcad`).
- **In the held thing's frame.** The hand is brought into the model's space, never the reverse, so nothing is
  transformed per triangle.
- **No allocation per solve.** Scratch lives in the shape.
- **The placement search** (the palm sliding along a grip through the hand) runs only on the first solve of a grip;
  re-solves keep its place.

See [Costs](#costs-1) for the numbers.

### The trigger finger (#3)

- On its own weapon, the index finger's target is the grasp's stop plus the trigger's pull (`triggerPull`: 0.6, 1.4
  and 1.2 curl units on its three joints at full trigger), so it curls onto the trigger past where it met the weapon.
- The trigger's touch sensor (OpenXR `input/trigger/touch`, on Touch, Touch Plus and Index) is read as
  `HandInput::triggerTouch`. A finger resting on the trigger curls at least half way onto it instead of pointing.
  `vr_mock_fingers`' fifth argument sets it in the mock.
- `trigger_pull.png`: the shotgun, super nailgun and lightning gun with the trigger released, the finger resting on it
  (touch) and the trigger pulled.

### Hotspots and the two-handed pistol grip (#5)

- **Your hotspot 2:** every index (1 to 4) is taken; I tested each on the shotgun's foregrip. What failed was the
  place: a grip beside the holding hand, not ahead of it, failed the two-handed aim's test (the hands' line must run
  along the gun). Such a grip, and the new type **Cup** (3), is now held without aiming. The weapon aims with the
  holding hand; the other hand is drawn on it.
- **Cup:** the helping hand turns with the holding hand. Its grip channel is aligned around the holding hand's
  (coaxial), where the hotspot puts it along it. It wraps the holding hand as well as the weapon: the solver's second
  shape is the other hand as drawn, and `vr_hand_fit_overlap_hands` (0.6 cm) is how far the hands may overlap.
- **New keys per hotspot:** `hsN_pitch/yaw/roll` (the helping hand's turn there, on top of the weapon's fixed-hand
  angles; for a cup, the holding hand's own) and `hsN_style` (0 the thumb wraps around, 1 the thumb along the top).
- **Menu:** Type gains Cup; Hand Pitch/Yaw/Roll; Thumb. "Put It Where the Other Hand Is" keeps a Cup a Cup.
- `vr_weapon_hotspot_here <n> [type] [main|off]` is the menu's "Put It Where the Other Hand Is" from the console.
- `cup_views.png`, `cup_shots.png`: the shotgun held in a cup.

### Wrapping thin things (#8)

Your sword screenshot: the hand holding a blade was placed with its origin on the blade's line, so the blade lay
beside the fingers. Each hand now has a **grip channel** (`grasp::gripChannel`): the line through the centres of the
circles its half-closed fingers curl around. The hand is turned (the least) and moved so that its channel lies on the
blade, and the fingers close around it. It still slides along the blade as before. A cup is placed the same way,
around the holding hand's channel. The per-hotspot Thumb style (above) chooses wrap or thumb on top.
`sword_views_now.png`, `sword_now.png`.

### The thumb (#10)

- The ball of the thumb (the thenar) was blended between its place and the metacarpal's by weight. Under a wide turn
  this pulled the skin in toward the thumb's base, which is the "candy wrapping" you saw. It now turns by its share of
  the metacarpal's turn about the pivot (a slerp).
- The solver no longer tries the widest turns (60° across the palm, 30° up and away); they stretched the skin.
- `thumbs_cmp.png`: the health box, shells box, backpack and armour, before and after.
- The rest of the crudeness is the model: the thumb is one of six old parts, with no skin weights round its base. That
  is #11.

### Carrying: nothing floats off the hand (#9)

- **Cause** (`vr_physics.cpp` `handOn`, `vr_held.cpp` `surfaceFit`): a carriable thing (box, gib, backpack) could be
  taken with the hand anywhere in its model's box, plus 2 units. A big box's model box is mostly air around a gib's or
  a backpack's shape. The fit then pushed the thing out along the palm's normal (up to half a metre) to sit on the
  palm, far from the fist.
- **Fix:** a thing is taken only within `vr_carry_reach` (8 cm) of its drawn surface, measured exactly on its
  triangles (`held::surfaceDistance`). The fit pushes it out at most 15 cm.
- `vr_debug_carry 1` draws, for each hand in a carriable thing's box, that box (green in reach, red not), the nearest
  point of its surface and the reach around the hand.
- `carry_reach.png`: three hand places near a health box and a gib, with the reach test drawn (`vr_debug_carry 1`),
  then gripped. What is taken sits against the hand; the gib is not taken from beyond the reach.

### The free hand and the other hand's weapon (#6)

- The drawn free hand is pushed out of the weapon in the other hand, along the surface's normal where its palm's
  centre or a fingertip is deepest: fully up to `vr_hand_collide` (4 cm), then less and less, none at twice that (it
  gives way and passes through). The push eases over 0.08 s.
- Only the drawn hand moves: aim, melee and grabs use the tracked hand as before.
- `collide.png`: the off hand moving through the shotgun, before and after.

### Inherit From (#7)

- New key `vr_wofs_inherit_NN` (the slot, 1..32; 0 none). A weapon takes that slot's value of every key wherever its
  own value is still the default. Not inherited: the model's name, the key itself and the models' vertex indices.
- **Defaults:** the lava nailguns, the multi grenade and rocket launchers and the plasma gun inherit from the
  nailguns, the grenade and rocket launchers and the lightning gun. Their anchor vertices are within 0.12 units of
  their bases'. I checked each pair side by side in the game (`alts.png`). The swords differ, so they don't inherit.
- **Migration** (`vr_wofs_version` 16): those five slots' own values go back to their defaults once, so that they
  inherit (you had tuned them the same as their bases).
- **Weapon Offsets:**
  - Inherit From: None or any weapon, by name.
  - An inheriting weapon's page edits the settings it inherits (the page title says so).
  - "Stop Inheriting (Copy Them Here)" makes them its own.

### Fingers and walls (#1)

- The hand itself stops at walls; its fingers used to reach past. Each finger's line from its knuckle through its
  joints to its tip is traced against the world (the client's BSP and the moving brush models).
- If a line is blocked, the finger curls (its joints alike) as little as keeps it out, found by halving six times, and
  eases like any curl.
- The cost is three traces per finger per frame when clear. `vr_hand_walls 0` turns it off; `vr_debug_grasp 3` prints
  each bend.
- `walls.png`: an open hand pushed into e1m1's wall and floor, before and after.

### The hand model (#11): not done

A new hand needs a proper thumb with skin weights, better proportions, Quake-style texturing and the blood and powerup
skins redone. That is an asset job of its own, and the solver and the fixes above don't depend on it. The jointed hand
is still fitted to the six old models (`make_hand_rig.py`). If you want it, it is the natural next round for the hands.

### Caches and the game directory

- **`view::viewModel`** caches every lookup, including missing models, until the next map load or game directory
  change (it had looked on disk every frame for a missing one).
- **Routed through that cache:** the torch's `place` and the leg holster.
- **`avatar::usable`** is a small table, so it no longer thrashes between a build's model and its fallback.
- **Caches keyed by a model** (`handrig::checked`, `avatar::info`, the grasp shapes) compare the model's name too.
- **`vr_grasp_dump`:** a hand no longer drawn forgets what it held, so the dump can't read a stale entity.
- **`VR_OnGameDirChanged`** calls `view::resetCaches` (clip sizes, `viewModel`, the jointed hand's check, the grasp
  shapes), `weapons::resetCaches` (the slot cache), `avatar::reset` and `flashlight::onGameDirChanged` (the gun spots
  by name).
- **Tested:** `game rogue`, then `game hipnotic rogue quakevr`, then a map. The hands, the gun and its muzzle are as
  before (`gamedir.png`).

### Determinism

- The coordinator asked: "the same scripted run twice, and two different builds, must give identical hand
  screenshots". Tested with `det.sh`: the two-handed shotgun from two views, the super shotgun and the sword, run
  twice on one build, and once on a build with an unrelated change (a global added to `vr_gadget.cpp`).
- Both hands' final poses in the trace are identical across all three runs.
- In the screenshots, the hands differ in at most 0.4% of their pixels, by at most 25 levels. The exception is the
  shotgun's sight LED, which pulses with time. The rest of the frame differs by the sky, the monsters' animation and
  time-driven lighting (`det_diff.png`).
- The audit's note above ("something in the hand's pose depends on the build") was the old worker thread: a solve
  landed a frame earlier or later depending on timing. With the synchronous solver it can't happen.

### Settings (new)

| Cvar | Default | |
|---|---|---|
| `vr_hand_fit_overlap` | 0.3 | cm the fingers and palm may sink into what they hold. |
| `vr_hand_fit_overlap_hands` | 0.6 | cm a cupping hand may sink into the other. |
| `vr_hand_fit_blend` | 0.12 | Seconds the fingers, thumb and palm take to ease into a new grasp (0: at once). |
| `vr_hand_fit_resolve` | 0.3 | cm (and twice that in degrees) the held thing may move in the hand before a re-solve (0: every frame). |
| `vr_hand_walls` | 1 | Fingers bend out of walls and floors. |
| `vr_hand_collide` | 4 | cm the free hand is held out of the other hand's weapon (0: never). |
| `vr_carry_reach` | 8 | cm from a carriable thing's surface within which a hand takes it (0: its whole box, as before). |
| `vr_debug_carry` | 0 | Draws the carry reach test. |
| `vr_debug_grasp_trace` | 0 | 1 main, 2 off, 3 both: every frame's grasp to `grasp_trace.txt`. |
| `vr_wofs_inherit_NN`, `vr_wofs_hsN_pitch/yaw/roll/style_NN` | | Per weapon, above. |

All but the debug cvars are archived and on **Hand/Gun Calibration > Fingers**: Fit Overlap, Fit Overlap: Hands, Pose
Blend, Refit Threshold, Fingers Stop at Walls, Hands Brush Weapons, Carry Reach (`menu_fingers.png`).

Commands:

- `vr_grasp_bench [n]`: times each hand's solve on what it holds, fresh and again n times (min/median/max).
- `vr_grasp_spheres`: prints the hand's spheres.
- `vr_weapon_hotspot_here <n> [type] [main|off]`.
- `vr_mock_fingers <hand> <trigger> <grip> [<thumb> [<trigger touch>]]`.
- `vr_grasp_dump` now also writes the spheres and, for a cup, the other hand.

### Costs

All costs were measured with `run.sh --exclusive` on vrfiringrange.

The target was well under 0.1 ms per hand per frame. The per-frame update meets it. A single re-solve on some things
does not; the reasons and numbers follow.

**Per frame** (`vr_profile`, 540 frames at 64 fps, the hand wobbling through 15–20° and 5–6 cm for 6 s; the times are
both hands together, per frame; `final/p1_*.csv`):

| Scene | `hand` (both hands' whole update) | of which the jointed hand | grasp solves | walls | brushing | `view entities` |
|---|---|---|---|---|---|---|
| Shotgun, off hand on the foregrip | **0.043 ms** (worst 0.24) | 0.023 | none | 0.008 | <0.001 | 0.060 |
| The same, re-solved every frame (`vr_hand_fit_resolve 0`) | 0.225 ms (worst 0.54) | 0.204 | 0.179 | 0.009 | 0 | 0.242 |
| The six old models (`vr_hand_rig 0`), for reference | 0.023 ms | – | – | – | – | 0.039 |
| A gib in the off hand | **0.031 ms** (worst 0.13) | 0.029 | none | 0.005 | 0 | 0.044 |
| The torch in the off hand | **0.034 ms** (worst 0.17) | 0.033 | none | 0.005 | 0 | 0.049 |

- So each hand's update costs about 0.015–0.022 ms a frame, against 0.012 ms for the six old models.
- In these runs the held things never moved in the hand by more than the refit threshold, so there were no re-solves
  at all.
- The walls' traces cost 0.005–0.008 ms a frame for both hands.
- The worst frames (0.13–0.24 ms) are the first frames after a grip.

**Each solve** (`vr_grasp_bench`, exclusive, the median of 300–500 solves in each of two runs; `final/ab_new_*.txt`):

| Held | Hand | Triangles | First solve (µs) | Re-solve min / median / max (µs) | Probes |
|---|---|---|---|---|---|
| Shotgun | main | 766 | 3012 | 30 / 30 / 106 | 17 |
| Super shotgun | main | 688 | 2635 | 88 / 88 / 163 | 71 |
| Nailgun | main | 480 | 4233 | 32 / 32 / 66 | 17 |
| Super nailgun | main | 726 | 167 | 46 / 47 / 124 | 32 |
| Grenade launcher | main | 386 | 267 | 44 / 44 / 358 | 27 |
| Rocket launcher | main | 499 | 297 | 32 / 33 / 258 | 18 |
| Lightning gun | main | 459 | 417 | 29 / 30 / 152 | 15 |
| Sword | main | 120 | 2984 | 88 / 89 / 177 | 105 |
| Shotgun, two hands | off (helping) | 766 | 2704 | 78 / 84 / 367 | 38 |
| Super shotgun, two hands | off (helping) | 688 | 206 | 19 / 20 / 104 | 17 |
| Shells box | off | 82 | 2512 | 19 / 19 / 79 | 33 |
| Health box | off | 292 | 120 | 46 / 46 / 233 | 48 |
| Backpack | off | 252 | 80 | 23 / 23 / 185 | 40 |
| Gib | off | 28 | 164 | 121 / 127 / 369 | 146 |
| Head | off | 92 | 1994 | 41 / 41 / 138 | 86 |
| Armour | off | 360 | 218 | 115 / 122 / 256 | 145 |
| Torch | off | 608 | 572 | 65 / 67 / 273 | 21 |

(The maxima are single outliers among hundreds of runs, i.e. the OS scheduling. The carried things' figures change a
little from run to run, because the physics puts them in the hand slightly differently each time.)

**Where it doesn't reach 0.1 ms, and why:**

1. **A grip's first solve takes 2–4 ms** (the shotguns, nailgun, sword, shells box and head). These are grips through the
   palm, or things the server pressed into it, and they get the placement search: the palm is tried at up to 28
   places along the grip, each closing all four fingers, to find where the hand holds best. It runs once per grip, when you take the weapon or the thing, and re-solves keep its place. Doing it
   once per grip is the point: that is what keeps the hand from hopping between two holds.
2. **A re-solve on the gib or the armour takes 0.12–0.13 ms.** Two or three fingers there close past the thing
   without touching it, grazing it. Conservative advancement then takes about 60 short steps per finger (146 probes
   in all), because near the surface each step may only be as long as the clearance.
   - The open-air shortcut (`12e8fcad`) only helps fingers far from the thing. It took the head from 71 to 41 µs and
     the torch from 115 to 67 µs.
   - I also tried a one-probe test of a whole free finger's close, bounded by its joints' chain lengths. It never
     succeeded on these things (the bound is too loose that close to them), so I left it out (`final/sweep_ab.txt`).
   - A re-solve happens only when the thing moves in the hand by more than 0.3 cm, so a single frame pays it, not
     every frame.

**Before this pass:** 3–30 ms a solve, on a worker thread.

**Before `12e8fcad`**, measured in the same way, alternating builds (`final/ab_old_*.txt`): the weapons are
unchanged, and the stops are the same within 0.01 of a curl frame. The exception is where the old solver ran out of
steps before a finger reached the fist.

### Tests (mock headset)

Composites in the scratchpad's `hands2/final/`:

- **Recoil:**
  - `recoil_all.png`: the shotgun and super shotgun, still and at four frames of the kick, in round 20, round 21 and
    now (the hand leaves the gun in round 21).
  - `recoil_k2_*.png`, `recoil_k3_*.png`: the same one build at a time; `recoil_n4.png` … `recoil_n8.png`: the other
    guns now.
- **Stability (traces):**
  - `tr_torch_before.png` / `tr_torch_after.png`.
  - `tr_sg2h_cmp.png`: before (the worker thread), attached, final.
  - `tr_parry_*`, `tr_stab_*`, `tr_blade_after.png`: your takes replayed with the trace on.
- **Solver:** `n_guns.png`, each gun from three views (`vr_grasp_dump`), not fitted against fitted.
- **Per note:**
  - `trigger_pull.png`
  - `cup_views.png`, `cup_shots.png`
  - `sword_views_now.png`, `sword_now.png`
  - `thumbs_cmp.png`
  - `carry_reach.png`
  - `collide.png`
  - `alts.png` (each alternate model beside its base)
  - `walls.png`
- **Game directory and determinism:** `gamedir.png`, `det_diff.png`.
- **Menu:** `menu_fingers.png`.
- **Your motion takes:** `vr_motion_eval` over 97 of them (parries with and without a gun, the gun's butt and swing, pommels,
  horizontal slashes, two-handed stabs), run on vr-cleanup's engine as merged (`986c4559`; the QC is the same) and on
  this branch's final build. Both reproduce 91 of 97 live hits. The two tables are identical take by take: the same
  hits, and the same hand error (0.239 units mean). The six that miss, miss the same way on the base (parry bashes
  whose live take recorded nothing, or a slap the replay sees as an overhead blow). The same holds with the torch off.
- **Numbers:**
  - `ab_old_*.txt`, `ab_new_*.txt`, `bench_*.txt`: `vr_grasp_bench`.
  - `p1_*.csv`: `vr_profile`.
  - `eval_sub_base986.csv`, `eval_sub_final*.csv`: the evaluation tables.

### Limitations

- **Two exceptions to the 0.1 ms target:** see [Costs](#costs-1). A grip's first solve takes milliseconds (its
  placement search, once per grip), and on a few things a re-solve is just over 0.1 ms. Re-solves happen only when
  the held thing moves in the hand, never every frame.
- **Cup:** the weapon aims with the holding hand only. The helping hand adds no stability to the aim, as there is no
  second point along the gun to aim with.
- **Walls:** only the fingers bend; the palm already stops at the wall. Thin moving brush models are traced, but
  other entities (monsters, items) are not.
- **The free hand's push** is visual only. Your tracked hand still passes through the weapon for aim, melee and
  grabs.
- **Inherit From:** a value set back to the default is inherited again, because "own" means "differs from the
  default". To pin a value equal to the default on an inheriting weapon, use Stop Inheriting.
- **The thumb** is still one of the old six models' parts (#11).

### In the headset

- [ ] Fire every gun: the hand and arm kick with it; the helping hand stays on the foregrip through the kick.
- [ ] Hold a gun still, then turn your wrist slowly: the fingers don't move. The same for the torch in either hand.
- [ ] Squeeze the trigger slowly: the index finger follows it onto the trigger. Rest the finger on the trigger
      without pulling: it curls halfway.
- [ ] Two-handed pistol grip: on the shotgun, Weapon Offsets > hotspot 2, Type Cup, Put It Where the Other Hand
      Is (holding it there). Then grab it: the hand should cup the main hand. Try Fit Overlap: Hands.
- [ ] The sword by the blade, level across your face: the fingers around the blade.
- [ ] The health box: does the thumb still look wrong?
- [ ] Grab a box, gib or backpack from the edge of its reach: it sits in the hand. Carry Reach sets how close.
- [ ] Pass the off hand through the gun: it resists a few cm, then goes through. Hands Brush Weapons sets how far.
- [ ] Weapon Offsets on a lava nailgun: it says it inherits from the nailgun; changing a value there changes both.
- [ ] Open hand into a wall or the floor: the fingers bend, not through.
- [ ] Pose Blend and Refit Threshold: too soft or too twitchy? The defaults are 0.12 s and 0.3 cm.

## Fitted hands, third pass (tuning)

Your eight notes after testing the second pass, all on the Weapon Offsets page. Branch `agent/handtune`, one commit
each (the merge of vr-cleanup between them). Composites and numbers are in the scratchpad's `handtune/final/`.

| # | Your note | What you get | Commit |
|---|---|---|---|
| 1 | Sliders that move the hand and the weapon together | **Hand and Weapon Together** X/Y/Z, Pitch/Yaw/Roll | `0bdb70c2` |
| 2 | Sliders for the hand alone (the bent wrist) | **Hand Only** X/Y/Z, Pitch/Yaw/Roll | `0bdb70c2`, `88d2b828` |
| 2 | See where the controller is | **Show Controller** | `0bdb70c2` |
| 3 | An overlap slider per weapon and per hotspot | **Overlap** (weapon), **Overlap There** (hotspot), 0..1 | `0bdb70c2` |
| 4 | The Cup hotspot far from where the hand ends up | A cup is now the palm's place: taken there, drawn there; your cups moved once | `0bdb70c2` |
| 5 | Offsets for the hand once it holds a hotspot | **Held Hand** X/Y/Z, Pitch/Yaw/Roll per hotspot | `0bdb70c2`, `88d2b828` |
| 6 | A laser to check the aim | **Show Controller Laser**: controller, shot and barrel lines | `0bdb70c2` |
| 7 | The super nailgun grabbed on its edge, sometimes | The same grip every time, for every weapon and hotspot | `e783bda8` |
| 8 | Turn the automatic fingers off for some weapons | **Fingers: Automatic / Manual**, per weapon and per hotspot | `c309350d` |

### The Weapon Offsets page

The sections, top to bottom:

1. **Inherit From** (if the weapon inherits, the page edits what it inherits).
2. **Weapon in the Hand:** Offset, Pitch/Yaw/Roll, Scale, Hide Hand. Unchanged.
3. **Tuning Aids:** Show Controller, Show Controller Laser.
4. **Hand and Weapon Together.**
5. **Hand Only.**
6. **Fingers on the Weapon:** Fingers (Automatic/Manual); with Manual, the five curls and Thumb Across; with Automatic,
   Overlap. Then the five finger tweaks and Thumb X/Y/Z, as before.
7. **Muzzle.**
8. **Other Hand's Grips (Hotspots).** Per hotspot:
   - Type, X/Y/Z (for a cup, the palm's place), Put It Where the Other Hand Is.
   - Hand Pitch/Yaw/Roll and Thumb (as before).
   - Fingers There (Automatic/Manual, and the curls), Overlap There.
   - Held Hand X/Y/Z, Pitch/Yaw/Roll.
   - Bias, Remove, Show Hotspots.
9. **Two-Handed Aim**, **Ammo Screen**, **This Weapon** (Print Changes to Console, Reset This Weapon): as before.

The positional and angle sliders go past their bar's ends (vr-cleanup's `.extend()`). The curls and the overlaps stay
within 0..1.

All the new keys are per weapon (`vr_wofs_<key>_NN`):

- They go through Inherit From.
- Print Changes to Console lists them.
- Reset This Weapon resets them.
- Their defaults are 0 in `vr_weapons.inc` (not written there, as for every key), except the overlaps, which default
  to 0.3: today's fit.
- The off hand mirrors them, as it does the weapon's own offsets.

`vr_savedefaults` writes the global settings: the new aids are debug views and are not saved.

| Keys | Meaning |
|---|---|
| `whole_x/y/z`, `whole_pitch/yaw/roll` | Hand and Weapon Together |
| `hand_only_x/y/z`, `hand_only_pitch/yaw/roll` | Hand Only |
| `overlap` | the weapon hand's overlap, 0..1 of a centimetre |
| `fgr_manual`, `fgr_curl_thumb/index/middle/ring/pinky`, `fgr_thumb_across` | Fingers: Manual |
| `hsN_overlap`, `hsN_vx/vy/vz`, `hsN_vpitch/vyaw/vroll` | per hotspot: Overlap There, Held Hand |
| `hsN_manual`, `hsN_curl_*`, `hsN_thumb_across` | per hotspot: Fingers There |

### Hand and Weapon Together (#1)

- The offset is applied to the hand as tracked and calibrated, before anything else sees it (`vr_hands.cpp`). The
  weapon is placed from that hand, so the muzzle, the aim and the melee points all move with it.
- **The frame** is the controller's aim frame, including Gun Angle: X forward along the aim, Y left, Z up. Pitch
  lifts the muzzle, yaw turns it left, roll rolls it. The turn is about the controller's point.
- **For the axe:** move the hand and the axe until the drawn hand is back on the controller. Show Controller draws
  both points and a yellow line between them.
- **The palm fit** is worked out between the hand and the weapon, so this offset doesn't change it: hand and weapon
  move as one.
- **Exact:**
  - Moving by (4, 2, −1.5) moves the muzzle by the offset turned into the aim frame, to 0.00006 units.
  - Turning by (10, 8, 15) turns the aim by exactly that rotation (error 1e-6), and the muzzle about the controller
    to 0.0001 units.
  - Why the weapon had to change for this: its own Pitch/Yaw/Roll are Euler angles added to the hand's, so a turned
    hand turns it slightly differently (0.07 units at the shotgun's muzzle). The weapon is now posed from the hand's
    angles before this turn, then turned rigidly by it (`hands::State::wholeTurn`).

### Hand Only (#2, the bent wrist)

- The drawn hand alone moves and turns on the weapon. The frame is the one the weapon carries (the aim frame at
  rest), and the turn is about the palm's middle.
- The weapon, its muzzle and its aim stay put: the muzzle moved 0.00000 units in every test. The fingers wrap the
  grip again where the hand now is.
- For the bent wrist in your shotgun screenshot, Hand Pitch or Hand Roll turns the hand without touching the gun. The
  forearm follows the drawn hand.
- The palm is fitted at the place without this offset, and the offset then moves the fitted hand (`88d2b828`). The
  first version searched the fit again at the new place, which pulled the hand back towards the best grip: a
  2.45-unit offset moved it 1.7. Now (2, −1, 1) moves the palm 2.450 units (the offset's length is 2.449).

### Show Controller and Show Controller Laser (#2, #6)

- **Show Controller:**
  - Each controller as tracked: its grip pose before any offset or IK.
  - Drawn as a translucent handle (3 × 3.5 × 11 cm along the grip), a ring over its front, and its axes (red forward,
    green left, blue up).
  - Where the hand's own point differs (moved by Hand and Weapon Together), a yellow point joined to it.
  - Drawn through the hands, so it shows even inside the fist.
- **Show Controller Laser**, for a weapon held in either hand:
  - **White**, from the controller along where it points (Gun Angle included, before the weapon's own offsets).
  - **Red**, from the muzzle, where the shots go.
  - **Green**, from the muzzle, along the barrel as drawn (the model's forward axis).
  - Turn the weapon (its Pitch and Yaw) until green runs along red.
  - Each laser ends at the wall it meets, with a dot.
- Both are `vr_show_controller` and `vr_show_controller_laser`, not saved.

### Overlap (#3)

- **Overlap** (the weapon hand) and **Overlap There** (each hotspot) run 0..1:
  - 0: the fingers and palm stop on the surface.
  - 1: they sink in by up to a centimetre (`weapons::maxOverlapCm`).
- The default, 0.3, is the fit you had: the global `vr_hand_fit_overlap` was 0.3 cm.
- That global now covers held things only (boxes, gibs, the torch) and is labelled "Fit Overlap: Things".
- Between the two hands on a cup, it is still "Fit Overlap: Hands".
- A change solves the grasp again once, from scratch, so it can't jitter.

### The Cup hotspot is where the palm sits (#4)

- **The problem:** a cup's point used to be where the helping hand's grip channel was aligned round the holding
  hand's, at the point's place along it. The hand ended up far from the point (your white ball), and the hand was
  grabbed by distance from the controller to that point.
- **Now:**
  - **Placement:** a cup's X/Y/Z is where the helping hand's palm (its middle) sits. The hand is placed so, turned as
    the holding hand is plus the hotspot's Hand Pitch/Yaw/Roll.
  - **The grab:** by the distance from the free hand's palm to that point (`hands::State::palmLocal`, in
    `vr_twohand.cpp` and the grip choice). The grab zone and the result coincide.
  - **Putting one there:** "Put It Where the Other Hand Is" (and `vr_weapon_hotspot_here <n> 3`) stores the palm's
    place.
- **Your cups** are moved once (`vr_wofs_version` 18):
  - The first time the weapon is held with both hands drawn, each cup's old result is worked out from its old point
    and the holding hand's channel. The palm's place and the hand's turn are stored as the cup's X/Y/Z and Hand
    Pitch/Yaw/Roll. The console says so.
  - **Tested:** a round-21 cup beside the shotgun's grip, drawn by the base build and by this one after the move. The
    weapon and the other hand, seen from the helping hand, are where they were to 0.0002 units.
  - **The fingers differ,** because they were never reproducible. The base build closes them differently depending on
    how the hand arrived: two approaches gave fingers up to 7 units apart. This build gives the same fingers every
    time, within the base build's own spread.
- **The white ball** of Show Hotspots is now at the cupped hand's palm (`cup_before_after.png`).

### A hotspot's Held Hand offset (#5)

- X/Y/Z and Pitch/Yaw/Roll move the helping hand's drawn pose once it holds the hotspot. They ease in with the grip.
- The frame is the holding hand's aim frame, carried by the weapon; the turn is about the palm.
- Visual only: where the hotspot is taken, the two-handed aim and the melee use the tracked hand.
- Use it where the hands overlap too much on a cup. As with Hand Only, the palm is fitted without it, and it then moves
  the fitted hand.

### The same grip every time (#7)

- **Measured:** ten takes of the super nailgun, switching to it from the axe with the hand at ten angles and moving.
  - Two grips came out, by turns: the palm moved 3 cm and turned 20° onto the frame's edge (your screenshot), or it
    didn't move.
  - Ten takes of the shotgun's foregrip gave ten grips, their palms up to 1.3 units apart.
- **Three causes, three fixes:**
  1. **Where the grasp was solved.**
     - **Cause:** it was solved from where the weapon was drawn in the hand this frame. That differs from its
        intended place by float noise (a hundredth of a unit) and, while the hand arrives, by more. The palm's turn
        towards "the surface its normal meets first" and the thumb's choice flip on such hairs.
     - **Fix:** a weapon in its hand, and the other hand on a grip or cup hotspot, are solved at their place as the
        settings define it (`Held::canonical`: worked out with the holding hand at the origin, unturned).
  2. **Warm solves.**
     - **Cause:** a solve started from the one before kept what it could of it, so the grip depended on the way in.
     - **Fix:** once what the hand holds rests in it, the grasp is solved once more from scratch.
  3. **The palm's turn.**
     - **Cause:** the turn towards the first surface picked the super nailgun's frame.
     - **Fix:** a weapon's grip has its palm's place searched within Palm Fit: Weapons (along the grip and the fingers,
        flush on it) for where the fingers hold best, and never turned. Cups keep their old fit, so migrated cups look
        as they did.
- **Result:** ten takes of every weapon (shotgun, super shotgun, nailgun, super nailgun, grenade and rocket launchers,
  lightning gun, sword, axe) from ten hand angles, and ten of the shotgun's foregrip. Every hand vertex was the same to
  0.0000 units across the ten takes (`grabs_consistency.txt`).
- **Better grips on the way** (`guns_before_after.png`): the grenade launcher, rocket launcher, lightning gun and axe
  now wrap their grips. Before, their palm was turned 20° off them.
- **The super nailgun** now always takes the middle, inside the frame. Its handle, as the weapon is placed in the
  hand, lies through the fingers: they are drawn at the controller's curl there, since no curl clears it. Hand Only or
  Fingers: Manual set it as you want it.
- **Cost:** one solve from scratch when a weapon comes to rest in the hand: 2–3 ms for a grip searched along the
  palm, once per take. Nothing per frame after that (see Costs).

### Fingers: Automatic / Manual (#8)

- **Where:** per weapon (Fingers on the Weapon > Fingers) and per hotspot (Fingers There).
- **What Manual does:**
  - The grasp isn't solved. Each finger stops at its curl (0 open, 1 a fist) the way a solved finger stops on what it
    holds, so the controller's grip still opens the hand and closes it up to your pose.
  - Thumb Across turns the thumb over the palm (up to 45°).
  - Thumb X/Y/Z and the five finger tweaks still apply.
  - On the weapon's own hand, the index finger still pulls with the trigger.
  - There is no palm fit: the hand is where the weapon's offsets put it.
- It goes through Inherit From, Print Changes and Reset, as every key does (`manual_fingers.png`).

### Tests (mock headset; `handtune/final/`)

- **`offsets_*.png`, `offsets.txt`:** the shotgun held level. From the side and behind: at 0, moved together, turned
  together, hand only moved, hand only turned, with the controller and the lasers.
- **`cup_before_after.png`:** your kind of cup, base build against this one after the move.
- **`sng_grabs_before_after.png`**, **`guns_before_after.png`**, **`grabs_consistency.txt`**.
- **`manual_fingers.png`:** the shotgun automatic, manual, manual with the trigger pulled; the foregrip automatic and
  manual.
- **`overlap.png`:** the shotgun at Overlap 0, 0.3 and 1.
- **`menu_*.png`:** the page's sections.
- **Motion takes:**
  - All 474 of your takes, replayed (`vr_motion_eval`) on vr-cleanup's engine (`5c89cec1`) and on this branch's, with
    every new slider at 0.
  - The tables are identical, take by take: the same events, verdicts
    and hand errors.
  - On both, 405 of 474 pass by `expect.cfg` and 164 reproduce the hits recorded live. The rest were recorded before
    the melee redesign, so their live hits came from the old melee code.
- **The hands in general:**
  - Muzzles unchanged with every slider at 0.
  - The second pass's recoil, walls and brushing are untouched: the offsets act before them.

### Costs

Measured with `run.sh --exclusive` and `vr_profile`: 540 frames at 64 fps each, the main hand wobbling. The times
are both hands together, per frame (`final/p3_*.csv`).

| Scene | `hand` | of which `rig hand` | Second pass |
|---|---|---|---|
| Shotgun, the off hand on the foregrip | 0.046 ms | 0.020 | 0.043 / 0.023 |
| The same with every offset set (together, hand only, held hand) | 0.052 ms | 0.023 | – |
| The same, re-solved every frame (`vr_hand_fit_resolve 0`) | 0.110 ms | 0.083 | 0.225 / 0.204 |
| A gib in the off hand | 0.039 ms | 0.037 | 0.031 |
| The torch in the off hand | 0.039 ms | 0.037 | 0.034 |

- **Per frame:** working out the grips' places from the settings adds 0.003 ms to both hands. The offsets add 0.006
  ms.
- **Every frame forced to re-solve** (`vr_hand_fit_resolve 0`) costs half what it did: a weapon's place in the hand no
  longer changes with float noise, so there is nothing to solve again.
- **Per take:** a weapon, or a grip hotspot, taken is solved from scratch once it rests, with its palm's place
  searched: 2–3 ms, once. The Hand Only or Held Hand fit adds one more solve when those sliders change.

### Limitations

- **The super nailgun's handle** lies through the fingers at its placement (above). The weapon's offsets, Hand Only
  or Manual fix it; I didn't change your placement.
- **Manual fingers** have no palm fit. A weapon whose grip needed the fit (most guns move the palm 2.6–3 cm) shows the
  hand where the offsets put it, so Hand Only goes with Manual.
- **Held Hand and Hand Only move the drawn hand only.** The tracked hand, and the melee and grabs that use it, stay
  where the controller is. Hand and Weapon Together moves both.
- **Show Controller** draws the grip pose, not the controller's model. Its handle box is the size of a Touch
  controller's grip.

### In the headset

- [ ] The axe: Show Controller, then Hand and Weapon Together until the drawn hand sits on the controller box. The
      axe's head and swings should follow.
- [ ] The shotgun's bent wrist: Hand Only Pitch/Roll a few degrees; the gun must not move (the red laser stays put).
- [ ] Show Controller Laser: turn a gun's Pitch/Yaw until green runs along red. Does white (your controller) point
      where you expect?
- [ ] Overlap at 0 and 1 on a gun and on the foregrip.
- [ ] Your cup: it should be where your hand ends up (Show Hotspots); grab it by putting your palm there. The Held
      Hand sliders pull the hands apart.
- [ ] The super nailgun: take it ten times; the same grip each time. If the handle through the fingers bothers you,
      try Hand Only or Fingers: Manual.
- [ ] Fingers: Manual on a weapon you don't like the fit on; does the trigger finger still pull?

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

**Stored.** Hanging lens down on the belt (see "Stored on the belt, worn on the head" below; at first it pointed
forward from the chest, then hung on the chest).

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

### Stored on the belt, worn on the head (your two later flashlight notes)

"The flashlight should be stored vertically on the body, not horizontally, encouraging the player to attach it on a
weapon or hold it in the off-hand. It should also be possible to attach the flashlight on top of the player's head,
with the same controls as for the guns."

The torch now lives in three places: the belt (stored), a gun, and the head. You also hold it in either hand.

**Stored on the belt** (`mountPose`):
- It hangs straight down from a clip on the belt on the off hand's side, between the buckle and the hip holster: 10 cm
  above the pelvis joint, 9 cm to the side, its tube 2.2 cm in front of the belt, the torch over the hip.
- The lens is at the bottom, the switch faces out. The lens leans out from the body by **Lean Out**
  (`vr_flashlight_tilt`, 8°; the slider was Tilt Down).
- Switched on there, it lights only the floor at your feet. That is on purpose: you take it, clip it on a gun, or
  put it on your head.
- Forward/Up/Out still move the clip.

**Why the belt, not the chest.** The first vertical clip was under the collarbone, where the off hand rests in a
boxing guard. Replaying your recorded takes, a fist clenched in the guard switched the torch on and took it, and the
punch that followed was no longer a fist: 4 off hand straight punches and 1 pommel strike lost their hits.

I checked candidate places against all 474 of your takes: where a hand pressed its grip or trigger within reach of the
torch.

| Place | Takes with such a press |
|---|---|
| The chest (that clip) | 7 |
| High on the shoulder strap | 3 (it also sits on the upper holster) |
| The ribs | 0 |
| The belt | 0 (fewest takes with a hand near it at all: 33, the ribs 78) |

The belt is out of the guard, the gadget and the upper holsters. It is also where a torch is carried. The hip holster
is 12 cm further out; where their reaches overlap, the nearer one wins (below).

**Deliberate presses only.** Taking or switching the stored torch, and taking it off the head or a gun, now needs
intent, not a fist that happens to close next to it:
- **An open hand:** the grip or trigger must be pressed from an open hand, under 0.3 for at least 0.15 s. A slow
  squeeze counts if it began within 0.6 s. B/Y has no analog value, so it needs only the still hand.
- **A still hand:** under 1 m/s (a punch is 2.75 and up), and so for the last 0.15 s. A hand that jumps (a teleport,
  tracking regained) also counts as moving.
- **The game wins** for a hand at the other hand's weapon (holding it with both: a two-handed sword held low reaches
  the belt), at a two-handed grip or a weapon hand-off hotspot, or nearer a holster whose reach it is in (a draw).
- A press it ignores goes to the game. `developer 1` prints "torch press ignored: ..." for each.
- Held in the hand, the torch's own presses (the switch, B/Y to flip, clip on a gun or the head) are unchanged:
  holding it is intent enough.

**On the head** (`headPose`, mode `OnHead`). The head is where a head torch is most useful: it lights wherever you
look, and both hands stay free for a gun in each hand, climbing, or carrying.
- **To put it on**, hold the torch (either hand) against a temple or the forehead. Within 10 cm of the temple on that
  side, you get a tap and the lamp brightens, as by a gun. Then press B or Y (either hand's).
  - It clips on at that side's temple with the gun clamp's click and a buzz in the hand.
  - The overhead grip is held further out, so B/Y there still turns the torch round.
- **Where it sits:**
  - The lens is level with your eyes, 3.5 cm above them and 8.5 cm out to the side. The tube runs back along the side
    of your head.
  - The beam goes where the head looks, crossing your line of sight 4 m ahead.
  - The light comes from beside the eyes, not from them, so shadows still read.
  - Nothing of it is ever in view: it is behind the eye plane and out at the side.
  - The cord isn't drawn there (it runs behind the neck).
  - **On Head Forward/Up/Out** (`vr_flashlight_head_*`) move it.
- **To take it off**, put a hand at it:
  - B or Y sends it back to the belt on its cord;
  - B or Y while gripping it, or the grip alone, takes it into that hand;
  - either way with the clamp's detach click.
- **Voice notes:** the off hand at the head torch uses Y for the torch, not a voice note.

**Death, level changes, fresh starts:**
- On death, at the intermission and on any map change, the torch goes back to the belt from the head, a gun or a
  hand, switched on or off as it was.
- A fresh start (the map command, New Game, a loaded save) switches it off, on the belt.
- A suicide (`kill`) is not a death as the client sees it: the respawn is at once, so the torch stays on the head.

**Tested** (mock, e1m1; composite `round21_flash2/flashlight_storage_head.png`):
- the chest storage before and after, from your view (off, then on: only the floor at your feet lit) and in the body
  preview from the front and the side;
- the head mount from your view: looking ahead, turned right, and looking down-left, the beam following the head
  with nothing of the torch in view. Also in the body preview facing you, the lens at the temple;
- the off hand: taken, to the left temple, Y (on the head), Y at it (back to the chest);
- the main hand: taken, to the right temple, B (on the head), gripped off into the hand, let go (back to the chest);
- the overhead grip by the head still flips, not attaches;
- `changelevel`: back on the chest, still on. `map`: off.

**The belt and the intent gate, tested** (mock; composite `round21_flash3/flashlight_belt.png`):
- the belt in the body preview (front, side), and from your view looking down: the chest hides it, as it hides the
  hip holsters, so you find it by reach. A tap and the lamp brightening tell you your hand is there;
- the off hand taking it and switching it from an open hand;
- ignored: a teleported hand pressing at once ("moving"), and a grip released and pressed again within a frame ("not
  from an open hand");
- the axe drawn from the left hip holster next to it (the holster nearer), not the torch;
- clipped on the shotgun in the other hand and taken off with Y;
- `vr_motion_eval` over all 474 takes, `developer 1`: **no "flashlight:" line at all** (nothing switched or taken, no
  press even ignored near it). punch_straight 02-18-24, 02-18-27, 02-18-35, 02-18-37 and hilt_pommel 02-44-08 pass;
  totals 423 pass, 51 fail (the same verdicts, take for take, as a run before the last gate fix). That run had the
  belt but not the two-handed rule: the two stab_two_hands takes (02-47-29, 02-47-32) had the off hand's grip, going
  down the sword's grip to its pommel, take the torch. The rule for a hand at the other hand's weapon fixed both.
- Limitation: in the body preview the holstered axe's head hangs over the torch (both by the left hip). From your own
  view the chest hides both.

### Your notes after testing it: the overhead grip, sliders, colour, the spin, the cord

- **The overhead grip went through the hand** (your screenshot). The flip turned the torch over about the middle of its
  grip, which put its wider head (3.9 cm across, the tube 2.6) in your ring and little fingers. The hand's grasp solver
  then pushed the drawn hand 3 cm off it, towards the palm, so the tube came out of the back of the hand.
  - Each grip now has its own place in the hand. By default both sit 0.5 cm towards the back of the hand, and the
    overhead one 2 cm further up the fist, so the head clears the fingers.
  - The solver now moves the palm 0 cm in the low grip and 0.7-2 cm in the overhead one (3 cm before).
  - Your own view of the overhead grip, before and after, is in the composite: the tube now goes through the fist in
    both hands.
- **Sliders**, Flashlight page, "In the Hand: Low Grip" and "In the Hand: Overhead Grip", six each:
  - Forward, Towards Palm and Up, ±30 cm;
  - Pitch, Yaw and Roll, ±180°, turned about the grip's middle.

  They are added on top of In Hand Forward/Up. The other hand gets the mirror image.
  - Positive Pitch tilts the beam up, in either grip. With the thumb in front and the pitch too high, lower Low
    Grip Pitch.
  - Cvars: `vr_flashlight_low_x/y/z/pitch/yaw/roll`, `vr_flashlight_high_*`.
- **Beam colour:** Beam Hue (`vr_flashlight_hue`, 40; the leftmost step follows the Player Effects Hue) and Beam
  Saturation (`vr_flashlight_saturation`, 0).
  - At saturation 0 it is white, the new default. It was a fixed warm white before; Hue 40 with Saturation 0.2
    gives that back.
  - It colours the light, the beam in the air and the lens. The lens is now a glowing disc drawn in the beam's colour,
    not the skin's fixed yellow.
- **The flip spins:**
  - B/Y turns the torch over in 0.25 s, eased, about the knuckles' way across the fist. The beam follows as it
    turns.
  - A second press during the spin turns it back from where it is.
  - The hand keeps its old grasp through the spin and is fitted once to the new grip at the end, blending in over
    the fitted hands' 0.12 s. The grasp trace shows the palm moving at most 0.19 cm a frame, a smooth slide, with no
    jump.
- **Wider sliders** (you couldn't go far enough):
  - the belt's Forward/Out ±30 cm and Up ±40;
  - In Hand ±30;
  - On Gun Forward -40..30 cm and Up/Out ±30;
  - On Head ±30;
  - Lean Out ±90°.
- **Cord** (`vr_flashlight_cord`, on) turns the cord from the belt to the torch off.
- Also fixed: with no body drawn, your off hand at the belt torch sat in the empty main hand's two-handed hotspot, and
  the torch wouldn't come.

Tested (mock, e1m1; composite `round21_flash4/flashlight_grips_r2.png`, trace `round21_flash4/palm_trace.png`):
- both grips in both hands, close up from four sides;
- your view of the overhead grip in four poses, before and after, each hand;
- the spin frame by frame;
- the beam in white and four colours on a wall, and the lens in blue;
- the palm through three flips in each hand;
- the per-grip Pitch: +20 tilts the beam up 20° in both grips;
- the Cord toggle.

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
  - [ ] the overhead grip in either hand: the tube in the fist, not through the back of the hand. If it still looks
        off, tune the In the Hand sliders, and tell me the values you end up with;
  - [ ] the flip spins, and the hand stays put;
  - [ ] Beam Hue and Saturation on the Flashlight page: white by default.
- [ ] Cord off: no cord from the belt.
- [ ] On the belt (off hand side): hanging lens down, easy to find and grip with either hand? Clear of the hip
      holster (draw the gun there: the gun, not the torch)? (Forward/Up/Out and Lean Out move it.)
- [ ] Fight with your fists in a guard, and clench them by your belt: the torch is never taken or switched. An open
      hand at the torch, then the grip: taken.
- [ ] On the head, with either hand:
  - [ ] hold the torch at a temple: a tap, then B/Y puts it on your head. Look around: the beam follows, the
        shadows read, nothing in view;
  - [ ] take it off: B/Y at it (back to the belt), or grip it (into the hand).
- [ ] Clip it on each gun: parallel to the barrel, under it (beside the super nailgun and the grenade launcher). Say
      which gun looks off, and use On Gun Forward/Up/Out.
- [ ] Off hand holding the torch at your mouth: Y flips it rather than recording. Is that the right priority?

Not verified: the haptic pulses and the stereo position of the chime were only logged in the mock (it has no
haptics or ears), and the counter's legibility at the Quest 3's resolution was not checked (the mock is 960 × 540).

## C++ audit fixes

The fixes chosen from the C++ audit, one commit each (or a small group). Measured with `run.sh --exclusive` and
`vr_profile`; images compared before and after in the mock (scripts and composites in the scratchpad's `perf/`).

### What changed

| # | Where | Fix |
|---|---|---|
| 1 | `vr_upscale.cpp` | FSR's 60 KB shader source is put together only when its program is compiled (`fsrProgram`), not at each call (up to six a frame). FSR, NIS and RCAS-alone images identical. |
| 2 | `docs/INSTALL.md` | The Visual C++ Redistributable needed: 14.44 or later (built with VS 2022 17.14, toolset 14.44.35207; older than 14.40 crashes at start: the 17.10 STL's constexpr `std::mutex`), how to check it, `aka.ms/vs/17/release/vc_redist.x64.exe`; a future installer will check it. |
| 3 | `vr_move.cpp`, `vr_server.cpp`, `vr_physics.cpp`, `vr_climb.cpp` | `readVrMove` reads the whole block and drops the move if any float is not finite (the previous move stands). The "reject if x > limit" tests are written `!(x <= limit)`: the teleport range, a hand crossing the surface (dt, speed, the needed speed), the swim's glitch and still tests, the compat muzzle shift, climbing's moved-away and hand-at-the-body tests. `sendSplash` refuses non-finite figures (they become integers in the message); the roomscale move ignores a non-finite one; the swim keeps SV_WaterMove's velocity if its sum isn't finite. |
| 4 | `vr_emissive.cpp`, `vr_decals.cpp`, `vr_fgfx.cpp` | Hashes in unsigned arithmetic: the torch seed, the decal atlas's noise lattice, the tendrils' noise keys (the only other signed ones found). |
| 5 | `vr_water.cpp` | `gridRises`: a flat power-of-two table, linear probing, slots stamped with the view (a stamp of our own: `r_framecount` restarts at each map), at most half full, doubling when needed: no clearing, no allocation once grown. `gridPins`: a flat table built once a map. |
| 6 | `vr_decals.cpp`, `vr_gore.cpp`, `vr_ring.hpp` | The decals in a `Ring` (a vector used as a FIFO ring) of `vr_decal_max` slots, made that size again when the cvar changes (at the next mark, as the deque was trimmed then); a mark goes into the next slot, its triangles into that slot's buffer. The clip buffers and corners are static scratch. The drops' quarter: a running count, the oldest drop removed by swapping the older marks up (their order and buffers kept). Gore's rays: a fixed ring of 320. |
| 7 | `common.c`, `vr_api.h`, `vr_gamedir.cpp` and the modules | `VR_OnGameDirChanged` (COM_SwitchGame, after `Mod_ResetAll` and the renderer's reload) empties the caches listed below. |
| 8 | `vr_menu.cpp` | `change()` copies the action's page and function out of the `Item` before running it. |
| 9 | `vr_climb.cpp` | `climbers` is a fixed array of `MAX_SCOREBOARD`, as the swimmers'. |
| 10 | headers | `inline constexpr` for the header-scope constants and tables: `vr_handrig_data.inc` (and `make_hand_rig.py`, which writes it), `vr_handrig.hpp`, `vr_gore.hpp`, `vr_units.hpp`, `vr_motion_take.hpp`. The keyword only. |
| 11 | `vr_gadget.cpp/.hpp`, `vr_text3d.cpp`, `vr_gfx_gl.cpp`, `vr_ao.cpp` | No per-frame string allocations: the gadget's wrapped lines in a reused pool, the hologram's key, the notify line, the messages copied into the elements already there, `VR_GameLineOnWrist`'s copy (all on the main thread, one call at a time). The wrist log's lines are views of the gadget's pool (`gadget::Log::lines` is a `std::vector<std::string_view>`, valid until the next `log()`). `text3d::queue` assigns into kept strings; a glyph's vertices are written in place. `pics` and the AO bake maps use a transparent hash. The AO candidates' order is a kept vector. |
| 12 | `vr_ao.cpp` | `groupOf`: a flat array of at most 64 (entity, group), searched in order. |
| 13 | `vr_particles.cpp` | The pool is reserved at startup to its cap: 32768 particles of 128 bytes, 4 MB. |
| 14 | `vr_physics.cpp`, `vr_progs.cpp` | The water sounds are a table (`WaterSound`), precached from C++ at each map's start with Quake VR's progs (`VR_OnSpawnServerBeforeLoad`; world.qc still names them too), played by precache index; the random choice as before. |
| 15 | `vr_ambient.cpp`, `vr_modellight.cpp`, `vr_evict.hpp` | Eviction at most once a frame, and only when something can be old enough (a scan notes the oldest entry it keeps). |
| 16 | `vr_decals.cpp`, `vr_gfx.hpp`, `vr_gfx_gl.cpp` | `gfx::StaticTriangles`: triangles in their own vertex buffer, uploaded (orphaned and refilled) only when they change. The settled decals are drawn from it in both eyes. `vr_decal_count` prints the uploads. |
| 17 | `vr_flashlight.cpp`, `vr_cvars.inc`, `vr_menu.cpp` | `vr_flashlight_beam_quality` (Flashlight > Beam Quality): 2 high = 16 sides x 9 rings every frame (as before, bit for bit); 1 medium (the default) = 8 x 9, the other sides the mean of their neighbours; 0 low = 8 x 5, the rings between too. Medium and low keep a trace while its line moved less than 1% of its length (at least 0.1 unit), for at most 0.1 s. |

### Measurements

| Item | Scene | Before | After |
|---|---|---|---|
| 17, the beam (`flashlight beam` scope) | e1m1's corridor, the torch held along the floor into the wall; still / moving | 0.018 / 0.017 ms | high 0.018 / 0.017; **medium 0.002 / 0.010**; low 0.002 / 0.006 ms |
| 17 | e1m2's dark room by the water (chest torch) | 0.049 ms average, 0.96 ms worst frame | not measured after |
| 12, `groupOf` (`vr_ao_show`) | firing range, ten grunts, 31 occluders of 73 | choosing them 0.015 ms a frame | 0.013 ms: the map's clear and refill cost about **2 us a frame** |
| 5, `gridRise` | e1m2's pool, a shotgun blast every 27 frames | `particle verts` 0.037 / 0.034 ms (the eyes) | 0.041 / 0.028 ms: within the noise at this load |
| 5, standalone (the same code, 14 lookups per crossing) | 100 / 400 / 1500 crossings a view | 6.1 / 42 / 196 us a view | 3.2 / 16 / 83 us |
| 16, decal uploads | the decal scene's 441 settled vertices; the cap scenes' 102-201 | 34.5 KB a frame (both eyes); 8-16 KB | uploaded only when they change: 33 uploads (325 KB in all) over the whole decal scene |
| 6 | firing range, 118-187 marks | `decal verts` 0.020 ms, `decals` 0.028 | 0.021, 0.026 ms (the same work; no allocation per mark) |
| 15, eviction (standalone, the same maps and test) | 1000 / 2100 / 3000 / 5000 entities drawn a frame, 30 short-lived a frame | 12 / 35 / 60 / 140 ms a frame | 0.015 / 0.023 / 0.028 / 0.042 ms, the same entries kept |

The per-frame costs of items 1, 5, 6, 11, 12 and 16 were already small (tens of microseconds or less): these fixes take
away allocations, upload bytes and worst cases rather than visible time in these scenes. Item 15's worst case was a
real hitch: past 2048 cached entities with none old enough, every entity drawn scanned the whole map.

### Caches emptied on a game directory change

`vr_ao`: the baked occlusion (by model name), the brush models' drawable flags (by model), the lit submodels, the
frame's occluders and groups; `vr_anchor`: the models' strip orders (by model pointer; its name check didn't catch
another game's model of the same name); `vr_detail`: detail.cfg's kinds and rules, the detail array, the textures'
details; `vr_emissive`: the torches; `vr_ambient`, `vr_modellight`: the entities' cached light and the map's lights;
`vr_gfx_gl`: the gfx.wad pictures by name (their pointers were stale after `Draw_NewGame`); `vr_bodyblood`: its drops.
Per-map state is emptied at the next map as before (`VR_NewMap`'s generation, `VR_OnClientClearState`), and the water
mesh is rebuilt at each map load. Tested: `game rogue`, then `game hipnotic rogue quakevr`, a map after each. The
hands' caches (`vr_view.cpp`'s `clipSizes` and `viewModel`, `vr_weapons.cpp`'s `slotCache`, `vr_avatar.cpp`'s `info`,
`vr_handrig.cpp`'s `checked`, the grasp shapes and `vr_flashlight.cpp`'s gun spots by name) are hooked in by the fitted
hands' second pass (below).

### Tests (mock headset)

- **Scenes** (the same list, run by the upscaler-only build as the reference and by the final one): e1m2's, e4m1's and
  the firing range's pools shot with the shotgun (rings, foam, drops), e4m3's lava with nails; the firing range's wall
  shot, grunts shot and rocketed (decals, gore); the cap at 40 then 20, and changed to 30, 100, 25 while marks are
  made; e1m2's torches; e1m1's start; FSR, NIS and RCAS alone; the gadget's log, hologram and a centre print; the
  flashlight's beam. Composites in `perf/report/`: `water_before_after.png`, `decals_before_after.png`,
  `torches_start_upscale_before_after.png`, `beam_quality.png`, `menu_flashlight.png`.
- **Identity checks in the running game** (checked builds, never committed): the water tables against the old maps on
  every call (682,196 calls in e1m2, e4m1 and the firing range, no mismatch); the decal ring against the old deque with
  the same operations after every mark and expiry (1,235 marks over the decal and cap scenes, no mismatch). The decal
  counts by kind are the same at every step, before and after.
- **Images:** the decals, water, torches, e1m1 and the upscalers match. The differences left are on the held gun and
  its hand (a few hundred pixels, up to 150 of 765 at the grip's edge) and one gib's shading (up to 9 of 765). They are
  not from the changed code: bisecting, builds with only keyword or bit-identical changes (item 10 alone; items 4, 8
  and 9) move them too, so something in the hand's pose depends on the build (worth a look by the hands' owner). It
  was the grasp's worker thread (a solve landing a frame sooner or later); the second pass solves on the main thread,
  and the hands are now the same from build to build (see [Determinism](#determinism)). The
  mock's scenes also depend on what ran before in the same process (the decal scene makes 118 marks after the water
  scenes, 128 alone), so comparisons use the same scene list.
- **Motion takes** (item 3 touches the network path): ten of the author's takes (punches, slashes, a stab, the gun's
  butt, a shove, a parry bash, a pommel, a miss), `vr_motion_eval <folder> recorded quit` in the firing range: 10 of
  10 replays hit as their takes did live, before and after, and the two evaluation tables are identical. (Three are
  FAIL by expect.cfg, hilt_pommel, parry_bash and stab_one_hand, the same before and after.)
- `vr_flashlight_beam_quality` 0, 1, 2: the beam on walls, the floor and the corridor's corners matches today's within
  the run-to-run noise at every setting (`beam_quality.png`; its last column is the low setting's difference x16).
- The Flashlight page shows Beam Quality: Medium.

### Not done or not verified

- The water sounds' random variety was checked only on the plip the water scenes play (the same one before and
  after). Their precache indices changed: the 22 water sounds now come first in the list.
- The beam's cost in e1m2's dark room, and with the torch on the chest, was not measured after.
- The eviction's O(n^2) case was measured in a standalone test with the same maps, not in the game (2048 cached
  entities needs a scene I didn't build).
- `vr_graphics_preset` doesn't set `vr_flashlight_beam_quality` (medium at every preset).
- Not tried in the headset.

## Menu: sliders past their ends

Your note: many sliders need wider ranges, and you want to be able to go further yourself. Two changes: the
ranges of the placement sliders are wider, and those sliders now go on past their ends with left and right (the
sticks), as far as a hard limit.

**How it works** (`vr_menu.cpp`: `Item::extend`, `stepSlider`; `vr_menuui.cpp`: `drawSlider`)

- A slider is marked extendable (`.extend(lo, hi)`) when going further means something: offsets, positions, angles,
  scales, distances. Shares, volumes, colours, chances and choices keep their ends.
- Left and right step past the bar's ends, to the hard limit. Coming from inside the bar, a step stops at the end
  first. A new press goes past, and so does holding on at the end for 0.6 s. Past the end, holding steps faster the
  longer it is held (2x after a second, then 5x, then 10x), on multiples of the steps. Coming back, a step lands on
  the end before going inside again.
- A value past the end: the thumb stays at that end in light blue, with an arrow outside it pointing on, and the
  value is in white. With the VR menu style off, the value is in white.
- The help of an extendable slider says so, with the hard limits: "Hold left or right past the ends to go further
  (-150.00 to 150.00)". It comes first while the value is on or past an end, and after the slider's own help
  otherwise. Pages whose only help is this now keep the four help lines too.
- The laser sets a value only along the bar. Pointing at a slider doesn't change it. A click on a thumb pinned at an
  end keeps the value until the laser moves 4 menu pixels along; then it follows the laser along the bar.
- Values set in the console or a config are not clamped by the menu: not when it draws them, not by other keys
  (up, down, A, the pointer). Left or right towards the bar brings a value beyond the hard limit to that limit. Left
  or right away from the bar does nothing.
- `VR_Menu_Key` gets the key's auto-repeat flag from `M_Keydown` (menu.c): the pause at the end and the speed-up need
  to know a held key from a new press.

**Why this design:** it is one gesture you already use (left and right), it works on every extendable slider, and
the bar keeps its resolution for the laser. Buttons at the ends would take room on every row, and a typed value
needs a keyboard.

**Sliders changed** (visible range → new visible range, and the hard limit; "=" unchanged)

| Page | Slider | Bar | Hard limit |
|---|---|---|---|
| VR Settings | Teleport Range | = 100..800 | 100..3000 |
| | Room Scale | = 0.5..2 | 0.2..5 |
| | Height | 1.2..2.2 → 1.0..2.2 m | 0.5..3 |
| | World Scale | 0.75..1.5 → 0.5..2 | 0.25..4 |
| | Floor Offset | -40..10 → -50..30 | -400..400 |
| | Gun Angle, Off Hand Angle | = -30..90 | -180..180 |
| | Crosshair Size | = 0.5..8 | 0..32 (the code's clamp) |
| | HUD Scale | = 0.01..0.05 | 0.005..0.3 |
| | Menu Distance | = 40..150 | 8..600 |
| | Menu Scale | = 0.08..0.3 | 0.02..1.5 |
| | Torso, Legs Offset | -0.15..0.3 → -0.2..0.4 m | -1..1 |
| | Shoulders Offset | -0.1..0.15 → -0.15..0.2 m | -0.5..0.5 |
| | Render Scale | = 0.5..1.5 | 0.25..2 (the code's clamp) |
| Body | Torso, Legs Offset | as above | -1..1 |
| | Shoulders Back | -0.1..0.15 → -0.15..0.2 m | -0.5..0.5 |
| | Shoulders Up | ±0.1 → ±0.15 m | ±0.5 |
| | Shoulders Width | ±0.08 → ±0.12 m | ±0.3 |
| | Eyes Forward, Eyes Up | 0..0.2 → 0..0.25 m | -0.1..0.5 |
| | Crouch Tilt | 0..60 → 0..80° (the code's clamp; not extendable) | |
| Arms and Pauldrons | Arm Length | 0.8..1.3 → 0.7..1.4 | 0.5..2 (the code's clamp) |
| | Arm Stretch | = 1..1.5 | 1..3 |
| | Shoulder Reach | 0..0.2 → 0..0.25 m | 0..0.6 |
| | Shoulders Up, Shoulders Forward | 0..45 → 0..60° | 0..90 |
| | Pauldron Size | = 0.5..1.5 | 0.25..4 (the code's clamp) |
| | Pauldron Forward, Up, Out | ±0.05 → ±0.08 m | ±0.3 |
| Flashlight | Range | = | 100..6000 |
| | Lean Out | = | -30..60 (the code's clamp) |
| | Forward, Up, Out, In Hand ×2, On Gun ×3, On Head ×3 | = (the flashlight change widens them) | 4 bar widths beyond each end |
| Wrist Gadget | Size | = 0.5..2 | 0.25..3 (the code's clamp) |
| | Along the Arm | ±10 → ±15 cm | ±40 |
| | Across the Arm | ±5 → ±8 cm | ±20 |
| | Height | -3..5 → -5..8 cm | -15..25 |
| | Pitch, Yaw | = ±90 | ±180 |
| Screens | Hologram Text Size | = 0.5..2 | 0.25..4 (the code's clamp) |
| | Hologram Height | 0..10 → 0..15 cm | 0..50 |
| | Console Log Height | = 0..20 cm | 0..60 |
| Force Grab | Distance | = 100..1500 | 100..4000 |
| Weapon Offsets | Offset X, Y, Z; Muzzle X, Y, Z; Aim Offset X, Y, Z | = ±30 | ±150 |
| | Scale | = 0.1..3 | 0.02..10 |
| | Thumb X, Y, Z | = ±4 | ±20 |
| | Hotspot X, Y, Z | = ±40 | ±200 |
| | Hand Pitch, Hand Yaw (hotspot) | = ±90 | ±180 |
| | Bias (hotspot) | = 0..10 | 0..50 |
| | Screen X, Y, Z | = ±20 | ±100 |
| | Screen Scale | = 0.05..3 | 0.01..10 |
| Hand/Gun Calibration | Gun Model Pitch, Gun Yaw, Off-Hand Pitch, Off-Hand Yaw | = ±90 | ±180 |
| | Gun Model Scale | = 0.1..2 | 0.02..5 |
| | Gun Model Z Offset | ±5 → ±10 | ±50 |
| | Gun Z Offset | = ±30 | ±150 |
| Player Calibration | World Scale | = 0.5..2 | 0.25..4 |
| | Floor Offset | = ±200 | ±400 |
| Locomotion | Teleport Range | = 100..800 | 100..3000 |
| Status Bar | HUD Scale | = 0.01..0.1 | 0.005..0.3 |
| | Offset X, Y, Z | = ±200 | ±1000 |
| Hotspots | Shoulder, shoulder holster, hip, upper holster X, Y, Z (12) | = ±50 | ±200 |
| | Virtual Stock, Shoulder, Hip, Upper Threshold | = 0..30 | 0..100 |
| | Holster Slot Scale | = 0.1..2 | 0.02..5 |
| | Holster Slot X, Y, Z | = ±100 | ±400 |
| Crosshair | Crosshair Z Offset | = ±10 | ±50 |
| Menu | Menu Scale, Menu Distance | = | as on VR Settings |

Not extended: the full-circle angles (±180: weapon and screen pitch, yaw and roll, the gadget's roll, Gun Angle on
Hand/Gun Calibration, the status bar's ±3.14 rad). Also Row Spacing and Menu Height, which the menu's layout clamps
to 1..2. The Carrying page is left as it is, since another change edits it.

### Tests (mock headset)

- The Wrist Gadget's Along the Arm (bar ±15, limit ±40), held right from 13 with the off stick, printed every 10
  frames: 13.5, 14, 14.5, 15, 15, 15, 15.5, 16, 16.5, 17.5, ... 19, 21, 22, 23, 25, ... 30, 32.5, 35, 37.5, 40, 40,
  and so on. That is the stop at the end, then past it, faster, up to the limit. Held left from 40: 39.5 ... 28, 25, 20,
  17.5, 15, 14, 13.5, 13, 12 ... So it lands on the end and goes on inside at the normal step. Pressed again at 15
  (not held): 15.5 at once.
- Set in the console: vr_gadget_x 60 (beyond the limit), vr_gadget_scale 5 (beyond Size's limit of 3). Moving the
  selection over them, A, and right leave 60 and 5. Left brings them to 40 and 3.
- Laser (mock hand aimed at the bar, `vr_gunangle 0`): vr_gadget_x 25; pointing at the bar's middle leaves 25. A
  trigger press on the pinned thumb leaves 25. Moving ~2 px leaves 25. Dragging to the middle sets -0.5. A click on
  the bar at a quarter sets -7.5.
- Pictures (`menu_past_end.png`, handed over with the report, not in the repository): the thumb at 15.0 cm with the hint first; past the right end (25.0 cm, 120°) and the
  left end (-12.00 cm); the laser pressed on the pinned thumb, then dragged; the desktop style with the white values.
  The Weapon Offsets and Hotspots pages were checked with the help lines.

## Blunt pommel sound

Your note: hilt and pommel hits should sound blunt, different from slashes and stabs. Before this, a sword's pommel
strike played the sword's cut (`knight/sword2.wav`), and an axe's or Mjolnir's handle end and a gun's butt played
the punch (`fisthit.wav`) or nothing.

- **The sound:** `vr/pommel1..3.wav`, made by `make_sounds.py` (`pommel()`; synthesised, credited in CREDITS.md). It
  is a hard tick, then a short wooden knock: the low modes of a dense knob, damped within tens of milliseconds, with
  no ring. Under it are a crunch of flesh and a short low thump. It has no whoosh (the shove's and the bash's), no
  metal clang (the bash's) and no ring (the parry's). The three are pitched 1.0, 0.89 and 1.12. Each hit plays one
  of the two not played last.
- **Compared** (energy by band; how soon 90% of it is out):

  | Sound | 90% by | Centroid | <150 Hz | 150-500 | 500-1.5k | 1.5-4k |
  |---|---|---|---|---|---|---|
  | `vr/pommel1..3` | 0.053 s | 450-530 Hz | 0.22 | 0.49-0.53 | 0.22-0.25 | 0.03 |
  | `fisthit` (punch, gun swing, axe) | 0.098 s | 310 Hz | 0.48 | 0.44 | 0.03 | 0.03 |
  | `knight/sword2` (slash, stab) | 0.098 s | 114 Hz | 0.94 | 0.03 | 0.02 | 0.01 |
  | `vr/bash` | 0.216 s | 388 Hz | 0.72 | 0.13 | 0.10 | 0.03 |
  | `player/axhit2` (a blade on a wall) | 0.086 s | 1650 Hz | 0.06 | 0.23 | 0.09 | 0.61 |

  It is the shortest of them, and the only one with its weight in the knock's 150 Hz-1.5 kHz, which the Quest's
  speakers carry.
- **Where** (`QC/vr_melee.qc` `VR_Melee_HitSound`, `weapons.qc`): a blow of the pommel kind (the near end: a sword's
  pommel or hilt, an axe's or Mjolnir's handle end, a gun's butt) that lands on something that takes damage.
  `PlayerVRMeleeImpl` marks the blow being struck (`vr_melee_blunt`). The sword, the axe and the gun then play the
  knock instead of their own sound, and Mjolnir, whose head's hit plays none, plays it too. Slashes, stabs, chops,
  a gun's swing and punches keep theirs. So do walls: the pommel on stone still clangs (`player/axhit2`). Precached in
  `world.qc`. With `developer` on, each melee hit sound prints `melee sound: <file>`.

### Tests (mock headset)

- 53 of your takes (all 27 hilt_pommel and 17 gun_strike_butt, and 2 gun swings, 3 slashes, 2 stabs and 2 punches),
  copied and replayed with `developer 2; map vrfiringrange; vr_motion_eval <folder> verbose quit`, with the sound on.
  The console's `melee sound:` lines: all 23 pommel strikes and 6 butt strikes played `vr/pommel1..3`. The 4 slashes
  and 2 stabs played `knight/sword2`, and the 1 chop, 3 jabs and 2 gun swings played `fisthit`. So the knock played
  for no other blow, and no pommel or butt strike played anything else. No "not precached" and no load errors.
- The verdicts are the round's: hilt_pommel 23 of 27 and gun_strike_butt 9 of 17 pass. The pommel takes that fail
  read as slashes (02-44-18, 04-05-33, noted above), and they play the slash's sound.
- Not heard: the sounds were checked by their spectra and length only, not listened to in the headset.

## Two-handed props

Your request: hold a physics prop with both hands at once, moving and turning naturally with both, thrown with both,
and passed from one hand to the other without dropping it. Until now a second hand couldn't take a carried thing:
the carry was per hand, and a carried thing isn't solid, so no hand touch reached it.

### What it does

- **Taking it in both hands.** While one hand carries a box, backpack, gib, head or armour, grip it with the other
  hand. The rules are the first grip's: a grip press (not a fist moved onto it) within Carry Reach (8 cm) of its
  drawn surface. It is then held in both hands, with a softer click and a buzz in the second hand.
- **Moving and turning.** It follows both hands rigidly, as if held at the two places the hands took it:
  - Move or turn both hands together (as one body) and it moves and turns exactly with them.
  - Move one hand and it swings about the other.
  - Twist both hands about the line between them and it rolls. One hand alone rolls it half as much.
  - Pull the hands apart or push them together and it doesn't stretch or shrink. It stays centred between them, and
    each drawn hand stays on its grip, up to Two-Handed Hand Drift (8 cm) off your real hand. Pulled further, the
    drawn hand leaves its grip.
- **Fingers.** Both hands' fingers wrap it (the fitted hands' grasp, each hand solved on its own). The second hand's
  palm moves up to Palm Fit (5 cm) to sit on the surface where it gripped.
- **Letting go with one hand.** The other hand keeps it, held from where it is, with no jump. The hand that let go,
  drawn on its grip until then, eases back onto your real hand over 0.15 s. This is the hand-over: grab with the
  other hand, let go with the first.
- **Throwing with both.** Let go with both hands within 0.1 s of each other (Two-Handed Throw Window) and it is thrown
  with its own motion as your hands moved it. Its velocity is that of its centre: the hands' middle, plus its spin
  about that middle. Its spin is that of a thing held rigidly at both hands: the line between the hands turning, plus
  the hands' own roll about that line. So a box flicked forward by both wrists tumbles forward. The usual throw rules
  follow: throw gain, throw gravity, Box Throw Speed, the damage of a box or gib thrown hard.
- **Holsters.** Let go at a holster with one hand while the other still holds it: it isn't taken; the other hand keeps
  it. Only a one-handed carry puts a thing in the pack. Both hands letting go together is a throw, never a take, even
  with a hand at a holster. With Take a Box on Trigger, either hand's trigger takes it.
- **Armour** held in both hands is put on when you let go over your torso, as with one hand.

### How it works

**The pose from both hands** (`vr_carry2h.cpp`, used by the server and the client alike). When the second hand
takes hold, three things are kept: each hand's grip in the object's frame, the object's turn in each hand, and the
direction from the off hand's grip to the main hand's. Each frame:

1. Each hand alone would carry it turned so (its turn in that hand). The two turns are averaged (a quaternion mean,
   the two on the same side, so never degenerate).
2. That turn is swung the least way that lines the kept grip-to-grip direction up with the line between the hands
   now.
3. The middle of the grips is put on the middle of the hands.

When both hands move as one rigid body, this gives back that motion exactly: steps 1 and 2 then have nothing to
average or swing. With the hands (or the grips when taken) within 3 cm of each other, there's no line to follow. The
swing fades out between 6 and 3 cm, and the averaged turn alone is left, as one hand's. Nothing divides by a
distance that can be zero, so no NaN.

**Server** (`QC/vr_carry.qc`, `VR_Carry_TwoHandFrame`, before the hands' own frames):

- A hand not carrying, with an empty hand, not force-grabbing, not holding the flashlight, pressing its grip in
  reach of what the other hand carries (`carryreach`, the touch test's), takes hold. Both hand fields then point to
  the thing, and `carry_2h` is set.
- The thing goes where `carry2h` puts it. It stops at walls and monsters and is dropped when left 32 units behind, as
  one hand's carry (`VR_Carry_Follow`, now shared).
- One hand letting go: `VR_Carry_Regrip` gives the other hand its place and turn from where it is (no carry fit, as
  it doesn't move).
- Both letting go: the main hand's frame lets go of it as before (throw, armour, drop), but not into the pack.
- A holster take needs the last two-handed hold to have ended more than the window (plus 0.05 s) before.

**Client** (`vr_held.cpp`). Your held things are drawn in your hands this frame (Drawn In the Hand). Held in both
hands (both carry stats name it), it is placed by the same solve from this frame's controllers. It is kept from where
it was drawn when the second hand took it, so it doesn't jump. Each hand is drawn on its grip: its place moved at most
the drift off the controller, and its turn at most 40Â° (a twist of one hand the other doesn't share). The hook in
`vr_view.cpp` is three lines: the hand is drawn as if its controller were there, so the fist's offsets, the arm and
the fitted fingers follow as usual.

**The throw** (`vr_throw.cpp` `estimateBothAt`, `vr_client.cpp`). The client throws with its own estimate, and
nothing new is sent. For a hand that lets go of a thing held in both hands, or held in both until the other hand let
go at most the window before, the throw estimate it sends is the object's. From both hands' samples of the same
frames, it builds the object's velocity and spin, as above. It then takes the peak as a one-handed throw does (the
same window, peak averaging and fit, direction look-back). There is no lever-arm flick: the spin about the middle is
exact. The server throws with the last hand's estimate, as ever.

**Other cases:**

- **Force grab** (the catch is a hand touch): it goes into a free hand as before. A hand that holds something can't
  force-grab.
- **Weapons:** a hand with a weapon can't take hold. A carried thing blocks weapons for both hands, as before.
- **Gibs:** a gib held in both hands can't be struck or shot "by the other hand".
- **Death:** new, for one hand too. What the hands carried dropped nowhere before: `PlayerPostThink` stops before the
  hands' frames while dead, so it hung in the air, still yours, until the respawn. `VR_Carry_Dead` lets go of it with
  the motion it had.
- **Save and load:** the flag is saved. The kept grips aren't: they are taken again from the loaded pose at the first
  frame.
- **Map change:** the new server forgets the grips (`resetRigidBodies`).
- **Multiplayer:** the server does it all per player, from the hands and throw estimates each client already sends.
  Your client draws it from your hands, as it does your one-handed carries. Others see where the server puts it.
  The drawn hands on their grips are local only: others see your hands where they are.
- **vr_carry_two_hands 0:** no new two-handed holds. One already held stays until let go of.

### Settings (Carrying and Gibs page)

| Cvar | Default | Menu | |
|---|---|---|---|
| `vr_carry_two_hands` | 1 | Two-Handed Carrying | The other hand can take hold of what one carries. |
| `vr_carry_two_hands_drift` | 8 | Two-Handed Hand Drift (0..20 cm, on to 50) | cm each drawn hand may be off your real hand to stay on its grip; 0: the drawn hands stay on your real hands. |
| `vr_carry_two_hands_window` | 0.1 | (console) | Seconds between the two hands letting go that still make a two-handed throw (the server allows 0.05 s more for the moves' timing). |

Debugging:

- `vr_debug_carry 2` writes `carry_trace.txt` in the game folder, one line a frame: the object's place and turn, and
  each hand's controller and drawn pose. With 2 or more, the second hand's reach test prints too.
- `developer 1` prints `carry: both hands`, `carry: one hand let go, held in the other`, `carry: both hands let go`,
  `carry: thrown at <velocity> (<speed>), spin <rad/s>`, `carry: dead, let go`.
- `vr_debug_throw 1` also prints `throw both hands (...)`.

### Costs

Measured with `run.sh --exclusive` in the firing range, over 7.4 s of the hands circling and rolling (about 1840
frames at 249 fps; `vr_profile`'s CSV, ms per frame):

| | `held` (client) | `carry2h` (server) | the two hands (`hand`) | `grasp solve` |
|---|---|---|---|---|
| nothing held | 0.000 | - | 0.018 | - |
| one hand (before this change) | - | - | 0.020 | - |
| one hand | 0.001 | - | 0.020 | - |
| both hands | 0.001 (max 0.046) | 0.000 (max 0.034) | 0.020 | none: the hands stay on their grips, so no re-solve |
| both, pulled 12 cm apart and back again and again (past the drift) | 0.001-0.002 | 0.000 | 0.029-0.032 | 0.24 a frame, 0.008-0.009 ms a frame |

The second hand's first grasp solve on the health box is a one-off 1.0 ms (`vr_grasp_bench`: 29 palm places tried,
as the hand isn't flush yet). After that it takes 10 Âµs (median of 50). The main hand's is 19 Âµs.

### Tests (mock headset)

Scripted with `vr_mock_play` (keyframes on the clock: the hands report the motion's velocities), in the firing range.
The main hand takes the thing and the off hand takes it second. The object's pose and the hands come from the trace;
"rigid" is what a thing rigidly held would do.

| Phase | Health box | Backpack | Green armour |
|---|---|---|---|
| Pulled apart 6 cm each: turn / drawn hands off the controllers / off their grips | 0Â° / 6.00 cm / 0.00 cm | 0Â° / 6.00 / 0.00 | 0Â° / 6.00 / 0.00 |
| Pushed together 5 cm each | 0Â° / 5.00 / 0.00 | 0Â° / 5.00 / 0.00 | 0Â° / 5.00 / 0.00 |
| Both roll 40Â° about the middle (rigid 40Â°) | 39.99Â° | 39.99Â° | 40.01Â° |
| Both pitch 45Â° about the grip line (rigid 45Â°) | 45.00Â° | 45.00Â° | 45.00Â° |
| Both yaw 45Â° about the middle (rigid 45Â°) | 45.00Â° | 45.00Â° | 45.00Â° |
| The main hand alone twists 40Â° (half: 20Â°) | 20.00Â° | 20.00Â° | 20.00Â° |
| The main hand alone up 15 cm (the swing about the off hand) | 27.35Â° (27.35) | 27.35Â° (27.35) | 20.56Â° (20.56) |

- **The grips' middle** stays on the hands' middle in every phase held in both hands (0.00 cm).
- **Changes of hold** (one hand, both, one, both, the hand-over to the off hand, both, thrown): the largest move of
  the object in one frame over each change is 0.00 cm and 0.00Â°. When the thrown box leaves the hands, it moves at
  its own speed as before. Stretched 5 cm and the off hand let go: the object stays put. The drawn main hand
  eases back onto its controller, frame by frame: 5.00, 4.99, 4.84, 4.54, 4.12, 3.61, 3.05, 2.45, 1.86, 1.30, 0.80,
  0.40, 0.12, 0.00 cm.
- **Throws** (the hands 0.5 m forward and 0.2 m up in 0.2 s, 2.5 m/s; the wrists flicking 60Â° forward, 5.24 rad/s;
  both let go together):

  | | Estimate sent (object's centre) | Spin | Thrown (u/s, after the throw gain) |
  |---|---|---|---|
  | Health box (its centre near the grip line) | 2.80 m/s | 5.24 rad/s about the grip line | 81.1 u/s |
  | Backpack (centre about 30 cm above the grips) | 3.94 m/s | 5.24 | 132.5 u/s |
  | Armour (centre above the grips) | 4.01 m/s | 5.24 | 135.9 u/s |

  Over the last frames held, the object's own motion (from the trace) is 2.50 m/s forward and 1.00 up with the
  hands' middle, and it spins at 6.11 rad/s (backspin run) or 5.24 (flick) with the hands. The tall things come out
  faster: their centre is above the hands, and a forward flick moves it forward. This is a rigid body's motion, not a
  tuning.
- **Hand timing:** the off hand let go 60 ms before the main: a two-handed throw ("60 ms after the other", 2.72 m/s,
  the tumble). 250 ms before: the main hand's own one-handed throw (2.69 m/s, its own spin). Drawn in the hand off
  (`vr_carry_local 0`): the same two-handed estimate.
- **Holsters:** held in both, the main hand let go at the right hip holster with the off hand also at the left one:
  "one hand let go, held in the other", not taken. The off hand alone then let go at the left hip holster: "into the
  pack" (100 health). A shells box let go of by both at once, both hands at hip holsters: dropped, not taken. The
  trigger (`vr_carry_take 1`) with both holding: taken ("You got the shells").
- **Weapon hand:** the off hand holding a shotgun (Weapon Grip Mode sticky, grip pressed anew at the box): no hold.
- **Force grab:** the main hand carrying the box, the off hand force-grabbed a floating armour and caught it; each hand
  had its own.
- **Walls, monsters:** the box held in both hands, pushed at a grunt: the box and the hands stop together at it.
  Lowered to the floor: they stop on it.
- **Degenerate hands:** the off hand took hold at the main hand's own place (grips 0 units apart). It was then held
  by the averaged turn: turned, moved, one hand twisted, pulled 10 cm apart and back. Then a normal hold whose hands
  passed through each other to swap sides, and back. Every value in the 2054 frames of the trace is finite. When the
  hands cross, the thing turns over, 180° while they are within 3-6 cm of each other (about 50 ms at that speed).
  Real hands can't pass through what they hold.
- **Death, save, map:** killed while holding it in both hands: "carry: dead, let go", on the floor. Saved and loaded
  while held in both: still held in both, and letting go with one hand, then taking hold again, work. A map loaded
  while held in both: nothing held after, no error.
- **One hand, before and after** (the baseline build against this one, the same script): the throw estimate is the
  same, 2.69 m/s, 5.2 rad/s. The holster take is the same, "into the pack". The gib throw is 4.49 against 4.48 m/s
  (the console-scripted hand's timing). Landing spots vary by a few units from run to run on either build (the rigid
  body's bounces).
- **Your motion takes:** all 474, `vr_motion_eval <folder> recorded` in vrfiringrange, before (97d098ef) and after:
  the tables are identical take by take. 164 of 474 reproduce their live hits on both; the rest were recorded before
  round 21's melee. The 12 of them picked for the check (punches, a stab, one- and two-palm shoves, a gun butt, parry
  poses, a no-hit shove) reproduce their live hits, 12 of 12, on the final build.
- Pictures (handed over with the report, not in the repository): `c_box.png`, `c_backpack.png`, `c_armour.png` (the
  14 phases each), `c_closeup.png` (the fingers on the box: one hand, both, pulled apart, pitched).

### Limitations

- The grips are kept where the hands were when the second one took hold. The client keeps them from its own frame,
  a frame after the server, so the drawn thing and the server's may differ by a frame's hand motion while held in
  both. Let go of, the drawn thing eases to the server's place over 0.2 s, as with one hand.
- A thing dropped when left 32 units behind a wall or monster could not be shown in the mock. The hands themselves
  stop at walls and monsters, so the thing never falls that far behind. The code is the one-handed carry's.
- The backpack is held where one hand's carry fit put it, well out from the fist (up to 15 cm). The second hand then
  grips it low. That is the one-handed carry fit, unchanged.
- Found, not changed: after loading a save, a carried box isn't drawn in the hand (with one hand too, on the base
  build), though the server still has it there.
- Not tried in the headset.

### In the headset

- [ ] Carry a box in one hand, grip its other side with the other hand: it's held in both, with a click and a buzz,
      without moving.
- [ ] Move both hands together, turn them together (roll, pitch, yaw): it moves and turns with them, the hands
      staying on it.
- [ ] Move one hand up or forward: it swings about the other. Twist one wrist alone: it rolls half as much.
- [ ] Pull your hands a little apart, then push them together: it doesn't stretch; your drawn hands stay on it.
      Pulled further than Two-Handed Hand Drift (8 cm), they come off it. Is 8 cm right?
- [ ] Hand-over: carry in the right hand, grip with the left, let go with the right. The left keeps it, no jump, and the
      right hand slides back to where your hand is.
- [ ] Throw with both (a chest pass, an overhead throw, a flick): does it fly as your hands threw it, and tumble as
      they turned it? Let go of one hand a little late: still a two-handed throw within 0.1 s (Two-Handed Throw Window,
      `vr_carry_two_hands_window`).
- [ ] At a holster: let go with one hand while the other holds it: not taken. Then let go of it with the other hand at a
      holster: into your pack.
- [ ] The armour in both hands, let go over your chest: worn.
- [ ] A backpack, a gib and a head in both hands: do the fingers sit on them?
- [ ] Die while holding something: it drops.
## Held weapons against models

Your request: a weapon you hold should not pass through monsters, other players, corpses and things on the ground,
stopped at the model as it is drawn, not at its hitbox (Quake's boxes are much bigger than the models), with a fast
test on the main thread. Branch `agent/wpncollide`; the code is `Quake/vr/vr_modelcollide.cpp`, hooked into the view
by two calls. Composites and traces are in the scratchpad's `wpncollide/`.

| | |
|---|---|
| What stops | monsters alive or dead (corpses, the training dummy), other players; with Objects, boxes, gibs, heads, backpacks, armour and weapons lying on the ground |
| What it tests | the model's triangles as the renderer draws them this frame: its two poses and the lerp between them, its movement's lerp, its scale and the networked scale and offset |
| What is stopped | the weapon in each hand (its own vertices), and with Hands Stop at Models the empty hand (fist or open) |
| How it gives | a push out of the surface, eased (out in about a hundredth of a second, back in over 0.06 s); up to Model Push Limit (20 cm) all of it, deeper less and less, none at twice (it lets go) |
| What the game reads | the tracked hands and muzzles as before: melee contacts, kinds and damage, shots, aim and two-handed grips are unchanged (your 474 takes replay exactly as before) |
| Cost | 0.002 ms a hand with nothing near, 0.006 with a monster near, 0.006-0.010 in contact; 0.045 ms a frame in a crowd of 12 overlapping grunts with both hands among them (budget: well under 0.1) |

### How it works

- **The weapon.** Each hand's weapon as it was drawn last frame (its model, its pose, where it sits in the hand) is 24
  of its own vertices, spread over it (farthest-point samples of the model's pose, made once per model and pose). The
  test casts a ray from the hand to each of them, and one more to the hand from 16 units behind it (towards the chest):
  the hand itself inside a model. An empty hand is its hand model (`progs/hand.mdl`) the same way.
- **What is near.** Every entity drawn this frame with an alias or brush model, not the player, not what either hand
  holds, and of the right kind (below). Its box as drawn (its frames' bounds, or a brush model's, placed by the
  renderer's matrix) must meet the rays' box, grown by twice the push limit (a push moves the rays).
- **Its triangles as drawn.** For those models only, the vertices are posed as `r_alias.c` draws them: the lerp
  between their two poses (`R_SetupAliasFrame`, read without changing it) and the move lerp of a walking monster
  (`R_SetupEntityTransform`), placed with `R_EntityMatrix` and the VR transforms (networked scale and offset).
  Triangles are kept in runs of 8 under one box: a ray skips a run at once.
- **The test.** Each ray against the triangles near it (Moller-Trumbore). Quake's models are wound clockwise seen
  from outside (so the renderer culls `GL_FRONT`): a ray that crosses a triangle from its front goes into the model.
  The first place a ray goes in, and the first place after it where it comes out of the same model (or its end): that
  part of the weapon is inside, and must be moved out of the surface it went in by (a plane: its point and normal).
  A ray that passes through a thin part (an arm) is pushed off it by the part's depth; a blade pushed into a body is
  pushed back as deep as its tip went in.
- **The push.** The least move out of every plane found (Gauss-Seidel, 16 sweeps at most); then the rays are tested
  again from there, three rounds at most (a curved surface, a second model).
- **Letting go.** Up to `vr_model_collide_max` (20 cm) the push is all of it. Deeper, it is less and less, none at
  twice that: a monster walking into your gun, or a blade pushed far into a body, passes through smoothly instead of
  the hand being dragged away without limit. Walls don't let go; models do, as they move.
- **Easing.** Out in about a hundredth of a second (a time constant of 0.012 s: nearly at once, so nothing shows
  through), back in over 0.06 s (no snap when the contact ends).
- **Drawn only.** The view moves `s.pos` of each hand by its push before the weapons, hands, fingers and body are
  placed (so the weapon, the hand, the fingers and the arm all move together, like the parried-blow knock) and takes
  it back out afterwards, from the hands, the muzzles and the two-handed grips. So the server, the melee, the aim,
  the crosshair and laser (where the shots go), the two-handed grip and holstering all read the tracked pose as
  before. The muzzle flash's light and your beams (lightning, the grapple's rope) start at the drawn muzzle.

**Melee is on the tracked pose.** A blade drawn stopped at a monster's surface still hits it as the tracked blade
does, with the same kind and damage: the server gets the hands and muzzles it got before. (That is also why the
walls' collision in `vr_handpose.cpp` leaves monsters out: a hand held back from a monster's box read as a new stroke,
round 15.)

**Not blocked:** the helping hand of a two-handed grip (drawn on the weapon it helps hold, which is pushed: it
follows); a gun carried by its foregrip; a hand at a holster or passing a weapon to the other hand; a hand holding a
thing or the flashlight; your own body (only other entities are tested). The weapon changing this frame is tested
from the next.

**Which entities.** While you host (single player), by what the server says: Monsters = `FL_MONSTER` (alive or dead)
and `FL_CLIENT` (other players); Objects = anything else drawn with a model that lies still (moving under 150 u/s:
not missiles, thrown or flying things), not the level's brush entities (doors, lifts, buttons: `SOLID_BSP` or bigger
than 72 units, which the walls' collision already stops). As a client of someone else's server, by the model: brush
models under 72 units and rotating pickups or gibs are objects, models with a missile's trail are nothing, the rest
monsters.

### Settings

| Menu (Hand/Gun Calibration, "Against Monsters and Things") | Cvar | Default |
|---|---|---|
| Weapons Stop at Models: Off / Monsters / Monsters and Objects | `vr_model_collide` 0 / 1 / 2 | Monsters and Objects |
| Hands Stop at Models | `vr_model_collide_hands` | on |
| Model Push Limit (5-50 cm, left/right go on to 1-100) | `vr_model_collide_max` | 20 cm |

Tools: `vr_debug_model_collide 1` prints each hand's push while a model stops it (the raw push, what is given, what
is drawn, the entity, the models, triangles, rays, rounds and planes); `2` also draws the rays (grey where tracked,
green where drawn, red while pushed) and the push (yellow). `vr_model_collide_bench [n] [list]` times the test of both
hands as they are (min, median, 99th percentile, max); `vr_model_collide_bench probe` casts a ray along your view and
lists each model triangle it goes in or out by. For tests: `impulse 241` puts a monster (`vr_test_spawn`: the firing
range dispenser's numbers, 0 grunt, 2 zombie, 3 shambler...) or a box (100 health, 101 shells) `vr_test_spawn_dist`
units ahead, facing you, killed at once with `vr_test_spawn_dead 1`; `vr_mock_camera <x> <y> <z> <pitch> <yaw>` draws
the mock headset's eyes from elsewhere (a spectator's view of your body and hands; the hands stay with the head).

### Costs

Release build, the mock headset, `vr_profile` in exclusive runs; `vr_model_collide_bench 500` (per hand).

| Case (`vr_model_collide_bench 1000`, exclusive runs) | Median | 99% | Triangles near, rounds |
|---|---|---|---|
| nothing near | 0.0017-0.0023 ms | 0.0028 | – |
| a grunt near the sword, no contact | 0.0059 ms | 0.0072 | 0 (its vertices posed: its box meets the rays) |
| a sword stopped by a grunt's chest | 0.0102 ms | 0.0131 | 138, 3 |
| a sword stopped by a zombie | 0.0083 ms | 0.0090 | 189, 2 |
| a fist stopped by a grunt's gun | 0.0087 ms | 0.0225 | 32, 2 |
| a shotgun on a health box | 0.0058 ms | 0.0355 | 28, 2 |
| a crowd: 12 grunts spawned overlapping round you, the sword deep among 5 of them, the fist near 5 | 0.020 + 0.013 ms | 0.028 + 0.019 | 473 (14 planes, pushed past the limit: let go), 7 |

`vr_profile` in that crowd (exclusive, 4-second intervals): `model collide` 0.043-0.047 ms a frame for both hands,
the worst frame of each interval 0.08-0.15 ms (the view's other scopes had their own worst frames of the same size:
the machine's scheduling). With `vr_model_collide 0` the scope is gone. A model's triangle list and a weapon pose's
samples are made once, at first use.

### Tests (mock headset; `wpncollide/` in the scratchpad)

Side views of your body with `vr_mock_camera`, before (`vr_model_collide 0`) and after (2), with the rays and the
hitbox (`r_showbboxes 1`):

- `collide_grunt.png`: a sword stabbed into a grunt's chest, and a shotgun pushed into it: through it before, stopped
  at its surface after (4.2 and 5.2 units of push, 13 and 16 cm).
- `collide_corpse_box.png`: a sword pushed down into a corpse (and its dropped backpack near), and a shotgun into a
  health box on the floor: resting on them after.
- `collide_hitbox.png`: a zombie, whose hitbox is a 32-unit box round a thin body: the sword inside the box, short of
  the body, is not stopped (no push: nothing is hit); pushed on into its chest it is stopped at the chest.
- `collide_fist.png`: a fist pushed into a grunt's gun: held on it, the arm with it; the rays drawn by
  `vr_debug_model_collide 2` for the grunt and the zombie.
- `trace_swing.txt`, `trace_swing_2.tsv`: a sword swung through the training dummy and then pushed in slowly and out
  (`vr_mock_play swing.txt`). In the swing the blade is held out about 3.4 units for the three frames it is inside; in
  the slow stab the push grows with the depth, holds at the limit (5.25 units at `vr_world_scale` 1), gives way to
  none at twice that, and comes back as the blade is drawn out; the eased drawn push has no jumps. The dummy reported
  the same blows with the collision off and on (a slash of 22-23 damage, a stab of 36.4). Scripted on the real clock,
  these runs vary a little by themselves (three runs off: slashes of 22.7, 44 and 22, stabs of 36.3 and 36.4): the
  exact check is the replay of your takes.
- **Melee regression:** your 474 takes (copied), `map vrfiringrange; wait60; vr_motion_eval <copy> quit`, on the base
  (97d098ef) and on this branch (with vr-cleanup 5c89cec1 merged, the collision on by default): 420 of 474 pass on
  both, and the two tables are identical line for line: every take's verdict, its blows (kind, sub, hand, damage,
  point, time), frames and hand error. (`eval_base.csv`, `eval_new.csv`.)
- The menu: `menu.png`.

### Found on the way

- Quake's triangles face inwards by `cross(b - a, c - a)` (clockwise seen from outside, alias and brush models alike:
  `vr_model_collide_bench probe` shows the first crossing of a monster as "out" with the opposite convention).
  `vr_grasp.cpp`'s `inside()` (the free hand held out of the other hand's weapon, `vr_hand_collide`) says the nearest
  triangle "faces away" from a point inside by that same cross product, which would make it take points just outside
  the weapon for inside and the reverse. Not checked further and not changed here (the hand agents' file): worth a
  look.

### Limitations

- A weapon is its vertices as rays from the hand: a part of a monster thinner than the gaps between them (a finger,
  a claw tip) can poke between two rays into the weapon's side. The rays' fan covers a blade and a barrel closely.
- A monster's pointed part pushed into the flat of a blade is found by the ray that crosses it; one that lies wholly
  between two rays is not.
- Only Quake's `.mdl` (and brush) models: an MD3/IQM replacement model, or a skeletal one, is not tested.
- The laser and crosshair stay on the tracked aim (where the shots go), a few units from a gun pushed sideways.
- Mock only: not tried in the headset.

### In the headset

- Poke a monster (the training dummy, a grunt from the range's buttons) with a gun and a sword: the weapon should stop
  at its body, not at the air round it, and your hand and arm with it; a zombie or a shambler lets it much closer
  than its box.
- Swing through the dummy: the hits, kinds and damage should read as before (the blade is drawn held back for a
  moment, the blow is the tracked one).
- Push slowly on: past 20 cm (Model Push Limit) the weapon starts to sink in, and at 40 cm goes through; tell whether
  the limit should be bigger or smaller, or whether it should never let go.
- Rest a gun on a health box, a corpse, a backpack; an empty hand on a monster (Hands Stop at Models).
- Hold a weapon two-handed into a monster: the helping hand should stay on it.
- Holster and draw near a monster: nothing should change.

## Reviewing failing takes

Your request: mark the takes that don't pass and give you an easy way to filter and inspect them in the game, to
decide whether to keep them or throw them out. `MOTIONS.md`, "Reviewing failing takes", has the details.

- **Marked**: `vr_motion_eval` now also writes each take's verdict into `motions/eval_status.csv` next to the takes
  (verdict, reason, expectation, the replay's and the take's own events, when), merged across evaluations; the takes
  themselves are never written. The melee agent's suspect takes (above) are in `quakevr/motions/suspects.cfg` (in git),
  with the reason.
- **Review Takes** (Advanced VR Options, under Motion Recorder): "474 takes: 54 failing, 39 suspect", how many you
  reviewed and discarded, when they were evaluated; Show To Review / Failing / Suspect / Not Evaluated / Reviewed / All
  / Discarded, and a Category. A row is the verdict (`*` suspect), the label, the time it was recorded and `k` / `r`
  (kept, relabelled); under the list, why it fails and what it registered. A take's page: expected, replay and live
  events, the reason, the suspicion; **Play Ghost**, **Keep**, **Discard** / **Restore**, **Relabel**, **Undo Last**,
  Next / Previous Take, **Re-evaluate This Take**.
- **The ghost**: the take replayed in front of the dummy, looping at 1x to 0.1x: its weapons and empty hands drawn
  translucent and tinted where the take had them relative to the dummy, the weapon's line and striking points, the
  far end's trail, the head, and the events as they happen (live ones where they hit, the replay's over the dummy).
  Nothing is driven: it plays in the headset with your own tracking untouched.
- **Discard / Relabel / Undo**: a discard moves the take into `motions/discarded/`; a relabel renames it to the new
  label (same time) and changes its header's label lines, keeping the original byte for byte in
  `motions/review/relabelled/`. Every change is in `motions/review/undo.csv`: Undo Last takes back the last one, also
  after a restart (checked: relabel, discard and keep made in one session and undone in the next leave both takes
  identical to yours, byte for byte).
- **Re-evaluate**: the evaluation replaces the head's and hands' tracking and reloads the map for each take, so it
  can't run in the game you play; the review runs a second copy of the game in the background (`-vrmock`
  `-noconfigwrite` `-noautoexec` `-evalcopy`: the mock headset, your config and autoexec untouched, no copy of a
  copy), with your current settings, and reads the verdicts when it quits. One take: 6 s.

### Checked (mock headset, on a copy of your 474 takes)

- `vr_motion_eval` on all of them: **420 of 474** pass, as on the base (the same 54 fail; the tables identical but one
  take's hand error, 0.073 against 0.091 units, the last take of the run: no verdict or event differs).
  `eval_status.csv` has all 474.
- The list: To Review 54 (every suspect take fails too); Failing + Expected Slash 7. A relabel (`no_hit` 02-21-03 to
  `palm_shove_2h`, the suspect "both open palms pushed out"), then Re-evaluate This Take: FAIL "no shove/both" (the
  replay registers two one-hand shoves).
- Robustness: a take renamed by hand shows `old` with its old verdict; new takes `new`; an empty or broken file
  refuses to play with a message; an undo whose file was deleted says so and is dropped.
- Found on the way: the engine keeps only the command line's first 256 characters for its `+` commands (a long
  `-basedir` cut the copy's script off); and the kit's `autoexec.cfg` ran in the copy (it started a copy of its own,
  and so on): hence `-noautoexec` and `-evalcopy`. `menu_vr <page> <row>` puts the cursor on a row (for scripts).

### Limitations

- The ghost's weapon is placed as a held weapon from the recorded hand pose with today's weapon offsets (the take
  records the hand, not the drawn model); the recorded weapon line and striking points, drawn with it, are exact.
  Empty hands are the plain hand model, not the fitted hands' pose. A gun carried by its pump isn't recorded as such.
- Re-evaluate shares the GPU with the game in the headset: a few takes should not be noticed, a long list may drop
  frames. Not tried in the headset.

### In the headset

- [ ] Firing range, Review Takes: the list, the filters, a take's page; readable?
- [ ] Play Ghost on a failing slash and a suspect no_hit: can you tell what went wrong? Is 0.25x a good speed?
- [ ] Keep, Discard, Relabel a few; Undo Last.
- [ ] Re-evaluate This Take (a small window appears on the desktop, a few seconds): any hitch in the headset? Then
      Re-evaluate Shown on a short list.
## Weapon posing mode

Your request: "a setup mode where the weapon model appears statically in front of the player, then I pose the hand
(per weapon) and offhands (per hotspot) as desired and I confirm the position by pressing a button on the other hand".
Branch `agent/posing`. Composites and logs are in the scratchpad's `posing/`.

| | What you get |
|---|---|
| Enter | Weapon Offsets > **Posing Mode** > **Pose This Weapon**, or a hotspot's **Pose This Hotspot** / **Pose a New Hotspot**; or `vr_pose` from the console |
| The weapon | floats still, 40 cm ahead of your head and 35 cm below it (chest height), level, pointing where you looked; drawn as it is held, with its hotspots, its muzzle (yellow), its barrel (green) and where your hand's shots would go (red) |
| Pose the weapon | the weapon hand is drawn at its controller and wraps the floating weapon live, as it will hold it; the **other** hand's A/X sets the weapon's place in the hand (Offset X/Y/Z, Pitch/Yaw/Roll) |
| Pose a hotspot | the weapon hand is drawn holding the floating weapon; the other hand poses the hotspot (Grip, Cup or Blade) and the **weapon hand's** A/X sets it (its point, a cup's palm point or a blade's share, and its Hand Pitch/Yaw/Roll) |
| Feedback | a click in both hands, the weapon pick-up sound, and a line in front of you saying what was set |
| Undo | B/Y undoes the last set, then the one before (every setting back to the exact text it had) |
| Leave | the menu button: back on the Weapon Offsets page (or to the game, if you started from the console) |

### Controls while posing

"The other hand" is the hand that isn't posing: the off hand while you pose the weapon with the main hand, the weapon
hand while you pose a hotspot. The text in front of you names its buttons.

| Button (the other hand) | Does |
|---|---|
| A / X | Set: what you see becomes the setting |
| B / Y | Undo the last set |
| Trigger | Next thing to pose: the weapon, hotspot 1, 2, 3, 4, the weapon... (the posing hand changes with it) |
| Stick click | Posing a hotspot: its type (Grip, Cup, Blade). Posing the weapon: the weapon back as it started |
| Stick | Turn the floating weapon: left/right spins it, up/down tilts it (about the middle of its grip and muzzle) |
| Menu (either hand) | Leave |

The posing hand's buttons, and the other hand's grip, do nothing: squeeze them freely (the posing hand's grip closes its fingers on the weapon, to see the wrap). You can walk round
the weapon; the sticks don't move or turn you while posing.

### In the headset, step by step

1. Hold the weapon to tune (in either hand), open the menu, go to Weapon Offsets for that hand.
2. Under **Posing Mode**, pick **Weapon Hand** (Main Hand, or Off Hand: mirrored; the settings are shared) and
   **Tuning Offsets on Confirm** (Keep, the default, or Set to 0: see below).
3. Click **Pose This Weapon**. The menu closes; the weapon floats in front of your chest.
4. Put your weapon hand on it as you want to hold it and squeeze the grip: the fingers wrap it as they will in play.
   Turn it with the other hand's stick to check it from the sides.
5. Press the other hand's A/X. You feel a click in both hands and hear the pick-up sound; the line in front of you says
   what was set. B/Y undoes it.
6. For the hotspots, press the other hand's trigger: now hotspot 1. The weapon is held by a copy of your weapon hand;
   put your other hand where it should hold it (a foregrip: where the hand is; a cup: your palm round the holding
   hand), pick the type with the weapon hand's stick click, and press the weapon hand's A/X. The trigger again for hotspot 2,
   and so on (a hotspot with no type becomes a new one).
7. Press the menu button: you are back on the page, the weapon in your hand, and play resumes. Hold it: hand and
   weapon are as you posed them.

### How it works (`vr_posing.cpp`, `vr_view.cpp` "Weapon posing mode")

- **The floating weapon** is the weapon hand's view entity, placed by the view's own weapon placement from a hand put
  where the weapon's current settings leave the weapon where it floats. It is drawn exactly as a held weapon is (skin,
  scale, hotspots, muzzle, ammo screen), and after a set it stays where it is while its settings change.
- **What a set writes is the placement's own steps undone:**
  - Held, the drawn hand's turn is the weapon's turn, times the weapon's angle offsets undone, times the hand's own
    (`attachedTurn`): `H = W · w0ᵀ · F`. With `H` the hand as tracked (the controller, as an empty hand is drawn) and
    `W` the floating weapon, `w0 = F · Hᵀ · W`; Pitch/Yaw/Roll come from `w0` (less `vr_gunmodelpitch`; yaw and roll
    mirrored for the off hand).
  - Offset X/Y/Z: the weapon's model origin seen from the hand's point, in the weapon's (mirrored) frame, over its model
    scale, less `vr_gunmodely`.
  - A grip hotspot's point is where the tracked hand's point is (where it is then taken from); a cup's, where the palm's
    middle is; a blade's, the share of the way from the hand to the tip. Its Hand Pitch/Yaw/Roll: the hand's turn with
    `helpingTurn` undone (the weapon's fixed-hand angles for a grip, the hand's own for a cup, mirrored for the main
    hand helping).
  - The grasp the posing hand shows is solved as the settings will put it (`Held::canonical`, at the candidate's place
    in the hand), so the fingers you see while posing are the fingers you get.
- **The tuning offsets** (Hand and Weapon Together, Hand Only, a hotspot's Held Hand). **Kept by default:** the pose is
  taken from the hand as tracked, and the posing hand is drawn with them, where play will draw it (Hand Only bends the
  drawn wrist on top of the grip; Hand and Weapon Together moves both from the controller). **Set to 0** zeroes them on
  a set, and the posing hand is drawn without them.
- **Mirroring:** posing with the off hand writes the same keys the main hand reads, mirrored as the off hand reads them.
- **Inherit From:** a weapon that inherits (the lava nailgun from the nailgun...) is posed with its own model, and the
  settings are written where the Weapon Offsets page edits them: the weapon it inherits from. Its own copy of each key
  written goes back to its default, so that both weapons hold the pose. To pose it apart, use "Stop Inheriting (Copy
  Them Here)" first. The text says whose settings are set.
- **A hotspot on a weapon whose two-handed use is Not Allowed** allows it, as the page does.
- **The game while posing:** nothing reaches the server but the finished settings. The moves carry the hands as they
  were when posing began (moving with you if you walk), with no speed, their buttons as they were, no two-handed aim,
  no teleport and no attack: no shot, blow, parry, grab, holster, throw or item touched. The buttons pressed while
  posing are the posing mode's; a grip held since before stays held for the game, so the weapon is still in your hand
  afterwards (with Weapon Grip Mode 0 too). Posing ends when the menu opens (any way), the map changes, you die, or VR
  stops.

### Held: exact relative to the hand; on the controller, as every weapon is

The hand and weapon you pose are, held, the same relative to each other at any controller angle (the drawn hand is
carried rigidly by the weapon). Where the pair sits on the controller is as before: the weapon's angle offsets are
Euler angles added to the hand's, so on a controller rolled 10° the pair sits about half a degree from where it was
drawn while posing (0.14° on a level one), more with large angle offsets on a steeply turned controller (numbers
below). Every weapon placement behaves so today; changing it would
move every tuned weapon and your takes' replays.

### Settings and commands

| | |
|---|---|
| `vr_pose_weapon_hand` (Weapon Hand) | 1 main (default), 0 off: the hand that holds the floating weapon and poses it; the other poses the hotspots and confirms |
| `vr_pose_reset_offsets` (Tuning Offsets on Confirm) | 0 keep (default), 1 set to 0 |
| `vr_pose [weapon \| 1..4 \| new \| stop] [main \| off]` | start (the weapon in the main hand, else the off hand's) or stop |
| `vr_pose_confirm`, `vr_pose_undo`, `vr_pose_next`, `vr_pose_type`, `vr_pose_turn <yaw> <tilt>` | the buttons, from the console |
| `vr_pose_check` | after leaving, holding the weapon: the hand against the last pose set |

### Tests (mock headset; `posing/` in the scratchpad)

Each: posed in the mock (the controller scripted to a pose on the floating weapon, the fingers closed), set with the
other hand's button, left, the weapon held (for a hotspot, the other hand taking it with its grip), `vr_pose_check`.
Units are world units (a 1.25 world scale: 26 to the metre).

| Case | Hand vs the pose (its rig) | Weapon's muzzle | Drawn palm (fitted) |
|---|---|---|---|
| Super shotgun, main hand (controller rolled 10°) | 0.0001 units, 0.0000° | 0.0001 | 0.0001 |
| The same with Hand Only (1.5, -0.7, 0; 8°, 0, -5°) and Hand and Weapon Together (2, 0, -1; 0, 6°, 4°) kept | 0.0001 units, 0.0000° | 0.0000 | 0.0000 |
| Off hand (mirrored) | 0.0001 units, 0.0000° | 0.0000 | 0.0000 |
| Lava nailgun (inherits the nailgun's): written to the nailgun, held as the lava and as the nailgun | 0.0001 units, 0.0000° (both) | 0.0001 | 0.0000 |
| Hotspot 2, Grip (off hand helping) | 0.0001 units, 0.0000° | 0.0001 | 0.0001 |
| Hotspot 2, Cup (off hand round the main hand) | 0.0000 units, 0.0000° | 0.0001 | 0.0000 |
| Hotspot 3, Grip, the weapon in the off hand, the main hand helping (mirrored) | 0.0001 units, 0.0000° | 0.0001 | 0.0001 |
| The floating weapon turned (60° spin, 20° tilt), the hand posed on it there | 0.0001 units, 0.0000° | 0.0002 | 0.0001 |

The drawn palm (after the grasp's palm fit) matching shows that the fingers in the preview are the ones held. The
same numbers hold with the controller then turned anywhere (tried 80–100° away from the pose). In the world, at the
same controller pose, the held hand was 0.009 units and 0.14° from where it was drawn while posing (a level
controller), 0.035 units and 0.57° (a controller rolled 10°), 0.07 units and 1.6° (with the offsets above kept), 0.46 units and 7.7° (the weapon turned, the controller turned 40° and rolled 15°, the angle offsets set to 23° of yaw): the
Euler angle offsets' quirk.

Undo: the six keys came back as the exact text they had ("0.0" stays "0.0"), and the offsets not touched stayed.

Replays of your 474 takes (`vr_motion_eval`, vrfiringrange): 420 of 474 pass, and the kit's eval finds 0 takes differing from the
baseline (`eval_posing.csv`). The
posing mode is off in them; the code they run through changed only in shape (the Hand and Weapon Together offset as a
function, the hand's drawing split out).

### Files

- `Quake/vr/vr_posing.cpp/.hpp` (new): the session, the buttons, set/undo, the text, the commands.
- `Quake/vr/vr_view.cpp`: the floating weapon, the posing hand and the solve ("Weapon posing mode"); `setupHand`'s
  drawing split out (`drawHand`), `showHotspots`' marks (`drawHotspots`), `vr_pose_check`.
- `Quake/vr/vr_hands.cpp/.hpp`: the Hand and Weapon Together offset as a function of a slot (the same arithmetic).
- `Quake/vr/vr_input.cpp` (the buttons and sticks while posing), `vr_client.cpp` (the moves while posing),
  `vr_main.cpp`, `vr_cvars.inc`, `vr_menu.cpp` (the Weapon Offsets page only).

### Limitations and not verified

- Mock only: not tried in the headset. The text's place (80 cm ahead, 20 cm below the eyes) and size are guesses.
- A blade hotspot is posed as a share along the blade only: the blade grip turns the hand itself, as before.
- Posing doesn't change the weapon's Scale, the finger tweaks or the muzzle: tune them on the page.
- Posing a hotspot, the weapon hand's copy is drawn closed as if gripping, whatever its controller does.

### In the headset

- Pose the shotgun with the main hand: does the grip feel the same when you then hold it? Try a tilted wrist.
- Pose it with the off hand (Weapon Hand: Off Hand), then hold it in the main hand: the mirror image?
- Pose a foregrip and a cup, then take them in play. Is the cup still round your hand?
- Is the text readable and out of the way? Is 40 cm ahead at the chest a good place for the weapon?
- Undo a few times; leave with the menu button; do the page's sliders show the new values?
## Box3D physics

Your request: two agents in parallel, one extending the current solver to stack, the other (this one) adding Box3D
and moving all the existing rigid-body physics onto it; you test both and choose. Branch `agent/box3d`. The code is
`Quake/vr/vr_box3d.cpp`; Box3D itself is vendored in `Quake/vr/external/box3d` (README with the upstream commit).
Composites and traces are in the scratchpad's `box3d/`.

**The switch:** Throwing and Physics > **Physics Engine**: Quake VR (`vr_physics_engine 0`, the default, the old solver
unchanged) or Box3D (`1`). It switches at once, live: at the next server frame the Box3D world is built and every
body made from its entity (where it is, how it is turned and moving, asleep or not), or destroyed (the entities
already hold every body's state, so the old solver just goes on from it). Saved games and level changes rebuild it
the same way.

### Box3D

- Erin Catto's 3D engine, the successor to Box2D: [erincatto/box3d](https://github.com/erincatto/box3d), MIT, version
  0.1.0 (alpha), commit `5643cd81` (2026-09-25). A shallow clone is a 2.9 MB download (the pack; 12.6 MB checked out); what is vendored (the library's
  sources, its public headers, the licence) is 2.3 MB.
- Built as C17 (`/std:c17`, no precompiled header) in the Visual Studio project (`quakevr.props`: `.c` files by
  wildcard, like the module's `.cpp`) and as a static library in the CMake build (`vr.cmake`; not built here: no
  CMake on this machine). The Release flags are the project's: `/O2`, `/GL` with link-time code generation,
  `/fp:precise`. SIMD is Box3D's own choice by architecture (`src/core.h`): SSE2 on x86-64 (the x64 baseline, so the
  build stays portable), NEON on ARM64 (the Quest's). No warning is added: one, C4756 ("overflow in constant
  arithmetic", whole-program optimization folding `1000 * FLT_MAX` in Box3D's CCD stall logger, a comparison with
  infinity that is meant), is disabled for Box3D's files only.
- **Single-threaded.** The world is made with `workerCount = 1` and no `enqueueTask`/`finishTask`: Box3D then takes
  its serial path (`physics_world.c`: `b3DefaultAddTaskFcn` runs each task inline and returns NULL, so there is
  nothing to finish). Its scheduler and threads (`scheduler.c`, `timer.c`'s `b3CreateThread`) are only reached with
  more than one worker. Nothing runs off the main thread.
- **Deterministic.** Box3D is by design (no FMA contraction, its own trigonometry); the module calls it in the same
  order every run (entities in edict order). Checked: two runs of the same script (`host_framerate` fixed, a pile of
  52 props toppling, a box thrown into it) give bit-identical states for every body at four points
  (`vr_physics_hash`); the motion eval below writes the same table on both engines.

### How it works

Once a server frame, at the end of `SV_Physics` (a new hook, `VR_PhysicsFrameEnd`), after every entity has thought and
moved:

1. **The world** (built once per map): a static triangle mesh of the world model's faces as drawn (8608 triangles in
   the firing range), sky and liquid faces left out, wound to face the open side, the BSP's shared vertices kept
   shared so that Box3D finds each triangle's neighbours (no bumps at inner edges). Clip brushes have no faces: props
   ignore them, as the old solver's point traces did.
2. **Brush entities** (`SOLID_BSP`: doors, plats, trains, buttons, walls, the mission packs' rotating brushes) are
   kinematic bodies made of their model's solid leaves (the BSP's hull 0 split into convex regions, each a convex
   hull; a door or a plat is usually one), placed each frame from their origin and angles with the velocity that gets
   them there over the step. So what rests on a lift rides it by contact, and `SV_PushMove` leaves Box3D's props alone
   (a second new hook, `VR_PushSkips`).
3. **Monsters and other solid boxes** (`SOLID_BBOX`, `SOLID_SLIDEBOX` with a size: monsters, the training dummy,
   exploding barrels) are kinematic boxes, Quake's, following them. **Players** (`vr_box3d_player_push`) are kinematic
   capsules of their body's width (`vr_box3d_player_radius`, 15 cm), feet to head. Kinematic bodies push props one
   way: nothing pushes them back. Missiles (no size) are left out.
4. **Props** (every `.vr_rigid` toss or bounce entity: thrown weapons, ammo and health boxes, backpacks, armour,
   gibs, heads) are dynamic bodies, one each, made or destroyed as their entity becomes or stops being one.
   Carried ones (in a hand, or both: the players' `mainhand_held`/`offhand_held`) are kinematic, following the hand,
   so a held box pushes others aside; let go, it is dynamic again with the throw's velocity and spin. A force grab's
   flight (noclip) has no body; caught it is held, missed it falls as a prop.
5. **QC in, Box3D out.** Box3D is authoritative for props. Each frame their origin, angles, velocity (the centre of
   mass's, as the old solver's), spin (`.vr_spin`) and sleep are written back to the entity; what QC changed since
   (a throw, a nudge, a knock from `T_Damage`, a force grab's drop, a teleport, `keepInWorld` putting a buried one
   back, `FL_ONGROUND` cleared) is seen against what was written and fed in, waking the body. Gravity is Quake's
   (`sv_gravity`, and each entity's `.gravity`: thrown things' true 9.81 m/s^2), in metres (`units::metresToUnits`).
6. **The step**: `b3World_Step` with `vr_box3d_substeps` (4) sub-steps, in pieces of at most 1/45 s.
7. **Touches**, after the step: a prop meeting a monster's or a brush entity's body touches it (`SV_Impact`: QC's
   damage and sounds, as the old solver's contacts with other entities); two props hitting each other faster than
   60 u/s touch each other (a thrown box into a pile: its knock, its throw over). Landing on the world touches
   nothing, as before.

### The old solver's behaviours, one by one

| Behaviour (`vr_rigid.cpp`) | With Box3D |
|---|---|
| An oriented box, the drawn model's bounds | The convex hull of the drawn model: an alias model's frame (weapons 32 vertices at most, the rest 16: blockier), a brush box's faces (the boxes as drawn, not Quake's padded box). A flat model (a hull thinner than a quarter unit) is its box |
| Mass: uniform density, unit mass | Mass from the hull's volume, a density per kind (weapons 700 kg/m^3, ammo and health boxes 400, armour 600, backpacks 250, gibs and heads 1000): only the ratios matter, what knocks what how far |
| Contacts: corners traced against the BSP and solid entities | Box3D's manifolds against the world mesh, brush entities' hulls, monsters' boxes, players' capsules, and every other prop |
| Sequential impulses, Coulomb friction `vr_throw_friction`, bounce `vr_throw_restitution` | Box3D's soft-step solver; the materials are `vr_throw_friction` (on everything: Box3D mixes `sqrt(a b)`) and `vr_throw_restitution` (on props; it takes the larger), updated when the settings change. Soft things (backpacks, gibs, heads) don't bounce |
| Split impulses: pushed out of surfaces, never flung | Box3D's contact softness: overlap is resolved at at most 3 m/s |
| Wedged in a gap: moved out the least way, or held still | Nothing special: Box3D resolves both sides at once; continuous collision keeps fast bodies out of walls |
| Spin drag `vr_throw_spin_drag` | Angular damping, the same rate |
| Rolling resistance in contact | The same (the spin dies away while touching, fast once slow; soft things twice as fast) |
| Static friction on slopes | Box3D's friction (a prop stays on a slope flatter than its friction angle) |
| Sleep: slow in floor contact for 0.3 s, `FL_ONGROUND`, lifts carry it | Box3D's island sleep (0.5 s under 5 cm/s); asleep is `FL_ONGROUND` and `groundentity` (the body under it: the world, a lift, another prop), awake clears it; lifts carry it by contact |
| Woken by QC's velocity or its support going | By any change QC makes (above), or by Box3D (something touching it, its support moving or going) |
| The hit box (`vr_throw_hitbox`) along a thrown thing's flight: monsters its thin corners would miss | The same sweep (`SV_Move` of the box from the centre of mass along the frame's move) before each step, then `SV_Impact` |
| Water: lift by how deep (60% under at rest, gibs sink), drag, drift, floating flat, the bob | The same formulas: the lift a force through the step (it and gravity balance at rest), the drag, drift, spin damping and turn to float flat on the velocity; floating things don't sleep (they bob). The liquids are the BSP's liquid leaves, sampled up the body's column |
| Splashes (the water transition) | The same: `SV_CheckWaterTransition` after each move, the splash hooks unchanged |
| Asleep under water deeper than it floats: lifted | The same, when it is made (a map's item under water, a saved game) |
| `keepInWorld`: a buried item back to its last free place | Unchanged (it runs before the dispatch; the put-back is fed in as a teleport). Also: a prop that falls out of the world (made inside a wall) stops there instead of falling for ever |
| Carried: moved by QC, not simulated | Kinematic: it pushes other props |
| Thin fast things through thin walls | Continuous collision against the world and brush entities; fast props (more than a fifth of their thickness a step) are bullets: continuous against other props and kinematic bodies too |

The rest is QC's and unchanged: throws (the velocity and spin QC sets), thrown weapons' and boxes' damage
(`forcegrabbable_touch`, `vr_throw_hit_min_speed`), gibs bursting on walls (their velocity turning sharply), carrying,
two-handed carrying and its throw, holstering, armour, force grab.

### Settings

| Menu | Cvar | Default | |
|---|---|---|---|
| Throwing and Physics > Physics Engine | `vr_physics_engine` | 0 | 0 Quake VR's solver, 1 Box3D |
| (console) | `vr_box3d_substeps` | 4 | Box3D's sub-steps a server frame (1..8) |
| (console) | `vr_box3d_player_push` | 1 | players' bodies push props |
| (console) | `vr_box3d_player_radius` | 15 | cm, that capsule's radius |
| (console) | `vr_debug_box3d` | 0 | 1: bodies made, moved by QC, put to sleep, hulls, slow frames; 2: every awake body every frame |

The Throwing and Physics page's Bounciness, Friction, Spin Drag, Hitbox and Hit Min Speed act on both engines.

Test commands (both engines: they only move entities): `vr_physics_stack <what> <n> <x> <y> <z> [<yaw> [<gap>]]`
(a column), `vr_physics_pyramid <what> <rows> <x> <y> <z> [<yaw>]`, `vr_physics_pile <what> <per column> <x> <y>
<z> [<spacing>]` (every one of them in toppling columns), `vr_physics_loose <what>` (a hanging armour or a pickup
made a loose prop, as a hand's knock makes it), `vr_physics_list [<what>]` (where they are, asleep or not, their
body, health), `vr_physics_hash` (a bitwise hash of every rigid body: determinism). `<what>`: an entity number, a
classname, or `props` (every rigid body). With `vr_rigid_place` (round 20) and `impulse 241`/`243`/`244`/`250`.
Note `vr_forcegrabbable_return` sends moved items back to their place after a while: 0 for long tests.

### Costs

`vr_profile` in exclusive runs (`run.sh --exclusive`), the mock headset, the firing range (52 props: 40 ammo and
health boxes, 12 armours), the server at 72 Hz. Milliseconds per server frame (the CSV's per-host-frame figures
divided by the server's 0.25 frames per host frame):

| | Quake VR (`rigid bodies`) | Box3D (`box3d`) |
|---|---|---|
| 52 props asleep, 6.4 s | 0.004-0.008 | 0.008 (sync 0.004, step under 0.004) |
| all 52 stacked in 6 columns of 9 that topple into a pile and settle, 4.4 s from the drop | 0.056 (they fall through each other to the floor) | 0.12-0.15 average over three runs (step 0.07-0.08, writing back 0.04, water and hit boxes 0.01, sync 0.004); worst frame 0.60-0.62 (the collapse) |
| the pile asleep, 8 s | 0.004-0.008 | 0.008 |
| `SV_Physics` in all (QC and everything) while settling | 0.14 | 0.21-0.24 |

Under the 0.2 ms target on average with 52 bodies all awake and colliding; the frame of the collapse itself (52 bodies
landing on each other at once, a few hundred contacts) is 0.6 ms. One run's first settle had a single 11 ms frame that
did not come back in three more (the machine's scheduling or a first allocation; `vr_debug_box3d 1` prints any frame
over 2 ms with Box3D's own profile and counts: none printed since). Box3D's memory for this map: the world mesh
(8608 triangles) and a hull per model. The Quest 3 renders; the server runs on the PC, as before.

### Tests (mock headset; scratchpad `box3d/`: each `<name>.png` is engine 0 over engine 1, its logs `<name>_e0.txt`, `<name>_e1.txt`)

The scripts are `compos.sh` (the composites), `t.sh`, `prof.py`; the positions below are `vr_physics_list`'s.

| Case | Quake VR (0) | Box3D (1) |
|---|---|---|
| `stack5`: 5 health boxes stacked (firing range), 0.3, 3 and 10 s | fall through each other into one layer | stand; asleep from the first second; every box where it was at 0.3 s at 10 s (to 0.1 unit) |
| `pyramid6`: 3 + 2 + 1 | a row on the floor | stands, asleep, unchanged at 10 s |
| `drop`: a shells box dropped level onto a stack of 4 | falls through to the floor | lands on top and stays (z 36.6 on the fourth box's 31.5) |
| `throw`: a shells box at 450 u/s into a stack of 5 | passes through | knocks the top two off (they land 23 and 48 units behind), rests on the other three |
| `carry.png`: a held box swept through a stack of 4 shells boxes (`vr_mock_play play_sweep.txt`) | passes through | knocks them over |
| `shotgun`: a gentle throw (`play_throw_weak.txt`, 145 u/s) | lands and settles on its side (roll -90) | lands and settles on its side (roll 80), 14 units shorter |
| `throwhit`: a shotgun thrown at a grunt 64 units ahead (`play_throw.txt`, 288 u/s) | kills it (30 health to -8: the hit box), bounces back off its box | kills it the same way (the same damage, -8); the grunt is flung first, the gun flies on and settles on its side |
| `slope`: a backpack and a loose green armour dropped on e1m1's 13-degree ramp | backpack on its side 14 units down; armour 13 units down | backpack on its back 9 units down; armour 8 units down |
| `lift`: a health and a shells box on e1m1's plat, a player stepping on | ride up (asleep, carried by `SV_PushMove`) and down | ride up (awake, carried by contact at 150 u/s) and down, 1-2 units of drift |
| `float4`: a backpack, a shells box and a health box into e4m1's pool | float at 60.3 / rising from a deep dive at 11 u/s / 71.5 | 60.1 / 13 u/s / 71.4 |
| `float2`: the same into e1m2's deep water | 161.0 / 171.5 / 171.6 | 160.2 / 165 (still rising) / 171.4 |
| `forcegrab`: a hanging armour pulled to the off hand and caught | caught, held | caught, held (kinematic) |
| force grab missed | falls, rests | the same place (it lands as Quake's toss before it is a rigid body, in both) |
| two-handed carry (`props2h/play_box.txt`: both hands, apart, roll, hand-over, both-hands throw) | the same messages, the throw at 78.8 u/s, spin 5.2 | identical |
| backpacks wedged in e4m1's 16-unit gap under the slab | still (0-1 u/s), none buried | still (0-1 u/s), none buried |
| save with a stack of 5 asleep, move one, load | - | the stack back as saved (to 0.1 unit), still asleep 5 s later |
| switching live: 1 -> 0 -> 1 with a stack standing | - | at 0 it falls into a layer (the old solver can't hold it); restacked at 1 it stands again |

**Determinism:** `det1_e1.txt`, `det2_e1.txt`: the same script twice (`host_framerate` 1/72, 52 props piled, a box
thrown into them): `vr_physics_hash` equal at all four points.

### Melee regression: your 474 takes

`map vrfiringrange; wait60; vr_motion_eval <copy of your motions> quit` (-Timeout 1200), on this branch:

- `vr_physics_engine 0`: **420 of 474 pass**, and the table (`eval_e0.csv` in the scratchpad) is identical line for line
  to the base's (the held-weapons round's `eval_base.csv`, vr-cleanup before this branch): every verdict, blow, frame
  and hand error.
- `vr_physics_engine 1`: **420 of 474**, and the table is identical line for line to engine 0's (`eval_e1.csv`). Your
  takes are melee against the dummy: no thrown or carried thing is in them, and the dummy is a kinematic box Box3D
  only reads.

### Files (and what touches the old solver)

- `Quake/vr/vr_box3d.cpp`, `vr_box3d.hpp`: all of it. `Quake/vr/external/box3d/`: the library.
- `vr_rigid.cpp`: five lines in `VR_RigidToss`, after `keepInWorld` and the first water check: `if(box3d::toss(ent))
  return 1;` (false with `vr_physics_engine 0`), and the include. Nothing else of the old solver changed: it merges
  with the stacking work (`agent/stack`) as long as that call stays before `rigidToss`.
- `sv_phys.c`: `VR_PhysicsFrameEnd()` at the end of `SV_Physics`'s entity loop, `VR_PushSkips()` in `SV_PushMove`
  (both declared in `vr_api.h`; both do nothing with engine 0).
- `vr_progs.cpp`: `box3d::reset()` with the other new-server resets. `vr_held.cpp`: `drawnVertices` (the drawn
  surface's corners, for the hulls). `vr_engine.hpp`: `sv_maxvelocity`. `vr_cvars.inc`, `vr_menu.cpp`: the settings.
- Build: `Windows/VisualStudio/quakevr.props`, `Quake/vr/vr.cmake`. Credits: `CREDITS.md`.

### Known gaps

- **Hands are not bodies.** A hand touching a prop gives it the hand's speed (QC's nudge, as before), and Box3D takes it
  from there: a hand can't hold a stack steady, press a box down, or catch a falling one by touch alone (a grip
  does). The held thing is a kinematic body and pushes.
- **Props still don't block anything in Quake.** Players, monsters, missiles and traces pass through them as before
  (`SOLID_NOT_BUT_TOUCHABLE`); only Box3D sees them. The players' capsules and the monsters' boxes push them aside
  (one way), and a monster's box is Quake's, much wider than its model: props rest against the air round a grunt.
- **Only rigid bodies.** Pickups hanging in the air, weapons on the range's racks, armour floating in place, stuck
  gibs, corpses are not bodies: props pass through them, as before.
- **Explosions** move only what QC gives a velocity (gibs; boxes and weapons take no damage), as before. Box3D's
  explosion impulse is not used.
- **The world mesh** is the drawn faces: T-junctions between faces (the BSP's) are not joined, so a sliding box may
  bump on some seams; not seen in the tests.
- **A prop made inside a wall** falls through (the mesh has no inside): `keepInWorld` puts it back where it was last
  free, as before; else it now stops under the map instead of falling for ever.
- **Soft things** (backpacks, gibs, heads) are rigid hulls without a bounce and with more rolling resistance; they
  can still come to rest on an edge where a real backpack would slump.
- **Frame time.** The world steps once a server frame by its time (1/72 s here), in pieces of at most 1/45 s; very
  low server rates (under 45 Hz) take more steps.
- **Box3D is 0.1.0, alpha.** No problem met, but untested at scale (large maps full of props, long sessions).
- **Not checked:** the CMake build (no CMake here), multiplayer (the server does it all, as the old solver did), the
  mission packs' rotating brushes (kinematic from their angles, like doors, but not tried), the headset.

### In the headset: the comparison

Play each with Physics Engine on Quake VR, then on Box3D (it switches at once; the firing range has the boxes, the
armour and a clear floor):

1. **Stacking** (the reason for all this): stack boxes by hand (grip, place, let go) in threes, fives; a pyramid. Quake
   VR: they fall through each other. Box3D: they should stand, still, for minutes, and wake only when touched.
2. **Knocking over:** throw a box into a stack; drop one on it; sweep a held box through it; walk into it (your body
   pushes, `vr_box3d_player_push`); push it with a hand. Does it topple plausibly, not explode or jitter?
3. **Piles:** drop a lot in one place (backpacks from `impulse 243`, boxes): a heap, not all at one height.
4. **Throwing weapons** at the range's grunts: the hits and the damage should read as before (same hit box); the gun
   should land and settle on its side, near where it did before.
5. **Bounce and friction:** the Physics sliders (Bounciness, Friction, Spin Drag) act on both; tell if Box3D feels
   livelier or deader, and which values you like for each.
6. **Slopes:** a backpack and armour dropped on a ramp (e1m1's): they should stay, not creep or roll.
7. **Lifts:** boxes on a plat ride it up and down (e1m1).
8. **Water:** boxes, backpacks, weapons floating (e1m2's big water, e4m1's pool and under its slab): the same depth and
   bob as before; gibs sink. Splashes as before.
9. **Carrying:** one and two hands, the hand-over, the throws (their spin), force grab (catch and miss), holstering and
   taking: all should be exactly as before, only now what you carry pushes other things.
10. **Wedges and corners:** boxes and backpacks in corners, under e4m1's slab, in the gap under its walkway: no shaking
    or spinning, no tunnelling into walls.
11. **Performance:** `vr_profile 2` while a pile settles: `box3d` under `SV_Physics`.

Tell me which you prefer and what either gets wrong.

## Carried box after loading a game

Found while testing two-handed props: load a game saved while carrying an ammo or health box (`vr_carry.qc`, one
hand or both) and the box is gone. The server still has it in the hand (`edict <n>`: its origin follows the hand,
letting go and gripping again work), but the client draws it nowhere. The base build (before two-handed props) does
it too, with one hand.

**Why.** The entity was sent and the client had it: it has its model, it's updated every frame, and `holdFrame`
(`vr_held.cpp`) placed it every frame. It was placed in the wrong spot:

- **One hand.** The client takes the box's place in the hand from where the server has it, for 0.1 s after the
  server says the hand holds it (`placeTime`: a caught box's jump to the hand can come a packet late), then keeps
  it. After a load, the carry stat arrives in the signon, at client time 0. The client clock then jumps to the saved
  time (3.49 s), so the 0.1 s window was already over on the first frame the box was placed. That frame was also the
  player's first in the world. The hands are computed before the entities are relinked (the move is sent first), so
  they were still round the world's origin (-5.6, 0.8, -31.7) while the box was at (304, -557, 69). The box was held
  647 units (about 20 m) off the hand for as long as it was carried.
- **Both hands.** The two-handed hold (the grips on the object, `carry2h::record`) is recorded once, on the first
  frame held in both. After a load that was 12 units (37 cm) too low. The hands' stair smoothing (`vr_hands.cpp`)
  eased the body up from the player's position before the server first placed it (0 after the client state is
  cleared). The body rose 12 units over the first 0.15 s, so the hold kept the gap, and the box was drawn 12 units
  high, at eye level and out of view. One hand was affected by this too, but its 0.1 s window re-took the place
  while the body rose.
- The held state (`holding`, `both`, the easings) wasn't cleared with the client state on a new map or a load,
  unlike the other client modules. Its times came from the old map's clock.

**Fix** (client only; no protocol, save or QC change):

- `held::resetClientState()`, called from `VR_OnClientClearState`: what the hands held is forgotten and taken again
  from the carry stats.
- The 0.1 s window starts on the first frame the place in the hand is taken, not when the stat arrives.
- Nothing is taken from the hands (a place in the hand, a two-handed hold) until the player has been placed in the
  world for two frames in a row (`playerFrames`): signed on, with its entity updated. Until then (one or two frames
  after a load) the box is drawn where the server has it.
- The stair smoothing doesn't start until the server has placed the player (`player.msgtime > 0`). The view, hands
  and body no longer rise 12 units over the first 0.15 s of every map and load.

### Checked (mock headset, firing range)

A health box placed in the main hand and gripped (the off hand moved out of the way first, see TESTING.md), then
also gripped by the off hand for the two-handed case; `save`, `load`, screenshots before and after; composites
`carrysave/b1.png`, `b2.png` (before) and `a1.png`, `a2.png` (after), in the round's scratch folder.

| | Before the save | After the load, before the fix | After the load, fixed |
|---|---|---|---|
| One hand | in the hand | not in view (placed 647 units off the hand) | in the hand, as before the save |
| Both hands | in both hands | not in view (12 units high); the drawn hands on their grips round nothing | in both hands, the grips' hold as before the save (the box at 304.6 -558.4 69.1, the server's place) |

- After the load, letting go drops the box (`carry: let go`); it falls to the floor and is drawn there.
- A box not carried when saving was drawn after the load before the fix too.
- Melee canary (`eval.sh`): 39/46 pass, the same as the baseline, no verdict or event differs.

### In the headset

- [ ] Carry a box (one hand, then both), save, load: is it in your hand(s)? Let go, grip it again.
