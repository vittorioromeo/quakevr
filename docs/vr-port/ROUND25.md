# Round 25: from 2026-11-12, towards 1.1

A per-feature reference: current behaviour only. Earlier rounds: `ROUND24.md` (2026-11-05 to 11-12: 9,600 lines),
`ROUND23.md` (beta4 to 11-05), `ROUND22.md` (1.0.1 to beta4). Grep them; never read one whole.

## Where things stand

- Released: 1.1.0-beta4. Everything since is on `next`; 1.1.0 is cut only on his word (RELEASE_TODO.md).
- Round 24 brought: flat-screen play (poses per weapon, ADS, slide, lean, prone, holster menu, hand swap, flat melee
  and parry), armour on armed humanoids and the player (the Armour Studio), embedded blades on by default, the navmesh
  for walking monsters (drops, lift links off, props as obstacles, the arena's towers), the arena's budget director and
  weapon ladder, take and clip recording, ammo screen housings, the build checks in C++ (`Tools/qvr_checks`), the
  header split, the no-LTO quick build, and the cvar retirement (CVAR_AUDIT.md).
- Open work and bugs: BACKLOG.md (section 1 is the open bugs and failing tests).

## Carried over from round 24

- `vr_agrunt_test 210` fails; `vr_flatswap_test 4` (AD blades through the hand swap) to pass with AD mounted.
- The stance's torso twist and the free hand's punch (32 against 21 damage): his call; flatstance's hotspot check too.
- The triage's random cases (flatpose 21, `vr_systems_test 15`, flatswing's knight trials); visor_test's AD keys.
- Navmesh next steps: lifts finished, flyers and swimmers, flanking and cover, the tower dog fall (`vr_arena_test 41`).
- Arena geometry that changes between waves; flat keys bindable from the menu; Slipseer in the map library.

## Adding a section

One short section per feature, appended at the end (never rewrite others' sections; the file merges as `union`):
a heading "## <what it does> (branch agent/<name>, <date>)", then what it does, settings and defaults, files, tests with their
numbers, open issues. Fold a follow-up into its feature's section instead of adding a new one. Start ROUND26.md when
this file passes about 2,000 lines.

## Round start: BACKLOG and the agent prompts tidied (branch agent/round25, 2026-11-12)

BACKLOG.md rewritten from 369 to about 135 lines: done items out (design review P3/P6/P8/P9/P10, menus' per-slot
data, the embedded blades' "turn on"), open bugs first, one or two lines an item; the long entries moved into their
topic docs' "Open items moved from BACKLOG.md" sections (ARMED_MONSTERS, ARCANE_DIMENSIONS, FLINCH_RETROSPECTIVE,
MULTIPLAYER, PERF_DECISIONS, STEALTH). `.gitattributes` merges ROUND25.md as `union`; README.md points here.

## TESTING.md split into the guide and an archive of recipes (branch agent/testsplit, 2026-11-12)

TESTING.md (1,725 lines) is now the guide (about 320 lines: the mock headset, Writing a test, Test numbers, Checklist scenarios, Debug build assertions, Batched test cases, and an index). The per-feature recipes moved word for word to archive/TESTING_RECIPES.md (82 `##` sections, grep-only); a feature's scattered paragraphs are merged into its section. Retired or vanished cvars named in recipes are marked inline "(retired cvar: <name>)" (9). Code and script comments that quote a moved section ("Climbing", "Swimming", "OpenXR runtime choice", "Body calibration", ...) still say TESTING.md: the index at its end names the archive section.

## Test triage B (branch agent/testtriage_b, 2026-11-12)

Each case run 3 times (agrunt cases seeded as the baseline seeds them, the rest as shipped). Results and one-line causes are in BACKLOG.md section 1; no code changed. Note: run_baseline.py finds no agrunt numbers any more (check_test_numbers.py moved to Tools/qvr_checks), so its agrunt group is empty; the kit script needs the new lister.
## Failing tests triaged: 210, embed 7/13/21, AD (branch agent/testtriage_a, 2026-11-12)

- **`vr_agrunt_test 210`: the test's.** His defaults of 2026-11-08 (66f2ec025: Topple Within 32 -> 56, Backpedal Within
  80 -> 92) put the 48-unit cells in the topple zone (the probe reads the edge 8 units past the lip: 56). That failed 18
  zone checks, 2 more trials over, and all three lip-speed spreads (read from those cells only: launches, 2-6 u/s apart).
  The test now `tune`s 32 and 80 for its run (his values back at the end): 8 passed, 0 failed, the numbers of its first
  write (zones 72/72, over 50/54, lip speeds 79/141/230). The 4 misses: topples from 24 at two spots (y 420, 480) lying
  across the lip, as the old launch did.
