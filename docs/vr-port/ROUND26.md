# Round 26: from 2026-11-24, towards 1.2

A per-feature reference: current behaviour only. Round 25 (2026-11-12 to 11-24, 12,400 lines) is history: `ROUND25.md`;
earlier: `ROUND24.md` to `ROUND22.md`. Grep them; never read one whole.

Open work and bugs: BACKLOG.md (section 1, headed "Open on 2026-11-24", names the ROUND25.md sections that hold the notes).

## Adding a section

One short section per feature, appended at the end (never rewrite others' sections; the file merges as `union`):
a heading "## <what it does> (branch agent/<name>, <date>)", then what it does, settings and defaults, files, tests with their
numbers, open issues. Fold a follow-up into its feature's section instead of adding a new one. Start ROUND27.md when
this file passes about 2,000 lines.

## lancepull: flatlance_test section 5's flake explained; the stuck Lance's pull-out and H are counted where the body holds still (branch agent/lancepull, 2026-11-24)

- **Cause (the test, not the game, in every run but one):** the counted checks in section 5 depended on chance. How many
  thrusts stick, and whether a press or the body gets a stuck Lance out, varies. The body moving off carries the Lance out
  into the hand ("too far from the hand" / "pulled out: slid", both `VR_Embed_Free`, so it's back in the hand as the
  Lance) before a press or an H. What moves it: each blow's push (`vr_melee_push`: 38 units in a third of a second; the
  first ogre ended ~100 away, so in some runs not one thrust of section 5 landed), staggers, and the ogre's own pain
  frames stepping back 23 units (vanilla `ai_pain`, 2 of its 5). Even with all of those off, a moving body's animation
  slides a 1-4 unit deep tip out. Before: 2 of 5 runs passed (H found no Lance stuck: 1; "fire pulls it out" with 0
  pulled out but every stick back in the hand: 2). In every one of those the hand got the Lance back.
- **Fix (tests):** flatthrust_test.sh's `vr_embed_test 59` (its grunt held still, QC/vr_flatthrust_test.qc) gets a second
  case: the Lance stuck, H lets go (the hand empty), it stays in the grunt a second later. Deterministic: 7/0. In
  flatlance_test.sh, section 5 now counts only the losses: every stuck Lance back in the hand, the hand never letting go,
  still the Lance after a press of 8, no magazine. The stick and pull counts are printed, not checked, and its H case is
  gone (now 59's). The blows' push is off from section 3 through section 5 (`vr_melee_push 0`, back to 0.5 after) so the
  ogres stay in reach. The log filter now keeps `thrown:` / let-go / knocked-out / dropped lines.
- **Open: one real loss seen** (flatlance after-run 3 of 10, logs in `scratch/lp_fin/log_3.txt`, posebake worktree): in
  section 5, after a shaft hit with the ogre touching ("the blow read as no stab (kind 2)", point 0.33), the main hand
  was empty: later V's were fist punches, the HUD read weapon 0. No embed lines, so it was not the hold. The old filter
  hid the cause. 7 more runs with a wide filter (`scratch/lp_wide.sh`) didn't reproduce it. Suspects: the pole pressed
  into the touching body letting go of the hand's weapon (as at a wall, lanceoffhand's open item), or a drop path. The
  new filter lines should name it next time.
- **Tests:** flatlance_test.sh 9 of 10 runs 29/0 after (the 10th was the loss above); flatthrust_test.sh 12/0 and 7/0;
  spear_test.sh 4/0.
- **Seen:** a stuck flat Lance rarely needs the fire press in play: the blow's own push carries the ogre off it within
  a third of a second, and the Lance comes back to the hand.
## polewall: the flat spear's other hand stays on the pole at a wall; no elbow snap in a pole slide (branch agent/polewall, 2026-11-24)

