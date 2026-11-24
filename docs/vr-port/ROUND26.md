# Round 26: from 2026-11-24, towards 1.2

A per-feature reference: current behaviour only. Round 25 (2026-11-12 to 11-24, 12,400 lines) is history: `ROUND25.md`;
earlier: `ROUND24.md` to `ROUND22.md`. Grep them; never read one whole.

Open work and bugs: BACKLOG.md (section 1, headed "Open on 2026-11-24", names the ROUND25.md sections that hold the notes).

## Adding a section

One short section per feature, appended at the end (never rewrite others' sections; the file merges as `union`):
a heading "## <what it does> (branch agent/<name>, <date>)", then what it does, settings and defaults, files, tests with their
numbers, open issues. Fold a follow-up into its feature's section instead of adding a new one. Start ROUND27.md when
this file passes about 2,000 lines.
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