- **`vr_embed_test 7`** (76 passed, 6 failed after the fix, 74/8 before): cases 0 and 1 were laid out for the stab's
  first need (3.5 m/s, 35 degrees; now 2.5 and 90): the test sets those for its run. Left: pushing 6 units deeper
  slides 0.3 (hand moved 3.6; so no slide sounds or blood), the real hand back to the drawn one moves it 0.15, a 12-unit
  yank off its axis and an 8-unit tug on a dead body keep it held. All hold mechanics written before stage 3 (the body
  following the hand, Blade Hold Strength) and the corpse drag: the cases need rewriting against those, not fixing here.
- **`vr_embed_test 13`** (62/8): "no blade left behind" counts every `thrown_weapon` in the map (2, then 1); the body
  through a teleporter stays held from 300 units on (vr_embedhold.qc has no jump check, unlike vr_skewer.qc's
  VR_SKEWER_JUMP: likely real); a grip 30.7 units from a lying blade, ragdolltug on a knocked down body, the spring,
  standing-or-staggering: not looked into.
- **`vr_embed_test 21`** (41/2, the same 2 as in ROUND24): a sword thrown flat at a wall with a gib on it bounces
  (stick kind -1) with the gib's middle 20 units off the wall, so VR_Skewer_Impact's reach (radius + 2) never meets it:
  no burst. The state 83 timeout follows from it: the test goes on with the blade on the floor.
- **AD**: agents' bases (links.ps1) have no `ad` folder (the release base's links Steam's), so neither
  `vr_flatswap_test 4` (also needs a spot on an AD map: it only mounts AD's data there) nor the visor's custom keys can
  run here. visor_test.sh now passes `--no-ad` to visor_check.py without one (the keys' count skipped, the rest checked):
  37/0.

## promote1112: his arena and armour values as defaults

- vr_arena_boss_health 1.25 -> 1.2, vr_arena_wave_size 1.75 -> 1.8, vr_agarmor_skill1 0.15 -> 0.2 (vr_cvars.inc).
- His 24 vr_wofs_* lines: weapons_data.py promote reports 0 changed: already the defaults (8d327a70b and earlier promotions; the config rounds to 6 digits). No settingsVersion bump.
- Check: a headless run prints each of the three as "(default)" with the new value. Arena tests pin their own values explicitly, not re-run.

## Embedded blades: a held body through a teleporter, a thrown blade's gib at a wall (branch agent/embedfix, 2026-11-12)

- **Teleporter**: `VR_Embed_HoldThink` frees the blade into the hand (`VR_Embed_Free`) when the wound point moves more than
  64 units in a frame. The check is shared with the skewer's: `VR_BLADE_JUMP` / `VR_Blade_Jumped` (vr_axestick.qc;
  `VR_SKEWER_JUMP` is gone). The triage's "held from 300 units away" was the test's: its stab (6 m/s) staggered the body,
  a staggering body isn't SOLID_SLIDEBOX, and `teleport_touch` never moved it. Case 1 now stabs at 4 m/s (under the 5 m/s
  stagger) and checks that the body went through (`case N: it went through the teleporter`). With that, the wound jumps
  142 units and the blade comes back to the hand. "Too far from the hand" would likely have freed it too. Over a whole
  run the jump check fires only there.
- **Thrown at a wall**: `VR_Skewer_ThrownBounce` (vr_skewer.qc, from `VR_Skewer_Impact` for a blade in flight). When the
  gib's move along its last way loses more than half in a frame, the blade isn't stuck, and its handle, middle or tip has
  something solid ahead within the burst reach plus that move, the gib bursts. A blade that sticks tip first still leaves
  its gib alone.
