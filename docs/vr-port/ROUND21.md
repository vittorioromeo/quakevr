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
| After the posing test | a gun held into a monster hits it (a grunt's head: 0 of 6 pellets before, 6 of 6 headshots now); the posing hand passes through the weapon, unsolved (the solved grip shown for 1.5 s after each set); Show Controller at the real grip, shaped as a Quest 3 controller, with offset sliders; Shot Pitch / Shot Yaw per weapon turn where shots go |
| Particles, long sessions | particles made into quads on the GPU from one record each, uploaded once a frame: at 4x and 8x their CPU a quarter of before (8x: 0.64 to 0.17 ms), GPU 15-45% less, the same images; long sessions: old maps' text boards freed (VRAM), collision caches per map, haptic delays bounded; a 35-minute soak shows no growth |
| Body and weapon models | weapons: bands, bolt heads and ribs on the crudest spots (1.3-1.85x the triangles), edge wear in every skin, nothing tuned moved (anchors, hotspots, offsets and the hand fit identical); body: rounder limbs and torso, a belt, shaped feet (1240 -> 2186 triangles), 256x256 skins with a quilted vest, straps, laces, a face, armour lames |
| Hand calibration | Hand/Gun Calibration > Hand Calibration: each hand moved (X/Y/Z cm, along the controller) and turned (Gun Angle and Gun Yaw as its pitch and yaw, plus a roll) on its controller; the off hand mirrored or its own; everything held and the server move with it (2 cm moves them 2.0000 cm, 10° turns them 10.000°); Show Controller and Match Controller Preview to line it up; nothing changes at 0 |
| Grab reach, two-handed detach, brushing fingers | a thing is taken only if the fist touches it (it was 8 cm from the fist's front, past the open fingertips), Grab Distance Bias; a prop held in both hands drops when both are pulled off it (one: the other keeps it); the free hand's fingers rest on or bend out of the weapon they brush |
| Items as physics pickups | the map's weapons, keys, runes and suits hang spinning like the armour until grabbed, knocked or force-grabbed, then are Box3D props; a gripped weapon is yours at once, the rest are taken at a holster (Weapons and Keys, default on). Props no longer rest in the floor (the firing range's weapons: 2.8 units in on average, 19.9 at most; now 0.1 above); spinning pickups' physics shapes turn with the model |
| Arm IK with calibrated hands | the wrist judged against a real, lopsided range (ulnar deviation, which his Gun Angle 70 adds, is natural), the elbow swinging only past it or for a roll; the pole takes the hand's roll only: his takes swing the elbow >30° in 2.6% of frames instead of 24.3%, the same motion gives elbows 4.1 cm apart under the two calibrations instead of 8.6; wrist bends turn the gadget 0-5° (was 7-35°) |
| Dummy attacks | a DUMMY ATTACKS button beside the firing range's training dummy: it winds up (a sound, a glow, the rifle raised) and strikes you every 2.5 s as a knight would, for parry practice (parry, stamina, counters as in a fight); off at every map load; replays turn it off, and a take recorded with it on replays its blows at the same moments; your 471 archived takes evaluate identically |
| Melee fixes | a punch holding the torch lands (it never did); the free palm with the torch hand pushing is a two-handed shove, the torch hand alone no shove; axe, Mjolnir, sword and gun blows strike walls (10 of 24 test chops before, 23 now); gibs on the floor burst when punched or chopped; your 471 takes replay identically |
| Dynamic wounds | blood, burns and wetness painted into each monster's, corpse's and your own skin where the blow lands (chunky, on the skin's texels, Quake's reds), drying, cooling, healing; your body and hands no longer use the wound skins; 16 MB, about 2 µs of GPU a hit |

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
(Done later: [Hands remodelled](#hands-remodelled).)

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

(Since "Simplification: Box3D only, knights always drop swords", at the end, Box3D is the only engine: the switch
below is gone and the old solver is on the branch `archive/old-solver-stacking`.)

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
| (retired) | `vr_physics_engine` | - | was 0 Quake VR's solver, 1 Box3D; Box3D only since "Simplification: Box3D only" (below) |
| (console) | `vr_box3d_substeps` | 4 | Box3D's sub-steps a server frame (1..8) |
| (console) | `vr_box3d_player_push` | 1 | players' bodies push props |
| (console) | `vr_box3d_player_radius` | 15 | cm, that capsule's radius |
| (console) | `vr_box3d_player_push_speed` | 2.5 | m/s, the most a player's capsule shoves a prop at (Polish) |
| (console) | `vr_box3d_mesh_junctions` | 1 | the world mesh's T-junctions joined (Polish) |
| Carrying and Gibs > Show Physics Shapes | `vr_debug_physics_shapes` | 0 | Box3D's bodies as wireframes (Polish) |
| Carrying and Gibs > Show Hand Bones | `vr_debug_hand_bones` | 0 | the jointed hands' bones, grasp spheres, palm fit (Polish) |
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
  (`SOLID_NOT_BUT_TOUCHABLE`); only Box3D sees them. The players' capsules and the monsters' hulls push them aside
  (one way). (Polish, below: monsters are their drawn hull now, not Quake's box; a player shoves at most at 2.5 m/s.)
- **Stuck gibs and corpses** are not bodies: props pass through them, as before. (Polish: hanging pickups, rack
  weapons and floating armour are.)
- **Explosions**: (Polish) `T_RadiusDamage` throws the props it sees, Box3D only.
- **The world mesh**: (Polish) T-junctions joined.
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

### Polish

Your request: "Box3D is definitely the way forward ... optimize/polish it", and debug checkboxes on the grabbing menu
for the physics shapes and the hand's bones. Branch `agent/b3dpolish`. Scratchpad `b3dpolish/`: `prof.sh` (the
profile), `scen.sh` (the scenes), `hands.sh`, their logs and composites.

**Costs** (`vr_profile` in exclusive runs, the firing range's 52 props, `prof.sh`: asleep 6.4 s; all 52 in 6
toppling columns of 9 that fall into a pile, 5 s; the pile asleep, 8 s; piled again from the pile, 4.5 s). The same
script on the branch's start (built from it) and after, milliseconds per server frame (`box3d`):

| | before (5d82758d) | after |
|---|---|---|
| 52 asleep | 0.009 | 0.008 |
| collapse and settle: average | 0.097 (step 0.055, write 0.026) | 0.104 (step 0.056, write 0.028) |
| collapse: worst frame (its step) | 0.63 (0.56) | 0.40 (0.32) to 0.47; one run had a 1.9 ms frame in the teleport's `sync` (below) |
| piled again: average, worst | 0.084, 0.42 | 0.11-0.13, 0.34 |
| the pile asleep | 0.010 | 0.008 |

Run to run the averages move by 20-30% (other agents' games on the machine, the host's frame rate): within that the
average is unchanged; the worst frames are lower.

What the "0.6 ms collapse frame" and the "one 11 ms frame" were:

- **Continuous collision.** `vr_debug_box3d 3` (new: every frame over 0.2 ms with Box3D's profile) showed a third of
  a toppling pile's step in `continuous`: every box falling faster than a fifth of its thickness a step was a bullet
  (continuous against the other props). Now a third (a throw at 450 u/s still is one; a box falling off a stack of 9
  mostly not): the worst step 0.56 -> 0.32-0.47 ms.
- **Catch-up steps.** After a slow host frame (a console dump, a save) the server frame is long and was cut into up
  to 5 pieces of 1/45 s, each a full step: 9 pieces were seen after a hitch (with printing on). Now at most 3 (pieces
  of up to 1/30 s, still 4 sub-steps each).
- **Printing.** With `vr_debug_box3d 1` the first frame of a map took 110-200 ms: the console lines for each of ~150
  bodies made (with `-condebug`, a flush each). Without it no frame over 1 ms was seen (`developer 1` now prints any,
  with Box3D's profile: a map's load, a second map, piles, blasts). The 11 ms frame was most likely that, or the
  machine; it did not come back.
- **The world mesh**: 6 ms for the firing range as before, 37 ms with the T-junctions joined (below), once per map as
  it loads, and now kept across a saved game, `restart`, switching the engine off and on (only a new map or
  `vr_box3d_mesh_junctions` make it again).
- **Growing Box3D's arrays**: its world is made with room for every edict as a body and 4096 contacts (no reallocation
  in the frame a pile collapses); the step's touch list is kept (no allocation a frame).
- **Teleporting a pile** (`vr_physics_pile` moving 52 sleeping bodies at once): 0.5-1.7 ms in `sync` that one frame
  (each move wakes and re-inserts a body). Only the test commands do that.
- **Kept:** 4 sub-steps (Box3D's recommended; 3 would save a quarter of the solve, and stacks of 9 stand at 4); Box3D's
  sleep (0.5 s under 5 cm/s: a pile is asleep 3-4 s after it lands); `SV_LinkEdict` for every awake prop each frame
  (its triggers, as the old solver's toss does).

**The known gaps:**

- **Monsters' shapes.** A monster (an alias model with a solid box) is the convex hull of its drawn model at rest
  (frame 0, 24 vertices at most) turned with its yaw, not Quake's box: a grunt's 32-unit box was twice its body. One
  shape for every frame (a shape made again as it animates would lose its contacts and touch again each time).
  Tested: 3 cells boxes stacked 20 units from a grunt's middle stay there (Quake's box would have pushed them 8 units
  out); boxes dropped on its head slide off its shoulders. Brush boxes (barrels) keep their box.
- **Hanging pickups.** A pickup that is not a rigid body (hanging armour, the range's rack weapons: `FL_ITEM` and
  `SOLID_TRIGGER`) is a kinematic body of its drawn hull: props rest on it and knock against it instead of passing
  through. No touches (its touch is the player's pickup). A hand's knock still makes it a prop, as before.
- **Explosions.** `T_RadiusDamage` calls a new builtin, `physicsblast(origin, damage)`: every prop within its reach
  (damage + 40 units) that the blast sees (its middle or its top, as `CanDamage`) is thrown away from it, a little
  upwards, at 4 units a second per point (`damage` less half the distance) for a health box's mass, lighter things
  faster and heavier slower (the square root of the mass ratio, within half to twice), at most 600; through a point
  under its middle, so that it tumbles. A rocket (120) at a pyramid of 6 health boxes: 430-450 u/s each, blown apart;
  rockets boxes 90 units off: 250-300. Box3D only (engine 0 unchanged). `vr_physics_blast x y z [damage]` for tests.
- **The player's push.** The capsule still pushes props (props don't block players: solid in Quake's movement, piles
  would be walls, steps and traps, and every trace would change; not done), but at most at
  `vr_box3d_player_push_speed` (2.5 m/s): the rest of a run's move it jumps, and the props it then overlaps are eased
  out by Box3D's contact softness. Walking into a stack shoves it; running at it no longer kicks boxes ahead at 8 m/s.
- **World-mesh seams.** Each drawn face's edges are checked against the BSP's vertices lying on them (within 0.1
  unit), which are put into the edge: every seam is then an edge both triangles share, which Box3D's edge
  identification smooths (no catching). The firing range: 3020 junctions in 1413 of 4407 faces, 14454 triangles
  instead of 8608. A face with junctions is fanned from a corner with none on its two edges (else from its middle).
  The same step time (A/B with `vr_box3d_mesh_junctions 0`).

**Checked** (`scen.sh`, `hands.sh`): stacks of 4 and a pyramid of 6 stand and sleep; a shells box thrown into a stack
knocks it over; the 52 in columns of 9 fall into a pile (one column of 9 left standing); the grunt's hull and boxes
by and on it; a blast throwing a pyramid; the melee canary (`eval.sh`).

**Debug views** (Carrying and Gibs > Debug; cvars, off by default, not saved; off, each costs one test a frame):

- **Show Physics Shapes** (`vr_debug_physics_shapes`): every Box3D body's shapes as wireframes (hulls by their edges,
  capsules by their rings), coloured by what it is and does: props awake green (fast, continuous: white), asleep blue;
  held yellow; doors, plats, buttons purple; monsters orange; players cyan (your own faint); hanging pickups grey. A
  prop's centre of mass (a dot), an awake one's contact points (red: pressed in, pink: apart). Also each hand's grab
  probe (as `vr_debug_carry`: the thing's box, the nearest point of its surface, the reach; green in reach). Drawn
  from the local server's bodies: a server frame ahead of the drawn models (a unit or two when fast).
- **Show Hand Bones** (`vr_debug_hand_bones`): both jointed hands as drawn: the bones (the palm's middle to each
  knuckle, then each finger's three joints to its tip; thumb red, index orange, middle yellow, ring green, little
  blue), the joints (white); the spheres the grasp tests the hand as, against what it holds: green touching (within a
  quarter of a unit), yellow near, red sunk in (with a line to the nearest point of the held thing), grey nothing
  near; the palm's fit: its middle where the hand is (white) and where the grasp moved and turned it (cyan), the way
  the palm faces (the cyan stroke), and the grip channel (magenta: where a handle lies in the curled fingers). A pose
  that looks off: red spheres say where the hand is in the thing; a long white-cyan line, the palm pushed far to fit;
  green on one side only, the thing held off-centre.

**Not checked:** the headset; the menu page by hand (the cvars were); monsters' hulls for every monster (the grunt and
the dummy seen); the mission packs' explosions (they call `T_RadiusDamage` too); the push speed in play.

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

## After the posing test

Your four notes after testing the posing mode. Branch `agent/posefix`; composites and logs in the scratchpad's
`posefix/`.

### A. A gun held into a monster hits it

**Why it missed.** Pushed into a grunt's head, the gun is drawn stopped at the head (Weapons Stop at Models) while the
tracked muzzle, where shots start, is inside the grunt's box. A Quake trace that starts inside a box doesn't report it:
it comes back as a miss (`trace_fraction` 1) and drops what lay behind (the pellets flew on). Reproduced in the mock:
the shotgun's muzzle 11 units into a grunt's box at head height: **0 of 6 pellets on the grunt** (all on a
button behind it). Projectiles fared unevenly: nails and rockets starting inside the box already hit (the physics
reports a missile that starts inside a box), but a gun long enough to reach through it (the laser cannon) missed.

**The fix** (`QC/weapons.qc`):
- A hand's hitscan shots (shotgun, super shotgun) and the lightning gun are traced from **behind the muzzle, along the
  shot's own line**, from where that line passes the grip (`VR_ShotBackPoint`), the range lengthened by as much. What
  lies along the weapon's length is hit like anything past the muzzle; a wall there stops the shot there (a gun poked
  through a thin wall doesn't shoot from behind it); with a wall between the hand and that point, from the hand. A
  pellet whose trace still starts inside something that takes damage (the grip itself in a big box) hits it there.
- **Projectiles** (nails, lava nails, rockets, grenades, proximity and multi grenades, multi rockets, lasers, plasma):
  with anything between the grip and the muzzle they start behind it (`VR_ShotPlace`), and a monster there is met at
  its surface in the same frame (`VR_ShotImpacts`, at the end of `PlayerPostThink`). Flying from so close they would
  start inside the monster's box as grown for missiles (15 units) and be reported a frame's flight past it, the hit
  placed beyond the monster (a nail in the head counted as an arm).
- A projectile's touch traces back along its flight to find where it struck; started inside another box (the
  shooter's own, fired point blank) it reported that box (the laser's hit was placed on the player): now traced again
  from the projectile (`VR_TouchTrace`: nails, lava nails, lasers).

| Grunt, muzzle inside its head's box (`vr_debug_shots`) | Before | After |
|---|---|---|
| Shotgun (6 pellets) | 0 on the grunt, 6 on a button behind | 6 on the grunt, all headshots (gibbed) |
| Super shotgun, both barrels (14 pellets) | – | 14 on the grunt, all headshots |
| Nailgun | 9, 9, 9 (body) | 13.5 (head), then 9 as the grunt is shoved |
| Super nailgun | – | 27, 27 (heads) |
| Rocket, grenade, proximity grenade | explode inside it | explode at its face (107, 108, 83 damage) |
| Laser cannon (its muzzle past the grunt) | missed (hit the wall behind) | 18 + 18 (body) |
| Lightning gun | 30 | 30 |

Composite: `A_shot_in_head.png` (the gun in the head with its box and the shot line; firing; the result), before and
after.

### B. Posing without the solver

While posing, the **posing hand is drawn at its controller** (moved by its tuning offset if it has one, as before)
**with no grasp solve and no palm fit**: it passes through the weapon, and its fingers are **the controller's own
curls** (squeeze for a fist, open the hand to see through it), without the weapon's finger tweaks. So it can be put
exactly where it should hold the weapon. After each set (A/X) it **shows the solved grip for 1.5 s** (what play will
draw), then passes through again. Posing a hotspot, the helping hand is unsolved the same way (the weapon hand holding
the floating weapon still wraps it). What a set writes is unchanged: it was always taken from the hand as tracked.
`vr_pose_solve 1` solves live, as before. Normal play is untouched.

`vr_pose_check` after a set, held: the weapon (super shotgun): hand 0.0001 units and 0.0000° from the pose, muzzle
0.0001, drawn palm 0.0001; hotspot 1 (off hand): 0.0001 / 0.0000° / 0.0001 / 0.0001: as before the change. The drawn
palm is compared when the hand was seen solved where it was set (else it says so). Composites:
`B_posing_unsolved.png` (fist and open hand unsolved; the old live solve; the solved grip after A/X; unsolved again),
`B_hotspot_unsolved.png`.

### C. Show Controller where the controller is

**Why it was off.** The preview was drawn at the tracked controller pose. With `vr_controller_legacy_pose 1` (the
default, which the gun angles and weapon offsets are tuned for) that is not OpenXR's grip pose but the old OpenVR "raw"
pose made from it: for a Touch controller **10.2 cm further along the controller and turned 20.6°** (the front of a
Quest 2 controller's ring). So the box floated ahead of the hand.

**Now** it is drawn at the runtime's **grip pose** (OpenXR `grip/pose`, the middle of the handle, in the palm: the
legacy conversion undone, `TrackingState::gripInHand`), shaped as a **Quest 3 (Touch Plus) controller**: the handle
(8 × 3.2 × 3.6 cm), the head's oval face over it (level when the controller points ahead), the thumbstick on the
thumb's side, the trigger in front; its axes at the grip point (red along the handle, green left, blue up). The shape
follows the controller's proportions, not a scan: the sliders line it up.

**Sliders** (Weapon Offsets > Tuning Aids > **Controller Preview**, saved): Preview X / Y / Z (cm along its red, green,
blue axes), Pitch (up) / Yaw (left) / Roll; **Off Hand Preview**: Mirrors the Main Hand's (Y, Yaw and Roll the other
way) or Its Own (six more sliders). All extendable; they move only the preview. Cvars `vr_show_controller_x/y/z`,
`_pitch/_yaw/_roll`, `vr_show_controller_off_own`, `vr_show_controller_off_*`.

Checked: `vr_dumpview` prints each grip: 10.22 cm from the tracked pose with the legacy pose, 0 without; the grip's
pitch is 60.6° up when the aim is level (the handle leaning 30° forward). Composite: `C_controller_preview.png`
(before: ahead of the hand; after: in the palm; with offsets). The menu: `CD_menu.png`.

### D. Shot Pitch and Shot Yaw

There were none (Aim Pitch/Yaw/Roll are two-handed aiming's; Muzzle X/Y/Z moves where shots start). New per weapon
(`vr_wofs_shot_pitch_NN`, `vr_wofs_shot_yaw_NN`; Weapon Offsets, under **Muzzle** and under **Posing Mode**): they
turn the direction the weapon's **shots, projectiles and beams** go, in the aim's frame (pitch up, yaw left; the off
hand's yaw mirrored), **without moving anything drawn**. Inherit From, Print Changes and Reset cover them (ordinary
keys; no migration: 0 by default).

- End to end: the client turns each hand's aim by them (`weapons::shotAngles`) and sends it in the move
  (`VrMove::shotRot`); the server puts it in `.shotrot` / `.offshotrot`; QC `VRGetWeaponFireRot` returns it (every
  weapon's fire direction goes through it; the lightning gun used the hand's aim directly and now uses it too; a bot,
  given none, keeps its hand's aim).
- The red line (Show Controller Laser, the posing mode's), the crosshair and your own beams follow it.
- The melee reads the hand's aim and the weapon's geometry, not the shot direction.

Measured (`vr_debug_shots`, the shotgun): Shot Pitch 5 turns the shots' direction by **5.000°** (up), Shot Yaw 5 by
**5.000°** (left); in the off hand Shot Yaw 5 turns them 5.000° to the right. The drawn weapon entity's origin and
angles are identical in all of them. Composite: `D_shot_angle.png` (red turns, green, the barrel as drawn, doesn't).

### Tools

`vr_debug_shots 1` (with `developer 1`): each hitscan shot (start, direction, and what its pellets hit: target and
headshots, world, nothing) and each damage you deal (target, amount, inflictor, hit region and point). `vr_pose_solve`.

### Checks

- Melee: the canary of your takes after each change, compared with vr-cleanup's own results: no difference. Against
  the kit's baseline 16 of 471 takes differ; **vr-cleanup 5d82758d alone gives the same 16** (all 471 takes identical
  to this branch): they come from its new weapon poses (gun butt and muzzle strikes, the no-hit set), not from these
  changes. Merged with vr-cleanup 71fe2aa9 (melee2) since: the canary is again identical to vr-cleanup's own (40/46
  pass), and the shot into the grunt's head still 6 of 6 headshots.
- Mock only: not tried in the headset.

### Limitations and not verified

- The preview's shape is a close guess at a Quest 3 controller; the grip pose itself is the runtime's.
- A gun poking through a thin wall was not reproduced (the hand's wall collision keeps the barrel out); the shot now
  starts on the hand's side of the barrel, so it meets the wall.
- A monster that walks in until your grip is inside its box: the hit still counts, placed at the grip.

### In the headset

- [ ] Hold the shotgun into a grunt's head and fire: a headshot. The same with the nailgun and the rocket launcher.
- [ ] Posing: the hand passes through the weapon; place it, press A/X: the solved grip shows for a moment.
- [ ] Show Controller on: is the preview in your palm where the real controller is? Tune Controller Preview until it
  matches, and tell me the numbers (they can become the defaults).
- [ ] Show Controller Laser on: turn Shot Pitch / Shot Yaw until the red line runs through the sights.

## Flashlight tuning

Your three notes after the flashlight's last pass. Branch `agent/flash5`. Composites in the scratchpad's
`flash5/final/`.

| Your note | What you get |
|---|---|
| The weapons' manual finger options for the torch, both grips | **Low Grip: Fingers on the Torch** and **Overhead Grip: Fingers on the Torch** |
| An offset for the head's attach and detach spot (a bit too low) | **On the Head > Head Zone Forward/Up/Out/Radius**; raised 4 cm by default |
| Is the gun's attach spot customizable? | Where it sits once clipped: **On a Gun > On Gun Forward/Up/Out** (they were there). Where it clips on: **On a Gun > Gun Zone Along/Up/Out/Radius** (new) |

All of it is on **Settings > Flashlight** (`menu_vr 15`). The page now has headers: On the Belt, In the Hand, the two
grips and their fingers, Reach Zones, On a Gun, On the Head.

### Fingers on the torch

Per grip, the same options as a weapon's Fingers on the Weapon:
- **Fingers:** Automatic (they wrap the torch) or Manual (each finger at its curl, no fitting).
- With Manual: Thumb Curl, Thumb Across, Index/Middle/Ring/Little Curl (0 open, 1 a fist). The controller's grip still
  opens the hand and closes it up to them.
- With Automatic: Overlap (0..1 of a centimetre; 0.3 as before).
- Either way: the five finger tweaks (Thumb, Index Finger...: -1..1 of a full curl) and Thumb X/Y/Z (±4, extendable to
  ±20).

Details:
- The manual curls start at what the automatic grasp does at a full grip (its joints' mean stops, measured with
  `vr_debug_grasp 2`): switching to Manual keeps the look. The palm isn't fitted in Manual (as for a weapon), so it sits
  1.1 cm (low) and 2 cm (overhead) off the automatic place; the grip's Forward/Towards Palm/Up move it.
- During the flip's spin the fingers keep the old grip's settings (the grasp holds the old grip); the tweaks and the
  thumb's place go over with the spin.
- The other hand mirrors them. Cvars: `vr_flashlight_low_*` and `_high_*`: `fingers`, `curl_thumb/index/middle/ring/
  pinky`, `thumb_across`, `overlap`, `bias_*`, `thumb_x/y/z`.

### The reach zones

**Show Flashlight Zones** (Reach Zones; `vr_show_flashlight_zones`, not saved) draws them:
- the head's: yellow balls at both temples and the forehead;
- each gun in a hand: an orange capsule round its line, from the hand to 3 cm past the muzzle;
- the held torch's middle (white): that is what they measure.

A zone turns green while in reach (B/Y clips the torch on), and so does the torch's middle. From your own view the
head's balls are round your eyes and aren't drawn; with the body's preview (`vr_body_debug 2` or `3`) they are drawn
round the preview's head too.

**The head** (On the Head, after On Head Forward/Up/Out, which still place the mounted torch):
- **Head Zone Forward/Up/Out** move the balls (Out: to the side at the temples, ahead at the forehead), ±20 cm,
  extendable. **Head Zone Radius**: 10 cm.
- **Up defaults to 0.04: 4 cm higher than before.** The temples' balls are now 7.5 cm over the eyes, the forehead's
  10 cm. With the 10 cm radius, the torch at your temple's upper edge, where a head torch's strap runs, is well in.
- The same zone takes it off: a hand at the head torch (at the torch as before, or its fist in that temple's ball)
  with B/Y (back to the belt), or the grip (into the hand).

**The gun** (On a Gun):
- **On Gun Forward/Up/Out** (already there) place the torch once clipped on.
- **Gun Zone Along/Up/Out** (±20 cm, extendable) move the zone where holding the torch clips it on, in the gun's frame:
  along the barrel, up, and out (away from your body). **Gun Zone Radius**: 12 cm, as before.
- Taking it off the gun is unchanged: the free hand at the torch, B/Y.

### Tests (mock headset, e1m1)

- **`final/fingers_off.png`, `final/fingers_main.png`:** each hand, each grip, from the side and the front:
  Automatic; Manual at the default curls (close to Automatic); Manual with index 0.1, middle 0.35, thumb 0.4, across
  0.1 (those fingers open); Automatic with the index and little tweaked -0.7 and Thumb X +3.
- **`final/zones.png`** (spectator camera, zones shown), the log's lines in brackets:
  - the head's three balls; the torch held at the left temple: green, Y ("on the head, the left temple"); on the head
    only that temple's ball; a fist in it green, the grip ("off the head, into the hand");
  - Head Zone Up 0.25: the torch at the old place is out of reach (Y flips it); 21 cm higher it is green and Y puts
    it on; Y at it with an open hand ("off the head, back to the belt");
  - the shotgun's capsule; the torch under the barrel green, Y ("clipped on the main hand's gun"); Y at it ("off the
    gun, back to the belt");
  - Gun Zone Along 0.35, Radius 0.08: the old place out of reach (Y flips); 35 cm further forward green, Y clips it,
    and it sits where On Gun puts it, as before;
  - the body preview with the balls round its head.
- **`final/menu.png`:** the new sections, and Fingers set to Manual rebuilding the page with the curls.
- After merging vr-cleanup (b793e23d): the melee canary (`eval.sh`) 40/46 pass, no take differs from the baseline;
  the zones and the off hand's fingers run again, the same.

### Also fixed: the belt's torch with a gun in the other hand

Testing the gun zone found this: with a gun in the other hand, the torch on the belt could not be taken (`developer 1`:
"torch press ignored: the off hand's is the game's (hotspot 1)"). The game's two-handed grip hotspot is anywhere
13-64 cm from the other hand, and the intent gate let the game have the grip there. But a gun held by its fitted
grips is only taken at its grip (within 5.5 units, `vr_twohand.cpp`), so the grip did nothing at all. Now, for such a
gun, the game wins only within 8 units of its grip. Swords and the other free two-handed holds keep the old rule, and
a hand at the other hand's weapon still leaves it to the game. The ignored press now says why under `developer 1`.

### In the headset

- [ ] Flashlight page, each grip: Fingers Manual, then move the curls; the tweaks and Thumb X/Y/Z in both modes.
- [ ] Show Flashlight Zones, the body preview (`vr_body_debug 2`): the head balls where you expect? Tune Head Zone
      Up until clipping it on feels natural; tell me the value.
- [ ] Hold the torch by a gun: the capsule green, B/Y clips it. Gun Zone Along/Up/Out if it's off.
- [ ] A gun at the ready in one hand, take the torch from the belt with the other: it comes (it didn't before).
## Particles at high multipliers; long sessions

Your notes: with the particle multiplier at 4, shooting a wall slows the game noticeably; and the game seems to slow
down a little over time, even with VDXR. One commit for the particles, one for the long-session fixes, one test cvar.
Measured with `run.sh --exclusive` and `vr_profile`, at 1448 x 1448 an eye (twice the mock's pixels), both eyes, on an
RTX 4090; scripts and images in the scratchpad's `perf2/`.

### Particles: where the time went

The scene: the firing range, facing the wall behind the start; at each multiplier 12 super-shotgun blasts, 3 s of
super nailgun, 6 rockets (`vr_particle_seed 7`: the same particles every run). At 4x there are 2000-5000 particles
alive, at 8x 6000-10 000.

- **CPU: the quads, made for each eye.** Every particle's six 40-byte vertices were made twice a frame (after a
  frustum test) and copied into the upload buffer twice. `particle verts` was 70% of the particles' CPU, the upload
  15%, the simulation 10%: 0.64 ms a frame at 8x.
- **GPU: mostly pixels.** The smoke puffs grow to 45 units across near the wall, and soft particles read the scene's
  distances at each pixel. The rest is the vertices: 12 per particle a frame, read at a 40-byte stride.
- **Spawning** is not the problem: `particle spawn` (new) is 0.001-0.002 ms a frame on average and 0.16 ms at
  worst, for a super-shotgun blast at 8x (about 4000 particles).

**The change.** Each particle is now one 96-byte record, made and uploaded once a frame and drawn in both eyes from the
same buffer. The record holds the position, size, colour, velocity and streak, the angle's cosine and sine, the
softness and the cell.
- The vertex shader builds the same quad from each eye's camera, with the same arithmetic the CPU used: turned by its
  angle, flat on the ground, streaked along its motion, pulled towards the eye for soft particles.
- The draw order, the blend and the texture are unchanged.
- The rings and foam lying on the waves stay on the CPU, per eye (they follow the waves).
- An eye skips the draw and the scene's distances when the box round all the particles is out of its view.
- Code: `vr_gfx`'s `uploadParticles` and `drawParticles`. The upload buffer is bound as an SSBO; binding 0 is borrowed
  and put back (`GL_GetShaderStorageRange`, gl_rmisc.c).

| Scene | Frame CPU before / after | Eyes' GPU before / after | vr particles CPU before / after | vr particles GPU before / after |
|---|---|---|---|---|
| 1x super shotgun | 0.821 / 0.708 | 0.556 / 0.540 | 0.030 / 0.022 | 0.036 / 0.052 |
| 1x super nailgun | 0.877 / 0.713 (B2) | 0.583 / 0.577 | 0.039 / 0.029 | 0.081 / 0.085 |
| 1x rockets | 0.926 / 0.803 | 0.577 / 0.565 | 0.056 / 0.029 | 0.045 / 0.045 |
| 4x super shotgun | 1.041 / 0.812 | 0.574 / 0.547 | 0.169 / 0.074 | 0.096 / 0.081 |
| 4x super nailgun | 1.020 / 0.796 | 0.723 / 0.698 | 0.190 / 0.083 | 0.232 / 0.220 |
| 4x rockets | 1.207 / 0.815 | 0.683 / 0.609 | 0.304 / 0.086 | 0.141 / 0.096 |
| 8x super shotgun | 1.406 / 0.861 | 0.808 / 0.650 | 0.588 / 0.147 | 0.332 / 0.178 |
| 8x super nailgun | 1.435 / 0.887 | 1.120 / 0.935 | 0.637 / 0.168 | 0.633 / 0.453 |
| 8x rockets | 1.490 / 0.895 | 1.090 / 0.907 | 0.652 / 0.163 | 0.505 / 0.356 |

(Milliseconds a frame; averages of two runs each, alternating before/after/before/after, and a third run of the final
build agreeing within 0.01 ms. `Frame CPU` is the whole frame. With no particles, the frame is about 0.7 ms and the
eyes about 0.52 ms. One 1x nailgun run of the new build had a 220 ms hitch in the scene's own drawing (not the
particles'; a shader made on first use, it seems), so that cell is the other run's.)

- **At 4x**, shooting the wall now adds 0.1 ms of CPU a frame over 1x (0.3 before, with rockets), and 0.03-0.17 ms of
  GPU.
- **At 8x** it adds 0.15 ms of CPU (0.6-0.7 before) and 0.1-0.4 ms of GPU (0.3-0.6 before).
- The particles' CPU is a quarter of what it was at every multiplier, and the upload a tenth.
- The GPU gains 15-45% at 4x and 8x (the vertices). What is left is the pixels, which is the look: the same puffs
  cover the same pixels.
- On this machine the cost at 4x wasn't big before either (0.3 ms CPU plus 0.15 ms GPU). If your frame is already near
  the headset's budget, a slower GPU and more pixels (a Quest 3 at your resolution, supersampled) make the GPU part
  several times bigger. If 4x still slows down for you, `vr_profile 2` while shooting shows whether it is `vr particles`
  (and its GPU column) or something else.
- **Images:** the same, seeded, before and after: the wall shot with the super shotgun, the nailgun and a rocket, a
  grunt shot, e1m2's water splashes, e1m1's rocket. The differences are single pixels at sparks' edges, as between two
  runs of the same build.
- `vr_particle_seed <n>` (new, for tests): the particles' random numbers restart from this seed at each map.

**Not changed:**

- The simulation: 0.06 ms at 10 000 particles.
- Sorting: there is none. They are drawn in the order made, which the premultiplied blend relies on (ROUND19.md).
- Lighting per particle: they aren't lit (their colours are the palette's).
- Traces per particle: none (only a drop falling into a liquid looks its surface up).
- Fewer or smaller particles far away: would change the look.

### Long sessions: the audit

I checked every container, cache, pool and GL object in `Quake/vr/*.cpp` and the engine hooks it adds for growth
without bound, missing eviction, or growth across maps. Nothing leaked without bound. Fixed (one commit):

| Where | What grew | Fix |
|---|---|---|
| `vr_text3d.cpp` (world-text boards) | once made, each board's image (up to 8 MB of VRAM; the firing range has 19) was kept for the rest of the session, and drawn again while the next map loaded | a board of an earlier map has its image freed (`gfx::releaseTarget`) and isn't drawn; it is made again when its map comes back |
| `vr_modelcollide.cpp` (`modelTris`, `samplesCache`) | the models' triangles and samples were never forgotten; a brush submodel (`*N`) was matched by name on the next map, and another game's model in a reused slot could have fewer vertices than the cached triangles index | forgotten at each map, with the posed cache |
| `vr_input.cpp` (`pendingHaptics`) | a NaN or huge delay from the server never came due, and the haptic stayed | the delay is clamped to 0..10 s; at most 64 wait |
| `vr_fgfx.cpp`, `vr_view.cpp` | pulses from `realtime` as a float: steps of 4 ms after 10 hours | worked out in double |

Checked and bounded (no change):

- **Particles:** the pool (32 768, reserved once); the frame's records and lying pieces (grown to the most at once);
  the splash plinks (48 a frame).
- **Decals:** a ring of `vr_decal_max`; the gibs' and holes' lists are pruned every 100 frames and emptied at each map.
- **Gore:** rays 320, sources 48, landings 192, pending 32. Body blood: 64 drops. Casings: 64 + 16.
- **Ambient and model-light caches:** at most 2048; entries older than 100 frames are evicted; emptied at each map and
  game change.
- **Grasp shapes:** by held model and frame (a few dozen). **AO:** the baked occlusion by model name grows only with
  the models seen (a few MB with id1).
- **Text3d:** floating texts 256, expiring; the frame's vertex buffers are cleared each frame. The gadget's queue 16,
  the hologram's 6. Lines and debug: cleared each frame.
- **Motion recorder:** a 60 s cap, the preroll trimmed. Voice notes: 180 s.
- **Per-entity data** (`entityData`, the posed caches, the rigid memos, carried): by entity number, emptied at each
  map.
- **GL objects:** every creation reuses or deletes the old one (render targets, static triangles, envmap, haze, water,
  bloom, upscalers); programs, atlases and queries are made once.
- **`GL_Upload`'s ring** grows to the most one frame needs (in 1.5x steps) and stays there. 32 768 particles would now
  need 3 MB of it (15.7 MB before).
- **Sound:** the client's own precaches are fixed names (found again, not added); the water sounds are a table
  refilled at each map.
- **The profiler:** one node per (parent, literal name); the GPU query pool grows to the most scopes in a frame.
- **Box3D** (read only, reported): bodies are destroyed and made again per entity slot, and the whole world at each
  server spawn. `propHulls`' keys include the prop's drawn size, so a prop whose size keeps changing adds a hull for
  each size until the next map.
- **Small, left as is:** `vr_gpustats.cpp`'s process names (one per process seen using the GPU, only while the memory
  log is on). The gadget's and text3d's `fmod(realtime, 1000)`: the CRT's noise jumps once every 16 minutes.

### Long sessions: the soak test

The mock headset, not exclusive (other agents' games ran alongside), at 90 fps: e1m1, e1m2, the firing range, over and
over. On each map:
- 5.5 s standing still, profiled (the frame time);
- then grunts spawned and rocketed, the super shotgun and nailgun at walls, and walking.

The multiplier was 4, `vr_decal_max` 512, and the memory log wrote a row every 20 s.

Two runs: before the fixes (35 minutes, 34 map loads) and after them (19 minutes, 24 map loads; the second run
had more of the other agents' games alongside, so its GPU times are noisier). Each map is compared with its own
earlier visits: its second visit (the first is the session's start) against its last.

| | Before: 2nd visit / last | After: 2nd visit / last |
|---|---|---|
| Private memory, e1m1 (MB) | 1225.7 / 1237.8 | 1089.1 / 1098.7 |
| Private memory, e1m2 (MB) | 1215.7 / 1238.7 | 1085.6 / 1086.7 |
| Private memory, firing range (MB) | 1230.3 / 1237.5 | 1109.0 / 1118.9 |
| GL textures / buffers / framebuffers, e1m1 | 609 / 211 / 49 and 610 / 211 / 50 | 589 / 211 / 30 and 591 / 211 / 31 |
| GL textures / buffers / framebuffers, firing range | 555 / 211 / 49 and 556 / 211 / 50 | 555 / 211 / 49 and 556 / 211 / 50 |
| Idle frame CPU, firing range, median of the first / second half of the visits (ms) | 0.854 / 0.864 | 1.072 / 0.954 |
| Idle eyes' GPU, firing range, same (ms) | 0.439 / 0.457 | 1.683 / 1.424 |

- **Nothing grows much.** Memory settles within the first two or three cycles. Before the fixes it then crept by
  about 20 MB in 30 minutes (e1m2: 1216 to 1239 MB over 10 visits; the firing range: 1230 to 1238). After them it
  stays within 10 MB of its second visit, with no trend over 19 minutes (the run was shorter). The GL objects are the same on every visit to the same map, give or take one or two
  made once (the hologram's target, an ammo screen's).
- **Frame times** show no trend from the first half of the run to the second, in either run. Per map, the CPU is
  0.75-0.95 ms before and 0.93-1.07 ms after (the machine was busier during the second run); the GPU goes up and down
  with the other games.
- **The fix shows:** after it, leaving the firing range frees its boards' images (framebuffers 49 to 30 on e1m1 and
  e1m2, 20 fewer textures). Private memory is also 110-140 MB lower through the second run than the first, on every
  map (not investigated further).
- The CSVs are `perf2/soak_base_memstats.csv` and `soak_fin1_memstats.csv`; the tables per map visit are
  `soak_base.tab` and `soak_fin1.tab`.

### Not verified

- In the headset: 4x and 8x shooting a wall (`vr_profile 2`). The particles should look exactly as before.
- The slowdown you feel over time. The soak shows no growth in the game: memory, GL objects and frame time stay flat
  over 35 minutes before the fixes and 19 after. If it persists, compare the memory log's `xr_*` columns with `busy_ms` and `gpu_eyes_ms` over a
  long session: that says whether it is the runtime (TESTING.md, "If it gets slower the longer you play").

## Leaning; menu opacity

Your notes: leaning your head a bit moved the legs, and leaning didn't seem to work at all; and a slider for the
menu's background opacity, to see the game behind it while tuning graphics.

### Leaning

**What was wrong.** The collision box did let the head lean off its middle (Lean, `vr_lean_radius`), but:

- the drawn body ignored it: the pelvis and the feet were placed under the head, so any head motion carried the whole
  body, and 25 cm of it made the feet step;
- the box slid back under the head at Lean Recentre (0.5 m/s) all the time, so a held lean was undone within a second
  anyway (its feet then stepping after it).

**Now.** Where the body stands is the box's middle: the feet stand under it, and the head's offset from it is the lean.
The back tilts from the hips towards the head, as far as the head has gone down for it (a tilt swings the head down on
an arc about the hips); a fifth of the lean is always in the hips, and a lean with the head kept high is the hips
shifting (the legs slant). The feet step only when the box moves more than 25 cm from them.

Whether the box follows the head (you walk) or stays (you lean) is told by the cues (`senseLean`, `vr_hands.cpp`):

| Cue | A lean | Walking in the room |
|---|---|---|
| the head's drop below its standing height (learnt while you stand over the box; the calibration only seeds it), against the drop a lean this far out would give (an arc of 0.43 x eye height about the hips) | lower, about as the arc says (5-6 cm at 28 cm out) | keeps its height (a walk's bob is smoothed out) |
| the head's tilt towards the offset (roll sideways; pitch forward counts half: you look down walking too) | tilted 5-15 degrees or more | level |
| the hands' middle, learnt while the head is over the box: how far it went along with the head | hanging hands stay by the hips (a quarter as far) | the hands go along |
| speed and duration | the first instants show few cues; a slow lean drifts | keeps going at a walking pace (above 0.15-0.35 m/s) |

Any one cue can tell (weighed together, then times Lean Detection), smoothed quick to rise and slower to let go (0.08 s,
0.35 s): the lean's hold. Within the Lean radius the box then stays while you lean, and otherwise catches up with the
head, closing the gap in about a quarter of a second (never slower than Lean Recentre); while the head moves, only once
it has kept going for 0.12-0.35 s at a walking pace. Past the radius the body follows the head as before, and at a wall
the head stops there as before. The torso's tilt also goes with the hold: walking, the body lagging the head stays
upright, the hips with the head, and the feet catch up.

**Settings** (VR Settings > Body and Movement > Body, and Advanced > Locomotion):

- **Lean Detection** (`vr_lean_detect`, new, default 1): 0 off (the box always slides back under the head at Lean
  Recentre, as before; the drawn body still stands at the box); higher, more readily a lean (up to 2).
- **Lean** (`vr_lean_radius`, now also on the Body page; yours is 12 in the baseline config): how far the head may lean
  off where the body stands before the body follows.
- **Lean Recentre** (`vr_lean_recenter`, 0.5): now the least speed the body catches up at (0: never).

### Checked (mock headset, `start`; scripted head and hand paths with head tilt, `vr_debug_lean 1` traces)

Scratch folder `lean/` (the motions `lean.txt`, `leanaim.txt`, `walk.txt`, `crouch.txt`, `wall.txt` from `gen.py`;
`out/plot_*.png` the head, box, pelvis and feet over time with the cues, before and after; `out/comp_*.png` the body
from a spectator camera, before and after):

| Motion | Before | After |
|---|---|---|
| leans left, right, forward (28-30 cm, head 5-6 cm lower, tilted 16-18 degrees, hands hanging), held 2 s, and back | the box followed the head within 0.1 s; the feet stepped at every lean and every return (14 steps) | the box stays (at most 7.6 cm off, over the lean's first instants); no step; the back tilts from the hips over the feet |
| a slow lean (3 s in) | as above | the box 7 cm off, no step |
| the same leans aiming (the hands with the head) | as above | the box at most 11 cm off (the drop and tilt alone tell), no step |
| walking 0.8 m forward, 0.5 m left, back, a 15 cm shuffle | feet stepping after the head, the first step 0.4 s in | the same (12 steps), the first step 0.64 s in; the body upright, the hips with the head |
| crouching (head to 1.05 m, 12 cm forward), then leaning left crouched | the pelvis and the box went with the head | the box and feet stay; after standing up the box comes back under the head in 0.3 s |
| strafing into a wall with the stick, leaning into it, walking 0.8 m into it (Lean Detection 0 and 1) | - | the box stays against the wall; the head stops 0.43 m off it (the radius, 0.46 m at world scale 1), as with detection off |

Known: walking into a wall in the room, the head held off by the radius, the body stays upright (the hips under the
head, the legs slanted to the feet at the box), as before.

Melee (`eval.sh`, after merging vr-cleanup b793e23d): the canary differs in 2 takes, so the full set ran: 429/471 pass
(the baseline 426), 13 takes differ, none from pass to fail: 3 gun-butt strikes now pass, and 10 keep their verdict with
other strengths (pommels, straight punches, a stab). With Lean Detection 0 the canary is identical to the baseline:
the difference is the box catching up faster while the head moves at a walking pace with no lean cues (a strike
stepped into), where it used to follow at 0.5 m/s.

### Menu background opacity

**Menu Background Opacity** (`scr_menubgalpha`, 0 to 1, 0.7 as shipped: today's look), on the Menu settings page (with
the distance, scale, spacing and height) and in VR Settings > Display under Menu Scale. It is Ironwail's own menu
background alpha, so the desktop menus follow it too (Ironwail's Interface > BG Alpha is the same setting). Below 0.55
the menus' text gets a dark outline a glyph pixel wide (`draw_textoutline`, `gl_draw.c`), stronger the fainter the
background (full at 0.1), so that it stays readable over the game; the box and slider glyphs are left as they are.
Checked in the mock (`out/menu_opacity.png`, `out/menu_zoom.png`): the VR panel and the desktop Options menu at 0.7,
0.3 and 0, over a fullbright room.

### In the headset

- [ ] Lean left, right and forward with your feet planted (and aiming round a corner): do the legs stay? Walk a step or
      two in the room: do they follow? If they still step when you lean, raise Lean Detection; if the body lags when
      you walk, lower it.
- [ ] Lower Menu Background Opacity while tuning graphics: is the text readable over bright scenes?
## Body and weapon models improved

Your request: "some modelling/texturing improvements for the player hands, player body, and weapon models. Improve
their look while keeping the same art style and low-poly look." The hands were another agent's; this is the body and
the weapons. Nothing you tuned moves: every weapon's anchors, hotspots, offsets and hand fit are the same to the byte.

### Weapons

**How** (`Misc/quakevr/mdlpolish.py`, `Misc/quakevr/polish_weapons.py`). The script takes the models as rounds 16-20
left them (kept byte for byte in `Misc/quakevr/src_models/r21/`; running it again gives the same files) and adds to
them without touching what was there: the header's scale and origin (what each weapon's offsets and Scale are applied
about) stay, every old vertex keeps its index and its bytes (position and normal), every old triangle and UV stays.
New vertices and triangles are appended and share no vertex with the old ones, so vr_anchor.cpp's strip order of the
old vertices is unchanged: the script checks that every anchor of every slot using the model (hand, muzzle,
two-handed grip, ammo screen and button) names the same vertex at the same place. A new part must fit inside the
model's old byte box in every frame (the write fails otherwise): the grenade launcher's sides and the axe's head are
as wide as the bounds, so they got no bolts, and the double shotgun's muzzle bands would have poked out of the box in
the recoil frames. Each part rides the old piece it sits on through every frame (the best rigid fit of that piece's
vertices from frame 0): it recoils, pumps and spins with it. The parts keep clear of where the hands hold the guns
(grips, triggers, foregrips, pumps), so the fitted fingers close on the same surfaces.

**What.** Low-poly, faceted and flat-shaded as id's parts are, painted face by face in each gun's own ramps
(blue-black, the launchers' browns, worn steel for bolt heads), lit from above as Quake's skins are; never a
fullbright index, so the sights and screens are untouched:

| Model | Added | Triangles |
|---|---|---|
| Shotgun | a muzzle crown, two pins through the receiver each side, a ventilated rib along the barrel's top (under the line from the receiver to the front sight) | 766 -> 1046 |
| Double shotgun | a band round both barrels, a sighting rib in the valley between them up to the bead, hinge pin and receiver bolts | 688 -> 928 |
| Nailgun, lava nailgun | two bands on each barrel, bolts on the lower block | 480 -> 888 |
| Super nailgun, lava super nailgun | a clamp round the four barrels (it spins with them), bolts on the body | 726 -> 1002, 884 -> 1148 |
| Grenade launcher, proximity gun, multi-grenade launcher | bolt heads along the top bevels | 386 -> 566, 378 -> 558, 380 -> 560 |
| Rocket launcher, multi-rocket launcher | three bands on the tube, bolts on the warhead | 499 -> 811 |
| Lightning gun, plasma gun | bolts along the body, a band on the muzzle | 459 -> 711 |
| Axe | a band at the neck | 56 -> 140 |
| Grappling hook, laser cannon, Mjolnir | edge wear only | unchanged |
| Knights' swords | unchanged (no edges the wear pass could take) | unchanged |

At most 1.85 times the triangles (the nailguns; the axe from a tiny 56), each model 2 new skin rows at most.

**Skins: edge wear** (`mdlpolish.edge_wear`, every model above). The texels along the id models' box corners and
bevels (a convex edge of more than 50 degrees between two faces big enough on the model and in the skin) move one
step lighter within their own colours (two on sharp corners), and some texels of the next row are scuffed: worn, lit
edges as Quake's skins paint them. The id models reuse and overlap their UVs heavily, so a texel changes only when
every triangle using it agrees, only in the rows the id models had (the grips and parts earlier rounds painted keep
their own highlights), and never where one texel would span a wide band of the model: 246 (shotgun) to 5276
(grappling hook) texels a model. The lighter colour is matched in RGB (the nearest palette entry of the same hue),
not stepped along a palette row: the darkest entries of every row are nearly black, and stepping from them brought
out their row's hue (green on the nailgun's black metal).

### Body

`make_vrbody.py`, all three builds. The skeleton, the joint names and places, the weights, and the rings the engine's
tables are worked out from (the belly's and the chest's ellipses for the holster plates, the belt for the flashlight's
clip) are unchanged.

- **Mesh**: each loft is refined between the rings given (Catmull-Rom: the given rings keep their places and sizes,
  and the texture still runs along them as before), with more sides where the silhouette shows: torso 16 (was 12),
  arms, bracers, legs and boots 12 (10), head 10 (8); extra rings at the shoulders, elbows and knees. A belt now
  stands out of the torso (its edges tucked into it). The feet have a heel, instep, ball and toe cap and a flat sole
  (a squarer section). 1240 -> 2186 triangles.
- **Skins** at 256x256 (were 128), every texel one of Quake's palette colours, none fullbright: the vest quilted in
  vertical channels, stitched in rows, laced down the front, shadowed at the belt, collar and armpits; a stitched belt
  with loops and a steel buckle; creased camouflage trousers with stitched side seams, folds behind the knees, bunched
  at the ankles; the thigh plates' ridges lit along their tops, riveted; the boots' two buckled straps, back seam,
  turned-down tops, laces and soles, scuffed toe caps; bracers with stitched rims and two buckled straps; arms shaded
  by muscle (deltoid, biceps, triceps, the crook of the elbow); the head on its own block now (it shared the arms'
  texels): short dark hair, brows, deep-set eyes, stubble.
- **Armour** (green, yellow, red: the armour model's colours, as before): five lames, each lit along its rolled lower
  edge and shadowed under the lame above, riveted either side of the front closure and by the side straps; chipped
  paint. The damage (cuts and blood, more with less health) as before, kept inside the arms', torso's and bracers'
  blocks.

### Checks

- **Anchors and hotspots, in the engine**: `vr_anchor_info` for every anchor of every slot's model, and
  `vr_hotspots_check`, before and after: identical output (every position to the printed 0.01, every hotspot
  distance to 0.0001).
- **The hand on each weapon** (`vr_dumpview` holding each of the 18 weapons, `impulse 253` for their hotspots): the
  hand, palm, muzzle and foregrip the same (the only differences, 0.0001, are frame-timing noise).
- **Holes**: `check_mdl_holes.py` passes on every model (the new parts are closed solids).
- **Melee**: the canary, with the weapons done (before merging vr-cleanup f82607ba) and again with the body done and
  f82607ba merged: no difference from the kit's baseline either time (40/46 pass).
- **Draw cost** (exclusive runs in vrfiringrange, the double shotgun held and the body in view, 1500 frames each, two
  runs before and two after): alias models 0.01 ms an eye on the GPU and 0.05 ms on the CPU before and after; the
  whole frame 0.73 ms on the GPU both ways.
- **Composites** (scratch `models/comp/`): `weapons_1..3.png` (each weapon from the player's view, its right side
  and the front left, before | after), `body_views.png` (the body from the front, side, back and front left: no
  armour, green, yellow, red and hurt), `body_bind_a.png`, `body_bind_b.png` (the three builds and their skins,
  front, side, back and three-quarter, before and after).

### Not done, not verified

- Mock only: not seen in the headset. The details are small (a bolt head is about a centimetre): whether they read in
  the headset, and whether the bolts on the brown guns are too bright, is for you to judge.
- The sights and the grips were left alone: the sights are your tuned hues, and the fitted fingers close on the
  grips.
- The knights' swords, Mjolnir, the laser cannon and the grappling hook have no new parts.
- `vr_modelcollide`'s 24 sample vertices are picked from all of a model's vertices (farthest from their middle): the
  new vertices move the middle a little, so a held weapon's wall-collision samples may differ slightly. Not measured.
- Rerunning `improve_weapons*.py` rewrites the models this pass starts from: copy their output to
  `src_models/r21/` and run `polish_weapons.py` again.

### In the headset

- [ ] Each gun close up and from above as you aim: the shotgun's rib and crown, the double's rib and band, the
  nailguns' bands, the launchers' bolts and bands. Too subtle, too bright, or right?
- [ ] Fire each: the parts recoil with the gun; the super nailgun's clamp spins with its barrels; the shotgun's pump
  slides under the rib.
- [ ] The body in the body preview (`vr_body_debug 2` / `3`) or looking down: the vest, belt, boots, and each armour.

## Hands remodelled

Your note: better hands in the same style and low-poly look — "more high-poly and rounded so they look more like human
fingers and not just squares", a better thumb and bones so that the fingers wrap things better, and proportions that
are "normalized". Branch `agent/handmodel`; composites and numbers in the scratchpad's `handmodel/final/`.

| | Before | Now |
|---|---|---|
| The mesh | the six old models, fitted: flat 5-vertex strips for fingers, a stub thumb | modelled by `make_hand_rig.py`: a lofted palm, 8-sided tapering fingers with knuckles and rounded tips, a three-segment thumb |
| Triangles, vertices (MD5) | 234, 256 | 662, 474 (2.8x, 1.9x) |
| Proportions (1 unit = 1.2 cm) | palm (wrist to middle knuckle) 12.4, middle finger 6.9 | palm 10.0, middle finger 7.5 (about human: finger 3/4 of the palm, phalanges 1 : 0.64 : 0.52) |
| Joints | 148: one per vertex, their matrices fitted to the old frames | 33: palm, 15 segments, 15 half-turn helpers, 2 for the thenar |
| Skin | the old skins cut and restacked | painted in the old skin's palette ramp with its grain: creases, knuckle wrinkles, nails, palm lines; the blood as before |
| Commits | | `c248fee6` mesh, rig and skin; `903be0b4` cups; `8357c53d` fuller fingers, the skin's tone; `a32e1294` the back's tendons |

### The mesh

- **Palm:** lofted through seven sections of 13 vertices, from inside the forearm's cuff to the knuckles. The wrist is
  narrower than the palm. The heel of the hand has pads, the back arches over the four metacarpals, and the palm is
  cupped towards the little finger's side. The knuckle line is an arc: the middle finger's knuckle is furthest out
  and the little finger's furthest back.
- **Fingers:** 8-sided tubes, a little flatter than wide. They taper from the knuckle to the tip, bulge a little at the
  knuckles on the back, have fuller pads on the palm's side and a rounded, pad-heavy tip. They splay slightly and are
  as wide as the palm has room for.
- **Thumb:** three segments — the metacarpal, from the carpometacarpal joint near the wrist, and two phalanges. The
  metacarpal is thick: it is the ball of the thumb. The thumb sits abducted towards the palm's side, as the old thumb
  did, so a handle through the hand passes between the thumb and the fingers.
- **Proportions:** the knuckles, the palm's side and the fingers' roots stay where the old hand had them, so grips
  land in the same place. The palm is shorter at the wrist's end: the arm now meets the jointed hand 2.5 units
  (3 cm) nearer the knuckles (`handrig::data::wrist`; the six models keep their wrist). The fingers are longer (the
  middle finger 7.5 units, was 6.9).

### The rig

- **Joints:**
  - Each finger has three hinges: the knuckle (MCP) and the two finger joints (PIP, DIP). The thumb has the CMC, MCP
    and IP. The pivots are inside the knuckles, on each segment's axis.
  - Each hinge turns about its own axis. The fingers' knuckle axes lean a little (the index -5 degrees, the little
    finger +12), so the fingers converge as they close, as a fist's do.
  - Each joint has its turn per curl frame (0 open .. 4 the tightest fist; 5 is 3). The curls, the trigger's pull, the
    solver's stops and Manual fingers all mean what they did.
  - A relaxed hand at 0 has a slight natural curl. In the fist the thumb comes across the palm towards the index and
    middle fingers; the grasp's opposition turn comes on top of that, about the same axis as before.
- **Skinning, with no candy-wrapping:**
  - The ring of vertices at each joint rides a helper joint turned half as far (slerped). Each joint is mitred: the
    two tubes meet at the plane that bisects them, and the ring keeps its size however far the joint turns.
  - The rest of each segment is rigid on it.
  - The palm's thumb side follows the metacarpal by weight (up to 75%, falling off with the distance from it). It
    blends between helpers turned 1/4, 1/2 and 3/4 as far, so the ball of the thumb never thins when the thumb
    opposes.
- **The engine** (`vr_handrig.cpp`):
  - The CPU computes the 33 joints' matrices from the pose, and the GPU skins the mesh (the body's path, one draw per
    hand).
  - The same blend gives the vertices on the CPU for the cup's collision with the other hand and `vr_grasp_dump`.
  - The old path computed a matrix per vertex (148) and turned each normal to the old models' own.
- **The grasp solver's proxies** come from the generator (`segmentSpheres`, `palmSpheres`, `thenarSpheres` in
  `vr_handrig_data.inc`):
  - Each segment is a row of spheres as wide as it is, their palm's side flush with the skin.
  - The palm's side is a grid of spheres under its skin.
  - The ball of the thumb is its own set of spheres. The thumb's metacarpal has no row of its own; the thenar spheres
    are its contact.
  - The palm's middle (`palmCentre`) is the old hand's exactly: cups, the palm's turn and the free hand's reach to a
    cup are measured from it.

### Your placements don't move

- **Every slot held in the same mock pose, before and after** (`vr_dumpview`: the axe, all the guns and the sword):
  the hand, the muzzle and the foregrip are identical to 4 decimals, and so are the cups' one-time moves.
- **The drawn palm** moves only by the palm fit: the hand slides flush on the grip, at most Palm Fit: Weapons (3 cm).
  - The fit depends on the palm's shape, so the drawn palm differs by up to 2.4 cm (0.79 units) on the nailgun and
    the sword, and by 0.6-2.2 cm on the others.
  - The weapon, its muzzle and its aim don't move.
  - The fit per weapon, before / after (cm): shotgun 3.0 / 3.0, super shotgun 2.1 / 3.0, nailgun 3.0 / 3.0, super
    nailgun 0 / 1.0, grenade launcher 2.7 / 2.7, rocket launcher 2.7 / 2.5, lightning gun 2.8 / 3.0, sword 3.0 / 0,
    axe 0 / 0.
  - Most grips sit at the fit's 3 cm limit with either hand: your placements put the grip a little below the palm.
- **Cups:** the one-time cup move (third pass) works out where the old hand drew the helping hand from both hands'
  grip channels. With the new fingers it came out up to 0.3 units elsewhere. It now uses the old hand's channel
  (`grasp::legacyGripChannel`, measured on it), and the moved cups are identical. Live blade and cup alignment use the
  new fingers' channel.
- **Posing mode** (`vr_pose_check`, the super shotgun): hand 0.0001 units and 0 degrees from the pose, muzzle 0.0001,
  as before.

### The fingers wrap better

Each weapon in the main hand, and a gib in the off hand, at the same pose (`vr_debug_grasp 2`), over 50 fingers:

| | Met (wrapped) | Inside (drawn at the controller's curl) | Free |
|---|---|---|---|
| Before | 37 | 12 | 1 |
| Now | 42 | 8 | 0 |

- **The thumb:**
  - It now closes on the sword, the axe and the gib; before, it was inside the sword and the gib and free of the axe.
  - On the sword and the axe it lies along the handle rather than over the fingers: the solver's choice of turn, as
    before.
  - It is still inside the grips of the super shotgun, the nailgun, both launchers and the lightning gun, as before,
    and is drawn at the controller's curl there.
- **The nailgun's index finger** is now inside its trigger guard (it was met): it is drawn at the trigger's curl.
- **Solve times** (`vr_grasp_bench 200`, exclusive):
  - Afresh: 2.2-4.7 ms, against 2.0-6.9 before.
  - Again: a median of 22-37 µs, against 22-88 before (the sword 24 µs, was 88).
  - Fewer probes on every weapon but the super nailgun.
- **Stability:** the torch in the off hand and the shotgun in the main hand, each wobbled for 10 s (25 degrees and
  8 cm; `vr_debug_grasp_trace`). There were no re-solves and no joint moved, before and after.

### Costs (`run.sh --exclusive`, `vr_profile`, start map, both hands, per frame)

| | Before | Now |
|---|---|---|
| Empty hands: `hand` CPU / `rig hand` | 0.023 / 0.022 ms | 0.019 / 0.018 ms |
| Shotgun and gib: `hand` CPU / `rig hand` | 0.037 / 0.024 ms | 0.033 / 0.020 ms |
| `alias` GPU (every alias model), empty hands | 0.014 ms | 0.014 ms |
| `alias` GPU, shotgun and gib | 0.037-0.039 ms | 0.041-0.042 ms |
| Frame GPU | 0.53 / 0.55 ms | 0.52 / 0.55 ms (no change) |

The CPU is a little cheaper (33 matrices instead of 148). The GPU draws 2.8 times the triangles for about 0.003 ms.

### The skin

- **Painted, not cut and restacked:** the old skin is a painting of a whole hand in another pose. Projected onto the
  new mesh, its painted fingers and shadows landed in the wrong places, so the generator paints the skin itself.
- **The look is the old skin's:**
  - The same palette ramp (112-127, and the brown ramp's darks for the deepest shade).
  - The old skin's grain: two plain patches of it, tiled without visible repeats.
  - A broad mottle.
  - The mean brightness matched to the texels the old models used (luminance 73 against 74).
- **The details, laid out on this mesh:**
  - Knuckle highlights and wrinkles on the back, and the joints' creases on the palm's side.
  - Nails with a darker rim and a light free edge, and lighter fingertip pads.
  - Shade between the fingers' roots, and tendons over the metacarpals.
  - The palm's lines (heart, head and life lines) and the wrist's creases.
  - The ball of the thumb painted as the palm where the two meet, so there is no seam.
- **Blood:** the four damage skins get their blood as `make_bloody_hands.py` paints the six models' (the same marks,
  per part).
- **Powerups:** the quad, pentagram and ring go through the entity as before (checked: the quad and the ring).

### Kept working (checked; `final/`)

- **Both hands, mirrored:** `blood_quad_ring_both_hands.png`.
- **Blood, the quad, the ring:** the same composite.
- **The six models (`vr_hand_rig 0`):** `six_models_path_vr_hand_rig_0.png` is the same as before.
- **Weapons:**
  - `weapons_side.png`: each weapon in the hand.
  - `shotgun_auto_trigger_manual_tweaks.png`: Automatic, the trigger pulled, Manual, and the finger tweaks.
- **Two hands:** `two_handed_shotgun_foregrip.png`, `two_handed_cup.png`, `two_handed_sword_blade.png`.
- **Things in the hand:** `box_off_hand.png` and `torch_off_hand.png` (the flashlight's low grip).
- **The hand alone:**
  - `poses_open_fist_point.png`: open, fist, point, three sides.
  - `thumb_closeup.png`: the thumb up and down, the fist with it open and closed.
  - `curl_sweep.png`: 0 to 1 in 0.05 steps. It is smooth; the old hand was already half closed at 0.05.
- **Show Hand Bones:** `hand_bones_shotgun_sword_axe.png`.
- **The arm** meets the new wrist inside its cuff, with no gap (`arm.png`).
- **The melee canary** (`eval.sh`) after merging vr-cleanup `f82607ba`: 40/46, no take differs from the baseline.

### Files

- `Misc/quakevr/make_hand_rig.py`: rewritten. It models the hand, rigs it, paints the skin and writes the tables. The
  six old models are read only for their placement constants.
- `quakevr/progs/hand_rig.md5mesh`, `.md5anim`, `hand_rig_0N_00.lmp`: generated.
- `Quake/vr/vr_handrig_data.inc`: generated.
- `Quake/vr/vr_handrig.hpp/.cpp`: joints and weights, and the skinning matrices.
- `Quake/vr/vr_grasp.cpp/.hpp`: the proxies from the tables, and `legacyGripChannel`.
- `Quake/vr/vr_view.cpp`: the rig's wrist for the arm, and the cups' migration on the old channel.
- `docs/vr-port/TESTING.md`: a line on the generator.

### Not verified

- **In a headset:** only the mock was used. The scale, the thickness of the fingers and the colour under the maps'
  lighting are worth a look in VR.
- **The pentagram** was not triggered in a test. It goes through the same entity light path as the suit, and the quad
  and the ring were checked.
- **Hipnotic and Rogue weapons** were not held one by one. Their muzzles and placements don't depend on the hand.

### Second pass

Your note: "a tiny little bit less round", and the fingers "seem disconnected from the main hand (they seem like
little sausages)". Commit `795b0771`; composites in the scratchpad's `handmodel2/final/` (old | new).

- **The fingers grow out of the palm:**
  - The palm's front is now the four fingers' first rings, the same vertices, so there is no seam or step at the
    knuckles.
  - Between two fingers, a web: a pair of vertices both fingers share, a little past the knuckles and low towards
    the palm. It follows both knuckles half each, so it stretches smoothly when one finger closes and its
    neighbour doesn't (the point).
  - The palm's last section follows the knuckle line's arc, with a low ridge over each metacarpal's head.
- **A little blockier:**
  - The fingers' and the thumb's sections are rounded rectangles: still 8 vertices, but flat on the back, the
    palm's side and the sides, with bevelled corners.
  - The tips are blunter and squarer, and the knuckles' bulges a little softer.
- **The skin:** the same palette, grain and details. The palm and the fingers are painted as one surface, so
  their colours agree where they meet. The broad mottle follows the hand, not the texture. The shade between
  the fingers is only on the sides that face another finger.
- **The thumb's base** already read as part of the ball of the thumb (checked in `thumb_closeup.png`). It gets
  the new section and is otherwise unchanged.
- **Unchanged:**
  - The joints, pivots, axes, curls and weights (the web's pair is the only new blend).
  - Every slot's hand, muzzle, foregrip and drawn palm, and the cups' moves: `vr_dumpview` gives the same lines,
    old and new, for all nine slots.
  - The palm's and the thenar's solver spheres are identical. The fingers' spheres move by at most 0.05 units
    (the squarer tips). The grasps: 42 met, 8 inside, as before.
  - The melee canary: 40/46, no take differs.
  - Blood, the quad, the ring, and the six models (`vr_hand_rig 0`).
- **Cost:** 656 triangles (was 662), 455 MD5 vertices (was 474). `hand` CPU, `alias` GPU and frame GPU are the
  same within noise (exclusive runs, old and new twice).
- **Composites:** `poses_open_fist_point.png`, `knuckles_web_closeup.png` (the knuckles and webs close up,
  open, fist and point), `curl_sweep.png` (smooth, no pinch at the knuckles), `weapons_side.png`,
  `two_handed_*.png`, `torch_off_hand.png`, `box_off_hand.png`, `blood_quad_ring_both_hands.png`.

### Third pass

Your note: "a bit too skinny", the colouring "a bit too flat/dark", and smaller than the previous hand; "find a middle
ground". Measured on the drawn meshes (`vr_grasp_dump`, the open hand and the fist) and on screenshots under the same
light, the fitted hand (`176379f2`) against the second pass (`6b41ec06`); composites (old | current | new) and the
numbers in the scratchpad's `handmodel3/final/`.

| (hand units) | Previous | Second pass | Now |
|---|---|---|---|
| Wrist to the middle fingertip | 18.95 | 17.31 | 18.19 |
| Across the knuckles (the fingers' roots) | 7.62 | 7.00 | 7.37 |
| Palm thickness (its middle) | 4.80 | 3.39 | 4.00 |
| Fingers, width at the first segment (index, middle, ring, little) | 1.74, 2.31, 2.23, 1.81 | 1.87, 1.92, 1.83, 1.62 | 1.97, 2.07, 1.97, 1.74 |
| Fingers, depth there | 2.71, 2.82, 2.66, 2.13 | 1.70, 1.75, 1.67, 1.49 | 2.01, 2.12, 2.03, 1.80 |
| Thumb's section (narrow x broad) | 1.74 x 2.96 | 1.75 x 1.94 | 2.09 x 2.46 |
| Silhouette, open hand: back / side (square units) | 131 / 82 | 108 / 55 | 125 / 67 |
| Silhouette, fist: back / side | 94 / 77 | 65 / 49 | 76 / 64 |
| On screen (luminance): mean, spread, 95th percentile | 71, 38, 148 | 55, 19, 87 | 66, 21, 102 |
| Skin texels: mean, spread, 75th / 95th percentile | 70, 31, 94 / 119 | 76, 12, 81 / 94 | 88, 19, 105 / 119 |

- **Size:** the hand is 5% larger along the fingers and across the palm, scaled about the palm's middle
  (`palmCentre`, the grip area; `HAND_SCALE` in `make_hand_rig.py`). The joints' pivots, the wrist and the solver's
  spheres scale with it; the joint limits, turns and weights are unchanged. The length is now halfway back.
- **Fuller:** the fingers and the thumb are deeper, grown on the back (`FLAT_BACK`, `THUMB_FLAT_BACK`), and the palm is
  thicker on the back (the sections' `back`). The palm's side, where it meets a grip, stays where it was: grown there
  (or scaled through the hand), the fingers met the gun before they closed on it, and three grasps got worse. The
  fingers are now about as deep as they are wide; the old fingers' extra depth was the six models' slab. The shape is
  kept: the fingers grown out of the palm, the webs, the rounded rectangles.
- **The skin:** brighter and with more contrast, in the same palette and with the same details: the knuckles, the
  back's middle and the palm's pads lighter, the palm's hollow and the creases deeper, the sides no darker (the side
  views were the old hand's brightest). The knobs are together in the generator (`SKIN_BASE`, `SKIN_CONTRAST`,
  `FORM_*`). On screen the old hand's contrast comes largely from its models' normals under the engine's light, which
  the skin can't match without overexposing it in brighter light.
- **Unchanged:** every slot's hand, muzzle and foregrip, and the cups' moves (`vr_dumpview`, nine slots: the same
  lines). The grasps: 42 met, 8 inside, as before. The drawn palm moves as its place search chooses: the same on
  seven slots, and the launchers slide further along the grip (the grenade launcher to the 3 cm limit, was 2.7; the
  rocket launcher 2.8, was 2.5), with all their fingers met as before. The melee canary: 40/46, no take differs. The
  mesh (656 triangles, 455 vertices) and the costs are the same.
- **Composites:** `poses_open_fist_point.png`, `knuckles_closeup.png`, `grip_shotgun_sword.png`, `grip_launchers.png`,
  `grip_torch.png`, `two_handed_cup.png`, `two_handed_shotgun_foregrip.png`, `blood_quad_both_hands.png`,
  `luminance_shots.png`, `skins_old_current_new.png`.

### Try

- [ ] Second pass: do the fingers now read as part of the hand, and is the blockier look right (or too much, or
      not enough: `BOX_ACROSS` and `BOX_UP` in `make_hand_rig.py` set how flat the faces are)?
- [ ] Look at your open hand, a fist and a point: do the fingers read as fingers, and does the hand fit your
      controller (the knuckles where yours are)?
- [ ] Hold each gun and the sword: do the fingers wrap the grip and the thumb close over it?
- [ ] Two hands on the shotgun's foregrip, your cups, and the sword's blade.
- [ ] Take damage (the blood) and a quad.
- [ ] Third pass: is the size and fullness the middle ground you wanted, and the skin bright enough? `HAND_SCALE`,
      `FLAT_BACK` and `SKIN_BASE` / `SKIN_CONTRAST` in `make_hand_rig.py` set them (one step of the palette ramp is
      a level).
- [ ] If the skin's tone is off in VR, `base_level` in `make_hand_rig.py` sets it (one step of the palette ramp is a
      level).

## Hand calibration

Your question: "Do we have sliders to tweak the general hand position/orientation? I want it to match my real-life
hands as much as possible". There were only rotations (Gun Angle, Gun Yaw, Off-Hand Pitch, Off-Hand Yaw), with no roll
and no position. Branch `agent/handcal`; composites and numbers are in the scratchpad's `handcal/`.

**Where:** VR Settings > Advanced VR Options > **Hand/Gun Calibration**, first section, **Hand Calibration**.

- **The steps**, on the page:
  1. Hold the controller as you always do.
  2. Turn Show Controller on, and look at it.
  3. Move and turn the hand until the controller sits in its palm, just where your real one is.
- **Show Controller**: the same preview as on Weapon Offsets. If the preview itself is off your real controller, line
  it up first (Weapon Offsets > Controller Preview).
- **Main Hand X (forward), Y (left), Z (up)**: centimetres along the controller's own axes (the preview's red, green and
  blue). Bars run -5..5; they extend further.
- **Main Hand Pitch (down), Yaw (left)**: these are Gun Angle and Gun Yaw (`vr_gunangle`, `vr_gunyaw`), the same cvars,
  so your saved values stay. They turn about the controller's tracked point, as they always have, and change the aim.
- **Main Hand Roll**: turns the hand about where it points, through the middle of the handle, so the palm stays on the
  controller. The right side goes down. It doesn't change the aim.
- **Off Hand**:
  - **Its Own Values** (the default, since your off-hand pitch and yaw differ from the main hand's): six sliders of its
    own. Its pitch and yaw are Off-Hand Pitch and Off-Hand Yaw.
  - **Mirrors the Main Hand**: the main hand's values, with Y, Yaw and Roll the other way.
- **Match Controller Preview** (`vr_handcal_match`): moves each hand (X, Y, Z only) so that the middle of its empty
  fist is on the preview's point.
  - The middle of the fist is the grip channel's point: the middle of the circles its half-closed fingers curl round.
  - The preview's point is the middle of the handle, where OpenXR puts the fist.
  - It is exact: pressing it again moves the hand 0.00 cm. It follows the hand model (the channel is measured on it).
  - It only moves the hand. Turn it yourself afterwards, then press it again.
  - The off hand is matched only when it has its own values.
  - In the mock, from 0 it moved the hand -1.0, ±1.3 and +3.5 cm: the handle ends up in the curled fingers rather
    than against the palm.
- **Reset Moves and Rolls**: X, Y, Z and Roll go back to 0 for both hands. Pitch and yaw are left alone, because they
  are your aim.
- The rest of the page (the gun model settings) is now under a **Guns** header.

**What moves.** The calibration is applied where Gun Angle was, so the whole hand and whatever it holds move as if the
controller had been held differently:
- the empty hand and weapons;
- carried things and the flashlight;
- the posing mode's hand;
- the body's arms;
- the hand positions sent to the server, so muzzles, shots and melee points move too.

Some details:
- A weapon is posed from the hand before the roll and then turned rigidly by it, as Hand and Weapon Together does, so
  the gun rolls exactly with the hand. Without this, its Euler angle offsets would make it turn a little differently.
- The hand's velocities include the move turning with the controller.
- The melee's wrist is part of the hand, so it moves with the calibration. `QC/vr_melee.qc` takes out only the pitch
  and yaw, as before, mirrored in mirror mode.
- The menu's laser uses the same pitch and yaw.
- Takes record the values in a `hand calibration` header line. Playing a take recorded before this feature sets them
  to 0, so the hands are placed as they were when you recorded it.

**At 0 nothing changes.** Every step is skipped at 0, so the numbers are the same bit for bit.
- The melee canary gives 40/46 pass with 0 differences from the baseline. (One canary take,
  `no_hit_2026-09-27_02-21-55.csv`, has since been moved to `motions/discarded/`, so 46 of 47 ran.)
- Every slot's `vr_dumpview` was compared with the base build: 18 main-hand slots, 3 off-hand ones, 230 lines. The only
  lines that differ are ones that also differ between two runs of the base build (muzzles at the 4th decimal).

**Numbers** (mock, a shotgun in each hand, the weapon's lag off to measure the pose itself: `handcal/parse_num.py`):

| Setting | The hand | The muzzle | Melee on the server (grip, far end, wrist) |
|---|---|---|---|
| X 2 cm | +2.0001, 0.0000, -0.0002 cm in the controller's frame | +1.9999, +0.0001, +0.0001 | 2.0000, 2.0001, 2.0000 cm |
| Y 2 cm | 0.0000, +1.9997, +0.0002 | 0.0000, +1.9997, +0.0002 | 2.0000, 1.9999, 1.9999 |
| Z 2 cm | +0.0003, -0.0002, +2.0000 | +0.0001, -0.0001, +2.0003 | 2.0000, 2.0000, 2.0000 |
| Roll 10° | turned 9.9999° about the line through the grip along the hand's forward (axis · forward 1.000000, residual 0.0004 cm) | the same fit | far end and wrist turned 10.0005° about the hand |
| Off hand X 2 cm, Roll 10° | 2.0000 cm; 10.0000° | 2.0004 cm | 2.0000 cm; 10.0010° |

- Mirror mode with main X 2, Y 1.5, Roll 10 gives an off hand identical to Its Own Values with 2, -1.5, -10 and pitch
  and yaw 39.5, -4, including its melee on the server.
- The differences in the last digit are the 4-decimal printing.
- With the weapon's lag on, the gun reaches the full roll over a few frames, as it does when you roll your wrist.

**Composites** (`handcal/`, first person and from the side, Show Controller on):
- `handcal_empty.png`: 0; mirror mode with X 1.5, Z -1, Roll 20; own values for the off hand.
- `handcal_shotgun.png`: shotguns at 0 and calibrated.
- `handcal_match.png`: before and after Match Controller Preview.
- `handcal_carry_pose.png`: a carried health box and the posing mode's hand, at 0 and calibrated.

**Settings:** `vr_handcal_x/_y/_z/_roll`, `vr_handcal_off_mirror`, `vr_handcal_off_x/_y/_z/_roll` (all saved,
0 by default), with `vr_gunangle`, `vr_gunyaw`, `vr_offhandpitch` and `vr_offhandyaw` as before.

**In the headset:**
- [ ] With Show Controller on, move and turn each hand until the controller sits in your palm as in real life.
- [ ] Try Match Controller Preview. Does it put the handle in the fingers?
- [ ] Check that aim, melee and throws still feel right with your values.

## Hand editable in Blender

Your note: "Ideally I would just like to open the hand model, make some tweaks to the vertices/proportions, save and
have it work properly." Branch `agent/handblend`. The step-by-step guide is `HANDS_IN_BLENDER.md`. Composites and
logs are in the scratchpad's `handblend/final/`.

### What changed

- **The engine reads the rig from `progs/hand_rig.md5mesh`** as it loads the model (`vr_handrig.cpp`). That covers
  the mesh, the weights and the joints' pivots (the bones' heads). The rest is worked out from them in about 2 ms:
  - **The hinges:** each joint's hinge turns with its segment's direction. If a finger is re-aimed, its curl turns
    it the new way. The curl frames' angles are the compiled ones.
  - **The grasp solver's spheres** are the compiled ones (tuned with the solver), moved and resized by how the mesh
    around each changed:
    - a finger segment's by its cross-section there (width, where its palm side is) and by the segment's length
      (from the pivots; a fingertip's from the mesh);
    - the palm's and the thenar's by the skin over them.
  - **The grip channel** comes from the spheres, as before.
- **Fixed, whatever the file says:**
  - the palm's frame;
  - `palmCentre` and the placement constants. Weapon placements and cups are measured from these, so an edited palm
    changes shape around the weapons, and the weapons stay put on the controller.
  - the wrist, where the arm meets the hand.
- **`vr_hand_reload`** reads the files again and draws and grasps with them live (the model, its skins and the rig).
  Held things are solved again.
- **`vr_hand_rig_info`** says where the rig in use came from, and compares it with the compiled one.
- **The compiled tables** (`vr_handrig_data.inc`, unchanged) are the fallback and the reference the spheres are
  fitted against.
- **Clear errors:** a file the rig can't use is refused with the reason (bone, vertex or line), and the hand you had
  is kept. This covers a renamed or missing bone, a vertex weighted to no bone, missing weights, weights that don't
  add up, more than 4 weights, and a damaged or cut-short file. A game started with such a file draws the six-model
  hand and says why; `vr_hand_reload` brings the jointed hand back once the file is fixed.
- **The Blender add-on** (`Misc/quakevr/blender/addons/quakevr_hand`, Blender 5.2) imports and exports the hand:
  - the armature (the 16 joints as finger chains, the 17 helpers in a hidden collection), the weights (vertex groups)
    and the seams joined;
  - the skin as an image. The image can be saved as a PNG, painted elsewhere and read back. On export it goes back
    into Quake's palette (never a fullbright colour). Unchanged texels keep their index, and the edits are carried
    under the blood of the three damage skins.
  - **Apply Pose as Rest (Mesh Too)**: scale or turn bones in Pose Mode, then make that the rest shape.
  - The export refuses renamed or missing bones and unweighted vertices (and selects them).
- **The generator** (`make_hand_rig.py`) still makes the shipped files. It writes the MD5 through the add-on's writer
  (`md5hand.py`) with every number exactly the compiled float, so the shipped files read back as the compiled rig bit
  for bit. Rerunning it overwrites Blender edits: keep the .blend.

### Same results with the shipped files

- **`vr_hand_rig_info`:** "the rig in use is the compiled one, bit for bit". Pivots, curl turns, all 104 solver
  spheres and every triangle's corners (places, joints, weights) are equal as floats.
- **The files:**
  - `vr_handrig_data.inc` and the four skins are byte-identical after regenerating.
  - The `.md5mesh` changed only in its digits. The old file rounded to 6 decimals; the engine-baked drawn vertices
    move by at most 5.7e-6 units (0.07 µm).
- **`vr_dumpview`**, nine slots: the same lines.
- **The grasps** (every weapon and a gib, `vr_debug_grasp 2`): 42 met, 8 inside. Every grasp line and finger stop is
  the same; only the timings differ.
- **The fist's `vr_grasp_dump`:** the same.
- **The melee canary:** 40/46, no take differs.
- **A Blender round trip without edits** (import, export) gives the same files byte for byte, but the `commandline`
  line. The skins come back the same too.

### The round-trip test (headless Blender 5.2, then the mock)

- **The edit:**
  - The thumb's and the four fingers' first bones scaled 1.1 in Pose Mode, then Apply Pose as Rest (the pivots move
    up to 0.59 units).
  - 7 vertices of the palm's pad pulled out 0.35.
  - The skin saved as a PNG, stripes painted on the back of the hand's texels with PIL, read back.
  - Exported.
- **In the game:**
  - `vr_hand_reload` mid-run: "the pivots moved 0.59 units at most ... the grasp's spheres moved 0.74 at most, sized
    x1.00 .. x1.10".
  - The hand is drawn edited, with longer fingers and the stripes (`reload_empty_hand.png`).
  - The index finger's spheres land where a 1.1x scale about its knuckle puts them, within 0.01 units, radii
    included (`spheres_open.png`, `spheres_shotgun.png`).
  - The spheres' fit to the skin is as before: the median of skin minus radius is -0.08 against -0.06.
- **Grasps with the edited hand:** 37 met, 13 inside (the shipped hand: 42, 8) over the same 50 fingers.
  - The 10% longer index fingers now start inside the shotgun's, the super shotgun's, the super nailgun's and the
    lightning gun's trigger guards, and are drawn at the controller's curl there.
  - The fuller palm pad leaves the nailgun no palm fit (0 cm, was 3), so its other three fingers start inside the
    grip; its thumb now closes on it, as does the rocket launcher's.
  - The sword, the axe, the grenade launcher and the gib: every finger met.
  - The health box: held in the closed fingers, as with the shipped hand. The shotgun and the box as dumped:
    `dump_shotgun_box.png`.
  - The composites for the shotgun are `reload_shotgun.png` and `spheres_shotgun.png`.
- **Blood (two levels), the quad and the ring** work with the edited hand and skin; the damage skins carry the
  stripes under their blood (`reload_blood_quad_ring.png`, `skins_lmp_shipped_edited.png`).
- **Also tested:**
  - A finger splayed 15 degrees: 3 hinges turned, it curls along its new direction.
  - The middle finger subdivided (602 vertices, 914 triangles, weights interpolated by Blender): loads, and grasps
    the shotgun as before.
  - The robustness cases (`robust.png`): each refused with its message and the hand kept. The last shot is a
    startup with a broken file (the six models), then fixed and reloaded.

### Files

- `Quake/vr/vr_handrig.cpp/.hpp`: the rig read from the file, its derivation, `vr_hand_reload`, `vr_hand_rig_info`.
- `Quake/vr/vr_grasp.cpp`: the spheres and pivots from `handrig::rig()`, rebuilt when it changes.
- `Quake/vr/vr_view.cpp`: the cup's and `vr_grasp_dump`'s mesh from the rig; grasps solved again after a reload.
- `Quake/gl_model.c`, `gl_mesh.c`: `VR_ModelReplacementOk` (the hand's MD5 checked first), `Mod_ReloadAliasModel`.
- `Misc/quakevr/blender/addons/quakevr_hand/`: the add-on and `md5hand.py`.
- `Misc/quakevr/make_hand_rig.py`: writes through `md5hand.py`.
- `quakevr/progs/hand_rig.md5mesh`, `.md5anim`: regenerated (exact digits).
- `docs/vr-port/HANDS_IN_BLENDER.md`: the guide.

### Not verified

- **Blender's interactive UI:** only tested headless, through the add-on's operators; the sidebar panel wasn't
  clicked.
- **Painting in Blender's Texture Paint:** it paints the same image, but only the external-PNG path was tested.
- **In a headset:** only the mock was used.

### Try

- [ ] Install the add-on, import `hand_rig.md5mesh`, change something, export, `vr_hand_reload`.
- [ ] Hold the guns and the sword with your edited hand: do the fingers wrap as the new shape suggests?

## Forearm, bracer and wrist

His note: part of the hand clipped through the bracer, and at extreme angles the bracer and the wrist went thin and
unnatural.

### Why

- **The clipping:** the bracer's cuff went on 3 cm past the point where the arm meets the hand. "Hands remodelled"
  moved that point 3 cm into the hand. So the cuff lay over the heel of the hand, which is wider than the cuff. The
  hand came through it even with a straight wrist: 6 to 12 mm, depending on the build.
- **The thinning:** the whole forearm turned rigidly with half of the hand's roll. The wrist's ring was blended 40/60
  between the forearm and the hand, so it had to take both the rest of the roll and all of the bend. Blended
  skinning pulls such a ring in: 83% of its area at 70 degrees of bend, 52% at a half turn.
- **The angles themselves:** the arm IK placed the elbow only by its pole. Turning the hand bent the wrist far past a
  real one's reach. In the mock, rolling the controller 90 degrees bent the wrist 75 degrees; pointing the hand a
  little up bent it 105.

### What changed

- **Skeleton** (`make_vrbody.py`; the engine's `bind()` and joint tables the same). The existing joint names, axes,
  bind pose and attachments are unchanged. After the legs come ten new joints, children of the forearms:
  - `foretwist1..4_<side>` on the forearm's axis, at its rings (5, 12, 19 and 22.5 cm from the elbow). They take a
    share of the hand's roll that grows along the forearm: none at the elbow, all at the wrist.
    `vr_body_forearm_twist` is now the share at the forearm's middle (0.5, the default, spreads it evenly).
  - `wrist_<side>` takes all of the roll and half of the wrist's bend. It is stretched across the bend by
    1 / cos(half the bend), which is how a mitre joint's section widens. `Bone` gained a `shape` matrix in its own
    axes for this.
  - The roll is split from the bend exactly (swing and twist). It is kept continuous past a half turn, so a hand held
    upside down no longer flips the forearm from one side to the other.
- **Bracer:**
  - It ends at the wrist in a 7 mm flared lip that turns with the hand and holds the base of the palm.
  - Near the wrist its rings are at least an athletic build's size, because the hand is the same for every build.
    The lean bracer's wrist is 2 to 3 mm fuller.
  - Each ring turns rigidly with one joint, so no ring blends between two rolls.
  - Its texture runs along it by length: the straps sit behind the wrist and the dark rim at the end.
- **Arm skin:** it now ends 2 cm inside the bracer (15 cm from the elbow) instead of running on to the wrist under it.
  Where the two bent differently, a skin underneath could only come out through the bracer. The visible arm and its
  texture are unchanged, and the model has 2186 triangles as before.
- **Arm IK: the elbow eases the wrist.**
  - When the hand would bend or turn the wrist past a real one's reach, the elbow swings round the line from the
    shoulder to the wrist, as far as that eases it. The reach is an ellipse of 75 degrees of flexion and 35 of
    deviation, with the strain starting at 60% of it, and 60 degrees of roll.
  - It chooses the least strain plus a cost for the swing itself. Within the wrist's reach the elbow stays exactly
    where the pole puts it: neutral poses are unchanged.
  - It keeps to the swing it had unless another is clearly (20%) better, and follows it in about 0.05 s. Where two
    swings are about as good, as with the hand upside down, the elbow doesn't flick between them.
  - `vr_body_wrist_limits` (Body menu, "Wrist Limits") scales the reach. 0 turns it off.
  - In the mock, with the right hand at the chest:
    - Controller rolled 90 degrees: the wrist's bend drops from 75 to 33 degrees, and its twist rises from 85 to 106.
    - Hand pointed a little up: bend 105 to 69, twist 28 to 119. The elbow lifts out to the side.
    - Neutral pose: no change.

### Numbers

These come from `wristlab.py` in the scratch folder: the engine's skinning of the generated MD5s in Python, with the
same pose code. Right arm; the left mirrors it.

- "Area" and "radius": the bracer's wrist ring, against the bind pose.
- "Worst": the thinnest ring of the whole bracer.
- "Hand": how far the jointed hand comes out of the bracer, in mm.

Athletic build:

| flex / dev / twist | before: area, radius, worst, hand | after: area, radius, worst, hand |
|---|---|---|
| 0 / 0 / 0 | 100, 100, 100, **8.9** | 100, 100, 100, **0** |
| 50 / 0 / 0 | 91, 91, 91, 9.2 | 110, 100, 100, 0 |
| 70 / 0 / 0 | **83, 83, 83**, 9.3 | 122 (the mitre), 100, 100, 0 |
| 90 / 0 / 0 | 72, 72, 72, 11.7 | 141, 100, 100, 0 |
| 0 / ±30 / 0 | 97, 97, 97, 9.4 | 104, 100, 100, 0 |
| 0 / 0 / 90 | 86, 93, 93, 9.4 | 100, 100, 99, 0 |
| 0 / 0 / 135 | 70, 84, 84, 9.4 | 100, 100, 98, 0 |
| 0 / 0 / 180 | **52, 72, 72**, 10.2 | 100, 100, 96, 0 |
| 70 / 30 / 90 | 69, 74, 74, 10.6 | 124, 101, 98, 0 |
| -70 / -30 / -90 | 75, 78, 78, 12.0 | 124, 101, 99, 0 |

- **Lean and brawny builds:** the same after, 0 mm everywhere. Before, lean was 11.9 to 13.9 mm and brawny 6.0 to
  10.2 mm.
- **The arm's skin:** it no longer comes through the bracer in any of 105 poses (flexion ±70, deviation ±30, twist
  ±150), in any build.

### Checks

- **Composites** (before, after; the player's view, a camera on the back of the hand, one under the palm), in
  `armfix/final/`:
  - The empty hand for all three builds.
  - The shotgun and the sword.
  - The off hand.
  - A calibration offset (X 2 cm, roll 20 degrees).
  - The body preview.
- **Weapon poses:** `vr_dumpview` holding every slot, before and after: identical. The hands, muzzles and palms are
  not touched.
- **Melee:** the canary gives 40/46 with no difference from the baseline.

### Hand generator (not changed here)

The hand's end inside the arm reaches 2.1 cm behind the wrist (`make_hand_rig.py`'s first palm section and the cap's
point). At strong bends it swings out towards the bracer. The bracer's fuller rings near the wrist hold it: 0 mm in
every pose above. If the hand grows again, or its Scale is raised, that margin goes first. The fix would be a shorter
end (about 1 cm) or a joint for it that turns with the forearm.

**In the headset:**
- [ ] Bend your wrist hard up, down and to both sides, and turn your palm up and down. Does the hand stay inside the
  cuff, and does the wrist keep its thickness?
- [ ] Roll the controller over. Does the elbow swing naturally, or too far? Try Body > Wrist Limits (0 is the old
  behaviour).
## AA and liquids; liquid transparency saved

Your notes: "enabling anti-aliasing in the graphics options screws up the water reflections or refractions", and
"I've tweaked the transparency settings multiple times for the liquids, but they always get reverted". Branch
`agent/aawater`. Composites are in the scratchpad's `aawater/`.

### Anti-aliasing and water

- **The cause:** with anti-aliasing (`vid_fsaa` 2/4/8, MSAA) the water's **refraction was switched off**. It reads
  the opaque scene's colours, which with MSAA are a multisampled texture that a shader can't sample, so
  `R_OpaqueSceneTexture` returned nothing and the refraction strength went to 0. The water then lost its bending
  of what is under it and looked flatter. The "reflections" (the fresnel sheen and the glints) are computed in the
  shader and weren't affected.
- **The fix:** `R_BindOpaqueScene` resolves the multisampled scene into a plain texture once per eye, as the first
  translucent liquid draws, and the refraction reads that. The resolve runs only with MSAA on, refraction on and a
  translucent liquid in view. The translucent pass draws into the OIT buffers, so the copy still holds the opaque
  scene. The Water Refraction help no longer says it needs anti-aliasing off.
- **Evidence:** `aa_before_after.png` shows e1m2's pool before and after, with AA off and at 4x. `e4m1_after.png`
  shows AA off, 4x and 8x. Both eyes match.
- **Cost:** 0.06 ms per eye at 2048x2048 with 4x MSAA on the RTX 4090 (the `refraction resolve` scope in
  `vr_profile`). At your 3292x3524 it should be about 0.17 ms per eye (estimated, not measured). It costs nothing
  without MSAA or with refraction off.
- **The other effects that read the scene or its depth** already handled MSAA:
  - the foam's and soft particles' distances read the first sample;
  - the heat haze resolves its own copy;
  - bloom, tone mapping and the upscaler run after the scene's resolve;
  - foveated rendering only sets the shading rate.

### Liquid transparency saved

- **Why your tweaks were lost:**
  - `r_lavaalpha`, `r_slimealpha` and `r_telealpha` were never written to the config (the old Quake VR saved them).
  - `quakevr.cfg`, which runs after the saved config, set `r_lavaalpha 1` at every start.
  - `r_wateralpha` was already saved, and your config has 0.6.
- **Now:**
  - All four are saved, and `vr_savedefaults` writes them.
  - Lava's opaque default moved to `vr_defaults.cfg` (`vr_default r_lavaalpha "1"`), and the forced line is gone.
  - Slime and teleporters stay at 0 by default, which means "same as Water Alpha".
- **Maps' own values:** a map's worldspawn keys (`wateralpha`, `lavaalpha`, ...) used to override your settings
  when the map loaded. Now your settings win. *Map's Own Alpha* (`vr_map_liquid_alpha 1`) in the Transparency menu
  lets the map's keys win again. No shipped map has these keys, so they weren't what reverted yours.
- **Moving a slider in game** now gives exactly what a reload would. Only liquids the map is vised for turn
  see-through, which avoids seeing into the void. `r_novis` also updates them now.
- **Evidence:**
  - Two launches: in the first, set water 0.55, lava 0.4, slime 0.5 and tele 0.7, then quit. The second launch has
    the same values after starting and after loading a map.
  - `mapkeys.png`: e1m2 with a test `wateralpha 0.1` key, shown with the toggle off, with it on, and with the
    setting changed while it's on.
  - `vr_savedefaults` wrote all three new ones.

### For you

- [ ] **Set your liquid values once more** (Transparency menu). They weren't saved before, so I don't have them.
  Only `r_wateralpha 0.6` is known, and it's already the default. Once they're in your config, I'll make them the
  shipped defaults.
- [ ] Turn Anti-aliasing on and look at water with Water Refraction on: what's under it should bend the same as
  with AA off.
## Parry stamina and counter-attacks

Your two notes: a stamina system in place of the random drop chance, and a counter-attack after a successful parry.
Both are in `QC/vr_melee.qc` ("Parry stamina and counter-attacks"), called from `VR_Parry` (`combat.qc`). Both are in the
menu under Gameplay > Parry, Bash and Headbutt, as two new sections.

### Parry stamina (off as shipped)

- **What it does.** Each parry with a weapon costs stamina: 30 with the weapon in one hand, 12 with it in two, out of
  100. The parry that leaves you no stamina still blocks the blow, but it knocks the parrying weapon out of your hand.
  This uses the drop chance's code (`DropWeaponInHand`) and applies to one hand or two. With the defaults, the fourth
  one-handed parry in a row drops the weapon, and the ninth two-handed one does. After 2 s without a parry, stamina
  comes back at 25 a second, so from empty it is full again 6 s after your last parry.
- **Why it is off by default.** The system is opt-in: turning it on changes how parrying plays. It is one switch
  (Parry Stamina) if you want to try it.
- **It replaces the drop chance.** With stamina on, `vr_parry_drop_chance` is not used at all, so a weapon never drops
  at random. With it off, the drop chance works as before. The two never combine: two separate reasons to lose a
  weapon would be hard to read. The Parry Drop Chance help text (Melee Settings) says this.
- **Shared, not per hand.** There is one pool for both hands. It stands for your arms and body, not the weapon.
  Separate pools would let you alternate hands to never tire, which defeats the point of choosing when to parry. It
  would also make "two hands cost less" unclear (whose stamina does it spend?). The weapon that drops is the one that
  made the parry that emptied the pool.
- **Crossed arms cost nothing.** An unarmed parry has no weapon to lose.
- **Feedback, two settings:**
  - Tiring Warning (0..1, the volume and strength). When one more one-handed parry would drop the weapon, you hear
    Quake's short breath (`player/gasp2`) and feel a low throb in the hand once the parry's buzz ends. When the weapon
    is knocked away, you hear the long gasp (`player/gasp1`) and feel a long, low buzz, and the gadget shows
    "Exhausted: the blow knocks the weapon out of your hand!".
  - Stamina Bar (on/off). Each parry shows a bar of ten cells rising from the weapon's middle, drawn with Quake's
    slider glyphs. It is green while you are rested, amber below 60%, and red once the next one-handed parry would
    drop the weapon. When the weapon drops it reads "EXHAUSTED". It uses the damage numbers' rising text
    (`floattext`), at a small scale. A bar on the wrist gadget would need changes to the gadget's code, which
    another agent is working on this round, and the ammo screen can't carry it because swords and axes have no
    ammo screen.

### Counter-attacks (on as shipped)

- **What it does.** Any parry (with a weapon or with crossed arms) opens a 1.5 s window. Your next melee attack in
  that window is a counter and does 1.5x the damage. That attack can be a blow with either hand (slash, stab, pommel,
  punch or gun strike) on something alive, a bash, or a shove; for a bash or a shove the knockback is also 1.5x.
  You get one counter per parry.
- **It replaces the parry-bash.** The parry-bash was a weapon bash within 1.5 s of a weapon parry, at 1.3x. It is now
  this one mechanic, with one window and one multiplier. That means a counter bash hits at 1.5x instead of 1.3x.
  It keeps its own sound (`vr/bash_parry`) and its `parrybash` event, so the recorder and the eval are unchanged.
  Counter-Attacks off turns off counter bashes too.
- **Feedback:**
  - Counter Sounds (volume). New `vr/counter_open.wav` plays as the window opens: a blade's scrape rising into a
    high ring, 70 ms after the parry's strike so that it doesn't cover it. Almost all its energy is above 1.5 kHz.
    New `vr/counter.wav` plays when a counter lands, on top of the attack's own sound: a crack, a deep boom and a
    sting of steel. Its energy is 0.34 below 150 Hz, 0.40 at 1.5-4 kHz, and 90% of it is out within 0.23 s. Both are
    synthesised in `make_sounds.py`.
  - Counter Glow (on/off). While the window is open, what your hands hold gives off golden embers along its length,
    fewer as the window closes.
  - Counter Pulses (0..1). Soft 160 Hz pulses in both hands every 0.25 s, weakening as the window closes. Both hands
    get them because either hand can land the counter.
  - The dummy's readout shows the bonus: "..., counter x1.50" after a blow, and "counter bash with the ... (x1.50)"
    after a bash or shove.

### Settings

| Setting | Default | Menu (Parry, Bash and Headbutt) |
|---|---|---|
| `vr_parry_stamina` | 0 | Parry Stamina |
| `vr_parry_stamina_max` | 100 | Stamina (20-300, extends) |
| `vr_parry_stamina_cost` | 30 | One-Handed Parry Cost (0-100, extends) |
| `vr_parry_stamina_cost_2h` | 12 | Two-Handed Parry Cost (0-100, extends) |
| `vr_parry_stamina_delay` | 2 s | Rest Before Recovering (0-6 s, extends) |
| `vr_parry_stamina_regen` | 25 /s | Recovery Rate (1-100, extends) |
| `vr_parry_stamina_warn` | 1 | Tiring Warning (0-1) |
| `vr_parry_stamina_show` | 1 | Stamina Bar |
| `vr_counter` | 1 | Counter-Attacks |
| `vr_counter_window` | 1.5 s | Counter Window (0.25-4 s, extends) |
| `vr_counter_damage` | 1.5 | Counter Damage (1-3x, extends) |
| `vr_counter_sound` | 1 | Counter Sounds (0-1) |
| `vr_counter_glow` | 1 | Counter Glow |
| `vr_counter_haptic` | 1 | Counter Pulses (0-1) |

**Tracing.** With `developer 1`, the console shows each step:
- "stamina: 40 of 100 left (one hand, -30)", ": low", ": none, the weapon is knocked away";
- "stamina: 0 left, coming back (2.00 s after the last parry)", "stamina: rested";
- "counter: open for 1.50 s (hand 1)", "counter: slash on monster_knight, x1.50, 1.17 s after the parry",
  "counter: closed unused".

With `vr_debug_shots 1`, a melee blow's damage is printed too (the line already existed for other damage).

**Test hook.** `impulse 242`: the nearest monster that strikes in melee, within 150 units, hits you for 10 right
away, the same way its own attack would. With `notarget`, the monster stands still, so blows come only when the
script says.

### Tests (mock headset; scripts in the scratchpad's `parry/gen.py`)

e1m1, a knight from `impulse 248` walked up to 33 units, `notarget`, god mode, the sword level across in one hand,
blows from `impulse 242`:

- **One hand, four parries 0.67 s apart:** 70, 40, 10 ("low", the breath), then 0: "none, the weapon is knocked
  away", the sword thrown to the floor, "Exhausted: ..." on the gadget.
- **Rest:** "coming back (2.00 s after the last parry)", then "rested, 100 (5.99 s after the last parry)".
- **Two hands** (the off hand on the blade): 88, 76, 64.
- **Counters** (stamina off; `vr_melee_dmg_multiplier 0.25` to keep the knight alive):

  | Test | Result |
  |---|---|
  | Slash 1.8 s after a parry | "closed unused (1.52 s)", then 5.40 damage at strength x0.72 |
  | Slash starting at once, landing 1.17 s after a parry | "counter: slash ... x1.50", 8.37 at x0.744 |
  | Bash landing just after the window closed | 4.8 (8 x 0.6, one hand) |
  | Bash 0.5 s after a parry | "counter: bash ... x1.50", 7.2 (x1.5) |

  The slashes' damage per unit of strength is 7.50 without the counter and 11.25 with it: exactly 1.5x.
- **Dummy** (vrfiringrange): parry the knight, turn, bash the dummy: "Dummy: 7.2 damage - counter bash with the
  Knight's Sword, one hand (x1.50), at the body".
- **Pictures** (scratchpad `parry/vis6.png`, `parry/menu_crop.png`): the bars at 70 (green), 40 (amber), 10 (red),
  the embers during the window, and the menu's two new sections.
- **Canary eval:** 40/46 pass, 0 differences from the baseline, with the new settings at their defaults.

### Not verified

- In the headset: the haptics (the throb, the buzz, the pulses), the sounds as heard, and how readable the bar is
  (it rises quickly; the mock's view is narrower than the headset's).
- The dummy's readout for a counter blow (as opposed to a bash). The mock's scripted slashes didn't reach the dummy
  from where `setpos` puts you. The same slash did land as a counter on a knight, and the readout code is a single
  line.

### In the headset

- [ ] Parry Stamina on, a knight: parry one-handed three times, hear the breath on the third, and on the fourth the
      sword flies. Wait about 6 s, then parry again: the bar is green.
- [ ] The same with the sword in two hands: many more parries before it drops.
- [ ] Parry, then slash within about a second: a heavier strike sound, and the embers stop. Wait 2 s after a parry,
      then slash: a normal hit.
- [ ] Parry, then bash: the counter bash (1.5x now, was 1.3x).
## Grab reach from the fist; two-handed detach; brushing fingers

Your four notes: things taken from beyond the hand (and your theory that the open hand's pose was tested), a bias
to force the hand closer or allow it further, a prop held in both hands that floats when the hands are pulled
apart, and fingers that react to the weapon the free hand brushes. Branch `agent/handsint`, one commit each.
Composites and logs in the scratchpad's `handsint/final/`.

| Your note | Status | Commit |
|---|---|---|
| Grabbing a box with the hand detached from it; the fist, not the open hand; a distance bias | Done: the fist must touch it; Grab Distance Bias | `4d93a3c6` |
| Two-handed carry: the box floats between hands pulled apart | Done: a hand pulled off lets go; both, and it drops | `a4f693ca` |
| Brushing weapons: the fingers should react to the weapon's shape | Done: fingers rest on it or bend out of it | `30edbd75` |

### What the grab test measured (and your theory)

The test (`vr_physics.cpp` `handOn`, and `carryreach` for the second hand) took one point, the hand's place as the
move sends it (`handpos`), and measured from it to the thing's drawn triangles: taken within Carry Reach (8 cm), if
the point was also within the thing's box plus 2 units. That point is the controller's front, which is the front
top corner of the drawn fist. Measured in the mock (`vr_dumpview`, in the hand's frame):

| From the hand's point | forward | up |
|---|---|---|
| the fist (a full grip, trigger and thumb) | -16.1 .. -0.2 cm | -12.1 .. -3.0 cm |
| the open hand | -16.1 .. **+6.6 cm** | -12.4 .. +0.3 cm |
| the palm's middle | -13.5 cm | -7.9 cm |

So a thing could be taken 8 cm from the fist's front (7 cm past the knuckles, face on: `sweep_before.txt`, taken at
7 cm, not at 8), and 11 cm above the fist's top. The open fingers reach 6.6 cm past the point: at the limit the
open fingertips are about 1.4 cm short of the thing. Your theory is right in effect: the reach matched the open hand.
The cause wasn't the open pose itself but the 8 cm reach measured from the front of the fist.

### Grab reach from the fist

- A hand takes a box, backpack, gib, head or armour only if its **fist touches it**. The fist is the empty hand closed
  as a full press draws it: the jointed hand's grasp spheres, 93 of them (palm and curled fingers).
  - The view places them every frame in the hand's frame, so they follow Hand Calibration, the fist's angle offsets
    and the hand's scale (`held::setFist`).
  - The server tests them at the hand's place and angles against the thing's drawn surface: the same triangles the
    grasp and the old test use.
  - It touches if a sphere is within **Grab Distance Bias** of the surface, or its middle is inside the thing (by the
    winding of the triangles, a brush model's one way and an alias model's the other).
- **Grab Distance Bias** (Carrying and Gibs, `vr_carry_grab_bias`, 0 cm, -3..5, `.extend()` to -10..20):
  - positive: taken from this far off too;
  - negative: only pressed this far into it.
- The same test for the second hand of a two-handed hold.
- Carry Reach (`vr_carry_reach`) is retired: kept so your config loads silently, and no longer saved.
- **Found on the way:** the old box test used the entity's box, a 6-unit cube round the origin for the scaled ammo
  and health boxes. It worked only because those boxes are drawn that small. The quick test now uses the box the
  thing is drawn in.
- **Without the jointed hand model** (a dedicated server, which draws nothing): the old point test.
- **In multiplayer,** a remote player's fist is the host's (the same rig and fist; hand calibration is the host's).
- **Debug draw** (`vr_debug_carry 1`, or Show Physics Shapes):
  - the box the thing is drawn in (green: the fist touches, red: not);
  - the fist's spheres (faint; the touching one green);
  - a line from the nearest sphere to the surface's nearest point.
  - `vr_debug_carry 2` prints each test ("the fist touches, -0.83 cm from its surface"); 3 also prints the fists not
    near.
- **Checked** (mock, firing range, a shells box floating face on in front of the fist; `sweep_*.txt`,
  `grab_before_after.png`):
  - before: taken with its face 7, 6 ... 0 cm ahead of the hand's point;
  - now: not taken at 4 cm (gap 4.17 cm) nor at 0 cm (the fist's front 0.17 cm short); taken 1 cm into it (-0.83).
  - A health box: taken at a gap of -1.69 cm, not at +0.31.
  - A gib (alias model): taken when the fist is at it (-7.3 cm, inside), not 12 cm back (+1.7 cm).

### Two-handed carry: pulled off

- **The rule:** a hand has let go of a prop held in both hands when both of these hold:
  - it is further from its grip on the prop than Two-Handed Hand Drift (8 cm) plus `vr_carry_two_hands_detach`
    (3 cm): its drawn hand can no longer be on the grip;
  - its fist no longer touches the prop, within 2 cm: pulled away, not sliding along it.
- **Both off:** it drops, falling with the motion it has. It's not a throw: a haptic tick in both hands, no pickup.
- **One off:** the other hand keeps it (the hand-over), moved onto that hand's grip so it doesn't float off it.
- **One hand pulled away from the other:** the prop, centred between the hands, leaves both grips alike, so both are
  off together. The hand that moved less than half as far as the other (less 3 cm, from the body, since it was last
  on its grip) keeps it. Because the prop follows the moving hand half-way, that hand lets go about 22 cm from where
  it held it.
- `vr_debug_carry 2` prints each hand's distance from its grip, how far it moved, and "pulled off".
- **Checked** (mock, a health box in both hands; `pull_*.txt`, `pull_before_after.png`):
  - Both hands pulled apart 2 cm a step each: dropped at 12 cm each (it fell to the pad below). Before: still held at
    24 cm each, floating between the hands.
  - The off hand alone pulled away: it lets go at 22-24 cm and the box stays on the main hand's grip (at the main
    hand's height, held). Before: held in both, floating.

### Brushing weapons: the fingers

- **Before,** the free hand pushed out of the other hand's weapon moved only as a whole: the push came from the
  deepest of the palm's middle and the fingertips, and the fingers kept the controller's curl.
- **Now,** within a finger's reach of the weapon, the fingers are solved against it: the grasp's solve (closing
  from open), with the palm where it is.
  - A finger closing onto it **stops on its surface**: it rests there, as far as the controller closes it.
  - A finger in it when open **bends out** of it: it curls to where it's clear.
  - A finger in it at every curl keeps its last clear pose, and pushes the hand out by its tip.
  - The push comes from the palm's middle, the knuckles and those stuck fingertips.
  - The thumb takes the solve's turn.
- **Stable:**
  - It's solved again only when the hand has moved on the weapon by Refit Threshold (as a held thing's grasp).
  - Every change is eased by Pose Blend.
  - Mock sweep: the off hand crossing the shotgun's barrel 1 mm every 2 frames, half-closed, 8 cm. There were 35
    solves in 160 frames, no joint reversal over 0.03 curl, and none at rest (`jit_after_trace.txt`).
  - Before the stuck-finger hold, a finger snapped from 0 back to the controller's 2.5 as it went in. That's fixed.
- **Cost:** 40-290 µs a solve on the shotgun (1046 triangles). The dearer solves are the ones with fingers in it at
  every curl. That averages about 20 µs a frame while sweeping, and nothing while still or away from the weapon.
- **Fingers Brush Weapons** (Hands page, `vr_hand_collide_fingers`, on). Off: the old push by the fingertips.
- **Checked** (`brush_before_after.png`: top view, the off hand at 4 and 7 cm towards the barrel, open and
  half-closed):
  - Before, the fingers go into the gun (half-closed), and at 7 cm the hand is cut by it.
  - Now the fingers rest on its side or open out of it, and the hand stays out.

### Checks

- The melee canary eval: 40/46, no differences from the baseline.
- Builds with 0 warnings.

### Not verified

- **In a headset.** The fist's volume matches the drawn fist in the mock, but whether 0 cm feels right is for you
  (Grab Distance Bias).
- **The menu pages by hand.** The cvars were checked; the pages weren't clicked.
- **Grab test cost.** It wasn't profiled with `vr_profile`: 93 spheres against the triangles near the fist, a few
  hundred at most for the models tested.
- **Multiplayer:** a remote player's fist and two-handed detach.

### Try

- [ ] Reach for a box as in your screenshot: it's taken only when the drawn fist touches it. Tune Grab Distance Bias.
- [ ] Hold a box in both hands and pull them apart: it drops once the drawn hands leave their grips (3 cm past the
      drift). Pull one hand away: the other keeps it.
- [ ] Brush the free hand along a held shotgun's barrel with the fingers open and half-closed.
## Items as physics pickups; sinking; spinning shapes

Your three notes: the map's weapons, keys, the hazmat suit and similar made like the armour (force-grabbable, a
physics object once touched, taken at a holster; weapons yours as soon as you grip them; powerups unchanged); newly
placed or dropped weapons and props resting a little inside the floor (the firing range's weapons); the spinning
guns' physics shape not turning with the model. Branch `agent/items`. Scratchpad `items/`: the scripts (`t1.sh` ..
`t4.sh`, `sink.sh`, `spin.sh`, `drops.txt`) and the composites.

### 1. Pickups as objects (`QC/items.qc`, "Pickups as objects"; `QC/vr_carry.qc`)

The armour's code (round 20, "Wearable armour") is now for every pickup that becomes an object (`VR_PickupObj_*`):

| Pickup | Taken by |
|---|---|
| Armour (green, yellow, red) | carried and let go of over your torso, as before (`vr_armor_wear`) |
| Weapons (id's seven, the mission pack's Mjolnir, laser cannon, proximity gun) | gripped: it is that hand's weapon at once (`weapon_touch`: its ammo, "You got the Shotgun", the sound), as a thrown weapon is; a force grab's catch the same. Touched without a grip, or poked with a gun, it is knocked loose |
| Keys (silver and gold, every world type: medieval, runic, base; the mission packs use id's), the end-of-episode rune, the biosuit, the wetsuit (Scourge of Armagon), the Horn of Conjuring | carried and let go of at a hip or shoulder holster (`VR_PickupObj_Holster`); never the trigger, whatever `vr_carry_take` says. Let go of anywhere else, it drops (thrown with the hand) |
| Quad damage, pentagram, ring of shadows, the mission packs' empathy shields, power shield, anti-grav belt, vengeance sphere, random powerup | unchanged: as before |

- **Hanging.** Placed on the floor by the map, each hangs with its middle at `vr_item_float_height` (26 units, as the
  armour; a honey floating item where it is), spinning as the map's pickups do, not a rigid body. Its Box3D shape is a
  kinematic body of its drawn hull that turns with it (3. below). A hand gripping it takes it (weapons) or carries it
  (the rest); a hand or a gun touching it without a grip knocks it loose; a force grab pulls it. From then on it is a
  physics object like the ammo boxes: a Box3D prop, carried, thrown, two-handed, floats in water. A missed force grab
  now falls as a prop (it was hung again wherever it had got to after 0.4 s).
- **The same pickups.** Taking one runs its own touch through `itemTouch`, so the sound, the message, the screen
  flash, the item's targets (a key that opens something when taken), the deathmatch respawn and the secret count are
  the originals'.
- **A key you have** (single player: a second one of the same colour): let go of at a holster, it drops there with a
  dull knock and a double buzz, as the armour you can't wear.
- **Coop**: a key taken at a holster stays for the others (Quake's rule): it goes back to its place, hanging, for the
  next player. A weapon gripped in coop (or deathmatch 2) stays too; caught from a force grab it goes back to its place.
- **Walking over them** does not take them for a player with tracked hands; bots and flat-screen players still take
  them by touch (co-op and deathmatch with bots work). Weapons with `vr_body_interactions 1`: as before (walking into
  one puts it in an empty hand).
- **Deathmatch.** Taken, each comes back after its usual time (weapons 30 s, the suit 60 s, armour 20 s) hanging where
  it was placed (`VR_PickupObj_Regen` replaces `itemTouch`'s and the powerups' own `SUB_regen`, which would have left a
  trigger where it was taken). Knocked away and left, it goes back after `vr_forcegrabbable_return_time_deathmatch`
  and hangs there again.
- **Its turn.** While it hangs, the server keeps its yaw on the drawn one (100 degrees a second, as `cl_main.c`'s
  `bobjrotate`), so a grab, a knock or a force grab goes on from the turn you see (it used to snap to the map's yaw).
- **The sparkle** of the map's weapons goes on while they hang and lie about.
- **The setting.** Carrying and Gibs > Armour and Pickups > **Weapons and Keys** (`vr_item_objects`, default 1; 0:
  touching takes them, as before). Which kind a map's pickup is is chosen as the map loads; turned off during a map, a
  hand touching one takes it. `vr_carry 0` also turns them back.

Two fixes on the way, both in the armour's code:

- Its placement (0.3 s after the spawn) could run before `PlaceItem` as a map loads (a map's first frames are long):
  the armour never got `itemTouch` (no deathmatch respawn, no targets fired), `FL_ITEM` or its drop to the floor. It
  now waits for `PlaceItem`.
- `self.vr_wear_floats = a || b` stored 0 (the fteqcc short circuit in an assignment already noted this round):
  rewritten as an `if`.

### 2. Sinking props

**Measured** with a new command, `vr_physics_sink [what]`: each prop's drawn model (its lowest corners, where the
entity is) against the floor under them, and its collision shape's lowest point against the drawn one. Positive
"sunk" is into the floor.

**Why they sank** (in order of size):

1. **Most weapons were not rigid bodies.** Only a weapon thrown by hand was (`vr_rigid 1`). Monsters' dropped guns,
   ammo boxes' weapons and the firing range's weapons (`func_weapon_grabbable`) were Quake's toss, landing on a cube
   round their origin (`WeaponIdToThrowBounds`) that the model, turned any way (they start at random angles), reaches
   well out of: up to 20 units in the floor, 2.8 on average. Now every `thrown_weapon` is a rigid body from its making
   (`MakeThrown`), its tumble its spin. The range's weapons start still (they fall 10 units and settle).
2. **Their Box3D hulls were three times too big.** `weapons::modelTransform` (the drawn weapon's scale) only applied
   when the *client* was connected with Quake VR's protocol; a map's first frames run before the client connects, and
   the hulls made then (from the view models at their own size) were kept. The guns then rested up to 11 units above
   the floor, or fell through it (4 of 26 in the firing range). It now also takes the local server's protocol, and a
   prop's shape is made again when its drawn box changes (a weapon's settings changed while it lies there).
3. **Weapons from ammo boxes were made at the box's origin**, a corner on the floor: half in it, and Box3D's contacts
   against the world's one-sided triangles can't push out a body made that deep (it stayed 9 units in). They are
   made on top of the box now.
4. **Hulls short of the drawn surface.** A hull is a subset of the drawn corners (Box3D's quickhull stops at its
   vertex budget: 32 for weapons, 16 for the rest), so a corner left out is drawn inside what the body rests on: a
   corpse's 16-corner hull left its back 1.3 units in the floor, a head 1.0. The budget now grows until no drawn corner
   is more than 0.2 units out (the firing range's hulls: 11 to 44 corners; Box3D's limit is 128).
5. Box3D's contact slop is not a cause: its mesh rest offset holds a resting hull 0.005 m off the world's triangles
   (0.1-0.2 units), which is what the "after" numbers show (props rest a hair *above* the floor).

**The safety net**: a prop found in a floor when its body is made, or when it comes to rest, is lifted out straight up
by its depth (`liftOutOfFloor`: the lower half's corners inside a solid, measured to the upward-facing surface above,
if that is below the prop's middle, so never onto a table it lies under). In the tests it lifted 2 of the firing
range's 26 weapons as they were made (0.5 and 1.8 units in), a grunt's dropped gun (1.9) and gibs that became rigid
bodies where Quake's bounce had left them (0.1 to 10 units in: a gib is Quake's bounce until it first lands); nothing
that came to rest.

| vrfiringrange, 6 s after the load (`vr_physics_sink`) | Before | After |
|---|---|---|
| the range's 26 weapons and the ammo boxes' weapons, Box3D | 2.79 units in on average, 19.9 at most | 0.12 units above on average, 0.09 in at most |
| every prop (82-101: boxes, armour, weapons, corpses, gibs, heads) | 1.67 in at most (corpses 1.1-1.3) | 0.09 in at most |
| the same weapons, the old solver (`vr_physics_engine 0`) | as Box3D's before (Quake's toss) | 0.5 above on average, 0.6 in at most |
| grunts killed and gibbed by a blast (`drops.txt`): corpses, heads, gibs, dropped guns | 1.26 in at most | 0.09 in at most |

Composites: `sink_compare.png` (top before, bottom after: the range's weapons from above; before, a Mjolnir's handle
and guns stuck into the tables; after, lying on them).

### 3. Spinning pickups' shapes turn (`vr_box3d.cpp`)

The map's pickups hanging in the air (the "fixtures" of the Box3D polish: kinematic bodies of their drawn hull that
props rest on and knock against) kept the map's yaw while the model spins on the client (`EF_ROTATE`: 100 degrees a
second). A fixture of an `EF_ROTATE` model now turns with the drawn yaw every server frame (moved as a kinematic body,
so what touches it is pushed as by a turning thing). The object pickups of 1. hanging are fixtures too (they had no
body at all while hanging: the armour, and now the weapons and keys). The shape follows the server's clock; the drawn
model the client's, a frame behind at most (1.4 degrees at 72 Hz).

`spin_compare.png`: the firing range's biosuit with Show Physics Shapes, three shots 0.3 s apart; before (top) the
grey hull stays side-on while the suit turns out of it; after (bottom) it turns with the suit. `spawn1_crop.png`: a
shotgun and two keys, the same.

### Tests (mock headset; scratchpad `items/`)

| Case | Result |
|---|---|
| a silver key spawned 25 units ahead (`vr_physics_spawn item_key1 25 -1`), hanging, spinning | a fixture turning with it (its yaw 81, 177, 148... as listed) |
| gripped by the off hand, carried to the left hip holster, let go | "carry: taken", "silver key taken at a holster", "You got the silver key" (`t1.png`) |
| a second silver key, the same | "not taken (you have it)": dropped, lies on the floor |
| a gold key let go of in front | falls, rests 0.10 above the floor |
| the biosuit to the holster | "You got the Biosuit" (the view turns green) |
| a shotgun gripped | "Shotgun taken as a weapon", "You got the Shotgun", in the hand (`t4.png`) |
| force grab of a gold key (`t2.png`), then to the holster | flies 84 units in 0.4 s, caught, carried; taken at the holster |
| force grab of a shotgun | caught: it is the hand's weapon |
| force grab of the biosuit, missed | falls as a prop, rests 0.09 above the floor (it hung again where it landed, 1.3 in, before the fix) |
| coop: a key taken at the holster | "left for the others": back at its place, hanging; you have the key |
| deathmatch (dm3): a shotgun gripped; the biosuit taken at the holster; the suit knocked away | back hanging at their places after 30 s and 60 s; the knocked one sent back |
| `vr_item_objects 0` | a key taken by the hand's touch, a shotgun by a grip, as before (they still turn in Box3D) |
| the melee canary (`eval.sh`) | 40/46, no difference from the baseline |

QC: 0 warnings.

### Settings and commands

| Menu | Cvar | Default | |
|---|---|---|---|
| Carrying and Gibs > Armour and Pickups > Weapons and Keys | `vr_item_objects` | 1 | 1 objects (as above), 0 touching takes them (next map) |

`vr_physics_sink [<number | classname | props>]` (the drawn models against the floor, the shapes against the drawn;
the summary's average and most are of those within 4 units of a floor); `vr_physics_spawn <classname> [<distance>
[<left>]]` (a map entity made by its spawn function on the floor ahead of you: a key, a weapon, a powerup); with
`vr_debug_box3d 1`, `vr_physics_list` also prints each one's movetype, solid, rigid and flags.

### Not verified

- The headset: the reach for a small key (8 units wide: the hand's reach test is the grab's own, unchanged), the
  holster let-go with a key, the look of a weapon gripped from the air.
- The hand's knock of a hanging weapon or key (the mock's hand has no speed; it is the armour's `VR_Carry_Nudge`,
  unchanged).
- The rune in e1m7 and friends, the horn in hip2m1 and the wetsuit in the mission pack's maps were not played to (their
  spawn functions are marked the same way; the horn's `SUB_UseTargets` runs through `itemTouch` as before).
- A bot taking one by touch (the path is the armour's, which was checked in round 20).
- The old solver (`vr_physics_engine 0`) with hanging pickups: it has no kinematic bodies, so nothing turns there.

### In the headset

- Reach for a hanging shotgun and grip it: it should be your weapon at once. Knock one with an open hand: it falls.
- Grip a key (or force-grab it), bring it to a hip or shoulder holster and let go: the key sound and "You got the
  silver key". A key you have should knock and drop.
- In the firing range the weapons on the tables should lie on them, none cut by the table top; drop and throw some.
- Show Physics Shapes: the grey outlines of the hanging items should turn with them.

## Gadget stats; gadget on the forearm; torch grip

Three of your notes:

- more on the gadget's FPS counter, as fpsVR shows it, to see spikes;
- the gadget spinning when the wrist is bent hard: it should follow the forearm, not the hand;
- the flashlight taken from the belt should always be in the overhead grip.

Branch `agent/gadget2`. Composites and logs are in the scratchpad's `gadget2/final/`.

### FPS counter: Detailed

Graphics > Performance > **FPS Counter on the Gadget** is now **Off / Basic / Detailed** (`vr_gadget_fps` 0 / 1 / 2).
Basic is the one line as before. Detailed:

```
 90 FPS   90 HZ   0 LATE
 MS  NOW  AVG  MIN  MAX
CPU  5.2  5.1  4.8  7.9
GPU  8.1  8.0  7.6 12.3
CPU  (graph of the last 3 s)
GPU  (graph of the last 3 s)
```

- **FPS:** frames a second over the last quarter second.
- **HZ:** the headset's refresh, as the runtime says it (`-` in the mock).
- **LATE:** frames in the last 5 s whose period ran over 1.25 refreshes. Each is a refresh the runtime filled by
  showing the previous frame again (reprojected).
  - OpenXR doesn't report reprojection itself under SteamVR. This counts the same thing from our side.
  - The memory log's `slow_frames` uses the same test.
- **CPU:** our work in the frame, without the runtime's and the swap's waits (the memory log's `busy_ms`).
- **GPU:** the two eyes' drawing (`gpu_eyes_ms`).
- **NOW:** the last frame. **AVG:** the last quarter second. **MIN** and **MAX:** the last 5 seconds.
  - The GPU's NOW is a few frames old, because its timestamps are read back later.
- **Budget:** one refresh period. The mock doesn't tell its refresh, so there 90 Hz is assumed.
  - A figure over the budget sits on a lit block, and so does LATE when it isn't 0.
- **Graphs:** the last 3 seconds, newest on the right.
  - Each column shows the worst frame over it. A long frame covers every column it lasted, so a hitch is as wide as
    it was.
  - They go up to twice the budget. The dotted line is the budget. What's over it is brighter, with a lit top.
- **Refresh:** the figures change four times a second, so they can be read. The graphs move twenty times a second.
- **Size:** 26 characters wide, the same 5 mm characters as Basic, about 13 × 5 cm under the gadget, in the screen's
  colour and the hologram's look.

**How:** the phases timed every frame anyway (the memory log's) now also keep each frame in a ring of 1024 frames
(`profile::frameSample`: 7 s at 144 Hz). A frame's GPU time is filled in when its timestamps come back. Hitches are
kept too: they are the spikes you want to see. There is no new timing code.

**Cost** (`run.sh --exclusive`, mock, e1m1, the gadget raised and facing the camera; 6 alternating 900-frame blocks each;
`cost_log3.txt`):

| | Off | Detailed |
|---|---|---|
| CPU `busy_ms` | 0.949 | 0.935 |
| GPU `gpu_eyes_ms` | 0.383 | 0.397 |

- The CPU difference is noise.
- The GPU's +0.014 ms is the image being drawn again. `vr_profile` (`profile_final.csv`) shows:
  - the image: 0.012 ms GPU and 0.003 ms CPU a frame on average, drawn 0.28 times a frame (at most 0.08 ms GPU);
  - its quad in each eye: 0.005-0.006 ms, as in Basic.
- An earlier 3-way run had two blocks with hitches of 0.2 s from outside the game. They landed in Detailed blocks
  that followed another Detailed block, so no mode switch was involved. The 12-block rerun above had no hitches.

**Tested** (`fps_states.png`, `fps_in_view.png`):
- **Steady:** the mock paces at 64 Hz against the assumed 90 Hz budget, so every frame counts as LATE (about 320 in 5 s).
- **`host_maxfps 45`:** the rate halves, and LATE drops with it (fewer frames).
- **A 0.36 s spike from `timerefresh`:** CPU MAX 356 on a lit block, and a wide block in the CPU graph.
  - The GPU graph has a gap there: the GPU isn't timed for a hitch frame.
- **Basic:** unchanged.

### The gadget follows the forearm

**Why it spun:**
- The gadget's axis was the forearm's, but its turn about the forearm came from the back of the hand, projected
  across the forearm.
- Bending the wrist tilted that projection. Past about 60° of extension, the back of the hand crossed the forearm's
  plane and the projection flipped: the spin.
- With the body off, the gadget followed the hand one to one.

**Now** (`setupGadget` in `vr_view.cpp`, `avatar::forearmFrame` in `vr_avatar.cpp`):
- **Along the forearm:** the gadget lies on the forearm's axis, from the elbow to the wrist.
- **Its turn:** the forearm's own twist where the gadget sits.
  - That is the twist joints' share of the hand's roll there, interpolated between the joints. The bracer under it
    turns the same way.
  - At the default place, 8.5 cm behind the wrist, the share is about two thirds.
- **What moves it:**
  - Rolling the hand turns the forearm, and the gadget with it.
  - Flexion, extension and deviation don't turn it.
- **Body off:** the arms are still solved with the same IK, but not drawn (`avatar::solveArms`: torso and arms, no legs
  or skinning, 0.003 ms a frame). The gadget uses that forearm.
  - Here the elbow doesn't swing to ease a bent wrist (Wrist Limits). There is no drawn arm to spare, and a real
    forearm stays still while the hand bends.
- **The armfix change:** the frame reads its twist joints (`foretwist1..4`) as posed. It stays right if those joints'
  shares change.
- **Along the Arm** still moves the gadget, and the twist is read at its new place.

**Measured** (`gadget_bend_before_after.png`, `bend_*.txt`):
- **The test:** the off hand turned about the wrist, which stays put, from a straight wrist. The bends are about the
  forearm's own axes. The number is how far the gadget turned from the straight wrist.

| Wrist | Before, body on | After, body on | Before, body off | After, body off | Before, Wrist Limits 0 | After, Wrist Limits 0 |
|---|---|---|---|---|---|---|
| flexion +60° | 19° | 20° | 60° | **3°** | 14° | **3°** |
| extension 60° | 17° | 16° | 60° | **2°** | 16° | **2°** |
| flexion +85° | 38° | 35° | 85° | **6°** | 47° | **6°** |
| extension 80° | 36° | 33° | 80° | **4°** | 51° | **4°** |
| deviation ±30° | 11°, 7° | 11°, 7° | 30° | **2°, 1°** | 3°, 2° | **2°, 1°** |
| flexion 60° + deviation 30° | 37° | 34° | 67° | **12°** | 35° | **12°** |
| twist ±45° | 49°, 46° | 40°, 34° | 45° | 31°, 34° | 45°, 46° | 31°, 34° |

- **Body off, and Wrist Limits 0:** a bent wrist hardly moves the gadget now.
  - What is left is the IK's elbow moving a little with the hand, the forearm's own axis by 2-6°.
  - Before, extreme bends turned it 47-51° (the flip).
- **Twist:** it follows at the forearm's share (two thirds) instead of all of the hand's roll.
- **Body on with Wrist Limits 1 (yours): not fixed, and not by the gadget.**
  - The armfix change swings the elbow when a bend strains the wrist. The drawn forearm itself then turns 16-34° for
    these bends, and the gadget sits on it.
  - The gadget's own turn now equals the forearm's (the "axis" figures in `bend_after_on.txt`). Before, it was the
    forearm's turn plus the projection's.
  - For the gadget to stay put, the IK would have to leave the forearm alone for ordinary bends. That is the
    armfix agent's area. Lowering Body > Wrist Limits is the knob until then.
  - Recommended: have that swing respond to roll only, or start later for flexion.

### Flashlight: the grip it is taken in

- **From the belt, always the overhead grip:**
  - This is whatever that hand held it in last.
  - It also applies if the hand catches the torch on its cord on the way back to the belt.
  - The torch hangs lens down there, and a hand coming down on it closes round it that way.
- **Off the head or a gun:** the grip that keeps its beam nearer where it points now.
  - From a temple it goes on lighting ahead, and from a gun along the barrel, instead of turning over as it comes
    into the hand.
  - The choice depends on how the hand comes at it. In the mock, taking it off a temple with the hand held four
    arbitrary ways, the grip picked was the nearer of the two every time (the beam then 40-100° off, as those
    orientations allowed; a hand closing round the tube naturally leaves one grip close to it).
  - A hand under the shotgun like a foregrip took the low grip: its beam at 0.81/-0.59 against the gun's
    0.77/-0.64, the same line. Held across the gun, the overhead or the low grip, whichever was nearer.
- **In the hand:** B/Y still flips the grip.
- **No spin:** the torch arrives in the chosen grip; it isn't flipped into it.
- **Logging:** `developer 1` prints the grip chosen and why.

**Tested** (`torch_belt_before_after.png`, the off hand, from the side):
- **First take from the belt:** before, the low grip; after, the overhead one.
- **B/Y flip, let go, take again:** after, overhead again every time. Before, it kept the grip it was let go in.
- **Head and gun:** the takes above are in the log only; their placement wasn't composited.

### The gadget remodelled: straps round the forearm

Your note: "improve the wrist gadget model: straps that circle the entire forearm instead of the small bits of metal,
and a bit more interesting, the same art style and dimensions."

**The straps** (`vrgadget_strap.mdl`, `make_gadget.py`):
- Two leather bands go all the way round the forearm, under the casing's ends. Each has a buckle and two rivets on
  the little finger's side.
- They are separate from the casing and follow the forearm itself:
  - its frame where each band sits, turning with the forearm's twist there, as the bracer does;
  - not your offsets and turns of the gadget. The casing rests on them.
- **Fitted to your build** (`avatar::forearmGirth`: make_vrbody.py's bracer rings, lean, athletic or brawny):
  - Each band sits 1.5 mm off the bracer's ellipse there. The bracer's 12-sided rings lie inside that ellipse, so a
    band never sinks in.
  - The bracer narrows from its cuff to the wrist, so a band is a cone as steep as the bracer under it.
  - The band model's frame 1 is the steepest cone. The engine blends it towards frame 0, a cylinder, by the bracer's
    taper there (the zero blend).
- **Body off:** the athletic forearm's size, round the forearm the gadget uses (the same IK).
- **Scaled exactly:** view entities gained an exact per-axis scale (`ViewEntity::scale`, applied in
  `vr_render.cpp`). An entity's own scale is a byte in sixteenths.

**The casing** (`vrgadget.mdl`, the same 3.8 × 2.6 × 0.7 outline, the same screen and bezel):
- a lower shell and a lid, parted by a dark seam;
- four screws in the lid at the ends;
- the two dials on the right end, as before;
- two buttons on the lower side;
- grip grooves across the left end;
- a short antenna on the left end;
- the hologram's emitter: a slot of dark glass in a metal frame along the top edge, over the screen, where the
  hologram rises;
- under it, two riveted lugs that the straps pass under.

The old strap stubs are gone. 26 closed pieces, 720 triangles (the strap: 272).

**Anchors:**
- The screen, the hologram and the FPS text are placed by code from the same numbers (`screenRect`, `screenCentre`,
  `gadgetTop`), which don't change.
- The model's screen and bezel corners, decoded from the old and new files, are the same within the file's grid
  (0.007 model units, 0.2 mm).

**Also fixed: the gadget drawn at its exact size.**
- The casing was drawn at the entity's byte scale, rounded down to sixteenths, while the screen's image used the exact
  scale.
- At Size 1.3 with world scale 1 (scale 1.04), the casing was drawn 4% small under its screen.
- It now uses the exact scale too.

**Checks:**
- `check_mdl_holes.py`: both models ok (no open loops, cracks or flipped edges).
- Composites: `gadget_model_before_after.png` (your view with the gadget raised; from the top side, the lower side
  and underneath; lean, athletic, brawny and body off; before and after), and `model_after_defaults.png` (the shipped
  placement: Size 1.3, turned 35° and moved across: the casing turns on the straps, which stay round the forearm).
- Not verified: just past the bracer's cuff (Along the Arm towards the elbow) a strap keeps the cuff's size, up to
  3 mm off the skin there.

**In the headset:**
- [ ] Look at the gadget from all sides and twist your forearm: do the straps hug the bracer, without gaps or
      poking through, for your build?

### Not verified

- **Legibility in the headset:** only checked in the mock, at 960 × 540. The Detailed counter is denser than the Basic
  line.
- **LATE on a real runtime:** needs a real refresh period (SteamVR). In the mock every frame counts as late against
  the assumed 90 Hz.
- **The gadget with your hands:** the bends were simulated by turning the controller about the wrist. Your real
  forearm may differ from the IK's.

### In the headset

- [ ] **Detailed counter:** Graphics > Performance > FPS Counter on the Gadget > Detailed.
  - [ ] Readable at arm's length?
  - [ ] Does HZ show your refresh?
  - [ ] Is LATE 0 when it feels smooth, and does it rise when it stutters?
  - [ ] Load a big map, or turn on something heavy: the spike should show in MAX and in the graph.
- [ ] **Wrist bends:** raise the gadget and bend your wrist hard up, down and sideways. With the body on, it turns only
      as much as the drawn forearm does.
  - [ ] Try Body > Wrist Limits at 0: the gadget should then stay put.
  - [ ] Then roll your hand: the gadget turns with the forearm.
- [ ] **Body off:** the same bends barely move it.
- [ ] **Torch grips:**
  - [ ] Take the torch from the belt after flipping it to the low grip: it comes in the overhead grip.
  - [ ] Take it off your head and off a gun: does the grip it comes in feel right?

## Editing the body, the weapons and the gadget in Blender

Your note: "Could you also enable me to edit other models in Blender? Specifically the body/legs/arms, the weapons, and
the wrist gadget."

A second add-on, **Quake VR Models** (`Misc/quakevr/blender/addons/quakevr_models`), next to the hand's. The guide:
[MODELS_IN_BLENDER.md](MODELS_IN_BLENDER.md).

- **The body** (the three builds' `.md5mesh`, `.md5anim` and the 16 `.tga` skins): the armature and weights, as for
  the hand.
  - The joints are the engine's, so the export keeps them and refuses a moved or renamed bone. **Apply Pose to Mesh**
    bakes a pose into the mesh.
  - Skins: you paint the plain one, and the export carries your changes into the 15 armour and damage skins, under
    their plates and wounds. The skins stay on Quake's palette unless you untick it (the engine takes full colour).
  - **The Other Builds Too** makes the same edits in the other two builds.
- **The weapons and the gadget** (`.mdl`): frames as shape keys, the skin, UVs and onseam vertices.
  - Frame 0's edits are carried into every frame by each part's motion (recoil, pump, spinning barrels).
  - The header's scale and origin, the frames and every old vertex's index are kept. New triangles get vertices of
    their own, so the anchors' strip order doesn't change.
  - After each export: every anchor before and after (vr_weapons.inc, vr_shells.cpp; the gadget's screen, emitter and
    lugs), and `check_mdl_holes.py`. A moved anchor warns. An anchor that would name another vertex refuses the
    export, unless you tick Remap Anchors (it prints the new `vr_wofs_*_av` values).
  - A vertex outside the model's byte box is refused, unless you tick Grow the Box (it prints the offsets that keep
    the weapon in place).
- **`vr_model_reload [model ...]`** reads the weapons, the body and the gadget again, and forgets what was worked out
  from them (strip orders, grasp shapes, collision triangles, the body's bone check). No restart: a map change
  doesn't read a model again.
- **The generators don't overwrite your edits** (`Misc/quakevr/genguard.py`, `generated.json`): `make_vrbody.py`,
  `polish_weapons.py`, `make_gadget.py`, `make_hand_rig.py` (and the earlier weapon scripts) stop and name the files
  you edited. `--keep-edited` writes the rest; `--force` overwrites. **Your hand edit (0d5c225b) is protected:**
  `make_hand_rig.py` now refuses to overwrite it.

### Tests (headless Blender 5.2, then the mock; scratchpad `blendall/`)

- **Unedited round trips, byte for byte:** all 22 `.mdl` (every `v_*.mdl`, `vrgadget.mdl`, `vrgadget_strap.mdl`) and
  the three bodies with their 16 skins. The same after a second export from the same scene, and through the
  operators in a fresh Blender profile (installed from the script directory, and from a zip).
- **The shotgun edited** (a part stretched 1.3× and raised, the skin repainted): the anchors unchanged, no new holes.
  `vr_dumpview` holding it is the same as with the shipped gun, but for a strap's 0.001 jitter that two runs of the
  shipped gun show too. It draws edited in the game.
- **Loud cases:**
  - moving the muzzle's vertex: "MOVED ... 0.333 units";
  - deleting an early triangle: refused (4 anchors renamed), then with Remap Anchors the new indices;
  - an extrusion: new triangles appended, the order unchanged below index 361;
  - a vertex out of the box: refused, then grown (the offsets printed).
- **`vr_model_reload`:** the shotgun and the gadget replaced while the game ran, then reloaded: drawn edited. The same
  for the body (`vr_model_reload vrbody`).
- **The body edited** (the athletic chest pushed out 1.25×, the skin repainted): the body preview (from the front and
  the side) draws it, with its arms, legs and holstered guns; `vr_dumpview` is the same. The Other Builds Too moved
  the same 131 vertices of the lean and brawny files (checked headless, not drawn).
- **The gadget edited** (its dials scaled): the screen, emitter and lugs unchanged.
- **The guard:** `make_hand_rig.py` refuses (your 6 hand files); a body file edited is refused, kept with
  `--keep-edited`, overwritten with `--force`. Every other generator's output matched its file when the manifest was
  recorded.

### Not verified

- **Blender's interactive UI:** only headless, through the operators. The sidebar panel and the warning popup weren't
  seen.
- **Painting in Texture Paint mode:** it paints the same image, but the tests painted it by script.
- **Colours under the game's light:** a painted blue on the gadget casing looked orange in e1m1's torch light (blue in
  `r_fullbright 1`); the file holds the colour painted.

### In the headset

- [ ] Import a weapon, move a part, export, `vr_model_reload`: does it draw edited, and is it held as before?
- [ ] Paint the body's plain skin, export, `vr_model_reload`: do the armour skins show your paint round their plates?
## Arm IK with calibrated hands

His note: after he calibrated his hands to match his real ones (Gun Angle 70 instead of 39.5, Gun Yaw 0, X -4, Y 0.58,
Z -2.5 cm, the off hand mirrored), the arm, elbow and shoulder IK looked "completely screwed up". Branch `agent/armik`;
scripts, traces and composites are in the scratchpad's `armik/` (`final/` for the results).

### Why

- **Not the controller's axes.** The arm IK already worked from the drawn hand: the wrist target is the drawn hand's
  wrist joint (after the calibration), and the wrist's bend is the drawn hand against the solved forearm. For the same
  drawn hand, both calibrations gave the same arm to the millimetre.
- **What changed is the hand it gets.** Gun Angle 70 turns the drawn hand 30.5° about its palm's normal: for a
  gripping hand that is ulnar deviation (the knuckles tilt towards the little finger).
  - Measured on 28 of his takes (his real controller motions, replayed under each calibration), against the forearm
    the elbow's pole alone gives: with his calibration the wrist sits on average in 10-30° of extension and 11-22° of
    ulnar deviation. That is the textbook posture of a hand gripping a handle, so the calibrated hand is plausible.
  - With the default one it read as radial deviation instead (-9 to -16° on average), which real wrists barely do.
- **The cause: the wrist-limits model (round 21's "the elbow eases the wrist").** It judged the wrist against a
  symmetric range centred on a straight wrist: 75° of flexion or extension and 35° of deviation either way, strained
  from 60% of it, so from 21° of deviation.
  - His natural ulnar deviation of 20-35° was "strain", and the elbow swung round to ease it, often by 30-50°: tucked
    in across the chest in a guard, or lifted out to the side.
  - With his calibration the elbow swung more than 10° in 51.5% of the frames of his takes, and more than 30° in
    24.3%. With the default one, 43.9% and 13%.
  - The same model also swung the elbow for ordinary flexion and extension, and the wrist gadget turned with the
    forearm: your other note.
- **A smaller leak:** the pole's hand term (Elbow Away from Hand) used the thumb's direction as it is. Tilting the
  hand along the forearm (deviation, or a calibration's pitch) moved the elbow too.

### What changed (`solveArm`, `wristStrain`, `wristTurn` in `vr_avatar.cpp`)

- **The wrist's range is a real one, lopsided:** 85° of flexion (towards the palm), 80° of extension, 45° of ulnar
  deviation and 30° of radial, measured quadrant by quadrant (an ellipse's quarter in each). `wristTurn` now knows the
  side, so flexion and extension are told apart (they were one symmetric axis).
- **A bend only strains 15% past that range**, and reaches 1 (as much as a 45° swing of the elbow) at 45% past.
  - A real elbow doesn't move while the wrist bends, even hard. Only a bend no wrist makes says the forearm is
    elsewhere.
  - The roll is unchanged in effect: it strains from 60° (a forearm turns about 85° either way; past that the upper
    arm turns and the elbow swings), reaching 1 at 85° (it was at 80°).
  - Wrist Limits (`vr_body_wrist_limits`) still scales the range; 0 still turns the swing off.
- **The pole's hand term takes only the hand's roll about the forearm:** the thumb's side across the forearm that the
  body's own pole gives (a first two-bone pass). Bending the wrist, or a calibration's pitch, no longer moves the
  elbow; rolling the hand still does (palm up brings the elbow in). The drawn arms and the body-off arms (the wrist
  gadget's) both use it.
- The forearm's twist joints, the wrist's mitre, the bracer, the hand bone and the gadget's `forearmFrame` are
  unchanged: they follow the solved forearm as before.
- **`vr_debug_arm`** (new, not saved):
  - 1 prints each drawn arm once: the shoulder, elbow and wrist in the body's axes, the elbow's angle and swing, the
    hand's axes, and the wrist's flexion, deviation, twist and strain against the solved forearm and against the
    pole's. It also prints them in the tracking space's axes, to set up `vr_mock_hand` poses.
  - 2 writes the same every frame to `arm_trace.txt`, with whether a take is playing.

### His takes, both calibrations (`final/takes_table.txt`)

28 takes (2 of each of 14 categories: guards, punches, stabs, slashes, gun strikes, shoves, no-hit), 23,326
arm-frames, replayed with `vr_motion_eval` from his raw controller poses. The calibration was set in copies of the takes'
headers. The swing is the elbow's turn off its pole.

| | swing > 10° | swing > 30° | mean swing |
|---|---|---|---|
| Default calibration, before | 43.9% | 13.0% | 14.4° |
| His calibration, before | **51.5%** | **24.3%** | 18.2° |
| His calibration, after | **11.7%** | **2.6%** | 3.3° |
| Default calibration, after | 13.6% | 5.4% | 4.5° |

- **Natural poses, his calibration, before → after** (swing > 10°):
  - guard (parry_pose) 79.8% → 21.9% (> 30°: 26.2% → 0.1%);
  - straight punch 73.7% → 6.7%;
  - not-a-parry pose 39.5% → 1.2%;
  - no-hit 21.2% → 1.0%;
  - jab 27.5% → 1.2%.
- **What still swings** is mostly sword and gun strikes (two-handed stab 30%, gun parry-bash 36%). There the pole's
  forearm would bend the wrist past its reach (ulnar deviation of 50-90°, or a roll past 60°).
- **The same real motion gives nearly the same arm under either calibration.** The elbow's distance between the two
  calibrations, frame by frame, went from 8.6 cm (median 7.4, 90th percentile 16.9) to 4.1 cm (median 3.1, 90th
  percentile 7.9).
  - The drawn wrist itself is 5.2 cm apart between them (the calibration moves it), and the pole alone puts the
    elbows 3.3 cm apart. So what is left is the wrist's own move, not the IK.
  - For the same drawn hand the arm is identical under both.

### Posed natural arms (mock, `ev.py`)

- **The poses:** relaxed, guard, aiming forward, reaching up, across the body, punching, and a two-handed shotgun.
  They are built as real arms (shoulder, elbow, wrist, the hand in line with the forearm), and the controllers are put
  where his calibration draws those hands.
- **Before and after, his calibration:** no swing in any of them (13 of 14 arms). The shotgun's front hand, palm up
  (65° of supination), turns the elbow 15-18° about an almost straight arm.
- **The default calibration, same controllers:** its hands are 30° off his (radial deviation 17-44°). After the fix
  they cause no swing either.

### Wrist bends: the gadget and the forearm (your second note)

- **The test:** round 21's (`gadget2/pivoted.py`): the off hand turned about its wrist, from a straight wrist, about the
  forearm's axes, with its labels.
  - Under his calibration, the controllers are placed so that the drawn hands are the same.
  - Body on, Wrist Limits 1 unless stated.
  - The number is how far the gadget turned from the straight wrist, and in brackets how far the elbow moved.

| Wrist | Before, default | Before, his | After, default | After, his | After, his, body off | After, his, Wrist Limits 0 |
|---|---|---|---|---|---|---|
| flexion +60° | 20° | 20° | **2°** (2 cm) | **3°** (2 cm) | 3° | 3° (2 cm) |
| extension 60° | 16° | 16° | **2°** (1 cm) | **2°** (1 cm) | 2° | 2° (1 cm) |
| deviation +30° | 11° | 11° | **1°** (1 cm) | **1°** (1 cm) | 1° | 1° (1 cm) |
| deviation -30° | 7° | 7° | **0°** (0 cm) | **1°** (0 cm) | 1° | 1° (0 cm) |
| flexion 60° + deviation 30° | 34° | 34° | **12°** (3 cm) | **12°** (3 cm) | 12° | 12° (3 cm) |
| flexion +85° | 35° | 35° | **5°** (3 cm) | **5°** (3 cm) | 5° | 5° (3 cm) |
| extension 80° | 33° | 33° | **2°** (1 cm) | **2°** (1 cm) | 4° | 4° (2 cm) |
| twist +45° | 40° | 40° | 50° (15 cm) | 50° (15 cm) | 31° | 31° (1 cm) |
| twist -45° | 34° | 34° | 33° (4 cm) | 33° (4 cm) | 33° | 33° (4 cm) |

- **Bends now leave the forearm still:** 0-5°, the same as with the body off.
  - The 12° of the combined bend is the twist such a turn carries (the forearm's share of it), as with the body off.
  - The centimetres left are the elbow following the wrist's own small move.
- **Rolls still swing the elbow.** The straight wrist here is already turned 57° (the palm down, reading the gadget),
  so twist +45° is a roll of about 100°, past a forearm's reach. The elbow swings out 38° and the gadget turns 50°
  with it, instead of the forearm's 31°. The other way, back towards the thumb up, nothing swings.
- On my own version of the test (bends about the hand's own axes, `final/gbtable.md`) the bends move the gadget by
  0-2°.

### Composites (`final/armik_side.png`, `armik_front.png`, `armik_eyes.png`)

- **What they show:** from the side, from the front (a spectator camera on the player's own body, so the drawn hands
  show), and from the player's eyes.
- **The columns:** the default calibration before, his calibration before, his calibration after, and the default
  calibration after.
- **The rows:**
  - the seven posed natural arms;
  - four frames of his own takes (guard, the start of a punch, a stab, a slash), from their raw controller poses.
  - The shotgun stays in the main hand after its row, in every column alike.
- **His take's guard:** before, the right elbow is lifted out to shoulder height (the swing easing his ulnar
  deviation). After, it hangs under the fist, as with the default calibration.
- In the posed natural arms the columns barely differ: nothing strained there, before or after.

### Checks

- `vr_body_wrist_limits` now swings the elbow over 10° in 1-7% of the frames of his calm takes (no-hit, not-a-parry,
  jabs, straight punches), instead of 21-74%. In his guards it's 22% (from 80%), almost all under 30°: there his right
  hand reads 60° of ulnar deviation against the pole's forearm, and the elbow moves out under the fist.
- The armfix examples, default calibration, right hand at the chest:
  - The controller rolled 90°: the elbow still swings, 15°. The twist goes from -64° to -73° and the bend from 73° to
    63°.
  - Pointed 50° up: 71° of radial deviation that no swing eases, since the arm is almost straight. The elbow swings
    14°. The armfix notes had it swinging to a 119° twist.
- The drawn hand, weapons, the melee's server points and `vr_dumpview` are untouched: only the drawn arms and the
  gadget's forearm change.

### Not verified

- **His real elbows:** the takes give his controllers, not his elbows. "Natural" is judged by the wrist staying within
  a real one's reach, and by the same motion giving the same arm under both calibrations.
- **The ranges:** 85/80/45/30° are textbook maximums. If his wrist reads past them in ordinary moves (the stabs' ulnar
  deviation), Wrist Limits above 1 widens them.
- **Melee:** not re-run. It doesn't read the body's arms, and his takes are archived.

### In the headset

- [ ] With your calibration, hold a guard, aim, punch and let your arms hang. Does the elbow stay where yours is,
      without jumping out or tucking in?
- [ ] Bend your wrist hard every way with the gadget up: the forearm and gadget should stay still.
- [ ] Roll your palm fully down and up: the elbow should swing only at the end of the roll.

## The flashlight and every other model in Blender

"Does the addon also support the flashlight?" Now yes, and every other `.mdl` (MODELS_IN_BLENDER.md, "The flashlight
and the other models").

- **The flashlight follows its model.** The game used to place the beam, the lens's glow, the belt clip, the cord
  and the gun clamp by numbers matching the shipped torch. It now reads them from `vrflashlight.mdl` as it loads it:
  the lens (the part painted fullbright in skin 1), its centre and radius; the tail; the switch; the outline. Move or
  enlarge the lens and the beam and light start there, that wide. With the shipped torch the values are the same;
  on the shotgun it sits 1 mm closer (8.2 cm under the barrel, was 8.3: the tube's true radius).
- **Why read it rather than check it:** the lens is marked in the model already (the "on" skin), so nothing is left
  for you to copy into the code. What can't come from the model stays a rule the export checks: +x is the beam, +z
  the switch, the origin the grip.
- **The export checks each VR model** the game places by numbers in its code (the holster's plate, the shell's rim,
  the pauldrons' skins, the ammo button, the unrigged hand, the knights' swords) and says what to change.
- **`vr_model_reload`** reads those models too, and prints the flashlight's lens.
- **Every generator that writes a model or a skin stops before overwriting your edits:** the flashlight, holster,
  pauldrons, shell, spawn buttons, detail textures, colour grades and the hands' blood skins. `make_bloody_hands.py`
  still runs after you reshape a hand, as long as you didn't paint its blood skins.

Tests:

- All 103 `.mdl` files round-trip byte for byte, twice, with no check firing. The operators were run in a temporary
  profile, with the hand add-on enabled beside them. The three bodies also round-trip.
- The torch's head was enlarged 1.4 times and its lens moved 0.5 forward and 0.3 up, through the operators. In the
  mock (belt, in the hand, on the shotgun, before and after) the beam comes out of the new lens. A lens left
  unmarked warns in the console and in the report. A switch turned to -z is refused as a CHECK.
- The guard: `make_flashlight.py` refused the edited torch, kept it with `--keep-edited` and overwrote it with
  `--force`. `make_bloody_hands.py` ran after a skin 0 edit and refused painted blood.

### In the headset

- [ ] Move or enlarge the flashlight's lens in Blender, export, `vr_model_reload vrflashlight`: does the beam start at
      the new lens, on the belt, in the hand and on a gun?
- [ ] Make the grip thicker: do the fingers still close on it?
## Align Sights to My Aim

Your request: line the in-game sights up the way you aim your airsoft guns, by "natural point of aim". Branch
`agent/sightalign`; composites and logs are in the scratchpad's `sightalign/`.

**Where:** Weapon Offsets, a new section just under Inherit From:
- **Align Sights to My Aim** (the button), then the result lines;
- **Apply** / **Cancel** once there is a result, then **Undo** after Apply;
- **Dominant Eye** (new: `vr_dominant_eye`, Right by default; there was no such setting);
- **Captures** (3, 4 or 5; `vr_sight_align_captures`, 4);
- **Show Sight Line** (`vr_show_sight_line`).

For a melee weapon (axe, Mjolnir, the swords, the empty hand) the button is replaced by "No sights: a melee weapon".

### In the headset: the eyes-closed routine

1. Hold the gun in the hand the page edits (the main hand, or the other one with Edit the Other Hand's Weapon).
2. Press **Align Sights to My Aim**. The menu closes, and the game runs.
3. Close your eyes and lower the gun. Three beeps count down; a higher beep says go.
4. Raise the gun exactly as you raise your airsoft gun, and hold still. After 0.4 s of stillness a click (the weapon
   pickup sound) says the aim was taken.
5. Lower it, and raise it again: another click. Repeat until the chime (4 captures by default).
6. Open your eyes. The menu is back on the page with the result:
   - how far off your sights were (degrees, and how far your eye was from the sight line, in cm);
   - how many captures were used, how far apart they were, and whether one was dropped;
   - the fix: how much the hand and gun turn, and how far the fist moves;
   - how much the shots turn and, for a gun with a foregrip, how much taking it used to turn the aim.

   In the game you see the sight line as it is (orange), as the fix puts it (green), and the eye captured (magenta).
7. **Apply** or **Cancel**. After Apply, **Undo** puts the values back exactly.

The menu button during the capture stops it; so do a map change or death. A capture is only taken with the gun held
up before your eye:
- the sights within 15 degrees of the eye's ray;
- the eye within 15 cm of the sight line;
- your head facing the sights within 20 degrees.

So a gun held low, or pointed at the menu, is never taken. After 45 s without a capture it gives up (or finishes, with
3 or more).

### What it changes

**The sight line.** Each gun has a rear and a front point in its model:
- **Painted sights**, found on the skin's sight texels (the fire colours) when the model is first needed:
  - the **shotgun**: two rear posts (the middle of the notch, at their tops) and a front post (its top);
  - the **double shotgun**: a rear ring (its middle, a circle fitted to it) and a front post (its top);
  - the **lightning gun** and its **plasma** alternate, which have painted notch sights too (round 20).
- **The others**, from a table (`Quake/vr/vr_sightalign_table.inc`, made by `Misc/quakevr/sightline_table.py`):
  - a line on the gun's middle, parallel to its barrel, from above the middle of the fist to the muzzle;
  - just over the highest part of the gun between them, so nothing stands above it;
  - the grapple's ends before its claws; the laser cannon's runs along its barrel (its handle and frame stand far
    above it).
- `lines_check.png`: every gun from the side and the top, with its line, its rear (yellow) and front (cyan) points and
  the fist (magenta). In the game, **Show Sight Line** draws the same on the guns you hold, the line going on to the
  wall.

**The fix** is worked out in the controller's calibrated frame, so it holds wherever you stand and look:
- The eye's place is averaged over the captures. A capture whose eye is further from the others' median than
  max(1.5 cm, 3 times their median spread) is dropped.
- The hand and the gun turn together, **about the middle of the fist**, by the least turn that takes the sight line onto
  the ray from your eye through the front sight (where you looked). The grip stays in the palm and the fingers keep
  their grip.
- They then move by the least amount (across that ray) that puts the line through the eye.
- It is stored in the gun's **Hand and Weapon Together** keys (`vr_wofs_whole_x/y/z/pitch/yaw/roll_NN`), composed with
  what they held. There is no new mechanism, and the page's sliders show the result.

**The shots follow the sights.** Shot Pitch and Shot Yaw (`vr_wofs_shot_pitch/yaw_NN`) are set so that the shots (the
red laser) run parallel to the sight line.
- They leave the muzzle, which is under the sights, so they land that far below the point the sights cover, at every
  range: 2.2 cm (double shotgun), 3.1 (shotgun), 3.6 (nailgun), 7.4 (rocket launcher).
- A zero at one range would be exact there and worse elsewhere (twice the offset at twice the range). Parallel keeps
  the offset small and the same everywhere.
- Before, the shots were 0.4 to 2.6 degrees off the sights.

**Two hands.** Two-handed aim points from the hand to the other hand (on the foregrip), moved by the gun's Aim Offset
and turned by its Aim Pitch/Yaw/Roll. It did not follow the one-handed pose: taking the foregrip turned the gun 6.6 to
10.7 degrees (in the mock, at your calibration).
- For a gun with a Grip hotspot, Apply also sets the **Aim Offset** (`vr_wofs_2h_x/y/z`): the aim then runs along the
  hand's own forward when the other hand holds the foregrip where it is drawn. The Aim Pitch/Yaw/Roll
  (`2h_pitch/yaw/roll`) go to 0.
- Taking the foregrip now turns the gun 0.001 degrees, so two-handed shooting keeps the aligned sights.
- The foregrip is fixed in the hand's frame, so this holds in any pose.
- Not for guns without a Grip hotspot, with No Two-Handed, or the swords.

**The off hand.** The keys are shared and mirrored, as all weapon offsets are.
- Aligning in either hand sets both: the other hand gets the mirror image. That is right when you shoot that hand with
  the eye on its side.
- Aligned in the off hand with the left eye, the main hand with the right eye was aligned to 0.0043 degrees in the
  mirrored pose.

**Inherit From.** As on the rest of the page, Apply writes the values the weapon uses: its own, or those of the weapon it
inherits from (the other ammo's model, the same gun: set once for both).
- The result says so ("Changes v_nail.mdl's (inherited)").
- Stop Inheriting first to keep them apart.

**Saved** with your config (the weapon keys are archived), as are `vr_dominant_eye` and `vr_sight_align_captures`.

### Tests (mock headset; the scratchpad's `sightalign/`)

The setup:
- your calibration (Gun Angle 70, hand X -4, Y 0.58, Z -2.5, the off hand mirrored), in the firing range;
- a "natural" pose that is off the sights by a known amount: the hand at (0.02, 1.60, -0.42) m, pitched 71, yawed 1;
  the head at 1.70 m, level;
- 5 captures, the gun lowered between them: 4 around that pose (pitch and yaw +-0.6 degrees) and one outlier 5 degrees
  off.

The numbers are `vr_sight_check` in that pose.

| Gun | Before: sights vs eye ray, eye to line | Fix: turn, fist move | After: sights vs eye ray, eye to line | Laser vs sight line, before / after | Two hands on the foregrip, before / after |
|---|---|---|---|---|---|
| Shotgun | 5.33 deg, 80.4 mm | 5.33 deg, 4.2 cm | **0.0043 deg, 0.067 mm** | 2.36 / 0.0046 deg | 10.71 / 0.0014 deg |
| Double shotgun | 6.80 deg, 95.1 mm | 6.81 deg, 4.5 cm | **0.0055 deg, 0.079 mm** | 0.37 / 0.0016 deg | 8.54 / 0.0005 deg |
| Nailgun | 6.18 deg, 83.5 mm | 6.18 deg, 4.0 cm | **0.0041 deg, 0.057 mm** | 2.00 / 0.0024 deg | 10.51 / 0.0009 deg |
| Rocket launcher | 4.85 deg, 86.9 mm | 4.88 deg, 5.1 cm | **0.026 deg, 0.47 mm** | 2.55 / 0.0036 deg | 6.63 / 0.0018 deg |

- The outlier was dropped each time (4 used; spread 0.25 to 0.33 degrees).
- In the dominant eye's image (2048 px), the rear and front sights land on the same pixel. The laser's dot on the wall
  is 1.3 to 2.8 px from the sight line's.
- `sightalign_before_after.png`: the right eye before and after, for each gun. The red ring is where the laser meets
  the wall.
- The nailgun held two-handed in the mock (the other hand put on its foregrip to 0.05 units, the grab taken): 0.23
  degrees and 3 mm, the precision of that placement.
- **Undo:** every key back to its exact string ("0", "0.0", "1.8"...), and `vr_sight_check` the same as before.
- **Restart:** the config written after Apply, loaded by a fresh game: the same keys, the same 0.0042 degrees (nailgun).
- **Off hand:** as above.
- **Inherit From:** the double shotgun made to inherit the shotgun. Apply wrote the shotgun's keys, and the double
  shotgun was aligned to 0.0044 degrees.
- **The menu** (`menu_flow.png`): the page before, the result with Apply and Cancel, then Undo after Apply.

### Settings and commands

| | |
|---|---|
| `vr_dominant_eye` | 0 right (default), 1 left. Saved. |
| `vr_sight_align_captures` | 3..5 (4). Saved. |
| `vr_show_sight_line` | the held guns' sight lines. Not saved. |
| `vr_sight_align [start [main or off] / apply / cancel / undo]` | the page's buttons, for scripts. Started with the menu open, it comes back to it. |
| `vr_sight_check [main or off] [size]` | the numbers above (see below). |
| `vr_sight_lines` | every weapon's sight line (model space) and where it comes from. |

`vr_sight_check` prints:
- the sight line against the eye's ray, and the eye's distance to it;
- where the sights, the line and the laser land in the dominant eye's image (pixels of a `size` image);
- the laser against the line at 2 to 50 m;
- the two-handed turn;
- the fist and muzzle in model space.

`developer 2` prints the capture's waiting: still, plausible, and the largest moves in the window.

### Choices that differ from the brief, and why

- **The lightning gun and the plasma gun** use their painted sights (they have real ones since round 20), not a table
  line.
- **Captures need the gun lowered between them** (8 cm or 15 degrees). Holding still twice in a row would give the same
  pose twice, not a second natural raise. The first capture needs it too, so the pose you pointed at the menu with is
  never taken.
- **A capture also waits for the gun's lag to settle** (the sights still to 2 mm in the controller's frame). The first
  tries were taken while the weighted gun was still catching up with the hand, and the fix was 0.13 degrees off.
- **The head must face the sights.** Without it, a gun held low was taken once: its sights happened to line up with the
  eye, pointing at the floor.
- **The fist moves 4 to 5 cm** in these tests. The brief asks for the line on the ray from the eye through the front
  sight, pivoting at the fist. The turn alone can't put the line through the eye (it turns about the fist, below it), so
  the rest is a move.
  - The other choice, a turn only, would keep the drawn hand exactly on yours, but the line through the eye would point
    elsewhere: 6 to 10 degrees off where you looked, here.
  - The result shows the move, so you can judge it. If a gun needs a large one, its sights sit higher (or lower) above
    the grip than your airsoft gun's.

### Limits

- The one-handed fix is exact for the pose you capture. Other ways of holding the same gun differ, as your real aim
  does.
- Two hands: exact with your other hand at the drawn foregrip. Elsewhere on the gun it turns the aim, as before. The
  virtual stock (Two-Handed Mode 2, with the gun hand near the shoulder) mixes in the shoulder's line and doesn't keep
  it.
- Sight lines are on the idle frame; a firing animation moves them with the gun.
- A mod's own gun models get a line only under these names. Painted sights are found on any skin of these models;
  other models have none until they are added to the table.
- The shots are parallel to the sights, not converging: a few cm low at every range (see above).

### In the headset

- [ ] Dominant Eye: is Right yours?
- [ ] Shotgun: do the routine. Opening your eyes, is the front post in the notch? Apply, then raise it again with your
  eyes closed: are the sights lined up?
- [ ] The double shotgun (the ring), then a nailgun and the rocket launcher (table lines): the same. Is Show Sight
  Line's line where you would look along each?
- [ ] How far does the fix move the fist (the result's cm)? Does the hand still sit right on the gun?
- [ ] Take the foregrip: do the sights stay lined up?
- [ ] Shoot a target at a few distances: do the shots land where the sights are (a few cm low)?
- [ ] Undo; and after a restart, are the values kept?

## Model bump maps

The author: enabling bump maps for models barely does anything. "Never compromise on quality."

**Why.** Measured first, in the mock at 2048², in e1m1 and the firing range, with `vr_normalmap_strength 0` as a flat
reference. The round-14 maps changed the grunt, ogre and shambler very little under a light from the side, and hardly
at all under the torch. There were two reasons:

- The maps were brightness taken as height. Every speck of an 8-bit skin's dithering became a bump, every change of
  paint a ramp, and dark skins came out shallow.
- The lights you see most come from about where you look from: the torch on your head or in your hand, a muzzle flash
  beside the gun. A surface lit straight on only dims by 1 − cos θ where a bump tilts it by θ (4% for 17°). Straight-on
  light shows little relief in reality too; only edges (steep bevels) and lights from the side show shape.

**What changed** (details in LIGHTING.md, "Model skins' normal maps"):

- **Better made maps for id's skins** (`TexMgr_SkinToNormals`). They are made in the engine at load, so nothing
  derived from id's skins is stored or shipped.
  - Edges: coherent ones are kept (plate edges, seams, creases); speckle below the skin's own noise level is dropped.
  - Forms: the brightness is blurred at three scales, so muscles, folds and plates read as broad shapes.
  - Paint: a change of colour counts less than a change of brightness.
  - Materials, by colour: metal is crisp and flat, flesh soft and broad, blood almost nothing.
  - Relief is relative to the skin's own contrast, so dark skins get as much shape as bright ones.
  - Bevels are compressed to at most about 50°.
  - Seams stay clean: every blur stays within the skin's islands.
- **Authored normal maps for models:**
  - Where they are looked for: `progs/<model>.mdl_<skin>_norm` (DarkPlaces' names), then `_bump` (heights), then skin
    0's. For MD5 meshes: `progs/<shader>_<ss>_<ff>_norm`, then `_00_00`'s.
  - They work under the 8-bit skin and under a full-colour replacement.
  - They have their own strength: `vr_normalmap_authored`, Graphics > "Authored Model Bumps" (1 = as authored).
  - They are not halved on held weapons and hands.
- **External full-colour skins:** `progs/<model>.mdl_<skin>`, with `_glow` or `_luma`, as model packs ship them.
- **The jointed hands and the body** (MD5 meshes, drawn by the alias shader with their bones) had no normal maps at
  all. They now get them, lit like any model: their own light and `vr_modellight`, dynamic lights, the torch and
  muzzle flashes.

**Before and after.** The sheets are in the scratchpad, `bumpmap/composites/` (left: before; right: now), for
`-Base qbase` and `qrp`:

| Sheets | What each shows |
|---|---|
| `grunt_*`, `ogre_*`, `knight_*`, `shambler_*`, `zombie_*` | mid range in the hand torch; close in the hand torch, in its own light, and with a light from the left and from the right |
| `shotgun_*`, `supernailgun_*` | own light, a light in front, a light from the side, the muzzle flash |
| `items_*` | the armour (the ammo and health boxes are brush models, so they don't change) |
| `firingrange_*` | the firing range |

The shots themselves are in `bumpmap/shots/{before,after}_{qbase,qrp}/`.

- **Lit from the side:** clearly more shape than before, and smoother. The ogre's head and arms, the shambler's ribs
  and thighs, and the knight's plates read as forms. The speckle on the thighs and legs is gone.
- **In the torch and the muzzle flash:** a little more at the edges (plates, the knight's legs), otherwise close to
  before, as the physics above predicts.
- **The hands** now show their knuckles and creases.

**Cost.** Measured on the RTX 4090, run exclusive: mock eyes at 2048² (the headset's 2064 x 2208 has about 8% more
pixels), e1m1 with a shambler and an ogre in the hand torch and the shotgun held, over 3600 frames.

| | Before | Now |
|---|---|---|
| Alias pass, both eyes | 0.022 ms a frame | 0.023 ms a frame |
| Whole frame, GPU | 1.12 ms | 1.14 ms (within run-to-run noise) |
| Managed textures on e1m1 | 49.5 MB | 56.2 MB (20 more normal maps: the hands' and the body's skins) |

- The shaders do the same work as before: the hands and the body used to read a flat map.
- Loading: each made map takes about 7-10 ms for a 256² skin (`developer 1` prints each). On e1m1 that's 118 skins
  in 0.5 s. Of each 7-10 ms, the steps the round-14 maps also took (the dilation and the heights) are 1-3 ms; the edges and the forms are about 3 ms each.

**A model pack** (CREDITS.md, "Model packs evaluated"):

- **The candidates:**
  - Quake Reforged's Bestiary: monster skins with `_norm`, `_gloss` and `_luma`, in DarkPlaces' naming.
  - AMI / Authentic Models for Quake: faithful, 8-bit, no normal maps. Quake VR's monsters appear to come from it
    already.
  - QRP's item textures: weapons and items, no model normal maps.
- **Only Reforged ships normal maps.** The site's previews were thumbnails, so I downloaded its 2048 archive (208 MB,
  from the official page) to judge it properly, and tried it in the game on id's original models.
- **It isn't faithful:**
  - the grunt becomes a dark-green armoured man;
  - the knight is pink copper;
  - the shambler is red and brown instead of pale grey;
  - the zombie is grey-green;
  - its `_luma` layers are faint copies of the skins (25-45% of their brightness), so the monsters glow in the dark.

  See `pack_*_qbase.png`: left, the stock skins with the new maps; right, Reforged.
- **It's also incompatible:** it is painted for id's original UV layouts, and Quake VR's monster models (AMI's, many
  converted from the remaster) have different ones. For example, the grunt's skin is 256 x 256 here and 300 x 194 in
  id's model.
- **Decision:** no pack, and no menu toggle. The loading is ready for any pack that uses DarkPlaces' names.
- **To try one anyway:**
  1. Put its `progs/` files in a folder next to `quakevr`, for example `qrtest/progs/`.
  2. For Reforged, add id's own models from `id1/pak0.pak` to that folder.
  3. Start with `-game quakevr -game qrtest`. Ironwail stacks game folders, and the last one wins.
- **Licence:** "free with credit", "GPL", changes to be reported to the authors, and the terms "may change". That
  doesn't plainly allow redistribution, so nothing of the pack is committed.

**For baking our own models' normal maps** (the next task):

- **File names:**
  - Alias models: `quakevr/progs/<model>.mdl_<skin>_norm.png`, for example `vrflashlight.mdl_0_norm.png`,
    `vrgadget.mdl_0_norm.png`, `v_shot.mdl_0_norm.png`. One map for skin 0 serves every skin of the model, since they
    share the UVs; add a `_<skin>_norm` only where a skin differs.
  - MD5 meshes: `quakevr/progs/hand_rig_00_00_norm.png` covers all four hand skins, and
    `quakevr/progs/vrbody_00_00_norm.png` all 16 body skins. The lean and brawny bodies use the shader `vrbody` too, but
    have their own UVs: check them before sharing one map.
- **Format:**
  - Tangent space, OpenGL / Blender convention (green = +V, up the image).
  - RGB = normal × 0.5 + 0.5, linear 8-bit (not sRGB). Keep blue a real z: the sheen's anti-aliasing uses it.
  - Alpha (optional) = height: 255 is the surface, lower is deeper. Only model parallax (`vr_parallax_models`) uses it;
    without alpha the surface is flat.
  - PNG or TGA, any size. The map is stretched over the whole UV square like the skin, so use the skin's aspect ratio
    at 2-4× its resolution.
- **Tangent frame:** the engine has no vertex tangents. It builds a per-pixel cotangent frame from the screen
  derivatives of the position and the UVs, and handles mirrored islands. Bake in Blender's tangent space with the
  model's own UVs and smooth normals, as the game draws them. An MDL's back-half onseam vertices use their shifted UVs
  (`s + skinwidth/2`).
- **Strength:** 1 = as baked (`vr_normalmap_authored`). The map carries the real bevels, so bake them at their true
  depth. Authored maps aren't halved when held.
- **Margins:** leave 4-8 texels of margin (dilation) round each island. Nothing outside the islands is read, apart from
  filtering and mips.

### Not verified

- **In the headset:** whether the new relief reads as shape rather than noise at the headset's resolution and in
  motion. I judged grain only from 2048² stills.
- **Held weapons:** the half-strength rule for made maps is unchanged, and the dark weapon skins show little either
  way.
- **MD3 models:** they get no normal maps. Quake VR ships none.
- **The firing range:** its outdoor light overexposes the monsters both before and after (not this round's change),
  which hides most bumps there.
- **Muzzle flash:** the shots were taken on the first frames of the flash, not at its peak.

### In the headset

- [ ] Sweep the torch in your hand across a shambler and an ogre up close: do they show shape rather than speckle?
- [ ] With a rocket or a lava ball passing the knight's plates and the grunt's arms: do the forms move with the light?
- [ ] Your own hands in the torch light: knuckles and creases, and no grain?
- [ ] Graphics > Authored Model Bumps: nothing should change until an authored map is installed (then 0 flattens it).

## Enemies hurt by liquids; holster orientation

Two voice notes of 2026-09-28. Branch `agent/misc22`; composites in the scratchpad's `misc22/`.

### Enemies Hurt by Liquids (the e1m1 note)

"I've pushed an enemy into Slime and it wasn't taking damage." In id's QuakeC only the player is hurt by liquids
(`WaterMove`, client.qc); monsters never were. Ours had two partial exceptions. A shove's landing burned once
(`VR_Shove_Landed`: 100 in lava, 30 in slime), but it tested the monster's origin, the middle of its box, which is above
e1m1's shallow slime (40 units deep): so nothing. And hipnotic's gremlins die in lava.

**Now:** Gameplay > Damage > **Enemies Hurt by Liquids** (`vr_enemy_liquid_damage`). It is **on** by default for
three reasons: it is how the world should work, it makes the shove a tactic, and it changes nothing for monsters
that stay out of slime and lava. The only map-placed monsters that stand in them are the immune ones below.

A monster in slime or lava burns as you do, by how deep it is. Its waterlevel is measured as the engine measures
yours (feet, waist, eyes):

| Liquid | Damage | Per second at waterlevel 1 / 2 / 3 |
|---|---|---|
| Lava | 10 x waterlevel every 0.2 s | 50 / 100 / 150 |
| Slime | 4 x waterlevel every second | 4 / 8 / 12 |
| Water | none: no drowning (Quake's monsters never needed air; an ogre wading a deep pool should not die) | - |

| Monster | Rule |
|---|---|
| Fish, eels (`FL_SWIM`) | immune: their liquid is their home, whatever it is |
| Chthon, Shub-Niggurath, Armagon, the dragon, the lava lord (Hephaestus) | immune: bosses with scripted deaths; Chthon and Hephaestus live in lava |
| The firing range's dummy | immune (it reports hits, vr_dummy.qc) |
| Zombies | lava burns them up at once, which gibs them (nothing less than a hit their size kills a zombie); slime's small ticks are shrugged off like any small hit (`zombie_pain`) |
| Everyone else (shamblers too) | burns |

It is the liquid's damage, with no attacker, as the player's. So there is no quad, no Damage to Enemies multiplier,
no hit push, no gore spray and no hit feedback.
- The monster's own pain code runs: its pain sounds and flinches, at its own pace.
- Its own death code runs: past its gib health it bursts, as usual.
- Lava sizzles as the monster goes in (`player/inlava.wav`). While it burns it smokes and throws embers (the smoke
  and the lava nails' sparkles, at the surface).
- Slime hisses (`player/slimbrn2.wav`).
- Corpses are left alone, so no gibs burst out of a pool later.
- The shove's landing burn now tests the feet.

The code is in `QC/vr_liquids.qc`, scanned from `StartFrame`. Each monster is checked every 0.1 s, and one in slime
or lava at its liquid's pace.

**Kill credit:** the shove and knockback code (`VR_Push`, the `vr_shove` slide) doesn't track who pushed, and nothing
needs it:
- the kill count counts every monster's death, whatever killed it;
- monsters have no obituaries;
- the shove's own hit already made the monster go for you.

Passing the shover as the attacker would have brought quad and the Damage to Enemies multiplier into the liquid's
damage. It would also have pushed the monster out of the pool with every tick. So the burn has no attacker, as the
player's.

**Checked** in the mock. `vr_debug_shots 1` with `developer 1` logs each burn with the monster's health.
- e1m1's slime pool (`setpos 200 2820 -60`, a grunt put in it with `impulse 241`):
  - on: waterlevel 2, 8 a second: 30, 22, 14, 6, dead at 4 s;
  - off: 30 throughout (`vr_enemy_liquid_damage 0` logs "not burnt" once a second).
- e1m7's lava (`setpos -50 48 20`, facing the lava; monsters 200 units ahead):
  - a grunt (waterlevel 2): dead in 0.4 s;
  - a zombie: gibbed on the first tick;
  - an ogre: 200 -> 0 in 2 s;
  - a shambler (waterlevel 1: the lava is 40 deep): 600 -> 0 in 12 s, with pain animations and smoke rising over it
    (`liq_shambler.png`);
  - off: a grunt at 30 throughout.
- Not checked end to end: an actual shove into slime (the mock's hands can't shove on cue here). The shove only has to
  put the monster there; the burn doesn't care how it got in.

**In the headset:**
- [ ] Shove a grunt or a dog off e1m1's walkway into the slime: it should flinch and die in a few seconds. An ogre
  takes much longer: slime is slow, as it is for you. Push a knight into e1m7's lava: it should be gone at once,
  smoking. Does it make the shove worth it? If slime feels too weak, say how much stronger.
- [ ] Monsters walking through slime on their own now get hurt too (their AI doesn't avoid it): fine, or too easy?

### Holster orientation (the start map note)

"Sliders for each holster to change the orientation ... roll, yaw, and pitch, and everything for each holster pair."

**Where:** Hotspots, under each pair's Threshold: **Shoulder / Hip / Upper Pitch, Yaw, Roll**.
- Range -180..180 degrees, in 5-degree steps.
- Cvars: `vr_shoulder_holster_pitch/yaw/roll`, `vr_hip_holster_*`, `vr_upper_holster_*`; archived, 0 by default.
- The three pairs are all the holster slots there are: the hip holsters are the leg holsters (the Holster Slot
  sliders' model), the upper ones are on the chest, and the shoulder ones are on the back.

**What turns:** the holster model and the gun in it, together. They turn about the holster's position, which is where
your hand reaches for it. That point stays put, so holstering and drawing work exactly as before.

The turn is in the body's frame at the holster. The axes are "out" (off the body's surface), the body's up, and
"outwards" (the body's right for the right holster, its left for the left one):
- **Pitch** tips the holster's top away from the body (negative: towards it);
- **Yaw** turns its face outwards (negative: inwards);
- **Roll** tips its top outwards (negative: inwards). A hanging gun's muzzle swings the other way, a cross-draw cant.

The order is yaw, then pitch, then roll, as Quake's angles.

The left holster's frame is the right one's mirror image. So yaw and roll mirror between the two and pitch does not,
as the offsets' Y mirrors and their X and Z don't.

With the body drawn, the frame is the body's surface where the holster lies. That surface follows the lean, the
crouch and, for the hips, the thighs, and the turn rides along with it.

0 0 0 skips the turn entirely, so today's look is exact. `vr_savedefaults` keeps the new cvars (it writes every
archived `vr_` cvar that differs from its default).

**Insertion direction:** there is no insertion direction to follow. A gun goes into a holster when you let go of it
within the holster's reach (a sphere, Threshold), whichever way it points, and a drawn gun is in your hand at once.
So turning the holster changes only how the holstered gun is carried and shown.

The only code change is in `vr_view.cpp` (`turnHolster`, `holsterTurn`). `vr_body.cpp` (holster positions) is
untouched.

**Checked** in the mock, with `r_fullbright 1` and `vr_body_mode 3`:
- `hol_hip_mirror.png`: the body preview facing you (`vr_body_debug 2`), then from its left (`3`). The hips at 0,
  pitch 30, yaw 45, roll 45 and roll -45 turn as mirror images. (The right hip holds the shotgun and the left the axe:
  the starting holsters.)
- `hol_upper.png`: the empty chest holsters' models at 0, pitch 30, yaw 45 and roll 45, mirrored.
- `hol_shoulder.png`: a shotgun holstered at the right shoulder, seen from behind
  (`vr_mock_camera 0.7 1.9 0.8 20 40`). At 0, pitch 30 (its top goes back), yaw 45, and roll 45 (its top goes
  outwards).
- `hol_eye.png`, the eyes looking down, with the hips at pitch 20, yaw 30, roll 40:
  - the guns come into view;
  - gripping at the right hip draws the shotgun;
  - letting go there holsters it again, turned.

**In the headset:**
- [ ] Hotspots: turn a hip holster (try Roll first: a cant) and look down. The gun should follow the holster, and the
  left one should mirror the right. Draw and holster: nothing about reaching should change.
- [ ] Try the chest and shoulder pairs the same way. If a slider feels backwards, say which one and which way it
  should go.

## Dummy attacks (firing range)

Your request: a button in the firing range that makes the training dummy strike at you every few seconds, to
practise parrying, without disturbing the melee recordings.

### What it does

- **The button** is beside the dummy, about 4 m to its south (on its right as it stands, on your left as you face it
  from the usual spot). It is a panel like the second row's, facing north, with the label **DUMMY ATTACKS OFF / ON**.
  Press it like the other buttons (a hand, a weapon or the body). While attacks are on, the button stays lit
  (its pressed texture), and the gadget shows "Dummy attacks ON" / "OFF". The console says what the settings are.
  `vr_dummy_attacks 1` / `0` in the console does the same; the label and the light follow.
- **In attack mode**, the dummy strikes the nearest living player within reach, once every 2.5 s, give or take up
  to 0.4 s at random:
  - **The tell** (the wind-up, 0.6 s): a new sound (`vr/dummy_windup.wav`: two clacks of the rifle taken up, then a
    rising, growling swell that breaks off just before the blow), a glow around the dummy (Quake's dim light), and
    the rifle raised as it turns to face you and rears back 12°.
  - **The blow**: it lunges (22° forward) with a sword's whoosh. The blow is dealt as a monster's melee blow, the
    same call a knight's attack makes (`T_Damage` from the monster itself). So the parry (a weapon across, or crossed
    arms), parry stamina, the counter's window, the knockback of an unparried blow, armour, god mode and death all
    work exactly as in a fight. It does 10 damage (the `impulse 242` test blow's damage; a knight's does 0-9).
    For this, the parry's list of monsters that strike in melee (`VR_Parry_MeleeMonster`) includes the dummy only
    while its own blow is being dealt.
  - **A parried blow** throws the dummy back (a lean), on top of the parry's own sparks, sound and haptics.
  - **Reach**: 80 units (2.4 m at your world scale), from its origin to yours, measured as a knight's reach is (60
    units). Further away it waits and turns back to its own facing. When you step in, the wind-up starts 0.5 s later
    at the soonest. If you step out during the wind-up, it misses (the whoosh, no damage). An unparried blow's
    knockback can push you out of reach; step back in.
  - **Your hits** are reported as before (the console and the floating numbers). They don't interrupt its attack,
    so a steady rhythm is kept. A counter's readout shows as before ("counter bash ... (x1.50)").
  - Players with `notarget` are left alone, as monsters leave them.
- **The button's state is not saved.** Attacks are off at every map load, a saved game's included (the engine turns
  `vr_dummy_attacks` off as the server spawns). A wind-up under way when the game was saved stops after loading it.

### Settings (Gameplay > Parry, Bash and Headbutt > Training Dummy Attacks)

| Setting | Default | Menu |
|---|---|---|
| `vr_dummy_attacks` | 0 | (the button; not saved, off at every map load) |
| `vr_dummy_attack_period` | 2.5 s | Time Between Blows (1-8 s, extends) |
| `vr_dummy_attack_jitter` | 0.4 s | Randomness (0-2 s, extends): each blow up to this much sooner or later |
| `vr_dummy_attack_windup` | 0.6 s | Wind-Up (0.1-2 s) |
| `vr_dummy_attack_reach` | 80 units | Reach (16-150 units; 150 is where a parry and a monster blow's push stop) |
| `vr_dummy_attack_damage` | 10 | Damage (0-50, extends) |

**Tracing** (`developer 1`): "dummy: winds up, the blow in 0.60 s", "dummy: strikes player for 10, 42 units away:
parried", "dummy: misses: nobody within reach", along with the parry's own lines ("parry: vr_dummy with hand 1",
"stamina: 70 of 100 left", "counter: open for 1.50 s").

### The motion recordings

- **Off, it isn't there.** With attacks off, none of this code runs: the dummy's think is the same as before, it
  draws no random numbers, and its angles are the same values. The eval shows this below.
- **The button's entities are appended** at the end of `vrfiringrange.ent`, so no entity before them is renumbered.
  The dummy is still entity 137, at the same place, and the player start is unchanged. That is what the takes'
  `target: vr_dummy #137` and their placement rely on. The panel's nearest point is 104 units from the dummy's middle, and
  71 units from anything any of the 471 takes did: where you stood, your head, your hands and the tips of your
  weapons. I checked every row of every take.
- **Replays turn it off.** `vr_motion_play` and `vr_motion_eval` set `vr_dummy_attacks` to 0 for the replay and put
  it back afterwards. The eval's map loads turn it off too. So the dummy never strikes on its own timer during a
  replay.
- **Takes recorded with attacks on.** Arming or recording is never affected by the button unless you turn attacks on.
  A take recorded with them on at any moment gets the header line `dummy attacks: on`. It also gets a `strike` event
  for each wind-up, blow (its damage, and whether it was parried) and miss. Its replay reproduces them: the take's
  dummy does each `strike` event in the server frame that plays that event's frame, after the player's, as it was
  recorded (the engine calls QC `VR_Dummy_Replay`). So the parries, the stamina, the counter's window and the
  knockback happen at the same moments as live.
  - Why this matters: a counter bash recorded against a real blow is a `parrybash`. Replayed without the blows, it
    would be a plain `bash` at a different damage (checked: 7.2 with the blows, 4.8 without).
- **The eval's expectations** stay about your own events. `strike` events are listed in the eval's `events` column
  (as are parries), but no expectation counts them: they are not hits, strokes, pushes or batting, so a `no_hit` take
  recorded with attacks on still passes if you didn't hit. Best keep them off for every category but Parry Bash and
  Other. The Motion Recorder section of MOTIONS.md says so.

### Files

- `QC/vr_dummy.qc`: "Attacks (parry practice)" (the wind-up, the blow, the replay's strikes) and "The firing range's
  'dummy attacks' button" (`vr_dummy_attack_toggle`: the label, the light, the toggle).
- `QC/combat.qc`: `VR_Parry_MeleeMonster` takes the dummy while it strikes; `vr_parried` tells the dummy how its blow
  was met.
- `quakevr/maps/vrfiringrange.ent`: three entities at the end (the panel, the button, the toggle with its label).
- `quakevr/maps/vr_panel_north.bsp`, `vr_button_north.bsp`: the map's soldier button and panel turned to face north,
  built by `Misc/quakevr/make_spawn_buttons.py`. They use Valve 220 texture axes turned with the brush, so their
  textures lie exactly as on the map's own button. The two west-facing models are unchanged.
- `quakevr/sound/vr/dummy_windup.wav`: `Misc/quakevr/make_sounds.py` (`dummy_windup`).
- Engine: `vr_cvars.inc` (the settings), `vr_progs.cpp` (off at every server spawn; the `VR_Dummy_Replay` binding),
  `vr_motion.cpp` (the header line), `vr_motion_play.cpp` (off during replays; the take's strikes done again),
  `vr_menu.cpp` (the section).

### Tests (mock headset; scratchpad `dummyatk/`, scripts `gen2.py`)

- **The button** (`button.png`): OFF, pressed by the hand ("Dummy attacks ON" on the gadget, `vr_dummy_attacks` 1),
  pressed again (OFF, 0). The label and the light follow; the light stays on while the button comes back out.
- **The wind-up and a blow landing**, from your eyes (`seqfp.png`) and from the side (`seq2.png`): standing, then
  the glow and the rifle raised, then the blow: the red flash, 100 to 90, pushed back 26 units (240 to 265.7).
- **A parried blow** (`parry2.png`; the sword across in one hand, parry stamina on): "parry: vr_dummy with hand 1",
  "stamina: 70 of 100 left", "counter: open for 1.50 s", the bar and the embers, "strikes player for 10: parried".
  Then a bash 0.24 s after the second parry: "counter: bash on vr_dummy, x1.50", "Dummy: 7.2 damage - counter bash
  with the Knight's Sword, one hand (x1.50), at the body".
- **Reach** (jitter 0): 110 units away, nothing for 4 s. Stepping in, the wind-up starts 0.5 s later and the blow
  lands. Stepping out during the next wind-up: "misses: nobody within reach". Turned off during a wind-up: no blow.
  A map load turns it off (`vr_dummy_attacks` 0), and so does loading a game saved during a wind-up (no blow; the
  label reads OFF, `load2.png`).
- **A take recorded with attacks on** (`take_attacks.csv`: two parried blows and a counter bash) has
  `# dummy attacks: on` and its `strike` events. Played back (`vr_motion_play ... recorded`) with attacks on
  beforehand: the same events at the same times (strike/windup, parry, strike/blow, push, **parrybash 7.2**, ...),
  hands within 0.014 units of the take's, "4 of its strike events done again". `vr_dummy_attacks` is 0 during the
  replay and 1 again after it. `vr_motion_eval` on it: the same, "1 of 1 replays hit as their takes did live". The
  same take with its header line removed replays a plain `bash 4.8`.
- **Your takes** (the 471 archived in `motions/pre_calibration_2026-09-28`, copied), `vr_motion_eval` before (the
  round's commit, 0d5c225b) and after (this change), with your old hand settings set and
  printed (`vr_gunangle` 39.5, `vr_gunyaw` 4, `vr_offhandpitch` 40.25, `vr_offhandyaw` -4, every `vr_handcal_*` 0,
  `vr_handcal_off_mirror` 0; printed again during a replay and after it, all the same: they are also the takes' own
  `hand angles`, which playback applies). Before and after: 429 of 471 pass, 181 replays hit as live, and the
  two tables are **identical in every column** (verdicts, reasons, events, recorded events, same hits, frames and
  hands' error). Two earlier runs, without setting those values first (the same values from the config), were
  identical too except `hand_error_u` in 4-5 takes. That column differs as much between two runs of the unchanged
  build (4 takes), so it is run-to-run noise already there. The 5 takes rerun alone on the unchanged build gave the
  new values.
- **The menu** (`menu.png`): the Training Dummy Attacks section under Counter-Attacks.
- QC: 0 warnings.

### Not verified

- In the headset: how readable the tell is (the glow and the raised rifle at 1.5 m, the sound's level against the
  range's), and whether 0.6 s of wind-up and 2.5 s between blows feel right for practice. All are settings.
- Coop: it strikes the nearest player; not tried with two.

### In the headset

- [ ] Firing range: press DUMMY ATTACKS (south of the dummy). The label reads ON and the button stays lit. Step in
      front of the dummy: it turns to you. Within a second or so you hear the clacks and the swell, it glows and
      raises its rifle, then lunges.
- [ ] Take a blow without parrying: 10 damage, pushed back. Then hold a sword across: the parry's ring and sparks,
      and it is thrown back.
- [ ] Parry Stamina on: the bar drops with each parry; a fourth one-handed parry in a row knocks the sword away.
- [ ] Parry, then bash or slash it within 1.5 s: the counter's readout on the dummy.
- [ ] Step back beyond about 2.4 m: it stops and turns back. Step out during a wind-up: it misses.
- [ ] Press the button again: OFF. Reload the map with it on: OFF.
## Flashlight shadows, cord and hand-over

Your voice notes (e1m1):
- the beam went through the gun in your hand, your hands and your body (props already blocked it);
- passing the torch between hands reset it every time;
- the cord was drawn on top of everything ("maybe like the old telephone cord with the spiral").

### Your hands, guns and body in the torch's shadow

- **Why props cast and these didn't.** The shadow pass takes its model casters from the entities drawn this frame.
  Props are world entities. Your hands, guns, body, gadget, holstered weapons and the torch are VR view entities, and
  dynamic lights skipped every view entity. Only map lights took them, by `vr_shadow_self`.
- **Now** every dynamic light that isn't your own takes them too, by the same `vr_shadow_self`: the flashlight,
  rockets, explosions, `vr_light_test`.
  - 0: none; 1: the body; 2 (the default): the body, hands, guns and the rest.
  - Your own lights (muzzle flashes, the quad and pentagram glows) still don't. They are keyed to you, and the light
    sits inside your gun or body.
- **Culling.** View entities are placed by the VR transforms (weapon offsets, posed arms), outside their model bounds.
  They are tested against each light and each face with a wider sphere: twice the model's radius plus 60 cm. The
  renderer's box cull isn't used for them, so no finger or barrel is dropped from the narrow torch tile.
- **The torch itself** casts too, for other lights. It can't shadow its own beam:
  - the light starts at the lens, and the torch lies behind it, beyond the projection's near plane (3.8 cm);
  - in both grips the fingers wrap the tube behind the lens;
  - on a gun, the barrel is 4.7-10 cm off the beam's axis and ends 1 cm past the lens. The spot on the wall is whole
    (the composite's last row).
- **First person.**
  - The body model's head and neck are collapsed (IK.md), so the head casts nothing. With the torch held behind your
    head, the shoulder's and the gun's shadows fall ahead, with no head.
  - With Body: Off, only the hands, guns and gadget are drawn, so only they cast. The body settings are followed as
    they are.
- **Bias** at 5-50 cm: unchanged, and enough.
  - The torch's tile is 1024 texels over ±24 degrees: 0.2 mm a texel at 20 cm.
  - The normal offset (a texel, doubled at grazing angles) and the 0.2% depth scale are as before.
  - The close-ups show no acne on the gun, hand or arm, and no gap at the shadows' roots.

Composite `flashfx/shadows_before_after.png` (scratchpad; before on the left):
- the torch at the other hand;
- at the held shotgun, from behind and close;
- first person, with the torch behind the head;
- at the arm and body (the pauldron's and the arm's shadow on the wall behind);
- clipped on the gun.

### Passing it between hands

Weapons pass at the hand switch spot: let go of one with the hands together and it goes into the other, gripping
hand. The torch now passes the same ways:
- The other hand grips the torch while the first hand holds it: it takes it, and the first hand's later release does
  nothing.
- You let go while the other hand is gripping the torch, or with the hands together (the hand switch spot, 19 cm): the
  torch passes to it.
- The other hand catches it within 0.4 s of a release: that is a hand-over too, not a take from the belt.

In every case:
- It stays switched as it was.
- It takes the grip (low or overhead) whose beam is nearer where it shines now, as when taken off the head or a gun.
- It eases from where it was onto the new grip over 0.12 s, the fitted hands' own blend. It doesn't go back to the
  belt.
- If the game already had the receiving hand's grip, that grip is released.
- It doesn't pass while that hand holds anything (a prop too) or is at a holster.

Logged in the mock (`flashfx/handover_after.log`, composite `flashfx/handover_before_after.png`). Before, none of
these passed: the torch stayed in the first hand or flew to the belt.

    flashlight: on
    TEST 2 main grips at the torch
    flashlight: passed to the main hand (on)
    flashlight: low grip (from the other hand, nearer its beam)
    TEST 4 main lets go with the off hand gripping at it
    flashlight: passed to the off hand (on)
    flashlight: low grip (from the other hand, nearer its beam)
    TEST 5 off lets go, main catches
    flashlight: passed to the main hand (on)

### The cord: coiled, springy, depth-tested

- **Drawn in the scene** (`VR_DrawSceneOpaque`), depth-tested and written. It is hidden behind the gun, your hands,
  the axe on your hip and the world, as it should be. Before, it was a line in the debug overlay, drawn over
  everything.
- **A telephone cord** (`vr_coil.cpp`):
  - 64 turns of 3.8 mm wire, 1.3 cm across, 24 cm long relaxed;
  - the coil keeps its wire's length: stretched, its turns open out and it narrows;
  - over its last 1.5 cm at each end it narrows to the bare wire, into the clip and the tail cap.
- **Springy.** The line is 12 springs: 30 g, 1.5 N/m in all, three times stiffer squeezed, with gravity and damping.
  - Stretched to the hand, it pulls nearly straight with a few centimetres of sag.
  - Near the belt, it bows and droops.
  - A quick move of the hand swings it, and it settles in about half a second.
  - Your body's own movement (walking, turning, lifts) carries it along; a teleport restarts it.
  - It leaves the clip downwards, where the torch hung, and goes into the tail cap along the torch.
- **Lit** as the models are:
  - the world's light, sampled at three points along it;
  - the dynamic lights, their cones included, so the torch pointed at it lights it;
  - a rubbery sheen, for each eye.
- **Detail:**
  - within 0.6 m of the eyes: 8 segments a turn, 6 sides;
  - within 1.2 m: 6 and 5;
  - beyond: 4 and 4.
- **Setting:** Cord on the Flashlight page is now Off / Coiled / Plain (`vr_flashlight_cord` 0/1/2). Plain is the
  same springy line, as a 4 mm cable.
- **How it's drawn.** The first version built the whole mesh on the CPU. That took 0.15 ms of CPU a frame, and
  uploading its 480 KB cost the GPU 0.07-0.09 ms. Now:
  - the CPU makes only the rings (their middles, axes and light: 50 KB);
  - `gfx::drawTube` builds the wire round them in the vertex shader.

Composites (scratchpad):
- `flashfx/cord_before_after.png`: first person with the gun over the cord, close up, behind the axe handle;
- `flashfx/cfa_zoom.png`: the coil close up;
- `flashfx/sw2zoom.png`: the swing, frame by frame.

### Cost

Exclusive runs, mock headset, the e1m1 start, the torch on and pointed at the held shotgun, the cord out:

| | CPU ms | GPU ms |
|---|---|---|
| dynamic light shadows, with / without your casters (`vr_shadow_self` 2 / 0) | 0.011 / 0.003 | 0.008 / 0.001 |
| the cord: its rings and their upload | 0.037-0.041 | – |
| the cord's draw, each eye | 0.000 | 0.001 |
| world+brush, each eye, cord on / off | – | 0.050 / 0.047 (noise) |

- Between intervals of the same run, the frame's total CPU time moves by ±0.15 ms.
- That comes from the scene's own CPU time, which changes with `vr_shadow_self` through the map lights'
  self-shadows. Those were already there: 0.234 ms before this work, in the same scene.
- No new option was needed. `vr_shadow_self` 0 turns off all your shadows, and Cord: Off turns off the cord.

### Mock recipes

- **The torch at the held shotgun,** in front of the wall behind the start:
  - with `vr_weapon_grip_mode 1`: `vr_mock_look 25 180; impulse 154`;
  - `vr_mock_hand main -0.1 1.35 0.4 0 180 0; vr_mock_hand off 0.25 1.45 0.0 -10 160 0`;
  - for the other hand instead: `vr_mock_hand main -0.1 1.42 0.4 0 180 90; vr_mock_hand off 0.1 1.45 0.0 -5 170 0`;
  - a spectator's view: `vr_mock_camera 0.25 1.9 -0.5 20 180`.
- **Hand-over:**
  - take the torch in the off hand (TESTING.md);
  - `vr_mock_fingers main 0 0`;
  - put the main hand at the torch: `vr_mock_hand main -0.06 1.3 -0.42 0 0 0`, with the off hand at `-0.12 1.3 -0.4`;
  - wait, then `vr_mock_button main grip 1`.

### Not verified

- Only seen in the mock: not at the headset's resolution, and how springy the cord feels as you move is untested.
- There is no clean image of the cord behind the world itself. The mock hands don't reach through walls, and the
  spectator camera stays in the room. The cord uses the same depth test that hides it behind the gun and the axe.
- With Body: Full, the belt torch shining down past your thigh wasn't looked at.

### Check in the headset

- [ ] The torch at your other hand, at the gun in it, at your forearm: do the shadows on the wall behind have the right
      shapes, with no speckle or gap where they start?
- [ ] The torch behind or beside your head: the shoulders' shadow ahead, and none of the head?
- [ ] The torch clipped on each gun: is the spot whole, not cut by the barrel?
- [ ] Rockets and explosions near you: your body's shadow from them. Right, or too much?
- [ ] Hand-over: grab the lit torch with the other hand; let go with the other hand closed on it; toss it across. Does
      it stay on, with no jump?
- [ ] The cord stretched, slack, swinging, behind the gun and your hand: springy enough? Too thick or too thin? Try
      Plain too.
## Baked normal maps

The author, after the model bump maps: give our own models real shape detail baked from geometry, not guesses from
paint. "Never compromise on quality."

**What ships** (`quakevr/progs`, 30 maps, 5.5 MB of PNG; tangent space, green up the image, linear 8-bit RGB, blue a
real z, no alpha):

| Model | Map | Size | What it carries |
|---|---|---|---|
| The jointed hand (all 4 skins) | `hand_rig_00_00_norm.png` | 1024² (2× the skin) | knuckles over the joints, the extensor tendons and two veins on the back, the nails (plate, fold), the pads of the palm, the fingers and the thumb's ball, knuckle bulges; the creases, wrinkles and nail folds painted into the skin as grooves |
| The body, all three builds, skins 00-03 | `vrbody_00_00_norm.png` | 1024² (4×) | muscles of the bare arms (deltoid, biceps, triceps, the elbow, the forearm); the quilted vest (padded channels, stitched rows), laced front, side seams, yoke, rolled collar; the belt with its buckle, loops and stitched edges; the trousers' stitched seams and folds; the ridged, riveted thigh plates; the boots' straps, buckles, turned-down tops, welts, laces; the bracers' rims, stitches and buckled straps; the face's nose, brows, eye sockets |
| The body, skins 04-15 (the armours) | `vrbody_04_00_norm.png` | 1024² | the same, the torso as the five overlapping lames of the armour, rolled edges, rivets, the front closure, the side straps |
| Flashlight, wrist gadget and strap, shell, holster, pauldrons | `<model>.mdl_0_norm.png` | 4× | rounded edges, faceted tubes made round; the flashlight's knurled tube, finned head, ribbed cap, crowned grip rings, stippled rubber; the gadget's parting line; the strap's webbing and stitched edges; the shell's ribs and primer; the holster's stitching and leather; the pauldrons' quilting and rivets |
| The 20 view models `v_*.mdl` | `<model>.mdl_0_norm.png` | 2× (4× under 256 wide) | rounded edges, faceted tubes made round, the painted panel lines grooved, painted rivets raised, wood grained along its painted streaks (the axe: its handle; its head's blood is no wood) |

- The lean and brawny bodies draw their skins with the same texture coordinates as the athletic one (checked on
  every bake), so one map serves the three builds; `vr_body_build` and runtime segment scaling don't need their own.
- The engine names: an alias model's skins read `progs/<model>.mdl_0_norm` (skin 0's serves all); a body skin without
  its own map now takes the nearest earlier skin's (`Mod_MD5SharedNormalMap`), so skin 04's serves the 12 armoured
  skins.

**How it bakes** (`Misc/quakevr/bake_normals.py`; the code is in the models' add-on so its button runs the same:
`quakevr_models/normalmaps.py`, `normalbake.py`, and the recipes `normaldetail.py`, `normalbody.py`,
`normaltiles.py`; numpy only, about 110 s for all 30 maps):

- **In the engine's own frame.** `normalbake.Raster` rasterises every triangle into the map's texels (2× supersampled,
  then box-filtered) and builds, per texel, the frame the shader builds from the screen derivatives: t along which u
  grows, b along which v grows, both perpendicular to the interpolated normal (the file's: anorms for an .mdl, the
  engine's own welded area-weighted normals for an MD5), mirrored islands included. The wanted normal is encoded in
  that frame exactly (0.01° on decoding), whatever the UVs' shear or stretch.
- **Shapes from the mesh:** each face's normal as the facets stand for it (averaged with the faces round each corner
  within 35°: a 12-sided tube is round, a box stays a box), and each hard crease (the faces' normals split) rounded
  over about 1.6-1.8 skin texels, turning half way to the other face (at most 32°) at the edge.
- **The hand's forms from its rig:** the joints are the rings of vertices at them (where the author's edit put them),
  each finger's axis and dorsal side from them; knuckles, tendons, veins, pads, nails are heights in space round those
  (the same on both sides of a UV seam, so no seam shows), bent into the normal by their surface gradient. The skin's
  painted creases (dark lines at least a few texels long, stronger the longer) become grooves: the relief follows his
  paint, the mesh and the skin are never changed.
- **The body from its generator:** `make_vrbody.py` paints each block texel by texel (`vest_texel`, `armor_texel`,
  `arm_texel`...); `normalbody.py` raises the same places (the same `around(bu, ...)` bands, rows and dashes) in Quake
  units through each texel's own surface scale.
- **The generated models' tiles:** every face of one of their materials maps onto that material's tile, so a tile's
  relief is on every face drawn from it; `normaltiles.py` raises what each generator paints on its tiles (the
  flashlight's knurling where `paint()` cuts it, its fins, ribs...), in texels, the same slope on every face. Their
  dithered paint is left out (speckle is no shape).
- **The view models' paint:** dark seams become V grooves, small bright spots rivets, wood its grain (the paint's
  streaks drawn out along their own direction, clean of the dither).
- **Shared texels:** a texel drawn by several triangles (a weapon's two sides, the body's left and right limbs, a tile's
  faces) takes each triangle's normal; the owners are the triangles drawing it at the least magnification (within 2×:
  a cap's fan or a screw's side squeezed onto a whole tile has no say); where the owners disagree by more than 6°
  their mean fades to flat by 14°.
- **Margins:** 8 texels round every island (mean of the covered neighbours, ring by ring).

**Engine** (small):

- `BumpedNormalK` (models only) normalises its frame's two axes each, as bakers do (Blender's MikkTSpace): scaled
  together (the longer one unit), a slope along a 512 × 178 skin's long side came out a third as steep. The world's
  `BumpedNormal` is unchanged. The made maps of non-square skins now read as made (in texels, both ways alike).
- `Mod_MD5SharedNormalMap`: a body skin without a map takes the nearest earlier skin's; the skin's own name is passed
  (it was its `_luma`'s for 32-bit skins).
- `TexMgr_ShareNormalMap`: an authored file serves every skin of its model with one texture, and isn't even read
  again (the hand's 4 skins, the body's 16, a model's skins).
- `developer 1` prints each authored map's load time beside its name.

**In Blender** (MODELS_IN_BLENDER.md and HANDS_IN_BLENDER.md, "Normal maps"):

- Import shows the map on the model (a Normal Map node into the material's Normal).
- After an edit it's one step: Export bakes the map again from the file it just wrote (on by default), or **Bake
  Normal Map** in the Quake VR panels. Then `vr_model_reload` / `vr_hand_reload` (both re-read the map: checked by
  swapping the file under a running game).
- **A high poly of his own:** select it, then the model, Bake Normal Map: Cycles bakes its normals in the model's
  object space, a layer at a time where the model's triangles share texels, and they are written in the engine's frame;
  the recipe's relief is laid on it unless Add Details is off.
- **The guard:** the maps are registered in `generated.json` (`genguard.py`). A map painted or baked from a high poly
  since is left alone by the script (`--keep-edited`, `--force`) and by the add-ons (Overwrite Edited Map). The script
  bakes everything before writing anything, so a failure leaves the maps as they were.

**Verification** (composites in the scratchpad, `bakenorm/composites/`; the shots in `bakenorm/shots/`):

- **Frame and handedness:**
  - `frame_domes_hand.png`: domes in texel space (tests the engine's decoding) and in 3D through the baker's frame
    (tests the encoding), lit from the left: convex everywhere, continuous across the fingers' and palm's seams.
  - `frame_domes_mirrored_shotgun.png`: the same on the shotgun, whose two sides are mirrored halves of its skin: both
    sides convex, lit the same way. `frame_domes_mirrored_body.png`: the arms, which share their texels.
  - Against Blender's own baker (Cycles, the low poly subdivided as a high poly, tangent and object space; the
    engine's reading of Blender's tangent map against Blender's object-space truth, texels drawn by one triangle):

    | | Engine before (axes scaled together) | Engine now (each unit) | This baker |
    |---|---|---|---|
    | Hand (512² skin): median / 95% / 99% error | 0.72° / 4.9° / 12.7° | 0.39° / 3.2° / 9.7° | 0.00° / 0.01° / 0.02° |
    | Nailgun (512 × 178): median / 95% | 3.37° / 39.7° | 1.83° / 36.1° | 0.00° / 0.01° |
    | Rocket launcher (512 × 194): median / 95% | 3.31° / 15.8° | 1.49° / 14.5° | 0.00° / 0.01° |

    Blender's tangent maps now read about right on smooth, square-textured meshes; on the hard-edged, faceted view
    models MikkTSpace's smoothed tangents differ from the per-pixel frame by tens of degrees at the creases, which is
    why the add-on bakes a high poly in object space and encodes it here.
- **Load-time maps against baked** (mock eyes 1536², e1m1; left load-time, right baked; `*_qbase.png`, and
  `-Base qrp` in `*_qrp.png`):
  - `hands_*`: both hands, the left's back and the right's palm, then the other way round; each in the map's light,
    the head torch (head-on) and the head torch seen from the side.
  - `body_*`: the body preview from the front in the map's light and the head torch, from its side, from behind with a
    light behind it.
  - `gadget_*`: the gadget on the wrist from above (map light, torch) and from the side. `flash_*`: the flashlight in
    the fist, a light from the side, from above.
  - `w154_*` shotgun, `w156_*` nailgun, `w160_*` rocket launcher, `w152_*` axe: in the eye view (map light, head
    torch), from the side (head torch, map light, a light from the side), and the muzzle flash.
  - Seen: the hands' knuckle wrinkles, tendons and finger creases read cleanly where the load-time maps give blotches
    of the painted highlights; the vest's quilting, the thigh plates' ridges, the straps and buckles read as shapes;
    the flashlight's head and the view models' barrels and bands are round instead of faceted, their edges catch the
    light, their painted panels are grooved; head-on light shows little relief (as it must: 1 − cos θ).
- **Seams:** `seams_moving_light.png`: the hands under a light moving past in three places: no line at the fingers'
  and the palm's seams (the heights are in space, the frame is the engine's).
- **Grain:** `grain_3_frames.png` (the shotgun from the side, moving) and `grain_flashlight_3_frames.png` (the
  flashlight's finned head and ribbed cap, turning a degree a frame): the relief holds still, no sparkle; the frame
  differences are the motion's (1.1-1.8 levels, as the load-time maps').
- **Round trip in Blender** (headless, both add-ons, a copy of the files): import shows the maps (a model, the hand,
  the body's armour skin); export re-bakes the same bytes as the script for an unchanged model; after a mesh edit the
  export bakes a different map, and the script bakes the same one from the edited file; the hand's Bake Normal Map
  gives the script's bytes; a high poly (the shotgun subdivided) bakes in 13 s.
- **Guard:** painting over `v_axe.mdl_0_norm.png` then baking: refused, nothing written; `--keep-edited` baked the
  others and kept the painted one; `--force` baked it again.

**Cost** (e1m1, mock, exclusive runs):

| | Load-time maps | Baked maps |
|---|---|---|
| Managed textures | 487, 218 normal maps, 56.2 MB | 461, 192 normal maps, 100.8 MB |
| Normal maps' load time | 115 made, 494 ms | 60 made (monsters, items) 220 ms + 29 authored files 102 ms = 322 ms |
| Per frame | | two more `inversesqrt` per model pixel with a normal map |

- The 29 authored maps are 56 MB (RGBA8 with mips, as every model normal map is uploaded, the z for the specular
  anti-aliasing): the hand's 1024² 5.3 MB, the body's two 10.7 MB, the generated models' 4.1 MB, 19 view models
  36.1 MB (every weapon is precached, the mission packs' too). They replace 55 made maps. RG8 would halve it, at the
  cost of the sheen's anti-aliasing from the maps.

### Not verified

- **In the headset:** whether the relief reads at arm's length at 2064 × 2208, in motion; the grain check was 3
  frames of slow motion in the mock.
- **The world's frame** (`BumpedNormal`) still scales its axes together: a non-square world texture's slopes along
  its long side are weaker than across it. Not changed here (the world's look would change).
- **A high poly in the game:** the add-on's Cycles path was tested headless (it bakes, it differs from the recipe's),
  not looked at in the game.
- **The view models' hands:** the remaster-style skins paint a gloved hand over half of each skin; where those faces
  are drawn, they get only their bevels and painted lines.
- **Shared texels:** where a weapon's two copies of a texel disagree (one next to a crease, the other not), the bevel
  fades out there (23-38% of the bevelled texels on the nailgun and shotgun).

### In the headset

- [ ] Your hands in the torch and in a muzzle flash: knuckles, tendons, nails and creases, no blotches, no seam lines
      where the fingers meet the palm?
- [ ] Look down at your body (and Body Preview): quilting, belt, straps, the thigh plates; with armour, the lames?
- [ ] The flashlight in your fist and the gadget on your wrist, close: round head and tube, knurling, fins?
- [ ] The shotgun, nailgun, rocket launcher and axe turned in the torch: rounded edges, panel lines, no sparkle?
- [ ] Graphics > Authored Model Bumps: 0 flattens them, 2 doubles them: is 1 right?
## Melee fixes: flashlight, axe on walls, gibs

Voice notes 2026-09-28: a punch with the flashlight in the fist doesn't register; a two-handed shove with the torch in
one hand should count as long as the other hand is an empty palm facing the enemy; punches register on walls but axe
blows don't (your theories: the axe pushed back by the wall stops the blow from registering, or the blow is detected
elsewhere in the swing); gibs on the floor can't be struck, while a gib held in the other hand can. Four bug fixes in
`QC/vr_melee.qc` (and `vr_carry.qc`, `client.qc`); no rule was retuned for the general classification, and your
recorded takes replay exactly as before (below).

### Why

- **The punch with the torch.** The torch takes the grip's press (`vr_flashlight.cpp`, `button`): the game never sees
  the grip, so the server saw an empty hand with its grip open. A punch needs a closed fist (the grip held), so a
  punch holding the torch never landed, in either grip (low or overhead: the grip turns the torch in the fist, not
  the hand). The same hand counted as an *open palm*: pushed palm first, the torch hand alone was a palm shove.
- **The shove with the torch.** The torch fist has no palm facing ahead, so a push of both hands was the free palm's
  alone: a one-handed shove (0.6 of the damage, 0.7 of the knockback). With the torch hand's palm turned ahead, the
  torch hand counted as a second palm (the bug above). In the mock the free palm's shove did register (one-handed);
  if in the headset it doesn't register at all, the free palm isn't passing the shove's own tests (facing ahead within
  53 degrees, moving the way it faces, the arm extending): tell me.
- **Weapons on walls.** Both your theories, checked in the mock:
  - *Pushed back: yes, this is the main cause.* `vr_handpose.cpp` stops the hand at walls, and pushes it back so its
    weapon's muzzle (the axe's head, a sword's tip, a gun's muzzle) stays a unit off the wall. The melee sweeps each
    point from the last pose to this one against walls with exact lines, so the head never reached the wall: an axe
    chopped at a wall 0.853 m ahead stopped its head at 0.834-0.854 m, and no contact was found. A fist's "knuckles"
    point is 4 units ahead of the hand's point, so when the hand is stopped they are 3 units inside the wall. That
    is why punches register.
  - *Detected elsewhere: no, but a second rule threw the contact away.* No other point (the handle's end, the hand)
    registered first: the handle's points strike nothing. But a point going down more steeply than 20 degrees
    (`VR_MELEE_WALL_DOWN`, for a hand reaching down to a holster) never struck a wall, and a chop meets the wall going
    down (the axe's head at -0.7 to -0.9, sine, in the diagonal and overhead chops). So a diagonal or overhead chop
    whose head did cross the wall was ignored too.
  - Weapons had world impact detection all along: the same sweep, and `W_FireAxe`'s "hit wall" thunk. The two causes
    above defeated it. Swords, Mjolnir and gun swings had the same problem; butt and pommel strikes didn't (the near end
    isn't held off walls).
- **Gibs on the floor.** A loose gib or head is `SOLID_NOT_BUT_TOUCHABLE`: it can be picked up but never blocks. The
  melee's sweep strikes monsters' boxes (`SOLID_SLIDEBOX`, `SOLID_BBOX`) and what its lines hit, and lines pass
  touchables (only shots trace with `MOVE_HITGIBS`), so a blow went straight through. The only way to hit one was the
  carry code's touch (`VR_Carry_Nudge` -> `VR_Gib_Struck`): the fist's palm and curled fingers on the gib's surface in the frame
  of the touch, at `vr_melee_speed` (a weapon's poke, `vr_wpntouch`, traces past touchables too). An axe's head or a sword's blade
  never reached it that way. A held gib has its own code (`VR_Gib_HeldFrameHand` sweeps the other hand's fist and
  weapon through it every frame), which is why it works. Box3D isn't involved: the gib's solidity is the QC's, and
  both physics engines failed the same way.

### What changed

- **The flashlight's hand is a closed fist** (`VR_Melee_HoldsTorch`, `VR_Melee_ClosedFist`: the engine's
  `QVR_VRBITS0_*HAND_BUSY`, "the hand holds the flashlight"). It punches under the same rules and with the same damage
  as a gripped fist. It is never an open palm, so it doesn't shove alone.
- **A two-handed shove with the torch** (`VR_Bash_TorchPushing`): an open palm's shove while the torch hand pushes
  along is a two-handed shove. "Pushes along" means the torch fist is going forward at the second hand's share of the
  push speed, as a second palm must, and extending the arm as a shove's palm does (`VR_Bash_Extends`, factored out of
  the palm's test).
- **Walls** (`VR_Melee_Sweep`, `VR_Melee_WallTaken`):
  - A striking point (the blade, a head, a muzzle, the fist) strikes a wall within its thickness past its sweep's end.
    That thickness is the 3 units (x `vr_melee_range_multiplier`) that already widen monsters' boxes.
  - The downward rule now keeps out only floors (a surface facing up, normal z 0.7 or more) and walls a point merely
    grazes going down (less than 0.25, cosine, into the wall: 75 degrees off head-on). A chop down into a wall
    strikes it.
- **Loose gibs and heads** are swept as boxes, like monsters (not a gib held in your own hand: that one is still
  `VR_Gib_HeldFrame`'s). A blow on one bursts it the way the held-gib strike does (`VR_Gib_Blow`:
  `VR_Gib_StrikeDamage` at the blow's speed, 20 x speed/`vr_melee_speed`, at most 60). It doesn't deal the weapon's
  blow damage, which is 8-12 and under a gib's 12 health.
- The developer "melee event" line says "a gib" for these. `developer 2` prints the wall rule's decision for points
  going down.

### Tests (mock headset, your calibration: `vr_gunangle 70`, `vr_handcal_*`, and your weapon offsets)

The motions are `Misc/quakevr/motion_synth.py`'s new presets, written for your config's hand settings
(`--settings-from`). The recipes are in TESTING.md, "Melee fixes". Before is vr-cleanup `b981ef7e`'s QC; after is this
branch.

| Test | Before | After |
|---|---|---|
| Off-hand punch at the dummy, gripped fist (the control) | punch 5.6 m/s | the same |
| The same punch holding the torch, low / overhead grip | nothing / nothing | punch (the same speed and strength as gripped) |
| Free palm + torch fist pushed at the dummy (both grips) | one-handed shove (4.8 damage) | two-handed shove (8) |
| The torch hand alone pushed palm first (both grips) | one-handed palm shove | nothing |
| Both palms / one palm, no torch (controls) | two-handed / one-handed | the same |
| Axe, Mjolnir, sword, shotgun: horizontal, diagonal and overhead chops into a wall, from 22 and 16 units | 10 of 24 hit the wall | 23 of 24 |
| The same chops in the open (no wall) | nothing | nothing |
| A gripped fist punched into the wall (22 / 16 units) | hits / nothing | the same |
| A gib on the floor punched down, and chopped with the axe (Box3D, and the Quake VR solver) | nothing (4 of 4) | bursts (4 of 4) |

- **The chops that still miss.** One of 24: the shotgun's horizontal swing from 16 units. Its muzzle meets the wall
  while the hand is still speeding up (4.8 m/s, under the swing's 5), then slides along it. That is the rules'
  verdict, the same before.
- **Rebounds.** A wall contact now lands at the hand's speed as it arrived (5.2-13.8 m/s in these tests), because it
  is caught within the points' thickness, before the wall stops the hand. A few swings register a second wall hit
  on the rebound, as before: the wall pushes the hand back, and that motion can pass as a new blow.

### Your recorded takes (the regression check)

The 471 archived takes (`motions/pre_calibration_2026-09-28/`) were replayed with `vr_motion_eval` before and after,
on the same build config. Both runs used your hand settings from before the calibration, set explicitly and printed in
the log: `vr_gunangle 39.5`, `vr_gunyaw 4`, `vr_offhandpitch 40.25`, `vr_offhandyaw -4`, every `vr_handcal_*` at 0 and
`vr_handcal_off_mirror 0` (playback also resets `vr_handcal_*` for takes that don't list them).

Result: 429 of 471 pass, both before and after, with **no difference**: every verdict, event, reason and hand error
is identical. The takes hold no flashlight, gib or wall contact.

### The calibrated hand (checked)

- **The points follow the calibration.** The punch's points ("the fist" at the hand's point, "the knuckles" 4 units
  ahead) and the palm (the hand's right side) are in the hand's frame, which the calibration moves and turns. With
  your calibration, the melee's hand point sat (+2.5, +/-0.6, -4.0) cm from the controller's pose: the drawn hand's
  offset (`vr_dumpview`). The palm faces the same way as the drawn palm: the drawn empty hand is turned from the
  hand's angles only by the fist slot's pitch (-7 degrees), about that same axis.
- **Found, not changed: the knuckles point is ahead of the drawn fist.** `vr_dumpview` puts the drawn fist 0.9 to
  16 cm *behind* the hand's point, and 3 to 12 cm below it. So "the fist" point is about 1 cm ahead of the fist's
  front and 3 cm above its top, and "the knuckles" point is 12-13 cm ahead of the real knuckles, plus the 3-unit
  (9 cm) thickness. A punch can register before the drawn knuckles reach the target. Moving the points onto the drawn
  fist would change every punch, and the punch rules were fitted on your takes with these points. It is left for when
  there are new takes to refit on.

### Not verified

- In the headset: all of it. The torch tests ran through `vr_mock_play` on the mock's real clock (their speeds vary a
  little run to run). They can't be synthetic takes: playback's first press sends the torch home.
- Only the off hand was tested holding the torch. The main hand's code path is the same one (a bit per hand).
- Only straight walls in the firing range were tested. Also untested: brush entities (doors, lifts), and breakables
  struck within the margin (they take the blow as before, now also from 3 units off).

### In the headset

- [ ] Punch the dummy with the torch in your fist, in both grips (B/Y flips it): does it land like a gripped punch?
- [ ] Shove with the free palm while the torch hand pushes too: "shove with both hands"? The torch hand alone, palm
      first: nothing?
- [ ] Chop a wall with the axe sideways, diagonally and from overhead: the thunk and buzz each time? Also Mjolnir, a
      sword and a gun swung into a wall.
- [ ] Reach down to a hip holster beside a wall, and holster a gun near a wall: still no wall hit?
- [ ] Punch and chop a gib lying on the floor: it bursts. A gib held in the other hand: as before.

## Dynamic wounds, burns and wetness

Your note: the body has sixteen skins (four armours times four wound levels); most games paint blood onto the
textures where a hit lands; could we, for the player and for enemies, and reuse it for burns and wet clothes? Yes:
every monster, corpse and player drawn with an alias model now gets its wounds painted into its own skin's layout
where each blow lands, and burns, char and wetness the same way. The cost is a few microseconds per hit and next to
nothing per frame; memory 16 MB.

### What you see

- **Wounds where the blow landed**, on the model's own texel grid, sharp-edged: a star of blood with a dark middle
  (6 to 9 spikes of their own widths and lengths, a ragged edge), a few drops spattered round it, and often a run
  dripping down the surface from it. Sizes by the blow: a pellet or bullet small, a nail larger, a blade or fist a
  slash drawn out along the swing (2 to 2.6 times as long as wide). Each shotgun pellet lands on its own.
- **Colours from Quake's palette**, as the monsters' own painted blood (id's skins use 47..87 red): palette 69 in the
  middle, 71 round it, 74 at the tips, the runs and the drops, with a little of the skin's shading through them.
  Blood shines a little (a tight highlight in the model's light) and fills the normal map's bumps.
- **Burns**: an explosion scorches the side of a model that faced it, blotchy (holes of skin left between patches of
  darkened, browned skin and, for strong blasts, near-black char), fully near the blast and fading a hand's width
  on; lightning leaves a small char mark, a lava ball or a lava nail or a laser bolt a burn with a small wound. Fresh
  burns glow: a few texels in the cracks of the deepest char flicker in Quake's fullbright oranges, cooling in 4 s.
- **Lava** chars everything under its surface in patches, with embers; **slime** eats in (lighter patches of char)
  and wets; **water** wets everything under its surface: darker and glossy (a broad sheen), the bumps half filled.
  Wetness dries in about 25 s from the waterline down (the deeper, the longer: the mask's value grows with the
  depth under the surface), with the shading's dither breaking the dry edge up; while wet, drops of water (greenish
  from slime) fall from under the waterline (monsters: from their own triangles; you: from your hands).
- **Blood and char stay** on monsters and on corpses. A monster's head flying off (the monster's own entity becomes
  `progs/h_*.mdl`) gets a bloody neck and face when the monster had bled. Separate gibs are not painted: their skins
  are raw meat already.
- **You**: your body and your jointed hands take your wounds where the blow came from, instead of the wound skins;
  the armour skins stay (your repaint, 24fa2de6, untouched; the wound skins are kept for Dynamic Wounds off). As your
  health comes back, blood and char fade (health + h from health h0: (h / (100 - h0)) of them, all of it at 100, over
  about a second); a respawn takes all of it off. Wetness dries as on monsters.
- **A new map starts clean**, and so does a **loaded game** (the masks are the client's only, never saved).

### How it works (`Quake/vr/vr_wounds.cpp`, `Quake/r_alias.c`, `Quake/gl_shaders.h`, `QC/vr_wounds.qc`)

- **The masks**: one texture array, `vr_wounds_pool` layers (64) of 256 x 256 RGBA8 (16 MB): red blood, green char,
  blue wetness, alpha heat. A model's mask takes the part of its layer of its skin's shape (the skin's own size when
  smaller than 256, else its longer side 256: the mod's soldier 256 x 256, the ogre 256 x 128, a head 128 x 128). Given on a model's first wound; when all are taken, the one drawn longest ago goes (and,
  between equals, the farthest), never the player's own three. A model with several meshes (several skins over one
  layout; none of Quake's) gets none.
- **Painting**: the model is drawn as it is this frame (its lerped pose, its place; the body's and hands' posed
  bones), into its layer, laid out by its skin's coordinates (the alias vertex shader with `WOUNDPAINT`: position =
  the texture coordinates), with the world position and normal for each texel; the paint shader
  (`wound_paint_fragment_shader`) evaluates up to 16 splats a draw (star wounds, burns, blasts, liquids) at that
  point and the mask keeps the most of it and what was there (blending by max). Distances are in the world, so a
  wound goes across the skin's seams and islands as it goes across the model (Quake's front/back halves, the
  onseam copies the loader already made). The triangles are then drawn again as lines (`glPolygonMode`): a texel an
  island's edge crosses without covering its middle is painted too, so the shading's reads at the edges find paint.
  A triangle folded to nothing (the posed body's hidden parts share texels) is skipped.
- **Where a blow lands**: the server's point is on the monster's box, not its model. The client finds the model's
  surface: the triangles as drawn (the weapons-against-models test's, `modelcollide::drawnTriangles`), the first
  front face the blow's line goes into near the point, else the facing triangle nearest the line; the wound is
  centred there, round the surface's normal. Your body and hands are skinned on the GPU (no triangles on the CPU):
  capsules round them as posed (the torso and the legs round the pelvis, each forearm elbow to wrist, each hand) take
  the blow, the first one along its line, and the same wound is painted on the body and both hands (a hit on a wrist
  marks the bracer and the hand). The server's point on you is the box's middle (your pelvis): hits are spread over
  the body, more on the chest.
- **Shading** (`WoundsAt` in the alias fragment shader): the mask read at the skin's texel (or the mask's, on a skin
  finer than twice it: QRP's), values shown through a 4 x 4 ordered dither over their edges: chunky marks on the
  skin's own grid, fading by dither as they dry or heal. With no mask (the instance's `Wound.x` 0) the shader's
  colour is the skin's, as before.
- **Hit events**: a new `svc_quakevr` sub-command, `QVR_SVC_WOUND` (14: entity, point, direction, kind, amount,
  extra; 17 bytes), written by a new QC builtin `woundevent` to `sv.datagram` (unreliable, as particles: a lost one
  misses a mark). Sent by `VR_Wound_Hit` (`combat.qc`'s `T_DamageImpl`, beside the gore's hit: every hit that takes
  health from a monster or a player; the kind from `vr_hitkind`, the inflictor and `IsExplosionDamage`), by
  `VR_Wound_Pellet` (`TraceAttack`: each pellet), and four times a second by `VR_Wounds_Frame` (`StartFrame`) for
  everyone standing in a liquid (the surface's height, `liquidentry`; monsters measured as `vr_liquids.qc` does).
  Lava and slime char only when they hurt (their damage's event), so a monster in lava with Enemies Hurt by Liquids
  off is neither burnt nor wet (lava wets nothing). No `random()` in the QC: the game plays the same with or without it.
- **Demos and protocol**: recorded like any message; a demo recorded now needs this engine to play (the sub-command
  is new, as round 21's other ones). Other mods' progs never send it. No protocol number change.
- **Over time**: every 0.1 s (the game's time, so paused games wait) a subtract pass on each mask that is drying (1/255
  a step), cooling or healing; nothing otherwise.

### Settings (Gore page, "Wounds on Models")

| Setting | Cvar | Default |
|---|---|---|
| Dynamic Wounds (blood; your body's and hands' instead of the wound skins) | `vr_wounds` | 1 |
| Burns (explosions, fire, lightning, lava, slime; embers) | `vr_wounds_burns` | 1 |
| Wet from Liquids (and drips) | `vr_wounds_wet` | 1 |
| Models Kept (32 / 64 / 128: 8 / 16 / 32 MB) | `vr_wounds_pool` | 64 |

Turning one off takes what it painted off every model; all three off frees the texture. Commands:
`vr_wounds_test <entity | self | ahead | all> <kind> [amount] [right] [up] [extra]` (a wound as the server would
send it: 1 shot, 2 nail, 3 melee, 4 blast, 5 burn, 6 zap, 7 lava, 8 slime, 9 liquid with `up` the surface over the
feet), `vr_wounds_info`, `vr_wounds_dump` (every mask to `quakevr/wounds/*.png`), `vr_wounds_debug 1` (each event
and where it landed; 2 also the player's capsules).

### Decisions (and where the design changed)

- **Alpha is heat, not dirt**: Quake has nothing that dirties a model, and fresh embers need to know which texels
  burnt recently. Slime is a lighter, patchier char plus wetness rather than a green tint (no fifth channel).
- **Healing also fades your char** (the design: burns persist). A burn that never healed on the player looked wrong
  after a health pack; on monsters and corpses burns do persist.
- **The masks are 256 x 256 layers in the skin's shape**, not the skin's own resolution: one texture array (one
  binding for every instanced draw), and Quake's skins are about that size; a QRP skin reads the mask at its texels
  (still Quake-sized marks).
- **The player is mapped through capsules**, not by painting along the blow's line (the first version): along a
  line, a forearm pointing at the attacker took the blood along its whole length.
- **Options on the Gore page** (with the rest of the blood), not Graphics.

### Costs (`run.sh --exclusive`, RTX 4090, mock eyes 2048², firing range, 12 monsters spawned ahead)

| | GPU | CPU |
|---|---|---|
| Alias models per eye, 32 masked models (every model hit) vs Dynamic Wounds off | 0.073 vs 0.067 ms | same |
| The wounds' frame work with nothing to paint (the scope `wounds`) | 0.001-0.003 ms | 0.001-0.003 ms |
| Painting: 32 models hit every frame | 0.051 ms a frame (1.6 µs a paint) | 0.119 ms (3.7 µs a paint) |
| A hit's frame (one to a few models) | under 0.01 ms | under 1 ms (never over the 1 ms log threshold) |
| Memory | 16 MB (64 masks; 8 or 32) | |

The one slow frame seen: 32 masks made in one frame (the stress test's first volley), 5 ms of CPU; a single blow's
first mask costs nothing measurable.

### Checked (mock headset; composites in the scratchpad's `wounds/final/`)

1. A grunt hit by seven blows (shots, nails, a slash, off-centre): marks where each landed, the chest's run down the
   belly; its mask on the skin shows marks going across the islands' edges (`1_grunt_before_after_mask.png`).
2. An ogre: an explosion 40 units in front of it (scorch on the facing side, blotchy), lightning and a burn, then 5 s
   later with the embers out (`2_ogre_blast_zap_burn_cooled.png`).
3. A grunt in lava to the knees: charred patches with embers, cooling; a gibbed grunt's head mask with its bloody neck
   (`3_lava_then_cooled_headgib_mask.png`).
4. A grunt wet to the thighs, then 15, 30 and 45 s later: dry from the waterline down (`4_grunt_wet_drying.png`).
5. Your body from ahead (health 30): clean, ten hits, then wet to the waist, then `give h 100` (the blood gone in about
   a second), then dry (`5_body_hits_wet_heal_dry.png`); first person: hits and a burn on the hands
   (`6_hands_firstperson.png`); a QRP grunt (`7_qrp_grunt.png`).
6. **Real game paths**: a grunt's shotgun at you (each pellet an event on your body and hands), an ogre's chainsaw
   and grenade at you (37 melee events, one blast), a melee blow into a grunt, a grunt standing in e1m1's slime (wet events four times a second, slime burns once a second with Enemies
   Hurt by Liquids on), a gibbing.
7. **Off is unchanged**: e1m1 (a corpse, your body at health 45 with its wound skin, your hands), fixed frame time,
   against the build before (0a44b4d5): with the three options off the eye images match but for the wrist gadget's
   live readout and 27 pixels 1/255 apart (`8_offcheck_base_off_diffx40_on.png`: before, off, the difference x40, on;
   on, the body shows no wound skin at health 45: the wounds painted replace it).

### Not verified

- A real rocket or grenade on a monster (on you it is checked: an ogre's grenade; the path is the same
  `T_RadiusDamage`; `vr_physics_blast` from the console loses its datagram: console commands run between server
  frames).
- Other players' `player.mdl` in multiplayer (painted as monsters are; not run).
- The drips' look (spawned; not caught in a screenshot).
- The headset: the look at 2064 x 2208 and in motion, the hands' wound size (0.65 of a body's), the blood's tone on
  your dark vest.

### In the headset

- [ ] Shoot a grunt a few times, then an ogre with the rocket launcher: marks where you hit, across seams, Quake-like?
- [ ] Let grunts shoot you, look at your hands, arms and chest; then take a health pack: the blood fades?
- [ ] Wade into water to the waist, come out, look down: wet, dripping, drying in half a minute?
- [ ] Shove a monster into lava or slime (Enemies Hurt by Liquids on): char with embers?
- [ ] Gore page: Dynamic Wounds off brings the wound skins back.

## Climbing with both hands

Your notes (e1m1): only the main hand seemed to climb, never both hands at once; you want to climb a ladder-like
structure hand over hand (up, down, left, right), and to hang from a ledge and shimmy along it one hand at a time.
Both work now: either empty hand takes hold on its own, both can hold together, and the hand-off between them
never moves you. Locomotion > **Climbing (Experimental)** (`vr_climb`, still off by default; your config has it on).

### Why it felt one-handed

The old code let a second hand take hold only at a ledge at most 16 units (about half a metre) above the one held,
by design ("shimmy, not climb"). Reaching up with the other hand to go higher was refused, so it looked as if the
second hand (usually the off hand) couldn't climb. Also, a hand at a shoulder or upper holster could never take hold
there, even with the holster empty. Nothing else differed between the hands: in the mock, both hands take hold the
same way from the same places.

### What changed (`Quake/vr/vr_climb.cpp`)

- **Either hand, both hands.** Each hand takes and lets go of its own hold. There's no limit on where the second hand
  holds: a rung above, one to the side, the same ledge further along.
- **The pull.** While you hang, the body moves by what the holding hands moved relative to it since the last frame,
  the other way (pull down and you rise). With two hands, each hand counts by how far it moved. So the hand doing
  the pulling carries you, a hand held still isn't dragged back against it, and two hands pulling alike move you by
  their average. The old code aimed the body at each hand's grab point, averaged. With the hands pulling unevenly,
  letting go of one then moved the body to the other hand's point in one frame.
- **No pop at the hand-off.** A hand taking hold or letting go changes only which hands pull, never where you are.
  The limits (feet no higher than the highest hold; within 48 units of each hold, horizontally) now only stop you
  from going further past them. So letting go of the upper hand can't drop you back under the lower hand's limit.
- **Holds on the edge's line.** A hold is where your hand is along the edge, on the top, 2 units in from the lip.
  The edge is found square to the face under it (a quarter-unit search, then the face's normal), not along whichever
  of eight directions found the drop first. The **drawn hand stays on its hold** (its palm's middle) while it holds,
  whatever the tracked hand does: it eases on in 0.06 s, glides to a new hold, and eases back 0.12 s after letting
  go. The server sends the holds as stats (`STAT_QVR_CLIMB*`); it's drawing only (the aim and the moves use the
  tracked hand). The arm follows the drawn hand.
- **Rungs and trims.** On the way from a hold to the drop, a step down is passed over (a rung above another, a
  ledge with a trim under its lip). The room needed above a hold is 8 units (was 16), so rungs 20 units apart are
  holds. Stairs are still not holds (their treads never give the 32-unit drop within 16 units). A hand against
  the face just under a lip (up to 12 units under it) takes the lip.
- **Shimmying.** Hang from a ledge with one or both hands. Let go with the leading hand, reach along, take hold,
  push both hands back the other way, bring the trailing hand over, and repeat. While one hand holds, you hang (no
  gravity). Your body slides along the face under the ledge (the box traces as before).
- **Mantle.** As before: pulled down 8 units and the head 8 above the ledge. Near a ledge's end, where the box
  would stick out past it, the spot on top may now be up to 16 units along the edge. The e1m1 ledge below needed
  it.
- **Holding things.** A hand with a weapon, a carried object, a locked force grab, or (new) the flashlight can't
  take hold. A holster that holds a weapon still wins over a hold while you stand (the grip draws it). An empty
  holster, or any holster while you hang, gives way to the hold.
- Kept as before: the grab delay after letting go of everything (0.4 s), Lowest Ledge, Ledge Fling (the release
  flings you with the hands' motion, capped at 200 u/s, 150 up), moving holds (a hold on a moving brush now carries
  you with it through the pull, as the aim point did), the QC's climbing bits (no melee from a holding hand).
- **No option for the old behaviour.** Its one-hand limit and its aim-point motion were what you asked to lose, and
  the new motion is the same as the old for one hand. `vr_climb_debug 2` prints a `climbtrace` line every frame (the
  body, the pull, what the body did, each hand and its hold). `vr_climb_probe [yaw]` lists holds ahead.

### The test map: `vrclimb`

`quakevr/maps/vrclimb.map` (.bsp, .lit, .lux; generated by `Misc/quakevr/climb/make_vrclimb_map.py`, compiled the
MAPPING.md "Full" way; ours, like vrexample). It has:

- a rung wall: 12 rungs 8 deep, 4 thick and 192 wide, 20 units apart, up a 280-unit tower you can mantle onto;
- a long ledge (48 high) running from the floor out over a 256-unit trench, with stairs out of the trench.

### Verified (mock headset, scripted `vr_mock_play` motions, per-frame `climbtrace`)

| Test | What happened |
|---|---|
| Off hand alone, then hand over hand up the rung wall (11 alternations; each new hand pulls while the other still holds, then that one lets go), then over the top | body from 24 to 305: 11 rungs (56 to 256), then the top edge and the mantle onto the tower |
| Two-hand hang on the ledge: the off hand pulls twice as far as the main; the main lets go mid-pull and takes hold again while the off hand pushes up | hangs throughout; the body follows the weighted pull |
| Shimmy 7 hand-overs left (out over the trench), 1 right | y 176 to 231, then back to 222 along the ledge, hanging the whole time |
| Let go of both over the trench | falls to the trench floor (-208) |
| Both hands on the long ledge, pull | mantled onto it |
| e1m1 (the ledge at x 274, y 2330..2350, 64 high, near the nailgun and rocket launcher): both hands, 2 hand-overs to the right, pull | mantled onto it (the spot on top slid along the edge, near its end) |

**No pop.** The hand-offs are 22 on the rungs, 36 on the ledge and 12 in e1m1. In each, the body's move in the
frame a hand took hold or let go was what the hands still holding pulled it by: at most 0.001 units (0.04 mm) off,
0.07 units (2.7 mm) at most in all. The pull is recomputed from the logged tracked hands, independently of the
engine's own figure, and matches the body's move within 0.4 mm on every hanging frame (the log's rounding). The only
frames where the body did less than the pull are those where it pressed against the face (2.8 mm at most a frame,
against the wall). **Before**, the same ledge play: letting go of a hand while the hands pulled unevenly moved the
body 1.36 units (52 mm) in one frame, then 39, 19, 15, 12 mm at the next ones.

Composite (`climb_composite.png` in the scratchpad): hands on the rungs from a spectator camera (top), both hands on
the ledge from the side and from the eyes, and one hand on the ledge over the trench (bottom).

### Check in the headset

- Climb a rung wall or a run of ledges hand over hand. The body should follow the pulling hand 1:1 and never jump
  when the other hand lets go or takes hold.
- Shimmy along a ledge both ways, and check that you don't fall while one hand holds.
- The drawn hands should sit on the edge while they hold. Look at how the arm stretches when your real hand has
  moved off the hold.
- A shoulder holster with a weapon, reached past while standing at a ledge: tell me if the grip should take the
  ledge or the weapon.
## Body calibration

Your notes: the wrist bent steeply where yours doesn't, and after tuning, the drawn elbow still locked straight before
yours did; a longer arm helped the elbow but gave odd hands; maybe only the forearm should be longer. You asked for a
calibration that has you take poses and measures each part. Branch `agent/bodycal`; scripts, logs and composites are
in the scratchpad's `bodycal/` (`final/` for the results).

### Why the elbow locked early, and what Arm Stretch did

- **One length for both bones.** The arm was the model's upper arm (29 cm) and forearm (26 cm) times your body scale
  (eye height / 1.646: 0.944 for you), times Arm Length: 27.4 + 24.5 = **51.9 cm** from the shoulder joint to the drawn
  wrist. Arm Length scaled both bones alike.
- **The elbow is straight whenever the wrist is that far from the shoulder.** Past it, Arm Stretch (1.2) grew both bones
  up to 1.2 times (62 cm), still straight; then the shoulder reached 8 cm towards the hand.
- **Your takes reach further than 51.9 cm.** Your drawn wrists (the takes replayed with their own hand settings) were
  beyond it in over 10% of the frames; each direction's 99th percentile is 55-65 cm. So a real arm still bent 10-20
  degrees was drawn straight and stretched. A longer Arm Length fixed the reach but lengthened the upper arm too. That
  moved the elbow, and with it the forearm's direction and the wrist's bend, away from yours.
- **The shoulders** sat where the model put them, never measured: 23 cm below the eyes, 7.9 cm behind them (with your
  Torso Offset and Shoulders Back), 38 cm apart between the joints.

### What changed

- **Separate lengths:** `vr_body_upper_arm` and `vr_body_forearm`, in real centimetres.
  - They go from the shoulder joint to the elbow, and from the elbow to the drawn hand's wrist. 0 is the old behaviour.
  - Each drawn bone stretches to its own length, so the drawn elbow is where the measured one is.
  - Arms and Pauldrons shows them as sliders.
- **Calibrated arms** (both set):
  - Past their reach, the shoulder reaches first (Shoulder Reach, 8 cm), and only then does the arm stretch.
  - **Arm Stretch after calibration** is a last resort, for hands past your measured reach plus the shoulder's
    (tracking glitches, a lunge). It no longer hides a short arm.
  - The shoulders rise continuously with the arm's lift, from the hand 0.6 of the arm below the shoulder to straight
    up, as a real shoulder does. Before, they rose only once the hand was above the shoulder. The calibration measures
    how far (Shoulders Up, Shoulders Forward).
- The wrist gadget and the blood's elbow use the solved forearm's own length.
- **Uncalibrated arms are exactly as before.**

### Body Calibration (VR menu > Body > Body Calibration, or Arms and Pauldrons)

- **Position:** Standing or Seated. Seated, the height is left alone; the arms and shoulders are measured the same.
- **Start Calibration** closes the menu. In front of you are the step's text, and a mirrored stick figure 1.7 m ahead
  showing the pose (moving, for the moves). After three beeps and a high one:
  1. **Stand Tall** (arms straight down): your eye height, and your shoulders' height over the straight arms.
  2. **T-Pose:** the reach to the sides.
  3. **Arms Forward** (shoulders relaxed): the reach forward, and how far the shoulders come forward.
  4. **Arms Up:** the reach up, and how far the shoulders rise.
  - These four are captured with a click when you hold still for half a second: the head within 1.5 cm, each wrist
    within 2 cm.
  - A pose that isn't the step's (an arm not out to the side, say) isn't taken. After 6 s the text says what's off.
  5. **Arm Circles** (8 s): big slow circles with straight arms. Your wrists on the sphere round your shoulders.
  6. **Elbows Still** (8 s): elbows at your sides, forearms waved up, down, in and out. Your wrists on the sphere round
     each elbow give the forearm's length and where the elbow is, and so the upper arm's.
  7. **Wrists** (6 s): forearms still, wrists bent every way. The point your hand turns about (your real wrist) is
     checked against the drawn hand's wrist: a check of Hand Calibration.
  - A move that didn't cover enough records up to 6 s longer ("bigger circles").
  - The menu button stops; Continue takes the rest. About two minutes in all.
- **The fit** takes all the samples at once and uses the body's own arm model: the same function places the shoulders
  in the game.
  - It is least squares (Levenberg-Marquardt), then Tukey's biweight four times, dropping the moves' outliers.
  - It measures where the shoulders sit on the chest (Shoulders Back, Up, Width), the reach and its split into upper
    arm and forearm, and the shoulders' rise and swing.
  - The chest is the body standing upright under your head, whatever the head does, so seated or standing give the
    same numbers.
- **The page with the result:**
  - Each value, now and new: eye height; upper arm, forearm, reach; shoulders below the eyes, behind them, apart; rise
    and swing.
  - Also each arm's reach, how far the drawn wrists are from your real ones, and the fit's error.
  - Each pose has a row with its error in cm (good, fair, or REDO). Clicking it takes that pose again and fits again.
  - **Showing: New Measurements / Current Settings** switches between the two while the page is open. It changes your
    body and the one in front of you (Body in Front: facing you, or from the side). Bend and straighten your arms to
    compare.
  - **Apply** sets them (and the height, standing). **Undo** puts back the exact values from before; they are kept in
    the config (`vr_bodycal_undo`). **Cancel** changes nothing.
  - **A fit whose poses disagree** (more than 2.5 cm rms, or a move to redo) isn't offered to Apply.
- **Each session is saved** to `quakevr/bodycal/<date>.txt`: every frame, the settings, the result.
  `vr_bodycal_refit <file>` fits it again: send me one if the result looks wrong.

### Tests (mock headset; the scratchpad's `bodycal/`)

**Synthetic people through the whole calibration.**
- **The people:** each is a skeleton with known shoulders, upper arm and forearm.
- **The runs:** the skeleton's poses become head and controller poses on the calibration's timeline: a take, played
  with `vr_motion_play ... watch` while the calibration runs.
  - The controllers are placed so that the mock's hand calibration draws the wrists at the skeleton's.
  - The take goes through the real capture, stillness check, fit and page.
- **Noise:** 2 mm on the wrists, the head swaying, the chest's estimate off by 3 mm.
- **Imperfect poses:** each arm up to 8 degrees off the pose, the elbow bent up to 8 degrees, uneven circles, the
  elbows drifting in the Elbows step.
- **Shoulders that move:** either as the game's model does (exact), or with a real shoulder rhythm unlike it. That one
  rises with the arm's lift (up to 5 cm) and comes forward (up to 3.5 cm).

| Person (truth: upper arm, forearm) | Shoulders | Upper arm | Forearm | Reach | Shoulder joint | Wrist check | Poses |
|---|---|---|---|---|---|---|---|
| Your size (30.5, 24.5) | game model | +0.0 | -0.1 | -0.1 | 0.6 cm | 0.0 cm | all good |
| Short forearm (33, 21.5) | game model | -0.1 | +0.1 | 0.0 | 0.2 cm | 0.0 | all good |
| Long forearm (30, 29) | game model | +0.5 | -0.5 | 0.0 | 0.1 cm | 0.1 | all good |
| Your size | real rhythm | +0.9 | +0.1 | +1.0 | 1.5 cm | 0.0 | all good |
| Short forearm | real rhythm | +1.5 | -0.3 | +1.1 | 2.0 cm | 0.0 | all good |
| Long forearm | real rhythm | +1.3 | -0.2 | +1.1 | 1.7 cm | 0.0 | all good |
| Your size, seated | real rhythm | +1.5 | -0.6 | +0.9 | 2.2 cm | 0.0 | all good |
| Your size, hand calibration off 1.5 cm | real rhythm | +1.1 | -0.3 | +0.8 | 1.4 cm | **1.5 cm** | all good |

- **Shoulders that move as the model does:** every length within 0.5 cm, the shoulders within 0.6 cm.
- **A real rhythm, unlike the model:**
  - The forearm is still within 0.6 cm.
  - The upper arm comes out 1-1.5 cm long, and the shoulder about 1 cm low and 1 cm narrow.
  - The game's shoulder can't move as that one does, so the fit trades one against the other to match the reach in
    every pose. It is the arm the game can draw best, not a flaw of the capture.
  - None of the variants I tried (a yaw per pose, heavier weights, fewer poses) got the resting shoulder under 1.2 cm.
- **The wrist check** found the 1.5 cm hand calibration error exactly, and 0.0-0.1 cm otherwise.
- **Seated:** the same numbers within 1 cm, the height untouched.

**Natural arms, drawn by the game** (`natmock.py`, the your-size person, real rhythm).
- **The poses:** seven natural ones (relaxed, guard, aim, punch, reaching up, hand at the chest, across).
- **The setup:** the controllers are where those poses put its hands, the drawn hand in line with its forearm.
- **The comparison:** the game's arm (`vr_debug_arm 1`) against the true one.

| | Elbow off | Elbow bend off | Drawn forearm off the real one (the wrist's false bend) |
|---|---|---|---|
| Your settings (Torso Offset 0.07, Shoulders Back -0.04, default arms) | 4.9 cm (max 8.4) | 12.3 deg | 11.0 deg (max 19.7) |
| Defaults | 3.8 cm (max 7.8) | 6.6 deg | 8.7 deg (max 18.4) |
| **Calibrated** | **2.5 cm** (max 7.1) | **2.4 deg** | **5.9 deg** (max 16.7) |

- **The elbow straightening** (the arm forward, bent 0 to 60 degrees):
  - Your settings drew it straight up to a 30 degree bend: an aim bent 25 degrees was drawn straight.
  - Calibrated, it is drawn straight only below about 20 degrees.
  - With shoulders that move as the model does, the drawn bend follows the real one within 2 degrees all the way
    (`nat_gamecal.txt`).
- **The wrist:** its false bend is the drawn forearm's error, so the steep wrist of your first note was mostly this.
  - What is left (the guard's 16.7 degrees) is the elbow's direction: the pole (Elbow Out, Back, From Hand). No length
    changes that.
  - A fist at the shoulder or the face is still drawn with the elbow 20-30 cm off (31 before): the pole again.

**Your takes** (46, two per category, 36,384 arm frames).
- **Replayed with their own settings,** as recorded before you calibrated your hands: Gun Angle 39.5, Gun Yaw 4, Off
  Hand 40.25 / -4, no hand calibration, and your body settings of the time. Your new hand calibration isn't used.
- **They don't give your bone lengths**, only how far your wrists went. So the calibration here is a plausible one:
  - a reach of **54.6 cm**: the median, over directions, of each direction's 99th percentile of the right arm's reach;
  - split as the calibration splits an average arm: upper arm 30.6, forearm 24.0;
  - the shoulders' rise 20, swing 15.
- **The envelope as a sphere:** fitting it directly failed (6.8 cm rms: lunges and shoulder shrugs). The page refused
  to Apply it, so that check works.

| | Before (51.9 cm, stretch 1.2) | Calibrated (54.6 cm) |
|---|---|---|
| Elbow drawn straight while the wrist is inside the calibrated reach | 2.1% of all frames | **0.0%** |
| ... with the wrist 52-54 cm from the shoulder | **73.9%** of those frames | **0.0%** |
| Elbow drawn straight at all | 4.2% | 1.8% |
| Wrist bent past a real wrist's range | 19.1% | 15.4% |
| Wrist deviation, mean | -7.2 deg (radial) | -0.9 deg |
| Wrist strain > 0 | 25.6% (mean 0.37) | 26.2% (mean 0.66) |
| Elbow swing > 10 deg | 16.6% | 17.5% |

- **Wrist strain went up** with the hands within 30 cm of the shoulder (0.24 to 1.12). There the longer upper arm folds
  the elbow harder, and the pole puts the forearm across the wrist.
- **Elsewhere it fell:** from 1.06 to 0.78 at 50-54 cm, and from 0.59 to 0.41 at 46-50 cm.

**Composites** (`final/takes_front_side.png`, `final/takes_eyes.png`):
- **The frames:** four of your takes (guard, aim, the punch extending, relaxed), from the front, the side and your
  eyes, before and calibrated.
- **What differs:** in the calibrated punch the elbow is still bent where the old arm had straightened. Otherwise they
  barely differ: the new lengths move the elbow a few centimetres.

**Align Sights to My Aim** now uses the shared stillness window and countdown (`vr_still.hpp`) instead of its own. Its
nailgun test run gave the same log, digit for digit, before merging
the round's latest vr-cleanup; after it, 0.0042 degrees and 0.059 mm (0.0041 and 0.057 before).

### Settings and commands

| | |
|---|---|
| `vr_body_upper_arm`, `vr_body_forearm` | real cm (0: the model's times Arm Length). Saved. |
| `vr_bodycal_seated` | the page's Position. Saved. |
| `vr_bodycal_preview` | Body in Front: 0 off, 1 facing you, 2 from the side. Saved. |
| `vr_bodycal_undo` | the settings from before the last Apply. Saved. |
| `vr_bodycal [standing or seated]` | starts it. `vr_bodycal_step <1..7>`: one pose again. |
| `vr_bodycal_apply`, `_cancel`, `_undo`, `_print` | the page's buttons; `_print`: every number, each pose's grade. |
| `vr_bodycal_refit <file>` | fits a saved session again. |
| `vr_bodycal_debug` | each empty hand's wrist, from the head and in the chest's frame. |

### Not verified

- **Your real arm.** The synthetic people are my models of an arm and a shoulder, and the takes give only your wrists.
  Only you can tell whether the calibrated elbow is where yours is.
- **The menu page in the headset:** its layout, the figure's size and distance, and where the text sits during the
  T-pose. In the mock, the capture, the fit and the page's numbers were driven by the commands.
- **The drawn wrist:** the calibration measures to the empty hand's wrist on its controller (your Hand Calibration).
  Holding a gun, the drawn hand may sit elsewhere (Weapon Offsets).
- **The elbow's direction** (Elbow Out, Back, From Hand) isn't measured. It causes the rest of the wrist's false bend
  in a guard, and with a fist at the face.
- **Melee:** not re-run; it doesn't read the arms.

### In the headset

- [ ] VR menu > Body > Body Calibration, Standing, Start. Follow the text and the figure; hold each pose until the
      click. About two minutes.
- [ ] Read the page: are the poses good? Redo any marked REDO.
- [ ] With Showing: New Measurements, straighten an arm slowly in front of you: the drawn elbow should lock when yours
      does. Switch to Current Settings and compare. Then aim, guard and punch: does the wrist look like yours?
- [ ] Apply. If it's worse, Undo. Either way, tell me, and keep the session file (`quakevr/bodycal/`).
- [ ] "Drawn wrists ... off": above 2 cm, your Hand Calibration's wrist isn't your real one.

## Simplification: Box3D only, knights always drop swords

Your voice notes (vrfiringrange, 2026-09-28): remove the too-niche options; knights always drop swords; Box3D is the only
physics engine, the bespoke pre-Box3D solver goes (Quake's own physics stays). Branch `agent/simplify`.

### What changed

- **The old solver is gone.** `vr_rigid.cpp` was Quake VR's own rigid-body solver (`vr_physics_engine 0`: each body an
  oriented box colliding by its corners with sequential impulses, split impulses, wedge escape, its own sleep, water
  and hit box). All of that is removed: `rigidToss`, `Body`, contacts, `settle`, `unwedge`, `waterStep`, the rest and
  water memos, and its `vr_debug_throw 3|4` prints. The code lives on in git: branch **`archive/old-solver-stacking`**
  (also on origin), which has it with the stacking experiment.
- **What stays in `vr_rigid.cpp`** (345 lines, was 1143) is what Box3D and the hands use: `VR_RigidToss` (keeps items
  and rigid bodies in the world, the first water check, then hands a rigid body to Box3D), `localBox` (the drawn
  model's box: `pointInModelBox`, `modelCentre`, `keepInWorld`), `carryAngles`, `vr_rigid_place`, the resets.
- **The `.vr_rest` field** (the old solver's sleep timer) is gone from QC and the engine: only the old solver read it;
  Box3D only wrote it. Old saved games still load (an unknown field is skipped quietly).
- **Box3D** is unchanged except that nothing asks for the engine any more (`vr_physics_list` prints `N, Box3D:`).
  Quake's own physics (`sv_phys.c`, `MOVETYPE_*`, QC's movetypes) is untouched; only a comment changed there.
- **Knights and hell knights always drop their sword** when they die, gibbed or not. Kept: a statue knight
  (spawnflags 2) drops none, as before: it has no sword to lose. The random draw is gone from `VR_DropKnightSword`.
- **Menus:** Throwing and Physics > Physics starts at Bounciness (the Physics Engine cycle is gone); Gameplay >
  Knights' Swords keeps only Sword Damage, its help now saying they always drop.
- **Retired cvars:** `vr_physics_engine` and `vr_sword_drop` stay registered, do nothing, and are no longer archived
  (the project's way, like `vr_carry_reach`): an old `ironwail.cfg` or a bind setting them loads without a word, and
  the next saved config leaves them out. `vr_defaults.cfg` no longer sets `vr_physics_engine`.

Lines: the code (engine and QC) is 875 lines shorter and 56 longer (comments and the retired cvars): **819 fewer**.
The Release binary is 27 KB smaller, and none of the old solver's strings are in it (`wedged at`, `rigid %d: origin`,
the contact prints; `vr_rigid.obj` has no `rigidToss`, `unwedge`, `waterStep`, `restMemo`).

### How it was checked

The same scenes before (vr-cleanup `d1e5bd2a`) and after, with `host_framerate` 1/72 so Box3D is deterministic;
`vr_physics_list` and `vr_physics_hash` at the end, and the screenshots (scratchpad `simplify/`, `sc.sh`):

| Scene | Before | After |
|---|---|---|
| a column of 5 health boxes (firing range) | hash `e92ce6db…` | identical |
| a shells box dropped on a stack of 4 | `ad1129d0…` | identical |
| a box thrown into a stack of 5 | `ba8208c2…` | identical |
| a pyramid of 6 | `650d4f75…` | identical |
| every prop piled, then a box thrown into the pile | `9474122c…` | identical |
| a grunt gibbed by a blast (firing range) | `664fdecd…` | identical |
| e1m1: a backpack and a loose armour on the ramp | `68a87764…` | identical |
| e1m1: a health and a shells box riding the plat | `d91df523…` | identical |
| e1m1: a grunt gibbed at the start | `d0bca7ed…` | identical |
| a shotgun gripped and thrown (`play_throw_weak.txt`) | lands on its side, asleep | the same (see below) |
| a shotgun thrown at a grunt (`play_throw.txt`) | kills it, lands asleep | the same |

The screenshots differ only in particles and the signs' flicker (the props are where they were). The two throws come
from recorded hand motions played by real time, so no two runs are the same, before or after: four runs of the
build before and three after land the shotgun at 309 -688 on the same side (one run before tipped onto the other
side), and the throw at the grunt kills it every time.

**Swords:** a knight, a hell knight, a knight gibbed by a blast: each dropped its sword (before too, at
`vr_sword_drop 1`). With `vr_sword_drop 0` set: before, two knights and a hell knight dropped none; after, all three
dropped theirs. The knights' scenes' hashes differ from before only because the QC draws one random number fewer.

**Old configs:** `exec` of a file with `vr_physics_engine "0"`, `vr_sword_drop "0.3"` and an unknown control cvar: only
the control prints `Unknown command`; physics stays Box3D.

### For you

- [ ] Throwing and Physics: the Physics section starts at Bounciness. Throw, stack and carry as usual.
- [ ] Kill a few knights and hell knights: each drops its sword.
- [ ] Your own `ironwail.cfg` still has `vr_physics_engine` and `vr_sword_drop`: no console error at start; they drop
      out of it the next time it is saved.
## Body collisions

Your note (e1m1): hands, arms, the body, the wrist gadget and held weapons all pass through each other. You asked for
an option that stops the most glaring overlaps, not a physics body: a hand pushed into the arm should stop at it, and
go through only once pushed fully through. Branch `agent/handcoll`; scripts, traces and composites are in the
scratchpad's `handcoll/`.

### What you see

- **A hand, or the weapon in it, stops at the surface** of your other hand, your other arm (upper arm and forearm), the
  wrist gadget on it, and your body: the torso, the neck and head, and the legs in the full body.
- **The arm follows:** the arm's IK reaches the drawn hand, so the elbow and forearm move with it.
- **Push on and it goes through.** It is held out until it is 70% of the way through (Pass Through At), or held out
  10.5 cm. Then it lets go and slides through in about a tenth of a second. It stays let go until it is clear again:
  no flicker at the threshold, and pulled back out it doesn't catch on the way.
- **Two hands** pressed together each give way half.
- **Elbows** swing out of the torso round the line from the shoulder to the wrist (a hand across the chest).
- **Drawn only:** melee, shots, aim, grabs and holsters use the tracked hands as before (below).

### How it works (`Quake/vr/vr_selfcollide.cpp`)

- **Proxies** (capsules; a sphere is a capsule of no length):
  - **Each hand:** the grasp's spheres of the jointed hand as it was drawn last frame (93), in the hand's frame. The
    six old models: three spheres along the hand.
  - **Each held weapon:** capsules fitted once per model and frame to its triangles (the grasp's shape, at the model's
    per-axis scale).
    - Along its length in 2 to 5 slabs. Each slab's cross-section (3rd to 97th percentile) is one capsule, or two or
      three side by side where it is flat. The shotgun gets 6.
    - A capsule whose middle is behind the hand is the stock.
  - **The torso** (from the head, this frame, as the body is posed): two upright capsules for the chest (0.125 m
    round, 0.058 m either side, times the build's torso) and two for the belly and hips, after make_vrbody.py's rings.
  - **The neck and head** (the head isn't drawn in your eyes, but it's there): a capsule and a 9.5 cm sphere.
  - **Each arm** as last posed: the upper arm, the forearm's thick half and its bracer. The shoulder and elbow ride
    the chest's frame, the wrist its hand.
  - **The legs** (full body): thighs and calves. **The gadget:** two capsules round its casing.
  - The wound capsules ("Dynamic wounds") were too coarse to reuse (one round torso of 7.5 units).
- **The test,** from the tracked hands (with the knocks and vr_model_collide's push in):
  - A contact is a hand (or its weapon) against one part, or the two hands (or weapons) against each other.
  - A new contact's way out is where the parts are deepest now: the way it came in (a millimetre in before it counts).
  - The way is kept while the hand goes deeper, following the surface only within 35 degrees of it. So a hand pushed
    through an arm isn't turned out of the far side half-way, nor slid round it.
  - Each frame the share through is measured along it: 0 touching on the side it came in, 1 touching on the far
    side, counting the whole hand or weapon.
- **Letting go** (the contact passes until the shapes are apart again):
  - past vr_body_collide_pass (0.7) of the way through;
  - or when it would hold the hand out more than 0.7 × 15 cm: one contact alone (a hand round an arm, its fingers
    already past it), or all of a hand's contacts together (pressed onto the gadget and the forearm under it, two
    hands and their wrists pressed together).
- **The push:** Gauss-Seidel over the contacts that hold, 4 rounds, each along its own way out; two hands share
  theirs. Eased: out in 0.012 s, back in 0.03 s as the hand comes out, through in 0.07 s once let go.
- **When:** `beginView`, after vr_model_collide's and before the weapons and hands are placed, moves `s.pos`;
  `endView` records what was drawn and puts the tracked hands back.

### Left alone: what is meant to touch

- A hand against its own arm, weapon and gadget.
- **Two-handed grips:**
  - The pairs between the two sides fade out as the grip is taken (its transition).
  - They also fade as an empty hand comes within 9 units of the other gun's grip or cup, gone at the 5.5 units the
    grab takes it from, and while it is still there after letting go. A pistol's cup and the shotgun's foregrip are
    reached and let go unhindered.
  - The helping hand, drawn on the gun, isn't pushed.
- **A prop in both hands**, a hand on a ledge and a gun carried by its foregrip: the hand is drawn on its grip, not
  moved.
- **The torch:** the pairs between the hands fade within 10 units of the torch in the other hand (the hand-over).
- **Holsters:**
  - The body against a weapon fades from 1.6 times a holster's reach to its reach.
  - Against a hand, only deep in one (from its reach to half of it): the holsters stand out of the body, so a hand at
    one is outside it.
- **The stock** isn't tested against the torso and legs (a shouldered gun rests on the shoulder).
- **The head:** no weapon is tested against it (aiming down the sights), nor a hand holding the torch or a gun.
- **A free hand against the other hand's weapon** stays vr_hand_collide's (triangle exact, the fingers brushing it).
- **Met while mostly eased out** (under half: coming to a grip or into a holster, a grip just let go), a contact
  passes until clear rather than holding with part of its push. The pairs between the hands come back over a third of
  a second after a grip or a hand-over. The helping arm swings back from the grip onto its controller meanwhile, and
  its gadget used to knock the gun 8 cm aside for a few frames as the grip was let go.
- **Align Sights to My Aim** turns it off while capturing.

### Drawn only: what the game reads

- `s.pos` is put back exactly after the view. The muzzles and two-handed grips have the push taken out, as
  vr_model_collide does.
- The muzzle flash and your beams start at the drawn muzzle (as with vr_model_collide).
- **The grip hotspot** the other hand takes was chosen from the drawn hands (vr_view.cpp). It is now chosen as
  tracked, so a push can't change which grip the game offers.
- **Kept as they were:**
  - the laser crosshair starts at the tracked muzzle;
  - a helping hand's drawn pose is what a gun is then carried by (`twohand::recordHelp`), whatever pushed the gun.
- **Checked** (`s8.py`: the shotgun held 3.8 cm out of the other arm, on against off):
  - `vr_dumpview`'s hands, controllers, aims, muzzles and foregrip: identical;
  - the melee's grip, far end and wrist as the server has them (`developer 3`): identical;
  - the shot's start and direction (`vr_debug_shots 1`): identical.
  - Only the drawn palms and the drawn gun differ.

### Settings (VR menu > Body > Body Collisions)

| | |
|---|---|
| Body Collisions (`vr_body_collide`) | 1: on (the default). 0: off, everything as before. |
| Pass Through At (`vr_body_collide_pass`) | 0.7: the share of the way through that lets go (and held out at most this × 15 cm). 1: only once all the way through. |
| Elbows Out of the Torso (`vr_body_collide_elbows`) | 1: on. |
| `vr_debug_body_collide` | 1: each contact's change printed, a line a frame in `body_collide_trace.txt`; 2: and the proxies drawn. |
| `vr_body_collide_bench [n] [list]` | times the solve as it is now; `list`: every capsule. |

### Tests (mock headset; the scratchpad's `handcoll/`)

- **The runs:** each scenario twice, off and on, the same scripted hand path in 1 cm steps.
- **The shots** (`s*_*.png`): your eyes and two spectator cameras, before and after, at each stage.
- **The traces:** `runs/*/on_trace.txt`. `offsets.png` plots each drawn offset over time, with the contacts' states.

| Scenario | Held out (the drawn hand from the tracked one) | Let go | Seen |
|---|---|---|---|
| 1. Off hand up into the main forearm, then through | up to 10.2 cm | at the limit (the hand round the arm, 0.49 through) | stops under the forearm; slides through in 0.1 s |
| 2. Main hand into the chest | up to 10.2 cm | at the limit | stays at the chest (side view) where it went in before |
| 3. Shotgun swept through the off arm | up to 9.9 cm | all four contacts together, held out 10.5 cm between them | the gun stays beside the forearm where it went under it before |
| 4. Main hand pressed onto the gadget | up to 10.1 cm (the tracked hand 12 cm down) | lifted first | rests on the gadget; before, the hand covered it |
| 5. Two-handed shotgun grip | none | the off forearm crossing the barrel on the way to the foregrip, and the gadget as the grip was let go, pass | grip taken, gun swung, let go: the same on and off |
| 6. Health box in both hands, to the chest and out | none | | the same on and off |
| 7. Shotgun holstered over the right shoulder, drawn again | 1.5 cm (the hand by the chest) | | holstered and drawn as before |
| 8. Palms pressed together, then through | up to 10.4 and 7.3 cm (each wrist in the other hand too) | both hands' contacts together at the limit | palms touch where they crossed before |
| 9. Hand at the chest (elbows) | | | the elbow swung 4.4 cm out of the torso |

- **Also checked:** a shouldered shotgun and one raised to the sights (no weapon contact; the hand 0.2 cm off the
  chest for a moment), and the six old hand models (three spheres a hand).
- **No flicker:** each contact goes in, holds, lets go and clears once in every run. A contact grazing at no depth
  used to come and go (harmless, 0 push); it now counts only a millimetre in.

### Cost (`run.sh --exclusive`, RTX 4090, the mock at 250 fps)

- `vr_profile`, the `body collide` scope, per frame (5 s averages):
  - hands apart: 4 µs;
  - the shotgun held into the off arm: 17 µs;
  - pressed onto the gadget: 15 µs.
  - At most 0.26 ms in a window.
- `vr_body_collide_bench` (medians): apart 2.4 µs; the shotgun in the arm 10.2 µs (7 contacts tested, 4 holding);
  the gadget 12.0 µs.
- The elbow's test runs only with an elbow in the torso.

### Not verified

- **In the headset:** how the sizes feel, the 10.5 cm limit, and whether sliding through in a tenth of a second reads
  as "through" or as a pop.
- **Your body** is the model's (the build's torso), not yours: a real chest thinner than the drawn one stops the drawn
  hand in front of it (it doesn't go into the drawn chest).
- **Props** (boxes, gibs, the torch) aren't proxies: a carried box still goes into the chest.
- **Fast swipes** through thin things (the gadget, a wrist) within one frame aren't caught: each frame's pose is
  tested, not the motion between.
- **A pistol with a cup hotspot:** the hands don't collide within about 34 cm of the cup (the grab's reach and the
  fade), so the cup is never blocked.
- **Melee:** the archived takes weren't replayed; the melee reads none of it (`s8.py`).
- **Not run in the mock with it on:** the torch's hand-over and climbing. They are excluded by the rules above.

### In the headset

- [ ] Push a hand into your other forearm, slowly: it should stop on it and your arm follow; push on and it slides
      through. Pull back: no catch.
- [ ] Tap and press the wrist gadget; clap; rub your hands.
- [ ] Sweep a gun through your other arm; hold it against your chest; shoulder it and aim down the sights.
- [ ] Take the foregrip, cup a pistol, carry a box in both hands, holster at the hips, chest and shoulders, pass the
      torch between your hands: none should be hindered.
- [ ] If it holds too long, lower Pass Through At; if a hand pops through too soon, raise it. Tell me which.