- **Lance at a wall** (lanceoffhand's open end): head-on (no along-the-wall share) the wall state let go of the other hand
  (flatsight `wallLetGo`): pin 0, the hand 18-20 units off the tilted pole. A pole weapon now keeps hold (vr_flatsight.cpp
  `wallLetGo` returns 0 for `twohand::poleWeapon`): pinned, it rides the tilt (vr_view.cpp's rigid move, setupHand's grip).
  Measured (vr_flat_pole_track, vrtesthall 0 560 north, `vr_flat_wall_slide 0`): off 18.1 / along 6.8 -> off 4.4 (5.2 in the
  tilt's ease) / along 9.1 (standing 2.3 / 9.2). Along a wall (slide share 1, the default case there) it already kept hold:
  off 2.0-2.6. **Open:** the 2 units more off at the tilt is the drawn hand's turn on the tilted pole (not the collisions:
  the same with vr_model_collide/vr_body_collide 0); flatlance_test's new `pwall` check allows standing + 3.5.
- **Pole slide elbow snap** (flatfb1124b's open end): found in the arm IK trace (vr_debug_arm 2): ~10 frames in, the crouch
  lowering the shoulders under the locked hands straightens the arm; a two-bone elbow swings ever faster near straight
  (124 -> 143 -> 180 degrees in two frames, 8.6 cm), and the other arm snapped straight 8 cm as its shoulder reach ran out.
  Now a soft reach (vr_avatar.cpp solveArm, `vr_body_arm_soft` 0.05, Tunables > Body > Body "Arm Soft Reach"; 0 = old):
  over the last 5% of the arm the elbow eases straight and the bones lengthen by at most 1.9% to keep the wrist; past all
  reach the arm eases on towards the wrist (`ikWrist`) instead of snapping straight. All drawn arms (VR, flat, others).
  rope_flat_test poleslide: elbow move most 2.74 -> 1.46 units a frame, shake 2.56 -> 1.09 (its bound 3 -> 1.5);
  rope slide unchanged (shake 2.38-2.52: the swaying rope's wrist targets jump 8 cm at its end, not the IK; bound 3 kept).
- Tests: flatlance_test 33/0 (new pwall), rope_flat_test slide 9/0, poleslide 10/0, spear_test 4/0. menu_paths.txt regenerated.

## embedtests: VR embed test 24 cases 2/3 aimed by the axe's edge as held (agent/embedtests, 2026-11-25)
- Test 24 (`vr_embed_test 24`) `VR_EH_Blow` swing: for an axe the way "across" was the level side of the blow (laid out for axe pitch 0). Now it is the
  hand's forward turned square to the blade's line, reversed (`VRGetHandRot`: the edge faces so with the shipped Weapon Offset pitch 15.5, found by
  `vr_debug_axestick` leads). Test-side only (QC/vr_embed_test.qc); swords unchanged.
- Numbers: 24 before 16/3 (cases 2 and 3 fail, plus the known flaky "eased"); after 21/1 over 2 runs: cases 2 and 3 pass; the one left is the gentle
  tug in case 3 (hand reaches its goal 30 cm out, the blade is still held at t 4 s, `vr_hold_tugv` 0): case 3 never got that far before. Open.
- Test 33 (embedgrip) read 15/0 in 2 runs on this tree (case 46 passed): not reproduced here; no change made. Test 52 (embeddual) 2/2 fail "case 1
  swing stays in" with no change of mine in its path (it does not use VR_EH_Blow): not investigated.
- Update: case 3's gentle tug was the test's goal: the swinging hand runs on past the blade (`vr_hold_err` +10 units at the stick), the goal assumed 0, so
  the pull stopped 6 units short of the 20 cm tug. The goal now adds `max(0, vr_hold_err)`. Test 24 now 22/0 (2 runs), test 33 15/0 (3 runs).
- embeddual (52): `origin/next`'s own QC/vr_embed_test.qc fails the same, 2 passed 2 failed ("case 1: the axe's swing stays in the grunt, held (4 swings)"), 2 runs: a regression on next, not this change; not fixed here.
## spearfix: a thrown spear sticks in grunts; armed grunts hold the spear as a spear (branch agent/spearfix, 2026-11-24)

- **Thrown pole weapons stick in bodies** (engine `vr_axestick.cpp` `stickByPoint`): before, a spear thrown at a grunt stayed
  in about 1 run in 3 (ROUND25.md "spear", open): the moving model's surface met the point up to 70+ degrees off straight in
  and it skidded by the sword's rule (`vr_embed_sword_incidence` 65). Now a pole weapon's blade (`Blade::pole`, from
  `twohand::poleWeapon(slot)`: its slot's Other Hand Anywhere 3, data, not the spear by name) thrown into a body (a model or
  a monster's box) judges its glance as the lesser of the surface's and the body's (its flight against the level way to the
  upright middle of the body's box, `bodyGlance`), up to the new tunable **`vr_embed_pole_incidence` 70** (Tunables, Embedded
  Blades: "Thrown Spear: Most Glancing"). The level, props, other swords' throws and every held stab: unchanged. The bounce
  reason reads "too glancing (a pole weapon's: vr_embed_pole_incidence)".
- **Armed grunts with the spear** (toolgun Armed Grunt: Spear, code 106; any pole weapon by its slot: QC
  `VR_PoleWeapon(wid, flags)` in vr_poleweapon.qc, `VR_AGP_Pole(p)` in the pose): two-handed (`VR_AGP_TwoHanded`, unless its
  Weapon Holds row says One-Handed), never paired (`VR_AG_OneHanded` false). Pose (`VR_AGP_Melee`): carried at port arms
  as other two-handed weapons; ready, low and level at its right hip (`VR_AGP_POLE_READY` '3 -5 5'), the head at its enemy
  (`VR_AGP_PoleAim`); the off hand a point on the shaft `VR_AGP_POLE_SPAN` 11 ahead (the slot's grip hotspot is the
  player's, off the shaft). Its attack a **thrust**: drawn back 4, driven out 8 along the line at the target, the blow
  (`VR_AGP_Strike`, W_SwordMelee) on the way out; the front hand keeps its place and the shaft slides through it; both
  hands stay on (`VR_AGP_SwingOneHand` false). No Pose Studio keys for a pole weapon (none authored; `VR_AGK_Keys` returns
  early outside a pickup). The engine's native pose (`vr_builtins_agpose.inc` agPose) leaves a pole weapon's holder to
  QuakeC (one copy of the pose; rare).
- **Debug:** `vr_agholds_report` (Debug > Armed Humanoids: Weapon Holds Report) prints for a pole weapon's holder
  `agpole <ent>: right hand X off its shaft (share along), left ...: both on the shaft | off the shaft` (within 3.5 units;
  the solved grips kept per slot in `armSeen`, outside ArmPose: the Live tape's size).
- **Tests:** `spear_test.sh` 4/0 (VR 25/0: case 3 "a spear thrown at a grunt stays in it" now a check (it began in the wall
  case 2 left you at: fixed, from home); new case 8: ten straight throws at a grunt posed in run1..run8, stand1, stand5,
  turned -35/0/35 and struck up to 5 units off its chest, its AI step off: 10 of 10 stuck (the old rule: 9 of 10, 70 deg
  glances); flat 6/0). New `vr_agrunt_test 230` (QC/vr_armedgrunt_spear_test.qc) via `Misc/quakevr/agspear_test.sh` 2/0:
  the spear, two-handed, its head 0.2 deg off its enemy, agpole right 2.1 / left 3.3 off the shaft (both on), its thrust
  15 damage. Eyeshot `qvr-kit/scratch/spearfix_grunt.png`. `flatlance_test.sh` 31/1 (the known H flake, BACKLOG 1).
  Scene `agrunt_spear`.
- **Open:** the off hand is a point goal (the IK's fist at the shaft), not a grip frame turned round it; a Lance on a grunt
  (not in any pool) would take the same pose untested.
## menupaths

`vr_menu_dump_paths` built the Held Weapon pages (Weapon Offsets, Weights, the Offsets parts) with no weapon in the hand, so they were only a "hold a weapon" line and a full regeneration of menu_paths.txt dropped their rows (they were hand-added). The dump now pins weapon slot 16 (cvars `_17`) while it builds the index (`pinSlotPages`, vr_menu_paths.inc) and unpins after. A full regeneration now keeps every row. Diff vs before: ~66 real additions (Headset Taken Off, Centre Message Size, Spin Up/Motor Volume, flat spear/thrust tunables, eye rect/dither, debug rows), 1 rename (Wheel Hold / Quick Tap Time), 2 removed duplicates (Mark a Gap I Got Through). `vr_menu_path_check maps/vrcalibration.map`: 6 found, 0 missing; menu_shortcuts_test 14/0.

## agfix1125: armed-grunt test regressions on next, bisected (branch agent/agfix1125, 2026-11-25)

Bisected over next's first-parent merges d137c1f7d..c93a53f2c (346; Release builds; seed 1; `vr_agrunt_test N` in vrtesthall).
Each is its own cause, mostly a legitimate change that moved a seed-1 test off its assumption:
- **61** Dagger thrown by you: `agent/throwfix` (a667ce354; blades come round point first); Knight's Sword thrown by you:
  `agent/flatthrow2` (6978347af; a long throw stays its thrower's, so it sticks). The test's arc toss (160 forward, 120 up)
  now sticks point first in the floor, and a blade stuck in the level is never taken (VR_AG_FindWeapon: VR_AxeStick_Kept).
  Test: a low flat toss that lands lying. That shifted the seed and showed a real game bug (case 32, enforcer Crowbar
  lying): a weapon at rest with its origin a hair under the floor's plane (z -0.14 on a floor at 0, kicked by its feet) read
  as solid: VR_AG_Reachable said "in a liquid" and FindWeapon's line of sight hit the floor, so an unarmed humanoid stood
  idle beside it for good. Fixed: both test 4 units above the origin. The not-taken case prints why (`pickup case N: not
  taken: ...`), and the failure line says whether the source was stuck.
- **42** Shotgun + Axe "both held": `agent/flat1116` (913b970c7) made a gun then a one-handed blade a pair from the keys
  (vr_ag_gunaxe, the toolgun's gun+blade pairs): stale expectation, now TRUE (Axe + Shotgun stays FALSE).
- **74** downtug 18/23 (80% needed): `agent/btnfix` (400898fcb; monsters no longer press command buttons). No downtug code
  changed: seed 1's judo-throw getting-up tries went from "no state" (hand never got onto it) to "nothing" (the humanoid
  already fully up at the press, knockdown 0). Not fixed (open).
- **203** dualdisarm 11/12 "going for the fallen one": `agent/vr1116d` (88e354c0f; the knight's grip and weapon values). The
  spare was dropped by a humanoid spawned 25 ahead, inside you: the weapon fell at your back and the walk round you made
  no headway ("gave up: no nearer"). Test: dropped 60 ahead, 35 degrees to your side. Case 11 passes; case 12 (a knight's
  two swords) now takes it but its swords read undrawn (weapondrawnpose 30 '0 0 0', gap 0.0) through the rest of the case
  in the full run only (from case 12 alone: 7/7): open.
- **35** parry Axe "2 blows, 0 parried, 22 damage": `findings-qc-2026-11-19` merge b9967a243 (readytime per player,
  Mjolnir frames, items; a direct merge on next). Not analysed further (open).

Lines 104..120 of that range don't build with this compiler (vr_menu.cpp's unused lambda capture `[page]`); the bisect
patched it per build and stashed the patch (two `agfix bisect patch` stashes).