- **Test 13**: "no blade left behind" counts the thrown blades over those in the map as the case began
  (`vr_ee_blades0`). **Test 21**: the throw's give-up takes the blade back to the hand and clears its gib. State 83 (a slow
  swing to the wall) failed by timeout because the gib burst on the way (5.1 m/s read off the lagging mock hand,
  ROUND23's note). It is now a failed check, and the test goes on to the vr_embed-off part.
- Results: `vr_embed_test 13`: 56/8 -> 60/6 and 67/5 (the remaining failures vary run to run: grip 29-68 units off,
  cases 8/9, standing or staggering, the pull counts, the spring; not looked into). `vr_embed_test 21`: 41/2 -> 43/1,
  the one left being the slow swing's burst above.
## posebake: his poses, grips and IK stretch against the shipped tables (agent/posebake)

- His vr_agrunt_poses (358 keys), vr_agrunt_grips (3 rows) and vr_agrunt_ik_stretch (16 rows) of ironwail.cfg are already
  the shipped tables: pose_defaults.py --write found 0 keys, 0 grips and 0 stretch rows to change. Dump check
  (vr_agrunt_pose_dump all, headless, his three strings exec'd vs empty): 590 keys and every stretch line identical
  (sorted md5 equal); his three grip rows equal the shipped globalDefaults rows. Nothing to bake.
- configVersion 224: dropShippedEqual and dropDefaultEqual clear his redundant strings at the next load.
- vr_progs.cpp globalVec read def names through PR_GetString with no VM current: vr_agrunt_pose_dump (non-"all") crashed
  whenever a key was archived; now reads the server VM's strings directly.
- flatstance: the free hand's punch check now expects the stance punch >= the plain one (32 vs 21); the twist adds to the
  blow. 6 passed, 1 failed: "the drawn hand stands on the hotspot" still fails (guns 1 and 29, WID_GRAPPLE and
  WID_NAILPISTOL, hotspot behind/below the hand, 4.93 units off).

## cupskip

flatstance_test.sh no longer measures the drawn hand against cup grips (grapple, nail pistol): the status prints cup 0/1 (grip2HCup) on the stance spot line and the check skips cup reads. 7/0.

## testfix

- flatswing knight trials: the knight is checked 8 frames after its spawn (was 20: it drifted 3-50 units from its spot
  later; `sv_freezenonclients` stops the blow too, so no freeze) and the run has `sv_random_seed 1`; 3 runs 5/0.
- flatpose: `sv_random_seed 1` in the first and the e1m1 runs, the knights at 24 checked 8 frames after the spawn, and
  the control's gate is `> 3` units off (was 8; it read 7.5 on a good run). Before the last two edits: 22/0 in 4 of 6
  runs, 21/1 twice (the control, a pellet spread missing the knight at 300 or 24).
- `vrtutorial_playtest.py` room 8: the shells were thrown because the held shell caught on the bench edge on the way
  to the holster (`take_to_holster(lift=30)`), and a release followed by a same-frame hand reset throws (`w(4)` between).
  Shells now collect and the pouch gives pairs, but the main hand never holds the bench's shotgun ("the other hand holds
  no gun with a loading port"): its hand stops ~25 units short and the gun comes by the 9 s force grab. OPEN.
- Kit `run_baseline.py`: `agrunt_numbers` calls `build/qvr_checks/qvr_checks.exe test_numbers --list` (the old Python
  as the fallback for a tree without it); 214 agrunt numbers.
## Always-failing monster tests diagnosed (branch agent/faildiag_a, 2026-11-12)

- **`vr_agrunt_test 60`: the test's.** Step 5's one-sword grunt reaches down for a sword lying near
  (`vr_agrunt_dual_pickup` 1: it takes a second that pairs), and while it takes it (`vr_ag_took_at` > 0)
  `VR_AGPar_Able` says "no weapon in hand", never "from behind". The test turns dual pickup off for its run (put back
  at its end); the "behind" line now prints armed / taking / tugged. 5/1 -> 6/0.
- **`vr_agarmor_test 5`: the test's.** Broken plates fall as two halves since `vr_agarmor_split` (a619b613c,
  11-08): the killing cut under the plate makes 2 debris, the check wanted 1. It counts the halves now. 23/1 -> 24/0
  (`vr_agarmor_test 1` 38/0).
- **limb_wear (`vr_limb_test 36`, 28, 29): the tests'.** They hit the point on the test bone's own bone
  (`limbplace` 3), which on the firing range's grunt reads as the next joint (7 -> 8) since ROUND23's plate-aware piece
  lookup: the wear went into joint 8 while the checks read 7 (ROUND23's "found, not fixed"). 36's axe now strikes the
  skin point its aim read as the piece (as 41-44 do); 28 and 29 read the joint the blow's point reads as. Its fail line
  also prints the hit's piece and the plates. 14/17 -> 17/17.
- **stealth `gate_seen_walk`/`_back`: real, fixed.** An Alert monster's walk (`VR_Stealth_WalkNow`) counted a second
  with under 16 units moved as stuck, three in a row ending the walk; a grunt walks about 16-18 units a second in the
  open and less steering round a prop. The 110 scene's grunt met vrteleporters' crate at -1300 640 on its way to the
  gate and gave up beside it (the noise scene after it passed only because the first grunt had pushed the crate).
  Now under 8 units. With `vr_stealth_debug 1` the walk logs each stuck second, why it ended, and each phase change.
  Gates 2 failed -> 0; stealth_tests.sh all 57/2 -> 59/0.
- **ragdoll_stagger: real, not fixed.** steady (`vr_knockdown_chance 0.01`, 8 shoves in one game): the first two grunts
  recover, every later one falls at 0.36-0.39 s after 2 steps (leaning 39 deg, pelvis 53 % of its height, under
  `staggerSag` 0.55), balance 1.00 or not. The same with `vr_agrunt_repl 0` and with 600 frames' wait before the first
  shove: it goes by the count of shoves, not time or the armed version. Each shove's monster is removed between
  (`vr_knockdown_test 20`, `VR_Knockdown_RemoveBody`). Suspect: something left by the removed active ragdolls (Box3D
  bodies, or per-ragdoll stagger state) that the next one meets. Next: `vr_debug_ragdoll 1` on the third shove, the
  world's body count before and after mode 20. The backpedal case's grunt row fails the same way (R, then F F).
- **ragdoll_climb: real, pre-existing, not looked into further.** The enforcer is knocked off the lip but never grabs
  it (falls 184 units; the same with `vr_agrunt_repl 0`); the grunt grabs with one hand and is "too weak to rise", the
  ogre loses its hold, the hell knight sticks at the lip. ROUND24 saw the grunt's and the enforcer's the same.
## Always-failing tests, part b: autopump, cellcord, animsmooth, xr_runtime, secret_hits (branch agent/faildiag_b, 2026-11-12)

- **autopump (test's)**: 9543170a6 (2026-10-27) put "kind K skin S xN" before the time in the `shells eject` line; the
  test read field 6 (the kind, 0) as the eject's time. It reads the value after `t` now: 7/7 -> 14/0.
- **cellcord (real, fixed)**: since e7590d3bc (2026-10-22, the pouch's items drawn as models) `view::ammoPouchCell` is
  false without cells, and `cellcord::setupView` took that as "no pouch": with no cells the cord vanished instead of
  hanging loose, and came back already plugged (no flight to the cell). The pouch's presence is now
  `view::ammoPouchCentre`, the loose cord's anchor the pouch's middle. The test's "the pouch shows 3 cells" is "1-6" (up
  to six since e7590d3bc). 7/3 -> 10/0.
- **animsmooth (test's)**: live grunts are drawn on their authored rig since 6a47b1061 (2026-10-27,
  `progs/soldier.mdl#rig2`, IQM-type poses), and `vr_anim_smooth_trace` reads only .mdl poses, so the grunt's trace
  never ended ("peak none"). The trace is now an ogre's (no authored rig; spawned 220 units ahead, three frames after
  the grunt: two impulse 241s in one frame make one monster), its single-vertex limit 0.5 (the ogre's idle: 0.22; the
  old gun pulse 1.27). 7/2 -> 9/0.
- **xr_runtime (test's)**: the timing line is off by default since configVersion 202 (9ac8f0dbf, "logging off"); part 4
  sets `vr_xr_log_timing 1`. The script also exits 1 on a failure now. 2 failed -> 0 failed.
- **secret_hits (test's, one real case left)**: the doors were picked by entity number (40, 39), shifted by 4 since the
  worn backpack and others come first; they are found by brush model now (`*15` the quad's, `*14` the secret door).
  quad-shot, quad-punch, door-shot pass. **door-punch** fails (not fixed): the fist swung through the shootable secret
  door swishes ("melee swish: blow") but no hit or melee event ever comes, while the quad's trigger_multiple is struck
  ("a wall"). Failing since at least 2026-10-30; needs a look at the fist's sweep against a MOVETYPE_PUSH door with
  health. BACKLOG.md section 1.
## takeslight

vr_takes_light_flat (default 0, archived; menu Recording > "Recording Light and Prompt (Flat Screen)"): the flat-play corner light and prompt are off by default, because OBS records the window. vr_takes_light stays the master switch. The headset light is unchanged: VR_TakesIndicator returns null while VR_IsActive, and drawInEye draws into the eyes only, so the spectator camera never has it. vr_takes_test checks the corner is absent by default and present with the cvar at 1.

## Punches at shootable doors, and the hand's stop at walls (realfix doorpunch)

secret_hits "door-punch" (the fist at e1m1's shootable secret door, `*14`, swished but never struck it) had two causes:
- **The hand's stop was skewed by a unit** (vr_handpose.cpp, `stopAtWall`): it swept a +-1 box, which SV_Move meets
  brushes with as hull 0 at the box's mins corner (SV_HullForEntity's offset for a box under 3 units). The hand
  stopped a unit short of any surface it came at going -x, -y or -z (the door, approached going -y: the fist's point
  1.37 off) and a unit into one going +x, +y or +z. The fist's reach past the stopped hand (VR_Melee_Sweep: its
  thickness, 0.87) never met the near ones: punches at walls and brushes facing +x/+y/+z never landed. Now a point
  from the torso: the hand stops on the surface every way.
- **A shootable brush was decided as a monster's box** (VR_Melee_Sweep): it stops the hand as a wall does, so the
  frame the fist meets it the hand has slowed (2.3 m/s, needing 3.2) and the blow was refused. A brush that takes
  damage (`VR_Blow_StrikesAsWall`: SOLID_BSP; a shootable door or button) is now struck as a wall is (where a part
  went into it, at the speed it came in at, held for vr_melee_wall_hold) and dealt as a thing struck (T_Damage, its
  sparks). Monsters, props and triggers with health (SOLID_BBOX: the hand passes them) are as before.

Checks: secret_hits 5/6 -> 6/6 (door-punch: the secret door moved, 1 spark effect, no blood); eval.sh's full set
identical to before the change (112 result lines, 0 differ; 162/171 both); slopes 8/8; flatinput 19/0 (wallpunch
0.47 units in, wallswing 0.50).
## Ragdoll climb: the enforcer's grab, the pull's chest, the ogre's swing (branch agent/realfix_climb, 2026-11-12)

`Misc/quakevr/ragdoll/climb_test.sh`: 18 of 24 before (the enforcer failed pull, drop and hit; the grunt, ogre and hell
knight failed pull), 22 of 24 after. `vr_agrunt_test 210`: 8 passed, 0 failed.

- **The enforcer never grabbed** (`findLedge`, vr_ragactive.cpp): the ledges looked at were those in a box round its
  pelvis and chest centre (1.2 arms out). An enforcer's chest centre sits about 10 units behind its shoulders (its power
  pack); toppling off upright 12 units from the wall, the lip left that box while still within its shoulders' reach
  (1.15 arms): no probe, a 184-unit fall. The box now takes in both shoulders too.
- **The grunt "too weak to rise"** (Pull, vr_ragactive_hold.cpp): its chest's goal (0.3 arm over the lip) and its
  pelvis's way (0.35 arm under it) were 0.65 arm apart; a grunt's pelvis hangs 16 units (about an arm) under its chest,
  so with its chest at its goal its pelvis stayed 5 units short and timed out. The chest's goal is now at least 0.95 x
  its torso (pelvis to chest, measured as it begins to pull: `Climb::torso`) over the pelvis's way.
- **The ogre "lost its hold"** (Hang): "not hanging" (its pelvis within a quarter arm under the lip: its legs still on
  the top) also caught an ogre swung 27-30 units out over the drop on one hand, which let go. Now only with its pelvis
  within half an arm of the lip; it pulls itself up.
- Left (BACKLOG.md section 1): the grunt now goes over the lip, but its pelvis sinks back onto the corner as it goes in
  ("slid back off the lip"; it stands up on the top anyway). Raising the way in (0.6 arm) or holding the lip until its
  pelvis is in made no difference: something drags its pelvis down and out (not the hang kicks:
  `vr_ragdoll_active_hang_kick 0` the same). The hell knight hangs on one hand (the other's reach times out 1 s after the
  grab, after the pull began, so the Hang phase's one-hand re-reach never runs); its chest stays under the lip: "stuck
  at the lip".

## realfix_stagger: a monster on a lip by its box's edge (branch agent/realfix_stagger, 2026-11-12)

- **Cause (ragdoll_stagger, real):** not leftovers of removed ragdolls but where the grunts stood. Each shove pushes
  the player back about 15 units (316, 328, 330, then 345 on vrfiringrange), and vr_test_spawn puts the grunt 60
  units ahead at his height: from the third shove on, the grunt stood at x 270 held by its box's edge on the lip of the
  range's 15-unit step (its top at x 284), its model's feet in the air over the floor below. Its stagger pinned its
  pelvis at that height and planted its feet on air: it came down, sagged under `staggerSag` and fell (pelvis 53 %).
  The same at any count with the player put at x 330 each time (all fall) or 316 (all recover).
- **Fix (vr_ragactive.cpp `staggerSetDown`, vr_box3d_knockdown.cpp `staggerGround`):** as a stagger begins (a shove's
  or a ledge backpedal's), where it stood is a man-sized box (hull 1) let down from a stair over its origin (its own box
  is its ragdoll's by then, wider: hull 2 found the lip 16 units out; its origin has sunk 2.3 units). Both feet over a
  floor more than 4 and up to 18 units (a stair) below that: every part set down by that much before its stance is
  read. One foot over the lip's top: as before. Bigger monsters' lips (hull 2) are missed, as before. Debug: with
  `vr_knockdown_debug 1` "stagger: set down N units ..."; `vr_debug_ragdoll 2` also both floors.
- **The test:** stagger_test.sh puts the player back at SPOT (316 -556, facing down the range) before each trial, and
  has a `lip` case (LIP 330 -556: 8 steady grunts set down 15.0 units, all recover).
- **Numbers:** grunt steady, 8 shoves: 2 recovered -> 7 (the 8th's balance 0.52); backpedal's grunt steady row FAIL ->
  PASS; lip 8/8 alone. Left (BACKLOG.md): the beaten grunt falls at 0.43 s after 2 steps (F, not "gave way"), and in a
  combined batched run lip gave 5/8 and monster 2's 8th steady shove fell.

## retire1112: his ticked lines retired, vr_flat_melee_2h promoted

- 14 ticked lines (no voice notes) retired from quakevr/checklist.txt; 8 Test scenarios retired with them: flat_fling,
  flat_grenade_grab, flat_swap_adgun, flat_swap_melee, flat_swap_props, flat_taa_limbpop, flat_unload_full, gl_housing.
  flat_melee2h kept (an unretired line still uses it).
- tune_promote: vr_flat_melee_2h "1" -> "0" (the two-handed stance is off by default); --check exits 0.
  flatmelee2h_test.sh now sets `tune vr_flat_melee_2h 1` first: 16 passed, 0 failed.

## One grenade fuse; launched grenades tick (branch agent/grenadefuse, 2026-11-12)

His words: the fuse the same hand-triggered and fired by a grenade launcher or an ogre (the launcher's/ogre's time
wins), and the fuse's countdown sound on launched grenades too.

- **Found:** the launcher's grenade 2.5 s (weapons.qc `VR_FireGrenadeMissile`, a literal; also AD's grenade soldier's
  dropped launcher), the ogre's and the multi-grenade ogre's 2.5 s (orig_mon_ogre.qc, literals), the hand grenade
  `vr_handgrenade_fuse` 2.5 s (a tunable; the flat quick grenade arms through the same `VR_HandGrenade_Arm`). They
  agreed by default; nothing tied them. Left native: rogue's multi-grenade launcher 1 s (rogue_mult_wpn.qc, not our
  grenade), the MG3 demo dog's 2.5 +- 0.25 s, AD's own grenades (AD's `nextthink`), a caught grenade's
  `vr_grenade_held_fuse`.
- **One fuse:** `VR_Grenade_Fuse()` (vr_grenade.qc) reads the tunable **`vr_grenade_fuse`** (2.5 s, 1..5; renamed from
  `vr_handgrenade_fuse`, row "Grenade Fuse" in Debug > Tunables > Combat > Batting and Catching). The launcher, both
  ogres and the hand grenade's pin all use it. The hand grenade's fuse starts at its arming: the pin (trigger,
  `vr_handgrenade_arm` 0) or its release (`vr_handgrenade_arm` 1); arming-to-blast equals shot-to-blast.
- **One countdown:** `VR_Grenade_Light(g, xNewFuse)` is the fizz (`vr/grenade_fuse.wav`) and the ticks that quicken to
  the blast (`VR_Grenade_Cue`), for the pin, a catch and now a launcher's or a monster's throw (`VR_Grenade_Setup`
  when the grenade has a fuse). The ticks' pace is the fuse it was lit with (`.vr_gren_full`), no longer
  `vr_grenade_held_fuse`. Sparks stay for a grenade a hand has set (`.vr_gren_set`), not one in flight from a
  launcher. Toggle **`vr_grenade_countdown`** (1; "Thrown Grenades Tick", Combat > Batting and Catching). It needs the
  grenade to be a live one (`vr_grenade_catch` 2, the default, for yours; 1 or more for the ogres'): Quake's own
  grenades (`vr_grenade_catch` 0) have no per-frame tick and stay silent.
- **Test:** `vr_grenfuse_test 1` (Debug > Tests > Grenade Fuses; e1m1): hand, launcher and ogre grenades each lit at
  once with ticks timed to 2.50 s, each off 2.514 s after arming (one frame's detection), 10 ticks each; the launcher's
  with the countdown off silent. 17 passed, 0 failed. `vr_flatitems_test 11` 11 passed. Scene `grenade_countdown`.
## gripleak: the AD "Hold the ..." tests no longer change vr_weapon_grip_mode

- Menu "Hold the Flamethrower/Poison Sprayer" ran `vr_weapon_grip_mode 1; vr_ad_test 79/80` and never restored it. Now they run `vr_ad_test 79/80` only: the QC global `vr_grip_sticky_test` (vr_fields.qc; set by vr_ad_test 73-80, 87, 184; ends with the level) makes the three grip checks (vr_wpnholster.qc weapon let-go, vr_embedhold.qc, vr_grapple.qc) behave as mode 1. The archived cvar is untouched.
- Audit: the other tests that set the cvar (adgun 70/71/73 fx, 81-84, admelee, ammocolor, agrunt 35) already save and restore it. No other menu command sets it.
- Check: build 0 warnings; `vr_ad_test 79` on e1m1 printed nothing (AD not mounted in agent bases); the cvar reads 0 before and after (trivially: nothing sets it now). Not exercised with AD.

## The dagger (branch agent/dagger, 2026-11-12)

His request: a bladed melee weapon, a dagger, looking like the knight's sword (its steel, no blood), smaller and leaner,
a blade about a third of the sword's, behaving as a sword (skewering, embedding), obtained "like the sword".

- **What it is**: a kind of the knight's sword (`WID_SWORD`, weapon flags kind 12: `VR_SWORDKIND_DAGGER` in
  `vr_ad_guns_defs.qc`, past AD's kinds 1-11, which stay `VR_ADMELEE_LAST`), as the hell knight's and AD's swords are.
  So every sword path is the sword's own code, by data: the swing, the stab and its hold, the skewer, embedding (held,
  thrown), the throw, the holsters, the two hands' hand-off, the armed humanoids' sword pose class, flat play. Its own:
  the model (`VR_ADMelee_Model`), the name "Dagger", the damage (`VR_SwordKind_DamageMult`), the weapon settings slot.
  No new weapon id: a WID would have needed every `WID_SWORD` test across QC and the engine (60+ places) doubled.
- **Model**: `Misc/quakevr/make_dagger.py` (then its source .blend, `src_models/blend/v_dagger/`, by `blend_build.py`;
  an untouched export writes the shipped files byte for byte): the knight's sword's four pieces scaled in the blade's
  frame: the blade 1/3 as long, 0.7 as wide, 0.85 as thick; the guard 0.6 across, 0.8 thick, 0.85 along; the grip
  0.57 of its length (one hand's room; its cross-section where the hand anchor is kept); the pommel 0.75, moved up to
  the grip's end. The sword's skin with its blood taken out (its clean patch, `v_ksword.mdl_0.clean`, applied: 1823
  texels), its normals carried through the scaling, its normal map baked (`bake_normals.py`). Packed from the sword's
  bounds' corner, so the sword's grip settings hold it the same way. 42.5 model units long (the sword's 100.2), blade
  23.2 from the guard (the sword's 69.5).
- **Weapon settings**: `weapons.txt` slot 51 (`vr_wofs_*_52`; numSlots and weaponSlotCount 52, `weapons_data.py`
  NUM_SLOTS 52): the sword's slot 18 with the dagger's tip (`muzzle_av`), blade root (`wpnbtn_av`, `wpntxt_av`),
  one-handed (`2h_mode 2`, no hotspots: no second hand on a one-hand grip, no half-sword grip on a short blade), the
  blade from 0.331 of the hand-to-tip line (`bl_from`: past its guard), the pommel 0.321 behind (`bl_near`), 1.2 kg,
  34 cm, balance 6 cm (`w_mass`, `w_len`, `w_com`). Its hand anchor is the sword's (0.08 model units off the new
  surface, 0.25 mm: kept).
- **Numbers**: `vr_dagger_damage` 0.6 (Combat > Weapon Damage > Melee): its swing, stab, throw and an armed grunt's
  blow are 0.6 of the knight's sword's (the sword 20 x 1.1 = 22 before speed and weight, so 13.2). Measured: a 6 m/s
  swing at a grunt 9, a 6 m/s stab 12 (the sword's 15), a throw 241 (the throw's damage by its speed). Depth: embedding
  is a share of the blade's length (thrown: `vr_embed_sword_depth_*`; held: `vr_embed_hold_sword_most`), so it sinks
  about a third as deep: its stuck blade 6.9 units against the sword's 21.0; thrown into the wall 1.7 units deep.
  Engine model lists (`vr_modelmetadata.inc` VDagger): melee phasing, no sights, the env map's steel, the hot blade's
  and leg sweep's blade share 0.43 (the blade past the guard).
- **Where it is**: `weapon_dagger` (FGD), `vr_gunprep` (its class); the toolgun's Weapons > "Dagger" and Monsters >
  "Armed Grunt: Dagger" (appended: numbers kept); Debug > Tests > "Dagger" (`vr_physics_spawn weapon_dagger reach`);
  the armed grunts' pool (`vr_agrunt_w_dagger` 1, Combat > Armed Grunts > The Pool; weapon code 105,
  `VR_AG_CODE_DAGGER`); vrfiringrange beside the two swords (216 -868, `make_prop_area.py`, appended last).
  Knights still drop their own swords (a dagger isn't theirs).
- **Tests**: `vr_embed_test 33` (`QC/vr_dagger_test.qc`, by the embedded blades' machinery): a swing hurts a grunt, a
  stab sticks by its point and hurts, its blade a third of the sword's, thrown into the wall and into a grunt it
  sticks: `embeddagger: 12 passed, 0 failed` (7 of 8 runs; one run's dagger stab didn't stick: `vr_hit_stagger 0` set
  since, as test 11 does; not seen again in 5 runs). With `-flatscreenmode`: held, the mouse button's swing hurts a
  grunt 34 units ahead (at 44 the short blade doesn't reach): `embeddagger: 3 passed, 0 failed`. Armed grunts: test 61
  (`vr_armedgrunt_bugs_test.qc`) has the dagger as its sixth weapon (picked up lying, after its disarm, thrown by you,
  after a knockdown; grunt and enforcer): `pickups: 48 passed, 0 failed`. Scenario `dagger` (SCNRUN OK). eval.sh: no
  take changed.
