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
