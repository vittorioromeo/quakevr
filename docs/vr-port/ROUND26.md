# Round 26: from 2026-11-24, towards 1.2

A per-feature reference: current behaviour only. Round 25 (2026-11-12 to 11-24, 12,400 lines) is history: `ROUND25.md`;
earlier: `ROUND24.md` to `ROUND22.md`. Grep them; never read one whole.

Open work and bugs: BACKLOG.md (section 1, headed "Open on 2026-11-24", names the ROUND25.md sections that hold the notes).

## Adding a section

One short section per feature, appended at the end (never rewrite others' sections; the file merges as `union`):
a heading "## <what it does> (branch agent/<name>, <date>)", then what it does, settings and defaults, files, tests with their
numbers, open issues. Fold a follow-up into its feature's section instead of adding a new one. Start ROUND27.md when
this file passes about 2,000 lines.

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
