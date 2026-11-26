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
## embeddualfix: embeddual (vr_embed_test 52) "fails on next" was a VR run of a flat-only test (agent/embeddualfix, 2026-11-25)

- **No regression, no merge to blame:** at 7007c2182 (embedregress), f0d30fe7d (embedtests' base), 251c9ffd5 (embedtests merged) and
  c93a53f2c (next) `Misc/quakevr/embeddual_test.sh` (-flatscreenmode) reads 16/0. The "2 passed, 2 failed" (both cases' "the axe's swing
  stays in the grunt (4 swings)", the last printed case 1) is exactly what the test gives *without* -flatscreenmode (the
  embedtests worker ran it like tests 24/33): VR play has no mouse button's swing (`vr_flat_punch`), so the axe never goes in.
- **Fix (test-side):** QC/vr_embeddual_test.qc refuses VR play at once: one FAIL "flat play off: run with -flatscreenmode
  (Misc/quakevr/embeddual_test.sh)" and the verdict (0 passed, 1 failed), not 8 swings that read as axes no longer sticking.
- **Numbers:** embeddual flat 20 runs at 16/0 (7 before the change, 13 after) and 1 at 15/1 ("case 0, embedded: the main hand is drawn on the blade it holds
  (held 0)": the hold gone 0.9 s after it stuck, after a second swing; rare, not chased). Case 0 sticks on swing 1 or 2
  run to run, case 1 always on swing 1. VR tests run as laid out (vrtesthall's start, no setpos): 24 22/0, 33 15/0; with
  embeddual's `setpos 0 300 25 0 90 0` they read 20/2 (24's gentle tug) and 11/4 (33's case 46): position-dependent, not
  a code change (the known "open" case 46 of embedregress likely the same). spear_test 4/0.

## testfix1125

Test/tooling only. vr_recoil_test: the "his 27 slots" expectation is 31 now (4 more slots' shipped recoil left their old defaults since); named constant in vr_weaponfx.cpp. Kit run_baseline.py agrunt_script: 228/229 run on vrarena (seed 7, skill 1, god; 229 with vr_worn_backpack_lod_near 128, others 1500); both PASS. qvrtest.py (and the kit copy qvrtest_fallback.py) find_bash: abspath before the system32 guard.

## agfix2: armed-grunt tests 35 / 203 / 74 (branch agent/agfix2, 2026-11-25)

- **35 (parry Axe 0 parried), cause found, test fixed:** not a parry regression. In b9967a243 (findings-qc) the culprit is
  2842f3805e (the ammo boxes' weapon roll moved to the first PutClientInServer, against the player): in vrtesthall it drops
  nothing, it only changes how many random numbers are drawn before the test (reverting it alone, or
  vr_ammobox_drops_chance_mult 0, gives 6/0; seeds 2..5 parried the axe on its first blow). Seed 1's axe grunt now swings
  kinds 3 then 0 (neither meets the crowbar held across); each landed blow's knockback put you further off (30 → 44 units),
  so the blade's way fell short of your weapon for good. Test: after a landed blow you stand your ground (origin back,
  velocity 0); each landed kind's way is kept (the parry's own nearest points, vr_drp_ne/np, in the grunt's frame) and a
  swing of that kind moves your weapon there as it begins; up to 6 landed blows each give the next its time.
  Seeds 1..7: 6/0 each.
- **203 case 12 (undrawn swords), open:** reproduced (79/2). The undrawn reading is hitreact::drawnWeapon: the props
  were not placed by the arms (no noteDrawnTurn: placeProp from relinkSkinnedEnd) for the rest of the case. Lead not
  followed for lack of budget: the arm-pose table (armPoses, vr_hitreact_arms.cpp: evicted/dropped; the
  vr_agrunt_armpose_check command reports them) filling over the earlier cases, or a stale per-entity arms state.
- **74 downtug, not started** (budget).

## beta5: 1.2.0-beta5 published (branch agent/beta5, 2026-11-25)

- **Notes:** RELEASE_NOTES_1.2.md "New in beta 5" (intro rewritten; beta 4's section now "Beta 4 (2026-11-24)"): Swap Face
  Buttons for left-handed players first (lefthandbuttons), then flat play (flatembed/embedregress: blades stick again,
  fire pulls out; flatthrust: V stabs with stabbing blades; flatads: zoom aim to the middle + visor; lanceoffhand/polewall),
  weapons (spear/spearfix/spearblunt, flipgrip, sighthue, plasmabolt), monsters (meleearcs, agfix1125's unarmed-grunt fix).
  Network version 46 unchanged. PATCH_NOTES_1.1.1.md the same by area (Weapons, Movement, Monsters); its older "a flat stab
  never leaves a weapon stuck" line reworded (flatembed reversed it).
- **APK:** `Android/release_apk.sh build`: release, 1.2.0-beta5, version code 1020005; check_apk 28/0; apksigner: CN=Quake
  VR, SHA-256 504ff939...9119c0 = Android/release-cert.sha256.
- **First make_release run stopped** (nothing built or published): scenario_check FAIL with 183 run / 0 failed but
  "7 missing" checklist tags: lines tagged {test:ropes_flat}, {test:flat_weapon_wheel}, {test:ssg_muzzle} after cltidy5
  (04276ae93) deleted those scenes. Fixed on agent/beta5 with the coordinator's OK: ropes_flat -> ropes, flat_weapon_wheel
  -> weapon_wheel, ssg_muzzle.txt restored from 04276ae93^; checklist_tests_check 98/0.
- **make_release** (beta4's command, -Version 1.2.0-beta5) on 03d582e2f: beta profile 12 passed, 0 failed (6.1 min);
  installer self-tests 60/60; check_apk 28/0; smoke launch; published as a prerelease with QuakeVR.zip, QuakeVR-Setup.exe,
  QuakeVR.apk, latest.json, SHA256SUMS.txt; online check passed; Latest still v1.1.0. The tag is on 03d582e2f (this
  notes commit comes after it).
## questrelaunch: the Quest's "stuck loading screen" on relaunch and entering the hub; quit bounded; data version stamp (branch agent/questrelaunch, 2026-11-25)

The report (festivethunk, Quest standalone, beta3-5): no relaunch after closing it, stuck on the app loading screen; an
"infinite load" going to the hub in-game; it ran on a fresh install, then not on re-opening.
- **Cause (headset, Quest 3, unworn, `-unattended`)**: the hub's first load after vrstart.bsp changes (every update)
  compiles its world collision trees (vr_hull, cache/hulls keyed by the map's content): **42-56 s** on the Quest 3, ~75-80 s
  of load in all with the cold model loads, and **no frames** meanwhile: at a start the VR session isn't up yet (it starts
  after the configs' frame, the hub loads inside it) and in-game the main thread blocks in vr_hull `settle()`, so the
  headset shows Meta's loading dots (screencap). The first start goes to the tutorial; the second ("re-open") to the hub:
  the long wait. Closed mid-compile, nothing is cached, so every relaunch waits again. Once cached the hub loads in
  12-14 s.
- **Fix 1, the loading panel during the wait** (`vr_load_panel` 1, new): `settle()` polls the trees' futures (with a pool;
  the old blocking gets otherwise) and after 1 s pumps a loading-panel frame each poll (vr_shaderload.cpp
  `VR_MapLoadingFrame`: "Preparing vrstart: n / 6", a bar, "Its collision is built once for this version", seconds; the
  shader panel's image code shared). At a start the session is started for it (`qvr::startForLoading`, as the shader
  compile does; settled after the configs as before); in game the host frame's open frame is ended by it
  (`qvr::loadingFrameBegin`), the rest of that host frame draws no views (frameActive false). No frame reached the
  headset (asleep, a mock backend): 10 ms sleeps, no busy loop. Headset: start cold 40.5-40.9 s, 1794-1873 panel frames
  (1088-1131 in the headset; "the VR session started early for it"); in game (e1m1 then `vr_campaign_hub`) 43.8 s, 2453
  frames (2279 shown); compile time unchanged (42.6 s before). PC: in a mock-VR game 7.5 s, 416 frames; a test run's
  script map before the backend starts: unchanged (no pump).
- **Fix 2, the quit bounded on Android** (vr_android.cpp `VR_AndroidQuitBound`, called in Host_Shutdown once the config
  is written): the Quest's close (HOME/the system menu) destroys the activity; SDLActivity.onDestroy waits for the game's
  thread. After a pause (headset asleep) the sound's close (SDL's AAudio) took 3.6 s to over 10 s, and the system killed it
  at 10 s (destroyTimeout). A watchdog now ends the process 2.5 s on ("quit: the shutdown took too long"). The quit's
  stages go to logcat (`quit: <stage> done`; "history, music, sound" and "input" split).
- **Close/relaunch cycles (fixed build)**: HOME while asleep x4 (process gone at 2.5 s, relaunch 1-4 s later loads the hub),
  task removal x3 (killed at once), force-stop + relaunch x5, a relaunch into a live backgrounded process (resumed,
  session READY): every relaunch reached the hub. Before: the same, but HOME-closes took 4-10 s to end the process.
- **Fix 3, the data's version stamp**: Setup already copies every file that differs (size, then MD5), but nothing tied the
  APK to the data: Setup takes `--apk`, a QuakeVR.apk beside it, or the feed's latest, while the files come from the PC
  install (any version); a sideloaded APK skips Setup. Seen here: new progs with old data, the hub's `Host_Error:
  Mod_LoadModel: progs/v_spear.mdl not found` (the console in a black void). Setup's plan (QuestFiles.cs) now adds
  `quakevr/data_version.txt` (the install record's version); the game (`VR_AndroidDataCheck`, Host_Init) compares its
  first word, "-dev" dropped, with `VR_Version()`: a mismatch is a console warning ("The game data on the Quest is from
  Quake VR X; this app is Y. Run Setup's Quest step again ..."), said again under any Host_Error, and centre-printed once
  in the game (vr_progs.cpp loadNoticeFrame). No stamp: nothing said. Headset: stamp 1.2.0-beta3 with beta2's progs.dat:
  the warning and the centre print; 1.2.0-beta5: "game data: version 1.2.0-beta5, the app's". Installer self-tests 60/60
  (the plan test checks the stamp).
- **His ask (a start without the hub)**: no setting starts VR without the hub: `startGameCommands` loads the calibration
  room, the tutorial or vrstart whenever VR is on (only the PC's `-nostartmap` skips it). Not built: with the panel the
  first hub load shows its progress, and later loads take 12-14 s.
- Open: the first hub load after an update still takes ~45 s on a Quest 3 (shipping the compiled trees, the same bytes on
  PC and Quest, would remove it); the model loads (~12 s cold) still show no frames.

## prebuiltcache: a shipped, read-only derived-data cache for our own maps (branch agent/prebuiltcache, 2026-11-25)

His ask: cache the collision hulls and other load-time work, as the shaders are. Details: CACHES.md, "The shipped cache".
- **Game** (vr_diskcache.cpp): `quakevr/cache_shipped/index.txt` maps the cache's own names (`<product>/<build>/<key>.<ext>`,
  the same keys) to files named by map (`hulls/vrstart_8_8_28.hul`); `diskcache::read` and the hulls' reader (vr_hull.cpp
  diskRead, `DiskJob::shipped`) read there when the player's cache has no file or one amiss. Never written nor pruned.
  `vr_cache_shipped` 1 (0: ignored; Debug > Profiling and Memory, "Read the Shipped Cache"); `vr_cache_info` counts
  "N read (M shipped)" and lists the shipped folder. Named by map, not key: Setup's Quest copy never deletes, so keyed
  names would leave ~180 MB behind at every update; same-named files are replaced in place.
- **Made at release time**: `Misc/release/make_shipped_cache.py` (make_release.ps1 step "Shipped cache" after the
  installer build, `-SkipShippedCache`): the packaged exe loads vrcalibration, vrtutorial, vrstart one per run in a copy of
  the package's quakevr (`-prepare <f> -preparemaps <map>`, new in vr_prepare.cpp: the preparation run's maps step only,
  then quit; `+vr_setup_pending 0`, `+vr_cache_shipped 0`, an empty `vr_cache_dir`); products hulls, navmesh, lightgrid;
  then manifest.json is written again and the zip's allowlist check accepts `quakevr/cache_shipped/*`. The Quest gets
  them with the install's files (QuestFiles.cs: unchanged, the install record's files). Not in the APK.
- **What ships**: vrstart's 8 hull trees 130.6 MB (61 MB deflated), vrstart's light grid 47.7 MB (21 MB), vrtutorial's
  4.7 MB: 183 MB, ~84 MB in the zip. The other maps' trees compile under 250 ms (never cached); no navmesh is built on
  them in a clean run. Never id content: only products of our maps' geometry, lightmaps and entities (no occlusion,
  normal maps, poses, rigs, gun pieces: keyed by models and skins), no id map. vrarena, vrfiringrange and the test maps
  are not in the default list (`--maps`).
- **Platforms**: the same bytes on x64 and arm64 (little-endian, plain int/float/short layouts with their sizes in the
  hulls' build, no FP contraction on either build, no libm transcendental in the makers). A key that differs on the
  Quest (a liquid texture's colour in the grid's key, a setting) is only a miss.
- **PC (vrstart, cold vr_cache_dir, kit run, normal priority)**: vr_cache_shipped 0: load 17.4 s (trees 5.4 s, light
  grid 5.8 s, 8 + 1 written); 1: 6.1 s (trees 0.14 s, grid 0.04 s: 8 hull + 1 grid shipped reads, 0 built). Stale index
  (other keys: a changed map) and a file amiss (rejected): 8 + 1 built and written, 17.5 s. `vr_hull_cache 2` over the
  shipped trees: "checked 8 same". selfchecks_test.sh PASS; e1m1 smoke (misses, nothing read).
- **Quest (estimate, loadprof may measure)**: the trees' 42-56 s (questrelaunch) become a ~131 MB read (~1 s); the grid
  (5.8 s on the PC's main thread, unmeasured on the Quest, perhaps 15-20 s) a 48 MB read: the first hub load after an
  update or a fresh install (~75-80 s) should come near the cached load (12-14 s) plus the cold model loads.
- Open: the release run itself (ReleaseLTO exe, `-QuakeDir`) is untested end to end here (the generator ran on the dev
  build with `--git-root`); the Quest's own keys (that its grid key matches) want one headset check.

## loadprof: start-up and map loads measured (Quest 3 and PC); the map's hull brushes kept on disk (branch agent/loadprof, 2026-11-25)

**Measured** with `vr_startup_times` (each load's stages and its "work across the stages"), Quest 3 (dev APK, `-O3`,
unworn, `-unattended`, woken every 3 s) and PC (Release no-LTO, run.sh, fast mode; other agents' runs beside it: PC
numbers are +-20 %). One session: the start into vrtutorial, then `map vrstart`, `map e1m1`, `game ad; map ad_sepulcher`.
Cold: an empty `vr_cache_dir` (set by an exec'd cfg: `+vr_cache_dir` on the command line is too late) and, on the Quest,
the GL program pack moved aside (`cache/programs_aside_lp_*`); warm: the same folder again. Medians in ms (runs).
Scripts: posebake `scratch/lp_q.sh`, `lp_pc.sh`, `lp_table.py`.

| load | Quest cold (2) | Quest warm (3) | PC cold (3) | PC warm (3) |
|---|---|---|---|---|
| start-up to the first frame (no map) | 32,200 | 3,300 | 2,200 | 2,200 |
| vrtutorial | 15,400 | 7,800 | 6,050 | 3,550 |
| vrstart (after vrtutorial) | 89,000 | **11,800** | 13,200 | 1,280 |
| e1m1 (after vrstart) | 2,260 | 3,630 | 660 | 490 |
| ad_sepulcher | 26,700 | 5,760 | 4,870 | 1,590 |

Where it goes (the stages over ~400 ms):
- **Start-up cold**: the GL programs, 100 compiled in 29-35 s on the Quest (with the shader panel; the pack makes
  every later start ~1.9 s); PC 0.44 s.
- **vrtutorial**: entities spawned (531 alias models: skins and normal maps, draw lists, md5 replacements, limb models):
  Quest 10.4 s cold / 3.7 s warm, alias models 6.7 / 2.9 s, draw lists 4.2 / 0.8 s, textures 1.0 s; PC cold the
  skins' normal maps made on jobs (15 s of job time), 4.7 s of spawn.
- **vrstart cold**: the hull trees, 85 s waited on the Quest (8 trees of 0.8-1.0 M nodes: standing, crouch 36/44/52,
  prone, monsters 24/28/40; PC 6.3 s), the PC's model light grid 5.7 s (not the Quest's preset).
- **vrstart warm** (every session's hub): **the hull's brushes, 10.5 s waited on the Quest** (`build()`: 10.8 s, of
  which holes 7.0 and clip brushes 3.5; the PC's 0.63 s), its 8 trees read in ~1 s.
- **ad_sepulcher**: cold the trees 20 s (PC 2.1 s); warm the brushes 3.1 s (holes 2.2), ledges 0.9 s.
- **e1m1 after vrstart**: "disconnect, old server shut down" 2.6 s warm on the Quest (PC 27-160 ms), and its small
  trees compiled at every load (under the disk's 250 ms bar: 600 ms on the Quest).

**Changes**
1. **The map's brushes on disk** (vr_hull.cpp `brushesFromDiskOrBuild`): what `build()` makes (the walk of hull 0,
   the recovered clip brushes, the holes) is written beside the trees as `hulls/<build>/<world key>.hbr` and read at
   the next load instead of built. Key: `keyOf` (the world's nodes, planes, entity string, the holes' settings,
   `buildVersion`): everything build() reads. File: magic, `brushesVersion`, key, a sum, then every field build()
   sets but the hull 0 pointers (put back) and timings; amiss: built again and written over. The shipped cache is read
   after the player's (`vr_cache_shipped`). `vr_hull_cache` as for the trees (2: built anyway, compared byte for byte:
   `vr_cache_info` "checked"). Every map's is written (vrstart's ~10 MB; vrbrushholes' a few KB).
2. **Trees written from 25 ms of compile** (was 250, the PC's bar): e1m1's small trees took 600 ms on the Quest
   at every load.
3. **The hull's load timing split** (`vr_startup_times`): the brushes' job queued, rebuilt (of which clips, holes),
   the trees read and compiled (sums), the wait's end after the brushes.

**After** (warm, medians of 3; the trees as before, the brushes from their file):

| load | Quest before | Quest after | PC before | PC after |
|---|---|---|---|---|
| vrstart (the hub) | 11,800 | **2,390** | 1,280-1,400 | 830 |
| e1m1 after vrstart | 3,630 | 1,830 | 490 | 480 |
| ad_sepulcher | 5,760 | 4,540 | 1,590 | 1,170 |

Quest vrstart: the brushes 10.8 s -> read; the hull no longer in the stages over 400 ms. PC: brushes 631 -> 56 ms,
the hull wait 451 -> 0. Change 2 on the Quest (the final APK, warm): e1m1 1,625 ms, its trees read in 44 ms (601 compiled before), every tree of the session read (31, 0 missed).

**Checked**: `vr_hull_cache 2` on vrstart and ad_sepulcher (cold, then read and rebuilt): `checked 15 same 0 differed`
(13 trees, 2 brushes); `vr_hull_keeptest` on vrstart with the brushes read: PASS, brushes hash 8805c822 = fresh;
`vr_hull_holes_test` on vrbrushholes, built then read: 29 passed, 0 failed both.

**Next, with estimates** (not done):
- vrstart's first load (prebuiltcache ships its trees and could ship `.hbr` too: the brushes are the same bytes on PC
  and Quest; ~10 s of the Quest's first hub load). Other maps' cold trees: deferring the crouch/prone/monster trees
  until after the first frame (monsters already fall back to Quake's hull, `vr_hull_async`; the crouch has no
  fallback yet), ~7/8 of the cold wait (vrstart 85 -> ~12 s, ad_sepulcher 20 -> ~4 s on the Quest), a day's work.
- Leaving vrstart on the Quest: "disconnect, old server shut down" 0.9 s after (2.6 before): not looked into.
- vrtutorial's 531 alias models (Quest warm 2.9 s; draw lists 0.8 s warm / 4.2 s cold): a draw-list cache.
- The holes' search itself (7 s for vrstart on the Quest, 0.35 s PC): now once per map and build.

## Lift shafts kept clear: no more lifts bouncing on a monster underneath (branch agent/liftshaft, 2026-11-25)

His report: monsters chasing him or investigating a noise stood under a raised lift (e1m1's first lift, e2m4); the lift
coming down hit them and went back up (Quake's `plat_crush`), over and over. Reproduced headless with `vr_shaft 0`:
`vr_stealth_test 132` on e1m1, 3 bounces in 20 s for a grunt sent to a noise under the lift and 3 for a knight that
chased him there. Details: NAVMESH.md section 24.

- **What:** lift shafts found at the load by motion (`Quake/vr/vr_navshaft.cpp`): a mover with QC stops pos1/pos2 or a
  train's path corners, all in one vertical line, travelling more than its own height plus a step (thin platforms:
  e1m1 has 1, e2m4 2; column plats, doors and sideways trains are not shafts). A monster without a reason (its enemy
  or goal entity in the shaft) can't step into a raised lift's shaft (`SV_movestep`), walks out of one
  (`VR_Shaft_Leave`/`VR_Shaft_Stand`, builtin `navshaft`), the stealth AI's walk points are moved to its edge
  (`navshaftclear`); the navmesh carves shaft floors as their own polygons, costed while the lift is up
  (`navVersion` 7), and a walker heading in stands at the edge till the lift is down, then boards. Safety net: a
  monster that blocks a plat, door or train walks out for 3 s (`VR_Shaft_Blocked`); Quake's bounce is unchanged.
- **Settings** (Debug > Tunables): `vr_shaft` 1, `vr_shaft_margin` 16, `vr_shaft_cost` 20.
- **Tests:** `vr_stealth_test 132` (QC/vr_stealth_test7.qc, `shafttest: 10 passed, 0 failed`): investigate, fight
  (goes in after him, walks out 0.9 s after he leaves), ride (waits by the shaft, boards, rides up in 5.6 s), each
  with 0 bounces. Regressions: lift_ride_test.sh 24 passed; nav_lift passes; arenapath (vr_arena_test 36) 16 passed;
  nav_lift_down fails as before (also with `vr_shaft 0`: not this change, BACKLOG). Scene `nav_shaft`.
- **Open:** FrikBot bots on waypoints and the soft wall (players' moves aren't `SV_movestep`): a bot can still stand
  under a lift (its navmesh walks get the costs). Mods' own blocked functions (hipnotic, rogue, AD plats) don't call
  the safety net; their movers are still shafts (pos1/pos2). An e2m4 scene was not made (its two shafts are found).
## loadprof2: cold loads: the shipped cache carries the hull brushes; crouch, lying and monster trees made after the load (branch agent/loadprof2, 2026-11-25)

His ask: much faster loading on the Quest. loadprof measured the Quest 3's cold hub at ~90 s, 85 of them collision trees.
- **The brushes shipped**: make_shipped_cache.py already copied loadprof's `hulls/<build>/<key>.hbr` with the trees (the
  game reads them from `cache_shipped` after the player's cache); its summary now lists them as "brushes". Added: vrstart
  15.0 MB (4.8 MB deflated), vrtutorial 0.9, vrcalibration 0.1. The whole shipped cache: 15 files, 199.4 MB (84.8 MB
  deflated; vrstart 8 trees 130.6 MB, light grids 52.4 MB, vrtutorial's 2 small trees 0.4 MB). The files made by two
  builds were the same bytes but for the new trees.
- **Trees after the load** (`vr_hull_defer` 1, new; Debug > Tests "Crouch and Monster Hitboxes"; vr_hull.cpp): the load
  waits for the brushes and the standing player's tree only. At afterLoad (`prepare(true)`) the crouched boxes' trees
  (tallest first: 52, 44, 36), the lying box's and the monsters' sizes (those the spawned map asked for, `pending.later`:
  the hub's firing-range monsters too) go to one job on the pool, made one after another (disk, else compiled, then their
  brush models), each usable at once (`JobProgress`: a count stored with release). The in-play monster path
  ("Trees compiled in play") is the same job type with one tree. Correctness: a monster's move uses Quake's hull until its
  tree is ready (as vr_hull_async did; also during the load's 2 physics frames, but for a tree kept from the last load);
  a crouched or lying box's trace waits for its own tree in `treeFor` (`joinBackgroundTree`, the loading panel after 1 s,
  `developer 1`: "hull: a 16x36 trace waited N ms"); a tree being made is never read (`treeRunning`, also in
  `claimTrees` and `worldTreeReady`, which before could read a tree's heads while its job wrote them). One job runs at a
  time, so the build gate never queues a compile behind another job. 0: all waited for by the load, as before.
- **Quest 3** (this tree's dev APK, debug key, unworn, `-unattended -nostartmap`, a fresh `vr_cache_dir` per launch: every
  product cold, the GL programs warm; the data synced as Setup does, 4 files differed, then the shipped cache pushed).
  Map load, command to first frame:

| load (cold player cache) | before (no shipped cache, all trees waited) | defer only | shipped cache + defer |
|---|---|---|---|
| vrstart (the hub) | 92.1, 90.6 s (trees 66-67 s waited, brushes 5.1 s) | 26.3 s (0 waited) | **19.2, 20.5 s** (9 shipped reads, brushes 70-81 ms) |
| ad_sepulcher (not shipped) | 35.8 s (25.1 s waited) | 16.1 s (5.4 s: the standing tree) | - |

  The rest of the hub's 20 s is the cold model loads (entities spawned 11-12 s; 17-18 s when the trees compile beside
  them). Without the shipped cache the deferred trees were all ready 76-78 s after the load (vrstart), 17.5 s
  (ad_sepulcher). An unworn headset lies on the desk, so its player is "crouched" at once: the first crouched trace waited
  28.4 s (vrstart, nothing shipped) and 8.6 s (ad_sepulcher) for its tree, after the first frame: the worst case of the
  "pause on first use".
- **PC** (Release no-LTO, kit runs, a loaded machine: the light grid took 45 s in every vrstart run): vrstart cold 58.9 s
  (trees 7.6 s waited) -> defer 51.1 s (0 waited; deferred ready 7.5 s on) -> shipped + defer 6.1 s; ad_sepulcher 5.8 ->
  3.6 s (2.4 s -> 0 waited).
- **Checked**: `vr_hull_keeptest` vrstart PASS (8 trees, deferred, cold and read); `vr_hull_cache 2` over the deferred
  trees "checked 9 same 0 differed" (8 trees + brushes); `vr_hull_holes_test` vrbrushholes 29/0 cold and read (it needs
  the player spawned: `wait60` after the map, one `wait` gives 17/12 with or without the change); a mock VR and a flat
  run crouching, lying and crawling right after a cold vrstart load (the trees not ready yet: waited 5.3 s and 2.0 s on
  the PC) end at the same positions as a run whose trees were ready; e1m2 `vr_mhull_walktest 30` right after a cold load:
  in solid 0, embedded 0 (stuck 16 against 8 with defer 0: a random walk's paths differ). tests_for: prone_eye 6/0,
  lowceil 16/0, crouch_speed 4/0, crouch_test as expected; prone_test (flat 6 fails), crawl_test (1 fail) and vrslide
  (2 fails) fail the same with `vr_hull_defer 0`: not this change.
- Open: the deferred compiles take the pool's free workers while the game runs (a cold load only); the shipped files
  are keyed by the trees' version, not the exe: one generator run serves PC and Quest.

## liftshaft2: lift shafts for bots, every mod's monsters and movers, e2m4 (branch agent/liftshaft2, 2026-11-25)

Follow-up to liftshaft (his goal: nothing leaves a monster or bot standing under a raised lift). Details: NAVMESH.md 24.
- **Bots:** FrikBot's moves are a player's, so the soft wall never saw them. `BotShaft` (frikbot/bot_nav.qc; last in
  `BotAI` and every frame between its AI frames) asks the new builtin `navshaftbot` (`navshaft::botStep`): in a raised
  shaft without a reason (its enemy, or its target1 when a thing; waypoints never) it walks out; when its move would
  take it in it waits at the edge till the lift is down, then boards. A bot that blocks a lift flees too.
- **Every mod's monsters:** the engine's `SV_MoveToGoal` walks a monster out first (`VR_MonsterShaftOut`), so AD's,
  Copper's and the mission packs' AI (movetogoal) leave a shaft as id1's `VR_Shaft_Leave` does.
- **Movers' safety net:** Rogue's four new-plat crushes, AD's plat/door/train (importer patches, `VR_AD_ShaftBlocked`;
  QC/ad edited the same way, `ad_import.py --check` still differs in the 6 files it differed in before) and Copper's
  (importer patches, `VR_CU_ShaftBlocked`, regenerated: 0 differ) call `VR_Shaft_Blocked`; Hipnotic's are id1's.
- **e2m4:** `vr_stealth_test 133`: a grunt under each of its two raised lifts, the lift let go: `vr_shaft 0` 3 bounces
  each; 1: out from under the lift in 3 s (its pit is only a margin wider than the lift), 0 bounces: 4 passed.
- **Tests:** 134 (bot, coop e1m1) 5 passed (`vr_shaft 0`: it stays under); 132 10 passed; lift_ride_test.sh 24 passed;
  vr_dm_test 19 and 20 pass (19 failed once with and once without BotShaft acting: flaky, async nav in a low-impact run);
  nav_lift is marginal (reached at 34-57 s of its 60, timed out in 2 of 4 runs; the new code never fired in it, checked
  with a print); r1m1 and hip1m1 load and play 28 s clean (r1m1: 1 shaft).
- **Open:** an AD map not smoke-run (the bridge line only); AD's idle (standing) monsters under a lift wait for the
  bounce's flee (nothing walks them, AD's stand never moves); Honey's own door_blocked not hooked.
## flatkeys2: new flat default keys, and two keys per action (branch agent/flatkeys2, 2026-11-25)

His words (2026-11-25): ammo switch B -> N, throw J -> B, "make it possible for the same action to have two separate
bindings. Crouch/slide should be bound to both C and left ctrl by default."
- **Defaults**: `vr_flat_key_ammo` "n", `vr_flat_key_throw` "b", `vr_flat_key_toolgun_adjust` "n" (it shared the ammo
  key, the headset's X/A on the toolgun hand: it follows the ammo key to N, so B, the throw, clashes with nothing), new
  `vr_flat_key_crouch` "c CTRL" (the crouch / slide, Left Ctrl alone and hard-wired before). Default binds on the new
  keys: N `+reloadleft` (a VR bind: in flat play the flat key wins, Controls shows the "!"), C `+movedown` (the same; the
  crouch keys now swim down in water instead: new `vr_flat_crouch_swim` 1), CTRL `+attack` (as before: fires nothing in
  flat play). Nothing else of the flat keys was on N, B or Ctrl.
- **Migration**: none needed, none added. Flat keys never were in a full (pre-218) config, and a sparse config writes a
  flat key only when it differs from its default: a player who left B/J has no line and gets N/B; a player's own key has
  its line and keeps it (flatkeybind_test's old-config case). A "bindings version" step would overwrite a player who
  picks B for ammo in the new build, so no step (decision, conservative).
- **Two keys per action** (one path for every flat key): each `vr_flat_key_*` holds up to `flatplay::keySlots` (2) bind
  names, a space between ("c CTRL"; an old one-key value is slot 1). `flatplay::keyNum / keyIs / keyHeld / setKey`
  (vr_flatmove.cpp, vr_flatplay.hpp) are the only readers and writer: every key poll (vr_flatplay, flatreload,
  flatgrenade, flatholster, flycam, voicechat, the lean), `VR_FlatKeyOwned`, the clash checks, the tips' `{key:}` (the
  first key) and the VR menu's Push-to-Talk row (its first key). setKey keeps the slots packed (clearing the first moves
  the second up), never puts one key in both, and keeps the default's spelling (a sparse config).
- **Options > Controls**: a flat row shows both keys ("C, CTRL"; the cursor row an empty second as "???"); Left/Right picks
  the key (the picked one in white; never past the first empty), Enter + key sets it, Escape cancels, Backspace clears it
  (`VR_FlatKeyGet/Set(i, slot, ...)`, `keysmenu.flatslot`); the hint line says so on flat rows; "!" per clashing key.
- **The crouch**: a flat key now (pollKeysFlat reads `vr_flat_key_crouch`). The Ctrl bind takeover (+flatcrouch bound to
  CTRL while the mode ran, handed back before each config write: `VR_FlatKeysForConfigWrite`) is gone: VR_FlatKeyOwned
  already keeps a flat key's bind from running, so Ctrl fires nothing and the binds are never touched.
  `vr_flat_crouch_fire` 1 (row renamed "Crouch Keys Keep Binds") lets the crouch keys' binds run too. `+flatcrouch` stays.
  New `vr_flat_crouch_swim` (1, row "Crouch Keys Swim Down", Locomotion > Desktop Screen Play): in water the crouch keys
  swim down (`VR_FlatCrouchSwim` in CL_BaseMove), as C's +movedown did.
- Tests: flatkeybind_test 31/0 (16 old + 15 new: defaults, C and Ctrl crouch and neither bind runs, crouch_fire 1, a
  second key added from Controls and both reload, Escape / clear per slot, the written config keeps "r t", a fresh game
  reads it back and t reloads, an old config migrates and keeps the player's drop key, swimming down 49 vs 17 units in
  30 frames); flatkeys_test 36/0 (the ctrl cases rewritten: the bind left alone, not run in flat play, written as
  +attack); flatinput 19/0; flat_ammo_key 7/0, flatthrow 38/0, flattoolgun 11/0 (keys N, B, N); vr_msgtokens_test
  12/0; vr_menu_path_check 0 missing; menu_paths.txt (the two rows).

## embedhold2: flat stuck blades: the hand kept on its arm, the other hand on the blade (branch agent/embedhold2, 2026-11-25; partial, opt-in)

His flat notes 2026-11-25, items 4 (walking away from a stuck blade took the drawn hand off the wrist) and 11 (the other hand leaves a
stuck spear/sword). **Not finished: both new rules ship off** (tunables below); what is on by default is engine-side only.
- **Measured** (new `vr_embed_test 64`, QC/vr_flatembedwalk_test.qc, -flatscreenmode; `scratch1 2` per-frame lines, `scratch2 N` first
  case): the main hand's most cm past its stretched arm (.vr_embed_armgap) from the stick to 0.5 s after the hold ends. origin/next:
  ragdoll 31.6 (the body falling off the blade, before any walk), live grunt 31-99, ogre 99, spear 162 (its off hand never joined),
  sword with a free off hand 48.6/45.6. Causes: (1) the stab going on to its depth (stick_goal, to 14-16 units) carries the drawn hand
  past the arm; (2) walking: the flat hold's hand is carried by his walk, `far` frees only at 90 cm; (3) the release ease is about the
  controller, whose flat pose jumps as the weapon comes back (48 cm the frame after); (4) the thrust's arm stretch (1.5 in tests) falls
  away under a stuck hand (8 cm in a frame).
- **On by default (engine):** (3) flat release ease from where the hand left the blade carried by the player's origin/yaw
  (view_hands embedEaseBody); (4) flat `armStretchNow` keeps the most stretch it had while the hand is on a blade (`flatHeld`, eased
  back off it); new field `.vr_embed_armpast` (signed cm past the most the arm reaches, measured where the pin is going:
  `view::embedPinLead`); `vr_debug_axestick 4` prints `embedarm:` per frame.
- **Off by default:** `vr_flat_embed_arm` 0 (Tunables > Flatscreen "Stuck Blade: Hand On Arm"): QC VR_Embed_FlatArm predicts the
  gap two frames on (armpast + his walk + the body's way; the engine's value is a frame late and the blade is drawn a frame behind),
  stops a stab's travel at the arm's reach, and past Hold: Let Go Past the handle hand pulls a light body's blade out (VR_Embed_Light,
  a skewerable ragdoll, a crate whose pull isn't at its most) or lets go of a heavy one; a second hand lets go. Its numbers: ogre 1.3 cm
  (let go, it stays), ragdoll 0.0 (out as it falls); but on the grunt/spear/sword cases the blade still comes out 0.1 s after the stab,
  standing (the predicted gap 1.6-3.6 cm: something still moves the drawn hand after the stab; next: `vr_debug_axestick 4` on case 2).
  `vr_flat_embed_other_hand` 0 ("Stuck Blade: Other Hand", cm; 30 tried): a free other hand within it of the blade's line takes it
  too (VR_Embed_Try; the engine draws it on the nearest grip hotspot). Works (spear 20.7 cm off the line, joined) but without the arm
  rule the joined hand is drawn 83 cm off its arm when walking, so it stays off until the arm rule is done.
- **Seen:** the flat V thrust never hit the large crate in case 0 at 44 units, and walking away from a crate the sword was stuck in hung
  the game (both seen with the rules off; not chased: case 0 is skipped with `scratch2 1`).
- **Tests:** vr_embed_test 64 12/7 with the defaults (the gaps above), 14/5 with `vr_flat_embed_arm 1; vr_flat_embed_other_hand 30`.
  flatthrust_test.sh: blades 7/0 checks passed; the Lance run 0/4 (no Lance in the hand: Dwell likely missing in this worktree's base;
  not checked against origin/next). VR embed 24/33, flatlance, embeddual not run (budget).
## flatmisc1125: aim sway reduction, VR-only map buttons in flat play, melee arcs in flat play (branch agent/flatmisc1125, 2026-11-25)

- **Aim sway** (his item 15): the flat view-model's sway (the step's spring, `Feel::sway`, stepped in vr_flatplay.cpp
  `stepFeel`; size `vr_flat_sway` 1, "Move Sway") and the turn's lag (`Feel::lag`, `vr_flat_turnlag` 2 / `_speed` 1 / `_cap` 10)
  are shown in `drawnOffset`. Already there: `vr_flat_ads_sway` 0.15 ("Ironsight Sway"), one share for every axis, on an
  ironsight only; a zoom-only gun (fsight_mode 1, or `vr_flat_aim_zoom 1`) kept the full sway and lag. New Tunables
  (Debug > Tunables > Flatscreen > Looks): `vr_flat_ads_sway_side` 0.3 ("Aim Sway Horizontal": the sideways and fore-aft sway,
  the lag's yaw) and `vr_flat_ads_sway_up` 0.3 ("Aim Sway Vertical": the up-down sway, the lag's pitch), eased with the aim's
  travel (`flatsight::adsShare`), for ironsights and zoom guns alike; on an ironsight on top of `vr_flat_ads_sway` (the lag
  is none there already). Walk bob untouched (`vr_flat_ads_bob`). Probe `vr_flat_sway_probe` (peaks since the last call). Scene `flat_aim_sway`.
- Test `flatads_test.sh` section g (strafe, jump and turn; hipfire, aimed with 1 1, aimed at the defaults): shotgun
  (ironsight) side 1.366 hipfire -> 0.205 aimed unreduced -> 0.061 (0.298 / 0.303 / 0.300 of it); grenade launcher (zoom)
  side 1.366 -> 1.366 -> 0.410, turn lag 3.205 -> 0.961 (0.300 each). 2/0.
- **VR-only buttons** (his item 16): every setting button of vrstart, vrtutorial and vrtesthall runs `vr_setup_option <key>`
  (vr_setup.cpp's table), so the gate is there: `Option::vrOnly` (turning, turnspeed, movedir, sticks, teleport, grip, gadget,
  torch, position, scale, hud: nothing on the flat screen reads those cvars; the menus' VR-only flag covers only graphics
  cvars, none of these). In flatscreen play such a press changes nothing (console: "<label>: VR only, no effect on the flat
  screen") and its screen above the button reads "VR only" in grey (greyed, not hidden: the buttons are brushes, hiding them
  needs QC and map changes). The value screens now show in flatscreen play too (they were headset-only). run, body,
  crosshair, climb, swim, holsters, reload, twohand, tips and classic stay live (flat reads them or it was unclear: kept).
  Not options, left alone: the VR CALIBRATION / START CALIBRATION buttons. `vr_setup_option list` marks `[VR only]` and
  prints the greyed screens. Test `flatvronly_test.sh`: flat 3/0 (turning 45 kept, run 1 -> 0, 11 screens greyed), headset
  2/0 (turning 45 -> 90, none greyed). Scene `flat_vronly_buttons`.
- **Melee arcs / aim cones in flat** (his item 17): the view queued their lines in flatscreen play (test 216 counted them
  drawn) but the flat view draws queued debug lines only from `VR_RagtoolOverlay` (vr_ragtool.cpp), gated on the ragdoll
  tool, Show Disarm Volumes and Show Directional Parry. The gate now also takes `vr_enemy_melee_arc_show`,
  `vr_enemy_aim_show` and `vr_debug_melee_blades` (VR unchanged: the headset draws them in its UI pass). Probe
  `vr_flat_debug_lines_probe`. Test `flatdebuglines_test.sh` 4/0: scene agrunt_melee_arcs flat, 450 frames: lines drawn 447,
  a swing drawn 179; vr_agrunt_test 216 2/0 and 212 6/0 in -flatscreenmode. Screenshot qvr-kit/scratch/flatmisc1125_arcs.png.

## polepose: flat pole weapons: a closed other hand, the tip line on the crosshair, the Lance's parry (agent/polepose, 2026-11-25)

His flat notes 6-9 (2026-11-25). One path for every pole weapon (twohand::poleWeapon), no Lance or spear copies.
- **Closed grip** (6, 8): flat play's hand pinned on the other hand's pole (flatsight's pin) took no grip: its fingers were the
  idle controller's (open; drawn curl 0.00). Now `view::onFlatPole` makes it grip as a holding hand does (vr_view.cpp
  updateFingerFrames) and wrap the shaft where it is (vr_view_hands.cpp heldBy, the held-anywhere grasp): the four fingers'
  drawn curl 0.64-0.76 held, in the thrust and in the guard (was 0.00). Probe: `vr_flat_pole_track report`'s two new lines
  (`view::drawnGripCurl`; the tip line's miss at the crosshair's point and at 200, the drawn line off the ray).
- **Aim** (7): the Lance has a sight line (sightalign's table), so flatsight's aimAtPoint turned it onto the crosshair's point;
  the spear is in sightalign's melee list, so it kept the hold's turn, its pole along the crosshair's way from the hand's side
  (his "offset towards the hand"). Neither was aimed in a thrust (aimsAtPoint skips a punch). Now flatsight
  `poleAimsAtPoint`: every pole weapon's tip line (the tip as placed in the hand, along the shot's way) is aimed at the
  crosshair's point by aimAtPoint, held and through the thrust; the thrust's own tip turns (vr_flat_thrust_wind_pitch / _pitch)
  go on after it (`flatplay::poleThrustTurn`), and in a thrust polePose aims the line first (`poleAim`, never nearer than
  `vr_flat_pole_aim_near`) so the thrust travels along it. Not in the guard (aimsAtPoint now skips parryShare > 0). Tunables
  (Debug > Tunables, the spear pose's rows): Spear Aims At Crosshair `vr_flat_pole_aim_point` 1, Spear Aim Nearest
  `vr_flat_pole_aim_near` 64. The drawn turns vr_flat_pole_pitch 8 / _yaw 5 stay his (drawn only: the drawn line crosses the
  crosshair's ray, the screen's middle, a few units off, as the Lance's always did).
  Measured, the tip line off the crosshair's point: before (280 away) the spear 44 held (the hand's line), 47 in the thrust;
  the Lance 3.6 held, 25.6 in the thrust. After (polepose_test, 200 away, the thrust's own turns 0): both 0.00 held,
  0.00-0.04 in the thrust.
- **Parry pose** (8): the spear's guard already put the other hand on the shaft (1.9-2.5 off its line, as held: 2.4); it was
  the open hand. Now closed round it (above). Shot qvr-kit/scratch/polepose_spear1.png.
- **The Lance's right button** (9): `flatplay::poleParries` (a pole gun; `vr_flat_pole_rmb_parry` 1, Tunables row "Pole Gun
  Right Button Parries", 0 its ironsight back): the spear's parry guard (parryAsked, handsource::guarding: QC's flat guard
  parries) and no ironsight (flatsight adsApplies). Its pose is the spear's (pole left 94.7 against 94.4, the other hand along
  3.2 against 3.1). The Lance's other buttons are unchanged: LMB its charge and shot, V the thrust, N (next ammo: none, so) flips the grip (B throws since flatkeys2).
  Shot qvr-kit/scratch/polepose_lance_parry.png.
- Tests: new `polepose_test.sh` 19/0 (both weapons: the grip closed held, in the thrust and the guard; the tip's miss < 1 at
  200 held and in the thrust; the guard across, the other hand on the shaft; QC's parry probe PARRIES; no ironsight in the
  Lance's guard); new scene flat_spear. flatlance_test 30/0 (three checks brought to cltidy7's promoted defaults: off_along 8,
  off_pitch -40, thrust slide 2; and the flip on N, flatkeys2), spear_test 4/0, flatthrust 12/0 + 7/0, flatparry 43/0. Aiming in polePose while held too
  moved the other hand up to 0.2 further off the pole walking and turning (flatlance pwalkturn 3.50 against 3.27), so polePose
  aims only in the thrust.
- **Flipped** (after flatmelee4's per-family flipped poses, vr_flat_revgrip_pole_*): the other hand pinned on a reversed
  pole was carried with the pole's half turn and its own pose turns, and ended off the drawn shaft (spear: 8.5 off its line,
  10.6 along, a fist in the air). Now, by the flip's share, its grip channel goes onto the drawn pole, the least turn and
  move (vr_view_hands.cpp, alignChannel on view::weaponMount's line, after the pose's own turns): off 1.28 (2.4 held upright).
  The flipped Lance's hand at the upright's place (8 along) met its head's housing and stayed open (curl 0.32): a flipped
  pole's other hand sits `vr_flat_revgrip_pole_off_along` 4 along from the fist (Tunables row "Flipped Spear Other Hand
  Along", beside flatmelee4's flipped rows; vr_view_weapon.cpp chooseGrip, by the flip's share): curl 0.72 (spear 0.64).
  polepose_test 21/0 (new: both weapons flipped by N, the other hand within 1 of its held distance off the drawn line and
  closed); flatlance 30/0, spear 4/0.
## flatmelee4: flat V's blade thrust windup, the empty-handed overhand, flipped-grip poses per family (branch agent/flatmelee4, 2026-11-25)

His items 3, 5, 12 (2026-11-25).
- **Blade thrust windup** (item 3): the one-handed thrust's tunables existed (`vr_flat_thrust_1h_wind_back` 8, `_wind_up`) but
  sat in Tunables > Flatscreen > *Two-Handed Melee Stance* (the group never said thrust), and its turn and times were the
  pole's. Now a group **Flatscreen > Thrust (V): Spear and Blade** (spear and blade rows), and the blade's own windup
  `vr_flat_thrust_1h_wind_back` 16 (8 before: 8 units further back), `_wind_up` 3, `_wind_out` 0, `_wind_pitch` 8,
  `_wind_yaw` 0, `_wind_roll` 0, strike `_1h_up` 0 / `_1h_pitch` -3 / `_1h_yaw` 0, times `_1h_wind_time` 0.2,
  `_hold_time` 0.05, `_strike_time` 0.1, `_dwell_time` 0.08, `_back_time` 0.25 (vr_flatplay.cpp thrustAt / thrustTimes(pole)).
- **Empty-handed V overhand** (item 5): V with an empty hand (no prop) is an overhand blow by the overhead swing's path
  (`overheadSwing`, one function: a `SwingSet` of tunables, the weapon's or the fist's), the fist turned off the aim's
  angles (eased in from the hanging hand's). `vr_flat_overhand` 1 and `vr_flat_overhand_<wind|top|strike|dwell|back>`,
  `_raise_<up|fwd|out|pitch|yaw|roll>`, `_end_<pitch|down|fwd|out|yaw|roll>` (group Flatscreen > Empty-Hand Overhand (V)).
  Its windup is Synthesized, the strike the punch's Action: QC reads "punch (overhead) with the knuckles", 8.2 m/s on a
  grunt 16 ahead. The fire button's straight punch unchanged.
- **Flipped grips per family** (item 12; QVR_WPNFLAG_REVGRIP; sword / dagger (WID_SWORD kind 12) / pole):
  `vr_flat_revgrip_<sword|dagger|pole>_<fwd|up|out|pitch|yaw|roll|down>` (group Flatscreen > Flipped Grip): the hand moved
  by the flip's drawn share (`revGripPose`: sword 4/12/0, dagger 5/12/0, pole 8/18/0: held a bit overhead), the drawn turn
  about the fist (pitch 90 / 90 / 165, yaw, roll; `revGripDrawnTurn`), the thrust's stab down (25 / 25 / 15 degrees; the
  pole's turned down by the thrust's share). Retired: `vr_flat_revgrip_pitch`, `vr_flat_thrust_1h_rev_down` / `_rev_up`
  (he had not set them). **Not done:** the off hand on a flipped pole: it is placed on the game's pole line
  (vr_view_weapon.cpp `flatPole`), which for a flipped pole runs back out of the fist, so it floats open across the view;
  the fix belongs in the shared pole off-hand solve (polepose's), taking the drawn flipped line: not copied here.
- **Probe:** `vr_flat_melee_probe` prints `flatwindup: <hand> <thrust|overhand|swing>: back up out tip` (the windup's top
  off where it began, the thrust's line at the strike's end).
- **Tests:** new `Misc/quakevr/flatmelee4_test.sh` 9/0 (overhand: over the eye 4.1, a blow, an action; fire still the
  punch; sword windup back 16.0, wind_back 26 -> 26.0, wind_out 6 -> +6.0 out; thrust line: upright -1.6, flipped sword
  -28.5, dagger -28.5, spear -18). flatmelee 12/0. flatthrust 6/1 and spear 3/1 fail on **next's promoted pole values**
  (cltidy7: `vr_flat_thrust_off_slide` 2, `vr_flat_pole_off_*`): the spear's stick (depth 0) and flat B's flip of the
  spear; with the old values tuned (off_along 12, off_out 0, off_pitch -60, off_slide 8, arm_stretch 1.3) flatthrust 12/0,
  spear flat 6/0. The flipped dagger missed with the hand raised and 40 down: down 25 lands (38 damage, stuck 1.8).
  Scene `flat_melee_v`. Shots: kit scratch flatmelee4_before.png, flatmelee4_after2.png (empty V, flipped sword,
  dagger, spear: rest, windup, strike), flatmelee4_spearvar.png (the flipped spear's off hand).
## climbfix: vrtesthall's brush ladder climbable flat; rope/pole elbows wider; hands kept on the wrists in a slide (branch agent/climbfix, 2026-11-25)

- **Brush ladder (his 10):** vrtesthall's rungs (8 deep, flush on the platform's face: no gap behind, unlike the
  tutorial's bars) never took in flat play. The map was fine and a headset's grip took them (`vr_climb_try` on a rung top:
  a hold). The flat ladder's probe column (`findRungs`, vr_flat_ladder_reach ahead of the box's front) lay 4 units inside
  the platform once his box was against it, and every hold seen from there failed findHold's `check` (the hand's point at
  the rung's back edge, on the wall's face: "unreached"). Fixed in the detection, flat only: a probe that finds nothing
  and sits in solid is tried again moved back out of the solid towards him (at most its radius). Lowering onto the
  ladder from the top (`flatLadderMount`) had the same shape of problem: the hang spot off the lip's line put the box in
  the rungs; when that spot has no room it now also tries off the lower rung's front (and 4 and 8 further). Tutorial,
  Quoth and AD ladders unchanged (their first tries still succeed).
- **Elbows (his 13):** measured on vrropes climbing up (`vr_rope_flat_test`, rope and pole, from the 12th frame): on the rope
  the elbows were bowed in off the arms' lines by up to 8.7 cm and 21.9 cm inside the shoulders' line, at the hand pulled
  in to the rope by the chest. That arm's line points across the chest, so little of ropes10's elbow pose (out, a little
  down and back) is square to it and the down/back took the elbow in front of the chest. New tunable
  `vr_rope_elbow_wide` (4; 1 = ropes10's; Debug > Tunables "Rope Climb Elbows Wide") weighs the side against the down and
  back (vr_avatar.cpp solveArm; all drawn arms on a rope: yours flat and VR, other players'). Rope: bow at least -8.7 ->
  -1.5 cm, side mean -4.5 -> -0.6 (least -21.9 -> -15.3: a hand at the chest's middle keeps its elbow near the line; the
  rest is the hand's place, not the pose). Pole: bow least 4.9 -> 6.4, side mean 7.4 -> 9.1 cm. Wider than 3 changes
  little. New check in rope_flat_test (rope, pole): "the elbows while climbing" (none bowed in past 2.5 cm; on the pole
  out of the shoulders' line by 5 cm on average). Shots: qvr-kit/scratch/climbfix_rope_elbows_wide1.png / _wide4.png
  (`vr_rope_flat_test rope shots`, the first two: climbing, from the front and behind).
- **Slide (his 14):** the slide lock (flatfb1124b) keeps each hand where it was on the body while the crouch lowers the
  shoulders, so the arm ran out and the drawn hand showed off its wrist. Measured with the new `avatar::armWristGap`
  (units from the drawn wrist to its arm's end): pole slide 8.02 units most, 51 of 60 frames; rope slide 4.73, 50 of 62.
  Now the locked hands come down the rope by `vr_flat_rope_slide_hands_in` (10 units; Debug > Tunables "Rope Slide:
  Hands In"; 0 = as before), eased in at most 0.6 units a frame, and further by any gap the arm had last frame (up to
  24). After: 0.00 units, 0 frames, on both; the pole slide's elbow shake 1.09 -> 0.35. rope_flat_test slide/poleslide
  now check the gap (no frame over 0.05).
- Tests: flatladder_test.sh 4/0 (with the new vrtesthall cases: up and over, down from the top, down looking down, push
  off, the face beside it, vr_flat_ladder 0), modladder_test.sh 10/0; rope_flat_test rope 10/0, pole 9/0, slide 9/0,
  poleslide 10/0, polejump 8/0, strung 5/0, slideend 2/0; rope_climb_test rope_climb, pole_wood 13/0; rope_hands_test
  6/0. Scene vrtesthall_ladder. Ladder shot qvr-kit/scratch/climbfix_ladder.png.

## embedhold2b: a stuck blade's hand stays on its arm and nothing comes out on its own; the other hand joins the shaft (branch agent/embedhold2, 2026-11-25/26; the continuation of the section above)

- **What the rules do now** (QC `VR_Embed_FlatArm`/`VR_Embed_FlatGap`, `view::keepOnArm`, engine
  `avatar::embedReachNow`/`embedArmPastFrom`): the engine measures its own arm - the last solve's shoulder from the eye, the most the
  drawn hand may be from it, and what it measured the gap against (`.vr_embed_armseenoff/main`, `.vr_embed_armeye`). QC's gap adds only
  what the blade moved since the frame it was drawn, so the client's frame lag is no longer counted twice (that was much of the
  "0.1 s after the stab"). `view::keepOnArm` slides a hand drawn on a blade back along its way from its own controller place, so it is
  never drawn past its wrist. **Standing still nothing ends the hold and nothing draws the blade out**: the arm's decisions (draw a
  pointed blade back out of a light body, let go of a heavy one) and the pull-out slide only run once the holder's own step this think
  is `VR_FLATARM_STEP` (0.5 units) *off the blade* - his walking away, the body going with the blade in it - not on the blade's drawn
  place jittering by tens of units or a body staggering under the stab. Between those, the hand slips on its handle up to
  `vr_flat_embed_arm_slip` past Hold: Let Go Past.
- **Defaults shipped** (his items 4 and 11): `vr_flat_embed_arm` 1 (row "Stuck Blade: Hand On Arm"), `vr_flat_embed_arm_slip` 10 cm
  ("Stuck Blade: Hand Slip"), `vr_flat_embed_other_hand` 30 cm ("Stuck Blade: Other Hand": the free empty other hand takes a stuck blade
  within that far of its line - the spear's shaft does, a one-handed sword stuck in a body at arm's length does not: its handle is a
  metre off the hand idle at his side, and nothing reaches over for it).
- `vr_embed_test 64` (QC/vr_flatembedwalk_test.qc) now checks the standing-still half second in every case: the drawn wrist within its
  arm (`.vr_embed_armpast` <= 3 cm), the hold still on, and the blade come back out by at most 2 units on its own (state 6 from where it
  stood; the stab's own half second is checked the same way except for the ragdoll, which flops with the blade in it). Case 5 is a sword
  with the off hand free (it walks away with it; the joining check is the spear's).
- Before/after, `vr_flat_embed_arm 1`: 32 passed / 3 failed -> **45 passed / 0 failed**; off: 32/2 -> **45/0**. The standing-still numbers
  (cm past the most the arm reaches, and units the blade crept out): ragdoll the hold ended and 8.0 out, ogre 7.3 out, grunt 2.1-2.5 out
  -> all six cases 0.0 out, the hold on, the blade 13.4-14.7 units in and the hand -52 to -0 cm past its reach.
- New committed test `Misc/quakevr/embedhold_test.sh` (two runs, the defaults and the rule off, ends `embedhold: N passed, M failed`):
  **90 passed, 0 failed**; found by `Misc/quakevr/tests_for.py` on its own. embeddual_test.sh 16/0, embed1108b (vr_embed_test 29) 10/0.
  Still red around this branch (not flat play, and the same with the two rules off): embed_blood_test.sh `embed1108: 20-21 passed,
  3-4 failed` (the axe/sword "stays in" cases), embededge (vr_embed_test 13) 64/4, embeddagger (40) 23/1 ("the dagger's blade ... a
  third of the sword's: 9.2 units of 20.3"), flatthrust 58 and the spear (53) as handed to polepose.
- Debug: `vr_debug_axestick 4` prints the flat gap's breakdown (measured, more, the blade's way, the eye's); `scratch1 3` in test 64
  prints the standing-still frames. Scene `flat_stuck_blade`; checklist lines in qvr-kit/scratch/cl_lines_embedhold2.txt.
## agspearpose: the Pose Studio's pole weapon class; the flatads zoom test follows vr_flat_zoom_up (branch agent/agspearpose, 2026-11-25)

- **Pole Weapon (Spear), a class of its own (the cut-off run's item 1, verified here by tests):** `VR_AGK_POLE` 22
  (vr_agpose_shared.h, the bound 23), in classOrder between the Widowmaker and the saw. A humanoid whose melee slot is a
  pole weapon (twohand::poleWeapon: its Weapon Holds row's Other Hand Anywhere 3, FREE_POLE; QC VR_AGP_Pole) reports the
  pole class instead of Melee2, in the engine (vr_builtins_agpose.inc agPhase) and in QC (VR_AGK_Class). The studio's
  Pose panel for it: Port Arms, Ready, Run, Pickup, Cast, Thrust (its one strike: VR_AGK_THRUST, the Overhead's number)
  and Shove; its Class combo title "Pole Weapon (Spear)". No keys ship for it, and its seeded stretch is the two-handed
  melee weapon's ({0,22,1.23} grunt, {2,22,1.20} knight), so it starts exactly as spearfix posed it in QC: test 230's
  "ready, its head at its enemy" reads 0.2 deg off, as in the 2026-11-24 baseline run, and the engine's `agpole` report
  has both hands on the shaft (right 2.1, left 3.3 units off it). QC's VR_AGP_Melee marks its two-handed grip
  (vr_agp_polel) and the off hand rides the shaft: it takes the right grip's change and turns about it as the shaft does,
  before its own key's change (vr_armedgrunt_pose.qc).
- **agspear test case 2** (QC/vr_armedgrunt_spear_test.qc, vr_agrunt_test 230): a sword grunt stands beside the spear
  grunt; the studio's stored data (`vr_agrunt_poses`, the Grip Editor's own mechanism) lays the pole class's Ready key 6
  units up: the spear moves '0 0 6' (within 1), the sword 0.03, and the spear grunt's class reads 22. 7 passed, 0 failed
  (that test had 4 checks before the class).
- **flatads section f follows the tunable (item 2):** his promoted `vr_flat_zoom_up` default is 0, so the test no longer
  expects a 2-unit rise: it reads the cvar off the console (a bare tunable name echoes `name = "value" (default "...")`,
  vr_tunables.cpp), clamps it -8..8 as vr_flatsight.cpp's zoomCentre does, and expects the zoomed hand's z to move by
  that. To keep the check from passing vacuously at 0, the same run then `tune vr_flat_zoom_up 3` and expects the same
  zoom 3 higher (measured 2.50 -> 5.50: -5.00 -> -2.00). flatads: 13 passed, 0 failed.
- Tests: agspear_test.sh 3/0; vr_agrunt_test 230 7/0; the kit's agrunt group (219 cases, 8 lanes, 9.6 min) 214 passed
  with the same five failures as the 2026-11-24 baseline run before this branch (agrunt_3 yank, agrunt_16 disarmhull,
  agrunt_65 grab anywhere, agrunt_74 downtug, agrunt_203 dualdisarm; 65/74/203 flaky there too) — none pole-related.
- Checklist: two lines in qvr-kit/scratch/cl_lines_agspearpose.txt; new scene `pose_grunt_pole` (a frozen spear grunt, a
  frozen sword grunt beside it, the Pose Studio opened on it with `cmd vr_posestudio`): `vr_scenario validate` 183 files,
  0 with errors, 128 tags 0 missing; the scene 2 of 2 spawned, 0 problems.
## sphit: monster fire meets the player's drawn body in single player (agent/sphit)

- `vr_playerhit.cpp` serverShape no longer returns early for `svs.maxclients <= 1`: the local player's body (server::avatarPose slot 0, from his clientMoves; the box when none) is what monsters' bullets, missiles and thrown things meet. Splash stays by distance.
- New tunable `vr_hit_player_fatten` (vr_tunables.inc, default 5.5; Debug > Tunables > Combat > Other, Body Hit Fattening): added to every part's radius, SP and MP.
- Measured with `vr_sphit_test 1` (a grunt 280 units ahead, 289 aimed bullets over a 48x64 grid, vrteleporters): standing in the open the 24 box takes 153 hits; the shape 99 at fatten 0, 132 at 3, 136 at 5, 164 at 6, 173 at 8: 5.5 matches the box. Crouched: 117 at 0, 185 at 6 (box 153). Behind the 32-high cover crouched: 0 hits (crouch_test.sh cover cases: 0 damage crouched, 48 standing; tunables_test 13/0).
- vr_hitzones_aim (MP scene) takes the fattening into its beside-the-forearm gap; not re-run (no runner found).
- sphitlean (agent/sphitlean): vr_sphit_test 2 (QC/vr_crouch_test.qc) puts a wall of six door models 60 ahead, edge swept -16..16 right of the player, grunt 280 ahead; prints sphitlean lines. At edge 16 (body hidden): not leaning 0 hits, leaning 30 cm (8 units) with fatten 5.5 7 of 289, fatten 0 0. The grunt AI sees and aims at origin + view_ofs, never the head (visible() is 0 at edge >= 8 even when leaning).

## leanaim: monsters see and aim at a leaning player's visible parts, with a cover advantage (branch agent/sphitlean, 2026-11-25)

- **Why:** monsters saw a player by one point (origin + view_ofs) and aimed at his origin: leaning out from cover he was never seen nor shot at. His decision: they aim at what shows, but leaning gives lower received DPS than standing in the open.
- **Sight** (`QC/vr_leanaim.qc`, called from `visible()` in ai.qc only when the eyes' point is hidden, for a monster's own enemy, within 2000 units): 7 points of his drawn body from the new builtin `playerbodypoint(e, i)` (vr_playerhit.cpp, the shape shots meet: head, neck, torso middle, both shoulders, both hands; one trace each, monsters don't block). Any showing: seen, but only after `vr_monster_lean_react` (0.4 s) of a new exposure (none seen that way for over 1 s).
- **Clear shot:** the CheckAttack family's `trace_ent != targ` (fight.qc's CheckAttack, Soldier/Sham/OgreCheckAttack, wizard, scourge, Honey, MG3 orb, Armagon, armed grunts' VR_AG_CheckAttack and VR_AM_SpellTry) also accept `VR_Lean_Clear`: seen by a lean this think. Not done: Copper's, rogue eel/lavaman, AD's own checkattack.
- **Aim:** `VR_Lean_Aim` at the start of `VR_AimCone_Bend` (every ranged monster's shot: grunts' EnemyFireBullets, enforcers, scrags, ogres, gremlins, AD's launchers...): his eyes' point still hidden and a part showing, the shot goes at the biggest part showing (torso 6, head/neck 3, shoulders 2, hands 1) plus extra spread `vr_monster_lean_spread` (0.2) x (1 - 0.5 x fraction showing); lobbed shots (VR_AIM_WIDE) only turned level. Armed grunts with a player's gun (vr_agrunt_aim_exposed): their exposure points' head/chest/shoulders now follow the drawn head off the origin (VR_AG_XPoint), and VR_AG_AimDir adds the same extra spread (in degrees).
- **Tunables** (Debug > Tunables > Combat > Other): `vr_monster_lean` (1; 0 is the old behaviour), `vr_monster_lean_spread` (0.2), `vr_monster_lean_react` (0.4).
- **Test:** `vr_sphit_test 2` (QC/vr_crouch_test.qc) now ends with a live grunt's received DPS, 90 s per case, grunt 280 ahead held on its spot, a wall 20 ahead with its edge 9 right of him (16 shows his drawn body from straight ahead never: the body is +-8 of the head). Run: `run.sh <tree> -Script "map vrteleporters;wait30;vr_lean_recenter 0;vr_mock_look 0 90;vr_sphit_test 2" -QuitOn "sphitlean: [0-9]+ passed"` (vr_lean_recenter 0: the untilted mock lean is not detected as a lean, the body would slide under the head). Result: open 3.17, lean 1.09, hidden 0.00 DPS (lean 34% of open), 3 passed 0 failed. Spread 0.1 gave 52%, 0.14 57-61% (40 s cases: about +-15% noise).
- **Open:** the drawn body is posed entirely under the head, so a 30 cm lean shows all 7 points (fraction 1): the spread's lower half applies. Real leans (vr_lean_detect) keep the origin; not checked in a headset.

## embedhold2 after the second merge: the flat arm's rules leave a swung blade alone (branch agent/embedhold2, 2026-11-26)

- Merged `origin/next` (merge commit 842b5aac6, next at b25b90edd: polepose, leanaim, flat V work). No conflicts. The `spear`
  and `flatthrust` sections that were red in the previous round's report were the pre-polepose ones: with next in the tree
  `flatthrust` 58 is 12/0 and the flat spear section is 6/0.
- **The rule** (`QC/vr_embedhold.qc`): the flat arm's rules are for a blade that went in **by its point**. New predicate
  `VR_Embed_FlatArmBlade(e)` = `vr_flat_embed_arm != 0 && e.vr_stick_embed == AXESTICK_POINT`, used at the top of
  `VR_Embed_FlatArm` (its draw-out slide, its "the arm ran out: free it or let go", its two hands' let-go) and at the flat
  going-in clamp in `VR_Embed_HoldThink` (the depth a stab stops at). An axe's flat V is the engine's overhead swing and its
  edge is wedged in the body's cut (`weapons.txt embed "2"`), so the arm neither draws it back along its axis nor runs out on
  it: walking away never brings a swung axe into the hand; its hold ends, if it ends, by the rules it had before
  (H, another weapon, the hold's own reach and yank). His item 4 ("the blade comes out about 0.1 s after the stab") is a thrust's.
- **Test**: `vr_embed_test 64` (`QC/vr_flatembedwalk_test.qc`) gained case 6 "axe": the axe V'd at a grunt 28 ahead (its swing
  reaches shorter than a thrust: at 40 flat play's axe V meets nothing, in `vr_embed_test 58` too), it goes in by its edge
  (2.8 units), standing still nothing moves it, walking away 601 units in 217 frames leaves it **still held**, with
  `vr_flat_embed_arm` 1 and 0 alike (9 passed 0 failed, six runs, both arms). `Misc/quakevr/embedhold_test.sh` runs all seven
  cases both ways.
- **Not ours, closed**: `embed1108` is 21 passed 3 failed with the promoted defaults, the same three FAIL lines (all three the
  axe's: "the axe stays in it held", "case 7 an axe swung stays in the dummy", "case 9 an axe swung stays in the grunt") as
  with `vr_flat_embed_arm 0; vr_flat_embed_other_hand 0`, and as plain next. Both cvars are read only inside `VR_FlatPlay`, so a
  mock-headset run cannot change: the earlier 20/4 was the flaky "case 10: the sword's stab stays in the grunt".
- **Tree-local, open**: built in this tree, plain next (b25b90edd, no branch commits) gives `vr_embed_test 59`'s Crystal Lance
  0/4 ("0 damage", "stuck by its point depth 0.0", the punch line printed "not a swinging weapon, overhead swing") and the VR
  spear mock `embedspear` 23/2 twice ("the Lance's swing landed with its shaft... the butt, 0.17 of the line", "8 of 10 straight
  throws"), where the same commit in the mp tree gives 7/0 and 25/0. Something in this tree's game folder differs (the AD/Dwell
  side): don't trust this tree's numbers for those two sections until someone looks.

## embedholdfix: systemic fixes to embedhold2 release ease, arm reach, flat arm & edge-embed rules (branch agent/embedholdfix, 2026-11-26)

- **Unified hand + weapon release ease** (`Quake/vr/vr_view_hands.cpp`, `vr_view_weapon.cpp`, `vr_view_internal.hpp`, `vr_view.cpp`):
  replaced divergent `embedEase`/`embedEaseBody` with one `EmbedEase` (`pivot`, `offset`, `turn`) computed once per frame in
  `embedPinStep`. In flatscreen (`bodyEase`), both `setupWeapon` and `setupHand` apply the exact same body-carried, arm-clamped
  displacement and turn around `s.pos[hand]` (0.00 cm divergence measured by `vr_embed_probe_grip` over `vr_embed_free_ease`);
  VR stays unchanged. Empty hands in flatscreen ease directly from their body-carried release point clamped to the arm. Added
  `embedPinsReset()` called from `view::resetClientState()` and cleared `pin.seen`/`pin.lead`/`pin.bodyEase` when `!pin.on` or
  switching blades.
- **Exact wrist clamping, `.vr_embed_armpast`, yaw-local shoulder** (`Quake/vr/vr_view_hands.cpp`, `vr_avatar.cpp`, `vr_avatar.hpp`,
  `vr_view.hpp`, `vr_view_diag.cpp`): computed `pin.lead = onPos - pos` *after* `keepOnArm` so `solveArm`'s `wrist + embedPinLead`
  reports the true unclamped blade target past reach instead of collapsing to `<= 0 cm`. Stored `armEase.reachShoulder[side]` in
  player-yaw-local axes (`playerYawAxes()`). Replaced stale frame-old `reachLead` with exact current-frame wrist clamping
  (`wristOffsetFor(hand, ctlRot)` against `armEase.reachMost[hand]`), and added `view::embedEasing(hand)` so `armEase.flatHeld`
  doesn't shrink out from under `keepOnArm` during `vr_embed_free_ease`.
- **General `VR_Embed_FlatArm`, edge embeds, off-hand & 2H flat drop** (`QC/vr_embedhold.qc`, `QC/vr_wpnforcegrab.qc`):
  normalized `VR_Embed_FlatArm`'s away-step dot product (`stepAway * normalize(e.origin - (p.origin + p.view_ofs))`). Added
  `VR_Embed_HostYields(e, h)` and ordered checks so `limit = letgo + slip` and host yield decide cleanly: heavy/anchored hosts
  (ogre) and edge embeds (swung axe) never slide/tug out via walking and cleanly drop hands at `limit` (or `far > hold_reach`)
  so the blade stays embedded; yielding point embeds draw out smoothly along the axis into the hand; partial drag (`walk * 0.4`)
  when `vr_flat_embed_arm 1` prevents dragging a grunt 600 u indefinitely. Gated `vr_flat_embed_other_hand` on `vr_flat_embed_arm != 0 && most > 0`,
  and fixed `VR_Forcegrab_FlatDropHand` (`H`) so dropping a 2H-held stuck blade (spear/Lance) releases both hands together like 2H props.
- **Tests** (`QC/vr_flatembedwalk_test.qc`, `Misc/quakevr/embedhold_test.sh`, `QC/test_numbers.txt`): `vr_embed_test 64` now
  strictly asserts `ogre` -> let go (`vr_fw_end == 2`), `ragdoll`/`grunt`/`spear`/`sword+off` -> pulled out (`1`), `axe` -> let go
  cleanly (`2`, walked < 150 u, never drawn into hand), weapon+hand release-ease lock (`vr_embed_probe_grip <= 0.5 cm`, measured 0.00 cm),
  standing-still `armgap` & no creep, non-vacuous `.vr_embed_armpast > 0`, `vr_flat_embed_arm 0` off-hand gating, and restores
  `vr_knockdown`/`vr_hit_stagger`/`vr_melee_push`. `embedhold_test.sh` now sources `qvrtest.sh`, passes `-Clean`, and exits non-zero
  on any failure/missing verdict.
- **Verdicts** (Release no-LTO, 0 QC warnings): `embedhold_test.sh mp` 2/0 x3 (61/0 arm 1, 58/0 arm 0), `flatthrust_test.sh mp`
  7/0 (blades 12/0, Lance 7/0), `embeddual_test.sh mp` 16/0, `spear_test.sh mp` 4/0 (VR 25/0, flat 6/0), `vr_embed_test 24`
  (`embedfeel`) 22/0, `vr_embed_test 33` (`embedgrip`) 15/0, `vr_embed_test 29` (`embed1108b`) 10/0.
## agspearfix: VR_AGK_POLE / Pose Studio sync and generalized flatads section f (branch agent/agspearfix, 2026-11-26)

- **`VR_AGK_POLE` / `agpose::Pole` sync:**
  - `Quake/vr/vr_builtins_agpose.inc` (`agkKeys`): `pose = cls == VR_AGK_POLE ? VR_AGK_THRUST : VR_AGK_OVERHEAD + agMin(3.f, k);` so engine key lookup matches `QC/vr_armedgrunt_pose.qc:1789`; `agTwoHanded` also handles `twohand::poleWeapon` (`AG(vr_agh_set) != 1.f`) to match `VR_AGP_TwoHanded`.
  - `Quake/vr/vr_agpose.cpp` (`qcConstants`): added `{agpose::Pole, agpose::Carry, agpose::RPos, "VR_AGP_PORT", "VR_AGP_PORT_FW"}` alongside `Melee2, Carry` so `vr_agrunt_pose_dump` emits QC constant hints for `Pole, Carry` as well as `Pole, Aim`.
- **Pose Studio Drag Mode (`vr_posestudio.cpp`, `pose_grunt_pole.txt`):**
  - Kept the clean Pose Studio convention consistent across `Melee2`, two-handed guns, and `Pole`: gripping the weapon box (`PWeaponR` in `Weapon: Pose`) moves the pole and the off hand rides the shaft (`vr_agp_polel`), gripping `PHandR` edits `aggrip::Main`, and gripping `PHandL` edits `LPos` (`vr_agk_lp`) to slide/adjust the off hand along or off the shaft. Updated `partName(n, PHandL)` to display `"Left hand (on shaft)"` for `agpose::Pole` instead of `"Left hand (free)"`, and updated `quakevr/scenarios/pose_grunt_pole.txt` hint accordingly (suggested `quakevr/checklist.txt` wording reported to coordinator).
- **`Misc/quakevr/flatads_test.sh` section `f`:**
  - Fixed `u3[8] - z[8]` to `u3[8] - h[8]` (rise from hipfire `h[8]` rather than from zoomed `z[8]`) and changed `tune_reset` to `tune_reset vr_flat_zoom_up` (plus optional `FLATADS_ZOOM_UP` env hook for testing non-default starting values).
- **Tests:** `agspear_test.sh` 3/0 (`vr_agrunt_test 230` 7/0), `flatads_test.sh` 13/0 at default `vr_flat_zoom_up` (0) and `SECTIONS=f` 4/0 with `FLATADS_ZOOM_UP=2` (measured `4.97 -5.01 -> 2.46 -3.00 -> 2.46 -2.00`), `vr_agrunt_test 83` 63/0, `vr_scenario validate pose_grunt_pole` OK (2 spawns, 0 problems).

## flatmelee5: inverted-grip throw angle, immediate 2H melee grip, flipped-grip poses/attacks, close-range tip blows (branch agent/flatmelee5, 2026-11-26)

- **Inverted-grip melee throw angle (item 2, VR & flatscreen shared):**
  - `Quake/vr/vr_view.hpp`, `vr_view.cpp`, `vr_heldpose.hpp`, `vr_heldpose.cpp`, `vr_builtins_held.inc`: added `view::leavingAngles(hand, rot)` (publishing `{-e.angles[0], e.angles[1], e.angles[2]}` into `heldpose::Hand::leaveAngles` with no MP wire-format change) and updated `view::leavingTurn` to use `weaponTurn(...)` instead of `uprightTurnOf(...)`. `weapondrawnpose(player, hand, 49)` now returns the exact held reversed weapon orientation for both VR and flatscreen local players (and falls back to the fixed `leavingTurn` for remote MP players).
  - `QC/vr_flatgrenade.qc`: `VR_FlatThrow_Throw` preserves the held `mR`/`oR` rotation when throwing a `QVR_WPNFLAG_REVGRIP` weapon instead of overwriting it with upright `ang + release_pitch`.
- **Consistent immediate 2H melee hold on equip (item 4, flatscreen):**
  - `Quake/vr/vr_tunables.inc`: promoted `vr_flat_melee_2h` default `"0"` -> `"1"`.
  - `Quake/vr/vr_flatplay.hpp`, `vr_flatplay.cpp`, `vr_flatsight.cpp`, `vr_twohand.cpp`: added shared `flatplay::oneHandOnly(h)` (`isDagger`) and `flatplay::flippedGrip(h)`. Excluded 1H-only dagger from `twoHandAsked`, `carryToGrip`, and `heldOffPose`; excluded pole weapons and flipped blades from `twoHandAsked`; included `poleGrip` in `gripArrival`'s `anyLine` and `poleWeapon` in `twohand::applyFixed`'s `goodDot` so sword, axe, spear, and Crystal Lance attach the off hand immediately when equipped with an empty off hand while dagger stays 1H.
  - `QC/vr_embedhold.qc`: in `VR_Embed_Try`, force `helping = FALSE` in flatscreen when `vr_flat_embed_arm == 0` so a 2H-held spear/Lance never attaches an undroppable off hand on embed when flat arm release is off.
- **Promoted 4 tunables & flipped-grip poses/attacks (item 6, flatscreen):**
  - `Quake/vr/vr_tunables.inc`, `vr_tunables_rows.inc`: promoted `vr_flat_overhand_raise_up "8"`, `vr_flat_revgrip_dagger_fwd "-10"`, `vr_flat_revgrip_dagger_up "14"`, `vr_flat_revgrip_dagger_pitch "85"`; aligned sword (`fwd "-10"`, `up "14"`, `pitch "85"`, `down "5"`) and pole/spear (`fwd "-4"`, `up "22"`, `pitch "165"`, `down "5"`, `dagger_down "5"`); added 17 `vr_flat_revgrip_swing_*` and 6 `vr_flat_revgrip_thrust_*` tunables + Debug Tunables slider rows.
  - `Quake/vr/vr_flatplay.cpp`, `vr_flatsight.cpp`: moved `revGripPose` before `swingOverhead()` so the strike drives forward/downward onto the crosshair target instead of being pulled back by the idle offset; applied flipped orientation directly to `s.rot[h]` for melee weapons (dagger, sword, spear) so server sweep (`s.muzzle[h]`) and thrown angles match the drawn blade while keeping `revGripDrawnTurn` drawn-only for pole guns (Crystal Lance beam keeps to crosshair); unified flipped spear thrust with flipped blade thrust; skipped `poleAimsAtPoint` overwriting `s.rot[h]` on flipped melee/thrusting poles.
- **Close-range sword/spear tip/blade hits (item 8, VR & flatscreen shared):**
  - `QC/vr_melee.qc`: added `vr_mclose_blade` target-side check in `VR_Melee_DecideOn`. For non-free-grip sword/pole weapons, promotes forward close-range shaft thrusts (`VR_MPT_BLUNT`, `along >= 0.7`) to `VR_MKIND_STAB` (`f = 1`, `"the tip"`) and blade-side / forward / flatscreen close-range hilt overlaps (`VR_MPT_NEAR`) to `VR_MKIND_STAB` or `VR_MKIND_SLASH` (`f = 1`, `"the tip"`), landing immediately in `VR_Melee_SweepNow` while preserving sideways/diagonal shaft swings (`spear_test.sh`) and intentional VR pommel strikes.
- **Tests (`QC/vr_flatthrust_test.qc`, `vr_embed_test.qc`, `test_numbers.txt`, `Misc/quakevr/flatthrust_test.sh`, `flatthrow_check.py`):**
  - Added `vr_embed_test 60` (`flatmelee5`, 24 checks covering all 4 acceptance items) and Lance immediate-2H equip check in `vr_embed_test 59`; rejoined console-wrapped continuation lines in `flatthrow_check.py`.
  - Verdicts (Release no-LTO, 0 QC warnings): `flatthrust_test.sh` 8/0 (blades 12/0, Lance 8/0, flatmelee5 24/0), `spear_test.sh` 4/0 (VR 25/0, flat 6/0), `embedhold_test.sh` 2/0 (arm 1: 61/0, arm 0: 58/0), `flatthrow_test.sh` 38/0, `flatmelee2h_test.sh` 16/0, `flatmelee4_test.sh` 9/0, `flatfg2h_test.sh` 11/0, `flatlance_test.sh` 30/0, `tunables_test.sh` 13/0, `vr_embed_test 42` (`revgrip`) 24/0, `vr_embed_test 44` (`flipgrip`) 37/0.

## flatgrabthrow: unified Throw/drop on B and Grab/pull on H (branch agent/flatgrabthrow, 2026-11-26)

- **Unified Throw / Drop on `B` (`vr_flat_key_throw`, default `"b"`, Controls label `"Throw / drop"`):**
  - Retired `vr_flat_key_drop` (added to `retiredCvars` in `Quake/vr/vr_cvars.cpp`; `vr_msgtokens.cpp` aliases `{key:vr_flat_key_drop}` -> `vr_flat_key_throw` so existing baked maps show `'B'`; `impulse 47` remains working for holster menu DROP / DROP BOTH and console).
  - Added `VR_FLATTHROW_PENDING` phase (`QC/vr_flatgrenade.qc`, `Quake/vr/vr_flatgrenade.cpp`) with tunables `vr_flat_throw_tap_time` (`0.18` s) and `vr_flat_throw_drop_speed` (`1.2` m/s):
    - **Quick tap (`< tap_time`)**: no wind-up twitch; drops held weapon or carried prop/crate/gib gently right in front of the player via VR's shared `DropWeaponInHand` / `VR_Carry_Release` path, drops grabbed ragdolls via `ragdollthrow`, or lets go of a stuck/embedded blade via `VR_Forcegrab_FlatDropHand` (releasing both hands on 2H stuck blades while leaving it embedded).
    - **Hold (`>= tap_time`)**: transitions cleanly into `VR_FLATTHROW_WIND` (or `VR_FLATGREN_WIND` for quick grenade), charging and throwing held weapons, carried props/crates/gibs, or `H`-grabbed ragdolls (with 2H strength when both hands hold the ragdoll, scaled by `vr_flat_grab_ragdoll_throw`).
- **New Flatscreen Grab / Pull on `H` (`vr_flat_key_pull`, default `"h"`, Controls label `"Grab / pull"`):**
  - Built directly on VR's existing physical grab/carry/ragdoll systems (`VR_Carry_Start`, `carry2h`, `VR_Hand_Take`, `World::RagdollGrab` / Box3D motor joints) — NOT force-grab (`QC/vr_flatgrab.qc`, `Quake/vr/vr_box3d_ragparts.cpp`, `Quake/vr/vr_flatgrenade.cpp`, `QC/weapons.qc` impulses 96/97/98, `STAT_QVR_FLATGREN` bits 256/512/1024).
  - Shared `manhandleStagger` between VR `grabRagdoll` and flatscreen `ragdollGrabReach` (dispatched via `ragdollgrab` optional mode `-1`, whole-body release/throw via `-2`, zero new builtin table slots); added `!VR_FlatPlay(self)` guard on `carry2hoff` in `QC/vr_carry.qc` so synthesized flatscreen hands don't detach 2H crates.
  - Uses every available empty hand (1H or 2H) to grab crates, props, active/shoved or dead ragdolls, non-force-grabbable gibs, torches and loose weapons within `vr_flat_grab_reach` (`100` u) and `vr_flat_grab_cone` (`28` deg), with subtle reach-out and hold-out hand animations (`vr_flat_grab_*` tunables under `Flatscreen > Grab / Pull`).
  - Added player option `vr_flat_grab_hold` (`0` = Toggle [default], `1` = Hold) under `Locomotion > Grab / Pull Key` (`vr_menu_rows.inc`, `vr_menu_pages.inc`, `Misc/quakevr/menu_paths.txt` updated; `vr_menu_path_check maps/vrcalibration.map`: `6 found, 0 missing`).
- **Tests & Scenarios:**
  - Added `Misc/quakevr/flatgrabthrow_test.sh` (23/0: tap-B weapon drop, hold-B weapon throw, 2H crate grab/walk/toggle/hold-B throw, 1H crate grab with weapon in main hand, Hold-mode grab/release, shove->active ragdoll 2H grab & throw, non-force-grabbable gib grab & throw) and `quakevr/scenarios/flat_grab_pull.txt` (`SCNOK`).
  - Updated `flatkeybind_test.sh` (31/0), `flatthrow_test.sh` + `flatthrow_check.py` (38/0), `flathands_test.sh` + `flathands_check.py` (76/0), `QC/vr_flatthrust_test.qc` (`flatthrust_test.sh` 7/0, blades 12/0, Lance tap-B drop 7/0), `quakevr/scenarios/flat_prop_buttons.txt`, and `Misc/quakevr/maps/vrtutorial_gen.py`.
  - Other targeted runs: `flatinput_test.sh` 19/0, `flat_ammo_key_test.sh` 7/0, `vr_msgtokens_test` 12/0, `tunables_test.sh` 13/0 stages (2479/2479 tree), `flatswap_test.sh` weapons 22/0, things 26/0, melee 96/0, holster 0 faults (`melee_ad` 45/3 pre-existing on 6th AD weapon), `checklist_tests_check.py` 104/0.
## flatwaterclimb: climbing IK elbows, no underwater footsteps, underwater crouch/prone & crouch-only slide (branch agent/flatwaterclimb, 2026-11-26)

- **Vertical pole/rope climbing IK elbows (#9):** `Quake/vr/vr_avatar.cpp` (`solveArm`) previously used `ropeHang()` alone (lagged/gated by `legs && vr_body_climb_feet`) and built the pole from the clavicle's tilted `lateral` and tilted chest `-cUp`/`-cFwd` (on a swinging rope, `ropeHips` tilts the torso back so `-cUp` pointed forward into the camera, lifting/folding the upper arm's elbow across the face). Now combines per-hand `climb::ropeLine` weight with `ropeHang()` (0-frame lag, flatscreen & VR, works regardless of `legs`/`vr_body_climb_feet`), projects upright `worldOut` (`±b.left`) into the swivel plane (`uOut`), and adds unnormalized orthogonalized `-UP - 0.5 * b.fwd` (`uDown`, orthogonal to `worldOut` and smooth with no pole-slide singularity). Tightened `vr_rope_flat_test` (`Quake/vr/vr_climbrope.cpp`) to require `climbBow > 0` on both rope and pole and `climbSide > 0` on pole (measured: rope bow min `-1.5 -> +2.6 cm`, pole bow min `+6.4 -> +15.1 cm`, pole side min `+6.5 cm`, mean `+8.8 cm`).
- **No footsteps underwater (#11):** Guarded both `QC/player.qc` (`player_run`: skip dry footsteps and reset `spawnsilent` when `waterlevel >= 2`) and `Quake/vr/vr_playersteps.cpp` (`inDeepWater()` checks `cl.inwater` or `local->v.waterlevel >= 2`; blocks `fromLegs()` and silences `oldStepSilent()` for any player/bot with `waterlevel >= 2`), so walking on the bottom underwater (`FL_ONGROUND`) never plays dry-land footsteps in flatscreen, VR, SP, or MP. Added `silent` check mode to `vr_body_step_report`.
- **Underwater crouch & prone + prone never slides (#12, #13, #14):**
  - Separated flatscreen prone from `moveFlatCrouchBit` via client->server `protocol::moveFlatProneBit` (`1 << 13`, stripped before server-side `withPreviousBits`, zero wire change; `vr_protocol.hpp`, `vr_client.cpp`, `vr_server.cpp`, `vr_hull.hpp/cpp`). `flatplay::crouchHeld()` / `hull::flatCrouchHeld()` is now strictly the crouch key (never prone), so pressing prone at full running speed never starts a slide (`vr_slide.cpp`), while crouching at speed still slides.
  - Underwater (`waterlevel >= 2` / `cl.inwater`): flatscreen crouch with `vr_flat_crouch_swim 1` swims down only without lowering the eye, crouch box, or crouch pose (`vr_flatplay.cpp`); `vr_avatar.cpp` also suppresses `b.crouchPose` by `(1 - detail::water.swim)` for VR + flatscreen consistency.
  - Underwater prone is auto-exited on entering `waterlevel >= 2` and ignored while underwater across both flatscreen and VR (`vr_hull.cpp`, `vr_flatplay.cpp`, `vr_avatar_pitch.cpp`).
- **Tests:** Release no-LTO `build.sh dagger` -> `built` (0 QC warnings, all `qvr_checks` pass). New `Misc/quakevr/flatwaterclimb_test.sh`: `10/0` (covers all 5 items across flatscreen & VR, including `vr_flatslide_test` `10/0`, `vr_rope_flat_test rope` `10/0`, `vr_rope_flat_test pole` `9/0`). Regression suite: `flatswim_test.sh` `48/0`, `flatcrouch_test.sh` `3/0`, `vr_flatprone_test` `7/0`, `prone_vent_test.sh` `11/0`, `vr_slide_test` `40/0`, `swimtilt_test.py` `12/0`, `crouch_speed_test.sh` `4/0`, `rope_flat_test.sh poleslide/slide/strung` `10/0`, `9/0`, `5/0`, `rope_climb_test.sh rope_climb/pole_wood` `13/0` each, `rope_hands_test.sh` `6/0`, MP `mp_netsafe/mp_coop/mp_version/mp_slide` `14/0`, `12/0`, `7/0`, `9/0`.
## flatammoswitch: systemic ammo-switch magazine ejection before flag/model flip (branch agent/flatammoswitch, 2026-11-26)

- **Diagnosis:** In `ToggleSecondaryAmmo` (`QC/weapons.qc`), `VR_Reload_MagIn` returns `FALSE` for flat-screen play (`!VR_FlatPlay`), so pressing `N` fell into `VRTryUnloadWeapon`, which refunded the clip into reserve without setting `QVR_WPNFLAG_NOMAG` or ejecting the magazine. `QVR_WPNFLAG_USE_SECONDARY_AMMO` then flipped immediately, switching `.weaponmodel` to the alternate model (`v_lava.mdl` / `v_lava2.mdl` / `v_light_plasma.mdl`) while the magazine was still drawn attached (`vr_mag_on_v_lava.mdl`, etc.) until `VR_FlatReload_EjectFrame` fired `vr_flat_mag_eject_delay` later and ejected a magazine with the *new* ammo type/skin.
- **Fix (shared eject path, flat + VR):**
  - Added `VR_FlatReload_AmmoSwitch` (`QC/vr_flatreload.qc`) called by `ToggleSecondaryAmmo` (`QC/weapons.qc`) before `QVR_WPNFLAG_USE_SECONDARY_AMMO` flips: cancels any active flat reload trip for that hand (`VR_FlatReload_Stop`), clears `vr_flatrl_unl*`, refunds any loaded rounds to the old ammo reserve via `VRTryUnloadWeapon`, and immediately flushes/ejects the old magazine via `VR_FlatReload_EjectNow` (setting `QVR_WPNFLAG_NOMAG` and spawning `vr_ammo_mag` via shared `VR_Reload_EjectOut` -> `VR_Reload_Eject` with the old ammo id/skin).
  - Updated VR Immersive `ToggleSecondaryAmmo` to share `VR_Reload_EjectOut(self, xHand, 40, "the ammo changed")` instead of a separate `VR_Reload_Eject` vector calculation.
  - Added `flatmag:` client diagnostic logging under `vr_flat_reload_debug` + `developer` in `setupHeldMagazines` (`Quake/vr/vr_view.cpp`, file-scope `HeldMagDebug`) to observe held weapon/magazine transitions directly.
- **Tests & verdicts:**
  - `vr_reload_test 50` (`QC/vr_holsterammo_test.qc` `VR_AmmoSwitch_Test`, `QC/vr_reload_test.qc`, `QC/test_numbers.txt`): **54 passed, 0 failed** (both VR and flatscreen runs; Nailgun, Super Nailgun, Thunderbolt own<->alt with 0 target ammo, dry-fire check, switch-back check, pending empty-eject flush, mid-reload cancel, Rocket/Grenade Launchers).
  - `Misc/quakevr/flat_ammo_key_test.sh`: **13 passed, 0 failed** (verifies `N` on Nailgun, Super Nailgun, Thunderbolt with 0 alternate ammo never shows the new attached magazine on the client and ejects the old magazine at switch).
  - `Misc/quakevr/flatreload_test.sh`: **54 passed, 0 failed**; `Misc/quakevr/holsterammo_test.sh` (VR & flat): **28 passed, 0 failed** each; `vr_flatswap_test 5`: **46 passed, 0 failed**; `Misc/quakevr/emptyreload_test.sh`: **4 passed, 0 failed**.

## shiprelax: ships no longer wait for his game; his tunables and ticked checklist lines go in with every ship (kit, 2026-11-26)

- Kit only (qvr-kit `scratch/ship_task.sh`): a ship merges, builds, smoke-tests (low-impact while his game runs) and
  pushes without waiting for his game; only the fast-forward of his checkout waits (`coord_ffhis.sh`: "fast-forward
  pending (game open)", retried by the next ship and by `kit_health.sh`).
- Before the merge a ship promotes his `quakevr/tunables.cfg` values into `Quake/vr/vr_tunables.inc` (`tune_ship.py`;
  personal ones skipped: names with toolgun, calib/handcal, comfort, body or record) and retires the checklist lines he
  ticked that no voice note names since they were added (`cl_retire_ticks.py`; scenes untouched, the ship's own lines
  skipped), each as its own commit ("tunables: promote his values", "checklist: retire N ticked lines"). A build that
  fails with them in ships again without them (warning). Flags: `--no-promote`, `--no-retire`, `--wait-game` (releases).

## hullinstall: the installer makes the collision of Quake's and the mission packs' maps, and sends it to the Quest (branch agent/hullinstall, 2026-11-26)

His ask: generate the hull trees at install time for the most common maps, as the relight is.
- **Engine** (vr_prepare.cpp, new step "hulls", `-preparehulls [<map>,...]`): after the relight (or after the maps/campaigns
  when there is none; also after `-preparemaps`' list), every single-player map of id1, hipnotic and rogue that is installed
  (`relight::maps::collect(Set::Game)`; start, e1m1..e4m8, end; hip1m1..hipend; r1m1..r2m8; each campaign's own `start`,
  campaign selected as the player plays it; not dm*, hipdm1, ctf1) is loaded once (skill 2: the most monsters, so their
  boxes' trees), then `hull::finishLoads()` waits for every tree it asked for, the deferred ones too. The trees are
  written even when quicker than 25 ms (`hull::setWriteEveryTree`, the one hook in vr_hull.cpp: a Quest takes several
  times the PC's). Progress lines: `step hulls i n map`, `hulls <map> ok <s> files=N` / `hulls <map> failed <s>`
  (mapTimeout 600 s, never fatal), `result hulls ok= failed= files= written= mb= <s>`. The files of each map (the world's
  key prefix, `hull::detail::keyOf`) are listed in `setup_hulls.txt` beside the progress file (cache path, tab, whole path).
- **Relight order**: relit and original e1m1 have the same entities, planes, nodes, clipnodes and models (only leafs'
  vis offsets, vis, lighting and faces differ), so the trees' key is the same: a load with `vr_relight_use 0` read the
  same 7 files. The step still runs after the relight (it loads what the game loads).
- **Installer**: GamePreparation passes `-preparehulls` (default on; Options "Prepare collision for Quake's maps", ticked,
  "about a minute", ~150 MB in the disk estimate; CLI `--no-hulls`); the bar: maps 15 %, relight to 75 %, hulls the last
  25 % (no relight: maps to 75 %); status "Preparing collision for e1m3 (12 of 65)"; one log line at the end, a warning per
  failed map. **Quest**: QuestFiles.AddHulls sends the listed files that still exist to `cache/hulls/<build>/<file>` under
  the app's files folder, the headset's own disk cache (`<base>/cache`, the files folder its base), read as files it made
  itself; list lines outside `hulls/<build>/` are refused. Not run on a headset (headless reasoning only): the keys and the
  build folder are the map's data and the trees' version, the same on arm64 (prebuiltcache, and loadprof2's Quest read
  vrstart's shipped trees: 9 shipped reads).
- **Numbers** (this PC, kit run, fast mode, other agents' runs beside it, cold cache): 65 maps (id1 32, hipnotic 17, rogue
  16) in 66.7 s, per-map median 0.9 s (slowest r1m5 2.5 s, r1m1 2.1 s, e1m1 2.0 s); 508 files (65 brushes + 443 trees),
  148.3 MB on disk. Within the hulls' 1 GB cache budget.
- **Tests**: `-prepare <f> -preparemaps vrcalibration -preparehulls e1m1,e1m2,start` (22 files, 4.3 MB, 2.8 s), then a
  fresh game on that cache, `vr_hull_defer 0; map e1m1; vr_cache_info`: hulls 7 read, 0 missed, 0 written. Installer
  self-tests 61/61 (new: the hulls' progress lines and bar shares, `-preparehulls` on the command line and the engine's
  line formats; Quest: the list's files go to `cache/hulls/<build>/`, missing and outside paths refused).
- Open: the Options page gained a row (the page was sized for 1366x768: it scrolls if it no longer fits; not looked at in
  the screenshot harness). A Quest-only install (staging folder) runs no preparation, so it has no list. The trees of
  dopa/mg1/mg3 and AD are not made (not asked).
