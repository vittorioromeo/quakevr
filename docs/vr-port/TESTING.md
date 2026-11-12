# Testing without a headset

The reference for the testing tools: the mock headset, scripted motions, the test scripts and checks, and the
fixtures. Playtesting in the headset (build, controls, voice notes, what to try) is in [PLAYTEST.md](PLAYTEST.md).
"The kit" below is the author's agent toolkit outside the repository (`C:/OHWorkspace/qvr-kit`: `build.sh`,
`run.sh`, `eval.sh`, `bench.sh`), which wraps these same commands for headless runs. The release's test suite,
`Misc/release/run_test_suite.py`, runs the scripts that give a verdict ([RELEASING.md](RELEASING.md), "Tests
before publishing").

The recipes for retesting single features (climbing, swimming, stamina, reloading, the AD guns, ...) are in [archive/TESTING_RECIPES.md](archive/TESTING_RECIPES.md): grep-only reference, one `##` section a feature, listed in the index at the end of this file.

## The mock headset

`vr_backend mock; vr_enabled 1` runs everything with a pretend headset. `vr_mock_button <main|off> <trigger|grip|primary|secondary|stickclick|menu> <0|1>`,
`vr_mock_stick <main|off> <x> <y>` `vr_mock_hand <main|off|head> <x> <y> <z> [<pitch> <yaw> <roll>]` and `vr_mock_look <pitch> <yaw>` drive it (`vr_mock_hand_to <main|off> <x> <y> <z>` puts a hand at a world point, `vr_mock_hand_to main weapon <fraction> [<cm>]` over the nearest weapon lying about, that far along it: ROUND21.md, "Crowbar follow-ups"; `vr_mock_hand_to main spot <index> [<cm>]` over its hotspot `index`, `vr_mock_hand_to off carried` at the handle of the weapon the other hand carries: ROUND21.md, "Weapons taken by their hotspots"; `vr_mock_hand_to off held <fraction> [<cm>]` in the weapon the other hand holds or carries, that far from its handle to its tip: ROUND21.md, "Weapons held anywhere"; `vr_mock_hand_to off heldspot <i> [palm]` on the other hand's weapon's hotspot `i`, 0-based, `palm` its palm there as a cup is taken: `Misc/quakevr/twohand/cup_aim_test.sh <agent>`, ROUND22.md, "Widowmaker cup grip aims two-handed") (grip state across weapon changes: `Misc/quakevr/twohand/grip_state_test.py <agent>`, ROUND21.md, "Grips reset with the weapon"); `vr_mock_swing <period>`
swings the main hand for throwing tests. `vr_mock_shake <degrees>` shakes the head (quick small turns at 5-13 Hz and
4 mm a degree of position wobble) and `vr_mock_shake_turn <degrees/s>` turns it slowly under that, timed from when the
shake starts (the same poses every run with `vr_fixed_frames 1`); `vr_window_log 1` (retired cvar: vr_window_log) prints the head's and the window
camera's angles each frame (Smoothed Mirror, Spectator Camera: ROUND21.md, "Recording"). The window's `screenshot` is
the window's view; `vr_eyeshot 1` the eyes'. `imagehash [filter]` checksums every loaded texture as the GPU holds it (to `imagehash.txt`, the sum in the console): two runs loaded the same pixels when their sorted files match (eyeshots of identical content still differ by 1 in a few pixels between runs). `vr_particle_seed <n>` (not 0) makes the particles the same in every run (their random numbers restart from it at each map), for comparing images. `sv_random_seed <n>` (not 0; before the `map`) does the same for the server's random numbers (QuakeC's `random()`, the monsters' turns; their own stream since 2026-10-09, never moved by the client's effects): with `vr_fixed_frames 1`, an AI scene plays the same every run (`vr_bench_statehash` equal at the same frame); 0, the default, seeds from the clock at each map load (`sv_random_info` prints the seed). `vr_test_crand <n>` (retired cvar: vr_test_crand) draws n numbers from the C library's `rand()`, as the client's effects do (they must not change the game). `vr_mock_fingers <main|off> <trigger> <grip> [<thumb>]` sets the finger
sensors (0..1); a fifth argument sets the index finger's touch on the trigger.
Fast test runs (ROUND21.md, "Faster tests and startup"): `vr_mock_fast 1` runs the mock headset's frames as fast as
the machine makes them while the game's clock is fixed (`vr_fixed_frames 1`; `vr_motion_play` and `vr_motion_eval`
not in watch mode): no frame cap, no vsync, every frame 1/72 s of game time as before, so the same frames, poses and
numbers in less time (`wait600`: 6.7 s at `host_maxfps 90`, 1.7 s fast). Frames on the real clock (no
`vr_fixed_frames`) keep their pace, and so does a take played in watch mode. The kit's `run.sh` sets it for every run
(`-RealTime` leaves it off: a test that needs the wall clock with fixed frames, or measuring the frame cap). A `wait`
waits for a server frame: on the real clock that is 1/72 s whatever the frame rate, so `wait600` is 8.3 s there;
with `vr_fixed_frames 1` every frame is one at 72 Hz (`vr_fixed_frames_rate`; at 90, four frames in five; below 72 a frame
that runs two ticks counts once). Server ticks (ROUND21.md, "Server tick rate"): `vr_fixed_frames_rate <hz>` with
`vr_fixed_frames_jitter 0.02` (each frame's time off by up to 2%, as a headset's; the wall clock's jitter, which
`vr_display_clock` takes away: `vr_display_clock 0` to test it), then `host_tickstats reset`, `wait...`,
`host_tickstats`: ticks a second, their lengths, each frame's tick count, the server's clock, Box3D's steps;
`host_fixedtick 0` for the old cadence. `vr_motion_play`/`vr_motion_eval ... rate <hz> [jitter <fraction>]` print them
over the take. Sound (off in the kit) plays at the wall clock's pace, ahead of the game.
`vr_walltime [label]` prints the wall clock and the process's CPU time since the last call; `vr_startup_times` the
start-up's stages and the last map load's (with the work summed across them: model loads, image decoding, normal
maps, uploads) and the last session's quit (its stages, printed at the quit too and kept in `cache/quit_times.txt`);
`vr_normalmap_cache 2` makes the cached skin normal maps anyway and warns of any that differ.
`quakevr/cache/` (the skin normal maps, `prefetch.txt`) is made again when removed: the first start after that is
about 0.4 s slower. A timing that compares builds must run each one twice (the second run's caches are warm).
Unpaced frames also skip most presents (`VR_SkipSwap`: ten a second): a present waits for the display's refresh even
with vsync off, most of such a frame (e1m1: 2.8 -> 1.4 ms). `vr_mock_fast 2` doesn't draw them at all (`VR_SkipScreen`):
each sets up only what the game reads from the drawing (`VR_HeadlessView`: each eye's refdef and the view entities,
the weapons' and hands' poses the melee samples), with no GL, so parallel runs don't wait on the shared GPU; the models'
occlusion isn't baked either (seconds of 5 threads at each start). The kit's `eval.sh` uses it (the melee verdicts and
every column of its table are the same as drawn replays'); `screenshot` in such frames shows an old image. Hidden test
runs: with `QVR_TEST_HIDDEN` set the game's window is never shown (the kit sets it unless `-Visible`); screenshots are
the same as a shown window's.

Sound is off in test runs (run.sh passes `-nosound` unless `-Sound`): `S_Update` then returns at once, and everything
behind it (spatial audio's per-frame listener update and mix, `VR_SndListener` and `VR_SndPaint`, which place sounds in
the world) never runs. A bug there does not show headless without `-Sound`: the "Loading..." crash of 1.0.0 was one

`quit` in a test run (`QVR_TEST_BACKGROUND`, which the kit sets) quits at once, whatever has the keys: without a map
the console is up, so a script's `toggleconsole` closed it and `quit` opened the quit confirmation and waited there
until the kit's timeout (so did `quit` after `menu_vr ...`). A player's `quit` outside the console still asks. A test run
uses no network: no add-on list download, the map index read from its cache only (`maps_fetch` fetches).

`vr_mock_camera <x> <y> <z> <pitch> <yaw>` draws the mock eyes from elsewhere in the tracking space (a spectator's view of
your body; the hands stay with the head), `vr_mock_camera` alone puts them back.

## Writing a test: one verdict grammar, one library

Every test, in any language, prints one line per check, `<name>: PASS <what>` or `<name>: FAIL <what>`, and ends on
its verdict line `<name>: N passed, M failed` (the run ends there: `-QuitOn "<name>: [0-9]+ passed"`). The writers:
QC `VR_Test_Begin/BeginFlags/Check/Verdict` (QC/vr_testutil.qc), C++ `testkit::Tally` (vr_testkit.hpp), Python
`qvrtest.Tally` (Misc/quakevr/qvrtest.py), shell `check` / `verdict` (`source Misc/quakevr/qvrtest.sh`).
`qvrtest.py` is the scripts' one library: `KIT`, `tree(agent)`, `run`, `run_sh`, `lane()` / `lane_env` / `game_dir`,
the mock-file builders (`hand`, `button`, `grip`, `waits`, `motion`) and the parser the runners use
(`FAIL_LINE`, `counts`, `verdict_near`). Known-flaky tests: one list, `Misc/quakevr/test_flaky.txt` (run_test_suite.py
`--allow-flaky` and the refactor baseline read it). qvr_checks canonical's `test-*` families fail the build on a new
hand-made tally, verdict line, `def verdict(`, spelt-out kit path or shell `check()` copy (today's sites are in its
allowlist; migrate them when you touch a test, then `--tighten`).

## Test numbers

The numbered test cvars (`vr_agrunt_test N`, `vr_ad_test N`, `vr_rv_test`, `vr_np_test`, `vr_flare_test`,
`vr_reload_test`, `vr_pickup_test`) have one registry: `QC/test_numbers.txt` (its header has the format). To add a
test, **claim its number there** in the same commit as its dispatch branch: a number no line has (a block such as
`vr_agrunt_test 50-59` belongs to its file). `build/qvr_checks/qvr_checks.exe test_numbers` (run by the kit's build.sh,
which fails on it) checks: no number claimed by two lines or taken by two dispatch branches (the second would be dead
code: 74 and 84 clashed this way), every dispatched number and every Debug menu command's number registered, every
registered number dispatched (`N*`: run by the dispatcher's fall-through). `--list` prints the dispatched numbers. A new
dispatcher (another cvar, or a sub-dispatcher a range is handed to) gets a `dispatch` line. Since 2026-10-29 every QC
`vr_*_test` cvar that takes a test number has one (26 more: `vr_arena_test`, `vr_limb_test`, `vr_agarmor_test`,
`vr_decap_test`, `vr_mg3_test`, ...), as `loose` dispatchers (their set-up branches share numbers with the cases);
`vr_shock_hit_test` (a damage amount), `vr_crouch_test` and `vr_dmgcredit_test` (no numbered branches) are not
numbered tests. Every `vr_agrunt_test` ends
on `agtest: test N over: P passed, F failed` (`-QuitOn "test N over"`).

## Checklist scenarios: how to write one

A checklist line gets a one-press test scene by ending with ` {test:<id>}` (the tag is not shown, not part of the
item's key: adding one keeps its tick). `<id>` names `quakevr/scenarios/<id>.txt`; several lines may share one. Test
loads the map fresh, places everything, closes the menu and floats the hint (the line's text, or the file's `hint`)
ahead for `vr_scenario_hint_time` seconds; Reset Test (the page, or `vr_scenario reset`, bindable) runs it again, End
Test (`vr_scenario stop`) puts its settings back. The file, one directive a line (`#` comments, "quoted" names):

```
title Armed grunts with each melee weapon          # what it is (vr_scenario list)
map vrtesthall                                      # required; loaded fresh every run
start 0 -400 25 -90                                 # optional: your origin and yaw (default: the map's start)
god                                                 # and/or notarget; health 50
give "Crowbar" main                                 # a toolgun Weapons entry into a hand (main|off)
spawn "Armed Grunt: Axe" 240 -80                     # toolgun entry (label or classname): fwd, left, [up], [yaw]
spawnarmed monster_vr_grunt 2 2 200 40             # an armed humanoid by weapon codes (right, left; vr_toolgun_armed_list; 0 pool, -1 fists, left 0 none), then fwd [left] [up] [yaw] as spawn: duals and any weapon the toolgun has no entry for
cvar vr_debug_melee_blades 1                        # set for the test, put back after (never saved)
cmd impulse 9                                       # console commands, after everything is placed
hint Swing at them: ...                             # optional; default the checklist line's text
```

- Spawn names are the toolgun panel's labels exactly (`vr_toolgun_spawnlist`, Quake/vr/vr_toolgun.cpp `spawnables`,
  vr_toolgun_packs.inc, vr_ad_spawns.inc): "Grunt", "Wooden Crate", "Explosive Box", "Armed Grunt: Shotgun",
  "Armed Enforcer: Axe", "AD Death Sergeant", "AD Death Sergeant's Flail"... Offsets are units from your feet in the
  direction you face (`left` positive to your left); each lands on the floor under its point (`up` above it, so a crate
  can stand on another: `spawn "Wooden Crate" 90 30 34`); monsters face you unless a yaw (relative) is given.
- Spots: `map <name>` then `vr_scenario where` prints a `start` line for where you stand, the free distance ahead, left,
  behind and right, and the nearest liquid with its surface and extent. vrtesthall: start 0 -288 25 facing 90, 640
  free ahead, 616 left; its pool x 224..528, y -416..-112 (surface -9, 119 deep: monsters placed in it sink; no
  shallow water), `start 150 -264 40 0` at its west edge; `start -400 -400 25 -90` faces a plain stretch of the south
  wall (its middle has the New Game buttons: keep melee away from them). `developer 1` prints each spawn's edict and
  place. An Arcane Dimensions map (`map ad_crucial`: `ad_breakables`, `ad_lighting`) is found only while AD is the
  running campaign (Play > Official Campaigns; headless: `-Games "-game hipnotic -game rogue -game ad -game quakevr"`
  and the scene first, as loading a Quake map switches the campaign back); elsewhere its Test says "start Arcane
  Dimensions first".
- Flat-screen lines: the game's mode is picked at start-up (`-flatscreenmode`), not by the scene, so a flat line's
  scene is an ordinary one; check it headless with `-ExtraArgs "-flatscreenmode"` (`vr_scenario runall quit <id>...`).
- Offline (no game): `python Misc/quakevr/checklist_tests_check.py` -- every tag's file exists and parses (known
  directives, one map that exists, `give` main|off, `spawn` offsets), the untagged lines and the unused scenes listed.
- Check before committing: `vr_scenario validate` (SCNOK/SCNERR per file: the map, entries, settings, commands; and
  every checklist tag's file; SCNSUM) and `vr_scenario runall quit [<id>...]` (each run in turn for 3 s of game time,
  SCNRUN OK/FAIL with the first problem: an entry not available here, a spot in a wall, a QC error, a given weapon no
  longer in its hand; SCNHANDS per scene: each hand's given weapon then and now; SCNRUNSUM). A one-off eyeshot:
  `vr_scenario <id>;wait120;screenshot`.
- A `give` puts the weapon in the hand as if just grabbed, in either grip mode: with `vr_weapon_grip_mode 0` (hold) it
  stays, the grip unpressed, until that hand's grip is pressed (from then on the grip holds it: let go on release);
  sticky mode lets go after a squeeze and a release as usual (QC `VRWeaponGivenHeld`). The mock's hands never grip:
  runall checks the weapons stay in both modes.
- Armour: `cvar vr_toolgun_armed_armor <1-4>` (green, yellow, red, random; `vr_toolgun_armed_pieces` 0 cuirass and
  helmet, 1 all ten, 2 random) puts the toolgun's Armed Humanoid armour on every armed humanoid the scene places
  (`spawn "Armed Grunt: ..."` and `spawnarmed` alike); unset, each wears its own roll as on a map.
  Armed knights need `cvar vr_agknight 1` (`monster_vr_knight`), armed death knights `cvar vr_agdknight 1`
  (`monster_vr_hknight`; weapon 0 is his own sword). A Debug > Tests setup works as a `cmd` (`cmd vr_agarmor_test 6`).
- No scene for menu-only items (graphics presets, menu layout), calibration or seated play: leave those untagged; nor
  for the flat-screen Rig Studio, startup and config checks, load times, or what needs late arena waves or a long play.

**Every new checklist line gets a test when one makes sense.** A line is `<what to do>: <what you should see>
{test:<id>}`; when shipping one, reuse a scene whose setup fits (`vr_scenario list`; the same id on many lines is
normal: the line's own text is the hint) or add `quakevr/scenarios/<id>.txt`, then `vr_scenario validate` (`0 missing`)
and `vr_scenario runall quit <id>` (`SCNRUN|<id>|OK`). Scenes on Arcane Dimensions' maps or entries (`AD ...`) need
AD running, so the headless check skips them (`-Games "-game hipnotic -game rogue -game ad -game quakevr"` when its
data is in the base).

## Debug build assertions (2026-10-07)

The Debug build (`MSBuild Windows/VisualStudio/ironwail.sln
-p:Configuration=Debug -p:Platform=x64`; Zancle's asserts with it, QVR_ZANCLE_DEBUG) checks what Release takes on
trust. Run it with `SDL_ASSERT=abort` in the environment (the kit's run.ps1 sets it), never without: a failed
`SDL_assert` then exits with code 42 and no message instead of opening a modal dialog. The regression run: the kit's
`bench_maps.ps1` for the custom maps, then the Debug exe on `map warden; wait300; quit`, `map ad_grendel; wait300;
quit` and `map start; wait200; map e1m1; wait30; impulse 9; vr_flashlight 1; wait200; quit`: `exit=0` each. Exit 42
means an assertion: put a `Sys_Error` before the suspect `SDL_assert` (its message lands in `qvr_error.txt`) to see
which. warden's lit water used to fail `num_instances > 0` in R_AddBModelCall (ROUND21.md, "Zero-instance water
calls"). The kit runs it with `bash <kit>/build.sh <name> --debug` (QC and the Debug engine; MSBuild's log in the
worktree's `scratch/build_debug.log`), then `bash <kit>/run.sh <name> -Debug -Script "..."`: the same run as Release's
(SDL_ASSERT=abort set, as for every run), and an assertion prints as `exit=42 (an SDL assertion failed ...)`. Its
console shows OpenGL's debug messages (`GL api error ...`), so filter on `ENGINE|exit=|TIMEOUT` rather than `rror`.
A clean run has no `GL api error` or `GL api undefined` line (ROUND21.md, "GL errors in Debug builds"): filter on
`GL api (error|undefined)|GL error caller|exit=` to check. Each error or undefined-behaviour message is followed, once
per distinct stack, by `GL error caller: fn (file.c:12) < caller (file.c:34) < ...` (the debug output is synchronous,
so the GL call that failed is on that stack; frames in the driver show as addresses).

## Batched test cases: many cases a game, games in parallel (2026-10-14)

A headless case used to be a game of its own (kit `run.sh`: start-up ~0.8 s, the first map ~2 s, the case's frames at
the wall clock's pace, quit), one after another. `Misc/quakevr/testbatch/` plays a suite's cases as batches instead, as
the kit's `eval.sh` does the melee takes (ROUND22.md, "Faster test suites": the numbers):

- **Many cases in one game.** Each case runs from its own cfg between `echo @@TB_BEGIN <n>` / `vr_test_reset` and
  `disconnect` / `echo @@TB_END <n>`. `vr_test_snapshot` (at the batch's start) and `vr_test_reset` (engine,
  `vr_testbatch.cpp`): the cvars a case set go back to the batch's start (a cvar made since: its default), every engine
  `+command` is let go (`-grabmain`, `-forward`), the mock headset's poses, buttons, sticks, glides, plays, aims, walks,
  turns and camera go back to start-up (`resetMockState`), the client's `rand()` is seeded as at start-up. The case's
  own `map` does the rest (the server, the entities, `sv_random_seed`, the items). Not reset: aliases, the game folder
  (`vr_campaign_native`: every case of a batch should switch the same way), what the menus or saves leave behind.
- **Games in parallel.** The cases are split between up to `TB_JOBS` (8) copies of the game (kit `run.ps1 -Instance
  <k>`, their own game folders; the agent's other game folders, `ad`, linked in), as many as the kit's free run slots
  allow (`slots.sh acquire_many`, with the agent's run lock held: its runs still go one at a time).
- **Unpaced frames.** The batches run `vr_fixed_frames 1; vr_fixed_frames_rate 250`: the real-time test run's frames
  (the baseline's `host_maxfps` 250 over the server's 72 Hz ticks, `host_fixedtick`), as fast as they come. At rate 72
  (a tick a frame) the client's per-frame work (the hands' hits) sees 3.5 times fewer frames, and reload_test.sh lost 5
  of its 110 checks: keep 250 unless a suite is proven at another (`TB_RATE`). `TB_NODRAW=1` (`vr_mock_fast 2`) skips
  drawing the frames (5-10 times faster again): a suite opts in (`export TB_NODRAW=${TB_NODRAW-1}`) once its verdicts
  are proven the same, since a check that reads the drawing (a "first drawn" line, the pixels) needs the frames drawn.
- **Each case's output as run.sh would have printed it alone**: `exit=0` (or the copy's `TIMEOUT` / `ENGINE ERROR` /
  `ENGINE CRASH` lines for the case that ended it), its console lines through its `-Filter` (Select-String's: a regex,
  case-insensitive), the same blank lines and CRLFs. A case past its `-Timeout` is stopped (`TIMEOUT after <n> s`, the
  game killed) and the cases after it in that copy run again in a new one, as do a crashed copy's.
- **Not batched** (run alone by `run.sh`, as before): `-Out`, `-Clean`, `-RealTime`, `-Visible`, `-Full`,
  `--exclusive` (timings), and a script that takes a `screenshot`.

A bash suite keeps its shape: its body in a function, its `bash $KIT/run.sh $AGENT ...` calls as `tb_run_sh ...`:

    source "$(dirname "$0")/../testbatch/testbatch.sh"   # (from Misc/quakevr/<dir>/)
    tb_body() {
        log=$(tb_run_sh -Script "map e1m1;wait60;...;toggleconsole;quit" -Filter "^reload:" 2>&1)
        check ...                                          # as before
    }
    tb_main "$AGENT" tb_body "$@"

`tb_main` runs the body twice: first collecting its runs (`tb_run_sh` prints nothing; that pass's output is thrown
away), then the batches, then the body for real, each `tb_run_sh` printing that run's output. A run whose arguments
differ the second time (a script built from an earlier run's output) runs then, alone (a batch of one). A Python
suite: `testbatch.run_sh(AGENT, [...run.sh arguments...])` in place of its subprocess call, and
`sys.exit(testbatch.two_pass(AGENT, "<name>", main))` (main resets its own fail count: it runs twice); or
`testbatch.Batch` (`case()`, `run()`, `output()`) for a suite that lists its cases first.

- `TB_SLOW=1`: every run by `run.sh`, one after another, at the wall clock's pace: the suite as it was (its reference
  results). A converted suite is proven by both (same PASS/FAIL, numbers within the run-to-run spread).
- `TB_FIXED=0` (real-time frames in the batches), `TB_ISOLATE=1` (a game for each case, still unpaced and in parallel:
  a suite whose cases can't share a game), `TB_JOBS=N`, `TB_RATE`, `TB_NODRAW`, `TB_STATS=1` (games, cases, seconds).
- The cases, outputs and `stats.json`: `<tree>/scratch/testbatch/<suite>/`.

Waits that end on a condition (engine, `vr_testbatch.cpp`), in place of a fixed `waitN` long enough for the slowest
run; each counts frames as `wait` does (a server tick each) and goes on at once when its condition holds:

- `vr_waituntil <cvar> <op> <value> <frames>`: until the cvar compares so (`== != < <= > >=`; numbers as numbers,
  else text, `==`/`!=` only). QC can raise a flag for it with `cvar_set`.
- `vr_waitprint <frames> <text...>`: until the console prints a line holding the text (after the command).
- Running out prints `vr_waituntil: timed out ...` / `vr_waitprint: timed out ...` and the script goes on.

A fixed wait that a check measures across (frames, seconds, distances after it) is the check's own: replace only waits
that end on an event (the ragdoll settled, the line printed), and prove the suite again.

## Index of the feature recipes (archive/TESTING_RECIPES.md)

Each line is a `##` section of [archive/TESTING_RECIPES.md](archive/TESTING_RECIPES.md); grep it for a cvar, command or script name.

- Stuck, unstick and stuck monsters: vr_stuck_info, vr_stuck_test, stuck monsters and stairs
- Climbing: plays, props, jump to a ledge, grab leniency, orientation, sliding, ledge map, lifts, sync
- Swimming: strokes against the palm, air supply, sounds, out of the water by hand
- Swimming tilt: the body's pitch (2026-11-06): the body's pitch in the water
- Stamina and tired arms: shaking arms, heavy hands, stamina for shoves and strikes
- Pain: hits knocking the hands: vr_debug_pain knocks
- Throwing: the release angle, a thrown axe's spin, sticking and throw grace: throw plays, spin, axe sticking, grace
- Fitted hands, the hand rig and hand calibration: grasp bench, controller preview, rig, Blender, calibration
- Anchors: points on the models' surfaces
- Map content seeds: vr_fresh_map_seeds
- Multiview twins: vr_multiview_check
- Smoother animations: r_lerpmodels 3 checks
- Allocations a frame, where: vr_alloc_sites
- The game's thread pool: vr_jobs_*
- Two-handed props: carrying a box with both hands, across a save
- Precise hit detection and held weapons against models: hit tests on models, vr_hitmodel_bench, model collide
- Shots pushing props, the player's hitbox and flung props: shotpush, vr_hull_hittest, flung props
- Blood on you and your gear: vr_wounds_*, a real chainsaw cut
- The ogres' chainsaw: impulses, fit, cord pull, start failures
- Mjolnir in water and prop size: impulse 153 / 215, vr_prop_size
- Dawn of the Machine: weapons, tests 1-14, the campaign route
- The crowbar: impulse 167, hotspots, parry pose
- Enemy guns: grunts' shotguns, enforcers' rifles, burst rifles
- Armed humanoids: drawn motion and posed shove: vr_rig2_motion_test, test 84
- Enemy shoves: grunt, enforcer, vore shoves; bloodless shoves; config 129
- Flatscreen play: hit feedback, two-hand, reload, move, slide, seated, crouch tests
- Grappling hook: mock hook, unreel, persistence, physical rope
- Leaning: vr_debug_lean, lean through stick turns
- Stray button presses at a load: wall buttons at a map load
- Body calibration: vr_bodycal, refit, body tracking
- VR Calibration: the hub's menu, wall buttons, menu path check
- Elbow tucked, arms options and holster limits: armsweep, a holster behind you
- Body collisions: vr_debug_body_collide, bench
- Arm IK: vr_debug_arm, elbow near the face
- Corpse collision and ragdolls: corpses, ragdolls, active ragdolls, hit reactions
- Limb gore: vr_limb_test
- Headshots, training dummies and dummy attacks: debug shots, vr_dummy_*
- Weapon posing mode: vr_pose, posing in a holster
- Rigid bodies, Box3D and the pool: vr_physics_*, multi-thread bench
- Physics props: pickups, phasing and standing on props: physics pickups, vr_physics_inside, vr_physics_player
- Debug views for the physics and the hands: wireframes, grab probes
- Grab reach from the fist: vr_debug_carry 2
- Holsters: tuning the body, guns in holsters, per-weapon holstered pose
- Weapon Offsets' pages: menu_vr 84..91
- Draw and holster blend: vr_debug_draw_blend, the config across a killed session
- Wounds, burns and wetness: vr_wounds_test, own arms close up
- Enemies hurt by liquids: slime, lava, vr_enemy_liquid_damage
- Flashlight: tuning zones, lit but not taken, flash_grab_test
- Wrist gadget: vr_gadget_info, fps
- Melee fixes: motion_synth takes, flashlight, gibs, axe on walls
- Deflection, grenades and ogre aim: impulse 246 / 247, catching, bashes
- Weight, heavy weapons and sticky grips: spring model, wrenched out, heavy handling, explosive boxes
- Wall torches: taking torches, lit again from flames
- Burning: vr_burn_test, crates and nails, pieces, heat haze
- Incandescent blades: hotblade_test.sh, vr_hotblade_test
- Hands and weapons as bodies, pushes by mass: vr_debug_box3d, by-mass pushes
- Held weapons and props against the level: vr_debug_gun_wall, held props meeting
- Rocks and bricks, the firing range's prop area and .ent overrides: vr_debug_debris, vr_brick, external ents
- Held props' grips and Align Sights to My Aim: vr_sight_align, held grip modes
- Hand grenades: the pouch and shooting grenades: pouch take and throw, impulse 210, catch tests
- Hands, teleporters and climbing stamina: hands/gen.py plays, tele fixtures
- Hardcoded limits: vr_limits, stress tests
- The HUD in the headset, shimmer and menu sharpness: hudstyle, shimmer_test.py, vr_menu_resolution
- Profiler, Bullet Time and the spectator camera: vr_profile_*, spectator fx, paired renders
- Crash reports: vr_crash.cpp, vr_crash_test
- The Loading... crash and sound-off blind spots: loading_switch_test.sh, vr_zone_threadcheck
- Menus: centring, scroll memory, shortcuts, flat menu: menu_vr pos / dump / ids / expect, flat mouse, quicksave
- Weapons through a save and a load: load_models_test.sh
- Playtest checklist: checklist.txt format, ticks, vr_checklist, folding
- Teleporter and melee regression fixtures (2026-10-04): start-room and melee fixtures
- Parry interrupts (2026-10-05): parry stagger and tests
- Immersive reloading (2026-10-07): reload_test.sh, pouch, magazines
- Put-away transition (2026-10-07): the transition
- Shotgun auto pump (2026-10-07): autopump_test.sh
- OpenXR runtime choice (2026-10-09): xr_runtime_test.sh, fakexr
- Arcane Dimensions: soldiers' guns, upgrades per weapon, the gadget's AD page (2026-10-12): vr_ad_test, soldiers' guns, upgrades, the gadget page
- Piled-up decals: the mark limit a spot (2026-10-12): the mark limit a spot
- Judo throws, held throws and lifts with the mock hands (2026-10-13): held throws and lifts with the mock hands
- Closer to enemies: body inset and step in (2026-10-13): body inset, step in, voreshove
- Blade shapes and Edit Blade Shape (2026-10-23): Edit Blade Shape
- Ragdoll rigs: every seed table's model (2026-11-02): ad_rig_sweep.sh
- The batched suites, proven again on next (2026-10-15): times and verdicts of the suites
