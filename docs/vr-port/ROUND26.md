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

## vrstartgeo: the hub's geometry measured for cheaper collision; no safe simplification gains anything (branch agent/vrstartgeo, 2026-11-26)

His question: could vrstart be made cheaper (fewer brushes, smaller hull trees) by merging brushes or slightly
simplifying geometry, without changing its looks or function. **Answer: not without visible changes.** The terrain is
all of the collision cost, and the parts of it nobody sees closely are already sparse. Nothing was changed: the
generator, the .map, the .bsp, the relit copy and the shipped cache stay as they are, so nothing needs regenerating.

**What the map is made of** (`vrstart_gen.py` output, 10,450 brushes and 63,767 planes): the terrain's prisms are 9,069
(87 %: island 3,744, cliffs and mountains 4,042, lake floor 1,283). The rest: lights 252, pines 202 (about 21 planes
each), boulders 175 (about 24), the structures about 610 (pavilion 123, bridge 104, tower 91, range 82, pier 63, terrace
54, stairs 75, arrival 22), func_detail_illusionary 76, buttons 34, barrels/boat/islets 19. The terrain's 10,366
triangles (merged into 9,069 prisms where exactly coplanar), by area: island inland 2,939, cliff faces 1,908, open lake
floor 1,483, cliff tops behind the edge 1,349, the island's shore underwater 1,133, cliff feet underwater 746, beach 464,
mountains 344.

**The compiled .bsp** (qbsp 0.18.1, the same counts when rebuilt from the committed .map): 72,755 world faces, 53,407
nodes; hull 0: 20,694 solid leaves, 23,002 empty, 9,707 water, 5 sky; hull 1: 82,974 clipnodes, hull 2: 69,109;
97,987 marksurfaces; 17.9 MB. **The terrain alone** (`--only-terrain`): 20,741 solid leaves, 76,041 / 70,730 clipnodes.
So the structures and decor add practically no solid leaves (they add 25,000 drawn faces).

**In the game** (PC, Release no-LTO, `vr_cache_shipped 0; vr_hull_cache 0; vr_hull_defer 0`, cold, other jobs beside
it, so times are rough): 24,773 brushes (141 clip), 516,832 planes (354,146 bevels); 8 trees of 0.81-1.01 M nodes,
21.6 MB each; the trees' compile times summed to 23.8 s. On the terrain-only .bsp: 21,012 brushes, 466,608 planes, and the
standing tree has 1.03 M nodes, *more* than the full map's 0.92 M. The trees are the terrain's.

**Tried** (terrain only, qbsp; solid leaves and hull 1 clipnodes against 20,741 / 76,041):

| change | prisms | hull 0 solid leaves | hull 1 clipnodes |
|---|---|---|---|
| underwater and hidden parts simplified (the shore deeper than 16 units within 3 units, the lake floor 6, cliff feet below -24 within 4, cliff tops past the edge within 6) | -3.7 % | +4.9 % | +5.6 % |
| island points lying exactly on the coarser surface left out (no change in shape) | -3.7 % | +4.8 % | +5.6 % |
| both | -7.5 % | -0.3 % | +0.1 % |
| the prisms' floor raised (FLOOR_Z -1024 to -640, -448, -384) | 0 | 0 | 0 |
| qbsp -maxnodesize 512 / 256 (full map) | 0 | +6 % / +13 % | -4 % / -9 % |
| qbsp -forcegoodtree | | hull 0 came out broken (1,376 nodes) | |
| *visible*: the island's open ground on a 128 lattice instead of 64 (the coast more than 200 units back; outlines of paths, zones and the ravine kept) | -13 % | -14 % | -13 % |
| *visible*: that, plus the cliff faces' rings 180 points instead of 260 | -20 % | -23 % | -20 % |

What dominates is the number of distinct terrain faces. qbsp splits only on face planes (hidden faces between prisms
don't count), so how the solid is cut into brushes, or how deep it goes, changes nothing. An edit that removes under
about 5 % of the faces is lost in qbsp's noise: any edit moves the leaf count by +-5 %.

**Ranked, for him to decide** (none done):
1. The island's open ground at 128 units instead of 64 (visible: facets twice as wide on the grass, grass patch outlines
   coarser, walkable heights move by a few units, so vrstart_walktest.py would need rechecking for snags): about -14 % of
   leaves and clipnodes, and the trees probably about the same.
2. Fewer points in the cliff-face rings (260 to 180: facets about 80 to 115 units wide along the shore): with 1, -23 %.
3. Engine side (agent/hullspeed's area, untested idea): most recovered terrain brushes are slanted slices with ~14 bevels;
   brushes that touch only other solid (deep under the ground), or that no box can reach (behind the cliffs), could be
   left out of the trees.
4. Not worth doing (measured above): simplifying underwater or hidden parts, dropping exactly-redundant points, raising
   the floor, qbsp's split flags.

Scripts (worktree round25, `scratch/`, not committed): `vsg_count.py` (brushes and planes per group in a .map),
`vsg_bsp.py` (BSP2: hull 0 leaves by contents, clipnodes per hull), `vsg_qbsp.sh` (qbsp only, then the stats),
`vsg_bands.py` (terrain triangles by area), `vsg_exp2.py` (terrain variants: tolerances, `VSG_FLOOR`, `VSG_INLAND`,
`VSG_GEN`).
## hullwait: no pause in play for a crouch after a cold load; the loading panel says what it builds (branch agent/hullwait, 2026-11-26)

His words: a pause in play is worse than a longer load; loads need clarity (a progress bar like the shaders' one).
- **The player's boxes with the load again** (`vr_hull_defer` 1, same value, new meaning: no migration; its Debug > Tests
  row renamed "Monster Hitbox Builds"): beforeLoad posts the standing, crouched (52/44/36) and lying boxes' trees together
  (`playerBoxExts`), each a job on the pool within `vr_hull_build_mb` as the standing one was, so they build beside the
  entity spawn and the load (settle) waits for all; afterLoad's job holds the monsters' sizes only (Quake's hull
  meanwhile, seamless). A player's box is never in a job (a setting's change builds it at once: prepare), so
  `joinBackgroundTree` can only wait for a diagnostic's box; its `hull: a ... trace waited` line stays as the guard.
  prepare(true) now settles first. 0: the monsters' trees waited for too, as before.
- **The loading panel** (`vr_load_panel`, vr_shaderload.cpp, the shaders' `drawPanel` shared): after a second of waiting,
  "Building the map's brushes (1 of 6)", then "Building collision for standing/crouching/lying down/monsters (k of n)"
  (the first tree being built), a bar of the real progress, "vrstart: 50% - built once for this version" and the
  seconds. Progress: the brushes one step, each tree 0-1: its brushes grown (`growAllOnPool`, per run of 256: a quarter)
  then its top's units built (`speculate`: each leaf unit adds its share of the split, half its parent's, so they sum to
  1: the rest); a tree read from the disk is done at once (`LoadTrees`: atomics written by the pool, the entries fixed
  while the load's jobs run). Shown in the headset and now the window too (flat-screen, the desktop beside VR; not the
  Quest's immersive app: `VR_NoWindow`). `VR_MapLoadingFrame(heading, detail, done, seconds)`.
- **Test aid** `vr_load_panel_shot` (Debug > Tests "Save the Loading Panel", At Half Way = 0.5): the panel's image saved
  as `screenshots/load_panel.png` when a load's progress reaches it, then back to 0. Scene `load_collision_cold` (vrstart
  with `vr_hull_keep 0`, `vr_hull_cache 0`, `vr_hull_build_mb 300`: a cold build every time, nothing written).
- **PC runs** (Release no-LTO, a loaded machine): cold vrstart (fresh `vr_cache_dir`, `vr_cache_shipped 0`): 5 trees with
  the load (standing, 3 crouched, lying), built during the 7-20 s entity spawn: waited 0-0.6 s; the 3 monsters' ready
  6.9-8.9 s after; crouching (box 36) and lying (box 16) a second after the load: no `trace waited` line;
  `vr_hull_keeptest` PASS (8 trees) cold and read. vrstart again with nothing kept or read: the panel 15.9 s (VR mock,
  build_mb 300: 951 frames), 8.3 s and 5.8 s (flat), saved at 50% "Building collision for standing (4 of 6)"; flat
  `+flatcrouch` right after it: box 36, no wait. crouch_speed_test 4/0.
- Quest: not measured (the optional step 4 skipped). Expected: ad_sepulcher's cold load between loadprof2's 16.1 s and
  35.8 s (the crouched and lying trees now built beside the spawn), with the panel, instead of an 8.6 s pause in play.
## Flat stuck blades: free off hand joins, pole shaft grip, ragdoll smoothing (branch agent/flatembed3, 2026-11-26)

His items 3, 7 and 10 (flat notes 2026-11-25/26).
- **Off hand joins (item 3):** flat play's free, empty other hand takes any two-handed stuck blade (not the dagger, not a
  2h_mode 2 weapon: `VR_Embed_CanTwoHand`) when the blade's grip/shaft segment (handle to wound) is within its arm's reach
  from the off shoulder: `VR_EMBED_FLATARM_REACH` 0.6 m plus `vr_flat_embed_other_hand` cm (default 30; 0 never). Checked at
  the stab (`VR_Embed_Try`) and every hold think while one hand holds it (`VR_Embed_HoldThink`, not during a pull), so a
  one-handed stab or a later regrab gets the second hand too. A hand `VR_Embed_FlatArm` dropped for reach waits 0.4 s and
  then needs 85% of the reach (`vr_hold_flatdrop_*`). Both hands' flat anchors are the handle (`VR_Embed_SetG0`). H drops both.
  Test: vr_embed_test 64 (`embedhold_test.sh`): the spear, the sword stabbed one-handed and the swung axe all assert the
  other hand on it the whole hold with vr_flat_embed_arm 1, none with 0.
- **Pole shaft grip (item 7):** one helper, `poleShaftGrip` (vr_view_hands.cpp, on `alignChannel`): the hand's grip channel
  turned and moved the least onto the pole's line. Held (flat): every state now, by `vr_flat_pole_off_snap` (default 1;
  0 = only the flipped grip, as before), on the drawn mount-to-muzzle line. Stuck in a body (VR and flat): a hand on a grip
  hotspot (not the handle) of a spear/Lance goes onto the shaft line through that hotspot (butt-to-tip way,
  `weaponLocalGeometry`; the model's butt/tip points themselves, placed by `hotspotFrame`, were 13-21 units off the drawn
  grip); in flat play, out of the arm's reach, it slides down the shaft towards the shoulder (`keepOnArm` along the shaft),
  and if no shaft point is in reach, as near as the arm gets. Probe: `vr_embed_probe_pdist` / `_pturn` / `_pon` (cm and
  degrees the channel is off the shaft, frames). The embedded holder frame keeps `reversed=false` on purpose: it is a frame
  rigid with the blade, not the holder's real hand.
- **Ragdoll smoothing (item 10):** `view::embedSmoothStep` (before `embedPinStep`): a held blade stuck in a ragdoll is drawn
  with its place eased and turn slerped (`vr_embed_ragdoll_smooth_time` 0.04 s, 0 off), trailing the body by at most
  `_max_pos` 2 cm / `_max_deg` 6 degrees. Written into the client entity, so the blade and every hand drawn on it share it.
  Only ragdolls: STAT_QVR_EMBED* is now edict * 8 + in a ragdoll 4 + reversed 2 + by handle 1 (`ragdoll::published` of the
  blade's host; `axestick::heldEnt` / `heldInRagdoll`; vr_flatplay.cpp and vr_gadgetlock.cpp use `heldEnt`). Smoothing a
  standing body's blade broke the flat arm's reach (vr_embed_test 64, ogre case), hence the gate. State reset on map change
  (`embedPinsReset`) and on a new blade entity or model. Probe: `vr_embed_probe_jraw` / `_jdrawn` (mean frame-to-frame change
  of velocity in units plus turn rate in degrees, in the body and as drawn), `_jlag` (cm), `_jon`.
- **flatembed4 (finish):** (a) `vr_flat_pole_off_pitch` turned the drawn other hand 2 of 30 degrees: the shaft grip's least
  turn undid it. Its shaft-grip share is now turned about the shaft after the grip (`turnAboutShaft`): the hand stays on the shaft
  and the tunable turns it (flatlance 30 degrees). (b) flatlance's "many thrusts": the Lance left in the ogre. Not the join stealing
  it: with both hands anchored at the handle (`VR_Embed_SetG0`, item 3) the hold's averaged `far` lost the few units of slack the
  second hand's own anchor up the shaft had given, and an ogre staggering 19 units back after a fast thrust took it past Hold Reach,
  where embedholdfix's flat rule let go of a point in a heavy body. Now that rule lets go only when he himself walks off the blade
  (`VR_Embed_FlatGoingAway`, shared with `VR_Embed_FlatArm`); a point the body carries off comes free into its hand. (c) Out of the
  arm's reach the stuck shaft's hand went back to its own place, the spear's shaft 38 cm away; now it is pulled from the shaft's
  nearest point straight towards the shoulder (13 cm off at 42 units, on it when in reach). Probes: `_pdist` / `_pturn` are taken
  after every placement (`poleProbeNote`), `_hoff` skips frames the flat arm holds the hand short of the handle (pin.lead).
  Tests: `poleembed_test.sh`: vr_embed_test 71 (spear and Lance, upright, flipped, guarding, stuck: 0.00 cm, 0.0 degrees, 16/0;
  the spear's grunt at 32, its shaft in reach) and 72 (a sword in a falling ragdoll: smoothed 0.41 of 0.60 jitter drawn, lag 2.00
  cm = max_pos, hand 0.00 cm off the handle; smoothing 0: the same jitter, no lag; 13/0).
- Open: the parry pose and the smoothing were not checked visually; at 42 units the stuck spear's hands are drawn short of it
  (the flat arm's reach), the main hand too.
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

## hullspeed: the hull tree builder folds same-leaf nodes; the exact axial choice tried and left out (branch agent/hullspeed, 2026-11-26)

His ask: make the hull tree compile faster in general. What changed, in `vr_hull.cpp`'s tree builder only:

- **Fold (shipped)**: `TreeBuilder::fold` takes back out a node whose two sides came out as the same leaf (qbsp's
  PruneNodes); its parent can fold in turn. The build on the pool does the same in `emit` (kind-1 units) and the
  models-at-once path, so the pool, one thread and the disk cache still give the same bytes. Leaf counts count the
  folded leaf once. `treeVersion` 1 -> 2: **the shipped cache (`cache_shipped`) needs a rerun at release**
  (make_shipped_cache.py). `developer 1` adds "N nodes folded" to the tree's report line (`Tree::folded`).
- **B, the exact axial choice (tried, not shipped)**: every axial plane weighed at big nodes, with front/back counts by
  binary search in the pieces' bounds sorted per axis (the same counts as one by one). vrstart: same build time
  (17.2-30 s noisy vs 17.5-17.8), **+1.5 % nodes** (7.29M -> 7.40M). The brief's premise had gone: the slowest unit is
  now about 500 pieces and 0.6-1.1 s (the 20,423-piece unit was before the units' later split), so a better pick at the
  top does not pay. The diff is kept: `scratch/hs_B_exactaxial.diff` (not committed).
- **F, arenas**: not tried (time neutral overall, no profile showing allocation over 5 %).
- **DAG sharing of identical subtrees**: not done (every walker would need checking; the fold alone took the bulk).

Numbers (PC, ThinLTO, `--exclusive`, cold trees `vr_hull_cache 0`, interleaved A/B; A = 3b638f311):

| | A nodes | B nodes | A trees' sum ms (runs) | B (runs) | disk files |
|---|---|---|---|---|---|
| vrstart, 8 trees | 7,293,885 | 4,521,324 (-38 %) | 17,482 / 17,841 / 19,079 | 16,222 / 17,551 | ~131 -> 98.9 MiB |
| ad_sepulcher, 6 trees | 2,863,601 | 2,116,302 (-26 %) | 4,240 / 6,014 / 6,274 | 6,182 / 5,836 / 6,099 | (same ratio) |

Build time is within the noise (the deferred trees share the pool with the load and each other: the same tree's
"units built" swings 2x between runs; the first, synchronous tree is equal to the ms). ~355k of vrstart's ~918k nodes
a tree were splits with solid on both sides: the work that made them is still done; only the nodes go. So: trees a
third smaller in memory and on disk (the 1 GB disk cache holds more maps), warm reads 0.2-0.3 s either way.

A pointer for a real time cut (not done, budget): those folded nodes measure the work spent splitting inside solid.
The coordinator's vrstartgeo study suggests leaving out recovered brushes buried in other solid (terrain interior) before
the build; the fold count per tree (`developer 1`) is the measure to check such a cull against.

Tests: `vr_hull_keeptest` vrstart PASS 8/8 on the pool and with `vr_jobs_parallel 0` (one thread: same hashes);
`vr_hull_cache 2` vrstart 8 the same, 0 different; `vr_hull_holes_test` vrbrushholes 29/0; `vr_mhull_walktest 30` e1m2 in
solid 0, stuck 10-16 (base 10: the deferred trees' timing varies it); lowceil 16/0, crouch speed 4/0, prone 26/6 and crawl
17/1 (the same fails on the base exe: flat prone's crawl/jump cases and crawl's "stick standing", pre-existing).

## hullcull: culling buried brushes from the hull trees gains nothing on vrstart; the exact version measured and reverted (branch agent/hullcull, 2026-11-26)

His OK: build the collision trees faster by not building what hullspeed's fold throws away. **Result: nothing to cut.**
vrstart's hull 0 solid leaves are short cells on the surface, not deep solid, so a cull based on buried solid finds
almost nothing. Nothing ships: the code is in history only (commit 0f56f35f5, reverted by the next commit; the diff is
also in `scratch/hc_cut_experiment.diff`). treeVersion stays 2, so the shipped cache doesn't change.

**What was measured** (PC, Release no-LTO, cold, `vr_hull_cache 0; vr_hull_defer 0`, other jobs running beside it):
- vrstart: 20,698 world brushes (24,736 per tree with the clip and hole brushes). Mean height **73 units**. **20,269** of
  them are open right down to their bottom: some face reaching their lowest point has air or water just past it. **18**
  are shut on every face (the world's margin counts as shut). e1m1 is the same: 1,181 of 1,208 failed on an open face.
- Trees (8): 525,833 to 610,633 nodes, 285,635 to 396,911 folded per tree (about 40% of the nodes before the fold),
  14.6-14.9 s summed. These are the same counts hullspeed reported.

**Tried, in order (both exact, both checked by the new point test):**
1. The brief's cull. Leave out a brush when every face is covered by another world brush's solid (a 1/16-unit slab past
   each face, walked down hull 0, every piece inside its leaf's brush), no box fits inside it, and none of its neighbours
   is left out. Then any box reaching into it also reaches into a kept neighbour. Result: **0 brushes** left out of any tree.
2. The general form: grow only the parts a box can reach open space from. A grown point outside the solid S has its box
   crossing S's open surface, so the solid it touches lies within 2B of that surface. Below the lowest open face in reach,
   minus 2B, a brush can go in ungrown. Each brush was cut there (heights rounded down to 32): the upper part grown, the
   lower part as it is, a brush with nothing open in reach not grown at all. Result: **9 brushes cut and 1 not grown** per
   tree. Nodes moved by -0.7% to +0.9%, fold counts by under 1%, and finding the open faces cost 95-114 ms per cold load.
   Not kept.
- Not tried: stopping a split early when a node is "solid-enclosed". The folded nodes are overlaps between neighbouring
  surface cells' grown copies (a band one box wide on each side of their shared faces, mostly in solid). The only exact
  test I found needs each node's region walked down hull 0. That means carrying the region through the pool's units and
  merge (`speculate`/`emit`) and keeping the bytes identical to the one-thread build, which is more than this task's
  budget. The real remaining lever is vrstartgeo's: fewer terrain faces (its ranked items 1 and 2).

**Test (in the reverted commit)**: `vr_hull_culltest [points]` (Debug > Tests "Check Buried Brushes"). For each server
tree it builds a reference from every brush grown, then compares 100,000 random points (half anywhere in the world's
bounds, half around a random brush, all inside the world's margin) and 10,000 random moves from them. A move that starts
in solid compares startsolid only, because Quake's allsolid/fraction for such moves depends on the tree's splits, not on
the shape. A difference within 0.1 units of the reference's boundary counts as "on a plane". vrstart: PASS, 8 trees,
0 differences (moves 0-2 on a plane per tree). ad_sepulcher could not be run: neither kit base has it now ("Couldn't spawn
server maps/ad_sepulcher.bsp" on qbase and qrp). hullspeed's numbers stand for it.

## vrstartsimp: the hub's open ground and cliff faces simplified; cold collision about 8-11 % cheaper (branch agent/vrstartsimp, 2026-11-26)

His OK ("Try it") for vrstartgeo's two visible options. `vrstart_gen.py`: `ISLAND_COARSE_INLAND` 200 (the island's open
ground more than 200 units in from the coast on the 128-unit lattice; 0 = the 64 lattice everywhere, as before) and
`CLIFF_RING_POINTS` 180 (the cliff face's six rings; 260 before). The zones', paths' and ravine's outlines keep their own
points either way. Regenerated and compiled with the final preset (695 s; light 666 s). bsp_holes: 2 unlisted underwater
faces (the old build had 3), no rays run.

| | before | after | change |
|---|---|---|---|
| .map brushes (terrain prisms) | 10,450 (9,069) | 8,612 (7,231) | -18 % (-20 %) |
| hull 0 solid leaves | 20,694 | 18,592 | -10 % |
| hull 1 / 2 clipnodes | 82,974 / 69,109 | 76,887 / 63,866 | -7 % / -8 % |
| hull 0 nodes, world faces | 53,407, 72,755 | 49,146, 67,679 | -8 %, -7 % |
| .bsp size | 17.9 MB | 12.8 MB | -29 % |
| in game: recovered brushes | 24,772 | 22,545 | -9 % |
| in game: tree nodes (8 trees) | 526k-611k | 468k-546k | -11 % |
| cold trees' compile time, summed (median of 3, A/B alternated) | 18.4 s | 17.0 s | -8 % |
| cold load's wait for the trees (median of 3) | 4.8 s | 4.3 s | -11 % |
| hull cache written (`cache/hulls`) | 114 MB | 103 MB | -10 % |

The terrain alone (`--only-terrain`, qbsp only) reproduces vrstartgeo's measurement exactly (15,959 solid leaves and
60,572 clipnodes: -23 % / -20 %), but in the full map the structures cut the terrain differently and the gain is about
half. Times: PC, Release no-LTO, `vr_cache_shipped 0; vr_hull_cache 0; vr_hull_keep 0; vr_hull_defer 0`, the old .bsp
loaded as `vss_old` beside the new in alternated runs (not exclusive: rough).

**Looks:** flat screenshots from 8 fixed spots (open ground from a hill, across the grass, from above; arrival; terrace;
north shore; cliffs from the water and from the top), same lighting: hard to tell apart (mean pixel difference 0.5-5.9
of 255; the arrival's hillside shading moved most). Pairs and montage in the worktree's `scratch/vss_*`. His call.

**Function:** hub_start_test 7/0, startgun 16/0, waterzap (vrstart, flat and mock) 8/0, flatvronly 5/0, mp_coop with
the hub as start map (`QVR_MP_STARTMAP=1`) 12/0, `vr_hull_keeptest` PASS (8 trees). Navmesh (a grunt spawned): class 0
builds, 1,024 tiles, 16,534 polygons (the old map 256 tiles, 1,279: not looked into); `vr_nav_cliptest` and
`vr_nav_toptest` check 0 polygons on vrstart before and after (they report FAIL on both maps). Walks: the hub's path
(`vrstart_walktest.py`) 18/18 before and after; 113 legs of 256 units over the open ground (a 256 grid more than 250
units inland): reached 36 before, 41 after, stopped short (obstacles, slopes) 62 before, 52 after. One point snag after
at (-2, 371) on a 1:4 slope (16 units either side walks through); the old map had one at (31, 267) instead.
`vrstart_walktest.py` now sets `cl_alwaysrun 1` (its frames assume running; walking, 16 of 18 legs fell short).

After the ship: the relit copy and the shipped cache need regenerating (and his `quakevr/relit`).

## vsnav: vrstart's navmesh counts explained; the old map's mesh had lost the island (branch agent/vsnav, 2026-11-26)

vrstartsimp reported 1,024 tiles and 16,534 polygons on the new hub against 256 and 1,279 on the old. The new count is
right; the old mesh was broken. Same exe (Release no-LTO), `vr_nav_async 0; vr_nav_cache 0`, a grunt spawned, class
r12 h56:

| map | brushes (clip) | bounds (Quake units) | grid | tiles, polygons | build | reachable from the spawn |
|---|---|---|---|---|---|---|
| old, before | 20,839 (141) | x -30,728..4,096, z -32,893..31,820 | 69 x 52 of 128 voxels | 256, 1,279 | 610 ms | none (all at z -16,510) |
| old, fixed | 20,839 (141), 1 cut | -4,096..4,096, z -1,184..2,464 | 32 x 33 of 64 | 1,024, 16,575 | 577 ms | 7,539 |
| new (unchanged by the fix) | 18,707 (113), 0 cut | -4,096..4,096, z -1,120..2,400 | 32 x 32 of 64 | 1,024, 16,534 | 606-668 ms | 7,466 |

- **Why:** one clip brush vr_hull recovered on the old map reached about 30,000 units out (an open leaf beyond the sealed
  box). The mesh's bounds took it in: the grid doubled its tiles (12 bits of tiles), and Recast's spans hold 13 bits of
  height (16,382 units at 2 a voxel), so everything more than 16,382 units above the bounds' bottom, the whole island,
  was cut off; the 1,279 polygons lay on that brush's bottom at z -16,510 (9 of them inside it: the cliptest failed).
  The new map happens to have no such brush.
- **Fix** (vr_nav.cpp `triangulate`): a brush whose faces reach more than 128 units past the world model's bounds is
  cut to that box (its planes get the box's six, in the set too so `fillSolids` agrees; those faces never walkable).
  On maps with none (vrstart, e1m1, e4m7: 0 cut) the input, its hash and the mesh are the same as before, so their
  disk caches stay valid. A map taller than Recast's reach now warns at load.
- **The 0-polygon tests:** `vr_nav_cliptest` and `vr_nav_toptest` check only classes that are ready. vrstart spawns no
  walker, and a spawned one's class lands on a later frame (async, NAVMESH.md 20): run straight after the spawn they
  saw no class. Not a bug in the tests or the mesh; they now say "no class ready" when they check nothing, and
  NAVMESH.md 8 gives the recipe. With a class ready: new vrstart cliptest PASS (16,717 polygons, 0 inside 113 clip
  brushes), toptest PASS (worst 16.0); old vrstart fixed PASS / PASS; e1m1 PASS / PASS.
- `vr_nav_info` also prints the mesh's bounds, its grid, the brushes cut, and each class's polygons reachable from the
  player's feet through the mesh's links (e1m1 195 of 2,921: its start room is shut by doors; vrstart about 45 %:
  the rest are roofs, cliff ledges and the sea floor).
- Checks: `arenapath_test.sh` (vr_arena_test 36) 16/0. Open: BACKLOG section 4, "Brushes beyond the world".

## clipstray: recovered clip brushes stay inside their bounds (branch agent/clipstray, 2026-11-26)

vsnav's 30,000-unit clip brush on the old vrstart, found at the source (`recoverClips`, vr_hull.cpp). Two faults, both
in the clip brushes vr_hull rebuilds from hull 1's solid leaves (they feed the player's trees, `boxInTree`, the holes
and the navmesh's `fillSolids`):

- **A sliver that lost its faces** (old vrstart, hull 1 leaf 96761): the piece reaching that leaf was two triangles 0.2
  units across on two nearly opposite planes; every other face had gone to `splitPoly`'s epsilon (a face is kept only on
  the sides its points are on). Two planes bound nothing: the brush made of them was a slab reaching the bogus winding's
  end (faces x -255,000..17,000 at Quake's box). The room test still called it a clip brush (its points a unit in from
  its faces lie outside a 0.1-unit piece). Fix: a clip piece whose planes, at Quake's box, make faces reaching more than
  half a unit past the box its points span gets that box as faces too (as hull 0's brushes get axial bevels) and is
  looked at again: here a 0.1-unit brush, harmless.
- **A thin wedge's moved faces** (both vrstarts, hip1m1, hip2m3, r2m3, AD): the faces lying against more of hull 1's
  solid move with a shorter box (growsZ); two floor-like faces meeting at a shallow angle move by different amounts and
  their corner runs along their edge: the shipped vrstart's brush 18658 reached 5,500 units through the map in the prone
  box's tree (y 1,045..6,688 for bounds 1,001..1,206), ad_tears' worst 540. The sweeps meet a clip brush only within its
  bounds (`leavesOfBox`), the trees met all of it. Fix: where its faces at the widest lowest box (32 wide,
  `lowestBoxHeight` 12: every face at its farthest) reach past its bounds, the bounds' six planes are added; trees,
  sweeps and the navmesh now agree.

Survey (`vr_hull_strays`, new in vr_hull_diag.cpp: brushes reaching more than 128 past the world's box, and clip brushes
past their own bounds with the worst overreach; Release, `vr_hull_cache 0`):

| map | past the world (before) | clip past own bounds (before, worst) | after | brushes hash |
|---|---|---|---|---|
| old vrstart | 1 (2 planes, 259,000 units) | 14 (259,316) | 0, 0 | changed |
| vrstart | 1 (6 planes, y to 6,688) | 7 (5,482) | 0, 0 | changed |
| hip1m1, hip2m3, r2m3 | 0 | 2 (72), 3 (49), 1 (44) | 0, 0 | changed |
| ad_sepulcher, ad_tears, ad_swampy | 0 | 26 (220), 156 (541), 20 (386) | 0, 0 | changed |
| vrtutorial, vrcalibration, e1m1-e1m8, e4m7, hip3m4, r1m1 | 0 | 0 | 0, 0 | identical |

- `buildVersion` 6 -> 7 (the brushes' bytes change on the maps above; the file layout, `brushesVersion`, does not).
  Clip brushes' build time unchanged (vrstart 190 -> 203 ms, ad_tears 356 -> 346: noise).
- Old vrstart's navmesh now cuts 0 brushes (vsnav's safety net stays).
- Tests: `vr_hull_keeptest` vrstart PASS (8 trees); `vr_hull_holes_test` vrbrushholes 29/0; `vr_hull_cache 2` (keep 0)
  vrstart + hip1m1 15 the same, 0 different; `vr_nav_cliptest` / `vr_nav_toptest` (NAVMESH.md 8 recipe) PASS on vrstart,
  old vrstart, e1m1; lowceil_test 16/0; prone_test 26/6 (the 6 flat fails known, ROUND26 line ~398).

## shipcachefix: the shipped cache is the same files from any exe on any PC (branch agent/shipcachefix, 2026-11-26)

- **Cause (beta6 dry run: the dev Release exe gave 16 files 156.4 MB, the packaged ThinLTO exe 15 files 151.7 MB):**
  every product's write was gated on how long it took on THIS PC, and the generator's maps' step (`-preparemaps`) never
  set the "write everything" switch the installer's hulls step uses. vrtutorial's light grid took 122 ms with the
  no-LTO exe (bar `GRID_CACHE_SECONDS` 0.1 s, vr_ambient.cpp): written; under 100 ms with ThinLTO: left out. vrtutorial
  asks for the same 8 boxes in every run (5 player boxes, the monsters' 24x56, 28x56, 40x88 deferred after the load),
  all 11-42 ms to compile against the trees' 25 ms bar (`diskMinMs`), so which ones were written was chance
  (8_8_18 vs 14_14_28 in the two lists; a third probe run wrote 12_12_28, 8_8_26, 8_8_28). vrstart's trees all take
  seconds: always written, hence its 9 identical files. The navmesh (`navmesh` in the products) was built on the pool
  but written only at a monster's first path: none came in the 3 s settle, so no map ever shipped one.
- **Fix:** `-preparemaps` sets `hull::setWriteEveryTree(true)` and the new `ambient::setWriteEveryGrid(true)`; before a
  map's "ok" line the preparation run calls `hull::finishLoads()` (the load's and the deferred monster trees joined
  and written) and the new `nav::finishBuilds()` (each size class's build joined, its file written). The installer's
  full preparation run gets the joins too (a no-op in effect: the next map's load joined them anyway).
- **Proof:** make_shipped_cache.py run three times on this tree (Release twice, ReleaseLTO once, the LTO run alongside
  the second Release run): 30 files, 169.0 MB (161.2 MiB, the script's figure), all three byte-identical, index.txt
  included. New files over beta6's dev list: vrcalibration's 5 player trees and light grid, vrtutorial's 20_20_44,
  8_8_18/22/26/28 trees, both navmeshes (vrstart 3.3 MB, vrtutorial 0.8 MB).
- Files: vr_prepare.cpp, vr_ambient.cpp/.hpp, vr_nav.cpp/.hpp, vr_hull.hpp (comment), make_shipped_cache.py (doc),
  CACHES.md ("The shipped cache"; its "What ships" figures are beta5's, superseded by the line added above it).
- Open: which monster boxes a map asks for still depends on its spawns at the run's skill (vrtutorial skill 0, vrstart
  the default); a player reaching the hub at another skill may compile an unshipped box on the Quest.

## questb6: the Quest 3's first hub load with beta6's files (branch agent/questb6, 2026-11-26)

Measure only (his OK "Measure on Quest"). Method as loadprof2: this tree's dev APK (origin/next 40f4c0bdd, beta6's code
8d7714c6 but its version bump; debug key), the beta6 package's game data synced as Setup does (24 files differed:
progs.dat, vrstart bsp/lit/lux, maps, scenarios, and the 30-file `cache_shipped`, which replaced loadprof2's 15), unworn,
`-unattended -nostartmap -noconfigwrite`, a fresh empty `vr_cache_dir` per launch (every player-cache product cold), the
GL programs warm (program cache read, 100). `developer 1`, then `map <map>`; command to first frame (vr_startup_times):

| load (cold player cache) | loadprof2: no shipped cache, trees waited | loadprof2: shipped + defer | beta6's files |
|---|---|---|---|
| vrstart (the hub) | 92.1, 90.6 s | 19.2, 20.5 s | **16.9 s** median (16.94, 15.58, 17.63) |
| vrtutorial | - | - | **13.8 s** (one launch) |

- **Collision: no wait.** hulls 9 read, all shipped (vrstart 102.6 MB; the trees 0.28-0.34 s), 0 compiled, 0 written;
  "the map's brushes and compiled hulls, waited for" 0.0 ms: the 5 player trees hullwait waits for are shipped reads.
  No crouched trace waited after the first frame (an unworn headset is "crouched" at once: 28 s in loadprof2 without
  the shipped cache). The hull job runs beside the spawn, off the critical path.
- **Navmesh shipped:** 2 read (1 shipped: vrstart's, 12 ms), 0 built; "VR after load: navmesh" 0.25-0.31 s (vrtutorial
  15 ms); the props' 17 tiles are still rebuilt on the pool after the load (0.2 s).
- **What is left is the cold models:** "server: entities spawned" 10.3-10.8 s of vrstart's 16-17 s (vrtutorial 10.4 of
  13.8): per launch 556 dequantised poses, 62 ragdoll rigs and 13 gun pieces built, ~250 occlusion bakes (on their task,
  after the load), all keyed by id models, so never shipped. Then R_NewMap 1.1-1.7 s, sounds 0.4-0.9 s, the renderer
  prewarm ~1.2 s. The light grid is not used on the Quest (0 read, 0 built, 0.5 ms).
- **Headset quirk:** a direct `adb push` into the app's folder now fails ("remote fchown failed: Operation not
  permitted") and adbd then unlinks the target, so the old file is gone. Push to /data/local/tmp, then `adb shell mv`
  into place (scratch qb6_sync.py `put`); a folder adbd made is left drwxrwx--- (chmod 777 it). The game-made
  `quakevr/doom` folder is not writable by the shell (its 6 Freedoom files were not synced; not in this measurement).
- **Restored:** the APK the headset had (1.2.0-beta5, pulled from it, `install -r -d`) and the 9 data files the sync
  replaced (progs.dat 8242a8d4, vrstart's maps...): the headset held beta5, not beta2, and beta2's re-signed APK is gone
  (its parked folder no longer exists). Beta6's `cache_shipped` stays (beta5's vrstart keys miss it: harmless).

## questpush: Setup's Quest copy never loses a file (branch agent/questpush, 2026-11-26)

- **The risk (questb6's finding, read in adbd's sync code):** adbd unlinks a push's target before writing it, and when
  the push then fails ("remote fchown failed: Operation not permitted" on questb6's Quest 3, a pulled cable, a full
  disk) the target stays unlinked. Setup pushed straight onto the files: a failed push lost the headset's copy of a
  shipped file it was replacing (progs.dat, a map, a pak: the game broken until a copy that works), and an fchown failure
  stopped the whole install with "Copying to the Quest failed" (so every later run failed the same way). Saves and
  configs were never at risk (never shipped, so never pushed onto); the size check after did not see a same-size file
  left as its old copy.
- **Now (`QuestInstaller.Push`):** every batch (same batching: several files of a folder a push, 256 MB) goes into
  Setup's staging folder `files/.quakevr-setup` (made, chgrp ext_data_rw, 2777 like the others), then one shell line per
  batch moves each file beside its target (`.name.qvrpart`) and renames it over the target, printing a marker per file
  placed. The target is only replaced by a complete copy. If a push into the app's folder fails (anything but transient
  or a full disk: adbd then unlinked only the staged copy), that batch and every later one go through
  `/data/local/tmp/quakevr-setup` (one warning in the log); the move from there is a copy (another filesystem), still
  into the `.qvrpart` name, so a copy cut short leaves the target as it was (the half copy is removed). Both staging
  folders are removed before and after every copy (`rm -rf` of the two constant paths); those and `.qvrpart` are the
  only things Setup ever removes.
- **Reports:** a file not moved in counts as not arrived (even at the right size); a full disk says so, a failed push or
  move says "Nothing on the Quest was replaced or removed" / "Those files are as they were". A folder the game made itself
  (quakevr/doom: the move is refused, "Permission denied") still only has its files listed as not copied (the Done
  page's warning), the others go on.
- **Cost:** one extra shell command per batch and a rename per file; through /data/local/tmp, a copy on the headset per
  file (flash speed, well above USB). Not measured on the headset (he was testing on it); not tried there yet.
- **Tests (Installer self-tests, `quest` filter, 11/11):** new "adb refusing the app's folder" (the fake adb now unlinks a
  push's target like adbd and, with `FchownFails`, fails "remote fchown failed" in the app's folder; it proves a direct
  push loses the file, then: a release with two changed files arrives through /data/local/tmp with one failed try, saves
  and configs kept; a disk filling during the move from /data/local/tmp keeps the old progs.dat byte for byte; a game
  folder through /data/local/tmp is reported, the rest copied). `NothingRemoved` now checks every `rm` and every moved-away
  path is Setup's own and no staging or `.qvrpart` is left. Files: `Installer/src/QuakeVR.Installer.Core/Quest/QuestInstaller.cs`,
  `Installer/tests/QuakeVR.Installer.SelfTest/FakeAdb.cs`, `QuestTests.cs`, `Installer/README.md`.

## logo1126
Shipped-asset matches (premultiplied mean diff vs art): menu_banner.png = vertical_logo_pixelated_0 (1.8; NOT very_pixelated_0, 17), scaled Lanczos to 2048 tall; Windows/QuakeVR.ico, Installer app.ico, docs/images/quakevr-unleashed-icon.webp = qvru_icon_0 (0.8/0.8/2.0); installer logo_square.png and flatpak icon-512.png = square_logo_0 (3.4); installer logo_wide.png = horizontal_banner_0 (2.0). TrenchBroom Icon.png unmatched; no Android icons in repo.
Updated: menu_banner.png <- vertical_logo_very_pixelated_1 (make_menu_banner.py now scales to fit, as the shipped one was made), both .ico and the docs webp <- qvru_icon_1. Left for him: logo_square, flatpak icon-512, logo_wide (square_logo_1 / horizontal_banner_1 exist).

## logo1126c
Remaining old-art copies replaced from art/: docs/images/quakevr-unleashed-square.webp <- square_logo_1, -wide.webp <- horizontal_banner_1 (webp q90), -vertical.png <- vertical_logo_pixelated_1 (all same size as before; matched pixelated_0/square_logo_0/horizontal_banner_0 at diff 0.0/0.4/0.3). README uses the wide one. Installer logo_square/logo_wide and flatpak icon-512 were already _1 (logo1126b). Unchanged: Misc/trenchbroom/QuakeVR/Icon.png (32px headset icon, not the art), no Android icons, no vertical_banner_0 copies in the repo. No game/installer code touched, so no build needed. Compare: scratch/logo1126c_compare.png.

## flatloadbar: the loading panel through the whole map load, flat and VR (branch agent/flatloadbar, 2026-11-26)

His flat `map vrstart` took 13.3 s with nothing on screen (entities 6.2 s, the model light grid rebuilt 5.1 s).

- **What it does:** once a map load has run 1 s (`showAfter`), the same panel as the shaders' and hullwait's (one code
  path, `vr_shaderload.cpp` `mapPanelFrame`: the window's blit, the headset's quad layer, the early VR start) shows the
  stage the load is in, in player words: "Spawning the map's monsters and items", "Preparing the monsters' limbs",
  "Lighting the models" (note "built once for each map version"), "Loading models", "Loading sounds", "Preparing your
  hands and weapons", "Preparing water, slime and lava", "Preparing effects", "Preparing textures and lightmaps", "Loading
  the map's geometry", "Loading the game code", "Settling the physics"; else "Loading <map>". The bar: the stage's own
  count where it counts (`VR_LoadCount`: entities n of N, models, sounds, the light grid's rows as a %), else the share of
  the load's stages done (`VR_TimeMark`s; the whole is the last load's count, 60 before one). The line under it: the map
  and the count ("vrstart: 91 of 181 entities", "step 12 of about 55"); the footer the seconds. A load under 1 s shows
  nothing; `vr_load_panel 0` turns it off (and the collision wait's panel), as before.
- **How:** `qvr::loadbar` (vr_shaderload.cpp) follows the load's stages from the existing hooks whether or not the
  start-up trace runs: begin at the load's command (`VR_TimeLoadCommand`) or spawn (`VR_TimeLoadBegin`), end at its first
  frame drawn or its failure (`VR_TimeFrameEnd`); `VR_StepBegin`/`VR_StepEnd`/`VR_TimeMark` push and pop a small stage
  stack (8 deep). The panel is drawn from the main thread inside the long work, at most every 100 ms (`drawEvery`): each
  stage's start and end, each entity (`ED_LoadFromFile`, the count from the entity text's braces), each model and sound
  precached, each model file loaded (`Mod_LoadModel`: `VR_LoadPump`), each light-grid row the main thread finishes.
  `toWindow` now puts back the GL state it changes (viewport, scissor, clear colour, framebuffers): it runs mid-load.
  New nested step "server: monsters' limbs and hulls (VR spawned)" (`VR_OnSpawnServerSpawned`, ~0.7 s of vrstart's spawn).
- **The light grid a row a chunk** (vr_ambient.cpp `gridBuild`): its two `parallelFor`s were a layer (z) a chunk; the
  main thread then waited seconds for the last few uneven layers with no frame (a 4.4 s gap). Now a row (z, y) a chunk,
  the entries merged in the same order: the cached grid file is byte-identical (vrstart's `.mlg` md5 the same), and the
  build went from 5.4-6.3 s to 3.1-3.4 s on the test machine. `vr_model_light_grid_test` 25/0 (e1m1). developer 1 prints
  "model light grid: probes N ms (M of R rows on the main thread), clear cells N ms".
- **Cost** (cold vrstart, Release no-LTO, a loaded machine): 56-99 frames in 11-16 s, 66-163 ms drawing them in all
  (0.5-1.1% of the load; 1.1-1.8 ms a frame); the end line says so: "loading panel: vrstart's load, 12.1 s: 81 frames (0
  in the headset), 116.0 ms drawing them, the longest gap 0.81 s (ending in ...)"; developer 1 also names each gap over
  0.5 s. A/B wall times are noise here (11.6-18.7 s either way). Longest gaps left: 0.5-0.9 s inside the entity spawn
  (one slow entity), and the collision wait counts as a gap (it draws its own frames).
- **Test aid:** `vr_load_panel_shot` (Debug > Tests "Save the Loading Panel") now also shows every load's panel from its
  start and saves `screenshots/load_panel_<entities|lightgrid|models|sounds>.png` when that stage's count reaches the
  share (the collision wait's `load_panel.png` and its reset to 0 as before). Scene `load_panel_stages` (vrstart with it).
- **Runs:** cold vrstart (fresh `vr_cache_dir`, `vr_cache_shipped 0`), mock VR and `-flatscreenmode`: the four shots saved
  at 50% each; the collision wait's own shot still saved (`vr_hull_keep 0; vr_hull_cache 0; vr_hull_build_mb 300`: "Building
  collision for crouching (5 of 6)"); perfload_test: 0 models loaded in play, vrfiringrange's 2.4 s load 13 frames, 3.4 ms.
- **Not the panel (found while measuring):** in 7 of 22 cold flat vrstart loads (panel on or off: 1 of 7 with it off) the
  light grid's probe pass ran entirely on the main thread, 46-58 s instead of ~3 s: no pool helper ever joined that
  `parallelFor` (the clear pass right after it got helpers in 50 ms). Seen with the old layer chunks too. Also in 1 of 4
  mock-VR loads (54 s). A pool issue (helpers queued behind something, or not posted) for a follow-up; the panel kept moving.
- Not covered: the start-up before the first frame (the shaders' panel covers its slow part), a save game's load
  (no stages hooked), a client's connect to a remote server past its precache.
## logwarn1126: his flat session's console warnings (branch agent/logwarn1126, 2026-11-26)

His log `quakevr/logs/2026-11-26_17-50-24/console.log` (VS2026, flat, `-basedir <Steam Quake> -game quakevr -flatscreenmode`).

- **vr_flat_grab_hold "says QVR_PREF but is not sent"**: the server never reads it (the client picks the impulse it sends:
  96 Toggle, 97/98 Hold, vr_flatgrenade.cpp), so it is `QVR_OWN`, not a preference to send. New qvr_check `client_prefs`
  (Tools/qvr_checks/small_checks.cpp, in the build's `all`): every `QVR_PREF` row of vr_cvars.inc is in vr_clientprefs.cpp's
  prefNames and every prefNames cvar says `QVR_PREF` (31 settings + 3 tunables today). The start-up warning stays as a
  runtime backstop.
- **"ragdoll: progs/mon_enforcer.mdl is not the model its seed table was made for"** (and mon_zombie, mon_hknight,
  mon_knight): his game loaded **Dwell's** loose `dwellv2p2/progs/mon_*.mdl` (other meshes: 340/265/393/192 vertices;
  Dwell's knight 97 poses against AD's 163) over **AD's** `ad/pak0.pak` ones. Both folders are mounted in every campaign
  (`skipOutside`), Dwell's after AD's, so Dwell's won outside the AD campaign. Not only no ragdolls (the death pose stayed):
  AD's defender/eliminator (armed replacements), knights, hell knights and mummies drew Dwell's meshes with AD's frame
  numbers, and AD's Widowmaker VR model (made from AD's v_shot3.mdl) was not made. Fixed at the cause, as Quoth already
  does: `dwell_data.py` now lists Dwell's `progs/` files AD also has (owner 3: skipped in Dwell's folder while
  `vr_ad_status` is 1), except `progs/g_shot3.mdl` (Dwell's Rotary Shotgun pickup: KEEP). Dwell's skies, menus and
  start.lit stay its own; its own v_shot3 is still read in place (owned path). vr_dwell_data.inc: 78 names (id1 61,
  rogue 3, ad 14). Checked: `vr_ragdoll_info` on the four in e1m1 with Dwell installed: 222/177/250/268 vertices, 12 bones.
- **The paks opened 3-4 times** at start-up: Ironwail's mod list (`Modlist_FindLocal`, host_cmd.c) mounts each base-dir
  folder in turn to read its description (add, read, reset); its probe of `quakevr` runs our hook, which mounts hipnotic,
  rogue, ad, dwell and quoth; then `-game quakevr` mounts them for real. The final search path (`path`) has each folder
  once: left as is (the reopens are sub-millisecond).
- **"made in the map package map package 8297cb7f"**: an unknown package's fallback title is "map package <id>"; the
  sentences that already say "the map package" now use `packageName` (the id alone then). vr_continue.cpp.
- **Also found**: "VR language source" said twice for the rerelease's pak0.pak (3359 strings: every entry twice):
  `ownedRoots` listed the same folder by two spellings (Steam's install path and `-basedir`'s `<Quake>/rerelease`). Roots
  are now unique by `files::resolved` (case aside on Windows), the highest place kept. vr_gamedir.cpp.
- Tests: dwell_owned, dwell_vrmodels, loading_switch, flatgrabthrow; a flat start-up in the kit shows none of the lines.
## gridstall: a parallelFor started while every gate was taken never got a helper (branch agent/gridstall, 2026-11-26)

flatloadbar's finding: in 8 of 26 cold vrstart loads the model light grid's probe pass ran entirely on the main thread
(46-58 s instead of ~3 s), no helper ever joining, while the clear pass right after got helpers within 50 ms.

- **Cause** (reproduced: 1 in 6 cold flat loads, then the cells pass 4 times in 14 with the gate count in the line):
  Zancle's `parallelFor` takes one of 64 "gates" (`ParallelForSlots`) per call under way with helpers; a call that found
  none ran every chunk on its caller and never looked again. During a cold vrstart load the hull's recursive two-way
  `parallelFor`s ("hull speculate", vr_hull.cpp, a call per recursion level on every worker), with the trees' and
  ao's, held all 64 at once for a moment: every stall line said "64 of 64 gates in use at its start", while a second later
  only 5-14 of 31 workers were busy and 12-41 gates in use. The probe pass that started in that moment ran alone to its
  end. Not lost wake-ups, not helpers queued behind long jobs (0 tasks queued, the 3-44 helpers queued were running).
- **Fix** (`ParallelFor.hpp`, a local Zancle change listed in its README): a call whose helpers are missing (no free
  gate, or the queued-helper budget used up) tries again after each of its own chunks, taking the gate and posting its
  helpers as soon as one frees; the check is a load or two and only while some are missing. Every pool user gets it
  (hull, ao, normal maps, Box3D's steps' calls, the Quest's fewer workers).
- **Diagnostics kept**: each job keeps where it was posted (file and line, every build: before, profiling builds only);
  `vr_jobs_info` ends with the pool now ("pool: N tasks and M helpers queued, G of 64 gates, B of W workers busy:
  vr_hull.cpp:4843 x1 (13.5 s), hull speculate x11 (1.2 s), ..."). With developer 1, a split call whose caller has run
  alone over a second says so with that line and the gates in use at its start, then how it ended (main thread's calls
  only: the console is its).
- **Proved**: 33 cold vrstart loads after (22 flat, 11 mock VR, fresh `vr_cache_dir`, `vr_cache_shipped 0`): 0 stalls, 0
  stall lines; the main thread ran 153-2955 of the 27432 probe rows (before: all of them in a stall). The probes took 9-11 s because the
  author's game ran meanwhile (low-impact runs: 8 CPUs, idle priority; 3 s before, on the whole machine, as flatloadbar's
  3.1-3.4 s); the "< 6 s" check needs a full-machine run. Every load's `.mlg` the same bytes (md5 bdf21622, before,
  stalled and after). `vr_jobs_test` 15/0 with a new check: a call made with all 64 gates held gets helpers once they
  free (80 of 100 chunks by helpers; before the fix, 0). `vr_model_light_grid_test` e1m1 25/0, `vr_hull_keeptest`
  vrstart PASS (8 trees), `vr_physics_portal_sleep_test` 12/0 (Box3D on the pool).
- Left: the hull's speculate recursion still takes a gate per level (a call there that finds none runs alone until one
  frees); a bound on its depth would leave gates for the rest. Not done (the hull's own tuning).


## box3dup3: Box3D updated to upstream 042be58 (branch agent/box3dup3, 2026-11-26)

**Update** (`Quake/vr/external/box3d`, README.md and CREDITS.md name the commit). 1767150d (2026-11-17) to upstream main
042be58 (2026-11-25): 4 commits, re-applied by 3-way merge as box3dup did (`git merge-file`; upstream in
`F:\QVR-Parked\box3d-upstream`). What matters to us:

- #236 "Motion locks are now joints": `b3BodyDef::motionLocks` and b3Body_SetMotionLocks are gone; `fixedRotation`
  is back (locks every turn) and a new **motion lock joint** (`b3CreateMotionLockJoint`: linear x/y/z locks, an angular
  lock leaving only frame A's z turn) does the rest. Our only use, the pushable corpse (`vr_corpse_collide` 2 or 4):
  the box (2) is `fixedRotation`; the fitted one (4, turning about its yaw alone) is held upright by a motion lock
  joint to a new shapeless static body, `World::anchor` (made with the world, `vr_box3d.cpp`; `vr_box3d_props.cpp`).
  Also #236: the manifold allocators are a fixed array of `B3_MAX_MANIFOLDS`, empty until used; recording major 7.
- #237 "Bug fixes": the contact solver's overflow path restructured (the friction centre now weighted by each point's
  separation, a spinning-top drift fix), a **spherical joint's motor velocity is in joint frame A**, no longer the
  world's (rotated with body A through the substeps). Our only non-zero use, the active ragdolls' drive
  (`vr_ragactive.cpp`), now passes `inverse(jointFrameA) * spin` (spin is in the parent's, body A's, axes; was
  `parentRotation * spin`). Shape and solver fixes besides.
- #238: b3CreateHull fails (NULL) on degenerate input it used to accept: a hull thinner than the larger of
  `B3_LINEAR_SLOP` (5 mm, ~0.2 units) and 0.1% of its extent, or a broken quickhull merge; every caller already handles
  NULL (a box instead). Other hulls are the same bytes: `vr_gun_pieces_cache 2` read 11 pieces, 11 same, 0 differed,
  so no cache bump (`cutVersion` stays 1; the rig cache does not use Box3D). The contact solver: 12 lines.
- #241 "Vehicle tech": a new tire joint (`b3CreateTireJoint`, ray/shape-cast wheels; experimental). Unused.

**Local changes**: all kept (none is upstream's now). The long manifold arrays: kept on the new fixed allocator array
(`b3World_GetManifoldMemory`'s `pools` now counts the allocators holding blocks). The elliptical cones' recording
version is now **8** (upstream took 7). b3CreateHullWith's allocator hooks re-applied to the new failure paths.

**Tests** (Release, the merged tree; the same checks on the base origin/next 0f6fa56b3 built in this tree):

| check | base | new |
|---|---|---|
| climb_test pull, seeds 7-10 (24) | 21/24 (knight 9, 10; hknight 8) | 19/24 (+ grunt 10 "slowly in", knight 7) |
| kdstir | 5/0 | 4/1: six 2.5 s knockdowns, one get-up 0.80 s later with stirring than without (limit 0.3; mean 0.03) |
| ragdoll_stagger | 2 FAIL (monster 0 backpedal; monster 2 "all 8 recovered") | 2 FAIL (monster 0 backpedal; grunt "its spot taken") |
| active_test (45 cases) | no fails | no fails; the drive follows the death animation closer (grunt err 13/15/31/23 -> 12/8/19/10 deg) |
| axe_stick_rates (8 kinds x 60) | 60 50 62 50 67 70 7 2 % | 53 62 57 68 78 72 3 0 % (within the noise of 60 throws) |

The new build is deterministic run to run (the same lines in two runs). The two climb cases and the kdstir get-up are
outcome flips in chaotic cases (contacts on the ground and the lip under #237's friction centre change; the knight's
hold was already "chaotic: seeds swap run singly", ROUND25 box3dup2), not crashes or energy: **open**, not tuned. New only: throw_spin 15/0, held_over_props PASS, throw_grace clean, magthrow 3/0, gib_2h_throw 13 of 28
bursts (was 9-11), embedhold 2/0 (flatembedwalk 60/0), ragdoll_test runs clean, selfcollide clean, selfchecks PASS
(physics workers). vr_physics_mtbench: the same hash for every worker count, now 200/300 and 400/300
`adf2a36dbfa46191` (was `b071f169ad1d96cb`: the solver changed). Pushable corpses (`scratch/box3dup3_corpse.sh`,
vr_corpse_collide 4 and 2, a dead grunt flung 300 units): tilt 0.00 throughout (vr_corpse_list prints a corpse's tilt
now).

**Perf**: no measurable change (Release, not LTO; the same machine, sequential). The active test's cost scenes, Box3D
step: base 0.251/0.772/0.293 and 0.229/0.618/0.308 ms, new 0.229/0.702/0.319 and 0.221/0.692/0.235 ms. physbench with
200 explosive boxes (`PB_CRATES=0`) stops in both builds at a QC "Program error" after the collapse (VR_PhysicsFrameEnd
-> a QC function: unrelated, not looked into); its collapse phase: base 0.335 / 0.288 ms, new 0.270 / 0.289 ms. With crates
(random, new only): collapse 0.263 ms, toss 0.421 ms.

## b3dtune: the ledge climb's going-in judged from the edge; a fast grab's hold; kdstir's pairing (branch agent/b3dtune, 2026-11-26)

The three cases box3dup3 flipped (Box3D 042be58), each traced (`vr_ragdoll_active_trace`, the climb test's own batched
runs: a run alone, drawn, plays out differently from the batched headless one, so traces were taken in the batch):
- **climb pull grunt seed 10** ("slowly in", then down at z 24). Its holds took the lip 5 and 15 units out from the edge
  (box3dup2: a ledge hold is where the hand was), so the Over phase's "in" (the pelvis from `Climb::lip`, the holds' mean)
  read 7 units in with the pelvis 3 out over the drop; its legs reached for "the top" in the air and it lay on its thighs
  across the corner. Its get-up then found no place (`vr_knockdown_debug 2`'s new line: its pelvis 9 under the top, every
  place inward "a wall between", the floor under it none) and the QC's forced get-up stood it up on the top or, by a hair,
  out over the drop. Fix (`vr_ragactive_hold.cpp` climbFrame): going in (the `in` point, the chest's and legs' goals, the
  pelvis's "in" for `inside`, "in on the top" and "slowly in") is measured from `rim`, the lip moved onto the edge's line
  (`Climb::edge`); the rise (`under`, `over`) and the let-go test unchanged. Now "slowly in" with its pelvis 7 in from the
  edge, 4 over the top, and it stands on the top. endClimb's line says "in from the edge".
- **climb pull knight seeds 7, 9, 10** ("lost its hold", the same cause; 9 and 10 failed before box3dup3 too). Falling at
  3.6 m/s, its left hand grabbed the lip 25 units off (a ledge is grabbed up to an arm off at speed, its hold pulling the
  hand in) and keepHold's "pulled away" (0.6 of an arm) let it go 0.01 s later; on the right hand alone it was pulled off.
  Fix (`vr_ragactive.cpp` keepHold, holdOn; `EnvHand::gap`): a ledge hold is pulled away only 0.6 of an arm past the gap it
  had at the grab, the gap shrinking as the hand comes in. holdOn's debug line prints the hand's gap.
- **kdstir** (one of six get-ups 0.80 s late stirring): not the stirring. The six knockdowns run one after another, each
  from where the last get-up left the grunt; the two runs (stirring, not) parted at the 4th (lying 16 units apart; the 6th
  270), and a fall landing on its other side makes the get-up a roll over, 0.8 s longer, either way (the 6th was 0.8 s
  sooner stirring). The test's worst-case 0.3 s assumed paired falls: now it is over the knockdowns lying in the same place
  (their get-up spots within 12 units; 3 or more needed); the mean (0.1 s) still over all six.

Left: hell knight seed 8 ("stuck at the lip": a weak pull, its chest stayed 13 under the top; it failed before box3dup3 as
"slid back off the lip", not this cause).

| check | box3dup3 (origin/next) | now |
|---|---|---|
| climb_test pull, seeds 7-10 (24) | 19/24 (grunt 10; knight 7, 9, 10; hknight 8) | 23/24 (hknight 8) |
| climb_test drop, hit, off (18) | | 18/18 |
| kdstir | 4/1 | 5/0 (worst 0.20 s over 4 paired; mean 0.03) |
| ragdoll_stagger | 2 FAIL (monster 0 backpedal; grunt "its spot taken") | the same 2 |
| active_test, env_test | no fails | no fails |
| grab_test | | death 5/5, rag 4/5 ("grab rag enforcer: no grab", its known flake, ROUND23), off 5/5 (shove: INFO) |

Debug aids: `vr_knockdown_debug 2` prints why a get-up found no place on dry ground (its pelvis, the floor under it, the
places with a wall between, no floor near its own, no room), as it did in a liquid; `vr_debug_ragdoll`'s ledge "holds on"
line gives the hand's gap. Not checked in the headset (a monster knocked off a ledge pulling itself up: the same look).

## flatloadbar2: the flat loading panel small and centred over the last screen dimmed (branch agent/flatloadbar2, 2026-11-26)

His words: the flat progress bars "take most of the screen" on "a completely black background"; asked for them much
smaller, centred, over a 50% black so the previous screen (the console) shows underneath.

- **Size:** the window's blit of the panel's image (768x192, the same image the headset's quad layer gets: one
  `drawPanel`) is now at most a third of the window's width (and height x4), at a ratio that keeps the console font sharp
  (`panelWidth`, GL_NEAREST): half the image (384x96: the image draws in 2x2 blocks, so each lands on one pixel) below
  2304 px of width, the image's own size (768x192) up to 4608, then whole multiples. His 3440x1441 window: 768x192 (22%
  wide); 1920x1080: 384x96 (20%); the tests' 960x540: 384x96 (40%: the half-size floor). Centred. Shared by the shaders'
  panel at a cold start (no backdrop there: nothing drawn yet, black).
- **Backdrop:** the window's last frame (console, menu, the map before), taken once when the panel first shows after it
  (`takeBackdrop`): read back from the buffer that holds it (gl_vidsdl.c `VID_LoadingLastFrame`: GL_FRONT once presented,
  GL_BACK when VR_SkipSwap left it unpresented; ES: none), every channel halved on the CPU (the 50% black), uploaded to a
  texture, then blitted under every panel frame. So both of the window's buffers get the same backdrop and the panel never
  dims its own frames. The texture is given back at the load's end (`dropBackdrop`).
- **No console flashes:** a load from the console (not connected) redrew the console at every print (`Con_Printf`) and
  every host frame before the signon, alternating with the panel's frames (also before this change: the black panel and
  the console flickered). While the panel is up in flat (`VR_LoadPanelOwnsWindow`: the loadbar active with a frame drawn, no
  VR session) neither draws; the first signed-on frame ends the loadbar as before. With VR running nothing changes.
- **VR:** the headset's panel is unchanged (same image, same quad layer); the desktop window beside VR gets the new look
  (small, over the mirror's last frame dimmed), the console redraws there unchanged.
- **Cost:** one readback per load (960x540: 2.3-9.5 ms in low-impact runs; his 3440x1441 ~5x the pixels), then one
  full-window blit a panel frame. The end line now says the swaps' share: "loading panel: vrstart's load, 22.1 s: 65 frames,
  1097.7 ms drawing them (11.8 in the window's swaps)" (the drawing times swing 78-1600 ms between identical runs here: the
  author's game was running, idle priority; the CPU image is unchanged). developer 1: "loading panel: the window's last frame
  (960x540, the front buffer) taken as its backdrop in 2.3 ms".
- **Test aid:** `vr_load_panel_shot` also saves the window's image of the same frame, before it is shown:
  `screenshots/load_panel_<stage>_window.png` ("loading panel: saved ... (the window, 960x540: the panel 384x96, over the
  last frame dimmed)"). Shots: flat over the console, flat over the menu, mock VR (the headset image unchanged, the
  window over the mirror). perfload_test (0 models in play, vrfiringrange 368 ms), hub_start_test 7/0.
- Not done: the window's own scale (scr_menuscale/conscale) isn't read: the size follows the window alone (his settings
  give the same 768 px).

## menubarreset

The menu shortcut bar (the column beside the menu in the headset and the flat VR-style panel, the row of icons in the classic flat menus) now has half a button of vertical gap before its "-" and "+" buttons (column layouts; the column fit counts it) and a third button, RESET, next to them: the shipped menu size (vr_menu_scale / vr_menu_flat_scale / scr_menuscale by the layout shown) and, for the flat panel, the window centre (vr_menu_flat_x/y back to their defaults, so a saved config drops them). The headset menu has no position setting, only its size. One code path for all three layouts (zoomMenu dir 0, the existing vr_menu_zoom reset); the mouse and the laser hit it like -/+ (new mock spots `vr_mock_mouse zoomreset`, `vr_mock_laser zoomreset`). Gamepad focus: the -/+ buttons were never in the gamepad focus order (pointer only), so RESET is too; vr_menu_zoom reset is the console equivalent. Tests: menu_flat_panel_test.py gained resetbtn steps (flat: moved to 0.4/0.45 at 0.6, RESET back to 0.954/0.5/0.499; VR: 0.4 back to 0.25): 28 passed, 3 failed; the same 3 fail with the unmodified test on this build (Quit menu number 20 vs 21, headset Quit dialog, classic scr_menuscale step: unrelated, not looked into). Screenshots scratch/menubar_{classic,flat,vr}.png.

## flathold2: H holds big props by their surface like ragdolls, small ones in the hand, an arm's reach (branch agent/flathold2, 2026-11-26)

His note (flat feedback 2026-11-26, item 12): H held ragdolls well (the hands on their visible models, dragged naturally) but
not props; big props should be held the same way, small ones as a force grab takes them; tunables for the hands' offsets in
that hold; H reached too far.

- **One rule, by mass** (`VR_FlatGrab_Drags`, QC/vr_flatgrab.qc): a carryable prop whose mass (Held Object Weights: Mass,
  or its estimate) is at least `vr_flat_grab_drag_mass` (15 kg) is **dragged by its surface**; lighter things go **into the
  hand as a force grab's catch** (`VR_FlatGrab_TakeHandTouch`: placed at the hand by its grab point, then its own handtouch
  with `forcegrabCatching`, exactly the force grab's handover; the old carry placement is only a fallback). Measured: crate
  40 kg, barrel 30, explosive boxes 80 and 40 (dragged); rock 2, health box 2 (in the hand). Gibs: in the hand.
- **Dragged by the surface** (engine: `propdrag` builtin, `box3d::propDrag`, vr_box3d_ragparts.cpp "A big prop dragged by
  its surface"): the embedded blade's prop hold reused (`World::PropBlade`, kinematic body at the hand, motor joint) with a
  `drag` flag: the joint at the point of the prop's Box3D shape nearest where the crosshair enters its box (two hands side
  by side, as far apart as the hands), its spring linear only (no torque: it hangs and swings from the hands), `force`
  level (`vr_flat_grab_drag_force` 4000 N, the ragdoll grab's), up at most `vr_flat_grab_drag_lift` x its weight per hand
  (0.2: both 0.4, under the half that tips a box: it slides on its far side), the target never higher above the held point
  than that lift stretches the spring (so the level pull stays full while the hand is above it), stiffness
  `vr_flat_grab_drag_hertz` (4 Hz), the point eased to the hand over `vr_flat_grab_reach_time`; let go of when the point
  stays more than `vr_flat_grab_drag_letgo` (1.2 m) off the hand. The prop stays in the physics (not carried); the hand is a
  new busy kind `VR_HOLD_DRAG` (vr_fields.qc; `VR_Hand_HeldOff` true: its fist body pushes nothing). B drops / throws it as
  a ragdoll held (kind 3; `VR_FlatGrab_ReleaseHeld`, a heavy prop leaves slower: `VR_Thrown_Lenient`); `VR_FlatGrab_Held`
  is "what this hand holds" for H's code, B's and the flatkeys test.
- **The hands drawn on it** (vr_view_hands.cpp `limbHold`): a dragged prop is a limb to the hand drawing
  (`box3d::propDragHold`, the listen server's): the hand drawn on the held point (as far as the let-go distance: a prop too
  heavy to hoist lies below the hold pose), turned with it, its palm rested on the prop's drawn mesh (at least 20 cm of
  slide, whichever way the palm faces), the fingers closing on its shape; vr_modelcollide skips it as a held limb.
  `vr_ragdoll_hand_probe` reports it ("part -1").
- **The hold's offsets**: `vr_flat_grab_hold_fwd` / `_out` / `_down` (Flatscreen > Grab / Pull rows) already set the
  hands of the hold pose for ragdolls and big props (and now say so; Hold Down's slider goes to -0.8, negative = above the
  shoulder): what is held comes to them. Defaults unchanged.
- **Reach** (`vr_flat_grab_reach`, now metres, 1.25; was 100 units = 3.8 m, why it felt long): measured from the eyes to
  where the crosshair meets the thing's box, level and up as they are, down not at all (down to the floor at your feet,
  nothing below them): an arm (0.65 m) past the body box's 0.6 m half width. Too short in play: he raises it.
- New tunables (Flatscreen > Grab / Pull): `vr_flat_grab_drag_mass` 15, `_drag_force` 4000, `_drag_lift` 0.2,
  `_drag_hertz` 4, `_drag_letgo` 1.2.
- Tests: `flatgrabthrow_test.sh` 34/0 (new: crate and explosive box dragged by their surface with both hands, palms within
  5 cm of their meshes, dragged along backing away, let go of with H; health box and rock in the hand, no hold pose; a
  crate 64 units ahead out of reach; the crate walk now backs away: walking into a dragged crate pushes it off the hands,
  which let go); `tunables_test.sh` 13/0. Screenshots scratch/fh2_crate.png, fh2_ragdoll.png.
- Open: walking forward into a dragged prop shoves it away and the hold lets go (pulling is what dragging does); ammo boxes
  were not found by H in the hall's probe (picked up by walking onto them there), not looked into.
## flatthrow2: flat throw key: cancel with the attack button, no catch by the guiding hand, flipped-grip wind-up tunables (branch agent/flatthrow2, 2026-11-26)

His flat feedback 2026-11-26, items 2, 3, 13.
- **Cancel (item 3):** while B (vr_flat_key_throw) is held (the tap window, the wind-up, or the quick grenade wound up by
  B), a fresh press of the throwing hand's own attack button (as the mouse is routed now: `flatplay::attackButtons`, LMB
  for a lone weapon, LMB/RMB per hand otherwise, the grenade's button for the quick grenade) sends impulse 41 (resent
  every 8 frames while the server still aims). QC `VR_FlatThrow_Cancel`: the throw phase back to 0, nothing thrown or
  dropped; a B-wound quick grenade goes back to HELD/ARMED as it was (`vr_flatgren_btn` set so the press neither arms nor
  winds up). The client swallows that button until it is let go (`flatgrenade::swallowedButtons`, applied in
  `flatplay::routeFire`), so nothing fires, swings or punches; releasing B afterwards does nothing (the server has no
  phase). While B is held the throwing hand's fire is now also blocked during the tap window (`flatgrenade::aimsWith`;
  before, only during the wind-up), so the first frame of the press can't fire. A button already held when B went down
  does not cancel (edge only). Debug: `vr_flat_grenade_debug 1` prints `flatthrow: the <hand> hand's attack button ...:
  throw cancelled`, developer 1 `flat throw: cancelled: ...`; `vr_flatgrenade_report` counts cancels. Note: a 2H crate's
  off hand let go as the wind-up began (`VR_Carry_EndTwoHands`) and stays off after a cancel.
- **No catch while aiming (item 13):** QC `VR_FlatThrow_Guides(p, hand)`: in flat play, the hand that is not winding up
  while a throw (B) or the quick grenade winds up. `VR_Grenade_HandCatches` refuses it (covers the palm's sweep
  `VR_Grenade_CatchCheck` and the touch `VR_Grenade_HandTouch`), and `VR_Carry_Handtouch` doesn't start a carry with it
  (it nudges). It applies whatever `vr_flat_throw_guide` is (the hand is part of the throw). Catching is unchanged
  otherwise.
- **Flipped-grip wind-up (item 2):** new tunables over the wind-up of a weapon with QVR_WPNFLAG_REVGRIP, by the flip's
  drawn share (`flatplay::flipShare`), applied after the wind-up blend in `flatgrenade::apply`:
  `vr_flat_throw_rev_fwd/_out/_up` (units along the look / own side / up, ±30) and `vr_flat_throw_rev_pitch/_yaw/_roll`
  (degrees about the fist, ±180, yaw/roll mirrored for the left hand). All default 0 = the pose as before. Rows in
  Flatscreen > Throw ("Throw: Flipped Grip ..."). The hand turns, so the thrown weapon's starting angles follow the tuned
  pose (vr_embed_test 60's "rev throw spear ... matching held inverted angle" check assumes the defaults: 23/1 with
  rev_up 12 / rev_pitch 60, 24/0 with defaults).
- **Files:** Quake/vr/vr_flatgrenade.cpp/.hpp, vr_flatplay.cpp/.hpp, vr_client.cpp/.hpp (`client::mainButton`),
  vr_tunables.inc, vr_tunables_rows.inc; QC/vr_flatgrenade.qc, weapons.qc (impulse 41), vr_grenade.qc, vr_carry.qc,
  vr_flatitems_test.qc; Misc/quakevr/flatthrow_test.sh, flatthrow_check.py; scenario quakevr/scenarios/flat_throw_cancel.txt.
- **Tests:** flatthrow_test.sh: new runs `cancel` (nail gun: LMB in the wind-up cancels, no nail, still held, B again
  throws; quick grenade: cancelled, kept unarmed, phase 2), `cancoff` (a health box in the off hand, RMB cancels), `guide`
  (`vr_flatitems_test 12`, new: an ogre's grenade set at the guiding off hand's palm, velocity into it, while B winds up
  the quick grenade and the axe: not stopped or taken; control without B: caught, 6/0); `nofire` changed (the fire button
  now held from before B through the throw, since a press in the wind-up cancels). flatthrow 48/0, tunables_test 13/0,
  vr_embed_test 60 24/0.
## flatparry2

His flat notes 2026-11-26, items 1 and 4-6 (the parry guard's pose).

- **Inverted grip in the guard** (item 1): while the flat guard is asked for or not yet eased out
  (`flatplay::parryUnflips`), the view's `gripFlipStep` (vr_view_weapon.cpp) draws a flipped grip upright over
  `vr_flat_parry_unflip_time` (0.1 s; negative keeps the flipped grip) and flips it back over the same time once the guard
  is down (the button's let-go starts it). Only the drawn flip changes, not QVR_WPNFLAG_REVGRIP, so everything that reads
  the drawn flip follows it: revGripPose, flippedGrip, the 2H stance gate, the blade line the game parries with. Every
  weapon with a flipped grip (swords, the dagger, the spear) takes the same path. vr_flatplay_debug prints
  "flatparry: ... grip upright, N s" and "... flipped again, N s"; vr_flatplay_status prints "parry flip:".
- **Free hand near the tip** (items 4-6): the guard's blade grip (vr_flat_parry_blade) needs `twohand::swordBlade`, which
  needed a Two-Handed: Sword weapon with a Blade hotspot (type 2) at a share > 0. Audit of weapons.txt:

  | weapon (slot, wofs NN) | 2h_mode | hotspots | guard before | now |
  |---|---|---|---|---|
  | judicator's sword `w_judgesword` (49, 50) | sword | hs1 Grip at -5.71 0.20 -8.12 (the death knight's sword's hs1 copied: slots 41/43/47 the same); hs2 Blade at share **0** (his, wofs v57) | no blade grip: both hands on the handle | shaft fallback |
  | hunter ogre's hammer `w_ogreham` (44, 45) | sword | hs1 Grip only | both on the handle | shaft fallback |
  | hunter ogre's mace `w_ogremac` (45, 46) | sword | hs1 Grip only | both on the handle | shaft fallback |
  | death fury's sword `w_dfurysword1` (40, 41) | sword | hs1 Grip only (hs2 off, his) | both on the handle | shaft fallback |
  | flail `w_blueflail` (50, 51) | sword | hs1 Grip (the mace's copied) | both on the handle | shaft fallback (untested) |

  Fallback: a Two-Handed: Sword weapon with no usable Blade hotspot answers `vr_flat_parry_shaft_at` (0.8 of the way from
  the hand to the tip/head) and its point is held within `vr_flat_parry_shaft_reach` (22 u) of the holding hand (the
  blade's own reach stays vr_flat_parry_blade_reach 14). A Blade hotspot he sets up (share > 0) replaces the fallback at
  once (and its reach is the blade's 14). Hotspot-mode melee weapons (the axe, Mjolnir: 2h_mode 1) are unchanged (no
  blade grip). A "grab"/Grip hotspot near the tip is not read (a Grip on a sword-mode weapon is the foregrip below the
  hand; its line runs the other way), so for the curved fury sword the way is a Blade hotspot (flat pins one point at its
  share; in a headset the half-sword grip runs along the straight hand-to-tip line).
- Test aids: `vr_ad_test 171..174` (fury, judicator, hammer, mace alone in the main hand; Debug menu "... Alone in Hand"),
  scene `flat_parry_ad`.
- Tests: `Misc/quakevr/flatparry2_test.sh` (AD mounted): flip runs (knight's sword, judicator) flip 1.00 -> 0.00 in
  0.097 s -> 1.00 in 0.097 s, the probe parried; per weapon the free hand posed 0 u and drawn 1.3-2.1 u off the point;
  point / tip from the holding hand: sword 14/27.9, fury 22/33.5, judicator 22/48.3, hammer 22/31.7 (9.7 from the head),
  mace 22/27.8 (5.8): 33 passed, 0 failed. Screenshots scratch/fp2_<weapon>.png. flatparry 43/0, flatmelee4 9/0;
  flatbash 17/3 (sword "took the blade" line: the 2H stance's foregrip was already held so the switch to the blade prints
  nothing; a 165-degree frame turn; the axe's bash missed the grunt; none touch the paths changed here).
## flatmelee6: V's two-fisted overhead blow with both hands empty (branch agent/flatmelee6, 2026-11-26)

His item 11 (flat feedback 2026-11-26): "If both hands are empty, the quick melee button should perform a simultaneous
overhead attack with both hands. Make this an option (on by default)."
- **Option** `vr_flat_overhand_both` 1 (`CVAR_ARCHIVE | QVR_OWN`; row "Quick Melee: Both Fists", Advanced VR Options >
  Movement > Locomotion, Desktop Screen Play, after Grab / Pull Key). 0: today's one-fist overhand (vr_flat_overhand).
- **Motion** (vr_flatplay.cpp startPunch): V's overhand on one hand with the other hand also empty (no weapon, no prop,
  not punching, shoving, in the quick grenade, the force grab or on a stuck blade) starts the other hand's overhand on the
  same clock (`fs.punchTwoFist[h]` both): the one overhand path (`overheadSwing`, `fistSwing`), each hand its own side's
  mirror. Each fist's top `vr_flat_overhand_2h_raise_out` -3 further out (in: the fists 6 from the middle, not 9) and its
  end `vr_flat_overhand_2h_end_out` 1.5 out at its own side (side by side, not in one another). End_out 3 had the main
  fist meet the grunt glancing, with its fist and not its knuckles, at 9.3 m/s instead of 12 (half the strength): 1.5
  keeps the knuckles' 11.8 m/s.
- **One hit**: the off fist is marked Synthesized all through ("two-fisted blow's other fist": no blow, no gesture); the
  main fist is the Action as before, plus `handsource::twoFisted()`: the move's `.handsynth` 256 (QC `VR_HANDTWOFIST`,
  `VR_HandTwoFisted`). `.handsynth` is now a short on the wire (it had all 8 bits used): `QVR_NET_VERSION` 47.
- **Damage and push** (QC): `VRMeleeDmg` times `vr_flat_overhand_2h_damage` 2 (the two fists' sum; outside the
  vr_melee_dmg_min/max cap, which bounds strength x zone only), `T_Damage_VRMelee`'s push times
  `vr_flat_overhand_2h_push` 1 on top of what that damage gives (so by default the push is the sum too, up to VR_Push's
  own 600 cap). Tunables group Flatscreen > Empty-Hand Overhand (V), the four "Both Fists" rows.
- **Probe / prints**: `flatplay: main punch (...): melee, overhand blow, both fists`; the melee probe's kind
  `overhand both` for each hand.
- **Tests**: new `Misc/quakevr/flatmelee6_test.sh` 9/0 (both empty: the two-fisted blow, both hands' probe lines
  overlapping, both over the eye 8.0, the off fist synthesized, one melee hit (60 on a grunt 16 ahead; one fist 26-32);
  option off: one fist, one hit; a gun in the off hand: one fist). Optional 2nd argument: console commands said first
  (e.g. `"tune vr_flat_overhand_2h_end_out 0"`). `flatmelee4_test.sh` 9/0 and `flatmelee_test.sh` 12/0 (their V with
  empty hands is now the two-fisted blow; their checks still hold). eval.sh not run: no shared melee code changed
  (VRMeleeDmg/T_Damage_VRMelee only scale when the flat bit is set). Scene `flat_melee_2fist`.

## questcrash: Quest Foveation Off no longer crashes; presets on the Quest keep its budget (branch agent/questcrash, 2026-11-26)

His Quest (next ab5d33a, standalone): the game crashed switching Quest Foveation from High to Off; Standalone -> "Quake
(off)" -> Low mid-game left it under 1 fps with Standalone out of reach; then every start crashed after loading vrstart.
- **The crash** (logcat 19:38:10, 19:47:04, 19:47:52; the crash reports themselves are 0660 to adb): every one SIGSEGV 0x30
  in libGLESv2_adreno under libvrapiimpl, from `xrEndFrame` (OpenXrBackend::endFrame <- VR_RenderView): C4p's /
  qfovcrash's / hubcrash's signature. **Cause**: since qfov3 the eye swapchains were always made foveation-capable
  (XrSwapchainCreateInfoFoveationFB, scaled bins), and with Off (a level-0 profile) the eyes take the post-process path
  (late acquire, the image's invalidate, the shared framebuffer attach) into those images: the runtime's driver work in
  xrEndFrame crashes. Off saved in the config (Quake (off) put the Standalone rows back to the PC's values: foveation 0)
  made it every start. **Fix** (vr_backend_openxr_quest.cpp): capable images only while `vr_xr_foveation` is over 0
  (`QuestExtensions::eyesCapable`); a level crossing 0 takes new eye swapchains (as a level change already did, C4p), Off
  makes plain ones (the images every Quest build used before qfov3) and applies no profile.
- **The slow presets** (vr_gfxpresets.cpp): leaving Standalone put back every Standalone row as it was before it, i.e.
  the PC defaults (full resolution, no dynamic resolution, OIT, the float depth, foveation off...); Low then added its
  shadows. On the Quest (and under `-glescompat`) a graphics preset or My Settings now sets its look while the headset's
  budget stays: the Standalone rows no graphics preset sets keep the nearest Standalone level's values, and the Quest's
  must-haves (`vr_quest_check`'s: MSAA, bloom, the shadowed dynamic lights, the dynamic resolution's floor, and new: the
  shadowed map lights) hold after every pick, each said. The PC is unchanged.
- **Never stuck**: any pick (a graphics preset, Standalone, My Settings; the menu, the console, `vr_graphics_preset`)
  whose frames then stay under `vr_preset_revert_fps` (5, archived; 0 off) is put back as before it: past a second's
  grace (the programs it compiles), the median of 8 frames; said on the console and the menu's note line ("Too slow on
  this headset: put back as before."). `vr_preset_revert_test <ms>` counts each frame as that long (Debug > Startup:
  Preset Revert Test 500 ms / Off).
- Tests: `Misc/quakevr/questcrash_preset_test.sh` (ES profile: Standalone -> Off -> Low -> Medium keep the budget rows,
  the must-haves hold, a 20 ms pick kept, a 500 ms pick put back; PC: leaving Standalone as before) 20/0;
  haze_preset_test 8/0.
- **Headset** (Quest 3, 20:28-20:38, unworn, `-noconfigwrite -unattended`, his config). Exported read-only
  (`vr_android_export`): his `ironwail.cfg` saves `r_oit 1`, `vr_tonemap 0` and no `vr_xr_foveation`/`vr_render_scale`
  (so Off at full resolution); the crash reports agree: 19:47:04 "frame 10: xrEndFrame" after the first world
  programs, 19:38:10 right after Off's new eye swapchains. The old build (ab5d33a) with his config ran 90 s unworn
  **without** crashing (FPS 41/72): the crash needs the worn headset (focus, the compositor's real work), so no
  before/after pair unworn. The fixed build (this branch, merged with next c13b78b0a) installed the questinst way (APK
  `install -r`, 7 changed data files; his config kept): his config to vrstart, then Off -> 3 -> 0 -> 3 -> 0 -> 2 -> 0
  (each a new pair of eye swapchains), Standalone, Quake (off), Low, Standalone again, quit by itself after 189 s: no
  crash, 72/72 throughout but one second of program compiles after Quake (off). Rollback: `scratch/qc_installed_before.apk`,
  `scratch/qc_backup/`. To confirm: worn, start with his config and switch Quest Foveation High <-> Off.

## The flashlight lights the dark rooms in the Standalone preset; Show Foveation shows the Quest's foveation (branch agent/questrender, 2026-11-26)

His notes (Quest standalone): the flashlight lit only the first part of the tutorial's dark room, the rest pitch black
wherever he pointed it; "Show Foveation" showed nothing with Quest Foveation.
- **The flashlight (vr_gl_world_dlmap, the Standalone preset's dynamic lights summed into the lightmap, C5g):** the light
  tool leaves the faces no map light reaches without a lightmap (vrtutorial's dark course: 612 of its 740 faces), and
  Ironwail's `GL_PackLitSurfaces` gave them all one shared black block; the dlmap's `lightSurface` skipped them
  (`!s->samples`). Only the faces near the lit hall's door, which have baked light, took the beam. Now, in a lit map,
  each such face gets a black block of its own (`r_brush.c`, `own_black`; a map without light data keeps the shared one)
  and the dlmap lights it. The PC's per-pixel loop never depended on it. Cost: the lightmap atlas grows by those faces'
  texels (black), no per-frame cost beyond the faces the beam now lights. Brightness: as the loop's without its
  specular sheen (the far block: dlmap 33, loop 80, loop with `vr_specular 0` 35; the dlmap never had the sheen, C5g's
  look call). Pictures: `scratch/qr_flashlight_before.png` / `_after.png` / `_loop.png` (not committed).
- **`vr_gl_world_dlmap_probe x y z [min] | end`** (new, vr_dlightmap.cpp): the light the last build added at a point on
  a world face (from the batched texture's CPU copy) and whether the face has baked light; `min` makes it a check.
- **Show Foveation on the Quest (`vr_foveated_debug`, now Off / On / Quest Pattern):** it drew only NVIDIA's rates and
  vr_upscale's ring, neither on the Quest. It now also draws the Quest's foveation zones, yellow at half the pixels (2x1),
  red at a quarter (2x2): vr_gl_foveation's exactly (its gain and area, the focal point on the projection centre: the
  QCOM density 1 / (gain^2 d^2 - area)), else vr_xr_foveation's modelled (`foveated::runtimeZones`: centred on the
  drawn part, as qfov4 found; gains 1.3 / 1.6 / 2 for Low / Medium / High: Low only the corners, High from ~0.7 of the
  way out; the runtime's true pattern is Meta's and falls on whole bins, so it is approximate). Quest Pattern (2) draws
  the modelled zones at vr_xr_foveation's level (1 if off) on any headset. Files: vr_foveated.cpp/.hpp, vr_stereo.cpp
  (`glFoveationZones`, sharing foveateEye's gains and areas), the row in vr_menu_rows.inc.
- **Tests:** `Misc/quakevr/darkflash_test.sh` (vrtutorial's dark course, Standalone preset, the PC's unlit world form
  through `vr_gl_pc_forms 15`, a head torch on): the block face ~310 ahead and the far wall ~680 ahead (a face without
  baked light) take light; 2/0. Scenes `quest_darkroom`, `quest_foveation`. Not checked on the headset (he was using it).
## flatgrip3: daggers and axes one-handed in flat play; the pole's other hand round the drawn shaft (branch agent/flatgrip3, 2026-11-26)

His flat items 7 and 8 (2026-11-26).
- **One-handed daggers and axes (item 7):** `vr_flat_1h_dagger` and `vr_flat_1h_axe` (cvars, archived, the player's own; default 1;
  rows "Daggers One-Handed" / "Axes One-Handed" in Locomotion > Desktop Screen Play). `flatplay::oneHandOnly` (was the dagger
  alone) now reads both: the dagger (WID_SWORD kind 12), the axes (WID_AXE: the axe and AD's Shadow Axe; WID_SUPERAXE). It
  gates the two-handed stance (`twoHandAsked`, was `!isDagger`), the parry guard's blade grip (`bladeWanted`), flatsight's
  carry-to-grip and held-off pose, vr_twohand's grab (`flatLetsGo`: flat only) and the stuck blade's other-hand join
  (QC `VR_Embed_CanTwoHand`). With an option at 0 the weapon is two-handed as the swords. Tests about the 2H mechanics that use
  the axe now set `vr_flat_1h_axe 0`: flatmelee2h_test, flatfg2h_test, embedhold_test.
- **The pole's other hand on the shaft (item 8):** three causes, all fixed in `vr_view_hands.cpp`:
  1. The line: flatembed3 put the hand on the line from the drawn weapon's origin (the main hand's mount) to its muzzle; the
     origin is off the shaft, so that line ran beside it (spear 15 cm, Lance 6 cm at the hand, and turned 24 degrees for the
     Lance). Now `poleShaftLine`: the shaft from the model's own triangles (`grasp::shapeOf`, as drawn: `shapeToWorld`): the
     principal axis and mean of the vertices in the middle half of its length (no head, no butt), taken at the model's drawn scale
     (an alias model's raw vertices are scaled per axis), kept per shape (`shaftMemos`, file scope); also the shaft's radius. The
     stuck pole too: the hotspot projected onto that line.
  2. The fingers: the grasp of a helping hand on a grip hotspot was solved on the hotspot's canonical place in the hand
     (`canonicalHotspotGrip`), not where the pole is drawn: once moved onto the shaft, the fingers closed on a pole that wasn't
     there (a fist, 0 of 4 fingers on it). A hand put wholly on the drawn shaft (`shaftGripped`) is solved on the pole as drawn.
  3. The thickness: the hand's grip channel is taken at the curl that wraps the shaft's radius (`grasp::gripChannelFor`,
     bisected between half closed and the tightest fist; `gripChannel` takes a curl now, default 2.5 as before).
- **Why the old test passed:** vr_embed_test 71 measured the grip channel against the same origin-to-muzzle line the code had
  put it on (0.00 cm by construction), before the grasp, with the fingers' closure never looked at. Now `poleProbeNote` runs after
  the hand is drawn: the drawn channel (with the grasp's palm move, `drawnInRig`, at the shaft's curl) against the shaft as the
  model is drawn, and `vr_embed_probe_pmet`, the fewest of the four fingers closed on the pole (test: at least 3,
  `VR_PE_FINGERS`). The stuck case skips frames the arm holds the hand short of the shaft (off its line by design). With the old
  placement the new probe read 0 of 4 fingers in every held state (spear and Lance) and the Lance's hand 5.8-9.9 cm and 23.6
  degrees off its shaft (the spear's numbers of that run came before the shaft fit's scale fix: not quoted); now 0.00-0.28 cm, 0 degrees, 4 of 4 fingers in every state. `vr_embed_probe_shots 1`: a screenshot
  of each state the test measures.
- Tests: `poleembed_test.sh` (vr_embed_test 71 16/0, 72 13/0, with his promoted pole tunables of origin/next 363f0b1ab);
  `flatgrip1h_test.sh` (new, 9/0: axe, super axe, dagger one-handed held and in the guard; the sword and `vr_flat_1h_axe 0` the
  controls); flatmelee2h_test 16/0. embedhold_test: its ogre case fails one check (`.vr_embed_armpast` -2.0 cm "while clamped";
  a sword, no code of this branch on its path; not investigated: possibly the 2H stance tunables promoted in 363f0b1ab).
## flatxhair: the flipped spear's crosshair on where its thrust lands; melee blows widen the crosshair (branch agent/flatxhair, 2026-11-26)

His flat notes 2026-11-26, items 9 and 10.
- **Flipped pole crosshair (9).** The visor crosshair (vr_flatxhair.cpp) traces each hand's shot line as sent (muzzle +
  `view::heldShotAngles`). The spear has no shot, so that line stood in for its stab: right upright (flatsight aims the
  pole's tip line at the crosshair's point and the thrust goes along it), wrong flipped: its game hand is turned
  (revGripPose, vr_flat_revgrip_pole_pitch 220 since the 363f0b1ab promotion) and the drawn pole reversed, so the shot line
  pointed back over the shoulder (its hit 400-1200 units behind; the crosshair just sat on the centre) while the thrust is
  thrustPose's blade-to-target solver: the blade turned onto the crosshair's point tilted vr_flat_revgrip_pole_down, the
  fist driven out along it and the tip turned by vr_flat_revgrip_thrust_* (reach 26, up -4, pitch -10 by default): a stab
  forward and steeply down. Now `flatplay::thrustPath` (noteThrustPath, computed each pose right before thrustPose, from
  the same helpers: `thrustWant`, `bladeOnto`, `thrustHandAt`, `thrustAt`) gives the tip's way through the strike: the
  drawn tip off the hand, turned whole onto the solver's line, at the strike's start and end. flatxhair traces that
  segment: what it meets is where the blow lands; nothing in reach: the tip's end (the place a thing must be for the blow
  to land on it; extending the tilted blade put the crosshair on the floor ahead). Only a flipped melee pole (the spear);
  the flipped Lance's crosshair stays its bolt's (its fire button shoots), the upright pole its shot line. Tunable
  `vr_flat_xhair_thrust_path` 1 (Debug > Tunables > Flatscreen > Crosshair, "Flipped Spear Crosshair"; 0 the old line).
  Measured on e1m1's wall (the landing from the QC's melee mark, `flatxhair::noteMeleeMark` in vr_decals meleeEvent):
  flipped 16 and 24 units off the wall 2.0 units apart (the mark's own 2-unit lift off the wall: exact), upright 2.3;
  before, nowhere near. The real reach ends about 2 units short of the path's end (the strike slows at its end).
- **Melee bloom (10).** `flatplay::lastStrike` (each punch's strike start: a thrust's wind + hold, a swing's wind + top,
  the fist's windup; its weapon's w_mass) feeds `meleeKick`: vr_flat_xhair_melee_kick 10 px (at 1080) as the strike
  begins, times sqrt(mass / vr_flat_xhair_melee_mass 3) (0.5..2; a fist or a prop vr_flat_xhair_melee_fist 0.5), decaying
  over vr_flat_xhair_melee_decay 0.15 s. Added to the gun's recoil kick in the same spread (under vr_flat_xhair_kick_max,
  smoothed by vr_flat_xhair_kick_smooth); every hand's spread is now kept whether its crosshair is shown or not, so a
  sword or fist blow widens the centre crosshair, and an off-hand blade widens the main gun's. Rows in the same group.
  There is no light/heavy attack in flat play: the weight and the fist share are the scale.
- Probe: `vr_flat_xhair_probe` prints each hand's `melee kick ... blows ... path`, the path's ends, and after a blow on
  the world `melee landed` / `melee said <known> ... apart <units> px <px>`.
- Tests: new `Misc/quakevr/flatxhair_melee_test.sh` 9/0 (flipped 16/24 lands on the crosshair, old line far off, out of
  reach on the tip's end, upright lands on it, the bloom rises and eases back for the spear, upright and fist, the fist's
  less). flatmelee4 9/0, tunables_test 13/0. Pre-existing on origin/next (same with my engine files reverted, merge base
  3836fa187^2): `flatxhair_test.sh` 28/3 (its `flush` case: wall share 0.0 at the first probe) and `flatthrust_test.sh`
  6/2 (the Lance run 0/5, flatmelee5's "rev thrust sword embeds" 23/1), likely the promoted pole/revgrip tunables.
## questname

The Quest app is now "Quake VR: Unleashed" (the PC's casing: quakedef.h title, QuakeVR.rc, the installer). Changed:
`Android/app/src/main/AndroidManifest.xml` (application and activity `android:label`), `Android/check_apk.sh` (new
check: aapt2 badging label), the installer's Quest texts (`QuestInstaller.cs` done and uninstall hints), `docs/INSTALL.md`,
`ANDROID_RELEASE.md`, `RELEASE_NOTES_1.2.md`. Package id unchanged. PC title, exe description and shortcuts already used
the name. APK rebuilt with package_apk.sh: badging says label 'Quake VR: Unleashed'; check_apk 21/0.

## brandcaps

The product name is now "Quake VR: UNLEASHED" (all caps) everywhere user-visible: quakedef.h titles, error/crash
dialogs and reports, QuakeVR.rc, the menus' version box and update notice, Android manifest labels + check_apk.sh label
check, the installer (window, texts, uninstall display name, Quest texts, shortcut names/descriptions), make_release
(.py/.ps1 titles), README, INSTALL, RELEASING, ANDROID_RELEASE, FEATURES and other current docs, release notes 1.2.
Left alone: ROUND*.md, PATCH_NOTES_1.1*, RELEASE_NOTES_1.1, RELEASE_1_1, the checklist history, trailer scripts, the
gameplay preset "Unleashed", the uninstall key `QuakeVRUnleashed`, file names and package ids.
Shortcut migration: Windows keeps the old letter case of an existing file/folder, so `ShellLink.Save` now renames an
existing Start menu folder / deletes an existing .lnk that differs only in case before writing (self-test "shortcuts of
the old letter case are renamed"; installer self-tests 63/63).

## vrspear: VR's off hand round a pole's shaft, fixed on it, kept through an embed (branch agent/vrspear, 2026-11-26)

His VR notes 2026-11-26 20:07-20:10 (vrtesthall).
- **Round the shaft (20:07):** VR's pole grip (`vr_twohand.cpp` FREE_POLE: spear, Crystal Lance) drew the off hand where the
  controller took hold, beside the shaft by however far off it grabbed. Now `setupHand` closes it round the shaft as drawn with
  flat play's own helpers (`poleShaftLine`, `poleShaftGrip`; shared, not copied), by the grip's blend (`twohand::poleHelpShare`):
  the least turn and move, so its place along the shaft is the grab's. Drawn only (the aim and the let-go go by the controller).
- **Fixed on the pole (20:09):** `vr_pole_grip_fixed` (archived, default 1; "Other Hand Fixed on Poles" in Physics and
  Interaction under Weapon Hand Slides on Poles): `updateFree` keeps its place on the pole (u + the holding hand's slide
  constant); how far the hand is from that place lets go of it (the hotspots' 20-unit keep, times stickiness). 0: today's slide.
  The holding hand's own slide (`poleShift`, a loose main grip) still works; its change is taken from where the hand would slide
  (`FreeGrip::uFree`), so a loosened grip never jumps.
- **A bug under the main hand's slide:** `freeHand` drew the off hand slid by its place from the holding hand, and the holding
  hand's drawn pose (`drawnAs`) is already moved back by `poleShift`: with the main hand sliding, the drawn off hand slid along
  the pole by the whole slide, both modes (10 cm in the test). Now less the slide since (`FreeGrip::handAlong`).
- **Through an embed (20:10):** a stab that stuck put the second hand's pin on the blade's nearest grip hotspot from its
  controller, eased in from the controller (the jump he saw). Now the off hand's last drawn pose on the pole is kept in the model's
  frame (`poleGripNote`, `grasp::shapeToWorld` without scale) and a new pin on the same model within 0.25 s (`poleGripWas`) starts
  there, at full weight, no hotspot; the stuck shaft grip keeps it round the shaft. A stuck pole's shaft hand now wraps with the
  slot's Grip Overlap (`heldBy`): with the default the Lance's palm was drawn 2.6 cm off its shaft. Both settings of the option.
  The server never let go in the mock (hands 3 the whole measure); his "detaches" was the drawn jump, as far as the mock shows.
- **Probe:** `vr_embed_probe_palong` (cm along the shaft from its middle, last frame), `_pslide` (most minus least since reset).
- **Tests:** `vrspear_test.sh`: vr_embed_test 75 (the Lance with the main grip loose, so the main hand slides; the spear with
  `vr_pole_grip_fixed 0`) 18/0, 76 (the spear fixed, main grip closed) 9/0: the hand put 1-2 cm over the shaft takes it, 0.00 cm
  off the shaft line, 0 degrees, 4 of 4 fingers; moved 15% of the pole along it, the drawn hand moved 0.00 cm (fixed) or
  10.2 cm (0); the stab sticks, hands 3 for 0.6 s, round the stuck shaft, 0.1 cm from where it was held. (76 is its own run: a
  third case in one run never took the pole, the fist 0.3 units off the shaft whatever the offset; not chased.) `VR_EH_Blow`
  takes a give command (`vr_eh_givecmd`, the Lance's `vr_ad_test 207`). dwell_lance_test now sets `vr_pole_grip_fixed 0` in its
  slide sections (51/0); spear_test 4/0 (VR 25/0); poleembed_test 16/0, 13/0.
## tutorial: round rungs gripped round, the tutorial's lips, dark block and rope pit (branch agent/tutorial, 2026-11-26)

His VR notes 2026-11-26 19:55-20:02 (vrtutorial).
- **Round rungs** (Quake/vr/vr_climb.cpp findRung, rungAt, rungUp; display only): a hand's hold on a thin round rung is
  drawn gripping it, the rung through the fist as a level pole's (gripAxes: ropeTurn without the rope's pitch), its palm's
  middle out from the rung's axis under it, turned `vr_climb_rung_tilt` (60) towards the climber. Found by shape, any map:
  probes down/up/across at the hold (worldtrace, so a remote client too): a bar at most `vr_climb_rung_max` (5) thick and
  deep, open under it, 2 units of room behind it, its front curving in near its top and bottom by `vr_climb_rung_round`
  (0.06) of its thickness (the tutorial's octagon 0.14, a square bar 0). Searched once per hold. Square or chunky rungs and
  ledges: as before. `climb::gripLine` (rope, pole or round rung) now feeds the hand's grip (vr_view_hands.cpp ropeGrip:
  the palm on the surface; ropeHeld: the fingers stop on it; vr_view.cpp: the thumb closes); `ropeLine` stays the rope's
  (the arms' rope elbows, the rope tests). Switch: `vr_climb_rung_grip` (VR Settings > Climbing > Hand on a Hold: Hand
  Round Thin Rungs). Flat ladder and headset alike (one drawnHand).
- Test `vr_rung_grip_test` (vr_flatmove.cpp, flat): vrtutorial's ladder walked into and hung from: each hand round its
  rung (fingers 0.39 in at the deepest, palm 0.4 off, vertices over and under it; the ledge pose with the switch off: 3
  under), 4/0; vrtesthall's square rungs: held as ledges, 2/0. Tilt 45 left the fingertips on top, 70+ nothing under it.
- **The lips** (vrtutorial_gen.py hazard_lip): the ladder block's and the jump wall's hazard strips stood 2 out of the
  wall (a head hugging the wall caught under the jump wall's); now an 8 high, 16 deep detail cap flush with the face.
  Playtest `--ledge` gates lipbase/underlip: a jump hugging the wall rises 44 (blocked it was at most 36).
- **Dark course** block 3: 48 -> 64 high (a climb, not a jump), its north lip a hazard cap (R9_BLOCKS' 4th field).
- **Rope pit** (room 14): wall lamps half way down the pit's walls (5; none over the ladder) and two on the room's long
  walls by the rope at LOW + 160; the rope is lit as the light where it hangs.
- Map recompiled (final, holes 0): vrtutorial's shipped cache keys change (bsp, lit, lux).
- Playtest: the "ledge" gate (catch the high ledge at a jump's top) fails on origin/next's map too (no mantle); on the new
  map the mantle starts, then "moved away, letting go": not this task's, left. Its grab now at x 3170 (over the lip).
- Screens: scratch/tut_before.png, tut_after.png (pit, dark block, jump wall), tut_rung_vrtutorial.png.
## VR fixes from his evening notes: rope grunts gone, preset rows toggle, punches on a hugged crate, AD crossbow knight, front sight hue (branch agent/vrmisc, 2026-11-26)

- **No grunts on ropes, chains, poles** (his note 20:03): the pull grunt (`ropePullGrunt`, `vr_rope_grunt`,
  `vr_rope_grunt_sound`, tunable `vr_pole_grunt`, their Climbing rows) removed. Flat ropes never grunted. The mantle's
  grunt (ledges) stays. `rope_plays.py` lost its `grunts` checks (`vr_rope_climb_check grunts` gone).
- **Preset rows toggle both ways** (his note 20:06): Handedness, Comfort and Gore Preset show Custom when the settings match
  no choice; stepped to, Custom set nothing and the next frame's sync showed the old choice again (Left-handed clicked
  stayed Left-handed). `vr_menu.cpp shownOnlyChoice`: a step passes over Custom, the drop-down's pick of it changes
  nothing. Test `vr_menu_preset_cycle_test` (Debug > Tools: each row stepped both ways, never stuck, never on Custom;
  the settings put back): 9/0.
- **Punches on a hugged crate land** (his note 19:56, VR and flat): a blow strikes what the hand struck last
  (`mh_struck`) again only entering it (his note 2026-10-12: a retraction or a blade turned in a body struck again).
  A fist resting in a crate's box as you hug it (a flat fist's rest past your box) started every punch in it, so
  only the first landed (swish, no hit). `vr_melee.qc VR_Melee_PunchedIn`: a fist's point starting in it that comes
  at least half its sweep nearer its middle strikes again; drawn back or turned, still not; blades unchanged (they
  embed). Test `vr_prop_impact_test 8` (hugpunch, Debug > Automation "Punch a Hugged Crate"; vrfiringrange): four
  punches hugging (knuckles a unit off), four from 3 units in, four 6 units off: before 2/1 (from in it 1 of 4), now 3/0.
  eval.sh: no new failures; secret_hits 4/4. Not run: a flat-screen punch scene (the fix is in the shared sweep).
- **AD crossbow knights stay AD's** (his note 20:21): `vr_agrunt_repl_crossbow` 0 (new, Armed Humanoid Replacements row
  "AD Crossbow Knights"; QC `VR_HUM_On`); every other variant still replaced. `vr_ad_test 219` sets it on for its rows and
  checks it off (humrepl 49/0); humcensus now leaves crossbow knights by default. BACKLOG section 6: make it a proper
  armed humanoid, then turn it back on.
- **Front sight hue** (his note 19:57): `vr_sight_front_hue_shift` default 240 -> 90 (his value).
## flashgrab: the belt flashlight is a holster slot; Holster Setup places, turns and sizes it (branch agent/flashgrab, 2026-11-26)

His note (VR, 19:58): a quick grab of the belt torch often missed it, the hand had to stop half a second; a hand arriving
already closed should take it too; he wants to drag it in Holster Setup, turn it, and set its grab leniency without
resizing the model.

- **Grab rules** (`vr_flashlight.cpp`, `onBelt()`): on the belt (or springing home) an empty hand's grip in the grab zone
  takes the torch at once: no `deliberate()` (no stillness, no open-hand-before check) and no punch exclusion. A grip
  closed before arriving arms the late grip from anywhere and takes it the frame the hand is in the zone, for
  `vr_flashlight_closed_grab` real seconds (QVR_TUNABLE, default 1; Debug > Tunables > Closed Grab Window). The
  arbitration with the game is kept (a holster nearer than the torch draws, the other hand's weapon wins: `gameGripWins`).
  On the head and on a gun nothing changed (deliberate presses, the late grip with stillness); the trigger toggle at the
  lamp still needs a deliberate press.
- **The other holsters** draw on the grip's press edge (QC `VR_HandGrabUtil_StartedHandGrabbing`: the hand in the hotspot
  within 0.125 s of the press). A hand closed earlier than that does not draw from them; the torch now allows 1 s, as
  he asked. Making the holsters match is a QC change left for him to decide.
- **Holster Setup** (`vr_holstersetup.cpp`): the torch is a slot after the zones (`ZoneTorch`), its box round its axis
  sized by its reach. Gripped, it drags like a holster (vr_flashlight_forward/_out/_up, solved in metres: `Params` now
  carries each kind's rounding, snap, limit and step); turning the holding hand turns it about its middle past 4 degrees
  (new `vr_flashlight_yaw`, `vr_flashlight_roll` with `vr_flashlight_tilt`, set by `flashlight::setBeltTurn`); both
  hands' spread or the stick sizes its grab zone (`vr_flashlight_grab_range` 0.25..4: the zone, not the model). Undo,
  Cancel, Reset Holster and Reset All cover it. No seated layout of its own (the same cvars seated).
- **Side**: `vr_flashlight_side` stays, as a mirror of the saved place and turn (out, yaw and roll mirrored to the right
  hip); no migration. Tilt now honours -90..90 (it was clamped to -30..60 though its slider went to 90).
- Menus: Flashlight > On the Belt gets Turn and Lean Sideways; Reset Body and Equipment resets tilt, yaw and roll too.
  Mock: `vr_mock_hand_to <hand> torch [along left out]` (the slot's middle); `vr_flashlight_probe` prints the slot
  (middle, reach, beam, length); `vr_holstersetup_report` prints the torch's settings.
- Tests: `flash_grab_test.py <agent> slot` (new; 17 checks: quick grabs at 2.2 and 4.4 m/s, closed 0.3 s before taken,
  1.5 s not, the drag moves it and the grab follows, the old place no longer takes, the turn keeps its middle, the stick
  enlarges the zone only, the right side mirrored; verdict "flash_grab_test slot: N passed, M failed"). `timing`: the
  former punch/away/closed controls now must take it; a stale-closed control added. Run: slot 17/0, timing 297/0,
  mounted:0, returning, otherweapon 7/0, flash_trigger 6/0, tunables_test, holstersetup_test.
- Known, not this branch: `holstersetup_test.sh` "gun at the right hip (drag) + 0 out: DROP (want IN)" fails with this
  branch's flashlight/setup files reverted to origin/next too.
- **Follow-up, no closed-hand grab** (agent/noclosedgrab, his word 2026-11-26: "disable/remove that functionality altogether,
  also for the flashlight"): `vr_flashlight_closed_grab` and its row are gone, and so is the belt branch of the late grip
  (`startLate`/`lateGrips`; the late grip stays for the head only). The belt torch now takes a grip only pressed with the hand
  in its grab zone (moving or not: the instant grab stays). The holsters were checked: `StartedHandGrabbing`'s 0.125 s is
  a press-timing window (hand in the hotspot within 0.125 s of the press edge), a hand closed earlier does not draw, so
  nothing to remove there (the mid-air catch window is for weapons flying through the air, not the holsters). Tests: slot
  17/0 (`closed` and `stale` are controls now; `quick`/`quickfast` press at the middle); timing 87 reaches taken, 15 controls
  (presses beyond ~9 cm out, the closed arrivals, are controls) 0 wrong; returning, otherweapon 7/0, tunables_test 13/0.
## revthrow: reverse grip throws settle point first more readily (branch agent/revthrow, 2026-11-26)

His words (VR, 20:11): a spear thrown overhead from the reverse grip spun with "even the smallest amount of wrist snap"; he wants
it to "almost immediately stabilize with its point forward", a little of that for swords and daggers too, each tweakable.
- A thrown weapon keeps QVR_WPNFLAG_REVGRIP (DropWeaponInHandScaled). The engine's Point-First Settling (vr_box3d_step.cpp
  `pointAlign`) reads it at the release and takes a leniency L: `vr_throw_point_rev_pole` 0.8 for pole weapons (the spear,
  the Crystal Lance: `axestick::pointPole`), `vr_throw_point_rev_blade` 0.3 for swords and daggers. With L the release's
  tumble across the blade is cut by L (all at 1), the spring is (1 + 2L) times as stiff (still critically damped), tumbles
  up to (1 + 3L) times Point-First Settling: Tumble are steadied, and full help applies from its slowest speed (3 m/s).
  L 0, or an upright throw: the same maths as before. Rows: Debug > Tunables, "Reverse Grip Settling: Spears / Blades".
- Flat-screen reversed throws carry the flag too, so they get the same leniency (shared path).
- Test `vr_embed_test 73` (QC/vr_revthrow_test.qc, vrtesthall; ends `revthrow: N passed, M failed`): scripted reverse grip
  releases (point 10 degrees up, the snap turning it down; spears at 8..20 rad/s and 10..16 m/s, sword and dagger at
  14..22 rad/s) at a wall, three passes: at the settings every spear is point first within 150 ms and stays so and sticks by
  its point (mean 6 degrees against 59 at L 0); sword and dagger steadier than at L 0; upright throws match the L 0 reversed
  ones exactly. 6/6.
## scenes

His report (2026-11-26 20:21): checklist Test buttons "don't take me to the right places"; the Quoth lines went to AD's
start map. Cause, one bug in the scene runner: the Quoth lines (ne_marb, kellbase1 buttons, lpqsp1, metmon) were tagged
`{test:hub_start}` (`map start`), and `map start` runs in whichever campaign the scene before left active (after any AD
scene: AD's own start map; VR_CheckSpawnCampaignMap). Fixes: (1) a scene without a `campaign` line now runs in Quake's
campaign first (mapFoundOrCampaignSelected switches back from an add-on; Quake's maps and `start` are found there),
else in the add-on that has the map; (2) new directive `campaign <folder>` (before `map`) for package maps found in any
campaign but played by one's rules (Quoth's metmon); (3) four new scenes quoth_ne_marb, quoth_kellbase_buttons,
quoth_lpqsp1, quoth_metmon (the four checklist lines need retagging: kit scratch/cl_lines_scenes.txt and
scratch/scenes_cl.diff in the questi tree); (4) `vr_scenario runall` prints `SCNAT|id|map|at x y z|monsters within
1200|spawn lines|start strayed` and fails a scene that landed on another map. Audit (196 scenes, headless, -game ad):
before and after 2 FAIL (the kellbase scenes: Quoth's data is absent on this base) and 2 SKIP (Dwell, Crystal Lance data);
the runner never saw the wrong-place cases (the map did load), the audit found them from the checklist tags: 4 Quoth lines
on hub_start. Not fixed: humrepl_variants (the AD soldiers line) has no `start`, and vr_ad_test 219 is a PASS/FAIL
self-test that makes the soldiers before you at ad_crucial's start. New scene pose_spear_humanoids: frozen spear enforcer,
knight and death knight in vrtesthall's open middle (start 0 -40), the Pose Studio opened by the scene.
- **Map Library package scenes (quothscenes, 2026-11-26)**. His report: quoth_metmon and quoth_lpqsp1 "not started: no map
  maps/metmon.bsp". metmon (metmon1d), lpqsp1 and ne_marb are Map Library packages (qvr_addons/<sha16>/), not in
  quoth/; a package folder is on the search path only while it is the active one, so the scene's COM_FileExists missed
  them. kellbase1 is in Quoth's own pak1/pak2 (its three scenes were fine). His Steam install has none of the three
  packages installed. Fixes (vr_scenario.cpp): (1) a scene's map is also found in an installed package
  (mapinstall::packageFor, the `map` command's own lookup); its `map` then mounts the package as the Map Library's Play
  does (Host_Map_f's VR_AddonForMapCommand), in the scene's campaign; (2) new directive `package <sha256 prefix> <name>`
  (before `map`): the package the map is in; not installed, the scene says "the Map Library package metmon1d is not
  installed: install it from the Map Library (or maps_install 59971ea4 at the console, when the index is loaded), then
  start this test again" instead of "no map". A scene never downloads (installing stays the player's choice in the Map
  Library). quoth_metmon/lpqsp1/ne_marb carry their package lines (59971ea485de52bf, 50105e2a0c4ffa3d,
  764ad7c7ef0d947f: Quaddicted's zip sha256s, as quoth_setup.py installs them). checklist_tests_check knows `package`.
  Test: `Misc/quakevr/quoth_scenes.sh <agent> [present|absent]`: present 20/0 (each package mounted, the 6 quoth_
  scenes run), absent 17/0 (the three say which package to install; the kellbase ones run). The test base needs Quoth
  (kit links.ps1) and, for present, the packages (kit bases with quoth_setup.py's install, e.g. round25's qvr_addons).
  scenario_check.sh on a base without the packages still reports those three as SCNERR (by design: they can't run).
## promofails: flat tests failing on next after his promoted tunables (branch agent/promofails, 2026-11-26)

Checked first with the pre-900606064 tunables set in the run: only flatmelee5's "rev thrust sword" depended on them; the
others failed the same with the old values (other merges: flatmelee5's 2H stance on equip, flatgrip3's one-handed axe).
His values stay the defaults.
- **flatxhair_test `flush` (3 fails)**: wall share 0. With today's defaults the shotgun 18 units from the wall is cleared
  by the push back towards the shoulder alone (flatwallturn: room -0.66, tilt 0), so nothing tilts and the wall shot is
  not exercised. Not a bug (the tilt is only what the push back can't clear). Fix, test: the case keeps the push back short
  (`tune vr_flat_wall_arm_keep 20; tune vr_flat_wall_fold 20`, reset by its tune_reset) so the tilt engages (4.0 deg).
- **flatthrust_test, flatmelee5 "rev thrust sword embeds" (and at times "rev thrust spear")**: the case read the stuck
  blade on the frame of the hit; with his flipped pose (revgrip_thrust_up 4) the hit lands frames before the drawn blade
  reaches it and sticks, so it read depth 0. Fix, test: cases 11/12 wait to be stuck (or the 0.9 s), as test 58 does.
- **flatthrust_test, flatmelee5 "2h equip axe / superaxe"**: flatgrip3's `vr_flat_1h_axe 1`. Fix, test: that run sets
  `vr_flat_1h_axe 0` (the cases are about the 2H hold).
- **flatbash_test "sword: took the blade"**: since flatmelee5 the sword is two-handed on equip (vr_flat_melee_2h 1), so
  the guard switches the held grip below the hand to the blade (vr_twohand.cpp) and the take's debug line never printed.
  Fix: the switch prints its own `2h grip: off hand took the blade from the grip below the hand (N units off it)`.
- **flatbash_test "turned 163.5 degrees a frame"**: a real snap. The free hand was drawn from the stance's grip onto the
  blade in one frame (the grip held throughout, so its blend stayed 1). Fix: `easeGripSwitch` (vr_view_hands.cpp, flat
  only): a switch between grips while held eases the drawn free hand from where it was, in the holding hand's frame,
  over 0.25 s (smoothstep); now 16.0 degrees a frame at most (the main hand's own turn).
- **flatbash_test "axe: a one-handed bash" (none, at times)**: the sword's bashed grunt, knocked back and staggered,
  stood about where the axe's grunt spawns; some runs found neither in reach. Fix, test: each weapon at its own spot
  (the axe 256 units to the side). The axe's 1H bash (flatgrip3) is what the test expects.
- embedhold_test: 2/0 on this tree (arm 1 63/0, arm 0 60/0): the reported ogre `.vr_embed_armpast` fail did not recur.
- Tests: flatxhair_test 31/0, flatthrust_test 8/0 (blades 12/0, Lance 8/0, flatmelee5 24/0), flatbash_test 20/0,
  embedhold_test 2/0, flatmelee 12/0, flatinput 19/0, flatthrow 48/0, flatswing 4/0, flatshovesmooth 6/0, flatplay 31/0,
  flatparry 43/0, flatmelee2h 16/0, flatfg2h 11/0; flatswap_test: weapons, things, melee 96/0, holster all pass; its AD
  run (vr_flatswap_test 4, map start) 47/1 then 46/2: the last case (the knight's sword, stance 1, left to right) never
  swaps (no "flat swap" line) and all hands are undrawn for 78 frames, about 4 minutes into the run; no blade grip in it,
  so not the grip switch's ease. Open: look at it on AD's start map.
## embedfire: a weapon fires where it is, in the hand or stuck in a body; the Crystal Lance first (branch agent/embedfire, 2026-11-26)

His VR notes 2026-11-26 20:24-20:27: the Lance's electric attack while it is stuck in an enemy and he still holds it, charged
before the stab or after it; not a Lance special case, a system for every weapon (later: weapons fired as props, turrets).
- **Design: one firing path, parameterised by where the weapon is.** weapons.qc `W_TriggerFrame(hand, pressed)` is the
  trigger's part of the hand's weapon frame (the Lance's charge, `W_Attack`: ammo, refire, shot, effects, sounds, network).
  `W_WeaponFrameImpl` calls it for the weapon in the hand; `VR_PropFire_Frame` (QC/vr_propfire.qc) calls it for a weapon
  prop the hand holds. A prop keeps its weapon identity (`.weapon`, `.weaponflags`, `.weaponinst`: variant, clip); for its
  trigger frame the hand takes it on (`VR_PropFire_Begin`, the armed grunts' `VR_AG_SetHand` way): the hand's weapon
  fields are the prop's, its muzzle the prop's model's muzzle (`weapondrawnpose(w, 0, 6)`, the Weapon Offsets' anchor),
  its aim the prop's line (handle to muzzle), its place the prop's handle; `VR_PropFire_End` puts the empty hand back.
  The player pays (his pouch, `delAmmoOrClipAndUpdateCounter`), his hand's refire and charge (`.dw_lc*`) go on, so a
  charge carries over hand -> stuck -> pulled out. Shots know the prop (`vr_fire_prop`, the armed grunts' `dw_beam_prop`
  made general): the beam is keyed to the prop (drawn from it), `VR_WeaponFired` flashes it.
- **Who drives it:** the hand holding it by its handle role (`.vr_hold_handle`: the hand that stabbed, or the one left on
  it); its trigger (VR) or its fire button (flat). No hand on it: nothing fires.
- **Which weapons:** weapons.txt `embed_fire` (Embedded page; 1: slot 55, the Lance held as a lance), under the rule
  `vr_embed_fire` (default 1, Carrying and Throwing > Throwing and Physics > Stuck Weapons Fire). No Lance branch in the
  path. Others: only blades embed today and none of them shoots; a bayonet gun or Mjolnir given `embed 1` would need
  `embed_fire 1` only (their shots read the same hand fields), plus `VR_WeaponFired` in their fire code for the flash on
  the prop. Not carried over for such a gun yet: the lightning gun's per-frame stream (`FrameDrawLightningTrail`) and the
  Widowmaker's spin, which run before the trigger part in `W_WeaponFrameImpl`.
- **The Lance's trace:** `dw_fire_lance` now traces from behind its tip like every hand gun (`VR_ShotBackPoint`, from the
  hand's place on its line; the beam is still drawn from the tip): a body the tip is in is hit. In the hand the same unless
  the tip is inside something; stuck, the hand's place is the prop's handle, so the bolt goes through the host first.
- **Flat conflict, decided:** flat play's fire button pulls a stuck blade (flatembed); on a stuck weapon that fires there it
  is the trigger instead (held: charge, let go full: shot), and a **tap** still pulls: let go within
  `vr_flat_embed_fire_tap` s (tunable, 0.3) of its press with nothing fired. A button held from before the hold (the charge
  brought in) is no tap. Swords, axes, the spear: unchanged (press pulls). A future instant-fire gun would fire on the press
  and could never tap-pull: it would need a hold-to-fire rule.
- **Client:** `vr_lancecharge.cpp` draws your charge on the stuck Lance (`stuckLance`: `axestick::heldEnt`'s entity), keeps
  a charge whose Lance is found nowhere for 0.25 s (the frame between hand and prop), and draws a stuck Lance's bolt from
  its tip (`boltStart` for a prop your hand holds). Other players' stuck Lance: no charge look (their hands' blades are not
  sent to other clients); their bolt is drawn from the server's muzzle (4 units off the tip).
- **Tests:** `embedfire_test.sh`: vr_embed_test 77 (VR) 29/0, 78 (flat) 22/0, script 5/0 (the charge never lost on the
  client, the stuck bolt from its tip). Regressions: dwell_lance_test 51/0, vrspear 18/0 9/0, poleembed 16/0 13/0,
  spear_test (embedspear 25/0 6/0, spear 4/0), flatthrust_test 8/0 (the Lance's press-pull, now a tap: 8/0; flatmelee5
  24/0), embedhold_test (flatembedwalk 63/0 60/0) 2/0. Debug > Tests: "Crystal Lance Fired While Stuck Check". Scene
  embed_fire. flatlance_test 29/1: "the thrust is the head's stab" (section 3's V at an
  ogre 52 ahead lands nothing); the same with `vr_embed_fire 0` (the new path off), so not this change as far as that
  shows; not checked against an origin/next build.
## flatpush: H's big props pushed by the hands as you walk into them, kept at the grab's arm length; let go of when too heavy or too steep (branch agent/flatpush, 2026-11-26)

His words (2026-11-26): hold a crate and move towards it: keep the arm length it was grabbed at, so it gets pushed; too heavy
and too fast, or the positions/angles too steep, and the hands come off. Follows flathold2 (its open gap: walking into a
dragged prop shoved it off the hands).
- **Kept at the grab's arm** (`vr_flat_grab_push` 1; vr_box3d_ragparts.cpp `dragTarget`, `World::PropBlade::keep/arm`): a
  dragged prop's held point is asked to stay level where it was taken relative to the body (along his yaw's forward and
  left, turned with him; measured: the crate 38 units ahead), not eased in to the hold-pose hand; its height is still the
  hand's (the lift's limit as before). The joint is the same spring (4 Hz, `vr_flat_grab_drag_force` level): it pushes as
  well as pulls. 0 = the old pull to the hand.
- **The push comes through the hands**: the player's capsule makes no contact with a prop he drags (preSolve, `dragsProp`),
  and Quake's move bumping it doesn't shove it (`shoveBumped`). (His move passes through what he holds, so a prop that
  can't keep up ends in him: the let-gos below come first.)
- **Let go of** (holdPropDrag, debug `vr_debug_ragdoll 1` prints why; 2 prints the push each frame): (1) too steep: the held
  point more than `vr_flat_grab_push_angle` (60 deg) round from where the arm reaches, level from the eyes (turning sharply
  away, walking past it); (2) too heavy for that pace: the force each hand's hold would need, `need` = its share of the mass
  x (w^2 x lag + 2w x slip) (the spring's, uncapped: how far and how fast the point lags its target), above
  `vr_flat_grab_push_force` (8000 N) long enough: 0.15 s weighted by how far over (a jolt is borne, a prop far too heavy goes
  at once); (3) as before, the point more than `vr_flat_grab_drag_letgo` off its target (now the kept arm's).
- **The hands drawn on it** reach `vr_flat_grab_drag_letgo` + `vr_flat_grab_reach` from the hand (vr_view_hands.cpp), since
  the point now stays up to the reach ahead.
- **Measured** (vrtesthall, needs a hand): crate 40 kg run forward 6.5k N peak (kept: 28 -> 17..26 units ahead, pushed 120
  units), backed away fine; explosive box 80 kg: pushed at 60 u/s 6.5-9.7k, walking 17k, running 25-27k (all let go at 8000:
  pushed at its top edge it tips and digs in rather than slides), dragged backing at full speed kept (at 5000 N it let go,
  why the default is 8000). So: crates, barrels and the small explosive box push at a run; the big one only drags.
- New tunables (Flatscreen > Grab / Pull): `vr_flat_grab_push` 1 (Push Held Props), `vr_flat_grab_push_force` 8000 N (Push
  Strength), `vr_flat_grab_push_angle` 60 (Push Max Angle). `vr_flatkeys_test 4` now ends with the player's origin (`me x y z`).
- Tests: flatgrabthrow_test.sh 46/0 (new run `push`: a crate pushed 68 units ahead while walking, 28 -> 22 units ahead, both
  hands on it, none let go; backing away drags it 63 back; an instant 100-degree turn lets go "too steep"; the explosive box
  run into lets go "too heavy for that pace"); tunables_test 13/0 (stages), 20/0, 3/0.
- Open: ragdolls not changed (their own hold, `RagdollGrab`, eases the limb to the hand: a follow-up if wanted); the big
  explosive box can't be pushed even slowly (Push Strength up to ~10000 lets it at a slow walk).

## Flat view size report: "the world looks tiny" (branch agent/tinyworld, 2026-11-26)

- A new flat player (Discord) said the monsters and the player look "tiiiiny". Audit of every setting that sizes the flat
  view, at its default for a first start with no headset (fresh config, Setup's flat shortcut): `fov` 90 with `fov_adapt` 1
  (106 x 74 degrees on 16:9, Quake's own Hor+), `r_scale` 1 (resolution only), `vr_flat_body_height` 1.55 m,
  `vr_world_scale` 1.2 and `vr_gunmodelscale` 0.7 (these three size only his own body, hands and held weapons in flat: the
  eye is the box's `view_ofs` 22, as in Quake), `vr_flat_speed_fov` (x1.2 only above 350 u/s, eased back),
  `vr_flat_model_fov` 0 (off), `vr_pickup_scale` 1. `vr_height_calibration` is VR-only (flat uses `vr_flat_body_height`).
  An inherited `id1/config.cfg` / `autoexec.cfg` (`fov 130` from another port) is already skipped (cmd.c
  `Cmd_ForeignStartupConfig`, 1.0.1), and Setup writes none of these. No path found that gives a first-time flat player a
  bad value (a fresh-config flat start checked: every one at its default).
- His screenshot measured (vanishing points of the walls and the floor): about 105 degrees across, the default; the view
  is pitched up about 20 degrees, which puts the flat hands below the screen (reproduced on e1m1), and the grunt is far
  back in the room behind the door. Consistent with a default setup: Quake's fov 90 is wider than most modern games.
- Added: the flat mode prints `flatscreen view: fov ..: W x H degrees in the WxH window, r_scale ..; your body .. m,
  vr_world_scale .., vr_gunmodelscale ..` at the first map (after the configs), and a `changed from the defaults:` line
  naming any of fov, fov_adapt, vr_flat_body_height, vr_world_scale, vr_gunmodelscale off default (with a note over fov
  110). It is in every run's `quakevr/logs/<date>/console.log` and in `vr_flatplay_status` (Debug menu: Flatscreen Play
  Status). `Quake/vr/vr_flatplay.cpp` `viewSizeReport`.
- Tests: flatkeys_test.sh 36/0 (smoke); the line checked by hand at the defaults and at `fov 120` / `130`.
- Open: what to ask the player for: his `quakevr/logs/<newest>/console.log` (the `flatscreen view:` line), his
  `quakevr/ironwail.cfg`, his screen resolution, and whether he changed Options > Field of View.

## A bayonet model for the shotguns (branch agent/bayonetmodel, 2026-11-26)

- What: `quakevr/progs/bayonet.mdl`, an original socket bayonet in Quake's style (234 triangles: an octagonal socket
  with a top lug that clips under a barrel, steel bands and a leather strap, a cranked leather-wrapped shank, a small
  guard, a 20 cm single-edged blade with fullers and a clipped point, edge down). Full-colour skin `bayonet.mdl_0.png`
  (1024 x 512; 8-bit 256 x 128 in Quake's palette) and a baked normal map `bayonet.mdl_0_norm.png`.
- Pipeline: `Misc/quakevr/make_bayonet.py` (the spear's way: lofts, painted full-colour skin, `relief()` for
  `normaltiles.py`; genguard-guarded), `python Misc/quakevr/bake_normals.py bayonet.mdl` (registered in normaltiles.py
  MODELS and normalmaps.py GENERATED). `--placed <folder>` writes the bayonet moved onto each gun for renders.
- Axes: one unit a world unit, +x along the blade, origin the mounting point (the socket's top front, at the barrel's
  underside at the muzzle).
- Mounts (gun model space; position, angles, scale) in `docs/vr-port/ATTACHMENTS.md`, "The bayonet", for agent/attach1:
  v_shot 32.9 0 2.84, 0 0 0, 2.2727; v_shot2 26.0 0 3.62, 0 0 0, 2.0833; g_shot0 18.2 0 33.79, 2.3048; g_shot 21.6 0
  34.86, 2.9398. The shotgun's socket stays clear of the pump's front; the SSG's sits under the barrels' shroud.
- On its own: `vr_bayonet` (QC `vr_bayonet.qc`, precached at world spawn; FGD regenerated), the toolgun's Props
  "Bayonet" (appended: entry 303), props.txt slot 71 (0.45 kg, Along the Handle -0.9..1.06, one hand, tip 6.5 0 -0.82).
  Debug menu test aid: Which Gib (vr_test_held_pick) 14 "Nearest Bayonet" (and 13 "Nearest Crate or Barrel", which
  existed without a menu entry).
- Tests: e1m1 `vr_toolgun_spawn_test 303 303` + `check`: spawned, alive, drawn, 0 precaches missing on the client;
  `vr_test_held_pick 14; impulse 252`: taken into the main hand along its handle, let go, thrown. check_mdl_art (3 LOW
  findings) and check_mdl_holes (ok). Renders for review: `scratch/bayonet_*.png` in the worktree.
- Open: it is a prop (a club with a tip), not a sword kind: no edge cuts on its own (the dagger-style anchors are in
  ATTACHMENTS.md if it becomes one). The mounts are frame 0's: the fire frames move the guns' fronts.

## attach1: weapon attachments; the bayonet on the shotgun and the super shotgun (branch agent/attach1, 2026-11-26)

His words 2026-11-26: attachments (a bayonet first) attached under most guns, offsets per weapon in Weapon Offsets, kept in
every state, synced in multiplayer. The design and the ideas list: `docs/vr-port/ATTACHMENTS.md`, "The system".
- **State**: the weapon record's `.wi_attach` (QC vr_weaponinst.qc / vr_attach.qc): hands, holsters, props, embeds, saves;
  a level change: parm77 (engine `lastExtSpawnParm` 77). Network version 48: `QVR_SVC_WEAPONATTACH` 58 by weapon id; an
  attachment byte in each gun look and holstered gun of the avatar.
- **Data / menu**: weapons.txt `att1_ok/x/y/z/pitch/yaw/roll/scale` (slots 1, 2 from bayonetmodel's mounts);
  Weapon Offsets > Attachments (page "Weapon Offsets - Attachments", appended to the pages table), the bayonet drawn on the
  held weapon while the page is open.
- **Drawing**: `view::placeAttachment` for held (VR and flat), holstered, lying and other players' guns; follows the firing
  frames by the muzzle anchor and your own SSG's barrels opening.
- **Use**: VR snap-on (`vr_attach_snap_radius` 5, `vr_attach_snap_angle` 50), pull-off by the empty hand's grip
  (`vr_attach_pull_radius` 6); flat Attach key `vr_flat_key_attach` J (impulse 52; any mode). With a bayonet the gun strikes as a
  sword-kind blade to the bayonet's tip, sticks by its point (engine `attachBlade`), fires while stuck (embedfire path);
  flat V thrusts with it.
- **Tests (attach2)**: `Misc/quakevr/attach_test.sh` (QC/vr_attach_test.qc, `vr_embed_test 79-82`; `menu_vr attachtest`).
  79 VR 14/3: snap-on by the mock off hand (aim nudged until the carried bayonet points its mounted way), pull-off (no 2H
  grip line), no straight re-snap, on again, holster/draw, drop, both guns stabbed and stuck by the point and fired there,
  fired in the hand. FAILS: the drawn tip is 3.55 units off the server's (`vr_attach_probe`), the mock hand's pick-up of the
  dropped gun, a thrown bayonet gun bounces (not stuck). 80 flat 8/5: J on/off/on (called as `VR_Attach_Key`: a console
  `impulse 70` never reached flat play: attach3), V sticks, fired stuck, SSG breaks open stuck; R reloads stuck but its trip pulls the
  gun out (and the stab's damage reads 0). 81/82: save/load 2/0; level change 0/2 (82 sees no weapon: unexplained). Menu
  6/0. Not run: the MP test, eval.sh, screenshots.
- **Fixed (attach2)**: attachinfo's muzzle back in model units (drawn = k*Scale*own; was 25 units off), turned by the gun's
  Pitch/Yaw/Roll (`view::weaponTurnInHand`; `vr_attach_turn 0` compares: little difference); the snap's angle against the
  bayonet's mounted way, not the hand's forward (69 degrees apart in the mock); a pulled-off bayonet doesn't snap back
  until 2x the radius away (`.vr_att_fresh`); the 2H grip isn't armed by a grip at the drawn attachment mount
  (`view::drawnAttachment`, vr_twohand.cpp); `VR_PropFire_End` keeps the frame's weapon flags on the prop (an SSG's spent
  shells / open); flat R and its frames run on a stuck gun (the propfire bracket in vr_flatreload.qc).
- **Fixed (attach3)**: all green: 79 VR 16/0 (+1 PENDING), 80 flat 15/0, 81/82 2/0 both ways, menu 6/0; eval.sh no new
  failures. The level change: the game kept the bayonets; tests 81/82 ran on StartFrame's leftover `self` (a phys_obj after
  the change): now on `VR_Player()`. Flat J: impulse 70 was the flat holster menu's (60..79, VR_FlatHolster_Impulses runs
  first), so J did nothing in flat play: now impulse 52 (weapons.qc ImpulseCommands; the test sends it as a console
  impulse). Flat R on a stuck gun: in place: `.vr_flatrl` + 4 off / 8 main (STAT_QVR_FLATRELOAD bits 2, 3) set while R's
  bracket has the stuck gun (`vr_fire_prop`): the client draws no trip (vr_flatreload.cpp), and VR_Embed_HoldThink
  doesn't pull while it lasts (a bayonet's tip is in shallow: -0.2 units out was enough); the test checks it is still
  stuck 0.3 s after. V's damage was dealt (38): the test read it against a 5000 the spawned grunt no longer had, and
  VR_AT_HoldGrunt reset 4000 - 38: now read just before the thrust, reset only under 1000. The tip gap: one measure,
  `attach::fromMuzzle` (vr_attach.cpp): the mount/tip through `view::modelPoint` (the ModelSpace placeAttachment draws
  with) less weaponLocalGeometry's muzzle; attachinfo uses it (the old `k * Scale * own` left out the model's Offset):
  0.03 units off. The VR pick-up: a test problem (it failed with the bayonet off too): the mock fist stays ~6 units over a
  gun on the floor; now the open hand onto it (`weaponpalm`, followed a few frames), then the press, with
  `vr_test_handcontact 1 1` (vr_pickup_test 17's aid). The thrown gun sticking: `attach: PENDING ...` (not counted).
- **Open**: ~~the gun's Box3D body leaves the bayonet out~~ (attach4); holstered/lying/other players' SSGs (closed
  barrels: not checked); ~~VR immersive reloading of a stuck gun~~ (attach4); g_shot pickups carry no record.
- **attach4 (2026-11-26)**: 79 VR 20/0 (no PENDING), 80 flat 15/0, 81/82 2/0 both ways, menu 6/0; eval.sh no new failures.
  - Thrown/dropped: a lying or flying gun's Box3D body has the attachment's hull (vr_box3d_mesh.cpp `attachmentHull`: the
    bayonet model's frame-0 vertices and its tip through the mount and the gun's drawn transform,
    `held::drawnAttachmentVertices`; one more piece beside the gun's and its seated magazine's, the gun weighing as
    before; `Slot::attach` remakes the body when it changes). The PENDING throw was mostly the test's geometry: from home
    the grunt stood on the hall's first mantle block (32 high from y 352) and the gun's stock struck its face first
    (`axestick: the hilt struck the level first`); now thrown from 160 units back, a real check (stuck by its point).
  - VR reload of a stuck gun: a shell or pair in the off hand at the stuck gun's port already went in by the lying guns'
    path (VR_Reload_PropsFrame: a stuck gun is a thrown_weapon); the super shotgun's frame now runs on the stuck gun
    (vr_reload.qc `VR_Reload_SsgFrameWhere`: VR_PropFire_Begin/End round VR_Reload_SsgFrame, the hand's load point the
    prop's from `loadportof`): B/Y and the off hand's hit from above break it open where it is, a hit from below shuts it.
    The flick and the pry stay held-only (the engine reads the hand's own gun's stats). Test 79: the stuck SSG broken
    open by B, then each stuck gun loaded by the mock off hand bringing a pouch round to its port; still stuck after.
  - MP: not done (out of budget). Aid for it: client command `vr_attach_report` (vr_view.cpp: every attachment drawn
    last frame, `attach_drawn: where=hand_main|hand_off|holster|lying|other_held|other_holster ent= kind= gun= at=`,
    then `attach_report: N drawn`). Screenshots not done.
- **bayonetfix (2026-11-27)**: his flat test: the HUD lost the ammo count with the gun stuck; after J the bayonet was held
  but the hand looked empty.
  - **HUD on a weapon prop** (general: anything embedfire fires, the Crystal Lance too): QC `updateAmmoCounter` asks
    `VR_PropFire_HudCounter` first (vr_propfire.qc): a hand empty on a prop it drives (`VR_PropFire_Held`) takes the prop's
    weapon on for the counter, so `.ammocounter`, `.weaponclipsize` and the ammo-type mirror `.currentammo` are the prop's;
    `VR_PropFire_HudFrame` (each PlayerPreThink, VR_Hand_Derive) keeps `.vr_pfire_hud` (the props' weapon ids, off + 256 x
    main) and recounts when it changes. Engine: new stat `STAT_QVR_PROPHUD` (field `vr_pfire_hud`), and the hand's
    `STAT_QVR_WEAPONCLIP` is the prop's record's clip meanwhile (vr_server.cpp). Client: `flathud::hudWeapon(hand)` (the
    prop's weapon, else the hand's) for the flat status bars' counters and clip (`VR_FlatHudHand`, `VR_FlatHudClip`) and the
    flat visor; the wrist gadget reads the counters already. The hand's own weapon stat stays the fist (drawing unchanged).
  - **The bayonet in the hand**: Held Object Offsets' Along the Handle put a prop's "head" (out of the fist on the index
    finger's side) at its weight's end before its melee tip's; the bayonet's weight is towards its socket (props.txt com_x
    -1.2), so every hold of it (J, a VR pull-off, a grab) had the blade below the fist (in flat off the screen). Now the tip's
    end first, then the weight's (vr_grip.cpp handlePlace; the torch's tip and weight agree: unchanged; no other Along the
    Handle prop has a tip). J (`VR_Attach_TakeOff(p, hand, xByKey)`) also turns the bayonet along the taking hand before
    the hold (a VR pull-off keeps it as the hand took it at the mount); it eases from the gun into the hand.
  - **Probe**: `vr_held_report` (Debug menu "Drawn in the Hands"): each hand's carried entity (the server's stat), the one
    drawn there, its model, its +x in the hand and its box corners on the screen.
  - **Tests**: attach_test.sh: 79 VR 22/0, 80 flat 17/0 (new: the HUD check on each stuck gun, the held probe after the
    take-off), 81/82 2/0 both, menu 6/0, and `Misc/quakevr/attach_probe_check.py` (the client's `vr_flathud_report` after
    each stuck gun: weapon, shown, flat's big number the clip; `vr_held_report` after the take-off: carried = drawn, the
    bayonet, blade up, flat on the screen) 10/0; the script 16/0. Test 79's stabs now let go of the off hand's grip first (it
    rested by the super shotgun and took its two-handed grip once the bayonet's new hold shifted the run: the blow read as
    no stab). embedfire_test 29/0 22/0 5/0, flathud_test 11/0.
  - Open: flat's first attach_test run showed both V thrusts failing (hurt 0) before any change here, then passing on every
    run after: a flake to watch. The stuck gun's own ammo screen (on its model) not checked.

## flatthrow3: flat throw's release carries the thing on before it leaves; an ogre's grenade caught only with a timed H (branch agent/flatthrow3, 2026-11-26)

His flat feedback 2026-11-26 (late), items 5 and 9.
- **Release follow-through (item 5):** let go of B (or the quick grenade's fire button), the follow-through starts at once
  with the thing still in the hand; it leaves `vr_flat_throw_release_time` (0.07 s, 0..0.5; 0 = at once, as before) later,
  from where the hand then is (VR_FlatThrow_From reads the drawn hand: no pop-in) and as hard as it was wound up (the speed
  by charge, aimed at the crosshair as before). QC: `VR_FlatThrow_Release` / `VR_FlatGrenade_LetGo` set the THROWN phase
  and `.vr_flatthrow_launch` / `.vr_flatgren_launch`; the frames (`VR_FlatThrow_Frame`, `VR_FlatGrenade_Frame`) throw at
  that time (`VR_FlatThrow_Throw`, `VR_FlatGrenade_Throw`, unchanged), the follow-through timed from the key's release
  (`VR_FlatThrow_FollowTime`: never over before the thing has left). Meanwhile the weapon fires nothing
  (`VR_FlatThrow_WindsUpWeapon`), B and G start nothing. No new stat bits: the client draws the THROWN phase as before,
  plus `vr_flat_throw_release_arc` (0.1 of the reach, ±0.5: the hand raised halfway from the wind-up into the
  follow-through, `flatgrenade::apply`). Every throwable: weapons, carried props, ragdolls, the quick grenade (B or fire
  button). Rows: Flatscreen > Throw "Throw: Release Delay", "Throw: Release Arc". developer 1: "carried on N s into the
  follow-through", "left the hand N s after the key's release, N units from it".
- **No auto-catch (item 9):** a flat hand's grip always reads held (vr_client.cpp `gripping`), so an empty hand took any
  live grenade passing its palm (VR_Grenade_CatchCheck / HandTouch -> Meet -> Take). Now `VR_Grenade_HandCatches` refuses
  in flat play unless H's catch window is open (`.vr_flatcatch_until`). An H press (toggle 96, or hold mode's first 97,
  not its retries) opens it for `vr_flat_grab_catch_window` (0.3 s); in it a catchable grenade whose path this frame passes
  within `vr_flat_grab_catch_reach` (1 m) of his body (origin to eyes) is taken by a free hand, main first
  (`VR_FlatGrab_CatchTry`, vr_flatgrab.qc; then the quick grenade's state, VR_FlatGrenade_Adopt); a press that catches does
  nothing else (no grab, no release). The palm's own reach/touch catch in the window too. Rows: Flatscreen > Grab / Pull
  "Catch Window", "Catch Reach". Other auto-catches in flat: none found (VR_Carry_Handtouch needs a fresh grip press,
  which flat never makes; force grab is E, deliberate).
- **VR (asked):** unchanged. A headset's hand catches only with the grip held or closing as the grenade meets the palm (an
  open palm stops it and waits vr_grenade_catch_window for the grip): already a deliberate grab.
- **Files:** QC/vr_flatgrenade.qc, vr_flatgrab.qc, vr_grenade.qc, vr_flatitems_test.qc, vr_cvars_gen.qc; Quake/vr/
  vr_flatgrenade.cpp, vr_tunables.inc, vr_tunables_rows.inc; Misc/quakevr/flatthrow_test.sh, flatthrow_check.py;
  scenario quakevr/scenarios/flat_ogre_catch.txt.
- **Tests:** flatthrow_test.sh 54/0: new run `release` (the nail gun with Release Delay 0 leaves at B's release, 3.4 units
  ahead of the eye; with 0.07 it leaves 0.069 s later, 16.5 ahead, 0.0 units from the hand; the quick grenade's fire-button
  throw 0.069 s); `guide` = `vr_flatitems_test 12` extended to 13/0 (new cases: a grenade at the free palm flies on without
  H, caught after an H press; H pressed and its window let go by: flies on; thrown at his face from 90 units: caught with H
  before it meets him, meets him without); `move` starts standing/running at y -380 (the running throw, now leaving ~35
  units further on, bounced into what stands at y 350). tunables_test 13/0.
- **Open:** flatgrabthrow_test.sh fails (27, "nothing grabbable in reach": TryGrab's reach check, untouched here; its
  crates sit past the promoted `vr_flat_grab_reach` 0.65 m, b5629706a): its distances need updating (not done here).
- **attach5 (2026-11-26): the MP test.** `mp_test.py attach` (suite `mp_attach`): a listen host A (VR), B and C (flat) in coop
  e1m1. Server aids (vr_attach_test.qc, `vr_dm_test` 25-27, one-shot): 25 a bayonet into the empty off hand of player
  `vr_dm_test_who` (an entity number: coop's test impulses are off for clients, deathmatch has none); 26 the server's
  report, `attachmp: player ent= main=wid,kind off= holsters=...`, `attachmp: lying wid= attach= stuck= speed= at=`, the
  monsters within 160 units. The clients' side is attach4's `vr_attach_report`; a lying gun is matched by place (within
  48 units of the server's gun), not entity number.
  - PASS: J on B's shotgun (record and own hand); A and C draw it on B's gun held, holstered (flat holster menu impulse 74,
    the left upper holster: the shoulders' are not drawn on the copy and his hips hold his start's axe and shotgun), drawn
    again, dropped (impulse 79: A, B and C draw it lying); a thrown gun's record keeps it and A and B draw it; B armed again,
    V sticks the shotgun in a grunt by the bayonet (30 -> 1 health, `stuck=1`, the record's bayonet on it), A and C draw it
    on the stuck gun; in deathmatch (changelevel: a listen host's `map` drops the remote clients) B draws his holstered
    shotgun (impulse 71) and J puts a bayonet on it.
  - Fixed: other players' holstered guns were drawn with their bayonet but never listed by `vr_attach_report`
    (vr_mpavatar.cpp drawHolsteredParts / wearHolsters now pass the player's entity to drawGunParts).
  - Green after the follow-up: 26/0. C (flat) takes the dropped gun with H standing beside it looking down (the grab
    reaches 0.65 m, 20 units, level from his eyes; placing him nudges the gun, so its place is read again before each of up
    to six tries, from each side in turn); A and B draw the bayonet on his gun; he throws it with the throw key held down
    (`vr_mock_key b`: his client sends 40 while the key is up, so console impulses 39/40 only dropped it), and A and B draw
    it where the thrown gun comes to rest (64+ units ahead). Deathmatch has no setpos: `vr_dm_test 27` puts player
    `vr_dm_test_who` at `vr_dm_test_x/y/z` facing the nearest other living player; B's V thrust then hurts A. The stab's
    stuck gun is told from the thrown one (which can stick in the level) by place. Found no sync bug. Screenshots dropped
    (he looks in game).
## flatxhair2: every melee weapon's crosshair on where its blow lands, either grip; the wound-up throw's on where it lands (branch agent/flatxhair2, 2026-11-26)

His flat notes 2026-11-26, items 1 and 6.
- **Melee (6).** `flatplay::noteStrikePath` (poseHands, right before the swing and the thrust) rehearses hand h's blow on a
  copy of the hand as posed now: the running blow's (a V thrust too, a bayonet's), else the one its attack button would
  start (`punchKindOf`, split out of startPunch and used by it: a melee weapon's overhead swing, upright or flipped; a melee
  pole's thrust). `PunchRehearsal` sets the hand's punch state and clock for it (and puts them back); the pose's own
  `swingPose` (overheadSwing + its turn, now also poseHands') and `thrustPose` move the copy to 13 times through the strike;
  the muzzle as last placed in the hand (`flatsight::muzzleInHand`, the server's blade line's far end) rides on each.
  `flatplay::strikePath` hands flatxhair the 13 tip points; it traces them segment by segment: the first thing met is where
  the blow lands, nothing in reach the tip's end. Replaces `noteThrustPath`/`thrustPath` (the flipped spear's sums). A melee
  weapon's crosshair is now shown whatever its own crosshair mode (swords, daggers, axes had none: the centre).
  Same tunable, renamed in the menu: `vr_flat_xhair_thrust_path` 1, "Melee Crosshair" (Shot Line / Blow Path).
  Measured (e1m1's wall, 12 units off; the QC's melee mark against the crosshair as the press went in): sword 2.7, flipped
  sword 2.5, dagger 2.0, axe 3.4, hammer 5.5, mace 3.0, crowbar 3.7 units apart (the axe, hammer, mace and crowbar have no
  flipped hold: the same). The flipped sword lands high (the raised fist's blow): its crosshair goes there.
- **Throw (1).** QC `VR_FlatThrow_Aim` (each server frame from VR_FlatThrow_Frame): while the throw key winds up (a weapon,
  a carried thing, a body) or the quick grenade is wound up by the key, where letting go now would land it, sent to the view
  by the new builtin `flatthrowaim(player, hand, at)` (the listen server's own player only; `flatxhair::noteThrowAim`).
  The release's own pieces, split out and used by the release too: `VR_FlatThrow_Speed` / `_Up` / `_RagVel`,
  `VR_FlatGrenade_Speed` / `_Up`, `VR_Throw_WeaponVel` (DropWeaponInHandScaled), `VR_Carry_ThrowVel` (VR_Carry_Release);
  then the throw assist, his own motion and the throw's gravity; `VR_Throw_ArcEnd` steps the arc 0.02 s at a time (4 s at
  most), a traceline each step (monsters too, past him and the thing in the hand). The spin turns the thing about its middle,
  not its way: not modelled. The hand's crosshair is shown on it while fresh (0.1 s), whatever it holds, never dimmed by
  the wall or a reload. Tunable `vr_flat_xhair_throw_aim` 1 ("Throw Crosshair", Debug > Tunables > Flatscreen > Crosshair).
  `vr_flat_grenade_debug 2` prints `flat throw: aim`; developer 1 prints `flat throw: first hit at` / `hit from` (the frame
  before) and the landed line now ends with where.
  Measured (vrtutorial lesson 12's wall; the arc from the frame before the hit on to the wall): sword from 164 units 3.0
  units apart, axe from 294 units 9.2: its launch and its fall are the prediction's exactly (velocity 25.4 -484.8 25.4,
  gravity 312), but the thrown axe's origin flies about 10 units over the hand's arc from the start (where the thrown
  body is placed off the hand: not chased). Open: start the arc from the thrown thing's own place (CreateThrownWeapon's /
  the rigid body's), not the hand's.
- Tests: `flatxhair_melee_test.sh` 21/1: new per weapon and grip (11) and the two throws; the one fail is the old `flip24`
  case, 7.3 units apart (was 2.0 when flatxhair landed): the flipped pole's tunables were promoted since
  (vr_flat_revgrip_pole_out -2, _roll 0, _down 0); the rehearsal gives the same said point as the old sums did. The flipped
  dagger's swing at a wall leaves no melee mark (no landing to compare): left out. Not run: the full flat suite.
- Code shared, not copied: the swing and the thrust are the pose's own functions; the throw's speed, launch velocities and
  gravity are the release's own helpers.


## Map-made armed humanoids hold their guns right: Quoth's kellbase1 enforcers (branch agent/quothpose, 2026-11-26)

His note: kellbase1's enforcer with the Centurion's plasma rifle had its left arm off and its elbow flung out; the same
enforcer and gun spawned on vrragtriggers was right.
- Cause: AD's and Quoth's soldiers (eliminator, defender, pyro... `VR_HUM_Replace`) become armed humanoids inside the
  map's spawn functions, and QC measures the held gun's points then, once (`VR_AGP_Geo`: muzzle, butt, second grip, kept
  on the weapon entity). During spawn `sv.active` is still false and the client has not connected, so
  `weapons::modelTransform` (vr_weapons.cpp) saw no Quake VR protocol and gave no transform: every gun was measured at
  its model's own size (the Centurion's gun's muzzle 33 units ahead instead of 12, the burst rifle variants all alike).
  The pose code took the long gun's butt and grips from that, the gun sat 10 units across the chest and the left arm
  reached round it. A toolgun spawn (after spawn) measured right. Not Quoth-only: any armed humanoid made at a map's
  spawn with a gun (AD's maps too); the id maps' armed grunts get theirs rolled later.
- Fix: `modelTransform` also takes the server's protocol while the server's QC runs during loading
  (`sv.state == ss_loading && qcvm == &sv.qcvm`). The comment there already named the same hole for the first frames
  (thrown weapons' hulls three times too big); hulls and boxes made during spawn are now the drawn size too.
- Debug: with `vr_agrunt_ik_debug 1`, `vr_agholds_report` (Debug > Armed Humanoids: Weapon Holds Report) adds `agarm`
  lines: each arm's hand and elbow off its shoulder (model space) and the body's model, frame, scale.
  `vr_agholds_pose save|pin|check|done` (vr_hitreact.cpp) compares the arms of those holding one gun model to a reference.
- Test: `Misc/quakevr/quothpose_test.sh` (needs Quoth 2.2): scene `agrunt_enf_centurion` (vrragtriggers, frozen, the
  reference, pinned within 1 unit), then `quoth_kellbase_enforcer` (campaign quoth, frozen): both kellbase1 eliminators
  within 1.5 units of it (before the fix about 10 units off). 2/0.
- Kit: Quoth was missing from the kit's bases; `qvr-kit/links.ps1` now mirrors his Steam `quoth` as it does `ad`
  (hard links for the paks and txt, no .cfg or .bat) into `qbase/quoth` and links it into each agent's bases.
- Known, not mine: `reload/pouch_test.sh` fails "super shotgun (main hand, dual-wielded): fills too" with or without
  this change.
## flathold3: hands drawn on what they hold never leave the wrists (branch agent/flathold3, 2026-11-26)

His words (flat feedback 2026-11-26 late, item 2): grabbing crates with H, "the hands detach from the wrists when the player is
too far or the prop gets too far. This should not happen ever."
- **Cause**: flatpush let a dragged prop's drawn hand reach `vr_flat_grab_drag_letgo` + `vr_flat_grab_reach` from its own
  place (limbDrawnHand's `stick`), past what the body's arm reaches: the arm stopped at full stretch, the hand went on.
  Measured backing away from a crate too heavy to follow: the wrist 116 cm past the arm.
- **Fix** (vr_view_hands.cpp, end of `placeController`): whatever a hand is drawn on (a prop held in both hands, a ragdoll's
  limb, a dragged prop, a tug's weapon, the other hand's weapon, a hold, an enemy, a blade, a gib), it goes only as far towards
  it from its own place as its arm reaches: one helper, `keepOnArm` (the embedded blade's, embedhold2), now for all of them.
  It also pulls the hand straight in towards the shoulder when its own place is out of reach too, and returns how far it was
  past. Flat play only (keepOnArm needs the drawn body's arm; VR unchanged). Past the reach the hand stays at full stretch,
  off the prop, until the hold lets go (the let-gos unchanged: too far, too heavy for the pace, too steep).
- **Probe**: `vr_hold_arm_probe 1` / `report` (vr_view_diag.cpp; Debug > Reports: Held Hands on Arms: Start / Report): every
  frame a hand is drawn on a held limb or a dragged prop, the most its wrist was past its arm (avatar::embedArmGapCm) and how
  often the hand was pulled in.
- Tests: flatgrabthrow_test.sh new run `far`: the crate (Drag Strength 100 N) backed away from until it lets go: 34 frames
  held, wrist past by 0.00 cm (116 before), pulled in on 28; a ragdoll held while backing away then left behind: 150 frames,
  0.00 cm. The test now pins `vr_flat_grab_reach 1.25` and `vr_flat_grab_hold_down 0.18` (flathold2's): his promoted 0.65 m
  reach grabbed none of its props (27 of 46 failed on origin/next), and with Hold Down 0 the push run's crate, backed away
  from, lagged 38.0 units off the hand, just past Let Go's 37.8, and was let go of.
- Open: with his defaults (Hold Down 0) a dragged crate backed away from at a walk may be let go of at once (above): if
  dragging backwards should keep it, Let Go (`vr_flat_grab_drag_letgo`, 1.2 m) up a little. Ragdolls were within reach
  already (vr_ragdoll_hand_stick); now clamped too.
## flatparry3: the reverse-grip guard; the flipped pole's other hand tunables; the judicator's two-handed grip on its handle (branch agent/flatparry3, 2026-11-26)

His flat notes 2026-11-26, items 3, 4 and 8.
- **Reverse-grip guard** (item 3, "the transition from 2H reverse grip to parry pose is too abrupt... we do need reverse
  grip parry poses"): by default (`vr_flat_parry_rev` 1) a flipped grip's guard keeps the grip flipped and takes its own
  pose: the fist at its own side, the blade out of the little finger's side laid across the view (a hammer-fist guard).
  vr_flatplay.cpp `parryPose` now computes a guard from a `GuardSet` (`guardOf`): the upright set (vr_flat_parry_*, the hand
  first rolled a quarter turn in) and the flipped set (`vr_flat_parry_rev_fwd` 13, `_up` 1, `_out` 6, `_pitch` 5, `_yaw` 0,
  `_roll` 0; the hand first rolled a quarter turn out), blended by the drawn flip's share (`revGuardShare`), so a flip in
  the guard goes from one to the other as it turns. Its own times `vr_flat_parry_rev_in_time` / `_out_time` 0.2 s (the
  upright's 0.05 / 0.1). `revGripPose` (the raised flipped hold) gives way to it by the guard's share. Poles and blades alike
  (sword, dagger, spear, Crystal Lance). `vr_flat_parry_rev` 0: flatparry2's unflip (`vr_flat_parry_unflip_time`), kept.
  In the reverse guard a free hand already on the flipped weapon's handle keeps it (`bladeWanted`'s keepsHandle): taking
  the blade, it let go of it as the guard ended and nothing took the flipped handle again. An empty free hand still takes
  the blade as in the upright guard. Rows: Tunables > Flatscreen > Parry Guard, "Reverse-Grip Guard" and "Reverse Guard ...".
  Status: `vr_flatplay_status` "parry flip: ... reverse-grip guard share".
- **Flipped pole's other hand** (item 4): `vr_flat_revgrip_pole_off_up` / `_off_out` (units, default 0) and `_off_pitch`
  -40 / `_off_yaw` 34 / `_off_roll` 0 (degrees; the upright's values, so nothing changes by default), by the flip's share in
  every flipped state (held, guarding, thrust). The turns replace vr_flat_pole_off_pitch/_yaw/_roll by the flip's share
  (vr_view_hands.cpp, the spear pose's turn); the moves are drawn after the shaft grip (vr_flat_pole_off_snap closed the
  hand round the shaft again, so a move of the grip point, as the upright's vr_flat_pole_off_up does, moved the drawn
  hand 0.9 of 4 u). Rows "Flipped Spear Other Hand Up / Out / Pitch / Yaw / Roll".
- **The judicator's two-handed grip** (item 8): not the shaft fallback (that is the guard's alone, flatplay::bladeAsked):
  its one Grip hotspot, the death knight's sword's copied, lies 10.7 u up the weapon from the holding hand (its hand sits
  lower on a longer handle: 2h_hand_av z -14.8 against the knight's -7.5), so vr_flat_melee_grip's "lowest grip" was on the
  blade. General fix (vr_view_weapon.cpp chooseGrip): flat play, a weapon held as a sword (twohand::swordHeld) whose grip
  lies above the holding hand or less than `vr_flat_melee_grip_below` (4 u) under it is taken that far below it. Judicator
  10.7 -> -4.0 u; knight's sword -4.5, hammer -6.1, mace -7.5 unchanged; fury -4.0. (A Grip hotspot he sets below the hand
  wins.) The judicator in the flipped grip still lets go of its handle at the flip (its grip's angle limit): not changed.
- Status: `vr_flatplay_status` "two-handed stance: the off hand N u along the weapon from the holding hand ... tip ...;
  holds its grip" and its drawn offset and angles (melee and pole weapons).
- Tests: `Misc/quakevr/flatparry2_test.sh` 54/0 (AD mounted): the flip runs under vr_flat_parry_rev 0 (the unflip's most
  turn a frame into the guard 22.6 / 22.3 degrees); the reverse guard (knight's sword held in two hands then flipped,
  judicator, spear): flip stays 1.00, guard share 1.00, parries the probe, most turn a frame into it 8.7 / 9.1 / 15.9 and
  out 8.6 / 8.9 / 15.7 (20 at most); the knight's sword's free hand on the handle in it and after (-5.2 u); every weapon
  not parrying: the free hand at or below the holding hand; the flipped spear's other hand moved 4.9 u / turned 43.9
  degrees by _off_up 4 / _off_pitch 0. Open: the judicator's flipped one-handed guard lets go of the blade at its end with
  the free hand turning 81 degrees in a frame (the upright guard's let-go, not this change's).
## floataxe: a thrown axe whose blow takes off the head it hit falls, never left floating (branch agent/floataxe, 2026-11-26)

- **His report (flat):** an axe thrown at a zombie's head popped it, and the axe stayed floating in mid air after the zombie
  died.
- **Cause:** `VR_AxeStick_Stuck` (QC/vr_axestick.qc) deals the throw's blow (`forcegrabbable_touch`, `VR_Decap_After`) after
  the engine has stuck the blade in the body. When that blow beheads or pops the head the blade is in (`VR_Decap_Cut` ->
  `VR_AxeStick_CutPrepare`/`_CutDone`), the blade falls out at once (`VR_AxeStick_Fall`: `AXESTICK_OUT`, `MOVETYPE_TOSS`).
  Then the rest of `VR_AxeStick_Stuck` ran its stuck set-up anyway (`MOVETYPE_NONE`, velocity 0), so the blade was held
  still, stuck in nothing. Its think never let it go, because nothing it is in is left to check. This hit any monster whose
  head comes off from a thrown axe's own blow (a zombie always; a grunt or knight at a killing blow), in VR and flat alike.
- **Fix:** the stuck set-up is skipped when the blow already let go of the blade, so it falls from where the head was with
  the body's motion (`vr_debug_axestick 1`: "struck, and fell out of"). The other host-removal cases already worked and are
  now tested: a body gibbed whole, a body removed, a zombie gibbed (its head gib is the same entity with another model);
  the think finds the model changed or gone within 0.02 s. A blade held in a hand gets its hand back
  (`VR_Embed_Free`, tests 25/26). A broken crate frees it (test 30).
- **Choice:** the blade falls rather than riding the flying head gib. The brief allowed either. Falling is the existing rule
  for every other host change.
- **Test:** `vr_embed_test 83` (QC/vr_floataxe_test.qc; map vrtesthall, god, notarget; ends `floataxe: N passed, M failed`):
  an axe thrown edge first at 14 m/s at a zombie's head and at a grunt's head (health 1: the head comes off), and into a
  grunt's chest (then gibbed whole, or removed) and a zombie's chest (then gibbed). 1 s later, in every case, it never
  floated (held still in nothing or in a gone body for no more than 3 frames) and it fell at least 8 units. 15/0. Without
  the fix: 11/4 (both head cases floating, 0 units fallen).

## flatlance_test: the Lance's flat thrust test ogre within reach (branch agent/lancestab, 2026-11-27)

`Misc/quakevr/flatlance_test.sh` failed 29/1 on next ("the thrust is the head's stab": no thrust landed). Culprit:
900606064 "tunables: promote his values" (in merge 363f0b1ab), `vr_flat_pole_off_up` 5 -> 0 (the other hand on the pole,
not 5 above it). Not a code bug: the thrust's game line and the drawn Lance agree (the QC sweep's tip and the drawn tip
both reach about 38 units ahead of the player, belly height); an ogre at 52 has its belly right at that reach, so the
stab only grazed it at the thrust's end, and the off hand's 2H pull (a unit or so sideways) decided graze or miss. The
ogre now spawns at 50: the head lands mid-strike at full speed (12.8 m/s, 58 damage) with his pose and with the old one.
The "ogre hurt" check compared health against a fixed 200 and passed with nothing landing (ogres there spawn at 180);
it now reads the health just before the thrust and wants it lower after. Test only, no code. 30/0.

## questcrash2: his Quest config no longer crashes at vrstart; old "left Standalone" configs repaired at start (branch agent/questcrash2, 2026-11-27)

His Quest 3 (bf4eecc2f, beta5-dev build 15431): flat lighting, the menu crashed, then every start crashed at vrstart.
- **The crashes** (logcat 00:19:29 and 00:19:43, crash reports pulled with `vr_android_export`): both SIGSEGV 0x30 in
  libGLESv2_adreno under libvrapiimpl from `xrEndFrame` (OpenXrBackend::endFrame <- VR_RenderView), questcrash's
  signature. The second came at the first world frame, right after the `world|OIT 1` programs; the first a few
  seconds into vrstart as the menu opened. His `ironwail.cfg` (exported read-only) still held what builds before
  questcrash left after Standalone -> "Quake (off)": `r_oit 1`, no `vr_xr_foveation` / `vr_render_scale` / `vr_dynres`
  (Off, full resolution, no dynamic resolution) and Quake (off)'s look rows. So the eyes went through the post-process
  (OIT keeps them off the straight-in path) into plain images (questcrash's fix made Off plain, which this config then
  used at every start): late acquire, the image's invalidate, OIT's programs, 14 fps (unworn, measured). Unworn runs
  never draw enough eye frames to crash (the runtime stops an unworn session after ~10 s), so the cause is reasoned,
  not reproduced: every crash of this signature since C4p had the post-process path's late order or a switch into it.
- **Flat lighting**: his config's Quake (off) look (vr_model_lighting 0, vr_colored_lights 0, the light grid off...),
  not a bug. Headset eyeshots at vrstart: Standalone lightmapped and coloured, Quake (off) flatter and darker; the
  lightmaps are there in both (questrender's packing fine on ES).
- **Fixes** (Quest and `-glescompat` only; the PC unchanged):
  - `vr_quest_check` (every Android start after the config): OIT saved on the Quest marks such a config (no Quest
    preset turns OIT on), and its budget rows go to the nearest Standalone level once, as a pick does since
    questcrash; the look's rows stay. Said on the console. His config: 22 rows at Standalone (Quest 2 (72 Hz)): scale
    0.65, dynamic resolution, foveation Low, OIT off.
  - `r_oit 0` is a Quest must-have (held at start and after every pick; `vr_quest_guard 0` only says it).
  - vr_stereo.cpp: on the Quest the post-process path always takes the foveated images' order (the image acquired
    before the scene, never invalidated), plain images too (Quest Foveation Off, bloom). `vr_xr_foveation_probe 1`
    gives back the late order (the A/B).
- **Headset** (Quest 3, 00:21-00:45, unworn, `-noconfigwrite -unattended`, his config): the fixed APK repaired his
  config at start, vrstart at 72/72 (was 14), menu open/close, Quest Foveation 1 -> 0 (post-process, early order) ->
  menu with Off -> 3, Quake (off), Off again, Standalone: no crash. Installed the questinst way (APK `install -r`, data
  already in sync: 0 files); his config file untouched (it is repaired in memory at each start and saved by the game on
  its next normal quit). Rollback: `scratch/qc2/qc2_installed_before.apk` (bf4eecc2f). **To confirm worn**: start, open
  the menu, Quest Foveation High <-> Off.
- Tests: questcrash_preset_test 23/0 (new: an old config repaired by `vr_quest_check`, look rows kept), haze_preset_test
  8/0, stereo_targets_test 12/0.

## grenaim: the quick grenade wound up by the attack button shows its landing too (branch agent/grenaim, 2026-11-27)

His flat note 2026-11-27 item 3 ("Throwing a live grenade with the throw button shows the dynamic crosshair target position,
but not with the attack button"), folded into flatxhair2's throw aim:
- `VR_FlatThrow_Aim` (QC vr_flatgrenade.qc) now aims the quick grenade's wind-up whatever winds it up: B, the main or the
  off hand's attack button held on it (`vr_flat_grenade_arm_mode` 1: a press arms, the next held winds up), a live grenade
  taken another way (force grab, catch: VR_FlatGrenade_Adopt). It was gated on `vr_flatgren_bykey` (B's only). Every flat
  throw input path now goes through the one aim: the throw key's VR_FLATTHROW_WIND (any hand, any kind) and the quick
  grenade's VR_FLATGREN_WIND (B or fire, either hand); no other path winds up a throw.
- The quick grenade's throw (VR_FlatGrenade_Throw) also sets `vr_flatthrow_last` / `_from` / `_watch` (developer 1): its
  "flat throw: first hit at" / "landed: grenade" lines, as the throw key's things (the globals moved up the file). Each
  throw resets the watched one (`vr_flatthrow_hitwho`): a grenade gone off frees its slot, and the next throw reusing it
  was taken for the one already printed (no "first hit" at all).
- Tests: `flatxhair_melee_test.sh` 24/1 (new: grenb, grenatk, grenoff from 294 units: the crosshair 1.8, 2.6 and 2.6 units
  from where the grenade first meets the wall, under the axe's 12; 6.5-7.9 before origin/next's release time 0.025; the
  one fail is the known `flip24`). `flatthrow_test.sh` 54/0, after fitting it to his promoted defaults
  (vr_flat_throw_speed_min/max 8/16, vr_flat_grab_catch_window 0.1, _reach 0.3; his values kept):
  - move: the running throw (x 0) now flew far enough for its bounce to meet the mantle blocks (x -32..192, y 352-416):
    it runs at x -280 (clear between the table and the slope); landed 777 units against 654 standing. The checks were
    relative already (running further than standing, his motion kept), so no number follows the speeds.
  - guide (vr_flatitems_test 12, case 4 "thrown at him, H as it nears"): H pressed as the grenade left 90 units away
    opened a 0.1 s window that shut before it arrived (0.2 s flight). H is now pressed as it nears (half a window from his
    face: a condition, not a wait), and caught; at his 0.3 m reach the free palm's catch in H's window takes it before
    H's own near-the-body catch, so the check takes either (held, not hit); case 5 (no H) still meets him. Not a
    behaviour problem: a 0.1 s press before arrival can be timed, though it is tight against a fast grenade.
  Checklist: scenario flat_quick_grenade.

## cltidy9: checklist sub-headings (2026-11-27)

The checklist file gained "## Name" sub-headings (parser: vr_checklist.cpp `subheading`, drawn as "> Name" headers by pageChecklist); every section is split into categories and [New] sorted. Details in CHECKLIST_REORG.md, ninth pass.


## beta6: 1.2.0-beta6 stopped before building: two release-test regressions fixed (branch agent/beta6, 2026-11-27)

- **Notes:** RELEASE_NOTES_1.2.md "New in beta 6" (intro for beta 6; beta 5's section now "Beta 5 (2026-11-25)"): Quest
  standalone (crash fixes, first hub load ~17 s, presets within the Quest's limits, relaunch, app name), loading (panel,
  shipped cache), flat play, VR (pole grip, reverse-grip throws, belt flashlight, rungs), weapons (bayonet, Lance firing
  embedded), physics and monsters (Box3D, lift shafts), AD/Quoth/Dwell, Setup, network version 48 (everyone updates).
  PATCH_NOTES_1.1.1.md the same by area (Campaigns, Multiplayer beta, Weapons, Movement, Monsters, Performance, Quest,
  Installer).
- **APK:** `Android/release_apk.sh build` after "Version 1.2.0-beta6": release, version code 1020006; check_apk 29/0;
  apksigner CN=Quake VR, SHA-256 504ff939...9119c0 = Android/release-cert.sha256.
- **make_release** (beta4's command, -Version 1.2.0-beta6 -BumpVersion ... -Notes RELEASE_NOTES_1.2.md -Apk <apk>):
  dry run clean; the beta profile **11 passed, 1 failed: scenario_check** (210 run, 4 failed), so nothing built, tagged
  or published. Not flakes, two scene regressions of 11-26:
  - `quoth_lpqsp1`, `quoth_metmon`, `quoth_ne_marb` (quothscenes): a scene whose Map Library package isn't installed
    says so by design (SCNERR, "not started", SCNRUN FAIL) and scenario_check counted it a failure on any PC without
    those packages. `scenario_check.sh` now leaves those scenes' lines out (matched by their line start "N|the Map
    Library package ": run.sh wraps the long lines) and lets SCNSUM's "with errors" and SCNRUNSUM's "failed" hold exactly
    that many; it prints "N Map Library package scene(s) not run". (Counted with `grep -c`: a `sort` in a nohup'd
    run can be Windows' sort.exe.)
  - `quest_foveation` (questrender): `cmd vr_foveated_debug 1` leaked the cvar (SCNLEAK counts as a failure); now a
    `cvar` line, put back after the scene.
  - Check: scenario_check rerun with the scene fix: only the 3 package scenes failed, no leak; its verdict logic replayed
    on that log: PASS (and on the release run's log: FAIL, the leak's 4th failure).
  - Full scenario_check rerun on c534df741 (both fixes): **PASS** (210 run, 3 failed = the 3 package scenes, 2 skipped).
- **Waiting for the coordinator's go** (brief: any non-flaky failure stops before publishing). Not merged with
  origin/next (judicator landed since): the release is cut from the next it started on. Then: the same make_release
  command on agent/beta6's HEAD, the online checks, the Quest install (cert = installed app's) and data sync.
- **Headset (read only, lock taken and released):** the Quest has a DEBUG-signed app (CN=Android Debug, SHA-256
  76226902...f39929; versionName 1.2.0-beta5, versionCode 1: a dev build), not beta5's release-signed one: the release
  APK's cert differs, so it can't be installed over it (install -r refuses; never uninstall: his saves). Pulled copy:
  scratch/b6_installed_before.apk. Nothing changed on the headset.
- **Second make_release (coordinator's go) on 6ff0c383e: stopped again, nothing published.** Beta profile 11/1:
  scenario_check 210 run, 8 failed: the 3 allowed package scenes plus 5 vrtesthall scenes 'placed in a wall'
  (pd_jolts, pose_enforcers_guns/_melee, pose_grunts_guns/_melee; spawns 120-147 left of the start). scenario_check
  alone right after, same tree: PASS (3 failed = the package scenes). Not seen in the first make_release run either
  (also parallel lanes): intermittent, maybe the suite's parallel lanes (a lane writing vrtesthall's collision cache
  while this one reads it?) - not investigated. Raw log: scratch/beta6_scn_release2_raw.log.
